import {
  sanitizeAgentFacingToolResult,
  sanitizeAgentFacingValue,
  type ToolResultBase,
} from '@blueprinthelper/task-core/result/tool-result';
import { resolveArtifactRoot, writeJsonArtifact } from './artifacts.js';

export const CLI_RESULT_SCHEMA = 'BlueprintHelper.CliResult.v1';
export const CLI_FULL_RESULT_SCHEMA = 'BlueprintHelper.CliFullResult.v1';
export const CLI_DEBUG_RESULT_SCHEMA = 'BlueprintHelper.CliDebugResult.v1';

export type CliFormat = 'summary' | 'json' | 'full';

export type CliCommandKind =
  | 'tool.invoke'
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
  toolName?: string;
  file?: string;
  json?: string;
  params?: Record<string, unknown>;
  stdin?: boolean;
  expert?: boolean;
  taskRunId?: string;
  bridgeCommand?: string;
  artifactDir?: string;
  maxBytes?: number;
  fields?: string[];
  omitFields?: string[];
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
  const previewId = isPreviewCommand(input.command)
    ? readString(extra['previewId'])
      ?? readString(data?.['preview_id'])
      ?? readString(input.toolResult.trace_id)
    : undefined;
  const taskRunId =
    readString(data?.['task_run_id']) ??
    readString(task?.['task_run_id']) ??
    input.command.taskRunId;

  return omitUndefined({
    ok: input.toolResult.ok,
    schema: CLI_RESULT_SCHEMA,
    operation: input.command.kind,
    tool_name: input.command.toolName,
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
    error_code: input.toolResult.ok ? undefined : input.toolResult.error?.code,
    message: input.toolResult.ok ? undefined : input.toolResult.error?.message,
  });
}

export function writeCliResult(
  runtime: CliOutputRuntime,
  command: CliCommand,
  toolResult: ToolResultBase,
  extra: Record<string, unknown> = {},
): CliWriteOutcome {
  const debugResult = command.expert ? buildDebugResult(command, toolResult, extra) : undefined;
  const safeToolResult = stripExecutePreviewId(command, sanitizeAgentFacingToolResult(toolResult));
  const safeExtra = sanitizeAgentFacingValue(extra);
  const artifactRoot = resolveArtifactRoot({ cwd: runtime.cwd, cliDir: command.artifactDir });
  const runId = inferRunId(command, safeToolResult, safeExtra);
  const fullToolResult = compactCliToolResult(safeToolResult);
  const fullExtra = compactCliExtra(safeExtra);
  const fullResult = omitUndefined({
    schema: CLI_FULL_RESULT_SCHEMA,
    toolResult: fullToolResult,
    extra: Object.keys(fullExtra).length > 0 ? fullExtra : undefined,
  });
  const artifactRefs: Record<string, string> = {
    full_result: writeJsonArtifact({
      root: artifactRoot,
      runId,
      name: 'result',
      value: fullResult,
    }),
  };

  if (debugResult) {
    artifactRefs['debug_result'] = writeJsonArtifact({
      root: artifactRoot,
      runId,
      name: 'debug',
      value: sanitizeAgentFacingValue(debugResult),
    });
  }

  const taskPlan = asTaskPlanLike(safeExtra['taskPlan']) ?? asTaskPlanLike(asRecord(safeToolResult.data)?.['task_plan']);
  if (taskPlan) {
    artifactRefs['task_plan'] = writeJsonArtifact({
      root: artifactRoot,
      runId,
      name: 'task_plan',
      value: taskPlan,
    });
  }

  const output = shapeCliOutput(
    command.format === 'summary'
      ? buildCliSummary({ command, toolResult: safeToolResult, artifactRefs, extra: safeExtra })
      : buildOutput(command, safeToolResult, fullToolResult, artifactRefs, compactCliExtraForOutput(safeExtra)),
    command.fields,
    command.omitFields,
  );
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
  fields?: string[];
  omitFields?: string[];
}): Record<string, unknown> {
  return shapeCliOutput(omitUndefined({
    ok: false,
    schema: CLI_RESULT_SCHEMA,
    operation: input.operation,
    status: input.status,
    message: input.message,
    artifacts: input.artifactRefs,
  }), input.fields, input.omitFields);
}

