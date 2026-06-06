import type { TaskPlan } from '../schema/task-schemas.js';
import { isRecord } from './compiler-helpers.js';

export function summarizeTaskPlan(taskPlan: TaskPlan) {
  return {
    schema: taskPlan.schema,
    task_name: taskPlan.task_name,
    task_type: taskPlan.task_type,
    target_assets: taskPlan.target_assets,
    steps: taskPlan.steps.map((step) => summarizeTaskPlanStep(step)),
  };
}

function summarizeTaskPlanStep(step: TaskPlan['steps'][number]) {
  if ('capability' in step) {
    return {
      step_id: step.step_id,
      capability: step.capability,
      target: step.target,
      strategy: step.write.strategy,
      ops: step.write.ops.length,
      ...('depends_on' in step && Array.isArray(step.depends_on) ? { depends_on: step.depends_on } : {}),
    };
  }

  const args = step.args as Record<string, unknown>;
  const replacement = isRecord(args['replacement']) ? args['replacement'] : undefined;
  const nodes = Array.isArray(args['nodes'])
    ? args['nodes'].length
    : Array.isArray(replacement?.['nodes'])
      ? replacement['nodes'].length
      : undefined;
  const links = Array.isArray(args['links'])
    ? args['links'].length
    : Array.isArray(replacement?.['links'])
      ? replacement['links'].length
      : undefined;

  return {
    step_id: step.step_id,
    operation: step.operation,
    target: step.target,
    ...(nodes !== undefined ? { nodes } : {}),
    ...(links !== undefined ? { links } : {}),
  };
}
