import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

import {
  graphWriteOperationKey,
  graphWriteVariantNames,
  makeGraphWriteGeneralityOperations,
  type GraphWriteGeneralityOperation,
} from './graphwrite-generality-matrix.js';
import { getRequiredContainerActionRoles } from '../schema/task-schemas.js';

type JsonRecord = Record<string, unknown>;

export interface GraphWriteGeneralityBundle {
  operation: GraphWriteGeneralityOperation;
  assetPath: string;
  graphName: string;
  setupSpecs: JsonRecord[];
  graphWriteSpec: JsonRecord;
  expectedVariantNames: string[];
  expectedNodeCandidates: readonly string[];
  expectedReadback: string;
}

export function makeGraphWriteGeneralitySetupSpecs(input: { assetPath: string }): JsonRecord[] {
  return [
    {
      schema: 'BlueprintHelper.TaskSpec.v1',
      context_id: 'ctx_graphwrite_generality_create_asset',
      task_type: 'create_asset',
      feature_name: 'GraphWriteGeneralityAsset',
      target: {
        asset_path: input.assetPath,
        target_type: 'asset',
      },
      behavior: {
        asset_strategy: 'ensure_asset',
        asset: {
          asset_type: 'blueprint_class',
          parent_class: 'Actor',
          collision_policy: 'fail_if_exists',
        },
      },
    },
    {
      schema: 'BlueprintHelper.TaskSpec.v1',
      context_id: 'ctx_graphwrite_generality_fixture',
      task_type: 'create_blueprint_feature',
      feature_name: 'GraphWriteGeneralityFixture',
      target: {
        asset_path: input.assetPath,
        target_type: 'blueprint',
      },
      scope_policy: {
        prefer_new_graph: true,
        graph_name: 'EG_GraphWriteGeneralityFixture',
        allow_modify_user_nodes: false,
        allow_create_assets: false,
      },
      asset_policy: {
        if_target_asset_missing: 'fail',
        if_referenced_asset_missing: 'fail',
        if_component_exists: 'reuse_if_type_matches',
      },
      components: [
        { name: 'SceneRoot', class: 'SceneComponent', set_as_root: true },
        { name: 'TriggerBox', class: 'BoxComponent', attach_to: 'SceneRoot' },
      ],
      variables: [
        { name: 'GWGenBool', type: 'bool', default: false, category: 'GraphWriteGenerality' },
        { name: 'GWGenInt', type: 'int', default: 0, category: 'GraphWriteGenerality' },
        { name: 'GWGenFloat', type: 'float', default: 0, category: 'GraphWriteGenerality' },
        { name: 'GWGenString', type: 'string', default: '', category: 'GraphWriteGenerality' },
        { name: 'GWGenRotator', pin_type: { category: 'struct', object_path: '/Script/CoreUObject.Rotator' }, category: 'GraphWriteGenerality' },
        { name: 'GWGenIntArray', pin_type: { category: 'int', container_type: 'array' }, category: 'GraphWriteGenerality' },
        { name: 'GWGenOtherIntArray', pin_type: { category: 'int', container_type: 'array' }, category: 'GraphWriteGenerality' },
        { name: 'GWGenIntSet', pin_type: { category: 'int', container_type: 'set' }, category: 'GraphWriteGenerality' },
        { name: 'GWGenOtherIntSet', pin_type: { category: 'int', container_type: 'set' }, category: 'GraphWriteGenerality' },
        { name: 'GWGenStringIntMap', pin_type: { category: 'string', container_type: 'map', value_type: { category: 'int' } }, category: 'GraphWriteGenerality' },
        { name: 'GWGenRandomStream', pin_type: { category: 'struct', object_path: '/Script/CoreUObject.RandomStream' }, category: 'GraphWriteGenerality' },
      ],
    },
  ];
}

