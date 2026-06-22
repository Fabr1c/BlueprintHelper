import { strict as assert } from 'node:assert';
import test from 'node:test';

import { BlueprintSignatureTaskSpecSchema, TaskSpecSchema } from './task-schemas.js';

const minimalTaskSpec = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  task_type: 'edit_blueprint_variables',
  target: {
    asset_path: '/Game/BH_Tests/BP_AgentFacingPolicy',
    target_type: 'blueprint',
  },
  behavior: {
    variable_strategy: 'member_variables',
    changes: [{
      kind: 'ensure_member_variable',
      name: 'PolicyValue',
      variable_type: { category: 'bool' },
    }],
  },
};

test('Agent-facing TaskSpec rejects runtime policy namespaces', () => {
  for (const field of ['execution_policy', 'validation'] as const) {
    const result = TaskSpecSchema.safeParse({
      ...minimalTaskSpec,
      [field]: {},
    });

    assert.equal(result.success, false, field);
    if (!result.success) {
      assert.match(JSON.stringify(result.error.issues), new RegExp(field));
    }
  }
});

test('Blueprint signature schema rejects remove_signature legacy event_name-only payloads', () => {
  const result = BlueprintSignatureTaskSpecSchema.safeParse({
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_signature',
    target: {
      asset_path: '/Game/BH_Tests/BP_AgentFacingPolicy',
      target_type: 'blueprint',
    },
    behavior: {
      signature_strategy: 'signature_edit',
      changes: [{
        kind: 'remove_signature',
        event_name: 'ReceiveBeginPlay',
      }],
    },
  });

  assert.equal(result.success, false);
  if (!result.success) {
    const issues = JSON.stringify(result.error.issues);
    assert.match(issues, /signature_remove_kind_required: remove_signature requires signature_kind\./);
    assert.match(issues, /signature_remove_name_required: remove_signature requires signature_name\./);
    assert.match(issues, /signature_remove_legacy_name_fields_removed: use signature_name with signature_kind\./);
  }
});
