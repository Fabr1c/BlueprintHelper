import assert from 'node:assert/strict';
import test from 'node:test';

import {
  EXECUTION_RECEIPT_IDENTITY_FIELDS,
  buildExecutionReceiptIdentity,
  mergeExecutionReceiptIdentity,
  protectExecutionReceiptFields,
} from './execution-receipt-projection.js';

const receipt = {
  schema: 'BlueprintHelper.ExecutionReceipt.v1',
  receipt_id: 'receipt_projection',
  cli_run_id: 'cli_projection',
  preview_id: 'preview_projection',
  task_run_id: 'task_projection',
  task_spec_hash: 'a'.repeat(64),
  task_plan_hash: 'b'.repeat(64),
  policy_hash: 'c'.repeat(64),
  status: 'applied',
  target_assets: ['/Game/BP/BP_Door'],
  created_at: '2026-06-20T00:00:00.000Z',
  updated_at: '2026-06-20T00:00:00.000Z',
} as const;

test('buildExecutionReceiptIdentity exposes only stable receipt identity fields', () => {
  assert.deepEqual(buildExecutionReceiptIdentity(receipt), {
    receipt_id: 'receipt_projection',
    cli_run_id: 'cli_projection',
    preview_id: 'preview_projection',
    task_run_id: 'task_projection',
    task_spec_hash: 'a'.repeat(64),
    task_plan_hash: 'b'.repeat(64),
    policy_hash: 'c'.repeat(64),
  });
});

test('mergeExecutionReceiptIdentity restores receipt identity after select or omit shaping', () => {
  const shaped = mergeExecutionReceiptIdentity({ status: 'executed' }, receipt);

  assert.equal(shaped.status, 'executed');
  assert.equal(shaped.receipt_id, 'receipt_projection');
  assert.equal(shaped.task_run_id, 'task_projection');
  assert.deepEqual(shaped.receipt, buildExecutionReceiptIdentity(receipt));
});

test('protectExecutionReceiptFields keeps receipt identity in protected CLI fields', () => {
  const protectedFields = protectExecutionReceiptFields(['ok', 'status']);

  for (const field of EXECUTION_RECEIPT_IDENTITY_FIELDS) {
    assert.equal(protectedFields.includes(field), true);
  }
  assert.equal(protectedFields.includes('receipt'), true);
});
