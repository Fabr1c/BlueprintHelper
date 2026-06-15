import assert from 'node:assert/strict';
import test from 'node:test';

import type { BridgeResponse } from '../../../bridge/bridge-client.js';
import { TaskTimingTrace } from '../../../task/service/task-timing.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { getActiveReadContextRouteDescriptors } from '../../templates/read-context-template-registry.js';
import { buildReadContextCapabilitiesPayload } from './read-context-capabilities.js';
import { executeReadContext } from './read-context-handler.js';
import { buildLogicFlowPayload } from './read-context-logic-flow.js';
import { buildReadContextBridgeRequest } from './read-context-route-builder.js';
import {
  LOGIC_PROJECTION_CALLBACK_CAPABILITIES,
  LOGIC_PROJECTION_OWNER,
  READ_CONTEXT_LOGIC_FORMATS,
  ReadContextInputSchema,
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
    ['logic_flow', 'logic_json'],
  );
  assert.deepEqual(
    LOGIC_PROJECTION_CALLBACK_CAPABILITIES,
    [
      'ue.raw_snapshot.logic_json',
      'ue.raw_snapshot.logic_flow',
    ],
  );
  assert.equal(LOGIC_PROJECTION_OWNER, 'task-core');
});

test('ReadContext keeps markdown disabled for blueprint logic reads', () => {
  const removedMarkdownFormat = ['logic', 'md'].join('_');
  const result = ReadContextInputSchema.safeParse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Test',
      target_type: 'function',
      target_name: 'Run',
    },
    view: {
      format: removedMarkdownFormat,
    },
  });

  assert.equal(result.success, false);
});

test('ReadContext material_graph_context schema accepts only the P0 logic read surface', () => {
  for (const format of ['logic_json', 'logic_flow'] as const) {
    const result = ReadContextInputSchema.safeParse({
      schema: 'BlueprintHelper.ReadSpec.v1',
      read_type: 'material_graph_context',
      target: {
        asset_path: '/Game/Materials/M_Test',
        target_type: 'material_graph',
      },
      view: {
        format,
      },
    });
    assert.equal(result.success, true, `${format} should be accepted`);
  }

  const removedMarkdownFormat = ['logic', 'md'].join('_');
  assert.equal(ReadContextInputSchema.safeParse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'material_graph_context',
    target: {
      asset_path: '/Game/Materials/M_Test',
      target_type: 'material_graph',
    },
    view: {
      format: removedMarkdownFormat,
    },
  }).success, false);

  assert.equal(ReadContextInputSchema.safeParse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'material_graph_context',
    target: {
      target_type: 'material_graph',
    },
    view: {
      format: 'logic_json',
    },
  }).success, false);

  assert.equal(ReadContextInputSchema.safeParse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'material_graph_context',
    domain: 'material',
    target: {
      asset_path: '/Game/Materials/M_Test',
      target_type: 'material_graph',
    },
    view: {
      format: 'logic_json',
    },
  }).success, false);

  assert.equal(ReadContextInputSchema.safeParse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'material_graph_context',
    target: {
      asset_path: '/Game/Materials/M_Test',
      target_type: 'material_graph',
    },
    view: {
      format: 'material_graph',
    },
  }).success, false);
});

