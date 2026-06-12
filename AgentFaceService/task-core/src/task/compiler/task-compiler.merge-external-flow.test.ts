import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

const externalAnchor = {
  schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
  asset_path: '/Game/BP/BP_Door',
  graph_name: 'EventGraph',
  node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
  node_class: '/Script/BlueprintGraph.K2Node_CustomEvent',
  pin_name: 'then',
  pin_direction: 'output',
  semantic_role: 'exec_boundary',
  fingerprint: 'boundaryfp',
};

const logicJsonAnchorSelector = {
  schema: 'BlueprintHelper.LogicJsonAnchorSelector.v1',
  asset_path: '/Game/BP/BP_Door',
  graph_name: 'EventGraph',
  entry_name: 'ReloadTips',
  node_ref: 'nodes[7]',
  pin_ref: 'then',
};

const externalExecLinkAnchor = {
  anchor_type: 'external_link',
  anchor_ref: 'xlink:v1:e:aaaaaaaa.then>bbbbbbbb.execute#execfp',
};

function makeMergeExternalFlowSpec(overrides: {
  scopePolicy?: Record<string, unknown>;
  merge?: Record<string, unknown>;
  behavior?: Record<string, unknown>;
} = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_merge_external_flow_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'MergeExternalFlowTs',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
      external_mutation_policy: {
        strategy: 'merge_external_flow',
        allowed_mutations: ['exec_boundary_link'],
      },
      ...overrides.scopePolicy,
    },
    behavior: {
      graph_strategy: 'merge_external_flow',
      external_merges: [{
        kind: 'insert_external_flow',
        insert_strategy: 'append_after',
        anchor: externalAnchor,
        inserted: {
          body: {
            schema: 'BlueprintLogicSpec.v1',
            statements: [{ kind: 'call', target: 'PrintString' }],
          },
        },
        ...overrides.merge,
      }],
      ...overrides.behavior,
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

function compileMergeExternalStep(overrides?: Parameters<typeof makeMergeExternalFlowSpec>[0]) {
  const taskPlan = compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec(overrides) as never);
  const step = taskPlan.steps.find((candidate) => (
    (candidate as Record<string, unknown>).capability === 'graph_write'
  )) as Record<string, unknown> | undefined;
  assert.ok(step);
  return step;
}

test('merge_external_flow lowers to external graph edit with explicit mutation policy', () => {
  const step = compileMergeExternalStep();
  const write = step.write as { strategy: string; ops: Array<Record<string, unknown>> };

  assert.equal(write.strategy, 'external_graph_edit');
  assert.equal(write.ops.length, 1);
  assert.equal(write.ops[0]?.op, 'insert_external_flow');
  assert.equal(write.ops[0]?.insert_strategy, 'append_after');
  assert.deepEqual(write.ops[0]?.anchor, externalAnchor);
  assert.deepEqual(step.constraints, {
    allow_modify_user_nodes: false,
    ownership_scope: 'external_user_authored',
    external_mutation_policy: {
      strategy: 'merge_external_flow',
      allowed_mutations: ['exec_boundary_link'],
    },
  });
});

test('merge_external_flow accepts LogicJson node_ref selector as authoring anchor', () => {
  const step = compileMergeExternalStep({
    merge: {
      anchor: logicJsonAnchorSelector,
    },
  });
  const write = step.write as { strategy: string; ops: Array<Record<string, unknown>> };

  assert.equal(write.strategy, 'external_graph_edit');
  assert.deepEqual(write.ops[0]?.anchor, logicJsonAnchorSelector);
});

test('merge_external_flow lowers external exec link compact anchor for insert_between', () => {
  const step = compileMergeExternalStep({
    merge: {
      insert_strategy: 'insert_between',
      anchor: externalExecLinkAnchor,
    },
  });
  const write = step.write as { strategy: string; ops: Array<Record<string, unknown>> };

  assert.equal(write.strategy, 'external_graph_edit');
  assert.equal(write.ops[0]?.insert_strategy, 'insert_between');
  assert.deepEqual(write.ops[0]?.anchor, externalExecLinkAnchor);
});

test('merge_external_flow normalizes LogicJson selector graph alias', () => {
  const step = compileMergeExternalStep({
    merge: {
      anchor: {
        schema: 'BlueprintHelper.LogicJsonAnchorSelector.v1',
        asset_path: '/Game/BP/BP_Door',
        graph: 'EventGraph',
        entry_name: 'ReloadTips',
        node_ref: 'nodes[7]',
        pin_ref: 'then',
      },
    },
  });
  const write = step.write as { strategy: string; ops: Array<Record<string, unknown>> };

  assert.deepEqual(write.ops[0]?.anchor, logicJsonAnchorSelector);
});

