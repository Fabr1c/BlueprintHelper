import assert from 'node:assert/strict';
import test from 'node:test';

import type { BridgeResponse } from '../../../bridge/bridge-client.js';
import { TaskTimingTrace } from '../../../task/service/task-timing.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { executeReadContext } from './read-context-handler.js';
import { buildLogicFlowPayload } from './read-context-logic-flow.js';

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
        node_path: '$.graphs[AddMazeRelativeRotation].__function_entry__',
        node_ref: '__function_entry__',
      },
      nodes: [
        {
          node_ref: '__function_entry__',
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
              to_node: '__function_result__',
              to_pin: 'execute',
            },
          ],
        },
        {
          node_ref: '__function_result__',
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

test('logic_flow payload preserves external anchors in deterministic order', () => {
  const payload = buildLogicFlowPayload(makeLogicJsonWithExternalAnchors());

  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'execflow');
  const anchors = payload['anchors'] as Record<string, unknown>[];
  assert.ok(Array.isArray(anchors));
  assert.equal(anchors.length, 2);
  assert.equal(anchors[0]?.['semantic_role'], 'exec_boundary');
  assert.equal(anchors[1]?.['semantic_role'], 'node');
  assert.equal(anchors[0]?.['node_guid'], 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');
  assert.equal(anchors[0]?.['fingerprint'], 'boundaryfp');
  assert.equal(anchors[1]?.['fingerprint'], 'nodefp');
});

test('logic_flow payload renders function graph synthetic entry and result boundaries', () => {
  const payload = buildLogicFlowPayload(makeFunctionLogicJsonWithSyntheticBoundaries());

  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'execflow');
  assert.equal((payload['warnings'] as string[]).length, 0);
  assert.equal(payload['flow'], 'AddMazeRelativeRotation -> SetRelativeRotation -> Return');
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
  assert.ok(stageNames.includes('read_context.filter_payload'));
  assert.ok(stageNames.includes('read_context.bridge_payload_bytes'));
  assert.ok(stageNames.includes('read_context.ue_raw_payload_bytes'));
  assert.ok(stageNames.includes('read_context.post_processed_payload_bytes'));
  assert.equal(snapshot.nested?.[0]?.['name'], 'ue.read_blueprint_logic_json');

  const payload = result.data?.['payload'] as Record<string, unknown>;
  assert.equal(payload['timing'], undefined);
});

test('read_context logic_flow handler returns external anchors from bridge LogicJson', async () => {
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

  const anchors = payload['anchors'] as Record<string, unknown>[];
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
