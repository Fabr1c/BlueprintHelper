import { strict as assert } from 'node:assert';
import test from 'node:test';
import { compileTaskSpecToTaskPlan } from './task-compiler.js';

function makePatchOwnedGraphSpec(patch: Record<string, unknown>): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_owned_patch_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'OwnedPatchTs',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'patch_owned_graph',
      patches: [patch],
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

function compilePatchStep(patch: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(makePatchOwnedGraphSpec(patch) as never);
  const step = taskPlan.steps.find((candidate) => (
    (candidate as Record<string, unknown>).capability === 'graph_write'
  )) as Record<string, unknown> | undefined;
  assert.ok(step);
  return step;
}

function assertCompileRejectsPatch(patch: Record<string, unknown>, pattern: RegExp): void {
  let thrown: unknown;
  try {
    compilePatchStep(patch);
  } catch (error) {
    thrown = error;
  }
  assert.ok(thrown);
  const errorText = thrown instanceof Error
    ? `${thrown.name} ${thrown.message} ${JSON.stringify((thrown as { code?: unknown; issues?: unknown }))}`
    : JSON.stringify(thrown);
  assert.match(errorText, pattern);
}

test('owned connect_pins lowers source_ref into patch payload', () => {
  const step = compilePatchStep({
    kind: 'connect_pins',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_OpenDoor',
      pin_ref: 'execute',
    },
    source_ref: {
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
    },
  });
  const write = step.write as { strategy: string; ops: Array<Record<string, unknown>> };
  assert.equal(write.strategy, 'owned_graph_edit');
  assert.deepEqual(write.ops[0], {
    op: 'connect_pins',
    patch_scope: 'connect_pins',
    patched_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_OpenDoor',
      pin_ref: 'execute',
    },
    patch: {
      source_node_ref: 'Branch_DoorReady',
      source_pin_ref: 'then',
      source_block_id: 'EventGraph_OpenDoor',
    },
  });
});

test('owned patch compiler rejects raw target block_id anchors', () => {
  assertCompileRejectsPatch({
    kind: 'connect_pins',
    target_ref: {
      block_id: 'nodes[0]',
      node_ref: 'Call_OpenDoor',
      pin_ref: 'execute',
    },
    source_ref: {
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
    },
  }, /unsupported_graph_write_anchor|block_id|read-view/u);
});

test('owned patch compiler rejects redundant endpoint block ids', () => {
  assertCompileRejectsPatch({
    kind: 'connect_pins',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_OpenDoor',
      pin_ref: 'execute',
    },
    source_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
    },
  }, /source_ref\.block_id is redundant/u);

  assertCompileRejectsPatch({
    kind: 'replace_link',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
    replacement_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_CloseDoor',
      pin_ref: 'execute',
    },
  }, /replacement_ref\.block_id is redundant/u);
});

test('owned disconnect_link lowers link_ref without redundant expected state', () => {
  const step = compilePatchStep({
    kind: 'disconnect_link',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
  });
  const write = step.write as { ops: Array<Record<string, unknown>> };
  assert.deepEqual(write.ops[0], {
    op: 'disconnect_link',
    patch_scope: 'disconnect_link',
    patched_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
    patch: {},
  });
});

test('owned link/delete patch compiler rejects redundant expected_old_state', () => {
  assertCompileRejectsPatch({
    kind: 'connect_pins',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_OpenDoor',
      pin_ref: 'execute',
    },
    source_ref: {
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
    },
    expected_old_state: {
      already_linked: false,
    },
  }, /redundant_owned_patch_expected_old_state|expected_old_state/u);

  assertCompileRejectsPatch({
    kind: 'disconnect_link',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
    expected_old_state: {
      source_node_ref: 'Branch_DoorReady',
      source_pin_ref: 'then',
      target_node_ref: 'Call_OpenDoor',
      target_pin_ref: 'execute',
    },
  }, /redundant_owned_patch_expected_old_state|expected_old_state/u);

  assertCompileRejectsPatch({
    kind: 'replace_link',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
    replacement_ref: {
      node_ref: 'Call_CloseDoor',
      pin_ref: 'execute',
    },
    expected_old_state: {
      source_node_ref: 'Branch_DoorReady',
      source_pin_ref: 'then',
      target_node_ref: 'Call_OpenDoor',
      target_pin_ref: 'execute',
    },
  }, /redundant_owned_patch_expected_old_state|expected_old_state/u);

  assertCompileRejectsPatch({
    kind: 'delete_owned_node',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_DebugPrint',
    },
    delete_policy: {
      break_links: true,
      allow_entry_node: false,
      allow_lifecycle_root: false,
    },
    expected_old_state: {
      node_ref: 'Call_DebugPrint',
    },
  }, /redundant_owned_patch_expected_old_state|expected_old_state/u);
});

test('owned replace_link lowers replacement_ref into patch payload', () => {
  const step = compilePatchStep({
    kind: 'replace_link',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
    replacement_ref: {
      node_ref: 'Call_CloseDoor',
      pin_ref: 'execute',
    },
  });
  const write = step.write as { ops: Array<Record<string, unknown>> };
  assert.deepEqual(write.ops[0], {
    op: 'replace_link',
    patch_scope: 'replace_link',
    patched_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
    patch: {
      replacement_block_id: 'EventGraph_OpenDoor',
      replacement_node_ref: 'Call_CloseDoor',
      replacement_pin_ref: 'execute',
    },
  });
});

test('owned delete_owned_node lowers safe delete_policy', () => {
  const step = compilePatchStep({
    kind: 'delete_owned_node',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_DebugPrint',
    },
    delete_policy: {
      break_links: true,
      allow_entry_node: false,
      allow_lifecycle_root: false,
    },
  });
  const write = step.write as { ops: Array<Record<string, unknown>> };
  assert.deepEqual(write.ops[0], {
    op: 'delete_owned_node',
    patch_scope: 'node_delete',
    patched_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_DebugPrint',
    },
    patch: {
      break_links: true,
      allow_entry_node: false,
      allow_lifecycle_root: false,
    },
  });
});
