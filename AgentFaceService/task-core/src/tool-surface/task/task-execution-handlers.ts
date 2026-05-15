import {
  ExecuteTaskInputSchema,
  GetTaskResultInputSchema,
  PreviewTaskInputSchema,
  TaskSpecSchema,
} from '../../task/schema/task-schemas.js';
import type { BlueprintHelperToolContext } from '../types.js';

export async function previewTask(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  const taskSpec = 'task_spec' in input
    ? PreviewTaskInputSchema.parse(input).task_spec
    : TaskSpecSchema.parse(input);
  return (await context.taskRunner.previewTask(taskSpec)).toolResult;
}

export async function executeTask(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  const taskSpec = 'task_spec' in input
    ? ExecuteTaskInputSchema.parse(input).task_spec
    : TaskSpecSchema.parse(input);
  return await context.taskRunner.executeTask(taskSpec);
}

export async function getTaskResult(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  const parsed = GetTaskResultInputSchema.parse(input);
  return await context.taskRunner.getTaskResult(parsed.task_run_id);
}
