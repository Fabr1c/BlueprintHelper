import assert from 'node:assert/strict';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  parseJsonText,
  readJsonFile,
  readJsonFileText,
  stripJsonTextBom,
} from './json-input.js';

test('stripJsonTextBom removes one leading BOM only', () => {
  assert.equal(stripJsonTextBom('\uFEFF{"ok":true}'), '{"ok":true}');
  assert.equal(stripJsonTextBom('\uFEFF\uFEFF{"ok":true}'), '\uFEFF{"ok":true}');
  assert.equal(stripJsonTextBom('{"ok":true}'), '{"ok":true}');
});

test('parseJsonText accepts one leading BOM and keeps strict JSON', () => {
  assert.deepEqual(parseJsonText('\uFEFF{"ok":true}'), { ok: true });
  assert.throws(
    () => parseJsonText('\uFEFF{"ok":true,}'),
    /Unexpected token|Expected double-quoted property name/u,
  );
});

test('readJsonFileText and readJsonFile normalize leading BOM', () => {
  const dir = mkdtempSync(path.join(os.tmpdir(), 'bh-json-input-'));
  try {
    const file = path.join(dir, 'params.json');
    writeFileSync(file, '\uFEFF{"schema":"example"}', 'utf8');

    assert.equal(readJsonFileText(file), '{"schema":"example"}');
    assert.deepEqual(readJsonFile(file), { schema: 'example' });
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('readJsonFile rejects files with more than one leading BOM', () => {
  const dir = mkdtempSync(path.join(os.tmpdir(), 'bh-json-input-double-bom-'));
  try {
    const file = path.join(dir, 'params.json');
    writeFileSync(file, '\uFEFF\uFEFF{"schema":"example"}', 'utf8');

    assert.equal(readJsonFileText(file), '\uFEFF{"schema":"example"}');
    assert.throws(
      () => readJsonFile(file),
      /Unexpected token|Unexpected non-whitespace character after JSON/u,
    );
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

function taskCoreSource(relativePath: string): string {
  const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', 'src');
  return readFileSync(path.join(root, relativePath), 'utf8');
}

test('template composers use shared JSON input helper for file reads', () => {
  const files = [
    'tool-surface/templates/taskspec-template-composer.ts',
    'tool-surface/templates/read-context-template-composer.ts',
    'tool-surface/templates/slot-expression-composer.ts',
  ];

  for (const file of files) {
    const source = taskCoreSource(file);
    assert.match(source, /readJsonFile|parseJsonText/u, file);
    assert.doesNotMatch(source, /JSON\.parse\(fs\.readFileSync\([^)]*'utf8'[^)]*\)\)/u, file);
  }
});
