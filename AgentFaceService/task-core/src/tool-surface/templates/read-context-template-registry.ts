import type { ReadContextRouteDescriptor } from './read-context-template-types.js';

type ActiveRouteData = Omit<
  ReadContextRouteDescriptor,
  | 'template_id'
  | 'family'
  | 'cluster'
  | 'description'
  | 'read_spec'
  | 'required_fields'
  | 'optional_fields'
  | 'context_evidence'
  | 'recommended_invocation'
  | 'allowed_tools'
  | 'stop_conditions'
  | 'status'
  | 'payload_schema'
  | 'supported_asset_types'
  | 'supported_formats'
> & {
  template_path: string;
  required_fields: string[];
  optional_fields?: string[];
  context_evidence?: Record<string, string>;
  stop_conditions?: string[];
} & Partial<Pick<ReadContextRouteDescriptor, 'supported_asset_types' | 'supported_formats'>>;

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
  ...blueprintLogicDeltaRoutes('function'),
  ...blueprintLogicDeltaRoutes('event'),
  ...blueprintLogicDeltaRoutes('custom_event'),
  ...blueprintLogicDeltaRoutes('graph'),
  active('blueprint.logic.block.json', 'blueprint', 'logic', 'Read BlueprintHelper-owned block as LogicJson.', {
    read_type: 'blueprint_logic',
    target_type: 'block',
    format: 'logic_json',
    template_path: BLUEPRINT_LOGIC_TEMPLATE_BY_TARGET.block,
    bridge_command: 'read_blueprint_logic_json',
    output_schema: 'LogicJson.v1',
    required_fields: ['target.asset_path', 'target.block_id'],
    optional_fields: ['view.detail', 'view.max_items'],
    request_builder_id: 'blueprint_logic',
    payload_projector_id: 'logic',
    supported_asset_types: ['blueprint', 'block'],
    supported_formats: ['logic_json'],
  }),
  active('blueprint.asset.diagnostics', 'blueprint', 'asset', 'Read Blueprint asset diagnostics.', {
    read_type: 'asset_context',
    target_type: 'asset',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_asset_object_template.json',
    bridge_command: 'get_asset_info',
    output_schema: 'AssetContext.v1',
    required_fields: ['target.asset_path'],
    request_builder_id: 'asset_context',
    payload_projector_id: 'asset_context',
    supported_asset_types: ['asset', 'blueprint'],
    supported_formats: ['diagnostics_json'],
    format: 'diagnostics_json',
  }),
  active('blueprint.schema.variables', 'blueprint', 'schema', 'Read Blueprint variables.', {
    read_type: 'variable_context',
    target_type: 'member_variable',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_variables_template.json',
    bridge_command: 'list_variables',
    output_schema: 'VariableContext.v1',
    required_fields: ['target.asset_path'],
    request_builder_id: 'variable_context',
    payload_projector_id: 'variable_schema',
    supported_asset_types: ['blueprint', 'member_variable'],
    supported_formats: ['schema_json'],
    format: 'schema_json',
  }),
  active('blueprint.schema.event_dispatchers', 'blueprint', 'schema', 'Read Blueprint event dispatchers.', {
    read_type: 'variable_context',
    target_type: 'event_dispatcher',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_variables_template.json',
    bridge_command: 'list_event_dispatchers',
    output_schema: 'EventDispatcherContext.v1',
    required_fields: ['target.asset_path'],
    request_builder_id: 'variable_context',
    payload_projector_id: 'variable_schema',
    supported_asset_types: ['blueprint', 'event_dispatcher'],
    supported_formats: ['schema_json'],
    format: 'schema_json',
  }),
  active('blueprint.structure.components', 'blueprint', 'structure', 'Read Blueprint component tree.', {
    read_type: 'component_context',
    target_type: 'blueprint',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/blueprint_components_template.json',
    bridge_command: 'read_components',
    output_schema: 'ComponentContext.v1',
    required_fields: ['target.asset_path'],
    request_builder_id: 'component_context',
    payload_projector_id: 'component_tree',
    supported_asset_types: ['blueprint', 'component'],
    supported_formats: ['tree_json'],
    format: 'tree_json',
  }),
  active('widget.structure.tree_json', 'widget_blueprint', 'structure', 'Read WidgetTree JSON.', {
    read_type: 'widget_context',
    target_type: 'blueprint',
    format: 'tree_json',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/widget_tree_template.json',
    bridge_command: 'get_widget_tree',
    output_schema: 'WidgetTreeJson.v1',
    required_fields: ['target.asset_path'],
    request_builder_id: 'widget_tree',
    payload_projector_id: 'widget_tree',
    supported_asset_types: ['widget_blueprint', 'blueprint'],
    supported_formats: ['tree_json'],
  }),
  active('widget.structure.tree_flow', 'widget_blueprint', 'structure', 'Read WidgetTree compact flow.', {
    read_type: 'widget_context',
    target_type: 'blueprint',
    format: 'logic_flow',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/widget_tree_template.json',
    bridge_command: 'get_widget_tree',
    output_schema: 'WidgetTreeLogicFlow.v1',
    required_fields: ['target.asset_path'],
    request_builder_id: 'widget_tree',
    payload_projector_id: 'widget_tree',
    supported_asset_types: ['widget_blueprint', 'blueprint'],
    supported_formats: ['logic_flow'],
  }),
  active('widget.properties.widget', 'widget_blueprint', 'properties', 'Read single widget properties.', {
    read_type: 'widget_context',
    target_type: 'widget',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/widget_property_template.json',
    bridge_command: 'get_widget_properties',
    output_schema: 'WidgetPropertyContext.v1',
    required_fields: ['target.asset_path', 'target.target_name'],
    request_builder_id: 'widget_property',
    payload_projector_id: 'object_property',
    supported_asset_types: ['widget_blueprint', 'widget'],
    supported_formats: ['property_json'],
    format: 'property_json',
  }),
  active('data_table.schema.table', 'data_table', 'schema', 'Read DataTable schema.', {
    read_type: 'data_table_context',
    target_type: 'data_table',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_table_template.json',
    bridge_command: 'get_datatable_rows',
    output_schema: 'DataTableContext.v1',
    required_fields: ['target.asset_path'],
    request_builder_id: 'data_table',
    payload_projector_id: 'data_table_schema',
    supported_asset_types: ['data_table'],
    supported_formats: ['schema_json'],
    format: 'schema_json',
  }),
  active('data_table.schema.row', 'data_table', 'schema', 'Read DataTable row.', {
    read_type: 'data_table_context',
    target_type: 'data_table_row',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_table_row_template.json',
    bridge_command: 'get_datatable_rows',
    output_schema: 'DataTableContext.v1',
    required_fields: ['target.asset_path', 'target.target_name'],
    request_builder_id: 'data_table',
    payload_projector_id: 'data_table_schema',
    supported_asset_types: ['data_table', 'data_table_row'],
    supported_formats: ['schema_json'],
    format: 'schema_json',
  }),
  active('data_asset.properties.object', 'data_asset', 'properties', 'Read DataAsset object properties.', {
    read_type: 'data_asset_context',
    target_type: 'data_asset',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_asset_object_template.json',
    bridge_command: 'get_object_properties',
    output_schema: 'DataAssetContext.v1',
    required_fields: ['target.asset_path'],
    request_builder_id: 'data_asset',
    payload_projector_id: 'object_property',
    supported_asset_types: ['data_asset'],
    supported_formats: ['property_json'],
    format: 'property_json',
  }),
  active('blueprint.properties.object', 'blueprint', 'properties', 'Read Blueprint object property.', {
    read_type: 'object_property_context',
    target_type: 'property',
    template_path: 'AgentFaceService/agent-guide/Templates/read/routes/data_asset_object_template.json',
    bridge_command: 'get_object_properties',
    output_schema: 'ObjectPropertyContext.v1',
    required_fields: ['target.asset_path', 'target.target_name'],
    request_builder_id: 'object_property',
    payload_projector_id: 'object_property',
    supported_asset_types: ['blueprint', 'object_property', 'property'],
    supported_formats: ['property_json'],
    format: 'property_json',
  }),
  ...materialLogicRoutes(['logic_json', 'logic_flow']),
  reserved('material_instance.schema.asset', 'material_instance', 'schema', 'MaterialInstance ReadContext runtime adapter is not active yet.'),
  reserved('animation_blueprint.logic.graph.json', 'animation_blueprint', 'logic', 'Animation ReadContext runtime adapter is not active yet.'),
];

