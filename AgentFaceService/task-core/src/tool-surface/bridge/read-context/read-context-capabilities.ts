import { z } from 'zod';
import { successRead, type ToolResultBase } from '../../../result/tool-result.js';

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

const READ_CAPABILITIES = [
  {
    read_type: 'asset_context',
    asset_types: ['asset', 'blueprint', 'data_table', 'data_asset'],
    formats: [],
  },
  {
    read_type: 'blueprint_logic',
    asset_types: ['blueprint', 'graph', 'function', 'event', 'custom_event', 'block'],
    formats: ['logic_flow', 'logic_md', 'logic_json'],
  },
  {
    read_type: 'graph_context',
    asset_types: ['blueprint', 'graph', 'function', 'event', 'custom_event', 'block'],
    formats: ['logic_json'],
  },
  {
    read_type: 'component_context',
    asset_types: ['blueprint', 'component'],
    formats: [],
  },
  {
    read_type: 'variable_context',
    asset_types: ['blueprint', 'member_variable', 'event_dispatcher'],
    formats: [],
  },
  {
    read_type: 'widget_context',
    asset_types: ['blueprint', 'widget'],
    formats: [],
  },
  {
    read_type: 'data_table_context',
    asset_types: ['data_table', 'data_table_row', 'asset'],
    formats: [],
  },
  {
    read_type: 'data_asset_context',
    asset_types: ['data_asset', 'asset', 'object_property', 'property'],
    formats: [],
  },
  {
    read_type: 'object_property_context',
    asset_types: ['asset', 'data_asset', 'object_property', 'property'],
    formats: [],
  },
] as const;

export function buildReadContextCapabilitiesPayload(): Record<string, unknown> {
  return {
    schema: 'ReadContextCapabilities.v1',
    asset_types: [...ASSET_TYPES],
    formats: [...FORMATS],
    read_type_ids: READ_CAPABILITIES.map((capability) => capability.read_type),
    read_types: READ_CAPABILITIES.map((capability) => ({
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
  return successRead('read_context_capabilities', undefined, buildReadContextCapabilitiesPayload());
}

function difference<T extends string>(
  allValues: readonly T[],
  supportedValues: readonly string[],
): T[] {
  const supported = new Set(supportedValues);
  return allValues.filter((value) => !supported.has(value));
}
