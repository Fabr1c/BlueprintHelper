import assert from 'node:assert/strict';
import test from 'node:test';
import {
  WRITE_CAPABILITY_AUDIT_DIMENSIONS,
  WRITE_CAPABILITY_AUDIT_STATUSES,
  WRITE_CAPABILITY_VISIBILITIES,
} from './write-capability-contract-types.js';
import { buildWriteCapabilityContractInventory } from './write-capability-contract-inventory.js';

test('write capability audit vocabulary is explicit and closed', () => {
  assert.deepEqual(WRITE_CAPABILITY_VISIBILITIES, ['active', 'hidden', 'reserved', 'developer_only']);
  assert.deepEqual(WRITE_CAPABILITY_AUDIT_STATUSES, ['pass', 'gap', 'blocked', 'not_applicable']);
  assert.deepEqual(WRITE_CAPABILITY_AUDIT_DIMENSIONS, [
    'discovery',
    'schema',
    'template_shape',
    'compiler_acceptance',
    'runtime_adapter',
    'runtime_visibility',
    'preview_execute',
    'write_gate',
    'editor_lifecycle',
    'help_index_parity',
    'readback',
    'review_debug',
    'result_projection',
    'tests',
  ]);
});

test('inventory includes ordinary write surfaces without owning runtime behavior', () => {
  const contracts = buildWriteCapabilityContractInventory();
  const ids = new Set(contracts.map((contract) => contract.capability_id));

  assert.ok(ids.has('blueprint.write.taskspec.execute'));
  assert.ok(ids.has('editor.write.asset.save'));
  assert.ok([...ids].some((id) => id.startsWith('graphwrite.route.')));
  assert.ok([...ids].some((id) => id.startsWith('non_graphwrite.operation.')));
});

test('inventory records missing evidence as empty evidence instead of inventing facts', () => {
  const contracts = buildWriteCapabilityContractInventory();

  assert.ok(contracts.every((contract) => contract.schema === 'BlueprintHelper.WriteCapabilityContractAudit.v1'));
  assert.ok(contracts.every((contract) => contract.source_refs.length > 0));
  assert.ok(contracts.some((contract) => contract.preview_execute.classification === 'unknown'));
});

test('inventory records contract drift evidence refs for every contract', () => {
  const contracts = buildWriteCapabilityContractInventory();

  assert.ok(contracts.length > 0);
  assert.ok(contracts.every((contract) => contract.input_evidence.template_refs.length > 0));
  assert.ok(contracts.every((contract) => contract.input_evidence.validator_refs.length > 0));
  assert.ok(contracts.every((contract) => contract.preview_execute.evidence.length > 0));
  assert.ok(contracts.every((contract) => contract.tests.ts_tests.length > 0));
  assert.ok(contracts
    .filter((contract) => contract.capability_id.startsWith('graphwrite.route.'))
    .every((contract) => contract.runtime_adapter_id && contract.runtime_adapter_id.length > 0));
});
