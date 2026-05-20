import { failureResult, normalizeToolResult, successRead, type ToolResultBase } from '../../../result/tool-result.js';
import {
  addTaskTimingMarker,
  addNestedTaskTiming,
  extractBridgeTiming,
  extractBridgeTransportTiming,
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
import { buildPayloadSizeMetric } from './read-context-payload-metrics.js';
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
  const response = await measureTaskTimingAsync(timing, 'read_context.bridge_send_receive', () => (
    context.bridge.sendCommand(route.command, payload, {
      timing,
      timingPrefix: 'read_context.bridge_transport',
    })
  ));
  addReadPayloadSizeMarker(timing, 'read_context.bridge_payload_bytes', response.result);
  addNestedTaskTiming(timing, `bridge.${route.command}`, extractBridgeTransportTiming(response));
  addNestedTaskTiming(timing, `ue.${route.command}`, extractBridgeTiming(response.result));
  if (!response.success) {
    return normalizeBridgeToolResult('read_context', response);
  }

  const payloadResult = measureTaskTiming(timing, 'read_context.bridge_payload_extract', () => (
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
  const response = await measureTaskTimingAsync(timing, 'read_context.bridge_send_receive', () => (
    context.bridge.sendCommand(request.command, payload, {
      timing,
      timingPrefix: 'read_context.bridge_transport',
    })
  ));
  addReadPayloadSizeMarker(timing, 'read_context.bridge_payload_bytes', response.result);
  addNestedTaskTiming(timing, `bridge.${request.command}`, extractBridgeTransportTiming(response));
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

  const payloadResult = measureTaskTiming(timing, 'read_context.bridge_payload_extract', () => (
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
  addReadPayloadSizeMarker(timing, 'read_context.ue_raw_payload_bytes', payload);
  const strippedPayload = measureTaskTiming(timing, 'read_context.ue_timing_extract', () => (
    stripTimingPayload(payload)
  ));
  const readPayload = measureTaskTiming(timing, resolveReadContextPostProcessStage(payloadSchema, input), () => (
    postProcessReadContextPayload(input, payloadSchema, strippedPayload)
  ));
  addReadPayloadSizeMarker(timing, 'read_context.post_processed_payload_bytes', readPayload);
  return readPayload;
}

function stripTimingPayload(payload: Record<string, unknown>): Record<string, unknown> {
  if (!Object.hasOwn(payload, 'timing')) {
    return payload;
  }

  const stripped = { ...payload };
  delete stripped['timing'];
  return stripped;
}

function addReadPayloadSizeMarker(
  timing: BlueprintHelperToolContext['timing'],
  name: string,
  value: unknown,
): void {
  const metric = buildPayloadSizeMetric(name, value);
  addTaskTimingMarker(timing, metric.name, { bytes: metric.bytes });
}
