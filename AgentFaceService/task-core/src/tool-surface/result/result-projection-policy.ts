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
  omit_rules: ResultProjectionOmitRule[];
  extra_omit_rules: ResultProjectionOmitRule[];
}

export interface ResultProjectionOmitRule {
  field: string;
  parent_path?: string[];
  parent_path_suffix?: string[];
  value_prefix?: string;
  parent_has_field?: string;
  preserve_when_parent_field_equals?: {
    field: string;
    value: unknown;
  };
}

export interface ProjectToolResultForCliInput {
  command_kind: string;
  tool_name?: string;
  format: ResultProjectionFormat;
  develop?: boolean;
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
  readonly develop?: boolean;
  readonly expert?: boolean;
  readonly artifactKind?: 'stdout' | 'debug_artifact';
}

export const GENERIC_RESULT_PROJECTION_POLICY: ResultProjectionPolicy = {
  policy_id: 'tool.generic.default',
  default_fields: ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'],
  json_fields: ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'],
  full_fields: ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'],
  expert_fields: [],
  debug_artifact_fields: ['schema', 'tool_result', 'extra', 'bridge_result', 'debug'],
  omit_by_default: ['schema', 'trace_id', 'debug', 'bridge_result'],
  omit_rules: [
    {
      field: 'schema',
      value_prefix: 'BlueprintHelper.',
      preserve_when_parent_field_equals: {
        field: 'schema',
        value: 'BlueprintHelper.ExternalGraphAnchor.v1',
      },
    },
    { field: 'trace_id' },
    { field: 'debug' },
    { field: 'bridge_result' },
    { field: 'execution_policy' },
    { field: 'scope_policy' },
    { field: 'should_compile', parent_path_suffix: ['validation'] },
    { field: 'should_save', parent_path_suffix: ['validation'] },
    { field: 'target_assets', parent_has_field: 'target' },
    { field: 'task_run_id', parent_path_suffix: ['task'] },
    { field: 'target_assets', parent_path_suffix: ['task'] },
  ],
  extra_omit_rules: [
    { field: 'taskPlan', parent_path: [] },
  ],
};

export function selectProjectionFields(input: SelectProjectionFieldsInput): readonly string[] {
  if (input.artifactKind === 'debug_artifact') {
    return input.policy.debug_artifact_fields;
  }
  const withDevelopTiming = (fields: readonly string[]): readonly string[] => (
    input.develop === true ? uniqueStrings([...fields, 'data.timing']) : fields
  );
  if (input.expert === true) {
    return withDevelopTiming(uniqueStrings([...input.policy.full_fields, ...input.policy.expert_fields]));
  }
  if (input.format === 'summary') {
    return withDevelopTiming(input.policy.default_fields);
  }
  if (input.format === 'json') {
    return withDevelopTiming(input.policy.json_fields);
  }
  if (input.format === 'full') {
    return withDevelopTiming(input.policy.full_fields);
  }
  return withDevelopTiming(input.policy.default_fields);
}

export function projectToolResultForCli(input: ProjectToolResultForCliInput): ProjectToolResultForCliOutput {
  const sanitized = sanitizeAgentFacingToolResult(input.tool_result);
  const selectedFields = selectProjectionFields({
    policy: input.policy,
    format: input.format,
    develop: input.develop,
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
  return compactCliValue(projected, policy) as Record<string, unknown>;
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
  return compactCliValue(next, policy, selectOmitRules(policy, {
    includeExtraRules: true,
  })) as Record<string, unknown>;
}

export function compactTaskPlanForArtifact(
  taskPlan: unknown,
  policy: ResultProjectionPolicy = GENERIC_RESULT_PROJECTION_POLICY,
): unknown {
  return compactCliValue(taskPlan, policy);
}

export function projectMetricsReportDataForCli(
  data: Record<string, unknown>,
  format: ResultProjectionFormat,
): Record<string, unknown> {
  if (format !== 'markdown') {
    return data;
  }
  return omitUndefined({
    schema: readString(data['schema']),
    kind: readString(data['kind']),
    window: readString(data['window']),
    summary: asRecord(data['summary']),
    markdown_report_path: readString(data['markdown_report_path']),
  });
}

export function compactCliValue(
  value: unknown,
  policy: ResultProjectionPolicy = GENERIC_RESULT_PROJECTION_POLICY,
  rules: readonly ResultProjectionOmitRule[] = selectOmitRules(policy),
  path: readonly string[] = [],
): unknown {
  if (Array.isArray(value)) {
    return value.map((entry) => compactCliValue(entry, policy, rules, path));
  }
  if (!isRecord(value)) {
    return value;
  }

  const out: Record<string, unknown> = {};
  for (const [key, entry] of Object.entries(value)) {
    if (shouldOmitByRules(key, entry, value, path, rules)) {
      continue;
    }
    out[key] = compactCliValue(entry, policy, rules, [...path, key]);
  }
  return out;
}

export function compactPolicyOnlyFields(
  value: unknown,
  policy: ResultProjectionPolicy = GENERIC_RESULT_PROJECTION_POLICY,
): unknown {
  return compactCliValue(value, policy);
}

function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return isRecord(value) ? value : undefined;
}

function readString(value: unknown): string | undefined {
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function selectOmitRules(
  policy: ResultProjectionPolicy,
  options: { includeExtraRules?: boolean } = {},
): readonly ResultProjectionOmitRule[] {
  const policyRuleFields = new Set(policy.omit_rules.map((rule) => rule.field));
  const legacyRules = policy.omit_by_default
    .filter((field) => !policyRuleFields.has(field))
    .map((field) => ({ field }));
  return options.includeExtraRules === true
    ? [...policy.omit_rules, ...legacyRules, ...policy.extra_omit_rules]
    : [...policy.omit_rules, ...legacyRules];
}

function shouldOmitByRules(
  field: string,
  value: unknown,
  parent: Record<string, unknown>,
  path: readonly string[],
  rules: readonly ResultProjectionOmitRule[],
): boolean {
  return rules.some((rule) =>
    rule.field === field
    && matchesParentPath(path, rule)
    && matchesValue(value, rule)
    && matchesParent(parent, rule)
    && !isPreservedByRule(parent, rule));
}

function matchesParentPath(path: readonly string[], rule: ResultProjectionOmitRule): boolean {
  if (rule.parent_path && !arraysEqual(path, rule.parent_path)) {
    return false;
  }
  if (rule.parent_path_suffix && !hasPathSuffix(path, rule.parent_path_suffix)) {
    return false;
  }
  return true;
}

function matchesValue(value: unknown, rule: ResultProjectionOmitRule): boolean {
  if (!rule.value_prefix) {
    return true;
  }
  return typeof value === 'string' && value.startsWith(rule.value_prefix);
}

function matchesParent(parent: Record<string, unknown>, rule: ResultProjectionOmitRule): boolean {
  return !rule.parent_has_field || Object.hasOwn(parent, rule.parent_has_field);
}

function isPreservedByRule(parent: Record<string, unknown>, rule: ResultProjectionOmitRule): boolean {
  const condition = rule.preserve_when_parent_field_equals;
  return Boolean(condition && parent[condition.field] === condition.value);
}

function arraysEqual(left: readonly string[], right: readonly string[]): boolean {
  return left.length === right.length && left.every((entry, index) => entry === right[index]);
}

function hasPathSuffix(path: readonly string[], suffix: readonly string[]): boolean {
  return suffix.length <= path.length && suffix.every((entry, index) =>
    path[path.length - suffix.length + index] === entry);
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
