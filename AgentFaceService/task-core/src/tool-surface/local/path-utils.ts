import * as path from 'node:path';

export function ancestorDirs(start: string): string[] {
  const dirs: string[] = [];
  let current = start;
  for (;;) {
    dirs.push(current);
    const parent = path.dirname(current);
    if (parent === current) {
      return dirs;
    }
    current = parent;
  }
}

export function toUnrealPath(value: string): string {
  return path.resolve(value).replace(/\\/g, '/');
}

export function normalizeProcessPath(value: string): string {
  return value.replace(/\\/g, '/').toLowerCase();
}
