import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  getAllGraphWriteRoutes,
  getGraphWriteRoutesForTemplateDiscovery,
} from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import { getGraphWriteSlotsForRoute } from '../../task/compiler/graphwrite/graphwrite-slot-registry.js';
import {
  getRemovedDirectCliToolCommand,
  listToolCapabilities,
  listToolDomains,
} from '../tool-registry.js';
import { resolveResultProjectionPolicy } from '../result/result-projection-registry.js';
import { buildReadonlyToolCommandManifestRegistry } from './tool-command-manifest-builder.js';

const ACTIVE_GRAPHWRITE_ADAPTER_SYNC = 'active_requires_registered_non_reserved_adapter';
const FORBIDDEN_ACTIVE_ADAPTER_SYNC = new Set([
  'active_stub',
  'generated_active_stub',
  'stub',
  'temporary_active_stub',
  'todo',
]);

test('every public tool resolves through ToolCommandManifest registry', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  for (const capability of listDefaultPublicCapabilities()) {
    assert.equal(registry.has(capability.id), true, `${capability.id} must resolve through manifest registry`);
  }

  for (const alias of [
    'blueprinthelper_diagnostics',
  ]) {
    assert.ok(registry.require(alias), `${alias} must resolve through canonical manifest alias`);
  }

  for (const alias of [
    'blueprinthelper_preview_task',
    'blueprinthelper_execute_task',
    'blueprinthelper_get_task_result',
    'blueprinthelper_read_context',
  ]) {
    assert.ok(getRemovedDirectCliToolCommand(alias), `${alias} must be documented by removed direct command policy`);
  }
});

test('every public manifest resolves a handler id and result policy id', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  for (const manifest of registry.list()) {
    assert.equal(typeof manifest.handler_id, 'string', `${manifest.tool_id} must declare handler_id`);
    assert.equal(manifest.handler_id.length > 0, true, `${manifest.tool_id} must declare handler_id`);
    assert.equal(typeof manifest.result_policy_id, 'string', `${manifest.tool_id} must declare result_policy_id`);
    assert.equal(
      resolveResultProjectionPolicy({ manifestRegistry: registry, toolIdOrAlias: manifest.tool_id }).policy_id,
      resolveResultProjectionPolicy({ resultPolicyId: manifest.result_policy_id }).policy_id,
      `${manifest.tool_id} must resolve the policy declared by its manifest`,
    );
  }
});

test('every task tool manifest declares input shape adapters', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  for (const toolId of [
    'blueprint.plan.taskspec.preview',
    'blueprint.write.taskspec.execute',
    'project.read.task_result',
  ]) {
    const manifest = registry.require(toolId);
    assert.equal(manifest.input_shapes.length > 0, true, `${toolId} must declare input shapes`);
  }
});

test('manifest exposes only active public graph write route refs by default', () => {
  const manifest = buildReadonlyToolCommandManifestRegistry().require('blueprint.plan.taskspec.preview');
  const expectedRouteIds = getGraphWriteRoutesForTemplateDiscovery()
    .map((route) => route.route_id)
    .sort();
  const actualRouteIds = manifest.route_refs
    .filter((routeId) => routeId.startsWith('graph.'))
    .sort();

  assert.deepEqual(actualRouteIds, expectedRouteIds);
});

test('default template discovery excludes non-active graph write routes', () => {
  const manifest = buildReadonlyToolCommandManifestRegistry().require('blueprint.write.taskspec.execute');
  const defaultRouteIds = new Set(manifest.route_refs);
  const discoveredRouteIds = new Set(getGraphWriteRoutesForTemplateDiscovery().map((route) => route.route_id));
  const nonActiveRouteIds = getAllGraphWriteRoutes()
    .filter((route) => route.status !== 'active')
    .map((route) => route.route_id);

  assert.equal(nonActiveRouteIds.length > 0, true, 'non-active route fixture must exist for this guard');
  for (const routeId of nonActiveRouteIds) {
    assert.equal(defaultRouteIds.has(routeId), false, `${routeId} must not appear in default route discovery`);
    assert.equal(discoveredRouteIds.has(routeId), false, `${routeId} must not appear in template discovery`);
  }
});

test('route slot template refs resolve to existing template files', () => {
  for (const route of getGraphWriteRoutesForTemplateDiscovery()) {
    for (const slot of getGraphWriteSlotsForRoute(route.route_id)) {
      assert.equal(fs.existsSync(path.resolve(pluginRoot(), slot.template_path)), true, `${slot.slot_id} path must exist`);
      assert.equal(slot.supported_routes.includes(route.route_id), true, `${slot.slot_id} must apply to ${route.route_id}`);
    }
  }
});

