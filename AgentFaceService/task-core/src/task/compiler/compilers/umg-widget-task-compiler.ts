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
          parent_widget_name: optionalString(change, 'parent_widget_name'),
          parent_name: optionalString(change, 'parent_name'),
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
        message: 'Use create_widget, update_widget_property, or delete_widget.',
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}
