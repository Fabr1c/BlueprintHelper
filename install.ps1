[CmdletBinding(SupportsShouldProcess = $true)]
param(
  [switch]$SkipBuild,
  [switch]$SkipCodexMarketplace,
  [switch]$SkipCodexAgents,
  [switch]$SkipLifecycleMcp,
  [switch]$SkipCliLink,
  [switch]$SkipProjectProfile,
  [switch]$SkipDefaultPreferences,
  [switch]$InstallClaudeAgents,
  [switch]$InstallClaudePlugin,
  [switch]$InstallUePluginToEngine,
  [switch]$RunDiagnostics,
  [switch]$Interactive,
  [string]$ProjectFile,
  [string]$EngineRoot,
  [string]$EnginePluginDir,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'
$script:ThisCmdlet = $PSCmdlet

$Root = $PSScriptRoot
$CodexPluginRoot = Join-Path $Root 'CodexPlugin'
$ClaudePluginRoot = Join-Path $Root 'ClaudePlugin'
$AgentFaceServiceRoot = Join-Path $Root 'AgentFaceService'
$UePluginRoot = Join-Path $Root 'BlueprintHelper'

function Resolve-CommandPath {
  param(
    [Parameter(Mandatory = $true)]
    [string[]]$Names,
    [Parameter(Mandatory = $true)]
    [string]$DisplayName
  )

  foreach ($Name in $Names) {
    $Command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($Command) {
      return $Command.Source
    }
  }

  throw "$DisplayName was not found on PATH."
}

function Assert-Directory {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path,
    [Parameter(Mandatory = $true)]
    [string]$Name
  )

  if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
    throw "Missing $Name directory: $Path"
  }
}

function Assert-File {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path,
    [Parameter(Mandatory = $true)]
    [string]$Name
  )

  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    throw "Missing $Name file: $Path"
  }
}

function Invoke-External {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Description,
    [Parameter(Mandatory = $true)]
    [string]$FilePath,
    [Parameter(Mandatory = $true)]
    [string[]]$Arguments
  )

  Write-Host "==> $Description"
  if ($script:ThisCmdlet.ShouldProcess($Description, "$FilePath $($Arguments -join ' ')")) {
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
      throw "$Description failed with exit code $LASTEXITCODE."
    }
  }
}

function Invoke-Npm {
  param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,
    [Parameter(Mandatory = $true)]
    [string[]]$Arguments
  )

  Invoke-External -Description "npm --prefix $PackageDir $($Arguments -join ' ')" -FilePath $script:NpmCommand -Arguments (@('--prefix', $PackageDir) + $Arguments)
}

function Read-InstallText {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Prompt,
    [string]$DefaultValue = ''
  )

  $Suffix = if ($DefaultValue) { " [$DefaultValue]" } else { '' }
  $Value = Read-Host "$Prompt$Suffix"
  if ([string]::IsNullOrWhiteSpace($Value)) {
    return $DefaultValue
  }
  return $Value.Trim()
}

function Read-InstallYesNo {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Prompt,
    [bool]$DefaultYes = $true
  )

  $Hint = if ($DefaultYes) { 'Y/n' } else { 'y/N' }
  while ($true) {
    $Raw = Read-Host "$Prompt [$Hint]"
    if ([string]::IsNullOrWhiteSpace($Raw)) {
      return $DefaultYes
    }

    switch -Regex ($Raw.Trim()) {
      '^(y|yes|Y|YES|是|好|1)$' { return $true }
      '^(n|no|N|NO|否|不|0)$' { return $false }
      default { Write-Host 'Please answer y or n.' }
    }
  }
}

