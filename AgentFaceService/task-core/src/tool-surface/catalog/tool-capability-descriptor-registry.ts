import type { MetricsOperationIdentity } from '../../metrics/metrics-types.js';
import type {
  CliInvocationTemplateRef,
  GetToolTemplateDispatchOptions,
  ToolCapabilityItem,
  ToolTemplateRouteRef,
  ToolTemplateSlotRef,
} from './tool-capability-types.js';

export interface ToolCapabilityDescriptor {
  readonly tool_id: string;
  readonly tool_name: string;
  readonly route_refs: readonly ToolTemplateRouteRef[];
  readonly slot_refs: readonly ToolTemplateSlotRef[];
  readonly stop_conditions: readonly string[];
  readonly recommended_invocations: readonly string[];
  readonly metrics_identity?: MetricsOperationIdentity;
}

export interface ToolCapabilityDescriptorRegistryInput {
  readonly capabilities: readonly ToolCapabilityItem[];
  readonly blueprintTaskSpecRoutes: readonly ToolTemplateRouteRef[];
  readonly umgTaskSpecRoutes: readonly ToolTemplateRouteRef[];
  readonly dataTaskSpecRoutes: readonly ToolTemplateRouteRef[];
  readonly readContextRoutes: readonly ToolTemplateRouteRef[];
  readonly graphWriteSlotTemplateRefs: readonly ToolTemplateSlotRef[];
  readonly readContextSlots: readonly ToolTemplateSlotRef[];
}

export class ToolCapabilityDescriptorRegistry {
  private readonly descriptorsByToolId: Map<string, ToolCapabilityDescriptor>;

  constructor(descriptors: readonly ToolCapabilityDescriptor[]) {
    this.descriptorsByToolId = new Map(descriptors.map((descriptor) => [descriptor.tool_id, descriptor]));
  }

  get(toolId: string): ToolCapabilityDescriptor | undefined {
    return this.descriptorsByToolId.get(toolId);
  }

  require(toolId: string): ToolCapabilityDescriptor {
    const descriptor = this.get(toolId);
    if (!descriptor) {
      throw new Error(`Unknown BlueprintHelper tool capability descriptor: ${toolId}`);
    }
    return descriptor;
  }

  filterSlotTemplates(
    toolId: string,
    options: GetToolTemplateDispatchOptions,
    selectedRoute: ToolTemplateRouteRef | undefined,
  ): ToolTemplateSlotRef[] {
    const descriptor = this.require(toolId);
    const routeIds = selectedRoute
      ? new Set([selectedRoute.route_id])
      : new Set(descriptor.route_refs.map((routeEntry) => routeEntry.route_id));
    return descriptor.slot_refs.filter((slotTemplate) => {
      if (options.slotKind && slotTemplate.slot_type !== options.slotKind) {
        return false;
      }
      return slotTemplate.applies_to_routes.some((routeId) => routeIds.has(routeId));
    });
  }
}

export function createToolCapabilityDescriptorRegistry(
  input: ToolCapabilityDescriptorRegistryInput,
): ToolCapabilityDescriptorRegistry {
  return new ToolCapabilityDescriptorRegistry(input.capabilities.map((capability) => ({
    tool_id: capability.id,
    tool_name: capability.tool_name,
    route_refs: resolveRouteRefs(capability.id, input),
    slot_refs: resolveSlotRefs(capability.id, input),
    stop_conditions: resolveStopConditions(capability),
    recommended_invocations: [],
    metrics_identity: {
      capability: `${capability.domain}.${capability.kind}`,
      semantic_operation: capability.id,
    },
  })));
}

export function buildDescriptorRecommendedInvocation(
  descriptor: ToolCapabilityDescriptor,
  templates: readonly CliInvocationTemplateRef[],
): string {
  const templatePath = templates[0]?.path ?? '<filled-template.json>';
  if (descriptor.tool_name === 'blueprinthelper_preview_task') {
    return 'bh task preview --file <filled_taskspec.json> --format summary';
  }
  if (descriptor.tool_name === 'blueprinthelper_execute_task') {
    return 'bh task execute --file <filled_taskspec.json> --preview-token <preview_token> --format summary';
  }
  return `bh ${descriptor.tool_name} --file ${templatePath} --select status,artifacts.full_result`;
}

