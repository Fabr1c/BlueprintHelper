[CmdletBinding(SupportsShouldProcess = $true)]
param(
  [switch]$Interactive,
  [switch]$SkipCliUnlink,
  [switch]$SkipCodexPlugin,
  [switch]$SkipCodexAgents,
  [switch]$SkipLifecycleMcp,
  [switch]$SkipClaudePlugin,
  [switch]$SkipClaudeAgents,
  [switch]$RemoveProjectProfile,
  [string]$ProjectFile,
  [switch]$RemoveUePluginFromEngine,
  [string]$EngineRoot,
  [string]$EnginePluginDir,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'

trap {
  Write-Host ''
  Write-Host 'BlueprintHelper uninstall failed.' -ForegroundColor Red
  if ($_.Exception -and $_.Exception.Message) {
    Write-Host $_.Exception.Message -ForegroundColor Red
  } else {
    Write-Host $_ -ForegroundColor Red
  }
  exit 1
}

$ScriptRoot = $PSScriptRoot
$Root = Split-Path -Parent $ScriptRoot
$CodexPluginRoot = Join-Path $Root 'CodexPlugin'
$ClaudePluginRoot = Join-Path $Root 'ClaudePlugin'

function Read-UninstallYesNo {
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

function Read-UninstallText {
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

function Normalize-PathInput {
  param([string]$PathText)

  if ([string]::IsNullOrWhiteSpace($PathText)) {
    return $null
  }

  $CleanPath = $PathText.Trim()
  while (
    $CleanPath.Length -ge 2 -and
    (
      ($CleanPath.StartsWith('"') -and $CleanPath.EndsWith('"')) -or
      ($CleanPath.StartsWith("'") -and $CleanPath.EndsWith("'"))
    )
  ) {
    $CleanPath = $CleanPath.Substring(1, $CleanPath.Length - 2).Trim()
  }

  if ([string]::IsNullOrWhiteSpace($CleanPath)) {
    return $null
  }

  return $CleanPath
}

function Get-UserHome {
  $HomeDir = $env:USERPROFILE
  if (-not $HomeDir) {
    $HomeDir = $env:HOME
  }
  if (-not $HomeDir) {
    throw 'Unable to resolve user home directory.'
  }
  return $HomeDir
}

function Resolve-NpmCommandPath {
  $Commands = @(
    Get-Command 'npm.cmd' -ErrorAction SilentlyContinue
    Get-Command 'npm.exe' -ErrorAction SilentlyContinue
    Get-Command 'npm' -All -ErrorAction SilentlyContinue
  )

  $Executable = $Commands |
    Where-Object { $_ -and $_.CommandType -eq 'Application' -and $_.Source -and $_.Source -notmatch '\.ps1$' } |
    Select-Object -First 1
  if ($Executable) {
    return $Executable.Source
  }

  return $null
}

function Invoke-ExternalSoft {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Description,
    [Parameter(Mandatory = $true)]
    [string]$FilePath,
    [Parameter(Mandatory = $true)]
    [string[]]$Arguments
  )

  Write-Host "==> $Description"
  if (-not $PSCmdlet.ShouldProcess($Description, "$FilePath $($Arguments -join ' ')")) {
    return $true
  }

  try {
    & $FilePath @Arguments
    if ($LASTEXITCODE -eq 0) {
      return $true
    }
    Write-Warning "$Description failed with exit code $LASTEXITCODE."
    return $false
  } catch {
    Write-Warning "$Description failed to start. $($_.Exception.Message)"
    return $false
  }
}

function Resolve-NpmGlobalBinDirectory {
  $NpmCommand = Resolve-NpmCommandPath
  if (-not $NpmCommand) {
    return $null
  }

  $PrefixOutput = & $NpmCommand 'config' 'get' 'prefix' 2>$null
  if ($LASTEXITCODE -ne 0) {
    return $null
  }

  $Prefix = ($PrefixOutput | Select-Object -First 1)
  if ([string]::IsNullOrWhiteSpace($Prefix)) {
    return $null
  }

  $ResolvedPrefix = [System.IO.Path]::GetFullPath($Prefix.Trim())
  if ($env:OS -eq 'Windows_NT') {
    return $ResolvedPrefix
  }
  return Join-Path $ResolvedPrefix 'bin'
}

function Test-FileLooksBlueprintHelperOwned {
  param([Parameter(Mandatory = $true)][string]$Path)

  if ($Force) {
    return $true
  }
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    return $false
  }

  try {
    $Content = Get-Content -Raw -LiteralPath $Path -ErrorAction Stop
    return ($Content -match 'BlueprintHelper|blueprint-helper|blueprinthelper')
  } catch {
    return $false
  }
}

function Remove-FileIfPresent {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path,
    [string]$Description = 'Remove file',
    [switch]$RequireBlueprintHelperOwnership
  )

  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    return $false
  }

  if ($RequireBlueprintHelperOwnership -and -not (Test-FileLooksBlueprintHelperOwned -Path $Path)) {
    Write-Warning "Skipped $Path because it does not look BlueprintHelper-owned. Pass -Force to remove it."
    return $false
  }

  if ($PSCmdlet.ShouldProcess($Path, $Description)) {
    Remove-Item -LiteralPath $Path -Force
    Write-Host "Removed: $Path"
  }
  return $true
}

