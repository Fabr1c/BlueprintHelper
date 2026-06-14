import { strict as assert } from 'node:assert';
import test from 'node:test';

import type { BridgeResponse } from '../../bridge/bridge-client.js';
import type { MetricsEvent } from '../../metrics/metrics-types.js';
import { TOOL_RESULT_SCHEMA } from '../../result/tool-result.js';
import { TaskSpecCompileError, createCompiledTaskPlan } from '../compiler/task-compiler.js';
import type { TaskPlan, TaskSpec } from '../schema/task-schemas.js';
import {
  graphWriteAppendExpectedTaskPlanFixture,
  graphWriteAppendTaskSpecFixture,
  graphWriteReplaceExpectedTaskPlanFixture,
  graphWriteReplaceTaskSpecFixture,
} from '../fixtures/task-protocol.fixtures.js';
import { TaskTimingTrace } from './task-timing.js';
import { createTaskSpecRunner, type TaskRunnerBridge } from './task-spec-runner.js';

const previewBridgeResponse: BridgeResponse = {
  success: true,
  request_id: 'preview_cache_test_request',
  result: {
    ok: true,
    schema: TOOL_RESULT_SCHEMA,
    operation: 'preview_task_plan',
    trace_id: 'trace_preview_cache_test',
    status: 'dry_run',
    modified: false,
    data: {
      preview_token: '0123456789abcdef0123456789abcdef',
      dry_run: {
        can_execute: true,
        result: 'passed',
        errors: [],
        conflicts: [],
      },
      cache_diagnostics: {
        partial_preview_hits: 1,
      },
      graph_write_execution_stats: {
        steps: [
          {
            step_id: 'step_001',
            spawned_node_count: 3,
          },
        ],
      },
    },
  },
};

function createPreviewRunner() {
  const bridge: TaskRunnerBridge = {
    async sendCommand() {
      return previewBridgeResponse;
    },
  };

  return createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });
}

function createRunnerForPreviewResponse(response: BridgeResponse) {
  const bridge: TaskRunnerBridge = {
    async sendCommand() {
      return response;
    },
  };

  return createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });
}

function createRuntimeOnlyRoutePreviewRunner(response: BridgeResponse = previewBridgeResponse) {
  const taskPlan: TaskPlan = {
    ...graphWriteAppendExpectedTaskPlanFixture,
    steps: graphWriteAppendExpectedTaskPlanFixture.steps.map((step) => (
      step.step_id === 'step_002'
        ? { ...step, route_id: 'graph.merge_external_flow.insert_between' }
        : step
    )),
  };
  const bridge: TaskRunnerBridge = {
    async sendCommand() {
      return response;
    },
  };

  return createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan,
      strategyId: 'canonical_ts',
    }),
  });
}

function createMissingEvidencePreviewStep() {
  return {
    step_id: 'step_003',
    capability: 'graph_write',
    result: {
      ok: false,
      status: 'failed',
      error: {
        code: 'missing_required_evidence',
        message: 'Field variable action requires explicit owner evidence: semantic=field field_operation=get.',
        field: 'task_plan.steps[2]',
      },
    },
  };
}

function createSignatureMismatchPreviewStep() {
  return {
    step_id: 'step_001',
    capability: 'blueprint_signature',
    result: {
      ok: false,
      status: 'failed',
      error: {
        code: 'function_signature_mismatch',
        message: 'Function signature mismatch: ComputeScore.',
        field: 'inputs',
      },
      data: {
        signature_differences: [
          {
            path: 'inputs[0].pin_type.category',
            expected: 'float',
            actual: 'int',
          },
        ],
      },
    },
  };
}

