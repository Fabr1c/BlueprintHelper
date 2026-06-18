import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { buildHelpText } from './help.js';
import { runCli } from './run.js';
import { createDescriptorFixtureRuntimeCapabilityState } from '@blueprinthelper/task-core/tool-surface/tool-registry';
import {
  listReadContextTemplateClusters,
  listReadContextTemplateFamilies,
  listReadContextTemplates,
} from '@blueprinthelper/task-core/tool-surface/templates/read-context-template-index';
import {
  listTaskSpecTemplateClusters,
  listTaskSpecTemplateFamilies,
  listTaskSpecTemplateOperations,
  listTaskSpecTemplateQuickAccess,
  listTaskSpecTemplateWriteModes,
} from '@blueprinthelper/task-core/tool-surface/templates/taskspec-template-index';

async function readJsonArtifact(filePath: unknown): Promise<Record<string, any>> {
  assert.equal(typeof filePath, 'string');
  return JSON.parse(await fs.readFile(filePath as string, 'utf8')) as Record<string, any>;
}

const ACTIVE_RUNTIME_ARGS = [
  '--runtime-adapters',
  createDescriptorFixtureRuntimeCapabilityState().registered_runtime_adapter_ids.join(','),
] as const;

test('runCli returns active tool domains as catalog JSON without bridge access', async () => {
  const { output, stderr } = await runCliJson(['tools', 'domains', '--format', 'json']);

  assert.deepEqual(stderr, []);
  assert.equal(output.schema, 'BlueprintHelper.ToolDomainList.v1');
  assert.equal(output.items.some((item: Record<string, unknown>) => item.id === 'blueprint'), true);
  assert.equal(output.items.some((item: Record<string, unknown>) => item.id === 'material'), true);
  assert.equal(output.items.some((item: Record<string, unknown>) => item.id === 'animation'), false);
  assert.deepEqual(output.reserved, []);
  assert.deepEqual(output.next, {
    list_command: 'bh tools list <domain> <kind> --format json',
  });
});

test('runCli returns reserved tool domains only when requested', async () => {
  const { output } = await runCliJson(['tools', 'domains', '--include-reserved', '--format', 'json']);

  assert.equal(output.reserved.some((item: Record<string, unknown>) => item.id === 'animation'), true);
  assert.equal(output.reserved.some((item: Record<string, unknown>) => item.id === 'material'), false);
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
    'bh tools read-templates families --format json',
  );
});

test('runCli gates material write discovery by runtime adapter fixture', async () => {
  const hidden = await runCliJson([
    'tools',
    'list',
    'material',
    'write',
    '--runtime-adapters',
    'graphwrite_runtime_adapter',
    '--format',
    'json',
  ]);
  assert.equal(
    hidden.output.items.some((item: Record<string, unknown>) => item.id === 'material.write.taskspec.execute'),
    false,
  );

  const visible = await runCliJson([
    'tools',
    'list',
    'material',
    'write',
    '--runtime-adapters',
    'material_graph_runtime_adapter',
    '--format',
    'json',
  ]);
  assert.equal(
    visible.output.items.some((item: Record<string, unknown>) => item.id === 'material.write.taskspec.execute'),
    true,
  );
  assert.equal(
    JSON.stringify(visible.output).includes('material_instance.edit'),
    false,
  );

  const materialInstanceVisible = await runCliJson([
    'tools',
    'list',
    'material',
    'write',
    '--runtime-adapters',
    'material_instance_runtime_adapter',
    '--format',
    'json',
  ]);
  assert.equal(
    materialInstanceVisible.output.items.some((item: Record<string, unknown>) => item.id === 'material.write.taskspec.execute'),
    true,
  );
});

test('runCli hydrates material write discovery from runtime profile', async () => {
  const { output, stderr } = await runCliJsonWithRuntimeProfile([
    'tools',
    'list',
    'material',
    'write',
    '--format',
    'json',
  ], ['material_graph_runtime_adapter']);

  assert.deepEqual(stderr, []);
  assert.equal(
    output.items.some((item: Record<string, unknown>) => item.id === 'material.write.taskspec.execute'),
    true,
  );
});

