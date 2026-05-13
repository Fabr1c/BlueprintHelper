import assert from 'node:assert/strict';
import test from 'node:test';
import type { BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import {
  compileTaskSpecToTaskPlan,
  summarizeTaskPlan,
} from '@blueprinthelper/task-core/task/compiler/task-compiler';
import type { TaskCompiler } from '../../mcp/tools/task-tools.js';
import { createTaskSpecRunner } from '@blueprinthelper/task-core/task/service/task-spec-runner';
import {
  type TaskPlan,
  type TaskSpec,
  TaskSpecSchema,
} from '@blueprinthelper/task-core/task/schema/task-schemas';
import {
  invokeTool,
  registerWithBridge as registerWithBridgeBase,
} from '../../test-support/test-harness.js';

const taskCompilerForTaskToolTests: TaskCompiler = async (taskSpec, dryRun) => {
  const task_plan = compileTaskSpecForTaskToolTests(taskSpec);
  return {
    schema: 'BlueprintHelper.TaskCompilerResult.v1',
    task_plan,
    bridge_payload: {
      task_plan,
      dry_run: dryRun,
    },
    task_plan_summary: summarizeTaskPlan(task_plan),
  };
};

function registerWithBridge(
  sendCommand: (command: string, payload?: Record<string, unknown>) => Promise<BridgeResponse>,
) {
  return registerWithBridgeBase(sendCommand, { taskCompiler: taskCompilerForTaskToolTests });
}

function compileTaskSpecForTaskToolTests(taskSpec: TaskSpec): TaskPlan {
  if (taskSpec.task_type !== 'create_asset') {
    return compileTaskSpecToTaskPlan(TaskSpecSchema.parse(taskSpec));
  }

  const behavior = taskSpec.behavior as Record<string, unknown>;
  const asset = behavior.asset as Record<string, unknown>;
  const assetType = normalizeAssetTypeForTaskToolTests(String(asset.asset_type ?? ''));
  const op: { op: string; [key: string]: unknown } = {
    op: 'create_asset',
    asset_type: assetType,
    value_type: asset.value_type,
    collision: asset.collision_policy,
  };
  for (const field of ['parent_class', 'fields', 'row_struct', 'data_asset_class'] as const) {
    if (asset[field] !== undefined) {
      op[field] = asset[field];
    }
  }

  return {
    schema: 'BlueprintHelper.TaskPlan.v1',
    task_name: taskSpec.feature_name,
    task_type: taskSpec.task_type,
    context_id: taskSpec.context_id,
    target_assets: [taskSpec.target.asset_path],
    execution_policy: {
      dry_run_mode: taskSpec.execution_policy.dry_run_mode,
      should_compile: taskSpec.validation.should_compile,
      should_save: taskSpec.validation.should_save,
    },
    steps: [
      {
        step_id: 'step_001',
        capability: 'asset_factory',
        target: {
          asset_path: taskSpec.target.asset_path,
        },
        write: {
          strategy: 'asset_create',
          ops: [op],
        },
      },
    ],
  };
}

function normalizeAssetTypeForTaskToolTests(assetType: string): string {
  const normalized = assetType.trim().toLowerCase();
  const aliases: Record<string, string> = {
    blueprint: 'blueprint_class',
    actor: 'blueprint_class',
    blueprintclass: 'blueprint_class',
    datatable: 'data_table',
    widget: 'widget_blueprint',
    widgetblueprint: 'widget_blueprint',
  };
  return aliases[normalized] ?? normalized;
}

function makeTaskSpec() {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_test',
    task_type: 'edit_blueprint_graph',
    feature_name: 'DoorFeature',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_DoorFeature',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [
        {
          entry_type: 'custom_event',
          name: 'ToggleDoor',
          body: {
            schema: 'BlueprintLogicSpec.v1',
            statements: [
              {
                kind: 'call_function',
                name: 'PrintString',
                args: {
                  InString: {
                    kind: 'literal',
                    value_type: 'string',
                    value: 'hello',
                  },
                },
              },
            ],
          },
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}

function makeVariableTaskSpec() {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_variables',
    task_type: 'edit_blueprint_variables',
    feature_name: 'DoorVariables',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    behavior: {
      variable_strategy: 'member_variables',
      variables: [
        {
          op: 'ensure_member_variable',
          name: 'bDoorOpen',
          pin_type: { category: 'bool' },
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: true,
      should_save: false,
    },
  };
}

function makeAssetFactoryTaskSpec() {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_asset_factory',
    task_type: 'create_asset',
    feature_name: 'InteractInput',
    target: {
      asset_path: '/Game/Input/IA_Interact',
      target_type: 'asset',
    },
    behavior: {
      asset_strategy: 'ensure_asset',
      asset: {
        asset_type: 'input_action',
        value_type: 'bool',
        collision_policy: 'reuse_if_exists',
      },
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: true,
    },
  };
}

function makeStructureAssetFactoryTaskSpec() {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_asset_factory_structure',
    task_type: 'create_asset',
    feature_name: 'DataTableSmokeRow',
    target: {
      asset_path: '/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow',
      target_type: 'asset',
    },
    behavior: {
      asset_strategy: 'ensure_asset',
      asset: {
        asset_type: 'structure',
        fields: [
          { name: 'Damage', type: 'float' },
          { name: 'Ammo', type: 'int' },
        ],
        collision_policy: 'replace_existing',
      },
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: true,
    },
  };
}

function makeDataTableAssetFactoryTaskSpec() {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_asset_factory_data_table',
    task_type: 'create_asset',
    feature_name: 'DataTableSmoke',
    target: {
      asset_path: '/Game/BlueprintHelper/Smoke/DT_DataTableSmoke',
      target_type: 'asset',
    },
    behavior: {
      asset_strategy: 'ensure_asset',
      asset: {
        asset_type: 'datatable',
        row_struct: '/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow',
        collision_policy: 'replace_existing',
      },
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: true,
    },
  };
}

