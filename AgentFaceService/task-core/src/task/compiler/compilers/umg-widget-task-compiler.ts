import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import { UMG_WIDGET_OPERATION_MANIFEST } from '../../../tool-surface/templates/generated/umg-widget-operation-manifest.generated.js';
import type { UmgWidgetOperationDescriptor } from '../../../tool-surface/templates/umg-widget-operation-descriptors.js';
import {
  assertExactString,
  getRequiredString,
  isRecord,
  literalValue,
  makeCompositeCapabilityStep,
  makeTaskPlanWithSteps,
  omitUndefined,
  optionalString,
  requiredArray,
} from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

export const umgWidgetTaskCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'edit_umg_widget' }>> = {
  id: 'umg_widget',
  taskType: 'edit_umg_widget',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'edit_umg_widget' }> {
    return taskSpec.task_type === 'edit_umg_widget';
  },
  compile(taskSpec) {
    return compileUMGWidgetTaskSpecToTaskPlan(taskSpec);
  },
};

function compileUMGWidgetTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_umg_widget' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'widget_strategy',
    'widget_blueprint_edit',
    'behavior.widget_strategy',
    'Use widget_strategy="widget_blueprint_edit".',
  );

  const changes = requiredArray(behavior, 'changes', 'behavior.changes');
  const steps = changes.map((rawChange, index) => {
    const path = `behavior.changes[${index}]`;
    if (!isRecord(rawChange)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_umg_widget_change', path, message: 'Provide a UMG widget change object.' },
      ]);
    }

    const change = rawChange as Record<string, unknown>;
    rejectLegacyWidgetTreeFields(change, path);

    const kind = getRequiredString(change, 'kind', `${path}.kind`);
    const descriptor = getDescriptor(kind);
    if (descriptor) {
      validateRequiredDescriptorFields(change, descriptor, path);
      validateDescriptorSpecificRules(change, descriptor, path);
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        descriptor.taskplan_strategy,
        [buildDescriptorOperation(change, descriptor, path)],
      );
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', `Unsupported UMG widget change kind: ${kind}`, [
      {
        code: 'unsupported_umg_widget_change_kind',
        path: `${path}.kind`,
        message: `Use one of: ${UMG_WIDGET_OPERATION_MANIFEST.map((entry) => entry.kind).join(', ')}.`,
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}

function getDescriptor(kind: string): UmgWidgetOperationDescriptor | undefined {
  return UMG_WIDGET_OPERATION_MANIFEST.find((descriptor) => descriptor.kind === kind);
}

function buildDescriptorOperation(
  change: Record<string, unknown>,
  descriptor: UmgWidgetOperationDescriptor,
  path: string,
): Record<string, unknown> {
  const payload: Record<string, unknown> = { op: descriptor.taskplan_op };
  for (const field of [...descriptor.required_fields, ...descriptor.optional_fields]) {
    if (!Object.hasOwn(change, field)) {
      continue;
    }
    payload[field] = coerceDescriptorField(change, field, path);
  }
  return omitUndefined(payload);
}

function validateRequiredDescriptorFields(
  change: Record<string, unknown>,
  descriptor: UmgWidgetOperationDescriptor,
  path: string,
): void {
  for (const field of descriptor.required_fields) {
    if (field === 'value') {
      if (!Object.hasOwn(change, field)) {
        throwMissingField(descriptor, field, path);
      }
      continue;
    }
    if (field === 'is_variable' || field === 'preserve_children' || field === 'preserve_slot') {
      getRequiredBooleanWithCode(
        change,
        field,
        `${path}.${field}`,
        missingCodeForField(descriptor, field),
        `Provide boolean ${field} for ${descriptor.kind}.`,
      );
      continue;
    }
    if (field === 'virtual_index' || field === 'expected_virtual_index') {
      optionalInt(change, field, path);
      continue;
    }
    if (field === 'name_mapping') {
      if (!isRecord(change[field])) {
        throwMissingField(descriptor, field, path);
      }
      continue;
    }
    getRequiredStringWithCode(
      change,
      field,
      `${path}.${field}`,
      missingCodeForField(descriptor, field),
      `Provide ${field} for ${descriptor.kind}.`,
    );
  }
}

