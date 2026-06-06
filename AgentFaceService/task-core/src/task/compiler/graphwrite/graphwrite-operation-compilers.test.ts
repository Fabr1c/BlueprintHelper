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
import { compilePatchGraphWriteOps } from './graphwrite-patch-compiler.js';

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

test('patch compiler owns every owned patch payload kind', () => {
  const ops = compilePatchGraphWriteOps({
    patches: [
      { kind: 'set_pin_default', target_ref: { block_id: 'B', node_ref: 'N', pin_ref: 'P' }, value: '42' },
      { kind: 'set_node_comment', target_ref: { block_id: 'B', node_ref: 'N' }, value: 'hello' },
      { kind: 'connect_pins', target_ref: { block_id: 'B', node_ref: 'To', pin_ref: 'Exec' }, source_ref: { node_ref: 'From', pin_ref: 'Then' } },
      { kind: 'disconnect_link', target_ref: { block_id: 'B', node_ref: 'N', pin_ref: 'P', link_ref: 'L' } },
      { kind: 'replace_link', target_ref: { block_id: 'B', node_ref: 'N', pin_ref: 'P', link_ref: 'L' }, replacement_ref: { node_ref: 'New', pin_ref: 'Then' } },
      { kind: 'delete_owned_node', target_ref: { block_id: 'B', node_ref: 'N' } },
    ],
  });

  assert.deepEqual(ops.map((op) => op.op), [
    'set_pin_default',
    'set_node_comment',
    'connect_pins',
    'disconnect_link',
    'replace_link',
    'delete_owned_node',
  ]);
  assert.equal(ops[0]?.patch_scope, 'pin_default');
  assert.equal(ops[1]?.patch_scope, 'node_comment');
  assert.equal(ops[2]?.patch_scope, 'connect_pins');
  assert.equal(ops[3]?.patch_scope, 'disconnect_link');
  assert.equal(ops[4]?.patch_scope, 'replace_link');
  assert.equal(ops[5]?.patch_scope, 'node_delete');
});
