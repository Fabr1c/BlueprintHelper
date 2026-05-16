import assert from 'node:assert/strict';
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test, { describe, it } from 'node:test';
import type { BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import { registerWithBridge, registerResourcesWithBridge, invokeTool } from '../../test-support/test-harness.js';
import { registerSharedRegistryTools } from '../../mcp/tools/shared-registry-adapter.js';

const MCP_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const SHARED_PLUGIN_ROOT = path.resolve(MCP_ROOT, '..');
const REPO_ROOT = path.resolve(SHARED_PLUGIN_ROOT, '..');
const CLAUDE_PLUGIN_ROOT = path.resolve(REPO_ROOT, 'ClaudePlugin');
const UE_PLUGIN_ROOT = path.resolve(REPO_ROOT, 'BlueprintHelper');
const FROZEN_DESCRIPTION_PREFIX = 'FROZEN / Expert-only / Normal agents must not call directly';

const AGENT_FACING_TOOL_NAMES = [
  'blueprinthelper_read_agent_guide',
  'blueprinthelper_get_debug_case',
  'blueprint_get_runtime_profile',
  'blueprinthelper_diagnostics',
  'blueprinthelper_diagnostics_runtime',
  'blueprinthelper_request_write_session',
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
  'blueprint_read_components',
  'blueprint_add_component',
  'blueprint_set_component_property',
  'blueprint_set_component_properties',
  'blueprint_remove_component',
  'blueprint_validate_json',
  'blueprint_export_to_json',
  'blueprint_get_logic',
  'blueprint_get_logic_json',
  'blueprint_import_json_to_graph',
  'blueprint_import_agent_graph',
  'blueprint_compile_blueprint',
  'blueprint_open_asset',
  'blueprint_list_assets',
  'blueprint_search_assets',
  'blueprint_save_asset',
  'blueprint_get_asset_info',
  'blueprint_list_graphs',
  'blueprint_list_variables',
  'blueprint_list_event_dispatchers',
  'blueprint_add_variable',
  'blueprint_remove_variable',
  'blueprint_add_graph',
  'blueprint_remove_graph',
  'blueprint_add_event_dispatcher',
  'blueprint_delete_nodes',
  'blueprint_get_widget_tree',
  'blueprint_add_widget',
  'blueprint_remove_widget',
  'blueprint_move_widget',
  'blueprint_get_widget_properties',
  'blueprint_set_widget_property',
  'blueprint_get_object_properties',
  'blueprint_set_object_property',
  'blueprint_get_datatable_rows',
  'blueprint_add_datatable_row',
  'blueprint_update_datatable_row',
  'blueprint_delete_datatable_row',
  'blueprint_undo',
  'blueprint_redo',
  'blueprint_close_editor',
  'blueprint_play_in_editor',
  'blueprint_stop_pie',
  'blueprint_create_blueprint',
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
];

function readAgentGuideMarkdownFiles(): Array<{ file: string; text: string }> {
  const root = path.resolve(UE_PLUGIN_ROOT, 'Resources', 'AgentGuide');
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

test('frozen direct MCP tools are not registered', () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));

  for (const name of FROZEN_EXPERT_TOOL_NAMES) {
    assert.equal(tools.has(name), false, `${name} should be unregistered`);
  }

  for (const [name, tool] of tools) {
    assert.equal(
      tool.description?.includes(FROZEN_DESCRIPTION_PREFIX),
      false,
      `${name} should not be exposed with frozen-only guidance`,
    );
  }
});

test('blueprinthelper_request_write_session stores Bridge session without exposing its raw id', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  let storedSessionId = '';
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        write_session: {
          session_id: 'write-session-secret',
          scope: 'project',
          expires_at_utc: '2026-05-10T00:15:00Z',
        },
      },
    };
  }, {}, {
    setWriteSessionId: (sessionId: string) => {
      storedSessionId = sessionId;
    },
  });
  const tool = tools.get('blueprinthelper_request_write_session');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    reason: 'edit requested by user',
    scope: 'project',
    ttl_seconds: 900,
    asset_paths: ['/Game/BP_Player'],
  });

  assert.deepEqual(calls, [
    {
      command: 'request_write_session',
      payload: {
        reason: 'edit requested by user',
        scope: 'project',
        ttl_seconds: 900,
        asset_paths: ['/Game/BP_Player'],
      },
    },
  ]);
  assert.equal(storedSessionId, 'write-session-secret');
  assert.equal(result.isError, false);
  assert.equal(JSON.stringify(result).includes('write-session-secret'), false);
});

