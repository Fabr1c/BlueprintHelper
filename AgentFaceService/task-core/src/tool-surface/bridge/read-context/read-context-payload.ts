import type { ReadContextInput } from './read-context-schemas.js';
import { buildLogicFlowPayload } from './read-context-logic-flow.js';
import { isRecord } from '../bridge-tool-result-utils.js';

export type ReadContextPostProcessResult = {
  payload: Record<string, unknown>;
  debug?: Record<string, unknown>;
};

export function postProcessReadContextPayloadWithDebug(
  input: ReadContextInput,
  payloadSchema: string,
  payload: Record<string, unknown>,
): ReadContextPostProcessResult {
  if (payloadSchema === 'LogicFlow.v1') {
    const result = buildLogicFlowPayload(payload);
    return {
      payload: result.payload,
      debug: result.debug ? { logic_flow: result.debug } : undefined,
    };
  }

  return {
    payload: postProcessReadContextPayload(input, payloadSchema, payload),
  };
}

export function postProcessReadContextPayload(
  input: ReadContextInput,
  payloadSchema: string,
  payload: Record<string, unknown>,
): Record<string, unknown> {
  if (payloadSchema === 'LogicFlow.v1') {
    return buildLogicFlowPayload(payload).payload;
  }

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

export function resolveReadContextPostProcessStage(
  payloadSchema: string,
  input?: ReadContextInput,
): string {
  if (payloadSchema === 'LogicFlow.v1') {
    return 'read_context.logic_flow_build_payload';
  }
  if (input?.view?.detail === 'brief') {
    return 'read_context.compact_payload';
  }
  if (input?.target?.target_name) {
    return 'read_context.filter_payload';
  }
  return 'read_context.post_process_payload';
}

function compactLogicContextPayload(payload: Record<string, unknown>): Record<string, unknown> {
  const compactedPayload = compactLogicMdMarkdown(payload);
  const logic = payload['logic'];
  if (!isRecord(logic) || !Object.hasOwn(logic, 'asset_path')) {
    return compactedPayload;
  }

  const compactedLogic = { ...logic };
  delete compactedLogic['asset_path'];
  return {
    ...compactedPayload,
    logic: compactedLogic,
  };
}

function compactLogicMdMarkdown(payload: Record<string, unknown>): Record<string, unknown> {
  if (payload['schema'] !== 'LogicMd.v1' || typeof payload['markdown'] !== 'string') {
    return payload;
  }

  const markdown = stripLogicMdStatsLines(payload['markdown']);
  if (markdown === payload['markdown']) {
    return payload;
  }
  return {
    ...payload,
    markdown,
  };
}

function stripLogicMdStatsLines(markdown: string): string {
  const lines = markdown.split(/\r?\n/);
  const stripped = lines.filter((line) => !isLogicMdStatsLine(line));
  return stripped.join('\n').replace(/\n{3,}/g, '\n\n');
}

function isLogicMdStatsLine(line: string): boolean {
  return /^Nodes:\s*\d+\s*\|\s*Exec Links:\s*\d+\s*\|\s*Data Links:\s*\d+(?:\s*\|\s*Entry Points:\s*\d+)?\s*\|\s*Orphans:\s*\d+\s*$/i.test(line.trim());
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
