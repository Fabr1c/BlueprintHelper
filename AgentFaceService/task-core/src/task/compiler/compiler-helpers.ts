import { BlueprintPinTypeSpecSchema } from '../schema/blueprint-pin-type-spec.js';
import type { TaskPlan, TaskSpec } from '../schema/task-schemas.js';
import { TASK_PLAN_SCHEMA } from '../schema/task-schemas.js';
import { TaskSpecCompileError } from './task-compiler-errors.js';

export type TaskPlanStep = TaskPlan['steps'][number];

export function makeCompositeCapabilityStep(
  index: number,
  capability: string,
  assetPath: string,
  strategy: string,
  ops: Record<string, unknown>[],
): TaskPlanStep {
  return {
    step_id: `step_${String(index).padStart(3, '0')}`,
    capability,
    target: { asset_path: assetPath },
    write: { strategy, ops },
  } as TaskPlanStep;
}

export function makeSingleCapabilityTaskPlan(
  taskSpec: TaskSpec,
  capability: string,
  strategy: string,
  ops: Record<string, unknown>[],
  constraints?: Record<string, unknown>,
): TaskPlan {
  return makeTaskPlanWithSteps(taskSpec, [
    {
      ...makeCompositeCapabilityStep(1, capability, taskSpec.target.asset_path, strategy, ops),
      ...(constraints ? { constraints } : {}),
    } as TaskPlanStep,
  ]);
}

export function makeTaskPlanWithSteps(taskSpec: TaskSpec, steps: TaskPlanStep[]): TaskPlan {
  return {
    schema: TASK_PLAN_SCHEMA,
    task_name: taskSpec.feature_name,
    task_type: taskSpec.task_type,
    context_id: taskSpec.context_id,
    target_assets: [taskSpec.target.asset_path],
    execution_policy: {
      dry_run_mode: taskSpec.execution_policy.dry_run_mode,
      should_compile: taskSpec.validation.should_compile,
      should_save: taskSpec.validation.should_save,
      review_baseline_dirty_asset_policy: taskSpec.execution_policy.review_baseline_dirty_asset_policy ?? 'block',
    },
    steps: renumberSteps(steps),
  };
}

export function renumberSteps(steps: TaskPlanStep[]): TaskPlanStep[] {
  const oldIds = steps.map((step) => step.step_id);
  const newIds = steps.map((_, index) => `step_${String(index + 1).padStart(3, '0')}`);

  return steps.map((step, index) => ({
    ...step,
    step_id: newIds[index],
    ...('depends_on' in step && Array.isArray(step.depends_on)
      ? {
          depends_on: step.depends_on.map((id) => {
            for (let candidate = index - 1; candidate >= 0; candidate -= 1) {
              if (oldIds[candidate] === id) return newIds[candidate];
            }
            return id;
          }),
        }
      : {}),
  }));
}

export function assertExactString(
  record: Record<string, unknown>,
  field: string,
  expected: string,
  path: string,
  message: string,
): void {
  const actual = getRequiredString(record, field, path);
  if (actual === expected) return;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be ${expected}.`, [
    {
      code: 'unsupported_field_value',
      path,
      message,
    },
  ]);
}

export function requiredArray(record: Record<string, unknown>, field: string, path: string): unknown[] {
  const value = record[field];
  if (Array.isArray(value) && value.length > 0) return value;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty array.`, [
    {
      code: 'missing_required_array',
      path,
      message: `Provide at least one item in ${path}.`,
    },
  ]);
}

export function requiredNonEmptyArray(record: Record<string, unknown>, field: string, path: string): unknown[] {
  const value = record[field];
  if (Array.isArray(value) && value.length > 0) return value;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty list.`, [
    {
      code: 'missing_required_list',
      path,
      message: `${path} must be a non-empty list.`,
    },
  ]);
}

export function stringArrayOrEmpty(value: unknown, path: string): string[] {
  if (value === undefined || value === null) return [];
  if (!Array.isArray(value)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an array.`, [
      {
        code: 'invalid_string_array',
        path,
        message: `${path} must contain path strings.`,
      },
    ]);
  }
  return value.map((item, index) => {
    if (typeof item !== 'string' || item.length === 0) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}[${index}] must be a non-empty string.`, [
        {
          code: 'invalid_string_array_item',
          path: `${path}[${index}]`,
          message: `${path}[${index}] must be a non-empty string.`,
        },
      ]);
    }
    return item;
  });
}

export function classSettingsDefaultArray(rawSettings: unknown, path: string): Record<string, unknown>[] {
  if (rawSettings === undefined || rawSettings === null) return [];
  if (!Array.isArray(rawSettings)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an array.`, [
      {
        code: 'invalid_property_settings',
        path,
        message: 'Use an array of { property_path, value } settings.',
      },
    ]);
  }
  return rawSettings.map((rawSetting, index) => {
    if (!isRecord(rawSetting)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}[${index}] must be an object.`, [
        {
          code: 'invalid_property_setting',
          path: `${path}[${index}]`,
          message: 'Use { property_path, value }.',
        },
      ]);
    }
    const setting = rawSetting as Record<string, unknown>;
    if (!Object.hasOwn(setting, 'value')) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}[${index}].value is required.`, [
        {
          code: 'missing_property_value',
          path: `${path}[${index}].value`,
          message: 'Provide value.',
        },
      ]);
    }
    return {
      ...setting,
      property_path: getRequiredString(setting, 'property_path', `${path}[${index}].property_path`),
      value: literalValue(setting['value']),
    };
  });
}

