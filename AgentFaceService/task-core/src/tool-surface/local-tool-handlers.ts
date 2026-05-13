import { execFile, spawn } from 'node:child_process';
import * as fs from 'node:fs';
import * as path from 'node:path';
import {
  buildDiagnosticsData,
  buildDiagnosticsMarkdown,
  failureResult,
  successRead,
  type DiagnosticsMarkdownReport,
  type ToolResultBase,
} from '../result/tool-result.js';
import { resolveProjectEngineDir } from '../project-profile/agent-profile.js';
import { resolveExplicitProjectFile } from '../project-profile/editor-paths.js';
import type { BlueprintHelperToolContext, LocalProcessResult } from './types.js';

const agentGuideRelativePath = path.join(
  'BlueprintHelper',
  'Resources',
  'AgentGuide',
  '00_Agent_Onboarding_Index_20260504.md',
);
const agentGuidePackagedRelativePath = path.join(
  'Resources',
  'AgentGuide',
  '00_Agent_Onboarding_Index_20260504.md',
);
const agentGuideProjectPluginRelativePaths = [
  path.join('Plugins', 'BlueprintHelper', 'BlueprintHelper', agentGuidePackagedRelativePath),
  path.join('Plugins', 'BlueprintHelper', agentGuidePackagedRelativePath),
];

export const localToolNames = new Set([
  'blueprinthelper_read_agent_guide',
  'blueprinthelper_diagnostics',
  'blueprint_build_project',
  'blueprint_open_editor',
]);

export async function executeLocalTool(
  name: string,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  if (name === 'blueprinthelper_read_agent_guide') {
    return readAgentGuide(context);
  }
  if (name === 'blueprinthelper_diagnostics') {
    return readStaticDiagnostics(context);
  }
  if (name === 'blueprint_build_project') {
    return buildProject(input, context);
  }
  if (name === 'blueprint_open_editor') {
    return openEditor(input, context);
  }

  return failureResult(name, {
    code: 'local_tool_not_mapped',
    stage: 'parse_input',
    message: `No local tool mapping for ${name}.`,
    retryable: false,
    rollback_result: 'not_needed',
  });
}

function readAgentGuide(context: BlueprintHelperToolContext): ToolResultBase {
  const guidePath = resolveAgentGuidePath(context.cwd);
  if (!guidePath) {
    return failureResult('blueprinthelper_read_agent_guide', {
      code: 'agent_guide_not_found',
      stage: 'execute',
      message: `AgentGuide not found from cwd ${context.cwd}.`,
      retryable: false,
      rollback_result: 'not_needed',
    });
  }

  try {
    const markdown = fs.readFileSync(guidePath, 'utf8');
    return successRead('blueprinthelper_read_agent_guide', { target_type: 'asset' }, {
      schema: 'AgentGuideMarkdown.v1',
      format: 'markdown',
      markdown,
    }) as ToolResultBase;
  } catch (err) {
    return failureResult('blueprinthelper_read_agent_guide', {
      code: 'agent_guide_read_failed',
      stage: 'execute',
      message: err instanceof Error ? err.message : String(err),
      retryable: false,
      rollback_result: 'not_needed',
    });
  }
}

function resolveAgentGuidePath(cwd: string): string | undefined {
  const candidateRoots = ancestorDirs(path.resolve(cwd));
  for (const root of candidateRoots) {
    const candidates = [
      path.join(root, agentGuideRelativePath),
      path.join(root, agentGuidePackagedRelativePath),
      ...agentGuideProjectPluginRelativePaths.map((relativePath) => path.join(root, relativePath)),
    ];
    for (const candidate of candidates) {
      if (fs.existsSync(candidate)) {
        return candidate;
      }
    }
  }
  return undefined;
}

function ancestorDirs(start: string): string[] {
  const dirs: string[] = [];
  let current = start;
  for (;;) {
    dirs.push(current);
    const parent = path.dirname(current);
    if (parent === current) {
      return dirs;
    }
    current = parent;
  }
}

function readStaticDiagnostics(context: BlueprintHelperToolContext): ToolResultBase {
  const agentProfilePath = path.join(context.cwd, '.blueprinthelper', 'agent-profile.json');
  const report: DiagnosticsMarkdownReport = {
    blocking: fs.existsSync(agentProfilePath) ? [] : [{ code: 'agent_profile_missing' }],
    warnings: [],
    info: [{ code: 'cwd', extra: context.cwd }],
  };
  const markdown = buildDiagnosticsMarkdown(report);
  return successRead(
    'blueprinthelper_diagnostics',
    { target_type: 'asset' },
    buildDiagnosticsData('static', markdown),
  ) as ToolResultBase;
}

