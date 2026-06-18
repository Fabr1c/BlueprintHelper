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

test('Bridge route planner derives routes from generated manifests and the system registry', () => {
  const commandRegistry = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Entry', 'Bridge', 'BlueprintHelperBridgeCommandRegistry.cpp'),
    'utf8',
  );
  const routePlanner = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Entry', 'Bridge', 'BlueprintHelperBridgeRoutePlanner.cpp'),
    'utf8',
  );
  const routePlannerUtils = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Entry', 'Bridge', 'Utils', 'BlueprintHelperBridgeRoutePlannerUtils.cpp'),
    'utf8',
  );
  const generatedCapabilityHeader = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Public', 'Runtime', 'Capabilities', 'BlueprintHelperGeneratedCapabilityRegistry.h'),
    'utf8',
  );
  const generatedUmgHeader = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Generated', 'BlueprintHelperUMGWidgetOperationManifest.generated.h'),
    'utf8',
  );
  const generatedReadContextHeader = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Generated', 'BlueprintHelperReadContextRouteManifest.generated.h'),
    'utf8',
  );
  const systemRegistry = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Entry', 'Bridge', 'BlueprintHelperBridgeSystemCommandRegistry.cpp'),
    'utf8',
  );
  const systemTable = systemRegistry.match(
    /GBlueprintHelperBridgeSystemCommandDescriptors\[\]\s*=\s*\{([\s\S]*?)\};/u,
  )?.[1] ?? '';
  const commandNames = [...systemTable.matchAll(/\{\s*TEXT\("([^"]+)"\)/gu)].map((match) => match[1]);
  const duplicates = commandNames.filter((command, index) => commandNames.indexOf(command) !== index);

  assert.ok(commandNames.length > 0, 'system command registry should be discoverable');
  assert.deepEqual([...new Set(duplicates)].sort(), []);
  assert.doesNotMatch(commandRegistry, /FBlueprintHelperBridgeRoutePlanner::BuildPlan\(Command\)/u);
  assert.doesNotMatch(commandRegistry, /const\s+FString\s+Commands\[\]/u);
  assert.doesNotMatch(commandRegistry, /ResolveCapabilityHandlerCluster/u);
  assert.doesNotMatch(commandRegistry, /HandlerId\s*==\s*TEXT\(/u);
  assert.doesNotMatch(routePlannerUtils, /GBlueprintHelperBridgeRouteCommandClusters/u);
  assert.doesNotMatch(routePlanner, /TEXT\("generated\./u);
  assert.match(generatedCapabilityHeader, /RoutingSourceId/u);
  assert.match(generatedCapabilityHeader, /RoutingPolicyId/u);
  assert.match(generatedCapabilityHeader, /bRoutingAgentVisible/u);
  assert.match(generatedUmgHeader, /RouteSourceId/u);
  assert.match(generatedUmgHeader, /RoutePolicyId/u);
  assert.match(generatedUmgHeader, /bRouteAgentVisible/u);
  assert.match(generatedReadContextHeader, /RouteSourceId/u);
  assert.match(generatedReadContextHeader, /RoutePolicyId/u);
  assert.match(generatedReadContextHeader, /bRouteAgentVisible/u);
  assert.match(routePlanner, /class FBlueprintHelperBridgeRoutePlannerPrivate/u);
  assert.match(routePlanner, /FBlueprintHelperBridgeRoutePlannerPrivate::ResolveGeneratedCapabilityRoute/u);
  assert.doesNotMatch(routePlanner, /^static\s+bool\s+BlueprintHelperResolveGeneratedCapabilityRoute/mu);
  assert.match(routePlanner, /FindGeneratedUMGRouteForCommand/u);
  assert.match(routePlanner, /FindGeneratedReadContextRouteForCommand/u);
  assert.match(routePlanner, /FBlueprintHelperBridgeSystemCommandRegistry::TryFindDescriptor/u);
  assert.doesNotMatch(routePlanner, /BuildRegisteredRuntimeState/u);
});

test('Bridge server accepts clients through socket readiness wait instead of fixed polling', () => {
  const bridgeServer = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Entry', 'Bridge', 'BlueprintHelperBridgeServer.cpp'),
    'utf8',
  );

  assert.match(bridgeServer, /ListenerSocket->WaitForPendingConnection\(/u);
  assert.doesNotMatch(
    bridgeServer,
    /HasPendingConnection[\s\S]*?FPlatformProcess::Sleep\(0\.05f\)/u,
  );
});

test('TaskRuntime cache TTL and capacity defaults live in the cache config boundary', () => {
  const cacheFiles = [
    'Private/Runtime/TaskRuntime/BlueprintHelperTaskPartialPreviewCache.cpp',
    'Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCallFunctionResolutionCache.cpp',
    'Private/Runtime/TaskRuntime/BlueprintHelperGraphWritePlanCache.cpp',
  ]
    .map((relative) => path.resolve(UE_SOURCE_ROOT, relative))
    .filter((filePath) => fs.existsSync(filePath))
    .map((filePath) => fs.readFileSync(filePath, 'utf8'));

  for (const source of cacheFiles) {
    assert.doesNotMatch(source, /FromSeconds\((40|90|180)/u);
    assert.doesNotMatch(source, /Max(Entries|Bytes|Groups)\s*=\s*(64|256|512|2048|8388608|16777216)/u);
  }
});

test('TaskRuntime post operations use planner and executor boundaries', () => {
  const servicePath = path.resolve(
    UE_SOURCE_ROOT,
    'Private',
    'Runtime',
    'TaskRuntime',
    'BlueprintHelperTaskRuntimeService.cpp',
  );
  const pipelineExecutorPath = path.resolve(
    UE_SOURCE_ROOT,
    'Private',
    'Runtime',
    'TaskRuntime',
    'Pipeline',
    'BlueprintHelperTaskRuntimePipelineExecutors.cpp',
  );
  const serviceSource = fs.readFileSync(servicePath, 'utf8');
  const pipelineExecutorSource = fs.readFileSync(pipelineExecutorPath, 'utf8');

  assert.match(pipelineExecutorSource, /FBlueprintHelperTaskRuntimePostOperationPlanner::BuildPlan/u);
  assert.match(pipelineExecutorSource, /FBlueprintHelperTaskRuntimePostOperationExecutor/u);
  assert.doesNotMatch(
    serviceSource,
    /for\s*\(\s*const\s+FString&\s+AssetPath\s*:\s*TargetAssets\s*\)\s*\{\s*FBlueprintHelperToolResultBase\s+CompileResult/su,
  );
  assert.doesNotMatch(
    serviceSource,
    /for\s*\(\s*const\s+FString&\s+AssetPath\s*:\s*TargetAssets\s*\)\s*\{\s*FBlueprintHelperToolResultBase\s+SaveResult/su,
  );
});

test('GraphWrite linker and default applier use GraphWriteContext pin lookup', () => {
  const linker = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Systems', 'ToolClusters', 'GraphWrite', 'Pipeline', 'BlueprintGraphLinker.cpp'),
    'utf8',
  );
  const defaultApplier = fs.readFileSync(
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Systems', 'ToolClusters', 'GraphWrite', 'Pipeline', 'BlueprintGraphDefaultValueApplier.cpp'),
    'utf8',
  );

  assert.match(linker, /FBlueprintGraphWriteContext/u);
  assert.match(defaultApplier, /FBlueprintGraphWriteContext/u);
  assert.doesNotMatch(linker, /FBlueprintGraphNodeUtility::FindPinByAlias/u);
  assert.doesNotMatch(defaultApplier, /FBlueprintGraphNodeUtility::FindPinByAlias/u);
});

test('GraphWrite explicit asset path resolution does not open BlueprintEditor UI', () => {
  const graphResolver = fs.readFileSync(
    path.resolve(
      UE_SOURCE_ROOT,
      'Private',
      'Systems',
      'ToolClusters',
      'GraphWrite',
      'GraphSupport',
      'BlueprintHelperGraphResolver.cpp',
    ),
    'utf8',
  );
  const loadBlueprintByPath = graphResolver.match(
    /UBlueprint\*\s+FBlueprintHelperGraphResolver::LoadBlueprintByPath[\s\S]*?\n[}]/u,
  )?.[0] ?? '';

  assert.ok(loadBlueprintByPath.length > 0, 'LoadBlueprintByPath should be discoverable');
  assert.doesNotMatch(loadBlueprintByPath, /EnsureBlueprintEditorOpen/u);
  assert.doesNotMatch(loadBlueprintByPath, /OpenEditorForAsset/u);
});

test('CLI production task path depends on the compiler service and MCP keeps task fallback removed', () => {
  const productionFiles = [
    ...collectFiles(path.resolve(AGENT_FACE_ROOT, 'cli', 'src'), ['.ts']),
    ...collectFiles(path.resolve(AGENT_FACE_ROOT, 'mcp', 'src', 'mcp'), ['.ts']),
  ];
  const directCompilerUsers = productionFiles
    .filter((filePath) => /compileTaskSpecToTaskPlan/u.test(fs.readFileSync(filePath, 'utf8')))
    .map(toRepoRelativePath);

  assert.deepEqual(directCompilerUsers, []);
  assert.match(
    fs.readFileSync(path.resolve(AGENT_FACE_ROOT, 'cli', 'src', 'cli', 'run.ts'), 'utf8'),
    /createTaskSpecRunner/u,
  );
  assert.equal(
    fs.existsSync(path.resolve(AGENT_FACE_ROOT, 'mcp', 'src', 'mcp', 'tools', 'task-tools.ts')),
    false,
  );
  assert.equal(
    fs.existsSync(path.resolve(AGENT_FACE_ROOT, 'mcp', 'src', 'mcp', 'tools', 'shared-registry-adapter.ts')),
    false,
  );
});

test('GraphWrite runtime does not retain deprecated layout mutation support', () => {
  const productionFiles = [
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Runtime', 'TaskRuntime', 'BlueprintHelperTaskRuntimeService.cpp'),
    path.resolve(UE_SOURCE_ROOT, 'Public', 'Shared', 'GraphWrite', 'BlueprintHelperPatchGraphTypes.h'),
    path.resolve(UE_SOURCE_ROOT, 'Public', 'Systems', 'ToolClusters', 'GraphWrite', 'BlueprintHelperPatchBlueprintGraphService.h'),
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Systems', 'ToolClusters', 'GraphWrite', 'BlueprintHelperPatchBlueprintGraphService.cpp'),
  ];

  for (const filePath of productionFiles) {
    const source = fs.readFileSync(filePath, 'utf8');
    assert.doesNotMatch(source, /set_node_position/u, toRepoRelativePath(filePath));
    assert.doesNotMatch(source, /\bnode_position\b/u, toRepoRelativePath(filePath));
    assert.doesNotMatch(source, /\bSetNodePosition\b/u, toRepoRelativePath(filePath));
    assert.doesNotMatch(source, /\bNodePosition\b/u, toRepoRelativePath(filePath));
    assert.doesNotMatch(source, /DEPRECATED_LAYOUT/u, toRepoRelativePath(filePath));
  }
});

test('Review panel command service does not retain archive-baseline reject fallback', () => {
  const filePath = path.resolve(
    UE_SOURCE_ROOT,
    'Private',
    'UI',
    'Review',
    'BlueprintHelperReviewPanelCommandService.cpp',
  );
  const source = fs.readFileSync(filePath, 'utf8');

  assert.doesNotMatch(source, /Reject requires archive-baseline rollback service/u);
  assert.doesNotMatch(source, /RollbackMode\s*=\s*TEXT\("archive_baseline"\)/u);
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
