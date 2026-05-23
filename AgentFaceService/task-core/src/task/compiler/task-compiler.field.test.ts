import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeFieldSpec(statement: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_field_statement_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'FieldFeatureTs',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_FieldFeatureTs',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'ApplyFields',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [statement],
        },
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}

function compileFirstStatement(statement: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeFieldSpec(statement) as never);
  const graphWriteStep = taskPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown> | undefined;
  assert.ok(graphWriteStep);
  const write = graphWriteStep.write as { ops: Array<{ body: { statements: Record<string, unknown>[] } }> };
  return write.ops[0].body.statements[0];
}

test('set lowers to field variable set', () => {
  const statement = compileFirstStatement({
    kind: 'set',
    target: 'bIsClosed',
    value: { kind: 'literal', value_type: 'bool', value: true },
  });

  assert.equal(statement.kind, 'field');
  assert.equal(statement.field_operation, 'set');
  assert.equal(statement.field_scope, 'variable');
  assert.equal(statement.target, 'bIsClosed');
});

test('set_property lowers to field property set', () => {
  const statement = compileFirstStatement({
    kind: 'set_property',
    target: 'DoorMesh',
    property_path: 'RelativeRotation',
    value: { kind: 'literal', value_type: 'string', value: 'rot' },
  });

  assert.equal(statement.kind, 'field');
  assert.equal(statement.field_operation, 'set');
  assert.equal(statement.field_scope, 'property_path');
  assert.equal(statement.target, 'DoorMesh');
  assert.equal(statement.property_path, 'RelativeRotation');
});

test('get_property lowers to field property get inside a value expression', () => {
  const statement = compileFirstStatement({
    kind: 'set',
    target: 'YawCache',
    value: {
      kind: 'get_property',
      target: 'DoorMesh',
      property_path: 'RelativeRotation.Yaw',
    },
  });

  const value = statement.value as Record<string, unknown>;
  assert.equal(value.kind, 'field');
  assert.equal(value.field_operation, 'get');
  assert.equal(value.field_scope, 'property_path');
  assert.equal(value.target, 'DoorMesh');
  assert.equal(value.property_path, 'RelativeRotation.Yaw');
});