function Invoke-InteractiveInstallWizard {
  Write-Host ''
  Write-Host 'BlueprintHelper interactive install'
  Write-Host "Source root: $Root"
  Write-Host ''

  $script:SkipBuild = -not (Read-InstallYesNo -Prompt 'Build AgentFaceService packages' -DefaultYes:(-not $SkipBuild))
  $script:SkipCliLink = -not (Read-InstallYesNo -Prompt 'Link the bh CLI globally' -DefaultYes:(-not $SkipCliLink))

  if (Read-InstallYesNo -Prompt 'Install Codex Desktop plugin support' -DefaultYes:(-not ($SkipCodexMarketplace -and $SkipCodexAgents -and $SkipLifecycleMcp))) {
    $script:SkipCodexMarketplace = -not (Read-InstallYesNo -Prompt '  Register Codex local marketplace entry' -DefaultYes:(-not $SkipCodexMarketplace))
    $script:SkipCodexAgents = -not (Read-InstallYesNo -Prompt '  Install Codex subagents' -DefaultYes:(-not $SkipCodexAgents))
    $script:SkipLifecycleMcp = -not (Read-InstallYesNo -Prompt '  Install global lifecycle-only MCP config' -DefaultYes:(-not $SkipLifecycleMcp))
  } else {
    $script:SkipCodexMarketplace = $true
    $script:SkipCodexAgents = $true
    $script:SkipLifecycleMcp = $true
  }

  if (Read-InstallYesNo -Prompt 'Install Claude Code plugin support' -DefaultYes:$InstallClaudePlugin) {
    $script:InstallClaudePlugin = $true
    $script:InstallClaudeAgents = Read-InstallYesNo -Prompt '  Install Claude sideAgent definitions' -DefaultYes:$true
  } else {
    $script:InstallClaudePlugin = $false
    $script:InstallClaudeAgents = Read-InstallYesNo -Prompt 'Install only Claude sideAgent definitions' -DefaultYes:$InstallClaudeAgents
  }

  $script:SkipProjectProfile = -not (Read-InstallYesNo -Prompt 'Write or update project .blueprinthelper/agent-profile.json' -DefaultYes:(-not $SkipProjectProfile))
  if (-not $script:SkipProjectProfile) {
    $ProjectFileInput = Read-InstallText -Prompt '  Project .uproject path, blank to auto-detect' -DefaultValue $ProjectFile
    if ($ProjectFileInput) {
      $script:ProjectFile = $ProjectFileInput
    }

    $EngineRootInput = Read-InstallText -Prompt '  UE root, for example E:\UE_5.6 or E:\UE_5.6\Engine' -DefaultValue $EngineRoot
    if ($EngineRootInput) {
      $script:EngineRoot = $EngineRootInput
    }
  }

  $script:SkipDefaultPreferences = -not (Read-InstallYesNo -Prompt 'Create default Claude/Codex user preference files when missing' -DefaultYes:(-not $SkipDefaultPreferences))
  $script:RunDiagnostics = Read-InstallYesNo -Prompt 'Run BlueprintHelper diagnostics after install' -DefaultYes:$RunDiagnostics

  $script:InstallUePluginToEngine = Read-InstallYesNo -Prompt 'Copy the UE plugin into the engine Plugins/Marketplace folder' -DefaultYes:$InstallUePluginToEngine
  if ($script:InstallUePluginToEngine) {
    if (-not $script:EnginePluginDir) {
      $EnginePluginDirInput = Read-InstallText -Prompt '  Engine plugin target directory, blank to derive from UE root' -DefaultValue $EnginePluginDir
      if ($EnginePluginDirInput) {
        $script:EnginePluginDir = $EnginePluginDirInput
      }
    }
    if (-not $script:EngineRoot -and -not $script:EnginePluginDir) {
      $EngineRootInput = Read-InstallText -Prompt '  UE root required for engine plugin install' -DefaultValue $EngineRoot
      if ($EngineRootInput) {
        $script:EngineRoot = $EngineRootInput
      }
    }
    if (-not $script:EngineRoot -and -not $script:EnginePluginDir) {
      Write-Warning 'UE plugin engine install skipped: no EnginePluginDir or UE root was provided.'
      $script:InstallUePluginToEngine = $false
    }
  }

  $script:Force = Read-InstallYesNo -Prompt 'Allow replacing existing local links or engine plugin target when needed' -DefaultYes:$Force
  Write-Host ''
}

