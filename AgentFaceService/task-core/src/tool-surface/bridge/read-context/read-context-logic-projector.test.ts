import assert from 'node:assert/strict';
import test from 'node:test';

import { parseCompactAnchorRef } from './read-context-compact-anchor.js';
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

test('logic projector rejects removed markdown format instead of normalizing it', () => {
  const removedMarkdownFormat = ['logic', 'md'].join('_');
  assert.throws(() => projectReadContextLogic({
    requestedFormat: removedMarkdownFormat as never,
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
      schema: 'LogicJson.v1',
      material: {
        parameters: [{ name: 'BaseColor', type: 'Vector' }],
        outputs: [{ property: 'BaseColor' }],
      },
    },
    target: { asset_path: '/Game/Materials/M_Test' },
  }), /unsupported_logic_format/);
});

test('logic_json projection adds compact anchor fields to external links with stable endpoints', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_json',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
      schema: 'LogicJson.v1',
      logic: {
        graph: 'EventGraph',
        nodes: [
          {
            node_ref: 'nodes[0]',
            name: 'Set FocusActor',
            pins: [
              { pin_ref: 'then', pin_category: 'exec' },
              { pin_ref: 'FocusActor', pin_category: 'object' },
            ],
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
              node_class: '/Script/BlueprintGraph.K2Node_VariableSet',
              semantic_role: 'node',
              fingerprint: 'source_node_fp',
            },
          },
          {
            node_ref: 'nodes[1]',
            name: 'Branch',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
              node_class: '/Script/BlueprintGraph.K2Node_IfThenElse',
              semantic_role: 'node',
              fingerprint: 'target_node_fp',
            },
          },
        ],
        links: [
          {
            ownership: 'external_user',
            kind: 'exec',
            from_id: 'nodes[0]',
            from_pin: 'then',
            to_id: 'nodes[1]',
            to_pin: 'execute',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
              node_class: '/Script/BlueprintGraph.K2Node_VariableSet',
              pin_name: 'then',
              pin_direction: 'output',
              semantic_role: 'exec_boundary',
              fingerprint: 'boundaryfp1234567890',
            },
          },
          {
            kind: 'data',
            from_id: 'nodes[2]',
            from_pin: 'ReturnValue',
            to_id: 'nodes[3]',
            to_pin: 'AngleDegrees',
          },
        ],
      },
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  const logic = result.payload.logic as Record<string, unknown>;
  const nodes = logic.nodes as Record<string, unknown>[];
  const links = logic.links as Record<string, unknown>[];
  assert.equal(nodes[0]?.anchor_type, 'external_node');
  const pins = nodes[0]?.pins as Record<string, unknown>[];
  assert.equal(pins[0]?.anchor_type, 'external_pin');
  assert.match(String(pins[0]?.anchor_ref), /^xpin:v1:e:aaaaaaaa\.then#[a-f0-9]{10}$/u);
  assert.match(String(pins[1]?.anchor_ref), /^xpin:v1:d:aaaaaaaa\.FocusActor#[a-f0-9]{10}$/u);
  assert.equal(links[0]?.anchor_type, 'external_link');
  assert.equal(links[0]?.label, 'nodes[0].then -> nodes[1].execute');
  assert.match(String(links[0]?.anchor_ref), /^xlink:v1:e:aaaaaaaa\.then>bbbbbbbb\.execute#[a-f0-9]{10}$/u);
  assert.deepEqual(parseCompactAnchorRef(String(links[0]?.anchor_ref)), {
    anchorType: 'external_link',
    version: 'v1',
    kind: 'exec',
    sourceNodeKey: 'aaaaaaaa',
    sourcePinKey: 'then',
    targetNodeKey: 'bbbbbbbb',
    targetPinKey: 'execute',
    fingerprint: String(links[0]?.anchor_ref).split('#')[1],
  });
  assert.equal(links[1]?.anchor_type, undefined);
  assert.equal(links[1]?.anchor_ref, undefined);
});

test('logic_json projection requires ownership before adding external link compact anchor', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_json',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
      schema: 'LogicJson.v1',
      logic: {
        graph: 'EventGraph',
        nodes: [
          {
            node_ref: 'nodes[0]',
            name: 'Source',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
              node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
              semantic_role: 'node',
              fingerprint: 'source_fp',
            },
          },
          {
            node_ref: 'nodes[1]',
            name: 'Target',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
              node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
              semantic_role: 'node',
              fingerprint: 'target_fp',
            },
          },
        ],
        links: [
          {
            kind: 'exec',
            from_node: 'nodes[0]',
            from_pin: 'then',
            to_node: 'nodes[1]',
            to_pin: 'execute',
          },
        ],
      },
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  const logic = result.payload.logic as Record<string, unknown>;
  const links = logic.links as Record<string, unknown>[];
  assert.equal(links[0]?.anchor_type, undefined);
  assert.equal(links[0]?.anchor_ref, undefined);
  const diagnostics = result.debug?.compact_anchor_diagnostics as Record<string, unknown>[] | undefined;
  assert.equal(diagnostics?.[0]?.code, 'missing_link_ownership');
});

