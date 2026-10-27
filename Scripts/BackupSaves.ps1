# Kayit dosyalari yedek ALMA (docs/DESIGN_BACKUP.md, dilim B1). Saved\{SaveGames,MMOAchievements,MMODaily,MMOMail,MMORace} altindaki
# oyuncu verisinin manifestli (SHA-256) anlik goruntusunu <BackupRoot>\<yyyyMMddTHHmmssZ>-<etiket>\ altina yazar.
# Kaynak ASLA degistirilmez (yalniz okunur); B1 hicbir dosya SILMEZ (yalniz kendi olusturdugu yarim/basarisiz cikti: bkz. asagi).
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI (pwsh bu makinede yok). Dosya yalniz ASCII.
#
# Cikis kodu: 0 basarili (tum dosyalar gecerli, manifest yazildi ve goruntu kendi kendini dogruladi);
#             1 hata / kararsiz / bozuk dosya var / kopya hatasi / cakisma / bos kaynak / goruntu zaten var / kendi dogrulamasi basarisiz;
#             2 kullanim hatasi (gecersiz etiket/zaman damgasi/yol, -AllowLive + -RequireCleanShutdown birlikte);
#             3 ertelendi (fail-closed, hicbir sey yazilmadi): UE/derleme sureci calisiyor ya da surec sorgusu basarisiz/bos,
#               -RequireCleanShutdown kosulu saglanmadi, yedek koku Saved icinde/Saved yedek kokunun icinde/repo icinde, reparse point,
#               kaynak/yedek koku eksik/erisilemiyor, BackupRoot kilidi baska surecte.
# Sira (hicbir sey yazmadan once): parametreler -> yol yapisi -> surec denetimi -> temiz kapanis kaniti -> kaynak kok -> kaynak listesi
# -> yedek koku -> kilit -> goruntu dizini -> dosyalar (.partial -> hash dogrula -> adlandir) -> manifest EN SON -> kendi dogrulamasi.
# Manifest yoksa goruntu YARIM sayilir (Test-MMOSnapshot Incomplete). Kendi dogrulamasi basarisizsa manifest.json -> manifest.json.failed
# (goruntu yarim olur; silinmez, delil). Canli (-AllowLive) goruntu "tutarli" DEGILDIR: mode=live + uyari (DESIGN_BACKUP R1, K3).
# Konsol/backup.log oyuncu verisi icermez (yalniz sayilar ve neden kodlari; yok sayilan dosya adlari yalniz konsola).
param(
    [string]$SavedDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved'),
    [string]$BackupRoot = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'MMOBackups'),
    [string]$Label = 'manual',
    [switch]$AllowLive,
    [switch]$RequireCleanShutdown,
    [switch]$AllowRootAncestorReparse,
    # Surec envanteri saglayicisi (test enjeksiyonu): Name/ProcessId/CommandLine uyeli nesneler dondurur. Bos = Win32_Process (CIM).
    [scriptblock]$ProcessProvider = $null,
    # Sabit zaman damgasi (otomasyon/test): yyyyMMddTHHmmssZ. Bos = simdiki UTC.
    [string]$Timestamp = '',
    # Test kancalari (yalniz test): DuringCopy = {param($rel,$attempt,$src)}, AfterPartial = {param($rel,$partialPath)}, AfterManifest = {param($snapshotDir)}
    [hashtable]$TestHooks = $null
)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:SnapDir = $null
$script:SnapOwned = $false
$script:Log = New-Object System.Collections.ArrayList

function Write-BkLine {
    param([string]$Tag, [string]$Text)
    $line = '[MMO_BACKUP' + $Tag + '] ' + $Text
    Write-Host $line
    [void]$script:Log.Add($line)
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

function Get-BkSources {
    # Yalniz beyaz listedeki bes dizinin DOGRUDAN altindaki dosyalar. Alt dizin / beyaz liste disi ad = yok sayilir (rapor).
    # Reparse (dizin ya da girdi) = 3 (fail-closed). Listeleme hatasi = 1.
    param([string]$SavedFull)
    $files = New-Object System.Collections.ArrayList
    $ignored = New-Object System.Collections.ArrayList
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
            if ($it.PSIsContainer) { [void]$ignored.Add($rel + '/'); continue }
            $c = Get-MMOSaveFileClass $rel
            if ($c.Class -eq 'ignored') { [void]$ignored.Add($rel); continue }
            [void]$files.Add($rel)
        }
    }
    $arr = [string[]]@($files.ToArray())
    [Array]::Sort($arr, [StringComparer]::Ordinal)
    return [pscustomobject]@{ Files = $arr; Ignored = [string[]]@($ignored.ToArray()) }
}

function Copy-BkFile {
    # Bir kaynak dosyayi tek okuma + .partial + hash dogrulama + adlandirma ile goruntuye kopyalar. Donus: kararsiz mi ($true/$false).
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
        if ($null -ne $readErr) { Stop-Backup 1 ('read_failed:' + $readErr) }
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
    return (-not $stable)
}

