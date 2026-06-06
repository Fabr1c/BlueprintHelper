import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

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

test('GraphWrite operation compilers live in focused owner modules', () => {
  const taskCoreRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
  const graphwriteRoot = path.resolve(taskCoreRoot, 'src', 'task', 'compiler', 'graphwrite');
  const logicBodySource = readFileSync(path.join(graphwriteRoot, 'graphwrite-logic-body-compiler.ts'), 'utf8');
  const ownerModules = [
    'graphwrite-append-compiler.ts',
    'graphwrite-replace-compiler.ts',
    'graphwrite-patch-compiler.ts',
    'graphwrite-merge-compiler.ts',
  ];

  for (const forbidden of [
    'compileAppendGraphWriteOps',
    'compileReplaceGraphWriteOp',
    'compilePatchGraphWriteOps',
    'compileMergeGraphWriteOps',
    'compileExternalMergeGraphWriteOps',
    'compileExternalPatchGraphWriteOps',
    'compileExternalReplaceBodyGraphWriteOp',
  ]) {
    assert.equal(
      new RegExp(`export function ${forbidden}\\b`).test(logicBodySource),
      false,
      `${forbidden} must not be owned by graphwrite-logic-body-compiler.ts`,
    );
  }

  for (const ownerModule of ownerModules) {
    const source = readFileSync(path.join(graphwriteRoot, ownerModule), 'utf8');
    assert.equal(
      /from ['"]\.\/graphwrite-logic-body-compiler\.js['"];?\s*$/m.test(source) && /export\s*\{/.test(source),
      false,
      `${ownerModule} must implement operation compiler functions instead of re-exporting them`,
    );
  }
});
