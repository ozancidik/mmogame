# Kapanis duman testi kosucusunun (Scripts/TestShutdownSmoke.ps1) YARDIMCI fonksiyonlari (docs/DESIGN_SHUTDOWN_SMOKE.md, dilim S2/S3 + guvenlik turu S2b).
# Komut satiri kurma, log ayristirma (oyuncu adi, sunucu hazir mi, dinlenen port), sure tavani karari, kabul/erteleme karari, cikis kodu esleme,
# yol guvenligi ve temizlik (yalniz KENDI baslatilan surecler: Job Object + tutamac, torun agaci kimlik dogrulamali, salt-okunur geri alma,
# kalici isaret dosyasi ve yetim kurtarma, olusan dosyalari TAM ad desenleriyle silme) burada; UE BASLATMAZ.
# Dot-source edilir; kendi basina bir sey calistirmaz. Testi: Scripts/TestShutdownSmokeRunner.ps1 (UE/oyun gerektirmez).
# YASAK (testin kaynak taramasi yakalar): ad ile surec oldurme (Stop-Process, taskkill /IM), kod degistirme.
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII.

. (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')   # Get-MMOSaveFileClass / ConvertTo-MMOCanonicalKey: kayit dosyasi adlandirmasinin TEK kaynagi (DESIGN_BACKUP)

$script:SmokeNameRx = '^[A-Za-z0-9_()\-]{1,32}$'
$script:SmokeTolMs = 50              # kimlik karsilastirmasi: ayni surecin iki okumasi arasindaki en buyuk fark (ms); PID yeniden kullanimi bundan cok uzaktir
$script:SmokeMarkerRx = '^smoke_run_marker_[0-9a-f]{8}\.json$'

function ConvertTo-SmokeQuotedArg {
    param([string]$Value)
    if ([string]::IsNullOrEmpty($Value)) { throw 'empty argument value' }
    if ($Value -match '["\r\n\x00]') { throw 'argument value contains a quote, CR, LF or NUL' }
    return ('"' + $Value + '"')
}

function Get-SmokeServerArgs {
    # S1 olcumunun komutu: <uproject> <harita> -game -server -NullRHI ... -MultiHome=127.0.0.1 -MMOShutdownAfterSeconds=N -MMOMaintenanceReason=bakim
    # -MultiHome: motor kaynagi Sockets/SocketSubsystem.cpp GetMultihomeAddress + OnlineSubsystemUtils NetAddressResolution.cpp (GetLocalBindAddresses): sunucu yalniz loopback'e baglanir.
    param([string]$Project, [string]$Map, [string]$LogPath, [int]$Port, [int]$ShutdownAfter, [string]$Reason = 'bakim')
    if ($Map -notmatch '^[A-Za-z0-9_]{1,40}$') { throw 'bad map name' }
    if ($Port -lt 1024 -or $Port -gt 65535) { throw 'bad port' }
    if ($ShutdownAfter -lt 150 -or $ShutdownAfter -gt 900) { throw 'bad shutdown seconds (150..900)' }
    if ($Reason -notmatch '^[a-z]{1,12}$') { throw 'bad reason' }
    return @(
        (ConvertTo-SmokeQuotedArg $Project),
        ('/Game/HandPaintedEnvironment/Maps/' + $Map + '/' + $Map),
        '-game', '-server', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
        ('-ABSLOG=' + (ConvertTo-SmokeQuotedArg $LogPath)),
        ('-port=' + $Port),
        '-MultiHome=127.0.0.1',
        ('-MMOShutdownAfterSeconds=' + $ShutdownAfter),
        ('-MMOMaintenanceReason=' + $Reason)
    )
}

function Get-SmokeClientArgs {
    # Adres URL'de ?Name= YOK: S1'de yoksayildigi olculdu (oyuncu adi sunucu logundan okunur).
    # -seconds=N: motor kaynagi Launch/Private/LaunchEngineLoop.cpp InitTime + Tick (non-Shipping): kare sureleri toplami N'yi asinca RequestExit; kosucu olse bile istemci kendiliginden kapanir.
    param([string]$Project, [int]$Port, [string]$LogPath, [int]$MaxSeconds = 0)
    if ($Port -lt 1024 -or $Port -gt 65535) { throw 'bad port' }
    if ($MaxSeconds -ne 0 -and ($MaxSeconds -lt 60 -or $MaxSeconds -gt 6000)) { throw 'bad max seconds (0 or 60..6000)' }
    $a = @(
        (ConvertTo-SmokeQuotedArg $Project),
        ('127.0.0.1:' + $Port),
        '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
        ('-ABSLOG=' + (ConvertTo-SmokeQuotedArg $LogPath))
    )
    if ($MaxSeconds -gt 0) { $a += ('-seconds=' + $MaxSeconds) }
    return $a
}

function Get-SmokeJoinedPlayer {
    # Sunucu logundaki 'Join succeeded: <ad>' satirlarindan oyuncu adi. Name = $null: yok / gecersiz / belirsiz (birden cok farkli ad).
    param([AllowNull()][string]$ServerLogText)
    $res = [pscustomobject]@{ Name = $null; Distinct = 0; Invalid = $false }
    if ([string]::IsNullOrEmpty($ServerLogText)) { return $res }
    $names = New-Object 'System.Collections.Generic.List[string]'
    foreach ($m in [regex]::Matches($ServerLogText, '(?m)^(?:\[[^\]\r\n]*\]\[[ 0-9]{1,4}\])?LogNet: Join succeeded: (?<n>[^\r\n]*?)[ \t]*\r?$')) {
        $n = $m.Groups['n'].Value
        if ($n -cnotmatch $script:SmokeNameRx) { $res.Invalid = $true; continue }
        if (-not $names.Contains($n)) { $names.Add($n) }
    }
    $res.Distinct = $names.Count
    if ($names.Count -eq 1 -and -not $res.Invalid) { $res.Name = $names[0] }
    return $res
}

function Test-SmokeServerReady {
    # Ready: 'event=lifecycle_ready' satiri var; NetMode: o satirdaki netmode (1 = NM_DedicatedServer), yoksa $null.
    param([AllowNull()][string]$ServerLogText)
    $res = [pscustomobject]@{ Ready = $false; NetMode = $null }
    if ([string]::IsNullOrEmpty($ServerLogText)) { return $res }
    $m = [regex]::Match($ServerLogText, '(?m)LogMMOLifecycle: (?:Display|Warning): event=lifecycle_ready netmode=(?<nm>\d{1,3}) phase=[A-Za-z?]{1,12}[ \t]*\r?$')
    if ($m.Success) { $res.Ready = $true; $res.NetMode = [int]$m.Groups['nm'].Value }
    return $res
}

function Get-SmokeListeningPort {
    # Sunucu logundaki GERCEK baglanan UDP port(lari): 'GameNetDriver ... IpNetDriver listening on port N' (motor, istenen port doluysa sonraki porta kayabilir: MaxPortCountToTry).
    param([AllowNull()][string]$ServerLogText)
    $ports = New-Object 'System.Collections.Generic.List[int]'
    if ([string]::IsNullOrEmpty($ServerLogText)) { return $ports.ToArray() }
    foreach ($m in [regex]::Matches($ServerLogText, '(?m)LogNet: Name:GameNetDriver [^\r\n]*IpNetDriver listening on port (?<p>\d{1,5})[ \t]*\r?$')) {
        $v = [int]$m.Groups['p'].Value
        if (-not $ports.Contains($v)) { $ports.Add($v) }
    }
    return $ports.ToArray()
}

function Test-SmokeClosingSeen {
    param([AllowNull()][string]$ServerLogText)
    if ([string]::IsNullOrEmpty($ServerLogText)) { return $false }
    return [regex]::IsMatch($ServerLogText, 'event=shutdown_phase phase=Closing ')
}

function Test-SmokePhaseSeen {
    # Sunucu logunda 'event=shutdown_phase phase=<Faz> ' satiri var mi (Closing, Flushing, ...). Faz adi yalniz harf degilse $false.
    param([AllowNull()][string]$ServerLogText, [string]$Phase)
    if ([string]::IsNullOrEmpty($ServerLogText) -or $Phase -cnotmatch '^[A-Za-z]{1,12}$') { return $false }
    return [regex]::IsMatch($ServerLogText, 'event=shutdown_phase phase=' + $Phase + ' ')
}

function Get-SmokeJoinedNames {
    # Sunucu logundaki TUM 'Join succeeded: <ad>' adlari (tekrarsiz). Invalid: gecersiz ad bicimi gorulduyse $true.
    # Get-SmokeJoinedPlayer tek oyuncu icindir; bu, loginclosed'da ikinci istemcinin (adi rastgele Gezgin######) katilip katilmadigini gormek icindir.
    param([AllowNull()][string]$ServerLogText)
    $res = [pscustomobject]@{ Names = @(); Count = 0; Invalid = $false }
    if ([string]::IsNullOrEmpty($ServerLogText)) { return $res }
    $names = New-Object 'System.Collections.Generic.List[string]'
    foreach ($m in [regex]::Matches($ServerLogText, '(?m)^(?:\[[^\]\r\n]*\]\[[ 0-9]{1,4}\])?LogNet: Join succeeded: (?<n>[^\r\n]*?)[ \t]*\r?$')) {
        $n = $m.Groups['n'].Value
        if ($n -cnotmatch $script:SmokeNameRx) { $res.Invalid = $true; continue }
        if (-not $names.Contains($n)) { $names.Add($n) }
    }
    $res.Names = $names.ToArray()
    $res.Count = $names.Count
    return $res
}

function Get-SmokePreloginRejectedCount {
    # Sunucu logundaki 'event=prelogin_rejected phase=<Faz>' (LogMMOLifecycle) satir sayisi.
    param([AllowNull()][string]$ServerLogText)
    if ([string]::IsNullOrEmpty($ServerLogText)) { return 0 }
    return [regex]::Matches($ServerLogText, '(?m)LogMMOLifecycle: (?:Display|Warning): event=prelogin_rejected phase=[A-Za-z?]{1,12}[ \t]*\r?$').Count
}

function Get-SmokeBusyProcesses {
    # Girdi: {ProcessId; Name; CommandLine} satirlari. Dondurur: baska UE/UBT/derleyici/Live Coding/MyProject isi calisiyorsa o satirlar (kosucu ERTELER).
    # Desen build.ps1 Assert-NoUnrealRunning ile uyumlu + cl/link/UnrealTraceServer + Build.bat/UBT komut satirli cmd.
    param([AllowNull()][object[]]$Inventory)
    $busy = New-Object System.Collections.ArrayList
    foreach ($p in @($Inventory)) {
        if ($null -eq $p) { continue }
        $name = [string]$p.Name
        $cmd = [string]$p.CommandLine
        $hit = $false
        if ($name -match '^(UnrealEditor.*|LiveCoding.*|UnrealBuildTool.*|AutomationTool.*|MSBuild|UAT|UnrealTraceServer|cl|link|MyProject(?:Server|Client)?(?:-.*)?)\.exe$') { $hit = $true }
        elseif ($name -ieq 'dotnet.exe' -and ([string]::IsNullOrWhiteSpace($cmd) -or $cmd -match 'UnrealBuildTool|AutomationTool|RunUAT|Epic Games[\\/]UE_')) { $hit = $true }
        elseif ($name -ieq 'cmd.exe' -and $cmd -match 'Build\.bat|RunUBT|RunUAT|UnrealBuildTool|AutomationTool') { $hit = $true }
        elseif ($cmd -match 'MyProject\.uproject' -and $name -imatch '^(UnrealEditor|UE4Editor|CrashReportClient).*\.exe$') { $hit = $true }
        if ($hit) { [void]$busy.Add($p) }
    }
    return $busy.ToArray()
}

# ---- surec kimligi (PID yeniden kullanimina karsi) ----------------------------------------------------------------------------
function Get-SmokeProcStartUtc {
    # Process nesnesinin baslangic zamani (UTC) ya da $null (okunamiyorsa / sahte nesnede yoksa).
    param($Proc)
    try {
        $v = $Proc.StartTime
        if ($null -eq $v) { return $null }
        return ([datetime]$v).ToUniversalTime()
    } catch { return $null }
}

function Test-SmokeSameInstant {
    param($A, $B)
    if ($null -eq $A -or $null -eq $B) { return $false }
    try { return ([Math]::Abs((([datetime]$A).ToUniversalTime() - ([datetime]$B).ToUniversalTime()).TotalMilliseconds) -le $script:SmokeTolMs) } catch { return $false }
}

function Get-SmokeRowCreationUtc {
    # Envanter satirinin CreationDate'i (UTC) ya da $null (alan yok / bos).
    param($Row)
    try {
        $pr = $Row.PSObject.Properties['CreationDate']
        if ($null -eq $pr -or $null -eq $pr.Value) { return $null }
        return ([datetime]$pr.Value).ToUniversalTime()
    } catch { return $null }
}

function Get-SmokeDescendants {
    # Sahip surecin torunlari. Envanter satiri {ProcessId; ParentProcessId; CreationDate}. Bir satir YALNIZ cocugun olusum zamani ebeveyninkinden
    # kucuk degilse sayilir (ParentProcessId olu bir ebeveynin yeniden kullanilan PID'ini gosterebilir). Kok kimligi (RootStartUtc) bilinmiyorsa HICBIR sey dondurulmez.
    param([AllowNull()][object[]]$Inventory, [int]$RootPid, $RootStartUtc)
    $found = New-Object System.Collections.ArrayList
    if ($null -eq $RootStartUtc) { return @() }
    $seen = New-Object 'System.Collections.Generic.HashSet[int]'
    [void]$seen.Add($RootPid)
    $frontier = New-Object 'System.Collections.Generic.Queue[object]'
    $frontier.Enqueue([pscustomobject]@{ Pid = $RootPid; Creation = ([datetime]$RootStartUtc).ToUniversalTime() })
    $guard = 0
    while ($frontier.Count -gt 0 -and $guard -lt 5000) {
        $guard++
        $cur = $frontier.Dequeue()
        foreach ($p in @($Inventory)) {
            if ($null -eq $p) { continue }
            if ([int]$p.ParentProcessId -ne [int]$cur.Pid) { continue }
            $id = [int]$p.ProcessId
            if ($seen.Contains($id)) { continue }
            $cr = Get-SmokeRowCreationUtc $p
            if ($null -eq $cr) { continue }
            if ($cr -lt $cur.Creation.AddMilliseconds(-$script:SmokeTolMs)) { continue }
            [void]$seen.Add($id)
            $o = [pscustomobject]@{ Pid = $id; Creation = $cr }
            [void]$found.Add($o)
            $frontier.Enqueue($o)
        }
    }
    return $found.ToArray()
}

function Get-SmokeTimeDecision {
    # 'hard_cap' (toplam sure doldu veya girdi gecersiz: kapali basarisiz) | 'phase_timeout' (asama suresi doldu) | 'ok'.
    param($TotalElapsed, $HardCap, $PhaseElapsed = 0, $PhaseLimit = 0)
    foreach ($v in @($TotalElapsed, $HardCap, $PhaseElapsed, $PhaseLimit)) {
        if ($null -eq $v -or ($v -isnot [double] -and $v -isnot [int] -and $v -isnot [long])) { return 'hard_cap' }
        if ([double]::IsNaN([double]$v) -or [double]$v -lt 0) { return 'hard_cap' }
    }
    if ([double]$TotalElapsed -ge [double]$HardCap) { return 'hard_cap' }
    if ([double]$PhaseLimit -gt 0 -and [double]$PhaseElapsed -ge [double]$PhaseLimit) { return 'phase_timeout' }
    return 'ok'
}

function Get-SmokeVerdictExitCode {
    # Sozlesme: 0 PASS, 1 FAIL, 4 INCONCLUSIVE. Bilinmeyen deger kapali basarisiz (1).
    param([AllowNull()][string]$Verdict)
    switch -CaseSensitive ($Verdict) {
        'PASS' { return 0 }
        'FAIL' { return 1 }
        'INCONCLUSIVE' { return 4 }
        default { return 1 }
    }
}

function Resolve-SmokeFinalVerdict {
    # Degerlendirici verdict'i + kosucu seviyesindeki nedenler: FAIL > INCONCLUSIVE > PASS. Kosucu nedeni varken PASS donmez.
    param([AllowNull()][string]$EvidenceVerdict, [string[]]$RunnerFails = @(), [string[]]$RunnerInconclusives = @())
    if (@($RunnerFails).Count -gt 0) { return 'FAIL' }
    if ($EvidenceVerdict -ceq 'FAIL') { return 'FAIL' }
    if (@($RunnerInconclusives).Count -gt 0) { return 'INCONCLUSIVE' }
    if ($EvidenceVerdict -ceq 'INCONCLUSIVE') { return 'INCONCLUSIVE' }
    if ($EvidenceVerdict -ceq 'PASS') { return 'PASS' }
    return 'FAIL'
}

# ---- yol / depo guvenligi -----------------------------------------------------------------------------------------------------
function Test-SmokePathUnder {
    # Dize duzeyinde: Path, Root'un ALTINDA mi (Root'un kendisi sayilmaz); '..' ve karisik ayiricilar normallestirilir.
    param([string]$Root, [string]$Path)
    if ([string]::IsNullOrWhiteSpace($Root) -or [string]::IsNullOrWhiteSpace($Path)) { return $false }
    try {
        $r = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
        $p = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    } catch { return $false }
    return $p.StartsWith($r + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Test-SmokeIsMainCheckout {
    # Ana depo: '.git' bir KLASOR; izole worktree'de '.git' bir DOSYADIR. Belirsizse (hicbiri yok) ana depo sayilir (kapali basarisiz).
    # Bu YETMEZ (sahte '.git' dosyasi): kosucu ayrica 'git worktree list --porcelain' kaydini da dogrular (Test-SmokeIsRegisteredLinkedWorktree).
    param([string]$RepoRoot)
    $g = Join-Path $RepoRoot '.git'
    if (Test-Path -LiteralPath $g -PathType Leaf) { return $false }
    return $true
}

function Get-SmokeWorktreeEntries {
    # 'git worktree list --porcelain' metni -> kayitli worktree yollari (ILK kayit = ana worktree). Bos / bozuk girdi = bos dizi.
    param([AllowNull()][string]$PorcelainText)
    $out = New-Object System.Collections.ArrayList
    if ([string]::IsNullOrEmpty($PorcelainText)) { return $out.ToArray() }
    foreach ($m in [regex]::Matches($PorcelainText, '(?m)^worktree (?<p>[^\r\n]+?)\r?$')) {
        $p = $m.Groups['p'].Value
        try { [void]$out.Add([IO.Path]::GetFullPath($p).TrimEnd('\', '/')) } catch { }
    }
    return $out.ToArray()
}

function Test-SmokeIsRegisteredLinkedWorktree {
    # RepoRoot, git'in kayitli BAGLI worktree'lerinden biri mi (ana worktree = listenin ilk kaydi SAYILMAZ). Sahte '.git' dosyasi yetmez.
    param([string]$RepoRoot, [AllowNull()][string]$PorcelainText)
    $entries = @(Get-SmokeWorktreeEntries $PorcelainText)
    if ($entries.Count -lt 2) { return $false }
    try { $r = [IO.Path]::GetFullPath($RepoRoot).TrimEnd('\', '/') } catch { return $false }
    for ($i = 1; $i -lt $entries.Count; $i++) {
        if ($entries[$i] -ieq $r) { return $true }
    }
    return $false
}

function Test-SmokeReparse {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $false }
    return ((([IO.File]::GetAttributes($Path)) -band [IO.FileAttributes]::ReparsePoint) -ne 0)
}

function Test-SmokePathHasReparse {
    # Root'un kendisi dahil, Root'tan Path'e inen BUTUN bilesenlerde reparse (junction/symlink) var mi. Path Root'un altinda degilse $true (kapali basarisiz).
    param([string]$Root, [string]$Path)
    if ([string]::IsNullOrWhiteSpace($Root) -or [string]::IsNullOrWhiteSpace($Path)) { return $true }
    try {
        $r = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
        $p = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    } catch { return $true }
    if ($p -ieq $r) { return (Test-SmokeReparse $r) }
    if (-not $p.StartsWith($r + '\', [StringComparison]::OrdinalIgnoreCase)) { return $true }
    if (Test-SmokeReparse $r) { return $true }
    $cur = $r
    foreach ($seg in $p.Substring($r.Length + 1).Split('\')) {
        $cur = $cur + '\' + $seg
        if (Test-SmokeReparse $cur) { return $true }
    }
    return $false
}

function New-SmokeUdpHold {
    # Secilen loopback UDP portunu sunucu baslayana kadar SAHIPLENIR (secim -> baslatma TOCTOU penceresini daraltir; baska bir surec tutuyorsa $null).
    # Kalan pencere (birakma -> motorun baglanmasi) icin sunucu logundaki GERCEK port ayrica dogrulanir. Birakmak: Close-SmokeUdpHold.
    param([int]$Port)
    $s = $null
    try {
        $s = New-Object Net.Sockets.Socket([Net.Sockets.AddressFamily]::InterNetwork, [Net.Sockets.SocketType]::Dgram, [Net.Sockets.ProtocolType]::Udp)
        $s.ExclusiveAddressUse = $true
        $s.Bind((New-Object Net.IPEndPoint([Net.IPAddress]::Loopback, $Port)))
        return $s
    } catch { if ($null -ne $s) { try { $s.Close() } catch { } }; return $null }
}

function Close-SmokeUdpHold {
    param($Hold)
    if ($null -ne $Hold) { try { $Hold.Close() } catch { } }
}

function Get-SmokeCreatedFiles {
    # Kosu sirasinda OLUSAN dosyalar: oncesinde yok + sonrasinda var + yolu Saved altinda + TAM adi oyuncuya ait kayit/yan dosya deseni
    # (Get-MMOSaveFileClass: SaveGames/MMOSave_<ad>.sav, MMOAchievements|MMODaily/MMOSave_<ad>.json[.tmp|.bad|.tmp.bad], MMOMail/MMOMail_<anahtar>.json[...], MMORace/MMORace_[mmosave_]<anahtar>.json[...]).
    # Alt dize eslesmesi YOK: 'a' oyuncusu 'ab' dosyasini, 'MMO'/'Result' adlari MMOShutdownResult.json'i veya baska kayitlari silemez. Logs\ ve beyaz liste disi her sey haric.
    param([string[]]$Before, [string[]]$After, [string]$PlayerName, [string]$SavedRoot)
    if ([string]::IsNullOrEmpty($PlayerName) -or $PlayerName -cnotmatch $script:SmokeNameRx) { return @() }
    if ([string]::IsNullOrWhiteSpace($SavedRoot)) { return @() }
    $canon = ConvertTo-MMOCanonicalKey $PlayerName
    if ($null -eq $canon) { return @() }
    try { $root = [IO.Path]::GetFullPath($SavedRoot).TrimEnd('\', '/') } catch { return @() }
    $b = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($x in @($Before)) { [void]$b.Add([string]$x) }
    $out = New-Object System.Collections.ArrayList
    foreach ($x in @($After)) {
        $s = [string]$x
        if ($b.Contains($s)) { continue }
        if (-not (Test-SmokePathUnder $root $s)) { continue }
        $rel = ([IO.Path]::GetFullPath($s).Substring($root.Length + 1)).Replace('\', '/')
        if ($rel.StartsWith('Logs/', [StringComparison]::OrdinalIgnoreCase)) { continue }
        $c = Get-MMOSaveFileClass $rel
        if ($c.Class -eq 'ignored') { continue }
        if ($c.Group -cne $canon) { continue }
        [void]$out.Add($s)
    }
    return $out.ToArray()
}

function Remove-SmokeCreatedFiles {
    # Bu kosuda olusan, oyuncuya ait test dosyalarini siler (Get-SmokeCreatedFiles). Her dosya icin: Saved altinda + yolun HICBIR bileseni reparse degil.
    # Cagri sirasi: kanit okuma/kopyalama (Get-SmokeEvidenceBundle) -> SALT-OKUNUR GERI ALMA -> bu fonksiyon.
    param([string]$SavedRoot, [string[]]$Before, [string]$PlayerName, [scriptblock]$Log = $null)
    $removed = New-Object System.Collections.ArrayList
    if ([string]::IsNullOrEmpty($PlayerName) -or -not (Test-Path -LiteralPath $SavedRoot -PathType Container)) { return $removed.ToArray() }
    $after = @(Get-ChildItem -LiteralPath $SavedRoot -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
    foreach ($f in @(Get-SmokeCreatedFiles -Before $Before -After $after -PlayerName $PlayerName -SavedRoot $SavedRoot)) {
        if ((Test-SmokePathUnder $SavedRoot $f) -and -not (Test-SmokePathHasReparse $SavedRoot $f)) {
            try { [IO.File]::Delete($f); [void]$removed.Add($f); if ($null -ne $Log) { & $Log ('cleanup removed ' + $f) } }
            catch { if ($null -ne $Log) { & $Log ('cleanup could not remove ' + $f) } }
        }
    }
    return $removed.ToArray()
}

function Read-SmokeTextShared {
    # Yazan surec varken de oku (FileShare.ReadWrite). Dosya yoksa $null. Log icin: BOM atilir.
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $fs = New-Object IO.FileStream($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $sr = New-Object IO.StreamReader($fs, (New-Object Text.UTF8Encoding($false)), $true)
        try { return $sr.ReadToEnd() } finally { $sr.Dispose() }
    } finally { $fs.Dispose() }
}

function Read-SmokeRawText {
    # BOM'u KORUR (degerlendirici 'result_json_bom' icin): baytlar UTF8 GetString ile cozulur. Dosya yoksa $null.
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $fs = New-Object IO.FileStream($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $ms = New-Object IO.MemoryStream
        $fs.CopyTo($ms)
        return (New-Object Text.UTF8Encoding($false)).GetString($ms.ToArray())
    } finally { $fs.Dispose() }
}

function Get-SmokeEvidenceBundle {
    # KANIT TOPLAMA: sunucu logu, istemci logu, MMOShutdownResult.json metni; JSON ayrica CopyDirectory'ye BAYT BAYT kopyalanir (Saved temizligi oncesi).
    # Temizlik (dosya silme) bundan SONRA calisir; temizlik kanit dosyasina dokunamaz (Logs\ haric) ama sira da zorunludur.
    param([string]$ServerLog, [string]$ClientLog, [string]$ResultJson, [string]$CopyDirectory = '', [string]$ClientBLog = '')
    $b = [pscustomobject]@{ ServerText = $null; ClientText = $null; ClientBText = $null; JsonText = $null; JsonCopy = $null }
    $b.ServerText = Read-SmokeTextShared $ServerLog
    $b.ClientText = Read-SmokeTextShared $ClientLog
    if (-not [string]::IsNullOrEmpty($ClientBLog)) { $b.ClientBText = Read-SmokeTextShared $ClientBLog }
    $b.JsonText = Read-SmokeRawText $ResultJson
    if (-not [string]::IsNullOrEmpty($CopyDirectory) -and (Test-Path -LiteralPath $ResultJson -PathType Leaf) -and (Test-Path -LiteralPath $CopyDirectory -PathType Container)) {
        try {
            $dst = Join-Path $CopyDirectory 'MMOShutdownResult.json.copy'
            $in = New-Object IO.FileStream($ResultJson, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
            try {
                $out = New-Object IO.FileStream($dst, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
                try { $in.CopyTo($out) } finally { $out.Dispose() }
            } finally { $in.Dispose() }
            $b.JsonCopy = $dst
        } catch { $b.JsonCopy = $null }
    }
    return $b
}

# ---- Windows Job Object (kosucu olse bile surecler olur: KILL_ON_JOB_CLOSE) --------------------------------------------------
$script:SmokeJobSource = @'
using System;
using System.Runtime.InteropServices;
public static class MMOSmokeJobNative
{
    [StructLayout(LayoutKind.Sequential)]
    public struct IO_COUNTERS { public ulong ReadOperationCount; public ulong WriteOperationCount; public ulong OtherOperationCount; public ulong ReadTransferCount; public ulong WriteTransferCount; public ulong OtherTransferCount; }
    [StructLayout(LayoutKind.Sequential)]
    public struct JOBOBJECT_BASIC_LIMIT_INFORMATION { public long PerProcessUserTimeLimit; public long PerJobUserTimeLimit; public uint LimitFlags; public UIntPtr MinimumWorkingSetSize; public UIntPtr MaximumWorkingSetSize; public uint ActiveProcessLimit; public UIntPtr Affinity; public uint PriorityClass; public uint SchedulingClass; }
    [StructLayout(LayoutKind.Sequential)]
    public struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION { public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation; public IO_COUNTERS IoInfo; public UIntPtr ProcessMemoryLimit; public UIntPtr JobMemoryLimit; public UIntPtr PeakProcessMemoryUsed; public UIntPtr PeakJobMemoryUsed; }
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr CreateJobObject(IntPtr attrs, string name);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetInformationJobObject(IntPtr job, int infoClass, IntPtr info, uint size);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool IsProcessInJob(IntPtr process, IntPtr job, out bool result);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool CloseHandle(IntPtr handle);
    public static int LastError = 0;
    public static IntPtr Create()
    {
        IntPtr job = CreateJobObject(IntPtr.Zero, null);
        if (job == IntPtr.Zero) { LastError = Marshal.GetLastWin32Error(); return IntPtr.Zero; }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = new JOBOBJECT_EXTENDED_LIMIT_INFORMATION();
        info.BasicLimitInformation.LimitFlags = 0x2000;
        int size = Marshal.SizeOf(typeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
        IntPtr buf = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.StructureToPtr(info, buf, false);
            if (!SetInformationJobObject(job, 9, buf, (uint)size)) { LastError = Marshal.GetLastWin32Error(); CloseHandle(job); return IntPtr.Zero; }
        }
        finally { Marshal.FreeHGlobal(buf); }
        return job;
    }
    public static bool Assign(IntPtr job, IntPtr process)
    {
        if (!AssignProcessToJobObject(job, process)) { LastError = Marshal.GetLastWin32Error(); return false; }
        return true;
    }
    public static bool InJob(IntPtr job, IntPtr process)
    {
        bool r;
        if (!IsProcessInJob(process, job, out r)) { LastError = Marshal.GetLastWin32Error(); return false; }
        return r;
    }
    public static bool Close(IntPtr job) { return CloseHandle(job); }
}
'@

function New-SmokeJob {
    # KILL_ON_JOB_CLOSE'lu bos bir Job Object olusturur. Windows disi / derleme / olusturma hatasinda $null (cagiran ACIK uyari verir ve tutamac oldurmeye doner).
    if ($env:OS -ne 'Windows_NT') { return $null }
    try {
        if (-not ('MMOSmokeJobNative' -as [type])) { Add-Type -TypeDefinition $script:SmokeJobSource -ErrorAction Stop }
        $h = [MMOSmokeJobNative]::Create()
        if ($h -eq [IntPtr]::Zero) { return $null }
        return $h
    } catch { return $null }
}

function Close-SmokeJob {
    # Tutamagi kapatir: isin icindeki HAYATTA kalan her surec OS tarafindan olur.
    param($State)
    if ($null -eq $State.Job) { return }
    try { [void][MMOSmokeJobNative]::Close($State.Job) } catch { }
    $State.Job = $null
}

# ---- kalici isaret dosyasi (marker) ve yetim kurtarma --------------------------------------------------------------------------
function Get-SmokeMarkerPath {
    param([string]$MarkerDirectory, [string]$RepoRoot)
    $sha = [Security.Cryptography.SHA1]::Create()
    try { $h = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($RepoRoot.ToLowerInvariant().TrimEnd('\', '/'))) } finally { $sha.Dispose() }
    $hex = (($h | ForEach-Object { $_.ToString('x2') }) -join '').Substring(0, 8)
    return (Join-Path $MarkerDirectory ('smoke_run_marker_' + $hex + '.json'))
}

function Get-SmokeProcIdentity {
    param($Proc, [string]$Role)
    $start = Get-SmokeProcStartUtc $Proc
    $name = $null
    try { $name = [string]$Proc.ProcessName } catch { $name = $null }
    return [ordered]@{ role = $Role; pid = [int]$Proc.Id; name = $name; start_utc = $(if ($null -ne $start) { $start.ToString('o') } else { $null }) }
}

function Update-SmokeMarker {
    # Gecerli durumu (kosucu, surecler, salt-okunur yollar, oyuncu adi) isaret dosyasina ATOMIK yazar (.tmp -> tasi). Basari $true.
    param($State)
    if ($null -eq $State.MarkerPath) { return $true }
    try {
        $m = $State.Marker
        $m['written_utc'] = [datetime]::UtcNow.ToString('o')
        $m['player_name'] = $State.PlayerName
        $m['processes'] = @($State.MarkerProcs.ToArray())
        $m['readonly_paths'] = @($State.ReadOnly.ToArray())
        $json = ConvertTo-Json -InputObject $m -Depth 6
        $dir = Split-Path -Parent $State.MarkerPath
        if (-not (Test-Path -LiteralPath $dir -PathType Container)) { [void][IO.Directory]::CreateDirectory($dir) }
        $tmp = $State.MarkerPath + '.tmp'
        [IO.File]::WriteAllText($tmp, $json, (New-Object Text.UTF8Encoding($false)))
        if (Test-Path -LiteralPath $State.MarkerPath -PathType Leaf) { [IO.File]::Replace($tmp, $State.MarkerPath, [NullString]::Value) }
        else { [IO.File]::Move($tmp, $State.MarkerPath) }
        return $true
    } catch { return $false }
}

function Remove-SmokeMarker {
    # Temiz bitti: isaret dosyasini kaldirir. Temizlik sorunu varsa CAGRILMAZ (bir sonraki kosu kurtarir).
    param($State)
    if ($null -eq $State.MarkerPath) { return }
    try { if (Test-Path -LiteralPath $State.MarkerPath -PathType Leaf) { [IO.File]::Delete($State.MarkerPath) } } catch { [void]$State.Notes.Add('marker_delete_failed') }
}

function Get-SmokeJsonProp {
    param($Obj, [string]$Name)
    try { $pr = $Obj.PSObject.Properties[$Name]; if ($null -eq $pr) { return $null }; return $pr.Value } catch { return $null }
}

function Invoke-SmokeOrphanRecovery {
    # Onceki (olmus) kosucudan kalan isaret dosyalarini kurtarir: YALNIZ {PID + ad (UnrealEditor*) + baslangic zamani} UCLUSU eslesen CANLI surecleri oldurur,
    # beyaz listedeki karakter kaydinin (Saved altinda, reparse yok) salt-okunur bayragini kaldirir, dosyayi '.stale.<damga>' diye tasir. Kosucunun kendisi hala yasiyorsa
    # RunnerAlive=$true (cagiran ERTELER). Isaret dosyasi guvenilmez veridir: her alan dogrulanir.
    param([string]$MarkerDirectory, [scriptblock]$ProcessLookup = $null, [string]$Stamp = '')
    $res = [pscustomobject]@{ Found = 0; RunnerAlive = $false; Killed = (New-Object System.Collections.ArrayList); Restored = (New-Object System.Collections.ArrayList); Skipped = (New-Object System.Collections.ArrayList); Problems = (New-Object System.Collections.ArrayList); Moved = (New-Object System.Collections.ArrayList) }
    if ([string]::IsNullOrWhiteSpace($MarkerDirectory) -or -not (Test-Path -LiteralPath $MarkerDirectory -PathType Container)) { return $res }
    if ($null -eq $ProcessLookup) { $ProcessLookup = { param($ProcId) try { [Diagnostics.Process]::GetProcessById($ProcId) } catch { $null } } }
    if ([string]::IsNullOrEmpty($Stamp)) { $Stamp = [datetime]::UtcNow.ToString('yyyyMMddTHHmmssfff') }
    foreach ($f in @(Get-ChildItem -LiteralPath $MarkerDirectory -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -cmatch $script:SmokeMarkerRx })) {
        $res.Found++
        $m = $null
        try { $m = [IO.File]::ReadAllText($f.FullName, (New-Object Text.UTF8Encoding($false))) | ConvertFrom-Json } catch { $m = $null }
        if ($null -eq $m) {
            try { [IO.File]::Move($f.FullName, $f.FullName + '.corrupt.' + $Stamp); [void]$res.Moved.Add($f.Name + '.corrupt') } catch { [void]$res.Problems.Add('marker_corrupt_not_moved:' + $f.Name) }
            [void]$res.Skipped.Add('marker_corrupt:' + $f.Name)
            continue
        }
        $problemsBefore = $res.Problems.Count
        # 1) kosucu hala yasiyor mu
        $rn = Get-SmokeJsonProp $m 'runner'
        $rPid = Get-SmokeJsonProp $rn 'pid'
        $rStart = Get-SmokeJsonProp $rn 'start_utc'
        if ($rPid -is [int] -or $rPid -is [long]) {
            $rp = $null
            if ([int]$rPid -gt 4) { try { $rp = & $ProcessLookup ([int]$rPid) } catch { $rp = $null } }
            if ($null -ne $rp -and -not (Test-SmokeProcessGone $rp)) {
                $rs = $null
                try { $rs = ([datetime]::Parse([string]$rStart, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind)).ToUniversalTime() } catch { $rs = $null }
                if (Test-SmokeSameInstant (Get-SmokeProcStartUtc $rp) $rs) { $res.RunnerAlive = $true; [void]$res.Skipped.Add('runner_alive:' + $f.Name); continue }
            }
        }
        # 2) sahipsiz surecler
        foreach ($e in @((Get-SmokeJsonProp $m 'processes'))) {
            if ($null -eq $e) { continue }
            $ePid = Get-SmokeJsonProp $e 'pid'
            $eName = [string](Get-SmokeJsonProp $e 'name')
            $eStart = $null
            try { $eStart = ([datetime]::Parse([string](Get-SmokeJsonProp $e 'start_utc'), [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind)).ToUniversalTime() } catch { $eStart = $null }
            if (-not ($ePid -is [int] -or $ePid -is [long]) -or [int]$ePid -le 4 -or [int]$ePid -eq $PID) { [void]$res.Skipped.Add('bad_pid'); continue }
            if ($eName -cnotmatch '^UnrealEditor[A-Za-z0-9_\-]*$' -or $null -eq $eStart) { [void]$res.Skipped.Add('bad_identity:' + $ePid); continue }
            $p = $null
            try { $p = & $ProcessLookup ([int]$ePid) } catch { $p = $null }
            if ($null -eq $p -or (Test-SmokeProcessGone $p)) { [void]$res.Skipped.Add('gone:' + $ePid); continue }
            $liveName = $null
            try { $liveName = [string]$p.ProcessName } catch { $liveName = $null }
            if ($liveName -cne $eName -or -not (Test-SmokeSameInstant (Get-SmokeProcStartUtc $p) $eStart)) { [void]$res.Skipped.Add('identity_mismatch:' + $ePid); continue }
            try {
                $p.Kill()
                if ($p.WaitForExit(5000)) { [void]$res.Killed.Add([int]$ePid) } else { [void]$res.Problems.Add('orphan_not_stopped:' + $ePid) }
            } catch { [void]$res.Problems.Add('orphan_kill_error:' + $ePid) }
        }
        # 3) salt-okunur bayraklar (yalniz Saved altindaki karakter kaydi)
        $savedRoot = [string](Get-SmokeJsonProp $m 'saved_root')
        foreach ($rp2 in @((Get-SmokeJsonProp $m 'readonly_paths'))) {
            if ($null -eq $rp2) { continue }
            $path = [string]$rp2
            $ok = $false
            try {
                if (-not [string]::IsNullOrWhiteSpace($savedRoot) -and (Test-SmokePathUnder $savedRoot $path)) {
                    $rel = ([IO.Path]::GetFullPath($path).Substring([IO.Path]::GetFullPath($savedRoot).TrimEnd('\', '/').Length + 1)).Replace('\', '/')
                    if ((Get-MMOSaveFileClass $rel).Class -eq 'character' -and -not (Test-SmokePathHasReparse $savedRoot $path)) { $ok = $true }
                }
            } catch { $ok = $false }
            if (-not $ok) { [void]$res.Skipped.Add('readonly_path_rejected'); continue }
            try {
                if (Test-Path -LiteralPath $path -PathType Leaf) {
                    [IO.File]::SetAttributes($path, (([IO.File]::GetAttributes($path)) -band (-bnot [IO.FileAttributes]::ReadOnly)))
                    if ((([IO.File]::GetAttributes($path)) -band [IO.FileAttributes]::ReadOnly) -ne 0) { [void]$res.Problems.Add('orphan_readonly_not_restored:' + $path) } else { [void]$res.Restored.Add($path) }
                }
            } catch { [void]$res.Problems.Add('orphan_readonly_error:' + $path) }
        }
        # 4) isaret dosyasini kenara al (sorun yoksa)
        if ($res.Problems.Count -eq $problemsBefore) {
            try { [IO.File]::Move($f.FullName, $f.FullName + '.stale.' + $Stamp); [void]$res.Moved.Add($f.Name + '.stale') } catch { [void]$res.Problems.Add('marker_not_moved:' + $f.Name) }
        }
    }
    return $res
}

function Test-SmokeProcessGone {
    param($P)
    try { return [bool]$P.HasExited } catch { return $true }
}

# ---- temizlik: yalniz KENDI baslatilan surecler, salt-okunur geri alma --------------------------------------------------------
function New-SmokeState {
    return @{
        Procs = (New-Object System.Collections.ArrayList)
        ReadOnly = (New-Object System.Collections.ArrayList)
        Locks = (New-Object System.Collections.ArrayList)
        Problems = (New-Object System.Collections.ArrayList)
        Notes = (New-Object System.Collections.ArrayList)
        AfterRestore = $null
        Inventory = { @(Get-CimInstance -ClassName Win32_Process -ErrorAction Stop | Select-Object ProcessId, ParentProcessId, CreationDate) }
        OpenProcess = { param($ProcId) [Diagnostics.Process]::GetProcessById($ProcId) }
        Job = $null
        MarkerPath = $null
        Marker = $null
        MarkerProcs = (New-Object System.Collections.ArrayList)
        PlayerName = $null
        Done = $false
    }
}

function Add-SmokeOwnedProcess {
    param($State, $Proc)
    [void]$State.Procs.Add($Proc)
    if ($null -ne $State.Job) {
        $ok = $false
        try { $ok = [MMOSmokeJobNative]::Assign($State.Job, $Proc.Handle) } catch { $ok = $false }
        if ($ok) { [void]$State.Notes.Add('job_assigned:' + $Proc.Id) } else { [void]$State.Notes.Add('job_assign_failed:' + $Proc.Id) }
    }
    if ($null -ne $State.MarkerPath) {
        [void]$State.MarkerProcs.Add((Get-SmokeProcIdentity $Proc 'owned'))
        if (-not (Update-SmokeMarker $State)) { [void]$State.Notes.Add('marker_update_failed') }
    }
}

function Stop-SmokeOwnedProcess {
    # Yalniz verilen surec NESNESI (ad/PID aramasi yok). Yasiyorsa: ONCE torunlarin tutamaclari acilir ve kimlikleri (olusum zamani) dogrulanir,
    # SONRA kendisi, EN SON dogrulanmis torunlar oldurulur. Kimligi uyusmayan (PID yeniden kullanimi) hicbir surece dokunulmaz.
    param($State, $Proc)
    try {
        if ($Proc.HasExited) { return }
        $held = New-Object System.Collections.ArrayList
        try {
            $rootStart = Get-SmokeProcStartUtc $Proc
            $inv = & $State.Inventory
            foreach ($d in @(Get-SmokeDescendants -Inventory $inv -RootPid ([int]$Proc.Id) -RootStartUtc $rootStart)) {
                $h = $null
                try { $h = & $State.OpenProcess ([int]$d.Pid) } catch { $h = $null }
                if ($null -eq $h) { continue }
                $hs = Get-SmokeProcStartUtc $h
                if (-not (Test-SmokeSameInstant $hs $d.Creation)) { [void]$State.Notes.Add('descendant_identity_mismatch:' + $d.Pid); continue }
                try { $null = $h.Handle } catch { }
                [void]$held.Add($h)
            }
        } catch { $held.Clear() }
        $Proc.Kill()
        if (-not $Proc.WaitForExit(5000)) { [void]$State.Problems.Add('owned_process_not_stopped:' + $Proc.Id) }
        foreach ($k in $held) {
            try { if (-not $k.HasExited) { $k.Kill() } } catch { }
        }
    } catch { [void]$State.Problems.Add('owned_process_kill_error:' + $Proc.Id) }
}

function Add-SmokeLock {
    # Acik tutulan (FileShare.None) dosya tutamagi kaydi: temizlikte salt-okunur geri almadan ONCE kapatilir (kilitli dosyanin ozniteligi degistirilemez / silinemez).
    param($State, [IDisposable]$Handle)
    [void]$State.Locks.Add($Handle)
}

function Set-SmokeReadOnly {
    # ONCE kaydeder (yarim kalirsa bile geri alinsin; kalici isaret dosyasina da yazilir), sonra bayragi koyar.
    param($State, [string]$Path)
    [void]$State.ReadOnly.Add($Path)
    if (-not (Update-SmokeMarker $State)) { [void]$State.Notes.Add('marker_update_failed') }
    [IO.File]::SetAttributes($Path, (([IO.File]::GetAttributes($Path)) -bor [IO.FileAttributes]::ReadOnly))
}

function Restore-SmokeReadOnly {
    param($State)
    foreach ($p in @($State.ReadOnly.ToArray())) {
        try {
            if (-not (Test-Path -LiteralPath $p)) { continue }
            [IO.File]::SetAttributes($p, (([IO.File]::GetAttributes($p)) -band (-bnot [IO.FileAttributes]::ReadOnly)))
            if ((([IO.File]::GetAttributes($p)) -band [IO.FileAttributes]::ReadOnly) -ne 0) { [void]$State.Problems.Add('readonly_restore_failed:' + $p) }
        } catch { [void]$State.Problems.Add('readonly_restore_failed:' + $p) }
    }
}

function Invoke-SmokeCleanup {
    param($State)
    foreach ($p in @($State.Procs.ToArray())) { Stop-SmokeOwnedProcess $State $p }
    if ($State.ContainsKey('Locks')) {
        foreach ($lk in @($State.Locks.ToArray())) { try { $lk.Dispose() } catch { [void]$State.Problems.Add('lock_release_error') } }
        $State.Locks.Clear()
    }
    Restore-SmokeReadOnly $State
    if ($null -ne $State.AfterRestore) {
        try { & $State.AfterRestore } catch { [void]$State.Problems.Add('after_restore_error') }
    }
    Close-SmokeJob $State
    if ($State.Problems.Count -eq 0) { Remove-SmokeMarker $State }
    $State.Done = $true
}

function Invoke-SmokeGuarded {
    # Govde ne olursa olsun (hata, tavan, Ctrl+C) temizlik calisir. Govdenin istisnasi YUKARI atilir.
    param([scriptblock]$Body, $State)
    try { & $Body }
    finally { Invoke-SmokeCleanup $State }
}
