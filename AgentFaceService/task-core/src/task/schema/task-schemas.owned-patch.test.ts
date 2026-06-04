import { strict as assert } from 'node:assert';
import test from 'node:test';
import { GraphWriteTaskSpecSchema } from './task-schemas.js';

function makePatchOwnedGraphSpec(patch: Record<string, unknown>): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: {
      target_type: 'blueprint',
      asset_path: '/Game/BH_Tests/BP_OwnedPatch',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'patch_owned_graph',
      patches: [patch],
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}

function assertAcceptsPatch(patch: Record<string, unknown>): void {
  const result = GraphWriteTaskSpecSchema.safeParse(makePatchOwnedGraphSpec(patch));
  assert.equal(result.success, true, JSON.stringify(result, null, 2));
}

function assertRejectsPatch(patch: Record<string, unknown>, pattern: RegExp): void {
  const result = GraphWriteTaskSpecSchema.safeParse(makePatchOwnedGraphSpec(patch));
  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), pattern);
  }
}

test('GraphWrite owned patch schema accepts connect_pins with source_ref', () => {
  assertAcceptsPatch({
    kind: 'connect_pins',
    scope: 'connect_pins',
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
});

test('GraphWrite owned patch schema accepts disconnect_link with link_ref', () => {
  assertAcceptsPatch({
    kind: 'disconnect_link',
    scope: 'disconnect_link',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
  });
});

test('GraphWrite owned patch schema accepts replace_link with replacement_ref', () => {
  assertAcceptsPatch({
    kind: 'replace_link',
    scope: 'replace_link',
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
});

test('GraphWrite owned patch schema rejects redundant endpoint block ids', () => {
  assertRejectsPatch({
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

  assertRejectsPatch({
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

test('GraphWrite owned patch schema accepts delete_owned_node with safe delete_policy', () => {
  assertAcceptsPatch({
    kind: 'delete_owned_node',
    scope: 'node_delete',
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
});

test('GraphWrite owned patch schema rejects redundant expected_old_state for owned link/delete patches', () => {
  assertRejectsPatch({
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
  }, /connect_pins does not support expected_old_state/u);

  assertRejectsPatch({
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
  }, /disconnect_link does not support expected_old_state/u);

  assertRejectsPatch({
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
  }, /replace_link does not support expected_old_state/u);

  assertRejectsPatch({
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
  }, /delete_owned_node does not support expected_old_state/u);
});

test('GraphWrite owned patch schema rejects missing source_ref for connect_pins', () => {
  assertRejectsPatch({
    kind: 'connect_pins',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_OpenDoor',
      pin_ref: 'execute',
    },
  }, /source_ref/u);
});

test('GraphWrite owned patch schema rejects missing replacement_ref for replace_link', () => {
  assertRejectsPatch({
    kind: 'replace_link',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
  }, /replacement_ref/u);
});

test('GraphWrite owned patch schema rejects unsafe delete_policy values', () => {
  assertRejectsPatch({
    kind: 'delete_owned_node',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Call_DebugPrint',
    },
    delete_policy: {
      break_links: false,
      allow_entry_node: true,
      allow_lifecycle_root: true,
    },
  }, /delete_policy|break_links|allow_entry_node|allow_lifecycle_root/u);
});

test('GraphWrite owned patch schema rejects P0-D scope mismatch', () => {
  assertRejectsPatch({
    kind: 'disconnect_link',
    scope: 'pin_default',
    target_ref: {
      block_id: 'EventGraph_OpenDoor',
      node_ref: 'Branch_DoorReady',
      pin_ref: 'then',
      link_ref: 'Branch_DoorReady.then->Call_OpenDoor.execute',
    },
  }, /disconnect_link/u);
});
