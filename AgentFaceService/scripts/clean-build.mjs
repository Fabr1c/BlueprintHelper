import { rmSync } from 'node:fs';
import path from 'node:path';

const buildDir = path.resolve(process.cwd(), 'build');
const cwd = path.resolve(process.cwd());

if (!buildDir.startsWith(`${cwd}${path.sep}`)) {
  console.error(`Refusing to remove build directory outside cwd: ${buildDir}`);
  process.exit(1);
}

rmSync(buildDir, { recursive: true, force: true });
