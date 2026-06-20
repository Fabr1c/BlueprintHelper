import { createHash } from 'node:crypto';

export function createExecutionReceiptStableHash(value: unknown): string {
  return createHash('sha256')
    .update(stableJsonStringify(value))
    .digest('hex');
}

export function createExecutionReceiptId(input: {
  taskSpecHash: string;
  taskPlanHash?: string;
  previewId?: string;
  previewTokenHash?: string;
  taskRunId?: string;
}): string {
  const hash = createExecutionReceiptStableHash({
    task_spec_hash: input.taskSpecHash,
    task_plan_hash: input.taskPlanHash,
    preview_id: input.previewId,
    preview_token_hash: input.previewTokenHash,
    task_run_id: input.taskRunId,
  });
  return `receipt_${hash.slice(0, 32)}`;
}

export function createCliRunId(input: {
  taskSpecHash: string;
  taskPlanHash?: string;
  previewId?: string;
  receiptId?: string;
}): string {
  const hash = createExecutionReceiptStableHash({
    task_spec_hash: input.taskSpecHash,
    task_plan_hash: input.taskPlanHash,
    preview_id: input.previewId,
    receipt_id: input.receiptId,
  });
  return `cli_run_${hash.slice(0, 24)}`;
}

export function createPreviewTokenHash(previewToken: string): string {
  return createExecutionReceiptStableHash({ preview_token: previewToken });
}

function stableJsonStringify(value: unknown): string {
  return JSON.stringify(toStableJsonValue(value));
}

function toStableJsonValue(value: unknown): unknown {
  if (value === null || typeof value === 'string' || typeof value === 'number' || typeof value === 'boolean') {
    return value;
  }
  if (Array.isArray(value)) {
    return value.map((entry) => {
      const stableEntry = toStableJsonValue(entry);
      return stableEntry === undefined ? null : stableEntry;
    });
  }
  if (typeof value === 'object') {
    const source = value as Record<string, unknown>;
    const stableObject: Record<string, unknown> = {};
    for (const key of Object.keys(source).sort()) {
      const stableEntry = toStableJsonValue(source[key]);
      if (stableEntry !== undefined) {
        stableObject[key] = stableEntry;
      }
    }
    return stableObject;
  }
  return undefined;
}
