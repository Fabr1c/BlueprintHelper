#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');

const home = process.env.USERPROFILE || process.env.HOME;

if (!home) {
  console.error('[BlueprintHelper Claude Agents] Unable to resolve user home directory.');
  process.exit(1);
}

const pluginRoot = path.resolve(__dirname, '..');
const sourceDir = path.join(pluginRoot, 'agents');
const targetDir = path.join(home, '.claude', 'agents');
const agentFiles = [
  'blueprint-explorer.md',
  'sourcecode-explorer.md',
  'task-worker.md',
];

if (!fs.existsSync(sourceDir)) {
  console.error(`[BlueprintHelper Claude Agents] Missing source directory: ${sourceDir}`);
  process.exit(1);
}

fs.mkdirSync(targetDir, { recursive: true });

const installed = [];
for (const file of agentFiles) {
  const src = path.join(sourceDir, file);
  const dst = path.join(targetDir, file);

  if (!fs.existsSync(src)) {
    console.error(`[BlueprintHelper Claude Agents] Missing agent definition: ${src}`);
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
    {
      name: 'blueprint-explorer',
      model: 'haiku',
      reasoning: 'maximum_available'
    },
    {
      name: 'sourcecode-explorer',
      model: 'haiku',
      reasoning: 'maximum_available'
    },
    {
      name: 'task-worker',
      model: 'haiku',
      reasoning: 'maximum_available'
    }
  ]
}, null, 2));
