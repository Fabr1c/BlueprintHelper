import { z } from 'zod';
import { successRead, type ToolResultBase } from '../../../result/tool-result.js';
import { getActiveReadContextRouteDescriptors } from '../../templates/read-context-template-registry.js';
import { getCachedReadContextCapabilityPayload } from './read-context-capability-cache.js';

export const ReadContextCapabilitiesInputSchema = z.object({}).strict();

export type ReadContextCapabilitiesInput = z.infer<typeof ReadContextCapabilitiesInputSchema>;

export function buildReadContextCapabilitiesPayload(): Record<string, unknown> {
  const activeRoutes = getActiveReadContextRouteDescriptors();
  const capabilities = buildCapabilitiesFromRegistry(activeRoutes);
  const assetTypes = uniqueSorted(activeRoutes.flatMap((route) => route.supported_asset_types));
  const formats = uniqueSorted(activeRoutes.flatMap((route) => route.supported_formats));

  return {
    schema: 'ReadContextCapabilities.v1',
    asset_types: assetTypes,
    formats,
    read_type_ids: capabilities.map((capability) => capability.read_type),
    read_types: capabilities.map((capability) => ({
      read_type: capability.read_type,
      unsupported_asset_types: difference(assetTypes, capability.asset_types),
      unsupported_formats: difference(formats, capability.formats),
    })),
  };
}

export function executeReadContextCapabilities(
  rawInput: Record<string, unknown>,
): ToolResultBase {
  ReadContextCapabilitiesInputSchema.parse(rawInput);
  return successRead(
    'read_context_capabilities',
    undefined,
    getCachedReadContextCapabilityPayload(buildReadContextCapabilitiesPayload),
  );
}

function buildCapabilitiesFromRegistry(routes = getActiveReadContextRouteDescriptors()): Array<{
  read_type: string;
  asset_types: string[];
  formats: string[];
}> {
  const byReadType = new Map<string, {
    assetTypes: Set<string>;
    formats: Set<string>;
  }>();

  for (const route of routes) {
    const entry = byReadType.get(route.read_type) ?? {
      assetTypes: new Set<string>(),
      formats: new Set<string>(),
    };
    route.supported_asset_types.forEach((assetType) => entry.assetTypes.add(assetType));
    route.supported_formats.forEach((format) => entry.formats.add(format));
    byReadType.set(route.read_type, entry);
  }

  return [...byReadType.entries()]
    .map(([readType, entry]) => ({
      read_type: readType,
      asset_types: [...entry.assetTypes].sort(),
      formats: [...entry.formats].sort(),
    }))
    .sort((left, right) => left.read_type.localeCompare(right.read_type));
}

function difference<T extends string>(
  allValues: readonly T[],
  supportedValues: readonly string[],
): T[] {
  const supported = new Set(supportedValues);
  return allValues.filter((value) => !supported.has(value));
}

function uniqueSorted(values: readonly string[]): string[] {
  return [...new Set(values)].sort((left, right) => left.localeCompare(right));
}