test('task-level tools are registered without frozen legacy tools', () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));

  for (const name of [
    'blueprinthelper_read_task_context',
    'blueprinthelper_read_reference_context',
    'blueprinthelper_preview_task',
    'blueprinthelper_execute_task',
    'blueprinthelper_get_task_result',
  ]) {
    assert.equal(tools.has(name), true, name);
  }
});

test('read_task_context does not infer graph names from feature_name', async () => {
  const tools = registerWithBridge(async (command): Promise<BridgeResponse> => ({
    request_id: command,
    success: true,
    result: command === 'list_graphs'
      ? { graphs: [{ name: 'EventGraph', graph_type: 'EventGraph' }] }
      : {},
  }));

  const tool = tools.get('blueprinthelper_read_task_context');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    target: { asset_path: '/Game/BP/BP_Door' },
    feature_name: 'DoorFeature',
  });

  assert.equal(result.isError, false);
  const data = result.structuredContent?.data as Record<string, unknown>;
  const constraints = data.recommended_constraints as Record<string, unknown>;
  assert.equal(Object.hasOwn(constraints, 'recommended_graph_name'), false);
  assert.equal(Object.hasOwn(data, 'intent'), false);
});

test('preview_task compiles P1 AssetFactory TaskSpec and previews a UE TaskPlan', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        status: 'dry_run',
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
          steps: [],
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_preview_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeAssetFactoryTaskSpec() });

  assert.equal(result.isError, false);
  assert.equal(calls.length, 1);
  assert.equal(calls[0]?.command, 'preview_task_plan');
  const taskPlan = calls[0]?.payload?.task_plan as Record<string, unknown>;
  const steps = taskPlan.steps as Array<Record<string, unknown>>;
  assert.equal(steps[0]?.capability, 'asset_factory');
  assert.equal(Object.hasOwn(steps[0] ?? {}, 'operation'), false);
  assert.deepEqual((steps[0]?.write as Record<string, unknown>)?.strategy, 'asset_create');
  assert.deepEqual(taskPlan.execution_policy, {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: true,
  });
});

