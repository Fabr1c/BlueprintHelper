import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { compileTaskSpecToTaskPlan } from '../../task/compiler/task-compiler.js';
import { getAgentVisibleGraphWriteRoutes } from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import { TaskSpecSchema } from '../../task/schema/task-schemas.js';
import { createTaskSpecInputShapeAdapterRegistry } from '../input/taskspec-input-adapters.js';
import {
  composeTaskSpecTemplate,
  listTaskSpecTemplateClusters,
  listTaskSpecTemplateFamilies,
  listTaskSpecTemplateOperations,
  listTaskSpecTemplateQuickAccess,
  listTaskSpecTemplateWriteModes,
} from './taskspec-template-composer.js';

const PLUGIN_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../..');

test('TaskSpec template index exposes GraphWrite four-layer discovery', () => {
  const families = listTaskSpecTemplateFamilies({ workflow: 'preview_execute' });
  assert.equal(families.schema, 'BlueprintHelper.TaskSpecTemplateFamilies.v1');
  assert.equal(families.items.some((item) => item.family === 'graph_write'), true);
  assert.match(
    families.items.find((item) => item.family === 'graph_write')?.description ?? '',
    /Blueprint graph/i,
  );

  const writeModes = listTaskSpecTemplateWriteModes({ family: 'graph_write' });
  assert.deepEqual(
    writeModes.items.map((item) => item.write_mode).sort(),
    ['graph.append', 'graph.merge', 'graph.patch', 'graph.replace'],
  );
  assert.match(
    writeModes.items.find((item) => item.write_mode === 'graph.append')?.description ?? '',
    /new owned graph/i,
  );
  assert.equal(
    writeModes.items.find((item) => item.write_mode === 'graph.append')?.base_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/graph_append_owned_template.json',
  );
  assert.equal(
    writeModes.items.every((item) => !item.base_template_path.includes('/write/taskspec/')),
    true,
  );

  const clusters = listTaskSpecTemplateClusters({ family: 'graph_write' });
  assert.equal(clusters.items.some((item) => item.cluster_id === 'generic_ops'), true);
  assert.match(
    clusters.items.find((item) => item.cluster_id === 'generic_ops')?.description ?? '',
    /general Blueprint statements/i,
  );
  assert.equal(
    clusters.items.find((item) => item.cluster_id === 'generic_ops')?.unsupported_write_modes.includes('graph.patch'),
    true,
  );

  const operations = listTaskSpecTemplateOperations({
    family: 'graph_write',
    cluster: 'generic_ops',
    writeMode: 'graph.append',
  });
  assert.equal(operations.items.some((item) => item.operation_id === 'call'), true);
  assert.match(
    operations.items.find((item) => item.operation_id === 'call')?.description ?? '',
    /function/i,
  );

  const quickAccess = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: 'generic_ops',
    operation: 'call',
    writeMode: 'graph.append',
  });
  const directCall = quickAccess.items.find((item) => item.template_id === 'generic_ops.call.direct');
  assert.notEqual(directCall, undefined);
  assert.equal(directCall?.write_mode, 'graph.append');
  assert.equal(directCall?.source_slot_id, 'graph.statement.call.direct');
  assert.equal(directCall?.slot_type, 'statement');
  assert.deepEqual(directCall?.arg_slots, ['target_object(object)', 'args(*)', 'args(*)', 'args(*)']);
  assert.deepEqual(directCall?.insert_paths, ['behavior.entries[].body.statements[]']);
});

test('TaskSpec template families reject unsupported workflows instead of falling back', () => {
  assert.throws(
    () => listTaskSpecTemplateFamilies({ workflow: 'unsupported_workflow' }),
    /Unsupported TaskSpec template workflow/,
  );
});

test('TaskSpec template composer writes GraphWrite append TaskSpec without inserted_slots', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'graph-append.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.append',
    templateIds: ['generic_ops.call.direct'],
    outputPath,
  });

  assert.equal(result.schema, 'BlueprintHelper.TaskSpecTemplateComposition.v1');
  assert.equal(result.status, 'ok');
  assert.equal(result.output_path, path.resolve(outputPath).replaceAll('\\', '/'));
  assert.equal(Object.hasOwn(result, 'inserted_slots'), false);

  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    task_type: string;
    behavior: { entries: Array<{ body: { statements: unknown[] } }> };
  };
  assert.equal(taskSpec.task_type, 'edit_blueprint_graph');
  assert.equal(taskSpec.behavior.entries[0]?.body.statements.length, 1);
  assert.equal(Object.hasOwn(taskSpec, 'scope_policy'), false);
  assert.equal(Object.hasOwn(taskSpec, 'execution_policy'), false);
  assert.equal(Object.hasOwn(taskSpec, 'validation'), false);
});

