import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { createHelpBuilder } from './help-builder.js';
import { buildHelpText } from './help.js';

test('HelpBuilder renders preview help from manifest and template dispatch', () => {
  const builder = createHelpBuilder();
  const help = builder.build(['blueprinthelper_preview_task']);

  assert.match(help, /BlueprintHelper CLI help: blueprinthelper_preview_task/);
  assert.match(help, /bh task preview --file <filled_taskspec\.json> --format summary/);
  assert.match(help, /write\/blueprinthelper_preview_task_wrapper_template\.json/);
  assert.match(help, /write\/task_preview_bare_taskspec_template\.json/);
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

test('cli help source does not hardcode route template paths for manifest-backed tools', () => {
  const source = fs.readFileSync(sourcePath('help.ts'), 'utf8');

  assert.doesNotMatch(source, /write\/routes\/graph_/);
  assert.doesNotMatch(source, /read\/routes\/blueprint_logic_/);
  assert.doesNotMatch(source, /const helpEntries/);
});

test('buildHelpText keeps manifest-backed help compact', () => {
  const previewHelp = buildHelpText(['blueprinthelper_preview_task']);
  const readContextHelp = buildHelpText(['blueprinthelper_read_context']);

  assert.match(previewHelp, /write\/blueprinthelper_preview_task_wrapper_template\.json/);
  assert.match(previewHelp, /bh task preview --file <filled_taskspec\.json> --format summary/);
  assert.doesNotMatch(previewHelp, /routes: \[/);
  assert.doesNotMatch(previewHelp, /execution_policy/);
  assert.doesNotMatch(previewHelp, /scope_policy/);
  assert.doesNotMatch(previewHelp, /validation/);
  assert.match(readContextHelp, /read\/routes\/blueprint_logic_function_logic_flow_template\.json/);
});

function sourcePath(fileName: string): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', 'src', 'cli', fileName);
}
