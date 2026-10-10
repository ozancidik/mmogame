# Native gold telemetry dynamic delegate regression; disposable authority NullRHI world.
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'gold-metrics')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_gold_metrics_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestGoldMetrics,quit"')
$process = $null
$timedOut = $false
$nativeExit = $null
try {
    $process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(60000)) {
        $timedOut = $true
        if (-not $process.HasExited) { $process.Kill() }
        if ($process.WaitForExit(5000)) { $nativeExit = $process.ExitCode }
    } else { $nativeExit = $process.ExitCode }
} finally {
    if ($null -ne $process) {
        try {
            if (-not $process.HasExited) {
                $process.Kill()
                if (-not $process.WaitForExit(5000)) { throw 'Owned UE process did not stop' }
            }
        } finally { $process.Dispose() }
    }
}
Write-Output "[MMO_GOLD_METRIC_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_GOLD_METRIC_CASE\] (PASS|FAIL) case=')
$summaries = @($lines | Select-String '\[MMO_GOLD_METRIC_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_GOLD_METRIC_TEST\] PASS checks=60\b reason=')
$errors = @($lines | Select-String '\[MMO_GOLD_METRIC_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation')
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
if ($timedOut) { Write-Output 'FAIL watchdog_60s'; exit 2 }
if ($nativeExit -eq 0 -and $cases.Count -eq 60 -and $summaries.Count -eq 1 -and $passes.Count -eq 1 -and $errors.Count -eq 0) {
    & 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' (Join-Path $PSScriptRoot 'ValidateGoldMetricLog.py') $log
    if ($LASTEXITCODE -ne 0) { exit 1 }
    Write-Output '[MMO_GOLD_METRIC_RUNNER] PASS checks=60 cases=15 metrics=23'; exit 0
}
Write-Output "[MMO_GOLD_METRIC_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count)"
exit 1
