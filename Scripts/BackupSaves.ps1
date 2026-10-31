# Kayit dosyalari yedek ALMA (docs/DESIGN_BACKUP.md, dilim B1). Saved\{SaveGames,MMOAchievements,MMODaily,MMOMail,MMORace} altindaki
# oyuncu verisinin manifestli (SHA-256) anlik goruntusunu <BackupRoot>\<yyyyMMddTHHmmssZ>-<etiket>[-live]\ altina yazar.
# Kaynak ASLA degistirilmez (yalniz okunur); B1 hicbir dosya SILMEZ (yalniz kendi olusturdugu yarim/basarisiz cikti ve kendi .tmp'si: bkz. asagi).
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI (pwsh bu makinede yok). Dosya yalniz ASCII.
#
# CIKIS KODU SOZLESMESI (VerifySaves.ps1 ve TestBackup.ps1 ile ayni):
#   0  tamam: quiesced (UE/derleme sureci yok) + TUM dosyalar gecerli + yok sayilan oyuncu verisi yok (ya da -AllowIgnored) + temiz kanit bayat degil;
#      goruntu kendi kendini dogruladi. Yalniz bu kod "tam guvenilir yedek" demektir (last_ok.json yalniz burada yazilir).
#   1  yedek YOK ya da BASARISIZ: hata, kopya hatasi (copy_drift), quiesced_violated, bos kaynak, goruntu zaten var, kendi dogrulamasi basarisiz.
#      Manifest yoksa goruntu YARIM'dir (silinmez, delil); Verify "incomplete" der.
#   2  kullanim hatasi (gecersiz etiket/zaman damgasi/yol, -AllowLive + -RequireCleanShutdown, test parametresi test modu disinda).
#   3  ertelendi = HICBIR yedek ALINMADI (fail-closed): UE/derleme sureci calisiyor ya da surec sorgusu basarisiz/bos, -RequireCleanShutdown
#      kosulu saglanmadi, yedek koku Saved icinde/Saved yedek kokunun icinde/repo icinde (gercek yol dahil), reparse point, kaynak/yedek koku
#      eksik/erisilemiyor, BackupRoot kilidi baska surecte, bos disk alani < 1,5 x kaynak boyutu.
#   4  KULLANILABILIR yedek ALINDI ama tam guvenilir DEGIL; neden(ler) '+' ile birlesik, ornek reason=live+invalid_files:
#        live                  -AllowLive (dosyalar arasi tutarsiz olabilir; dizin adi -live ile biter)
#        invalid_files         valid=false giris var (bozuk kaynak, kararsiz, icerigi okunamayan/8 MB ustu: notes alanindan bakilir)
#        ignored_player_data   beyaz liste dizinlerinde oyuncu verisi OLABILECEK ama ad kuraliyla/beyaz listeyle dislanan dosya var (-AllowIgnored ile bilincli gecis)
#        collision             Windows'ta ayni slota dusen cakisan adlar (ikisi de yedekte; geri yukleme B3'te reddeder)
#        proof_stale           -RequireCleanShutdown kanitindan SONRA kaynak dosya degismis (kanit bayat, manifestte proof_stale=true)
#      Makine okunur: [MMO_BACKUP_RESULT] mode=<quiesced|live> exit= status= reason= snapshot= process_check=
# Her cikis yolunda (3 dahil, yedek koku guvenliyse) <BackupRoot>\last_run.json yazilir; last_ok.json yalniz kod 0'da (yazilamazsa uyari, yedek bozulmaz).
# Sira (hicbir sey yazmadan once): test kapisi -> parametreler -> yol yapisi (+gercek yol) -> surec denetimi -> kaynak kok -> temiz kapanis kaniti
# -> kaynak listesi -> bos alan -> yedek koku -> kilit -> goruntu dizini -> dosyalar (.partial -> hash dogrula -> adlandir) -> kopya sonrasi
# quiesced yeniden denetimi -> manifest EN SON (yazilan bayttan beklenen tabloyla karsilastirilarak) -> kendi dogrulamasi.
# Konsol ayrinti verir (dosya adlari dahil); backup.log YALNIZ neden kodu ve sayilar icerir (oyuncu adi / tam yol YOK).
# BUDAMA NOTU (B4): goruntu yasi KLASOR ADINDAN degil manifest created_utc'den okunmali; -Timestamp yalniz test modunda vardir ama klasor adi
# yine de elle yeniden adlandirilabilir.
param(
    [string]$SavedDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved'),
    [string]$BackupRoot = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'MMOBackups'),
    [string]$Label = 'manual',
    [switch]$AllowLive,
    [switch]$RequireCleanShutdown,
    [switch]$AllowRootAncestorReparse,
    # Oyuncu verisi olabilecek ama ad kuraliyla dislanan dosyalarin yedek disi kalmasini bilincli kabul et (aksi halde cikis 4: ignored_player_data).
    [switch]$AllowIgnored,
    # YALNIZ TEST (MMO_BACKUP_TESTMODE=1 gerekir, aksi halde kullanim hatasi 2). Surec envanteri saglayicisi: Name/ProcessId/CommandLine uyeli nesneler.
    [scriptblock]$ProcessProvider = $null,
    # YALNIZ TEST: sabit zaman damgasi yyyyMMddTHHmmssZ (gercek tarih olmali).
    [string]$Timestamp = '',
    # YALNIZ TEST: DuringCopy = {param($rel,$attempt,$src)}, AfterPartial = {param($rel,$partialPath)}, AfterCopy = {param($snapshotDir)},
    # BeforeManifest = {param($snapshotDir)}, AfterManifest = {param($snapshotDir)}, FreeBytes = {param($rootPath)} (bos alan enjeksiyonu)
    [hashtable]$TestHooks = $null
)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')

