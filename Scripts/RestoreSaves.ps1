# Kayit dosyalari GERI YUKLEME (docs/DESIGN_BACKUP.md, dilim B3). OYUNCU VERISININ UZERINE YAZAN TEK ARAC: fail-closed, KURU CALISMA VARSAYILAN.
# Bir anlik goruntuden (BackupSaves.ps1 ciktisi) tum agaci (-All) ya da secili karakter(ler)i (-Slots a,b) Saved\ altina geri koyar.
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII.
#
# KURU CALISMA (varsayilan, -Apply YOK): hicbir sey yazmaz / tasimaz / silmez (log ve durum dosyasi dahil); plani yazar (hangi dosya, hangi slot,
#   yazilacak / ayni / atlanacak ve neden) ve manifest.json dosyasinin TAM SHA-256 ozetini basar. Operator bu ozeti YEDEK ALIRKEN ayri yere
#   kaydettigi ozetle (backup.log manifest_sha256, last_ok.json) karsilastirir; arac hazir bir -Apply komutu BASMAZ (hesaplanan ozeti komuta
#   kopyalamak karsilastirmayi anlamsiz kilar). -Apply yalniz -ExpectedManifestSha256 verilir VE eslesirse calisir. Imzasiz yedekte bu
#   karsilastirma TEK koruma katmanidir (docs/DESIGN_BACKUP.md K6).
# KAPILAR (hepsi uygulamadan once; herhangi biri basarisizsa HICBIR dosyaya dokunulmaz):
#   1 Test-MMOSnapshot (boyut, SHA-256, valid/notes yeniden hesabi, fazladan dosya, reparse, header/entries digest, yarim goruntu) + manifest ozeti
#     (manifest.json TEK KEZ okunur: ayni bayt dizisi hem hash'lenir hem ayristirilir).
#   2 valid=false / unstable girisler ve .tmp/.bak/.bad/.tmp.bad yan dosyalar VARSAYILAN geri yuklenmez (-IncludeInvalid / -IncludeSideFiles, [MMO_RESTORE_FORCE]).
#   3 mode=live goruntu RED (-AcceptLiveSnapshot); manifestte bildirilmis cakisan adlar (collisions) varken RED (yalniz '-prerestore' goruntude
#     -AcceptCollisions: pre-restore goruntusunden geri alma yolu cakisan adli canli dosyalar yuzunden tikanmasin).
#   4 kaynak koku: manifest source_saved_dir hedef Saved ile ayni olmali (-AcceptDifferentSource; imzasiz oldugu icin bilgi amaclidir).
#   5 -Slots: -AcknowledgeCrossSlotRisk ZORUNLU (posta/ticaret altin cogaltma/kaybi riski). -All: canlida goruntude olmayan dosya (EXTRA) ya da
#     geri yuklenmeyen giris sifirdan buyukse, -Slots: geri yuklenmeyen canli posta kutularinda slota atif izi varsa ya da kutu DOGRULANAMIYORSA
#     (ayristirilamaz / sema belirsiz) -AcknowledgePartialState ZORUNLU. Bu araclar ticaret/posta altin etkisini GERI ALMAZ ve tamamen
#     DOGRULAYAMAZ; hicbir kapsam "tutarli" garantisi vermez.
#   6 Yazimdan HEMEN ONCE (iki kez: pre-restore oncesi ve ilk yazimdan once) surec denetimi (UnrealEditor/MyProjectServer/UBT; sorgu hatasi/bos = 3)
#     ve hedef Saved altinda son -RecentWriteSeconds saniyede degisen dosya var mi (canli yazim belirtisi; var = 3). Yazim dongusunde de
#     her N dosyada (varsayilan 20) ayni denetim (yalniz oyuncu dizinleri) tekrarlanir; bu makinenin surecleri disinda (baska makinedeki sunucu)
#     HICBIR sey gorulmez: operator dogrulamalidir.
#   7 PRE-RESTORE GORUNTUSU zorunlu ve ONCE: BackupSaves.ps1 ile '<ts>-prerestore' goruntusu; alinamaz ya da dogrulanamaz ya da uzerine yazilacak
#     bir canli dosyanin icerigi goruntude yoksa HICBIR dosyaya dokunulmaz. (Hedefte hic canli dosya yoksa pre-restore gerekmez: 'preimage=none'.)
#   8 Yazim: her dosya TEK okuma bayt dizisi (hash dogrulanir) -> hedef dizinde <ad>.restore.tmp (CreateNew) -> yeniden oku + hash -> yazim
#     aninda hedef kokten itibaren reparse yeniden dogrulamasi -> mevcut dosya <ad>.restore.old adina TASINIR (ayni dizinde atomik yeniden adlandirma)
#     -> tmp hedef adina tasinir. KARAR (File.Replace yerine): Replace yarida hata verirse (1176/1177 sinifi) hedef dosya asil adinda kalmayabilir
#     ve durum belirsizdir; Replace'in yedek dosyasi ayni birimde olmak zorundadir (pre-restore dizini baska birimde olabilir). Iki adimli Move ile
#     her adimin sonucu bilinir: ikinci adim basarisizsa eski dosya hemen geri tasinir; o da basarisizsa 'place_failed_old_displaced' + rollback.
#     Mevcut dosyaya dogrudan yazim YOK (sert baglanti / ADS tuzagi). Hedefte ayni kanonik anahtarli baska yazimli dosya = RED.
#     <ad>.restore.old basarida (hash = onceki icerik dogrulanarak) bu kosunun KENDI olusturdugu dosya olarak silinir; dogrulanamazsa yerinde kalir.
#   9 Herhangi bir dosya yazimi basarisizsa kalan dosyalara DEVAM EDILMEZ; DENENEN her dosya (basarisiz olan dahil) pre-restore goruntusunden
#     geri alinir (rollback; once surec/canli yazim denetimi): hedefin su anki icerigi ne bizim yazdigimiz ne de onceki ise (baska birisi
#     degistirdi) UZERINE YAZILMAZ: 'rollback_conflict' + pre-restore baytlari '<ad>.rollback-conflict' olarak yana yazilir. Bu kosunun
#     yeni yarattigi dosyalar yalniz hala bizim yazdigimiz bayt ise silinir. Rollback basarisizsa acik hata + tek, test edilmis geri alma komutu.
#  10 YARIDA KESILME: ilk yazimdan ONCE <BackupRoot>\restore_in_progress.json atomik yazilir (pre-restore adi, manifest ozeti, plan sayisi, UTC);
#     basari ya da TAM rollback sonrasi silinir. Varken yeni -Apply kod 3 ile reddedilir (-AcknowledgeStaleRestoreMarker ile bilincli devam).
#     restore.log'a (pre-restore dizini) ilk yazimdan once 'begin' satiri yazilir.
# SILME YOK: hedefte goruntude olmayan dosyalar silinmez, yalniz raporlanir. Goruntude olmayan/farkli canli .tmp/.bak yan dosyalari (oyunun kurtarma
#   mantigi eski artigi geri getirebilir) 'stale_side_files': -Apply VARSAYILAN RED; -QuarantineStaleSideFiles bunlari pre-restore dizinine
#   (quarantine\) TASIR (silme yok; rollback'te geri tasinir). Canli .bad/.tmp.bad kilitleri tasinmaz/dusurulmez, 'live_lock_files' uyarisi verilir.
#   Onceki kosudan kalan *.restore.tmp / *.restore.old / *.rollback.old / *.rollback-conflict dosyalari silinmez, 'leftover_restore_artifacts' olarak raporlanir.
#
# CIKIS KODU (BackupSaves.ps1 / VerifySaves.ps1 sozlesmesiyle uyumlu): 0 tamam; 1 basarisiz / reddedildi (dogrulama, kapi, yazim hatasi + rollback);
#   2 kullanim hatasi; 3 ertelendi (surec / canli yazim / yol yapisi / kaynak-hedef erisilemiyor / bayat is-isareti: HICBIR dosyaya dokunulmadi); 4 basari
#   ama uyarili (neden(ler) '+' ile: forced, live, different_source, stale_side_files, live_lock_files, entries_not_restored, proof_stale,
#   snapshot_ignored_player_data, partial_state_acknowledged, stale_marker_acknowledged, restore_marker_present, collisions_accepted,
#   leftover_restore_artifacts, restore_old_left, restore_marker_left).
# Makine okunur: [MMO_RESTORE_MANIFEST_SHA256], [MMO_RESTORE_SNAPSHOT|PLAN|WARN|FORCE|EXTRA|SUMMARY|PRE|QUARANTINE|ROLLBACK_FAILED|ROLLBACK_CONFLICT], son satir [MMO_RESTORE_RESULT].
# Konsol oyuncu adlari icerir; restore.log (yalniz -Apply, pre-restore goruntu dizininde) ve last_restore.json (<BackupRoot>) YALNIZ neden kodu ve
# sayilar icerir (oyuncu adi / tam yol YOK). Manifestten gelen her serbest metin konsola/loga kontrol karakterleri temizlenerek basilir.
param(
    [string]$Snapshot = '',
    [string]$SavedDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved'),
    [string]$BackupRoot = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'MMOBackups'),
    [switch]$All,
    [string[]]$Slots = @(),
    [switch]$Apply,
    [string]$ExpectedManifestSha256 = '',
    [switch]$AcknowledgeCrossSlotRisk,
    [switch]$AcknowledgePartialState,
    [switch]$AcknowledgeStaleRestoreMarker,
    [switch]$QuarantineStaleSideFiles,
    [switch]$IncludeInvalid,
    [switch]$IncludeSideFiles,
    [switch]$AcceptLiveSnapshot,
    [switch]$AcceptDifferentSource,
    [switch]$AcceptCollisions,
    [int]$RecentWriteSeconds = 60,
    [switch]$AllowRootAncestorReparse,
    # B3.2 (O1) CokUs KURTARMA: -Snapshot = bu yarim kalmis kosunun PRE-RESTORE goruntusu. Saved\ altinda kalan *.restore.old / *.restore.tmp yapitlarini
    # (hash dogrulanarak) isler; varsayilan KURU CALISMA, -Apply + -ExpectedManifestSha256 (pre-restore manifest ozeti) ile uygulanir.
    [switch]$Recover,
    # YALNIZ TEST (MMO_BACKUP_TESTMODE=1 gerekir, aksi halde kullanim hatasi 2). ProcessProvider: Name/ProcessId/CommandLine uyeli nesneler (BackupSaves ile ayni);
    # Timestamp: pre-restore goruntusu icin sabit yyyyMMddTHHmmssZ; TestHooks (ad -> scriptblock): AfterVerify {param($snap)}, BeforePreimage {param($live)},
    # AfterPreimageTaken {param($preDir)}, AfterPreimage {param($preDir)}, BeforeFirstWrite {param($preDir)}, BeforeFile {param($rel,$idx)},
    # AfterTmp {param($rel,$tmp,$dest)}, BeforePlace {param($rel,$tmp,$dest)}, AfterDisplace {param($rel,$tmp,$dest,$old)}, AfterWrite {param($rel,$dest)},
    # BeforeRollback {param($preDir)}, BeforeMarkerWrite {param($markerPath)}, BeforeQuarantineMove {param($from,$to)}, BeforeRecoverStep {param($action,$rel)};
    # QuietEvery (int): yazim dongusundeki denetim araligi.
    [scriptblock]$ProcessProvider = $null,
    [string]$Timestamp = '',
    [hashtable]$TestHooks = $null
)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')

$script:Bound = $PSBoundParameters
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
$script:PreSha = ''
$script:ProcessCheck = 'none'
$script:Attempted = $false
$script:Flags = New-Object System.Collections.ArrayList
$script:MarkerPath = $null
$script:MirrorPath = $null
$script:MarkerWritten = $false
$script:LockFs = $null
$script:ScopeSlots = ''
$script:ChainOpen = 0
$script:RunKind = 'restore'
$script:RecoverDir = $null
$script:Excl = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)

function Stop-Restore {
    param([int]$Code, [string]$Reason)
    throw ('MMORS|' + $Code + '|' + $Reason)
}

function Protect-RsText {
    # Manifestten / dosya sistemi hatasindan gelen serbest metin: kontrol, bicim ve satir/paragraf ayirici karakterler '?' olur (log/konsol satiri enjeksiyonu).
    # B3.2 (D9): gorunmez ama kategori Lo/So/Mn olan karakterler de '?' olur: U+3164 (Hangul filler), U+2800 (Braille blank), U+115F/U+1160/U+FFA0 (diger Hangul
    # filler'lar), U+034F (birlestirme oglagi), U+17B4/U+17B5 (Khmer gizli ses). C1 kontrol karakterleri (U+0080-U+009F, NEL dahil) \p{Cc} icindedir.
    param([string]$Text)
    if ($null -eq $Text) { return '' }
    return [regex]::Replace($Text, '[\p{Cc}\p{Cf}\p{Zl}\p{Zp}\u3164\u2800\u115F\u1160\uFFA0\u034F\u17B4\u17B5]', '?')
}

function Format-RsArg {
    # B3.2 (D3): komut satirina basilan yol / deger PowerShell tek tirnagiyla kacirilir. Tek tirnak sinifindaki her karakter (' ve U+2018..U+201B) ikilenir;
    # tek tirnak icinde $ ve ters kesme literaldir. Kontrol karakterleri konsolda zaten Protect-RsText ile '?' olur.
    param([string]$Value)
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append([char]39)
    foreach ($ch in $Value.ToCharArray()) {
        [void]$sb.Append($ch)
        $c = [int]$ch
        if ($c -eq 39 -or ($c -ge 0x2018 -and $c -le 0x201B)) { [void]$sb.Append($ch) }
    }
    [void]$sb.Append([char]39)
    return $sb.ToString()
}

