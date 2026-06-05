import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const taskCoreRoot = path.resolve(scriptDir, '..');
const pluginRoot = path.resolve(taskCoreRoot, '..', '..');
const sourceRelativePath = 'src/task/compiler/graphwrite/graphwrite-route-source.json';
const manifestRelativePath = 'src/task/compiler/graphwrite/generated/graphwrite-route-manifest.generated.ts';
const syncRelativePath = 'src/task/compiler/graphwrite/generated/graphwrite-ue-adapter-sync.generated.json';
const ueMirrorRelativePath = 'BlueprintHelper/Source/BlueprintHelper/Private/Generated/BlueprintHelperGraphWriteRouteAdapterSync.generated.json';
const checkOnly = process.argv.includes('--check');

const sourcePath = path.join(taskCoreRoot, sourceRelativePath);
const manifestPath = path.join(taskCoreRoot, manifestRelativePath);
const syncPath = path.join(taskCoreRoot, syncRelativePath);
const ueMirrorPath = path.join(pluginRoot, ueMirrorRelativePath);

const source = readJson(sourcePath);
if (source.schema !== 'BlueprintHelper.GraphWriteRouteSource.v1' || !Array.isArray(source.routes)) {
  throw new Error(`Invalid GraphWrite route source: ${sourcePath}`);
}

const routes = [...source.routes].sort((a, b) => String(a.route_id).localeCompare(String(b.route_id)));
validateRoutes(routes);

const manifestText = renderManifest(routes);
const syncText = `${JSON.stringify({
  schema: 'BlueprintHelper.GraphWriteRouteAdapterSync.v1',
  generated_from: sourceRelativePath,
  routes: routes.map(toSyncEntry),
}, null, 2)}\n`;
const ueMirrorText = `${JSON.stringify({
  schema: 'BlueprintHelper.GraphWriteRouteAdapterSync.v1',
  generated_from: `AgentFaceService/task-core/${sourceRelativePath}`,
  routes: routes.map(toSyncEntry),
}, null, 2)}\n`;

const outputs = [
  { path: manifestPath, text: manifestText },
  { path: syncPath, text: syncText },
  { path: ueMirrorPath, text: ueMirrorText },
];

if (checkOnly) {
  const drifted = outputs.filter((output) => !fs.existsSync(output.path) || fs.readFileSync(output.path, 'utf8') !== output.text);
  if (drifted.length > 0) {
    for (const output of drifted) {
      console.error(`Generated GraphWrite route artifact is stale or missing: ${path.relative(pluginRoot, output.path)}`);
    }
    process.exit(1);
  }
  console.log('Generated GraphWrite route artifacts are current.');
} else {
  for (const output of outputs) {
    fs.mkdirSync(path.dirname(output.path), { recursive: true });
    fs.writeFileSync(output.path, output.text, 'utf8');
  }
  console.log('Generated GraphWrite route artifacts.');
}

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function validateRoutes(routes) {
  const ids = new Set();
  const behaviorFieldByStrategy = new Map();
  for (const route of routes) {
    for (const field of ['route_id', 'task_type', 'graph_strategy', 'public_scope', 'behavior_field', 'taskplan_op', 'runtime_adapter_id', 'compiler_id', 'status']) {
      if (typeof route[field] !== 'string' || route[field].length === 0) {
        throw new Error(`GraphWrite route ${route.route_id ?? '<unknown>'} has invalid ${field}.`);
      }
    }
    if (ids.has(route.route_id)) {
      throw new Error(`Duplicate GraphWrite route_id: ${route.route_id}`);
    }
    ids.add(route.route_id);
    if (!['active', 'planned', 'hidden'].includes(route.status)) {
      throw new Error(`GraphWrite route ${route.route_id} has invalid status: ${route.status}`);
    }
    const existingBehaviorField = behaviorFieldByStrategy.get(route.graph_strategy);
    if (existingBehaviorField && existingBehaviorField !== route.behavior_field) {
      throw new Error(`GraphWrite strategy ${route.graph_strategy} has inconsistent behavior_field values.`);
    }
    behaviorFieldByStrategy.set(route.graph_strategy, route.behavior_field);
    if (route.status === 'active' && (typeof route.template_path !== 'string' || route.template_path.length === 0)) {
      throw new Error(`Active GraphWrite route ${route.route_id} requires template_path.`);
    }
    for (const arrayField of ['required_fields', 'optional_fields', 'insert_paths', 'allowed_slot_ids']) {
      if (!Array.isArray(route[arrayField]) || route[arrayField].some((entry) => typeof entry !== 'string')) {
        throw new Error(`GraphWrite route ${route.route_id} has invalid ${arrayField}.`);
      }
    }
    if (route.selector) {
      if (typeof route.selector.expected_kind !== 'string' || !Array.isArray(route.selector.required_fields)) {
        throw new Error(`GraphWrite route ${route.route_id} has invalid selector.`);
      }
      if (!route.selector.output_fields || typeof route.selector.output_fields !== 'object' || Array.isArray(route.selector.output_fields)) {
        throw new Error(`GraphWrite route ${route.route_id} has invalid selector.output_fields.`);
      }
    }
  }
}

function toSyncEntry(route) {
  return {
    route_id: route.route_id,
    runtime_adapter_id: route.runtime_adapter_id,
    graph_strategy: route.graph_strategy,
    public_scope: route.public_scope,
    behavior_field: route.behavior_field,
    compiler_id: route.compiler_id,
    taskplan_op: route.taskplan_op,
    status: route.status,
    adapter_sync: route.status === 'active' ? 'generated_active_stub' : route.status,
  };
}

function renderManifest(routes) {
  return `// <auto-generated>\n`
    + `// Generated by scripts/generate-graphwrite-route-manifest.mjs. Do not edit by hand.\n`
    + `// </auto-generated>\n`
    + `import type { GraphWriteRouteDescriptor } from '../graphwrite-route-descriptor.js';\n\n`
    + `export const GRAPHWRITE_ROUTE_MANIFEST_SCHEMA = 'BlueprintHelper.GraphWriteRouteManifest.v1';\n`
    + `export const GRAPHWRITE_ROUTE_MANIFEST_GENERATED_FROM = '${sourceRelativePath}';\n\n`
    + `export const GRAPHWRITE_ROUTE_MANIFEST = ${JSON.stringify(routes, null, 2)} as const satisfies readonly GraphWriteRouteDescriptor[];\n`;
}