function createSignatureMismatchPreviewBridgeResponse(): BridgeResponse {
  return {
    success: true,
    request_id: 'preview_signature_mismatch_request',
    result: {
      ok: true,
      schema: TOOL_RESULT_SCHEMA,
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_signature_mismatch',
      status: 'dry_run',
      modified: false,
      data: {
        preview_token: '11223344556677889900aabbccddeeff',
        dry_run: {
          can_execute: false,
          result: 'blocked',
          errors: [
            {
              code: 'function_signature_mismatch',
              target: 'inputs',
              message: 'Function signature mismatch: ComputeScore.',
              source: 'task_runtime',
            },
          ],
          conflicts: [],
        },
        steps: [
          createSignatureMismatchPreviewStep(),
        ],
      },
    },
  };
}

function assertIssueCodes(value: unknown): string[] {
  assert.ok(Array.isArray(value));
  return value.map((issue) => {
    assert.equal(typeof issue, 'object');
    assert.notEqual(issue, null);
    const code = (issue as Record<string, unknown>)['code'];
    assert.equal(typeof code, 'string');
    return code as string;
  });
}

test('preview task omits cache diagnostics unless develop timing is enabled', async () => {
  const runner = createPreviewRunner();
  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);

  assert.equal(JSON.stringify(result.toolResult).includes('cache_diagnostics'), false);
  assert.equal(JSON.stringify(result.toolResult).includes('graph_write_execution_stats'), false);
});

test('preview task includes cache diagnostics when develop timing is enabled', async () => {
  const runner = createPreviewRunner();
  const timing = TaskTimingTrace.start('preview_task', 'agentface_test');
  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture, timing);

  assert.deepEqual(result.toolResult.data?.['cache_diagnostics'], {
    partial_preview_hits: 1,
  });
  assert.deepEqual(result.toolResult.data?.['graph_write_execution_stats'], {
    steps: [
      {
        step_id: 'step_001',
        spawned_node_count: 3,
      },
    ],
  });
});

test('preview task surfaces runtime-only validation notes for merge external routes', async () => {
  const runner = createRuntimeOnlyRoutePreviewRunner();
  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);

  assert.deepEqual(result.toolResult.data?.['runtime_only_validation'], [
    {
      route_id: 'graph.merge_external_flow.insert_between',
      validation_classification: 'runtime_only',
      runtime_only_validation_notes: [
        'UE schema connection may reject links during execute.',
        'insert_between requires exactly one successor and exactly one open inserted body exec output.',
      ],
    },
  ]);
  assert.equal(JSON.stringify(result.toolResult).includes('ue_preview_result'), false);
});

test('preview task reports TaskSpec compile failures as structured preview blockers', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand() {
      throw new Error('Bridge should not be called for compile failures.');
    },
  };
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => {
      throw new TaskSpecCompileError(
        'taskspec_semantic_invalid',
        'GraphWrite connectivity static preflight failed: unconsumed_pure_data_node.',
        [{
          code: 'unconsumed_pure_data_node',
          path: 'behavior.entries[0].body.statements[0]',
          message: 'PureData producer is not consumed.',
        }],
      );
    },
  });

  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);

  assert.equal(result.passed, false);
  assert.equal(result.taskPlan, undefined);
  assert.equal(result.toolResult.ok, false);
  assert.equal(result.toolResult.error?.code, 'taskspec_semantic_invalid');
  assert.equal(result.toolResult.data?.['passed'], false);
  assert.equal(result.toolResult.data?.['blocked'], true);
  assert.deepEqual(result.toolResult.data?.['issues'], [{
    code: 'unconsumed_pure_data_node',
    path: 'behavior.entries[0].body.statements[0]',
    message: 'PureData producer is not consumed.',
  }]);
});

