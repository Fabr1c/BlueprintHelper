import assert from 'node:assert/strict';
import * as fs from 'node:fs';
import * as path from 'node:path';
import test from 'node:test';

const mcpRoot = path.resolve(import.meta.dirname, '..', '..', '..');

test('mcp package does not expose or contain CLI transport files', () => {
  const packageJson = JSON.parse(
    fs.readFileSync(path.join(mcpRoot, 'package.json'), 'utf8'),
  ) as Record<string, unknown>;

  assert.equal(packageJson['bin'], undefined);
  assert.equal((packageJson['scripts'] as Record<string, unknown> | undefined)?.['cli'], undefined);
  assert.equal(fs.existsSync(path.join(mcpRoot, 'src', 'cli')), false);
});
