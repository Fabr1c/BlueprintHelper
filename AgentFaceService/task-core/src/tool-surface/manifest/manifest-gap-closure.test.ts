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
  ['task preview status command-kind branch', /command\.kind\s*===\s*['"]task\.preview['"]/],
  ['task execute status command-kind branch', /command\.kind\s*===\s*['"]task\.execute['"]/],
  ['task result status command-kind branch', /command\.kind\s*===\s*['"]task\.result['"]/],
  ['metrics status command-kind branch', /command\.kind\s*===\s*['"]metrics\.report['"]/],
  ['bridge ping status command-kind branch', /command\.kind\s*===\s*['"]bridge\.ping['"]/],
  ['tool-name status alias branch', /command\.toolName\s*===\s*['"]blueprinthelper_/],
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
  const resultRegistrySource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'result', 'result-projection-registry.ts'),
    'utf8',
  );

  for (const [label, pattern] of FORBIDDEN_CLI_OUTPUT_PATTERNS) {
    assert.equal(pattern.test(outputSource), false, `${label} must move to result projection policy`);
  }
  assert.equal(/policyIdForCommandKind/.test(resultRegistrySource), false);
  assert.equal(/commandKind/.test(resultRegistrySource), false);
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
    assert.equal(manifest.help_usage.length > 0, true, `${manifest.tool_id} declares help usage`);
    assert.equal(manifest.source, 'readonly_mirror', `${manifest.tool_id} declares manifest source`);
  }
});

test('expert bridge tools are present in readonly manifest registry', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();
  const manifest = registry.get('blueprinthelper_apply_review_action');

  assert.ok(manifest, 'review apply action must have a manifest for expert CLI execution');
  assert.equal(manifest.audience, 'expert');
  assert.equal(manifest.requires_write_session, true);
  assert.equal(manifest.result_policy_id, 'review_expert_default');
  assert.deepEqual(manifest.input_shapes, ['bridge_payload']);
});

test('CLI grouped template subcommand routing is descriptor-owned', () => {
  const runSource = readFileSync(
    path.resolve(pluginRoot(), 'AgentFaceService', 'cli', 'src', 'cli', 'run.ts'),
    'utf8',
  );
  const subcommandDescriptorSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'cli', 'cli-subcommand-descriptor.ts'),
    'utf8',
  );
  const commandDescriptorSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'cli', 'cli-command-descriptor.ts'),
    'utf8',
  );
  const commandRouterSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'cli', 'cli-command-router.ts'),
    'utf8',
  );

  const oldTemplatesError = ['Unsupported BlueprintHelper CLI tools', 'templates command'].join(' ');
  const oldReadTemplatesError = ['Unsupported BlueprintHelper CLI tools', 'read-templates command'].join(' ');
  assert.equal(runSource.includes(oldTemplatesError), false);
  assert.equal(runSource.includes(oldReadTemplatesError), false);
  assert.equal(/case\s+'families'/.test(runSource), false);
  assert.equal(/case\s+'domains'/.test(runSource), false);
  assert.equal(runSource.includes('routeCliCommand'), true);
  assert.equal(runSource.includes('routeCliSubcommand'), false);
  assert.equal(commandDescriptorSource.includes('listCliSubcommandDescriptors'), true);
  assert.equal(commandRouterSource.includes('listCliCommandDescriptors'), true);
  assert.equal(subcommandDescriptorSource.includes('routeCliSubcommand'), true);
});

test('CLI help consumes template navigation from task-core command descriptors', () => {
  const helpBuilderSource = readFileSync(
    path.resolve(pluginRoot(), 'AgentFaceService', 'cli', 'src', 'cli', 'help-builder.ts'),
    'utf8',
  );

  assert.equal(/bh tools templates/.test(helpBuilderSource), false);
  assert.equal(/bh tools read-templates/.test(helpBuilderSource), false);
  assert.equal(helpBuilderSource.includes('templateNavigationUsageLinesForInputShapes'), true);
});