test('merge_external_flow rejects conflicting LogicJson selector graph aliases', () => {
  assert.throws(
    () => compileMergeExternalStep({
      merge: {
        anchor: {
          ...logicJsonAnchorSelector,
          graph: 'OtherGraph',
        },
      },
    }),
    /graph and graph_name must match/,
  );
});

test('merge_external_flow requires external mutation policy allowlist', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      scopePolicy: {
        external_mutation_policy: undefined,
      },
    }) as never),
    /unsupported_scope_policy/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      scopePolicy: {
        external_mutation_policy: {
          strategy: 'merge_external_flow',
          allowed_mutations: ['node_comment'],
        },
      },
    }) as never),
    /exec_boundary_link/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      scopePolicy: {
        external_mutation_policy: {
          strategy: 'merge_external_flow',
          allowed_mutations: ['exec_boundary_link', 'node_comment'],
        },
      },
    }) as never),
    /exactly/,
  );
});

test('merge_external_flow rejects broad user node mutation policy', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      scopePolicy: {
        allow_modify_user_nodes: true,
      },
    }) as never),
    /unsupported_scope_policy/,
  );
});

test('merge_external_flow rejects owned merges mixed into external strategy', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      behavior: {
        merges: [{
          kind: 'insert_flow',
          scope: 'custom_event_call',
          insert_strategy: 'append_after',
          anchor: { block_id: 'BH_Block', node_ref: 'nodes[0]', pin_ref: 'then' },
          inserted: { call_kind: 'custom_event_call', name: 'PrintString' },
        }],
      },
    }) as never),
    /merges does not belong/,
  );
});

test('owned graph strategies reject external merges mixed into behavior', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan({
      ...makeMergeExternalFlowSpec(),
      scope_policy: {
        graph_name: 'EventGraph',
        allow_modify_user_nodes: false,
      },
      behavior: {
        graph_strategy: 'merge_owned_graph',
        merges: [{
          kind: 'insert_flow',
          scope: 'custom_event_call',
          insert_strategy: 'append_after',
          anchor: {
            block_id: 'BH_Block',
            group_entry_node_path: 'entry',
            node_ref: 'CallNode',
            pin_ref: 'then',
          },
          inserted: {
            call_kind: 'custom_event_call',
            name: 'PrintString',
          },
        }],
        external_merges: [{
          kind: 'insert_external_flow',
          insert_strategy: 'append_after',
          anchor: externalAnchor,
          inserted: {
            body: {
              schema: 'BlueprintLogicSpec.v1',
              statements: [{ kind: 'call', target: 'PrintString' }],
            },
          },
        }],
      },
    } as never),
    /external_merges does not belong/,
  );
});

test('merge_external_flow rejects raw LogicJson and ad hoc anchors', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      merge: {
        anchor: {
          ...externalAnchor,
          node_guid: 'nodes[0]',
        },
      },
    }) as never),
    /node_guid/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      merge: {
        anchor: {
          ...externalAnchor,
          node_guid: '$.nodes[0]',
        },
      },
    }) as never),
    /node_guid/,
  );
});

test('merge_external_flow rejects display-only link refs in compact anchor slot', () => {
  assert.throws(() => compileMergeExternalStep({
    merge: {
      insert_strategy: 'insert_between',
      anchor: {
        anchor_type: 'external_link',
        anchor_ref: 'links[5]',
      },
    },
  }), /links\[n\]|read-view|display-only/i);
});

test('merge_external_flow branch_fork requires sequence_order', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      merge: {
        insert_strategy: 'branch_fork',
      },
    }) as never),
    /sequence_order/,
  );

  const step = compileMergeExternalStep({
    merge: {
      insert_strategy: 'branch_fork',
      sequence_order: ['inserted_logic', 'original_successor'],
    },
  });
  const write = step.write as { ops: Array<Record<string, unknown>> };
  assert.deepEqual(write.ops[0]?.sequence_order, ['inserted_logic', 'original_successor']);
});

test('merge_external_flow branch_fork rejects duplicate sequence_order entries', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      merge: {
        insert_strategy: 'branch_fork',
        sequence_order: ['inserted_logic', 'inserted_logic'],
      },
    }) as never),
    /sequence_order/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      merge: {
        insert_strategy: 'branch_fork',
        sequence_order: ['inserted_logic', 'original_successor', 'original_successor'],
      },
    }) as never),
    /sequence_order/,
  );
});

test('merge_external_flow rejects inserted body without BlueprintLogicSpec schema', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeMergeExternalFlowSpec({
      merge: {
        inserted: {
          body: {
            statements: [{ kind: 'call', target: 'PrintString' }],
          },
        },
      },
    }) as never),
    /BlueprintLogicSpec/,
  );
});