test('blueprinthelper_diagnostics accepts project agent-profile without legacy settings.json', async () => {
  const previousCwd = process.cwd();
  const projectDir = mkdtempSync(path.join(tmpdir(), 'blueprinthelper-diagnostics-'));
  try {
    mkdirSync(path.join(projectDir, '.blueprinthelper'), { recursive: true });
    writeFileSync(path.join(projectDir, 'SmokeProject.uproject'), '{}', 'utf8');
    writeFileSync(
      path.join(projectDir, '.blueprinthelper', 'agent-profile.json'),
      JSON.stringify({
        schema: 'BlueprintHelper.AgentProfile.v1',
        environment: {
          ue_engine_dir: path.join(projectDir, 'FakeUE_5.6'),
        },
      }),
      'utf8',
    );
    process.chdir(projectDir);

    const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));
    const tool = tools.get('blueprinthelper_diagnostics');
    assert.ok(tool);

    const result = await invokeTool(tool, {});
    assert.equal(result.isError, false);
    const data = result.structuredContent?.data as Record<string, unknown>;
    const markdown = String(data.markdown ?? '');
    assert.match(markdown, /agent_profile\.valid/);
    assert.doesNotMatch(markdown, /settings\.unavailable/);
    assert.doesNotMatch(markdown, /\.blueprinthelper\/settings\.json/);
  } finally {
    process.chdir(previousCwd);
    rmSync(projectDir, { recursive: true, force: true });
  }
});

test('frozen expert-only tools are removed from the MCP registry', () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));

  for (const name of FROZEN_EXPERT_TOOL_NAMES) {
    assert.equal(tools.has(name), false, `${name} should not be callable by name`);
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
});

test('setup command is a retired compatibility pointer to the root installer', () => {
  const text = readFileSync(path.resolve(CLAUDE_PLUGIN_ROOT, 'commands', 'setup.md'), 'utf8');
  const frontMatter = text.match(/^---\r?\n([\s\S]*?)\r?\n---/)?.[1] ?? '';

  assert.doesNotMatch(frontMatter, /\bmcp__blueprint-helper(?:__|\s|,|$)/);
  assert.match(text, /install\.ps1/);
  assert.match(text, /retired/i);
  assert.match(text, /Do not recreate the old setup flow/);

  for (const toolName of FROZEN_EXPERT_TOOL_NAMES) {
    assert.doesNotMatch(frontMatter, new RegExp(`mcp__blueprint-helper__${toolName}\\b`));
  }
});

test('active setup docs do not contain legacy path env pollution', () => {
  const activeDocs = [
    path.resolve(CLAUDE_PLUGIN_ROOT, 'AGENTS.md'),
    path.resolve(CLAUDE_PLUGIN_ROOT, 'README.md'),
    path.resolve(CLAUDE_PLUGIN_ROOT, 'commands', 'setup.md'),
    path.resolve(CLAUDE_PLUGIN_ROOT, 'commands', 'configure.md'),
    path.resolve(CLAUDE_PLUGIN_ROOT, 'skills', 'blueprint-helper', 'SKILL.md'),
    path.resolve(CLAUDE_PLUGIN_ROOT, 'skills', 'blueprint-helper', 'references', '04_Tool_Surface_Field_Templates_20260512.md'),
    path.resolve(UE_PLUGIN_ROOT, 'Docs', 'Install_CLI_QuickStart.md'),
    path.resolve(UE_PLUGIN_ROOT, 'Docs', 'CLI_Tools_API_Reference.md'),
    path.resolve(UE_PLUGIN_ROOT, 'Resources', 'AgentGuide', '00_Agent_Onboarding_Index_20260504.md'),
    path.resolve(UE_PLUGIN_ROOT, 'Resources', 'AgentGuide', 'Reference', '03_Runtime_Profile_And_Diagnostics.md'),
    path.resolve(UE_PLUGIN_ROOT, 'Resources', 'AgentGuide', 'Reference', '04_Tool_Surface_Field_Templates_20260512.md'),
    path.resolve(UE_PLUGIN_ROOT, 'Resources', 'Docs', 'TaskSpec_TaskPlan_Contract_20260504.md'),
    path.resolve(UE_PLUGIN_ROOT, 'Resources', 'Docs', 'Setup', 'RuntimeProfile_Example.json'),
    path.resolve(UE_PLUGIN_ROOT, 'Resources', 'Docs', 'Setup', 'SetupProfile_Example.json'),
  ];

  const forbiddenPatterns = [
    /\bUE_ENGINE_DIR\b/,
    /\bUE_PROJECT_FILE\b/,
    /\bue_project_file\b/,
    /<PROJECT_FILE>/,
    /<UE_ENGINE_DIR>/,
    /\b[A-Z]:[\\/](?:Users|UE_|UnrealPractise)\b/,
    /MrStone\.uproject/,
  ];

  for (const docPath of activeDocs) {
    assert.ok(existsSync(docPath), `${path.relative(REPO_ROOT, docPath)} should exist`);
    const text = readFileSync(docPath, 'utf8');
    for (const pattern of forbiddenPatterns) {
      assert.doesNotMatch(text, pattern, `${path.relative(REPO_ROOT, docPath)} should not contain ${pattern}`);
    }
  }

  const setupProfile = JSON.parse(readFileSync(
    path.resolve(UE_PLUGIN_ROOT, 'Resources', 'Docs', 'Setup', 'SetupProfile_Example.json'),
    'utf8',
  )) as Record<string, unknown>;
  assert.ok('environment' in setupProfile);
  assert.equal(JSON.stringify(setupProfile).includes('ue_project_file'), false);
});