function Remove-EmptyDirectoryIfPresent {
  param([Parameter(Mandatory = $true)][string]$Path)

  if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
    return
  }

  $Children = @(Get-ChildItem -LiteralPath $Path -Force)
  if ($Children.Count -eq 0 -and $PSCmdlet.ShouldProcess($Path, 'Remove empty directory')) {
    Remove-Item -LiteralPath $Path -Force
  }
}

function Uninstall-BlueprintHelperCliLink {
  $NpmCommand = Resolve-NpmCommandPath
  if ($NpmCommand) {
    Invoke-ExternalSoft -Description 'npm unlink global blueprint-helper-cli' -FilePath $NpmCommand -Arguments @('unlink', '-g', 'blueprint-helper-cli') | Out-Null
  } else {
    Write-Warning 'npm was not found on PATH; removing known CLI shims only.'
  }

  $GlobalBinDir = Resolve-NpmGlobalBinDirectory
  if (-not $GlobalBinDir) {
    return
  }

  foreach ($Name in @('bh', 'blueprinthelper-cli')) {
    foreach ($Suffix in @('', '.cmd', '.ps1')) {
      $Path = Join-Path $GlobalBinDir "$Name$Suffix"
      Remove-FileIfPresent -Path $Path -Description 'Remove BlueprintHelper CLI shim' -RequireBlueprintHelperOwnership | Out-Null
    }
  }
}

function Get-CodexPluginInstallInfo {
  $MarketplacePath = Join-Path $Root '.agents\plugins\marketplace.json'
  $ManifestPath = Join-Path $CodexPluginRoot '.codex-plugin\plugin.json'
  if (-not (Test-Path -LiteralPath $MarketplacePath -PathType Leaf) -or -not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    return $null
  }

  $Marketplace = Get-Content -Raw -LiteralPath $MarketplacePath | ConvertFrom-Json
  $Manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
  return [pscustomobject]@{
    marketplace_source = $Root
    marketplace_name = [string]$Marketplace.name
    plugin_name = [string]$Manifest.name
    install_spec = "$($Manifest.name)@$($Marketplace.name)"
  }
}

function Get-ClaudePluginInstallInfo {
  $MarketplacePath = Join-Path $ClaudePluginRoot '.claude-plugin\marketplace.json'
  $ManifestPath = Join-Path $ClaudePluginRoot '.claude-plugin\plugin.json'
  if (-not (Test-Path -LiteralPath $MarketplacePath -PathType Leaf) -or -not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    return $null
  }

  $Marketplace = Get-Content -Raw -LiteralPath $MarketplacePath | ConvertFrom-Json
  $Manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
  return [pscustomobject]@{
    marketplace_source = $ClaudePluginRoot
    marketplace_name = [string]$Marketplace.name
    plugin_name = [string]$Manifest.name
    install_spec = "$($Manifest.name)@$($Marketplace.name)"
  }
}

