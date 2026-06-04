import * as fs from 'node:fs';
import * as path from 'node:path';

export interface ProjectEngineDirConfig {
  ueEngineDir?: string;
}

const PROJECT_PROFILE_RELATIVE_PATH = path.join('.blueprinthelper', 'project-profile.json');
const LEGACY_AGENT_PROFILE_RELATIVE_PATH = path.join('.blueprinthelper', 'agent-profile.json');

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

function makeProjectEngineDirMissingError(
  projectFile: string,
  projectProfilePath: string,
  legacyAgentProfilePath: string,
  detail: string,
  activeProfilePath?: string,
): Error {
  return new Error(
    JSON.stringify(
      {
        success: false,
        code: 'PROJECT_PROFILE_ENGINE_DIR_MISSING',
        message:
          'Project profile must define environment.ue_engine_dir for this Unreal project.',
        detail,
        project_file: projectFile,
        project_profile_path: projectProfilePath,
        legacy_agent_profile_path: legacyAgentProfilePath,
        profile_path: activeProfilePath ?? projectProfilePath,
        expected_field: 'environment.ue_engine_dir',
        agent_instruction:
          'Run install.cmd from the BlueprintHelper repository root with -ProjectFile and -EngineRoot, or update .blueprinthelper/project-profile.json. Do not write project-specific UE paths to global Claude settings.',
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
  const projectProfilePath = path.join(projectDir, PROJECT_PROFILE_RELATIVE_PATH);
  const legacyAgentProfilePath = path.join(projectDir, LEGACY_AGENT_PROFILE_RELATIVE_PATH);
  const profilePath = fs.existsSync(projectProfilePath)
    ? projectProfilePath
    : fs.existsSync(legacyAgentProfilePath)
      ? legacyAgentProfilePath
      : undefined;

  if (profilePath) {
    let profile: unknown;
    try {
      profile = JSON.parse(fs.readFileSync(profilePath, 'utf8'));
    } catch (err) {
      throw new Error(
        JSON.stringify(
          {
            success: false,
            code: 'PROJECT_PROFILE_INVALID_JSON',
            message: 'Project profile exists but could not be parsed as JSON.',
            project_file: uprojectFile,
            project_profile_path: projectProfilePath,
            legacy_agent_profile_path: legacyAgentProfilePath,
            profile_path: profilePath,
            error: err instanceof Error ? err.message : String(err),
            agent_instruction:
              'Fix .blueprinthelper/project-profile.json or rerun install.cmd from the BlueprintHelper repository root with -ProjectFile and -EngineRoot.',
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
      projectProfilePath,
      legacyAgentProfilePath,
      `${path.basename(profilePath)} exists, but environment.ue_engine_dir is empty or missing.`,
      profilePath,
    );
  }

  if (config.ueEngineDir?.trim()) {
    return path.normalize(config.ueEngineDir);
  }

  throw makeProjectEngineDirMissingError(
    uprojectFile,
    projectProfilePath,
    legacyAgentProfilePath,
    'project-profile.json was not found, legacy agent-profile.json was not found, and no legacy UE engine fallback is configured.',
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
