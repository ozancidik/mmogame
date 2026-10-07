# Kayittan stat/puan butcesi, XP tavani ve puan harcama kurallari; oyuncu diski degismez.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_stats_save_budget_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestStatsSaveBudget,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) {
    $process.Kill()
    Write-Output 'ZAMAN ASIMI: Stat kayit butcesi testi kapanmadi'
    exit 2
}
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'Stat kayit test logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_STATS_SAVE_BUDGET_TEST\] PASS checks=15\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_STATS_SAVE_BUDGET_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
if ($pass -and -not $fail -and $process.ExitCode -eq 0) { Write-Output $pass.Line; exit 0 }
Write-Output "Stat kayit butcesi testi KALDI (exit=$($process.ExitCode))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
