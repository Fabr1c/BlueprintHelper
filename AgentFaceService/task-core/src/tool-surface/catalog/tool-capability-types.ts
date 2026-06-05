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
  taskspec_template_ids: string[];
}

export interface CliInvocationTemplateRef {
  cli_template_id: string;
  path: string;
  template_kind: 'cli_invocation';
  recommended_for?: string[];
  input_shape?: string;
}

export interface TaskSpecSemanticTemplateRef {
  taskspec_template_id: string;
  path: string;
  template_kind: 'taskspec_semantic';
  recommended_for?: string[];
  task_type?: string;
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
  taskspec_semantic_templates: TaskSpecSemanticTemplateRef[];
  input_shape?: string;
  recommended_invocation: string;
  allowed_tools: string[];
  stop_conditions: string[];
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
