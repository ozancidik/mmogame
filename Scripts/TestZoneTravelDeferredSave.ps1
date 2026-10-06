# Ertelenen yazim hatasi gecisi iptal etmeli; sonraki basarili kayit bekleyen isareti temizlemeli.
# Headless test kancasi iki flush denemesini reddeder, gercek disk arizasi/bulut testi degildir.
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root "MyProject.uproject"
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$log = Join-Path $env:TEMP "mmo_zone_travel_deferred_test.log"
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
$summer = "/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene"
$args = @("`"$project`"", $summer, "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash",
    "-stdout", "-ABSLOG=$log", '-ExecCmds="MMOTestZoneTravelDeferredSaveFailure"')
$process = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
try {
    $deadline = (Get-Date).AddSeconds(25)
    while ((Get-Date) -lt $deadline -and -not $process.HasExited) {
        Start-Sleep -Milliseconds 250
        if ((Test-Path -LiteralPath $log) -and
            (Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_DEFERRED_TEST\] (PASS|FAIL)' -Quiet)) { break }
    }
} finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
if (-not (Test-Path -LiteralPath $log)) {
    Write-Output "Ertelenen gecis kaydi testi KALDI: log yok"
    exit 1
}
$passed = Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_DEFERRED_TEST\] PASS lock=0 kept=1 retry=1 cleared=1'
$injected = @(Select-String -LiteralPath $log -Pattern 'deferred write intentionally failed').Count
$aborted = Select-String -LiteralPath $log -Pattern 'Zone travel aborted: Ertelenen karakter kaydi yazilamadi'
$failed = Select-String -LiteralPath $log -Pattern '\[MMO_ZONE_TRAVEL_DEFERRED_TEST\] FAIL|Zone travel started:|TravelFailure|Fatal error|Ensure condition|Access violation|Bringing World .*/Winter_scene'
if ($passed -and $injected -eq 2 -and $aborted -and -not $failed) {
    Write-Output "Ertelenen gecis kaydi testi GECTI: iki yazim hatasi -> iptal -> isaret korunur -> basarili kayit temizler"
    exit 0
}
Write-Output "Ertelenen gecis kaydi testi KALDI (passed=$([bool]$passed) injected=$injected aborted=$([bool]$aborted))"
if ($failed) { $failed | Select-Object -First 5 | ForEach-Object { Write-Output $_.Line } }
exit 1
