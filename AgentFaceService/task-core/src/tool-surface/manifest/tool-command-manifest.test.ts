import assert from 'node:assert/strict';
import test from 'node:test';

import {
  listToolCapabilities,
  TOOL_COMMAND_MANIFEST_SCHEMA as exportedSchema,
} from '../tool-registry.js';
import {
  buildReadonlyToolCommandManifestRegistry,
} from './tool-command-manifest-builder.js';
import {
  createToolCommandManifestRegistry,
} from './tool-command-manifest-registry.js';
import {
  TOOL_COMMAND_MANIFEST_SCHEMA,
  type ToolCommandManifest,
} from './tool-command-manifest.js';

test('ToolCommandManifest exports the stable read-only schema id', () => {
  assert.equal(TOOL_COMMAND_MANIFEST_SCHEMA, 'BlueprintHelper.ToolCommandManifest.v1');
  assert.equal(exportedSchema, TOOL_COMMAND_MANIFEST_SCHEMA);
  const manifest: ToolCommandManifest = {
    schema: TOOL_COMMAND_MANIFEST_SCHEMA,
    tool_id: 'blueprint.plan.taskspec.preview',
    tool_name: 'blueprinthelper_preview_task',
    aliases: ['task preview'],
    domain: 'blueprint',
    kind: 'plan',
    risk: 'low',
    audience: 'default',
    agent_role: 'task-worker',
    requires_bridge: false,
    requires_write_session: false,
    input_shapes: ['wrapped_taskspec_preview', 'bare_taskspec'],
    handler_id: 'blueprinthelper_preview_task',
    result_policy_id: 'task_preview_default',
    template_refs: ['blueprinthelper_preview_task_wrapper', 'task_preview_bare_taskspec'],
    route_refs: ['blueprint.create_feature', 'graph.append.container_action'],
    recommended_invocations: ['bh task preview --file <filled_taskspec.json> --format summary'],
    help_usage: ['bh task preview --file <filled_taskspec.json> --format summary'],
    help_notes: [],
    stop_conditions: ['tool_unavailable', 'preview_blocked'],
    source: 'readonly_mirror',
  };
  assert.equal(manifest.schema, TOOL_COMMAND_MANIFEST_SCHEMA);
});

test('manifest mirror exposes every default capability returned by listToolCapabilities', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();
  const blueprintPlan = listToolCapabilities({ domain: 'blueprint', kind: 'plan' });
  assert.equal(
    blueprintPlan.items.every((item) => registry.has(item.id)),
    true,
  );
  const preview = registry.require('blueprint.plan.taskspec.preview');
  assert.equal(preview.tool_name, 'blueprinthelper_preview_task');
  assert.deepEqual(preview.aliases, ['task preview']);
  assert.equal(preview.handler_id, 'blueprinthelper_preview_task');
  assert.equal(preview.result_policy_id, 'task_preview_default');
  assert.deepEqual(preview.help_usage, ['bh task preview --file <filled_taskspec.json> --format summary']);
  assert.deepEqual(preview.metrics_identity, {
    capability: 'blueprint.plan',
    semantic_operation: 'blueprint.plan.taskspec.preview',
  });
  assert.equal(preview.input_shapes.includes('wrapped_taskspec_preview'), true);
  assert.equal(preview.input_shapes.includes('bare_taskspec'), true);
  assert.equal(registry.get('task preview')?.tool_id, preview.tool_id);
  assert.equal(registry.get('task execute')?.tool_id, 'blueprint.write.taskspec.execute');
  assert.equal(registry.get('context read')?.tool_id, 'blueprint.read.context.logic_flow');
  assert.equal(registry.get('blueprinthelper_preview_task'), undefined);
  assert.equal(registry.get('blueprinthelper_execute_task'), undefined);
  assert.equal(registry.get('blueprinthelper_read_context'), undefined);
  const removedMarkdownFormat = ['logic', 'md'].join('_');
  const removedMarkdownTool = ['blueprint_get', 'logic', 'md'].join('_');
  assert.equal(registry.get(removedMarkdownTool), undefined);
  assert.equal(registry.list().some((entry) => JSON.stringify(entry).includes(removedMarkdownFormat)), false);
  assert.equal(registry.get('task result')?.tool_id, 'project.read.task_result');
});

test('manifest mirror keeps GraphWrite route ids descriptor-backed', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();
  const manifest = registry.require('blueprint.write.taskspec.execute');
  assert.equal(manifest.route_refs.includes('graph.replace.function_body'), true);
  assert.equal(manifest.route_refs.includes('blueprint.create_feature'), true);
});

test('manifest registry rejects duplicate ids and ambiguous lookup keys without canonical aliases', () => {
  const base = makeManifest({
    tool_id: 'test.one',
    tool_name: 'shared_tool',
    aliases: ['shared alias'],
  });
  assert.throws(
    () => createToolCommandManifestRegistry([
      base,
      makeManifest({ tool_id: 'test.one', tool_name: 'other_tool' }),
    ]),
    /Duplicate BlueprintHelper tool command manifest id/,
  );

  const registry = createToolCommandManifestRegistry([
    base,
    makeManifest({
      tool_id: 'test.two',
      tool_name: 'shared_tool',
      aliases: ['shared alias'],
    }),
  ]);
  assert.equal(registry.get('shared_tool'), undefined);
  assert.equal(registry.has('shared alias'), false);
  assert.throws(
    () => registry.require('shared_tool'),
    /Ambiguous BlueprintHelper tool command manifest lookup/,
  );

  const canonicalRegistry = createToolCommandManifestRegistry([
    base,
    makeManifest({
      tool_id: 'test.two',
      tool_name: 'shared_tool',
      aliases: ['shared alias'],
    }),
  ], {
    canonicalAliases: new Map([
      ['shared_tool', 'test.two'],
      ['shared alias', 'test.one'],
    ]),
  });
  assert.equal(canonicalRegistry.require('shared_tool').tool_id, 'test.two');
  assert.equal(canonicalRegistry.require('shared alias').tool_id, 'test.one');
});

function makeManifest(overrides: Partial<ToolCommandManifest>): ToolCommandManifest {
  return {
    schema: TOOL_COMMAND_MANIFEST_SCHEMA,
    tool_id: 'test.default',
    tool_name: 'test_tool',
    aliases: [],
    domain: 'debug',
    kind: 'diagnose',
    risk: 'low',
    audience: 'default',
    agent_role: 'sourcecode-explorer',
    requires_bridge: false,
    requires_write_session: false,
    input_shapes: ['empty_object'],
    handler_id: 'test_tool',
    result_policy_id: 'local_default',
    template_refs: [],
    route_refs: [],
    recommended_invocations: [],
    help_usage: [],
    help_notes: [],
    stop_conditions: ['tool_unavailable'],
    source: 'readonly_mirror',
    ...overrides,
  };
}
