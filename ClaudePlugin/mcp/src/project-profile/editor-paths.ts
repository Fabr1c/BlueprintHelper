import * as path from 'node:path';

export function resolveExplicitProjectFile(projectFile: string): string {
  const resolvedProjectFile = path.resolve(projectFile);
  if (path.extname(resolvedProjectFile).toLowerCase() !== '.uproject') {
    throw new Error(
      JSON.stringify(
        {
          success: false,
          code: 'PROJECT_FILE_NOT_UPROJECT',
          message: 'project_file must point to a .uproject file for the target Unreal project.',
          received_project_file: projectFile,
          resolved_project_file: resolvedProjectFile,
          agent_instruction:
            'Find the target .uproject in the current workspace and pass its absolute path as project_file.',
        },
        null,
        2,
      ),
    );
  }
  return resolvedProjectFile;
}
