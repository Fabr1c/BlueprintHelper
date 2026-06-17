import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import { makeTaskPlanWithSteps, omitUndefined, type TaskPlanStep } from '../compiler-helpers.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

type MaterialInstanceTaskSpec = Extract<TaskSpec, { task_type: 'edit_material_instance' }>;

type MaterialInstanceBehavior = {
  readonly material_instance_strategy: 'material_instance_edit';
  readonly operations: readonly MaterialInstanceOperation[];
};

type MaterialInstanceOperation = {
  readonly op: string;
  readonly [key: string]: unknown;
};

type MaterialInstanceTaskPlanOp = {
  readonly op: string;
  readonly [key: string]: unknown;
};

export const materialInstanceTaskCompiler: TaskTypeCompiler<MaterialInstanceTaskSpec> = {
  id: 'material_instance',
  taskType: 'edit_material_instance',
  canCompile(taskSpec): taskSpec is MaterialInstanceTaskSpec {
    return taskSpec.task_type === 'edit_material_instance';
  },
  compile(taskSpec) {
    return compileMaterialInstanceTaskSpecToTaskPlan(taskSpec);
  },
};

function compileMaterialInstanceTaskSpecToTaskPlan(taskSpec: MaterialInstanceTaskSpec): TaskPlan {
  const behavior = taskSpec.behavior as MaterialInstanceBehavior;
  return makeTaskPlanWithSteps(taskSpec, [{
    step_id: 'step_material_instance',
    capability: 'material_instance',
    target: {
      asset_path: taskSpec.target.asset_path,
      target_type: 'material_instance',
    },
    write: {
      strategy: 'material_instance_edit',
      ops: behavior.operations.map(normalizeMaterialInstanceOperation),
    },
    constraints: {
      asset_kind: 'material_instance_constant',
      parameter_scope: 'material_instance',
    },
  } as TaskPlanStep]);
}

function normalizeMaterialInstanceOperation(operation: MaterialInstanceOperation): MaterialInstanceTaskPlanOp {
  return omitUndefined({
    ...operation,
    parameter_name: typeof operation['parameter_name'] === 'string'
      ? operation['parameter_name']
      : undefined,
  }) as MaterialInstanceTaskPlanOp;
}
