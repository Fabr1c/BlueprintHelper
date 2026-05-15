import { failureResult, type ToolResultBase } from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { bridgeCommandByToolName } from './bridge-tool-command-map.js';
import { executeGenericBridgeTool } from './generic-bridge-tool-handler.js';
import { executeReadContext } from './read-context/read-context-handler.js';
import { executeWriteSessionRequest } from './write-session-handler.js';

export async function executeBridgeTool(
  toolName: string,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  if (toolName === 'blueprinthelper_read_context') {
    return executeReadContext(input, context);
  }
  if (toolName === 'blueprinthelper_request_write_session') {
    return executeWriteSessionRequest(input, context);
  }

  const bridgeCommand = bridgeCommandByToolName[toolName];
  if (!bridgeCommand) {
    return failureResult(toolName, {
      code: 'bridge_tool_not_mapped',
      stage: 'parse_input',
      message: `No Bridge command mapping for ${toolName}.`,
      retryable: false,
      rollback_result: 'not_needed',
    });
  }

  return executeGenericBridgeTool(toolName, bridgeCommand, input, context);
}
