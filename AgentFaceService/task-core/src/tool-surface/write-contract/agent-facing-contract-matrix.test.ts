import assert from 'node:assert/strict';
import test from 'node:test';

import { buildAgentFacingContractMatrix } from './agent-facing-contract-matrix.js';

test('agent-facing contract matrix excludes hidden and reserved write capabilities', () => {
  const matrix = buildAgentFacingContractMatrix();
  const ids = new Set(matrix.map((unit) => unit.capability_id));

  assert.ok(matrix.length > 0);
  assert.ok(matrix.every((unit) => unit.visibility === 'active' || unit.visibility === 'developer_only'));
  assert.equal(ids.has('struct.fields.edit'), false);
  assert.equal(ids.has('material_instance.edit'), false);
  assert.equal(ids.has('material.write.taskspec.execute'), true);
});

test('agent-facing contract matrix keeps GraphWrite active routes tied to runtime adapters', () => {
  const graphWriteUnits = buildAgentFacingContractMatrix()
    .filter((unit) => unit.visibility === 'active' && unit.capability_id.startsWith('graphwrite.route.'));

  assert.ok(graphWriteUnits.length > 0);
  assert.ok(graphWriteUnits.every((unit) => unit.runtime_adapter_id && unit.runtime_adapter_id.length > 0));
});

test('agent-facing contract matrix keeps non-GraphWrite active operations tied to templates', () => {
  const nonGraphWriteUnits = buildAgentFacingContractMatrix()
    .filter((unit) => unit.visibility === 'active' && unit.capability_id.startsWith('non_graphwrite.operation.'));

  assert.ok(nonGraphWriteUnits.length > 0);
  assert.ok(nonGraphWriteUnits.every((unit) => unit.template_ids.length > 0));
});

test('agent-facing contract matrix separates developer-only review action from ordinary active entries', () => {
  const reviewAction = buildAgentFacingContractMatrix()
    .find((unit) => unit.capability_id === 'review.write.apply_action');

  assert.equal(reviewAction?.visibility, 'developer_only');
  assert.equal(reviewAction?.tool_name, 'blueprinthelper_apply_review_action');
});
