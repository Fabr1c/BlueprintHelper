import * as fs from 'node:fs';
import * as path from 'node:path';

export function resolveArtifactRoot(input: { cwd: string; cliDir?: string }): string {
  return input.cliDir
    ?? process.env['BPH_CLI_ARTIFACT_DIR']
    ?? path.resolve(input.cwd, 'Saved', 'BlueprintHelper', 'Cli');
}

export function writeJsonArtifact(input: {
  root: string;
  runId: string;
  name: string;
  value: unknown;
}): string {
  const dir = path.resolve(input.root, safeSegment(input.runId));
  fs.mkdirSync(dir, { recursive: true });
  const filePath = path.resolve(dir, `${safeSegment(input.name)}.json`);
  fs.writeFileSync(filePath, stringifyJsonArtifact(input.value), 'utf8');
  return filePath;
}

export function stringifyJsonArtifact(value: unknown): string {
  return `${JSON.stringify(value, null, 2)}\n`.replace(/[^\u0000-\u007f]/g, (char) => {
    return `\\u${char.charCodeAt(0).toString(16).padStart(4, '0')}`;
  });
}

function safeSegment(value: string): string {
  const safe = value.replace(/[^A-Za-z0-9_.-]/g, '_');
  return safe.length > 0 ? safe : 'artifact';
}