export function getAllReadContextRouteDescriptors(): readonly ReadContextRouteDescriptor[] {
  return READ_CONTEXT_ROUTE_DESCRIPTORS;
}

export function getActiveReadContextRouteDescriptors(): readonly ReadContextRouteDescriptor[] {
  return READ_CONTEXT_ROUTE_DESCRIPTORS.filter((route) => route.status === 'active');
}

export function getReadContextRouteDescriptor(templateId: string): ReadContextRouteDescriptor | undefined {
  return READ_CONTEXT_ROUTE_DESCRIPTORS.find((route) => route.template_id === templateId);
}

function blueprintLogicRoutes(
  targetType: 'blueprint' | 'function' | 'event' | 'custom_event' | 'graph',
  formats: readonly ('logic_flow' | 'logic_json')[],
): ReadContextRouteDescriptor[] {
  return formats.map((format) => {
    const suffix = format === 'logic_flow' ? 'flow' : 'json';
    return active(`blueprint.logic.${targetType}.${suffix}`, 'blueprint', 'logic', `Read Blueprint ${targetType} as ${format}.`, {
      read_type: 'blueprint_logic',
      target_type: targetType,
      format,
      template_path: BLUEPRINT_LOGIC_TEMPLATE_BY_TARGET[targetType],
      bridge_command: 'read_blueprint_logic_json',
      output_schema: outputSchemaForLogicFormat(format),
      required_fields: targetType === 'blueprint'
        ? ['target.asset_path']
        : ['target.asset_path', 'target.target_name'],
      optional_fields: ['view.detail', 'view.max_items'],
      request_builder_id: 'blueprint_logic',
      payload_projector_id: 'logic',
      supported_asset_types: ['blueprint', targetType],
      supported_formats: [format],
    });
  });
}

