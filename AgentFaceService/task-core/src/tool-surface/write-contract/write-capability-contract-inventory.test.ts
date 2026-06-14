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
    'runtime_adapter',
    'preview_execute',
    'write_gate',
    'editor_lifecycle',
    'readback',
    'review_debug',
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
  assert.ok(contracts.some((contract) =>
    contract.preview_execute.classification === 'unknown'
    || contract.input_evidence.validator_refs.length === 0));
});
