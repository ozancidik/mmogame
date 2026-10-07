# Sunucuda ilk gorev tekilligi: pawn degisimi, kontrolcu aktarimi, yeni oturum.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_first_quest_metrics_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestFirstQuestMetrics,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'First quest testi zaman asimi'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'First quest logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_QUEST_METRICS_TEST\] PASS checks=6\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_QUEST_METRICS_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$firstAccept = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_quest_accept ')
$firstClaim = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_quest_claim ')
$sessions = @($firstAccept | ForEach-Object { if ($_.Line -match 'scope=controller sid=([a-fA-F0-9]{32}) ') { $Matches[1] } } | Select-Object -Unique)
$metricOk = $firstAccept.Count -eq 2 -and $firstClaim.Count -eq 1 -and $sessions.Count -eq 2
if ($metricOk) { $metricOk = $firstClaim[0].Line -match "scope=controller sid=$($sessions[0]) " }
$firstLoot = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_loot ')
$metricOk = $metricOk -and $firstLoot.Count -eq 2
if ($metricOk) {
    $metricOk = $firstLoot[0].Line -match "kind=gold item=Gold amount=2 scope=controller sid=$($sessions[0]) "
    $metricOk = $metricOk -and $firstLoot[1].Line -match "kind=gold item=Gold amount=2 scope=controller sid=$($sessions[1]) "
}
$firstKill = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_kill ')
$metricOk = $metricOk -and $firstKill.Count -eq 2
if ($metricOk) {
    $metricOk = $firstKill[0].Line -match "mob=Goblin mlv=1 xp=20 credit=fatal elite=0 scope=controller sid=$($sessions[0]) "
    $metricOk = $metricOk -and $firstKill[1].Line -match "mob=Goblin mlv=1 xp=20 credit=fatal elite=0 scope=controller sid=$($sessions[1]) "
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_QUEST_SESSION_TEST] PASS accepts=2 claims=1 sessions=2'
    Write-Output '[MMO_FIRST_LOOT_SESSION_TEST] PASS events=2 sessions=2 (pawn/transfer uniqueness)'
    Write-Output '[MMO_FIRST_KILL_SESSION_TEST] PASS events=2 sessions=2 (pawn/transfer uniqueness)'
    exit 0
}
Write-Output "First quest testi KALDI (exit=$($process.ExitCode) accepts=$($firstAccept.Count) claims=$($firstClaim.Count) sessions=$($sessions.Count))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
