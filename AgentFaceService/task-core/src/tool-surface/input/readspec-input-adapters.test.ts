import assert from 'node:assert/strict';
import test from 'node:test';

import { adaptToolInput } from './input-shape-adapter.js';
import { createReadSpecInputShapeAdapterRegistry } from './readspec-input-adapters.js';

test('ReadSpec adapter normalizes blueprinthelper_read_context input', () => {
  const registry = createReadSpecInputShapeAdapterRegistry();
  const normalized = adaptToolInput(registry, ['readspec'], {
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_ReadSpec',
      target_type: 'function',
      target_name: 'Compute',
    },
    view: {
      format: 'logic_flow',
    },
  });

  assert.equal(normalized.schema, 'BlueprintHelper.ReadSpec.v1');
  assert.deepEqual(normalized.view, { format: 'logic_flow' });
});

test('Read reference adapter applies request defaults before task handler execution', () => {
  const registry = createReadSpecInputShapeAdapterRegistry();
  const normalized = adaptToolInput(registry, ['read_reference_context'], {
    asset_path: '/Game/BP_ReadReference',
  });

  assert.equal(normalized.asset_path, '/Game/BP_ReadReference');
  assert.equal(normalized.target_type, 'asset');
  assert.equal(normalized.search_scope, 'project');
  assert.equal(normalized.resolution_policy, 'ue_then_name');
});

test('generic bridge and tool payload adapters preserve object payloads', () => {
  const registry = createReadSpecInputShapeAdapterRegistry();

  assert.deepEqual(adaptToolInput(registry, ['bridge_payload'], { asset_path: '/Game/BP' }), { asset_path: '/Game/BP' });
  assert.deepEqual(adaptToolInput(registry, ['tool_payload'], { limit: 5 }), { limit: 5 });
  assert.deepEqual(adaptToolInput(registry, ['empty_object'], {}), {});
});
