import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { buildHelpText } from './help.js';
import { runCli } from './run.js';

async function readJsonArtifact(filePath: unknown): Promise<Record<string, any>> {
  assert.equal(typeof filePath, 'string');
  return JSON.parse(await fs.readFile(filePath as string, 'utf8')) as Record<string, any>;
}

test('runCli returns active tool domains as catalog JSON without bridge access', async () => {
  const { output, stderr } = await runCliJson(['tools', 'domains', '--format', 'json']);

  assert.deepEqual(stderr, []);
  assert.equal(output.schema, 'BlueprintHelper.ToolDomainList.v1');
  assert.equal(output.items.some((item: Record<string, unknown>) => item.id === 'blueprint'), true);
  assert.equal(output.items.some((item: Record<string, unknown>) => item.id === 'animation'), false);
  assert.deepEqual(output.reserved, []);
  assert.deepEqual(output.next, {
    list_command: 'bh tools list <domain> <kind> --format json',
  });
});

test('runCli returns reserved tool domains only when requested', async () => {
  const { output } = await runCliJson(['tools', 'domains', '--include-reserved', '--format', 'json']);

  assert.equal(output.reserved.some((item: Record<string, unknown>) => item.id === 'animation'), true);
  assert.equal(output.reserved.some((item: Record<string, unknown>) => item.id === 'material'), true);
});

test('runCli filters tool capability catalog and points read workflows to ReadContext template index', async () => {
  const { output } = await runCliJson([
    'tools',
    'list',
    'blueprint',
    'read',
    '--requires-bridge',
    'true',
    '--risk',
    'low',
    '--format',
    'json',
  ]);

  assert.equal(output.schema, 'BlueprintHelper.ToolCapabilityList.v1');
  assert.equal(output.query.domain, 'blueprint');
  assert.equal(output.query.kind, 'read');
  assert.equal(output.items.some((item: Record<string, unknown>) => item.id === 'blueprint.read.context.logic_flow'), true);
  assert.equal(output.items.every((item: Record<string, unknown>) => item.requires_bridge === true), true);
  assert.equal(output.items.every((item: Record<string, unknown>) => item.risk === 'low'), true);
  assert.equal(
    output.next.template_index_command,
    'bh tools read-templates domains --format json',
  );
});

test('runCli exposes TaskSpec template four-layer index and compose output', async (t) => {
  const outDir = await fs.mkdtemp(path.join(os.tmpdir(), 'bh-cli-template-composer-'));
  t.after(() => fs.rm(outDir, { recursive: true, force: true }));
  const outputPath = path.join(outDir, 'graph-append.taskspec.json');

  const families = await runCliJson(['tools', 'templates', 'families', '--workflow', 'preview_execute', '--format', 'json']);
  assert.equal(families.output.schema, 'BlueprintHelper.TaskSpecTemplateFamilies.v1');
  assert.equal(families.output.items.some((item: Record<string, unknown>) => item.family === 'graph_write'), true);

  const writeModes = await runCliJson(['tools', 'templates', 'write-modes', '--family', 'graph_write', '--format', 'json']);
  assert.equal(writeModes.output.items.some((item: Record<string, unknown>) => item.write_mode === 'graph.append'), true);
  assert.equal(
    writeModes.output.items.find((item: Record<string, unknown>) => item.write_mode === 'graph.append')?.base_template_path,
    'AgentFaceService/agent-guide/Templates/write/taskspec/graph_append_template.json',
  );

  const clusters = await runCliJson(['tools', 'templates', 'clusters', '--family', 'graph_write', '--format', 'json']);
  assert.equal(clusters.output.items.some((item: Record<string, unknown>) => item.cluster_id === 'generic_ops'), true);

  const operations = await runCliJson([
    'tools',
    'templates',
    'operations',
    '--family',
    'graph_write',
    '--cluster',
    'generic_ops',
    '--write-mode',
    'graph.append',
    '--format',
    'json',
  ]);
  assert.equal(operations.output.items.some((item: Record<string, unknown>) => item.operation_id === 'call'), true);

  const quickAccess = await runCliJson([
    'tools',
    'templates',
    'quick-access',
    '--family',
    'graph_write',
    '--cluster',
    'generic_ops',
    '--operation',
    'call',
    '--write-mode',
    'graph.append',
    '--format',
    'json',
  ]);
  const directCall = quickAccess.output.items.find((item: Record<string, unknown>) => item.template_id === 'generic_ops.call.direct');
  assert.equal(directCall?.write_mode, 'graph.append');
  assert.deepEqual(directCall?.insert_paths, ['behavior.entries[].body.statements[]']);

  const composed = await runCliJson([
    'tools',
    'templates',
    'compose',
    '--family',
    'graph_write',
    '--write-mode',
    'graph.append',
    '--templates',
    'generic_ops.call.direct',
    '--out',
    outputPath,
    '--format',
    'json',
  ]);
  assert.equal(composed.output.schema, 'BlueprintHelper.TaskSpecTemplateComposition.v1');
  assert.equal(composed.output.status, 'ok');
  assert.equal('inserted_slots' in composed.output, false);
  assert.equal(JSON.parse(await fs.readFile(outputPath, 'utf8')).behavior.entries[0].body.statements.length, 1);
});

