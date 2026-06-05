import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import {
  asRecord,
  assertExactString,
  classSettingsDefaultArray,
  getRequiredString,
  makeCompositeCapabilityStep,
  makeTaskPlanWithSteps,
  stringArrayOrEmpty,
} from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

export const blueprintClassSettingsTaskCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'edit_blueprint_class_settings' }>> = {
  id: 'blueprint_class_settings',
  taskType: 'edit_blueprint_class_settings',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'edit_blueprint_class_settings' }> {
    return taskSpec.task_type === 'edit_blueprint_class_settings';
  },
  compile(taskSpec) {
    return compileBlueprintClassSettingsTaskSpecToTaskPlan(taskSpec);
  },
};

function compileBlueprintClassSettingsTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_class_settings' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'class_settings_strategy',
    'class_settings',
    'behavior.class_settings_strategy',
    'Use class_settings_strategy="class_settings".',
  );

  if (Object.hasOwn(behavior, 'parent_class')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Use behavior.reparent.new_parent_class for Blueprint reparent operations.', [
      {
        code: 'legacy_parent_class_field',
        path: 'behavior.parent_class',
        message: 'Use behavior.reparent.new_parent_class.',
      },
    ]);
  }

  const ops: Record<string, unknown>[] = [];
  const interfaces = asRecord(behavior['interfaces']);
  const ensurePresent = stringArrayOrEmpty(interfaces?.['ensure_present'], 'behavior.interfaces.ensure_present');
  if (ensurePresent.length > 0) {
    ops.push({
      op: 'add_implemented_interfaces',
      interface_paths: ensurePresent,
    });
  }

  const ensureAbsent = stringArrayOrEmpty(interfaces?.['ensure_absent'], 'behavior.interfaces.ensure_absent');
  if (ensureAbsent.length > 0) {
    ops.push({
      op: 'remove_implemented_interfaces',
      interface_paths: ensureAbsent,
    });
  }

  const classDefaults = classSettingsDefaultArray(behavior['class_defaults'], 'behavior.class_defaults');
  if (classDefaults.length > 0) {
    ops.push({
      op: 'set_class_default_properties',
      settings: classDefaults,
    });
  }

  const reparent = asRecord(behavior['reparent']);
  if (reparent) {
    ops.push({
      op: 'reparent_blueprint',
      new_parent_class: getRequiredString(reparent, 'new_parent_class', 'behavior.reparent.new_parent_class'),
    });
  }

  if (ops.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'edit_blueprint_class_settings requires at least one class settings change.', [
      {
        code: 'missing_class_settings_change',
        path: 'behavior',
        message: 'Provide interfaces, class_defaults, or reparent.new_parent_class.',
      },
    ]);
  }

  return makeTaskPlanWithSteps(
    taskSpec,
    ops.map((op, index) => makeCompositeCapabilityStep(
      index + 1,
      'blueprint_class_settings',
      taskSpec.target.asset_path,
      'class_settings',
      [op],
    )),
  );
}
