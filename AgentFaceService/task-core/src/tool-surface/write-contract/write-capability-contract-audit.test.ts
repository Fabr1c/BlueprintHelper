import assert from 'node:assert/strict';
import test from 'node:test';

import { auditWriteCapabilityContracts } from './write-capability-contract-audit.js';
import type { WriteCapabilityContract } from './write-capability-contract-types.js';

test('audit reports gaps without fixing the underlying system', () => {
  const result = auditWriteCapabilityContracts([contract({
    capability_id: 'example.write.missing_readback',
    readback: { read_context_refs: [], verification_fields: [], recipe_refs: [] },
  })], { generatedAt: '2026-06-14T00:00:00.000Z' });

  assert.ok(result.rows.some((row) =>
    row.capability_id === 'example.write.missing_readback'
    && row.dimension === 'readback'
    && row.status === 'gap'
    && row.remediation.length > 0));
});

test('audit reports readback gap when only generic route evidence exists', () => {
  const result = auditWriteCapabilityContracts([contract({
    capability_id: 'example.write.generic_readback_only',
    readback: {
      read_context_refs: ['read.blueprint.asset.asset.diagnostics_json'],
      verification_fields: ['asset_path'],
      recipe_refs: [],
    },
  })], { generatedAt: '2026-06-14T00:00:00.000Z' });

  assert.ok(result.rows.some((row) =>
    row.capability_id === 'example.write.generic_readback_only'
    && row.dimension === 'readback'
    && row.status === 'gap'
    && row.code === 'readback_recipe_missing'));
});

test('audit reports write gate gap when source-control evidence is missing for execute tools', () => {
  const result = auditWriteCapabilityContracts([contract({
    capability_id: 'example.write.execute_without_source_control',
    tool_name: 'blueprinthelper_execute_task',
    gates: {
      write_session_evidence: ['write_session'],
      source_control_evidence: [],
      close_save_evidence: [],
    },
  })], { generatedAt: '2026-06-14T00:00:00.000Z' });

  assert.ok(result.rows.some((row) =>
    row.capability_id === 'example.write.execute_without_source_control'
    && row.dimension === 'write_gate'
    && row.status === 'gap'
    && row.code === 'source_control_gate_evidence_missing'));
});

test('audit reports unknown preview execute classification as a gap', () => {
  const result = auditWriteCapabilityContracts([contract({
    capability_id: 'example.write.unknown_preview',
    preview_execute: {
      classification: 'unknown',
      evidence: [],
      runtime_only_notes: [],
    },
  })], { generatedAt: '2026-06-14T00:00:00.000Z' });

  assert.ok(result.rows.some((row) =>
    row.capability_id === 'example.write.unknown_preview'
    && row.dimension === 'preview_execute'
    && row.status === 'gap'
    && row.code === 'preview_execute_classification_unknown'));
});

test('audit marks developer-only review debug as not applicable when no evidence is required', () => {
  const result = auditWriteCapabilityContracts([contract({
    capability_id: 'review.write.apply_action',
    visibility: 'developer_only',
    review_debug: {
      review_v2_evidence: [],
      debug_bundle_evidence: [],
    },
  })], { generatedAt: '2026-06-14T00:00:00.000Z' });

  assert.ok(result.rows.some((row) =>
    row.capability_id === 'review.write.apply_action'
    && row.dimension === 'review_debug'
    && row.status === 'not_applicable'));
});

function contract(overrides: Partial<WriteCapabilityContract> = {}): WriteCapabilityContract {
  return {
    schema: 'BlueprintHelper.WriteCapabilityContractAudit.v1',
    capability_id: 'example.write.ok',
    tool_name: 'blueprinthelper_execute_task',
    write_family: 'example',
    visibility: 'active',
    source_refs: [{ kind: 'catalog', id: 'example.write.ok' }],
    input_evidence: {
      schema_refs: ['schema'],
      template_refs: ['template'],
      help_refs: ['help'],
      validator_refs: ['validator'],
    },
    preview_execute: {
      classification: 'shared_policy',
      evidence: ['policy'],
      runtime_only_notes: [],
    },
    gates: {
      write_session_evidence: ['write_session'],
      source_control_evidence: ['source_control'],
      close_save_evidence: [],
    },
    readback: {
      read_context_refs: ['read.context'],
      verification_fields: ['asset_path'],
      recipe_refs: ['recipe'],
    },
    review_debug: {
      review_v2_evidence: ['review_v2'],
      debug_bundle_evidence: ['debug_bundle'],
    },
    tests: {
      ts_tests: ['test.ts'],
      cli_tests: [],
      ue_tests: [],
      e2e_refs: [],
    },
    ...overrides,
  };
}
