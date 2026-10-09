# Native post-movement hook/gate fixtures; not client RPC transport or played PIE.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_first_move_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestFirstMove,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'First movement timeout'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'First movement log missing'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_MOVE_TEST\] PASS checks=36\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_MOVE_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$events = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_move ')
$modes = @('falling', 'walking', 'walking', 'falling')
$metricOk = $events.Count -eq $modes.Count
if ($metricOk) {
    for ($i = 0; $i -lt $events.Count; $i++) {
        $metricOk = $metricOk -and $events[$i].Line.Contains('mode=' + $modes[$i] + ' distance=')
    }
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    & 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' (Join-Path $PSScriptRoot 'ValidateFirstMoveLog.py') $log --events 4
    if ($LASTEXITCODE -ne 0) { exit 1 }
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_MOVE_HOOKS_TEST] PASS events=4 engine-walking/falling/gates/pawn/transfer/cleanup'
    exit 0
}
Write-Output "First movement FAILED (exit=$($process.ExitCode) events=$($events.Count))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