# Test parametreleri (-ProcessProvider/-Timestamp/-TestHooks) yalniz MMO_BACKUP_TESTMODE=1 iken etkin: uretim cagrisi bunlarla surec
# denetimini atlatamaz ya da klasor adini/zamanini uyduramaz (D1).
$script:TestMode = ($env:MMO_BACKUP_TESTMODE -ceq '1')
$script:TestParamUsed = $PSBoundParameters.ContainsKey('ProcessProvider') -or $PSBoundParameters.ContainsKey('Timestamp') -or $PSBoundParameters.ContainsKey('TestHooks')
$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:SnapDir = $null
$script:SnapOwned = $false
$script:SnapName = ''
$script:SavedFull = $null
$script:RootFull = $null
$script:ModeName = 'quiesced'
$script:ProcessCheck = 'none'
$script:ManifestSha = ''
$script:Notes = New-Object System.Collections.ArrayList
$script:Log = New-Object System.Collections.ArrayList

function Write-BkLine {
    param([string]$Tag, [string]$Text, [switch]$ConsoleOnly)
    $line = '[MMO_BACKUP' + $Tag + '] ' + $Text
    Write-Host $line
    if (-not $ConsoleOnly) { [void]$script:Log.Add($line) }
}

function Stop-Backup {
    param([int]$Code, [string]$Reason)
    throw ('MMOBK|' + $Code + '|' + $Reason)
}

function Test-BkDirectory {
    param([string]$Path)
    $a = Get-MMOAttributes $Path
    return (($a.State -eq 'ok') -and (($a.Attributes -band [IO.FileAttributes]::Directory) -ne 0))
}

function Get-BkRootProblem {
    # Yedek koku yapisi: Saved icinde/Saved yedek kokunun icinde/repo icinde mi. Once sozdizimsel, sonra GERCEK yol (junction/symlink/8.3 cozulmus;
    # D2). Cozulemeyen yol = ihlal (fail-closed). Gercek yol cozumu basarisizsa: -AllowRootAncestorReparse'te ihlal (reparse atasi kabul edilmis,
    # sozdizimsel denetim tek basina yetmez), aksi halde sozdizimsel sonuc (atalarda reparse zaten reddedilir). Donus: neden kodu ya da $null.
    param([string]$SavedFull, [string]$RootFull)
    if (Test-MMOPathUnder -Path $RootFull -Root $SavedFull -AllowEqual -ErrorIsViolation) { return 'backup_root_inside_saved' }
    if (Test-MMOPathUnder -Path $SavedFull -Root $RootFull -AllowEqual -ErrorIsViolation) { return 'saved_inside_backup_root' }
    if (Test-MMOPathUnder -Path $RootFull -Root $script:RepoRoot -AllowEqual -ErrorIsViolation) { return 'backup_root_inside_repo' }
    $fs = Get-MMOFinalPath $SavedFull
    $fr = Get-MMOFinalPath $RootFull
    $fp = Get-MMOFinalPath $script:RepoRoot
    if ($null -eq $fs -or $null -eq $fr -or $null -eq $fp) {
        if ($AllowRootAncestorReparse) { return 'final_path_unresolved' }
        return $null
    }
    if (Test-MMOPathUnder -Path $fr -Root $fs -AllowEqual -ErrorIsViolation) { return 'backup_root_inside_saved' }
    if (Test-MMOPathUnder -Path $fs -Root $fr -AllowEqual -ErrorIsViolation) { return 'saved_inside_backup_root' }
    if (Test-MMOPathUnder -Path $fr -Root $fp -AllowEqual -ErrorIsViolation) { return 'backup_root_inside_repo' }
    return $null
}