export function makeGraphWriteGeneralityBundles(input: { assetPath: string; graphName: string; operationIds?: readonly string[] }): GraphWriteGeneralityBundle[] {
  const operationFilter = makeOperationFilter(input.operationIds ?? []);
  return makeGraphWriteGeneralityOperations()
    .filter((operation) => operationFilter.size === 0
      || operationFilter.has(operation.operationId)
      || operationFilter.has(graphWriteOperationKey(operation.operationId)))
    .map((operation) => {
    const key = graphWriteOperationKey(operation.operationId);
    const assetPath = makeOperationAssetPath(input.assetPath, key);
    const variants = graphWriteVariantNames(operation);
    const candidates = operation.spawnCandidateNames.slice(0, operation.requiredVariantCount);
    const graphName = `${input.graphName}_${key}`.slice(0, 180);
    return {
      operation,
      assetPath,
      graphName,
      setupSpecs: makeGraphWriteGeneralitySetupSpecs({ assetPath }),
      expectedVariantNames: variants,
      expectedNodeCandidates: candidates,
      expectedReadback: graphName,
      graphWriteSpec: {
        schema: 'BlueprintHelper.TaskSpec.v1',
        context_id: `ctx_graphwrite_generality_${key}`,
        task_type: 'edit_blueprint_graph',
        feature_name: `GraphWriteGenerality_${key}`,
        target: {
          asset_path: assetPath,
          target_type: 'blueprint',
        },
        scope_policy: {
          graph_name: graphName,
          allow_modify_user_nodes: false,
        },
        behavior: {
          graph_strategy: 'append_new_owned_graph',
          entries: makeEntriesForOperation(operation, variants, candidates, assetPath),
        },
      },
    };
  });
}

function makeEntriesForOperation(
  operation: GraphWriteGeneralityOperation,
  variants: readonly string[],
  candidates: readonly string[],
  assetPath: string,
): JsonRecord[] {
  return variants.flatMap((variantName, index) => {
    const statement = makeStatementForOperation(operation, variantName, index, candidates[index] ?? operation.operationId, assetPath);
    const entries: JsonRecord[] = [];
    const handlerName = delegateHandlerName(variantName);
    if (operation.operationId.startsWith('event_delegate.') && operation.operationId !== 'event_delegate.delegate.clear') {
      entries.push(makeOverlapHandlerEntry(handlerName));
    }
    if (operation.operationId === 'schedule.timer_delegate_node') {
      entries.push(makeTimerHandlerEntry(handlerName));
    }
    entries.push({
      entry_type: 'custom_event',
      name: variantName,
      body: {
        schema: 'BlueprintLogicSpec.v1',
        statements: [statement],
      },
    });
    return entries;
  });
}

export function writeGraphWriteGeneralitySpecs(input: {
  assetPath: string;
  graphName: string;
  outDir: string;
  operationIds?: readonly string[];
}): string[] {
  mkdirSync(input.outDir, { recursive: true });
  const files: string[] = [];
  for (const bundle of makeGraphWriteGeneralityBundles(input)) {
    const operationDir = join(input.outDir, graphWriteOperationKey(bundle.operation.operationId));
    mkdirSync(operationDir, { recursive: true });
    const setupDir = join(operationDir, 'setup');
    mkdirSync(setupDir, { recursive: true });
    bundle.setupSpecs.forEach((spec, index) => {
      const file = join(setupDir, `${String(index + 1).padStart(2, '0')}_${index === 0 ? 'create_asset' : 'setup_fixture'}.json`);
      writeJson(file, spec);
      files.push(file);
    });
    const graphWriteFile = join(operationDir, 'graph_write.json');
    writeJson(graphWriteFile, bundle.graphWriteSpec);
    writeJson(join(operationDir, 'expected_variants.json'), {
      operation: bundle.operation,
      assetPath: bundle.assetPath,
      graphName: bundle.graphName,
      expectedVariantNames: bundle.expectedVariantNames,
      expectedNodeCandidates: bundle.expectedNodeCandidates,
      expectedReadback: bundle.expectedReadback,
    });
    files.push(graphWriteFile);
  }
  return files;
}