test('preview_task preserves AssetFactory structure fields in TaskPlan ops', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        status: 'dry_run',
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
          steps: [],
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_preview_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeStructureAssetFactoryTaskSpec() });

  assert.equal(result.isError, false);
  const taskPlan = calls[0]?.payload?.task_plan as Record<string, unknown>;
  const steps = taskPlan.steps as Array<Record<string, unknown>>;
  const ops = ((steps[0]?.write as Record<string, unknown>)?.ops ?? []) as Array<Record<string, unknown>>;
  assert.deepEqual(ops[0], {
    op: 'create_asset',
    asset_type: 'structure',
    value_type: undefined,
    collision: 'replace_existing',
    fields: [
      { name: 'Damage', type: 'float' },
      { name: 'Ammo', type: 'int' },
    ],
  });
});

test('preview_task normalizes AssetFactory DataTable alias and preserves row_struct', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        status: 'dry_run',
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
          steps: [],
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_preview_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeDataTableAssetFactoryTaskSpec() });

  assert.equal(result.isError, false);
  const taskPlan = calls[0]?.payload?.task_plan as Record<string, unknown>;
  const steps = taskPlan.steps as Array<Record<string, unknown>>;
  const ops = ((steps[0]?.write as Record<string, unknown>)?.ops ?? []) as Array<Record<string, unknown>>;
  assert.deepEqual(ops[0], {
    op: 'create_asset',
    asset_type: 'data_table',
    value_type: undefined,
    collision: 'replace_existing',
    row_struct: '/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow',
  });
});

test('read_reference_context forwards compact read request to the Bridge', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload): Promise<BridgeResponse> => {
    calls.push({ command, payload });
    return {
      request_id: 'reference_context',
      success: true,
      result: {
        ok: true,
        schema: 'BlueprintHelper.McpToolResult.v1',
        operation: 'read_reference_context',
        trace_id: 'trace_reference_context',
        status: 'completed',
        modified: false,
        target: {
          target_type: 'asset',
          asset_path: '/Game/BP/BP_Door',
        },
        data: {
          schema: 'BlueprintHelper.ReferenceContextPack.v1',
          context_id: 'refctx_001',
          analysis: {
            scope: 'safety_context',
            partial: true,
            truncated: false,
            max_results: 50,
            unsupported_checks: ['blueprint_calls'],
          },
          summary: {
            dependency_count: 0,
            referencer_count: 1,
            external_dependent_count: 0,
            blocking_dependent_count: 0,
            warning_count: 1,
          },
          dependencies: [],
          referencers: [
            {
              asset_path: '/Game/BP/BP_DoorUser',
              asset_type: 'Blueprint',
              reference_kind: 'package',
              evidence_path: '/Game/BP/BP_DoorUser',
              confidence: 'high',
            },
          ],
          external_dependents: [],
          agent_hints: {
            can_edit_safely: false,
            requires_preview: true,
            recommended_task_strategy: 'preview_before_write',
            blockers: ['external_referencers_exist'],
          },
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_read_reference_context');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    asset_path: '/Game/BP/BP_Door',
    target_type: 'asset',
    scope: 'safety_context',
    max_results: 50,
    include_samples: true,
  });

  assert.equal(result.isError, false);
  assert.equal(calls.length, 1);
  assert.equal(calls[0]?.command, 'read_reference_context');
  assert.deepEqual(calls[0]?.payload, {
    asset_path: '/Game/BP/BP_Door',
    target_type: 'asset',
    scope: 'safety_context',
    max_results: 50,
    include_samples: true,
  });
  assert.equal(result.structuredContent?.operation, 'read_reference_context');
  assert.equal(result.structuredContent?.modified, false);
  assert.equal((result.structuredContent?.data as Record<string, unknown>)?.schema, 'BlueprintHelper.ReferenceContextPack.v1');
});

