#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');

const home = process.env.USERPROFILE || process.env.HOME;

if (!home) {
  console.error('[BlueprintHelper Agents] Unable to resolve user home directory.');
  process.exit(1);
}

const pluginRoot = path.resolve(__dirname, '..');
const sourceDir = path.join(pluginRoot, '.codex', 'agents');
const targetDir = path.join(home, '.codex', 'agents');
const agentFiles = [
  'blueprint-explorer.toml',
  'sourcecode-explorer.toml',
  'task-worker.toml',
];

if (!fs.existsSync(sourceDir)) {
  console.error(`[BlueprintHelper Agents] Missing source directory: ${sourceDir}`);
  process.exit(1);
}

fs.mkdirSync(targetDir, { recursive: true });

const installed = [];
for (const file of agentFiles) {
  const src = path.join(sourceDir, file);
  const dst = path.join(targetDir, file);

  if (!fs.existsSync(src)) {
    console.error(`[BlueprintHelper Agents] Missing agent definition: ${src}`);
    process.exit(1);
  }

  fs.copyFileSync(src, dst);
  installed.push(dst);
}

console.log(JSON.stringify({
  success: true,
  target_dir: targetDir,
  installed,
  agents: [
    'blueprint-explorer',
    'sourcecode-explorer',
    'task-worker',
  ],
}, null, 2));
