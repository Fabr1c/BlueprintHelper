import type { MetricsErrorCategory } from './metrics-types.js';

export interface MetricsErrorClassification {
  category: MetricsErrorCategory;
  code?: string;
  issue_path?: string;
  detail_kind?: MetricsErrorDetailKind;
}

export type MetricsErrorDetailKind =
  | 'route_mismatch_with_suggested_route'
  | 'component_tree_safety_boundary'
  | 'suggested_route_property_failure'
  | 'unsupported_property_shape';

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
      'review_baseline_dirty_target_assets',
      'class_default_setter_signature_unsupported',
      'class_default_setter_readback_mismatch',
      'class_default_setter_restore_readback_mismatch',
    ],
  },
] as const;

const ERROR_CATEGORY_BY_CODE = new Map<string, Exclude<MetricsErrorCategory, 'unknown'>>(
  ERROR_RULE_GROUPS.flatMap((group) => group.codes.map((code) => [code, group.category] as const)),
);

const SUGGESTED_ROUTE_PROPERTY_FAILURE_CODES = new Set([
  'class_default_property_not_writable',
  'class_default_property_not_found',
  'type_mismatch',
]);

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
  const suggestedRoute = selectDetailRecord(raw, issue, 'suggested_route');
  const blockedBoundary = selectDetailRecord(raw, issue, 'blocked_boundary');

  if (hint) {
    return {
      category: hint,
      ...(code ? { code } : {}),
      ...(issuePath ? { issue_path: issuePath } : {}),
    };
  }

  const hintClassification = classifyHintedError(code, suggestedRoute, blockedBoundary);
  if (hintClassification) {
    return {
      ...hintClassification,
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

function classifyHintedError(
  code: string | undefined,
  suggestedRoute: Record<string, unknown> | undefined,
  blockedBoundary: Record<string, unknown> | undefined,
): Pick<MetricsErrorClassification, 'category' | 'detail_kind'> | undefined {
  if (
    code === 'class_default_property_setter_required' &&
    (
      readString(suggestedRoute?.['route_id']) === 'blueprint_class_settings.class_default_setter' ||
      readString(suggestedRoute?.['operation_id']) === 'set_class_default_via_setter' ||
      readString(suggestedRoute?.['task_type']) === 'edit_blueprint_class_settings'
    )
  ) {
    return {
      category: 'parameter_error',
      detail_kind: 'route_mismatch_with_suggested_route',
    };
  }

  if (
    code === 'component_not_owned_scs' &&
    readString(suggestedRoute?.['route_id']) === 'blueprint_class_settings.class_default'
  ) {
    return {
      category: 'parameter_error',
      detail_kind: 'route_mismatch_with_suggested_route',
    };
  }

  if (
    code === 'component_not_owned_scs' &&
    !suggestedRoute &&
    readString(blockedBoundary?.['boundary_id']) === 'component_tree_owned_scs_only'
  ) {
    return {
      category: 'capability_boundary',
      detail_kind: 'component_tree_safety_boundary',
    };
  }

  if (
    code &&
    SUGGESTED_ROUTE_PROPERTY_FAILURE_CODES.has(code) &&
    (
      readString(suggestedRoute?.['route_id']) === 'blueprint_class_settings.class_default' ||
      readString(suggestedRoute?.['task_type']) === 'edit_blueprint_class_settings'
    )
  ) {
    return {
      category: 'parameter_error',
      detail_kind: 'suggested_route_property_failure',
    };
  }

  if (code === 'class_default_setter_signature_unsupported') {
    return {
      category: 'runtime_state_error',
      detail_kind: 'unsupported_property_shape',
    };
  }

  return undefined;
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

function selectDetailRecord(
  raw: Record<string, unknown> | undefined,
  issue: Record<string, unknown> | undefined,
  field: 'suggested_route' | 'blocked_boundary',
): Record<string, unknown> | undefined {
  const nestedError = asRecord(raw?.['error']);
  const nestedData = asRecord(raw?.['data']);
  return asRecord(raw?.[field])
    ?? asRecord(nestedError?.[field])
    ?? asRecord(issue?.[field])
    ?? asRecord(nestedData?.[field]);
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
