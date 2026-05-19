import assert from 'node:assert/strict';
import test from 'node:test';
import { createTaskSpecRunner } from '../../task/service/task-spec-runner.js';
import { startTaskTiming } from '../../task/service/task-timing.js';
import {
  ExecuteTaskInputSchema,
  TaskPreviewTokenSchema,
  type TaskPlan,
  type TaskSpec,
} from '../../task/schema/task-schemas.js';
import type { BridgeResponse } from '../../bridge/bridge-client.js';

test('executeTask propagates modified state from Bridge execution result', async () => {
  const taskPlan = {
    schema: 'BlueprintHelper.TaskPlan.v1',
    task_name: 'ModifiedPropagation',
    task_type: 'edit_blueprint_graph',
    target_assets: ['/Game/BP/BP_Door'],
    execution_policy: {
      dry_run_mode: 'full',
      should_compile: true,
      should_save: false,
      review_baseline_dirty_asset_policy: 'block',
    },
    steps: [
      {
        step_id: 'step_001',
        capability: 'graph_write',
        target: {
          asset_path: '/Game/BP/BP_Door',
          graph: 'BH_Door',
        },
        write: {
          strategy: 'owned_graph_edit',
          ops: [
            {
              op: 'insert_flow',
            },
          ],
        },
        constraints: {
          allow_modify_user_nodes: false,
          ownership_scope: 'blueprinthelper_owned',
        },
      },
    ],
  } satisfies TaskPlan;

  const bridge = {
    async sendCommand(command: string): Promise<BridgeResponse> {
      if (command === 'preview_task_plan') {
        return {
          success: true,
          request_id: 'preview_request',
          result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: 'preview_task_plan',
            status: 'dry_run',
            modified: false,
            data: {
              dry_run: {
                result: 'passed',
                can_execute: true,
              },
            },
          },
        };
      }

      if (command === 'execute_task_plan') {
        return {
          success: true,
          request_id: 'execute_request',
          result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: 'execute_task_plan',
            status: 'applied',
            modified: true,
            data: {
              schema: 'BlueprintHelper.TaskRuntimeResult.v1',
              task_run_id: 'task_modified_propagation',
              steps: [
                {
                  step_id: 'step_001',
                  capability: 'graph_write',
                  operation: 'graph_write',
                  adapter_operation: 'merge_blueprint_graph',
                  result: {
                    ok: true,
                    operation: 'merge_blueprint_graph',
                    status: 'applied',
                    modified: true,
                  },
                },
              ],
            },
          },
        };
      }

      throw new Error(`Unexpected command: ${command}`);
    },
  };

  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => ({
      schema: 'BlueprintHelper.TaskCompilerResult.v1',
      task_plan: taskPlan,
      bridge_payload: {},
      task_plan_summary: {},
    }),
  });

  const result = await runner.executeTask({} as TaskSpec);

  assert.equal(result.ok, true);
  assert.equal(result.modified, true);
  assert.equal((result.data as Record<string, any>).task.modified_assets, 1);
});

