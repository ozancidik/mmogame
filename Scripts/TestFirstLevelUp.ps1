# First real authoritative XP level transition; native same-frame fixtures, not remote RPC/PIE.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_first_levelup_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestFirstLevelUp,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'First levelup timeout'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'First levelup log missing'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_LEVELUP_TEST\] PASS checks=16\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_LEVELUP_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$events = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_levelup ')
$payloads = @('lv=2 from=1 to=2','lv=2 from=1 to=2','lv=6 from=5 to=6','lv=80 from=79 to=80','lv=2 from=1 to=2')
$metricOk = $events.Count -eq $payloads.Count
if ($metricOk) {
    for ($i = 0; $i -lt $events.Count; $i++) {
        $metricOk = $metricOk -and $events[$i].Line.Contains($payloads[$i] + ' scope=controller ')
    }
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    & 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' (Join-Path $PSScriptRoot 'ValidateFirstLevelUpLog.py') $log --events 5
    if ($LASTEXITCODE -ne 0) { exit 1 }
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_LEVELUP_HOOKS_TEST] PASS events=5 threshold/multi-level/save-excluded/pawn/transfer/cap/NPC/refill-callback'
    exit 0
}
Write-Output "First levelup FAILED (exit=$($process.ExitCode) events=$($events.Count))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
