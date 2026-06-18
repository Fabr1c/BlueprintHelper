import { strict as assert } from 'node:assert';
import test from 'node:test';
import { GraphWriteTaskSpecSchema } from './task-schemas.js';
import { TASK_PROTOCOL_CONTRACT_V1 } from './task-contract.js';

function makePatchOwnedGraphSpec(patch: Record<string, unknown>): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: {
      target_type: 'blueprint',
      asset_path: '/Game/BH_Tests/BP_DeprecatedLayout',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'patch_owned_graph',
      patches: [patch],
    },
  };
}

test('GraphWrite TaskSpec rejects deprecated node-position patch authoring', () => {
  const result = GraphWriteTaskSpecSchema.safeParse(makePatchOwnedGraphSpec({
    kind: 'set_node_position',
    target_ref: {
      block_id: 'BH_DeprecatedLayout',
      group_entry_node_path: 'logic.groups[0].entry.node_path',
      node_ref: 'nodes[0]',
    },
    patch: {
      x: 320,
      y: 160,
    },
  }));

  assert.equal(result.success, false);
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /set_node_position/u);
  }
});

test('GraphWrite public contract does not advertise deprecated layout patch semantics', () => {
  const contract = TASK_PROTOCOL_CONTRACT_V1 as Record<string, any>;
  const taskSpecContract = contract.graph_write_taskspec_contract;
  const patchContract = taskSpecContract.patch_owned_graph;
  const taskPlanIrContract = contract.graph_write_taskplan_ir_contract;

  assert.deepEqual(patchContract.kinds, [
    'set_pin_default',
    'set_node_comment',
    'connect_pins',
    'disconnect_link',
    'replace_link',
    'delete_owned_node',
  ]);
  assert.deepEqual(Object.keys(patchContract.scope_derivation).sort(), [
    'connect_pins',
    'delete_owned_node',
    'disconnect_link',
    'replace_link',
    'set_node_comment',
    'set_pin_default',
  ]);
  assert.deepEqual(Object.keys(patchContract.field_shapes).sort(), [
    'connect_pins',
    'delete_owned_node',
    'disconnect_link',
    'replace_link',
    'set_node_comment',
    'set_pin_default',
  ]);
  assert.equal(JSON.stringify(taskSpecContract).includes('set_node_position'), false);
  assert.equal(JSON.stringify(taskSpecContract).includes('node_position'), false);
  assert.equal(JSON.stringify(taskSpecContract).includes('preserve_layout'), false);
  assert.equal(JSON.stringify(taskPlanIrContract).includes('set_node_position'), false);
  assert.equal(JSON.stringify(taskPlanIrContract).includes('node_position'), false);
});
