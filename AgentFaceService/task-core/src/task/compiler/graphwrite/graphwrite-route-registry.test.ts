import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  getAgentVisibleGraphWriteRoutes,
  getAllGraphWriteRoutes,
  getGraphWriteRequiredFieldByStrategy,
  getGraphWriteRouteById,
  requireGraphWriteRouteByScope,
} from './graphwrite-route-registry.js';

test('GraphWrite route manifest stays synchronized with generated UE adapter sync artifact', () => {
  const sync = readGeneratedAdapterSync('task-core');
  const routeIds = getAllGraphWriteRoutes().map((route) => route.route_id).sort();
  const syncRouteIds = sync.routes.map((route) => route.route_id).sort();

  assert.deepEqual(syncRouteIds, routeIds);
  for (const route of getAllGraphWriteRoutes()) {
    const syncRoute = sync.routes.find((entry) => entry.route_id === route.route_id);
    assert.equal(syncRoute?.runtime_adapter_id, route.runtime_adapter_id);
    assert.equal(syncRoute?.status, route.status);
    assert.equal(syncRoute?.behavior_field, route.behavior_field);
    assert.equal(syncRoute?.adapter_sync, expectedAdapterSyncForStatus(route.status));
  }
});

test('GraphWrite generated adapter sync artifacts stay mirrored between TS and UE', () => {
  const taskCoreSync = normalizeGeneratedAdapterSync(readGeneratedAdapterSync('task-core'));
  const ueSync = normalizeGeneratedAdapterSync(readGeneratedAdapterSync('ue'));

  assert.deepEqual(ueSync, taskCoreSync);
});

test('GraphWrite route registry exposes active macro route after runtime adapter support', () => {
  const visibleRouteIds = getAgentVisibleGraphWriteRoutes().map((route) => route.route_id);

  assert.equal(visibleRouteIds.includes('graph.replace.function_body'), true);
  assert.equal(visibleRouteIds.includes('graph.replace.macro_body'), true);
  assert.equal(getGraphWriteRouteById('graph.replace.macro_body')?.runtime_adapter_id, 'k2.macro_body');
  assert.equal(getGraphWriteRouteById('graph.replace.macro_body')?.status, 'active');
});

test('GraphWrite route registry resolves replace selector scopes from descriptors', () => {
  const route = requireGraphWriteRouteByScope('replace_owned_graph', 'function_body');

  assert.equal(route.route_id, 'graph.replace.function_body');
  assert.equal(route.selector?.expected_kind, 'function');
  assert.deepEqual(route.selector?.output_fields, { name: 'function_name' });
  assert.equal(route.selector?.graph_name_output_field, 'function_name');

  const macroRoute = requireGraphWriteRouteByScope('replace_owned_graph', 'macro_body');
  assert.equal(macroRoute.selector?.graph_name_output_field, 'entry_name');
});

test('GraphWrite route registry derives required behavior fields from descriptors', () => {
  const requiredFieldByStrategy = getGraphWriteRequiredFieldByStrategy();

  assert.equal(requiredFieldByStrategy.append_new_owned_graph, 'entries');
  assert.equal(requiredFieldByStrategy.replace_owned_graph, 'replace');
  assert.equal(requiredFieldByStrategy.patch_owned_graph, 'patches');
  assert.equal(requiredFieldByStrategy.merge_owned_graph, 'merges');
  assert.equal(requiredFieldByStrategy.merge_external_flow, 'external_merges');
  assert.equal(requiredFieldByStrategy.patch_external_graph, 'external_patches');
  assert.equal(requiredFieldByStrategy.replace_external_body, 'external_replace');
});

test('generated UE adapter sync uses runtime requirement semantics, not active stubs', () => {
  const deprecatedActiveStubSync = ['generated', 'active', 'stub'].join('_');
  const sync = readGeneratedAdapterSync('task-core');
  for (const route of sync.routes) {
    assert.notEqual(
      route.adapter_sync,
      deprecatedActiveStubSync,
      `${route.route_id} must not use deprecated active stub adapter sync`,
    );
    assert.equal(
      route.adapter_sync,
      expectedAdapterSyncForStatus(route.status),
      `${route.route_id} adapter sync semantics`,
    );
  }
});

interface AdapterSyncRoute {
  route_id: string;
  runtime_adapter_id: string;
  graph_strategy: string;
  public_scope: string;
  behavior_field: string;
  compiler_id: string;
  taskplan_op: string;
  status: string;
  adapter_sync: string;
}

type AdapterSyncArtifact = { routes: AdapterSyncRoute[] };

type NormalizedAdapterSyncRoute = Pick<
  AdapterSyncRoute,
  'route_id' | 'runtime_adapter_id' | 'graph_strategy' | 'public_scope' | 'taskplan_op' | 'status' | 'adapter_sync'
>;

function readGeneratedAdapterSync(kind: 'task-core' | 'ue'): AdapterSyncArtifact {
  const syncPath = kind === 'task-core'
    ? path.resolve(
      taskCoreRoot(),
      'src',
      'task',
      'compiler',
      'graphwrite',
      'generated',
      'graphwrite-ue-adapter-sync.generated.json',
    )
    : path.resolve(
      pluginRoot(),
      'BlueprintHelper',
      'Source',
      'BlueprintHelper',
      'Private',
      'Generated',
      'BlueprintHelperGraphWriteRouteAdapterSync.generated.json',
    );
  return JSON.parse(fs.readFileSync(syncPath, 'utf8')) as AdapterSyncArtifact;
}

function normalizeGeneratedAdapterSync(sync: AdapterSyncArtifact): NormalizedAdapterSyncRoute[] {
  return sync.routes
    .map((route) => ({
      route_id: route.route_id,
      runtime_adapter_id: route.runtime_adapter_id,
      graph_strategy: route.graph_strategy,
      public_scope: route.public_scope,
      taskplan_op: route.taskplan_op,
      status: route.status,
      adapter_sync: route.adapter_sync,
    }))
    .sort((left, right) => left.route_id.localeCompare(right.route_id));
}

function expectedAdapterSyncForStatus(status: string): string {
  return status === 'active'
    ? 'active_requires_registered_non_reserved_adapter'
    : `${status}_route_not_agent_executable`;
}

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
}

function pluginRoot(): string {
  return path.resolve(taskCoreRoot(), '..', '..');
}
