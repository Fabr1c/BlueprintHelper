import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import {
  composeReadContextTemplate,
  listReadContextTemplateClusters,
  listReadContextTemplateDomains,
  listReadContextTemplateQuickAccess,
  listReadContextTemplateTargets,
  listReadContextTemplateViews,
} from './read-context-template-composer.js';
import {
  getActiveReadContextRouteDescriptors,
  getReadContextRouteDescriptor,
} from './read-context-template-registry.js';

test('ReadContext template registry exposes active routes and hides reserved domains', () => {
  const active = getActiveReadContextRouteDescriptors();

  assert.equal(active.some((route) => route.route_id === 'read.blueprint.logic.function.logic_flow'), true);
  assert.equal(active.some((route) => route.domain === 'material'), false);

  const functionFlow = getReadContextRouteDescriptor('read.blueprint.logic.function.logic_flow');
  assert.equal(functionFlow?.domain, 'blueprint');
  assert.equal(functionFlow?.read_cluster, 'logic');
  assert.equal(functionFlow?.target_kind, 'function');
  assert.equal(functionFlow?.view_template, 'logic_flow');
  assert.equal(functionFlow?.read_type, 'blueprint_logic');
  assert.equal(functionFlow?.target_type, 'function');
  assert.equal(functionFlow?.format, 'logic_flow');
  assert.equal(functionFlow?.status, 'active');
  assert.equal(functionFlow?.request_builder_id, 'blueprint_logic');
  assert.equal(functionFlow?.payload_projector_id, 'logic');
  assert.deepEqual(functionFlow?.supported_asset_types, ['blueprint', 'function']);
  assert.deepEqual(functionFlow?.supported_formats, ['logic_flow']);

  const widgetTree = getReadContextRouteDescriptor('read.widget_blueprint.structure_tree.widget_tree.tree_json');
  assert.equal(widgetTree?.format, 'tree_json');
  assert.equal(widgetTree?.output_schema, 'WidgetTreeJson.v1');
  assert.equal(widgetTree?.request_builder_id, 'widget_tree');
  assert.equal(widgetTree?.payload_projector_id, 'widget_tree');
});

test('ReadContext active route descriptors own request and payload routing facts', () => {
  for (const route of getActiveReadContextRouteDescriptors()) {
    assert.equal(Boolean(route.request_builder_id), true, `${route.route_id} has request builder id`);
    assert.equal(Boolean(route.payload_projector_id), true, `${route.route_id} has payload projector id`);
    assert.equal(route.supported_asset_types.length > 0, true, `${route.route_id} has supported asset types`);
    assert.equal(route.supported_formats.length > 0, true, `${route.route_id} has supported formats`);
  }
});

test('ReadContext template index exposes domain cluster target and view discovery', () => {
  const domains = listReadContextTemplateDomains();
  assert.equal(domains.schema, 'BlueprintHelper.ReadContextTemplateDomains.v1');
  assert.equal(domains.items.some((item) => item.domain === 'blueprint'), true);
  assert.equal(domains.items.some((item) => item.domain === 'material'), false);

  const clusters = listReadContextTemplateClusters({ domain: 'blueprint' });
  assert.equal(clusters.items.some((item) => item.read_cluster === 'logic'), true);

  const targets = listReadContextTemplateTargets({ domain: 'blueprint', readCluster: 'logic' });
  assert.equal(targets.items.some((item) => item.target_kind === 'function'), true);

  const views = listReadContextTemplateViews({
    domain: 'blueprint',
    readCluster: 'logic',
    targetKind: 'function',
  });
  assert.deepEqual(views.items.map((item) => item.view_template), ['logic_flow', 'logic_json', 'logic_md']);

  const quickAccess = listReadContextTemplateQuickAccess({
    domain: 'blueprint',
    readCluster: 'logic',
    targetKind: 'function',
    viewTemplate: 'logic_flow',
  });
  assert.equal(quickAccess.items[0]?.template_id, 'read.blueprint.logic.function.logic_flow');
  assert.equal(quickAccess.items[0]?.required_target_fields.includes('target_name'), true);
});

test('ReadContext template composer writes widget tree tree_json ReadSpec from descriptor-backed route', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'widget-tree.readspec.json');

  const result = composeReadContextTemplate({
    domain: 'widget_blueprint',
    readCluster: 'structure_tree',
    targetKind: 'widget_tree',
    viewTemplate: 'tree_json',
    templateIds: [],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.equal(result.template_id, 'read.widget_blueprint.structure_tree.widget_tree.tree_json');

  const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, unknown>;
  assert.equal(readSpec.read_type, 'widget_context');
  assert.deepEqual(readSpec.target, {
    asset_path: '__REQUIRED_ASSET_PATH__',
    target_type: 'blueprint',
  });
  assert.deepEqual(readSpec.view, { format: 'tree_json' });
});

test('ReadContext template composer writes bare ReadSpec from descriptor-backed route', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'function-flow.readspec.json');

  const result = composeReadContextTemplate({
    domain: 'blueprint',
    readCluster: 'logic',
    targetKind: 'function',
    viewTemplate: 'logic_flow',
    templateIds: [],
    outputPath,
  });

  assert.equal(result.schema, 'BlueprintHelper.ReadContextTemplateComposition.v1');
  assert.equal(result.status, 'ok');
  assert.equal(result.template_id, 'read.blueprint.logic.function.logic_flow');
  assert.equal(result.next.read_command, `bh context read --file ${path.resolve(outputPath).replaceAll('\\', '/')} --format json`);

  const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, unknown>;
  assert.equal(readSpec.schema, 'BlueprintHelper.ReadSpec.v1');
  assert.equal(readSpec.read_type, 'blueprint_logic');
  assert.deepEqual(readSpec.target, {
    asset_path: '__REQUIRED_ASSET_PATH__',
    target_type: 'function',
    target_name: '__REQUIRED_TARGET_NAME__',
  });
  assert.deepEqual(readSpec.view, { format: 'logic_flow' });
  assert.equal(Object.hasOwn(readSpec, 'task_spec'), false);
});

test('ReadContext template composer reports diagnostics without writing unsupported reserved routes', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'material.readspec.json');

  const result = composeReadContextTemplate({
    domain: 'material',
    readCluster: 'logic',
    targetKind: 'graph',
    viewTemplate: 'logic_json',
    templateIds: [],
    outputPath,
  });

  assert.equal(result.status, 'failed');
  assert.equal(fs.existsSync(outputPath), false);
  assert.equal(result.diagnostics?.[0]?.code, 'unsupported_read_context_template');
});
