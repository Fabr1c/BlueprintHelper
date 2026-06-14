import type { ReadContextRouteDescriptor } from './read-context-template-types.js';

type ActiveRouteData = Omit<
  ReadContextRouteDescriptor,
  | 'route_id'
  | 'domain'
  | 'read_cluster'
  | 'target_kind'
  | 'view_template'
  | 'status'
  | 'payload_schema'
  | 'supported_asset_types'
  | 'supported_formats'
> & Partial<Pick<ReadContextRouteDescriptor, 'supported_asset_types' | 'supported_formats'>>;

const BLUEPRINT_LOGIC_TEMPLATE_BY_TARGET: Readonly<Record<string, string>> = {
  blueprint: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_graph_logic_json_template.json',
  function: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_function_logic_flow_template.json',
  event: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_event_logic_flow_template.json',
  custom_event: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_custom_event_logic_flow_template.json',
  graph: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_graph_logic_json_template.json',
  block: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_block_logic_json_template.json',
};

const MATERIAL_LOGIC_TEMPLATE = 'AgentFaceService/agent-guide/Templates/read/routes/material_graph_logic_template.json';

const READ_CONTEXT_ROUTE_DESCRIPTORS: readonly ReadContextRouteDescriptor[] = [
  ...blueprintLogicRoutes('blueprint', ['logic_flow', 'logic_json']),
  ...blueprintLogicRoutes('function', ['logic_flow', 'logic_json']),
  ...blueprintLogicRoutes('event', ['logic_flow', 'logic_json']),
  ...blueprintLogicRoutes('custom_event', ['logic_flow', 'logic_json']),
  ...blueprintLogicRoutes('graph', ['logic_flow', 'logic_json']),
  active('read.blueprint.logic.block.logic_json', 'blueprint', 'logic', 'block', 'logic_json', {
    read_type: 'blueprint_logic',
    target_type: 'block',
    format: 'logic_json',
    base_template_path: BLUEPRINT_LOGIC_TEMPLATE_BY_TARGET.block,
    bridge_command: 'read_blueprint_logic_json',
    output_schema: 'LogicJson.v1',
    required_target_fields: ['asset_path', 'block_id'],
    request_builder_id: 'blueprint_logic',
    payload_projector_id: 'logic',
    supported_asset_types: ['blueprint', 'block'],
  }),
  active('read.blueprint.asset.asset.diagnostics_json', 'blueprint', 'asset', 'asset', 'diagnostics_json', {
    read_type: 'asset_context',
    target_type: 'asset',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_asset_object_template.json',
    bridge_command: 'get_asset_info',
    output_schema: 'AssetContext.v1',
    required_target_fields: ['asset_path'],
    request_builder_id: 'asset_context',
    payload_projector_id: 'asset_context',
    supported_asset_types: ['asset', 'blueprint'],
  }),
  active('read.blueprint.variables.blueprint.schema_json', 'blueprint', 'variables', 'blueprint', 'schema_json', {
    read_type: 'variable_context',
    target_type: 'member_variable',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_variables_template.json',
    bridge_command: 'list_variables',
    output_schema: 'VariableContext.v1',
    required_target_fields: ['asset_path'],
    request_builder_id: 'variable_context',
    payload_projector_id: 'variable_schema',
    supported_asset_types: ['blueprint', 'member_variable'],
  }),
  active('read.blueprint.variables.event_dispatcher.schema_json', 'blueprint', 'variables', 'event_dispatcher', 'schema_json', {
    read_type: 'variable_context',
    target_type: 'event_dispatcher',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_variables_template.json',
    bridge_command: 'list_event_dispatchers',
    output_schema: 'EventDispatcherContext.v1',
    required_target_fields: ['asset_path'],
    request_builder_id: 'variable_context',
    payload_projector_id: 'variable_schema',
    supported_asset_types: ['blueprint', 'event_dispatcher'],
  }),
  active('read.blueprint.components.blueprint.tree_json', 'blueprint', 'components', 'blueprint', 'tree_json', {
    read_type: 'component_context',
    target_type: 'blueprint',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_components_template.json',
    bridge_command: 'read_components',
    output_schema: 'ComponentContext.v1',
    required_target_fields: ['asset_path'],
    request_builder_id: 'component_context',
    payload_projector_id: 'component_tree',
    supported_asset_types: ['blueprint', 'component'],
  }),
  active('read.widget_blueprint.structure_tree.widget_tree.tree_json', 'widget_blueprint', 'structure_tree', 'widget_tree', 'tree_json', {
    read_type: 'widget_context',
    target_type: 'blueprint',
    format: 'tree_json',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/widget_tree_template.json',
    bridge_command: 'get_widget_tree',
    output_schema: 'WidgetTreeJson.v1',
    required_target_fields: ['asset_path'],
    request_builder_id: 'widget_tree',
    payload_projector_id: 'widget_tree',
    supported_asset_types: ['widget_blueprint', 'blueprint'],
  }),
  active('read.widget_blueprint.structure_tree.widget_tree.logic_flow', 'widget_blueprint', 'structure_tree', 'widget_tree', 'logic_flow', {
    read_type: 'widget_context',
    target_type: 'blueprint',
    format: 'logic_flow',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/widget_tree_template.json',
    bridge_command: 'get_widget_tree',
    output_schema: 'WidgetTreeLogicFlow.v1',
    required_target_fields: ['asset_path'],
    request_builder_id: 'widget_tree',
    payload_projector_id: 'widget_tree',
    supported_asset_types: ['widget_blueprint', 'blueprint'],
  }),
  active('read.widget_blueprint.structure_tree.widget.property_json', 'widget_blueprint', 'structure_tree', 'widget', 'property_json', {
    read_type: 'widget_context',
    target_type: 'widget',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/widget_property_template.json',
    bridge_command: 'get_widget_properties',
    output_schema: 'WidgetPropertyContext.v1',
    required_target_fields: ['asset_path', 'target_name'],
    request_builder_id: 'widget_property',
    payload_projector_id: 'object_property',
    supported_asset_types: ['widget_blueprint', 'widget'],
  }),
  active('read.data_table.schema.data_table.schema_json', 'data_table', 'schema', 'data_table', 'schema_json', {
    read_type: 'data_table_context',
    target_type: 'data_table',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_table_template.json',
    bridge_command: 'get_datatable_rows',
    output_schema: 'DataTableContext.v1',
    required_target_fields: ['asset_path'],
    request_builder_id: 'data_table',
    payload_projector_id: 'data_table_schema',
    supported_asset_types: ['data_table'],
  }),
  active('read.data_table.schema.data_table_row.schema_json', 'data_table', 'schema', 'data_table_row', 'schema_json', {
    read_type: 'data_table_context',
    target_type: 'data_table_row',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_table_row_template.json',
    bridge_command: 'get_datatable_rows',
    output_schema: 'DataTableContext.v1',
    required_target_fields: ['asset_path', 'target_name'],
    request_builder_id: 'data_table',
    payload_projector_id: 'data_table_schema',
    supported_asset_types: ['data_table', 'data_table_row'],
  }),
  active('read.data_asset.schema.data_asset.property_json', 'data_asset', 'schema', 'data_asset', 'property_json', {
    read_type: 'data_asset_context',
    target_type: 'data_asset',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_asset_object_template.json',
    bridge_command: 'get_object_properties',
    output_schema: 'DataAssetContext.v1',
    required_target_fields: ['asset_path'],
    request_builder_id: 'data_asset',
    payload_projector_id: 'object_property',
    supported_asset_types: ['data_asset'],
  }),
  active('read.blueprint.properties.property.property_json', 'blueprint', 'properties', 'property', 'property_json', {
    read_type: 'object_property_context',
    target_type: 'property',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_asset_object_template.json',
    bridge_command: 'get_object_properties',
    output_schema: 'ObjectPropertyContext.v1',
    required_target_fields: ['asset_path', 'target_name'],
    request_builder_id: 'object_property',
    payload_projector_id: 'object_property',
    supported_asset_types: ['blueprint', 'object_property', 'property'],
  }),
  ...materialLogicRoutes(['logic_json', 'logic_flow', 'logic_md']),
  reserved('read.material_instance.schema.asset.schema_json', 'material_instance', 'schema', 'asset', 'schema_json', 'MaterialInstance ReadContext runtime adapter is not active yet.'),
  reserved('read.animation_blueprint.logic.graph.logic_json', 'animation_blueprint', 'logic', 'graph', 'logic_json', 'Animation ReadContext runtime adapter is not active yet.'),
];