function Write-RsLine {
    # Konsol + (yalniz kod duzeyli) log. $LogText verilmezse log'a yazilmaz (oyuncu adi / yol iceren satirlar).
    param([string]$Tag, [string]$Text, [string]$LogText = $null)
    Write-Host ('[MMO_RESTORE' + $Tag + '] ' + (Protect-RsText $Text))
    if ($null -ne $LogText) { [void]$script:Log.Add('[MMO_RESTORE' + $Tag + '] ' + (Protect-RsText $LogText)) }
}

function Add-RsNote {
    param([string]$Note)
    if (-not $script:Notes.Contains($Note)) { [void]$script:Notes.Add($Note) }
}

function Invoke-RsHook {
    # Test kancasi (yalniz TestHooks verilmisse); ciktisi atilir (fonksiyon donus degerlerini kirletmesin).
    param([string]$Name, [object[]]$HookArgs = @())
    if ($null -ne $TestHooks -and $TestHooks.ContainsKey($Name)) { [void](& $TestHooks[$Name] @HookArgs) }
}

function Add-RsExclude {
    # Bu kosunun kendi yazdigi dosya (ve yan adlari) "canli yazim" belirtisi sayilmaz.
    param([string]$Dest)
    foreach ($sfx in '', '.restore.tmp', '.restore.old', '.rollback.old', '.rollback-conflict') { [void]$script:Excl.Add($Dest + $sfx) }
}

function Open-RsLock {
    # B3.2 (O3): <BackupRoot>\.lock -- BackupSaves.ps1 ile AYNI dosya ve acilis kipi (FileShare.None, DeleteOnClose). -Apply boyunca tutulur; BackupSaves ya da
    # baska bir RestoreSaves -Apply ayni anda calisamaz. Basari = $true. Zaten tutuluyorsa $true.
    if ($null -ne $script:LockFs) { return $true }
    $lp = $script:RootFull + '\.lock'
    if (Test-MMOReparse $lp) { return $false }
    try { $script:LockFs = New-Object IO.FileStream($lp, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None, 4096, [IO.FileOptions]::DeleteOnClose) }
    catch { $script:LockFs = $null; return $false }
    return $true
}

function Close-RsLock {
    if ($null -ne $script:LockFs) { try { $script:LockFs.Dispose() } catch { } ; $script:LockFs = $null }
}

function Read-RsMarker {
    # Is-isareti icerigi: once birincil, sonra yansima. Okunamayan / ayristirilamayan = $null.
    param($State)
    foreach ($p in @($State.Primary, $State.Mirror)) {
        if ((Get-MMOAttributes $p).State -ne 'ok') { continue }
        try { return ((ConvertFrom-MMOUtf8Bytes (Read-MMOFileBytes -Path $p -MaxBytes 8388608)).Text | ConvertFrom-Json -ErrorAction Stop) } catch { }
    }
    return $null
}

