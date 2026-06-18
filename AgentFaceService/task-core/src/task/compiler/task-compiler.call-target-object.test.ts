import { strict as assert } from 'node:assert';
import test from 'node:test';

import { GRAPHWRITE_SLOT_MANIFEST } from './graphwrite/generated/graphwrite-slot-manifest.generated.js';
import { taskPlanToAppendBridgePayload } from './graphwrite/graphwrite-task-type-compiler.js';
import { compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeGraphSpec(statements: Record<string, unknown>[]) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: {
      asset_path: '/Game/BH_Tests/BP_CallTargetObject',
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
        name: 'GW_CallTargetObject',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements,
        },
      }],
    },
  };
}

function compileBridgeStatements(statements: Record<string, unknown>[]) {
  const taskPlan = compileTaskSpecToTaskPlan(makeGraphSpec(statements) as never);
  const payload = taskPlanToAppendBridgePayload(taskPlan, true);
  return payload.logic_spec.statements as Record<string, unknown>[];
}

test('call statement slots expose target_object before args', () => {
  for (const slotId of [
    'graph.statement.call.direct',
    'graph.statement.call.auto_search',
    'graph.statement.call.result_symbol',
  ]) {
    const slot = GRAPHWRITE_SLOT_MANIFEST.find((entry) => entry.slot_id === slotId);
    assert.ok(slot, `${slotId} exists`);

    assert.equal(slot.input_slots[0]?.name, 'target_object');
    assert.equal(slot.input_slots[0]?.path, 'target_object');
    assert.deepEqual(slot.input_slots[0]?.accepts, ['expression']);
    assert.equal(slot.input_slots[0]?.type_hint, 'object');
    assert.ok(slot.input_slots.some((input) => input.path.startsWith('args.')));
  }
});

test('compiler preserves nested target_object receiver expression in call statements', () => {
  const [statement] = compileBridgeStatements([{
    kind: 'call',
    target: 'SetPercent',
    target_object: {
      kind: 'field',
      field_operation: 'get',
      field_scope: 'field_access',
      target: 'StaminaBar',
      property_path: 'StaminaBar',
      target_object: { kind: 'get', target: 'VitalsHudWidget' },
      context_evidence: {
        field_owner_class: '/Game/UI/WBP_Vitals.WBP_Vitals_C',
      },
    },
    args: {
      InPercent: { kind: 'literal', value_type: 'float', value: 0.75 },
    },
  }]);

  assert.equal(statement.kind, 'call');
  assert.equal(statement.target, 'SetPercent');

  const targetObject = statement.target_object as Record<string, unknown>;
  assert.equal(targetObject.kind, 'field');
  assert.equal(targetObject.field_scope, 'field_access');
  assert.equal(targetObject.target, 'StaminaBar');

  const receiverOwner = targetObject.target_object as Record<string, unknown>;
  assert.equal(receiverOwner.kind, 'get');
  assert.equal(receiverOwner.target, 'VitalsHudWidget');
  assert.equal((statement.args as Record<string, unknown>).target_object, undefined);
});
