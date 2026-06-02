import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import { GraphWriteTaskSpecSchema } from './task-schemas.js';

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
  graph: 'EventGraph',
  entry_name: 'ReloadTips',
  node_ref: 'nodes[7]',
  pin_ref: 'then',
};

function makeMergeExternalFlowSpec(overrides: {
  scopePolicy?: Record<string, unknown>;
  merge?: Record<string, unknown>;
  behavior?: Record<string, unknown>;
} = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
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
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}

describe('GraphWrite merge_external_flow task schema', () => {
  it('accepts the exact P2 external mutation policy', () => {
    const result = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec());
    assert.equal(result.success, true);
  });

  it('accepts a LogicJson anchor selector as an authoring anchor', () => {
    const result = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec({
      merge: {
        anchor: logicJsonAnchorSelector,
      },
    }));
    assert.equal(result.success, true);
  });

  it('rejects ambiguous LogicJson selector refs', () => {
    const result = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec({
      merge: {
        anchor: {
          ...logicJsonAnchorSelector,
          link_ref: 'Call.Then->Next.Execute',
        },
      },
    }));
    assert.equal(result.success, false);
    assert.match(result.error.issues.map((issue) => issue.message).join('\n'), /exactly one of node_ref or link_ref/i);
  });

  it('rejects conflicting LogicJson selector graph aliases', () => {
    const result = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec({
      merge: {
        anchor: {
          ...logicJsonAnchorSelector,
          graph_name: 'OtherGraph',
        },
      },
    }));
    assert.equal(result.success, false);
    assert.match(result.error.issues.map((issue) => issue.message).join('\n'), /graph and graph_name must match/i);
  });

  it('rejects missing or extra allowed external mutations', () => {
    const missing = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec({
      scopePolicy: {
        external_mutation_policy: {
          strategy: 'merge_external_flow',
          allowed_mutations: [],
        },
      },
    }));
    assert.equal(missing.success, false);
    assert.match(missing.error.issues.map((issue) => issue.message).join('\n'), /exec_boundary_link/i);

    const extra = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec({
      scopePolicy: {
        external_mutation_policy: {
          strategy: 'merge_external_flow',
          allowed_mutations: ['exec_boundary_link', 'node_comment'],
        },
      },
    }));
    assert.equal(extra.success, false);
    assert.match(extra.error.issues.map((issue) => issue.message).join('\n'), /exactly/i);
  });

  it('rejects external merges on owned strategies', () => {
    const result = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec({
      behavior: {
        graph_strategy: 'merge_owned_graph',
        merges: [{
          kind: 'insert_flow',
          scope: 'custom_event_call',
          insert_strategy: 'append_after',
          anchor: {
            block_id: 'BH_Block',
            group_entry_node_path: 'entry',
            node_ref: 'call',
            pin_ref: 'then',
          },
          inserted: {
            call_kind: 'custom_event_call',
            name: 'PrintString',
          },
        }],
      },
    }));

    assert.equal(result.success, false);
    assert.match(result.error.issues.map((issue) => issue.message).join('\n'), /external_merges does not belong/i);
  });

  it('rejects invalid inserted body shapes before compile', () => {
    const result = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec({
      merge: {
        inserted: {
          body: {
            statements: [{ kind: 'call', target: 'PrintString' }],
          },
        },
      },
    }));

    assert.equal(result.success, false);
    assert.equal(result.error.issues.some((issue) => issue.path.join('.') === 'behavior.external_merges.0.inserted.body.schema'), true);
  });

  it('rejects duplicate branch_fork sequence_order entries', () => {
    const duplicateInserted = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec({
      merge: {
        insert_strategy: 'branch_fork',
        sequence_order: ['inserted_logic', 'inserted_logic'],
      },
    }));
    assert.equal(duplicateInserted.success, false);
    assert.match(duplicateInserted.error.issues.map((issue) => issue.message).join('\n'), /sequence_order/i);

    const duplicateOriginal = GraphWriteTaskSpecSchema.safeParse(makeMergeExternalFlowSpec({
      merge: {
        insert_strategy: 'branch_fork',
        sequence_order: ['inserted_logic', 'original_successor', 'original_successor'],
      },
    }));
    assert.equal(duplicateOriginal.success, false);
    assert.match(duplicateOriginal.error.issues.map((issue) => issue.message).join('\n'), /sequence_order/i);
  });
});
