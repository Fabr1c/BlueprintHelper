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
  const sync = readGeneratedAdapterSync();
  const routeIds = getAllGraphWriteRoutes().map((route) => route.route_id).sort();
  const syncRouteIds = sync.routes.map((route) => route.route_id).sort();

  assert.deepEqual(syncRouteIds, routeIds);
  for (const route of getAllGraphWriteRoutes()) {
    const syncRoute = sync.routes.find((entry) => entry.route_id === route.route_id);
    assert.equal(syncRoute?.runtime_adapter_id, route.runtime_adapter_id);
    assert.equal(syncRoute?.status, route.status);
    assert.equal(syncRoute?.behavior_field, route.behavior_field);
  }
});

test('GraphWrite route registry exposes active routes and keeps planned routes hidden', () => {
  const visibleRouteIds = getAgentVisibleGraphWriteRoutes().map((route) => route.route_id);

  assert.equal(visibleRouteIds.includes('graph.replace.function_body'), true);
  assert.equal(visibleRouteIds.includes('graph.replace.macro_body'), false);
  assert.equal(getGraphWriteRouteById('graph.replace.macro_body')?.status, 'planned');
});

test('GraphWrite route registry resolves replace selector scopes from descriptors', () => {
  const route = requireGraphWriteRouteByScope('replace_owned_graph', 'function_body');

  assert.equal(route.route_id, 'graph.replace.function_body');
  assert.equal(route.selector?.expected_kind, 'function');
  assert.deepEqual(route.selector?.output_fields, { name: 'function_name' });
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

interface AdapterSyncRoute {
  route_id: string;
  runtime_adapter_id: string;
  behavior_field: string;
  status: string;
}

function readGeneratedAdapterSync(): { routes: AdapterSyncRoute[] } {
  const syncPath = path.resolve(
    taskCoreRoot(),
    'src',
    'task',
    'compiler',
    'graphwrite',
    'generated',
    'graphwrite-ue-adapter-sync.generated.json',
  );
  return JSON.parse(fs.readFileSync(syncPath, 'utf8')) as { routes: AdapterSyncRoute[] };
}

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
}
