import assert from 'node:assert/strict';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test, { describe, it } from 'node:test';
import type { BridgeResponse } from './bridge-client.js';
import { registerWithBridge, registerResourcesWithBridge, invokeTool, withConnectedMcpServer } from './test-harness.js';

const PLUGIN_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const FROZEN_DESCRIPTION_PREFIX = 'FROZEN / Expert-only / Normal agents must not call directly';

const AGENT_FACING_TOOL_NAMES = [
  'blueprinthelper_read_agent_guide',
  'blueprint_get_runtime_profile',
  'blueprinthelper_diagnostics',
  'blueprinthelper_diagnostics_runtime',
  'blueprinthelper_read_context',
  'blueprinthelper_read_task_context',
  'blueprinthelper_read_reference_context',
  'blueprinthelper_preview_task',
  'blueprinthelper_execute_task',
  'blueprinthelper_get_task_result',
];

const FROZEN_EXPERT_TOOL_NAMES = [
  'blueprint_get_editor_context',
  'blueprint_get_logic_md',
  'blueprint_create_asset',
  'blueprint_add_component',
  'blueprint_import_json_to_graph',
  'blueprint_compile_blueprint',
  'blueprint_save_asset',
  'blueprint_close_editor',
  'blueprint_play_in_editor',
  'blueprint_exec_console_command',
  'blueprint_build_project',
];

const AGENT_GUIDE_FORBIDDEN_PATTERNS = [
  /\bblueprint_add_[a-z_]+\b/,
  /\bblueprint_set_[a-z_]+\b/,
  /\bblueprint_import_[a-z_]+\b/,
  /\bblueprint_export_[a-z_]+\b/,
  /\bblueprint_compile_[a-z_]+\b/,
  /\bblueprint_save_[a-z_]+\b/,
  /\bblueprint_open_asset\b/,
  /\bblueprint_close_editor\b/,
  /\bblueprint_play_in_editor\b/,
  /\bblueprint_stop_pie\b/,
  /\bblueprint_undo\b/,
  /\bblueprint_redo\b/,
  /\bblueprint_exec_console_command\b/,
  /\bblueprint_build_project\b/,
  /\bblueprint_create_blueprint\b/,
  /\bblueprint_get_logic(?:_md|_json)?\b/,
  /\bblueprint_get_widget_[a-z_]+\b/,
  /\bblueprint_get_datatable_[a-z_]+\b/,
  /\bblueprint_get_object_[a-z_]+\b/,
  /\bPIE\b/,
  /\bconsole\b/i,
  /控制台/,
];

function readAgentGuideMarkdownFiles(): Array<{ file: string; text: string }> {
  const root = path.resolve(PLUGIN_ROOT, 'Resources', 'AgentGuide');
  const files: Array<{ file: string; text: string }> = [];
  const visit = (dir: string) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        visit(fullPath);
      } else if (entry.isFile() && entry.name.endsWith('.md')) {
        files.push({ file: path.relative(root, fullPath), text: readFileSync(fullPath, 'utf8') });
      }
    }
  };
  visit(root);
  return files;
}

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

test('registered non-default tools remain available but are marked frozen expert-only', () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));

  for (const name of FROZEN_EXPERT_TOOL_NAMES) {
    const tool = tools.get(name);
    assert.ok(tool, `${name} should remain registered for compatibility`);
    assert.ok(
      tool.description?.includes(FROZEN_DESCRIPTION_PREFIX),
      `${name} description should include frozen expert-only guidance`,
    );
  }
});

test('agent-facing tools are not marked frozen', () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));

  for (const name of AGENT_FACING_TOOL_NAMES) {
    const tool = tools.get(name);
    assert.ok(tool, `${name} should be registered`);
    assert.equal(
      tool.description?.includes(FROZEN_DESCRIPTION_PREFIX),
      false,
      `${name} should remain in the normal Agent surface`,
    );
  }

  const openEditor = tools.get('blueprint_open_editor');
  assert.ok(openEditor, 'blueprint_open_editor should remain registered as a preflight helper');
  assert.equal(openEditor.description?.includes(FROZEN_DESCRIPTION_PREFIX), false);
  assert.match(openEditor.description ?? '', /Preflight only/i);
});

test('blueprinthelper_read_agent_guide returns the AgentGuide onboarding index without Bridge', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return { request_id: 'test', success: false, message: 'Bridge should not be called for AgentGuide' };
  });

  assert.equal(tools.has('blueprint_get_rule_markdown'), false);
  const tool = tools.get('blueprinthelper_read_agent_guide');
  assert.ok(tool);

  const result = await invokeTool(tool, {});
  const expected = readFileSync(
    path.resolve(PLUGIN_ROOT, 'Resources', 'AgentGuide', '00_Agent_Onboarding_Index_20260504.md'),
    'utf8',
  );

  assert.equal(result.isError, undefined);
  assert.equal(result.content[0].type, 'text');
  assert.equal(result.content[0].text, expected);
  assert.deepEqual(calls, []);
});

