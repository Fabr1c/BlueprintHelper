import assert from 'node:assert/strict';
import test from 'node:test';

import type { MetricsEvent } from './metrics-types.js';
import { createMetricsCollector } from './metrics-collector.js';

test('recordTaskPreviewCompleted emits sanitized task summary without raw payloads', async () => {
  const events: MetricsEvent[] = [];
  const collector = createMetricsCollector({
    sink: {
      record(event) {
        events.push(event);
      },
    },
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await collector.recordTaskPreviewCompleted({
    taskSpec: {
      schema: 'BlueprintHelper.TaskSpec.v1',
      task_type: 'create_blueprint_feature',
      feature_name: 'ReviewPanel_UI',
      target: {
        target_type: 'blueprint',
        asset_path: '/Game/UI/WBP_ReviewPanel',
        ref: {
          secretNode: 'SECRET_TASKSPEC_NODE',
        },
      },
      private_notes: 'SECRET_TASKSPEC_FIELD',
    } as never,
    passed: false,
    toolResult: {
      error_code: 'unsupported_scope_policy',
      error_category: 'capability_boundary',
      issue: {
        code: 'unsupported_scope_policy',
        path: 'body.statements[0].SECRET_PATH',
        message: 'SECRET_ISSUE_MESSAGE',
      },
      raw_payload: {
        token: 'SECRET_RAW_PAYLOAD',
      },
      DebugBundle: {
        token: 'SECRET_DEBUG_BUNDLE',
      },
    },
    duration_ms: 123,
  });

  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'taskspec_preview_completed');
  assert.equal(events[0]?.status, 'failed');
  assert.equal(events[0]?.error_category, 'capability_boundary');
  assert.equal(events[0]?.error_code, 'unsupported_scope_policy');
  assert.match(events[0]?.task_spec_hash ?? '', /^sha256:[a-f0-9]{64}$/);
  assert.match(events[0]?.task_key?.target_ref_label ?? '', /^\/Game\/.*WBP_ReviewPanel$/);
  assert.equal(events[0]?.issue?.code, 'unsupported_scope_policy');
  assert.match(events[0]?.issue?.message_digest ?? '', /^sha256:[a-f0-9]{64}$/);

  const serialized = JSON.stringify(events[0]);
  assert.doesNotMatch(serialized, /SECRET_TASKSPEC_NODE/);
  assert.doesNotMatch(serialized, /SECRET_TASKSPEC_FIELD/);
  assert.doesNotMatch(serialized, /SECRET_RAW_PAYLOAD/);
  assert.doesNotMatch(serialized, /SECRET_DEBUG_BUNDLE/);
  assert.doesNotMatch(serialized, /SECRET_ISSUE_MESSAGE/);
  assert.doesNotMatch(serialized, /raw_payload/);
  assert.doesNotMatch(serialized, /DebugBundle/);
});

test('collector normalizes execute tool step validation and readback events without raw payloads', async () => {
  const events: MetricsEvent[] = [];
  const collector = createMetricsCollector({
    sink: {
      record(event) {
        events.push(event);
      },
    },
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });
  const taskSpec = createSecretTaskSpec();

  await collector.recordTaskExecuteCompleted({
    taskSpec,
    passed: true,
    validationPassed: true,
    readbackPassed: true,
    toolResult: createRawToolResult('execute_should_not_leak'),
    duration_ms: 50,
    capability: 'graph_write',
    semantic_operation: 'call',
  });
  await collector.recordToolCompleted({
    tool_name: 'blueprinthelper_read_context',
    status: 'failed',
    toolResult: createRawToolResult('tool_should_not_leak'),
    duration_ms: 25,
    capability: 'read_context',
    semantic_operation: 'read.logic_flow',
  });
  await collector.recordTaskStepCompleted({
    taskSpec,
    status: 'failed',
    toolResult: createRawToolResult('step_should_not_leak'),
    duration_ms: 10,
    capability: 'graph_write',
    semantic_operation: 'set',
  });
  await collector.recordValidationCompleted({
    taskSpec,
    passed: false,
    toolResult: createRawToolResult('validation_should_not_leak'),
    duration_ms: 5,
  });
  await collector.recordReadbackCompleted({
    taskSpec,
    passed: true,
    toolResult: createRawToolResult('readback_should_not_leak'),
    duration_ms: 7,
  });

  assert.deepEqual(events.map((event) => event.event_type), [
    'taskspec_execute_completed',
    'tool_completed',
    'taskstep_completed',
    'validation_completed',
    'readback_completed',
  ]);
  assert.equal(events[0]?.tool_name, 'blueprinthelper_execute_task');
  assert.equal(events[0]?.status, 'success');
  assert.equal(events[0]?.correctness_basis, 'validation_readback');
  assert.equal(events[1]?.tool_name, 'blueprinthelper_read_context');
  assert.equal(events[1]?.status, 'failed');
  assert.equal(events[1]?.error_category, 'runtime_state_error');
  assert.equal(events[2]?.status, 'failed');
  assert.equal(events[2]?.capability, 'graph_write');
  assert.equal(events[3]?.status, 'failed');
  assert.equal(events[3]?.error_code, 'validation_failed');
  assert.equal(events[4]?.status, 'success');
  assert.match(events[0]?.task_spec_hash ?? '', /^sha256:[a-f0-9]{64}$/);
  assert.match(events[2]?.task_spec_hash ?? '', /^sha256:[a-f0-9]{64}$/);

  const serialized = JSON.stringify(events);
  assert.doesNotMatch(serialized, /SECRET_TASKSPEC_NODE/);
  assert.doesNotMatch(serialized, /SECRET_TASKSPEC_FIELD/);
  assert.doesNotMatch(serialized, /execute_should_not_leak/);
  assert.doesNotMatch(serialized, /tool_should_not_leak/);
  assert.doesNotMatch(serialized, /step_should_not_leak/);
  assert.doesNotMatch(serialized, /validation_should_not_leak/);
  assert.doesNotMatch(serialized, /readback_should_not_leak/);
  assert.doesNotMatch(serialized, /raw_payload/);
  assert.doesNotMatch(serialized, /DebugBundle/);
});

test('collector classifies errors through the shared metrics classifier', async () => {
  const events: MetricsEvent[] = [];
  const collector = createMetricsCollector({
    sink: {
      record(event) {
        events.push(event);
      },
    },
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await collector.recordToolCompleted({
    tool_name: 'blueprinthelper_execute_task',
    status: 'failed',
    toolResult: {
      error: {
        code: 'bridge_unavailable',
      },
    },
  });
  await collector.recordTaskStepCompleted({
    taskSpec: createSecretTaskSpec(),
    status: 'failed',
    toolResult: {
      issue_code: 'unsupported_scope_policy',
    },
  });
  await collector.recordValidationCompleted({
    taskSpec: createSecretTaskSpec(),
    passed: false,
    toolResult: {
      issue: {
        code: 'missing_required_field',
        message: 'SECRET_CLASSIFIER_MESSAGE',
      },
    },
  });
  await collector.recordReadbackCompleted({
    taskSpec: createSecretTaskSpec(),
    passed: false,
    toolResult: {
      error: {
        issues: [
          {
            code: 'asset_not_found',
            path: 'target.asset_path',
            message: 'SECRET_NESTED_ISSUE',
          },
        ],
      },
    },
  });

  assert.equal(events[0]?.error_category, 'runtime_state_error');
  assert.equal(events[0]?.error_code, 'bridge_unavailable');
  assert.equal(events[1]?.error_category, 'capability_boundary');
  assert.equal(events[1]?.error_code, 'unsupported_scope_policy');
  assert.equal(events[2]?.error_category, 'parameter_error');
  assert.equal(events[2]?.error_code, 'missing_required_field');
  assert.equal(events[3]?.error_category, 'context_error');
  assert.equal(events[3]?.error_code, 'asset_not_found');
  assert.equal(events[3]?.issue?.path, 'target.asset_path');

  const serialized = JSON.stringify(events);
  assert.doesNotMatch(serialized, /SECRET_CLASSIFIER_MESSAGE/);
  assert.doesNotMatch(serialized, /SECRET_NESTED_ISSUE/);
});

test('collector records CLI IO sizes without raw payload content', async () => {
  const events: MetricsEvent[] = [];
  const collector = createMetricsCollector({
    sink: {
      record(event) {
        events.push(event);
      },
    },
    now: () => new Date('2026-06-04T08:00:00.000Z'),
  });

  await collector.recordCliIoCompleted({
    tool_name: 'blueprinthelper_read_context',
    status: 'success',
    capability: 'read_context',
    semantic_operation: 'blueprint_logic.logic_flow',
    io: {
      input_source: 'stdin',
      input_chars: 19,
      input_utf8_bytes: 21,
      output_chars: 41,
      output_utf8_bytes: 43,
      estimated_input_tokens: 5,
      estimated_output_tokens: 11,
    },
  });

  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'cli_io_completed');
  assert.equal(events[0]?.tool_name, 'blueprinthelper_read_context');
  assert.equal(events[0]?.status, 'success');
  assert.deepEqual(events[0]?.io, {
    input_source: 'stdin',
    input_chars: 19,
    input_utf8_bytes: 21,
    output_chars: 41,
    output_utf8_bytes: 43,
    estimated_input_tokens: 5,
    estimated_output_tokens: 11,
  });

  const serialized = JSON.stringify(events[0]);
  assert.doesNotMatch(serialized, /SECRET/);
  assert.doesNotMatch(serialized, /raw_payload/);
});

function createSecretTaskSpec(): never {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'create_blueprint_feature',
    feature_name: 'ReviewPanel_UI',
    target: {
      target_type: 'blueprint',
      asset_path: '/Game/UI/WBP_ReviewPanel',
      ref: {
        secretNode: 'SECRET_TASKSPEC_NODE',
      },
    },
    private_notes: 'SECRET_TASKSPEC_FIELD',
  } as never;
}

function createRawToolResult(secret: string): unknown {
  return {
    error_code: 'validation_failed',
    error_category: 'runtime_state_error',
    issue: {
      code: 'validation_failed',
      path: 'body.statements[0]',
      message: secret,
    },
    raw_payload: {
      secret,
    },
    DebugBundle: {
      secret,
    },
  };
}
