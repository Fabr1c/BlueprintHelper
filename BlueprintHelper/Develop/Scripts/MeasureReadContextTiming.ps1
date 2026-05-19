param(
    [Parameter(Mandatory = $true)]
    [string]$SpecDir,

    [int]$Iterations = 5,
    [int]$Warmup = 1,

    [string]$CliPath = ".\AgentFaceService\cli\build\cli\index.js",
    [string]$OutputDir = ".\.tmp\read_timing"
)

$ErrorActionPreference = "Stop"

function Resolve-RepositoryPath([string]$PathValue) {
    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return $PathValue
    }
    return (Resolve-Path -LiteralPath (Join-Path (Get-Location) $PathValue)).Path
}

function Invoke-ReadContextCli([string]$ResolvedCliPath, [string]$SpecFile) {
    $extension = [System.IO.Path]::GetExtension($ResolvedCliPath).ToLowerInvariant()
    if ($extension -eq ".js") {
        return & node $ResolvedCliPath blueprinthelper_read_context --file $SpecFile --develop --format full 2>&1
    }
    return & $ResolvedCliPath blueprinthelper_read_context --file $SpecFile --develop --format full 2>&1
}

function Get-TimingStageDuration([object]$Timing, [string]$StageName) {
    if ($null -eq $Timing -or $null -eq $Timing.stages) {
        return $null
    }
    $stage = @($Timing.stages | Where-Object { $_.name -eq $StageName } | Select-Object -First 1)
    if ($stage.Count -eq 0) {
        return $null
    }
    return $stage[0].duration_ms
}

function Get-NestedTimingTotal([object]$Timing, [string]$Prefix) {
    if ($null -eq $Timing -or $null -eq $Timing.nested) {
        return $null
    }
    $nested = @($Timing.nested | Where-Object { "$($_.name)".StartsWith($Prefix) } | Select-Object -First 1)
    if ($nested.Count -eq 0) {
        return $null
    }
    return $nested[0].total_ms
}

function Get-PayloadBytes([object]$Timing, [string]$StageName) {
    if ($null -eq $Timing -or $null -eq $Timing.stages) {
        return $null
    }
    $stage = @($Timing.stages | Where-Object { $_.name -eq $StageName } | Select-Object -First 1)
    if ($stage.Count -eq 0) {
        return $null
    }
    return $stage[0].bytes
}

function Get-Percentile([double[]]$Values, [double]$Percentile) {
    if ($Values.Count -eq 0) {
        return $null
    }
    $sorted = @($Values | Sort-Object)
    $index = [Math]::Ceiling(($Percentile / 100.0) * $sorted.Count) - 1
    $index = [Math]::Max(0, [Math]::Min($index, $sorted.Count - 1))
    return [Math]::Round($sorted[$index], 3)
}

$resolvedSpecDir = Resolve-RepositoryPath $SpecDir
$resolvedCliPath = Resolve-RepositoryPath $CliPath
$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir
} else {
    Join-Path (Get-Location) $OutputDir
}

New-Item -ItemType Directory -Force $resolvedOutputDir | Out-Null

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$runDir = Join-Path $resolvedOutputDir "read_timing_$stamp"
New-Item -ItemType Directory -Force $runDir | Out-Null

$specFiles = @(Get-ChildItem -LiteralPath $resolvedSpecDir -Filter "*.json" | Sort-Object Name)
$results = @()

foreach ($spec in $specFiles) {
    for ($warmupIndex = 0; $warmupIndex -lt $Warmup; $warmupIndex++) {
        [void](Invoke-ReadContextCli $resolvedCliPath $spec.FullName)
    }

    for ($iteration = 1; $iteration -le $Iterations; $iteration++) {
        $startedAt = Get-Date
        $rawLines = Invoke-ReadContextCli $resolvedCliPath $spec.FullName
        $finishedAt = Get-Date
        $rawText = ($rawLines -join "`n").Trim()
        $samplePath = Join-Path $runDir "$($spec.BaseName).$iteration.json"
        $rawText | Set-Content -LiteralPath $samplePath -Encoding UTF8

        $parsed = $null
        $status = "failed"
        $timing = $null
        try {
            $parsed = $rawText | ConvertFrom-Json
            $toolResult = $parsed.tool_result
            if ($parsed.ok -eq $true -and ($null -eq $toolResult -or $toolResult.ok -eq $true)) {
                $status = "success"
            }
            if ($null -ne $toolResult) {
                $timing = $toolResult.data.timing
            } else {
                $timing = $parsed.data.timing
            }
        } catch {
            $status = "parse_failed"
        }

        $results += [pscustomobject]@{
            spec = $spec.Name
            iteration = $iteration
            status = $status
            wall_ms = [Math]::Round(($finishedAt - $startedAt).TotalMilliseconds, 3)
            cli_total_ms = if ($null -ne $timing) { $timing.total_ms } else { $null }
            bridge_send_receive_ms = Get-TimingStageDuration $timing "read_context.bridge_send_receive"
            post_process_ms = Get-TimingStageDuration $timing "read_context.post_process_payload"
            compact_ms = Get-TimingStageDuration $timing "read_context.compact_payload"
            filter_ms = Get-TimingStageDuration $timing "read_context.filter_payload"
            logic_flow_ms = Get-TimingStageDuration $timing "read_context.logic_flow_build_payload"
            ue_total_ms = Get-NestedTimingTotal $timing "ue."
            raw_payload_bytes = Get-PayloadBytes $timing "read_context.ue_raw_payload_bytes"
            final_payload_bytes = Get-PayloadBytes $timing "read_context.post_processed_payload_bytes"
            output_file = $samplePath
        }
    }
}

$summary = foreach ($group in ($results | Group-Object spec)) {
    $success = @($group.Group | Where-Object { $_.status -eq "success" })
    $wallValues = @($success | ForEach-Object { [double]$_.wall_ms })
    [pscustomobject]@{
        spec = $group.Name
        success_count = $success.Count
        failure_count = $group.Count - $success.Count
        median_wall_ms = Get-Percentile $wallValues 50
        p95_wall_ms = Get-Percentile $wallValues 95
        max_wall_ms = if ($wallValues.Count -gt 0) { [Math]::Round(($wallValues | Measure-Object -Maximum).Maximum, 3) } else { $null }
        slowest_success = @($success | Sort-Object wall_ms -Descending | Select-Object -First 1)
    }
}

$results | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath (Join-Path $runDir "samples.json") -Encoding UTF8
$summary | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath (Join-Path $runDir "summary.json") -Encoding UTF8

[pscustomobject]@{
    run_dir = $runDir
    spec_dir = $resolvedSpecDir
    iterations = $Iterations
    warmup = $Warmup
    sample_count = $results.Count
    summary = $summary
} | ConvertTo-Json -Depth 100
