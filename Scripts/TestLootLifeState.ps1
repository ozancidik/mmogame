# Loot: oluyken toplanmaz; dirilince altin/sarf/ekipman bir kez verilir.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_loot_life_state_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestLootLifeState,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) {
    $process.Kill()
    Write-Output 'ZAMAN ASIMI: Loot oluluk testi kapanmadi'
    exit 2
}
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'Loot test logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_LOOT_LIFE_STATE_TEST\] PASS checks=10\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_LOOT_LIFE_STATE_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$firstLoot = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_loot ')
$metricOk = $firstLoot.Count -eq 1 -and $firstLoot[0].Line -match 'kind=gold item=Gold amount=17 scope=controller sid=[a-fA-F0-9]{32} '
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_LOOT_LIFE_STATE_TEST] PASS events=1 gold=17 (dead/duplicate rejected)'
    exit 0
}
Write-Output "Loot oluluk testi KALDI (exit=$($process.ExitCode))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
