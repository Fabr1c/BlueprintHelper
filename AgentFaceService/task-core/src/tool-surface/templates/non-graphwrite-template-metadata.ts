import type { NonGraphWriteTemplateFamilyMetadata } from './taskspec-template-types.js';

export const NON_GRAPHWRITE_TEMPLATE_FAMILIES: readonly NonGraphWriteTemplateFamilyMetadata[] = [
  {
    family: 'blueprint_variables',
    task_type: 'edit_blueprint_variables',
    strategy_field: 'variable_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_variables_template.json',
    insert_targets: ['behavior.variables[]', 'behavior.changes[]', 'behavior.defaults[]'],
    status: 'supported',
    write_mode: 'variables.edit',
  },
  {
    family: 'blueprint_components',
    task_type: 'edit_blueprint_components',
    strategy_field: 'component_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
    insert_targets: ['behavior.changes[]'],
    status: 'blocked',
    blocked_until: ['dedicated component TaskSpec template path is separated from create_blueprint_feature route template'],
  },
  {
    family: 'blueprint_class_settings',
    task_type: 'edit_blueprint_class_settings',
    strategy_field: 'class_settings_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
    insert_targets: ['behavior.interfaces', 'behavior.class_defaults[]', 'behavior.reparent'],
    status: 'blocked',
    blocked_until: ['dedicated class settings TaskSpec template path is separated from create_blueprint_feature route template'],
  },
  {
    family: 'blueprint_signature',
    task_type: 'edit_blueprint_signature',
    strategy_field: 'signature_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
    insert_targets: ['behavior.changes[]'],
    status: 'blocked',
    blocked_until: ['dedicated signature TaskSpec template path is added'],
  },
  {
    family: 'blueprint_create_feature',
    task_type: 'create_blueprint_feature',
    strategy_field: 'composite',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
    insert_targets: ['components[]', 'variables[]', 'class_settings', 'behavior'],
    status: 'supported',
    write_mode: 'feature.create',
  },
  {
    family: 'umg_widget',
    task_type: 'edit_umg_widget',
    strategy_field: 'widget_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/umg_widget_edit_template.json',
    insert_targets: ['behavior.changes[]'],
    status: 'supported',
    write_mode: 'widget.edit',
  },
  {
    family: 'data_table',
    task_type: 'edit_data_table',
    strategy_field: 'row_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/data_table_rows_edit_template.json',
    insert_targets: ['behavior.rows[]'],
    status: 'supported',
    write_mode: 'table.rows',
  },
  {
    family: 'object_properties',
    task_type: 'edit_object_properties',
    strategy_field: 'property_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/data_object_properties_edit_template.json',
    insert_targets: ['behavior.changes[]'],
    status: 'supported',
    write_mode: 'object.properties',
  },
  {
    family: 'asset_factory',
    task_type: 'create_asset',
    strategy_field: 'asset_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/data_asset_create_template.json',
    insert_targets: ['behavior.asset'],
    status: 'supported',
    write_mode: 'asset.create',
  },
];

export function getSupportedNonGraphWriteTemplateFamilies(): readonly NonGraphWriteTemplateFamilyMetadata[] {
  return NON_GRAPHWRITE_TEMPLATE_FAMILIES.filter((entry) => entry.status === 'supported' && entry.write_mode);
}

export function getNonGraphWriteTemplateFamily(
  family: string,
): NonGraphWriteTemplateFamilyMetadata | undefined {
  return NON_GRAPHWRITE_TEMPLATE_FAMILIES.find((entry) => entry.family === family);
}