test('preview task fails when a UE preview step failed even if dry_run says can_execute', async () => {
  const failedStepPreviewBridgeResponse: BridgeResponse = {
    success: true,
    request_id: 'preview_failed_step_request',
    result: {
      ok: true,
      schema: TOOL_RESULT_SCHEMA,
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_failed_step',
      status: 'dry_run',
      modified: false,
      data: {
        preview_token: 'fedcba9876543210fedcba9876543210',
        dry_run: {
          can_execute: true,
          result: 'passed',
          errors: [],
          conflicts: [],
        },
        steps: [
          {
            step_id: 'step_003',
            capability: 'graph_write',
            result: {
              ok: false,
              status: 'failed',
              error: {
                code: 'missing_required_evidence',
                message: 'Field variable action requires explicit owner evidence: semantic=field field_operation=get.',
                field: 'task_plan.steps[2]',
              },
            },
          },
        ],
      },
    },
  };

  const bridge: TaskRunnerBridge = {
    async sendCommand() {
      return failedStepPreviewBridgeResponse;
    },
  };
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);

  assert.equal(result.passed, false);
  assert.equal(result.toolResult.data?.['passed'], false);
  assert.equal(result.toolResult.data?.['blocked'], true);
  assert.equal(result.issues.length, 1);
  assert.equal(result.issues[0]?.code, 'missing_required_evidence');
  assert.equal(result.issues[0]?.path, 'task_plan.steps[2]');
  assert.match(result.issues[0]?.message ?? '', /explicit owner evidence/);
});

test('preview task deduplicates mirrored failed preview step issues', async () => {
  const mirroredFailedStepPreviewBridgeResponse: BridgeResponse = {
    success: true,
    request_id: 'preview_mirrored_failed_step_request',
    result: {
      ok: true,
      schema: TOOL_RESULT_SCHEMA,
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_mirrored_failed_step',
      status: 'dry_run',
      modified: false,
      steps: [
        createMissingEvidencePreviewStep(),
      ],
      data: {
        preview_token: '00112233445566778899aabbccddeeff',
        dry_run: {
          can_execute: true,
          result: 'passed',
          errors: [],
          conflicts: [],
          warnings: [],
          steps: [
            createMissingEvidencePreviewStep(),
          ],
        },
        steps: [
          createMissingEvidencePreviewStep(),
        ],
      },
    },
  };

  const runner = createRunnerForPreviewResponse(mirroredFailedStepPreviewBridgeResponse);
  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);
  const toolIssueCodes = assertIssueCodes(result.toolResult.data?.['issues']);

  assert.equal(result.issues.filter((issue) => issue.code === 'missing_required_evidence').length, 1);
  assert.equal(toolIssueCodes.filter((code) => code === 'missing_required_evidence').length, 1);
});

test('preview task preserves dry run issues while deduplicating failed preview steps', async () => {
  const dryRunAndFailedStepPreviewBridgeResponse: BridgeResponse = {
    success: true,
    request_id: 'preview_dry_run_issues_failed_step_request',
    result: {
      ok: true,
      schema: TOOL_RESULT_SCHEMA,
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_dry_run_issues_failed_step',
      status: 'dry_run',
      modified: false,
      data: {
        preview_token: 'ffeeddccbbaa99887766554433221100',
        dry_run: {
          can_execute: true,
          result: 'passed',
          errors: [
            {
              code: 'dry_run_error',
              target: 'dry_run.errors[0]',
              message: 'Dry run error must be preserved.',
            },
          ],
          conflicts: [
            {
              code: 'dry_run_conflict',
              target: 'dry_run.conflicts[0]',
              message: 'Dry run conflict must be preserved.',
            },
          ],
          warnings: [
            {
              code: 'dry_run_warning',
              target: 'dry_run.warnings[0]',
              message: 'Dry run warning must be preserved.',
            },
          ],
          steps: [
            createMissingEvidencePreviewStep(),
          ],
        },
        steps: [
          createMissingEvidencePreviewStep(),
        ],
      },
    },
  };

  const runner = createRunnerForPreviewResponse(dryRunAndFailedStepPreviewBridgeResponse);
  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);
  const codes = result.issues.map((issue) => issue.code);
  const toolIssueCodes = assertIssueCodes(result.toolResult.data?.['issues']);

  assert.deepEqual(codes, [
    'dry_run_error',
    'dry_run_conflict',
    'dry_run_warning',
    'missing_required_evidence',
  ]);
  assert.deepEqual(toolIssueCodes, codes);
});

