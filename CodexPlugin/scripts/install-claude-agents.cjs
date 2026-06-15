#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');
const { resolveBlueprintHelperUserHome } = require('./user-home.cjs');

let home;
try {
  home = resolveBlueprintHelperUserHome();
} catch (error) {
  console.error('[BlueprintHelper Claude Agents] Unable to resolve user home directory.');
  console.error(error.message);
  process.exit(1);
}
const profile = parseProfileJson(process.env.BLUEPRINTHELPER_CLAUDE_AGENT_PROFILE_JSON);

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
const agents = [];
for (const file of agentFiles) {
  const src = path.join(sourceDir, file);
  const dst = path.join(targetDir, file);
  const agentName = path.basename(file, '.md');

  if (!fs.existsSync(src)) {
    console.error(`[BlueprintHelper Claude Agents] Missing agent definition: ${src}`);
    process.exit(1);
  }

  const sourceContent = fs.readFileSync(src, 'utf8');
  const installedContent = applyAgentProfile(sourceContent, profile.agents[agentName], agentName);
  fs.writeFileSync(dst, installedContent, 'utf8');
  installed.push(dst);
  agents.push({
    name: agentName,
    path: dst,
    model: readFrontmatterValue(installedContent, 'model'),
    reasoning: readInstalledReasoning(installedContent),
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
    console.error(`[BlueprintHelper Claude Agents] Invalid BLUEPRINTHELPER_CLAUDE_AGENT_PROFILE_JSON: ${error.message}`);
    process.exit(1);
  }
}

function applyAgentProfile(content, agentProfile, agentName) {
  if (!agentProfile || typeof agentProfile !== 'object' || Array.isArray(agentProfile)) {
    return content;
  }

  const selectedModel = normalizeString(agentProfile.model);
  const selectedReasoning = normalizeString(agentProfile.reasoning) || normalizeString(agentProfile.reasoning_effort);

  let nextContent = content;
  if (selectedModel) {
    nextContent = replaceFrontmatterValue(nextContent, 'model', selectedModel);
    nextContent = replacePolicyBullet(
      nextContent,
      /- Always run as a sideAgent on `[^`]+`\./,
      `- Always run as a sideAgent on \`${selectedModel}\`.`
    );
  }

  if (selectedReasoning) {
    const reasoningAction = agentName === 'task-worker'
      ? 'constructing TaskSpecs or running tools'
      : 'choosing tools or returning';
    nextContent = replaceReasoningPolicyLine(
      nextContent,
      `- Use the \`${selectedReasoning}\` reasoning level selected during install before ${reasoningAction}.`
    );
  }

  return nextContent;
}

function replaceFrontmatterValue(content, key, value) {
  const frontmatterMatch = content.match(/^\uFEFF?---\r?\n([\s\S]*?)\r?\n---/);
  if (!frontmatterMatch) {
    return content;
  }

  const frontmatter = frontmatterMatch[1];
  const keyRegex = new RegExp(`(^${escapeRegExp(key)}:\\s*).*$`, 'm');
  if (!keyRegex.test(frontmatter)) {
    return content;
  }

  const updatedFrontmatter = frontmatter.replace(keyRegex, `$1${value}`);
  return content.replace(frontmatter, updatedFrontmatter);
}

function replacePolicyBullet(content, pattern, replacement) {
  return pattern.test(content) ? content.replace(pattern, replacement) : content;
}

function replaceReasoningPolicyLine(content, replacement) {
  return content.replace(
    /(## Model and reasoning policy\r?\n\r?\n- Always run as a sideAgent on `[^`]+`\.\r?\n)- .*(\r?\n- Save tokens in the returned summary, not in your analysis process\.)/,
    `$1${replacement}$2`
  );
}

function readFrontmatterValue(content, key) {
  const frontmatterMatch = content.match(/^\uFEFF?---\r?\n([\s\S]*?)\r?\n---/);
  if (!frontmatterMatch) {
    return null;
  }

  const match = frontmatterMatch[1].match(new RegExp(`^${escapeRegExp(key)}:\\s*(.+)$`, 'm'));
  return match ? match[1].trim() : null;
}

function readInstalledReasoning(content) {
  const match = content.match(/- Use the `([^`]+)` reasoning level selected during install before (?:choosing tools or returning|constructing TaskSpecs or running tools)\./);
  if (match) {
    return match[1];
  }

  const namedLevelMatch = content.match(/- Use ([a-z_]+) reasoning \/ extended thinking where supported by the current Claude Code runtime/i);
  if (namedLevelMatch) {
    return namedLevelMatch[1].toLowerCase();
  }

  if (/maximum available extended thinking \/ highest reasoning depth/.test(content)) {
    return 'maximum_available';
  }

  return null;
}

function normalizeString(value) {
  return typeof value === 'string' && value.trim() ? value.trim() : null;
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}
