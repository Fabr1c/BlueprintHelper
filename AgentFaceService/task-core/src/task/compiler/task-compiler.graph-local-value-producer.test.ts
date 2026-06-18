import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';
import { taskPlanToAppendBridgePayload } from './graphwrite/graphwrite-task-type-compiler.js';

function makeGraphSpec(statements: Record<string, unknown>[]) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: {
      asset_path: '/Game/BH_Tests/BP_GraphWriteGraphLocalValueProducer',
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
        name: 'GW_GraphLocalValueProducer',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements,
        },
      }],
    },
  };
}

function compileBridgeStatements(statements: Record<string, unknown>[]) {
  const taskPlan = compileTaskSpecToTaskPlan(makeGraphSpec(statements) as never);
  const payload = taskPlanToAppendBridgePayload(taskPlan, true);
  return payload.logic_spec.statements as Record<string, unknown>[];
}

test('call statement preserves result_symbol for UE graph-local value production', () => {
  const statements = compileBridgeStatements([{
    kind: 'call',
    target: '/Script/Engine.KismetSystemLibrary:K2_SetTimer',
    value_type: 'TimerHandle',
    result_symbol: 'LoopTimerHandle',
    args: {
      FunctionName: { kind: 'literal', value_type: 'string', value: 'HandlePulse' },
      Time: { kind: 'literal', value_type: 'float', value: 0.75 },
      bLooping: { kind: 'literal', value_type: 'bool', value: true },
    },
  }, {
    kind: 'field',
    field_operation: 'set',
    field_scope: 'variable',
    target: 'LoopDoorTimerHandle',
    value: { kind: 'get', name: 'LoopTimerHandle' },
  }]);

  assert.equal(statements[0].kind, 'call');
  assert.equal(statements[0].result_symbol, 'LoopTimerHandle');
  assert.deepEqual(statements[1].value, {
    kind: 'get',
    name: 'LoopTimerHandle',
    id: 'GW_GraphLocalValueProducer_stmt_2_value',
  });
});

test('schedule statement preserves result_symbol and projected schedule evidence', () => {
  const statements = compileBridgeStatements([{
    kind: 'schedule',
    schedule_operation: 'timer_delegate_node',
    target: 'K2_SetTimerDelegate',
    value_type: 'TimerHandle',
    result_symbol: 'LoopTimerHandle',
    context_evidence: {
      schedule_action_stable_id: 'action_database:/Script/Engine.KismetSystemLibrary:/Script/BlueprintGraph.K2Node_CallFunction:(FieldName="/Script/Engine.KismetSystemLibrary:K2_SetTimerDelegate",NodeName="/Script/BlueprintGraph.K2Node_CallFunction")',
      schedule_node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
      schedule_spawner_signature: '(FieldName="/Script/Engine.KismetSystemLibrary:K2_SetTimerDelegate",NodeName="/Script/BlueprintGraph.K2Node_CallFunction")',
      schedule_owner_path: '/Script/Engine.KismetSystemLibrary',
      schedule_query: 'K2_SetTimerDelegate',
    },
    args: {
      Time: { kind: 'literal', value_type: 'float', value: 0.75 },
      bLooping: { kind: 'literal', value_type: 'bool', value: true },
    },
  }]);

  const statement = statements[0];
  assert.equal(statement.kind, 'schedule');
  assert.equal(statement.result_symbol, 'LoopTimerHandle');
  assert.equal((statement.context_evidence as Record<string, unknown>).schedule_query, 'K2_SetTimerDelegate');
});

test('pure call expression remains valid when used as a value expression', () => {
  const statements = compileBridgeStatements([{
    kind: 'field',
    field_operation: 'set',
    field_scope: 'variable',
    target: 'HealthPercent',
    value: {
      kind: 'call',
      target: 'GetHealthPercent',
      args: {
        Target: { kind: 'get', target: 'DoorPanel' },
      },
    },
  }]);

  const value = statements[0].value as Record<string, unknown>;
  assert.equal(value.kind, 'call');
  assert.equal(value.target, 'GetHealthPercent');
});

test('impure call expression is rejected when marked impure and used as a value expression', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec([{
      kind: 'field',
      field_operation: 'set',
      field_scope: 'variable',
      target: 'LoopDoorTimerHandle',
      value: {
        kind: 'call',
        target: '/Script/Engine.KismetSystemLibrary:K2_SetTimer',
        is_pure: false,
        args: {
          FunctionName: { kind: 'literal', value_type: 'string', value: 'HandlePulse' },
          Time: { kind: 'literal', value_type: 'float', value: 0.75 },
          bLooping: { kind: 'literal', value_type: 'bool', value: true },
        },
      },
    }]) as never),
    /impure call expressions require a statement result_symbol/,
  );
});

test('call statement result_symbol requires explicit output type evidence', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec([{
      kind: 'call',
      target: '/Script/Engine.KismetSystemLibrary:PrintString',
      result_symbol: 'VoidResult',
      args: {
        InString: { kind: 'literal', value_type: 'string', value: 'Pulse' },
      },
    }]) as never),
    /result_symbol requires explicit result output evidence/,
  );
});

test('schedule statement result_symbol requires explicit output type evidence', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec([{
      kind: 'schedule',
      schedule_operation: 'latent_or_async_node',
      result_symbol: 'LatentResult',
      args: {
        Duration: { kind: 'literal', value_type: 'float', value: 0.25 },
      },
    }]) as never),
    /result_symbol requires explicit result output evidence/,
  );
});

test('impure schedule expression is rejected when used as a value expression', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec([{
      kind: 'field',
      field_operation: 'set',
      field_scope: 'variable',
      target: 'LoopDoorTimerHandle',
      value: {
        kind: 'schedule',
        schedule_operation: 'timer_delegate_node',
        target: 'K2_SetTimerDelegate',
        args: {
          Time: { kind: 'literal', value_type: 'float', value: 0.75 },
          bLooping: { kind: 'literal', value_type: 'bool', value: true },
        },
      },
    }]) as never),
    /impure schedule expressions require a statement result_symbol/,
  );
});
