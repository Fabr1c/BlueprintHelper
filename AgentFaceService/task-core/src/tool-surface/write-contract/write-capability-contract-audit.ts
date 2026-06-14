import {
  WRITE_CAPABILITY_AUDIT_STATUSES,
  type WriteCapabilityAuditDimension,
  type WriteCapabilityAuditResult,
  type WriteCapabilityAuditRow,
  type WriteCapabilityAuditStatus,
  type WriteCapabilityContract,
} from './write-capability-contract-types.js';

export interface AuditWriteCapabilityContractsOptions {
  readonly generatedAt?: string;
}

export function auditWriteCapabilityContracts(
  contracts: readonly WriteCapabilityContract[],
  options: AuditWriteCapabilityContractsOptions = {},
): WriteCapabilityAuditResult {
  const rows = contracts.flatMap(auditContract);
  return {
    schema: 'BlueprintHelper.WriteCapabilityContractAuditResult.v1',
    generated_at: options.generatedAt ?? new Date().toISOString(),
    contracts: [...contracts],
    rows,
    summary: summarize(rows),
  };
}

function auditContract(contract: WriteCapabilityContract): WriteCapabilityAuditRow[] {
  return [
    auditDiscovery(contract),
    auditSchema(contract),
    auditRuntimeAdapter(contract),
    auditPreviewExecute(contract),
    auditWriteGate(contract),
    auditEditorLifecycle(contract),
    auditReadback(contract),
    auditReviewDebug(contract),
    auditTests(contract),
  ];
}

function auditDiscovery(contract: WriteCapabilityContract): WriteCapabilityAuditRow {
  return contract.source_refs.length > 0
    ? pass(contract, 'discovery', 'discovery_source_present', 'Capability has descriptor or registry source evidence.', sourceEvidence(contract), 'No remediation required.')
    : gap(contract, 'discovery', 'discovery_source_missing', 'Capability has no source descriptor evidence.', [], 'Derive this capability from an existing descriptor, registry, or route source.');
}

function auditSchema(contract: WriteCapabilityContract): WriteCapabilityAuditRow {
  const missing = [
    contract.input_evidence.schema_refs.length === 0 ? 'schema' : undefined,
    contract.input_evidence.template_refs.length === 0 ? 'template' : undefined,
    contract.input_evidence.help_refs.length === 0 ? 'help' : undefined,
    contract.input_evidence.validator_refs.length === 0 ? 'validator' : undefined,
  ].filter((entry): entry is string => entry !== undefined);
  return missing.length === 0
    ? pass(contract, 'schema', 'schema_evidence_present', 'Schema, template, help, and validator evidence are present.', [
      ...contract.input_evidence.schema_refs,
      ...contract.input_evidence.template_refs,
      ...contract.input_evidence.help_refs,
      ...contract.input_evidence.validator_refs,
    ], 'No remediation required.')
    : gap(contract, 'schema', 'schema_evidence_missing', `Missing input evidence: ${missing.join(', ')}.`, [
      ...contract.input_evidence.schema_refs,
      ...contract.input_evidence.template_refs,
      ...contract.input_evidence.help_refs,
      ...contract.input_evidence.validator_refs,
    ], 'Attach missing schema/template/help/validator evidence in the owning descriptor or validator boundary.');
}

function auditRuntimeAdapter(contract: WriteCapabilityContract): WriteCapabilityAuditRow {
  if (!contract.capability_id.startsWith('graphwrite.route.')) {
    return na(contract, 'runtime_adapter', 'runtime_adapter_not_applicable', 'Capability is not a GraphWrite route.', [], 'No remediation required.');
  }
  return contract.runtime_adapter_id
    ? pass(contract, 'runtime_adapter', 'runtime_adapter_present', 'GraphWrite route has runtime adapter id evidence.', [`runtime_adapter_id:${contract.runtime_adapter_id}`], 'No remediation required.')
    : gap(contract, 'runtime_adapter', 'runtime_adapter_missing', 'GraphWrite route lacks runtime adapter id evidence.', [], 'Attach runtime adapter id evidence and verify UE adapter registration in a separate remediation plan.');
}

