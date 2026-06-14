import assert from 'node:assert/strict';
import test from 'node:test';

import type { TaskSpec } from '../schema/task-schemas.js';
import {
  compileTaskSpecToTaskPlan,
} from './task-compiler.js';
import {
  createDefaultTaskTypeCompilerRegistry,
} from './compilers/default-task-type-compilers.js';

test('MaterialGraph task type is registered in the default compiler registry', () => {
  const registry = createDefaultTaskTypeCompilerRegistry();

  assert.equal(registry.has('edit_material_graph'), true);
});

test('MaterialGraph append compiles to one graph_write material domain step', () => {
  const plan = compileTaskSpecToTaskPlan(makeMaterialGraphTaskSpec({
    graph_strategy: 'append_new_owned_graph',
    entries: [makeBlock()],
  }));
  const step = plan.steps[0] as Record<string, unknown>;
  const write = step['write'] as Record<string, unknown>;
  const constraints = step['constraints'] as Record<string, unknown>;
  const ops = write['ops'] as Array<Record<string, unknown>>;

  assert.equal(plan.task_type, 'edit_material_graph');
  assert.equal(plan.target_assets[0], '/Game/Materials/M_Test');
  assert.equal(plan.steps.length, 1);
  assert.equal(step['capability'], 'graph_write');
  assert.equal(write['strategy'], 'owned_graph_edit');
  assert.equal(write['graph_domain'], 'material_graph');
  assert.equal(write['material_strategy'], 'append_new_owned_graph');
  assert.equal(constraints['allow_modify_user_nodes'], false);
  assert.equal(constraints['ownership_scope'], 'blueprinthelper_owned');
  assert.equal(constraints['graph_domain'], 'material_graph');
  assert.equal(ops.some((op) => op['op'] === 'spawn_material_expression' && op['node_key'] === 'roughness'), true);
  assert.equal(ops.some((op) => op['op'] === 'connect_material_property'), true);
  assert.equal(ops.at(-1)?.['op'], 'compile_material');
});

test('MaterialGraph compiler lowers each graph_strategy to one material graph_write step', () => {
  const cases: Array<[string, Record<string, unknown>]> = [
    ['append_new_owned_graph', { graph_strategy: 'append_new_owned_graph', entries: [makeBlock()] }],
    ['replace_owned_graph', { graph_strategy: 'replace_owned_graph', replace: makeBlock() }],
    ['patch_owned_graph', {
      graph_strategy: 'patch_owned_graph',
      patches: [{
        block_id: 'surface_params',
        deletes: [{ node_key: 'old_roughness' }],
        nodes: [],
        links: [],
      }],
    }],
    ['merge_owned_graph', { graph_strategy: 'merge_owned_graph', merges: [makeBlock()] }],
  ];

  for (const [strategy, behavior] of cases) {
    const plan = compileTaskSpecToTaskPlan(makeMaterialGraphTaskSpec(behavior));
    const step = plan.steps[0] as Record<string, unknown>;
    const write = step['write'] as Record<string, unknown>;
    const ops = write['ops'] as Array<Record<string, unknown>>;

    assert.equal(plan.steps.length, 1, strategy);
    assert.equal(step['capability'], 'graph_write', strategy);
    assert.equal(write['strategy'], 'owned_graph_edit', strategy);
    assert.equal(write['graph_domain'], 'material_graph', strategy);
    assert.equal(write['material_strategy'], strategy);
    assert.equal(JSON.stringify(write).includes('k2.'), false, strategy);
    assert.equal(ops.at(-1)?.['op'], 'compile_material', strategy);
  }
});

test('MaterialGraph query selector lowers to candidate resolution op', () => {
  const plan = compileTaskSpecToTaskPlan(makeMaterialGraphTaskSpec({
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
  const step = plan.steps[0] as Record<string, unknown>;
  const write = step['write'] as Record<string, unknown>;
  const ops = write['ops'] as Array<Record<string, unknown>>;

  assert.equal(ops.some((op) => op['op'] === 'resolve_material_expression' && op['node_key'] === 'noise'), true);
});

test('MaterialGraph candidate_id selector lowers to deterministic spawn op', () => {
  const plan = compileTaskSpecToTaskPlan(makeMaterialGraphTaskSpec({
    graph_strategy: 'append_new_owned_graph',
    entries: [{
      block_id: 'procedural',
      nodes: [{
        node_key: 'noise',
        selector: { candidate_id: 'mat_expr_noise_0001' },
      }],
      links: [],
    }],
  }));
  const step = plan.steps[0] as Record<string, unknown>;
  const write = step['write'] as Record<string, unknown>;
  const ops = write['ops'] as Array<Record<string, unknown>>;

  assert.equal(ops.some((op) => op['op'] === 'spawn_material_expression' && op['node_key'] === 'noise'), true);
  assert.equal(ops.some((op) => op['op'] === 'resolve_material_expression'), false);
});

function makeMaterialGraphTaskSpec(behavior: Record<string, unknown>): TaskSpec {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_material_graph_compiler',
    task_type: 'edit_material_graph',
    feature_name: 'MaterialGraphCompiler',
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
  } as TaskSpec;
}

function makeBlock(): Record<string, unknown> {
  return {
    block_id: 'surface_params',
    nodes: [{
      node_key: 'roughness',
      selector: 'scalar_parameter',
      properties: {
        parameter_name: 'Roughness',
        group: 'Surface',
        default_value: 0.5,
      },
    }],
    links: [{
      from: { node_key: 'roughness', pin: 'Value' },
      to: { node_key: '$material_output', pin: 'Roughness' },
    }],
  };
}
