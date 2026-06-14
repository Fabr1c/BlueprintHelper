import assert from 'node:assert/strict';
import test from 'node:test';

import { getAllGraphWriteSlotDescriptors } from './graphwrite-slot-registry.js';
import { defaultExpressionCompilerRegistry, requireGraphWriteExpressionCompiler } from './expression-compiler-registry.js';
import { defaultStatementCompilerRegistry } from './statement-compiler-registry.js';

test('GraphWrite statement compiler registry covers statement slot compiler ids', () => {
  const statementSlots = getAllGraphWriteSlotDescriptors().filter((slot) => slot.slot_type === 'statement');

  assert.ok(statementSlots.length > 0);
  for (const slot of statementSlots) {
    assert.equal(
      defaultStatementCompilerRegistry.getByCompilerId(slot.compiler_id)?.compiler_id,
      slot.compiler_id,
      `${slot.slot_id} compiler_id must resolve through StatementCompilerRegistry`,
    );
  }
});

test('GraphWrite expression compiler registry covers expression slot compiler ids', () => {
  const expressionSlots = getAllGraphWriteSlotDescriptors().filter((slot) => slot.slot_type === 'expression');

  assert.ok(expressionSlots.length > 0);
  for (const slot of expressionSlots) {
    assert.equal(
      defaultExpressionCompilerRegistry.getByCompilerId(slot.compiler_id)?.compiler_id,
      slot.compiler_id,
      `${slot.slot_id} compiler_id must resolve through ExpressionCompilerRegistry`,
    );
  }
});

test('GraphWrite compatibility compiler kinds are internal and not slot-discoverable', () => {
  const slotCompilerKinds = new Set(getAllGraphWriteSlotDescriptors().map((slot) => `${slot.compiler_id}:${slot.kind}`));
  const compatibilityCompilers = [
    ...defaultStatementCompilerRegistry.getAll(),
    ...defaultExpressionCompilerRegistry.getAll(),
  ].filter((descriptor) => descriptor.compatibility_kinds.length > 0);

  assert.ok(compatibilityCompilers.length > 0);
  for (const descriptor of compatibilityCompilers) {
    for (const kind of descriptor.compatibility_kinds) {
      assert.equal(
        slotCompilerKinds.has(`${descriptor.compiler_id}:${kind}`),
        false,
        `${descriptor.compiler_id}:${kind} must not bind template slots`,
      );
    }
  }
});

test('GraphWrite statement compiler registry resolves current public statement shapes', () => {
  const cases = [
    { kind: 'call', compiler_id: 'statement.call' },
    { kind: 'set', compiler_id: 'statement.set' },
    { kind: 'set_property', compiler_id: 'statement.set_property' },
    { kind: 'let', compiler_id: 'statement.let' },
    { kind: 'create', compiler_id: 'statement.create' },
    { kind: 'convert', compiler_id: 'statement.convert' },
    { kind: 'schedule', compiler_id: 'statement.schedule' },
    { kind: 'field', compiler_id: 'statement.field' },
    { kind: 'container_action', compiler_id: 'statement.container_action' },
    { kind: 'component_bound_event', compiler_id: 'statement.component_bound_event' },
    { kind: 'delegate.bind', delegateOperation: 'bind', compiler_id: 'statement.delegate' },
    { kind: 'control', controlKind: 'branch', compiler_id: 'statement.control.branch' },
    { kind: 'control', controlKind: 'return', compiler_id: 'statement.control.return' },
    { kind: 'control', controlKind: 'switch_int', compiler_id: 'statement.control.generic' },
  ];

  for (const item of cases) {
    assert.equal(
      defaultStatementCompilerRegistry.requireForStatement({
        kind: item.kind,
        path: 'statements[0]',
        controlKind: item.controlKind,
        delegateOperation: item.delegateOperation,
      }).compiler_id,
      item.compiler_id,
      item.kind,
    );
  }
});

test('GraphWrite expression compiler registry resolves current public expression shapes', () => {
  const cases = [
    { kind: 'literal', compiler_id: 'expression.literal' },
    { kind: 'get', compiler_id: 'expression.get' },
    { kind: 'get_property', compiler_id: 'expression.get_property' },
    { kind: 'field', compiler_id: 'expression.field' },
    { kind: 'field', capabilityId: 'field.function_param_get', compiler_id: 'expression.get_function_param' },
    { kind: 'call', compiler_id: 'expression.call' },
    { kind: 'op', compiler_id: 'expression.op' },
    { kind: 'construct', compiler_id: 'expression.construct' },
    { kind: 'deconstruct', compiler_id: 'expression.deconstruct' },
    { kind: 'select', compiler_id: 'expression.select' },
    { kind: 'create', compiler_id: 'expression.create' },
    { kind: 'convert', compiler_id: 'expression.convert' },
    { kind: 'schedule', compiler_id: 'expression.schedule' },
    { kind: 'container_action', compiler_id: 'expression.container_action' },
  ];

  for (const item of cases) {
    assert.equal(
      defaultExpressionCompilerRegistry.requireForExpression({
        kind: item.kind,
        path: 'value',
        capabilityId: item.capabilityId,
      }).compiler_id,
      item.compiler_id,
      item.kind,
    );
  }
});

test('unsupported self expression recommends get target self', () => {
  assert.throws(
    () => requireGraphWriteExpressionCompiler({ kind: 'self', path: 'logic[0].value' }),
    /Use \{"kind":"get","target":"self"\}/u,
  );
});
