export type TaskSpecTemplateFamily =
  | 'graph_write'
  | NonGraphWriteFamily;

export type GraphWriteTemplateWriteMode =
  | 'graph.append'
  | 'graph.replace'
  | 'graph.merge'
  | 'graph.patch';

export type NonGraphWriteFamily =
  | 'blueprint_variables'
  | 'blueprint_components'
  | 'blueprint_class_settings'
  | 'blueprint_signature'
  | 'blueprint_create_feature'
  | 'umg_widget'
  | 'data_table'
  | 'object_properties'
  | 'asset_factory'
  | 'material_graph';

export type NonGraphWriteTemplateWriteMode =
  | 'variables.edit'
  | 'components.edit'
  | 'class_settings.edit'
  | 'signature.edit'
  | 'feature.create'
  | 'widget.edit'
  | 'table.rows'
  | 'object.properties'
  | 'asset.create'
  | 'material.graph';

export type TaskSpecTemplateWriteMode =
  | GraphWriteTemplateWriteMode
  | NonGraphWriteTemplateWriteMode;

export type WriteValidationClassification =
  | 'preview_decidable'
  | 'runtime_only'
  | 'shared_policy';

export interface TaskSpecTemplateDiagnostic {
  code: string;
  family?: string;
  write_mode?: string;
  cluster_id?: string;
  operation_id?: string;
  template_id?: string;
  path?: string;
  message?: string;
}

export interface TaskSpecTemplateFamilyItem {
  family: TaskSpecTemplateFamily;
  task_type: string;
  description: string;
  status: 'supported';
}

export interface TaskSpecTemplateWriteModeItem {
  family: TaskSpecTemplateFamily;
  write_mode: TaskSpecTemplateWriteMode;
  description: string;
  base_template_path: string;
}

export interface TaskSpecTemplateClusterItem {
  family: TaskSpecTemplateFamily;
  cluster_id: string;
  description: string;
  unsupported_write_modes: TaskSpecTemplateWriteMode[];
}

export interface TaskSpecTemplateOperationItem {
  family: TaskSpecTemplateFamily;
  cluster_id: string;
  operation_id: string;
  description: string;
  validation_classification: WriteValidationClassification;
  runtime_only_validation_notes?: string[];
}

export interface TaskSpecTemplateQuickAccessItem {
  template_id: string;
  family: TaskSpecTemplateFamily;
  write_mode: TaskSpecTemplateWriteMode;
  cluster_id: string;
  operation_id: string;
  quick_access_id: string;
  source_slot_id: string;
  slot_type: 'statement' | 'expression' | 'route';
  arg_slots: string[];
  template_path: string;
  insert_paths: string[];
  unsupported_write_modes: TaskSpecTemplateWriteMode[];
  validation_classification: WriteValidationClassification;
  runtime_only_validation_notes?: string[];
}

export interface TaskSpecTemplateFamiliesResult {
  schema: 'BlueprintHelper.TaskSpecTemplateFamilies.v1';
  workflow: 'preview_execute';
  items: TaskSpecTemplateFamilyItem[];
}

export interface TaskSpecTemplateWriteModesResult {
  schema: 'BlueprintHelper.TaskSpecTemplateWriteModes.v1';
  family: string;
  items: TaskSpecTemplateWriteModeItem[];
}

export interface TaskSpecTemplateClustersResult {
  schema: 'BlueprintHelper.TaskSpecTemplateClusters.v1';
  family: string;
  items: TaskSpecTemplateClusterItem[];
}

export interface TaskSpecTemplateOperationsResult {
  schema: 'BlueprintHelper.TaskSpecTemplateOperations.v1';
  family: string;
  cluster_id: string;
  write_mode: string;
  items: TaskSpecTemplateOperationItem[];
}

export interface TaskSpecTemplateQuickAccessResult {
  schema: 'BlueprintHelper.TaskSpecTemplateQuickAccess.v1';
  family: string;
  cluster_id: string;
  operation_id: string;
  write_mode: string;
  items: TaskSpecTemplateQuickAccessItem[];
}

export interface ComposeTaskSpecTemplateInput {
  family: TaskSpecTemplateFamily | string;
  writeMode: TaskSpecTemplateWriteMode | string;
  templateIds: string[];
  outputPath: string;
}

export type TaskSpecTemplateCompositionResult =
  | {
    schema: 'BlueprintHelper.TaskSpecTemplateComposition.v1';
    status: 'ok';
    family: string;
    write_mode: string;
    output_path: string;
    next: {
      preview_command: string;
      execute_command: string;
    };
  }
  | {
    schema: 'BlueprintHelper.TaskSpecTemplateComposition.v1';
    status: 'failed';
    family: string;
    write_mode: string;
    diagnostics: TaskSpecTemplateDiagnostic[];
  };

export interface NonGraphWriteTemplateFamilyMetadata {
  family: NonGraphWriteFamily;
  task_type: string;
  description: string;
  strategy_field: string;
  base_template_path: string;
  insert_targets: string[];
  status: 'supported' | 'blocked';
  blocked_until?: string[];
  write_mode?: NonGraphWriteTemplateWriteMode;
}
