# Satici ekonomi kapilari: kapasite, canlilik, mesafe ve tek satis.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP 'mmo_vendor_economy_test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash',
    '-stdout', "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestVendorEconomy,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(60000)) {
    $process.Kill()
    Write-Output 'ZAMAN ASIMI: Satici ekonomi testi kapanmadi'
    exit 2
}
if (-not (Test-Path -LiteralPath $log)) { Write-Output 'Satici ekonomi test logu bulunamadi'; exit 1 }
$pass = Select-String -LiteralPath $log -Pattern '\[MMO_VENDOR_ECONOMY_TEST\] PASS checks=14\b'
$fail = Select-String -LiteralPath $log -Pattern '\[MMO_VENDOR_ECONOMY_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation'
if ($pass -and -not $fail -and $process.ExitCode -eq 0) { Write-Output $pass.Line; exit 0 }
Write-Output "Satici ekonomi testi KALDI (exit=$($process.ExitCode))"
if ($fail) { $fail | Select-Object -First 8 | ForEach-Object { Write-Output $_.Line } }
exit 1