test('runCli marks empty-object tool templates as no-input requests', async () => {
	const projectDiscover = await runCliJson([
		'tools',
		'list',
		'project',
		'discover',
		'--format',
		'json',
	]);
	const agentGuide = projectDiscover.output.items.find((item: Record<string, unknown>) =>
		item.cli_command === 'bh blueprinthelper_read_agent_guide');
	assert.equal(agentGuide?.input_shape, 'empty_object');
	assert.equal(agentGuide?.no_input, true);
	assert.match(agentGuide?.input_note as string, /Use the empty-object template as-is/);

	const editorRead = await runCliJson([
		'tools',
		'list',
		'editor',
		'read',
		'--format',
		'json',
	]);
	const runtimeProfile = editorRead.output.items.find((item: Record<string, unknown>) =>
		item.cli_command === 'bh blueprint_get_runtime_profile');
	assert.equal(runtimeProfile?.input_shape, 'empty_object');
	assert.equal(runtimeProfile?.no_input, true);
	assert.match(runtimeProfile?.input_note as string, /No parameters/);
});

test('runCli exposes TaskSpec template four-layer index and compose output', async (t) => {
  const outDir = await fs.mkdtemp(path.join(os.tmpdir(), 'bh-cli-template-composer-'));
  t.after(() => fs.rm(outDir, { recursive: true, force: true }));
  const outputPath = path.join(outDir, 'graph-append.taskspec.json');

  const families = await runCliJson(['tools', 'templates', 'families', '--workflow', 'preview_execute', '--format', 'json']);
  assert.equal(families.output.schema, 'BlueprintHelper.TaskSpecTemplateFamilies.v1');
  assert.equal(families.output.items.some((item: Record<string, unknown>) => item.family === 'graph_write'), true);
  assert.match(
    families.output.items.find((item: Record<string, unknown>) => item.family === 'graph_write')?.description as string,
    /Blueprint graph/i,
  );

  const writeModes = await runCliJson(['tools', 'templates', 'write-modes', '--family', 'graph_write', '--format', 'json']);
  assert.equal(writeModes.output.items.some((item: Record<string, unknown>) => item.write_mode === 'graph.append'), true);
  assert.match(
    writeModes.output.items.find((item: Record<string, unknown>) => item.write_mode === 'graph.append')?.description as string,
    /new owned graph/i,
  );
  assert.equal(
    writeModes.output.items.find((item: Record<string, unknown>) => item.write_mode === 'graph.append')?.base_template_path,
    'AgentFaceService/agent-guide/Templates/write/routes/graph_append_owned_template.json',
  );
  assert.equal(
    writeModes.output.items.every((item: Record<string, unknown>) => !(item.base_template_path as string).includes('/write/taskspec/')),
    true,
  );

  const clusters = await runCliJson(['tools', 'templates', 'clusters', '--family', 'graph_write', '--format', 'json']);
  assert.equal(clusters.output.items.some((item: Record<string, unknown>) => item.cluster_id === 'generic_ops'), true);
  assert.match(
    clusters.output.items.find((item: Record<string, unknown>) => item.cluster_id === 'generic_ops')?.description as string,
    /general Blueprint statements/i,
  );

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
  assert.match(
    operations.output.items.find((item: Record<string, unknown>) => item.operation_id === 'call')?.description as string,
    /function/i,
  );

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
  assert.equal(directCall?.slot_type, 'statement');
  assert.deepEqual(directCall?.arg_slots, ['target_object(object)', 'args(*)', 'args(*)', 'args(*)']);
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
  assert.deepEqual(
    composed.output.required_placeholders.map((item: Record<string, unknown>) => item.placeholder),
    [
      '__REQUIRED_FEATURE_NAME__',
      '__REQUIRED_BLUEPRINT_ASSET_PATH__',
      '__REQUIRED_CUSTOM_EVENT_NAME__',
      '__REQUIRED_FUNCTION_OR_ACTION_TARGET__',
    ],
  );
  assert.equal('inserted_slots' in composed.output, false);
  assert.equal(JSON.parse(await fs.readFile(outputPath, 'utf8')).behavior.entries[0].body.statements.length, 1);
});