test('read_reference_context maps failed Bridge response as MCP error', async () => {
  const tools = registerWithBridge(async (): Promise<BridgeResponse> => ({
    request_id: 'reference_context_failed',
    success: false,
    error_code: 'asset_not_found',
    message: 'Target asset was not found.',
  }));

  const tool = tools.get('blueprinthelper_read_reference_context');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    asset_path: '/Game/Missing/BP_Missing',
  });

  assert.equal(result.isError, true);
  assert.equal(result.structuredContent?.ok, false);
  assert.equal(result.structuredContent?.operation, 'read_reference_context');
  assert.equal((result.structuredContent?.error as Record<string, unknown>)?.code, 'asset_not_found');
});

test('preview_task compiles append GraphWrite TaskSpec and previews a UE TaskPlan', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        status: 'dry_run',
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
          steps: [],
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_preview_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeTaskSpec() });

  assert.equal(result.isError, false);
  assert.equal(calls.length, 1);
  assert.equal(calls[0]?.command, 'preview_task_plan');
  const taskPlan = calls[0]?.payload?.task_plan as Record<string, unknown>;
  assert.equal(taskPlan?.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.deepEqual(taskPlan?.execution_policy, {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
  });
  assert.equal(Object.hasOwn(calls[0]?.payload ?? {}, 'dry_run'), false);
  assert.equal(result.structuredContent?.operation, 'preview_task');
  assert.equal((result.structuredContent?.data as Record<string, unknown>)?.schema, 'BlueprintHelper.TaskPreview.v1');
});

