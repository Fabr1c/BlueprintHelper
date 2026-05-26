import { strict as assert } from 'node:assert';
import test from 'node:test';

import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeDelegateSpec(statements: Array<Record<string, unknown>>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_event_delegate_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'EventDelegateFeatureTs',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_EventDelegateFeatureTs',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'ApplyDelegates',
        body: {
          schema: 'BlueprintLogicSpec.v1',
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

function compileStatements(statements: Array<Record<string, unknown>>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeDelegateSpec(statements) as never);
  const graphWriteStep = taskPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown> | undefined;
  assert.ok(graphWriteStep);
  const write = graphWriteStep.write as { ops: Array<{ body: { statements: Record<string, unknown>[] } }> };
  return write.ops[0].body.statements;
}

test('append_new_owned_graph splits multiple entries into single-op graph_write steps', () => {
  const singleEntrySpec = makeDelegateSpec([{
    kind: 'delegate.bind',
    target: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
    handler: 'HandleOverlap',
  }]) as Record<string, unknown>;
  const behavior = singleEntrySpec.behavior as Record<string, unknown>;
  behavior.entries = [
    {
      entry_type: 'custom_event',
      name: 'HandleOverlap',
      inputs: [
        {
          name: 'OtherActor',
          pin_type: {
            category: 'object',
            object_path: '/Script/Engine.Actor',
          },
        },
      ],
      body: {
        schema: 'BlueprintLogicSpec.v1',
        statements: [{
          kind: 'call',
          target: 'PrintString',
        }],
      },
    },
    {
      entry_type: 'custom_event',
      name: 'ApplyDelegates',
      body: {
        schema: 'BlueprintLogicSpec.v1',
        statements: [{
          kind: 'delegate.bind',
          target: 'TriggerBox',
          delegate: 'OnComponentBeginOverlap',
          handler: 'HandleOverlap',
        }],
      },
    },
  ];

  const taskPlan = compileTaskSpecToTaskPlan(singleEntrySpec as never);
  const signatureSteps = taskPlan.steps.filter((step) => (step as Record<string, unknown>).capability === 'blueprint_signature') as Array<Record<string, unknown>>;
  const graphWriteSteps = taskPlan.steps.filter((step) => (step as Record<string, unknown>).capability === 'graph_write') as Array<Record<string, unknown>>;
  const firstSignatureOp = ((signatureSteps[0].write as { ops: Array<Record<string, unknown>> }).ops[0]);
  assert.deepEqual(firstSignatureOp.inputs, [
    {
      name: 'OtherActor',
      pin_type: {
        category: 'object',
        object_path: '/Script/Engine.Actor',
      },
    },
  ]);
  assert.equal(graphWriteSteps.length, 2);
  assert.deepEqual(graphWriteSteps.map((step) => (step.write as { ops: unknown[] }).ops.length), [1, 1]);
  assert.deepEqual(graphWriteSteps.map((step) => step.step_id), ['step_003', 'step_004']);
  assert.deepEqual(graphWriteSteps[0].depends_on, ['step_001', 'step_002']);
  assert.deepEqual(graphWriteSteps[1].depends_on, ['step_001', 'step_002', 'step_003']);
});

test('component_bound_event public statement is preserved as canonical internal component_bound_event', () => {
  const [statement] = compileStatements([{
    kind: 'component_bound_event',
    component: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
    handler: 'BH_HandleSmokeOverlap',
  }]);

  assert.equal(statement.kind, 'component_bound_event');
  assert.equal(statement.component, 'TriggerBox');
  assert.equal(statement.delegate, 'OnComponentBeginOverlap');
  assert.equal(statement.handler, 'BH_HandleSmokeOverlap');
  assert.equal(Object.hasOwn(statement, 'delegate_operation'), false);
});

test('delegate.bind public statement lowers to canonical delegate bind operation', () => {
  const [statement] = compileStatements([{
    kind: 'delegate.bind',
    target: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
    handler: 'BH_HandleSmokeOverlap',
  }]);

  assert.equal(statement.kind, 'delegate');
  assert.equal(statement.delegate_operation, 'bind');
  assert.equal(statement.target, 'TriggerBox');
  assert.equal(statement.delegate, 'OnComponentBeginOverlap');
  assert.equal(statement.handler, 'BH_HandleSmokeOverlap');
});

test('delegate.assign public statement lowers to canonical delegate assign operation', () => {
  const [statement] = compileStatements([{
    kind: 'delegate.assign',
    target: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
    handler: 'BH_HandleSmokeOverlap',
  }]);

  assert.equal(statement.kind, 'delegate');
  assert.equal(statement.delegate_operation, 'assign');
  assert.equal(statement.target, 'TriggerBox');
  assert.equal(statement.delegate, 'OnComponentBeginOverlap');
  assert.equal(statement.handler, 'BH_HandleSmokeOverlap');
});

test('delegate.unbind public statement lowers to canonical single delegate unbind operation', () => {
  const [statement] = compileStatements([{
    kind: 'delegate.unbind',
    target: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
    handler: 'BH_HandleSmokeOverlap',
  }]);

  assert.equal(statement.kind, 'delegate');
  assert.equal(statement.delegate_operation, 'unbind');
  assert.equal(statement.unbind_mode, 'single');
  assert.equal(statement.target, 'TriggerBox');
  assert.equal(statement.delegate, 'OnComponentBeginOverlap');
  assert.equal(statement.handler, 'BH_HandleSmokeOverlap');
});

test('delegate.unbind_all public statement lowers to canonical delegate clear operation', () => {
  const [statement] = compileStatements([{
    kind: 'delegate.unbind_all',
    target: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
  }]);

  assert.equal(statement.kind, 'delegate');
  assert.equal(statement.delegate_operation, 'clear');
  assert.equal(statement.unbind_mode, 'all');
  assert.equal(statement.target, 'TriggerBox');
  assert.equal(statement.delegate, 'OnComponentBeginOverlap');
});

test('delegate.call public statement lowers to canonical delegate call operation', () => {
  const [statement] = compileStatements([{
    kind: 'delegate.call',
    target: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
    args: {
      OtherActor: {
        kind: 'literal',
        value: 'None',
      },
    },
  }]);

  assert.equal(statement.kind, 'delegate');
  assert.equal(statement.delegate_operation, 'call');
  assert.equal(statement.target, 'TriggerBox');
  assert.equal(statement.delegate, 'OnComponentBeginOverlap');
  assert.deepEqual((statement.args as Record<string, Record<string, unknown>>).OtherActor.kind, 'literal');
  assert.deepEqual((statement.args as Record<string, Record<string, unknown>>).OtherActor.value, 'None');
  assert.ok((statement.args as Record<string, Record<string, unknown>>).OtherActor.id);
});

test('Agent-authored internal delegate statement is rejected before lowering', () => {
  assert.throws(
    () => compileStatements([{
      kind: 'delegate',
      delegate_operation: 'bind',
      target: 'TriggerBox',
      delegate: 'OnComponentBeginOverlap',
      handler: 'BH_HandleSmokeOverlap',
    }]),
    (err: unknown) => err instanceof TaskSpecCompileError
      && err.code === 'unsupported_statement_kind'
      && err.issues.some((issue) => issue.message.includes('Use component_bound_event or delegate.bind')),
  );
});
