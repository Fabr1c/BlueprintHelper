import assert from 'node:assert/strict';
import test from 'node:test';

import { createDefaultExpressionCompilerRegistry } from './expression-compiler-registry.js';
import {
  createGraphWriteExpressionCompilerRegistrations,
  getDefaultGraphWriteExpressionCompilerIds,
} from './graphwrite-expression-compilers.js';
import { getDefaultGraphWriteStatementCompilerIds, createGraphWriteStatementCompilerRegistrations } from './graphwrite-statement-compilers.js';
import { createDefaultStatementCompilerRegistry } from './statement-compiler-registry.js';
import type {
  CompiledConditionFlow,
  CompiledStatementFlow,
} from './graphwrite-compiler-types.js';

test('statement compiler registrations attach flow and node handlers to every default compiler id', () => {
  const statementRegistry = createDefaultStatementCompilerRegistry(
    createGraphWriteStatementCompilerRegistrations({
      compileFlow: (): CompiledStatementFlow => ({ nodes: [], links: [], exits: [] }),
      compileNode: () => ({ id: 'node', kind: 'call' }),
    }),
  );

  for (const compilerId of getDefaultGraphWriteStatementCompilerIds()) {
    const descriptor = statementRegistry.requireByCompilerId(compilerId);
    assert.equal(typeof descriptor.compile_flow, 'function');
    assert.equal(typeof descriptor.compile_node, 'function');
  }
});

test('expression compiler registrations attach compile handlers to every default compiler id', () => {
  const expressionRegistry = createDefaultExpressionCompilerRegistry(
    createGraphWriteExpressionCompilerRegistrations({
      compileExpression: (): CompiledConditionFlow => ({ nodes: [], links: [], defaultValue: undefined }),
    }),
  );

  for (const compilerId of getDefaultGraphWriteExpressionCompilerIds()) {
    const descriptor = expressionRegistry.requireByCompilerId(compilerId);
    assert.equal(typeof descriptor.compile, 'function');
  }
});
