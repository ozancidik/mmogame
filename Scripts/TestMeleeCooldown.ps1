# Native same-frame server entry calls; synthetic world times, not remote RPC/PIE.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_melee_cooldown_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', '-MMOAllowSyntheticWorldTime', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestMeleeCooldown,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'Melee cooldown timeout'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'Melee cooldown log missing'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_MELEE_COOLDOWN_TEST\] PASS checks=89\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_MELEE_COOLDOWN_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
if (-not ($pass -and -not $fail -and $process.ExitCode -eq 0)) {
    Write-Output "Melee cooldown FAILED (exit=$($process.ExitCode))"
    if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
    exit 1
}
Write-Output $pass.Line
# A normal editor/game process must reject the synthetic-time command before any fixture.
$guardLog = Join-Path $env:TEMP 'mmo_melee_cooldown_guard_test.log'
if (Test-Path -LiteralPath $guardLog) { Remove-Item -LiteralPath $guardLog }
$guardArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$guardLog`"", '-ExecCmds="MMOTestMeleeCooldown,quit"')
$guardProcess = Start-Process -FilePath $exe -ArgumentList $guardArgs -PassThru -WindowStyle Hidden
if (-not $guardProcess.WaitForExit(60000)) { $guardProcess.Kill(); Write-Output 'Melee cooldown guard timeout'; exit 2 }
if (-not (Test-Path -LiteralPath $guardLog)) { Write-Output 'Melee cooldown guard log missing'; exit 1 }
$reject = @(Select-String -LiteralPath $guardLog -Pattern '\[MMO_MELEE_COOLDOWN_TEST\] FAIL checks=0 reason=requires -MMOAllowSyntheticWorldTime')
$guardErrors = Select-String -LiteralPath $guardLog -Pattern 'Error:|Fatal error|Ensure condition|Access violation|\[MMO_MELEE_COOLDOWN_TEST\] PASS'
if ($reject.Count -eq 1 -and -not $guardErrors -and $guardProcess.ExitCode -eq 0) {
    Write-Output '[MMO_MELEE_COOLDOWN_GUARD_TEST] PASS unflagged command rejected before fixtures'
    exit 0
}
Write-Output "Melee cooldown guard FAILED (exit=$($guardProcess.ExitCode) rejections=$($reject.Count))"
exit 1
