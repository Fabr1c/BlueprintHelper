import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import ts from 'typescript';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const taskCoreRoot = path.resolve(scriptDir, '..');
const pluginRoot = path.resolve(taskCoreRoot, '..', '..');
const sourceRelativePath = 'src/tool-surface/templates/umg-widget-operation-descriptors.ts';
const manifestRelativePath = 'src/tool-surface/templates/generated/umg-widget-operation-manifest.generated.ts';
const ueMirrorRelativePath = 'BlueprintHelper/Source/BlueprintHelper/Private/Generated/BlueprintHelperUMGWidgetOperationManifest.generated.h';
const checkOnly = process.argv.includes('--check');

const sourcePath = path.join(taskCoreRoot, sourceRelativePath);
const manifestPath = path.join(taskCoreRoot, manifestRelativePath);
const ueMirrorPath = path.join(pluginRoot, ueMirrorRelativePath);

const sourceModule = await importTranspiledTs(sourcePath);
const descriptors = [...sourceModule.UMG_WIDGET_OPERATION_DESCRIPTORS]
  .map(normalizeDescriptor)
  .sort((a, b) => a.kind.localeCompare(b.kind));
validateDescriptors(descriptors);

const manifestText = renderTsManifest(descriptors);
const ueMirrorText = renderUeHeader(descriptors);
const outputs = [
  { path: manifestPath, text: manifestText },
  { path: ueMirrorPath, text: ueMirrorText },
];

