import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { CAPABILITY_DESCRIPTORS } from './capability-descriptors.js';
import {
  createCapabilityDescriptorRegistry,
  getCapabilityDescriptor,
  listAgentVisibleCapabilities,
  listCapabilityDescriptors,
} from './capability-descriptor-registry.js';

const THIS_DIR = path.dirname(fileURLToPath(import.meta.url));
const PLUGIN_ROOT = path.resolve(THIS_DIR, '../../../../..');
const GENERATED_REGISTRY_SOURCE_PATH = path.join(
  PLUGIN_ROOT,
  'BlueprintHelper/Source/BlueprintHelper/Private/Runtime/Capabilities/BlueprintHelperGeneratedCapabilityRegistry.cpp',
);

test('capability descriptor schema accepts valid built-in descriptors', () => {
  const descriptors = listCapabilityDescriptors();
  assert.equal(descriptors.length > 0, true);
  assert.equal(descriptors.every((descriptor) => descriptor.schema === 'BlueprintHelper.CapabilityDescriptor.v1'), true);
  assert.ok(getCapabilityDescriptor('graphwrite.execute'));
});

test('capability descriptor registry rejects duplicate ids', () => {
  const first = CAPABILITY_DESCRIPTORS[0];
  assert.ok(first);
  assert.throws(
    () => createCapabilityDescriptorRegistry([first, first]),
    /Duplicate BlueprintHelper capability descriptor id/u,
  );
});

test('capability descriptor schema rejects missing runtime adapter', () => {
  const first = CAPABILITY_DESCRIPTORS[0];
  assert.ok(first);
  const invalid = {
    ...first,
    runtime: {
      status: 'active',
    },
  };
  assert.throws(
    () => createCapabilityDescriptorRegistry([invalid as never]),
    /runtime/u,
  );
});

test('agent visible capabilities hide reserved or unavailable runtime adapters', () => {
  const visible = listAgentVisibleCapabilities({
    registered_runtime_adapter_ids: [
      'graphwrite_runtime_adapter',
      'material_graph_runtime_adapter',
      'debug_case_export_runtime_adapter',
    ],
    allow_write_capabilities: true,
    allow_high_risk_capabilities: true,
  });

  assert.equal(visible.some((descriptor) => descriptor.id === 'graphwrite.execute'), true);
  assert.equal(visible.some((descriptor) => descriptor.id === 'material_graph.edit'), true);
  assert.equal(visible.some((descriptor) => descriptor.id === 'debug_case.export_summary'), true);
  assert.equal(visible.some((descriptor) => descriptor.id === 'material_instance.edit'), false);
  assert.equal(visible.some((descriptor) => descriptor.id === 'asset_factory.create'), false);
});

test('MaterialInstance descriptor stays hidden while P4 runtime is blocked', () => {
  const hidden = listAgentVisibleCapabilities({
    registered_runtime_adapter_ids: [
      'graphwrite_runtime_adapter',
      'material_graph_runtime_adapter',
      'debug_case_export_runtime_adapter',
    ],
    allow_write_capabilities: true,
    allow_high_risk_capabilities: true,
  });
  const visible = listAgentVisibleCapabilities({
    registered_runtime_adapter_ids: [
      'graphwrite_runtime_adapter',
      'material_graph_runtime_adapter',
      'material_instance_runtime_adapter',
      'debug_case_export_runtime_adapter',
    ],
    allow_write_capabilities: true,
    allow_high_risk_capabilities: true,
  });

  assert.equal(hidden.some((descriptor) => descriptor.id === 'material_instance.edit'), false);
  assert.equal(visible.some((descriptor) => descriptor.id === 'material_instance.edit'), false);
  assert.equal(getCapabilityDescriptor('material_instance.edit')?.runtime.status, 'blocked_until_p4');
  assert.equal(getCapabilityDescriptor('material_instance.edit')?.safety.reserved_only, true);
});