test('preview task preserves candidate assistance data from failed preview steps', async () => {
  const candidates = [
    {
      candidate_id: 'mat_expr_12345678',
      display_name: 'Scalar Parameter',
      class_name: 'MaterialExpressionScalarParameter',
      category: 'Parameter',
      reason: 'query_match',
    },
  ];
  const candidatePreviewBridgeResponse: BridgeResponse = {
    success: true,
    request_id: 'preview_candidate_assistance_request',
    result: {
      ok: true,
      schema: TOOL_RESULT_SCHEMA,
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_candidate_assistance',
      status: 'dry_run',
      modified: false,
      data: {
        preview_token: '11223344556677889900aabbccddeeff',
        dry_run: {
          can_execute: true,
          result: 'passed',
          steps: [
            {
              step_id: 'material_candidate_step',
              capability: 'graph_write',
              result: {
                ok: false,
                status: 'failed',
                error: {
                  code: 'material_expression_candidate_confirmation_required',
                  message: 'MaterialGraph query selector requires candidate_id confirmation before execute.',
                  field: 'ops[2].selector',
                },
                data: {
                  asset_path: '/Game/Materials/M_Candidate',
                  query: 'scalar parameter',
                  fingerprint: 'candidate_cache_fingerprint',
                  schema_fingerprint: 'BlueprintHelper.MaterialExpressionCandidates.v1',
                  class_catalog_fingerprint: 'class_catalog_fingerprint',
                  class_catalog_revision: 'class_catalog_revision_001',
                  expires_in_seconds: 600,
                  candidates,
                },
              },
            },
          ],
        },
      },
    },
  };

  const runner = createRunnerForPreviewResponse(candidatePreviewBridgeResponse);
  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);
  const data = result.toolResult.data as Record<string, unknown>;
  const issue = (data['issues'] as Record<string, unknown>[])[0];
  const candidateSearch = data['candidate_search'] as Record<string, unknown>;

  assert.equal(result.passed, false);
  assert.equal(data['query'], 'scalar parameter');
  assert.deepEqual(data['candidates'], candidates);
  assert.deepEqual(issue['candidates'], candidates);
  assert.equal(candidateSearch['class_catalog_revision'], 'class_catalog_revision_001');
});

test('preview task preserves signature differences from failed preview step issues', async () => {
  const runner = createRunnerForPreviewResponse(createSignatureMismatchPreviewBridgeResponse());
  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);
  const issues = result.toolResult.data?.['issues'];

  assert.equal(result.passed, false);
  assert.ok(Array.isArray(issues));
  assert.equal(issues.length, 1);
  assert.equal(result.issues[0]?.code, 'function_signature_mismatch');
  assert.deepEqual((result.issues[0] as Record<string, unknown>)['signature_differences'], [
    {
      path: 'inputs[0].pin_type.category',
      expected: 'float',
      actual: 'int',
    },
  ]);
  assert.deepEqual((issues[0] as Record<string, unknown>)['signature_differences'], [
    {
      path: 'inputs[0].pin_type.category',
      expected: 'float',
      actual: 'int',
    },
  ]);
});

test('execute task promotes unique preview blocker code and keeps signature differences', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      assert.equal(command, 'preview_task_plan');
      return createSignatureMismatchPreviewBridgeResponse();
    },
  };
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.executeTask(graphWriteAppendTaskSpecFixture);
  const errorRecord = result.error as unknown as Record<string, unknown>;
  const issues = errorRecord['issues'];

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'function_signature_mismatch');
  assert.ok(Array.isArray(issues));
  assert.deepEqual((issues[0] as Record<string, unknown>)['signature_differences'], [
    {
      path: 'inputs[0].pin_type.category',
      expected: 'float',
      actual: 'int',
    },
  ]);
});

