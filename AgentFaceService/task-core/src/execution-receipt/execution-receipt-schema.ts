export const EXECUTION_RECEIPT_SCHEMA = 'BlueprintHelper.ExecutionReceipt.v1';

export const EXECUTION_RECEIPT_STATUSES = [
  'created',
  'previewed',
  'preview_blocked',
  'executing',
  'applied',
  'failed',
  'readback_required',
  'readback_verified',
  'reportable',
] as const;

export type ExecutionReceiptStatus = typeof EXECUTION_RECEIPT_STATUSES[number];

export const EXECUTION_RECEIPT_VERIFICATION_STATUSES = [
  'not_required',
  'pending_readback',
  'verified',
  'failed',
] as const;

export type ExecutionReceiptVerificationStatus = typeof EXECUTION_RECEIPT_VERIFICATION_STATUSES[number];

export type ExecutionReceiptErrorCode =
  | 'receipt_missing'
  | 'receipt_hash_mismatch'
  | 'receipt_task_run_mismatch'
  | 'receipt_readback_mismatch'
  | 'artifact_write_failed'
  | 'artifact_write_warning'
  | 'journal_write_failed'
  | 'journal_receipt_missing'
  | 'readback_required'
  | 'readback_not_verified'
  | 'receipt_not_reportable'
  | 'receipt_not_found';

export interface ExecutionReceipt {
  schema: typeof EXECUTION_RECEIPT_SCHEMA;
  receipt_id: string;
  request_id?: string;
  cli_run_id?: string;
  preview_id?: string;
  preview_token_hash?: string;
  task_run_id?: string;
  task_spec_hash: string;
  task_plan_hash?: string;
  policy_hash?: string;
  verification_hash?: string;
  verification_status?: ExecutionReceiptVerificationStatus;
  target_assets?: readonly string[];
  status: ExecutionReceiptStatus;
  artifact_refs?: Readonly<Record<string, string>>;
  journal_ref?: string;
  readback_ref?: string;
  created_at?: string;
  updated_at?: string;
  error_code?: ExecutionReceiptErrorCode;
  safe_next_action?: string;
}

export type ExecutionReceiptParseResult =
  | { ok: true; receipt: ExecutionReceipt }
  | { ok: false; error: { code: ExecutionReceiptErrorCode; message: string; safe_next_action: string } };

export type ExecutionReceiptNormalizeResult =
  | { ok: true; receipt: ExecutionReceipt; reportable: true }
  | {
      ok: false;
      legacy_incomplete: true;
      reportable: false;
      error: { code: ExecutionReceiptErrorCode; message: string; safe_next_action: string };
    };

export function parseExecutionReceipt(value: unknown): ExecutionReceiptParseResult {
  const record = asRecord(value);
  if (!record) {
    return receiptError('receipt_missing', 'ExecutionReceipt must be an object.');
  }

  if (record['schema'] !== EXECUTION_RECEIPT_SCHEMA) {
    return receiptError('receipt_missing', `ExecutionReceipt schema must be ${EXECUTION_RECEIPT_SCHEMA}.`);
  }

  for (const field of ['receipt_id', 'task_spec_hash', 'status']) {
    if (!readNonEmptyString(record[field])) {
      return receiptError('receipt_missing', `ExecutionReceipt is missing required field: ${field}.`);
    }
  }

  if (!isExecutionReceiptStatus(record['status'])) {
    return receiptError('receipt_missing', `ExecutionReceipt status is invalid: ${String(record['status'])}.`);
  }

  const receipt: ExecutionReceipt = {
    schema: EXECUTION_RECEIPT_SCHEMA,
    receipt_id: readNonEmptyString(record['receipt_id']) as string,
    task_spec_hash: readNonEmptyString(record['task_spec_hash']) as string,
    status: record['status'],
    ...(readNonEmptyString(record['request_id']) ? { request_id: readNonEmptyString(record['request_id']) } : {}),
    ...(readNonEmptyString(record['cli_run_id']) ? { cli_run_id: readNonEmptyString(record['cli_run_id']) } : {}),
    ...(readNonEmptyString(record['preview_id']) ? { preview_id: readNonEmptyString(record['preview_id']) } : {}),
    ...(readNonEmptyString(record['preview_token_hash']) ? { preview_token_hash: readNonEmptyString(record['preview_token_hash']) } : {}),
    ...(readNonEmptyString(record['task_run_id']) ? { task_run_id: readNonEmptyString(record['task_run_id']) } : {}),
    ...(readNonEmptyString(record['task_plan_hash']) ? { task_plan_hash: readNonEmptyString(record['task_plan_hash']) } : {}),
    ...(readNonEmptyString(record['policy_hash']) ? { policy_hash: readNonEmptyString(record['policy_hash']) } : {}),
    ...(readNonEmptyString(record['verification_hash']) ? { verification_hash: readNonEmptyString(record['verification_hash']) } : {}),
    ...(isExecutionReceiptVerificationStatus(record['verification_status']) ? { verification_status: record['verification_status'] } : {}),
    ...(arrayOfStrings(record['target_assets']).length > 0 ? { target_assets: arrayOfStrings(record['target_assets']) } : {}),
    ...(stringRecord(record['artifact_refs']) ? { artifact_refs: stringRecord(record['artifact_refs']) } : {}),
    ...(readNonEmptyString(record['journal_ref']) ? { journal_ref: readNonEmptyString(record['journal_ref']) } : {}),
    ...(readNonEmptyString(record['readback_ref']) ? { readback_ref: readNonEmptyString(record['readback_ref']) } : {}),
    ...(readNonEmptyString(record['created_at']) ? { created_at: readNonEmptyString(record['created_at']) } : {}),
    ...(readNonEmptyString(record['updated_at']) ? { updated_at: readNonEmptyString(record['updated_at']) } : {}),
    ...(isExecutionReceiptErrorCode(record['error_code']) ? { error_code: record['error_code'] } : {}),
    ...(readNonEmptyString(record['safe_next_action']) ? { safe_next_action: readNonEmptyString(record['safe_next_action']) } : {}),
  };

  return { ok: true, receipt };
}