test('blueprint_open_editor requires explicit project_file tool argument instead of UE_PROJECT_FILE env', async () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }), {
    ueEngineDir: '/fake/engine',
  });
  const tool = tools.get('blueprint_open_editor');
  assert.ok(tool);

  const parsed = tool.inputSchema.parse({
    project_file: '/fake/project/MrStone.uproject',
    wait_timeout_ms: 1,
  });
  assert.equal(parsed.project_file, '/fake/project/MrStone.uproject');
  assert.equal(tool.description?.includes('UE_PROJECT_FILE'), false);
  assert.equal(tool.description?.includes('Requires UE_ENGINE_DIR env var'), false);

  const result = await invokeTool(tool, { project_file: '/fake/project' });
  assert.equal(result.isError, true);
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
    path.resolve(UE_PLUGIN_ROOT, 'Resources', 'AgentGuide', '00_Agent_Onboarding_Index_20260504.md'),
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

test('shared registry MCP adapter can register default task tools', () => {
  const registered: string[] = [];
  const server = {
    registerTool: (name: string) => {
      registered.push(name);
    },
  };

  registerSharedRegistryTools(server as never, {
    sendCommand: async () => ({ request_id: 'test', success: true }),
  } as never, {
    cwd: process.cwd(),
    ueEngineDir: '',
    toolNames: new Set([
      'blueprinthelper_read_task_context',
      'blueprinthelper_preview_task',
      'blueprinthelper_execute_task',
    ]),
  });

  for (const name of [
    'blueprinthelper_read_task_context',
    'blueprinthelper_preview_task',
    'blueprinthelper_execute_task',
  ]) {
    assert.ok(registered.includes(name), name);
  }
});

