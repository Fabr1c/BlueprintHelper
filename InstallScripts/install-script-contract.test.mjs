import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import test from 'node:test';

const require = createRequire(import.meta.url);
const { resolveBlueprintHelperUserHome } = require('../CodexPlugin/scripts/user-home.cjs');

const installScript = await readFile(new URL('./install.ps1', import.meta.url), 'utf8');
const installPrompts = await readFile(new URL('./install-prompts.mjs', import.meta.url), 'utf8');
const uninstallScript = await readFile(new URL('./uninstall.ps1', import.meta.url), 'utf8');
const installDocs = await readFile(new URL('../AgentFaceService/docs/Install_CLI_QuickStart.md', import.meta.url), 'utf8');
const codexGlobalMcpInstaller = await readFile(new URL('../CodexPlugin/scripts/install-global-mcp.cjs', import.meta.url), 'utf8');
const codexAgentInstaller = await readFile(new URL('../CodexPlugin/scripts/install-codex-agents.cjs', import.meta.url), 'utf8');
const claudeAgentInstaller = await readFile(new URL('../CodexPlugin/scripts/install-claude-agents.cjs', import.meta.url), 'utf8');
const userHomeHelper = await readFile(new URL('../CodexPlugin/scripts/user-home.cjs', import.meta.url), 'utf8');

test('install.ps1 writes project profile as UTF-8 without BOM', () => {
  assert.match(installScript, /function Write-Utf8NoBomFile\b/);
  assert.match(installScript, /System\.Text\.UTF8Encoding\(\$false\)/);
  assert.match(installScript, /System\.IO\.File\]::WriteAllText/);

  const projectProfileWrite = /\$Profile\s*\|\s*ConvertTo-Json\s+-Depth\s+20\s*\|\s*Set-Content\s+-LiteralPath\s+\$ProfilePath\s+-Encoding\s+utf8/i;
  assert.doesNotMatch(installScript, projectProfileWrite);
  assert.match(installScript, /Write-Utf8NoBomFile\s+-Path\s+\$ProfilePath\s+-Value\s+\(\$Profile\s*\|\s*ConvertTo-Json\s+-Depth\s+20\)/);
});

test('install.ps1 can run one project UBT compile after install', () => {
  assert.match(installScript, /\[switch\]\$SkipProjectUbtCompile/);
  assert.match(installScript, /\[string\]\$ProjectEditorTarget/);
  assert.match(installScript, /function Invoke-ProjectUbtCompile\b/);
  assert.match(installScript, /function Resolve-ProjectEditorTargetName\b/);
  assert.match(installScript, /Multiple project editor targets were found/);
  assert.match(installScript, /Build\\BatchFiles\\Build\.bat/);
  assert.match(installScript, /-NoHotReloadFromIDE/);
  assert.match(installScript, /Invoke-ProjectUbtCompile\s+-ProjectProfileResult\s+\$ProjectProfileResult/);
  assert.match(installScript, /Project UBT compile:/);
});