function Invoke-BackupMain {
    $mode = 'quiesced'
    if ($AllowLive) { $mode = 'live' }
    # 1. Parametreler (kullanim hatasi = 2)
    if ($AllowLive -and $RequireCleanShutdown) { Stop-Backup 2 'allowlive_and_requirecleanshutdown' }
    if ($Label -cnotmatch '^[a-z0-9-]{0,24}\z') { Stop-Backup 2 'bad_label' }
    $ts = $Timestamp
    if ($ts -eq '') { $ts = (Get-Date).ToUniversalTime().ToString("yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture) }
    if ($ts -cnotmatch '^[0-9]{8}T[0-9]{6}Z\z') { Stop-Backup 2 'bad_timestamp' }
    $name = $ts
    if ($Label -ne '') { $name = $ts + '-' + $Label }
    $savedFull = $null
    $rootFull = $null
    $pathErr = $false
    try {
        $savedFull = (ConvertTo-MMOFullPath $SavedDir).TrimEnd('\')
        $rootFull = (ConvertTo-MMOFullPath $BackupRoot).TrimEnd('\')
    } catch { $pathErr = $true }
    if ($pathErr) { Stop-Backup 2 'path_invalid' }
    # 2. Yol yapisi / ozyineleme korumasi (3: fail-closed, hicbir sey yazilmaz)
    if (Test-MMOPathUnder -Path $rootFull -Root $savedFull -AllowEqual) { Stop-Backup 3 'backup_root_inside_saved' }
    if (Test-MMOPathUnder -Path $savedFull -Root $rootFull -AllowEqual) { Stop-Backup 3 'saved_inside_backup_root' }
    if (Test-MMOPathUnder -Path $rootFull -Root $script:RepoRoot -AllowEqual) { Stop-Backup 3 'backup_root_inside_repo' }
    # 3. Canli sunucu/editor denetimi (varsayilan RED; surec sorgusu hata verirse ya da bos donerse de RED)
    if (-not $AllowLive) {
        $procs = $null
        $qerr = $false
        try {
            if ($null -ne $ProcessProvider) { $procs = @(& $ProcessProvider | Where-Object { $null -ne $_ }) }
            else { $procs = @(Get-CimInstance -ClassName Win32_Process -ErrorAction Stop | Where-Object { $null -ne $_ }) }
        } catch { $qerr = $true }
        if ($qerr) { Stop-Backup 3 'process_query_failed' }
        if ($procs.Count -eq 0) { Stop-Backup 3 'process_query_empty' }
        $blk = @(Get-MMOBlockingProcesses -Processes $procs)
        if ($blk.Count -gt 0) {
            foreach ($b in $blk) { Write-BkLine '_BLOCKER' ('process=' + $b.Name + ' pid=' + $b.ProcessId) }
            Stop-Backup 3 'process_running'
        }
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
    foreach ($ig in $src.Ignored) { Write-Host ('[MMO_BACKUP_IGNORED] ' + $ig) }
    if ($src.Files.Count -eq 0) { Stop-Backup 1 'no_files' }
    $coll = @(Find-MMOCollisions $src.Files)
    if ($coll.Count -gt 0) { Stop-Backup 1 'collision' }
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
        # 8. Dosyalar
        $unstable = New-Object System.Collections.ArrayList
        foreach ($rel in $src.Files) {
            if (Copy-BkFile -SavedFull $savedFull -FilesRoot $filesRoot -Rel $rel -Live ([bool]$AllowLive)) { [void]$unstable.Add($rel) }
        }
        # 9. Manifest EN SON
        $mErr = $null
        try {
            $m = New-MMOManifest -FilesRoot $filesRoot -SourceSavedDir $savedFull -Mode $mode -ShutdownProof $proof -UnstablePaths ([string[]]@($unstable.ToArray())) -AllowRootAncestorReparse:$AllowRootAncestorReparse
            Write-MMOManifest -Manifest $m -SnapshotDir $snap -AllowRootAncestorReparse:$AllowRootAncestorReparse
        } catch { $mErr = $_.Exception.Message }
        if ($null -ne $mErr) { Stop-Backup 1 ('manifest_failed:' + $mErr) }
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
        $msha = Get-MMOFileSha256 ($snap + '\manifest.json')
        $nInvalid = @($vt.InvalidEntries).Count
        Write-BkLine '' ('files=' + $src.Files.Count + ' valid=' + ($src.Files.Count - $nInvalid) + ' invalid=' + $nInvalid + ' unstable=' + $unstable.Count + ' ignored=' + $src.Ignored.Count)
        Write-BkLine '' ('mode=' + $mode + ' shutdown_proof=' + $(if ($null -eq $proof) { 'none' } else { 'recorded' }))
        Write-BkLine '' ('manifest_sha256=' + $msha)
        if ($nInvalid -gt 0) { Stop-Backup 1 'invalid_files' }
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
$status = 'ok'
if ($exitCode -eq 3) { $status = 'deferred' }
elseif ($exitCode -eq 2) { $status = 'usage' }
elseif ($exitCode -ne 0) {
    $status = 'failed'
    if ($script:SnapOwned -and $reason -ne 'invalid_files' -and -not [IO.File]::Exists($script:SnapDir + '\manifest.json')) { $status = 'incomplete' }
    if ($reason -eq 'invalid_files') { $status = 'complete_with_invalid_files' }
}
Write-BkLine '' ('status=' + $status + ' reason=' + $reason + ' exit=' + $exitCode)
if ($script:SnapOwned) {
    try {
        $logText = ($script:Log.ToArray() -join "`r`n") + "`r`n"
        [IO.File]::WriteAllBytes($script:SnapDir + '\backup.log', (New-Object System.Text.UTF8Encoding($false)).GetBytes($logText))
    } catch { Write-Host '[MMO_BACKUP_WARN] backup.log yazilamadi' }
}
exit $exitCode
