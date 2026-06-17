#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const pluginRoot = path.resolve(__dirname, '..');

function candidate(entry) {
  return entry ? path.resolve(entry) : undefined;
}

function runtimeCandidate(source, file, root) {
  return file ? { source, file, root } : undefined;
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
  runtimeCandidate(
    'env:BLUEPRINTHELPER_LIFECYCLE_MCP_ENTRY',
    candidate(process.env.BLUEPRINTHELPER_LIFECYCLE_MCP_ENTRY),
    undefined,
  ),
  process.env.BLUEPRINTHELPER_ROOT
    ? runtimeCandidate(
      'env:BLUEPRINTHELPER_ROOT',
      path.resolve(process.env.BLUEPRINTHELPER_ROOT, lifecycleEntryRelative),
      path.resolve(process.env.BLUEPRINTHELPER_ROOT),
    )
    : undefined,
  runtimeCandidate('plugin_adjacent', path.resolve(pluginRoot, '..', lifecycleEntryRelative), path.resolve(pluginRoot, '..')),
  runtimeCandidate('plugin_parent', path.resolve(pluginRoot, '..', '..', lifecycleEntryRelative), path.resolve(pluginRoot, '..', '..')),
  runtimeCandidate('cwd', path.resolve(process.cwd(), lifecycleEntryRelative), process.cwd()),
  runtimeCandidate('cwd_parent', path.resolve(process.cwd(), '..', lifecycleEntryRelative), path.resolve(process.cwd(), '..')),
  ...codexWorkspaceRoots().map((root) => runtimeCandidate(
    'codex_workspace_root',
    path.resolve(root, lifecycleEntryRelative),
    path.resolve(root),
  )),
].filter(Boolean);

const resolved = candidates.find((item) => fs.existsSync(item.file));

if (!resolved) {
  console.error('[BlueprintHelper Lifecycle MCP] Unable to locate AgentFaceService/mcp/build/server/lifecycle-only.js.');
  console.error('[BlueprintHelper Lifecycle MCP] Build AgentFaceService/mcp or set BLUEPRINTHELPER_ROOT / BLUEPRINTHELPER_LIFECYCLE_MCP_ENTRY.');
  process.exit(1);
}

process.env.BLUEPRINTHELPER_LIFECYCLE_MCP_RESOLVED_ENTRY = resolved.file;
process.env.BLUEPRINTHELPER_LIFECYCLE_MCP_RESOLVED_SOURCE = resolved.source;
process.env.BLUEPRINTHELPER_LIFECYCLE_MCP_RESOLVED_ROOT = resolved.root || '';
process.env.BLUEPRINTHELPER_LIFECYCLE_MCP_PLUGIN_ROOT = pluginRoot;
process.env.BLUEPRINTHELPER_LIFECYCLE_MCP_STARTED_AT = new Date().toISOString();

console.error(`[BlueprintHelper Lifecycle MCP] Loading entry: ${resolved.file} (source=${resolved.source})`);

import(pathToFileURL(resolved.file).href).catch((error) => {
  console.error('[BlueprintHelper Lifecycle MCP] Failed to start:', error);
  process.exit(1);
});