test('metrics identities resolve for preview execute read diagnostics and GraphWrite routes', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  for (const alias of [
    'blueprint.plan.taskspec.preview',
    'blueprint.write.taskspec.execute',
    'blueprint.read.context.logic_flow',
    'blueprinthelper_diagnostics',
  ]) {
    const manifest = registry.require(alias);
    assert.equal(typeof manifest.metrics_identity?.capability, 'string', `${alias} must declare metrics capability`);
    assert.equal(typeof manifest.metrics_identity?.semantic_operation, 'string', `${alias} must declare metrics operation`);
  }

  assert.equal(
    getGraphWriteRoutesForTemplateDiscovery().some((route) => route.route_id === 'graph.replace.function_body'),
    true,
  );
});

test('active GraphWrite route runtime adapters resolve to generated TS and UE sync artifacts', () => {
  const activeRoutes = getGraphWriteRoutesForTemplateDiscovery();
  const taskCoreSync = readGraphWriteSyncArtifact(
    'AgentFaceService/task-core/src/task/compiler/graphwrite/generated/graphwrite-ue-adapter-sync.generated.json',
  );
  const ueSync = readGraphWriteSyncArtifact(
    'BlueprintHelper/Source/BlueprintHelper/Private/Generated/BlueprintHelperGraphWriteRouteAdapterSync.generated.json',
  );

  assert.equal(activeRoutes.length > 0, true, 'active GraphWrite routes must exist for this guard');
  for (const route of activeRoutes) {
    assert.equal(typeof route.runtime_adapter_id, 'string', `${route.route_id} must declare runtime_adapter_id`);
    assert.equal(route.runtime_adapter_id.trim().length > 0, true, `${route.route_id} must declare non-empty runtime_adapter_id`);
    assert.equal(route.adapter_sync, ACTIVE_GRAPHWRITE_ADAPTER_SYNC, `${route.route_id} must use active adapter sync policy`);
    assert.equal(
      FORBIDDEN_ACTIVE_ADAPTER_SYNC.has(route.adapter_sync.toLowerCase()),
      false,
      `${route.route_id} must not use temporary active adapter sync`,
    );
    assertGraphWriteSyncRoute(taskCoreSync, route.route_id, route.runtime_adapter_id);
    assertGraphWriteSyncRoute(ueSync, route.route_id, route.runtime_adapter_id);
  }
});

function listDefaultPublicCapabilities() {
  return listToolDomains().items.flatMap((domain) =>
    domain.default_kinds.flatMap((kind) =>
      listToolCapabilities({ domain: domain.id, kind }).items));
}

function pluginRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..', '..');
}

function readGraphWriteSyncArtifact(relativePath: string): Map<string, {
  adapterSync: string;
  runtimeAdapterId: string;
}> {
  const raw = fs.readFileSync(path.resolve(pluginRoot(), relativePath), 'utf8');
  const parsed = JSON.parse(raw) as {
    routes?: Array<{
      adapter_sync?: string;
      route_id?: string;
      runtime_adapter_id?: string;
      status?: string;
    }>;
  };
  return new Map(
    (parsed.routes ?? [])
      .filter((route) => route.status === 'active')
      .filter((route) => (
        typeof route.route_id === 'string'
        && route.route_id.length > 0
        && typeof route.runtime_adapter_id === 'string'
        && route.runtime_adapter_id.length > 0
        && typeof route.adapter_sync === 'string'
        && route.adapter_sync.length > 0
      ))
      .map((route) => [route.route_id as string, {
        adapterSync: route.adapter_sync as string,
        runtimeAdapterId: route.runtime_adapter_id as string,
      }]),
  );
}

function assertGraphWriteSyncRoute(
  syncArtifact: Map<string, {
    adapterSync: string;
    runtimeAdapterId: string;
  }>,
  routeId: string,
  runtimeAdapterId: string,
): void {
  const syncedRoute = syncArtifact.get(routeId);
  assert.ok(syncedRoute, `${routeId} must exist in generated sync artifact`);
  assert.equal(syncedRoute.runtimeAdapterId, runtimeAdapterId, `${routeId} generated sync runtime adapter must match route registry`);
  assert.equal(syncedRoute.adapterSync, ACTIVE_GRAPHWRITE_ADAPTER_SYNC, `${routeId} generated sync must use active adapter policy`);
}
