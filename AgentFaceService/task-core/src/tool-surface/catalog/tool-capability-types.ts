import type { ToolAudience, ToolRisk } from '../types.js';

export type ToolCapabilityDomain =
  | 'blueprint'
  | 'animation'
  | 'material'
  | 'umg'
  | 'data'
  | 'editor'
  | 'project'
  | 'debug'
  | 'review';

export type ToolCapabilityKind =
  | 'discover'
  | 'read'
  | 'plan'
  | 'write'
  | 'diagnose';

export type ToolDomainStatus = 'active' | 'reserved';

export interface ToolDomainCatalogItem {
  id: ToolCapabilityDomain;
  label: string;
  status: ToolDomainStatus;
  default_kinds: ToolCapabilityKind[];
  purpose: string;
  available_by_default: boolean;
  reason?: string;
}

export interface ToolCapabilityItem {
  id: string;
  domain: ToolCapabilityDomain;
  kind: ToolCapabilityKind;
  tool_name: string;
  purpose: string;
  agent_role: 'blueprint-explorer' | 'sourcecode-explorer' | 'task-worker' | 'main-agent';
  audience: ToolAudience;
  risk: ToolRisk;
  requires_bridge: boolean;
  requires_write_session: boolean;
  lifecycle_mcp_only?: boolean;
  deprecated?: boolean;
  frozen?: boolean;
  cli_template_ids: string[];
}

export interface CliInvocationTemplateRef {
  cli_template_id: string;
  path: string;
  template_kind: 'cli_invocation';
  recommended_for?: string[];
  input_shape?: string;
}

export type ToolTemplateRouteKind =
  | 'read_context'
  | 'graph_write'
  | 'taskspec';

export type ToolTemplateSlotKind =
  | 'statement'
  | 'expression'
  | 'target'
  | 'view'
  | 'patch'
  | 'merge';

export interface ToolTemplateRouteRef {
  route_id: string;
  route_kind: ToolTemplateRouteKind;
  purpose: string;
  template_paths: string[];
  required_fields: string[];
  optional_fields: string[];
  insert_paths: string[];
  when_to_use?: string;
  when_not_to_use?: string;
}

export interface ToolTemplateSlotRef {
  slot_id: string;
  slot_type: ToolTemplateSlotKind;
  path: string;
  applies_to_routes: string[];
  insert_path: string;
  keywords: string[];
  when_to_use: string;
  when_not_to_use?: string;
}

export interface ToolDomainListResult {
  schema: 'BlueprintHelper.ToolDomainList.v1';
  audience: ToolAudience;
  items: ToolDomainCatalogItem[];
  reserved: ToolDomainCatalogItem[];
  next: {
    list_command: 'bh tools list <domain> <kind> --format json';
  };
}

export interface ToolCapabilityListResult {
  schema: 'BlueprintHelper.ToolCapabilityList.v1';
  query: {
    domain: ToolCapabilityDomain;
    kind: ToolCapabilityKind;
    audience: ToolAudience;
  };
  items: ToolCapabilityItem[];
  next: {
    templates_command: 'bh tools templates <tool_id> --format json';
  };
}

export interface ToolTemplateDispatchResult {
  schema: 'BlueprintHelper.ToolTemplateSelection.v1';
  tool_id: string;
  tool_name: string;
  cli_invocation_templates: CliInvocationTemplateRef[];
  routes: ToolTemplateRouteRef[];
  selected_route?: ToolTemplateRouteRef;
  slot_templates: ToolTemplateSlotRef[];
  input_shape?: string;
  recommended_invocation: string;
  allowed_tools: string[];
  stop_conditions: string[];
  next: {
    route_command?: string;
    slot_command?: string;
  };
}

export interface ListToolDomainsOptions {
  audience?: ToolAudience;
  includeReserved?: boolean;
}

export interface ListToolCapabilitiesOptions {
  domain: ToolCapabilityDomain;
  kind: ToolCapabilityKind;
  audience?: ToolAudience;
  expert?: boolean;
  requiresBridge?: boolean;
  risks?: ToolRisk[];
}

export interface GetToolTemplateDispatchOptions {
  route?: string;
  slot?: boolean;
  slotKind?: ToolTemplateSlotKind;
}
