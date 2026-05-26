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
      assert.equal(operation.semanticKind, 'op');
      assert.equal(operation.semanticFamily, 'operator');
      assert.ok(operation.secondStageOperation?.startsWith('op.'));
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
