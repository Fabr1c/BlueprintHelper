import assert from 'node:assert/strict';
import test from 'node:test';

import type { TaskSpec } from '../schema/task-schemas.js';
import { TaskSpecSchema } from '../schema/task-schemas.js';
import { compileTaskSpecToTaskPlan } from './task-compiler.js';
import { createDefaultTaskTypeCompilerRegistry } from './compilers/default-task-type-compilers.js';

test('MaterialInstance task type is registered in the default compiler registry', () => {
  const registry = createDefaultTaskTypeCompilerRegistry();

  assert.equal(registry.has('edit_material_instance'), true);
});

test('MaterialInstance operations compile to one material_instance capability step', () => {
  const plan = compileTaskSpecToTaskPlan(makeMaterialInstanceTaskSpec([
    { op: 'create_material_instance', parent_material: '/Game/Materials/M_Parent' },
    { op: 'set_scalar_override', parameter_name: 'Roughness', value: 0.42 },
    { op: 'set_vector_override', parameter_name: 'BaseColor', value: { r: 0.1, g: 0.2, b: 0.3, a: 1 } },
    { op: 'set_texture_override', parameter_name: 'Albedo', texture_asset: '/Game/Textures/T_Albedo' },
    { op: 'set_static_switch_override', parameter_name: 'UseDetail', value: true },
    { op: 'clear_override', parameter_name: 'UseDetail', parameter_type: 'static_switch' },
    { op: 'read_parameter_schema' },
    { op: 'read_effective_value', parameter_name: 'Roughness', parameter_type: 'scalar' },
  ]));
  const step = plan.steps[0] as Record<string, unknown>;
  const target = step['target'] as Record<string, unknown>;
  const write = step['write'] as Record<string, unknown>;
  const constraints = step['constraints'] as Record<string, unknown>;
  const ops = write['ops'] as Array<Record<string, unknown>>;

  assert.equal(plan.task_type, 'edit_material_instance');
  assert.equal(plan.target_assets[0], '/Game/Materials/MI_Test');
  assert.equal(plan.steps.length, 1);
  assert.equal(step['capability'], 'material_instance');
  assert.equal(target['target_type'], 'material_instance');
  assert.equal(write['strategy'], 'material_instance_edit');
  assert.equal(constraints['asset_kind'], 'material_instance_constant');
  assert.equal(ops.some((op) => op['op'] === 'set_scalar_override' && op['parameter_name'] === 'Roughness'), true);
  assert.equal(ops.some((op) => op['op'] === 'clear_override' && op['parameter_type'] === 'static_switch'), true);
});

test('MaterialInstance schema rejects invalid target type', () => {
  const parsed = TaskSpecSchema.safeParse({
    ...makeMaterialInstanceTaskSpec([{ op: 'read_parameter_schema' }]),
    target: {
      asset_path: '/Game/Materials/MI_Test',
      target_type: 'material',
    },
  });

  assert.equal(parsed.success, false);
});

test('MaterialInstance schema rejects missing parameter name for clear override', () => {
  const parsed = TaskSpecSchema.safeParse(makeMaterialInstanceTaskSpec([
    { op: 'clear_override', parameter_type: 'scalar' },
  ]));

  assert.equal(parsed.success, false);
});

test('MaterialInstance schema rejects override value type mismatches', () => {
  const scalar = TaskSpecSchema.safeParse(makeMaterialInstanceTaskSpec([
    { op: 'set_scalar_override', parameter_name: 'Roughness', value: '0.42' },
  ]));
  const vector = TaskSpecSchema.safeParse(makeMaterialInstanceTaskSpec([
    { op: 'set_vector_override', parameter_name: 'BaseColor', value: { r: 0.1, g: 0.2 } },
  ]));
  const texture = TaskSpecSchema.safeParse(makeMaterialInstanceTaskSpec([
    { op: 'set_texture_override', parameter_name: 'Albedo', texture_asset: 123 },
  ]));
  const staticSwitch = TaskSpecSchema.safeParse(makeMaterialInstanceTaskSpec([
    { op: 'set_static_switch_override', parameter_name: 'UseDetail', value: 'true' },
  ]));

  assert.equal(scalar.success, false);
  assert.equal(vector.success, false);
  assert.equal(texture.success, false);
  assert.equal(staticSwitch.success, false);
});

function makeMaterialInstanceTaskSpec(operations: readonly Record<string, unknown>[]): TaskSpec {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_material_instance_compiler',
    task_type: 'edit_material_instance',
    feature_name: 'MaterialInstanceCompiler',
    target: {
      asset_path: '/Game/Materials/MI_Test',
      target_type: 'material_instance',
    },
    behavior: {
      material_instance_strategy: 'material_instance_edit',
      operations,
    },
  } as TaskSpec;
}
