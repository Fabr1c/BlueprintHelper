import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
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

test('CLI error projection preserves machine envelope fields even when selected fields omit them', () => {
  const chunks: string[] = [];
  const suggestedRoute = {
    route_id: 'blueprint_class_settings.class_default',
    family: 'blueprint_class_settings',
    operation_id: 'set_class_default',
    task_type: 'edit_blueprint_class_settings',
    property_path_hint: 'WeaponComponent.PrimaryWeapon',
  };
  const blockedBoundary = {
    boundary_id: 'component_tree_owned_scs_only',
    origin: 'native',
    blocked_operation: 'set_component_properties',
  };
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'summary',
    resultPolicyId: 'task.preview.default',
    fields: ['status'],
    omitFields: ['ok', 'operation', 'error_code', 'message', 'suggested_route', 'blocked_boundary'],
  };
  const toolResult: ToolResultBase = {
    ok: false,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'preview_task',
    trace_id: 'trace_projection_guard',
    status: 'failed',
    modified: false,
    error: {
      code: 'editor_not_ready',
      category: 'runtime_unavailable',
      stage: 'bridge',
      message: 'Editor is not ready.',
      retryable: true,
      rollback_result: 'not_needed',
      safe_next_action: 'editor.open_or_wait_then_retry',
      allowed_recovery_actions: ['editor.open', 'bridge.ping'],
      suggested_route: suggestedRoute,
      blocked_boundary: blockedBoundary,
    },
  };

  writeCliResult({
    cwd: process.cwd(),
    stdout: (text) => chunks.push(text),
  }, command, toolResult);

  const output = JSON.parse(chunks.join('')) as Record<string, unknown>;
  assert.equal(output.ok, false);
  assert.equal(output.operation, 'task.preview');
  assert.equal(output.status, 'preview_blocked');
  assert.equal(output.error_code, 'editor_not_ready');
  assert.equal(output.message, 'Editor is not ready.');
  assert.equal(output.safe_next_action, 'editor.open_or_wait_then_retry');
  assert.deepEqual(output.allowed_recovery_actions, ['editor.open', 'bridge.ping']);
  assert.deepEqual(output.suggested_route, suggestedRoute);
  assert.deepEqual(output.blocked_boundary, blockedBoundary);
});

test('CLI output preserves receipt identity even when selected and omitted fields remove it', () => {
  const chunks: string[] = [];
  const command: CliCommand = {
    kind: 'task.execute',
    format: 'json',
    resultPolicyId: 'task.execute.default',
    fields: ['status'],
    omitFields: ['receipt_id', 'receipt', 'verification_hash', 'verification_status', 'tool_result.data.receipt'],
  };
  const toolResult: ToolResultBase = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'execute_task',
    trace_id: 'trace_receipt_guard',
    status: 'completed',
    modified: true,
    data: {
      task_run_id: 'task_receipt_guard',
      receipt: {
        schema: 'BlueprintHelper.ExecutionReceipt.v1',
        receipt_id: 'receipt_guard',
        cli_run_id: 'cli_guard',
        preview_id: 'preview_guard',
        task_run_id: 'task_receipt_guard',
        task_spec_hash: 'a'.repeat(64),
        task_plan_hash: 'b'.repeat(64),
        policy_hash: 'c'.repeat(64),
        verification_hash: 'd'.repeat(64),
        verification_status: 'pending_readback',
        status: 'applied',
        created_at: '2026-06-20T00:00:00.000Z',
        updated_at: '2026-06-20T00:00:00.000Z',
      },
    },
  };

  writeCliResult({
    cwd: process.cwd(),
    stdout: (text) => chunks.push(text),
  }, command, toolResult);

  const output = JSON.parse(chunks.join('')) as Record<string, any>;
  assert.equal(output.status, 'executed');
  assert.equal(output.receipt_id, 'receipt_guard');
  assert.equal(output.cli_run_id, 'cli_guard');
  assert.equal(output.preview_id, 'preview_guard');
  assert.equal(output.task_run_id, 'task_receipt_guard');
  assert.equal(output.verification_hash, 'd'.repeat(64));
  assert.equal(output.verification_status, 'pending_readback');
  assert.equal(output.receipt.receipt_id, 'receipt_guard');
  assert.equal(output.receipt.verification_hash, 'd'.repeat(64));
  assert.equal(output.receipt.verification_status, 'pending_readback');
});

