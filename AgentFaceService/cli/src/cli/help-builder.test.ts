import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { createHelpBuilder } from './help-builder.js';
import { buildHelpText } from './help.js';
import { runCli } from './run.js';
import {
  buildReadonlyToolCommandManifestRegistry,
  listReadContextTemplateClusters,
  listReadContextTemplateFamilies,
  listReadContextTemplates,
  templateNavigationUsageLinesForInputShapes,
} from '@blueprinthelper/task-core/tool-surface/tool-registry';

test('HelpBuilder redirects removed direct preview help to global grouped help', () => {
  const builder = createHelpBuilder();
  const help = builder.build(['blueprinthelper_preview_task']);

  assert.match(help, /BlueprintHelper CLI/);
  assert.match(help, /bh task preview --file <task-spec\.json>/);
  assert.match(help, /bh context read/);
  assert.doesNotMatch(help, /BlueprintHelper CLI help: blueprinthelper_preview_task/);
  assert.doesNotMatch(help, /blueprinthelper_preview_task/);
  assert.doesNotMatch(help, /Direct tool-name CLI entry was removed/);
});

test('HelpBuilder redirects removed lifecycle help to global MCP guidance', () => {
  const builder = createHelpBuilder();
  const help = builder.build(['open_editor']);
  const closeHelp = builder.build(['close_editor']);

  assert.match(help, /BlueprintHelper CLI/);
  assert.match(help, /mcp__blueprint_helper__blueprint_open_editor/);
  assert.doesNotMatch(help, /BlueprintHelper CLI help: open_editor/);
  assert.doesNotMatch(help, /open_editor direct CLI command was removed/);
  assert.match(closeHelp, /BlueprintHelper CLI/);
  assert.match(closeHelp, /mcp__blueprint_helper__blueprint_close_editor/);
  assert.match(closeHelp, /mcp__blueprint_helper__blueprint_close_editor_dialogs/);
  assert.doesNotMatch(closeHelp, /BlueprintHelper CLI help: close_editor/);
  assert.doesNotMatch(closeHelp, /close_editor direct CLI command was removed/);
});

test('HelpBuilder resolves grouped task preview alias through manifest', () => {
  const builder = createHelpBuilder();
  const help = builder.build(['task', 'preview']);

  assert.match(help, /BlueprintHelper CLI help: task preview/);
  assert.match(help, /bare BlueprintHelper\.TaskSpec\.v1/);
  assert.match(help, /bh task preview --file <filled_taskspec\.json> --format summary/);
  assert.doesNotMatch(help, /task_spec/);
  assert.doesNotMatch(help, /args envelope/);
  assert.doesNotMatch(help, /wrapper/i);
});

test('HelpBuilder renders grouped read_context help from ReadContext template navigation', () => {
	const builder = createHelpBuilder();
	const help = builder.build(['context', 'read']);

  assert.match(help, /BlueprintHelper CLI help: context read/);
  assert.match(help, /bh context read --file <read-spec\.json>/);
  assert.match(help, /\$json \| bh context read --stdin --format full/);
  assert.match(help, /bh tools read-templates families --format json/);
  assert.match(help, /bh tools read-templates compose --template <template_id>/);
	assert.doesNotMatch(help, /bh tools templates compose --family <family>/);
});

test('HelpBuilder renders task result as grouped id-only command', () => {
  const builder = createHelpBuilder();
  const help = builder.build(['task', 'result']);

  assert.match(help, /BlueprintHelper CLI help: task result/);
  assert.match(help, /bh task result --id <task_run_id> --format summary/);
  assert.match(help, /CLI options only: --id <task_run_id>\./);
  assert.doesNotMatch(help, /blueprinthelper_get_task_result --file/);
  assert.doesNotMatch(help, /Use a template path before calling the tool/);
});

test('HelpBuilder renders read_context_capabilities help from manifest', () => {
	const builder = createHelpBuilder();
	const help = builder.build(['blueprinthelper_read_context_capabilities']);

	assert.match(help, /BlueprintHelper CLI help: blueprinthelper_read_context_capabilities/);
	assert.match(help, /bh blueprinthelper_read_context_capabilities --json "\{\}" --format json/);
	assert.match(help, /Root JSON: \{\}\. No parameters\. Use the empty-object template as-is\./);
	assert.doesNotMatch(help, /No tool-specific help is registered/);
});

test('HelpBuilder renders read_reference_context help as schema-rooted request', () => {
  const builder = createHelpBuilder();
  const help = builder.build(['blueprinthelper_read_reference_context']);

  assert.match(help, /BlueprintHelper CLI help: blueprinthelper_read_reference_context/);
  assert.match(help, /Root JSON: BlueprintHelper\.ReferenceContextRequest\.v1 with schema field/);
  assert.doesNotMatch(help, /bare BlueprintHelper\.ReferenceContextRequest\.v1/);
});

test('HelpBuilder renders compile blueprint help with explicit target contract', () => {
  const builder = createHelpBuilder();
  const help = builder.build(['blueprint_compile_blueprint']);

  assert.match(help, /BlueprintHelper CLI help: blueprint_compile_blueprint/);
  assert.match(help, /blueprint_compile_blueprint_template\.json/);
  assert.match(help, /target_blueprint/);
  assert.doesNotMatch(help, /active\/focused/i);
  assert.doesNotMatch(help, /active blueprint/i);
});

