import type { ReadContextInput } from '../bridge/read-context/read-context-schemas.js';

export type ReadContextTemplateDomain =
  | 'blueprint'
  | 'widget_blueprint'
  | 'data_table'
  | 'data_asset'
  | 'material'
  | 'material_instance'
  | 'animation_blueprint';

export type ReadContextTemplateRouteStatus =
  | 'active'
  | 'reserved'
  | 'hidden'
  | 'blocked';

export type ReadContextTemplateView =
  | 'logic_flow'
  | 'logic_json'
  | 'tree_json'
  | 'schema_json'
  | 'property_json'
  | 'diagnostics_json';

export type ReadContextRequestBuilderId =
  | 'blueprint_logic'
  | 'asset_context'
  | 'component_context'
  | 'variable_context'
  | 'widget_tree'
  | 'widget_property'
  | 'data_table'
  | 'data_asset'
  | 'object_property';

export type ReadContextPayloadProjectorId =
  | 'logic'
  | 'asset_context'
  | 'widget_tree'
  | 'component_tree'
  | 'variable_schema'
  | 'data_table_schema'
  | 'object_property';

export interface ReadContextTemplateDiagnostic {
  code: string;
  domain?: string;
  read_cluster?: string;
  target_kind?: string;
  view_template?: string;
  template_id?: string;
  path?: string;
  message?: string;
}

export interface ReadContextRouteDescriptor {
  route_id: string;
  domain: ReadContextTemplateDomain;
  read_cluster: string;
  target_kind: string;
  view_template: ReadContextTemplateView;
  read_type: ReadContextInput['read_type'] | string;
  target_type?: NonNullable<ReadContextInput['target']['target_type']> | string;
  format?: ReadContextTemplateView | string;
  base_template_path: string;
  status: ReadContextTemplateRouteStatus;
  payload_schema: 'BlueprintHelper.ReadSpec.v1';
  bridge_command?: string;
  output_schema: string;
  required_target_fields: string[];
  request_builder_id: ReadContextRequestBuilderId;
  payload_projector_id: ReadContextPayloadProjectorId;
  supported_asset_types: readonly string[];
  supported_formats: readonly string[];
  reason?: string;
}

export interface ReadContextTemplateDomainItem {
  domain: string;
  description: string;
  status: 'supported';
}

export interface ReadContextTemplateClusterItem {
  domain: string;
  read_cluster: string;
  description: string;
}

export interface ReadContextTemplateTargetItem {
  domain: string;
  read_cluster: string;
  target_kind: string;
  description: string;
  required_target_fields: string[];
}

export interface ReadContextTemplateViewItem {
  domain: string;
  read_cluster: string;
  target_kind: string;
  view_template: string;
  description: string;
  output_schema: string;
}

export interface ReadContextTemplateQuickAccessItem {
  template_id: string;
  domain: string;
  read_cluster: string;
  target_kind: string;
  view_template: string;
  source_route_id: string;
  read_type: string;
  target_type?: string;
  format?: string;
  template_path: string;
  required_target_fields: string[];
  output_schema: string;
}

export interface ReadContextTemplateDomainsResult {
  schema: 'BlueprintHelper.ReadContextTemplateDomains.v1';
  workflow: 'read_context';
  items: ReadContextTemplateDomainItem[];
}

export interface ReadContextTemplateClustersResult {
  schema: 'BlueprintHelper.ReadContextTemplateClusters.v1';
  domain: string;
  items: ReadContextTemplateClusterItem[];
}

export interface ReadContextTemplateTargetsResult {
  schema: 'BlueprintHelper.ReadContextTemplateTargets.v1';
  domain: string;
  read_cluster: string;
  items: ReadContextTemplateTargetItem[];
}

export interface ReadContextTemplateViewsResult {
  schema: 'BlueprintHelper.ReadContextTemplateViews.v1';
  domain: string;
  read_cluster: string;
  target_kind: string;
  items: ReadContextTemplateViewItem[];
}

export interface ReadContextTemplateQuickAccessResult {
  schema: 'BlueprintHelper.ReadContextTemplateQuickAccess.v1';
  domain: string;
  read_cluster: string;
  target_kind: string;
  view_template: string;
  items: ReadContextTemplateQuickAccessItem[];
}

export interface ComposeReadContextTemplateInput {
  domain: ReadContextTemplateDomain | string;
  readCluster: string;
  targetKind: string;
  viewTemplate: ReadContextTemplateView | string;
  templateIds: string[];
  outputPath: string;
}

export type ReadContextTemplateCompositionResult =
  | {
      schema: 'BlueprintHelper.ReadContextTemplateComposition.v1';
      status: 'ok';
      domain: string;
      read_cluster: string;
      target_kind: string;
      view_template: string;
      template_id: string;
      output_path: string;
      next: {
        read_command: string;
      };
    }
  | {
      schema: 'BlueprintHelper.ReadContextTemplateComposition.v1';
      status: 'failed';
      domain: string;
      read_cluster: string;
      target_kind: string;
      view_template: string;
      diagnostics: ReadContextTemplateDiagnostic[];
    };
