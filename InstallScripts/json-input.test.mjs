import assert from 'node:assert/strict';
import { mkdtemp, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import {
  parseJsonText,
  readJsonFile,
  stripJsonTextBom,
} from './json-input.mjs';

test('install script JSON helper strips one leading BOM', () => {
  assert.equal(stripJsonTextBom('\uFEFF{"ok":true}'), '{"ok":true}');
  assert.equal(stripJsonTextBom('\uFEFF\uFEFF{"ok":true}'), '\uFEFF{"ok":true}');
});

test('install script JSON helper parses BOM-prefixed files', async () => {
  const dir = await mkdtemp(path.join(os.tmpdir(), 'bh-install-json-'));
  try {
    const file = path.join(dir, 'selection.json');
    await writeFile(file, '\uFEFF{"ok":true}', 'utf8');
    assert.deepEqual(await readJsonFile(file), { ok: true });
    assert.deepEqual(parseJsonText('\uFEFF{"ok":true}'), { ok: true });
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
});

test('install script JSON helper rejects files with more than one leading BOM', async () => {
  const dir = await mkdtemp(path.join(os.tmpdir(), 'bh-install-json-double-bom-'));
  try {
    const file = path.join(dir, 'selection.json');
    await writeFile(file, '\uFEFF\uFEFF{"ok":true}', 'utf8');
    await assert.rejects(
      () => readJsonFile(file),
      /Unexpected token|Unexpected non-whitespace character after JSON/u,
    );
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
});

test('install script JSON helper keeps JSON parsing strict after a leading BOM', () => {
  assert.throws(
    () => parseJsonText('\uFEFF{"ok":true,}'),
    /Unexpected token|Expected double-quoted property name/u,
  );
});