function blueprintLogicDeltaRoutes(
  targetType: 'function' | 'event' | 'custom_event' | 'graph',
): ReadContextRouteDescriptor[] {
  return [
    active(`blueprint.logic.${targetType}.json_delta`, 'blueprint', 'logic', `Read Blueprint ${targetType} LogicJson delta after LogicFlow.`, {
      read_type: 'blueprint_logic',
      target_type: targetType,
      format: 'logic_json_delta_after_logic_flow',
      template_path: BLUEPRINT_LOGIC_TEMPLATE_BY_TARGET[targetType],
      bridge_command: 'read_blueprint_logic_json',
      output_schema: 'LogicJsonDeltaAfterLogicFlow.v1',
      required_fields: ['target.asset_path', 'target.target_name'],
      optional_fields: ['view.detail', 'view.max_items'],
      context_evidence: {
        'target.target_type.allowed_values': 'blueprint | function | event | custom_event | graph | block',
        'view.format.allowed_values': 'logic_flow | logic_json | logic_json_delta_after_logic_flow',
        'view.baseline_view.allowed_values': 'logic_flow',
        'view.detail.allowed_values': 'brief | normal | full | debug',
      },
      stop_conditions: [
        'missing_asset_path',
        'missing_target_name',
        'read_context_screenshot_conflict',
        'runtime_capability_missing',
      ],
      request_builder_id: 'blueprint_logic',
      payload_projector_id: 'logic',
      supported_asset_types: ['blueprint', targetType],
      supported_formats: ['logic_json_delta_after_logic_flow'],
    }),
  ];
}

function outputSchemaForLogicFormat(format: 'logic_flow' | 'logic_json'): string {
  const outputSchemas: Readonly<Record<typeof format, string>> = {
    logic_flow: 'LogicFlow.v1',
    logic_json: 'LogicJson.v1',
  };
  return outputSchemas[format];
}

function materialLogicRoutes(
  formats: readonly ('logic_flow' | 'logic_json')[],
): ReadContextRouteDescriptor[] {
  return formats.map((format) => {
    const suffix = format === 'logic_flow' ? 'flow' : 'json';
    return active(`material.logic.graph.${suffix}`, 'material', 'logic', `Read Material graph as ${format}.`, {
      read_type: 'material_graph_context',
      target_type: 'material_graph',
      format,
      template_path: MATERIAL_LOGIC_TEMPLATE,
      bridge_command: 'read_material_logic_json',
      output_schema: outputSchemaForLogicFormat(format),
      required_fields: ['target.asset_path'],
      optional_fields: ['view.detail', 'view.max_items'],
      request_builder_id: 'material_logic',
      payload_projector_id: 'logic',
      supported_asset_types: ['asset', 'material', 'material_graph'],
      supported_formats: [format],
    });
  });
}

function active(
  templateId: string,
  family: ReadContextRouteDescriptor['family'],
  cluster: string,
  description: string,
  data: ActiveRouteData,
): ReadContextRouteDescriptor {
  const {
    template_path: templatePath,
    required_fields: requiredFields,
    optional_fields: optionalFields,
    context_evidence: contextEvidence,
    stop_conditions: stopConditions,
    supported_asset_types: supportedAssetTypes,
    supported_formats: supportedFormats,
    ...runtimeData
  } = data;
  const readSpec = composeDescriptorReadSpec(data);
  return {
    template_id: templateId,
    family,
    cluster,
    description,
    template_path: templatePath,
    read_spec: readSpec,
    required_fields: requiredFields,
    optional_fields: optionalFields ?? [],
    context_evidence: contextEvidence ?? defaultContextEvidence(data),
    recommended_invocation: 'bh context read --file <read-spec.json> --format json',
    allowed_tools: ['bh tools read-templates compose', 'bh context read'],
    stop_conditions: stopConditions ?? [
      'missing_asset_path',
      'read_context_screenshot_conflict',
      'runtime_capability_missing',
    ],
    status: 'active',
    payload_schema: 'BlueprintHelper.ReadSpec.v1',
    ...runtimeData,
    supported_asset_types: supportedAssetTypes ?? uniqueStrings([family, data.target_type]),
    supported_formats: supportedFormats ?? uniqueStrings([data.format]),
  };
}

