import { strict as assert } from 'node:assert';
import test from 'node:test';

import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';
import { isCanonicalTsTaskType } from './task-compiler-registry.js';
import {
  componentTaskPlanFixture,
  componentTaskSpecFixture,
  dataTableTaskPlanFixture,
  dataTableTaskSpecFixture,
  widgetTaskPlanFixture,
  widgetTaskSpecFixture,
} from '../fixtures/task-protocol.fixtures.js';

function clone<T>(value: T): T {
  return structuredClone(value);
}

test('canonical registry advertises component, UMG widget, and DataTable task types', () => {
  assert.equal(isCanonicalTsTaskType('edit_blueprint_components'), true);
  assert.equal(isCanonicalTsTaskType('edit_umg_widget'), true);
  assert.equal(isCanonicalTsTaskType('edit_data_table'), true);
});

test('edit_blueprint_components lowers to existing blueprint_component TaskPlan shape', () => {
  assert.deepEqual(
    compileTaskSpecToTaskPlan(componentTaskSpecFixture as never),
    componentTaskPlanFixture,
  );
});

test('edit_umg_widget lowers supported WidgetTree changes to existing umg_widget TaskPlan shape', () => {
  assert.deepEqual(
    compileTaskSpecToTaskPlan(widgetTaskSpecFixture as never),
    widgetTaskPlanFixture,
  );
});

test('edit_data_table lowers add/update/delete rows to existing data_table TaskPlan shape', () => {
  assert.deepEqual(
    compileTaskSpecToTaskPlan(dataTableTaskSpecFixture as never),
    dataTableTaskPlanFixture,
  );
});

test('component configure change requires a non-empty properties array or object', () => {
  const taskSpec = clone(componentTaskSpecFixture) as Record<string, unknown>;
  const behavior = taskSpec.behavior as Record<string, unknown>;
  behavior.changes = [{
    kind: 'configure_component',
    name: 'DoorRoot',
  }];

  assert.throws(
    () => compileTaskSpecToTaskPlan(taskSpec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'missing_component_properties');
      return true;
    },
  );
});

test('UMG update_widget_property requires value before runtime lowering', () => {
  const taskSpec = clone(widgetTaskSpecFixture) as Record<string, unknown>;
  const behavior = taskSpec.behavior as Record<string, unknown>;
  behavior.changes = [{
    kind: 'update_widget_property',
    widget_name: 'TitleText',
    property_path: 'Text',
  }];

  assert.throws(
    () => compileTaskSpecToTaskPlan(taskSpec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'missing_umg_widget_property_value');
      return true;
    },
  );
});

test('DataTable update rows require fields before runtime lowering', () => {
  const taskSpec = clone(dataTableTaskSpecFixture) as Record<string, unknown>;
  const behavior = taskSpec.behavior as Record<string, unknown>;
  behavior.rows = [{
    action: 'update',
    row_name: 'Shotgun',
  }];

  assert.throws(
    () => compileTaskSpecToTaskPlan(taskSpec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'missing_data_table_row_fields');
      return true;
    },
  );
});
