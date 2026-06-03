import type { MetricsErrorCategory } from './metrics-types.js';

export interface MetricsErrorClassification {
  category: MetricsErrorCategory;
  code?: string;
  issue_path?: string;
}

interface ErrorRuleGroup {
  category: Exclude<MetricsErrorCategory, 'unknown'>;
  codes: readonly string[];
}

const ERROR_RULE_GROUPS: readonly ErrorRuleGroup[] = [
  {
    category: 'capability_boundary',
    codes: [
      'unsupported_task_type',
      'unsupported_scope_policy',
      'operation_not_supported',
      'graphwrite_connectivity_failed',
      'unsupported_graph_write_strategy',
      'unsupported_graph_write_op',
      'unsupported_variable_strategy',
      'unsupported_variable_op',
      'unsupported_asset_factory_strategy',
      'unsupported_component_strategy',
      'unsupported_widget_strategy',
      'unsupported_data_table_strategy',
      'unsupported_object_property_strategy',
      'unsupported_signature_strategy',
      'unsupported_taskplan_operation',
      'unsupported_graph_write_patch',
      'unsupported_graph_write_merge',
      'unsupported_graph_write_anchor',
      'unsupported_external_graph_write_merge',
      'unsupported_external_graph_anchor',
    ],
  },
  {
    category: 'parameter_error',
    codes: [
      'invalid_literal',
      'invalid_type',
      'invalid_enum_value',
      'missing_required_field',
      'missing_required_logic_body',
      'missing_property_value',
      'missing_target_asset_path',
      'missing_graph_name',
      'logic_spec_required',
      'invalid_read_context_payload',
      'taskspec_semantic_invalid',
      'unconsumed_pure_data_node',
      'malformed_json',
      'invalid_field_path',
    ],
  },
  {
    category: 'context_error',
    codes: [
      'asset_not_found',
      'context_required',
      'context_stale',
      'ambiguous_asset',
      'ambiguous_function',
    ],
  },
  {
    category: 'runtime_state_error',
    codes: [
      'bridge_unavailable',
      'bridge_error',
      'compile_failed',
      'save_failed',
      'readback_failed',
    ],
  },
] as const;

const ERROR_CATEGORY_BY_CODE = new Map<string, Exclude<MetricsErrorCategory, 'unknown'>>(
  ERROR_RULE_GROUPS.flatMap((group) => group.codes.map((code) => [code, group.category] as const)),
);

export function classifyMetricsError(input: unknown): MetricsErrorClassification {
  const raw = asRecord(input);
  const hint = firstDefinedCategory([
    raw?.['error_category_hint'],
    raw?.['error_category'],
    asRecord(raw?.['error'])?.['error_category_hint'],
    asRecord(raw?.['error'])?.['error_category'],
    raw?.['category_hint'],
    asRecord(raw?.['error'])?.['category_hint'],
  ]);
  const issue = selectPrimaryIssue(raw);
  const code = firstNonEmptyString([
    raw?.['code'],
    raw?.['error_code'],
    raw?.['issue_code'],
    asRecord(raw?.['error'])?.['code'],
    asRecord(raw?.['error'])?.['issue_code'],
    issue?.['code'],
    issue?.['issue_code'],
    raw?.['status'],
  ]);
  const issuePath = firstNonEmptyString([
    raw?.['issue_path'],
    raw?.['path'],
    asRecord(raw?.['error'])?.['field'],
    issue?.['path'],
  ]);

  if (hint) {
    return {
      category: hint,
      ...(code ? { code } : {}),
      ...(issuePath ? { issue_path: issuePath } : {}),
    };
  }

  const inferredCategory = (code ? ERROR_CATEGORY_BY_CODE.get(code) : undefined)
    ?? (issuePath?.startsWith('target.') ? 'context_error' : undefined)
    ?? 'unknown';

  return {
    category: inferredCategory,
    ...(code ? { code } : {}),
    ...(issuePath ? { issue_path: issuePath } : {}),
  };
}

function selectPrimaryIssue(raw: Record<string, unknown> | undefined): Record<string, unknown> | undefined {
  const directIssue = asRecord(raw?.['issue']);
  if (directIssue) {
    return directIssue;
  }

  const directIssues = asIssueArray(raw?.['issues']);
  if (directIssues.length > 0) {
    return directIssues[0];
  }

  const nestedError = asRecord(raw?.['error']);
  const nestedIssues = asIssueArray(nestedError?.['issues']);
  return nestedIssues[0];
}

function asIssueArray(value: unknown): Record<string, unknown>[] {
  if (!Array.isArray(value)) {
    return [];
  }

  return value
    .map((entry) => asRecord(entry))
    .filter((entry): entry is Record<string, unknown> => entry !== undefined);
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

function readCategory(value: unknown): MetricsErrorCategory | undefined {
  return value === 'capability_boundary'
    || value === 'parameter_error'
    || value === 'context_error'
    || value === 'runtime_state_error'
    || value === 'unknown'
    ? value
    : undefined;
}

function firstDefinedCategory(values: unknown[]): MetricsErrorCategory | undefined {
  for (const value of values) {
    const category = readCategory(value);
    if (category) {
      return category;
    }
  }

  return undefined;
}

function firstNonEmptyString(values: unknown[]): string | undefined {
  for (const value of values) {
    const text = readString(value);
    if (text) {
      return text;
    }
  }

  return undefined;
}

function readString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}
