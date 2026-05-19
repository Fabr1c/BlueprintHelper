import type { TaskSpec } from '../schema/task-schemas.js';
import {
  TaskSpecCompileError,
  type CompiledTaskPlan,
  type TaskCompileOptions,
} from './task-compiler.js';
import {
  TaskCompilerRegistry,
  createDefaultTaskCompilerRegistry,
} from './task-compiler-registry.js';
import {
  createTaskCompilerPolicy,
  type TaskCompilerPolicy,
  type TaskCompilerPolicyOptions,
} from './task-compiler-policy.js';

export type TaskCompiler = (taskSpec: TaskSpec, options: TaskCompileOptions) => Promise<CompiledTaskPlan>;

export interface TaskSpecCompilerOptions extends TaskCompilerPolicyOptions {
  registry?: TaskCompilerRegistry;
  policy?: TaskCompilerPolicy;
}

export function createTaskSpecCompiler(options: TaskSpecCompilerOptions = {}): TaskCompiler {
  const registry = options.registry ?? createDefaultTaskCompilerRegistry();
  const policy = options.policy ?? createTaskCompilerPolicy(options);

  return async (taskSpec, compileOptions) => {
    const selection = policy.select(taskSpec, registry);
    const strategy = registry.require(selection.strategyId);
    try {
      const compiled = await strategy.compile(taskSpec, compileOptions);
      return {
        ...compiled,
        strategyId: selection.strategyId,
        diagnostics: {
          ...(compiled.diagnostics ?? {}),
          ...(selection.parityStatus ? { parityStatus: selection.parityStatus } : {}),
          ...(selection.parityReason ? { parityReason: selection.parityReason } : {}),
          ...(selection.fallbackReason ? { fallbackReason: selection.fallbackReason } : {}),
        },
      };
    } catch (err) {
      if (err instanceof TaskSpecCompileError) {
        throw err;
      }
      const message = err instanceof Error ? err.message : String(err);
      throw new TaskSpecCompileError(
        'task_compiler_strategy_failed',
        `Task compiler strategy ${selection.strategyId} failed: ${message}`,
        [{
          code: 'task_compiler_strategy_failed',
          path: 'task_type',
          message,
        }],
      );
    }
  };
}
