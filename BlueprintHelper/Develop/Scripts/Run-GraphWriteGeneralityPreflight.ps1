param(
  [string]$PluginRoot = "D:\UEProjects\Template\Plugins\BlueprintHelper",
  [string]$AssetPath = "",
  [string]$GraphName = "EG_GraphWriteGenerality",
  [string]$RunId = ("GraphWriteGenerality_E2E_" + (Get-Date -Format "yyyyMMdd_HHmmss")),
  [string]$ReportDate = (Get-Date -Format "yyyyMMdd"),
  [string[]]$Operations = @(),
  [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$TaskCore = Join-Path $PluginRoot "AgentFaceService\task-core"
$CliPackage = Join-Path $PluginRoot "AgentFaceService\cli"
$Cli = Join-Path $CliPackage "build\cli\index.js"
$OutRoot = Join-Path "D:\UEProjects\Template\Saved\Automation" $RunId
$SpecRoot = Join-Path $OutRoot "specs"
$ResultRoot = Join-Path $OutRoot "results"
$ReportRoot = Join-Path $PluginRoot "BlueprintHelper\Develop\Report"

if ([string]::IsNullOrWhiteSpace($AssetPath)) {
  $SafeRunId = ($RunId -replace '[^A-Za-z0-9_]', '_')
  $AssetPath = "/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_$SafeRunId"
}

New-Item -ItemType Directory -Force -Path $SpecRoot, $ResultRoot, $ReportRoot | Out-Null

if (-not $SkipBuild) {
  Push-Location $TaskCore
  npm.cmd run build
  if ($LASTEXITCODE -ne 0) { throw "task-core build failed." }
  Pop-Location

  Push-Location $CliPackage
  npm.cmd run build
  if ($LASTEXITCODE -ne 0) { throw "cli build failed." }
  Pop-Location
}

$SpecArgs = @("--asset", $AssetPath, "--graph", $GraphName, "--out", $SpecRoot)
if ($Operations.Count -gt 0) {
  $SpecArgs += @("--operations", ($Operations -join ","))
}
node (Join-Path $TaskCore "build\task\testing\write-graphwrite-generality-specs.js") @SpecArgs
if ($LASTEXITCODE -ne 0) { throw "GraphWrite generality spec generation failed." }
if ($Operations.Count -gt 0 -and -not (Get-ChildItem -Path $SpecRoot -Directory -ErrorAction SilentlyContinue)) {
  throw "GraphWrite generality spec generation matched no operations: $($Operations -join ', ')"
}

$OperationSelection = @{}
if ($Operations.Count -gt 0) {
  foreach ($Operation in ($Operations -join "," -split ",")) {
    $TrimmedOperation = $Operation.Trim()
    if ([string]::IsNullOrWhiteSpace($TrimmedOperation)) {
      continue
    }

    $OperationKey = ($TrimmedOperation -replace '[^A-Za-z0-9]+', '_').Trim('_')
    if (-not [string]::IsNullOrWhiteSpace($OperationKey)) {
      $OperationSelection[$OperationKey] = $true
    }
    $OperationSelection[$TrimmedOperation] = $true
  }
}

function Invoke-CliJson {
  param(
    [Parameter(Mandatory = $true)][string[]]$Arguments,
    [Parameter(Mandatory = $true)][string]$OutputPath
  )

  $Output = & node $Cli @Arguments
  [System.IO.File]::WriteAllText($OutputPath, ($Output -join [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
  return ($Output -join [Environment]::NewLine) | ConvertFrom-Json
}

$OperationDirs = Get-ChildItem -Path $SpecRoot -Directory | Sort-Object Name
if ($Operations.Count -gt 0) {
  $OperationDirs = $OperationDirs | Where-Object { $OperationSelection.ContainsKey($_.Name) }
}
if (-not $OperationDirs) {
  throw "GraphWrite generality execution matched no operation directories: $($Operations -join ', ')"
}

$OperationDirs |
  ForEach-Object {
    $OperationDir = $_
    $OperationResultDir = Join-Path $ResultRoot $OperationDir.Name
    New-Item -ItemType Directory -Force -Path $OperationResultDir | Out-Null

    $SetupResultDir = Join-Path $OperationResultDir "setup"
    New-Item -ItemType Directory -Force -Path $SetupResultDir | Out-Null
    $SetupOk = $true
    Get-ChildItem -Path (Join-Path $OperationDir.FullName "setup") -Filter "*.json" | Sort-Object Name | ForEach-Object {
      if ($SetupOk -ne $true) { return }

      $PreviewPath = Join-Path $SetupResultDir ($_.BaseName + "_preview.json")
      $Preview = Invoke-CliJson -Arguments @("task", "preview", "--file", $_.FullName, "--format", "json", "--artifact-dir", $SetupResultDir) -OutputPath $PreviewPath
      if ($Preview.ok -ne $true -or $Preview.status -ne "preview_passed") {
        $SetupOk = $false
        return
      }

      $ExecutePath = Join-Path $SetupResultDir ($_.BaseName + "_execute.json")
      $Execute = Invoke-CliJson -Arguments @("task", "execute", "--file", $_.FullName, "--format", "json", "--artifact-dir", $SetupResultDir) -OutputPath $ExecutePath
      if ($Execute.ok -ne $true -or $Execute.status -ne "executed") {
        $SetupOk = $false
        return
      }
    }

    $ExpectedPath = Join-Path $OperationDir.FullName "expected_variants.json"
    $Expected = Get-Content -Raw -Path $ExpectedPath | ConvertFrom-Json
    $OperationAssetPath = if ($Expected.assetPath) { $Expected.assetPath } else { $AssetPath }
    $OperationGraphName = if ($Expected.graphName) { $Expected.graphName } else { $GraphName }

    [System.IO.File]::WriteAllText(
      (Join-Path $SetupResultDir "setup_summary.json"),
      (@{ ok = $SetupOk; asset_path = $OperationAssetPath; graph_name = $OperationGraphName; run_id = $RunId } | ConvertTo-Json -Depth 20),
      [System.Text.UTF8Encoding]::new($false))

    $GraphWriteSpecPath = Join-Path $OperationDir.FullName "graph_write.json"
    node (Join-Path $TaskCore "build\task\testing\patch-graphwrite-generality-projected-evidence.js") --spec $GraphWriteSpecPath --asset $OperationAssetPath --graph "EventGraph"
    if ($LASTEXITCODE -ne 0) { throw "GraphWrite projected evidence patch failed for $($OperationDir.Name)." }

    $PreviewFile = Join-Path $OperationResultDir "graph_write_preview.json"
    Invoke-CliJson -Arguments @("task", "preview", "--file", $GraphWriteSpecPath, "--format", "json", "--artifact-dir", $OperationResultDir) -OutputPath $PreviewFile | Out-Null

    $ExecuteFile = Join-Path $OperationResultDir "graph_write_execute.json"
    Invoke-CliJson -Arguments @("task", "execute", "--file", $GraphWriteSpecPath, "--format", "json", "--artifact-dir", $OperationResultDir) -OutputPath $ExecuteFile | Out-Null

    $ReadSpecPath = Join-Path $OperationResultDir "readback_spec.json"
    $ReadSpec = @{
      schema = "BlueprintHelper.ReadSpec.v1"
      read_type = "blueprint_logic"
      target = @{
        asset_path = $OperationAssetPath
        target_type = "graph"
        target_name = $OperationGraphName
      }
      view = @{
        format = "logic_json"
        max_items = 1000
        detail = "full"
      }
    } | ConvertTo-Json -Depth 80
    [System.IO.File]::WriteAllText($ReadSpecPath, $ReadSpec, [System.Text.UTF8Encoding]::new($false))
    Invoke-CliJson -Arguments @("blueprinthelper_read_context", "--file", $ReadSpecPath, "--format", "json", "--artifact-dir", $OperationResultDir) -OutputPath (Join-Path $OperationResultDir "readback.json") | Out-Null
  }

node (Join-Path $TaskCore "build\task\testing\write-graphwrite-generality-report.js") --run $OutRoot --report $ReportRoot --date $ReportDate
if ($LASTEXITCODE -ne 0) { throw "GraphWrite generality report generation failed." }

$SummaryPath = Join-Path $ReportRoot "BlueprintHelper_GraphWrite_GeneralityPreflight_LatestSummary.json"
Write-Output "GraphWrite generality E2E finished."
Write-Output "Run root: $OutRoot"
Write-Output "Asset path: $AssetPath"
Write-Output "Summary: $SummaryPath"
