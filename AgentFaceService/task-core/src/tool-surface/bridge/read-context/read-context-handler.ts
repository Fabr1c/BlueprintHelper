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
import { extractBridgePayload } from '../bridge-tool-result-utils.js';
import { buildReadContextBridgeRequest } from './read-context-route-builder.js';
import { buildReadContextTarget } from './read-context-target.js';
import {
  postProcessReadContextPayloadWithDebug,
  resolveReadContextPostProcessStage,
} from './read-context-payload.js';
import { buildPayloadSizeMetric } from './read-context-payload-metrics.js';
import type { ReadContextInput } from './read-context-schemas.js';
import type { ReadContextRouteDescriptor } from '../../templates/read-context-template-types.js';

type ReadPayloadWithDebug = {
  payload: Record<string, unknown>;
  debug?: Record<string, unknown>;
};

export async function executeReadContext(
  rawInput: ReadContextInput | Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const timing = context.timing;
  const input = measureTaskTiming(timing, 'read_context.parse_input', () => rawInput as ReadContextInput);
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

  const readPayloadResult = buildReadPayloadWithTiming(input, request.route, request.payloadSchema, payloadResult.payload, context);
  return measureTaskTiming(timing, 'read_context.result_wrap', () => buildReadContextResult(input, readPayloadResult));
}

function withReadTimingPayload(
  payload: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Record<string, unknown> {
  return context.timing ? { ...payload, include_timing: true } : payload;
}

function buildReadPayloadWithTiming(
  input: ReadContextInput,
  route: ReadContextRouteDescriptor,
  payloadSchema: string,
  payload: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): ReadPayloadWithDebug {
  const timing = context.timing;
  addReadPayloadSizeMarker(timing, 'read_context.ue_raw_payload_bytes', payload);
  const strippedPayload = measureTaskTiming(timing, 'read_context.ue_timing_extract', () => (
    stripTimingPayload(payload)
  ));
  const postProcessResult = measureTaskTiming(timing, resolveReadContextPostProcessStage(route, input), () => (
    postProcessReadContextPayloadWithDebug(input, route, payloadSchema, strippedPayload)
  ));
  addReadPayloadSizeMarker(timing, 'read_context.post_processed_payload_bytes', postProcessResult.payload);
  return postProcessResult;
}

function buildReadContextResult(
  input: ReadContextInput,
  readPayloadResult: ReadPayloadWithDebug,
): ToolResultBase {
  const toolResult = successRead('read_context', buildReadContextTarget(input), {
    schema: 'ReadContextPack.v1',
    payload: readPayloadResult.payload,
    truncated: false,
  }) as ToolResultBase;

  if (readPayloadResult.debug && Object.keys(readPayloadResult.debug).length > 0) {
    Object.defineProperty(toolResult, 'debug', {
      value: readPayloadResult.debug,
      enumerable: false,
      configurable: true,
    });
  }

  return toolResult;
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
