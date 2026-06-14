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
      'object_properties',
      'umg_widget',
    ],
  );
});

test('supported non-GraphWrite metadata points to real templates', () => {
  for (const entry of NON_GRAPHWRITE_TEMPLATE_FAMILIES) {
    if (entry.status !== 'supported') continue;
    assert.equal(fs.existsSync(path.join(PLUGIN_ROOT, entry.base_template_path)), true, `${entry.family} template exists`);
    assert.equal(entry.insert_targets.length > 0, true, `${entry.family} insert targets`);
  }
});

test('dedicated Blueprint component class settings and signature families are supported', () => {
  const byFamily = new Map(NON_GRAPHWRITE_TEMPLATE_FAMILIES.map((entry) => [entry.family, entry]));

  assert.equal(byFamily.get('blueprint_components')?.status, 'supported');
  assert.equal(
    byFamily.get('blueprint_components')?.base_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_components_template.json',
  );
  assert.equal(byFamily.get('blueprint_class_settings')?.status, 'supported');
  assert.equal(
    byFamily.get('blueprint_class_settings')?.base_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_class_settings_template.json',
  );
  assert.equal(byFamily.get('blueprint_signature')?.status, 'supported');
  assert.equal(
    byFamily.get('blueprint_signature')?.base_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_signature_template.json',
  );
  assert.equal(byFamily.get('material_graph')?.status, 'supported');
  assert.equal(
    byFamily.get('material_graph')?.base_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/material_graph_edit_template.json',
  );
});

test('dedicated non-GraphWrite families expose operation quick-access templates', () => {
  const componentOps = listNonGraphWriteTemplateOperations({
    family: 'blueprint_components',
    cluster: 'component_tree',
    writeMode: 'components.edit',
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
    writeMode: 'class_settings.edit',
  })[0];
  assert.equal(classReparent?.template_id, 'blueprint_class_settings.class_settings.reparent');
  assert.deepEqual(classReparent?.insert_paths, ['behavior.reparent']);

  const ensureFunction = listNonGraphWriteTemplateQuickAccess({
    family: 'blueprint_signature',
    cluster: 'signature',
    operation: 'ensure_function',
    writeMode: 'signature.edit',
  })[0];
  assert.equal(ensureFunction?.template_id, 'blueprint_signature.signature.ensure_function');
  assert.deepEqual(ensureFunction?.insert_paths, ['behavior.changes[]']);

  const appendMaterialBlock = listNonGraphWriteTemplateQuickAccess({
    family: 'material_graph',
    cluster: 'material_graph',
    operation: 'append_block',
    writeMode: 'material.graph',
  })[0];
  assert.equal(appendMaterialBlock?.template_id, 'material_graph.material_graph.append_block');
  assert.deepEqual(appendMaterialBlock?.insert_paths, ['behavior.entries[]']);
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
    writeMode: 'signature.edit',
  }).find((operation) => operation.operation_id === 'ensure_function');
  assert.equal(ensureFunction?.validation_classification, 'shared_policy');

  const ensureFunctionQuickAccess = listNonGraphWriteTemplateQuickAccess({
    family: 'blueprint_signature',
    cluster: 'signature',
    operation: 'ensure_function',
    writeMode: 'signature.edit',
  })[0];
  assert.equal(ensureFunctionQuickAccess?.validation_classification, 'shared_policy');
});

test('blueprint_variables exposes variable cluster operations and quick-access templates', () => {
  const clusters = listNonGraphWriteTemplateClusters({ family: 'blueprint_variables' });
  assert.deepEqual(clusters.map((cluster) => cluster.cluster_id), ['variables']);

  const operations = listNonGraphWriteTemplateOperations({
    family: 'blueprint_variables',
    cluster: 'variables',
    writeMode: 'variables.edit',
  });
  assert.ok(operations.some((operation) => operation.operation_id === 'ensure_member_variable'));
  assert.ok(operations.some((operation) => operation.operation_id === 'configure_member_variable'));

  const quickAccess = listNonGraphWriteTemplateQuickAccess({
    family: 'blueprint_variables',
    cluster: 'variables',
    operation: 'ensure_member_variable',
    writeMode: 'variables.edit',
  });
  assert.equal(quickAccess[0]?.template_id, 'blueprint_variables.variables.ensure_member_variable');
});
