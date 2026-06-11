import fs from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export const PROJECT_PROFILE_RELATIVE_PATH = path.join('.blueprinthelper', 'project-profile.json');
export const LEGACY_AGENT_PROFILE_RELATIVE_PATH = path.join('.blueprinthelper', 'agent-profile.json');
export const AGENT_WORKFLOW_RELATIVE_PATH = path.join('.blueprinthelper', 'AgentWorkFlow.md');

const CODEX_MARKER = 'BLUEPRINTHELPER CODEX';
const CLAUDE_MARKER = 'BLUEPRINTHELPER CLAUDE';

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function sectionRegex(markerName) {
  const marker = escapeRegExp(markerName);
  return new RegExp(`<!-- BEGIN ${marker} -->[\\s\\S]*?<!-- END ${marker} -->\\r?\\n?`, 'm');
}

function ensureTrailingNewline(value) {
  return value.endsWith('\n') ? value : `${value}\n`;
}

export function upsertManagedSection(content, markerName, body) {
  const normalizedBody = body.trimEnd();
  const section = `<!-- BEGIN ${markerName} -->\n${normalizedBody}\n<!-- END ${markerName} -->\n`;
  const source = content ?? '';
  const regex = sectionRegex(markerName);
  if (regex.test(source)) {
    return source.replace(regex, section);
  }
  return `${section}${source.length > 0 ? ensureTrailingNewline(source) : ''}`;
}

export function removeManagedSection(content, markerName) {
  const source = content ?? '';
  const regex = sectionRegex(markerName);
  if (!regex.test(source)) {
    return source;
  }
  return source.replace(regex, '').replace(/^\r?\n/, '');
}

