import assert from 'node:assert/strict';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { readCliInputObjectWithStats } from './input.js';

test('readCliInputObjectWithStats accepts BOM-prefixed --file JSON', async () => {
  const dir = mkdtempSync(path.join(os.tmpdir(), 'bh-cli-input-file-'));
  try {
    writeFileSync(path.join(dir, 'params.json'), '\uFEFF{"ok":true}', 'utf8');
    const result = await readCliInputObjectWithStats({ cwd: dir, file: 'params.json' });
    assert.deepEqual(result.value, { ok: true });
    assert.equal(result.io.input_source, 'file');
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('readCliInputObjectWithStats accepts BOM-prefixed --stdin JSON', async () => {
  const result = await readCliInputObjectWithStats({
    cwd: process.cwd(),
    stdin: true,
    readStdin: () => '\uFEFF{"ok":true}',
  });
  assert.deepEqual(result.value, { ok: true });
  assert.equal(result.io.input_source, 'stdin');
});

test('readCliInputObjectWithStats accepts BOM-prefixed --json and keeps PowerShell guidance for invalid inline JSON', async () => {
  const valid = await readCliInputObjectWithStats({
    cwd: process.cwd(),
    json: '\uFEFF{"ok":true}',
  });
  assert.deepEqual(valid.value, { ok: true });

  await assert.rejects(
    () => readCliInputObjectWithStats({ cwd: process.cwd(), json: '\uFEFF{ok:true}' }),
    /PowerShell often strips quotes from inline JSON/u,
  );
});