test('execute task auto-checks out before write when source-control checkout is required', async () => {
  const calls: string[] = [];
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      calls.push(command);
      if (command === 'preview_task_plan') {
        return previewBridgeResponse;
      }
      if (command === 'source_control_status') {
        return {
          success: true,
          request_id: 'source_control_status_checkout_required',
          result: {
            source_control: {
              status: 'checkout_required',
              files: [{
                asset_path: graphWriteAppendExpectedTaskPlanFixture.target_assets[0],
                status: 'checkout_required',
                editable: false,
              }],
            },
          },
        } as BridgeResponse;
      }
      if (command === 'source_control_checkout') {
        return editableSourceControlResponse(graphWriteAppendExpectedTaskPlanFixture.target_assets[0]);
      }
      if (command === 'execute_task_plan') {
        return executeBridgeResponse();
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.executeTask(graphWriteAppendTaskSpecFixture);

  assert.equal(result.ok, true);
  assert.deepEqual(calls, ['preview_task_plan', 'source_control_status', 'source_control_checkout', 'execute_task_plan']);
});

test('preview task records metrics with task identity and extracted operation', async () => {
  const events: MetricsEvent[] = [];
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand() {
        return previewBridgeResponse;
      },
    },
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteReplaceExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
    metrics: {
      record(event) {
        events.push(event);
      },
    },
  });

  const result = await runner.previewTask(graphWriteReplaceTaskSpecFixture);

  assert.equal(result.passed, true);
  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'taskspec_preview_completed');
  assert.equal(events[0]?.status, 'success');
  assert.equal(events[0]?.tool_name, 'blueprinthelper_preview_task');
  assert.equal(events[0]?.task_key?.task_type, 'edit_blueprint_graph');
  assert.equal(events[0]?.task_key?.feature_name, 'StoneGateActivationReplace');
  assert.match(events[0]?.task_spec_hash ?? '', /^sha256:[a-f0-9]{64}$/);
  assert.equal(events[0]?.capability, 'graph_write');
  assert.equal(events[0]?.semantic_operation, 'graph.replace.custom_event_body');
  assert.equal(JSON.stringify(result.toolResult).includes('BlueprintHelper.MetricsEvent.v1'), false);
});

test('preview task records compile failure metrics without calling bridge', async () => {
  const events: MetricsEvent[] = [];
  const runner = createTaskSpecRunner({
    bridge: {
      async sendCommand() {
        throw new Error('Bridge should not be called for compile failures.');
      },
    },
    taskCompiler: async () => {
      throw new TaskSpecCompileError(
        'taskspec_semantic_invalid',
        'GraphWrite connectivity static preflight failed: unconsumed_pure_data_node.',
        [{
          code: 'unconsumed_pure_data_node',
          path: 'behavior.entries[0].body.statements[0]',
          message: 'PureData producer is not consumed.',
        }],
      );
    },
    metrics: {
      record(event) {
        events.push(event);
      },
    },
  });

  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);

  assert.equal(result.passed, false);
  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'taskspec_preview_completed');
  assert.equal(events[0]?.status, 'failed');
});

test('execute task records pending_confirmation when success has no validation/readback evidence', async () => {
  const events: MetricsEvent[] = [];
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      if (command === 'preview_task_plan') {
        return previewBridgeResponse;
      }
      if (command === 'source_control_status') {
        return editableSourceControlResponse(graphWriteReplaceExpectedTaskPlanFixture.target_assets[0]);
      }
      if (command === 'execute_task_plan') {
        return executeBridgeResponse();
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteReplaceExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
    metrics: {
      record(event) {
        events.push(event);
      },
    },
  });

  const result = await runner.executeTask(graphWriteReplaceTaskSpecFixture);

  assert.equal(result.ok, true);
  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'taskspec_execute_completed');
  assert.equal(events[0]?.status, 'success');
  assert.equal(events[0]?.correctness_basis, 'pending_confirmation');
  assert.equal(events[0]?.capability, 'graph_write');
  assert.equal(events[0]?.semantic_operation, 'graph.replace.custom_event_body');
  assert.equal(JSON.stringify(result).includes('BlueprintHelper.MetricsEvent.v1'), false);
});

