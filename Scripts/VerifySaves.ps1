# Kayit dosyalari yedek DOGRULAMA (docs/DESIGN_BACKUP.md, dilim B2). Bir anlik goruntu dizinini (-Snapshot) ya da yedek kokundeki
# tum / son N goruntuyu (-BackupRoot + -All | -Last N) SALT OKUNUR dogrular. HICBIR SEY yazmaz, silmez, tasimaz (log dosyasi dahil:
# cikti yalniz konsoldur; kilit dosyasi da yok). Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII.
#
# Kontroller (Test-MMOSnapshot): manifest sozlesmesi + entries_digest/header_digest, her dosyanin boyutu + SHA-256'si, valid/notes'un icerikten
# yeniden hesabi, manifestte olmayan fazladan dosya, reparse point (fail-closed), cakisan adlar, boyut siniri. Ek olarak:
#  - manifest.json dosyasinin SHA-256 ozeti yazilir; dogrulama sirasinda degisirse hata. -ExpectedManifestSha256 ile DISARIDAN bilinen ozet
#    karsilastirilir (uyusmazlik = red; yalniz tek goruntu -Snapshot ile; BackupSaves.ps1 / backup.log'un yazdigi manifest_sha256).
#  - YARIM goruntu (manifest.json yok / manifest.json.failed / .partial kalintisi) acikca 'incomplete' raporlanir.
#  - mode=live ve shutdown_proof durumu raporlanir; valid=false girisler listelenir (B3 bunlari geri yuklemeyecek).
#  - .bad/.tmp/.bak yan dosyalari ve yetim yan dosyalar UYARI olarak listelenir.
#  - -Against <Saved dizini>: goruntu ile canli agacin farki (yalniz ad/boyut/hash; canli dosyalara YAZILMAZ, oyunun Load kurtarma
#    yollari cagrilmaz: dosyalar bayt olarak okunur).
#
# Cikis kodu (BackupSaves.ps1 sozlesmesiyle uyumlu): 0 tamam (tum goruntuler ok, ya da yalniz uyari/live);
#   1 dogrulama basarisiz (bozuk/yarim goruntu, valid=false giris, ozet uyusmazligi, -Against okunamadi, hic goruntu yok);
#   2 kullanim hatasi; 3 ertelendi/erisilemiyor (goruntu ya da yedek koku ya da -Against Saved'i yok/erisilemiyor/reparse).
# Makine okunur satirlar: [MMO_VERIFY_SUMMARY] (ilk satir) sonra [MMO_VERIFY_SNAPSHOT|ERROR|INVALID|WARN|INCOMPLETE|PROOF|AGAINST|DIFF].
# Konsol oyuncu adlari (dosya adlari) icerir: yalniz yerel konsola, log dosyasi yok.
param(
    [string]$Snapshot = '',
    [string]$BackupRoot = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'MMOBackups'),
    [int]$Last = 0,
    [switch]$All,
    [string]$ExpectedManifestSha256 = '',
    [string]$Against = '',
    [switch]$AllowRootAncestorReparse,
    # Test kancasi (yalniz test): AfterVerify = {param($snapshotDir)}; manifest dogrulama sirasinda degisirse hata gate'ini sinamak icin.
    [hashtable]$TestHooks = $null
)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')

$script:Detail = New-Object System.Collections.ArrayList
$script:Counts = [ordered]@{ snapshots = 0; ok = 0; bad = 0; incomplete = 0; with_invalid = 0; invalid_entries = 0; live = 0; warnings = 0; foreign = 0 }
$script:AgainstFailed = $false

function Stop-Verify {
    param([int]$Code, [string]$Reason)
    throw ('MMOVF|' + $Code + '|' + $Reason)
}

function Add-VfLine {
    param([string]$Tag, [string]$Text)
    [void]$script:Detail.Add('[MMO_VERIFY' + $Tag + '] ' + $Text)
}

function Get-VfManifestSha {
    # manifest.json dosyasinin SHA-256'si; yok / reparse / okunamiyor = 'none'.
    param([string]$Dir)
    $p = $Dir + '\manifest.json'
    if (-not [IO.File]::Exists($p)) { return 'none' }
    if (Test-MMOReparse $p) { return 'none' }
    try { return (Get-MMOFileSha256 $p) } catch { return 'none' }
}

