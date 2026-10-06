# Kayit callback'i gelmezse bolge gecisinin timeout ile acildigini headless dogrular.
# Editor kapali olmali. Surec timeout logu gorulunce bu script tarafindan kapatilir.

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root "MyProject.uproject"
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$log = Join-Path $env:TEMP "mmo_zone_travel_timeout_test.log"
if (Test-Path $log) { Remove-Item $log }

$summer = "/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene"
$args = @("`"$project`"", $summer, "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash",
    "-stdout", "-ABSLOG=$log", '-ExecCmds="MMOTestZoneTravelTimeout"')
$process = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(20)
$loadedWinter = $false
while ((Get-Date) -lt $deadline -and -not $process.HasExited) {
    Start-Sleep -Milliseconds 250
    if (Test-Path $log) {
        $loadedWinter = [bool](Select-String -Path $log -Pattern "(LoadMap:|Bringing World) .*/Winter_scene/Winter_scene" -Quiet)
        if ($loadedWinter) { break }
    }
}
if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }

$accepted = Select-String -Path $log -Pattern "\[MMO_ZONE_TRAVEL_TIMEOUT_TEST\] request=ACCEPTED"
$dropped = Select-String -Path $log -Pattern "\[MMO_ZONE_TRAVEL_TIMEOUT_TEST\] callback intentionally dropped"
$timeout = Select-String -Path $log -Pattern "Karakter kaydi zaman asimina ugradi" | Select-Object -First 1
$retry = Select-String -Path $log -Pattern "\[MMO_ZONE_TRAVEL_TEST\] request=ACCEPTED" | Select-Object -First 1
$travelStarted = Select-String -Path $log -Pattern "Zone travel started:" | Select-Object -First 1
$ordered = $timeout -and $retry -and $travelStarted -and
    $timeout.LineNumber -lt $travelStarted.LineNumber
$fatal = Select-String -Path $log -Pattern "TravelFailure|Fatal error|Ensure condition|Access violation"
if ($accepted -and $dropped -and $ordered -and $loadedWinter -and -not $fatal) {
    Write-Output "Bolge gecisi timeout testi GECTI: callback yok -> iptal -> kilit temiz -> yeniden gecis"
    exit 0
}
Write-Output "Bolge gecisi timeout testi KALDI (accepted=$([bool]$accepted) dropped=$([bool]$dropped) ordered=$([bool]$ordered) winter=$loadedWinter)"
if ($fatal) { $fatal | Select-Object -First 10 | ForEach-Object { Write-Output $_.Line } }
exit 1
