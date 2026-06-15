[CmdletBinding(SupportsShouldProcess = $true)]
param(
  [switch]$Interactive,
  [switch]$CheckOnly,
  [string]$Repository = 'Fabr1c/BlueprintHelper',
  [switch]$SkipPostInstall,
  [switch]$SkipBuild,
  [switch]$RunDiagnostics,
  [switch]$InstallClaudePlugin,
  [switch]$InstallClaudeAgents,
  [switch]$InstallUePluginToEngine,
  [string]$EngineRoot,
  [string]$EnginePluginDir,
  [string]$RunnerPackageRoot,
  [string]$RunnerReleaseInfoFile,
  [string]$RunnerTempDir,
  [string]$TargetRoot,
  [switch]$SkipBootstrap,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

try {
  [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
} catch {
}

$ScriptRoot = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($TargetRoot)) {
  $Root = Split-Path -Parent $ScriptRoot
} else {
  $Root = [System.IO.Path]::GetFullPath($TargetRoot)
}
$CodexManifestPath = Join-Path $Root 'CodexPlugin\.codex-plugin\plugin.json'
$UePluginDescriptorPath = Join-Path $Root 'BlueprintHelper\BlueprintHelper.uplugin'
$script:UpdateProgressActivity = 'BlueprintHelper update'
$script:UpdateProgressLastPercent = 0
$script:UpdateFailureCode = 'BH-UPD-UNHANDLED'
$script:UpdateFailureStage = 'startup'
$script:UpdateFailureLogPath = $null

function Set-UpdateFailureContext {
  param(
    [string]$Code = 'BH-UPD-UNHANDLED',
    [Parameter(Mandatory = $true)]
    [string]$Stage,
    [AllowNull()]
    [string]$LogPath = $null
  )

  $script:UpdateFailureCode = $Code
  $script:UpdateFailureStage = $Stage
  if (-not [string]::IsNullOrWhiteSpace($LogPath)) {
    $script:UpdateFailureLogPath = $LogPath
  }
}

function New-UpdateLogPath {
  param([Parameter(Mandatory = $true)][string]$Name)

  $LogRoot = [Environment]::GetFolderPath('LocalApplicationData')
  if ([string]::IsNullOrWhiteSpace($LogRoot)) {
    $LogRoot = [System.IO.Path]::GetTempPath()
  }

  $LogRoot = Join-Path $LogRoot 'BlueprintHelper\Logs'
  New-Item -ItemType Directory -Path $LogRoot -Force | Out-Null
  $Timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
  $SafeName = $Name -replace '[^A-Za-z0-9._-]', '-'
  return Join-Path $LogRoot "$SafeName-$Timestamp.log"
}

function Write-Step {
  param([Parameter(Mandatory = $true)][string]$Message)
  Write-Host ''
  Write-Host "==> $Message"
}

function Write-UpdateProgressBar {
  param(
    [Parameter(Mandatory = $true)][int]$Percent,
    [Parameter(Mandatory = $true)][string]$Status
  )

  $ClampedPercent = [Math]::Max(0, [Math]::Min(100, $Percent))
  $script:UpdateProgressLastPercent = $ClampedPercent
  $Width = 30
  $Filled = [int][Math]::Floor(($ClampedPercent / 100) * $Width)
  $Empty = $Width - $Filled
  $Bar = ('#' * $Filled) + ('-' * $Empty)

  Write-Host ("[{0}] {1,3}%  {2}" -f $Bar, $ClampedPercent, $Status)
  try {
    Write-Progress -Activity $script:UpdateProgressActivity -Status $Status -PercentComplete $ClampedPercent
  } catch {
  }
}

function Complete-UpdateProgressBar {
  param([Parameter(Mandatory = $true)][string]$Status)

  Write-UpdateProgressBar -Percent 100 -Status $Status
  try {
    Write-Progress -Activity $script:UpdateProgressActivity -Completed
  } catch {
  }
}

function Stop-UpdateProgressBar {
  param([Parameter(Mandatory = $true)][string]$Status)

  Write-UpdateProgressBar -Percent $script:UpdateProgressLastPercent -Status $Status
  try {
    Write-Progress -Activity $script:UpdateProgressActivity -Completed
  } catch {
  }
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

function Get-BlueprintHelperReleaseTagParts {
  param([Parameter(Mandatory = $true)][string]$Version)

  $Text = $Version.Trim()
  if ($Text.StartsWith('v') -or $Text.StartsWith('V')) {
    $Text = $Text.Substring(1)
  }

  $Match = [regex]::Match($Text, '^(?<base>\d+(\.\d+){0,2})(?<suffix>-.+)?(?:\+.*)?$')
  if (-not $Match.Success) {
    throw "Unsupported version format: $Version"
  }

  $Suffix = $Match.Groups['suffix'].Value
  return [pscustomobject]@{
    base_version = $Match.Groups['base'].Value
    suffix = if ([string]::IsNullOrWhiteSpace($Suffix)) { $null } else { $Suffix }
    has_update_suffix = -not [string]::IsNullOrWhiteSpace($Suffix)
  }
}

function Get-NormalizedVersionText {
  param([Parameter(Mandatory = $true)][string]$Version)

  return (Get-BlueprintHelperReleaseTagParts -Version $Version).base_version
}

function Test-ReleaseTagHasUpdateSuffix {
  param([Parameter(Mandatory = $true)][string]$Tag)

  return (Get-BlueprintHelperReleaseTagParts -Version $Tag).has_update_suffix
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

function Get-StableTextHash {
  param([Parameter(Mandatory = $true)][string]$Text)

  $Bytes = [Text.Encoding]::UTF8.GetBytes($Text.ToLowerInvariant())
  $Sha256 = [Security.Cryptography.SHA256]::Create()
  try {
    $Hash = $Sha256.ComputeHash($Bytes)
  } finally {
    $Sha256.Dispose()
  }

  return -join ($Hash | ForEach-Object { $_.ToString('x2') })
}

function Get-UpdateStatePaths {
  $LocalAppData = [Environment]::GetFolderPath('LocalApplicationData')
  $RootHash = Get-StableTextHash -Text (Resolve-Path -LiteralPath $Root).Path
  $Paths = New-Object System.Collections.Generic.List[string]

  if (-not [string]::IsNullOrWhiteSpace($LocalAppData)) {
    $StateRoot = Join-Path $LocalAppData 'BlueprintHelper\UpdateState'
    $Paths.Add((Join-Path $StateRoot "$RootHash.json"))
  }

  $FallbackStateRoot = Join-Path $Root '.blueprinthelper'
  $Paths.Add((Join-Path $FallbackStateRoot 'update-state.json'))
  return @($Paths)
}

function Get-UpdateStatePath {
  return @(Get-UpdateStatePaths)[0]
}

function Get-UpdateState {
  $StatePaths = @(Get-UpdateStatePaths)
  foreach ($StatePath in $StatePaths) {
    if (-not (Test-Path -LiteralPath $StatePath -PathType Leaf)) {
      continue
    }

    try {
      $State = Get-Content -Raw -LiteralPath $StatePath | ConvertFrom-Json
      return [pscustomobject]@{
        path = $StatePath
        state_paths = $StatePaths
        applied_release_tag = if ($State.applied_release_tag) { [string]$State.applied_release_tag } else { $null }
        applied_base_version = if ($State.applied_base_version) { [string]$State.applied_base_version } else { $null }
      }
    } catch {
      Write-Host "Warning: unable to read update state: $StatePath" -ForegroundColor Yellow
    }
  }

  return [pscustomobject]@{
    path = $StatePaths[0]
    state_paths = $StatePaths
    applied_release_tag = $null
    applied_base_version = $null
  }
}

function Write-UpdateState {
  param(
    [Parameter(Mandatory = $true)][object]$ReleaseInfo,
    [Parameter(Mandatory = $true)][string]$InstalledVersion
  )

  $StatePaths = @(Get-UpdateStatePaths)
  $State = [ordered]@{
    schema = 'BlueprintHelper.UpdateState.v1'
    source_root = (Resolve-Path -LiteralPath $Root).Path
    applied_release_tag = [string]$ReleaseInfo.tag
    applied_base_version = [string]$ReleaseInfo.version
    installed_version = (Get-NormalizedVersionText -Version $InstalledVersion)
    updated_at = (Get-Date).ToUniversalTime().ToString('o')
  }

  $WrittenPaths = @()
  foreach ($StatePath in $StatePaths) {
    try {
      $StateRoot = Split-Path -Parent $StatePath
      New-Item -ItemType Directory -Path $StateRoot -Force | Out-Null
      ($State | ConvertTo-Json -Depth 5) | Set-Content -LiteralPath $StatePath -Encoding UTF8
      $WrittenPaths += $StatePath
    } catch {
      Write-Host "Warning: update state could not be written: $StatePath" -ForegroundColor Yellow
      Write-Host "Warning: $($_.Exception.Message)" -ForegroundColor Yellow
    }
  }

  if ($WrittenPaths.Count -eq 0) {
    return
  }

  return $WrittenPaths
}

function Get-BlueprintHelperUpdateDecision {
  param(
    [Parameter(Mandatory = $true)][string]$CurrentVersion,
    [Parameter(Mandatory = $true)][object]$ReleaseInfo,
    [Parameter(Mandatory = $true)][object]$UpdateState
  )

  $Comparison = Compare-BlueprintHelperVersion -Left $CurrentVersion -Right $ReleaseInfo.version
  if ($Comparison -lt 0) {
    return [pscustomobject]@{
      should_update = $true
      status = 'newer_base_version'
      message = 'Remote base version is newer than the local version.'
    }
  }
  if ($Comparison -gt 0) {
    return [pscustomobject]@{
      should_update = $false
      status = 'local_newer'
      message = 'Local BlueprintHelper version is newer than the latest GitHub release.'
    }
  }

  if (Test-ReleaseTagHasUpdateSuffix -Tag $ReleaseInfo.tag) {
    if ($UpdateState.applied_release_tag -eq $ReleaseInfo.tag -and $UpdateState.applied_base_version -eq $ReleaseInfo.version) {
      return [pscustomobject]@{
        should_update = $false
        status = 'suffix_tag_already_applied'
        message = 'Latest same-version patch release tag is already applied.'
      }
    }

    return [pscustomobject]@{
      should_update = $true
      status = 'same_base_patch_tag'
      message = 'Latest release tag has a same-version patch suffix that has not been applied.'
    }
  }

  return [pscustomobject]@{
    should_update = $false
    status = 'same_base_version'
    message = 'Local BlueprintHelper version matches the latest release base version.'
  }
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
  Write-Host "Release tag:     $($ReleaseInfo.tag)"
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

function Get-BackupArchiveRoot {
  $Current = Get-Item -LiteralPath $Root
  $Ancestor = $Current.Parent
  while ($Ancestor) {
    if ($Ancestor.Name -ieq 'Plugins' -and $Ancestor.Parent) {
      return Join-Path $Ancestor.Parent.FullName 'BlueprintHelperBackups'
    }
    $Ancestor = $Ancestor.Parent
  }

  $Parent = Split-Path -Parent $Root
  return Join-Path $Parent 'BlueprintHelperBackups'
}

function Get-FallbackBackupArchiveRoot {
  $LocalAppData = [Environment]::GetFolderPath('LocalApplicationData')
  if ([string]::IsNullOrWhiteSpace($LocalAppData)) {
    $LocalAppData = [System.IO.Path]::GetTempPath()
  }

  return Join-Path $LocalAppData 'BlueprintHelper\Backups'
}

function Get-UniqueDirectoryPath {
  param([Parameter(Mandatory = $true)][string]$BasePath)

  if (-not (Test-Path -LiteralPath $BasePath)) {
    return $BasePath
  }

  for ($Index = 1; $Index -le 99; $Index++) {
    $Candidate = "$BasePath-$Index"
    if (-not (Test-Path -LiteralPath $Candidate)) {
      return $Candidate
    }
  }

  throw "Unable to find an unused backup archive directory for: $BasePath"
}

function Move-BackupToArchiveRoot {
  param(
    [Parameter(Mandatory = $true)][string]$BackupDir,
    [Parameter(Mandatory = $true)][string]$ArchiveRoot,
    [Parameter(Mandatory = $true)][string]$Description
  )

  $ArchiveDir = Get-UniqueDirectoryPath -BasePath (Join-Path $ArchiveRoot (Split-Path -Leaf $BackupDir))

  Write-Step $Description
  Write-Host "move `"$BackupDir`" `"$ArchiveDir`""
  if ($PSCmdlet.ShouldProcess($ArchiveDir, $Description)) {
    New-Item -ItemType Directory -Path $ArchiveRoot -Force | Out-Null
    Move-Item -LiteralPath $BackupDir -Destination $ArchiveDir -Force
  }

  return $ArchiveDir
}

function Move-BackupOutsidePluginScanPath {
  param([Parameter(Mandatory = $true)][string]$BackupDir)

  if (-not (Test-Path -LiteralPath $BackupDir -PathType Container)) {
    return $null
  }

  $PrimaryRoot = Get-BackupArchiveRoot
  try {
    return Move-BackupToArchiveRoot -BackupDir $BackupDir -ArchiveRoot $PrimaryRoot -Description 'Moving backup outside Unreal Plugins scan path'
  } catch {
    $PrimaryFailure = $_
    Write-Host "Primary backup archive failed: $($PrimaryFailure.Exception.Message)" -ForegroundColor Yellow
  }

  $FallbackRoot = Get-FallbackBackupArchiveRoot
  if ($FallbackRoot -ieq $PrimaryRoot) {
    throw $PrimaryFailure
  }

  try {
    return Move-BackupToArchiveRoot -BackupDir $BackupDir -ArchiveRoot $FallbackRoot -Description 'Moving backup to local fallback archive'
  } catch {
    throw "Failed to move backup outside the Unreal Plugins scan path. Primary failure: $($PrimaryFailure.Exception.Message). Fallback failure: $($_.Exception.Message)."
  }
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

  Write-UpdateProgressBar -Percent 30 -Status "Downloading release $($ReleaseInfo.tag)"
  Write-Step "Downloading release $($ReleaseInfo.tag)"
  Invoke-WebRequest @DownloadParams

  Write-UpdateProgressBar -Percent 42 -Status 'Expanding release package'
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
    $InstallScript = Resolve-InstallScriptPath -PackageRoot $Candidate.FullName -AllowLegacyRoot
    $CodexManifest = Join-Path $Candidate.FullName 'CodexPlugin\.codex-plugin\plugin.json'
    $UeDescriptor = Join-Path $Candidate.FullName 'BlueprintHelper\BlueprintHelper.uplugin'
    $AgentFaceService = Join-Path $Candidate.FullName 'AgentFaceService'

    if ($InstallScript -and
        (Test-Path -LiteralPath $CodexManifest) -and
        (Test-Path -LiteralPath $UeDescriptor) -and
        (Test-Path -LiteralPath $AgentFaceService)) {
      $PackageManifest = Get-JsonFromFile -Path $CodexManifest
      if (-not $PackageManifest.version) {
        throw "Downloaded package manifest has no version: $CodexManifest"
      }
      if ($PackageManifest.name -and ([string]$PackageManifest.name -ne 'blueprint-helper')) {
        throw "Downloaded package manifest name $($PackageManifest.name) is not blueprint-helper."
      }

      if ((Compare-BlueprintHelperVersion -Left ([string]$PackageManifest.version) -Right $ReleaseInfo.version) -ne 0) {
        throw "Downloaded package base version $($PackageManifest.version) does not match release base version $($ReleaseInfo.version) from tag $($ReleaseInfo.tag)."
      }

      return $Candidate.FullName
    }
  }

  throw "Unable to locate a valid BlueprintHelper package inside $ExtractDir."
}

function Resolve-InstallScriptPath {
  param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,
    [switch]$AllowLegacyRoot
  )

  $Candidates = @(
    (Join-Path $PackageRoot 'InstallScripts\install.ps1')
  )
  if ($AllowLegacyRoot) {
    $Candidates += (Join-Path $PackageRoot 'install.ps1')
  }

  foreach ($Candidate in $Candidates) {
    if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
      return $Candidate
    }
  }

  return $null
}

function Resolve-UpdateScriptPath {
  param([Parameter(Mandatory = $true)][string]$PackageRoot)

  $Candidate = Join-Path $PackageRoot 'InstallScripts\update.ps1'
  if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
    return $Candidate
  }

  return $null
}

