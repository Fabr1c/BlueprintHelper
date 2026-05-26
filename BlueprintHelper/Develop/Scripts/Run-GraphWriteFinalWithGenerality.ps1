param(
  [string]$PluginRoot = "D:\UEProjects\Template\Plugins\BlueprintHelper",
  [string]$ProjectPath = "D:\UEProjects\Template\Template.uproject"
)

$ErrorActionPreference = "Stop"

$Preflight = Join-Path $PluginRoot "BlueprintHelper\Develop\Scripts\Run-GraphWriteGeneralityPreflight.ps1"
& $Preflight -PluginRoot $PluginRoot

$SummaryPath = Join-Path $PluginRoot "BlueprintHelper\Develop\Report\BlueprintHelper_GraphWrite_GeneralityPreflight_LatestSummary.json"
$Summary = Get-Content -Raw -Path $SummaryPath | ConvertFrom-Json
if ($Summary.allOperationsPassed -ne $true) {
  throw "GraphWrite final test blocked: generality preflight failed. See $SummaryPath"
}

& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $ProjectPath -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.Capability80;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_Final_WithGenerality'
if ($LASTEXITCODE -ne 0) {
  throw "BlueprintHelper.GraphWrite.Capability80 failed."
}