test('ReadContext routes material_graph_context to material logic bridge commands', () => {
  const logicJson = ReadContextInputSchema.parse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'material_graph_context',
    target: {
      asset_path: '/Game/Materials/M_Test',
    },
    view: {
      format: 'logic_json',
    },
  });
  const logicJsonRequest = buildReadContextBridgeRequest(logicJson);
  assert.equal(logicJsonRequest.ok, true);
  if (logicJsonRequest.ok) {
    assert.equal(logicJsonRequest.command, 'read_material_logic_json');
    assert.equal(logicJsonRequest.payloadSchema, 'LogicJson.v1');
    assert.equal(logicJsonRequest.payload['asset_path'], '/Game/Materials/M_Test');
    assert.equal(logicJsonRequest.payload['target_type'], 'material_graph');
    assert.deepEqual(logicJsonRequest.payload['view'], { format: 'logic_json' });
  }

  const logicFlow = ReadContextInputSchema.parse({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'material_graph_context',
    target: {
      asset_path: '/Game/Materials/M_Test',
      target_type: 'material_graph',
    },
    view: {
      format: 'logic_flow',
    },
  });
  const logicFlowRequest = buildReadContextBridgeRequest(logicFlow);
  assert.equal(logicFlowRequest.ok, true);
  if (logicFlowRequest.ok) {
    assert.equal(logicFlowRequest.command, 'read_material_logic_json');
    assert.equal(logicFlowRequest.payloadSchema, 'LogicJson.v1');
  }
});

test('ReadContext material logic_json consumes Bridge payload with runtime status and diagnostics', async () => {
  const bridgeResponse: BridgeResponse = {
    request_id: 'material_status_payload',
    success: true,
    result: {
      schema: 'LogicJson.v1',
      scope: 'material_graph',
      status: 'completed',
      diagnostics: [],
      logic: {
        graph: 'MaterialGraph',
        graph_kind: 'material_graph',
        nodes: [],
        links: [],
      },
      material: {
        parameters: [],
        outputs: [],
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
    read_type: 'material_graph_context',
    target: {
      asset_path: '/Game/Materials/M_Test',
      target_type: 'material_graph',
    },
    view: {
      format: 'logic_json',
    },
  }, context);

  assert.equal(result.ok, true);
  assert.equal(bridgeCalls[0]?.command, 'read_material_logic_json');
  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicJson.v1');
  assert.equal(payload['status'], 'completed');
  assert.deepEqual(payload['diagnostics'], []);
});

test('ReadContext material logic_json consumes ToolResult data wrapper payload', async () => {
  const bridgeResponse: BridgeResponse = {
    request_id: 'material_tool_result_payload',
    success: true,
    result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'read_material_logic_json',
      status: 'completed',
      data: {
        schema: 'LogicJson.v1',
        scope: 'material_graph',
        status: 'completed',
        diagnostics: [],
        logic: {
          graph: 'MaterialGraph',
          graph_kind: 'material_graph',
          nodes: [],
          links: [],
        },
        material: {
          parameters: [],
          outputs: [],
        },
      },
    },
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
    read_type: 'material_graph_context',
    target: {
      asset_path: '/Game/Materials/M_Test',
      target_type: 'material_graph',
    },
    view: {
      format: 'logic_json',
    },
  }, context);

  assert.equal(result.ok, true);
  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicJson.v1');
  assert.equal(payload['status'], 'completed');
  assert.deepEqual(payload['diagnostics'], []);
});

test('ReadContext material projection failures return structured material error', async () => {
  const materialPayload: Record<string, unknown> = {
    schema: 'LogicJson.v1',
    scope: 'material_graph',
    material: {
      parameters: [],
      outputs: [],
    },
  };
  Object.defineProperty(materialPayload, 'logic', {
    enumerable: true,
    get() {
      throw new Error('projection boom');
    },
  });

  const bridgeResponse: BridgeResponse = {
    request_id: 'material_projection_failed_payload',
    success: true,
    result: materialPayload,
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
    read_type: 'material_graph_context',
    target: {
      asset_path: '/Game/Materials/M_Test',
      target_type: 'material_graph',
    },
    view: {
      format: 'logic_json',
    },
  }, context);

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'material_logic_projection_failed');
  assert.equal(result.error?.stage, 'post_process');
  assert.match(result.error?.message ?? '', /projection boom/);
});

