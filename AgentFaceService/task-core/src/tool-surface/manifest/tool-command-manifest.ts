import type { ToolAudience, ToolRisk } from '../types.js';
import type {
  ToolCapabilityDomain,
  ToolCapabilityItem,
  ToolCapabilityKind,
} from '../catalog/tool-capability-types.js';
import type { MetricsOperationIdentity } from '../../metrics/metrics-types.js';

export const TOOL_COMMAND_MANIFEST_SCHEMA = 'BlueprintHelper.ToolCommandManifest.v1' as const;

export type ToolInputShapeId =
  | 'empty_object'
  | 'bare_taskspec'
  | 'wrapped_taskspec_preview'
  | 'wrapped_taskspec_execute'
  | 'readspec'
  | 'read_reference_context'
  | 'bridge_payload'
  | 'bridge_logic_json_payload'
  | 'cli_options'
  | 'tool_payload';

export type ToolResultProjectionPolicyId =
  | 'local_default'
  | 'bridge_default'
  | 'task_preview_default'
  | 'task_execute_default'
  | 'task_result_default'
  | 'read_context_default'
  | 'diagnostics_default'
  | 'review_expert_default';

export interface ToolCommandManifest {
  schema: typeof TOOL_COMMAND_MANIFEST_SCHEMA;
  tool_id: string;
  tool_name: string;
  aliases: string[];
  domain: ToolCapabilityDomain;
  kind: ToolCapabilityKind;
  risk: ToolRisk;
  audience: ToolAudience;
  agent_role: ToolCapabilityItem['agent_role'];
  requires_bridge: boolean;
  requires_write_session: boolean;
  input_shapes: ToolInputShapeId[];
  handler_id: string;
  result_policy_id: ToolResultProjectionPolicyId;
  metrics_identity?: MetricsOperationIdentity;
  template_refs: string[];
  route_refs: string[];
  recommended_invocations: string[];
  help_usage: string[];
  help_notes: string[];
  stop_conditions: string[];
  source: 'readonly_mirror';
}