test('preview_task preserves Bridge ToolResultBase errors for blocked GraphWrite previews', async () => {
  const tools = registerWithBridge(async (): Promise<BridgeResponse> => ({
    request_id: 'preview_failed',
    success: false,
    error_code: 'execution_failed',
    message: '',
    result: {
      ok: false,
      schema: 'BlueprintHelper.McpToolResult.v1',
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_failed',
      status: 'failed',
      modified: false,
      target: {
        target_type: 'graph',
        asset_path: '/Game/BP/BP_Door',
        graph: 'BH_Smoke_20260505_001',
      },
      error: {
        code: 'target_graph_type_invalid',
        stage: 'preflight',
        message: 'Graph BH_Smoke_20260505_001 is a Function graph and cannot receive custom_event nodes.',
        retryable: false,
        rollback_result: 'not_needed',
        field: 'task_plan.steps[1].target.graph',
      },
      data: {
        schema: 'BlueprintHelper.TaskRuntimeResult.v1',
        dry_run: {
          can_execute: false,
          errors: [
            {
              code: 'target_graph_type_invalid',
              message: 'Function graph cannot receive custom_event nodes.',
              target: 'task_plan.steps[1].target.graph',
            },
          ],
          warnings: [],
          conflicts: [],
        },
      },
    },
  }));

  const tool = tools.get('blueprinthelper_preview_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeTaskSpec() });

  assert.equal(result.isError, true);
  assert.equal(result.structuredContent?.operation, 'preview_task');
  const error = result.structuredContent?.error as Record<string, unknown>;
  assert.equal(error.code, 'target_graph_type_invalid');
  assert.equal(error.stage, 'preflight');
  assert.equal(error.message, 'Graph BH_Smoke_20260505_001 is a Function graph and cannot receive custom_event nodes.');
  assert.equal(error.field, 'task_plan.steps[1].target.graph');

  const data = result.structuredContent?.data as Record<string, unknown>;
  assert.equal(data.schema, 'BlueprintHelper.TaskPreview.v1');
  assert.equal(data.passed, false);
  assert.equal(data.blocked, true);
  const issues = data.issues as Array<Record<string, unknown>>;
  assert.equal(issues[0]?.code, 'target_graph_type_invalid');
});

test('preview_task replaces empty Bridge ToolResultBase error messages with a useful fallback', async () => {
  const tools = registerWithBridge(async (): Promise<BridgeResponse> => ({
    request_id: 'preview_failed_empty_message',
    success: false,
    error_code: 'execution_failed',
    message: '',
    result: {
      ok: false,
      schema: 'BlueprintHelper.McpToolResult.v1',
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_failed_empty_message',
      status: 'failed',
      modified: false,
      error: {
        code: 'composite_preview_failed',
        stage: 'preflight',
        message: '',
        retryable: false,
        rollback_result: 'not_needed',
        field: 'task_plan.steps[0]',
      },
    },
  }));

  const tool = tools.get('blueprinthelper_preview_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeTaskSpec() });

  assert.equal(result.isError, true);
  const error = result.structuredContent?.error as Record<string, unknown>;
  assert.equal(error.code, 'composite_preview_failed');
  assert.equal(error.message, 'Bridge dry-run failed.');
  assert.equal(error.field, 'task_plan.steps[0]');
});

test('preview_task surfaces Bridge failed status as a blocked issue when dry-run issues are absent', async () => {
  const tools = registerWithBridge(async (): Promise<BridgeResponse> => ({
    request_id: 'preview_failed_status',
    success: true,
    result: {
      ok: false,
      schema: 'BlueprintHelper.McpToolResult.v1',
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_failed_status',
      status: 'failed',
      modified: false,
      error: {
        code: 'unsupported_taskplan_capability',
        stage: 'parse_input',
        message: 'Unsupported TaskPlan capability.',
        retryable: false,
        rollback_result: 'not_needed',
        field: 'task_plan.steps[0].capability',
      },
    },
  }));

  const tool = tools.get('blueprinthelper_preview_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeTaskSpec() });

  assert.equal(result.isError, false);
  const data = result.structuredContent?.data as Record<string, unknown>;
  assert.equal(data.schema, 'BlueprintHelper.TaskPreview.v1');
  assert.equal(data.passed, false);
  assert.equal(data.blocked, true);
  const issues = data.issues as Array<Record<string, unknown>>;
  assert.equal(issues.length, 1);
  assert.equal(issues[0]?.code, 'unsupported_taskplan_capability');
  assert.equal(issues[0]?.path, 'task_plan.steps[0].capability');
  assert.equal(issues[0]?.message, 'Unsupported TaskPlan capability.');
});

test('preview_task surfaces composite dry-run can_execute false as a blocked issue when issues are absent', async () => {
  const tools = registerWithBridge(async (): Promise<BridgeResponse> => ({
    request_id: 'preview_composite_can_execute_false',
    success: true,
    result: {
      ok: true,
      schema: 'BlueprintHelper.McpToolResult.v1',
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_composite_can_execute_false',
      status: 'dry_run',
      modified: false,
      data: {
        dry_run: {
          can_execute: false,
        },
      },
    },
  }));

  const tool = tools.get('blueprinthelper_preview_task');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    task_spec: {
      ...makeTaskSpec(),
      task_type: 'create_blueprint_feature',
    },
  });

  assert.equal(result.isError, false);
  const data = result.structuredContent?.data as Record<string, unknown>;
  assert.equal(data.schema, 'BlueprintHelper.TaskPreview.v1');
  assert.equal(data.passed, false);
  assert.equal(data.blocked, true);
  const issues = data.issues as Array<Record<string, unknown>>;
  assert.equal(issues.length, 1);
  assert.equal(issues[0]?.code, 'task_preview_blocked');
  assert.equal(issues[0]?.path, 'bridge.preview_task_plan');
  assert.equal(issues[0]?.message, 'Task preview was blocked.');
});

test('preview_task compiles Blueprint Variables TaskSpec and previews a UE TaskPlan', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        status: 'dry_run',
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
          steps: [
            {
              step_id: 'step_001',
              capability: 'blueprint_variable',
              operation: 'blueprint_variable',
              adapter_operation: 'add_blueprint_member_variables',
              status: 'dry_run',
            },
          ],
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_preview_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeVariableTaskSpec() });

  assert.equal(result.isError, false);
  assert.equal(calls.length, 1);
  assert.equal(calls[0]?.command, 'preview_task_plan');
  const taskPlan = calls[0]?.payload?.task_plan as Record<string, unknown>;
  const step = (taskPlan.steps as Array<Record<string, unknown>>)[0];
  assert.equal(step?.capability, 'blueprint_variable');
  assert.equal(Object.hasOwn(step ?? {}, 'operation'), false);
  assert.deepEqual(step?.target, { asset_path: '/Game/BP/BP_Door' });
});

