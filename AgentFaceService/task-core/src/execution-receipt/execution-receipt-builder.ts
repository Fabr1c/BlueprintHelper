import {
  EXECUTION_RECEIPT_SCHEMA,
  type ExecutionReceipt,
  type ExecutionReceiptStatus,
  type ExecutionReceiptVerificationStatus,
  parseExecutionReceipt,
} from './execution-receipt-schema.js';
import {
  createCliRunId,
  createExecutionReceiptId,
  createPreviewTokenHash,
} from './execution-receipt-hash.js';

export interface BuildPreviewExecutionReceiptInput {
  requestId?: string;
  cliRunId?: string;
  receiptId?: string;
  previewId: string;
  previewToken?: string;
  taskSpecHash: string;
  taskPlanHash?: string;
  policyHash?: string;
  verificationHash?: string;
  verificationStatus?: ExecutionReceiptVerificationStatus;
  targetAssets?: string[];
  status: Extract<ExecutionReceiptStatus, 'created' | 'previewed' | 'preview_blocked'>;
  now?: string;
}

export interface BuildExecuteExecutionReceiptInput {
  baseReceipt?: ExecutionReceipt;
  bridgeReceipt?: unknown;
  requestId?: string;
  cliRunId?: string;
  receiptId?: string;
  previewId?: string;
  previewToken?: string;
  taskRunId?: string;
  taskSpecHash: string;
  taskPlanHash?: string;
  policyHash?: string;
  verificationHash?: string;
  verificationStatus?: ExecutionReceiptVerificationStatus;
  targetAssets?: string[];
  status: Extract<ExecutionReceiptStatus, 'executing' | 'applied' | 'failed' | 'readback_required'>;
  journalRef?: string;
  artifactRefs?: Record<string, string>;
  now?: string;
}

export function buildPreviewExecutionReceipt(input: BuildPreviewExecutionReceiptInput): ExecutionReceipt {
  const now = input.now ?? new Date().toISOString();
  const receiptId = input.receiptId ?? createExecutionReceiptId({
    taskSpecHash: input.taskSpecHash,
    taskPlanHash: input.taskPlanHash,
    previewId: input.previewId,
  });
  const cliRunId = input.cliRunId ?? createCliRunId({
    taskSpecHash: input.taskSpecHash,
    taskPlanHash: input.taskPlanHash,
    previewId: input.previewId,
    receiptId,
  });
  return omitUndefined({
    schema: EXECUTION_RECEIPT_SCHEMA,
    receipt_id: receiptId,
    request_id: input.requestId,
    cli_run_id: cliRunId,
    preview_id: input.previewId,
    preview_token_hash: input.previewToken ? createPreviewTokenHash(input.previewToken) : undefined,
    task_spec_hash: input.taskSpecHash,
    task_plan_hash: input.taskPlanHash,
    policy_hash: input.policyHash,
    verification_hash: input.verificationHash,
    verification_status: input.verificationStatus ?? defaultVerificationStatus(input.verificationHash),
    target_assets: input.targetAssets && input.targetAssets.length > 0 ? input.targetAssets : undefined,
    status: input.status,
    created_at: now,
    updated_at: now,
  }) as unknown as ExecutionReceipt;
}

export function buildExecuteExecutionReceipt(input: BuildExecuteExecutionReceiptInput): ExecutionReceipt {
  const parsedBridgeReceipt = parseExecutionReceipt(input.bridgeReceipt);
  if (parsedBridgeReceipt.ok) {
    return {
      ...parsedBridgeReceipt.receipt,
      verification_hash: parsedBridgeReceipt.receipt.verification_hash
        ?? input.verificationHash
        ?? input.baseReceipt?.verification_hash,
      verification_status: parsedBridgeReceipt.receipt.verification_status
        ?? input.verificationStatus
        ?? input.baseReceipt?.verification_status
        ?? defaultVerificationStatus(input.verificationHash ?? input.baseReceipt?.verification_hash),
    };
  }

  const now = input.now ?? new Date().toISOString();
  const base = input.baseReceipt;
  const previewTokenHash = input.previewToken ? createPreviewTokenHash(input.previewToken) : base?.preview_token_hash;
  const receiptId = input.receiptId ?? base?.receipt_id ?? createExecutionReceiptId({
    taskSpecHash: input.taskSpecHash,
    taskPlanHash: input.taskPlanHash ?? base?.task_plan_hash,
    previewId: input.previewId ?? base?.preview_id,
    previewTokenHash,
    taskRunId: input.taskRunId,
  });
  return omitUndefined({
    schema: EXECUTION_RECEIPT_SCHEMA,
    receipt_id: receiptId,
    request_id: input.requestId ?? base?.request_id,
    cli_run_id: input.cliRunId ?? base?.cli_run_id ?? createCliRunId({
      taskSpecHash: input.taskSpecHash,
      taskPlanHash: input.taskPlanHash ?? base?.task_plan_hash,
      previewId: input.previewId ?? base?.preview_id,
      receiptId,
    }),
    preview_id: input.previewId ?? base?.preview_id,
    preview_token_hash: previewTokenHash,
    task_run_id: input.taskRunId ?? base?.task_run_id,
    task_spec_hash: input.taskSpecHash,
    task_plan_hash: input.taskPlanHash ?? base?.task_plan_hash,
    policy_hash: input.policyHash ?? base?.policy_hash,
    verification_hash: input.verificationHash ?? base?.verification_hash,
    verification_status: input.verificationStatus ?? base?.verification_status ?? defaultVerificationStatus(input.verificationHash ?? base?.verification_hash),
    target_assets: input.targetAssets && input.targetAssets.length > 0 ? input.targetAssets : base?.target_assets,
    status: input.status,
    artifact_refs: input.artifactRefs ?? base?.artifact_refs,
    journal_ref: input.journalRef ?? base?.journal_ref,
    readback_ref: base?.readback_ref,
    created_at: base?.created_at ?? now,
    updated_at: now,
  }) as unknown as ExecutionReceipt;
}

export function extractExecutionReceipt(value: unknown): ExecutionReceipt | undefined {
  const parsed = parseExecutionReceipt(value);
  return parsed.ok ? parsed.receipt : undefined;
}

function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}

function defaultVerificationStatus(verificationHash: string | undefined): ExecutionReceiptVerificationStatus | undefined {
  return verificationHash ? 'pending_readback' : undefined;
}
