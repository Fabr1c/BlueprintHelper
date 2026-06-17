import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { TASK_PROTOCOL_CONTRACT_V1 } from '../../task/schema/task-contract.js';
import { UMG_WIDGET_OPERATION_MANIFEST } from './generated/umg-widget-operation-manifest.generated.js';
import { READ_CONTEXT_ROUTE_MANIFEST } from './generated/read-context-route-manifest.generated.js';
import {
  listTaskSpecTemplateOperations,
  listTaskSpecTemplateQuickAccess,
} from './taskspec-template-index.js';
import {
  getActiveReadContextRouteDescriptors,
  getAllReadContextRouteDescriptors,
} from './read-context-template-registry.js';
import { listActiveUmgWidgetOperationDescriptors } from './umg-widget-operation-descriptors.js';
import { buildReadContextCapabilitiesPayload } from '../bridge/read-context/read-context-capabilities.js';
import { buildReadonlyToolCommandManifests } from '../manifest/tool-command-manifest-builder.js';

const TASK_CORE_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const PLUGIN_ROOT = path.resolve(TASK_CORE_ROOT, '../..');

test('generated UMG and ReadContext route manifests mirror source descriptors', () => {
  assert.deepEqual(
    stableRows(UMG_WIDGET_OPERATION_MANIFEST),
    stableRows(listActiveUmgWidgetOperationDescriptors()),
  );
  assert.deepEqual(
    stableRows(READ_CONTEXT_ROUTE_MANIFEST),
    stableRows(getAllReadContextRouteDescriptors()),
  );

  const umgHeader = readFileSync(
    path.join(PLUGIN_ROOT, 'BlueprintHelper/Source/BlueprintHelper/Private/Generated/BlueprintHelperUMGWidgetOperationManifest.generated.h'),
    'utf8',
  );
  assert.match(umgHeader, /GBlueprintHelperUMGWidgetOperationCommands/);
  for (const descriptor of UMG_WIDGET_OPERATION_MANIFEST) {
    assert.equal(umgHeader.includes(`TEXT("${descriptor.bridge_command}")`), true, `UE UMG manifest contains ${descriptor.bridge_command}`);
    assert.equal(umgHeader.includes(`TEXT("${descriptor.kind}")`), true, `UE UMG manifest contains ${descriptor.kind}`);
    assert.equal(descriptor.validation_classification, 'shared_policy', `${descriptor.kind} validation classification`);
  }
  const rootRemovalDescriptor = UMG_WIDGET_OPERATION_MANIFEST.find(
    (descriptor) => descriptor.kind === 'remove_root_widget',
  );
  assert.ok(rootRemovalDescriptor);
  assert.deepEqual(rootRemovalDescriptor.string_enum_fields?.replacement_policy, [
    'promote_single_child',
    'replace_with_empty_root',
    'remove_empty_root',
  ]);
  assert.equal(
    umgHeader.includes('replacement_policy=promote_single_child|replace_with_empty_root|remove_empty_root'),
    true,
    'UE UMG manifest mirrors descriptor-owned replacement policy enum rules',
  );

  const readContextHeader = readFileSync(
    path.join(PLUGIN_ROOT, 'BlueprintHelper/Source/BlueprintHelper/Private/Generated/BlueprintHelperReadContextRouteManifest.generated.h'),
    'utf8',
  );
  assert.match(readContextHeader, /GBlueprintHelperReadContextRoutes/);
  for (const descriptor of READ_CONTEXT_ROUTE_MANIFEST) {
    assert.equal(readContextHeader.includes(`TEXT("${descriptor.template_id}")`), true, `UE ReadContext manifest contains ${descriptor.template_id}`);
  }
});

