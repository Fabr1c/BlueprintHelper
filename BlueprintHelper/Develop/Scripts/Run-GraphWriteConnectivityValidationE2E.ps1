param(
  [string]$PluginRoot = "D:\UEProjects\Template\Plugins\BlueprintHelper",
  [string]$RunId = ("GraphWriteConnectivityValidationE2E_" + (Get-Date -Format "yyyyMMdd_HHmmss")),
  [string]$AssetPath = "",
  [string]$GraphName = "EG_GraphWriteConnectivityE2E",
  [string]$PositiveEventName = "GWConnectivity_Positive",
  [string]$RuntimeNegativeEventName = "GWConnectivity_RuntimeNegative"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path (Split-Path $PluginRoot -Parent) -Parent
$TaskCore = Join-Path $PluginRoot "AgentFaceService\task-core"
$CliPackage = Join-Path $PluginRoot "AgentFaceService\cli"
$Cli = Join-Path $CliPackage "build\cli\index.js"
$OutRoot = Join-Path (Join-Path $ProjectRoot "Saved\Automation") $RunId
$SpecRoot = Join-Path $OutRoot "specs"
$ResultRoot = Join-Path $OutRoot "results"

if ([string]::IsNullOrWhiteSpace($AssetPath)) {
  $SafeRunId = ($RunId -replace '[^A-Za-z0-9_]', '_')
  $AssetPath = "/Game/BlueprintHelper/ConnectivityValidationE2E/BP_GraphWriteConnectivity_$SafeRunId"
}

New-Item -ItemType Directory -Force -Path $SpecRoot, $ResultRoot | Out-Null

function Write-JsonFile {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)]$Value
  )

  $Json = $Value | ConvertTo-Json -Depth 100
  [System.IO.File]::WriteAllText($Path, $Json, [System.Text.UTF8Encoding]::new($false))
}

function Invoke-NpmBuild {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][string]$Name
  )

  Push-Location $Path
  try {
    npm.cmd run build
    if ($LASTEXITCODE -ne 0) {
      throw "$Name build failed."
    }
  }
  finally {
    Pop-Location
  }
}

function Invoke-CliJson {
  param(
    [Parameter(Mandatory = $true)][string[]]$Arguments,
    [Parameter(Mandatory = $true)][string]$OutputPath
  )

  $Output = & node $Cli @Arguments
  $ExitCode = $LASTEXITCODE
  $Raw = if ($null -eq $Output) { "" } else { $Output -join [Environment]::NewLine }
  [System.IO.File]::WriteAllText($OutputPath, $Raw, [System.Text.UTF8Encoding]::new($false))

  $Parsed = $null
  if (-not [string]::IsNullOrWhiteSpace($Raw)) {
    $Parsed = $Raw | ConvertFrom-Json
  }

  return [pscustomobject]@{
    exit_code = $ExitCode
    output = $Parsed
    raw = $Raw
    output_path = $OutputPath
  }
}

function Invoke-TaskPreview {
  param(
    [Parameter(Mandatory = $true)][string]$StepName,
    [Parameter(Mandatory = $true)]$Spec
  )

  $StepSpecDir = Join-Path $SpecRoot $StepName
  $StepResultDir = Join-Path $ResultRoot $StepName
  New-Item -ItemType Directory -Force -Path $StepSpecDir, $StepResultDir | Out-Null
  $SpecPath = Join-Path $StepSpecDir "task.json"
  Write-JsonFile -Path $SpecPath -Value $Spec

  $Result = Invoke-CliJson `
    -Arguments @("task", "preview", "--file", $SpecPath, "--format", "summary", "--artifact-dir", $StepResultDir) `
    -OutputPath (Join-Path $StepResultDir "preview.json")
  $Result | Add-Member -NotePropertyName "spec" -NotePropertyValue $SpecPath
  $Result | Add-Member -NotePropertyName "result_dir" -NotePropertyValue $StepResultDir
  return $Result
}

function Invoke-TaskExecute {
  param(
    [Parameter(Mandatory = $true)][string]$StepName,
    [Parameter(Mandatory = $true)]$Spec
  )

  $StepSpecDir = Join-Path $SpecRoot $StepName
  $StepResultDir = Join-Path $ResultRoot $StepName
  New-Item -ItemType Directory -Force -Path $StepSpecDir, $StepResultDir | Out-Null
  $SpecPath = Join-Path $StepSpecDir "task.json"
  Write-JsonFile -Path $SpecPath -Value $Spec

  $Result = Invoke-CliJson `
    -Arguments @("task", "execute", "--file", $SpecPath, "--format", "summary", "--artifact-dir", $StepResultDir) `
    -OutputPath (Join-Path $StepResultDir "execute.json")
  $Result | Add-Member -NotePropertyName "spec" -NotePropertyValue $SpecPath
  $Result | Add-Member -NotePropertyName "result_dir" -NotePropertyValue $StepResultDir
  return $Result
}

