import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import type { ToolResultBase } from '../../result/tool-result.js';
import {
  buildCliDebugArtifactSource,
  compactTaskPlanForArtifact,
  GENERIC_RESULT_PROJECTION_POLICY,
  projectMetricsReportDataForCli,
  projectToolResultForCli,
} from './result-projection-policy.js';
import { getBuiltinResultProjectionPolicy } from './result-projection-registry.js';

const FORBIDDEN_LOCAL_PRUNING_PATTERNS: readonly [string, RegExp][] = [
  ['schema local key equality', /key\s*===\s*['"]schema['"]/],
  ['trace local key equality', /key\s*===\s*['"]trace_id['"]/],
  ['debug local key equality', /key\s*===\s*['"]debug['"]/],
  ['bridge-result local key equality', /key\s*===\s*['"]bridge_result['"]/],
  ['execution-policy local key equality', /key\s*===\s*['"]execution_policy['"]/],
  ['scope-policy local key equality', /key\s*===\s*['"]scope_policy['"]/],
  ['validation flag local key equality', /key\s*===\s*['"]should_(compile|save)['"]/],
  ['task duplicate local key equality', /key\s*===\s*['"](task_run_id|target_assets)['"]/],
  ['taskPlan extra local key equality', /next\[['"]taskPlan['"]\]/],
];

test('ResultProjectionPolicy owns default pruning instead of local key checks', () => {
  const source = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'tool-surface', 'result', 'result-projection-policy.ts'),
    'utf8',
  );

  for (const [label, pattern] of FORBIDDEN_LOCAL_PRUNING_PATTERNS) {
    assert.equal(pattern.test(source), false, `${label} must be declared by policy data`);
  }
});

test('projectToolResultForCli summary omits trace debug schema and bridge_result', () => {
  const projected = projectToolResultForCli({
    command_kind: 'task.preview',
    format: 'summary',
    tool_result: makePreviewBlockedResult(),
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });
  const serialized = JSON.stringify(projected);

  assert.equal(serialized.includes('trace_id'), false);
  assert.equal(serialized.includes('bridge_result'), false);
  assert.equal(serialized.includes('BlueprintHelper.ToolResult.v1'), false);
});

test('projectToolResultForCli json keeps agent-facing data and artifact refs only', () => {
  const projected = projectToolResultForCli({
    command_kind: 'task.preview',
    format: 'json',
    tool_result: makePreviewBlockedResult(),
    artifact_refs: { full_result: 'result.json' },
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });

  assert.equal(JSON.stringify(projected).includes('debug'), false);
  assert.equal((projected.tool_result.data as Record<string, unknown>)['passed'], false);
});

test('projectToolResultForCli ordinary output omits CLI wrapper and debug-only bridge payloads recursively', () => {
  const projected = projectToolResultForCli({
    command_kind: 'task.preview',
    format: 'full',
    tool_result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'projection_wrapper_contract',
      trace_id: 'trace_projection_wrapper_contract',
      status: 'completed',
      modified: false,
      data: {
        schema: 'BlueprintHelper.CliResult.v1',
        toolResult: {
          schema: 'BlueprintHelper.ToolResult.v1',
          trace_id: 'trace_nested_projection_wrapper_contract',
          bridge_result: {
            schema: 'BlueprintHelper.ToolResult.v1',
          },
          debug: {
            bridge_result: {
              schema: 'BlueprintHelper.ToolResult.v1',
            },
          },
        },
        visible: true,
      },
      debug: {
        bridge_result: {
          schema: 'BlueprintHelper.ToolResult.v1',
        },
      },
    } as ToolResultBase,
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });
  const serialized = JSON.stringify(projected);

  assert.equal(serialized.includes('BlueprintHelper.CliResult.v1'), false);
  assert.equal(serialized.includes('BlueprintHelper.ToolResult.v1'), false);
  assert.equal(serialized.includes('trace_projection_wrapper_contract'), false);
  assert.equal(serialized.includes('trace_nested_projection_wrapper_contract'), false);
  assert.equal(serialized.includes('bridge_result'), false);
  assert.equal(serialized.includes('debug'), false);
  assert.equal((projected.tool_result.data as Record<string, unknown>)['visible'], true);
});

test('projectToolResultForCli full omits policy-only execution fields by default', () => {
  const taskPlan = {
    execution_policy: { dry_run_mode: 'full', should_compile: true },
    scope_policy: { graph_name: 'EventGraph' },
    validation: { should_compile: true, should_save: false, compile_success: false },
  };

  const compacted = compactTaskPlanForArtifact(taskPlan);
  const serialized = JSON.stringify(compacted);

  assert.equal(serialized.includes('execution_policy'), false);
  assert.equal(serialized.includes('scope_policy'), false);
  assert.equal(serialized.includes('should_compile'), false);
  assert.equal(serialized.includes('compile_success'), true);
});

test('projectToolResultForCli full removes nested execute task duplication', () => {
  const projected = projectToolResultForCli({
    command_kind: 'task.execute',
    format: 'full',
    tool_result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'execute_task',
      trace_id: 'trace_execute_projection',
      status: 'completed',
      modified: true,
      data: {
        task_run_id: 'task_cli_001',
        task: {
          task_run_id: 'task_cli_001',
          target_assets: ['/Game/BP_Player'],
          applied_steps: 1,
        },
      },
    } as ToolResultBase,
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });

  const data = projected.tool_result.data as Record<string, unknown>;
  const task = data['task'] as Record<string, unknown>;

  assert.equal(data['task_run_id'], 'task_cli_001');
  assert.equal('task_run_id' in task, false);
  assert.equal('target_assets' in task, false);
  assert.equal(task['applied_steps'], 1);
});

test('task execute projection policy omits execute preview_id from returned data', () => {
  const projected = projectToolResultForCli({
    command_kind: 'task.execute',
    format: 'full',
    tool_result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'execute_task',
      trace_id: 'trace_execute_preview_projection',
      status: 'completed',
      modified: true,
      data: {
        preview_id: 'preview_should_not_return',
        task_run_id: 'task_cli_001',
      },
    } as ToolResultBase,
    policy: getBuiltinResultProjectionPolicy('task.execute.default'),
  });

  const data = projected.tool_result.data as Record<string, unknown>;
  assert.equal('preview_id' in data, false);
  assert.equal(data['task_run_id'], 'task_cli_001');
});

test('metrics markdown output data is compacted by result projection policy helper', () => {
  const projected = projectMetricsReportDataForCli({
    schema: 'BlueprintHelper.MetricsReport.v1',
    kind: 'tool-usage',
    window: '7d',
    summary: { ok: true },
    rows: [{ tool: 'blueprinthelper_read_context' }],
    markdown_report_path: 'metrics/report.md',
  }, 'markdown');

  assert.deepEqual(projected, {
    schema: 'BlueprintHelper.MetricsReport.v1',
    kind: 'tool-usage',
    window: '7d',
    summary: { ok: true },
    markdown_report_path: 'metrics/report.md',
  });
});

test('projectToolResultForCli expert produces debug artifact source with bridge_result', () => {
  const projected = projectToolResultForCli({
    command_kind: 'task.execute',
    format: 'full',
    expert: true,
    tool_result: makePreviewBlockedResult(),
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });
  const debugArtifact = buildCliDebugArtifactSource({
    command_kind: 'task.execute',
    format: 'full',
    expert: true,
    tool_result: makePreviewBlockedResult(),
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });
  const serializedStdout = JSON.stringify(projected);
  const artifactToolResult = debugArtifact?.tool_result as Record<string, unknown>;

  assert.equal(serializedStdout.includes('bridge_result'), false);
  assert.equal(serializedStdout.includes('BlueprintHelper.ToolResult.v1'), false);
  assert.equal(debugArtifact?.schema, 'BlueprintHelper.CliDebugResult.v1');
  assert.equal(artifactToolResult?.schema, 'BlueprintHelper.ToolResult.v1');
  assert.equal(artifactToolResult?.trace_id, 'trace_projection');
  assert.equal(artifactToolResult?.operation, 'preview_task');
  assert.equal((debugArtifact?.bridge_result as Record<string, unknown>)?.schema, 'BlueprintHelper.ToolResult.v1');
});

test('projectToolResultForCli keeps actionable error fields for agents', () => {
  const projected = projectToolResultForCli({
    command_kind: 'task.execute',
    format: 'json',
    tool_result: {
      ok: false,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'projection_error_contract',
      trace_id: 'trace_projection_error_contract',
      status: 'failed',
      modified: false,
      error: {
        code: 'target_blueprint_not_found',
        stage: 'resolve_target',
        message: 'Target Blueprint was not found.',
        retryable: false,
        rollback_result: 'not_needed',
        safe_next_action: 'Run ReadContext asset discovery and retry with an existing asset path.',
      },
    } as unknown as ToolResultBase,
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });
  const error = projected.tool_result.error as Record<string, unknown>;

  assert.equal(error['code'], 'target_blueprint_not_found');
  assert.equal(error['stage'], 'resolve_target');
  assert.equal(error['message'], 'Target Blueprint was not found.');
  assert.equal(error['safe_next_action'], 'Run ReadContext asset discovery and retry with an existing asset path.');
  assert.equal(JSON.stringify(projected).includes('trace_projection_error_contract'), false);
});

test('projectToolResultForCli keeps review baseline dirty compact error fields', () => {
  const projected = projectToolResultForCli({
    command_kind: 'task.preview',
    format: 'json',
    tool_result: {
      ok: false,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'projection_dirty_baseline_contract',
      trace_id: 'trace_projection_dirty_baseline_contract',
      status: 'failed',
      modified: false,
      error: {
        code: 'review_baseline_dirty_target_assets',
        category: 'runtime_state_error',
        stage: 'preflight',
        message: 'Review baseline requires clean target assets before archive.',
        retryable: false,
        rollback_result: 'not_needed',
        dirty_state: 'unknown_dirty_origin',
        dirty_assets: ['/Game/BP_Dirty'],
        safe_next_action: 'ask_user_to_inspect_dirty_assets_before_retry',
        allowed_recovery_actions: ['user_resolve_then_retry'],
        evidence_refs: ['review://evidence/compact'],
      },
    } as unknown as ToolResultBase,
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });
  const error = projected.tool_result.error as Record<string, unknown>;

  assert.equal(error['code'], 'review_baseline_dirty_target_assets');
  assert.equal(error['category'], 'runtime_state_error');
  assert.equal(error['dirty_state'], 'unknown_dirty_origin');
  assert.deepEqual(error['dirty_assets'], ['/Game/BP_Dirty']);
  assert.equal(error['safe_next_action'], 'ask_user_to_inspect_dirty_assets_before_retry');
  assert.deepEqual(error['allowed_recovery_actions'], ['user_resolve_then_retry']);
  assert.deepEqual(error['evidence_refs'], ['review://evidence/compact']);
});

test('projectToolResultForCli preserves connectivity blocker issue summary', () => {
  const projected = projectToolResultForCli({
    command_kind: 'task.preview',
    format: 'json',
    tool_result: makePreviewBlockedResult(),
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });
  const issues = ((projected.tool_result.data as Record<string, unknown>).issues as Record<string, unknown>[]);

  assert.deepEqual(issues.map((issue) => ({
    code: issue.code,
    path: issue.path,
    message: issue.message,
  })), [{
    code: 'unconsumed_pure_data_node',
    path: 'logic_spec',
    message: 'PureData producer is not consumed.',
  }]);
});

test('projectToolResultForCli omits expanded anchors but keeps compact anchor fields', () => {
  const projected = projectToolResultForCli({
    command_kind: 'read.context',
    format: 'json',
    tool_result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'read_context',
      trace_id: 'trace_external_anchor_projection',
      status: 'completed',
      modified: false,
      data: {
        payload: {
          schema: 'LogicJson.v1',
          logic: {
            nodes: [{
              anchor_type: 'external_node',
              kind: 'node',
              label: 'Set FocusActor',
              anchor_ref: 'xnode:v1:aaaaaaaa#anchorfp',
              external_anchor: {
                schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
                node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                fingerprint: 'anchorfp',
              },
            }],
          },
        },
      },
    } as ToolResultBase,
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });

  const data = projected.tool_result.data as Record<string, unknown>;
  const payload = data['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicJson.v1');
  const logic = payload['logic'] as Record<string, unknown>;
  const nodes = logic['nodes'] as Record<string, unknown>[];
  assert.equal(nodes[0]?.['external_anchor'], undefined);
  assert.equal(nodes[0]?.['anchor_type'], 'external_node');
  assert.equal(nodes[0]?.['anchor_ref'], 'xnode:v1:aaaaaaaa#anchorfp');
});

test('buildCliDebugArtifactSource preserves expanded anchors for expert artifact', () => {
  const debugArtifact = buildCliDebugArtifactSource({
    command_kind: 'read.context',
    format: 'json',
    expert: true,
    tool_result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'read_context',
      trace_id: 'trace_external_anchor_debug_projection',
      status: 'completed',
      modified: false,
      data: {
        payload: {
          schema: 'LogicJson.v1',
          logic: {
            nodes: [{
              external_anchor: {
                schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
                node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                fingerprint: 'anchorfp',
              },
            }],
          },
        },
      },
      debug: {
        anchors: [{
          schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
          node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
          fingerprint: 'anchorfp',
        }],
      },
    } as ToolResultBase,
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });

  const debug = debugArtifact?.debug as Record<string, unknown>;
  const anchors = debug['anchors'] as Record<string, unknown>[];
  assert.equal(anchors[0]?.['schema'], 'BlueprintHelper.ExternalGraphAnchor.v1');
  assert.equal(anchors[0]?.['node_guid'], 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');
});

test('projectToolResultForCli applies policy omit_by_default recursively', () => {
  const projected = projectToolResultForCli({
    command_kind: 'tool.invoke',
    format: 'full',
    tool_result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'policy_projection',
      trace_id: 'trace_policy_projection',
      status: 'completed',
      modified: false,
      data: {
        schema: 'LogicFlow.v1',
        keep: true,
        policy_hidden: 'drop',
        nested: {
          policy_hidden: 'drop nested',
          keep_nested: true,
        },
      },
    } as ToolResultBase,
    extra: {
      policy_hidden: 'drop extra',
      keep_extra: true,
    },
    policy: {
      ...GENERIC_RESULT_PROJECTION_POLICY,
      policy_id: 'test.policy.omit',
      omit_by_default: [...GENERIC_RESULT_PROJECTION_POLICY.omit_by_default, 'policy_hidden'],
    },
  });

  const serialized = JSON.stringify(projected);

  assert.equal(serialized.includes('policy_hidden'), false);
  assert.equal(serialized.includes('LogicFlow.v1'), true);
  assert.equal(serialized.includes('keep_nested'), true);
  assert.equal(serialized.includes('keep_extra'), true);
});

function makePreviewBlockedResult(): ToolResultBase {
  return {
    ok: false,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'preview_task',
    trace_id: 'trace_projection',
    status: 'failed',
    modified: false,
    data: {
      passed: false,
      issues: [{
        code: 'unconsumed_pure_data_node',
        path: 'logic_spec',
        message: 'PureData producer is not consumed.',
        hidden: 'safe to omit later',
      }],
      bridge_result: {
        schema: 'BlueprintHelper.ToolResult.v1',
        ok: true,
      },
    },
    debug: {
      bridge_result: {
        schema: 'BlueprintHelper.ToolResult.v1',
        ok: true,
      },
    },
  } as ToolResultBase;
}

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
}
