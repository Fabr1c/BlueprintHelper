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
  | 'material_graph'
  | 'material_instance';

export type TaskSpecTemplateWriteMode = GraphWriteTemplateWriteMode;

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
  safe_next_action?: string;
  suggested_route?: string;
  message?: string;
}

export type TaskSpecTemplateNavigationLevel =
  | 'write_mode'
  | 'cluster'
  | 'operation'
  | 'quick_access'
  | 'leaf_template';

export interface TaskSpecTemplateFamilyNavigation {
  levels: readonly TaskSpecTemplateNavigationLevel[];
  next_command: string;
  compose_command: string;
  requires_write_mode: boolean;
}

export interface TaskSpecTemplateFamilyItem {
  family: TaskSpecTemplateFamily;
  task_type: string;
  description: string;
  status: 'supported';
  navigation: TaskSpecTemplateFamilyNavigation;
}

export interface TaskSpecTemplateWriteModeItem {
  family: TaskSpecTemplateFamily;
  write_mode: TaskSpecTemplateWriteMode;
  description: string;
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
  write_mode?: TaskSpecTemplateWriteMode;
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
  guidance: string;
  items: TaskSpecTemplateFamilyItem[];
}

export interface TaskSpecTemplateWriteModesResult {
  schema: 'BlueprintHelper.TaskSpecTemplateWriteModes.v1';
  family: string;
  status: 'ok' | 'failed';
  guidance: string;
  items: TaskSpecTemplateWriteModeItem[];
  diagnostics?: TaskSpecTemplateDiagnostic[];
}

export interface TaskSpecTemplateClustersResult {
  schema: 'BlueprintHelper.TaskSpecTemplateClusters.v1';
  family: string;
  guidance: string;
  items: TaskSpecTemplateClusterItem[];
}

export interface TaskSpecTemplateOperationsResult {
  schema: 'BlueprintHelper.TaskSpecTemplateOperations.v1';
  family: string;
  cluster_id?: string;
  write_mode?: string;
  guidance: string;
  items: TaskSpecTemplateOperationItem[];
}

export interface TaskSpecTemplateQuickAccessResult {
  schema: 'BlueprintHelper.TaskSpecTemplateQuickAccess.v1';
  family: string;
  cluster_id?: string;
  operation_id?: string;
  write_mode?: string;
  items: TaskSpecTemplateQuickAccessItem[];
}

export interface ComposeTaskSpecTemplateInput {
  family?: TaskSpecTemplateFamily | string;
  writeMode?: TaskSpecTemplateWriteMode | string;
  templateIds?: string[];
  templateId?: string;
  outputPath: string;
}

export interface TaskSpecTemplateRequiredPlaceholder {
  path: string;
  placeholder: string;
  meaning?: string;
  expected_source?: string;
}

export type TaskSpecTemplateCompositionResult =
  | {
    schema: 'BlueprintHelper.TaskSpecTemplateComposition.v1';
    status: 'ok';
    family: string;
    write_mode?: string;
    output_path: string;
    required_placeholders: TaskSpecTemplateRequiredPlaceholder[];
    next: {
      preview_command: string;
      execute_command: string;
    };
  }
  | {
    schema: 'BlueprintHelper.TaskSpecTemplateComposition.v1';
    status: 'failed';
    family?: string;
    write_mode?: string;
    diagnostics: TaskSpecTemplateDiagnostic[];
  };

export interface NonGraphWriteTemplateFamilyMetadata {
  family: NonGraphWriteFamily;
  task_type: string;
  description: string;
  strategy_field: string;
  internal_scaffold_template_path: string;
  insert_targets: string[];
  navigation: TaskSpecTemplateFamilyNavigation;
  status: 'supported' | 'blocked';
  blocked_until?: string[];
}
