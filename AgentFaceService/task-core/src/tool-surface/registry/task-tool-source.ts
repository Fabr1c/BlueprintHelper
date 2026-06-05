import { executeTaskTool, getDefaultTaskToolHandlerRegistry } from '../task/task-tool-dispatcher.js';
import type { TaskToolName } from '../task/task-tool-handler-registry.js';
import type { ToolSource } from './tool-source.js';

const registry = getDefaultTaskToolHandlerRegistry();

export const taskToolSource: ToolSource = {
  id: 'task',
  canHandle(toolName: string): boolean {
    return registry.has(toolName);
  },
  getInputSchema(toolName: string) {
    return registry.require(toolName).inputSchema;
  },
  execute(toolName, input, context) {
    return executeTaskTool(toolName as TaskToolName, input, context);
  },
};
