import type { MetricsOperationIdentity } from '../../metrics/metrics-types.js';

export interface ToolCapabilityDescriptor {
  readonly tool_id: string;
  readonly tool_name: string;
  readonly route_refs: readonly string[];
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
}

export function createToolCapabilityDescriptorRegistry(
  input: ToolCapabilityDescriptorRegistryInput,
): ToolCapabilityDescriptorRegistry {
  return new ToolCapabilityDescriptorRegistry(input.descriptors);
}
