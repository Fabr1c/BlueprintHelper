import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan, taskPlanToAppendBridgePayload } from './task-compiler.js';

function makeCreateSpec(statement: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_create_statement_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'CreateFeatureTs',
    target: {
      asset_path: '/Game/BP/BP_Create',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_CreateFeatureTs',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'ApplyCreate',
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

function compileTaskPlanStatement(statement: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeCreateSpec(statement) as never);
  const graphWriteStep = taskPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown> | undefined;
  assert.ok(graphWriteStep);
  const write = graphWriteStep.write as { ops: Array<{ body: { statements: Record<string, unknown>[] } }> };
  return write.ops[0].body.statements[0];
}

function compileBridgeStatement(statement: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeCreateSpec(statement) as never);
  const bridgePayload = taskPlanToAppendBridgePayload(taskPlan, true) as unknown as Record<string, unknown>;
  const logicSpec = bridgePayload.logic_spec as { statements: Record<string, unknown>[] };
  return logicSpec.statements[0];
}

test('create statement preserves broad-create evidence', () => {
  const statement = compileTaskPlanStatement({
    kind: 'create',
    create_operation: 'make_array',
    pin_type: { category: 'int' },
    args: {
      item: { kind: 'literal', value_type: 'number', value: 42 },
    },
  });

  assert.equal(statement.kind, 'create');
  assert.equal(statement.create_operation, 'make_array');
  assert.deepEqual(statement.pin_type, { category: 'int' });
  const args = statement.args as Record<string, Record<string, unknown>>;
  assert.equal(args.item.kind, 'literal');
  assert.equal(args.item.id, 'ApplyCreate_stmt_1_arg_item');
});

test('create expression is preserved inside value expressions', () => {
  const statement = compileTaskPlanStatement({
    kind: 'set',
    target: 'CreatedActor',
    value: {
      kind: 'create',
      create_operation: 'spawn_actor',
      class_path: '/Script/Engine.Actor',
      args: {
        transform: { kind: 'literal', value_type: 'string', value: 'Identity' },
      },
    },
  });

  const value = statement.value as Record<string, unknown>;
  assert.equal(value.kind, 'create');
  assert.equal(value.create_operation, 'spawn_actor');
  assert.equal(value.class_path, '/Script/Engine.Actor');
  const args = value.args as Record<string, Record<string, unknown>>;
  assert.equal(args.transform.id, 'ApplyCreate_stmt_1_value_transform');
});

test('append bridge accepts create statement lowering', () => {
  const statement = compileBridgeStatement({
    kind: 'create',
    create_operation: 'construct_object',
    class_path: '/Script/CoreUObject.Object',
  });

  assert.equal(statement.kind, 'create');
  assert.equal(statement.create_operation, 'construct_object');
  assert.equal(statement.class_path, '/Script/CoreUObject.Object');
});

test('function-backed create preserves FunctionAction ownership evidence', () => {
  const input = {
    kind: 'create',
    create_operation: 'function_backed_create',
    function_operation: 'create_function',
    target: 'CreateWidget',
    class_path: '/Script/UMG.UserWidget',
    args: {},
  };

  for (const statement of [compileTaskPlanStatement(input), compileBridgeStatement(input)]) {
    assert.equal(statement.kind, 'create');
    assert.equal(statement.create_operation, 'function_backed_create');
    assert.equal(statement.function_operation, 'create_function');
    assert.equal(statement.target, 'CreateWidget');
    assert.equal(statement.class_path, '/Script/UMG.UserWidget');
  }
});

test('function-backed create operation derives create_function ownership', () => {
  const statement = compileTaskPlanStatement({
    kind: 'create',
    create_operation: 'async_action',
    target: 'AsyncLoadPrimaryAsset',
    args: {},
  });

  assert.equal(statement.kind, 'create');
  assert.equal(statement.create_operation, 'async_action');
  assert.equal(statement.function_operation, 'create_function');
  assert.equal(statement.target, 'AsyncLoadPrimaryAsset');
});

test('function-backed create requires a callable target', () => {
  assert.throws(
    () => compileTaskPlanStatement({
      kind: 'create',
      create_operation: 'function_backed_create',
      function_operation: 'create_function',
      class_path: '/Script/UMG.UserWidget',
      args: {},
    }),
    /missing_create_function_target/,
  );
});

test('generic create rejects FunctionAction ownership mixing', () => {
  assert.throws(
    () => compileTaskPlanStatement({
      kind: 'create',
      create_operation: 'spawn_actor',
      function_operation: 'create_function',
      class_path: '/Script/Engine.Actor',
    }),
    /unsupported_create_owner_mix/,
  );
});

test('create requires create_operation', () => {
  assert.throws(
    () => compileTaskPlanStatement({
      kind: 'create',
      class_path: '/Script/Engine.Actor',
    }),
    /create_operation/,
  );
});
