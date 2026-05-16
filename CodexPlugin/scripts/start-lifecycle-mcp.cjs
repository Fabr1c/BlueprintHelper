#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const pluginRoot = path.resolve(__dirname, '..');

function candidate(entry) {
  return entry ? path.resolve(entry) : undefined;
}

function codexWorkspaceRoots() {
  const home = process.env.USERPROFILE || process.env.HOME;
  if (!home) {
    return [];
  }

  const statePath = path.join(home, '.codex', '.codex-global-state.json');
  try {
    const state = JSON.parse(fs.readFileSync(statePath, 'utf8'));
    return [
      ...(Array.isArray(state['active-workspace-roots']) ? state['active-workspace-roots'] : []),
      ...(Array.isArray(state['electron-saved-workspace-roots']) ? state['electron-saved-workspace-roots'] : []),
      ...(Array.isArray(state['project-order']) ? state['project-order'] : []),
    ].filter((entry) => typeof entry === 'string');
  } catch {
    return [];
  }
}

const lifecycleEntryRelative = path.join('AgentFaceService', 'mcp', 'build', 'server', 'lifecycle-only.js');

const candidates = [
  candidate(process.env.BLUEPRINTHELPER_LIFECYCLE_MCP_ENTRY),
  process.env.BLUEPRINTHELPER_ROOT
    ? path.resolve(process.env.BLUEPRINTHELPER_ROOT, lifecycleEntryRelative)
    : undefined,
  path.resolve(pluginRoot, '..', lifecycleEntryRelative),
  path.resolve(pluginRoot, '..', '..', lifecycleEntryRelative),
  path.resolve(process.cwd(), lifecycleEntryRelative),
  path.resolve(process.cwd(), '..', lifecycleEntryRelative),
  ...codexWorkspaceRoots().map((root) => path.resolve(root, lifecycleEntryRelative)),
].filter(Boolean);

const entry = candidates.find((file) => fs.existsSync(file));

if (!entry) {
  console.error('[BlueprintHelper Lifecycle MCP] Unable to locate AgentFaceService/mcp/build/server/lifecycle-only.js.');
  console.error('[BlueprintHelper Lifecycle MCP] Build AgentFaceService/mcp or set BLUEPRINTHELPER_ROOT / BLUEPRINTHELPER_LIFECYCLE_MCP_ENTRY.');
  process.exit(1);
}

import(pathToFileURL(entry).href).catch((error) => {
  console.error('[BlueprintHelper Lifecycle MCP] Failed to start:', error);
  process.exit(1);
});
