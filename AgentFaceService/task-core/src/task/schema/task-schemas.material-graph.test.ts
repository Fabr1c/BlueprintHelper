import { strict as assert } from 'node:assert';
import test from 'node:test';

import {
  MATERIAL_GRAPH_COMMON_SELECTORS,
  MaterialGraphTaskSpecSchema,
  TaskSpecSchema,
} from './task-schemas.js';
import { TASK_PROTOCOL_CONTRACT_V1 } from './task-contract.js';

test('MaterialGraph schema accepts append_new_owned_graph with common selectors', () => {
  const result = TaskSpecSchema.safeParse(makeMaterialGraphSpec({
    graph_strategy: 'append_new_owned_graph',
    entries: [makeBlock()],
  }));

  assert.equal(result.success, true, JSON.stringify(result, null, 2));
});

test('MaterialGraph schema rejects domain and layout fields', () => {
  const result = MaterialGraphTaskSpecSchema.safeParse({
    ...makeMaterialGraphSpec({
      graph_strategy: 'append_new_owned_graph',
      entries: [makeBlock()],
    }),
    domain: 'material',
    position: { x: 10, y: 20 },
    comment: 'not a P0 field',
    label: 'not a P0 field',
  });

  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    const issues = JSON.stringify(result.error.issues);
    assert.match(issues, /domain/);
    assert.match(issues, /position/);
    assert.match(issues, /comment/);
    assert.match(issues, /label/);
  }
});

test('MaterialGraph schema enforces strategy payload exclusivity', () => {
  const result = MaterialGraphTaskSpecSchema.safeParse(makeMaterialGraphSpec({
    graph_strategy: 'replace_owned_graph',
    replace: makeBlock(),
    entries: [makeBlock()],
  }));

  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /entries does not belong to graph_strategy replace_owned_graph/);
  }
});

test('MaterialGraph schema rejects unknown selectors', () => {
  const result = MaterialGraphTaskSpecSchema.safeParse(makeMaterialGraphSpec({
    graph_strategy: 'append_new_owned_graph',
    entries: [{
      block_id: 'surface_params',
      nodes: [{
        node_key: 'bad',
        selector: 'branch',
      }],
      links: [],
    }],
  }));

  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /Invalid enum value|Expected/);
  }
});

test('MaterialGraph schema rejects K2 graph path', () => {
  const spec = makeMaterialGraphSpec({
    graph_strategy: 'append_new_owned_graph',
    entries: [makeBlock()],
  });
  spec.target = {
    ...spec.target,
    graph_path: 'EventGraph',
  };

  const result = MaterialGraphTaskSpecSchema.safeParse(spec);

  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /target.graph_path/);
  }
});

test('MaterialGraph schema accepts query selector for preview candidate discovery', () => {
  const result = MaterialGraphTaskSpecSchema.safeParse(makeMaterialGraphSpec({
    graph_strategy: 'append_new_owned_graph',
    entries: [{
      block_id: 'procedural',
      nodes: [{
        node_key: 'noise',
        selector: { query: 'noise material expression' },
      }],
      links: [],
    }],
  }));

  assert.equal(result.success, true, JSON.stringify(result, null, 2));
});

test('MaterialGraph public contract exposes P0 selectors and forbids domain/layout fields', () => {
  const contract = TASK_PROTOCOL_CONTRACT_V1 as Record<string, any>;
  const materialContract = contract.material_graph_taskspec_contract;

  assert.equal(contract.supported_task_types.includes('edit_material_graph'), true);
  assert.equal(materialContract.task_type, 'edit_material_graph');
  assert.deepEqual(materialContract.graph_strategies, [
    'append_new_owned_graph',
    'replace_owned_graph',
    'patch_owned_graph',
    'merge_owned_graph',
  ]);
  assert.deepEqual(materialContract.common_selectors, MATERIAL_GRAPH_COMMON_SELECTORS);
  assert.equal(materialContract.candidate_selector_contract.rule, 'candidate selector requires exactly one of candidate_id or query');
  assert.equal(materialContract.material_output_contract.pseudo_node_key, '$material_output');
  assert.equal(materialContract.forbidden_agent_shapes.includes('domain'), true);
  assert.equal(materialContract.forbidden_agent_shapes.includes('position'), true);
  assert.equal(materialContract.forbidden_agent_shapes.includes('target.graph_path'), true);
});

function makeMaterialGraphSpec(behavior: Record<string, unknown>): Record<string, any> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_material_graph_schema',
    task_type: 'edit_material_graph',
    feature_name: 'MaterialGraphSchema',
    target: {
      asset_path: '/Game/Materials/M_Test',
    },
    behavior,
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}

function makeBlock(): Record<string, unknown> {
  return {
    block_id: 'surface_params',
    nodes: [
      {
        node_key: 'roughness',
        selector: 'scalar_parameter',
        properties: {
          parameter_name: 'Roughness',
          group: 'Surface',
          default_value: 0.5,
        },
      },
      {
        node_key: 'base_color',
        selector: 'vector_parameter',
        properties: {
          parameter_name: 'BaseColor',
          group: 'Surface',
          default_value: {
            r: 1,
            g: 1,
            b: 1,
            a: 1,
          },
        },
      },
    ],
    links: [
      {
        from: { node_key: 'roughness', pin: 'Value' },
        to: { node_key: '$material_output', pin: 'Roughness' },
      },
      {
        from: { node_key: 'base_color', pin: 'RGB' },
        to: { node_key: '$material_output', pin: 'BaseColor' },
      },
    ],
  };
}
