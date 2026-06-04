import { strict as assert } from 'node:assert';
import test from 'node:test';

import type { ToolResultBase } from '@blueprinthelper/task-core/result/tool-result';
import { buildCliSummary, writeCliResult, type CliCommand } from './output.js';

test('CLI summary exposes preview-blocked connectivity violations concisely', () => {
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'summary',
  };
  const toolResult: ToolResultBase = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'execute_task_plan',
    trace_id: 'trace_cli_connectivity',
    status: 'dry_run',
    modified: false,
    data: {
      passed: false,
      issues: [{
        code: 'unconsumed_pure_data_node',
        severity: 'error',
        message: 'PureData node is generated but never consumed.',
        node_id: 'Pure_1',
        path: 'behavior.entries[0].body.statements[0]',
        internal_debug_only: 'not propagated',
      }],
    },
  };

  const summary = buildCliSummary({
    command,
    toolResult,
    artifactRefs: {},
  });

  assert.equal(summary.status, 'preview_blocked');
  assert.equal(summary.error_code, 'unconsumed_pure_data_node');
  assert.equal(summary.message, 'PureData node is generated but never consumed.');
  assert.deepEqual(summary.violations, [{
    code: 'unconsumed_pure_data_node',
    node_id: 'Pure_1',
    path: 'behavior.entries[0].body.statements[0]',
    message: 'PureData node is generated but never consumed.',
  }]);
});

test('CLI execute summary keeps preview connectivity blocker visible', () => {
  const command: CliCommand = {
    kind: 'task.execute',
    format: 'summary',
  };
  const toolResult = {
    ok: false,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'execute_task',
    trace_id: 'trace_cli_execute_connectivity',
    status: 'failed',
    modified: false,
    error: {
      code: 'task_preview_blocked',
      stage: 'parse_input',
      message: 'Task preview was blocked; execute_task did not write assets.',
      retryable: true,
      rollback_result: 'not_needed',
      issues: [{
        code: 'graphwrite_connectivity_failed',
        path: 'logic_spec',
        message: 'GraphWrite connectivity validation failed.',
      }],
    },
  } as ToolResultBase;

  const summary = buildCliSummary({
    command,
    toolResult,
    artifactRefs: {},
  });

  assert.equal(summary.status, 'execute_failed');
  assert.equal(summary.error_code, 'task_preview_blocked');
  assert.deepEqual(summary.violations, [{
    code: 'graphwrite_connectivity_failed',
    path: 'logic_spec',
    message: 'GraphWrite connectivity validation failed.',
  }]);
});

test('CLI full output exposes preview-blocked issue code for selected fields', () => {
  const chunks: string[] = [];
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'full',
    fields: ['status', 'error_code', 'message', 'artifacts.full_result'],
  };
  const toolResult: ToolResultBase = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'preview_task',
    trace_id: 'trace_cli_signature_mismatch',
    status: 'dry_run',
    modified: false,
    data: {
      passed: false,
      issues: [{
        code: 'function_signature_mismatch',
        path: 'inputs',
        message: 'Function signature mismatch: ComputeScore.',
      }],
    },
  };

  writeCliResult({
    cwd: process.cwd(),
    stdout: (text) => chunks.push(text),
  }, command, toolResult);
  const output = JSON.parse(chunks.join('')) as Record<string, unknown>;

  assert.equal(output.status, 'preview_blocked');
  assert.equal(output.error_code, 'function_signature_mismatch');
  assert.equal(output.message, 'Function signature mismatch: ComputeScore.');
  assert.ok((output.artifacts as Record<string, unknown>)['full_result']);
});

test('CLI preview summary exposes static preflight issue from error payload', () => {
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'summary',
  };
  const toolResult = {
    ok: false,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'preview_task',
    trace_id: 'trace_cli_static_preflight',
    status: 'failed',
    modified: false,
    data: {
      passed: false,
      blocked: true,
      task_type: 'edit_blueprint_graph',
      issues: [{
        code: 'unconsumed_pure_data_node',
        path: 'behavior.entries[0].body.statements[0]',
        message: 'PureData producer is not consumed.',
      }],
    },
    error: {
      code: 'taskspec_semantic_invalid',
      stage: 'preflight',
      message: 'GraphWrite connectivity static preflight failed: unconsumed_pure_data_node.',
      retryable: true,
      rollback_result: 'not_needed',
      issues: [{
        code: 'unconsumed_pure_data_node',
        path: 'behavior.entries[0].body.statements[0]',
        message: 'PureData producer is not consumed.',
      }],
    },
  } as ToolResultBase;

  const summary = buildCliSummary({
    command,
    toolResult,
    artifactRefs: {},
  });

  assert.equal(summary.status, 'preview_blocked');
  assert.equal(summary.error_code, 'taskspec_semantic_invalid');
  assert.deepEqual(summary.violations, [{
    code: 'unconsumed_pure_data_node',
    path: 'behavior.entries[0].body.statements[0]',
    message: 'PureData producer is not consumed.',
  }]);
});
