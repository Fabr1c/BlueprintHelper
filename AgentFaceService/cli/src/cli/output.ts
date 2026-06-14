import {
  sanitizeAgentFacingToolResult,
  sanitizeAgentFacingValue,
  type ToolResultBase,
} from '@blueprinthelper/task-core/result/tool-result';
import {
  buildReadonlyToolCommandManifestRegistry,
  buildCliDebugArtifactSource,
  compactExtraForDefaultCliOutput,
  compactTaskPlanForArtifact,
  getBuiltinResultProjectionPolicy,
  projectToolResultForCli,
  projectMetricsReportDataForCli,
  resolveResultProjectionPolicy,
  type BuiltinResultProjectionPolicyId,
  type CliCommandInputIoKind,
  type CliCommandOutputDataPolicyId,
  type CliCommandRunIdPolicyId,
  type CliCommandStatusPolicyId,
  type ResultProjectionPolicy,
} from '@blueprinthelper/task-core/tool-surface/tool-registry';
import type { MetricsReportKind } from '@blueprinthelper/task-core/metrics/metrics-reporter';
import type { MetricsWindow } from '@blueprinthelper/task-core/metrics/metrics-store';
import { resolveArtifactRoot, writeJsonArtifact } from './artifacts.js';
import { createOutputIoSummary } from './io-stats.js';

export const CLI_DEBUG_RESULT_SCHEMA = 'BlueprintHelper.CliDebugResult.v1';

export type CliFormat = 'summary' | 'json' | 'full' | 'markdown';

export type CliCommandKind =
  | 'tool.invoke'
  | 'task.preview'
  | 'task.execute'
  | 'task.result'
  | 'tools.domains'
  | 'tools.list'
  | 'tools.templates.families'
  | 'tools.templates.write_modes'
  | 'tools.templates.clusters'
  | 'tools.templates.operations'
  | 'tools.templates.quick_access'
  | 'tools.templates.compose'
  | 'tools.read_templates.domains'
  | 'tools.read_templates.clusters'
  | 'tools.read_templates.targets'
  | 'tools.read_templates.views'
  | 'tools.read_templates.quick_access'
  | 'tools.read_templates.compose'
  | 'bridge.ping'
  | 'bridge.call'
  | 'context.read'
  | 'metrics.report'
  | 'output';

export interface CliCommand {
  kind: CliCommandKind;
  format: CliFormat;
  resultPolicyId?: BuiltinResultProjectionPolicyId;
  statusPolicyId?: CliCommandStatusPolicyId;
  runIdPolicyId?: CliCommandRunIdPolicyId;
  outputDataPolicyId?: CliCommandOutputDataPolicyId;
  manifestLookupId?: string;
  metricsToolName?: string;
  metricsLookupId?: string;
  inputIoKind?: CliCommandInputIoKind;
  toolName?: string;
  file?: string;
  json?: string;
  params?: Record<string, unknown>;
  stdin?: boolean;
  develop?: boolean;
  expert?: boolean;
  previewToken?: string;
  compileOnly?: boolean;
  taskRunId?: string;
  includeReserved?: boolean;
  audience?: 'default' | 'compat' | 'expert';
  toolDomain?: string;
  toolCatalogKind?: string;
  workflow?: string;
  family?: string;
  writeMode?: string;
  cluster?: string;
  operation?: string;
  templateIds?: string[];
  outputPath?: string;
  domain?: string;
  readCluster?: string;
  targetKind?: string;
  viewTemplate?: string;
  requiresBridge?: boolean;
  risks?: string[];
  bridgeCommand?: string;
  metricsKind?: MetricsReportKind;
  metricsRoot?: string;
  window?: MetricsWindow;
  limit?: number;
  artifactDir?: string;
  maxBytes?: number;
  fields?: string[];
  omitFields?: string[];
}

const TOOL_COMMAND_MANIFEST_REGISTRY = buildReadonlyToolCommandManifestRegistry();

export interface CliOutputRuntime {
  cwd: string;
  stdout: (text: string) => void;
}

