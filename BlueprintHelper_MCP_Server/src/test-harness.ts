import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import type { BridgeClient, BridgeResponse } from './bridge-client.js';
import { registerResources } from './resources.js';
import { registerTools } from './tools.js';

export type ToolHandler = (args: Record<string, unknown>) => Promise<{
  content: Array<{
    type: string;
    text?: string;
    uri?: string;
    name?: string;
    description?: string;
    mimeType?: string;
  }>;
  isError?: boolean;
  structuredContent?: Record<string, unknown>;
}>;

export interface RegisteredTool {
  inputSchema: {
    parse(input: unknown): Record<string, unknown>;
  };
  handler: ToolHandler;
}

export type ResourceHandler = (uri: URL) => Promise<{
  contents: Array<{ uri: string; mimeType: string; text: string }>;
}>;

export interface RegisteredResource {
  uriOrTemplate: unknown;
  handler: ResourceHandler;
}

export function registerWithBridge(
  sendCommand: (command: string, payload?: Record<string, unknown>) => Promise<BridgeResponse>,
): Map<string, RegisteredTool> {
  const tools = new Map<string, RegisteredTool>();
  const server = {
    registerTool(name: string, config: { inputSchema: RegisteredTool['inputSchema'] }, handler: ToolHandler) {
      tools.set(name, { inputSchema: config.inputSchema, handler });
    },
  } as unknown as McpServer;

  const bridge = { sendCommand } as unknown as BridgeClient;
  registerTools(server, bridge, { ueEngineDir: '', ueProjectFile: '' });
  return tools;
}

export function registerResourcesWithBridge(
  sendCommand: (command: string, payload?: Record<string, unknown>) => Promise<BridgeResponse>,
): Map<string, RegisteredResource> {
  const resources = new Map<string, RegisteredResource>();
  const server = {
    registerResource(
      name: string,
      uriOrTemplate: unknown,
      _config: Record<string, unknown>,
      handler: ResourceHandler,
    ) {
      resources.set(name, { uriOrTemplate, handler });
    },
  } as unknown as McpServer;

  const bridge = { sendCommand } as unknown as BridgeClient;
  registerResources(server, bridge);
  return resources;
}

export async function invokeTool(tool: RegisteredTool, args: Record<string, unknown>) {
  return tool.handler(tool.inputSchema.parse(args));
}

export async function withConnectedMcpServer(
  sendCommand: (command: string, payload?: Record<string, unknown>) => Promise<BridgeResponse>,
  run: (client: Client) => Promise<void>,
): Promise<void> {
  const server = new McpServer({ name: 'blueprint-helper-test-server', version: '0.0.0' });
  const bridge = { sendCommand } as unknown as BridgeClient;
  registerTools(server, bridge, { ueEngineDir: '', ueProjectFile: '' });

  const client = new Client({ name: 'blueprint-helper-test-client', version: '0.0.0' });
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);
  await client.connect(clientTransport);

  try {
    await run(client);
  } finally {
    await client.close();
    await server.close();
  }
}
