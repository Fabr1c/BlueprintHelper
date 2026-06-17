import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const pluginRoot = path.resolve(scriptDir, '..');

const lifecycleTools = [
  'blueprint_lifecycle_mcp_status',
  'blueprint_open_editor',
  'blueprint_close_editor',
  'blueprint_dismiss_editor_dialogs',
  'blueprint_close_editor_dialogs',
];

test('global MCP installer allowlists all Agent-facing lifecycle tools', () => {
  const installer = readFileSync(path.join(scriptDir, 'install-global-mcp.cjs'), 'utf8');

  for (const tool of lifecycleTools) {
    assert.match(
      installer,
      new RegExp(`\\[mcp_servers\\."blueprint-helper"\\.tools\\.${tool}\\]`),
      `install-global-mcp.cjs must allowlist ${tool}`,
    );
    assert.match(
      installer,
      new RegExp(`agent_facing_tools:[^\\n]+${tool}`),
      `install-global-mcp.cjs must report ${tool} as Agent-facing lifecycle tool`,
    );
  }
});

test('global MCP installer pins lifecycle server to BlueprintHelper root env', () => {
  const installer = readFileSync(path.join(scriptDir, 'install-global-mcp.cjs'), 'utf8');
  const launcher = readFileSync(path.join(scriptDir, 'start-lifecycle-mcp.cjs'), 'utf8');

  assert.match(installer, /\[mcp_servers\."blueprint-helper"\.env\]/);
  assert.match(installer, /BLUEPRINTHELPER_ROOT =/);
  assert.match(installer, /blueprintHelperRoot/);
  assert.match(launcher, /BLUEPRINTHELPER_LIFECYCLE_MCP_RESOLVED_ENTRY/);
  assert.match(launcher, /BLUEPRINTHELPER_LIFECYCLE_MCP_RESOLVED_SOURCE/);
  assert.match(launcher, /Loading entry:/);
});

test('Codex hook and workflow docs mention close-editor-dialog lifecycle tool', () => {
  const hookConfig = readFileSync(path.join(pluginRoot, 'hooks.json'), 'utf8');
  const mainSkill = readFileSync(path.join(pluginRoot, 'skills', 'blueprint-helper', 'SKILL.md'), 'utf8');
  const configureSkill = readFileSync(path.join(pluginRoot, 'skills', 'blueprint-helper-configure', 'SKILL.md'), 'utf8');

  assert.match(hookConfig, /mcp__blueprint_helper__blueprint_lifecycle_mcp_status/);
  assert.match(hookConfig, /mcp__blueprint_helper__blueprint_close_editor_dialogs/);
  assert.match(mainSkill, /mcp__blueprint_helper__blueprint_lifecycle_mcp_status/);
  assert.match(mainSkill, /mcp__blueprint_helper__blueprint_close_editor_dialogs/);
  assert.match(configureSkill, /mcp__blueprint_helper__blueprint_lifecycle_mcp_status/);
  assert.match(configureSkill, /mcp__blueprint_helper__blueprint_close_editor_dialogs/);
});
