import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
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
    if (kind === 'create_widget') {
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_tree_edit',
        [omitUndefined({
          op: 'add_widget',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
          widget_class: getRequiredString(change, 'widget_class', `${path}.widget_class`),
          parent_name: optionalString(change, 'parent_name'),
          slot_name: optionalString(change, 'slot_name'),
          virtual_index: optionalInt(change, 'virtual_index', path),
          expected_parent_name: optionalString(change, 'expected_parent_name'),
        })],
      );
    }

    if (kind === 'move_widget') {
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_tree_edit',
        [omitUndefined({
          op: 'move_widget',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
          new_parent_name: getRequiredString(change, 'new_parent_name', `${path}.new_parent_name`),
          virtual_index: optionalInt(change, 'virtual_index', path),
          expected_parent_name: optionalString(change, 'expected_parent_name'),
          expected_virtual_index: optionalInt(change, 'expected_virtual_index', path),
        })],
      );
    }

    if (kind === 'set_named_slot_content') {
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_tree_edit',
        [omitUndefined({
          op: 'set_named_slot_content',
          host_widget_name: getRequiredString(change, 'host_widget_name', `${path}.host_widget_name`),
          slot_name: getRequiredString(change, 'slot_name', `${path}.slot_name`),
          virtual_index: optionalInt(change, 'virtual_index', path),
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
          widget_class: getRequiredString(change, 'widget_class', `${path}.widget_class`),
          replace_existing: change['replace_existing'] === true ? true : undefined,
          expected_content_widget_name: optionalString(change, 'expected_content_widget_name'),
        })],
      );
    }

    if (kind === 'update_widget_property') {
      if (!Object.hasOwn(change, 'value')) {
        throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.value is required.`, [
          {
            code: 'missing_umg_widget_property_value',
            path: `${path}.value`,
            message: 'Provide value for update_widget_property.',
          },
        ]);
      }
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
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_property_edit',
        [omitUndefined({
          op: 'set_widget_property',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
          property_path: propertyPath,
          property_name: propertyName,
          value: literalValue(change['value']),
        })],
      );
    }

    if (kind === 'delete_widget') {
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_tree_edit',
        [{
          op: 'remove_widget',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
        }],
      );
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', `Unsupported UMG widget change kind: ${kind}`, [
      {
        code: 'unsupported_umg_widget_change_kind',
        path: `${path}.kind`,
        message: 'Use create_widget, update_widget_property, delete_widget, move_widget, or set_named_slot_content.',
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
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