test('logic_json projection adds external link compact anchor when ownership is external_user', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_json',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
      schema: 'LogicJson.v1',
      logic: {
        graph: 'EventGraph',
        nodes: [
          {
            node_ref: 'nodes[0]',
            name: 'Source',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
              node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
              semantic_role: 'node',
              fingerprint: 'source_fp',
            },
          },
          {
            node_ref: 'nodes[1]',
            name: 'Target',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
              node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
              semantic_role: 'node',
              fingerprint: 'target_fp',
            },
          },
        ],
        links: [
          {
            ownership: 'external_user',
            kind: 'exec',
            from_node: 'nodes[0]',
            from_pin: 'then',
            to_node: 'nodes[1]',
            to_pin: 'execute',
          },
        ],
      },
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  const logic = result.payload.logic as Record<string, unknown>;
  const links = logic.links as Record<string, unknown>[];
  assert.equal(links[0]?.ownership, 'external_user');
  assert.equal(links[0]?.anchor_type, 'external_link');
  assert.match(String(links[0]?.anchor_ref), /^xlink:v1:e:aaaaaaaa\.then>bbbbbbbb\.execute#[a-f0-9]{10}$/u);
});

test('logic_json projection keeps owned_internal links unanchored', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_json',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
      schema: 'LogicJson.v1',
      logic: {
        graph: 'EventGraph',
        nodes: [
          {
            node_ref: 'nodes[0]',
            name: 'Owned Source',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
              node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
              semantic_role: 'node',
              fingerprint: 'owned_source_fp',
            },
          },
          {
            node_ref: 'nodes[1]',
            name: 'Owned Target',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
              node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
              semantic_role: 'node',
              fingerprint: 'owned_target_fp',
            },
          },
        ],
        links: [
          {
            ownership: 'owned_internal',
            kind: 'exec',
            from_node: 'nodes[0]',
            from_pin: 'then',
            to_node: 'nodes[1]',
            to_pin: 'execute',
          },
        ],
      },
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  const logic = result.payload.logic as Record<string, unknown>;
  const links = logic.links as Record<string, unknown>[];
  assert.equal(links[0]?.ownership, 'owned_internal');
  assert.equal(links[0]?.anchor_type, undefined);
  assert.equal(links[0]?.anchor_ref, undefined);
});

test('logic_json projection enriches grouped UE logic payloads recursively', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_json',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
      schema: 'LogicJson.v1',
      logic: {
        graph: 'EventGraph',
        groups: [{
          group_type: 'global_event_flow',
          nodes: [
            {
              node_ref: 'nodes[1]',
              name: 'Event BeginPlay',
              external_anchor: {
                schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
                node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                node_class: '/Script/BlueprintGraph.K2Node_Event',
                semantic_role: 'node',
                fingerprint: 'event_node_fp',
              },
              links: [{
                link_ref: 'links[0]',
                ownership: 'external_boundary',
                type: 'exec',
                from_pin: 'then',
                to_node: 'nodes[2]',
                to_pin: 'execute',
                external_anchor: {
                  schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
                  node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                  node_class: '/Script/BlueprintGraph.K2Node_Event',
                  pin_name: 'then',
                  pin_direction: 'output',
                  semantic_role: 'exec_boundary',
                  fingerprint: 'event_then_fp',
                },
              }],
            },
            {
              node_ref: 'nodes[2]',
              name: 'Print String',
              external_anchor: {
                schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
                node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
                node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
                semantic_role: 'node',
                fingerprint: 'call_node_fp',
              },
            },
          ],
        }],
      },
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  const logic = result.payload.logic as Record<string, unknown>;
  const groups = logic.groups as Record<string, unknown>[];
  const nodes = groups[0]?.nodes as Record<string, unknown>[];
  const links = nodes[0]?.links as Record<string, unknown>[];
  assert.equal(nodes[0]?.anchor_type, 'external_node');
  assert.equal(links[0]?.anchor_type, 'external_link');
  assert.match(String(links[0]?.anchor_ref), /^xlink:v1:e:aaaaaaaa\.then>bbbbbbbb\.execute#[a-f0-9]{10}$/u);
  const anchors = result.debug?.anchors as Record<string, unknown>[];
  assert.ok(anchors.some((anchor) => anchor['fingerprint'] === 'event_then_fp'));
});