function validateDescriptorSpecificRules(
  change: Record<string, unknown>,
  descriptor: UmgWidgetOperationDescriptor,
  path: string,
): void {
  if (descriptor.kind === 'update_widget_property') {
    const propertyPath = optionalString(change, 'property_path');
    const propertyName = optionalString(change, 'property_name');
    if (!propertyPath && !propertyName) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.property_path is required.`, [
        {
          code: 'missing_umg_widget_property_path',
          path: `${path}.property_path`,
          message: 'Provide property_path or property_name.',
        },
      ]);
    }
  }
}

function coerceDescriptorField(
  change: Record<string, unknown>,
  field: string,
  path: string,
): unknown {
  if (field === 'virtual_index' || field === 'expected_virtual_index') {
    return optionalInt(change, field, path);
  }
  if (field === 'value') {
    return literalValue(change[field]);
  }
  return change[field];
}

function optionalInt(record: Record<string, unknown>, field: string, path: string): number | undefined {
  if (!Object.hasOwn(record, field)) return undefined;
  const value = record[field];
  if (!Number.isInteger(value) || (value as number) < 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.${field} must be a non-negative integer.`, [{
      code: 'invalid_umg_widget_virtual_index',
      path: `${path}.${field}`,
      message: `${field} must be a non-negative integer.`,
    }]);
  }
  return value as number;
}

function throwMissingField(
  descriptor: UmgWidgetOperationDescriptor,
  field: string,
  path: string,
): never {
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.${field} is required.`, [
    {
      code: missingCodeForField(descriptor, field),
      path: `${path}.${field}`,
      message: `Provide ${field} for ${descriptor.kind}.`,
    },
  ]);
}

function missingCodeForField(
  descriptor: UmgWidgetOperationDescriptor,
  field: string,
): string {
  const explicitCodes: Readonly<Record<string, string>> = {
    'update_widget_property.value': 'missing_umg_widget_property_value',
    'update_widget_property.property_path': 'missing_umg_widget_property_path',
    'update_widget_property.property_name': 'missing_umg_widget_property_path',
    'update_widget_property.widget_name': 'missing_umg_widget_name',
    'set_slot_property.value': 'missing_umg_slot_property_value',
    'set_slot_property.property_path': 'missing_umg_slot_property_path',
    'set_slot_property.widget_name': 'missing_umg_widget_name',
    'set_widget_as_variable.widget_name': 'missing_umg_widget_name',
    'set_widget_as_variable.is_variable': 'missing_umg_widget_variable_state',
  };
  return explicitCodes[`${descriptor.kind}.${field}`] ?? `missing_umg_${field}`;
}

function getRequiredStringWithCode(
  record: Record<string, unknown>,
  field: string,
  path: string,
  code: string,
  message: string,
): string {
  const value = record[field];
  if (typeof value === 'string' && value.trim().length > 0) {
    return value;
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty string.`, [
    { code, path, message },
  ]);
}

function getRequiredBooleanWithCode(
  record: Record<string, unknown>,
  field: string,
  path: string,
  code: string,
  message: string,
): boolean {
  const value = record[field];
  if (typeof value === 'boolean') {
    return value;
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a boolean.`, [
    { code, path, message },
  ]);
}

function rejectLegacyWidgetTreeFields(change: Record<string, unknown>, path: string): void {
  if (Object.hasOwn(change, 'parent_widget_name')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.parent_widget_name is not supported.`, [{
      code: 'unsupported_umg_widget_parent_widget_name',
      path: `${path}.parent_widget_name`,
      message: 'Use parent_name.',
    }]);
  }
  if (Object.hasOwn(change, 'insert_index') || Object.hasOwn(change, 'child_index')) {
    const field = Object.hasOwn(change, 'insert_index') ? 'insert_index' : 'child_index';
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.${field} is not supported.`, [{
      code: `unsupported_umg_widget_${field}`,
      path: `${path}.${field}`,
      message: 'Use virtual_index.',
    }]);
  }
}