function Write-RsMarkerFile {
    # EXCLUSIVE yazim (O3): kendi .tmp'si (CreateNew) -> Move; File.Move var olan hedefin uzerine YAZMAZ, bu yuzden iki eszamanli -Apply'dan ikincisi basarisiz olur.
    # Donus: '' (basari) | 'exists' | 'failed'.
    param([string]$Path, [string]$Json)
    if (Test-MMOReparse $Path) { return 'failed' }
    if ((Get-MMOAttributes $Path).State -ne 'missing') { return 'exists' }
    $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($Json)
    $tmp = $Path + '.' + $PID + '.tmp'
    $fs = $null
    $err = $null
    try {
        $fs = New-Object IO.FileStream($tmp, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $fs.Write($bytes, 0, $bytes.Length)
        $fs.Flush($true)
    } catch { $err = $_.Exception.Message } finally { if ($null -ne $fs) { $fs.Dispose() } }
    if ($null -ne $err) { return 'failed' }
    try { [IO.File]::Move($tmp, $Path) } catch {
        try { [IO.File]::Delete($tmp) } catch { }
        if ((Get-MMOAttributes $Path).State -ne 'missing') { return 'exists' }
        return 'failed'
    }
    return ''
}

function Remove-RsMarkers {
    # Is-isaretinin iki kopyasini siler. Donus: kalan kopya sayisi (0 = temiz).
    param($State)
    $left = 0
    foreach ($p in @($State.Primary, $State.Mirror)) {
        if ((Get-MMOAttributes $p).State -eq 'missing') { continue }
        try { [IO.File]::Delete($p) } catch { }
        if ((Get-MMOAttributes $p).State -ne 'missing') { $left++ }
    }
    return $left
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
    # -Exclude: bu kosunun kendi yazdigi dosyalar; -PlayerDirsOnly: yalniz bes oyuncu dizini (yazim dongusu icin hafif tarama).
    param([string]$SavedFull, [int]$Seconds, $Exclude = $null, [switch]$PlayerDirsOnly)
    $cut = [datetime]::UtcNow.AddSeconds(-$Seconds)
    $n = 0
    $stack = New-Object System.Collections.Stack
    if ($PlayerDirsOnly) {
        foreach ($d in 'SaveGames', 'MMOAchievements', 'MMODaily', 'MMOMail', 'MMORace') {
            $p = $SavedFull + '\' + $d
            $a = Get-MMOAttributes $p
            if ($a.State -eq 'missing') { continue }
            if ($a.State -ne 'ok') { return -1 }
            $stack.Push($p)
        }
    } else { $stack.Push($SavedFull) }
    while ($stack.Count -gt 0) {
        $dir = [string]$stack.Pop()
        $items = $null
        try { $items = @(Get-ChildItem -LiteralPath $dir -Force -ErrorAction Stop) } catch { return -1 }
        foreach ($it in $items) {
            if ($it.PSIsContainer) {
                if (-not (Test-MMOReparse $it.FullName)) { $stack.Push($it.FullName) }
                continue
            }
            if ($null -ne $Exclude -and $Exclude.Contains($it.FullName)) { continue }
            $t = [IO.File]::GetLastWriteTimeUtc($it.FullName)
            if ($t -gt $cut) { $n++ }
        }
    }
    return $n
}

function Get-RsQuietProblem {
    # Surec yok + canli yazim belirtisi yok. Donus: $null (sakin) | neden kodu.
    param($Exclude = $null, [switch]$Light)
    $pc = Get-RsBlockers
    if ($null -ne $pc.Err) {
        foreach ($b in $pc.Blockers) { Write-RsLine '_BLOCKER' ('process=' + $b.Name + ' pid=' + $b.ProcessId) }
        return $pc.Err
    }
    $rw = Get-RsRecentWrites -SavedFull $script:SavedFull -Seconds $RecentWriteSeconds -Exclude $Exclude -PlayerDirsOnly:$Light
    if ($rw -lt 0) { return 'recent_write_check_failed' }
    if ($rw -gt 0) { return 'recent_write_detected' }
    return $null
}

function Assert-RsQuiet {
    # Yazimdan hemen once: surec yok + canli yazim belirtisi yok. Hata = 3.
    $q = Get-RsQuietProblem -Exclude $script:Excl
    if ($null -ne $q) { Stop-Restore 3 $q }
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

function Get-RsLeftovers {
    # Onceki (yarim kalmis) bir kosudan kalan kendi yapitlarimiz: bunlar silinmez (.restore.old = eski oyuncu verisi olabilir), yalniz raporlanir.
    param([string]$SavedFull)
    $out = New-Object System.Collections.ArrayList
    foreach ($d in 'SaveGames', 'MMOAchievements', 'MMODaily', 'MMOMail', 'MMORace') {
        $p = $SavedFull + '\' + $d
        if ((Get-MMOAttributes $p).State -ne 'ok') { continue }
        $items = $null
        try { $items = @(Get-ChildItem -LiteralPath $p -Force -ErrorAction Stop) } catch { $items = $null }
        if ($null -eq $items) { continue }
        foreach ($it in $items) {
            if ($it.PSIsContainer) { continue }
            if ($it.Name -cmatch '\.(restore\.tmp|restore\.old|rollback\.old|rollback-conflict)\z') { [void]$out.Add($d + '/' + $it.Name) }
        }
    }
    return , ([string[]]@($out.ToArray()))
}

function Get-RsMailFindings {
    # -Slots modunda: geri yuklenmeyen CANLI posta kutularinda (MMOMail\MMOMail_<anahtar>.json, yan dosyalar degil) geri yuklenen slota atif.
    # Sema (Source/MyProject/Private/Mail/MMOMailStore.cpp): {"version":1|2,"mails":[{"from","text","gold","sent"}]}. Atif = 'from' kanonik anahtari
    # slot anahtari ya da 'text' slot anahtarini / bosluklu halini iceriyor. Ayristirilamayan / sema belirsiz kutu = Unverified (DOGRULANMADI).
    # Bu bir DOGRULAMA DEGILDIR: ticaret ve baska yollarla tasinan altin/esya izlenmez.
    # B3.2 (D5): .bak ve .tmp kutulari da taranir (oyunun kurtarma mantigi eski .bak'i geri getirebilir). Bu kutularda iz = ana kutuyla ayni (onay gerekir);
    # ayristirilamayan .bak/.tmp kutusu yalniz UYARI (SideUnverified: .tmp yarim yazim olabilir, bu tek basina onay gerektirmez). .bad / .tmp.bad taranmaz.
    # YANLIS NEGATIF SINIRLARI (belgelenmis): yalniz 'from' kanonik anahtari ve 'text' icinde slot adinin gecmesi aranir; yeniden adlandirilmis gonderici, ticaretle
    # tasinan altin/esya, kutudan zaten silinmis/okunmus posta, 8 MB ustu ya da sema disi kutu, 'text' icinde bicimi bozulmus ad (A l i) bulunamaz; kisa slot
    # anahtarlari ('al') metinde rastgele eslesip yanlis pozitif verir.
    param([string]$SavedFull, $Live, $SlotKeys)
    $traces = New-Object System.Collections.ArrayList
    $unver = New-Object System.Collections.ArrayList
    $sideUnver = New-Object System.Collections.ArrayList
    foreach ($lf in $Live) {
        $c = Get-MMOSaveFileClass $lf
        if ($c.Kind -cne 'mail') { continue }
        $isSide = ($c.Suffix -ceq '.bak' -or $c.Suffix -ceq '.tmp')
        if ($c.Suffix -cne '' -and -not $isSide) { continue }
        if ($SlotKeys.Contains([string]$c.Group)) { continue }
        $o = $null
        try {
            $bytes = Read-MMOFileBytes ($SavedFull + '\' + $lf.Replace('/', '\'))
            $o = (ConvertFrom-MMOUtf8Bytes $bytes).Text | ConvertFrom-Json -ErrorAction Stop
        } catch { $o = $null }
        $mails = $null
        $okSchema = $false
        if ($null -ne $o -and $o -is [pscustomobject]) {
            # Ozellik dogrudan okunur (bos dizi fonksiyon donusunde $null'a acilirdi)
            $pv = $o.PSObject.Properties['version']
            $pm = $o.PSObject.Properties['mails']
            if ($null -ne $pv -and $null -ne $pm) {
                $ver = $pv.Value
                if ((($ver -is [int]) -or ($ver -is [long])) -and ($ver -eq 1 -or $ver -eq 2) -and ($pm.Value -is [array])) { $okSchema = $true; $mails = $pm.Value }
            }
        }
        if (-not $okSchema) { if ($isSide) { [void]$sideUnver.Add($lf) } else { [void]$unver.Add($lf) }; continue }
        $hit = New-Object System.Collections.ArrayList
        foreach ($mm in $mails) {
            if ($mm -isnot [pscustomobject]) { $hit.Clear(); if ($isSide) { [void]$sideUnver.Add($lf) } else { [void]$unver.Add($lf) }; break }
            $from = Get-MMOMember $mm 'from'
            $text = Get-MMOMember $mm 'text'
            foreach ($sk in $SlotKeys) {
                $found = $false
                if ($from -is [string]) {
                    $fk = ConvertTo-MMOCanonicalKey $from
                    if ($null -ne $fk -and $fk -ceq $sk) { $found = $true }
                }
                if (-not $found -and $text -is [string]) {
                    $tl = $text.ToLowerInvariant()
                    if ($tl.Contains($sk) -or $tl.Contains($sk.Replace('_', ' '))) { $found = $true }
                }
                if ($found -and -not $hit.Contains($sk)) { [void]$hit.Add($sk) }
            }
        }
        foreach ($h in $hit) { [void]$traces.Add($lf + '|' + $h) }
    }
    return [pscustomobject]@{ Traces = [string[]]@($traces.ToArray()); Unverified = [string[]]@(@($unver.ToArray() | Select-Object -Unique)); SideUnverified = [string[]]@(@($sideUnver.ToArray() | Select-Object -Unique)) }
}

function Assert-RsDestChain {
    # Yazim aninda (Move oncesi) hedef kokten itibaren her bilesen reparse mi diye yeniden bakilir (planla yazim arasinda dizin junction'a cevrilebilir).
    param([string]$Rel)
    try { [void](Resolve-MMOSafePath -Root $script:SavedFull -RelPath $Rel -AllowRootAncestorReparse:$AllowRootAncestorReparse) }
    catch {
        $m = $_.Exception.Message
        if ($m -cmatch '^reparse_component:(?<s>.*)\z') {
            if ($Matches['s'] -ceq ($Rel.Split('/')[-1])) { throw 'dest_reparse' }
            throw 'target_dir_reparse'
        }
        if ($m -cmatch '^(reparse_root|root_)') { throw 'target_root_unsafe' }
        throw 'target_path_unsafe'
    }
}

function Write-RsFile {
    # Tek dosya: hedef dizinde <ad>.restore.tmp (CreateNew; var olana yazmaz) -> yeniden oku + hash -> reparse yeniden dogrulama -> mevcut hedef
    # <ad><OldSuffix> adina TASINIR -> tmp hedef adina tasinir (basarisizsa eski dosya geri tasinir). Donus: tasinan eski dosyanin yolu ya da $null (yeni dosya).
    # Hatada KENDI olusturdugu .tmp silinir. Hata = throw (kod).
    param([string]$Dest, [byte[]]$Bytes, [string]$Sha, [string]$Rel, [string]$OldSuffix = '.restore.old', [switch]$NoHooks)
    $tmp = $Dest + '.restore.tmp'
    $old = $Dest + $OldSuffix
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
    $displaced = $false
    try {
        if ($null -ne $err) { throw $err }
        if (-not $NoHooks) { Invoke-RsHook 'AfterTmp' @($Rel, $tmp, $Dest) }
        $chk = Get-MMOFileSha256 $tmp
        if ($chk -cne $Sha) { throw 'tmp_verify_failed' }
        if (-not $NoHooks) { Invoke-RsHook 'BeforePlace' @($Rel, $tmp, $Dest) }
        Assert-RsDestChain $Rel
        if ([IO.File]::Exists($Dest)) {
            # Onceki (yarim kalmis) kosudan kalan yan dosya: icerigi TAM OLARAK simdi yazacagimiz icerikse gereksiz kopyadir (bilgi kaybi yok) ve silinir;
            # farkli icerik (eski oyuncu verisi olabilir) silinmez: RED.
            if ((Get-MMOAttributes $old).State -ne 'missing') { [void](Remove-RsOwnFile -Path $old -ExpectSha $Sha) }
            if ((Get-MMOAttributes $old).State -ne 'missing') { throw 'restore_old_exists' }
            [IO.File]::Move($Dest, $old)
            $displaced = $true
            if (-not $NoHooks) { Invoke-RsHook 'AfterDisplace' @($Rel, $tmp, $Dest, $old) }
            [IO.File]::Move($tmp, $Dest)
            return $old
        }
        [IO.File]::Move($tmp, $Dest)
        return $null
    } catch {
        $m = $_.Exception.Message
        $backOk = $true
        if ($displaced) {
            $backOk = $false
            try { [IO.File]::Move($old, $Dest); $backOk = $true } catch { $backOk = $false }
        }
        if ($created -and [IO.File]::Exists($tmp)) { try { [IO.File]::Delete($tmp) } catch { } }
        if (-not $backOk) { throw 'place_failed_old_displaced' }
        if ($m -cmatch '^[a-z_]+\z') { throw $m }
        throw 'place_failed'
    }
}

function Write-RsAside {
    # Rollback cakismasinda pre-restore baytlari: <hedef>.rollback-conflict (CreateNew; varsa YAZILMAZ). Hedefin kendisine dokunulmaz.
    param([string]$Path, [byte[]]$Bytes, [string]$Sha)
    if ((Get-MMOAttributes $Path).State -ne 'missing') { throw 'aside_exists' }
    $fs = $null
    try {
        $fs = New-Object IO.FileStream($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $fs.Write($Bytes, 0, $Bytes.Length)
        $fs.Flush($true)
    } finally { if ($null -ne $fs) { $fs.Dispose() } }
    if ((Get-MMOFileSha256 $Path) -cne $Sha) { throw 'aside_verify_failed' }
}

function Get-RsDestState {
    # Kind: missing | file (Sha dolu) | other (dizin / reparse / okunamayan: dokunulmaz).
    param([string]$Dest)
    $a = Get-MMOAttributes $Dest
    if ($a.State -eq 'missing') { return [pscustomobject]@{ Kind = 'missing'; Sha = '' } }
    if ($a.State -ne 'ok') { return [pscustomobject]@{ Kind = 'other'; Sha = '' } }
    if ((($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) -or (($a.Attributes -band [IO.FileAttributes]::Directory) -ne 0)) { return [pscustomobject]@{ Kind = 'other'; Sha = '' } }
    try { return [pscustomobject]@{ Kind = 'file'; Sha = (Get-MMOFileSha256 $Dest) } } catch { return [pscustomobject]@{ Kind = 'other'; Sha = '' } }
}

function Remove-RsOwnFile {
    # Bu kosunun kendi olusturdugu yardimci dosyayi (.restore.old / .rollback.old) yalniz icerigi beklenen hash ise siler. Silindi = $true.
    param([string]$Path, [string]$ExpectSha)
    if ((Get-MMOAttributes $Path).State -ne 'ok') { return $false }
    try {
        if ((Get-MMOFileSha256 $Path) -cne $ExpectSha) { return $false }
        [IO.File]::Delete($Path)
        return $true
    } catch { return $false }
}

function Invoke-RsRollback {
    # DENENEN her dosyayi (basarisiz olan dahil) ters sirayla geri al. Hedefin su anki icerigi:
    #   = onceki icerik (PrevSha)            -> dokunulmaz;
    #   = bizim yazdigimiz (Sha) ya da yok   -> pre-restore goruntusunden (hash dogrulanarak) yazilir;
    #   baska bir sey (baskasi degistirdi)   -> UZERINE YAZILMAZ: rollback_conflict, pre-restore baytlari <ad>.rollback-conflict olarak yana yazilir.
    # Yeni dosya: yalniz hala bizim bayt ise silinir, aksi conflict. Karantinaya alinan yan dosyalar geri tasinir. Hata olsa da kalanlara devam eder.
    # Donus: Conflicts[], Failures[].
    param($Attempted, $Quarantined, [string]$PreDir)
    $conflicts = New-Object System.Collections.ArrayList
    $failures = New-Object System.Collections.ArrayList
    $both = New-Object System.Collections.ArrayList
    for ($i = $Attempted.Count - 1; $i -ge 0; $i--) {
        $it = $Attempted[$i]
        try {
            $st = Get-RsDestState $it.Dest
            if ($it.Target -ceq 'overwrite') {
                if ($st.Kind -eq 'file' -and $st.Sha -ceq $it.PrevSha) { [void](Remove-RsOwnFile -Path ($it.Dest + '.restore.old') -ExpectSha $it.PrevSha); continue }
                if ($null -eq $PreDir) { throw 'no_preimage' }
                $pb = Read-MMOFileBytes ($PreDir + '\files\' + ([string]$it.Rel).Replace('/', '\'))
                if ((Get-MMOBytesSha256 $pb) -cne $it.PrevSha) { throw 'preimage_hash_mismatch' }
                if ($st.Kind -eq 'missing' -or ($st.Kind -eq 'file' -and $st.Sha -ceq $it.Sha)) {
                    $oldp = Write-RsFile -Dest $it.Dest -Bytes $pb -Sha $it.PrevSha -Rel $it.Rel -OldSuffix '.rollback.old' -NoHooks
                    if ((Get-MMOFileSha256 $it.Dest) -cne $it.PrevSha) { throw 'rollback_verify_failed' }
                    if ($null -ne $oldp) { [void](Remove-RsOwnFile -Path $oldp -ExpectSha $it.Sha) }
                    [void](Remove-RsOwnFile -Path ($it.Dest + '.restore.old') -ExpectSha $it.PrevSha)
                } else {
                    Write-RsAside -Path ($it.Dest + '.rollback-conflict') -Bytes $pb -Sha $it.PrevSha
                    [void]$conflicts.Add($it.Rel)
                }
            } else {
                if (-not $it.Placed) { continue }
                if ($st.Kind -eq 'missing') { continue }
                if ($st.Kind -eq 'file' -and $st.Sha -ceq $it.Sha) { [IO.File]::Delete($it.Dest); continue }
                [void]$conflicts.Add($it.Rel)
            }
        } catch { [void]$failures.Add($it.Rel + ':' + $_.Exception.Message) }
    }
    for ($i = $Quarantined.Count - 1; $i -ge 0; $i--) {
        $q = $Quarantined[$i]
        try {
            # B3.2 (D7): karantina dosyasi YOKSA ve eski yerinde de yoksa sessizce gecilmez (veri kaybi): quarantine_missing hatasi.
            $toS = (Get-MMOAttributes $q.To).State
            $fromS = (Get-MMOAttributes $q.From).State
            if ($toS -eq 'missing') {
                if ($fromS -ne 'missing') { continue }
                throw 'quarantine_missing'
            }
            if ($toS -ne 'ok') { throw 'quarantine_inaccessible' }
            if ($fromS -ne 'missing') { [void]$both.Add($q.Rel); [void]$conflicts.Add($q.Rel); continue }
            try { [IO.File]::Move($q.To, $q.From) } catch {
                # Birimler arasi Move = kopyala-sil: yarida kalirsa iki yerde de dosya olabilir (rapor edilir, hicbiri silinmez)
                if ((Get-MMOAttributes $q.To).State -ne 'missing' -and (Get-MMOAttributes $q.From).State -ne 'missing') { [void]$both.Add($q.Rel) }
                throw 'quarantine_move_back_failed'
            }
            if ((Get-MMOFileSha256 $q.From) -cne $q.Sha) { throw 'quarantine_restore_verify_failed' }
        } catch { [void]$failures.Add($q.Rel + ':' + $_.Exception.Message) }
    }
    return [pscustomobject]@{ Conflicts = [string[]]@($conflicts.ToArray()); Failures = [string[]]@($failures.ToArray()); Both = [string[]]@($both.ToArray()) }
}

function Invoke-RsPreimage {
    # BackupSaves.ps1 ile '<ts>-prerestore' goruntusu. Donus: goruntu dizini. Hata = Stop (HICBIR dosya henuz yazilmadi).
    $bk = Join-Path $PSScriptRoot 'BackupSaves.ps1'
    $p = @{ SavedDir = $script:SavedFull; BackupRoot = $script:RootFull; Label = 'prerestore'; AllowIgnored = $true }
    if ($AllowRootAncestorReparse) { $p['AllowRootAncestorReparse'] = $true }
    if ($null -ne $ProcessProvider) { $p['ProcessProvider'] = $ProcessProvider }
    if ($Timestamp -ne '') { $p['Timestamp'] = $Timestamp }
    # O3: <BackupRoot>\.lock'u cocuk BackupSaves'e DEVRET (ayni kilit dosyasi; ayni anda iki sahip olamaz). Cocuk bitince hemen yeniden alinir; arada baska bir
    # BackupSaves kilidi kaparsa yeniden alim basarisiz olur ve HICBIR dosya yazilmadan 3 ile ertelenir (pre-restore goruntusu kalir, zararsiz). Yazim dongusu
    # ve is-isareti ancak kilit yeniden alindiktan SONRA baslar.
    Close-RsLock
    $global:LASTEXITCODE = -999
    $lines = @(& $bk @p *>&1 | ForEach-Object { [string]$_ })
    $code = $global:LASTEXITCODE
    $relock = Open-RsLock
    foreach ($ln in $lines) { Write-Host ('[MMO_RESTORE_PRE] ' + (Protect-RsText $ln)) }
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
    if (-not $relock) { Stop-Restore 3 'lock_lost_after_preimage' }
    return $script:PreDir
}

function Write-RsJsonAtomic {
    # last_restore.json / restore_in_progress.json: kendi .tmp'si (CreateNew) -> Replace/Move. Hedef reparse ise yazilmaz. Basari = $true.
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

function Write-RsLogFile {
    # restore.log: pre-restore goruntu dizininde (yoksa yazilmaz). Ilk yazimdan ONCE 'begin' ile, sonda tam icerikle yeniden yazilir.
    # Recover modunda log pre-restore goruntusune (yarim kalan kosunun restore.log'unu EZMEMELI) degil recover dizinine <BackupRoot>\restore_recover_<ts>\recover.log yazilir.
    $logDir = $script:PreDir
    $logName = '\restore.log'
    if ($script:RunKind -ceq 'recover') { $logDir = $script:RecoverDir; $logName = '\recover.log' }
    if ($null -eq $logDir -or -not [IO.Directory]::Exists($logDir) -or (Test-MMOReparse $logDir)) { return $false }
    try {
        $logText = ($script:Log.ToArray() -join "`r`n") + "`r`n"
        [IO.File]::WriteAllBytes($logDir + $logName, (New-Object System.Text.UTF8Encoding($false)).GetBytes($logText))
        return $true
    } catch { Write-Host '[MMO_RESTORE_WARN] restore.log yazilamadi'; return $false }
}

function Get-RsStatusRoot {
    # last_restore.json / restore_in_progress.json icin MEVCUT guvenli yedek koku (olusturulmaz): Saved/repo icinde degil, dizin, reparse degil. Aksi $null.
    if ($null -eq $script:RootFull -or $null -eq $script:SavedFull) { return $null }
    if (Test-MMOPathUnder -Path $script:RootFull -Root $script:SavedFull -AllowEqual -ErrorIsViolation) { return $null }
    if (Test-MMOPathUnder -Path $script:SavedFull -Root $script:RootFull -AllowEqual -ErrorIsViolation) { return $null }
    if (Test-MMOPathUnder -Path $script:RootFull -Root $script:RepoRoot -AllowEqual -ErrorIsViolation) { return $null }
    $a = Get-MMOAttributes $script:RootFull
    if ($a.State -ne 'ok') { return $null }
    if (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { return $null }
    if (($a.Attributes -band [IO.FileAttributes]::Directory) -eq 0) { return $null }
    if (-not $AllowRootAncestorReparse -and $null -ne (Get-MMOReparseAncestor $script:RootFull)) { return $null }
    # B3.2 (D1): GERCEK yol (8.3 / subst / junction cozulmus) iliskisi de ayni denetimden gecer; cozulemezse -AllowRootAncestorReparse'te fail-closed.
    $fr = Get-MMOFinalPath $script:RootFull
    $fs = Get-MMOFinalPath $script:SavedFull
    $fp = Get-MMOFinalPath $script:RepoRoot
    if ($null -eq $fr -or $null -eq $fs -or $null -eq $fp) {
        if ($AllowRootAncestorReparse) { return $null }
    } else {
        if (Test-MMOPathUnder -Path $fr -Root $fs -AllowEqual -ErrorIsViolation) { return $null }
        if (Test-MMOPathUnder -Path $fs -Root $fr -AllowEqual -ErrorIsViolation) { return $null }
        if (Test-MMOPathUnder -Path $fr -Root $fp -AllowEqual -ErrorIsViolation) { return $null }
    }
    return $script:RootFull
}

function Get-RsRollbackCommand {
    # TEK geri alma komutu (testte aynen calistirilir): pre-restore goruntusunden -All; goruntude gecersiz / yan dosya / adlari cakisan canli dosyalar olabilir,
    # arada kalan kismi durum (EXTRA / eski yan dosya / bayat is-isareti) komutu tikamasin diye ilgili onay bayraklari dahil. Beklenen ozet = bu kosunun
    # pre-restore goruntusu icin KENDI hesapladigi ozet. Yazim sonrasi dosyalar taze olacagindan -RecentWriteSeconds kadar beklemek gerekir.
    # B3.2 (D3/D4): yol tek tirnakla (' ikilenerek) kacirilir; kapsam bu kosunun kapsamidir (-Slots ise yalniz o slotlar: diger slotlarin sonraki
    # degisikliklerini ezmesin); komut rutin DEGILDIR (bkz. Write-RsRollbackWarnings).
    if ($null -eq $script:PreDir -or $script:PreSha -eq '') { return '' }
    $scope = '-All'
    if ($script:ScopeSlots -ne '') { $scope = '-Slots ' + $script:ScopeSlots + ' -AcknowledgeCrossSlotRisk' }
    return ('RestoreSaves.ps1 -Snapshot ' + (Format-RsArg $script:PreDir) + ' ' + $scope + ' -IncludeInvalid -IncludeSideFiles -AcceptCollisions -AcknowledgePartialState -QuarantineStaleSideFiles -AcknowledgeStaleRestoreMarker -ExpectedManifestSha256 ' + $script:PreSha + ' -Apply')
}

function Get-RsRecoverCommand {
    # Cokus kurtarma komutu (O1): pre-restore goruntusu + onun manifest ozeti. Once KURU CALISMA (-Apply'siz) onerilir.
    if ($null -eq $script:PreDir -or $script:PreSha -eq '') { return '' }
    return ('RestoreSaves.ps1 -Recover -Snapshot ' + (Format-RsArg $script:PreDir) + ' -ExpectedManifestSha256 ' + $script:PreSha + ' -Apply')
}

function Write-RsRollbackWarnings {
    # D4: geri alma komutunun bayraklari guvenlik kapilarini gevsetir; komut RUTIN gibi sunulmaz.
    Write-RsLine '_WARN' 'code=rollback_command_not_routine (komut -IncludeInvalid -IncludeSideFiles -AcceptCollisions -AcknowledgePartialState -QuarantineStaleSideFiles -AcknowledgeStaleRestoreMarker bayraklarini ICERIR: bunlar guvenlik kapilarini gevsetir; yalniz durumu inceledikten sonra, sunucu kapaliyken calistir)' 'code=rollback_command_not_routine'
    Write-RsLine '_WARN' 'code=rollback_command_stale_if_server_ran (ARADA sunucu / oyuncu / baska bir islem Saved\ dizinine yazdiysa bu komutu KULLANMA: geri alma o yazimlari ezer; once Saved\ durumunu pre-restore goruntusuyle karsilastir)' 'code=rollback_command_stale_if_server_ran'
    $rc = Get-RsRecoverCommand
    if ($rc -ne '') { Write-RsLine '_WARN' ('code=recover_first yarim kalan *.restore.old / *.restore.tmp yapitlari varsa normal -Apply reddedilir; once (kuru calismayi inceleyip): ' + $rc) 'code=recover_first' }
}

function Move-RsAside {
    # Recover: dosyayi karantina yoluna TASIR (silme yok). D7: birimler arasi Move kopyala-sil olabilir; hata sonrasi iki yerde de dosya varsa 'both' raporlanir.
    # Donus: 'ok' | 'both' | 'failed'.
    param([string]$From, [string]$To, [string]$Sha)
    try {
        if ((Get-MMOAttributes $To).State -ne 'missing') { return 'failed' }
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($To))
        Invoke-RsHook 'BeforeQuarantineMove' @($From, $To)
    } catch { return 'failed' }
    $mvFailed = $false
    try { [IO.File]::Move($From, $To) } catch { $mvFailed = $true }
    if ($mvFailed) {
        if ((Get-MMOAttributes $From).State -ne 'missing' -and (Get-MMOAttributes $To).State -ne 'missing') { return 'both' }
        return 'failed'
    }
    try { if ((Get-MMOFileSha256 $To) -cne $Sha) { return 'failed' } } catch { return 'failed' }
    return 'ok'
}

function Invoke-RsRecover {
    # O1 COKUS KURTARMA. Girdi: dogrulanmis PRE-RESTORE goruntusu (Test-MMOSnapshot + ozet kapisi cagrilmadan once yapildi), varsa is-isareti.
    # Saved\ beyaz liste dizinlerindeki *.restore.old / *.restore.tmp / *.rollback.old / *.rollback-conflict yapilari islenir. Hicbir oyuncu dosyasi SILINMEZ:
    #   old_restore    : <ad>.restore.old, hedef YOK ve old'un SHA-256'si pre-restore goruntusundeki giris ozetine esit -> asil adina geri adlandirilir
    #   old_quarantine : hedef VAR, old ozeti goruntu ile ayni (goruntude kopya var) -> recover karantinasina tasinir
    #   tmp_discard    : <ad>.restore.tmp ozeti is-isaretindeki PLANLANAN icerige (kaynak goruntudeki baytlara) esit -> bu kosunun kendi, tam yazilmis hazirlik kopyasi.
    #                    "Yeniden kullanim" YORUMU: recover geri alma yonunde calisir (dosyayi onceki duruma getirir), hazirlik kopyasini yerlestirmez; ayni baytlar
    #                    kaynak goruntuden her zaman yeniden uretilebilecegi icin (bilgi kaybi yok) hash dogrulanip silinir. Farkliysa / plan yoksa tmp_quarantine (silme yok).
    #   rollback.old / rollback-conflict ve ozeti dogrulanamayan yapilar -> unresolved (elle; recover_incomplete)
    # Sonunda is-isaretindeki plan ile hedef durumu karsilastirilir: hepsi onceki durumda ya da hepsi yeni durumda ise isaret kaldirilir; karisik durumda KALIR ve
    # pre-restore goruntusunden geri alma komutu yazilir. Varsayilan KURU CALISMA; -Apply icin -ExpectedManifestSha256 (pre-restore manifest ozeti) zorunlu (ana akista kontrol edildi).
    param($Manifest, [string]$ManifestSha, $StaleMarker, $MarkerState)
    $savedFull = $script:SavedFull
    $snapFull = $script:SnapFull
    $lbl = Get-MMOMember $Manifest 'label'
    $tl = [string](Get-MMOMember $Manifest 'tool')
    if ($script:SnapName -cnotmatch '^[0-9]{8}T[0-9]{6}Z-prerestore\z' -or $lbl -cne 'prerestore' -or -not $tl.StartsWith('BackupSaves/', [StringComparison]::Ordinal)) { Stop-Restore 1 'recover_snapshot_not_preimage' }
    if ([string](Get-MMOMember $Manifest 'mode') -cne 'quiesced') { Stop-Restore 1 'recover_snapshot_not_quiesced' }
    $script:PreName = $script:SnapName
    $script:PreDir = $snapFull
    $script:PreSha = $ManifestSha
    $planMap = @{}
    $chainCount = 0
    $expectedPlan = 0
    $markerKnown = $false
    if ($MarkerState.Present) {
        if ($null -eq $StaleMarker) { Stop-Restore 1 'recover_marker_unreadable' }
        if ([string](Get-MMOMember $StaleMarker 'preimage') -cne $script:SnapName -or [string](Get-MMOMember $StaleMarker 'preimage_manifest_sha256') -cne $ManifestSha) {
            Write-RsLine '_WARN' ('code=recover_marker_mismatch marker_preimage=' + [string](Get-MMOMember $StaleMarker 'preimage') + ' (-Snapshot is-isaretindeki pre-restore goruntusu degil)')
            Stop-Restore 1 'recover_marker_mismatch'
        }
        $markerKnown = $true
        $wcv = Get-MMOMember $StaleMarker 'write_count'
        if (($wcv -is [int]) -or ($wcv -is [long])) { $expectedPlan = [int]$wcv } else { $expectedPlan = -1 }
        foreach ($pi in @(Get-MMOMember $StaleMarker 'plan')) {
            if ($pi -is [pscustomobject] -and $pi.PSObject.Properties['rel'] -and $pi.PSObject.Properties['sha']) { $planMap[[string]$pi.rel] = $pi }
        }
        $cc = Get-MMOMember $StaleMarker 'chain'
        if ($null -ne $cc) { $chainCount = @($cc).Count }
    }
    $preByPath = @{}
    foreach ($pe in @($Manifest.entries)) { $preByPath[[string]$pe.path] = $pe }
    [void](Get-RsLiveFiles -SavedFull $savedFull)
    $leftovers = Get-RsLeftovers -SavedFull $savedFull
    if ($leftovers.Count -eq 0 -and -not $MarkerState.Present) { Write-RsLine '_WARN' 'code=nothing_to_recover'; Stop-Restore 1 'nothing_to_recover' }
    $items = New-Object System.Collections.ArrayList
    foreach ($lo in $leftovers) {
        $parts = $lo -split '/', 2
        $full = $savedFull + '\' + $parts[0] + '\' + $parts[1]
        $it = [pscustomobject]@{ Rel = $lo; Full = $full; Base = ''; Dest = ''; Sha = ''; Action = 'unresolved'; Reason = ''; Result = '' }
        $mt = [regex]::Match($parts[1], '^(?<b>.+)\.(?<s>restore\.old|restore\.tmp|rollback\.old|rollback-conflict)\z')
        if (-not $mt.Success) { $it.Reason = 'unknown_artifact'; [void]$items.Add($it); continue }
        $it.Base = $parts[0] + '/' + $mt.Groups['b'].Value
        $it.Dest = $savedFull + '\' + $parts[0] + '\' + $mt.Groups['b'].Value
        $sfx = $mt.Groups['s'].Value
        if ((Get-MMOSaveFileClass $it.Base).Class -eq 'ignored') { $it.Reason = 'base_not_whitelisted'; [void]$items.Add($it); continue }
        if (Test-MMOReparse $full) { $it.Reason = 'leftover_reparse'; [void]$items.Add($it); continue }
        if ($sfx -ceq 'rollback.old' -or $sfx -ceq 'rollback-conflict') { $it.Reason = 'manual_rollback_artifact'; [void]$items.Add($it); continue }
        try { $it.Sha = Get-MMOFileSha256 $full } catch { $it.Reason = 'unreadable'; [void]$items.Add($it); continue }
        if ($sfx -ceq 'restore.old') {
            $pe = $null
            if ($preByPath.ContainsKey($it.Base)) { $pe = $preByPath[$it.Base] }
            if ($null -eq $pe -or ([string]$pe.notes -ceq 'read_error') -or ([string]$pe.notes -ceq 'too_large') -or ([string]$pe.sha256 -cne $it.Sha)) { $it.Reason = 'old_hash_not_in_preimage'; [void]$items.Add($it); continue }
            $ds = Get-RsDestState $it.Dest
            if ($ds.Kind -eq 'missing') { $it.Action = 'old_restore'; $it.Reason = 'dest_missing_hash_matches_preimage' }
            elseif ($ds.Kind -eq 'file') { $it.Action = 'old_quarantine'; $it.Reason = 'dest_present_copy_in_preimage' }
            else { $it.Reason = 'dest_not_file' }
        } else {
            $planned = ''
            if ($planMap.ContainsKey($it.Base)) { $planned = [string]$planMap[$it.Base].sha }
            if ($planned -ne '' -and $planned -ceq $it.Sha) { $it.Action = 'tmp_discard'; $it.Reason = 'equals_planned_content' }
            else { $it.Action = 'tmp_quarantine'; $it.Reason = $(if ($planned -eq '') { 'no_plan_to_compare' } else { 'differs_from_planned_content' }) }
        }
        [void]$items.Add($it)
    }
    $nUnres = 0
    foreach ($it in $items) {
        if ($it.Action -ceq 'unresolved') { $nUnres++ }
        Write-RsLine '_RECOVER' ('action=' + $it.Action + ' path=' + $it.Rel + ' reason=' + $it.Reason) ('action=' + $it.Action + ' reason=' + $it.Reason)
    }
    Write-RsLine '_SUMMARY' ('recover leftovers=' + $items.Count + ' unresolved=' + $nUnres + ' marker=' + ([string]$MarkerState.Present).ToLowerInvariant() + ' plan_items=' + $planMap.Count + ' chain=' + $chainCount) ('recover leftovers=' + $items.Count + ' unresolved=' + $nUnres + ' chain=' + $chainCount)
    if ($markerKnown) {
        $stc = Get-RsPlanState -PlanMap $planMap
        Write-RsLine '_RECOVER_STATE' ('at_preimage=' + $stc.Pre + ' at_planned=' + $stc.New + ' other=' + $stc.Other + ' (kurtarma oncesi; yarim kalan dosyalar other sayilir)') ('at_preimage=' + $stc.Pre + ' at_planned=' + $stc.New + ' other=' + $stc.Other)
    }
    if (-not $Apply) {
        $pc = Get-RsBlockers
        if ($null -ne $pc.Err) { Write-RsLine '_WARN' ('code=apply_would_defer reason=' + $pc.Err) }
        if ($nUnres -gt 0) { Add-RsNote 'recover_unresolved' }
        Write-RsLine '_WARN' 'code=other_machines (surec denetimi YALNIZ bu makineyi gorur: ayni Saved\ dizinine yazan baska makinedeki sunuculari da kapattigini dogrula)'
        Write-RsLine '_NEXT' ('uygulamak icin: RestoreSaves.ps1 -Recover -Snapshot ' + (Format-RsArg $snapFull) + ' -ExpectedManifestSha256 <yedek alirken / backup.log\da KAYDETTIGIN pre-restore ozeti> -Apply')
        Write-RsLine '_COMPUTED_SHA256' ('sha256=' + $ManifestSha + ' (bu degeri komuta KOPYALAMA: pre-restore backup.log manifest_sha256 / yarim kalan kosunun yazdigi komutla karsilastir - docs/DESIGN_BACKUP.md K6)')
        return
    }
    # --- UYGULA ---
    if ($MarkerState.Present) { [void]$script:Excl.Add($script:MirrorPath) }
    Write-RsLine '_WARN' 'code=other_machines (surec denetimi YALNIZ bu makineyi gorur: ayni Saved\ dizinine yazan baska makinedeki sunuculari da kapattigini dogrula)'
    if ($null -eq (Get-RsStatusRoot)) { Stop-Restore 3 'restore_marker_root_unsafe' }
    if (-not (Open-RsLock)) { Stop-Restore 3 'lock_held' }
    Assert-RsQuiet
    $stampFile = $Timestamp
    if ($stampFile -eq '') { $stampFile = (Get-Date).ToUniversalTime().ToString("yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture) }
    $qroot = $script:RootFull + '\restore_recover_' + $stampFile
    try { [void][IO.Directory]::CreateDirectory($qroot) } catch { Stop-Restore 1 'recover_dir_create_failed' }
    if (Test-MMOReparse $qroot) { Stop-Restore 1 'recover_dir_reparse' }
    $script:RecoverDir = $qroot
    $nDone = 0
    foreach ($it in $items) {
        if ($it.Action -ceq 'unresolved') { continue }
        $res = 'failed'
        try {
            Invoke-RsHook 'BeforeRecoverStep' @($it.Action, $it.Rel)
            # her adimda durum yeniden dogrulanir (planlama ile uygulama arasinda degismis olabilir)
            $shaNow = Get-MMOFileSha256 $it.Full
            $dsNow = Get-RsDestState $it.Dest
            if ($shaNow -cne $it.Sha) { $res = 'failed' }
            elseif ($it.Action -ceq 'old_restore') {
                if ($dsNow.Kind -eq 'missing') {
                    [IO.File]::Move($it.Full, $it.Dest)
                    if ((Get-MMOFileSha256 $it.Dest) -ceq $it.Sha) { $res = 'ok' }
                }
            }
            elseif ($it.Action -ceq 'tmp_discard') {
                [IO.File]::Delete($it.Full)
                if ((Get-MMOAttributes $it.Full).State -eq 'missing') { $res = 'ok' }
            }
            else {
                if ($it.Action -ceq 'old_quarantine' -and $dsNow.Kind -ne 'file') { $res = 'failed' }
                else {
                    $qd = $qroot + '\quarantine\' + ([string]$it.Rel).Replace('/', '\')
                    $res = Move-RsAside -From $it.Full -To $qd -Sha $it.Sha
                    if ($res -ceq 'both') { Write-RsLine '_WARN' ('code=quarantine_both_present path=' + $it.Rel + ' (kopyala-sil yarida kaldi: dosya hem yerinde hem karantinada; ikisi de SILINMEDI, elle incele)') 'code=quarantine_both_present' }
                }
            }
        } catch { $res = 'failed' }
        $it.Result = $res
        if ($res -ceq 'ok') { $nDone++ } else { $nUnres++ }
        Write-RsLine '_RECOVER_DONE' ('action=' + $it.Action + ' path=' + $it.Rel + ' result=' + $res) ('action=' + $it.Action + ' result=' + $res)
    }
    $leftNow = Get-RsLeftovers -SavedFull $savedFull
    $stAfter = $null
    if ($markerKnown) { $stAfter = Get-RsPlanState -PlanMap $planMap }
    if ($null -ne $stAfter) { Write-RsLine '_RECOVER_STATE' ('after at_preimage=' + $stAfter.Pre + ' at_planned=' + $stAfter.New + ' other=' + $stAfter.Other) ('after at_preimage=' + $stAfter.Pre + ' at_planned=' + $stAfter.New + ' other=' + $stAfter.Other) }
    if ($nUnres -gt 0 -or $leftNow.Count -gt 0) {
        Write-RsLine '_WARN' ('code=recover_unresolved count=' + [Math]::Max($nUnres, $leftNow.Count) + ' (isaret KALDIRILMADI; kalan yapilari elle incele)') 'code=recover_unresolved'
        Stop-Restore 1 'recover_incomplete'
    }
    if ($MarkerState.Present) {
        # plan eksiksiz olmali (write_count ile ayni sayida kayit) ve hicbir dosya "baska" durumda olmamali; yoksa isaret KALIR
        $consistent = ($null -ne $stAfter) -and ($planMap.Count -eq $expectedPlan) -and ($stAfter.Other -eq 0) -and (($stAfter.New -eq 0) -or ($stAfter.Pre -eq 0))
        if ($chainCount -gt 0) {
            Write-RsLine '_WARN' ('code=restore_chain_open count=' + $chainCount + ' (isaret onceki yarim kalmis geri yuklemelere zincirli: KALDIRILMADI)') ('code=restore_chain_open count=' + $chainCount)
            Add-RsNote 'restore_chain_open'
        } elseif ($consistent) {
            $mleft = Remove-RsMarkers $MarkerState
            if ($mleft -eq 0) { Write-RsLine '_RECOVER_STATE' 'marker_removed=true' 'marker_removed=true' } else { Write-RsLine '_WARN' 'code=restore_marker_left'; Add-RsNote 'restore_marker_left' }
        } else {
            Write-RsLine '_WARN' 'code=recover_state_mixed (hedef dosyalar kismen yeni kismen eski durumda: is-isareti KALDIRILMADI)' 'code=recover_state_mixed'
            Write-RsLine '_NEXT' ('pre-restore goruntusunden geri al: ' + (Get-RsRollbackCommandFor -Scope ([string](Get-MMOMember $StaleMarker 'scope'))))
            Write-RsRollbackWarnings
            Add-RsNote 'recover_state_mixed_marker_kept'
        }
    }
    Write-RsLine '_APPLIED' ('recovered=' + $nDone + ' unresolved=0 preimage=' + $script:PreName) ('recovered=' + $nDone + ' unresolved=0')
}

function Get-RsPlanState {
    # Is-isareti planindaki her dosyanin simdiki durumu: Pre (onceki icerik / yeni dosya icin yok), New (planlanan icerik), Other.
    param($PlanMap)
    $pre = 0; $new = 0; $oth = 0
    foreach ($k in $PlanMap.Keys) {
        $pi = $PlanMap[$k]
        $dest = $null
        try { $dest = Resolve-MMOSafePath -Root $script:SavedFull -RelPath ([string]$k) -AllowRootAncestorReparse:$AllowRootAncestorReparse } catch { $oth++; continue }
        $st = Get-RsDestState $dest
        $atNew = ($st.Kind -eq 'file' -and $st.Sha -ceq [string]$pi.sha)
        $atPre = $false
        if ([string]$pi.target -ceq 'new') { $atPre = ($st.Kind -eq 'missing') }
        else { $atPre = ($st.Kind -eq 'file' -and $st.Sha -ceq [string]$pi.prev) }
        if ($atNew) { $new++ } elseif ($atPre) { $pre++ } else { $oth++ }
    }
    return [pscustomobject]@{ Pre = $pre; New = $new; Other = $oth }
}

function Get-RsRollbackCommandFor {
    # Recover: is-isaretindeki kapsamdan ('all' | 'slots:a,b') geri alma komutu.
    param([string]$Scope)
    if ($Scope -cmatch '^slots:(?<k>[a-z0-9_,-]+)\z') { $script:ScopeSlots = $Matches['k'] }
    return (Get-RsRollbackCommand)
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
    if ($Recover) {
        $script:RunKind = 'recover'
        if ($All -or $useSlots) { Stop-Restore 2 'recover_excludes_scope' }
    } else {
        if ($All -and $useSlots) { Stop-Restore 2 'all_and_slots_exclusive' }
        if (-not $All -and -not $useSlots) { Stop-Restore 2 'scope_required_all_or_slots' }
        if ($useSlots -and -not $AcknowledgeCrossSlotRisk) { Stop-Restore 2 'slots_require_acknowledge_cross_slot_risk' }
    }
    if ($useSlots) { $script:ScopeSlots = ($slotKeys.ToArray() -join ',') }
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
    if ($AcceptCollisions -and $script:SnapName -cnotmatch '^[0-9]{8}T[0-9]{6}Z-prerestore\z') { Stop-Restore 2 'accept_collisions_only_for_preimage' }
    if ($Apply) { $script:Attempted = $true }
    foreach ($fn in 'Recover', 'All', 'IncludeInvalid', 'IncludeSideFiles', 'AcceptLiveSnapshot', 'AcceptDifferentSource', 'AcceptCollisions', 'AcknowledgeCrossSlotRisk', 'AcknowledgePartialState', 'AcknowledgeStaleRestoreMarker', 'QuarantineStaleSideFiles', 'AllowRootAncestorReparse') {
        if ($script:Bound.ContainsKey($fn) -and [bool]$script:Bound[$fn]) { [void]$script:Flags.Add($fn) }
    }
    if ($useSlots) { [void]$script:Flags.Add('Slots') }
    if ($script:Bound.ContainsKey('RecentWriteSeconds')) { [void]$script:Flags.Add('RecentWriteSeconds=' + $RecentWriteSeconds) }
    $script:ProcessCheck = 'cim'
    if ($null -ne $ProcessProvider) { $script:ProcessCheck = 'injected' }

    # 2. Yol yapisi: goruntu Saved icinde olamaz; Saved goruntu / yedek koku icinde ya da onlari iceren bir yol olamaz; hedef Saved gecerli kok (var, reparse degil)
    if (Test-MMOPathUnder -Path $snapFull -Root $savedFull -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'snapshot_inside_saved' }
    if (Test-MMOPathUnder -Path $savedFull -Root $snapFull -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'saved_inside_snapshot' }
    if (Test-MMOPathUnder -Path $savedFull -Root $script:RootFull -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'saved_inside_backup_root' }
    if (Test-MMOPathUnder -Path $script:RootFull -Root $savedFull -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'backup_root_inside_saved' }
    $srcErr = $null
    try { [void](Assert-MMORootSafe -Root $savedFull -AllowRootAncestorReparse:$AllowRootAncestorReparse) } catch { $srcErr = $_.Exception.Message }
    if ($null -ne $srcErr) { Stop-Restore 3 ('saved_' + ($srcErr -split ':', 2)[0]) }
    $sa = Get-MMOAttributes $snapFull
    if ($sa.State -eq 'missing') { Stop-Restore 3 'snapshot_missing' }
    if ($sa.State -ne 'ok' -or (($sa.Attributes -band [IO.FileAttributes]::Directory) -eq 0)) { Stop-Restore 3 'snapshot_inaccessible' }
    if (($sa.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { Stop-Restore 3 'snapshot_reparse' }
    if (-not $AllowRootAncestorReparse -and $null -ne (Get-MMOReparseAncestor $snapFull)) { Stop-Restore 3 'snapshot_reparse_ancestor' }
    # B3.2 (D1): GERCEK yol iliskisi (8.3 kisa ad / subst / junction cozulmus). Cozulemeyen yol -AllowRootAncestorReparse ile fail-closed (3); aksi halde
    # yukaridaki sozdizimsel denetim gecerlidir (atalarda reparse zaten reddedildi).
    $fsnap = Get-MMOFinalPath $snapFull
    $fsav = Get-MMOFinalPath $savedFull
    $froot = Get-MMOFinalPath $script:RootFull
    if ($null -eq $fsnap -or $null -eq $fsav -or $null -eq $froot) {
        if ($AllowRootAncestorReparse) { Stop-Restore 3 'final_path_unresolved' }
    } else {
        if (Test-MMOPathUnder -Path $fsnap -Root $fsav -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'snapshot_inside_saved' }
        if (Test-MMOPathUnder -Path $fsav -Root $fsnap -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'saved_inside_snapshot' }
        if (Test-MMOPathUnder -Path $fsav -Root $froot -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'saved_inside_backup_root' }
        if (Test-MMOPathUnder -Path $froot -Root $fsav -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'backup_root_inside_saved' }
    }

    # 2b. Bayat is-isareti (yarida kesilmis onceki -Apply): varken yeni -Apply reddedilir; kuru calisma uyarir.
    # B3.2 (D6): isaretin IKI kopyasi da (BackupRoot + Saved yansimasi) denetlenir; biri varsa isaret var sayilir.
    $mkPaths = Get-MMORestoreMarkerPaths -SavedFull $savedFull -RootFull $script:RootFull
    $script:MarkerPath = $mkPaths.Primary
    $script:MirrorPath = $mkPaths.Mirror
    $mkState = Get-MMORestoreMarkerState -SavedFull $savedFull -RootFull $script:RootFull
    $staleMarker = $null
    if ($mkState.Present) {
        $mi = ''
        $mo = Read-RsMarker $mkState
        if ($null -ne $mo) {
            $mi = ' snapshot=' + [string](Get-MMOMember $mo 'snapshot') + ' preimage=' + [string](Get-MMOMember $mo 'preimage') + ' time_utc=' + [string](Get-MMOMember $mo 'time_utc')
            $mbr = [string](Get-MMOMember $mo 'backup_root')
            if ($mbr -ne '' -and -not $mbr.TrimEnd('\').Equals($script:RootFull, [StringComparison]::OrdinalIgnoreCase)) { $mi += ' marker_backup_root_differs=true' }
        } else { $mi = ' (is-isareti okunamadi)' }
        if ($mkState.PrimaryPresent -ne $mkState.MirrorPresent) { $mi += ' copies=' + $(if ($mkState.PrimaryPresent) { 'backup_root_only' } else { 'saved_mirror_only' }) }
        $staleMarker = $mo
        if ($Recover) {
            Write-RsLine '_WARN' ('code=restore_in_progress_marker' + $mi + ' (cokus kurtarma modu: isaret ve pre-restore goruntusu birlikte degerlendirilir)')
        } else {
            Write-RsLine '_WARN' ('code=restore_in_progress_marker' + $mi + ' (onceki -Apply yarida kesilmis olabilir: Saved\ yari geri yuklenmis olabilir. Once -Recover (kuru calisma) ile yarim kalan yapilari incele; Sunucu kapaliyken Saved\ durumunu pre-restore goruntusuyle karsilastir; pre-restore goruntusunden geri yukle ya da durumu kontrol edip -AcknowledgeStaleRestoreMarker ile bilincli devam et: eski isaret .stale olarak TASINIR ve yeni isarete zincir olarak eklenir)')
            if ($Apply) {
                if (-not $AcknowledgeStaleRestoreMarker) { Stop-Restore 3 'restore_in_progress_marker' }
                Write-RsLine '_FORCE' 'flag=AcknowledgeStaleRestoreMarker' 'flag=AcknowledgeStaleRestoreMarker'
                Add-RsNote 'stale_marker_acknowledged'
            } else { Add-RsNote 'restore_marker_present' }
        }
    }

    # 3. Goruntu dogrulamasi (kapi 1): yarim goruntu kalintisi, manifest TEK KEZ okunur (ozet + ayristirma ayni bayt dizisi), Test-MMOSnapshot
    foreach ($leftover in 'manifest.json.failed', 'manifest.json.tmp') {
        if ((Get-MMOAttributes ($snapFull + '\' + $leftover)).State -ne 'missing') { Stop-Restore 1 'snapshot_incomplete' }
    }
    $mpath = $snapFull + '\manifest.json'
    if (-not [IO.File]::Exists($mpath)) { Stop-Restore 1 'snapshot_incomplete' }
    $mbytes = $null
    try { $mbytes = Read-MMOFileBytes -Path $mpath -MaxBytes $script:MMOSvMaxManifestBytes } catch { Stop-Restore 1 'manifest_unreadable' }
    $sha1 = Get-MMOBytesSha256 $mbytes
    $rm = Read-MMOManifest -SnapshotDir $snapFull -AllowRootAncestorReparse:$AllowRootAncestorReparse -ManifestBytes $mbytes
    $t = Test-MMOSnapshot -SnapshotDir $snapFull -AllowRootAncestorReparse:$AllowRootAncestorReparse -ManifestBytes $mbytes
    Invoke-RsHook 'AfterVerify' @($snapFull)
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
    if ($Recover) {
        Invoke-RsRecover -Manifest $m -ManifestSha $sha1 -StaleMarker $staleMarker -MarkerState $mkState
        return
    }

    # 4. Goruntu ozellikleri: live, cakisma, kaynak koku, uyarilar
    $mode = [string](Get-MMOMember $m 'mode')
    $mLabel = Get-MMOMember $m 'label'
    $mTool = [string](Get-MMOMember $m 'tool')
    $labelText = 'none'
    if ($mLabel -is [string]) { $labelText = $mLabel }
    $proofStale = ((Get-MMOMember $m 'proof_stale') -eq $true)
    $ignSummary = [string](Get-MMOMember $m 'ignored_summary')
    $ignPd = 0
    if ($ignSummary -cmatch 'player_data=(?<n>[0-9]+)') { $ignPd = [int]$Matches['n'] }
    $collList = @()
    $cv = Get-MMOMember $m 'collisions'
    if ($null -ne $cv) { $collList = [string[]]@(@($cv) | ForEach-Object { [string]$_ }) }
    $srcDir = ([string](Get-MMOMember $m 'source_saved_dir')).TrimEnd('\')
    $srcMatch = $srcDir.Equals($savedFull, [StringComparison]::OrdinalIgnoreCase)
    Write-RsLine '_SNAPSHOT' ('name=' + $script:SnapName + ' label=' + $labelText + ' tool=' + $mTool + ' mode=' + $mode + ' created_utc=' + (ConvertTo-MMOUtcString (Get-MMOMember $m 'created_utc')) + ' entries=' + @($m.entries).Count + ' source_matches=' + ([string]$srcMatch).ToLowerInvariant() + ' proof_stale=' + ([string]$proofStale).ToLowerInvariant() + ' collisions=' + $collList.Count + ' ignored_player_data=' + $ignPd) `
        ('mode=' + $mode + ' entries=' + @($m.entries).Count + ' source_matches=' + ([string]$srcMatch).ToLowerInvariant() + ' proof_stale=' + ([string]$proofStale).ToLowerInvariant() + ' collisions=' + $collList.Count)
    $collForSel = $collList
    if ($collList.Count -gt 0) {
        foreach ($cg in $collList) { Write-RsLine '_WARN' ('code=collisions group=' + $cg) }
        if (-not $AcceptCollisions) { Stop-Restore 1 'snapshot_has_collisions' }
        # B3.2 (D2): dizin adi tek basina yetmez; manifestin kendi etiketi 'prerestore' ve araci BackupSaves olmali (manifest header ozetine bagli)
        if ($labelText -cne 'prerestore' -or -not $mTool.StartsWith('BackupSaves/', [StringComparison]::Ordinal)) { Stop-Restore 1 'accept_collisions_label_mismatch' }
        Write-RsLine '_FORCE' 'flag=AcceptCollisions reason=preimage_collisions' 'flag=AcceptCollisions'
        Add-RsNote 'collisions_accepted'
        $collForSel = @()
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
    if ($Apply -and $RecentWriteSeconds -lt 60) {
        Write-RsLine '_FORCE' ('flag=RecentWriteSeconds value=' + $RecentWriteSeconds + ' (canli yazim penceresi 60 sn altina indirildi)') ('flag=RecentWriteSeconds value=' + $RecentWriteSeconds)
        Add-RsNote 'forced'
    }

    # 5. Secim (kapi 2/3): yan dosya / valid=false / unstable / no_content varsayilan disarida; kapsam -All ya da -Slots
    $entries = @($m.entries)
    $entryByPath = @{}
    foreach ($e in $entries) { $entryByPath[[string]$e.path] = $e }
    $sel = Get-MMORestoreSelection -Entries $entries -IncludeSideFiles:$IncludeSideFiles -IncludeInvalid:$IncludeInvalid -CollisionPaths $collForSel
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
    $restoreSet = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($sr in $sortedRels) { [void]$restoreSet.Add($sr) }
    foreach ($rel in $sortedRels) {
        $e = $entryByPath[$rel]
        $c = Get-MMOSaveFileClass $rel
        $dest = $null
        try { $dest = Resolve-MMOSafePath -Root $savedFull -RelPath $rel -AllowRootAncestorReparse:$AllowRootAncestorReparse } catch { Write-RsLine '_WARN' ('code=target_path_unsafe path=' + $rel); Stop-Restore 1 'target_path_unsafe' }
        # Hedefte ayni kanonik anahtarli (tur, sonek, grup) ama FARKLI yazimli dosya (Ali / ali): Windows'ta ayni dosya = RED.
        # Ayni kumede ('prerestore' goruntusunde bildirilen, kabul edilmis cakisma) birlikte geri yuklenen dosya cakisma sayilmaz.
        $dn = ($rel -split '/', 2)[0]
        $fn = ($rel -split '/', 2)[1]
        if ($dirNames.ContainsKey($dn)) {
            foreach ($other in $dirNames[$dn]) {
                if ($other -ceq $fn) { continue }
                if ($restoreSet.Contains($dn + '/' + $other)) { continue }
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

    # 7. Fazlalik (silinmez, raporlanir), eski .tmp/.bak artiklari, canli kilitler, onceki kosudan kalan yapitlar
    $extras = 0
    $stale = 0
    $locks = 0
    $staleList = New-Object System.Collections.ArrayList
    foreach ($lf in $live) {
        if (-not (& $inScope $lf)) { continue }
        $lc = Get-MMOSaveFileClass $lf
        if (-not $entryByPath.ContainsKey($lf)) { $extras++; Write-RsLine '_EXTRA' ('path=' + $lf + ' (goruntude yok; silinmez)') }
        $isTmpBak = ($lc.Class -eq 'side' -and ($lc.Suffix -ceq '.tmp' -or $lc.Suffix -ceq '.bak'))
        $isLock = ($lc.Class -eq 'side-lock')
        if (($isTmpBak -or $isLock) -and -not $restoreSet.Contains($lf)) {
            # Goruntudeki ile BAYTI AYNI olan canli yan dosya goruntu aniyla tutarlidir (eski artik degil); farkli / goruntude olmayan = eski artik.
            $same = $false
            if ($entryByPath.ContainsKey($lf)) {
                try { $same = ((Get-MMOFileSha256 ($savedFull + '\' + $lf.Replace('/', '\'))) -ceq [string]$entryByPath[$lf].sha256) } catch { $same = $false }
            }
            if (-not $same) {
                if ($isTmpBak) {
                    $stale++
                    [void]$staleList.Add($lf)
                    Write-RsLine '_WARN' ('code=stale_side_file path=' + $lf + ' (oyunun kurtarma mantigi eski artigi geri getirebilir; -QuarantineStaleSideFiles ile pre-restore dizinine tasinir)')
                } else {
                    $locks++
                    Write-RsLine '_WARN' ('code=live_lock_file path=' + $lf + ' (canli .bad kilidi goruntude yok / farkli: TASINMAZ ve dusurulmez; kilit geri yuklenen veriyle uyumsuz olabilir, operator incelemeli)')
                }
            }
        }
    }
    if ($stale -gt 0) { Add-RsNote 'stale_side_files' }
    if ($locks -gt 0) { Add-RsNote 'live_lock_files' }
    $leftovers = Get-RsLeftovers -SavedFull $savedFull
    foreach ($lo in $leftovers) { Write-RsLine '_WARN' ('code=leftover_restore_artifact path=' + $lo + ' (onceki kosudan kalan yapi; silinmez - .restore.old eski oyuncu verisi olabilir)') }
    if ($leftovers.Count -gt 0) { Add-RsNote 'leftover_restore_artifacts' }
    # Kismi durum: -All'da EXTRA / geri yuklenmeyen giris; -Slots'ta geri yuklenmeyen canli posta kutularinda slota atif ya da dogrulanamayan kutu
    $ackReasons = New-Object System.Collections.ArrayList
    if (-not $useSlots) {
        if ($extras -gt 0) { [void]$ackReasons.Add('extras=' + $extras) }
        if ($notRestored -gt 0) { [void]$ackReasons.Add('not_restored=' + $notRestored) }
    } else {
        $mf = Get-RsMailFindings -SavedFull $savedFull -Live $live -SlotKeys $slotKeys
        foreach ($tr in $mf.Traces) { $tp = $tr -split '\|', 2; Write-RsLine '_WARN' ('code=mail_trace mailbox=' + $tp[0] + ' slot=' + $tp[1] + ' (baska kutuda geri yuklenen slota atif var: gonderilen altin geri yuklenmeyecek, cogalabilir)'); }
        foreach ($uv in $mf.Unverified) { Write-RsLine '_WARN' ('code=mail_unverified mailbox=' + $uv + ' (ayristirilamadi / sema belirsiz: slota atif DOGRULANAMADI)') }
        foreach ($sv in $mf.SideUnverified) { Write-RsLine '_WARN' ('code=mail_side_unverified mailbox=' + $sv + ' (.bak/.tmp posta kutusu ayristirilamadi / sema belirsiz: slota atif DOGRULANAMADI; yalniz uyari)') }
        if ($mf.Traces.Count -gt 0) { [void]$ackReasons.Add('mail_traces=' + $mf.Traces.Count) }
        if ($mf.Unverified.Count -gt 0) { [void]$ackReasons.Add('mail_unverified=' + $mf.Unverified.Count) }
    }
    $needAck = ($ackReasons.Count -gt 0)
    if ($needAck) { Write-RsLine '_WARN' ('code=partial_state ' + ($ackReasons.ToArray() -join ' ') + ' (-Apply icin -AcknowledgePartialState gerekir)') }
    Write-RsLine '_WARN' 'code=cross_slot (geri yukleme posta/ticaret ile baglantili altin / esya tutarliligini bozabilir: goruntu ile simdi arasindaki ticaret/posta etkileri GERI ALINMAZ ve arac bunlari DOGRULAYAMAZ; -All da -Slots da tutarlilik garantisi vermez)'
    Write-RsLine '_SUMMARY' ('write=' + $nWrite + ' unchanged=' + $nSame + ' skipped=' + $skipList.Count + ' skipped_side_files=' + $skipSide + ' not_restored=' + $notRestored + ' extras=' + $extras + ' stale_side_files=' + $stale + ' live_locks=' + $locks + ' live_files=' + $live.Count) `
        ('write=' + $nWrite + ' unchanged=' + $nSame + ' skipped=' + $skipList.Count + ' not_restored=' + $notRestored + ' extras=' + $extras + ' stale_side_files=' + $stale + ' live_locks=' + $locks)

    # 8. Kuru calisma: burada biter (hicbir sey yazilmadi)
    if (-not $Apply) {
        $pc = Get-RsBlockers
        if ($null -ne $pc.Err) { Write-RsLine '_WARN' ('code=apply_would_defer reason=' + $pc.Err) }
        if ($needAck) { Write-RsLine '_WARN' 'code=apply_would_refuse reason=partial_state_requires_acknowledge' }
        if ($stale -gt 0) { Write-RsLine '_WARN' 'code=apply_would_refuse reason=stale_side_files_require_quarantine_flag' }
        if ($leftovers.Count -gt 0) { Write-RsLine '_WARN' 'code=apply_would_refuse reason=leftover_restore_artifacts_require_recover (once -Recover)' }
        Write-RsLine '_WARN' 'code=other_machines (surec denetimi YALNIZ bu makineyi gorur: ayni Saved\ dizinine yazan baska makinedeki sunuculari da kapattigini dogrula)'
        Write-RsLine '_NEXT' ('uygulamak icin: RestoreSaves.ps1 -Snapshot ' + (Format-RsArg $snapFull) + ' ' + $(if ($useSlots) { '-Slots ' + ($slotKeys.ToArray() -join ',') + ' -AcknowledgeCrossSlotRisk' } else { '-All' }) + ' -ExpectedManifestSha256 <yedek alirken KAYDETTIGIN ozet> -Apply')
        Write-RsLine '_COMPUTED_SHA256' ('sha256=' + $sha1 + ' (bu degeri komuta KOPYALAMA: yedek alirken ayri yere kaydettigin ozetle karsilastir; imzasiz yedekte tek koruma budur - docs/DESIGN_BACKUP.md K6)')
        return
    }

    # 9. UYGULA: onay kapilari -> yarim kalmis yapi kapisi -> kilit -> kapi 6 (surec + canli yazim) -> kapi 7 (pre-restore, ONCE) -> kapi 6 yeniden -> is-isareti -> yazim -> dogrulama / rollback
    # B3.2 (O1): onceki kosudan kalan *.restore.old / *.restore.tmp / *.rollback.* yapilari varken normal -Apply pre-restore'dan ONCE kod 3 ile reddedilir;
    # once -Recover (kuru calisma -> -Apply) gerekir. Isaret bayragi (-AcknowledgeStaleRestoreMarker) bunu AKLAMAZ.
    if ($leftovers.Count -gt 0) {
        Write-RsLine '_WARN' ('code=recover_required count=' + $leftovers.Count + ' (yarim kalmis restore yapilari: RestoreSaves.ps1 -Recover -Snapshot <pre-restore goruntusu> -ExpectedManifestSha256 <ozet>; once kuru calisma)') ('code=recover_required count=' + $leftovers.Count)
        Stop-Restore 3 'leftover_restore_artifacts_require_recover'
    }
    if ($needAck) {
        if (-not $AcknowledgePartialState) { Stop-Restore 1 'partial_state_requires_acknowledge' }
        Write-RsLine '_FORCE' ('flag=AcknowledgePartialState ' + ($ackReasons.ToArray() -join ' ')) 'flag=AcknowledgePartialState'
        Add-RsNote 'partial_state_acknowledged'
    }
    if ($stale -gt 0 -and -not $QuarantineStaleSideFiles) { Stop-Restore 1 'stale_side_files_require_quarantine_flag' }
    Write-RsLine '_WARN' 'code=other_machines (surec denetimi YALNIZ bu makineyi gorur: ayni Saved\ dizinine yazan baska makinedeki sunuculari da kapattigini dogrula)'
    # O3: yedek koku guvenli olmali ve <BackupRoot>\.lock -Apply boyunca tutulur (BackupSaves / ikinci bir -Apply ayni anda calisamaz). Kilit HICBIR dosya
    # yazilmadan ONCE alinir; alinamazsa 3. Pre-restore cocugu icin kilit kisa sureligine devredilir (Invoke-RsPreimage).
    if ($null -eq (Get-RsStatusRoot)) { Stop-Restore 3 'restore_marker_root_unsafe' }
    if (-not (Open-RsLock)) { Stop-Restore 3 'lock_held' }
    Assert-RsQuiet
    $haveLive = ($live.Count -gt 0)
    $preDir = $null
    if ($haveLive) {
        Invoke-RsHook 'BeforePreimage' @(, $live)
        $preDir = Invoke-RsPreimage
        Invoke-RsHook 'AfterPreimageTaken' @($preDir)
        $pt = Test-MMOSnapshot -SnapshotDir $preDir -AllowRootAncestorReparse:$AllowRootAncestorReparse
        if (-not $pt.Ok -or $pt.Incomplete) { Stop-Restore 1 'preimage_verify_failed' }
        if ($pt.Mode -cne 'quiesced') { Stop-Restore 1 'preimage_not_quiesced' }
        $pre = Read-MMOManifest -SnapshotDir $preDir -AllowRootAncestorReparse:$AllowRootAncestorReparse
        if (-not $pre.Ok) { Stop-Restore 1 'preimage_verify_failed' }
        if (([string](Get-MMOMember $pre.Manifest 'label') -cne 'prerestore') -or -not ([string](Get-MMOMember $pre.Manifest 'tool')).StartsWith('BackupSaves/', [StringComparison]::Ordinal)) { Stop-Restore 1 'preimage_label_mismatch' }
        try { $script:PreSha = Get-MMOFileSha256 ($preDir + '\manifest.json') } catch { Stop-Restore 1 'preimage_verify_failed' }
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
    Invoke-RsHook 'AfterPreimage' @($preDir)
    Assert-RsQuiet
    # goruntu hala ayni mi (pre-restore sirasinda degisemez ama ucuz)
    $sha3 = $null
    try { $sha3 = Get-MMOFileSha256 $mpath } catch { Stop-Restore 1 'manifest_unreadable' }
    if ($sha3 -cne $sha1) { Stop-Restore 1 'manifest_changed_before_write' }

    # 9b. Is-isareti (ilk yazimdan ONCE; yazilamazsa HICBIR sey yazilmaz) + restore.log 'begin'
    $mroot = Get-RsStatusRoot
    if ($null -eq $mroot) { Stop-Restore 3 'restore_marker_root_unsafe' }
    $stampB = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture)
    Invoke-RsHook 'BeforeMarkerWrite' @($script:MarkerPath)
    # O2: bilincli kabul edilen BAYAT isaret silinmez: .stale adiyla TASINIR ve yeni isarete 'chain' olarak eklenir (eski preimage / ozet bilgisi korunur).
    $chain = New-Object System.Collections.ArrayList
    $markerNow = Get-MMORestoreMarkerState -SavedFull $savedFull -RootFull $script:RootFull
    if ($markerNow.Present) {
        if (-not $AcknowledgeStaleRestoreMarker) { Stop-Restore 3 'restore_marker_exists' }
        $stampFile = (Get-Date).ToUniversalTime().ToString("yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture)
        $oldMo = Read-RsMarker $markerNow
        $staleLeaf = @()
        foreach ($mp in @($markerNow.Primary, $markerNow.Mirror)) {
            if ((Get-MMOAttributes $mp).State -eq 'missing') { continue }
            # her iki .stale kopyasi da YEDEK KOKUNE tasinir (Saved\ icinde kalici yapi birakilmaz; silme yok)
            $dstBase = $script:RootFull + '\' + [IO.Path]::GetFileName($mp)
            $dst = $dstBase + '.' + $stampFile + '.stale'
            $sn = 0
            while ((Get-MMOAttributes $dst).State -ne 'missing') { $sn++; $dst = $dstBase + '.' + $stampFile + '.' + $sn + '.stale'; if ($sn -gt 99) { Stop-Restore 1 'restore_marker_write_failed' } }
            try { [IO.File]::Move($mp, $dst) } catch { Stop-Restore 1 'restore_marker_write_failed' }
            $staleLeaf += [IO.Path]::GetFileName($dst)
        }
        $entry = [ordered]@{ stale_files = [string[]]@($staleLeaf); readable = ($null -ne $oldMo); snapshot = ''; preimage = ''; manifest_sha256 = ''; preimage_manifest_sha256 = ''; time_utc = '' }
        if ($null -ne $oldMo) {
            foreach ($fk in 'snapshot', 'preimage', 'manifest_sha256', 'preimage_manifest_sha256', 'time_utc') { $entry[$fk] = [string](Get-MMOMember $oldMo $fk) }
            $oc = Get-MMOMember $oldMo 'chain'
            if ($null -ne $oc) { foreach ($ce in @($oc)) { [void]$chain.Add($ce) } }
        }
        [void]$chain.Insert(0, $entry)
        Write-RsLine '_WARN' ('code=stale_marker_chained count=' + $chain.Count + ' (eski isaret .stale olarak TASINDI, silinmedi; yeni isaret onu zincir olarak tasir)') ('code=stale_marker_chained count=' + $chain.Count)
    }
    $script:ChainOpen = $chain.Count
    $planItems = @($plan | Where-Object { $_.Action -ceq 'write' } | ForEach-Object { [ordered]@{ rel = [string]$_.Rel; sha = [string]$_.Sha; prev = [string]$_.PrevSha; target = [string]$_.Target } })
    $markerObj = [ordered]@{
        tool = 'RestoreSaves/3'; snapshot = $script:SnapName; preimage = $script:PreName; manifest_sha256 = $sha1; preimage_manifest_sha256 = $script:PreSha
        plan_count = $plan.Count; write_count = $nWrite; time_utc = $stampB; pid = $PID; saved_dir = $savedFull; backup_root = $script:RootFull
        scope = $(if ($useSlots) { 'slots:' + $script:ScopeSlots } else { 'all' }); chain = [object[]]@($chain.ToArray()); plan = [object[]]@($planItems) }
    $markerJson = $markerObj | ConvertTo-Json -Depth 8
    # O3 + D6: ikisi de EXCLUSIVE (var olan uzerine yazilmaz). Birincil basarisiz = HICBIR sey yazilmaz; yansima basarisizsa birincil geri alinir.
    $wr = Write-RsMarkerFile -Path $script:MarkerPath -Json $markerJson
    if ($wr -ceq 'exists') { Stop-Restore 3 'restore_marker_exists' }
    if ($wr -cne '') { Stop-Restore 1 'restore_marker_write_failed' }
    $wr2 = Write-RsMarkerFile -Path $script:MirrorPath -Json $markerJson
    if ($wr2 -cne '') {
        try { [IO.File]::Delete($script:MarkerPath) } catch { }
        if ($wr2 -ceq 'exists') { Stop-Restore 3 'restore_marker_exists' }
        Stop-Restore 1 'restore_marker_write_failed'
    }
    [void]$script:Excl.Add($script:MirrorPath)
    $script:MarkerWritten = $true
    [void]$script:Log.Add('[MMO_RESTORE_BEGIN] plan=' + $plan.Count + ' write=' + $nWrite + ' preimage=' + $script:PreName)
    [void](Write-RsLogFile)

    $attempted = New-Object System.Collections.ArrayList
    $quarantined = New-Object System.Collections.ArrayList
    $failReason = $null
    # 9c. Eski yan dosyalarin karantinaya alinmasi (silme YOK; tasima; rollback'te geri tasinir)
    if ($stale -gt 0 -and $QuarantineStaleSideFiles) {
        Write-RsLine '_FORCE' ('flag=QuarantineStaleSideFiles count=' + $stale) ('flag=QuarantineStaleSideFiles count=' + $stale)
        foreach ($qrel in $staleList) {
            try {
                if ($null -eq $script:PreDir) { throw 'quarantine_no_dir' }
                $qsrc = Resolve-MMOSafePath -Root $savedFull -RelPath $qrel -MustExist -AllowRootAncestorReparse:$AllowRootAncestorReparse
                $qsha = Get-MMOFileSha256 $qsrc
                $qdst = $script:PreDir + '\quarantine\' + ([string]$qrel).Replace('/', '\')
                if ((Get-MMOAttributes $qdst).State -ne 'missing') { throw 'quarantine_target_exists' }
                [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($qdst))
                $rec = [pscustomobject]@{ Rel = $qrel; From = $qsrc; To = $qdst; Sha = $qsha }
                Invoke-RsHook 'BeforeQuarantineMove' @($qsrc, $qdst)
                $mvFailed = $false
                try { [IO.File]::Move($qsrc, $qdst) } catch { $mvFailed = $true }
                if ($mvFailed) {
                    # D7: birimler arasi Move = kopyala-sil; yarida kalirsa iki yerde de dosya olabilir. Tasindi-ama-hata: rollback geri tasisin; iki yerde de var: rapor.
                    $fS = (Get-MMOAttributes $qsrc).State
                    $tS = (Get-MMOAttributes $qdst).State
                    if ($fS -eq 'missing' -and $tS -eq 'ok') { [void]$quarantined.Add($rec) }
                    elseif ($fS -ne 'missing' -and $tS -ne 'missing') { Write-RsLine '_WARN' ('code=quarantine_both_present path=' + $qrel + ' (kopyala-sil yarida kaldi: dosya hem yerinde hem karantinada; ikisi de SILINMEDI, elle incele)') 'code=quarantine_both_present' }
                    throw 'quarantine_move_failed'
                }
                [void]$quarantined.Add($rec)
                if ((Get-MMOFileSha256 $qdst) -cne $qsha) { throw 'quarantine_verify_failed' }
                Write-RsLine '_QUARANTINE' ('path=' + $qrel + ' moved_to=pre-restore\quarantine') ('moved=1')
            } catch {
                $qm = $_.Exception.Message
                if ($qm -cmatch '^[a-z_]+\z') { $failReason = $qm } else { $failReason = 'quarantine_failed' }
                break
            }
        }
    }

    $qEvery = 20
    if ($null -ne $TestHooks -and $TestHooks.ContainsKey('QuietEvery')) { $qEvery = [int]$TestHooks['QuietEvery'] }
    if ($null -eq $failReason) {
        Invoke-RsHook 'BeforeFirstWrite' @($preDir)
        $idx = 0
        foreach ($p in $plan) {
            if ($p.Action -cne 'write') { continue }
            $idx++
            try {
                Invoke-RsHook 'BeforeFile' @($p.Rel, $idx)
                # yazim sirasinda surec / canli yazim yeniden denetimi (hafif: yalniz oyuncu dizinleri, kendi yazdiklarimiz haric)
                if ((($idx - 1) % $qEvery) -eq 0) {
                    $qp = Get-RsQuietProblem -Exclude $script:Excl -Light
                    if ($null -ne $qp) { throw $qp }
                }
                # hedef hala goruntu alinirkenki gibi mi (TOCTOU)
                if ($p.Target -ceq 'overwrite') {
                    $cur = Read-MMOFileBytes $p.Dest
                    if ((Get-MMOBytesSha256 $cur) -cne $p.PrevSha) { throw 'target_changed_since_plan' }
                } elseif ((Get-MMOAttributes $p.Dest).State -ne 'missing') { throw 'target_appeared_since_plan' }
                $src = Resolve-MMOSafePath -Root ($snapFull + '\files') -RelPath $p.Rel -MustExist -AllowRootAncestorReparse:$AllowRootAncestorReparse
                $bytes = Read-MMOFileBytes $src
                if ([long]$bytes.Length -ne $p.Size) { throw 'source_size_mismatch' }
                if ((Get-MMOBytesSha256 $bytes) -cne $p.Sha) { throw 'source_hash_mismatch' }
                $rec = [pscustomobject]@{ Rel = $p.Rel; Dest = $p.Dest; Target = $p.Target; PrevSha = $p.PrevSha; Sha = $p.Sha; Old = $null; Placed = $false }
                [void]$attempted.Add($rec)
                Add-RsExclude $p.Dest
                $rec.Old = Write-RsFile -Dest $p.Dest -Bytes $bytes -Sha $p.Sha -Rel $p.Rel
                $rec.Placed = $true
                Invoke-RsHook 'AfterWrite' @($p.Rel, $p.Dest)
            } catch {
                $m0 = $_.Exception.Message
                if ($m0 -cmatch '^[a-z_]+\z') { $failReason = $m0 } else { $failReason = 'write_failed' }
                break
            }
        }
    }
    if ($null -eq $failReason) {
        # Sonrasi: yazilan her dosyayi yeniden oku + hash (kapi: tum yazilanlar)
        foreach ($a0 in $attempted) {
            $ok = $false
            try { $ok = ((Get-MMOFileSha256 $a0.Dest) -ceq $a0.Sha) } catch { $ok = $false }
            if (-not $ok) { $failReason = 'post_verify_failed'; break }
        }
    }
    if ($null -ne $failReason) {
        Invoke-RsHook 'BeforeRollback' @($preDir)
        $quietNow = Get-RsQuietProblem -Exclude $script:Excl
        if ($null -ne $quietNow) {
            # Sunucu / baska yazici varken rollback YAPILMAZ (kullanici verisinin uzerine yazmamak icin); durum acikca bildirilir.
            Write-RsLine '_ROLLBACK_FAILED' ('preimage=' + $script:PreName + ' reason=rollback_not_quiet:' + $quietNow + ' rollback YAPILMADI; sunucuyu kapat, en az ' + $RecentWriteSeconds + ' sn bekle, sonra: ' + (Get-RsRollbackCommand)) ('preimage=' + $script:PreName + ' reason=rollback_not_quiet:' + $quietNow)
            Write-RsRollbackWarnings
            Stop-Restore 1 ('rollback_failed:' + $failReason)
        }
        $rb = Invoke-RsRollback -Attempted $attempted -Quarantined $quarantined -PreDir $script:PreDir
        foreach ($cf in $rb.Conflicts) { Write-RsLine '_ROLLBACK_CONFLICT' ('path=' + $cf + ' (hedef degistirilmis: UZERINE YAZILMADI; pre-restore baytlari <ad>.rollback-conflict olarak yana yazildi)') ('conflict=1') }
        foreach ($bt in $rb.Both) { Write-RsLine '_WARN' ('code=quarantine_both_present path=' + $bt + ' (dosya hem yerinde hem karantinada; ikisi de SILINMEDI, elle incele)') 'code=quarantine_both_present' }
        if ($rb.Failures.Count -gt 0) {
            foreach ($fl in $rb.Failures) { Write-RsLine '_ERROR' ('rollback ' + $fl) }
            Write-RsLine '_ROLLBACK_FAILED' ('preimage=' + $script:PreName + ' reason=rollback_item_failed elle geri al: ' + (Get-RsRollbackCommand)) ('preimage=' + $script:PreName + ' reason=rollback_item_failed')
            Write-RsRollbackWarnings
            Stop-Restore 1 ('rollback_failed:' + $failReason)
        }
        if ($rb.Conflicts.Count -gt 0) { Stop-Restore 1 ('rollback_conflict:' + $failReason) }
        # TAM rollback: is-isareti silinir -- ANCAK kapanmamis bir zincir (bilincli kabul edilen eski bayat isaret) varsa SILINMEZ: rollback bu kosunun basindaki
        # (eski kesintiden kalma, belirsiz) duruma doner, o durum hala cozulmus degildir (O2).
        if ($script:ChainOpen -gt 0) {
            Write-RsLine '_WARN' ('code=restore_chain_open count=' + $script:ChainOpen + ' (rollback tamamlandi ama onceki yarim kalmis geri yukleme zinciri KAPANMADI: is-isareti kaldirilmadi; eski .stale isaretlerindeki pre-restore goruntuleriyle durumu cozumle)') ('code=restore_chain_open count=' + $script:ChainOpen)
        } else {
            $mleft = Remove-RsMarkers (Get-MMORestoreMarkerState -SavedFull $savedFull -RootFull $script:RootFull)
            if ($mleft -eq 0) { $script:MarkerWritten = $false } else { Write-RsLine '_WARN' 'code=restore_marker_left' }
        }
        Stop-Restore 1 ('write_failed_rolled_back:' + $failReason)
    }
    # Basari: bu kosunun kendi yardimci dosyalari (.restore.old, hash = onceki icerik dogrulanarak) silinir, is-isareti kalkar
    $oldLeft = 0
    foreach ($a1 in $attempted) {
        if ($null -ne $a1.Old) { if (-not (Remove-RsOwnFile -Path $a1.Old -ExpectSha $a1.PrevSha)) { $oldLeft++; Write-RsLine '_WARN' ('code=restore_old_left path=' + $a1.Rel + ' (onceki icerik yan dosyada kaldi; dogrulandiktan sonra elle silinebilir)') } }
    }
    if ($oldLeft -gt 0) { Add-RsNote 'restore_old_left' }
    # Zincir (O2): bayat isaret bilincli kabul edilmisse ve bu kosu eski kesintinin bulundugu durumu TAMAMEN kapsamiyorsa (-Slots ya da geri yuklenmeyen giris) isaret kalir.
    if ($script:ChainOpen -gt 0 -and ($useSlots -or $notRestored -gt 0)) {
        Write-RsLine '_WARN' ('code=restore_chain_open count=' + $script:ChainOpen + ' (bu kosu onceki yarim kalmis geri yuklemenin kapsadigi her seyi geri yuklemedi: is-isareti kaldirilmadi)') ('code=restore_chain_open count=' + $script:ChainOpen)
        Add-RsNote 'restore_chain_open'
    } else {
        $mleft = Remove-RsMarkers (Get-MMORestoreMarkerState -SavedFull $savedFull -RootFull $script:RootFull)
        if ($mleft -eq 0) { $script:MarkerWritten = $false } else { Write-RsLine '_WARN' 'code=restore_marker_left'; Add-RsNote 'restore_marker_left' }
    }
    Write-RsLine '_APPLIED' ('written=' + $attempted.Count + ' unchanged=' + $nSame + ' quarantined=' + $quarantined.Count + ' preimage=' + $script:PreName) ('written=' + $attempted.Count + ' unchanged=' + $nSame + ' quarantined=' + $quarantined.Count)
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
        $reason = 'unexpected:' + (Protect-RsText $msg)
        # D10: beklenmedik istisnada is-isareti kalmis olabilir: yarim kalan yapilari cokus kurtarma (O1) yolu kapsar; komut yazilir.
        if ($script:MarkerWritten) {
            Write-RsLine '_WARN' ('code=marker_left_after_unexpected is-isareti KALDI (yarim kalmis olabilir). Once kuru calisma: ' + (Get-RsRecoverCommand).Replace(' -Apply', '')) 'code=marker_left_after_unexpected'
            Write-RsRollbackWarnings
        }
    }
}
Close-RsLock
if ($exitCode -eq 0 -and $script:Notes.Count -gt 0) {
    $exitCode = 4
    $reason = ($script:Notes.ToArray() -join '+')
}
$runMode = 'dry-run'
if ($Apply) { $runMode = 'apply' }
if ($Recover) { $runMode = 'recover-' + $runMode }
$status = 'ok'
if ($exitCode -eq 4) { $status = 'ok_with_warnings' }
elseif ($exitCode -eq 3) { $status = 'deferred' }
elseif ($exitCode -eq 2) { $status = 'usage' }
elseif ($exitCode -ne 0) {
    $status = 'failed'
    if ($reason.StartsWith('write_failed_rolled_back', [StringComparison]::Ordinal)) { $status = 'rolled_back' }
    elseif ($reason.StartsWith('rollback_failed', [StringComparison]::Ordinal)) { $status = 'rollback_failed' }
    elseif ($reason.StartsWith('rollback_conflict', [StringComparison]::Ordinal)) { $status = 'rollback_conflict' }
}
$reasonCode = ($reason -split ':', 2)[0]
Write-Host ('[MMO_RESTORE] status=' + $status + ' reason=' + $reason + ' exit=' + $exitCode)
[void]$script:Log.Add('[MMO_RESTORE] status=' + $status + ' reason=' + $reasonCode + ' exit=' + $exitCode)
$resultText = 'mode=' + $runMode + ' exit=' + $exitCode + ' status=' + $status + ' reason=' + $reason + ' snapshot=' + (Protect-RsText $script:SnapName) + ' preimage=' + $script:PreName + ' process_check=' + $script:ProcessCheck + ' manifest_sha256=' + $script:ManifestSha
Write-Host ('[MMO_RESTORE_RESULT] ' + $resultText)
[void]$script:Log.Add('[MMO_RESTORE_RESULT] mode=' + $runMode + ' exit=' + $exitCode + ' status=' + $status + ' reason=' + $reasonCode + ' process_check=' + $script:ProcessCheck)
# Yalniz -Apply denemelerinde (exit 2 haric) dosya yazilir: restore.log (pre-restore goruntu dizinine) ve <BackupRoot>\last_restore.json. Kuru calisma HICBIR sey yazmaz.
if ($Apply -and $script:Attempted -and $exitCode -ne 2) {
    [void](Write-RsLogFile)
    $statusRoot = $null
    try { $statusRoot = Get-RsStatusRoot } catch { $statusRoot = $null }
    if ($null -eq $statusRoot) {
        Write-Host '[MMO_RESTORE_WARN] last_restore.json yazilmadi: yedek koku mevcut/guvenli degil'
    } else {
        $stamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture)
        $okRun = $false
        try {
            $okRun = Write-RsJsonAtomic -Path ($statusRoot + '\last_restore.json') -Object ([ordered]@{
                exit_code = $exitCode; status = $status; reason = $reasonCode; snapshot = (Protect-RsText $script:SnapName); preimage = $script:PreName
                manifest_sha256 = $script:ManifestSha; process_check = $script:ProcessCheck; flags = [string[]]@($script:Flags.ToArray())
                marker_left = $script:MarkerWritten; time_utc = $stamp; tool = 'RestoreSaves/2' })
        } catch { $okRun = $false }
        if (-not $okRun) { Write-Host '[MMO_RESTORE_WARN] last_restore.json yazilamadi' }
    }
}
exit $exitCode
