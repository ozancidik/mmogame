# Gorev odulunde yigin/canta doluluk, tam sigma ve tek sefer teslim regresyonu.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_quest_capacity_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestQuestRewardCapacity,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) {
    $process.Kill()
    Write-Output 'ZAMAN ASIMI: Gorev kapasite testi kapanmadi'
    exit 2
}
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'Gorev kapasite logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_QUEST_CAPACITY_TEST\] PASS checks=13\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_QUEST_CAPACITY_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
if ($pass -and -not $fail -and $process.ExitCode -eq 0) {
    Write-Output $pass.Line
    exit 0
}
Write-Output "Gorev kapasite testi KALDI (exit=$($process.ExitCode))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
