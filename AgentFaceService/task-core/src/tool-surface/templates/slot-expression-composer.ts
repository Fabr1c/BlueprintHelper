import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  getAllGraphWriteSlotDescriptors,
  requireGraphWriteSlotById,
} from '../../task/compiler/graphwrite/graphwrite-slot-registry.js';
import type { GraphWriteSlotDescriptor } from '../../task/compiler/graphwrite/graphwrite-slot-descriptor.js';
import type { GraphWriteTemplateWriteMode } from './taskspec-template-types.js';
import type {
  TaskSpecTemplateDiagnostic,
  TaskSpecTemplateQuickAccessItem,
} from './taskspec-template-types.js';
import { resolveSlotTemplateAlias } from './slot-template-alias.js';
import {
  parseSlotExpression,
  type SlotExpressionArg,
  type SlotExpressionNode,
} from './slot-expression-parser.js';

export type SlotExpressionCompositionResult =
  | { ok: true; value: unknown }
  | { ok: false; diagnostics: TaskSpecTemplateDiagnostic[] };

const MAX_SLOT_EXPRESSION_DEPTH = 12;

interface ResolvedSlotExpressionItem {
  quickAccess: TaskSpecTemplateQuickAccessItem;
  slot: GraphWriteSlotDescriptor;
}

export function composeSlotExpressionTemplate(input: {
  expression: string;
  writeMode: GraphWriteTemplateWriteMode;
  quickAccessCatalog: TaskSpecTemplateQuickAccessItem[];
}): SlotExpressionCompositionResult {
  let root: SlotExpressionNode;
  try {
    root = parseSlotExpression(input.expression);
  } catch (error) {
    return {
      ok: false,
      diagnostics: [{
        code: 'invalid_slot_expression_syntax',
        template_id: input.expression,
        message: error instanceof Error ? error.message : String(error),
      }],
    };
  }

  const context = new CompositionContext(input.quickAccessCatalog, input.writeMode);
  const result = context.composeNode(root, { isRoot: true, depth: 0 });
  return result.ok ? { ok: true, value: result.value } : { ok: false, diagnostics: [result.diagnostic] };
}

class CompositionContext {
  private readonly byTemplateId = new Map<string, TaskSpecTemplateQuickAccessItem[]>();

  constructor(
    quickAccessCatalog: TaskSpecTemplateQuickAccessItem[],
    private readonly writeMode: GraphWriteTemplateWriteMode,
  ) {
    for (const item of quickAccessCatalog) {
      const items = this.byTemplateId.get(item.template_id) ?? [];
      items.push(item);
      this.byTemplateId.set(item.template_id, items);
    }
  }

  composeNode(
    node: SlotExpressionNode,
    options: { isRoot: boolean; depth: number },
  ): { ok: true; value: unknown; item: ResolvedSlotExpressionItem } | { ok: false; diagnostic: TaskSpecTemplateDiagnostic } {
    if (options.depth > MAX_SLOT_EXPRESSION_DEPTH) {
      return {
        ok: false,
        diagnostic: {
          code: 'slot_expression_depth_exceeded',
          template_id: node.templateId,
        },
      };
    }

    const resolved = this.resolveQuickAccess(node.templateId);
    if (!resolved.ok) {
      return resolved;
    }
    const item = resolved.item;
    if (options.isRoot && item.slot.slot_type === 'expression') {
      return {
        ok: false,
        diagnostic: {
          code: 'root_expression_slot_not_composable',
          template_id: node.templateId,
        },
      };
    }

    const value = cloneJson(readJson(pluginPath(item.quickAccess.template_path)));
    if (node.args.length > item.slot.input_slots.length) {
      return {
        ok: false,
        diagnostic: {
          code: 'slot_input_index_out_of_range',
          template_id: node.templateId,
          path: String(item.slot.input_slots.length),
        },
      };
    }
    for (let index = 0; index < node.args.length; index += 1) {
      const arg = node.args[index];
      if (arg.kind === 'skip') {
        continue;
      }
      const inputSlot = item.slot.input_slots[index];
      if (!inputSlot) {
        return {
          ok: false,
          diagnostic: {
            code: 'slot_input_index_out_of_range',
            template_id: node.templateId,
            path: String(index),
          },
        };
      }

      const child = this.composeChild(arg, options.depth + 1);
      if (!child.ok) {
        return child;
      }
      if (!inputSlot.accepts.includes(acceptedTypeForChild(child.item.slot.slot_type))) {
        return {
          ok: false,
          diagnostic: {
            code: 'slot_input_type_mismatch',
            template_id: arg.templateId,
            path: `${node.templateId}.${inputSlot.name}`,
            message: `Input ${inputSlot.name} does not accept ${child.item.slot.slot_type}.`,
          },
        };
      }
      setJsonPath(value, inputSlot.path, child.value);
    }

    return { ok: true, value, item };
  }

