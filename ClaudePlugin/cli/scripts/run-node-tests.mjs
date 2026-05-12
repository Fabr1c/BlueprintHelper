import { readdirSync } from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const buildDir = path.resolve('build');
const testFiles = collectTestFiles(buildDir).sort();

for (const testFile of testFiles) {
  await import(pathToFileURL(testFile).href);
}

function collectTestFiles(dir) {
  return readdirSync(dir, { withFileTypes: true })
    .flatMap((entry) => {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        return collectTestFiles(fullPath);
      }
      return entry.isFile() && entry.name.endsWith('.test.js') ? [fullPath] : [];
    });
}