function Get-BkSources {
    # Yalniz beyaz listedeki bes dizinin DOGRUDAN altindaki dosyalar. Alt dizin / beyaz liste disi ad = yok sayilir (rapor; oyuncu verisi olabilecek
    # ad-kurali/ad-deseni disi dosyalar PlayerData=true ile ayri sayilir, O4). Reparse (dizin ya da girdi) = 3 (fail-closed). Listeleme hatasi = 1.
    # Stats: dosya basina 'boyut|LastWriteTime ticks' (taze FileInfo; dizin numaralandirmasindan DEGIL) - kopya sonrasi quiesced yeniden denetimi (O2).
    param([string]$SavedFull)
    $files = New-Object System.Collections.ArrayList
    $ignored = New-Object System.Collections.ArrayList
    $sizes = @{}
    $stats = @{}
    $maxTicks = 0L
    foreach ($d in 'SaveGames', 'MMOAchievements', 'MMODaily', 'MMOMail', 'MMORace') {
        $p = $SavedFull + '\' + $d
        $a = Get-MMOAttributes $p
        if ($a.State -eq 'missing') { continue }
        if ($a.State -ne 'ok') { Stop-Backup 3 'source_inaccessible' }
        if (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { Stop-Backup 3 'source_reparse' }
        if (($a.Attributes -band [IO.FileAttributes]::Directory) -eq 0) { Stop-Backup 3 'source_not_directory' }
        $items = $null
        try { $items = @(Get-ChildItem -LiteralPath $p -Force -ErrorAction Stop) } catch { $items = $null }
        if ($null -eq $items) { Stop-Backup 1 'tree_unreadable' }
        foreach ($it in $items) {
            if (Test-MMOReparse $it.FullName) { Stop-Backup 3 'source_reparse_entry' }
            $rel = $d + '/' + $it.Name
            if ($it.PSIsContainer) { [void]$ignored.Add([pscustomobject]@{ Rel = ($rel + '/'); Reason = 'subdir'; PlayerData = $false }); continue }
            $c = Get-MMOSaveFileClass $rel
            if ($c.Class -eq 'ignored') {
                $pd = (([string]$c.Reason -ceq 'name_rule') -or ([string]$c.Reason -ceq 'not_whitelisted_name'))
                [void]$ignored.Add([pscustomobject]@{ Rel = $rel; Reason = ([string]$c.Reason -split ':', 2)[0]; PlayerData = $pd })
                continue
            }
            [void]$files.Add($rel)
            $fi = New-Object IO.FileInfo($it.FullName)
            $sizes[$rel] = [long]$fi.Length
            $tk = $fi.LastWriteTimeUtc.Ticks
            $stats[$rel] = ([string]$fi.Length + '|' + [string]$tk)
            if ($tk -gt $maxTicks) { $maxTicks = $tk }
        }
    }
    $arr = [string[]]@($files.ToArray())
    [Array]::Sort($arr, [StringComparer]::Ordinal)
    return [pscustomobject]@{ Files = $arr; Ignored = @($ignored.ToArray()); Sizes = $sizes; Stats = $stats; MaxTicks = $maxTicks }
}

function Get-BkIgnoredSummary {
    # Yok sayilan dosyalarin ozeti: oyuncu verisi olabilecekler ayri sayilir; oyuncu adi / yol YOK (yalniz sayilar ve neden kodlari).
    param($Ignored)
    $pd = 0
    $other = 0
    $reasons = @{}
    foreach ($i in @($Ignored)) {
        if ($i.PlayerData) { $pd++ } else { $other++ }
        $k = [string]$i.Reason
        if (-not $reasons.ContainsKey($k)) { $reasons[$k] = 0 }
        $reasons[$k] = $reasons[$k] + 1
    }
    $keys = [string[]]@($reasons.Keys)
    [Array]::Sort($keys, [StringComparer]::Ordinal)
    $s = 'player_data=' + $pd + ';other=' + $other
    foreach ($k in $keys) { $s += ';' + $k + '=' + $reasons[$k] }
    return [pscustomobject]@{ Text = $s; PlayerData = $pd; Other = $other }
}

function Get-BkBlockers {
    # Surec denetimi (UE/derleme). Err: $null | process_query_failed | process_query_empty | process_running; Blockers: engelleyen surecler.
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

function Copy-BkFile {
    # Bir kaynak dosyayi tek okuma + .partial + hash dogrulama + adlandirma ile goruntuye kopyalar.
    # Donus: Skipped ($null | 'read_error' | 'too_large': icerik alinamadi, goruntuye dosya YAZILMADI, tum yedek iptal EDILMEZ - O5),
    # Unstable (canli yedekte kararsiz), Info (yazilan bayttan Size/Sha256/Valid/Notes: manifest beklenen tablosu, O1).
    param([string]$SavedFull, [string]$FilesRoot, [string]$Rel, [bool]$Live)
    $src = $SavedFull + '\' + $Rel.Replace('/', '\')
    $c = Get-MMOSaveFileClass $Rel
    if ($c.Class -eq 'ignored') { Stop-Backup 1 'copy_not_whitelisted' }
    $attempts = 1
    if ($Live) { $attempts = 3 }
    $info = $null
    $stable = $true
    for ($try = 1; $try -le $attempts; $try++) {
        $readErr = $null
        try { $info = Get-MMOFileSnapshotInfo -Path $src -ClassInfo $c } catch { $readErr = $_.Exception.Message }
        if ($null -ne $readErr) {
            $why = 'read_error'
            if ($readErr -ceq 'too_large') { $why = 'too_large' }
            return [pscustomobject]@{ Skipped = $why; Unstable = $false; Info = $null }
        }
        if ($null -ne $TestHooks -and $TestHooks.ContainsKey('DuringCopy')) { & $TestHooks['DuringCopy'] $Rel $try $src }
        if (-not $Live) { break }
        $same = $false
        try {
            $b2 = Read-MMOFileBytes $src
            $same = (([long]$b2.Length -eq $info.Size) -and ((Get-MMOBytesSha256 $b2) -ceq $info.Sha256))
        } catch { $same = $false }
        if ($same) { $stable = $true; break }
        $stable = $false
    }
    $dst = $FilesRoot + '\' + $Rel.Replace('/', '\')
    $dstDir = [IO.Path]::GetDirectoryName($dst)
    [void][IO.Directory]::CreateDirectory($dstDir)
    if (Test-MMOReparse $dstDir) { Stop-Backup 1 'snapshot_dir_reparse' }
    $part = $dst + '.partial'
    $fs = $null
    $werr = $null
    try {
        $fs = New-Object IO.FileStream($part, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $fs.Write($info.Bytes, 0, $info.Bytes.Length)
        $fs.Flush($true)
    } catch { $werr = $_.Exception.Message } finally { if ($null -ne $fs) { $fs.Dispose() } }
    if ($null -ne $werr) { Stop-Backup 1 'write_failed' }
    if ($null -ne $TestHooks -and $TestHooks.ContainsKey('AfterPartial')) { & $TestHooks['AfterPartial'] $Rel $part }
    $chkErr = $null
    $chk = $null
    try { $chk = Get-MMOFileSha256 $part } catch { $chkErr = $_.Exception.Message }
    if ($null -ne $chkErr -or $chk -cne $info.Sha256) { Stop-Backup 1 'partial_verify_failed' }
    $mvErr = $null
    try { [IO.File]::Move($part, $dst) } catch { $mvErr = $_.Exception.Message }
    if ($null -ne $mvErr) { Stop-Backup 1 'rename_failed' }
    $exp = @{ Size = [long]$info.Size; Sha256 = [string]$info.Sha256; Valid = [bool]$info.Valid; Notes = [string]$info.Notes }
    if (-not $stable) { $exp.Valid = $false; $exp.Notes = 'unstable' }
    return [pscustomobject]@{ Skipped = $null; Unstable = (-not $stable); Info = $exp }
}

function Write-BkJsonAtomic {
    # last_run.json / last_ok.json: kendi .tmp'si (CreateNew) -> Replace/Move. Hedef reparse ise yazilmaz. Basari = $true.
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

function Get-BkStatusRoot {
    # last_run.json icin yazilabilir GUVENLI yedek koku: yapi ihlali (Saved/repo icinde, gercek yol dahil), reparse, dizin degil, kok erisilemez = $null.
    # Kok yoksa ve ust dizini guvenliyse olusturulur (kod 3 ertelemelerinde de durum gorunsun diye).
    if ($null -eq $script:RootFull -or $null -eq $script:SavedFull) { return $null }
    if ($null -ne (Get-BkRootProblem -SavedFull $script:SavedFull -RootFull $script:RootFull)) { return $null }
    $root = $script:RootFull
    $a = Get-MMOAttributes $root
    if ($a.State -eq 'inaccessible') { return $null }
    if ($a.State -eq 'ok') {
        if (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { return $null }
        if (($a.Attributes -band [IO.FileAttributes]::Directory) -eq 0) { return $null }
        if (-not $AllowRootAncestorReparse -and $null -ne (Get-MMOReparseAncestor $root)) { return $null }
        return $root
    }
    $parent = [IO.Path]::GetDirectoryName($root)
    if ([string]::IsNullOrEmpty($parent) -or -not (Test-BkDirectory $parent)) { return $null }
    if (-not $AllowRootAncestorReparse -and $null -ne (Get-MMOReparseAncestor $root)) { return $null }
    try { [void][IO.Directory]::CreateDirectory($root) } catch { return $null }
    if (-not (Test-BkDirectory $root) -or (Test-MMOReparse $root)) { return $null }
    return $root
}

function Invoke-BackupMain {
    $mode = 'quiesced'
    if ($AllowLive) { $mode = 'live' }
    $script:ModeName = $mode
    # Yollar erken (yalniz last_run.json icin) sessizce mutlaklastirilir; gecersizlik asagida eski sirada kullanim hatasi (2) olur.
    $pathErr = $false
    try {
        $script:SavedFull = (ConvertTo-MMOFullPath $SavedDir).TrimEnd('\')
        $script:RootFull = (ConvertTo-MMOFullPath $BackupRoot).TrimEnd('\')
    } catch { $pathErr = $true; $script:SavedFull = $null; $script:RootFull = $null }
    # 0. Test parametreleri yalniz test modunda (D1)
    if ($script:TestParamUsed -and -not $script:TestMode) { Stop-Backup 2 'test_parameter_requires_testmode' }
    # 1. Parametreler (kullanim hatasi = 2)
    if ($AllowLive -and $RequireCleanShutdown) { Stop-Backup 2 'allowlive_and_requirecleanshutdown' }
    if ($Label -cnotmatch '^[a-z0-9-]{0,24}\z') { Stop-Backup 2 'bad_label' }
    $ts = $Timestamp
    if ($ts -eq '') { $ts = (Get-Date).ToUniversalTime().ToString("yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture) }
    if ($ts -cnotmatch '^[0-9]{8}T[0-9]{6}Z\z') { Stop-Backup 2 'bad_timestamp' }
    $realDate = [datetime]::MinValue
    if (-not [datetime]::TryParseExact($ts, "yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None, [ref]$realDate)) { Stop-Backup 2 'bad_timestamp' }
    $name = $ts
    if ($Label -ne '') { $name = $name + '-' + $Label }
    if ($AllowLive) { $name = $name + '-live' }
    $script:SnapName = $name
    if ($pathErr) { Stop-Backup 2 'path_invalid' }
    $savedFull = $script:SavedFull
    $rootFull = $script:RootFull
    # 2. Yol yapisi / ozyineleme korumasi (3: fail-closed, hicbir sey yazilmaz)
    $rp = Get-BkRootProblem -SavedFull $savedFull -RootFull $rootFull
    if ($null -ne $rp) { Stop-Backup 3 $rp }
    # 3. Canli sunucu/editor denetimi (varsayilan RED; surec sorgusu hata verirse ya da bos donerse de RED)
    if (-not $AllowLive) {
        $script:ProcessCheck = 'cim'
        if ($null -ne $ProcessProvider) { $script:ProcessCheck = 'injected' }
        $pc = Get-BkBlockers
        if ($null -ne $pc.Err) {
            foreach ($b in $pc.Blockers) { Write-BkLine '_BLOCKER' ('process=' + $b.Name + ' pid=' + $b.ProcessId) -ConsoleOnly }
            Stop-Backup 3 $pc.Err
        }
    } else {
        $script:ProcessCheck = 'skipped'
    }
    # 4. Kaynak kok (var, erisilebilir, reparse degil)
    $srcErr = $null
    try { [void](Assert-MMORootSafe -Root $savedFull -AllowRootAncestorReparse:$AllowRootAncestorReparse) } catch { $srcErr = $_.Exception.Message }
    if ($null -ne $srcErr) { Stop-Backup 3 ('saved_' + $srcErr) }
    # 5. Temiz kapanis kaniti (yalniz -RequireCleanShutdown'da okunur; dosya yazilmaz/silinmez) + kaynak listesi
    $proof = $null
    if ($RequireCleanShutdown) {
        $sr = Read-MMOShutdownResult -SavedDir $savedFull
        if (-not $sr.Ok) { Stop-Backup 3 $sr.Reason }
        $proof = $sr.Proof
    }
    $src = Get-BkSources -SavedFull $savedFull
    foreach ($ig in $src.Ignored) { Write-Host ('[MMO_BACKUP_IGNORED] ' + $ig.Rel) }
    if ($src.Files.Count -eq 0) { Stop-Backup 1 'no_files' }
    $ign = Get-BkIgnoredSummary -Ignored $src.Ignored
    $coll = @(Find-MMOCollisions $src.Files)
    # 5b. Bos alan (D7): kaynak toplam boyutunun 1,5 kati yoksa yazmadan ertele
    $total = 0L
    foreach ($k in $src.Sizes.Keys) { $total += [long]$src.Sizes[$k] }
    $need = [long][math]::Ceiling([double]$total * 1.5)
    $free = -1L
    if ($null -ne $TestHooks -and $TestHooks.ContainsKey('FreeBytes')) { $free = [long](& $TestHooks['FreeBytes'] $rootFull) }
    else { $free = Get-MMOFreeBytes $rootFull }
    if ($free -lt 0) { Stop-Backup 3 'disk_space_unknown' }
    if ($free -lt $need) { Stop-Backup 3 'disk_space_low' }
    # 6. Yedek koku (yoksa yalniz kendisi olusturulur; ust dizini var olmali), kilit
    $ra = Get-MMOAttributes $rootFull
    if ($ra.State -eq 'inaccessible') { Stop-Backup 3 'backup_root_inaccessible' }
    if ($ra.State -eq 'missing') {
        $parent = [IO.Path]::GetDirectoryName($rootFull)
        if ([string]::IsNullOrEmpty($parent) -or -not (Test-BkDirectory $parent)) { Stop-Backup 3 'backup_root_parent_missing' }
        if (-not $AllowRootAncestorReparse) {
            if ($null -ne (Get-MMOReparseAncestor $rootFull)) { Stop-Backup 3 'backup_root_reparse_ancestor' }
        }
        $mkErr = $null
        try { [void][IO.Directory]::CreateDirectory($rootFull) } catch { $mkErr = $_.Exception.Message }
        if ($null -ne $mkErr) { Stop-Backup 3 'backup_root_unwritable' }
    }
    $rootErr = $null
    try { [void](Assert-MMORootSafe -Root $rootFull -AllowRootAncestorReparse:$AllowRootAncestorReparse) } catch { $rootErr = $_.Exception.Message }
    if ($null -ne $rootErr) { Stop-Backup 3 ('backup_root_' + $rootErr) }
    if (-not (Test-BkDirectory $rootFull)) { Stop-Backup 3 'backup_root_not_directory' }
    $lockPath = $rootFull + '\.lock'
    if (Test-MMOReparse $lockPath) { Stop-Backup 3 'lock_reparse' }
    $lockFs = $null
    $lockErr = $null
    try { $lockFs = New-Object IO.FileStream($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None, 4096, [IO.FileOptions]::DeleteOnClose) } catch { $lockErr = $_.Exception.Message }
    if ($null -ne $lockErr) { Stop-Backup 3 'lock_held' }
    try {
        # 7. Goruntu dizini (var olana ASLA yazilmaz)
        $snap = $rootFull + '\' + $name
        if ((Get-MMOAttributes $snap).State -ne 'missing') { Stop-Backup 1 'snapshot_exists' }
        [void][IO.Directory]::CreateDirectory($snap)
        $script:SnapDir = $snap
        $script:SnapOwned = $true
        $filesRoot = $snap + '\files'
        [void][IO.Directory]::CreateDirectory($filesRoot)
        if ($AllowLive) { Write-BkLine '_WARN' 'mode=live: canli goruntu tutarli DEGILDIR (sunucu/editor calisirken alindi); kritik yedek icin sunucuyu kapatip -RequireCleanShutdown kullan' }
        if ($ign.PlayerData -gt 0) {
            Write-BkLine '_WARN' ('OYUNCU VERISI OLABILIR: beyaz liste dizinlerinde ad kurali/desenine uymadigi icin yedek DISI kalan dosya sayisi=' + $ign.PlayerData + ' (adlar yalniz konsolda)')
        }
        if ($coll.Count -gt 0) { Write-BkLine '_WARN' ('cakisan ad grubu sayisi=' + $coll.Count + ' (ikisi de yedekte; geri yukleme reddedecek)') }
        # 8. Dosyalar
        $unstable = New-Object System.Collections.ArrayList
        $expected = @{}
        $skipped = [ordered]@{}
        foreach ($rel in $src.Files) {
            $cr = Copy-BkFile -SavedFull $savedFull -FilesRoot $filesRoot -Rel $rel -Live ([bool]$AllowLive)
            if ($null -ne $cr.Skipped) { $skipped[$rel] = $cr.Skipped; continue }
            $expected[$rel] = $cr.Info
            if ($cr.Unstable) { [void]$unstable.Add($rel) }
        }
        if ($null -ne $TestHooks -and $TestHooks.ContainsKey('AfterCopy')) { & $TestHooks['AfterCopy'] $snap }
        # 8b. Quiesced iddiasinin kopya SONRASI yeniden denetimi (O2): surec geri geldi ya da kaynak kopya sirasinda degistiyse goruntu guvenilmez.
        # Manifest yazilmaz (goruntu yarim kalir, silinmez); -AllowLive'da kaynagin degismesi beklenir (kararsiz dosyalar zaten isaretlenir).
        if (-not $AllowLive) {
            $cause = $null
            $pc2 = Get-BkBlockers
            if ($null -ne $pc2.Err) { $cause = $pc2.Err }
            else {
                $src2 = $null
                try { $src2 = Get-BkSources -SavedFull $savedFull } catch { $src2 = $null }
                if ($null -eq $src2) { $cause = 'source_relist_failed' }
                elseif (($src2.Files -join '|') -cne ($src.Files -join '|')) { $cause = 'source_file_set_changed' }
                else {
                    foreach ($rel in $src.Files) { if ($src2.Stats[$rel] -cne $src.Stats[$rel]) { $cause = 'source_changed'; break } }
                }
            }
            if ($null -ne $cause) {
                Write-BkLine '_DETAIL' ('quiesced_violated cause=' + $cause) -ConsoleOnly
                Stop-Backup 1 'quiesced_violated'
            }
        }
        # Kanit bayat mi (D6): kapanis kanitindan SONRA degisen kaynak dosya var mi (saniye cozunurlugu; alt-saniye farki yakalanmaz)
        $proofStale = $null
        if ($null -ne $proof) {
            $shut = [datetime]::ParseExact([string](Get-MMOMember $proof 'time_utc'), "yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal)
            $shutSec = [long](($shut.Ticks - ($shut.Ticks % 10000000L)) / 10000000L)
            $newSec = [long](([long]$src.MaxTicks - ([long]$src.MaxTicks % 10000000L)) / 10000000L)
            $proofStale = ($newSec -gt $shutSec)
        }
        # 9. Manifest EN SON
        if ($null -ne $TestHooks -and $TestHooks.ContainsKey('BeforeManifest')) { & $TestHooks['BeforeManifest'] $snap }
        $mErr = $null
        try {
            $m = New-MMOManifest -FilesRoot $filesRoot -SourceSavedDir $savedFull -Mode $mode -ShutdownProof $proof -UnstablePaths ([string[]]@($unstable.ToArray())) `
                -ExpectedEntries $expected -SkippedEntries $skipped -ProofStale $proofStale -IgnoredCount $src.Ignored.Count -IgnoredSummary $ign.Text `
                -ProcessCheck $script:ProcessCheck -AllowCollisions -AllowRootAncestorReparse:$AllowRootAncestorReparse
            Write-MMOManifest -Manifest $m -SnapshotDir $snap -AllowRootAncestorReparse:$AllowRootAncestorReparse
        } catch { $mErr = $_.Exception.Message }
        if ($null -ne $mErr) {
            if ($mErr.StartsWith('copy_drift:', [StringComparison]::Ordinal)) { Stop-Backup 1 $mErr }
            Stop-Backup 1 ('manifest_failed:' + $mErr)
        }
        if ($null -ne $TestHooks -and $TestHooks.ContainsKey('AfterManifest')) { & $TestHooks['AfterManifest'] $snap }
        # 10. Kendi dogrulamasi; basarisizsa manifest kaldirilir (goruntu yarim olur, delil icin .failed olarak kalir)
        $vt = Test-MMOSnapshot -SnapshotDir $snap -AllowRootAncestorReparse:$AllowRootAncestorReparse
        if (-not $vt.Ok) {
            $mp = $snap + '\manifest.json'
            $moved = $false
            try { [IO.File]::Move($mp, $mp + '.failed'); $moved = $true } catch { $moved = $false }
            if (-not $moved) {
                try { [IO.File]::Delete($mp) } catch { }
                if ([IO.File]::Exists($mp)) { Stop-Backup 1 'verify_failed_manifest_stuck' }
            }
            Stop-Backup 1 'verify_failed'
        }
        $script:ManifestSha = Get-MMOFileSha256 ($snap + '\manifest.json')
        $nInvalid = @($vt.InvalidEntries).Count
        Write-BkLine '' ('files=' + $src.Files.Count + ' valid=' + ($src.Files.Count - $nInvalid) + ' invalid=' + $nInvalid + ' unstable=' + $unstable.Count + ' unreadable=' + $skipped.Count + ' ignored=' + $src.Ignored.Count + ' ignored_player_data=' + $ign.PlayerData + ' collisions=' + $coll.Count)
        Write-BkLine '' ('mode=' + $mode + ' shutdown_proof=' + $(if ($null -eq $proof) { 'none' } else { 'recorded' }) + ' proof_stale=' + $(if ($null -eq $proofStale) { 'n/a' } else { ([string]$proofStale).ToLowerInvariant() }) + ' process_check=' + $script:ProcessCheck)
        Write-BkLine '' ('manifest_sha256=' + $script:ManifestSha)
        # Kullanilabilir ama tam guvenilir olmayan yedek nedenleri (cikis 4)
        if ($AllowLive) { [void]$script:Notes.Add('live') }
        if ($nInvalid -gt 0) { [void]$script:Notes.Add('invalid_files') }
        if ($ign.PlayerData -gt 0 -and -not $AllowIgnored) { [void]$script:Notes.Add('ignored_player_data') }
        if ($coll.Count -gt 0) { [void]$script:Notes.Add('collision') }
        if ($proofStale -eq $true) { [void]$script:Notes.Add('proof_stale') }
    } finally {
        if ($null -ne $lockFs) { $lockFs.Dispose() }
    }
}

$exitCode = 1
$reason = 'unexpected'
try {
    Invoke-BackupMain
    $exitCode = 0
    $reason = 'ok'
} catch {
    $msg = $_.Exception.Message
    if ($msg -cmatch '^MMOBK\|(?<c>[0-9]+)\|(?<r>.*)\z') {
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
$status = 'ok'
if ($exitCode -eq 4) {
    if ($script:Notes.Contains('invalid_files')) { $status = 'complete_with_invalid_files' }
    elseif ($script:Notes.Contains('live')) { $status = 'complete_live' }
    else { $status = 'complete_with_warnings' }
}
elseif ($exitCode -eq 3) { $status = 'deferred' }
elseif ($exitCode -eq 2) { $status = 'usage' }
elseif ($exitCode -ne 0) {
    $status = 'failed'
    if ($script:SnapOwned -and -not [IO.File]::Exists($script:SnapDir + '\manifest.json')) { $status = 'incomplete' }
}
# Konsol tam neden (ayrinti: oyuncu dosya adi olabilir); backup.log yalniz neden KODU (':' sonrasi atilir)
$reasonCode = ($reason -split ':', 2)[0]
$snapForResult = 'none'
if ($script:SnapOwned) { $snapForResult = $script:SnapName }
Write-Host ('[MMO_BACKUP] status=' + $status + ' reason=' + $reason + ' exit=' + $exitCode)
[void]$script:Log.Add('[MMO_BACKUP] status=' + $status + ' reason=' + $reasonCode + ' exit=' + $exitCode)
Write-Host ('[MMO_BACKUP_RESULT] mode=' + $script:ModeName + ' exit=' + $exitCode + ' status=' + $status + ' reason=' + $reason + ' snapshot=' + $snapForResult + ' process_check=' + $script:ProcessCheck)
[void]$script:Log.Add('[MMO_BACKUP_RESULT] mode=' + $script:ModeName + ' exit=' + $exitCode + ' status=' + $status + ' reason=' + $reasonCode + ' snapshot=' + $snapForResult + ' process_check=' + $script:ProcessCheck)
# last_run.json (her cikis yolunda) / last_ok.json (yalniz kod 0): yazilamazsa yalniz uyari; yedegi ve cikis kodunu etkilemez
$stamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture)
$statusRoot = $null
try { $statusRoot = Get-BkStatusRoot } catch { $statusRoot = $null }
if ($null -eq $statusRoot) {
    Write-BkLine '_WARN' 'last_run.json yazilmadi: yedek koku guvenli/yazilabilir degil'
} else {
    $okRun = $false
    try {
        $okRun = Write-BkJsonAtomic -Path ($statusRoot + '\last_run.json') -Object ([ordered]@{
            exit_code = $exitCode; status = $status; reason = $reasonCode; mode = $script:ModeName; snapshot = $snapForResult
            process_check = $script:ProcessCheck; time_utc = $stamp; tool = 'BackupSaves/2' })
    } catch { $okRun = $false }
    if (-not $okRun) { Write-BkLine '_WARN' 'last_run.json yazilamadi' }
    if ($exitCode -eq 0 -and $script:ModeName -ceq 'quiesced') {
        $okOk = $false
        try {
            $okOk = Write-BkJsonAtomic -Path ($statusRoot + '\last_ok.json') -Object ([ordered]@{
                snapshot = $script:SnapName; manifest_sha256 = $script:ManifestSha; mode = $script:ModeName; time_utc = $stamp; tool = 'BackupSaves/2' })
        } catch { $okOk = $false }
        if (-not $okOk) { Write-BkLine '_WARN' 'last_ok.json yazilamadi' }
    }
}
if ($script:SnapOwned) {
    try {
        $logText = ($script:Log.ToArray() -join "`r`n") + "`r`n"
        [IO.File]::WriteAllBytes($script:SnapDir + '\backup.log', (New-Object System.Text.UTF8Encoding($false)).GetBytes($logText))
    } catch { Write-Host '[MMO_BACKUP_WARN] backup.log yazilamadi' }
}
exit $exitCode
