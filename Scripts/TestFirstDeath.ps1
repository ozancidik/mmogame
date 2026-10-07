# Same-frame native server deaths; no real network transport, timer execution or PIE input.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_first_death_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestFirstDeath,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output 'First death timeout'; exit 2 }
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'First death log missing'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_DEATH_TEST\] PASS checks=14\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_DEATH_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
$events = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=first_death ')
$deaths = @(Select-String -LiteralPath $log -Pattern '\[MMOMETRIC\].* ev=death ')
$sessions = @($events | ForEach-Object { if ($_.Line -match 'scope=controller sid=([a-fA-F0-9]{32}) ') { $Matches[1] } } | Select-Object -Unique)
$gates = @(Select-String -LiteralPath $log -Pattern '\[MMO_FIRST_DEATH_GATES\] rejected=6\b')
$metricOk = $events.Count -eq 3 -and $deaths.Count -eq 5 -and $sessions.Count -eq 3 -and $gates.Count -eq 1
if ($metricOk) {
    $payloads = @('lv=1 by=Goblin xploss=5', 'lv=1 by=none xploss=0', 'lv=3 by=none xploss=2')
    $regularIndexes = @(0, 3, 4)
    $metricOk = $events[0].LineNumber -gt $gates[0].LineNumber
    for ($i = 0; $i -lt 3; $i++) {
        $metricOk = $metricOk -and $events[$i].Line.Contains("$($payloads[$i]) scope=controller ")
        $metricOk = $metricOk -and $events[$i].LineNumber -lt $deaths[$regularIndexes[$i]].LineNumber
        if ($events[$i].Line -match 'who=(\S+) ') {
            $metricOk = $metricOk -and $deaths[$regularIndexes[$i]].Line.Contains("who=$($Matches[1]) ")
        } else { $metricOk = $false }
    }
}
if ($pass -and -not $fail -and $process.ExitCode -eq 0 -and $metricOk) {
    & 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' (Join-Path $PSScriptRoot 'ValidateFirstDeathLog.py') $log --events 3
    if ($LASTEXITCODE -ne 0) { exit 1 }
    Write-Output $pass.Line
    Write-Output '[MMO_FIRST_DEATH_HOOKS_TEST] PASS firsts=3 deaths=5 sessions=3 gates=6 mob/unknown/zero/clamp/revive/replacement/NPC'
    exit 0
}
Write-Output "First death FAILED (exit=$($process.ExitCode) firsts=$($events.Count) deaths=$($deaths.Count) sessions=$($sessions.Count))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
