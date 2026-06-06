import assert from 'node:assert/strict';
import test from 'node:test';

import type { TaskSpec } from '../../schema/task-schemas.js';
import { compileTaskSpecToTaskPlan } from '../task-compiler.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import { compositeBlueprintFeatureTaskCompiler } from './composite-blueprint-feature-task-compiler.js';

function makeCompositeSpec(overrides: Partial<Record<string, unknown>> = {}): Extract<TaskSpec, { task_type: 'create_blueprint_feature' }> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'create_blueprint_feature',
    feature_name: 'CompositeFeatureCompilerTest',
    context_id: 'ctx_composite_feature_compiler_test',
    target: { asset_path: '/Game/BP_CompositeFeatureCompilerTest', target_type: 'blueprint' },
    execution_policy: { dry_run_mode: 'full' },
    validation: { should_compile: false, should_save: false },
    components: [{
      name: 'StatusRoot',
      class: '/Script/Engine.SceneComponent',
      attach_rule: 'keep_relative',
      properties: {
        RelativeLocation: { kind: 'literal', value: '0,0,0' },
      },
    }],
    variables: [{
      name: 'bCompositeReady',
      type: 'bool',
      default: { kind: 'literal', value: true },
    }],
    class_settings: {
      class_defaults: {
        Tags: [{ kind: 'literal', value: 'CompositeFeatureCompilerTest' }],
      },
    },
    ...overrides,
  } as Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>;
}

test('composite compiler builds TaskPlan through shared builder', () => {
  const plan = compositeBlueprintFeatureTaskCompiler.compile(makeCompositeSpec(), { source: 'facade' });

  assert.equal(plan.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.equal(plan.task_type, 'create_blueprint_feature');
  assert.deepEqual(plan.target_assets, ['/Game/BP_CompositeFeatureCompilerTest']);
  assert.equal(plan.execution_policy.dry_run_mode, 'full');
  assert.deepEqual(plan.steps.map((step) => step.step_id), ['step_001', 'step_002', 'step_003', 'step_004', 'step_005']);
  assert.equal(plan.steps.some((step) => 'capability' in step && step.capability === 'blueprint_component'), true);
  assert.equal(plan.steps.some((step) => 'capability' in step && step.capability === 'blueprint_variable'), true);
  assert.equal(plan.steps.some((step) => 'capability' in step && step.capability === 'blueprint_class_settings'), true);
});

test('facade compiles create_blueprint_feature through registered composite compiler', () => {
  const plan = compileTaskSpecToTaskPlan(makeCompositeSpec());

  assert.equal(plan.task_type, 'create_blueprint_feature');
  assert.equal(plan.steps[0]?.step_id, 'step_001');
});

test('composite compiler preserves existing unsupported integration error', () => {
  assert.throws(
    () => compositeBlueprintFeatureTaskCompiler.compile(makeCompositeSpec({
      integration: { input: { action: 'Jump' } },
    }), { source: 'facade' }),
    (error: unknown) => {
      assert.equal(error instanceof TaskSpecCompileError, true);
      assert.equal((error as TaskSpecCompileError).code, 'unsupported_composite_integration');
      return true;
    },
  );
});
