import type { ReadContextInput } from './read-context-schemas.js';
import { isRecord } from '../bridge-tool-result-utils.js';

export function postProcessReadContextPayload(
  input: ReadContextInput,
  payloadSchema: string,
  payload: Record<string, unknown>,
): Record<string, unknown> {
  const normalized: Record<string, unknown> = {
    schema: payload['schema'] ?? payloadSchema,
    ...payload,
  };
  delete normalized['format'];

  if (input.read_type === 'blueprint_logic' || input.read_type === 'graph_context') {
    return compactLogicContextPayload(normalized);
  }
  if (input.read_type === 'asset_context') {
    return compactAssetContextPayload(normalized);
  }

  const targetName = input.target.target_name;
  if (!targetName) {
    return normalized;
  }

  if (input.read_type === 'component_context') {
    return filterNamedArrayPayload(normalized, 'components', targetName, ['name', 'component_name']);
  }
  if (input.read_type === 'variable_context') {
    const key = input.target.target_type === 'event_dispatcher' ? 'event_dispatchers' : 'variables';
    const filtered = filterNamedArrayPayload(normalized, key, targetName, ['name', 'variable_name']);
    if (key === 'variables' && !Array.isArray(normalized[key]) && Array.isArray(normalized['member_variables'])) {
      return filterNamedArrayPayload(normalized, 'member_variables', targetName, ['name', 'variable_name']);
    }
    return filtered;
  }
  if (input.read_type === 'object_property_context' || input.read_type === 'data_asset_context') {
    return filterNamedArrayPayload(normalized, 'properties', targetName, ['name', 'property_name']);
  }
  return normalized;
}

function compactLogicContextPayload(payload: Record<string, unknown>): Record<string, unknown> {
  const logic = payload['logic'];
  if (!isRecord(logic) || !Object.hasOwn(logic, 'asset_path')) {
    return payload;
  }

  const compactedLogic = { ...logic };
  delete compactedLogic['asset_path'];
  return {
    ...payload,
    logic: compactedLogic,
  };
}

function compactAssetContextPayload(payload: Record<string, unknown>): Record<string, unknown> {
  const compacted = { ...payload };
  delete compacted['path'];
  delete compacted['name'];
  return compacted;
}

function filterNamedArrayPayload(
  payload: Record<string, unknown>,
  arrayKey: string,
  targetName: string,
  nameKeys: string[],
): Record<string, unknown> {
  const value = payload[arrayKey];
  if (!Array.isArray(value)) {
    return payload;
  }

  const filtered = value.filter((item) => {
    if (!isRecord(item)) {
      return false;
    }
    return nameKeys.some((key) => {
      const candidate = item[key];
      return typeof candidate === 'string' && candidate.toLowerCase() === targetName.toLowerCase();
    });
  });
  return {
    ...payload,
    [arrayKey]: filtered,
    count: filtered.length,
  };
}
