import { strict as assert } from 'node:assert';
import test from 'node:test';

import { TaskSpecSchema } from './task-schemas.js';

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
