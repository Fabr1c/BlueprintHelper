import {
  sanitizeAgentFacingToolResult,
  sanitizeAgentFacingValue,
  type ToolResultBase,
} from '../../result/tool-result.js';

export type ResultProjectionFormat = 'summary' | 'json' | 'full' | 'markdown';

export interface ResultProjectionPolicy {
  policy_id: string;
  default_fields: string[];
  json_fields: string[];
  full_fields: string[];
  expert_fields: string[];
  debug_artifact_fields: string[];
  omit_by_default: string[];
}

export interface ProjectToolResultForCliInput {
  command_kind: string;
  tool_name?: string;
  format: ResultProjectionFormat;
  expert?: boolean;
  tool_result: ToolResultBase;
  extra?: Record<string, unknown>;
  artifact_refs?: Record<string, string>;
  policy: ResultProjectionPolicy;
}

export interface ProjectToolResultForCliOutput {
  tool_result: Record<string, unknown>;
  extra: Record<string, unknown>;
}

export interface SelectProjectionFieldsInput {
  readonly policy: ResultProjectionPolicy;
  readonly format: ResultProjectionFormat;
  readonly expert?: boolean;
  readonly artifactKind?: 'stdout' | 'debug_artifact';
}

export const GENERIC_RESULT_PROJECTION_POLICY: ResultProjectionPolicy = {
  policy_id: 'tool.generic.default',
  default_fields: ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'],
  json_fields: ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'],
  full_fields: ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'],
  expert_fields: [],
  debug_artifact_fields: ['tool_result', 'extra', 'bridge_result', 'debug'],
  omit_by_default: ['schema', 'trace_id', 'debug', 'bridge_result'],
};

export function selectProjectionFields(input: SelectProjectionFieldsInput): readonly string[] {
  if (input.artifactKind === 'debug_artifact') {
    return input.policy.debug_artifact_fields;
  }
  if (input.expert === true) {
    return uniqueStrings([...input.policy.full_fields, ...input.policy.expert_fields]);
  }
  if (input.format === 'summary') {
    return input.policy.default_fields;
  }
  if (input.format === 'json') {
    return input.policy.json_fields;
  }
  if (input.format === 'full') {
    return input.policy.full_fields;
  }
  return input.policy.default_fields;
}

export function projectToolResultForCli(input: ProjectToolResultForCliInput): ProjectToolResultForCliOutput {
  const sanitized = sanitizeAgentFacingToolResult(input.tool_result);
  const selectedFields = selectProjectionFields({
    policy: input.policy,
    format: input.format,
    expert: input.expert,
    artifactKind: 'stdout',
  });
  return {
    tool_result: compactToolResultForDefaultCliOutput(sanitized, input.policy, selectedFields),
    extra: compactExtraForDefaultCliOutput(
      sanitizeAgentFacingValue(input.extra ?? {}) as Record<string, unknown>,
      input.policy,
      selectedFields,
    ),
  };
}

export function buildCliDebugArtifactSource(input: ProjectToolResultForCliInput): Record<string, unknown> | undefined {
  if (input.expert !== true) {
    return undefined;
  }

  const selectedFields = new Set(selectProjectionFields({
    policy: input.policy,
    format: input.format,
    expert: input.expert,
    artifactKind: 'debug_artifact',
  }));
  const toolResult = sanitizeAgentFacingToolResult(input.tool_result, { preserveDebug: true });
  const debug = asRecord((toolResult as ToolResultBase & { debug?: Record<string, unknown> }).debug);
  const data = asRecord(toolResult.data);
  const bridgeResult = debug?.['bridge_result'] ?? data?.['bridge_result'];
  const remainingDebug = debug
    ? Object.fromEntries(Object.entries(debug).filter(([key]) => key !== 'bridge_result'))
    : undefined;
  const toolResultWithoutDebug = { ...toolResult } as Record<string, unknown>;
  delete toolResultWithoutDebug['debug'];

  return filterRecordByTopLevelFields(omitUndefined({
    schema: 'BlueprintHelper.CliDebugResult.v1',
    command: input.command_kind,
    tool_name: input.tool_name,
    tool_result: toolResultWithoutDebug,
    extra: input.extra && Object.keys(input.extra).length > 0 ? sanitizeAgentFacingValue(input.extra) : undefined,
    bridge_result: bridgeResult,
    debug: remainingDebug && Object.keys(remainingDebug).length > 0 ? remainingDebug : undefined,
  }), selectedFields);
}

export function compactToolResultForDefaultCliOutput(
  result: ToolResultBase,
  policy: ResultProjectionPolicy = GENERIC_RESULT_PROJECTION_POLICY,
  selectedFields: readonly string[] = policy.full_fields,
): Record<string, unknown> {
  const projected = projectValueByFields(result, selectedFields);
  const compacted = compactTaskSpecExecutionData(compactPolicyOnlyFields(compactCliValue(projected, policy))) as Record<string, unknown>;
  delete compacted['debug'];
  delete compacted['schema'];
  delete compacted['trace_id'];
  return compacted;
}

export function compactExtraForDefaultCliOutput(
  extra: Record<string, unknown>,
  policy: ResultProjectionPolicy = GENERIC_RESULT_PROJECTION_POLICY,
  selectedFields: readonly string[] = policy.full_fields,
): Record<string, unknown> {
  const extraFields = selectedFields
    .filter((field) => field === 'extra' || field.startsWith('extra.'))
    .map((field) => field === 'extra' ? '' : field.slice('extra.'.length))
    .filter((field) => field.length > 0);
  const next = extraFields.length > 0
    ? projectValueByFields(extra, extraFields)
    : { ...extra };
  delete next['taskPlan'];
  return compactPolicyOnlyFields(compactCliValue(next, policy)) as Record<string, unknown>;
}

