import { executeTaskTool } from '../task/task-tool-dispatcher.js';
import { taskToolSchemas, type TaskToolName } from '../task/task-tool-schemas.js';
import type { ToolSource } from './tool-source.js';

export const taskToolSource: ToolSource = {
  id: 'task',
  canHandle(toolName: string): boolean {
    return toolName in taskToolSchemas;
  },
  getInputSchema(toolName: string) {
    return taskToolSchemas[toolName as TaskToolName];
  },
  execute(toolName, input, context) {
    return executeTaskTool(toolName as TaskToolName, input, context);
  },
};