test('blueprinthelper_read_agent_guide does not expose frozen tool names', async () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));
  const tool = tools.get('blueprinthelper_read_agent_guide');
  assert.ok(tool);

  const result = await invokeTool(tool, {});
  const text = result.content[0].text ?? '';

  for (const pattern of AGENT_GUIDE_FORBIDDEN_PATTERNS) {
    assert.doesNotMatch(text, pattern);
  }
});

test('AgentGuide markdown does not document frozen direct tool calls', () => {
  for (const { file, text } of readAgentGuideMarkdownFiles()) {
    for (const pattern of AGENT_GUIDE_FORBIDDEN_PATTERNS) {
      assert.doesNotMatch(text, pattern, `${file} should not expose ${pattern}`);
    }
  }
});

test('blueprinthelper_read_context reads blueprint logic as LogicMd through ReadContextPack', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        ok: true,
        schema: 'BlueprintHelper.ToolResultBase.v1',
        operation: 'read_blueprint_logic_md',
        status: 'completed',
        modified: false,
        target: {
          asset_path: '/Game/BP/BP_PhysicsDoor',
          target_type: 'graph',
          graph: 'EventGraph',
        },
        data: {
          schema: 'LogicMd.v1',
          markdown: '# EventGraph',
          scope: 'target_graph',
          stats: { nodes: 3 },
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_read_context');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP/BP_PhysicsDoor',
      target_type: 'graph',
      target_name: 'EventGraph',
    },
    view: {
      format: 'logic_md',
    },
  });

  assert.deepEqual(calls, [
    {
      command: 'read_blueprint_logic_md',
      payload: {
        asset_path: '/Game/BP/BP_PhysicsDoor',
        graph: 'EventGraph',
        scope: 'target_graph',
      },
    },
  ]);
  assert.equal(result.isError, false);
  assert.equal(result.structuredContent?.schema, 'BlueprintHelper.McpToolResult.v1');
  assert.equal(result.structuredContent?.operation, 'read_context');
  assert.equal(result.structuredContent?.modified, false);
  assert.equal((result.structuredContent?.data as Record<string, unknown>)?.schema, 'ReadContextPack.v1');
  assert.equal((result.structuredContent?.data as Record<string, unknown>)?.format, 'logic_md');
  assert.deepEqual((result.structuredContent?.data as Record<string, unknown>)?.payload, {
    schema: 'LogicMd.v1',
    markdown: '# EventGraph',
    scope: 'target_graph',
    stats: { nodes: 3 },
  });
});

test('blueprinthelper_read_context reads blueprint logic as LogicJson by custom event target', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        ok: true,
        schema: 'BlueprintHelper.ToolResultBase.v1',
        operation: 'read_blueprint_logic_json',
        status: 'completed',
        modified: false,
        target: {
          asset_path: '/Game/BP/BP_PhysicsDoor',
          target_type: 'custom_event',
          event: 'OpenDoor',
        },
        data: {
          schema: 'LogicJson.v1',
          logic: { nodes: [{ id: 'OpenDoor_entry' }] },
          scope: 'target_custom_event',
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_read_context');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP/BP_PhysicsDoor',
      target_type: 'custom_event',
      target_name: 'OpenDoor',
    },
    view: {
      format: 'logic_json',
    },
  });

  assert.deepEqual(calls, [
    {
      command: 'read_blueprint_logic_json',
      payload: {
        asset_path: '/Game/BP/BP_PhysicsDoor',
        event: 'OpenDoor',
        scope: 'target_custom_event',
      },
    },
  ]);
  assert.equal(result.isError, false);
  assert.equal((result.structuredContent?.data as Record<string, unknown>)?.schema, 'ReadContextPack.v1');
  assert.equal((result.structuredContent?.data as Record<string, unknown>)?.format, 'logic_json');
  assert.deepEqual((result.structuredContent?.data as Record<string, unknown>)?.payload, {
    schema: 'LogicJson.v1',
    logic: { nodes: [{ id: 'OpenDoor_entry' }] },
    scope: 'target_custom_event',
  });
});

test('blueprinthelper_read_context returns blueprint logic schema without Bridge', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return { request_id: 'test', success: false, message: 'Bridge should not be called for schema format' };
  });

  const tool = tools.get('blueprinthelper_read_context');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP/BP_PhysicsDoor',
      target_type: 'blueprint',
    },
    view: {
      format: 'schema',
    },
  });

  assert.deepEqual(calls, []);
  assert.equal(result.isError, false);
  assert.equal((result.structuredContent?.data as Record<string, unknown>)?.schema, 'ReadContextPack.v1');
  assert.equal((result.structuredContent?.data as Record<string, unknown>)?.format, 'schema');
  assert.deepEqual(((result.structuredContent?.data as Record<string, unknown>)?.payload as Record<string, unknown>)?.formats, [
    'logic_md',
    'logic_json',
    'summary',
    'schema',
  ]);
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
  const fixturesDir = path.resolve(PLUGIN_ROOT, 'Develop', 'TestFixtures', 'MCPRegression');
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
