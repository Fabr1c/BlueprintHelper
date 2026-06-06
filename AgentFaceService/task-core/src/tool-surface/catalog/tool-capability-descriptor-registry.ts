import type { MetricsOperationIdentity } from '../../metrics/metrics-types.js';
import type {
  CliInvocationTemplateRef,
  GetToolTemplateDispatchOptions,
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
  readonly descriptors: readonly ToolCapabilityDescriptor[];
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
  return new ToolCapabilityDescriptorRegistry(input.descriptors);
}

export function buildDescriptorRecommendedInvocation(
  descriptor: ToolCapabilityDescriptor,
  templates: readonly CliInvocationTemplateRef[],
): string {
  const templatePath = templates[0]?.path ?? '<filled-template.json>';
  return descriptor.recommended_invocations[0]
    ?? `bh ${descriptor.tool_name} --file ${templatePath} --select status,artifacts.full_result`;
}
