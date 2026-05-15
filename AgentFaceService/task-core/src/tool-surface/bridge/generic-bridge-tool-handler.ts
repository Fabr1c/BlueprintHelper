import { normalizeBridgeToolResult } from './bridge-tool-result-utils.js';
import type { BlueprintHelperToolContext } from '../types.js';
import type { ToolResultBase } from '../../result/tool-result.js';

export async function executeGenericBridgeTool(
  toolName: string,
  bridgeCommand: string,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const payload = normalizeBridgePayload(toolName, input);
  const response = await context.bridge.sendCommand(bridgeCommand, payload);
  return normalizeBridgeToolResult(toolName, response);
}

export function normalizeBridgePayload(toolName: string, input: Record<string, unknown>): Record<string, unknown> {
  if (toolName === 'blueprint_get_logic') {
    return { format: 'logic_md', ...input };
  }
  if (toolName === 'blueprint_get_logic_json') {
    return { format: 'logic_json', ...input };
  }
  return input;
}
