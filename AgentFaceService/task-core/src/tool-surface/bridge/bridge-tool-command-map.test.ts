import assert from 'node:assert/strict';
import test from 'node:test';

import { bridgeCommandByToolName } from './bridge-tool-command-map.js';

test('Bridge command map only owns tool-to-command routing', () => {
  assert.equal(bridgeCommandByToolName.blueprint_get_logic, 'export_logic');
  assert.equal(bridgeCommandByToolName.blueprint_get_logic_json, 'export_logic');
  assert.equal(bridgeCommandByToolName.blueprint_get_logic_md, 'read_blueprint_logic_md');
});
