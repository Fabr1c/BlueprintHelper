import assert from 'node:assert/strict';
import test from 'node:test';

import {
  applyProjectedEvidence,
  collectProjectedEvidenceRequests,
  unresolvedProjectedEvidenceItems,
} from './patch-graphwrite-generality-projected-evidence.js';

test('GraphWrite projected evidence patcher requests only ActionDatabase-backed operations', () => {
  const spec = makeSpec([
    {
      kind: 'create',
      create_operation: 'asset_action',
      context_evidence: {
        'graphwrite_generality.operation_id': 'create.asset_action',
        'graphwrite_generality.variant_name': 'GW_asset',
      },
    },
    {
      kind: 'schedule',
      schedule_operation: 'timer_delegate_node',
      context_evidence: {
        'graphwrite_generality.operation_id': 'schedule.timer_delegate_node',
        'graphwrite_generality.variant_name': 'GW_timer',
      },
    },
    {
      kind: 'call',
      context_evidence: {
        'graphwrite_generality.operation_id': 'function_action.call_function',
        'graphwrite_generality.variant_name': 'GW_call',
      },
    },
  ]);

  assert.deepEqual(collectProjectedEvidenceRequests(spec), [
    {
      request_id: 'GW_asset',
      operation_id: 'create.asset_action',
      projection_kind: 'asset_action',
      queries: ['Make Array'],
    },
    {
      request_id: 'GW_timer',
      operation_id: 'schedule.timer_delegate_node',
      projection_kind: 'schedule',
      queries: ['Set Timer by Event', 'Set Timer by Delegate', 'Set Timer'],
    },
  ]);
});

test('GraphWrite projected evidence patcher overlays UE evidence without dropping handler proof', () => {
  const spec = makeSpec([
    {
      kind: 'schedule',
      schedule_operation: 'timer_delegate_node',
      context_evidence: {
        'graphwrite_generality.operation_id': 'schedule.timer_delegate_node',
        'graphwrite_generality.variant_name': 'GW_timer',
        handler_name: 'Handle_GW_timer',
        handler_function_path: '/Game/BP.BP:Handle_GW_timer',
      },
    },
  ]);

  const patched = applyProjectedEvidence(spec, [
    {
      request_id: 'GW_timer',
      status: 'resolved',
      evidence: {
        schedule_action_stable_id: 'action_database:/Script/Engine.KismetSystemLibrary:/Script/BlueprintGraph.K2Node_CallFunction:sig',
        schedule_node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
        schedule_spawner_signature: 'sig',
        schedule_owner_path: '/Script/Engine.KismetSystemLibrary',
      },
    },
  ]);

  const evidence = ((spec.behavior as Record<string, unknown>).entries as Record<string, unknown>[])[0]
    .body as Record<string, unknown>;
  const statement = (evidence.statements as Record<string, unknown>[])[0];
  const contextEvidence = statement.context_evidence as Record<string, unknown>;
  assert.equal(patched, 1);
  assert.equal(contextEvidence['handler_name'], 'Handle_GW_timer');
  assert.equal(contextEvidence['schedule_action_stable_id'], 'action_database:/Script/Engine.KismetSystemLibrary:/Script/BlueprintGraph.K2Node_CallFunction:sig');
});

test('GraphWrite projected evidence patcher reports unresolved items', () => {
  assert.deepEqual(unresolvedProjectedEvidenceItems([
    { request_id: 'a', status: 'resolved', evidence: {} },
    { request_id: 'b', status: 'failed', message: 'missing' },
  ]), [
    { request_id: 'b', status: 'failed', message: 'missing' },
  ]);
});

function makeSpec(statements: Record<string, unknown>[]) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    behavior: {
      entries: [
        {
          entry_type: 'custom_event',
          name: 'Entry',
          body: {
            schema: 'BlueprintLogicSpec.v1',
            statements,
          },
        },
      ],
    },
  };
}
