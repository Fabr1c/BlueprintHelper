import type { TaskSpec } from '../task/schema/task-schemas.js';
import type {
  MetricsEvent,
  MetricsEventSink,
  MetricsIssueSummary,
  MetricsOperationIdentity,
  MetricsStatus,
} from './metrics-types.js';
import { NOOP_METRICS_SINK } from './metrics-types.js';
import { classifyMetricsError, type MetricsErrorClassification } from './error-classifier.js';
import type { MetricsStore } from './metrics-store.js';
import { hashStableJson } from './stable-hash.js';
import { createMetricsTaskKey } from './task-key.js';

export interface CreateMetricsCollectorOptions {
  sink?: MetricsEventSink;
  store?: MetricsStore;
  now?: Date | (() => Date);
}

export interface MetricsCollector {
  recordTaskPreviewCompleted(input: RecordTaskSpecCompletedInput): Promise<void>;
  recordTaskExecuteCompleted(input: RecordTaskSpecCompletedInput & {
    validationPassed?: boolean;
    readbackPassed?: boolean;
  }): Promise<void>;
  recordTaskStepCompleted(input: RecordTaskStepCompletedInput): Promise<void>;
  recordToolCompleted(input: RecordToolCompletedInput): Promise<void>;
  recordValidationCompleted(input: RecordTaskEvidenceCompletedInput): Promise<void>;
  recordReadbackCompleted(input: RecordTaskEvidenceCompletedInput): Promise<void>;
}

export interface RecordTaskSpecCompletedInput extends MetricsOperationIdentity {
  taskSpec: TaskSpec;
  passed: boolean;
  toolResult?: unknown;
  duration_ms?: number;
  tool_name?: string;
}

export interface RecordTaskStepCompletedInput extends MetricsOperationIdentity {
  taskSpec?: TaskSpec;
  status: MetricsStatus;
  toolResult?: unknown;
  duration_ms?: number;
  tool_name?: string;
}

export interface RecordToolCompletedInput extends MetricsOperationIdentity {
  tool_name: string;
  status?: MetricsStatus;
  passed?: boolean;
  toolResult?: unknown;
  duration_ms?: number;
}

export interface RecordTaskEvidenceCompletedInput {
  taskSpec: TaskSpec;
  passed: boolean;
  toolResult?: unknown;
  duration_ms?: number;
  tool_name?: string;
}

export function createMetricsCollector(options: CreateMetricsCollectorOptions = {}): MetricsCollector {
  const sink = options.sink ?? options.store ?? NOOP_METRICS_SINK;
  const store = options.store;
  const now = createNowProvider(options.now);

  return {
    async recordTaskPreviewCompleted(input) {
      await emit({
        ...createTaskSpecEventBase(input.taskSpec, now()),
        event_type: 'taskspec_preview_completed',
        tool_name: input.tool_name ?? 'blueprinthelper_preview_task',
        status: statusFromPassed(input.passed),
        duration_ms: input.duration_ms,
        correctness_basis: 'not_applicable',
        capability: input.capability,
        semantic_operation: input.semantic_operation,
        ...errorFields(input.passed, input.toolResult),
      });
    },

    async recordTaskExecuteCompleted(input) {
      const correctnessBasis = input.passed && input.validationPassed === true && input.readbackPassed === true
        ? 'validation_readback'
        : input.passed
          ? 'pending_confirmation'
          : 'not_applicable';

      await emit({
        ...createTaskSpecEventBase(input.taskSpec, now()),
        event_type: 'taskspec_execute_completed',
        tool_name: input.tool_name ?? 'blueprinthelper_execute_task',
        status: statusFromPassed(input.passed),
        duration_ms: input.duration_ms,
        correctness_basis: correctnessBasis,
        capability: input.capability,
        semantic_operation: input.semantic_operation,
        ...errorFields(input.passed, input.toolResult),
      });
    },

    async recordTaskStepCompleted(input) {
      await emit({
        schema: 'BlueprintHelper.MetricsEvent.v1',
        timestamp: now().toISOString(),
        event_type: 'taskstep_completed',
        ...(input.tool_name ? { tool_name: input.tool_name } : {}),
        ...(input.taskSpec ? createTaskSpecIdentity(input.taskSpec) : {}),
        status: input.status,
        duration_ms: input.duration_ms,
        correctness_basis: 'not_applicable',
        capability: input.capability,
        semantic_operation: input.semantic_operation,
        ...errorFields(input.status === 'success', input.toolResult),
      });
    },

    async recordToolCompleted(input) {
      const status = input.status ?? statusFromPassed(input.passed ?? true);
      await emit({
        schema: 'BlueprintHelper.MetricsEvent.v1',
        timestamp: now().toISOString(),
        event_type: 'tool_completed',
        tool_name: input.tool_name,
        status,
        duration_ms: input.duration_ms,
        correctness_basis: 'not_applicable',
        capability: input.capability,
        semantic_operation: input.semantic_operation,
        ...errorFields(status === 'success', input.toolResult),
      });
    },

    async recordValidationCompleted(input) {
      await emit({
        ...createTaskSpecEventBase(input.taskSpec, now()),
        event_type: 'validation_completed',
        tool_name: input.tool_name,
        status: statusFromPassed(input.passed),
        duration_ms: input.duration_ms,
        correctness_basis: 'not_applicable',
        ...errorFields(input.passed, input.toolResult),
      });
    },

    async recordReadbackCompleted(input) {
      await emit({
        ...createTaskSpecEventBase(input.taskSpec, now()),
        event_type: 'readback_completed',
        tool_name: input.tool_name,
        status: statusFromPassed(input.passed),
        duration_ms: input.duration_ms,
        correctness_basis: 'not_applicable',
        ...errorFields(input.passed, input.toolResult),
      });
    },
  };

  async function emit(event: MetricsEvent): Promise<void> {
    try {
      await sink.record(removeUndefined(event));
      if (store !== undefined) {
        await store.upsertEpisodeAttempt(event);
      }
    } catch {
      return;
    }
  }
}