test('TaskSpec template composer writes replace_external_body route without Agent-facing internal policy fields', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'replace-external-body.taskspec.json');

  const quickAccess = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: 'external_body',
    operation: 'replace_body',
    writeMode: 'graph.replace',
  });
  assert.equal(
    quickAccess.items.some((item) => item.template_id === 'external_body.replace_body.body'),
    true,
  );

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.replace',
    templateIds: ['external_body.replace_body.body(generic_ops.call.direct)'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    schema: string;
    behavior: {
      graph_strategy: string;
      external_replace: {
        require_full_dry_run: boolean;
        body: { statements: Array<{ kind: string }> };
      };
    };
  } & Record<string, unknown>;
  assert.equal(taskSpec.schema, 'BlueprintHelper.TaskSpec.v1');
  assert.equal(taskSpec.behavior.graph_strategy, 'replace_external_body');
  assert.equal(taskSpec.behavior.external_replace.require_full_dry_run, true);
  assert.equal(taskSpec.behavior.external_replace.body.statements[0]?.kind, 'call');
  assert.equal(Object.hasOwn(taskSpec, 'scope_policy'), false);
  assert.equal(Object.hasOwn(taskSpec, 'execution_policy'), false);
  assert.equal(Object.hasOwn(taskSpec, 'validation'), false);
});

test('TaskSpec template composer writes every active GraphWrite route quick-access root', () => {
  const quickAccess = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: '',
    operation: '',
    writeMode: '',
  }).items;

  for (const route of getAgentVisibleGraphWriteRoutes()) {
    const routeItem = quickAccess.find((item) =>
      item.slot_type === 'route'
      && item.source_slot_id === route.route_id);
    assert.notEqual(routeItem, undefined, `${route.route_id} route quick-access`);

    const expression = routeItem?.arg_slots.some((slot) => slot.includes('statement[]'))
      ? `${routeItem.template_id}(${statementTemplateIdForRoute(quickAccess, route)})`
      : routeItem?.template_id ?? '';
    const outputPath = path.join(
      fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-')),
      `${route.route_id}.taskspec.json`,
    );
    const result = composeTaskSpecTemplate({
      family: 'graph_write',
      writeMode: route.write_mode,
      templateIds: [expression],
      outputPath,
    });

    assert.equal(result.status, 'ok', `${route.route_id}: ${JSON.stringify(result)}`);
    const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as { schema?: string; task_type?: string };
    assert.equal(taskSpec.schema, 'BlueprintHelper.TaskSpec.v1', route.route_id);
    assert.equal(taskSpec.task_type, 'edit_blueprint_graph', route.route_id);
    assertRouteRequiredFields(taskSpec, route.required_fields, route.route_id);
  }
});

test('TaskSpec template index exposes external user graph patch and insert route templates', () => {
  const patchOperations = listTaskSpecTemplateOperations({
    family: 'graph_write',
    cluster: 'patch',
    writeMode: 'graph.patch',
  });

  assert.equal(patchOperations.items.some((item) => item.operation_id === 'external_link_patch'), true);
  assert.equal(patchOperations.items.some((item) => item.operation_id === 'external_property_patch'), true);

  const externalLinks = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: 'patch',
    operation: 'external_link_patch',
    writeMode: 'graph.patch',
  });
  assert.deepEqual(
    externalLinks.items.map((item) => item.template_id).sort(),
    [
      'patch.external_links.connect_pins',
      'patch.external_links.disconnect_link',
      'patch.external_links.insert_pure_resolver_between_data_link',
      'patch.external_links.replace_link',
    ],
  );

  const externalProperties = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: 'patch',
    operation: 'external_property_patch',
    writeMode: 'graph.patch',
  });
  assert.deepEqual(
    externalProperties.items.map((item) => item.template_id).sort(),
    [
      'patch.external_graph.node_comment',
      'patch.external_graph.node_property',
      'patch.external_graph.pin_default',
    ],
  );

  const merge = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: 'generic_ops',
    operation: 'merge',
    writeMode: 'graph.merge',
  });
  assert.equal(
    merge.items.some((item) => item.template_id === 'generic_ops.merge.external_insert_between'),
    true,
  );
});

test('TaskSpec template composer writes external insert-between statements into existing route array entry', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'external-insert-between.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.merge',
    templateIds: ['generic_ops.merge.external_insert_between(generic_ops.call.direct)'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    behavior: {
      external_merges: Array<{
        insert_strategy: string;
        anchor: Record<string, unknown>;
        inserted: { body: { statements: Array<{ kind: string }> } };
      }>;
    };
  } & Record<string, unknown>;
  assert.equal(Object.hasOwn(taskSpec.behavior, 'external_merges[]'), false);
  assert.equal(taskSpec.behavior.external_merges[0]?.insert_strategy, 'insert_between');
  assert.equal(taskSpec.behavior.external_merges[0]?.anchor.anchor_type, 'external_link');
  assert.deepEqual(taskSpec.behavior.external_merges[0]?.inserted.body.statements.map((statement) => statement.kind), ['call']);
});