function makeOperationFilter(operationIds: readonly string[]): Set<string> {
  return new Set(operationIds
    .map((operationId) => operationId.trim())
    .filter((operationId) => operationId.length > 0)
    .flatMap((operationId) => [operationId, graphWriteOperationKey(operationId)]));
}

function makeOperationAssetPath(baseAssetPath: string, operationKey: string): string {
  const slashIndex = baseAssetPath.lastIndexOf('/');
  const directory = slashIndex >= 0 ? baseAssetPath.slice(0, slashIndex) : '/Game/BlueprintHelper/Generality';
  const baseName = slashIndex >= 0 ? baseAssetPath.slice(slashIndex + 1) : baseAssetPath;
  const shortKey = operationKey.length > 48 ? `${operationKey.slice(0, 48)}_${stableHash(operationKey)}` : operationKey;
  const maxAssetNameLength = 96;
  const suffix = `_${shortKey}`;
  const safeBaseName = baseName.replace(/[^A-Za-z0-9_]/g, '_');
  const runMarker = safeBaseName.length + suffix.length > maxAssetNameLength
    ? `_r${stableHash(baseName)}`
    : '';
  const prefixBudget = Math.max(16, maxAssetNameLength - suffix.length - runMarker.length);
  const assetName = `${safeBaseName.slice(0, prefixBudget)}${runMarker}${suffix}`;
  return `${directory}/${assetName}`;
}

function stableHash(value: string): string {
  let hash = 5381;
  for (let index = 0; index < value.length; index += 1) {
    hash = ((hash << 5) + hash) ^ value.charCodeAt(index);
  }
  return (hash >>> 0).toString(36).slice(0, 6);
}

