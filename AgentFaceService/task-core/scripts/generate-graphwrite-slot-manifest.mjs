import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const taskCoreRoot = path.resolve(scriptDir, '..');
const pluginRoot = path.resolve(taskCoreRoot, '..', '..');
const sourceRelativePath = 'src/task/compiler/graphwrite/graphwrite-slot-source.json';
const routeSourceRelativePath = 'src/task/compiler/graphwrite/graphwrite-route-source.json';
const manifestRelativePath = 'src/task/compiler/graphwrite/generated/graphwrite-slot-manifest.generated.ts';
const checkOnly = process.argv.includes('--check');

const sourcePath = path.join(taskCoreRoot, sourceRelativePath);
const routeSourcePath = path.join(taskCoreRoot, routeSourceRelativePath);
const manifestPath = path.join(taskCoreRoot, manifestRelativePath);

const source = readJson(sourcePath);
if (source.schema !== 'BlueprintHelper.GraphWriteSlotSource.v1' || !Array.isArray(source.slots)) {
  throw new Error(`Invalid GraphWrite slot source: ${sourcePath}`);
}

const routeSource = readJson(routeSourcePath);
if (routeSource.schema !== 'BlueprintHelper.GraphWriteRouteSource.v1' || !Array.isArray(routeSource.routes)) {
  throw new Error(`Invalid GraphWrite route source: ${routeSourcePath}`);
}

const visibleRouteIds = new Set(
  routeSource.routes
    .filter((route) => route.status === 'active' && typeof route.template_path === 'string' && route.template_path.length > 0)
    .map((route) => route.route_id),
);
const slots = [...source.slots]
  .map(normalizeSlot)
  .sort((a, b) => a.slot_id.localeCompare(b.slot_id));
validateSlots(slots, visibleRouteIds);

const manifestText = renderManifest(slots);

if (checkOnly) {
  if (!fs.existsSync(manifestPath) || fs.readFileSync(manifestPath, 'utf8') !== manifestText) {
    console.error(`Generated GraphWrite slot manifest is stale or missing: ${path.relative(pluginRoot, manifestPath)}`);
    process.exit(1);
  }
  console.log('Generated GraphWrite slot manifest is current.');
} else {
  fs.mkdirSync(path.dirname(manifestPath), { recursive: true });
  fs.writeFileSync(manifestPath, manifestText, 'utf8');
  console.log('Generated GraphWrite slot manifest.');
}

function readJson(filePath) {
  return JSON.parse(stripJsonTextBom(fs.readFileSync(filePath, 'utf8')));
}

function stripJsonTextBom(text) {
  return text.startsWith('\uFEFF') ? text.slice(1) : text;
}

function normalizeSlot(slot) {
  return {
    ...slot,
    insert_paths: cloneArray(slot.insert_paths),
    input_slots: Array.isArray(slot.input_slots)
      ? slot.input_slots.map((input) => ({
        ...input,
        accepts: cloneArray(input.accepts),
      }))
      : slot.input_slots,
    supported_routes: cloneArray(slot.supported_routes),
    validation_hints: cloneArray(slot.validation_hints),
    keywords: cloneArray(slot.keywords),
    tags: slot.tags === undefined ? undefined : cloneArray(slot.tags),
  };
}

function cloneArray(values) {
  if (!Array.isArray(values)) {
    return values;
  }
  return [...values];
}

function validateSlots(slots, visibleRouteIds) {
  const ids = new Set();
  for (const slot of slots) {
    for (const field of ['slot_id', 'slot_type', 'compiler_id', 'kind', 'template_path', 'when_to_use', 'status']) {
      if (typeof slot[field] !== 'string' || slot[field].length === 0) {
        throw new Error(`GraphWrite slot ${slot.slot_id ?? '<unknown>'} has invalid ${field}.`);
      }
    }
    if (ids.has(slot.slot_id)) {
      throw new Error(`Duplicate GraphWrite slot_id: ${slot.slot_id}`);
    }
    ids.add(slot.slot_id);
    if (!['statement', 'expression'].includes(slot.slot_type)) {
      throw new Error(`GraphWrite slot ${slot.slot_id} has invalid slot_type: ${slot.slot_type}`);
    }
    if (!['active', 'planned', 'hidden'].includes(slot.status)) {
      throw new Error(`GraphWrite slot ${slot.slot_id} has invalid status: ${slot.status}`);
    }
    validateQuickAccess(slot);
    for (const arrayField of ['insert_paths', 'supported_routes', 'validation_hints', 'keywords']) {
      if (!Array.isArray(slot[arrayField]) || slot[arrayField].some((entry) => typeof entry !== 'string' || entry.length === 0)) {
        throw new Error(`GraphWrite slot ${slot.slot_id} has invalid ${arrayField}.`);
      }
    }
    if (slot.tags !== undefined &&
      (!Array.isArray(slot.tags) || slot.tags.some((entry) => typeof entry !== 'string' || entry.length === 0)))
    {
      throw new Error(`GraphWrite slot ${slot.slot_id} has invalid tags.`);
    }
    if (slot.status === 'active') {
      if (slot.supported_routes.length === 0) {
        throw new Error(`Active GraphWrite slot ${slot.slot_id} must support at least one visible route.`);
      }
      if (!fs.existsSync(path.join(pluginRoot, slot.template_path))) {
        throw new Error(`Active GraphWrite slot ${slot.slot_id} points to a missing template: ${slot.template_path}`);
      }
      for (const routeId of slot.supported_routes) {
        if (!visibleRouteIds.has(routeId)) {
          throw new Error(`Active GraphWrite slot ${slot.slot_id} exposes hidden, planned, or unknown route: ${routeId}`);
        }
      }
    }
    validateInputSlots(slot);
  }
}

