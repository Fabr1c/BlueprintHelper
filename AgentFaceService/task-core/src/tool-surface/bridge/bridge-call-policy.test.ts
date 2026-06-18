import assert from 'node:assert/strict';
import test from 'node:test';

import {
  getCliBridgeCallPolicy,
  isCliBridgeCallAllowed,
  listCliBridgeCallPolicies,
} from './bridge-call-policy.js';

test('CLI raw Bridge call policy is expert-only and separate from tool descriptors', () => {
  assert.equal(getCliBridgeCallPolicy('get_editor_context').policy, 'expert_only');
  assert.equal(getCliBridgeCallPolicy('read_reference_context').policy, 'expert_only');
  assert.equal(getCliBridgeCallPolicy('create_asset').policy, 'forbidden');
  assert.equal(isCliBridgeCallAllowed('get_editor_context'), true);
  assert.equal(isCliBridgeCallAllowed('read_reference_context'), true);
  assert.equal(isCliBridgeCallAllowed('get_task_run_journal'), true);
  assert.equal(isCliBridgeCallAllowed('create_asset'), false);

  const policies = listCliBridgeCallPolicies();
  assert.equal(policies.some((descriptor) =>
    descriptor.bridge_command === 'get_editor_context' && descriptor.source === 'descriptor'), true);
  assert.equal(policies.some((descriptor) =>
    descriptor.bridge_command === 'read_reference_context' && descriptor.source === 'read_context_route'), true);
});
