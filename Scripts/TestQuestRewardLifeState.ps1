# Gorev odulu: oluyken tuketilmez; dirilince XP/altin/sarf bir kez verilir.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_quest_life_state_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestQuestRewardLifeState,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) {
    $process.Kill()
    Write-Output 'ZAMAN ASIMI: Gorev oluluk testi kapanmadi'
    exit 2
}
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'Gorev test logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_QUEST_LIFE_STATE_TEST\] PASS checks=8\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_QUEST_LIFE_STATE_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$accepts = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=quest_accept ')
$claims = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=quest_claim ')
$metricOk = $accepts.Count -eq 2 -and $claims.Count -eq 1
if ($metricOk) {
    $metricOk = $accepts[0].Line -match 'quest=Q_Goblins\b' -and $accepts[1].Line -match 'quest=Q_Cleanup\b' `
        -and $claims[0].Line -match 'lv=1 to=2 quest=Q_Goblins xp_reward=200 capped=0\b'
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    Write-Output $pass.Line
    Write-Output '[MMO_QUEST_METRICS_TEST] PASS accepts=2 claims=1 (reddedilen istekler olay uretmedi)'
    exit 0
}
if (-not $metricOk) { Write-Output "Gorev telemetrisi KALDI (accepts=$($accepts.Count) claims=$($claims.Count))" }
Write-Output "Gorev oluluk testi KALDI (exit=$($process.ExitCode))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
