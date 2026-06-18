import assert from 'node:assert/strict';
import test from 'node:test';

import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeGraphSpec(statement: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_graphwrite_pin_type_legacy',
    task_type: 'edit_blueprint_graph',
    feature_name: 'PinTypeLegacy',
    target: {
      target_type: 'blueprint',
      asset_path: '/Game/Test/BP_PinType.BP_PinType',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'ApplyPinTypeLegacy',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [statement],
        },
      }],
    },
  };
}

test('GraphWrite rejects legacy string pin_type token', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec({
      kind: 'create',
      create_operation: 'make_array',
      pin_type: 'string',
      args: {
        item: { kind: 'literal', value_type: 'string', value: 'A' },
      },
    }) as never),
    (error: unknown) => error instanceof TaskSpecCompileError
      && error.code === 'legacy_pin_type_token_unsupported',
  );
});

test('GraphWrite rejects legacy string key and value pin type tokens', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec({
      kind: 'create',
      create_operation: 'make_map',
      key_pin_type: 'string',
      value_pin_type: 'int',
    }) as never),
    (error: unknown) => error instanceof TaskSpecCompileError
      && error.code === 'legacy_pin_type_token_unsupported',
  );
});

test('GraphWrite rejects non-record result type proof evidence', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec({
      kind: 'let',
      name: 'SelectedValue',
      value: {
        kind: 'select',
        condition: { kind: 'literal', value_type: 'bool', value: true },
        options: [
          { kind: 'literal', value_type: 'string', value: 'A' },
          { kind: 'literal', value_type: 'string', value: 'B' },
        ],
        context_evidence: {
          'generic.select.result_type_proof': 'string',
        },
      },
    }) as never),
    (error: unknown) => error instanceof TaskSpecCompileError
      && error.code === 'legacy_pin_type_token_unsupported',
  );
});

test('GraphWrite rejects legacy string pin_type token in create expressions', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec({
      kind: 'set',
      target: 'CreatedValues',
      value: {
        kind: 'create',
        create_operation: 'make_array',
        pin_type: 'string',
        args: {
          item: { kind: 'literal', value_type: 'string', value: 'A' },
        },
      },
    }) as never),
    (error: unknown) => error instanceof TaskSpecCompileError
      && error.code === 'legacy_pin_type_token_unsupported'
      && error.issues[0]?.path === 'behavior.entries[0].body.statements[0].value.pin_type',
  );
});

test('GraphWrite rejects legacy string generic transform pin type evidence', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec({
      kind: 'convert',
      transform_operation: 'blueprint_autocast',
      target_class_path: '/Script/CoreUObject.String',
      context_evidence: {
        'generic.transform.operation': 'blueprint_autocast',
        'generic.transform.source_pin_type': 'name',
        'generic.transform.target_pin_type': { category: 'string' },
      },
      args: {
        value: { kind: 'literal', value_type: 'name', value: 'DisplayName' },
      },
    }) as never),
    (error: unknown) => error instanceof TaskSpecCompileError
      && error.code === 'legacy_pin_type_token_unsupported'
      && error.issues[0]?.path === 'behavior.entries[0].body.statements[0].context_evidence.generic.transform.source_pin_type',
  );
});

test('GraphWrite accepts structured BlueprintPinTypeSpec for create pin_type and select result proof', () => {
  const plan = compileTaskSpecToTaskPlan(makeGraphSpec({
    kind: 'create',
    create_operation: 'make_array',
    pin_type: { category: 'string' },
    args: {
      item: { kind: 'literal', value_type: 'string', value: 'A' },
    },
    context_evidence: {
      result_pin_type: { category: 'string', container_type: 'array' },
    },
  }) as never);

  const graphStep = plan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown>;
  assert.ok(graphStep);
  const write = graphStep.write as { ops: Array<{ body: { statements: Array<Record<string, unknown>> } }> };
  const createStatement = write.ops[0].body.statements[0];
  assert.deepEqual(createStatement.pin_type, { category: 'string' });
  assert.deepEqual((createStatement.context_evidence as Record<string, unknown>).result_pin_type, {
    category: 'string',
    container_type: 'array',
  });

  const selectPlan = compileTaskSpecToTaskPlan(makeGraphSpec({
    kind: 'let',
    name: 'SelectedValue',
    value: {
      kind: 'select',
      condition: { kind: 'literal', value_type: 'bool', value: true },
      options: [
        { kind: 'literal', value_type: 'string', value: 'A' },
        { kind: 'literal', value_type: 'string', value: 'B' },
      ],
      context_evidence: {
        'generic.select.result_type_proof': {
          pin_type: { category: 'string' },
        },
      },
    },
  }) as never);

  const selectGraphStep = selectPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown>;
  assert.ok(selectGraphStep);
  const selectWrite = selectGraphStep.write as { ops: Array<{ body: { statements: Array<Record<string, unknown>> } }> };
  const letStatement = selectWrite.ops[0].body.statements[0];
  const selectValue = letStatement.value as Record<string, unknown>;
  assert.deepEqual((selectValue.context_evidence as Record<string, unknown>)['generic.select.result_type_proof'], {
    pin_type: { category: 'string' },
  });
});