function Get-OfficialPluginCommands {
  param([Parameter(Mandatory = $true)][string[]]$Names)

  $Commands = @()
  foreach ($Name in $Names) {
    $Command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($Command -and $Command.Source) {
      $Commands += [pscustomobject]@{
        name = $Name
        path = $Command.Source
      }
    }
  }
  return $Commands
}

function Uninstall-PluginViaOfficialEntry {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Surface,
    [Parameter(Mandatory = $true)]
    [object]$Info,
    [AllowNull()]
    [AllowEmptyCollection()]
    [object[]]$Commands = @()
  )

  if (-not $Info) {
    Write-Warning "$Surface plugin manifest or marketplace was not found; skipping official uninstall."
    return
  }
  if ($Commands.Count -eq 0) {
    Write-Warning "No callable $Surface official plugin CLI was found; remove '$($Info.install_spec)' from the $Surface Plugins UI if it is installed."
    return
  }

  foreach ($Command in $Commands) {
    $RemovedPlugin = Invoke-ExternalSoft -Description "$Surface official plugin uninstall ($($Command.name))" -FilePath $Command.path -Arguments @('plugin', 'uninstall', $Info.install_spec)
    $RemovedMarketplace = Invoke-ExternalSoft -Description "$Surface official marketplace remove ($($Command.name))" -FilePath $Command.path -Arguments @('plugin', 'marketplace', 'remove', $Info.marketplace_source)
    if ($RemovedPlugin -or $RemovedMarketplace) {
      return
    }
  }
}

function Remove-CodexAgents {
  $HomeDir = Get-UserHome
  $AgentDir = Join-Path $HomeDir '.codex\agents'
  foreach ($File in @('blueprint-explorer.toml', 'sourcecode-explorer.toml', 'task-worker.toml')) {
    Remove-FileIfPresent -Path (Join-Path $AgentDir $File) -Description 'Remove Codex subagent' -RequireBlueprintHelperOwnership | Out-Null
  }
  Remove-EmptyDirectoryIfPresent -Path $AgentDir
}

function Remove-ClaudeAgents {
  $HomeDir = Get-UserHome
  $AgentDir = Join-Path $HomeDir '.claude\agents'
  foreach ($File in @('blueprint-explorer.md', 'sourcecode-explorer.md', 'task-worker.md')) {
    Remove-FileIfPresent -Path (Join-Path $AgentDir $File) -Description 'Remove Claude sideAgent' -RequireBlueprintHelperOwnership | Out-Null
  }
  Remove-EmptyDirectoryIfPresent -Path $AgentDir
}

function Remove-CodexLifecycleMcpConfig {
  $HomeDir = Get-UserHome
  $ConfigPath = Join-Path $HomeDir '.codex\config.toml'
  if (-not (Test-Path -LiteralPath $ConfigPath -PathType Leaf)) {
    return
  }

  $Current = Get-Content -Raw -LiteralPath $ConfigPath
  $Lines = $Current -split "\r?\n"
  $Output = @()
  $Skipping = $false
  $Removed = $false

  foreach ($Line in $Lines) {
    $IsBlueprintHelperSection = $Line -match '^\[mcp_servers\.(?:"blueprint-helper"|blueprint-helper)\]\s*$'
    $IsNextSection = $Line -match '^\[' -and -not $IsBlueprintHelperSection

    if ($IsBlueprintHelperSection) {
      $Skipping = $true
      $Removed = $true
      continue
    }
    if ($Skipping -and $IsNextSection) {
      $Skipping = $false
    }
    if (-not $Skipping) {
      $Output += $Line
    }
  }

  if (-not $Removed) {
    return
  }

  $Next = (($Output -join "`n") -replace "\s+$", '') + "`n"
  if ($Current -ne $Next) {
    if ($PSCmdlet.ShouldProcess($ConfigPath, 'Remove BlueprintHelper lifecycle MCP config')) {
      Set-Content -LiteralPath $ConfigPath -Value $Next -Encoding UTF8
      Write-Host "Updated: $ConfigPath"
    }
  }
}

