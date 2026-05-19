import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import test from 'node:test';
import { successRead } from '../../result/tool-result.js';
import { ReadFunctionChainContextInputSchema } from '../../tool-surface/bridge/function-chain-context-schema.js';
import { ReadContextInputSchema } from '../../tool-surface/bridge/read-context/read-context-schemas.js';
import { getBlueprintHelperToolRegistry } from '../../tool-surface/tool-registry.js';
import type { TaskSpecRunner } from '../../task/service/task-spec-runner.js';

const expectedToolNames = [
  'blueprinthelper_read_task_context',
  'blueprinthelper_read_reference_context',
  'blueprinthelper_preview_task',
  'blueprinthelper_execute_task',
  'blueprinthelper_get_task_result',
  'blueprinthelper_read_agent_guide',
  'blueprinthelper_get_debug_case',
  'blueprinthelper_list_debug_cases',
  'blueprinthelper_export_debug_bundle',
  'blueprinthelper_query_review_records',
  'blueprinthelper_apply_review_action',
  'blueprinthelper_read_function_chain_context',
  'blueprinthelper_read_context_capabilities',
  'blueprinthelper_read_context',
  'blueprint_get_runtime_profile',
  'blueprinthelper_request_write_session',
  'blueprinthelper_diagnostics',
  'blueprinthelper_diagnostics_runtime',
  'blueprint_open_editor',
  'blueprint_close_editor',
];

const frozenToolNames = [
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
  'blueprint_play_in_editor',
  'blueprint_stop_pie',
  'blueprint_create_blueprint',
  'blueprint_exec_console_command',
  'blueprint_build_project',
];

test('shared registry covers only the current non-frozen CLI tool-name surface', () => {
  const registry = getBlueprintHelperToolRegistry();
  const names = registry.map((tool) => tool.name).sort();

  for (const expected of expectedToolNames) {
    assert.ok(names.includes(expected), expected);
  }
  assert.deepEqual(names, [...expectedToolNames].sort());
  assert.equal(new Set(names).size, names.length);
});

test('shared registry does not expose frozen direct tools', () => {
  const registry = getBlueprintHelperToolRegistry();
  const names = new Set(registry.map((tool) => tool.name));

  for (const name of frozenToolNames) {
    assert.equal(names.has(name), false, name);
  }
});

test('read agent guide resolves from task-core cwd through plugin ancestor layout', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_agent_guide');
  assert.ok(tool);

  const result = await tool.execute({}, {
    cwd: process.cwd(),
    bridge: {} as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  assert.equal(result.operation, 'blueprinthelper_read_agent_guide');
  assert.equal(result.data?.['schema'], 'AgentGuideMarkdown.v1');
  assert.equal(result.data?.['format'], 'markdown');
  assert.match(String(result.data?.['markdown'] ?? ''), /BlueprintHelper/i);
});

test('read agent guide resolves from project root AgentFaceService layout', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_agent_guide');
  assert.ok(tool);

  const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-agent-guide-'));
  try {
    const guideDir = path.join(tempRoot, 'AgentFaceService', 'agent-guide');
    fs.mkdirSync(guideDir, { recursive: true });
    fs.writeFileSync(
      path.join(guideDir, '00_Agent_Onboarding_Index_20260504.md'),
      '# BlueprintHelper AgentGuide\n\nFixture guide.',
      'utf8',
    );

    const result = await tool.execute({}, {
      cwd: tempRoot,
      bridge: {} as never,
      taskRunner: {} as TaskSpecRunner,
    });

    assert.equal(result.ok, true);
    assert.equal(result.data?.['schema'], 'AgentGuideMarkdown.v1');
    assert.equal(Object.hasOwn(result.data ?? {}, 'path'), false);
    assert.match(String(result.data?.['markdown'] ?? ''), /Fixture guide/);
  } finally {
    fs.rmSync(tempRoot, { recursive: true, force: true });
  }
});

