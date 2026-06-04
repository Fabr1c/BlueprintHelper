import assert from 'node:assert/strict';
import { test } from 'node:test';
import { inferTaskResultIntentForTest } from './task-result-store.js';

test('task result store maps edit_blueprint_signature to blueprint_signature intent', () => {
  const taskPlan = {
    schema: 'BlueprintHelper.TaskPlan.v1',
    task_type: 'edit_blueprint_signature',
    task_name: 'SignatureIntent',
    target_assets: ['/Game/Test/BP_Signature.BP_Signature'],
    execution_policy: {
      dry_run_mode: 'full',
      should_compile: false,
      should_save: false,
      review_baseline_dirty_asset_policy: 'block',
    },
    steps: [
      {
        step_id: 'step_001',
        capability: 'blueprint_signature',
        target: {
          asset_path: '/Game/Test/BP_Signature.BP_Signature',
        },
        write: {
          strategy: 'function_signature',
          ops: [
            {
              op: 'ensure_function',
              function_name: 'ComputeScore',
              inputs: [{ name: 'BaseScore', pin_type: { category: 'int' } }],
              outputs: [{ name: 'FinalScore', pin_type: { category: 'int' } }],
            },
          ],
        },
      },
    ],
  };

  const intent = inferTaskResultIntentForTest(taskPlan as never);
  assert.equal(intent.capability, 'blueprint_signature');
  assert.match(intent.generatedIntent, /BlueprintSignature/);
  assert.match(intent.generatedIntent, /ComputeScore/);
});