function Resolve-ProjectProfilePath {
  if ($ProjectFile) {
    $CleanProjectFile = Normalize-PathInput -PathText $ProjectFile
    if ($CleanProjectFile) {
      $ResolvedProjectFile = [System.IO.Path]::GetFullPath($CleanProjectFile)
      return Join-Path (Split-Path -Parent $ResolvedProjectFile) '.blueprinthelper\agent-profile.json'
    }
  }

  $Candidates = @(Get-ChildItem -LiteralPath $Root -Filter '*.uproject' -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 2)
  if ($Candidates.Count -eq 1) {
    return Join-Path $Candidates[0].DirectoryName '.blueprinthelper\agent-profile.json'
  }

  return $null
}

function Remove-ProjectAgentProfile {
  $ProfilePath = Resolve-ProjectProfilePath
  if (-not $ProfilePath) {
    Write-Warning 'Project profile path was not provided and could not be auto-detected; skipping project profile removal.'
    return
  }

  Remove-FileIfPresent -Path $ProfilePath -Description 'Remove project BlueprintHelper agent profile' | Out-Null
  Remove-EmptyDirectoryIfPresent -Path (Split-Path -Parent $ProfilePath)
}

function Resolve-EnginePluginTarget {
  if ($EnginePluginDir) {
    return [System.IO.Path]::GetFullPath((Normalize-PathInput -PathText $EnginePluginDir))
  }

  if (-not $EngineRoot) {
    return $null
  }

  $CleanEngineRoot = Normalize-PathInput -PathText $EngineRoot
  if (-not $CleanEngineRoot) {
    return $null
  }

  $ResolvedRoot = [System.IO.Path]::GetFullPath($CleanEngineRoot)
  $EngineDir = if ((Split-Path -Leaf $ResolvedRoot) -ieq 'Engine') {
    $ResolvedRoot
  } else {
    Join-Path $ResolvedRoot 'Engine'
  }

  return Join-Path $EngineDir 'Plugins\Marketplace\BlueprintHelper'
}

