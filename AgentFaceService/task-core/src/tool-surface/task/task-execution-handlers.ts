import {
  ExecuteTaskInputSchema,
  GetTaskResultInputSchema,
  PreviewTaskInputSchema,
  TaskSpecSchema,
} from '../../task/schema/task-schemas.js';
import {
  failureResult,
  type ToolResultBase,
  type ToolResultError,
} from '../../result/tool-result.js';
import {
  attachTaskTiming,
  measureTaskTiming,
  startTaskTiming,
} from '../../task/service/task-timing.js';
import type { BlueprintHelperToolContext } from '../types.js';

export async function previewTask(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  const timing = context.timing ?? startTaskTiming(input['develop'] === true, 'preview_task');
  const parsed = measureTaskTiming(timing, 'taskspec_parse', () => (
    'task_spec' in input
      ? PreviewTaskInputSchema.parse(input)
      : { task_spec: TaskSpecSchema.parse(input) }
  ));
  const taskSpec = parsed.task_spec;
  return (await context.taskRunner.previewTask(taskSpec, timing)).toolResult;
}

export async function executeTask(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  const timing = context.timing ?? startTaskTiming(input['develop'] === true, 'execute_task');
  if (!('task_spec' in input) && 'preview_token' in input) {
    return attachTaskTiming(makeExecuteInputFailure(
      'preview_token_requires_task_spec_wrapper',
      'execute_task preview_token is only accepted on the wrapped input shape: { task_spec, preview_token }.',
      'preview_token',
    ), timing);
  }
  const parsed = measureTaskTiming(timing, 'taskspec_parse', () => (
    'task_spec' in input
      ? ExecuteTaskInputSchema.parse(input)
      : { task_spec: TaskSpecSchema.parse(input) }
  ));
  return await context.taskRunner.executeTask(parsed.task_spec, timing, { previewToken: parsed.preview_token });
}

export async function getTaskResult(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  const parsed = GetTaskResultInputSchema.parse(input);
  return await context.taskRunner.getTaskResult(parsed.task_run_id);
}

function makeExecuteInputFailure(code: string, message: string, field: string): ToolResultBase {
  return failureResult('execute_task', {
    code,
    category: 'semantic_error',
    stage: 'parse_input',
    message,
    retryable: true,
    rollback_result: 'not_needed',
    agent_action: 'fix_taskspec_and_retry',
    field,
    issues: [{
      code,
      path: field,
      message,
    }],
  } as ToolResultError);
}
