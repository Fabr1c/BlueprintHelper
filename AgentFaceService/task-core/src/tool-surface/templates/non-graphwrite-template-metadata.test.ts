import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  listNonGraphWriteTemplateClusters,
  listNonGraphWriteTemplateOperations,
  listNonGraphWriteTemplateQuickAccess,
  listNonGraphWriteValidationClassificationDescriptors,
  NON_GRAPHWRITE_TEMPLATE_FAMILIES,
} from './non-graphwrite-template-metadata.js';
import { NON_GRAPHWRITE_OPERATION_DESCRIPTORS } from './non-graphwrite-operation-metadata.js';

const PLUGIN_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../..');

test('non-GraphWrite metadata declares every TaskSpec family from design', () => {
  assert.deepEqual(
    NON_GRAPHWRITE_TEMPLATE_FAMILIES.map((entry) => entry.family).sort(),
    [
      'asset_factory',
      'blueprint_class_settings',
      'blueprint_components',
      'blueprint_create_feature',
      'blueprint_signature',
      'blueprint_variables',
      'data_table',
      'material_graph',
      'material_instance',
      'object_properties',
      'umg_widget',
    ],
  );
});

test('supported non-GraphWrite metadata points to real templates', () => {
  for (const entry of NON_GRAPHWRITE_TEMPLATE_FAMILIES) {
    if (entry.status !== 'supported') continue;
    assert.equal(fs.existsSync(path.join(PLUGIN_ROOT, entry.internal_scaffold_template_path)), true, `${entry.family} template exists`);
    assert.equal(entry.insert_targets.length > 0, true, `${entry.family} insert targets`);
    assert.equal(entry.navigation.requires_write_mode, false, `${entry.family} navigation should not require write-mode`);
    assert.equal('write_mode' in entry, false, `${entry.family} metadata should not carry write_mode`);
  }
  for (const descriptor of NON_GRAPHWRITE_OPERATION_DESCRIPTORS) {
    assert.equal('write_mode' in descriptor, false, `${descriptor.template_id} descriptor should not carry write_mode`);
  }
});

test('dedicated Blueprint component class settings and signature families are supported', () => {
  const byFamily = new Map(NON_GRAPHWRITE_TEMPLATE_FAMILIES.map((entry) => [entry.family, entry]));

  assert.equal(byFamily.get('blueprint_components')?.status, 'supported');
  assert.equal(
    byFamily.get('blueprint_components')?.internal_scaffold_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_components_template.json',
  );
  assert.equal(byFamily.get('blueprint_class_settings')?.status, 'supported');
  assert.equal(
    byFamily.get('blueprint_class_settings')?.internal_scaffold_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_class_settings_template.json',
  );
  assert.equal(byFamily.get('blueprint_signature')?.status, 'supported');
  assert.equal(
    byFamily.get('blueprint_signature')?.internal_scaffold_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_signature_template.json',
  );
  assert.equal(byFamily.get('material_graph')?.status, 'supported');
  assert.equal(
    byFamily.get('material_graph')?.internal_scaffold_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/material_graph_edit_template.json',
  );
  assert.equal(byFamily.get('material_instance')?.status, 'supported');
  assert.equal(
    byFamily.get('material_instance')?.internal_scaffold_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/material_instance_edit_template.json',
  );
});

test('dedicated non-GraphWrite families expose operation quick-access templates', () => {
  const componentOps = listNonGraphWriteTemplateOperations({
    family: 'blueprint_components',
    cluster: 'component_tree',
  });
  assert.deepEqual(componentOps.map((item) => item.operation_id), [
    'ensure_component_present',
    'configure_component',
    'rename_component',
    'reparent_component',
    'attach_component',
    'detach_component',
    'set_root_component',
    'remove_component',
  ]);

  const classReparent = listNonGraphWriteTemplateQuickAccess({
    family: 'blueprint_class_settings',
    cluster: 'class_settings',
    operation: 'reparent',
  })[0];
  assert.equal(classReparent?.template_id, 'blueprint_class_settings.class_settings.reparent');
  assert.deepEqual(classReparent?.insert_paths, ['behavior.reparent']);

  const ensureFunction = listNonGraphWriteTemplateQuickAccess({
    family: 'blueprint_signature',
    cluster: 'signature',
    operation: 'ensure_function',
  })[0];
  assert.equal(ensureFunction?.template_id, 'blueprint_signature.signature.ensure_function');
  assert.deepEqual(ensureFunction?.insert_paths, ['behavior.changes[]']);

  const appendMaterialBlock = listNonGraphWriteTemplateQuickAccess({
    family: 'material_graph',
    cluster: 'material_graph',
    operation: 'append_block',
  })[0];
  assert.equal(appendMaterialBlock?.template_id, 'material_graph.material_graph.append_block');
  assert.deepEqual(appendMaterialBlock?.insert_paths, ['behavior.entries[]']);

  const setScalarOverride = listNonGraphWriteTemplateQuickAccess({
    family: 'material_instance',
    cluster: 'material_instance',
    operation: 'set_scalar_override',
  })[0];
  assert.equal(setScalarOverride?.template_id, 'material_instance.material_instance.set_scalar_override');
  assert.deepEqual(setScalarOverride?.insert_paths, ['behavior.operations[]']);
});

