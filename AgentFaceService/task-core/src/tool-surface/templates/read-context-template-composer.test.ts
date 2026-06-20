import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import {
  composeReadContextTemplate,
  listReadContextTemplateClusters,
  listReadContextTemplateFamilies,
  listReadContextTemplates,
} from './read-context-template-composer.js';
import {
  getActiveReadContextRouteDescriptors,
  getReadContextRouteDescriptor,
} from './read-context-template-registry.js';
import { ReadContextInputSchema } from '../bridge/read-context/read-context-schemas.js';

test('ReadContext template registry exposes active flat routes and supported families', () => {
  const active = getActiveReadContextRouteDescriptors();

  assert.equal(active.some((route) => route.template_id === 'blueprint.logic.function.flow'), true);
  assert.equal(active.some((route) => route.family === 'material'), true);
  assert.equal(active.some((route) => route.family === 'material_instance'), true);

  const functionFlow = getReadContextRouteDescriptor('blueprint.logic.function.flow');
  assert.equal(functionFlow?.family, 'blueprint');
  assert.equal(functionFlow?.cluster, 'logic');
  assert.equal(functionFlow?.read_type, 'blueprint_logic');
  assert.equal(functionFlow?.target_type, 'function');
  assert.equal(functionFlow?.format, 'logic_flow');
  assert.equal(functionFlow?.status, 'active');
  assert.equal(functionFlow?.request_builder_id, 'blueprint_logic');
  assert.equal(functionFlow?.payload_projector_id, 'logic');
  assert.deepEqual(functionFlow?.supported_asset_types, ['blueprint', 'function']);
  assert.deepEqual(functionFlow?.supported_formats, ['logic_flow']);

  const widgetTree = getReadContextRouteDescriptor('widget.structure.tree_json');
  assert.equal(widgetTree?.format, 'tree_json');
  assert.equal(widgetTree?.output_schema, 'WidgetTreeJson.v1');
  assert.equal(widgetTree?.request_builder_id, 'widget_tree');
  assert.equal(widgetTree?.payload_projector_id, 'widget_tree');

  const materialLogic = getReadContextRouteDescriptor('material.logic.graph.json');
  assert.equal(materialLogic?.family, 'material');
  assert.equal(materialLogic?.read_type, 'material_graph_context');
  assert.equal(materialLogic?.target_type, 'material_graph');
  assert.equal(materialLogic?.status, 'active');

  const materialInstance = getReadContextRouteDescriptor('material_instance.schema.asset');
  assert.equal(materialInstance?.family, 'material_instance');
  assert.equal(materialInstance?.cluster, 'schema');
  assert.equal(materialInstance?.read_type, 'material_instance_context');
  assert.equal(materialInstance?.target_type, 'material_instance');
  assert.equal(materialInstance?.format, 'schema_json');
  assert.equal(materialInstance?.status, 'active');
  assert.equal(materialInstance?.request_builder_id, 'material_instance_context');
  assert.equal(materialInstance?.payload_projector_id, 'material_instance');
});

test('ReadContext active route descriptors own request and payload routing facts', () => {
  for (const route of getActiveReadContextRouteDescriptors()) {
    assert.equal(Boolean(route.request_builder_id), true, `${route.template_id} has request builder id`);
    assert.equal(Boolean(route.payload_projector_id), true, `${route.template_id} has payload projector id`);
    assert.equal(route.supported_asset_types.length > 0, true, `${route.template_id} has supported asset types`);
    assert.equal(route.supported_formats.length > 0, true, `${route.template_id} has supported formats`);
    assert.equal(route.allowed_tools.includes('bh tools read-templates compose'), true);
    assert.equal(route.allowed_tools.includes('bh context read'), true);
  }
});

test('ReadContext active route descriptors emit schema-valid ReadSpec inputs', () => {
  for (const route of getActiveReadContextRouteDescriptors()) {
    const result = ReadContextInputSchema.safeParse(route.read_spec);
    assert.equal(
      result.success,
      true,
      `${route.template_id} emits invalid ReadSpec: ${JSON.stringify(result.success ? [] : result.error.issues)}`,
    );
  }
});

test('ReadContext active graph logic_json route keeps blueprint logic route only', () => {
  const routes = getActiveReadContextRouteDescriptors().filter((route) =>
    route.family === 'blueprint'
    && route.cluster === 'logic'
    && route.target_type === 'graph'
    && route.format === 'logic_json');

  assert.deepEqual(routes.map((route) => route.template_id), ['blueprint.logic.graph.json']);
  assert.equal(routes[0]?.read_type, 'blueprint_logic');
  assert.equal(routes[0]?.request_builder_id, 'blueprint_logic');
});

test('ReadContext registry exposes Blueprint class default property route', () => {
  const route = getReadContextRouteDescriptor('blueprint.class_defaults.property');

  assert.ok(route);
  assert.equal(route.family, 'blueprint');
  assert.equal(route.cluster, 'properties');
  assert.equal(route.read_type, 'blueprint_class_default_context');
  assert.equal(route.target_type, 'property');
  assert.equal(route.bridge_command, 'read_blueprint_class_default_property');
  assert.equal(route.output_schema, 'BlueprintClassDefaultPropertyContext.v1');
  assert.equal(route.request_builder_id, 'class_default_property');
  assert.equal(route.payload_projector_id, 'class_default_property');
  assert.deepEqual(route.required_fields, ['target.asset_path', 'target.target_name']);
});

