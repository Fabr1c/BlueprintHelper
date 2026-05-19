import type {
  TaskIssue,
  TaskPlan,
  TaskPreviewToken,
  TaskSpec,
} from '../schema/task-schemas.js';
import {
  createExecutionPolicyHash,
  createTaskPlanHash,
  createTaskSpecHash,
} from './task-plan-hash.js';

export interface TaskPreviewCacheOutcome {
  previewId: string;
  taskSpec: TaskSpec;
  taskPlan: TaskPlan;
  passed: boolean;
  issues: TaskIssue[];
}

export type TaskPreviewTokenValidation =
  | {
    ok: true;
    entry: TaskPreviewCacheEntry;
  }
  | {
    ok: false;
    code: 'preview_token_missing' | 'preview_token_expired' | 'preview_token_mismatch';
    message: string;
    field?: string;
    expected?: string;
    actual?: string;
  };

export interface TaskPreviewCacheEntry {
  previewId: string;
  taskPlan: TaskPlan;
  passed: boolean;
  issues: TaskIssue[];
  token: TaskPreviewToken;
  expiresAtMs: number;
  lastAccessedAtMs: number;
}

export class TaskPreviewCache {
  private readonly entries = new Map<string, TaskPreviewCacheEntry>();

  constructor(
    private readonly maxEntries = 64,
    private readonly ttlMs = 10 * 60 * 1000,
    private readonly nowMs: () => number = () => Date.now(),
  ) {}

  store(outcome: TaskPreviewCacheOutcome): TaskPreviewToken {
    this.pruneExpired();
    const now = this.nowMs();
    const token: TaskPreviewToken = {
      preview_id: outcome.previewId,
      task_plan_hash: createTaskPlanHash(outcome.taskPlan),
      task_spec_hash: createTaskSpecHash(outcome.taskSpec),
      execution_policy_hash: createExecutionPolicyHash(outcome.taskPlan.execution_policy),
      created_at: new Date(now).toISOString(),
    };

    this.entries.set(outcome.previewId, {
      previewId: outcome.previewId,
      taskPlan: outcome.taskPlan,
      passed: outcome.passed,
      issues: outcome.issues,
      token,
      expiresAtMs: now + this.ttlMs,
      lastAccessedAtMs: now,
    });
    this.trimToBounds();
    return token;
  }

  validate(
    token: TaskPreviewToken,
    taskSpec: TaskSpec,
    taskPlan: TaskPlan,
  ): TaskPreviewTokenValidation {
    this.pruneExpired();
    const entry = this.entries.get(token.preview_id);
    if (!entry) {
      return {
        ok: false,
        code: 'preview_token_missing',
        message: `Preview token preview_id=${token.preview_id} is not available in the process-local preview cache.`,
        field: 'preview_token.preview_id',
        actual: token.preview_id,
      };
    }

    const now = this.nowMs();
    if (entry.expiresAtMs <= now) {
      this.entries.delete(token.preview_id);
      return {
        ok: false,
        code: 'preview_token_expired',
        message: `Preview token preview_id=${token.preview_id} has expired; run preview_task again.`,
        field: 'preview_token.preview_id',
        actual: token.preview_id,
      };
    }

    const tokenFieldMismatch = this.findTokenFieldMismatch(token, entry.token);
    if (tokenFieldMismatch) {
      return tokenFieldMismatch;
    }

    const expectedTaskSpecHash = createTaskSpecHash(taskSpec);
    if (token.task_spec_hash !== expectedTaskSpecHash) {
      return this.hashMismatch('preview_token.task_spec_hash', expectedTaskSpecHash, token.task_spec_hash);
    }

    const expectedTaskPlanHash = createTaskPlanHash(taskPlan);
    if (token.task_plan_hash !== expectedTaskPlanHash) {
      return this.hashMismatch('preview_token.task_plan_hash', expectedTaskPlanHash, token.task_plan_hash);
    }

    const expectedPolicyHash = createExecutionPolicyHash(taskPlan.execution_policy);
    if (token.execution_policy_hash !== expectedPolicyHash) {
      return this.hashMismatch('preview_token.execution_policy_hash', expectedPolicyHash, token.execution_policy_hash);
    }

    entry.lastAccessedAtMs = now;
    return { ok: true, entry };
  }

  private findTokenFieldMismatch(
    actual: TaskPreviewToken,
    expected: TaskPreviewToken,
  ): TaskPreviewTokenValidation | undefined {
    const fields = [
      'task_plan_hash',
      'task_spec_hash',
      'execution_policy_hash',
      'created_at',
    ] as const;
    for (const field of fields) {
      if (actual[field] !== expected[field]) {
        return {
          ok: false,
          code: 'preview_token_mismatch',
          message: `Preview token field ${field} does not match the cached preview.`,
          field: `preview_token.${field}`,
          expected: expected[field],
          actual: actual[field],
        };
      }
    }
    return undefined;
  }

  private hashMismatch(
    field: string,
    expected: string,
    actual: string,
  ): TaskPreviewTokenValidation {
    return {
      ok: false,
      code: 'preview_token_mismatch',
      message: `${field} does not match the current TaskSpec compile result.`,
      field,
      expected,
      actual,
    };
  }

  private pruneExpired(): void {
    const now = this.nowMs();
    for (const [previewId, entry] of this.entries) {
      if (entry.expiresAtMs <= now) {
        this.entries.delete(previewId);
      }
    }
  }

  private trimToBounds(): void {
    while (this.entries.size > this.maxEntries) {
      const oldest = [...this.entries.values()]
        .sort((left, right) => left.lastAccessedAtMs - right.lastAccessedAtMs)[0];
      if (!oldest) {
        return;
      }
      this.entries.delete(oldest.previewId);
    }
  }
}
