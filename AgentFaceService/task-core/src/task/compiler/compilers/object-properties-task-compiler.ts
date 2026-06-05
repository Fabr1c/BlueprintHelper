import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import {
  assertExactString,
  getRequiredString,
  isRecord,
  literalValue,
  makeSingleCapabilityTaskPlan,
  requiredArray,
} from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

export const objectPropertiesTaskCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'edit_object_properties' }>> = {
  id: 'object_properties',
  taskType: 'edit_object_properties',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'edit_object_properties' }> {
    return taskSpec.task_type === 'edit_object_properties';
  },
  compile(taskSpec) {
    return compileObjectPropertiesTaskSpecToTaskPlan(taskSpec);
  },
};

function compileObjectPropertiesTaskSpecToTaskPlan(taskSpec: Extract<TaskSpec, { task_type: 'edit_object_properties' }>): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'property_strategy',
    'property_edit',
    'behavior.property_strategy',
    'Use property_strategy="property_edit".',
  );

  const changes = requiredArray(behavior, 'changes', 'behavior.changes');
  const settings = changes.map((rawChange, index) => {
    const path = `behavior.changes[${index}]`;
    if (!isRecord(rawChange)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_property_change', path, message: 'Use { property_path, value }.' },
      ]);
    }
    const change = rawChange as Record<string, unknown>;
    if (!Object.hasOwn(change, 'value')) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.value is required.`, [
        { code: 'missing_property_value', path: `${path}.value`, message: 'Provide value.' },
      ]);
    }
    return {
      property_path: getRequiredString(change, 'property_path', `${path}.property_path`),
      value: literalValue(change['value']),
    };
  });

  const op = settings.length === 1
    ? {
        op: 'set_object_property',
        property_path: settings[0].property_path,
        value: settings[0].value,
      }
    : {
        op: 'set_object_properties',
        settings,
      };

  return makeSingleCapabilityTaskPlan(
    taskSpec,
    'object_property',
    'property_edit',
    [op],
    { property_scope: 'uobject' },
  );
}
