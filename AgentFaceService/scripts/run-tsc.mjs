import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const cwd = process.cwd();
const candidates = [
  path.resolve(cwd, 'node_modules', 'typescript', 'lib', 'tsc.js'),
  path.resolve(cwd, '..', 'node_modules', 'typescript', 'lib', 'tsc.js'),
  path.resolve(scriptDir, '..', 'node_modules', 'typescript', 'lib', 'tsc.js'),
  path.resolve(scriptDir, '..', 'mcp', 'node_modules', 'typescript', 'lib', 'tsc.js'),
];

const tscPath = candidates.find((candidate) => existsSync(candidate));
if (!tscPath) {
  console.error('Unable to find TypeScript compiler. Run npm install in AgentFaceService/mcp or the current package.');
  process.exit(1);
}

const result = spawnSync(process.execPath, [tscPath, ...process.argv.slice(2)], {
  cwd,
  stdio: 'inherit',
});

process.exit(result.status ?? 1);
