import { existsSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import path from 'node:path';

const [rawPackageDir, ...npmArgs] = process.argv.slice(2);

if (!rawPackageDir || npmArgs.length === 0) {
  console.error('Usage: node run-package-npm.mjs <package-dir> <npm-args...>');
  process.exit(1);
}

const packageDir = path.resolve(process.cwd(), rawPackageDir);
const packageJson = path.join(packageDir, 'package.json');

if (!existsSync(packageJson)) {
  console.error(`Missing npm package manifest: ${packageJson}`);
  process.exit(1);
}

const npmExecPath = process.env.npm_execpath;
const command = npmExecPath && existsSync(npmExecPath)
  ? process.execPath
  : (process.platform === 'win32' ? 'npm.cmd' : 'npm');
const args = npmExecPath && existsSync(npmExecPath)
  ? [npmExecPath, ...npmArgs]
  : npmArgs;

const result = spawnSync(command, args, {
  cwd: packageDir,
  stdio: 'inherit',
  shell: false,
});

if (result.error) {
  console.error(`Failed to start ${command}: ${result.error.message}`);
  process.exit(1);
}

if (result.signal) {
  console.error(`${command} terminated by signal ${result.signal}`);
  process.exit(1);
}

process.exit(result.status ?? 1);