function Ensure-CodexHomeMarketplace {
  param(
    [Parameter(Mandatory = $true)]
    [string]$HomeDir
  )

  $HomePluginsDir = Join-Path $HomeDir 'plugins'
  $LinkPath = Join-Path $HomePluginsDir 'blueprint-helper'
  $MarketplaceDir = Join-Path $HomeDir '.agents\plugins'
  $MarketplacePath = Join-Path $MarketplaceDir 'marketplace.json'

  if ($script:ThisCmdlet.ShouldProcess($HomePluginsDir, 'Create local plugin directory')) {
    New-Item -ItemType Directory -Force -Path $HomePluginsDir | Out-Null
  }

  if (Test-Path -LiteralPath $LinkPath) {
    $Existing = Get-Item -LiteralPath $LinkPath -Force
    $ExistingTarget = @($Existing.Target)[0]
    $ResolvedExistingTarget = if ($ExistingTarget) { [System.IO.Path]::GetFullPath($ExistingTarget) } else { $null }
    $ResolvedTarget = [System.IO.Path]::GetFullPath($CodexPluginRoot)

    if ($ResolvedExistingTarget -and ($ResolvedExistingTarget -ieq $ResolvedTarget)) {
      Write-Host "==> Codex home plugin link already points to $CodexPluginRoot"
    } elseif ($Force) {
      if ($script:ThisCmdlet.ShouldProcess($LinkPath, 'Replace existing Codex home plugin entry')) {
        Remove-Item -LiteralPath $LinkPath -Recurse -Force
        New-Item -ItemType Junction -Path $LinkPath -Target $CodexPluginRoot | Out-Null
      }
    } else {
      throw "Codex home plugin path already exists and points elsewhere: $LinkPath. Re-run with -Force to replace it."
    }
  } elseif ($script:ThisCmdlet.ShouldProcess($LinkPath, "Create junction to $CodexPluginRoot")) {
    New-Item -ItemType Junction -Path $LinkPath -Target $CodexPluginRoot | Out-Null
  }

  if ($script:ThisCmdlet.ShouldProcess($MarketplaceDir, 'Create Codex marketplace directory')) {
    New-Item -ItemType Directory -Force -Path $MarketplaceDir | Out-Null
  }

  if (Test-Path -LiteralPath $MarketplacePath) {
    $Marketplace = Get-Content -Raw -LiteralPath $MarketplacePath | ConvertFrom-Json
  } else {
    $Marketplace = [pscustomobject]@{
      name = 'local'
      interface = [pscustomobject]@{
        displayName = 'Local Plugins'
      }
      plugins = @()
    }
  }

  $Entry = [pscustomobject]@{
    name = 'blueprint-helper'
    source = [pscustomobject]@{
      source = 'local'
      path = './plugins/blueprint-helper'
    }
    policy = [pscustomobject]@{
      installation = 'AVAILABLE'
      authentication = 'ON_INSTALL'
    }
    category = 'Coding'
  }

  $Plugins = @($Marketplace.plugins | Where-Object { $_.name -ne 'blueprint-helper' })
  $Plugins += $Entry
  $Marketplace.plugins = $Plugins

  if ($script:ThisCmdlet.ShouldProcess($MarketplacePath, 'Update Codex home marketplace')) {
    $Marketplace | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $MarketplacePath -Encoding utf8
  }
}

