import assert from 'node:assert/strict';
import test from 'node:test';

import {
  parseSlotExpression,
  splitTopLevelSlotExpressions,
} from './slot-expression-parser.js';

test('splitTopLevelSlotExpressions splits only depth-zero commas', () => {
  assert.deepEqual(
    splitTopLevelSlotExpressions('generic_ops.let.default(generic_ops.expression.literal),generic_ops.call.direct(0,generic_ops.expression.get_symbol_or_variable)'),
    [
      'generic_ops.let.default(generic_ops.expression.literal)',
      'generic_ops.call.direct(0,generic_ops.expression.get_symbol_or_variable)',
    ],
  );
});

test('parseSlotExpression parses nested expressions and skip args', () => {
  assert.deepEqual(
    parseSlotExpression('generic_ops.call.direct(0,generic_ops.expression.op(generic_ops.expression.get_symbol_or_variable,generic_ops.expression.literal))'),
    {
      kind: 'slot',
      templateId: 'generic_ops.call.direct',
      args: [
        { kind: 'skip' },
        {
          kind: 'slot',
          templateId: 'generic_ops.expression.op',
          args: [
            { kind: 'slot', templateId: 'generic_ops.expression.get_symbol_or_variable', args: [] },
            { kind: 'slot', templateId: 'generic_ops.expression.literal', args: [] },
          ],
        },
      ],
    },
  );
});

test('parseSlotExpression rejects empty args and unbalanced parentheses', () => {
  assert.throws(
    () => parseSlotExpression('generic_ops.call.direct(generic_ops.expression.literal,)'),
    /invalid_slot_expression_syntax/,
  );
  assert.throws(
    () => parseSlotExpression('generic_ops.call.direct(generic_ops.expression.literal'),
    /invalid_slot_expression_syntax/,
  );
});
