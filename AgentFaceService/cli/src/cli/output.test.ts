import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import test from 'node:test';

import type { ToolResultBase } from '@blueprinthelper/task-core/result/tool-result';
import { buildCliError, buildCliSummary, writeCliResult, type CliCommand } from './output.js';

test('CLI summary exposes preview-blocked connectivity violations concisely', () => {
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'summary',
    resultPolicyId: 'task.preview.default',
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
    resultPolicyId: 'task.execute.default',
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

test('CLI summary exposes MaterialGraph connectivity violations with material anchors', () => {
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'summary',
    resultPolicyId: 'task.preview.default',
  };
  const toolResult: ToolResultBase = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'preview_task',
    trace_id: 'trace_cli_material_connectivity',
    status: 'dry_run',
    modified: false,
    data: {
      passed: false,
      connectivity: {
        violations: [{
          code: 'material_unconsumed_expression',
          severity: 'error',
          message: "MaterialGraph generated expression 'bh_orphan' has no outgoing material data connection.",
          graph_name: 'MaterialGraph',
          target_key: 'bh_orphan',
          pin_name: 'RGB',
          field: 'entries[0].nodes[0]',
          internal_debug_only: 'not propagated',
        }],
      },
    },
  };

  const summary = buildCliSummary({
    command,
    toolResult,
    artifactRefs: {},
  });

  assert.equal(summary.status, 'preview_blocked');
  assert.deepEqual(summary.violations, [{
    code: 'material_unconsumed_expression',
    target_key: 'bh_orphan',
    graph_name: 'MaterialGraph',
    pin_name: 'RGB',
    field: 'entries[0].nodes[0]',
    message: "MaterialGraph generated expression 'bh_orphan' has no outgoing material data connection.",
  }]);
});

test('CLI execute summary exposes MaterialGraph connectivity counters', () => {
  const command: CliCommand = {
    kind: 'task.execute',
    format: 'summary',
    resultPolicyId: 'task.execute.default',
  };
  const toolResult: ToolResultBase = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'execute_task',
    trace_id: 'trace_cli_material_connectivity_counters',
    status: 'completed',
    modified: true,
    data: {
      requested_connections: 2,
      verified_connections: 2,
      graph_sync_connections: 2,
      connectivity_violation_count: 0,
    },
  };

  const summary = buildCliSummary({
    command,
    toolResult,
    artifactRefs: {},
  });

  assert.deepEqual((summary.summary as Record<string, unknown>).connectivity, {
    requested: 2,
    verified: 2,
    graph_sync: 2,
    violations: 0,
  });
});

