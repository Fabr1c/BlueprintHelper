import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { compileTaskSpecToTaskPlan } from './task-compiler.js';
import { TaskPlanSchema, TaskSpecSchema } from './task-schemas.js';
import {
  blueprintVariableTaskPlanFixtures,
  blueprintVariableTaskSpecFixtures,
  graphWriteAppendExpectedTaskPlanFixture,
  graphWriteAppendTaskSpecFixture,
  graphWriteExpectedTaskPlanFixtures,
  graphWriteTaskSpecFixtures,
  p1TaskPlanFixtures,
  p1TaskSpecFixtures,
} from './task-protocol.fixtures.js';

describe('Task protocol fixtures', () => {
  it('parses and compiles the GraphWrite Append TaskSpec fixture into the expected TaskPlan', () => {
    const taskSpec = TaskSpecSchema.parse(graphWriteAppendTaskSpecFixture);
    const taskPlan = compileTaskSpecToTaskPlan(taskSpec);

    assert.deepEqual(taskPlan, graphWriteAppendExpectedTaskPlanFixture);
  });

  it('parses and compiles canonical GraphWrite TaskSpec fixtures into structured IR TaskPlans', () => {
    assert.equal(graphWriteTaskSpecFixtures.length, graphWriteExpectedTaskPlanFixtures.length);

    for (let index = 0; index < graphWriteTaskSpecFixtures.length; index++) {
      const taskSpec = TaskSpecSchema.parse(graphWriteTaskSpecFixtures[index]);
      const expectedTaskPlan = graphWriteExpectedTaskPlanFixtures[index];
      const taskPlan = compileTaskSpecToTaskPlan(taskSpec);

      assert.deepEqual(taskPlan, expectedTaskPlan);
      assert.doesNotThrow(() => TaskPlanSchema.parse(taskPlan));
      for (const step of taskPlan.steps) {
        assert.ok('capability' in step);
        assert.ok(
          step.capability === 'graph_write' || step.capability === 'blueprint_signature',
          `${taskPlan.task_name}.${step.step_id}.${step.capability}`,
        );
        assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false, taskPlan.task_name);
      }
    }
  });

  it('uses should_compile and should_save instead of legacy compile and save fields', () => {
    const taskSpec = graphWriteAppendTaskSpecFixture as Record<string, unknown>;
    const taskPlan = graphWriteAppendExpectedTaskPlanFixture as Record<string, unknown>;

    assertNoLegacyCompileSaveFields('TaskSpec.validation', getRecord(taskSpec, 'validation'));
    assertNoLegacyCompileSaveFields('TaskSpec.execution_policy', getRecord(taskSpec, 'execution_policy'));
    assertNoLegacyCompileSaveFields('TaskPlan.execution_policy', getRecord(taskPlan, 'execution_policy'));
  });

  it('parses canonical P1 TaskSpec fixtures with TaskSpecSchema', () => {
    for (const taskSpecFixture of p1TaskSpecFixtures) {
      assert.doesNotThrow(() => TaskSpecSchema.parse(taskSpecFixture));
    }
  });

  it('parses and compiles canonical Blueprint Variable TaskSpec fixtures into expected TaskPlans', () => {
    assert.equal(blueprintVariableTaskSpecFixtures.length, blueprintVariableTaskPlanFixtures.length);

    for (let index = 0; index < blueprintVariableTaskSpecFixtures.length; index++) {
      const taskSpec = TaskSpecSchema.parse(blueprintVariableTaskSpecFixtures[index]);
      const expectedTaskPlan = blueprintVariableTaskPlanFixtures[index];
      const taskPlan = compileTaskSpecToTaskPlan(taskSpec);

      assert.deepEqual(taskPlan, expectedTaskPlan);
      assert.doesNotThrow(() => TaskPlanSchema.parse(taskPlan));
    }
  });

  it('keeps Blueprint Variable fixtures as compiler-owned structural IR, not adapter calls', () => {
    const expected = new Map([
      ['member_variables', [
        'ensure_member_variable',
        'set_member_variable_properties',
        'remove_member_variable',
      ]],
      ['member_defaults', [
        'set_member_default',
      ]],
      ['local_variables', [
        'ensure_local_variable',
        'set_local_variable_properties',
        'remove_local_variable',
      ]],
    ]);

    for (const taskPlanFixture of blueprintVariableTaskPlanFixtures) {
      const parsedTaskPlan = TaskPlanSchema.parse(taskPlanFixture);
      const step = parsedTaskPlan.steps[0];
      assert.ok(step && 'capability' in step);
      assert.equal(step.capability, 'blueprint_variable');
      assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false, parsedTaskPlan.task_name);
      assert.equal(Object.hasOwn(step.write as Record<string, unknown>, 'operation'), false, parsedTaskPlan.task_name);

      const expectedOps = expected.get(step.write.strategy);
      assert.ok(expectedOps, step.write.strategy);
      assert.deepEqual(step.write.ops.map((op) => op.op), expectedOps);

      if (step.write.strategy === 'local_variables') {
        assert.equal((step.target as Record<string, unknown>).function_name, 'CalculateDamage');
      }
    }
  });

  it('parses canonical P1 TaskPlan fixtures with TaskPlanSchema and rejects adapter operation fields on steps', () => {
    for (const taskPlanFixture of p1TaskPlanFixtures) {
      assert.doesNotThrow(() => TaskPlanSchema.parse(taskPlanFixture));

      const parsedTaskPlan = TaskPlanSchema.parse(taskPlanFixture);
      for (const step of parsedTaskPlan.steps) {
        assert.equal(Object.hasOwn(step, 'operation'), false, `${parsedTaskPlan.task_name}.${step.step_id}`);
      }
    }
  });

  it('keeps canonical P1 TaskPlan fixtures as one structural op per compiler-owned step', () => {
    const expected = new Map([
      ['create_asset', {
        capabilities: ['asset_factory'],
        strategies: ['asset_create'],
        ops: ['create_asset'],
      }],
      ['edit_blueprint_components', {
        capabilities: ['blueprint_component', 'blueprint_component', 'blueprint_component'],
        strategies: ['component_tree', 'component_tree', 'component_tree'],
        ops: ['add_component', 'set_component_properties', 'remove_component'],
      }],
      ['edit_blueprint_class_settings', {
        capabilities: ['blueprint_class_settings', 'blueprint_class_settings', 'blueprint_class_settings'],
        strategies: ['class_settings', 'class_settings', 'class_settings'],
        ops: ['add_implemented_interfaces', 'remove_implemented_interfaces', 'set_class_default_properties'],
      }],
      ['edit_umg_widget', {
        capabilities: ['umg_widget', 'umg_widget', 'umg_widget'],
        strategies: ['widget_tree_edit', 'widget_property_edit', 'widget_tree_edit'],
        ops: ['add_widget', 'set_widget_property', 'remove_widget'],
      }],
      ['edit_data_table', {
        capabilities: ['data_table', 'data_table', 'data_table'],
        strategies: ['row_edit', 'row_edit', 'row_edit'],
        ops: ['add_row', 'update_row', 'delete_row'],
      }],
    ]);

    for (const taskPlanFixture of p1TaskPlanFixtures) {
      const expectedShape = expected.get(taskPlanFixture.task_type);
      assert.ok(expectedShape, taskPlanFixture.task_type);

      const capabilities: string[] = [];
      const strategies: string[] = [];
      const ops: string[] = [];

      for (const rawStep of taskPlanFixture.steps) {
        const step = rawStep as Record<string, unknown>;
        const write = getRecord(step, 'write');
        const writeOps = write.ops;
        assert.equal(Array.isArray(writeOps), true, `${taskPlanFixture.task_type}.${String(step.step_id)}.write.ops`);
        assert.equal((writeOps as unknown[]).length, 1, `${taskPlanFixture.task_type}.${String(step.step_id)}.write.ops`);
        const op = (writeOps as Record<string, unknown>[])[0];
        assert.equal(typeof op.op, 'string', `${taskPlanFixture.task_type}.${String(step.step_id)}.write.ops[0].op`);

        capabilities.push(String(step.capability));
        strategies.push(String(write.strategy));
        ops.push(String(op.op));
      }

      assert.deepEqual(capabilities, expectedShape.capabilities, `${taskPlanFixture.task_type}.capabilities`);
      assert.deepEqual(strategies, expectedShape.strategies, `${taskPlanFixture.task_type}.strategies`);
      assert.deepEqual(ops, expectedShape.ops, `${taskPlanFixture.task_type}.ops`);
    }
  });
});

function assertNoLegacyCompileSaveFields(label: string, value: Record<string, unknown>) {
  assert.equal(Object.hasOwn(value, 'compile'), false, `${label}.compile`);
  assert.equal(Object.hasOwn(value, 'save'), false, `${label}.save`);
}

function getRecord(record: Record<string, unknown>, field: string): Record<string, unknown> {
  const value = record[field];
  assert.equal(typeof value, 'object', field);
  assert.notEqual(value, null, field);
  assert.equal(Array.isArray(value), false, field);
  return value as Record<string, unknown>;
}
