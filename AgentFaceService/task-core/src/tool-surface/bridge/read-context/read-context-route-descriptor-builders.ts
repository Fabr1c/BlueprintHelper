import { getActiveReadContextRouteDescriptors } from '../../templates/read-context-template-registry.js';
import type {
  ReadContextRequestBuilderId,
  ReadContextRouteDescriptor,
  ReadContextTemplateView,
} from '../../templates/read-context-template-types.js';
import type { ReadContextInput } from './read-context-schemas.js';

export type ReadContextBridgeRequest =
  | {
      ok: true;
      command: string;
      payload: Record<string, unknown>;
      payloadSchema: string;
      route: ReadContextRouteDescriptor;
    }
  | {
      ok: false;
      code: string;
      message: string;
    };

export type ReadContextLogicFormat = 'logic_flow' | 'logic_json';

export type ReadContextLogicBridgeRoute = {
  format: ReadContextLogicFormat;
  command: 'read_blueprint_logic_json';
  payloadSchema: 'LogicFlow.v1' | 'LogicJson.v1';
};

export type ReadContextRequestBuilder = (
  input: ReadContextInput,
  route: ReadContextRouteDescriptor,
) => ReadContextBridgeRequest;

const LOGIC_FORMATS = new Set<ReadContextTemplateView>(['logic_flow', 'logic_json']);

const TARGET_PAYLOAD_KEY_BY_TYPE: Readonly<Record<string, string>> = {
  graph: 'graph',
  function: 'function',
  event: 'event',
  custom_event: 'event',
  block: 'block_id',
};

const LOGIC_SCOPE_BY_TYPE: Readonly<Record<string, string>> = {
  asset: 'blueprint',
  blueprint: 'blueprint',
  function: 'target_function',
  event: 'target_event',
  custom_event: 'target_custom_event',
  graph: 'target_graph',
  block: 'target_graph',
};

const REQUEST_BUILDERS: Readonly<Record<ReadContextRequestBuilderId, ReadContextRequestBuilder>> = {
  blueprint_logic: buildBlueprintLogicBridgeRequest,
  asset_context: buildAssetBridgeRequest,
  component_context: buildAssetBridgeRequest,
  variable_context: buildAssetBridgeRequest,
  widget_tree: buildAssetBridgeRequest,
  widget_property: buildWidgetPropertyBridgeRequest,
  data_table: buildDataTableBridgeRequest,
  data_asset: buildAssetBridgeRequest,
  object_property: buildAssetBridgeRequest,
};

export function getReadContextRequestBuilder(id: ReadContextRequestBuilderId): ReadContextRequestBuilder {
  return REQUEST_BUILDERS[id];
}

export function resolveReadContextRouteDescriptor(input: ReadContextInput): ReadContextRouteDescriptor | undefined {
  const candidates = getActiveReadContextRouteDescriptors()
    .filter((route) => route.read_type === input.read_type)
    .filter((route) => routeMatchesTarget(route, input))
    .filter((route) => routeMatchesFormat(route, input));
  return candidates[0];
}

export function buildReadContextBridgeRequest(input: ReadContextInput): ReadContextBridgeRequest {
  const route = resolveReadContextRouteDescriptor(input);
  if (!route) {
    return {
      ok: false,
      code: 'unsupported_read_context_route',
      message: `read_context route is not descriptor-backed for read_type=${input.read_type}, target_type=${input.target.target_type}, format=${input.view?.format ?? '<default>'}.`,
    };
  }

  const builder = getReadContextRequestBuilder(route.request_builder_id);
  return builder(input, route);
}

export function resolveReadContextLogicFormat(input: ReadContextInput): ReadContextLogicFormat | undefined {
  const route = resolveReadContextRouteDescriptor(input);
  return route && isLogicFormat(route.format) ? route.format : undefined;
}

export function buildReadContextLogicBridgeRoute(format: ReadContextLogicFormat): ReadContextLogicBridgeRoute {
  return {
    format,
    command: resolveReadContextBridgeCommand(format),
    payloadSchema: resolveReadContextPayloadSchema(format),
  };
}

export function resolveReadContextBridgeCommand(
  format: ReadContextLogicFormat,
): 'read_blueprint_logic_json' {
  return 'read_blueprint_logic_json';
}

export function resolveReadContextPayloadSchema(
  format: ReadContextLogicFormat,
): 'LogicFlow.v1' | 'LogicJson.v1' {
  const schemas: Readonly<Record<ReadContextLogicFormat, 'LogicFlow.v1' | 'LogicJson.v1'>> = {
    logic_flow: 'LogicFlow.v1',
    logic_json: 'LogicJson.v1',
  };
  return schemas[format];
}