export interface CliWriteOutcome {
  outputTooLarge: boolean;
  artifactRefs: Record<string, string>;
  outputChars: number;
  outputUtf8Bytes: number;
  estimatedOutputTokens: number;
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
  const violations = collectConnectivityViolations(data, input.toolResult.error);
  const connectivitySummary = collectConnectivitySummary(data);
  const policies = resolveCliOutputPolicies(input.command);
  const status = mapStatus(policies.statusPolicyId, input.toolResult, data);
  const blockedIssueCode = status === 'preview_blocked' ? readString(issues[0]?.['code']) : undefined;
  const blockedIssueMessage = status === 'preview_blocked' ? readString(issues[0]?.['message']) : undefined;
  const previewId = policies.runIdPolicyId === 'task.preview_run_id'
    ? readString(extra['previewId'])
      ?? readString(data?.['preview_id'])
      ?? readString(input.toolResult.trace_id)
    : undefined;
  const previewToken = policies.runIdPolicyId === 'task.preview_run_id'
    ? readString(extra['previewToken'])
      ?? readString(data?.['preview_token'])
    : undefined;
  const taskRunId =
    readString(data?.['task_run_id']) ??
    readString(task?.['task_run_id']) ??
    input.command.taskRunId;

  return omitUndefined({
    ok: input.toolResult.ok,
    operation: input.command.kind,
    tool_name: input.command.toolName,
    status,
    task_run_id: taskRunId,
    preview_id: previewId,
    preview_token: previewToken,
    summary: omitUndefined({
      target_assets: targetAssets.length > 0 ? targetAssets : undefined,
      task_type: taskType,
      planned_steps: taskPlan ? arrayOfRecords(taskPlan['steps']).length : readNumber(task?.['applied_steps']),
      warnings: countIssues(issues, 'warning'),
      errors: input.toolResult.ok ? countIssues(issues, 'error') : Math.max(1, countIssues(issues, 'error')),
      modified: input.toolResult.modified,
      connectivity: connectivitySummary,
    }),
    artifacts: input.artifactRefs,
    error_code: input.toolResult.ok ? blockedIssueCode : input.toolResult.error?.code,
    message: input.toolResult.ok ? blockedIssueMessage : input.toolResult.error?.message,
    violations: violations.length > 0 ? violations : undefined,
  });
}

export function writeCliResult(
  runtime: CliOutputRuntime,
  command: CliCommand,
  toolResult: ToolResultBase,
  extra: Record<string, unknown> = {},
): CliWriteOutcome {
  const outputPolicies = resolveCliOutputPolicies(command);
  const projectionPolicy = outputPolicies.projectionPolicy;
  const debugResult = buildCliDebugArtifactSource({
    command_kind: command.kind,
    tool_name: command.toolName,
    format: command.format,
    expert: command.expert,
    tool_result: toolResult,
    extra,
    policy: projectionPolicy,
  });
  const safeToolResult = sanitizeAgentFacingToolResult(toolResult);
  const safeExtra = sanitizeAgentFacingValue(extra);
  const artifactRoot = resolveArtifactRoot({ cwd: runtime.cwd, cliDir: command.artifactDir });
  const runId = inferRunId(outputPolicies.runIdPolicyId, command, safeToolResult, safeExtra);
  const projected = projectToolResultForCli({
    command_kind: command.kind,
    tool_name: command.toolName,
    format: command.format,
    develop: command.develop,
    expert: command.expert,
    tool_result: safeToolResult,
    extra: safeExtra,
    policy: projectionPolicy,
  });
  const fullToolResult = projected.tool_result;
  const fullExtra = projected.extra;
  const fullResult = omitUndefined({
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
      value: compactTaskPlanForArtifact(taskPlan),
    });
  }

  const output = shapeCliOutput(
    command.format === 'summary'
      ? buildCliSummary({ command, toolResult: safeToolResult, artifactRefs, extra: safeExtra })
      : buildOutput(command, outputPolicies, safeToolResult, fullToolResult, artifactRefs, compactCliExtraForOutput(safeExtra)),
    command.fields,
    command.omitFields,
  );
  const text = `${JSON.stringify(output)}\n`;
  if (command.maxBytes !== undefined && Buffer.byteLength(text, 'utf8') > command.maxBytes) {
    const budgetResult = outputTooLargeResult(command, artifactRefs);
    const budgetText = `${JSON.stringify(budgetResult)}\n`;
    runtime.stdout(budgetText);
    return createWriteOutcome(true, artifactRefs, budgetText);
  }

  runtime.stdout(text);
  return createWriteOutcome(false, artifactRefs, text);
}

