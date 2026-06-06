import assert from 'node:assert/strict';
import test from 'node:test';

import { getDefaultGraphWriteExpressionCompilerIds } from './graphwrite-expression-compilers.js';
import { getAllGraphWriteSlotDescriptors } from './graphwrite-slot-registry.js';
import { getDefaultGraphWriteStatementCompilerIds } from './graphwrite-statement-compilers.js';

test('GraphWrite slot migration guard requires statement and expression compiler registrations', () => {
  const statementCompilerIds = new Set(getDefaultGraphWriteStatementCompilerIds());
  const expressionCompilerIds = new Set(getDefaultGraphWriteExpressionCompilerIds());

  for (const slot of getAllGraphWriteSlotDescriptors()) {
    if (slot.slot_type === 'statement') {
      assert.equal(
        statementCompilerIds.has(slot.compiler_id),
        true,
        `${slot.slot_id} compiler_id must be registered in graphwrite-statement-compilers.ts`,
      );
    }
    if (slot.slot_type === 'expression') {
      assert.equal(
        expressionCompilerIds.has(slot.compiler_id),
        true,
        `${slot.slot_id} compiler_id must be registered in graphwrite-expression-compilers.ts`,
      );
    }
  }
});