function Invoke-TaskSpecStep {
  param(
    [Parameter(Mandatory = $true)][string]$StepName,
    [Parameter(Mandatory = $true)]$Spec
  )

  $Preview = Invoke-TaskPreview "${StepName}_preview" $Spec
  if ($Preview.exit_code -ne 0 -or $Preview.output.status -ne "preview_passed") {
    throw "Preview failed for $StepName. See $($Preview.output_path)"
  }

  $Execute = Invoke-TaskExecute "${StepName}_execute" $Spec
  if ($Execute.exit_code -ne 0 -or $Execute.output.status -ne "executed") {
    throw "Execute failed for $StepName. See $($Execute.output_path)"
  }

  return [ordered]@{
    name = $StepName
    spec = $Execute.spec
    preview_status = $Preview.output.status
    execute_status = $Execute.output.status
    result_dir = $Execute.result_dir
  }
}

function Invoke-LogicRead {
  param([Parameter(Mandatory = $true)][string]$StepName)

  $ReadSpecDir = Join-Path $SpecRoot $StepName
  $ReadResultDir = Join-Path $ResultRoot $StepName
  New-Item -ItemType Directory -Force -Path $ReadSpecDir, $ReadResultDir | Out-Null
  $ReadSpecPath = Join-Path $ReadSpecDir "read.json"

  Write-JsonFile -Path $ReadSpecPath -Value ([ordered]@{
    schema = "BlueprintHelper.ReadSpec.v1"
    read_type = "blueprint_logic"
    target = [ordered]@{
      asset_path = $AssetPath
      target_type = "graph"
      target_name = $GraphName
    }
    view = [ordered]@{
      format = "logic_json"
      max_items = 2000
      detail = "full"
    }
  })

  $Result = Invoke-CliJson `
    -Arguments @("blueprinthelper_read_context", "--file", $ReadSpecPath, "--format", "json", "--artifact-dir", $ReadResultDir) `
    -OutputPath (Join-Path $ReadResultDir "readback.json")
  $Result | Add-Member -NotePropertyName "spec" -NotePropertyValue $ReadSpecPath
  $Result | Add-Member -NotePropertyName "result_dir" -NotePropertyValue $ReadResultDir
  return $Result
}

function Test-ContainsAny {
  param(
    [Parameter(Mandatory = $true)][string]$Text,
    [Parameter(Mandatory = $true)][string[]]$Needles
  )

  foreach ($Needle in $Needles) {
    if ($Text.Contains($Needle)) {
      return $true
    }
  }
  return $false
}

function Test-ConnectivityBlocked {
  param([Parameter(Mandatory = $true)]$Result)

  $BlockedStatuses = @("preview_blocked", "execute_failed", "cli_error")
  return $Result.exit_code -ne 0 `
    -and $BlockedStatuses.Contains([string]$Result.output.status) `
    -and (Test-ContainsAny $Result.raw @(
      "graphwrite_connectivity_failed",
      "taskspec_semantic_invalid",
      "unconsumed_pure_data_node",
      "GraphWrite connectivity static preflight failed"
    ))
}

function Measure-RecursiveLogicLinks {
  param([Parameter(Mandatory = $true)]$Value)

  $Counts = [ordered]@{ exec = 0; data = 0 }

  function Visit-LogicValue {
    param($Node)

    if ($null -eq $Node) {
      return
    }
    if ($Node -is [string]) {
      return
    }
    if ($Node -is [System.Collections.IEnumerable] -and -not ($Node -is [System.Management.Automation.PSCustomObject])) {
      foreach ($Item in $Node) {
        Visit-LogicValue $Item
      }
      return
    }

    $TypeProperty = $Node.PSObject.Properties["type"]
    if ($TypeProperty -and $TypeProperty.Value -eq "exec") {
      $Counts.exec = [int]$Counts.exec + 1
    }
    elseif ($TypeProperty -and $TypeProperty.Value -eq "data") {
      $Counts.data = [int]$Counts.data + 1
    }

    foreach ($Property in $Node.PSObject.Properties) {
      Visit-LogicValue $Property.Value
    }
  }

  Visit-LogicValue $Value
  return [pscustomobject]$Counts
}

function Get-LogicGroupEntryNames {
  param($Logic)

  $Names = @()
  foreach ($Group in @($Logic.groups)) {
    if ($Group.entry -and $Group.entry.name) {
      $Names += [string]$Group.entry.name
    }
  }
  return $Names
}

function LitString([string]$Value) {
  return [ordered]@{ kind = "literal"; value_type = "string"; value = $Value }
}

function LitFloat([double]$Value) {
  return [ordered]@{ kind = "literal"; value_type = "float"; value = $Value }
}

