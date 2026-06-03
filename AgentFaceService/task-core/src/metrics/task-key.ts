import type { TaskSpec } from '../task/schema/task-schemas.js';
import type { MetricsTaskKey } from './metrics-types.js';
import { hashStableJson } from './stable-hash.js';

const MAX_TARGET_LABEL_LENGTH = 96;
const TARGET_LABEL_HEAD_LENGTH = 48;
const TARGET_LABEL_TAIL_LENGTH = MAX_TARGET_LABEL_LENGTH - TARGET_LABEL_HEAD_LENGTH - 3;

export function createMetricsTaskKey(taskSpec: TaskSpec): MetricsTaskKey {
  const raw = taskSpec as Record<string, unknown>;
  const target = asRecord(raw['target']) ?? {};
  const targetType = readString(target['target_type']) ?? 'unknown_target_type';
  const featureName = readString(raw['feature_name']);
  const assetPath = readString(target['asset_path']);
  const assetClass = readString(target['asset_class']);
  const targetRef = assetPath ?? assetClass ?? targetType;

  return {
    task_type: readString(raw['task_type']) ?? 'unknown_task_type',
    ...(featureName ? { feature_name: featureName } : {}),
    target_type: targetType,
    target_ref_hash: hashStableJson({
      target_type: targetType,
      ref: targetRef,
    }),
    target_ref_label: compactTargetLabel(targetRef),
  };
}

export function compactTargetLabel(value: string): string {
  const compactSlashPath = compactSlashDelimitedLabel(value);
  if (compactSlashPath) {
    return compactLength(compactSlashPath);
  }

  return compactLength(value);
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

function readString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}

function compactSlashDelimitedLabel(value: string): string | undefined {
  if (!value.includes('/')) {
    return undefined;
  }

  const hasLeadingSlash = value.startsWith('/');
  const segments = value.split('/').filter((segment) => segment.length > 0);
  if (segments.length < 2) {
    return undefined;
  }

  const prefix = hasLeadingSlash ? '/' : '';
  return `${prefix}${segments[0]}/.../${segments[segments.length - 1]}`;
}

function compactLength(value: string): string {
  if (value.length <= MAX_TARGET_LABEL_LENGTH) {
    return value;
  }

  return `${value.slice(0, TARGET_LABEL_HEAD_LENGTH)}...${value.slice(-TARGET_LABEL_TAIL_LENGTH)}`;
}