function auditPreviewExecute(contract: WriteCapabilityContract): WriteCapabilityAuditRow {
  if (contract.preview_execute.classification === 'unknown') {
    return gap(contract, 'preview_execute', 'preview_execute_classification_unknown', 'Preview/execute classification is unknown.', contract.preview_execute.evidence, 'Classify invariants as preview_decidable, shared_policy, or runtime_only in the owning descriptor.');
  }
  return pass(contract, 'preview_execute', 'preview_execute_classification_present', 'Preview/execute classification is explicit.', [
    `classification:${contract.preview_execute.classification}`,
    ...contract.preview_execute.evidence,
  ], 'No remediation required.');
}

function auditWriteGate(contract: WriteCapabilityContract): WriteCapabilityAuditRow {
  if (contract.visibility === 'developer_only' || contract.capability_id === 'project.write.write_session') {
    return na(contract, 'write_gate', 'write_gate_not_applicable', 'Capability is developer-only or is the write-session request itself.', [], 'No remediation required.');
  }
  const evidence = [
    ...contract.gates.write_session_evidence,
    ...contract.gates.source_control_evidence,
  ];
  if (requiresSourceControlGate(contract) && contract.gates.source_control_evidence.length === 0) {
    return gap(contract, 'write_gate', 'source_control_gate_evidence_missing', 'Capability lacks source-control gate evidence.', evidence, 'Record source-control gate evidence from the existing write pipeline or split a runtime remediation plan.');
  }
  return evidence.length > 0
    ? pass(contract, 'write_gate', 'write_gate_evidence_present', 'Write gate evidence is present.', evidence, 'No remediation required.')
    : gap(contract, 'write_gate', 'write_gate_evidence_missing', 'Capability has no write-session or source-control gate evidence.', [], 'Record gate evidence from the existing write pipeline or split a runtime remediation plan.');
}

function auditEditorLifecycle(contract: WriteCapabilityContract): WriteCapabilityAuditRow {
  const requiresCloseSaveEvidence = contract.capability_id.includes('save') || contract.tool_name.includes('save');
  if (!requiresCloseSaveEvidence) {
    return na(contract, 'editor_lifecycle', 'editor_lifecycle_not_applicable', 'Capability is not a save/close lifecycle entry.', [], 'No remediation required.');
  }
  return contract.gates.close_save_evidence.length > 0
    ? pass(contract, 'editor_lifecycle', 'close_save_evidence_present', 'Close/save lifecycle evidence is present.', contract.gates.close_save_evidence, 'No remediation required.')
    : gap(contract, 'editor_lifecycle', 'close_save_evidence_missing', 'Save/close lifecycle evidence is missing.', [], 'Record close/save gate evidence in a separate lifecycle remediation plan.');
}

function auditReadback(contract: WriteCapabilityContract): WriteCapabilityAuditRow {
  if (contract.write_family === 'editor' || contract.write_family === 'project' || contract.visibility === 'developer_only') {
    return na(contract, 'readback', 'readback_not_applicable', 'Capability does not directly mutate readable asset content in this audit.', [], 'No remediation required.');
  }
  const evidence = [
    ...contract.readback.read_context_refs,
    ...contract.readback.verification_fields.map((field) => `verify:${field}`),
    ...contract.readback.recipe_refs.map((recipe) => `recipe:${recipe}`),
  ];
  if (contract.readback.recipe_refs.length === 0) {
    return gap(contract, 'readback', 'readback_recipe_missing', 'Capability has readback route evidence but no capability/family readback recipe.', evidence, 'Add a capability- or family-specific readback recipe in the owning read_context/projection boundary.');
  }
  return contract.readback.read_context_refs.length > 0 && contract.readback.verification_fields.length > 0
    ? pass(contract, 'readback', 'readback_evidence_present', 'Readback route and verification field evidence are present.', evidence, 'No remediation required.')
    : gap(contract, 'readback', 'readback_evidence_missing', 'Capability lacks readback route or verification fields.', evidence, 'Add a readback recipe in the owning read_context/projection boundary.');
}

