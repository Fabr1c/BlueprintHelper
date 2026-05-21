import type { BlueprintHelperToolContext } from '../types.js';
import { executeTask, getTaskResult, previewTask } from './task-execution-handlers.js';
import { readReferenceContext } from './task-context-handlers.js';
import type { TaskToolName } from './task-tool-schemas.js';

export async function executeTaskTool(
  name: TaskToolName,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
) {
  switch (name) {
    case 'blueprinthelper_read_reference_context':
      return readReferenceContext(input, context);
    case 'blueprinthelper_preview_task':
      return previewTask(input, context);
    case 'blueprinthelper_execute_task':
      return executeTask(input, context);
    case 'blueprinthelper_get_task_result':
      return getTaskResult(input, context);
    default:
      throw new Error(`Unsupported task tool: ${name}`);
  }
}