test('execute_task previews before writing and stores a task result', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload): Promise<BridgeResponse> => {
    calls.push({ command, payload });
    if (calls.length === 1) {
      return {
        request_id: 'preview',
        success: true,
        result: {
          status: 'dry_run',
          data: {
            schema: 'BlueprintHelper.TaskRuntimeResult.v1',
            dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
            steps: [],
          },
        },
      };
    }
    return {
      request_id: 'execute',
      success: true,
      result: {
        status: 'applied',
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          task_run_id: 'task_ue_001',
          steps: [
            {
              step_id: 'step_001',
              operation: 'append_blueprint_graph',
              status: 'applied',
              result: {
                data: {
                  append_result: {
                    graph: { graph_id: 'EG_DoorFeature', graph_name: 'EG_DoorFeature' },
                    block_refs: ['ToggleDoor'],
                  },
                  write_ref: { transaction_id: 'tx_001', journal_recorded: true },
                },
              },
            },
          ],
        },
      },
    };
  });

  const executeTool = tools.get('blueprinthelper_execute_task');
  assert.ok(executeTool);

  const executeResult = await invokeTool(executeTool, { task_spec: makeTaskSpec() });

  assert.equal(executeResult.isError, false);
  assert.deepEqual(calls.map((call) => [call.command, (call.payload?.task_plan as Record<string, unknown> | undefined)?.schema]), [
    ['preview_task_plan', 'BlueprintHelper.TaskPlan.v1'],
    ['execute_task_plan', 'BlueprintHelper.TaskPlan.v1'],
  ]);

  const executionData = executeResult.structuredContent?.data as Record<string, unknown>;
  assert.equal(executionData.schema, 'BlueprintHelper.TaskExecution.v1');
  assert.equal(executionData.task_run_id, 'task_ue_001');
  assert.equal((executionData.task as Record<string, unknown>).task_run_id, 'task_ue_001');

  const getResultTool = tools.get('blueprinthelper_get_task_result');
  assert.ok(getResultTool);

  const taskResult = await invokeTool(getResultTool, {
    task_run_id: executionData.task_run_id,
  });

  assert.equal(taskResult.isError, false);
  const journal = taskResult.structuredContent?.data as Record<string, unknown>;
  assert.equal(journal.schema, 'BlueprintHelper.TaskRunJournal.v1');
  assert.equal(journal.task_run_id, 'task_ue_001');
});

test('shared TaskSpec runner preserves preview-before-execute Bridge command order', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const runner = createTaskSpecRunner({
    bridge: {
      sendCommand: async (command, payload): Promise<BridgeResponse> => {
        calls.push({ command, payload });
        if (command === 'preview_task_plan') {
          return {
            request_id: 'preview',
            success: true,
            result: {
              status: 'dry_run',
              data: {
                schema: 'BlueprintHelper.TaskRuntimeResult.v1',
                dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
                steps: [],
              },
            },
          };
        }
        return {
          request_id: 'execute',
          success: true,
          result: {
            status: 'applied',
            data: {
              schema: 'BlueprintHelper.TaskRuntimeResult.v1',
              task_run_id: 'task_shared_runner_001',
              steps: [],
            },
          },
        };
      },
    },
    taskCompiler: taskCompilerForTaskToolTests,
  });

  const result = await runner.executeTask(TaskSpecSchema.parse(makeTaskSpec()));

  assert.equal(result.ok, true);
  assert.deepEqual(calls.map((call) => call.command), ['preview_task_plan', 'execute_task_plan']);
  assert.equal(((result.data?.task as Record<string, unknown>) ?? {}).task_run_id, 'task_shared_runner_001');
});

