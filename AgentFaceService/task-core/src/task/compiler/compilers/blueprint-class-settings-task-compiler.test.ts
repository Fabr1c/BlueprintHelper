import assert from 'node:assert/strict';
import test from 'node:test';

import type { TaskSpec } from '../../schema/task-schemas.js';
import { TaskSpecSchema } from '../../schema/task-schemas.js';
import { compileTaskSpecToTaskPlan } from '../task-compiler.js';
import { blueprintClassSettingsTaskCompiler } from './blueprint-class-settings-task-compiler.js';

function makeClassSettingsSpec(
  overrides: Partial<Extract<TaskSpec, { task_type: 'edit_blueprint_class_settings' }>> = {},
): Extract<TaskSpec, { task_type: 'edit_blueprint_class_settings' }> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_class_settings',
    feature_name: 'ClassSettingsCompilerTest',
    context_id: 'ctx_class_settings_compiler_test',
    target: {
      asset_path: '/Game/BH_Tests/BP_ClassSettingsCompilerTest',
      target_type: 'blueprint',
    },
    behavior: {
      class_settings_strategy: 'class_settings',
      class_defaults: [{
        property_path: 'Mesh.SkeletalMeshAsset',
        value: '/Game/Test/SKM_Test.SKM_Test',
      }],
    },
    ...overrides,
  } as Extract<TaskSpec, { task_type: 'edit_blueprint_class_settings' }>;
}

test('blueprint class settings compiler preserves class default mutation_strategy', () => {
  const spec = makeClassSettingsSpec({
    behavior: {
      class_settings_strategy: 'class_settings',
      class_defaults: [{
        property_path: 'Mesh.SkeletalMeshAsset',
        value: '/Game/Test/SKM_Test.SKM_Test',
        mutation_strategy: 'setter_aware_property',
      }],
    },
  });

  const plan = blueprintClassSettingsTaskCompiler.compile(spec, { source: 'facade' });
  const settings = (((plan.steps[0] as { write: { ops: Array<{ settings?: Record<string, unknown>[] }> } })
    .write.ops[0]?.settings) ?? []);

  assert.equal(settings[0]?.['mutation_strategy'], 'setter_aware_property');
});

test('TaskSpec schema accepts class default mutation_strategy enum values', () => {
  for (const mutationStrategy of ['direct_property', 'setter_aware_property'] as const) {
    const result = TaskSpecSchema.safeParse(makeClassSettingsSpec({
      behavior: {
        class_settings_strategy: 'class_settings',
        class_defaults: [{
          property_path: 'Mesh.SkeletalMeshAsset',
          value: '/Game/Test/SKM_Test.SKM_Test',
          mutation_strategy: mutationStrategy,
        }],
      },
    }));

    assert.equal(result.success, true, mutationStrategy);
  }
});

test('TaskSpec schema rejects unsupported class default mutation_strategy', () => {
  const result = TaskSpecSchema.safeParse(makeClassSettingsSpec({
    behavior: {
      class_settings_strategy: 'class_settings',
      class_defaults: [{
        property_path: 'Mesh.SkeletalMeshAsset',
        value: '/Game/Test/SKM_Test.SKM_Test',
        mutation_strategy: 'legacy_shortcut',
      }],
    },
  }));

  assert.equal(result.success, false);
  assert.match(JSON.stringify(result.error.issues), /mutation_strategy/);
});

test('facade compiles Blueprint class settings through registered compiler', () => {
  const plan = compileTaskSpecToTaskPlan(makeClassSettingsSpec());

  assert.equal(plan.task_type, 'edit_blueprint_class_settings');
  assert.equal(plan.steps[0]?.step_id, 'step_001');
});
