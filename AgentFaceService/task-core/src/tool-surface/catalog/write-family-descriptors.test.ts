import assert from 'node:assert/strict';
import test from 'node:test';

import { getAgentVisibleGraphWriteRoutes } from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import {
  getActiveWriteFamilyDescriptors,
  getAgentVisibleWriteFamilyDescriptors,
  requireWriteFamilyDescriptor,
  resolveRuntimeAdapterIdsForDescriptor,
} from './write-family-descriptors.js';

const REQUIRED_WRITE_FAMILIES = [
  'graphwrite',
  'asset_factory',
  'blueprint_signature',
  'blueprint_variables',
  'class_settings',
  'blueprint_component',
  'object_property',
  'data_table',
  'umg_widget',
] as const;

type UERuntimeFamilyExpectation = {
  runtime_adapter_id: string;
  cluster_family: string;
  readback_projection_mode: string;
  metrics_identity: string;
};

const UE_RUNTIME_FAMILY_MATRIX: ReadonlyMap<string, UERuntimeFamilyExpectation> = new Map([
  ['graphwrite', {
    runtime_adapter_id: 'graphwrite',
    cluster_family: 'GraphWrite',
    readback_projection_mode: 'graph_body_adapter',
    metrics_identity: 'blueprint.write.graphwrite',
  }],
  ['asset_factory', {
    runtime_adapter_id: 'asset_factory',
    cluster_family: 'AssetFactory',
    readback_projection_mode: 'asset_factory',
    metrics_identity: 'blueprint.write.asset_factory',
  }],
  ['blueprint_signature', {
    runtime_adapter_id: 'blueprint_signature',
    cluster_family: 'Signature',
    readback_projection_mode: 'blueprint_signature',
    metrics_identity: 'blueprint.write.signature',
  }],
  ['blueprint_variables', {
    runtime_adapter_id: 'blueprint_variables',
    cluster_family: 'BlueprintVariables',
    readback_projection_mode: 'blueprint_variables',
    metrics_identity: 'blueprint.write.variables',
  }],
  ['class_settings', {
    runtime_adapter_id: 'class_settings',
    cluster_family: 'ClassSettings',
    readback_projection_mode: 'class_settings',
    metrics_identity: 'blueprint.write.class_settings',
  }],
  ['blueprint_component', {
    runtime_adapter_id: 'blueprint_component',
    cluster_family: 'Component',
    readback_projection_mode: 'blueprint_component',
    metrics_identity: 'blueprint.write.component',
  }],
  ['object_property', {
    runtime_adapter_id: 'object_property',
    cluster_family: 'ObjectProperty',
    readback_projection_mode: 'object_property',
    metrics_identity: 'blueprint.write.object_property',
  }],
  ['data_table', {
    runtime_adapter_id: 'data_table',
    cluster_family: 'DataTable',
    readback_projection_mode: 'data_table',
    metrics_identity: 'blueprint.write.data_table',
  }],
  ['umg_widget', {
    runtime_adapter_id: 'umg_widget',
    cluster_family: 'UMGWidget',
    readback_projection_mode: 'widget_tree',
    metrics_identity: 'umg.write.umg_widget',
  }],
] as const);

test('write family descriptor catalog includes every active family from the architecture closure plan', () => {
  const activeFamilies = new Set(
    getActiveWriteFamilyDescriptors().map((descriptor) => descriptor.write_family),
  );

  assert.deepEqual([...activeFamilies].sort(), [...REQUIRED_WRITE_FAMILIES].sort());
});

test('active write family descriptors declare runtime, bridge, metrics, and dry-run identities', () => {
  for (const descriptor of getActiveWriteFamilyDescriptors()) {
    assert.equal(descriptor.runtime_adapter_id.length > 0, true, `${descriptor.write_family} runtime_adapter_id`);
    assert.equal(descriptor.bridge_command.length > 0, true, `${descriptor.write_family} bridge_command`);
    assert.equal(descriptor.metrics_identity.length > 0, true, `${descriptor.write_family} metrics_identity`);
    assert.equal(descriptor.dry_run_policy_id.length > 0, true, `${descriptor.write_family} dry_run_policy_id`);
  }
});

test('agent-visible write family descriptors include only active families', () => {
  assert.deepEqual(
    getAgentVisibleWriteFamilyDescriptors().map((descriptor) => descriptor.write_family).sort(),
    [...REQUIRED_WRITE_FAMILIES].sort(),
  );
  assert.equal(
    getAgentVisibleWriteFamilyDescriptors().every((descriptor) => descriptor.status === 'active'),
    true,
    'reserved, hidden, and planned families must not be agent-visible',
  );
});

test('task-core write family descriptors mirror UE runtime family identifiers', () => {
  for (const descriptor of getActiveWriteFamilyDescriptors()) {
    const expected = UE_RUNTIME_FAMILY_MATRIX.get(descriptor.write_family);
    assert.ok(expected, `${descriptor.write_family} must be in the UE runtime family matrix`);
    assert.equal(descriptor.runtime_adapter_id, expected.runtime_adapter_id, `${descriptor.write_family} runtime_adapter_id`);
    assert.equal(descriptor.cluster_family, expected.cluster_family, `${descriptor.write_family} cluster_family`);
    assert.equal(descriptor.readback_projection_mode, expected.readback_projection_mode, `${descriptor.write_family} readback_projection_mode`);
    assert.equal(descriptor.metrics_identity, expected.metrics_identity, `${descriptor.write_family} metrics_identity`);
  }
});

test('graphwrite active descriptor relates to active GraphWrite route runtime adapters', () => {
  const descriptor = requireWriteFamilyDescriptor('graphwrite');
  const activeRouteRuntimeAdapterIds = new Set(
    getAgentVisibleGraphWriteRoutes().map((route) => route.runtime_adapter_id),
  );

  assert.equal(activeRouteRuntimeAdapterIds.size > 0, true, 'expected at least one active GraphWrite route');

  const relatedRuntimeAdapterIds = resolveRuntimeAdapterIdsForDescriptor(descriptor);
  assert.equal(relatedRuntimeAdapterIds.length > 0, true, 'graphwrite descriptor must resolve at least one runtime adapter id');
  assert.equal(
    relatedRuntimeAdapterIds.some((runtimeAdapterId) => activeRouteRuntimeAdapterIds.has(runtimeAdapterId)),
    true,
    'graphwrite descriptor must relate to an active GraphWrite route runtime_adapter_id',
  );
});
