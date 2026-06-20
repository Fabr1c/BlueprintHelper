import type { BridgeResponse } from './bridge-client.js';

export const BRIDGE_RESPONSE_SCHEMA = 'BlueprintHelper.BridgeResponse.v1';

export type BridgeResponseParseFailure = {
  ok: false;
  code: 'bridge_response_invalid';
  field: string;
  message: string;
};

type BridgeResponseParseSuccess = {
  ok: true;
  response: BridgeResponse;
};

export type BridgeResponseParseResult = BridgeResponseParseSuccess | BridgeResponseParseFailure;

export function parseBridgeResponse(value: unknown): BridgeResponseParseResult {
  if (!isObjectRecord(value)) {
    return invalidBridgeResponse('response', 'response must be an object.');
  }

  if (value.schema !== BRIDGE_RESPONSE_SCHEMA) {
    return invalidBridgeResponse('schema', `schema must equal ${BRIDGE_RESPONSE_SCHEMA}.`);
  }

  if (!isNonEmptyString(value.request_id)) {
    return invalidBridgeResponse('request_id', 'request_id must be a non-empty string.');
  }

  if (typeof value.success !== 'boolean') {
    return invalidBridgeResponse('success', 'success must be a boolean.');
  }

  if (!value.success) {
    if (!isNonEmptyString(value.error_code)) {
      return invalidBridgeResponse('error_code', 'error_code is required when success is false.');
    }
    if (!isNonEmptyString(value.message)) {
      return invalidBridgeResponse('message', 'message is required when success is false.');
    }
  }

  return {
    ok: true,
    response: normalizeBridgeResponse(value),
  };
}

function invalidBridgeResponse(field: string, message: string): BridgeResponseParseFailure {
  return {
    ok: false,
    code: 'bridge_response_invalid',
    field,
    message,
  };
}

function isObjectRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function isNonEmptyString(value: unknown): value is string {
  return typeof value === 'string' && value.length > 0;
}

function normalizeBridgeResponse(value: Record<string, unknown>): BridgeResponse {
  const response: BridgeResponse = {
    schema: BRIDGE_RESPONSE_SCHEMA,
    request_id: value.request_id as string,
    success: value.success as boolean,
  };

  if (typeof value.error_code === 'string') {
    response.error_code = value.error_code;
  }

  if (typeof value.message === 'string') {
    response.message = value.message;
  }

  if (isObjectRecord(value.result)) {
    response.result = value.result;
  }

  if (isObjectRecord(value.transport_timing)) {
    response.transport_timing = value.transport_timing;
  }

  return response;
}
