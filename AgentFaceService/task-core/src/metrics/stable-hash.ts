import { createHash } from 'node:crypto';

export function stableStringify(value: unknown): string {
  return JSON.stringify(normalizeForStableJson(value)) ?? 'null';
}

export function hashStableJson(value: unknown): string {
  const digest = createHash('sha256').update(stableStringify(value)).digest('hex');
  return `sha256:${digest}`;
}

function normalizeForStableJson(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((entry) => normalizeForStableJson(entry));
  }

  if (value !== null && typeof value === 'object') {
    return Object.fromEntries(
      Object.entries(value as Record<string, unknown>)
        .filter(([, entry]) => entry !== undefined)
        .sort(([left], [right]) => compareStableKeys(left, right))
        .map(([key, entry]) => [key, normalizeForStableJson(entry)]),
    );
  }

  return value;
}

function compareStableKeys(left: string, right: string): number {
  if (left < right) {
    return -1;
  }
  if (left > right) {
    return 1;
  }
  return 0;
}
