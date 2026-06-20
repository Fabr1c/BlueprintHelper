import type { ExecutionReceipt } from './execution-receipt-schema.js';

export const EXECUTION_RECEIPT_IDENTITY_FIELDS = [
  'receipt_id',
  'cli_run_id',
  'preview_id',
  'task_run_id',
  'task_spec_hash',
  'task_plan_hash',
  'policy_hash',
  'verification_hash',
  'verification_status',
] as const;

export type ExecutionReceiptIdentityField = typeof EXECUTION_RECEIPT_IDENTITY_FIELDS[number];

export function buildExecutionReceiptIdentity(receipt: ExecutionReceipt): Record<ExecutionReceiptIdentityField, string | undefined> {
  return omitUndefined({
    receipt_id: receipt.receipt_id,
    cli_run_id: receipt.cli_run_id,
    preview_id: receipt.preview_id,
    task_run_id: receipt.task_run_id,
    task_spec_hash: receipt.task_spec_hash,
    task_plan_hash: receipt.task_plan_hash,
    policy_hash: receipt.policy_hash,
    verification_hash: receipt.verification_hash,
    verification_status: receipt.verification_status,
  }) as Record<ExecutionReceiptIdentityField, string | undefined>;
}

export function mergeExecutionReceiptIdentity<T extends Record<string, unknown>>(
  output: T,
  receipt: ExecutionReceipt | undefined,
): T & Record<string, unknown> {
  if (!receipt) {
    return output;
  }
  const identity = buildExecutionReceiptIdentity(receipt);
  return {
    ...output,
    ...identity,
    receipt: identity,
  };
}

export function protectExecutionReceiptFields(fields: readonly string[]): string[] {
  return uniqueStrings([
    ...fields,
    ...EXECUTION_RECEIPT_IDENTITY_FIELDS,
    'receipt',
  ]);
}

function uniqueStrings(values: readonly string[]): string[] {
  return [...new Set(values)];
}

function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}