function createTaskSpecEventBase(taskSpec: TaskSpec, now: Date): Omit<MetricsEvent, 'event_type' | 'status'> {
  return {
    schema: 'BlueprintHelper.MetricsEvent.v1',
    timestamp: now.toISOString(),
    ...createTaskSpecIdentity(taskSpec),
  };
}

function createTaskSpecIdentity(taskSpec: TaskSpec): Pick<MetricsEvent, 'task_key' | 'task_spec_hash'> {
  return {
    task_key: createMetricsTaskKey(taskSpec),
    task_spec_hash: hashStableJson(taskSpec),
  };
}

function statusFromPassed(passed: boolean): MetricsStatus {
  return passed ? 'success' : 'failed';
}

function errorFields(
  passed: boolean,
  source: unknown,
): Pick<MetricsEvent, 'error_category' | 'error_code' | 'issue'> {
  if (passed) {
    return {};
  }

  const record = asRecord(source);
  const classification = classifyMetricsError(source);
  const issue = readIssueSummary(record, classification);
  const errorCode = classification.code
    ?? readString(record?.error_code)
    ?? readString(record?.code)
    ?? issue?.code;

  return removeUndefined({
    error_category: classification.category,
    error_code: errorCode,
    issue,
  });
}

function readIssueSummary(
  record: Record<string, unknown> | undefined,
  classification: MetricsErrorClassification,
): MetricsIssueSummary | undefined {
  const issueRecord = selectIssueRecord(record);
  const code = readString(issueRecord?.code)
    ?? readString(issueRecord?.issue_code)
    ?? classification.code;
  const pathValue = readString(issueRecord?.path)
    ?? classification.issue_path;
  const message = readString(issueRecord?.message);

  if (code === undefined && pathValue === undefined && message === undefined) {
    return undefined;
  }

  return removeUndefined({
    code,
    path: pathValue,
    message_digest: message === undefined ? undefined : hashStableJson({ message }),
  });
}

function selectIssueRecord(record: Record<string, unknown> | undefined): Record<string, unknown> | undefined {
  const nestedError = asRecord(record?.error);
  return asRecord(record?.issue)
    ?? firstIssue(record?.issues)
    ?? asRecord(nestedError?.issue)
    ?? firstIssue(nestedError?.issues);
}

function firstIssue(value: unknown): Record<string, unknown> | undefined {
  return Array.isArray(value) ? asRecord(value[0]) : undefined;
}

function readString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

function removeUndefined<T extends object>(value: T): T {
  return Object.fromEntries(Object.entries(value).filter(([, entry]) => entry !== undefined)) as T;
}

function createNowProvider(now: Date | (() => Date) | undefined): () => Date {
  if (typeof now === 'function') {
    return () => new Date(now().getTime());
  }
  if (now instanceof Date) {
    return () => new Date(now.getTime());
  }
  return () => new Date();
}
