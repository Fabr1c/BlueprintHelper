import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

test('public task tool dispatch is registry driven during migration', () => {
  const dispatcher = fs.readFileSync(
    sourcePath('task-tool-dispatcher.ts'),
    'utf8',
  );
  const handlers = fs.readFileSync(
    sourcePath('task-execution-handlers.ts'),
    'utf8',
  );
  const registry = fs.readFileSync(
    sourcePath('task-tool-handler-registry.ts'),
    'utf8',
  );
  const taskToolSource = fs.readFileSync(
    path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..', 'src', 'tool-surface', 'registry', 'task-tool-source.ts'),
    'utf8',
  );
  const runnerSource = fs.readFileSync(
    path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..', 'src', 'task', 'service', 'task-spec-runner.ts'),
    'utf8',
  );
  const legacyReadTaskContextTokens = [
    'read' + 'TaskContext',
    'read_' + 'task_context',
    'blueprinthelper_' + 'read_' + 'task_context',
    'Task' + 'ContextPack',
  ];

  assert.doesNotMatch(dispatcher, /switch\s*\(name\)/);
  assert.doesNotMatch(dispatcher, /case 'blueprinthelper_/);
  assert.doesNotMatch(handlers, /'task_spec'\s+in\s+input/);
  assert.doesNotMatch(handlers, /TaskSpecSchema\.parse\(input\)/);
  assert.doesNotMatch(registry, /switch\s*\(toolName\)/);
  assert.doesNotMatch(registry, /if\s*\(\s*toolName\s*===\s*'blueprinthelper_/);
  assert.doesNotMatch(taskToolSource, /blueprinthelper_preview_task|blueprinthelper_execute_task|blueprinthelper_get_task_result/);
  assert.match(taskToolSource, /getDefaultTaskToolHandlerRegistry/);
  for (const legacyToken of legacyReadTaskContextTokens) {
    assert.equal(runnerSource.includes(legacyToken), false);
    assert.equal(registry.includes(legacyToken), false);
    assert.equal(dispatcher.includes(legacyToken), false);
    assert.equal(taskToolSource.includes(legacyToken), false);
  }
});

function sourcePath(fileName: string): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..', 'src', 'tool-surface', 'task', fileName);
}
