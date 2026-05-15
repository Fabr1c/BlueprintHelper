import * as path from 'node:path';
import {
  failureResult,
  successRead,
  type ToolResultBase,
} from '../../../result/tool-result.js';
import { resolveProjectEngineDir } from '../../../project-profile/agent-profile.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { readOptionalNumber, readOptionalStringArray } from '../input-readers.js';
import { resolveProjectFileFromInput } from '../project-file-resolver.js';
import { runProcess } from '../process-runner.js';
import { safeBridgePing } from './bridge-lifecycle.js';
import { buildDefaultEditorArgs } from './editor-args.js';

export async function openEditor(
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
