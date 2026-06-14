import {
  TaskSpecCompileError,
  compileTaskSpecToTaskPlan,
  createCompiledTaskPlan,
  type TaskCompileOptions,
  type TaskCompilerStrategy,
  type TaskCompilerStrategyId,
} from './task-compiler.js';

const CANONICAL_TS_TASK_TYPES = new Set([
  'create_asset',
  'create_blueprint_feature',
  'edit_blueprint_graph',
  'edit_material_graph',
  'edit_blueprint_variables',
  'edit_object_properties',
  'edit_blueprint_signature',
  'edit_blueprint_class_settings',
  'edit_blueprint_components',
  'edit_umg_widget',
  'edit_data_table',
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
    .register(createCanonicalTsCompilerStrategy());
}

export function createCanonicalTsCompilerStrategy(): TaskCompilerStrategy {
  return {
    id: 'canonical_ts',
    canCompile(taskSpec) {
      return CANONICAL_TS_TASK_TYPES.has(taskSpec.task_type);
    },
    async compile(taskSpec, _options: TaskCompileOptions) {
      return createCompiledTaskPlan({
        taskPlan: compileTaskSpecToTaskPlan(taskSpec),
        strategyId: 'canonical_ts',
      });
    },
  };
}

export function isCanonicalTsTaskType(taskType: string): boolean {
  return CANONICAL_TS_TASK_TYPES.has(taskType);
}
