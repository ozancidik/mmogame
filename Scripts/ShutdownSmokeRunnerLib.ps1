# Kapanis duman testi kosucusunun (Scripts/TestShutdownSmoke.ps1) YARDIMCI fonksiyonlari (docs/DESIGN_SHUTDOWN_SMOKE.md, dilim S2/S3).
# Komut satiri kurma, log ayristirma (oyuncu adi, sunucu hazir mi), sure tavani karari, kabul/erteleme karari, cikis kodu esleme,
# yol guvenligi ve temizlik (yalniz KENDI baslatilan surecler, salt-okunur geri alma, olusan dosyalari silme) burada; UE BASLATMAZ.
# Dot-source edilir; kendi basina bir sey calistirmaz. Testi: Scripts/TestShutdownSmokeRunner.ps1 (UE/oyun gerektirmez).
# YASAK (testin kaynak taramasi yakalar): ad ile surec oldurme (Stop-Process, taskkill /IM), kod degistirme.
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII.

$script:SmokeNameRx = '^[A-Za-z0-9_()\-]{1,32}$'

function ConvertTo-SmokeQuotedArg {
    param([string]$Value)
    if ([string]::IsNullOrEmpty($Value)) { throw 'empty argument value' }
    if ($Value -match '["\r\n\x00]') { throw 'argument value contains a quote, CR, LF or NUL' }
    return ('"' + $Value + '"')
}

function Get-SmokeServerArgs {
    # S1 olcumunun komutu: <uproject> <harita> -game -server -NullRHI ... -MMOShutdownAfterSeconds=N -MMOMaintenanceReason=bakim
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
        ('-MMOShutdownAfterSeconds=' + $ShutdownAfter),
        ('-MMOMaintenanceReason=' + $Reason)
    )
}

