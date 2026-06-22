import assert from 'node:assert/strict';
import { test } from 'node:test';
import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';
import { TaskSpecSchema } from '../schema/task-schemas.js';

function makeSignatureSpec(changes: Array<Record<string, unknown>>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_signature',
    target: {
      target_type: 'blueprint',
      asset_path: '/Game/Test/BP_Signature.BP_Signature',
    },
    behavior: {
      signature_strategy: 'signature_edit',
      changes,
    },
  };
}

test('edit_blueprint_signature lowers typed function inputs and outputs', () => {
  const plan = compileTaskSpecToTaskPlan(makeSignatureSpec([{
    kind: 'ensure_function',
    function_name: 'ComputeScore',
    inputs: [{ name: 'BaseScore', pin_type: { category: 'int' } }],
    outputs: [{ name: 'FinalScore', pin_type: { category: 'int' } }],
    is_pure: true,
  }]) as never);

  const step = plan.steps[0] as Record<string, unknown>;
  const write = step.write as Record<string, unknown>;
  const op = (write.ops as Array<Record<string, unknown>>)[0];
  assert.equal(step.capability, 'blueprint_signature');
  assert.equal(write.strategy, 'function_signature');
  assert.equal(op.op, 'ensure_function');
  assert.deepEqual(op.inputs, [{ name: 'BaseScore', pin_type: { category: 'int' } }]);
  assert.deepEqual(op.outputs, [{ name: 'FinalScore', pin_type: { category: 'int' } }]);
});

test('ensure_function preserves signature mismatch policy for UE structured differences', () => {
  const plan = compileTaskSpecToTaskPlan(makeSignatureSpec([{
    kind: 'ensure_function',
    function_name: 'ComputeScore',
    inputs: [{ name: 'BaseScore', pin_type: { category: 'int' } }],
    outputs: [{ name: 'FinalScore', pin_type: { category: 'int' } }],
    signature_mismatch_policy: 'block',
  }]) as never);

  const op = (((plan.steps[0] as Record<string, unknown>).write as Record<string, unknown>).ops as Array<Record<string, unknown>>)[0];
  assert.equal(op.signature_mismatch_policy, 'block');
});

test('edit_blueprint_signature rejects legacy string pin_type', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeSignatureSpec([{
      kind: 'ensure_function',
      function_name: 'BadFunction',
      inputs: [{ name: 'Count', pin_type: 'int' }],
    }]) as never),
    (error: unknown) => error instanceof TaskSpecCompileError
      && error.code === 'legacy_pin_type_token_unsupported'
      && error.issues[0]?.path === 'behavior.changes[0].inputs[0].pin_type',
  );
});

test('edit_blueprint_signature lowers macro signature inputs and outputs', () => {
  const plan = compileTaskSpecToTaskPlan(makeSignatureSpec([{
    kind: 'ensure_macro',
    macro_name: 'ClampScore',
    inputs: [{ name: 'Execute', pin_type: { category: 'exec' } }],
    outputs: [{ name: 'Then', pin_type: { category: 'exec' } }],
  }]) as never);

  const step = plan.steps[0] as Record<string, unknown>;
  const write = step.write as Record<string, unknown>;
  const op = (write.ops as Array<Record<string, unknown>>)[0];
  assert.equal(step.capability, 'blueprint_signature');
  assert.equal(write.strategy, 'macro_signature');
  assert.equal(op.op, 'ensure_macro');
  assert.equal(op.macro_name, 'ClampScore');
  assert.deepEqual(op.inputs, [{ name: 'Execute', pin_type: { category: 'exec' } }]);
  assert.deepEqual(op.outputs, [{ name: 'Then', pin_type: { category: 'exec' } }]);
});

test('remove_signature defaults require_reference_context to true', () => {
  const plan = compileTaskSpecToTaskPlan(makeSignatureSpec([{
    kind: 'remove_signature',
    signature_kind: 'function',
    signature_name: 'ComputeScore',
  }]) as never);

  const op = (((plan.steps[0] as Record<string, unknown>).write as Record<string, unknown>).ops as Array<Record<string, unknown>>)[0];
  assert.equal(op.require_reference_context, true);
});

test('remove_signature lowers reference guidance policy fields', () => {
  const plan = compileTaskSpecToTaskPlan(makeSignatureSpec([{
    kind: 'remove_signature',
    signature_kind: 'function',
    signature_name: 'ComputeScore',
    execute_policy: 'execute_if_unreferenced',
  }]) as never);

  const op = (((plan.steps[0] as Record<string, unknown>).write as Record<string, unknown>).ops as Array<Record<string, unknown>>)[0];
  assert.equal(op.require_reference_context, true);
  assert.equal(op.execute_policy, 'execute_if_unreferenced');
});

test('remove_signature rejects legacy event_name-only lowering without explicit signature selectors', () => {
  assert.throws(
    () => {
      const taskSpec = TaskSpecSchema.parse(makeSignatureSpec([{
        kind: 'remove_signature',
        event_name: 'ReceiveBeginPlay',
      }]));
      compileTaskSpecToTaskPlan(taskSpec as never);
    },
    (error: unknown) => error instanceof Error
      && /signature_remove_kind_required|signature_remove_name_required/.test(error.message),
  );
});

test('remove_signature lowers native_event from explicit signature kind and name', () => {
  const plan = compileTaskSpecToTaskPlan(makeSignatureSpec([{
    kind: 'remove_signature',
    signature_kind: 'native_event',
    signature_name: 'ReceiveBeginPlay',
    execute_policy: 'execute_if_unreferenced',
  }]) as never);

  const step = plan.steps[0] as Record<string, unknown>;
  const write = step.write as Record<string, unknown>;
  const op = (write.ops as Array<Record<string, unknown>>)[0];
  assert.equal(step.capability, 'blueprint_signature');
  assert.equal(write.strategy, 'override_event_signature');
  assert.equal(op.op, 'remove_signature');
  assert.equal(op.signature_kind, 'native_event');
  assert.equal(op.signature_name, 'ReceiveBeginPlay');
  assert.equal(op.execute_policy, 'execute_if_unreferenced');
  assert.equal(op.require_reference_context, true);
});
