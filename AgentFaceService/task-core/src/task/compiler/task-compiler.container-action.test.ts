import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan, taskPlanToAppendBridgePayload } from './task-compiler.js';

function makeContainerSpec(statement: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: {
      asset_path: '/Game/BH_Tests/BP_GraphWriteContainer',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'GW_ContainerSmoke',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [statement],
        },
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}

test('container_action array add lowers as first-class GraphWrite statement', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeContainerSpec({
    kind: 'container_action',
    container_kind: 'array',
    container_operation: 'add',
    target: { kind: 'get', name: 'Items' },
    item: { kind: 'literal', value_type: 'number', value: 7 },
    element_type: 'int',
  }) as never);

  const payload = taskPlanToAppendBridgePayload(taskPlan, true);
  const statement = payload.logic_spec.statements[0] as Record<string, unknown>;

  assert.equal(statement.kind, 'container_action');
  assert.equal(statement.container_kind, 'array');
  assert.equal(statement.container_operation, 'add');
  assert.equal(statement.element_type, 'int');
  assert.deepEqual(statement.target, { kind: 'get', name: 'Items', id: 'GW_ContainerSmoke_stmt_1_target' });
  assert.deepEqual(statement.item, { kind: 'literal', value_type: 'number', value: 7, id: 'GW_ContainerSmoke_stmt_1_item' });
});

test('container_action map contains lowers as first-class GraphWrite expression', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeContainerSpec({
    kind: 'let',
    name: 'bHasScore',
    value: {
      kind: 'container_action',
      container_kind: 'map',
      container_operation: 'contains',
      target: { kind: 'get', name: 'Scores' },
      key: { kind: 'literal', value_type: 'string', value: 'PlayerA' },
      key_type: 'string',
      value_type: 'int',
    },
  }) as never);

  const payload = taskPlanToAppendBridgePayload(taskPlan, true);
  const statement = payload.logic_spec.statements[0] as Record<string, unknown>;
  const value = statement.value as Record<string, unknown>;

  assert.equal(statement.kind, 'let');
  assert.equal(value.kind, 'container_action');
  assert.equal(value.container_kind, 'map');
  assert.equal(value.container_operation, 'contains');
  assert.equal(value.key_type, 'string');
  assert.equal(value.value_type, 'int');
  assert.deepEqual(value.target, { kind: 'get', name: 'Scores', id: 'GW_ContainerSmoke_stmt_1_value_target' });
  assert.deepEqual(value.key, { kind: 'literal', value_type: 'string', value: 'PlayerA', id: 'GW_ContainerSmoke_stmt_1_value_key' });
});

test('container_action rejects unsupported foreach in V1', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeContainerSpec({
      kind: 'container_action',
      container_kind: 'array',
      container_operation: 'foreach',
      target: { kind: 'get', name: 'Items' },
      element_type: 'int',
    }) as never),
    /Unsupported container_operation/,
  );
});

test('container_action rejects missing required roles', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeContainerSpec({
      kind: 'container_action',
      container_kind: 'map',
      container_operation: 'add',
      target: { kind: 'get', name: 'Scores' },
      key: { kind: 'literal', value_type: 'string', value: 'PlayerA' },
      key_type: 'string',
      value_type: 'int',
    }) as never),
    /requires value/,
  );
});

test('container_action target string shorthand does not rewrite literal key strings', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeContainerSpec({
    kind: 'let',
    name: 'bHasScore',
    value: {
      kind: 'container_action',
      container_kind: 'map',
      container_operation: 'contains',
      target: 'Scores',
      key: 'PlayerA',
      key_type: 'string',
      value_type: 'int',
    },
  }) as never);

  const payload = taskPlanToAppendBridgePayload(taskPlan, true);
  const statement = payload.logic_spec.statements[0] as Record<string, unknown>;
  const value = statement.value as Record<string, unknown>;

  assert.deepEqual(value.target, { kind: 'get', name: 'Scores', id: 'GW_ContainerSmoke_stmt_1_value_target' });
  assert.equal(value.key, 'PlayerA');
});

test('container_action query statement does not lower to plain call and preserves result_symbol', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeContainerSpec({
    kind: 'container_action',
    container_kind: 'set',
    container_operation: 'contains',
    target: { kind: 'get', name: 'Tags' },
    item: { kind: 'literal', value_type: 'string', value: 'Ready' },
    element_type: 'string',
    result_symbol: 'bHasReady',
  }) as never);

  const payload = taskPlanToAppendBridgePayload(taskPlan, true);
  const statement = payload.logic_spec.statements[0] as Record<string, unknown>;

  assert.equal(statement.kind, 'container_action');
  assert.notEqual(statement.kind, 'call');
  assert.equal(statement.result_symbol, 'bHasReady');
  assert.equal(statement.element_type, 'string');
});

test('container_action rejects result_symbol on mutating operations', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeContainerSpec({
      kind: 'container_action',
      container_kind: 'array',
      container_operation: 'add',
      target: { kind: 'get', name: 'Items' },
      item: { kind: 'literal', value_type: 'number', value: 7 },
      element_type: 'int',
      result_symbol: 'AddedIndex',
    }) as never),
    /result_symbol is only supported for query container_action operations/,
  );
});

test('container_action rejects result_symbol when no single result output exists', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeContainerSpec({
      kind: 'container_action',
      container_kind: 'map',
      container_operation: 'get_key_value_by_index',
      target: { kind: 'get', name: 'Scores' },
      index: { kind: 'literal', value_type: 'number', value: 0 },
      key_type: 'string',
      value_type: 'int',
      result_symbol: 'PairValue',
    }) as never),
    /single result output/,
  );
});