export function getAllReadContextRouteDescriptors(): readonly ReadContextRouteDescriptor[] {
  return READ_CONTEXT_ROUTE_DESCRIPTORS;
}

export function getActiveReadContextRouteDescriptors(): readonly ReadContextRouteDescriptor[] {
  return READ_CONTEXT_ROUTE_DESCRIPTORS.filter((route) => route.status === 'active');
}

export function getReadContextRouteDescriptor(routeId: string): ReadContextRouteDescriptor | undefined {
  return READ_CONTEXT_ROUTE_DESCRIPTORS.find((route) => route.route_id === routeId);
}

function blueprintLogicRoutes(
  targetKind: 'blueprint' | 'function' | 'event' | 'custom_event' | 'graph',
  formats: readonly ('logic_flow' | 'logic_json')[],
): ReadContextRouteDescriptor[] {
  return formats.map((format) => active(`read.blueprint.logic.${targetKind}.${format}`, 'blueprint', 'logic', targetKind, format, {
    read_type: 'blueprint_logic',
    target_type: targetKind,
    format,
    base_template_path: BLUEPRINT_LOGIC_TEMPLATE_BY_TARGET[targetKind],
    bridge_command: 'read_blueprint_logic_json',
    output_schema: outputSchemaForLogicFormat(format),
    required_target_fields: targetKind === 'blueprint' ? ['asset_path'] : ['asset_path', 'target_name'],
    request_builder_id: 'blueprint_logic',
    payload_projector_id: 'logic',
    supported_asset_types: ['blueprint', targetKind],
  }));
}