test('previewTask omits timing unless a develop trace is supplied', async () => {
  const taskPlan = makeSingleStepTaskPlan('TimingDefault');
  let previewPayload: Record<string, unknown> | undefined;
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> {
        assert.equal(command, 'preview_task_plan');
        previewPayload = payload;
        return makePreviewBridgeResponse();
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const preview = await runner.previewTask({} as TaskSpec);

  assert.equal(preview.passed, true);
  assert.equal(previewPayload?.include_timing, undefined);
  assert.equal((preview.toolResult.data as Record<string, unknown>).timing, undefined);
  assert.equal((preview.toolResult.data as Record<string, unknown>).ue_preview_result, undefined);
  assert.equal((preview.toolResult.data as Record<string, unknown>).dry_run, undefined);
});

test('previewTask enables timing when a develop trace is supplied', async () => {
  const taskPlan = makeSingleStepTaskPlan('TimingDevelop');
  let previewPayload: Record<string, unknown> | undefined;
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> {
        assert.equal(command, 'preview_task_plan');
        previewPayload = payload;
        return makePreviewBridgeResponse({
          data: {
            dry_run: {
              result: 'passed',
              can_execute: true,
            },
            timing: {
              schema: 'BlueprintHelper.TimingTrace.v1',
              source: 'ue_task_runtime',
              operation: 'preview_task_plan',
              timing_id: 'timing_ue_preview',
              total_ms: 1,
              stages: [],
            },
            call_function_resolution_cache: {
              hits: 1,
              misses: 1,
              entries: 1,
            },
            runtime_facts: {
              resolved_call_functions: [{
                stable_id: '/Script/Engine.KismetSystemLibrary:PrintString',
                native_name: 'PrintString',
              }],
            },
          },
        });
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const timing = startTaskTiming(true, 'preview_task');
  const preview = await runner.previewTask({} as TaskSpec, timing);
  const resultData = preview.toolResult.data as Record<string, any>;
  const resultTiming = resultData.timing as Record<string, unknown>;
  const nested = resultTiming.nested as Array<Record<string, unknown>>;

  assert.equal(preview.passed, true);
  assert.equal(previewPayload?.include_timing, true);
  assert.equal(resultTiming.source, 'agentface_task_runner');
  assert.equal(nested[0].name, 'ue.preview_task_plan');
  assert.equal(resultData.dry_run.can_execute, true);
  assert.deepEqual(resultData.call_function_resolution_cache, {
    hits: 1,
    misses: 1,
    entries: 1,
  });
  assert.equal(
    resultData.runtime_facts.resolved_call_functions[0].stable_id,
    '/Script/Engine.KismetSystemLibrary:PrintString',
  );
  assert.equal(resultData.ue_preview_result.operation, 'preview_task_plan');
  assert.equal(resultData.ue_preview_result.data.call_function_resolution_cache.hits, 1);
});

test('previewTask returns and exposes a 32 hex preview token', async () => {
  const taskPlan = makeSingleStepTaskPlan('PreviewToken');
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string): Promise<BridgeResponse> {
        assert.equal(command, 'preview_task_plan');
        return makePreviewBridgeResponse();
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const preview = await runner.previewTask({} as TaskSpec);
  const data = preview.toolResult.data as Record<string, unknown>;

  assert.equal(typeof preview.previewToken, 'string');
  assert.match(preview.previewToken ?? '', /^[0-9a-f]{32}$/);
  assert.doesNotThrow(() => TaskPreviewTokenSchema.parse(preview.previewToken));
  assert.deepEqual(data.preview_token, preview.previewToken);
});

test('ExecuteTaskInputSchema accepts preview_token on wrapped task_spec input', () => {
  const taskSpec = makeGraphWriteTaskSpec();
  const parsed = ExecuteTaskInputSchema.parse({
    task_spec: taskSpec,
    preview_token: '0123456789abcdef0123456789abcdef',
  });

  assert.equal(parsed.preview_token, '0123456789abcdef0123456789abcdef');
});

test('executeTask with matching preview token reuses cached TaskPlan without a second preview', async () => {
  const taskPlan = makeSingleStepTaskPlan('MatchingPreviewToken');
  const calls: string[] = [];
  let executePayload: Record<string, unknown> | undefined;
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> {
        calls.push(command);
        if (command === 'preview_task_plan') {
          return makePreviewBridgeResponse();
        }
        if (command === 'execute_task_plan') {
          executePayload = payload;
          return makeExecuteBridgeResponse('task_matching_preview_token');
        }
        throw new Error(`Unexpected command: ${command}`);
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const taskSpec = {} as TaskSpec;
  const preview = await runner.previewTask(taskSpec);
  const result = await runner.executeTask(taskSpec, undefined, { previewToken: preview.previewToken });

  assert.equal(result.ok, true);
  assert.deepEqual(calls, ['preview_task_plan', 'execute_task_plan']);
  assert.equal(executePayload?.preview_token, preview.previewToken);
  assert.equal(Object.hasOwn(executePayload ?? {}, 'task_plan'), false);
});

test('executeTask with malformed preview token fails before execute_task_plan', async () => {
  const taskPlan = makeSingleStepTaskPlan('MalformedPreviewToken');
  const calls: string[] = [];
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string): Promise<BridgeResponse> {
        calls.push(command);
        if (command === 'preview_task_plan') {
          return makePreviewBridgeResponse();
        }
        throw new Error(`Unexpected command: ${command}`);
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const taskSpec = {} as TaskSpec;
  const preview = await runner.previewTask(taskSpec);
  const result = await runner.executeTask(taskSpec, undefined, {
    previewToken: `${preview.previewToken}00`,
  });

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'preview_token_mismatch');
  assert.deepEqual(calls, ['preview_task_plan']);
});

test('executeTask with no token and dry_run_mode none fails before any Bridge call', async () => {
  const taskPlan = makeSingleStepTaskPlan('DryRunNoneRequiresToken', 'none');
  const calls: string[] = [];
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string): Promise<BridgeResponse> {
        calls.push(command);
        if (command === 'preview_task_plan') {
          return makePreviewBridgeResponse();
        }
        throw new Error(`Unexpected command: ${command}`);
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const result = await runner.executeTask({} as TaskSpec);

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'dry_run_mode_none_requires_preview_token');
  assert.deepEqual(calls, []);
});

test('executeTask develop timing records preview token validation without TaskSpec compile', async () => {
  const taskPlan = makeSingleStepTaskPlan('PreviewTokenTiming');
  const calls: string[] = [];
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string): Promise<BridgeResponse> {
        calls.push(command);
        if (command === 'preview_task_plan') {
          return makePreviewBridgeResponse();
        }
        if (command === 'execute_task_plan') {
          return makeExecuteBridgeResponse('task_preview_token_timing');
        }
        throw new Error(`Unexpected command: ${command}`);
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const taskSpec = {} as TaskSpec;
  const preview = await runner.previewTask(taskSpec);
  const timing = startTaskTiming(true, 'execute_task');
  const result = await runner.executeTask(taskSpec, timing, { previewToken: preview.previewToken });
  const resultTiming = (result.data as Record<string, unknown>).timing as Record<string, unknown>;
  const stageNames = (resultTiming.stages as Array<Record<string, unknown>>).map((stage) => stage.name);

  assert.equal(result.ok, true);
  assert.deepEqual(calls, ['preview_task_plan', 'execute_task_plan']);
  assert.equal(stageNames.includes('preview_token.validate'), true);
  assert.equal(stageNames.includes('taskspec_compile'), false);
});

test('previewTask develop timing records preview token request preparation', async () => {
  const taskPlan = makeSingleStepTaskPlan('PreviewTokenStoreTiming');
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string): Promise<BridgeResponse> {
        if (command === 'preview_task_plan') {
          return makePreviewBridgeResponse();
        }
        throw new Error(`Unexpected command: ${command}`);
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const timing = startTaskTiming(true, 'preview_task');
  const preview = await runner.previewTask({} as TaskSpec, timing);
  const resultTiming = (preview.toolResult.data as Record<string, unknown>).timing as Record<string, unknown>;
  const stageNames = (resultTiming.stages as Array<Record<string, unknown>>).map((stage) => stage.name);

  assert.equal(preview.toolResult.ok, true);
  assert.equal(stageNames.includes('preview_token.allocate_preview_id'), true);
  assert.equal(stageNames.includes('preview_token.prepare_request'), true);
});

function makeSingleStepTaskPlan(taskName: string, dryRunMode: 'none' | 'quick' | 'full' = 'full'): TaskPlan {
  return {
    schema: 'BlueprintHelper.TaskPlan.v1',
    task_name: taskName,
    task_type: 'edit_blueprint_graph',
    target_assets: ['/Game/BP/BP_Door'],
    execution_policy: {
      dry_run_mode: dryRunMode,
      should_compile: true,
      should_save: false,
      review_baseline_dirty_asset_policy: 'block',
    },
    steps: [
      {
        step_id: 'step_001',
        capability: 'graph_write',
        target: {
          asset_path: '/Game/BP/BP_Door',
          graph: 'BH_Door',
        },
        write: {
          strategy: 'owned_graph_edit',
          ops: [
            {
              op: 'insert_flow',
            },
          ],
        },
        constraints: {
          allow_modify_user_nodes: false,
          ownership_scope: 'blueprinthelper_owned',
        },
      },
    ],
  } satisfies TaskPlan;
}

function makeGraphWriteTaskSpec(): TaskSpec {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'BH_Door',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'PreviewTokenSchema',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [],
        },
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
    },
    validation: {
      should_compile: true,
      should_save: false,
    },
  } as TaskSpec;
}

