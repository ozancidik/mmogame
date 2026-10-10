# Native trade teardown dynamic delegate regression; disposable authority NullRHI world.
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'trade-teardown')
$ErrorActionPreference = 'Stop'
function Test-TeardownCoverage([string[]]$CaseLines) {
    $expected = @(
        foreach($side in 0..1) { foreach($mode in 0..10) { "side${side}_mode${mode}" } }
        foreach($retired in 0..1) { foreach($caller in 0..1) { foreach($route in 0..1) { "early_retired${retired}_caller${caller}_route${route}" } } }
    )
    $records = @($CaseLines | ForEach-Object {
        if ($_ -match ' case=(\S+) check=(\S+)') { [pscustomobject]@{case=$Matches[1];check=$Matches[2]} }
    })
    $actual = @($records.case | Sort-Object -Unique)
    $duplicates = @($records | Group-Object case,check | Where-Object {$_.Count -ne 1})
    return $records.Count -eq 220 -and $records.Count -eq $CaseLines.Count -and $actual.Count -eq 30 -and
        @(Compare-Object $expected $actual).Count -eq 0 -and $duplicates.Count -eq 0
}
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_trade_teardown_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestTradeTeardown,quit"')
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
Write-Output "[MMO_TRADE_TEARDOWN_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_TRADE_TEARDOWN_CASE\] (PASS|FAIL) case=')
$coverageOK = Test-TeardownCoverage @($cases | ForEach-Object {$_.Line})
$summaries = @($lines | Select-String '\[MMO_TRADE_TEARDOWN_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_TRADE_TEARDOWN_TEST\] PASS checks=220\b reason=')
$errors = @($lines | Select-String '\[MMO_TRADE_TEARDOWN_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation')
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
if ($timedOut) { Write-Output 'FAIL watchdog_60s'; exit 2 }
if ($coverageOK -and $nativeExit -eq 0 -and $cases.Count -eq 220 -and $summaries.Count -eq 1 -and $passes.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output '[MMO_TRADE_TEARDOWN_RUNNER] PASS checks=220 cases=30'; exit 0
}
Write-Output "[MMO_TRADE_TEARDOWN_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count) coverage=$coverageOK"
exit 1
