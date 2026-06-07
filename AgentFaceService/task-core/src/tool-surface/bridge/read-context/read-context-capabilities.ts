import { z } from 'zod';
import { successRead, type ToolResultBase } from '../../../result/tool-result.js';
import { getActiveReadContextRouteDescriptors } from '../../templates/read-context-template-registry.js';
import { getCachedReadContextCapabilityPayload } from './read-context-capability-cache.js';

export const ReadContextCapabilitiesInputSchema = z.object({}).strict();

export type ReadContextCapabilitiesInput = z.infer<typeof ReadContextCapabilitiesInputSchema>;

const ASSET_TYPES = [
  'asset',
  'blueprint',
  'graph',
  'function',
  'event',
  'custom_event',
  'component',
  'member_variable',
  'event_dispatcher',
  'widget',
  'data_table',
  'data_table_row',
  'data_asset',
  'object_property',
  'property',
  'block',
] as const;

const FORMATS = [
  'logic_flow',
  'logic_md',
  'logic_json',
] as const;

export function buildReadContextCapabilitiesPayload(): Record<string, unknown> {
  const capabilities = buildCapabilitiesFromRegistry();
  return {
    schema: 'ReadContextCapabilities.v1',
    asset_types: [...ASSET_TYPES],
    formats: [...FORMATS],
    read_type_ids: capabilities.map((capability) => capability.read_type),
    read_types: capabilities.map((capability) => ({
      read_type: capability.read_type,
      unsupported_asset_types: difference(ASSET_TYPES, capability.asset_types),
      unsupported_formats: difference(FORMATS, capability.formats),
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

function buildCapabilitiesFromRegistry(): Array<{
  read_type: string;
  asset_types: string[];
  formats: string[];
}> {
  const byReadType = new Map<string, {
    assetTypes: Set<string>;
    formats: Set<string>;
  }>();

  for (const route of getActiveReadContextRouteDescriptors()) {
    const entry = byReadType.get(route.read_type) ?? {
      assetTypes: new Set<string>(),
      formats: new Set<string>(),
    };
    if (route.target_type) {
      entry.assetTypes.add(route.target_type);
    }
    if (route.format && FORMATS.includes(route.format as typeof FORMATS[number])) {
      entry.formats.add(route.format);
    }
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
