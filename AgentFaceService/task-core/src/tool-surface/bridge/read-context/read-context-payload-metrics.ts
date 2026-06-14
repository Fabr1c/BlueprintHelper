export interface ReadContextPayloadSizeMetric {
  name: string;
  duration_ms: number;
  bytes: number;
}

export function estimateJsonPayloadBytes(value: unknown): number {
  if (value === undefined) {
    return 0;
  }
  try {
    const serialized = JSON.stringify(value);
    return serialized === undefined
      ? 0
      : Buffer.byteLength(serialized, 'utf8');
  } catch {
    return 0;
  }
}

export function buildPayloadSizeMetric(name: string, value: unknown): ReadContextPayloadSizeMetric {
  return {
    name,
    duration_ms: 0,
    bytes: estimateJsonPayloadBytes(value),
  };
}