test('read_context capabilities are derived from active ReadContext template registry routes', () => {
  const payload = buildReadContextCapabilitiesPayload();
  const activeRoutes = getActiveReadContextRouteDescriptors();
  const readTypeIds = payload['read_type_ids'] as string[];

  assert.equal(activeRoutes.every((route) => readTypeIds.includes(route.read_type)), true);
  assert.equal(readTypeIds.includes('widget_context'), true);
  assert.equal(readTypeIds.includes('data_table_context'), true);
  assert.equal(readTypeIds.includes('material_graph_context'), true);
  assert.equal(readTypeIds.includes('material_context'), false);
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
  assert.deepEqual(result.payload['adapter_boundary'], {
    runtime_adapter_id: 'k2.function_body',
    entry_count: 1,
    exit_count: 1,
  });
});

test('logic_flow exposes adapter body entry and body fingerprint for replace_external_body', () => {
  const result = buildLogicFlowPayload({
    schema: 'LogicJson.v1',
    adapter_boundary: {
      runtime_adapter_id: 'k2.external_graph.replace_body',
      graph_name: 'EventGraph',
      entry_boundaries: [{ node_ref: 'BodyEntry', display_name: 'BeginPlay' }],
      body_entry: {
        node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
        node_class: '/Script/BlueprintGraph.K2Node_Event',
        semantic_role: 'body_entry',
        fingerprint: 'body_entry_fp',
      },
      body_fingerprint: 'body_fp_before',
    },
    logic: {
      nodes: [
        { node_ref: 'BodyEntry', name: 'BeginPlay', kind: 'event' },
        { node_ref: 'PrintString', name: 'Print String', kind: 'call' },
      ],
      links: [
        { type: 'exec', from_node: 'BodyEntry', from_pin: 'then', to_node: 'PrintString', to_pin: 'execute' },
      ],
    },
  });

  assert.deepEqual(result.payload['adapter_boundary'], {
    runtime_adapter_id: 'k2.external_graph.replace_body',
    graph_name: 'EventGraph',
    body_entry: {
      node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
      node_class: '/Script/BlueprintGraph.K2Node_Event',
      semantic_role: 'body_entry',
      fingerprint: 'body_entry_fp',
    },
    body_fingerprint: 'body_fp_before',
    entry_count: 1,
    exit_count: 0,
  });
});

test('logic_flow does not synthesize body boundary evidence without adapter boundary', () => {
  const result = buildLogicFlowPayload({
    schema: 'LogicJson.v1',
    logic: {
      graph: 'EventGraph',
      nodes: [
        { node_ref: 'EventBeginPlay', name: 'Event BeginPlay', kind: 'event' },
        { node_ref: 'PrintString', name: 'Print String', kind: 'call' },
      ],
      links: [
        { type: 'exec', from_node: 'EventBeginPlay', from_pin: 'then', to_node: 'PrintString', to_pin: 'execute' },
      ],
    },
  });

  assert.equal(result.payload['adapter_boundary'], undefined);
});

test('logic_flow does not guess entry roots when adapter boundary is present but unmatched', () => {
  const result = buildLogicFlowPayload({
    schema: 'LogicJson.v1',
    adapter_boundary: {
      runtime_adapter_id: 'k2.function_body',
      entry_boundaries: [{ node_ref: 'FunctionEntry', display_name: 'AddScore' }],
      exit_boundaries: [{ node_ref: 'FunctionResult', display_name: 'Return' }],
    },
    logic: {
      nodes: [
        { node_ref: 'LocalEvent', name: 'Local Event', kind: 'event' },
        { node_ref: 'SetScore', name: 'Set Score', kind: 'call' },
      ],
      links: [
        { type: 'exec', from_node: 'LocalEvent', from_pin: 'then', to_node: 'SetScore', to_pin: 'execute' },
      ],
    },
  });

  assert.equal(result.payload['flow'], '');
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
  assert.deepEqual(result.payload['adapter_boundary'], {
    runtime_adapter_id: 'k2.macro_body',
    entry_count: 1,
    exit_count: 1,
  });
});