test('execute task with preview token preserves context_stale refresh action', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      if (command === 'source_control_status') {
        return editableSourceControlResponse(graphWriteAppendExpectedTaskPlanFixture.target_assets[0]);
      }
      if (command === 'execute_task_plan') {
        return {
          success: false,
          request_id: 'context_stale_request',
          error: {
            code: 'context_stale',
            message: 'Target Blueprint or graph structure changed after preview; run preview_task again.',
            field: 'preview_token.context_revision',
            retryable: true,
            agent_action: 'refresh_context_and_preview',
          },
        } as unknown as BridgeResponse;
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };

  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.executeTask(
    graphWriteAppendTaskSpecFixture,
    undefined,
    { previewToken: '0123456789abcdef0123456789abcdef' },
  );

  assert.equal(result.ok, false);
  const errorRecord = result.error as unknown as Record<string, unknown>;
  assert.equal(result.error?.code, 'context_stale');
  assert.equal(errorRecord['category'], 'bridge_error');
  assert.equal(result.error?.retryable, true);
  assert.equal(result.error?.field, 'preview_token.context_revision');
  assert.equal(errorRecord['agent_action'], 'refresh_context_and_preview');
});

test('execute task with preview token auto-checks out before write when source-control checkout is required', async () => {
  const calls: string[] = [];
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      calls.push(command);
      if (command === 'source_control_status') {
        return {
          success: true,
          request_id: 'source_control_status_preview_token_checkout_required',
          result: {
            source_control: {
              status: 'checkout_required',
              files: [{
                asset_path: graphWriteAppendExpectedTaskPlanFixture.target_assets[0],
                status: 'checkout_required',
                editable: false,
              }],
            },
          },
        } as BridgeResponse;
      }
      if (command === 'source_control_checkout') {
        return editableSourceControlResponse(graphWriteAppendExpectedTaskPlanFixture.target_assets[0]);
      }
      if (command === 'execute_task_plan') {
        return executeBridgeResponse();
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.executeTask(
    graphWriteAppendTaskSpecFixture,
    undefined,
    { previewToken: '0123456789abcdef0123456789abcdef' },
  );

  assert.equal(result.ok, true);
  assert.deepEqual(calls, ['source_control_status', 'source_control_checkout', 'execute_task_plan']);
});

test('execute task with preview token preserves context_stale from failed UE ToolResult', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      if (command === 'source_control_status') {
        return editableSourceControlResponse(graphWriteAppendExpectedTaskPlanFixture.target_assets[0]);
      }
      if (command === 'execute_task_plan') {
        return {
          success: true,
          request_id: 'context_stale_tool_result_request',
          result: {
            ok: false,
            operation: 'execute_task_plan',
            status: 'failed',
            modified: false,
            error: {
              code: 'execution_failed',
              message: 'Target Blueprint or graph structure changed after preview; run preview_task again.',
              field: 'bridge.execute_task_plan',
              retryable: false,
              agent_action: 'refresh_context_and_preview',
              issues: [
                {
                  code: 'context_stale',
                  path: 'preview_token.context_revision',
                  message: 'Target Blueprint or graph structure changed after preview; run preview_task again.',
                },
              ],
            },
          },
        } as unknown as BridgeResponse;
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };

  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.executeTask(
    graphWriteAppendTaskSpecFixture,
    undefined,
    { previewToken: '0123456789abcdef0123456789abcdef' },
  );

  assert.equal(result.ok, false);
  const errorRecord = result.error as unknown as Record<string, unknown>;
  assert.equal(result.error?.code, 'context_stale');
  assert.equal(errorRecord['category'], 'bridge_error');
  assert.equal(result.error?.retryable, true);
  assert.equal(result.error?.field, 'preview_token.context_revision');
  assert.equal(errorRecord['agent_action'], 'refresh_context_and_preview');
});

