import assert from 'node:assert/strict';
import test from 'node:test';

import { bridgeCommandByToolName } from './bridge-tool-command-map.js';

test('Bridge command map only owns tool-to-command routing', () => {
  assert.equal(bridgeCommandByToolName.blueprint_get_logic, 'export_logic');
  assert.equal(bridgeCommandByToolName.blueprint_get_logic_json, 'export_logic');
  const removedMarkdownTool = ['blueprint_get', 'logic', 'md'].join('_');
  const removedMarkdownCommand = ['read_blueprint', 'logic', 'md'].join('_');
  assert.equal(bridgeCommandByToolName[removedMarkdownTool], undefined);
  assert.equal(Object.values(bridgeCommandByToolName).includes(removedMarkdownCommand), false);
});
