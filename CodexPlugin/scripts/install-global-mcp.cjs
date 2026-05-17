#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');

const home = process.env.USERPROFILE || process.env.HOME;

if (!home) {
  console.error('[BlueprintHelper MCP] Unable to resolve user home directory.');
  process.exit(1);
}

const configPath = path.join(home, '.codex', 'config.toml');
const mcpScriptPath = path.resolve(__dirname, 'start-lifecycle-mcp.cjs');
const escapedMcpScriptPath = mcpScriptPath.replace(/\\/g, '\\\\');

const block = [
  '[mcp_servers."blueprint-helper"]',
  'command = "node"',
  'args = [',
  `    "${escapedMcpScriptPath}",`,
  ']',
  'type = "stdio"',
  '',
].join('\n');

function replaceBlueprintHelperMcpBlock(text) {
  const lines = text.split(/\r?\n/);
  const output = [];
  let skipping = false;

  for (const line of lines) {
    const isBlueprintHelperSection = /^\[mcp_servers\.(?:"blueprint-helper"|blueprint-helper)\]\s*$/.test(line);
    const isNextSection = /^\[/.test(line) && !isBlueprintHelperSection;

    if (isBlueprintHelperSection) {
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
  agent_facing_tools: ['blueprint_open_editor', 'blueprint_close_editor'],
  developer_tools: ['blueprint_developer_exec_console_command'],
  command: 'node',
  args: [mcpScriptPath],
}, null, 2));