test('runCli composes nested slot expression without splitting inner commas', async (t) => {
  const outDir = await fs.mkdtemp(path.join(os.tmpdir(), 'bh-cli-template-composer-'));
  t.after(() => fs.rm(outDir, { recursive: true, force: true }));
  const outputPath = path.join(outDir, 'nested-op.taskspec.json');

  const { output } = await runCliJson([
    'tools',
    'templates',
    'compose',
    '--family',
    'graph_write',
    '--write-mode',
    'graph.append',
    '--templates',
    'generic_ops.let.default(generic_ops.expression.op(generic_ops.expression.get_symbol_or_variable,generic_ops.expression.literal))',
    '--out',
    outputPath,
    '--format',
    'json',
  ]);

  assert.equal(output.status, 'ok');
  const taskSpec = JSON.parse(await fs.readFile(outputPath, 'utf8'));
  assert.equal(taskSpec.behavior.entries[0].body.statements[0].value.kind, 'op');
});

test('runCli composes class-backed create alias through TaskSpec template composer', async (t) => {
  const outDir = await fs.mkdtemp(path.join(os.tmpdir(), 'bh-cli-template-composer-'));
  t.after(() => fs.rm(outDir, { recursive: true, force: true }));
  const outputPath = path.join(outDir, 'create-widget.taskspec.json');

  const { output } = await runCliJson([
    'tools',
    'templates',
    'compose',
    '--family',
    'graph_write',
    '--write-mode',
    'graph.append',
    '--templates',
    'generic_ops.create.class_backed',
    '--out',
    outputPath,
    '--format',
    'json',
  ]);

  assert.equal(output.status, 'ok');
  const taskSpec = JSON.parse(await fs.readFile(outputPath, 'utf8'));
  assert.equal(taskSpec.behavior.entries[0].body.statements[0].kind, 'create');
  assert.equal(taskSpec.behavior.entries[0].body.statements[0].class_path, '__REQUIRED_CLASS_PATH__');
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

test('CLI exposes blueprint_variables cluster and quick-access discovery', async () => {
  const clusters = await runCliJson([
    'tools',
    'templates',
    'clusters',
    '--family',
    'blueprint_variables',
    '--format',
    'json',
  ]);
  assert.equal(clusters.output.items.some((item: Record<string, unknown>) => item.cluster_id === 'variables'), true);

  const quickAccess = await runCliJson([
    'tools',
    'templates',
    'quick-access',
    '--family',
    'blueprint_variables',
    '--cluster',
    'variables',
    '--operation',
    'ensure_member_variable',
    '--write-mode',
    'variables.edit',
    '--format',
    'json',
  ]);
  assert.equal(
    quickAccess.output.items.some((item: Record<string, unknown>) =>
      item.template_id === 'blueprint_variables.variables.ensure_member_variable'),
    true,
  );
});

test('runCli exposes ReadContext flat template index and compose output', async (t) => {
  const outDir = await fs.mkdtemp(path.join(os.tmpdir(), 'bh-cli-read-template-composer-'));
  t.after(() => fs.rm(outDir, { recursive: true, force: true }));
  const outputPath = path.join(outDir, 'function-flow.readspec.json');

  const families = await runCliJson(['tools', 'read-templates', 'families', '--format', 'json']);
  assert.equal(families.output.schema, 'BlueprintHelper.ReadContextTemplateFamilies.v1');
  assertTemplateGuidance(families.output, /Pick a read family/i, 'read families');
  assert.equal(families.output.items.some((item: Record<string, unknown>) => item.family === 'blueprint'), true);
  assert.match(
    families.output.items.find((item: Record<string, unknown>) => item.family === 'blueprint')?.description as string,
    /Blueprint/i,
  );

  const clusters = await runCliJson(['tools', 'read-templates', 'clusters', '--family', 'blueprint', '--format', 'json']);
  assert.equal(clusters.output.schema, 'BlueprintHelper.ReadContextTemplateClusters.v1');
  assertTemplateGuidance(clusters.output, /Pick a read cluster/i, 'read clusters');
  assert.equal(clusters.output.items.some((item: Record<string, unknown>) => item.cluster === 'logic'), true);
  assert.match(
    clusters.output.items.find((item: Record<string, unknown>) => item.cluster === 'logic')?.description as string,
    /logic/i,
  );

  const templates = await runCliJson([
    'tools',
    'read-templates',
    'list',
    '--family',
    'blueprint',
    '--cluster',
    'logic',
    '--format',
    'json',
  ]);
  assert.equal(templates.output.schema, 'BlueprintHelper.ReadContextTemplates.v1');
  assertNoTopLevelGuidance(templates.output, 'read template list');
  assert.equal(templates.output.items.some((item: Record<string, unknown>) => item.template_id === 'blueprint.logic.function.flow'), true);
  assert.equal(templates.output.items.some((item: Record<string, unknown>) => item.template_id === 'blueprint.logic.function.json_delta'), true);
  assert.match(
    templates.output.items.find((item: Record<string, unknown>) => item.template_id === 'blueprint.logic.function.flow')?.description as string,
    /function/i,
  );

  const composed = await runCliJson([
    'tools',
    'read-templates',
    'compose',
    '--template',
    'blueprint.logic.function.flow',
    '--out',
    outputPath,
    '--format',
    'json',
  ]);
  assert.equal(composed.output.schema, 'BlueprintHelper.ReadContextTemplateComposition.v1');
  assert.equal(composed.output.status, 'ok');
  assert.equal(JSON.parse(await fs.readFile(outputPath, 'utf8')).schema, 'BlueprintHelper.ReadSpec.v1');
});

test('CLI TaskSpec template index mirrors descriptor-backed template index', async () => {
  const families = await runCliJson(['tools', 'templates', 'families', '--workflow', 'preview_execute', '--format', 'json']);
  const expectedFamilies = listTaskSpecTemplateFamilies({ workflow: 'preview_execute' });
  assert.deepEqual(jsonRows(families.output.items), jsonRows(expectedFamilies.items));
  assertNonEmptyDescriptions(families.output.items, 'family');
  assertTemplateGuidance(families.output, /Pick a write family/i, 'template families');

  for (const family of expectedFamilies.items) {
    const writeModes = await runCliJson([
      'tools',
      'templates',
      'write-modes',
      '--family',
      family.family,
      '--format',
      'json',
    ]);
    const expectedWriteModes = listTaskSpecTemplateWriteModes({ family: family.family });
    assert.deepEqual(jsonRows(writeModes.output.items), jsonRows(expectedWriteModes.items));
    assertNonEmptyDescriptions(writeModes.output.items, `write modes for ${family.family}`);
    assertTemplateGuidance(writeModes.output, /Pick a write_mode/i, `write modes for ${family.family}`);

    const clusters = await runCliJson([
      'tools',
      'templates',
      'clusters',
      '--family',
      family.family,
      '--format',
      'json',
    ]);
    const expectedClusters = listTaskSpecTemplateClusters({ family: family.family });
    assert.deepEqual(jsonRows(clusters.output.items), jsonRows(expectedClusters.items));
    assertNonEmptyDescriptions(clusters.output.items, `clusters for ${family.family}`);
    assertTemplateGuidance(clusters.output, /Pick a cluster/i, `clusters for ${family.family}`);

    for (const writeMode of expectedWriteModes.items) {
      for (const cluster of expectedClusters.items) {
        const operations = await runCliJson([
          'tools',
          'templates',
          'operations',
          '--family',
          family.family,
          '--cluster',
          cluster.cluster_id,
          '--write-mode',
          writeMode.write_mode,
          '--format',
          'json',
        ]);
        const expectedOperations = listTaskSpecTemplateOperations({
          family: family.family,
          cluster: cluster.cluster_id,
          writeMode: writeMode.write_mode,
        });
        assert.deepEqual(jsonRows(operations.output.items), jsonRows(expectedOperations.items));
        assertNonEmptyDescriptions(operations.output.items, `operations for ${family.family}/${cluster.cluster_id}/${writeMode.write_mode}`);
        assertTemplateGuidance(operations.output, /Pick an operation/i, `operations for ${family.family}/${cluster.cluster_id}/${writeMode.write_mode}`);

        for (const operation of expectedOperations.items) {
          const quickAccess = await runCliJson([
            'tools',
            'templates',
            'quick-access',
            '--family',
            family.family,
            '--cluster',
            cluster.cluster_id,
            '--operation',
            operation.operation_id,
            '--write-mode',
            writeMode.write_mode,
            '--format',
            'json',
          ]);
          const expectedQuickAccess = listTaskSpecTemplateQuickAccess({
            family: family.family,
            cluster: cluster.cluster_id,
            operation: operation.operation_id,
            writeMode: writeMode.write_mode,
          });
          assert.deepEqual(jsonRows(quickAccess.output.items), jsonRows(expectedQuickAccess.items));
          assertNoTopLevelGuidance(quickAccess.output, `quick-access for ${family.family}/${cluster.cluster_id}/${operation.operation_id}`);
        }
      }
    }
  }

  assertNoLifecycleTemplateIds(JSON.stringify(families.output));
});

test('CLI ReadContext template index mirrors descriptor-backed template index', async () => {
  const families = await runCliJson(['tools', 'read-templates', 'families', '--format', 'json']);
  const expectedFamilies = listReadContextTemplateFamilies();
  assert.deepEqual(jsonRows(families.output.items), jsonRows(expectedFamilies.items));
  assertNonEmptyDescriptions(families.output.items, 'read families');
  assertTemplateGuidance(families.output, /Pick a read family/i, 'read families');

  for (const family of expectedFamilies.items) {
    const clusters = await runCliJson([
      'tools',
      'read-templates',
      'clusters',
      '--family',
      family.family,
      '--format',
      'json',
    ]);
    const expectedClusters = listReadContextTemplateClusters({ family: family.family });
    assert.deepEqual(jsonRows(clusters.output.items), jsonRows(expectedClusters.items));
    assertNonEmptyDescriptions(clusters.output.items, `read clusters for ${family.family}`);
    assertTemplateGuidance(clusters.output, /Pick a read cluster/i, `read clusters for ${family.family}`);

    for (const cluster of expectedClusters.items) {
      const templates = await runCliJson([
        'tools',
        'read-templates',
        'list',
        '--family',
        family.family,
        '--cluster',
        cluster.cluster,
        '--format',
        'json',
      ]);
      const expectedTemplates = listReadContextTemplates({
        family: family.family,
        cluster: cluster.cluster,
      });
      assert.deepEqual(jsonRows(templates.output.items), jsonRows(expectedTemplates.items));
      assertNonEmptyDescriptions(templates.output.items, `read templates for ${family.family}/${cluster.cluster}`);
      assertNoTopLevelGuidance(templates.output, `read templates for ${family.family}/${cluster.cluster}`);
    }
  }

  assertNoLifecycleTemplateIds(JSON.stringify(families.output));
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
  assert.match(stderr.join(''), /Unsupported BlueprintHelper CLI subcommand: tools\.templates/);
});

test('runCli rejects descriptor-driven task preview when runtime profile has no adapter', async (t) => {
  const workspace = await fs.mkdtemp(path.join(os.tmpdir(), 'tmp-cli-runtime-gate-'));
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
  let runtimeProfileCalls = 0;
  const exitCode = await runCli({
    argv: [
      'task',
      'preview',
      '--file',
      taskSpecPath,
      '--format',
      'json',
    ],
    cwd: workspaceRoot(),
    runner: {
      async readReferenceContext() {
        throw new Error('runtime gate failure must not read reference context.');
      },
      async previewTask() {
        throw new Error('runtime gate failure must not preview tasks.');
      },
      async executeTask() {
        throw new Error('runtime gate failure must not execute tasks.');
      },
      async getTaskResult() {
        throw new Error('runtime gate failure must not read task results.');
      },
    },
    bridge: runtimeProfileBridge([], () => {
      runtimeProfileCalls += 1;
    }),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 2);
  assert.equal(runtimeProfileCalls, 1);
  assert.deepEqual(stderr, []);
  const output = JSON.parse(stdout.join('')) as Record<string, unknown>;
  assert.equal(output.ok, false);
  assert.equal(output.error_code, 'capability_unavailable');
  assert.match(output.message as string, /graphwrite\.execute/);
});

test('runCli hydrates descriptor runtime state from runtime profile for compile-only task preview', async (t) => {
  const workspace = await fs.mkdtemp(path.join(os.tmpdir(), 'tmp-cli-runtime-profile-gate-'));
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
  let runtimeProfileCalls = 0;
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
    bridge: runtimeProfileBridge(createDescriptorFixtureRuntimeCapabilityState().registered_runtime_adapter_ids, () => {
      runtimeProfileCalls += 1;
    }),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 0);
  assert.equal(runtimeProfileCalls, 1);
  assert.deepEqual(stderr, []);
  const output = JSON.parse(stdout.join('')) as Record<string, any>;
  const taskPlan = await readJsonArtifact(output.artifacts.task_plan);
  assert.equal(taskPlan.steps.some((step: Record<string, any>) =>
    step.capability === 'graph_write'
    && step.write.strategy === 'owned_graph_edit'), true);
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
      ...ACTIVE_RUNTIME_ARGS,
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
  assert.match(help, /bh tools read-templates families --format json/);
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

async function runCliJsonWithRuntimeProfile(
  argv: string[],
  registeredRuntimeAdapterIds: string[],
): Promise<{ output: Record<string, any>; stderr: string[] }> {
  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv,
    cwd: workspaceRoot(),
    bridge: runtimeProfileBridge(registeredRuntimeAdapterIds),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 0);
  return {
    output: JSON.parse(stdout.join('')) as Record<string, any>,
    stderr,
  };
}

function runtimeProfileBridge(
  registeredRuntimeAdapterIds: string[],
  onCall?: () => void,
) {
  return {
    async sendCommand(command: string) {
      assert.equal(command, 'get_runtime_profile');
      onCall?.();
      return {
        request_id: 'test_runtime_profile',
        success: true,
        result: {
          schema: 'RuntimeProfile.v1',
          runtime_profile: {
            status: 'ok',
            capability_runtime_state: {
              registered_runtime_adapter_ids: registeredRuntimeAdapterIds,
              allow_write_capabilities: true,
              allow_high_risk_capabilities: true,
            },
          },
        },
      };
    },
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

function jsonRows(rows: unknown[]): unknown[] {
  return rows
    .map((row) => JSON.parse(JSON.stringify(row)) as unknown)
    .sort((left, right) => JSON.stringify(left).localeCompare(JSON.stringify(right)));
}

function assertNonEmptyDescriptions(rows: unknown[], label: string): void {
  for (const row of rows) {
    assert.equal(typeof (row as Record<string, unknown>).description, 'string', `${label} row must declare description`);
    assert.equal(((row as Record<string, string>).description ?? '').trim().length > 0, true, `${label} row description must be non-empty`);
  }
}

function assertTemplateGuidance(output: Record<string, unknown>, pattern: RegExp, label: string): void {
  assert.equal(typeof output.guidance, 'string', `${label} output must declare navigation guidance`);
  assert.match(output.guidance as string, pattern, `${label} output guidance should explain the next index hop`);
}

function assertNoTopLevelGuidance(output: Record<string, unknown>, label: string): void {
  assert.equal(Object.hasOwn(output, 'guidance'), false, `${label} is a leaf output and should not declare top-level guidance`);
}

function assertNoLifecycleTemplateIds(text: string): void {
  assert.doesNotMatch(text, /blueprint_open_editor/);
  assert.doesNotMatch(text, /blueprint_close_editor/);
  assert.doesNotMatch(text, /open_editor/);
  assert.doesNotMatch(text, /close_editor/);
}
