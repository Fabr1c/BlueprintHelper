import assert from 'node:assert/strict';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import test, { describe, it } from 'node:test';
import type { BridgeResponse } from './bridge-client.js';
import { registerWithBridge, registerResourcesWithBridge, invokeTool, withConnectedMcpServer } from './test-harness.js';

test('blueprint_export_to_json accepts current and legacy scope values', () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));
  const tool = tools.get('blueprint_export_to_json');
  assert.ok(tool);

  for (const scope of ['graph', 'blueprint', 'selection', 'full_graph', 'full_blueprint']) {
    assert.equal(tool.inputSchema.parse({ scope }).scope, scope);
  }
});

test('blueprint_import_json_to_graph defaults to strict import without manual auth token', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return { request_id: 'test', success: true };
  });
  const tool = tools.get('blueprint_import_json_to_graph');
  assert.ok(tool);

  await invokeTool(tool, { json: '{"nodes":[],"links":[]}' });

  assert.deepEqual(calls, [
    {
      command: 'import_json',
      payload: {
        json: '{"nodes":[],"links":[]}',
        compile_after_import: true,
        strict: true,
        allow_partial: false,
      },
    },
  ]);
  assert.equal(Object.hasOwn(calls[0].payload ?? {}, 'auth_token'), false);
});

test('blueprint_get_logic keeps markdown first when no structured metadata is available', async () => {
  const tools = registerWithBridge(async () => ({
    request_id: 'test',
    success: true,
    result: {
      markdown: '# Logic',
      status: 'failed',
      operations_applied: 0,
      nodes_created: 1,
      links_connected: 0,
      warnings: ['link skipped'],
      errors: ['missing pin'],
      rolled_back: true,
    },
  }));
  const tool = tools.get('blueprint_get_logic');
  assert.ok(tool);

  const result = await invokeTool(tool, {});

  assert.equal(result.content[0].text, '# Logic');
  assert.equal(result.isError, false);
  assert.equal(result.content.length, 1);
  assert.equal(result.structuredContent, undefined);
});

test('blueprint_get_logic returns markdown text with structured metadata', async () => {
  const tools = registerWithBridge(async () => ({
    request_id: 'test',
    success: true,
    result: {
      format: 'logic_md',
      schema: 'BlueprintHelper.LogicMd.v1',
      assetPath: '/Game/BP/BP_Test.BP_Test',
      graph: 'EventGraph',
      importable: false,
      markdown: '# Logic',
      stats: { nodes: 2 },
      diagnostics: [{ severity: 'info', code: 'ok', message: 'ready' }],
    },
  }));
  const tool = tools.get('blueprint_get_logic');
  assert.ok(tool);

  const result = await invokeTool(tool, {});

  assert.equal(result.content[0].type, 'text');
  assert.equal(result.content[0].text, '# Logic');
  assert.deepEqual(result.structuredContent, {
    format: 'logic_md',
    schema: 'BlueprintHelper.LogicMd.v1',
    assetPath: '/Game/BP/BP_Test.BP_Test',
    graph: 'EventGraph',
    importable: false,
    stats: { nodes: 2 },
    diagnostics: [{ severity: 'info', code: 'ok', message: 'ready' }],
  });
});

test('blueprint_get_logic_json unwraps Bridge JSON strings into structuredContent', async () => {
  const logicPayload = {
    format: 'logic_json',
    schema: 'BlueprintHelper.LogicJson.v1',
    assetPath: '/Game/BP/BP_Test.BP_Test',
    graph: 'EventGraph',
    importable: false,
    logic: { nodes: [{ id: 'BeginPlay' }] },
    stats: { nodes: 1 },
  };
  const tools = registerWithBridge(async () => ({
    request_id: 'test',
    success: true,
    result: JSON.stringify(logicPayload) as unknown as BridgeResponse['result'],
  }));
  const tool = tools.get('blueprint_get_logic_json');
  assert.ok(tool);

  const result = await invokeTool(tool, {});

  assert.equal(result.content[0].type, 'text');
  assert.match(result.content[0].text ?? '', /Exported LogicJson/);
  assert.equal((result.content[0].text ?? '').includes('\\"nodes\\"'), false);
  assert.deepEqual(result.structuredContent, logicPayload);
});