function makeStatementForOperation(
  operation: GraphWriteGeneralityOperation,
  variantName: string,
  index: number,
  nodeCandidate: string,
  assetPath: string,
): JsonRecord {
  const evidence = makeEvidence(operation, variantName, index);
  const id = operation.operationId;
  if (id === 'function_action.call_function') {
    return callStatement(variantName, evidence, nodeCandidate);
  }
  if (id === 'function_action.macro_like') {
    return macroLikeStatement(nodeCandidate, variantName, index, evidence);
  }
  if (id === 'event.custom_event') {
    return callStatement(variantName, evidence, debugFunctionTargets()[index % debugFunctionTargets().length]);
  }
  if (id === 'field.field_access') {
    return fieldAccessStatement(nodeCandidate, variantName, index, evidence);
  }
  if (id === 'field.component_ref') {
    const target = nodeCandidate === 'component_ref_trigger_box' ? 'TriggerBox' : 'SceneRoot';
    return { kind: 'let', name: `${variantName}_component`, value: { kind: 'field', field_operation: 'get', field_scope: 'component_ref', target, context_evidence: evidence } };
  }
  if (id === 'field.struct_member_set') {
    const structEvidence = {
      ...evidence,
      'generic.struct.operation': 'set_fields_in_struct',
      'generic.struct.struct_path': '/Script/CoreUObject.Rotator',
      'generic.struct.selected_field_paths': 'Roll',
    };
    return {
      kind: 'field',
      field_operation: 'set',
      field_scope: 'field_access',
      target: 'GWGenRotator',
      property_path: 'Roll',
      capability_id: 'field.struct_member_set',
      capability_facts: {
        'field.struct_type': '/Script/CoreUObject.Rotator',
        'field.property_path': 'Roll',
        'generic.struct.struct_path': '/Script/CoreUObject.Rotator',
        'generic.struct.selected_field_paths': 'Roll',
      },
      value: numberLiteral(index),
      context_evidence: structEvidence,
    };
  }
  if (id.startsWith('container.')) {
    return containerStatement(id, variantName, index, evidence);
  }
  if (id.startsWith('generic_ops.control.')) {
    return controlStatement(id, variantName, index, evidence);
  }
  if (id.startsWith('generic_ops.transform.')) {
    if (id === 'generic_ops.transform.type_promotion') {
      return {
        kind: 'convert',
        transform_operation: 'type_promotion',
        args: { value: numberLiteral(index + 1) },
        context_evidence: {
          ...evidence,
          type_promotion_stable_id: 'type_promotion:Add:int:real',
          type_promotion_operator: 'Add',
          type_promotion_source_pin_type: { category: 'int' },
          type_promotion_target_pin_type: { category: 'real' },
          type_promotion_result_pin_type: { category: 'real' },
        },
      };
    }
    return { kind: 'convert', transform_operation: id.split('.').at(-1), target_class_path: '/Script/Engine.Actor', args: { value: { kind: 'literal', value_type: 'object', value: 'Self' } }, context_evidence: evidence };
  }
  if (id.startsWith('generic_ops.create.')) {
    return createStatement(id, index, evidence);
  }
  if (id === 'generic_ops.struct_select.make_struct') {
    return { kind: 'let', name: `${variantName}_struct`, value: vectorConstruct(index, structEvidence('/Script/CoreUObject.Vector', 'X,Y,Z', evidence)) };
  }
  if (id === 'generic_ops.struct_select.break_struct') {
    const evidenceForStruct = structEvidence('/Script/CoreUObject.Vector', 'X,Y,Z', evidence);
    return {
      kind: 'let',
      name: `${variantName}_break`,
      value: {
        kind: 'deconstruct',
        type: '/Script/CoreUObject.Vector',
        fields: ['X', 'Y', 'Z'],
        value: vectorConstruct(index, evidenceForStruct),
        context_evidence: evidenceForStruct,
      },
    };
  }
  if (id === 'generic_ops.struct_select.select') {
    return {
      kind: 'let',
      name: `${variantName}_select`,
      value: {
        kind: 'select',
        condition: boolLiteral(index % 2 === 0),
        options: [stringLiteral(`${variantName}_A`), stringLiteral(`${variantName}_B`)],
        context_evidence: {
          ...evidence,
          'generic.select.result_type_proof': {
            pin_type: { category: 'string' },
          },
        },
      },
    };
  }
  if (id.startsWith('op.')) {
    if (id === 'op.intpoint_equal') {
      return {
        kind: 'let',
        name: `${variantName}_op`,
        value: {
          kind: 'op',
          op: 'intpoint_equal',
          left: intPointConstruct(index, index + 1),
          right: intPointConstruct(index, index + 1),
          context_evidence: evidence,
        },
      };
    }
    return { kind: 'let', name: `${variantName}_op`, value: { kind: 'op', op: id.slice(3), left: numberLiteral(index + 1), right: numberLiteral(index + 2), context_evidence: evidence } };
  }
  if (id.startsWith('event_delegate.')) {
    return delegateStatement(id, variantName, evidence, assetPath);
  }
  if (id === 'create.asset_action') {
    return { kind: 'create', create_operation: 'asset_action', class_path: '/Script/Engine.Actor', context_evidence: evidence };
  }
  if (id === 'schedule.timer_delegate_node' || id === 'schedule.latent_or_async_node') {
    const scheduleOperation = id.split('.').at(-1);
    const contextEvidence = { ...evidence };
    if (scheduleOperation === 'timer_delegate_node') {
      Object.assign(contextEvidence, scheduleHandlerEvidence(assetPath, delegateHandlerName(variantName)));
    }
    const statement: JsonRecord = {
      kind: 'schedule',
      schedule_operation: scheduleOperation,
      args: { time: numberLiteral(0.1 + index / 100) },
      context_evidence: contextEvidence,
    };
    if (scheduleOperation === 'latent_or_async_node') {
      statement.graph_latent_allowed = true;
      contextEvidence.graph_latent_allowed = 'true';
    }
    return statement;
  }
  return callStatement(variantName, evidence);
}