if (checkOnly) {
  const drifted = outputs.filter((output) => !fs.existsSync(output.path) || fs.readFileSync(output.path, 'utf8') !== output.text);
  if (drifted.length > 0) {
    for (const output of drifted) {
      console.error(`Generated UMG widget operation artifact is stale or missing: ${path.relative(pluginRoot, output.path)}`);
    }
    process.exit(1);
  }
  console.log('Generated UMG widget operation artifacts are current.');
} else {
  for (const output of outputs) {
    fs.mkdirSync(path.dirname(output.path), { recursive: true });
    fs.writeFileSync(output.path, output.text, 'utf8');
  }
  console.log('Generated UMG widget operation artifacts.');
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

function normalizeDescriptor(descriptor) {
  return {
    ...descriptor,
    required_fields: cloneStringArray(descriptor.required_fields),
    optional_fields: cloneStringArray(descriptor.optional_fields),
    string_enum_fields: cloneStringArrayRecord(descriptor.string_enum_fields),
  };
}

function cloneStringArray(values) {
  if (!Array.isArray(values)) {
    return values;
  }
  return [...values];
}

function cloneStringArrayRecord(record) {
  if (record === undefined) {
    return undefined;
  }
  if (typeof record !== 'object' || record === null || Array.isArray(record)) {
    return record;
  }
  return Object.fromEntries(Object.entries(record).map(([key, values]) => [key, cloneStringArray(values)]));
}

function validateDescriptors(descriptors) {
  const ids = new Set();
  const taskplanOps = new Set();
  for (const descriptor of descriptors) {
    for (const field of [
      'kind',
      'taskplan_op',
      'bridge_command',
      'taskplan_strategy',
      'review_target_subkind',
      'readback_view',
      'planned_preview_effect',
      'validation_classification',
      'status',
    ]) {
      if (typeof descriptor[field] !== 'string' || descriptor[field].length === 0) {
        throw new Error(`UMG widget operation ${descriptor.kind ?? '<unknown>'} has invalid ${field}.`);
      }
    }
    if (ids.has(descriptor.kind)) {
      throw new Error(`Duplicate UMG widget operation kind: ${descriptor.kind}`);
    }
    ids.add(descriptor.kind);
    if (taskplanOps.has(descriptor.taskplan_op)) {
      throw new Error(`Duplicate UMG widget taskplan operation: ${descriptor.taskplan_op}`);
    }
    taskplanOps.add(descriptor.taskplan_op);
    if (!['widget_tree_edit', 'widget_property_edit', 'widget_blueprint_class_edit'].includes(descriptor.taskplan_strategy)) {
      throw new Error(`UMG widget operation ${descriptor.kind} has invalid taskplan_strategy: ${descriptor.taskplan_strategy}`);
    }
    if (descriptor.status !== 'active') {
      throw new Error(`UMG widget operation ${descriptor.kind} must be active for this closure.`);
    }
    if (!['tree_json', 'property_json'].includes(descriptor.readback_view)) {
      throw new Error(`UMG widget operation ${descriptor.kind} has invalid readback_view: ${descriptor.readback_view}`);
    }
    if (!['widget_tree_structural', 'widget_property', 'widget_metadata', 'widget_blueprint_class'].includes(descriptor.planned_preview_effect)) {
      throw new Error(`UMG widget operation ${descriptor.kind} has invalid planned_preview_effect: ${descriptor.planned_preview_effect}`);
    }
    if (!['preview_decidable', 'runtime_only', 'shared_policy'].includes(descriptor.validation_classification)) {
      throw new Error(`UMG widget operation ${descriptor.kind} has invalid validation_classification: ${descriptor.validation_classification}`);
    }
    for (const arrayField of ['required_fields', 'optional_fields']) {
      if (!Array.isArray(descriptor[arrayField]) || descriptor[arrayField].some((entry) => typeof entry !== 'string' || entry.length === 0)) {
        throw new Error(`UMG widget operation ${descriptor.kind} has invalid ${arrayField}.`);
      }
    }
    const duplicatedField = descriptor.required_fields.find((field) => descriptor.optional_fields.includes(field));
    if (duplicatedField) {
      throw new Error(`UMG widget operation ${descriptor.kind} repeats field in required and optional lists: ${duplicatedField}`);
    }
    if (descriptor.string_enum_fields !== undefined) {
      if (typeof descriptor.string_enum_fields !== 'object' || descriptor.string_enum_fields === null || Array.isArray(descriptor.string_enum_fields)) {
        throw new Error(`UMG widget operation ${descriptor.kind} has invalid string_enum_fields.`);
      }
      const allowedFields = new Set([...descriptor.required_fields, ...descriptor.optional_fields]);
      for (const [field, values] of Object.entries(descriptor.string_enum_fields)) {
        if (!allowedFields.has(field)) {
          throw new Error(`UMG widget operation ${descriptor.kind} has enum rule for unknown field: ${field}`);
        }
        if (!Array.isArray(values) || values.length === 0 || values.some((value) => typeof value !== 'string' || value.length === 0)) {
          throw new Error(`UMG widget operation ${descriptor.kind} has invalid enum values for field: ${field}`);
        }
      }
    }
  }
}

function renderTsManifest(descriptors) {
  return `// <auto-generated>\n`
    + `// Generated by scripts/generate-umg-widget-operation-manifest.mjs. Do not edit by hand.\n`
    + `// </auto-generated>\n`
    + `import type { UmgWidgetOperationDescriptor } from '../umg-widget-operation-descriptors.js';\n\n`
    + `export const UMG_WIDGET_OPERATION_MANIFEST_SCHEMA = 'BlueprintHelper.UMGWidgetOperationManifest.v1';\n`
    + `export const UMG_WIDGET_OPERATION_MANIFEST_GENERATED_FROM = '${sourceRelativePath}';\n\n`
    + `export const UMG_WIDGET_OPERATION_MANIFEST = ${JSON.stringify(descriptors, null, 2)} as const satisfies readonly UmgWidgetOperationDescriptor[];\n`;
}

function renderUeHeader(descriptors) {
  const rows = descriptors.map((descriptor) =>
    `\t{TEXT("${escapeCppString(descriptor.bridge_command)}"), TEXT("UMGWidget"), TEXT("${escapeCppString(descriptor.required_fields.join(','))}"), TEXT("${escapeCppString(descriptor.optional_fields.join(','))}"), TEXT("${escapeCppString(renderStringEnumFields(descriptor.string_enum_fields))}"), true, TEXT("${escapeCppString(descriptor.kind)}"), TEXT("${escapeCppString(descriptor.taskplan_op)}"), TEXT("${escapeCppString(descriptor.taskplan_strategy)}"), TEXT("${escapeCppString(descriptor.review_target_subkind)}"), TEXT("${escapeCppString(descriptor.readback_view)}"), TEXT("${escapeCppString(descriptor.planned_preview_effect)}")}`);
  return `// <auto-generated>\n`
    + `// Generated by AgentFaceService/task-core/scripts/generate-umg-widget-operation-manifest.mjs. Do not edit by hand.\n`
    + `// </auto-generated>\n`
    + `#pragma once\n\n`
    + `#include "CoreMinimal.h"\n\n`
    + `struct FBlueprintHelperGeneratedCommandDescriptor\n`
    + `{\n`
    + `\tconst TCHAR* Command;\n`
    + `\tconst TCHAR* Cluster;\n`
    + `\tconst TCHAR* RequiredFields;\n`
    + `\tconst TCHAR* OptionalFields;\n`
    + `\tconst TCHAR* StringEnumFields;\n`
    + `\tbool bWriteCommand;\n`
    + `\tconst TCHAR* OperationKind;\n`
    + `\tconst TCHAR* TaskPlanOp;\n`
    + `\tconst TCHAR* TaskPlanStrategy;\n`
    + `\tconst TCHAR* ReviewTargetSubkind;\n`
    + `\tconst TCHAR* ReadbackView;\n`
    + `\tconst TCHAR* PlannedPreviewEffect;\n`
    + `};\n\n`
    + `static const FBlueprintHelperGeneratedCommandDescriptor GBlueprintHelperUMGWidgetOperationCommands[] = {\n`
    + rows.join(',\n')
    + `\n};\n\n`
    + `static constexpr int32 GBlueprintHelperUMGWidgetOperationCommandCount = UE_ARRAY_COUNT(GBlueprintHelperUMGWidgetOperationCommands);\n`;
}

function escapeCppString(value) {
  return String(value).replace(/\\/g, '\\\\').replace(/"/g, '\\"');
}

function renderStringEnumFields(record) {
  if (record === undefined) {
    return '';
  }
  return Object.entries(record)
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([field, values]) => `${field}=${values.join('|')}`)
    .join(';');
}
