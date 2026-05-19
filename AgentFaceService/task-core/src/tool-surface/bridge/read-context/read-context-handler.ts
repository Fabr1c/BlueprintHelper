import { failureResult, normalizeToolResult, successRead, type ToolResultBase } from '../../../result/tool-result.js';
import {
  addNestedTaskTiming,
  extractBridgeTiming,
  measureTaskTiming,
  measureTaskTimingAsync,
} from '../../../task/service/task-timing.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { extractBridgePayload, normalizeBridgeToolResult } from '../bridge-tool-result-utils.js';
import {
  buildReadContextLogicBridgeRoute,
  buildBlueprintLogicReadPayload,
  buildReadContextBridgeRequest,
  resolveReadContextLogicFormat,
} from './read-context-route-builder.js';
import { buildReadContextTarget } from './read-context-target.js';
import {
  postProcessReadContextPayload,
  resolveReadContextPostProcessStage,
} from './read-context-payload.js';
import { ReadContextInputSchema, type ReadContextInput } from './read-context-schemas.js';

export async function executeReadContext(
  rawInput: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const timing = context.timing;
  const input = measureTaskTiming(timing, 'read_context.parse_input', () => (
    ReadContextInputSchema.parse(rawInput)
  ));
  const format = measureTaskTiming(timing, 'read_context.resolve_format', () => (
    resolveReadContextLogicFormat(input)
  ));

  if (input.read_type !== 'blueprint_logic' && input.read_type !== 'graph_context') {
    return executeBridgeBackedReadContext(input, context);
  }

  const bridgeFormat = format ?? 'logic_md';
  const route = measureTaskTiming(timing, 'read_context.resolve_bridge_request', () => (
    buildReadContextLogicBridgeRoute(bridgeFormat)
  ));
  const payload = measureTaskTiming(timing, 'read_context.build_bridge_payload', () => (
    withReadTimingPayload(buildBlueprintLogicReadPayload(input), context)
  ));
  const response = await measureTaskTimingAsync(timing, `read_context.bridge.${route.command}`, () => (
    context.bridge.sendCommand(route.command, payload)
  ));
  addNestedTaskTiming(timing, `ue.${route.command}`, extractBridgeTiming(response.result));
  if (!response.success) {
    return normalizeBridgeToolResult('read_context', response);
  }

  const payloadResult = measureTaskTiming(timing, 'read_context.extract_bridge_payload', () => (
    extractBridgePayload(response.result)
  ));
  if (!payloadResult.ok) {
    return failureResult('read_context', {
      code: 'invalid_read_context_payload',
      stage: 'bridge',
      message: payloadResult.message,
      retryable: false,
      rollback_result: 'not_needed',
    });
  }
  const readPayload = buildReadPayloadWithTiming(input, route.payloadSchema, payloadResult.payload, context);
  return measureTaskTiming(timing, 'read_context.result_wrap', () => successRead('read_context', buildReadContextTarget(input), {
    schema: 'ReadContextPack.v1',
    payload: readPayload,
    truncated: false,
  }) as ToolResultBase);
}

async function executeBridgeBackedReadContext(
  input: ReadContextInput,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const timing = context.timing;
  const request = measureTaskTiming(timing, 'read_context.resolve_bridge_request', () => (
    buildReadContextBridgeRequest(input)
  ));
  if (!request.ok) {
    return failureResult('read_context', {
      code: request.code,
      stage: 'parse_input',
      message: request.message,
      retryable: false,
      rollback_result: 'not_needed',
    }, buildReadContextTarget(input) as never);
  }

  const payload = measureTaskTiming(timing, 'read_context.build_bridge_payload', () => (
    withReadTimingPayload(request.payload, context)
  ));
  const response = await measureTaskTimingAsync(timing, `read_context.bridge.${request.command}`, () => (
    context.bridge.sendCommand(request.command, payload)
  ));
  addNestedTaskTiming(timing, `ue.${request.command}`, extractBridgeTiming(response.result));
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

  const payloadResult = measureTaskTiming(timing, 'read_context.extract_bridge_payload', () => (
    extractBridgePayload(response.result)
  ));
  if (!payloadResult.ok) {
    return failureResult('read_context', {
      code: 'invalid_read_context_payload',
      stage: 'bridge',
      message: payloadResult.message,
      retryable: false,
      rollback_result: 'not_needed',
    }, buildReadContextTarget(input) as never);
  }

  const readPayload = buildReadPayloadWithTiming(input, request.payloadSchema, payloadResult.payload, context);
  return measureTaskTiming(timing, 'read_context.result_wrap', () => successRead('read_context', buildReadContextTarget(input), {
    schema: 'ReadContextPack.v1',
    payload: readPayload,
    truncated: false,
  }) as ToolResultBase);
}

function withReadTimingPayload(
  payload: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Record<string, unknown> {
  return context.timing ? { ...payload, include_timing: true } : payload;
}

function buildReadPayloadWithTiming(
  input: ReadContextInput,
  payloadSchema: string,
  payload: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Record<string, unknown> {
  const timing = context.timing;
  const strippedPayload = measureTaskTiming(timing, 'read_context.strip_bridge_timing', () => (
    stripTimingPayload(payload)
  ));
  return measureTaskTiming(timing, resolveReadContextPostProcessStage(payloadSchema), () => (
    postProcessReadContextPayload(input, payloadSchema, strippedPayload)
  ));
}

function stripTimingPayload(payload: Record<string, unknown>): Record<string, unknown> {
  if (!Object.hasOwn(payload, 'timing')) {
    return payload;
  }

  const stripped = { ...payload };
  delete stripped['timing'];
  return stripped;
}
