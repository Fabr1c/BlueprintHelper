import { normalizeBridgeToolResult } from './bridge-tool-result-utils.js';
import type { BlueprintHelperToolContext } from '../types.js';
import type { ToolResultBase } from '../../result/tool-result.js';

export async function executeGenericBridgeTool(
  toolName: string,
  bridgeCommand: string,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const response = await context.bridge.sendCommand(bridgeCommand, input);
  return normalizeBridgeToolResult(toolName, response);
}
