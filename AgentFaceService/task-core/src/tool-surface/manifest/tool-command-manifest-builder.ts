import {
  getToolCapabilityDescriptor,
  isToolCapabilityDomain,
  isToolCapabilityKind,
  listToolCapabilityItems,
  listToolDomains,
} from '../catalog/tool-capability-catalog.js';
import { createDescriptorFixtureRuntimeCapabilityState } from '../capabilities/capability-runtime-state.js';
import type {
  ToolCapabilityItem,
} from '../catalog/tool-capability-types.js';
import type {
  ToolCommandManifest,
} from './tool-command-manifest.js';
import { TOOL_COMMAND_MANIFEST_SCHEMA } from './tool-command-manifest.js';
import { inferInputShapesFromTemplateIds } from './tool-input-shape-metadata.js';
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
  ['task execute', 'blueprint.write.taskspec.execute'],
  ['task result', 'project.read.task_result'],
  ['context read', 'blueprint.read.context.logic_flow'],
]);

const MANIFEST_RUNTIME_FIXTURE = createDescriptorFixtureRuntimeCapabilityState();

export function buildReadonlyToolCommandManifests(): ToolCommandManifest[] {
  const manifests = listToolDomains().items.flatMap((domain) =>
    domain.default_kinds.flatMap((kind) =>
      listManifestCapabilitiesForDomainKind(domain.id, kind).map(buildManifestForCapability)));
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
  const descriptor = getToolCapabilityDescriptor(capability.id);
  if (!descriptor) {
    throw new Error(`Missing BlueprintHelper capability descriptor for ${capability.id}.`);
  }
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
    input_shapes: inferInputShapesFromTemplateIds({
      templateIds: capability.cli_template_ids,
      requiresBridge: capability.requires_bridge,
      emptyTemplateInputShape: emptyTemplateInputShapeForCapability(capability),
    }),
    handler_id: capability.tool_name,
    result_policy_id: descriptor.result_policy_id,
    metrics_identity: descriptor.metrics_identity,
    template_refs: [...capability.cli_template_ids],
    route_refs: [...descriptor.route_refs],
    recommended_invocations: [...descriptor.recommended_invocations],
    help_usage: [...descriptor.help_usage],
    help_notes: [...descriptor.help_notes],
    stop_conditions: [...descriptor.stop_conditions],
    source: 'readonly_mirror',
  };
}

function emptyTemplateInputShapeForCapability(capability: ToolCapabilityItem): 'cli_options' | undefined {
  return capability.id === 'project.read.task_result' ? 'cli_options' : undefined;
}

function listManifestCapabilitiesForDomainKind(
  domain: ToolCapabilityItem['domain'],
  kind: ToolCapabilityItem['kind'],
): ToolCapabilityItem[] {
  const capabilitiesById = new Map<string, ToolCapabilityItem>();
  for (const capability of [
    ...listToolCapabilityItems({ domain, kind, runtime: MANIFEST_RUNTIME_FIXTURE }),
    ...listToolCapabilityItems({ domain, kind, audience: 'expert', expert: true, runtime: MANIFEST_RUNTIME_FIXTURE }),
  ]) {
    capabilitiesById.set(capability.id, capability);
  }
  return [...capabilitiesById.values()];
}

function listGroupedAliasCapabilities(): ToolCapabilityItem[] {
  return [...GROUPED_COMMAND_ALIASES.keys()].map((toolId) => {
    const [domain, kind] = toolId.split('.');
    if (!domain || !kind || !isToolCapabilityDomain(domain) || !isToolCapabilityKind(kind)) {
      throw new Error(`Grouped command alias uses an invalid BlueprintHelper tool id: ${toolId}`);
    }
    const capability = listToolCapabilityItems({ domain, kind, runtime: MANIFEST_RUNTIME_FIXTURE }).find((item) => item.id === toolId);
    if (!capability) {
      throw new Error(`Grouped command alias is missing from the tool catalog: ${toolId}`);
    }
    return capability;
  });
}