test('logic_flow remaps macro tunnel boundary refs to raw logic_json node refs', () => {
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
        {
          node_ref: 'nodes[0]',
          name: 'Inputs',
          kind: 'unknown',
          external_anchor: { node_class: '/Script/BlueprintGraph.K2Node_Tunnel' },
          external_anchors: [{ semantic_role: 'exec_boundary', pin_direction: 'output' }],
          links: [
            { type: 'exec', from_pin: 'Execute', to_node: 'nodes[2]', to_pin: 'execute' },
          ],
        },
        {
          node_ref: 'nodes[1]',
          name: 'Outputs',
          kind: 'unknown',
          external_anchor: { node_class: '/Script/BlueprintGraph.K2Node_Tunnel' },
        },
        { node_ref: 'nodes[2]', name: 'Print String', kind: 'call_function', links: [
          { type: 'exec', from_pin: 'then', to_node: 'nodes[1]', to_pin: 'Then' },
        ] },
      ],
    },
  });

  assert.equal(result.payload['flow'], 'Macro In -> Print String -> Macro Out');
  assert.deepEqual(result.payload['adapter_boundary'], {
    runtime_adapter_id: 'k2.macro_body',
    entry_count: 1,
    exit_count: 1,
  });
});

test('logic_flow remaps adapter boundary refs from semantic exec boundary anchors without graph-type guards', () => {
  const result = buildLogicFlowPayload({
    schema: 'LogicJson.v1',
    adapter_boundary: {
      runtime_adapter_id: 'k2.macro_body',
      entry_boundaries: [{ node_ref: 'AdapterEntry', display_name: 'Macro In' }],
      exit_boundaries: [{ node_ref: 'AdapterExit', display_name: 'Macro Out' }],
      visible_boundary_node_refs: ['AdapterEntry', 'AdapterExit'],
    },
    logic: {
      nodes: [
        {
          node_ref: 'raw-entry',
          name: 'Inputs',
          kind: 'unknown',
          external_anchors: [{ semantic_role: 'exec_boundary', pin_direction: 'output' }],
          links: [
            { type: 'exec', from_pin: 'Execute', to_node: 'body-call', to_pin: 'execute' },
          ],
        },
        {
          node_ref: 'raw-exit',
          name: 'Outputs',
          kind: 'unknown',
          external_anchors: [{ semantic_role: 'exec_boundary', pin_direction: 'input' }],
        },
        { node_ref: 'body-call', name: 'Print String', kind: 'call_function', links: [
          { type: 'exec', from_pin: 'then', to_node: 'raw-exit', to_pin: 'Then' },
        ] },
      ],
    },
  });

  assert.equal(result.payload['flow'], 'Macro In -> Print String -> Macro Out');
  assert.deepEqual(result.payload['adapter_boundary'], {
    runtime_adapter_id: 'k2.macro_body',
    entry_count: 1,
    exit_count: 1,
  });
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

test('read_context widget tree routes through bridge and projects logic_flow', async () => {
  const bridgeResponse: BridgeResponse = {
    request_id: 'widget_tree',
    success: true,
    result: {
      ok: true,
      data: {
        schema: 'WidgetContext.v1',
        asset_path: '/Game/UI/WBP_Menu',
        root: {
          widget_name: 'Canvas_Root',
          widget_class_path: '/Script/UMG.CanvasPanel',
          virtual_index: 0,
          children: [
            { widget_name: 'Dialog_Shell', widget_class_path: '/Script/UMG.ExpandableArea', virtual_index: 0, children: [] },
          ],
        },
        index: {
          Canvas_Root: { widget_class_path: '/Script/UMG.CanvasPanel', virtual_index: 0 },
          Dialog_Shell: {
            widget_class_path: '/Script/UMG.ExpandableArea',
            parent_name: 'Canvas_Root',
            virtual_index: 0,
          },
          BodyText: {
            widget_class_path: '/Script/UMG.TextBlock',
            parent_name: 'Dialog_Shell',
            slot_name: 'Body',
            virtual_index: 0,
          },
        },
        named_slots: [
          {
            host_widget_name: 'Dialog_Shell',
            slot_name: 'Body',
            content_widget_name: 'BodyText',
            virtual_index: 0,
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
    read_type: 'widget_context',
    target: {
      asset_path: '/Game/UI/WBP_Menu',
      target_type: 'blueprint',
    },
    view: {
      format: 'logic_flow',
    },
  }, context);

  assert.equal(result.ok, true);
  assert.equal(bridgeCalls[0]?.command, 'get_widget_tree');
  assert.deepEqual(bridgeCalls[0]?.payload, { asset_path: '/Game/UI/WBP_Menu' });

  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'WidgetTreeLogicFlow.v1');
  assert.equal(payload['asset_path'], '/Game/UI/WBP_Menu');
  assert.equal(payload['flow'], 'widgetroot[CanvasPanel] -> (Dialog_Shell[ExpandableArea](Body[NamedSlot](BodyText[TextBlock])))');
  assert.deepEqual(payload['warnings'], []);
});

test('read_context widget tree tree_json projects first-class tree payload', async () => {
  const bridgeResponse: BridgeResponse = {
    request_id: 'widget_tree_json',
    success: true,
    result: {
      ok: true,
      data: {
        schema: 'WidgetContext.v1',
        asset_path: '/Game/UI/WBP_Menu',
        root: {
          widget_name: 'Canvas_Root',
          widget_class_path: '/Script/UMG.CanvasPanel',
          virtual_index: 0,
          children: [
            { widget_name: 'Button_Start', widget_class_path: '/Script/UMG.Button', virtual_index: 0, children: [] },
          ],
        },
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
    read_type: 'widget_context',
    target: {
      asset_path: '/Game/UI/WBP_Menu',
      target_type: 'blueprint',
    },
    view: {
      format: 'tree_json',
    },
  }, context);

  assert.equal(result.ok, true);
  assert.equal(bridgeCalls[0]?.command, 'get_widget_tree');

  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'WidgetTreeJson.v1');
  assert.equal(payload['format'], 'tree_json');
  assert.equal(payload['domain'], 'widget_blueprint');
  assert.equal(payload['asset_path'], '/Game/UI/WBP_Menu');
  assert.equal(Boolean(payload['root']), true);
  assert.equal(Boolean(payload['index']), true);
  assert.equal(Array.isArray(payload['named_slots']), true);
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
  assert.equal(bridgeCalls[0]?.payload?.['graph_name'], 'ComputeValue');
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
  assert.equal(bridgeCalls[0]?.payload?.['graph_name'], 'AddMazeRelativeRotation');
  assert.equal(bridgeCalls[0]?.payload?.['function'], 'AddMazeRelativeRotation');
  assert.equal(bridgeCalls[0]?.payload?.['scope'], 'target_function');

  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'execflow');
  assert.equal(payload['flow'], 'AddMazeRelativeRotation -> SetRelativeRotation -> Return');
});

test('read_context event logic_flow sends graph_name for adapter boundary readback', async () => {
  const bridgeResponse: BridgeResponse = {
    request_id: 'test',
    success: true,
    result: makeLogicJsonWithExternalAnchors(),
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
      asset_path: '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter',
      target_type: 'event',
      target_name: 'ReceiveTick',
    },
    view: {
      format: 'logic_flow',
    },
  }, context);

  assert.equal(result.ok, true);
  assert.equal(bridgeCalls[0]?.command, 'read_blueprint_logic_json');
  assert.equal(bridgeCalls[0]?.payload?.['target_type'], 'event');
  assert.equal(bridgeCalls[0]?.payload?.['target_name'], 'ReceiveTick');
  assert.equal(bridgeCalls[0]?.payload?.['graph_name'], 'EventGraph');
  assert.equal(bridgeCalls[0]?.payload?.['event'], 'ReceiveTick');
  assert.equal(bridgeCalls[0]?.payload?.['scope'], 'target_event');
});
