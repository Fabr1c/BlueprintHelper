import * as fs from 'node:fs';
import * as path from 'node:path';
import { resolveExplicitProjectFile } from '../../project-profile/editor-paths.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { readOptionalString } from './input-readers.js';
import { ancestorDirs } from './path-utils.js';

export function resolveProjectFileFromInput(input: Record<string, unknown>, context: BlueprintHelperToolContext): string {
  const explicitProjectFile = readOptionalString(input, 'project_file');
  if (explicitProjectFile) {
    return resolveExplicitProjectFile(explicitProjectFile);
  }
  return discoverProjectFile(context.cwd);
}

export function discoverProjectFile(cwd: string): string {
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
