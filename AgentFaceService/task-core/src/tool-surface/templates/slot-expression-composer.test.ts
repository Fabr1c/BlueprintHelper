import assert from 'node:assert/strict';
import test from 'node:test';

import { listTaskSpecTemplateQuickAccess } from './taskspec-template-index.js';
import { composeSlotExpressionTemplate } from './slot-expression-composer.js';

const quickAccessCatalog = listTaskSpecTemplateQuickAccess({
  family: 'graph_write',
  cluster: '',
  operation: '',
  writeMode: '',
}).items;

test('composeSlotExpressionTemplate embeds let literal expression by descriptor input path', () => {
  const result = composeSlotExpressionTemplate({
    expression: 'generic_ops.let.default(generic_ops.expression.literal)',
    writeMode: 'graph.append',
    quickAccessCatalog,
  });

  assert.equal(result.ok, true);
  assert.deepEqual(result.ok ? result.value : undefined, {
    kind: 'let',
    name: '__REQUIRED_SYMBOL_NAME__',
    value: {
      kind: 'literal',
      value_type: '__REQUIRED_LITERAL_VALUE_TYPE__',
      value: '__REQUIRED_VALUE__',
    },
  });
});

test('composeSlotExpressionTemplate writes dynamic call args by positional placeholders', () => {
  const result = composeSlotExpressionTemplate({
    expression: 'generic_ops.call.direct(0,0,0,generic_ops.expression.get_symbol_or_variable)',
    writeMode: 'graph.append',
    quickAccessCatalog,
  });

  assert.equal(result.ok, true);
  const value = result.ok ? result.value as { args: Record<string, unknown> } : { args: {} };
  assert.deepEqual(Object.keys(value.args), ['__REQUIRED_ARG_2_NAME__']);
  assert.deepEqual(value.args['__REQUIRED_ARG_2_NAME__'], {
    kind: 'get',
    target: '__REQUIRED_SYMBOL_OR_VARIABLE_NAME__',
  });
});

test('composeSlotExpressionTemplate writes call receiver by descriptor path', () => {
  const result = composeSlotExpressionTemplate({
    expression: 'generic_ops.call.direct(generic_ops.expression.get_symbol_or_variable)',
    writeMode: 'graph.append',
    quickAccessCatalog,
  });

  assert.equal(result.ok, true);
  const value = result.ok
    ? result.value as { target_object?: unknown; args: Record<string, unknown> }
    : { args: {} };
  assert.deepEqual(value.target_object, {
    kind: 'get',
    target: '__REQUIRED_SYMBOL_OR_VARIABLE_NAME__',
  });
  assert.deepEqual(Object.keys(value.args), []);
});

test('composeSlotExpressionTemplate embeds nested expression children', () => {
  const result = composeSlotExpressionTemplate({
    expression: 'generic_ops.let.default(generic_ops.expression.op(generic_ops.expression.get_symbol_or_variable,generic_ops.expression.literal))',
    writeMode: 'graph.append',
    quickAccessCatalog,
  });

  assert.equal(result.ok, true);
  const statement = result.ok ? result.value as { value: { kind: string; left: unknown; right: unknown } } : undefined;
  assert.equal(statement?.value.kind, 'op');
  assert.deepEqual(statement?.value.left, {
    kind: 'get',
    target: '__REQUIRED_SYMBOL_OR_VARIABLE_NAME__',
  });
  assert.deepEqual(statement?.value.right, {
    kind: 'literal',
    value_type: '__REQUIRED_LITERAL_VALUE_TYPE__',
    value: '__REQUIRED_VALUE__',
  });
});

test('composeSlotExpressionTemplate rejects expression roots and invalid child positions', () => {
  const expressionRoot = composeSlotExpressionTemplate({
    expression: 'generic_ops.expression.literal',
    writeMode: 'graph.append',
    quickAccessCatalog,
  });
  assert.equal(expressionRoot.ok, false);
  assert.equal(expressionRoot.ok ? undefined : expressionRoot.diagnostics[0]?.code, 'root_expression_slot_not_composable');

  const tooManyArgs = composeSlotExpressionTemplate({
    expression: 'generic_ops.let.default(generic_ops.expression.literal,generic_ops.expression.literal)',
    writeMode: 'graph.append',
    quickAccessCatalog,
  });
  assert.equal(tooManyArgs.ok, false);
  assert.equal(tooManyArgs.ok ? undefined : tooManyArgs.diagnostics[0]?.code, 'slot_input_index_out_of_range');
});
