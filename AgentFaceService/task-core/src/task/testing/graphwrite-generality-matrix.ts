import {
  GRAPHWRITE_CAPABILITY_CONTRACT,
  type GraphWriteOperationContract,
  type GraphWriteOperationGroupOperation,
  type GraphWriteRuntimeOwner,
  type GraphWriteSupportStatus,
} from '../schema/graphwrite-capability-contract.js';

export type GraphWriteGeneralityVariantMode = 'parameterized_10' | 'limited_real_nodes' | 'singleton_1';

export interface GraphWriteGeneralityOperation {
  operationId: string;
  source: 'cluster' | 'operation_group';
  sourceId: string;
  runtimeOwner?: GraphWriteRuntimeOwner;
  semanticKind?: string;
  supportStatus: GraphWriteSupportStatus;
  variantMode: GraphWriteGeneralityVariantMode;
  attemptedVariantTarget: number;
  availableSpawnCount: number;
  requiredVariantCount: number;
  requiredEvidenceKeys: readonly string[];
  spawnCandidateNames: readonly string[];
}

const SINGLETON_OPERATION_IDS = new Set([
  'generic_ops.control.branch',
  'generic_ops.control.sequence',
  'generic_ops.control.return',
]);

const EXPLICIT_SPAWN_CANDIDATES = new Map<string, readonly string[]>([
  ['function_action.call_function', [
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
  ]],
  ['function_action.macro_like', [
    'sequence',
    'branch',
    'do_once',
    'do_n',
    'gate',
    'flip_flop',
    'for_loop',
    'for_loop_with_break',
    'while_loop',
    'multi_gate',
  ]],
  ['event.custom_event', [
    'custom_event_00',
    'custom_event_01',
    'custom_event_02',
    'custom_event_03',
    'custom_event_04',
    'custom_event_05',
    'custom_event_06',
    'custom_event_07',
    'custom_event_08',
    'custom_event_09',
  ]],
  ['field.field_access', [
    'variable_set_bool',
    'variable_set_int',
    'variable_set_float',
    'variable_set_string',
    'variable_get_bool',
    'variable_get_int',
    'variable_get_float',
    'variable_get_string',
  ]],
  ['field.component_ref', [
    'component_ref_scene_root',
    'component_ref_trigger_box',
  ]],
]);

export function graphWriteOperationKey(operationId: string): string {
  return operationId.replace(/[^A-Za-z0-9]+/g, '_').replace(/^_+|_+$/g, '');
}

export function graphWriteVariantNames(operation: GraphWriteGeneralityOperation): string[] {
  const key = graphWriteOperationKey(operation.operationId);
  return Array.from({ length: operation.requiredVariantCount }, (_, index) =>
    `GWGen_${key}_${String(index).padStart(2, '0')}`);
}

export function makeGraphWriteGeneralityOperations(): GraphWriteGeneralityOperation[] {
  const clusterOperations = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.flatMap((cluster) =>
    cluster.operations.map((operation) => fromClusterOperation(cluster.id, operation)));
  const groupOperations = GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups.flatMap((group) =>
    group.operations.map((operation) => fromGroupOperation(group.id, operation)));
  return [...clusterOperations, ...groupOperations]
    .filter((operation) => operation.supportStatus === 'supported')
    .sort((a, b) => a.operationId.localeCompare(b.operationId));
}

function fromClusterOperation(
  clusterId: string,
  operation: GraphWriteOperationContract,
): GraphWriteGeneralityOperation {
  const operationId = operation.id.includes('.')
    || operation.id.startsWith('schedule.')
    || operation.id.startsWith('create.')
    ? operation.id
    : `${clusterId}.${operation.id}`;
  return makeOperation({
    operationId,
    source: 'cluster',
    sourceId: clusterId,
    runtimeOwner: clusterRuntimeOwner(clusterId),
    semanticKind: operation.kind,
    supportStatus: operation.supportStatus,
    requiredEvidenceKeys: operation.requiredEvidenceKeys ?? [],
  });
}

function fromGroupOperation(
  groupId: string,
  operation: GraphWriteOperationGroupOperation,
): GraphWriteGeneralityOperation {
  const operationId = groupId === 'op_coverage' && operation.secondStageOperation
    ? operation.secondStageOperation
    : operation.id;
  return makeOperation({
    operationId,
    source: 'operation_group',
    sourceId: groupId,
    runtimeOwner: operation.runtimeOwner,
    semanticKind: operation.semanticKind,
    supportStatus: operation.supportStatus,
    requiredEvidenceKeys: operation.requiredEvidenceKeys ?? [],
  });
}

function makeOperation(input: Omit<GraphWriteGeneralityOperation, 'variantMode' | 'attemptedVariantTarget' | 'availableSpawnCount' | 'requiredVariantCount' | 'spawnCandidateNames'>): GraphWriteGeneralityOperation {
  const attemptedVariantTarget = SINGLETON_OPERATION_IDS.has(input.operationId) ? 1 : 10;
  const spawnCandidateNames = makeSpawnCandidateNames(input.operationId, attemptedVariantTarget);
  const availableSpawnCount = spawnCandidateNames.length;
  const variantMode: GraphWriteGeneralityVariantMode = SINGLETON_OPERATION_IDS.has(input.operationId)
    ? 'singleton_1'
    : availableSpawnCount >= attemptedVariantTarget
      ? 'parameterized_10'
      : 'limited_real_nodes';
  return {
    ...input,
    variantMode,
    attemptedVariantTarget,
    availableSpawnCount,
    requiredVariantCount: Math.min(attemptedVariantTarget, availableSpawnCount),
    spawnCandidateNames,
  };
}

function makeSpawnCandidateNames(operationId: string, attemptedVariantTarget: number): readonly string[] {
  const explicit = EXPLICIT_SPAWN_CANDIDATES.get(operationId);
  if (explicit) return explicit.slice(0, attemptedVariantTarget);
  return [operationId];
}

function clusterRuntimeOwner(clusterId: string): GraphWriteRuntimeOwner | undefined {
  if (clusterId === 'function_action' || clusterId === 'container_action') return 'FunctionAction';
  if (clusterId === 'asset_action' || clusterId === 'generic_schedule') return 'GenericAssetStructControlAction';
  return undefined;
}
