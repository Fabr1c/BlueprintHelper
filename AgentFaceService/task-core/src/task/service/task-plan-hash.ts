import { createHash } from 'node:crypto';

export function createTaskPlanHash(taskPlan: unknown): string {
  return createStableSha256(taskPlan);
}

export function createTaskSpecHash(taskSpec: unknown): string {
  return createStableSha256(taskSpec);
}

export function createExecutionPolicyHash(policy: unknown): string {
  return createStableSha256(policy);
}

export function createTaskVerificationHash(verification: unknown): string {
  return createStableSha256(verification);
}

function createStableSha256(value: unknown): string {
  return createHash('sha256')
    .update(stableJsonStringify(value))
    .digest('hex');
}

function stableJsonStringify(value: unknown): string {
  return JSON.stringify(toStableJsonValue(value));
}

function toStableJsonValue(value: unknown): unknown {
  if (value === null || typeof value === 'string' || typeof value === 'number' || typeof value === 'boolean') {
    return value;
  }

  if (Array.isArray(value)) {
    return value.map((entry) => {
      const stableEntry = toStableJsonValue(entry);
      return stableEntry === undefined ? null : stableEntry;
    });
  }

  if (typeof value === 'object') {
    const source = value as Record<string, unknown>;
    const stableObject: Record<string, unknown> = {};
    for (const key of Object.keys(source).sort()) {
      const stableEntry = toStableJsonValue(source[key]);
      if (stableEntry !== undefined) {
        stableObject[key] = stableEntry;
      }
    }
    return stableObject;
  }

  return undefined;
}
