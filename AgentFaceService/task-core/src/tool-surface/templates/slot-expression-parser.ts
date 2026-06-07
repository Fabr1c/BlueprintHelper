export type SlotExpressionArg =
  | { kind: 'skip' }
  | SlotExpressionNode;

export interface SlotExpressionNode {
  kind: 'slot';
  templateId: string;
  args: SlotExpressionArg[];
}

const TEMPLATE_ID_PATTERN = /^[A-Za-z0-9_.-]+$/;

export function splitTopLevelSlotExpressions(input: string): string[] {
  const items: string[] = [];
  let depth = 0;
  let start = 0;
  for (let index = 0; index < input.length; index += 1) {
    const char = input[index];
    if (char === '(') {
      depth += 1;
    } else if (char === ')') {
      depth -= 1;
      if (depth < 0) {
        throw syntaxError('Unbalanced closing parenthesis.');
      }
    } else if (char === ',' && depth === 0) {
      pushSlice(items, input, start, index);
      start = index + 1;
    }
  }
  if (depth !== 0) {
    throw syntaxError('Unbalanced opening parenthesis.');
  }
  pushSlice(items, input, start, input.length);
  return items;
}

export function parseSlotExpression(input: string): SlotExpressionNode {
  const trimmed = input.trim();
  if (trimmed.length === 0) {
    throw syntaxError('Empty slot expression.');
  }
  const openIndex = trimmed.indexOf('(');
  if (openIndex === -1) {
    assertTemplateId(trimmed);
    return { kind: 'slot', templateId: trimmed, args: [] };
  }
  if (!trimmed.endsWith(')')) {
    throw syntaxError('Slot expression must end with a closing parenthesis.');
  }

  const templateId = trimmed.slice(0, openIndex).trim();
  assertTemplateId(templateId);
  const inner = trimmed.slice(openIndex + 1, -1).trim();
  return {
    kind: 'slot',
    templateId,
    args: inner.length === 0
      ? []
      : splitTopLevelSlotExpressions(inner).map(parseArg),
  };
}

function parseArg(input: string): SlotExpressionArg {
  const trimmed = input.trim();
  if (trimmed === '0') {
    return { kind: 'skip' };
  }
  return parseSlotExpression(trimmed);
}

function pushSlice(items: string[], input: string, start: number, end: number): void {
  const value = input.slice(start, end).trim();
  if (value.length === 0) {
    throw syntaxError('Empty slot expression argument.');
  }
  items.push(value);
}

function assertTemplateId(templateId: string): void {
  if (!TEMPLATE_ID_PATTERN.test(templateId)) {
    throw syntaxError(`Invalid template id: ${templateId}`);
  }
}

function syntaxError(message: string): Error {
  return new Error(`invalid_slot_expression_syntax: ${message}`);
}
