import { performance } from 'node:perf_hooks';
import type { ToolResultBase } from '../../result/tool-result.js';

export interface TaskTimingStage {
  name: string;
  started_at_ms: number;
  duration_ms: number;
}

export interface TaskTimingTraceSnapshot {
  schema: 'BlueprintHelper.TimingTrace.v1';
  source: string;
  operation: string;
  timing_id: string;
  total_ms: number;
  stages: TaskTimingStage[];
  nested?: Array<Record<string, unknown>>;
}

let timingCounter = 0;

export class TaskTimingTrace {
  private readonly timingId: string;
  private readonly source: string;
  private readonly operation: string;
  private readonly startedAt = performance.now();
  private readonly stages: TaskTimingStage[] = [];
  private readonly nested: Array<Record<string, unknown>> = [];

  private constructor(source: string, operation: string) {
    this.source = source;
    this.operation = operation;
    this.timingId = `timing_${Date.now()}_${++timingCounter}`;
  }

  static start(operation: string, source = 'agentface_task_runner'): TaskTimingTrace {
    return new TaskTimingTrace(source, operation);
  }

  measure<T>(name: string, fn: () => T): T {
    const startedAt = performance.now();
    try {
      return fn();
    } finally {
      this.addStage(name, startedAt, performance.now());
    }
  }

  async measureAsync<T>(name: string, fn: () => Promise<T>): Promise<T> {
    const startedAt = performance.now();
    try {
      return await fn();
    } finally {
      this.addStage(name, startedAt, performance.now());
    }
  }

  addNested(name: string, timing: unknown): void {
    if (!timing || typeof timing !== 'object' || Array.isArray(timing)) {
      return;
    }

    this.nested.push({
      name,
      ...(timing as Record<string, unknown>),
    });
  }

  snapshot(): TaskTimingTraceSnapshot {
    return {
      schema: 'BlueprintHelper.TimingTrace.v1',
      source: this.source,
      operation: this.operation,
      timing_id: this.timingId,
      total_ms: roundMs(performance.now() - this.startedAt),
      stages: this.stages.map((stage) => ({ ...stage })),
      ...(this.nested.length > 0 ? { nested: this.nested.map((entry) => ({ ...entry })) } : {}),
    };
  }

  private addStage(name: string, startedAt: number, finishedAt: number): void {
    this.stages.push({
      name,
      started_at_ms: roundMs(startedAt - this.startedAt),
      duration_ms: roundMs(finishedAt - startedAt),
    });
  }
}

export function startTaskTiming(
  enabled: boolean,
  operation: string,
  source = 'agentface_task_runner',
): TaskTimingTrace | undefined {
  return enabled ? TaskTimingTrace.start(operation, source) : undefined;
}

export function hasTaskTiming(timing: TaskTimingTrace | undefined): boolean {
  return timing !== undefined;
}

export function measureTaskTiming<T>(
  timing: TaskTimingTrace | undefined,
  name: string,
  fn: () => T,
): T {
  return timing ? timing.measure(name, fn) : fn();
}

export async function measureTaskTimingAsync<T>(
  timing: TaskTimingTrace | undefined,
  name: string,
  fn: () => Promise<T>,
): Promise<T> {
  return timing ? timing.measureAsync(name, fn) : fn();
}

export function addNestedTaskTiming(
  timing: TaskTimingTrace | undefined,
  name: string,
  value: unknown,
): void {
  timing?.addNested(name, value);
}

export function attachTaskTiming(
  result: ToolResultBase,
  timing: TaskTimingTrace | undefined,
): ToolResultBase {
  if (!timing) {
    return result;
  }

  const data = result.data ?? {};
  result.data = {
    ...data,
    timing: timing.snapshot(),
  };
  return result;
}

export function extractBridgeTiming(value: unknown): Record<string, unknown> | undefined {
  const result = asRecord(value);
  const data = asRecord(result?.['data']);
  const timing = asRecord(data?.['timing']) ?? asRecord(result?.['timing']);
  return timing;
}

function roundMs(value: number): number {
  return Math.round(value * 1000) / 1000;
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}
