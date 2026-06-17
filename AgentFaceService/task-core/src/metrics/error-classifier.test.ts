import assert from 'node:assert/strict';
import test from 'node:test';

import { classifyMetricsError } from './error-classifier.js';

test('classifyMetricsError honors category_hint before code-based inference', () => {
  const result = classifyMetricsError({
    category_hint: 'runtime_state_error',
    code: 'unsupported_task_type',
    issues: [
      {
        code: 'unsupported_task_type',
        path: 'task_type',
        message: 'Unsupported task type.',
      },
    ],
  });

  assert.deepEqual(result, {
    category: 'runtime_state_error',
    code: 'unsupported_task_type',
    issue_path: 'task_type',
  });
});

test('classifyMetricsError honors error_category_hint before code-based inference', () => {
  const result = classifyMetricsError({
    error_category_hint: 'parameter_error',
    code: 'unsupported_scope_policy',
  });

  assert.deepEqual(result, {
    category: 'parameter_error',
    code: 'unsupported_scope_policy',
  });
});

test('classifyMetricsError honors error_category before code-based inference', () => {
  const result = classifyMetricsError({
    error_category: 'context_error',
    issue_code: 'unsupported_scope_policy',
  });

  assert.deepEqual(result, {
    category: 'context_error',
    code: 'unsupported_scope_policy',
  });
});

test('classifyMetricsError prefers live error_category over legacy category_hint', () => {
  const result = classifyMetricsError({
    category_hint: 'runtime_state_error',
    error_category: 'parameter_error',
    code: 'unsupported_scope_policy',
  });

  assert.deepEqual(result, {
    category: 'parameter_error',
    code: 'unsupported_scope_policy',
  });
});

test('classifyMetricsError maps known capability boundary codes', () => {
  const result = classifyMetricsError({
    error_code: 'operation_not_supported',
    issues: [
      {
        code: 'operation_not_supported',
        path: 'behavior.steps[0]',
        message: 'Operation is not supported.',
      },
    ],
  });

  assert.deepEqual(result, {
    category: 'capability_boundary',
    code: 'operation_not_supported',
    issue_path: 'behavior.steps[0]',
  });
});

test('classifyMetricsError maps expanded capability boundary strategy codes', () => {
  for (const code of [
    'unsupported_graph_write_strategy',
    'unsupported_graph_write_op',
    'unsupported_variable_strategy',
    'unsupported_variable_op',
    'unsupported_asset_factory_strategy',
    'unsupported_component_strategy',
    'unsupported_widget_strategy',
    'unsupported_data_table_strategy',
    'unsupported_object_property_strategy',
    'unsupported_signature_strategy',
    'unsupported_taskplan_operation',
    'unsupported_graph_write_patch',
    'unsupported_graph_write_merge',
    'unsupported_graph_write_anchor',
    'unsupported_external_graph_write_merge',
    'unsupported_external_graph_anchor',
  ]) {
    const result = classifyMetricsError({ issue_code: code });

    assert.deepEqual(result, {
      category: 'capability_boundary',
      code,
    });
  }
});

test('classifyMetricsError maps expanded parameter error codes', () => {
  for (const code of [
    'missing_required_logic_body',
    'logic_spec_required',
    'missing_property_value',
    'missing_target_asset_path',
    'missing_graph_name',
    'missing_required_field',
    'invalid_read_context_payload',
  ]) {
    const result = classifyMetricsError({ issue_code: code });

    assert.deepEqual(result, {
      category: 'parameter_error',
      code,
    });
  }
});

test('classifyMetricsError reads nested ToolResultBase error shapes', () => {
  const result = classifyMetricsError({
    ok: false,
    status: 'failed',
    error: {
      code: 'unsupported_taskplan_operation',
      issues: [
        {
          code: 'unsupported_taskplan_operation',
          path: 'steps[0].operation',
          message: 'Only append_blueprint_graph lowering adapter TaskPlan steps are supported.',
        },
      ],
    },
  });

  assert.deepEqual(result, {
    category: 'capability_boundary',
    code: 'unsupported_taskplan_operation',
    issue_path: 'steps[0].operation',
  });
});

