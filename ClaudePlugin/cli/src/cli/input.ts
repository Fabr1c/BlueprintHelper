import * as fs from 'node:fs';
import * as path from 'node:path';

export interface CliInputOptions {
  cwd: string;
  file?: string;
  json?: string;
  stdin?: boolean;
  readStdin?: () => Promise<string> | string;
}

export async function readCliInputObject(options: CliInputOptions): Promise<Record<string, unknown>> {
  const sourceCount = [options.file, options.json, options.stdin ? 'stdin' : undefined]
    .filter((value) => value !== undefined).length;
  if (sourceCount !== 1) {
    throw new Error('Choose exactly one params input source: --file, --json, or --stdin.');
  }

  const text = options.file
    ? fs.readFileSync(path.resolve(options.cwd, options.file), 'utf8')
    : options.json ?? await readStdinText(options);
  const parsed = JSON.parse(text);
  if (!isRecord(parsed)) {
    throw new Error('CLI params must be a JSON object.');
  }
  return parsed;
}

async function readStdinText(options: CliInputOptions): Promise<string> {
  if (!options.readStdin) {
    throw new Error('--stdin requires a stdin reader.');
  }
  return await options.readStdin();
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
