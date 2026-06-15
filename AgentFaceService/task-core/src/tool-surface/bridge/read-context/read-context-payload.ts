import type { ReadContextRouteDescriptor } from '../../templates/read-context-template-types.js';
import { isRecord } from '../bridge-tool-result-utils.js';
import {
  getReadContextPayloadProjector,
  registerReadContextPayloadProjector,
  type ReadContextPayloadProjectorInput,
  type ReadContextPostProcessResult,
} from './read-context-payload-projector-registry.js';
import {
  projectReadContextLogic,
  type ReadContextLogicFormat,
} from './read-context-logic-projector.js';
import type { ReadContextInput } from './read-context-schemas.js';
import {
  buildWidgetTreeJsonPayload,
  buildWidgetTreeLogicFlowPayload,
} from './read-context-widget-tree-projection.js';

export type { ReadContextPostProcessResult } from './read-context-payload-projector-registry.js';

const LOGIC_FORMATS = new Set<ReadContextLogicFormat>([
  'logic_flow',
  'logic_json',
  'logic_json_delta_after_logic_flow',
]);

export function postProcessReadContextPayloadWithDebug(
  input: ReadContextInput,
  route: ReadContextRouteDescriptor,
  payloadSchema: string,
  payload: Record<string, unknown>,
): ReadContextPostProcessResult {
  const projector = getReadContextPayloadProjector(route.payload_projector_id);
  return projector({
    input,
    route,
    payloadSchema,
    payload,
  });
}

export function postProcessReadContextPayload(
  input: ReadContextInput,
  route: ReadContextRouteDescriptor,
  payloadSchema: string,
  payload: Record<string, unknown>,
): Record<string, unknown> {
  return postProcessReadContextPayloadWithDebug(input, route, payloadSchema, payload).payload;
}

export function resolveReadContextPostProcessStage(
  route: ReadContextRouteDescriptor,
  input?: ReadContextInput,
): string {
  const stages: Readonly<Record<ReadContextRouteDescriptor['payload_projector_id'], string>> = {
    logic: 'read_context.logic_project_payload',
    asset_context: 'read_context.asset_project_payload',
    widget_tree: 'read_context.widget_tree_project_payload',
    component_tree: 'read_context.component_project_payload',
    variable_schema: 'read_context.variable_project_payload',
    data_table_schema: 'read_context.data_table_project_payload',
    object_property: 'read_context.object_property_project_payload',
  };
  if (input?.view?.detail === 'brief') {
    return 'read_context.compact_payload';
  }
  return stages[route.payload_projector_id];
}

function projectLogicPayload({
  input,
  route,
  payloadSchema,
  payload,
}: ReadContextPayloadProjectorInput): ReadContextPostProcessResult {
  const result = projectReadContextLogic({
    requestedFormat: resolveRequestedLogicFormat(route, payloadSchema),
    bridgePayloadSchema: asLogicProjectionSchema(payloadSchema),
    bridgePayload: payload,
    target: { ...input.target },
    view: { ...input.view },
  });
  return {
    payload: compactLogicContextPayload(result.payload),
    debug: result.debug,
  };
}

function projectAssetPayload({
  payloadSchema,
  payload,
}: ReadContextPayloadProjectorInput): ReadContextPostProcessResult {
  return {
    payload: compactAssetContextPayload(normalizePayload(payloadSchema, payload)),
  };
}

function projectWidgetTreePayload({
  input,
  route,
  payload,
}: ReadContextPayloadProjectorInput): ReadContextPostProcessResult {
  const payloadWithTarget = typeof payload['asset_path'] === 'string' && payload['asset_path'].length > 0
    ? payload
    : { ...payload, asset_path: input.target.asset_path };
  return {
    payload: route.format === 'logic_flow'
      ? buildWidgetTreeLogicFlowPayload(payloadWithTarget)
      : buildWidgetTreeJsonPayload(payloadWithTarget),
  };
}

function projectComponentPayload({
  input,
  payloadSchema,
  payload,
}: ReadContextPayloadProjectorInput): ReadContextPostProcessResult {
  const normalized = normalizePayload(payloadSchema, payload);
  return {
    payload: input.target.target_name
      ? filterNamedArrayPayload(normalized, 'components', input.target.target_name, ['name', 'component_name'])
      : normalized,
  };
}

function projectVariablePayload({
  input,
  route,
  payloadSchema,
  payload,
}: ReadContextPayloadProjectorInput): ReadContextPostProcessResult {
  const normalized = normalizePayload(payloadSchema, payload);
  if (!input.target.target_name) {
    return { payload: normalized };
  }

  const primaryKey = route.target_type === 'event_dispatcher' ? 'event_dispatchers' : 'variables';
  const filtered = filterNamedArrayPayload(normalized, primaryKey, input.target.target_name, ['name', 'variable_name']);
  if (primaryKey === 'variables' && !Array.isArray(normalized[primaryKey]) && Array.isArray(normalized['member_variables'])) {
    return {
      payload: filterNamedArrayPayload(normalized, 'member_variables', input.target.target_name, ['name', 'variable_name']),
    };
  }
  return { payload: filtered };
}

function projectDataTablePayload({
  payloadSchema,
  payload,
}: ReadContextPayloadProjectorInput): ReadContextPostProcessResult {
  return {
    payload: normalizePayload(payloadSchema, payload),
  };
}

function projectObjectPropertyPayload({
  input,
  payloadSchema,
  payload,
}: ReadContextPayloadProjectorInput): ReadContextPostProcessResult {
  const normalized = normalizePayload(payloadSchema, payload);
  return {
    payload: input.target.target_name
      ? filterNamedArrayPayload(normalized, 'properties', input.target.target_name, ['name', 'property_name'])
      : normalized,
  };
}

type LogicProjectionSchema = 'LogicFlow.v1' | 'LogicJson.v1' | 'LogicSnapshot.v1';

function asLogicProjectionSchema(payloadSchema: string): LogicProjectionSchema {
  const logicSchemas = new Set<string>(['LogicFlow.v1', 'LogicJson.v1', 'LogicSnapshot.v1']);
  if (logicSchemas.has(payloadSchema)) {
    return payloadSchema as LogicProjectionSchema;
  }
  throw new Error(`ReadContext logic projector received non-logic schema: ${payloadSchema}`);
}

function resolveRequestedLogicFormat(
  route: ReadContextRouteDescriptor,
  payloadSchema: string,
): ReadContextLogicFormat {
  if (LOGIC_FORMATS.has(route.format as ReadContextLogicFormat)) {
    return route.format as ReadContextLogicFormat;
  }
  const schemaFormats: Readonly<Record<string, ReadContextLogicFormat>> = {
    'LogicFlow.v1': 'logic_flow',
    'LogicJson.v1': 'logic_json',
  };
  return schemaFormats[payloadSchema] ?? 'logic_flow';
}

function normalizePayload(payloadSchema: string, payload: Record<string, unknown>): Record<string, unknown> {
  const normalized: Record<string, unknown> = {
    schema: payload['schema'] ?? payloadSchema,
    ...payload,
  };
  delete normalized['format'];
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

registerReadContextPayloadProjector('logic', projectLogicPayload);
registerReadContextPayloadProjector('asset_context', projectAssetPayload);
registerReadContextPayloadProjector('widget_tree', projectWidgetTreePayload);
registerReadContextPayloadProjector('component_tree', projectComponentPayload);
registerReadContextPayloadProjector('variable_schema', projectVariablePayload);
registerReadContextPayloadProjector('data_table_schema', projectDataTablePayload);
registerReadContextPayloadProjector('object_property', projectObjectPropertyPayload);
