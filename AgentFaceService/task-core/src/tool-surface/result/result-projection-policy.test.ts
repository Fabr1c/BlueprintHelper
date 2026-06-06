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
import { resolveResultProjectionPolicy } from './result-projection-registry.js';

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
    policy: resolveResultProjectionPolicy({ commandKind: 'task.execute' }),
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
  const debugArtifact = buildCliDebugArtifactSource({
    command_kind: 'task.execute',
    format: 'full',
    expert: true,
    tool_result: makePreviewBlockedResult(),
    policy: GENERIC_RESULT_PROJECTION_POLICY,
  });

  assert.equal((debugArtifact?.bridge_result as Record<string, unknown>)?.schema, 'BlueprintHelper.ToolResult.v1');
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

test('projectToolResultForCli preserves ExternalGraphAnchor schema', () => {
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
  const anchor = nodes[0]?.['external_anchor'] as Record<string, unknown>;
  assert.equal(anchor['schema'], 'BlueprintHelper.ExternalGraphAnchor.v1');
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