function makeEvidence(operation: GraphWriteGeneralityOperation, variantName: string, index: number): JsonRecord {
  const evidence: JsonRecord = {
    'graphwrite_generality.operation_id': operation.operationId,
    'graphwrite_generality.variant_name': variantName,
  };
  for (const key of operation.requiredEvidenceKeys) {
    evidence[key] = defaultEvidenceValue(key, operation.operationId, index);
  }
  return evidence;
}

function defaultEvidenceValue(key: string, operationId: string, index: number): string | JsonRecord {
  const suffix = `${graphWriteOperationKey(operationId)}_${index}`;
  if (key.includes('node_class')) return '/Script/BlueprintGraph.K2Node_CallFunction';
  if (key.includes('stable') || key.includes('signature') || key.includes('evidence_id')) return `general:${suffix}`;
  if (key.includes('class_path') || key.includes('owner_path')) return '/Script/Engine.Actor';
  if (key.includes('pin_type') || key.includes('return')) return { category: 'real' };
  if (key.includes('case_values')) return '0,1,2';
  if (key.includes('enum_path')) return '/Script/Engine.EInputEvent';
  if (key.includes('default_policy')) return 'include_default';
  if (key.includes('dynamic_output_count')) return '2';
  if (key.includes('macro.graph_path')) return '/Engine/EditorBlueprintResources/StandardMacros.StandardMacros';
  if (key.includes('pin_shape_snapshot')) return 'general_shape';
  if (key.includes('delegate_property_name')) return 'OnActorBeginOverlap';
  if (key.includes('handler')) return 'GWGenHandler';
  if (key.includes('binding_object')) return 'self';
  return `general:${suffix}`;
}

function callStatement(variantName: string, evidence: JsonRecord, target = '/Script/Engine.KismetSystemLibrary:PrintString'): JsonRecord {
  const statement: JsonRecord = {
    kind: 'call',
    target,
    context_evidence: evidence,
  };
  if (target.endsWith(':PrintString') || target === 'PrintString') {
    statement.args = { InString: stringLiteral(variantName) };
  } else if (target.endsWith(':PrintText') || target === 'PrintText') {
    statement.args = { InText: stringLiteral(variantName) };
  }
  return statement;
}

function debugFunctionTargets(): readonly string[] {
  return [
    '/Script/Engine.KismetSystemLibrary:PrintString',
    '/Script/Engine.KismetSystemLibrary:PrintText',
    '/Script/Engine.KismetSystemLibrary:DrawDebugLine',
    '/Script/Engine.KismetSystemLibrary:DrawDebugPoint',
    '/Script/Engine.KismetSystemLibrary:DrawDebugSphere',
    '/Script/Engine.KismetSystemLibrary:DrawDebugBox',
    '/Script/Engine.KismetSystemLibrary:DrawDebugCapsule',
    '/Script/Engine.KismetSystemLibrary:DrawDebugArrow',
    '/Script/Engine.KismetSystemLibrary:DrawDebugCoordinateSystem',
    '/Script/Engine.KismetSystemLibrary:FlushPersistentDebugLines',
  ];
}

function fieldAccessStatement(candidate: string, variantName: string, index: number, evidence: JsonRecord): JsonRecord {
  const setValueByTarget: Record<string, JsonRecord> = {
    GWGenBool: boolLiteral(index % 2 === 0),
    GWGenInt: numberLiteral(index),
    GWGenFloat: numberLiteral(index + 0.5),
    GWGenString: stringLiteral(variantName),
  };
  if (candidate.startsWith('variable_get_')) {
    const target = fieldCandidateTarget(candidate);
    return {
      kind: 'let',
      name: `${variantName}_value`,
      value: { kind: 'field', field_operation: 'get', field_scope: 'variable', target, context_evidence: evidence },
    };
  }
  const target = fieldCandidateTarget(candidate);
  return {
    kind: 'field',
    field_operation: 'set',
    field_scope: 'variable',
    target,
    value: setValueByTarget[target] ?? numberLiteral(index),
    context_evidence: evidence,
  };
}

