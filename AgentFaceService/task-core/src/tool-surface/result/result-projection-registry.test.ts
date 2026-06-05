import assert from 'node:assert/strict';
import test from 'node:test';

import { buildReadonlyToolCommandManifestRegistry } from '../manifest/tool-command-manifest-builder.js';
import { resolveResultProjectionPolicy } from './result-projection-registry.js';

test('resolveResultProjectionPolicy uses manifest result_policy_id', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();
  const policy = resolveResultProjectionPolicy({
    manifestRegistry: registry,
    toolIdOrAlias: 'blueprinthelper_preview_task',
  });

  assert.equal(policy.policy_id, 'task.preview.default');
});

test('resolveResultProjectionPolicy maps preview aliases to preview policy', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  assert.equal(resolveResultProjectionPolicy({ manifestRegistry: registry, toolIdOrAlias: 'task preview' }).policy_id, 'task.preview.default');
});

test('resolveResultProjectionPolicy maps execute aliases to execute policy', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  assert.equal(resolveResultProjectionPolicy({ manifestRegistry: registry, toolIdOrAlias: 'task execute' }).policy_id, 'task.execute.default');
});

test('resolveResultProjectionPolicy falls back to generic policy for compat tools', () => {
  const policy = resolveResultProjectionPolicy({ commandKind: 'bridge.call' });

  assert.equal(policy.policy_id, 'tool.generic.default');
});

test('all public task tool manifests declare result_policy_id', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  for (const manifest of registry.list()) {
    if (manifest.tool_name.startsWith('blueprinthelper_')) {
      assert.equal(typeof manifest.result_policy_id, 'string', `${manifest.tool_id} must declare result_policy_id`);
    }
  }
});
