export interface ReadContextPayloadSizeMetric {
  name: string;
  duration_ms: number;
  bytes: number;
}

export function estimateJsonPayloadBytes(value: unknown): number {
  if (value === undefined) {
    return 0;
  }
  return Buffer.byteLength(JSON.stringify(value), 'utf8');
}

export function buildPayloadSizeMetric(name: string, value: unknown): ReadContextPayloadSizeMetric {
  return {
    name,
    duration_ms: 0,
    bytes: estimateJsonPayloadBytes(value),
  };
}
