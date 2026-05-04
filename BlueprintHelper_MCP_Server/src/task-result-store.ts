import {
  TASK_RUN_JOURNAL_SCHEMA,
  type TaskPlan,
} from './task-schemas.js';

let taskCounter = 0;
let previewCounter = 0;

const taskResults = new Map<string, Record<string, unknown>>();

export function nextPreviewId() {
  return `preview_${Date.now()}_${String(++previewCounter).padStart(4, '0')}`;
}

export function nextTaskRunId() {
  return `task_${Date.now()}_${String(++taskCounter).padStart(4, '0')}`;
}

export function storeTaskResult(input: {
  taskRunId: string;
  previewId: string;
  taskPlan: TaskPlan;
  status: 'completed' | 'failed';
  bridgeResult?: Record<string, unknown>;
}) {
  const firstStep = input.taskPlan.steps[0];
  const transactionId = extractTransactionId(input.bridgeResult);
  const journal = {
    schema: TASK_RUN_JOURNAL_SCHEMA,
    task_run_id: input.taskRunId,
    preview_id: input.previewId,
    task_type: input.taskPlan.task_type,
    feature_name: input.taskPlan.task_name,
    status: input.status,
    target_assets: input.taskPlan.target_assets,
    steps: firstStep
      ? [
          {
            step_id: firstStep.step_id,
            operation: firstStep.operation,
            status: input.status === 'completed' ? 'applied' : 'failed',
            ...(transactionId ? { transaction_id: transactionId } : {}),
          },
        ]
      : [],
    bridge_result: input.bridgeResult,
  };
  taskResults.set(input.taskRunId, journal);
  return journal;
}

export function storeTaskRunJournal(taskRunId: string, journal: Record<string, unknown>) {
  taskResults.set(taskRunId, journal);
  return journal;
}

export function getTaskResult(taskRunId: string) {
  return taskResults.get(taskRunId);
}

function extractTransactionId(result: Record<string, unknown> | undefined) {
  if (!result) {
    return undefined;
  }

  const data = getRecord(result, 'data');
  if (!data) {
    return undefined;
  }

  const writeRef = getRecord(data, 'write_ref');
  const transactionId = writeRef?.['transaction_id'];
  if (typeof transactionId === 'string') {
    return transactionId;
  }

  const rawSteps = data?.['steps'];
  const steps = Array.isArray(rawSteps) ? rawSteps : [];
  const firstStep = steps.find((step): step is Record<string, unknown> =>
    step !== null && typeof step === 'object' && !Array.isArray(step),
  );
  const stepResult = getRecord(firstStep, 'result');
  if (!stepResult) {
    return undefined;
  }

  return extractTransactionId(stepResult);
}

function getRecord(record: Record<string, unknown> | undefined, field: string) {
  const value = record?.[field];
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}
