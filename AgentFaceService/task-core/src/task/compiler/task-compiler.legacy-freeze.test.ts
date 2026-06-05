import assert from 'node:assert/strict';
import { mkdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { pathToFileURL } from 'node:url';
import { fileURLToPath } from 'node:url';

test('legacy task-compiler file remains frozen for new public capabilities', async () => {
  const module = await import(pathToFileURL(freezeScriptPath()).href) as {
    runLegacyFreezeCheck(root?: string): { ok?: boolean; matches?: unknown[] };
  };
  const result = module.runLegacyFreezeCheck(taskCoreRoot());

  assert.equal(result.ok, true);
  assert.deepEqual(result.matches ?? [], []);
});

test('legacy task-compiler freeze catches unknown strategy and switch adapter branches', async (t) => {
  const root = await mkdtemp(path.join(os.tmpdir(), 'bph-legacy-freeze-'));
  t.after(() => rm(root, { recursive: true, force: true }));
  const compilerPath = path.join(root, 'src', 'task', 'compiler', 'task-compiler.ts');
  await mkdir(path.dirname(compilerPath), { recursive: true });
  await writeFile(compilerPath, `
    function compile(strategy: string, operation: string) {
      if (strategy === 'macro_body') return true;
      switch (operation) {
        case 'macro_blueprint_graph':
          return true;
        default:
          return false;
      }
    }
  `, 'utf8');
  const module = await import(pathToFileURL(freezeScriptPath()).href) as {
    runLegacyFreezeCheck(root?: string): { ok?: boolean; matches?: Array<{ kind?: string; value?: string }> };
  };

  const result = module.runLegacyFreezeCheck(root);

  assert.equal(result.ok, false);
  assert.equal(result.matches?.some((match) => match.kind === 'graph_strategy' && match.value === 'macro_body'), true);
  assert.equal(result.matches?.some((match) => match.kind === 'adapter_operation' && match.value === 'macro_blueprint_graph'), true);
});

function freezeScriptPath(): string {
  return path.resolve(taskCoreRoot(), 'scripts', 'check-task-compiler-legacy-freeze.mjs');
}

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
}
