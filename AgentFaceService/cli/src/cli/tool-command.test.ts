import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

test('invokeCliTool normalizes through manifest input shapes before tool schema execution', () => {
  const source = readFileSync(
    path.resolve(cliRoot(), 'src', 'cli', 'tool-command.ts'),
    'utf8',
  );

  assert.equal(
    /tool\.inputSchema\.parse\(params\)/.test(source),
    false,
    'CLI direct tool execution must not parse raw params before InputShapeAdapter',
  );
  assert.equal(
    /normalizeToolInputForManifest/.test(source),
    true,
    'CLI direct tool execution must consume the shared manifest input normalizer',
  );
});

function cliRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
}
