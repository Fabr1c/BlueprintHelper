import assert from 'node:assert/strict';
import test from 'node:test';

import { makeGraphWriteGeneralityBundles } from './graphwrite-generality-spec-factory.js';

test('GraphWrite generality spec factory preserves container_action target roles', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'container.array.add');

  assert.ok(bundle);
  const spec = bundle.graphWriteSpec as Record<string, any>;
  const statement = spec.behavior.entries[0].body.statements[0] as Record<string, unknown>;
  assert.deepEqual(statement.target, { kind: 'get', name: 'GWGenIntArray' });
});

test('GraphWrite generality filter array fixture uses class-typed filter literal', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'container.array.filter_array');

  assert.ok(bundle);
  const spec = bundle.graphWriteSpec as Record<string, any>;
  const statement = spec.behavior.entries[0].body.statements[0] as Record<string, any>;
  assert.deepEqual(statement.filter_class, {
    kind: 'literal',
    value_type: '/Script/CoreUObject.Class',
    value: '/Script/Engine.Actor',
  });
});

test('GraphWrite generality setup creates string-key int-value map fixtures', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'container.map.is_empty');

  assert.ok(bundle);
  const fixtureSpec = bundle.setupSpecs[1] as Record<string, any>;
  const variables = fixtureSpec.variables as Array<Record<string, any>>;
  const mapVariable = variables.find((variable) => variable.name === 'GWGenStringIntMap');

  assert.ok(mapVariable);
  assert.deepEqual(mapVariable.pin_type, {
    category: 'string',
    container_type: 'map',
    value_type: { category: 'int' },
  });
});

test('GraphWrite generality control fixtures carry concrete control evidence', () => {
  const bundles = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  });

  const branch = bundles.find((candidate) => candidate.operation.operationId === 'generic_ops.control.branch');
  assert.ok(branch);
  const branchSpec = branch.graphWriteSpec as Record<string, any>;
  const branchStatement = branchSpec.behavior.entries[0].body.statements[0] as Record<string, any>;
  assert.equal(branchStatement.context_evidence['generic.control.operation'], 'branch');

  const macro = bundles.find((candidate) => candidate.operation.operationId === 'generic_ops.control.for_loop');
  assert.ok(macro);
  const macroSpec = macro.graphWriteSpec as Record<string, any>;
  const macroStatement = macroSpec.behavior.entries[0].body.statements[0] as Record<string, any>;
  assert.equal(macroStatement.context_evidence['generic.control.operation'], 'for_loop');
  assert.equal(macroStatement.context_evidence['generic.macro.graph_path'], '/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForLoop');

  const doN = bundles.find((candidate) => candidate.operation.operationId === 'generic_ops.control.do_n');
  assert.ok(doN);
  const doNSpec = doN.graphWriteSpec as Record<string, any>;
  const doNStatement = doNSpec.behavior.entries[0].body.statements[0] as Record<string, any>;
  assert.equal(doNStatement.context_evidence['generic.macro.graph_path'], '/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:Do N');

  const macroLike = bundles.find((candidate) => candidate.operation.operationId === 'function_action.macro_like');
  assert.ok(macroLike);
  const macroLikeSpec = macroLike.graphWriteSpec as Record<string, any>;
  const multiGateStatement = macroLikeSpec.behavior.entries[9].body.statements[0] as Record<string, any>;
  assert.equal(multiGateStatement.context_evidence['generic.control.operation'], 'multi_gate');
  assert.equal(multiGateStatement.context_evidence['generic.control.dynamic_output_count'], '2');
});

test('GraphWrite generality spec factory can focus specific operations', () => {
  const bundles = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
    operationIds: ['generic_ops.control.branch', 'container_map_is_empty'],
  });

  assert.deepEqual(bundles.map((bundle) => bundle.operation.operationId), [
    'container.map.is_empty',
    'generic_ops.control.branch',
  ]);
});

test('GraphWrite generality select fixtures carry concrete result type proof', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'generic_ops.struct_select.select');

  assert.ok(bundle);
  const spec = bundle.graphWriteSpec as Record<string, any>;
  const selectExpression = spec.behavior.entries[0].body.statements[0].value as Record<string, any>;
  assert.equal(selectExpression.context_evidence['generic.select.result_type_proof'], 'string');
});

