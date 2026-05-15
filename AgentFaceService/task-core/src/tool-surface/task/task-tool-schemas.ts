import { z } from 'zod';
import {
  ExecuteTaskInputSchema,
  GetTaskResultInputSchema,
  PreviewTaskInputSchema,
  ReadTaskContextInputSchema,
  TaskSpecSchema,
} from '../../task/schema/task-schemas.js';
import { ReadReferenceContextInputSchema } from './read-reference-context-schema.js';

export const taskToolSchemas = {
  blueprinthelper_read_task_context: ReadTaskContextInputSchema,
  blueprinthelper_read_reference_context: ReadReferenceContextInputSchema,
  blueprinthelper_preview_task: z.union([PreviewTaskInputSchema, TaskSpecSchema]),
  blueprinthelper_execute_task: z.union([ExecuteTaskInputSchema, TaskSpecSchema]),
  blueprinthelper_get_task_result: GetTaskResultInputSchema,
} as const;

export type TaskToolName = keyof typeof taskToolSchemas;
