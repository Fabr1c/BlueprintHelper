import * as fs from 'node:fs';
import * as path from 'node:path';

export interface ProjectEngineDirConfig {
  ueEngineDir?: string;
}

const PROJECT_AGENT_PROFILE_RELATIVE_PATH = path.join('.blueprinthelper', 'agent-profile.json');

function expandProjectProfilePathVars(rawPath: string, projectDir: string): string {
  return rawPath
    .replace(/\$\{projectDir\}/gi, projectDir)
    .replace(/\$\{workspaceFolder\}/gi, projectDir)
    .replace(/\$\{workspaceRoot\}/gi, projectDir)
    .replace(/\$\{userHome\}/gi, process.env['USERPROFILE'] ?? process.env['HOME'] ?? '');
}

function resolveProfilePathValue(rawPath: string, projectDir: string): string {
  const expanded = expandProjectProfilePathVars(rawPath.trim(), projectDir);
  return path.isAbsolute(expanded) ? path.normalize(expanded) : path.resolve(projectDir, expanded);
}

function makeProjectEngineDirMissingError(projectFile: string, profilePath: string, detail: string): Error {
  return new Error(
    JSON.stringify(
      {
        success: false,
        code: 'PROJECT_AGENT_PROFILE_ENGINE_DIR_MISSING',
        message:
          'Project agent profile must define environment.ue_engine_dir for this Unreal project.',
        detail,
        project_file: projectFile,
        agent_profile_path: profilePath,
        expected_field: 'environment.ue_engine_dir',
        agent_instruction:
          'Run install.cmd from the BlueprintHelper repository root with -ProjectFile and -EngineRoot, or update this project with /blueprint-helper:configure. Do not write project-specific UE paths to global Claude settings.',
      },
      null,
      2,
    ),
  );
}

export function resolveProjectEngineDir(
  uprojectFile: string,
  config: ProjectEngineDirConfig,
): string {
  const projectDir = path.dirname(uprojectFile);
  const profilePath = path.join(projectDir, PROJECT_AGENT_PROFILE_RELATIVE_PATH);

  if (fs.existsSync(profilePath)) {
    let profile: unknown;
    try {
      profile = JSON.parse(fs.readFileSync(profilePath, 'utf8'));
    } catch (err) {
      throw new Error(
        JSON.stringify(
          {
            success: false,
            code: 'PROJECT_AGENT_PROFILE_INVALID_JSON',
            message: 'Project agent profile exists but could not be parsed as JSON.',
            project_file: uprojectFile,
            agent_profile_path: profilePath,
            error: err instanceof Error ? err.message : String(err),
            agent_instruction:
              'Fix .blueprinthelper/agent-profile.json or rerun install.cmd from the BlueprintHelper repository root with -ProjectFile and -EngineRoot.',
          },
          null,
          2,
        ),
      );
    }

    const root = isRecord(profile) ? profile : undefined;
    const environment = getRecordField(root, 'environment');
    const rawEngineDir =
      getStringField(environment, 'ue_engine_dir') ??
      getStringField(environment, 'UE_ENGINE_DIR');

    if (rawEngineDir?.trim()) {
      return resolveProfilePathValue(rawEngineDir, projectDir);
    }

    throw makeProjectEngineDirMissingError(
      uprojectFile,
      profilePath,
      'agent-profile.json exists, but environment.ue_engine_dir is empty or missing.',
    );
  }

  if (config.ueEngineDir?.trim()) {
    return path.normalize(config.ueEngineDir);
  }

  throw makeProjectEngineDirMissingError(
    uprojectFile,
    profilePath,
    'agent-profile.json was not found and no legacy UE engine fallback is configured.',
  );
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function getRecordField(record: Record<string, unknown> | undefined, field: string): Record<string, unknown> | undefined {
  const value = record?.[field];
  return isRecord(value) ? value : undefined;
}

function getStringField(record: Record<string, unknown> | undefined, field: string): string | undefined {
  const value = record?.[field];
  return typeof value === 'string' ? value : undefined;
}
