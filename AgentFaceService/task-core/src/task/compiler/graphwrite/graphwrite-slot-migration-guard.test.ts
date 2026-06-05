import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { getAllGraphWriteSlotDescriptors } from './graphwrite-slot-registry.js';

test('GraphWrite slot migration guard blocks branch-only statement and expression kind additions', () => {
  const taskCompilerSource = fs.readFileSync(resolveTaskCompilerSourcePath(), 'utf8');
  const statementKinds = new Set(
    getAllGraphWriteSlotDescriptors()
      .filter((slot) => slot.slot_type === 'statement')
      .map((slot) => slot.kind),
  );
  const expressionKinds = new Set(
    getAllGraphWriteSlotDescriptors()
      .filter((slot) => slot.slot_type === 'expression')
      .map((slot) => slot.kind),
  );

  [
    'control',
    'branch',
    'return',
    'sequence',
    'delegate',
    'delegate.assign',
    'delegate.unbind',
    'delegate.unbind_all',
    'delegate.call',
  ].forEach((kind) => statementKinds.add(kind));
  [
    'field',
    'get_property',
    'call',
    'deconstruct',
    'create',
    'convert',
    'schedule',
    'container_action',
    'literal',
  ].forEach((kind) => expressionKinds.add(kind));

  const statementDispatchKinds = extractKindComparisons(taskCompilerSource, 'compileStatementFlow')
    .concat(extractKindComparisons(taskCompilerSource, 'compileStatementNode'));
  const expressionDispatchKinds = extractKindComparisons(taskCompilerSource, 'compileValueExpression');

  for (const kind of statementDispatchKinds) {
    assert.equal(statementKinds.has(kind), true, `Statement dispatch kind ${kind} must be slot or registry declared`);
  }
  for (const kind of expressionDispatchKinds) {
    assert.equal(expressionKinds.has(kind), true, `Expression dispatch kind ${kind} must be slot or registry declared`);
  }
});

function extractKindComparisons(source: string, functionName: string): string[] {
  const body = extractFunctionBody(source, functionName);
  return [...body.matchAll(/\bkind\s*={2,3}\s*'([^']+)'/g)]
    .map((match) => match[1])
    .filter((kind) => kind !== 'string');
}

function extractFunctionBody(source: string, functionName: string): string {
  const start = source.indexOf(`function ${functionName}`);
  assert.notEqual(start, -1, `${functionName} must exist`);
  const braceStart = source.indexOf('{', start);
  assert.notEqual(braceStart, -1, `${functionName} body must start`);
  let depth = 0;
  for (let index = braceStart; index < source.length; index += 1) {
    const char = source[index];
    if (char === '{') {
      depth += 1;
    } else if (char === '}') {
      depth -= 1;
      if (depth === 0) {
        return source.slice(braceStart, index + 1);
      }
    }
  }
  throw new Error(`${functionName} body did not close`);
}

function resolveTaskCompilerSourcePath(): string {
  return path.resolve(
    path.dirname(fileURLToPath(import.meta.url)),
    '..',
    '..',
    '..',
    '..',
    'src',
    'task',
    'compiler',
    'task-compiler.ts',
  );
}
