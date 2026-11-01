# Kapanis duman testi, GERCEK surec kosucusu (docs/DESIGN_SHUTDOWN_SMOKE.md, dilimler S2 = clean, S3 = savefail, S2b = guvenlik turu).
# Gercek dedicated sunucu (UnrealEditor-Cmd.exe -game -server -NullRHI) + 1 gercek istemci baslatir; sunucu geri sayimla KENDILIGINDEN kapanir;
# isletim sistemi Process.ExitCode, Saved/Logs/MMOShutdownResult.json, sunucu/istemci loglari ve .sav metadatasi saf degerlendiriciye
# (Scripts/ShutdownSmokeEvidence.ps1) verilir. Oyun kodu DEGISTIRILMEZ, derleme YAPILMAZ (hazir ikili kullanilir).
#
# UYARI: -MMOShutdownAfterSeconds yalniz bu tek seferlik baslaticidadir; kalici servis tanimina YAZILMAZ (DESIGN_SERVER_LIFECYCLE 5).
# Kullanim (izole worktree; ikililer derlenmis olmali):
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File Scripts\TestShutdownSmoke.ps1 -RepoRoot "<worktree>" -Scenario clean|savefail
#                  [-Map Summer_scene] [-ShutdownAfter 240] [-HardCapSeconds 0] [-Port 0] [-LogDirectory <yol>] [-MarkerDirectory <yol>] [-AllowMainCheckout]
# Cikis: 0 PASS, 1 FAIL, 2 kullanim hatasi, 3 ERTELENDI / ortam yok (hicbir sey baslatilmadi), 4 INCONCLUSIVE (kanit eksik). 4 hicbir yerde "gecti" degildir.
# Ortam kurallari: baska UE/UBT/derleyici/Live Coding/MyProject sureci veya git ortak 'agent-build.lock' kilidi varsa ERTELER (3);
#   GERCEK Saved/ icin: RepoRoot, git'in KAYITLI bagli worktree'si olmali (git worktree list --porcelain); ana depo -AllowMainCheckout verilmedikce reddedilir.
# Surec guvenligi: sunucu ve istemci bir Windows Job Object'e (KILL_ON_JOB_CLOSE) baglanir; kosucu olse bile OS ikisini de oldurur. Ek olarak
#   kalici bir isaret dosyasi (-MarkerDirectory, varsayilan %TEMP%\mmo_l4_marker) yazilir; bir sonraki kosu BASINDA yetim kalan surec/salt-okunur bayrak kurtarilir.
# Soguk acilis: geri sayim motor baslatma bitmeden basladigindan (S1 K2b) ilk kosu (bos AssetRegistry onbellegi) INCONCLUSIVE olabilir; tekrar edin.
# PowerShell 5.1 ile yazildi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII.
param(
    [string]$Scenario = '',
    [string]$RepoRoot = '',
    [string]$Map = 'Summer_scene',
    [int]$ShutdownAfter = 240,
    [int]$HardCapSeconds = 0,
    [int]$Port = 0,
    [string]$LogDirectory = '',
    [string]$MarkerDirectory = '',
    [string]$ExePath = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [switch]$AllowMainCheckout
)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ShutdownSmokeEvidence.ps1')
. (Join-Path $PSScriptRoot 'ShutdownSmokeRunnerLib.ps1')

# Kullanim hatasi = cikis 2 (parametre baglama hatasi cikis 1 verirdi, bu yuzden elle dogrulanir). loginclosed S4'tur: bu dilimde YOK.
if (@('clean', 'savefail') -cnotcontains $Scenario) { Write-Host '[MMO_SMOKE_RUN] usage Scenario must be clean or savefail (loginclosed is slice S4, not implemented)'; exit 2 }
if ($Map -cnotmatch '^[A-Za-z0-9_]{1,40}$') { Write-Host '[MMO_SMOKE_RUN] usage bad Map'; exit 2 }
if ($ShutdownAfter -lt 150 -or $ShutdownAfter -gt 900) { Write-Host '[MMO_SMOKE_RUN] usage ShutdownAfter must be 150..900'; exit 2 }
if ($HardCapSeconds -lt 0 -or $HardCapSeconds -gt 3000) { Write-Host '[MMO_SMOKE_RUN] usage HardCapSeconds must be 0..3000'; exit 2 }
if ($Port -ne 0 -and ($Port -lt 1024 -or $Port -gt 65535)) { Write-Host '[MMO_SMOKE_RUN] usage Port must be 0 or 1024..65535'; exit 2 }

