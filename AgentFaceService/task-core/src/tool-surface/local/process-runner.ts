import { execFile, spawn } from 'node:child_process';
import * as path from 'node:path';
import type { BlueprintHelperToolContext, LocalProcessResult } from '../types.js';

export function runProcess(
  context: BlueprintHelperToolContext,
  command: string,
  args: string[],
  options: { timeoutMs?: number; detached?: boolean } = {},
): Promise<LocalProcessResult> {
  if (context.runLocalProcess) {
    return context.runLocalProcess(command, args, options);
  }

  if (options.detached) {
    if (process.platform === 'win32') {
      const script = [
        '$ErrorActionPreference = "Stop"',
        `$ArgumentList = @(${args.map(quotePowerShellString).join(', ')})`,
        `Start-Process -FilePath ${quotePowerShellString(command)} -ArgumentList $ArgumentList -WorkingDirectory ${quotePowerShellString(path.dirname(command))}`,
      ].join('; ');
      return new Promise((resolve) => {
        execFile('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', script], {
          maxBuffer: 1024 * 1024,
          timeout: 30_000,
        }, (error, stdout, stderr) => {
          resolve({
            exitCode: error && typeof error.code === 'number' ? error.code : error ? 1 : 0,
            stdout,
            stderr,
          });
        });
      });
    }

    const child = spawn(command, args, {
      detached: true,
      stdio: 'ignore',
    });
    child.unref();
    return Promise.resolve({ exitCode: 0, stdout: '', stderr: '' });
  }

  return new Promise((resolve) => {
    execFile(command, args, {
      maxBuffer: 10 * 1024 * 1024,
      timeout: options.timeoutMs,
    }, (error, stdout, stderr) => {
      resolve({
        exitCode: error && typeof error.code === 'number' ? error.code : error ? 1 : 0,
        stdout,
        stderr,
      });
    });
  });
}

function quotePowerShellString(value: string): string {
  return `'${value.replace(/'/g, "''")}'`;
}
