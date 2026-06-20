import assert from 'node:assert/strict';
import test from 'node:test';
import { createTaskSpecRunner } from '../../task/service/task-spec-runner.js';
import { startTaskTiming } from '../../task/service/task-timing.js';
import {
  ExecuteTaskInputSchema,
  TaskPreviewTokenSchema,
  type TaskPlan,
  type TaskSpec,
  type TaskVerificationContract,
} from '../../task/schema/task-schemas.js';
import { BRIDGE_RESPONSE_SCHEMA } from '../../bridge/bridge-response-schema.js';
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
        return withBridgeSchema({
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
        });
      }

      if (command === 'source_control_status') {
        return makeEditableSourceControlResponse('/Game/BP/BP_Door');
      }

      if (command === 'execute_task_plan') {
        return withBridgeSchema({
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
        });
      }

      throw new Error(`Unexpected command: ${command}`);
    },
  };

  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => ({
      schema: 'BlueprintHelper.TaskCompilerResult.v1',
      taskPlan,
      strategyId: 'canonical_ts',
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

test('previewTask sends and returns execution receipt identity', async () => {
  const taskPlan = makeSingleStepTaskPlan('ReceiptPreview');
  let previewPayload: Record<string, any> | undefined;
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> {
        assert.equal(command, 'preview_task_plan');
        previewPayload = payload as Record<string, any>;
        return makePreviewBridgeResponse();
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const preview = await runner.previewTask({} as TaskSpec);
  const receipt = (preview.toolResult.data as Record<string, any>).receipt;

  assert.equal(previewPayload?.preview_token_request.receipt_id, receipt.receipt_id);
  assert.equal(previewPayload?.preview_token_request.preview_id, preview.previewId);
  assert.equal(receipt.schema, 'BlueprintHelper.ExecutionReceipt.v1');
  assert.equal(receipt.preview_id, preview.previewId);
  assert.equal(receipt.preview_token_hash.length, 64);
  assert.equal(receipt.task_spec_hash.length, 64);
  assert.equal(receipt.task_plan_hash.length, 64);
  assert.equal(receipt.status, 'previewed');
});

test('previewTask carries verification identity through token request and receipt', async () => {
  const taskPlan = makeVerifiedTaskPlan('ReceiptPreviewVerification');
  let previewPayload: Record<string, any> | undefined;
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> {
        assert.equal(command, 'preview_task_plan');
        previewPayload = payload as Record<string, any>;
        return makePreviewBridgeResponse();
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const preview = await runner.previewTask(makeVerifiedGraphWriteTaskSpec());
  const receipt = (preview.toolResult.data as Record<string, any>).receipt;

  assert.equal(typeof receipt.verification_hash, 'string');
  assert.equal(receipt.verification_hash.length, 64);
  assert.equal(receipt.verification_status, 'pending_readback');
  assert.equal(previewPayload?.preview_token_request.verification_hash, receipt.verification_hash);
  assert.equal(previewPayload?.preview_token_request.receipt.verification_hash, receipt.verification_hash);
});

test('executeTask with preview token sends receipt metadata and returns matching receipt', async () => {
  const bridgeCalls: Array<{ command: string; payload?: Record<string, any> }> = [];
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand(command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> {
        bridgeCalls.push({ command, payload: payload as Record<string, any> });
        if (command === 'source_control_status') {
          return makeEditableSourceControlResponse('/Game/BP/BP_Door');
        }
        if (command === 'execute_task_plan') {
          return withBridgeSchema({
            success: true,
            request_id: 'execute_receipt_request',
            result: {
              ok: true,
              schema: 'BlueprintHelper.ToolResult.v1',
              operation: 'execute_task_plan',
              status: 'applied',
              modified: true,
              data: {
                task_run_id: 'task_receipt_001',
                receipt: {
                  schema: 'BlueprintHelper.ExecutionReceipt.v1',
                  receipt_id: 'receipt_from_cli',
                  cli_run_id: 'cli_from_cli',
                  task_run_id: 'task_receipt_001',
                  task_spec_hash: 'a'.repeat(64),
                  status: 'applied',
                  created_at: '2026-06-20T00:00:00.000Z',
                  updated_at: '2026-06-20T00:00:00.000Z',
                },
                target_assets: ['/Game/BP/BP_Door'],
              },
            },
          });
        }
        throw new Error(`Unexpected command: ${command}`);
      },
    },
    taskCompiler: async () => makeCompilerResult(makeSingleStepTaskPlan('ReceiptExecute')),
  });

  const result = await runner.executeTask(makeVerifiedGraphWriteTaskSpec(), undefined, {
    previewToken: '0123456789abcdef0123456789abcdef',
    receiptId: 'receipt_from_cli',
    cliRunId: 'cli_from_cli',
    previewId: 'preview_from_cli',
  });
  const executeCall = bridgeCalls.find((call) => call.command === 'execute_task_plan');
  const resultReceipt = (result.data as Record<string, any>).receipt;

  assert.equal(executeCall?.payload?.receipt.receipt_id, 'receipt_from_cli');
  assert.equal(executeCall?.payload?.receipt.preview_id, 'preview_from_cli');
  assert.equal(executeCall?.payload?.receipt.preview_token_hash.length, 64);
  assert.equal(executeCall?.payload?.receipt.verification_hash.length, 64);
  assert.equal(executeCall?.payload?.receipt.verification_status, 'pending_readback');
  assert.equal(resultReceipt.receipt_id, 'receipt_from_cli');
  assert.equal(resultReceipt.task_run_id, 'task_receipt_001');
  assert.equal(resultReceipt.verification_hash, executeCall?.payload?.receipt.verification_hash);
  assert.equal(resultReceipt.status, 'applied');
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
        if (command === 'source_control_status') {
          return makeEditableSourceControlResponse('/Game/BP/BP_Door');
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

  const taskSpec = makeGraphWriteTaskSpec();
  const preview = await runner.previewTask(taskSpec);
  const result = await runner.executeTask(taskSpec, undefined, { previewToken: preview.previewToken });

  assert.equal(result.ok, true);
  assert.deepEqual(calls, ['preview_task_plan', 'source_control_status', 'execute_task_plan']);
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
        if (command === 'source_control_status') {
          return makeEditableSourceControlResponse('/Game/BP/BP_Door');
        }
        if (command === 'execute_task_plan') {
          return makeExecuteBridgeResponse('task_preview_token_timing');
        }
        throw new Error(`Unexpected command: ${command}`);
      },
    },
    taskCompiler: async () => makeCompilerResult(taskPlan),
  });

  const taskSpec = makeGraphWriteTaskSpec();
  const preview = await runner.previewTask(taskSpec);
  const timing = startTaskTiming(true, 'execute_task');
  const result = await runner.executeTask(taskSpec, timing, { previewToken: preview.previewToken });
  const resultTiming = (result.data as Record<string, unknown>).timing as Record<string, unknown>;
  const stageNames = (resultTiming.stages as Array<Record<string, unknown>>).map((stage) => stage.name);

  assert.equal(result.ok, true);
  assert.deepEqual(calls, ['preview_task_plan', 'source_control_status', 'execute_task_plan']);
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
  const stages = resultTiming.stages as Array<Record<string, unknown>>;
  const stageNames = stages.map((stage) => stage.name);
  const strategyStage = stages.find((stage) => stage.name === 'taskspec_compile.strategy');

  assert.equal(preview.toolResult.ok, true);
  assert.equal(stageNames.includes('preview_token.allocate_preview_id'), true);
  assert.equal(stageNames.includes('preview_token.prepare_request'), true);
  assert.equal(strategyStage?.strategy, 'canonical_ts');
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

function makeVerifiedTaskPlan(taskName: string): TaskPlan {
  return {
    ...makeSingleStepTaskPlan(taskName),
    verification: makeVerificationContract(),
  };
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
  } as TaskSpec;
}

