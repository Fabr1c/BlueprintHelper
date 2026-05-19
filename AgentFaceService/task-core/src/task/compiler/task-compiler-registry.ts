import type { TaskSpec } from '../schema/task-schemas.js';
import {
  TaskSpecCompileError,
  compileTaskSpecToTaskPlan,
  createCompiledTaskPlan,
  type TaskCompileOptions,
  type TaskCompilerStrategy,
  type TaskCompilerStrategyId,
} from './task-compiler.js';
import { compileTaskSpecWithPython } from './task-python-orchestrator.js';

const TS_FAST_PATH_TASK_TYPES = new Set([
  'create_blueprint_feature',
  'edit_blueprint_graph',
  'edit_blueprint_variables',
  'edit_object_properties',
  'edit_blueprint_signature',
]);

export class TaskCompilerRegistry {
  private readonly strategies = new Map<TaskCompilerStrategyId, TaskCompilerStrategy>();

  register(strategy: TaskCompilerStrategy): this {
    this.strategies.set(strategy.id, strategy);
    return this;
  }

  get(strategyId: TaskCompilerStrategyId): TaskCompilerStrategy | undefined {
    return this.strategies.get(strategyId);
  }

  require(strategyId: TaskCompilerStrategyId): TaskCompilerStrategy {
    const strategy = this.get(strategyId);
    if (!strategy) {
      throw new TaskSpecCompileError(
        'task_compiler_strategy_not_registered',
        `Task compiler strategy is not registered: ${strategyId}`,
        [{
          code: 'task_compiler_strategy_not_registered',
          path: 'task_type',
          message: `Register compiler strategy ${strategyId} before selecting it.`,
        }],
      );
    }
    return strategy;
  }

  list(): TaskCompilerStrategy[] {
    return Array.from(this.strategies.values());
  }
}

export function createDefaultTaskCompilerRegistry(): TaskCompilerRegistry {
  return new TaskCompilerRegistry()
    .register(createCanonicalPythonCompilerStrategy())
    .register(createTsFastPathCompilerStrategy())
    .register(createDisabledPythonWorkerCompilerStrategy());
}

export function createCanonicalPythonCompilerStrategy(): TaskCompilerStrategy {
  return {
    id: 'canonical_python',
    canCompile() {
      return true;
    },
    compile(taskSpec, options) {
      return compileTaskSpecWithPython(taskSpec, options);
    },
  };
}

export function createTsFastPathCompilerStrategy(): TaskCompilerStrategy {
  return {
    id: 'ts_fast_path',
    canCompile(taskSpec) {
      return TS_FAST_PATH_TASK_TYPES.has(taskSpec.task_type);
    },
    async compile(taskSpec, _options: TaskCompileOptions) {
      return createCompiledTaskPlan({
        taskPlan: compileTaskSpecToTaskPlan(taskSpec),
        strategyId: 'ts_fast_path',
      });
    },
  };
}

export function createDisabledPythonWorkerCompilerStrategy(): TaskCompilerStrategy {
  return {
    id: 'python_worker',
    canCompile() {
      return false;
    },
    async compile(taskSpec: TaskSpec) {
      throw new TaskSpecCompileError(
        'python_worker_not_enabled',
        'Python compiler worker strategy is registered but not enabled in P1.',
        [{
          code: 'python_worker_not_enabled',
          path: 'task_type',
          message: `TaskSpec task_type=${taskSpec.task_type} should use canonical_python or ts_fast_path for P1.`,
        }],
      );
    },
  };
}

export function isTsFastPathTaskType(taskType: string): boolean {
  return TS_FAST_PATH_TASK_TYPES.has(taskType);
}
