export const WRITE_CAPABILITY_VISIBILITIES = ['active', 'hidden', 'reserved', 'developer_only'] as const;
export type WriteCapabilityVisibility = typeof WRITE_CAPABILITY_VISIBILITIES[number];

export const WRITE_CAPABILITY_AUDIT_STATUSES = ['pass', 'gap', 'blocked', 'not_applicable'] as const;
export type WriteCapabilityAuditStatus = typeof WRITE_CAPABILITY_AUDIT_STATUSES[number];

export const WRITE_CAPABILITY_AUDIT_DIMENSIONS = [
  'discovery',
  'schema',
  'runtime_adapter',
  'preview_execute',
  'write_gate',
  'editor_lifecycle',
  'readback',
  'review_debug',
  'tests',
] as const;
export type WriteCapabilityAuditDimension = typeof WRITE_CAPABILITY_AUDIT_DIMENSIONS[number];

export type PreviewExecuteClassification =
  | 'preview_decidable'
  | 'shared_policy'
  | 'runtime_only'
  | 'unknown';

export interface WriteCapabilitySourceRef {
  readonly kind: 'catalog' | 'graphwrite_route' | 'non_graphwrite_operation' | 'bridge_tool' | 'review_tool';
  readonly id: string;
  readonly file?: string;
}

export interface WriteCapabilityContract {
  readonly schema: 'BlueprintHelper.WriteCapabilityContractAudit.v1';
  readonly capability_id: string;
  readonly tool_name: string;
  readonly write_family: string;
  readonly visibility: WriteCapabilityVisibility;
  readonly source_refs: readonly WriteCapabilitySourceRef[];
  readonly task_type?: string;
  readonly route_id?: string;
  readonly runtime_adapter_id?: string;
  readonly input_evidence: {
    readonly schema_refs: readonly string[];
    readonly template_refs: readonly string[];
    readonly help_refs: readonly string[];
    readonly validator_refs: readonly string[];
  };
  readonly preview_execute: {
    readonly classification: PreviewExecuteClassification;
    readonly evidence: readonly string[];
    readonly runtime_only_notes: readonly string[];
  };
  readonly gates: {
    readonly write_session_evidence: readonly string[];
    readonly source_control_evidence: readonly string[];
    readonly close_save_evidence: readonly string[];
  };
  readonly readback: {
    readonly read_context_refs: readonly string[];
    readonly verification_fields: readonly string[];
    readonly recipe_refs: readonly string[];
  };
  readonly review_debug: {
    readonly review_v2_evidence: readonly string[];
    readonly debug_bundle_evidence: readonly string[];
  };
  readonly tests: {
    readonly ts_tests: readonly string[];
    readonly cli_tests: readonly string[];
    readonly ue_tests: readonly string[];
    readonly e2e_refs: readonly string[];
  };
}

export interface WriteCapabilityAuditRow {
  readonly capability_id: string;
  readonly dimension: WriteCapabilityAuditDimension;
  readonly status: WriteCapabilityAuditStatus;
  readonly code: string;
  readonly message: string;
  readonly evidence: readonly string[];
  readonly remediation: string;
}

export interface WriteCapabilityAuditResult {
  readonly schema: 'BlueprintHelper.WriteCapabilityContractAuditResult.v1';
  readonly generated_at: string;
  readonly contracts: readonly WriteCapabilityContract[];
  readonly rows: readonly WriteCapabilityAuditRow[];
  readonly summary: Record<WriteCapabilityAuditStatus, number>;
}
