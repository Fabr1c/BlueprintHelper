import type { TaskPlan, TaskSpec } from '../schema/task-schemas.js';
import { createDefaultTaskTypeCompilerRegistry } from './compilers/default-task-type-compilers.js';
export { TaskSpecCompileError } from './task-compiler-errors.js';
import { TaskSpecCompileError } from './task-compiler-errors.js';

export const TASK_COMPILER_RESULT_SCHEMA = 'BlueprintHelper.TaskCompilerResult.v1';
const defaultTaskTypeCompilerRegistry = createDefaultTaskTypeCompilerRegistry();

export type TaskCompilerStrategyId = 'canonical_ts';

export interface TaskCompileOptions {
  dryRun: boolean;
  diagnostics?: boolean;
}

export interface TaskCompileDiagnostics {
  bridgePayload?: Record<string, unknown>;
  taskPlanSummary?: Record<string, unknown>;
  compilerOutputBytes?: number;
  [key: string]: unknown;
}

export interface CompiledTaskPlan {
  schema: typeof TASK_COMPILER_RESULT_SCHEMA;
  taskPlan: TaskPlan;
  strategyId: TaskCompilerStrategyId;
  diagnostics?: TaskCompileDiagnostics;
}

export interface TaskCompilerStrategy {
  readonly id: TaskCompilerStrategyId;
  canCompile(taskSpec: TaskSpec, options?: TaskCompileOptions): boolean;
  compile(taskSpec: TaskSpec, options: TaskCompileOptions): Promise<CompiledTaskPlan>;
}

export function createCompiledTaskPlan(input: {
  taskPlan: TaskPlan;
  strategyId: TaskCompilerStrategyId;
  diagnostics?: TaskCompileDiagnostics;
}): CompiledTaskPlan {
  return {
    schema: TASK_COMPILER_RESULT_SCHEMA,
    taskPlan: input.taskPlan,
    strategyId: input.strategyId,
    ...(input.diagnostics ? { diagnostics: input.diagnostics } : {}),
  };
}

export function compileTaskSpecToTaskPlan(taskSpec: TaskSpec): TaskPlan {
  const compiler = defaultTaskTypeCompilerRegistry.get(taskSpec.task_type);
  if (!compiler?.canCompile(taskSpec)) {
    throw new TaskSpecCompileError('unsupported_task_type', `Unsupported TaskSpec task_type: ${taskSpec.task_type}`, [
      {
        code: 'unsupported_task_type',
        path: 'task_type',
        message: 'Register a TaskTypeCompiler before compiling this task_type.',
      },
    ]);
  }
  return compiler.compile(taskSpec as never, { source: 'facade' });
}
