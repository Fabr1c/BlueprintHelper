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
