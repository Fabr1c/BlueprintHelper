import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { buildReadonlyToolCommandManifestRegistry } from './tool-command-manifest-builder.js';

const FORBIDDEN_CATALOG_PATTERNS: readonly [string, RegExp][] = [
  ['route map', /ROUTES_BY_TOOL_ID/],
  ['slot map', /SLOTS_BY_TOOL_ID/],
  ['stop-condition map', /STOP_CONDITIONS_BY_TOOL_ID/],
];

const FORBIDDEN_HELP_PATTERNS: readonly [string, RegExp][] = [
  ['static metrics help entry builder', /metricsHelpEntry/],
  ['static command help map', /STATIC_GROUP_HELP/],
  ['legacy metrics command help factory', /metricsCommandHelp/],
];

test('tool capability catalog no longer owns route slot or stop-condition maps', () => {
  const catalogSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'catalog', 'tool-capability-catalog.ts'),
    'utf8',
  );

  for (const [label, pattern] of FORBIDDEN_CATALOG_PATTERNS) {
    assert.equal(pattern.test(catalogSource), false, `${label} must move to descriptor registry`);
  }
});

test('CLI help no longer owns static metrics help entries', () => {
  const helpBuilderSource = readFileSync(
    path.resolve(pluginRoot(), 'AgentFaceService', 'cli', 'src', 'cli', 'help-builder.ts'),
    'utf8',
  );

  for (const [label, pattern] of FORBIDDEN_HELP_PATTERNS) {
    assert.equal(pattern.test(helpBuilderSource), false, `${label} must move to manifest descriptors`);
  }
});

test('read_context handler consumes adapter-normalized input', () => {
  const readContextHandlerSource = readFileSync(
    path.resolve(
      taskCoreRoot(),
      'src',
      'tool-surface',
      'bridge',
      'read-context',
      'read-context-handler.ts',
    ),
    'utf8',
  );

  assert.equal(
    /ReadContextInputSchema\.parse/.test(readContextHandlerSource),
    false,
    'ReadSpec parsing must stay in InputShapeAdapter, not read-context handler',
  );
});

test('every readonly tool command manifest declares input shape and result policy', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  for (const manifest of registry.list()) {
    assert.equal(manifest.input_shapes.length > 0, true, `${manifest.tool_id} declares input shape`);
    assert.equal(Boolean(manifest.result_policy_id), true, `${manifest.tool_id} declares result policy`);
  }
});

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
}

function pluginRoot(): string {
  return path.resolve(taskCoreRoot(), '../..');
}
