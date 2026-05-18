[CmdletBinding(SupportsShouldProcess = $true)]
param(
  [switch]$Interactive,
  [switch]$CheckOnly,
  [string]$Repository = 'Fabr1c/BlueprintHelper',
  [switch]$SkipPostInstall,
  [switch]$SkipBuild,
  [switch]$RunDiagnostics,
  [switch]$InstallClaudeAgents,
  [switch]$InstallUePluginToEngine,
  [string]$EngineRoot,
  [string]$EnginePluginDir,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

try {
  [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
} catch {
}

$Root = $PSScriptRoot
$CodexManifestPath = Join-Path $Root 'CodexPlugin\.codex-plugin\plugin.json'
$UePluginDescriptorPath = Join-Path $Root 'BlueprintHelper\BlueprintHelper.uplugin'

function Write-Step {
  param([Parameter(Mandatory = $true)][string]$Message)
  Write-Host ''
  Write-Host "==> $Message"
}

function Get-JsonFromFile {
  param([Parameter(Mandatory = $true)][string]$Path)

  if (-not (Test-Path -LiteralPath $Path)) {
    throw "Missing required file: $Path"
  }

  return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

function Get-CurrentBlueprintHelperVersion {
  if (Test-Path -LiteralPath $CodexManifestPath) {
    $Manifest = Get-JsonFromFile -Path $CodexManifestPath
    if ($Manifest.version) {
      return [string]$Manifest.version
    }
  }

  if (Test-Path -LiteralPath $UePluginDescriptorPath) {
    $Descriptor = Get-JsonFromFile -Path $UePluginDescriptorPath
    if ($Descriptor.VersionName) {
      return [string]$Descriptor.VersionName
    }
  }

  throw 'Unable to resolve current BlueprintHelper version.'
}

function Get-NormalizedVersionText {
  param([Parameter(Mandatory = $true)][string]$Version)

  $Text = $Version.Trim()
  if ($Text.StartsWith('v') -or $Text.StartsWith('V')) {
    $Text = $Text.Substring(1)
  }

  $Text = ($Text -split '[-+]')[0]
  if ($Text -notmatch '^\d+(\.\d+){0,2}$') {
    throw "Unsupported version format: $Version"
  }

  return $Text
}

function Get-VersionParts {
  param([Parameter(Mandatory = $true)][string]$Version)

  $Text = Get-NormalizedVersionText -Version $Version
  $Parts = $Text.Split('.')
  $Major = [int]$Parts[0]
  $Minor = 0
  $Patch = 0
  if ($Parts.Length -ge 2) {
    $Minor = [int]$Parts[1]
  }
  if ($Parts.Length -ge 3) {
    $Patch = [int]$Parts[2]
  }

  return @($Major, $Minor, $Patch)
}

function Compare-BlueprintHelperVersion {
  param(
    [Parameter(Mandatory = $true)][string]$Left,
    [Parameter(Mandatory = $true)][string]$Right
  )

  $LeftParts = Get-VersionParts -Version $Left
  $RightParts = Get-VersionParts -Version $Right

  for ($Index = 0; $Index -lt 3; $Index++) {
    if ($LeftParts[$Index] -lt $RightParts[$Index]) {
      return -1
    }
    if ($LeftParts[$Index] -gt $RightParts[$Index]) {
      return 1
    }
  }

  return 0
}

function Invoke-GitHubRest {
  param([Parameter(Mandatory = $true)][string]$Uri)

  $Params = @{
    Uri = $Uri
    Headers = @{
      'User-Agent' = 'BlueprintHelper-Updater'
      'Accept' = 'application/vnd.github+json'
    }
    ErrorAction = 'Stop'
  }
  if ($PSVersionTable.PSVersion.Major -lt 6) {
    $Params.UseBasicParsing = $true
  }

  return Invoke-RestMethod @Params
}

function Get-LatestReleaseInfo {
  Write-Step "Checking latest GitHub release for $Repository"
  $Release = Invoke-GitHubRest -Uri "https://api.github.com/repos/$Repository/releases/latest"

  if (-not $Release.tag_name) {
    throw 'Latest GitHub release does not include tag_name.'
  }
  if (-not $Release.zipball_url) {
    throw 'Latest GitHub release does not include zipball_url.'
  }

  return [pscustomobject]@{
    tag = [string]$Release.tag_name
    version = (Get-NormalizedVersionText -Version ([string]$Release.tag_name))
    name = [string]$Release.name
    url = [string]$Release.html_url
    zipball_url = [string]$Release.zipball_url
  }
}

function Read-UpdateConfirmation {
  param(
    [Parameter(Mandatory = $true)][string]$CurrentVersion,
    [Parameter(Mandatory = $true)][object]$ReleaseInfo
  )

  if ($Force) {
    return $true
  }

  Write-Host ''
  Write-Host 'Update available.'
  Write-Host "Current version: v$(Get-NormalizedVersionText -Version $CurrentVersion)"
  Write-Host "Latest version:  v$($ReleaseInfo.version)"
  if ($ReleaseInfo.url) {
    Write-Host "Release page:    $($ReleaseInfo.url)"
  }
  Write-Host ''

  $Answer = Read-Host 'Update BlueprintHelper now? [y/N]'
  return ($Answer -match '^(y|yes)$')
}

function Get-BackupDirectory {
  param([Parameter(Mandatory = $true)][string]$CurrentVersion)

  $Parent = Split-Path -Parent $Root
  $Leaf = Split-Path -Leaf $Root
  $VersionText = Get-NormalizedVersionText -Version $CurrentVersion
  $Timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
  $Candidate = Join-Path $Parent "$Leaf.backup-v$VersionText-$Timestamp"

  if (Test-Path -LiteralPath $Candidate) {
    throw "Backup directory already exists: $Candidate"
  }

  return $Candidate
}

function Invoke-RobocopyMirror {
  param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Destination,
    [Parameter(Mandatory = $true)][string]$Description
  )

  $Args = @(
    $Source,
    $Destination,
    '/MIR',
    '/R:2',
    '/W:1'
  )

  Write-Step $Description
  Write-Host "robocopy `"$Source`" `"$Destination`" /MIR"
  if ($PSCmdlet.ShouldProcess($Destination, $Description)) {
    & robocopy @Args | Out-Host
    if ($LASTEXITCODE -gt 7) {
      throw "robocopy failed with exit code $LASTEXITCODE."
    }
    $global:LASTEXITCODE = 0
    return $true
  }

  return $false
}

function Download-AndExpandRelease {
  param([Parameter(Mandatory = $true)][object]$ReleaseInfo)

  $TempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("BlueprintHelperUpdate_" + [System.Guid]::NewGuid().ToString('N'))
  $ZipPath = Join-Path $TempDir 'release.zip'
  $ExtractDir = Join-Path $TempDir 'expanded'

  New-Item -ItemType Directory -Force -Path $TempDir | Out-Null
  New-Item -ItemType Directory -Force -Path $ExtractDir | Out-Null

  $DownloadParams = @{
    Uri = $ReleaseInfo.zipball_url
    OutFile = $ZipPath
    Headers = @{ 'User-Agent' = 'BlueprintHelper-Updater' }
    ErrorAction = 'Stop'
  }
  if ($PSVersionTable.PSVersion.Major -lt 6) {
    $DownloadParams.UseBasicParsing = $true
  }

  Write-Step "Downloading release $($ReleaseInfo.tag)"
  Invoke-WebRequest @DownloadParams

  Write-Step 'Expanding release package'
  Expand-Archive -LiteralPath $ZipPath -DestinationPath $ExtractDir -Force

  return [pscustomobject]@{
    temp_dir = $TempDir
    extract_dir = $ExtractDir
  }
}

function Find-ExtractedPackageRoot {
  param(
    [Parameter(Mandatory = $true)][string]$ExtractDir,
    [Parameter(Mandatory = $true)][object]$ReleaseInfo
  )

  $Candidates = Get-ChildItem -LiteralPath $ExtractDir -Directory
  foreach ($Candidate in $Candidates) {
    $InstallScript = Join-Path $Candidate.FullName 'install.ps1'
    $CodexManifest = Join-Path $Candidate.FullName 'CodexPlugin\.codex-plugin\plugin.json'
    $UeDescriptor = Join-Path $Candidate.FullName 'BlueprintHelper\BlueprintHelper.uplugin'
    $AgentFaceService = Join-Path $Candidate.FullName 'AgentFaceService'

    if ((Test-Path -LiteralPath $InstallScript) -and
        (Test-Path -LiteralPath $CodexManifest) -and
        (Test-Path -LiteralPath $UeDescriptor) -and
        (Test-Path -LiteralPath $AgentFaceService)) {
      $PackageManifest = Get-JsonFromFile -Path $CodexManifest
      if (-not $PackageManifest.version) {
        throw "Downloaded package manifest has no version: $CodexManifest"
      }

      if ((Compare-BlueprintHelperVersion -Left ([string]$PackageManifest.version) -Right $ReleaseInfo.version) -ne 0) {
        throw "Downloaded package version $($PackageManifest.version) does not match release tag $($ReleaseInfo.tag)."
      }

      return $Candidate.FullName
    }
  }

  throw "Unable to locate a valid BlueprintHelper package inside $ExtractDir."
}

function Test-ClaudeAgentsInstalled {
  $HomeDir = $env:USERPROFILE
  if (-not $HomeDir) {
    $HomeDir = $env:HOME
  }
  if (-not $HomeDir) {
    return $false
  }

  return (Test-Path -LiteralPath (Join-Path $HomeDir '.claude\agents\blueprint-explorer.md'))
}

function Invoke-PostInstallRefresh {
  $InstallScript = Join-Path $Root 'install.ps1'
  if (-not (Test-Path -LiteralPath $InstallScript)) {
    throw "Missing install script after update: $InstallScript"
  }

  $Args = @(
    '-SkipProjectProfile',
    '-SkipDefaultPreferences'
  )

  if ($SkipBuild) {
    $Args += '-SkipBuild'
  }
  if ($RunDiagnostics) {
    $Args += '-RunDiagnostics'
  }
  if ($InstallClaudeAgents -or (Test-ClaudeAgentsInstalled)) {
    $Args += '-InstallClaudeAgents'
  }
  if ($InstallUePluginToEngine) {
    $Args += '-InstallUePluginToEngine'
    if ($EngineRoot) {
      $Args += @('-EngineRoot', $EngineRoot)
    }
    if ($EnginePluginDir) {
      $Args += @('-EnginePluginDir', $EnginePluginDir)
    }
  }
  if ($Force) {
    $Args += '-Force'
  }

  Write-Step 'Running post-update install refresh'
  Write-Host "powershell -NoProfile -ExecutionPolicy Bypass -File `"$InstallScript`" $($Args -join ' ')"
  & powershell -NoProfile -ExecutionPolicy Bypass -File $InstallScript @Args
  if ($LASTEXITCODE -ne 0) {
    throw "Post-update install refresh failed with exit code $LASTEXITCODE."
  }
}

function Restore-Backup {
  param([Parameter(Mandatory = $true)][string]$BackupDir)

  if (-not (Test-Path -LiteralPath $BackupDir)) {
    throw "Backup directory is missing, cannot roll back: $BackupDir"
  }

  Invoke-RobocopyMirror -Source $BackupDir -Destination $Root -Description 'Rolling back to backup'
}

function Invoke-BlueprintHelperUpdate {
  Write-Host 'BlueprintHelper updater'
  Write-Host "Source root: $Root"

  $CurrentVersion = Get-CurrentBlueprintHelperVersion
  $ReleaseInfo = Get-LatestReleaseInfo
  $Comparison = Compare-BlueprintHelperVersion -Left $CurrentVersion -Right $ReleaseInfo.version

  Write-Host ''
  Write-Host "Current version: v$(Get-NormalizedVersionText -Version $CurrentVersion)"
  Write-Host "Latest version:  v$($ReleaseInfo.version)"
  Write-Host "Release tag:     $($ReleaseInfo.tag)"
  if ($ReleaseInfo.url) {
    Write-Host "Release page:    $($ReleaseInfo.url)"
  }

  if ($Comparison -eq 0) {
    Write-Host ''
    Write-Host 'BlueprintHelper is already up to date.'
    return
  }

  if ($Comparison -gt 0) {
    Write-Host ''
    Write-Host 'Local BlueprintHelper version is newer than the latest GitHub release. No update was applied.' -ForegroundColor Yellow
    return
  }

  if ($CheckOnly) {
    Write-Host ''
    Write-Host 'An update is available. Re-run without -CheckOnly to apply it.'
    exit 2
  }

  if (-not (Read-UpdateConfirmation -CurrentVersion $CurrentVersion -ReleaseInfo $ReleaseInfo)) {
    Write-Host ''
    Write-Host 'Update cancelled.'
    return
  }

  $BackupDir = $null
  $PackageTempDir = $null
  $ReplaceStarted = $false
  $DidReplace = $false

  try {
    $Package = Download-AndExpandRelease -ReleaseInfo $ReleaseInfo
    $PackageTempDir = $Package.temp_dir
    $PackageRoot = Find-ExtractedPackageRoot -ExtractDir $Package.extract_dir -ReleaseInfo $ReleaseInfo

    $BackupDir = Get-BackupDirectory -CurrentVersion $CurrentVersion
    Invoke-RobocopyMirror -Source $Root -Destination $BackupDir -Description 'Backing up current BlueprintHelper directory' | Out-Null

    $ReplaceStarted = $true
    $DidReplace = Invoke-RobocopyMirror -Source $PackageRoot -Destination $Root -Description 'Replacing BlueprintHelper with downloaded release'

    if ($WhatIfPreference) {
      Write-Host ''
      Write-Host 'WhatIf: update was not applied.'
      return
    }

    if (-not $SkipPostInstall) {
      Invoke-PostInstallRefresh
    }

    $UpdatedVersion = Get-CurrentBlueprintHelperVersion

    Write-Host ''
    Write-Host 'BlueprintHelper update finished.'
    Write-Host "Previous version: v$(Get-NormalizedVersionText -Version $CurrentVersion)"
    Write-Host "Current version:  v$(Get-NormalizedVersionText -Version $UpdatedVersion)"
    Write-Host "Backup path:      $BackupDir"
    Write-Host 'UE engine plugin copy is updated only when -InstallUePluginToEngine is passed.'
  } catch {
    $Failure = $_
    if (($ReplaceStarted -or $DidReplace) -and $BackupDir) {
      Write-Host ''
      Write-Host 'Update failed after replacement. Attempting rollback...' -ForegroundColor Yellow
      try {
        Restore-Backup -BackupDir $BackupDir
        Write-Host "Rollback restored backup: $BackupDir" -ForegroundColor Yellow
      } catch {
        Write-Host "Rollback failed: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "Backup path: $BackupDir" -ForegroundColor Red
      }
    }

    throw $Failure
  } finally {
    if ($PackageTempDir -and (Test-Path -LiteralPath $PackageTempDir)) {
      Remove-Item -LiteralPath $PackageTempDir -Recurse -Force
    }
  }
}

try {
  Invoke-BlueprintHelperUpdate
} catch {
  Write-Host ''
  Write-Host 'BlueprintHelper update failed.' -ForegroundColor Red
  if ($_.Exception -and $_.Exception.Message) {
    Write-Host $_.Exception.Message -ForegroundColor Red
  } else {
    Write-Host $_ -ForegroundColor Red
  }
  exit 1
}