$ReadyBudgetSeconds = 120      # [PLACEHOLDER] sunucu hazir olma butcesi (S1: sicak ~8 sn)
$JoinBudgetSeconds = 100       # [PLACEHOLDER] istemci katilim butcesi (S1: ~22 sn)
$ExitGraceSeconds = 3          # istemci logunun atilma satirlarini yazmasi icin sunucu ciktiktan sonra bekleme
$ClientSelfExitMargin = 60     # istemci -seconds= emniyet agi: sert tavan + bu kadar (kosucu normalde istemciyi ondan once kapatir)
if ($HardCapSeconds -eq 0) { $HardCapSeconds = $ShutdownAfter + 180 }
if ($HardCapSeconds -lt ($ShutdownAfter + 60)) { Write-Host '[MMO_SMOKE_RUN] usage HardCapSeconds must be >= ShutdownAfter + 60'; exit 2 }

function Say { param([string]$Line) Write-Host ('[MMO_SMOKE_RUN] ' + $Line) }
function Finish {
    param([string]$Verdict, [string]$Reason = '')
    $code = Get-SmokeVerdictExitCode $Verdict
    Say ('RESULT scenario=' + $Scenario + ' verdict=' + $Verdict + ' exit=' + $code + $(if ($Reason) { ' reason=' + $Reason } else { '' }))
    exit $code
}
$script:buildLock = $null
function Defer {
    param([string]$Reason)
    if ($null -ne $script:buildLock) { $script:buildLock.Dispose(); $script:buildLock = $null }
    Say ('DEFERRED reason=' + $Reason + ' (nothing started)')
    exit 3
}

# ---- 1. ortam kabulu (hicbir surec baslamadan) --------------------------------------------------------------------------------
if ([string]::IsNullOrWhiteSpace($RepoRoot)) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
if (-not (Test-Path -LiteralPath $RepoRoot -PathType Container)) { Say 'usage RepoRoot is not a directory'; exit 2 }
$RepoRoot = [IO.Path]::GetFullPath($RepoRoot).TrimEnd('\', '/')
if ([string]::IsNullOrWhiteSpace($MarkerDirectory)) { $MarkerDirectory = Join-Path $env:TEMP 'mmo_l4_marker' }
$MarkerDirectory = [IO.Path]::GetFullPath($MarkerDirectory).TrimEnd('\', '/')
if ((Test-SmokePathUnder $RepoRoot $MarkerDirectory) -or ($MarkerDirectory -ieq $RepoRoot)) { Say 'usage MarkerDirectory must be outside the repo'; exit 2 }
$project = Join-Path $RepoRoot 'MyProject.uproject'
$dll = Join-Path $RepoRoot 'Binaries\Win64\UnrealEditor-MyProject.dll'
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { Say 'usage RepoRoot has no MyProject.uproject'; exit 2 }
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) { Defer 'unreal_editor_cmd_missing' }
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) { Defer 'binaries_not_built (this runner never builds; build the worktree first)' }
if ((Test-SmokeIsMainCheckout $RepoRoot) -and -not $AllowMainCheckout) { Defer 'main_checkout_refused (test saves would go to the real Saved/; use an isolated worktree or -AllowMainCheckout)' }
$saved = Join-Path $RepoRoot 'Saved'
foreach ($rp in @($RepoRoot, $saved, (Join-Path $saved 'SaveGames'), (Join-Path $saved 'Logs'))) {
    if (Test-SmokeReparse $rp) { Say ('usage reparse point refused: ' + $rp); exit 2 }
}
# git'in stderr'i 5.1'de $ErrorActionPreference='Stop' altinda istisna uretir: yerel olarak Continue.
$ErrorActionPreference = 'Continue'
$gitCommon = @(& git -C $RepoRoot rev-parse --path-format=absolute --git-common-dir 2>$null)
$gitCommonCode = $LASTEXITCODE
$gitHead = @(& git -C $RepoRoot rev-parse --short HEAD 2>$null)
$wtList = @(& git -C $RepoRoot worktree list --porcelain 2>$null)
$wtListCode = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($gitCommonCode -ne 0 -or $gitCommon.Count -ne 1) { Defer 'git_common_dir_unresolved' }
if ($wtListCode -ne 0) { Defer 'worktree_list_unresolved' }
if (-not $AllowMainCheckout) {
    if (-not (Test-SmokeIsRegisteredLinkedWorktree $RepoRoot ($wtList -join "`n"))) { Defer 'worktree_not_registered (RepoRoot is not a linked worktree in git worktree list; a bare .git file is not enough)' }
}

