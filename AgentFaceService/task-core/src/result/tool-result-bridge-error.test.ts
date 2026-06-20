import test from 'node:test';
import assert from 'node:assert/strict';

import { BRIDGE_RESPONSE_SCHEMA } from '../bridge/bridge-response-schema.js';
import { TOOL_RESULT_SCHEMA, normalizeToolResult } from './tool-result.js';

test('normalizeToolResult preserves canonical bridge error defaults for editor_not_ready', () => {
  const result = normalizeToolResult(
    {
      schema: BRIDGE_RESPONSE_SCHEMA,
      request_id: 'bridge_ping_1',
      success: false,
      error_code: 'editor_not_ready',
      message: 'Editor is not ready.',
    },
    'bridge.ping',
  );

  assert.equal(result.ok, false);
  assert.equal(result.schema, TOOL_RESULT_SCHEMA);
  assert.equal(result.operation, 'bridge.ping');
  assert.equal(result.status, 'failed');
  assert.equal(result.error?.code, 'editor_not_ready');
  assert.equal(result.error?.message, 'Editor is not ready.');
  assert.equal(result.error?.stage, 'bridge');
  assert.equal(result.error?.category, 'runtime_unavailable');
  assert.equal(result.error?.retryable, true);
  assert.equal(result.error?.safe_next_action, 'editor.open_or_wait_then_retry');
  assert.deepEqual(result.error?.allowed_recovery_actions, ['editor.open', 'bridge.ping']);
});

test('normalizeToolResult treats non-retryable Bridge command failures as blocked user action', () => {
  const result = normalizeToolResult(
    {
      schema: BRIDGE_RESPONSE_SCHEMA,
      request_id: 'req_unauthorized',
      success: false,
      error_code: 'unauthorized',
      message: 'Write session is not authorized.',
    },
    'bridge.execute_task_plan',
  );

  assert.equal(result.ok, false);
  assert.equal(result.error?.category, 'authorization_error');
  assert.equal(result.error?.retryable, false);
  assert.equal(result.error?.safe_next_action, 'request_write_session_before_retry');
  assert.deepEqual(result.error?.allowed_recovery_actions, ['request_write_session']);
});

for (const testCase of [
  {
    code: 'asset_not_found',
    category: 'target_resolution_error',
    safeNextAction: 'read_context_or_correct_target_then_retry',
    allowedRecoveryActions: ['context.read', 'task.preview'],
  },
  {
    code: 'graph_not_found',
    category: 'target_resolution_error',
    safeNextAction: 'read_context_or_correct_target_then_retry',
    allowedRecoveryActions: ['context.read', 'task.preview'],
  },
  {
    code: 'command_disabled',
    category: 'capability_unavailable',
    safeNextAction: 'use_tools_catalog_then_retry',
    allowedRecoveryActions: ['tools.list', 'tools.templates'],
  },
  {
    code: 'unknown_command',
    category: 'capability_unavailable',
    safeNextAction: 'use_tools_catalog_then_retry',
    allowedRecoveryActions: ['tools.list', 'tools.templates'],
  },
  {
    code: 'execution_failed',
    category: 'bridge_error',
    safeNextAction: 'inspect_debug_bundle_before_retry',
    allowedRecoveryActions: ['diagnostics.runtime'],
  },
] as const) {
  test(`normalizeToolResult classifies ${testCase.code} bridge failures`, () => {
    const result = normalizeToolResult(
      {
        schema: BRIDGE_RESPONSE_SCHEMA,
        request_id: `req_${testCase.code}`,
        success: false,
        error_code: testCase.code,
        message: `${testCase.code} message.`,
      },
      'bridge.call',
    );

    assert.equal(result.ok, false);
    assert.equal(result.error?.code, testCase.code);
    assert.equal(result.error?.category, testCase.category);
    assert.equal(result.error?.retryable, false);
    assert.equal(result.error?.safe_next_action, testCase.safeNextAction);
    assert.deepEqual(result.error?.allowed_recovery_actions, testCase.allowedRecoveryActions);
  });
}

test('normalizeToolResult falls back to bridge_error when Bridge failure omits error_code', () => {
  const result = normalizeToolResult(
    {
      schema: BRIDGE_RESPONSE_SCHEMA,
      request_id: 'req_missing_error_code',
      success: false,
      message: 'Bridge request failed without an explicit error code.',
    },
    'bridge.call',
  );

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'bridge_error');
  assert.equal(result.error?.category, 'bridge_error');
  assert.equal(result.error?.retryable, false);
  assert.equal(result.error?.safe_next_action, 'inspect_debug_bundle_before_retry');
  assert.deepEqual(result.error?.allowed_recovery_actions, ['diagnostics.runtime']);
});
