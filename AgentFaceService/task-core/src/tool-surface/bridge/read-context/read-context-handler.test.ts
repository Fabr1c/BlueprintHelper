import assert from 'node:assert/strict';
import test from 'node:test';

import type { BridgeResponse } from '../../../bridge/bridge-client.js';
import { TaskTimingTrace } from '../../../task/service/task-timing.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { executeReadContext } from './read-context-handler.js';
import { buildLogicFlowPayload } from './read-context-logic-flow.js';
import {
  LOGIC_PROJECTION_CALLBACK_CAPABILITIES,
  LOGIC_PROJECTION_OWNER,
  READ_CONTEXT_LOGIC_FORMATS,
} from './read-context-schemas.js';

const nodeAnchor = {
  schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
  asset_path: '/Game/BP_Test',
  graph_name: 'EventGraph',
  node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
  node_class: '/Script/BlueprintGraph.K2Node_CustomEvent',
  semantic_role: 'node',
  fingerprint: 'nodefp',
};

const boundaryAnchor = {
  schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
  asset_path: '/Game/BP_Test',
  graph_name: 'EventGraph',
  node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
  node_class: '/Script/BlueprintGraph.K2Node_CustomEvent',
  pin_name: 'then',
  pin_direction: 'output',
  semantic_role: 'exec_boundary',
  fingerprint: 'boundaryfp',
};

const duplicateBoundaryAnchor = {
  ...boundaryAnchor,
};

test('read_context logic formats declare UE callback capabilities with task-core projection owner', () => {
  assert.deepEqual(
    READ_CONTEXT_LOGIC_FORMATS,
    ['logic_flow', 'logic_md', 'logic_json'],
  );
  assert.deepEqual(
    LOGIC_PROJECTION_CALLBACK_CAPABILITIES,
    [
      'ue.raw_snapshot.logic_json',
      'ue.raw_snapshot.logic_md',
      'ue.raw_snapshot.logic_flow',
    ],
  );
  assert.equal(LOGIC_PROJECTION_OWNER, 'task-core');
});

function makeLogicJsonWithExternalAnchors(): Record<string, unknown> {
  return {
    schema: 'LogicJson.v1',
    format: 'logic_json',
    importable: false,
    scope: 'blueprint',
    logic: {
      asset_path: '/Game/BP_Test',
      graph: 'EventGraph',
      nodes: [
        {
          node_ref: 'nodes[0]',
          kind: 'custom_event',
          name: 'OpenDoor',
          external_anchor: nodeAnchor,
          external_anchors: [boundaryAnchor],
          links: [
            {
              link_ref: 'links[0]',
              type: 'exec',
              from_pin: 'then',
              to_node: 'nodes[1]',
              to_pin: 'execute',
              external_anchor: duplicateBoundaryAnchor,
            },
          ],
        },
        {
          node_ref: 'nodes[1]',
          kind: 'call_function',
          name: 'PrintString',
        },
      ],
    },
    stats: {
      nodes: 2,
      exec_links: 1,
      data_links: 0,
      orphan_nodes: 0,
    },
  };
}

function makeFunctionLogicJsonWithSyntheticBoundaries(): Record<string, unknown> {
  return {
    schema: 'LogicJson.v1',
    format: 'logic_json',
    importable: false,
    scope: 'target_function',
    logic: {
      asset_path: '/Game/Gameplay/Maze/BP_Maze',
      graph: 'AddMazeRelativeRotation',
      function: 'AddMazeRelativeRotation',
      entry: {
        kind: 'function',
        name: 'AddMazeRelativeRotation',
        node_path: '$.graphs[AddMazeRelativeRotation].FunctionEntry',
        node_ref: 'FunctionEntry',
      },
      nodes: [
        {
          node_ref: 'FunctionEntry',
          kind: 'function',
          name: 'AddMazeRelativeRotation',
          links: [
            {
              link_ref: 'links[0]',
              type: 'exec',
              from_pin: 'then',
              to_node: 'nodes[0]',
              to_pin: 'execute',
            },
          ],
        },
        {
          node_ref: 'nodes[0]',
          kind: 'call_function',
          name: 'SetRelativeRotation',
          links: [
            {
              link_ref: 'links[1]',
              type: 'exec',
              from_pin: 'then',
              to_node: 'FunctionResult',
              to_pin: 'execute',
            },
          ],
        },
        {
          node_ref: 'FunctionResult',
          kind: 'return',
          name: 'Return',
        },
      ],
    },
    stats: {
      nodes: 3,
      exec_links: 2,
      data_links: 0,
      orphan_nodes: 0,
    },
  };
}

