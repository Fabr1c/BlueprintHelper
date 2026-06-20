import { strict as assert } from 'node:assert';
import test from 'node:test';
import { BRIDGE_RESPONSE_SCHEMA, parseBridgeResponse } from './bridge-response-schema.js';

test('accepts canonical success responses', () => {
  const parsed = parseBridgeResponse({
    schema: BRIDGE_RESPONSE_SCHEMA,
    request_id: 'canonical_success_request',
    success: true,
    result: {
      value: 42,
    },
    transport_timing: {
      parse_ms: 1,
    },
    extra_field: 'ignored',
  });

  assert.deepEqual(parsed, {
    ok: true,
    response: {
      schema: BRIDGE_RESPONSE_SCHEMA,
      request_id: 'canonical_success_request',
      success: true,
      result: {
        value: 42,
      },
      transport_timing: {
        parse_ms: 1,
      },
    },
  });
});

test('accepts canonical failure responses', () => {
  const parsed = parseBridgeResponse({
    schema: BRIDGE_RESPONSE_SCHEMA,
    request_id: 'canonical_failure_request',
    success: false,
    error_code: 'editor_not_ready',
    message: 'Editor is not ready.',
    error: {
      code: 'ignored_nested_error',
      message: 'ignored',
    },
  });

  assert.deepEqual(parsed, {
    ok: true,
    response: {
      schema: BRIDGE_RESPONSE_SCHEMA,
      request_id: 'canonical_failure_request',
      success: false,
      error_code: 'editor_not_ready',
      message: 'Editor is not ready.',
    },
  });
});

test('rejects nested-error-only failures', () => {
  const parsed = parseBridgeResponse({
    schema: BRIDGE_RESPONSE_SCHEMA,
    request_id: 'nested_error_only_request',
    success: false,
    error: {
      code: 'editor_not_ready',
      message: 'Editor is not ready.',
    },
  });

  assert.equal(parsed.ok, false);
  assert.equal(parsed.code, 'bridge_response_invalid');
  assert.match(parsed.message, /error_code/u);
});

test('rejects responses without request_id', () => {
  const parsed = parseBridgeResponse({
    schema: BRIDGE_RESPONSE_SCHEMA,
    success: true,
  });

  assert.deepEqual(parsed, {
    ok: false,
    code: 'bridge_response_invalid',
    field: 'request_id',
    message: 'request_id must be a non-empty string.',
  });
});

test('rejects responses without protocol schema', () => {
  const parsed = parseBridgeResponse({
    request_id: 'req-no-schema',
    success: true,
  });

  assert.deepEqual(parsed, {
    ok: false,
    code: 'bridge_response_invalid',
    field: 'schema',
    message: `schema must equal ${BRIDGE_RESPONSE_SCHEMA}.`,
  });
});
