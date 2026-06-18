import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import ts from 'typescript';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const taskCoreRoot = path.resolve(scriptDir, '..');
const pluginRoot = path.resolve(taskCoreRoot, '..', '..');
const sourceRelativePath = 'src/tool-surface/templates/read-context-template-registry.ts';
const manifestRelativePath = 'src/tool-surface/templates/generated/read-context-route-manifest.generated.ts';
const ueMirrorRelativePath = 'BlueprintHelper/Source/BlueprintHelper/Private/Generated/BlueprintHelperReadContextRouteManifest.generated.h';
const checkOnly = process.argv.includes('--check');

const sourcePath = path.join(taskCoreRoot, sourceRelativePath);
const manifestPath = path.join(taskCoreRoot, manifestRelativePath);
const ueMirrorPath = path.join(pluginRoot, ueMirrorRelativePath);

const sourceModule = await importTranspiledTs(sourcePath);
const routes = [...sourceModule.getAllReadContextRouteDescriptors()]
  .map(normalizeRoute)
  .sort((a, b) => a.template_id.localeCompare(b.template_id));
validateRoutes(routes);

const manifestText = renderTsManifest(routes);
const ueMirrorText = renderUeHeader(routes);
const outputs = [
  { path: manifestPath, text: manifestText },
  { path: ueMirrorPath, text: ueMirrorText },
];

if (checkOnly) {
  const drifted = outputs.filter((output) => !fs.existsSync(output.path) || fs.readFileSync(output.path, 'utf8') !== output.text);
  if (drifted.length > 0) {
    for (const output of drifted) {
      console.error(`Generated ReadContext route artifact is stale or missing: ${path.relative(pluginRoot, output.path)}`);
    }
    process.exit(1);
  }
  console.log('Generated ReadContext route artifacts are current.');
} else {
  for (const output of outputs) {
    fs.mkdirSync(path.dirname(output.path), { recursive: true });
    fs.writeFileSync(output.path, output.text, 'utf8');
  }
  console.log('Generated ReadContext route artifacts.');
}

async function importTranspiledTs(filePath) {
  const sourceText = fs.readFileSync(filePath, 'utf8');
  const transpiled = ts.transpileModule(sourceText, {
    compilerOptions: {
      target: ts.ScriptTarget.ES2022,
      module: ts.ModuleKind.ES2022,
      importsNotUsedAsValues: ts.ImportsNotUsedAsValues.Remove,
    },
    fileName: filePath,
  }).outputText;
  const tempDir = path.join(taskCoreRoot, 'build', '.generated-script-cache');
  fs.mkdirSync(tempDir, { recursive: true });
  const tempFile = path.join(tempDir, `${path.basename(filePath, '.ts')}.${Date.now()}.mjs`);
  fs.writeFileSync(tempFile, transpiled, 'utf8');
  try {
    return await import(pathToFileURL(tempFile).href);
  } finally {
    fs.rmSync(tempFile, { force: true });
  }
}

function normalizeRoute(route) {
  return {
    ...route,
    required_target_fields: cloneStringArray(route.required_target_fields),
    supported_asset_types: cloneStringArray(route.supported_asset_types),
    supported_formats: cloneStringArray(route.supported_formats),
  };
}

function cloneStringArray(values) {
  if (!Array.isArray(values)) {
    return values;
  }
  return [...values];
}

