$ErrorActionPreference = 'Stop'

$Root = 'D:\UEProjects\Template\Plugins\BlueprintHelper'
$Cli = Join-Path $Root 'AgentFaceService\cli\build\cli\index.js'
$ArtifactRoot = Join-Path $Root 'BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524'
$ResultRoot = Join-Path $ArtifactRoot 'Results'

New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$PositiveSpecs = @(
  '00_create_smoke_actor.json',
  '01_prepare_smoke_fixture.json',
  '02_function_field_graph.json',
  '03_event_delegate_graph.json',
  '04_generic_graph.json'
)

$DiagnosticSpecs = @(
  '05_generic_expected_diagnostics.json'
)

function Invoke-TaskCommand {
  param(
    [Parameter(Mandatory = $true)][string]$Mode,
    [Parameter(Mandatory = $true)][string]$SpecName,
    [Parameter(Mandatory = $true)][string]$OutputPath
  )

  $SpecPath = Join-Path $ArtifactRoot $SpecName
  $Output = & node $Cli task $Mode --file $SpecPath --format full
  [System.IO.File]::WriteAllText($OutputPath, ($Output -join [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
  $Json = ($Output -join [Environment]::NewLine) | ConvertFrom-Json
  return $Json
}

function Test-PreviewPassed {
  param([Parameter(Mandatory = $true)]$Result)
  return $Result.ok -eq $true -and $Result.status -eq 'preview_passed'
}

function Test-ExecutePassed {
  param([Parameter(Mandatory = $true)]$Result)
  return $Result.ok -eq $true -and $Result.status -eq 'executed'
}

foreach ($SpecName in $PositiveSpecs) {
  $PreviewPath = Join-Path $ResultRoot ($SpecName + '.preview.json')
  $Preview = Invoke-TaskCommand -Mode 'preview' -SpecName $SpecName -OutputPath $PreviewPath
  if (-not (Test-PreviewPassed -Result $Preview)) {
    throw "Preview failed for $SpecName. See $PreviewPath"
  }

  $ExecutePath = Join-Path $ResultRoot ($SpecName + '.execute.json')
  $Execute = Invoke-TaskCommand -Mode 'execute' -SpecName $SpecName -OutputPath $ExecutePath
  if (-not (Test-ExecutePassed -Result $Execute)) {
    throw "Execute failed for $SpecName. See $ExecutePath"
  }
}

foreach ($SpecName in $DiagnosticSpecs) {
  $PreviewPath = Join-Path $ResultRoot ($SpecName + '.preview.json')
  $Preview = Invoke-TaskCommand -Mode 'preview' -SpecName $SpecName -OutputPath $PreviewPath
  if (Test-PreviewPassed -Result $Preview) {
    throw "Expected diagnostic preview did not block for $SpecName. See $PreviewPath"
  }
  $DiagnosticText = (($Preview | ConvertTo-Json -Depth 80) -join [Environment]::NewLine)
  if ($DiagnosticText -notmatch 'needs_more_semantic_context|type_promotion|timer_delegate_node|schedule_operation|spawner evidence') {
    throw "Expected diagnostic preview blocked with the wrong reason for $SpecName. See $PreviewPath"
  }
}

$ReadSpecPath = Join-Path $ArtifactRoot 'read_logic_flow.json'
$ReadSpec = @{
  schema = 'BlueprintHelper.ReadSpec.v1'
  read_type = 'blueprint_logic'
  target = @{
    asset_path = '/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524_FixRun02'
    target_type = 'custom_event'
    target_name = 'BH_GenericSmoke_20260524'
  }
  view = @{
    format = 'logic_flow'
  }
} | ConvertTo-Json -Depth 80
[System.IO.File]::WriteAllText($ReadSpecPath, $ReadSpec, [System.Text.UTF8Encoding]::new($false))

$ReadOutput = & node $Cli blueprinthelper_read_context --file $ReadSpecPath --format full
$ReadOutputPath = Join-Path $ResultRoot 'read_logic_flow.json'
[System.IO.File]::WriteAllText($ReadOutputPath, ($ReadOutput -join [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))

$AutomationReport = 'D:\UEProjects\Template\Saved\Automation\GraphWrite_FourClusterE2ESmoke_20260524_001'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath=$AutomationReport
if ($LASTEXITCODE -ne 0) {
  throw "BlueprintHelper.GraphWrite automation failed. See $AutomationReport"
}

& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
if ($LASTEXITCODE -ne 0) {
  throw "UE 5.6 build failed."
}

Write-Output "Four-cluster E2E smoke finished. Results: $ResultRoot"