test('agent-facing sanitizer removes UE GUID and review target selector fields recursively', () => {
  const result = successRead('fixture_read', { target_type: 'asset', asset_path: '/Game/BP_Door' }, {
    schema: 'Fixture.v1',
    task_run_id: 'task_opaque_id_is_allowed',
    review_record_id: 'review_opaque_id_is_allowed',
    payload: {
      node_guid: '11111111111111111111111111111111',
      target_guid: '22222222222222222222222222222222',
      node_ref: 'nodes[0]',
      nested: {
        guid: '33333333333333333333333333333333',
        guidance: 'ordinary guidance text is not a GUID field',
        target_keys: ['graph:EventGraph:node:44444444444444444444444444444444'],
        target_key: 'graph:EventGraph:node:55555555555555555555555555555555',
        atomic_targets: [{ node_guid: '66666666666666666666666666666666' }],
        visual_group_key: 'EventGraph:node:77777777777777777777777777777777',
        safe_label: 'Door setup',
      },
    },
  });

  assertNoUnsafeAgentFacingKeys(result);
  assert.equal(result.data?.['task_run_id'], 'task_opaque_id_is_allowed');
  assert.equal(result.data?.['review_record_id'], 'review_opaque_id_is_allowed');

  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['node_ref'], 'nodes[0]');
  assert.equal((payload['nested'] as Record<string, unknown>)['safe_label'], 'Door setup');
  assert.equal((payload['nested'] as Record<string, unknown>)['guidance'], 'ordinary guidance text is not a GUID field');
});