function reserved(
  templateId: string,
  family: ReadContextRouteDescriptor['family'],
  cluster: string,
  reason: string,
): ReadContextRouteDescriptor {
  const data: ActiveRouteData = {
    read_type: 'asset_context',
    template_path: '',
    output_schema: 'Reserved.v1',
    required_fields: [],
    request_builder_id: 'asset_context',
    payload_projector_id: 'asset_context',
    supported_asset_types: [],
    supported_formats: [],
    reason,
  };
  return {
    template_id: templateId,
    family,
    cluster,
    description: reason,
    template_path: '',
    read_spec: composeDescriptorReadSpec(data),
    required_fields: [],
    optional_fields: [],
    context_evidence: {},
    output_schema: 'Reserved.v1',
    recommended_invocation: 'bh context read --file <read-spec.json> --format json',
    allowed_tools: ['bh tools read-templates compose', 'bh context read'],
    stop_conditions: ['runtime_capability_missing'],
    status: 'reserved',
    payload_schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'asset_context',
    request_builder_id: 'asset_context',
    payload_projector_id: 'asset_context',
    supported_asset_types: [],
    supported_formats: [],
    reason,
  };
}

function composeDescriptorReadSpec(data: ActiveRouteData): ReadContextRouteDescriptor['read_spec'] {
  const target: Record<string, unknown> = {};
  if (data.required_fields.includes('target.asset_path')) {
    target['asset_path'] = '__REQUIRED_ASSET_PATH__';
  }
  if (data.target_type) {
    target['target_type'] = data.target_type;
  }
  if (data.required_fields.includes('target.target_name')) {
    target['target_name'] = '__REQUIRED_TARGET_NAME__';
  }
  if (data.required_fields.includes('target.block_id')) {
    target['block_id'] = '__REQUIRED_BLOCK_ID__';
  }

  const readSpec: ReadContextRouteDescriptor['read_spec'] = {
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: data.read_type,
    target,
  };
  if (shouldEmitReadSpecView(data)) {
    readSpec.view = {
      format: data.format,
    };
    if (data.format === 'logic_json_delta_after_logic_flow') {
      readSpec.view['baseline_view'] = 'logic_flow';
    }
  }
  return readSpec;
}

function shouldEmitReadSpecView(data: Pick<ActiveRouteData, 'read_type' | 'target_type' | 'format'>): boolean {
  const format = data.format;
  if (format === undefined) {
    return false;
  }
  if (data.read_type === 'blueprint_logic') {
    return format === 'logic_flow'
      || format === 'logic_json'
      || format === 'logic_json_delta_after_logic_flow';
  }
  if (data.read_type === 'material_graph_context') {
    return format === 'logic_flow' || format === 'logic_json';
  }
  if (data.read_type === 'widget_context' && data.target_type === 'blueprint') {
    return format === 'tree_json' || format === 'logic_flow';
  }
  return false;
}

function defaultContextEvidence(data: Pick<ActiveRouteData, 'read_type' | 'target_type' | 'format'>): Record<string, string> {
  const evidence: Record<string, string> = {
    'target.target_type.allowed_values': 'blueprint | function | event | custom_event | graph | block | widget | data_table | data_table_row | data_asset | property | material_graph',
  };
  const format = data.format;
  if (typeof format === 'string') {
    if (shouldEmitReadSpecView(data)) {
      evidence['view.format.allowed_values'] = format === 'logic_flow' || format === 'logic_json' || format === 'logic_json_delta_after_logic_flow'
        ? 'logic_flow | logic_json | logic_json_delta_after_logic_flow'
        : format;
    } else {
      evidence['output.format'] = format;
    }
  }
  evidence['view.detail.allowed_values'] = 'brief | normal | full | debug';
  return evidence;
}

function uniqueStrings(values: readonly (string | undefined)[]): string[] {
  return [...new Set(values.filter((value): value is string => typeof value === 'string' && value.length > 0))];
}