test('compact anchor parser accepts node pin link and body refs', () => {
  assert.deepEqual(parseCompactAnchorRef('xnode:v1:ABCD1234#deadbeef01'), {
    anchorType: 'external_node',
    version: 'v1',
    nodeKey: 'ABCD1234',
    fingerprint: 'deadbeef01',
  });
  assert.deepEqual(parseCompactAnchorRef('xpin:v1:d:ABCD1234.ReturnValue#deadbeef01'), {
    anchorType: 'external_pin',
    version: 'v1',
    kind: 'data',
    nodeKey: 'ABCD1234',
    pinKey: 'ReturnValue',
    fingerprint: 'deadbeef01',
  });
  assert.deepEqual(parseCompactAnchorRef('xbody:v1:function:ABCD1234#deadbeef01'), {
    anchorType: 'external_body',
    version: 'v1',
    scope: 'function',
    entryNodeKey: 'ABCD1234',
    fingerprint: 'deadbeef01',
  });
  assert.equal(parseCompactAnchorRef('links[5]'), undefined);
});

test('logic_json projection adds compact anchor to external boundary link with stable endpoint evidence', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_json',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
      schema: 'LogicJson.v1',
      logic: {
        graph: 'EventGraph',
        nodes: [
          {
            node_ref: 'nodes[1]',
            name: 'Generated Resolver',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
              node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
              semantic_role: 'node',
              fingerprint: 'generated_node_fp',
            },
          },
        ],
        links: [
          {
            ownership: 'external_boundary',
            kind: 'data',
            from_pin: 'ReturnValue',
            to_id: 'nodes[1]',
            to_pin: 'AngleDegrees',
            external_anchor: {
              schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
              asset_path: '/Game/BP_Door',
              graph_name: 'EventGraph',
              node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
              node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
              pin_name: 'ReturnValue',
              pin_direction: 'output',
              semantic_role: 'node',
              fingerprint: 'source_boundary_fp',
            },
          },
        ],
      },
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  const logic = result.payload.logic as Record<string, unknown>;
  const links = logic.links as Record<string, unknown>[];
  assert.equal(links[0]?.anchor_type, 'external_link');
  assert.equal(links[0]?.ownership, 'external_boundary');
  assert.match(String(links[0]?.anchor_ref), /^xlink:v1:d:aaaaaaaa\.ReturnValue>bbbbbbbb\.AngleDegrees#[a-f0-9]{10}$/u);
});

test('logic_json projection does not synthesize link anchors from display refs alone', () => {
  const result = projectReadContextLogic({
    requestedFormat: 'logic_json',
    bridgePayloadSchema: 'LogicJson.v1',
    bridgePayload: {
      schema: 'LogicJson.v1',
      logic: {
        graph: 'EventGraph',
        links: [{
          kind: 'exec',
          from_id: 'nodes[0]',
          from_pin: 'then',
          to_id: 'nodes[1]',
          to_pin: 'execute',
          external_anchor: {
            schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
            asset_path: '/Game/BP_Door',
            graph_name: 'EventGraph',
            node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
            node_class: '/Script/BlueprintGraph.K2Node_VariableSet',
            pin_name: 'then',
            pin_direction: 'output',
            semantic_role: 'exec_boundary',
            fingerprint: 'boundaryfp1234567890',
          },
        }],
      },
    },
    target: { asset_path: '/Game/BP_Door', graph: 'EventGraph' },
  });

  const logic = result.payload.logic as Record<string, unknown>;
  const links = logic.links as Record<string, unknown>[];
  assert.equal(links[0]?.anchor_type, undefined);
  assert.equal(links[0]?.anchor_ref, undefined);
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
