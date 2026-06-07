import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const PRODUCTION_FILES = [
  'src/tool-surface/tool-registry.ts',
  'src/tool-surface/catalog/tool-capability-types.ts',
  'src/tool-surface/catalog/tool-capability-catalog.ts',
  'src/tool-surface/manifest/tool-command-manifest-builder.ts',
];

test('old ToolTemplateSelection dispatch surface is removed from production code', () => {
  const forbidden = [
    'ToolTemplateSelection.v1',
    'getToolTemplateDispatch',
    'GetToolTemplateDispatchOptions',
    'selected_route',
    'slot_templates',
    'ToolsTemplateBuilder',
    'bh tools templates <tool_id>',
  ];

  for (const relativePath of PRODUCTION_FILES) {
    const text = fs.readFileSync(path.join(ROOT, relativePath), 'utf8');
    for (const token of forbidden) {
      assert.equal(text.includes(token), false, `${relativePath} still contains ${token}`);
    }
  }
});
