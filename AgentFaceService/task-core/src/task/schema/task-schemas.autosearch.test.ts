import { strict as assert } from 'node:assert';
import test from 'node:test';

import { TaskSpecSchema } from './task-schemas.js';

function makeGraphSpec(overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    feature_name: 'AutoSearchPolicySpec',
    target: { asset_path: '/Game/BP_A.BP_A' },
    scope_policy: { graph_name: 'EventGraph', allow_modify_user_nodes: false },
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        scope: 'graph',
        selector: { kind: 'graph', graph_id: 'EventGraph' },
        body: {
          schema: 'BlueprintLogicSpec.v2',
          statements: [
            { kind: 'call', target: 'Tween Float', resolution_policy: 'auto_search' },
          ],
        },
      },
    },
    ...overrides,
  };
}

test('GraphWrite TaskSpec accepts task-level auto_search preview policy', () => {
  const parsed = TaskSpecSchema.parse(makeGraphSpec({
    graph_write_policy: {
      auto_search: {
        mode: 'on_preview_resolution_failure',
        max_candidates_per_statement: 3,
        max_auto_search_statements: 16,
        max_total_auto_search_ms: 120,
        detail_level: 'short',
      },
    },
  })) as Record<string, unknown>;

  const policy = parsed['graph_write_policy'] as Record<string, unknown>;
  assert.equal((policy['auto_search'] as Record<string, unknown>)['mode'], 'on_preview_resolution_failure');
});

test('GraphWrite TaskSpec rejects unknown auto_search policy fields', () => {
  const result = TaskSpecSchema.safeParse(makeGraphSpec({
    graph_write_policy: {
      auto_search: {
        mode: 'on_preview_resolution_failure',
        debug_passthrough: 'do_not_accept',
      },
    },
  }));

  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /debug_passthrough/);
  }
});

test('GraphWrite TaskSpec rejects Agent-facing kind=action', () => {
  const spec = makeGraphSpec();
  const behavior = spec.behavior as Record<string, any>;
  behavior.replace.body.statements = [{ kind: 'action', target: 'Tween Float' }];

  const result = TaskSpecSchema.safeParse(spec);
  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /kind=\\"action\\" is not an Agent-facing GraphWrite statement kind/);
  }
});

test('GraphWrite TaskSpec accepts call action_selection candidate id', () => {
  const spec = makeGraphSpec();
  const behavior = spec.behavior as Record<string, any>;
  behavior.replace.body.statements = [{
    kind: 'call',
    target: 'Tween Float',
    resolution_policy: 'auto_search',
    action_selection: { candidate_id: 'preview:gw_01:s_tween_float:001' },
  }];

  const parsed = TaskSpecSchema.parse(spec) as Record<string, any>;
  assert.equal(
    parsed.behavior.replace.body.statements[0].action_selection.candidate_id,
    'preview:gw_01:s_tween_float:001',
  );
});

test('GraphWrite TaskSpec rejects malformed action_selection candidate id', () => {
  const spec = makeGraphSpec();
  const behavior = spec.behavior as Record<string, any>;
  behavior.replace.body.statements = [{
    kind: 'call',
    target: 'Tween Float',
    resolution_policy: 'auto_search',
    action_selection: { candidate_id: 'gw_01:s_tween_float:001' },
  }];

  const result = TaskSpecSchema.safeParse(spec);
  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /candidate_id/);
  }
});

test('GraphWrite TaskSpec rejects action_selection on non-call statements', () => {
  const spec = makeGraphSpec();
  const behavior = spec.behavior as Record<string, any>;
  behavior.replace.body.statements = [{
    kind: 'set',
    target: 'Speed',
    action_selection: { candidate_id: 'preview:gw_01:s_speed:001' },
    value: { kind: 'literal', value_type: 'float', value: 1.0 },
  }];

  const result = TaskSpecSchema.safeParse(spec);
  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /action_selection is supported only on kind=\\"call\\" statements/);
  }
});

test('GraphWrite TaskSpec rejects resolution_policy on non-call statements', () => {
  const spec = makeGraphSpec();
  const behavior = spec.behavior as Record<string, any>;
  behavior.replace.body.statements = [{
    kind: 'let',
    name: 'Speed',
    resolution_policy: 'auto_search',
    value: { kind: 'literal', value_type: 'float', value: 1.0 },
  }];

  const result = TaskSpecSchema.safeParse(spec);
  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /resolution_policy is supported only on kind=\\"call\\" statements/);
  }
});
