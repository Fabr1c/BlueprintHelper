import { failureResult, type ToolResultBase } from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { getBridgeToolDescriptor } from './bridge-tool-descriptor.js';
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

const BRIDGE_TOOL_HANDLERS_BY_ID = new Map<string, BridgeToolHandler>();

export function registerBridgeToolHandler(handlerId: string, handler: BridgeToolHandler): void {
  if (BRIDGE_TOOL_HANDLERS_BY_ID.has(handlerId)) {
    throw new Error(`Duplicate BlueprintHelper Bridge tool handler: ${handlerId}`);
  }
  BRIDGE_TOOL_HANDLERS_BY_ID.set(handlerId, handler);
}

export function getBridgeToolHandler(toolName: string): BridgeToolHandler {
  const descriptor = getBridgeToolDescriptor(toolName);
  const handler = descriptor ? BRIDGE_TOOL_HANDLERS_BY_ID.get(descriptor.handler_id) : undefined;
  if (handler) {
    return handler;
  }
  if (descriptor?.bridge_command) {
    return (input, context) => executeGenericBridgeTool(toolName, descriptor.bridge_command!, input, context);
  }

  return async () => failureResult(toolName, {
    code: 'bridge_tool_not_mapped',
    stage: 'parse_input',
        message: `No Bridge tool descriptor for ${toolName}.`,
    retryable: false,
    rollback_result: 'not_needed',
  });
}

registerBridgeToolHandler('read_context', (input, context) =>
  executeReadContext(input as ReadContextInput, context));
registerBridgeToolHandler('read_context_capabilities', (input) =>
  executeReadContextCapabilities(input));
registerBridgeToolHandler('write_session', (input, context) =>
  executeWriteSessionRequest(input, context));
registerBridgeToolHandler('capture_screenshot', (input, context) =>
  executeCaptureScreenshot(input, context));