test('TaskSpec template composer preserves no-arg GraphWrite patch route placeholders', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'patch-connect-pins.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.patch',
    templateIds: ['patch.connect_pins.default'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    behavior: { patches: Array<{ target_ref?: Record<string, unknown>; source_ref?: Record<string, unknown> }> };
  };
  assert.equal(taskSpec.behavior.patches[0]?.target_ref?.node_ref, '__REQUIRED_TARGET_NODE_REF__');
  assert.equal(taskSpec.behavior.patches[0]?.source_ref?.node_ref, '__REQUIRED_SOURCE_NODE_REF__');
});

test('TaskSpec template composer writes nested expression slots into GraphWrite statements', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'nested.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.append',
    templateIds: ['generic_ops.let.default(generic_ops.expression.literal)'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    behavior: { entries: Array<{ body: { statements: unknown[] } }> };
  };
  assert.deepEqual(taskSpec.behavior.entries[0]?.body.statements[0], {
    kind: 'let',
    name: '__REQUIRED_SYMBOL_NAME__',
    value: {
      kind: 'literal',
      value_type: '__REQUIRED_LITERAL_VALUE_TYPE__',
      value: '__REQUIRED_VALUE__',
    },
  });
});

test('TaskSpec template composer reports required placeholders for GraphWrite scaffold', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'graph-placeholder-summary.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.append',
    templateIds: ['generic_ops.let.default(generic_ops.expression.literal)'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.deepEqual(result.required_placeholders, [
    { path: 'feature_name', placeholder: '__REQUIRED_FEATURE_NAME__' },
    { path: 'target.asset_path', placeholder: '__REQUIRED_BLUEPRINT_ASSET_PATH__' },
    { path: 'behavior.entries[0].name', placeholder: '__REQUIRED_CUSTOM_EVENT_NAME__' },
    { path: 'behavior.entries[0].body.statements[0].name', placeholder: '__REQUIRED_SYMBOL_NAME__' },
    { path: 'behavior.entries[0].body.statements[0].value.value_type', placeholder: '__REQUIRED_LITERAL_VALUE_TYPE__' },
    { path: 'behavior.entries[0].body.statements[0].value.value', placeholder: '__REQUIRED_VALUE__' },
  ]);
});

test('TaskSpec template composer reports required placeholders for non-GraphWrite scaffold', () => {
  const outputPath = path.join(
    fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-')),
    'blueprint-variable-placeholder-summary.taskspec.json',
  );

  const result = composeTaskSpecTemplate({
    family: 'blueprint_variables',
    writeMode: 'variables.edit',
    templateIds: ['blueprint_variables.variables.ensure_member_variable'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.deepEqual(result.required_placeholders, [
    { path: 'feature_name', placeholder: '__REQUIRED_FEATURE_NAME__' },
    { path: 'target.asset_path', placeholder: '__REQUIRED_BLUEPRINT_ASSET_PATH__' },
    { path: 'behavior.changes[0].name', placeholder: '__REQUIRED_VARIABLE_NAME__' },
    { path: 'behavior.changes[0].pin_type.category', placeholder: '__REQUIRED_PIN_CATEGORY__' },
  ]);
});

test('TaskSpec template composer ignores optional placeholders in required summary', () => {
  const outputPath = path.join(
    fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-')),
    'blueprint-variable-optional-placeholder-summary.taskspec.json',
  );

  const result = composeTaskSpecTemplate({
    family: 'blueprint_variables',
    writeMode: 'variables.edit',
    templateIds: ['blueprint_variables.variables.ensure_member_variable'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  assert.equal(
    result.required_placeholders.some((item) => item.placeholder === '__OPTIONAL_PIN_SUBCATEGORY__'),
    false,
  );
  assert.equal(
    result.required_placeholders.some((item) => item.placeholder === '__OPTIONAL_DEFAULT_VALUE__'),
    false,
  );
});

test('TaskSpec template composer writes skipped dynamic args by descriptor position', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'call-arg.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.append',
    templateIds: ['generic_ops.call.direct(0,0,0,generic_ops.expression.get_symbol_or_variable)'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    behavior: { entries: Array<{ body: { statements: Array<{ args: Record<string, unknown> }> } }> };
  };
  assert.deepEqual(Object.keys(taskSpec.behavior.entries[0]?.body.statements[0]?.args ?? {}), ['__REQUIRED_ARG_2_NAME__']);
});

test('TaskSpec template composer accepts class-backed create alias and nested class path args', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'create-widget.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.append',
    templateIds: ['generic_ops.create.class_backed'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    behavior: { entries: Array<{ body: { statements: Array<Record<string, unknown>> } }> };
  };
  const statement = taskSpec.behavior.entries[0]?.body.statements[0];
  assert.equal(statement?.kind, 'create');
  assert.equal(statement?.create_operation, '__REQUIRED_CREATE_OPERATION__');
  assert.equal(statement?.class_path, '__REQUIRED_CLASS_PATH__');
});

test('TaskSpec template composer nests expression quick-access under call args', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'call-with-symbol.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.append',
    templateIds: ['generic_ops.call.direct(0,0,0,generic_ops.expression.get_symbol_or_variable)'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    behavior: { entries: Array<{ body: { statements: Array<{ kind: string; args?: Record<string, unknown> }> } }> };
  };
  assert.equal(taskSpec.behavior.entries[0]?.body.statements[0]?.kind, 'call');
  assert.equal(Object.hasOwn(taskSpec.behavior.entries[0]?.body.statements[0]?.args ?? {}, '__REQUIRED_ARG_2_NAME__'), true);
});

test('TaskSpec template composer rejects expression quick-access at root', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'invalid.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.append',
    templateIds: ['generic_ops.expression.literal'],
    outputPath,
  });

  assert.equal(result.status, 'failed');
  assert.equal(result.diagnostics[0]?.code, 'root_expression_slot_not_composable');
  assert.equal(fs.existsSync(outputPath), false);
});