function Get-VfSavedListing {
    # -Against: Saved'in beyaz listedeki bes dizininin DOGRUDAN altindaki dosyalar (BackupSaves ile ayni kume). Reparse = 3, listeleme hatasi = 1.
    param([string]$SavedFull)
    $files = New-Object System.Collections.ArrayList
    foreach ($d in 'SaveGames', 'MMOAchievements', 'MMODaily', 'MMOMail', 'MMORace') {
        $p = $SavedFull + '\' + $d
        $a = Get-MMOAttributes $p
        if ($a.State -eq 'missing') { continue }
        if ($a.State -ne 'ok') { Stop-Verify 3 'against_inaccessible' }
        if (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { Stop-Verify 3 'against_reparse' }
        if (($a.Attributes -band [IO.FileAttributes]::Directory) -eq 0) { Stop-Verify 3 'against_not_directory' }
        $items = $null
        try { $items = @(Get-ChildItem -LiteralPath $p -Force -ErrorAction Stop) } catch { $items = $null }
        if ($null -eq $items) { Stop-Verify 1 'against_tree_unreadable' }
        foreach ($it in $items) {
            if (Test-MMOReparse $it.FullName) { Stop-Verify 3 'against_reparse_entry' }
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

function Get-VfLiveInfo {
    # Canli dosyayi TEK KEZ bayt olarak okur (yazmaz). Donus: Ok, Size, Sha256, Error.
    param([string]$SavedFull, [string]$Rel)
    $path = $SavedFull + '\' + $Rel.Replace('/', '\')
    $bytes = $null
    $err = $null
    try { $bytes = Read-MMOFileBytes $path } catch { $err = $_.Exception.Message }
    if ($null -ne $err) { return [pscustomobject]@{ Ok = $false; Size = -1L; Sha256 = ''; Error = $err } }
    return [pscustomobject]@{ Ok = $true; Size = [long]$bytes.Length; Sha256 = (Get-MMOBytesSha256 $bytes); Error = '' }
}

function Test-VfOneSnapshot {
    param([string]$Dir, [string]$Expected, $LiveCache, [string]$SavedFull, [string[]]$LiveFiles)
    $name = [IO.Path]::GetFileName($Dir)
    $errors = New-Object System.Collections.ArrayList
    $warns = New-Object System.Collections.ArrayList
    $reasons = New-Object System.Collections.ArrayList
    $isDir = $false
    $a = Get-MMOAttributes $Dir
    if ($a.State -eq 'ok' -and (($a.Attributes -band [IO.FileAttributes]::Directory) -ne 0)) { $isDir = $true }
    $shaBefore = 'none'
    $rm = $null
    $t = $null
    $shaAfter = 'none'
    $listing = @()
    if (-not $isDir) {
        [void]$errors.Add('not_a_directory')
    } else {
        $shaBefore = Get-VfManifestSha $Dir
        $rm = Read-MMOManifest -SnapshotDir $Dir -AllowRootAncestorReparse:$AllowRootAncestorReparse
        $t = Test-MMOSnapshot -SnapshotDir $Dir -AllowRootAncestorReparse:$AllowRootAncestorReparse
        if ($null -ne $TestHooks -and $TestHooks.ContainsKey('AfterVerify')) { & $TestHooks['AfterVerify'] $Dir }
        $shaAfter = Get-VfManifestSha $Dir
        if ($shaBefore -cne $shaAfter) { [void]$errors.Add('manifest_changed_during_verify') }
        foreach ($e in @($t.Errors)) { [void]$errors.Add([string]$e) }
        # Yarim goruntu kalintilari (tum dizin; reparse / okunamayan agac = hata)
        $partials = 0
        try { $listing = Get-MMOTreeFiles -Root $Dir -AllowRootAncestorReparse:$AllowRootAncestorReparse } catch {
            [void]$errors.Add('tree_unusable:' + $_.Exception.Message)
            $listing = @()
        }
        $hasFailed = $false
        $hasTmp = $false
        foreach ($f in $listing) {
            if ($f.EndsWith('.partial', [StringComparison]::Ordinal)) { $partials++ }
            elseif ($f -ceq 'manifest.json.failed') { $hasFailed = $true }
            elseif ($f -ceq 'manifest.json.tmp') { $hasTmp = $true }
        }
        if ($t.Incomplete) {
            [void]$reasons.Add('manifest_missing')
            if ($hasFailed) { [void]$reasons.Add('manifest_failed') }
            if ($hasTmp) { [void]$reasons.Add('manifest_tmp') }
            if ($partials -gt 0) { [void]$reasons.Add('partial_files=' + $partials) }
        } else {
            if ($partials -gt 0) { [void]$errors.Add('partial_leftover:' + $partials) }
            if ($hasFailed) { [void]$errors.Add('manifest_failed_leftover') }
            if ($hasTmp) { [void]$errors.Add('manifest_tmp_leftover') }
        }
    }
    if ($Expected -ne '') {
        if ($shaBefore -cne $Expected) { [void]$errors.Add('expected_digest_mismatch:expected=' + $Expected + ':actual=' + $shaBefore) }
    }
    $invalid = @()
    if ($null -ne $t) { $invalid = @($t.InvalidEntries) }
    $mode = 'unknown'
    $proofState = 'unknown'
    $proofLine = $null
    $entryCount = 0
    $manifestOk = ($null -ne $rm) -and $rm.Ok
    if ($manifestOk) {
        $mf = $rm.Manifest
        $mode = [string]$mf.mode
        $entryCount = @($mf.entries).Count
        $pr = Get-MMOMember $mf 'shutdown_proof'
        if ($null -eq $pr) { $proofState = 'none' }
        else {
            $proofState = 'recorded'
            $proofLine = 'run_id=' + [string](Get-MMOMember $pr 'run_id') + ' exit_code=' + [string](Get-MMOMember $pr 'exit_code') + ' time_utc=' + (ConvertTo-MMOUtcString (Get-MMOMember $pr 'time_utc'))
        }
        if ($mode -ceq 'live') { [void]$warns.Add('mode=live: canli goruntu tutarli DEGILDIR (sunucu/editor calisirken alindi)') }
        foreach ($e in @($mf.entries)) {
            $c = Get-MMOSaveFileClass ([string]$e.path)
            if ($c.Class -eq 'ignored') { continue }
            if ($c.Class -eq 'side-lock') { [void]$warns.Add('lock_file=' + $e.path) }
            elseif ($c.Suffix -ne '') { [void]$warns.Add('side_file=' + $e.path) }
        }
        $groups = Get-MMOSlotGroups ([string[]]@($mf.entries | ForEach-Object { [string]$_.path }))
        foreach ($k in $groups.Keys) {
            $has = $false
            foreach ($it in $groups[$k].Items) { if ($it.Class -eq 'character') { $has = $true } }
            if (-not $has) { [void]$warns.Add('orphan_group=' + $k + ' (yan dosya var, karakter .sav yok)') }
        }
    }
    $status = 'ok'
    if ($t -ne $null -and $t.Incomplete) { $status = 'incomplete' }
    elseif ($errors.Count -gt 0) { $status = 'bad' }
    elseif ($invalid.Count -gt 0) { $status = 'complete_with_invalid_files' }
    $script:Counts['snapshots']++
    if ($status -eq 'ok') { $script:Counts['ok']++ }
    elseif ($status -eq 'bad') { $script:Counts['bad']++ }
    elseif ($status -eq 'incomplete') { $script:Counts['incomplete']++ }
    else { $script:Counts['with_invalid']++ }
    $script:Counts['invalid_entries'] += $invalid.Count
    if ($mode -ceq 'live') { $script:Counts['live']++ }
    $script:Counts['warnings'] += $warns.Count
    Add-VfLine '_SNAPSHOT' ('name=' + $name + ' status=' + $status + ' manifest_sha256=' + $shaBefore + ' mode=' + $mode + ' shutdown_proof=' + $proofState + ' entries=' + $entryCount + ' errors=' + $errors.Count + ' invalid=' + $invalid.Count)
    if ($null -ne $proofLine) { Add-VfLine '_PROOF' ('snap=' + $name + ' ' + $proofLine) }
    if ($status -eq 'incomplete') { Add-VfLine '_INCOMPLETE' ('snap=' + $name + ' reasons=' + ($reasons.ToArray() -join ',')) }
    foreach ($e in $errors) { Add-VfLine '_ERROR' ('snap=' + $name + ' ' + $e) }
    foreach ($i in $invalid) { Add-VfLine '_INVALID' ('snap=' + $name + ' path=' + $i + ' (geri yuklenmeyecek)') }
    foreach ($w in $warns) { Add-VfLine '_WARN' ('snap=' + $name + ' ' + $w) }
    # -Against: goruntu ile canli agac farki (yalniz tutarli manifest varsa)
    if ($SavedFull -ne '') {
        if (-not $manifestOk) {
            Add-VfLine '_AGAINST' ('snap=' + $name + ' skipped=manifest_not_usable')
        } else {
            $same = 0; $changed = 0; $onlySnap = 0; $onlySaved = 0
            $inSnap = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
            foreach ($e in @($rm.Manifest.entries)) {
                $rel = [string]$e.path
                [void]$inSnap.Add($rel)
                if (-not $LiveCache.ContainsKey($rel)) {
                    if (([array]::IndexOf($LiveFiles, $rel)) -lt 0) {
                        $onlySnap++
                        Add-VfLine '_DIFF' ('snap=' + $name + ' kind=only_in_snapshot path=' + $rel)
                        continue
                    }
                    $LiveCache[$rel] = Get-VfLiveInfo -SavedFull $SavedFull -Rel $rel
                }
                $li = $LiveCache[$rel]
                if (-not $li.Ok) {
                    $script:AgainstFailed = $true
                    Add-VfLine '_DIFF' ('snap=' + $name + ' kind=saved_unreadable path=' + $rel + ' error=' + $li.Error)
                    continue
                }
                if (($li.Size -eq [long]$e.size) -and ($li.Sha256 -ceq [string]$e.sha256)) { $same++ }
                else {
                    $changed++
                    Add-VfLine '_DIFF' ('snap=' + $name + ' kind=changed path=' + $rel + ' size_snapshot=' + $e.size + ' size_saved=' + $li.Size)
                }
            }
            foreach ($rel in $LiveFiles) {
                if (-not $inSnap.Contains($rel)) {
                    $onlySaved++
                    Add-VfLine '_DIFF' ('snap=' + $name + ' kind=only_in_saved path=' + $rel)
                }
            }
            Add-VfLine '_AGAINST' ('snap=' + $name + ' same=' + $same + ' changed=' + $changed + ' only_in_snapshot=' + $onlySnap + ' only_in_saved=' + $onlySaved)
        }
    }
}

function Invoke-VerifyMain {
    # 1. Parametreler (kullanim hatasi = 2)
    if ($Last -lt 0) { Stop-Verify 2 'bad_last' }
    $haveSnap = ($Snapshot -ne '')
    $haveRoot = ($All -or ($Last -gt 0))
    if ($haveSnap -and $haveRoot) { Stop-Verify 2 'snapshot_and_root_selectors' }
    if ($All -and ($Last -gt 0)) { Stop-Verify 2 'all_and_last' }
    if ((-not $haveSnap) -and (-not $haveRoot)) { Stop-Verify 2 'no_target' }
    $expected = ''
    if ($ExpectedManifestSha256 -ne '') {
        if ($ExpectedManifestSha256 -cnotmatch '^[0-9a-fA-F]{64}\z') { Stop-Verify 2 'bad_expected_sha256' }
        if (-not $haveSnap) { Stop-Verify 2 'expected_requires_snapshot' }
        $expected = $ExpectedManifestSha256.ToLowerInvariant()
    }
    # 2. Hedef goruntu(ler)
    $dirs = New-Object System.Collections.ArrayList
    if ($haveSnap) {
        $d = $null
        try { $d = (ConvertTo-MMOFullPath $Snapshot).TrimEnd('\') } catch { Stop-Verify 2 'path_invalid' }
        $a = Get-MMOAttributes $d
        if ($a.State -eq 'missing') { Stop-Verify 3 'snapshot_missing' }
        if ($a.State -ne 'ok') { Stop-Verify 3 'snapshot_inaccessible' }
        if (($a.Attributes -band [IO.FileAttributes]::Directory) -eq 0) { Stop-Verify 3 'snapshot_not_directory' }
        [void]$dirs.Add($d)
    } else {
        $rootFull = $null
        $rootErr = $null
        try { $rootFull = Assert-MMORootSafe -Root $BackupRoot -AllowRootAncestorReparse:$AllowRootAncestorReparse } catch { $rootErr = $_.Exception.Message }
        if ($null -ne $rootErr) {
            if ($rootErr -ceq 'root_invalid') { Stop-Verify 2 'path_invalid' }
            Stop-Verify 3 ('backup_' + $rootErr)
        }
        $items = $null
        try { $items = @(Get-ChildItem -LiteralPath $rootFull -Force -ErrorAction Stop) } catch { $items = $null }
        if ($null -eq $items) { Stop-Verify 3 'backup_root_unreadable' }
        $cand = New-Object System.Collections.ArrayList
        foreach ($it in $items) {
            if ($it.Name -cmatch '^(?<p>pre-restore-)?(?<t>[0-9]{8}T[0-9]{6}Z)(-[a-z0-9-]{0,24})?\z') {
                [void]$cand.Add([pscustomobject]@{ Key = ($Matches['t'] + '|' + $it.Name); Path = $it.FullName })
            } else { $script:Counts['foreign']++ }
        }
        $keys = [string[]]@($cand | ForEach-Object { $_.Key })
        $paths = [string[]]@($cand | ForEach-Object { $_.Path })
        [Array]::Sort($keys, $paths, [StringComparer]::Ordinal)
        $start = 0
        if ($Last -gt 0 -and $paths.Length -gt $Last) { $start = $paths.Length - $Last }
        for ($i = $start; $i -lt $paths.Length; $i++) { [void]$dirs.Add($paths[$i]) }
        if ($dirs.Count -eq 0) { Stop-Verify 1 'no_snapshots' }
    }
    # 3. -Against (Saved kok guvenli mi; canli dosya listesi)
    $savedFull = ''
    $liveFiles = [string[]]@()
    $liveCache = @{}
    if ($Against -ne '') {
        $sErr = $null
        try { $savedFull = Assert-MMORootSafe -Root $Against -AllowRootAncestorReparse:$AllowRootAncestorReparse } catch { $sErr = $_.Exception.Message }
        if ($null -ne $sErr) {
            if ($sErr -ceq 'root_invalid') { Stop-Verify 2 'path_invalid' }
            Stop-Verify 3 ('against_' + $sErr)
        }
        $liveFiles = Get-VfSavedListing -SavedFull $savedFull
    }
    # 4. Dogrulama
    foreach ($d in $dirs) {
        Test-VfOneSnapshot -Dir $d -Expected $expected -LiveCache $liveCache -SavedFull $savedFull -LiveFiles $liveFiles
    }
    if ($script:AgainstFailed) { Stop-Verify 1 'against_unreadable' }
    $bad = $script:Counts['bad'] + $script:Counts['incomplete'] + $script:Counts['with_invalid']
    if ($bad -gt 0) { Stop-Verify 1 'verification_failed' }
}

$exitCode = 1
$reason = 'unexpected'
try {
    Invoke-VerifyMain
    $exitCode = 0
    $reason = 'ok'
} catch {
    $msg = $_.Exception.Message
    if ($msg -cmatch '^MMOVF\|(?<c>[0-9]+)\|(?<r>.*)\z') {
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
elseif ($exitCode -ne 0) { $status = 'failed' }
$c = $script:Counts
Write-Host ('[MMO_VERIFY_SUMMARY] status=' + $status + ' reason=' + $reason + ' exit=' + $exitCode + ' snapshots=' + $c['snapshots'] + ' ok=' + $c['ok'] + ' bad=' + $c['bad'] + ' incomplete=' + $c['incomplete'] + ' with_invalid=' + $c['with_invalid'] + ' invalid_entries=' + $c['invalid_entries'] + ' live=' + $c['live'] + ' warnings=' + $c['warnings'] + ' foreign=' + $c['foreign'])
foreach ($l in $script:Detail) { Write-Host $l }
exit $exitCode
