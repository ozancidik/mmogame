# Gercek TakeDamage -> death -> XP yolu; ilk kredi turleri ve tekillik.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_first_kill_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestFirstKill,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'First kill testi zaman asimi'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'First kill logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_KILL_TEST\] PASS checks=10\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_KILL_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$kills = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_kill ')
$sessions = @($kills | ForEach-Object { if ($_.Line -match 'scope=controller sid=([a-fA-F0-9]{32}) ') { $Matches[1] } } | Select-Object -Unique)
$metricOk = $kills.Count -eq 5 -and $sessions.Count -eq 5
$expected = @('xp=2 credit=contribution', 'xp=14 credit=fatal', 'xp=2 credit=party', 'xp=20 credit=fallback', 'xp=20 credit=fatal')
foreach ($payload in $expected) {
    $count = @($kills | Where-Object { $_.Line -match "lv=1 mob=Goblin mlv=1 $payload elite=0 scope=controller " }).Count
    $metricOk = $metricOk -and $count -eq 1
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_KILL_HOOKS_TEST] PASS events=5 sessions=5 credit=fatal/contribution/party/fallback'
    exit 0
}
Write-Output "First kill testi KALDI (exit=$($process.ExitCode) events=$($kills.Count) sessions=$($sessions.Count))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
