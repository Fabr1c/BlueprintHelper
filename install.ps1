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
  [string]$InstallTipsBase64,
  [string]$ProjectFile,
  [string]$EngineRoot,
  [string]$EnginePluginDir,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'
trap {
  Write-Host ''
  Write-Host 'BlueprintHelper install failed.' -ForegroundColor Red
  if ($_.Exception -and $_.Exception.Message) {
    Write-Host $_.Exception.Message -ForegroundColor Red
  } else {
    Write-Host $_ -ForegroundColor Red
  }
  exit 1
}

$script:ThisCmdlet = $PSCmdlet
$script:NodeCommand = $null
$script:NpmCommand = $null

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

  $PowerShellShim = $Commands |
    Where-Object { $_ -and $_.Source -match '\.ps1$' } |
    Select-Object -First 1
  if ($PowerShellShim) {
    throw "npm resolved only to a PowerShell shim that may be blocked by ExecutionPolicy: $($PowerShellShim.Source). Add npm.cmd to PATH or reinstall Node.js/npm."
  }

  throw 'npm was not found on PATH.'
}

function Get-NodeCommand {
  if (-not $script:NodeCommand) {
    $script:NodeCommand = Resolve-CommandPath -Names @('node.exe', 'node') -DisplayName 'Node.js'
  }
  return $script:NodeCommand
}

function Get-NpmCommand {
  if (-not $script:NpmCommand) {
    $script:NpmCommand = Resolve-NpmCommandPath
  }
  return $script:NpmCommand
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
    try {
      & $FilePath @Arguments
    } catch {
      throw "$Description failed to start. $($_.Exception.Message)"
    }
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

  $ResolvedPackageDir = [System.IO.Path]::GetFullPath($PackageDir)
  Assert-Directory -Path $ResolvedPackageDir -Name 'npm package directory'
  Assert-File -Path (Join-Path $ResolvedPackageDir 'package.json') -Name 'npm package manifest'

  Push-Location $ResolvedPackageDir
  try {
    Invoke-External -Description "npm --cwd $ResolvedPackageDir $($Arguments -join ' ')" -FilePath (Get-NpmCommand) -Arguments $Arguments
  } finally {
    Pop-Location
  }
}

function Resolve-NpmGlobalBinDirectory {
  $NpmCommand = Get-NpmCommand
  $PrefixOutput = & $NpmCommand 'config' 'get' 'prefix' 2>$null
  if ($LASTEXITCODE -ne 0) {
    Write-Warning "Unable to resolve npm global prefix with '$NpmCommand config get prefix'."
    return $null
  }

  $Prefix = ($PrefixOutput | Select-Object -First 1)
  if ([string]::IsNullOrWhiteSpace($Prefix)) {
    Write-Warning 'npm global prefix was empty; cannot repair CLI shims.'
    return $null
  }

  $ResolvedPrefix = [System.IO.Path]::GetFullPath($Prefix.Trim())
  if ($env:OS -eq 'Windows_NT') {
    return $ResolvedPrefix
  }
  return Join-Path $ResolvedPrefix 'bin'
}

function Repair-BlueprintHelperCliShims {
  $GlobalBinDir = Resolve-NpmGlobalBinDirectory
  if (-not $GlobalBinDir) {
    return
  }

  $Removed = @()
  foreach ($CommandName in @('bh', 'blueprinthelper-cli')) {
    $PowerShellShim = Join-Path $GlobalBinDir "$CommandName.ps1"
    $CmdShim = Join-Path $GlobalBinDir "$CommandName.cmd"
    if ((Test-Path -LiteralPath $PowerShellShim -PathType Leaf) -and (Test-Path -LiteralPath $CmdShim -PathType Leaf)) {
      if ($script:ThisCmdlet.ShouldProcess($PowerShellShim, 'Remove PowerShell shim so Windows resolves the .cmd launcher')) {
        Remove-Item -LiteralPath $PowerShellShim -Force
      }
      $Removed += $PowerShellShim
    }
  }

  if ($Removed.Count -gt 0) {
    Write-Host "==> CLI shims: removed PowerShell .ps1 shims that can be blocked by ExecutionPolicy"
    foreach ($Path in $Removed) {
      Write-Host "    $Path"
    }
    Write-Host '    PowerShell will resolve bh/blueprinthelper-cli to the .cmd launchers installed by npm link.'
  } else {
    Write-Host '==> CLI shims: no blocking PowerShell .ps1 shims found, or .cmd launchers were not present.'
  }
}

