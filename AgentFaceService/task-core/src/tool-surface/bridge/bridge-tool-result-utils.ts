import { normalizeToolResult, type ToolResultBase } from '../../result/tool-result.js';
import type { BridgeResponse } from '../../bridge/bridge-client.js';

export function normalizeBridgeToolResult(toolName: string, response: BridgeResponse): ToolResultBase {
  if (isToolResultBase(response.result)) {
    return response.result;
  }
  return normalizeToolResult(response, toolName);
}

export function extractBridgePayload(result: unknown): { ok: true; payload: Record<string, unknown> } | { ok: false; message: string } {
  if (!isRecord(result)) {
    return { ok: true, payload: {} };
  }
  const normalized = normalizeBridgePayloadValue(result['data'] ?? result);
  if (!normalized.ok) {
    return normalized;
  }
  return isRecord(normalized.value)
    ? { ok: true, payload: normalized.value }
    : { ok: false, message: 'read_context Bridge payload must be a JSON object.' };
}

function normalizeBridgePayloadValue(value: unknown): { ok: true; value: unknown } | { ok: false; message: string } {
  if (typeof value !== 'string') {
    return { ok: true, value };
  }

  const text = value.trim();
  if (text.length === 0) {
    return { ok: true, value: {} };
  }

  const looksLikeJson =
    (text.startsWith('{') && text.endsWith('}')) ||
    (text.startsWith('[') && text.endsWith(']'));
  if (!looksLikeJson) {
    return { ok: false, message: 'read_context Bridge payload was a non-JSON string.' };
  }

  try {
    return { ok: true, value: JSON.parse(text) as unknown };
  } catch (err) {
    return {
      ok: false,
      message: `read_context Bridge payload string is not valid JSON: ${err instanceof Error ? err.message : String(err)}`,
    };
  }
}

function isToolResultBase(value: unknown): value is ToolResultBase {
  return isRecord(value)
    && typeof value['ok'] === 'boolean'
    && typeof value['schema'] === 'string'
    && typeof value['operation'] === 'string'
    && typeof value['status'] === 'string';
}

export function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

export function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}
