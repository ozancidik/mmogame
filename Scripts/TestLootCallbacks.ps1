# Native pickup reentry regression; disposable authority NullRHI world.
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'loot-callbacks')
$ErrorActionPreference = 'Stop'
function Test-LootCallbackCoverage([string[]]$CaseLines) {
    $expected = @(foreach($route in 0..1) { foreach($partial in 0..1) { foreach($mode in 0..3) { "route${route}_partial${partial}_mode${mode}" } } })
    $checks = @('fixture','real_callout','nested_blocked','outer_inventory','outer_pickup','retry_final','conservation','stale_and_other_resources')
    $records = @($CaseLines | ForEach-Object {
        if ($_ -match ' case=(\S+) check=(\S+)') { [pscustomobject]@{case=$Matches[1];check=$Matches[2]} }
    })
    $actual = @($records.case | Sort-Object -Unique)
    $duplicates = @($records | Group-Object case,check | Where-Object {$_.Count -ne 1})
    $badChecks = @($records | Where-Object {$_.check -notin $checks})
    return $records.Count -eq 128 -and $records.Count -eq $CaseLines.Count -and $actual.Count -eq 16 -and
        @(Compare-Object $expected $actual).Count -eq 0 -and $duplicates.Count -eq 0 -and $badChecks.Count -eq 0
}
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_loot_callbacks_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestLootCallbacks,quit"')
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
Write-Output "[MMO_LOOT_CALLBACK_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_LOOT_CALLBACK_CASE\] (PASS|FAIL) case=')
$summaries = @($lines | Select-String '\[MMO_LOOT_CALLBACK_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_LOOT_CALLBACK_TEST\] PASS checks=128\b reason=')
$errors = @($lines | Select-String '\[MMO_LOOT_CALLBACK_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation')
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
$coverage = Test-LootCallbackCoverage @($cases | ForEach-Object {$_.Line})
Write-Output "[MMO_LOOT_CALLBACK_RUNNER] coverage=$coverage"
if ($timedOut) { Write-Output 'FAIL watchdog_60s'; exit 2 }
if ($nativeExit -eq 0 -and $coverage -and $summaries.Count -eq 1 -and $passes.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output '[MMO_LOOT_CALLBACK_RUNNER] PASS checks=128 routes=2 partial_modes=2 callback_modes=4 cases=16'; exit 0
}
Write-Output "[MMO_LOOT_CALLBACK_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count)"
exit 1