function Remove-UeEnginePluginCopy {
  $Target = Resolve-EnginePluginTarget
  if (-not $Target) {
    Write-Warning 'Pass -EngineRoot or -EnginePluginDir when removing an Engine plugin copy.'
    return
  }

  $ResolvedTarget = [System.IO.Path]::GetFullPath($Target)
  $ResolvedSource = [System.IO.Path]::GetFullPath((Join-Path $Root 'BlueprintHelper'))
  if ($ResolvedTarget.TrimEnd('\') -ieq $ResolvedSource.TrimEnd('\')) {
    throw "Refusing to remove the source UE plugin directory: $ResolvedTarget"
  }
  if ((Split-Path -Leaf $ResolvedTarget) -ine 'BlueprintHelper') {
    throw "Engine plugin target must end with BlueprintHelper: $ResolvedTarget"
  }
  if (-not (Test-Path -LiteralPath $ResolvedTarget -PathType Container)) {
    return
  }

  $Descriptor = Join-Path $ResolvedTarget 'BlueprintHelper.uplugin'
  if (-not $Force -and -not (Test-Path -LiteralPath $Descriptor -PathType Leaf)) {
    throw "Refusing to remove Engine plugin target without BlueprintHelper.uplugin. Pass -Force to override: $ResolvedTarget"
  }

  if ($PSCmdlet.ShouldProcess($ResolvedTarget, 'Remove Engine BlueprintHelper plugin copy')) {
    Remove-Item -LiteralPath $ResolvedTarget -Recurse -Force
    Write-Host "Removed: $ResolvedTarget"
  }
}

function Invoke-InteractiveUninstallWizard {
  Write-Host 'BlueprintHelper interactive uninstall'
  Write-Host "Source root: $Root"
  Write-Host ''

  $script:SkipCliUnlink = -not (Read-UninstallYesNo -Prompt 'Unlink the global bh CLI' -DefaultYes:(-not $SkipCliUnlink))
  $script:SkipCodexPlugin = -not (Read-UninstallYesNo -Prompt 'Uninstall Codex plugin through official entry when available' -DefaultYes:(-not $SkipCodexPlugin))
  $script:SkipCodexAgents = -not (Read-UninstallYesNo -Prompt 'Remove Codex subagents' -DefaultYes:(-not $SkipCodexAgents))
  $script:SkipLifecycleMcp = -not (Read-UninstallYesNo -Prompt 'Remove Codex lifecycle MCP config' -DefaultYes:(-not $SkipLifecycleMcp))
  $script:SkipClaudePlugin = -not (Read-UninstallYesNo -Prompt 'Uninstall Claude Code plugin through official entry when available' -DefaultYes:(-not $SkipClaudePlugin))
  $script:SkipClaudeAgents = -not (Read-UninstallYesNo -Prompt 'Remove Claude sideAgents' -DefaultYes:(-not $SkipClaudeAgents))
  $script:RemoveProjectProfile = Read-UninstallYesNo -Prompt 'Remove project .blueprinthelper/agent-profile.json' -DefaultYes:$RemoveProjectProfile
  if ($script:RemoveProjectProfile) {
    $script:ProjectFile = Normalize-PathInput -PathText (Read-UninstallText -Prompt '  Project .uproject path, blank to auto-detect' -DefaultValue $ProjectFile)
  }
  $script:RemoveUePluginFromEngine = Read-UninstallYesNo -Prompt 'Remove UE plugin copy from Engine Plugins/Marketplace' -DefaultYes:$RemoveUePluginFromEngine
  if ($script:RemoveUePluginFromEngine) {
    $script:EnginePluginDir = Normalize-PathInput -PathText (Read-UninstallText -Prompt '  Engine plugin target directory, blank to derive from UE root' -DefaultValue $EnginePluginDir)
    if (-not $script:EnginePluginDir) {
      $script:EngineRoot = Normalize-PathInput -PathText (Read-UninstallText -Prompt '  UE root, for example E:\UE_5.6 or E:\UE_5.6\Engine' -DefaultValue $EngineRoot)
    }
  }
  $script:Force = Read-UninstallYesNo -Prompt 'Allow removing ambiguous BlueprintHelper-named files when needed' -DefaultYes:$Force
}

if ($Interactive) {
  Invoke-InteractiveUninstallWizard
}

if (-not $SkipCliUnlink) {
  Uninstall-BlueprintHelperCliLink
}
if (-not $SkipCodexPlugin) {
  Uninstall-PluginViaOfficialEntry -Surface 'Codex' -Info (Get-CodexPluginInstallInfo) -Commands (Get-OfficialPluginCommands -Names @('droid', 'codex'))
}
if (-not $SkipCodexAgents) {
  Remove-CodexAgents
}
if (-not $SkipLifecycleMcp) {
  Remove-CodexLifecycleMcpConfig
}
if (-not $SkipClaudePlugin) {
  Uninstall-PluginViaOfficialEntry -Surface 'Claude' -Info (Get-ClaudePluginInstallInfo) -Commands (Get-OfficialPluginCommands -Names @('claude', 'claude-code'))
}
if (-not $SkipClaudeAgents) {
  Remove-ClaudeAgents
}
if ($RemoveProjectProfile) {
  Remove-ProjectAgentProfile
}
if ($RemoveUePluginFromEngine) {
  Remove-UeEnginePluginCopy
}

Write-Host ''
Write-Host 'BlueprintHelper uninstall finished.'
Write-Host 'The source checkout was not removed.'
