# Kayit dosyalari GERI YUKLEME (docs/DESIGN_BACKUP.md, dilim B3). OYUNCU VERISININ UZERINE YAZAN TEK ARAC: fail-closed, KURU CALISMA VARSAYILAN.
# Bir anlik goruntuden (BackupSaves.ps1 ciktisi) tum agaci (-All) ya da secili karakter(ler)i (-Slots a,b) Saved\ altina geri koyar.
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII.
#
# KURU CALISMA (varsayilan, -Apply YOK): hicbir sey yazmaz / tasimaz / silmez (log ve durum dosyasi dahil); plani yazar (hangi dosya, hangi slot,
#   yazilacak / ayni / atlanacak ve neden) ve manifest.json dosyasinin TAM SHA-256 ozetini basar. Operator bu ozeti yedek alinirken kaydettigi
#   ozetle (backup.log manifest_sha256, last_ok.json) karsilastirir. -Apply yalniz -ExpectedManifestSha256 verilir VE eslesirse calisir.
# KAPILAR (hepsi uygulamadan once; herhangi biri basarisizsa HICBIR dosyaya dokunulmaz):
#   1 Test-MMOSnapshot (boyut, SHA-256, valid/notes yeniden hesabi, fazladan dosya, reparse, header/entries digest, yarim goruntu) + manifest ozeti.
#   2 valid=false / unstable girisler ve .tmp/.bak/.bad/.tmp.bad yan dosyalar VARSAYILAN geri yuklenmez (-IncludeInvalid / -IncludeSideFiles, [MMO_RESTORE_FORCE]).
#   3 mode=live goruntu RED (-AcceptLiveSnapshot); manifestte bildirilmis cakisan adlar (collisions) varken RED.
#   4 kaynak koku: manifest source_saved_dir hedef Saved ile ayni olmali (-AcceptDifferentSource; imzasiz oldugu icin bilgi amaclidir).
#   5 -Slots: -AcknowledgeCrossSlotRisk ZORUNLU (posta/ticaret altin cogaltma/kaybi riski). Tum-goruntu geri yukleme de ayni uyariyi yazar.
#   6 Yazimdan HEMEN ONCE (iki kez: pre-restore oncesi ve ilk yazimdan once) surec denetimi (UnrealEditor/MyProjectServer/UBT; sorgu hatasi/bos = 3)
#     ve hedef Saved altinda son -RecentWriteSeconds saniyede degisen dosya var mi (canli yazim belirtisi; var = 3).
#   7 PRE-RESTORE GORUNTUSU zorunlu ve ONCE: BackupSaves.ps1 ile '<ts>-prerestore' goruntusu; alinamaz ya da dogrulanamaz ya da uzerine yazilacak
#     bir canli dosyanin icerigi goruntude yoksa HICBIR dosyaya dokunulmaz. (Hedefte hic canli dosya yoksa pre-restore gerekmez: 'preimage=none'.)
#   8 Yazim: her dosya TEK okuma bayt dizisi (hash dogrulanir) -> hedef dizinde <ad>.restore.tmp (CreateNew) -> yeniden oku + hash -> Replace/Move.
#     Mevcut dosyaya dogrudan yazim YOK (sert baglanti / ADS tuzagi). Hedefte ayni kanonik anahtarli baska yazimli dosya = RED.
#   9 Herhangi bir dosya yazimi basarisizsa kalan dosyalara DEVAM EDILMEZ; yazilanlar pre-restore goruntusunden geri alinir (rollback; bu kosunun
#     KENDI olusturdugu yeni dosyalar silinir, baska hicbir dosya silinmez) ve cikis 1. Rollback basarisizsa acik hata + pre-restore goruntu adi.
# SILME YOK: hedefte goruntude olmayan dosyalar silinmez, yalniz raporlanir. Goruntude olmayan canli .tmp/.bak yan dosyalari (oyunun kurtarma mantigi
#   eski artigi geri getirebilir) 'stale_side_files' uyarisi ve cikis 4 verir; tasimak/silmek operator isidir (docs/DESIGN_BACKUP.md B3 notu).
#
# CIKIS KODU (BackupSaves.ps1 / VerifySaves.ps1 sozlesmesiyle uyumlu): 0 tamam; 1 basarisiz / reddedildi (dogrulama, kapi, yazim hatasi + rollback);
#   2 kullanim hatasi; 3 ertelendi (surec / canli yazim / yol yapisi / kaynak-hedef erisilemiyor: HICBIR dosyaya dokunulmadi); 4 basari ama uyarili
#   (neden(ler) '+' ile: forced, live, different_source, stale_side_files, entries_not_restored, proof_stale, snapshot_ignored_player_data).
# Makine okunur: [MMO_RESTORE_MANIFEST_SHA256], [MMO_RESTORE_SNAPSHOT|PLAN|WARN|FORCE|EXTRA|SUMMARY|PRE|ROLLBACK_FAILED], son satir [MMO_RESTORE_RESULT].
# Konsol oyuncu adlari icerir; restore.log (yalniz -Apply, pre-restore goruntu dizininde) ve last_restore.json (<BackupRoot>) YALNIZ neden kodu ve
# sayilar icerir (oyuncu adi / tam yol YOK).
param(
    [string]$Snapshot = '',
    [string]$SavedDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved'),
    [string]$BackupRoot = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'MMOBackups'),
    [switch]$All,
    [string[]]$Slots = @(),
    [switch]$Apply,
    [string]$ExpectedManifestSha256 = '',
    [switch]$AcknowledgeCrossSlotRisk,
    [switch]$IncludeInvalid,
    [switch]$IncludeSideFiles,
    [switch]$AcceptLiveSnapshot,
    [switch]$AcceptDifferentSource,
    [int]$RecentWriteSeconds = 60,
    [switch]$AllowRootAncestorReparse,
    # YALNIZ TEST (MMO_BACKUP_TESTMODE=1 gerekir, aksi halde kullanim hatasi 2). ProcessProvider: Name/ProcessId/CommandLine uyeli nesneler (BackupSaves ile ayni);
    # Timestamp: pre-restore goruntusu icin sabit yyyyMMddTHHmmssZ; TestHooks: BeforeFirstWrite {param($preDir)}, AfterPreimage {param($preDir)},
    # BeforeFile {param($rel,$idx)}, AfterTmp {param($rel,$tmp,$dest)}, AfterWrite {param($rel,$dest)}, BeforeRollback {param($preDir)}.
    [scriptblock]$ProcessProvider = $null,
    [string]$Timestamp = '',
    [hashtable]$TestHooks = $null
)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')