function outputSchemaForLogicFormat(format: 'logic_flow' | 'logic_json'): string {
  const outputSchemas: Readonly<Record<typeof format, string>> = {
    logic_flow: 'LogicFlow.v1',
    logic_json: 'LogicJson.v1',
  };
  return outputSchemas[format];
}

function materialLogicRoutes(
  formats: readonly ('logic_flow' | 'logic_json' | 'logic_md')[],
): ReadContextRouteDescriptor[] {
  return formats.map((format) => active(`read.material.logic.graph.${format}`, 'material', 'logic', 'graph', format, {
    read_type: 'material_graph_context',
    target_type: 'material_graph',
    format,
    base_template_path: MATERIAL_LOGIC_TEMPLATE,
    bridge_command: format === 'logic_md' ? 'read_material_logic_md' : 'read_material_logic_json',
    output_schema: outputSchemaForMaterialLogicFormat(format),
    required_target_fields: ['asset_path'],
    request_builder_id: 'material_logic',
    payload_projector_id: 'logic',
    supported_asset_types: ['asset', 'material', 'material_graph'],
    supported_formats: [format],
  }));
}

function outputSchemaForMaterialLogicFormat(format: 'logic_flow' | 'logic_json' | 'logic_md'): string {
  const outputSchemas: Readonly<Record<typeof format, string>> = {
    logic_flow: 'LogicJson.v1',
    logic_json: 'LogicJson.v1',
    logic_md: 'LogicMd.v1',
  };
  return outputSchemas[format];
}

function active(
  routeId: string,
  domain: ReadContextRouteDescriptor['domain'],
  readCluster: string,
  targetKind: string,
  viewTemplate: ReadContextRouteDescriptor['view_template'],
  data: ActiveRouteData,
): ReadContextRouteDescriptor {
  return {
    route_id: routeId,
    domain,
    read_cluster: readCluster,
    target_kind: targetKind,
    view_template: viewTemplate,
    status: 'active',
    payload_schema: 'BlueprintHelper.ReadSpec.v1',
    ...data,
    supported_asset_types: data.supported_asset_types ?? uniqueStrings([domain, data.target_type]),
    supported_formats: data.supported_formats ?? uniqueStrings([data.format ?? viewTemplate]),
  };
}

function reserved(
  routeId: string,
  domain: ReadContextRouteDescriptor['domain'],
  readCluster: string,
  targetKind: string,
  viewTemplate: ReadContextRouteDescriptor['view_template'],
  reason: string,
): ReadContextRouteDescriptor {
  return {
    route_id: routeId,
    domain,
    read_cluster: readCluster,
    target_kind: targetKind,
    view_template: viewTemplate,
    read_type: 'asset_context',
    base_template_path: '',
    status: 'reserved',
    payload_schema: 'BlueprintHelper.ReadSpec.v1',
    output_schema: 'Reserved.v1',
    required_target_fields: [],
    request_builder_id: 'asset_context',
    payload_projector_id: 'asset_context',
    supported_asset_types: [],
    supported_formats: [],
    reason,
  };
}

function uniqueStrings(values: readonly (string | undefined)[]): string[] {
  return [...new Set(values.filter((value): value is string => typeof value === 'string' && value.length > 0))];
}
