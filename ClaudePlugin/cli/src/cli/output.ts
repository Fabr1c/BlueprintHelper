import type { ToolResultBase } from '@blueprinthelper/task-core/result/tool-result';
import { resolveArtifactRoot, writeJsonArtifact } from './artifacts.js';

export const CLI_RESULT_SCHEMA = 'BlueprintHelper.CliResult.v1';

export type CliFormat = 'summary' | 'json' | 'full';

export type CliCommandKind =
  | 'task.preview'
  | 'task.execute'
  | 'task.result'
  | 'bridge.ping'
  | 'bridge.call'
  | 'context.read'
  | 'output';

export interface CliCommand {
  kind: CliCommandKind;
  format: CliFormat;
  file?: string;
  taskRunId?: string;
  bridgeCommand?: string;
  artifactDir?: string;
  maxBytes?: number;
}

export interface CliOutputRuntime {
  cwd: string;
  stdout: (text: string) => void;
}

export interface CliWriteOutcome {
  outputTooLarge: boolean;
  artifactRefs: Record<string, string>;
}

export function buildCliSummary(input: {
  command: CliCommand;
  toolResult: ToolResultBase;
  artifactRefs: Record<string, string>;
  extra?: Record<string, unknown>;
  forceBudgetFailure?: boolean;
}): Record<string, unknown> {
  if (input.forceBudgetFailure) {
    return outputTooLargeResult(input.command, input.artifactRefs);
  }

  const data = asRecord(input.toolResult.data);
  const extra = input.extra ?? {};
  const taskPlan = asTaskPlanLike(extra['taskPlan']) ?? asTaskPlanLike(data?.['task_plan']);
  const task = asRecord(data?.['task']);
  const issues = arrayOfRecords(data?.['issues']);
  const targetAssets = collectTargetAssets(input.toolResult, data, task, taskPlan);
  const taskType = readString(taskPlan?.task_type) ?? readString(data?.['task_type']) ?? readString(task?.['task_type']);
  const previewId =
    readString(extra['previewId']) ??
    readString(data?.['preview_id']) ??
    (input.command.kind === 'task.preview' ? readString(input.toolResult.trace_id) : undefined);
  const taskRunId =
    readString(data?.['task_run_id']) ??
    readString(task?.['task_run_id']) ??
    input.command.taskRunId;

  return omitUndefined({
    ok: input.toolResult.ok,
    schema: CLI_RESULT_SCHEMA,
    operation: input.command.kind,
    status: mapStatus(input.command, input.toolResult, data),
    task_run_id: taskRunId,
    preview_id: previewId,
    summary: omitUndefined({
      target_assets: targetAssets.length > 0 ? targetAssets : undefined,
      task_type: taskType,
      planned_steps: taskPlan ? arrayOfRecords(taskPlan['steps']).length : readNumber(task?.['applied_steps']),
      warnings: countIssues(issues, 'warning'),
      errors: input.toolResult.ok ? countIssues(issues, 'error') : Math.max(1, countIssues(issues, 'error')),
      modified: input.toolResult.modified,
    }),
    artifacts: input.artifactRefs,
    message: input.toolResult.ok ? undefined : input.toolResult.error?.message,
  });
}

export function writeCliResult(
  runtime: CliOutputRuntime,
  command: CliCommand,
  toolResult: ToolResultBase,
  extra: Record<string, unknown> = {},
): CliWriteOutcome {
  const artifactRoot = resolveArtifactRoot({ cwd: runtime.cwd, cliDir: command.artifactDir });
  const runId = inferRunId(command, toolResult, extra);
  const artifactRefs: Record<string, string> = {
    full_result: writeJsonArtifact({
      root: artifactRoot,
      runId,
      name: 'result',
      value: { toolResult, extra },
    }),
  };

  const taskPlan = asTaskPlanLike(extra['taskPlan']) ?? asTaskPlanLike(asRecord(toolResult.data)?.['task_plan']);
  if (taskPlan) {
    artifactRefs['task_plan'] = writeJsonArtifact({
      root: artifactRoot,
      runId,
      name: 'task_plan',
      value: taskPlan,
    });
  }

  const output = buildOutput(command, toolResult, artifactRefs, extra);
  const text = `${JSON.stringify(output)}\n`;
  if (command.maxBytes !== undefined && Buffer.byteLength(text, 'utf8') > command.maxBytes) {
    const budgetResult = outputTooLargeResult(command, artifactRefs);
    runtime.stdout(`${JSON.stringify(budgetResult)}\n`);
    return { outputTooLarge: true, artifactRefs };
  }

  runtime.stdout(text);
  return { outputTooLarge: false, artifactRefs };
}