function makeLogicJsonWithUnknownLink(): Record<string, unknown> {
  return {
    schema: 'LogicJson.v1',
    format: 'logic_json',
    importable: false,
    scope: 'blueprint',
    logic: {
      asset_path: '/Game/BP_Test',
      graph: 'EventGraph',
      nodes: [
        { node_ref: 'nodes[0]', kind: 'event', name: 'BeginPlay' },
        { node_ref: 'nodes[1]', kind: 'call_function', name: 'PrintString' },
      ],
      links: [
        {
          link_ref: 'links[0]',
          from_node: 'nodes[0]',
          from_pin: 'then',
          to_node: 'nodes[1]',
          to_pin: 'execute',
        },
      ],
    },
    stats: {
      nodes: 2,
      exec_links: 0,
      data_links: 0,
      links: 1,
    },
  };
}

test('logic_flow payload omits anchors by default and keeps them in debug metadata', () => {
  const result = buildLogicFlowPayload(makeLogicJsonWithExternalAnchors());

  assert.equal(result.payload['schema'], 'LogicFlow.v1');
  assert.equal(result.payload['mode'], 'execflow');
  assert.equal(result.payload['anchors'], undefined);
  const anchors = result.debug?.['anchors'] as Record<string, unknown>[];
  assert.ok(Array.isArray(anchors));
  assert.equal(anchors.length, 2);
  assert.equal(anchors[0]?.['semantic_role'], 'exec_boundary');
  assert.equal(anchors[1]?.['semantic_role'], 'node');
  assert.equal(anchors[0]?.['fingerprint'], 'boundaryfp');
  assert.equal(anchors[1]?.['fingerprint'], 'nodefp');
});

test('logic_flow payload renders function graph synthetic entry and result boundaries', () => {
  const result = buildLogicFlowPayload(makeFunctionLogicJsonWithSyntheticBoundaries());
  const payload = result.payload;

  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'execflow');
  assert.equal((payload['warnings'] as string[]).length, 0);
  assert.equal(payload['flow'], 'AddMazeRelativeRotation -> SetRelativeRotation -> Return');
});

test('logic_flow uses adapter boundary projection instead of local function guessing', () => {
  const result = buildLogicFlowPayload({
    schema: 'LogicJson.v1',
    adapter_boundary: {
      runtime_adapter_id: 'k2.function_body',
      entry_boundaries: [{ node_ref: 'FunctionEntry', display_name: 'AddScore' }],
      exit_boundaries: [{ node_ref: 'FunctionResult', display_name: 'Return' }],
      folded_boundary_node_refs: ['FunctionEntry'],
      visible_boundary_node_refs: ['FunctionResult'],
    },
    logic: {
      nodes: [
        { node_ref: 'FunctionEntry', name: 'Entry', kind: 'function_entry' },
        { node_ref: 'SetScore', name: 'Set Score', kind: 'call' },
        { node_ref: 'FunctionResult', name: 'Return', kind: 'return' },
      ],
      links: [
        { type: 'exec', from_node: 'FunctionEntry', from_pin: 'then', to_node: 'SetScore', to_pin: 'execute' },
        { type: 'exec', from_node: 'SetScore', from_pin: 'then', to_node: 'FunctionResult', to_pin: 'execute' },
      ],
    },
  });

  assert.equal(result.payload['flow'], 'AddScore -> Set Score -> Return');
});