test('ReadContext flat template index exposes families clusters and flattened templates', () => {
  const families = listReadContextTemplateFamilies();
  assert.equal(families.schema, 'BlueprintHelper.ReadContextTemplateFamilies.v1');
  assert.equal(families.items.some((item) => item.family === 'blueprint'), true);
  assert.equal(families.items.some((item) => item.family === 'material'), true);
  assert.equal(families.items.some((item) => item.family === 'material_instance'), true);

  const clusters = listReadContextTemplateClusters({ family: 'blueprint' });
  assert.equal(clusters.schema, 'BlueprintHelper.ReadContextTemplateClusters.v1');
  assert.deepEqual(
    clusters.items.map((item) => item.cluster).sort(),
    ['asset', 'logic', 'properties', 'schema', 'structure'],
  );

  const logicTemplates = listReadContextTemplates({ family: 'blueprint', cluster: 'logic' });
  assert.equal(logicTemplates.schema, 'BlueprintHelper.ReadContextTemplates.v1');
  assert.equal(logicTemplates.items.some((item) => item.template_id === 'blueprint.logic.function.flow'), true);
  assert.equal(logicTemplates.items.some((item) => item.template_id === 'blueprint.logic.function.json'), true);
  assert.equal(logicTemplates.items.some((item) => item.template_id === 'blueprint.logic.function.json_delta'), true);

  const functionFlow = logicTemplates.items.find((item) => item.template_id === 'blueprint.logic.function.flow');
  assert.ok(functionFlow);
  assert.equal(functionFlow.read_spec.read_type, 'blueprint_logic');
  assert.deepEqual(functionFlow.required_fields, ['target.asset_path', 'target.target_name']);
  assert.deepEqual(functionFlow.optional_fields, ['view.detail', 'view.max_items']);
  assert.equal(
    functionFlow.context_evidence['view.format.allowed_values'],
    'logic_flow | logic_json | logic_json_delta_after_logic_flow',
  );
  assert.equal(functionFlow.recommended_invocation, 'bh context read --file <read-spec.json> --format json');
  assert.deepEqual(functionFlow.allowed_tools, ['bh tools read-templates compose', 'bh context read']);
  assert.equal(functionFlow.stop_conditions.includes('read_context_screenshot_conflict'), true);
});

