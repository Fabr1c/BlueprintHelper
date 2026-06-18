import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeReplaceSpec(replace: Record<string, unknown>, scopePolicy?: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_replace_scope_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'ReplaceScopeFeatureTs',
    target: {
      asset_path: '/Game/BP/BP_ReplaceScope',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
      ...scopePolicy,
    },
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace,
    },
  };
}

function compileReplaceOp(replace: Record<string, unknown>) {
  const graphWriteStep = compileReplaceStep(replace);
  const write = graphWriteStep.write as { ops: Array<Record<string, unknown>> };
  assert.equal(write.ops.length, 1);
  return write.ops[0];
}

function compileReplaceStep(replace: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeReplaceSpec(replace) as never);
  const graphWriteStep = taskPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown> | undefined;
  assert.ok(graphWriteStep);
  return graphWriteStep;
}

test('replace graph scope compiles to graph replace instead of an event or block scope', () => {
  const op = compileReplaceOp({
    scope: 'graph',
    selector: {
      kind: 'graph',
      graph_id: 'EventGraph',
    },
    body: {
      schema: 'BlueprintLogicSpec.v1',
      statements: [{
        kind: 'call',
        target: 'PrintString',
        args: {
          InString: { kind: 'literal', value_type: 'string', value: 'replace graph' },
        },
      }],
    },
  });

  assert.equal(op.op, 'replace_body');
  assert.equal(op.replace_scope, 'graph');
  assert.deepEqual(op.selector, { graph_id: 'EventGraph' });
});

test('replace event and function scopes keep distinct selectors', () => {
  const eventOp = compileReplaceOp({
    scope: 'custom_event_body',
    selector: {
      kind: 'custom_event',
      name: 'ReplaceEvent',
    },
    body: {
      schema: 'BlueprintLogicSpec.v1',
      statements: [{ kind: 'call', target: 'PrintString' }],
    },
  });
  const functionOp = compileReplaceOp({
    scope: 'function_body',
    selector: {
      kind: 'function',
      name: 'ReplaceFunction',
    },
    body: {
      schema: 'BlueprintLogicSpec.v1',
      statements: [{ kind: 'call', target: 'PrintString' }],
    },
  });

  assert.equal(eventOp.replace_scope, 'custom_event_body');
  assert.deepEqual(eventOp.selector, { entry_name: 'ReplaceEvent' });
  assert.equal(functionOp.replace_scope, 'function_body');
  assert.deepEqual(functionOp.selector, { function_name: 'ReplaceFunction' });
});

test('replace function scope targets the selected function graph', () => {
  const step = compileReplaceStep({
    scope: 'function_body',
    selector: {
      kind: 'function',
      name: 'ReplaceFunction',
    },
    body: {
      schema: 'BlueprintLogicSpec.v1',
      statements: [{ kind: 'call', target: 'PrintString' }],
    },
  });

  assert.deepEqual(step.target, {
    asset_path: '/Game/BP/BP_ReplaceScope',
    graph: 'ReplaceFunction',
  });
});

test('owned replace graph write step carries owned-only constraints', () => {
  const step = compileReplaceStep({
    scope: 'custom_event_body',
    selector: {
      kind: 'custom_event',
      name: 'ReplaceEvent',
    },
    body: {
      schema: 'BlueprintLogicSpec.v1',
      statements: [{ kind: 'call', target: 'PrintString' }],
    },
  });

  assert.deepEqual(step.constraints, {
    allow_modify_user_nodes: false,
    ownership_scope: 'blueprinthelper_owned',
  });
});

test('owned replace rejects broad user node mutation policy', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeReplaceSpec({
      scope: 'custom_event_body',
      selector: {
        kind: 'custom_event',
        name: 'ReplaceEvent',
      },
      body: {
        schema: 'BlueprintLogicSpec.v1',
        statements: [{ kind: 'call', target: 'PrintString' }],
      },
    }, {
      allow_modify_user_nodes: true,
    }) as never),
    /unsupported_scope_policy/,
  );
});