test('install-prompts.mjs exposes project UBT compile selection', () => {
  assert.match(installPrompts, /makeOption\('projectUbtCompile'.*bool\(input\.projectUbtCompile,\s*true\)/);
  assert.match(installPrompts, /projectEditorTarget: normalizeString\(defaults\.paths\?\.projectEditorTarget\)/);
  assert.match(installPrompts, /const projectUbtCompile = optionSelected\(items,\s*'projectUbtCompile'\)/);
  assert.match(installPrompts, /if \(!projectProfile && !projectUbtCompile && !ueEnginePlugin\)/);
  assert.match(installPrompts, /if \(projectProfile \|\| projectUbtCompile\)/);
  assert.match(installPrompts, /if \(projectUbtCompile\)/);
  assert.match(installPrompts, /ProjectEditorTarget:/);
  assert.match(installPrompts, /projectUbtCompile: optionSelected\(items,\s*'projectUbtCompile'\)/);
  assert.match(installPrompts, /projectEditorTarget: normalizeString\(selectedPaths\.projectEditorTarget\)/);
  assert.match(installPrompts, /if \(selectedOptions\.projectUbtCompile\) enabled\.push\('project-ubt-compile'\)/);
});

test('install-prompts.mjs uses shared install JSON helper for defaults', () => {
  assert.match(installPrompts, /from '\.\/json-input\.mjs'/);
  assert.match(installPrompts, /readJsonFile\(defaultsPath\)/);
  assert.doesNotMatch(installPrompts, /\.replace\(\/\^\\uFEFF\/,\s*''\)/);
});

test('install quickstart documents default UBT compile and skip switch', () => {
  assert.match(installDocs, /-SkipProjectUbtCompile/);
  assert.match(installDocs, /-ProjectEditorTarget <TargetName>/);
  assert.match(installDocs, /Build\.bat <ProjectName>Editor Win64 Development -Project=<Project\.uproject> -WaitMutex -NoHotReloadFromIDE/);
  assert.match(installDocs, /installer runs one UBT compile by default/);
  assert.match(installDocs, /custom Editor target name or has multiple `\*Editor\.Target\.cs` files/);
});

test('install and uninstall resolve Codex and Claude config paths from the real Windows user profile', () => {
  assert.match(installScript, /function Resolve-BlueprintHelperUserHome\b/);
  assert.match(installScript, /function Get-WindowsUsersRoot\b/);
  assert.match(installScript, /Join-Path\s+\$UsersRoot\s+\$env:USERNAME/);
  assert.doesNotMatch(installScript, /Get-ChildItem\s+-LiteralPath\s+\$UsersRoot\s+-Directory/);
  assert.doesNotMatch(installScript, /Join-Path\s+\$Child\.FullName\s+\$ProductDirectory/);
  assert.match(installScript, /\$UserHome\s*=\s*Resolve-BlueprintHelperUserHome\b/);
  assert.doesNotMatch(installScript, /\$UserHome\s*=\s*\$env:USERPROFILE/);

  assert.match(uninstallScript, /function Resolve-BlueprintHelperUserHome\b/);
  assert.match(uninstallScript, /function Get-WindowsUsersRoot\b/);
  assert.match(uninstallScript, /Join-Path\s+\$UsersRoot\s+\$env:USERNAME/);
  assert.doesNotMatch(uninstallScript, /Get-ChildItem\s+-LiteralPath\s+\$UsersRoot\s+-Directory/);
  assert.match(uninstallScript, /Join-Path\s+\(Get-UserHome\)\s+'.codex\\config\.toml'/);
  assert.match(uninstallScript, /Join-Path\s+\(Get-UserHome\)\s+'.claude\\settings\.json'/);
  assert.doesNotMatch(uninstallScript, /\$HomeDir\s*=\s*\$env:USERPROFILE/);
});

test('node install helpers resolve user profile through shared Windows-aware helper', () => {
  for (const source of [codexGlobalMcpInstaller, codexAgentInstaller, claudeAgentInstaller]) {
    assert.match(source, /require\('\.\/user-home\.cjs'\)/);
    assert.match(source, /resolveBlueprintHelperUserHome\(/);
    assert.doesNotMatch(source, /process\.env\.USERPROFILE\s*\|\|\s*process\.env\.HOME/);
  }
});

test('node user home helper ignores polluted profile env and uses the current C:\\Users username directory', () => {
  const originalUserProfile = process.env.USERPROFILE;
  const originalHome = process.env.HOME;
  const originalUsername = process.env.USERNAME;
  const username = originalUsername || path.basename(originalUserProfile || '');
  assert.ok(username, 'test requires a Windows username');

  try {
    process.env.USERPROFILE = process.cwd();
    process.env.HOME = process.cwd();
    process.env.USERNAME = username;

    assert.equal(resolveBlueprintHelperUserHome(), path.join('C:\\Users', username));
    assert.doesNotMatch(userHomeHelper, /fs\.readdirSync\(usersRoot\)/);
  } finally {
    process.env.USERPROFILE = originalUserProfile;
    process.env.HOME = originalHome;
    process.env.USERNAME = originalUsername;
  }
});

test('node user home helper rejects another C:\\Users account from polluted profile env', () => {
  const originalUserProfile = process.env.USERPROFILE;
  const originalHome = process.env.HOME;
  const originalUsername = process.env.USERNAME;
  const username = originalUsername || path.basename(originalUserProfile || '');
  assert.ok(username, 'test requires a Windows username');

  try {
    process.env.USERPROFILE = path.join('C:\\Users', 'Public');
    process.env.HOME = path.join('C:\\Users', 'Public');
    process.env.USERNAME = username;

    assert.equal(resolveBlueprintHelperUserHome(), path.join('C:\\Users', username));
  } finally {
    process.env.USERPROFILE = originalUserProfile;
    process.env.HOME = originalHome;
    process.env.USERNAME = originalUsername;
  }
});
