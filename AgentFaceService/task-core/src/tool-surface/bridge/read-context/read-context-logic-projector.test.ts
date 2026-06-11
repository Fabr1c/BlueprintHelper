import assert from 'node:assert/strict';
import test from 'node:test';

import { projectReadContextLogic } from './read-context-logic-projector.js';

const logicJsonPayload = {
  schema: 'LogicJson.v1',
  format: 'logic_json',
  logic: {
    graph: 'EventGraph',
    nodes: [
      {
        node_ref: 'nodes[0]',
        kind: 'custom_event',
        name: 'OpenDoor',
        links: [{
          type: 'exec',
          from_pin: 'then',
          to_node: 'nodes[1]',
          to_pin: 'execute',
        }],
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
  },
};

test('logic projector routes logic_flow through canonical task-core builder', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_flow',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: logicJsonPayload,
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  assert.equal(result.format, 'logic_flow');
  assert.equal(result.payload.schema, 'LogicFlow.v1');
  assert.match(String(result.payload.flow), /OpenDoor/);
});

test('logic projector degrades unknown links to logic_json', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_flow',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
      ...logicJsonPayload,
      logic: {
        ...logicJsonPayload.logic,
        nodes: [],
        links: [{
          type: 'latent',
          from_node: 'a',
          to_node: 'b',
        }],
      },
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  assert.equal(result.format, 'logic_json');
  assert.equal(result.payload.schema, 'LogicJson.v1');
  assert.deepEqual(result.payload.warnings, ['logic_flow_degraded_unknown_link']);
});

test('logic projector preserves task-core ownership metadata for raw UE payloads', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_json',
    bridgePayloadSchema: 'LogicSnapshot.v1',
    bridgePayload: {
      format: 'logic_json',
      projection_owner: 'task-core',
      ue_callback_schema: 'LogicSnapshot.v1',
      raw_snapshot: logicJsonPayload,
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  assert.equal(result.format, 'logic_json');
  assert.equal(result.payload.schema, 'LogicJson.v1');
  assert.equal(result.payload.projection_owner, 'task-core');
});

test('logic projector preserves body entry boundary evidence in logic_flow', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_flow',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
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
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  assert.equal(result.format, 'logic_flow');
  assert.deepEqual(result.payload.adapter_boundary, {
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
