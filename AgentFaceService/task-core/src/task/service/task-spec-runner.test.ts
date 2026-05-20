import { strict as assert } from 'node:assert';
import test from 'node:test';

import type { BridgeResponse } from '../../bridge/bridge-client.js';
import { TOOL_RESULT_SCHEMA } from '../../result/tool-result.js';
import { createCompiledTaskPlan } from '../compiler/task-compiler.js';
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
      strategyId: 'ts_fast_path',
    }),
  });
}

test('preview task omits cache diagnostics unless develop timing is enabled', async () => {
  const runner = createPreviewRunner();
  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);

  assert.equal(JSON.stringify(result.toolResult).includes('cache_diagnostics'), false);
});

test('preview task includes cache diagnostics when develop timing is enabled', async () => {
  const runner = createPreviewRunner();
  const timing = TaskTimingTrace.start('preview_task', 'agentface_test');
  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture, timing);

  assert.deepEqual(result.toolResult.data?.['cache_diagnostics'], {
    partial_preview_hits: 1,
  });
});
