import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { createHelpBuilder } from './help-builder.js';
import { buildHelpText } from './help.js';

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
  assert.match(help, /bh tools read-templates domains --format json/);
  assert.match(help, /bh tools read-templates compose --domain <domain>/);
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

test('HelpBuilder explains empty-object templates for no-input tools', () => {
	const builder = createHelpBuilder();
	const agentGuideHelp = builder.build(['blueprinthelper_read_agent_guide']);
	const runtimeProfileHelp = builder.build(['blueprint_get_runtime_profile']);

	assert.match(agentGuideHelp, /Root JSON: \{\}\. No parameters\. Use the empty-object template as-is\./);
	assert.match(runtimeProfileHelp, /Root JSON: \{\}\. No parameters\. Use the empty-object template as-is\./);
});

test('global help includes ReadContext template navigation', () => {
	const help = buildHelpText();

  assert.match(help, /bh tools read-templates domains --format json/);
  assert.match(help, /bh tools read-templates compose --domain <domain>/);
  assert.match(help, /bh context read \(\--file <read-spec\.json> \| --json <json> \| --stdin\)/);
  assert.doesNotMatch(help, /bh <tool_name>/);
  assert.doesNotMatch(help, /Default tool names:/);
  assert.doesNotMatch(help, /bh bridge call/);
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
