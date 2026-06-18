import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import { TASK_PLAN_SCHEMA } from '../../schema/task-schemas.js';
import {
  getRequiredString,
  makeTaskPlanExecutionPolicy,
  omitUndefined,
  optionalString,
} from '../compiler-helpers.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

export const assetFactoryTaskCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'create_asset' }>> = {
  id: 'asset_factory',
  taskType: 'create_asset',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'create_asset' }> {
    return taskSpec.task_type === 'create_asset';
  },
  compile(taskSpec) {
    return compileAssetFactoryTaskSpecToTaskPlan(taskSpec);
  },
};

function compileAssetFactoryTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'create_asset' }>,
): TaskPlan {
  const asset = taskSpec.behavior.asset as Record<string, unknown>;
  const op = omitUndefined({
    op: 'create_asset',
    asset_type: getRequiredString(asset, 'asset_type', 'behavior.asset.asset_type'),
    parent_class: optionalString(asset, 'parent_class'),
    value_type: optionalString(asset, 'value_type'),
    fields: Array.isArray(asset['fields']) ? asset['fields'] : undefined,
    row_struct: optionalString(asset, 'row_struct'),
    data_asset_class: optionalString(asset, 'data_asset_class'),
    collision: optionalString(asset, 'collision') ?? optionalString(asset, 'collision_policy'),
  }) as { op: string; [key: string]: unknown };

  return {
    schema: TASK_PLAN_SCHEMA,
    task_name: taskSpec.feature_name,
    task_type: taskSpec.task_type,
    context_id: taskSpec.context_id,
    target_assets: [taskSpec.target.asset_path],
    execution_policy: makeTaskPlanExecutionPolicy(),
    steps: [{
      step_id: 'step_001',
      capability: 'asset_factory',
      target: {
        asset_path: taskSpec.target.asset_path,
      },
      write: {
        strategy: 'asset_create',
        ops: [op],
      },
    }],
  };
}
