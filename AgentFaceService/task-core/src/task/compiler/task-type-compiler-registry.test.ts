import assert from 'node:assert/strict';
import test from 'node:test';

import {
  createDefaultTaskTypeCompilerRegistry,
} from './compilers/default-task-type-compilers.js';
import {
  TaskTypeCompilerRegistry,
} from './task-type-compiler-registry.js';

const taskSpec = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P3_Registry',
  context_id: 'ctx-p3-registry',
  target: { asset_path: '/Game/BP_P3_Registry', target_type: 'blueprint' },
  execution_policy: { dry_run_mode: 'quick' },
  validation: { should_compile: false, should_save: false },
  behavior: {
    variable_strategy: 'member_variables',
    changes: [{
      kind: 'ensure_member_variable',
      name: 'P3Value',
      variable_type: { category: 'bool' },
    }],
  },
};

test('TaskTypeCompilerRegistry resolves compiler by task_type', () => {
  const registry = new TaskTypeCompilerRegistry()
    .register({
      id: 'blueprint_variables',
      taskType: 'edit_blueprint_variables',
      canCompile: (candidate): candidate is never => candidate.task_type === 'edit_blueprint_variables',
      compile: () => ({ schema: 'BlueprintHelper.TaskPlan.v1', steps: [] }) as never,
    });

  assert.equal(registry.has('edit_blueprint_variables'), true);
  assert.equal(registry.requireForTaskSpec(taskSpec as never).id, 'blueprint_variables');
});

test('TaskTypeCompilerRegistry rejects duplicate task_type registration', () => {
  const registry = new TaskTypeCompilerRegistry();
  const compiler = {
    id: 'duplicate_variables',
    taskType: 'edit_blueprint_variables',
    canCompile: (candidate: { task_type: string }) => candidate.task_type === 'edit_blueprint_variables',
    compile: () => ({ schema: 'BlueprintHelper.TaskPlan.v1', steps: [] }) as never,
  };

  registry.register(compiler as never);
  assert.throws(() => registry.register(compiler as never), /already registered: edit_blueprint_variables/);
});

test('TaskTypeCompilerRegistry unsupported error preserves TaskSpec compile error shape', () => {
  const registry = new TaskTypeCompilerRegistry();

  assert.throws(
    () => registry.requireForTaskSpec(taskSpec as never),
    /Unsupported TaskSpec task_type: edit_blueprint_variables/,
  );
});

test('compiler helper module exports primitives needed by non-GraphWrite compilers', async () => {
  const helpers = await import('./compiler-helpers.js');

  assert.equal(typeof helpers.makeTaskPlanWithSteps, 'function');
  assert.equal(typeof helpers.getRequiredString, 'function');
  assert.equal(typeof helpers.optionalString, 'function');
  assert.equal(typeof helpers.omitUndefined, 'function');
});

test('default task type registry includes all P3 non-GraphWrite compilers and excludes GraphWrite surfaces', () => {
  const registry = createDefaultTaskTypeCompilerRegistry();

  for (const taskType of [
    'create_asset',
    'edit_blueprint_variables',
    'edit_object_properties',
    'edit_blueprint_signature',
    'edit_blueprint_class_settings',
    'edit_blueprint_components',
    'edit_umg_widget',
    'edit_data_table',
  ]) {
    assert.equal(registry.has(taskType), true, `${taskType} should be registered in P3`);
  }

  assert.equal(registry.has('edit_blueprint_graph'), false);
  assert.equal(registry.has('create_blueprint_feature'), false);
});
