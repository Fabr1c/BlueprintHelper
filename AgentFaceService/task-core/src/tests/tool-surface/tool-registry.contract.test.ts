import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import test from 'node:test';
import { getBlueprintHelperToolRegistry } from '../../tool-surface/tool-registry.js';
import type { TaskSpecRunner } from '../../task/service/task-spec-runner.js';

const expectedToolNames = [
  'blueprinthelper_read_task_context',
  'blueprinthelper_read_reference_context',
  'blueprinthelper_preview_task',
  'blueprinthelper_execute_task',
  'blueprinthelper_get_task_result',
  'blueprinthelper_read_agent_guide',
  'blueprinthelper_get_debug_case',
  'blueprinthelper_read_context',
  'blueprint_get_runtime_profile',
  'blueprinthelper_request_write_session',
  'blueprinthelper_diagnostics',
  'blueprinthelper_diagnostics_runtime',
  'blueprint_open_editor',
];

const frozenToolNames = [
  'blueprint_get_editor_context',
  'blueprint_get_logic_md',
  'blueprint_create_asset',
  'blueprint_read_components',
  'blueprint_add_component',
  'blueprint_set_component_property',
  'blueprint_set_component_properties',
  'blueprint_remove_component',
  'blueprint_validate_json',
  'blueprint_export_to_json',
  'blueprint_get_logic',
  'blueprint_get_logic_json',
  'blueprint_import_json_to_graph',
  'blueprint_import_agent_graph',
  'blueprint_compile_blueprint',
  'blueprint_open_asset',
  'blueprint_list_assets',
  'blueprint_search_assets',
  'blueprint_save_asset',
  'blueprint_get_asset_info',
  'blueprint_list_graphs',
  'blueprint_list_variables',
  'blueprint_list_event_dispatchers',
  'blueprint_add_variable',
  'blueprint_remove_variable',
  'blueprint_add_graph',
  'blueprint_remove_graph',
  'blueprint_add_event_dispatcher',
  'blueprint_delete_nodes',
  'blueprint_get_widget_tree',
  'blueprint_add_widget',
  'blueprint_remove_widget',
  'blueprint_move_widget',
  'blueprint_get_widget_properties',
  'blueprint_set_widget_property',
  'blueprint_get_object_properties',
  'blueprint_set_object_property',
  'blueprint_get_datatable_rows',
  'blueprint_add_datatable_row',
  'blueprint_update_datatable_row',
  'blueprint_delete_datatable_row',
  'blueprint_undo',
  'blueprint_redo',
  'blueprint_play_in_editor',
  'blueprint_stop_pie',
  'blueprint_create_blueprint',
  'blueprint_exec_console_command',
  'blueprint_close_editor',
  'blueprint_build_project',
];

test('shared registry covers only the current non-frozen CLI tool-name surface', () => {
  const registry = getBlueprintHelperToolRegistry();
  const names = registry.map((tool) => tool.name).sort();

  for (const expected of expectedToolNames) {
    assert.ok(names.includes(expected), expected);
  }
  assert.deepEqual(names, [...expectedToolNames].sort());
  assert.equal(new Set(names).size, names.length);
});

test('shared registry does not expose frozen direct tools', () => {
  const registry = getBlueprintHelperToolRegistry();
  const names = new Set(registry.map((tool) => tool.name));

  for (const name of frozenToolNames) {
    assert.equal(names.has(name), false, name);
  }
});

test('read agent guide resolves from task-core cwd through plugin ancestor layout', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_agent_guide');
  assert.ok(tool);

  const result = await tool.execute({}, {
    cwd: process.cwd(),
    bridge: {} as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  assert.equal(result.operation, 'blueprinthelper_read_agent_guide');
  assert.equal(result.data?.['schema'], 'AgentGuideMarkdown.v1');
  assert.equal(result.data?.['format'], 'markdown');
  assert.match(String(result.data?.['markdown'] ?? ''), /BlueprintHelper/i);
});

test('read agent guide resolves from project root plugin copy layout', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_agent_guide');
  assert.ok(tool);

  const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-agent-guide-'));
  try {
    const guideDir = path.join(tempRoot, 'Plugins', 'BlueprintHelper', 'BlueprintHelper', 'Resources', 'AgentGuide');
    fs.mkdirSync(guideDir, { recursive: true });
    fs.writeFileSync(
      path.join(guideDir, '00_Agent_Onboarding_Index_20260504.md'),
      '# BlueprintHelper AgentGuide\n\nFixture guide.',
      'utf8',
    );

    const result = await tool.execute({}, {
      cwd: tempRoot,
      bridge: {} as never,
      taskRunner: {} as TaskSpecRunner,
    });

    assert.equal(result.ok, true);
    assert.equal(result.data?.['schema'], 'AgentGuideMarkdown.v1');
    assert.equal(Object.hasOwn(result.data ?? {}, 'path'), false);
    assert.match(String(result.data?.['markdown'] ?? ''), /Fixture guide/);
  } finally {
    fs.rmSync(tempRoot, { recursive: true, force: true });
  }
});

test('preview task registry handler calls TaskSpecRunner.previewTask', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_preview_task');
  assert.ok(tool);
  let called = false;
  const runner = {
    previewTask: async () => {
      called = true;
      return {
        previewId: 'preview_registry_001',
        taskPlan: {
          schema: 'BlueprintHelper.TaskPlan.v1',
          task_name: 'RegistryPreview',
          task_type: 'edit_blueprint_graph',
          context_id: 'ctx_registry',
          target_assets: ['/Game/BP_Player'],
          execution_policy: { dry_run_mode: 'full', should_compile: true, should_save: false },
          steps: [],
        },
        passed: true,
        issues: [],
        toolResult: {
          ok: true,
          schema: 'BlueprintHelper.ToolResult.v1',
          operation: 'preview_task',
          trace_id: 'trace_registry',
          status: 'dry_run',
          modified: false,
        },
      };
    },
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const result = await tool.execute({
    task_spec: {
      schema: 'BlueprintHelper.TaskSpec.v1',
      context_id: 'ctx_registry',
      task_type: 'edit_blueprint_graph',
      feature_name: 'RegistryPreview',
      target: { asset_path: '/Game/BP_Player', target_type: 'blueprint' },
      scope_policy: { graph_name: 'EventGraph', allow_modify_user_nodes: false },
      behavior: {
        graph_strategy: 'append_new_owned_graph',
        entries: [{
          entry_type: 'custom_event',
          name: 'RegistryPreview',
          body: { schema: 'BlueprintLogicSpec.v1', statements: [] },
        }],
      },
      execution_policy: {
        dry_run_mode: 'full',
        on_missing_capability: 'stop_and_report',
      },
      validation: { should_compile: true, should_save: false },
    },
  }, {
    cwd: process.cwd(),
    bridge: {} as never,
    taskRunner: runner,
  });

  assert.equal(called, true);
  assert.equal(result.operation, 'preview_task');
});

