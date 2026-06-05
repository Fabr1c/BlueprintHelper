import {
  ExecuteTaskInputSchema,
  GetTaskResultInputSchema,
  PreviewTaskInputSchema,
} from '../../task/schema/task-schemas.js';
import {
  measureTaskTiming,
  startTaskTiming,
} from '../../task/service/task-timing.js';
import type { BlueprintHelperToolContext } from '../types.js';

export async function previewTask(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  const timing = context.timing ?? startTaskTiming(input['develop'] === true, 'preview_task');
  const parsed = measureTaskTiming(timing, 'taskspec_parse', () => PreviewTaskInputSchema.parse(input));
  return (await context.taskRunner.previewTask(parsed.task_spec, timing)).toolResult;
}

export async function executeTask(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  const timing = context.timing ?? startTaskTiming(input['develop'] === true, 'execute_task');
  const parsed = measureTaskTiming(timing, 'taskspec_parse', () => ExecuteTaskInputSchema.parse(input));
  return await context.taskRunner.executeTask(parsed.task_spec, timing, { previewToken: parsed.preview_token });
}

export async function getTaskResult(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  const parsed = GetTaskResultInputSchema.parse(input);
  return await context.taskRunner.getTaskResult(parsed.task_run_id);
}
