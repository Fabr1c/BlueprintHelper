import { getBridgeToolDescriptor } from '../bridge/bridge-tool-descriptor.js';
import { executeBridgeTool } from '../bridge/bridge-tool-dispatcher.js';
import type { ToolSource } from './tool-source.js';

export const bridgeToolSource: ToolSource = {
  id: 'bridge',
  canHandle(toolName: string): boolean {
    return getBridgeToolDescriptor(toolName) !== undefined;
  },
  getInputSchema(toolName: string) {
    return getBridgeToolDescriptor(toolName)?.schema;
  },
  execute(toolName, input, context) {
    return executeBridgeTool(toolName, input, context);
  },
};
