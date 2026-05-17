import {
  normalizeToolResult,
  sanitizeAgentFacingToolResult,
  type ToolResultBase,
} from '../../result/tool-result.js';
import type { BridgeResponse } from '../../bridge/bridge-client.js';

const functionChainLegacySchema = 'BlueprintHelper.FunctionChainContext.v1';
const functionChainSchema = 'FunctionChainContext.v1';

export function normalizeBridgeToolResult(toolName: string, response: BridgeResponse): ToolResultBase {
  const normalized = isToolResultBase(response.result)
    ? sanitizeAgentFacingToolResult(response.result)
    : normalizeToolResult(response, toolName);
  return toolName === 'blueprinthelper_read_function_chain_context'
    ? normalizeFunctionChainPayload(normalized)
    : normalized;
}

function normalizeFunctionChainPayload(result: ToolResultBase): ToolResultBase {
  return normalizeFunctionChainValue(stripKeysRecursive(result, new Set(['node_ref', 'node_path']))) as ToolResultBase;
}

function normalizeFunctionChainValue(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => normalizeFunctionChainValue(item));
  }
  if (!isRecord(value)) {
    return value;
  }

  const normalized: Record<string, unknown> = {};
  for (const [key, entryValue] of Object.entries(value)) {
    normalized[key] = key === 'schema' && entryValue === functionChainLegacySchema
      ? functionChainSchema
      : normalizeFunctionChainValue(entryValue);
  }
  return normalized;
}

function stripKeysRecursive(value: unknown, keysToStrip: ReadonlySet<string>): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => stripKeysRecursive(item, keysToStrip));
  }
  if (!isRecord(value)) {
    return value;
  }

  const stripped: Record<string, unknown> = {};
  for (const [key, entryValue] of Object.entries(value)) {
    if (keysToStrip.has(key)) {
      continue;
    }
    stripped[key] = stripKeysRecursive(entryValue, keysToStrip);
  }
  return stripped;
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
