export type GraphWriteRouteStatus = 'active' | 'planned' | 'hidden';

export interface GraphWriteSelectorDescriptor {
  expected_kind: string;
  required_fields: string[];
  output_fields: Record<string, string>;
  graph_name_output_field?: string;
  passthrough_fields?: string[];
}

export interface GraphWriteRouteDescriptor {
  route_id: string;
  task_type: 'edit_blueprint_graph';
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
  compiler_id: string;
  status: GraphWriteRouteStatus;
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
  adapter_sync: 'generated_active_stub' | 'planned' | 'hidden';
}

export interface GraphWriteRouteSyncManifest {
  schema: 'BlueprintHelper.GraphWriteRouteAdapterSync.v1';
  generated_from: string;
  routes: GraphWriteRouteSyncEntry[];
}

export function isAgentVisibleGraphWriteRoute(route: GraphWriteRouteDescriptor): boolean {
  return route.status === 'active' && route.template_path !== undefined;
}

export function makeGraphWriteRouteKey(graphStrategy: string, publicScope: string): string {
  return `${graphStrategy}:${publicScope}`;
}
