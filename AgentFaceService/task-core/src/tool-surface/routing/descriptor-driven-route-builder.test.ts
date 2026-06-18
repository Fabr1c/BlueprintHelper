import assert from 'node:assert/strict';
import test from 'node:test';

import { getCapabilityDescriptor } from '../capabilities/capability-descriptor-registry.js';
import type { RuntimeCapabilityState } from '../capabilities/capability-descriptor.schema.js';
import { buildDescriptorDrivenRoute } from './descriptor-driven-route-builder.js';

const ACTIVE_RUNTIME: RuntimeCapabilityState = {
  registered_runtime_adapter_ids: [
    'graphwrite_runtime_adapter',
    'material_graph_runtime_adapter',
    'debug_case_export_runtime_adapter',
  ],
  allow_write_capabilities: true,
  allow_high_risk_capabilities: true,
};

test('descriptor-driven route builder creates task execute route from capability descriptor', () => {
  const descriptor = getCapabilityDescriptor('graphwrite.execute');
  assert.ok(descriptor);

  const route = buildDescriptorDrivenRoute({
    descriptor,
    runtime: ACTIVE_RUNTIME,
    mode: 'execute',
    command: {
      file: 'filled_taskspec.json',
      previewToken: 'preview_token_001',
      format: 'summary',
    },
  });

  assert.ok(route.ok);
  assert.equal(route.plan.capability_id, 'graphwrite.execute');
  assert.equal(route.plan.handler_id, 'task_runtime');
  assert.equal(route.plan.bridge_command, 'execute_task_plan');
  assert.equal(route.plan.preview_supported, true);
  assert.equal(route.plan.preview_required_for_execute, true);
  assert.equal(route.plan.payload['file'], 'filled_taskspec.json');
  assert.equal(route.plan.payload['preview_token'], 'preview_token_001');
  assert.equal(route.plan.descriptor_refs.review_evidence_adapter, 'graphwrite_review_evidence');
  assert.deepEqual(route.plan.descriptor_refs.read_context_route_refs, ['blueprint.logic.flow']);
});

test('descriptor-driven route builder creates debug route with projection refs', () => {
  const descriptor = getCapabilityDescriptor('debug_case.export_summary');
  assert.ok(descriptor);

  const route = buildDescriptorDrivenRoute({
    descriptor,
    runtime: ACTIVE_RUNTIME,
    mode: 'bridge',
    command: {
      file: 'debug_request.json',
      json: '{"scope":"review"}',
    },
  });

  assert.ok(route.ok);
  assert.equal(route.plan.capability_id, 'debug_case.export_summary');
  assert.equal(route.plan.handler_id, 'debug_case_export');
  assert.equal(route.plan.bridge_command, 'export_debug_bundle');
  assert.equal(route.plan.preview_supported, false);
  assert.equal(route.plan.descriptor_refs.debug_export_projection, 'debug_case_export_projection');
  assert.equal(route.plan.payload['json'], '{"scope":"review"}');
});

test('descriptor-driven route builder returns structured unavailable for missing runtime adapter', () => {
  const descriptor = getCapabilityDescriptor('material_graph.edit');
  assert.ok(descriptor);

  const route = buildDescriptorDrivenRoute({
    descriptor,
    runtime: {
      registered_runtime_adapter_ids: [],
      allow_write_capabilities: true,
      allow_high_risk_capabilities: true,
    },
    mode: 'execute',
    command: { file: 'material_taskspec.json' },
  });

  assert.equal(route.ok, false);
  if (route.ok) {
    throw new Error('Expected material_graph.edit to be unavailable.');
  }
  assert.equal(route.status, 'capability_unavailable');
  assert.equal(route.reason, 'runtime_adapter_unregistered');
  assert.equal(route.runtime_adapter_id, 'material_graph_runtime_adapter');
});

test('descriptor-driven route builder creates MaterialInstance route after P4 runtime unlock', () => {
  const descriptor = getCapabilityDescriptor('material_instance.edit');
  assert.ok(descriptor);

  const route = buildDescriptorDrivenRoute({
    descriptor,
    runtime: {
      ...ACTIVE_RUNTIME,
      registered_runtime_adapter_ids: [
        ...ACTIVE_RUNTIME.registered_runtime_adapter_ids,
        'material_instance_runtime_adapter',
      ],
    },
    mode: 'execute',
    command: { file: 'material_instance_taskspec.json' },
  });

  assert.equal(route.ok, true);
  if (!route.ok) {
    throw new Error('Expected material_instance.edit to be routable after P4 unlock.');
  }
  assert.equal(route.plan.capability_id, 'material_instance.edit');
  assert.equal(route.plan.handler_id, 'task_runtime');
  assert.equal(route.plan.bridge_command, 'execute_task_plan');
  assert.equal(route.plan.payload['file'], 'material_instance_taskspec.json');
  assert.equal(route.plan.descriptor_refs.review_evidence_adapter, 'material_instance_review_evidence');
  assert.equal(route.plan.descriptor_refs.read_context_projection_adapter, 'material_instance_projection');
  assert.deepEqual(route.plan.descriptor_refs.read_context_route_refs, ['material_instance.schema.asset']);
});

test('descriptor-driven route builder respects write and high-risk runtime policies', () => {
  const descriptor = getCapabilityDescriptor('graphwrite.execute');
  assert.ok(descriptor);

  const writeDisabled = buildDescriptorDrivenRoute({
    descriptor,
    runtime: {
      registered_runtime_adapter_ids: ['graphwrite_runtime_adapter'],
      allow_write_capabilities: false,
      allow_high_risk_capabilities: true,
    },
    mode: 'execute',
    command: { file: 'taskspec.json' },
  });
  assert.equal(writeDisabled.ok, false);
  if (writeDisabled.ok) {
    throw new Error('Expected graphwrite.execute write-disabled route to be unavailable.');
  }
  assert.equal(writeDisabled.reason, 'write_capability_disabled');

  const highRiskDisabled = buildDescriptorDrivenRoute({
    descriptor,
    runtime: {
      registered_runtime_adapter_ids: ['graphwrite_runtime_adapter'],
      allow_write_capabilities: true,
      allow_high_risk_capabilities: false,
    },
    mode: 'execute',
    command: { file: 'taskspec.json' },
  });
  assert.equal(highRiskDisabled.ok, false);
  if (highRiskDisabled.ok) {
    throw new Error('Expected graphwrite.execute high-risk-disabled route to be unavailable.');
  }
  assert.equal(highRiskDisabled.reason, 'high_risk_capability_disabled');
});
