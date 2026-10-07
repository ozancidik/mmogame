# Accepted authoritative basic attacks; same-frame native fixtures, not real RPC/PIE.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_first_basic_attack_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestFirstBasicAttack,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'First basic attack timeout'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'First basic attack log missing'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_BASIC_ATTACK_TEST\] PASS checks=21\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_BASIC_ATTACK_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$events = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_basic_attack ')
$sessions = @($events | ForEach-Object { if ($_.Line -match 'scope=controller sid=([a-fA-F0-9]{32}) ') { $Matches[1] } } | Select-Object -Unique)
$gates = @(Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_BASIC_ATTACK_GATES\] rejected=10')
$metricOk = $events.Count -eq 8 -and $sessions.Count -eq 8 -and $gates.Count -eq 1
if ($metricOk) {
    $metricOk = $events[0].LineNumber -gt $gates[0].LineNumber
    for ($i = 0; $i -lt $events.Count; $i++) {
        $payload = if ($i -eq 4) { 'player targetid=Player tlv=7' } else { 'mob targetid=Goblin tlv=1' }
        $metricOk = $metricOk -and $events[$i].Line.Contains("lv=1 target=$payload scope=controller ")
    }
    $kills = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_kill ')
    $metricOk = $metricOk -and $kills.Count -eq 1
    if ($kills.Count -eq 1) {
        $metricOk = $metricOk -and $events[6].LineNumber -lt $kills[0].LineNumber
        $metricOk = $metricOk -and $kills[0].Line -match "scope=controller sid=$($sessions[6]) "
        $metricOk = $metricOk -and $kills[0].Line.Contains('lv=1 mob=Goblin mlv=1 xp=200 credit=fatal elite=0 scope=controller ')
    }
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    & 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' (Join-Path $PSScriptRoot 'ValidateFirstBasicAttackLog.py') $log --events 8
    if ($LASTEXITCODE -ne 0) { exit 1 }
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_BASIC_ATTACK_HOOKS_TEST] PASS events=8 sessions=8 pre-XP/zero-damage/native-RPC/rejected/duplicate/NPC'
    exit 0
}
Write-Output "First basic attack FAILED (exit=$($process.ExitCode) events=$($events.Count) sessions=$($sessions.Count))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
