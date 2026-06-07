import assert from 'node:assert/strict';
import test from 'node:test';

import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';

function firstOp(plan: ReturnType<typeof compileTaskSpecToTaskPlan>): unknown {
  return ((plan.steps[0] as unknown as Record<string, unknown>)['write'] as { ops: unknown[] }).ops[0];
}

function makeSpec(changes: Record<string, unknown>[]) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_umg_widget',
    feature_name: 'UMGWidgetTreeP0',
    target: { asset_path: '/Game/UI/WBP_Menu', target_type: 'widget_blueprint' },
    behavior: {
      widget_strategy: 'widget_blueprint_edit',
      changes,
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: true,
      should_save: false,
    },
  };
}

test('edit_umg_widget lowers create_widget with virtual_index and expected parent', () => {
  const plan = compileTaskSpecToTaskPlan(makeSpec([{
    kind: 'create_widget',
    widget_name: 'Button_Start',
    widget_class: '/Script/UMG.Button',
    parent_name: 'VerticalBox_Menu',
    virtual_index: 0,
    expected_parent_name: 'VerticalBox_Menu',
  }]) as never);

  assert.deepEqual(firstOp(plan), {
    op: 'add_widget',
    widget_name: 'Button_Start',
    widget_class: '/Script/UMG.Button',
    parent_name: 'VerticalBox_Menu',
    virtual_index: 0,
    expected_parent_name: 'VerticalBox_Menu',
  });
});

test('edit_umg_widget lowers move_widget with expected_virtual_index', () => {
  const plan = compileTaskSpecToTaskPlan(makeSpec([{
    kind: 'move_widget',
    widget_name: 'Button_Start',
    new_parent_name: 'Canvas_Root',
    virtual_index: 1,
    expected_parent_name: 'VerticalBox_Menu',
    expected_virtual_index: 0,
  }]) as never);

  assert.deepEqual(firstOp(plan), {
    op: 'move_widget',
    widget_name: 'Button_Start',
    new_parent_name: 'Canvas_Root',
    virtual_index: 1,
    expected_parent_name: 'VerticalBox_Menu',
    expected_virtual_index: 0,
  });
});

test('edit_umg_widget lowers set_named_slot_content as standalone op', () => {
  const plan = compileTaskSpecToTaskPlan(makeSpec([{
    kind: 'set_named_slot_content',
    host_widget_name: 'DialogShell',
    slot_name: 'Body',
    virtual_index: 0,
    widget_name: 'InventoryPanel',
    widget_class: '/Game/UI/WBP_InventoryPanel',
    replace_existing: true,
    expected_content_widget_name: 'OldBody',
  }]) as never);

  assert.deepEqual(firstOp(plan), {
    op: 'set_named_slot_content',
    host_widget_name: 'DialogShell',
    slot_name: 'Body',
    virtual_index: 0,
    widget_name: 'InventoryPanel',
    widget_class: '/Game/UI/WBP_InventoryPanel',
    replace_existing: true,
    expected_content_widget_name: 'OldBody',
  });
});

test('edit_umg_widget rejects old parent_widget_name and insert_index fields', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeSpec([{
      kind: 'create_widget',
      widget_name: 'Button_Start',
      widget_class: '/Script/UMG.Button',
      parent_widget_name: 'Canvas_Root',
    }]) as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.issues[0]?.code, 'unsupported_umg_widget_parent_widget_name');
      return true;
    },
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makeSpec([{
      kind: 'move_widget',
      widget_name: 'Button_Start',
      new_parent_name: 'Canvas_Root',
      insert_index: 0,
    }]) as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.issues[0]?.code, 'unsupported_umg_widget_insert_index');
      return true;
    },
  );
});