function validateInputSlots(slot) {
  if (!Array.isArray(slot.input_slots)) {
    throw new Error(`GraphWrite slot ${slot.slot_id} requires input_slots array.`);
  }
  const inputIndexes = new Set();
  for (const input of slot.input_slots) {
    if (!Number.isInteger(input.index) || input.index < 0) {
      throw new Error(`GraphWrite slot ${slot.slot_id} has invalid input_slots.index.`);
    }
    if (inputIndexes.has(input.index)) {
      throw new Error(`GraphWrite slot ${slot.slot_id} has duplicate input_slots index ${input.index}.`);
    }
    inputIndexes.add(input.index);
    for (const field of ['name', 'path']) {
      if (typeof input[field] !== 'string' || input[field].length === 0) {
        throw new Error(`GraphWrite slot ${slot.slot_id} has invalid input_slots.${field}.`);
      }
    }
    if (!Array.isArray(input.accepts) || input.accepts.length === 0) {
      throw new Error(`GraphWrite slot ${slot.slot_id} input ${input.index} requires accepts.`);
    }
    for (const accepts of input.accepts) {
      if (accepts !== 'expression' && accepts !== 'statement[]') {
        throw new Error(`GraphWrite slot ${slot.slot_id} input ${input.index} has invalid accepts: ${accepts}`);
      }
    }
    if (input.type_hint !== undefined && typeof input.type_hint !== 'string') {
      throw new Error(`GraphWrite slot ${slot.slot_id} input ${input.index} has invalid type_hint.`);
    }
  }
}

function validateQuickAccess(slot) {
  const quickAccess = slot.quick_access;
  if (!quickAccess || typeof quickAccess !== 'object' || Array.isArray(quickAccess)) {
    throw new Error(`GraphWrite slot ${slot.slot_id} requires quick_access metadata.`);
  }
  for (const field of ['template_id', 'family', 'cluster_id', 'operation_id', 'quick_access_id']) {
    if (typeof quickAccess[field] !== 'string' || quickAccess[field].length === 0) {
      throw new Error(`GraphWrite slot ${slot.slot_id} has invalid quick_access.${field}.`);
    }
  }
  if (quickAccess.family !== 'graph_write') {
    throw new Error(`GraphWrite slot ${slot.slot_id} has invalid quick_access.family: ${quickAccess.family}`);
  }
  if (!Array.isArray(quickAccess.unsupported_write_modes)) {
    return;
  }
  for (const writeMode of quickAccess.unsupported_write_modes) {
    if (!['graph.append', 'graph.replace', 'graph.merge', 'graph.patch'].includes(writeMode)) {
      throw new Error(`GraphWrite slot ${slot.slot_id} has invalid unsupported write mode: ${writeMode}`);
    }
  }
}

function renderManifest(slots) {
  return `// <auto-generated>\n`
    + `// Generated by scripts/generate-graphwrite-slot-manifest.mjs. Do not edit by hand.\n`
    + `// </auto-generated>\n`
    + `import type { GraphWriteSlotDescriptor } from '../graphwrite-slot-descriptor.js';\n\n`
    + `export const GRAPHWRITE_SLOT_MANIFEST_SCHEMA = 'BlueprintHelper.GraphWriteSlotManifest.v1';\n`
    + `export const GRAPHWRITE_SLOT_MANIFEST_GENERATED_FROM = '${sourceRelativePath}';\n\n`
    + `export const GRAPHWRITE_SLOT_MANIFEST = ${JSON.stringify(slots, null, 2)} as const satisfies readonly GraphWriteSlotDescriptor[];\n`;
}
