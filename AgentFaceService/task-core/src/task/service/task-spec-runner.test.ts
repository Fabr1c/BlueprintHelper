import { strict as assert } from 'node:assert';
import test from 'node:test';

import type { BridgeResponse } from '../../bridge/bridge-client.js';
import { TOOL_RESULT_SCHEMA } from '../../result/tool-result.js';
import { TaskSpecCompileError, createCompiledTaskPlan } from '../compiler/task-compiler.js';
import {
  graphWriteAppendExpectedTaskPlanFixture,
  graphWriteAppendTaskSpecFixture,
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
