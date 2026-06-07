import type { ToolResultBase } from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { getBridgeToolHandler } from './bridge-tool-handler-registry.js';

export async function executeBridgeTool(
  toolName: string,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  return getBridgeToolHandler(toolName)(input, context);
}