test('execute_task falls back to an MCP task_run_id when UE omits one', async () => {
  const tools = registerWithBridge(async (command): Promise<BridgeResponse> => {
    if (command === 'preview_task_plan') {
      return {
        request_id: 'preview',
        success: true,
        result: {
          status: 'dry_run',
          data: {
            schema: 'BlueprintHelper.TaskRuntimeResult.v1',
            dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
            steps: [],
          },
        },
      };
    }
    return {
      request_id: 'execute',
      success: true,
      result: {
        status: 'applied',
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          steps: [],
        },
      },
    };
  });

  const executeTool = tools.get('blueprinthelper_execute_task');
  assert.ok(executeTool);

  const executeResult = await invokeTool(executeTool, { task_spec: makeTaskSpec() });

  assert.equal(executeResult.isError, false);
  const executionData = executeResult.structuredContent?.data as Record<string, unknown>;
  assert.equal(executionData.schema, 'BlueprintHelper.TaskExecution.v1');
  assert.equal(typeof executionData.task_run_id, 'string');
  assert.match(executionData.task_run_id as string, /^task_\d+_\d{4}$/);
});

test('execute_task preserves Bridge ToolResultBase errors for failed writes', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload): Promise<BridgeResponse> => {
    calls.push({ command, payload });
    if (command === 'preview_task_plan') {
      return {
        request_id: 'preview',
        success: true,
        result: {
          status: 'dry_run',
          data: {
            schema: 'BlueprintHelper.TaskRuntimeResult.v1',
            dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
            steps: [],
          },
        },
      };
    }

    return {
      request_id: 'execute_failed',
      success: false,
      error_code: 'execution_failed',
      message: '',
      result: {
        ok: false,
        schema: 'BlueprintHelper.McpToolResult.v1',
        operation: 'execute_task_plan',
        trace_id: 'trace_execute_failed',
        status: 'failed',
        modified: false,
        error: {
          code: 'node_create_failed',
          stage: 'execute',
          message: 'Agent import failed while creating custom_event body.',
          retryable: false,
          rollback_result: 'rolled_back',
          field: 'task_plan.steps[1].write.ops[0]',
        },
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          task_run_id: 'task_ue_failed',
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_execute_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeTaskSpec() });

  assert.equal(result.isError, true);
  assert.deepEqual(calls.map((call) => call.command), ['preview_task_plan', 'execute_task_plan']);
  assert.equal(result.structuredContent?.operation, 'execute_task');
  const error = result.structuredContent?.error as Record<string, unknown>;
  assert.equal(error.code, 'node_create_failed');
  assert.equal(error.stage, 'execute');
  assert.equal(error.message, 'Agent import failed while creating custom_event body.');
  assert.equal(error.rollback_result, 'rolled_back');
  assert.equal(error.field, 'task_plan.steps[1].write.ops[0]');
});

