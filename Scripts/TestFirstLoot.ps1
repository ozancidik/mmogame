# Ilk gercek loot: sifir kazanc/doluluk olayi yok; kismi grant miktari ve oturum tekilligi.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_first_loot_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestFirstLoot,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'First loot testi zaman asimi'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'First loot logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_LOOT_TEST\] PASS checks=6\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_LOOT_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$loot = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_loot ')
$sessions = @($loot | ForEach-Object { if ($_.Line -match 'scope=controller sid=([a-fA-F0-9]{32}) ') { $Matches[1] } } | Select-Object -Unique)
$metricOk = $loot.Count -eq 3 -and $sessions.Count -eq 3
if ($metricOk) {
    $metricOk = $loot[0].Line -match 'kind=gold item=Gold amount=2 scope=controller '
    $metricOk = $metricOk -and $loot[1].Line -match 'kind=item item=HealthPotion amount=1 scope=controller '
    $metricOk = $metricOk -and $loot[2].Line -match 'kind=equipment item=ShortSword amount=1 scope=controller '
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_LOOT_HOOKS_TEST] PASS gold=2 item=1 equipment=1 sessions=3'
    exit 0
}
Write-Output "First loot testi KALDI (exit=$($process.ExitCode) events=$($loot.Count) sessions=$($sessions.Count))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
