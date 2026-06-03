import assert from 'node:assert/strict';
import test from 'node:test';

import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';

function baseSpec(): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_variables',
    feature_name: 'ReplicateDoorState',
    target: { asset_path: '/Game/BH/P0C/BP_Door', target_type: 'blueprint' },
    behavior: {
      variable_strategy: 'member_variables',
      changes: [{
        kind: 'configure_member_variable',
        name: 'DoorState',
        properties: [],
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
      review_baseline_dirty_asset_policy: 'block',
    },
    validation: { should_compile: true, should_save: false },
  };
}

function setReplicationProperty(spec: Record<string, unknown>, value: Record<string, unknown>): void {
  const behavior = spec.behavior as { changes: Array<{ properties: unknown[] }> };
  behavior.changes[0].properties = [{
    property_path: 'replication',
    value,
  }];
}

test('compiler lowers member variable rep_notify replication setting', () => {
  const spec = baseSpec();
  setReplicationProperty(spec, { mode: 'rep_notify', condition: 'owner_only' });

  const plan = compileTaskSpecToTaskPlan(spec as never);

  const step = plan.steps[0] as unknown as { write: { ops: unknown[] } };
  const op = step.write.ops[0] as Record<string, unknown>;
  assert.equal(op.op, 'set_member_variable_properties');
  assert.equal(op.name, 'DoorState');

  const settings = op.settings as Array<Record<string, unknown>>;
  assert.deepEqual(settings[0], {
    property_path: 'replication',
    value: {
      mode: 'rep_notify',
      condition: 'owner_only',
      notify_function: 'OnRep_DoorState',
      create_notify_function: true,
      reuse_existing_notify_function: false,
    },
  });
});

test('compiler rejects hidden replication condition before runtime lowering', () => {
  const spec = baseSpec();
  setReplicationProperty(spec, { mode: 'replicated', condition: 'COND_Dynamic' });

  assert.throws(
    () => compileTaskSpecToTaskPlan(spec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'invalid_replication_condition');
      assert.equal(error.issues[0]?.path, 'behavior.changes[0].properties[0].value.condition');
      return true;
    },
  );
});

test('compiler rejects condition on non-networked replication mode', () => {
  const spec = baseSpec();
  setReplicationProperty(spec, { mode: 'none', condition: 'owner_only' });

  assert.throws(
    () => compileTaskSpecToTaskPlan(spec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'replication_condition_requires_networked_mode');
      assert.equal(error.issues[0]?.path, 'behavior.changes[0].properties[0].value.condition');
      return true;
    },
  );
});

test('compiler rejects local variable replication with explicit unsupported code', () => {
  const spec = baseSpec();
  spec.behavior = {
    variable_strategy: 'local_variables',
    function_name: 'DoWork',
    changes: [{
      kind: 'configure_local_variable',
      name: 'DoorState',
      properties: [{
        property_path: 'replication',
        value: { mode: 'replicated' },
      }],
    }],
  };

  assert.throws(
    () => compileTaskSpecToTaskPlan(spec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'local_variable_replication_unsupported');
      assert.equal(error.issues[0]?.path, 'behavior.changes[0].properties[0].property_path');
      return true;
    },
  );
});