$state = New-SmokeState
$script:runnerFails = New-Object System.Collections.ArrayList
$script:runnerInc = New-Object System.Collections.ArrayList
$script:playerName = $null
$script:savePath = $null
$script:serverStartUtc = $null
$script:procExit = $null
$script:saveInfo = $null
$script:evidence = $null
$serverLog = ''
$clientLog = ''
$portHold = $null
try {
    $lockPath = Join-Path ([IO.Path]::GetFullPath($gitCommon[0])) 'agent-build.lock'
    try { $script:buildLock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
    catch [IO.IOException] { Defer 'agent_build_lock_held' }

    # Yetim kurtarma: onceki kosucu olduyse arkasinda kalan sunucu/istemci ve salt-okunur bayrak TEMIZLENIR (yoksa asagidaki mesgul denetimi sonsuza dek erteler).
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $MarkerDirectory
    if ($rec.Found -gt 0) { Say ('orphan recovery: markers=' + $rec.Found + ' killed=' + $rec.Killed.Count + ' readonly_restored=' + $rec.Restored.Count + ' skipped=' + $rec.Skipped.Count + ' problems=' + $rec.Problems.Count + ' moved=' + ($rec.Moved -join ',')) }
    if ($rec.RunnerAlive) { Defer 'another_smoke_runner_active' }
    if ($rec.Problems.Count -gt 0) { Defer ('orphan_recovery_failed ' + ($rec.Problems -join ',')) }

    $inventory = @()
    try { $inventory = @(Get-CimInstance -ClassName Win32_Process -ErrorAction Stop | Select-Object ProcessId, Name, CommandLine) }
    catch { Defer 'process_inventory_unavailable' }
    $busy = @(Get-SmokeBusyProcesses -Inventory $inventory)
    if ($busy.Count -gt 0) { Defer ('ue_or_build_process_active pids=' + ((@($busy | ForEach-Object { $_.ProcessId })) -join ',')) }

    $used = @{}
    try { Get-NetUDPEndpoint -ErrorAction Stop | ForEach-Object { $used[[int]$_.LocalPort] = $true } } catch { Defer 'udp_endpoint_inventory_unavailable' }
    $selected = $Port
    if ($Port -ne 0) { if ($used.ContainsKey($Port)) { Defer ('port_in_use ' + $Port) } }
    else {
        for ($i = 0; $i -lt 60 -and $selected -eq 0; $i++) { $c = Get-Random -Minimum 18000 -Maximum 18999; if (-not $used.ContainsKey($c)) { $selected = $c } }
        if ($selected -eq 0) { Defer 'no_free_port' }
    }
    # TOCTOU: secilen portu sunucu baslayana kadar SAHIPLEN (baska surec tutuyorsa ertele); birakma-baglanma penceresi icin gercek port logdan dogrulanir.
    $portHold = New-SmokeUdpHold $selected
    if ($null -eq $portHold) { Defer ('port_not_bindable ' + $selected) }
    if ([string]::IsNullOrWhiteSpace($LogDirectory)) { $LogDirectory = Join-Path $env:TEMP ('mmo_l4_' + [guid]::NewGuid().ToString('N')) }
    $LogDirectory = [IO.Path]::GetFullPath($LogDirectory).TrimEnd('\', '/')
    if (Test-SmokePathUnder $RepoRoot $LogDirectory) { Close-SmokeUdpHold $portHold; Say 'usage LogDirectory must be outside the repo'; exit 2 }
    if (Test-Path -LiteralPath $LogDirectory) { Close-SmokeUdpHold $portHold; Say 'usage LogDirectory must not exist yet'; exit 2 }
    [void][IO.Directory]::CreateDirectory($LogDirectory)
    $serverLog = Join-Path $LogDirectory 'server.log'
    $clientLog = Join-Path $LogDirectory 'client.log'
    $resultJson = Join-Path $saved 'Logs\MMOShutdownResult.json'
    Say ('start scenario=' + $Scenario + ' repo=' + $RepoRoot + ' head=' + ($gitHead -join '') + ' port=' + $selected + ' shutdown_after=' + $ShutdownAfter + ' hard_cap=' + $HardCapSeconds + ' logs=' + $LogDirectory)
    Say 'note editor-binary dedicated (-game -server), not a packaged Server target; PowerShell 5.1 only'

    # Kalici isaret dosyasi (surecler baslamadan ONCE) + Job Object.
    $state.MarkerPath = Get-SmokeMarkerPath -MarkerDirectory $MarkerDirectory -RepoRoot $RepoRoot
    $state.MarkerProcs.Clear()
    $state.Marker = [ordered]@{ format = 1; repo_root = $RepoRoot; saved_root = $saved; log_directory = $LogDirectory; runner = (Get-SmokeProcIdentity ([Diagnostics.Process]::GetCurrentProcess()) 'runner') }
    if (-not (Update-SmokeMarker $state)) { Close-SmokeUdpHold $portHold; $state.MarkerPath = $null; Defer ('marker_not_writable ' + $MarkerDirectory) }
    $state.Job = New-SmokeJob
    if ($null -eq $state.Job) { Say 'WARNING job_object_unavailable: falling back to handle-based kill only (a killed runner would leave the server/client running; the next run recovers them from the marker)' }
    else { Say 'job_object=active (KILL_ON_JOB_CLOSE)' }

    $snapshotBefore = @()
    if (Test-Path -LiteralPath $saved) { $snapshotBefore = @(Get-ChildItem -LiteralPath $saved -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }) }

    # Temizlik (finally): surecler, salt-okunur geri alma, ardindan yalniz bu kosuda olusan ve oyuncuya ait TAM ad desenli dosyalar (kanit oncesinde okundu/kopyalandi).
    $state.AfterRestore = {
        if ($null -ne $script:playerName) {
            [void](Remove-SmokeCreatedFiles -SavedRoot $saved -Before $snapshotBefore -PlayerName $script:playerName -Log { param($m) Say $m })
        }
    }

    $clock = [Diagnostics.Stopwatch]::StartNew()
    Invoke-SmokeGuarded -State $state -Body {
        # ---- 2. sunucu ----
        $sargs = Get-SmokeServerArgs -Project $project -Map $Map -LogPath $serverLog -Port $selected -ShutdownAfter $ShutdownAfter
        Close-SmokeUdpHold $portHold; $portHold = $null
        $srv = Start-Process -FilePath $ExePath -ArgumentList $sargs -PassThru -WindowStyle Hidden
        $null = $srv.Handle    # ExitCode'un okunabilmesi icin tutamaci onbellekle (PS 5.1)
        Add-SmokeOwnedProcess $state $srv
        $script:serverStartUtc = $srv.StartTime.ToUniversalTime()
        Say ('server started pid=' + $srv.Id)

        $phase = [Diagnostics.Stopwatch]::StartNew()
        $ready = $null
        while ($true) {
            $d = Get-SmokeTimeDecision $clock.Elapsed.TotalSeconds $HardCapSeconds $phase.Elapsed.TotalSeconds $ReadyBudgetSeconds
            if ($d -ne 'ok') { [void]$script:runnerInc.Add('server_not_ready:' + $d); return }
            if ($srv.HasExited) { [void]$script:runnerInc.Add('server_exited_before_ready'); return }
            $ready = Test-SmokeServerReady (Read-SmokeTextShared $serverLog)
            if ($ready.Ready) { break }
            Start-Sleep -Milliseconds 500
        }
        Say ('server ready after ' + [int]$phase.Elapsed.TotalSeconds + 's netmode=' + $ready.NetMode)
        if ($ready.NetMode -ne 1) { [void]$script:runnerInc.Add('server_not_dedicated_netmode_' + $ready.NetMode); return }

        # ---- 2b. gercek baglanan port (TOCTOU) ve loopback baglama ----
        $lp = @(Get-SmokeListeningPort (Read-SmokeTextShared $serverLog))
        if ($lp.Count -eq 0) { [void]$script:runnerInc.Add('listen_port_not_logged'); return }
        if ($lp.Count -ne 1 -or $lp[0] -ne $selected) { [void]$script:runnerInc.Add('listen_port_mismatch:' + ($lp -join '/') + '_expected_' + $selected); return }
        $binds = $null
        try { $binds = @(Get-NetUDPEndpoint -OwningProcess $srv.Id -ErrorAction Stop | Where-Object { $_.LocalPort -eq $selected }) } catch { $binds = $null }
        if ($null -eq $binds -or $binds.Count -eq 0) { Say 'bind_address_unverified (Get-NetUDPEndpoint gave no entry for the server)' }
        else {
            Say ('server udp bind: ' + ((@($binds | ForEach-Object { $_.LocalAddress + ':' + $_.LocalPort })) -join ','))
            if (@($binds | Where-Object { $_.LocalAddress -ne '127.0.0.1' }).Count -gt 0) { [void]$script:runnerInc.Add('server_not_loopback_bound'); return }
        }

        # ---- 3. istemci ----
        $cargs = Get-SmokeClientArgs -Project $project -Port $selected -LogPath $clientLog -MaxSeconds ($HardCapSeconds + $ClientSelfExitMargin)
        $cli = Start-Process -FilePath $ExePath -ArgumentList $cargs -PassThru -WindowStyle Hidden
        $null = $cli.Handle
        Add-SmokeOwnedProcess $state $cli
        Say ('client started pid=' + $cli.Id)
        $phase = [Diagnostics.Stopwatch]::StartNew()
        while ($true) {
            $d = Get-SmokeTimeDecision $clock.Elapsed.TotalSeconds $HardCapSeconds $phase.Elapsed.TotalSeconds $JoinBudgetSeconds
            if ($d -ne 'ok') { [void]$script:runnerInc.Add('client_never_joined:' + $d); return }
            if ($srv.HasExited -or $cli.HasExited) { [void]$script:runnerInc.Add('process_exited_before_join'); return }
            $slogText = Read-SmokeTextShared $serverLog
            $jp = Get-SmokeJoinedPlayer $slogText
            if ($jp.Invalid -or $jp.Distinct -gt 1) { [void]$script:runnerInc.Add('player_name_ambiguous_or_invalid'); return }
            if ($null -ne $jp.Name) { $script:playerName = $jp.Name; break }
            if (Test-SmokeClosingSeen $slogText) { [void]$script:runnerInc.Add('client_never_joined:login_already_closed'); return }
            Start-Sleep -Milliseconds 500
        }
        Say ('client joined as ' + $script:playerName + ' after ' + [int]$phase.Elapsed.TotalSeconds + 's')
        $state.PlayerName = $script:playerName
        if (-not (Update-SmokeMarker $state)) { [void]$state.Notes.Add('marker_update_failed') }
        $script:savePath = Join-Path $saved ('SaveGames\MMOSave_' + $script:playerName + '.sav')
        if (-not (Test-SmokePathUnder $saved $script:savePath)) { [void]$script:runnerInc.Add('save_path_outside_saved'); return }
        if (Test-SmokePathHasReparse $saved $script:savePath) { [void]$script:runnerInc.Add('save_path_has_reparse_component'); return }
        if (@($snapshotBefore | Where-Object { $_ -ieq $script:savePath }).Count -gt 0) { [void]$script:runnerInc.Add('player_save_preexisting (refusing to touch a save that existed before the run)'); return }

        # ---- 4. savefail enjeksiyonu: oyuncu KAYDI salt-okunur (kod degistirilmez) ----
        if ($Scenario -ceq 'savefail') {
            if (-not (Test-Path -LiteralPath $script:savePath -PathType Leaf)) {
                # S1 K6: kayit katilimda yok, atma aninda olusur. Baska bir .sav kopyalanir (yoksa yer tutucu bayt) ve salt-okunur yapilir.
                $tmpl = @(Get-ChildItem -LiteralPath (Join-Path $saved 'SaveGames') -Filter 'MMOSave_*.sav' -File -ErrorAction SilentlyContinue | Where-Object { $_.Length -gt 0 } | Sort-Object Name | Select-Object -First 1)
                if ($tmpl.Count -eq 1) { [IO.File]::Copy($tmpl[0].FullName, $script:savePath, $false) }
                else { [IO.File]::WriteAllBytes($script:savePath, [byte[]](1..64)) }
            }
            Set-SmokeReadOnly $state $script:savePath
            $ro = (([IO.File]::GetAttributes($script:savePath)) -band [IO.FileAttributes]::ReadOnly) -ne 0
            Say ('savefail injection readonly=' + $ro)
            if (-not $ro) { [void]$script:runnerInc.Add('savefail_injection_not_applied'); return }
        }

        # ---- 5. sunucunun KENDILIGINDEN cikmasini bekle (sert tavan) ----
        while (-not $srv.HasExited) {
            $d = Get-SmokeTimeDecision $clock.Elapsed.TotalSeconds $HardCapSeconds
            if ($d -ne 'ok') { [void]$script:runnerFails.Add('hard_cap_exceeded'); Say ('hard cap exceeded at ' + [int]$clock.Elapsed.TotalSeconds + 's'); return }
            if ($cli.HasExited) { Say 'note client exited on its own before the server' }
            Start-Sleep -Milliseconds 500
        }
        $script:procExit = $srv.ExitCode
        Say ('server exited on its own after ' + [int]$clock.Elapsed.TotalSeconds + 's process_exit_code=' + $script:procExit)
        Start-Sleep -Seconds $ExitGraceSeconds
        Stop-SmokeOwnedProcess $state $cli     # istemci sunucu cikinca kendiliginden kapanmaz (S1 K7): KENDI surecimizi oldururuz
        Say 'client stopped by runner'
        # KANIT ONCE, TEMIZLIK SONRA: loglar, MMOShutdownResult.json (+ kopya) ve .sav metadata finally temizligi calismadan once yakalanir.
        $script:evidence = Get-SmokeEvidenceBundle -ServerLog $serverLog -ClientLog $clientLog -ResultJson $resultJson -CopyDirectory $LogDirectory
        $fi = Get-Item -LiteralPath $script:savePath -ErrorAction SilentlyContinue
        if ($null -ne $fi) { $script:saveInfo = [pscustomobject]@{ Exists = $true; Length = [long]$fi.Length; LastWriteUtc = $fi.LastWriteTimeUtc; ServerStartUtc = $script:serverStartUtc } }
        else { $script:saveInfo = [pscustomobject]@{ Exists = $false; Length = 0; LastWriteUtc = $null; ServerStartUtc = $script:serverStartUtc } }
    }
} catch {
    Say ('runner exception: ' + $_.Exception.Message)
    [void]$script:runnerFails.Add('runner_exception')
} finally {
    Close-SmokeUdpHold $portHold
    if ($null -ne $script:buildLock) { $script:buildLock.Dispose(); $script:buildLock = $null }
}

# ---- 6. temizlik sonucu ve kanit degerlendirmesi ---------------------------------------------------------------------------------
foreach ($p in @($state.Problems)) { Say ('cleanup problem: ' + $p); [void]$script:runnerFails.Add('cleanup:' + $p) }
foreach ($n in @($state.Notes)) { Say ('note ' + $n) }
if (-not $state.Done) { Say 'cleanup did not run'; [void]$script:runnerFails.Add('cleanup_not_run') }
elseif ($state.Problems.Count -gt 0 -and $null -ne $state.MarkerPath) { Say ('marker kept for the next run to recover: ' + $state.MarkerPath) }

$evVerdict = $null
if (@($script:runnerFails).Count -eq 0 -and @($script:runnerInc).Count -eq 0) {
    if ($null -eq $script:evidence) { [void]$script:runnerInc.Add('evidence_not_captured') }
    else {
        $r = Get-ShutdownEvidenceVerdict -ServerLogText $script:evidence.ServerText -ClientLogText $script:evidence.ClientText -ResultJsonText $script:evidence.JsonText -ProcessExitCode $script:procExit -Scenario $Scenario -SaveFileInfo $script:saveInfo -PlayerA $script:playerName -ClientKickPattern 'ControlChannelClose'
        $evVerdict = $r.Verdict
        foreach ($l in $r.Lines) { Write-Host $l }
        try { [IO.File]::WriteAllLines((Join-Path $LogDirectory 'verdict.txt'), [string[]]$r.Lines) } catch { }
    }
}
foreach ($x in @($script:runnerFails)) { Say ('runner FAIL ' + $x) }
foreach ($x in @($script:runnerInc)) { Say ('runner INCONCLUSIVE ' + $x) }
if ($null -ne $evVerdict) { Say ('evidence verdict=' + $evVerdict) }
$final = Resolve-SmokeFinalVerdict -EvidenceVerdict $evVerdict -RunnerFails @($script:runnerFails) -RunnerInconclusives @($script:runnerInc)
Say ('logs kept in ' + $LogDirectory)
Finish $final