function validateRoutes(routes) {
  const ids = new Set();
  for (const route of routes) {
    for (const field of [
      'template_id',
      'family',
      'cluster',
      'description',
      'read_type',
      'payload_schema',
      'output_schema',
      'request_builder_id',
      'payload_projector_id',
      'route_cluster',
      'route_source_id',
      'route_policy_id',
      'status',
    ]) {
      if (typeof route[field] !== 'string' || route[field].length === 0) {
        throw new Error(`ReadContext route ${route.template_id ?? '<unknown>'} has invalid ${field}.`);
      }
    }
    if (ids.has(route.template_id)) {
      throw new Error(`Duplicate ReadContext template_id: ${route.template_id}`);
    }
    ids.add(route.template_id);
    if (!['active', 'reserved'].includes(route.status)) {
      throw new Error(`ReadContext route ${route.template_id} has invalid status: ${route.status}`);
    }
    if (typeof route.route_agent_visible !== 'boolean') {
      throw new Error(`ReadContext route ${route.template_id} has invalid route_agent_visible.`);
    }
    if (typeof route.route_requires_game_thread !== 'boolean') {
      throw new Error(`ReadContext route ${route.template_id} has invalid route_requires_game_thread.`);
    }
    for (const arrayField of ['required_fields', 'optional_fields', 'stop_conditions', 'supported_asset_types', 'supported_formats']) {
      if (!Array.isArray(route[arrayField]) || route[arrayField].some((entry) => typeof entry !== 'string')) {
        throw new Error(`ReadContext route ${route.template_id} has invalid ${arrayField}.`);
      }
    }
    if (!route.read_spec || typeof route.read_spec !== 'object') {
      throw new Error(`ReadContext route ${route.template_id} has invalid read_spec.`);
    }
    if (route.status === 'active') {
      if (typeof route.template_path !== 'string' || route.template_path.length === 0) {
        throw new Error(`Active ReadContext route ${route.template_id} requires template_path.`);
      }
      if (typeof route.target_type !== 'string' || route.target_type.length === 0) {
        throw new Error(`Active ReadContext route ${route.template_id} requires target_type.`);
      }
      if (typeof route.bridge_command !== 'string' || route.bridge_command.length === 0) {
        throw new Error(`Active ReadContext route ${route.template_id} requires bridge_command.`);
      }
    }
  }
}

function renderTsManifest(routes) {
  return `// <auto-generated>\n`
    + `// Generated by scripts/generate-read-context-route-manifest.mjs. Do not edit by hand.\n`
    + `// </auto-generated>\n`
    + `import type { ReadContextRouteDescriptor } from '../read-context-template-types.js';\n\n`
    + `export const READ_CONTEXT_ROUTE_MANIFEST_SCHEMA = 'BlueprintHelper.ReadContextRouteManifest.v1';\n`
    + `export const READ_CONTEXT_ROUTE_MANIFEST_GENERATED_FROM = '${sourceRelativePath}';\n\n`
    + `export const READ_CONTEXT_ROUTE_MANIFEST = ${JSON.stringify(routes, null, 2)} as const satisfies readonly ReadContextRouteDescriptor[];\n`;
}

function renderUeHeader(routes) {
  const rows = routes.map((route) =>
    `\t{TEXT("${escapeCppString(route.template_id)}"), TEXT("${escapeCppString(route.bridge_command ?? '')}"), TEXT("${escapeCppString(route.route_cluster)}"), TEXT("${escapeCppString(route.route_source_id)}"), TEXT("${escapeCppString(route.route_policy_id)}"), ${route.route_agent_visible ? 'true' : 'false'}, ${route.route_requires_game_thread ? 'true' : 'false'}, TEXT("${escapeCppString(route.family)}"), TEXT("${escapeCppString(route.cluster)}"), TEXT("${escapeCppString(route.target_type ?? '')}"), TEXT("${escapeCppString(route.format ?? '')}"), TEXT("${escapeCppString(route.status)}")}`);
  return `// <auto-generated>\n`
    + `// Generated by AgentFaceService/task-core/scripts/generate-read-context-route-manifest.mjs. Do not edit by hand.\n`
    + `// </auto-generated>\n`
    + `#pragma once\n\n`
    + `#include "CoreMinimal.h"\n\n`
    + `struct FBlueprintHelperGeneratedReadContextRouteDescriptor\n`
    + `{\n`
    + `\tconst TCHAR* TemplateId;\n`
    + `\tconst TCHAR* Command;\n`
    + `\tconst TCHAR* Cluster;\n`
    + `\tconst TCHAR* RouteSourceId;\n`
    + `\tconst TCHAR* RoutePolicyId;\n`
    + `\tbool bRouteAgentVisible;\n`
    + `\tbool bRouteRequiresGameThread;\n`
    + `\tconst TCHAR* Family;\n`
    + `\tconst TCHAR* ReadCluster;\n`
    + `\tconst TCHAR* TargetType;\n`
    + `\tconst TCHAR* Format;\n`
    + `\tconst TCHAR* Status;\n`
    + `};\n\n`
    + `static const FBlueprintHelperGeneratedReadContextRouteDescriptor GBlueprintHelperReadContextRoutes[] = {\n`
    + rows.join(',\n')
    + `\n};\n\n`
    + `static constexpr int32 GBlueprintHelperReadContextRouteCount = UE_ARRAY_COUNT(GBlueprintHelperReadContextRoutes);\n`;
}

function escapeCppString(value) {
  return String(value).replace(/\\/g, '\\\\').replace(/"/g, '\\"');
}