function Write-InstallTips {
  param([string]$TipsBase64)

  if (-not $TipsBase64) {
    return
  }

  try {
    $TipsBytes = [System.Convert]::FromBase64String($TipsBase64)
    $TipsText = [System.Text.Encoding]::UTF8.GetString($TipsBytes)
  } catch {
    Write-Warning "Unable to decode install tips. $($_.Exception.Message)"
    return
  }

  Write-Host $TipsText
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

function Read-InstallPathDetails {
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
}

function Invoke-SequentialInstallWizard {
  Write-InstallTips -TipsBase64 $InstallTipsBase64
  Write-Host ''
  Write-Host 'BlueprintHelper interactive install'
  Write-Host "Source root: $Root"
  Write-Host ''

  $script:SkipBuild = -not (Read-InstallYesNo -Prompt 'Build AgentFaceService packages' -DefaultYes:(-not $SkipBuild))
  $script:SkipCliLink = -not (Read-InstallYesNo -Prompt 'Link the bh CLI globally' -DefaultYes:(-not $SkipCliLink))

  if (Read-InstallYesNo -Prompt 'Install Codex Desktop plugin support' -DefaultYes:(-not ($SkipCodexMarketplace -and $SkipCodexAgents -and $SkipLifecycleMcp))) {
    $script:SkipCodexMarketplace = -not (Read-InstallYesNo -Prompt '  Install Codex plugin through official entry' -DefaultYes:(-not $SkipCodexMarketplace))
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
  $script:SkipDefaultPreferences = -not (Read-InstallYesNo -Prompt 'Create default Claude/Codex user preference files when missing' -DefaultYes:(-not $SkipDefaultPreferences))
  $script:RunDiagnostics = Read-InstallYesNo -Prompt 'Run BlueprintHelper diagnostics after install' -DefaultYes:$RunDiagnostics

  $script:InstallUePluginToEngine = Read-InstallYesNo -Prompt 'Copy the UE plugin into the engine Plugins/Marketplace folder' -DefaultYes:$InstallUePluginToEngine
  $script:Force = Read-InstallYesNo -Prompt 'Allow replacing existing local links or engine plugin target when needed' -DefaultYes:$Force
  Read-InstallPathDetails
  Write-Host ''
}

function New-InstallMenuOption {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Key,
    [Parameter(Mandatory = $true)]
    [string]$Label,
    [Parameter(Mandatory = $true)]
    [bool]$Selected,
    [Parameter(Mandatory = $true)]
    [string]$Tip,
    [int]$Indent = 0,
    [string]$Parent = ''
  )

  return [pscustomobject]@{
    Key = $Key
    Label = $Label
    Selected = $Selected
    Tip = $Tip
    Indent = $Indent
    Parent = $Parent
    Enabled = $true
  }
}

function Get-InstallMenuOption {
  param(
    [Parameter(Mandatory = $true)]
    [object[]]$Options,
    [Parameter(Mandatory = $true)]
    [string]$Key
  )

  foreach ($Option in $Options) {
    if ($Option.Key -eq $Key) {
      return $Option
    }
  }
  return $null
}

function New-InstallMenuOptions {
  return @(
    (New-InstallMenuOption -Key 'build' -Label 'Build AgentFaceService packages' -Selected:(-not $SkipBuild) -Tip 'Install and build the shared task-core package, CLI package, and MCP compatibility package. Requires Node.js and npm on PATH.'),
    (New-InstallMenuOption -Key 'cliLink' -Label 'Link bh CLI globally' -Selected:(-not $SkipCliLink) -Tip 'Run npm link for the CLI package, then remove npm PowerShell .ps1 shims so bh resolves through the .cmd launcher and is not blocked by ExecutionPolicy.'),
    (New-InstallMenuOption -Key 'codexSupport' -Label 'Codex Desktop plugin support' -Selected:(-not ($SkipCodexMarketplace -and $SkipCodexAgents -and $SkipLifecycleMcp)) -Tip 'Enable Codex Desktop integration. Child items control official plugin installation, subagents, and lifecycle MCP config.'),
    (New-InstallMenuOption -Key 'codexMarketplace' -Label 'Install Codex plugin via official entry' -Selected:(-not $SkipCodexMarketplace) -Tip 'Register the repository local marketplace with the official Codex plugin installer, then install blueprint-helper from that marketplace.' -Indent 1 -Parent 'codexSupport'),
    (New-InstallMenuOption -Key 'codexAgents' -Label 'Install Codex subagents' -Selected:(-not $SkipCodexAgents) -Tip 'Install BlueprintHelper Codex subagent definitions into the user Codex agents directory.' -Indent 1 -Parent 'codexSupport'),
    (New-InstallMenuOption -Key 'lifecycleMcp' -Label 'Install lifecycle MCP config' -Selected:(-not $SkipLifecycleMcp) -Tip 'Install the global lifecycle-only MCP config used for opening and closing Unreal Editor from Codex.' -Indent 1 -Parent 'codexSupport'),
    (New-InstallMenuOption -Key 'claudePlugin' -Label 'Claude Code plugin support' -Selected:$InstallClaudePlugin -Tip 'Register the Claude plugin marketplace through the official Claude plugin installer, then install blueprint-helper from that marketplace when a callable Claude CLI is available.'),
    (New-InstallMenuOption -Key 'claudeAgents' -Label 'Install Claude sideAgent definitions' -Selected:($InstallClaudeAgents -or $InstallClaudePlugin) -Tip 'Install Claude sideAgent definitions. This can be selected with or without the Claude plugin support item.'),
    (New-InstallMenuOption -Key 'projectProfile' -Label 'Write project agent-profile.json' -Selected:(-not $SkipProjectProfile) -Tip 'Create or update .blueprinthelper/agent-profile.json for the detected Unreal project. Path prompts appear after menu confirmation.'),
    (New-InstallMenuOption -Key 'defaultPreferences' -Label 'Create default user preference files' -Selected:(-not $SkipDefaultPreferences) -Tip 'Create missing Claude/Codex BlueprintHelper user preference files without overwriting existing preference files.'),
    (New-InstallMenuOption -Key 'diagnostics' -Label 'Run diagnostics after install' -Selected:$RunDiagnostics -Tip 'Run BlueprintHelper static diagnostics after installation. Useful for validating CLI, profile, Bridge, and runtime configuration.'),
    (New-InstallMenuOption -Key 'ueEnginePlugin' -Label 'Copy UE plugin to Engine' -Selected:$InstallUePluginToEngine -Tip 'Copy the UE-side BlueprintHelper plugin into an Engine Plugins/Marketplace folder. Path prompts appear after menu confirmation.'),
    (New-InstallMenuOption -Key 'force' -Label 'Allow replacing existing targets' -Selected:$Force -Tip 'Allow the installer to replace existing local links or engine plugin targets when needed.')
  )
}

function Update-InstallMenuDependencies {
  param([Parameter(Mandatory = $true)][object[]]$Options)

  $CodexSupport = Get-InstallMenuOption -Options $Options -Key 'codexSupport'
  $CodexChildren = @(
    (Get-InstallMenuOption -Options $Options -Key 'codexMarketplace'),
    (Get-InstallMenuOption -Options $Options -Key 'codexAgents'),
    (Get-InstallMenuOption -Options $Options -Key 'lifecycleMcp')
  )

  foreach ($Child in $CodexChildren) {
    $Child.Enabled = $CodexSupport.Selected
    if (-not $CodexSupport.Selected) {
      $Child.Selected = $false
    }
  }
}

function Toggle-InstallMenuOption {
  param(
    [Parameter(Mandatory = $true)]
    [object[]]$Options,
    [Parameter(Mandatory = $true)]
    [object]$Option
  )

  if (-not $Option.Enabled) {
    return
  }

  $Option.Selected = -not $Option.Selected

  if ($Option.Key -eq 'codexSupport') {
    $CodexChildren = @(
      (Get-InstallMenuOption -Options $Options -Key 'codexMarketplace'),
      (Get-InstallMenuOption -Options $Options -Key 'codexAgents'),
      (Get-InstallMenuOption -Options $Options -Key 'lifecycleMcp')
    )
    foreach ($Child in $CodexChildren) {
      $Child.Selected = $Option.Selected
    }
  } elseif ($Option.Parent -eq 'codexSupport') {
    $CodexSupport = Get-InstallMenuOption -Options $Options -Key 'codexSupport'
    $AnyCodexChildSelected = $false
    foreach ($Key in @('codexMarketplace', 'codexAgents', 'lifecycleMcp')) {
      if ((Get-InstallMenuOption -Options $Options -Key $Key).Selected) {
        $AnyCodexChildSelected = $true
      }
    }
    $CodexSupport.Selected = $AnyCodexChildSelected
  } elseif ($Option.Key -eq 'claudePlugin' -and $Option.Selected) {
    (Get-InstallMenuOption -Options $Options -Key 'claudeAgents').Selected = $true
  }

  Update-InstallMenuDependencies -Options $Options
}

function Render-InstallMenu {
  param(
    [Parameter(Mandatory = $true)]
    [object[]]$Options,
    [Parameter(Mandatory = $true)]
    [int]$SelectedIndex
  )

  Clear-Host
  Write-Host 'BlueprintHelper interactive install'
  Write-Host "Source root: $Root"
  Write-Host ''
  Write-Host 'Use Up/Down to move, Space to toggle, Enter to start install, Esc to cancel.'
  Write-Host ''

  for ($Index = 0; $Index -lt $Options.Count; $Index++) {
    $Option = $Options[$Index]
    $Cursor = if ($Index -eq $SelectedIndex) { '>' } else { ' ' }
    $Check = if ($Option.Selected) { '[x]' } else { '[ ]' }
    if (-not $Option.Enabled) {
      $Check = '[-]'
    }
    $Indent = '  ' * $Option.Indent
    $Line = "$Cursor $Indent$Check $($Option.Label)"

    if (-not $Option.Enabled) {
      Write-Host $Line -ForegroundColor DarkGray
    } elseif ($Index -eq $SelectedIndex) {
      Write-Host $Line -ForegroundColor Cyan
    } else {
      Write-Host $Line
    }
  }

  $Current = $Options[$SelectedIndex]
  Write-Host ''
  Write-Host 'Tip:' -ForegroundColor Yellow
  if ($Current.Enabled) {
    Write-Host $Current.Tip
  } else {
    Write-Host "$($Current.Tip) Enable the parent option first."
  }
}

function Test-InstallMenuSupported {
  try {
    if ([Console]::IsInputRedirected -or [Console]::IsOutputRedirected) {
      return $false
    }
    $null = $Host.UI.RawUI.WindowSize
    return $true
  } catch {
    return $false
  }
}

function Apply-InstallMenuOptions {
  param([Parameter(Mandatory = $true)][object[]]$Options)

  $CodexSupport = (Get-InstallMenuOption -Options $Options -Key 'codexSupport').Selected
  $script:SkipBuild = -not (Get-InstallMenuOption -Options $Options -Key 'build').Selected
  $script:SkipCliLink = -not (Get-InstallMenuOption -Options $Options -Key 'cliLink').Selected
  $script:SkipCodexMarketplace = -not ($CodexSupport -and (Get-InstallMenuOption -Options $Options -Key 'codexMarketplace').Selected)
  $script:SkipCodexAgents = -not ($CodexSupport -and (Get-InstallMenuOption -Options $Options -Key 'codexAgents').Selected)
  $script:SkipLifecycleMcp = -not ($CodexSupport -and (Get-InstallMenuOption -Options $Options -Key 'lifecycleMcp').Selected)
  $script:InstallClaudePlugin = (Get-InstallMenuOption -Options $Options -Key 'claudePlugin').Selected
  $script:InstallClaudeAgents = (Get-InstallMenuOption -Options $Options -Key 'claudeAgents').Selected
  $script:SkipProjectProfile = -not (Get-InstallMenuOption -Options $Options -Key 'projectProfile').Selected
  $script:SkipDefaultPreferences = -not (Get-InstallMenuOption -Options $Options -Key 'defaultPreferences').Selected
  $script:RunDiagnostics = (Get-InstallMenuOption -Options $Options -Key 'diagnostics').Selected
  $script:InstallUePluginToEngine = (Get-InstallMenuOption -Options $Options -Key 'ueEnginePlugin').Selected
  $script:Force = (Get-InstallMenuOption -Options $Options -Key 'force').Selected
}

function Invoke-MenuInstallWizard {
  $Options = New-InstallMenuOptions
  Update-InstallMenuDependencies -Options $Options
  $SelectedIndex = 0

  while ($true) {
    Render-InstallMenu -Options $Options -SelectedIndex $SelectedIndex
    $KeyInfo = [Console]::ReadKey($true)

    switch ($KeyInfo.Key) {
      'UpArrow' {
        $SelectedIndex--
        if ($SelectedIndex -lt 0) {
          $SelectedIndex = $Options.Count - 1
        }
      }
      'DownArrow' {
        $SelectedIndex++
        if ($SelectedIndex -ge $Options.Count) {
          $SelectedIndex = 0
        }
      }
      'Spacebar' {
        Toggle-InstallMenuOption -Options $Options -Option $Options[$SelectedIndex]
      }
      'Enter' {
        Apply-InstallMenuOptions -Options $Options
        Clear-Host
        Write-Host 'BlueprintHelper install selections confirmed.'
        Write-Host "Source root: $Root"
        Write-Host ''
        Read-InstallPathDetails
        Write-Host ''
        return
      }
      'Escape' {
        throw 'Install cancelled by user.'
      }
    }
  }
}

function Invoke-InteractiveInstallWizard {
  if (Test-InstallMenuSupported) {
    try {
      Invoke-MenuInstallWizard
    } catch {
      if ($_.Exception.Message -eq 'Install cancelled by user.') {
        throw
      }
      Write-Warning "Interactive menu is unavailable. Falling back to legacy prompts. $($_.Exception.Message)"
      Invoke-SequentialInstallWizard
    }
  } else {
    Invoke-SequentialInstallWizard
  }
}

function Get-CodexPluginInstallInfo {
  $MarketplacePath = Join-Path $Root '.agents\plugins\marketplace.json'
  $ManifestPath = Join-Path $CodexPluginRoot '.codex-plugin\plugin.json'

  Assert-File -Path $MarketplacePath -Name 'Codex plugin marketplace'
  Assert-File -Path $ManifestPath -Name 'Codex plugin manifest'

  $Marketplace = Get-Content -Raw -LiteralPath $MarketplacePath | ConvertFrom-Json
  $Manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json

  if (-not $Marketplace.name) {
    throw "Codex marketplace is missing name: $MarketplacePath"
  }
  if (-not $Manifest.name) {
    throw "Codex plugin manifest is missing name: $ManifestPath"
  }

  $Entry = @($Marketplace.plugins | Where-Object { $_.name -eq $Manifest.name } | Select-Object -First 1)
  if (-not $Entry) {
    throw "Codex marketplace does not include plugin '$($Manifest.name)': $MarketplacePath"
  }

  return [pscustomobject]@{
    marketplace_path = $MarketplacePath
    marketplace_source = $Root
    marketplace_name = [string]$Marketplace.name
    plugin_name = [string]$Manifest.name
    install_spec = "$($Manifest.name)@$($Marketplace.name)"
  }
}

function Get-CodexOfficialPluginCommands {
  $Commands = @()
  foreach ($Name in @('droid', 'codex')) {
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

function Install-CodexPluginViaOfficialEntry {
  $Info = Get-CodexPluginInstallInfo
  $MarketplaceArgs = @('plugin', 'marketplace', 'add', $Info.marketplace_source)
  $InstallArgs = @('plugin', 'install', $Info.install_spec)
  $Failures = @()

  Write-Host "==> Codex marketplace source: $($Info.marketplace_source)"
  Write-Host "==> Codex plugin install spec: $($Info.install_spec)"

  foreach ($Command in (Get-CodexOfficialPluginCommands)) {
    try {
      Invoke-External -Description "Codex official marketplace install ($($Command.name))" -FilePath $Command.path -Arguments $MarketplaceArgs
      Invoke-External -Description "Codex official plugin install ($($Command.name))" -FilePath $Command.path -Arguments $InstallArgs

      return [pscustomobject]@{
        status = if ($WhatIfPreference) { 'whatif' } else { 'installed_via_official_entry' }
        command = $Command.name
        marketplace_command = "$($Command.name) $($MarketplaceArgs -join ' ')"
        install_command = "$($Command.name) $($InstallArgs -join ' ')"
        install_spec = $Info.install_spec
        marketplace_source = $Info.marketplace_source
      }
    } catch {
      $Failures += "$($Command.name): $($_.Exception.Message)"
      Write-Warning "Codex official plugin command failed through '$($Command.name)'. $($_.Exception.Message)"
    }
  }

  Write-Warning 'No callable Codex official plugin installer was available. The repository marketplace is ready; install from the Codex Plugins UI or run the commands below with the official Codex plugin CLI.'
  Write-Host "    plugin marketplace add $($Info.marketplace_source)"
  Write-Host "    plugin install $($Info.install_spec)"

  return [pscustomobject]@{
    status = 'manual_official_install_required'
    command = $null
    marketplace_command = "plugin marketplace add $($Info.marketplace_source)"
    install_command = "plugin install $($Info.install_spec)"
    install_spec = $Info.install_spec
    marketplace_source = $Info.marketplace_source
    failures = $Failures
  }
}

function Get-ClaudePluginInstallInfo {
  $ManifestPath = Join-Path $ClaudePluginRoot '.claude-plugin\plugin.json'
  $MarketplacePath = Join-Path $ClaudePluginRoot '.claude-plugin\marketplace.json'

  Assert-File -Path $ManifestPath -Name 'Claude plugin manifest'
  Assert-File -Path $MarketplacePath -Name 'Claude plugin marketplace'

  $Marketplace = Get-Content -Raw -LiteralPath $MarketplacePath | ConvertFrom-Json
  $Manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json

  if (-not $Marketplace.name) {
    throw "Claude marketplace is missing name: $MarketplacePath"
  }
  if (-not $Manifest.name) {
    throw "Claude plugin manifest is missing name: $ManifestPath"
  }

  $Entry = @($Marketplace.plugins | Where-Object { $_.name -eq $Manifest.name } | Select-Object -First 1)
  if (-not $Entry) {
    throw "Claude marketplace does not include plugin '$($Manifest.name)': $MarketplacePath"
  }

  return [pscustomobject]@{
    marketplace_path = $MarketplacePath
    marketplace_source = $ClaudePluginRoot
    marketplace_name = [string]$Marketplace.name
    plugin_name = [string]$Manifest.name
    install_spec = "$($Manifest.name)@$($Marketplace.name)"
  }
}

function Get-ClaudeOfficialPluginCommands {
  $Commands = @()
  foreach ($Name in @('claude', 'claude-code')) {
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

function Install-ClaudePluginViaOfficialEntry {
  $Info = Get-ClaudePluginInstallInfo
  $MarketplaceArgs = @('plugin', 'marketplace', 'add', $Info.marketplace_source)
  $InstallArgs = @('plugin', 'install', $Info.install_spec)
  $Failures = @()

  Write-Host "==> Claude marketplace source: $($Info.marketplace_source)"
  Write-Host "==> Claude plugin install spec: $($Info.install_spec)"

  foreach ($Command in (Get-ClaudeOfficialPluginCommands)) {
    try {
      Invoke-External -Description "Claude official marketplace install ($($Command.name))" -FilePath $Command.path -Arguments $MarketplaceArgs
      Invoke-External -Description "Claude official plugin install ($($Command.name))" -FilePath $Command.path -Arguments $InstallArgs

      return [pscustomobject]@{
        status = if ($WhatIfPreference) { 'whatif' } else { 'installed_via_official_entry' }
        command = $Command.name
        source_path = $Info.marketplace_source
        marketplace_command = "$($Command.name) $($MarketplaceArgs -join ' ')"
        install_command = "$($Command.name) $($InstallArgs -join ' ')"
        install_spec = $Info.install_spec
        marketplace_source = $Info.marketplace_source
      }
    } catch {
      $Failures += "$($Command.name): $($_.Exception.Message)"
      Write-Warning "Claude official plugin command failed through '$($Command.name)'. $($_.Exception.Message)"
    }
  }

  Write-Warning 'No callable Claude official plugin installer was available. The local Claude marketplace is ready; install from Claude Code with the official /plugin commands below.'
  Write-Host "    /plugin marketplace add $($Info.marketplace_source)"
  Write-Host "    /plugin install $($Info.install_spec)"

  return [pscustomobject]@{
    status = 'manual_official_install_required'
    command = $null
    source_path = $Info.marketplace_source
    marketplace_command = "/plugin marketplace add $($Info.marketplace_source)"
    install_command = "/plugin install $($Info.install_spec)"
    install_spec = $Info.install_spec
    marketplace_source = $Info.marketplace_source
    failures = $Failures
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
- Do not inspect BlueprintHelper plugin package or implementation source for ordinary plugin usage. Use installed skill instructions, AgentGuide, CLI reference, and templates instead. Plugin source reads are allowed only for explicit BlueprintHelper plugin development, installation repair, or debugging tasks.
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
      & (Get-NodeCommand) $CliEntry 'blueprinthelper_diagnostics' '--json' '{}' '--select' 'status,summary'
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
  Repair-BlueprintHelperCliShims
}

$UserHome = $env:USERPROFILE
if (-not $UserHome) {
  $UserHome = $env:HOME
}
if (-not $UserHome) {
  throw 'Unable to resolve user home directory.'
}

$CodexPluginResult = [pscustomobject]@{
  status = 'skipped'
  command = $null
  marketplace_command = $null
  install_command = $null
  install_spec = $null
  marketplace_source = $null
}

if (-not $SkipCodexMarketplace) {
  $CodexPluginResult = Install-CodexPluginViaOfficialEntry
}

if (-not $SkipCodexAgents) {
  Invoke-External -Description 'Install Codex subagents' -FilePath (Get-NodeCommand) -Arguments @((Join-Path $CodexPluginRoot 'scripts\install-codex-agents.cjs'))
}

if (-not $SkipLifecycleMcp) {
  Invoke-External -Description 'Install lifecycle-only MCP config' -FilePath (Get-NodeCommand) -Arguments @((Join-Path $CodexPluginRoot 'scripts\install-global-mcp.cjs'))
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
  $ClaudePluginResult = Install-ClaudePluginViaOfficialEntry
}

if ($InstallClaudeAgents -or $InstallClaudePlugin) {
  Invoke-External -Description 'Install Claude subagents' -FilePath (Get-NodeCommand) -Arguments @((Join-Path $CodexPluginRoot 'scripts\install-claude-agents.cjs'))
  $ClaudeAgentsStatus = if ($WhatIfPreference) { 'whatif' } else { 'installed' }
}

if ($InstallUePluginToEngine) {
  Install-UePluginToEngine
}

Write-Host ''
Write-Host 'BlueprintHelper install finished.'
Write-Host "Source root: $Root"
Write-Host 'CLI: bh or blueprinthelper-cli (.cmd launcher; installer removes blocking .ps1 shims when npm creates them)'
Write-Host 'CLI JSON input: prefer --file or pipe JSON to --stdin in PowerShell; avoid inline --json for non-trivial payloads.'
Write-Host "Codex plugin: $($CodexPluginResult.status)"
if ($CodexPluginResult.install_spec) {
  Write-Host "Codex plugin install spec: $($CodexPluginResult.install_spec)"
}
if ($CodexPluginResult.marketplace_source) {
  Write-Host "Codex marketplace source: $($CodexPluginResult.marketplace_source)"
}
if ($CodexPluginResult.command) {
  Write-Host "Codex installer command: $($CodexPluginResult.command)"
} elseif ($CodexPluginResult.install_command) {
  Write-Host "Codex marketplace command: $($CodexPluginResult.marketplace_command)"
  Write-Host "Codex install command: $($CodexPluginResult.install_command)"
}
Write-Host "Claude plugin: $($ClaudePluginResult.status)"
if ($ClaudePluginResult.source_path) {
  Write-Host "Claude plugin source: $($ClaudePluginResult.source_path)"
}
if ($ClaudePluginResult.install_spec) {
  Write-Host "Claude plugin install spec: $($ClaudePluginResult.install_spec)"
}
if ($ClaudePluginResult.command) {
  Write-Host "Claude installer command: $($ClaudePluginResult.command)"
} elseif ($ClaudePluginResult.install_command) {
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
