import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import * as path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const TASK_CORE_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const REPO_ROOT = path.resolve(TASK_CORE_ROOT, '..', '..');
const UE_PLUGIN_ROOT = path.resolve(REPO_ROOT, 'BlueprintHelper');
const UE_SOURCE_ROOT = path.resolve(UE_PLUGIN_ROOT, 'Source', 'BlueprintHelper');
const AGENT_FACE_ROOT = path.resolve(REPO_ROOT, 'AgentFaceService');

test('UE include boundaries keep Shared, Systems, and TaskRuntime clusters decoupled', () => {
  const violations = [
    ...findIncludeViolations(
      [
        path.resolve(UE_SOURCE_ROOT, 'Public', 'Shared'),
        path.resolve(UE_SOURCE_ROOT, 'Private', 'Shared'),
      ],
      /^Systems\//u,
      'Shared layer must not include Systems layer',
    ),
    ...findIncludeViolations(
      [
        path.resolve(UE_SOURCE_ROOT, 'Public', 'Systems'),
        path.resolve(UE_SOURCE_ROOT, 'Private', 'Systems'),
      ],
      /^Entry\//u,
      'Systems layer must not include Entry layer',
    ),
    ...findIncludeViolations(
      [
        path.resolve(UE_SOURCE_ROOT, 'Public', 'Runtime', 'TaskRuntime', 'Clusters'),
        path.resolve(UE_SOURCE_ROOT, 'Private', 'Runtime', 'TaskRuntime', 'Clusters'),
      ],
      /BlueprintHelperTaskRuntimeService\.h$/u,
      'TaskRuntime clusters must not include the orchestration service header',
    ),
  ];

  assert.deepEqual(violations, []);
});

test('TaskPlan adapters keep a one-header one-cpp implementation contract', () => {
  const publicRoot = path.resolve(UE_SOURCE_ROOT, 'Public', 'Runtime', 'TaskRuntime', 'TaskPlanAdapters');
  const privateRoot = path.resolve(UE_SOURCE_ROOT, 'Private', 'Runtime', 'TaskRuntime', 'TaskPlanAdapters');
  const missingImplementations = collectFiles(publicRoot, ['.h'])
    .map((headerPath) => path.relative(publicRoot, headerPath))
    .map((relativeHeader) => relativeHeader.replace(/\.h$/u, '.cpp'))
    .filter((relativeCpp) => !fs.existsSync(path.resolve(privateRoot, relativeCpp)));

  assert.deepEqual(missingImplementations, []);
});

test('Bridge route planner declares every command once in the route registry table', () => {
  const routePlannerUtils = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Entry', 'Bridge', 'Utils', 'BlueprintHelperBridgeRoutePlannerUtils.cpp'),
    'utf8',
  );
  const routeTable = routePlannerUtils.match(
    /GBlueprintHelperBridgeRouteCommandClusters\[\]\s*=\s*\{([\s\S]*?)\};/u,
  )?.[1] ?? '';
  const commandNames = [...routeTable.matchAll(/TEXT\("([^"]+)"\)/gu)].map((match) => match[1]);
  const duplicates = commandNames.filter((command, index) => commandNames.indexOf(command) !== index);

  assert.ok(commandNames.length > 0, 'route planner command table should be discoverable');
  assert.deepEqual([...new Set(duplicates)].sort(), []);
});

test('CLI and MCP production task paths use Python compiler instead of TS fallback compiler', () => {
  const productionFiles = [
    ...collectFiles(path.resolve(AGENT_FACE_ROOT, 'cli', 'src'), ['.ts']),
    ...collectFiles(path.resolve(AGENT_FACE_ROOT, 'mcp', 'src', 'mcp'), ['.ts']),
  ];
  const directTsCompilerUsers = productionFiles
    .filter((filePath) => fs.readFileSync(filePath, 'utf8').includes('compileTaskSpecToTaskPlan'))
    .map(toRepoRelativePath);

  assert.deepEqual(directTsCompilerUsers, []);
  assert.match(
    fs.readFileSync(path.resolve(AGENT_FACE_ROOT, 'cli', 'src', 'cli', 'run.ts'), 'utf8'),
    /compileTaskSpecWithPython/u,
  );
  assert.match(
    fs.readFileSync(path.resolve(AGENT_FACE_ROOT, 'mcp', 'src', 'mcp', 'tools', 'task-tools.ts'), 'utf8'),
    /compileTaskSpecWithPython/u,
  );
  assert.match(
    fs.readFileSync(path.resolve(AGENT_FACE_ROOT, 'mcp', 'src', 'mcp', 'tools', 'shared-registry-adapter.ts'), 'utf8'),
    /compileTaskSpecWithPython/u,
  );
});

function findIncludeViolations(
  roots: string[],
  forbiddenIncludePattern: RegExp,
  message: string,
): string[] {
  return roots
    .flatMap((root) => collectFiles(root, ['.h', '.cpp']))
    .flatMap((filePath) => {
      const text = fs.readFileSync(filePath, 'utf8');
      return [...text.matchAll(/^\s*#\s*include\s+"([^"]+)"/gmu)]
        .map((match) => normalizePath(match[1]))
        .filter((includePath) => forbiddenIncludePattern.test(includePath))
        .map((includePath) => `${message}: ${toRepoRelativePath(filePath)} includes ${includePath}`);
    });
}

function collectFiles(root: string, extensions: string[]): string[] {
  if (!fs.existsSync(root)) {
    return [];
  }

  const entries = fs.readdirSync(root, { withFileTypes: true });
  return entries.flatMap((entry) => {
    const fullPath = path.resolve(root, entry.name);
    if (entry.isDirectory()) {
      return collectFiles(fullPath, extensions);
    }
    return entry.isFile() && extensions.includes(path.extname(entry.name)) ? [fullPath] : [];
  });
}

function normalizePath(value: string): string {
  return value.replace(/\\/gu, '/');
}

function toRepoRelativePath(filePath: string): string {
  return normalizePath(path.relative(REPO_ROOT, filePath));
}
