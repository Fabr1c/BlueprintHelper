import { bridgeCommandByToolName } from '../bridge/bridge-tool-command-map.js';
import { executeBridgeTool } from '../bridge/bridge-tool-dispatcher.js';
import { bridgeToolSchemas } from '../bridge/bridge-tool-schemas.js';
import type { ToolSource } from './tool-source.js';

export const bridgeToolSource: ToolSource = {
  id: 'bridge',
  canHandle(toolName: string): boolean {
    return toolName in bridgeCommandByToolName || bridgeToolSchemas[toolName] !== undefined;
  },
  getInputSchema(toolName: string) {
    return bridgeToolSchemas[toolName];
  },
  execute(toolName, input, context) {
    return executeBridgeTool(toolName, input, context);
  },
};