test('execute_task replaces empty Bridge ToolResultBase error messages with a useful fallback', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload): Promise<BridgeResponse> => {
    calls.push({ command, payload });
    if (command === 'preview_task_plan') {
      return {
        request_id: 'preview',
        success: true,
        result: {
          status: 'dry_run',
          data: {
            schema: 'BlueprintHelper.TaskRuntimeResult.v1',
            dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
            steps: [],
          },
        },
      };
    }

    return {
      request_id: 'execute_failed_empty_message',
      success: false,
      error_code: 'execution_failed',
      message: '',
      result: {
        ok: false,
        schema: 'BlueprintHelper.McpToolResult.v1',
        operation: 'execute_task_plan',
        trace_id: 'trace_execute_failed_empty_message',
        status: 'failed',
        modified: false,
        error: {
          code: 'link_create_failed',
          stage: 'execute',
          message: '',
          retryable: false,
          rollback_result: 'rolled_back',
          field: 'task_plan.steps[0].write.ops[0]',
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_execute_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeTaskSpec() });

  assert.equal(result.isError, true);
  assert.match(result.content[0]?.text ?? '', /Bridge write failed\./);
  assert.deepEqual(calls.map((call) => call.command), ['preview_task_plan', 'execute_task_plan']);
  const error = result.structuredContent?.error as Record<string, unknown>;
  assert.equal(error.code, 'link_create_failed');
  assert.equal(error.message, 'Bridge write failed.');
  assert.equal(error.rollback_result, 'rolled_back');
  assert.equal(error.field, 'task_plan.steps[0].write.ops[0]');
});

test('get_task_result falls back to UE TaskRunJournal when not stored in process', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload): Promise<BridgeResponse> => {
    calls.push({ command, payload });
    return {
      request_id: 'journal',
      success: true,
      result: {
        status: 'completed',
        data: {
          schema: 'BlueprintHelper.TaskRunJournal.v1',
          task_run_id: 'task_ue_external',
          preview_id: 'preview_ue_external',
          task_type: 'edit_blueprint_graph',
          status: 'completed',
          target_assets: ['/Game/BP/BP_Door'],
          steps: [
            {
              step_id: 'step_001',
              operation: 'append_blueprint_graph',
              adapter_operation: 'append_blueprint_graph',
              status: 'completed',
              result: {
                ok: true,
                schema: 'BlueprintHelper.McpToolResult.v1',
                operation: 'append_blueprint_graph',
                status: 'completed',
                modified: true,
                target: {
                  target_type: 'graph',
                  asset_path: '/Game/BP/BP_Door',
                  graph: 'EG_DoorFeature',
                },
                data: {
                  schema: 'AppendBlueprintGraphResult.v1',
                  append_result: {
                    graph: {
                      graph_name: 'EG_DoorFeature',
                    },
                  },
                },
              },
            },
          ],
        },
      },
    };
  });

  const getResultTool = tools.get('blueprinthelper_get_task_result');
  assert.ok(getResultTool);

  const taskResult = await invokeTool(getResultTool, {
    task_run_id: 'task_ue_external',
  });

  assert.equal(taskResult.isError, false);
  assert.equal(calls.length, 1);
  assert.equal(calls[0]?.command, 'get_task_run_journal');
  assert.deepEqual(calls[0]?.payload, { task_run_id: 'task_ue_external' });
  const journal = taskResult.structuredContent?.data as Record<string, unknown>;
  assert.equal(journal.schema, 'BlueprintHelper.TaskRunJournal.v1');
  assert.equal(journal.task_run_id, 'task_ue_external');
  assert.equal(journal.generated_intent, '使用 GraphWrite 写入蓝图逻辑 - BP_Door.EG_DoorFeature');
});

test('execute_task does not write when preview dry-run is blocked', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'preview',
      success: true,
      result: {
        status: 'dry_run',
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          dry_run: {
            can_execute: false,
            warnings: [],
            conflicts: [],
            errors: [{ code: 'target_graph_not_empty', message: 'Graph is not empty.' }],
          },
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_execute_task');
  assert.ok(tool);

  const result = await invokeTool(tool, { task_spec: makeTaskSpec() });

  assert.equal(result.isError, true);
  assert.equal(result.structuredContent?.operation, 'execute_task');
  assert.equal((result.structuredContent?.error as Record<string, unknown>)?.code, 'task_preview_blocked');
  assert.equal(calls.length, 1);
  assert.equal(calls[0]?.command, 'preview_task_plan');
});