test('logic_flow renders macro tunnel boundaries from adapter projection', () => {
  const result = buildLogicFlowPayload({
    schema: 'LogicJson.v1',
    adapter_boundary: {
      runtime_adapter_id: 'k2.macro_body',
      entry_boundaries: [{ node_ref: 'TunnelEntry', display_name: 'Macro In' }],
      exit_boundaries: [{ node_ref: 'TunnelExit', display_name: 'Macro Out' }],
      visible_boundary_node_refs: ['TunnelEntry', 'TunnelExit'],
    },
    logic: {
      nodes: [
        { node_ref: 'TunnelEntry', name: 'Tunnel Entry', kind: 'macro_entry' },
        { node_ref: 'Clamp', name: 'Clamp', kind: 'call' },
        { node_ref: 'TunnelExit', name: 'Tunnel Exit', kind: 'macro_exit' },
      ],
      links: [
        { type: 'exec', from_node: 'TunnelEntry', from_pin: 'then', to_node: 'Clamp', to_pin: 'execute' },
        { type: 'exec', from_node: 'Clamp', from_pin: 'then', to_node: 'TunnelExit', to_pin: 'execute' },
      ],
    },
  });

  assert.equal(result.payload['flow'], 'Macro In -> Clamp -> Macro Out');
});

test('logic_flow payload degrades to logic_json when links are unknown', () => {
  const result = buildLogicFlowPayload(makeLogicJsonWithUnknownLink());

  assert.equal(result.payload['schema'], 'LogicJson.v1');
  assert.equal(result.payload['format'], 'logic_json');
  assert.equal(result.payload['requested_format'], 'logic_flow');
  assert.equal(result.payload['mode'], undefined);
  assert.equal(result.payload['flow'], undefined);
  assert.deepEqual(result.payload['warnings'], ['logic_flow_degraded_unknown_link']);
  assert.equal(result.debug?.['degraded'], true);
  assert.equal(result.debug?.['reason'], 'unknown_link');
});

test('read_context variable filter preserves member variable metadata fields', async () => {
  const bridgeResponse: BridgeResponse = {
    request_id: 'variable_metadata',
    success: true,
    result: {
      ok: true,
      data: {
        schema: 'ReadMemberVariables.v1',
        member_variables: [
          {
            variable_name: 'DoorPrompt',
            variable_type: { category: 'string', container: 'single' },
            category: 'Door',
            tooltip: 'Prompt displayed near the door.',
            instance_editable: true,
            expose_on_spawn: true,
          },
          {
            variable_name: 'InternalCounter',
            variable_type: { category: 'int', container: 'single' },
            instance_editable: false,
            expose_on_spawn: false,
          },
        ],
      },
    },
  };

  const bridgeCalls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const context: BlueprintHelperToolContext = {
    cwd: process.cwd(),
    bridge: {
      sendCommand: async (command: string, payload?: Record<string, unknown>) => {
        bridgeCalls.push({ command, payload });
        return bridgeResponse;
      },
    } as never,
    taskRunner: {} as never,
  };

  const result = await executeReadContext({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'variable_context',
    target: {
      asset_path: '/Game/BP_Door',
      target_type: 'member_variable',
      target_name: 'DoorPrompt',
    },
  }, context);

  assert.equal(result.ok, true);
  assert.equal(bridgeCalls[0]?.command, 'list_variables');

  const payload = result.data?.['payload'] as Record<string, unknown>;
  const variables = payload['member_variables'] as Record<string, unknown>[];
  assert.equal(variables.length, 1);
  assert.equal(variables[0]?.['variable_name'], 'DoorPrompt');
  assert.equal(variables[0]?.['category'], 'Door');
  assert.equal(variables[0]?.['tooltip'], 'Prompt displayed near the door.');
  assert.equal(variables[0]?.['instance_editable'], true);
  assert.equal(variables[0]?.['expose_on_spawn'], true);
});

