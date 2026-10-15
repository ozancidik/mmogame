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
    [int]$Port,
    [string]$UserDir,
    [string]$LogDirectory,
    [ValidateRange(0, 4000)]
    [int]$WatchdogSeconds = 0
)

$ErrorActionPreference = 'Stop'
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$project = Join-Path (Split-Path -Parent $PSScriptRoot) "MyProject.uproject"
# Her kosuya ayri kimlik: ayni TEMP/saniyede paralel smoke loglari birbirini ezmesin.
$stamp = (Get-Date -Format yyyyMMdd_HHmmss_fff) + '_' + [guid]::NewGuid().ToString('N')
$slog = Join-Path $env:TEMP "mmo_net_server_$stamp.log"
$clog = Join-Path $env:TEMP "mmo_net_client_$stamp.log"
$mapPath = "/Game/HandPaintedEnvironment/Maps/$Map/$Map"

function Assert-NetSmokePath([string]$Path) {
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path.Substring(2) -match '[:"\r\n]') { throw 'Absolute local unquoted path required.' }
    $full = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $part = $full
    while ($part) {
        if ((Test-Path -LiteralPath $part) -and
            (((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)) { throw "Reparse path refused: $part" }
        $part = Split-Path -Parent $part
    }
    return $full
}
$isolated = -not [string]::IsNullOrWhiteSpace($UserDir)
if ($isolated -or -not [string]::IsNullOrWhiteSpace($LogDirectory)) {
    if (-not $isolated -or [string]::IsNullOrWhiteSpace($LogDirectory)) { throw 'UserDir and LogDirectory must be supplied together.' }
    $root = Assert-NetSmokePath (Split-Path -Parent $project)
    $UserDir = Assert-NetSmokePath $UserDir
    $LogDirectory = Assert-NetSmokePath $LogDirectory
    if ((Split-Path -Parent $UserDir) -ine (Join-Path $root 'Saved') -or
        (Split-Path -Leaf $UserDir) -notmatch '^WinterNet_[0-9a-f]{32}$' -or
        -not (Test-Path -LiteralPath (Join-Path $root '.git') -PathType Leaf)) { throw 'Unique UserDir under isolated worktree Saved required.' }
    $null = Assert-NetSmokePath (Join-Path $root '.git')
    $registered = @(& git -C $root -c core.quotePath=false worktree list --porcelain)
    if ($LASTEXITCODE -ne 0 -or @($registered | Where-Object {
        $_.StartsWith('worktree ') -and [IO.Path]::GetFullPath($_.Substring(9)).TrimEnd('\', '/') -ieq $root
    }).Count -ne 1) { throw 'Registered isolated worktree required.' }
    foreach ($entry in @($registered | Where-Object { $_.StartsWith('worktree ') })) {
        $registeredRoot = [IO.Path]::GetFullPath($entry.Substring(9)).TrimEnd('\', '/')
        if ($LogDirectory -ieq $registeredRoot -or $LogDirectory.StartsWith($registeredRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'LogDirectory must be outside registered worktrees.'
        }
    }
    if ((Test-Path -LiteralPath $UserDir) -or (Test-Path -LiteralPath $LogDirectory)) { throw 'Fresh UserDir and LogDirectory required; existing path refused.' }
    $null = [IO.Directory]::CreateDirectory($LogDirectory)
    $null = [IO.Directory]::CreateDirectory($UserDir)
    $slog = Join-Path $LogDirectory 'server.log'
    $clog = Join-Path $LogDirectory 'client.log'
}
$manifestPath = if ($isolated) { Join-Path $LogDirectory 'processes.json' } else { Join-Path $env:TEMP "mmo_net_processes_$stamp.json" }
$deadline = if ($WatchdogSeconds) { $WatchdogSeconds } else { $ServerSeconds + $ClientSeconds + 20 }
$clock = [Diagnostics.Stopwatch]::StartNew()
$state = [ordered]@{ schema = 1; map = $Map; port = $null; userDir = $UserDir;
    serverLog = $slog; clientLog = $clog; watchdogSeconds = $deadline; timedOut = $false;
    observed = $false; serverObservedSeconds = 0; clientObservedSeconds = 0; cleanupOk = $true; exitCode = 1;
    readinessReady = $false; readinessPort = $false; readinessMap = $false; readinessElapsedSeconds = $null;
    clientBudgetRemainingSeconds = $null; startupOverheadSeconds = 5; startupFailure = $null;
    processes = @([ordered]@{ role = 'server'; pid = $null; log = $slog; stopped = $false; cleanupError = $null },
        [ordered]@{ role = 'client'; pid = $null; log = $clog; stopped = $false; cleanupError = $null }) }
function Save-NetSmokeManifest {
    $null = Assert-NetSmokePath $manifestPath
    [IO.File]::WriteAllText($manifestPath, (ConvertTo-Json -InputObject $state -Depth 6), (New-Object Text.UTF8Encoding($false)))
}
function Assert-NetSmokeDeadline {
    if ($clock.Elapsed.TotalSeconds -ge $deadline) { $state.timedOut = $true; throw "Owned-process watchdog exceeded ${deadline}s." }
}
Write-Output "Loglar: $slog ; $clog"
Write-Output "Process manifest: $manifestPath"
Save-NetSmokeManifest

# Sabit 7777 yerine bos rastgele port: 2026-10-09'da 7777 baska bir sureçte (esanli test kosusu) doluydu, sunucu 7778'e
# kaydi ve istemci 7777'deki YABANCI sunucuya baglanip testi yaniltti. Port bos mu diye UDP uclarina bakilir.
$used = @{}
try { Get-NetUDPEndpoint -ErrorAction Stop | ForEach-Object { $used[[int]$_.LocalPort] = $true } } catch {
    if ($isolated) { throw 'UDP endpoint inventory unavailable; no child launched.' }
}
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
$state.port = $selectedPort
Save-NetSmokeManifest

function Assert-NetSmokeAlive($Server, $Client) {
    Assert-NetSmokeDeadline
    if ($Server.HasExited) {
        if (-not $state.readinessReady) { $state.startupFailure = 'server-exited-before-ready' }
        throw "Sunucu gozlem tamamlanmadan kapandi (exit=$($Server.ExitCode))."
    }
    if ($null -ne $Client -and $Client.HasExited) { throw "Istemci gozlem tamamlanmadan kapandi (exit=$($Client.ExitCode))." }
}
function Wait-NetSmokeAlive([int]$Seconds, $Server, $Client) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        Assert-NetSmokeAlive $Server $Client
        Start-Sleep -Milliseconds 250
    }
    Assert-NetSmokeAlive $Server $Client
    return $timer.Elapsed.TotalSeconds
}
function Assert-NetSmokeClientBudget {
    $state.clientBudgetRemainingSeconds = $deadline - $clock.Elapsed.TotalSeconds
    if ($state.clientBudgetRemainingSeconds -lt ($ClientSeconds + $state.startupOverheadSeconds)) {
        $state.startupFailure = 'insufficient-client-budget'
        throw 'Startup readiness: full client window plus overhead no longer fits the total deadline.'
    }
}
function Wait-NetSmokeReadiness($Server) {
    # Use the original total clock; do not grant a fresh startup or client deadline.
    $portPattern = '\bLogNet:[^\r\n]*\blistening on port ' + $selectedPort + '\b'
    $worldPattern = '\bLogWorld: Bringing World ' + [regex]::Escape("$mapPath.$Map") + ' up for play(?:\s|$)'
    while ($true) {
        Assert-NetSmokeAlive $Server $null
        if (Test-Path -LiteralPath $slog) {
            $state.readinessPort = Select-String -LiteralPath $slog -Pattern $portPattern -Quiet
            $state.readinessMap = Select-String -LiteralPath $slog -Pattern $worldPattern -Quiet
        }
        Assert-NetSmokeAlive $Server $null # A server exiting during log reads is still a failure.
        $state.readinessElapsedSeconds = $clock.Elapsed.TotalSeconds
        $state.clientBudgetRemainingSeconds = $deadline - $state.readinessElapsedSeconds
        if ($state.readinessPort -and $state.readinessMap) {
            Assert-NetSmokeClientBudget
            $state.readinessReady = $true
            Save-NetSmokeManifest
            return
        }
        if ($state.clientBudgetRemainingSeconds -le ($ClientSeconds + $state.startupOverheadSeconds)) {
            $state.startupFailure = 'readiness-timeout'
            throw "Startup readiness: exact requested raw map and gameplay port $selectedPort were not both ready within the reserved client budget."
        }
        Start-Sleep -Milliseconds 250
    }
}

$srv = $null
$cli = $null
$observed = $false
$cleanupOk = $true
try {
    $userArguments = if ($isolated) { @("-UserDir=`"$UserDir`"") } else { @() }
    Assert-NetSmokeDeadline
    $srv = Start-Process -FilePath $exe -ArgumentList (@("`"$project`"", "${mapPath}?listen", "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=`"$slog`"", "-port=$selectedPort") + $userArguments) -PassThru -WindowStyle Hidden
    $null = $srv.Handle # Retain native handle; ownership never reconstructed from a PID/name lookup.
    $state.processes[0].pid = $srv.Id
    Save-NetSmokeManifest
    $state.serverObservedSeconds = Wait-NetSmokeAlive $ServerSeconds $srv $null
    Wait-NetSmokeReadiness $srv
    Assert-NetSmokeAlive $srv $null
    Assert-NetSmokeClientBudget # Include readiness manifest I/O in the same reserved budget.
    $cli = Start-Process -FilePath $exe -ArgumentList (@("`"$project`"", "127.0.0.1:$selectedPort", "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=`"$clog`"") + $userArguments) -PassThru -WindowStyle Hidden
    $null = $cli.Handle
    $state.processes[1].pid = $cli.Id
    Save-NetSmokeManifest
    $state.clientObservedSeconds = Wait-NetSmokeAlive $ClientSeconds $srv $cli
    $observed = $true
} catch {
    if ($state.startupFailure) { Write-Output "[NET_SMOKE_STARTUP] FAIL reason=$($state.startupFailure) elapsed=$($state.readinessElapsedSeconds) port=$($state.readinessPort) map=$($state.readinessMap) remaining=$($state.clientBudgetRemainingSeconds)" }
    Write-Output "KALDI: $($_.Exception.Message)"
} finally {
    # Bir cleanup hatasi diger sahip olunan surecin temizlenmesini engellemez.
    foreach ($owned in @($cli, $srv)) {
        if ($null -eq $owned) { continue }
        $row = if ([object]::ReferenceEquals($owned, $srv)) { $state.processes[0] } else { $state.processes[1] }
        try {
            if (-not $owned.HasExited) { $owned.Kill() }
            if (-not $owned.WaitForExit(5000)) { throw 'NetSmoke sureci sonlandirilamadi.' }
            $row.stopped = $true
        } catch {
            $cleanupOk = $false
            $row.cleanupError = $_.Exception.Message
            Write-Output "KALDI: cleanup: $($_.Exception.Message)"
        }
    }
    $state.observed = $observed
    $state.cleanupOk = $cleanupOk
    Save-NetSmokeManifest
}
$code = if ($state.timedOut) { 2 } else { 1 }
if (-not $observed -or -not $cleanupOk) { $state.exitCode = $code; Save-NetSmokeManifest; exit $code }
try {
    Assert-NetSmokeDeadline
    $serverLines = @(Get-Content -LiteralPath $slog)
    $clientLines = @(Get-Content -LiteralPath $clog)
    $joined = @($serverLines | Select-String -Pattern 'Join succeeded').Count
    $welcomed = @($clientLines | Select-String -Pattern 'Welcomed by server').Count
    $errS = @($serverLines | Select-String -Pattern 'Error:|Fatal error|Ensure condition|Access violation').Count
    $errC = @($clientLines | Select-String -Pattern 'Error:|Fatal error|Ensure condition|Access violation').Count
    Write-Output "Harita: $Map  Port: $selectedPort  Baglanti(server)=$joined  Welcomed(istemci)=$welcomed  Hata(server)=$errS  Hata(istemci)=$errC"
    if ($joined -lt 1 -or $welcomed -lt 1 -or $errS -gt 0 -or $errC -gt 0) { throw 'Baglanti kaniti eksik veya hata satiri var.' }
    Write-Output 'GECTI: iki surec gozlem boyunca yasadi; native temiz cikis veya gameplay RPC garantisi degildir.'
    $code = 0
} catch {
    Write-Output "KALDI: $($_.Exception.Message)"
}
$state.exitCode = if ($state.timedOut) { 2 } else { $code }
Save-NetSmokeManifest
exit $state.exitCode
