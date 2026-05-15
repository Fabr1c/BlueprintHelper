import * as path from 'node:path';
import {
  failureResult,
  successRead,
  type ToolResultBase,
} from '../../result/tool-result.js';
import { resolveProjectEngineDir } from '../../project-profile/agent-profile.js';
import { resolveExplicitProjectFile } from '../../project-profile/editor-paths.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { readOptionalString, readRequiredString } from './input-readers.js';
import { runProcess } from './process-runner.js';

export async function buildProject(
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