test('Struct fields descriptor stays hidden until a real runtime adapter is implemented', () => {
  const hidden = listAgentVisibleCapabilities({
    registered_runtime_adapter_ids: [
      'struct_runtime_adapter',
      'debug_case_export_runtime_adapter',
    ],
    allow_write_capabilities: true,
    allow_high_risk_capabilities: true,
  });

  assert.equal(hidden.some((descriptor) => descriptor.id === 'struct.fields.edit'), false);
  assert.equal(getCapabilityDescriptor('struct.fields.edit')?.runtime.status, 'planned');
  assert.equal(getCapabilityDescriptor('struct.fields.edit')?.safety.reserved_only, true);
});

test('write safety filters high-risk descriptors', () => {
  const visible = listAgentVisibleCapabilities({
    registered_runtime_adapter_ids: [
      'graphwrite_runtime_adapter',
      'debug_case_export_runtime_adapter',
    ],
    allow_write_capabilities: false,
    allow_high_risk_capabilities: false,
  });

  assert.equal(visible.some((descriptor) => descriptor.id === 'graphwrite.execute'), false);
  assert.equal(visible.some((descriptor) => descriptor.id === 'debug_case.export_summary'), true);
});

test('generated UE capability registry mirrors TS descriptor gate fields', () => {
  const generatedRows = parseGeneratedRegistrySource(readFileSync(GENERATED_REGISTRY_SOURCE_PATH, 'utf8'));
  const descriptors = listCapabilityDescriptors();
  assert.equal(generatedRows.size, descriptors.length);

  for (const descriptor of descriptors) {
    const generated = generatedRows.get(descriptor.id);
    assert.ok(generated, `Missing generated UE capability descriptor row: ${descriptor.id}`);
    assert.equal(generated.family, descriptor.family, descriptor.id);
    assert.equal(generated.operation, descriptor.operation, descriptor.id);
    assert.equal(generated.routingCliCommand, descriptor.routing.cli_command, descriptor.id);
    assert.equal(generated.routingBridgeCommand, descriptor.routing.bridge_command ?? '', descriptor.id);
    assert.equal(generated.routingHandlerId, descriptor.routing.handler_id, descriptor.id);
    assert.equal(generated.runtimeAdapterId, descriptor.runtime.adapter_id, descriptor.id);
    assert.equal(generated.runtimeStatus, descriptor.runtime.status, descriptor.id);
    assert.equal(generated.safetyRisk, descriptor.safety.risk, descriptor.id);
    assert.equal(generated.reservedOnly, descriptor.safety.reserved_only, descriptor.id);
    assert.equal(generated.writeApprovalRequired, descriptor.safety.write_approval_required, descriptor.id);
  }
});

interface GeneratedCapabilityRegistryRow {
  readonly family: string;
  readonly operation: string;
  readonly routingCliCommand: string;
  readonly routingBridgeCommand: string;
  readonly routingHandlerId: string;
  readonly runtimeAdapterId: string;
  readonly runtimeStatus: string;
  readonly safetyRisk: string;
  readonly reservedOnly: boolean;
  readonly writeApprovalRequired: boolean;
}

function parseGeneratedRegistrySource(source: string): ReadonlyMap<string, GeneratedCapabilityRegistryRow> {
  const rows = new Map<string, GeneratedCapabilityRegistryRow>();
  const rowPattern = /\{\s*TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\),\s*(true|false),\s*(true|false)\s*\},/gu;
  for (const match of source.matchAll(rowPattern)) {
    const [
      ,
      id,
      family,
      operation,
      routingCliCommand,
      routingBridgeCommand,
      routingHandlerId,
      runtimeAdapterId,
      runtimeStatus,
      safetyRisk,
      reservedOnly,
      writeApprovalRequired,
    ] = match;
    rows.set(id, {
      family,
      operation,
      routingCliCommand,
      routingBridgeCommand,
      routingHandlerId,
      runtimeAdapterId,
      runtimeStatus,
      safetyRisk,
      reservedOnly: reservedOnly === 'true',
      writeApprovalRequired: writeApprovalRequired === 'true',
    });
  }
  return rows;
}
