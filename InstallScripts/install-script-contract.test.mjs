import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const installScript = await readFile(new URL('./install.ps1', import.meta.url), 'utf8');
const installPrompts = await readFile(new URL('./install-prompts.mjs', import.meta.url), 'utf8');
const installDocs = await readFile(new URL('../AgentFaceService/docs/Install_CLI_QuickStart.md', import.meta.url), 'utf8');

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

test('install quickstart documents default UBT compile and skip switch', () => {
  assert.match(installDocs, /-SkipProjectUbtCompile/);
  assert.match(installDocs, /-ProjectEditorTarget <TargetName>/);
  assert.match(installDocs, /Build\.bat <ProjectName>Editor Win64 Development -Project=<Project\.uproject> -WaitMutex -NoHotReloadFromIDE/);
  assert.match(installDocs, /installer runs one UBT compile by default/);
  assert.match(installDocs, /custom Editor target name or has multiple `\*Editor\.Target\.cs` files/);
});