test('classifyMetricsError prefers nested ToolResultBase error code over failed status', () => {
  const result = classifyMetricsError({
    ok: false,
    status: 'failed',
    error: {
      code: 'malformed_json',
      stage: 'parse_input',
      message: 'Failed to parse --json input as JSON.',
    },
  });

  assert.deepEqual(result, {
    category: 'parameter_error',
    code: 'malformed_json',
  });
});

test('classifyMetricsError maps TaskSpec semantic authoring failures to parameter errors', () => {
  const result = classifyMetricsError({
    ok: false,
    status: 'failed',
    error: {
      code: 'taskspec_semantic_invalid',
      stage: 'preflight',
      message: 'GraphWrite connectivity static preflight failed: unconsumed_pure_data_node.',
      issues: [{
        code: 'unconsumed_pure_data_node',
        path: 'behavior.entries[0].body.statements[0]',
        message: 'PureData producer is not consumed.',
      }],
    },
  });

  assert.deepEqual(result, {
    category: 'parameter_error',
    code: 'taskspec_semantic_invalid',
    issue_path: 'behavior.entries[0].body.statements[0]',
  });
});

test('classifyMetricsError reads read_context handler failure shapes', () => {
  const result = classifyMetricsError({
    ok: false,
    error: {
      code: 'invalid_read_context_payload',
      stage: 'bridge',
      message: 'Bridge payload is missing result.',
    },
  });

  assert.deepEqual(result, {
    category: 'parameter_error',
    code: 'invalid_read_context_payload',
  });
});

test('classifyMetricsError infers runtime state errors from top-level status', () => {
  const result = classifyMetricsError({
    status: 'bridge_unavailable',
  });

  assert.deepEqual(result, {
    category: 'runtime_state_error',
    code: 'bridge_unavailable',
  });
});

test('classifyMetricsError maps review baseline dirty to runtime state error', () => {
  const result = classifyMetricsError({
    ok: false,
    error: {
      code: 'review_baseline_dirty_target_assets',
      category: 'runtime_state_error',
      stage: 'baseline_preflight',
      dirty_state: 'dirty_with_open_review',
      safe_next_action: 'review.reject_or_accept_pending_changes_then_retry',
    },
  });

  assert.deepEqual(result, {
    category: 'runtime_state_error',
    code: 'review_baseline_dirty_target_assets',
  });
});

test('classifyMetricsError infers capability boundary errors from top-level issue_code', () => {
  const result = classifyMetricsError({
    issue_code: 'unsupported_scope_policy',
  });

  assert.deepEqual(result, {
    category: 'capability_boundary',
    code: 'unsupported_scope_policy',
  });
});

test('classifyMetricsError treats target-scoped issues as context errors', () => {
  const result = classifyMetricsError({
    issues: [
      {
        code: 'asset_not_found',
        path: 'target.asset_path',
        message: 'Asset does not exist.',
      },
    ],
  });

  assert.deepEqual(result, {
    category: 'context_error',
    code: 'asset_not_found',
    issue_path: 'target.asset_path',
  });
});

test('classifyMetricsError maps context_stale to context_error', () => {
  const result = classifyMetricsError({
    issue_code: 'context_stale',
    path: 'preview_token.context_revision',
  });

  assert.deepEqual(result, {
    category: 'context_error',
    code: 'context_stale',
    issue_path: 'preview_token.context_revision',
  });
});

test('classifyMetricsError falls back to unknown when no rule matches', () => {
  const result = classifyMetricsError({
    error_code: 'totally_new_error',
    issues: [
      {
        code: 'totally_new_error',
        path: 'behavior.unknown',
        message: 'Unexpected issue.',
      },
    ],
  });

  assert.deepEqual(result, {
    category: 'unknown',
    code: 'totally_new_error',
    issue_path: 'behavior.unknown',
  });
});