test('UMG TaskSpec contract and template metadata are descriptor-backed', () => {
  const operationKinds = UMG_WIDGET_OPERATION_MANIFEST.map((descriptor) => descriptor.kind).sort();
  const taskplanOps = UMG_WIDGET_OPERATION_MANIFEST.map((descriptor) => descriptor.taskplan_op).sort();
  const bridgeCommands = UMG_WIDGET_OPERATION_MANIFEST.map((descriptor) => descriptor.bridge_command).sort();
  const umgSlice = TASK_PROTOCOL_CONTRACT_V1.supported_p1_slices.find((slice) => slice.task_type === 'edit_umg_widget');
  assert.ok(umgSlice);
  assert.deepEqual([...umgSlice.change_kinds].sort(), operationKinds);
  assert.deepEqual([...umgSlice.runtime_supported_structural_ops].sort(), taskplanOps);
  assert.deepEqual([...umgSlice.runtime_lowering_adapters].sort(), taskplanOps);

  const capability = TASK_PROTOCOL_CONTRACT_V1.capability_catalog.task_runtime_clusters.find(
    (cluster) => cluster.cluster === 'umg_widget_blueprint',
  );
  assert.ok(capability);
  assert.deepEqual([...capability.runtime_adapter_operations].sort(), taskplanOps);
  const capabilityCommands = new Set(capability.ue_commands);
  for (const command of bridgeCommands) {
    assert.equal(capabilityCommands.has(command), true, `capability catalog exposes ${command}`);
  }

  const operations = listTaskSpecTemplateOperations({
    family: 'umg_widget',
    cluster: 'widget_tree',
    writeMode: 'widget.edit',
  }).items.map((entry) => entry.operation_id).sort();
  assert.deepEqual(operations, operationKinds);

  const quickAccessItems = listTaskSpecTemplateQuickAccess({
    family: 'umg_widget',
    cluster: 'widget_tree',
    operation: '',
    writeMode: 'widget.edit',
  }).items;
  const quickAccess = quickAccessItems.map((entry) => entry.operation_id).sort();
  assert.deepEqual(quickAccess, operationKinds);
  for (const entry of quickAccessItems) {
    assert.equal(entry.validation_classification, 'shared_policy', `${entry.operation_id} quick-access validation classification`);
  }
});

test('ReadContext active descriptors, generated manifest, capabilities and command route refs agree', () => {
  const activeRouteIds = getActiveReadContextRouteDescriptors().map((route) => route.template_id).sort();
  const generatedActiveRouteIds = READ_CONTEXT_ROUTE_MANIFEST
    .filter((route) => route.status === 'active')
    .map((route) => route.template_id)
    .sort();
  assert.deepEqual(generatedActiveRouteIds, activeRouteIds);

  const capabilityPayload = buildReadContextCapabilitiesPayload();
  const activeAssetTypes = new Set(getActiveReadContextRouteDescriptors().flatMap((route) => route.supported_asset_types));
  const activeFormats = new Set(getActiveReadContextRouteDescriptors().flatMap((route) => route.supported_formats));
  assert.deepEqual(new Set(capabilityPayload['asset_types'] as string[]), activeAssetTypes);
  assert.deepEqual(new Set(capabilityPayload['formats'] as string[]), activeFormats);

  const commandRouteRefs = new Set(
    buildReadonlyToolCommandManifests().flatMap((manifest) => manifest.route_refs),
  );
  const allReadContextRouteIds = new Set<string>(READ_CONTEXT_ROUTE_MANIFEST.map((route) => route.template_id));
  const commandReadContextRouteRefs = [...commandRouteRefs]
    .filter((routeId) => allReadContextRouteIds.has(routeId))
    .sort();
  assert.deepEqual(commandReadContextRouteRefs, activeRouteIds);
  for (const routeId of activeRouteIds) {
    assert.equal(commandRouteRefs.has(routeId), true, `ToolCommandManifest route_refs include ${routeId}`);
  }
});

function stableRows(rows: readonly unknown[]): unknown[] {
  return [...rows].sort((left, right) => JSON.stringify(left).localeCompare(JSON.stringify(right)));
}
