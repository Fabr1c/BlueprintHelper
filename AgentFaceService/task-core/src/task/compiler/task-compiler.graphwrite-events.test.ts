import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan, taskPlanToAppendBridgePayload } from './task-compiler.js';

function makeGraphWriteSpec(entryOverrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_graphwrite_event_compiler_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'GraphWriteEventCompilerTs',
    target: {
      asset_path: '/Game/BP/BP_GraphWriteEventCompiler',
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

function compileGraphWriteStep(entryOverrides: Record<string, unknown> = {}): Record<string, unknown> {
  const taskPlan = compileTaskSpecToTaskPlan(makeGraphWriteSpec(entryOverrides) as never);
  const graphWriteStep = taskPlan.steps.find((step) => (
    (step as Record<string, unknown>).capability === 'graph_write'
  )) as Record<string, unknown> | undefined;
  assert.ok(graphWriteStep);
  return graphWriteStep;
}

function compileBridgeEntry(entryOverrides: Record<string, unknown> = {}): Record<string, unknown> {
  const taskPlan = compileTaskSpecToTaskPlan(makeGraphWriteSpec(entryOverrides) as never);
  const bridgePayload = taskPlanToAppendBridgePayload(taskPlan, true) as unknown as Record<string, unknown>;
  const logicSpec = bridgePayload.logic_spec as Record<string, unknown>;
  return logicSpec.entry as Record<string, unknown>;
}

test('append ensure_entry preserves event_kind and catalog_evidence through TaskPlan and bridge lowering', () => {
  const catalogEvidence = {
    source: 'signature',
    signature_evidence_id: 'signature:override_event:ReceiveBeginPlay',
  };
  const graphWriteStep = compileGraphWriteStep({
    name: 'ReceiveBeginPlay',
    event_kind: 'override_event',
    catalog_evidence: catalogEvidence,
  });

  const write = graphWriteStep.write as { ops: Array<Record<string, unknown>> };
  assert.equal(write.ops[0].event_kind, 'override_event');
  assert.deepEqual(write.ops[0].catalog_evidence, catalogEvidence);
  assert.equal(write.ops[0].signature_evidence_id, 'signature:override_event:ReceiveBeginPlay');

  const bridgeEntry = compileBridgeEntry({
    name: 'ReceiveBeginPlay',
    event_kind: 'override_event',
    catalog_evidence: catalogEvidence,
  });
  assert.equal(bridgeEntry.kind, 'override_event');
  assert.deepEqual(bridgeEntry.catalog_evidence, catalogEvidence);
  assert.equal(bridgeEntry.signature_evidence_id, 'signature:override_event:ReceiveBeginPlay');
});

test('append ensure_entry defaults missing event_kind to custom_event', () => {
  const graphWriteStep = compileGraphWriteStep({
    name: 'BndEvt__BusinessTelemetry',
  });

  const write = graphWriteStep.write as { ops: Array<Record<string, unknown>> };
  assert.equal(Object.hasOwn(write.ops[0], 'event_kind'), false);
  assert.equal(write.ops[0].signature_evidence_id, 'signature:custom_event:BndEvt__BusinessTelemetry');

  const bridgeEntry = compileBridgeEntry({
    name: 'BndEvt__BusinessTelemetry',
  });
  assert.equal(bridgeEntry.kind, 'custom_event');
  assert.equal(bridgeEntry.signature_evidence_id, 'signature:custom_event:BndEvt__BusinessTelemetry');
});
