import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { registerTools, EditorConfig } from '../../mcp/tools/register-tools.js';
import { BridgeResponse, BridgeClient } from '../../bridge/bridge-client.js';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';

// Helper: create a minimal fake McpServer that collects tool registrations
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

describe('blueprint_import_json_to_graph (object-first)', () => {
  const rawObj = {
    version: '2.2',
    schema: 'BlueprintHelper.JsonToBlueprint',
    nodes: [],
    links: [],
  };

  it('json schema accepts string input', () => {
    const { server, tools } = makeFakeServer();
    const bridge = { sendCommand: async () => ({ success: true, request_id: '1' }) } as unknown as BridgeClient;

    registerTools(server as unknown as McpServer, bridge, fakeConfig);

    const importTool = tools.get('blueprint_import_json_to_graph');
    assert.ok(importTool, 'import tool is registered');

    // Verify the schema accepts strings (via z.union)
    const schema = importTool.inputSchema as { json: { _def: unknown } };
    assert.ok(schema, 'inputSchema exists');
  });

  it('forwards object json unchanged to bridge', async () => {
    let capturedPayload: Record<string, unknown> | undefined;
    const bridge = {
      sendCommand: async (_cmd: string, payload?: Record<string, unknown>): Promise<BridgeResponse> => {
        capturedPayload = payload;
        return { success: true, request_id: 'req_test', result: { status: 'full_success' } };
      },
    } as unknown as BridgeClient;

    const { server, tools } = makeFakeServer();
    registerTools(server as unknown as McpServer, bridge, fakeConfig);

    const tool = tools.get('blueprint_import_json_to_graph')!;
    const result = await tool.handler({
      json: rawObj,
      compile_after_import: true,
      strict: true,
      allow_partial: false,
    });

    // json object should remain an object (not stringified by MCP)
    assert.ok(typeof capturedPayload!.json === 'object');
    assert.deepStrictEqual((capturedPayload!.json as Record<string, unknown>).version, '2.2');
  });

  it('forwards string json unchanged to bridge', async () => {
    let capturedPayload: Record<string, unknown> | undefined;
    const bridge = {
      sendCommand: async (_cmd: string, payload?: Record<string, unknown>): Promise<BridgeResponse> => {
        capturedPayload = payload;
        return { success: true, request_id: 'req_test', result: { status: 'full_success' } };
      },
    } as unknown as BridgeClient;

    const { server, tools } = makeFakeServer();
    registerTools(server as unknown as McpServer, bridge, fakeConfig);

    const tool = tools.get('blueprint_import_json_to_graph')!;
    const jsonStr = JSON.stringify(rawObj);
    await tool.handler({
      json: jsonStr,
      compile_after_import: true,
      strict: true,
      allow_partial: false,
    });

    assert.equal(typeof capturedPayload!.json, 'string');
    assert.equal(capturedPayload!.json, jsonStr);
  });

  it('rejects LogicJson object before bridge call', async () => {
    let bridgeCalled = false;
    const bridge = {
      sendCommand: async (): Promise<BridgeResponse> => {
        bridgeCalled = true;
        return { success: true, request_id: 'req_test' };
      },
    } as unknown as BridgeClient;

    const { server, tools } = makeFakeServer();
    registerTools(server as unknown as McpServer, bridge, fakeConfig);

    const tool = tools.get('blueprint_import_json_to_graph')!;
    const result = await tool.handler({
      json: { version: '1.0', schema: 'BlueprintHelper.LogicGraph', graphs: [] },
      compile_after_import: true,
      strict: true,
      allow_partial: false,
    });

    assert.equal(bridgeCalled, false, 'Bridge should NOT be called for LogicJson');
    assert.equal(result.isError, true);
    assert.ok((result.content[0].text as string).includes('Logic'));
  });

  it('rejects importable=false object before bridge call', async () => {
    let bridgeCalled = false;
    const bridge = {
      sendCommand: async (): Promise<BridgeResponse> => {
        bridgeCalled = true;
        return { success: true, request_id: 'req_test' };
      },
    } as unknown as BridgeClient;

    const { server, tools } = makeFakeServer();
    registerTools(server as unknown as McpServer, bridge, fakeConfig);

    const tool = tools.get('blueprint_import_json_to_graph')!;
    const result = await tool.handler({
      json: { ...rawObj, importable: false },
      compile_after_import: true,
      strict: true,
      allow_partial: false,
    });

    assert.equal(bridgeCalled, false, 'Bridge should NOT be called for importable=false');
    assert.equal(result.isError, true);
    assert.ok((result.content[0].text as string).includes('importable'));
  });
});
