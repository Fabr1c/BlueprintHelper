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

export function deriveReadContextStats(input: ReadContextInput, payload: Record<string, unknown>): Record<string, unknown> {
  if (isRecord(payload['stats'])) {
    return payload['stats'];
  }
  switch (input.read_type) {
    case 'component_context':
      return { components: countArray(payload['components']) };
    case 'variable_context':
      return {
        variables: countArray(payload['variables'] ?? payload['member_variables']),
        event_dispatchers: countArray(payload['event_dispatchers']),
      };
    case 'widget_context':
      return {
        widgets: countArray(payload['widgets']),
        properties: countArray(payload['properties']),
      };
    case 'data_table_context':
      return {
        rows: typeof payload['row_count'] === 'number' ? payload['row_count'] : countArray(payload['rows']),
        columns: countArray(payload['columns']),
      };
    case 'data_asset_context':
    case 'object_property_context':
      return { properties: typeof payload['count'] === 'number' ? payload['count'] : countArray(payload['properties']) };
    default:
      return {};
  }
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

function countArray(value: unknown): number {
  return Array.isArray(value) ? value.length : 0;
}