function LitInt([int]$Value) {
  return [ordered]@{ kind = "literal"; value_type = "int"; value = $Value }
}

function LitBool([bool]$Value) {
  return [ordered]@{ kind = "literal"; value_type = "bool"; value = $Value }
}

function InRangeIntCall {
  return [ordered]@{
    kind = "call"
    target = "/Script/Engine.KismetMathLibrary:InRange_IntInt"
    args = [ordered]@{
      Value = (LitInt 1)
      Min = (LitInt 0)
      Max = (LitInt 2)
      InclusiveMin = (LitBool $true)
      InclusiveMax = (LitBool $true)
    }
  }
}

function PrintStmt([string]$Text) {
  return [ordered]@{
    kind = "call"
    target = "PrintString"
    args = [ordered]@{
      InString = (LitString $Text)
      Duration = (LitFloat 1.0)
    }
  }
}

function TaskBase([string]$ContextId, [string]$TaskType, [string]$FeatureName, [string]$TargetType) {
  return [ordered]@{
    schema = "BlueprintHelper.TaskSpec.v1"
    context_id = $ContextId
    task_type = $TaskType
    feature_name = $FeatureName
    target = [ordered]@{
      asset_path = $AssetPath
      target_type = $TargetType
    }
    execution_policy = [ordered]@{
      dry_run_mode = "full"
      on_missing_capability = "stop_and_report"
      review_baseline_dirty_asset_policy = "save_before_archive"
    }
    validation = [ordered]@{
      should_compile = $true
      should_save = $true
    }
  }
}

Invoke-NpmBuild -Path $TaskCore -Name "task-core"
Invoke-NpmBuild -Path $CliPackage -Name "cli"

$ExecutedSteps = @()

$StaticNegative = TaskBase "ctx_gw_connectivity_static_negative" "edit_blueprint_graph" "ConnectivityStaticNegative" "blueprint"
$StaticNegative.scope_policy = [ordered]@{
  graph_name = $GraphName
  allow_modify_user_nodes = $false
}
$StaticNegative.behavior = [ordered]@{
  graph_strategy = "append_new_owned_graph"
  entries = @(
    [ordered]@{
      entry_type = "custom_event"
      name = "GWConnectivity_StaticNegative"
      body = [ordered]@{
        schema = "BlueprintLogicSpec.v1"
        statements = @(
          [ordered]@{
            kind = "let"
            name = "UnusedStaticBool"
            value = (InRangeIntCall)
          }
        )
      }
    }
  )
}
$StaticPreview = Invoke-TaskPreview "00_static_preflight_negative" $StaticNegative
$StaticPreflightBlocked = Test-ConnectivityBlocked $StaticPreview

$CreateAsset = TaskBase "ctx_gw_connectivity_create_asset" "create_asset" "ConnectivityValidationAsset" "asset"
$CreateAsset.behavior = [ordered]@{
  asset_strategy = "ensure_asset"
  asset = [ordered]@{
    asset_type = "blueprint_class"
    parent_class = "Actor"
    collision_policy = "fail_if_exists"
  }
}
$ExecutedSteps += Invoke-TaskSpecStep "01_create_asset" $CreateAsset

$RuntimeNegative = TaskBase "ctx_gw_connectivity_runtime_negative" "edit_blueprint_graph" "ConnectivityRuntimeNegative" "blueprint"
$RuntimeNegative.scope_policy = [ordered]@{
  graph_name = $GraphName
  allow_modify_user_nodes = $false
}
$UnconsumedPureCall = InRangeIntCall
$UnconsumedPureCall.value_type = "bool"
$UnconsumedPureCall.result_symbol = "UnusedRuntimeBool"
$RuntimeNegative.behavior = [ordered]@{
  graph_strategy = "append_new_owned_graph"
  entries = @(
    [ordered]@{
      entry_type = "custom_event"
      name = $RuntimeNegativeEventName
      body = [ordered]@{
        schema = "BlueprintLogicSpec.v1"
        statements = @($UnconsumedPureCall)
      }
    }
  )
}
$RuntimeNegativePreview = Invoke-TaskPreview "02_runtime_negative_preview" $RuntimeNegative
$RuntimeNegativeExecute = Invoke-TaskExecute "03_runtime_negative_execute" $RuntimeNegative
$NegativePreviewBlocked = Test-ConnectivityBlocked $RuntimeNegativePreview
$NegativeExecuteBlocked = Test-ConnectivityBlocked $RuntimeNegativeExecute

