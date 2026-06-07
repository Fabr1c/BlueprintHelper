import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { getGraphWriteRoutesForTemplateDiscovery } from './graphwrite-route-registry.js';
import {
  getAllGraphWriteSlotDescriptors,
  getGraphWriteSlotsForRoute,
  requireGraphWriteSlotById,
} from './graphwrite-slot-registry.js';

test('GraphWrite slot descriptors own active slot template files and routes', () => {
  const visibleRoutes = getGraphWriteRoutesForTemplateDiscovery();
  const visibleRouteIds = new Set(visibleRoutes.map((route) => route.route_id));
  const descriptors = getAllGraphWriteSlotDescriptors();

  assert.ok(descriptors.length > 0);
  for (const descriptor of descriptors) {
    assert.equal(fs.existsSync(resolvePluginPath(descriptor.template_path)), true, `${descriptor.slot_id} template must exist`);
    assert.ok(descriptor.supported_routes.length > 0, `${descriptor.slot_id} must apply to at least one route`);
    for (const routeId of descriptor.supported_routes) {
      assert.equal(visibleRouteIds.has(routeId), true, `${descriptor.slot_id} exposes non-visible route ${routeId}`);
    }
  }
});

test('GraphWrite function parameter slot is scoped to function body discovery', () => {
  const functionParamSlot = requireGraphWriteSlotById('graph.expression.get.function_param');

  assert.deepEqual(functionParamSlot.supported_routes, ['graph.replace.function_body']);
  assert.equal(
    getGraphWriteSlotsForRoute('graph.replace.function_body', 'expression')
      .some((slot) => slot.slot_id === functionParamSlot.slot_id),
    true,
  );

  for (const routeId of [
    'graph.append.custom_event',
    'graph.replace.event_body',
    'graph.patch.pin_default',
    'graph.merge_external_flow.append_after',
  ]) {
    assert.equal(
      getGraphWriteSlotsForRoute(routeId, 'expression')
        .some((slot) => slot.slot_id === functionParamSlot.slot_id),
      false,
      `${routeId} must not expose function parameter reads`,
    );
  }
});

test('GraphWrite return slot is scoped to function body discovery', () => {
  const returnSlot = requireGraphWriteSlotById('graph.statement.control.return');

  assert.deepEqual(returnSlot.supported_routes, ['graph.replace.function_body']);
  assert.equal(
    getGraphWriteSlotsForRoute('graph.replace.function_body', 'statement')
      .some((slot) => slot.slot_id === returnSlot.slot_id),
    true,
  );

  for (const routeId of [
    'graph.append.custom_event',
    'graph.replace.event_body',
    'graph.patch.pin_default',
    'graph.merge_external_flow.append_after',
  ]) {
    assert.equal(
      getGraphWriteSlotsForRoute(routeId, 'statement')
        .some((slot) => slot.slot_id === returnSlot.slot_id),
      false,
      `${routeId} must not expose function return statements`,
    );
  }
});

test('GraphWrite slots declare four-layer quick-access metadata', () => {
  for (const slot of getAllGraphWriteSlotDescriptors()) {
    assert.equal(slot.quick_access.family, 'graph_write');
    assert.equal(slot.quick_access.template_id.split('.').length >= 3, true, `${slot.slot_id} template_id`);
    assert.equal(slot.quick_access.cluster_id.length > 0, true, `${slot.slot_id} cluster`);
    assert.equal(slot.quick_access.operation_id.length > 0, true, `${slot.slot_id} operation`);
    assert.equal(slot.quick_access.quick_access_id.length > 0, true, `${slot.slot_id} quick access`);
  }
});

test('GraphWrite active slots declare positional input slots', () => {
  for (const slot of getAllGraphWriteSlotDescriptors()) {
    assert.equal(Array.isArray(slot.input_slots), true, `${slot.slot_id} input_slots`);
    const indexes = new Set<number>();
    for (const input of slot.input_slots) {
      assert.equal(Number.isInteger(input.index), true, `${slot.slot_id} input index`);
      assert.equal(indexes.has(input.index), false, `${slot.slot_id} duplicate input index ${input.index}`);
      indexes.add(input.index);
      assert.equal(typeof input.name, 'string', `${slot.slot_id} input name`);
      assert.equal(input.name.length > 0, true, `${slot.slot_id} input name`);
      assert.equal(typeof input.path, 'string', `${slot.slot_id} input path`);
      assert.equal(input.path.length > 0, true, `${slot.slot_id} input path`);
      assert.equal(Array.isArray(input.accepts), true, `${slot.slot_id} input accepts`);
      assert.equal(input.accepts.includes('expression'), true, `${slot.slot_id} input must accept expression in P1`);
    }
  }
});

function resolvePluginPath(relativePath: string): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../../..', relativePath);
}