test('blueprint class settings exposes setter-aware class default quick-access template', () => {
  const operations = listNonGraphWriteTemplateOperations({
    family: 'blueprint_class_settings',
    cluster: 'class_settings',
  });

  assert.equal(operations.some((item) => item.operation_id === 'set_class_default_via_setter'), true);

  const quickAccess = listNonGraphWriteTemplateQuickAccess({
    family: 'blueprint_class_settings',
    cluster: 'class_settings',
    operation: 'set_class_default_via_setter',
  });

  assert.equal(quickAccess[0]?.template_id, 'blueprint_class_settings.class_settings.set_class_default_via_setter');
  assert.match(quickAccess[0]?.template_path ?? '', /blueprint_class_settings_class_default_setter_template\.json$/);
  assert.deepEqual(quickAccess[0]?.insert_paths, ['behavior.class_defaults']);
});

test('component and class-default operation descriptions expose native component route guidance', () => {
  const componentConfigure = NON_GRAPHWRITE_OPERATION_DESCRIPTORS.find((entry) =>
    entry.family === 'blueprint_components' && entry.operation_id === 'configure_component');
  const setClassDefault = NON_GRAPHWRITE_OPERATION_DESCRIPTORS.find((entry) =>
    entry.family === 'blueprint_class_settings' && entry.operation_id === 'set_class_default');

  assert.match(componentConfigure?.description ?? '', /owned SCS/i);
  assert.match(componentConfigure?.description ?? '', /native\/inherited component defaults/i);
  assert.match(componentConfigure?.description ?? '', /blueprint_class_settings\.class_default/);
  assert.match(setClassDefault?.description ?? '', /WeaponComponent\.PrimaryWeapon/);
  assert.match(setClassDefault?.description ?? '', /native component default/i);
});

test('non-GraphWrite operations declare preview execute validation classification', () => {
  const allowedClassifications = new Set(['preview_decidable', 'runtime_only', 'shared_policy']);
  for (const descriptor of NON_GRAPHWRITE_OPERATION_DESCRIPTORS) {
    assert.equal(
      allowedClassifications.has(descriptor.validation_classification),
      true,
      `${descriptor.template_id} validation classification`,
    );
  }

  const validationDescriptors = listNonGraphWriteValidationClassificationDescriptors();
  assert.equal(validationDescriptors.length, NON_GRAPHWRITE_OPERATION_DESCRIPTORS.length);
  assert.deepEqual(
    new Set(validationDescriptors.map((descriptor) => descriptor.template_id)),
    new Set(NON_GRAPHWRITE_OPERATION_DESCRIPTORS.map((descriptor) => descriptor.template_id)),
  );

  const ensureFunction = listNonGraphWriteTemplateOperations({
    family: 'blueprint_signature',
    cluster: 'signature',
  }).find((operation) => operation.operation_id === 'ensure_function');
  assert.equal(ensureFunction?.validation_classification, 'shared_policy');

  const ensureFunctionQuickAccess = listNonGraphWriteTemplateQuickAccess({
    family: 'blueprint_signature',
    cluster: 'signature',
    operation: 'ensure_function',
  })[0];
  assert.equal(ensureFunctionQuickAccess?.validation_classification, 'shared_policy');
});

test('blueprint_variables exposes variable cluster operations and quick-access templates', () => {
  const clusters = listNonGraphWriteTemplateClusters({ family: 'blueprint_variables' });
  assert.deepEqual(clusters.map((cluster) => cluster.cluster_id), ['variables']);

  const operations = listNonGraphWriteTemplateOperations({
    family: 'blueprint_variables',
    cluster: 'variables',
  });
  assert.ok(operations.some((operation) => operation.operation_id === 'ensure_member_variable'));
  assert.ok(operations.some((operation) => operation.operation_id === 'configure_member_variable'));

  const quickAccess = listNonGraphWriteTemplateQuickAccess({
    family: 'blueprint_variables',
    cluster: 'variables',
    operation: 'ensure_member_variable',
  });
  assert.equal(quickAccess[0]?.template_id, 'blueprint_variables.variables.ensure_member_variable');
});

test('asset_factory agent-facing operations expose specialized root create templates', () => {
  const clusters = listNonGraphWriteTemplateClusters({ family: 'asset_factory' });
  assert.deepEqual(clusters.map((cluster) => cluster.cluster_id), ['asset']);

  const operations = listNonGraphWriteTemplateOperations({
    family: 'asset_factory',
  });
  assert.deepEqual(
    operations.map((operation) => operation.operation_id).sort(),
    ['create_blueprint', 'create_data_asset', 'create_widget_blueprint'],
  );

  const cases = [
    [
      'create_data_asset',
      'asset_factory.asset.create_data_asset',
      'AgentFaceService/agent-guide/Templates/write/routes/data_asset_create_template.json',
    ],
    [
      'create_blueprint',
      'asset_factory.asset.create_blueprint',
      'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_template.json',
    ],
    [
      'create_widget_blueprint',
      'asset_factory.asset.create_widget_blueprint',
      'AgentFaceService/agent-guide/Templates/write/routes/widget_blueprint_create_template.json',
    ],
  ] as const;

  for (const [operation, templateId, templatePath] of cases) {
    const quickAccess = listNonGraphWriteTemplateQuickAccess({
      family: 'asset_factory',
      operation,
    });
    assert.equal(quickAccess.length, 1, operation);
    assert.equal(quickAccess[0]?.template_id, templateId);
    assert.equal(quickAccess[0]?.template_path, templatePath);
    assert.equal('write_mode' in (quickAccess[0] ?? {}), false);
    assert.equal(quickAccess[0]?.insert_paths.includes('behavior.asset'), true);
  }
});
