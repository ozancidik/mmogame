# Yukseltme olcum satiri ([MMOMETRIC] ev=upgrade) native testi; tek kullanimlik NullRHI dunyasi, diske yazmaz.
# RunSystemTests.ps1 / RunAllChecks.ps1 adimi DEGILDIR (elle calistirilir).
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'upgrade-metric')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_upgrade_metric_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestUpgradeMetrics,quit"')
$process = $null
$timedOut = $false
$nativeExit = $null
try {
    $process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(90000)) {
        $timedOut = $true
        if (-not $process.HasExited) { $process.Kill() }
        if ($process.WaitForExit(5000)) { $nativeExit = $process.ExitCode }
    } else { $nativeExit = $process.ExitCode }
} finally {
    # Yalniz bu betigin baslattigi sureci oldurur.
    if ($null -ne $process) {
        try {
            if (-not $process.HasExited) {
                $process.Kill()
                if (-not $process.WaitForExit(5000)) { throw 'Owned UE process did not stop' }
            }
        } finally { $process.Dispose() }
    }
}
Write-Output "[MMO_UPGRADE_METRIC_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_UPGRADE_METRIC_CASE\] (PASS|FAIL) case=\S+ check=\S+')
$summaries = @($lines | Select-String '\[MMO_UPGRADE_METRIC_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_UPGRADE_METRIC_TEST\] PASS checks=(\d+) reason=')
$errors = @($lines | Select-String '\[MMO_UPGRADE_METRIC_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation')
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
# 3 sonuc (success/failure/burn) x 2 mod (duz/ic ice) x 10 kontrol + 6 ret kapisi = 66.
$reported = 0
if ($passes.Count -eq 1 -and $passes[0].Line -match 'checks=(\d+)') { $reported = [int]$Matches[1] }
$unique = @($cases | ForEach-Object { if ($_.Line -match 'case=(\S+) check=(\S+)') { $Matches[1] + '/' + $Matches[2] } } | Sort-Object -Unique)
$expected = 66
$consistent = ($reported -eq $expected) -and ($cases.Count -eq $reported) -and ($unique.Count -eq $reported)
Write-Output "[MMO_UPGRADE_METRIC_RUNNER] reported=$reported case_lines=$($cases.Count) unique_labels=$($unique.Count) consistent=$consistent"
if ($timedOut) { Write-Output 'FAIL watchdog_90s'; exit 2 }
if ($nativeExit -eq 0 -and $consistent -and $summaries.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output "[MMO_UPGRADE_METRIC_RUNNER] PASS checks=$reported"; exit 0
}
Write-Output "[MMO_UPGRADE_METRIC_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count)"
exit 1
