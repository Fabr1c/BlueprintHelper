import { failureResult, normalizeToolResult, successRead, type ToolResultBase } from '../../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { extractBridgePayload, isRecord, normalizeBridgeToolResult } from '../bridge-tool-result-utils.js';
import {
  buildBlueprintLogicReadPayload,
  buildReadContextBridgeRequest,
  buildReadContextSchemaPayload,
  inferBlueprintLogicScope,
  isTargetEntryLogicRead,
  normalizeReadContextFormat,
} from './read-context-route-builder.js';
import { buildReadContextTarget } from './read-context-target.js';
import { deriveReadContextStats, postProcessReadContextPayload } from './read-context-payload.js';
import { ReadContextInputSchema, type ReadContextInput } from './read-context-schemas.js';

export async function executeReadContext(
  rawInput: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const input = ReadContextInputSchema.parse(rawInput);
  const requestedFormat = input.view?.format ?? 'logic_md';
  const format = normalizeReadContextFormat(input, requestedFormat);
  if (format === 'schema') {
    return successRead('read_context', buildReadContextTarget(input), {
      schema: 'ReadContextPack.v1',
      read_type: input.read_type,
      format,
      payload: buildReadContextSchemaPayload(),
      stats: {},
      truncated: false,
    }) as ToolResultBase;
  }

  if (input.read_type !== 'blueprint_logic' && input.read_type !== 'graph_context') {
    return executeBridgeBackedReadContext(input, context, format);
  }

  const bridgeFormat = format === 'logic_json' || format === 'summary' || isTargetEntryLogicRead(input)
    ? 'logic_json'
    : 'logic_md';
  const response = await context.bridge.sendCommand(
    bridgeFormat === 'logic_json' ? 'read_blueprint_logic_json' : 'read_blueprint_logic_md',
    buildBlueprintLogicReadPayload(input),
  );
  if (!response.success) {
    return normalizeBridgeToolResult('read_context', response);
  }

  const payloadResult = extractBridgePayload(response.result);
  if (!payloadResult.ok) {
    return failureResult('read_context', {
      code: 'invalid_read_context_payload',
      stage: 'bridge',
      message: payloadResult.message,
      retryable: false,
      rollback_result: 'not_needed',
    });
  }
  const payload = payloadResult.payload;
  return successRead('read_context', buildReadContextTarget(input), {
    schema: 'ReadContextPack.v1',
    read_type: input.read_type,
    format,
    scope: payload['scope'] ?? inferBlueprintLogicScope(input),
    payload,
    stats: isRecord(payload['stats']) ? payload['stats'] : {},
    truncated: false,
  }) as ToolResultBase;
}

async function executeBridgeBackedReadContext(
  input: ReadContextInput,
  context: BlueprintHelperToolContext,
  format: string,
): Promise<ToolResultBase> {
  const request = buildReadContextBridgeRequest(input);
  if (!request.ok) {
    return failureResult('read_context', {
      code: request.code,
      stage: 'parse_input',
      message: request.message,
      retryable: false,
      rollback_result: 'not_needed',
    }, buildReadContextTarget(input) as never);
  }

  const response = await context.bridge.sendCommand(request.command, request.payload);
  if (!response.success) {
    return normalizeToolResult(response, 'read_context', {
      target: buildReadContextTarget(input),
      error: {
        code: response.error_code ?? 'read_context_bridge_error',
        stage: 'bridge',
        message: response.message ?? `${request.command} failed.`,
      },
    });
  }

  const payloadResult = extractBridgePayload(response.result);
  if (!payloadResult.ok) {
    return failureResult('read_context', {
      code: 'invalid_read_context_payload',
      stage: 'bridge',
      message: payloadResult.message,
      retryable: false,
      rollback_result: 'not_needed',
    }, buildReadContextTarget(input) as never);
  }

  const payload = postProcessReadContextPayload(input, request.payloadSchema, payloadResult.payload);
  return successRead('read_context', buildReadContextTarget(input), {
    schema: 'ReadContextPack.v1',
    read_type: input.read_type,
    format,
    scope: request.scope,
    payload,
    stats: deriveReadContextStats(input, payload),
    truncated: false,
  }) as ToolResultBase;
}
