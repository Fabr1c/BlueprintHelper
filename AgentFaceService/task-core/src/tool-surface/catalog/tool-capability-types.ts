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
    template_index_command:
      | 'bh tools templates families --workflow preview_execute --format json'
      | 'bh tools read-templates domains --format json';
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