$Positive = TaskBase "ctx_gw_connectivity_positive" "edit_blueprint_graph" "ConnectivityPositive" "blueprint"
$Positive.scope_policy = [ordered]@{
  graph_name = $GraphName
  allow_modify_user_nodes = $false
}
$Positive.behavior = [ordered]@{
  graph_strategy = "append_new_owned_graph"
  entries = @(
    [ordered]@{
      entry_type = "custom_event"
      name = $PositiveEventName
      body = [ordered]@{
        schema = "BlueprintLogicSpec.v1"
        statements = @(
          [ordered]@{
            kind = "control"
            control = "branch"
            condition = (InRangeIntCall)
            then = @((PrintStmt "connectivity positive then"))
            "else" = @((PrintStmt "connectivity positive else"))
          }
        )
      }
    }
  )
}
$ExecutedSteps += Invoke-TaskSpecStep "04_positive_append" $Positive

$Readback = Invoke-LogicRead "05_positive_readback"
if ($Readback.exit_code -ne 0 -or $Readback.output.status -ne "completed") {
  throw "Readback failed. See $($Readback.output_path)"
}

$Logic = $Readback.output.tool_result.data.payload.logic
$PayloadStats = $Readback.output.tool_result.data.payload.stats
$LinkCounts = Measure-RecursiveLogicLinks $Logic
$EntryNames = Get-LogicGroupEntryNames $Logic
$NegativeResidueAbsent = -not $EntryNames.Contains($RuntimeNegativeEventName)
$PositiveReadbackExecLinks = if ($PayloadStats -and $null -ne $PayloadStats.exec_links) {
  [int]$PayloadStats.exec_links
} else {
  [int]$LinkCounts.exec
}
$PositiveReadbackDataLinks = if ($PayloadStats -and $null -ne $PayloadStats.data_links) {
  [int]$PayloadStats.data_links
} else {
  [int]$LinkCounts.data
}
$PositiveReadbackOk = $PositiveReadbackExecLinks -ge 1 -and $PositiveReadbackDataLinks -ge 1
$PositiveExecutePassed = @($ExecutedSteps | Where-Object { $_.name -eq "04_positive_append" }).Count -eq 1
$RejectCleanupVerified = $NegativeResidueAbsent

$Ok = $StaticPreflightBlocked `
  -and $NegativePreviewBlocked `
  -and $NegativeExecuteBlocked `
  -and $PositiveExecutePassed `
  -and $PositiveReadbackOk `
  -and $RejectCleanupVerified

$Summary = [ordered]@{
  schema = "BlueprintHelper.GraphWriteConnectivityValidationE2E.v1"
  run_id = $RunId
  ok = $Ok
  asset_path = $AssetPath
  graph_name = $GraphName
  static_preflight_blocked = $StaticPreflightBlocked
  negative_preview_blocked = $NegativePreviewBlocked
  negative_execute_blocked = $NegativeExecuteBlocked
  positive_execute_passed = $PositiveExecutePassed
  positive_readback_exec_links = $PositiveReadbackExecLinks
  positive_readback_data_links = $PositiveReadbackDataLinks
  reject_cleanup_verified = $RejectCleanupVerified
  reject_cleanup_mode = "blocked_preview_execute_no_review_record"
  negative_residue_absent = $NegativeResidueAbsent
  entry_names = $EntryNames
  steps = [ordered]@{
    static_preview = [ordered]@{
      exit_code = $StaticPreview.exit_code
      status = $StaticPreview.output.status
      output = $StaticPreview.output_path
    }
    runtime_negative_preview = [ordered]@{
      exit_code = $RuntimeNegativePreview.exit_code
      status = $RuntimeNegativePreview.output.status
      error_code = $RuntimeNegativePreview.output.error_code
      violations = $RuntimeNegativePreview.output.violations
      output = $RuntimeNegativePreview.output_path
    }
    runtime_negative_execute = [ordered]@{
      exit_code = $RuntimeNegativeExecute.exit_code
      status = $RuntimeNegativeExecute.output.status
      error_code = $RuntimeNegativeExecute.output.error_code
      violations = $RuntimeNegativeExecute.output.violations
      output = $RuntimeNegativeExecute.output_path
    }
    positive = $ExecutedSteps
    readback = [ordered]@{
      exit_code = $Readback.exit_code
      status = $Readback.output.status
      output = $Readback.output_path
    }
  }
  run_root = $OutRoot
}

Write-JsonFile -Path (Join-Path $OutRoot "summary.json") -Value $Summary

if ($Ok -ne $true) {
  throw "GraphWrite connectivity validation E2E failed. See $(Join-Path $OutRoot 'summary.json')"
}

Write-Output "GraphWrite connectivity validation E2E finished."
Write-Output "Run root: $OutRoot"
Write-Output "Asset path: $AssetPath"
Write-Output "Exec links: $PositiveReadbackExecLinks"
Write-Output "Data links: $PositiveReadbackDataLinks"
Write-Output "Summary: $(Join-Path $OutRoot 'summary.json')"
