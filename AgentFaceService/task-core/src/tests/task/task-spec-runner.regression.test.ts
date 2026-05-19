import assert from 'node:assert/strict';
import test from 'node:test';
import { createTaskSpecRunner } from '../../task/service/task-spec-runner.js';
import { startTaskTiming } from '../../task/service/task-timing.js';
import type { TaskPlan, TaskSpec } from '../../task/schema/task-schemas.js';
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
          },
        });
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const timing = startTaskTiming(true, 'preview_task');
  const preview = await runner.previewTask({} as TaskSpec, timing);
  const resultTiming = (preview.toolResult.data as Record<string, unknown>).timing as Record<string, unknown>;
  const nested = resultTiming.nested as Array<Record<string, unknown>>;

  assert.equal(preview.passed, true);
  assert.equal(previewPayload?.include_timing, true);
  assert.equal(resultTiming.source, 'agentface_task_runner');
  assert.equal(nested[0].name, 'ue.preview_task_plan');
});

function makeSingleStepTaskPlan(taskName: string): TaskPlan {
  return {
    schema: 'BlueprintHelper.TaskPlan.v1',
    task_name: taskName,
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
      ...result,
    },
  };
}