test('Nested slot expression composed TaskSpec compiles after placeholders are filled', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'compile-nested.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.append',
    templateIds: ['generic_ops.let.default(generic_ops.expression.literal)'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8'));
  taskSpec.feature_name = 'NestedSlotExpressionSmoke';
  taskSpec.target.asset_path = '/Game/BlueprintHelperSmoke/BP_NestedSlotExpressionSmoke';
  taskSpec.behavior.entries[0].name = 'NestedSlotExpressionEvent';
  const statement = taskSpec.behavior.entries[0].body.statements[0];
  statement.name = 'LocalGreeting';
  statement.value.value_type = 'string';
  statement.value.value = 'hello';

  const parsed = TaskSpecSchema.parse(taskSpec);
  const plan = compileTaskSpecToTaskPlan(parsed);
  assert.equal(Array.isArray(plan.steps), true);
  assert.equal(plan.steps.length > 0, true);
});

test('agent-facing write templates do not expose hidden execution policy fields', () => {
  const templateRoot = path.join(PLUGIN_ROOT, 'AgentFaceService/agent-guide/Templates/write');
  const forbiddenKeys = new Set([
    'scope_policy',
    'execution_policy',
    'validation',
    'dry_run_mode',
    'review_baseline_dirty_asset_policy',
    'should_compile',
    'should_save',
    'allow_modify_user_nodes',
  ]);
  const hits: string[] = [];

  for (const filePath of listJsonFiles(templateRoot)) {
    const relativePath = normalizePath(path.relative(PLUGIN_ROOT, filePath));
    assert.equal(relativePath.includes('/write/taskspec/'), false, `${relativePath} is a removed legacy template path`);
    collectForbiddenKeys(JSON.parse(fs.readFileSync(filePath, 'utf8')), forbiddenKeys, relativePath, '', hits);
  }

  assert.deepEqual(hits, []);
});

test('GraphWrite route templates use current BlueprintLogicSpec schema', () => {
  const mergeTemplatePath = path.join(
    PLUGIN_ROOT,
    'AgentFaceService/agent-guide/Templates/write/routes/graph_merge_external_insert_between_template.json',
  );
  const taskSpec = JSON.parse(fs.readFileSync(mergeTemplatePath, 'utf8')) as {
    behavior: {
      external_merges: Array<{
        inserted: {
          body: {
            schema: string;
          };
        };
      }>;
    };
  };
  assert.equal(taskSpec.behavior.external_merges[0]?.inserted.body.schema, 'BlueprintLogicSpec.v2');
});

test('TaskSpec template composer reports diagnostics and does not write unsupported output', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'unsupported.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.patch',
    templateIds: ['generic_ops.call.direct'],
    outputPath,
  });

  assert.equal(result.status, 'failed');
  assert.equal(fs.existsSync(outputPath), false);
  assert.equal(result.diagnostics?.[0]?.code, 'slot_not_supported_for_write_mode');
});

