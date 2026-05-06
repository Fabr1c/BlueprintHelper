import assert from 'node:assert/strict';
import test from 'node:test';
import {
  TaskPlanSchema,
  TaskSpecSchema,
} from './task-schemas.js';
import { compileTaskSpecToTaskPlan } from './task-compiler.js';

function baseTaskSpec(taskType: string, behavior: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: taskType,
    feature_name: 'P2Smoke',
    target: {
      asset_path: '/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke',
      target_type: 'blueprint',
    },
    execution_policy: { dry_run_mode: 'full' },
    validation: { should_compile: false, should_save: true },
    behavior,
  };
}

test('compiles edit_object_properties into object_property TaskPlan IR', () => {
  const taskSpec = TaskSpecSchema.parse(baseTaskSpec('edit_object_properties', {
    property_strategy: 'property_edit',
    changes: [
      { kind: 'set_property', property_path: 'DisplayName', value: { kind: 'literal', value_type: 'string', value: 'Door' } },
      { kind: 'set_property', property_path: 'OpenSpeed', value: { kind: 'literal', value_type: 'float', value: 1.5 } },
    ],
  }));

  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  TaskPlanSchema.parse(taskPlan);
  const step = taskPlan.steps[0] as Record<string, any>;
  assert.equal(step.capability, 'object_property');
  assert.equal(step.write.strategy, 'property_edit');
  assert.equal(step.write.ops[0].op, 'set_object_properties');
});

test('compiles manage_blueprinthelper_ownership into graph_cleanup_ownership TaskPlan IR', () => {
  const taskSpec = TaskSpecSchema.parse(baseTaskSpec('manage_blueprinthelper_ownership', {
    ownership_strategy: 'owned_block_lifecycle',
    changes: [
      {
        kind: 'cleanup_block',
        graph_id: 'DoorLogic',
        block_ref: 'OpenDoor0',
        missing_policy: 'ignore',
      },
      {
        kind: 'convert_block_to_user_owned',
        block_id: 'DoorLogic_OpenDoor0',
        already_user_owned_policy: 'ignore',
      },
    ],
  }));

  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  TaskPlanSchema.parse(taskPlan);
  const firstStep = taskPlan.steps[0] as Record<string, any>;
  const secondStep = taskPlan.steps[1] as Record<string, any>;
  assert.equal(taskPlan.steps.length, 2);
  assert.equal(firstStep.capability, 'graph_cleanup_ownership');
  assert.equal(firstStep.write.ops[0].op, 'cleanup_blueprint_helper_block');
  assert.equal(secondStep.write.ops[0].op, 'convert_blueprint_helper_block_to_user_owned');
});