test('execute task with preview token preserves signature differences from failed UE ToolResult data', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      if (command === 'source_control_status') {
        return editableSourceControlResponse(graphWriteAppendExpectedTaskPlanFixture.target_assets[0]);
      }
      if (command === 'execute_task_plan') {
        return {
          success: true,
          request_id: 'signature_mismatch_execute_tool_result_request',
          result: {
            ok: false,
            operation: 'ensure_function',
            status: 'failed',
            modified: false,
            error: {
              code: 'function_signature_mismatch',
              message: 'Function signature mismatch: ComputeScore.',
              field: 'inputs',
              retryable: false,
            },
            data: {
              signature_differences: [
                {
                  path: 'inputs[0].pin_type.category',
                  expected: 'float',
                  actual: 'int',
                },
              ],
            },
          },
        } as unknown as BridgeResponse;
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };

  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.executeTask(
    graphWriteAppendTaskSpecFixture,
    undefined,
    { previewToken: '0123456789abcdef0123456789abcdef' },
  );
  const errorRecord = result.error as unknown as Record<string, unknown>;
  const issues = errorRecord['issues'];

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'function_signature_mismatch');
  assert.ok(Array.isArray(issues));
  assert.deepEqual((issues[0] as Record<string, unknown>)['signature_differences'], [
    {
      path: 'inputs[0].pin_type.category',
      expected: 'float',
      actual: 'int',
    },
  ]);
});

test('execute task classifies bridge request timeout as editor blocked recovery', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      if (command === 'preview_task_plan') {
        return previewBridgeResponse;
      }
      if (command === 'source_control_status') {
        return editableSourceControlResponse(graphWriteAppendExpectedTaskPlanFixture.target_assets[0]);
      }
      if (command === 'execute_task_plan') {
        throw new Error('Bridge request timed out after 600000ms');
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };

  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.executeTask(graphWriteAppendTaskSpecFixture);
  const data = result.data as Record<string, unknown> | undefined;
  const recovery = data?.editor_blocked_recovery as Record<string, unknown> | undefined;
  const errorRecord = result.error as unknown as Record<string, unknown>;

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'unknown_mutation_state');
  assert.equal(errorRecord['category'], 'editor_command_blocked');
  assert.equal(result.error?.stage, 'bridge');
  assert.equal(recovery?.code, 'unknown_mutation_state');
  assert.equal(recovery?.category, 'editor_command_blocked');
});

test('execute task classifies bridge connection timeout as transport recovery', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      if (command === 'preview_task_plan') {
        return previewBridgeResponse;
      }
      if (command === 'source_control_status') {
        throw new Error('Bridge connection timed out after 30000ms');
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };

  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.executeTask(graphWriteAppendTaskSpecFixture);
  const data = result.data as Record<string, unknown> | undefined;
  const recovery = data?.editor_blocked_recovery as Record<string, unknown> | undefined;
  const errorRecord = result.error as unknown as Record<string, unknown>;

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'transport_connect_timeout');
  assert.equal(errorRecord['category'], 'bridge_unavailable');
  assert.equal(result.error?.stage, 'bridge');
  assert.equal(recovery?.code, 'transport_connect_timeout');
  assert.equal(recovery?.category, 'bridge_unavailable');
});