test('GraphWrite generality schedule latent fixture carries graph permission evidence', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'schedule.latent_or_async_node');

  assert.ok(bundle);
  const spec = bundle.graphWriteSpec as Record<string, any>;
  const statement = spec.behavior.entries[0].body.statements[0] as Record<string, any>;
  assert.equal(statement.graph_latent_allowed, true);
  assert.equal(statement.context_evidence.graph_latent_allowed, 'true');
});

test('GraphWrite generality schedule timer fixture declares BlueprintSignature handler evidence', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'schedule.timer_delegate_node');

  assert.ok(bundle);
  const spec = bundle.graphWriteSpec as Record<string, any>;
  const entries = spec.behavior.entries as Array<Record<string, any>>;
  const handlerEntry = entries.find((entry) => entry.name === 'Handle_GWGen_schedule_timer_delegate_node_00');
  assert.ok(handlerEntry);
  assert.equal(handlerEntry.entry_type, 'custom_event');
  const statement = entries.find((entry) => entry.name === 'GWGen_schedule_timer_delegate_node_00')?.body.statements[0] as Record<string, any>;
  assert.equal(statement.context_evidence.handler_name, 'Handle_GWGen_schedule_timer_delegate_node_00');
  assert.equal(
    statement.context_evidence.handler_function_path,
    '/Game/BlueprintHelper/Generality/BP_Test_schedule_timer_delegate_node.SKEL_BP_Test_schedule_timer_delegate_node_C:Handle_GWGen_schedule_timer_delegate_node_00',
  );
  assert.equal(statement.context_evidence.handler_source_cluster, 'BlueprintSignature');
  assert.equal(statement.context_evidence.signature_evidence_id, 'signature:custom_event:Handle_GWGen_schedule_timer_delegate_node_00');
});

test('GraphWrite generality create map fixture carries map pin evidence as strings', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'generic_ops.create.make_map');

  assert.ok(bundle);
  const spec = bundle.graphWriteSpec as Record<string, any>;
  const statement = spec.behavior.entries[0].body.statements[0] as Record<string, any>;
  assert.equal(statement.pin_type, 'category=wildcard|container=map');
  assert.equal(statement.key_pin_type, 'string');
  assert.equal(statement.value_pin_type, 'int');
});

test('GraphWrite generality struct fixtures populate required field maps and facts', () => {
  const bundles = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  });

  const structSet = bundles.find((candidate) => candidate.operation.operationId === 'field.struct_member_set');
  assert.ok(structSet);
  const structSetSpec = structSet.graphWriteSpec as Record<string, any>;
  const structSetStatement = structSetSpec.behavior.entries[0].body.statements[0] as Record<string, any>;
  assert.equal(structSetStatement.capability_facts['field.struct_type'], '/Script/CoreUObject.Rotator');
  assert.equal(structSetStatement.capability_facts['field.property_path'], 'Roll');

  const makeStruct = bundles.find((candidate) => candidate.operation.operationId === 'generic_ops.struct_select.make_struct');
  assert.ok(makeStruct);
  const makeStructSpec = makeStruct.graphWriteSpec as Record<string, any>;
  const construct = makeStructSpec.behavior.entries[0].body.statements[0].value as Record<string, any>;
  assert.deepEqual(Object.keys(construct.fields), ['X', 'Y', 'Z']);

  const breakStruct = bundles.find((candidate) => candidate.operation.operationId === 'generic_ops.struct_select.break_struct');
  assert.ok(breakStruct);
  const breakStructSpec = breakStruct.graphWriteSpec as Record<string, any>;
  const deconstruct = breakStructSpec.behavior.entries[0].body.statements[0].value as Record<string, any>;
  assert.deepEqual(deconstruct.fields, ['X', 'Y', 'Z']);
  assert.deepEqual(Object.keys(deconstruct.value.fields), ['X', 'Y', 'Z']);
});

