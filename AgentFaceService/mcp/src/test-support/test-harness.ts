import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import type { BridgeClient, BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import { registerResources } from '../mcp/resources/resources.js';
import { registerTools, type EditorConfig } from '../mcp/tools/register-tools.js';

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
  annotations?: Record<string, unknown>;
  description?: string;
  inputSchema: {
    parse(input: unknown): Record<string, unknown>;
  };
  meta?: Record<string, unknown>;
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
  config: Partial<EditorConfig> = {},
  bridgeExtras: Partial<BridgeClient> = {},
): Map<string, RegisteredTool> {
  const tools = new Map<string, RegisteredTool>();
  const server = {
    registerTool(
      name: string,
      config: {
        annotations?: Record<string, unknown>;
        description?: string;
        inputSchema: RegisteredTool['inputSchema'];
        _meta?: Record<string, unknown>;
      },
      handler: ToolHandler,
    ) {
      tools.set(name, {
        annotations: config.annotations,
        description: config.description,
        inputSchema: config.inputSchema,
        meta: config._meta,
        handler,
      });
    },
  } as unknown as McpServer;

  const bridge = { sendCommand, ...bridgeExtras } as unknown as BridgeClient;
  registerTools(server, bridge, { ueEngineDir: '', ...config });
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
  registerTools(server, bridge, { ueEngineDir: '' });

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
