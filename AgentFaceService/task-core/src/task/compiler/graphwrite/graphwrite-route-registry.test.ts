import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  GRAPHWRITE_ROUTE_MANIFEST_GENERATED_FROM,
} from './generated/graphwrite-route-manifest.generated.js';
import {
  getAgentVisibleGraphWriteRoutes,
  getAllGraphWriteRoutes,
  getGraphWriteRoutesForTemplateDiscovery,
  getGraphWriteRequiredFieldByStrategy,
  getGraphWriteRouteById,
  requireGraphWriteRouteByScope,
} from './graphwrite-route-registry.js';

const ACTIVE_ADAPTER_SYNC = 'active_requires_registered_non_reserved_adapter';
const RESERVED_ADAPTER_SYNC = 'reserved_hidden_from_agent';
const DEPRECATED_GENERATED_ACTIVE_STUB_SYNC = ['generated', 'active', 'stub'].join('_');

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
    assert.equal(syncRoute?.adapter_sync, adapterSyncOfDescriptor(route));
  }
});

test('GraphWrite route source owns explicit adapter sync semantics', () => {
  const source = readRouteSource();

  assert.equal(GRAPHWRITE_ROUTE_MANIFEST_GENERATED_FROM, 'src/task/compiler/graphwrite/graphwrite-route-source.json');
  assert.deepEqual(
    source.routes.map((route) => route.route_id).sort(),
    getAllGraphWriteRoutes().map((route) => route.route_id).sort(),
  );

  for (const route of source.routes) {
    const adapterSync = adapterSyncOfDescriptor(route);
    assert.notEqual(
      adapterSync,
      DEPRECATED_GENERATED_ACTIVE_STUB_SYNC,
      `${route.route_id} source must not use deprecated active stub adapter sync`,
    );

    if (route.status === 'active') {
      assert.equal(adapterSync, ACTIVE_ADAPTER_SYNC, `${route.route_id} source active adapter sync semantics`);
      assert.notEqual(route.runtime_adapter_id, '', `${route.route_id} source active runtime adapter id`);
    } else {
      assert.equal(adapterSync, RESERVED_ADAPTER_SYNC, `${route.route_id} source reserved adapter sync semantics`);
    }
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

test('replace_external_body route is descriptor-backed before composer exposure', () => {
  const route = getGraphWriteRouteById('graph.replace_external_body.body');

  assert.equal(route?.graph_strategy, 'replace_external_body');
  assert.equal(route?.runtime_adapter_id, 'k2.external_graph.replace_body');
  assert.equal(route?.adapter_sync, ACTIVE_ADAPTER_SYNC);
  assert.equal(route?.status, 'active');
  assert.equal(
    route?.template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/graph_replace_external_body_template.json',
  );
  assert.deepEqual(route?.insert_paths, ['behavior.external_replace.body.statements[]']);
});

test('external user graph patch routes are active and template-backed', () => {
  const expectedRoutes = [
    'graph.merge_external_flow.insert_between',
    'graph.patch_external_graph.node_comment',
    'graph.patch_external_graph.node_property',
    'graph.patch_external_graph.pin_default',
    'graph.patch_external_links.connect_pins',
    'graph.patch_external_links.disconnect_link',
    'graph.patch_external_links.insert_pure_resolver_between_data_link',
    'graph.patch_external_links.replace_link',
  ];
  const visibleRouteIds = new Set(getAgentVisibleGraphWriteRoutes().map((route) => route.route_id));

  for (const routeId of expectedRoutes) {
    const route = getGraphWriteRouteById(routeId);
    assert.equal(route?.status, 'active', routeId);
    assert.equal(route?.adapter_sync, ACTIVE_ADAPTER_SYNC, routeId);
    assert.equal(typeof route?.template_path, 'string', routeId);
    assert.equal(visibleRouteIds.has(routeId), true, routeId);
  }
});

test('agent-visible GraphWrite routes exactly match active runtime-backed route descriptors', () => {
  const expectedRouteIds = getAllGraphWriteRoutes()
    .filter((route) => (
      route.status === 'active'
      && adapterSyncOfDescriptor(route) === ACTIVE_ADAPTER_SYNC
      && route.template_path !== undefined
    ))
    .map((route) => route.route_id)
    .sort();
  const visibleRouteIds = getAgentVisibleGraphWriteRoutes().map((route) => route.route_id).sort();

  assert.deepEqual(visibleRouteIds, expectedRouteIds);
});

test('GraphWrite route descriptors declare explicit adapter sync semantics', () => {
  for (const route of getAllGraphWriteRoutes()) {
    const adapterSync = adapterSyncOfDescriptor(route);
    assert.notEqual(
      adapterSync,
      DEPRECATED_GENERATED_ACTIVE_STUB_SYNC,
      `${route.route_id} must not use deprecated active stub adapter sync`,
    );

    if (route.status === 'active') {
      assert.equal(adapterSync, ACTIVE_ADAPTER_SYNC, `${route.route_id} active adapter sync semantics`);
      assert.notEqual(route.runtime_adapter_id, '', `${route.route_id} active runtime adapter id`);
      continue;
    }

    assert.equal(adapterSync, RESERVED_ADAPTER_SYNC, `${route.route_id} reserved adapter sync semantics`);
  }
});

test('reserved GraphWrite routes stay hidden from template discovery', () => {
  const discoveryRouteIds = new Set(getGraphWriteRoutesForTemplateDiscovery().map((route) => route.route_id));
  const reservedRoutes = getAllGraphWriteRoutes().filter((route) => adapterSyncOfDescriptor(route) === RESERVED_ADAPTER_SYNC);

  assert.notEqual(reservedRoutes.length, 0, 'expected reserved GraphWrite routes in descriptor set');
  for (const route of reservedRoutes) {
    assert.equal(discoveryRouteIds.has(route.route_id), false, `${route.route_id} must stay out of template discovery`);
  }
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
  assert.equal(requiredFieldByStrategy.patch_external_links, 'external_link_patches');
  assert.equal(requiredFieldByStrategy.replace_external_body, 'external_replace');
});

test('GraphWrite routes declare explicit template write modes', () => {
  for (const route of getAllGraphWriteRoutes()) {
    assert.ok(
      ['graph.append', 'graph.replace', 'graph.merge', 'graph.patch'].includes(route.write_mode),
      `${route.route_id} has valid write_mode`,
    );
  }
});

test('generated UE adapter sync uses runtime requirement semantics, not active stubs', () => {
  const sync = readGeneratedAdapterSync('task-core');
  for (const route of sync.routes) {
    assert.notEqual(
      route.adapter_sync,
      DEPRECATED_GENERATED_ACTIVE_STUB_SYNC,
      `${route.route_id} must not use deprecated active stub adapter sync`,
    );
    if (route.status === 'active') {
      assert.equal(route.adapter_sync, ACTIVE_ADAPTER_SYNC, `${route.route_id} active sync semantics`);
      assert.notEqual(route.runtime_adapter_id, '', `${route.route_id} active runtime adapter id`);
    } else {
      assert.equal(route.adapter_sync, RESERVED_ADAPTER_SYNC, `${route.route_id} reserved sync semantics`);
    }
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

interface RouteSourceRoute {
  route_id: string;
  runtime_adapter_id: string;
  status: string;
  adapter_sync?: unknown;
}

type RouteSourceArtifact = { routes: RouteSourceRoute[] };

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

function readRouteSource(): RouteSourceArtifact {
  const sourcePath = path.resolve(
    taskCoreRoot(),
    'src',
    'task',
    'compiler',
    'graphwrite',
    'graphwrite-route-source.json',
  );
  return JSON.parse(fs.readFileSync(sourcePath, 'utf8')) as RouteSourceArtifact;
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

function adapterSyncOfDescriptor(route: { route_id: string; adapter_sync?: unknown }): string {
  if (typeof route.adapter_sync !== 'string') {
    assert.fail(`${route.route_id} must declare adapter_sync`);
  }
  return route.adapter_sync;
}

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
}

function pluginRoot(): string {
  return path.resolve(taskCoreRoot(), '..', '..');
}
