# Accepted authoritative casts, rejected requests, and controller session uniqueness.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_first_skill_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestFirstSkill,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'First skill timeout'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'First skill log missing'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_SKILL_TEST\] PASS checks=17\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_SKILL_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$events = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_skill ')
$sessions = @($events | ForEach-Object { if ($_.Line -match 'scope=controller sid=([a-fA-F0-9]{32}) ') { $Matches[1] } } | Select-Object -Unique)
$expected = @('War_PowerStrike effect=0 mana=8.000', 'Arc_QuickShot effect=1 mana=6.000',
    'War_Cleave effect=2 mana=16.000', 'Mag_Meteor effect=3 mana=38.000',
    'Pri_HolyLight effect=4 mana=15.000', 'War_BattleShout effect=5 mana=15.000', 'Pri_GreaterHeal effect=6 mana=40.000')
$metricOk = $events.Count -eq 7 -and $sessions.Count -eq 7
$gates = @(Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_SKILL_GATES\] rejected=8')
$metricOk = $metricOk -and $gates.Count -eq 1
if ($metricOk) { $metricOk = $events[0].LineNumber -gt $gates[0].LineNumber }
if ($metricOk) {
    for ($i = 0; $i -lt $expected.Count; $i++) {
        $metricOk = $metricOk -and $events[$i].Line.Contains("lv=20 skill=$($expected[$i]) scope=controller ")
        $metricOk = $metricOk -and $events[$i].Line -match ' start=\d+\.\d{3} elapsed=\d+\.\d{3}'
    }
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    & 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' (Join-Path $PSScriptRoot 'ValidateFirstSkillLog.py') $log --events 7
    if ($LASTEXITCODE -ne 0) { exit 1 }
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_SKILL_HOOKS_TEST] PASS events=7 sessions=7 effects=0..6 rejected/duplicate/NPC excluded'
    exit 0
}
Write-Output "First skill FAILED (exit=$($process.ExitCode) events=$($events.Count) sessions=$($sessions.Count))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
