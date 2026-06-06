import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import { buildTaskPlan } from '../task-plan-builder.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';
import {
  assertSupportedTaskSpec,
  defaultFieldOwnerClassForBlueprintAsset,
} from './graphwrite-logic-body-compiler.js';
import { compileGraphWriteOps } from './default-graphwrite-operation-compilers.js';
import { makeGraphWriteTaskPlanSteps } from './graphwrite-plan-step-builder.js';

export {
  assertSupportedTaskSpec,
  defaultFieldOwnerClassForBlueprintAsset,
  taskPlanToAppendBridgePayload,
} from './graphwrite-logic-body-compiler.js';
export { makeGraphWriteTaskPlanSteps } from './graphwrite-plan-step-builder.js';
export { compileGraphWriteOps } from './default-graphwrite-operation-compilers.js';

export const graphWriteTaskTypeCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'edit_blueprint_graph' }>> = {
  id: 'graphwrite',
  taskType: 'edit_blueprint_graph',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'edit_blueprint_graph' }> {
    return taskSpec.task_type === 'edit_blueprint_graph';
  },
  compile(taskSpec): TaskPlan {
    assertSupportedTaskSpec(taskSpec);
    const behavior = taskSpec.behavior as Record<string, unknown>;
    return buildTaskPlan({
      taskSpec,
      steps: makeGraphWriteTaskPlanSteps(taskSpec, compileGraphWriteOps(behavior, {
        defaultFieldOwnerClass: defaultFieldOwnerClassForBlueprintAsset(taskSpec.target.asset_path),
      })),
    });
  },
};
