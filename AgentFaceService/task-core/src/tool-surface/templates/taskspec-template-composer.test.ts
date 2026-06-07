import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

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

  const writeModes = listTaskSpecTemplateWriteModes({ family: 'graph_write' });
  assert.deepEqual(
    writeModes.items.map((item) => item.write_mode).sort(),
    ['graph.append', 'graph.merge', 'graph.patch', 'graph.replace'],
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
  assert.equal(result.diagnostics?.[0]?.code, 'cluster_not_supported_for_write_mode');
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

function normalizePath(filePath: string): string {
  return filePath.replaceAll('\\', '/');
}
