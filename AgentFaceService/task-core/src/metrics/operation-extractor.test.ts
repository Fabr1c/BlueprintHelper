import assert from 'node:assert/strict';
import test from 'node:test';

import { extractReadToolOperation, extractTaskPlanMetricOperations } from './operation-extractor.js';

test('extractTaskPlanMetricOperations uses structured graph_write ops before strategy', () => {
  const operations = extractTaskPlanMetricOperations({
    steps: [
      {
        capability: 'graph_write',
        target: {
          asset_path: '/Game/Blueprints/BP_StoneGate',
          graph: 'BH_StoneGateActivation',
        },
        write: {
          strategy: 'owned_graph_edit',
          ops: [
            { op: 'ensure_entry' },
            { op: 'insert_flow' },
          ],
        },
      },
    ],
  });

  assert.deepEqual(operations, [
    { capability: 'graph_write', semantic_operation: 'ensure_entry' },
    { capability: 'graph_write', semantic_operation: 'insert_flow' },
  ]);
});

test('extractTaskPlanMetricOperations prefers GraphWrite route descriptor for replace scope', () => {
  const operations = extractTaskPlanMetricOperations({
    steps: [
      {
        capability: 'graph_write',
        target: {
          asset_path: '/Game/Blueprints/BP_StoneGate',
          graph: 'EventGraph',
        },
        write: {
          strategy: 'owned_graph_edit',
          ops: [
            {
              op: 'replace_body',
              replace_scope: 'function_body',
              selector: {
                function_name: 'ApplySaveGame',
              },
            },
          ],
        },
      },
    ],
  });

  assert.deepEqual(operations, [
    { capability: 'graph_write', semantic_operation: 'graph.replace.function_body' },
  ]);
});

test('extractTaskPlanMetricOperations derives stable semantic operation for graph_write adapter patch steps', () => {
  const operations = extractTaskPlanMetricOperations({
    steps: [
      {
        operation: 'patch_blueprint_graph',
        target: {
          asset_path: '/Game/Blueprints/BP_StoneGate',
          graph: 'EventGraph',
          patch_scope: 'pin_default',
        },
        args: {
          patch_type: 'set_pin_default',
        },
      },
    ],
  });

  assert.deepEqual(operations, [
    {
      capability: 'graph_write',
      semantic_operation: 'graph.patch.pin_default',
    },
  ]);
});

test('extractTaskPlanMetricOperations supports legacy action-like capability steps', () => {
  const operations = extractTaskPlanMetricOperations({
    steps: [
      {
        capability: 'graph_write',
        action: {
          strategy: 'external_graph_patch',
          ops: [
            { op: 'replace_node' },
          ],
        },
      },
    ],
  });

  assert.deepEqual(operations, [
    { capability: 'graph_write', semantic_operation: 'replace_node' },
  ]);
});

test('extractTaskPlanMetricOperations folds external graph-write adapter operations into graph_write', () => {
  const operations = extractTaskPlanMetricOperations({
    steps: [
      {
        operation: 'merge_external_flow',
        target: {
          asset_path: '/Game/Blueprints/BP_StoneGate',
          graph: 'EventGraph',
        },
        args: {
          insert_strategy: 'append_after',
        },
      },
      {
        operation: 'patch_external_graph',
        target: {
          asset_path: '/Game/Blueprints/BP_StoneGate',
          graph: 'EventGraph',
        },
        args: {
          kind: 'set_external_node_property',
          property_descriptor_id: 'k2.node.comment',
        },
      },
      {
        operation: 'patch_external_links',
        target: {
          asset_path: '/Game/Blueprints/BP_StoneGate',
          graph: 'EventGraph',
        },
        args: {
          kind: 'replace_link',
        },
      },
      {
        operation: 'replace_external_body',
        target: {
          asset_path: '/Game/Blueprints/BP_StoneGate',
          graph: 'EventGraph',
        },
        args: {
          scope: 'custom_event_body',
        },
      },
    ],
  });

  assert.deepEqual(operations, [
    { capability: 'graph_write', semantic_operation: 'graph.merge_external_flow.append_after' },
    { capability: 'graph_write', semantic_operation: 'graph.patch_external_graph.node_comment' },
    { capability: 'graph_write', semantic_operation: 'graph.patch_external_links.replace_link' },
    { capability: 'graph_write', semantic_operation: 'graph.replace_external_body.body' },
  ]);
});

test('extractReadToolOperation combines read_type with explicit format', () => {
  const operation = extractReadToolOperation({
    read_type: 'blueprint_logic',
    view: {
      format: 'logic_flow',
    },
  });

  assert.deepEqual(operation, {
    capability: 'read_context',
    semantic_operation: 'blueprint_logic.logic_flow',
  });
});

test('extractReadToolOperation defaults blueprint_logic to logic_flow', () => {
  const operation = extractReadToolOperation({
    read_type: 'blueprint_logic',
  });

  assert.deepEqual(operation, {
    capability: 'read_context',
    semantic_operation: 'blueprint_logic.logic_flow',
  });
});

test('extractReadToolOperation defaults graph_context to logic_json', () => {
  const operation = extractReadToolOperation({
    read_type: 'graph_context',
  });

  assert.deepEqual(operation, {
    capability: 'read_context',
    semantic_operation: 'graph_context.logic_json',
  });
});

test('extractReadToolOperation falls back to read.unknown when format is absent', () => {
  const operation = extractReadToolOperation({});

  assert.deepEqual(operation, {
    capability: 'read_context',
    semantic_operation: 'read.unknown',
  });
});