export function shapeCliOutput(
  output: Record<string, unknown>,
  fields?: string[],
  omitFields?: string[],
): Record<string, unknown> {
  return omitCliFields(projectCliFields(output, fields), omitFields);
}

export function projectCliFields(
  output: Record<string, unknown>,
  fields?: string[],
): Record<string, unknown> {
  if (!fields || fields.length === 0) {
    return output;
  }

  const projected: Record<string, unknown> = {};
  for (const field of fields) {
    const parts = field.split('.').filter((part) => part.length > 0);
    if (parts.length === 0) {
      continue;
    }
    const value = readPath(output, parts);
    if (value !== undefined) {
      writePath(projected, parts, value);
    }
  }
  return projected;
}

export function omitCliFields(
  output: Record<string, unknown>,
  fields?: string[],
): Record<string, unknown> {
  if (!fields || fields.length === 0) {
    return output;
  }

  const omitted = cloneValue(output) as Record<string, unknown>;
  for (const field of fields) {
    const parts = field.split('.').filter((part) => part.length > 0);
    if (parts.length === 0) {
      continue;
    }
    deletePath(omitted, parts);
  }
  return omitted;
}

function buildOutput(
  command: CliCommand,
  toolResult: ToolResultBase,
  outputToolResult: Record<string, unknown>,
  artifactRefs: Record<string, string>,
  extra: Record<string, unknown>,
): Record<string, unknown> {
  return {
    ok: toolResult.ok,
    schema: CLI_RESULT_SCHEMA,
    operation: command.kind,
    tool_name: command.toolName,
    status: mapStatus(command, toolResult, asRecord(toolResult.data)),
    tool_result: outputToolResult,
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
    fields: command.fields,
    omitFields: command.omitFields,
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
  if (command.kind === 'tool.invoke' && command.toolName === 'blueprinthelper_preview_task') {
    return data?.['passed'] === false || !toolResult.ok ? 'preview_blocked' : 'preview_passed';
  }
  if (command.kind === 'task.execute') {
    return toolResult.ok ? 'executed' : 'execute_failed';
  }
  if (command.kind === 'tool.invoke' && command.toolName === 'blueprinthelper_execute_task') {
    return toolResult.ok ? 'executed' : 'execute_failed';
  }
  if (command.kind === 'task.result') {
    return toolResult.ok ? 'result_found' : 'result_missing';
  }
  if (command.kind === 'tool.invoke' && command.toolName === 'blueprinthelper_get_task_result') {
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
  const taskRunId = readString(data?.['task_run_id'])
    ?? readString(task?.['task_run_id'])
    ?? command.taskRunId;
  if (!isPreviewCommand(command)) {
    return taskRunId ?? `cli_${Date.now()}`;
  }
  return readString(extra['previewId'])
    ?? readString(data?.['preview_id'])
    ?? taskRunId
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

function isPreviewCommand(command: CliCommand): boolean {
  return command.kind === 'task.preview'
    || (command.kind === 'tool.invoke' && command.toolName === 'blueprinthelper_preview_task');
}

function isExecuteCommand(command: CliCommand): boolean {
  return command.kind === 'task.execute'
    || (command.kind === 'tool.invoke' && command.toolName === 'blueprinthelper_execute_task');
}

function stripExecutePreviewId(command: CliCommand, toolResult: ToolResultBase): ToolResultBase {
  if (!toolResult.ok || !isExecuteCommand(command)) {
    return toolResult;
  }
  const data = asRecord(toolResult.data);
  if (!data || !Object.hasOwn(data, 'preview_id')) {
    return toolResult;
  }
  const nextData = { ...data };
  delete nextData['preview_id'];
  return {
    ...toolResult,
    data: nextData,
  };
}

function buildDebugResult(
  command: CliCommand,
  toolResult: ToolResultBase,
  extra: Record<string, unknown>,
): Record<string, unknown> | undefined {
  const debug = asRecord((toolResult as ToolResultBase & { debug?: Record<string, unknown> }).debug);
  const data = asRecord(toolResult.data);
  const bridgeResult = debug?.['bridge_result'] ?? data?.['bridge_result'];
  const remainingDebug = debug
    ? Object.fromEntries(Object.entries(debug).filter(([key]) => key !== 'bridge_result'))
    : undefined;
  const toolResultWithoutDebug = { ...toolResult } as Record<string, unknown>;
  delete toolResultWithoutDebug['debug'];

  const result = omitUndefined({
    schema: CLI_DEBUG_RESULT_SCHEMA,
    command: command.kind,
    tool_name: command.toolName,
    tool_result: toolResultWithoutDebug,
    extra: Object.keys(extra).length > 0 ? extra : undefined,
    bridge_result: bridgeResult,
    debug: remainingDebug && Object.keys(remainingDebug).length > 0 ? remainingDebug : undefined,
  });

  return result;
}

function compactCliToolResult(toolResult: ToolResultBase): Record<string, unknown> {
  return compactTaskSpecExecutionData(compactCliValue(toolResult)) as Record<string, unknown>;
}

function compactCliExtra(extra: Record<string, unknown>): Record<string, unknown> {
  const next = { ...extra };
  delete next['taskPlan'];
  return compactCliValue(next) as Record<string, unknown>;
}

function compactCliExtraForOutput(extra: Record<string, unknown>): Record<string, unknown> {
  return compactCliExtra(extra);
}

function compactTaskSpecExecutionData(value: unknown): unknown {
  if (!isRecord(value)) {
    return value;
  }

  const data = asRecord(value['data']);
  const task = asRecord(data?.['task']);
  if (!task) {
    return value;
  }

  delete task['task_run_id'];
  delete task['target_assets'];
  if (Object.keys(task).length === 0) {
    delete data?.['task'];
  }
  return value;
}

function compactCliValue(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => compactCliValue(item));
  }

  if (!isRecord(value)) {
    return value;
  }

  const compacted: Record<string, unknown> = {};
  for (const [key, entry] of Object.entries(value)) {
    if (key === 'debug' || key === 'trace_id' || key === 'bridge_result') {
      continue;
    }
    if (key === 'schema' && typeof entry === 'string' && entry.startsWith('BlueprintHelper.')) {
      continue;
    }
    compacted[key] = compactCliValue(entry);
  }
  return compacted;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function readPath(record: Record<string, unknown>, parts: string[]): unknown {
  let current: unknown = record;
  for (const part of parts) {
    if (!isRecord(current) || !(part in current)) {
      return undefined;
    }
    current = current[part];
  }
  return current;
}

function writePath(record: Record<string, unknown>, parts: string[], value: unknown): void {
  let current = record;
  for (const part of parts.slice(0, -1)) {
    if (!isRecord(current[part])) {
      current[part] = {};
    }
    current = current[part] as Record<string, unknown>;
  }
  current[parts[parts.length - 1]] = value;
}

function deletePath(record: Record<string, unknown>, parts: string[]): void {
  let current: unknown = record;
  for (const part of parts.slice(0, -1)) {
    if (!isRecord(current)) {
      return;
    }
    current = current[part];
  }
  if (isRecord(current)) {
    delete current[parts[parts.length - 1]];
  }
}

function cloneValue(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => cloneValue(item));
  }
  if (isRecord(value)) {
    return Object.fromEntries(
      Object.entries(value).map(([key, entry]) => [key, cloneValue(entry)]),
    );
  }
  return value;
}

function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}
