import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { getNonGraphWriteTemplateFamily } from './non-graphwrite-template-metadata.js';
import { composeSlotExpressionTemplate } from './slot-expression-composer.js';
import {
  listTaskSpecTemplateClusters,
  listTaskSpecTemplateFamilies,
  listTaskSpecTemplateOperations,
  listTaskSpecTemplateQuickAccess,
  listTaskSpecTemplateWriteModes,
} from './taskspec-template-index.js';
import type {
  ComposeTaskSpecTemplateInput,
  GraphWriteTemplateWriteMode,
  TaskSpecTemplateCompositionResult,
  TaskSpecTemplateDiagnostic,
} from './taskspec-template-types.js';

export {
  listTaskSpecTemplateClusters,
  listTaskSpecTemplateFamilies,
  listTaskSpecTemplateOperations,
  listTaskSpecTemplateQuickAccess,
  listTaskSpecTemplateWriteModes,
};

export function composeTaskSpecTemplate(input: ComposeTaskSpecTemplateInput): TaskSpecTemplateCompositionResult {
  if (input.family === 'graph_write') {
    return composeGraphWriteTaskSpecTemplate(input);
  }
  return composeNonGraphWriteTaskSpecTemplate(input);
}

function composeGraphWriteTaskSpecTemplate(input: ComposeTaskSpecTemplateInput): TaskSpecTemplateCompositionResult {
  const writeMode = input.writeMode;
  const writeModeItem = listTaskSpecTemplateWriteModes({ family: 'graph_write' })
    .items
    .find((item) => item.write_mode === writeMode);
  if (!writeModeItem || !isGraphWriteTemplateWriteMode(writeMode)) {
    return failed(input, [{
      code: 'unsupported_write_mode',
      family: input.family,
      write_mode: writeMode,
      message: `Unsupported GraphWrite template write mode: ${writeMode}`,
    }]);
  }

  const quickAccessCatalog = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: '',
    operation: '',
    writeMode: '',
  }).items;
  const diagnostics: TaskSpecTemplateDiagnostic[] = [];
  const statements: unknown[] = [];
  for (const templateId of input.templateIds) {
    const composed = composeSlotExpressionTemplate({
      expression: templateId,
      writeMode,
      quickAccessCatalog,
    });
    if (!composed.ok) {
      diagnostics.push(...composed.diagnostics.map((diagnostic) => ({
        ...diagnostic,
        family: input.family,
        write_mode: writeMode,
      })));
      continue;
    }
    statements.push(composed.value);
  }

  if (diagnostics.length > 0) {
    return failed(input, diagnostics);
  }

  const taskSpec = readJson(pluginPath(writeModeItem.base_template_path)) as Record<string, unknown>;
  if (writeMode !== 'graph.patch') {
    const target = getGraphWriteStatementTarget(taskSpec, writeMode);
    target.length = 0;
    target.push(...statements);
  }
  writeJson(input.outputPath, taskSpec);
  return ok(input);
}

function composeNonGraphWriteTaskSpecTemplate(input: ComposeTaskSpecTemplateInput): TaskSpecTemplateCompositionResult {
  const family = getNonGraphWriteTemplateFamily(input.family);
  if (!family) {
    return failed(input, [{
      code: 'unsupported_family',
      family: input.family,
      write_mode: input.writeMode,
    }]);
  }
  if (family.status !== 'supported' || !family.write_mode) {
    return failed(input, [{
      code: 'family_not_composable',
      family: input.family,
      write_mode: input.writeMode,
      message: family.blocked_until?.join('; '),
    }]);
  }
  if (family.write_mode !== input.writeMode) {
    return failed(input, [{
      code: 'unsupported_write_mode',
      family: input.family,
      write_mode: input.writeMode,
    }]);
  }
  if (input.templateIds.length > 0) {
    return failed(input, input.templateIds.map((templateId) => ({
      code: 'unsupported_template_for_family',
      family: input.family,
      write_mode: input.writeMode,
      template_id: templateId,
    })));
  }

  writeJson(input.outputPath, readJson(pluginPath(family.base_template_path)));
  return ok(input);
}

function getGraphWriteStatementTarget(taskSpec: Record<string, unknown>, writeMode: GraphWriteTemplateWriteMode): unknown[] {
  const behavior = requireRecord(taskSpec['behavior'], 'behavior');
  if (writeMode === 'graph.append') {
    const entries = requireArray(behavior['entries'], 'behavior.entries');
    const firstEntry = requireRecord(entries[0], 'behavior.entries[0]');
    const body = requireRecord(firstEntry['body'], 'behavior.entries[0].body');
    return requireArray(body['statements'], 'behavior.entries[0].body.statements');
  }
  if (writeMode === 'graph.replace') {
    const replace = requireRecord(behavior['replace'], 'behavior.replace');
    const body = requireRecord(replace['body'], 'behavior.replace.body');
    return requireArray(body['statements'], 'behavior.replace.body.statements');
  }
  if (writeMode === 'graph.merge') {
    const externalMerges = requireArray(behavior['external_merges'], 'behavior.external_merges');
    const firstMerge = requireRecord(externalMerges[0], 'behavior.external_merges[0]');
    const inserted = requireRecord(firstMerge['inserted'], 'behavior.external_merges[0].inserted');
    const body = requireRecord(inserted['body'], 'behavior.external_merges[0].inserted.body');
    return requireArray(body['statements'], 'behavior.external_merges[0].inserted.body.statements');
  }
  return requireArray(behavior['patches'], 'behavior.patches');
}

function ok(input: ComposeTaskSpecTemplateInput): TaskSpecTemplateCompositionResult {
  const outputPath = normalizePath(path.resolve(input.outputPath));
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateComposition.v1',
    status: 'ok',
    family: input.family,
    write_mode: input.writeMode,
    output_path: outputPath,
    next: {
      preview_command: `bh task preview --file ${outputPath} --format json`,
      execute_command: `bh task execute --file ${outputPath} --format json`,
    },
  };
}

function failed(
  input: ComposeTaskSpecTemplateInput,
  diagnostics: TaskSpecTemplateDiagnostic[],
): TaskSpecTemplateCompositionResult {
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateComposition.v1',
    status: 'failed',
    family: input.family,
    write_mode: input.writeMode,
    diagnostics,
  };
}

function readJson(filePath: string): unknown {
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function writeJson(filePath: string, value: unknown): void {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, `${JSON.stringify(value, null, 2)}\n`, 'utf8');
}

function pluginPath(relativePath: string): string {
  return path.resolve(pluginRoot(), relativePath);
}

function pluginRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../..');
}

function requireRecord(value: unknown, label: string): Record<string, unknown> {
  if (value && typeof value === 'object' && !Array.isArray(value)) {
    return value as Record<string, unknown>;
  }
  throw new Error(`TaskSpec template is missing object: ${label}`);
}

function requireArray(value: unknown, label: string): unknown[] {
  if (Array.isArray(value)) {
    return value;
  }
  throw new Error(`TaskSpec template is missing array: ${label}`);
}

function isGraphWriteTemplateWriteMode(value: string): value is GraphWriteTemplateWriteMode {
  return value === 'graph.append'
    || value === 'graph.replace'
    || value === 'graph.merge'
    || value === 'graph.patch';
}

function normalizePath(filePath: string): string {
  return filePath.replaceAll('\\', '/');
}