function Get-RunnerReleaseInfo {
  if ([string]::IsNullOrWhiteSpace($RunnerReleaseInfoFile)) {
    Set-UpdateFailureContext -Code 'BH-UPD-RUNNER-FAILED' -Stage 'runner_release_info'
    throw 'Update runner did not receive release metadata.'
  }
  if (-not (Test-Path -LiteralPath $RunnerReleaseInfoFile -PathType Leaf)) {
    Set-UpdateFailureContext -Code 'BH-UPD-RUNNER-FAILED' -Stage 'runner_release_info'
    throw "Update runner release metadata file is missing: $RunnerReleaseInfoFile"
  }

  $Info = Get-JsonFromFile -Path $RunnerReleaseInfoFile
  if (-not $Info.tag -or -not $Info.version) {
    Set-UpdateFailureContext -Code 'BH-UPD-RUNNER-FAILED' -Stage 'runner_release_info'
    throw "Update runner release metadata is incomplete: $RunnerReleaseInfoFile"
  }

  return [pscustomobject]@{
    tag = [string]$Info.tag
    version = [string]$Info.version
    name = if ($Info.name) { [string]$Info.name } else { '' }
    url = if ($Info.url) { [string]$Info.url } else { '' }
    zipball_url = if ($Info.zipball_url) { [string]$Info.zipball_url } else { '' }
  }
}