async function buildProject(
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const projectFile = readRequiredString(input, 'project_file');
  const uprojectFile = resolveExplicitProjectFile(projectFile);
  const ueEngineDir = resolveProjectEngineDir(uprojectFile, { ueEngineDir: context.ueEngineDir });
  const buildBat = path.join(ueEngineDir, 'Engine', 'Build', 'BatchFiles', 'Build.bat');
  const projectName = path.basename(uprojectFile, '.uproject');
  const buildTarget = readOptionalString(input, 'target') ?? `${projectName}Editor`;
  const buildConfig = readOptionalString(input, 'configuration') ?? 'Development';
  const buildPlatform = readOptionalString(input, 'platform') ?? 'Win64';
  const result = await runProcess(
    context,
    buildBat,
    [buildTarget, buildPlatform, buildConfig, uprojectFile, '-WaitMutex'],
    { timeoutMs: 600_000 },
  );
  const output = `${result.stdout}${result.stderr ? `\n--- stderr ---\n${result.stderr}` : ''}`;
  if (result.exitCode !== 0) {
    return failureResult('blueprint_build_project', {
      code: 'build_failed',
      stage: 'execute',
      message: `Build failed with exit code ${result.exitCode}.`,
      retryable: true,
      rollback_result: 'not_needed',
    }, { target_type: 'asset' });
  }
  return successRead('blueprint_build_project', { target_type: 'asset' }, {
    schema: 'LocalProcessResult.v1',
    message: 'Build succeeded.',
    output: output.slice(-4000),
  }) as ToolResultBase;
}

async function openEditor(
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const projectFile = readRequiredString(input, 'project_file');
  const uprojectFile = resolveExplicitProjectFile(projectFile);
  const ueEngineDir = resolveProjectEngineDir(uprojectFile, { ueEngineDir: context.ueEngineDir });
  const editorExe = path.join(ueEngineDir, 'Engine', 'Binaries', 'Win64', 'UnrealEditor.exe');
  const timeoutMs = readOptionalNumber(input, 'wait_timeout_ms') ?? 120_000;
  await runProcess(context, editorExe, [uprojectFile], { detached: true });

  const startTime = Date.now();
  const sleep = context.sleep ?? ((ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)));
  while (Date.now() - startTime < timeoutMs) {
    await sleep(3000);
    if (await context.bridge.ping()) {
      return successRead('blueprint_open_editor', { target_type: 'asset' }, {
        schema: 'EditorLaunchResult.v1',
        code: 'EDITOR_BRIDGE_AVAILABLE',
        editor_exe: editorExe,
        uproject_path: uprojectFile,
        elapsed_ms: Date.now() - startTime,
      }) as ToolResultBase;
    }
  }

  return failureResult('blueprint_open_editor', {
    code: 'EDITOR_STARTED_BRIDGE_TIMEOUT',
    stage: 'execute',
    message: `Unreal Editor started but Bridge did not become available within ${timeoutMs}ms.`,
    retryable: true,
    rollback_result: 'not_needed',
  }, { target_type: 'asset' });
}

function runProcess(
  context: BlueprintHelperToolContext,
  command: string,
  args: string[],
  options: { timeoutMs?: number; detached?: boolean } = {},
): Promise<LocalProcessResult> {
  if (context.runLocalProcess) {
    return context.runLocalProcess(command, args, options);
  }

  if (options.detached) {
    const child = spawn(command, args, {
      detached: true,
      stdio: 'ignore',
      windowsHide: true,
    });
    child.unref();
    return Promise.resolve({ exitCode: 0, stdout: '', stderr: '' });
  }

  return new Promise((resolve) => {
    execFile(command, args, {
      maxBuffer: 10 * 1024 * 1024,
      timeout: options.timeoutMs,
    }, (error, stdout, stderr) => {
      resolve({
        exitCode: error && typeof error.code === 'number' ? error.code : error ? 1 : 0,
        stdout,
        stderr,
      });
    });
  });
}

function readRequiredString(input: Record<string, unknown>, field: string): string {
  const value = input[field];
  if (typeof value !== 'string' || value.length === 0) {
    throw new Error(`${field} is required.`);
  }
  return value;
}

function readOptionalString(input: Record<string, unknown>, field: string): string | undefined {
  const value = input[field];
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

function readOptionalNumber(input: Record<string, unknown>, field: string): number | undefined {
  const value = input[field];
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}
