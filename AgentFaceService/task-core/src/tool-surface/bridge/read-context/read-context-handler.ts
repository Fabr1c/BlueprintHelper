import {
  failureResult,
  normalizeToolResult,
  successRead,
  type ToolResultBase,
  type ToolResultError,
} from '../../../result/tool-result.js';
import {
  addTaskTimingMarker,
  addNestedTaskTiming,
  extractBridgeTiming,
  extractBridgeTransportTiming,
  measureTaskTiming,
  measureTaskTimingAsync,
} from '../../../task/service/task-timing.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { extractBridgePayload, isRecord } from '../bridge-tool-result-utils.js';
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
      ...(request.category ? { category: request.category } : {}),
      ...(request.safe_next_action ? { safe_next_action: request.safe_next_action } : {}),
      ...(request.suggested_route ? { suggested_route: request.suggested_route } : {}),
      ...(request.suggested_read_type ? { suggested_read_type: request.suggested_read_type } : {}),
      ...(request.blocked_boundary ? { blocked_boundary: request.blocked_boundary } : {}),
      ...(request.blocked_boundary_detail ? { blocked_boundary_detail: request.blocked_boundary_detail } : {}),
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
    const bridgeError = extractBridgeToolError(response.result);
    return normalizeToolResult(response, 'read_context', {
      target: buildReadContextTarget(input),
      error: {
        code: bridgeError.code ?? response.error_code ?? 'read_context_bridge_error',
        stage: bridgeError.stage ?? 'bridge',
        message: bridgeError.message ?? response.message ?? `${request.command} failed.`,
        ...bridgeError,
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

  const readPayloadResult = buildReadPayloadWithTimingSafe(input, request.route, request.payloadSchema, payloadResult.payload, context);
  if (!readPayloadResult.ok) {
    return failureResult('read_context', {
      code: input.read_type === 'material_graph_context'
        ? 'material_logic_projection_failed'
        : 'read_context_projection_failed',
      stage: 'post_process',
      message: readPayloadResult.message,
      retryable: false,
      rollback_result: 'not_needed',
    }, buildReadContextTarget(input) as never);
  }
  return measureTaskTiming(timing, 'read_context.result_wrap', () => buildReadContextResult(input, readPayloadResult));
}

function withReadTimingPayload(
  payload: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Record<string, unknown> {
  return context.timing ? { ...payload, include_timing: true } : payload;
}

function extractBridgeToolError(result: unknown): Partial<ToolResultError> {
  if (!isRecord(result) || !isRecord(result['error'])) {
    return {};
  }
  const error = result['error'];
  const metadata: Partial<ToolResultError> = {};
  for (const key of [
    'code',
    'stage',
    'message',
    'category',
    'safe_next_action',
    'suggested_route',
    'suggested_read_type',
    'blocked_boundary',
    'blocked_boundary_detail',
  ] as const) {
    const value = error[key];
    if (typeof value === 'string') {
      (metadata as Record<string, string>)[key] = value;
    }
  }
  const retryable = error['retryable'];
  if (typeof retryable === 'boolean') {
    metadata.retryable = retryable;
  }
  const rollbackResult = error['rollback_result'];
  if (
    rollbackResult === 'not_needed'
    || rollbackResult === 'rolled_back'
    || rollbackResult === 'rollback_failed'
    || rollbackResult === 'unavailable'
  ) {
    metadata.rollback_result = rollbackResult;
  }
  return metadata;
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

function buildReadPayloadWithTimingSafe(
  input: ReadContextInput,
  route: ReadContextRouteDescriptor,
  payloadSchema: string,
  payload: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): { ok: true; payload: Record<string, unknown>; debug?: Record<string, unknown> } | { ok: false; message: string } {
  try {
    return {
      ok: true,
      ...buildReadPayloadWithTiming(input, route, payloadSchema, payload, context),
    };
  } catch (err) {
    return {
      ok: false,
      message: err instanceof Error ? err.message : String(err),
    };
  }
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
