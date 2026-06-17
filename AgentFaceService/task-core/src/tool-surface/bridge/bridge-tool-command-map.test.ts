import assert from 'node:assert/strict';
import test from 'node:test';

import { bridgeCommandByToolName } from './bridge-tool-command-map.js';
import { isCliBridgeCallAllowed } from './bridge-tool-descriptor.js';
import { listToolCapabilities } from '../catalog/tool-capability-catalog.js';

test('Bridge command map only owns tool-to-command routing', () => {
  assert.equal(bridgeCommandByToolName.blueprint_get_logic, 'export_logic');
  assert.equal(bridgeCommandByToolName.blueprint_get_logic_json, 'export_logic');
  const removedMarkdownTool = ['blueprint_get', 'logic', 'md'].join('_');
  const removedMarkdownCommand = ['read_blueprint', 'logic', 'md'].join('_');
  assert.equal(bridgeCommandByToolName[removedMarkdownTool], undefined);
  assert.equal(Object.values(bridgeCommandByToolName).includes(removedMarkdownCommand), false);
});

test('raw Bridge call allowlist is expert-only and not ordinary capability truth', () => {
  assert.equal(isCliBridgeCallAllowed('get_editor_context'), true);
  assert.equal(isCliBridgeCallAllowed('read_reference_context'), true);
  assert.equal(isCliBridgeCallAllowed('create_asset'), false);

  const serializedDefaultCapabilities = JSON.stringify([
    ...listToolCapabilities({ domain: 'editor', kind: 'read' }).items,
    ...listToolCapabilities({ domain: 'debug', kind: 'diagnose' }).items,
    ...listToolCapabilities({ domain: 'project', kind: 'read' }).items,
  ]);

  assert.equal(serializedDefaultCapabilities.includes('bh bridge call'), false);
  assert.equal(serializedDefaultCapabilities.includes('read_reference_context'), false);
});
