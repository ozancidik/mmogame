# Cok oyunculu duman testi: goruntusuz bir listen server + bir istemci baslatir, istemcinin baglandigini
# dogrular ve iki log'daki hatalari sayar. Editor KAPALI olmali. Kullanim:
#   powershell -File Scripts/NetSmokeTest.ps1 [-Map Summer_scene|Winter_scene] [-ClientSeconds 45] [-Port 17779]
# Cikis 0 = istemci baglandi ve iki tarafta hata satiri yok.
param(
    [string]$Map = "Summer_scene",
    [int]$ClientSeconds = 45,
    [ValidateRange(1, 65535)]
    [int]$Port
)

$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$project = Join-Path (Split-Path -Parent $PSScriptRoot) "MyProject.uproject"
$stamp = Get-Date -Format HHmmss
$slog = Join-Path $env:TEMP "mmo_net_server_$stamp.log"
$clog = Join-Path $env:TEMP "mmo_net_client_$stamp.log"
$mapPath = "/Game/HandPaintedEnvironment/Maps/$Map/$Map"

# Sabit 7777 yerine bos rastgele port: 2026-10-09'da 7777 baska bir sureçte (esanli test kosusu) doluydu, sunucu 7778'e
# kaydi ve istemci 7777'deki YABANCI sunucuya baglanip testi yaniltti. Port bos mu diye UDP uclarina bakilir.
$used = @{}
try { Get-NetUDPEndpoint -ErrorAction Stop | ForEach-Object { $used[[int]$_.LocalPort] = $true } } catch {}
$selectedPort = $Port
if ($PSBoundParameters.ContainsKey('Port')) {
    if ($used.ContainsKey($selectedPort)) { Write-Output "KALDI: istenen port $selectedPort kullanimda."; exit 1 }
} else {
    $selectedPort = 0
    for ($i = 0; $i -lt 50 -and $selectedPort -eq 0; $i++) {
        $cand = Get-Random -Minimum 17000 -Maximum 17999
        if (-not $used.ContainsKey($cand)) { $selectedPort = $cand }
    }
    if ($selectedPort -eq 0) { Write-Output 'KALDI: bos test portu bulunamadi.'; exit 1 }
}

$srv = Start-Process -FilePath $exe -ArgumentList @("`"$project`"", "${mapPath}?listen", "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=$slog", "-port=$selectedPort") -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
# Sunucu gercekten istenen portu dinlemiyorsa (port yine doldu ve motor bir sonrakine kaydi) istemciyi baslatma: yanlis sunucuya baglanirdi.
$listening = (Test-Path -LiteralPath $slog) -and (Select-String -Path $slog -Pattern "listening on port $selectedPort\b" -Quiet)
if (-not $listening) {
    if (-not $srv.HasExited) { $srv.Kill() }
    Write-Output "KALDI: sunucu port $selectedPort dinlemiyor (port dolu ya da sunucu acilmadi). Log: $slog"
    exit 1
}
$cli = Start-Process -FilePath $exe -ArgumentList @("`"$project`"", "127.0.0.1:$selectedPort", "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=$clog") -PassThru -WindowStyle Hidden
Start-Sleep -Seconds $ClientSeconds
if (-not $cli.HasExited) { $cli.Kill() }
Start-Sleep -Seconds 3
if (-not $srv.HasExited) { $srv.Kill() }

$joined = (Select-String -Path $slog -Pattern "Join succeeded" | Measure-Object).Count
$welcomed = (Select-String -Path $clog -Pattern "Welcomed by server" | Measure-Object).Count
$errS = (Select-String -Path $slog -Pattern "Error:|Fatal error|Ensure condition" | Measure-Object).Count
$errC = (Select-String -Path $clog -Pattern "Error:|Fatal error|Ensure condition" | Measure-Object).Count
Write-Output "Harita: $Map  Port: $selectedPort  Baglanti(server)=$joined  Welcomed(istemci)=$welcomed  Hata(server)=$errS  Hata(istemci)=$errC"
Write-Output "Loglar: $slog ; $clog"
if ($joined -ge 1 -and $welcomed -ge 1 -and $errS -eq 0 -and $errC -eq 0) { exit 0 } else { exit 1 }
