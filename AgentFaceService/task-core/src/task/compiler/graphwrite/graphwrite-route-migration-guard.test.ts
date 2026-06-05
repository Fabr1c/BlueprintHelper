import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

test('GraphWrite route migration guard blocks new public route literals in legacy compiler facade', () => {
  const compilerSource = readTaskCoreFile('src/task/compiler/task-compiler.ts');
  const routeSource = JSON.parse(readTaskCoreFile('src/task/compiler/graphwrite/graphwrite-route-source.json')) as {
    routes: Array<{ route_id: string }>;
  };
  const descriptorRouteIds = new Set(routeSource.routes.map((route) => route.route_id));
  const routeLiterals = [...compilerSource.matchAll(/['"](graph\.(?:append|replace|merge|patch|merge_external_flow|patch_external_graph|replace_external_body)[A-Za-z0-9_.-]*)['"]/g)]
    .map((match) => match[1])
    .filter((routeId): routeId is string => routeId !== undefined);

  for (const routeId of routeLiterals) {
    assert.equal(
      descriptorRouteIds.has(routeId),
      true,
      `${routeId} must enter graphwrite-route-source.json instead of task-compiler.ts legacy branches.`,
    );
  }
});

function readTaskCoreFile(relativePath: string): string {
  return fs.readFileSync(path.resolve(taskCoreRoot(), relativePath), 'utf8');
}

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
}