test('AgentGuide documents non-owned graph content as read-only for normal GraphWrite', () => {
  const workflow = readFileSync(
    path.resolve(UE_PLUGIN_ROOT, 'Resources', 'AgentGuide', 'Workflows', '04_TaskSpec_Edit_Blueprint_Workflow.md'),
    'utf8',
  );

  assert.match(workflow, /Non-BlueprintHelper-owned graph content/);
  assert.match(workflow, /read-only/);
  assert.match(workflow, /allow_modify_user_nodes=false/);
  assert.match(workflow, /unsupported_scope_policy/);
  assert.match(workflow, /GUID-first selectors remain expert\/debug fallback only/);
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

test('blueprinthelper_read_context reads function logic_md through structured target slice', async () => {
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
          asset_path: '/Game/Gameplay/Maze/BP_Maze',
          target_type: 'function',
          function: 'AddMazeRelativeRotation',
        },
        data: {
          schema: 'LogicJson.v1',
          scope: 'target_function',
          logic: {
            asset_path: '/Game/Gameplay/Maze/BP_Maze',
            graph: 'AddMazeRelativeRotation',
            function: 'AddMazeRelativeRotation',
            entry: {
              kind: 'function',
              name: 'AddMazeRelativeRotation',
              node_path: '$.graphs[AddMazeRelativeRotation].nodes[0]',
              node_ref: 'nodes[0]',
            },
            nodes: [
              {
                node_ref: 'nodes[0]',
                kind: 'function',
                name: 'AddMazeRelativeRotation',
                links: [
                  {
                    link_ref: 'links[0]',
                    type: 'exec',
                    from_pin: 'then',
                    to_node: 'nodes[1]',
                    to_pin: 'execute',
                  },
                ],
              },
              {
                node_ref: 'nodes[1]',
                kind: 'call_function',
                name: 'SetRelativeRotation',
              },
            ],
          },
          stats: { nodes: 250, exec_links: 118, data_links: 141, orphan_nodes: 19 },
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
      asset_path: '/Game/Gameplay/Maze/BP_Maze',
      target_type: 'function',
      target_name: 'AddMazeRelativeRotation',
    },
    view: {
      format: 'logic_md',
    },
  });

  assert.deepEqual(calls, [
    {
      command: 'read_blueprint_logic_json',
      payload: {
        asset_path: '/Game/Gameplay/Maze/BP_Maze',
        function: 'AddMazeRelativeRotation',
        scope: 'target_function',
      },
    },
  ]);

  assert.equal(result.isError, false);
  const data = result.structuredContent?.data as Record<string, unknown>;
  const payload = data.payload as Record<string, unknown>;
  assert.equal(data.format, 'logic_md');
  assert.equal(payload.schema, 'LogicMd.v1');
  assert.equal(payload.scope, 'target_function');
  assert.deepEqual(payload.stats, { nodes: 2, exec_links: 1, data_links: 0, orphan_nodes: 0 });
  const markdown = String(payload.markdown);
  assert.match(markdown, /Function: AddMazeRelativeRotation/);
  assert.match(markdown, /SetRelativeRotation/);
  assert.doesNotMatch(markdown, /Nodes: 250/);
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

test('blueprinthelper_read_context summary returns compact metadata without LogicMd markdown', async () => {
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
        data: {
          schema: 'LogicJson.v1',
          scope: 'target_function',
          logic: {
            asset_path: '/Game/Gameplay/Maze/BP_Maze',
            graph: 'Graph',
            function: 'AddMazeRelativeRotation',
            entry: {
              kind: 'function',
              name: 'AddMazeRelativeRotation',
              node_path: 'Graph/node_1',
              node_ref: 'node_1',
            },
            nodes: Array.from({ length: 250 }, (_, index) => ({
              node_ref: `node_${index + 1}`,
              kind: 'call_function',
              name: `Node${index + 1}`,
            })),
          },
          markdown: '# Logic Graph\n\nThis must not leak through summary.',
          stats: { nodes: 250, exec_links: 118, data_links: 141, orphan_nodes: 19 },
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
      asset_path: '/Game/Gameplay/Maze/BP_Maze',
      target_type: 'function',
      target_name: 'AddMazeRelativeRotation',
    },
    view: {
      format: 'summary',
    },
  });

  assert.deepEqual(calls, [
    {
      command: 'read_blueprint_logic_json',
      payload: {
        asset_path: '/Game/Gameplay/Maze/BP_Maze',
        function: 'AddMazeRelativeRotation',
        scope: 'target_function',
      },
    },
  ]);
  assert.equal(result.isError, false);
  const data = result.structuredContent?.data as Record<string, unknown>;
  const payload = data.payload as Record<string, unknown>;
  assert.equal(data.format, 'summary');
  assert.equal(payload.schema, 'LogicSummary.v1');
  assert.equal(payload.target_found, true);
  assert.deepEqual(payload.stats, { nodes: 250, exec_links: 118, data_links: 141, orphan_nodes: 19 });
  assert.deepEqual(payload.entry, {
    kind: 'function',
    name: 'AddMazeRelativeRotation',
    node_path: 'Graph/node_1',
    node_ref: 'node_1',
  });
  assert.equal(Object.hasOwn(payload, 'markdown'), false);
  assert.equal(Object.hasOwn(payload, 'logic'), false);
  assert.equal(JSON.stringify(result).includes('This must not leak through summary'), false);
});

test('blueprinthelper_read_context logic_json honors max_items and marks truncation', async () => {
  const tools = registerWithBridge(async () => ({
    request_id: 'test',
    success: true,
    result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResultBase.v1',
      operation: 'read_blueprint_logic_json',
      status: 'completed',
      modified: false,
      data: {
        schema: 'LogicJson.v1',
        scope: 'target_function',
        logic: {
          asset_path: '/Game/Gameplay/Maze/BP_Maze',
          graph: 'Graph',
          function: 'AddMazeRelativeRotation',
          entry: {
            kind: 'function',
            name: 'AddMazeRelativeRotation',
            node_path: 'Graph/node_1',
            node_ref: 'node_1',
          },
          nodes: [
            { node_ref: 'node_1', kind: 'function', name: 'AddMazeRelativeRotation' },
            { node_ref: 'node_2', kind: 'call_function', name: 'SetRelativeRotation' },
            { node_ref: 'node_3', kind: 'call_function', name: 'PrintString' },
          ],
        },
        stats: { nodes: 3 },
      },
    },
  }));

  const tool = tools.get('blueprinthelper_read_context');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/Gameplay/Maze/BP_Maze',
      target_type: 'function',
      target_name: 'AddMazeRelativeRotation',
    },
    view: {
      format: 'logic_json',
      max_items: 2,
    },
  });

  assert.equal(result.isError, false);
  const data = result.structuredContent?.data as Record<string, unknown>;
  const payload = data.payload as Record<string, unknown>;
  const logic = payload.logic as Record<string, unknown>;
  assert.equal(data.truncated, true);
  assert.deepEqual(payload.truncation, { nodes_total: 3, nodes_returned: 2 });
  assert.equal((logic.nodes as unknown[]).length, 2);
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
  const fixturesDir = path.resolve(UE_PLUGIN_ROOT, 'Develop', 'TestFixtures', 'MCPRegression');
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
