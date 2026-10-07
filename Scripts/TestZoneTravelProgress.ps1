# Ilk snapshot sonrasi XP/altin, ek SaveCharacter istegi olmadan son kayda alinmali.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_zone_travel_progress_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestZoneTravelProgress"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
try {
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline -and -not $process.HasExited) {
        Start-Sleep -Milliseconds 250
        if ((Test-Path -LiteralPath $log) -and
            (Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_PROGRESS_TEST\] (PASS|FAIL)' -Quiet)) { break }
    }
} finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'Gecis ilerleme test logu yok'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_PROGRESS_TEST\] PASS changed=1 xp=1 gold=1 started=1 cleared=1'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_PROGRESS_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation|TravelFailure'
$travel = Select-String -LiteralPath $log -Pattern 'Zone travel started: Summer_scene -> .*/Winter_scene'
if ($pass -and $travel -and -not $fail) { Write-Output $pass.Line; exit 0 }
Write-Output "Gecis ilerleme testi KALDI (pass=$([bool]$pass) travel=$([bool]$travel))"
if ($fail) { $fail | Select-Object -First 6 | ForEach-Object { Write-Output $_.Line } }
exit 1
