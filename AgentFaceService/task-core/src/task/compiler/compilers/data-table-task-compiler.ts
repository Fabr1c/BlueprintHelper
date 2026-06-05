import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import {
  assertExactString,
  getRequiredString,
  isRecord,
  makeCompositeCapabilityStep,
  makeTaskPlanWithSteps,
  omitUndefined,
  optionalFieldsObject,
  requiredArray,
} from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

export const dataTableTaskCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'edit_data_table' }>> = {
  id: 'data_table',
  taskType: 'edit_data_table',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'edit_data_table' }> {
    return taskSpec.task_type === 'edit_data_table';
  },
  compile(taskSpec) {
    return compileDataTableTaskSpecToTaskPlan(taskSpec);
  },
};

function compileDataTableTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_data_table' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'row_strategy',
    'row_edit',
    'behavior.row_strategy',
    'Use row_strategy="row_edit".',
  );

  const rows = requiredArray(behavior, 'rows', 'behavior.rows');
  const steps = rows.map((rawRow, index) => {
    const path = `behavior.rows[${index}]`;
    if (!isRecord(rawRow)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_data_table_row', path, message: 'Provide a DataTable row object.' },
      ]);
    }

    const row = rawRow as Record<string, unknown>;
    const action = getRequiredString(row, 'action', `${path}.action`);
    const rowName = getRequiredString(row, 'row_name', `${path}.row_name`);

    if (action === 'add') {
      return makeCompositeCapabilityStep(
        index + 1,
        'data_table',
        taskSpec.target.asset_path,
        'row_edit',
        [omitUndefined({
          op: 'add_row',
          row_name: rowName,
          fields: optionalFieldsObject(row['fields'], `${path}.fields`, false),
        })],
      );
    }

    if (action === 'update') {
      return makeCompositeCapabilityStep(
        index + 1,
        'data_table',
        taskSpec.target.asset_path,
        'row_edit',
        [{
          op: 'update_row',
          row_name: rowName,
          fields: optionalFieldsObject(row['fields'], `${path}.fields`, true),
        }],
      );
    }

    if (action === 'delete') {
      return makeCompositeCapabilityStep(
        index + 1,
        'data_table',
        taskSpec.target.asset_path,
        'row_edit',
        [{
          op: 'delete_row',
          row_name: rowName,
        }],
      );
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', `Unsupported DataTable row action: ${action}`, [
      {
        code: 'unsupported_data_table_row_action',
        path: `${path}.action`,
        message: 'Use add, update, or delete.',
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}