test('HelpBuilder explains empty-object templates for no-input tools', () => {
	const builder = createHelpBuilder();
	const agentGuideHelp = builder.build(['blueprinthelper_read_agent_guide']);
	const runtimeProfileHelp = builder.build(['blueprint_get_runtime_profile']);

	assert.match(agentGuideHelp, /Root JSON: \{\}\. No parameters\. Use the empty-object template as-is\./);
	assert.match(runtimeProfileHelp, /Root JSON: \{\}\. No parameters\. Use the empty-object template as-is\./);
});

test('global help includes ReadContext template navigation', () => {
  const help = buildHelpText();

  assert.match(help, /bh tools read-templates families --format json/);
  assert.match(help, /bh tools read-templates compose --template <template_id>/);
  assert.match(help, /bh context read \(\--file <read-spec\.json> \| --json <json> \| --stdin\)/);
  assert.match(help, /Use global MCP lifecycle tools/);
  assert.match(help, /mcp__blueprint_helper__blueprint_open_editor/);
  assert.match(help, /mcp__blueprint_helper__blueprint_close_editor/);
  assert.match(help, /mcp__blueprint_helper__blueprint_dismiss_editor_dialogs/);
  assert.match(help, /mcp__blueprint_helper__blueprint_close_editor_dialogs/);
  assert.doesNotMatch(help, /bh <tool_name>/);
  assert.doesNotMatch(help, /Default tool names:/);
  assert.doesNotMatch(help, /bh bridge call/);
  assert.doesNotMatch(help, /bh open_editor/);
  assert.doesNotMatch(help, /bh close_editor/);
});

test('manifest-backed help template navigation mirrors input shape policy', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();
  const builder = createHelpBuilder(registry);

  for (const target of [
    'task preview',
    'task execute',
    'context read',
  ]) {
    const manifest = registry.require(target);
    const help = builder.build(target.split(' '));
    const navigationLines = templateNavigationUsageLinesForInputShapes(manifest.input_shapes);
    assert.equal(navigationLines.length > 0, true, `${target} must have template navigation lines`);
    for (const line of navigationLines) {
      assert.match(help, new RegExp(escapeRegExp(line)), `${target} help must include descriptor-derived navigation: ${line}`);
    }
  }
});

test('runCli --help mirrors manifest help and template refs for every manifest-backed tool', async () => {
  const registry = buildReadonlyToolCommandManifestRegistry();
  const builder = createHelpBuilder(registry);
  const readTemplateIds = await listCliReadContextTemplateIds();

  for (const manifest of registry.list()) {
    const directHelp = await runCliHelp([manifest.tool_id, '--help']);
    assert.equal(directHelp, builder.build([manifest.tool_id]));

    const navigationLines = templateNavigationUsageLinesForInputShapes(manifest.input_shapes);
    for (const line of navigationLines) {
      assert.match(
        directHelp,
        new RegExp(escapeRegExp(line)),
        `${manifest.tool_id} help must include descriptor-derived template navigation: ${line}`,
      );
    }

    if (manifest.input_shapes.includes('readspec')) {
      for (const templateId of manifest.template_refs) {
        assert.equal(
          readTemplateIds.has(templateId),
          true,
          `${manifest.tool_id} read template ref must be listed by bh tools read-templates: ${templateId}`,
        );
      }
    }

    if (manifest.input_shapes.includes('bare_taskspec')) {
      assert.match(directHelp, /bh tools templates families --workflow preview_execute --format json/);
    }
  }
});

test('cli help source does not hardcode route template paths for manifest-backed tools', () => {
  const source = fs.readFileSync(sourcePath('help.ts'), 'utf8');

  assert.doesNotMatch(source, /write\/routes\/graph_/);
  assert.doesNotMatch(source, /read\/routes\/blueprint_logic_/);
  assert.doesNotMatch(source, /const helpEntries/);
});

test('buildHelpText keeps manifest-backed help compact', () => {
  const previewHelp = buildHelpText(['task', 'preview']);
  const readContextHelp = buildHelpText(['context', 'read']);

  assert.match(previewHelp, /bh tools templates families --workflow preview_execute --format json/);
  assert.match(previewHelp, /bh task preview --file <filled_taskspec\.json> --format summary/);
  assert.doesNotMatch(previewHelp, /routes: \[/);
  assert.doesNotMatch(previewHelp, /execution_policy/);
  assert.doesNotMatch(previewHelp, /scope_policy/);
  assert.doesNotMatch(previewHelp, /validation/);
  assert.doesNotMatch(readContextHelp, new RegExp(['bh tools templates', '<tool_id>'].join(' ')));
});

function sourcePath(fileName: string): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', 'src', 'cli', fileName);
}

async function runCliHelp(argv: string[]): Promise<string> {
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
  return stdout.join('').trimEnd();
}

function workspaceRoot(): string {
  return path.join('D:', 'UEProjects', 'Template', 'Plugins', 'BlueprintHelper');
}

async function listCliReadContextTemplateIds(): Promise<ReadonlySet<string>> {
  const ids = new Set<string>();
  const families = listReadContextTemplateFamilies();
  for (const family of families.items) {
    const clusters = listReadContextTemplateClusters({ family: family.family });
    for (const cluster of clusters.items) {
      const templates = listReadContextTemplates({
        family: family.family,
        cluster: cluster.cluster,
      });
      for (const template of templates.items) {
        ids.add(template.template_id);
      }
    }
  }
  return ids;
}

function escapeRegExp(value: string): string {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}
