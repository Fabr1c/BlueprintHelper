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

const FORBIDDEN_DESCRIPTOR_REGISTRY_PATTERNS: readonly [string, RegExp][] = [
  ['route resolver branch', /function\s+resolveRouteRefs/],
  ['slot resolver branch', /function\s+resolveSlotRefs/],
  ['stop-condition tool-name branch', /stopConditionsByToolName/],
  ['recommended-invocation tool-name branch', /descriptor\.tool_name\s*===/],
  ['tool-id equality branch', /toolId\s*===/],
];

const FORBIDDEN_HELP_PATTERNS: readonly [string, RegExp][] = [
  ['static metrics help entry builder', /metricsHelpEntry/],
  ['static command help map', /STATIC_GROUP_HELP/],
  ['legacy metrics command help factory', /metricsCommandHelp/],
  ['manifest usage tool-name switch', /function\s+formatUsage/],
  ['manifest notes tool-name switch', /function\s+toolSpecificNotes/],
  ['local command descriptor table', /LOCAL_COMMAND_HELP_DESCRIPTORS/],
  ['metrics local option factory', /function\s+metricsCommonOptions/],
  ['lifecycle local descriptor factory', /function\s+lifecycleLocalCommandDescriptor/],
];

const FORBIDDEN_GENERIC_BRIDGE_HANDLER_PATTERNS: readonly [string, RegExp][] = [
  ['logic md tool-name payload branch', /blueprint_get_logic/],
  ['logic json tool-name payload branch', /blueprint_get_logic_json/],
  ['handler-local payload normalizer', /normalizeBridgePayload/],
];

const FORBIDDEN_CLI_OUTPUT_PATTERNS: readonly [string, RegExp][] = [
  ['execute preview id local pruning', /stripExecutePreviewId/],
  ['metrics markdown local pruning', /compactMetricsOutput/],
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

test('tool capability descriptor registry is data-driven and has no tool-id route slot branches', () => {
  const descriptorRegistrySource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'catalog', 'tool-capability-descriptor-registry.ts'),
    'utf8',
  );

  for (const [label, pattern] of FORBIDDEN_DESCRIPTOR_REGISTRY_PATTERNS) {
    assert.equal(pattern.test(descriptorRegistrySource), false, `${label} must move into descriptor data`);
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

test('generic Bridge handler consumes descriptor-shaped payloads without tool-name payload branches', () => {
  const handlerSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'bridge', 'generic-bridge-tool-handler.ts'),
    'utf8',
  );

  for (const [label, pattern] of FORBIDDEN_GENERIC_BRIDGE_HANDLER_PATTERNS) {
    assert.equal(pattern.test(handlerSource), false, `${label} must move out of generic bridge handler`);
  }
});

test('CLI output delegates pruning and compact result shaping to result projection policy', () => {
  const outputSource = readFileSync(
    path.resolve(pluginRoot(), 'AgentFaceService', 'cli', 'src', 'cli', 'output.ts'),
    'utf8',
  );

  for (const [label, pattern] of FORBIDDEN_CLI_OUTPUT_PATTERNS) {
    assert.equal(pattern.test(outputSource), false, `${label} must move to result projection policy`);
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
