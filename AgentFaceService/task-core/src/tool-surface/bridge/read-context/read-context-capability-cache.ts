const READ_CONTEXT_CAPABILITY_SCHEMA_VERSION = 'ReadContextCapabilities.v1';
const READ_CONTEXT_CAPABILITY_CACHE_KEY = `${READ_CONTEXT_CAPABILITY_SCHEMA_VERSION}:task-core`;

export interface ReadContextCapabilityCacheEntry {
  key: string;
  payload: Record<string, unknown>;
}

let cachedEntry: ReadContextCapabilityCacheEntry | undefined;

export function getCachedReadContextCapabilityPayload(
  builder: () => Record<string, unknown>,
): Record<string, unknown> {
  if (!cachedEntry || cachedEntry.key !== READ_CONTEXT_CAPABILITY_CACHE_KEY) {
    cachedEntry = {
      key: READ_CONTEXT_CAPABILITY_CACHE_KEY,
      payload: builder(),
    };
  }
  return cloneRecord(cachedEntry.payload);
}

export function clearReadContextCapabilityCacheForTests(): void {
  cachedEntry = undefined;
}

function cloneRecord(value: Record<string, unknown>): Record<string, unknown> {
  return JSON.parse(JSON.stringify(value)) as Record<string, unknown>;
}
