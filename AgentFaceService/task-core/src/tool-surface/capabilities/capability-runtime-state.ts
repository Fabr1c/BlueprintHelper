import {
  RuntimeCapabilityStateSchema,
  type RuntimeCapabilityState,
} from './capability-descriptor.schema.js';
import { listCapabilityDescriptors } from './capability-descriptor-registry.js';

export function createRuntimeCapabilityState(
  input: Partial<RuntimeCapabilityState> = {},
): RuntimeCapabilityState {
  return RuntimeCapabilityStateSchema.parse({
    registered_runtime_adapter_ids: uniqueStrings(input.registered_runtime_adapter_ids ?? []),
    allow_write_capabilities: input.allow_write_capabilities ?? true,
    allow_high_risk_capabilities: input.allow_high_risk_capabilities ?? true,
  });
}

export function createDescriptorFixtureRuntimeCapabilityState(
  input: Partial<RuntimeCapabilityState> = {},
): RuntimeCapabilityState {
  const activeAdapterIds = listCapabilityDescriptors()
    .filter((descriptor) => descriptor.runtime.status === 'active')
    .map((descriptor) => descriptor.runtime.adapter_id);

  return createRuntimeCapabilityState({
    registered_runtime_adapter_ids: activeAdapterIds,
    allow_write_capabilities: true,
    allow_high_risk_capabilities: true,
    ...input,
  });
}

function uniqueStrings(values: readonly string[]): string[] {
  const seen = new Set<string>();
  const result: string[] = [];
  for (const value of values) {
    const normalized = value.trim();
    if (normalized.length === 0 || seen.has(normalized)) continue;
    seen.add(normalized);
    result.push(normalized);
  }
  return result;
}