test('read_context registry output strips GUID fields from Bridge logic_json payload', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Door',
      target_type: 'graph',
      target_name: 'EventGraph',
    },
    view: {
      format: 'logic_json',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand(command: string) {
        assert.equal(command, 'read_blueprint_logic_json');
        return {
          success: true,
          request_id: 'read_context_redaction',
          result: {
            schema: 'LogicJson.v1',
            format: 'logic_json',
            scope: 'target_graph',
            logic: {
              asset_path: '/Game/BP_Door',
              graph: 'EventGraph',
              nodes: [{
                id: 'Node_0',
                node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                ref: 'BeginPlay',
                node_ref: 'nodes[0]',
              }],
              links: [{
                link_ref: 'links[0]',
                target_key: 'graph:EventGraph:node:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
              }],
            },
            stats: {
              nodes: 1,
              exec_links: 0,
            },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  assertNoUnsafeAgentFacingKeys(result);
  assert.match(JSON.stringify(result), /nodes\[0\]/);
  const data = result.data as Record<string, unknown>;
  const payload = data['payload'] as Record<string, unknown>;
  const logic = payload['logic'] as Record<string, unknown>;
  assert.equal(Object.hasOwn(data, 'read_type'), false);
  assert.equal(Object.hasOwn(data, 'format'), false);
  assert.equal(Object.hasOwn(data, 'scope'), false);
  assert.equal(Object.hasOwn(data, 'stats'), false);
  assert.equal(payload['schema'], 'LogicJson.v1');
  assert.equal(payload['scope'], 'target_graph');
  assert.deepEqual(payload['stats'], { nodes: 1, exec_links: 0 });
  assert.equal(Object.hasOwn(payload, 'format'), false);
  assert.equal(Object.hasOwn(logic, 'asset_path'), false);
});

test('read_context LogicMD payload keeps stats structured and strips duplicate markdown stats lines', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Door',
      target_type: 'graph',
      target_name: 'EventGraph',
    },
    view: {
      format: 'logic_md',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand(command: string) {
        assert.equal(command, 'read_blueprint_logic_md');
        return {
          success: true,
          request_id: 'read_context_logic_md_compact',
          result: {
            schema: 'LogicMd.v1',
            format: 'logic_md',
            markdown: [
              '# Logic Graph',
              '',
              'Nodes: 3 | Exec Links: 1 | Data Links: 2 | Entry Points: 1 | Orphans: 0',
              '',
              '## Graph: EventGraph',
              'Nodes: 3 | Exec Links: 1 | Data Links: 2 | Entry Points: 1 | Orphans: 0',
              '',
              '### Entry Points',
              '- nodes[0] BeginPlay',
            ].join('\n'),
            stats: {
              nodes: 3,
              exec_links: 1,
              data_links: 2,
              orphan_nodes: 0,
              events: 1,
            },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  const data = result.data as Record<string, unknown>;
  const payload = data['payload'] as Record<string, unknown>;
  const markdown = String(payload['markdown'] ?? '');
  assert.equal(payload['schema'], 'LogicMd.v1');
  assert.deepEqual(payload['stats'], {
    nodes: 3,
    exec_links: 1,
    data_links: 2,
    orphan_nodes: 0,
    events: 1,
  });
  assert.doesNotMatch(markdown, /^Nodes: 3 \| Exec Links: 1/m);
  assert.match(markdown, /## Graph: EventGraph/);
  assert.match(markdown, /### Entry Points/);
  assert.equal(Object.hasOwn(payload, 'format'), false);
});

test('read_context logic_flow returns execflow from structured logic_json payload', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Player',
      target_type: 'event',
      target_name: 'Secondary Thumbstick',
    },
    view: {
      format: 'logic_flow',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand(command: string) {
        assert.equal(command, 'read_blueprint_logic_json');
        return {
          success: true,
          request_id: 'read_context_logic_flow_exec',
          result: {
            schema: 'LogicJson.v1',
            logic: {
              graph: 'EventGraph',
              entry: { node_ref: 'nodes[0]', name: '事件Secondary Thumbstick' },
              nodes: [
                { node_ref: 'nodes[0]', kind: 'event', name: '事件Secondary Thumbstick' },
                { node_ref: 'nodes[1]', kind: 'function_call', name: 'DoLook' },
              ],
              links: [
                { type: 'exec', from_node: 'nodes[0]', from_pin: 'then', to_node: 'nodes[1]', to_pin: 'execute' },
                { type: 'data', from_node: 'nodes[0]', from_pin: 'Axis_X', to_node: 'nodes[1]', to_pin: 'Yaw' },
                { type: 'data', from_node: 'nodes[0]', from_pin: 'Axis_Y', to_node: 'nodes[1]', to_pin: 'Pitch' },
              ],
            },
            stats: {
              nodes: 2,
              exec_links: 1,
              data_links: 2,
              orphan_nodes: 0,
            },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  const payload = (result.data as Record<string, unknown>)['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'execflow');
  assert.equal(payload['flow'], '事件Secondary Thumbstick(Axis_X,Axis_Y) -> DoLook[Yaw=&.Axis_X, Pitch=&.Axis_Y]');
  assert.deepEqual(payload['stats'], {
    nodes: 2,
    exec_links: 1,
    data_links: 2,
    orphan_nodes: 0,
  });
  assert.deepEqual(payload['warnings'], []);
});

test('read_context logic_flow returns dataflow when structured logic has no exec links', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Math',
      target_type: 'function',
      target_name: 'ComputeOffset',
    },
    view: {
      format: 'logic_flow',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand(command: string) {
        assert.equal(command, 'read_blueprint_logic_json');
        return {
          success: true,
          request_id: 'read_context_logic_flow_data',
          result: {
            schema: 'LogicJson.v1',
            logic: {
              graph: 'ComputeOffset',
              nodes: [
                { node_ref: 'nodes[0]', name: 'GetActorLocation' },
                { node_ref: 'nodes[1]', name: 'GetVelocity' },
                { node_ref: 'nodes[2]', name: '*' },
                { node_ref: 'nodes[3]', name: '+' },
                { node_ref: 'nodes[4]', name: 'ReturnValue' },
              ],
              links: [
                { type: 'data', from_node: 'nodes[1]', from_pin: 'ReturnValue', to_node: 'nodes[2]', to_pin: 'A' },
                { type: 'data', from_node: 'nodes[2]', from_pin: 'ReturnValue', to_node: 'nodes[3]', to_pin: 'B' },
                { type: 'data', from_node: 'nodes[0]', from_pin: 'ReturnValue', to_node: 'nodes[3]', to_pin: 'A' },
                { type: 'data', from_node: 'nodes[3]', from_pin: 'ReturnValue', to_node: 'nodes[4]', to_pin: 'ReturnValue' },
              ],
            },
            stats: {
              nodes: 5,
              exec_links: 0,
              data_links: 4,
              orphan_nodes: 0,
            },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  const payload = (result.data as Record<string, unknown>)['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'dataflow');
  assert.match(String(payload['flow']), /^dataflow:/);
  assert.match(String(payload['flow']), /\$p0 = GetActorLocation/);
  assert.match(String(payload['flow']), /ReturnValue = /);
});

test('read_context logic_flow does not expose raw LogicJson anchors or UE identity fields', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Door',
      target_type: 'event',
      target_name: 'BeginPlay',
    },
    view: {
      format: 'logic_flow',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand() {
        return {
          success: true,
          request_id: 'read_context_logic_flow_no_anchor',
          result: {
            schema: 'LogicJson.v1',
            logic: {
              asset_path: '/Game/BP_Door',
              graph: 'EventGraph',
              nodes: [
                {
                  node_ref: 'nodes[0]',
                  node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                  name: 'BeginPlay',
                },
                {
                  node_ref: 'nodes[1]',
                  node_path: '$.graphs.EventGraph.nodes[1]',
                  name: 'PrintString',
                },
              ],
              links: [
                {
                  link_ref: 'links[0]',
                  type: 'exec',
                  from_node: 'nodes[0]',
                  from_pin: 'then',
                  to_node: 'nodes[1]',
                  to_pin: 'execute',
                },
              ],
            },
            stats: { nodes: 2, exec_links: 1, data_links: 0 },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  assertNoUnsafeAgentFacingKeys(result);
  const serialized = JSON.stringify(result);
  assert.doesNotMatch(serialized, /node_path/);
  assert.doesNotMatch(serialized, /link_ref/);
  assert.doesNotMatch(serialized, /asset_path.*BP_Door.*logic/);
  assert.match(serialized, /BeginPlay -> PrintString/);
});

test('read_context logic_flow keeps multi-exec output pin names', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Door',
      target_type: 'event',
      target_name: 'Interact',
    },
    view: {
      format: 'logic_flow',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand() {
        return {
          success: true,
          request_id: 'read_context_logic_flow_branch',
          result: {
            schema: 'LogicJson.v1',
            logic: {
              nodes: [
                { node_ref: 'nodes[0]', kind: 'event', name: 'Interact' },
                { node_ref: 'nodes[1]', name: 'Branch' },
                { node_ref: 'nodes[2]', name: 'OpenDoor' },
                { node_ref: 'nodes[3]', name: 'CloseDoor' },
              ],
              links: [
                { type: 'exec', from_node: 'nodes[0]', from_pin: 'then', to_node: 'nodes[1]', to_pin: 'execute' },
                { type: 'exec', from_node: 'nodes[1]', from_pin: 'True', to_node: 'nodes[2]', to_pin: 'execute' },
                { type: 'exec', from_node: 'nodes[1]', from_pin: 'False', to_node: 'nodes[3]', to_pin: 'execute' },
                { type: 'data', from_node: 'nodes[0]', from_pin: 'bDoorOpen', to_node: 'nodes[1]', to_pin: 'Condition' },
              ],
            },
            stats: { nodes: 4, exec_links: 3, data_links: 1 },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  const payload = (result.data as Record<string, unknown>)['payload'] as Record<string, unknown>;
  assert.match(String(payload['flow']), /Interact\(bDoorOpen\) -> Branch\[Condition=&\.bDoorOpen\]/);
  assert.match(String(payload['flow']), /  True -> OpenDoor/);
  assert.match(String(payload['flow']), /  False -> CloseDoor/);
});

test('read_context asset summary removes payload path and name but keeps asset class', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'asset_context',
    target: {
      asset_path: '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter',
      target_type: 'blueprint',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand(command: string, payload?: Record<string, unknown>) {
        assert.equal(command, 'get_asset_info');
        assert.deepEqual(payload, {
          asset_path: '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter',
        });
        return {
          success: true,
          request_id: 'read_asset_context_compact',
          result: {
            schema: 'AssetContext.v1',
            path: '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter',
            name: 'BP_ThirdPersonCharacter',
            class: 'Blueprint',
            parent_class: "TemplateCharacter'",
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  assert.deepEqual(result.target, {
    asset_path: '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter',
    target_type: 'blueprint',
  });
  const data = result.data as Record<string, unknown>;
  const payload = data['payload'] as Record<string, unknown>;
  assert.equal(Object.hasOwn(data, 'read_type'), false);
  assert.equal(Object.hasOwn(data, 'format'), false);
  assert.equal(payload['schema'], 'AssetContext.v1');
  assert.equal(payload['class'], 'Blueprint');
  assert.equal(payload['parent_class'], "TemplateCharacter'");
  assert.equal(Object.hasOwn(payload, 'path'), false);
  assert.equal(Object.hasOwn(payload, 'name'), false);
});

test('apply_review_action is expert-only and sanitized when invoked through the registry', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_apply_review_action');
  assert.ok(tool);
  assert.equal(tool.audience, 'expert');
  assert.equal(tool.requiresExpert, true);

  const result = await tool.execute({
    review_record_id: 'review_opaque_id_is_allowed',
    action: 'reject',
    target_keys: ['graph:EventGraph:node:cccccccccccccccccccccccccccccccc'],
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand(command: string) {
        assert.equal(command, 'apply_review_action');
        return {
          success: true,
          request_id: 'review_action_redaction',
          result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: 'apply_review_action',
            status: 'applied',
            modified: true,
            data: {
              review_record_id: 'review_opaque_id_is_allowed',
              target_keys: ['graph:EventGraph:node:dddddddddddddddddddddddddddddddd'],
              target_guid: 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee',
            },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  assertNoUnsafeAgentFacingKeys(result);
  assert.equal(result.data?.['review_record_id'], 'review_opaque_id_is_allowed');
});

test('function chain context input schema accepts compact entry requests and rejects GUID selectors', () => {
  const parsed = ReadFunctionChainContextInputSchema.parse({
    asset_path: '/Game/BP_PlayerController',
    target_type: 'event',
    target_name: 'Input_Fire',
  });

  assert.equal(parsed.asset_path, '/Game/BP_PlayerController');
  assert.equal(parsed.target_type, 'event');
  assert.equal(parsed.target_name, 'Input_Fire');
  assert.equal(parsed.max_depth, 3);
  assert.equal(parsed.include_data_dependencies, true);
  assert.equal(parsed.expand_cross_asset, true);

  assert.throws(() => ReadFunctionChainContextInputSchema.parse({
    asset_path: '/Game/BP_PlayerController',
    target_type: 'event',
    target_name: 'Input_Fire',
    target_guid: '00000000000000000000000000000000',
  }), /Unrecognized key/);
});

test('read context capabilities is a compact local discovery tool', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context_capabilities');
  assert.ok(tool);

  const result = await tool.execute({}, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand() {
        throw new Error('read context capabilities must not call Bridge');
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  assert.equal(result.operation, 'read_context_capabilities');
  assert.equal(result.data?.['schema'], 'ReadContextCapabilities.v1');
  assert.deepEqual(result.data?.['formats'], ['logic_flow', 'logic_md', 'logic_json']);
  assert.deepEqual(result.data?.['read_type_ids'], [
    'asset_context',
    'blueprint_logic',
    'graph_context',
    'component_context',
    'variable_context',
    'widget_context',
    'data_table_context',
    'data_asset_context',
    'object_property_context',
  ]);

  const readTypes = result.data?.['read_types'] as Array<Record<string, unknown>>;
  const blueprintLogic = readTypes.find((entry) => entry['read_type'] === 'blueprint_logic');
  assert.ok(blueprintLogic);
  assert.deepEqual(blueprintLogic['unsupported_formats'], []);
  assert.deepEqual(blueprintLogic['unsupported_asset_types'], [
    'asset',
    'component',
    'member_variable',
    'event_dispatcher',
    'widget',
    'data_table',
    'data_table_row',
    'data_asset',
    'object_property',
    'property',
  ]);

  const assetContext = readTypes.find((entry) => entry['read_type'] === 'asset_context');
  assert.ok(assetContext);
  assert.deepEqual(assetContext['unsupported_formats'], ['logic_flow', 'logic_md', 'logic_json']);
  const graphContext = readTypes.find((entry) => entry['read_type'] === 'graph_context');
  assert.ok(graphContext);
  assert.deepEqual(graphContext['unsupported_formats'], ['logic_flow', 'logic_md']);
});

test('read_context rejects removed and unsupported view formats', () => {
  assert.doesNotThrow(() => ReadContextInputSchema.parse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Player',
      target_type: 'event',
      target_name: 'Input_Fire',
    },
    view: {
      format: 'logic_flow',
    },
  }));

  assert.throws(() => ReadContextInputSchema.parse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'asset_context',
    target: {
      asset_path: '/Game/BP_Player',
      target_type: 'asset',
    },
    view: {
      format: 'schema',
    },
  }), /Invalid enum value/);

  assert.throws(() => ReadContextInputSchema.parse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Player',
      target_type: 'blueprint',
    },
    view: {
      format: 'summary',
    },
  }), /Invalid enum value/);

  assert.throws(() => ReadContextInputSchema.parse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'asset_context',
    target: {
      asset_path: '/Game/BP_Player',
      target_type: 'asset',
    },
    view: {
      format: 'logic_json',
    },
  }), /view\.format is only supported/);

  assert.throws(() => ReadContextInputSchema.parse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'graph_context',
    target: {
      asset_path: '/Game/BP_Player',
      target_type: 'graph',
      target_name: 'EventGraph',
    },
    view: {
      format: 'logic_md',
    },
  }), /graph_context only supports logic_json/);

  assert.throws(() => ReadContextInputSchema.parse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'graph_context',
    target: {
      asset_path: '/Game/BP_Player',
      target_type: 'graph',
      target_name: 'EventGraph',
    },
    view: {
      format: 'logic_flow',
    },
  }), /graph_context only supports logic_json/);
});

test('function chain context registry dispatch strips forbidden identity fields from Bridge payload', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_function_chain_context');
  assert.ok(tool);

  const result = await tool.execute({
    asset_path: '/Game/BP_PlayerController',
    target_type: 'event',
    target_name: 'Input_Fire',
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand(command: string, payload?: Record<string, unknown>) {
        assert.equal(command, 'read_function_chain_context');
        assert.equal(payload?.['asset_path'], '/Game/BP_PlayerController');
        assert.equal(payload?.['target_type'], 'event');
        assert.equal(payload?.['target_name'], 'Input_Fire');
        return {
          success: true,
          request_id: 'req_function_chain',
          result: successRead('read_function_chain_context', { target_type: 'asset' }, {
            schema: 'BlueprintHelper.FunctionChainContext.v1',
            entry: { asset_path: '/Game/BP_PlayerController' },
            target: { asset_path: '/Game/BP_PlayerController' },
            query: { target_name: 'Input_Fire' },
            custom_logic_refs: [
              {
                order: 1,
                depth: 1,
                parent_order: 0,
                asset_path: '/Game/BP_Weapon',
                owner_asset_path: '/Game/BP_Weapon',
                target_type: 'function',
                target_name: 'CanFire',
                graph_name: 'CanFire',
                call_kind: 'pure_function',
                reason: 'branch_condition',
                node_guid: '00000000000000000000000000000000',
                node_ref: 'K2Node_CallFunction_0',
                node_path: 'EventGraph.K2Node_CallFunction_0',
              },
            ],
            summary: {
              returned_custom_refs: 1,
            },
          }) as unknown as Record<string, unknown>,
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  const data = result.data as Record<string, unknown>;
  assertNoUnsafeAgentFacingKeys(data);
  assert.equal(data['schema'], 'FunctionChainContext.v1');
  assert.equal(Array.isArray(data['custom_logic_refs']), true);
  const refs = data['custom_logic_refs'] as Array<Record<string, unknown>>;
  assert.equal(refs[0]?.['call_kind'], 'pure_function');
  assert.equal(refs[0]?.['reason'], 'branch_condition');
  assert.equal(Object.hasOwn(refs[0] ?? {}, 'node_ref'), false);
  assert.equal(Object.hasOwn(refs[0] ?? {}, 'node_path'), false);
});

test('preview task registry handler calls TaskSpecRunner.previewTask', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_preview_task');
  assert.ok(tool);
  let called = false;
  const runner = {
    previewTask: async () => {
      called = true;
      return {
        previewId: 'preview_registry_001',
        taskPlan: {
          schema: 'BlueprintHelper.TaskPlan.v1',
          task_name: 'RegistryPreview',
          task_type: 'edit_blueprint_graph',
          context_id: 'ctx_registry',
          target_assets: ['/Game/BP_Player'],
          execution_policy: { dry_run_mode: 'full', should_compile: true, should_save: false, review_baseline_dirty_asset_policy: 'block' },
          steps: [],
        },
        previewToken: {
          preview_id: 'preview_registry_001',
          task_plan_hash: 'task_plan_hash_registry',
          task_spec_hash: 'task_spec_hash_registry',
          execution_policy_hash: 'execution_policy_hash_registry',
          created_at: '2026-05-19T00:00:00.000Z',
        },
        passed: true,
        issues: [],
        toolResult: {
          ok: true,
          schema: 'BlueprintHelper.ToolResult.v1',
          operation: 'preview_task',
          trace_id: 'trace_registry',
          status: 'dry_run',
          modified: false,
        },
      };
    },
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const result = await tool.execute({
    task_spec: {
      schema: 'BlueprintHelper.TaskSpec.v1',
      context_id: 'ctx_registry',
      task_type: 'edit_blueprint_graph',
      feature_name: 'RegistryPreview',
      target: { asset_path: '/Game/BP_Player', target_type: 'blueprint' },
      scope_policy: { graph_name: 'EventGraph', allow_modify_user_nodes: false },
      behavior: {
        graph_strategy: 'append_new_owned_graph',
        entries: [{
          entry_type: 'custom_event',
          name: 'RegistryPreview',
          body: { schema: 'BlueprintLogicSpec.v1', statements: [] },
        }],
      },
      execution_policy: {
        dry_run_mode: 'full',
        on_missing_capability: 'stop_and_report',
      },
      validation: { should_compile: true, should_save: false },
    },
  }, {
    cwd: process.cwd(),
    bridge: {} as never,
    taskRunner: runner,
  });

  assert.equal(called, true);
  assert.equal(result.operation, 'preview_task');
});

test('execute task registry handler passes preview token to TaskSpecRunner', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_execute_task');
  assert.ok(tool);
  const previewToken = {
    preview_id: 'preview_registry_execute_001',
    task_plan_hash: 'task_plan_hash_registry_execute',
    task_spec_hash: 'task_spec_hash_registry_execute',
    execution_policy_hash: 'execution_policy_hash_registry_execute',
    created_at: '2026-05-19T00:00:00.000Z',
  };
  let receivedPreviewToken: unknown;
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
    executeTask: async (_taskSpec: unknown, _timing: unknown, options?: { previewToken?: unknown }) => {
      receivedPreviewToken = options?.previewToken;
      return {
        ok: true,
        schema: 'BlueprintHelper.ToolResult.v1',
        operation: 'execute_task',
        trace_id: 'trace_registry_execute',
        status: 'completed',
        modified: false,
      };
    },
    getTaskResult: async () => { throw new Error('not used'); },
  } as unknown as TaskSpecRunner;

  await tool.execute({
    task_spec: {
      schema: 'BlueprintHelper.TaskSpec.v1',
      context_id: 'ctx_registry_execute',
      task_type: 'edit_blueprint_graph',
      feature_name: 'RegistryExecute',
      target: { asset_path: '/Game/BP_Player', target_type: 'blueprint' },
      scope_policy: { graph_name: 'EventGraph', allow_modify_user_nodes: false },
      behavior: {
        graph_strategy: 'append_new_owned_graph',
        entries: [{
          entry_type: 'custom_event',
          name: 'RegistryExecute',
          body: { schema: 'BlueprintLogicSpec.v1', statements: [] },
        }],
      },
      execution_policy: {
        dry_run_mode: 'full',
        on_missing_capability: 'stop_and_report',
      },
      validation: { should_compile: true, should_save: false },
    },
    preview_token: previewToken,
  }, {
    cwd: process.cwd(),
    bridge: {} as never,
    taskRunner: runner,
  });

  assert.deepEqual(receivedPreviewToken, previewToken);
});

test('execute task registry handler rejects direct TaskSpec preview token', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_execute_task');
  assert.ok(tool);
  const previewToken = {
    preview_id: 'preview_registry_execute_direct_001',
    task_plan_hash: 'task_plan_hash_registry_execute_direct',
    task_spec_hash: 'task_spec_hash_registry_execute_direct',
    execution_policy_hash: 'execution_policy_hash_registry_execute_direct',
    created_at: '2026-05-19T00:00:00.000Z',
  };
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('direct TaskSpec preview_token should be rejected before runner'); },
    getTaskResult: async () => { throw new Error('not used'); },
  } as unknown as TaskSpecRunner;

  const result = await tool.execute({
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_registry_execute_direct',
    task_type: 'edit_blueprint_graph',
    feature_name: 'RegistryExecuteDirect',
    target: { asset_path: '/Game/BP_Player', target_type: 'blueprint' },
    scope_policy: { graph_name: 'EventGraph', allow_modify_user_nodes: false },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'RegistryExecuteDirect',
        body: { schema: 'BlueprintLogicSpec.v1', statements: [] },
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: { should_compile: true, should_save: false },
    preview_token: previewToken,
  }, {
    cwd: process.cwd(),
    bridge: {} as never,
    taskRunner: runner,
  });

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'preview_token_requires_task_spec_wrapper');
});

function assertNoUnsafeAgentFacingKeys(value: unknown): void {
  const violations = collectUnsafeAgentFacingKeys(value);
  assert.deepEqual(violations, []);
}

function collectUnsafeAgentFacingKeys(value: unknown, currentPath = '$'): string[] {
  if (Array.isArray(value)) {
    return value.flatMap((item, index) => collectUnsafeAgentFacingKeys(item, `${currentPath}[${index}]`));
  }
  if (!isRecord(value)) {
    return [];
  }

  return Object.entries(value).flatMap(([key, entryValue]) => {
    const normalized = key.toLowerCase();
    const nextPath = `${currentPath}.${key}`;
    const selfViolation = isUnsafeAgentFacingKey(normalized)
      ? [nextPath]
      : [];
    return [
      ...selfViolation,
      ...collectUnsafeAgentFacingKeys(entryValue, nextPath),
    ];
  });
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function isUnsafeAgentFacingKey(normalized: string): boolean {
  const tokenized = normalized.replace(/[^a-z0-9]+/g, '_');
  const compact = normalized.replace(/[^a-z0-9]+/g, '');
  return tokenized === 'guid'
    || tokenized === 'guids'
    || tokenized.startsWith('guid_')
    || tokenized.startsWith('guids_')
    || tokenized.endsWith('_guid')
    || tokenized.endsWith('_guids')
    || tokenized.includes('_guid_')
    || tokenized.includes('_guids_')
    || compact.endsWith('guid')
    || compact.endsWith('guids')
    || normalized === 'target_key'
    || normalized === 'target_keys'
    || normalized === 'atomic_targets'
    || normalized === 'visual_group_key';
}

