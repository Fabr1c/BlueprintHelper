import assert from 'node:assert/strict';
import test from 'node:test';

import {
  getRemovedDirectCliToolCommand,
  listToolCapabilities,
  listToolDomains,
} from '../tool-registry.js';
import {
  buildReadonlyToolCommandManifestRegistry,
} from './tool-command-manifest-builder.js';

test('manifest mirror covers all active domains and default visible capabilities', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();
  for (const domain of listToolDomains().items) {
    for (const kind of domain.default_kinds) {
      const list = listToolCapabilities({ domain: domain.id, kind });
      for (const capability of list.items) {
        assert.ok(registry.has(capability.id), `${capability.id} is missing from manifest mirror`);
        const manifest = registry.require(capability.id);
        if (!getRemovedDirectCliToolCommand(manifest.tool_name)) {
          assert.ok(registry.has(manifest.tool_name), `${manifest.tool_name} does not resolve to a manifest`);
        }
      }
    }
  }
});

test('manifest mirror keeps GraphWrite routes visible for preview and execute capabilities', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();
  for (const toolId of ['blueprint.plan.taskspec.preview', 'blueprint.write.taskspec.execute']) {
    const manifest = registry.require(toolId);
    assert.equal(manifest.route_refs.includes('graph.append.container_action'), true);
    assert.equal(manifest.route_refs.includes('graph.replace.function_body'), true);
    assert.equal(manifest.route_refs.includes('graph.merge_external_flow.insert_between'), true);
  }
});

test('manifest mirror covers grouped command aliases outside default domain kinds', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  assert.equal(registry.get('task preview')?.tool_id, 'blueprint.plan.taskspec.preview');
  assert.equal(registry.get('task execute')?.tool_id, 'blueprint.write.taskspec.execute');
  assert.equal(registry.get('task result')?.tool_id, 'project.read.task_result');
  assert.equal(registry.get('context read')?.tool_id, 'blueprint.read.context.logic_flow');
});

test('manifest mirror keeps removed direct tool names out of canonical duplicate lookup', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();

  assert.equal(registry.get('blueprinthelper_preview_task'), undefined);
  assert.equal(registry.get('blueprinthelper_execute_task'), undefined);
  assert.equal(registry.get('blueprinthelper_read_context'), undefined);
});

test('manifest mirror does not expose editor lifecycle compat commands', () => {
  const registry = buildReadonlyToolCommandManifestRegistry();
  const serialized = JSON.stringify(registry.list());

  assert.equal(registry.get('blueprint_open_editor'), undefined);
  assert.equal(registry.get('blueprint_close_editor'), undefined);
  assert.equal(registry.get('open_editor'), undefined);
  assert.equal(registry.get('close_editor'), undefined);
  assert.equal(serialized.includes('lifecycle_mcp_only'), false);
});