$script:TestMode = ($env:MMO_BACKUP_TESTMODE -ceq '1')
$script:TestParamUsed = $PSBoundParameters.ContainsKey('ProcessProvider') -or $PSBoundParameters.ContainsKey('Timestamp') -or $PSBoundParameters.ContainsKey('TestHooks')
$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:Notes = New-Object System.Collections.ArrayList
$script:Log = New-Object System.Collections.ArrayList
$script:SavedFull = $null
$script:RootFull = $null
$script:SnapFull = $null
$script:SnapName = 'none'
$script:ManifestSha = 'none'
$script:PreName = 'none'
$script:PreDir = $null
$script:ProcessCheck = 'none'
$script:Attempted = $false

function Stop-Restore {
    param([int]$Code, [string]$Reason)
    throw ('MMORS|' + $Code + '|' + $Reason)
}

function Write-RsLine {
    # Konsol + (yalniz kod duzeyli) log. $LogText verilmezse log'a yazilmaz (oyuncu adi / yol iceren satirlar).
    param([string]$Tag, [string]$Text, [string]$LogText = $null)
    Write-Host ('[MMO_RESTORE' + $Tag + '] ' + $Text)
    if ($null -ne $LogText) { [void]$script:Log.Add('[MMO_RESTORE' + $Tag + '] ' + $LogText) }
}

function Add-RsNote {
    param([string]$Note)
    if (-not $script:Notes.Contains($Note)) { [void]$script:Notes.Add($Note) }
}

function Get-RsBlockers {
    # Surec denetimi (UE/derleme). Err: $null | process_query_failed | process_query_empty | process_running.
    $procs = $null
    $qerr = $false
    try {
        if ($null -ne $ProcessProvider) { $procs = @(& $ProcessProvider | Where-Object { $null -ne $_ }) }
        else { $procs = @(Get-CimInstance -ClassName Win32_Process -ErrorAction Stop | Where-Object { $null -ne $_ }) }
    } catch { $qerr = $true }
    if ($qerr) { return [pscustomobject]@{ Err = 'process_query_failed'; Blockers = @() } }
    if ($procs.Count -eq 0) { return [pscustomobject]@{ Err = 'process_query_empty'; Blockers = @() } }
    $blk = @(Get-MMOBlockingProcesses -Processes $procs)
    if ($blk.Count -gt 0) { return [pscustomobject]@{ Err = 'process_running'; Blockers = $blk } }
    return [pscustomobject]@{ Err = $null; Blockers = @() }
}

function Get-RsRecentWrites {
    # Hedef Saved agacinda (reparse izlenmez) son $Seconds saniyede degisen dosya sayisi. Listeleme hatasi = -1 (fail-closed).
    param([string]$SavedFull, [int]$Seconds)
    $cut = [datetime]::UtcNow.AddSeconds(-$Seconds)
    $n = 0
    $stack = New-Object System.Collections.Stack
    $stack.Push($SavedFull)
    while ($stack.Count -gt 0) {
        $dir = [string]$stack.Pop()
        $items = $null
        try { $items = @(Get-ChildItem -LiteralPath $dir -Force -ErrorAction Stop) } catch { return -1 }
        foreach ($it in $items) {
            if ($it.PSIsContainer) {
                if (-not (Test-MMOReparse $it.FullName)) { $stack.Push($it.FullName) }
                continue
            }
            $t = [IO.File]::GetLastWriteTimeUtc($it.FullName)
            if ($t -gt $cut) { $n++ }
        }
    }
    return $n
}

function Assert-RsQuiet {
    # Yazimdan hemen once: surec yok + canli yazim belirtisi yok. Hata = 3.
    $pc = Get-RsBlockers
    if ($null -ne $pc.Err) {
        foreach ($b in $pc.Blockers) { Write-RsLine '_BLOCKER' ('process=' + $b.Name + ' pid=' + $b.ProcessId) }
        Stop-Restore 3 $pc.Err
    }
    $rw = Get-RsRecentWrites -SavedFull $script:SavedFull -Seconds $RecentWriteSeconds
    if ($rw -lt 0) { Stop-Restore 3 'recent_write_check_failed' }
    if ($rw -gt 0) { Stop-Restore 3 'recent_write_detected' }
}