export function buildCliError(input: {
  operation: CliCommandKind | 'output';
  status: string;
  message: string;
  artifactRefs?: Record<string, string>;
}): Record<string, unknown> {
  return omitUndefined({
    ok: false,
    schema: CLI_RESULT_SCHEMA,
    operation: input.operation,
    status: input.status,
    message: input.message,
    artifacts: input.artifactRefs,
  });
}

function buildOutput(
  command: CliCommand,
  toolResult: ToolResultBase,
  artifactRefs: Record<string, string>,
  extra: Record<string, unknown>,
): Record<string, unknown> {
  if (command.format === 'summary') {
    return buildCliSummary({ command, toolResult, artifactRefs, extra });
  }

  return {
    ok: toolResult.ok,
    schema: CLI_RESULT_SCHEMA,
    operation: command.kind,
    status: mapStatus(command, toolResult, asRecord(toolResult.data)),
    tool_result: toolResult,
    extra,
    artifacts: artifactRefs,
  };
}

function outputTooLargeResult(command: CliCommand, artifactRefs: Record<string, string>): Record<string, unknown> {
  return buildCliError({
    operation: 'output',
    status: 'output_too_large',
    message: 'CLI stdout exceeds --max-bytes. Read the artifact path instead.',
    artifactRefs,
  });
}

function mapStatus(
  command: CliCommand,
  toolResult: ToolResultBase,
  data: Record<string, unknown> | undefined,
): string {
  if (command.kind === 'task.preview') {
    return data?.['passed'] === false || !toolResult.ok ? 'preview_blocked' : 'preview_passed';
  }
  if (command.kind === 'task.execute') {
    return toolResult.ok ? 'executed' : 'execute_failed';
  }
  if (command.kind === 'task.result') {
    return toolResult.ok ? 'result_found' : 'result_missing';
  }
  if (command.kind === 'bridge.ping') {
    return toolResult.ok ? 'bridge_available' : 'bridge_unavailable';
  }
  return toolResult.status;
}

function inferRunId(
  command: CliCommand,
  toolResult: ToolResultBase,
  extra: Record<string, unknown>,
): string {
  const data = asRecord(toolResult.data);
  const task = asRecord(data?.['task']);
  return readString(extra['previewId'])
    ?? readString(data?.['preview_id'])
    ?? readString(data?.['task_run_id'])
    ?? readString(task?.['task_run_id'])
    ?? command.taskRunId
    ?? `cli_${Date.now()}`;
}

function collectTargetAssets(
  toolResult: ToolResultBase,
  data: Record<string, unknown> | undefined,
  task: Record<string, unknown> | undefined,
  taskPlan: Record<string, unknown> | undefined,
): string[] {
  if (taskPlan) {
    return arrayOfStrings(taskPlan['target_assets']);
  }
  const taskTargets = arrayOfStrings(task?.['target_assets']);
  if (taskTargets.length > 0) {
    return taskTargets;
  }
  const dataTargets = arrayOfStrings(data?.['target_assets']);
  if (dataTargets.length > 0) {
    return dataTargets;
  }
  return toolResult.target?.asset_path ? [toolResult.target.asset_path] : [];
}

function countIssues(issues: Array<Record<string, unknown>>, kind: 'warning' | 'error'): number {
  if (kind === 'warning') {
    return issues.filter((issue) => readString(issue['severity']) === 'warning').length;
  }
  return issues.filter((issue) => readString(issue['severity']) !== 'warning').length;
}

function asTaskPlanLike(value: unknown): Record<string, unknown> | undefined {
  if (!isRecord(value)) {
    return undefined;
  }
  if (Array.isArray(value['steps'])) {
    return value;
  }
  return undefined;
}

function arrayOfRecords(value: unknown): Array<Record<string, unknown>> {
  return Array.isArray(value)
    ? value.filter((item): item is Record<string, unknown> => isRecord(item))
    : [];
}

function arrayOfStrings(value: unknown): string[] {
  return Array.isArray(value)
    ? value.filter((item): item is string => typeof item === 'string')
    : [];
}

function readString(value: unknown): string | undefined {
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

function readNumber(value: unknown): number | undefined {
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return isRecord(value) ? value : undefined;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}
