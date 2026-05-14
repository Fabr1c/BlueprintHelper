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
  'blueprint_close_editor',
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
  if (name === 'blueprint_close_editor') {
    return closeEditor(input, context);
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
  const uprojectFile = resolveProjectFileFromInput(input, context);
  const ueEngineDir = resolveProjectEngineDir(uprojectFile, { ueEngineDir: context.ueEngineDir });
  const editorExe = path.join(ueEngineDir, 'Engine', 'Binaries', 'Win64', 'UnrealEditor.exe');
  const timeoutMs = readOptionalNumber(input, 'wait_timeout_ms') ?? 240_000;
  const editorArgs = buildDefaultEditorArgs(uprojectFile, readOptionalStringArray(input, 'editor_args'));
  await runProcess(context, editorExe, [uprojectFile, ...editorArgs], { detached: true });

  const startTime = Date.now();
  const sleep = context.sleep ?? ((ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)));
  let stablePingCount = 0;
  while (Date.now() - startTime < timeoutMs) {
    await sleep(3000);
    const alive = await safeBridgePing(context);
    stablePingCount = alive ? stablePingCount + 1 : 0;
    if (stablePingCount >= 2) {
      return successRead('blueprint_open_editor', { target_type: 'asset' }, {
        schema: 'EditorLaunchResult.v1',
        code: 'EDITOR_BRIDGE_AVAILABLE',
        editor_exe: editorExe,
        uproject_path: uprojectFile,
        editor_args: editorArgs,
        launch_command: `"${editorExe}" "${uprojectFile}" ${editorArgs.join(' ')}`.trim(),
        elapsed_ms: Date.now() - startTime,
        stable_ping_count: stablePingCount,
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

async function closeEditor(
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const startTime = Date.now();
  const saveAll = readOptionalBoolean(input, 'save_all') ?? true;
  const timeoutMs = readOptionalNumber(input, 'wait_timeout_ms') ?? 60_000;
  const force = readOptionalBoolean(input, 'force') ?? false;
  let uprojectFile: string | undefined;
  try {
    uprojectFile = resolveProjectFileFromInput(input, context);
  } catch {
    uprojectFile = undefined;
  }

  let bridgeError: string | undefined;
  try {
    const response = await context.bridge.sendCommand('close_editor', { save_all: saveAll });
    if (response.success) {
      await waitForBridgeUnavailable(context, timeoutMs);
      return successRead('blueprint_close_editor', { target_type: 'asset' }, {
        schema: 'EditorCloseResult.v1',
        code: 'EDITOR_CLOSE_REQUESTED',
        method: 'bridge',
        save_all: saveAll,
        elapsed_ms: Date.now() - startTime,
        bridge_response: response.result ?? {},
      }) as ToolResultBase;
    }
    bridgeError = response.message ?? response.error_code ?? 'Bridge close_editor failed.';
  } catch (err) {
    bridgeError = err instanceof Error ? err.message : String(err);
  }

  const processes = await findUnrealEditorProcesses(context, uprojectFile);
  if (processes.length === 0) {
    return failureResult('blueprint_close_editor', {
      code: 'EDITOR_PROCESS_NOT_FOUND',
      stage: 'execute',
      message: bridgeError
        ? `Bridge close failed and no matching UnrealEditor process was found. Bridge: ${bridgeError}`
        : 'No matching UnrealEditor process was found.',
      retryable: true,
      rollback_result: 'not_needed',
    }, { target_type: 'asset' });
  }

  const killedPids: number[] = [];
  for (const processInfo of processes) {
    const args = ['/PID', String(processInfo.pid), '/T'];
    if (force) {
      args.push('/F');
    }
    const result = await runProcess(context, 'taskkill.exe', args, { timeoutMs: 15_000 });
    if (result.exitCode === 0) {
      killedPids.push(processInfo.pid);
    }
  }

  const closed = await waitForEditorProcessesToExit(context, uprojectFile, timeoutMs);
  if (!closed) {
    return failureResult('blueprint_close_editor', {
      code: 'EDITOR_CLOSE_TIMEOUT',
      stage: 'execute',
      message: `Requested close for UnrealEditor process(es), but they did not exit within ${timeoutMs}ms.`,
      retryable: true,
      rollback_result: 'not_needed',
    }, { target_type: 'asset' });
  }

  return successRead('blueprint_close_editor', { target_type: 'asset' }, {
    schema: 'EditorCloseResult.v1',
    code: 'EDITOR_PROCESS_CLOSED',
    method: force ? 'taskkill_force' : 'taskkill',
    project_file: uprojectFile,
    pids: killedPids,
    elapsed_ms: Date.now() - startTime,
    bridge_error: bridgeError,
  }) as ToolResultBase;
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
    if (process.platform === 'win32') {
      const script = [
        '$ErrorActionPreference = "Stop"',
        `$ArgumentList = @(${args.map(quotePowerShellString).join(', ')})`,
        `Start-Process -FilePath ${quotePowerShellString(command)} -ArgumentList $ArgumentList -WorkingDirectory ${quotePowerShellString(path.dirname(command))}`,
      ].join('; ');
      return new Promise((resolve) => {
        execFile('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', script], {
          maxBuffer: 1024 * 1024,
          timeout: 30_000,
        }, (error, stdout, stderr) => {
          resolve({
            exitCode: error && typeof error.code === 'number' ? error.code : error ? 1 : 0,
            stdout,
            stderr,
          });
        });
      });
    }

    const child = spawn(command, args, {
      detached: true,
      stdio: 'ignore',
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

function quotePowerShellString(value: string): string {
  return `'${value.replace(/'/g, "''")}'`;
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

function readOptionalBoolean(input: Record<string, unknown>, field: string): boolean | undefined {
  const value = input[field];
  return typeof value === 'boolean' ? value : undefined;
}

function readOptionalNumber(input: Record<string, unknown>, field: string): number | undefined {
  const value = input[field];
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

function readOptionalStringArray(input: Record<string, unknown>, field: string): string[] {
  const value = input[field];
  if (value === undefined || value === null) {
    return [];
  }
  if (typeof value === 'string') {
    const trimmed = value.trim();
    return trimmed.length > 0 ? trimmed.split(/\s+/) : [];
  }
  if (!Array.isArray(value)) {
    throw new Error(`${field} must be a string array.`);
  }
  return value.map((item, index) => {
    if (typeof item !== 'string') {
      throw new Error(`${field}[${index}] must be a string.`);
    }
    return item;
  });
}

function resolveProjectFileFromInput(input: Record<string, unknown>, context: BlueprintHelperToolContext): string {
  const explicitProjectFile = readOptionalString(input, 'project_file');
  if (explicitProjectFile) {
    return resolveExplicitProjectFile(explicitProjectFile);
  }
  return discoverProjectFile(context.cwd);
}

function discoverProjectFile(cwd: string): string {
  for (const dir of ancestorDirs(path.resolve(cwd))) {
    const projectFiles = fs.readdirSync(dir)
      .filter((entry) => entry.toLowerCase().endsWith('.uproject'))
      .map((entry) => path.join(dir, entry));
    if (projectFiles.length === 1) {
      return projectFiles[0];
    }
    if (projectFiles.length > 1) {
      throw new Error(JSON.stringify({
        success: false,
        code: 'PROJECT_FILE_AMBIGUOUS',
        message: 'Multiple .uproject files were found while walking up from cwd.',
        cwd,
        directory: dir,
        project_files: projectFiles,
        agent_instruction: 'Pass project_file explicitly for this command.',
      }, null, 2));
    }
  }
  throw new Error(JSON.stringify({
    success: false,
    code: 'PROJECT_FILE_NOT_FOUND',
    message: 'No .uproject file was found from cwd or its parent directories.',
    cwd,
    agent_instruction: 'Run bh open_editor from the Unreal project directory or pass project_file explicitly.',
  }, null, 2));
}

function buildDefaultEditorArgs(uprojectFile: string, explicitArgs: string[]): string[] {
  const projectDir = path.dirname(uprojectFile);
  const shaderWorkingDir = path.join(projectDir, 'Intermediate', 'Shaders', 'WorkingDirectory', 'BlueprintHelperCli');
  fs.mkdirSync(shaderWorkingDir, { recursive: true });

  const args = [...explicitArgs];
  if (!hasSwitch(args, '-DDC-ForceMemoryCache') && !hasSwitchPrefix(args, '-ddc=')) {
    args.unshift('-DDC-ForceMemoryCache');
  }
  if (!hasSwitchPrefix(args, '-ShaderWorkingDir=')) {
    args.push(`-ShaderWorkingDir=${toUnrealPath(shaderWorkingDir)}/`);
  }
  if (!hasSwitch(args, '-NoSplash')) {
    args.push('-NoSplash');
  }
  return args;
}

function hasSwitch(args: string[], flag: string): boolean {
  return args.some((arg) => arg.toLowerCase() === flag.toLowerCase());
}

function hasSwitchPrefix(args: string[], prefix: string): boolean {
  return args.some((arg) => arg.toLowerCase().startsWith(prefix.toLowerCase()));
}

function toUnrealPath(value: string): string {
  return path.resolve(value).replace(/\\/g, '/');
}

async function safeBridgePing(context: BlueprintHelperToolContext): Promise<boolean> {
  try {
    return await context.bridge.ping();
  } catch {
    return false;
  }
}

async function waitForBridgeUnavailable(context: BlueprintHelperToolContext, timeoutMs: number): Promise<boolean> {
  const startTime = Date.now();
  const sleep = context.sleep ?? ((ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)));
  while (Date.now() - startTime < timeoutMs) {
    await sleep(1000);
    if (!await safeBridgePing(context)) {
      return true;
    }
  }
  return false;
}

interface UnrealEditorProcessInfo {
  pid: number;
  commandLine: string;
}

async function findUnrealEditorProcesses(
  context: BlueprintHelperToolContext,
  uprojectFile: string | undefined,
): Promise<UnrealEditorProcessInfo[]> {
  const script = [
    "$items = Get-CimInstance Win32_Process -Filter \"Name = 'UnrealEditor.exe'\" -ErrorAction SilentlyContinue | Select-Object ProcessId,CommandLine",
    'if ($null -eq $items) { return }',
    '$items | ConvertTo-Json -Compress',
  ].join('; ');
  const result = await runProcess(context, 'powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', script], { timeoutMs: 10_000 });
  if (result.exitCode !== 0 || result.stdout.trim().length === 0) {
    return [];
  }

  const parsed = JSON.parse(result.stdout) as unknown;
  const records = Array.isArray(parsed) ? parsed : [parsed];
  const processes = records
    .map((record) => {
      if (!record || typeof record !== 'object') {
        return undefined;
      }
      const value = record as Record<string, unknown>;
      const pid = typeof value['ProcessId'] === 'number' ? value['ProcessId'] : Number(value['ProcessId']);
      const commandLine = typeof value['CommandLine'] === 'string' ? value['CommandLine'] : '';
      return Number.isFinite(pid) ? { pid, commandLine } : undefined;
    })
    .filter((record): record is UnrealEditorProcessInfo => record !== undefined);

  if (!uprojectFile) {
    return processes;
  }

  const normalizedProject = normalizeProcessPath(uprojectFile);
  const projectName = path.basename(uprojectFile).toLowerCase();
  return processes.filter((processInfo) => {
    const normalizedCommand = normalizeProcessPath(processInfo.commandLine);
    return normalizedCommand.includes(normalizedProject) || normalizedCommand.includes(projectName);
  });
}

async function waitForEditorProcessesToExit(
  context: BlueprintHelperToolContext,
  uprojectFile: string | undefined,
  timeoutMs: number,
): Promise<boolean> {
  const startTime = Date.now();
  const sleep = context.sleep ?? ((ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)));
  while (Date.now() - startTime < timeoutMs) {
    await sleep(1000);
    if ((await findUnrealEditorProcesses(context, uprojectFile)).length === 0) {
      return true;
    }
  }
  return false;
}

function normalizeProcessPath(value: string): string {
  return value.replace(/\\/g, '/').toLowerCase();
}