function makeVerifiedGraphWriteTaskSpec(): TaskSpec {
  return {
    ...makeGraphWriteTaskSpec(),
    verification: makeVerificationContract(),
  } as TaskSpec;
}

function makeVerificationContract(): TaskVerificationContract {
  return {
    schema: 'BlueprintHelper.TaskVerification.v1',
    mode: 'required',
    requirements: [{
      id: 'door-flow-created',
      fact: 'blueprint.graph.node.exists',
      target: {
        asset_path: '/Game/BP/BP_Door',
        graph: 'BH_Door',
      },
      expected: true,
    }],
  };
}

function makeCompilerResult(taskPlan: TaskPlan) {
  return {
    schema: 'BlueprintHelper.TaskCompilerResult.v1',
    taskPlan,
    strategyId: 'canonical_ts',
  } as const;
}

function makePreviewBridgeResponse(result: Record<string, unknown> = {}): BridgeResponse {
  const resultData = result.data && typeof result.data === 'object' && !Array.isArray(result.data)
    ? result.data as Record<string, unknown>
    : {};
  const resultWithoutData = { ...result };
  delete resultWithoutData.data;
  return {
    schema: BRIDGE_RESPONSE_SCHEMA,
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
    schema: BRIDGE_RESPONSE_SCHEMA,
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

function makeEditableSourceControlResponse(assetPath: string): BridgeResponse {
  return {
    schema: BRIDGE_RESPONSE_SCHEMA,
    success: true,
    request_id: 'source_control_status_editable',
    result: {
      source_control: {
        status: 'editable',
        files: [{ asset_path: assetPath, status: 'editable', editable: true }],
      },
    },
  };
}

function withBridgeSchema<T extends { request_id: string; success: boolean }>(response: T): T & { schema: string } {
  return {
    schema: BRIDGE_RESPONSE_SCHEMA,
    ...response,
  };
}