function Get-ClaudePluginInstallInfo {
  $ManifestPath = Join-Path $ClaudePluginRoot '.claude-plugin\plugin.json'
  $MarketplacePath = Join-Path $ClaudePluginRoot '.claude-plugin\marketplace.json'

  Assert-File -Path $ManifestPath -Name 'Claude plugin manifest'
  Assert-File -Path $MarketplacePath -Name 'Claude plugin marketplace'

  $SourcePath = './ClaudePlugin'
  $MarketplaceCommand = "/plugin marketplace add $SourcePath"
  $InstallCommand = '/plugin install blueprint-helper@blueprint-helper-dev'

  Write-Host "==> Claude plugin source: $SourcePath"
  Write-Host '    Start Claude Code from this repository root, then run:'
  Write-Host "    $MarketplaceCommand"
  Write-Host "    $InstallCommand"

  return [pscustomobject]@{
    status = 'ready_for_claude_code'
    source_path = $SourcePath
    marketplace_command = $MarketplaceCommand
    install_command = $InstallCommand
  }
}

function Get-JsonProperty {
  param(
    [Parameter(Mandatory = $true)]
    [object]$Object,
    [Parameter(Mandatory = $true)]
    [string]$Name
  )

  $Property = $Object.PSObject.Properties[$Name]
  if ($Property) {
    return $Property.Value
  }
  return $null
}

function Set-JsonProperty {
  param(
    [Parameter(Mandatory = $true)]
    [object]$Object,
    [Parameter(Mandatory = $true)]
    [string]$Name,
    [Parameter(Mandatory = $true)]
    [AllowNull()]
    [object]$Value
  )

  $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value -Force
}

function Ensure-JsonObjectProperty {
  param(
    [Parameter(Mandatory = $true)]
    [object]$Object,
    [Parameter(Mandatory = $true)]
    [string]$Name
  )

  $Value = Get-JsonProperty -Object $Object -Name $Name
  if ($null -eq $Value) {
    $Value = [pscustomobject]@{}
    Set-JsonProperty -Object $Object -Name $Name -Value $Value
  }
  return $Value
}

function Resolve-UeRootForProfile {
  param([string]$RawEngineRoot)

  if (-not $RawEngineRoot) {
    return $null
  }

  $Resolved = [System.IO.Path]::GetFullPath($RawEngineRoot)
  $Name = Split-Path -Leaf $Resolved
  if ($Name -ieq 'Engine') {
    return Split-Path -Parent $Resolved
  }
  return $Resolved
}

function Resolve-UeEngineDirectory {
  param([string]$RawEngineRoot)

  if (-not $RawEngineRoot) {
    return $null
  }

  $Resolved = [System.IO.Path]::GetFullPath($RawEngineRoot)
  $Name = Split-Path -Leaf $Resolved
  if ($Name -ieq 'Engine') {
    return $Resolved
  }

  return Join-Path $Resolved 'Engine'
}

function Resolve-ProjectFile {
  if ($ProjectFile) {
    $ResolvedProjectFile = [System.IO.Path]::GetFullPath($ProjectFile)
    if (-not (Test-Path -LiteralPath $ResolvedProjectFile -PathType Leaf)) {
      throw "Project file does not exist: $ResolvedProjectFile"
    }
    if ([System.IO.Path]::GetExtension($ResolvedProjectFile) -ine '.uproject') {
      throw "Project file must end with .uproject: $ResolvedProjectFile"
    }
    return $ResolvedProjectFile
  }

  $Dir = $Root
  while ($Dir) {
    $Matches = @(Get-ChildItem -LiteralPath $Dir -Filter '*.uproject' -File -ErrorAction SilentlyContinue)
    if ($Matches.Count -eq 1) {
      return $Matches[0].FullName
    }
    if ($Matches.Count -gt 1) {
      Write-Warning "Multiple .uproject files found in $Dir. Re-run with -ProjectFile."
      return $null
    }

    $Parent = Split-Path -Parent $Dir
    if (-not $Parent -or ($Parent -eq $Dir)) {
      break
    }
    $Dir = $Parent
  }

  return $null
}

