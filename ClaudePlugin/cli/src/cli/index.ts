#!/usr/bin/env node
import { runCli } from './run.js';

const exitCode = await runCli({
  argv: process.argv.slice(2),
  cwd: process.cwd(),
  stdout: (text) => process.stdout.write(text),
  stderr: (text) => process.stderr.write(text),
});

process.exit(exitCode);
