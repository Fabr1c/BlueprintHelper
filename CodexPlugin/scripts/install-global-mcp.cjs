#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');
const { resolveBlueprintHelperUserHome } = require('./user-home.cjs');

let home;
try {
  home = resolveBlueprintHelperUserHome();
} catch (error) {
  console.error('[BlueprintHelper MCP] Unable to resolve user home directory.');
  console.error(error.message);
  process.exit(1);
}

const configPath = path.join(home, '.codex', 'config.toml');
const mcpScriptPath = path.resolve(__dirname, 'start-lifecycle-mcp.cjs');
const escapedMcpScriptPath = mcpScriptPath.replace(/\\/g, '\\\\');
const blueprintHelperRoot = path.resolve(__dirname, '..', '..');
const escapedBlueprintHelperRoot = blueprintHelperRoot.replace(/\\/g, '\\\\');

const block = [
  '[mcp_servers."blueprint-helper"]',
  'command = "node"',
  'args = [',
  `    "${escapedMcpScriptPath}",`,
  ']',
  'type = "stdio"',
  '',
  '[mcp_servers."blueprint-helper".env]',
  `BLUEPRINTHELPER_ROOT = "${escapedBlueprintHelperRoot}"`,
  '',
  '[mcp_servers."blueprint-helper".tools.blueprint_lifecycle_mcp_status]',
  'approval_mode = "approve"',
  '',
  '[mcp_servers."blueprint-helper".tools.blueprint_open_editor]',
  'approval_mode = "approve"',
  '',
  '[mcp_servers."blueprint-helper".tools.blueprint_close_editor]',
  'approval_mode = "approve"',
  '',
  '[mcp_servers."blueprint-helper".tools.blueprint_dismiss_editor_dialogs]',
  'approval_mode = "approve"',
  '',
  '[mcp_servers."blueprint-helper".tools.blueprint_close_editor_dialogs]',
  'approval_mode = "approve"',
  '',
  '[mcp_servers."blueprint-helper".tools.blueprint_developer_exec_console_command]',
  'approval_mode = "approve"',
  '',
].join('\n');

function replaceBlueprintHelperMcpBlock(text) {
  const lines = text.split(/\r?\n/);
  const output = [];
  let skipping = false;
  const ownedSections = new Set([
    '[mcp_servers."blueprint-helper"]',
    '[mcp_servers.blueprint-helper]',
    '[mcp_servers."blueprint-helper".env]',
    '[mcp_servers."blueprint-helper".tools.blueprint_lifecycle_mcp_status]',
    '[mcp_servers."blueprint-helper".tools.blueprint_open_editor]',
    '[mcp_servers."blueprint-helper".tools.blueprint_close_editor]',
    '[mcp_servers."blueprint-helper".tools.blueprint_dismiss_editor_dialogs]',
    '[mcp_servers."blueprint-helper".tools.blueprint_close_editor_dialogs]',
    '[mcp_servers."blueprint-helper".tools.blueprint_developer_exec_console_command]',
  ]);

  for (const line of lines) {
    const normalized = line.trim();
    const isOwnedSection = ownedSections.has(normalized);
    const isNextSection = /^\[/.test(line) && !isOwnedSection;

    if (isOwnedSection) {
      skipping = true;
      continue;
    }

    if (skipping && isNextSection) {
      skipping = false;
    }

    if (!skipping) {
      output.push(line);
    }
  }

  const trimmed = output.join('\n').replace(/\s+$/u, '');
  return `${trimmed}\n\n${block}`;
}

fs.mkdirSync(path.dirname(configPath), { recursive: true });

const current = fs.existsSync(configPath)
  ? fs.readFileSync(configPath, 'utf8')
  : '';
const next = replaceBlueprintHelperMcpBlock(current);

if (current !== next) {
  fs.writeFileSync(configPath, next, 'utf8');
}

console.log(JSON.stringify({
  success: true,
  config_path: configPath,
  mcp_server: 'blueprint-helper',
  surface: 'lifecycle_plus_developer',
  agent_facing_tools: ['blueprint_lifecycle_mcp_status', 'blueprint_open_editor', 'blueprint_close_editor', 'blueprint_dismiss_editor_dialogs', 'blueprint_close_editor_dialogs'],
  developer_tools: ['blueprint_developer_exec_console_command'],
  approval_mode: 'approve',
  command: 'node',
  args: [mcpScriptPath],
  env: {
    BLUEPRINTHELPER_ROOT: blueprintHelperRoot,
  },
}, null, 2));
