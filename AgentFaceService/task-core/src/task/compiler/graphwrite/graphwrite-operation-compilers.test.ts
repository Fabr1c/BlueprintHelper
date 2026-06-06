import assert from 'node:assert/strict';
import test from 'node:test';

import {
  graphWriteAppendTaskSpecFixture,
  graphWriteReplaceTaskSpecFixture,
} from '../../fixtures/task-protocol.fixtures.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import { createDefaultGraphWriteOperationCompilerRegistry } from './default-graphwrite-operation-compilers.js';

test('default GraphWrite operation registry compiles append and replace routes', () => {
  const registry = createDefaultGraphWriteOperationCompilerRegistry();
  const appendOps = registry.compile(graphWriteAppendTaskSpecFixture.behavior as Record<string, unknown>);
  const replaceOps = registry.compile(graphWriteReplaceTaskSpecFixture.behavior as Record<string, unknown>);

  assert.equal(appendOps.some((op) => op.op === 'ensure_entry'), true);
  assert.equal(replaceOps.some((op) => op.op === 'replace_body'), true);
});

test('default GraphWrite operation registry rejects unknown strategy with structured error', () => {
  const registry = createDefaultGraphWriteOperationCompilerRegistry();

  assert.throws(
    () => registry.compile({ graph_strategy: 'old_inline_branch' }),
    (error: unknown) => {
      assert.equal(error instanceof TaskSpecCompileError, true);
      assert.equal((error as TaskSpecCompileError).code, 'unsupported_graph_strategy');
      return true;
    },
  );
});