test('CLI artifact write failure still emits parseable stdout JSON with receipt identity', () => {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bph-cli-artifact-fail-'));
  const artifactRootFile = path.join(tempDir, 'not-a-directory');
  fs.writeFileSync(artifactRootFile, 'occupied', 'utf8');
  const chunks: string[] = [];
  const command: CliCommand = {
    kind: 'task.execute',
    format: 'json',
    resultPolicyId: 'task.execute.default',
    artifactDir: artifactRootFile,
  };
  const toolResult: ToolResultBase = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'execute_task',
    trace_id: 'trace_artifact_fail',
    status: 'completed',
    modified: true,
    data: {
      task_run_id: 'task_artifact_fail',
      receipt: {
        schema: 'BlueprintHelper.ExecutionReceipt.v1',
        receipt_id: 'receipt_artifact_fail',
        task_run_id: 'task_artifact_fail',
        task_spec_hash: 'a'.repeat(64),
        status: 'applied',
        created_at: '2026-06-20T00:00:00.000Z',
        updated_at: '2026-06-20T00:00:00.000Z',
      },
    },
  };

  assert.doesNotThrow(() => {
    writeCliResult({
      cwd: process.cwd(),
      stdout: (text) => chunks.push(text),
    }, command, toolResult);
  });

  const output = JSON.parse(chunks.join('')) as Record<string, any>;
  assert.equal(output.ok, true);
  assert.equal(output.receipt_id, 'receipt_artifact_fail');
  assert.equal(output.artifact_warning.code, 'artifact_write_warning');
  assert.match(output.artifact_warning.message, /not-a-directory/);
});

test('CLI preview summary fails closed when issues omit passed flag', () => {
  const summary = buildCliSummary({
    command: {
      kind: 'task.preview',
      format: 'summary',
      resultPolicyId: 'task.preview.default',
    },
    toolResult: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'preview_task',
      trace_id: 'trace_missing_passed',
      status: 'dry_run',
      modified: false,
      data: {},
    },
    artifactRefs: {},
  });

  assert.equal(summary.ok, false);
  assert.equal(summary.status, 'preview_blocked');
  assert.equal(summary.error_code, 'preview_result_missing_passed');
  assert.equal(summary.message, 'Preview result is missing boolean data.passed.');
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

test('CLI projection preserves machine envelope fields even when selected fields omit them', () => {
  const suggestedRoute = {
    route_id: 'blueprint_class_settings.class_default',
    task_type: 'edit_blueprint_class_settings',
    property_path_hint: 'WeaponComponent.PrimaryWeapon',
  };
  const blockedBoundary = {
    boundary_id: 'component_tree_owned_scs_only',
    origin: 'native',
    blocked_operation: 'set_component_properties',
  };
  const output = buildCliError({
    operation: 'task.preview',
    status: 'preview_blocked',
    message: 'Preview blocked by runtime policy.',
    safe_next_action: 'inspect.preview.result',
    allowed_recovery_actions: ['task.preview.retry'],
    suggested_route: suggestedRoute,
    blocked_boundary: blockedBoundary,
    artifactRefs: {
      full_result: 'artifact://full',
    },
    fields: ['artifacts.full_result'],
    omitFields: [
      'ok',
      'operation',
      'status',
      'error_code',
      'message',
      'safe_next_action',
      'allowed_recovery_actions',
      'suggested_route',
      'blocked_boundary',
    ],
  });

  assert.equal(output.ok, false);
  assert.equal(output.operation, 'task.preview');
  assert.equal(output.status, 'preview_blocked');
  assert.equal(output.error_code, 'preview_blocked');
  assert.equal(output.message, 'Preview blocked by runtime policy.');
  assert.equal(output.safe_next_action, 'inspect.preview.result');
  assert.deepEqual(output.allowed_recovery_actions, ['task.preview.retry']);
  assert.deepEqual(output.suggested_route, suggestedRoute);
  assert.deepEqual(output.blocked_boundary, blockedBoundary);
  assert.deepEqual(output.artifacts, {
    full_result: 'artifact://full',
  });
});

test('CLI preview status fails closed when passed flag is missing', () => {
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'summary',
    resultPolicyId: 'task.preview.default',
  };
  const toolResult: ToolResultBase = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'preview_task',
    trace_id: 'trace_cli_missing_preview_passed',
    status: 'dry_run',
    modified: false,
    data: {
      issues: [],
    },
  };

  const summary = buildCliSummary({
    command,
    toolResult,
    artifactRefs: {},
  });

  assert.equal(summary.ok, false);
  assert.equal(summary.status, 'preview_blocked');
  assert.equal(summary.error_code, 'preview_result_missing_passed');
  assert.equal(summary.message, 'Preview result is missing boolean data.passed.');
});

test('CLI preview status fails closed when passed flag is non-boolean', () => {
  const command: CliCommand = {
    kind: 'task.preview',
    format: 'summary',
    resultPolicyId: 'task.preview.default',
  };
  const toolResult: ToolResultBase = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'preview_task',
    trace_id: 'trace_cli_non_boolean_preview_passed',
    status: 'dry_run',
    modified: false,
    data: {
      passed: 'yes',
      issues: [],
    },
  };

  const summary = buildCliSummary({
    command,
    toolResult,
    artifactRefs: {},
  });

  assert.equal(summary.ok, false);
  assert.equal(summary.status, 'preview_blocked');
  assert.equal(summary.error_code, 'preview_result_missing_passed');
  assert.equal(summary.message, 'Preview result is missing boolean data.passed.');
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
