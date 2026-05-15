import {
  failureResult,
  successRead,
  type ToolResultBase,
} from '../../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { readOptionalBoolean, readOptionalNumber } from '../input-readers.js';
import { resolveProjectFileFromInput } from '../project-file-resolver.js';
import { runProcess } from '../process-runner.js';
import { waitForBridgeUnavailable } from './bridge-lifecycle.js';
import { findUnrealEditorProcesses, waitForEditorProcessesToExit } from './editor-processes.js';

export async function closeEditor(
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
