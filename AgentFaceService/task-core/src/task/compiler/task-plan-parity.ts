import type { TaskPlan } from '../schema/task-schemas.js';

export type TaskPlanParityStatus = 'passed' | 'failed';

export interface TaskPlanParityResult {
  status: TaskPlanParityStatus;
  differences: string[];
}

export function compareTaskPlanParity(left: TaskPlan, right: TaskPlan): TaskPlanParityResult {
  const leftNormalized = normalizeTaskPlanForParity(left);
  const rightNormalized = normalizeTaskPlanForParity(right);
  const leftJson = JSON.stringify(leftNormalized);
  const rightJson = JSON.stringify(rightNormalized);

  if (leftJson === rightJson) {
    return {
      status: 'passed',
      differences: [],
    };
  }

  return {
    status: 'failed',
    differences: collectFirstDifferences(leftNormalized, rightNormalized),
  };
}

export function normalizeTaskPlanForParity(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => normalizeTaskPlanForParity(item));
  }

  if (isRecord(value)) {
    return Object.fromEntries(
      Object.keys(value)
        .sort()
        .flatMap((key) => {
          const item = value[key];
          return item === undefined ? [] : [[key, normalizeTaskPlanForParity(item)]];
        }),
    );
  }

  return value;
}

function collectFirstDifferences(left: unknown, right: unknown, path = '$', out: string[] = []): string[] {
  if (out.length >= 8) {
    return out;
  }

  if (Array.isArray(left) || Array.isArray(right)) {
    if (!Array.isArray(left) || !Array.isArray(right)) {
      out.push(`${path}: type mismatch`);
      return out;
    }
    if (left.length !== right.length) {
      out.push(`${path}: array length ${left.length} != ${right.length}`);
    }
    const max = Math.min(left.length, right.length);
    for (let index = 0; index < max && out.length < 8; index += 1) {
      collectFirstDifferences(left[index], right[index], `${path}[${index}]`, out);
    }
    return out;
  }

  if (isRecord(left) || isRecord(right)) {
    if (!isRecord(left) || !isRecord(right)) {
      out.push(`${path}: type mismatch`);
      return out;
    }
    const keys = Array.from(new Set([...Object.keys(left), ...Object.keys(right)])).sort();
    for (const key of keys) {
      if (!(key in left)) {
        out.push(`${path}.${key}: missing from left`);
      } else if (!(key in right)) {
        out.push(`${path}.${key}: missing from right`);
      } else {
        collectFirstDifferences(left[key], right[key], `${path}.${key}`, out);
      }
      if (out.length >= 8) break;
    }
    return out;
  }

  if (left !== right) {
    out.push(`${path}: ${JSON.stringify(left)} != ${JSON.stringify(right)}`);
  }
  return out;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
