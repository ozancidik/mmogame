# Summer -> Winter bolge gecisinin kayit callback'lerinden sonra basladigini headless dogrular.
# Editor kapali olmali. Surec yeni harita yuklendikten sonra bu script tarafindan kapatilir.

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root "MyProject.uproject"
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$log = Join-Path $env:TEMP "mmo_zone_travel_test.log"
if (Test-Path $log) { Remove-Item $log }

$summer = "/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene"
$args = @("`"$project`"", $summer, "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash",
    "-stdout", "-ABSLOG=$log", '-ExecCmds="MMOTestZoneTravel"')
$process = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(45)
$loadedWinter = $false
while ((Get-Date) -lt $deadline -and -not $process.HasExited) {
    Start-Sleep -Milliseconds 500
    if (Test-Path $log) {
        # Seamless travel logs "Bringing World" while a direct map load logs "LoadMap".
        $loadedWinter = [bool](Select-String -Path $log -Pattern "(LoadMap:|Bringing World) .*/Winter_scene/Winter_scene" -Quiet)
        if ($loadedWinter) { break }
    }
}
if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }

$accepted = Select-String -Path $log -Pattern "\[MMO_ZONE_TRAVEL_TEST\] request=ACCEPTED"
$started = Select-String -Path $log -Pattern "Zone travel started: Summer_scene -> /Game/.*/Winter_scene"
$failed = Select-String -Path $log -Pattern "Zone travel aborted|TravelFailure|Fatal error|Ensure condition|Access violation"
if ($accepted -and $started -and $loadedWinter -and -not $failed) {
    Write-Output "Bolge gecisi testi GECTI: kayit -> ServerTravel -> Winter_scene"
    exit 0
}
Write-Output "Bolge gecisi testi KALDI (accepted=$([bool]$accepted) started=$([bool]$started) winter=$loadedWinter)"
if ($failed) { $failed | Select-Object -First 10 | ForEach-Object { Write-Output $_.Line } }
exit 1