function collectConnectivitySummary(
  data: Record<string, unknown> | undefined,
): Record<string, unknown> | undefined {
  const requested = readNumber(data?.['requested_connections']);
  const verified = readNumber(data?.['verified_connections']);
  const graphSync = readNumber(data?.['graph_sync_connections']);
  const violations = readNumber(data?.['connectivity_violation_count']);
  if (requested === undefined && verified === undefined && graphSync === undefined && violations === undefined) {
    return undefined;
  }

  return omitUndefined({
    requested,
    verified,
    graph_sync: graphSync,
    violations,
  });
}

function createWriteOutcome(
  outputTooLarge: boolean,
  artifactRefs: Record<string, string>,
  text: string,
): CliWriteOutcome {
  const outputIo = createOutputIoSummary(text);
  return {
    outputTooLarge,
    artifactRefs,
    outputChars: outputIo.output_chars ?? 0,
    outputUtf8Bytes: outputIo.output_utf8_bytes ?? 0,
    estimatedOutputTokens: outputIo.estimated_output_tokens ?? 0,
  };
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
  outputPolicies: CliOutputPolicies,
  toolResult: ToolResultBase,
  outputToolResult: Record<string, unknown>,
  artifactRefs: Record<string, string>,
  extra: Record<string, unknown>,
): Record<string, unknown> {
  const data = asRecord(toolResult.data);
  const status = mapStatus(outputPolicies.statusPolicyId, toolResult, data);
  const issues = arrayOfRecords(data?.['issues']);
  const blockedIssueCode = status === 'preview_blocked' ? readString(issues[0]?.['code']) : undefined;
  const blockedIssueMessage = status === 'preview_blocked' ? readString(issues[0]?.['message']) : undefined;

  if (outputPolicies.outputDataPolicyId === 'metrics.report_data') {
    const metricsData = data ?? {};
    return omitUndefined({
      ok: toolResult.ok,
      operation: command.kind,
      status,
      data: projectMetricsReportDataForCli(metricsData, command.format),
      artifacts: artifactRefs,
      error_code: toolResult.error?.code,
      message: toolResult.error?.message,
    });
  }

  return {
    ok: toolResult.ok,
    operation: command.kind,
    tool_name: command.toolName,
    status,
    tool_result: outputToolResult,
    extra,
    artifacts: artifactRefs,
    error_code: toolResult.ok ? blockedIssueCode : toolResult.error?.code,
    message: toolResult.ok ? blockedIssueMessage : toolResult.error?.message,
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

interface CliOutputPolicies {
  readonly projectionPolicy: ResultProjectionPolicy;
  readonly statusPolicyId: CliCommandStatusPolicyId;
  readonly runIdPolicyId: CliCommandRunIdPolicyId;
  readonly outputDataPolicyId: CliCommandOutputDataPolicyId;
}

function resolveCliOutputPolicies(command: CliCommand): CliOutputPolicies {
  const projectionPolicy = command.resultPolicyId
    ? getBuiltinResultProjectionPolicy(command.resultPolicyId)
    : resolveResultProjectionPolicy({
      manifestRegistry: TOOL_COMMAND_MANIFEST_REGISTRY,
      toolIdOrAlias: command.toolName,
    });
  return {
    projectionPolicy,
    statusPolicyId: command.statusPolicyId ?? statusPolicyIdForProjectionPolicy(projectionPolicy.policy_id),
    runIdPolicyId: command.runIdPolicyId ?? runIdPolicyIdForProjectionPolicy(projectionPolicy.policy_id),
    outputDataPolicyId: command.outputDataPolicyId ?? outputDataPolicyIdForProjectionPolicy(projectionPolicy.policy_id),
  };
}

function statusPolicyIdForProjectionPolicy(policyId: string): CliCommandStatusPolicyId {
  if (policyId === 'task.preview.default') return 'task.preview_status';
  if (policyId === 'task.execute.default') return 'task.execute_status';
  if (policyId === 'task.result.default') return 'task.result_status';
  if (policyId === 'metrics.report.default') return 'metrics.report_status';
  return 'tool.result_status';
}

function runIdPolicyIdForProjectionPolicy(policyId: string): CliCommandRunIdPolicyId {
  if (policyId === 'task.preview.default') return 'task.preview_run_id';
  if (policyId === 'metrics.report.default') return 'metrics.report_run_id';
  return 'tool.result_task_or_cli';
}

function outputDataPolicyIdForProjectionPolicy(policyId: string): CliCommandOutputDataPolicyId {
  return policyId === 'metrics.report.default' ? 'metrics.report_data' : 'tool.result_data';
}

function mapStatus(
  statusPolicyId: CliCommandStatusPolicyId,
  toolResult: ToolResultBase,
  data: Record<string, unknown> | undefined,
): string {
  if (statusPolicyId === 'metrics.report_status') {
    return toolResult.ok ? 'reported' : 'report_failed';
  }
  if (statusPolicyId === 'task.preview_status') {
    return data?.['passed'] === false || !toolResult.ok ? 'preview_blocked' : 'preview_passed';
  }
  if (statusPolicyId === 'task.execute_status') {
    return toolResult.ok ? 'executed' : 'execute_failed';
  }
  if (statusPolicyId === 'task.result_status') {
    return toolResult.ok ? 'result_found' : 'result_missing';
  }
  if (statusPolicyId === 'bridge.ping_status') {
    return toolResult.ok ? 'bridge_available' : 'bridge_unavailable';
  }
  return toolResult.status;
}

function inferRunId(
  runIdPolicyId: CliCommandRunIdPolicyId,
  command: CliCommand,
  toolResult: ToolResultBase,
  extra: Record<string, unknown>,
): string {
  if (runIdPolicyId === 'metrics.report_run_id') {
    return `metrics_${Date.now()}`;
  }

  const data = asRecord(toolResult.data);
  const task = asRecord(data?.['task']);
  const taskRunId = readString(data?.['task_run_id'])
    ?? readString(task?.['task_run_id'])
    ?? command.taskRunId;
  if (runIdPolicyId !== 'task.preview_run_id') {
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

function collectConnectivityViolations(
  data: Record<string, unknown> | undefined,
  error: unknown,
): Array<Record<string, unknown>> {
  const connectivity = asRecord(data?.['connectivity']);
  const dataViolations = arrayOfRecords(connectivity?.['violations']).map(toConciseViolation).filter(isRecord);
  if (dataViolations.length > 0) {
    return dataViolations;
  }

  const dataIssues = arrayOfRecords(data?.['issues'])
    .filter((issue) => isConnectivityViolationCode(readString(issue['code'])))
    .map(toConciseViolation)
    .filter(isRecord);
  if (dataIssues.length > 0) {
    return dataIssues;
  }

  const errorIssues = arrayOfRecords(asRecord(error)?.['issues'])
    .filter((issue) => isConnectivityViolationCode(readString(issue['code'])))
    .map(toConciseViolation)
    .filter(isRecord);
  return errorIssues;
}

function toConciseViolation(value: Record<string, unknown>): Record<string, unknown> | undefined {
  const code = readString(value['code']);
  const message = readString(value['message']);
  if (!code || !message) {
    return undefined;
  }

  return omitUndefined({
    code,
    node_id: readString(value['node_id']),
    target_key: readString(value['target_key']),
    graph_name: readString(value['graph_name']),
    pin_name: readString(value['pin_name']),
    field: readString(value['field']),
    path: readString(value['path']),
    message,
  });
}

function isConnectivityViolationCode(code: string | undefined): boolean {
  return code === 'missing_expected_link'
    || code === 'graphwrite_connectivity_failed'
    || code === 'unreachable_exec_node'
    || code === 'unconsumed_pure_data_node'
    || code === 'unreachable_pure_data_chain'
    || code === 'invalid_expression_exec_node'
    || code === 'unregistered_generated_node'
    || code === 'unregistered_generated_link'
    || code === 'external_boundary_not_connected'
    || code === 'material_unconsumed_expression'
    || code === 'material_connectivity_violation'
    || code === 'material_property_not_supported';
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

function compactCliExtraForOutput(extra: Record<string, unknown>): Record<string, unknown> {
  return compactExtraForDefaultCliOutput(extra);
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
