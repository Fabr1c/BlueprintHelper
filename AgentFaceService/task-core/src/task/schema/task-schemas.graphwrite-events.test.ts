import { strict as assert } from 'node:assert';
import test from 'node:test';

import { GraphWriteTaskSpecSchema } from './task-schemas.js';

function makeGraphWriteSpec(entryOverrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_graphwrite_event_schema_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'GraphWriteEventSchemaTs',
    target: {
      asset_path: '/Game/BP/BP_GraphWriteEventSchema',
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
        name: 'DefaultEvent',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [{
            kind: 'call',
            target: 'PrintString',
          }],
        },
        ...entryOverrides,
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

test('append entry accepts custom_event without catalog evidence', () => {
  const result = GraphWriteTaskSpecSchema.safeParse(makeGraphWriteSpec({
    name: 'BndEvt__BusinessTelemetry',
    event_kind: 'custom_event',
  }));

  assert.equal(result.success, true, JSON.stringify(result, null, 2));
});

test('append entry rejects non-custom event kinds without catalog evidence', () => {
  const result = GraphWriteTaskSpecSchema.safeParse(makeGraphWriteSpec({
    name: 'ReceiveBeginPlay',
    event_kind: 'override_event',
  }));

  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /catalog_evidence/);
  }
});

test('append entry requires signature_evidence_id for signature catalog evidence', () => {
  const result = GraphWriteTaskSpecSchema.safeParse(makeGraphWriteSpec({
    name: 'ReceiveTick',
    event_kind: 'override_event',
    catalog_evidence: {
      source: 'signature',
    },
  }));

  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /signature_evidence_id/);
  }
});

test('append entry requires action_stable_id and context_fingerprint for graph action catalog evidence', () => {
  const result = GraphWriteTaskSpecSchema.safeParse(makeGraphWriteSpec({
    name: 'InpActEvt_Interact_K2Node_InputActionEvent_0',
    event_kind: 'input_action_event',
    catalog_evidence: {
      source: 'graph_action_catalog',
    },
  }));

  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    const issues = JSON.stringify(result.error.issues);
    assert.match(issues, /action_stable_id/);
    assert.match(issues, /context_fingerprint/);
  }
});