function Get-ExistingProfileEngineRoot {
  param([string]$ProfilePath)

  if (-not (Test-Path -LiteralPath $ProfilePath -PathType Leaf)) {
    return $null
  }

  try {
    $Profile = Get-Content -Raw -LiteralPath $ProfilePath | ConvertFrom-Json
    $Environment = Get-JsonProperty -Object $Profile -Name 'environment'
    if ($Environment) {
      $Existing = Get-JsonProperty -Object $Environment -Name 'ue_engine_dir'
      if ($Existing) {
        return [string]$Existing
      }
    }
  } catch {
    throw "Unable to parse existing project profile: $ProfilePath. $($_.Exception.Message)"
  }

  return $null
}

function Get-UeVersionFromEngineRoot {
  param([string]$UeRoot)

  $Name = Split-Path -Leaf $UeRoot
  if ($Name -match '(\d+\.\d+)') {
    return $Matches[1]
  }
  return ''
}

function Ensure-ProjectAgentProfile {
  $ResolvedProjectFile = Resolve-ProjectFile
  if (-not $ResolvedProjectFile) {
    Write-Host '==> Project profile: skipped (no unique .uproject found; pass -ProjectFile and -EngineRoot to create one)'
    return [pscustomobject]@{
      status = 'skipped'
      path = $null
      project_file = $null
      engine_root = $null
    }
  }

  $ProjectDir = Split-Path -Parent $ResolvedProjectFile
  $ProfileDir = Join-Path $ProjectDir '.blueprinthelper'
  $ProfilePath = Join-Path $ProfileDir 'agent-profile.json'
  $ResolvedEngineRoot = Resolve-UeRootForProfile -RawEngineRoot $EngineRoot
  if (-not $ResolvedEngineRoot) {
    $ResolvedEngineRoot = Get-ExistingProfileEngineRoot -ProfilePath $ProfilePath
  }
  if (-not $ResolvedEngineRoot) {
    Write-Host "==> Project profile: skipped ($ProfilePath needs environment.ue_engine_dir; pass -EngineRoot)"
    return [pscustomobject]@{
      status = 'skipped_engine_missing'
      path = $ProfilePath
      project_file = $ResolvedProjectFile
      engine_root = $null
    }
  }

  $Profile = if (Test-Path -LiteralPath $ProfilePath -PathType Leaf) {
    Get-Content -Raw -LiteralPath $ProfilePath | ConvertFrom-Json
  } else {
    [pscustomobject]@{}
  }

  if (-not (Get-JsonProperty -Object $Profile -Name 'schema')) {
    Set-JsonProperty -Object $Profile -Name 'schema' -Value 'BlueprintHelper.AgentProfile.v1'
  }

  $Environment = Ensure-JsonObjectProperty -Object $Profile -Name 'environment'
  Set-JsonProperty -Object $Environment -Name 'ue_engine_dir' -Value $ResolvedEngineRoot
  $UeVersion = Get-UeVersionFromEngineRoot -UeRoot $ResolvedEngineRoot
  if ($UeVersion) {
    Set-JsonProperty -Object $Environment -Name 'ue_version' -Value $UeVersion
  }

  $ActiveProfile = Ensure-JsonObjectProperty -Object $Profile -Name 'active_profile'
  Set-JsonProperty -Object $ActiveProfile -Name 'safety_profile' -Value 'Conservative'
  Set-JsonProperty -Object $ActiveProfile -Name 'missing_capability_policy' -Value 'stop_and_report'
  Set-JsonProperty -Object $ActiveProfile -Name 'auto_save_policy' -Value 'never_auto_save'

  $Agent = Ensure-JsonObjectProperty -Object $Profile -Name 'agent'
  Set-JsonProperty -Object $Agent -Name 'agent_entry_mode' -Value 'cli_task_spec_first'
  Set-JsonProperty -Object $Agent -Name 'fallback_when_task_tools_unavailable' -Value 'stop_and_report'

  $EditorLifecycle = Ensure-JsonObjectProperty -Object $Profile -Name 'editor_lifecycle'
  Set-JsonProperty -Object $EditorLifecycle -Name 'entry' -Value 'global_lifecycle_only_mcp'
  Set-JsonProperty -Object $EditorLifecycle -Name 'open_tool' -Value 'mcp__blueprint_helper__blueprint_open_editor'
  Set-JsonProperty -Object $EditorLifecycle -Name 'close_tool' -Value 'mcp__blueprint_helper__blueprint_close_editor'
  Set-JsonProperty -Object $EditorLifecycle -Name 'main_agent_only' -Value $true

  if ($script:ThisCmdlet.ShouldProcess($ProfilePath, 'Write project agent profile')) {
    New-Item -ItemType Directory -Force -Path $ProfileDir | Out-Null
    $Profile | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $ProfilePath -Encoding utf8
  }

  Write-Host "==> Project profile: $ProfilePath"
  return [pscustomobject]@{
    status = 'written'
    path = $ProfilePath
    project_file = $ResolvedProjectFile
    engine_root = $ResolvedEngineRoot
  }
}

