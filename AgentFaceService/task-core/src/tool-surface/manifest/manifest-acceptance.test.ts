import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  getAllGraphWriteRoutes,
  getGraphWriteRoutesForTemplateDiscovery,
} from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import { getGraphWriteSlotsForTemplateDiscovery } from '../../task/compiler/graphwrite/graphwrite-slot-registry.js';
import {
  getToolTemplateDispatch,
  listToolCapabilities,
  listToolDomains,
} from '../tool-registry.js';
import { resolveResultProjectionPolicy } from '../result/result-projection-registry.js';
import { buildReadonlyToolCommandManifestRegistry } from './tool-command-manifest-builder.js';

test('every public tool resolves through ToolCommandManifest registry', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  for (const capability of listDefaultPublicCapabilities()) {
    assert.equal(registry.has(capability.id), true, `${capability.id} must resolve through manifest registry`);
  }

  for (const alias of [
    'blueprinthelper_preview_task',
    'blueprinthelper_execute_task',
    'blueprinthelper_get_task_result',
    'blueprinthelper_read_context',
    'blueprinthelper_diagnostics',
  ]) {
    assert.ok(registry.require(alias), `${alias} must resolve through canonical manifest alias`);
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

test('tools templates exposes only active public graph write routes by default', () => {
  const dispatch = getToolTemplateDispatch('blueprint.plan.taskspec.preview');
  const expectedRouteIds = getGraphWriteRoutesForTemplateDiscovery()
    .map((route) => route.route_id)
    .sort();
  const actualRouteIds = dispatch.routes
    .filter((entry) => entry.route_kind === 'graph_write')
    .map((route) => route.route_id)
    .sort();

  assert.deepEqual(actualRouteIds, expectedRouteIds);
});

test('default template discovery excludes planned graph write routes', () => {
  const defaultRouteIds = new Set(getToolTemplateDispatch('blueprint.write.taskspec.execute')
    .routes
    .map((route) => route.route_id));
  const plannedRouteIds = getAllGraphWriteRoutes()
    .filter((route) => route.status === 'planned')
    .map((route) => route.route_id);

  assert.equal(plannedRouteIds.length > 0, true, 'planned route fixture must exist for this guard');
  for (const routeId of plannedRouteIds) {
    assert.equal(defaultRouteIds.has(routeId), false, `${routeId} must not appear in default route discovery`);
  }
});

test('route slot template refs resolve to existing template files', () => {
  for (const route of getGraphWriteRoutesForTemplateDiscovery()) {
    for (const slot of getGraphWriteSlotsForTemplateDiscovery(route.route_id)) {
      assert.equal(fs.existsSync(path.resolve(pluginRoot(), slot.path)), true, `${slot.slot_id} path must exist`);
      assert.equal(slot.applies_to_routes.includes(route.route_id), true, `${slot.slot_id} must apply to ${route.route_id}`);
    }
  }
});

test('metrics identities resolve for preview execute read diagnostics and GraphWrite routes', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  for (const alias of [
    'blueprinthelper_preview_task',
    'blueprinthelper_execute_task',
    'blueprinthelper_read_context',
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

function listDefaultPublicCapabilities() {
  return listToolDomains().items.flatMap((domain) =>
    domain.default_kinds.flatMap((kind) =>
      listToolCapabilities({ domain: domain.id, kind }).items));
}

function pluginRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..', '..');
}
