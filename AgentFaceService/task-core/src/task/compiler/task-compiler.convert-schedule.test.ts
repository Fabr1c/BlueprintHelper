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

test('function-backed transform preserves FunctionAction ownership evidence', () => {
  const input = {
    kind: 'convert',
    function_operation: 'convert_function',
    transform_operation: 'blueprint_autocast',
    target_class_path: '/Script/CoreUObject.String',
    context_evidence: {
      'generic.transform.operation': 'blueprint_autocast',
      'generic.transform.source_pin_type': 'name',
      'generic.transform.target_pin_type': 'string',
    },
    args: {
      value: { kind: 'literal', value_type: 'name', value: 'DisplayName' },
    },
  };

  for (const statement of [compileTaskPlanStatement(input), compileBridgeStatement(input)]) {
    assert.equal(statement.kind, 'convert');
    assert.equal(statement.function_operation, 'convert_function');
    assert.equal(statement.transform_operation, 'blueprint_autocast');
    assert.deepEqual(statement.context_evidence, input.context_evidence);
  }
});

test('schedule expression preserves operation and latent evidence', () => {
  const input = {
    kind: 'call',
    target: 'PrintString',
    args: {
      value: {
        kind: 'schedule',
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
    assert.equal(value.schedule_operation, 'timer_delegate_node');
    assert.equal(Object.hasOwn(value, 'function_operation'), false);
    assert.equal(value.graph_latent_allowed, true);
    const nestedArgs = value.args as Record<string, Record<string, unknown>>;
    assert.equal(nestedArgs.delay.id, 'ApplyConvertSchedule_stmt_1_arg_value_delay');
  }
});

test('convert and schedule preserve context_evidence through compiler outputs', () => {
  const convertInput = {
    kind: 'convert',
    function_operation: 'convert_function',
    transform_operation: 'type_promotion',
    context_evidence: {
      type_promotion_stable_id: 'type_promotion:Add:int:real',
      type_promotion_operator: 'Add',
      type_promotion_source_pin_type: 'int',
      type_promotion_target_pin_type: 'real',
      type_promotion_result_pin_type: 'real',
    },
    args: {
      value: { kind: 'literal', value_type: 'number', value: 7 },
    },
  };

  for (const statement of [compileTaskPlanStatement(convertInput), compileBridgeStatement(convertInput)]) {
    assert.equal(statement.kind, 'convert');
    assert.equal(statement.transform_operation, 'type_promotion');
    assert.deepEqual(statement.context_evidence, convertInput.context_evidence);
  }

  const nestedScheduleInput = {
    kind: 'call',
    target: 'PrintString',
    args: {
      value: {
        kind: 'schedule',
        schedule_operation: 'latent_or_async_node',
        context_evidence: {
          graph_latent_allowed: 'false',
          schedule_operation: 'latent_or_async_node',
        },
        args: {
          delay: { kind: 'literal', value_type: 'number', value: 0.25 },
        },
      },
    },
  };

  for (const statement of [compileTaskPlanStatement(nestedScheduleInput), compileBridgeStatement(nestedScheduleInput)]) {
    const args = statement.args as Record<string, Record<string, unknown>>;
    const value = args.value;
    assert.equal(value.kind, 'schedule');
    assert.equal(value.schedule_operation, 'latent_or_async_node');
    assert.deepEqual(value.context_evidence, {
      graph_latent_allowed: 'false',
      schedule_operation: 'latent_or_async_node',
    });
  }
});

test('generic schedule statement compiles without function_operation ownership mixing', () => {
  const input = {
    kind: 'schedule',
    schedule_operation: 'timer_delegate_node',
    context_evidence: {
      schedule_action_stable_id: 'action_database:/Script/Engine.KismetSystemLibrary:/Script/BlueprintGraph.K2Node_CallFunction:sig',
      schedule_node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
      schedule_spawner_signature: 'sig',
      schedule_owner_path: '/Script/Engine.KismetSystemLibrary',
      handler_name: 'HandleTimerElapsed',
      handler_function_path: '/Game/BP/BP_Timer.HandleTimerElapsed',
      handler_source_cluster: 'BlueprintSignature',
      signature_evidence_id: 'signature:function:HandleTimerElapsed',
    },
    args: {
      time: { kind: 'literal', value_type: 'number', value: 0.25 },
    },
  };

  for (const statement of [compileTaskPlanStatement(input), compileBridgeStatement(input)]) {
    assert.equal(statement.kind, 'schedule');
    assert.equal(statement.schedule_operation, 'timer_delegate_node');
    assert.equal(Object.hasOwn(statement, 'function_operation'), false);
    assert.deepEqual(statement.context_evidence, input.context_evidence);
  }
});

test('function-backed schedule preserves FunctionAction ownership evidence', () => {
  const input = {
    kind: 'schedule',
    function_operation: 'schedule_function',
    schedule_operation: 'delay',
    graph_latent_allowed: true,
    context_evidence: {
      schedule_action_stable_id: 'action_database:/Script/Engine.KismetSystemLibrary:Delay',
      schedule_spawner_signature: 'Delay(WorldContextObject,Duration,LatentInfo)',
    },
    args: {
      duration: { kind: 'literal', value_type: 'number', value: 0.25 },
    },
  };

  for (const statement of [compileTaskPlanStatement(input), compileBridgeStatement(input)]) {
    assert.equal(statement.kind, 'schedule');
    assert.equal(statement.function_operation, 'schedule_function');
    assert.equal(statement.schedule_operation, 'delay');
    assert.equal(statement.graph_latent_allowed, true);
    assert.deepEqual(statement.context_evidence, input.context_evidence);
  }
});

test('generic schedule rejects function_operation ownership mixing', () => {
  const input = {
    kind: 'schedule',
    function_operation: 'schedule_function',
    schedule_operation: 'timer_delegate_node',
    args: {},
  };

  assert.throws(
    () => compileTaskPlanStatement(input),
    /unsupported_schedule_owner_mix/,
  );
});

test('generic schedule expression rejects function_operation ownership mixing', () => {
  const input = {
    kind: 'call',
    target: 'PrintString',
    args: {
      value: {
        kind: 'schedule',
        function_operation: 'latent_or_async_function',
        schedule_operation: 'latent_or_async_node',
        args: {},
      },
    },
  };

  assert.throws(
    () => compileTaskPlanStatement(input),
    /unsupported_schedule_owner_mix/,
  );
});