test('runCli composes supported non-GraphWrite base TaskSpec without template ids', async (t) => {
  const outDir = await fs.mkdtemp(path.join(os.tmpdir(), 'bh-cli-template-composer-'));
  t.after(() => fs.rm(outDir, { recursive: true, force: true }));
  const outputPath = path.join(outDir, 'data-table.taskspec.json');

  const { output } = await runCliJson([
    'tools',
    'templates',
    'compose',
    '--family',
    'data_table',
    '--write-mode',
    'table.rows',
    '--out',
    outputPath,
    '--format',
    'json',
  ]);

  assert.equal(output.status, 'ok');
  assert.equal(JSON.parse(await fs.readFile(outputPath, 'utf8')).task_type, 'edit_data_table');
});

test('runCli exposes ReadContext template four-layer index and compose output', async (t) => {
  const outDir = await fs.mkdtemp(path.join(os.tmpdir(), 'bh-cli-read-template-composer-'));
  t.after(() => fs.rm(outDir, { recursive: true, force: true }));
  const outputPath = path.join(outDir, 'function-flow.readspec.json');

  const domains = await runCliJson(['tools', 'read-templates', 'domains', '--format', 'json']);
  assert.equal(domains.output.schema, 'BlueprintHelper.ReadContextTemplateDomains.v1');
  assert.equal(domains.output.items.some((item: Record<string, unknown>) => item.domain === 'blueprint'), true);

  const clusters = await runCliJson(['tools', 'read-templates', 'clusters', '--domain', 'blueprint', '--format', 'json']);
  assert.equal(clusters.output.items.some((item: Record<string, unknown>) => item.read_cluster === 'logic'), true);

  const targets = await runCliJson([
    'tools',
    'read-templates',
    'targets',
    '--domain',
    'blueprint',
    '--read-cluster',
    'logic',
    '--format',
    'json',
  ]);
  assert.equal(targets.output.items.some((item: Record<string, unknown>) => item.target_kind === 'function'), true);

  const views = await runCliJson([
    'tools',
    'read-templates',
    'views',
    '--domain',
    'blueprint',
    '--read-cluster',
    'logic',
    '--target-kind',
    'function',
    '--format',
    'json',
  ]);
  assert.deepEqual(views.output.items.map((item: Record<string, unknown>) => item.view_template), ['logic_flow']);

  const quickAccess = await runCliJson([
    'tools',
    'read-templates',
    'quick-access',
    '--domain',
    'blueprint',
    '--read-cluster',
    'logic',
    '--target-kind',
    'function',
    '--view-template',
    'logic_flow',
    '--format',
    'json',
  ]);
  assert.equal(quickAccess.output.items[0]?.template_id, 'read.blueprint.logic.function.logic_flow');

  const composed = await runCliJson([
    'tools',
    'read-templates',
    'compose',
    '--domain',
    'blueprint',
    '--read-cluster',
    'logic',
    '--target-kind',
    'function',
    '--view-template',
    'logic_flow',
    '--out',
    outputPath,
    '--format',
    'json',
  ]);
  assert.equal(composed.output.schema, 'BlueprintHelper.ReadContextTemplateComposition.v1');
  assert.equal(composed.output.status, 'ok');
  assert.equal(JSON.parse(await fs.readFile(outputPath, 'utf8')).schema, 'BlueprintHelper.ReadSpec.v1');
});

