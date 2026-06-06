import {
  getRawToolTemplateDispatch,
  isToolCapabilityDomain,
  isToolCapabilityKind,
  listToolCapabilities,
  listToolDomains,
} from '../catalog/tool-capability-catalog.js';
import type {
  CliInvocationTemplateRef,
  ToolCapabilityItem,
} from '../catalog/tool-capability-types.js';
import type {
  ToolCommandManifest,
  ToolInputShapeId,
  ToolResultProjectionPolicyId,
} from './tool-command-manifest.js';
import { TOOL_COMMAND_MANIFEST_SCHEMA } from './tool-command-manifest.js';
import {
  createToolCommandManifestRegistry,
  type ToolCommandManifestRegistry,
} from './tool-command-manifest-registry.js';

const GROUPED_COMMAND_ALIASES = new Map<string, string[]>([
  ['blueprint.plan.taskspec.preview', ['task preview']],
  ['blueprint.write.taskspec.execute', ['task execute']],
  ['project.read.task_result', ['task result']],
  ['blueprint.read.context.logic_flow', ['context read']],
  ['blueprint.read.context.logic_json', ['context read']],
]);

const CANONICAL_LOOKUP_ALIASES = new Map<string, string>([
  ['task preview', 'blueprint.plan.taskspec.preview'],
  ['blueprinthelper_preview_task', 'blueprint.plan.taskspec.preview'],
  ['task execute', 'blueprint.write.taskspec.execute'],
  ['blueprinthelper_execute_task', 'blueprint.write.taskspec.execute'],
  ['task result', 'project.read.task_result'],
  ['context read', 'blueprint.read.context.logic_flow'],
  ['blueprinthelper_read_context', 'blueprint.read.context.logic_flow'],
]);

export function buildReadonlyToolCommandManifests(): ToolCommandManifest[] {
  const manifests = listToolDomains().items.flatMap((domain) =>
    domain.default_kinds.flatMap((kind) =>
      listToolCapabilities({ domain: domain.id, kind }).items.map(buildManifestForCapability)));
  const coveredIds = new Set(manifests.map((manifest) => manifest.tool_id));

  for (const capability of listGroupedAliasCapabilities()) {
    if (!coveredIds.has(capability.id)) {
      manifests.push(buildManifestForCapability(capability));
      coveredIds.add(capability.id);
    }
  }

  return manifests;
}

export function buildReadonlyToolCommandManifestRegistry(): ToolCommandManifestRegistry {
  return createToolCommandManifestRegistry(buildReadonlyToolCommandManifests(), {
    canonicalAliases: CANONICAL_LOOKUP_ALIASES,
  });
}

function buildManifestForCapability(capability: ToolCapabilityItem): ToolCommandManifest {
  const dispatch = getRawToolTemplateDispatch(capability.id);
  return {
    schema: TOOL_COMMAND_MANIFEST_SCHEMA,
    tool_id: capability.id,
    tool_name: capability.tool_name,
    aliases: GROUPED_COMMAND_ALIASES.get(capability.id) ?? [],
    domain: capability.domain,
    kind: capability.kind,
    risk: capability.risk,
    audience: capability.audience,
    agent_role: capability.agent_role,
    requires_bridge: capability.requires_bridge,
    requires_write_session: capability.requires_write_session,
    ...(capability.lifecycle_mcp_only === undefined ? {} : { lifecycle_mcp_only: capability.lifecycle_mcp_only }),
    input_shapes: inferInputShapes(capability, dispatch.cli_invocation_templates),
    handler_id: capability.tool_name,
    result_policy_id: inferResultPolicyId(capability),
    metrics_identity: inferMetricsIdentity(capability),
    template_refs: dispatch.cli_invocation_templates.map((template) => template.cli_template_id),
    route_refs: dispatch.routes.map((route) => route.route_id),
    recommended_invocations: [dispatch.recommended_invocation],
    stop_conditions: [...dispatch.stop_conditions],
    source: 'readonly_mirror',
  };
}

function inferInputShapes(
  capability: ToolCapabilityItem,
  templates: CliInvocationTemplateRef[],
): ToolInputShapeId[] {
  const shapes = new Set<ToolInputShapeId>();

  for (const template of templates) {
    if (template.cli_template_id === 'blueprinthelper_preview_task_wrapper') {
      shapes.add('wrapped_taskspec_preview');
      continue;
    }
    if (template.cli_template_id === 'blueprinthelper_execute_task_wrapper') {
      shapes.add('wrapped_taskspec_execute');
      continue;
    }
    if (
      template.cli_template_id === 'task_preview_bare_taskspec'
      || template.cli_template_id === 'task_execute_bare_taskspec'
    ) {
      shapes.add('bare_taskspec');
      continue;
    }
    const inputShape = template.input_shape ?? '';
    if (inputShape === '{}') {
      shapes.add('empty_object');
      continue;
    }
    if (inputShape === 'BlueprintHelper.ReadSpec.v1') {
      shapes.add('readspec');
      continue;
    }
    if (inputShape === 'BlueprintHelper.ReferenceContextRequest.v1') {
      shapes.add('read_reference_context');
      continue;
    }
    shapes.add(capability.requires_bridge ? 'bridge_payload' : 'tool_payload');
  }

  if (shapes.size === 0) {
    shapes.add(capability.requires_bridge ? 'bridge_payload' : 'tool_payload');
  }
  return [...shapes];
}

function inferResultPolicyId(capability: ToolCapabilityItem): ToolResultProjectionPolicyId {
  if (capability.tool_name === 'blueprinthelper_preview_task') {
    return 'task_preview_default';
  }
  if (capability.tool_name === 'blueprinthelper_execute_task') {
    return 'task_execute_default';
  }
  if (capability.tool_name === 'blueprinthelper_get_task_result') {
    return 'task_result_default';
  }
  if (capability.tool_name === 'blueprinthelper_read_context') {
    return 'read_context_default';
  }
  if (
    capability.tool_name === 'blueprinthelper_diagnostics'
    || capability.tool_name === 'blueprinthelper_diagnostics_runtime'
    || capability.tool_name.startsWith('blueprinthelper_get_debug_')
    || capability.tool_name === 'blueprinthelper_list_debug_cases'
    || capability.tool_name === 'blueprinthelper_export_debug_bundle'
  ) {
    return 'diagnostics_default';
  }
  if (capability.tool_name === 'blueprinthelper_apply_review_action') {
    return 'review_expert_default';
  }
  return capability.requires_bridge ? 'bridge_default' : 'local_default';
}

function inferMetricsIdentity(capability: ToolCapabilityItem): { capability: string; semantic_operation: string } {
  return {
    capability: `${capability.domain}.${capability.kind}`,
    semantic_operation: capability.id,
  };
}

function listGroupedAliasCapabilities(): ToolCapabilityItem[] {
  return [...GROUPED_COMMAND_ALIASES.keys()].map((toolId) => {
    const [domain, kind] = toolId.split('.');
    if (!domain || !kind || !isToolCapabilityDomain(domain) || !isToolCapabilityKind(kind)) {
      throw new Error(`Grouped command alias uses an invalid BlueprintHelper tool id: ${toolId}`);
    }
    const capability = listToolCapabilities({ domain, kind }).items.find((item) => item.id === toolId);
    if (!capability) {
      throw new Error(`Grouped command alias is missing from the tool catalog: ${toolId}`);
    }
    return capability;
  });
}
