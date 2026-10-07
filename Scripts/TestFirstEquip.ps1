# Oyuncu kusanma istegi: automatic starter/restore haric, server grant ve tekillik.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_first_equip_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestFirstEquip,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'First equip testi zaman asimi'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'First equip logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_EQUIP_TEST\] PASS checks=8\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_EQUIP_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$equip = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_equip ')
$sessions = @($equip | ForEach-Object { if ($_.Line -match 'scope=controller sid=([a-fA-F0-9]{32}) ') { $Matches[1] } } | Select-Object -Unique)
$metricOk = $equip.Count -eq 2 -and $sessions.Count -eq 2
if ($metricOk) {
    $metricOk = $equip[0].Line -match 'lv=1 item=ShortSword instance=4 slot=0 scope=controller '
    $metricOk = $metricOk -and $equip[1].Line -match 'lv=1 item=LeatherArmor instance=1 slot=1 scope=controller '
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_EQUIP_HOOKS_TEST] PASS events=2 sessions=2 automatic/restore/stale/level excluded'
    exit 0
}
Write-Output "First equip testi KALDI (exit=$($process.ExitCode) events=$($equip.Count) sessions=$($sessions.Count))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
