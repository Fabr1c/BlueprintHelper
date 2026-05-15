import * as path from 'node:path';
import type { BlueprintHelperToolContext } from '../../types.js';
import { normalizeProcessPath } from '../path-utils.js';
import { runProcess } from '../process-runner.js';

export interface UnrealEditorProcessInfo {
  pid: number;
  commandLine: string;
}

export async function findUnrealEditorProcesses(
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

export async function waitForEditorProcessesToExit(
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