function Ensure-DefaultUserPreferences {
  $PreferenceFiles = @(
    (Join-Path $ClaudePluginRoot 'skills\blueprint-helper\references\08_User_Preferences.md'),
    (Join-Path $CodexPluginRoot 'skills\blueprint-helper\references\08_User_Preferences.md')
  )

  $DefaultText = @'
# 08 - BlueprintHelper User Preferences

schema: BlueprintHelper.UserPreferences.v1
generated_by: install.ps1
source: install_default_conservative_profile

## Purpose

This file records durable user-facing Agent preferences for BlueprintHelper work.

## Active Preferences

- Default safety profile: `Conservative`.
- Default transport: `cli_task_spec_first`.
- Preview is the write gate. Do not execute writes when preview is blocked.
- Missing capability default: `stop_and_report`.
- Default save policy: no automatic save.
- Use BlueprintHelper CLI for ordinary UE editor asset reads and writes.
- Use global lifecycle-only MCP only for opening and closing Unreal Editor.
- Use normal repository tools for source, scripts, config, tests, and docs.
- Do not request or store Bridge tokens, auth sessions, raw payloads, or private environment details.

## Manual Notes

- No manual notes recorded yet.
'@

  $Created = @()
  $Kept = @()
  foreach ($File in $PreferenceFiles) {
    if (Test-Path -LiteralPath $File -PathType Leaf) {
      $Kept += $File
      continue
    }

    if ($script:ThisCmdlet.ShouldProcess($File, 'Create default BlueprintHelper user preferences')) {
      New-Item -ItemType Directory -Force -Path (Split-Path -Parent $File) | Out-Null
      Set-Content -LiteralPath $File -Value $DefaultText -Encoding utf8
    }
    $Created += $File
  }

  Write-Host "==> User preferences: kept=$($Kept.Count) created=$($Created.Count)"
  return [pscustomobject]@{
    kept = $Kept
    created = $Created
  }
}

function Invoke-BlueprintHelperDiagnostics {
  param([object]$ProjectProfileResult)

  $CliEntry = Join-Path $AgentFaceServiceRoot 'cli\build\cli\index.js'
  if (-not (Test-Path -LiteralPath $CliEntry -PathType Leaf)) {
    Write-Host '==> Diagnostics: skipped (CLI build output missing)'
    return 'skipped_cli_missing'
  }

  $DiagnosticsCwd = if ($ProjectProfileResult.project_file) {
    Split-Path -Parent $ProjectProfileResult.project_file
  } else {
    $Root
  }

  Write-Host '==> Diagnostics: bh blueprinthelper_diagnostics'
  if ($script:ThisCmdlet.ShouldProcess($DiagnosticsCwd, 'Run BlueprintHelper diagnostics')) {
    Push-Location $DiagnosticsCwd
    try {
      & $script:NodeCommand $CliEntry 'blueprinthelper_diagnostics' '--json' '{}' '--select' 'status,summary'
      $ExitCode = $LASTEXITCODE
    } finally {
      Pop-Location
    }
    if ($ExitCode -eq 0) {
      return 'passed'
    }
    Write-Warning "Diagnostics exited with code $ExitCode."
    return "failed:$ExitCode"
  }

  return 'whatif'
}