test('TaskSpec template composer writes component quick-access changes', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'components.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'blueprint_components',
    writeMode: 'components.edit',
    templateIds: ['blueprint_components.component_tree.ensure_component_present'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    task_type: string;
    behavior: { changes: Array<Record<string, unknown>> };
  };
  assert.equal(taskSpec.task_type, 'edit_blueprint_components');
  assert.deepEqual(taskSpec.behavior.changes.map((change) => change.kind), ['ensure_component_present']);
  assert.equal(Object.hasOwn(taskSpec, 'scope_policy'), false);
  assert.equal(Object.hasOwn(taskSpec, 'execution_policy'), false);
  assert.equal(Object.hasOwn(taskSpec, 'validation'), false);
});

test('TaskSpec template composer writes class settings quick-access into object and array paths', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'class-settings.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'blueprint_class_settings',
    writeMode: 'class_settings.edit',
    templateIds: [
      'blueprint_class_settings.class_settings.add_interface',
      'blueprint_class_settings.class_settings.reparent',
    ],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    behavior: {
      interfaces: { ensure_present: unknown[]; ensure_absent: unknown[] };
      class_defaults: unknown[];
      reparent: Record<string, unknown>;
    };
  };
  assert.deepEqual(taskSpec.behavior.interfaces.ensure_present, ['__REQUIRED_INTERFACE_PATH__']);
  assert.deepEqual(taskSpec.behavior.interfaces.ensure_absent, []);
  assert.deepEqual(taskSpec.behavior.class_defaults, []);
  assert.deepEqual(taskSpec.behavior.reparent, { new_parent_class: '__REQUIRED_NEW_PARENT_CLASS__' });
});

test('TaskSpec template composer writes signature quick-access changes that compile after placeholders are filled', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'signature.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'blueprint_signature',
    writeMode: 'signature.edit',
    templateIds: ['blueprint_signature.signature.ensure_function'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8'));
  taskSpec.feature_name = 'TemplateSignatureSmoke';
  taskSpec.target.asset_path = '/Game/BH_Tests/BP_TemplateSignatureSmoke';
  const change = taskSpec.behavior.changes[0];
  change.function_name = 'ComputeTemplateScore';
  change.inputs = [{ name: 'BaseScore', pin_type: { category: 'int' } }];
  change.outputs = [{ name: 'FinalScore', pin_type: { category: 'int' } }];
  const parsed = TaskSpecSchema.parse(taskSpec);
  const plan = compileTaskSpecToTaskPlan(parsed);
  const step = plan.steps[0] as Record<string, unknown> | undefined;
  assert.equal(step?.capability, 'blueprint_signature');
});

test('all expression quick-access templates are rejected as compose roots', () => {
  const expressionItems = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: 'generic_ops',
    operation: 'expression',
    writeMode: 'graph.append',
  }).items;

  assert.equal(expressionItems.length > 0, true);
  for (const item of expressionItems) {
    const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
    const outputPath = path.join(outDir, 'expression-root.taskspec.json');
    const result = composeTaskSpecTemplate({
      family: 'graph_write',
      writeMode: 'graph.append',
      templateIds: [item.template_id],
      outputPath,
    });
    assert.equal(result.status, 'failed', item.template_id);
    assert.equal(result.diagnostics[0]?.code, 'root_expression_slot_not_composable', item.template_id);
    assert.equal(fs.existsSync(outputPath), false, item.template_id);
  }
});

test('active quick-access items expose slot type and arg slots without internal paths', () => {
  const items = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: '',
    operation: '',
    writeMode: '',
  }).items;

  for (const item of items) {
    assert.ok(
      item.slot_type === 'statement' || item.slot_type === 'expression' || item.slot_type === 'route',
      `${item.template_id} slot_type`,
    );
    assert.equal(Array.isArray(item.arg_slots), true, `${item.template_id} arg_slots`);
    for (const argSlot of item.arg_slots) {
      assert.equal(argSlot.includes('__REQUIRED_'), false, `${item.template_id} exposes raw placeholder`);
      assert.equal(argSlot.includes('.'), false, `${item.template_id} exposes path-like arg slot`);
    }
  }
});

test('TaskSpec template composer writes supported non-GraphWrite base templates', () => {
  const families = [
    'blueprint_variables',
    'blueprint_create_feature',
    'umg_widget',
    'data_table',
    'object_properties',
    'asset_factory',
  ] as const;
  for (const family of families) {
    const outputPath = path.join(
      fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-')),
      `${family}.taskspec.json`,
    );
    const result = composeTaskSpecTemplate({
      family,
      writeMode: writeModeForFamily(family),
      templateIds: [],
      outputPath,
    });
    assert.equal(result.status, 'ok', family);
    assert.equal(fs.existsSync(outputPath), true, family);
  }
});

