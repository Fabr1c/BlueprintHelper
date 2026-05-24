import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan, taskPlanToAppendBridgePayload } from './task-compiler.js';

function makeConvertScheduleSpec(statement: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_convert_schedule_statement_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'ConvertScheduleFeatureTs',
    target: {
      asset_path: '/Game/BP/BP_ConvertSchedule',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_ConvertScheduleFeatureTs',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'ApplyConvertSchedule',
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
  const taskPlan = compileTaskSpecToTaskPlan(makeConvertScheduleSpec(statement) as never);
  const graphWriteStep = taskPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown> | undefined;
  assert.ok(graphWriteStep);
  const write = graphWriteStep.write as { ops: Array<{ body: { statements: Record<string, unknown>[] } }> };
  return write.ops[0].body.statements[0];
}

function compileBridgeStatement(statement: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeConvertScheduleSpec(statement) as never);
  const bridgePayload = taskPlanToAppendBridgePayload(taskPlan, true) as unknown as Record<string, unknown>;
  const logicSpec = bridgePayload.logic_spec as { statements: Record<string, unknown>[] };
  return logicSpec.statements[0];
}

test('convert statement preserves generic transform evidence', () => {
  const input = {
    kind: 'convert',
    function_operation: 'convert_function',
    transform_operation: 'dynamic_cast',
    target_class_path: '/Script/Engine.Actor',
    args: {
      value: { kind: 'literal', value_type: 'object', value: 'Self' },
    },
  };

  for (const statement of [compileTaskPlanStatement(input), compileBridgeStatement(input)]) {
    assert.equal(statement.kind, 'convert');
    assert.equal(statement.function_operation, 'convert_function');
    assert.equal(statement.transform_operation, 'dynamic_cast');
    assert.equal(statement.target_class_path, '/Script/Engine.Actor');
    const args = statement.args as Record<string, Record<string, unknown>>;
    assert.equal(args.value.id, 'ApplyConvertSchedule_stmt_1_arg_value');
  }
});

test('schedule expression preserves operation and latent evidence', () => {
  const input = {
    kind: 'call',
    target: 'PrintString',
    args: {
      value: {
        kind: 'schedule',
        function_operation: 'schedule_function',
        schedule_operation: 'timer_delegate_node',
        graph_latent_allowed: true,
        args: {
          delay: { kind: 'literal', value_type: 'number', value: 0.25 },
        },
      },
    },
  };

  for (const statement of [compileTaskPlanStatement(input), compileBridgeStatement(input)]) {
    const args = statement.args as Record<string, Record<string, unknown>>;
    const value = args.value;
    assert.equal(value.kind, 'schedule');
    assert.equal(value.function_operation, 'schedule_function');
    assert.equal(value.schedule_operation, 'timer_delegate_node');
    assert.equal(value.graph_latent_allowed, true);
    const nestedArgs = value.args as Record<string, Record<string, unknown>>;
    assert.equal(nestedArgs.delay.id, 'ApplyConvertSchedule_stmt_1_arg_value_delay');
  }
});
