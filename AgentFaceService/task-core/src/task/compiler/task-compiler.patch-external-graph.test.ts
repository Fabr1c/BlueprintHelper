import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

const externalNodeAnchor = {
  schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
  asset_path: '/Game/BP/BP_Door',
  graph_name: 'EventGraph',
  node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
  node_class: '/Script/BlueprintGraph.K2Node_CustomEvent',
  semantic_role: 'node',
  fingerprint: 'nodefp',
};

function makePatchExternalGraphSpec(overrides: {
  scopePolicy?: Record<string, unknown>;
  patch?: Record<string, unknown>;
  behavior?: Record<string, unknown>;
} = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_patch_external_graph_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'PatchExternalGraphTs',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
      external_mutation_policy: {
        strategy: 'patch_external_graph',
        allowed_mutations: ['pin_default', 'node_comment'],
      },
      ...overrides.scopePolicy,
    },
    behavior: {
      graph_strategy: 'patch_external_graph',
      external_patches: [{
        kind: 'set_external_node_comment',
        anchor: externalNodeAnchor,
        value: 'reviewed',
        expected_old_state: { value: '' },
        ...overrides.patch,
      }],
      ...overrides.behavior,
    },
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

function compilePatchExternalStep(overrides?: Parameters<typeof makePatchExternalGraphSpec>[0]) {
  const taskPlan = compileTaskSpecToTaskPlan(makePatchExternalGraphSpec(overrides) as never);
  const step = taskPlan.steps.find((candidate) => (
    (candidate as Record<string, unknown>).capability === 'graph_write'
  )) as Record<string, unknown> | undefined;
  assert.ok(step);
  return step;
}

test('patch_external_graph lowers to external graph edit with exact mutation policy', () => {
  const step = compilePatchExternalStep();
  const write = step.write as { strategy: string; ops: Array<Record<string, unknown>> };

  assert.equal(write.strategy, 'external_graph_edit');
  assert.equal(write.ops.length, 1);
  assert.equal(write.ops[0]?.op, 'set_external_node_comment');
  assert.deepEqual(write.ops[0]?.anchor, externalNodeAnchor);
  assert.deepEqual(write.ops[0]?.expected_old_state, { value: '' });
  assert.deepEqual(step.constraints, {
    allow_modify_user_nodes: false,
    ownership_scope: 'external_user_authored',
    external_mutation_policy: {
      strategy: 'patch_external_graph',
      allowed_mutations: ['pin_default', 'node_comment'],
    },
  });
});

test('patch_external_graph accepts pin default patches with explicit pin anchor fields', () => {
  const step = compilePatchExternalStep({
    patch: {
      kind: 'set_external_pin_default',
      anchor: {
        ...externalNodeAnchor,
        pin_name: 'Value',
        pin_direction: 'input',
      },
      value: '42',
      expected_old_state: { value: '0' },
    },
  });
  const write = step.write as { ops: Array<Record<string, unknown>> };
  assert.equal(write.ops[0]?.op, 'set_external_pin_default');
  assert.deepEqual(write.ops[0]?.anchor, {
    ...externalNodeAnchor,
    pin_name: 'Value',
    pin_direction: 'input',
  });
});

test('patch_external_graph rejects broad policy, missing expected state, and non-field mutations', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makePatchExternalGraphSpec({
      scopePolicy: {
        allow_modify_user_nodes: true,
      },
    }) as never),
    /unsupported_scope_policy/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makePatchExternalGraphSpec({
      patch: {
        expected_old_state: undefined,
      },
    }) as never),
    /expected_old_state/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makePatchExternalGraphSpec({
      patch: {
        kind: 'connect_pins',
      },
    }) as never),
    /not supported|set_external_pin_default|set_external_node_comment|patch_external_graph/,
  );
});

test('patch_external_graph rejects owned patches mixed into external strategy', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makePatchExternalGraphSpec({
      behavior: {
        patches: [{
          kind: 'set_node_comment',
          target_ref: { node_ref: 'nodes[0]' },
          value: 'wrong channel',
        }],
      },
    }) as never),
    /patches does not belong|external_patches/,
  );
});