test('TaskSpec template composer writes UMG widget operation quick-access changes', () => {
  const outputPath = path.join(
    fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-')),
    'umg-widget-create.taskspec.json',
  );

  const result = composeTaskSpecTemplate({
    family: 'umg_widget',
    writeMode: 'widget.edit',
    templateIds: ['umg.widget_tree.create_widget'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    task_type: string;
    behavior: { changes: Array<Record<string, unknown>> };
  };
  assert.equal(taskSpec.task_type, 'edit_umg_widget');
  assert.deepEqual(taskSpec.behavior.changes.map((change) => change.kind), ['create_widget']);
});

test('TaskSpec template composer writes Blueprint variable quick-access changes', () => {
  const outputPath = path.join(
    fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-')),
    'blueprint-variable.taskspec.json',
  );

  const result = composeTaskSpecTemplate({
    family: 'blueprint_variables',
    writeMode: 'variables.edit',
    templateIds: ['blueprint_variables.variables.ensure_member_variable'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as {
    schema: string;
    task_type: string;
    behavior: { changes: Array<{ kind: string; pin_type?: unknown }> };
  };
  assert.equal(taskSpec.schema, 'BlueprintHelper.TaskSpec.v1');
  assert.equal(taskSpec.task_type, 'edit_blueprint_variables');
  assert.equal(taskSpec.behavior.changes[0]?.kind, 'ensure_member_variable');
  assert.equal(typeof taskSpec.behavior.changes[0]?.pin_type, 'object');
});

test('TaskSpec template composer writes schema-valid MaterialGraph append scaffold', () => {
  const outputPath = path.join(
    fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-')),
    'material-append.taskspec.json',
  );

  const result = composeTaskSpecTemplate({
    family: 'material_graph',
    writeMode: 'material.graph',
    templateIds: ['material_graph.material_graph.append_block'],
    outputPath,
  });

  assert.equal(result.status, 'ok');
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as Record<string, unknown>;
  const parseResult = TaskSpecSchema.safeParse(taskSpec);
  assert.equal(parseResult.success, true, parseResult.success ? undefined : parseResult.error.message);
  assert.equal(Object.hasOwn(taskSpec['behavior'] as Record<string, unknown>, 'patches'), false);
  assert.equal(Object.hasOwn(taskSpec['behavior'] as Record<string, unknown>, 'merges'), false);
});

test('supported TaskSpec template families have discoverable operations quick-access and composable scaffolds', () => {
  const families = listTaskSpecTemplateFamilies({ workflow: 'preview_execute' }).items;
  assert.equal(families.length > 0, true);

  for (const family of families) {
    const writeModes = listTaskSpecTemplateWriteModes({ family: family.family }).items;
    assert.equal(writeModes.length > 0, true, `${family.family} write modes`);

    for (const writeMode of writeModes) {
      const clusters = listTaskSpecTemplateClusters({ family: family.family }).items
        .filter((cluster) => !cluster.unsupported_write_modes.includes(writeMode.write_mode));
      assert.equal(clusters.length > 0, true, `${family.family}/${writeMode.write_mode} clusters`);

      for (const cluster of clusters) {
        const operations = listTaskSpecTemplateOperations({
          family: family.family,
          cluster: cluster.cluster_id,
          writeMode: writeMode.write_mode,
        }).items;
        assert.equal(
          operations.length > 0,
          true,
          `${family.family}/${writeMode.write_mode}/${cluster.cluster_id} operations`,
        );

        for (const operation of operations) {
          const quickAccess = listTaskSpecTemplateQuickAccess({
            family: family.family,
            cluster: cluster.cluster_id,
            operation: operation.operation_id,
            writeMode: writeMode.write_mode,
          }).items;
          assert.equal(
            quickAccess.length > 0,
            true,
            `${family.family}/${writeMode.write_mode}/${cluster.cluster_id}/${operation.operation_id} quick-access`,
          );

          for (const item of quickAccess) {
            if (item.slot_type === 'expression') {
              continue;
            }
            const outputPath = path.join(
              fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-')),
              `${item.template_id.replaceAll('.', '-')}.taskspec.json`,
            );
            const result = composeTaskSpecTemplate({
              family: family.family,
              writeMode: writeMode.write_mode,
              templateIds: [templateExpressionForCoverage(item, quickAccess)],
              outputPath,
            });
            assert.equal(result.status, 'ok', `${item.template_id}: ${JSON.stringify(result)}`);
            const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8')) as { schema?: string };
            assert.equal(taskSpec.schema, 'BlueprintHelper.TaskSpec.v1', item.template_id);
            assertComposedTaskSpecDoesNotExposeRemovedFields(taskSpec, item.template_id);
            assertAgentFacingTaskSpecPassesInternalSchema(taskSpec, item.template_id);
          }
        }
      }
    }
  }
});

function writeModeForFamily(family: string): string {
  switch (family) {
    case 'blueprint_variables':
      return 'variables.edit';
    case 'blueprint_create_feature':
      return 'feature.create';
    case 'umg_widget':
      return 'widget.edit';
    case 'data_table':
      return 'table.rows';
    case 'object_properties':
      return 'object.properties';
    case 'asset_factory':
      return 'asset.create';
    default:
      throw new Error(`Unexpected test family: ${family}`);
  }
}

function firstStatementTemplateId(
  quickAccess: ReturnType<typeof listTaskSpecTemplateQuickAccess>['items'],
  writeMode: string,
): string {
  const statement = quickAccess.find((item) => item.slot_type === 'statement' && item.write_mode === writeMode);
  assert.notEqual(statement, undefined, `statement quick-access for ${writeMode}`);
  return statement?.template_id ?? '';
}

function statementTemplateIdForRoute(
  quickAccess: ReturnType<typeof listTaskSpecTemplateQuickAccess>['items'],
  route: ReturnType<typeof getAgentVisibleGraphWriteRoutes>[number],
): string {
  const explicitStatement = quickAccess.find((item) =>
    item.slot_type === 'statement'
    && item.write_mode === route.write_mode
    && route.allowed_slot_ids.includes(item.source_slot_id));
  if (explicitStatement) {
    return explicitStatement.template_id;
  }
  return firstStatementTemplateId(quickAccess, route.write_mode);
}

function templateExpressionForCoverage(
  item: ReturnType<typeof listTaskSpecTemplateQuickAccess>['items'][number],
  quickAccess: ReturnType<typeof listTaskSpecTemplateQuickAccess>['items'],
): string {
  if (item.slot_type !== 'route' || !item.arg_slots.some((slot) => slot.includes('statement[]'))) {
    return item.template_id;
  }
  const statement = quickAccess.find((candidate) =>
    candidate.slot_type === 'statement'
    && candidate.write_mode === item.write_mode
    && !candidate.unsupported_write_modes.includes(item.write_mode));
  assert.notEqual(statement, undefined, `${item.template_id} route has statement child`);
  return `${item.template_id}(${statement?.template_id ?? ''})`;
}

function assertRouteRequiredFields(taskSpec: unknown, requiredFields: readonly string[], routeId: string): void {
  for (const requiredField of requiredFields) {
    const separatorIndex = requiredField.indexOf('=');
    const pathExpression = separatorIndex >= 0 ? requiredField.slice(0, separatorIndex) : requiredField;
    const expectedValue = separatorIndex >= 0 ? requiredField.slice(separatorIndex + 1) : undefined;
    const values = collectValuesAtPath(taskSpec, pathExpression.split('.'));
    if (expectedValue === undefined) {
      assert.notEqual(values.length, 0, `${routeId} missing ${requiredField}`);
      continue;
    }
    const allowedValues = expectedValue.split('|');
    assert.equal(
      values.some((value) => allowedValues.includes(String(value))),
      true,
      `${routeId} missing ${requiredField}; actual values: ${values.map(String).join(', ')}`,
    );
  }
}

function collectValuesAtPath(value: unknown, segments: string[]): unknown[] {
  if (segments.length === 0) {
    return value === undefined || value === null ? [] : [value];
  }
  const [segment, ...rest] = segments;
  const isArraySegment = segment.endsWith('[]');
  const key = isArraySegment ? segment.slice(0, -2) : segment;
  if (!value || typeof value !== 'object') {
    return [];
  }
  const next = (value as Record<string, unknown>)[key];
  if (isArraySegment) {
    if (!Array.isArray(next)) {
      return [];
    }
    return next.flatMap((item) => collectValuesAtPath(item, rest));
  }
  return collectValuesAtPath(next, rest);
}

function listJsonFiles(root: string): string[] {
  const entries = fs.readdirSync(root, { withFileTypes: true });
  return entries.flatMap((entry) => {
    const fullPath = path.join(root, entry.name);
    if (entry.isDirectory()) {
      return listJsonFiles(fullPath);
    }
    return entry.isFile() && entry.name.endsWith('.json') ? [fullPath] : [];
  });
}

function collectForbiddenKeys(
  value: unknown,
  forbiddenKeys: Set<string>,
  filePath: string,
  pointer: string,
  hits: string[],
): void {
  if (!value || typeof value !== 'object') {
    return;
  }
  if (Array.isArray(value)) {
    value.forEach((item, index) => collectForbiddenKeys(item, forbiddenKeys, filePath, `${pointer}/${index}`, hits));
    return;
  }

  for (const [key, child] of Object.entries(value)) {
    const childPointer = `${pointer}/${key}`;
    if (forbiddenKeys.has(key)) {
      hits.push(`${filePath}:${childPointer}`);
    }
    collectForbiddenKeys(child, forbiddenKeys, filePath, childPointer, hits);
  }
}

function normalizePath(filePath: string): string {
  return filePath.replaceAll('\\', '/');
}

function assertComposedTaskSpecDoesNotExposeRemovedFields(taskSpec: unknown, label: string): void {
  const hits: string[] = [];
  collectRemovedComposedFields(taskSpec, label, '', hits);
  assert.deepEqual(hits, [], label);
}

function assertAgentFacingTaskSpecPassesInternalSchema(taskSpec: unknown, label: string): void {
  const coverageTaskSpec = fillTaskSpecCoveragePlaceholders(taskSpec);
  let adapted: unknown;
  try {
    adapted = createTaskSpecInputShapeAdapterRegistry().require('bare_taskspec').adapt(coverageTaskSpec);
  } catch (error) {
    assert.fail(`${label}: ${(error as Error).message}`);
  }
  const parseResult = TaskSpecSchema.safeParse((adapted as { task_spec?: unknown }).task_spec);
  assert.equal(
    parseResult.success,
    true,
    parseResult.success ? undefined : `${label}: ${parseResult.error.message}`,
  );
}

function fillTaskSpecCoveragePlaceholders(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => fillTaskSpecCoveragePlaceholders(item));
  }
  if (!value || typeof value !== 'object') {
    return value;
  }
  const record = value as Record<string, unknown>;
  const output: Record<string, unknown> = {};
  for (const [key, child] of Object.entries(record)) {
    if (key === 'property_descriptor_id' && typeof child === 'string' && child.startsWith('__REQUIRED_')) {
      output[key] = 'k2.node.comment';
      continue;
    }
    if (key === 'pin_direction' && typeof child === 'string' && child.startsWith('__REQUIRED_')) {
      output[key] = record['semantic_role'] === 'exec_boundary' ? 'output' : 'input';
      continue;
    }
    if (key === 'category' && typeof child === 'string' && child.includes('PIN_CATEGORY__')) {
      output[key] = 'bool';
      continue;
    }
    if (key.endsWith('_mapping') && typeof child === 'string' && child.startsWith('__')) {
      output[key] = {};
      continue;
    }
    if (key === 'replacement_policy' && typeof child === 'string' && child.startsWith('__')) {
      output[key] = 'replace_with_empty_root';
      continue;
    }
    if (key === 'is_variable' && typeof child === 'string' && child.startsWith('__')) {
      output[key] = true;
      continue;
    }
    if (key === 'anchor_ref' && typeof child === 'string' && child.startsWith('__REQUIRED_')) {
      output[key] = schemaCoverageAnchorRef(record);
      continue;
    }
    output[key] = fillTaskSpecCoveragePlaceholders(child);
  }
  return output;
}

function schemaCoverageAnchorRef(anchor: Record<string, unknown>): string {
  const anchorType = typeof anchor['anchor_type'] === 'string' ? anchor['anchor_type'] : '';
  const current = typeof anchor['anchor_ref'] === 'string' ? anchor['anchor_ref'] : '';
  switch (anchorType) {
    case 'external_link':
      return current.includes('EXEC') ? 'xlink:v1:e:coverage-link' : 'xlink:v1:d:coverage-link';
    case 'external_pin':
      return 'xpin:v1:d:coverage-pin';
    case 'external_node':
      return 'xnode:v1:coverage-node#coverage-fingerprint';
    case 'external_body':
      return 'xbody:v1:coverage-body#coverage-fingerprint';
    default:
      return current;
  }
}

function collectRemovedComposedFields(
  value: unknown,
  label: string,
  pointer: string,
  hits: string[],
): void {
  if (!value || typeof value !== 'object') {
    return;
  }
  if (Array.isArray(value)) {
    value.forEach((item, index) => collectRemovedComposedFields(item, label, `${pointer}/${index}`, hits));
    return;
  }

  const record = value as Record<string, unknown>;
  if (record['property_path'] === 'default_value') {
    hits.push(`${label}:${pointer}/property_path=default_value`);
  }
  if (Object.hasOwn(record, 'execution_policy')) {
    hits.push(`${label}:${pointer}/execution_policy`);
  }
  if (Object.hasOwn(record, 'validation')) {
    hits.push(`${label}:${pointer}/validation`);
  }
  if (Object.hasOwn(record, 'dry_run_mode')) {
    hits.push(`${label}:${pointer}/dry_run_mode`);
  }
  if (Object.hasOwn(record, 'should_compile')) {
    hits.push(`${label}:${pointer}/should_compile`);
  }
  if (Object.hasOwn(record, 'should_save')) {
    hits.push(`${label}:${pointer}/should_save`);
  }
  for (const [key, child] of Object.entries(record)) {
    collectRemovedComposedFields(child, label, `${pointer}/${key}`, hits);
  }
}
