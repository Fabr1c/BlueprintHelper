import type { ReadContextInput } from './read-context-schemas.js';

export type ReadContextBridgeRequest =
  | {
      ok: true;
      command: string;
      payload: Record<string, unknown>;
      payloadSchema: string;
      scope: string;
    }
  | {
      ok: false;
      code: string;
      message: string;
    };

export function normalizeReadContextFormat(input: ReadContextInput, requestedFormat: string): string {
  if (requestedFormat === 'schema') {
    return 'schema';
  }
  if (input.read_type === 'blueprint_logic' || input.read_type === 'graph_context') {
    return requestedFormat;
  }
  return 'summary';
}

export function isTargetEntryLogicRead(input: ReadContextInput): boolean {
  const targetType = input.target.target_type;
  return input.read_type === 'graph_context' ||
    targetType === 'function' || targetType === 'event' || targetType === 'custom_event';
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
      return { ok: true, command: 'get_asset_info', payload, payloadSchema: 'AssetContext.v1', scope: 'asset' };
    case 'component_context':
      return { ok: true, command: 'read_components', payload, payloadSchema: 'ComponentContext.v1', scope: 'components' };
    case 'variable_context':
      return input.target.target_type === 'event_dispatcher'
        ? { ok: true, command: 'list_event_dispatchers', payload, payloadSchema: 'EventDispatcherContext.v1', scope: 'event_dispatchers' }
        : { ok: true, command: 'list_variables', payload, payloadSchema: 'VariableContext.v1', scope: 'variables' };
    case 'widget_context':
      if (targetName) {
        payload['widget_name'] = targetName;
        return { ok: true, command: 'get_widget_properties', payload, payloadSchema: 'WidgetPropertyContext.v1', scope: 'widget' };
      }
      return { ok: true, command: 'get_widget_tree', payload, payloadSchema: 'WidgetContext.v1', scope: 'widget_tree' };
    case 'data_table_context':
      if (targetName) {
        payload['row_names'] = [targetName];
      }
      return { ok: true, command: 'get_datatable_rows', payload, payloadSchema: 'DataTableContext.v1', scope: targetName ? 'data_table_row' : 'data_table' };
    case 'data_asset_context':
      return { ok: true, command: 'get_object_properties', payload, payloadSchema: 'DataAssetContext.v1', scope: 'data_asset_properties' };
    case 'object_property_context':
      return { ok: true, command: 'get_object_properties', payload, payloadSchema: 'ObjectPropertyContext.v1', scope: targetName ? 'object_property' : 'object_properties' };
    default:
      return {
        ok: false,
        code: 'unsupported_read_type',
        message: `read_context read_type is not bridge-backed: ${input.read_type}.`,
      };
  }
}

export function buildReadContextSchemaPayload(): Record<string, unknown> {
  return {
    schema: 'ReadContextSchema.v1',
    read_types: [
      {
        read_type: 'asset_context',
        target_types: ['asset', 'blueprint', 'data_table', 'data_asset'],
        bridge_command: 'get_asset_info',
        payload_schema: 'AssetContext.v1',
      },
      {
        read_type: 'blueprint_logic',
        target_types: ['blueprint', 'graph', 'function', 'event', 'custom_event', 'block'],
        formats: ['logic_md', 'logic_json', 'summary', 'schema'],
        bridge_commands: ['read_blueprint_logic_md', 'read_blueprint_logic_json'],
        payload_schema: 'BlueprintLogicReadSchema.v1',
      },
      {
        read_type: 'graph_context',
        target_types: ['blueprint', 'graph', 'function', 'event', 'custom_event', 'block'],
        formats: ['logic_json', 'summary', 'schema'],
        bridge_command: 'read_blueprint_logic_json',
        payload_schema: 'GraphContext.v1',
      },
      {
        read_type: 'component_context',
        target_types: ['blueprint', 'component'],
        bridge_command: 'read_components',
        payload_schema: 'ComponentContext.v1',
      },
      {
        read_type: 'variable_context',
        target_types: ['blueprint', 'member_variable', 'event_dispatcher'],
        bridge_commands: ['list_variables', 'list_event_dispatchers'],
        payload_schema: 'VariableContext.v1',
      },
      {
        read_type: 'widget_context',
        target_types: ['blueprint', 'widget'],
        bridge_commands: ['get_widget_tree', 'get_widget_properties'],
        payload_schema: 'WidgetContext.v1',
      },
      {
        read_type: 'data_table_context',
        target_types: ['data_table', 'data_table_row', 'asset'],
        bridge_command: 'get_datatable_rows',
        payload_schema: 'DataTableContext.v1',
      },
      {
        read_type: 'data_asset_context',
        target_types: ['data_asset', 'asset', 'object_property', 'property'],
        bridge_command: 'get_object_properties',
        payload_schema: 'DataAssetContext.v1',
      },
      {
        read_type: 'object_property_context',
        target_types: ['asset', 'data_asset', 'object_property', 'property'],
        bridge_command: 'get_object_properties',
        payload_schema: 'ObjectPropertyContext.v1',
      },
    ],
  };
}