export function normalizeExecutionReceipt(source: unknown): ExecutionReceiptNormalizeResult {
  const record = asRecord(source);
  const nestedReceipt = asRecord(record?.['receipt']);
  const parsed = parseExecutionReceipt(nestedReceipt ?? source);
  if (parsed.ok) {
    return { ok: true, receipt: parsed.receipt, reportable: true };
  }

  return {
    ok: false,
    legacy_incomplete: true,
    reportable: false,
    error: receiptErrorDetails(
      nestedReceipt ? parsed.error.code : 'journal_receipt_missing',
      nestedReceipt ? parsed.error.message : 'TaskRunJournal is missing ExecutionReceipt evidence.',
    ),
  };
}

export function safeNextActionForReceiptError(code: ExecutionReceiptErrorCode): string {
  switch (code) {
    case 'receipt_missing':
    case 'receipt_not_found':
      return 'rerun_preview';
    case 'receipt_hash_mismatch':
      return 'rerun_preview_from_current_task_spec';
    case 'artifact_write_failed':
    case 'artifact_write_warning':
      return 'inspect_stdout_receipt_and_retry_artifact_export';
    case 'journal_write_failed':
    case 'journal_receipt_missing':
      return 'query_execution_receipt_or_retry_journal_persist';
    case 'readback_required':
    case 'readback_not_verified':
      return 'run_readback_for_receipt';
    case 'receipt_readback_mismatch':
      return 'rerun_readback_with_receipt_target';
    case 'receipt_task_run_mismatch':
    case 'receipt_not_reportable':
      return 'stop_and_report_receipt_evidence_mismatch';
    default:
      return 'stop_and_report_receipt_evidence_mismatch';
  }
}

function receiptError(code: ExecutionReceiptErrorCode, message: string): ExecutionReceiptParseResult {
  return { ok: false, error: receiptErrorDetails(code, message) };
}

function receiptErrorDetails(code: ExecutionReceiptErrorCode, message: string) {
  return {
    code,
    message,
    safe_next_action: safeNextActionForReceiptError(code),
  };
}

function isExecutionReceiptStatus(value: unknown): value is ExecutionReceiptStatus {
  return typeof value === 'string' && (EXECUTION_RECEIPT_STATUSES as readonly string[]).includes(value);
}

function isExecutionReceiptVerificationStatus(value: unknown): value is ExecutionReceiptVerificationStatus {
  return typeof value === 'string' && (EXECUTION_RECEIPT_VERIFICATION_STATUSES as readonly string[]).includes(value);
}

function isExecutionReceiptErrorCode(value: unknown): value is ExecutionReceiptErrorCode {
  return typeof value === 'string' && [
    'receipt_missing',
    'receipt_hash_mismatch',
    'receipt_task_run_mismatch',
    'receipt_readback_mismatch',
    'artifact_write_failed',
    'artifact_write_warning',
    'journal_write_failed',
    'journal_receipt_missing',
    'readback_required',
    'readback_not_verified',
    'receipt_not_reportable',
    'receipt_not_found',
  ].includes(value);
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

function readNonEmptyString(value: unknown): string | undefined {
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

function arrayOfStrings(value: unknown): string[] {
  return Array.isArray(value)
    ? value.filter((entry): entry is string => typeof entry === 'string' && entry.length > 0)
    : [];
}

function stringRecord(value: unknown): Record<string, string> | undefined {
  const record = asRecord(value);
  if (!record) {
    return undefined;
  }
  const entries = Object.entries(record).filter((entry): entry is [string, string] => typeof entry[1] === 'string');
  return entries.length > 0 ? Object.fromEntries(entries) : undefined;
}
