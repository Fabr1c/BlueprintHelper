import assert from 'node:assert/strict';
import test from 'node:test';

import { graphWriteVariantNames, makeGraphWriteGeneralityOperations } from './graphwrite-generality-matrix.js';

test('GraphWrite generality matrix is derived from supported capability contract rows', () => {
  const operations = makeGraphWriteGeneralityOperations();
  assert.equal(operations.length, 139);
  assert.ok(operations.some((operation) => operation.operationId === 'function_action.call_function'));
  assert.ok(operations.some((operation) => operation.operationId === 'container.array.identical'));
  assert.ok(operations.some((operation) => operation.operationId === 'generic_ops.control.branch'));
  assert.ok(operations.some((operation) => operation.operationId === 'op.boolean_and'));
  assert.equal(operations.some((operation) => operation.operationId === 'op.array_identical'), false);
});

test('GraphWrite generality matrix applies singleton and ten-variant rules', () => {
  const operations = makeGraphWriteGeneralityOperations();
  const branch = operations.find((operation) => operation.operationId === 'generic_ops.control.branch');
  const call = operations.find((operation) => operation.operationId === 'function_action.call_function');
  const containerAdd = operations.find((operation) => operation.operationId === 'container.array.add');
  assert.ok(branch);
  assert.ok(call);
  assert.ok(containerAdd);
  assert.equal(branch.requiredVariantCount, 1);
  assert.equal(graphWriteVariantNames(branch).length, 1);
  assert.equal(call.requiredVariantCount, 10);
  assert.equal(graphWriteVariantNames(call).length, 10);
  assert.equal(call.spawnCandidateNames.length, 10);
  assert.notDeepEqual(new Set(call.spawnCandidateNames).size, 1);
  assert.equal(containerAdd.requiredVariantCount, 1);
  assert.equal(containerAdd.variantMode, 'limited_real_nodes');
});