export function compactTaskPlanForArtifact(
  taskPlan: unknown,
  policy: ResultProjectionPolicy = GENERIC_RESULT_PROJECTION_POLICY,
): unknown {
  return compactPolicyOnlyFields(compactCliValue(taskPlan, policy));
}

export function compactCliValue(
  value: unknown,
  policy: ResultProjectionPolicy = GENERIC_RESULT_PROJECTION_POLICY,
): unknown {
  if (Array.isArray(value)) {
    return value.map((entry) => compactCliValue(entry, policy));
  }
  if (!isRecord(value)) {
    return value;
  }

  const out: Record<string, unknown> = {};
  const bExternalGraphAnchor = value['schema'] === 'BlueprintHelper.ExternalGraphAnchor.v1';
  for (const [key, entry] of Object.entries(value)) {
    const bPreserveExternalAnchorSchema = bExternalGraphAnchor && key === 'schema';
    if (!bPreserveExternalAnchorSchema && shouldOmitByPolicy(key, entry, policy)) {
      continue;
    }
    if (!bPreserveExternalAnchorSchema && key === 'schema' && typeof entry === 'string' && entry.startsWith('BlueprintHelper.')) {
      continue;
    }
    out[key] = compactCliValue(entry, policy);
  }
  return out;
}

export function compactPolicyOnlyFields(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => compactPolicyOnlyFields(item));
  }

  if (!isRecord(value)) {
    return value;
  }

  const compacted: Record<string, unknown> = {};
  for (const [key, entry] of Object.entries(value)) {
    if (key === 'execution_policy' || key === 'scope_policy') {
      continue;
    }

    if (key === 'validation') {
      const compactedValidation = compactValidationForDefaultReturn(entry);
      if (compactedValidation !== undefined) {
        compacted[key] = compactedValidation;
      }
      continue;
    }

    compacted[key] = compactPolicyOnlyFields(entry);
  }
  return compacted;
}

function compactValidationForDefaultReturn(value: unknown): unknown {
  if (!isRecord(value)) {
    return compactPolicyOnlyFields(value);
  }

  const compacted: Record<string, unknown> = {};
  for (const [key, entry] of Object.entries(value)) {
    if (key === 'should_compile' || key === 'should_save') {
      continue;
    }
    compacted[key] = compactPolicyOnlyFields(entry);
  }

  return Object.keys(compacted).length > 0 ? compacted : undefined;
}

function compactTaskSpecExecutionData(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => compactTaskSpecExecutionData(item));
  }
  if (!isRecord(value)) {
    return value;
  }

  const compacted: Record<string, unknown> = {};
  for (const [key, entry] of Object.entries(value)) {
    if (key === 'bridge_result') {
      continue;
    }
    if (key === 'target_assets' && isRecord(value['target'])) {
      continue;
    }
    if (key === 'task' && isRecord(entry)) {
      compacted[key] = compactTaskExecutionSummary(entry);
      continue;
    }
    compacted[key] = compactTaskSpecExecutionData(entry);
  }
  return compacted;
}

function compactTaskExecutionSummary(task: Record<string, unknown>): Record<string, unknown> {
  const compacted: Record<string, unknown> = {};
  for (const [key, entry] of Object.entries(task)) {
    if (key === 'task_run_id' || key === 'target_assets') {
      continue;
    }
    compacted[key] = compactTaskSpecExecutionData(entry);
  }
  return compacted;
}

function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return isRecord(value) ? value : undefined;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function shouldOmitByPolicy(key: string, value: unknown, policy: ResultProjectionPolicy): boolean {
  if (key === 'schema') {
    return typeof value === 'string'
      && value.startsWith('BlueprintHelper.')
      && policy.omit_by_default.includes(key);
  }
  return policy.omit_by_default.includes(key);
}

function projectValueByFields<TValue>(value: TValue, fields: readonly string[]): TValue | Record<string, unknown> {
  if (fields.length === 0 || !isRecord(value)) {
    return value;
  }
  const projected: Record<string, unknown> = {};
  for (const field of fields) {
    const parts = field.split('.').filter((part) => part.length > 0);
    if (parts.length === 0) {
      continue;
    }
    const pathValue = readPath(value, parts);
    if (pathValue !== undefined) {
      writePath(projected, parts, pathValue);
    }
  }
  return projected;
}

function readPath(value: unknown, parts: readonly string[]): unknown {
  let cursor = value;
  for (const part of parts) {
    if (!isRecord(cursor) || !(part in cursor)) {
      return undefined;
    }
    cursor = cursor[part];
  }
  return cursor;
}

function writePath(output: Record<string, unknown>, parts: readonly string[], value: unknown): void {
  let cursor = output;
  parts.forEach((part, index) => {
    if (index === parts.length - 1) {
      cursor[part] = value;
      return;
    }
    const existing = cursor[part];
    if (!isRecord(existing)) {
      cursor[part] = {};
    }
    cursor = cursor[part] as Record<string, unknown>;
  });
}

function filterRecordByTopLevelFields(
  record: Record<string, unknown>,
  fields: ReadonlySet<string>,
): Record<string, unknown> {
  if (fields.size === 0) {
    return record;
  }
  return Object.fromEntries(Object.entries(record).filter(([key]) => fields.has(key)));
}

function uniqueStrings(values: readonly string[]): string[] {
  return [...new Set(values)];
}