function resolveRouteRefs(
  toolId: string,
  input: ToolCapabilityDescriptorRegistryInput,
): readonly ToolTemplateRouteRef[] {
  if (toolId === 'blueprint.plan.taskspec.preview' || toolId === 'blueprint.write.taskspec.execute') {
    return input.blueprintTaskSpecRoutes;
  }
  if (toolId === 'umg.plan.taskspec.preview' || toolId === 'umg.write.taskspec.execute') {
    return input.umgTaskSpecRoutes;
  }
  if (toolId === 'data.plan.taskspec.preview' || toolId === 'data.write.taskspec.execute') {
    return input.dataTaskSpecRoutes;
  }
  if (toolId === 'blueprint.read.context.logic_flow') {
    return input.readContextRoutes.filter((entry) => entry.route_id.includes('logic_flow'));
  }
  if (toolId === 'blueprint.read.context.logic_json') {
    return input.readContextRoutes.filter((entry) => entry.route_id.includes('logic_json'));
  }
  if (toolId === 'blueprint.read.context.variables') {
    return input.readContextRoutes.filter((entry) => entry.route_id === 'read.blueprint.variables');
  }
  if (toolId === 'blueprint.read.context.components') {
    return input.readContextRoutes.filter((entry) => entry.route_id === 'read.blueprint.components');
  }
  if (toolId === 'umg.read.widget_tree') {
    return input.readContextRoutes.filter((entry) => entry.route_id === 'read.widget.tree');
  }
  if (toolId === 'umg.read.widget_property') {
    return input.readContextRoutes.filter((entry) => entry.route_id === 'read.widget.property');
  }
  if (toolId === 'data.read.data_asset') {
    return input.readContextRoutes.filter((entry) => entry.route_id === 'read.data_asset.object');
  }
  if (toolId === 'data.read.data_table') {
    return input.readContextRoutes.filter((entry) => entry.route_id.startsWith('read.data_table.'));
  }
  return [];
}

function resolveSlotRefs(
  toolId: string,
  input: ToolCapabilityDescriptorRegistryInput,
): readonly ToolTemplateSlotRef[] {
  if (toolId === 'blueprint.plan.taskspec.preview' || toolId === 'blueprint.write.taskspec.execute') {
    return input.graphWriteSlotTemplateRefs;
  }
  if (
    toolId === 'blueprint.read.context.logic_flow'
    || toolId === 'blueprint.read.context.logic_json'
    || toolId === 'blueprint.read.context.variables'
    || toolId === 'blueprint.read.context.components'
    || toolId === 'umg.read.widget_tree'
    || toolId === 'umg.read.widget_property'
    || toolId === 'data.read.data_asset'
    || toolId === 'data.read.data_table'
  ) {
    return input.readContextSlots;
  }
  return [];
}

function resolveStopConditions(capabilityItem: ToolCapabilityItem): string[] {
  const specific = stopConditionsByToolName(capabilityItem.tool_name);
  if (specific) {
    return [...specific];
  }
  return capabilityItem.requires_bridge
    ? ['tool_unavailable', 'bridge_unavailable']
    : ['tool_unavailable'];
}

function stopConditionsByToolName(toolName: string): readonly string[] | undefined {
  switch (toolName) {
    case 'blueprinthelper_find_assets':
      return ['tool_unavailable', 'bridge_unavailable', 'target asset not found'];
    case 'blueprinthelper_read_context':
      return ['tool_unavailable', 'bridge_unavailable', 'target not found', 'target asset not found', 'target graph not found'];
    case 'blueprinthelper_read_reference_context':
      return ['tool_unavailable', 'bridge_unavailable', 'reference target not found'];
    case 'blueprinthelper_read_function_chain_context':
      return ['tool_unavailable', 'bridge_unavailable', 'entry function not found'];
    case 'blueprinthelper_preview_task':
      return ['tool_unavailable', 'write_session_required', 'taskspec_template_unavailable', 'preview_blocked'];
    case 'blueprinthelper_execute_task':
      return ['tool_unavailable', 'write_session_required', 'preview_required', 'execute_failed'];
    case 'blueprinthelper_request_write_session':
      return ['tool_unavailable', 'preview_required', 'write_permission_denied'];
    case 'blueprinthelper_diagnostics':
      return ['tool_unavailable', 'diagnostics_failed'];
    case 'blueprinthelper_diagnostics_runtime':
      return ['tool_unavailable', 'bridge_unavailable', 'diagnostics_failed'];
    case 'blueprint_compile_blueprint':
      return ['tool_unavailable', 'bridge_unavailable', 'write_session_required', 'compile_failed', 'target_asset_not_found', 'tool_failed'];
    case 'blueprint_save_asset':
      return ['tool_unavailable', 'bridge_unavailable', 'write_session_required', 'source_control_unavailable', 'checked_out_by_other', 'source_control_conflicted', 'checkout_required', 'not_editable', 'save_failed', 'target_asset_not_found', 'tool_failed'];
    case 'blueprinthelper_source_control_status':
      return ['tool_unavailable', 'bridge_unavailable', 'source_control_unavailable', 'checked_out_by_other', 'source_control_conflicted', 'not_editable'];
    case 'blueprinthelper_source_control_checkout':
      return ['tool_unavailable', 'bridge_unavailable', 'source_control_unavailable', 'checked_out_by_other', 'source_control_conflicted', 'checkout_failed', 'not_editable'];
    default:
      return undefined;
  }
}
