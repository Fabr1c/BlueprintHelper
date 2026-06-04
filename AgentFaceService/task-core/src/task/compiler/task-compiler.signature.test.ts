import assert from 'node:assert/strict';
import { test } from 'node:test';
import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';

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
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
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

test('remove_signature defaults require_reference_context to true', () => {
  const plan = compileTaskSpecToTaskPlan(makeSignatureSpec([{
    kind: 'remove_signature',
    signature_kind: 'function',
    signature_name: 'ComputeScore',
  }]) as never);

  const op = (((plan.steps[0] as Record<string, unknown>).write as Record<string, unknown>).ops as Array<Record<string, unknown>>)[0];
  assert.equal(op.require_reference_context, true);
});
