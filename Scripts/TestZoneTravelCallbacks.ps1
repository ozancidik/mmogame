# Gercek GameMode callback girisini izole controllersiz karakterlerle denetler.
# Bulut magazasi/gercek travel degildir; iki beklenen iptal logu fixture'in parcasidir.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_zone_travel_callback_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestZoneTravelCallbacks,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) {
    $process.Kill()
    Write-Output 'ZAMAN ASIMI: callback testi kapanmadi'
    exit 2
}
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'Callback test logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_CALLBACK_TEST\] PASS checks=9\b'
$expectedAbort = 'LogTemp: Error: Zone travel aborted: (Bir karakter kaydedilemedi; gecis iptal edildi|Bir oyuncu kayit sirasinda ayrildi; gecis iptal edildi)$'
$abortCount = @(Select-String -LiteralPath $log -Pattern $expectedAbort).Count
$errors = @(Select-String -LiteralPath $log -Pattern 'Error:|Fatal error|Ensure condition|Access violation' |
    Where-Object { $_.Line -notmatch $expectedAbort })
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_CALLBACK_TEST\] FAIL|Zone travel started:|TravelFailure|Bringing World .*/Winter_scene'
if ($pass -and $abortCount -eq 2 -and $errors.Count -eq 0 -and -not $fail -and $process.ExitCode -eq 0) {
    Write-Output $pass.Line
    exit 0
}
Write-Output "Callback testi KALDI (exit=$($process.ExitCode) aborted=$abortCount errors=$($errors.Count))"
@($fail) + $errors | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line }
exit 1
