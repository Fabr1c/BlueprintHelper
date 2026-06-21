import {
  parseSlotExpression,
  type SlotExpressionArg,
  type SlotExpressionNode,
} from './slot-expression-parser.js';

export interface GraphWriteEntryComposeSource {
  readonly line?: number;
  readonly column?: number;
}

export interface GraphWriteEntryComposeItem {
  readonly label?: string;
  readonly routeExpression: SlotExpressionNode;
  readonly bodyExpressions: string[];
  readonly bodySourceLines: number[];
  readonly source: GraphWriteEntryComposeSource;
}

const LABEL_PATTERN = /^[A-Za-z0-9_.-]+$/u;

interface PendingEntryFileItem {
  label?: string;
  routeTemplateId: string;
  bodyExpressions: string[];
  bodyNodes: SlotExpressionNode[];
  bodySourceLines: number[];
  source: GraphWriteEntryComposeSource;
}

export function parseGraphWriteInlineEntries(input: string): GraphWriteEntryComposeItem[] {
  const trimmed = input.trim();
  if (trimmed.length === 0) {
    return [];
  }

  return splitTopLevelEntries(trimmed).map((entryText) => {
    const { label, expression } = splitOptionalLabel(entryText);
    const routeExpression = parseSlotExpression(expression);
    return {
      label,
      routeExpression,
      bodyExpressions: routeExpression.args
        .filter((arg): arg is SlotExpressionNode => arg.kind !== 'skip')
        .map(stringifySlotExpressionNode),
      bodySourceLines: [],
      source: {},
    };
  });
}

export function parseGraphWriteEntriesFile(input: string): GraphWriteEntryComposeItem[] {
  const lines = stripBom(input).split(/\r?\n/u);
  const entries: GraphWriteEntryComposeItem[] = [];
  let pending: PendingEntryFileItem | undefined;

  for (let index = 0; index < lines.length; index += 1) {
    const lineNumber = index + 1;
    const line = lines[index] ?? '';
    const trimmed = line.trim();
    if (trimmed.length === 0 || trimmed.startsWith('#')) {
      continue;
    }

    if (/^\s/u.test(line)) {
      if (!pending) {
        throw parseError('entry_header_required', lineNumber);
      }
      pending.bodyExpressions.push(trimmed);
      pending.bodyNodes.push(parseBodyExpression(trimmed, lineNumber));
      pending.bodySourceLines.push(lineNumber);
      continue;
    }

    pushPendingEntry(entries, pending);
    pending = parseEntryHeader(trimmed, lineNumber);
  }

  pushPendingEntry(entries, pending);
  return entries;
}

function splitTopLevelEntries(input: string): string[] {
  const entries: string[] = [];
  let depth = 0;
  let start = 0;
  for (let index = 0; index < input.length; index += 1) {
    const char = input[index];
    if (char === '(') {
      depth += 1;
      continue;
    }
    if (char === ')') {
      depth -= 1;
      if (depth < 0) {
        throw parseError('invalid_slot_expression_syntax', undefined, 'Unbalanced closing parenthesis.');
      }
      continue;
    }
    if (char === ';' && depth === 0) {
      pushEntrySlice(entries, input, start, index);
      start = index + 1;
    }
  }

  if (depth !== 0) {
    throw parseError('invalid_slot_expression_syntax', undefined, 'Unbalanced opening parenthesis.');
  }
  pushEntrySlice(entries, input, start, input.length);
  return entries;
}

function pushEntrySlice(entries: string[], input: string, start: number, end: number): void {
  const value = input.slice(start, end).trim();
  if (value.length === 0) {
    throw parseError('entry_expression_required');
  }
  entries.push(value);
}

