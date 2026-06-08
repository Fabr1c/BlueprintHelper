import { BRIDGE_TOOL_DESCRIPTORS } from './bridge-tool-descriptor.js';

export const bridgeCommandByToolName: Record<string, string> = Object.fromEntries(
  BRIDGE_TOOL_DESCRIPTORS
    .filter((descriptor) => descriptor.bridge_command !== undefined)
    .map((descriptor) => [descriptor.tool_name, descriptor.bridge_command as string]),
);
