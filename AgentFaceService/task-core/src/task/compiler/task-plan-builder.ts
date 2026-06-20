import type { TaskPlan, TaskSpec } from '../schema/task-schemas.js';
import { TASK_PLAN_SCHEMA } from '../schema/task-schemas.js';
import {
  makeTaskPlanExecutionPolicy,
  type TaskPlanExecutionPolicyOverrides,
} from './compiler-helpers.js';

export interface BuildTaskPlanInput {
  readonly taskSpec: TaskSpec;
  readonly steps: TaskPlan['steps'];
  readonly executionPolicy?: TaskPlanExecutionPolicyOverrides;
}

export function buildTaskPlan(input: BuildTaskPlanInput): TaskPlan {
  return {
    schema: TASK_PLAN_SCHEMA,
    task_name: input.taskSpec.feature_name,
    task_type: input.taskSpec.task_type,
    context_id: input.taskSpec.context_id,
    target_assets: [input.taskSpec.target.asset_path],
    ...(input.taskSpec.verification ? { verification: input.taskSpec.verification } : {}),
    execution_policy: makeTaskPlanExecutionPolicy(input.executionPolicy),
    steps: renumberTaskPlanSteps(input.steps),
  };
}

export function renumberTaskPlanSteps<TStep extends TaskPlan['steps'][number]>(steps: readonly TStep[]): TStep[] {
  const oldIds = steps.map((step) => step.step_id);
  const newIds = steps.map((_, index) => `step_${String(index + 1).padStart(3, '0')}`);

  return steps.map((step, index) => ({
    ...step,
    step_id: newIds[index],
    ...('depends_on' in step && Array.isArray(step.depends_on)
      ? {
          depends_on: step.depends_on.map((id) => {
            for (let candidate = index - 1; candidate >= 0; candidate -= 1) {
              if (oldIds[candidate] === id) return newIds[candidate];
            }
            return id;
          }),
        }
      : {}),
  }));
}
