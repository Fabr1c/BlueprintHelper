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
$ScriptRoot = $PSScriptRoot
$Root = Split-Path -Parent $ScriptRoot
$script:UninstallFailureCode = 'BH-UNINSTALL-UNHANDLED'
$script:UninstallFailureStage = 'startup'

function Set-UninstallFailureContext {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Code,
    [Parameter(Mandatory = $true)]
    [string]$Stage
  )

  $script:UninstallFailureCode = $Code
  $script:UninstallFailureStage = $Stage
}

trap {
  Write-Host ''
  Write-Host 'BlueprintHelper uninstall failed.' -ForegroundColor Red
  Write-Host "Failure code: $script:UninstallFailureCode" -ForegroundColor Red
  Write-Host "Failure stage: $script:UninstallFailureStage" -ForegroundColor Red
  if ($_.Exception -and $_.Exception.Message) {
    Write-Host $_.Exception.Message -ForegroundColor Red
  } else {
    Write-Host $_ -ForegroundColor Red
  }
  Write-Host "Failure docs: $(Join-Path $Root 'INSTALL_FAILURE_CODES.md')" -ForegroundColor Yellow
  exit 1
}

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

function Get-WindowsUsersRoot {
  $SystemDrive = $env:SystemDrive
  if ([string]::IsNullOrWhiteSpace($SystemDrive)) {
    $SystemDrive = 'C:'
  }
  return Join-Path $SystemDrive.TrimEnd('\') 'Users'
}

function Add-UserHomeCandidate {
  param(
    [System.Collections.ArrayList]$Candidates,
    [AllowNull()]
    [string]$PathText
  )

  if ([string]::IsNullOrWhiteSpace($PathText)) {
    return
  }

  [void]$Candidates.Add($PathText.Trim())
}

function Test-PathUnderDirectory {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path,
    [Parameter(Mandatory = $true)]
    [string]$Directory
  )

  $ResolvedPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\') + '\'
  $ResolvedDirectory = [System.IO.Path]::GetFullPath($Directory).TrimEnd('\') + '\'
  return $ResolvedPath.StartsWith($ResolvedDirectory, [System.StringComparison]::OrdinalIgnoreCase)
}

function Resolve-BlueprintHelperUserHome {
  $Candidates = New-Object System.Collections.ArrayList
  Add-UserHomeCandidate -Candidates $Candidates -PathText $env:USERPROFILE
  Add-UserHomeCandidate -Candidates $Candidates -PathText $env:HOME
  Add-UserHomeCandidate -Candidates $Candidates -PathText ([Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile))

  $UsersRoot = Get-WindowsUsersRoot
  if (Test-Path -LiteralPath $UsersRoot -PathType Container) {
    if (-not [string]::IsNullOrWhiteSpace($env:USERNAME)) {
      Add-UserHomeCandidate -Candidates $Candidates -PathText (Join-Path $UsersRoot $env:USERNAME)
    }
  }

  $Seen = @{}
  foreach ($Candidate in $Candidates) {
    if ([string]::IsNullOrWhiteSpace($Candidate)) {
      continue
    }

    $Resolved = [System.IO.Path]::GetFullPath($Candidate)
    $Key = $Resolved.ToLowerInvariant()
    if ($Seen.ContainsKey($Key)) {
      continue
    }
    $Seen[$Key] = $true

    if (-not (Test-Path -LiteralPath $Resolved -PathType Container)) {
      continue
    }

    if ((Test-Path -LiteralPath $UsersRoot -PathType Container) -and -not (Test-PathUnderDirectory -Path $Resolved -Directory $UsersRoot)) {
      continue
    }

    if ((Test-Path -LiteralPath $UsersRoot -PathType Container) -and -not [string]::IsNullOrWhiteSpace($env:USERNAME)) {
      $Leaf = Split-Path -Leaf $Resolved
      if ($Leaf -ine $env:USERNAME) {
        continue
      }
    }

    return $Resolved
  }

  throw "Unable to resolve Windows user home directory under $UsersRoot for BlueprintHelper Codex/Claude config."
}

function Get-UserHome {
  return Resolve-BlueprintHelperUserHome
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

function Resolve-NodeCommandPath {
  $Commands = @(
    Get-Command 'node.exe' -ErrorAction SilentlyContinue
    Get-Command 'node' -All -ErrorAction SilentlyContinue
  )

  $Executable = $Commands |
    Where-Object { $_ -and $_.CommandType -eq 'Application' -and $_.Source } |
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
    Set-UninstallFailureContext -Code 'BH-UNINSTALL-EXTERNAL-COMMAND-FAILED' -Stage $Description
    Write-Warning "$Description failed with exit code $LASTEXITCODE."
    return $false
  } catch {
    Set-UninstallFailureContext -Code 'BH-UNINSTALL-EXTERNAL-COMMAND-FAILED' -Stage $Description
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

function Remove-TomlTableSections {
  param(
    [AllowNull()]
    [string]$Text,
    [Parameter(Mandatory = $true)]
    [string[]]$Headers
  )

  $HeaderSet = @{}
  foreach ($Header in $Headers) {
    $HeaderSet[$Header.Trim()] = $true
  }

  $Lines = if ($null -eq $Text) { @() } else { $Text -split "\r?\n" }
  $Output = @()
  $Skipping = $false
  $Removed = $false

  foreach ($Line in $Lines) {
    $Trimmed = $Line.Trim()
    $IsTableHeader = $Trimmed -match '^\[[^\]]+\]\s*$'

    if ($IsTableHeader) {
      if ($HeaderSet.ContainsKey($Trimmed)) {
        $Skipping = $true
        $Removed = $true
        continue
      }
      $Skipping = $false
    }

    if (-not $Skipping) {
      $Output += $Line
    }
  }

  return [pscustomobject]@{
    text = ($Output -join "`n")
    removed = $Removed
  }
}

function Remove-TomlOwnedSectionsFromFile {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path,
    [Parameter(Mandatory = $true)]
    [string[]]$Headers,
    [Parameter(Mandatory = $true)]
    [string]$Description
  )

  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    return $false
  }

  $Current = Get-Content -Raw -LiteralPath $Path
  $Result = Remove-TomlTableSections -Text $Current -Headers $Headers
  if (-not $Result.removed) {
    return $false
  }

  $Next = (($Result.text -replace "\s+$", '') + "`n")
  if ($PSCmdlet.ShouldProcess($Path, $Description)) {
    Set-Content -LiteralPath $Path -Value $Next -Encoding UTF8
    Write-Host "Updated: $Path"
  }
  return $true
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

function Remove-JsonProperty {
  param(
    [Parameter(Mandatory = $true)]
    [object]$Object,
    [Parameter(Mandatory = $true)]
    [string]$Name
  )

  $Property = $Object.PSObject.Properties[$Name]
  if (-not $Property) {
    return $false
  }

  $Object.PSObject.Properties.Remove($Name)
  return $true
}

function Remove-CodexPluginConfig {
  $Info = Get-CodexPluginInstallInfo
  if (-not $Info) {
    Write-Warning 'Codex plugin manifest or marketplace was not found; skipping Codex config cleanup.'
    return
  }

  $ConfigPath = Join-Path (Get-UserHome) '.codex\config.toml'
  $Headers = @(
    "[marketplaces.$($Info.marketplace_name)]",
    "[plugins.`"$($Info.install_spec)`"]"
  )

  Remove-TomlOwnedSectionsFromFile -Path $ConfigPath -Headers $Headers -Description 'Remove BlueprintHelper Codex plugin config' | Out-Null
}

function Remove-ClaudePluginConfig {
  $Info = Get-ClaudePluginInstallInfo
  if (-not $Info) {
    Write-Warning 'Claude plugin manifest or marketplace was not found; skipping Claude settings cleanup.'
    return
  }

  $SettingsPath = Join-Path (Get-UserHome) '.claude\settings.json'
  if (-not (Test-Path -LiteralPath $SettingsPath -PathType Leaf)) {
    return
  }

  $RawSettings = Get-Content -Raw -LiteralPath $SettingsPath
  if ([string]::IsNullOrWhiteSpace($RawSettings)) {
    return
  }

  $Settings = $RawSettings | ConvertFrom-Json
  $Changed = $false

  $EnabledPlugins = Get-JsonProperty -Object $Settings -Name 'enabledPlugins'
  if ($EnabledPlugins -and $EnabledPlugins -is [pscustomobject]) {
    $Changed = (Remove-JsonProperty -Object $EnabledPlugins -Name $Info.install_spec) -or $Changed
  }

  $ExtraKnownMarketplaces = Get-JsonProperty -Object $Settings -Name 'extraKnownMarketplaces'
  if ($ExtraKnownMarketplaces -and $ExtraKnownMarketplaces -is [pscustomobject]) {
    $Changed = (Remove-JsonProperty -Object $ExtraKnownMarketplaces -Name $Info.marketplace_name) -or $Changed
  }

  if (-not $Changed) {
    return
  }

  if ($PSCmdlet.ShouldProcess($SettingsPath, 'Remove BlueprintHelper Claude plugin config')) {
    Set-Content -LiteralPath $SettingsPath -Value ($Settings | ConvertTo-Json -Depth 32) -Encoding UTF8
    Write-Host "Updated: $SettingsPath"
  }
}

function Remove-CodexAgents {
  $HomeDir = Get-UserHome
  $AgentDir = Join-Path $HomeDir '.codex\agents'
  foreach ($File in @('blueprint-explorer.toml', 'sourcecode-explorer.toml', 'sourcecode-worker.toml', 'task-worker.toml')) {
    Remove-FileIfPresent -Path (Join-Path $AgentDir $File) -Description 'Remove Codex subagent' -RequireBlueprintHelperOwnership | Out-Null
  }
  Remove-EmptyDirectoryIfPresent -Path $AgentDir
}

function Remove-ClaudeAgents {
  $HomeDir = Get-UserHome
  $AgentDir = Join-Path $HomeDir '.claude\agents'
  foreach ($File in @('blueprint-explorer.md', 'sourcecode-explorer.md', 'sourcecode-worker.md', 'task-worker.md')) {
    Remove-FileIfPresent -Path (Join-Path $AgentDir $File) -Description 'Remove Claude sideAgent' -RequireBlueprintHelperOwnership | Out-Null
  }
  Remove-EmptyDirectoryIfPresent -Path $AgentDir
}

function Remove-CodexLifecycleMcpConfig {
  $HomeDir = Get-UserHome
  $ConfigPath = Join-Path $HomeDir '.codex\config.toml'
  $Headers = @(
    '[mcp_servers."blueprint-helper"]',
    '[mcp_servers.blueprint-helper]',
    '[mcp_servers."blueprint-helper".env]',
    '[mcp_servers."blueprint-helper".tools.blueprint_lifecycle_mcp_status]',
    '[mcp_servers."blueprint-helper".tools.blueprint_open_editor]',
    '[mcp_servers."blueprint-helper".tools.blueprint_close_editor]',
    '[mcp_servers."blueprint-helper".tools.blueprint_dismiss_editor_dialogs]',
    '[mcp_servers."blueprint-helper".tools.blueprint_close_editor_dialogs]',
    '[mcp_servers."blueprint-helper".tools.blueprint_developer_exec_console_command]'
  )

  Remove-TomlOwnedSectionsFromFile -Path $ConfigPath -Headers $Headers -Description 'Remove BlueprintHelper lifecycle MCP config' | Out-Null
}

function Resolve-ProjectProfilePath {
  if ($ProjectFile) {
    $CleanProjectFile = Normalize-PathInput -PathText $ProjectFile
    if ($CleanProjectFile) {
      $ResolvedProjectFile = [System.IO.Path]::GetFullPath($CleanProjectFile)
      return Join-Path (Split-Path -Parent $ResolvedProjectFile) '.blueprinthelper\project-profile.json'
    }
  }

  $Candidates = @(Get-ChildItem -LiteralPath $Root -Filter '*.uproject' -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 2)
  if ($Candidates.Count -eq 1) {
    return Join-Path $Candidates[0].DirectoryName '.blueprinthelper\project-profile.json'
  }

  return $null
}

function Remove-ProjectAgentProfile {
  $ProfilePath = Resolve-ProjectProfilePath
  if (-not $ProfilePath) {
    Write-Warning 'Project profile path was not provided and could not be auto-detected; skipping project profile removal.'
    return
  }

  $ProfileDir = Split-Path -Parent $ProfilePath
  $ProjectDir = Split-Path -Parent $ProfileDir
  $HelperPath = Join-Path $Root 'InstallScripts\agent-workflow-install.mjs'
  $NodeCommand = Resolve-NodeCommandPath

  if ($NodeCommand -and (Test-Path -LiteralPath $HelperPath -PathType Leaf)) {
    if ($PSCmdlet.ShouldProcess($ProjectDir, 'Remove BlueprintHelper project Agent workflow')) {
      & $NodeCommand $HelperPath 'uninstall' '--project-dir' $ProjectDir
      if ($LASTEXITCODE -ne 0) {
        throw "Remove BlueprintHelper project Agent workflow failed with exit code $LASTEXITCODE."
      }
      Write-Host "Removed BlueprintHelper project Agent workflow markers from: $ProjectDir"
    }
  } else {
    Write-Warning 'Node.js or agent workflow uninstall helper was not found; removing profile files only.'
    Remove-FileIfPresent -Path $ProfilePath -Description 'Remove project BlueprintHelper project profile' | Out-Null
    Remove-FileIfPresent -Path (Join-Path $ProfileDir 'agent-profile.json') -Description 'Remove legacy BlueprintHelper agent profile' | Out-Null
    Remove-FileIfPresent -Path (Join-Path $ProfileDir 'AgentWorkFlow.md') -Description 'Remove BlueprintHelper AgentWorkFlow' | Out-Null
  }

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
  $script:SkipCodexPlugin = -not (Read-UninstallYesNo -Prompt 'Remove Codex local plugin config' -DefaultYes:(-not $SkipCodexPlugin))
  $script:SkipCodexAgents = -not (Read-UninstallYesNo -Prompt 'Remove Codex subagents' -DefaultYes:(-not $SkipCodexAgents))
  $script:SkipLifecycleMcp = -not (Read-UninstallYesNo -Prompt 'Remove Codex lifecycle MCP config' -DefaultYes:(-not $SkipLifecycleMcp))
  $script:SkipClaudePlugin = -not (Read-UninstallYesNo -Prompt 'Remove Claude local plugin config' -DefaultYes:(-not $SkipClaudePlugin))
  $script:SkipClaudeAgents = -not (Read-UninstallYesNo -Prompt 'Remove Claude sideAgents' -DefaultYes:(-not $SkipClaudeAgents))
  $script:RemoveProjectProfile = Read-UninstallYesNo -Prompt 'Remove project .blueprinthelper/project-profile.json, AgentWorkFlow, and root BlueprintHelper markers' -DefaultYes:$RemoveProjectProfile
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
  Remove-CodexPluginConfig
}
if (-not $SkipCodexAgents) {
  Remove-CodexAgents
}
if (-not $SkipLifecycleMcp) {
  Remove-CodexLifecycleMcpConfig
}
if (-not $SkipClaudePlugin) {
  Remove-ClaudePluginConfig
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
