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
import {
  parseSlotExpression,
  type SlotExpressionArg,
  type SlotExpressionNode,
} from './slot-expression-parser.js';
import type {
  ComposeTaskSpecTemplateInput,
  GraphWriteTemplateWriteMode,
  TaskSpecTemplateCompositionResult,
  TaskSpecTemplateDiagnostic,
  TaskSpecTemplateQuickAccessItem,
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

  const routeRoot = findRouteRoot(input, writeMode, quickAccessCatalog);
  if (routeRoot.status === 'failed') {
    return failed(input, routeRoot.diagnostics);
  }
  if (routeRoot.status === 'route') {
    return composeGraphWriteRouteTaskSpecTemplate(input, writeMode, quickAccessCatalog, routeRoot.item, routeRoot.node);
  }

  const diagnostics: TaskSpecTemplateDiagnostic[] = [];
  const statements: unknown[] = [];
  const slotQuickAccessCatalog = quickAccessCatalog.filter((item) => item.slot_type !== 'route');
  for (const templateId of input.templateIds) {
    const composed = composeSlotExpressionTemplate({
      expression: templateId,
      writeMode,
      quickAccessCatalog: slotQuickAccessCatalog,
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

function composeGraphWriteRouteTaskSpecTemplate(
  input: ComposeTaskSpecTemplateInput,
  writeMode: GraphWriteTemplateWriteMode,
  quickAccessCatalog: TaskSpecTemplateQuickAccessItem[],
  routeItem: TaskSpecTemplateQuickAccessItem,
  routeNode: SlotExpressionNode,
): TaskSpecTemplateCompositionResult {
  const diagnostics: TaskSpecTemplateDiagnostic[] = [];
  const slotQuickAccessCatalog = quickAccessCatalog.filter((item) => item.slot_type !== 'route');
  const taskSpec = readJson(pluginPath(routeItem.template_path)) as Record<string, unknown>;
  clearInsertTargets(taskSpec, routeItem.insert_paths);

  for (const arg of routeNode.args) {
    if (arg.kind === 'skip') {
      continue;
    }
    const composed = composeSlotExpressionTemplate({
      expression: stringifySlotExpressionNode(arg),
      writeMode,
      quickAccessCatalog: slotQuickAccessCatalog,
    });
    if (!composed.ok) {
      diagnostics.push(...composed.diagnostics.map((diagnostic) => ({
        ...diagnostic,
        family: input.family,
        write_mode: writeMode,
      })));
      continue;
    }
    for (const insertPath of routeItem.insert_paths) {
      insertTemplateFragment(taskSpec, insertPath, composed.value);
    }
  }

  if (diagnostics.length > 0) {
    return failed(input, diagnostics);
  }
  writeJson(input.outputPath, taskSpec);
  return ok(input);
}

function findRouteRoot(
  input: ComposeTaskSpecTemplateInput,
  writeMode: GraphWriteTemplateWriteMode,
  quickAccessCatalog: TaskSpecTemplateQuickAccessItem[],
): (
  | { status: 'none' }
  | { status: 'route'; item: TaskSpecTemplateQuickAccessItem; node: SlotExpressionNode }
  | { status: 'failed'; diagnostics: TaskSpecTemplateDiagnostic[] }
) {
  const routeItems = quickAccessCatalog.filter((item) => item.slot_type === 'route' && item.write_mode === writeMode);
  const routeRoots: Array<{ item: TaskSpecTemplateQuickAccessItem; node: SlotExpressionNode }> = [];
  for (const templateExpression of input.templateIds) {
    let node: SlotExpressionNode;
    try {
      node = parseSlotExpression(templateExpression);
    } catch {
      continue;
    }
    const item = routeItems.find((candidate) => candidate.template_id === node.templateId);
    if (item) {
      routeRoots.push({ item, node });
    }
  }

  if (routeRoots.length === 0) {
    return { status: 'none' };
  }
  if (routeRoots.length > 1 || input.templateIds.length !== 1) {
    return {
      status: 'failed',
      diagnostics: [{
        code: 'route_template_must_be_single_root',
        family: input.family,
        write_mode: writeMode,
        message: 'Route-level templates must be the only top-level compose expression.',
      }],
    };
  }
  return { status: 'route', ...routeRoots[0] };
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
    const taskSpec = readJson(pluginPath(family.base_template_path)) as Record<string, unknown>;
    clearInsertTargets(taskSpec, family.insert_targets);
    const diagnostics: TaskSpecTemplateDiagnostic[] = [];
    const quickAccessCatalog = listTaskSpecTemplateQuickAccess({
      family: input.family,
      cluster: '',
      operation: '',
      writeMode: input.writeMode,
    }).items;
    for (const templateId of input.templateIds) {
      const item = quickAccessCatalog.find((candidate) => candidate.template_id === templateId);
      if (!item) {
        diagnostics.push({
          code: 'unknown_quick_access_template',
          family: input.family,
          write_mode: input.writeMode,
          template_id: templateId,
        });
        continue;
      }
      const fragment = readJson(pluginPath(item.template_path));
      for (const insertPath of item.insert_paths) {
        insertTemplateFragment(taskSpec, insertPath, fragment);
      }
    }
    if (diagnostics.length > 0) {
      return failed(input, diagnostics);
    }
    writeJson(input.outputPath, taskSpec);
    return ok(input);
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

function stringifySlotExpressionArg(arg: SlotExpressionArg): string {
  return arg.kind === 'skip' ? '0' : stringifySlotExpressionNode(arg);
}

function stringifySlotExpressionNode(node: SlotExpressionNode): string {
  if (node.args.length === 0) {
    return node.templateId;
  }
  return `${node.templateId}(${node.args.map(stringifySlotExpressionArg).join(',')})`;
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

function clearInsertTargets(root: Record<string, unknown>, insertTargets: readonly string[]): void {
  for (const insertTarget of insertTargets) {
    if (insertTarget.endsWith('[]')) {
      getPathArray(root, insertTarget.slice(0, -2)).length = 0;
      continue;
    }
    deletePath(root, insertTarget);
  }
}

function insertTemplateFragment(root: Record<string, unknown>, insertPath: string, fragment: unknown): void {
  if (insertPath.endsWith('[]')) {
    getPathArray(root, insertPath.slice(0, -2)).push(fragment);
    return;
  }
  setPathValue(root, insertPath, fragment);
}

function getPathArray(root: Record<string, unknown>, pathExpression: string): unknown[] {
  const parent = ensurePathParent(root, pathExpression);
  const value = parent.record[parent.key];
  if (Array.isArray(value)) {
    return value;
  }
  parent.record[parent.key] = [];
  return parent.record[parent.key] as unknown[];
}

function setPathValue(root: Record<string, unknown>, pathExpression: string, value: unknown): void {
  const parent = ensurePathParent(root, pathExpression);
  parent.record[parent.key] = value;
}

function deletePath(root: Record<string, unknown>, pathExpression: string): void {
  const parent = resolvePathParent(root, pathExpression);
  if (parent) {
    delete parent.record[parent.key];
  }
}

function ensurePathParent(root: Record<string, unknown>, pathExpression: string): { record: Record<string, unknown>; key: string } {
  const parts = pathExpression.split('.');
  const key = parts.pop();
  if (!key) {
    throw new Error(`Invalid insert path: ${pathExpression}`);
  }
  let cursor = root;
  for (const part of parts) {
    const child = cursor[part];
    if (!child || typeof child !== 'object' || Array.isArray(child)) {
      cursor[part] = {};
    }
    cursor = cursor[part] as Record<string, unknown>;
  }
  return { record: cursor, key };
}

function resolvePathParent(root: Record<string, unknown>, pathExpression: string): { record: Record<string, unknown>; key: string } | undefined {
  const parts = pathExpression.split('.');
  const key = parts.pop();
  if (!key) {
    return undefined;
  }
  let cursor: Record<string, unknown> = root;
  for (const part of parts) {
    const child = cursor[part];
    if (!child || typeof child !== 'object' || Array.isArray(child)) {
      return undefined;
    }
    cursor = child as Record<string, unknown>;
  }
  return { record: cursor, key };
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
