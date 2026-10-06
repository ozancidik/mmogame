# Kayit onayi bekleme penceresinde savasa giren oyuncu tasinmamali.
# Yerel magaza onayi test kancasiyla tutulur; gercek bulut entegrasyonu testi degildir.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root "MyProject.uproject"
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$log = Join-Path $env:TEMP "mmo_zone_travel_state_test.log"
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }

$summer = "/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene"
$args = @("`"$project`"", $summer, "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash",
    "-stdout", "-ABSLOG=$log", '-ExecCmds="MMOTestZoneTravelStateChange"')
$process = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
try {
    $deadline = (Get-Date).AddSeconds(25)
    while ((Get-Date) -lt $deadline -and -not $process.HasExited) {
        Start-Sleep -Milliseconds 250
        if ((Test-Path -LiteralPath $log) -and
            (Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_STATE_TEST\] (PASS|FAIL)' -Quiet)) {
            break
        }
    }
} finally {
    # Yalniz bu betigin baslattigi test surecini kapat.
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
if (-not (Test-Path -LiteralPath $log)) {
    Write-Output "Bolge gecisi durum testi KALDI: log yok"
    exit 1
}
$passed = Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_STATE_TEST\] PASS combat=1 lock=0 pending=0'
$accepted = Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_TEST\] request=ACCEPTED'
$aborted = Select-String -LiteralPath $log -Pattern 'Zone travel aborted: Kayit beklerken bir oyuncu oldu veya savasa girdi'
$failed = Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_STATE_TEST\] FAIL|Zone travel started:|TravelFailure|Fatal error|Ensure condition|Access violation|Bringing World .*/Winter_scene'
if ($accepted -and $aborted -and $passed -and -not $failed) {
    Write-Output "Bolge gecisi durum testi GECTI: onay beklerken savas -> iptal -> kilit ve zamanlayici temiz"
    exit 0
}
Write-Output "Bolge gecisi durum testi KALDI (accepted=$([bool]$accepted) aborted=$([bool]$aborted) passed=$([bool]$passed))"
if ($failed) { $failed | Select-Object -First 5 | ForEach-Object { Write-Output $_.Line } }
exit 1
