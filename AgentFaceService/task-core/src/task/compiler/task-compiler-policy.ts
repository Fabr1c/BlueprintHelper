import type { TaskSpec } from '../schema/task-schemas.js';
import {
  TaskSpecCompileError,
  type TaskCompilerStrategyId,
} from './task-compiler.js';
import {
  TaskCompilerRegistry,
  isTsFastPathTaskType,
} from './task-compiler-registry.js';

export type TaskCompilerStrategyMode = TaskCompilerStrategyId | 'auto';
export type TaskCompilerParityStatus = 'passed' | 'failed' | 'unknown';

export interface TaskCompilerParityEntry {
  status: TaskCompilerParityStatus;
  reason?: string;
}

export type TaskCompilerParityTable = Record<string, TaskCompilerParityEntry>;

export interface TaskCompilerPolicyOptions {
  strategyMode?: TaskCompilerStrategyMode;
  parityTable?: TaskCompilerParityTable;
  enableTsFastPath?: boolean;
}

export interface TaskCompilerSelection {
  strategyId: TaskCompilerStrategyId;
  parityStatus?: TaskCompilerParityStatus;
  parityReason?: string;
  fallbackReason?: string;
}

interface TaskCompilerEligibility {
  strategyId?: TaskCompilerStrategyId;
  parityStatus?: TaskCompilerParityStatus;
  parityReason?: string;
  fallbackReason?: string;
}

export interface TaskCompilerPolicy {
  select(taskSpec: TaskSpec, registry: TaskCompilerRegistry): TaskCompilerSelection;
}

export const DEFAULT_TASK_COMPILER_PARITY_TABLE: TaskCompilerParityTable = {
  create_blueprint_feature: { status: 'passed', reason: 'covered_by_task_plan_parity_tests' },
  edit_blueprint_graph: { status: 'passed', reason: 'covered_by_task_plan_parity_tests' },
  edit_blueprint_variables: { status: 'passed', reason: 'covered_by_task_plan_parity_tests' },
  edit_object_properties: { status: 'passed', reason: 'covered_by_task_plan_parity_tests' },
  edit_blueprint_signature: { status: 'passed', reason: 'covered_by_task_plan_parity_tests' },
};

const STRATEGY_MODE_ALIASES: Record<string, TaskCompilerStrategyMode> = {
  auto: 'auto',
  canonical: 'canonical_python',
  canonical_python: 'canonical_python',
  python: 'canonical_python',
  ts: 'ts_fast_path',
  ts_fast_path: 'ts_fast_path',
  python_worker: 'python_worker',
};

export function createTaskCompilerPolicy(options: TaskCompilerPolicyOptions = {}): TaskCompilerPolicy {
  const strategyMode = options.strategyMode ?? strategyModeFromEnv() ?? 'auto';
  const parityTable = options.parityTable ?? DEFAULT_TASK_COMPILER_PARITY_TABLE;
  const enableTsFastPath = options.enableTsFastPath ?? process.env['BPH_TASK_COMPILER_DISABLE_TS_FAST_PATH'] !== '1';

  return {
    select(taskSpec, registry) {
      if (strategyMode !== 'auto') {
        return selectForcedStrategy(taskSpec, registry, strategyMode, parityTable, enableTsFastPath);
      }

      const tsEligibility = evaluateTsFastPathEligibility(taskSpec, registry, parityTable, enableTsFastPath);
      if (tsEligibility.strategyId) {
        return {
          strategyId: tsEligibility.strategyId,
          parityStatus: tsEligibility.parityStatus,
          parityReason: tsEligibility.parityReason,
          fallbackReason: tsEligibility.fallbackReason,
        };
      }

      return {
        strategyId: 'canonical_python',
        parityStatus: tsEligibility.parityStatus,
        parityReason: tsEligibility.parityReason,
        fallbackReason: tsEligibility.fallbackReason,
      };
    },
  };
}

function selectForcedStrategy(
  taskSpec: TaskSpec,
  registry: TaskCompilerRegistry,
  strategyId: TaskCompilerStrategyId,
  parityTable: TaskCompilerParityTable,
  enableTsFastPath: boolean,
): TaskCompilerSelection {
  const strategy = registry.require(strategyId);
  const tsEligibility = strategyId === 'ts_fast_path'
    ? evaluateTsFastPathEligibility(taskSpec, registry, parityTable, enableTsFastPath)
    : undefined;

  if (strategyId === 'ts_fast_path' && tsEligibility?.strategyId !== 'ts_fast_path') {
    throw unavailableStrategy(strategyId, taskSpec, tsEligibility?.fallbackReason ?? 'ts_fast_path_unavailable');
  }
  if (!strategy.canCompile(taskSpec, { dryRun: true })) {
    throw unavailableStrategy(strategyId, taskSpec, 'strategy_cannot_compile_task_type');
  }

  return {
    strategyId,
    parityStatus: tsEligibility?.parityStatus,
    parityReason: tsEligibility?.parityReason,
  };
}

function evaluateTsFastPathEligibility(
  taskSpec: TaskSpec,
  registry: TaskCompilerRegistry,
  parityTable: TaskCompilerParityTable,
  enableTsFastPath: boolean,
): TaskCompilerEligibility {
  const parity = parityTable[taskSpec.task_type] ?? { status: 'unknown', reason: 'missing_parity_entry' };
  const strategy = registry.get('ts_fast_path');

  if (!enableTsFastPath) {
    return {
      parityStatus: parity.status,
      parityReason: parity.reason,
      fallbackReason: 'ts_fast_path_disabled',
    };
  }
  if (!strategy || !isTsFastPathTaskType(taskSpec.task_type) || !strategy.canCompile(taskSpec, { dryRun: true })) {
    return {
      parityStatus: parity.status,
      parityReason: parity.reason,
      fallbackReason: 'ts_fast_path_not_supported',
    };
  }
  if (parity.status !== 'passed') {
    return {
      parityStatus: parity.status,
      parityReason: parity.reason,
      fallbackReason: `ts_fast_path_parity_${parity.status}`,
    };
  }

  return {
    strategyId: 'ts_fast_path',
    parityStatus: parity.status,
    parityReason: parity.reason,
  };
}

function strategyModeFromEnv(): TaskCompilerStrategyMode | undefined {
  const raw = process.env['BPH_TASK_COMPILER_STRATEGY'];
  if (!raw) return undefined;
  const mode = STRATEGY_MODE_ALIASES[raw.trim().toLowerCase()];
  if (!mode) {
    throw new TaskSpecCompileError(
      'invalid_task_compiler_strategy',
      `Invalid BPH_TASK_COMPILER_STRATEGY: ${raw}`,
      [{
        code: 'invalid_task_compiler_strategy',
        path: 'BPH_TASK_COMPILER_STRATEGY',
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