function auditReviewDebug(contract: WriteCapabilityContract): WriteCapabilityAuditRow {
  if (contract.visibility === 'developer_only') {
    return na(contract, 'review_debug', 'review_debug_developer_only', 'Developer-only write is not required to expose ordinary Agent Review/Debug evidence in Phase 1.', [], 'No remediation required.');
  }
  const evidence = [
    ...contract.review_debug.review_v2_evidence,
    ...contract.review_debug.debug_bundle_evidence,
  ];
  return contract.review_debug.review_v2_evidence.length > 0 && contract.review_debug.debug_bundle_evidence.length > 0
    ? pass(contract, 'review_debug', 'review_debug_evidence_present', 'Review v2 and DebugBundle evidence are present.', evidence, 'No remediation required.')
    : gap(contract, 'review_debug', 'review_debug_evidence_missing', 'Capability lacks Review v2 or DebugBundle evidence.', evidence, 'Map the existing mutation boundary to Review/Debug evidence in a separate remediation plan.');
}

function auditTests(contract: WriteCapabilityContract): WriteCapabilityAuditRow {
  const missing = [
    contract.tests.ts_tests.length === 0 ? 'ts_tests' : undefined,
    contract.tests.cli_tests.length === 0 ? 'cli_tests' : undefined,
    contract.tests.ue_tests.length === 0 ? 'ue_tests' : undefined,
    contract.tests.e2e_refs.length === 0 ? 'e2e_refs' : undefined,
  ].filter((entry): entry is string => entry !== undefined);
  const evidence = [
    ...contract.tests.ts_tests,
    ...contract.tests.cli_tests,
    ...contract.tests.ue_tests,
    ...contract.tests.e2e_refs,
  ];
  return missing.length === 0
    ? pass(contract, 'tests', 'test_evidence_present', 'TS, CLI, UE, and E2E evidence are present.', evidence, 'No remediation required.')
    : gap(contract, 'tests', 'test_evidence_missing', `Missing test evidence: ${missing.join(', ')}.`, evidence, 'Add missing test evidence in focused remediation plans; do not treat static audit as runtime proof.');
}

function pass(
  contract: WriteCapabilityContract,
  dimension: WriteCapabilityAuditDimension,
  code: string,
  message: string,
  evidence: readonly string[],
  remediation: string,
): WriteCapabilityAuditRow {
  return row(contract, dimension, 'pass', code, message, evidence, remediation);
}

function gap(
  contract: WriteCapabilityContract,
  dimension: WriteCapabilityAuditDimension,
  code: string,
  message: string,
  evidence: readonly string[],
  remediation: string,
): WriteCapabilityAuditRow {
  return row(contract, dimension, 'gap', code, message, evidence, remediation);
}

function na(
  contract: WriteCapabilityContract,
  dimension: WriteCapabilityAuditDimension,
  code: string,
  message: string,
  evidence: readonly string[],
  remediation: string,
): WriteCapabilityAuditRow {
  return row(contract, dimension, 'not_applicable', code, message, evidence, remediation);
}

function row(
  contract: WriteCapabilityContract,
  dimension: WriteCapabilityAuditDimension,
  status: WriteCapabilityAuditStatus,
  code: string,
  message: string,
  evidence: readonly string[],
  remediation: string,
): WriteCapabilityAuditRow {
  return {
    capability_id: contract.capability_id,
    dimension,
    status,
    code,
    message,
    evidence: [...evidence],
    remediation,
  };
}

function sourceEvidence(contract: WriteCapabilityContract): string[] {
  return contract.source_refs.map((source) => `${source.kind}:${source.id}`);
}

function requiresSourceControlGate(contract: WriteCapabilityContract): boolean {
  if (contract.capability_id === 'editor.write.source_control.checkout') {
    return false;
  }
  return contract.tool_name === 'blueprinthelper_execute_task'
    || contract.tool_name === 'blueprint_save_asset'
    || contract.capability_id.startsWith('graphwrite.route.')
    || contract.capability_id.startsWith('non_graphwrite.operation.');
}

function summarize(rows: readonly WriteCapabilityAuditRow[]): Record<WriteCapabilityAuditStatus, number> {
  const summary = Object.fromEntries(WRITE_CAPABILITY_AUDIT_STATUSES.map((status) => [status, 0])) as Record<WriteCapabilityAuditStatus, number>;
  for (const rowEntry of rows) {
    summary[rowEntry.status] += 1;
  }
  return summary;
}
