import { strict as assert } from 'node:assert';
import test from 'node:test';

import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';
import {
  componentExpansionExpectedTaskPlanFixture,
  componentExpansionTaskSpecFixture,
  componentTaskSpecFixture,
} from '../fixtures/task-protocol.fixtures.js';

function clone<T>(value: T): T {
  return structuredClone(value);
}

function componentOps(taskSpec: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(taskSpec as never);
  return taskPlan.steps.map((step) => {
    const write = (step as Record<string, unknown>).write as { ops: Record<string, unknown>[] };
    assert.equal(write.ops.length, 1, `${step.step_id}.write.ops`);
    return write.ops[0];
  });
}

function specWithChange(change: Record<string, unknown>): Record<string, unknown> {
  const taskSpec = clone(componentTaskSpecFixture) as Record<string, unknown>;
  const behavior = taskSpec.behavior as Record<string, unknown>;
  behavior.changes = [change];
  return taskSpec;
}

test('component expansion fixture lowers to TaskPlan structural ops without runtime internals in TaskSpec', () => {
  assert.deepEqual(
    compileTaskSpecToTaskPlan(componentExpansionTaskSpecFixture as never),
    componentExpansionExpectedTaskPlanFixture,
  );
});

test('new component mutation kinds each lower to one blueprint_component TaskPlan step', () => {
  const cases: Array<[Record<string, unknown>, Record<string, unknown>]> = [
    [{
      kind: 'rename_component',
      name: 'DoorMesh',
      new_name: 'DoorVisual',
    }, {
      op: 'rename_component',
      component_name: 'DoorMesh',
      new_component_name: 'DoorVisual',
    }],
    [{
      kind: 'reparent_component',
      name: 'DoorMesh',
      new_parent: 'DoorRoot',
      socket: 'DoorSocket',
      attach_rule: 'keep_world',
      transform_policy: 'preserve_world',
    }, {
      op: 'reparent_component',
      component_name: 'DoorMesh',
      new_parent_component: 'DoorRoot',
      socket_name: 'DoorSocket',
      attach_rule: 'keep_world',
      transform_policy: 'preserve_world',
    }],
    [{
      kind: 'attach_component',
      name: 'DoorMesh',
      parent: 'DoorRoot',
      socket: 'DoorSocket',
      attach_rule: 'snap_to_target',
      transform_policy: 'reset_relative',
    }, {
      op: 'attach_component',
      component_name: 'DoorMesh',
      parent_component: 'DoorRoot',
      socket_name: 'DoorSocket',
      attach_rule: 'snap_to_target',
      transform_policy: 'reset_relative',
    }],
    [{
      kind: 'detach_component',
      name: 'DoorMesh',
      transform_policy: 'preserve_relative',
      default_root_policy: 'create_default_scene_root_when_needed',
    }, {
      op: 'detach_component',
      component_name: 'DoorMesh',
      transform_policy: 'preserve_relative',
      default_root_policy: 'create_default_scene_root_when_needed',
    }],
    [{
      kind: 'set_root_component',
      name: 'DoorRoot',
      old_root_policy: 'remove_default_scene_root_when_empty',
      default_root_policy: 'require_scene_component',
    }, {
      op: 'set_root_component',
      component_name: 'DoorRoot',
      old_root_policy: 'remove_default_scene_root_when_empty',
      default_root_policy: 'require_scene_component',
    }],
    [{
      kind: 'remove_component',
      name: 'DeprecatedMarker',
      delete_policy: 'promote_children',
    }, {
      op: 'remove_component',
      component_name: 'DeprecatedMarker',
      delete_policy: 'promote_children',
    }],
  ];

  for (const [change, expectedOp] of cases) {
    const ops = componentOps(specWithChange(change));
    assert.deepEqual(ops, [expectedOp], String(change.kind));
  }
});

test('ensure_component_present lowers class mismatch policy through name_collision_policy', () => {
  const ops = componentOps(specWithChange({
    kind: 'ensure_component_present',
    name: 'DoorMesh',
    class: '/Script/Engine.StaticMeshComponent',
    parent: 'DoorRoot',
    socket: 'DoorSocket',
    attach_rule: 'keep_relative',
    name_collision_policy: 'block_if_class_mismatch',
  }));

  assert.deepEqual(ops, [{
    op: 'add_component',
    component_name: 'DoorMesh',
    component_class: '/Script/Engine.StaticMeshComponent',
    parent_component: 'DoorRoot',
    socket_name: 'DoorSocket',
    attach_rule: 'keep_relative',
    name_collision_policy: 'block_if_class_mismatch',
  }]);
});

test('component lowering rejects unsupported delete_policy before runtime execution', () => {
  assert.throws(
    () => componentOps(specWithChange({
      kind: 'remove_component',
      name: 'DeprecatedMarker',
      delete_policy: 'delete_everything',
    })),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'unsupported_delete_policy');
      return true;
    },
  );
});

test('component lowering requires hierarchy parent fields before preview routing', () => {
  for (const kind of ['reparent_component', 'attach_component'] as const) {
    assert.throws(
      () => componentOps(specWithChange({
        kind,
        name: 'DoorMesh',
      })),
      (error) => {
        assert.ok(error instanceof TaskSpecCompileError);
        assert.equal(error.code, 'taskspec_semantic_invalid');
        assert.equal(error.issues[0]?.code, 'parent_component_not_found');
        return true;
      },
    );
  }
});
