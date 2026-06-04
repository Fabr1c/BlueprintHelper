import assert from 'node:assert/strict';
import test from 'node:test';

import {
  BlueprintPinTypeSpecSchema,
  SignaturePinSpecSchema,
} from './blueprint-pin-type-spec.js';
import { TaskSpecSchema } from './task-schemas.js';

test('BlueprintPinTypeSpec accepts scalar, object, soft class, struct, enum, and map value type', () => {
  const samples = [
    { category: 'bool' },
    { category: 'int' },
    { category: 'double' },
    { category: 'object', object_path: '/Script/Engine.Actor' },
    { category: 'soft_class', object_path: '/Script/Engine.Actor' },
    { category: 'struct', object_path: '/Script/CoreUObject.Vector' },
    { category: 'enum', object_path: '/Script/Engine.ECollisionChannel' },
    { category: 'string', container_type: 'map', value_type: { category: 'int' } },
  ];

  for (const sample of samples) {
    assert.equal(BlueprintPinTypeSpecSchema.safeParse(sample).success, true, JSON.stringify(sample));
  }
});

test('BlueprintPinTypeSpec rejects legacy string tokens and nested map value containers', () => {
  assert.equal(BlueprintPinTypeSpecSchema.safeParse('int').success, false);
  assert.equal(BlueprintPinTypeSpecSchema.safeParse('category=wildcard|container=map').success, false);
  assert.equal(BlueprintPinTypeSpecSchema.safeParse({
    category: 'string',
    container_type: 'map',
    value_type: { category: 'int', container_type: 'array' },
  }).success, false);
});

test('SignaturePinSpec requires name and structured pin_type', () => {
  assert.equal(SignaturePinSpecSchema.safeParse({
    name: 'Count',
    pin_type: { category: 'int' },
    default_value: { mode: 'literal', value: '0' },
  }).success, true);

  assert.equal(SignaturePinSpecSchema.safeParse({
    name: 'Count',
    pin_type: 'int',
  }).success, false);
});

test('edit_blueprint_signature rejects duplicate signature pin names in inputs and outputs', () => {
  for (const field of ['inputs', 'outputs'] as const) {
    const parsed = TaskSpecSchema.safeParse({
      schema: 'BlueprintHelper.TaskSpec.v1',
      task_type: 'edit_blueprint_signature',
      target: { asset_path: '/Game/Test/BP_Signature' },
      behavior: {
        signature_strategy: 'signature_edit',
        changes: [{
          kind: 'ensure_function',
          function_name: 'DoThing',
          [field]: [
            { name: 'Count', pin_type: { category: 'int' } },
            { name: 'Count', pin_type: { category: 'int' } },
          ],
        }],
      },
    });

    assert.equal(parsed.success, false, field);
  }
});