test('CLI command execution is registry-owned instead of command-kind if chains', () => {
  const runSource = readFileSync(
    path.resolve(pluginRoot(), 'AgentFaceService', 'cli', 'src', 'cli', 'run.ts'),
    'utf8',
  );
  const toolsCommandSource = readFileSync(
    path.resolve(pluginRoot(), 'AgentFaceService', 'cli', 'src', 'cli', 'tools-command.ts'),
    'utf8',
  );
  const metricsRuntimeSource = readFileSync(
    path.resolve(pluginRoot(), 'AgentFaceService', 'cli', 'src', 'cli', 'metrics-runtime.ts'),
    'utf8',
  );

  for (const forbidden of [
    "group === 'task' && action === 'preview'",
    "group === 'task' && action === 'execute'",
    "group === 'task' && action === 'result'",
    "group === 'bridge' && action === 'ping'",
    "group === 'bridge' && action === 'call'",
    "group === 'context' && action === 'read'",
    "group === 'metrics' && isMetricsAction(action)",
    'Unsupported BlueprintHelper CLI tools command:',
    "if (command.kind === 'task.preview')",
    "if (command.kind === 'task.execute')",
    "if (command.kind === 'task.result')",
    "if (command.kind === 'context.read')",
    "if (command.kind === 'bridge.ping')",
    "if (command.kind === 'bridge.call')",
    "command.kind.startsWith('tools.templates.')",
    "command.kind.startsWith('tools.read_templates.')",
  ]) {
    assert.equal(runSource.includes(forbidden), false, `run.ts must not contain ${forbidden}`);
  }
  assert.equal(runSource.includes('CLI_COMMAND_EXECUTORS'), true);
  assert.equal(runSource.includes('resolveCliCommandExecutorDescriptor'), true);
  assert.equal(runSource.includes('listCliCommandKindsByExecutorId'), true);

  for (const forbidden of [
    "if (command.kind === 'tools.domains')",
    "if (command.kind === 'tools.list')",
    "if (command.kind === 'tools.templates.",
    "if (command.kind === 'tools.read_templates.",
  ]) {
    assert.equal(toolsCommandSource.includes(forbidden), false, `tools-command.ts must not contain ${forbidden}`);
  }
  assert.equal(toolsCommandSource.includes('TOOLS_COMMAND_EXECUTORS'), true);
  assert.equal(toolsCommandSource.includes('resolveCliCommandExecutorDescriptor'), true);

  for (const forbidden of [
    "command.kind === 'task.preview'",
    "command.kind === 'task.execute'",
    "command.kind === 'task.result'",
  ]) {
    assert.equal(metricsRuntimeSource.includes(forbidden), false, `metrics-runtime.ts must not contain ${forbidden}`);
  }
  assert.equal(metricsRuntimeSource.includes('metricsToolName'), true);
});

test('Bridge dispatcher uses registered handlers instead of tool-name if-chain', () => {
  const dispatcherSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'bridge', 'bridge-tool-dispatcher.ts'),
    'utf8',
  );
  const registrySource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'bridge', 'bridge-tool-handler-registry.ts'),
    'utf8',
  );
  const descriptorSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'bridge', 'bridge-tool-descriptor.ts'),
    'utf8',
  );
  const commandMapSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'bridge', 'bridge-tool-command-map.ts'),
    'utf8',
  );
  const schemasSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'bridge', 'bridge-tool-schemas.ts'),
    'utf8',
  );

  assert.equal(/if\s*\(\s*toolName\s*===\s*['"]blueprinthelper_/.test(dispatcherSource), false);
  assert.equal(dispatcherSource.includes('getBridgeToolHandler'), true);
  assert.equal(registrySource.includes('registerBridgeToolHandler'), true);
  assert.equal(registrySource.includes("registerBridgeToolHandler('blueprinthelper_"), false);
  assert.equal(registrySource.includes('getBridgeToolDescriptor'), true);
  assert.equal(descriptorSource.includes('BRIDGE_TOOL_DESCRIPTORS'), true);
  assert.equal(descriptorSource.includes('allow_cli_bridge_call'), false);
  assert.equal(commandMapSource.includes('BRIDGE_TOOL_DESCRIPTORS'), true);
  assert.equal(commandMapSource.includes('blueprinthelper_get_debug_case:'), false);
  assert.equal(schemasSource.includes('BRIDGE_TOOL_DESCRIPTORS'), true);
  assert.equal(schemasSource.includes('blueprinthelper_read_context:'), false);
});

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
}

function pluginRoot(): string {
  return path.resolve(taskCoreRoot(), '../..');
}
