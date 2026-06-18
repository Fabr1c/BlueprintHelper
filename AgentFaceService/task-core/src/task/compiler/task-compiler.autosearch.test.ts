import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeSpec(statement: Record<string, unknown>, policy: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    feature_name: 'AutoSearchCompilerSpec',
    target: { asset_path: '/Game/BP_A.BP_A' },
    scope_policy: { graph_name: 'EventGraph', allow_modify_user_nodes: false },
    graph_write_policy: policy,
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        scope: 'graph',
        selector: { kind: 'graph', graph_id: 'EventGraph' },
        body: {
          schema: 'BlueprintLogicSpec.v2',
          statements: [statement],
        },
      },
    },
  };
}

test('compiler passes task-level auto_search policy to graph_write step', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeSpec(
    { kind: 'call', target: 'Tween Float', resolution_policy: 'auto_search' },
    {
      auto_search: {
        mode: 'on_preview_resolution_failure',
        max_candidates_per_statement: 3,
        max_auto_search_statements: 16,
        max_total_auto_search_ms: 120,
        detail_level: 'short',
        debug_passthrough: 'do_not_lower',
      },
    },
  ) as never);

  const graphWriteStep = taskPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, any>;
  assert.equal(graphWriteStep.write.auto_search_policy.mode, 'on_preview_resolution_failure');
  assert.equal(graphWriteStep.write.auto_search_policy.max_candidates_per_statement, 3);
  assert.equal(graphWriteStep.write.auto_search_policy.debug_passthrough, undefined);
});

test('compiler preserves call resolution_policy and action_selection in logic_spec', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeSpec({
    kind: 'call',
    target: 'Tween Float',
    resolution_policy: 'auto_search',
    action_selection: { candidate_id: 'preview:gw_01:s_tween_float:001' },
  }) as never);

  const graphWriteStep = taskPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, any>;
  const statement = graphWriteStep.write.ops[0].logic_spec.statements[0];
  assert.equal(statement.resolution_policy, 'auto_search');
  assert.equal(statement.action_selection.candidate_id, 'preview:gw_01:s_tween_float:001');
});