test('metrics sink failures do not change preview or execute results', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      if (command === 'preview_task_plan') {
        return previewBridgeResponse;
      }
      if (command === 'source_control_status') {
        return editableSourceControlResponse(graphWriteAppendExpectedTaskPlanFixture.target_assets[0]);
      }
      if (command === 'execute_task_plan') {
        return executeBridgeResponse();
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
    metrics: {
      record() {
        throw new Error('metrics store unavailable');
      },
    },
  });

  const preview = await runner.previewTask(graphWriteAppendTaskSpecFixture);
  const execute = await runner.executeTask(graphWriteAppendTaskSpecFixture);

  assert.equal(preview.passed, true);
  assert.equal(preview.toolResult.ok, true);
  assert.equal(execute.ok, true);
  assert.equal(execute.operation, 'execute_task');
});

test('preview and execute ToolResult target uses TaskPlan target type for material graphs', async () => {
  const materialTaskSpec = {
    schema_version: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_material_graph',
    target: {
      asset_path: '/Game/Materials/M_TargetProjection',
      target_type: 'material_graph',
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [],
    },
  } as unknown as TaskSpec;
  const materialTaskPlan = {
    ...graphWriteAppendExpectedTaskPlanFixture,
    task_name: 'MaterialTargetProjection',
    task_type: 'edit_material_graph',
    target_assets: ['/Game/Materials/M_TargetProjection'],
    steps: [
      {
        ...(graphWriteAppendExpectedTaskPlanFixture.steps[0] as Record<string, unknown>),
        target: {
          asset_path: '/Game/Materials/M_TargetProjection',
          graph: 'MaterialGraph',
          target_type: 'material_graph',
        },
        write: {
          strategy: 'owned_graph_edit',
          graph_domain: 'material_graph',
          material_strategy: 'append_new_owned_graph',
          ops: [],
        },
      },
    ],
  } as unknown as TaskPlan;
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      if (command === 'preview_task_plan') {
        return previewBridgeResponse;
      }
      if (command === 'source_control_status') {
        return editableSourceControlResponse('/Game/Materials/M_TargetProjection');
      }
      if (command === 'execute_task_plan') {
        return {
          success: true,
          request_id: 'execute_material_target_projection',
          result: {
            ok: true,
            schema: TOOL_RESULT_SCHEMA,
            operation: 'execute_task_plan',
            trace_id: 'trace_execute_material_target_projection',
            status: 'completed',
            modified: true,
            data: {
              task_run_id: 'run_material_target_projection',
              target_assets: ['/Game/Materials/M_TargetProjection'],
              steps: [
                {
                  step_id: 'step_001',
                  operation: 'material_graph_edit',
                  status: 'completed',
                },
              ],
            },
          },
        } as BridgeResponse;
      }
      throw new Error(`Unexpected command ${command}.`);
    },
  };
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: materialTaskPlan,
      strategyId: 'canonical_ts',
    }),
  });

  const preview = await runner.previewTask(materialTaskSpec);
  const execute = await runner.executeTask(materialTaskSpec);

  assert.deepEqual(preview.toolResult.target, {
    target_type: 'material_graph',
    asset_path: '/Game/Materials/M_TargetProjection',
  });
  assert.deepEqual(execute.target, {
    target_type: 'material_graph',
    asset_path: '/Game/Materials/M_TargetProjection',
  });
});

function editableSourceControlResponse(assetPath = '/Game/Blueprints/BP_StoneGate'): BridgeResponse {
  return {
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

function executeBridgeResponse(): BridgeResponse {
  return {
    success: true,
    request_id: 'execute_metrics_test_request',
    result: {
      ok: true,
      schema: TOOL_RESULT_SCHEMA,
      operation: 'execute_task_plan',
      trace_id: 'trace_execute_metrics_test',
      status: 'completed',
      modified: true,
      data: {
        task_run_id: 'run_metrics_test',
        target_assets: ['/Game/Blueprints/BP_StoneGate'],
        steps: [
          {
            step_id: 'step_001',
            modified: true,
          },
        ],
      },
    },
  };
}
