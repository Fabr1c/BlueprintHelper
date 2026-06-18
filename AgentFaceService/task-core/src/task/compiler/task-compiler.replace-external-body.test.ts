import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

const externalBodyAnchor = {
  schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
  asset_path: '/Game/BP/BP_Door',
  graph_name: 'EventGraph',
  node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
  node_class: '/Script/BlueprintGraph.K2Node_CustomEvent',
  semantic_role: 'body_entry',
  fingerprint: 'body_entry_fp',
};

const replacementBody = {
  schema: 'BlueprintLogicSpec.v2',
  statements: [{
    kind: 'call',
    target: {
      kind: 'function',
      name: 'DoSomething',
    },
  }],
};

function makeReplaceExternalBodySpec(overrides: {
  scopePolicy?: Record<string, unknown>;
  externalReplace?: Record<string, unknown>;
  behavior?: Record<string, unknown>;
} = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_replace_external_body_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'ReplaceExternalBodyTs',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
      external_mutation_policy: {
        strategy: 'replace_external_body',
        allowed_mutations: ['body_replace'],
      },
      ...overrides.scopePolicy,
    },
    behavior: {
      graph_strategy: 'replace_external_body',
      external_replace: {
        scope: 'custom_event_body',
        anchor: externalBodyAnchor,
        body: replacementBody,
        expected_body_fingerprint: 'body_fp_before',
        require_full_dry_run: true,
        ...overrides.externalReplace,
      },
      ...overrides.behavior,
    },
  };
}

function compileReplaceExternalStep(overrides?: Parameters<typeof makeReplaceExternalBodySpec>[0]) {
  const taskPlan = compileTaskSpecToTaskPlan(makeReplaceExternalBodySpec(overrides) as never);
  const step = taskPlan.steps.find((candidate) => (
    (candidate as Record<string, unknown>).capability === 'graph_write'
  )) as Record<string, unknown> | undefined;
  assert.ok(step);
  return step;
}

test('replace_external_body lowers to external graph edit with exact mutation policy', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeReplaceExternalBodySpec() as never);
  const step = taskPlan.steps.find((candidate) => (
    (candidate as Record<string, unknown>).capability === 'graph_write'
  )) as Record<string, unknown> | undefined;
  assert.ok(step);
  const write = step.write as { strategy: string; ops: Array<Record<string, unknown>> };

  assert.deepEqual(taskPlan.execution_policy, {
    dry_run_mode: 'full',
    should_compile: true,
    should_save: true,
    review_baseline_dirty_asset_policy: 'block',
  });
  assert.equal(write.strategy, 'external_graph_edit');
  assert.equal(write.ops.length, 1);
  assert.equal(write.ops[0]?.op, 'replace_external_body');
  assert.equal(write.ops[0]?.replace_scope, 'custom_event_body');
  assert.deepEqual(write.ops[0]?.anchor, externalBodyAnchor);
  assert.equal(write.ops[0]?.expected_body_fingerprint, 'body_fp_before');
  assert.equal(write.ops[0]?.require_full_dry_run, true);
  assert.deepEqual(step.constraints, {
    allow_modify_user_nodes: false,
    ownership_scope: 'external_user_authored',
    external_mutation_policy: {
      strategy: 'replace_external_body',
      allowed_mutations: ['body_replace'],
    },
  });
});

test('replace_external_body accepts explicit event and function body scopes', () => {
  assert.equal(
    ((compileReplaceExternalStep({
      externalReplace: { scope: 'event_body' },
    }).write as { ops: Array<Record<string, unknown>> }).ops[0]?.replace_scope),
    'event_body',
  );
  assert.equal(
    ((compileReplaceExternalStep({
      externalReplace: { scope: 'function_body' },
    }).write as { ops: Array<Record<string, unknown>> }).ops[0]?.replace_scope),
    'function_body',
  );
});

test('replace_external_body rejects broad policy, whole graph scope, missing fingerprint, and disabled full dry-run requirement', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeReplaceExternalBodySpec({
      scopePolicy: {
        allow_modify_user_nodes: true,
      },
    }) as never),
    /unsupported_scope_policy/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makeReplaceExternalBodySpec({
      externalReplace: {
        scope: 'graph',
      },
    }) as never),
    /not supported|custom_event_body|event_body|function_body/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makeReplaceExternalBodySpec({
      externalReplace: {
        expected_body_fingerprint: undefined,
      },
    }) as never),
    /expected_body_fingerprint/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makeReplaceExternalBodySpec({
      externalReplace: {
        require_full_dry_run: false,
      },
    }) as never),
    /require_full_dry_run/,
  );

});

test('replace_external_body rejects display-name selector and owned replace mixed into external strategy', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeReplaceExternalBodySpec({
      externalReplace: {
        anchor: undefined,
        selector: {
          kind: 'custom_event',
          name: 'OnOpened',
        },
      },
    }) as never),
    /anchor/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makeReplaceExternalBodySpec({
      behavior: {
        replace: {
          scope: 'custom_event_body',
          selector: { kind: 'custom_event', name: 'OnOpened' },
          body: replacementBody,
        },
      },
    }) as never),
    /replace does not belong|external_replace/,
  );
});
