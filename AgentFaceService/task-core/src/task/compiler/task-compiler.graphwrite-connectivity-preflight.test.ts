import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeSpec(statements: Record<string, unknown>[]) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    feature_name: 'StaticConnectivity',
    context_id: 'ctx_static_connectivity',
    target: {
      asset_path: '/Game/BH_Tests/BP_StaticConnectivity',
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
        name: 'GW_StaticConnectivity',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements,
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

test('graphwrite connectivity preflight rejects unused let value producer', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeSpec([{
      kind: 'let',
      name: 'UnusedScore',
      value: {
        kind: 'call',
        target: 'GetScorePercent',
        args: {},
      },
    }]) as never),
    /unconsumed_pure_data_node/,
  );
});

test('graphwrite connectivity preflight allows let consumed by later statement', () => {
  const plan = compileTaskSpecToTaskPlan(makeSpec([{
    kind: 'let',
    name: 'ScoreText',
    value: {
      kind: 'literal',
      value_type: 'string',
      value: 'ok',
    },
  }, {
    kind: 'call',
    target: 'PrintString',
    args: {
      InString: {
        kind: 'get',
        name: 'ScoreText',
      },
    },
  }]) as never);

  assert.equal(plan.steps.filter((step) => (step as Record<string, unknown>).capability === 'graph_write').length, 1);
});

test('graphwrite connectivity preflight sees nested branch consumption', () => {
  const plan = compileTaskSpecToTaskPlan(makeSpec([{
    kind: 'let',
    name: 'BranchMessage',
    value: {
      kind: 'literal',
      value_type: 'string',
      value: 'branch',
    },
  }, {
    kind: 'control',
    control: 'branch',
    condition: {
      kind: 'literal',
      value_type: 'bool',
      value: true,
    },
    then: [{
      kind: 'call',
      target: 'PrintString',
      args: {
        InString: {
          kind: 'get',
          name: 'BranchMessage',
        },
      },
    }],
  }]) as never);

  assert.equal(plan.steps.filter((step) => (step as Record<string, unknown>).capability === 'graph_write').length, 1);
});