test('ReadContext flat composer writes Blueprint class default ReadSpec', () => {
  const outputPath = path.join(
    fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')),
    'class-default.readspec.json',
  );

  const result = composeReadContextTemplate({
    templateId: 'blueprint.class_defaults.property',
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.equal(result.template_id, 'blueprint.class_defaults.property');

  const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, any>;
  assert.equal(readSpec.schema, 'BlueprintHelper.ReadSpec.v1');
  assert.equal(readSpec.read_type, 'blueprint_class_default_context');
  assert.deepEqual(readSpec.target, {
    asset_path: '__REQUIRED_ASSET_PATH__',
    target_type: 'property',
    target_name: '__REQUIRED_TARGET_NAME__',
  });
  assert.equal(Object.hasOwn(readSpec, 'view'), false);
});

test('ReadContext flat composer writes MaterialInstance schema ReadSpec without view format', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'material-instance.readspec.json');

  const result = composeReadContextTemplate({
    templateId: 'material_instance.schema.asset',
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.equal(result.template_id, 'material_instance.schema.asset');

  const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, any>;
  assert.equal(readSpec.schema, 'BlueprintHelper.ReadSpec.v1');
  assert.equal(readSpec.read_type, 'material_instance_context');
  assert.deepEqual(readSpec.target, {
    asset_path: '__REQUIRED_ASSET_PATH__',
    target_type: 'material_instance',
  });
  assert.equal(Object.hasOwn(readSpec, 'view'), false);
});

test('ReadContext flat composer writes graph logic_json using blueprint logic route', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'graph-logic-json.readspec.json');

  const result = composeReadContextTemplate({
    templateId: 'blueprint.logic.graph.json',
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.equal(result.template_id, 'blueprint.logic.graph.json');
  const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, any>;
  assert.equal(readSpec.read_type, 'blueprint_logic');
  assert.equal(readSpec.target.target_type, 'graph');
  assert.equal(readSpec.target.target_name, '__REQUIRED_TARGET_NAME__');
});

test('ReadContext flat composer does not inherit graph target fields for blueprint logic routes', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'blueprint-logic-flow.readspec.json');

  const result = composeReadContextTemplate({
    templateId: 'blueprint.logic.blueprint.flow',
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.equal(result.template_id, 'blueprint.logic.blueprint.flow');
  const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, any>;
  assert.equal(readSpec.read_type, 'blueprint_logic');
  assert.deepEqual(readSpec.target, {
    asset_path: '__REQUIRED_ASSET_PATH__',
    target_type: 'blueprint',
  });
  assert.deepEqual(readSpec.view, { format: 'logic_flow' });
});

test('ReadContext flat composer writes widget tree tree_json ReadSpec from descriptor-backed route', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'widget-tree.readspec.json');

  const result = composeReadContextTemplate({
    templateId: 'widget.structure.tree_json',
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.equal(result.template_id, 'widget.structure.tree_json');

  const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, unknown>;
  assert.equal(readSpec.read_type, 'widget_context');
  assert.deepEqual(readSpec.target, {
    asset_path: '__REQUIRED_ASSET_PATH__',
    target_type: 'blueprint',
  });
  assert.deepEqual(readSpec.view, { format: 'tree_json' });
});

test('ReadContext flat composer writes bare ReadSpec from descriptor-backed route', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'function-flow.readspec.json');

  const result = composeReadContextTemplate({
    templateId: 'blueprint.logic.function.flow',
    outputPath,
  });

  assert.equal(result.schema, 'BlueprintHelper.ReadContextTemplateComposition.v1');
  assert.equal(result.status, 'ok');
  assert.equal(result.template_id, 'blueprint.logic.function.flow');
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

test('ReadContext flat composer exposes entry body logic_flow templates for replace_external_body evidence', () => {
  for (const targetType of ['event', 'function', 'custom_event'] as const) {
    const outputPath = path.join(
      fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')),
      `${targetType}-body-flow.readspec.json`,
    );

    const result = composeReadContextTemplate({
      templateId: `blueprint.logic.${targetType}.flow`,
      outputPath,
    });

    assert.equal(result.status, 'ok');
    assert.equal(result.template_id, `blueprint.logic.${targetType}.flow`);

    const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, any>;
    assert.equal(readSpec.read_type, 'blueprint_logic');
    assert.equal(readSpec.target.target_type, targetType);
    assert.equal(readSpec.target.target_name, '__REQUIRED_TARGET_NAME__');
    assert.deepEqual(readSpec.view, { format: 'logic_flow' });
  }
});

test('ReadContext flat composer writes LogicJson delta ReadSpec with baseline view', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'function-delta.readspec.json');

  const result = composeReadContextTemplate({
    templateId: 'blueprint.logic.function.json_delta',
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.equal(result.template_id, 'blueprint.logic.function.json_delta');

  const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, any>;
  assert.equal(readSpec.schema, 'BlueprintHelper.ReadSpec.v1');
  assert.equal(readSpec.view.format, 'logic_json_delta_after_logic_flow');
  assert.equal(readSpec.view.baseline_view, 'logic_flow');
  assert.equal(readSpec.target.target_name, '__REQUIRED_TARGET_NAME__');
});

test('ReadContext flat composer writes every active read template as schema-valid ReadSpec', () => {
  for (const route of getActiveReadContextRouteDescriptors()) {
    const outputPath = path.join(
      fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')),
      `${route.template_id.replaceAll('.', '-')}.readspec.json`,
    );

    const result = composeReadContextTemplate({
      templateId: route.template_id,
      outputPath,
    });

    assert.equal(result.status, 'ok', `${route.template_id}: ${JSON.stringify(result)}`);
    const readSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, unknown>;
    const parseResult = ReadContextInputSchema.safeParse(readSpec);
    assert.equal(
      parseResult.success,
      true,
      parseResult.success ? undefined : `${route.template_id}: ${parseResult.error.message}`,
    );
  }
});

test('ReadContext flat composer reports unknown template id without writing output', () => {
  const outputPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bh-read-template-')), 'missing.readspec.json');

  const result = composeReadContextTemplate({
    templateId: 'blueprint.logic.function.missing',
    outputPath,
  });

  assert.equal(result.schema, 'BlueprintHelper.ReadContextTemplateComposition.v1');
  assert.equal(result.status, 'failed');
  assert.equal(fs.existsSync(outputPath), false);
  assert.equal(result.diagnostics[0]?.code, 'unknown_template_id');
  assert.equal(result.diagnostics[0]?.template_id, 'blueprint.logic.function.missing');
  assert.match(result.diagnostics[0]?.message ?? '', /bh tools read-templates families --format json/);
  assert.match(result.diagnostics[0]?.message ?? '', /bh tools read-templates list --family <family> --cluster <cluster> --format json/);
});

test('ReadContext active route descriptors keep removed markdown format globally disabled', () => {
  const active = getActiveReadContextRouteDescriptors();
  const removedMarkdownFormat = ['logic', 'md'].join('_');
  const removedMarkdownCommands = [
    ['read_blueprint', 'logic', 'md'].join('_'),
    ['read_material', 'logic', 'md'].join('_'),
  ];
  assert.equal(active.some((route) => route.format === removedMarkdownFormat), false);
  assert.equal(active.some((route) => route.supported_formats.includes(removedMarkdownFormat)), false);
  assert.equal(active.some((route) => route.template_id.includes(removedMarkdownFormat)), false);
  assert.equal(active.some((route) => removedMarkdownCommands.includes(route.bridge_command ?? '')), false);
  assert.equal(JSON.stringify(active).includes(['Logic', 'Md'].join('')), false);
});
