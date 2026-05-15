import { executeLocalTool } from '../local/local-tool-dispatcher.js';
import { localToolNames } from '../local/local-tool-names.js';
import type { ToolSource } from './tool-source.js';

export const localToolSource: ToolSource = {
  id: 'local',
  canHandle(toolName: string): boolean {
    return localToolNames.has(toolName);
  },
  getInputSchema() {
    return undefined;
  },
  execute(toolName, input, context) {
    return executeLocalTool(toolName, input, context);
  },
};