test('read_context component filter preserves component readback facts', async () => {
  const bridgeResponse: BridgeResponse = {
    request_id: 'component_facts',
    success: true,
    result: {
      ok: true,
      data: {
        schema: 'BlueprintComponent.v1',
        components: [
          {
            component_name: 'DoorMesh',
            component_class: 'StaticMeshComponent',
            class_path: '/Script/Engine.StaticMeshComponent',
            component_template_path: '/Game/BP_Door.BP_Door_C:DoorMesh',
            component_id: '/Game/BP_Door.BP_Door::SCS::DoorMesh',
            parent: 'DoorRoot',
            children: ['DoorHandle'],
            selected_defaults: {
              mobility: 'movable',
            },
            readback_revision: 'BlueprintComponentFacts.v1',
            readback_fingerprint: 'abcdef0123456789',
            is_owned_scs: true,
            can_reparent: true,
          },
          {
            component_name: 'OtherMesh',
            component_id: '/Game/BP_Door.BP_Door::SCS::OtherMesh',
          },
        ],
      },
    },
  };

  const bridgeCalls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const context: BlueprintHelperToolContext = {
    cwd: process.cwd(),
    bridge: {
      sendCommand: async (command: string, payload?: Record<string, unknown>) => {
        bridgeCalls.push({ command, payload });
        return bridgeResponse;
      },
    } as never,
    taskRunner: {} as never,
  };

  const result = await executeReadContext({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'component_context',
    target: {
      asset_path: '/Game/BP_Door',
      target_type: 'component',
      target_name: 'DoorMesh',
    },
  }, context);

  assert.equal(result.ok, true);
  assert.equal(bridgeCalls[0]?.command, 'read_components');

  const payload = result.data?.['payload'] as Record<string, unknown>;
  const components = payload['components'] as Record<string, unknown>[];
  assert.equal(components.length, 1);
  assert.equal(components[0]?.['component_name'], 'DoorMesh');
  assert.equal(components[0]?.['component_template_path'], '/Game/BP_Door.BP_Door_C:DoorMesh');
  assert.equal(components[0]?.['component_id'], '/Game/BP_Door.BP_Door::SCS::DoorMesh');
  assert.equal(components[0]?.['parent'], 'DoorRoot');
  assert.deepEqual(components[0]?.['children'], ['DoorHandle']);
  assert.deepEqual(components[0]?.['selected_defaults'], { mobility: 'movable' });
  assert.equal(components[0]?.['readback_fingerprint'], 'abcdef0123456789');
  assert.equal(components[0]?.['can_reparent'], true);
});

test('read_context handler records timing around bridge and post-processing stages', async () => {
  const timing = TaskTimingTrace.start('read_context_test', 'agentface_test');
  const bridgeResponse: BridgeResponse = {
    request_id: 'test',
    success: true,
    result: {
      schema: 'LogicJson.v1',
      format: 'logic_json',
      importable: false,
      scope: 'blueprint',
      logic: {
        asset_path: '/Game/BP_Test',
        groups: [],
      },
      stats: {
        nodes: 0,
        exec_links: 0,
        data_links: 0,
        orphan_nodes: 0,
      },
      timing: {
        schema: 'BlueprintHelper.TimingTrace.v1',
        source: 'ue_bridge_router',
        operation: 'read_blueprint_logic_json',
        timing_id: 'ue_test',
        total_ms: 1,
        stages: [{ name: 'route_execute', started_at_ms: 0, duration_ms: 1 }],
      },
    },
  };

  const bridgeCalls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const context: BlueprintHelperToolContext = {
    cwd: process.cwd(),
    bridge: {
      sendCommand: async (command: string, payload?: Record<string, unknown>) => {
        bridgeCalls.push({ command, payload });
        return bridgeResponse;
      },
    } as never,
    taskRunner: {} as never,
    timing,
  };

  const result = await executeReadContext({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Test',
      target_type: 'function',
      target_name: 'ComputeValue',
    },
    view: {
      format: 'logic_json',
    },
  }, context);

  assert.equal(result.ok, true);
  assert.equal(bridgeCalls[0]?.command, 'read_blueprint_logic_json');
  assert.equal(bridgeCalls[0]?.payload?.['target_type'], 'function');
  assert.equal(bridgeCalls[0]?.payload?.['target_name'], 'ComputeValue');
  assert.equal(bridgeCalls[0]?.payload?.['function'], 'ComputeValue');
  assert.equal(bridgeCalls[0]?.payload?.['scope'], 'target_function');

  const snapshot = timing.snapshot();
  const stageNames = snapshot.stages.map((stage) => stage.name);
  assert.ok(stageNames.includes('read_context.bridge_send_receive'));
  assert.ok(stageNames.includes('read_context.bridge_payload_extract'));
  assert.ok(stageNames.includes('read_context.ue_timing_extract'));
  assert.ok(stageNames.includes('read_context.logic_project_payload'));
  assert.ok(stageNames.includes('read_context.bridge_payload_bytes'));
  assert.ok(stageNames.includes('read_context.ue_raw_payload_bytes'));
  assert.ok(stageNames.includes('read_context.post_processed_payload_bytes'));
  assert.equal(snapshot.nested?.[0]?.['name'], 'ue.read_blueprint_logic_json');

  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['timing'], undefined);
});

