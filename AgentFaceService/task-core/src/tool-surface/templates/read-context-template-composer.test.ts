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
  assert.deepEqual(views.items.map((item) => item.view_template), ['logic_flow']);

  const quickAccess = listReadContextTemplateQuickAccess({
    domain: 'blueprint',
    readCluster: 'logic',
    targetKind: 'function',
    viewTemplate: 'logic_flow',
  });
  assert.equal(quickAccess.items[0]?.template_id, 'read.blueprint.logic.function.logic_flow');
  assert.equal(quickAccess.items[0]?.required_target_fields.includes('target_name'), true);
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
    asset_path: '<asset_path>',
    target_type: 'function',
    target_name: '<target_name>',
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
