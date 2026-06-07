import { failureResult, type ToolResultBase } from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { bridgeCommandByToolName } from './bridge-tool-command-map.js';
import { executeGenericBridgeTool } from './generic-bridge-tool-handler.js';
import { executeReadContextCapabilities } from './read-context/read-context-capabilities.js';
import { executeReadContext } from './read-context/read-context-handler.js';
import type { ReadContextInput } from './read-context/read-context-schemas.js';
import { executeCaptureScreenshot } from './screenshot/capture-screenshot-handler.js';
import { executeWriteSessionRequest } from './write-session-handler.js';

export type BridgeToolHandler = (
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
) => ToolResultBase | Promise<ToolResultBase>;

const BRIDGE_TOOL_HANDLERS = new Map<string, BridgeToolHandler>();

export function registerBridgeToolHandler(toolName: string, handler: BridgeToolHandler): void {
  if (BRIDGE_TOOL_HANDLERS.has(toolName)) {
    throw new Error(`Duplicate BlueprintHelper Bridge tool handler: ${toolName}`);
  }
  BRIDGE_TOOL_HANDLERS.set(toolName, handler);
}

export function getBridgeToolHandler(toolName: string): BridgeToolHandler {
  const handler = BRIDGE_TOOL_HANDLERS.get(toolName);
  if (handler) {
    return handler;
  }

  const bridgeCommand = bridgeCommandByToolName[toolName];
  if (bridgeCommand) {
    return (input, context) => executeGenericBridgeTool(toolName, bridgeCommand, input, context);
  }

  return async () => failureResult(toolName, {
    code: 'bridge_tool_not_mapped',
    stage: 'parse_input',
    message: `No Bridge command mapping for ${toolName}.`,
    retryable: false,
    rollback_result: 'not_needed',
  });
}

registerBridgeToolHandler('blueprinthelper_read_context', (input, context) =>
  executeReadContext(input as ReadContextInput, context));
registerBridgeToolHandler('blueprinthelper_read_context_capabilities', (input) =>
  executeReadContextCapabilities(input));
registerBridgeToolHandler('blueprinthelper_request_write_session', (input, context) =>
  executeWriteSessionRequest(input, context));
registerBridgeToolHandler('blueprinthelper_capture_screenshot', (input, context) =>
  executeCaptureScreenshot(input, context));