function isRecord(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function buildProjectProfile({ engineRoot, ueVersion, existingProfile = {} }) {
  const existing = isRecord(existingProfile) ? existingProfile : {};
  const existingEnvironment = isRecord(existing.environment) ? existing.environment : {};
  const existingWorkflowDocs = isRecord(existing.workflow_docs) ? existing.workflow_docs : {};
  const profile = {
    ...existing,
    schema: 'BlueprintHelper.ProjectProfile.v1',
    environment: { ...existingEnvironment },
    workflow_docs: {
      ...existingWorkflowDocs,
      agent_workflow: '.blueprinthelper/AgentWorkFlow.md',
    },
  };

  if (engineRoot) {
    profile.environment.ue_engine_dir = engineRoot;
  }
  if (ueVersion) {
    profile.environment.ue_version = ueVersion;
  }

  return `${JSON.stringify(profile, null, 2)}\n`;
}

function buildAgentWorkflowMarkdown() {
  return `# BlueprintHelper Agent WorkFlow

This file is the project-level BlueprintHelper workflow prompt source for Codex and Claude agents.

If BlueprintHelper evidence sources disagree, stop and report \`evidence_conflict\`. Do not read \`.uasset\`, \`.umap\`, or other Unreal binary asset files as fallback evidence.

## Editor Lifecycle

- Editor lifecycle is global MCP only.
- Use \`mcp__blueprint_helper__blueprint_open_editor\` to open Unreal Editor.
- Use \`mcp__blueprint_helper__blueprint_close_editor\` to close Unreal Editor.
- Do not start or close Unreal Editor through CLI lifecycle aliases such as \`bh open_editor\`, \`bh close_editor\`, \`blueprint_open_editor\`, or \`blueprint_close_editor\`.
- If lifecycle MCP is unavailable, report \`lifecycle_mcp_unavailable\` instead of using a CLI fallback.
- The Main Agent owns lifecycle MCP. Subagents must not call MCP tools.

## Ordinary Asset Workflow

- Use BlueprintHelper CLI for ordinary UE editor asset reads and writes.
- On Windows PowerShell, resolve the globally linked \`bh.cmd\` launcher to an absolute path before invoking CLI commands. Prefer:

~~~powershell
$BhCmd = (Get-Command bh.cmd -CommandType Application -ErrorAction Stop).Source
& $BhCmd blueprint_get_runtime_profile --json "{}" --select status,summary
~~~

- If command discovery is unavailable, resolve the npm global prefix and call the launcher by full path:

~~~powershell
$NpmPrefix = (& npm.cmd config get prefix).Trim()
$BhCmd = Join-Path $NpmPrefix "bh.cmd"
& $BhCmd <command> <args>
~~~

- Do not call a relative \`.\\bh.cmd\` unless the current directory is the npm global bin directory. Bare \`bh\` may resolve to a blocked \`bh.ps1\` on old installs; rerun \`install.cmd\` or use the absolute \`bh.cmd\` path.
- After intent, target, and scope assessment, run a pre-dispatch editor/Bridge gate before assigning any SideAgent work.
- The pre-dispatch gate is lightweight: run \`blueprint_get_runtime_profile\`; if the intended Editor/Bridge is unavailable, stale, or not the target project, the Main Agent may open the target Editor once through \`mcp__blueprint_helper__blueprint_open_editor\`, then rerun runtime profile.
- If lifecycle MCP is unavailable, report \`lifecycle_mcp_unavailable\` instead of using CLI lifecycle aliases or shell-launched editor fallbacks.
- If the Editor opens but Bridge remains unavailable, stop or assign only bounded diagnostics with \`Bridge unavailable\` as the stop condition; do not ask SideAgents to repair lifecycle.
- Use TaskSpec-first writes: gather context, build a \`BlueprintHelper.TaskSpec.v1\`, preview, request write approval when required, execute, and read back results.
- Use ReadSpec/read-context flows for Blueprint, UMG, DataAsset, and DataTable discovery.
- If \`read_context\`, Editor screenshots/visible state, preview, execute, or readback evidence disagree, stop and report \`evidence_conflict\`; do not inspect Unreal binary asset files as fallback.
- Do not use deprecated MCP ordinary read/write/debug/task tools as fallback paths.

## Source And Documentation Work

- Use normal repository tools for C++, TypeScript, Python, JSON, config, build scripts, documentation, AGENTS files, CLAUDE files, and memory files.
- Do not inspect BlueprintHelper implementation source for ordinary plugin usage. Use installed skills, AgentGuide, CLI reference, and templates instead. Source reads are allowed for plugin development, installation repair, and debugging tasks.

## Safety

- Preview is the write gate. Do not execute writes when preview is blocked.
- Missing capability policy is stop and report.
- Evidence conflict policy is stop and report. Direct \`.uasset\` / \`.umap\` binary reads are not a fallback path.
- Do not request, pass, or store raw Bridge tokens, auth sessions, or private environment details.
- Before CLI writes, ensure the Bridge belongs to the intended running Editor session and that the Editor lifecycle was managed through the global MCP path.
`;
}

function buildCodexEntry() {
  return `## BlueprintHelper Codex Entry

Read and obey:
- .blueprinthelper/AgentWorkFlow.md

Editor lifecycle is global MCP only. Do not start or close Unreal Editor through CLI lifecycle aliases. If lifecycle MCP is unavailable, report \`lifecycle_mcp_unavailable\`.
`;
}

function buildClaudeEntry() {
  return `## BlueprintHelper Claude Entry

Read and obey:
- .blueprinthelper/AgentWorkFlow.md

Editor lifecycle is global MCP only. Do not start or close Unreal Editor through CLI lifecycle aliases. If lifecycle MCP is unavailable, report \`lifecycle_mcp_unavailable\`.
`;
}

async function readTextIfPresent(filePath) {
  try {
    return await fs.readFile(filePath, 'utf8');
  } catch (error) {
    if (error && error.code === 'ENOENT') {
      return '';
    }
    throw error;
  }
}

async function readJsonIfPresent(filePath) {
  const content = await readTextIfPresent(filePath);
  if (!content.trim()) {
    return {};
  }
  return JSON.parse(content);
}

async function writeText(filePath, content) {
  await fs.mkdir(path.dirname(filePath), { recursive: true });
  await fs.writeFile(filePath, content, 'utf8');
}

async function removeFileIfPresent(filePath) {
  try {
    await fs.rm(filePath, { force: true });
  } catch (error) {
    if (!error || error.code !== 'ENOENT') {
      throw error;
    }
  }
}

async function removeFileIfEmpty(filePath) {
  if (!existsSync(filePath)) {
    return;
  }
  const content = await fs.readFile(filePath, 'utf8');
  if (content.trim().length === 0) {
    await fs.rm(filePath, { force: true });
  }
}

export async function installProjectWorkflow({ projectDir, engineRoot = '', ueVersion = '' }) {
  if (!projectDir) {
    throw new Error('projectDir is required');
  }
  const resolvedProjectDir = path.resolve(projectDir);
  const blueprintHelperDir = path.join(resolvedProjectDir, '.blueprinthelper');

  await fs.mkdir(blueprintHelperDir, { recursive: true });
  await writeText(
    path.join(resolvedProjectDir, AGENT_WORKFLOW_RELATIVE_PATH),
    buildAgentWorkflowMarkdown(),
  );

  if (engineRoot) {
    const profilePath = path.join(resolvedProjectDir, PROJECT_PROFILE_RELATIVE_PATH);
    await writeText(
      profilePath,
      buildProjectProfile({
        engineRoot,
        ueVersion,
        existingProfile: await readJsonIfPresent(profilePath),
      }),
    );
  }

  const agentsPath = path.join(resolvedProjectDir, 'AGENTS.md');
  const agentsContent = await readTextIfPresent(agentsPath);
  await writeText(agentsPath, upsertManagedSection(agentsContent, CODEX_MARKER, buildCodexEntry()));

  const claudePath = path.join(resolvedProjectDir, 'CLAUDE.md');
  const claudeContent = await readTextIfPresent(claudePath);
  await writeText(claudePath, upsertManagedSection(claudeContent, CLAUDE_MARKER, buildClaudeEntry()));
}

export async function removeProjectWorkflow({ projectDir, removeLegacyAgentProfile = true }) {
  if (!projectDir) {
    throw new Error('projectDir is required');
  }
  const resolvedProjectDir = path.resolve(projectDir);

  await removeFileIfPresent(path.join(resolvedProjectDir, PROJECT_PROFILE_RELATIVE_PATH));
  await removeFileIfPresent(path.join(resolvedProjectDir, AGENT_WORKFLOW_RELATIVE_PATH));
  if (removeLegacyAgentProfile) {
    await removeFileIfPresent(path.join(resolvedProjectDir, LEGACY_AGENT_PROFILE_RELATIVE_PATH));
  }

  const agentsPath = path.join(resolvedProjectDir, 'AGENTS.md');
  if (existsSync(agentsPath)) {
    await writeText(agentsPath, removeManagedSection(await readTextIfPresent(agentsPath), CODEX_MARKER));
    await removeFileIfEmpty(agentsPath);
  }

  const claudePath = path.join(resolvedProjectDir, 'CLAUDE.md');
  if (existsSync(claudePath)) {
    await writeText(claudePath, removeManagedSection(await readTextIfPresent(claudePath), CLAUDE_MARKER));
    await removeFileIfEmpty(claudePath);
  }
}

function readArgValue(args, name) {
  const index = args.indexOf(name);
  if (index === -1) {
    return '';
  }
  return args[index + 1] ?? '';
}

async function main() {
  const [, , command, ...args] = process.argv;
  const projectDir = readArgValue(args, '--project-dir');
  if (command === 'install') {
    await installProjectWorkflow({
      projectDir,
      engineRoot: readArgValue(args, '--engine-root'),
      ueVersion: readArgValue(args, '--ue-version'),
    });
    return;
  }
  if (command === 'uninstall') {
    await removeProjectWorkflow({ projectDir });
    return;
  }
  throw new Error('Usage: agent-workflow-install.mjs <install|uninstall> --project-dir <ProjectDir> [--engine-root <UE_ROOT>] [--ue-version <version>]');
}

const invokedPath = process.argv[1] ? path.resolve(process.argv[1]) : '';
if (invokedPath === fileURLToPath(import.meta.url)) {
  main().catch((error) => {
    console.error(error instanceof Error ? error.message : String(error));
    process.exitCode = 1;
  });
}
