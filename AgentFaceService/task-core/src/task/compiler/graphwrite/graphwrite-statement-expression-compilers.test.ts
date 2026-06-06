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
  GraphWriteExpressionCompileInput,
  GraphWriteStatementNodeCompileInput,
} from './graphwrite-compiler-types.js';

test('statement compiler registrations attach flow and node handlers to every default compiler id', () => {
  const statementRegistry = createDefaultStatementCompilerRegistry(
    createGraphWriteStatementCompilerRegistrations(makeStatementCompilerServices()),
  );

  const flowHandlers = new Set<unknown>();
  const nodeHandlers = new Set<unknown>();
  for (const compilerId of getDefaultGraphWriteStatementCompilerIds()) {
    const descriptor = statementRegistry.requireByCompilerId(compilerId);
    assert.equal(typeof descriptor.compile_flow, 'function');
    assert.equal(typeof descriptor.compile_node, 'function');
    flowHandlers.add(descriptor.compile_flow);
    nodeHandlers.add(descriptor.compile_node);
  }
  assert.equal(flowHandlers.size > 1, true, 'statement flow compiler ids must not all share one handler');
  assert.equal(nodeHandlers.size > 1, true, 'statement node compiler ids must not all share one handler');
});

test('expression compiler registrations attach compile handlers to every default compiler id', () => {
  const expressionRegistry = createDefaultExpressionCompilerRegistry(
    createGraphWriteExpressionCompilerRegistrations(makeExpressionCompilerServices()),
  );

  const handlers = new Set<unknown>();
  for (const compilerId of getDefaultGraphWriteExpressionCompilerIds()) {
    const descriptor = expressionRegistry.requireByCompilerId(compilerId);
    assert.equal(typeof descriptor.compile, 'function');
    handlers.add(descriptor.compile);
  }
  assert.equal(handlers.size > 1, true, 'expression compiler ids must not all share one handler');
});

function makeStatementCompilerServices() {
  const flow = (): CompiledStatementFlow => ({ nodes: [], links: [], exits: [] });
  const node = () => ({ id: 'node', kind: 'call' });
  return {
    compileBranchFlow: flow,
    compileReturnFlow: flow,
    compileSequenceFlow: flow,
    compileGenericControlFlow: flow,
    compileLetFlow: flow,
    compileContainerActionFlow: flow,
    compileDefaultExecFlow: flow,
    compileCallNode: node,
    compileComponentBoundEventNode: node,
    compileContainerActionNode: node,
    compileGenericControlNode: node,
    compileConvertOrScheduleNode: node,
    compileCreateNode: node,
    compileDelegateNode: node,
    compileFieldNode: node,
    compileSetNode: node,
    compileSetPropertyNode: node,
    compileUnsupportedNode: (_input: GraphWriteStatementNodeCompileInput): never => {
      throw new Error('unsupported');
    },
  };
}

function makeExpressionCompilerServices() {
  const expression = (_input: GraphWriteExpressionCompileInput): CompiledConditionFlow => ({
    nodes: [],
    links: [],
    defaultValue: undefined,
  });
  return {
    compileLiteral: expression,
    compileContainerAction: expression,
    compileFieldGet: expression,
    compileGeneral: expression,
  };
}