test('runCli rejects old tool-id template dispatch path', async () => {
  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv: ['tools', 'templates', 'blueprint.write.taskspec.execute', '--format', 'json'],
    cwd: workspaceRoot(),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 64);
  assert.equal(stdout.join(''), '');
  assert.match(stderr.join(''), /Unsupported BlueprintHelper CLI tools templates command/);
});

test('runCli supports compile-only task preview without bridge access', async (t) => {
  const workspace = await fs.mkdtemp(path.join(os.tmpdir(), 'tmp-cli-compile-only-'));
  t.after(async () => {
    await fs.rm(workspace, { recursive: true, force: true });
  });
  const taskSpecPath = path.join(workspace, 'replace-function-body.taskspec.json');
  await fs.writeFile(
    taskSpecPath,
    JSON.stringify(makeReplaceFunctionBodyTaskSpec(), null, 2),
    'utf8',
  );
  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv: [
      'task',
      'preview',
      '--file',
      taskSpecPath,
      '--compile-only',
      '--format',
      'json',
    ],
    cwd: workspaceRoot(),
    runner: {
      async readReferenceContext() {
        throw new Error('compile-only preview must not read reference context.');
      },
      async previewTask() {
        throw new Error('compile-only preview must not call the Bridge-backed runner.');
      },
      async executeTask() {
        throw new Error('compile-only preview must not execute tasks.');
      },
      async getTaskResult() {
        throw new Error('compile-only preview must not read task results.');
      },
    },
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  const output = JSON.parse(stdout.join('')) as Record<string, any>;
  const taskPlan = await readJsonArtifact(output.artifacts.task_plan);
  assert.equal(taskPlan.steps.some((step: Record<string, any>) =>
    step.capability === 'graph_write'
    && step.write.strategy === 'owned_graph_edit'
    && step.write.ops.some((op: Record<string, unknown>) => op.op === 'replace_body')), true);
});

test('global help points template selection to TaskSpec composer index', () => {
  const help = buildHelpText();

  assert.match(help, /bh tools domains --format json/);
  assert.match(help, /bh tools list <domain> <kind> --format json/);
  assert.match(help, /bh tools templates families --workflow preview_execute --format json/);
  assert.match(help, /bh tools read-templates domains --format json/);
  assert.match(help, /bh tools templates compose --family <family>/);
  assert.doesNotMatch(help, new RegExp(['bh tools templates', '<tool_id>'].join(' ')));
});

async function runCliJson(argv: string[]): Promise<{ output: Record<string, any>; stderr: string[] }> {
  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv,
    cwd: workspaceRoot(),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  return {
    output: JSON.parse(stdout.join('')) as Record<string, any>,
    stderr,
  };
}

function workspaceRoot(): string {
  return path.join('D:', 'UEProjects', 'Template', 'Plugins', 'BlueprintHelper');
}

function makeReplaceFunctionBodyTaskSpec(): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_cli_compile_only_replace_function',
    task_type: 'edit_blueprint_graph',
    feature_name: 'CliCompileOnlyReplaceFunction',
    target: {
      asset_path: '/Game/BH_Tests/BP_CliCompileOnly',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        scope: 'function_body',
        selector: {
          kind: 'function',
          name: 'ComputeValue',
        },
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [{
            kind: 'control',
            control: 'return',
            values: {
              ReturnValue: { kind: 'literal', value_type: 'number', value: 7 },
            },
          }],
        },
      },
    },
    execution_policy: {
      dry_run_mode: 'full',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}
