import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import { GRAPHWRITE_CAPABILITY_CONTRACT } from './graphwrite-capability-contract.js';

describe('GraphWrite capability contract', () => {
  it('covers the stability clusters', () => {
    const clusters = new Map(
      GRAPHWRITE_CAPABILITY_CONTRACT.clusters.map((cluster) => [cluster.id, cluster]),
    );

    assert.equal(GRAPHWRITE_CAPABILITY_CONTRACT.version, 1);
    assert.equal(GRAPHWRITE_CAPABILITY_CONTRACT.status, 'stable-candidate');

    for (const id of ['function_action', 'field', 'event', 'asset_action', 'container_action', 'generic_schedule'] as const) {
      assert.ok(clusters.has(id), `missing GraphWrite cluster contract: ${id}`);
    }
  });

  it('pins asset_action to UE ActionDatabase projection and execute-time revalidation', () => {
    const assetAction = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.find((cluster) => cluster.id === 'asset_action');

    assert.ok(assetAction);
    assert.ok(assetAction.operations.some((operation) => operation.id === 'create.asset_action'));
    assert.deepEqual(assetAction.evidence.requiredKeys, [
      'asset_action_stable_id',
      'asset_action_node_class',
      'asset_action_spawner_signature',
      'asset_action_owner_path',
    ]);
    assert.equal(assetAction.evidence.projectionSource, 'UE ActionDatabase');
    assert.equal(assetAction.executeRevalidation, 'required');
    assert.equal(
      assetAction.operations.find((operation) => operation.id === 'create.asset_action')?.reviewEvidence,
      'graph_surface_atomic_target',
    );
  });

  it('keeps every operation explicit about support and review evidence', () => {
    for (const cluster of GRAPHWRITE_CAPABILITY_CONTRACT.clusters) {
      for (const operation of cluster.operations) {
        assert.notEqual(operation.supportStatus, 'unknown', `${cluster.id}.${operation.id} has unknown support`);
        assert.ok(operation.reviewEvidence, `${cluster.id}.${operation.id} lacks review evidence policy`);
      }
    }
  });

  it('publishes first-class container_action operations with graph-level review evidence', () => {
    const containerAction = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.find((cluster) => cluster.id === 'container_action');

    assert.ok(containerAction);
    assert.equal(containerAction.executeRevalidation, 'not-required');
    assert.equal(containerAction.evidence.projectionSource, 'ActionContext');
    assert.ok(containerAction.operations.some((operation) => operation.id === 'container.array.add'));
    assert.ok(containerAction.operations.some((operation) => operation.id === 'container.map.contains'));
    assert.ok(containerAction.operations.some((operation) => operation.id === 'container.set.to_array'));
    assert.ok(containerAction.operations.every((operation) => operation.reviewEvidence === 'graph_surface_atomic_target'));
  });

  it('pins Generic schedule to projected spawner evidence and graph-level review evidence', () => {
    const genericSchedule = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.find((cluster) => cluster.id === 'generic_schedule');

    assert.ok(genericSchedule);
    assert.equal(genericSchedule.evidence.projectionSource, 'UE ActionDatabase');
    assert.equal(genericSchedule.executeRevalidation, 'required');
    assert.deepEqual(genericSchedule.evidence.requiredKeys, [
      'schedule_action_stable_id',
      'schedule_node_class',
      'schedule_spawner_signature',
      'schedule_owner_path',
    ]);
    assert.equal(
      genericSchedule.operations.find((operation) => operation.id === 'schedule.timer_delegate_node')?.reviewEvidence,
      'graph_surface_atomic_target',
    );
    assert.equal(
      genericSchedule.operations.find((operation) => operation.id === 'schedule.latent_or_async_node')?.reviewEvidence,
      'graph_surface_atomic_target',
    );
  });

  it('keeps discussion-gated event overlap out of supported GraphWrite ownership', () => {
    const eventCluster = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.find((cluster) => cluster.id === 'event');

    assert.ok(eventCluster);
    assert.equal(
      eventCluster.operations.find((operation) => operation.id === 'custom_event')?.supportStatus,
      'supported',
    );
    assert.equal(
      eventCluster.operations.find((operation) => operation.id === 'override_native_event')?.supportStatus,
      'discussion-gated',
    );
    assert.equal(
      eventCluster.operations.find((operation) => operation.id === 'delegate_component_bound_event')?.supportStatus,
      'discussion-gated',
    );
  });

  it('does not publish op coverage as a runtime graphwrite_op cluster', () => {
    const runtimeClusterIds: readonly string[] = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.map((cluster) => cluster.id);
    assert.ok(!runtimeClusterIds.includes('graphwrite_op'));

    const opGroup = GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups.find((group) => group.id === 'op_coverage');
    assert.ok(opGroup);
    for (const operation of opGroup.operations.filter((item) => item.supportStatus === 'supported')) {
      assert.equal(operation.runtimeCluster, 'FunctionAction');
      assert.equal(operation.runtimeOwner, 'FunctionAction');
      assert.equal(operation.semanticKind, 'op');
      assert.equal(operation.semanticFamily, 'operator');
      assert.ok(operation.secondStageOperation?.startsWith('op.'));
    }
  });

  it('does not publish GenericOps as new runtime clusters', () => {
    const runtimeClusterIds: readonly string[] = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.map((cluster) => cluster.id);
    for (const forbidden of ['control', 'generic_transform', 'generic_create', 'struct_select', 'generic_op']) {
      assert.equal(runtimeClusterIds.includes(forbidden), false, forbidden);
    }
  });

  it('publishes EventDelegate as a use-site logical operation group', () => {
    const group = GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups.find((item) => item.id === 'event_delegate');
    assert.ok(group);

    for (const op of ['component_bound_event', 'delegate.bind', 'delegate.assign', 'delegate.unbind', 'delegate.call', 'delegate.clear'] as const) {
      const operation: (typeof group.operations)[number] | undefined = group.operations.find((item) => item.id === `event_delegate.${op}`);
      assert.ok(operation, op);
      assert.equal(operation.supportStatus, 'supported');
      assert.equal(operation.runtimeOwner, 'EventDelegateAction');
      assert.equal(operation.runtimeCluster, 'EventDelegateAction');
      assert.ok(operation.semanticKind === 'component_bound_event' || operation.semanticKind === 'delegate');
      assert.ok(operation.requiredEvidenceKeys?.every((key: string) => key.startsWith('event_delegate.')));
    }

    assert.equal(
      group.operations.find((item) => item.id === 'event_delegate.component_bound_duplicate_policy.replace')?.rejectionReason,
      'duplicate_mutation_policy_blocked',
    );
    assert.equal(
      group.operations.find((item) => item.id === 'event_delegate.component_bound_duplicate_policy.merge')?.rejectionReason,
      'duplicate_mutation_policy_blocked',
    );
  });

  it('keeps EventDelegate side-effect assign policy out of the contract', () => {
    const serialized = JSON.stringify(GRAPHWRITE_CAPABILITY_CONTRACT);
    assert.equal(serialized.includes('assign_auto_attached_event_policy'), false);
    assert.equal(serialized.includes('attached_custom_event'), false);
    assert.equal(serialized.includes('ue_delegate_manual_assign_factory'), false);
  });

  it('publishes GenericOps logical groups with ownership-scoped operations', () => {
    const operationById = new Map(
      GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups
        .flatMap((group) => group.operations)
        .map((operation) => [operation.id, operation]),
    );

    function assertOperationOwner(id: string, runtimeOwner: 'FunctionAction' | 'GenericAssetStructControlAction') {
      const operation = operationById.get(id);
      assert.ok(operation, `missing operation: ${id}`);
      assert.equal(operation.supportStatus, 'supported');
      assert.equal(operation.runtimeOwner, runtimeOwner, id);
      assert.equal(operation.runtimeCluster, runtimeOwner, id);
      assert.ok(operation.semanticKind, `${id} missing semanticKind`);
      assert.ok(operation.semanticFamily, `${id} missing semanticFamily`);
      assert.ok(operation.secondStageOperation, `${id} missing secondStageOperation`);
      assert.ok(operation.requiredEvidenceKeys, `${id} missing evidence keys`);
    }

    assertOperationOwner('generic_ops.control.switch_enum', 'GenericAssetStructControlAction');
    assertOperationOwner('generic_ops.control.for_loop', 'GenericAssetStructControlAction');
    assertOperationOwner('generic_ops.container.array.add', 'FunctionAction');
    assertOperationOwner('generic_ops.transform.function_conversion', 'FunctionAction');
    assertOperationOwner('generic_ops.create.asset_action', 'GenericAssetStructControlAction');
    assertOperationOwner('generic_ops.create.function_backed_create', 'FunctionAction');
    assertOperationOwner('generic_ops.schedule.timer_by_handle', 'FunctionAction');
    assertOperationOwner('generic_ops.schedule.timer_delegate_node', 'GenericAssetStructControlAction');
    assertOperationOwner('generic_ops.struct_select.set_fields_in_struct', 'GenericAssetStructControlAction');

    assert.equal(
      operationById.get('generic_ops.struct_select.split_pin')?.rejectionReason,
      'split_pin_is_not_a_graphwrite_statement_operation',
    );
    assert.equal(
      operationById.get('generic_ops.struct_select.split_pin')?.excludedReason,
      'split_pin_is_not_a_graphwrite_statement_operation',
    );
    assert.equal(
      operationById.get('generic_ops.transform.link_time_auto_conversion')?.rejectionReason,
      'link_time_auto_conversion_requires_linker_readback',
    );
  });

  it('keeps OpCoverage disjoint from GenericOps logical groups', () => {
    const genericOps = new Set(
      GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups
        .filter((group) => group.id.startsWith('generic_ops.'))
        .flatMap((group) => group.operations.map((operation) => operation.id)),
    );
    const opCoverage = GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups.find((group) => group.id === 'op_coverage');
    assert.ok(opCoverage);
    for (const operation of opCoverage.operations) {
      assert.equal(genericOps.has(operation.id), false, operation.id);
      assert.equal(operation.id.startsWith('generic_ops.'), false, operation.id);
    }
  });

  it('publishes complete supported and excluded op coverage lists', () => {
    const opGroup = GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups.find((group) => group.id === 'op_coverage');
    assert.ok(opGroup);

    const supported = new Set(
      opGroup.operations
        .filter((operation) => operation.supportStatus === 'supported')
        .map((operation) => operation.id),
    );
    const expectedSupported = [
      'bitwise_and',
      'bitwise_or',
      'boolean_and',
      'boolean_or',
      'boolean_nand',
      'max',
      'min',
      'string_append',
      'boolean_not',
      'boolean_xor',
      'boolean_nor',
      'bitwise_not',
      'bitwise_xor',
      'abs',
      'modulo',
      'negate',
      'dot',
      'dot3',
      'cross',
      'cross3',
      'near_equal',
      'intpoint_equal',
      'transform_compose',
      'equal_exact',
      'not_equal_exact',
      'equal_ignore_case',
      'not_equal_ignore_case',
      'datetime_add_datetime',
      'datetime_add_timespan',
      'datetime_subtract_datetime',
      'datetime_subtract_timespan',
      'datetime_equal',
      'datetime_not_equal',
      'datetime_greater',
      'datetime_greater_equal',
      'datetime_less',
      'datetime_less_equal',
      'array_identical',
    ];
    assert.deepEqual([...supported].sort(), [...expectedSupported].sort());

    const excluded = new Map(
      opGroup.operations
        .filter((operation) => operation.supportStatus === 'rejected')
        .map((operation) => [operation.id, operation.rejectionReason]),
    );
    for (const id of [
      'enum_equal',
      'enum_not_equal',
      'slate_brush_equal',
      'slate_brush_not_equal',
      'convert_numeric',
      'convert_string_text_name',
      'array_map_set_mutation',
      'validity_predicate',
    ]) {
      assert.equal(excluded.get(id), 'excluded_op_operation');
    }
  });
});
