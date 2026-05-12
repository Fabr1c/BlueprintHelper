import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { BridgeClient } from '@blueprinthelper/task-core/bridge/bridge-client';
import { EditorConfig, registerTools } from '../../mcp/tools/register-tools.js';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';

function makeFakeServer(): { server: Record<string, unknown>; tools: Map<string, { inputSchema: unknown; handler: Function }> } {
  const tools = new Map<string, { inputSchema: unknown; handler: Function }>();

  const server = {
    registerTool: (name: string, config: { inputSchema: unknown }, handler: Function) => {
      tools.set(name, { inputSchema: config.inputSchema, handler });
    },
    registerResource: () => {},
    registerResourceTemplate: () => {},
  };

  return { server, tools };
}

const fakeConfig: EditorConfig = {
  ueEngineDir: '/fake/engine',
};

describe('blueprint_import_json_to_graph registry surface', () => {
  it('does not register the frozen direct import tool', () => {
    const { server, tools } = makeFakeServer();
    const bridge = { sendCommand: async () => ({ success: true, request_id: '1' }) } as unknown as BridgeClient;

    registerTools(server as unknown as McpServer, bridge, fakeConfig);

    assert.equal(tools.has('blueprint_import_json_to_graph'), false);
  });
});
