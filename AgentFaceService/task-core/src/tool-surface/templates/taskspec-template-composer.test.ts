import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { compileTaskSpecToTaskPlan } from '../../task/compiler/task-compiler.js';
import { TaskSpecSchema } from '../../task/schema/task-schemas.js';
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
  assert.deepEqual(directCall?.arg_slots, ['args(*)', 'args(*)', 'args(*)']);
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

test('TaskSpec template composer writes replace_external_body route with full dry-run policy', () => {
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
    execution_policy: { dry_run_mode: string };
    scope_policy: {
      allow_modify_user_nodes: boolean;
      external_mutation_policy: { strategy: string };
    };
  };
  assert.equal(taskSpec.schema, 'BlueprintHelper.TaskSpec.v1');
  assert.equal(taskSpec.behavior.graph_strategy, 'replace_external_body');
  assert.equal(taskSpec.behavior.external_replace.require_full_dry_run, true);
  assert.equal(taskSpec.behavior.external_replace.body.statements[0]?.kind, 'call');
  assert.equal(taskSpec.execution_policy.dry_run_mode, 'full');
  assert.equal(taskSpec.scope_policy.allow_modify_user_nodes, false);
  assert.equal(taskSpec.scope_policy.external_mutation_policy.strategy, 'replace_external_body');
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

test('TaskSpec template composer writes skipped dynamic args by descriptor position', () => {
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-template-composer-'));
  const outputPath = path.join(outDir, 'call-arg.taskspec.json');

  const result = composeTaskSpecTemplate({
    family: 'graph_write',
    writeMode: 'graph.append',
    templateIds: ['generic_ops.call.direct(0,0,generic_ops.expression.get_symbol_or_variable)'],
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
    templateIds: ['generic_ops.call.direct(0,0,generic_ops.expression.get_symbol_or_variable)'],
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

  assert.deepEqual(hits.filter((hit) => !isAllowedRoutePolicyHit(hit)), []);
});

test('GraphWrite route templates use current BlueprintLogicSpec schema', () => {
  const mergeTemplatePath = path.join(
    PLUGIN_ROOT,
    'AgentFaceService/agent-guide/Templates/write/routes/graph_merge_external_flow_template.json',
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

function isAllowedRoutePolicyHit(hit: string): boolean {
  const allowedPrefix = 'AgentFaceService/agent-guide/Templates/write/routes/graph_replace_external_body_template.json:';
  const allowedPointers = new Set([
    '/scope_policy',
    '/scope_policy/allow_modify_user_nodes',
    '/execution_policy',
    '/execution_policy/dry_run_mode',
    '/validation',
    '/validation/should_compile',
    '/validation/should_save',
  ]);
  if (!hit.startsWith(allowedPrefix)) {
    return false;
  }
  return allowedPointers.has(hit.slice(allowedPrefix.length));
}

function normalizePath(filePath: string): string {
  return filePath.replaceAll('\\', '/');
}
