#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');

const home = process.env.USERPROFILE || process.env.HOME;
const profile = parseProfileJson(process.env.BLUEPRINTHELPER_CODEX_AGENT_PROFILE_JSON);

if (!home) {
  console.error('[BlueprintHelper Agents] Unable to resolve user home directory.');
  process.exit(1);
}

const pluginRoot = path.resolve(__dirname, '..');
const sourceDir = path.join(pluginRoot, 'agents');
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
const agents = [];
for (const file of agentFiles) {
  const src = path.join(sourceDir, file);
  const dst = path.join(targetDir, file);
  const agentName = path.basename(file, '.toml');

  if (!fs.existsSync(src)) {
    console.error(`[BlueprintHelper Agents] Missing agent definition: ${src}`);
    process.exit(1);
  }
  if (fs.statSync(src).size === 0) {
    console.error(`[BlueprintHelper Agents] Empty agent definition: ${src}`);
    process.exit(1);
  }

  const sourceContent = fs.readFileSync(src, 'utf8');
  const installedContent = applyAgentProfile(sourceContent, profile.agents[agentName]);
  fs.writeFileSync(dst, installedContent, 'utf8');
  installed.push(dst);
  agents.push({
    name: agentName,
    path: dst,
    model: readTomlValue(installedContent, 'model'),
    reasoning_effort: readTomlValue(installedContent, 'reasoning_effort'),
  });
}

console.log(JSON.stringify({
  success: true,
  source_dir: sourceDir,
  target_dir: targetDir,
  installed,
  agents,
}, null, 2));

function parseProfileJson(rawValue) {
  if (!rawValue) {
    return { agents: {} };
  }

  try {
    const parsed = JSON.parse(rawValue);
    if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
      throw new Error('Profile root must be an object.');
    }

    const agents = parsed.agents;
    if (!agents || typeof agents !== 'object' || Array.isArray(agents)) {
      return { agents: {} };
    }

    return { agents };
  } catch (error) {
    console.error(`[BlueprintHelper Agents] Invalid BLUEPRINTHELPER_CODEX_AGENT_PROFILE_JSON: ${error.message}`);
    process.exit(1);
  }
}

function applyAgentProfile(content, agentProfile) {
  if (!agentProfile || typeof agentProfile !== 'object' || Array.isArray(agentProfile)) {
    return content;
  }

  let nextContent = content;

  if (typeof agentProfile.model === 'string' && agentProfile.model.trim()) {
    nextContent = upsertTomlValue(nextContent, 'model', agentProfile.model.trim());
  }

  const selectedReasoning = normalizeString(agentProfile.reasoning_effort) || normalizeString(agentProfile.reasoning);
  if (selectedReasoning) {
    nextContent = upsertTomlValue(nextContent, 'reasoning_effort', selectedReasoning, {
      anchorKey: 'model',
    });
  }

  return nextContent;
}

function upsertTomlValue(content, key, value, options = {}) {
  const escapedKey = escapeRegExp(key);
  const lineRegex = new RegExp(`(^${escapedKey}\\s*=\\s*)".*?"`, 'm');
  if (lineRegex.test(content)) {
    return content.replace(lineRegex, `$1"${value}"`);
  }

  if (options.anchorKey) {
    const anchorRegex = new RegExp(`(^${escapeRegExp(options.anchorKey)}\\s*=\\s*".*?"\\r?\\n)`, 'm');
    if (anchorRegex.test(content)) {
      return content.replace(anchorRegex, `$1${key} = "${value}"\n`);
    }
  }

  const developerInstructionsIndex = content.indexOf('developer_instructions = """');
  if (developerInstructionsIndex >= 0) {
    const prefix = content.slice(0, developerInstructionsIndex);
    const suffix = content.slice(developerInstructionsIndex);
    const normalizedPrefix = prefix.endsWith('\n\n') ? prefix : `${prefix.replace(/\n?$/, '\n')}\n`;
    return `${normalizedPrefix}${key} = "${value}"\n\n${suffix}`;
  }

  const trimmed = content.replace(/\s*$/, '');
  return `${trimmed}\n${key} = "${value}"\n`;
}

function readTomlValue(content, key) {
  const match = content.match(new RegExp(`^${escapeRegExp(key)}\\s*=\\s*"([^"]*)"`, 'm'));
  return match ? match[1] : null;
}

function normalizeString(value) {
  return typeof value === 'string' && value.trim() ? value.trim() : null;
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}
