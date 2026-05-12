import { z } from 'zod';
import {
  ExecuteTaskInputSchema,
  GetTaskResultInputSchema,
  PreviewTaskInputSchema,
  ReadTaskContextInputSchema,
  TaskSpecSchema,
} from '../task/schema/task-schemas.js';
import type { BlueprintHelperToolContext } from './types.js';

export const ReadReferenceContextInputSchema = z.object({
  asset_path: z.string(),
  target_type: z.enum([
    'asset',
    'blueprint',
    'graph',
    'function',
    'event',
    'custom_event',
    'member_variable',
    'block',
    'widget',
    'data_table_row',
    'interface',
  ]).optional().default('asset'),
  target_name: z.string().optional(),
  graph_name: z.string().optional(),
  block_id: z.string().optional(),
  widget_name: z.string().optional(),
  row_name: z.string().optional(),
  interface_path: z.string().optional(),
  scope: z.enum([
    'safety_context',
    'dependencies',
    'referencers',
    'external_dependents',
    'all',
  ]).optional().default('safety_context'),
  max_results: z.number().int().positive().max(500).optional().default(50),
  include_samples: z.boolean().optional().default(true),
});

export const taskToolSchemas = {
  blueprinthelper_read_task_context: ReadTaskContextInputSchema,
  blueprinthelper_read_reference_context: ReadReferenceContextInputSchema,
  blueprinthelper_preview_task: z.union([PreviewTaskInputSchema, TaskSpecSchema]),
  blueprinthelper_execute_task: z.union([ExecuteTaskInputSchema, TaskSpecSchema]),
  blueprinthelper_get_task_result: GetTaskResultInputSchema,
};

export async function executeTaskTool(
  name: keyof typeof taskToolSchemas,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
) {
  if (name === 'blueprinthelper_read_task_context') {
    return await context.taskRunner.readTaskContext(ReadTaskContextInputSchema.parse(input));
  }
  if (name === 'blueprinthelper_read_reference_context') {
    return await context.taskRunner.readReferenceContext(ReadReferenceContextInputSchema.parse(input));
  }
  if (name === 'blueprinthelper_preview_task') {
    const taskSpec = 'task_spec' in input
      ? PreviewTaskInputSchema.parse(input).task_spec
      : TaskSpecSchema.parse(input);
    return (await context.taskRunner.previewTask(taskSpec)).toolResult;
  }
  if (name === 'blueprinthelper_execute_task') {
    const taskSpec = 'task_spec' in input
      ? ExecuteTaskInputSchema.parse(input).task_spec
      : TaskSpecSchema.parse(input);
    return await context.taskRunner.executeTask(taskSpec);
  }
  if (name === 'blueprinthelper_get_task_result') {
    const parsed = GetTaskResultInputSchema.parse(input);
    return await context.taskRunner.getTaskResult(parsed.task_run_id);
  }
  throw new Error(`Unsupported task tool: ${name}`);
}
