import type { z } from 'zod';

import {
  ExecuteTaskInputSchema,
  GetTaskResultInputSchema,
  PreviewTaskInputSchema,
  TaskSpecSchema,
} from '../../task/schema/task-schemas.js';
import {
  addTaskTimingMarker,
  attachTaskTiming,
  startTaskTiming,
} from '../../task/service/task-timing.js';
import {
  failureResult,
  type ToolResultBase,
  type ToolResultError,
} from '../../result/tool-result.js';
import {
  InputShapeAdapterError,
  adaptToolInput,
  type InputShapeId,
} from '../input/input-shape-adapter.js';
import { createReadSpecInputShapeAdapterRegistry } from '../input/readspec-input-adapters.js';
import { createTaskSpecInputShapeAdapterRegistry } from '../input/taskspec-input-adapters.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { readReferenceContext } from './task-context-handlers.js';
import { executeTask, getTaskResult, previewTask } from './task-execution-handlers.js';
import { ReadReferenceContextInputSchema } from './read-reference-context-schema.js';

export const taskToolNames = [
  'blueprinthelper_read_reference_context',
  'blueprinthelper_preview_task',
  'blueprinthelper_execute_task',
  'blueprinthelper_get_task_result',
] as const;

export type TaskToolName = typeof taskToolNames[number];

export interface TaskToolHandlerDescriptor {
  readonly toolName: TaskToolName;
  readonly inputShapeIds: readonly InputShapeId[];
  readonly inputSchema: z.ZodTypeAny;
  execute(input: Record<string, unknown>, context: BlueprintHelperToolContext): Promise<ToolResultBase>;
}

export class TaskToolHandlerRegistry {
  private readonly handlers = new Map<TaskToolName, TaskToolHandlerDescriptor>();

  register(descriptor: TaskToolHandlerDescriptor): this {
    if (this.handlers.has(descriptor.toolName)) {
      throw new Error(`Task tool handler is already registered: ${descriptor.toolName}`);
    }
    this.handlers.set(descriptor.toolName, descriptor);
    return this;
  }

  has(toolName: string): toolName is TaskToolName {
    return this.handlers.has(toolName as TaskToolName);
  }

  get(toolName: string): TaskToolHandlerDescriptor | undefined {
    return this.handlers.get(toolName as TaskToolName);
  }

  require(toolName: string): TaskToolHandlerDescriptor {
    const descriptor = this.get(toolName);
    if (!descriptor) {
      throw new Error(`Unsupported task tool: ${toolName}`);
    }
    return descriptor;
  }

  list(): readonly TaskToolHandlerDescriptor[] {
    return Array.from(this.handlers.values());
  }
}

const taskSpecInputShapeAdapters = createTaskSpecInputShapeAdapterRegistry();
const readSpecInputShapeAdapters = createReadSpecInputShapeAdapterRegistry();

function adaptTaskSpecInput(
  inputShapeIds: readonly InputShapeId[],
  input: Record<string, unknown>,
): Record<string, unknown> {
  return adaptToolInput(taskSpecInputShapeAdapters, inputShapeIds, input);
}

export function createDefaultTaskToolHandlerRegistry(): TaskToolHandlerRegistry {
  const previewInputShapeIds = ['wrapped_taskspec_preview', 'bare_taskspec'] as const;
  const executeInputShapeIds = ['wrapped_taskspec_execute', 'bare_taskspec'] as const;
  const readReferenceInputShapeIds = ['read_reference_context'] as const;

  return new TaskToolHandlerRegistry()
    .register({
      toolName: 'blueprinthelper_read_reference_context',
      inputShapeIds: readReferenceInputShapeIds,
      inputSchema: ReadReferenceContextInputSchema,
      execute(input, context) {
        return readReferenceContext(
          adaptToolInput(readSpecInputShapeAdapters, readReferenceInputShapeIds, input),
          context,
        );
      },
    })
    .register({
      toolName: 'blueprinthelper_preview_task',
      inputShapeIds: previewInputShapeIds,
      inputSchema: PreviewTaskInputSchema.or(TaskSpecSchema),
      execute(input, context) {
        return previewTask(adaptTaskSpecInput(previewInputShapeIds, input), context);
      },
    })
    .register({
      toolName: 'blueprinthelper_execute_task',
      inputShapeIds: executeInputShapeIds,
      inputSchema: ExecuteTaskInputSchema.or(TaskSpecSchema),
      execute(input, context) {
        try {
          return executeTask(adaptTaskSpecInput(executeInputShapeIds, input), context);
        } catch (error) {
          if (error instanceof InputShapeAdapterError) {
            const timing = context.timing ?? startTaskTiming(input['develop'] === true, 'execute_task');
            addTaskTimingMarker(timing, 'input_shape_adapt.failed', {
              code: error.code,
              ...(error.field ? { field: error.field } : {}),
            });
            return Promise.resolve(attachTaskTiming(makeInputShapeFailure('execute_task', error), timing));
          }
          throw error;
        }
      },
    })
    .register({
      toolName: 'blueprinthelper_get_task_result',
      inputShapeIds: ['empty_object'],
      inputSchema: GetTaskResultInputSchema,
      execute: getTaskResult,
    });
}

function makeInputShapeFailure(operation: string, error: InputShapeAdapterError): ToolResultBase {
  return failureResult(operation, {
    code: error.code,
    category: 'semantic_error',
    stage: 'parse_input',
    message: error.message,
    retryable: true,
    rollback_result: 'not_needed',
    agent_action: 'fix_taskspec_and_retry',
    field: error.field,
    issues: [{
      code: error.code,
      path: error.field ?? 'input',
      message: error.message,
    }],
  } as ToolResultError);
}