test('blueprint_export_to_json defaults to a RawJson resource link', async () => {
  const tools = registerWithBridge(async () => ({
    request_id: 'test',
    success: true,
    result: {
      payload: { version: '2.2', schema: 'BlueprintHelper.JsonToBlueprint', nodes: [], links: [] },
      json: { version: '2.2', schema: 'BlueprintHelper.JsonToBlueprint', nodes: [], links: [] },
      stats: { nodes: 0, links: 0 },
      diagnostics: [],
    },
  }));
  const tool = tools.get('blueprint_export_to_json');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    target_blueprint: '/Game/BP/BP_Test.BP_Test',
    target_graph: 'EventGraph',
  });

  assert.equal(result.content[0].type, 'text');
  assert.equal(result.content[1].type, 'resource_link');
  assert.equal(result.content[1].mimeType, 'application/json');
  assert.equal(result.structuredContent?.format, 'raw_json_ref');
  assert.equal(result.structuredContent?.schema, 'BlueprintHelper.RawJsonRef.v1');
  assert.equal(result.structuredContent?.assetPath, '/Game/BP/BP_Test.BP_Test');
  assert.equal(result.structuredContent?.graph, 'EventGraph');
  assert.equal(result.structuredContent?.importable, true);
  assert.match(String(result.structuredContent?.rawUri), /^blueprint:\/\/asset\//);
  assert.deepEqual((result.structuredContent as Record<string, unknown>)?.json, {
    version: '2.2', schema: 'BlueprintHelper.JsonToBlueprint', nodes: [], links: [],
  });
});

test('blueprint_export_to_json passes real MCP SDK output validation', async () => {
  await withConnectedMcpServer(
    async () => ({
      request_id: 'test',
      success: true,
      result: {
        payload: { version: '2.2', schema: 'BlueprintHelper.JsonToBlueprint', nodes: [], links: [] },
        json: { version: '2.2', schema: 'BlueprintHelper.JsonToBlueprint', nodes: [], links: [] },
        stats: { nodes: 0, links: 0 },
        diagnostics: [],
      },
    }),
    async (client) => {
      const result = await client.callTool({
        name: 'blueprint_export_to_json',
        arguments: {
          target_blueprint: '/Game/BP/BP_Test.BP_Test',
          target_graph: 'EventGraph',
        },
      });

      assert.equal(result.isError, false, JSON.stringify(result));
      const structured = result.structuredContent as Record<string, unknown> | undefined;
      assert.equal(structured?.format, 'raw_json_ref');
      assert.match(String(structured?.rawUri), /^blueprint:\/\/asset\//);
    },
  );
});

test('blueprint_export_to_json keeps legacy text JSON mode when requested', async () => {
  const rawPayload = { version: '2.2', schema: 'BlueprintHelper.JsonToBlueprint', nodes: [], links: [] };
  const tools = registerWithBridge(async () => ({
    request_id: 'test',
    success: true,
    result: { payload: rawPayload, json: rawPayload },
  }));
  const tool = tools.get('blueprint_export_to_json');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    target_blueprint: '/Game/BP/BP_Test.BP_Test',
    response_mode: 'legacy_text_json',
  });

	assert.equal(result.content.length, 1);
	const parsed = JSON.parse(result.content[0].text ?? '');
	assert.equal(parsed.format, 'raw_json');
	assert.equal(parsed.schema, 'BlueprintHelper.RawJsonRef.v1');
	assert.deepEqual(parsed.json, rawPayload);
	assert.equal(result.structuredContent?.format, 'raw_json');
	assert.equal(result.structuredContent?.schema, 'BlueprintHelper.RawJsonRef.v1');
	assert.deepEqual((result.structuredContent as Record<string, unknown>)?.json, rawPayload);
});

test('blueprint asset resource reads raw JSON through Bridge on demand', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const resources = registerResourcesWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        payload: { nodes: [], links: [] },
        json: { nodes: [], links: [] },
      },
    };
  });

  const resource = resources.get('blueprint-asset-view');
  assert.ok(resource);

  const uri = new URL('blueprint://asset/Game%2FBP%2FBP_Test.BP_Test?view=raw-json&graph=EventGraph');
  const result = await resource.handler(uri);

  assert.deepEqual(calls, [
    {
      command: 'export_to_json',
      payload: {
        target_blueprint: '/Game/BP/BP_Test.BP_Test',
        target_graph: 'EventGraph',
      },
    },
  ]);
  assert.equal(result.contents[0].uri, uri.href);
  assert.equal(result.contents[0].mimeType, 'application/json');
  assert.deepEqual(JSON.parse(result.contents[0].text), { nodes: [], links: [] });
});

test('MCP regression fixtures exist and are valid JSON', () => {
  const fixturesDir = path.resolve(process.cwd(), '..', 'Resources', 'TestFixtures', 'MCPRegression');
  const requiredFixtures = [
    'legacy_full_graph_scope.mcp.json',
    'legacy_full_blueprint_scope.mcp.json',
    'strict_import_link_failure.mcp.json',
    'strict_import_default_failure.mcp.json',
  ];

  assert.equal(existsSync(fixturesDir), true);
  const fixtureNames = new Set(readdirSync(fixturesDir));
  for (const fixtureName of requiredFixtures) {
    assert.equal(fixtureNames.has(fixtureName), true, fixtureName);
    JSON.parse(readFileSync(path.join(fixturesDir, fixtureName), 'utf8'));
  }
});