  private composeChild(
    node: SlotExpressionNode,
    depth: number,
  ): { ok: true; value: unknown; item: ResolvedSlotExpressionItem } | { ok: false; diagnostic: TaskSpecTemplateDiagnostic } {
    return this.composeNode(node, { isRoot: false, depth });
  }

  private resolveQuickAccess(
    templateId: string,
  ): { ok: true; item: ResolvedSlotExpressionItem } | { ok: false; diagnostic: TaskSpecTemplateDiagnostic } {
    const canonicalTemplateId = resolveSlotTemplateAlias(templateId, getAllGraphWriteSlotDescriptors());
    const candidates = this.byTemplateId.get(canonicalTemplateId) ?? [];
    const item = candidates.find((candidate) => candidate.write_mode === this.writeMode);
    if (item) {
      return { ok: true, item: { quickAccess: item, slot: requireGraphWriteSlotById(item.source_slot_id) } };
    }

    const unsupported = candidates.find((candidate) => candidate.unsupported_write_modes.includes(this.writeMode));
    if (unsupported) {
      return {
        ok: false,
        diagnostic: {
          code: 'slot_not_supported_for_write_mode',
          write_mode: this.writeMode,
          cluster_id: unsupported.cluster_id,
          operation_id: unsupported.operation_id,
          template_id: templateId,
        },
      };
    }

    return {
      ok: false,
      diagnostic: {
        code: 'unknown_quick_access_template',
        write_mode: this.writeMode,
        template_id: templateId,
      },
    };
  }
}

function acceptedTypeForChild(slotType: GraphWriteSlotDescriptor['slot_type']): 'expression' | 'statement[]' {
  return slotType === 'expression' ? 'expression' : 'statement[]';
}

function setJsonPath(target: unknown, jsonPath: string, value: unknown): void {
  const parts = jsonPath.split('.').filter((part) => part.length > 0);
  if (parts.length === 0) {
    throw new Error('slot_input_path_empty');
  }

  let cursor = requireContainer(target, jsonPath);
  for (let index = 0; index < parts.length - 1; index += 1) {
    const part = parts[index] ?? '';
    const nextPart = parts[index + 1] ?? '';
    cursor = getOrCreateChild(cursor, part, isArrayIndex(nextPart));
  }
  setChild(cursor, parts[parts.length - 1] ?? '', value);
}

function getOrCreateChild(container: Record<string, unknown> | unknown[], part: string, nextIsArrayIndex: boolean): Record<string, unknown> | unknown[] {
  if (Array.isArray(container)) {
    const index = parseArrayIndex(part);
    const existing = container[index];
    if (existing && typeof existing === 'object') {
      return existing as Record<string, unknown> | unknown[];
    }
    const next = nextIsArrayIndex ? [] : {};
    container[index] = next;
    return next;
  }

  const existing = container[part];
  if (existing && typeof existing === 'object') {
    return existing as Record<string, unknown> | unknown[];
  }
  const next = nextIsArrayIndex ? [] : {};
  container[part] = next;
  return next;
}

function setChild(container: Record<string, unknown> | unknown[], part: string, value: unknown): void {
  if (Array.isArray(container)) {
    container[parseArrayIndex(part)] = value;
    return;
  }
  container[part] = value;
}

function requireContainer(target: unknown, label: string): Record<string, unknown> | unknown[] {
  if (target && typeof target === 'object') {
    return target as Record<string, unknown> | unknown[];
  }
  throw new Error(`slot_input_path_target_not_container: ${label}`);
}

function isArrayIndex(part: string): boolean {
  return /^\d+$/.test(part);
}

function parseArrayIndex(part: string): number {
  if (!isArrayIndex(part)) {
    throw new Error(`slot_input_path_expected_array_index: ${part}`);
  }
  return Number(part);
}

function readJson(filePath: string): unknown {
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function cloneJson(value: unknown): unknown {
  return JSON.parse(JSON.stringify(value));
}

function pluginPath(relativePath: string): string {
  return path.resolve(pluginRoot(), relativePath);
}

function pluginRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../..');
}
