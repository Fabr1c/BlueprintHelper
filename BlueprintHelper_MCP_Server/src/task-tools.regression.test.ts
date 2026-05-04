import assert from 'node:assert/strict';
import test from 'node:test';
import type { BridgeResponse } from './bridge-client.js';
import { invokeTool, registerWithBridge } from './test-harness.js';

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

test('task-level tools are registered without removing legacy tools', () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));

  for (const name of [
    'blueprinthelper_read_task_context',
    'blueprinthelper_preview_task',
    'blueprinthelper_execute_task',
    'blueprinthelper_get_task_result',
    'blueprint_get_logic',
  ]) {
    assert.equal(tools.has(name), true, name);
  }
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
          steps: [],
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
