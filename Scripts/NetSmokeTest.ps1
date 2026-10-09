# Cok oyunculu duman testi: goruntusuz bir listen server + bir istemci baslatir, istemcinin baglandigini
# dogrular ve iki log'daki hatalari sayar. Editor KAPALI olmali. Kullanim:
#   powershell -File Scripts/NetSmokeTest.ps1 [-Map Summer_scene|Winter_scene] [-ClientSeconds 45] [-Port 17779]
# Cikis 0 = istemci baglandi, gozlem boyunca iki surec yasadi ve hata satiri yok.
param(
    [ValidateSet('Summer_scene', 'Winter_scene')]
    [string]$Map = "Summer_scene",
    [ValidateRange(1, 3600)]
    [int]$ClientSeconds = 45,
    [ValidateRange(1, 300)]
    [int]$ServerSeconds = 30,
    [ValidateRange(1, 65535)]
    [int]$Port
)

$ErrorActionPreference = 'Stop'
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$project = Join-Path (Split-Path -Parent $PSScriptRoot) "MyProject.uproject"
# Her kosuya ayri kimlik: ayni TEMP/saniyede paralel smoke loglari birbirini ezmesin.
$stamp = (Get-Date -Format yyyyMMdd_HHmmss_fff) + '_' + [guid]::NewGuid().ToString('N')
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

function Assert-NetSmokeAlive($Server, $Client) {
    if ($Server.HasExited) { throw "Sunucu gozlem tamamlanmadan kapandi (exit=$($Server.ExitCode))." }
    if ($null -ne $Client -and $Client.HasExited) { throw "Istemci gozlem tamamlanmadan kapandi (exit=$($Client.ExitCode))." }
}
function Wait-NetSmokeAlive([int]$Seconds, $Server, $Client) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        Assert-NetSmokeAlive $Server $Client
        Start-Sleep -Milliseconds 250
    }
    Assert-NetSmokeAlive $Server $Client
}

$srv = $null
$cli = $null
$observed = $false
$cleanupOk = $true
Write-Output "Loglar: $slog ; $clog"
try {
    $srv = Start-Process -FilePath $exe -ArgumentList @("`"$project`"", "${mapPath}?listen", "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=`"$slog`"", "-port=$selectedPort") -PassThru -WindowStyle Hidden
    Wait-NetSmokeAlive $ServerSeconds $srv $null
    # Sahip olunan sunucunun taze logu secilen portu dogrulamadan istemci baslatilmaz.
    $listening = (Test-Path -LiteralPath $slog) -and (Select-String -LiteralPath $slog -Pattern "listening on port $selectedPort\b" -Quiet)
    if (-not $listening) { throw "Sunucu port $selectedPort dinlemiyor (port dolu ya da sunucu acilmadi)." }
    $ready = Select-String -LiteralPath $slog -SimpleMatch "LogWorld: Bringing World $mapPath.$Map up for play" -Quiet
    if (-not $ready) { throw 'Sunucu istenen haritayi yuklemedi.' }
    $cli = Start-Process -FilePath $exe -ArgumentList @("`"$project`"", "127.0.0.1:$selectedPort", "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=`"$clog`"") -PassThru -WindowStyle Hidden
    Wait-NetSmokeAlive $ClientSeconds $srv $cli
    $observed = $true
} catch {
    Write-Output "KALDI: $($_.Exception.Message)"
} finally {
    # Bir cleanup hatasi diger sahip olunan surecin temizlenmesini engellemez.
    foreach ($owned in @($cli, $srv)) {
        if ($null -eq $owned) { continue }
        try {
            if (-not $owned.HasExited) { $owned.Kill() }
            if (-not $owned.WaitForExit(5000)) { throw 'NetSmoke sureci sonlandirilamadi.' }
        } catch {
            $cleanupOk = $false
            Write-Output "KALDI: cleanup: $($_.Exception.Message)"
        }
    }
}
if (-not $observed -or -not $cleanupOk) { exit 1 }
try {
    $serverLines = @(Get-Content -LiteralPath $slog)
    $clientLines = @(Get-Content -LiteralPath $clog)
    $joined = @($serverLines | Select-String -Pattern 'Join succeeded').Count
    $welcomed = @($clientLines | Select-String -Pattern 'Welcomed by server').Count
    $errS = @($serverLines | Select-String -Pattern 'Error:|Fatal error|Ensure condition|Access violation').Count
    $errC = @($clientLines | Select-String -Pattern 'Error:|Fatal error|Ensure condition|Access violation').Count
    Write-Output "Harita: $Map  Port: $selectedPort  Baglanti(server)=$joined  Welcomed(istemci)=$welcomed  Hata(server)=$errS  Hata(istemci)=$errC"
    if ($joined -lt 1 -or $welcomed -lt 1 -or $errS -gt 0 -or $errC -gt 0) { throw 'Baglanti kaniti eksik veya hata satiri var.' }
    Write-Output 'GECTI: iki surec gozlem boyunca yasadi; native temiz cikis veya gameplay RPC garantisi degildir.'
    exit 0
} catch {
    Write-Output "KALDI: $($_.Exception.Message)"
    exit 1
}