function fieldCandidateTarget(candidate: string): string {
  if (candidate.endsWith('_bool')) return 'GWGenBool';
  if (candidate.endsWith('_int')) return 'GWGenInt';
  if (candidate.endsWith('_float')) return 'GWGenFloat';
  if (candidate.endsWith('_string')) return 'GWGenString';
  return 'GWGenInt';
}

function macroLikeStatement(candidate: string, variantName: string, index: number, evidence: JsonRecord): JsonRecord {
  if (candidate === 'sequence' || candidate === 'branch' || candidate === 'multi_gate') {
    return controlStatement(`generic_ops.control.${candidate}`, variantName, index, evidence);
  }
  return controlStatement(`generic_ops.control.${candidate}`, variantName, index, {
    ...evidence,
    'generic.control.operation': candidate,
    'generic.macro.graph_path': standardMacroGraphPath(candidate),
    'generic.macro.pin_shape_snapshot': `macro:${candidate}`,
  });
}

function standardMacroGraphPath(operation: string): string {
  const graphByOperation: Record<string, string> = {
    do_once: 'DoOnce',
    do_n: 'Do N',
    gate: 'Gate',
    flip_flop: 'FlipFlop',
    for_loop: 'ForLoop',
    for_loop_with_break: 'ForLoopWithBreak',
    foreach_loop: 'ForEachLoop',
    foreach_loop_with_break: 'ForEachLoopWithBreak',
    while_loop: 'WhileLoop',
  };
  return `/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:${graphByOperation[operation] ?? operation}`;
}

function containerStatement(operationId: string, variantName: string, index: number, evidence: JsonRecord): JsonRecord {
  const [, containerKind, containerOperation] = operationId.split('.');
  const base: JsonRecord = {
    kind: 'container_action',
    container_kind: containerKind,
    container_operation: containerOperation,
    target: { kind: 'get', name: containerTargetName(containerKind) },
    element_type: 'int',
    key_type: 'string',
    value_type: 'int',
    context_evidence: evidence,
  };
  const roles = roleValues(containerKind, variantName, index);
  for (const role of getRequiredContainerActionRoles(containerKind, containerOperation)) {
    if (role === 'target') {
      continue;
    }
    base[role] = roles[role];
  }
  return base;
}

function containerTargetName(containerKind: string): string {
  if (containerKind === 'map') return 'GWGenStringIntMap';
  if (containerKind === 'set') return 'GWGenIntSet';
  return 'GWGenIntArray';
}

function roleValues(containerKind: string, variantName: string, index: number): JsonRecord {
  return {
    item: numberLiteral(index),
    items: [{ kind: 'get', name: 'GWGenOtherIntArray' }],
    key: stringLiteral(`${variantName}_key`),
    value: numberLiteral(index),
    index: numberLiteral(index),
    other: { kind: 'get', name: containerKind === 'set' ? 'GWGenOtherIntSet' : 'GWGenOtherIntArray' },
    result: { kind: 'get', name: containerKind === 'set' ? 'GWGenIntSet' : 'GWGenIntArray' },
    size: numberLiteral(index + 1),
    first_index: numberLiteral(0),
    second_index: numberLiteral(1),
    random_stream: { kind: 'get', name: 'GWGenRandomStream' },
    filter_class: classLiteral('/Script/Engine.Actor'),
  };
}

function controlStatement(operationId: string, variantName: string, index: number, evidence: JsonRecord): JsonRecord {
  const control = operationId.split('.').at(-1) ?? 'sequence';
  const controlEvidence = controlEvidenceForOperation(control, evidence);
  if (control === 'branch') {
    return {
      kind: 'control',
      control,
      condition: boolLiteral(index % 2 === 0),
      then: [callStatement(`${variantName}_then`, evidence)],
      else: [callStatement(`${variantName}_else`, evidence)],
      context_evidence: controlEvidence,
    };
  }
  if (control === 'sequence' || control === 'return') {
    return { kind: 'control', control, value: stringLiteral(variantName), context_evidence: controlEvidence };
  }
  return { kind: 'control', control, args: { value: numberLiteral(index) }, context_evidence: controlEvidence };
}

