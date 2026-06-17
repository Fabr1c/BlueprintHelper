import assert from 'node:assert/strict';
import test from 'node:test';

import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';

function baseSpec(): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_variables',
    feature_name: 'RecordDisplayState',
    target: { asset_path: '/Game/Gameplay/Manager/OB_DataManager', target_type: 'blueprint' },
    behavior: {
      variable_strategy: 'member_variables',
      changes: [],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
      review_baseline_dirty_asset_policy: 'block',
    },
    validation: { should_compile: true, should_save: false },
  };
}

test('compiler lowers ensure_member_variable default_value alias into member default step', () => {
  const spec = baseSpec();
  (spec.behavior as { changes: unknown[] }).changes = [{
    kind: 'ensure_member_variable',
    name: 'bCurrentRunCompletedForRecordDisplay',
    pin_type: { category: 'bool' },
    default_value: false,
  }];

  const plan = compileTaskSpecToTaskPlan(spec as never);
  const firstStep = plan.steps[0] as unknown as { write: { strategy: string; ops: unknown[] } };
  const secondStep = plan.steps[1] as unknown as { depends_on?: string[]; write: { strategy: string; ops: unknown[] } };

  assert.equal(plan.steps.length, 2);
  assert.equal(firstStep.write.strategy, 'member_variables');
  assert.deepEqual(firstStep.write.ops, [{
    op: 'ensure_member_variable',
    name: 'bCurrentRunCompletedForRecordDisplay',
    pin_type: { category: 'bool' },
  }]);

  assert.equal(secondStep.write.strategy, 'member_defaults');
  assert.deepEqual(secondStep.depends_on, ['step_001']);
  assert.deepEqual(secondStep.write.ops, [{
    op: 'set_member_default',
    name: 'bCurrentRunCompletedForRecordDisplay',
    value: false,
  }]);
});

test('compiler rejects member default through configure_member_variable properties', () => {
  const spec = baseSpec();
  (spec.behavior as { changes: unknown[] }).changes = [{
    kind: 'configure_member_variable',
    name: 'bCurrentRunCompletedForRecordDisplay',
    properties: [{
      property_path: 'default_value',
      value: false,
    }],
  }];

  assert.throws(
    () => compileTaskSpecToTaskPlan(spec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'member_variable_default_requires_member_defaults');
      assert.equal(error.issues[0]?.path, 'behavior.changes[0].properties[0].property_path');
      return true;
    },
  );
});
