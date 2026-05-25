import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import { GRAPHWRITE_CAPABILITY_CONTRACT } from './graphwrite-capability-contract.js';

describe('GraphWrite capability contract', () => {
  it('covers the four stability clusters', () => {
    const clusters = new Map(
      GRAPHWRITE_CAPABILITY_CONTRACT.clusters.map((cluster) => [cluster.id, cluster]),
    );

    assert.equal(GRAPHWRITE_CAPABILITY_CONTRACT.version, 1);
    assert.equal(GRAPHWRITE_CAPABILITY_CONTRACT.status, 'stable-candidate');

    for (const id of ['function_action', 'field', 'event', 'asset_action'] as const) {
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
  });

  it('keeps every operation explicit about support and review evidence', () => {
    for (const cluster of GRAPHWRITE_CAPABILITY_CONTRACT.clusters) {
      for (const operation of cluster.operations) {
        assert.notEqual(operation.supportStatus, 'unknown', `${cluster.id}.${operation.id} has unknown support`);
        assert.ok(operation.reviewEvidence, `${cluster.id}.${operation.id} lacks review evidence policy`);
      }
    }
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
});