function Install-UePluginToEngine {
  if (-not $EnginePluginDir) {
    if (-not $EngineRoot) {
      throw 'Pass -EngineRoot or -EnginePluginDir when using -InstallUePluginToEngine.'
    }
    $EngineDir = Resolve-UeEngineDirectory -RawEngineRoot $EngineRoot
    $EnginePluginDir = Join-Path $EngineDir 'Plugins\Marketplace\BlueprintHelper'
  }

  $ResolvedTarget = [System.IO.Path]::GetFullPath($EnginePluginDir)
  $ResolvedSource = [System.IO.Path]::GetFullPath($UePluginRoot)

  if ($ResolvedSource -ieq $ResolvedTarget) {
    throw 'UE plugin source and target are the same path.'
  }

  if ((Test-Path -LiteralPath $ResolvedTarget) -and -not $Force) {
    throw "Engine plugin target already exists: $ResolvedTarget. Re-run with -Force to overwrite files."
  }

  $TargetParent = Split-Path -Parent $ResolvedTarget
  if ($script:ThisCmdlet.ShouldProcess($TargetParent, 'Create UE engine plugin parent directory')) {
    New-Item -ItemType Directory -Force -Path $TargetParent | Out-Null
  }

  $RoboArgs = @(
    $ResolvedSource,
    $ResolvedTarget,
    '/E',
    '/XD',
    'Binaries',
    'Intermediate',
    'Saved',
    '.vs',
    '/XF',
    '*.sln'
  )

  Write-Host "==> Install UE plugin to engine: $ResolvedTarget"
  if ($script:ThisCmdlet.ShouldProcess($ResolvedTarget, 'Copy UE plugin files')) {
    & robocopy @RoboArgs | Out-Host
    if ($LASTEXITCODE -gt 7) {
      throw "robocopy failed with exit code $LASTEXITCODE."
    }
    $global:LASTEXITCODE = 0
  }
}

Assert-Directory -Path $CodexPluginRoot -Name 'CodexPlugin'
Assert-Directory -Path $ClaudePluginRoot -Name 'ClaudePlugin'
Assert-Directory -Path $AgentFaceServiceRoot -Name 'AgentFaceService'
Assert-Directory -Path $UePluginRoot -Name 'BlueprintHelper UE plugin'
Assert-File -Path (Join-Path $CodexPluginRoot '.codex-plugin\plugin.json') -Name 'Codex plugin manifest'
Assert-File -Path (Join-Path $ClaudePluginRoot '.claude-plugin\plugin.json') -Name 'Claude plugin manifest'
Assert-File -Path (Join-Path $ClaudePluginRoot '.claude-plugin\marketplace.json') -Name 'Claude plugin marketplace'
Assert-File -Path (Join-Path $UePluginRoot 'BlueprintHelper.uplugin') -Name 'UE plugin descriptor'

$script:NodeCommand = Resolve-CommandPath -Names @('node') -DisplayName 'Node.js'
$script:NpmCommand = Resolve-CommandPath -Names @('npm.cmd', 'npm') -DisplayName 'npm'

if ($Interactive) {
  Invoke-InteractiveInstallWizard
}

