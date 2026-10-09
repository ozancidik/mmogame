# Cok oyunculu duman testi: goruntusuz bir listen server + bir istemci baslatir, istemcinin baglandigini
# dogrular ve iki log'daki hatalari sayar. Editor KAPALI olmali. Kullanim:
#   powershell -File Scripts/NetSmokeTest.ps1 [-Map Summer_scene|Winter_scene] [-ClientSeconds 45] [-Port 7777]
# Cikis 0 = istemci baglandi ve iki tarafta hata satiri yok.
param(
    [string]$Map = "Summer_scene",
    [int]$ClientSeconds = 45,
    [ValidateRange(1, 65535)]
    [int]$Port = 7777
)

$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$project = Join-Path (Split-Path -Parent $PSScriptRoot) "MyProject.uproject"
$stamp = Get-Date -Format HHmmss
$slog = Join-Path $env:TEMP "mmo_net_server_$stamp.log"
$clog = Join-Path $env:TEMP "mmo_net_client_$stamp.log"
$mapPath = "/Game/HandPaintedEnvironment/Maps/$Map/$Map"

$srv = Start-Process -FilePath $exe -ArgumentList @("`"$project`"", "${mapPath}?listen", "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=$slog", "-port=$Port") -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$cli = Start-Process -FilePath $exe -ArgumentList @("`"$project`"", "127.0.0.1:$Port", "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=$clog") -PassThru -WindowStyle Hidden
Start-Sleep -Seconds $ClientSeconds
if (-not $cli.HasExited) { $cli.Kill() }
Start-Sleep -Seconds 3
if (-not $srv.HasExited) { $srv.Kill() }

$joined = (Select-String -Path $slog -Pattern "Join succeeded" | Measure-Object).Count
$welcomed = (Select-String -Path $clog -Pattern "Welcomed by server" | Measure-Object).Count
$errS = (Select-String -Path $slog -Pattern "Error:|Fatal error|Ensure condition" | Measure-Object).Count
$errC = (Select-String -Path $clog -Pattern "Error:|Fatal error|Ensure condition" | Measure-Object).Count
Write-Output "Harita: $Map  Port: $Port  Baglanti(server)=$joined  Welcomed(istemci)=$welcomed  Hata(server)=$errS  Hata(istemci)=$errC"
Write-Output "Loglar: $slog ; $clog"
if ($joined -ge 1 -and $welcomed -ge 1 -and $errS -eq 0 -and $errC -eq 0) { exit 0 } else { exit 1 }