function Start-UpdateRunnerFromPackage {
  param(
    [Parameter(Mandatory = $true)][string]$PackageRoot,
    [Parameter(Mandatory = $true)][object]$ReleaseInfo,
    [Parameter(Mandatory = $true)][string]$PackageTempDir
  )

  Set-UpdateFailureContext -Code 'BH-UPD-BOOTSTRAP-FAILED' -Stage 'bootstrap_update_runner'
  $SourceScript = Resolve-UpdateScriptPath -PackageRoot $PackageRoot
  if (-not $SourceScript) {
    throw "Downloaded package does not include InstallScripts\\update.ps1: $PackageRoot"
  }

  $RunnerRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("BlueprintHelperUpdateRunner_" + [System.Guid]::NewGuid().ToString('N'))
  $RunnerScript = Join-Path $RunnerRoot 'InstallScripts\update.ps1'
  $ReleaseInfoPath = Join-Path $RunnerRoot 'release-info.json'

  try {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $RunnerScript) | Out-Null
    Copy-Item -LiteralPath $SourceScript -Destination $RunnerScript -Force
    $ReleaseInfo | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ReleaseInfoPath -Encoding UTF8

    $Args = @(
      '-NoProfile',
      '-ExecutionPolicy',
      'Bypass',
      '-File',
      $RunnerScript,
      '-SkipBootstrap',
      '-RunnerPackageRoot',
      $PackageRoot,
      '-RunnerReleaseInfoFile',
      $ReleaseInfoPath,
      '-RunnerTempDir',
      $PackageTempDir,
      '-TargetRoot',
      $Root
    )

    if ($SkipPostInstall) {
      $Args += '-SkipPostInstall'
    }
    if ($SkipBuild) {
      $Args += '-SkipBuild'
    }
    if ($RunDiagnostics) {
      $Args += '-RunDiagnostics'
    }
    if ($InstallClaudePlugin) {
      $Args += '-InstallClaudePlugin'
    }
    if ($InstallClaudeAgents) {
      $Args += '-InstallClaudeAgents'
    }
    if ($InstallUePluginToEngine) {
      $Args += '-InstallUePluginToEngine'
    }
    if ($EngineRoot) {
      $Args += @('-EngineRoot', $EngineRoot)
    }
    if ($EnginePluginDir) {
      $Args += @('-EnginePluginDir', $EnginePluginDir)
    }
    if ($Force) {
      $Args += '-Force'
    }
    if ($WhatIfPreference) {
      $Args += '-WhatIf'
    }

    Write-UpdateProgressBar -Percent 54 -Status 'Starting downloaded update runner'
    Write-Step 'Starting downloaded update runner'
    Write-Host "Runner update script: $RunnerScript"
    Write-Host "Runner target root:   $Root"
    & powershell @Args
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne 0) {
      Set-UpdateFailureContext -Code 'BH-UPD-RUNNER-FAILED' -Stage 'downloaded_update_runner'
      throw "Downloaded update runner failed with exit code $ExitCode."
    }
  } finally {
    if (Test-Path -LiteralPath $RunnerRoot) {
      Remove-Item -LiteralPath $RunnerRoot -Recurse -Force -WhatIf:$false
    }
  }
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

