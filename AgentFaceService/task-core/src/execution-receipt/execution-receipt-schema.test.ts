import assert from 'node:assert/strict';
import test from 'node:test';

import {
  EXECUTION_RECEIPT_SCHEMA,
  normalizeExecutionReceipt,
  parseExecutionReceipt,
  safeNextActionForReceiptError,
} from './execution-receipt-schema.js';

test('parseExecutionReceipt accepts a complete ExecutionReceipt.v1 identity', () => {
  const parsed = parseExecutionReceipt({
    schema: EXECUTION_RECEIPT_SCHEMA,
    receipt_id: 'receipt_001',
    cli_run_id: 'cli_run_001',
    preview_id: 'preview_001',
    task_spec_hash: 'a'.repeat(64),
    task_plan_hash: 'b'.repeat(64),
    policy_hash: 'c'.repeat(64),
    status: 'previewed',
    target_assets: ['/Game/BP/BP_Door'],
    artifact_refs: { full_result: 'result.json' },
    created_at: '2026-06-20T00:00:00.000Z',
    updated_at: '2026-06-20T00:00:00.000Z',
  });

  assert.equal(parsed.ok, true);
  assert.equal(parsed.receipt.receipt_id, 'receipt_001');
  assert.equal(parsed.receipt.status, 'previewed');
  assert.deepEqual(parsed.receipt.target_assets, ['/Game/BP/BP_Door']);
});

test('parseExecutionReceipt preserves verification identity and status', () => {
  const parsed = parseExecutionReceipt({
    schema: EXECUTION_RECEIPT_SCHEMA,
    receipt_id: 'receipt_verified',
    task_spec_hash: 'a'.repeat(64),
    verification_hash: 'b'.repeat(64),
    verification_status: 'pending_readback',
    status: 'applied',
  });

  assert.equal(parsed.ok, true);
  const receipt = parsed.receipt as unknown as Record<string, unknown>;
  assert.equal(receipt.verification_hash, 'b'.repeat(64));
  assert.equal(receipt.verification_status, 'pending_readback');
});

test('parseExecutionReceipt rejects missing required identity fields', () => {
  const parsed = parseExecutionReceipt({
    schema: EXECUTION_RECEIPT_SCHEMA,
    task_spec_hash: 'a'.repeat(64),
    status: 'previewed',
  });

  assert.equal(parsed.ok, false);
  assert.equal(parsed.error.code, 'receipt_missing');
  assert.match(parsed.error.message, /receipt_id/);
});

test('normalizeExecutionReceipt marks legacy journals without receipt as incomplete evidence', () => {
  const normalized = normalizeExecutionReceipt({
    schema: 'BlueprintHelper.TaskRunJournal.v1',
    task_run_id: 'task_legacy',
    status: 'completed',
  });

  assert.equal(normalized.ok, false);
  assert.equal(normalized.legacy_incomplete, true);
  assert.equal(normalized.error.code, 'journal_receipt_missing');
  assert.equal(normalized.reportable, false);
});

test('safeNextActionForReceiptError maps fail-closed errors to actionable recovery', () => {
  assert.equal(safeNextActionForReceiptError('receipt_hash_mismatch'), 'rerun_preview_from_current_task_spec');
  assert.equal(safeNextActionForReceiptError('readback_required'), 'run_readback_for_receipt');
  assert.equal(safeNextActionForReceiptError('artifact_write_warning'), 'inspect_stdout_receipt_and_retry_artifact_export');
});