function Get-SmokeClientArgs {
    # Adres URL'de ?Name= YOK: S1'de yoksayildigi olculdu (oyuncu adi sunucu logundan okunur).
    param([string]$Project, [int]$Port, [string]$LogPath)
    if ($Port -lt 1024 -or $Port -gt 65535) { throw 'bad port' }
    return @(
        (ConvertTo-SmokeQuotedArg $Project),
        ('127.0.0.1:' + $Port),
        '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
        ('-ABSLOG=' + (ConvertTo-SmokeQuotedArg $LogPath))
    )
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

function Test-SmokeClosingSeen {
    param([AllowNull()][string]$ServerLogText)
    if ([string]::IsNullOrEmpty($ServerLogText)) { return $false }
    return [regex]::IsMatch($ServerLogText, 'event=shutdown_phase phase=Closing ')
}

function Get-SmokeBusyProcesses {
    # Girdi: {ProcessId; Name; CommandLine} satirlari. Dondurur: baska UE/UBT/Live Coding/MyProject isi calisiyorsa o satirlar (kosucu ERTELER).
    param([AllowNull()][object[]]$Inventory)
    $busy = New-Object System.Collections.ArrayList
    foreach ($p in @($Inventory)) {
        if ($null -eq $p) { continue }
        $name = [string]$p.Name
        $cmd = [string]$p.CommandLine
        $hit = $false
        if ($name -match '^(UnrealEditor.*|LiveCoding.*|UnrealBuildTool.*|AutomationTool.*|MSBuild|UAT)\.exe$') { $hit = $true }
        elseif ($name -ieq 'dotnet.exe' -and ([string]::IsNullOrWhiteSpace($cmd) -or $cmd -match 'UnrealBuildTool|AutomationTool|RunUAT|Epic Games[\\/]UE_')) { $hit = $true }
        elseif ($cmd -match 'MyProject\.uproject' -and $name -imatch '^(UnrealEditor|UE4Editor|CrashReportClient).*\.exe$') { $hit = $true }
        if ($hit) { [void]$busy.Add($p) }
    }
    return $busy.ToArray()
}

function Get-SmokeDescendantPids {
    # Sahip surecin torunlari (yalniz verilen kok PID'den inen zincir); envanter {ProcessId; ParentProcessId}.
    param([AllowNull()][object[]]$Inventory, [int]$RootPid)
    $found = New-Object 'System.Collections.Generic.List[int]'
    $frontier = New-Object 'System.Collections.Generic.Queue[int]'
    $frontier.Enqueue($RootPid)
    $guard = 0
    while ($frontier.Count -gt 0 -and $guard -lt 5000) {
        $guard++
        $cur = $frontier.Dequeue()
        foreach ($p in @($Inventory)) {
            if ($null -eq $p) { continue }
            $pp = [int]$p.ParentProcessId
            $id = [int]$p.ProcessId
            if ($pp -eq $cur -and $id -ne $RootPid -and -not $found.Contains($id)) { $found.Add($id); $frontier.Enqueue($id) }
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
    param([string]$RepoRoot)
    $g = Join-Path $RepoRoot '.git'
    if (Test-Path -LiteralPath $g -PathType Leaf) { return $false }
    return $true
}

function Test-SmokeReparse {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $false }
    return ((([IO.File]::GetAttributes($Path)) -band [IO.FileAttributes]::ReparsePoint) -ne 0)
}

function Get-SmokeCreatedFiles {
    # Kosu sirasinda OLUSAN dosyalar: oncesinde yok + sonrasinda var + adi oyuncu adini iceriyor. Baska hicbir dosya silinmez.
    param([string[]]$Before, [string[]]$After, [string]$PlayerName)
    if ([string]::IsNullOrEmpty($PlayerName) -or $PlayerName -cnotmatch $script:SmokeNameRx) { return @() }
    $b = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($x in @($Before)) { [void]$b.Add([string]$x) }
    $out = New-Object System.Collections.ArrayList
    foreach ($x in @($After)) {
        $s = [string]$x
        if ($b.Contains($s)) { continue }
        if ([IO.Path]::GetFileName($s).IndexOf($PlayerName, [StringComparison]::OrdinalIgnoreCase) -lt 0) { continue }
        [void]$out.Add($s)
    }
    return $out.ToArray()
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

# ---- temizlik: yalniz KENDI baslatilan surecler, salt-okunur geri alma ---------------------------------------------------------
function New-SmokeState {
    return @{
        Procs = (New-Object System.Collections.ArrayList)
        ReadOnly = (New-Object System.Collections.ArrayList)
        Problems = (New-Object System.Collections.ArrayList)
        AfterRestore = $null
        Inventory = { @(Get-CimInstance -ClassName Win32_Process -ErrorAction Stop | Select-Object ProcessId, ParentProcessId) }
        Done = $false
    }
}

function Add-SmokeOwnedProcess {
    param($State, $Proc)
    [void]$State.Procs.Add($Proc)
}

function Stop-SmokeOwnedProcess {
    # Yalniz verilen surec NESNESI (ad/PID aramasi yok). Yasiyorsa once torunlari (kendi agaci), sonra kendisini oldurur.
    param($State, $Proc)
    try {
        if ($Proc.HasExited) { return }
        $kids = @()
        try { $kids = @(Get-SmokeDescendantPids -Inventory (& $State.Inventory) -RootPid ([int]$Proc.Id)) } catch { $kids = @() }
        $Proc.Kill()
        if (-not $Proc.WaitForExit(5000)) { [void]$State.Problems.Add('owned_process_not_stopped:' + $Proc.Id) }
        foreach ($k in $kids) {
            try { $kp = [Diagnostics.Process]::GetProcessById($k); $kp.Kill() } catch { }
        }
    } catch { [void]$State.Problems.Add('owned_process_kill_error:' + $Proc.Id) }
}

function Set-SmokeReadOnly {
    # ONCE kaydeder (yarim kalirsa bile geri alinsin), sonra bayragi koyar.
    param($State, [string]$Path)
    [void]$State.ReadOnly.Add($Path)
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
    Restore-SmokeReadOnly $State
    if ($null -ne $State.AfterRestore) {
        try { & $State.AfterRestore } catch { [void]$State.Problems.Add('after_restore_error') }
    }
    $State.Done = $true
}

function Invoke-SmokeGuarded {
    # Govde ne olursa olsun (hata, tavan, Ctrl+C) temizlik calisir. Govdenin istisnasi YUKARI atilir.
    param([scriptblock]$Body, $State)
    try { & $Body }
    finally { Invoke-SmokeCleanup $State }
}
