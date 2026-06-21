import assert from 'node:assert/strict';
import test from 'node:test';

import {
  parseGraphWriteEntriesFile,
  parseGraphWriteInlineEntries,
} from './graphwrite-entry-compose-parser.js';

test('parseGraphWriteInlineEntries parses unlabeled and labeled route expressions', () => {
  const entries = parseGraphWriteInlineEntries(
    'fire:generic_ops.entry.custom_event(generic_ops.call.direct,generic_ops.call.direct);generic_ops.entry.custom_event(generic_ops.call.direct)',
  );

  assert.deepEqual(entries.map((entry) => entry.label), ['fire', undefined]);
  assert.equal(entries[0]?.routeExpression.templateId, 'generic_ops.entry.custom_event');
  assert.deepEqual(entries[0]?.bodyExpressions, [
    'generic_ops.call.direct',
    'generic_ops.call.direct',
  ]);
  assert.equal(entries[1]?.source.line, undefined);
});

test('parseGraphWriteEntriesFile parses comments blank lines labels and body source lines', () => {
  const entries = parseGraphWriteEntriesFile([
    '# compose two custom events',
    'entry route=generic_ops.entry.custom_event label=fire',
    '  generic_ops.call.direct',
    '  generic_ops.call.direct(0,0,0,generic_ops.expression.get_symbol_or_variable)',
    '',
    'entry route=generic_ops.entry.custom_event',
    '  generic_ops.let.default(generic_ops.expression.literal)',
  ].join('\n'));

  assert.equal(entries.length, 2);
  assert.equal(entries[0]?.label, 'fire');
  assert.equal(entries[0]?.source.line, 2);
  assert.deepEqual(entries[0]?.bodyExpressions, [
    'generic_ops.call.direct',
    'generic_ops.call.direct(0,0,0,generic_ops.expression.get_symbol_or_variable)',
  ]);
  assert.deepEqual(entries[0]?.bodySourceLines, [3, 4]);
  assert.equal(entries[1]?.label, undefined);
  assert.deepEqual(entries[1]?.bodySourceLines, [7]);
});

test('parseGraphWriteEntriesFile reports missing route with line number', () => {
  assert.throws(
    () => parseGraphWriteEntriesFile('entry label=fire\n  generic_ops.call.direct\n'),
    /entry_route_required.*line 1/,
  );
  assert.throws(
    () => parseGraphWriteEntriesFile('entry route= label=fire\n  generic_ops.call.direct\n'),
    /entry_route_required.*line 1/,
  );
});

test('parseGraphWriteEntriesFile rejects empty label with line number', () => {
  assert.throws(
    () => parseGraphWriteEntriesFile('entry route=generic_ops.entry.custom_event label=\n  generic_ops.call.direct\n'),
    /entry_label_invalid.*line 1/,
  );
});

test('parseGraphWriteEntriesFile rejects body before entry header', () => {
  assert.throws(
    () => parseGraphWriteEntriesFile('  generic_ops.call.direct\n'),
    /entry_header_required.*line 1/,
  );
});

test('parseGraphWriteInlineEntries rejects invalid labeled entry expression', () => {
  assert.throws(
    () => parseGraphWriteInlineEntries('fire:generic_ops.entry.custom_event('),
    /invalid_slot_expression_syntax/,
  );
});
