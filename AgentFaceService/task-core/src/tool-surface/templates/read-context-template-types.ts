import type { ReadContextInput } from '../bridge/read-context/read-context-schemas.js';

export type ReadContextTemplateFamily =
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
  | 'logic_json_delta_after_logic_flow'
  | 'tree_json'
  | 'schema_json'
  | 'property_json'
  | 'diagnostics_json';

export type ReadContextRequestBuilderId =
  | 'blueprint_logic'
  | 'material_logic'
  | 'asset_context'
  | 'component_context'
  | 'variable_context'
  | 'widget_tree'
  | 'widget_property'
  | 'data_table'
  | 'data_asset'
  | 'object_property'
  | 'material_instance_context';

export type ReadContextPayloadProjectorId =
  | 'logic'
  | 'asset_context'
  | 'widget_tree'
  | 'component_tree'
  | 'variable_schema'
  | 'data_table_schema'
  | 'object_property'
  | 'material_instance';

export interface ReadContextTemplateDiagnostic {
  code: string;
  template_id?: string;
  family?: string;
  cluster?: string;
  path?: string;
  message?: string;
}

export interface ReadContextTemplateDescriptor {
  template_id: string;
  family: ReadContextTemplateFamily;
  cluster: string;
  description: string;
  template_path: string;
  read_spec: {
    schema: 'BlueprintHelper.ReadSpec.v1';
    read_type: ReadContextInput['read_type'] | string;
    target: Record<string, unknown>;
    view?: Record<string, unknown>;
  };
  required_fields: string[];
  optional_fields: string[];
  context_evidence: Record<string, string>;
  output_schema: string;
  recommended_invocation: 'bh context read --file <read-spec.json> --format json';
  allowed_tools: readonly [
    'bh tools read-templates compose',
    'bh context read',
  ];
  stop_conditions: string[];
}

export interface ReadContextRouteDescriptor extends ReadContextTemplateDescriptor {
  status: ReadContextTemplateRouteStatus;
  payload_schema: 'BlueprintHelper.ReadSpec.v1';
  read_type: ReadContextInput['read_type'] | string;
  bridge_command?: string;
  route_cluster: string;
  route_source_id: 'generated.read_context_manifest';
  route_policy_id: 'generated.route_manifest';
  route_agent_visible: false;
  route_requires_game_thread: true;
  request_builder_id: ReadContextRequestBuilderId;
  payload_projector_id: ReadContextPayloadProjectorId;
  supported_asset_types: readonly string[];
  supported_formats: readonly string[];
  target_type?: NonNullable<ReadContextInput['target']['target_type']> | string;
  format?: ReadContextTemplateView | string;
  reason?: string;
}

export interface ReadContextTemplateFamilyItem {
  family: string;
  description: string;
  status: 'supported';
}

export interface ReadContextTemplateClusterItem {
  family: string;
  cluster: string;
  description: string;
}

export interface ReadContextTemplateFamiliesResult {
  schema: 'BlueprintHelper.ReadContextTemplateFamilies.v1';
  workflow: 'read_context';
  guidance: string;
  items: ReadContextTemplateFamilyItem[];
}

export interface ReadContextTemplateClustersResult {
  schema: 'BlueprintHelper.ReadContextTemplateClusters.v1';
  family: string;
  guidance: string;
  items: ReadContextTemplateClusterItem[];
}

export interface ReadContextTemplatesResult {
  schema: 'BlueprintHelper.ReadContextTemplates.v1';
  family: string;
  cluster: string;
  items: ReadContextTemplateDescriptor[];
}

export interface ComposeReadContextTemplateInput {
  templateId: string;
  outputPath: string;
}

export type ReadContextTemplateCompositionResult =
  | {
      schema: 'BlueprintHelper.ReadContextTemplateComposition.v1';
      status: 'ok';
      template_id: string;
      family: string;
      cluster: string;
      output_path: string;
      next: {
        read_command: string;
      };
    }
  | {
      schema: 'BlueprintHelper.ReadContextTemplateComposition.v1';
      status: 'failed';
      template_id: string;
      diagnostics: ReadContextTemplateDiagnostic[];
    };
