import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));

test('GraphWrite manifest generators read JSON through single-BOM-tolerant helpers', () => {
  for (const scriptName of [
    'generate-graphwrite-route-manifest.mjs',
    'generate-graphwrite-slot-manifest.mjs',
  ]) {
    const source = fs.readFileSync(path.join(scriptDir, scriptName), 'utf8');

    assert.match(source, /stripJsonTextBom/u, `${scriptName} must define or import stripJsonTextBom`);
    assert.match(source, /JSON\.parse\(stripJsonTextBom\(fs\.readFileSync\(filePath,\s*'utf8'\)\)\)/u, `${scriptName} must parse normalized JSON text`);
    assert.doesNotMatch(source, /return JSON\.parse\(fs\.readFileSync\(filePath,\s*'utf8'\)\)/u, `${scriptName} must not directly parse file text`);
  }
});