test('GraphWrite generality delegate fixtures declare handlers and do not attach handlers to clear', () => {
  const bundles = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  });

  const bind = bundles.find((candidate) => candidate.operation.operationId === 'event_delegate.delegate.bind');
  assert.ok(bind);
  const bindSpec = bind.graphWriteSpec as Record<string, any>;
  const bindEntries = bindSpec.behavior.entries as Array<Record<string, any>>;
  const handlerEntry = bindEntries.find((entry) => entry.name === 'Handle_GWGen_event_delegate_delegate_bind_00');
  assert.ok(handlerEntry);
  assert.equal(handlerEntry.entry_type, 'custom_event');
  assert.equal(handlerEntry.inputs.length, 6);
  const bindStatement = bindEntries.find((entry) => entry.name === 'GWGen_event_delegate_delegate_bind_00')?.body.statements[0] as Record<string, any>;
  assert.equal(bindStatement.target, 'TriggerBox');
  assert.equal(bindStatement.delegate, 'OnComponentBeginOverlap');
  assert.equal(bindStatement.context_evidence['event_delegate.handler_source_cluster'], 'BlueprintSignature');
  assert.equal(
    bindStatement.context_evidence['event_delegate.handler_function_path'],
    '/Game/BlueprintHelper/Generality/BP_Test_event_delegate_delegate_bind.SKEL_BP_Test_event_delegate_delegate_bind_C:Handle_GWGen_event_delegate_delegate_bind_00',
  );
  assert.equal(
    bindStatement.context_evidence['event_delegate.signature_evidence_id'],
    'signature:custom_event:Handle_GWGen_event_delegate_delegate_bind_00',
  );

  const clear = bundles.find((candidate) => candidate.operation.operationId === 'event_delegate.delegate.clear');
  assert.ok(clear);
  const clearSpec = clear.graphWriteSpec as Record<string, any>;
  const clearStatement = clearSpec.behavior.entries.find((entry: Record<string, any>) => entry.name === 'GWGen_event_delegate_delegate_clear_00').body.statements[0] as Record<string, any>;
  assert.equal(clearStatement.kind, 'delegate.unbind_all');
  assert.equal(Object.hasOwn(clearStatement, 'handler'), false);
});

test('GraphWrite generality type promotion fixture carries projected type-promotion evidence', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'generic_ops.transform.type_promotion');

  assert.ok(bundle);
  const spec = bundle.graphWriteSpec as Record<string, any>;
  const statement = spec.behavior.entries[0].body.statements[0] as Record<string, any>;
  assert.equal(statement.transform_operation, 'type_promotion');
  assert.equal(statement.context_evidence.type_promotion_stable_id, 'type_promotion:Add:int:real');
  assert.equal(statement.context_evidence.type_promotion_operator, 'Add');
  assert.equal(statement.context_evidence.type_promotion_source_pin_type, 'int');
  assert.equal(statement.context_evidence.type_promotion_target_pin_type, 'real');
  assert.equal(statement.context_evidence.type_promotion_result_pin_type, 'real');
});

test('GraphWrite generality intpoint_equal fixture uses typed IntPoint operands', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'op.intpoint_equal');

  assert.ok(bundle);
  const spec = bundle.graphWriteSpec as Record<string, any>;
  const value = spec.behavior.entries[0].body.statements[0].value as Record<string, any>;
  assert.equal(value.op, 'intpoint_equal');
  assert.equal(value.left.type, '/Script/CoreUObject.IntPoint');
  assert.equal(value.right.type, '/Script/CoreUObject.IntPoint');
  assert.deepEqual(Object.keys(value.left.fields), ['X', 'Y']);
  assert.deepEqual(Object.keys(value.right.fields), ['X', 'Y']);
});

test('GraphWrite generality spec factory emits distinct function call node candidates', () => {
  const bundle = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_Test',
    graphName: 'EG_Test',
  }).find((candidate) => candidate.operation.operationId === 'function_action.call_function');

  assert.ok(bundle);
  const spec = bundle.graphWriteSpec as Record<string, any>;
  const entries = spec.behavior.entries as Array<Record<string, any>>;
  const targets = entries.map((entry) => entry.body.statements[0].target);
  assert.equal(entries.length, 10);
  assert.equal(new Set(targets).size, 10);
  assert.equal(targets[0], '/Script/Engine.KismetSystemLibrary:PrintString');
  assert.equal(bundle.assetPath.endsWith('_function_action_call_function'), true);
});

test('GraphWrite generality asset paths remain unique when long run ids share a truncated prefix', () => {
  const first = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWrite80_RealCaseE2E_AllFix02_20260527',
    graphName: 'EG_Test',
    operationIds: ['generic_ops.control.for_loop_with_break'],
  })[0];
  const second = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWrite80_RealCaseE2E_AllFix03_20260527',
    graphName: 'EG_Test',
    operationIds: ['generic_ops.control.for_loop_with_break'],
  })[0];

  assert.ok(first);
  assert.ok(second);
  assert.notEqual(first.assetPath, second.assetPath);
});
