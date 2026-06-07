import type { NonGraphWriteTemplateFamilyMetadata } from './taskspec-template-types.js';
import { UMG_WIDGET_OPERATION_MANIFEST } from './generated/umg-widget-operation-manifest.generated.js';
import type {
  TaskSpecTemplateClusterItem,
  TaskSpecTemplateOperationItem,
  TaskSpecTemplateQuickAccessItem,
} from './taskspec-template-types.js';

export const NON_GRAPHWRITE_TEMPLATE_FAMILIES: readonly NonGraphWriteTemplateFamilyMetadata[] = [
  {
    family: 'blueprint_variables',
    task_type: 'edit_blueprint_variables',
    description: 'Edit Blueprint member variables, defaults, and related variable metadata.',
    strategy_field: 'variable_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_variables_template.json',
    insert_targets: ['behavior.variables[]', 'behavior.changes[]', 'behavior.defaults[]'],
    status: 'supported',
    write_mode: 'variables.edit',
  },
  {
    family: 'blueprint_components',
    task_type: 'edit_blueprint_components',
    description: 'Edit Blueprint component tree entries when the dedicated component template path is active.',
    strategy_field: 'component_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
    insert_targets: ['behavior.changes[]'],
    status: 'blocked',
    blocked_until: ['dedicated component TaskSpec template path is separated from create_blueprint_feature route template'],
  },
  {
    family: 'blueprint_class_settings',
    task_type: 'edit_blueprint_class_settings',
    description: 'Edit Blueprint class settings such as interfaces, defaults, and reparenting.',
    strategy_field: 'class_settings_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
    insert_targets: ['behavior.interfaces', 'behavior.class_defaults[]', 'behavior.reparent'],
    status: 'blocked',
    blocked_until: ['dedicated class settings TaskSpec template path is separated from create_blueprint_feature route template'],
  },
  {
    family: 'blueprint_signature',
    task_type: 'edit_blueprint_signature',
    description: 'Edit function, event, or macro signatures before writing bodies.',
    strategy_field: 'signature_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
    insert_targets: ['behavior.changes[]'],
    status: 'blocked',
    blocked_until: ['dedicated signature TaskSpec template path is added'],
  },
  {
    family: 'blueprint_create_feature',
    task_type: 'create_blueprint_feature',
    description: 'Create a composite Blueprint feature including components, variables, class settings, and behavior.',
    strategy_field: 'composite',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
    insert_targets: ['components[]', 'variables[]', 'class_settings', 'behavior'],
    status: 'supported',
    write_mode: 'feature.create',
  },
  {
    family: 'umg_widget',
    task_type: 'edit_umg_widget',
    description: 'Edit Widget Blueprint tree, widget properties, named slots, and widget class settings.',
    strategy_field: 'widget_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/umg_widget_edit_template.json',
    insert_targets: ['behavior.changes[]'],
    status: 'supported',
    write_mode: 'widget.edit',
  },
  {
    family: 'data_table',
    task_type: 'edit_data_table',
    description: 'Edit DataTable rows through TaskSpec row operations.',
    strategy_field: 'row_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/data_table_rows_edit_template.json',
    insert_targets: ['behavior.rows[]'],
    status: 'supported',
    write_mode: 'table.rows',
  },
  {
    family: 'object_properties',
    task_type: 'edit_object_properties',
    description: 'Edit UObject, DataAsset, or Blueprint object properties.',
    strategy_field: 'property_strategy',
    base_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/data_object_properties_edit_template.json',
    insert_targets: ['behavior.changes[]'],
    status: 'supported',
    write_mode: 'object.properties',
  },
  {
    family: 'asset_factory',
    task_type: 'create_asset',
    description: 'Create supported Unreal assets through asset factory TaskSpec routes.',
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

export function listNonGraphWriteTemplateClusters(input: {
  family: string;
}): TaskSpecTemplateClusterItem[] {
  if (input.family !== 'umg_widget') {
    return [];
  }
  return [
    {
      family: 'umg_widget',
      cluster_id: 'widget_tree',
      description: 'Widget tree and Widget Blueprint structural edit operations.',
      unsupported_write_modes: [],
    },
  ];
}

export function listNonGraphWriteTemplateOperations(input: {
  family: string;
  cluster: string;
  writeMode: string;
}): TaskSpecTemplateOperationItem[] {
  if (input.family !== 'umg_widget' || input.cluster !== 'widget_tree' || input.writeMode !== 'widget.edit') {
    return [];
  }
  return UMG_WIDGET_OPERATION_MANIFEST.map((descriptor) => ({
    family: 'umg_widget',
    cluster_id: 'widget_tree',
    operation_id: descriptor.kind,
    description: describeUmgWidgetOperation(descriptor.kind),
  }));
}

function describeUmgWidgetOperation(kind: string): string {
  const words = kind.replaceAll('_', ' ');
  return `Use for ${words} Widget Blueprint operations.`;
}

export function listNonGraphWriteTemplateQuickAccess(input: {
  family: string;
  cluster: string;
  operation: string;
  writeMode: string;
}): TaskSpecTemplateQuickAccessItem[] {
  if (input.family !== 'umg_widget' || input.cluster !== 'widget_tree' || input.writeMode !== 'widget.edit') {
    return [];
  }
  return UMG_WIDGET_OPERATION_MANIFEST
    .filter((descriptor) => input.operation.length === 0 || descriptor.kind === input.operation)
    .map((descriptor) => ({
      template_id: `umg.widget_tree.${descriptor.kind}`,
      family: 'umg_widget',
      write_mode: 'widget.edit',
      cluster_id: 'widget_tree',
      operation_id: descriptor.kind,
      quick_access_id: descriptor.kind,
      source_slot_id: descriptor.kind,
      slot_type: 'statement',
      arg_slots: descriptor.required_fields.map((field) => `${field}(*)`),
      template_path: 'AgentFaceService/agent-guide/Templates/write/routes/umg_widget_edit_template.json',
      insert_paths: ['behavior.changes[]'],
      unsupported_write_modes: [],
    }));
}