export function buildBlueprintLogicReadPayload(
  input: ReadContextInput,
  route = resolveReadContextRouteDescriptor(input),
): Record<string, unknown> {
  const targetType = route?.target_type ?? input.target.target_type ?? 'blueprint';
  const targetName = input.target.target_name;
  const graphName = resolveBlueprintLogicGraphName(input, targetType, targetName);
  const payload: Record<string, unknown> = {
    asset_path: input.target.asset_path,
    target_type: targetType,
  };
  if (targetName) {
    payload['target_name'] = targetName;
  }
  if (graphName) {
    payload['graph_name'] = graphName;
  }

  const payloadKey = TARGET_PAYLOAD_KEY_BY_TYPE[targetType];
  if (payloadKey) {
    const payloadValue = targetType === 'block'
      ? input.target.block_id ?? targetName
      : targetName;
    if (payloadValue) {
      payload[payloadKey] = payloadValue;
    }
  }

  payload['scope'] = inferBlueprintLogicScope(input, route);
  return payload;
}

function resolveBlueprintLogicGraphName(
  input: ReadContextInput,
  targetType: string,
  targetName: string | undefined,
): string | undefined {
  if (input.target.graph_name) {
    return input.target.graph_name;
  }
  if (targetType === 'function') {
    return targetName;
  }
  if (targetType === 'event' || targetType === 'custom_event') {
    return 'EventGraph';
  }
  return undefined;
}

export function inferBlueprintLogicScope(
  input: ReadContextInput,
  route = resolveReadContextRouteDescriptor(input),
): string {
  const targetType = route?.target_type ?? input.target.target_type ?? 'blueprint';
  return LOGIC_SCOPE_BY_TYPE[targetType] ?? 'target_graph';
}

function buildBlueprintLogicBridgeRequest(
  input: ReadContextInput,
  route: ReadContextRouteDescriptor,
): ReadContextBridgeRequest {
  const format = isLogicFormat(route.format) ? route.format : 'logic_flow';
  const command = route.bridge_command ?? resolveReadContextBridgeCommand(format);
  return okRequest(route, command, buildBlueprintLogicReadPayload(input, route), route.output_schema);
}

function buildAssetBridgeRequest(
  input: ReadContextInput,
  route: ReadContextRouteDescriptor,
): ReadContextBridgeRequest {
  return okRequest(route, requiredBridgeCommand(route), {
    asset_path: input.target.asset_path,
  }, route.output_schema);
}

function buildWidgetPropertyBridgeRequest(
  input: ReadContextInput,
  route: ReadContextRouteDescriptor,
): ReadContextBridgeRequest {
  const payload: Record<string, unknown> = {
    asset_path: input.target.asset_path,
  };
  if (input.target.target_name) {
    payload['widget_name'] = input.target.target_name;
  }
  return okRequest(route, requiredBridgeCommand(route), payload, route.output_schema);
}

function buildDataTableBridgeRequest(
  input: ReadContextInput,
  route: ReadContextRouteDescriptor,
): ReadContextBridgeRequest {
  const payload: Record<string, unknown> = {
    asset_path: input.target.asset_path,
  };
  if (input.target.target_name) {
    payload['row_names'] = [input.target.target_name];
  }
  return okRequest(route, requiredBridgeCommand(route), payload, route.output_schema);
}

function okRequest(
  route: ReadContextRouteDescriptor,
  command: string,
  payload: Record<string, unknown>,
  payloadSchema: string,
): ReadContextBridgeRequest {
  return {
    ok: true,
    command,
    payload,
    payloadSchema,
    route,
  };
}

function requiredBridgeCommand(route: ReadContextRouteDescriptor): string {
  if (route.bridge_command) {
    return route.bridge_command;
  }
  throw new Error(`ReadContext route ${route.route_id} is missing bridge_command.`);
}

function routeMatchesTarget(route: ReadContextRouteDescriptor, input: ReadContextInput): boolean {
  const targetType = input.target.target_type ?? 'blueprint';
  if (!route.supported_asset_types.includes(targetType) && route.target_type !== targetType) {
    return false;
  }
  if (route.required_target_fields.includes('target_name') && !input.target.target_name) {
    return false;
  }
  if (route.required_target_fields.includes('block_id') && !input.target.block_id) {
    return false;
  }
  if (route.target_kind === 'widget_tree' && input.target.target_name) {
    return false;
  }
  return true;
}

function routeMatchesFormat(route: ReadContextRouteDescriptor, input: ReadContextInput): boolean {
  const requestedFormat = input.view?.format;
  return requestedFormat === undefined || route.format === requestedFormat;
}

function isLogicFormat(format: unknown): format is ReadContextLogicFormat {
  return typeof format === 'string' && LOGIC_FORMATS.has(format as ReadContextTemplateView);
}
