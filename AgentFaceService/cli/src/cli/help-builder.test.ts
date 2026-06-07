import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { createHelpBuilder } from './help-builder.js';
import { buildHelpText } from './help.js';

test('HelpBuilder renders preview help from manifest and TaskSpec composer navigation', () => {
  const builder = createHelpBuilder();
  const help = builder.build(['blueprinthelper_preview_task']);

  assert.match(help, /BlueprintHelper CLI help: blueprinthelper_preview_task/);
  assert.match(help, /bh task preview --file <filled_taskspec\.json> --format summary/);
  assert.match(help, /bh tools templates families --workflow preview_execute --format json/);
  assert.match(help, /bh tools templates compose --family <family>/);
  assert.doesNotMatch(help, new RegExp(['bh tools templates', '<tool_id>'].join(' ')));
  assert.doesNotMatch(help, /execution_policy/);
  assert.doesNotMatch(help, /scope_policy/);
  assert.doesNotMatch(help, /validation/);
});

test('HelpBuilder resolves grouped task preview alias through manifest', () => {
  const builder = createHelpBuilder();
  const help = builder.build(['task', 'preview']);

  assert.match(help, /BlueprintHelper CLI help: task preview/);
  assert.match(help, /bare BlueprintHelper\.TaskSpec\.v1/);
  assert.match(help, /bh task preview --file <filled_taskspec\.json> --format summary/);
});

test('HelpBuilder renders read_context help from ReadContext template navigation', () => {
	const builder = createHelpBuilder();
	const help = builder.build(['blueprinthelper_read_context']);

  assert.match(help, /BlueprintHelper CLI help: blueprinthelper_read_context/);
  assert.match(help, /bh context read --file <read-spec\.json>/);
  assert.match(help, /bh tools read-templates domains --format json/);
  assert.match(help, /bh tools read-templates compose --domain <domain>/);
	assert.doesNotMatch(help, /bh tools templates compose --family <family>/);
});

test('HelpBuilder renders read_context_capabilities help from manifest', () => {
	const builder = createHelpBuilder();
	const help = builder.build(['blueprinthelper_read_context_capabilities']);

	assert.match(help, /BlueprintHelper CLI help: blueprinthelper_read_context_capabilities/);
	assert.match(help, /bh blueprinthelper_read_context_capabilities --json "\{\}" --format json/);
	assert.match(help, /Root JSON: \{\}\. No parameters\. Use the empty-object template as-is\./);
	assert.doesNotMatch(help, /No tool-specific help is registered/);
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
});

test('cli help source does not hardcode route template paths for manifest-backed tools', () => {
  const source = fs.readFileSync(sourcePath('help.ts'), 'utf8');

  assert.doesNotMatch(source, /write\/routes\/graph_/);
  assert.doesNotMatch(source, /read\/routes\/blueprint_logic_/);
  assert.doesNotMatch(source, /const helpEntries/);
});

test('buildHelpText keeps manifest-backed help compact', () => {
  const previewHelp = buildHelpText(['blueprinthelper_preview_task']);
  const readContextHelp = buildHelpText(['blueprinthelper_read_context']);

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