function splitOptionalLabel(input: string): { label?: string; expression: string } {
  let depth = 0;
  for (let index = 0; index < input.length; index += 1) {
    const char = input[index];
    if (char === '(') {
      depth += 1;
      continue;
    }
    if (char === ')') {
      depth -= 1;
      continue;
    }
    if (char === ':' && depth === 0) {
      const label = input.slice(0, index).trim();
      const expression = input.slice(index + 1).trim();
      if (!LABEL_PATTERN.test(label)) {
        throw parseError('entry_label_invalid', undefined, `Invalid label: ${label}`);
      }
      if (expression.length === 0) {
        throw parseError('entry_expression_required');
      }
      return { label, expression };
    }
  }

  return { expression: input.trim() };
}

function parseEntryHeader(line: string, lineNumber: number): PendingEntryFileItem {
  const fields = new Map<string, string>();
  const parts = line.split(/\s+/u).filter((part) => part.length > 0);
  if (parts[0] !== 'entry') {
    throw parseError('entry_header_required', lineNumber);
  }

  for (const part of parts.slice(1)) {
    const separatorIndex = part.indexOf('=');
    if (separatorIndex <= 0) {
      throw parseError('invalid_entry_header_syntax', lineNumber, `Invalid token: ${part}`);
    }
    const key = part.slice(0, separatorIndex);
    const value = part.slice(separatorIndex + 1);
    if (value.length === 0 && key !== 'route' && key !== 'label') {
      throw parseError('invalid_entry_header_syntax', lineNumber, `Missing value for ${key}`);
    }
    fields.set(key, value);
  }

  const routeValue = fields.get('route');
  if (!routeValue) {
    throw parseError('entry_route_required', lineNumber);
  }

  const label = fields.get('label');
  if (label !== undefined && !LABEL_PATTERN.test(label)) {
    throw parseError('entry_label_invalid', lineNumber, `Invalid label: ${label}`);
  }

  const routeNode = parseHeaderRoute(routeValue, lineNumber);
  return {
    label,
    routeTemplateId: routeNode.templateId,
    bodyExpressions: [],
    bodyNodes: [],
    bodySourceLines: [],
    source: { line: lineNumber, column: 1 },
  };
}

function parseHeaderRoute(input: string, lineNumber: number): SlotExpressionNode {
  let routeNode: SlotExpressionNode;
  try {
    routeNode = parseSlotExpression(input);
  } catch (error) {
    throw parseError('invalid_slot_expression_syntax', lineNumber, errorMessage(error));
  }
  if (routeNode.args.length > 0) {
    throw parseError('entry_route_must_not_have_args', lineNumber);
  }
  return routeNode;
}

function parseBodyExpression(input: string, lineNumber: number): SlotExpressionNode {
  try {
    return parseSlotExpression(input);
  } catch (error) {
    throw parseError('invalid_slot_expression_syntax', lineNumber, errorMessage(error));
  }
}

function pushPendingEntry(entries: GraphWriteEntryComposeItem[], pending?: PendingEntryFileItem): void {
  if (!pending) {
    return;
  }
  entries.push({
    label: pending.label,
    routeExpression: {
      kind: 'slot',
      templateId: pending.routeTemplateId,
      args: pending.bodyNodes,
    },
    bodyExpressions: [...pending.bodyExpressions],
    bodySourceLines: [...pending.bodySourceLines],
    source: pending.source,
  });
}

function stringifySlotExpressionNode(node: SlotExpressionNode): string {
  if (node.args.length === 0) {
    return node.templateId;
  }
  return `${node.templateId}(${node.args.map(stringifySlotExpressionArg).join(',')})`;
}

function stringifySlotExpressionArg(arg: SlotExpressionArg): string {
  if (arg.kind === 'skip') {
    return '0';
  }
  return stringifySlotExpressionNode(arg);
}

function stripBom(input: string): string {
  return input.replace(/^\uFEFF/u, '');
}

function parseError(code: string, line?: number, detail?: string): Error {
  const location = typeof line === 'number' ? `: line ${line}` : '';
  const suffix = detail ? `: ${detail}` : '';
  return new Error(`${code}${location}${suffix}`);
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
