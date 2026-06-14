import assert from 'node:assert/strict';
import test from 'node:test';

import { classifyEditorBlockedError } from './editor-blocked-recovery.js';

test('request timeout is classified as unknown mutation state', () => {
  const result = classifyEditorBlockedError(new Error('Bridge request timed out after 600000ms'));

  assert.equal(result.code, 'unknown_mutation_state');
  assert.equal(result.category, 'editor_command_blocked');
  assert.match(result.recommended_action, /read_context/u);
  assert.match(result.recommended_action, /source-control status/u);
});

test('connect timeout is classified separately from command blockage', () => {
  const result = classifyEditorBlockedError(new Error('Bridge connection timed out after 30000ms'));

  assert.equal(result.code, 'transport_connect_timeout');
  assert.equal(result.category, 'bridge_unavailable');
});