test('read_context logic_flow handler omits default anchors and keeps debug anchors', async () => {
  const bridgeResponse: BridgeResponse = {
    request_id: 'test',
    success: true,
    result: makeLogicJsonWithExternalAnchors(),
  };

  const context: BlueprintHelperToolContext = {
    cwd: process.cwd(),
    bridge: {
      sendCommand: async () => bridgeResponse,
    } as never,
    taskRunner: {} as never,
  };

  const result = await executeReadContext({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Test',
      target_type: 'graph',
      target_name: 'EventGraph',
    },
    view: {
      format: 'logic_flow',
    },
  }, context);

  assert.equal(result.ok, true);
  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'execflow');
  assert.match(payload['flow'] as string, /OpenDoor/);
  assert.equal(payload['anchors'], undefined);
  assert.equal(result.debug?.['logic_flow'] !== undefined, true);
  const anchors = (result.debug?.['logic_flow'] as Record<string, unknown>)['anchors'] as Record<string, unknown>[];
  assert.ok(Array.isArray(anchors));
  assert.equal(anchors.length, 2);
  assert.equal(anchors[0]?.['semantic_role'], 'exec_boundary');
  assert.equal(anchors[1]?.['semantic_role'], 'node');
});

test('read_context logic_flow handler renders function graph synthetic entry and result boundaries', async () => {
  const bridgeResponse: BridgeResponse = {
    request_id: 'test',
    success: true,
    result: makeFunctionLogicJsonWithSyntheticBoundaries(),
  };

  const bridgeCalls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const context: BlueprintHelperToolContext = {
    cwd: process.cwd(),
    bridge: {
      sendCommand: async (command: string, payload?: Record<string, unknown>) => {
        bridgeCalls.push({ command, payload });
        return bridgeResponse;
      },
    } as never,
    taskRunner: {} as never,
  };

  const result = await executeReadContext({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/Gameplay/Maze/BP_Maze',
      target_type: 'function',
      target_name: 'AddMazeRelativeRotation',
    },
    view: {
      format: 'logic_flow',
    },
  }, context);

  assert.equal(result.ok, true);
  assert.equal(bridgeCalls[0]?.command, 'read_blueprint_logic_json');
  assert.equal(bridgeCalls[0]?.payload?.['target_type'], 'function');
  assert.equal(bridgeCalls[0]?.payload?.['target_name'], 'AddMazeRelativeRotation');
  assert.equal(bridgeCalls[0]?.payload?.['function'], 'AddMazeRelativeRotation');
  assert.equal(bridgeCalls[0]?.payload?.['scope'], 'target_function');

  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'execflow');
  assert.equal(payload['flow'], 'AddMazeRelativeRotation -> SetRelativeRotation -> Return');
});
