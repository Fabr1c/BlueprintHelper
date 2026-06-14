export type GraphWriteRouteStatus = 'active' | 'planned' | 'hidden';
export type GraphWriteRouteAdapterSync =
  | 'active_requires_registered_non_reserved_adapter'
  | 'reserved_hidden_from_agent';
export type GraphWriteTemplateWriteMode =
  | 'graph.append'
  | 'graph.replace'
  | 'graph.merge'
  | 'graph.patch';
export type GraphWriteRouteValidationClassification =
  | 'preview_decidable'
  | 'runtime_only'
  | 'shared_policy';

export interface GraphWriteSelectorDescriptor {
  expected_kind: string;
  required_fields: string[];
  output_fields: Record<string, string>;
  graph_name_output_field?: string;
  passthrough_fields?: string[];
}

export interface GraphWriteRouteQuickAccessDescriptor {
  template_id: string;
  family: 'graph_write';
  cluster_id: string;
  operation_id: string;
  quick_access_id: string;
  arg_slots?: string[];
}

export interface GraphWriteRouteDescriptor {
  route_id: string;
  task_type: 'edit_blueprint_graph';
  write_mode: GraphWriteTemplateWriteMode;
  graph_strategy: string;
  public_scope: string;
  behavior_field: string;
  taskplan_op: string;
  runtime_adapter_id: string;
  selector?: GraphWriteSelectorDescriptor;
  template_path?: string;
  required_fields: string[];
  optional_fields: string[];
  insert_paths: string[];
  purpose: string;
  when_to_use?: string;
  when_not_to_use?: string;
  allowed_slot_ids: string[];
  quick_access?: GraphWriteRouteQuickAccessDescriptor;
  compiler_id: string;
  status: GraphWriteRouteStatus;
  adapter_sync: GraphWriteRouteAdapterSync;
  validation_classification?: GraphWriteRouteValidationClassification;
  runtime_only_validation_notes?: string[];
}

export interface GraphWriteRouteSyncEntry {
  route_id: string;
  runtime_adapter_id: string;
  graph_strategy: string;
  public_scope: string;
  behavior_field: string;
  compiler_id: string;
  taskplan_op: string;
  status: GraphWriteRouteStatus;
  adapter_sync: GraphWriteRouteAdapterSync;
}

export interface GraphWriteRouteSyncManifest {
  schema: 'BlueprintHelper.GraphWriteRouteAdapterSync.v1';
  generated_from: string;
  routes: GraphWriteRouteSyncEntry[];
}

export function isAgentVisibleGraphWriteRoute(route: GraphWriteRouteDescriptor): boolean {
  return route.status === 'active'
    && route.adapter_sync === 'active_requires_registered_non_reserved_adapter'
    && route.template_path !== undefined;
}

export function makeGraphWriteRouteKey(graphStrategy: string, publicScope: string): string {
  return `${graphStrategy}:${publicScope}`;
}