function Get-RsLiveFiles {
    # Hedef Saved'in beyaz listedeki bes dizininin DOGRUDAN altindaki dosyalar (BackupSaves ile ayni kume). Reparse = 3, listeleme hatasi = 1.
    param([string]$SavedFull)
    $files = New-Object System.Collections.ArrayList
    foreach ($d in 'SaveGames', 'MMOAchievements', 'MMODaily', 'MMOMail', 'MMORace') {
        $p = $SavedFull + '\' + $d
        $a = Get-MMOAttributes $p
        if ($a.State -eq 'missing') { continue }
        if ($a.State -ne 'ok') { Stop-Restore 3 'target_inaccessible' }
        if (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { Stop-Restore 3 'target_reparse' }
        if (($a.Attributes -band [IO.FileAttributes]::Directory) -eq 0) { Stop-Restore 3 'target_not_directory' }
        $items = $null
        try { $items = @(Get-ChildItem -LiteralPath $p -Force -ErrorAction Stop) } catch { $items = $null }
        if ($null -eq $items) { Stop-Restore 1 'target_tree_unreadable' }
        foreach ($it in $items) {
            if (Test-MMOReparse $it.FullName) { Stop-Restore 3 'target_reparse_entry' }
            if ($it.PSIsContainer) { continue }
            $rel = $d + '/' + $it.Name
            $c = Get-MMOSaveFileClass $rel
            if ($c.Class -eq 'ignored') { continue }
            [void]$files.Add($rel)
        }
    }
    $arr = [string[]]@($files.ToArray())
    [Array]::Sort($arr, [StringComparer]::Ordinal)
    return , $arr
}

function Write-RsFile {
    # Tek dosya: hedef dizinde <ad>.restore.tmp (CreateNew; var olana yazmaz) -> yeniden oku + hash -> Replace (mevcut) / Move (yeni).
    # Hatada KENDI olusturdugu .tmp silinir. Hata = throw (kod).
    param([string]$Dest, [byte[]]$Bytes, [string]$Sha, [string]$Rel)
    $tmp = $Dest + '.restore.tmp'
    if ((Get-MMOAttributes $tmp).State -ne 'missing') { throw 'restore_tmp_exists' }
    $dir = [IO.Path]::GetDirectoryName($Dest)
    if (-not [IO.Directory]::Exists($dir)) {
        try { [void][IO.Directory]::CreateDirectory($dir) } catch { throw 'target_dir_create_failed' }
    }
    if (Test-MMOReparse $dir) { throw 'target_dir_reparse' }
    $created = $false
    $fs = $null
    $err = $null
    try {
        $fs = New-Object IO.FileStream($tmp, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $created = $true
        $fs.Write($Bytes, 0, $Bytes.Length)
        $fs.Flush($true)
    } catch { $err = 'tmp_write_failed' } finally { if ($null -ne $fs) { $fs.Dispose() } }
    try {
        if ($null -ne $err) { throw $err }
        if ($null -ne $TestHooks -and $TestHooks.ContainsKey('AfterTmp')) { & $TestHooks['AfterTmp'] $Rel $tmp $Dest }
        $chk = Get-MMOFileSha256 $tmp
        if ($chk -cne $Sha) { throw 'tmp_verify_failed' }
        if (Test-MMOReparse $Dest) { throw 'dest_reparse' }
        if ([IO.File]::Exists($Dest)) { [IO.File]::Replace($tmp, $Dest, [NullString]::Value) }
        else { [IO.File]::Move($tmp, $Dest) }
    } catch {
        $m = $_.Exception.Message
        if ($created -and [IO.File]::Exists($tmp)) { try { [IO.File]::Delete($tmp) } catch { } }
        if ($m -cmatch '^[a-z_]+\z') { throw $m }
        throw 'place_failed'
    }
}

function Invoke-RsRollback {
    # Yazilanlari ters sirayla geri al: mevcut dosya = pre-restore goruntusunden (hash dogrulanarak), bu kosunun yarattigi yeni dosya = sil (yalniz
    # hala bizim yazdigimiz bayt ise). Hata olsa da kalanlara devam eder. Donus: tum geri almalar basarili mi.
    param($Written, [string]$PreDir)
    $ok = $true
    for ($i = $Written.Count - 1; $i -ge 0; $i--) {
        $it = $Written[$i]
        try {
            if ($it.Target -ceq 'overwrite') {
                if ($null -eq $PreDir) { throw 'no_preimage' }
                $pb = Read-MMOFileBytes ($PreDir + '\files\' + ([string]$it.Rel).Replace('/', '\'))
                if ((Get-MMOBytesSha256 $pb) -cne $it.PrevSha) { throw 'preimage_hash_mismatch' }
                Write-RsFile -Dest $it.Dest -Bytes $pb -Sha $it.PrevSha -Rel $it.Rel
            } else {
                $cur = Read-MMOFileBytes $it.Dest
                if ((Get-MMOBytesSha256 $cur) -cne $it.Sha) { throw 'new_file_changed' }
                [IO.File]::Delete($it.Dest)
            }
        } catch { $ok = $false }
    }
    return $ok
}

function Invoke-RsPreimage {
    # BackupSaves.ps1 ile '<ts>-prerestore' goruntusu. Donus: goruntu dizini. Hata = Stop (HICBIR dosya henuz yazilmadi).
    $bk = Join-Path $PSScriptRoot 'BackupSaves.ps1'
    $p = @{ SavedDir = $script:SavedFull; BackupRoot = $script:RootFull; Label = 'prerestore'; AllowIgnored = $true }
    if ($AllowRootAncestorReparse) { $p['AllowRootAncestorReparse'] = $true }
    if ($null -ne $ProcessProvider) { $p['ProcessProvider'] = $ProcessProvider }
    if ($Timestamp -ne '') { $p['Timestamp'] = $Timestamp }
    $global:LASTEXITCODE = -999
    $lines = @(& $bk @p *>&1 | ForEach-Object { [string]$_ })
    $code = $global:LASTEXITCODE
    foreach ($ln in $lines) { Write-Host ('[MMO_RESTORE_PRE] ' + $ln) }
    $name = $null
    foreach ($ln in $lines) {
        if ($ln -cmatch '^\[MMO_BACKUP_RESULT\] .* snapshot=(?<s>\S+)( |\z)') { $name = $Matches['s'] }
    }
    if ($null -ne $name -and $name -cmatch '^[0-9]{8}T[0-9]{6}Z-prerestore\z') {
        $script:PreName = $name
        $script:PreDir = $script:RootFull + '\' + $name
    }
    if ($code -ne 0 -and $code -ne 4) {
        if ($code -eq 3) { Stop-Restore 3 'preimage_deferred' }
        Stop-Restore 1 'preimage_failed'
    }
    if ($null -eq $script:PreDir) { Stop-Restore 1 'preimage_unnamed' }
    return $script:PreDir
}

function Write-RsJsonAtomic {
    # last_restore.json: kendi .tmp'si (CreateNew) -> Replace/Move. Hedef reparse ise yazilmaz. Basari = $true.
    param([string]$Path, $Object)
    if (Test-MMOReparse $Path) { return $false }
    $json = $Object | ConvertTo-Json -Depth 4
    $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($json)
    $tmp = $Path + '.' + $PID + '.tmp'
    $fs = $null
    $err = $null
    try {
        $fs = New-Object IO.FileStream($tmp, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $fs.Write($bytes, 0, $bytes.Length)
        $fs.Flush($true)
    } catch { $err = $_.Exception.Message } finally { if ($null -ne $fs) { $fs.Dispose() } }
    if ($null -ne $err) { return $false }
    try {
        if ([IO.File]::Exists($Path)) { [IO.File]::Replace($tmp, $Path, [NullString]::Value) } else { [IO.File]::Move($tmp, $Path) }
    } catch {
        try { [IO.File]::Delete($tmp) } catch { }
        return $false
    }
    return $true
}

function Get-RsStatusRoot {
    # last_restore.json icin MEVCUT guvenli yedek koku (olusturulmaz): Saved/repo icinde degil, dizin, reparse degil. Aksi $null.
    if ($null -eq $script:RootFull -or $null -eq $script:SavedFull) { return $null }
    if (Test-MMOPathUnder -Path $script:RootFull -Root $script:SavedFull -AllowEqual -ErrorIsViolation) { return $null }
    if (Test-MMOPathUnder -Path $script:SavedFull -Root $script:RootFull -AllowEqual -ErrorIsViolation) { return $null }
    if (Test-MMOPathUnder -Path $script:RootFull -Root $script:RepoRoot -AllowEqual -ErrorIsViolation) { return $null }
    $a = Get-MMOAttributes $script:RootFull
    if ($a.State -ne 'ok') { return $null }
    if (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { return $null }
    if (($a.Attributes -band [IO.FileAttributes]::Directory) -eq 0) { return $null }
    if (-not $AllowRootAncestorReparse -and $null -ne (Get-MMOReparseAncestor $script:RootFull)) { return $null }
    return $script:RootFull
}

function Invoke-RestoreMain {
    # 0. Test parametreleri yalniz test modunda
    if ($script:TestParamUsed -and -not $script:TestMode) { Stop-Restore 2 'test_parameter_requires_testmode' }
    # 1. Parametreler (kullanim hatasi = 2)
    if ($Snapshot -eq '') { Stop-Restore 2 'snapshot_required' }
    $slotKeys = New-Object System.Collections.ArrayList
    foreach ($s0 in @($Slots)) {
        foreach ($s in ([string]$s0).Split(',')) {
            $k = ConvertTo-MMOCanonicalKey $s
            if ($null -eq $k -or -not (Test-MMOFileKey $k 32)) { Stop-Restore 2 'bad_slot_name' }
            if (-not $slotKeys.Contains($k)) { [void]$slotKeys.Add($k) }
        }
    }
    $useSlots = ($slotKeys.Count -gt 0)
    if ($All -and $useSlots) { Stop-Restore 2 'all_and_slots_exclusive' }
    if (-not $All -and -not $useSlots) { Stop-Restore 2 'scope_required_all_or_slots' }
    if ($useSlots -and -not $AcknowledgeCrossSlotRisk) { Stop-Restore 2 'slots_require_acknowledge_cross_slot_risk' }
    if ($RecentWriteSeconds -lt 1 -or $RecentWriteSeconds -gt 86400) { Stop-Restore 2 'bad_recent_write_seconds' }
    $expectSha = ''
    if ($ExpectedManifestSha256 -ne '') {
        if ($ExpectedManifestSha256 -cnotmatch '^[0-9A-Fa-f]{64}\z') { Stop-Restore 2 'bad_expected_sha' }
        $expectSha = $ExpectedManifestSha256.ToLowerInvariant()
    }
    if ($Apply -and $expectSha -eq '') { Stop-Restore 2 'apply_requires_expected_manifest_sha256' }
    if ($Timestamp -ne '') {
        $rd = [datetime]::MinValue
        if ($Timestamp -cnotmatch '^[0-9]{8}T[0-9]{6}Z\z' -or -not [datetime]::TryParseExact($Timestamp, "yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None, [ref]$rd)) { Stop-Restore 2 'bad_timestamp' }
    }
    try {
        $script:SavedFull = (ConvertTo-MMOFullPath $SavedDir).TrimEnd('\')
        $script:RootFull = (ConvertTo-MMOFullPath $BackupRoot).TrimEnd('\')
        $script:SnapFull = (ConvertTo-MMOFullPath $Snapshot).TrimEnd('\')
    } catch { Stop-Restore 2 'path_invalid' }
    $savedFull = $script:SavedFull
    $snapFull = $script:SnapFull
    $script:SnapName = [IO.Path]::GetFileName($snapFull)
    if ($Apply) { $script:Attempted = $true }

    # 2. Yol yapisi: goruntu Saved icinde olamaz; hedef Saved gecerli kok (var, reparse degil)
    if (Test-MMOPathUnder -Path $snapFull -Root $savedFull -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'snapshot_inside_saved' }
    $srcErr = $null
    try { [void](Assert-MMORootSafe -Root $savedFull -AllowRootAncestorReparse:$AllowRootAncestorReparse) } catch { $srcErr = $_.Exception.Message }
    if ($null -ne $srcErr) { Stop-Restore 3 ('saved_' + ($srcErr -split ':', 2)[0]) }
    $sa = Get-MMOAttributes $snapFull
    if ($sa.State -eq 'missing') { Stop-Restore 3 'snapshot_missing' }
    if ($sa.State -ne 'ok' -or (($sa.Attributes -band [IO.FileAttributes]::Directory) -eq 0)) { Stop-Restore 3 'snapshot_inaccessible' }
    if (($sa.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { Stop-Restore 3 'snapshot_reparse' }
    if (-not $AllowRootAncestorReparse -and $null -ne (Get-MMOReparseAncestor $snapFull)) { Stop-Restore 3 'snapshot_reparse_ancestor' }

    # 3. Goruntu dogrulamasi (kapi 1): yarim goruntu kalintisi, manifest ozeti (once/sonra), Test-MMOSnapshot
    foreach ($leftover in 'manifest.json.failed', 'manifest.json.tmp') {
        if ((Get-MMOAttributes ($snapFull + '\' + $leftover)).State -ne 'missing') { Stop-Restore 1 'snapshot_incomplete' }
    }
    $mpath = $snapFull + '\manifest.json'
    if (-not [IO.File]::Exists($mpath)) { Stop-Restore 1 'snapshot_incomplete' }
    $sha1 = $null
    try { $sha1 = Get-MMOFileSha256 $mpath } catch { Stop-Restore 1 'manifest_unreadable' }
    $rm = Read-MMOManifest -SnapshotDir $snapFull -AllowRootAncestorReparse:$AllowRootAncestorReparse
    $t = Test-MMOSnapshot -SnapshotDir $snapFull -AllowRootAncestorReparse:$AllowRootAncestorReparse
    $sha2 = $null
    try { $sha2 = Get-MMOFileSha256 $mpath } catch { Stop-Restore 1 'manifest_unreadable' }
    $script:ManifestSha = $sha1
    Write-RsLine '_MANIFEST_SHA256' ('sha256=' + $sha1 + ' snapshot=' + $script:SnapName) ('sha256_recorded=true')
    if ($sha1 -cne $sha2) { Stop-Restore 1 'manifest_changed_during_verify' }
    if (-not $rm.Ok -or -not $t.Ok) {
        $errs = @($rm.Errors) + @($t.Errors)
        foreach ($e in $errs) { Write-RsLine '_ERROR' ([string]$e) }
        if ($t.Incomplete -or $rm.Incomplete) { Stop-Restore 1 'snapshot_incomplete' }
        Stop-Restore 1 'snapshot_verify_failed'
    }
    if ($expectSha -ne '' -and $sha1 -cne $expectSha) { Stop-Restore 1 'manifest_sha256_mismatch' }
    $m = $rm.Manifest

    # 4. Goruntu ozellikleri: live, cakisma, kaynak koku, uyarilar
    $mode = [string](Get-MMOMember $m 'mode')
    $proofStale = ((Get-MMOMember $m 'proof_stale') -eq $true)
    $ignSummary = [string](Get-MMOMember $m 'ignored_summary')
    $ignPd = 0
    if ($ignSummary -cmatch 'player_data=(?<n>[0-9]+)') { $ignPd = [int]$Matches['n'] }
    $collList = @()
    $cv = Get-MMOMember $m 'collisions'
    if ($null -ne $cv) { $collList = [string[]]@(@($cv) | ForEach-Object { [string]$_ }) }
    $srcDir = ([string](Get-MMOMember $m 'source_saved_dir')).TrimEnd('\')
    $srcMatch = $srcDir.Equals($savedFull, [StringComparison]::OrdinalIgnoreCase)
    Write-RsLine '_SNAPSHOT' ('name=' + $script:SnapName + ' mode=' + $mode + ' created_utc=' + (ConvertTo-MMOUtcString (Get-MMOMember $m 'created_utc')) + ' entries=' + @($m.entries).Count + ' source_matches=' + ([string]$srcMatch).ToLowerInvariant() + ' proof_stale=' + ([string]$proofStale).ToLowerInvariant() + ' collisions=' + $collList.Count + ' ignored_player_data=' + $ignPd) `
        ('mode=' + $mode + ' entries=' + @($m.entries).Count + ' source_matches=' + ([string]$srcMatch).ToLowerInvariant() + ' proof_stale=' + ([string]$proofStale).ToLowerInvariant() + ' collisions=' + $collList.Count)
    if ($collList.Count -gt 0) {
        foreach ($cg in $collList) { Write-RsLine '_WARN' ('code=collisions group=' + $cg) }
        Stop-Restore 1 'snapshot_has_collisions'
    }
    if ($mode -ceq 'live') {
        if (-not $AcceptLiveSnapshot) { Write-RsLine '_WARN' 'code=live_snapshot (tutarsiz olabilir; bilincli kabul icin -AcceptLiveSnapshot)'; Stop-Restore 1 'live_snapshot_requires_flag' }
        Write-RsLine '_FORCE' 'flag=AcceptLiveSnapshot reason=live_snapshot' 'flag=AcceptLiveSnapshot'
        Add-RsNote 'live'
    }
    if (-not $srcMatch) {
        if (-not $AcceptDifferentSource) { Write-RsLine '_WARN' ('code=source_mismatch manifest_source=' + $srcDir); Stop-Restore 1 'source_mismatch' }
        Write-RsLine '_FORCE' 'flag=AcceptDifferentSource reason=source_mismatch' 'flag=AcceptDifferentSource'
        Add-RsNote 'different_source'
    }
    if ($proofStale) { Write-RsLine '_WARN' 'code=manifest_proof_stale (temiz kapanis kanitindan sonra kaynak degismis)'; Add-RsNote 'proof_stale' }
    if ($ignPd -gt 0) { Write-RsLine '_WARN' ('code=manifest_ignored_player_data count=' + $ignPd + ' (yedek alinirken ad kuraliyla dislanan oyuncu verisi olabilir)'); Add-RsNote 'snapshot_ignored_player_data' }
    if ($IncludeSideFiles) { Write-RsLine '_FORCE' 'flag=IncludeSideFiles' 'flag=IncludeSideFiles' }

    # 5. Secim (kapi 2/3): yan dosya / valid=false / unstable / no_content varsayilan disarida; kapsam -All ya da -Slots
    $entries = @($m.entries)
    $entryByPath = @{}
    foreach ($e in $entries) { $entryByPath[[string]$e.path] = $e }
    $sel = Get-MMORestoreSelection -Entries $entries -IncludeSideFiles:$IncludeSideFiles -IncludeInvalid:$IncludeInvalid -CollisionPaths $collList
    if ($sel.Forced.Count -gt 0) { Add-RsNote 'forced'; [void]$script:Log.Add('[MMO_RESTORE_FORCE] flag=IncludeInvalid count=' + $sel.Forced.Count) }
    $inScope = {
        param([string]$rel)
        if (-not $useSlots) { return $true }
        $c = Get-MMOSaveFileClass $rel
        return ($slotKeys.Contains([string]$c.Group))
    }
    if ($useSlots) {
        foreach ($sk in $slotKeys) {
            $found = $false
            foreach ($e in $entries) { if ((Get-MMOSaveFileClass ([string]$e.path)).Group -ceq $sk) { $found = $true; break } }
            if (-not $found) { Write-RsLine '_WARN' ('code=slot_not_in_snapshot slot=' + $sk); Stop-Restore 1 'slot_not_in_snapshot' }
        }
    }
    $restoreRels = New-Object System.Collections.ArrayList
    foreach ($r in $sel.Restore) { if (& $inScope $r) { [void]$restoreRels.Add($r) } }
    $notRestored = 0
    $skipSide = 0
    $skipList = New-Object System.Collections.ArrayList
    foreach ($sk in $sel.Skipped) {
        if (-not (& $inScope $sk.Path)) { continue }
        [void]$skipList.Add($sk)
        if ($sk.Reason -ceq 'side_file_requires_flag') { $skipSide++ } else { $notRestored++ }
    }
    if ($notRestored -gt 0) { Add-RsNote 'entries_not_restored' }
    if ($restoreRels.Count -eq 0) { Write-RsLine '_WARN' 'code=nothing_to_restore'; Stop-Restore 1 'nothing_to_restore' }

    # 6. Hedef: canli dosyalar, kok kontrolu, yazim plani
    $live = Get-RsLiveFiles -SavedFull $savedFull
    $dirNames = @{}
    foreach ($lf in $live) {
        $dn = ($lf -split '/', 2)[0]
        if (-not $dirNames.ContainsKey($dn)) { $dirNames[$dn] = New-Object System.Collections.ArrayList }
        [void]$dirNames[$dn].Add(($lf -split '/', 2)[1])
    }
    $plan = New-Object System.Collections.ArrayList
    $sortedRels = [string[]]@($restoreRels.ToArray())
    [Array]::Sort($sortedRels, [StringComparer]::Ordinal)
    foreach ($rel in $sortedRels) {
        $e = $entryByPath[$rel]
        $c = Get-MMOSaveFileClass $rel
        $dest = $null
        try { $dest = Resolve-MMOSafePath -Root $savedFull -RelPath $rel -AllowRootAncestorReparse:$AllowRootAncestorReparse } catch { Write-RsLine '_WARN' ('code=target_path_unsafe path=' + $rel); Stop-Restore 1 'target_path_unsafe' }
        # Hedefte ayni kanonik anahtarli (tur, sonek, grup) ama FARKLI yazimli dosya (Ali / ali): Windows'ta ayni dosya = RED
        $dn = ($rel -split '/', 2)[0]
        $fn = ($rel -split '/', 2)[1]
        if ($dirNames.ContainsKey($dn)) {
            foreach ($other in $dirNames[$dn]) {
                if ($other -ceq $fn) { continue }
                $oc = Get-MMOSaveFileClass ($dn + '/' + $other)
                if ($oc.Class -ne 'ignored' -and $oc.Kind -ceq $c.Kind -and $oc.Suffix -ceq $c.Suffix -and $oc.Group -ceq $c.Group) {
                    Write-RsLine '_WARN' ('code=target_name_collision path=' + $rel + ' other=' + $other)
                    Stop-Restore 1 'target_name_collision'
                }
            }
        }
        $da = Get-MMOAttributes $dest
        $target = 'new'
        $prevSha = ''
        if ($da.State -eq 'inaccessible') { Stop-Restore 1 'target_inaccessible' }
        if ($da.State -eq 'ok') {
            if (($da.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { Stop-Restore 1 'target_reparse' }
            if (($da.Attributes -band [IO.FileAttributes]::Directory) -ne 0) { Stop-Restore 1 'target_is_directory' }
            $lb = $null
            try { $lb = Read-MMOFileBytes $dest } catch { Stop-Restore 1 'target_unreadable' }
            $prevSha = Get-MMOBytesSha256 $lb
            if ($prevSha -ceq [string]$e.sha256) { $target = 'same' } else { $target = 'overwrite' }
        }
        $action = 'write'
        if ($target -ceq 'same') { $action = 'unchanged' }
        [void]$plan.Add([pscustomobject]@{ Rel = $rel; Dest = $dest; Group = [string]$c.Group; Kind = [string]$c.Kind; Size = [long]$e.size; Sha = [string]$e.sha256; Target = $target; PrevSha = $prevSha; Action = $action; Valid = [bool]$e.valid })
    }
    $nWrite = @($plan | Where-Object { $_.Action -ceq 'write' }).Count
    $nSame = $plan.Count - $nWrite
    foreach ($p in $plan) {
        Write-RsLine '_PLAN' ('action=' + $p.Action + ' path=' + $p.Rel + ' slot=' + $p.Group + ' target=' + $p.Target + ' valid=' + ([string]$p.Valid).ToLowerInvariant())
    }
    foreach ($sk in $skipList) { Write-RsLine '_PLAN' ('action=skip path=' + $sk.Path + ' reason=' + $sk.Reason) }

    # 7. Fazlalik (silinmez, raporlanir) ve eski .tmp/.bak artiklari
    $restoreSet = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($p in $plan) { [void]$restoreSet.Add($p.Rel) }
    $extras = 0
    $stale = 0
    foreach ($lf in $live) {
        if (-not (& $inScope $lf)) { continue }
        $lc = Get-MMOSaveFileClass $lf
        if (-not $entryByPath.ContainsKey($lf)) { $extras++; Write-RsLine '_EXTRA' ('path=' + $lf + ' (goruntude yok; silinmez)') }
        if ($lc.Class -eq 'side' -and ($lc.Suffix -ceq '.tmp' -or $lc.Suffix -ceq '.bak') -and -not $restoreSet.Contains($lf)) {
            # Goruntudeki ile BAYTI AYNI olan canli yan dosya goruntu aniyla tutarlidir (eski artik degil); farkli / goruntude olmayan = eski artik.
            $same = $false
            if ($entryByPath.ContainsKey($lf)) {
                try { $same = ((Get-MMOFileSha256 ($savedFull + '\' + $lf.Replace('/', '\'))) -ceq [string]$entryByPath[$lf].sha256) } catch { $same = $false }
            }
            if (-not $same) {
                $stale++
                Write-RsLine '_WARN' ('code=stale_side_file path=' + $lf + ' (oyunun kurtarma mantigi eski artigi geri getirebilir; operator tasimali)')
            }
        }
    }
    if ($stale -gt 0) { Add-RsNote 'stale_side_files' }
    Write-RsLine '_WARN' 'code=cross_slot (geri yukleme posta/ticaret ile baglantili altin / esya tutarliligini bozabilir: tek karakteri eski goruntuden koymak altin cogaltabilir; tum agac tek goruntuden en guvenlisidir)'
    Write-RsLine '_SUMMARY' ('write=' + $nWrite + ' unchanged=' + $nSame + ' skipped=' + $skipList.Count + ' skipped_side_files=' + $skipSide + ' not_restored=' + $notRestored + ' extras=' + $extras + ' stale_side_files=' + $stale + ' live_files=' + $live.Count) `
        ('write=' + $nWrite + ' unchanged=' + $nSame + ' skipped=' + $skipList.Count + ' not_restored=' + $notRestored + ' extras=' + $extras + ' stale_side_files=' + $stale)

    # 8. Kuru calisma: burada biter (hicbir sey yazilmadi)
    if (-not $Apply) {
        $pc = Get-RsBlockers
        $script:ProcessCheck = 'cim'
        if ($null -ne $ProcessProvider) { $script:ProcessCheck = 'injected' }
        if ($null -ne $pc.Err) { Write-RsLine '_WARN' ('code=apply_would_defer reason=' + $pc.Err) }
        Write-RsLine '_NEXT' ('uygulamak icin: RestoreSaves.ps1 -Snapshot "' + $snapFull + '" ' + $(if ($useSlots) { '-Slots ' + ($slotKeys.ToArray() -join ',') + ' -AcknowledgeCrossSlotRisk' } else { '-All' }) + ' -ExpectedManifestSha256 ' + $sha1 + ' -Apply   (once yukaridaki ozeti yedek alirken kaydettiginle karsilastir)')
        return
    }

    # 9. UYGULA: kapi 6 (surec + canli yazim) -> kapi 7 (pre-restore, ONCE) -> kapi 6 yeniden -> yazim -> dogrulama / rollback
    $script:ProcessCheck = 'cim'
    if ($null -ne $ProcessProvider) { $script:ProcessCheck = 'injected' }
    Assert-RsQuiet
    $haveLive = ($live.Count -gt 0)
    $preDir = $null
    if ($haveLive) {
        $preDir = Invoke-RsPreimage
        $pt = Test-MMOSnapshot -SnapshotDir $preDir -AllowRootAncestorReparse:$AllowRootAncestorReparse
        if (-not $pt.Ok -or $pt.Incomplete) { Stop-Restore 1 'preimage_verify_failed' }
        if ($pt.Mode -cne 'quiesced') { Stop-Restore 1 'preimage_not_quiesced' }
        $pre = Read-MMOManifest -SnapshotDir $preDir -AllowRootAncestorReparse:$AllowRootAncestorReparse
        if (-not $pre.Ok) { Stop-Restore 1 'preimage_verify_failed' }
        $preByPath = @{}
        foreach ($pe in @($pre.Manifest.entries)) { $preByPath[[string]$pe.path] = $pe }
        foreach ($p in $plan) {
            if ($p.Target -cne 'overwrite') { continue }
            if (-not $preByPath.ContainsKey($p.Rel)) { Stop-Restore 1 'preimage_missing_entry' }
            $pe = $preByPath[$p.Rel]
            if (([string]$pe.notes -ceq 'read_error') -or ([string]$pe.notes -ceq 'too_large')) { Stop-Restore 1 'preimage_no_content' }
            if ([string]$pe.sha256 -cne $p.PrevSha) { Stop-Restore 1 'target_changed_during_preimage' }
        }
        Write-RsLine '_PREIMAGE' ('name=' + $script:PreName) ('preimage=recorded')
    } else {
        Write-RsLine '_PREIMAGE' 'name=none (hedefte canli dosya yok; pre-restore gerekmez)' 'preimage=none_needed'
    }
    if ($null -ne $TestHooks -and $TestHooks.ContainsKey('AfterPreimage')) { & $TestHooks['AfterPreimage'] $preDir }
    Assert-RsQuiet
    # goruntu hala ayni mi (pre-restore sirasinda degisemez ama ucuz)
    $sha3 = $null
    try { $sha3 = Get-MMOFileSha256 $mpath } catch { Stop-Restore 1 'manifest_unreadable' }
    if ($sha3 -cne $sha1) { Stop-Restore 1 'manifest_changed_during_verify' }
    if ($null -ne $TestHooks -and $TestHooks.ContainsKey('BeforeFirstWrite')) { & $TestHooks['BeforeFirstWrite'] $preDir }

    $written = New-Object System.Collections.ArrayList
    $failReason = $null
    $idx = 0
    foreach ($p in $plan) {
        if ($p.Action -cne 'write') { continue }
        $idx++
        try {
            if ($null -ne $TestHooks -and $TestHooks.ContainsKey('BeforeFile')) { & $TestHooks['BeforeFile'] $p.Rel $idx }
            # hedef hala goruntu alinirkenki gibi mi (TOCTOU)
            if ($p.Target -ceq 'overwrite') {
                $cur = Read-MMOFileBytes $p.Dest
                if ((Get-MMOBytesSha256 $cur) -cne $p.PrevSha) { throw 'target_changed_since_plan' }
            } elseif ((Get-MMOAttributes $p.Dest).State -ne 'missing') { throw 'target_appeared_since_plan' }
            $src = Resolve-MMOSafePath -Root ($snapFull + '\files') -RelPath $p.Rel -MustExist -AllowRootAncestorReparse:$AllowRootAncestorReparse
            $bytes = Read-MMOFileBytes $src
            if ([long]$bytes.Length -ne $p.Size) { throw 'source_size_mismatch' }
            if ((Get-MMOBytesSha256 $bytes) -cne $p.Sha) { throw 'source_hash_mismatch' }
            Write-RsFile -Dest $p.Dest -Bytes $bytes -Sha $p.Sha -Rel $p.Rel
            [void]$written.Add($p)
            if ($null -ne $TestHooks -and $TestHooks.ContainsKey('AfterWrite')) { & $TestHooks['AfterWrite'] $p.Rel $p.Dest }
        } catch {
            $m0 = $_.Exception.Message
            if ($m0 -cmatch '^[a-z_]+\z') { $failReason = $m0 } else { $failReason = 'write_failed' }
            break
        }
    }
    if ($null -eq $failReason) {
        # Sonrasi: yazilan her dosyayi yeniden oku + hash (kapi: tum yazilanlar)
        foreach ($p in $written) {
            $ok = $false
            try { $ok = ((Get-MMOFileSha256 $p.Dest) -ceq $p.Sha) } catch { $ok = $false }
            if (-not $ok) { $failReason = 'post_verify_failed'; break }
        }
    }
    if ($null -ne $failReason) {
        if ($null -ne $TestHooks -and $TestHooks.ContainsKey('BeforeRollback')) { & $TestHooks['BeforeRollback'] $preDir }
        $rb = Invoke-RsRollback -Written $written -PreDir $preDir
        if (-not $rb) {
            Write-RsLine '_ROLLBACK_FAILED' ('preimage=' + $script:PreName + ' elle geri al: RestoreSaves.ps1 -Snapshot "' + $script:PreDir + '" -All -IncludeInvalid -IncludeSideFiles -ExpectedManifestSha256 <ozet> -Apply') ('preimage=' + $script:PreName)
            Stop-Restore 1 ('rollback_failed:' + $failReason)
        }
        Stop-Restore 1 ('write_failed_rolled_back:' + $failReason)
    }
    Write-RsLine '_APPLIED' ('written=' + $written.Count + ' unchanged=' + $nSame + ' preimage=' + $script:PreName) ('written=' + $written.Count + ' unchanged=' + $nSame)
}

$exitCode = 1
$reason = 'unexpected'
try {
    Invoke-RestoreMain
    $exitCode = 0
    $reason = 'ok'
} catch {
    $msg = $_.Exception.Message
    if ($msg -cmatch '^MMORS\|(?<c>[0-9]+)\|(?<r>.*)\z') {
        $exitCode = [int]$Matches['c']
        $reason = $Matches['r']
    } else {
        $exitCode = 1
        $reason = 'unexpected:' + $msg
    }
}
if ($exitCode -eq 0 -and $script:Notes.Count -gt 0) {
    $exitCode = 4
    $reason = ($script:Notes.ToArray() -join '+')
}
$runMode = 'dry-run'
if ($Apply) { $runMode = 'apply' }
$status = 'ok'
if ($exitCode -eq 4) { $status = 'ok_with_warnings' }
elseif ($exitCode -eq 3) { $status = 'deferred' }
elseif ($exitCode -eq 2) { $status = 'usage' }
elseif ($exitCode -ne 0) {
    $status = 'failed'
    if ($reason.StartsWith('write_failed_rolled_back', [StringComparison]::Ordinal)) { $status = 'rolled_back' }
    elseif ($reason.StartsWith('rollback_failed', [StringComparison]::Ordinal)) { $status = 'rollback_failed' }
}
$reasonCode = ($reason -split ':', 2)[0]
Write-Host ('[MMO_RESTORE] status=' + $status + ' reason=' + $reason + ' exit=' + $exitCode)
[void]$script:Log.Add('[MMO_RESTORE] status=' + $status + ' reason=' + $reasonCode + ' exit=' + $exitCode)
$resultText = 'mode=' + $runMode + ' exit=' + $exitCode + ' status=' + $status + ' reason=' + $reason + ' snapshot=' + $script:SnapName + ' preimage=' + $script:PreName + ' process_check=' + $script:ProcessCheck + ' manifest_sha256=' + $script:ManifestSha
Write-Host ('[MMO_RESTORE_RESULT] ' + $resultText)
[void]$script:Log.Add('[MMO_RESTORE_RESULT] mode=' + $runMode + ' exit=' + $exitCode + ' status=' + $status + ' reason=' + $reasonCode + ' process_check=' + $script:ProcessCheck)
# Yalniz -Apply denemelerinde (exit 2 haric) dosya yazilir: restore.log (pre-restore goruntu dizinine) ve <BackupRoot>\last_restore.json. Kuru calisma HICBIR sey yazmaz.
if ($Apply -and $script:Attempted -and $exitCode -ne 2) {
    if ($null -ne $script:PreDir -and [IO.Directory]::Exists($script:PreDir) -and -not (Test-MMOReparse $script:PreDir)) {
        try {
            $logText = ($script:Log.ToArray() -join "`r`n") + "`r`n"
            [IO.File]::WriteAllBytes($script:PreDir + '\restore.log', (New-Object System.Text.UTF8Encoding($false)).GetBytes($logText))
        } catch { Write-Host '[MMO_RESTORE_WARN] restore.log yazilamadi' }
    }
    $statusRoot = $null
    try { $statusRoot = Get-RsStatusRoot } catch { $statusRoot = $null }
    if ($null -eq $statusRoot) {
        Write-Host '[MMO_RESTORE_WARN] last_restore.json yazilmadi: yedek koku mevcut/guvenli degil'
    } else {
        $stamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture)
        $okRun = $false
        try {
            $okRun = Write-RsJsonAtomic -Path ($statusRoot + '\last_restore.json') -Object ([ordered]@{
                exit_code = $exitCode; status = $status; reason = $reasonCode; snapshot = $script:SnapName; preimage = $script:PreName
                manifest_sha256 = $script:ManifestSha; time_utc = $stamp; tool = 'RestoreSaves/1' })
        } catch { $okRun = $false }
        if (-not $okRun) { Write-Host '[MMO_RESTORE_WARN] last_restore.json yazilamadi' }
    }
}
exit $exitCode
