import type { TaskSpec } from '../schema/task-schemas.js';
import {
  TaskSpecCompileError,
  type TaskCompilerStrategyId,
} from './task-compiler.js';
import {
  TaskCompilerRegistry,
} from './task-compiler-registry.js';

const CANONICAL_TS_STRATEGY_ID: TaskCompilerStrategyId = 'canonical_ts';

const STRATEGY_MODE_ALIASES = {
  auto: 'auto',
  canonical: CANONICAL_TS_STRATEGY_ID,
  canonical_ts: CANONICAL_TS_STRATEGY_ID,
} as const;

export type TaskCompilerStrategyMode = keyof typeof STRATEGY_MODE_ALIASES;

export interface TaskCompilerPolicyOptions {
  strategyMode?: TaskCompilerStrategyMode;
}

export interface TaskCompilerSelection {
  strategyId: TaskCompilerStrategyId;
}

export interface TaskCompilerPolicy {
  select(taskSpec: TaskSpec, registry: TaskCompilerRegistry): TaskCompilerSelection;
}

export function createTaskCompilerPolicy(options: TaskCompilerPolicyOptions = {}): TaskCompilerPolicy {
  const strategyMode = normalizeStrategyMode(
    options.strategyMode ?? strategyModeFromEnv() ?? 'auto',
    'strategyMode',
  );

  return {
    select(taskSpec, registry) {
      const strategyId = strategyMode === 'auto' ? CANONICAL_TS_STRATEGY_ID : strategyMode;
      const strategy = registry.require(strategyId);
      if (!strategy.canCompile(taskSpec, { dryRun: true })) {
        throw unavailableStrategy(strategyId, taskSpec, 'unsupported_task_type');
      }
      return { strategyId };
    },
  };
}

function strategyModeFromEnv(): TaskCompilerStrategyMode | undefined {
  const raw = process.env['BPH_TASK_COMPILER_STRATEGY'];
  if (!raw) return undefined;
  return normalizeStrategyMode(raw, 'BPH_TASK_COMPILER_STRATEGY');
}

function normalizeStrategyMode(raw: string, path: string): 'auto' | TaskCompilerStrategyId {
  const normalized = raw.trim().toLowerCase();
  const mode = STRATEGY_MODE_ALIASES[normalized as TaskCompilerStrategyMode];
  if (!mode) {
    throw new TaskSpecCompileError(
      'invalid_task_compiler_strategy',
      `Invalid task compiler strategy: ${raw}`,
      [{
        code: 'invalid_task_compiler_strategy',
        path,
        message: `Use one of: ${Object.keys(STRATEGY_MODE_ALIASES).join(', ')}.`,
      }],
    );
  }
  return mode;
}

function unavailableStrategy(strategyId: TaskCompilerStrategyId, taskSpec: TaskSpec, reason: string): TaskSpecCompileError {
  return new TaskSpecCompileError(
    'task_compiler_strategy_unavailable',
    `Task compiler strategy ${strategyId} cannot compile task_type=${taskSpec.task_type}: ${reason}.`,
    [{
      code: 'task_compiler_strategy_unavailable',
      path: 'task_type',
      message: reason,
    }],
  );
}
