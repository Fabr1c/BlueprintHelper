import type { ReadContextInput } from './read-context-schemas.js';

export type ReadContextBridgeRequest =
  | {
      ok: true;
      command: string;
      payload: Record<string, unknown>;
      payloadSchema: string;
    }
  | {
      ok: false;
      code: string;
      message: string;
    };

export type ReadContextLogicFormat = 'logic_flow' | 'logic_md' | 'logic_json';

export function resolveReadContextLogicFormat(input: ReadContextInput): ReadContextLogicFormat | undefined {
  if (input.read_type === 'graph_context') {
    return 'logic_json';
  }
  if (input.read_type === 'blueprint_logic') {
    return input.view?.format ?? 'logic_md';
  }
  return undefined;
}

export function resolveReadContextBridgeCommand(
  format: ReadContextLogicFormat,
): 'read_blueprint_logic_md' | 'read_blueprint_logic_json' {
  return format === 'logic_md'
    ? 'read_blueprint_logic_md'
    : 'read_blueprint_logic_json';
}

export function resolveReadContextPayloadSchema(
  format: ReadContextLogicFormat,
): 'LogicFlow.v1' | 'LogicMd.v1' | 'LogicJson.v1' {
  if (format === 'logic_flow') return 'LogicFlow.v1';
  if (format === 'logic_json') return 'LogicJson.v1';
  return 'LogicMd.v1';
}

export function buildBlueprintLogicReadPayload(input: ReadContextInput): Record<string, unknown> {
  const payload: Record<string, unknown> = {
    asset_path: input.target.asset_path,
  };
  const targetName = input.target.target_name;
  const targetType = input.read_type === 'graph_context' && targetName && input.target.target_type !== 'block'
    ? 'graph'
    : input.target.target_type;
  switch (targetType) {
    case 'graph':
      if (targetName) payload['graph'] = targetName;
      break;
    case 'function':
      if (targetName) payload['function'] = targetName;
      break;
    case 'event':
    case 'custom_event':
      if (targetName) payload['event'] = targetName;
      break;
    case 'block':
      if (input.target.block_id ?? targetName) payload['block_id'] = input.target.block_id ?? targetName;
      break;
  }
  payload['scope'] = inferBlueprintLogicScope(input);
  return payload;
}

export function inferBlueprintLogicScope(input: ReadContextInput): string {
  const targetType = input.read_type === 'graph_context' && input.target.target_name && input.target.target_type !== 'block'
    ? 'graph'
    : input.target.target_type;
  if (targetType === 'blueprint' || targetType === 'asset') return 'blueprint';
  if (targetType === 'function') return 'target_function';
  if (targetType === 'event') return 'target_event';
  if (targetType === 'custom_event') return 'target_custom_event';
  return 'target_graph';
}

export function buildReadContextBridgeRequest(input: ReadContextInput): ReadContextBridgeRequest {
  const targetName = input.target.target_name;
  const payload: Record<string, unknown> = {
    asset_path: input.target.asset_path,
  };

  switch (input.read_type) {
    case 'asset_context':
      return { ok: true, command: 'get_asset_info', payload, payloadSchema: 'AssetContext.v1' };
    case 'component_context':
      return { ok: true, command: 'read_components', payload, payloadSchema: 'ComponentContext.v1' };
    case 'variable_context':
      return input.target.target_type === 'event_dispatcher'
        ? { ok: true, command: 'list_event_dispatchers', payload, payloadSchema: 'EventDispatcherContext.v1' }
        : { ok: true, command: 'list_variables', payload, payloadSchema: 'VariableContext.v1' };
    case 'widget_context':
      if (targetName) {
        payload['widget_name'] = targetName;
        return { ok: true, command: 'get_widget_properties', payload, payloadSchema: 'WidgetPropertyContext.v1' };
      }
      return { ok: true, command: 'get_widget_tree', payload, payloadSchema: 'WidgetContext.v1' };
    case 'data_table_context':
      if (targetName) {
        payload['row_names'] = [targetName];
      }
      return { ok: true, command: 'get_datatable_rows', payload, payloadSchema: 'DataTableContext.v1' };
    case 'data_asset_context':
      return { ok: true, command: 'get_object_properties', payload, payloadSchema: 'DataAssetContext.v1' };
    case 'object_property_context':
      return { ok: true, command: 'get_object_properties', payload, payloadSchema: 'ObjectPropertyContext.v1' };
    default:
      return {
        ok: false,
        code: 'unsupported_read_type',
        message: `read_context read_type is not bridge-backed: ${input.read_type}.`,
      };
  }
}