export function getRequiredString(record: Record<string, unknown>, field: string, path: string): string {
  const value = record[field];
  if (typeof value === 'string' && value.trim().length > 0) {
    return value;
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty string.`, [
    {
      code: 'missing_required_string',
      path,
      message: `${path} must be a non-empty string.`,
    },
  ]);
}

export function optionalString(record: Record<string, unknown>, field: string): string | undefined {
  const value = record[field];
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

export function literalValue(value: unknown): unknown {
  if (isRecord(value) && value['kind'] === 'literal') {
    return value['value'];
  }
  return value;
}

export function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}

export function requiredRecord(record: Record<string, unknown>, field: string, path: string): Record<string, unknown> {
  const value = record[field];
  if (isRecord(value)) return value;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
    {
      code: 'missing_required_object',
      path,
      message: `${path} must be an object.`,
    },
  ]);
}

export function componentParent(component: Record<string, unknown>): unknown {
  if (typeof component['parent'] === 'string' && component['parent'].length > 0) {
    return component['parent'];
  }
  return compositeComponentParent(component);
}

export function componentSocket(component: Record<string, unknown>): unknown {
  if (typeof component['socket'] === 'string' && component['socket'].length > 0) {
    return component['socket'];
  }
  return compositeComponentSocket(component);
}

export function componentAttachRule(component: Record<string, unknown>): unknown {
  return compositeComponentAttachRule(component);
}

export function normalizeComponentCollisionPolicy(value: unknown): string | undefined {
  if (value === 'reuse_existing') return 'reuse_if_exists';
  if (value === 'reuse_if_type_matches' || value === 'reuse_if_exists') return 'reuse_if_exists';
  if (value === 'fail_if_exists') return 'fail_if_exists';
  if (value === 'block_if_class_mismatch') return 'block_if_class_mismatch';
  return undefined;
}

export function optionalComponentPolicyValue<T extends readonly string[]>(
  change: Record<string, unknown>,
  field: string,
  allowedValues: T,
  path: string,
  errorCode: string,
): T[number] | undefined {
  const value = change[field];
  if (value === undefined || value === null) {
    return undefined;
  }
  if (typeof value === 'string' && allowedValues.includes(value as T[number])) {
    return value as T[number];
  }
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is not supported.`, [
    {
      code: errorCode,
      path,
      message: `Supported values: ${allowedValues.join(', ')}.`,
    },
  ]);
}

export function requiredComponentHierarchyParent(
  change: Record<string, unknown>,
  path: string,
  fieldCandidates: readonly string[],
): string {
  for (const field of fieldCandidates) {
    if (typeof change[field] === 'string' && change[field].length > 0) {
      return change[field] as string;
    }
  }

  const fallbackParent = componentParent(change);
  if (typeof fallbackParent === 'string' && fallbackParent.length > 0) {
    return fallbackParent;
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is required.`, [
    {
      code: 'parent_component_not_found',
      path,
      message: 'Provide a parent component before lowering hierarchy mutations.',
    },
  ]);
}

export function propertySettingsArray(
  rawSettings: unknown,
  path: string,
  requireNonEmpty: boolean,
  owner: 'component' | 'property' = 'property',
): Record<string, unknown>[] {
  if (rawSettings === undefined || rawSettings === null) {
    if (requireNonEmpty) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is required.`, [
        {
          code: owner === 'component' ? 'missing_component_properties' : 'missing_property_settings',
          path,
          message: 'Provide at least one property setting.',
        },
      ]);
    }
    return [];
  }

  const settings = Array.isArray(rawSettings)
    ? rawSettings.map((rawSetting, index) => {
        if (!isRecord(rawSetting)) {
          throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Property setting must be an object.', [
            {
              code: 'invalid_property_setting',
              path: `${path}[${index}]`,
              message: 'Use { "property_path": "...", "value": ... }.',
            },
          ]);
        }
        const setting = rawSetting as Record<string, unknown>;
        if (!Object.hasOwn(setting, 'value')) {
          throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Property setting requires value.', [
            {
              code: 'missing_property_value',
              path: `${path}[${index}].value`,
              message: 'Provide value.',
            },
          ]);
        }
        return {
          property_path: getRequiredString(setting, 'property_path', `${path}[${index}].property_path`),
          value: literalValue(setting['value']),
        };
      })
    : isRecord(rawSettings)
      ? Object.entries(rawSettings).map(([propertyPath, value]) => ({
          property_path: propertyPath,
          value: literalValue(value),
        }))
      : undefined;

  if (!settings) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object or array.`, [
      {
        code: 'invalid_property_settings',
        path,
        message: 'Use an object map or an array of { property_path, value } settings.',
      },
    ]);
  }

  if (requireNonEmpty && settings.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must not be empty.`, [
      {
        code: owner === 'component' ? 'missing_component_properties' : 'missing_property_settings',
        path,
        message: 'Provide at least one property setting.',
      },
    ]);
  }

  return settings;
}