function controlEvidenceForOperation(control: string, evidence: JsonRecord): JsonRecord {
  const controlEvidence: JsonRecord = { ...evidence, 'generic.control.operation': control };
  if (control === 'multi_gate' && typeof controlEvidence['generic.control.dynamic_output_count'] !== 'string') {
    controlEvidence['generic.control.dynamic_output_count'] = '2';
  }
  if (isStandardMacroControl(control)) {
    controlEvidence['generic.macro.graph_path'] = standardMacroGraphPath(control);
    if (typeof controlEvidence['generic.macro.pin_shape_snapshot'] !== 'string'
      || controlEvidence['generic.macro.pin_shape_snapshot'].length === 0) {
      controlEvidence['generic.macro.pin_shape_snapshot'] = `macro:${control}`;
    }
  }
  return controlEvidence;
}

function isStandardMacroControl(control: string): boolean {
  return [
    'do_once',
    'do_n',
    'gate',
    'flip_flop',
    'for_loop',
    'for_loop_with_break',
    'foreach_loop',
    'foreach_loop_with_break',
    'while_loop',
  ].includes(control);
}

function createStatement(operationId: string, index: number, evidence: JsonRecord): JsonRecord {
  const createOperation = operationId.split('.').at(-1) ?? 'make_array';
  const statement: JsonRecord = { kind: 'create', create_operation: createOperation, context_evidence: evidence, args: { value: numberLiteral(index) } };
  if (createOperation === 'spawn_actor' || createOperation === 'construct_object' || createOperation === 'create_widget') {
    statement.class_path = createOperation === 'create_widget' ? '/Script/UMG.UserWidget' : '/Script/Engine.Actor';
  }
  if (createOperation === 'make_array') statement.pin_type = { category: 'int' };
  if (createOperation === 'make_set') statement.pin_type = { category: 'int' };
  if (createOperation === 'make_map') {
    statement.pin_type = {
      category: 'string',
      container_type: 'map',
      value_type: { category: 'int' },
    };
    statement.key_pin_type = { category: 'string' };
    statement.value_pin_type = { category: 'int' };
  }
  return statement;
}

function delegateStatement(operationId: string, variantName: string, evidence: JsonRecord, assetPath: string): JsonRecord {
  const contextEvidence = generalityEvidence(evidence);
  const handler = delegateHandlerName(variantName);
  if (operationId === 'event_delegate.component_bound_event') {
    return {
      kind: 'component_bound_event',
      component: 'TriggerBox',
      delegate: 'OnComponentBeginOverlap',
      handler,
      context_evidence: delegateHandlerEvidence(contextEvidence, assetPath, handler),
    };
  }
  const delegateOperation = operationId.replace('event_delegate.', '');
  if (delegateOperation === 'delegate.clear') {
    return {
      kind: 'delegate.unbind_all',
      target: 'TriggerBox',
      delegate: 'OnComponentBeginOverlap',
      args: {},
      context_evidence: contextEvidence,
    };
  }
  return {
    kind: delegateOperation,
    target: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
    handler,
    args: {},
    context_evidence: delegateHandlerEvidence(contextEvidence, assetPath, handler),
  };
}

function delegateHandlerName(variantName: string): string {
  return `Handle_${variantName}`;
}

