import { readdirSync } from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const buildDir = path.resolve('build');
const testFiles = readdirSync(buildDir)
  .filter((name) => name.endsWith('.test.js'))
  .sort();

for (const testFile of testFiles) {
  await import(pathToFileURL(path.join(buildDir, testFile)).href);
}