if (-not $SkipBuild) {
  Invoke-Npm -PackageDir (Join-Path $AgentFaceServiceRoot 'task-core') -Arguments @('install')
  Invoke-Npm -PackageDir (Join-Path $AgentFaceServiceRoot 'task-core') -Arguments @('run', 'build')
  Invoke-Npm -PackageDir (Join-Path $AgentFaceServiceRoot 'cli') -Arguments @('install')
  Invoke-Npm -PackageDir (Join-Path $AgentFaceServiceRoot 'cli') -Arguments @('run', 'build')
  Invoke-Npm -PackageDir (Join-Path $AgentFaceServiceRoot 'mcp') -Arguments @('install')
  Invoke-Npm -PackageDir (Join-Path $AgentFaceServiceRoot 'mcp') -Arguments @('run', 'build')
}

if (-not $SkipCliLink) {
  Invoke-Npm -PackageDir (Join-Path $AgentFaceServiceRoot 'cli') -Arguments @('link')
}

$UserHome = $env:USERPROFILE
if (-not $UserHome) {
  $UserHome = $env:HOME
}
if (-not $UserHome) {
  throw 'Unable to resolve user home directory.'
}

if (-not $SkipCodexMarketplace) {
  Ensure-CodexHomeMarketplace -HomeDir $UserHome
}

if (-not $SkipCodexAgents) {
  Invoke-External -Description 'Install Codex subagents' -FilePath $script:NodeCommand -Arguments @((Join-Path $CodexPluginRoot 'scripts\install-codex-agents.cjs'))
}

if (-not $SkipLifecycleMcp) {
  Invoke-External -Description 'Install lifecycle-only MCP config' -FilePath $script:NodeCommand -Arguments @((Join-Path $CodexPluginRoot 'scripts\install-global-mcp.cjs'))
}

$ProjectProfileResult = [pscustomobject]@{
  status = 'skipped'
  path = $null
  project_file = $null
  engine_root = $null
}

$ClaudePluginResult = [pscustomobject]@{
  status = 'skipped'
  source_path = $null
  marketplace_command = $null
  install_command = $null
}

$ClaudeAgentsStatus = 'skipped'

if (-not $SkipProjectProfile) {
  $ProjectProfileResult = Ensure-ProjectAgentProfile
}

if (-not $SkipDefaultPreferences) {
  Ensure-DefaultUserPreferences | Out-Null
}

$DiagnosticsStatus = 'skipped'
if ($RunDiagnostics) {
  $DiagnosticsStatus = Invoke-BlueprintHelperDiagnostics -ProjectProfileResult $ProjectProfileResult
}

if ($InstallClaudePlugin) {
  $ClaudePluginResult = Get-ClaudePluginInstallInfo
}

if ($InstallClaudeAgents -or $InstallClaudePlugin) {
  Invoke-External -Description 'Install Claude subagents' -FilePath $script:NodeCommand -Arguments @((Join-Path $CodexPluginRoot 'scripts\install-claude-agents.cjs'))
  $ClaudeAgentsStatus = if ($WhatIfPreference) { 'whatif' } else { 'installed' }
}

if ($InstallUePluginToEngine) {
  Install-UePluginToEngine
}

Write-Host ''
Write-Host 'BlueprintHelper install finished.'
Write-Host "Source root: $Root"
Write-Host 'CLI: bh or blueprinthelper-cli'
Write-Host 'Codex plugin: blueprint-helper'
Write-Host "Claude plugin: $($ClaudePluginResult.status)"
if ($ClaudePluginResult.source_path) {
  Write-Host "Claude plugin source: $($ClaudePluginResult.source_path)"
  Write-Host "Claude marketplace command: $($ClaudePluginResult.marketplace_command)"
  Write-Host "Claude install command: $($ClaudePluginResult.install_command)"
}
Write-Host "Claude agents: $ClaudeAgentsStatus"
Write-Host "Project profile: $($ProjectProfileResult.status)"
if ($ProjectProfileResult.path) {
  Write-Host "Project profile path: $($ProjectProfileResult.path)"
}
Write-Host "Diagnostics: $DiagnosticsStatus"
Write-Host 'UE plugin: install per project, or use -InstallUePluginToEngine for an engine plugin copy.'
