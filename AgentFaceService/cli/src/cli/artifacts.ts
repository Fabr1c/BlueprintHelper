import * as fs from 'node:fs';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import { readJsonFile } from '@blueprinthelper/task-core/json/json-input';

export function resolveArtifactRoot(input: { cwd: string; cliDir?: string }): string {
  return input.cliDir
    ?? process.env['BPH_CLI_ARTIFACT_DIR']
    ?? resolveConfiguredArtifactRoot(input.cwd)
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

function resolveConfiguredArtifactRoot(cwd: string): string | undefined {
  const projectRoot = findProjectRoot(cwd);
  const configuredDir = readCliArtifactSetting(cwd, projectRoot);
  if (!configuredDir) {
    return undefined;
  }
  return path.resolve(projectRoot ?? cwd, configuredDir);
}

function readCliArtifactSetting(cwd: string, projectRoot: string | undefined): string | undefined {
  const effectiveSetting = loadEffectiveSetting(projectRoot);
  const value = readPath(effectiveSetting, ['cli', 'artifacts', 'default_output_dir']);
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}

function loadEffectiveSetting(projectRoot: string | undefined): Record<string, unknown> {
  const effective: Record<string, unknown> = {};
  mergeJsonFileIfExists(defaultSettingPath(), effective);

  if (projectRoot) {
    mergeJsonFileIfExists(path.join(projectRoot, '.blueprinthelper', 'setting.json'), effective);
    mergeJsonFileIfExists(path.join(projectRoot, 'Saved', 'BlueprintHelper', 'setting.user.json'), effective);
  }
  return effective;
}

function defaultSettingPath(): string {
  return path.join(pluginWorkspaceRoot(), 'BlueprintHelper', 'Config', 'DefaultSetting.json');
}

function pluginWorkspaceRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..', '..');
}

function findProjectRoot(cwd: string): string | undefined {
  let current = path.resolve(cwd);
  while (true) {
    if (
      fs.existsSync(path.join(current, '.blueprinthelper')) ||
      fs.readdirSync(current, { withFileTypes: true }).some((entry) => entry.isFile() && entry.name.endsWith('.uproject'))
    ) {
      return current;
    }

    const parent = path.dirname(current);
    if (parent === current) {
      return undefined;
    }
    current = parent;
  }
}

function mergeJsonFileIfExists(filePath: string, target: Record<string, unknown>): void {
  if (!fs.existsSync(filePath)) {
    return;
  }

  try {
    const parsed: unknown = readJsonFile(filePath);
    if (isRecord(parsed)) {
      deepMerge(target, parsed);
    }
  } catch {
    // CLI output must still be writable when optional settings are invalid.
  }
}

function deepMerge(target: Record<string, unknown>, source: Record<string, unknown>): void {
  for (const [key, value] of Object.entries(source)) {
    if (isRecord(value) && isRecord(target[key])) {
      deepMerge(target[key] as Record<string, unknown>, value);
    } else {
      target[key] = value;
    }
  }
}

function readPath(record: Record<string, unknown>, parts: string[]): unknown {
  let current: unknown = record;
  for (const part of parts) {
    if (!isRecord(current) || !(part in current)) {
      return undefined;
    }
    current = current[part];
  }
  return current;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