export function optionalFieldsObject(value: unknown, path: string, requireNonEmpty: boolean): Record<string, unknown> | undefined {
  if (value === undefined || value === null) {
    if (requireNonEmpty) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is required.`, [
        {
          code: 'missing_data_table_row_fields',
          path,
          message: 'DataTable update rows require a non-empty fields object.',
        },
      ]);
    }
    return undefined;
  }

  if (!isRecord(value) || Object.keys(value).length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty object.`, [
      {
        code: requireNonEmpty ? 'missing_data_table_row_fields' : 'invalid_data_table_row_fields',
        path,
        message: 'Use a non-empty object keyed by row field name.',
      },
    ]);
  }

  return value;
}

export function requireStructuredPinType(
  value: unknown,
  path: string,
  code: 'invalid_pin_type' | 'invalid_pin_type_evidence' = 'invalid_pin_type',
): Record<string, unknown> {
  if (typeof value === 'string') {
    throwLegacyPinTypeTokenUnsupported(path);
  }
  if (!isRecord(value)) {
    throwInvalidStructuredPinType(
      code,
      path,
      code === 'invalid_pin_type' ? invalidPinTypeMessage(path) : invalidPinTypeEvidenceMessage(path),
    );
  }

  const parsed = BlueprintPinTypeSpecSchema.safeParse(value);
  if (!parsed.success) {
    const firstIssue = parsed.error.issues[0];
    throw new TaskSpecCompileError(
      code,
      code === 'invalid_pin_type' ? invalidPinTypeMessage(path) : invalidPinTypeEvidenceMessage(path),
      [{
        code,
        path: joinIssuePath(path, firstIssue?.path ?? []),
        message: firstIssue?.message ?? (code === 'invalid_pin_type'
          ? invalidPinTypeMessage(path)
          : invalidPinTypeEvidenceMessage(path)),
      }],
    );
  }
  return parsed.data as Record<string, unknown>;
}

export function asRecord(value: unknown): Record<string, unknown> | undefined {
  return isRecord(value) ? value : undefined;
}

export function isNonEmptyRecord(value: unknown): value is Record<string, unknown> {
  return isRecord(value) && Object.keys(value).length > 0;
}

export function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function compositeComponentParent(component: Record<string, unknown>): unknown {
  const attach = asRecord(component['attach']);
  return typeof attach?.['parent'] === 'string' && attach['parent'].length > 0 ? attach['parent'] : undefined;
}

function compositeComponentSocket(component: Record<string, unknown>): unknown {
  const attach = asRecord(component['attach']);
  return typeof attach?.['socket'] === 'string' && attach['socket'].length > 0 ? attach['socket'] : undefined;
}

function compositeComponentAttachRule(component: Record<string, unknown>): unknown {
  if (typeof component['attach_rule'] === 'string' && component['attach_rule'].length > 0) {
    return component['attach_rule'];
  }
  const attach = asRecord(component['attach']);
  return typeof attach?.['rule'] === 'string' && attach['rule'].length > 0 ? attach['rule'] : undefined;
}

function invalidPinTypeMessage(path: string): string {
  return `${path} must be a structured BlueprintPinTypeSpec object.`;
}

function invalidPinTypeEvidenceMessage(path: string): string {
  return `${path} must be structured pin-type evidence.`;
}

function throwLegacyPinTypeTokenUnsupported(path: string): never {
  throw new TaskSpecCompileError('legacy_pin_type_token_unsupported', invalidPinTypeMessage(path), [
    {
      code: 'legacy_pin_type_token_unsupported',
      path,
      message: 'Use a structured BlueprintPinTypeSpec object instead of a legacy string token.',
    },
  ]);
}

function throwInvalidStructuredPinType(
  code: 'invalid_pin_type' | 'invalid_pin_type_evidence',
  path: string,
  message: string,
): never {
  throw new TaskSpecCompileError(code, message, [
    {
      code,
      path,
      message,
    },
  ]);
}

function joinIssuePath(basePath: string, issuePath: readonly (string | number)[]): string {
  if (issuePath.length === 0) return basePath;
  return `${basePath}.${issuePath.join('.')}`;
}