function Test-ClaudePluginShouldRefresh {
  return ($InstallClaudePlugin -or (Test-ClaudeAgentsInstalled))
}

function Invoke-PostInstallRefresh {
  $InstallScript = Resolve-InstallScriptPath -PackageRoot $Root
  if (-not $InstallScript -or -not (Test-Path -LiteralPath $InstallScript)) {
    Set-UpdateFailureContext -Code 'BH-UPD-POSTINSTALL-FAILED' -Stage 'post_update_install_refresh'
    throw "Missing install script after update: $(Join-Path $Root 'InstallScripts\install.ps1')"
  }

  $Args = @(
    # Keeps machine project-profile.json untouched while install.ps1 still refreshes
    # .blueprinthelper/AgentWorkFlow.md and root AGENTS.md / CLAUDE.md markers.
    '-SkipProjectProfile',
    '-SkipDefaultPreferences'
  )

  if ($SkipBuild) {
    $Args += '-SkipBuild'
  }
  if ($RunDiagnostics) {
    $Args += '-RunDiagnostics'
  }
  if (Test-ClaudePluginShouldRefresh) {
    $Args += '-InstallClaudePlugin'
  } elseif ($InstallClaudeAgents) {
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

  Write-UpdateProgressBar -Percent 86 -Status 'Running post-update install refresh'
  Write-Step 'Running post-update install refresh'
  $LogPath = New-UpdateLogPath -Name 'post-update-install-refresh'
  Set-UpdateFailureContext -Code 'BH-UPD-POSTINSTALL-FAILED' -Stage 'post_update_install_refresh' -LogPath $LogPath
  Write-Host "Post-update install refresh log: $LogPath"
  Write-Host "powershell -NoProfile -ExecutionPolicy Bypass -File `"$InstallScript`" $($Args -join ' ')"
  $Output = & powershell -NoProfile -ExecutionPolicy Bypass -File $InstallScript @Args 2>&1
  $ExitCode = $LASTEXITCODE
  $Output | Tee-Object -FilePath $LogPath
  if ($ExitCode -ne 0) {
    Set-UpdateFailureContext -Code 'BH-UPD-POSTINSTALL-FAILED' -Stage 'post_update_install_refresh' -LogPath $LogPath
    throw "Post-update install refresh failed with exit code $ExitCode. See log: $LogPath"
  }
}

function Restore-Backup {
  param([Parameter(Mandatory = $true)][string]$BackupDir)

  if (-not (Test-Path -LiteralPath $BackupDir)) {
    throw "Backup directory is missing, cannot roll back: $BackupDir"
  }

  Invoke-RobocopyMirror -Source $BackupDir -Destination $Root -Description 'Rolling back to backup'
}

function Invoke-BlueprintHelperUpdateRunner {
  param(
    [Parameter(Mandatory = $true)][object]$ReleaseInfo,
    [Parameter(Mandatory = $true)][string]$PackageRoot,
    [AllowNull()][string]$PackageTempDir = $null
  )

  $CurrentVersion = Get-CurrentBlueprintHelperVersion

  $BackupDir = $null
  $ReplaceStarted = $false
  $DidReplace = $false
  $UpdateVerified = $false
  $ArchivedBackupDir = $null

  try {
    $BackupDir = Get-BackupDirectory -CurrentVersion $CurrentVersion
    Write-UpdateProgressBar -Percent 60 -Status 'Backing up current directory'
    Invoke-RobocopyMirror -Source $Root -Destination $BackupDir -Description 'Backing up current BlueprintHelper directory' | Out-Null

    $ReplaceStarted = $true
    Write-UpdateProgressBar -Percent 72 -Status 'Replacing local files'
    $DidReplace = Invoke-RobocopyMirror -Source $PackageRoot -Destination $Root -Description 'Replacing BlueprintHelper with downloaded release'

    if ($WhatIfPreference) {
      Write-Host ''
      Write-Host 'WhatIf: update was not applied.'
      Complete-UpdateProgressBar -Status 'WhatIf complete'
      return
    }

    if (-not $SkipPostInstall) {
      Invoke-PostInstallRefresh
    } else {
      Write-UpdateProgressBar -Percent 86 -Status 'Skipping post-update install refresh'
    }

    Write-UpdateProgressBar -Percent 92 -Status 'Verifying updated version'
    $UpdatedVersion = Get-CurrentBlueprintHelperVersion
    $UpdateVerified = $true

    Write-UpdateProgressBar -Percent 96 -Status 'Moving backup outside Plugins'
    $ArchivedBackupDir = Move-BackupOutsidePluginScanPath -BackupDir $BackupDir

    Write-Host ''
    Write-Host 'BlueprintHelper update finished.'
    Write-Host "Previous version: v$(Get-NormalizedVersionText -Version $CurrentVersion)"
    Write-Host "Current version:  v$(Get-NormalizedVersionText -Version $UpdatedVersion)"
    Write-Host "Backup archive:   $ArchivedBackupDir"
    Write-Host 'UE engine plugin copy is updated only when -InstallUePluginToEngine is passed.'
    $StatePaths = @(Write-UpdateState -ReleaseInfo $ReleaseInfo -InstalledVersion $UpdatedVersion | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($StatePaths.Count -gt 0) {
      Write-Host 'Applied tag state:'
      foreach ($StatePath in $StatePaths) {
        Write-Host "  $StatePath"
      }
    }
    Complete-UpdateProgressBar -Status 'Update complete'
  } catch {
    $Failure = $_
    if ((-not $UpdateVerified) -and ($ReplaceStarted -or $DidReplace) -and $BackupDir) {
      Write-Host ''
      Write-Host 'Update failed after replacement. Attempting rollback...' -ForegroundColor Yellow
      try {
        Write-UpdateProgressBar -Percent 92 -Status 'Rolling back from backup'
        Restore-Backup -BackupDir $BackupDir
        Write-Host "Rollback restored backup: $BackupDir" -ForegroundColor Yellow
      } catch {
        Write-Host "Rollback failed: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "Backup path: $BackupDir" -ForegroundColor Red
      }
    }
    if ($UpdateVerified -and $BackupDir -and (Test-Path -LiteralPath $BackupDir -PathType Container)) {
      Write-Host ''
      Write-Host 'Update files were installed, but the backup could not be moved out of the Plugins scan path.' -ForegroundColor Red
      Write-Host "Move this backup directory outside Plugins before starting Unreal Editor: $BackupDir" -ForegroundColor Red
    }

    throw $Failure
  } finally {
    if ($PackageTempDir -and (Test-Path -LiteralPath $PackageTempDir)) {
      Write-UpdateProgressBar -Percent $script:UpdateProgressLastPercent -Status 'Cleaning temporary update files'
      Remove-Item -LiteralPath $PackageTempDir -Recurse -Force -WhatIf:$false
    }
  }
}

function Invoke-BlueprintHelperUpdate {
  Write-Host 'BlueprintHelper updater'
  Write-Host "Source root: $Root"

  if (-not [string]::IsNullOrWhiteSpace($RunnerPackageRoot)) {
    Set-UpdateFailureContext -Code 'BH-UPD-RUNNER-FAILED' -Stage 'runner_package_validation'
    if (-not (Test-Path -LiteralPath $RunnerPackageRoot -PathType Container)) {
      throw "Update runner package root is missing: $RunnerPackageRoot"
    }

    $ReleaseInfo = Get-RunnerReleaseInfo
    $ResolvedPackageRoot = (Resolve-Path -LiteralPath $RunnerPackageRoot).Path
    Write-UpdateProgressBar -Percent 56 -Status 'Running downloaded update package'
    return Invoke-BlueprintHelperUpdateRunner -ReleaseInfo $ReleaseInfo -PackageRoot $ResolvedPackageRoot -PackageTempDir $RunnerTempDir
  }

  Write-UpdateProgressBar -Percent 3 -Status 'Reading local version'
  $CurrentVersion = Get-CurrentBlueprintHelperVersion
  Write-UpdateProgressBar -Percent 8 -Status 'Checking latest GitHub release'
  $ReleaseInfo = Get-LatestReleaseInfo
  $UpdateState = Get-UpdateState
  $UpdateDecision = Get-BlueprintHelperUpdateDecision -CurrentVersion $CurrentVersion -ReleaseInfo $ReleaseInfo -UpdateState $UpdateState
  Write-UpdateProgressBar -Percent 16 -Status 'Comparing versions'

  Write-Host ''
  Write-Host "Current version: v$(Get-NormalizedVersionText -Version $CurrentVersion)"
  Write-Host "Latest version:  v$($ReleaseInfo.version)"
  Write-Host "Release tag:     $($ReleaseInfo.tag)"
  if ($UpdateState.applied_release_tag) {
    Write-Host "Applied tag:     $($UpdateState.applied_release_tag)"
  }
  if ($ReleaseInfo.url) {
    Write-Host "Release page:    $($ReleaseInfo.url)"
  }
  Write-Host "Update status:   $($UpdateDecision.status)"

  if (-not $UpdateDecision.should_update) {
    Write-Host ''
    if ($UpdateDecision.status -eq 'local_newer') {
      Write-Host "$($UpdateDecision.message) No update was applied." -ForegroundColor Yellow
      Complete-UpdateProgressBar -Status 'No update applied'
    } else {
      Write-Host 'BlueprintHelper is already up to date.'
      Write-Host $UpdateDecision.message
      Complete-UpdateProgressBar -Status 'Already up to date'
    }
    return
  }

  if ($CheckOnly) {
    Write-Host ''
    Write-Host 'An update is available. Re-run without -CheckOnly to apply it.'
    Write-Host $UpdateDecision.message
    Complete-UpdateProgressBar -Status 'Update available'
    exit 2
  }

  Write-UpdateProgressBar -Percent 20 -Status 'Waiting for update confirmation'
  if (-not (Read-UpdateConfirmation -CurrentVersion $CurrentVersion -ReleaseInfo $ReleaseInfo)) {
    Write-Host ''
    Write-Host 'Update cancelled.'
    Complete-UpdateProgressBar -Status 'Update cancelled'
    return
  }

  $PackageTempDir = $null
  try {
    $Package = Download-AndExpandRelease -ReleaseInfo $ReleaseInfo
    $PackageTempDir = $Package.temp_dir
    Write-UpdateProgressBar -Percent 50 -Status 'Validating downloaded package'
    $PackageRoot = Find-ExtractedPackageRoot -ExtractDir $Package.extract_dir -ReleaseInfo $ReleaseInfo

    if ((-not $SkipBootstrap) -and [string]::IsNullOrWhiteSpace($RunnerPackageRoot)) {
      Start-UpdateRunnerFromPackage -PackageRoot $PackageRoot -ReleaseInfo $ReleaseInfo -PackageTempDir $PackageTempDir
      return
    }

    return Invoke-BlueprintHelperUpdateRunner -ReleaseInfo $ReleaseInfo -PackageRoot $PackageRoot -PackageTempDir $PackageTempDir
  } catch {
    if ($PackageTempDir -and (Test-Path -LiteralPath $PackageTempDir)) {
      Write-UpdateProgressBar -Percent $script:UpdateProgressLastPercent -Status 'Cleaning temporary update files'
      Remove-Item -LiteralPath $PackageTempDir -Recurse -Force -WhatIf:$false
    }
    throw
  }
}

try {
  Invoke-BlueprintHelperUpdate
} catch {
  Stop-UpdateProgressBar -Status 'Update failed'
  Write-Host ''
  Write-Host 'BlueprintHelper update failed.' -ForegroundColor Red
  Write-Host "Failure code: $script:UpdateFailureCode" -ForegroundColor Red
  Write-Host "Failure stage: $script:UpdateFailureStage" -ForegroundColor Red
  if (-not [string]::IsNullOrWhiteSpace($script:UpdateFailureLogPath)) {
    Write-Host "Failure log: $script:UpdateFailureLogPath" -ForegroundColor Red
  }
  if ($_.Exception -and $_.Exception.Message) {
    Write-Host $_.Exception.Message -ForegroundColor Red
  } else {
    Write-Host $_ -ForegroundColor Red
  }
  Write-Host "Failure docs: $(Join-Path $Root 'INSTALL_FAILURE_CODES.md')" -ForegroundColor Yellow
  exit 1
}