function makeOverlapHandlerEntry(name: string): JsonRecord {
  return {
    entry_type: 'custom_event',
    name,
    inputs: [
      { name: 'OverlappedComponent', pin_type: { category: 'object', object_path: '/Script/Engine.PrimitiveComponent' } },
      { name: 'OtherActor', pin_type: { category: 'object', object_path: '/Script/Engine.Actor' } },
      { name: 'OtherComp', pin_type: { category: 'object', object_path: '/Script/Engine.PrimitiveComponent' } },
      { name: 'OtherBodyIndex', pin_type: { category: 'int' } },
      { name: 'bFromSweep', pin_type: { category: 'bool' } },
      { name: 'SweepResult', pin_type: { category: 'struct', object_path: '/Script/Engine.HitResult', is_reference: true, is_const: true } },
    ],
    body: {
      schema: 'BlueprintLogicSpec.v1',
      statements: [callStatement(`${name}_body`, {})],
    },
  };
}

function makeTimerHandlerEntry(name: string): JsonRecord {
  return {
    entry_type: 'custom_event',
    name,
    body: {
      schema: 'BlueprintLogicSpec.v1',
      statements: [callStatement(`${name}_body`, {})],
    },
  };
}

function delegateHandlerEvidence(evidence: JsonRecord, assetPath: string, handler: string): JsonRecord {
  const handlerClassPath = skeletonClassPathForBlueprintAsset(assetPath);
  return {
    ...evidence,
    'event_delegate.handler_name': handler,
    'event_delegate.handler_scope_class_path': handlerClassPath,
    'event_delegate.handler_function_path': `${handlerClassPath}:${handler}`,
    'event_delegate.handler_source_cluster': 'BlueprintSignature',
    'event_delegate.signature_evidence_id': `signature:custom_event:${handler}`,
  };
}

function scheduleHandlerEvidence(assetPath: string, handler: string): JsonRecord {
  const handlerClassPath = skeletonClassPathForBlueprintAsset(assetPath);
  return {
    handler_name: handler,
    handler_function_path: `${handlerClassPath}:${handler}`,
    handler_source_cluster: 'BlueprintSignature',
    signature_evidence_id: `signature:custom_event:${handler}`,
  };
}

function skeletonClassPathForBlueprintAsset(assetPath: string): string {
  const cleanAssetPath = assetPath.trim().replace(/\/+$/, '');
  const slashIndex = cleanAssetPath.lastIndexOf('/');
  const assetName = slashIndex >= 0 ? cleanAssetPath.slice(slashIndex + 1) : cleanAssetPath;
  return `${cleanAssetPath}.SKEL_${assetName}_C`;
}

function generalityEvidence(evidence: JsonRecord): JsonRecord {
  return {
    'graphwrite_generality.operation_id': evidence['graphwrite_generality.operation_id'],
    'graphwrite_generality.variant_name': evidence['graphwrite_generality.variant_name'],
  };
}

function structEvidence(structPath: string, selectedFields: string, evidence: JsonRecord): JsonRecord {
  return {
    ...evidence,
    'generic.struct.struct_path': structPath,
    'generic.struct.selected_field_paths': selectedFields,
  };
}

function vectorConstruct(index: number, evidence: JsonRecord): JsonRecord {
  return {
    kind: 'construct',
    type: '/Script/CoreUObject.Vector',
    fields: {
      X: numberLiteral(index),
      Y: numberLiteral(index + 1),
      Z: numberLiteral(index + 2),
    },
    context_evidence: evidence,
  };
}

function intPointConstruct(x: number, y: number): JsonRecord {
  return {
    kind: 'construct',
    type: '/Script/CoreUObject.IntPoint',
    fields: {
      X: numberLiteral(x),
      Y: numberLiteral(y),
    },
  };
}

function stringLiteral(value: string): JsonRecord {
  return { kind: 'literal', value_type: 'string', value };
}

function classLiteral(classPath: string): JsonRecord {
  return { kind: 'literal', value_type: '/Script/CoreUObject.Class', value: classPath };
}

function numberLiteral(value: number): JsonRecord {
  return { kind: 'literal', value_type: 'number', value };
}

function boolLiteral(value: boolean): JsonRecord {
  return { kind: 'literal', value_type: 'bool', value };
}

function writeJson(file: string, value: unknown): void {
  writeFileSync(file, `${JSON.stringify(value, null, 2)}\n`, 'utf8');
}
