# Native same-frame authority XP fixture. Label distinguishes old-algorithm and fixed runs.
param(
    [ValidatePattern('^[A-Za-z0-9_.-]+$')]
    [string]$Label = 'reentrant-experience'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = [System.IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = [System.IO.Path]::GetFullPath((Join-Path $env:TEMP 'mmo_reentrant_experience_test.log'))
$expectedChecks = 206 # MMOReentrantExperienceSelfTest.cpp: 15 * 12 + 26.
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'

if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Project missing: $project" }
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Unreal commandlet missing: $exe" }
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestReentrantExperience,quit"')

$process = $null
$timedOut = $false
$nativeExit = $null
try {
    # This runner owns only this hidden process; never find/kill other UE processes.
    $process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(60000)) {
        $timedOut = $true
        if (-not $process.HasExited) { $process.Kill() }
        $stopped = $process.WaitForExit(5000)
        if ($stopped) { $nativeExit = $process.ExitCode }
    } else {
        $nativeExit = $process.ExitCode
    }
} finally {
    if ($null -ne $process) {
        try {
            if (-not $process.HasExited) {
                $process.Kill()
                $null = $process.WaitForExit(5000)
            }
        } finally {
            $process.Dispose()
        }
    }
}

Write-Output "[MMO_REENTRANT_EXPERIENCE_RUNNER] label=$Label exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) {
    Write-Output "[MMO_REENTRANT_EXPERIENCE_RUNNER] FAIL label=$Label reason=log_missing"
    if ($timedOut) { exit 2 }
    exit 1
}

$caseData = @(Select-String -LiteralPath $log -Pattern '\[MMO_REENTRANT_EXPERIENCE_CASE\] (PASS|FAIL) case=')
$summaries = @(Select-String -LiteralPath $log -Pattern '\[MMO_REENTRANT_EXPERIENCE_TEST\] (PASS|FAIL) checks=')
$passes = @(Select-String -LiteralPath $log -Pattern "\[MMO_REENTRANT_EXPERIENCE_TEST\] PASS checks=$expectedChecks\b reason=")
$errors = @(Select-String -LiteralPath $log -Pattern '\[MMO_REENTRANT_EXPERIENCE_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation')

# Always print every observed native expected/actual pair, including on old baseline failures.
$caseData | ForEach-Object { Write-Output $_.Line }
$summaries | ForEach-Object { Write-Output $_.Line }
if ($errors.Count -gt 0) { $errors | ForEach-Object { Write-Output $_.Line } }
if ($timedOut) {
    Write-Output "[MMO_REENTRANT_EXPERIENCE_RUNNER] FAIL label=$Label reason=watchdog_60s observed_checks=$($caseData.Count)"
    exit 2
}
if ($nativeExit -eq 0 -and $passes.Count -eq 1 -and $summaries.Count -eq 1 -and
    $caseData.Count -eq $expectedChecks -and $errors.Count -eq 0) {
    Write-Output "[MMO_REENTRANT_EXPERIENCE_RUNNER] PASS label=$Label checks=$expectedChecks states=15 native_exit=0"
    exit 0
}
Write-Output "[MMO_REENTRANT_EXPERIENCE_RUNNER] FAIL label=$Label exit=$nativeExit observed_checks=$($caseData.Count) expected_checks=$expectedChecks summaries=$($summaries.Count) passes=$($passes.Count) errors=$($errors.Count)"
exit 1
