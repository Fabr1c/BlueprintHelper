import type { ReadContextRouteDescriptor } from './read-context-template-types.js';

const READ_CONTEXT_ROUTE_DESCRIPTORS: readonly ReadContextRouteDescriptor[] = [
  active('read.blueprint.logic.function.logic_flow', 'blueprint', 'logic', 'function', 'logic_flow', {
    read_type: 'blueprint_logic',
    target_type: 'function',
    format: 'logic_flow',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_function_logic_flow_template.json',
    bridge_command: 'read_blueprint_logic_json',
    output_schema: 'LogicFlow.v1',
    required_target_fields: ['asset_path', 'target_name'],
  }),
  active('read.blueprint.logic.event.logic_flow', 'blueprint', 'logic', 'event', 'logic_flow', {
    read_type: 'blueprint_logic',
    target_type: 'event',
    format: 'logic_flow',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_event_logic_flow_template.json',
    bridge_command: 'read_blueprint_logic_json',
    output_schema: 'LogicFlow.v1',
    required_target_fields: ['asset_path', 'target_name'],
  }),
  active('read.blueprint.logic.custom_event.logic_flow', 'blueprint', 'logic', 'custom_event', 'logic_flow', {
    read_type: 'blueprint_logic',
    target_type: 'custom_event',
    format: 'logic_flow',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_custom_event_logic_flow_template.json',
    bridge_command: 'read_blueprint_logic_json',
    output_schema: 'LogicFlow.v1',
    required_target_fields: ['asset_path', 'target_name'],
  }),
  active('read.blueprint.logic.graph.logic_json', 'blueprint', 'logic', 'graph', 'logic_json', {
    read_type: 'graph_context',
    target_type: 'graph',
    format: 'logic_json',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_graph_logic_json_template.json',
    bridge_command: 'read_blueprint_logic_json',
    output_schema: 'LogicJson.v1',
    required_target_fields: ['asset_path', 'target_name'],
  }),
  active('read.blueprint.logic.block.logic_json', 'blueprint', 'logic', 'block', 'logic_json', {
    read_type: 'blueprint_logic',
    target_type: 'block',
    format: 'logic_json',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_logic_block_logic_json_template.json',
    bridge_command: 'read_blueprint_logic_json',
    output_schema: 'LogicJson.v1',
    required_target_fields: ['asset_path', 'block_id'],
  }),
  active('read.blueprint.variables.blueprint.schema_json', 'blueprint', 'variables', 'blueprint', 'schema_json', {
    read_type: 'variable_context',
    target_type: 'member_variable',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_variables_template.json',
    bridge_command: 'list_variables',
    output_schema: 'VariableContext.v1',
    required_target_fields: ['asset_path'],
  }),
  active('read.blueprint.components.blueprint.tree_json', 'blueprint', 'components', 'blueprint', 'tree_json', {
    read_type: 'component_context',
    target_type: 'blueprint',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_components_template.json',
    bridge_command: 'read_components',
    output_schema: 'ComponentContext.v1',
    required_target_fields: ['asset_path'],
  }),
  active('read.widget_blueprint.structure_tree.widget_tree.tree_json', 'widget_blueprint', 'structure_tree', 'widget_tree', 'tree_json', {
    read_type: 'widget_context',
    target_type: 'blueprint',
    format: 'logic_json',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/widget_tree_template.json',
    bridge_command: 'get_widget_tree',
    output_schema: 'WidgetContext.v1',
    required_target_fields: ['asset_path'],
  }),
  active('read.widget_blueprint.structure_tree.widget_tree.logic_flow', 'widget_blueprint', 'structure_tree', 'widget_tree', 'logic_flow', {
    read_type: 'widget_context',
    target_type: 'blueprint',
    format: 'logic_flow',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/widget_tree_template.json',
    bridge_command: 'get_widget_tree',
    output_schema: 'WidgetTreeLogicFlow.v1',
    required_target_fields: ['asset_path'],
  }),
  active('read.widget_blueprint.structure_tree.widget.property_json', 'widget_blueprint', 'structure_tree', 'widget', 'property_json', {
    read_type: 'widget_context',
    target_type: 'widget',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/widget_property_template.json',
    bridge_command: 'get_widget_properties',
    output_schema: 'WidgetPropertyContext.v1',
    required_target_fields: ['asset_path', 'target_name'],
  }),
  active('read.data_table.schema.data_table.schema_json', 'data_table', 'schema', 'data_table', 'schema_json', {
    read_type: 'data_table_context',
    target_type: 'data_table',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_table_template.json',
    bridge_command: 'get_datatable_rows',
    output_schema: 'DataTableContext.v1',
    required_target_fields: ['asset_path'],
  }),
  active('read.data_table.schema.data_table_row.schema_json', 'data_table', 'schema', 'data_table_row', 'schema_json', {
    read_type: 'data_table_context',
    target_type: 'data_table_row',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_table_row_template.json',
    bridge_command: 'get_datatable_rows',
    output_schema: 'DataTableContext.v1',
    required_target_fields: ['asset_path', 'target_name'],
  }),
  active('read.data_asset.schema.data_asset.property_json', 'data_asset', 'schema', 'data_asset', 'property_json', {
    read_type: 'data_asset_context',
    target_type: 'data_asset',
    base_template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_asset_object_template.json',
    bridge_command: 'get_object_properties',
    output_schema: 'DataAssetContext.v1',
    required_target_fields: ['asset_path'],
  }),
  reserved('read.material.logic.graph.logic_json', 'material', 'logic', 'graph', 'logic_json', 'Material ReadContext runtime adapter is not active yet.'),
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

function active(
  routeId: string,
  domain: ReadContextRouteDescriptor['domain'],
  readCluster: string,
  targetKind: string,
  viewTemplate: ReadContextRouteDescriptor['view_template'],
  data: Omit<ReadContextRouteDescriptor, 'route_id' | 'domain' | 'read_cluster' | 'target_kind' | 'view_template' | 'status' | 'payload_schema'>,
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
    reason,
  };
}
