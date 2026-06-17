import {
  CapabilityDescriptorSchema,
  RuntimeCapabilityStateSchema,
  type CapabilityDescriptor,
  type RuntimeCapabilityState,
} from './capability-descriptor.schema.js';
import { CAPABILITY_DESCRIPTORS } from './capability-descriptors.js';

export interface CapabilityDescriptorRegistry {
  readonly descriptors: readonly CapabilityDescriptor[];
  readonly byId: ReadonlyMap<string, CapabilityDescriptor>;
}

export function createCapabilityDescriptorRegistry(
  descriptors: readonly CapabilityDescriptor[],
): CapabilityDescriptorRegistry {
  const byId = new Map<string, CapabilityDescriptor>();
  const parsedDescriptors = descriptors.map((descriptor) => CapabilityDescriptorSchema.parse(descriptor));
  for (const descriptor of parsedDescriptors) {
    if (byId.has(descriptor.id)) {
      throw new Error(`Duplicate BlueprintHelper capability descriptor id: ${descriptor.id}`);
    }
    byId.set(descriptor.id, descriptor);
  }
  return { descriptors: parsedDescriptors, byId };
}

export function listCapabilityDescriptors(): CapabilityDescriptor[] {
  return [...createDefaultRegistry().descriptors];
}

export function getCapabilityDescriptor(id: string): CapabilityDescriptor | undefined {
  return createDefaultRegistry().byId.get(id);
}

export function listAgentVisibleCapabilities(
  runtime: RuntimeCapabilityState,
): CapabilityDescriptor[] {
  const parsedRuntime = RuntimeCapabilityStateSchema.parse(runtime);
  const registeredAdapters = new Set(parsedRuntime.registered_runtime_adapter_ids);
  return createDefaultRegistry().descriptors.filter((descriptor) => {
    if (descriptor.safety.reserved_only) return false;
    if (descriptor.runtime.status !== 'active') return false;
    if (!registeredAdapters.has(descriptor.runtime.adapter_id)) return false;
    if (descriptor.safety.write_approval_required && !parsedRuntime.allow_write_capabilities) return false;
    if (descriptor.safety.risk === 'high' && !parsedRuntime.allow_high_risk_capabilities) return false;
    return true;
  });
}

function createDefaultRegistry(): CapabilityDescriptorRegistry {
  return createCapabilityDescriptorRegistry(CAPABILITY_DESCRIPTORS);
}

