#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const pluginRoot = path.resolve(__dirname, '..');

function candidate(entry) {
  return entry ? path.resolve(entry) : undefined;
}

const candidates = [
  candidate(process.env.BLUEPRINTHELPER_MCP_ENTRY),
  process.env.BLUEPRINTHELPER_ROOT
    ? path.resolve(process.env.BLUEPRINTHELPER_ROOT, 'AgentFaceService', 'mcp', 'build', 'index.js')
    : undefined,
  path.resolve(pluginRoot, '..', 'AgentFaceService', 'mcp', 'build', 'index.js'),
  path.resolve(pluginRoot, '..', '..', 'AgentFaceService', 'mcp', 'build', 'index.js'),
  path.resolve(process.cwd(), 'AgentFaceService', 'mcp', 'build', 'index.js'),
  path.resolve(process.cwd(), '..', 'AgentFaceService', 'mcp', 'build', 'index.js'),
].filter(Boolean);

const entry = candidates.find((file) => fs.existsSync(file));

if (!entry) {
  console.error('[BlueprintHelper MCP] Unable to locate AgentFaceService/mcp/build/index.js.');
  console.error('[BlueprintHelper MCP] Set BLUEPRINTHELPER_ROOT or BLUEPRINTHELPER_MCP_ENTRY if the CodexPlugin folder is installed outside the source tree.');
  process.exit(1);
}

import(pathToFileURL(entry).href).catch((error) => {
  console.error('[BlueprintHelper MCP] Failed to start:', error);
  process.exit(1);
});