function makeCompilerResult(taskPlan: TaskPlan) {
  return {
    schema: 'BlueprintHelper.TaskCompilerResult.v1',
    task_plan: taskPlan,
    bridge_payload: {},
    task_plan_summary: {},
  } as const;
}

function makePreviewBridgeResponse(result: Record<string, unknown> = {}): BridgeResponse {
  const resultData = result.data && typeof result.data === 'object' && !Array.isArray(result.data)
    ? result.data as Record<string, unknown>
    : {};
  const resultWithoutData = { ...result };
  delete resultWithoutData.data;
  return {
    success: true,
    request_id: 'preview_request',
    result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'preview_task_plan',
      status: 'dry_run',
      modified: false,
      data: {
        dry_run: {
          result: 'passed',
          can_execute: true,
        },
        preview_token: '0123456789abcdef0123456789abcdef',
        ...resultData,
      },
      ...resultWithoutData,
    },
  };
}

function makeExecuteBridgeResponse(taskRunId: string): BridgeResponse {
  return {
    success: true,
    request_id: 'execute_request',
    result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'execute_task_plan',
      status: 'applied',
      modified: true,
      data: {
        schema: 'BlueprintHelper.TaskRuntimeResult.v1',
        task_run_id: taskRunId,
        steps: [
          {
            step_id: 'step_001',
            result: {
              modified: true,
            },
          },
        ],
      },
    },
  };
}