test('CLI full output exposes preview-blocked issue code for selected fields', () => {
  const chunks: string[] = [];
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'full',
    resultPolicyId: 'task.preview.default',
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
    resultPolicyId: 'task.preview.default',
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

test('CLI summary exposes review baseline dirty recovery guidance', () => {
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'summary',
    resultPolicyId: 'task.preview.default',
  };
  const toolResult = {
    ok: false,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'preview_task',
    trace_id: 'trace_cli_dirty_baseline',
    status: 'failed',
    modified: false,
    error: {
      code: 'review_baseline_dirty_target_assets',
      category: 'runtime_state_error',
      stage: 'preflight',
      message: 'Review baseline requires clean target assets before archive.',
      retryable: false,
      rollback_result: 'not_needed',
      dirty_state: 'dirty_with_open_review',
      dirty_assets: ['/Game/BP_Dirty'],
      safe_next_action: 'review.reject_or_accept_pending_changes_then_retry',
      allowed_recovery_actions: ['review.reject', 'review.accept'],
    },
  } as ToolResultBase;

  const summary = buildCliSummary({
    command,
    toolResult,
    artifactRefs: {},
  });

  assert.equal(summary.status, 'preview_blocked');
  assert.equal(summary.error_code, 'review_baseline_dirty_target_assets');
  assert.equal(summary.category, 'runtime_state_error');
  assert.equal(summary.stage, 'preflight');
  assert.equal(summary.dirty_state, 'dirty_with_open_review');
  assert.deepEqual(summary.dirty_assets, ['/Game/BP_Dirty']);
  assert.equal(summary.safe_next_action, 'review.reject_or_accept_pending_changes_then_retry');
  assert.deepEqual(summary.allowed_recovery_actions, ['review.reject', 'review.accept']);
});

test('CLI error output omits wrapper schema by default', () => {
  const output = buildCliError({
    operation: 'output',
    status: 'cli_error',
    message: 'Policy blocked execution.',
  });

  assert.equal('schema' in output, false);
  assert.equal(output.ok, false);
  assert.equal(output.operation, 'output');
  assert.equal(output.status, 'cli_error');
  assert.equal(output.message, 'Policy blocked execution.');
});

test('CLI full result keeps validation errors but drops validation policy keys', () => {
  const chunks: string[] = [];
  writeCliResult({
    cwd: process.cwd(),
    stdout: (text) => chunks.push(text),
  }, {
    kind: 'task.execute',
    format: 'full',
    resultPolicyId: 'task.execute.default',
  }, {
    ok: false,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'execute_task',
    trace_id: 'trace_validation_details',
    status: 'failed',
    modified: false,
    validation: {
      should_compile: true,
      should_save: false,
      compiled: true,
      saved: false,
      compile_success: false,
      errors: [{
        code: 'compile_error',
        message: 'Blueprint compile failed.',
        severity: 'error',
      }],
      warnings: [],
    },
  });

  const output = JSON.parse(chunks.join('')) as Record<string, unknown>;
  const fullPath = String((output.artifacts as Record<string, unknown>).full_result);
  const fullResult = JSON.parse(fs.readFileSync(fullPath, 'utf8')) as Record<string, unknown>;
  const serialized = JSON.stringify(fullResult);
  assert.doesNotMatch(serialized, /"should_compile"/);
  assert.doesNotMatch(serialized, /"should_save"/);
  assert.match(serialized, /"compile_success":false/);
  assert.match(serialized, /"compile_error"/);
});

test('expert read_context debug artifact keeps logic_flow anchors while full_result omits debug', () => {
  const chunks: string[] = [];
  const toolResult = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'read_context',
    trace_id: 'trace_logic_flow_anchors',
    status: 'completed',
    modified: false,
    target: { target_type: 'blueprint', asset_path: '/Game/BP_Test' },
    data: {
      schema: 'ReadContextPack.v1',
      payload: {
        schema: 'LogicFlow.v1',
        mode: 'execflow',
        flow: 'OpenDoor -> PrintString',
        stats: { nodes: 2, exec_links: 1, data_links: 0 },
        warnings: [],
      },
    },
    debug: {
      logic_flow: {
        anchors: [{ semantic_role: 'exec_boundary', fingerprint: 'boundaryfp' }],
      },
    },
  } as ToolResultBase;

  const outcome = writeCliResult({
    cwd: process.cwd(),
    stdout: (text) => chunks.push(text),
  }, {
    kind: 'tool.invoke',
    format: 'json',
    toolName: 'blueprinthelper_read_context',
    expert: true,
  }, toolResult);

  const output = JSON.parse(chunks.join('')) as Record<string, unknown>;
  const artifacts = output.artifacts as Record<string, unknown>;
  const fullResult = JSON.parse(fs.readFileSync(String(artifacts.full_result), 'utf8')) as Record<string, unknown>;
  const debugResult = JSON.parse(fs.readFileSync(String(artifacts.debug_result), 'utf8')) as Record<string, unknown>;

  assert.equal(outcome.outputTooLarge, false);
  assert.equal(((fullResult.toolResult as Record<string, unknown>).data as Record<string, unknown>)['debug'], undefined);
  assert.equal((fullResult.toolResult as Record<string, unknown>)['debug'], undefined);
  const debug = debugResult.debug as Record<string, unknown>;
  const logicFlowDebug = debug['logic_flow'] as Record<string, unknown>;
  const anchors = logicFlowDebug['anchors'] as Record<string, unknown>[];
  assert.equal(anchors[0]?.['fingerprint'], 'boundaryfp');
});