describe('Bridge payload shape regression (object-first)', () => {
  const rawObj = {
    version: '2.2',
    schema: 'BlueprintHelper.JsonToBlueprint',
    nodes: [],
    links: [],
  };

  it('handles { payload: object, json: object } shape', async () => {
    const tools = registerWithBridge(async () => ({
      request_id: 'test',
      success: true,
      result: { payload: rawObj, json: rawObj, stats: { nodes: 0, links: 0 }, diagnostics: [] },
    }));
    const tool = tools.get('blueprint_export_to_json');
    assert.ok(tool);
    const result = await invokeTool(tool, {
      target_blueprint: '/Game/BP/BP_Test.BP_Test',
    });
    assert.equal(result.content[1].type, 'resource_link');
    assert.equal(result.structuredContent?.format, 'raw_json_ref');
  });

  it('handles { json: object } shape (no payload)', async () => {
    const tools = registerWithBridge(async () => ({
      request_id: 'test',
      success: true,
      result: { json: rawObj, stats: { nodes: 0, links: 0 } },
    }));
    const tool = tools.get('blueprint_export_to_json');
    assert.ok(tool);
    const result = await invokeTool(tool, {
      target_blueprint: '/Game/BP/BP_Test.BP_Test',
    });
    assert.equal(result.structuredContent?.format, 'raw_json_ref');
  });

  it('handles legacy { json: string } shape (backward compat)', async () => {
    const tools = registerWithBridge(async () => ({
      request_id: 'test',
      success: true,
      result: { json: JSON.stringify(rawObj) },
    }));
    const tool = tools.get('blueprint_export_to_json');
    assert.ok(tool);
    const result = await invokeTool(tool, {
      target_blueprint: '/Game/BP/BP_Test.BP_Test',
    });
    assert.equal(result.structuredContent?.format, 'raw_json_ref');
  });

  it('handles legacy { json_text: string } shape (backward compat)', async () => {
    const tools = registerWithBridge(async () => ({
      request_id: 'test',
      success: true,
      result: { json_text: JSON.stringify(rawObj) },
    }));
    const tool = tools.get('blueprint_export_to_json');
    assert.ok(tool);
    const result = await invokeTool(tool, {
      target_blueprint: '/Game/BP/BP_Test.BP_Test',
    });
    assert.equal(result.structuredContent?.format, 'raw_json_ref');
  });

  it('handles { payload: obj, json: obj, json_text: string } — payload wins', async () => {
    const otherObj = { ...rawObj, version: '1.0' };
    const tools = registerWithBridge(async () => ({
      request_id: 'test',
      success: true,
      result: {
        payload: rawObj,  // version 2.2
        json: otherObj,   // version 1.0 (should NOT be used for rawUri content)
        json_text: JSON.stringify({ ...rawObj, version: '0.9' }),
      },
    }));
    const tool = tools.get('blueprint_export_to_json');
    assert.ok(tool);
    const result = await invokeTool(tool, {
      target_blueprint: '/Game/BP/BP_Test.BP_Test',
    });
    assert.equal(result.structuredContent?.format, 'raw_json_ref');
  });
});

describe('MCP import regression (object-first)', () => {
  const rawObj = {
    version: '2.2',
    schema: 'BlueprintHelper.JsonToBlueprint',
    nodes: [],
    links: [],
  };

  it('import returns success for valid object json', async () => {
    let capturedJson: unknown;
    const tools = registerWithBridge(async (_cmd, payload) => {
      capturedJson = payload?.json;
      return { request_id: 'test', success: true, result: { status: 'full_success' } };
    });
    const tool = tools.get('blueprint_import_json_to_graph');
    assert.ok(tool);
    const result = await invokeTool(tool, {
      json: rawObj,
      compile_after_import: true,
      strict: true,
      allow_partial: false,
    });
    assert.equal(typeof capturedJson, 'object');
    assert.equal(result.isError, false);
  });

  it('import returns success for valid string json', async () => {
    let capturedJson: unknown;
    const tools = registerWithBridge(async (_cmd, payload) => {
      capturedJson = payload?.json;
      return { request_id: 'test', success: true, result: { status: 'full_success' } };
    });
    const tool = tools.get('blueprint_import_json_to_graph');
    assert.ok(tool);
    const result = await invokeTool(tool, {
      json: JSON.stringify(rawObj),
      compile_after_import: true,
      strict: true,
      allow_partial: false,
    });
    assert.equal(typeof capturedJson, 'string');
    assert.equal(result.isError, false);
  });
});
