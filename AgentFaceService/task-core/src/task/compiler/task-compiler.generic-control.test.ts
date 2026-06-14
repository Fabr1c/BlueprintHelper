import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';
import { taskPlanToAppendBridgePayload } from './graphwrite/graphwrite-task-type-compiler.js';

function makeControlSpec(statement: Record<string, unknown>) {
  return makeControlSequenceSpec([statement]);
}

function makeControlSequenceSpec(statements: Record<string, unknown>[]) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_generic_control_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'GenericControlTs',
    target: {
      asset_path: '/Game/BP/BP_GenericControl',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_GenericControlTs',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'ApplyGenericControl',
        body: {
          schema: 'BlueprintLogicSpec.v2',
          statements,
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

function compileControlSequence(statements: Record<string, unknown>[]) {
  return compileTaskSpecToTaskPlan(makeControlSequenceSpec(statements) as never);
}

function compileTaskPlanStatement(statement: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeControlSpec(statement) as never);
  const graphWriteStep = taskPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown> | undefined;
  assert.ok(graphWriteStep);
  const write = graphWriteStep.write as { ops: Array<{ body: { statements: Record<string, unknown>[] } }> };
  return write.ops[0].body.statements[0];
}

function compileBridgeStatement(statement: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeControlSpec(statement) as never);
  const bridgePayload = taskPlanToAppendBridgePayload(taskPlan, true) as unknown as Record<string, unknown>;
  const logicSpec = bridgePayload.logic_spec as { statements: Record<string, unknown>[] };
  return logicSpec.statements[0];
}

test('switch control lowers to UE generic control evidence', () => {
  const input = {
    kind: 'control',
    control: 'switch_int',
    case_values: [0, 1],
    context_evidence: {
      'generic.control.default_policy': 'has_default',
    },
  };

  for (const statement of [compileTaskPlanStatement(input), compileBridgeStatement(input)]) {
    assert.equal(statement.kind, 'control');
    assert.equal(statement.control, 'switch_int');
    assert.equal(statement.control_operation, 'switch_int');
    assert.deepEqual(statement.context_evidence, {
      'generic.control.default_policy': 'has_default',
      'generic.control.operation': 'switch_int',
      'generic.control.case_values': '0,1',
    });
  }
});

test('switch enum requires enum path evidence', () => {
  assert.throws(
    () => compileTaskPlanStatement({
      kind: 'control',
      control: 'switch_enum',
      case_values: ['Idle', 'Running'],
    }),
    /generic\.control\.enum_path/,
  );
});

test('multi gate control lowers dynamic output count evidence', () => {
  const statement = compileTaskPlanStatement({
    kind: 'control',
    control: 'multi_gate',
    dynamic_output_count: 3,
  });

  assert.equal(statement.kind, 'control');
  assert.equal(statement.control_operation, 'multi_gate');
  assert.deepEqual(statement.context_evidence, {
    'generic.control.operation': 'multi_gate',
    'generic.control.dynamic_output_count': '3',
  });
});

test('standard macro control lowers macro evidence', () => {
  const statement = compileBridgeStatement({
    kind: 'control',
    control: 'for_loop',
    macro_graph_path: '/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForLoop',
    macro_pin_shape_snapshot: 'Exec,LoopBody,Completed,Index',
  });

  assert.equal(statement.kind, 'control');
  assert.equal(statement.control_operation, 'for_loop');
  assert.deepEqual(statement.context_evidence, {
    'generic.control.operation': 'for_loop',
    'generic.macro.graph_path': '/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForLoop',
    'generic.macro.pin_shape_snapshot': 'Exec,LoopBody,Completed,Index',
  });
});

test('return control preserves named output map for Bridge lowering', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeControlSpec({
    kind: 'control',
    control: 'return',
    outputs: {
      bCompleted: { kind: 'get', target: 'bCompleted' },
      bIsNewRecord: { kind: 'get', target: 'bIsNewRecord' },
    },
  }) as never);
  const bridgePayload = taskPlanToAppendBridgePayload(taskPlan, true) as unknown as Record<string, unknown>;
  const logicSpec = bridgePayload.logic_spec as { statements?: Array<{ outputs?: Record<string, unknown> }> };
  const returnOutputs = logicSpec.statements?.[0]?.outputs as Record<string, unknown> | undefined;

  assert.deepEqual(returnOutputs, {
    bCompleted: { kind: 'get', target: 'bCompleted' },
    bIsNewRecord: { kind: 'get', target: 'bIsNewRecord' },
  });
});

test('return control rejects removed single value shape', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeControlSpec({
    kind: 'control',
    control: 'return',
    value: { kind: 'literal', value_type: 'boolean', value: true },
  }) as never);

  assert.throws(
    () => taskPlanToAppendBridgePayload(taskPlan, true),
    (error: unknown) => {
      assert.equal(error instanceof Error, true);
      assert.equal((error as { code?: string }).code, 'return_value_shape_removed');
      return true;
    },
  );
});

test('generic control rejects implicit linear continuation', () => {
  assert.throws(
    () => compileControlSequence([
      {
        kind: 'control',
        control: 'switch_int',
        case_values: [0, 1],
        context_evidence: {
          'generic.control.default_policy': 'has_default',
        },
      },
      {
        kind: 'call',
        target: 'PrintString',
        args: {
          InString: { kind: 'literal', value_type: 'string', value: 'after switch' },
        },
      },
    ]),
    /unsupported_control_continuation/,
  );
});
