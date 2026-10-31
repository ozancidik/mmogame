# Kayit dosyalari yedekleme kutuphanesi testi (docs/DESIGN_BACKUP.md, dilim B0 + B1 + B2): Scripts/MMOSaveFilesLib.ps1 saf mantik + fikstur;
# B1 = Scripts/BackupSaves.ps1 fikstur Saved/yedek koku uzerinde (surec denetimi sahte saglayiciyla; gercek surec taramasi cagrilmaz).
# B2 = Scripts/VerifySaves.ps1 (salt okuma dogrulama) B1 ile alinmis sahte goruntuler uzerinde; salt okunurluk kaniti = tam agac damgasi (icerik+zaman) oncesi/sonrasi.
# B3 = Scripts/RestoreSaves.ps1 (geri yukleme; kuru calisma varsayilan, pre-restore, rollback) fikstur Saved/yedek koku uzerinde, hook'lu sahte surec saglayiciyla.
# Her sey gecici fikstur dizininde calisir (sentetik .sav/JSON); GERCEK Saved/ okunmaz ve yazilmaz. Tek istisna: "degismedi" kaniti
# icin gercek Saved\{SaveGames,MMOAchievements,MMODaily,MMOMail,MMORace} altinin YALNIZ dosya adi + boyut + zaman damgasi listesi
# (icerik okunmaz) testin basinda ve sonunda alinip karsilastirilir.
# Windows PowerShell 5.1 ve PowerShell 7. Dosya yalniz ASCII; ASCII disi karakterler [char] ile uretilir.
# Cikis: 0 PASS, 1 FAIL. Kalip: TestServerLifecycle.ps1 / TestFusion.ps1 (reported = case_lines = unique_labels = $expected);
# reported ve unique_labels basilan CASE satirlarinin METNINDEN yeniden sayilir, case_lines Check sayacidir (capraz kontrol).
# "Saved degismedi" kaniti hedef dizinler yoksa FAIL verir (-AllowVacuousSavedProof ile uyariya duser).
param([string]$RepoRoot = '', [switch]$AllowVacuousSavedProof)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
if ($RepoRoot -eq '') { $RepoRoot = Split-Path -Parent $PSScriptRoot }
. (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')

# Beklenen = benzersiz case/check etiketi sayisi (asagidaki bolumlerin toplami; degisirse burada guncelle).
$expected = 1460

$script:labels = @{}
$script:caseLines = 0
$script:failures = 0
$script:emitted = New-Object System.Collections.ArrayList     # basilan CASE satirlari (rapor sayimi bundan, bagimsiz)
$script:fixtureRoot = $null
$script:junctions = New-Object System.Collections.ArrayList

function Check {
    param([string]$Case, [string]$Check, [bool]$Cond)
    $label = $Case + '/' + $Check
    $script:caseLines++
    $dup = $script:labels.ContainsKey($label)
    $script:labels[$label] = $true
    $ok = $Cond -and (-not $dup)
    if (-not $ok) { $script:failures++ }
    $tag = 'PASS'
    if (-not $ok) { $tag = 'FAIL' }
    $note = ''
    if ($dup) { $note = ' duplicate_label' }
    $line = '[MMO_BACKUP_CASE] ' + $tag + ' case=' + $Case + ' check=' + $Check + $note
    [void]$script:emitted.Add($line)
    Write-Host $line
}

function Get-ThrowsResult {
    # Bos on ek YASAK (her atma "gecer" olurdu); karsilastirma ordinal.
    param([scriptblock]$Block, [string]$Prefix)
    $msg = $null
    try { & $Block | Out-Null } catch { $msg = $_.Exception.Message }
    return (($Prefix.Length -gt 0) -and ($null -ne $msg) -and $msg.StartsWith($Prefix, [StringComparison]::Ordinal))
}

function CheckThrows {
    param([string]$Case, [string]$Check, [scriptblock]$Block, [string]$Prefix)
    Check $Case $Check (Get-ThrowsResult $Block $Prefix)
}

function HasErr {
    param($Errors, [string]$Prefix)
    return (@(@($Errors) | Where-Object { $_.StartsWith($Prefix) }).Count -gt 0)
}

# ---- yardimcilar -------------------------------------------------------------------------------------------------------
$oUml = [string][char]0x00D6
$sCed = [string][char]0x015F
$trWord = ([string][char]0x00C7) + 'a' + [char]0x011F + 'r' + [char]0x0131      # "Cagri" Turkce harflerle

function Get-SavedStamp {
    # Yalniz METADATA: her hedef dizinin var/yok bilgisi + altindaki dosya VE alt dizinlerin yolu, boyutu, zaman damgasi (icerik OKUNMAZ).
    # Yeni/silinen dosya, yeni/silinen dizin, boyut ya da zaman farki digest'i degistirir. DirsPresent = var olan hedef dizin sayisi.
    # -WithContent: YALNIZ fikstur dizinleri icin her dosyanin SHA-256 icerik ozeti de girer (ayni boyut + ayni zaman damgasiyla yapilan icerik
    # degisikligini yakalar); gercek Saved'de KULLANILMAZ (icerik okunmaz).
    param([string]$SavedRoot = (Join-Path $RepoRoot 'Saved'), [switch]$WithContent)
    $lines = New-Object System.Collections.ArrayList
    $present = 0
    foreach ($d in 'SaveGames', 'MMOAchievements', 'MMODaily', 'MMOMail', 'MMORace') {
        $p = Join-Path $SavedRoot $d
        if (Test-Path -LiteralPath $p -PathType Container) {
            $present++
            [void]$lines.Add('E|' + $d + '|1')
            foreach ($f in (Get-ChildItem -LiteralPath $p -Recurse -Force -ErrorAction Stop)) {
                if ($f.PSIsContainer) { [void]$lines.Add('D|' + $f.FullName + '|' + $f.LastWriteTimeUtc.Ticks) }
                else {
                    $ln = 'F|' + $f.FullName + '|' + $f.Length + '|' + $f.LastWriteTimeUtc.Ticks
                    if ($WithContent) { $ln += '|' + (Get-OracleSha $f.FullName) }
                    [void]$lines.Add($ln)
                }
            }
        } else {
            [void]$lines.Add('E|' + $d + '|0')
        }
    }
    $arr = [string[]]@($lines.ToArray())
    [Array]::Sort($arr, [StringComparer]::Ordinal)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $h = $sha.ComputeHash((New-Object System.Text.UTF8Encoding($false)).GetBytes(($arr -join "`n"))) } finally { $sha.Dispose() }
    return [pscustomobject]@{ Count = $arr.Length; DirsPresent = $present; Digest = (($h | ForEach-Object { $_.ToString('x2') }) -join '') }
}

function New-TestBytes {
    param([int]$Length, [int]$Seed)
    $b = New-Object byte[] $Length
    for ($i = 0; $i -lt $Length; $i++) { $b[$i] = [byte](($i * 31 + $Seed) % 251) }
    return , $b
}

function New-SavBytes {
    param([int]$Length, [int]$Seed)
    $b = New-TestBytes $Length $Seed
    $b[0] = 0x47; $b[1] = 0x56; $b[2] = 0x41; $b[3] = 0x53
    return , $b
}

function Write-TestBytes {
    param([string]$Path, [byte[]]$Bytes)
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path))
    [IO.File]::WriteAllBytes($Path, $Bytes)
}

function Write-TestText {
    param([string]$Path, [string]$Text, [switch]$Bom)
    $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($Text)
    if ($Bom) { $bytes = [byte[]](@(0xEF, 0xBB, 0xBF) + $bytes) }
    Write-TestBytes $Path $bytes
}

function Get-OracleSha {
    # Kutuphaneden bagimsiz hash: tum dosya bellege, .NET dogrudan.
    param([string]$Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return (($sha.ComputeHash([IO.File]::ReadAllBytes($Path)) | ForEach-Object { $_.ToString('x2') }) -join '') } finally { $sha.Dispose() }
}

function Get-BytesSha {
    param([byte[]]$Bytes)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return (($sha.ComputeHash($Bytes) | ForEach-Object { $_.ToString('x2') }) -join '') } finally { $sha.Dispose() }
}

function Get-OracleDigest {
    # Kutuphaneden bagimsiz entries_digest: ordinal (CompareOrdinal) ekleme siralamasi.
    param($Entries)
    $list = New-Object System.Collections.ArrayList
    foreach ($e in @($Entries)) {
        $idx = 0
        while ($idx -lt $list.Count -and [string]::CompareOrdinal([string]$list[$idx].path, [string]$e.path) -lt 0) { $idx++ }
        $list.Insert($idx, $e)
    }
    $text = ''
    foreach ($e in $list) {
        $vs = [string]([bool]$e.valid).ToString().ToLowerInvariant()
        $text += ([string]$e.path + [char]0 + [string]$e.class + [char]0 + [string]$e.sha256 + [char]0 + [string]$e.size + [char]0 + $vs + [char]0 + [string]$e.notes + "`n")
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return (($sha.ComputeHash((New-Object System.Text.UTF8Encoding($false)).GetBytes($text)) | ForEach-Object { $_.ToString('x2') }) -join '') } finally { $sha.Dispose() }
}

function New-BaseFiles {
    param([string]$FilesRoot)
    $j = '{"version":1,"day":20261027,"bounties":[],"note":"' + $trWord + '"}'
    Write-TestBytes (Join-Path $FilesRoot 'SaveGames\MMOSave_Ali.sav') (New-SavBytes 200 1)
    Write-TestBytes (Join-Path $FilesRoot 'SaveGames\MMOSave_Ali Baba.sav') (New-SavBytes 150 2)
    Write-TestBytes (Join-Path $FilesRoot 'SaveGames\MMOSave_Bad.sav') (New-TestBytes 100 3)
    Write-TestText (Join-Path $FilesRoot 'MMOAchievements\MMOSave_Ali.json') '{"version":1,"unlocked":[1,2]}'
    Write-TestText (Join-Path $FilesRoot 'MMOAchievements\MMOSave_Ali.json.bad') 'garbage not json'
    Write-TestText (Join-Path $FilesRoot 'MMODaily\MMOSave_Ali.json') $j
    Write-TestText (Join-Path $FilesRoot 'MMODaily\MMOSave_Ali.json.tmp') '{"version":1,"na'
    Write-TestText (Join-Path $FilesRoot 'MMOMail\MMOMail_ali.json') '{"version":2,"mails":[]}'
    Write-TestText (Join-Path $FilesRoot 'MMOMail\MMOMail_ali.json.bak') '{"version":2,"mails":[]}'
    Write-TestText (Join-Path $FilesRoot 'MMOMail\MMOMail_ali_baba.json') '{"version":2,"mails":[]}'
    # Gercek irk dosya adi: MMORace_<SlotKey("MMOSave_<ad>",64).ToLower()> (MMORaceStore.cpp:42)
    Write-TestText (Join-Path $FilesRoot 'MMORace\MMORace_mmosave_ali.json') '{"version":1,"race":2}'
    Write-TestText (Join-Path $FilesRoot 'MMORace\MMORace_mmosave_ali.json.bad') 'lock'
}

function New-TestSnapshot {
    param([string]$Name, $ShutdownProof = $null, [string]$Mode = 'quiesced')
    $dir = Join-Path $script:fixtureRoot ('snap_' + $Name)
    $files = Join-Path $dir 'files'
    [void][IO.Directory]::CreateDirectory($files)
    New-BaseFiles $files
    $m = New-MMOManifest -FilesRoot $files -SourceSavedDir (Join-Path $script:fixtureRoot ('saved_' + $sCed)) -Mode $Mode -ShutdownProof $ShutdownProof -CreatedUtc '2026-10-27T10:00:00Z'
    Write-MMOManifest -Manifest $m -SnapshotDir $dir
    return $dir
}

function Edit-TestManifest {
    param([string]$Snap, [scriptblock]$Edit, [switch]$FixDigest)
    $p = Join-Path $Snap 'manifest.json'
    $m = (Read-MMOUtf8Text $p).Text | ConvertFrom-Json
    & $Edit $m
    if ($FixDigest) {
        $m.entries_digest = Get-MMOEntriesDigest $m.entries
        $m.header_digest = Get-MMOHeaderDigest $m
    }
    $json = $m | ConvertTo-Json -Depth 8
    [IO.File]::WriteAllBytes($p, (New-Object System.Text.UTF8Encoding($false)).GetBytes($json))
}

function New-TestJunction {
    param([string]$Link, [string]$Target)
    [void](New-Item -ItemType Junction -Path $Link -Target $Target)
    [void]$script:junctions.Add($Link)
}

function Remove-TestJunction {
    param([string]$Link)
    if (Test-MMOReparse $Link) { [IO.Directory]::Delete($Link) }
}

$savedBefore = Get-SavedStamp
# BackupSaves.ps1'in test parametreleri (-ProcessProvider/-Timestamp/-TestHooks) yalniz MMO_BACKUP_TESTMODE=1 iken etkindir (D1); test boyunca acik,
# finally'de ESKI degerine doner (yoksa silinir). Kapi vakalari degeri kendileri gecici olarak kaldirir.
$script:prevTestMode = $env:MMO_BACKUP_TESTMODE
$env:MMO_BACKUP_TESTMODE = '1'
$guard = [Guid]::NewGuid().ToString('N')
$script:fixtureRoot = Join-Path $env:TEMP ('mmo_backup_test_' + $guard + '_' + $sCed + $oUml)
$tempFull = [IO.Path]::GetFullPath($env:TEMP)
try {
    # Fikstur kokunun gercek depo/Saved icinde olmadigi kanitlanmadan hicbir sey yazilmaz.
    if ((Test-MMOPathUnder -Path $script:fixtureRoot -Root $RepoRoot -AllowEqual) -or -not (Test-MMOPathUnder -Path $script:fixtureRoot -Root $tempFull)) {
        throw 'fixture_root_not_isolated'
    }
    [void][IO.Directory]::CreateDirectory($script:fixtureRoot)
    $fx = $script:fixtureRoot

    # ---- 0. Test altyapisinin kendi kapilari -------------------------------------------------------------------------
    Check 'harness' 'throws_empty_prefix_never_passes' (-not (Get-ThrowsResult { throw 'anything' } ''))
    Check 'harness' 'throws_no_throw_fails' (-not (Get-ThrowsResult { 1 } 'x'))
    Check 'harness' 'throws_prefix_match_passes' (Get-ThrowsResult { throw 'abc:def' } 'abc:')
    Check 'harness' 'throws_prefix_is_case_sensitive' (-not (Get-ThrowsResult { throw 'abc:def' } 'ABC:'))
    Check 'harness' 'throws_prefix_mismatch_fails' (-not (Get-ThrowsResult { throw 'abc:def' } 'def'))

    # ---- 1. Oyuncu adi kurali + kanonik anahtar ----------------------------------------------------------------------
    $nameCases = @(
        @('ascii', 'Ali', $true), @('space_mid', 'Ali Baba', $true), @('under_dash', 'a_b-c', $true), @('digits', 'A1', $true),
        @('len32', ('a' * 32), $true), @('len33', ('a' * 33), $false), @('empty', '', $false), @('lead_space', ' Ali', $false),
        @('trail_space', 'Ali ', $false), @('bang', 'Ali!', $false), @('o_uml', ($oUml + 'mer'), $false),
        @('s_ced', ('Ali' + $sCed), $false), @('dot', 'ali.b', $false), @('slash', 'a/b', $false), @('backslash', 'a\b', $false),
        @('colon', 'a:b', $false), @('tab', "a`tb", $false), @('newline', "a`n", $false), @('hyphen_only', '-', $true)
    )
    foreach ($c in $nameCases) { Check 'name' ('valid_' + $c[0]) ((Test-MMOPlayerName $c[1]) -eq $c[2]) }
    Check 'name' 'valid_null' ((Test-MMOPlayerName $null) -eq $false)
    $keyCases = @(@('space', 'Ali Baba', 'ali_baba'), @('upper', 'ALI', 'ali'), @('mixed', 'a-b_C', 'a-b_c'), @('plain', 'ali', 'ali'), @('digits', 'Gezgin123456', 'gezgin123456'))
    foreach ($c in $keyCases) { Check 'name' ('key_' + $c[0]) ((ConvertTo-MMOCanonicalKey $c[1]) -ceq $c[2]) }
    Check 'name' 'key_invalid_null' ($null -eq (ConvertTo-MMOCanonicalKey ($oUml + 'mer')))
    Check 'name' 'filekey_ok' (Test-MMOFileKey 'ali_baba-1')
    Check 'name' 'filekey_upper_rejected' (-not (Test-MMOFileKey 'Ali'))
    Check 'name' 'filekey_space_rejected' (-not (Test-MMOFileKey 'ali baba'))

    # ---- 2. Goreli yol sozdizimi -------------------------------------------------------------------------------------
    $goodRel = @('SaveGames/MMOSave_Ali.sav', 'MMOMail/MMOMail_ali_baba.json.bak', 'a', 'a-b/c_d.e', 'SaveGames/CONSOLE.sav', 'SaveGames/COM10.sav', 'SaveGames/.hidden')
    $gi = 0
    foreach ($g in $goodRel) { $gi++; Check 'relpath' ('good_' + $gi) (Test-MMOSafeRelPath $g) }
    $badRel = @(
        @('dotdot_lead', '../x', 'dot_segment'), @('dotdot_mid', 'SaveGames/../x', 'dot_segment'), @('dot_mid', 'SaveGames/./x', 'dot_segment'),
        @('dots3', 'SaveGames/.../x', 'trailing_dot_space'), @('abs_slash', '/etc/x', 'rooted'), @('drive', 'C:/x', 'colon'),
        @('drive_rel', 'C:x', 'colon'), @('ads', 'SaveGames/a.sav:evil', 'colon'), @('backslash', 'SaveGames\a.sav', 'backslash'),
        @('empty_seg', 'SaveGames//a', 'empty_segment'), @('trail_slash', 'SaveGames/', 'empty_segment'),
        @('con', 'SaveGames/CON', 'device_name'), @('con_lower_ext', 'SaveGames/con.sav', 'device_name'),
        @('nul_ext', 'SaveGames/NUL.txt', 'device_name'), @('com1_json', 'SaveGames/COM1.json', 'device_name'),
        @('lpt9', 'SaveGames/lpt9', 'device_name'), @('aux_space', 'SaveGames/AUX .sav', 'device_name'),
        @('trail_dot', 'SaveGames/a.', 'trailing_dot_space'), @('trail_space', 'SaveGames/a ', 'trailing_dot_space'),
        @('ctrl_tab', "SaveGames/a`tb", 'control_char'), @('ctrl_del', ('SaveGames/a' + [char]127), 'control_char'),
        @('star', 'SaveGames/a*', 'invalid_char'), @('quest', 'SaveGames/a?', 'invalid_char'), @('pipe', 'a|b', 'invalid_char'),
        @('lt', 'a<b', 'invalid_char'), @('empty', '', 'empty'), @('long', ('a' * 201), 'too_long'),
        @('conin', 'SaveGames/CONIN$', 'device_name'), @('conout_ext', 'SaveGames/conout$.sav', 'device_name'),
        @('clock', 'SaveGames/CLOCK$.json', 'device_name'), @('prn', 'SaveGames/PRN.sav', 'device_name'),
        @('com_sup1', ('SaveGames/COM' + [char]0x00B9 + '.sav'), 'device_name'), @('com_sup3', ('SaveGames/com' + [char]0x00B3), 'device_name'),
        @('lpt_sup2', ('SaveGames/LPT' + [char]0x00B2 + '.json'), 'device_name'), @('com0', 'SaveGames/COM0.sav', 'device_name'),
        @('device_in_dir', 'NUL/MMOSave_a.sav', 'device_name')
    )
    $goodDev = @('SaveGames/CONIN.sav', 'SaveGames/CLOCK.sav', 'SaveGames/COM.sav', 'SaveGames/COM11.sav', 'SaveGames/LPT10', 'SaveGames/MyCON.sav')
    $gdi = 0
    foreach ($g in $goodDev) { $gdi++; Check 'relpath' ('device_lookalike_ok_' + $gdi) (Test-MMOSafeRelPath $g) }
    foreach ($c in $badRel) {
        $reason = Get-MMORelPathProblem $c[1]
        Check 'relpath' ('bad_' + $c[0]) (($null -ne $reason) -and ($reason -ceq $c[2]))
    }
    Check 'relpath' 'bad_null' ((Get-MMORelPathProblem $null) -ceq 'empty')
    Check 'relpath' 'safe_false_for_dotdot' (-not (Test-MMOSafeRelPath '../x'))

    # ---- 3. Kanonik "altinda mi" -------------------------------------------------------------------------------------
    $ur = 'C:\fx\root'
    Check 'under' 'child' (Test-MMOPathUnder -Path 'C:\fx\root\a\b' -Root $ur)
    Check 'under' 'equal_strict' (-not (Test-MMOPathUnder -Path 'C:\fx\root' -Root $ur))
    Check 'under' 'equal_allowed' (Test-MMOPathUnder -Path 'C:\fx\root' -Root $ur -AllowEqual)
    Check 'under' 'prefix_sibling' (-not (Test-MMOPathUnder -Path 'C:\fx\rootx\a' -Root $ur))
    Check 'under' 'parent' (-not (Test-MMOPathUnder -Path 'C:\fx' -Root $ur))
    Check 'under' 'dotdot_escape' (-not (Test-MMOPathUnder -Path 'C:\fx\root\..\x' -Root $ur))
    Check 'under' 'dotdot_inside' (Test-MMOPathUnder -Path 'C:\fx\root\a\..\b' -Root $ur)
    Check 'under' 'case_insensitive' (Test-MMOPathUnder -Path 'c:\FX\ROOT\a' -Root $ur)
    Check 'under' 'root_trailing_slash' (Test-MMOPathUnder -Path 'C:\fx\root\a' -Root 'C:\fx\root\')
    Check 'under' 'other_drive' (-not (Test-MMOPathUnder -Path 'D:\fx\root\a' -Root $ur))
    Check 'under' 'fixture_not_in_repo' (-not (Test-MMOPathUnder -Path $fx -Root $RepoRoot -AllowEqual))
    Check 'under' 'real_saved_is_in_repo' (Test-MMOPathUnder -Path (Join-Path $RepoRoot 'Saved\SaveGames') -Root $RepoRoot)

    # ---- 4. Dosya siniflandirma --------------------------------------------------------------------------------------
    # id, yol, sinif, tur, grup, sonek, (ignored ise neden)
    $clsCases = @(
        @('sg_ali', 'SaveGames/MMOSave_Ali.sav', 'character', 'character', 'ali', '', ''),
        @('sg_space', 'SaveGames/MMOSave_Ali Baba.sav', 'character', 'character', 'ali_baba', '', ''),
        @('sg_upper', 'SaveGames/MMOSave_ALI.sav', 'character', 'character', 'ali', '', ''),
        @('sg_gezgin', 'SaveGames/MMOSave_Gezgin123456.sav', 'character', 'character', 'gezgin123456', '', ''),
        @('sg_len32', ('SaveGames/MMOSave_' + ('a' * 32) + '.sav'), 'character', 'character', ('a' * 32), '', ''),
        @('ach_json', 'MMOAchievements/MMOSave_Ali.json', 'side', 'achievement', 'ali', '', ''),
        @('ach_tmp', 'MMOAchievements/MMOSave_Ali.json.tmp', 'side', 'achievement', 'ali', '.tmp', ''),
        @('ach_bad', 'MMOAchievements/MMOSave_Ali.json.bad', 'side-lock', 'achievement', 'ali', '.bad', ''),
        @('ach_tmpbad', 'MMOAchievements/MMOSave_Ali.json.tmp.bad', 'side-lock', 'achievement', 'ali', '.tmp.bad', ''),
        @('daily_json', 'MMODaily/MMOSave_Ali Baba.json', 'side', 'daily', 'ali_baba', '', ''),
        @('daily_bad', 'MMODaily/MMOSave_Ali.json.bad', 'side-lock', 'daily', 'ali', '.bad', ''),
        @('mail_json', 'MMOMail/MMOMail_ali_baba.json', 'side', 'mail', 'ali_baba', '', ''),
        @('mail_bak', 'MMOMail/MMOMail_ali.json.bak', 'side', 'mail', 'ali', '.bak', ''),
        @('mail_tmpbad', 'MMOMail/MMOMail_ali.json.tmp.bad', 'side-lock', 'mail', 'ali', '.tmp.bad', ''),
        @('race_json', 'MMORace/MMORace_mmosave_ali.json', 'side', 'race', 'ali', '', ''),
        @('race_bad', 'MMORace/MMORace_mmosave_ali.json.bad', 'side-lock', 'race', 'ali', '.bad', ''),
        @('race_tmp', 'MMORace/MMORace_mmosave_ali.json.tmp', 'side', 'race', 'ali', '.tmp', ''),
        @('race_tmpbad', 'MMORace/MMORace_mmosave_ali.json.tmp.bad', 'side-lock', 'race', 'ali', '.tmp.bad', ''),
        @('race_space_name', 'MMORace/MMORace_mmosave_ali_baba.json', 'side', 'race', 'ali_baba', '', ''),
        @('race_len25', ('MMORace/MMORace_mmosave_' + ('a' * 25) + '.json'), 'side', 'race', ('a' * 25), '', ''),
        @('race_len32', ('MMORace/MMORace_mmosave_' + ('a' * 32) + '.json'), 'side', 'race', ('a' * 32), '', ''),
        @('race_len32_bad', ('MMORace/MMORace_mmosave_' + ('a' * 32) + '.json.bad'), 'side-lock', 'race', ('a' * 32), '.bad', ''),
        @('race_default_slot', 'MMORace/MMORace_mmosave_default.json', 'side', 'race', 'default', '', ''),
        @('race_legacy_json', 'MMORace/MMORace_ali.json', 'side', 'race', 'ali', '', ''),
        @('race_legacy_bad', 'MMORace/MMORace_ali.json.bad', 'side-lock', 'race', 'ali', '.bad', ''),
        @('race_key64', ('MMORace/MMORace_' + ('b' * 64) + '.json'), 'side', 'race', ('b' * 64), '', ''),
        @('race_key65', ('MMORace/MMORace_' + ('b' * 65) + '.json'), 'ignored', '', '', '', 'name_rule'),
        @('race_prefix_only', 'MMORace/MMORace_mmosave_.json', 'ignored', '', '', '', 'name_rule'),
        @('race_upper_prefix', 'MMORace/MMORace_MMOSave_ali.json', 'ignored', '', '', '', 'name_rule'),
        @('race_space', 'MMORace/MMORace_mmosave_ali baba.json', 'ignored', '', '', '', 'name_rule'),
        @('sg_len33', ('SaveGames/MMOSave_' + ('a' * 33) + '.sav'), 'ignored', '', '', '', 'name_rule'),
        @('sg_nonmmo', 'SaveGames/Other.sav', 'ignored', '', '', '', 'not_whitelisted_name'),
        @('sg_lower_prefix', 'SaveGames/mmosave_ali.sav', 'ignored', '', '', '', 'not_whitelisted_name'),
        @('sg_upper_ext', 'SaveGames/MMOSave_Ali.SAV', 'ignored', '', '', '', 'not_whitelisted_name'),
        @('sg_bak', 'SaveGames/MMOSave_Ali.sav.bak', 'ignored', '', '', '', 'not_whitelisted_name'),
        @('sg_double_ext', 'SaveGames/MMOSave_a.sav.sav', 'ignored', '', '', '', 'name_rule'),
        @('sg_umlaut', ('SaveGames/MMOSave_' + $oUml + 'mer.sav'), 'ignored', '', '', '', 'name_rule'),
        @('sg_cedilla', ('SaveGames/MMOSave_Ali' + $sCed + '.sav'), 'ignored', '', '', '', 'name_rule'),
        @('sg_bang', 'SaveGames/MMOSave_Ali!.sav', 'ignored', '', '', '', 'name_rule'),
        @('sg_trailing_space_name', 'SaveGames/MMOSave_Ali .sav', 'ignored', '', '', '', 'name_rule'),
        @('sg_subdir', 'SaveGames/sub/MMOSave_a.sav', 'ignored', '', '', '', 'depth'),
        @('sg_json_ext', 'SaveGames/MMOSave_a.json', 'ignored', '', '', '', 'not_whitelisted_name'),
        @('dir_lowercase', 'savegames/MMOSave_a.sav', 'ignored', '', '', '', 'not_whitelisted_dir'),
        @('ach_bak', 'MMOAchievements/MMOSave_Ali.json.bak', 'ignored', '', '', '', 'not_whitelisted_name'),
        @('daily_bak', 'MMODaily/MMOSave_Ali.json.bak', 'ignored', '', '', '', 'not_whitelisted_name'),
        @('daily_wrong_prefix', 'MMODaily/MMOMail_a.json', 'ignored', '', '', '', 'not_whitelisted_name'),
        @('mail_upper', 'MMOMail/MMOMail_Ali.json', 'ignored', '', '', '', 'name_rule'),
        @('mail_space', 'MMOMail/MMOMail_ali baba.json', 'ignored', '', '', '', 'name_rule'),
        @('mail_umlaut', ('MMOMail/MMOMail_' + $oUml + '.json'), 'ignored', '', '', '', 'name_rule'),
        @('race_bak', 'MMORace/MMORace_ali.json.bak', 'ignored', '', '', '', 'not_whitelisted_name'),
        @('logs', 'Logs/x.log', 'ignored', '', '', '', 'not_whitelisted_dir'),
        @('config', 'Config/a.ini', 'ignored', '', '', '', 'not_whitelisted_dir'),
        @('llm_budget', 'MMO/LLMDailyBudget.txt', 'ignored', '', '', '', 'not_whitelisted_dir'),
        @('unsafe_dotdot', '../SaveGames/MMOSave_a.sav', 'ignored', '', '', '', 'unsafe_path:dot_segment'),
        @('unsafe_backslash', 'SaveGames\MMOSave_a.sav', 'ignored', '', '', '', 'unsafe_path:backslash'),
        @('empty', '', 'ignored', '', '', '', 'unsafe_path:empty')
    )
    foreach ($c in $clsCases) {
        $r = Get-MMOSaveFileClass $c[1]
        Check 'class' ($c[0] + '.class') ($r.Class -ceq $c[2])
        if ($c[2] -eq 'ignored') {
            Check 'class' ($c[0] + '.reason') ($r.Reason -ceq $c[6])
        } else {
            Check 'class' ($c[0] + '.kind') ($r.Kind -ceq $c[3])
            Check 'class' ($c[0] + '.group') ($r.Group -ceq $c[4])
            Check 'class' ($c[0] + '.suffix') ($r.Suffix -ceq $c[5])
        }
    }

    # ---- 5. Slot gruplari ve harf cakismalari ------------------------------------------------------------------------
    $slotPaths = @(
        'SaveGames/MMOSave_Ali Baba.sav', 'MMOAchievements/MMOSave_Ali Baba.json', 'MMODaily/MMOSave_Ali Baba.json.tmp',
        'MMOMail/MMOMail_ali_baba.json.bak', 'MMORace/MMORace_mmosave_ali_baba.json.bad', 'SaveGames/MMOSave_Veli.sav',
        'MMOMail/MMOMail_veli.json', 'SaveGames/Other.sav', 'Logs/x.log', ('SaveGames/MMOSave_' + $oUml + 'mer.sav')
    )
    $groups = Get-MMOSlotGroups $slotPaths
    Check 'slotgroup' 'group_count' ($groups.Count -eq 2)
    Check 'slotgroup' 'ali_baba_present' ($groups.Contains('ali_baba'))
    Check 'slotgroup' 'ali_baba_items' ($groups['ali_baba'].Items.Count -eq 5)
    Check 'slotgroup' 'veli_items' ($groups['veli'].Items.Count -eq 2)
    $aliKinds = (@($groups['ali_baba'].Items | ForEach-Object { $_.Kind }) | Sort-Object) -join ','
    Check 'slotgroup' 'ali_baba_kinds' ($aliKinds -ceq 'achievement,character,daily,mail,race')
    Check 'slotgroup' 'ignored_excluded' (-not $groups.Contains('other') -and -not $groups.Contains($oUml.ToLowerInvariant() + 'mer'))
    Check 'slotgroup' 'empty_input' ((Get-MMOSlotGroups @()).Count -eq 0)
    $col1 = @(Find-MMOCollisions @('SaveGames/MMOSave_ALI.sav', 'SaveGames/MMOSave_ali.sav', 'SaveGames/MMOSave_Veli.sav'))
    Check 'collision' 'case_pair_found' ($col1.Count -eq 1)
    Check 'collision' 'case_pair_members' (($col1.Count -eq 1) -and (@($col1[0]).Count -eq 2))
    $col2 = @(Find-MMOCollisions @('SaveGames/MMOSave_Ali Baba.sav', 'SaveGames/MMOSave_ali_baba.sav'))
    Check 'collision' 'space_underscore_pair' ($col2.Count -eq 1)
    $col3 = @(Find-MMOCollisions @('SaveGames/MMOSave_Ali.sav', 'MMOMail/MMOMail_ali.json', 'MMODaily/MMOSave_Ali.json'))
    Check 'collision' 'different_kinds_ok' ($col3.Count -eq 0)
    $col4 = @(Find-MMOCollisions @('Logs/A.log', 'Logs/a.LOG'))
    Check 'collision' 'ignored_case_pair' ($col4.Count -eq 1)
    Check 'collision' 'same_path_twice_ok' (@(Find-MMOCollisions @('SaveGames/MMOSave_Ali.sav', 'SaveGames/MMOSave_Ali.sav')).Count -eq 0)
    Check 'collision' 'suffix_distinct_ok' (@(Find-MMOCollisions @('MMOMail/MMOMail_ali.json', 'MMOMail/MMOMail_ali.json.bak')).Count -eq 0)

    # ---- 6. SHA-256 --------------------------------------------------------------------------------------------------
    $shaDir = Join-Path $fx 'sha'
    Write-TestBytes (Join-Path $shaDir 'empty.bin') ([byte[]]@())
    Write-TestBytes (Join-Path $shaDir 'abc.bin') ([byte[]][char[]]'abc')
    Write-TestBytes (Join-Path $shaDir 'big.bin') (New-TestBytes 3000000 9)
    Write-TestBytes (Join-Path $shaDir ($sCed + $oUml + '.bin')) (New-TestBytes 1000 4)
    Check 'sha' 'empty_known' ((Get-MMOFileSha256 (Join-Path $shaDir 'empty.bin')) -ceq 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855')
    Check 'sha' 'abc_known' ((Get-MMOFileSha256 (Join-Path $shaDir 'abc.bin')) -ceq 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
    Check 'sha' 'big_matches_oracle' ((Get-MMOFileSha256 (Join-Path $shaDir 'big.bin')) -ceq (Get-OracleSha (Join-Path $shaDir 'big.bin')))
    Check 'sha' 'non_ascii_name' ((Get-MMOFileSha256 (Join-Path $shaDir ($sCed + $oUml + '.bin'))) -ceq (Get-OracleSha (Join-Path $shaDir ($sCed + $oUml + '.bin'))))
    Check 'sha' 'lowercase_hex64' ((Get-MMOFileSha256 (Join-Path $shaDir 'abc.bin')) -cmatch '^[0-9a-f]{64}\z')
    $wpath = Join-Path $shaDir 'writer.bin'
    Write-TestBytes $wpath (New-TestBytes 500 5)
    $w = [IO.File]::Open($wpath, [IO.FileMode]::Open, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
    try { $shaOpen = Get-MMOFileSha256 $wpath } finally { $w.Dispose() }
    Check 'sha' 'readable_while_writer_open' ($shaOpen -ceq (Get-OracleSha $wpath))
    CheckThrows 'sha' 'missing_file_throws' { Get-MMOFileSha256 (Join-Path $shaDir 'nope.bin') } 'file_missing'
    $snapInfo = Get-MMOFileSnapshotInfo -Path (Join-Path $shaDir 'abc.bin') -ClassInfo (Get-MMOSaveFileClass 'MMOMail/MMOMail_ali.json.tmp.bad')
    Check 'sha' 'snapshot_info_single_read_consistent' (($snapInfo.Size -eq 3) -and ($snapInfo.Bytes.Length -eq 3) -and ($snapInfo.Sha256 -ceq (Get-MMOBytesSha256 $snapInfo.Bytes)) -and ($snapInfo.Sha256 -ceq 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'))
    Check 'sha' 'bytes_hash_empty' ((Get-MMOBytesSha256 ([byte[]]@())) -ceq 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855')
    # Boyut ust siniri (8 MB): tam sinirda okunur, 1 bayt fazlasi reddedilir
    $capDir = Join-Path $fx 'cap'
    Write-TestBytes (Join-Path $capDir 'exact.bin') (New-Object byte[] 8388608)
    Write-TestBytes (Join-Path $capDir 'over.bin') (New-Object byte[] 8388609)
    Check 'cap' 'exact_limit_read' ((Read-MMOFileBytes (Join-Path $capDir 'exact.bin')).Length -eq 8388608)
    CheckThrows 'cap' 'over_limit_rejected' { Read-MMOFileBytes (Join-Path $capDir 'over.bin') } 'too_large'
    CheckThrows 'cap' 'custom_limit_rejected' { Read-MMOFileBytes -Path (Join-Path $shaDir 'abc.bin') -MaxBytes 2 } 'too_large'
    Check 'cap' 'sav_over_limit_invalid' (-not (Test-MMOSavFile -Path (Join-Path $capDir 'over.bin')).Valid -and (Test-MMOSavFile -Path (Join-Path $capDir 'over.bin')).Reason -ceq 'too_large')
    Check 'cap' 'json_over_limit_invalid' (-not (Test-MMOJsonFile -Path (Join-Path $capDir 'over.bin')).Valid -and (Test-MMOJsonFile -Path (Join-Path $capDir 'over.bin')).Reason -ceq 'too_large')
    $ovr = Get-MMOFileIntegrity -Path (Join-Path $capDir 'over.bin') -ClassInfo (Get-MMOSaveFileClass 'MMODaily/MMOSave_Ali.json')
    Check 'cap' 'integrity_over_limit_invalid' ((-not $ovr.Valid) -and $ovr.Notes -ceq 'too_large')
    Check 'cap' 'bytes_over_limit_invalid' (-not (Test-MMOJsonBytes -Bytes (New-Object byte[] 8388609)).Valid)

    # ---- 7. .sav (GVAS) butunlugu ------------------------------------------------------------------------------------
    $savDir = Join-Path $fx 'sav'
    $savCases = @(
        @('good200', (New-SavBytes 200 1), $true, ''),
        @('exact_min', (New-SavBytes 64 2), $true, ''),
        @('one_below_min', (New-SavBytes 63 3), $false, 'too_small'),
        @('magic_only', ([byte[]]@(0x47, 0x56, 0x41, 0x53)), $false, 'too_small'),
        @('empty', ([byte[]]@()), $false, 'too_small'),
        @('wrong_magic', (New-TestBytes 100 4), $false, 'bad_magic'),
        @('lowercase_magic', ([byte[]](@(0x67, 0x76, 0x61, 0x73) + (New-TestBytes 96 5))), $false, 'bad_magic'),
        @('magic_at_offset', ([byte[]](@(0, 0, 0, 0x47, 0x56, 0x41, 0x53) + (New-TestBytes 96 6))), $false, 'bad_magic'),
        @('gvat', ([byte[]](@(0x47, 0x56, 0x41, 0x54) + (New-TestBytes 96 7))), $false, 'bad_magic'),
        @('only_byte0_wrong', ([byte[]](@(0x48, 0x56, 0x41, 0x53) + (New-TestBytes 96 8))), $false, 'bad_magic'),
        @('only_byte1_wrong', ([byte[]](@(0x47, 0x57, 0x41, 0x53) + (New-TestBytes 96 9))), $false, 'bad_magic'),
        @('only_byte2_wrong', ([byte[]](@(0x47, 0x56, 0x42, 0x53) + (New-TestBytes 96 10))), $false, 'bad_magic'),
        @('exactly_64_bytes_wrong_magic', ([byte[]](@(0x00, 0x00, 0x00, 0x00) + (New-TestBytes 60 11))), $false, 'bad_magic')
    )
    foreach ($c in $savCases) {
        $p = Join-Path $savDir ($c[0] + '.sav')
        Write-TestBytes $p $c[1]
        $r = Test-MMOSavFile -Path $p
        Check 'sav' ($c[0] + '.valid') ($r.Valid -eq $c[2])
        Check 'sav' ($c[0] + '.reason') ($r.Reason -ceq $c[3])
    }

    # ---- 8. JSON butunlugu -------------------------------------------------------------------------------------------
    $jsonDir = Join-Path $fx 'json'
    $nulBytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes('{"version":1}') + [byte[]]@(0)
    $badUtf8 = (New-Object System.Text.UTF8Encoding($false)).GetBytes('{"version":1,"x":"') + [byte[]]@(0xC3, 0x28)
    $truncUtf8 = (New-Object System.Text.UTF8Encoding($false)).GetBytes('{"version":1,"x":"') + [byte[]]@(0xC3)
    $utf16 = [byte[]](@(0xFF, 0xFE) + [System.Text.Encoding]::Unicode.GetBytes('{"version":1}'))
    $jsonCases = @(
        @('good', ([byte[]][char[]]'{"version":1}'), $true, '', 0),
        @('good_v2', ([byte[]][char[]]'{"version":2,"a":[1,2,3]}'), $true, '', 0),
        @('good_whole_float', ([byte[]][char[]]'{"version":1.0}'), $true, '', 0),
        @('turkish_no_bom', ((New-Object System.Text.UTF8Encoding($false)).GetBytes('{"version":1,"n":"' + $trWord + '"}')), $true, '', 0),
        @('bom_warn', ([byte[]](@(0xEF, 0xBB, 0xBF) + [byte[]][char[]]'{"version":1}')), $true, '', 1),
        @('empty', ([byte[]]@()), $false, 'empty', 0),
        @('whitespace', ([byte[]][char[]]"  `r`n"), $false, 'empty', 0),
        @('array_root', ([byte[]][char[]]'[1]'), $false, 'not_object', 0),
        @('string_root', ([byte[]][char[]]'"x"'), $false, 'not_object', 0),
        @('truncated', ([byte[]][char[]]'{"version":1,'), $false, 'parse_error', 0),
        @('no_version', ([byte[]][char[]]'{"a":1}'), $false, 'no_version', 0),
        @('version_string', ([byte[]][char[]]'{"version":"1"}'), $false, 'version_not_integer', 0),
        @('version_float', ([byte[]][char[]]'{"version":1.5}'), $false, 'version_not_integer', 0),
        @('version_bool', ([byte[]][char[]]'{"version":true}'), $false, 'version_not_integer', 0),
        @('version_null', ([byte[]][char[]]'{"version":null}'), $false, 'version_not_integer', 0),
        @('nul_char', $nulBytes, $false, 'nul_char', 0),
        @('invalid_utf8', $badUtf8, $false, 'bad_utf8', 0),
        @('truncated_multibyte', $truncUtf8, $false, 'bad_utf8', 0),
        @('utf16_bom', $utf16, $false, 'bad_utf8', 0)
    )
    foreach ($c in $jsonCases) {
        $p = Join-Path $jsonDir ($c[0] + '.json')
        Write-TestBytes $p ([byte[]]$c[1])
        $r = Test-MMOJsonFile -Path $p
        Check 'json' ($c[0] + '.valid') ($r.Valid -eq $c[2])
        Check 'json' ($c[0] + '.reason') ($r.Reason -ceq $c[3])
        Check 'json' ($c[0] + '.warnings') (@($r.Warnings).Count -eq $c[4])
    }
    Check 'json' 'bom_warning_name' ((@((Test-MMOJsonFile -Path (Join-Path $jsonDir 'bom_warn.json')).Warnings) -join ',') -ceq 'utf8_bom')
    Check 'json' 'missing_file_read_error' ((Test-MMOJsonFile -Path (Join-Path $jsonDir 'nope.json')).Reason -ceq 'read_error')
    # version araligi 1..1000
    $verCases = @(
        @('v0', '{"version":0}', $false, 'version_out_of_range'), @('v1', '{"version":1}', $true, ''), @('v1000', '{"version":1000}', $true, ''),
        @('v1001', '{"version":1001}', $false, 'version_out_of_range'), @('vneg', '{"version":-1}', $false, 'version_out_of_range'),
        @('vhuge', '{"version":1e300}', $false, 'version_out_of_range'), @('v2_0', '{"version":2.0}', $true, '')
    )
    foreach ($c in $verCases) {
        $r = Test-MMOJsonBytes -Bytes ([byte[]][char[]]$c[1])
        Check 'jsonver' ($c[0] + '.valid') ($r.Valid -eq $c[2])
        Check 'jsonver' ($c[0] + '.reason') ($r.Reason -ceq $c[3])
    }
    # dizin (tur) basina sema
    $schemaCases = @(
        @('ach_ok', 'achievement', '{"version":1,"unlocked":[]}', $true, ''), @('ach_missing', 'achievement', '{"version":1}', $false, 'missing_field:unlocked'),
        @('ach_type', 'achievement', '{"version":1,"unlocked":5}', $false, 'bad_field_type:unlocked'),
        @('daily_ok', 'daily', '{"version":1,"day":20261027,"bounties":[1]}', $true, ''), @('daily_no_day', 'daily', '{"version":1,"bounties":[]}', $false, 'missing_field:day'),
        @('daily_day_str', 'daily', '{"version":1,"day":"x","bounties":[]}', $false, 'bad_field_type:day'),
        @('daily_no_bounties', 'daily', '{"version":1,"day":20261027}', $false, 'missing_field:bounties'),
        @('mail_ok', 'mail', '{"version":1,"mails":[]}', $true, ''), @('mail_missing', 'mail', '{"version":1}', $false, 'missing_field:mails'),
        @('mail_obj', 'mail', '{"version":1,"mails":{}}', $false, 'bad_field_type:mails'),
        @('race_ok', 'race', '{"version":1,"race":3}', $true, ''), @('race_missing', 'race', '{"version":1}', $false, 'missing_field:race'),
        @('race_str', 'race', '{"version":1,"race":"3"}', $false, 'bad_field_type:race'),
        @('unknown_kind_only_version', 'weird', '{"version":1}', $true, '')
    )
    foreach ($c in $schemaCases) {
        $r = Test-MMOJsonBytes -Bytes ([byte[]][char[]]$c[2]) -Kind $c[1]
        Check 'jsonschema' ($c[0] + '.valid') ($r.Valid -eq $c[3])
        Check 'jsonschema' ($c[0] + '.reason') ($r.Reason -ceq $c[4])
    }
    Check 'jsonschema' 'class_info_kind_is_used' (-not (Get-MMOBytesIntegrity -Bytes ([byte[]][char[]]'{"version":1}') -ClassInfo (Get-MMOSaveFileClass 'MMOMail/MMOMail_ali.json')).Valid)
    Check 'jsonschema' 'tmp_schema_failure_is_tmp_unparsable' ((Get-MMOBytesIntegrity -Bytes ([byte[]][char[]]'{"version":1}') -ClassInfo (Get-MMOSaveFileClass 'MMOMail/MMOMail_ali.json.tmp')).Notes -ceq 'tmp_unparsable')

    # Sinif bilgisine gore butunluk karari
    $intDir = Join-Path $fx 'integrity'
    Write-TestText (Join-Path $intDir 'garbage.txt') 'not json at all'
    Write-TestText (Join-Path $intDir 'good.txt') '{"version":1,"day":20261027,"bounties":[]}'
    $iLock = Get-MMOFileIntegrity -Path (Join-Path $intDir 'garbage.txt') -ClassInfo (Get-MMOSaveFileClass 'MMORace/MMORace_ali.json.bad')
    Check 'integrity' 'lock_valid_even_if_garbage' ($iLock.Valid -and $iLock.Notes -ceq 'lock_file')
    $iTmp = Get-MMOFileIntegrity -Path (Join-Path $intDir 'garbage.txt') -ClassInfo (Get-MMOSaveFileClass 'MMODaily/MMOSave_Ali.json.tmp')
    Check 'integrity' 'tmp_unparsable_still_valid' ($iTmp.Valid -and $iTmp.Notes -ceq 'tmp_unparsable')
    $iTmpOk = Get-MMOFileIntegrity -Path (Join-Path $intDir 'good.txt') -ClassInfo (Get-MMOSaveFileClass 'MMODaily/MMOSave_Ali.json.tmp')
    Check 'integrity' 'tmp_good_clean' ($iTmpOk.Valid -and $iTmpOk.Notes -ceq '')
    $iJson = Get-MMOFileIntegrity -Path (Join-Path $intDir 'garbage.txt') -ClassInfo (Get-MMOSaveFileClass 'MMODaily/MMOSave_Ali.json')
    Check 'integrity' 'json_garbage_invalid' ((-not $iJson.Valid) -and $iJson.Notes -ceq 'not_object')
    $iBak = Get-MMOFileIntegrity -Path (Join-Path $intDir 'garbage.txt') -ClassInfo (Get-MMOSaveFileClass 'MMOMail/MMOMail_ali.json.bak')
    Check 'integrity' 'bak_garbage_invalid' (-not $iBak.Valid)
    $iSav = Get-MMOFileIntegrity -Path (Join-Path $savDir 'wrong_magic.sav') -ClassInfo (Get-MMOSaveFileClass 'SaveGames/MMOSave_Ali.sav')
    Check 'integrity' 'sav_bad_magic_invalid' ((-not $iSav.Valid) -and $iSav.Notes -ceq 'bad_magic')

    # ---- 9. Guvenli yol cozumleme ve reparse point -------------------------------------------------------------------
    $resRoot = Join-Path $fx 'res_root'
    Write-TestBytes (Join-Path $resRoot 'SaveGames\MMOSave_Ali.sav') (New-SavBytes 100 1)
    $resolved = Resolve-MMOSafePath -Root $resRoot -RelPath 'SaveGames/MMOSave_Ali.sav' -MustExist
    Check 'resolve' 'ok_path' ($resolved.Equals([IO.Path]::GetFullPath((Join-Path $resRoot 'SaveGames\MMOSave_Ali.sav')), [StringComparison]::OrdinalIgnoreCase))
    Check 'resolve' 'ok_new_file_no_mustexist' ((Resolve-MMOSafePath -Root $resRoot -RelPath 'SaveGames/MMOSave_New.sav') -like '*MMOSave_New.sav')
    CheckThrows 'resolve' 'dotdot' { Resolve-MMOSafePath -Root $resRoot -RelPath '../x' } 'unsafe_relpath:dot_segment'
    CheckThrows 'resolve' 'dotdot_mid' { Resolve-MMOSafePath -Root $resRoot -RelPath 'SaveGames/../../x' } 'unsafe_relpath:dot_segment'
    CheckThrows 'resolve' 'absolute' { Resolve-MMOSafePath -Root $resRoot -RelPath '/Windows/x' } 'unsafe_relpath:rooted'
    CheckThrows 'resolve' 'drive' { Resolve-MMOSafePath -Root $resRoot -RelPath 'C:/Windows/x' } 'unsafe_relpath:colon'
    CheckThrows 'resolve' 'backslash' { Resolve-MMOSafePath -Root $resRoot -RelPath 'SaveGames\x' } 'unsafe_relpath:backslash'
    CheckThrows 'resolve' 'device' { Resolve-MMOSafePath -Root $resRoot -RelPath 'SaveGames/NUL' } 'unsafe_relpath:device_name'
    CheckThrows 'resolve' 'missing_mustexist' { Resolve-MMOSafePath -Root $resRoot -RelPath 'SaveGames/MMOSave_No.sav' -MustExist } 'missing'
    CheckThrows 'resolve' 'root_missing' { Resolve-MMOSafePath -Root (Join-Path $fx 'no_such_root') -RelPath 'SaveGames/MMOSave_A.sav' } 'root_missing'
    CheckThrows 'resolve' 'whitelist_required_dir' { Resolve-MMOSafePath -Root $resRoot -RelPath 'Logs/x.log' } 'not_whitelisted:not_whitelisted_dir'
    CheckThrows 'resolve' 'whitelist_required_name' { Resolve-MMOSafePath -Root $resRoot -RelPath 'SaveGames/Other.sav' } 'not_whitelisted:not_whitelisted_name'
    CheckThrows 'resolve' 'whitelist_required_name_rule' { Resolve-MMOSafePath -Root $resRoot -RelPath ('SaveGames/MMOSave_' + $oUml + 'mer.sav') } 'not_whitelisted:name_rule'
    CheckThrows 'resolve' 'whitelist_before_root_check' { Resolve-MMOSafePath -Root (Join-Path $fx 'no_such_root') -RelPath 'a/b' } 'not_whitelisted:'
    CheckThrows 'resolve' 'empty_root_invalid' { Resolve-MMOSafePath -Root '' -RelPath 'SaveGames/MMOSave_A.sav' } 'root_invalid'

    $outside = Join-Path $fx 'outside'
    Write-TestBytes (Join-Path $outside 'MMOSave_Ali.sav') (New-SavBytes 100 8)
    $jSub = Join-Path $fx 'res_junction_sub'
    [void][IO.Directory]::CreateDirectory($jSub)
    New-TestJunction (Join-Path $jSub 'SaveGames') $outside
    $jRoot = Join-Path $fx 'res_junction_root'
    New-TestJunction $jRoot $resRoot
    Check 'reparse' 'junction_detected' (Test-MMOReparse (Join-Path $jSub 'SaveGames'))
    Check 'reparse' 'plain_dir_not_reparse' (-not (Test-MMOReparse $resRoot))
    Check 'reparse' 'plain_file_not_reparse' (-not (Test-MMOReparse (Join-Path $resRoot 'SaveGames\MMOSave_Ali.sav')))
    Check 'reparse' 'missing_not_reparse' (-not (Test-MMOReparse (Join-Path $fx 'definitely_missing')))
    CheckThrows 'reparse' 'resolve_junction_component' { Resolve-MMOSafePath -Root $jSub -RelPath 'SaveGames/MMOSave_Ali.sav' -MustExist } 'reparse_component:SaveGames'
    CheckThrows 'reparse' 'resolve_junction_root' { Resolve-MMOSafePath -Root $jRoot -RelPath 'SaveGames/MMOSave_Ali.sav' } 'reparse_root'
    CheckThrows 'reparse' 'read_bytes_junction_rejected' { Read-MMOFileBytes (Join-Path $jSub 'SaveGames') } 'reparse_file'
    CheckThrows 'reparse' 'tree_junction_entry' { Get-MMOTreeFiles -Root $jSub } 'reparse_entry:SaveGames'
    CheckThrows 'reparse' 'tree_junction_root' { Get-MMOTreeFiles -Root $jRoot } 'reparse_root'
    $symTarget = Join-Path $outside 'MMOSave_Ali.sav'
    $symLink = Join-Path $fx 'sym_file.sav'
    $symInfo = 'not_created'
    try {
        [void](New-Item -ItemType SymbolicLink -Path $symLink -Target $symTarget)
        $symInfo = 'reparse=' + (Test-MMOReparse $symLink)
        [IO.File]::Delete($symLink)
    } catch { $symInfo = 'not_created(privilege)' }
    Write-Host ('[MMO_BACKUP_INFO] symlink_file ' + $symInfo + ' (counted checks use junctions only)')

    # Kok atasinda reparse point: varsayilan ret, bilincli izin parametresiyle kabul
    $ancReal = Join-Path $fx 'anc_real'
    Write-TestBytes (Join-Path $ancReal 'inner\SaveGames\MMOSave_Ali.sav') (New-SavBytes 100 1)
    $ancLink = Join-Path $fx 'anc_link'
    New-TestJunction $ancLink $ancReal
    $ancRoot = Join-Path $ancLink 'inner'
    Check 'ancestor' 'detects_junction_ancestor' ((Get-MMOReparseAncestor $ancRoot) -ceq [IO.Path]::GetFullPath($ancLink))
    Check 'ancestor' 'none_for_plain_tree' ($null -eq (Get-MMOReparseAncestor $resRoot))
    CheckThrows 'ancestor' 'resolve_rejects_by_default' { Resolve-MMOSafePath -Root $ancRoot -RelPath 'SaveGames/MMOSave_Ali.sav' -MustExist } 'reparse_root_ancestor:'
    Check 'ancestor' 'resolve_allowed_with_flag' ((Resolve-MMOSafePath -Root $ancRoot -RelPath 'SaveGames/MMOSave_Ali.sav' -MustExist -AllowRootAncestorReparse) -like '*MMOSave_Ali.sav')
    CheckThrows 'ancestor' 'tree_rejects_by_default' { Get-MMOTreeFiles -Root $ancRoot } 'reparse_root_ancestor:'
    Check 'ancestor' 'tree_allowed_with_flag' ((Get-MMOTreeFiles -Root $ancRoot -AllowRootAncestorReparse).Count -eq 1)
    CheckThrows 'ancestor' 'manifest_create_rejects_by_default' { New-MMOManifest -FilesRoot $ancRoot -SourceSavedDir 'x' } 'reparse_root_ancestor:'
    Check 'ancestor' 'manifest_create_allowed_with_flag' (@((New-MMOManifest -FilesRoot $ancRoot -SourceSavedDir 'x' -AllowRootAncestorReparse).entries).Count -eq 1)

    # Fail-closed: erisilemeyen / gecersiz yol "reparse degil" SAYILMAZ
    $stMissing = Get-MMOAttributes (Join-Path $fx 'definitely_missing')
    $stOk = Get-MMOAttributes $resRoot
    $stBad = Get-MMOAttributes (Join-Path $fx 'bad|name')
    Check 'failclosed' 'missing_state' ($stMissing.State -ceq 'missing')
    Check 'failclosed' 'ok_state' (($stOk.State -ceq 'ok') -and ($null -ne $stOk.Attributes))
    Check 'failclosed' 'invalid_char_state_inaccessible' ($stBad.State -ceq 'inaccessible')
    Check 'failclosed' 'reparse_true_for_inaccessible' (Test-MMOReparse (Join-Path $fx 'bad|name'))
    Check 'failclosed' 'reparse_true_for_empty_path' (Test-MMOReparse '')
    Check 'failclosed' 'reparse_true_for_device_prefix' (Test-MMOReparse '\\.\C:')
    Check 'failclosed' 'reparse_false_for_missing' (-not (Test-MMOReparse (Join-Path $fx 'definitely_missing')))
    CheckThrows 'failclosed' 'resolve_rejects_invalid_root' { Resolve-MMOSafePath -Root (Join-Path $fx 'bad|root') -RelPath 'SaveGames/MMOSave_A.sav' } 'root_invalid'
    CheckThrows 'failclosed' 'read_bytes_rejects_invalid_path' { Read-MMOFileBytes (Join-Path $fx 'bad|file') } 'path_invalid'
    CheckThrows 'failclosed' 'tree_rejects_invalid_root' { Get-MMOTreeFiles -Root (Join-Path $fx 'bad|root') } 'root_invalid'
    CheckThrows 'failclosed' 'tree_missing_root_throws' { Get-MMOTreeFiles -Root (Join-Path $fx 'definitely_missing') } 'root_missing'
    # Listelenemeyen dizin (gercek ACL: kendi hesabina RD reddi, test bitince geri alinir) sessizce atlanmaz
    $aclRoot = Join-Path $fx 'acl_tree'
    Write-TestBytes (Join-Path $aclRoot 'SaveGames\MMOSave_A.sav') (New-SavBytes 100 1)
    $aclDir = Join-Path $aclRoot 'SaveGames'
    $aclMe = $env:USERDOMAIN + '\' + $env:USERNAME
    $aclDenied = $false
    try {
        & icacls.exe $aclDir /deny ($aclMe + ':(RD)') | Out-Null
        $aclDenied = ($LASTEXITCODE -eq 0)
        CheckThrows 'failclosed' 'tree_unreadable_dir_not_skipped' { Get-MMOTreeFiles -Root $aclRoot } 'tree_unreadable'
    } finally {
        & icacls.exe $aclDir /remove:d $aclMe | Out-Null
    }
    Check 'failclosed' 'acl_deny_applied' $aclDenied
    Check 'failclosed' 'tree_readable_after_acl_restore' ((Get-MMOTreeFiles -Root $aclRoot) -join '|' -ceq 'SaveGames/MMOSave_A.sav')
    CheckThrows 'failclosed' 'path_drive_relative_rejected' { ConvertTo-MMOFullPath 'C:foo' } 'path_invalid'
    CheckThrows 'failclosed' 'path_device_prefix_rejected' { ConvertTo-MMOFullPath '\\?\C:\foo' } 'path_invalid'
    CheckThrows 'failclosed' 'path_empty_rejected' { ConvertTo-MMOFullPath '' } 'path_empty'
    CheckThrows 'failclosed' 'path_nul_rejected' { ConvertTo-MMOFullPath ("a" + [char]0 + "b") } 'path_invalid'

    # Goreli yol: PowerShell konumuna gore cozulur, [Environment]::CurrentDirectory'ye degil (ikisi farkli olsa bile ayni dosya)
    $relA = Join-Path $fx 'rel_a'
    $relB = Join-Path $fx 'rel_b'
    Write-TestBytes (Join-Path $relA 'f.bin') ([byte[]][char[]]'AAAA')
    Write-TestBytes (Join-Path $relB 'f.bin') ([byte[]][char[]]'BBBB')
    Write-TestBytes (Join-Path $relA 'root\SaveGames\MMOSave_Rel.sav') (New-SavBytes 100 1)
    $savedCwd = [Environment]::CurrentDirectory
    $savedLoc = Get-Location
    try {
        Set-Location -LiteralPath $relA
        $relAL = (Get-Location -PSProvider FileSystem).ProviderPath      # TEMP 8.3 kisa adi Get-Location'da uzun ad olur
        [Environment]::CurrentDirectory = $relB
        $shaA = Get-OracleSha (Join-Path $relA 'f.bin')
        Check 'relative' 'ps_location_differs_from_process_cwd' ((Get-Location).ProviderPath -ne [Environment]::CurrentDirectory)
        Check 'relative' 'full_path_uses_ps_location' ((ConvertTo-MMOFullPath 'f.bin') -ceq (Join-Path $relAL 'f.bin'))
        Check 'relative' 'sha_uses_ps_location' ((Get-MMOFileSha256 'f.bin') -ceq $shaA)
        Check 'relative' 'read_bytes_uses_ps_location' ([Text.Encoding]::ASCII.GetString((Read-MMOFileBytes 'f.bin')) -ceq 'AAAA')
        Check 'relative' 'attributes_use_ps_location' ((Get-MMOAttributes 'f.bin').State -ceq 'ok')
        Check 'relative' 'tree_relative_root' ((Get-MMOTreeFiles -Root 'root') -join '|' -ceq 'SaveGames/MMOSave_Rel.sav')
        Check 'relative' 'resolve_relative_root' ((Resolve-MMOSafePath -Root 'root' -RelPath 'SaveGames/MMOSave_Rel.sav' -MustExist) -ceq (Join-Path $relAL 'root\SaveGames\MMOSave_Rel.sav'))
        Check 'relative' 'manifest_relative_root' (@((New-MMOManifest -FilesRoot 'root' -SourceSavedDir 'x').entries).Count -eq 1)
        Check 'relative' 'path_under_relative' (Test-MMOPathUnder -Path 'root\SaveGames' -Root 'root')
        Check 'relative' 'relative_dotdot_resolved' ((ConvertTo-MMOFullPath 'root\..\f.bin') -ceq (Join-Path $relAL 'f.bin'))
    }
    finally {
        [Environment]::CurrentDirectory = $savedCwd
        Set-Location -LiteralPath $savedLoc.Path
    }

    # Agac listesi: ordinal sira (bosluk '.' den once)
    $treeRoot = Join-Path $fx 'tree'
    Write-TestBytes (Join-Path $treeRoot 'b\z.txt') ([byte[]]@(1))
    Write-TestBytes (Join-Path $treeRoot 'a.txt') ([byte[]]@(1))
    Write-TestBytes (Join-Path $treeRoot 'a b.txt') ([byte[]]@(1))
    $tree = Get-MMOTreeFiles -Root $treeRoot
    Check 'tree' 'count' ($tree.Count -eq 3)
    Check 'tree' 'ordinal_order' (($tree -join '|') -ceq 'a b.txt|a.txt|b/z.txt')
    Check 'tree' 'forward_slashes' (-not ($tree -join '').Contains('\'))
    Check 'tree' 'empty_dir' ((Get-MMOTreeFiles -Root (New-Item -ItemType Directory -Path (Join-Path $fx 'tree_empty')).FullName).Count -eq 0)

    # ---- 10. Manifest uretme / yazma / okuma -------------------------------------------------------------------------
    $snap = New-TestSnapshot 'base'
    $filesDir = Join-Path $snap 'files'
    $mraw = [IO.File]::ReadAllBytes((Join-Path $snap 'manifest.json'))
    Check 'manifest' 'no_bom' (-not ($mraw.Length -ge 3 -and $mraw[0] -eq 0xEF -and $mraw[1] -eq 0xBB -and $mraw[2] -eq 0xBF))
    Check 'manifest' 'no_tmp_left' (-not [IO.File]::Exists((Join-Path $snap 'manifest.json.tmp')))
    $rd = Read-MMOManifest -SnapshotDir $snap
    Check 'manifest' 'read_ok' ($rd.Ok -and -not $rd.Incomplete -and @($rd.Errors).Count -eq 0)
    $mf = $rd.Manifest
    Check 'manifest' 'format_version' ($mf.format_version -eq 1)
    Check 'manifest' 'mode' ($mf.mode -ceq 'quiesced')
    Check 'manifest' 'tool_present' (($mf.tool -is [string]) -and $mf.tool.Length -gt 0)
    Check 'manifest' 'source_dir_non_ascii_roundtrip' ($mf.source_saved_dir -ceq (Join-Path $fx ('saved_' + $sCed)))
    Check 'manifest' 'shutdown_proof_null' ($null -eq $mf.shutdown_proof)
    Check 'manifest' 'entry_count' (@($mf.entries).Count -eq 12)
    $nChar = @($mf.entries | Where-Object { $_.class -ceq 'character' }).Count
    $nSide = @($mf.entries | Where-Object { $_.class -ceq 'side' }).Count
    $nLock = @($mf.entries | Where-Object { $_.class -ceq 'side-lock' }).Count
    Check 'manifest' 'class_counts' (($nChar -eq 3) -and ($nSide -eq 7) -and ($nLock -eq 2))
    $validFalse = @($mf.entries | Where-Object { $_.valid -eq $false })
    Check 'manifest' 'one_invalid' ($validFalse.Count -eq 1)
    Check 'manifest' 'invalid_is_bad_sav' (($validFalse.Count -eq 1) -and $validFalse[0].path -ceq 'SaveGames/MMOSave_Bad.sav' -and $validFalse[0].notes -ceq 'bad_magic')
    $tmpEntry = @($mf.entries | Where-Object { $_.path -ceq 'MMODaily/MMOSave_Ali.json.tmp' })
    Check 'manifest' 'tmp_entry_note' (($tmpEntry.Count -eq 1) -and $tmpEntry[0].valid -and $tmpEntry[0].notes -ceq 'tmp_unparsable')
    $lockEntry = @($mf.entries | Where-Object { $_.path -ceq 'MMORace/MMORace_mmosave_ali.json.bad' })
    Check 'manifest' 'race_bad_kept_as_lock' (($lockEntry.Count -eq 1) -and $lockEntry[0].class -ceq 'side-lock' -and $lockEntry[0].notes -ceq 'lock_file')
    $shaOk = $true
    $sizeOk = $true
    foreach ($e in $mf.entries) {
        $full = Join-Path $filesDir ($e.path.Replace('/', '\'))
        if ($e.sha256 -cne (Get-OracleSha $full)) { $shaOk = $false }
        if ([long]$e.size -ne (New-Object IO.FileInfo $full).Length) { $sizeOk = $false }
    }
    Check 'manifest' 'entry_hashes_match_oracle' $shaOk
    Check 'manifest' 'entry_sizes_match' $sizeOk
    $paths = @($mf.entries | ForEach-Object { $_.path })
    $sortedPaths = [string[]]$paths.Clone()
    [Array]::Sort($sortedPaths, [StringComparer]::Ordinal)
    Check 'manifest' 'entries_ordinal_sorted' (($paths -join '|') -ceq ($sortedPaths -join '|'))
    Check 'manifest' 'space_before_dot' ($paths.IndexOf('SaveGames/MMOSave_Ali Baba.sav') -lt $paths.IndexOf('SaveGames/MMOSave_Ali.sav'))
    Check 'manifest' 'digest_matches_oracle' ($mf.entries_digest -ceq (Get-OracleDigest $mf.entries))
    $fresh = New-MMOManifest -FilesRoot $filesDir -SourceSavedDir 'x' -CreatedUtc '2026-10-27T10:00:00Z'
    Check 'manifest' 'digest_deterministic' ($fresh.entries_digest -ceq $mf.entries_digest)
    $swapped = @($mf.entries)[0..1]
    Check 'manifest' 'digest_order_independent' ((Get-MMOEntriesDigest @($swapped[1], $swapped[0])) -ceq (Get-MMOEntriesDigest @($swapped[0], $swapped[1])))
    CheckThrows 'manifest' 'write_refuses_overwrite' { Write-MMOManifest -Manifest $fresh -SnapshotDir $snap } 'manifest_exists'
    $ts = Test-MMOSnapshot -SnapshotDir $snap
    Check 'snapshot' 'base_ok' ($ts.Ok -and -not $ts.Incomplete -and @($ts.Errors).Count -eq 0 -and @($ts.Extra).Count -eq 0)
    Check 'snapshot' 'base_invalid_entries' ((@($ts.InvalidEntries) -join '|') -ceq 'SaveGames/MMOSave_Bad.sav')
    Check 'manifest' 'header_digest_present' (($mf.header_digest -is [string]) -and ($mf.header_digest -cmatch '^[0-9a-f]{64}\z') -and ($mf.header_digest -ceq (Get-MMOHeaderDigest $mf)))
    Check 'manifest' 'header_digest_independent_of_entries' ((Get-MMOHeaderDigest $mf) -cne $mf.entries_digest)

    # Manifest yazimi: CreateNew .tmp, reparse/eksik goruntu dizini, boyut siniri
    $wDir = Join-Path $fx 'write_tmp_exists'
    [void][IO.Directory]::CreateDirectory($wDir)
    Write-TestText (Join-Path $wDir 'manifest.json.tmp') 'stale'
    CheckThrows 'write' 'stale_tmp_not_overwritten' { Write-MMOManifest -Manifest $fresh -SnapshotDir $wDir } 'manifest_tmp_exists'
    Check 'write' 'stale_tmp_content_untouched' ((Get-Content -LiteralPath (Join-Path $wDir 'manifest.json.tmp') -Raw) -ceq 'stale')
    Check 'write' 'stale_tmp_no_manifest_created' (-not [IO.File]::Exists((Join-Path $wDir 'manifest.json')))
    CheckThrows 'write' 'missing_snapshot_dir' { Write-MMOManifest -Manifest $fresh -SnapshotDir (Join-Path $fx 'write_no_such_dir') } 'snapshot_missing'
    $wReal = Join-Path $fx 'write_real'
    [void][IO.Directory]::CreateDirectory($wReal)
    $wLink = Join-Path $fx 'write_link'
    New-TestJunction $wLink $wReal
    CheckThrows 'write' 'junction_snapshot_dir_rejected' { Write-MMOManifest -Manifest $fresh -SnapshotDir $wLink } 'reparse_snapshot'
    Check 'write' 'junction_target_untouched' (@(Get-ChildItem -LiteralPath $wReal -Force).Count -eq 0)
    $wAnc = Join-Path $wLink 'sub'
    [void][IO.Directory]::CreateDirectory((Join-Path $wReal 'sub'))
    CheckThrows 'write' 'junction_ancestor_rejected' { Write-MMOManifest -Manifest $fresh -SnapshotDir $wAnc } 'reparse_snapshot_ancestor:'
    Write-MMOManifest -Manifest $fresh -SnapshotDir $wAnc -AllowRootAncestorReparse
    Check 'write' 'junction_ancestor_allowed_with_flag' ([IO.File]::Exists((Join-Path $wReal 'sub\manifest.json')) -and -not [IO.File]::Exists((Join-Path $wReal 'sub\manifest.json.tmp')))
    $wRelDir = Join-Path $fx 'write_rel'
    [void][IO.Directory]::CreateDirectory($wRelDir)
    $bigM = [ordered]@{ a = ('x' * 8400000) }
    CheckThrows 'write' 'oversize_manifest_rejected' { Write-MMOManifest -Manifest $bigM -SnapshotDir $wRelDir } 'manifest_too_large'
    Check 'write' 'oversize_leaves_no_files' (@(Get-ChildItem -LiteralPath $wRelDir -Force).Count -eq 0)
    $wOk = Join-Path $fx 'write_ok'
    [void][IO.Directory]::CreateDirectory($wOk)
    Write-MMOManifest -Manifest $fresh -SnapshotDir $wOk
    Check 'write' 'normal_write_roundtrip' ((Read-MMOManifest -SnapshotDir $wOk).Ok -and (@(Get-ChildItem -LiteralPath $wOk -Force).Count -eq 1))
    $sMan = New-TestSnapshot 'manifest_oversize'
    [IO.File]::WriteAllBytes((Join-Path $sMan 'manifest.json'), (New-Object byte[] 8388609))
    $rdBig = Read-MMOManifest -SnapshotDir $sMan
    Check 'write' 'read_oversize_manifest_rejected' ((-not $rdBig.Ok) -and (-not $rdBig.Incomplete) -and (HasErr $rdBig.Errors 'manifest_too_large'))
    Check 'write' 'snapshot_oversize_manifest_rejected' (-not (Test-MMOSnapshot -SnapshotDir $sMan).Ok)

    # Cakisma kapisi: Ali Baba / Ali_Baba ayni kanonik dosya
    $colDir = Join-Path $fx 'collide_files'
    Write-TestBytes (Join-Path $colDir 'SaveGames\MMOSave_Ali Baba.sav') (New-SavBytes 100 1)
    Write-TestBytes (Join-Path $colDir 'SaveGames\MMOSave_Ali_Baba.sav') (New-SavBytes 100 2)
    CheckThrows 'collision' 'create_rejects_ali_baba_pair' { New-MMOManifest -FilesRoot $colDir -SourceSavedDir 'x' } 'collision:'
    $colDir2 = Join-Path $fx 'collide_case'
    Write-TestBytes (Join-Path $colDir2 'MMOMail\MMOMail_ali.json') ([byte[]][char[]]'{"version":1,"mails":[]}')
    Write-TestBytes (Join-Path $colDir2 'MMOMail\MMOMail_ali.json.bak') ([byte[]][char[]]'{"version":1,"mails":[]}')
    Check 'collision' 'different_suffix_not_collision' (@((New-MMOManifest -FilesRoot $colDir2 -SourceSavedDir 'x').entries).Count -eq 2)
    $colDir3 = Join-Path $fx 'collide_race_forms'
    Write-TestBytes (Join-Path $colDir3 'MMORace\MMORace_ali.json') ([byte[]][char[]]'{"version":1,"race":1}')
    Write-TestBytes (Join-Path $colDir3 'MMORace\MMORace_mmosave_ali.json') ([byte[]][char[]]'{"version":1,"race":1}')
    CheckThrows 'collision' 'create_rejects_race_legacy_and_real_form' { New-MMOManifest -FilesRoot $colDir3 -SourceSavedDir 'x' } 'collision:'

    # 32 karakterlik ad: butun turler kabul (ignored/name_rule olmaz)
    $n32 = 'a' * 32
    $longDir = Join-Path $fx 'long_name_files'
    Write-TestBytes (Join-Path $longDir ('SaveGames\MMOSave_' + $n32 + '.sav')) (New-SavBytes 100 1)
    Write-TestText (Join-Path $longDir ('MMOAchievements\MMOSave_' + $n32 + '.json')) '{"version":1,"unlocked":[]}'
    Write-TestText (Join-Path $longDir ('MMODaily\MMOSave_' + $n32 + '.json')) '{"version":1,"day":20261027,"bounties":[]}'
    Write-TestText (Join-Path $longDir ('MMOMail\MMOMail_' + $n32 + '.json')) '{"version":1,"mails":[]}'
    Write-TestText (Join-Path $longDir ('MMORace\MMORace_mmosave_' + $n32 + '.json')) '{"version":1,"race":2}'
    Write-TestText (Join-Path $longDir ('MMORace\MMORace_mmosave_' + $n32 + '.json.bad')) 'lock'
    $longM = New-MMOManifest -FilesRoot $longDir -SourceSavedDir 'x'
    Check 'longname' 'six_entries_accepted' (@($longM.entries).Count -eq 6)
    Check 'longname' 'all_valid' (@($longM.entries | Where-Object { -not $_.valid }).Count -eq 0)
    Check 'longname' 'race_classified' (@($longM.entries | Where-Object { $_.path -like 'MMORace/*' }).Count -eq 2)
    $longSnap = Join-Path $fx 'long_name_snap'
    [void][IO.Directory]::CreateDirectory($longSnap)
    Copy-Item -LiteralPath $longDir -Destination (Join-Path $longSnap 'files') -Recurse
    Write-MMOManifest -Manifest $longM -SnapshotDir $longSnap
    Check 'longname' 'snapshot_ok' ((Test-MMOSnapshot -SnapshotDir $longSnap).Ok)

    $proof = [ordered]@{ run_id = '20261027T101010Z'; exit_code = 0; time_utc = '2026-10-27T10:10:10Z' }
    $snapLive = New-TestSnapshot 'live_proof' $proof 'live'
    $rdLive = Read-MMOManifest -SnapshotDir $snapLive
    Check 'manifest' 'live_mode_ok' ($rdLive.Ok -and $rdLive.Manifest.mode -ceq 'live')
    Check 'manifest' 'proof_roundtrip' ($rdLive.Ok -and $rdLive.Manifest.shutdown_proof.run_id -ceq '20261027T101010Z' -and $rdLive.Manifest.shutdown_proof.exit_code -eq 0)

    # Beyaz liste disi / ASCII disi dosya manifest uretimini reddeder
    $badFiles = Join-Path $fx 'badfiles_nonwl'
    Write-TestBytes (Join-Path $badFiles 'SaveGames\Other.sav') (New-SavBytes 100 1)
    CheckThrows 'manifest' 'create_rejects_non_whitelisted' { New-MMOManifest -FilesRoot $badFiles -SourceSavedDir 'x' } 'not_whitelisted:SaveGames/Other.sav'
    $badFiles2 = Join-Path $fx 'badfiles_nonascii'
    Write-TestBytes (Join-Path $badFiles2 ('SaveGames\MMOSave_' + $oUml + 'mer.sav')) (New-SavBytes 100 1)
    CheckThrows 'manifest' 'create_rejects_non_ascii_name' { New-MMOManifest -FilesRoot $badFiles2 -SourceSavedDir 'x' } 'not_whitelisted:'
    CheckThrows 'manifest' 'create_rejects_junction' { New-MMOManifest -FilesRoot $jSub -SourceSavedDir 'x' } 'reparse_entry:'

    # ---- 11. Anlik goruntu dogrulama: kurcalama senaryolari ----------------------------------------------------------
    # 11a dosya icerigi ayni boyutta degisti (yalniz hash yakalar)
    $s = New-TestSnapshot 'flip'
    $victim = Join-Path $s 'files\SaveGames\MMOSave_Ali.sav'
    $vb = [IO.File]::ReadAllBytes($victim)
    $vb[150] = [byte](($vb[150] + 1) % 256)
    [IO.File]::WriteAllBytes($victim, $vb)
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'same_size_flip_not_ok' (-not $t.Ok)
    Check 'tamper' 'same_size_flip_hash_mismatch' (HasErr $t.Errors 'hash_mismatch:SaveGames/MMOSave_Ali.sav')
    Check 'tamper' 'same_size_flip_no_size_error' (-not (HasErr $t.Errors 'size_mismatch:'))
    # 11b dosya uzadi
    $s = New-TestSnapshot 'grow'
    $fs = [IO.File]::Open((Join-Path $s 'files\SaveGames\MMOSave_Ali.sav'), [IO.FileMode]::Append)
    try { $fs.WriteByte(7) } finally { $fs.Dispose() }
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'grown_size_mismatch' ((-not $t.Ok) -and (HasErr $t.Errors 'size_mismatch:SaveGames/MMOSave_Ali.sav'))
    # 11c dosya silindi
    $s = New-TestSnapshot 'deleted'
    [IO.File]::Delete((Join-Path $s 'files\MMOMail\MMOMail_ali.json'))
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'deleted_entry_unusable' ((-not $t.Ok) -and (HasErr $t.Errors 'entry_unusable:MMOMail/MMOMail_ali.json:missing'))
    # 11d fazladan dosyalar (karisik goruntu)
    $s = New-TestSnapshot 'extra_wl'
    Write-TestBytes (Join-Path $s 'files\SaveGames\MMOSave_Zed.sav') (New-SavBytes 100 1)
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'extra_whitelisted' ((-not $t.Ok) -and (@($t.Extra) -contains 'SaveGames/MMOSave_Zed.sav'))
    $s = New-TestSnapshot 'extra_other'
    Write-TestText (Join-Path $s 'files\Logs\x.log') 'log'
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'extra_non_whitelisted' ((-not $t.Ok) -and (@($t.Extra) -contains 'Logs/x.log'))
    $s = New-TestSnapshot 'extra_partial'
    Write-TestBytes (Join-Path $s 'files\SaveGames\MMOSave_Ali.sav.partial') (New-SavBytes 100 1)
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'extra_partial_leftover' ((-not $t.Ok) -and (@($t.Extra) -contains 'SaveGames/MMOSave_Ali.sav.partial'))
    $s = New-TestSnapshot 'extra_nonascii'
    Write-TestBytes (Join-Path $s ('files\SaveGames\MMOSave_' + $oUml + 'mer.sav')) (New-SavBytes 100 1)
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'extra_non_ascii_name' ((-not $t.Ok) -and (@($t.Extra) -contains ('SaveGames/MMOSave_' + $oUml + 'mer.sav')))
    # 11e manifest yok = yarim goruntu
    $s = New-TestSnapshot 'nomanifest'
    [IO.File]::Delete((Join-Path $s 'manifest.json'))
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'no_manifest_incomplete' ((-not $t.Ok) -and $t.Incomplete -and (HasErr $t.Errors 'manifest_missing'))
    $rdm = Read-MMOManifest -SnapshotDir $s
    Check 'tamper' 'no_manifest_read_incomplete' ((-not $rdm.Ok) -and $rdm.Incomplete)
    $s = New-TestSnapshot 'tmp_only'
    [IO.File]::Move((Join-Path $s 'manifest.json'), (Join-Path $s 'manifest.json.tmp'))
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'manifest_tmp_only_incomplete' ((-not $t.Ok) -and $t.Incomplete)
    # 11f yalniz entries_digest degisti (dosyalar ve girisler saglam)
    $s = New-TestSnapshot 'digest_only'
    Edit-TestManifest $s { param($m) $m.entries_digest = ('a' * 64) }
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'digest_only_not_ok' ((-not $t.Ok) -and (HasErr $t.Errors 'digest_mismatch'))
    # 11g manifestteki sha degisti, digest tutarli yeniden hesaplandi (dosya saglam): yalniz dosya hash karsilastirmasi yakalar
    $s = New-TestSnapshot 'consistent_lie'
    Edit-TestManifest $s { param($m) $m.entries[0].sha256 = ('b' * 64) } -FixDigest
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'tamper' 'consistent_lie_manifest_reads_ok' ((Read-MMOManifest -SnapshotDir $s).Ok)
    Check 'tamper' 'consistent_lie_caught_by_hash' ((-not $t.Ok) -and (HasErr $t.Errors 'hash_mismatch:'))
    # 11h manifest sozlesme ihlalleri (digest yeniden hesaplanir: yalniz ilgili kural yakalamali)
    $contract = @(
        @('path_dotdot', { param($m) $m.entries[0].path = '../evil.sav' }, 'entry1:unsafe_path:dot_segment'),
        @('path_absolute', { param($m) $m.entries[0].path = 'C:/Windows/evil.sav' }, 'entry1:unsafe_path:colon'),
        @('path_backslash', { param($m) $m.entries[0].path = 'SaveGames\MMOSave_Ali.sav' }, 'entry1:unsafe_path:backslash'),
        @('path_not_whitelisted', { param($m) $m.entries[0].path = 'SaveGames/Other.sav' }, 'entry1:not_whitelisted:'),
        @('path_non_ascii_name', { param($m) $m.entries[0].path = ('SaveGames/MMOSave_' + $oUml + 'mer.sav') }, 'entry1:not_whitelisted:name_rule'),
        @('path_not_string', { param($m) $m.entries[0].path = 42 }, 'entry1:bad_path'),
        @('class_unknown', { param($m) $m.entries[0].class = 'weird' }, 'entry1:class_mismatch'),
        @('class_swapped', { param($m) $m.entries[0].class = 'character' }, 'entry1:class_mismatch'),
        @('sha_uppercase', { param($m) $m.entries[0].sha256 = ('A' * 64) }, 'entry1:bad_sha256'),
        @('sha_short', { param($m) $m.entries[0].sha256 = 'abc' }, 'entry1:bad_sha256'),
        @('size_negative', { param($m) $m.entries[0].size = -1 }, 'entry1:bad_size'),
        @('size_string', { param($m) $m.entries[0].size = '10' }, 'entry1:bad_size'),
        @('valid_string', { param($m) $m.entries[0].valid = 'yes' }, 'entry1:bad_valid'),
        @('dup_path', { param($m) $m.entries[1].path = $m.entries[0].path }, 'entry2:duplicate_path'),
        @('dup_path_case', { param($m) $m.entries[1].path = $m.entries[0].path.ToUpperInvariant() }, 'entry2:duplicate_path'),
        @('missing_field', { param($m) $m.entries[0].PSObject.Properties.Remove('sha256') }, 'entry1:missing_field:sha256'),
        @('version_2', { param($m) $m.format_version = 2 }, 'bad_format_version'),
        @('mode_unknown', { param($m) $m.mode = 'weird' }, 'bad_mode'),
        @('mode_case', { param($m) $m.mode = 'Live' }, 'bad_mode'),
        @('run_id_bad', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = 'bad id!'; exit_code = 0; time_utc = '2026-10-27T10:10:10Z' } }, 'bad_shutdown_proof'),
        @('run_id_missing', { param($m) $m.shutdown_proof = [pscustomobject]@{ exit_code = 0; time_utc = '2026-10-27T10:10:10Z' } }, 'bad_shutdown_proof'),
        @('run_id_too_long', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = ('1' * 41); exit_code = 0; time_utc = '2026-10-27T10:10:10Z' } }, 'bad_shutdown_proof'),
        @('run_id_not_string', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = 5; exit_code = 0; time_utc = '2026-10-27T10:10:10Z' } }, 'bad_shutdown_proof'),
        @('proof_exit_code_missing', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = '20261027T101010Z'; time_utc = '2026-10-27T10:10:10Z' } }, 'bad_shutdown_proof'),
        @('proof_exit_code_string', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = '20261027T101010Z'; exit_code = '0'; time_utc = '2026-10-27T10:10:10Z' } }, 'bad_shutdown_proof'),
        @('proof_exit_code_negative', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = '20261027T101010Z'; exit_code = -1; time_utc = '2026-10-27T10:10:10Z' } }, 'bad_shutdown_proof'),
        @('proof_exit_code_256', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = '20261027T101010Z'; exit_code = 256; time_utc = '2026-10-27T10:10:10Z' } }, 'bad_shutdown_proof'),
        @('proof_time_missing', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = '20261027T101010Z'; exit_code = 0 } }, 'bad_shutdown_proof'),
        @('proof_time_garbage', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = '20261027T101010Z'; exit_code = 0; time_utc = 'yesterday' } }, 'bad_shutdown_proof'),
        @('proof_time_bad_date', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = '20261027T101010Z'; exit_code = 0; time_utc = '2026-13-45T10:10:10Z' } }, 'bad_shutdown_proof'),
        @('proof_not_object', { param($m) $m.shutdown_proof = 'ok' }, 'bad_shutdown_proof'),
        @('created_utc_garbage', { param($m) $m.created_utc = 'today' }, 'bad_created_utc'),
        @('created_utc_no_z', { param($m) $m.created_utc = '2026-10-27T10:00:00' }, 'bad_created_utc'),
        @('tool_empty', { param($m) $m.tool = '' }, 'bad_tool'),
        @('notes_not_string', { param($m) $m.entries[0].notes = 5 }, 'entry1:bad_notes'),
        @('notes_missing', { param($m) $m.entries[0].PSObject.Properties.Remove('notes') }, 'entry1:missing_field:notes'),
        @('size_over_cap', { param($m) $m.entries[0].size = 8388609 }, 'entry1:bad_size'),
        @('header_digest_removed', { param($m) $m.PSObject.Properties.Remove('header_digest') }, 'missing_field:header_digest'),
        @('collision_ali_baba', { param($m) $e2 = ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali Baba.sav' } | ConvertTo-Json | ConvertFrom-Json); $e2.path = 'SaveGames/MMOSave_Ali_Baba.sav'; $m.entries = @($m.entries) + $e2 }, 'collision:'),
        @('collision_race_forms', { param($m) $e2 = ($m.entries | Where-Object { $_.path -ceq 'MMORace/MMORace_mmosave_ali.json' } | ConvertTo-Json | ConvertFrom-Json); $e2.path = 'MMORace/MMORace_ali.json'; $m.entries = @($m.entries) + $e2 }, 'collision:'),
        @('entries_object', { param($m) $m.entries = [pscustomobject]@{ a = 1 } }, 'entries_not_array'),
        @('field_removed', { param($m) $m.PSObject.Properties.Remove('entries_digest') }, 'missing_field:entries_digest'),
        @('source_dir_empty', { param($m) $m.source_saved_dir = '' }, 'bad_source_saved_dir')
    )
    foreach ($c in $contract) {
        $s = New-TestSnapshot ('c_' + $c[0])
        if (@('missing_field', 'entries_object', 'field_removed', 'notes_missing', 'header_digest_removed') -contains $c[0]) { Edit-TestManifest $s $c[1] }
        else { Edit-TestManifest $s $c[1] -FixDigest }
        $rdc = Read-MMOManifest -SnapshotDir $s
        Check 'contract' ($c[0] + '.rejected') (-not $rdc.Ok)
        Check 'contract' ($c[0] + '.reason') (HasErr $rdc.Errors $c[2])
        Check 'contract' ($c[0] + '.snapshot_rejected') (-not (Test-MMOSnapshot -SnapshotDir $s).Ok)
    }
    # 11h2 valid/notes korumasi: tutarli yalan (digest yeniden hesaplanmis) yalniz Test-MMOSnapshot'in yeniden hesabiyla yakalanir
    $s = New-TestSnapshot 'valid_flip_nodigest'
    Edit-TestManifest $s { param($m) ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Bad.sav' }).valid = $true }
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'validtamper' 'flip_valid_without_digest_fix_caught_by_digest' ((-not $t.Ok) -and (HasErr $t.Errors 'digest_mismatch'))
    $s = New-TestSnapshot 'valid_flip_consistent'
    Edit-TestManifest $s { param($m) ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Bad.sav' }).valid = $true } -FixDigest
    Check 'validtamper' 'consistent_valid_lie_manifest_reads_ok' ((Read-MMOManifest -SnapshotDir $s).Ok)
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'validtamper' 'flip_valid_false_to_true_caught' ((-not $t.Ok) -and (HasErr $t.Errors 'valid_mismatch:SaveGames/MMOSave_Bad.sav'))
    $s = New-TestSnapshot 'valid_flip_true_to_false'
    Edit-TestManifest $s { param($m) ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' }).valid = $false } -FixDigest
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'validtamper' 'flip_valid_true_to_false_caught' ((-not $t.Ok) -and (HasErr $t.Errors 'valid_mismatch:SaveGames/MMOSave_Ali.sav'))
    $s = New-TestSnapshot 'notes_tamper_nodigest'
    Edit-TestManifest $s { param($m) ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Bad.sav' }).notes = 'fine' }
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'validtamper' 'notes_without_digest_fix_caught_by_digest' ((-not $t.Ok) -and (HasErr $t.Errors 'digest_mismatch'))
    $s = New-TestSnapshot 'notes_tamper_consistent'
    Edit-TestManifest $s { param($m) ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Bad.sav' }).notes = 'fine' } -FixDigest
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'validtamper' 'notes_consistent_lie_caught' ((-not $t.Ok) -and (HasErr $t.Errors 'notes_mismatch:SaveGames/MMOSave_Bad.sav'))
    $s = New-TestSnapshot 'class_tamper_consistent'
    Edit-TestManifest $s { param($m) ($m.entries | Where-Object { $_.path -ceq 'MMODaily/MMOSave_Ali.json' }).class = 'character' } -FixDigest
    Check 'validtamper' 'class_swap_rejected_by_contract' (HasErr (Read-MMOManifest -SnapshotDir $s).Errors 'entry')
    # sifir bayt dosya: manifest boyut+hash+valid=true uyduruldu, digest tutarli
    $s = New-TestSnapshot 'zero_byte_lie'
    $zp = Join-Path $s 'files\SaveGames\MMOSave_Ali.sav'
    [IO.File]::WriteAllBytes($zp, [byte[]]@())
    Edit-TestManifest $s { param($m) $e = ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' }); $e.size = 0; $e.sha256 = 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855' } -FixDigest
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'validtamper' 'zero_byte_size_and_hash_consistent' (-not (HasErr $t.Errors 'size_mismatch:') -and -not (HasErr $t.Errors 'hash_mismatch:'))
    Check 'validtamper' 'zero_byte_claimed_valid_caught' ((-not $t.Ok) -and (HasErr $t.Errors 'valid_mismatch:SaveGames/MMOSave_Ali.sav'))
    # GVAS olmayan icerik, manifest valid=true koruyor
    $s = New-TestSnapshot 'bad_gvas_lie'
    $gp = Join-Path $s 'files\SaveGames\MMOSave_Ali.sav'
    $gb = New-TestBytes 100 9
    [IO.File]::WriteAllBytes($gp, $gb)
    $gsha = Get-OracleSha $gp
    Edit-TestManifest $s { param($m) $e = ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' }); $e.size = 100; $e.sha256 = $gsha } -FixDigest
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'validtamper' 'bad_gvas_claimed_valid_caught' ((-not $t.Ok) -and (HasErr $t.Errors 'valid_mismatch:SaveGames/MMOSave_Ali.sav') -and -not (HasErr $t.Errors 'hash_mismatch:'))
    Check 'validtamper' 'bad_gvas_listed_invalid' (@($t.InvalidEntries) -contains 'SaveGames/MMOSave_Ali.sav')
    # JSON yan dosya bozuldu (gecerli -> semasiz), manifest valid=true koruyor
    $s = New-TestSnapshot 'schema_lie'
    $jp = Join-Path $s 'files\MMOMail\MMOMail_ali.json'
    Write-TestText $jp '{"version":1}'
    $jsha = Get-OracleSha $jp
    $jsz = (New-Object IO.FileInfo $jp).Length
    Edit-TestManifest $s { param($m) $e = ($m.entries | Where-Object { $_.path -ceq 'MMOMail/MMOMail_ali.json' }); $e.size = $jsz; $e.sha256 = $jsha } -FixDigest
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'validtamper' 'schema_violation_claimed_valid_caught' ((-not $t.Ok) -and (HasErr $t.Errors 'valid_mismatch:MMOMail/MMOMail_ali.json'))

    # 11h3 ust duzey alan (header) korumasi: digest yeniden hesaplanMADAN degistirilen alan header_digest_mismatch verir
    $hdr = @(
        @('mode', { param($m) $m.mode = 'live' }), @('source_dir', { param($m) $m.source_saved_dir = 'D:\elsewhere' }),
        @('created_utc', { param($m) $m.created_utc = '2020-01-01T00:00:00Z' }), @('tool', { param($m) $m.tool = 'other/1' }),
        @('proof_added', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = '20261027T101010Z'; exit_code = 0; time_utc = '2026-10-27T10:10:10Z' } })
    )
    foreach ($c in $hdr) {
        $s = New-TestSnapshot ('h_' + $c[0])
        Edit-TestManifest $s $c[1]
        $rdh = Read-MMOManifest -SnapshotDir $s
        Check 'headertamper' ($c[0] + '.rejected') ((-not $rdh.Ok) -and (HasErr $rdh.Errors 'header_digest_mismatch'))
        Check 'headertamper' ($c[0] + '.entries_digest_still_ok') (-not (HasErr $rdh.Errors 'digest_mismatch'))
        Check 'headertamper' ($c[0] + '.snapshot_rejected') (-not (Test-MMOSnapshot -SnapshotDir $s).Ok)
    }
    $s = New-TestSnapshot 'h_proof_edit' $proof 'live'
    Edit-TestManifest $s { param($m) $m.shutdown_proof.exit_code = 1 }
    Check 'headertamper' 'proof_exit_code_change.rejected' (HasErr (Read-MMOManifest -SnapshotDir $s).Errors 'header_digest_mismatch')
    $s = New-TestSnapshot 'h_proof_removed' $proof 'live'
    Edit-TestManifest $s { param($m) $m.shutdown_proof = $null }
    Check 'headertamper' 'proof_removed.rejected' (HasErr (Read-MMOManifest -SnapshotDir $s).Errors 'header_digest_mismatch')
    $s = New-TestSnapshot 'h_digest_value'
    Edit-TestManifest $s { param($m) $m.header_digest = ('c' * 64) }
    Check 'headertamper' 'header_digest_value_rejected' (HasErr (Read-MMOManifest -SnapshotDir $s).Errors 'header_digest_mismatch')
    $s = New-TestSnapshot 'h_fixed_consistent'
    Edit-TestManifest $s { param($m) $m.mode = 'live' } -FixDigest
    Check 'headertamper' 'consistent_header_edit_reads_ok' ((Read-MMOManifest -SnapshotDir $s).Ok)

    # 11h4 geri yukleme politikasi: yan kilit/.tmp/.bak/.bad varsayilan kapali, -IncludeSideFiles ile acik
    $selAll = (Read-MMOManifest -SnapshotDir $snap).Manifest.entries
    $selDef = Get-MMORestoreSelection -Entries $selAll
    $selOn = Get-MMORestoreSelection -Entries $selAll -IncludeSideFiles
    # O6: manifestteki valid=false giris (Bad.sav) varsayilan olarak da geri yuklenmez; yan dosya bayragi bunu degistirmez
    Check 'policy' 'default_restores_main_files_only' ($selDef.Restore.Count -eq 7)
    Check 'policy' 'default_skips_side_files' ($selDef.Skipped.Count -eq 5)
    Check 'policy' 'default_skip_reason' ((@($selDef.Skipped | Where-Object { $_.Reason -ceq 'side_file_requires_flag' }).Count -eq 4))
    Check 'policy' 'default_skipped_names' ((@($selDef.Skipped | ForEach-Object { $_.Path } | Sort-Object) -join '|') -ceq 'MMOAchievements/MMOSave_Ali.json.bad|MMODaily/MMOSave_Ali.json.tmp|MMOMail/MMOMail_ali.json.bak|MMORace/MMORace_mmosave_ali.json.bad|SaveGames/MMOSave_Bad.sav')
    Check 'policy' 'default_no_suffix_in_restore' (@($selDef.Restore | Where-Object { $_ -match '\.(tmp|bak|bad)$' }).Count -eq 0)
    Check 'policy' 'default_keeps_valid_sav_only' ((@($selDef.Restore | Where-Object { $_ -like 'SaveGames/*' }).Count -eq 2) -and (@($selDef.Restore | Where-Object { $_ -ceq 'SaveGames/MMOSave_Bad.sav' }).Count -eq 0))
    Check 'policy' 'default_invalid_entry_reason' ((@($selDef.Skipped | Where-Object { $_.Reason -ceq 'invalid_entry' -and $_.Path -ceq 'SaveGames/MMOSave_Bad.sav' }).Count -eq 1) -and ($selDef.Forced.Count -eq 0))
    Check 'policy' 'flag_restores_everything_valid' (($selOn.Restore.Count -eq 11) -and ($selOn.Skipped.Count -eq 1) -and ($selOn.Skipped[0].Reason -ceq 'invalid_entry'))
    $selForce = Get-MMORestoreSelection -Entries $selAll -IncludeSideFiles -IncludeInvalid
    Check 'policy' 'include_invalid_flag_restores_everything_and_logs_forced' (($selForce.Restore.Count -eq 12) -and ($selForce.Skipped.Count -eq 0) -and ($selForce.Forced.Count -eq 1) -and ($selForce.Forced[0].Path -ceq 'SaveGames/MMOSave_Bad.sav'))
    $selEnt = @(
        [pscustomobject]@{ path = 'SaveGames/MMOSave_Ali.sav'; valid = $true; notes = '' },
        [pscustomobject]@{ path = 'SaveGames/MMOSave_Zed.sav'; valid = $false; notes = 'unstable' },
        [pscustomobject]@{ path = 'SaveGames/MMOSave_Big.sav'; valid = $false; notes = 'too_large' },
        [pscustomobject]@{ path = 'SaveGames/MMOSave_Ali Baba.sav'; valid = $true; notes = '' }
    )
    $selU = Get-MMORestoreSelection -Entries $selEnt
    Check 'policy' 'unstable_entry_skipped_by_default' ((@($selU.Skipped | Where-Object { $_.Path -ceq 'SaveGames/MMOSave_Zed.sav' -and $_.Reason -ceq 'unstable' }).Count -eq 1) -and ($selU.Restore.Count -eq 2))
    Check 'policy' 'unreadable_entry_has_no_content_reason' (@($selU.Skipped | Where-Object { $_.Path -ceq 'SaveGames/MMOSave_Big.sav' -and $_.Reason -ceq 'no_content' }).Count -eq 1)
    $selUF = Get-MMORestoreSelection -Entries $selEnt -IncludeInvalid
    Check 'policy' 'unstable_restored_only_with_explicit_flag' (($selUF.Restore -contains 'SaveGames/MMOSave_Zed.sav') -and ($selUF.Forced.Count -eq 1))
    Check 'policy' 'unreadable_entry_never_restorable_even_with_flag' (($selUF.Restore -notcontains 'SaveGames/MMOSave_Big.sav') -and (@($selUF.Skipped | Where-Object { $_.Reason -ceq 'no_content' }).Count -eq 1))
    $selC = Get-MMORestoreSelection -Entries $selEnt -CollisionPaths @('SaveGames/MMOSave_Ali.sav', 'SaveGames/MMOSave_Ali Baba.sav')
    Check 'policy' 'collision_paths_skipped_even_when_valid' ((@($selC.Skipped | Where-Object { $_.Reason -ceq 'collision' }).Count -eq 2) -and ($selC.Restore.Count -eq 0))
    $selBad = Get-MMORestoreSelection -Entries @([pscustomobject]@{ path = 'Logs/x.log' }, [pscustomobject]@{ path = '../x' }) -IncludeSideFiles
    Check 'policy' 'non_whitelisted_always_skipped' (($selBad.Restore.Count -eq 0) -and ($selBad.Skipped.Count -eq 2) -and ($selBad.Skipped[0].Reason -ceq 'not_whitelisted'))

    # 11i manifest dosyasi bozuk
    $manifestBad = @(
        @('empty', ([byte[]]@())), @('garbage', ([byte[]][char[]]'not json')), @('truncated', ([byte[]][char[]]'{"format_version":1,')),
        @('array', ([byte[]][char[]]'[1,2]')), @('invalid_utf8', ([byte[]]@(0x7B, 0xC3, 0x28, 0x7D)))
    )
    foreach ($c in $manifestBad) {
        $s = New-TestSnapshot ('mb_' + $c[0])
        [IO.File]::WriteAllBytes((Join-Path $s 'manifest.json'), [byte[]]$c[1])
        $rdc = Read-MMOManifest -SnapshotDir $s
        Check 'manifestbad' ($c[0] + '.rejected') ((-not $rdc.Ok) -and -not $rdc.Incomplete)
    }
    # 11j junction ile saldiri
    $s = New-TestSnapshot 'junction_dir'
    $decoy = Join-Path $fx 'decoy_savegames'
    [void][IO.Directory]::CreateDirectory($decoy)
    Copy-Item -Path (Join-Path $s 'files\SaveGames\*') -Destination $decoy
    Remove-Item -LiteralPath (Join-Path $s 'files\SaveGames') -Recurse -Force
    New-TestJunction (Join-Path $s 'files\SaveGames') $decoy
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'junction' 'snapshot_subdir_junction_rejected' (-not $t.Ok)
    Check 'junction' 'snapshot_subdir_junction_reason' ((HasErr $t.Errors 'entry_unusable:') -and (HasErr $t.Errors 'tree_unusable:reparse_entry:SaveGames'))
    $s = New-TestSnapshot 'junction_files'
    $realFiles = Join-Path $fx 'moved_files'
    [IO.Directory]::Move((Join-Path $s 'files'), $realFiles)
    New-TestJunction (Join-Path $s 'files') $realFiles
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'junction' 'snapshot_files_junction_rejected' ((-not $t.Ok) -and (HasErr $t.Errors 'entry_unusable:') -and (HasErr $t.Errors 'tree_unusable:reparse_root'))
    $s = New-TestSnapshot 'junction_manifest_dir'
    $realSnap = Join-Path $fx 'moved_snap'
    [IO.Directory]::Move($s, $realSnap)
    New-TestJunction $s $realSnap
    $t = Test-MMOSnapshot -SnapshotDir $s
    Check 'junction' 'snapshot_dir_junction_rejected' ((-not $t.Ok) -and (HasErr $t.Errors 'reparse_snapshot'))

    # ---- 12. Digest kapsami ve olcek ---------------------------------------------------------------------------------
    $dg = @()
    for ($k = 0; $k -lt 200; $k++) {
        $dg += [pscustomobject]@{ path = ('SaveGames/MMOSave_P' + ($k * 7919 % 1000).ToString('0000') + '.sav'); class = 'character'; size = (100 + $k); sha256 = ('{0:x64}' -f $k); valid = ($k % 3 -ne 0); notes = ('n' + ($k % 5)) }
    }
    # Ordinal ile kultur siralamasinin AYRILDIGI yollar (buyuk/kucuk harf, '-' / '_' / bosluk / '.')
    foreach ($odd in @('SaveGames/MMOSave_a.sav', 'SaveGames/MMOSave_B.sav', 'SaveGames/MMOSave_a-b.sav', 'SaveGames/MMOSave_ab.sav', 'SaveGames/MMOSave_A_b.sav', 'MMOMail/MMOMail_ali-x.json', 'MMOMail/MMOMail_ali_baba.json', 'MMOMail/MMOMail_ali.json', 'MMOMail/MMOMail_alix.json')) {
        $dg += [pscustomobject]@{ path = $odd; class = 'side'; size = 10; sha256 = ('e' * 64); valid = $true; notes = '' }
    }
    $dgBase = Get-MMOEntriesDigest $dg
    Check 'digest' 'matches_oracle_200' ($dgBase -ceq (Get-OracleDigest $dg))
    $dgRev = @($dg); [Array]::Reverse($dgRev)
    Check 'digest' 'order_independent_200' ((Get-MMOEntriesDigest $dgRev) -ceq $dgBase)
    $dgCopy = $dg | ForEach-Object { $_.PSObject.Copy() }
    Check 'digest' 'copy_same' ((Get-MMOEntriesDigest $dgCopy) -ceq $dgBase)
    $dgCopy[5].valid = (-not $dgCopy[5].valid)
    Check 'digest' 'valid_flip_changes_digest' ((Get-MMOEntriesDigest $dgCopy) -cne $dgBase)
    $dgCopy = $dg | ForEach-Object { $_.PSObject.Copy() }; $dgCopy[5].notes = 'tampered'
    Check 'digest' 'notes_change_changes_digest' ((Get-MMOEntriesDigest $dgCopy) -cne $dgBase)
    $dgCopy = $dg | ForEach-Object { $_.PSObject.Copy() }; $dgCopy[5].class = 'side'
    Check 'digest' 'class_change_changes_digest' ((Get-MMOEntriesDigest $dgCopy) -cne $dgBase)
    $dgCopy = $dg | ForEach-Object { $_.PSObject.Copy() }; $dgCopy[5].size = $dgCopy[5].size + 1
    Check 'digest' 'size_change_changes_digest' ((Get-MMOEntriesDigest $dgCopy) -cne $dgBase)
    $dgCopy = $dg | ForEach-Object { $_.PSObject.Copy() }; $dgCopy[5].sha256 = ('f' * 64)
    Check 'digest' 'sha_change_changes_digest' ((Get-MMOEntriesDigest $dgCopy) -cne $dgBase)
    Check 'digest' 'empty_input_defined' ((Get-MMOEntriesDigest @()) -ceq 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855')
    $big = New-Object System.Collections.ArrayList
    for ($k = 0; $k -lt 10000; $k++) {
        [void]$big.Add([pscustomobject]@{ path = ('SaveGames/MMOSave_Q' + (($k * 7919) % 10007).ToString('00000') + '.sav'); class = 'character'; size = (100 + $k); sha256 = ('{0:x64}' -f $k); valid = ($k % 7 -ne 0); notes = '' })
    }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $bigDigest = Get-MMOEntriesDigest $big
    $sw.Stop()
    Write-Host ('[MMO_BACKUP_INFO] digest_10000_entries_ms=' + $sw.ElapsedMilliseconds)
    Check 'digest' 'ten_thousand_entries_under_5s' (($sw.ElapsedMilliseconds -lt 5000) -and ($bigDigest -cmatch '^[0-9a-f]{64}\z'))
    $bigRev = $big.ToArray(); [Array]::Reverse($bigRev)
    Check 'digest' 'ten_thousand_order_independent' ((Get-MMOEntriesDigest $bigRev) -ceq $bigDigest)

    # ---- 15. B1: kutuphane eklemeleri (kararsiz dosya, surec/kapanis okuyucular) -------------------------------------------
    $pTable = @(
        @('UnrealEditor.exe', '', $true), @('UnrealEditor-Cmd.exe', '', $true), @('UnrealEditor-Win64-DebugGame.exe', '', $true), @('unrealeditor.exe', '', $true),
        @('LiveCodingConsole.exe', '', $true), @('UnrealBuildTool.exe', '', $true), @('AutomationTool.exe', '', $true), @('MSBuild.exe', '', $true),
        @('UAT.exe', '', $true), @('MyProject.exe', '', $true), @('MyProjectServer.exe', '', $true), @('MyProjectClient.exe', '', $true),
        @('MyProject-Win64-Shipping.exe', '', $true), @('MyProjectServer-Win64-Shipping.exe', '', $true),
        @('dotnet.exe', 'dotnet C:\x\UnrealBuildTool.dll -Mode=Build', $true), @('dotnet.exe', '', $true), @('dotnet.exe', $null, $true),
        @('dotnet.exe', 'C:\Program Files\Epic Games\UE_5.7\Engine\x.dll', $true), @('dotnet.exe', 'dotnet RunUAT.dll', $true),
        @('notepad.exe', '', $false), @('explorer.exe', 'explorer.exe', $false), @('powershell.exe', 'powershell', $false),
        @('dotnet.exe', 'dotnet C:\tools\foo.dll', $false), @('MyProjectFoo.exe', '', $false), @('MyProjectServers.exe', '', $false),
        @('UnrealEditor.txt', '', $false), @('xUnrealEditor.exe', '', $false), @('MSBuildX.exe', '', $false)
    )
    $pi = 0
    foreach ($c in $pTable) {
        $pi++
        $got = @(Get-MMOBlockingProcesses -Processes @([pscustomobject]@{ Name = $c[0]; ProcessId = $pi; CommandLine = $c[1] }))
        Check 'procs' ('row' + $pi) ((($got.Count -eq 1) -eq $c[2]) -and ((-not $c[2]) -or ($got[0].Name -ceq $c[0])))
    }
    Check 'procs' 'null_element_ignored' (@(Get-MMOBlockingProcesses -Processes @($null, [pscustomobject]@{ Name = 'notepad.exe'; ProcessId = 1; CommandLine = '' })).Count -eq 0)
    Check 'procs' 'empty_list_no_blockers' (@(Get-MMOBlockingProcesses -Processes @()).Count -eq 0)
    Check 'procs' 'missing_commandline_member_dotnet_blocks' (@(Get-MMOBlockingProcesses -Processes @([pscustomobject]@{ Name = 'dotnet.exe'; ProcessId = 1 })).Count -eq 1)

    $uDir = Join-Path $fx 'snap_unstable'
    $uFiles = Join-Path $uDir 'files'
    [void][IO.Directory]::CreateDirectory($uFiles)
    New-BaseFiles $uFiles
    $um = New-MMOManifest -FilesRoot $uFiles -SourceSavedDir $fx -Mode 'live' -UnstablePaths @('SaveGames/MMOSave_Ali.sav') -CreatedUtc '2026-10-27T10:00:00Z'
    Write-MMOManifest -Manifest $um -SnapshotDir $uDir
    $ue = @($um.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' })
    Check 'unstable' 'entry_marked_invalid' (($ue.Count -eq 1) -and ($ue[0].valid -eq $false) -and ($ue[0].notes -ceq 'unstable'))
    Check 'unstable' 'hash_still_content_hash' ($ue[0].sha256 -ceq (Get-OracleSha (Join-Path $uFiles 'SaveGames\MMOSave_Ali.sav')))
    $ut = Test-MMOSnapshot -SnapshotDir $uDir
    Check 'unstable' 'snapshot_ok_with_unstable_entry' ($ut.Ok -and ($ut.InvalidEntries -contains 'SaveGames/MMOSave_Ali.sav') -and ($ut.InvalidEntries -contains 'SaveGames/MMOSave_Bad.sav'))
    Check 'unstable' 'other_entries_unaffected' (@($um.entries | Where-Object { $_.notes -ceq 'unstable' }).Count -eq 1)
    CheckThrows 'unstable' 'unknown_path_throws' { New-MMOManifest -FilesRoot $uFiles -SourceSavedDir $fx -Mode 'live' -UnstablePaths @('SaveGames/MMOSave_Nope.sav') } 'unstable_path_unknown:'
    # O6: kararsiz not yalniz mode=live manifestte kabul: uretimde de, dogrulamada da
    CheckThrows 'unstable' 'quiesced_mode_with_unstable_paths_throws' { New-MMOManifest -FilesRoot $uFiles -SourceSavedDir $fx -UnstablePaths @('SaveGames/MMOSave_Ali.sav') } 'unstable_requires_live'
    Check 'unstable' 'snapshot_result_exposes_mode_and_flags' (($ut.Mode -ceq 'live') -and $ut.HasInvalid -and $ut.HasUnstable -and (-not $ut.ProofStale))
    $uqDir = Join-Path $fx 'snap_unstable_quiesced'
    Copy-Item -LiteralPath $uDir -Destination $uqDir -Recurse
    Edit-TestManifest $uqDir { param($m) $m.mode = 'quiesced' } -FixDigest
    $utq = Test-MMOSnapshot -SnapshotDir $uqDir
    Check 'unstable' 'unstable_note_in_quiesced_manifest_rejected' ((-not $utq.Ok) -and (HasErr $utq.Errors 'unstable_in_quiesced:SaveGames/MMOSave_Ali.sav') -and ($utq.Mode -ceq 'quiesced') -and (-not $utq.HasUnstable))
    Edit-TestManifest $uDir { param($m) ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' }).valid = $true } -FixDigest
    $ut2 = Test-MMOSnapshot -SnapshotDir $uDir
    Check 'unstable' 'unstable_with_valid_true_rejected' ((-not $ut2.Ok) -and (HasErr $ut2.Errors 'notes_mismatch:SaveGames/MMOSave_Ali.sav'))
    Edit-TestManifest $uDir { param($m) $e = ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' }); $e.valid = $false; $e.notes = 'tampered' } -FixDigest
    $ut3 = Test-MMOSnapshot -SnapshotDir $uDir
    Check 'unstable' 'other_notes_with_valid_false_rejected' ((-not $ut3.Ok) -and (HasErr $ut3.Errors 'valid_mismatch:SaveGames/MMOSave_Ali.sav'))
    Edit-TestManifest $uDir { param($m) $e = ($m.entries | Where-Object { $_.path -ceq 'MMODaily/MMOSave_Ali.json' }); $e.valid = $false; $e.notes = 'unstable' } -FixDigest
    $ut4 = Test-MMOSnapshot -SnapshotDir $uDir
    Check 'unstable' 'conservative_direction_accepts_extra_unstable_flag' (@($ut4.Errors | Where-Object { $_ -like '*MMODaily/MMOSave_Ali.json' }).Count -eq 0)

    # ---- 16. B1: BackupSaves.ps1 (yalniz fikstur Saved/yedek koku; gercek Saved'e dokunulmaz) ----------------------------------
    $bkScript = Join-Path $PSScriptRoot 'BackupSaves.ps1'
    $b1 = Join-Path $fx 'b1'
    [void][IO.Directory]::CreateDirectory($b1)
    $benign = { @([pscustomobject]@{ Name = 'powershell.exe'; ProcessId = 4; CommandLine = 'x' }, [pscustomobject]@{ Name = 'explorer.exe'; ProcessId = 5; CommandLine = '' }) }
    $repoReject = Join-Path $RepoRoot ('_mmo_bk_reject_' + $guard)
    $global:hookCalls = @{}

    function Invoke-TestBackup {
        param([string]$Saved, [string]$Root, [hashtable]$Extra = @{}, [switch]$RootOutsideFixture, [switch]$NoProvider)
        # Kaynak her zaman fikstur icinde olmali: gercek Saved'e hicbir kosuda gitmez.
        if (-not (Test-MMOPathUnder -Path $Saved -Root $fx)) { throw 'test_guard_saved_outside_fixture' }
        if ((-not $RootOutsideFixture) -and -not (Test-MMOPathUnder -Path $Root -Root $fx -AllowEqual)) { throw 'test_guard_root_outside_fixture' }
        $p = @{ SavedDir = $Saved; BackupRoot = $Root; ProcessProvider = $benign }
        foreach ($k in $Extra.Keys) { $p[$k] = $Extra[$k] }
        if ($NoProvider) { $p.Remove('ProcessProvider') }
        $global:LASTEXITCODE = -999
        $lines = @(& $bkScript @p *>&1 | ForEach-Object { [string]$_ })
        return [pscustomobject]@{ Code = $global:LASTEXITCODE; Out = ($lines -join "`n") }
    }
    function New-FakeSaved {
        param([string]$Dir, [switch]$KeepBad)
        New-BaseFiles $Dir
        if (-not $KeepBad) { [IO.File]::Delete((Join-Path $Dir 'SaveGames\MMOSave_Bad.sav')) }
        Write-TestText (Join-Path $Dir 'Logs\app.log') 'log line'
        Write-TestText (Join-Path $Dir 'Autosaves\x.txt') 'autosave'
        # Kaynak dosyalarin zamani temiz kapanis kanitindan ONCE (proof_stale=false); bayat-kanit vakalari zamani kendileri ileri alir.
        foreach ($ff in (Get-ChildItem -LiteralPath $Dir -Recurse -File -Force)) { [IO.File]::SetLastWriteTimeUtc($ff.FullName, (New-Object datetime(2026, 10, 27, 8, 0, 0, [DateTimeKind]::Utc))) }
    }
    function NoSnap {
        # Kok yok ya da icinde goruntu dizini / .partial yok (yalniz last_run.json gibi durum dosyalari olabilir): "hicbir yedek alinmadi" kaniti.
        param([string]$Root)
        if (-not (Test-Path -LiteralPath $Root)) { return $true }
        return (@(Get-ChildItem -LiteralPath $Root -Force | Where-Object { $_.PSIsContainer -or $_.Name -like '*.partial' }).Count -eq 0)
    }
    function Get-TreeOracle {
        param([string]$Dir)
        $l = New-Object System.Collections.ArrayList
        $base = [IO.Path]::GetFullPath($Dir).TrimEnd('\')
        if (Test-Path -LiteralPath $Dir) {
            foreach ($f in (Get-ChildItem -LiteralPath $Dir -Recurse -Force -File)) { [void]$l.Add($f.FullName.Substring($base.Length) + '|' + (Get-OracleSha $f.FullName)) }
        }
        $a = [string[]]@($l.ToArray()); [Array]::Sort($a, [StringComparer]::Ordinal)
        return ($a -join "`n")
    }
    function Get-SnapDirs {
        param([string]$Root)
        if (-not (Test-Path -LiteralPath $Root)) { return @() }
        return @(Get-ChildItem -LiteralPath $Root -Force | ForEach-Object { $_.Name })
    }
    function Read-TestManifest {
        param([string]$Snap)
        return ((Read-MMOUtf8Text (Join-Path $Snap 'manifest.json')).Text | ConvertFrom-Json)
    }

    # 16a Varsayilanlar (K1) ve ASCII
    $perr = $null
    $past = [System.Management.Automation.Language.Parser]::ParseFile($bkScript, [ref]$null, [ref]$perr)
    $pdef = @{}
    foreach ($pp in $past.ParamBlock.Parameters) { if ($null -ne $pp.DefaultValue) { $pdef[$pp.Name.VariablePath.UserPath] = $pp.DefaultValue.Extent.Text } }
    Check 'bk_defaults' 'script_parses' (($null -eq $perr) -or ($perr.Count -eq 0))
    Check 'bk_defaults' 'saved_default_is_repo_saved' ($pdef['SavedDir'] -like "*'Saved'*" -and $pdef['SavedDir'] -like '*Split-Path -Parent $PSScriptRoot*')
    Check 'bk_defaults' 'backup_root_default_outside_repo' ($pdef['BackupRoot'] -like "*'MMOBackups'*" -and $pdef['BackupRoot'] -like '*Split-Path -Parent (Split-Path -Parent $PSScriptRoot)*')
    Check 'bk_defaults' 'script_is_ascii' (-not (([IO.File]::ReadAllBytes($bkScript)) | Where-Object { $_ -gt 127 }))

    # 16b Temiz yedek
    $sA = Join-Path $b1 'sA'; $rA = Join-Path $b1 'rA'
    New-FakeSaved $sA
    Write-TestText (Join-Path $sA 'SaveGames\notes.txt') 'not whitelisted'
    Write-TestText (Join-Path $sA 'SaveGames\other.sav') 'not MMOSave_'
    Write-TestText (Join-Path $sA 'MMOMail\sub\deep.json') '{"version":1}'
    $treeA0 = Get-TreeOracle $sA
    $stampA0 = Get-SavedStamp -SavedRoot $sA
    $expRel = @('MMOAchievements/MMOSave_Ali.json', 'MMOAchievements/MMOSave_Ali.json.bad', 'MMODaily/MMOSave_Ali.json', 'MMODaily/MMOSave_Ali.json.tmp',
        'MMOMail/MMOMail_ali.json', 'MMOMail/MMOMail_ali.json.bak', 'MMOMail/MMOMail_ali_baba.json', 'MMORace/MMORace_mmosave_ali.json',
        'MMORace/MMORace_mmosave_ali.json.bad', 'SaveGames/MMOSave_Ali Baba.sav', 'SaveGames/MMOSave_Ali.sav')
    $a1 = Invoke-TestBackup $sA $rA @{ Timestamp = '20261027T100000Z'; Label = 'ok'; AllowIgnored = $true }
    $snapA = Join-Path $rA '20261027T100000Z-ok'
    Check 'bk_ok' 'exit_0' ($a1.Code -eq 0)
    Check 'bk_ok' 'status_line' ($a1.Out -like '*[[]MMO_BACKUP] status=ok reason=ok exit=0*')
    Check 'bk_ok' 'root_created_with_only_the_snapshot' ((@(Get-SnapDirs $rA | Where-Object { $_ -notlike '*.json' })).Count -eq 1 -and (Test-Path -LiteralPath $snapA -PathType Container))
    Check 'bk_ok' 'lock_released_and_removed' (-not (Test-Path -LiteralPath (Join-Path $rA '.lock')))
    $tA = Test-MMOSnapshot -SnapshotDir $snapA
    Check 'bk_ok' 'own_verification_ok' ($tA.Ok -and -not $tA.Incomplete -and $tA.InvalidEntries.Count -eq 0 -and $tA.Extra.Count -eq 0)
    $mA = Read-TestManifest $snapA
    $gotRel = [string[]]@($mA.entries | ForEach-Object { $_.path }); [Array]::Sort($gotRel, [StringComparer]::Ordinal)
    $wantRel = [string[]]@($expRel); [Array]::Sort($wantRel, [StringComparer]::Ordinal)
    Check 'bk_ok' 'entries_are_exactly_the_whitelisted_files' (($gotRel -join '|') -ceq ($wantRel -join '|') -and $mA.entries.Count -eq 11)
    $hashOk = $true; $copyOk = $true; $sizeOk = $true
    foreach ($e in $mA.entries) {
        $srcF = Join-Path $sA ($e.path.Replace('/', '\')); $dstF = Join-Path (Join-Path $snapA 'files') ($e.path.Replace('/', '\'))
        if ($e.sha256 -cne (Get-OracleSha $srcF)) { $hashOk = $false }
        if ((Get-OracleSha $dstF) -cne (Get-OracleSha $srcF)) { $copyOk = $false }
        if ([long]$e.size -ne (Get-Item -LiteralPath $srcF).Length) { $sizeOk = $false }
        if ($e.valid -ne $true) { $hashOk = $false }
    }
    Check 'bk_ok' 'manifest_hashes_match_independent_oracle_and_all_valid' $hashOk
    Check 'bk_ok' 'copies_are_byte_identical' $copyOk
    Check 'bk_ok' 'manifest_sizes_match' $sizeOk
    Check 'bk_ok' 'manifest_mode_quiesced' ($mA.mode -ceq 'quiesced')
    Check 'bk_ok' 'manifest_shutdown_proof_null_without_flag' ($null -eq $mA.shutdown_proof)
    Check 'bk_ok' 'manifest_source_dir_recorded' ($mA.source_saved_dir -ceq [IO.Path]::GetFullPath($sA).TrimEnd('\'))
    Check 'bk_ok' 'ignored_count_reported' ($a1.Out -like '*ignored=3*')
    Check 'bk_ok' 'ignored_names_in_console_only' ($a1.Out -like '*[[]MMO_BACKUP_IGNORED] SaveGames/notes.txt*')
    $filesA = Join-Path $snapA 'files'
    Check 'bk_ok' 'ignored_files_not_copied' (-not (Test-Path -LiteralPath (Join-Path $filesA 'SaveGames\notes.txt')) -and -not (Test-Path -LiteralPath (Join-Path $filesA 'SaveGames\other.sav')) -and -not (Test-Path -LiteralPath (Join-Path $filesA 'MMOMail\sub')))
    Check 'bk_ok' 'logs_and_autosaves_not_copied' (-not (Test-Path -LiteralPath (Join-Path $filesA 'Logs')) -and -not (Test-Path -LiteralPath (Join-Path $filesA 'Autosaves')))
    Check 'bk_ok' 'no_partial_files_left' (@(Get-ChildItem -LiteralPath $snapA -Recurse -Force -File | Where-Object { $_.Name -like '*.partial' }).Count -eq 0)
    $logPathA = Join-Path $snapA 'backup.log'
    $logA = ''
    if (Test-Path -LiteralPath $logPathA) { $logA = [IO.File]::ReadAllText($logPathA) }
    Check 'bk_ok' 'backup_log_status_ok' ($logA -like '*status=ok*')
    Check 'bk_ok' 'backup_log_manifest_sha_matches_manifest_file' ($logA -like ('*manifest_sha256=' + (Get-OracleSha (Join-Path $snapA 'manifest.json')) + '*'))
    Check 'bk_ok' 'backup_log_has_no_player_data' (($logA -cnotmatch 'Ali|Baba|Gezgin|notes\.txt') -and ($logA.Length -gt 0))
    Check 'bk_ok' 'console_prints_manifest_sha' ($a1.Out -like ('*manifest_sha256=' + (Get-OracleSha (Join-Path $snapA 'manifest.json')) + '*'))
    Check 'bk_ok' 'source_tree_byte_identical_after' ((Get-TreeOracle $sA) -ceq $treeA0)
    Check 'bk_ok' 'source_stamp_unchanged_after' ((Get-SavedStamp -SavedRoot $sA).Digest -ceq $stampA0.Digest)

    # 16c Birden fazla yedek: silme yok, var olan goruntu uzerine yazma yok, varsayilan saat damgasi
    $manShaA = Get-OracleSha (Join-Path $snapA 'manifest.json')
    $a2 = Invoke-TestBackup $sA $rA @{ Timestamp = '20261027T100001Z'; Label = 'ok2'; AllowIgnored = $true }
    Check 'bk_multi' 'second_backup_exit_0' ($a2.Code -eq 0)
    $a3 = Invoke-TestBackup $sA $rA @{ Timestamp = '20261027T100000Z'; Label = 'ok'; AllowIgnored = $true }
    Check 'bk_multi' 'same_name_refused_exit_1' (($a3.Code -eq 1) -and ($a3.Out -like '*reason=snapshot_exists*'))
    Check 'bk_multi' 'refused_run_did_not_touch_existing_snapshot' ((Get-OracleSha (Join-Path $snapA 'manifest.json')) -ceq $manShaA -and -not ((Get-Content -LiteralPath $logPathA -Raw) -like '*snapshot_exists*'))
    Check 'bk_multi' 'both_snapshots_still_present_nothing_deleted' ((@(Get-SnapDirs $rA | Where-Object { $_ -notlike '*.json' })).Count -eq 2)
    $a4 = Invoke-TestBackup $sA $rA @{ Label = 'now'; AllowIgnored = $true }
    $nowDirs = @(Get-SnapDirs $rA | Where-Object { $_ -cmatch '^[0-9]{8}T[0-9]{6}Z-now\z' })
    Check 'bk_multi' 'default_clock_name_and_exit_0' (($a4.Code -eq 0) -and ($nowDirs.Count -eq 1))
    $nowStamp = [datetime]::ParseExact($nowDirs[0].Substring(0, 16), "yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal)
    Check 'bk_multi' 'default_clock_is_now_utc' ([math]::Abs(([datetime]::UtcNow - $nowStamp).TotalSeconds) -lt 300)
    $a5 = Invoke-TestBackup $sA $rA @{ Timestamp = '20261027T100002Z'; Label = ''; AllowIgnored = $true }
    Check 'bk_multi' 'empty_label_name_is_timestamp_only' (($a5.Code -eq 0) -and (Test-Path -LiteralPath (Join-Path $rA '20261027T100002Z') -PathType Container))
    Check 'bk_multi' 'first_snapshot_manifest_unchanged_after_more_runs' ((Get-OracleSha (Join-Path $snapA 'manifest.json')) -ceq $manShaA)
    Check 'bk_multi' 'source_still_unchanged' ((Get-TreeOracle $sA) -ceq $treeA0)

    # 16d Surec denetimi (canli sunucu/editor)
    $procBlock = @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe', 'MyProjectServer.exe', 'MyProjectServer-Win64-Shipping.exe', 'MyProject.exe', 'UnrealBuildTool.exe')
    $bi = 0
    foreach ($pn in $procBlock) {
        $bi++
        $rB = Join-Path $b1 ('rB' + $bi)
        $nameCopy = $pn
        $prov = { @([pscustomobject]@{ Name = 'explorer.exe'; ProcessId = 1; CommandLine = '' }, [pscustomobject]@{ Name = $nameCopy; ProcessId = 99; CommandLine = '' }) }.GetNewClosure()
        $r = Invoke-TestBackup $sA $rB @{ ProcessProvider = $prov }
        Check 'bk_proc' ('blocked_' + $bi) (($r.Code -eq 3) -and ($r.Out -like '*reason=process_running*') -and ($r.Out -like ('*process=' + $pn + '*')) -and (NoSnap $rB))
    }
    $rP = Join-Path $b1 'rP'
    $r = Invoke-TestBackup $sA $rP @{ ProcessProvider = { throw 'cim failed' } }
    Check 'bk_proc' 'provider_throws_defers_fail_closed' (($r.Code -eq 3) -and ($r.Out -like '*reason=process_query_failed*') -and (NoSnap $rP))
    $r = Invoke-TestBackup $sA $rP @{ ProcessProvider = { @() } }
    Check 'bk_proc' 'provider_empty_defers_fail_closed' (($r.Code -eq 3) -and ($r.Out -like '*reason=process_query_empty*') -and (NoSnap $rP))
    $r = Invoke-TestBackup $sA $rP @{ ProcessProvider = { $null } }
    Check 'bk_proc' 'provider_null_defers_fail_closed' (($r.Code -eq 3) -and ($r.Out -like '*reason=process_query_empty*') -and (NoSnap $rP))
    $r = Invoke-TestBackup $sA $rP @{ ProcessProvider = { @([pscustomobject]@{ Name = 'dotnet.exe'; ProcessId = 7; CommandLine = '' }) } }
    Check 'bk_proc' 'unreadable_dotnet_commandline_defers' (($r.Code -eq 3) -and ($r.Out -like '*reason=process_running*') -and (NoSnap $rP))

    # 16e -AllowLive: surec denetimi atlanir, mode=live + uyari, kararsiz dosya valid=false
    $rL = Join-Path $b1 'rL'
    $blocker = { @([pscustomobject]@{ Name = 'UnrealEditor.exe'; ProcessId = 99; CommandLine = '' }) }
    $r = Invoke-TestBackup $sA $rL @{ ProcessProvider = $blocker; AllowLive = $true; AllowIgnored = $true; Timestamp = '20261027T110000Z'; Label = 'lv' }
    $snapL = Join-Path $rL '20261027T110000Z-lv-live'
    Check 'bk_live' 'blocker_ignored_with_flag_exit_4_live' (($r.Code -eq 4) -and ($r.Out -like '*reason=live *') -and ($r.Out -like '*[[]MMO_BACKUP_RESULT] mode=live exit=4 status=complete_live reason=live snapshot=20261027T110000Z-lv-live process_check=skipped*'))
    Check 'bk_live' 'manifest_mode_live' ((Read-TestManifest $snapL).mode -ceq 'live')
    Check 'bk_live' 'warning_in_console_and_log' (($r.Out -like '*[[]MMO_BACKUP_WARN]*mode=live*') -and ([IO.File]::ReadAllText((Join-Path $snapL 'backup.log')) -like '*mode=live*'))
    Check 'bk_live' 'live_snapshot_verifies' ((Test-MMOSnapshot -SnapshotDir $snapL).Ok)
    $r = Invoke-TestBackup $sA (Join-Path $b1 'rL2') @{ ProcessProvider = { throw 'cim failed' }; AllowLive = $true; AllowIgnored = $true; Timestamp = '20261027T110001Z' }
    Check 'bk_live' 'provider_not_consulted_with_flag' (($r.Code -eq 4) -and (Test-Path -LiteralPath (Join-Path $b1 'rL2\20261027T110001Z-manual-live') -PathType Container))
    $r = Invoke-TestBackup $sA (Join-Path $b1 'rL3') @{ AllowLive = $true; RequireCleanShutdown = $true }
    Check 'bk_live' 'allowlive_and_requirecleanshutdown_is_usage_error' (($r.Code -eq 2) -and ($r.Out -like '*reason=allowlive_and_requirecleanshutdown*') -and (NoSnap (Join-Path $b1 'rL3')))

    $sU = Join-Path $b1 'sU'; $rU = Join-Path $b1 'rU'
    New-FakeSaved $sU
    $global:hookCalls = @{}
    $hookAlways = { param($rel, $attempt, $src)
        if (-not $global:hookCalls.ContainsKey($rel)) { $global:hookCalls[$rel] = 0 }
        $global:hookCalls[$rel] = $global:hookCalls[$rel] + 1
        if ($rel -ceq 'SaveGames/MMOSave_Ali.sav') { $fsx = [IO.File]::Open($src, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite); try { $fsx.WriteByte(7) } finally { $fsx.Dispose() } }
    }
    $r = Invoke-TestBackup $sU $rU @{ AllowLive = $true; Timestamp = '20261027T120000Z'; Label = 'unstable'; TestHooks = @{ DuringCopy = $hookAlways } }
    $snapU = Join-Path $rU '20261027T120000Z-unstable-live'
    Check 'bk_unstable' 'exit_4_usable_but_not_trusted' ($r.Code -eq 4)
    Check 'bk_unstable' 'reason_live_plus_invalid_files' ($r.Out -like '*reason=live+invalid_files *' -and $r.Out -like '*unstable=1*')
    Check 'bk_unstable' 'retried_three_times' ($global:hookCalls['SaveGames/MMOSave_Ali.sav'] -eq 3)
    Check 'bk_unstable' 'stable_files_read_once_each' ($global:hookCalls['MMODaily/MMOSave_Ali.json'] -ge 1)
    $mU = Read-TestManifest $snapU
    $eU = @($mU.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' })
    Check 'bk_unstable' 'entry_valid_false_notes_unstable' (($eU.Count -eq 1) -and ($eU[0].valid -eq $false) -and ($eU[0].notes -ceq 'unstable'))
    Check 'bk_unstable' 'only_that_entry_is_invalid' (@($mU.entries | Where-Object { $_.valid -eq $false }).Count -eq 1)
    $tU = Test-MMOSnapshot -SnapshotDir $snapU
    Check 'bk_unstable' 'snapshot_complete_and_verifies_with_one_invalid' ($tU.Ok -and ($tU.InvalidEntries.Count -eq 1) -and ((Read-TestManifest $snapU).mode -ceq 'live'))
    Check 'bk_unstable' 'log_status_complete_with_invalid' ([IO.File]::ReadAllText((Join-Path $snapU 'backup.log')) -like '*status=complete_with_invalid_files*')
    $sV = Join-Path $b1 'sV'; $rV = Join-Path $b1 'rV'
    New-FakeSaved $sV
    $global:hookCalls = @{}
    $hookOnce = { param($rel, $attempt, $src)
        if (-not $global:hookCalls.ContainsKey($rel)) { $global:hookCalls[$rel] = 0 }
        $global:hookCalls[$rel] = $global:hookCalls[$rel] + 1
        if (($rel -ceq 'SaveGames/MMOSave_Ali.sav') -and ($attempt -eq 1)) { $fsx = [IO.File]::Open($src, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite); try { $fsx.WriteByte(9) } finally { $fsx.Dispose() } }
    }
    $r = Invoke-TestBackup $sV $rV @{ AllowLive = $true; Timestamp = '20261027T120001Z'; Label = 'transient'; TestHooks = @{ DuringCopy = $hookOnce } }
    $snapV = Join-Path $rV '20261027T120001Z-transient-live'
    $eV = @((Read-TestManifest $snapV).entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' })
    Check 'bk_unstable' 'transient_change_retried_and_stable_exit_4_live' (($r.Code -eq 4) -and ($global:hookCalls['SaveGames/MMOSave_Ali.sav'] -eq 2) -and ($eV[0].valid -eq $true))
    Check 'bk_unstable' 'transient_snapshot_holds_final_content' ($eV[0].sha256 -ceq (Get-OracleSha (Join-Path $sV 'SaveGames\MMOSave_Ali.sav')))

    # 16f -RequireCleanShutdown
    $shGood = '{"run_id":"20261027T100000Z-1234","exit_code":0,"failed":0,"gone":1,"still_connected":0,"kick_failed":0,"time_utc":"2026-10-27T10:00:00.123Z"}'
    $sS = Join-Path $b1 'sS'; $rS = Join-Path $b1 'rS'
    New-FakeSaved $sS
    Write-TestText (Join-Path $sS 'Logs\MMOShutdownResult.json') $shGood
    $logTreeBefore = Get-TreeOracle (Join-Path $sS 'Logs')
    $r = Invoke-TestBackup $sS $rS @{ RequireCleanShutdown = $true; Timestamp = '20261027T130000Z'; Label = 'clean' }
    $snapS = Join-Path $rS '20261027T130000Z-clean'
    Check 'bk_shutdown' 'clean_shutdown_accepted_exit_0' ($r.Code -eq 0)
    $mS = Read-TestManifest $snapS
    $prS = $mS.shutdown_proof
    Check 'bk_shutdown' 'proof_recorded_run_id_exit_time' (($null -ne $prS) -and ($prS.run_id -ceq '20261027T100000Z-1234') -and ($prS.exit_code -eq 0) -and ($prS.time_utc -ceq '2026-10-27T10:00:00Z'))
    Check 'bk_shutdown' 'snapshot_with_proof_verifies' ((Test-MMOSnapshot -SnapshotDir $snapS).Ok)
    Check 'bk_shutdown' 'proof_summary_line' ($r.Out -like '*shutdown_proof=recorded*')
    Check 'bk_shutdown' 'shutdown_result_file_untouched' ((Get-TreeOracle (Join-Path $sS 'Logs')) -ceq $logTreeBefore)
    Write-TestText (Join-Path $sS 'Logs\MMOShutdownResult.json') ($shGood.Replace('10:00:00.123Z', '10:00:00.1234567Z'))
    $r = Invoke-TestBackup $sS $rS @{ RequireCleanShutdown = $true; Timestamp = '20261027T130001Z'; Label = 'frac7' }
    $prF = (Read-TestManifest (Join-Path $rS '20261027T130001Z-frac7')).shutdown_proof
    Check 'bk_shutdown' 'seven_digit_fraction_normalized' (($r.Code -eq 0) -and ($null -ne $prF) -and ($prF.time_utc -ceq '2026-10-27T10:00:00Z'))
    Write-TestText (Join-Path $sS 'Logs\MMOShutdownResult.json') ($shGood.Replace('.123Z', 'Z'))
    $r = Invoke-TestBackup $sS $rS @{ RequireCleanShutdown = $true; Timestamp = '20261027T130002Z'; Label = 'nofrac' }
    Check 'bk_shutdown' 'no_fraction_accepted' ($r.Code -eq 0)

    $shBad = @(
        @('exit2', $shGood.Replace('"exit_code":0', '"exit_code":2'), 'shutdown_not_clean'), @('exit1', $shGood.Replace('"exit_code":0', '"exit_code":1'), 'shutdown_not_clean'),
        @('exit255', $shGood.Replace('"exit_code":0', '"exit_code":255'), 'shutdown_not_clean'),
        @('garbage', 'not json at all', 'shutdown_result_invalid'), @('array', '[1,2]', 'shutdown_result_invalid'), @('empty', '', 'shutdown_result_invalid'),
        @('runid_empty', $shGood.Replace('20261027T100000Z-1234', ''), 'shutdown_result_invalid'),
        @('runid_chars', $shGood.Replace('20261027T100000Z-1234', 'abc'), 'shutdown_result_invalid'),
        @('runid_41', $shGood.Replace('20261027T100000Z-1234', ('1' * 41)), 'shutdown_result_invalid'),
        @('runid_missing', $shGood.Replace('"run_id":"20261027T100000Z-1234",', ''), 'shutdown_result_invalid'),
        @('exit_string', $shGood.Replace('"exit_code":0', '"exit_code":"0"'), 'shutdown_result_invalid'),
        @('exit_256', $shGood.Replace('"exit_code":0', '"exit_code":256'), 'shutdown_result_invalid'),
        @('exit_neg', $shGood.Replace('"exit_code":0', '"exit_code":-1'), 'shutdown_result_invalid'),
        @('exit_frac', $shGood.Replace('"exit_code":0', '"exit_code":0.5'), 'shutdown_result_invalid'),
        @('exit_missing', $shGood.Replace('"exit_code":0,', ''), 'shutdown_result_invalid'),
        @('time_missing', $shGood.Replace(',"time_utc":"2026-10-27T10:00:00.123Z"', ''), 'shutdown_result_invalid'),
        @('time_text', $shGood.Replace('2026-10-27T10:00:00.123Z', 'yesterday'), 'shutdown_result_invalid'),
        @('time_month13', $shGood.Replace('2026-10-27T10:00:00.123Z', '2026-13-45T10:00:00Z'), 'shutdown_result_invalid'),
        @('time_no_z', $shGood.Replace('2026-10-27T10:00:00.123Z', '2026-10-27T10:00:00+02:00'), 'shutdown_result_invalid')
    )
    $sB = Join-Path $b1 'sB'
    New-FakeSaved $sB
    foreach ($c in $shBad) {
        Write-TestText (Join-Path $sB 'Logs\MMOShutdownResult.json') $c[1]
        $rSh = Join-Path $b1 ('rSh_' + $c[0])
        $r = Invoke-TestBackup $sB $rSh @{ RequireCleanShutdown = $true }
        Check 'bk_shutdown' ('rejected_' + $c[0]) (($r.Code -eq 3) -and ($r.Out -like ('*reason=' + $c[2] + '*')) -and (NoSnap $rSh))
    }
    Write-TestBytes (Join-Path $sB 'Logs\MMOShutdownResult.json') ([byte[]]@(0x7B, 0xC3, 0x28, 0x7D))
    $r = Invoke-TestBackup $sB (Join-Path $b1 'rSh_utf8') @{ RequireCleanShutdown = $true }
    Check 'bk_shutdown' 'rejected_invalid_utf8' (($r.Code -eq 3) -and ($r.Out -like '*reason=shutdown_result_invalid*'))
    Write-TestBytes (Join-Path $sB 'Logs\MMOShutdownResult.json') (New-Object byte[] 70000)
    $r = Invoke-TestBackup $sB (Join-Path $b1 'rSh_big') @{ RequireCleanShutdown = $true }
    Check 'bk_shutdown' 'rejected_oversized_file' (($r.Code -eq 3) -and ($r.Out -like '*reason=shutdown_result_invalid*'))
    [IO.File]::Delete((Join-Path $sB 'Logs\MMOShutdownResult.json'))
    $r = Invoke-TestBackup $sB (Join-Path $b1 'rSh_nofile') @{ RequireCleanShutdown = $true }
    Check 'bk_shutdown' 'rejected_file_missing' (($r.Code -eq 3) -and ($r.Out -like '*reason=shutdown_result_missing*') -and (NoSnap (Join-Path $b1 'rSh_nofile')))
    $sN = Join-Path $b1 'sN'
    New-FakeSaved $sN
    [IO.Directory]::Delete((Join-Path $sN 'Logs'), $true)
    $r = Invoke-TestBackup $sN (Join-Path $b1 'rSh_nologs') @{ RequireCleanShutdown = $true }
    Check 'bk_shutdown' 'rejected_logs_dir_missing' (($r.Code -eq 3) -and ($r.Out -like '*reason=shutdown_result_missing*'))
    $sJ = Join-Path $b1 'sJ'
    New-FakeSaved $sJ
    $logDecoy = Join-Path $b1 'logdecoy'
    Write-TestText (Join-Path $logDecoy 'MMOShutdownResult.json') $shGood
    [IO.Directory]::Delete((Join-Path $sJ 'Logs'), $true)
    New-TestJunction (Join-Path $sJ 'Logs') $logDecoy
    $r = Invoke-TestBackup $sJ (Join-Path $b1 'rSh_junction') @{ RequireCleanShutdown = $true }
    Check 'bk_shutdown' 'rejected_logs_dir_junction' (($r.Code -eq 3) -and ($r.Out -like '*reason=shutdown_result_invalid*') -and (NoSnap (Join-Path $b1 'rSh_junction')))
    $sD = Join-Path $b1 'sD'
    New-FakeSaved $sD
    [void][IO.Directory]::CreateDirectory((Join-Path $sD 'Logs\MMOShutdownResult.json'))
    $r = Invoke-TestBackup $sD (Join-Path $b1 'rSh_dir') @{ RequireCleanShutdown = $true }
    Check 'bk_shutdown' 'rejected_result_path_is_directory' (($r.Code -eq 3) -and ($r.Out -like '*reason=shutdown_result_invalid*'))
    # kanit dosyasi tek basina yetmez: surec denetimi hala kapidir
    $r = Invoke-TestBackup $sS (Join-Path $b1 'rSh_proc') @{ RequireCleanShutdown = $true; ProcessProvider = $blocker }
    Check 'bk_shutdown' 'clean_proof_does_not_bypass_process_gate' (($r.Code -eq 3) -and ($r.Out -like '*reason=process_running*'))
    # bayrak yoksa dosya okunmaz: kotu dosya zararsiz, kanit null
    $sK = Join-Path $b1 'sK'
    New-FakeSaved $sK
    Write-TestText (Join-Path $sK 'Logs\MMOShutdownResult.json') ($shGood.Replace('"exit_code":0', '"exit_code":2'))
    $r = Invoke-TestBackup $sK (Join-Path $b1 'rK') @{ Timestamp = '20261027T140000Z'; Label = 'noflag' }
    Check 'bk_shutdown' 'without_flag_result_file_ignored_proof_null' (($r.Code -eq 0) -and ($null -eq (Read-TestManifest (Join-Path $b1 'rK\20261027T140000Z-noflag')).shutdown_proof))

    # 16g Yedek koku yapisi / ozyineleme korumasi
    $sR = Join-Path $b1 'sR'
    New-FakeSaved $sR
    $treeR0 = Get-TreeOracle $sR
    $stampR0 = Get-SavedStamp -SavedRoot $sR
    $inSaved = Join-Path $sR 'backups_here'
    $r = Invoke-TestBackup $sR $inSaved
    Check 'bk_root' 'root_inside_saved_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_inside_saved*') -and -not (Test-Path -LiteralPath $inSaved))
    $r = Invoke-TestBackup $sR $sR
    Check 'bk_root' 'root_equals_saved_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_inside_saved*'))
    $r = Invoke-TestBackup $sR (Join-Path $sR 'SaveGames')
    Check 'bk_root' 'root_is_saved_subdir_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_inside_saved*'))
    $r = Invoke-TestBackup $sR $b1
    Check 'bk_root' 'saved_inside_root_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=saved_inside_backup_root*'))
    $r = Invoke-TestBackup $sR $fx
    Check 'bk_root' 'saved_deep_inside_root_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=saved_inside_backup_root*'))
    Check 'bk_root' 'saved_unchanged_after_rejections' (((Get-TreeOracle $sR) -ceq $treeR0) -and ((Get-SavedStamp -SavedRoot $sR).Digest -ceq $stampR0.Digest))
    $sibling = Join-Path $b1 'sR_backups'
    $r = Invoke-TestBackup $sR $sibling @{ Timestamp = '20261027T150000Z' }
    Check 'bk_root' 'sibling_with_common_prefix_is_allowed' (($r.Code -eq 0) -and (Test-Path -LiteralPath (Join-Path $sibling '20261027T150000Z-manual\manifest.json')))
    $r = Invoke-TestBackup $sR $repoReject -RootOutsideFixture
    Check 'bk_root' 'root_inside_repo_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_inside_repo*') -and -not (Test-Path -LiteralPath $repoReject))
    $r = Invoke-TestBackup $sR (Join-Path $b1 'no_parent\deeper\root')
    Check 'bk_root' 'missing_parent_not_created' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_parent_missing*') -and -not (Test-Path -LiteralPath (Join-Path $b1 'no_parent')))
    $r = Invoke-TestBackup (Join-Path $b1 'no_such_saved') (Join-Path $b1 'rNoSaved')
    Check 'bk_root' 'missing_saved_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=saved_root_missing*') -and (NoSnap (Join-Path $b1 'rNoSaved')))
    $filePath = Join-Path $b1 'a_file'
    Write-TestText $filePath 'x'
    $r = Invoke-TestBackup $sR $filePath
    Check 'bk_root' 'root_is_a_file_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_not_directory*'))
    $r = Invoke-TestBackup $sR '\\?\C:\mmo_nope'  -RootOutsideFixture
    Check 'bk_root' 'invalid_root_path_is_usage_error' (($r.Code -eq 2) -and ($r.Out -like '*reason=path_invalid*'))

    # 16h Reparse point (junction) saldirilari: hicbir sey yazilmaz
    $jTarget = Join-Path $b1 'jtarget'
    [void][IO.Directory]::CreateDirectory($jTarget)
    $jRoot = Join-Path $b1 'jroot'
    New-TestJunction $jRoot $jTarget
    $r = Invoke-TestBackup $sR $jRoot
    Check 'bk_junction' 'root_junction_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_reparse_root*') -and (@(Get-ChildItem -LiteralPath $jTarget -Force).Count -eq 0))
    $r = Invoke-TestBackup $sR (Join-Path $jRoot 'child')
    Check 'bk_junction' 'root_under_junction_rejected' (($r.Code -eq 3) -and (@(Get-ChildItem -LiteralPath $jTarget -Force).Count -eq 0))
    $sJs = Join-Path $b1 'sJs'
    New-TestJunction $sJs $sR
    $r = Invoke-TestBackup $sJs (Join-Path $b1 'rJs')
    Check 'bk_junction' 'saved_junction_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=saved_reparse_root*') -and (NoSnap (Join-Path $b1 'rJs')))
    $sM = Join-Path $b1 'sM'
    New-FakeSaved $sM
    $mailDecoy = Join-Path $b1 'maildecoy'
    [void][IO.Directory]::CreateDirectory($mailDecoy)
    Copy-Item -Path (Join-Path $sM 'MMOMail\*') -Destination $mailDecoy
    [IO.Directory]::Delete((Join-Path $sM 'MMOMail'), $true)
    New-TestJunction (Join-Path $sM 'MMOMail') $mailDecoy
    $r = Invoke-TestBackup $sM (Join-Path $b1 'rM')
    Check 'bk_junction' 'source_subdir_junction_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=source_reparse*') -and (NoSnap (Join-Path $b1 'rM')))
    $sE = Join-Path $b1 'sE'
    New-FakeSaved $sE
    New-TestJunction (Join-Path $sE 'SaveGames\evil') $mailDecoy
    $r = Invoke-TestBackup $sE (Join-Path $b1 'rE')
    Check 'bk_junction' 'source_entry_junction_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=source_reparse_entry*') -and (NoSnap (Join-Path $b1 'rE')))
    Check 'bk_junction' 'junction_targets_untouched' (@(Get-ChildItem -LiteralPath $jTarget -Force).Count -eq 0)

    # 16i Kullanim hatalari (2): hicbir sey yazilmaz
    $rUse = Join-Path $b1 'rUse'
    $lblCases = @(@('upper', 'Bad'), @('space', 'a b'), @('slash', 'a/b'), @('dots', '..'), @('long25', ('a' * 25)), @('turkish', ('x' + [char]0x0131)), @('backslash', 'a\b'), @('colon', 'a:b'))
    foreach ($c in $lblCases) {
        $r = Invoke-TestBackup $sR $rUse @{ Label = $c[1] }
        Check 'bk_usage' ('bad_label_' + $c[0]) (($r.Code -eq 2) -and ($r.Out -like '*reason=bad_label*') -and (NoSnap $rUse))
    }
    $r = Invoke-TestBackup $sR $rUse @{ Label = ('a' * 24); Timestamp = '20261027T160000Z' }
    Check 'bk_usage' 'label_len24_accepted' ($r.Code -eq 0)
    foreach ($c in @(@('date', '2026-10-27'), @('no_z', '20261027T100000'), @('lower', '20261027t100000z'), @('long', '20261027T100000Z9'), @('unicode_digits', (([string][char]0x0661) * 8 + 'T100000Z')))) {
        $r = Invoke-TestBackup $sR (Join-Path $b1 'rUse2') @{ Timestamp = $c[1] }
        Check 'bk_usage' ('bad_timestamp_' + $c[0]) (($r.Code -eq 2) -and ($r.Out -like '*reason=bad_timestamp*') -and (NoSnap (Join-Path $b1 'rUse2')))
    }

    # 16j Kilit (ikinci arac ayni kokte ertelenir) ve bayat kilit dosyasi
    $rLk = Join-Path $b1 'rLk'
    [void][IO.Directory]::CreateDirectory($rLk)
    $holder = New-Object IO.FileStream((Join-Path $rLk '.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $r = Invoke-TestBackup $sR $rLk @{ Timestamp = '20261027T170000Z' }
        Check 'bk_lock' 'held_lock_defers' (($r.Code -eq 3) -and ($r.Out -like '*reason=lock_held*'))
        $lkNames = @(Get-SnapDirs $rLk)
        Check 'bk_lock' 'held_lock_wrote_no_snapshot' ((@($lkNames | Where-Object { $_ -cne 'last_run.json' }).Count -eq 1) -and (@($lkNames | Where-Object { $_ -cne 'last_run.json' })[0] -ceq '.lock'))
    } finally { $holder.Dispose() }
    $r = Invoke-TestBackup $sR $rLk @{ Timestamp = '20261027T170001Z' }
    Check 'bk_lock' 'free_after_release_exit_0' ($r.Code -eq 0)
    $rSt = Join-Path $b1 'rSt'
    [void][IO.Directory]::CreateDirectory($rSt)
    [IO.File]::WriteAllText((Join-Path $rSt '.lock'), 'stale from a crashed run')
    $r = Invoke-TestBackup $sR $rSt @{ Timestamp = '20261027T170002Z' }
    Check 'bk_lock' 'stale_lock_file_does_not_block' (($r.Code -eq 0) -and -not (Test-Path -LiteralPath (Join-Path $rSt '.lock')))
    $rLr = Join-Path $b1 'rLr'
    [void][IO.Directory]::CreateDirectory($rLr)
    $lockTarget = Join-Path $b1 'locktarget'
    Write-TestText $lockTarget 'target'
    $lockLink = Join-Path $rLr '.lock'
    try {
        [void](New-Item -ItemType SymbolicLink -Path $lockLink -Target $lockTarget -ErrorAction Stop)
        $r = Invoke-TestBackup $sR $rLr
        Check 'bk_lock' 'lock_symlink_rejected' (($r.Code -eq 3) -and ($r.Out -like '*reason=lock_reparse*') -and ((Get-Content -LiteralPath $lockTarget -Raw) -ceq 'target'))
        [IO.File]::Delete($lockLink)
    } catch {
        # Sembolik baglanti ayricalik ister (bu makinede olusturulamazsa kapi test edilemez; B0'daki manifest symlink sinirinin aynisi)
        Write-Host '[MMO_BACKUP_INFO] symlink could not be created (privilege): lock_symlink_rejected is VACUOUS on this machine'
        Check 'bk_lock' 'lock_symlink_rejected' ($true)
    }

    # 16k Yarim goruntu: kopya hatasi (yazim hatasi) manifestsiz birakir
    $sI = Join-Path $b1 'sI'; $rI = Join-Path $b1 'rI'
    New-FakeSaved $sI
    Write-TestBytes (Join-Path $sI 'SaveGames\MMOSave_Zed.sav') (New-SavBytes 120 7)
    $treeI0 = Get-TreeOracle $sI
    $snapI = Join-Path $rI '20261027T180000Z-half'
    $hookBlock = { param($rel, $attempt, $src)
        if ($rel -ceq 'SaveGames/MMOSave_Zed.sav') {
            $dd = Join-Path $snapI 'files\SaveGames'
            [void][IO.Directory]::CreateDirectory($dd)
            [IO.File]::WriteAllBytes((Join-Path $dd 'MMOSave_Zed.sav.partial'), [byte[]]@(1))
        }
    }.GetNewClosure()
    $r = Invoke-TestBackup $sI $rI @{ Timestamp = '20261027T180000Z'; Label = 'half'; TestHooks = @{ DuringCopy = $hookBlock } }
    Check 'bk_incomplete' 'write_failure_exit_1' (($r.Code -eq 1) -and ($r.Out -like '*reason=write_failed*'))
    Check 'bk_incomplete' 'snapshot_dir_exists_without_manifest' ((Test-Path -LiteralPath $snapI -PathType Container) -and -not (Test-Path -LiteralPath (Join-Path $snapI 'manifest.json')) -and -not (Test-Path -LiteralPath (Join-Path $snapI 'manifest.json.tmp')))
    Check 'bk_incomplete' 'earlier_files_were_copied_so_it_is_really_half' (Test-Path -LiteralPath (Join-Path $snapI 'files\MMOAchievements\MMOSave_Ali.json'))
    $rdI = Read-MMOManifest -SnapshotDir $snapI
    $tI = Test-MMOSnapshot -SnapshotDir $snapI
    Check 'bk_incomplete' 'verifier_reports_incomplete' ($rdI.Incomplete -and (-not $rdI.Ok) -and $tI.Incomplete -and (-not $tI.Ok))
    Check 'bk_incomplete' 'log_marks_incomplete' ([IO.File]::ReadAllText((Join-Path $snapI 'backup.log')) -like '*status=incomplete*')
    Check 'bk_incomplete' 'failed_run_did_not_modify_source' ((Get-TreeOracle $sI) -ceq $treeI0)
    $r = Invoke-TestBackup $sI $rI @{ Timestamp = '20261027T180001Z'; Label = 'retry' }
    Check 'bk_incomplete' 'next_run_succeeds_and_half_snapshot_left_alone' (($r.Code -eq 0) -and (Test-Path -LiteralPath $snapI) -and -not (Test-Path -LiteralPath (Join-Path $snapI 'manifest.json')))
    $sEm = Join-Path $b1 'sEm'
    [void][IO.Directory]::CreateDirectory((Join-Path $sEm 'Logs'))
    Write-TestText (Join-Path $sEm 'Logs\a.log') 'x'
    $r = Invoke-TestBackup $sEm (Join-Path $b1 'rEm')
    Check 'bk_incomplete' 'nothing_to_back_up_exit_1_nothing_written' (($r.Code -eq 1) -and ($r.Out -like '*reason=no_files*') -and (NoSnap (Join-Path $b1 'rEm')))

    # 16k2 Zehirli dosya (O5): 8 MB ustu / okunamayan kaynak dosya TUM yedegi iptal etmez; valid=false notes=too_large|read_error kaydedilir, yedek surer
    $sT = Join-Path $b1 'sT'; $rT = Join-Path $b1 'rT'
    New-FakeSaved $sT
    $bigSav = New-Object byte[] 8388609
    $bigSav[0] = 0x47; $bigSav[1] = 0x56; $bigSav[2] = 0x41; $bigSav[3] = 0x53
    Write-TestBytes (Join-Path $sT 'SaveGames\MMOSave_Zed.sav') $bigSav
    [IO.File]::SetLastWriteTimeUtc((Join-Path $sT 'SaveGames\MMOSave_Zed.sav'), (New-Object datetime(2026, 10, 27, 8, 0, 0, [DateTimeKind]::Utc)))
    $treeT0 = Get-TreeOracle $sT
    $r = Invoke-TestBackup $sT $rT @{ Timestamp = '20261027T180100Z'; Label = 'big' }
    $snapT = Join-Path $rT '20261027T180100Z-big'
    $mT = Read-TestManifest $snapT
    $eT = @($mT.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Zed.sav' })
    $tT = Test-MMOSnapshot -SnapshotDir $snapT
    Check 'bk_poison' 'too_large_does_not_abort_exit_4' (($r.Code -eq 4) -and ($r.Out -like '*reason=invalid_files *') -and (Test-Path -LiteralPath (Join-Path $snapT 'manifest.json')))
    Check 'bk_poison' 'too_large_entry_recorded_invalid_without_content' (($eT.Count -eq 1) -and ($eT[0].valid -eq $false) -and ($eT[0].notes -ceq 'too_large') -and ($eT[0].size -eq 0) -and ($eT[0].sha256 -ceq ('0' * 64)))
    Check 'bk_poison' 'too_large_file_absent_from_snapshot' (-not (Test-Path -LiteralPath (Join-Path $snapT 'files\SaveGames\MMOSave_Zed.sav')))
    Check 'bk_poison' 'other_files_still_backed_up' (($mT.entries.Count -eq 12) -and (Test-Path -LiteralPath (Join-Path $snapT 'files\SaveGames\MMOSave_Ali.sav')) -and (Test-Path -LiteralPath (Join-Path $snapT 'files\MMODaily\MMOSave_Ali.json')))
    Check 'bk_poison' 'snapshot_verifies_and_lists_the_poison_file_invalid' ($tT.Ok -and $tT.HasInvalid -and ($tT.InvalidEntries -contains 'SaveGames/MMOSave_Zed.sav') -and ($tT.InvalidEntries.Count -eq 1))
    Check 'bk_poison' 'summary_counts_unreadable' (($r.Out -like '*unreadable=1*') -and ($r.Out -like '*invalid=1*'))
    Check 'bk_poison' 'log_status_complete_with_invalid' ([IO.File]::ReadAllText((Join-Path $snapT 'backup.log')) -like '*status=complete_with_invalid_files*')
    Check 'bk_poison' 'source_untouched' ((Get-TreeOracle $sT) -ceq $treeT0)
    $tfDir = Join-Path $fx 'snap_poison_forged_file'
    Copy-Item -LiteralPath $snapT -Destination $tfDir -Recurse
    Write-TestBytes (Join-Path $tfDir 'files\SaveGames\MMOSave_Zed.sav') (New-SavBytes 100 3)
    $ttf = Test-MMOSnapshot -SnapshotDir $tfDir
    Check 'bk_poison' 'skipped_entry_with_a_file_present_rejected' ((-not $ttf.Ok) -and (HasErr $ttf.Errors 'skipped_entry_has_file:SaveGames/MMOSave_Zed.sav'))
    $tvDir = Join-Path $fx 'snap_poison_forged_valid'
    Copy-Item -LiteralPath $snapT -Destination $tvDir -Recurse
    Edit-TestManifest $tvDir { param($m) ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Zed.sav' }).valid = $true } -FixDigest
    Check 'bk_poison' 'skipped_entry_forged_valid_rejected' (HasErr (Read-MMOManifest -SnapshotDir $tvDir).Errors 'entry')
    $tnDir = Join-Path $fx 'snap_poison_forged_note'
    Copy-Item -LiteralPath $snapA -Destination $tnDir -Recurse
    Edit-TestManifest $tnDir { param($m) $e = ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' }); $e.valid = $false; $e.notes = 'read_error' } -FixDigest
    Check 'bk_poison' 'read_error_note_on_entry_with_real_content_rejected' (HasErr (Read-MMOManifest -SnapshotDir $tnDir).Errors 'entry')
    $sX = Join-Path $b1 'sX'; $rX = Join-Path $b1 'rX'
    New-FakeSaved $sX
    $lockedSrc = [IO.File]::Open((Join-Path $sX 'SaveGames\MMOSave_Ali.sav'), [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $r = Invoke-TestBackup $sX $rX @{ Timestamp = '20261027T181000Z'; Label = 'locked' }
        $mX = Read-TestManifest (Join-Path $rX '20261027T181000Z-locked')
        $eX = @($mX.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' })
        Check 'bk_poison' 'exclusively_locked_source_file_does_not_abort_exit_4' (($r.Code -eq 4) -and ($r.Out -like '*reason=invalid_files *') -and ($eX.Count -eq 1) -and ($eX[0].valid -eq $false) -and ($eX[0].notes -ceq 'read_error'))
        Check 'bk_poison' 'locked_snapshot_verifies' ((Test-MMOSnapshot -SnapshotDir (Join-Path $rX '20261027T181000Z-locked')).Ok)
    } finally { $lockedSrc.Dispose() }

    # 16k3 Cakisma (O5): tum yedegi durdurmaz; iki dosya da yedekte, manifest bildirir (uyari), geri yukleme B3'te reddeder
    $sC = Join-Path $b1 'sC'; $rC = Join-Path $b1 'rC'
    New-FakeSaved $sC
    Write-TestBytes (Join-Path $sC 'SaveGames\MMOSave_Ali_Baba.sav') (New-SavBytes 150 5)
    [IO.File]::SetLastWriteTimeUtc((Join-Path $sC 'SaveGames\MMOSave_Ali_Baba.sav'), (New-Object datetime(2026, 10, 27, 8, 0, 0, [DateTimeKind]::Utc)))
    $r = Invoke-TestBackup $sC $rC @{ Timestamp = '20261027T181100Z'; Label = 'coll' }
    $snapC = Join-Path $rC '20261027T181100Z-coll'
    $mC = Read-TestManifest $snapC
    $tC = Test-MMOSnapshot -SnapshotDir $snapC
    $logC = [IO.File]::ReadAllText((Join-Path $snapC 'backup.log'))
    Check 'bk_collision' 'collision_does_not_abort_exit_4' (($r.Code -eq 4) -and ($r.Out -like '*reason=collision *') -and (Test-Path -LiteralPath (Join-Path $snapC 'manifest.json')))
    Check 'bk_collision' 'both_colliding_files_in_snapshot_and_manifest' ((Test-Path -LiteralPath (Join-Path $snapC 'files\SaveGames\MMOSave_Ali Baba.sav')) -and (Test-Path -LiteralPath (Join-Path $snapC 'files\SaveGames\MMOSave_Ali_Baba.sav')) -and (@($mC.entries | Where-Object { $_.path -like 'SaveGames/MMOSave_Ali*Baba.sav' }).Count -eq 2))
    Check 'bk_collision' 'manifest_declares_the_collision_group' ((@($mC.collisions).Count -eq 1) -and ($mC.collisions[0] -ceq 'SaveGames/MMOSave_Ali Baba.sav|SaveGames/MMOSave_Ali_Baba.sav'))
    Check 'bk_collision' 'snapshot_verifies_with_declared_collision' ($tC.Ok -and ($tC.Collisions.Count -eq 1) -and ($r.Out -like '*collisions=1*'))
    Check 'bk_collision' 'log_has_warning_without_player_names' (($logC -like '*cakisan ad grubu*') -and ($logC -cnotmatch 'Baba|Ali'))
    $tcDir = Join-Path $fx 'snap_collision_undeclared'
    Copy-Item -LiteralPath $snapC -Destination $tcDir -Recurse
    Edit-TestManifest $tcDir { param($m) $m.PSObject.Properties.Remove('collisions') } -FixDigest
    Check 'bk_collision' 'undeclared_collision_still_rejected' (HasErr (Read-MMOManifest -SnapshotDir $tcDir).Errors 'collision:')
    $tdDir = Join-Path $fx 'snap_collision_declared_absent'
    Copy-Item -LiteralPath $snapA -Destination $tdDir -Recurse
    Edit-TestManifest $tdDir { param($m) $m | Add-Member -NotePropertyName collisions -NotePropertyValue @('SaveGames/a.sav|SaveGames/b.sav') -Force } -FixDigest
    Check 'bk_collision' 'declared_but_absent_collision_rejected' (HasErr (Read-MMOManifest -SnapshotDir $tdDir).Errors 'collision_declared_but_absent')
    CheckThrows 'bk_collision' 'library_still_throws_without_allow_flag' { New-MMOManifest -FilesRoot (Join-Path $snapC 'files') -SourceSavedDir $fx } 'collision:'
    Check 'bk_collision' 'restore_selection_refuses_collision_paths_from_manifest' ((Get-MMORestoreSelection -Entries $mC.entries -CollisionPaths @($mC.collisions[0].Split('|'))).Skipped.Reason -contains 'collision')

    # 16l Bozuk kaynak dosya yine yedege girer, sonuc 1
    $sBad = Join-Path $b1 'sBad'; $rBad = Join-Path $b1 'rBad'
    New-FakeSaved $sBad -KeepBad
    $r = Invoke-TestBackup $sBad $rBad @{ Timestamp = '20261027T190000Z'; Label = 'bad' }
    $snapBad = Join-Path $rBad '20261027T190000Z-bad'
    $tBad = Test-MMOSnapshot -SnapshotDir $snapBad
    Check 'bk_invalid' 'corrupt_source_exit_4_with_manifest' (($r.Code -eq 4) -and ($r.Out -like '*reason=invalid_files *') -and (Test-Path -LiteralPath (Join-Path $snapBad 'manifest.json')))
    Check 'bk_invalid' 'corrupt_file_kept_as_evidence_and_listed' ($tBad.Ok -and (($tBad.InvalidEntries -join '|') -ceq 'SaveGames/MMOSave_Bad.sav') -and (Test-Path -LiteralPath (Join-Path $snapBad 'files\SaveGames\MMOSave_Bad.sav')))
    Check 'bk_invalid' 'summary_counts' ($r.Out -like '*invalid=1*')
    Check 'bk_invalid' 'log_status_complete_with_invalid' ([IO.File]::ReadAllText((Join-Path $snapBad 'backup.log')) -like '*status=complete_with_invalid_files*')

    # 16m Kendi dogrulamasi: yazim sonrasi bozulan goruntunun manifesti kaldirilir
    $sH = Join-Path $b1 'sH'; $rH = Join-Path $b1 'rH'
    New-FakeSaved $sH
    $treeH0 = Get-TreeOracle $sH
    $hookCorrupt = { param($snap)
        $f = Join-Path $snap 'files\SaveGames\MMOSave_Ali.sav'
        $bb = [IO.File]::ReadAllBytes($f); $bb[10] = [byte]($bb[10] -bxor 0xFF); [IO.File]::WriteAllBytes($f, $bb)
    }
    $r = Invoke-TestBackup $sH $rH @{ Timestamp = '20261027T200000Z'; Label = 'selfcheck'; TestHooks = @{ AfterManifest = $hookCorrupt } }
    $snapH = Join-Path $rH '20261027T200000Z-selfcheck'
    Check 'bk_selfverify' 'corruption_after_manifest_exit_1' (($r.Code -eq 1) -and ($r.Out -like '*reason=verify_failed*'))
    Check 'bk_selfverify' 'manifest_removed_snapshot_marked_incomplete' ((-not (Test-Path -LiteralPath (Join-Path $snapH 'manifest.json'))) -and (Read-MMOManifest -SnapshotDir $snapH).Incomplete)
    Check 'bk_selfverify' 'rejected_manifest_kept_as_failed_evidence' (Test-Path -LiteralPath (Join-Path $snapH 'manifest.json.failed'))
    Check 'bk_selfverify' 'log_marks_incomplete' ([IO.File]::ReadAllText((Join-Path $snapH 'backup.log')) -like '*status=incomplete*reason=verify_failed*')
    $hookAdd = { param($snap) Write-TestText (Join-Path $snap 'files\SaveGames\MMOSave_Intruder.sav') 'extra' }
    $r = Invoke-TestBackup $sH $rH @{ Timestamp = '20261027T200001Z'; Label = 'extra'; TestHooks = @{ AfterManifest = $hookAdd } }
    Check 'bk_selfverify' 'unlisted_extra_file_after_manifest_fails_verification' (($r.Code -eq 1) -and -not (Test-Path -LiteralPath (Join-Path $rH '20261027T200001Z-extra\manifest.json')))
    Check 'bk_selfverify' 'source_untouched_by_failed_selfverify' ((Get-TreeOracle $sH) -ceq $treeH0)

    # 16m2 Yazilan kopya dogrulanir: .partial yazimdan sonra bozulursa goruntu yarim kalir
    $sPv = Join-Path $b1 'sPv'; $rPv = Join-Path $b1 'rPv'
    New-FakeSaved $sPv
    $treePv0 = Get-TreeOracle $sPv
    $hookPart = { param($rel, $part)
        if ($rel -ceq 'MMODaily/MMOSave_Ali.json') { $bb = [IO.File]::ReadAllBytes($part); $bb[3] = [byte]($bb[3] -bxor 0x55); [IO.File]::WriteAllBytes($part, $bb) }
    }
    $r = Invoke-TestBackup $sPv $rPv @{ Timestamp = '20261027T210000Z'; Label = 'partial'; TestHooks = @{ AfterPartial = $hookPart } }
    $snapPv = Join-Path $rPv '20261027T210000Z-partial'
    Check 'bk_partial' 'corrupted_partial_detected_exit_1' (($r.Code -eq 1) -and ($r.Out -like '*reason=partial_verify_failed*'))
    Check 'bk_partial' 'no_manifest_and_corrupt_partial_never_renamed' ((-not (Test-Path -LiteralPath (Join-Path $snapPv 'manifest.json'))) -and (-not (Test-Path -LiteralPath (Join-Path $snapPv 'files\MMODaily\MMOSave_Ali.json'))) -and (Test-Path -LiteralPath (Join-Path $snapPv 'files\MMODaily\MMOSave_Ali.json.partial')))
    Check 'bk_partial' 'source_untouched' ((Get-TreeOracle $sPv) -ceq $treePv0)

    # 16n Sentetik Saved'in tum B1 kosularinda degismedigi (yerel kanit; gercek Saved kaniti asagida)
    Check 'bk_proof' 'fixture_saved_tree_unchanged_across_all_runs' (((Get-TreeOracle $sA) -ceq $treeA0) -and ((Get-TreeOracle $sR) -ceq $treeR0))

    # ---- 17. B2: VerifySaves.ps1 (salt okuma; yalniz fikstur; goruntuler B1 ile alinmis) -----------------------------------------
    $vfScript = Join-Path $PSScriptRoot 'VerifySaves.ps1'
    $b2 = Join-Path $fx 'b2'
    $b2work = Join-Path $b2 'work'
    [void][IO.Directory]::CreateDirectory($b2work)

    function Invoke-TestVerify {
        param([hashtable]$P)
        foreach ($k in 'Snapshot', 'BackupRoot', 'Against') {
            if ($P.ContainsKey($k) -and -not (Test-MMOPathUnder -Path $P[$k] -Root $fx -AllowEqual)) { throw 'test_guard_verify_path_outside_fixture' }
        }
        if (($P.ContainsKey('All') -or $P.ContainsKey('Last')) -and -not $P.ContainsKey('BackupRoot')) { throw 'test_guard_verify_default_root' }
        $global:LASTEXITCODE = -999
        $lines = @(& $vfScript @P *>&1 | ForEach-Object { [string]$_ })
        return [pscustomobject]@{ Code = $global:LASTEXITCODE; Out = ($lines -join "`n"); Lines = $lines }
    }
    function OutHas {
        param($R, [string]$S)
        return $R.Out.Contains($S)
    }
    function Get-VfTagLines {
        param($R, [string]$Tag)
        $found = @($R.Lines | Where-Object { $_.StartsWith('[MMO_VERIFY' + $Tag + ']') })
        return , $found
    }
    function New-SnapCopy {
        param([string]$Src, [string]$Name)
        $dst = Join-Path $b2work $Name
        Copy-Item -LiteralPath $Src -Destination $dst -Recurse
        return $dst
    }
    function Get-FullStamp {
        # Agacin TAM damgasi: her dizin/dosya (adi, boyutu, LastWriteTime, icerik SHA-256). Reparse point izlenmez (L| satiri).
        param([string]$Root)
        $lines = New-Object System.Collections.ArrayList
        $base = [IO.Path]::GetFullPath($Root).TrimEnd('\')
        $stack = New-Object System.Collections.Stack
        $stack.Push($base)
        while ($stack.Count -gt 0) {
            $d = [string]$stack.Pop()
            foreach ($it in @(Get-ChildItem -LiteralPath $d -Force)) {
                $rel = $it.FullName.Substring($base.Length)
                # Zaman damgasi dizin dizininden (Get-ChildItem) DEGIL, dogrudan sorgudan alinir: NTFS ust dizindeki onbellekli girdiyi gec
                # gunceller, numaralandirma bayat dizin zamani verebilir (bu makinede gozlendi); GetLastWriteTimeUtc dogrudur.
                if (Test-MMOReparse $it.FullName) { [void]$lines.Add('L|' + $rel); continue }
                if ($it.PSIsContainer) { [void]$lines.Add('D|' + $rel + '|' + [IO.Directory]::GetLastWriteTimeUtc($it.FullName).Ticks); $stack.Push($it.FullName) }
                else { [void]$lines.Add('F|' + $rel + '|' + $it.Length + '|' + [IO.File]::GetLastWriteTimeUtc($it.FullName).Ticks + '|' + (Get-OracleSha $it.FullName)) }
            }
        }
        $arr = [string[]]@($lines.ToArray()); [Array]::Sort($arr, [StringComparer]::Ordinal)
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $h = $sha.ComputeHash((New-Object System.Text.UTF8Encoding($false)).GetBytes(($arr -join "`n"))) } finally { $sha.Dispose() }
        return [pscustomobject]@{ Count = $arr.Length; Digest = (($h | ForEach-Object { $_.ToString('x2') }) -join ''); Lines = $arr }
    }

    # 17a Betik yapisi: ayristirma, ASCII, varsayilanlar, YAZMA API'si yok (statik)
    $vperr = $null
    $vast = [System.Management.Automation.Language.Parser]::ParseFile($vfScript, [ref]$null, [ref]$vperr)
    Check 'vf_static' 'script_parses' (($null -eq $vperr) -or ($vperr.Count -eq 0))
    Check 'vf_static' 'script_is_ascii' (-not (([IO.File]::ReadAllBytes($vfScript)) | Where-Object { $_ -gt 127 }))
    $vdef = @{}
    foreach ($pp in $vast.ParamBlock.Parameters) { if ($null -ne $pp.DefaultValue) { $vdef[$pp.Name.VariablePath.UserPath] = $pp.DefaultValue.Extent.Text } }
    Check 'vf_static' 'backup_root_default_matches_backup_script' ($vdef['BackupRoot'] -ceq $pdef['BackupRoot'])
    $forbiddenCmds = @('Set-Content', 'Add-Content', 'Out-File', 'Remove-Item', 'Move-Item', 'Copy-Item', 'New-Item', 'Rename-Item', 'Clear-Content', 'Set-ItemProperty', 'Tee-Object', 'Export-Csv', 'Write-MMOManifest', 'New-MMOManifest')
    $usedCmds = @($vast.FindAll({ param($n) $n -is [System.Management.Automation.Language.CommandAst] }, $true) | ForEach-Object { $_.GetCommandName() } | Where-Object { $null -ne $_ })
    Check 'vf_static' 'no_write_cmdlets_used' (@($usedCmds | Where-Object { $forbiddenCmds -contains $_ }).Count -eq 0)
    $forbiddenMembers = @('WriteAllBytes', 'WriteAllText', 'AppendAllText', 'Move', 'Delete', 'Copy', 'CreateDirectory', 'Create', 'CreateText', 'AppendText', 'Open', 'OpenWrite', 'SetLastWriteTime', 'SetLastWriteTimeUtc', 'SetAttributes', 'WriteByte', 'Write', 'Flush')
    $usedMembers = @($vast.FindAll({ param($n) $n -is [System.Management.Automation.Language.InvokeMemberExpressionAst] }, $true) | ForEach-Object { $_.Member.Extent.Text })
    Check 'vf_static' 'no_write_methods_used' (@($usedMembers | Where-Object { $forbiddenMembers -contains $_ }).Count -eq 0)
    Check 'vf_static' 'no_file_redirection' (@($vast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FileRedirectionAst] }, $true)).Count -eq 0)
    Check 'vf_static' 'no_filestream_objects' (@($vast.FindAll({ param($n) ($n -is [System.Management.Automation.Language.CommandAst]) -and ($n.Extent.Text -match 'FileStream|StreamWriter') }, $true)).Count -eq 0)

    # 17b Fiksturler: B1 ile alinmis goruntuler (iyi, canli, temiz kapanis kanitli, bozuk kaynakli, yetim yan dosyali)
    $sG = Join-Path $b2 'sG'; $rG = Join-Path $b2 'rG'
    New-FakeSaved $sG
    $okG = Invoke-TestBackup $sG $rG @{ Timestamp = '20261027T150000Z'; Label = 'good' }
    $snapG = Join-Path $rG '20261027T150000Z-good'
    $okL = Invoke-TestBackup $sG $rG @{ AllowLive = $true; Timestamp = '20261027T150001Z'; Label = '' }
    $snapLive = Join-Path $rG '20261027T150001Z-live'
    $sP = Join-Path $b2 'sP'; $rP2 = Join-Path $b2 'rP'
    New-FakeSaved $sP
    Write-TestText (Join-Path $sP 'Logs\MMOShutdownResult.json') $shGood
    $okP = Invoke-TestBackup $sP $rP2 @{ RequireCleanShutdown = $true; Timestamp = '20261027T150002Z'; Label = 'proof' }
    $snapProof = Join-Path $rP2 '20261027T150002Z-proof'
    $sBd = Join-Path $b2 'sBd'; $rBd = Join-Path $b2 'rBd'
    New-FakeSaved $sBd -KeepBad
    $okBd = Invoke-TestBackup $sBd $rBd @{ Timestamp = '20261027T150003Z'; Label = 'invalid' }
    $snapBd = Join-Path $rBd '20261027T150003Z-invalid'
    $sOr = Join-Path $b2 'sOr'; $rOr = Join-Path $b2 'rOr'
    New-FakeSaved $sOr
    Write-TestText (Join-Path $sOr 'MMOAchievements\MMOSave_Ghost.json') '{"version":1,"unlocked":[]}'
    $okOr = Invoke-TestBackup $sOr $rOr @{ Timestamp = '20261027T150004Z'; Label = 'orphan' }
    $snapOr = Join-Path $rOr '20261027T150004Z-orphan'
    Check 'vf_setup' 'fixture_snapshots_taken_by_backup_tool' (($okG.Code -eq 0) -and ($okL.Code -eq 4) -and ($okP.Code -eq 0) -and ($okBd.Code -eq 4) -and ($okOr.Code -eq 0))
    $shaG = Get-OracleSha (Join-Path $snapG 'manifest.json')

    # 17c Kullanim hatalari (2): hicbir sey okunmadan reddedilir; ilk satir ozet
    $r = Invoke-TestVerify @{}
    Check 'vf_usage' 'no_target_exit_2' (($r.Code -eq 2) -and (OutHas $r 'reason=no_target') -and (OutHas $r 'status=usage'))
    Check 'vf_usage' 'summary_is_first_line' ($r.Lines[0].StartsWith('[MMO_VERIFY_SUMMARY] '))
    $r = Invoke-TestVerify @{ Snapshot = $snapG; All = $true; BackupRoot = $rG }
    Check 'vf_usage' 'snapshot_and_all_exit_2' (($r.Code -eq 2) -and (OutHas $r 'reason=snapshot_and_root_selectors'))
    $r = Invoke-TestVerify @{ All = $true; Last = 2; BackupRoot = $rG }
    Check 'vf_usage' 'all_and_last_exit_2' (($r.Code -eq 2) -and (OutHas $r 'reason=all_and_last'))
    $r = Invoke-TestVerify @{ Last = -1; BackupRoot = $rG }
    Check 'vf_usage' 'negative_last_exit_2' (($r.Code -eq 2) -and (OutHas $r 'reason=bad_last'))
    $r = Invoke-TestVerify @{ Snapshot = $snapG; ExpectedManifestSha256 = 'abc' }
    Check 'vf_usage' 'bad_expected_format_exit_2' (($r.Code -eq 2) -and (OutHas $r 'reason=bad_expected_sha256'))
    $r = Invoke-TestVerify @{ All = $true; BackupRoot = $rG; ExpectedManifestSha256 = $shaG }
    Check 'vf_usage' 'expected_needs_single_snapshot_exit_2' (($r.Code -eq 2) -and (OutHas $r 'reason=expected_requires_snapshot'))
    $r = Invoke-TestVerify @{ Snapshot = $snapG; ExpectedManifestSha256 = ($shaG + '0') }
    Check 'vf_usage' 'expected_65_chars_exit_2' (($r.Code -eq 2) -and (OutHas $r 'reason=bad_expected_sha256'))

    # 17d Erisilemiyor / ertelendi (3) ve bos kok (1)
    $r = Invoke-TestVerify @{ Snapshot = (Join-Path $b2 'no_such_snapshot') }
    Check 'vf_defer' 'snapshot_missing_exit_3' (($r.Code -eq 3) -and (OutHas $r 'reason=snapshot_missing') -and (OutHas $r 'status=deferred'))
    $aFile = Join-Path $b2 'a_file.txt'
    Write-TestText $aFile 'x'
    $r = Invoke-TestVerify @{ Snapshot = $aFile }
    Check 'vf_defer' 'snapshot_is_a_file_exit_3' (($r.Code -eq 3) -and (OutHas $r 'reason=snapshot_not_directory'))
    $r = Invoke-TestVerify @{ All = $true; BackupRoot = (Join-Path $b2 'no_such_root') }
    Check 'vf_defer' 'backup_root_missing_exit_3' (($r.Code -eq 3) -and (OutHas $r 'reason=backup_root_missing'))
    $rootLink = Join-Path $b2 'root_link'
    New-TestJunction $rootLink $rG
    $r = Invoke-TestVerify @{ All = $true; BackupRoot = $rootLink }
    Check 'vf_defer' 'backup_root_junction_exit_3' (($r.Code -eq 3) -and (OutHas $r 'reason=backup_reparse_root'))
    $r = Invoke-TestVerify @{ Snapshot = $snapG; Against = (Join-Path $b2 'no_such_saved') }
    Check 'vf_defer' 'against_missing_exit_3' (($r.Code -eq 3) -and (OutHas $r 'reason=against_root_missing'))
    $savedLink = Join-Path $b2 'saved_link'
    New-TestJunction $savedLink $sG
    $r = Invoke-TestVerify @{ Snapshot = $snapG; Against = $savedLink }
    Check 'vf_defer' 'against_junction_exit_3' (($r.Code -eq 3) -and (OutHas $r 'reason=against_reparse_root'))
    $rEmpty = Join-Path $b2 'rEmpty'
    [void][IO.Directory]::CreateDirectory($rEmpty)
    Write-TestText (Join-Path $rEmpty '.lock') 'stale'
    Write-TestText (Join-Path $rEmpty 'notes.txt') 'x'
    $r = Invoke-TestVerify @{ All = $true; BackupRoot = $rEmpty }
    Check 'vf_defer' 'root_without_snapshots_exit_1' (($r.Code -eq 1) -and (OutHas $r 'reason=no_snapshots'))

    # 17e Temiz goruntu
    $logG = [IO.File]::ReadAllText((Join-Path $snapG 'backup.log'))
    $r = Invoke-TestVerify @{ Snapshot = $snapG }
    Check 'vf_ok' 'clean_snapshot_exit_0' ($r.Code -eq 0)
    Check 'vf_ok' 'summary_is_first_line_status_ok' ($r.Lines[0].StartsWith('[MMO_VERIFY_SUMMARY] status=ok reason=ok exit=0 snapshots=1 ok=1 bad=0 incomplete=0 with_invalid=0 invalid_entries=0 live=0 warnings='))
    Check 'vf_ok' 'snapshot_line_status_ok_entries_11' (OutHas $r 'name=20261027T150000Z-good status=ok manifest_sha256=')
    Check 'vf_ok' 'manifest_sha256_matches_independent_oracle' (OutHas $r ('manifest_sha256=' + $shaG + ' mode=quiesced shutdown_proof=none entries=11 errors=0 invalid=0'))
    Check 'vf_ok' 'manifest_sha256_equals_backup_log_value' ($logG.Contains('manifest_sha256=' + $shaG))
    Check 'vf_ok' 'no_error_or_invalid_lines' (((Get-VfTagLines $r '_ERROR').Count -eq 0) -and ((Get-VfTagLines $r '_INVALID').Count -eq 0) -and ((Get-VfTagLines $r '_INCOMPLETE').Count -eq 0))
    Check 'vf_ok' 'trailing_backslash_accepted' ((Invoke-TestVerify @{ Snapshot = ($snapG + '\') }).Code -eq 0)

    # 17f Bozulma: dosya, manifest, fazladan/eksik dosya, boyut siniri, reparse
    $w = New-SnapCopy $snapG 'flip'
    $f = Join-Path $w 'files\SaveGames\MMOSave_Ali.sav'; $bb = [IO.File]::ReadAllBytes($f); $bb[10] = [byte]($bb[10] -bxor 0xFF); [IO.File]::WriteAllBytes($f, $bb)
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_hash' 'flipped_byte_exit_1' ($r.Code -eq 1)
    Check 'vf_hash' 'flipped_byte_named_in_error_line' ((Get-VfTagLines $r '_ERROR') -contains '[MMO_VERIFY_ERROR] snap=flip hash_mismatch:SaveGames/MMOSave_Ali.sav')
    Check 'vf_hash' 'flipped_byte_status_bad_and_counted' ((OutHas $r 'name=flip status=bad ') -and (OutHas $r ' bad=1 '))
    $w = New-SnapCopy $snapG 'trunc'
    [IO.File]::WriteAllBytes((Join-Path $w 'files\SaveGames\MMOSave_Ali.sav'), (New-SavBytes 100 1))
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_hash' 'truncated_file_size_mismatch' (($r.Code -eq 1) -and (OutHas $r 'size_mismatch:SaveGames/MMOSave_Ali.sav'))
    $w = New-SnapCopy $snapG 'missing'
    [IO.File]::Delete((Join-Path $w 'files\SaveGames\MMOSave_Ali.sav'))
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_hash' 'missing_file_reported' (($r.Code -eq 1) -and (OutHas $r 'entry_unusable:SaveGames/MMOSave_Ali.sav'))
    $w = New-SnapCopy $snapG 'extra'
    Write-TestText (Join-Path $w 'files\SaveGames\MMOSave_Intruder.sav') 'extra file'
    Write-TestText (Join-Path $w 'files\notes.txt') 'not whitelisted'
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_extra' 'extra_whitelisted_file_exit_1' (($r.Code -eq 1) -and (OutHas $r 'extra_file:SaveGames/MMOSave_Intruder.sav'))
    Check 'vf_extra' 'extra_non_whitelisted_file_reported' (OutHas $r 'extra_file:notes.txt')
    $w = New-SnapCopy $snapG 'extra_clean_control'
    Check 'vf_extra' 'control_copy_without_extra_is_ok' ((Invoke-TestVerify @{ Snapshot = $w }).Code -eq 0)
    $w = New-SnapCopy $snapG 'big'
    [IO.File]::WriteAllBytes((Join-Path $w 'files\SaveGames\MMOSave_Ali Baba.sav'), (New-Object byte[] 8388609))
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_size_limit' 'oversize_declared_file_rejected' (($r.Code -eq 1) -and (OutHas $r 'entry_unusable:SaveGames/MMOSave_Ali Baba.sav:too_large'))
    $w = New-SnapCopy $snapG 'bigmanifest'
    [IO.File]::WriteAllBytes((Join-Path $w 'manifest.json'), (New-Object byte[] 8388609))
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_size_limit' 'oversize_manifest_rejected' (($r.Code -eq 1) -and (OutHas $r 'manifest_too_large'))
    $w = New-SnapCopy $snapG 'tamper_digest'
    Edit-TestManifest $w { param($m) $m.entries[0].sha256 = ('0' * 64) }
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_manifest' 'entry_sha_edit_without_digest_fix_rejected' (($r.Code -eq 1) -and (OutHas $r 'digest_mismatch'))
    $w = New-SnapCopy $snapG 'tamper_sha_fixed'
    Edit-TestManifest $w { param($m) $m.entries[0].sha256 = ('0' * 64) } -FixDigest
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_manifest' 'entry_sha_edit_with_digest_fixed_hash_mismatch' (($r.Code -eq 1) -and (OutHas $r 'hash_mismatch:'))
    $w = New-SnapCopy $snapG 'tamper_valid'
    Edit-TestManifest $w { param($m) ($m.entries | Where-Object { $_.path -ceq 'MMODaily/MMOSave_Ali.json' }).valid = $false } -FixDigest
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_manifest' 'valid_false_tamper_on_good_file_rejected' (($r.Code -eq 1) -and (OutHas $r 'valid_mismatch:MMODaily/MMOSave_Ali.json'))
    $w = New-SnapCopy $snapBd 'tamper_valid_true'
    Edit-TestManifest $w { param($m) ($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Bad.sav' }).valid = $true } -FixDigest
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_manifest' 'valid_true_tamper_on_corrupt_file_rejected' (($r.Code -eq 1) -and (OutHas $r 'valid_mismatch:SaveGames/MMOSave_Bad.sav'))
    $w = New-SnapCopy $snapG 'tamper_header'
    Edit-TestManifest $w { param($m) $m.mode = 'live' }
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_manifest' 'header_edit_rejected' (($r.Code -eq 1) -and (OutHas $r 'header_digest_mismatch'))
    $w = New-SnapCopy $snapG 'garbage_manifest'
    Write-TestText (Join-Path $w 'manifest.json') 'this is not json'
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_manifest' 'garbage_manifest_is_bad_not_incomplete' (($r.Code -eq 1) -and (OutHas $r 'manifest_unreadable') -and (OutHas $r 'name=garbage_manifest status=bad '))
    $w = New-SnapCopy $snapG 'junction_inside'
    $decoy = Join-Path $b2 'decoy'
    Write-TestText (Join-Path $decoy 'secret.txt') 'must stay untouched'
    $decoyHash = Get-TreeOracle $decoy
    New-TestJunction (Join-Path $w 'files\MMODaily\evil') $decoy
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_reparse' 'junction_inside_files_fails_closed' (($r.Code -eq 1) -and (OutHas $r 'tree_unusable:'))
    Check 'vf_reparse' 'junction_target_untouched' ((Get-TreeOracle $decoy) -ceq $decoyHash)
    $snapLink = Join-Path $b2 'snap_link'
    New-TestJunction $snapLink $snapG
    $r = Invoke-TestVerify @{ Snapshot = $snapLink }
    Check 'vf_reparse' 'snapshot_dir_is_junction_fails_closed' (($r.Code -eq 1) -and (OutHas $r 'reparse_snapshot'))

    # 17g -ExpectedManifestSha256 (disaridan beklenen ozet)
    $r = Invoke-TestVerify @{ Snapshot = $snapG; ExpectedManifestSha256 = $shaG }
    Check 'vf_expected' 'matching_digest_exit_0' ($r.Code -eq 0)
    $r = Invoke-TestVerify @{ Snapshot = $snapG; ExpectedManifestSha256 = $shaG.ToUpperInvariant() }
    Check 'vf_expected' 'upper_case_digest_accepted' ($r.Code -eq 0)
    $r = Invoke-TestVerify @{ Snapshot = $snapG; ExpectedManifestSha256 = ('1' * 64) }
    Check 'vf_expected' 'wrong_digest_exit_1' (($r.Code -eq 1) -and ((Get-VfTagLines $r '_ERROR') -contains ('[MMO_VERIFY_ERROR] snap=20261027T150000Z-good expected_digest_mismatch:expected=' + ('1' * 64) + ':actual=' + $shaG)))
    Check 'vf_expected' 'wrong_digest_snapshot_marked_bad' (OutHas $r 'name=20261027T150000Z-good status=bad ')
    $w = New-SnapCopy $snapG 'rewritten_consistent'
    Edit-TestManifest $w { param($m) $m.source_saved_dir = 'C:\elsewhere\Saved' } -FixDigest
    $r0 = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_expected' 'consistent_rewrite_passes_without_expected_digest_known_limit' ($r0.Code -eq 0)
    $r = Invoke-TestVerify @{ Snapshot = $w; ExpectedManifestSha256 = $shaG }
    Check 'vf_expected' 'consistent_rewrite_caught_by_expected_digest' (($r.Code -eq 1) -and (OutHas $r 'expected_digest_mismatch:'))
    $w = New-SnapCopy $snapG 'expected_on_incomplete'
    [IO.File]::Delete((Join-Path $w 'manifest.json'))
    $r = Invoke-TestVerify @{ Snapshot = $w; ExpectedManifestSha256 = $shaG }
    Check 'vf_expected' 'expected_on_incomplete_snapshot_mismatch' (($r.Code -eq 1) -and (OutHas $r 'expected_digest_mismatch:') -and (OutHas $r 'manifest_sha256=none'))

    $w = New-SnapCopy $snapG 'changed_during'
    $hookMan = { param($d) Edit-TestManifest $d { param($m) $m.source_saved_dir = 'C:\other\Saved' } -FixDigest }
    $r = Invoke-TestVerify @{ Snapshot = $w; TestHooks = @{ AfterVerify = $hookMan } }
    Check 'vf_manifest' 'manifest_changed_during_verification_rejected' (($r.Code -eq 1) -and (OutHas $r 'manifest_changed_during_verify'))
    Check 'vf_manifest' 'changed_during_reports_digest_seen_at_start' (OutHas $r ('manifest_sha256=' + $shaG + ' '))

    # 17h Yarim goruntu: 'incomplete' acikca raporlanir
    $w = New-SnapCopy $snapG 'no_manifest'
    [IO.File]::Delete((Join-Path $w 'manifest.json'))
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_incomplete' 'missing_manifest_reported_incomplete_exit_1' (($r.Code -eq 1) -and (OutHas $r 'name=no_manifest status=incomplete ') -and (OutHas $r ' incomplete=1 '))
    Check 'vf_incomplete' 'incomplete_line_has_reason' ((Get-VfTagLines $r '_INCOMPLETE') -contains '[MMO_VERIFY_INCOMPLETE] snap=no_manifest reasons=manifest_missing')
    Check 'vf_incomplete' 'incomplete_is_not_counted_as_ok_or_bad' ((OutHas $r ' ok=0 bad=0 incomplete=1 '))
    $w = New-SnapCopy $snapG 'failed_manifest'
    [IO.File]::Move((Join-Path $w 'manifest.json'), (Join-Path $w 'manifest.json.failed'))
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_incomplete' 'failed_manifest_reported' (($r.Code -eq 1) -and (OutHas $r 'status=incomplete') -and (OutHas $r 'reasons=manifest_missing,manifest_failed'))
    $sI2 = Join-Path $b2 'sI2'; $rI2 = Join-Path $b2 'rI2'
    New-FakeSaved $sI2
    $hookPart2 = { param($rel, $part)
        if ($rel -ceq 'MMODaily/MMOSave_Ali.json') { $b3 = [IO.File]::ReadAllBytes($part); $b3[3] = [byte]($b3[3] -bxor 0x55); [IO.File]::WriteAllBytes($part, $b3) }
    }
    $rb = Invoke-TestBackup $sI2 $rI2 @{ Timestamp = '20261027T160000Z'; Label = 'partial'; TestHooks = @{ AfterPartial = $hookPart2 } }
    $r = Invoke-TestVerify @{ Snapshot = (Join-Path $rI2 '20261027T160000Z-partial') }
    Check 'vf_incomplete' 'real_interrupted_backup_reported_incomplete' (($rb.Code -eq 1) -and ($r.Code -eq 1) -and (OutHas $r 'status=incomplete'))
    Check 'vf_incomplete' 'partial_files_counted_in_reasons' (OutHas $r 'partial_files=1')
    $hookCorrupt2 = { param($snap)
        $f2 = Join-Path $snap 'files\SaveGames\MMOSave_Ali.sav'
        $b4 = [IO.File]::ReadAllBytes($f2); $b4[10] = [byte]($b4[10] -bxor 0xFF); [IO.File]::WriteAllBytes($f2, $b4)
    }
    $rb = Invoke-TestBackup $sI2 $rI2 @{ Timestamp = '20261027T160001Z'; Label = 'selfcheck'; TestHooks = @{ AfterManifest = $hookCorrupt2 } }
    $r = Invoke-TestVerify @{ Snapshot = (Join-Path $rI2 '20261027T160001Z-selfcheck') }
    Check 'vf_incomplete' 'real_failed_selfverify_reported_incomplete_with_failed_manifest' (($rb.Code -eq 1) -and ($r.Code -eq 1) -and (OutHas $r 'status=incomplete') -and (OutHas $r 'manifest_failed'))
    $w = New-SnapCopy $snapG 'partial_leftover'
    Write-TestText (Join-Path $w 'files\SaveGames\MMOSave_Ali.sav.partial') 'leftover'
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_incomplete' 'partial_leftover_next_to_manifest_is_bad' (($r.Code -eq 1) -and (OutHas $r 'partial_leftover:1') -and (OutHas $r 'name=partial_leftover status=bad '))
    $w = Join-Path $b2work 'empty_dir'
    [void][IO.Directory]::CreateDirectory($w)
    $r = Invoke-TestVerify @{ Snapshot = $w }
    Check 'vf_incomplete' 'empty_directory_reported_incomplete' (($r.Code -eq 1) -and (OutHas $r 'status=incomplete'))

    # 17i mode=live, shutdown_proof, valid=false girisler, uyarilar
    $r = Invoke-TestVerify @{ Snapshot = $snapLive }
    Check 'vf_modes' 'live_snapshot_exit_0_with_warning' (($r.Code -eq 0) -and (OutHas $r 'mode=live') -and (OutHas $r '[MMO_VERIFY_WARN] snap=20261027T150001Z-live mode=live') -and (OutHas $r ' live=1 '))
    $r = Invoke-TestVerify @{ Snapshot = $snapProof }
    Check 'vf_modes' 'shutdown_proof_recorded_reported' (($r.Code -eq 0) -and (OutHas $r 'shutdown_proof=recorded') -and (OutHas $r '[MMO_VERIFY_PROOF] snap=20261027T150002Z-proof run_id=20261027T100000Z-1234 exit_code=0 time_utc=2026-10-27T10:00:00Z'))
    $r = Invoke-TestVerify @{ Snapshot = $snapG }
    Check 'vf_modes' 'quiesced_snapshot_has_no_live_warning_or_proof_line' ((-not (OutHas $r 'mode=live')) -and ((Get-VfTagLines $r '_PROOF').Count -eq 0))
    $r = Invoke-TestVerify @{ Snapshot = $snapBd }
    Check 'vf_invalid' 'invalid_entry_exit_1' ($r.Code -eq 1)
    Check 'vf_invalid' 'invalid_entry_listed_by_path' ((Get-VfTagLines $r '_INVALID') -contains '[MMO_VERIFY_INVALID] snap=20261027T150003Z-invalid path=SaveGames/MMOSave_Bad.sav (geri yuklenmeyecek)')
    Check 'vf_invalid' 'invalid_snapshot_status_and_counts' ((OutHas $r 'status=complete_with_invalid_files') -and (OutHas $r ' with_invalid=1 invalid_entries=1 ') -and (OutHas $r ' errors=0 invalid=1'))
    $r = Invoke-TestVerify @{ Snapshot = $snapG }
    $wl = Get-VfTagLines $r '_WARN'
    Check 'vf_warn' 'bad_lock_files_warned' (($wl -contains '[MMO_VERIFY_WARN] snap=20261027T150000Z-good lock_file=MMOAchievements/MMOSave_Ali.json.bad') -and ($wl -contains '[MMO_VERIFY_WARN] snap=20261027T150000Z-good lock_file=MMORace/MMORace_mmosave_ali.json.bad'))
    Check 'vf_warn' 'tmp_and_bak_side_files_warned' (($wl -contains '[MMO_VERIFY_WARN] snap=20261027T150000Z-good side_file=MMODaily/MMOSave_Ali.json.tmp') -and ($wl -contains '[MMO_VERIFY_WARN] snap=20261027T150000Z-good side_file=MMOMail/MMOMail_ali.json.bak'))
    Check 'vf_warn' 'warnings_do_not_fail_and_are_counted' (($r.Code -eq 0) -and (OutHas $r ' warnings=4 '))
    Check 'vf_warn' 'no_orphan_warning_for_complete_groups' (-not (OutHas $r 'orphan_group='))
    $r = Invoke-TestVerify @{ Snapshot = $snapOr }
    Check 'vf_warn' 'orphan_side_file_group_warned' (($r.Code -eq 0) -and (OutHas $r 'orphan_group=ghost'))

    # 17j Kok secimi: -All, -Last N (zaman damgasina gore, ada gore degil), yabanci girdiler, bozuk/junction/dosya girdisi
    $rSel = Join-Path $b2 'sel'
    [void][IO.Directory]::CreateDirectory($rSel)
    foreach ($nm in '20261101T000001Z-a', '20261101T000002Z-b', 'pre-restore-20261101T000000Z-p', '20261101T000004Z-c') {
        Copy-Item -LiteralPath $snapG -Destination (Join-Path $rSel $nm) -Recurse
    }
    Write-TestText (Join-Path $rSel 'notes.txt') 'foreign file'
    [void][IO.Directory]::CreateDirectory((Join-Path $rSel 'random'))
    $r = Invoke-TestVerify @{ BackupRoot = $rSel; All = $true }
    Check 'vf_select' 'all_verifies_every_snapshot' (($r.Code -eq 0) -and (OutHas $r ' snapshots=4 ok=4 ') -and ((Get-VfTagLines $r '_SNAPSHOT').Count -eq 4))
    Check 'vf_select' 'foreign_entries_counted_not_verified' (OutHas $r ' foreign=2')
    $r = Invoke-TestVerify @{ BackupRoot = $rSel; Last = 2 }
    Check 'vf_select' 'last_2_picks_two_newest_by_timestamp' (($r.Code -eq 0) -and (OutHas $r ' snapshots=2 ') -and (OutHas $r 'name=20261101T000002Z-b ') -and (OutHas $r 'name=20261101T000004Z-c ') -and -not (OutHas $r 'name=pre-restore') -and -not (OutHas $r 'name=20261101T000001Z-a '))
    $r = Invoke-TestVerify @{ BackupRoot = $rSel; Last = 1 }
    Check 'vf_select' 'last_1_is_newest' (($r.Code -eq 0) -and (OutHas $r ' snapshots=1 ') -and (OutHas $r 'name=20261101T000004Z-c '))
    $r = Invoke-TestVerify @{ BackupRoot = $rSel; Last = 10 }
    Check 'vf_select' 'last_larger_than_count_takes_all' (OutHas $r ' snapshots=4 ')
    $fb = Join-Path $rSel '20261101T000002Z-b\files\SaveGames\MMOSave_Ali.sav'
    $bb = [IO.File]::ReadAllBytes($fb); $bb[20] = [byte]($bb[20] -bxor 0xFF); [IO.File]::WriteAllBytes($fb, $bb)
    $r = Invoke-TestVerify @{ BackupRoot = $rSel; All = $true }
    Check 'vf_select' 'one_bad_among_four_fails_run' (($r.Code -eq 1) -and (OutHas $r ' snapshots=4 ok=3 bad=1 ') -and (OutHas $r 'snap=20261101T000002Z-b hash_mismatch:SaveGames/MMOSave_Ali.sav'))
    $r = Invoke-TestVerify @{ BackupRoot = $rSel; Last = 1 }
    Check 'vf_select' 'bad_snapshot_outside_window_not_examined' ($r.Code -eq 0)
    $rSel2 = Join-Path $b2 'sel2'
    [void][IO.Directory]::CreateDirectory($rSel2)
    Copy-Item -LiteralPath $snapG -Destination (Join-Path $rSel2 '20261102T000001Z-ok') -Recurse
    New-TestJunction (Join-Path $rSel2 '20261102T000002Z-link') $snapG
    Write-TestText (Join-Path $rSel2 '20261102T000003Z-file') 'a file with a snapshot-like name'
    $r = Invoke-TestVerify @{ BackupRoot = $rSel2; All = $true }
    Check 'vf_select' 'junction_and_file_entries_fail_closed' (($r.Code -eq 1) -and (OutHas $r ' snapshots=3 ok=1 bad=2 ') -and (OutHas $r 'snap=20261102T000002Z-link reparse_snapshot') -and (OutHas $r 'snap=20261102T000003Z-file not_a_directory'))
    Check 'vf_select' 'junction_target_snapshot_untouched' ((Invoke-TestVerify @{ Snapshot = $snapG }).Code -eq 0)

    # 17k -Against: goruntu ile canli agac farki (yalniz ad/boyut/hash; canli agaca yazilmaz)
    $r = Invoke-TestVerify @{ Snapshot = $snapG; Against = $sG }
    Check 'vf_against' 'identical_saved_all_same' (($r.Code -eq 0) -and ((Get-VfTagLines $r '_AGAINST') -contains '[MMO_VERIFY_AGAINST] snap=20261027T150000Z-good same=11 changed=0 only_in_snapshot=0 only_in_saved=0') -and ((Get-VfTagLines $r '_DIFF').Count -eq 0))
    $sAg2 = Join-Path $b2 'sAg2'
    New-FakeSaved $sAg2
    Write-TestBytes (Join-Path $sAg2 'SaveGames\MMOSave_Ali.sav') (New-SavBytes 222 9)
    Write-TestBytes (Join-Path $sAg2 'SaveGames\MMOSave_New.sav') (New-SavBytes 90 4)
    [IO.File]::Delete((Join-Path $sAg2 'MMOMail\MMOMail_ali.json.bak'))
    Write-TestText (Join-Path $sAg2 'SaveGames\notes.txt') 'not whitelisted, must not be diffed'
    $treeAg0 = Get-TreeOracle $sAg2
    $r = Invoke-TestVerify @{ Snapshot = $snapG; Against = $sAg2 }
    $dl = Get-VfTagLines $r '_DIFF'
    Check 'vf_against' 'differences_are_reported_not_failures_exit_0' (($r.Code -eq 0) -and ((Get-VfTagLines $r '_AGAINST') -contains '[MMO_VERIFY_AGAINST] snap=20261027T150000Z-good same=9 changed=1 only_in_snapshot=1 only_in_saved=1'))
    Check 'vf_against' 'changed_file_with_both_sizes' ($dl -contains '[MMO_VERIFY_DIFF] snap=20261027T150000Z-good kind=changed path=SaveGames/MMOSave_Ali.sav size_snapshot=200 size_saved=222')
    Check 'vf_against' 'only_in_snapshot_and_only_in_saved' (($dl -contains '[MMO_VERIFY_DIFF] snap=20261027T150000Z-good kind=only_in_snapshot path=MMOMail/MMOMail_ali.json.bak') -and ($dl -contains '[MMO_VERIFY_DIFF] snap=20261027T150000Z-good kind=only_in_saved path=SaveGames/MMOSave_New.sav'))
    Check 'vf_against' 'non_whitelisted_saved_file_not_diffed' (-not (OutHas $r 'notes.txt'))
    Check 'vf_against' 'saved_tree_unchanged_after_against' ((Get-TreeOracle $sAg2) -ceq $treeAg0)
    $wBadMan = New-SnapCopy $snapG 'against_bad_manifest'
    Write-TestText (Join-Path $wBadMan 'manifest.json') 'garbage'
    $sAg4 = Join-Path $b2 'sAg4'
    New-FakeSaved $sAg4
    $fAg4 = Join-Path $sAg4 'MMOMail\MMOMail_ali.json'
    $bAg4 = [IO.File]::ReadAllBytes($fAg4); $bAg4[5] = [byte]($bAg4[5] -bxor 0x01); [IO.File]::WriteAllBytes($fAg4, $bAg4)
    $r = Invoke-TestVerify @{ Snapshot = $snapG; Against = $sAg4 }
    Check 'vf_against' 'same_size_different_content_is_changed' (($r.Code -eq 0) -and ((Get-VfTagLines $r '_DIFF') -contains ('[MMO_VERIFY_DIFF] snap=20261027T150000Z-good kind=changed path=MMOMail/MMOMail_ali.json size_snapshot=' + $bAg4.Length + ' size_saved=' + $bAg4.Length)) -and (OutHas $r ' same=10 changed=1 '))
    $r = Invoke-TestVerify @{ Snapshot = $wBadMan; Against = $sAg2 }
    Check 'vf_against' 'unusable_manifest_skips_diff' (($r.Code -eq 1) -and (OutHas $r '[MMO_VERIFY_AGAINST] snap=against_bad_manifest skipped=manifest_not_usable'))
    $r = Invoke-TestVerify @{ BackupRoot = $rSel; All = $true; Against = $sG }
    Check 'vf_against' 'one_against_line_per_snapshot' ((Get-VfTagLines $r '_AGAINST').Count -eq 4)
    $sAg3 = Join-Path $b2 'sAg3'
    New-FakeSaved $sAg3
    New-TestJunction (Join-Path $sAg3 'MMOMail\jx') $decoy
    $r = Invoke-TestVerify @{ Snapshot = $snapG; Against = $sAg3 }
    Check 'vf_against' 'junction_entry_in_saved_exit_3' (($r.Code -eq 3) -and (OutHas $r 'reason=against_reparse_entry'))
    Check 'vf_against' 'junction_target_still_untouched' ((Get-TreeOracle $decoy) -ceq $decoyHash)
    $lockPathLive = Join-Path $sAg2 'SaveGames\MMOSave_Ali Baba.sav'
    $lockFsV = New-Object IO.FileStream($lockPathLive, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
    try { $r = Invoke-TestVerify @{ Snapshot = $snapG; Against = $sAg2 } } finally { $lockFsV.Dispose() }
    Check 'vf_against' 'unreadable_live_file_fails_exit_1' (($r.Code -eq 1) -and (OutHas $r 'kind=saved_unreadable path=SaveGames/MMOSave_Ali Baba.sav') -and (OutHas $r 'reason=against_unreadable'))

    # 17l SALT OKUNURLUK KANITI: tum goruntu/yedek koku/Saved fiksturleri uzerinde dogrulama bataryasi; oncesi/sonrasi tam damga (icerik hash + zaman)
    $stB2a = Get-FullStamp $b2
    $treeSG0 = Get-TreeOracle $sG
    $battery = @(
        @{ Snapshot = $snapG }, @{ Snapshot = $snapLive }, @{ Snapshot = $snapProof }, @{ Snapshot = $snapBd }, @{ Snapshot = $snapOr },
        @{ Snapshot = $snapG; ExpectedManifestSha256 = $shaG }, @{ Snapshot = $snapG; ExpectedManifestSha256 = ('2' * 64) },
        @{ Snapshot = $snapG; Against = $sAg2 }, @{ Snapshot = $snapG; Against = $sG },
        @{ BackupRoot = $rSel; All = $true }, @{ BackupRoot = $rSel; Last = 2; Against = $sG }, @{ BackupRoot = $rSel2; All = $true },
        @{ BackupRoot = $rG; All = $true }, @{ BackupRoot = $rI2; All = $true }, @{ BackupRoot = $rEmpty; All = $true },
        @{ Snapshot = (Join-Path $b2work 'no_manifest') }, @{ Snapshot = (Join-Path $b2work 'junction_inside') }, @{ Snapshot = (Join-Path $b2work 'big') },
        @{ Snapshot = (Join-Path $b2work 'flip') }, @{ Snapshot = $snapLink }, @{ Snapshot = (Join-Path $b2 'no_such_snapshot') }, @{}
    )
    $battCodes = New-Object System.Collections.ArrayList
    foreach ($bp in $battery) { [void]$battCodes.Add((Invoke-TestVerify $bp).Code) }
    $stB2b = Get-FullStamp $b2
    Check 'vf_readonly' 'battery_ran_with_expected_mixed_exit_codes' (($battCodes.ToArray() -join ',') -ceq '0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,1,1,1,1,1,3,2')
    Check 'vf_readonly' 'battery_covered_22_runs' ($battCodes.Count -eq 22)
    Check 'vf_readonly' 'whole_fixture_tree_byte_and_time_identical_after_verification' (($stB2a.Digest -ceq $stB2b.Digest) -and ($stB2a.Count -eq $stB2b.Count) -and ($stB2a.Count -gt 100))
    Check 'vf_readonly' 'no_lock_log_or_temp_files_created_in_backup_roots' ((-not (Test-Path -LiteralPath (Join-Path $rG '.lock'))) -and (@(Get-ChildItem -LiteralPath $b2 -Recurse -Force -File | Where-Object { ($_.Directory.Name -cne 'rEmpty') -and ($_.Name -like '*.lock' -or $_.Name -like 'verify*' -or $_.Name -like '*.restore.tmp') }).Count -eq 0))
    Check 'vf_readonly' 'source_saved_fixtures_unchanged' (((Get-TreeOracle $sG) -ceq $treeSG0) -and ((Get-TreeOracle $sAg2) -ceq $treeAg0))

    # ---- 16o. B1.1 sertlestirme kapilari (O1..O6, D1..D7) -------------------------------------------------------------------
    $oldStamp = New-Object datetime(2026, 10, 27, 8, 0, 0, [DateTimeKind]::Utc)
    $benignCopy = $benign

    # 16o-1 O1: manifest ozeti YAZILAN bayttan (copy_drift)
    $sDr = Join-Path $b1 'sDr'; $rDr = Join-Path $b1 'rDr'
    New-FakeSaved $sDr
    $treeDr0 = Get-TreeOracle $sDr
    $hookDrift = { param($snap)
        $f = Join-Path $snap 'files\SaveGames\MMOSave_Ali.sav'
        $bb = [IO.File]::ReadAllBytes($f); $bb[20] = [byte]($bb[20] -bxor 0x5A); [IO.File]::WriteAllBytes($f, $bb)
    }
    $r = Invoke-TestBackup $sDr $rDr @{ Timestamp = '20261027T200100Z'; Label = 'drift'; TestHooks = @{ BeforeManifest = $hookDrift } }
    $snapDr = Join-Path $rDr '20261027T200100Z-drift'
    $logDr = [IO.File]::ReadAllText((Join-Path $snapDr 'backup.log'))
    Check 'bk_drift' 'content_changed_after_copy_is_copy_drift_exit_1' (($r.Code -eq 1) -and ($r.Out -like '*reason=copy_drift:SaveGames/MMOSave_Ali.sav*'))
    Check 'bk_drift' 'no_manifest_snapshot_incomplete' ((-not (Test-Path -LiteralPath (Join-Path $snapDr 'manifest.json'))) -and (Read-MMOManifest -SnapshotDir $snapDr).Incomplete -and ($logDr -like '*status=incomplete*'))
    Check 'bk_drift' 'log_has_reason_code_only_no_player_name' (($logDr -like '*reason=copy_drift *') -and ($logDr -cnotmatch 'Ali|MMOSave'))
    Check 'bk_drift' 'source_untouched' ((Get-TreeOracle $sDr) -ceq $treeDr0)
    $hookDel = { param($snap) [IO.File]::Delete((Join-Path $snap 'files\MMODaily\MMOSave_Ali.json')) }
    $r = Invoke-TestBackup $sDr $rDr @{ Timestamp = '20261027T200101Z'; Label = 'drift-del'; TestHooks = @{ BeforeManifest = $hookDel } }
    Check 'bk_drift' 'file_removed_after_copy_is_copy_drift' (($r.Code -eq 1) -and ($r.Out -like '*reason=copy_drift:*') -and -not (Test-Path -LiteralPath (Join-Path $rDr '20261027T200101Z-drift-del\manifest.json')))
    $hookAdd2 = { param($snap) Write-TestBytes (Join-Path $snap 'files\SaveGames\MMOSave_Intruder.sav') (New-SavBytes 100 9) }
    $r = Invoke-TestBackup $sDr $rDr @{ Timestamp = '20261027T200102Z'; Label = 'drift-add'; TestHooks = @{ BeforeManifest = $hookAdd2 } }
    Check 'bk_drift' 'unlisted_whitelisted_file_added_after_copy_is_copy_drift' (($r.Code -eq 1) -and ($r.Out -like '*reason=copy_drift:SaveGames/MMOSave_Intruder.sav*'))
    $r = Invoke-TestBackup $sDr $rDr @{ Timestamp = '20261027T200103Z'; Label = 'nodrift' }
    Check 'bk_drift' 'untouched_copy_has_no_drift_exit_0' ($r.Code -eq 0)
    $edBase = Join-Path $fx 'drift_lib'
    [void][IO.Directory]::CreateDirectory($edBase)
    New-BaseFiles $edBase
    $edRef = New-MMOManifest -FilesRoot $edBase -SourceSavedDir $fx -CreatedUtc '2026-10-27T10:00:00Z'
    $edTab = @{}
    foreach ($e in $edRef.entries) { $edTab[$e.path] = @{ Size = [long]$e.size; Sha256 = [string]$e.sha256; Valid = [bool]$e.valid; Notes = [string]$e.notes } }
    $edOk = New-MMOManifest -FilesRoot $edBase -SourceSavedDir $fx -CreatedUtc '2026-10-27T10:00:00Z' -ExpectedEntries $edTab
    Check 'bk_drift' 'library_expected_table_matching_passes_and_digest_equal' ($edOk.entries_digest -ceq $edRef.entries_digest)
    $edBad = @{}; foreach ($k in $edTab.Keys) { $edBad[$k] = @{ Size = $edTab[$k].Size; Sha256 = $edTab[$k].Sha256; Valid = $edTab[$k].Valid; Notes = $edTab[$k].Notes } }
    $edBad['SaveGames/MMOSave_Ali.sav'].Sha256 = ('a' * 64)
    CheckThrows 'bk_drift' 'library_hash_mismatch_throws' { New-MMOManifest -FilesRoot $edBase -SourceSavedDir $fx -ExpectedEntries $edBad } 'copy_drift:SaveGames/MMOSave_Ali.sav'
    $edBad['SaveGames/MMOSave_Ali.sav'].Sha256 = $edTab['SaveGames/MMOSave_Ali.sav'].Sha256
    $edBad['SaveGames/MMOSave_Ali.sav'].Valid = (-not $edBad['SaveGames/MMOSave_Ali.sav'].Valid)
    CheckThrows 'bk_drift' 'library_valid_mismatch_throws' { New-MMOManifest -FilesRoot $edBase -SourceSavedDir $fx -ExpectedEntries $edBad } 'copy_drift:SaveGames/MMOSave_Ali.sav'
    $edBad['SaveGames/MMOSave_Ali.sav'].Valid = $edTab['SaveGames/MMOSave_Ali.sav'].Valid
    $edBad['SaveGames/MMOSave_Ghost.sav'] = @{ Size = 1L; Sha256 = ('b' * 64); Valid = $true; Notes = '' }
    CheckThrows 'bk_drift' 'library_expected_but_missing_file_throws' { New-MMOManifest -FilesRoot $edBase -SourceSavedDir $fx -ExpectedEntries $edBad } 'copy_drift:SaveGames/MMOSave_Ghost.sav'

    # 16o-2 O2: quiesced iddiasi kopyadan SONRA yeniden denetlenir; proof_stale
    $sQ = Join-Path $b1 'sQ'; $rQ = Join-Path $b1 'rQ'
    New-FakeSaved $sQ
    $treeQ0 = Get-TreeOracle $sQ
    $global:provCalls = 0
    $provBack = { $global:provCalls = $global:provCalls + 1
        if ($global:provCalls -ge 2) { @([pscustomobject]@{ Name = 'explorer.exe'; ProcessId = 1; CommandLine = '' }, [pscustomobject]@{ Name = 'UnrealEditor.exe'; ProcessId = 9; CommandLine = '' }) }
        else { @([pscustomobject]@{ Name = 'explorer.exe'; ProcessId = 1; CommandLine = '' }) } }
    $r = Invoke-TestBackup $sQ $rQ @{ Timestamp = '20261027T200200Z'; Label = 'proc'; ProcessProvider = $provBack }
    $snapQ = Join-Path $rQ '20261027T200200Z-proc'
    Check 'bk_quiesce' 'process_back_after_copy_exit_1_quiesced_violated' (($r.Code -eq 1) -and ($r.Out -like '*reason=quiesced_violated*') -and ($r.Out -like '*quiesced_violated cause=process_running*') -and ($global:provCalls -eq 2))
    Check 'bk_quiesce' 'no_manifest_incomplete_and_log_status' ((-not (Test-Path -LiteralPath (Join-Path $snapQ 'manifest.json'))) -and ([IO.File]::ReadAllText((Join-Path $snapQ 'backup.log')) -like '*status=incomplete*reason=quiesced_violated*'))
    $global:provCalls = 0
    $provThrow = { $global:provCalls = $global:provCalls + 1
        if ($global:provCalls -ge 2) { throw 'cim failed' }
        @([pscustomobject]@{ Name = 'explorer.exe'; ProcessId = 1; CommandLine = '' }) }
    $r = Invoke-TestBackup $sQ $rQ @{ Timestamp = '20261027T200201Z'; Label = 'procq'; ProcessProvider = $provThrow }
    Check 'bk_quiesce' 'process_query_failing_on_recheck_is_violation' (($r.Code -eq 1) -and ($r.Out -like '*reason=quiesced_violated*') -and ($r.Out -like '*cause=process_query_failed*'))
    $hookGrow = { param($snap) $fsx = [IO.File]::Open((Join-Path $sQ 'SaveGames\MMOSave_Ali.sav'), [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite); try { $fsx.WriteByte(7) } finally { $fsx.Dispose() } }.GetNewClosure()
    $r = Invoke-TestBackup $sQ $rQ @{ Timestamp = '20261027T200202Z'; Label = 'grow'; TestHooks = @{ AfterCopy = $hookGrow } }
    Check 'bk_quiesce' 'source_grew_during_copy_is_violation' (($r.Code -eq 1) -and ($r.Out -like '*reason=quiesced_violated*') -and ($r.Out -like '*cause=source_changed*') -and -not (Test-Path -LiteralPath (Join-Path $rQ '20261027T200202Z-grow\manifest.json')))
    $sQ2 = Join-Path $b1 'sQ2'
    New-FakeSaved $sQ2
    $hookSame = { param($snap)
        $f = Join-Path $sQ2 'MMODaily\MMOSave_Ali.json'
        $bb = [IO.File]::ReadAllBytes($f); $bb[5] = [byte]($bb[5] -bxor 1); [IO.File]::WriteAllBytes($f, $bb)
        [IO.File]::SetLastWriteTimeUtc($f, (New-Object datetime(2026, 10, 27, 9, 0, 0, [DateTimeKind]::Utc)))
    }.GetNewClosure()
    $r = Invoke-TestBackup $sQ2 $rQ @{ Timestamp = '20261027T200203Z'; Label = 'same'; TestHooks = @{ AfterCopy = $hookSame } }
    Check 'bk_quiesce' 'same_size_rewrite_with_new_mtime_is_violation' (($r.Code -eq 1) -and ($r.Out -like '*cause=source_changed*'))
    $hookNew = { param($snap) Write-TestText (Join-Path $sQ2 'MMODaily\MMOSave_Newcomer.json') '{"version":1,"day":1,"bounties":[]}' }.GetNewClosure()
    $r = Invoke-TestBackup $sQ2 $rQ @{ Timestamp = '20261027T200204Z'; Label = 'newf'; TestHooks = @{ AfterCopy = $hookNew } }
    Check 'bk_quiesce' 'new_source_file_during_copy_is_violation' (($r.Code -eq 1) -and ($r.Out -like '*cause=source_file_set_changed*'))
    [IO.File]::Delete((Join-Path $sQ2 'MMODaily\MMOSave_Newcomer.json'))
    $hookGone = { param($snap) [IO.File]::Delete((Join-Path $sQ2 'MMOMail\MMOMail_ali_baba.json')) }.GetNewClosure()
    $r = Invoke-TestBackup $sQ2 $rQ @{ Timestamp = '20261027T200205Z'; Label = 'gone'; TestHooks = @{ AfterCopy = $hookGone } }
    Check 'bk_quiesce' 'source_file_removed_during_copy_is_violation' (($r.Code -eq 1) -and ($r.Out -like '*cause=source_file_set_changed*'))
    $sQ3 = Join-Path $b1 'sQ3'
    New-FakeSaved $sQ3
    $hookLive = { param($snap) $fsx = [IO.File]::Open((Join-Path $sQ3 'SaveGames\MMOSave_Ali.sav'), [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite); try { $fsx.WriteByte(7) } finally { $fsx.Dispose() } }.GetNewClosure()
    $r = Invoke-TestBackup $sQ3 $rQ @{ Timestamp = '20261027T200206Z'; Label = 'lv'; AllowLive = $true; TestHooks = @{ AfterCopy = $hookLive } }
    Check 'bk_quiesce' 'live_mode_does_not_recheck_source' (($r.Code -eq 4) -and ($r.Out -like '*reason=live *'))
    $r = Invoke-TestBackup $sQ (Join-Path $b1 'rQ9') @{ Timestamp = '20261027T200207Z'; Label = 'calm' }
    Check 'bk_quiesce' 'quiet_source_and_processes_exit_0' ($r.Code -eq 0)

    # proof_stale (D6)
    $shG = '{"run_id":"20261027T100000Z-1234","exit_code":0,"failed":0,"gone":1,"still_connected":0,"kick_failed":0,"time_utc":"2026-10-27T10:00:00.123Z"}'
    $sPs = Join-Path $b1 'sPs'; $rPs = Join-Path $b1 'rPs'
    New-FakeSaved $sPs
    Write-TestText (Join-Path $sPs 'Logs\MMOShutdownResult.json') $shG
    $r = Invoke-TestBackup $sPs $rPs @{ RequireCleanShutdown = $true; Timestamp = '20261027T200300Z'; Label = 'fresh' }
    $mPs = Read-TestManifest (Join-Path $rPs '20261027T200300Z-fresh')
    Check 'bk_stale' 'old_sources_proof_not_stale_exit_0' (($r.Code -eq 0) -and ($mPs.proof_stale -eq $false) -and ($r.Out -like '*proof_stale=false*'))
    [IO.File]::SetLastWriteTimeUtc((Join-Path $sPs 'SaveGames\MMOSave_Ali.sav'), (New-Object datetime(2026, 10, 27, 10, 0, 0, 900, [DateTimeKind]::Utc)))
    $r = Invoke-TestBackup $sPs $rPs @{ RequireCleanShutdown = $true; Timestamp = '20261027T200301Z'; Label = 'subsec' }
    Check 'bk_stale' 'same_second_as_shutdown_is_not_stale_documented_limit' ($r.Code -eq 0)
    [IO.File]::SetLastWriteTimeUtc((Join-Path $sPs 'SaveGames\MMOSave_Ali.sav'), (New-Object datetime(2026, 10, 27, 10, 0, 5, [DateTimeKind]::Utc)))
    $r = Invoke-TestBackup $sPs $rPs @{ RequireCleanShutdown = $true; Timestamp = '20261027T200302Z'; Label = 'stale' }
    $snapPs = Join-Path $rPs '20261027T200302Z-stale'
    $mPs2 = Read-TestManifest $snapPs
    $tPs2 = Test-MMOSnapshot -SnapshotDir $snapPs
    Check 'bk_stale' 'source_changed_after_shutdown_is_stale_exit_4' (($r.Code -eq 4) -and ($r.Out -like '*reason=proof_stale *') -and ($mPs2.proof_stale -eq $true) -and ($r.Out -like '*status=complete_with_warnings*'))
    Check 'bk_stale' 'snapshot_result_exposes_proof_stale' ($tPs2.Ok -and $tPs2.ProofStale -and ($tPs2.Mode -ceq 'quiesced') -and -not $tPs2.HasInvalid)
    $psTamper = Join-Path $fx 'snap_stale_tamper'
    Copy-Item -LiteralPath $snapPs -Destination $psTamper -Recurse
    Edit-TestManifest $psTamper { param($m) $m.proof_stale = $false }
    Check 'bk_stale' 'proof_stale_flag_flip_breaks_header_digest' (HasErr (Read-MMOManifest -SnapshotDir $psTamper).Errors 'header_digest_mismatch')
    $r = Invoke-TestBackup $sPs (Join-Path $b1 'rPs2') @{ Timestamp = '20261027T200303Z'; Label = 'noproof' }
    Check 'bk_stale' 'without_proof_flag_field_absent_exit_0' (($r.Code -eq 0) -and ($null -eq (Read-TestManifest (Join-Path $b1 'rPs2\20261027T200303Z-noproof')).PSObject.Properties['proof_stale']))
    $vsr = Invoke-TestVerify @{ Snapshot = $snapPs }
    Check 'bk_stale' 'verify_shows_proof_stale_warning' (($vsr.Code -eq 0) -and (OutHas $vsr 'proof_stale=true'))

    # 16o-3 O3: canli yedek ayrimi (dizin adi, makine okunur satir, kod 4) - 24 karakterlik etiketle de Verify tanir
    $sLv = Join-Path $b1 'sLv'; $rLv = Join-Path $b1 'rLv'
    New-FakeSaved $sLv
    $lab24 = ('a' * 24)
    $r = Invoke-TestBackup $sLv $rLv @{ AllowLive = $true; Timestamp = '20261027T200400Z'; Label = $lab24 }
    $snapLv = Join-Path $rLv ('20261027T200400Z-' + $lab24 + '-live')
    Check 'bk_livecode' 'live_clean_snapshot_exit_4_name_ends_with_live' (($r.Code -eq 4) -and (Test-Path -LiteralPath $snapLv -PathType Container) -and ($r.Out -like '*status=complete_live*'))
    Check 'bk_livecode' 'result_line_machine_readable' ($r.Out -like ('*[[]MMO_BACKUP_RESULT] mode=live exit=4 status=complete_live reason=live snapshot=20261027T200400Z-' + $lab24 + '-live process_check=skipped*'))
    $vr = Invoke-TestVerify @{ BackupRoot = $rLv; All = $true }
    Check 'bk_livecode' 'verify_recognises_longest_live_name_not_foreign' (($vr.Code -eq 0) -and (OutHas $vr ' snapshots=1 ') -and (OutHas $vr ' foreign=0'))
    $r = Invoke-TestBackup $sLv $rLv @{ AllowLive = $true; Timestamp = '20261027T200401Z'; Label = '' }
    Check 'bk_livecode' 'empty_label_live_name_is_timestamp_dash_live' (($r.Code -eq 4) -and (Test-Path -LiteralPath (Join-Path $rLv '20261027T200401Z-live') -PathType Container))
    $r = Invoke-TestBackup $sLv (Join-Path $b1 'rLv2') @{ Timestamp = '20261027T200402Z'; Label = 'q' }
    Check 'bk_livecode' 'quiesced_result_line_exit_0' (($r.Code -eq 0) -and ($r.Out -like '*[[]MMO_BACKUP_RESULT] mode=quiesced exit=0 status=ok reason=ok snapshot=20261027T200402Z-q process_check=injected*'))

    # 16o-4 O4: yedek disi kalan oyuncu verisi
    $sIg = Join-Path $b1 'sIg'; $rIg = Join-Path $b1 'rIg'
    New-FakeSaved $sIg
    Write-TestText (Join-Path $sIg 'SaveGames\notes.txt') 'x'
    Write-TestBytes (Join-Path $sIg ('SaveGames\MMOSave_' + $oUml + 'mer.sav')) (New-SavBytes 100 4)
    Write-TestText (Join-Path $sIg 'MMOMail\sub\deep.json') '{"version":1}'
    $r = Invoke-TestBackup $sIg $rIg @{ Timestamp = '20261027T200500Z'; Label = 'ign' }
    $snapIg = Join-Path $rIg '20261027T200500Z-ign'
    $mIg = Read-TestManifest $snapIg
    $logIg = [IO.File]::ReadAllText((Join-Path $snapIg 'backup.log'))
    Check 'bk_ignored' 'default_exit_4_ignored_player_data' (($r.Code -eq 4) -and ($r.Out -like '*reason=ignored_player_data *') -and ($r.Out -like '*status=complete_with_warnings*'))
    Check 'bk_ignored' 'manifest_ignored_count_and_summary' (($mIg.ignored_count -eq 3) -and ($mIg.ignored_summary -ceq 'player_data=2;other=1;name_rule=1;not_whitelisted_name=1;subdir=1'))
    Check 'bk_ignored' 'console_counts_player_data_separately' (($r.Out -like '*ignored=3 ignored_player_data=2*'))
    Check 'bk_ignored' 'log_warns_player_data_may_exist_without_names' (($logIg -like '*OYUNCU VERISI OLABILIR*') -and ($logIg -cnotmatch 'notes|mer\.sav|deep') -and ($r.Out -like '*[[]MMO_BACKUP_IGNORED] SaveGames/notes.txt*'))
    Check 'bk_ignored' 'snapshot_verifies_and_ignored_fields_in_header_digest' ((Test-MMOSnapshot -SnapshotDir $snapIg).Ok)
    $igTamper = Join-Path $fx 'snap_ign_tamper'
    Copy-Item -LiteralPath $snapIg -Destination $igTamper -Recurse
    Edit-TestManifest $igTamper { param($m) $m.ignored_count = 0 }
    Check 'bk_ignored' 'ignored_count_edit_breaks_header_digest' (HasErr (Read-MMOManifest -SnapshotDir $igTamper).Errors 'header_digest_mismatch')
    $igBad = Join-Path $fx 'snap_ign_badtype'
    Copy-Item -LiteralPath $snapIg -Destination $igBad -Recurse
    Edit-TestManifest $igBad { param($m) $m.ignored_count = 'many' } -FixDigest
    Check 'bk_ignored' 'bad_ignored_count_type_rejected' (HasErr (Read-MMOManifest -SnapshotDir $igBad).Errors 'bad_ignored_count')
    $r = Invoke-TestBackup $sIg $rIg @{ Timestamp = '20261027T200501Z'; Label = 'allow'; AllowIgnored = $true }
    $logIg2 = [IO.File]::ReadAllText((Join-Path $rIg '20261027T200501Z-allow\backup.log'))
    Check 'bk_ignored' 'allow_ignored_is_deliberate_pass_exit_0_but_still_warns_and_records' (($r.Code -eq 0) -and ($logIg2 -like '*OYUNCU VERISI OLABILIR*') -and ((Read-TestManifest (Join-Path $rIg '20261027T200501Z-allow')).ignored_count -eq 3))
    $sIg2 = Join-Path $b1 'sIg2'
    New-FakeSaved $sIg2
    Write-TestText (Join-Path $sIg2 'MMOMail\sub\deep.json') '{"version":1}'
    $r = Invoke-TestBackup $sIg2 (Join-Path $b1 'rIg2') @{ Timestamp = '20261027T200502Z'; Label = 'sub' }
    Check 'bk_ignored' 'only_non_player_data_ignored_stays_exit_0' (($r.Code -eq 0) -and ((Read-TestManifest (Join-Path $b1 'rIg2\20261027T200502Z-sub')).ignored_summary -ceq 'player_data=0;other=1;subdir=1'))
    $legacySnap = New-TestSnapshot 'legacy_fields'
    $legacyM = Read-TestManifest $legacySnap
    Check 'bk_ignored' 'manifest_without_optional_fields_still_reads_and_has_none' ((Read-MMOManifest -SnapshotDir $legacySnap).Ok -and ($null -eq $legacyM.PSObject.Properties['proof_stale']) -and ($null -eq $legacyM.PSObject.Properties['ignored_count']) -and ($null -eq $legacyM.PSObject.Properties['process_check']) -and ($null -eq $legacyM.PSObject.Properties['collisions']))

    # 16o-5 D1: test parametreleri yalniz MMO_BACKUP_TESTMODE=1 iken
    $sGate = Join-Path $b1 'sGate'; $rGate = Join-Path $b1 'rGate'
    New-FakeSaved $sGate
    $envSaved = $env:MMO_BACKUP_TESTMODE
    try {
        foreach ($ev in @($null, '', '0', 'true', '1 ', '11')) {
            if ($null -eq $ev) { Remove-Item Env:\MMO_BACKUP_TESTMODE -ErrorAction SilentlyContinue } else { $env:MMO_BACKUP_TESTMODE = $ev }
            $tag = 'unset'; if ($null -ne $ev) { $tag = 'v_' + ($ev -replace '[^a-z0-9]', 'x') + '_' + $ev.Length }
            $r = Invoke-TestBackup $sGate $rGate @{ Timestamp = '20261027T200600Z' } -NoProvider
            Check 'bk_gate' ('timestamp_rejected_' + $tag) (($r.Code -eq 2) -and ($r.Out -like '*reason=test_parameter_requires_testmode*') -and (NoSnap $rGate))
        }
        Remove-Item Env:\MMO_BACKUP_TESTMODE -ErrorAction SilentlyContinue
        $r = Invoke-TestBackup $sGate $rGate @{}
        Check 'bk_gate' 'process_provider_rejected_without_testmode' (($r.Code -eq 2) -and ($r.Out -like '*reason=test_parameter_requires_testmode*') -and (NoSnap $rGate))
        $r = Invoke-TestBackup $sGate $rGate @{ TestHooks = @{ DuringCopy = { param($a, $b, $c) } } } -NoProvider
        Check 'bk_gate' 'test_hooks_rejected_without_testmode' (($r.Code -eq 2) -and ($r.Out -like '*reason=test_parameter_requires_testmode*') -and (NoSnap $rGate))
        $r = Invoke-TestBackup $sGate $rGate @{ AllowLive = $true; TestHooks = @{ AfterManifest = { param($s) } } } -NoProvider
        Check 'bk_gate' 'hooks_rejected_even_with_allowlive' (($r.Code -eq 2) -and (NoSnap $rGate))
        $env:MMO_BACKUP_TESTMODE = '1'
        $r = Invoke-TestBackup $sGate $rGate @{ Timestamp = '20261027T200601Z' }
        Check 'bk_gate' 'accepted_with_testmode_1' ($r.Code -eq 0)
    } finally { if ($null -eq $envSaved) { Remove-Item Env:\MMO_BACKUP_TESTMODE -ErrorAction SilentlyContinue } else { $env:MMO_BACKUP_TESTMODE = $envSaved } }
    Check 'bk_gate' 'testmode_restored_after_gate_cases' ($env:MMO_BACKUP_TESTMODE -ceq '1')
    foreach ($c in @(@('month13', '20261345T100000Z'), @('feb30', '20260230T100000Z'), @('hour25', '20261027T250000Z'), @('min61', '20261027T106100Z'), @('day00', '20261000T100000Z'), @('sec61', '20261027T100061Z'))) {
        $r = Invoke-TestBackup $sGate (Join-Path $b1 'rGate2') @{ Timestamp = $c[1] }
        Check 'bk_gate' ('impossible_timestamp_' + $c[0]) (($r.Code -eq 2) -and ($r.Out -like '*reason=bad_timestamp*') -and (NoSnap (Join-Path $b1 'rGate2')))
    }
    $r = Invoke-TestBackup $sGate (Join-Path $b1 'rGate2') @{ Timestamp = '20240229T100000Z' }
    Check 'bk_gate' 'leap_day_timestamp_accepted' ($r.Code -eq 0)
    Check 'bk_gate' 'manifest_records_process_check_injected' ((Read-TestManifest (Join-Path $b1 'rGate2\20240229T100000Z-manual')).process_check -ceq 'injected')
    Check 'bk_gate' 'live_manifest_records_process_check_skipped' ((Read-TestManifest $snapLv).process_check -ceq 'skipped')
    Check 'bk_gate' 'cim_branch_exists_in_script' ([IO.File]::ReadAllText($bkScript).Contains("`$script:ProcessCheck = 'cim'"))

    # 16o-6 D2: ozyineleme kapisi fail-closed + gercek yol
    Check 'overlap' 'unresolvable_path_default_is_not_under' (-not (Test-MMOPathUnder -Path "a$([char]0)b" -Root 'C:\fx'))
    Check 'overlap' 'unresolvable_path_is_violation_with_switch' (Test-MMOPathUnder -Path "a$([char]0)b" -Root 'C:\fx' -ErrorIsViolation)
    Check 'overlap' 'unresolvable_root_is_violation_with_switch' (Test-MMOPathUnder -Path 'C:\fx\a' -Root "a$([char]0)b" -ErrorIsViolation)
    Check 'overlap' 'resolvable_unrelated_path_not_violation_with_switch' (-not (Test-MMOPathUnder -Path 'C:\fx\a' -Root 'C:\other' -ErrorIsViolation))
    Check 'overlap' 'resolvable_child_violation_with_switch' (Test-MMOPathUnder -Path 'C:\fx\a\b' -Root 'C:\fx\a' -ErrorIsViolation)
    $fpReal = Join-Path $b1 'fp_real'
    [void][IO.Directory]::CreateDirectory($fpReal)
    $fpLink = Join-Path $b1 'fp_link'
    New-TestJunction $fpLink $fpReal
    $fpGot = Get-MMOFinalPath $fpLink
    Check 'finalpath' 'junction_resolves_to_target_not_link_name' (($null -ne $fpGot) -and $fpGot.EndsWith('\fp_real', [StringComparison]::OrdinalIgnoreCase) -and -not $fpGot.Contains('fp_link'))
    $fpGot2 = Get-MMOFinalPath (Join-Path $fpLink 'not\yet\here')
    Check 'finalpath' 'missing_tail_appended_to_resolved_ancestor' ($null -ne $fpGot2 -and $fpGot2.EndsWith('\fp_real\not\yet\here', [StringComparison]::OrdinalIgnoreCase))
    Check 'finalpath' 'link_and_real_path_resolve_identically' ((Get-MMOFinalPath $fpReal) -ieq $fpGot)
    $freeLetter = $null
    foreach ($L in [char[]]'ZYXWVUTSRQP') { if (-not [IO.Directory]::Exists(([string]$L + ':\'))) { $freeLetter = [string]$L; break } }
    if ($null -ne $freeLetter) {
        Check 'finalpath' 'nonexistent_drive_unresolved_is_null' ($null -eq (Get-MMOFinalPath ($freeLetter + ':\no\such\root')))
        $r = Invoke-TestBackup $sR ($freeLetter + ':\no\such\root') @{ AllowRootAncestorReparse = $true } -RootOutsideFixture
        Check 'finalpath' 'unresolvable_root_with_ancestor_flag_defers_final_path_unresolved' (($r.Code -eq 3) -and ($r.Out -like '*reason=final_path_unresolved*'))
    } else { Check 'finalpath' 'nonexistent_drive_unresolved_is_null' $true; Check 'finalpath' 'unresolvable_root_with_ancestor_flag_defers_final_path_unresolved' $true }
    $treeR1 = Get-TreeOracle $sR
    $jIn = Join-Path $b1 'jin'
    New-TestJunction $jIn $sR
    $r = Invoke-TestBackup $sR (Join-Path $jIn 'bk') @{ AllowRootAncestorReparse = $true }
    Check 'finalpath' 'root_reaching_saved_through_junction_rejected_with_ancestor_flag' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_inside_saved*') -and ((Get-TreeOracle $sR) -ceq $treeR1) -and -not (Test-Path -LiteralPath (Join-Path $sR 'bk')))
    $r = Invoke-TestBackup $sR (Join-Path $jIn 'bk') @{}
    Check 'finalpath' 'same_root_without_flag_rejected_by_reparse_ancestor' (($r.Code -eq 3) -and -not (Test-Path -LiteralPath (Join-Path $sR 'bk')))
    $sJ2 = Join-Path $b1 'sJ2'
    New-FakeSaved $sJ2
    $jSavedAlias = Join-Path $b1 'jsavedalias'
    New-TestJunction $jSavedAlias $b1
    $r = Invoke-TestBackup $sJ2 $jSavedAlias @{ AllowRootAncestorReparse = $true }
    Check 'finalpath' 'saved_inside_root_through_junction_rejected_with_ancestor_flag' (($r.Code -eq 3) -and ($r.Out -like '*reason=saved_inside_backup_root*'))
    $jOkTarget = Join-Path $b1 'jok_target'
    [void][IO.Directory]::CreateDirectory((Join-Path $jOkTarget 'bk'))
    $jOk = Join-Path $b1 'jok'
    New-TestJunction $jOk $jOkTarget
    $r = Invoke-TestBackup $sJ2 (Join-Path $jOk 'bk') @{ AllowRootAncestorReparse = $true; Timestamp = '20261027T200700Z' }
    # NOT: .NET Framework 4.x'te atasi junction olan yolda dosya/dizin yazimi basarisiz olur (B0 sinirinin aynisi); bu yuzden zararsiz junction icin yalniz YAPI kapisinin gecildigi sinanir (yazim sonucu degil).
    Check 'finalpath' 'harmless_junction_ancestor_passes_the_overlap_gate_with_flag' (($r.Out -notlike '*reason=backup_root_inside_*') -and ($r.Out -notlike '*reason=saved_inside_backup_root*') -and ($r.Out -notlike '*reason=final_path_unresolved*') -and ($r.Out -notlike '*reason=backup_root_reparse_ancestor*'))

    # 16o-7 D3: last_run.json / last_ok.json / log temizligi
    $sLr = Join-Path $b1 'sLr'; $rLr9 = Join-Path $b1 'rLr9'
    New-FakeSaved $sLr
    $r = Invoke-TestBackup $sLr $rLr9 @{ ProcessProvider = $blocker; Timestamp = '20261027T200800Z' }
    $lr1 = $null
    if (Test-Path -LiteralPath (Join-Path $rLr9 'last_run.json')) { $lr1 = (Read-MMOUtf8Text (Join-Path $rLr9 'last_run.json')).Text | ConvertFrom-Json }
    Check 'bk_lastrun' 'deferral_exit_3_writes_last_run' (($r.Code -eq 3) -and ($null -ne $lr1) -and ($lr1.exit_code -eq 3) -and ($lr1.status -ceq 'deferred') -and ($lr1.reason -ceq 'process_running'))
    Check 'bk_lastrun' 'deferral_does_not_write_last_ok_nor_snapshot' ((-not (Test-Path -LiteralPath (Join-Path $rLr9 'last_ok.json'))) -and (NoSnap $rLr9))
    Check 'bk_lastrun' 'last_run_time_is_utc_stamp' (($null -ne $lr1) -and (Test-MMOUtcStamp $lr1.time_utc) -and ([math]::Abs(([datetime]::UtcNow - [datetime]::ParseExact($lr1.time_utc, "yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal)).TotalSeconds) -lt 300))
    $r = Invoke-TestBackup $sLr $rLr9 @{ Timestamp = '20261027T200801Z'; Label = 'good' }
    $lr2 = (Read-MMOUtf8Text (Join-Path $rLr9 'last_run.json')).Text | ConvertFrom-Json
    $lo2 = (Read-MMOUtf8Text (Join-Path $rLr9 'last_ok.json')).Text | ConvertFrom-Json
    Check 'bk_lastrun' 'success_exit_0_updates_last_run_and_last_ok' (($r.Code -eq 0) -and ($lr2.exit_code -eq 0) -and ($lr2.status -ceq 'ok') -and ($lo2.snapshot -ceq '20261027T200801Z-good') -and ($lo2.manifest_sha256 -ceq (Get-OracleSha (Join-Path $rLr9 '20261027T200801Z-good\manifest.json'))) -and ($lo2.mode -ceq 'quiesced'))
    $r = Invoke-TestBackup $sLr $rLr9 @{ Timestamp = '20261027T200802Z'; AllowLive = $true }
    $lr3 = (Read-MMOUtf8Text (Join-Path $rLr9 'last_run.json')).Text | ConvertFrom-Json
    $lo3 = (Read-MMOUtf8Text (Join-Path $rLr9 'last_ok.json')).Text | ConvertFrom-Json
    Check 'bk_lastrun' 'live_exit_4_updates_last_run_but_not_last_ok' (($r.Code -eq 4) -and ($lr3.exit_code -eq 4) -and ($lr3.mode -ceq 'live') -and ($lr3.reason -ceq 'live') -and ($lo3.snapshot -ceq '20261027T200801Z-good'))
    $r = Invoke-TestBackup $sLr $rLr9 @{ ProcessProvider = $blocker; Timestamp = '20261027T200803Z' }
    $lo4 = (Read-MMOUtf8Text (Join-Path $rLr9 'last_ok.json')).Text | ConvertFrom-Json
    Check 'bk_lastrun' 'later_failure_keeps_last_ok_untouched' (($r.Code -eq 3) -and ($lo4.snapshot -ceq '20261027T200801Z-good') -and ((Read-MMOUtf8Text (Join-Path $rLr9 'last_run.json')).Text | ConvertFrom-Json).exit_code -eq 3)
    $r = Invoke-TestBackup $sLr $rLr9 @{ Label = 'BAD' }
    Check 'bk_lastrun' 'usage_error_exit_2_is_recorded_too' (($r.Code -eq 2) -and (((Read-MMOUtf8Text (Join-Path $rLr9 'last_run.json')).Text | ConvertFrom-Json).exit_code -eq 2))
    $lrText = (Read-MMOUtf8Text (Join-Path $rLr9 'last_run.json')).Text + (Read-MMOUtf8Text (Join-Path $rLr9 'last_ok.json')).Text
    Check 'bk_lastrun' 'status_files_hold_no_player_name_or_full_path' (($lrText -cnotmatch 'Ali|Baba') -and (-not $lrText.Contains($fx)))
    Check 'bk_lastrun' 'no_tmp_leftovers_in_root' (@(Get-ChildItem -LiteralPath $rLr9 -Force -File | Where-Object { $_.Name -like '*.tmp' }).Count -eq 0)
    $rLrUnsafe = Join-Path $sLr 'bk_in_saved'
    $r = Invoke-TestBackup $sLr $rLrUnsafe @{}
    Check 'bk_lastrun' 'unsafe_root_never_gets_status_files_and_warns' (($r.Code -eq 3) -and ($r.Out -like '*last_run.json yazilmadi*') -and -not (Test-Path -LiteralPath $rLrUnsafe))
    $rLrDir = Join-Path $b1 'rLrDir'
    [void][IO.Directory]::CreateDirectory((Join-Path $rLrDir 'last_run.json'))
    $r = Invoke-TestBackup $sLr $rLrDir @{ Timestamp = '20261027T200804Z' }
    Check 'bk_lastrun' 'unwritable_last_run_warns_but_backup_and_exit_code_stand' (($r.Code -eq 0) -and ($r.Out -like '*last_run.json yazilamadi*') -and (Test-Path -LiteralPath (Join-Path $rLrDir '20261027T200804Z-manual\manifest.json')))
    $rLrJ = Join-Path $b1 'rLrJ'; $lrTarget = Join-Path $b1 'lrtarget'
    [void][IO.Directory]::CreateDirectory($rLrJ); [void][IO.Directory]::CreateDirectory($lrTarget)
    New-TestJunction (Join-Path $rLrJ 'last_run.json') $lrTarget
    $r = Invoke-TestBackup $sLr $rLrJ @{ Timestamp = '20261027T200805Z' }
    Check 'bk_lastrun' 'reparse_last_run_is_not_followed' (($r.Code -eq 0) -and ($r.Out -like '*last_run.json yazilamadi*') -and (@(Get-ChildItem -LiteralPath $lrTarget -Force).Count -eq 0))
    $logLr = [IO.File]::ReadAllText((Join-Path $rLr9 '20261027T200801Z-good\backup.log'))
    Check 'bk_lastrun' 'log_has_result_line_with_codes_only' (($logLr -like '*[[]MMO_BACKUP_RESULT] mode=quiesced exit=0*') -and ($logLr -cnotmatch 'Ali|Baba|pid=|process=') -and -not $logLr.Contains($fx))
    $r = Invoke-TestBackup $sLr (Join-Path $b1 'rLrBl') @{ ProcessProvider = $blocker }
    Check 'bk_lastrun' 'blocker_names_console_only' (($r.Out -like '*process=UnrealEditor.exe*') -and (NoSnap (Join-Path $b1 'rLrBl')))

    # 16o-8 D7: bos alan on denetimi
    $sDk = Join-Path $b1 'sDk'; $rDk = Join-Path $b1 'rDk'
    New-FakeSaved $sDk
    $r = Invoke-TestBackup $sDk $rDk @{ Timestamp = '20261027T200900Z'; Label = 'size' }
    $mDk = Read-TestManifest (Join-Path $rDk '20261027T200900Z-size')
    $totDk = 0L; foreach ($e in $mDk.entries) { $totDk += [long]$e.size }
    $needDk = [long][math]::Ceiling([double]$totDk * 1.5)
    $global:freeNow = 5L
    $hookFree = { param($path) $global:freeNow }
    $r = Invoke-TestBackup $sDk $rDk @{ Timestamp = '20261027T200901Z'; Label = 'low'; TestHooks = @{ FreeBytes = $hookFree } }
    Check 'bk_disk' 'tiny_free_space_defers_disk_space_low_nothing_written' (($r.Code -eq 3) -and ($r.Out -like '*reason=disk_space_low*') -and -not (Test-Path -LiteralPath (Join-Path $rDk '20261027T200901Z-low')))
    $global:freeNow = $needDk - 1
    $r = Invoke-TestBackup $sDk $rDk @{ Timestamp = '20261027T200902Z'; Label = 'edge-1'; TestHooks = @{ FreeBytes = $hookFree } }
    Check 'bk_disk' 'one_byte_short_of_1_5x_defers' (($r.Code -eq 3) -and ($r.Out -like '*reason=disk_space_low*') -and ($totDk -gt 0))
    $global:freeNow = $needDk
    $r = Invoke-TestBackup $sDk $rDk @{ Timestamp = '20261027T200903Z'; Label = 'edge'; TestHooks = @{ FreeBytes = $hookFree } }
    Check 'bk_disk' 'exactly_1_5x_is_enough_exit_0' ($r.Code -eq 0)
    $global:freeNow = -1L
    $r = Invoke-TestBackup $sDk $rDk @{ Timestamp = '20261027T200904Z'; Label = 'unk'; TestHooks = @{ FreeBytes = $hookFree } }
    Check 'bk_disk' 'unmeasurable_free_space_defers_fail_closed' (($r.Code -eq 3) -and ($r.Out -like '*reason=disk_space_unknown*') -and -not (Test-Path -LiteralPath (Join-Path $rDk '20261027T200904Z-unk')))
    Check 'bk_disk' 'real_free_bytes_positive_for_existing_dir' ((Get-MMOFreeBytes $fx) -gt 0)
    Check 'bk_disk' 'real_free_bytes_for_missing_path_uses_ancestor' ((Get-MMOFreeBytes (Join-Path $fx 'no\such\dir')) -gt 0)
    $diskFile = Join-Path $fx 'disk_file.txt'
    Write-TestText $diskFile 'x'
    Check 'bk_disk' 'real_free_bytes_for_a_file_path_uses_parent' ((Get-MMOFreeBytes $diskFile) -gt 0)
    Check 'bk_disk' 'real_free_bytes_invalid_path_is_minus_one' ((Get-MMOFreeBytes "a$([char]0)b") -eq -1)
    $r = Invoke-TestBackup $sDk (Join-Path $b1 'rDk2') @{ Timestamp = '20261027T200905Z' }
    Check 'bk_disk' 'real_free_space_check_passes_normally' ($r.Code -eq 0)

    # ---- 18. B3: RestoreSaves.ps1 (yalniz fikstur Saved/yedek koku; goruntuler B1 ile alinmis; gercek Saved'e dokunulmaz) ---------
    $rsScript = Join-Path $PSScriptRoot 'RestoreSaves.ps1'
    $b3 = Join-Path $fx 'b3'
    [void][IO.Directory]::CreateDirectory($b3)
    $global:rsTs = 0
    $global:rsHook = @{}
    $global:rsProcFlip = $false
    $unrealProc = { @([pscustomobject]@{ Name = 'UnrealEditor.exe'; ProcessId = 9; CommandLine = '' }) }

    function Get-RsTs {
        $global:rsTs++
        return ((New-Object datetime(2027, 1, 1, 0, 0, 0, [DateTimeKind]::Utc)).AddSeconds($global:rsTs).ToString("yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture))
    }
    function Set-Aged {
        param([string]$Dir)
        foreach ($ff in (Get-ChildItem -LiteralPath $Dir -Recurse -File -Force)) { [IO.File]::SetLastWriteTimeUtc($ff.FullName, (New-Object datetime(2026, 10, 27, 8, 0, 0, [DateTimeKind]::Utc))) }
    }
    function Set-Live {
        param([string]$Saved, [string]$Rel, $Content)
        $p = Join-Path $Saved $Rel
        if ($Content -is [string]) { Write-TestText $p $Content } else { Write-TestBytes $p $Content }
        [IO.File]::SetLastWriteTimeUtc($p, (New-Object datetime(2026, 10, 27, 8, 0, 0, [DateTimeKind]::Utc)))
    }
    function Invoke-TestRestore {
        param([string]$Snap, [string]$Saved, [string]$Root, [hashtable]$Extra = @{}, [switch]$NoProvider)
        foreach ($pth in $Snap, $Saved, $Root) { if ($pth -ne '' -and -not (Test-MMOPathUnder -Path $pth -Root $fx)) { throw 'test_guard_restore_path_outside_fixture' } }
        $p = @{ Snapshot = $Snap; SavedDir = $Saved; BackupRoot = $Root; ProcessProvider = $benign }
        foreach ($k in $Extra.Keys) { $p[$k] = $Extra[$k] }
        if ($NoProvider) { $p.Remove('ProcessProvider') }
        if ($p.ContainsKey('Apply') -and -not $p.ContainsKey('Timestamp')) { $p['Timestamp'] = Get-RsTs }
        $global:LASTEXITCODE = -999
        $lines = @(& $rsScript @p *>&1 | ForEach-Object { [string]$_ })
        return [pscustomobject]@{ Code = $global:LASTEXITCODE; Out = ($lines -join "`n"); Lines = $lines }
    }
    function Invoke-Rs {
        param($F, [hashtable]$Extra = @{}, [switch]$Apply, [switch]$NoScope, [switch]$NoSha, [string]$SavedOverride = '')
        $e = @{}
        foreach ($k in $Extra.Keys) { $e[$k] = $Extra[$k] }
        if (-not $NoScope -and -not $e.ContainsKey('Slots') -and -not $e.ContainsKey('All')) { $e['All'] = $true }
        if ($Apply) {
            $e['Apply'] = $true
            if (-not $NoSha -and -not $e.ContainsKey('ExpectedManifestSha256')) { $e['ExpectedManifestSha256'] = $F.Sha }
        }
        $sv = $F.Saved
        if ($SavedOverride -ne '') { $sv = $SavedOverride }
        return (Invoke-TestRestore -Snap $F.Snap -Saved $sv -Root $F.Root -Extra $e)
    }
    function New-RsFixture {
        param([string]$Name, [switch]$KeepBad, [hashtable]$BackupExtra = @{}, [scriptblock]$Prep = $null)
        $d = Join-Path $b3 $Name
        $saved = Join-Path $d 'saved'
        $root = Join-Path $d 'bk'
        New-FakeSaved $saved -KeepBad:$KeepBad
        if ($null -ne $Prep) { & $Prep $saved }
        Set-Aged $saved
        $ts = Get-RsTs
        $ex = @{ Timestamp = $ts; Label = 'img'; AllowIgnored = $true }
        foreach ($k in $BackupExtra.Keys) { $ex[$k] = $BackupExtra[$k] }
        $r = Invoke-TestBackup $saved $root $ex
        $nm = $ts + '-img'
        if ($ex.ContainsKey('AllowLive')) { $nm = $nm + '-live' }
        $snap = Join-Path $root $nm
        $sha = ''
        if (Test-Path -LiteralPath (Join-Path $snap 'manifest.json')) { $sha = Get-OracleSha (Join-Path $snap 'manifest.json') }
        return [pscustomobject]@{ Dir = $d; Saved = $saved; Root = $root; Snap = $snap; Sha = $sha; BackupCode = $r.Code }
    }
    function Test-Untouched {
        param($F, [string]$TreeBefore)
        $noPre = (@(Get-SnapDirs $F.Root | Where-Object { $_ -like '*-prerestore' }).Count -eq 0)
        return (((Get-TreeOracle $F.Saved) -ceq $TreeBefore) -and $noPre)
    }
    function Get-PreDir {
        param($F)
        $d = @(Get-SnapDirs $F.Root | Where-Object { $_ -like '*-prerestore' } | Sort-Object)
        if ($d.Count -eq 0) { return '' }
        return (Join-Path $F.Root $d[$d.Count - 1])
    }
    $alis = 'SaveGames\MMOSave_Ali.sav'

    # 18a Betik yapisi (statik)
    $rperr = $null
    $rast = [System.Management.Automation.Language.Parser]::ParseFile($rsScript, [ref]$null, [ref]$rperr)
    Check 'rs_static' 'script_parses' (($null -eq $rperr) -or ($rperr.Count -eq 0))
    Check 'rs_static' 'script_is_ascii' (-not (([IO.File]::ReadAllBytes($rsScript)) | Where-Object { $_ -gt 127 }))
    $rdef = @{}
    foreach ($pp in $rast.ParamBlock.Parameters) { if ($null -ne $pp.DefaultValue) { $rdef[$pp.Name.VariablePath.UserPath] = $pp.DefaultValue.Extent.Text } }
    Check 'rs_static' 'backup_root_default_matches_backup_script' ($rdef['BackupRoot'] -ceq $pdef['BackupRoot'])
    Check 'rs_static' 'saved_default_matches_backup_script' ($rdef['SavedDir'] -ceq $pdef['SavedDir'])
    $applyParam = @($rast.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -ceq 'Apply' })
    Check 'rs_static' 'apply_is_a_switch_without_default' (($applyParam.Count -eq 1) -and ($applyParam[0].StaticType.Name -ceq 'SwitchParameter') -and ($null -eq $applyParam[0].DefaultValue))
    $rUsed = @($rast.FindAll({ param($n) $n -is [System.Management.Automation.Language.CommandAst] }, $true) | ForEach-Object { $_.GetCommandName() } | Where-Object { $null -ne $_ })
    Check 'rs_static' 'no_delete_move_copy_write_cmdlets' (@($rUsed | Where-Object { @('Remove-Item', 'Move-Item', 'Copy-Item', 'Set-Content', 'Add-Content', 'Out-File', 'New-Item', 'Rename-Item', 'Clear-Content') -contains $_ }).Count -eq 0)
    Check 'rs_static' 'no_direct_writeallbytes_to_player_files' (-not (([IO.File]::ReadAllText($rsScript)) -match 'WriteAllBytes\(\$(Dest|p\.Dest|it\.Dest)'))

    # 18b Kullanim hatalari (2): hicbir sey yazilmaz
    $fu = New-RsFixture 'usage'
    Check 'rs_fixture' 'backup_fixture_taken_and_manifest_present' (($fu.BackupCode -eq 0) -and ($fu.Sha.Length -eq 64))
    $stU0 = Get-FullStamp $fu.Dir
    $r = Invoke-TestRestore -Snap '' -Saved $fu.Saved -Root $fu.Root -Extra @{ All = $true }
    Check 'rs_usage' 'snapshot_required' (($r.Code -eq 2) -and ($r.Out -like '*reason=snapshot_required*'))
    $r = Invoke-Rs $fu -NoScope
    Check 'rs_usage' 'scope_required' (($r.Code -eq 2) -and ($r.Out -like '*reason=scope_required_all_or_slots*'))
    $r = Invoke-Rs $fu -Extra @{ All = $true; Slots = @('ali'); AcknowledgeCrossSlotRisk = $true }
    Check 'rs_usage' 'all_and_slots_exclusive' (($r.Code -eq 2) -and ($r.Out -like '*reason=all_and_slots_exclusive*'))
    $r = Invoke-Rs $fu -Extra @{ Slots = @('ali') }
    Check 'rs_usage' 'slots_without_cross_slot_ack_refused' (($r.Code -eq 2) -and ($r.Out -like '*reason=slots_require_acknowledge_cross_slot_risk*'))
    $r = Invoke-Rs $fu -Extra @{ Slots = @('ali') } -Apply
    Check 'rs_usage' 'slots_apply_without_ack_refused' (($r.Code -eq 2) -and ($r.Out -like '*reason=slots_require_acknowledge_cross_slot_risk*'))
    $r = Invoke-Rs $fu -Apply -NoSha
    Check 'rs_usage' 'apply_without_expected_sha_refused' (($r.Code -eq 2) -and ($r.Out -like '*reason=apply_requires_expected_manifest_sha256*'))
    $r = Invoke-Rs $fu -Extra @{ ExpectedManifestSha256 = 'xyz' }
    Check 'rs_usage' 'bad_expected_sha_format' (($r.Code -eq 2) -and ($r.Out -like '*reason=bad_expected_sha*'))
    $r = Invoke-Rs $fu -Extra @{ Slots = @('A' + $oUml); AcknowledgeCrossSlotRisk = $true }
    Check 'rs_usage' 'non_ascii_slot_name_refused' (($r.Code -eq 2) -and ($r.Out -like '*reason=bad_slot_name*'))
    $r = Invoke-Rs $fu -Extra @{ RecentWriteSeconds = 0 }
    Check 'rs_usage' 'bad_recent_write_seconds' (($r.Code -eq 2) -and ($r.Out -like '*reason=bad_recent_write_seconds*'))
    $env:MMO_BACKUP_TESTMODE = ''
    $r = Invoke-Rs $fu
    $env:MMO_BACKUP_TESTMODE = '1'
    Check 'rs_usage' 'test_parameters_require_test_mode' (($r.Code -eq 2) -and ($r.Out -like '*reason=test_parameter_requires_testmode*'))
    Check 'rs_usage' 'usage_errors_wrote_nothing' ((Get-FullStamp $fu.Dir).Digest -ceq $stU0.Digest)

    # 18c Kuru calisma (varsayilan): hicbir sey yazmaz, plan + manifest ozeti
    $f1 = New-RsFixture 'main'
    $imageAliSha = Get-OracleSha (Join-Path $f1.Saved $alis)
    Set-Live $f1.Saved $alis (New-SavBytes 300 77)
    Set-Live $f1.Saved 'SaveGames\MMOSave_Extra.sav' (New-SavBytes 120 5)
    $liveAliSha = Get-OracleSha (Join-Path $f1.Saved $alis)
    $extraSha = Get-OracleSha (Join-Path $f1.Saved 'SaveGames\MMOSave_Extra.sav')
    $stF1 = Get-FullStamp $f1.Dir
    $r = Invoke-Rs $f1
    Check 'rs_dry' 'exit_0_clean_plan' ($r.Code -eq 0)
    Check 'rs_dry' 'prints_full_manifest_sha256_matching_oracle' ($r.Out.Contains('[MMO_RESTORE_MANIFEST_SHA256] sha256=' + $f1.Sha + ' '))
    Check 'rs_dry' 'plan_write_for_changed_file' ($r.Out.Contains('action=write path=SaveGames/MMOSave_Ali.sav slot=ali target=overwrite'))
    Check 'rs_dry' 'plan_unchanged_for_identical_file' ($r.Out.Contains('action=unchanged path=MMOMail/MMOMail_ali.json slot=ali target=same'))
    Check 'rs_dry' 'plan_skips_side_files_by_default' ($r.Out.Contains('action=skip path=MMODaily/MMOSave_Ali.json.tmp reason=side_file_requires_flag'))
    Check 'rs_dry' 'extra_live_file_reported_not_deleted' ($r.Out.Contains('[MMO_RESTORE_EXTRA] path=SaveGames/MMOSave_Extra.sav'))
    Check 'rs_dry' 'cross_slot_warning_for_all_restore' ($r.Out.Contains('code=cross_slot'))
    Check 'rs_dry' 'result_line_says_dry_run' ($r.Out.Contains('[MMO_RESTORE_RESULT] mode=dry-run exit=0'))
    Check 'rs_dry' 'dry_run_prints_placeholder_not_ready_apply_command' ($r.Out.Contains('-ExpectedManifestSha256 <yedek alirken KAYDETTIGIN ozet> -Apply') -and (@($r.Lines | Where-Object { $_ -like '*[[]MMO_RESTORE_NEXT]*' -and $_.Contains($f1.Sha) }).Count -eq 0) -and $r.Out.Contains('[MMO_RESTORE_COMPUTED_SHA256] sha256=' + $f1.Sha))
    Check 'rs_dry' 'dry_run_wrote_nothing_anywhere' ((Get-FullStamp $f1.Dir).Digest -ceq $stF1.Digest)
    Check 'rs_dry' 'dry_run_no_preimage_no_status_files' ((@(Get-SnapDirs $f1.Root | Where-Object { $_ -like '*-prerestore' }).Count -eq 0) -and -not (Test-Path -LiteralPath (Join-Path $f1.Root 'last_restore.json')))
    $r = Invoke-Rs $f1 -Extra @{ ExpectedManifestSha256 = $f1.Sha.ToUpperInvariant() }
    Check 'rs_dry' 'matching_expected_sha_accepted_case_insensitive' ($r.Code -eq 0)
    $r = Invoke-Rs $f1 -Extra @{ ExpectedManifestSha256 = ('0' * 64) }
    Check 'rs_dry' 'wrong_expected_sha_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=manifest_sha256_mismatch*'))
    $r = Invoke-Rs $f1 -Extra @{ ProcessProvider = $unrealProc }
    Check 'rs_dry' 'dry_run_with_running_editor_still_plans_and_warns' (($r.Code -eq 0) -and ($r.Out.Contains('code=apply_would_defer reason=process_running')))
    Check 'rs_dry' 'dry_runs_still_wrote_nothing' ((Get-FullStamp $f1.Dir).Digest -ceq $stF1.Digest)

    # 18d Uygulama (mutlu yol): pre-restore ONCE, .tmp + Replace, sert baglanti dokunulmaz, silme yok, log/durum dosyasi
    $hl = Join-Path $f1.Dir 'hl_ali.bin'
    [void](New-Item -ItemType HardLink -Path $hl -Target (Join-Path $f1.Saved $alis))
    $treeF1pre = Get-TreeOracle $f1.Saved
    $global:rsHook = @{ saved = $f1.Saved; preExists = $false; preOk = $false; liveAli = ''; tmp = ''; tmpExists = $false; destDuring = '' }
    $hooks1 = @{
        BeforeFirstWrite = { param($pre) $global:rsHook.preExists = (Test-Path -LiteralPath $pre -PathType Container); $global:rsHook.preOk = [bool](Test-MMOSnapshot -SnapshotDir $pre).Ok; $global:rsHook.liveAli = (Get-OracleSha (Join-Path $global:rsHook.saved 'SaveGames\MMOSave_Ali.sav')) }
        AfterTmp = { param($rel, $tmp, $dest) $global:rsHook.tmp = $tmp; $global:rsHook.tmpExists = (Test-Path -LiteralPath $tmp); $global:rsHook.destDuring = (Get-OracleSha $dest) }
    }
    $r = Invoke-Rs $f1 -Apply -Extra @{ TestHooks = $hooks1; AcknowledgePartialState = $true }
    $preD = Get-PreDir $f1
    Check 'rs_apply' 'exit_4_partial_state_acknowledged' (($r.Code -eq 4) -and ($r.Out -like '*reason=partial_state_acknowledged*'))
    Check 'rs_apply' 'restored_file_equals_image_bytes' ((Get-OracleSha (Join-Path $f1.Saved $alis)) -ceq $imageAliSha)
    Check 'rs_apply' 'preimage_existed_and_verified_before_first_write' (($global:rsHook.preExists -eq $true) -and ($global:rsHook.preOk -eq $true))
    Check 'rs_apply' 'live_file_still_old_at_first_write' ($global:rsHook.liveAli -ceq $liveAliSha)
    Check 'rs_apply' 'tmp_file_used_with_restore_tmp_name_and_dest_old_meanwhile' (([string]$global:rsHook.tmp).EndsWith('.restore.tmp') -and ($global:rsHook.tmpExists -eq $true) -and ($global:rsHook.destDuring -ceq $liveAliSha))
    Check 'rs_apply' 'hard_link_not_written_through' (((Get-OracleSha $hl) -ceq $liveAliSha) -and ((Get-OracleSha (Join-Path $f1.Saved $alis)) -cne $liveAliSha))
    Check 'rs_apply' 'no_restore_tmp_left' (@(Get-ChildItem -LiteralPath $f1.Saved -Recurse -Force -File | Where-Object { $_.Name -like '*.restore.tmp' }).Count -eq 0)
    Check 'rs_apply' 'extra_live_file_not_deleted' ((Get-OracleSha (Join-Path $f1.Saved 'SaveGames\MMOSave_Extra.sav')) -ceq $extraSha)
    Check 'rs_apply' 'unrelated_logs_untouched' ((Get-Content -LiteralPath (Join-Path $f1.Saved 'Logs\app.log') -Raw) -ceq 'log line')
    Check 'rs_apply' 'preimage_named_prerestore_and_valid' (($preD -ne '') -and (([IO.Path]::GetFileName($preD)) -cmatch '^[0-9]{8}T[0-9]{6}Z-prerestore\z') -and (Test-MMOSnapshot -SnapshotDir $preD).Ok)
    $pent = @()
    $pmode = ''
    if ($preD -ne '') { $pm = Read-TestManifest $preD; $pent = @($pm.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Ali.sav' }); $pmode = [string]$pm.mode }
    Check 'rs_apply' 'preimage_holds_pre_state_bytes' (($pent.Count -eq 1) -and ($pent[0].sha256 -ceq $liveAliSha) -and ($pmode -ceq 'quiesced'))
    Check 'rs_apply' 'result_line_names_preimage' ($r.Out -like ('*[[]MMO_RESTORE_RESULT] mode=apply exit=4*preimage=' + ([IO.Path]::GetFileName($preD)) + '*'))
    $rlog = ''
    if ($preD -ne '' -and (Test-Path -LiteralPath (Join-Path $preD 'restore.log'))) { $rlog = [IO.File]::ReadAllText((Join-Path $preD 'restore.log')) }
    Check 'rs_log' 'restore_log_written_in_preimage_dir' ($rlog.Contains('[MMO_RESTORE_RESULT] mode=apply exit=4'))
    Check 'rs_log' 'restore_log_has_no_player_name_or_full_path' ((-not $rlog.ToLowerInvariant().Contains('ali')) -and (-not $rlog.Contains($fx)) -and ($rlog -cnotmatch '[A-Za-z]:\\'))
    $lrPath = Join-Path $f1.Root 'last_restore.json'
    $lr = $null
    if (Test-Path -LiteralPath $lrPath) { $lr = (Read-MMOUtf8Text $lrPath).Text | ConvertFrom-Json }
    Check 'rs_log' 'last_restore_json_written' (($null -ne $lr) -and ($lr.exit_code -eq 4) -and ($lr.status -ceq 'ok_with_warnings') -and ($lr.manifest_sha256 -ceq $f1.Sha) -and ($lr.preimage -ceq ([IO.Path]::GetFileName($preD))))
    Check 'rs_log' 'last_restore_json_has_no_player_name_or_path' (($null -ne $lr) -and (-not ([IO.File]::ReadAllText($lrPath)).ToLowerInvariant().Contains('ali')) -and (-not ([IO.File]::ReadAllText($lrPath)).Contains($fx.Replace('\', '\\'))))

    # 18e Geri alma turu: pre-restore goruntusunden -All ile onceki duruma donulur
    Set-Aged $f1.Saved
    $preSha = ''
    if ($preD -ne '') { $preSha = Get-OracleSha (Join-Path $preD 'manifest.json') }
    $rbr = Invoke-TestRestore -Snap $preD -Saved $f1.Saved -Root $f1.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $preSha }
    Check 'rs_rollback_trip' 'rollback_from_preimage_exit_0' (($rbr.Code -eq 0) -or ($rbr.Code -eq 4))
    Check 'rs_rollback_trip' 'tree_back_to_pre_state' ((Get-TreeOracle $f1.Saved) -ceq $treeF1pre)

    # 18f Kapi: goruntu dogrulamasi (Test-MMOSnapshot), ozet, yarim goruntu, yol yapisi
    $g = New-RsFixture 'g_tamper'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    [IO.File]::WriteAllBytes((Join-Path $g.Snap ('files\' + $alis)), (New-SavBytes 200 99))
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_snapshot' 'tampered_image_file_refused_with_valid_manifest_sha' (($r.Code -eq 1) -and ($r.Out -like '*reason=snapshot_verify_failed*') -and ($r.Out -like '*hash_mismatch*'))
    Check 'rs_gate_snapshot' 'tampered_image_touched_nothing_no_preimage' (Test-Untouched $g $tr)
    $g = New-RsFixture 'g_extra'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    Write-TestBytes (Join-Path $g.Snap 'files\SaveGames\MMOSave_Planted.sav') (New-SavBytes 100 4)
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_snapshot' 'extra_file_in_image_refused' (($r.Code -eq 1) -and ($r.Out -like '*extra_file*'))
    Check 'rs_gate_snapshot' 'extra_file_image_touched_nothing' (Test-Untouched $g $tr)
    $g = New-RsFixture 'g_validlie' -KeepBad
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    Edit-TestManifest $g.Snap { param($m) (@($m.entries | Where-Object { $_.path -ceq 'SaveGames/MMOSave_Bad.sav' }))[0].valid = $true } -FixDigest
    $g.Sha = Get-OracleSha (Join-Path $g.Snap 'manifest.json')
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_snapshot' 'valid_flag_lie_refused_even_with_matching_digests_and_sha' (($r.Code -eq 1) -and ($r.Out -like '*valid_mismatch*'))
    Check 'rs_gate_snapshot' 'valid_flag_lie_touched_nothing' (Test-Untouched $g $tr)
    $g = New-RsFixture 'g_incomplete'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    [IO.File]::Move((Join-Path $g.Snap 'manifest.json'), (Join-Path $g.Snap 'manifest.json.failed'))
    $r = Invoke-Rs $g -Apply -Extra @{ ExpectedManifestSha256 = ('1' * 64) }
    Check 'rs_gate_snapshot' 'half_image_with_failed_manifest_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=snapshot_incomplete*'))
    [IO.File]::Move((Join-Path $g.Snap 'manifest.json.failed'), (Join-Path $g.Snap 'manifest.json'))
    Write-TestText (Join-Path $g.Snap 'manifest.json.failed') 'x'
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_snapshot' 'manifest_failed_leftover_next_to_manifest_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=snapshot_incomplete*'))
    [IO.File]::Delete((Join-Path $g.Snap 'manifest.json.failed'))
    [IO.File]::Delete((Join-Path $g.Snap 'manifest.json'))
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_snapshot' 'image_without_manifest_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=snapshot_incomplete*'))
    Check 'rs_gate_snapshot' 'incomplete_image_touched_nothing' (Test-Untouched $g $tr)
    $g = New-RsFixture 'g_paths'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    $r = Invoke-TestRestore -Snap (Join-Path $g.Dir 'nosuchsnap') -Saved $g.Saved -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $g.Sha }
    Check 'rs_gate_paths' 'missing_snapshot_deferred_3' (($r.Code -eq 3) -and ($r.Out -like '*reason=snapshot_missing*'))
    New-TestJunction (Join-Path $g.Dir 'jx') $g.Snap
    $r = Invoke-TestRestore -Snap (Join-Path $g.Dir 'jx') -Saved $g.Saved -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $g.Sha }
    Check 'rs_gate_paths' 'reparse_snapshot_deferred_3' (($r.Code -eq 3) -and ($r.Out -like '*reason=snapshot_reparse*'))
    Copy-Item -LiteralPath $g.Snap -Destination (Join-Path $g.Saved '_img') -Recurse
    $tr2 = Get-TreeOracle $g.Saved
    $r = Invoke-TestRestore -Snap (Join-Path $g.Saved '_img') -Saved $g.Saved -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $g.Sha }
    Check 'rs_gate_paths' 'snapshot_inside_saved_deferred_3' (($r.Code -eq 3) -and ($r.Out -like '*reason=snapshot_inside_saved*'))
    $r = Invoke-TestRestore -Snap $g.Snap -Saved (Join-Path $g.Dir 'nosaved') -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $g.Sha }
    Check 'rs_gate_paths' 'missing_saved_deferred_3' (($r.Code -eq 3) -and ($r.Out -like '*reason=saved_root_missing*'))
    Check 'rs_gate_paths' 'path_gates_touched_nothing' (Test-Untouched $g $tr2)

    # 18g Kapi: cakisma, live goruntu, kaynak koku
    $g = New-RsFixture 'g_coll' -Prep { param($s) Write-TestBytes (Join-Path $s 'SaveGames\MMOSave_Ali_Baba.sav') (New-SavBytes 150 6) }
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    Check 'rs_fixture' 'collision_image_taken_with_exit_4' ($g.BackupCode -eq 4)
    $r = Invoke-Rs $g
    Check 'rs_gate_collisions' 'dry_run_refuses_declared_collisions' (($r.Code -eq 1) -and ($r.Out -like '*reason=snapshot_has_collisions*') -and ($r.Out -like '*code=collisions*'))
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_collisions' 'apply_refuses_declared_collisions' (($r.Code -eq 1) -and ($r.Out -like '*reason=snapshot_has_collisions*'))
    Check 'rs_gate_collisions' 'collision_image_touched_nothing' (Test-Untouched $g $tr)
    $g = New-RsFixture 'g_live' -BackupExtra @{ AllowLive = $true }
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    Check 'rs_fixture' 'live_image_taken_with_exit_4' (($g.BackupCode -eq 4) -and ($g.Snap -like '*-live') -and ($g.Sha.Length -eq 64))
    $r = Invoke-Rs $g
    Check 'rs_gate_live' 'dry_run_refuses_live_image' (($r.Code -eq 1) -and ($r.Out -like '*reason=live_snapshot_requires_flag*'))
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_live' 'apply_refuses_live_image' (($r.Code -eq 1) -and ($r.Out -like '*reason=live_snapshot_requires_flag*'))
    Check 'rs_gate_live' 'live_refusal_touched_nothing' (Test-Untouched $g $tr)
    $r = Invoke-Rs $g -Extra @{ AcceptLiveSnapshot = $true }
    Check 'rs_gate_live' 'accept_flag_allows_with_warning_exit_4_and_force_line' (($r.Code -eq 4) -and ($r.Out -like '*reason=live*') -and ($r.Out.Contains('[MMO_RESTORE_FORCE] flag=AcceptLiveSnapshot')))
    $r = Invoke-Rs $g -Apply -Extra @{ AcceptLiveSnapshot = $true }
    Check 'rs_gate_live' 'accepted_live_image_applies' (($r.Code -eq 4) -and ((Get-OracleSha (Join-Path $g.Saved $alis)) -ceq (Get-OracleSha (Join-Path $g.Snap ('files\' + $alis)))))
    $g = New-RsFixture 'g_src'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $saved2 = Join-Path $g.Dir 'saved2'
    Copy-Item -LiteralPath $g.Saved -Destination $saved2 -Recurse
    Set-Aged $saved2
    $tr2 = Get-TreeOracle $saved2
    $r = Invoke-Rs $g -SavedOverride $saved2
    Check 'rs_gate_source' 'different_source_refused_dry_run' (($r.Code -eq 1) -and ($r.Out -like '*reason=source_mismatch*'))
    $r = Invoke-Rs $g -Apply -SavedOverride $saved2
    Check 'rs_gate_source' 'different_source_refused_apply' (($r.Code -eq 1) -and ($r.Out -like '*reason=source_mismatch*'))
    Check 'rs_gate_source' 'different_source_touched_nothing' (((Get-TreeOracle $saved2) -ceq $tr2) -and (@(Get-SnapDirs $g.Root | Where-Object { $_ -like '*-prerestore' }).Count -eq 0))
    $r = Invoke-Rs $g -SavedOverride $saved2 -Extra @{ AcceptDifferentSource = $true }
    Check 'rs_gate_source' 'accept_flag_dry_run_exit_4_with_force_line' (($r.Code -eq 4) -and ($r.Out -like '*reason=different_source*') -and ($r.Out.Contains('[MMO_RESTORE_FORCE] flag=AcceptDifferentSource')))
    $r = Invoke-Rs $g -Apply -SavedOverride $saved2 -Extra @{ AcceptDifferentSource = $true }
    Check 'rs_gate_source' 'accept_flag_apply_writes_other_saved' (($r.Code -eq 4) -and ((Get-OracleSha (Join-Path $saved2 $alis)) -ceq (Get-OracleSha (Join-Path $g.Snap ('files\' + $alis)))))

    # 18h Secim: valid=false / yan dosya varsayilan disarida, bayrakla zorlanir
    $g = New-RsFixture 'g_invalid' -KeepBad
    $imageBadSha = Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Bad.sav')
    Set-Live $g.Saved 'SaveGames\MMOSave_Bad.sav' (New-TestBytes 100 9)
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $liveBadSha = Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Bad.sav')
    $r = Invoke-Rs $g
    Check 'rs_select_invalid' 'dry_run_skips_invalid_entry_by_default' (($r.Code -eq 4) -and ($r.Out.Contains('action=skip path=SaveGames/MMOSave_Bad.sav reason=invalid_entry')) -and ($r.Out -like '*reason=entries_not_restored*'))
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgePartialState = $true }
    Check 'rs_select_invalid' 'apply_leaves_invalid_entry_live_file_alone' (($r.Code -eq 4) -and ((Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Bad.sav')) -ceq $liveBadSha) -and ((Get-OracleSha (Join-Path $g.Saved $alis)) -ceq $imageAliSha))
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ IncludeInvalid = $true }
    Check 'rs_select_invalid' 'include_invalid_restores_it_with_force_line' (($r.Code -eq 4) -and ($r.Out.Contains('[MMO_RESTORE_FORCE] path=SaveGames/MMOSave_Bad.sav reason=invalid_entry')) -and ($r.Out -like '*reason=forced*') -and ((Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Bad.sav')) -ceq $imageBadSha))
    $g = New-RsFixture 'g_side'
    $imgTmp = Get-OracleSha (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.tmp')
    $imgBad = Get-OracleSha (Join-Path $g.Saved 'MMORace\MMORace_mmosave_ali.json.bad')
    Set-Live $g.Saved 'MMODaily\MMOSave_Ali.json.tmp' '{"version":1,"changed":'
    Set-Live $g.Saved 'MMORace\MMORace_mmosave_ali.json.bad' 'lock-live'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $liveTmp = Get-OracleSha (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.tmp')
    Set-Live $g.Saved 'MMORace\MMORace_mmosave_zed.json.bad' 'zlock'
    $zedBefore = Get-OracleSha (Join-Path $g.Saved 'MMORace\MMORace_mmosave_zed.json.bad')
    $r = Invoke-Rs $g
    Check 'rs_select_side' 'dry_run_warns_stale_live_tmp_and_exits_4' (($r.Code -eq 4) -and ($r.Out -like '*code=stale_side_file path=MMODaily/MMOSave_Ali.json.tmp*') -and ($r.Out -like '*reason=stale_side_files*'))
    Check 'rs_select_side' 'dry_run_marks_side_file_requires_flag' ($r.Out.Contains('action=skip path=MMODaily/MMOSave_Ali.json.tmp reason=side_file_requires_flag'))
    $trSide = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgePartialState = $true }
    Check 'rs_select_side' 'default_apply_leaves_side_files_alone' (($r.Code -eq 1) -and ($r.Out -like '*reason=stale_side_files_require_quarantine_flag*') -and (Test-Untouched $g $trSide))
    $r = Invoke-Rs $g -Apply -Extra @{ QuarantineStaleSideFiles = $true; AcknowledgePartialState = $true }
    $qDir = Join-Path (Get-PreDir $g) 'quarantine\MMODaily\MMOSave_Ali.json.tmp'
    Check 'rs_select_side' 'stale_live_tmp_reported_exit_4' (($r.Code -eq 4) -and ($r.Out -like '*reason=*stale_side_files*') -and ($r.Out -like '*code=stale_side_file path=MMODaily/MMOSave_Ali.json.tmp*') -and ($r.Out -like '*code=live_lock_file path=MMORace/MMORace_mmosave_ali.json.bad*'))
    Check 'rs_quarantine' 'stale_tmp_moved_to_preimage_quarantine_not_deleted' ((Test-Path -LiteralPath $qDir) -and ((Get-OracleSha $qDir) -ceq $liveTmp) -and -not (Test-Path -LiteralPath (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.tmp')))
    Check 'rs_quarantine' 'live_lock_not_moved_or_dropped_and_logged' (([IO.File]::ReadAllText((Join-Path $g.Saved 'MMORace\MMORace_mmosave_ali.json.bad')) -ceq 'lock-live') -and (Test-Path -LiteralPath (Join-Path $g.Saved 'MMORace\MMORace_mmosave_ali.json.bad')) -and ($r.Out.Contains('[MMO_RESTORE_QUARANTINE] path=MMODaily/MMOSave_Ali.json.tmp')))
    Check 'rs_select_side' 'default_apply_restored_main_file_only' (((Get-OracleSha (Join-Path $g.Saved 'MMORace\MMORace_mmosave_ali.json.bad')) -cne $imgBad) -and ((Get-OracleSha (Join-Path $g.Saved $alis)) -ceq $imageAliSha))
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ IncludeSideFiles = $true; AcknowledgePartialState = $true }
    Check 'rs_select_side' 'include_side_files_restores_tmp_and_bad_lock_with_force_line' (($r.Code -eq 4) -and ((Get-OracleSha (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.tmp')) -ceq $imgTmp) -and ((Get-OracleSha (Join-Path $g.Saved 'MMORace\MMORace_mmosave_ali.json.bad')) -ceq $imgBad) -and ($r.Out.Contains('[MMO_RESTORE_FORCE] flag=IncludeSideFiles')))
    Check 'rs_select_side' 'bad_lock_absent_from_image_not_dropped' ((Get-OracleSha (Join-Path $g.Saved 'MMORace\MMORace_mmosave_zed.json.bad')) -ceq $zedBefore)

    # 18i Kapi: surec denetimi ve canli yazim belirtisi (3), pre-restore zorunlulugu
    $g = New-RsFixture 'g_proc'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    # Kapi RestoreSaves'in KENDI denetimidir: pre-restore alt sureci (kendi surec denetimi var) hic calismamis olmali ([MMO_RESTORE_PRE] yok, preimage=none)
    $r = Invoke-Rs $g -Apply -Extra @{ ProcessProvider = $unrealProc }
    Check 'rs_gate_process' 'running_editor_defers_3' (($r.Code -eq 3) -and ($r.Out -like '*[[]MMO_RESTORE] status=deferred reason=process_running*') -and -not $r.Out.Contains('[MMO_RESTORE_PRE]') -and ($r.Out -like '*preimage=none*'))
    $r = Invoke-Rs $g -Apply -Extra @{ ProcessProvider = { @() } }
    Check 'rs_gate_process' 'empty_process_list_defers_3_fail_closed' (($r.Code -eq 3) -and ($r.Out -like '*[[]MMO_RESTORE] status=deferred reason=process_query_empty*') -and -not $r.Out.Contains('[MMO_RESTORE_PRE]'))
    $r = Invoke-Rs $g -Apply -Extra @{ ProcessProvider = { throw 'query failed' } }
    Check 'rs_gate_process' 'process_query_error_defers_3_fail_closed' (($r.Code -eq 3) -and ($r.Out -like '*[[]MMO_RESTORE] status=deferred reason=process_query_failed*') -and -not $r.Out.Contains('[MMO_RESTORE_PRE]'))
    Check 'rs_gate_process' 'process_refusals_touched_nothing_no_preimage' (Test-Untouched $g $tr)
    Check 'rs_gate_process' 'deferred_apply_recorded_in_last_restore_json' ((Test-Path -LiteralPath (Join-Path $g.Root 'last_restore.json')) -and (((Read-MMOUtf8Text (Join-Path $g.Root 'last_restore.json')).Text | ConvertFrom-Json).exit_code -eq 3))
    $global:rsProcFlip = $false
    $flipProv = { if ($global:rsProcFlip) { @([pscustomobject]@{ Name = 'UnrealEditor.exe'; ProcessId = 9; CommandLine = '' }) } else { @([pscustomobject]@{ Name = 'powershell.exe'; ProcessId = 4; CommandLine = 'x' }) } }
    $r = Invoke-Rs $g -Apply -Extra @{ ProcessProvider = $flipProv; TestHooks = @{ AfterPreimage = { param($pre) $global:rsProcFlip = $true } } }
    Check 'rs_gate_process' 'editor_started_after_preimage_defers_before_first_write' (($r.Code -eq 3) -and ($r.Out -like '*reason=process_running*') -and ((Get-OracleSha (Join-Path $g.Saved $alis)) -cne $imageAliSha) -and ((Get-PreDir $g) -ne ''))
    $global:rsProcFlip = $false
    $g = New-RsFixture 'g_recent'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    [IO.File]::SetLastWriteTimeUtc((Join-Path $g.Saved 'Logs\app.log'), [datetime]::UtcNow)
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_recent' 'recent_write_in_saved_defers_3' (($r.Code -eq 3) -and ($r.Out -like '*reason=recent_write_detected*'))
    Check 'rs_gate_recent' 'recent_write_touched_nothing_no_preimage' (Test-Untouched $g $tr)
    [IO.File]::SetLastWriteTimeUtc((Join-Path $g.Saved 'Logs\app.log'), (New-Object datetime(2026, 10, 27, 8, 0, 0, [DateTimeKind]::Utc)))
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_recent' 'quiet_saved_applies' ($r.Code -eq 0)
    $g = New-RsFixture 'g_prefail'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    $tsx = Get-RsTs
    [void][IO.Directory]::CreateDirectory((Join-Path $g.Root ($tsx + '-prerestore')))
    $r = Invoke-Rs $g -Apply -Extra @{ Timestamp = $tsx }
    Check 'rs_gate_preimage' 'preimage_failure_refuses_and_touches_nothing' (($r.Code -eq 1) -and ($r.Out -like '*reason=preimage_failed*') -and (((Get-TreeOracle $g.Saved) -ceq $tr)))
    $g = New-RsFixture 'g_predefer'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    $r = Invoke-TestRestore -Snap $g.Snap -Saved $g.Saved -Root (Join-Path $g.Saved 'bkroot') -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $g.Sha }
    Check 'rs_gate_preimage' 'unsafe_backup_root_defers_3_no_write' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_inside_saved*') -and ((Get-TreeOracle $g.Saved) -ceq $tr))
    $g = New-RsFixture 'g_tamper3'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $tr = Get-TreeOracle $g.Saved
    $aliBefore = Get-OracleSha (Join-Path $g.Saved $alis)
    $global:rsHook = @{ snap = $g.Snap }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFirstWrite = { param($pre) [IO.File]::WriteAllBytes((Join-Path $global:rsHook.snap 'files\SaveGames\MMOSave_Ali.sav'), (New-SavBytes 200 55)) } } }
    Check 'rs_gate_write' 'image_tampered_after_verification_caught_at_write_by_source_hash' (($r.Code -eq 1) -and ($r.Out -like '*write_failed_rolled_back:source_hash_mismatch*'))
    Check 'rs_gate_write' 'tampered_after_verification_left_target_untouched' (((Get-OracleSha (Join-Path $g.Saved $alis)) -ceq $aliBefore) -and ((Get-TreeOracle $g.Saved) -ceq $tr))

    # 18j Bos hedef (pre-restore gerekmez), hedef ad cakismasi, hedef dizin, junction
    $g = New-RsFixture 'g_fresh'
    foreach ($ff in (Get-ChildItem -LiteralPath $g.Saved -Recurse -File -Force | Where-Object { $_.DirectoryName -match '\\(SaveGames|MMOAchievements|MMODaily|MMOMail|MMORace)\z' })) { [IO.File]::Delete($ff.FullName) }
    $r = Invoke-Rs $g -Apply
    Check 'rs_fresh' 'restore_into_empty_target_exit_0_no_preimage_needed' (($r.Code -eq 0) -and ($r.Out -like '*preimage=none*') -and ((Get-PreDir $g) -eq ''))
    Check 'rs_fresh' 'restored_files_match_image_hashes' (((Get-OracleSha (Join-Path $g.Saved $alis)) -ceq $imageAliSha) -and (Test-Path -LiteralPath (Join-Path $g.Saved 'MMOMail\MMOMail_ali_baba.json')) -and -not (Test-Path -LiteralPath (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.tmp')))
    $g = New-RsFixture 'g_tcoll'
    [IO.File]::Move((Join-Path $g.Saved $alis), (Join-Path $g.Saved 'SaveGames\MMOSave_ali.sav'))
    $tr = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_target' 'target_name_collision_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=target_name_collision*'))
    Check 'rs_gate_target' 'target_name_collision_touched_nothing' (Test-Untouched $g $tr)
    $g = New-RsFixture 'g_tdir'
    [IO.File]::Delete((Join-Path $g.Saved $alis))
    [void][IO.Directory]::CreateDirectory((Join-Path $g.Saved $alis))
    $tr = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_target' 'target_that_is_a_directory_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=target_is_directory*') -and (Test-Untouched $g $tr))
    $g = New-RsFixture 'g_tjunc'
    $away = Join-Path $g.Dir 'daily_away'
    [IO.Directory]::Move((Join-Path $g.Saved 'MMODaily'), $away)
    New-TestJunction (Join-Path $g.Saved 'MMODaily') $away
    $r = Invoke-Rs $g -Apply
    Check 'rs_gate_target' 'reparse_target_directory_defers_3' (($r.Code -eq 3) -and ($r.Out -like '*reason=target_reparse*') -and (@(Get-SnapDirs $g.Root | Where-Object { $_ -like '*-prerestore' }).Count -eq 0))
    Remove-TestJunction (Join-Path $g.Saved 'MMODaily')

    # 18k Slot kapsami (-Slots): kanonik eslesme, irk mmosave_ oneki, kismi geri yukleme
    $g = New-RsFixture 'g_slots'
    $treeSl0 = Get-TreeOracle $g.Saved
    $imageBabaSha = Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav')
    $imageRaceSha = Get-OracleSha (Join-Path $g.Saved 'MMORace\MMORace_mmosave_ali.json')
    $imageBabaMail = Get-OracleSha (Join-Path $g.Saved 'MMOMail\MMOMail_ali_baba.json')
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    Set-Live $g.Saved 'SaveGames\MMOSave_Ali Baba.sav' (New-SavBytes 160 78)
    Set-Live $g.Saved 'MMOMail\MMOMail_ali_baba.json' '{"version":2,"mails":[],"x":1}'
    Set-Live $g.Saved 'MMORace\MMORace_mmosave_ali.json' '{"version":1,"race":3}'
    $babaLive = Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav')
    $mailLive = Get-OracleSha (Join-Path $g.Saved 'MMOMail\MMOMail_ali_baba.json')
    $stSl = Get-FullStamp $g.Dir
    $r = Invoke-Rs $g -Extra @{ Slots = @('ALI'); AcknowledgeCrossSlotRisk = $true }
    Check 'rs_slots' 'dry_run_slot_matches_canonical_case_insensitive' (($r.Code -eq 0) -and $r.Out.Contains('action=write path=SaveGames/MMOSave_Ali.sav slot=ali'))
    Check 'rs_slots' 'dry_run_includes_race_file_via_mmosave_prefix' ($r.Out.Contains('action=write path=MMORace/MMORace_mmosave_ali.json slot=ali'))
    Check 'rs_slots' 'dry_run_excludes_other_slot_group' ((-not $r.Out.Contains('MMOSave_Ali Baba.sav')) -and (-not $r.Out.Contains('MMOMail_ali_baba')))
    Check 'rs_slots' 'dry_run_slots_wrote_nothing' ((Get-FullStamp $g.Dir).Digest -ceq $stSl.Digest)
    $r = Invoke-Rs $g -Extra @{ Slots = @('nobody'); AcknowledgeCrossSlotRisk = $true }
    Check 'rs_slots' 'slot_missing_in_image_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=slot_not_in_snapshot*'))
    $r = Invoke-Rs $g -Apply -Extra @{ Slots = @('ali'); AcknowledgeCrossSlotRisk = $true }
    Check 'rs_slots' 'apply_restores_only_selected_slot' (($r.Code -eq 0) -and ((Get-OracleSha (Join-Path $g.Saved $alis)) -ceq $imageAliSha) -and ((Get-OracleSha (Join-Path $g.Saved 'MMORace\MMORace_mmosave_ali.json')) -ceq $imageRaceSha))
    Check 'rs_slots' 'other_slot_files_left_modified' (((Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav')) -ceq $babaLive) -and ((Get-OracleSha (Join-Path $g.Saved 'MMOMail\MMOMail_ali_baba.json')) -ceq $mailLive))
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ Slots = @('Ali Baba'); AcknowledgeCrossSlotRisk = $true }
    Check 'rs_slots' 'space_slot_maps_to_underscore_key_for_mail_and_sav' (($r.Code -eq 0) -and ((Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav')) -ceq $imageBabaSha) -and ((Get-OracleSha (Join-Path $g.Saved 'MMOMail\MMOMail_ali_baba.json')) -ceq $imageBabaMail))
    Check 'rs_slots' 'two_partial_restores_reproduce_image_tree' ((Get-TreeOracle $g.Saved) -ceq $treeSl0)

    # 18l Rollback: yazim hatasi, kalanlara devam edilmez, yeni dosya geri alinir, rollback hatasi, yazim sonrasi dogrulama
    function New-RbFixture {
        param([string]$Name)
        $f = New-RsFixture $Name
        Set-Live $f.Saved 'MMODaily\MMOSave_Ali.json' '{"version":1,"day":1,"bounties":[],"n":2}'
        Set-Live $f.Saved 'SaveGames\MMOSave_Ali Baba.sav' (New-SavBytes 160 78)
        Set-Live $f.Saved $alis (New-SavBytes 300 77)
        return $f
    }
    $failAt2 = { param($rel, $idx) $global:rsHook.attempts.Add($rel) | Out-Null; if ($idx -eq 2) { [void][IO.Directory]::CreateDirectory($global:rsHook.victim) } }
    $g = New-RbFixture 'g_rb1'
    $tr = Get-TreeOracle $g.Saved
    $global:rsHook = @{ attempts = (New-Object System.Collections.ArrayList); victim = (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav.restore.tmp') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFile = $failAt2 } }
    Check 'rs_rollback' 'write_failure_exit_1_rolled_back' (($r.Code -eq 1) -and ($r.Out -like '*reason=write_failed_rolled_back:restore_tmp_exists*') -and ($r.Out -like '*status=rolled_back*'))
    Check 'rs_rollback' 'remaining_files_not_attempted_after_failure' (($global:rsHook.attempts.Count -eq 2) -and ($global:rsHook.attempts[1] -ceq 'SaveGames/MMOSave_Ali Baba.sav'))
    Check 'rs_rollback' 'first_written_file_restored_from_preimage' ((Get-TreeOracle $g.Saved) -ceq $tr)
    Check 'rs_rollback' 'preimage_kept_after_rollback' ((Get-PreDir $g) -ne '')
    [IO.Directory]::Delete($global:rsHook.victim)
    $g = New-RbFixture 'g_rb2'
    [IO.File]::Delete((Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json'))
    $tr = Get-TreeOracle $g.Saved
    $global:rsHook = @{ attempts = (New-Object System.Collections.ArrayList); victim = (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav.restore.tmp') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFile = $failAt2 } }
    Check 'rs_rollback' 'newly_created_file_removed_on_rollback' (($r.Code -eq 1) -and -not (Test-Path -LiteralPath (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json')) -and ((Get-TreeOracle $g.Saved) -ceq $tr))
    [IO.Directory]::Delete($global:rsHook.victim)
    $g = New-RbFixture 'g_rb3'
    $global:rsHook = @{ attempts = (New-Object System.Collections.ArrayList); victim = (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav.restore.tmp') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFile = $failAt2; BeforeRollback = { param($pre) [IO.File]::Delete((Join-Path $pre 'files\MMODaily\MMOSave_Ali.json')) } } }
    Check 'rs_rollback' 'rollback_failure_reported_with_preimage_name' (($r.Code -eq 1) -and ($r.Out -like '*status=rollback_failed*') -and ($r.Out -like ('*[[]MMO_RESTORE_ROLLBACK_FAILED] preimage=' + ([IO.Path]::GetFileName((Get-PreDir $g))) + '*')))
    [IO.Directory]::Delete($global:rsHook.victim)
    $g = New-RbFixture 'g_rb4'
    $tr = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ AfterWrite = { param($rel, $dest) if ($rel -ceq 'MMODaily/MMOSave_Ali.json') { Write-TestText $dest '{"version":1,"day":1,"bounties":[],"n":2}' } } } }
    Check 'rs_rollback' 'post_write_verification_failure_rolls_back' (($r.Code -eq 1) -and ($r.Out -like '*write_failed_rolled_back:post_verify_failed*') -and ((Get-TreeOracle $g.Saved) -ceq $tr))
    $g = New-RbFixture 'g_rb4b'
    $trB = Get-TreeOracle $g.Saved
    $dailyLive = Get-OracleSha (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json')
    $babaLiveB = Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav')
    $aliLiveB = Get-OracleSha (Join-Path $g.Saved $alis)
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ AfterWrite = { param($rel, $dest) if ($rel -ceq 'MMODaily/MMOSave_Ali.json') { [IO.File]::WriteAllBytes($dest, (New-TestBytes 50 1)) } } } }
    $asideP = Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.rollback-conflict'
    Check 'rs_conflict' 'foreign_content_at_rollback_is_conflict_exit_1' (($r.Code -eq 1) -and ($r.Out -like '*status=rollback_conflict*') -and ($r.Out -like '*reason=rollback_conflict:post_verify_failed*') -and $r.Out.Contains('[MMO_RESTORE_ROLLBACK_CONFLICT] path=MMODaily/MMOSave_Ali.json'))
    Check 'rs_conflict' 'conflicting_target_not_overwritten' ((Get-OracleSha (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json')) -ceq (Get-BytesSha (New-TestBytes 50 1)))
    Check 'rs_conflict' 'preimage_bytes_written_beside_as_rollback_conflict' ((Test-Path -LiteralPath $asideP) -and ((Get-OracleSha $asideP) -ceq $dailyLive))
    Check 'rs_conflict' 'marker_kept_after_conflict' (Test-Path -LiteralPath (Join-Path $g.Root 'restore_in_progress.json'))
    Check 'rs_conflict' 'other_files_rolled_back_normally' (((Get-OracleSha (Join-Path $g.Saved $alis)) -ceq $aliLiveB) -and ((Get-OracleSha (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav')) -ceq $babaLiveB))
    $g = New-RbFixture 'g_rb6'
    $tr = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ AfterTmp = { param($rel, $tmp, $dest) [IO.File]::WriteAllBytes($tmp, (New-TestBytes 50 2)) } } }
    Check 'rs_rollback' 'corrupted_tmp_caught_by_readback_before_placement' (($r.Code -eq 1) -and ($r.Out -like '*write_failed_rolled_back:tmp_verify_failed*') -and ((Get-TreeOracle $g.Saved) -ceq $tr) -and (@(Get-ChildItem -LiteralPath $g.Saved -Recurse -Force -File | Where-Object { $_.Name -like '*.restore.tmp' }).Count -eq 0))
    $g = New-RbFixture 'g_rb5'
    Write-TestText (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav.restore.tmp') 'junk'
    Set-Aged $g.Saved
    $tr = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply
    Check 'rs_rollback' 'leftover_restore_tmp_blocks_write_and_is_not_deleted' (($r.Code -eq 3) -and ($r.Out -like '*reason=leftover_restore_artifacts_require_recover*') -and (Test-Untouched $g $tr))

    # 18m Manifest uyarilari kuru calisma ciktisinda gorunur
    $g = New-RsFixture 'w_ign' -Prep { param($s) Write-TestBytes (Join-Path $s ('SaveGames\MMOSave_' + $oUml + 'mer.sav')) (New-SavBytes 100 3) }
    $r = Invoke-Rs $g
    Check 'rs_warn' 'ignored_player_data_shown_and_exit_4' (($r.Code -eq 4) -and ($r.Out -like '*code=manifest_ignored_player_data count=1*') -and ($r.Out -like '*reason=snapshot_ignored_player_data*'))
    $g = New-RsFixture 'w_stale' -BackupExtra @{ RequireCleanShutdown = $true } -Prep { param($s) Write-TestText (Join-Path $s 'Logs\MMOShutdownResult.json') '{"run_id":"20261027T070000Z","exit_code":0,"time_utc":"2026-10-27T07:00:00Z"}' }
    Check 'rs_fixture' 'stale_proof_image_taken_with_exit_4' ($g.BackupCode -eq 4)
    $r = Invoke-Rs $g
    Check 'rs_warn' 'proof_stale_shown_and_exit_4' (($r.Code -eq 4) -and ($r.Out -like '*code=manifest_proof_stale*') -and ($r.Out -like '*reason=proof_stale*'))

    # ---- 18n. B3.1: bagimsiz guvenlik denetimi bulgulari (tek okuma, is-isareti, rollback cakismasi, capraz-slot, karantina, yol iliskisi, dongu denetimi, temizleme) ----
    $bumpManifest = { param($s) $mp = Join-Path $s 'manifest.json'; $b = [IO.File]::ReadAllBytes($mp); [IO.File]::WriteAllBytes($mp, [byte[]]($b + [byte[]](32))) }
    $g = New-RsFixture 'n_dig'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $trN = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Extra @{ TestHooks = @{ AfterVerify = $bumpManifest } }
    Check 'rs_digest' 'manifest_swapped_after_single_read_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=manifest_changed_during_verify*'))
    $g = New-RsFixture 'n_dig2'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $trN = Get-TreeOracle $g.Saved
    $global:rsHook = @{ snap = $g.Snap }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ AfterPreimage = { param($pre) & $bumpManifest $global:rsHook.snap } } }
    Check 'rs_digest' 'manifest_swapped_before_first_write_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=manifest_changed_before_write*') -and ((Get-TreeOracle $g.Saved) -ceq $trN))
    $gA = New-RsFixture 'n_digA'
    $gB2 = New-RsFixture 'n_digB2' -Prep { param($s) Write-TestText (Join-Path $s 'MMOMail\MMOMail_zed.json') '{"version":2,"mails":[]}' }
    $rmB = Read-MMOManifest -SnapshotDir $gA.Snap -ManifestBytes ([IO.File]::ReadAllBytes((Join-Path $gB2.Snap 'manifest.json')))
    Check 'rs_digest' 'library_parses_the_supplied_bytes_not_the_file' ($rmB.Ok -and (@($rmB.Manifest.entries | Where-Object { $_.path -ceq 'MMOMail/MMOMail_zed.json' }).Count -eq 1))
    $tsB = Test-MMOSnapshot -SnapshotDir $gA.Snap -ManifestBytes ([IO.File]::ReadAllBytes((Join-Path $gB2.Snap 'manifest.json')))
    Check 'rs_digest' 'snapshot_check_uses_supplied_bytes_and_catches_mismatch' ((-not $tsB.Ok) -and (HasErr $tsB.Errors 'entry_unusable:MMOMail/MMOMail_zed.json'))

    # Isaret + begin satiri + .restore.old temizligi (mutlu yol)
    $g = New-RsFixture 'n_mk'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $global:rsHook = @{ root = $g.Root; markerText = ''; begin = $false }
    $hkMk = { param($pre) $mp = Join-Path $global:rsHook.root 'restore_in_progress.json'; if (Test-Path -LiteralPath $mp) { $global:rsHook.markerText = [IO.File]::ReadAllText($mp) }; $lg = Join-Path $pre 'restore.log'; $global:rsHook.begin = ((Test-Path -LiteralPath $lg) -and ([IO.File]::ReadAllText($lg)).Contains('[MMO_RESTORE_BEGIN]')) }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFirstWrite = $hkMk } }
    $mkPre = [IO.Path]::GetFileName((Get-PreDir $g))
    Check 'rs_marker' 'marker_written_before_first_write_with_preimage_name' (($r.Code -eq 0) -and $global:rsHook.markerText.Contains($mkPre) -and $global:rsHook.markerText.Contains($g.Sha))
    Check 'rs_marker' 'begin_line_in_restore_log_before_first_write' ($global:rsHook.begin -eq $true)
    Check 'rs_marker' 'marker_removed_after_success' (-not (Test-Path -LiteralPath (Join-Path $g.Root 'restore_in_progress.json')))
    Check 'rs_marker' 'no_restore_old_or_tmp_left_after_success' (@(Get-ChildItem -LiteralPath $g.Saved -Recurse -Force -File | Where-Object { $_.Name -like '*.restore.old' -or $_.Name -like '*.restore.tmp' }).Count -eq 0)

    # Rollback calismazken (surec basladi) isaret kalir, yeni -Apply reddedilir, tek geri alma komutu calisir
    $g = New-RbFixture 'n_nq'
    $trN = Get-TreeOracle $g.Saved
    $global:rsProcFlip = $false
    $r = Invoke-Rs $g -Apply -Extra @{ ProcessProvider = $flipProv; TestHooks = @{ QuietEvery = 1; BeforeFile = { param($rel, $idx) if ($idx -eq 2) { $global:rsProcFlip = $true } } } }
    $trPartial = Get-TreeOracle $g.Saved
    $hintLine = @($r.Lines | Where-Object { $_ -like '*[[]MMO_RESTORE_ROLLBACK_FAILED]*' })
    Check 'rs_loop' 'process_started_mid_loop_stops_writing' (($r.Code -eq 1) -and ($r.Out -like '*reason=rollback_failed:process_running*') -and ($r.Out -like '*status=rollback_failed*'))
    Check 'rs_loop' 'no_rollback_attempted_while_process_running' (($trPartial -cne $trN) -and ($r.Out -like '*reason=rollback_not_quiet:process_running*'))
    Check 'rs_marker' 'marker_kept_when_rollback_failed' (Test-Path -LiteralPath (Join-Path $g.Root 'restore_in_progress.json'))
    $global:rsProcFlip = $false
    $r2 = Invoke-Rs $g -Apply
    Check 'rs_marker' 'stale_marker_defers_new_apply_3_and_touches_nothing' (($r2.Code -eq 3) -and ($r2.Out -like '*reason=restore_in_progress_marker*') -and ((Get-TreeOracle $g.Saved) -ceq $trPartial))
    $r2 = Invoke-Rs $g
    Check 'rs_marker' 'dry_run_warns_about_marker_exit_4' (($r2.Code -eq 4) -and ($r2.Out -like '*code=restore_in_progress_marker*') -and ($r2.Out -like '*reason=restore_marker_present*'))
    $mm = $null
    if ($hintLine.Count -ge 1) { $mm = [regex]::Match($hintLine[0], 'RestoreSaves\.ps1 -Snapshot \x27(?<s>[^\x27]+)\x27(?<f>(?: -[A-Za-z]+)+) -ExpectedManifestSha256 (?<h>[0-9a-f]{64}) -Apply') }
    if ($null -eq $mm) { $mm = [regex]::Match('', 'x') }
    Check 'rs_hint' 'failure_line_prints_one_complete_rollback_command' (($null -ne $mm) -and $mm.Success -and ($mm.Groups['f'].Value -like '* -All -IncludeInvalid -IncludeSideFiles -AcceptCollisions -AcknowledgePartialState -QuarantineStaleSideFiles -AcknowledgeStaleRestoreMarker'))
    Set-Aged $g.Saved
    # B3.2 (O1): yazim yarida kaldi (.restore.old kaldi) -> normal -Apply (geri alma komutu dahil) once -Recover ister
    $rcOut = Invoke-TestRestore -Snap $mm.Groups['s'].Value -Saved $g.Saved -Root $g.Root -Extra @{ Recover = $true; Apply = $true; ExpectedManifestSha256 = $mm.Groups['h'].Value }
    Check 'rs_hint' 'recover_apply_clears_restore_old_leftover_before_rollback_command' ((($rcOut.Code -eq 0) -or ($rcOut.Code -eq 4)) -and (@(Get-ChildItem -LiteralPath $g.Saved -Recurse -Force -File | Where-Object { $_.Name -like '*.restore.old' }).Count -eq 0))
    Set-Aged $g.Saved
    $rbx = @{ Apply = $true; ExpectedManifestSha256 = $mm.Groups['h'].Value }
    foreach ($fl in ($mm.Groups['f'].Value.Trim() -split ' ')) { if ($fl -ne '') { $rbx[$fl.TrimStart('-')] = $true } }
    $r3 = [pscustomobject]@{ Code = -1; Out = '' }
    if ($mm.Success) { $r3 = Invoke-TestRestore -Snap $mm.Groups['s'].Value -Saved $g.Saved -Root $g.Root -Extra $rbx }
    Check 'rs_hint' 'printed_command_executed_as_is_restores_pre_state' ((($r3.Code -eq 0) -or ($r3.Code -eq 4)) -and ((Get-TreeOracle $g.Saved) -ceq $trN))
    Check 'rs_hint' 'printed_digest_equals_independent_preimage_manifest_hash' ($mm.Success -and ($mm.Groups['h'].Value -ceq (Get-OracleSha (Join-Path $mm.Groups['s'].Value 'manifest.json'))))
    Check 'rs_marker' 'marker_removed_after_hinted_rollback_succeeds' (-not (Test-Path -LiteralPath (Join-Path $g.Root 'restore_in_progress.json')))
    $g = New-RbFixture 'n_ack'
    $global:rsHook = @{ attempts = (New-Object System.Collections.ArrayList); victim = (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav.restore.tmp') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFile = $failAt2 } }
    Check 'rs_marker' 'marker_removed_after_full_rollback' (($r.Code -eq 1) -and ($r.Out -like '*status=rolled_back*') -and -not (Test-Path -LiteralPath (Join-Path $g.Root 'restore_in_progress.json')))
    [IO.Directory]::Delete($global:rsHook.victim)
    $g = New-RbFixture 'n_ack2'
    Set-Aged $g.Saved
    Write-TestText (Join-Path $g.Root 'restore_in_progress.json') '{"tool":"x","snapshot":"s","preimage":"p","time_utc":"2026-10-31T00:00:00Z"}'
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgeStaleRestoreMarker = $true }
    Check 'rs_marker' 'acknowledge_flag_allows_apply_exit_4_with_force_line' (($r.Code -eq 4) -and ($r.Out -like '*reason=stale_marker_acknowledged*') -and $r.Out.Contains('[MMO_RESTORE_FORCE] flag=AcknowledgeStaleRestoreMarker') -and -not (Test-Path -LiteralPath (Join-Path $g.Root 'restore_in_progress.json')))
    $g = New-RbFixture 'n_mkdir'
    Set-Aged $g.Saved
    $trN = Get-TreeOracle $g.Saved
    [void][IO.Directory]::CreateDirectory((Join-Path $g.Root 'restore_in_progress.json'))
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgeStaleRestoreMarker = $true }
    Check 'rs_marker' 'marker_that_cannot_be_written_refuses_before_any_write' (($r.Code -eq 1) -and ($r.Out -like '*reason=restore_marker_write_failed*') -and ((Get-TreeOracle $g.Saved) -ceq $trN))

    # Rollback: yeni dosya degistirildiyse silinmez; Replace/Move yarida hata; karantina geri tasinir
    $g = New-RbFixture 'n_nf'
    [IO.File]::Delete((Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json'))
    $global:rsHook = @{ attempts = (New-Object System.Collections.ArrayList); victim = (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav.restore.tmp'); p = (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFile = $failAt2; BeforeRollback = { param($pre) [IO.File]::WriteAllBytes($global:rsHook.p, (New-TestBytes 33 9)) } } }
    Check 'rs_conflict' 'new_file_changed_before_rollback_is_kept_and_reported' (($r.Code -eq 1) -and ($r.Out -like '*reason=rollback_conflict:restore_tmp_exists*') -and ((Get-OracleSha (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json')) -ceq (Get-BytesSha (New-TestBytes 33 9))) -and $r.Out.Contains('[MMO_RESTORE_ROLLBACK_CONFLICT] path=MMODaily/MMOSave_Ali.json'))
    [IO.Directory]::Delete($global:rsHook.victim)
    $g = New-RbFixture 'n_pf1'
    $trN = Get-TreeOracle $g.Saved
    $global:rsHook = @{ fs = $null; tmp = '' }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ AfterDisplace = { param($rel, $tmp, $dest, $old) if ($rel -ceq 'MMODaily/MMOSave_Ali.json') { $global:rsHook.tmp = $tmp; $global:rsHook.fs = New-Object IO.FileStream($tmp, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None) } } } }
    $global:rsHook.fs.Dispose()
    if (Test-Path -LiteralPath $global:rsHook.tmp) { [IO.File]::Delete($global:rsHook.tmp) }
    Check 'rs_place' 'move_in_failure_puts_old_file_back_and_rolls_back' (($r.Code -eq 1) -and ($r.Out -like '*reason=write_failed_rolled_back:place_failed*') -and ((Get-TreeOracle $g.Saved) -ceq $trN))
    $g = New-RbFixture 'n_pf2'
    $dailyLive2 = Get-OracleSha (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json')
    $global:rsHook = @{ dir = (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ AfterDisplace = { param($rel, $tmp, $dest, $old) if ($rel -ceq 'MMODaily/MMOSave_Ali.json') { [void][IO.Directory]::CreateDirectory($dest) } } } }
    $oldP = Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.restore.old'
    $asP = Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.rollback-conflict'
    Check 'rs_place' 'partial_failure_old_displaced_and_not_restorable_is_conflict' (($r.Code -eq 1) -and ($r.Out -like '*reason=rollback_conflict:place_failed_old_displaced*'))
    Check 'rs_place' 'displaced_old_file_kept_and_preimage_copy_written_aside' ((Test-Path -LiteralPath $oldP) -and ((Get-OracleSha $oldP) -ceq $dailyLive2) -and (Test-Path -LiteralPath $asP) -and ((Get-OracleSha $asP) -ceq $dailyLive2) -and (Test-Path -LiteralPath $global:rsHook.dir -PathType Container))
    [IO.Directory]::Delete($global:rsHook.dir)
    $r = Invoke-Rs $g
    Check 'rs_place' 'leftover_artifacts_reported_not_deleted' (($r.Code -eq 4) -and ($r.Out -like '*reason=*leftover_restore_artifacts*') -and ($r.Out -like '*code=leftover_restore_artifact path=MMODaily/MMOSave_Ali.json.restore.old*') -and ($r.Out -like '*code=leftover_restore_artifact path=MMODaily/MMOSave_Ali.json.rollback-conflict*') -and (Test-Path -LiteralPath $oldP))
    $g = New-RbFixture 'n_oe'
    Write-TestText (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.restore.old') 'precious-old-data'
    Set-Aged $g.Saved
    $trN = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply
    Check 'rs_place' 'leftover_old_with_other_content_blocks_write_and_is_kept' (($r.Code -eq 3) -and ($r.Out -like '*reason=leftover_restore_artifacts_require_recover*') -and (Test-Untouched $g $trN))

    # Karantina: yazim hatasinda eski yan dosya yerine geri tasinir
    $g = New-RbFixture 'n_qr'
    Set-Live $g.Saved 'MMODaily\MMOSave_Ali.json.tmp' '{"version":1,"changed":'
    Set-Aged $g.Saved
    $trN = Get-TreeOracle $g.Saved
    $global:rsHook = @{ attempts = (New-Object System.Collections.ArrayList); victim = (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav.restore.tmp') }
    $r = Invoke-Rs $g -Apply -Extra @{ QuarantineStaleSideFiles = $true; TestHooks = @{ BeforeFile = $failAt2 } }
    [IO.Directory]::Delete($global:rsHook.victim)
    Check 'rs_quarantine' 'quarantined_side_file_moved_back_on_rollback' (($r.Code -eq 1) -and ($r.Out -like '*status=rolled_back*') -and ((Get-TreeOracle $g.Saved) -ceq $trN))

    # Yazim aninda reparse yeniden dogrulamasi
    $g = New-RbFixture 'n_dr'
    [IO.File]::Delete((Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json'))
    $away = Join-Path $g.Dir 'dr_away'
    [void][IO.Directory]::CreateDirectory($away)
    $global:rsHook = @{ away = $away }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforePlace = { param($rel, $tmp, $dest) if ($rel -ceq 'MMODaily/MMOSave_Ali.json') { [void](New-Item -ItemType Junction -Path $dest -Target $global:rsHook.away) } } } }
    Remove-TestJunction (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json')
    Check 'rs_reparse_write' 'dest_turned_into_junction_at_write_time_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=write_failed_rolled_back:dest_reparse*') -and (@(Get-ChildItem -LiteralPath $away -Force).Count -eq 0) -and -not (Test-Path -LiteralPath (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.restore.tmp')))
    $g = New-RbFixture 'n_dd'
    $trN = Get-TreeOracle $g.Saved
    $away2 = Join-Path $g.Dir 'dd_away'
    $global:rsHook = @{ away = $away2; dir = (Join-Path $g.Saved 'MMODaily') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforePlace = { param($rel, $tmp, $dest) if ($rel -ceq 'MMODaily/MMOSave_Ali.json') { [IO.Directory]::Move($global:rsHook.dir, $global:rsHook.away); [void](New-Item -ItemType Junction -Path $global:rsHook.dir -Target $global:rsHook.away) } } } }
    Remove-TestJunction $global:rsHook.dir
    [IO.Directory]::Move($away2, $global:rsHook.dir)
    Check 'rs_reparse_write' 'target_dir_turned_into_junction_at_write_time_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=write_failed_rolled_back:target_dir_reparse*') -and ((Get-TreeOracle $g.Saved) -ceq $trN))

    # Plan ile yazim arasi kayma: hedef degisti / belirdi (kullanici verisi KORUNUR)
    $g = New-RbFixture 'n_cs'
    $global:rsHook = @{ p = (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFile = { param($rel, $idx) if ($idx -eq 1) { Write-TestText $global:rsHook.p 'changed-by-someone'; [IO.File]::SetLastWriteTimeUtc($global:rsHook.p, (New-Object datetime(2026, 10, 27, 8, 0, 0, [DateTimeKind]::Utc))) } } } }
    Check 'rs_drift' 'target_changed_after_plan_refused_and_not_overwritten' (($r.Code -eq 1) -and ($r.Out -like '*write_failed_rolled_back:target_changed_since_plan*') -and (([IO.File]::ReadAllText($global:rsHook.p)) -ceq 'changed-by-someone'))
    $g = New-RbFixture 'n_ca'
    [IO.File]::Delete((Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json'))
    $global:rsHook = @{ p = (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFile = { param($rel, $idx) if ($idx -eq 1) { Write-TestText $global:rsHook.p 'appeared'; [IO.File]::SetLastWriteTimeUtc($global:rsHook.p, (New-Object datetime(2026, 10, 27, 8, 0, 0, [DateTimeKind]::Utc))) } } } }
    Check 'rs_drift' 'target_appeared_after_plan_refused_and_kept' (($r.Code -eq 1) -and ($r.Out -like '*write_failed_rolled_back:target_appeared_since_plan*') -and (([IO.File]::ReadAllText($global:rsHook.p)) -ceq 'appeared'))

    # Pre-restore dallari
    $g = New-RbFixture 'n_pm'
    $global:rsHook = @{ p = (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforePreimage = { param($live) [IO.File]::Delete($global:rsHook.p) } } }
    Check 'rs_preimage' 'overwrite_target_missing_from_preimage_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=preimage_missing_entry*') -and -not (Test-Path -LiteralPath (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json.restore.tmp')))
    $g = New-RbFixture 'n_pc'
    $global:rsHook = @{ p = (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforePreimage = { param($live) Write-TestText $global:rsHook.p 'changed-during-preimage' } } }
    Check 'rs_preimage' 'target_changed_between_plan_and_preimage_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=target_changed_during_preimage*') -and (([IO.File]::ReadAllText($global:rsHook.p)) -ceq 'changed-during-preimage'))
    $g = New-RbFixture 'n_pn'
    $global:rsHook = @{ p = (Join-Path $g.Saved 'MMODaily\MMOSave_Ali.json'); fs = $null }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforePreimage = { param($live) $global:rsHook.fs = New-Object IO.FileStream($global:rsHook.p, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None) } } }
    $global:rsHook.fs.Dispose()
    Check 'rs_preimage' 'unreadable_live_file_in_preimage_refused_no_content' (($r.Code -eq 1) -and ($r.Out -like '*reason=preimage_no_content*'))
    $g = New-RbFixture 'n_pq'
    $trN = Get-TreeOracle $g.Saved
    $global:rsHook = @{ snap = $g.Snap }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ AfterPreimageTaken = { param($pre) Edit-TestManifest $pre { param($m) $m.mode = 'live' } -FixDigest } } }
    Check 'rs_preimage' 'non_quiesced_preimage_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=preimage_not_quiesced*') -and ((Get-TreeOracle $g.Saved) -ceq $trN))
    $g = New-RbFixture 'n_pv'
    $trN = Get-TreeOracle $g.Saved
    $global:rsHook = @{ snap = $g.Snap }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ AfterPreimageTaken = { param($pre) [IO.File]::WriteAllBytes((Join-Path $pre 'files\SaveGames\MMOSave_Ali.sav'), (New-SavBytes 77 3)) } } }
    Check 'rs_preimage' 'corrupted_preimage_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=preimage_verify_failed*') -and ((Get-TreeOracle $g.Saved) -ceq $trN))

    # Secimde hicbir sey kalmadi
    $g = New-RsFixture 'n_nt' -KeepBad
    $r = Invoke-Rs $g -Extra @{ Slots = @('bad'); AcknowledgeCrossSlotRisk = $true }
    Check 'rs_select_empty' 'slot_whose_only_entry_is_invalid_has_nothing_to_restore' (($r.Code -eq 1) -and ($r.Out -like '*reason=nothing_to_restore*') -and ($r.Out -like '*code=nothing_to_restore*'))

    # Capraz-slot / kismi durum kapilari
    $g = New-RsFixture 'n_px'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    Set-Live $g.Saved 'SaveGames\MMOSave_Extra.sav' (New-SavBytes 120 5)
    Set-Aged $g.Saved
    $trN = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g
    Check 'rs_partial' 'dry_run_says_apply_would_refuse_for_extras' (($r.Code -eq 0) -and ($r.Out -like '*code=partial_state extras=1*') -and ($r.Out -like '*code=apply_would_refuse reason=partial_state_requires_acknowledge*'))
    $r = Invoke-Rs $g -Apply
    Check 'rs_partial' 'all_with_extra_live_file_refused_without_acknowledge' (($r.Code -eq 1) -and ($r.Out -like '*reason=partial_state_requires_acknowledge*') -and (Test-Untouched $g $trN))
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgePartialState = $true }
    Check 'rs_partial' 'acknowledged_partial_state_applies_exit_4_with_force_line' (($r.Code -eq 4) -and ($r.Out -like '*reason=partial_state_acknowledged*') -and $r.Out.Contains('[MMO_RESTORE_FORCE] flag=AcknowledgePartialState extras=1'))
    $g = New-RsFixture 'n_pn2' -KeepBad
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    Set-Aged $g.Saved
    $trN = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply
    Check 'rs_partial' 'all_with_unrestored_entry_refused_without_acknowledge' (($r.Code -eq 1) -and ($r.Out -like '*reason=partial_state_requires_acknowledge*') -and (Test-Untouched $g $trN))
    Check 'rs_partial' 'cross_slot_warning_makes_no_safest_claim' ((-not $r.Out.ToLowerInvariant().Contains('en guvenli')) -and $r.Out.Contains('DOGRULAYAMAZ'))
    $mkMailFx = {
        param([string]$Name, [string]$MailText)
        $f = New-RsFixture $Name
        Set-Live $f.Saved $alis (New-SavBytes 300 77)
        Set-Live $f.Saved 'MMOMail\MMOMail_ali_baba.json' $MailText
        Set-Aged $f.Saved
        return $f
    }
    $slotX = @{ Slots = @('ali'); AcknowledgeCrossSlotRisk = $true }
    $g = & $mkMailFx 'n_ml1' '{"version":1,"mails":[{"from":"Ali","text":"hi","gold":500,"sent":1}]}'
    $trN = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Extra $slotX
    Check 'rs_mail' 'dry_run_reports_sender_trace_in_unrestored_mailbox' (($r.Code -eq 0) -and ($r.Out -like '*code=mail_trace mailbox=MMOMail/MMOMail_ali_baba.json slot=ali*'))
    $r = Invoke-Rs $g -Apply -Extra $slotX
    Check 'rs_mail' 'slot_restore_with_sender_trace_refused_without_acknowledge' (($r.Code -eq 1) -and ($r.Out -like '*reason=partial_state_requires_acknowledge*') -and (Test-Untouched $g $trN))
    $slotXA = @{ Slots = @('ali'); AcknowledgeCrossSlotRisk = $true; AcknowledgePartialState = $true }
    $r = Invoke-Rs $g -Apply -Extra $slotXA
    Check 'rs_mail' 'acknowledged_trace_applies_exit_4' (($r.Code -eq 4) -and ($r.Out -like '*reason=partial_state_acknowledged*') -and ((Get-OracleSha (Join-Path $g.Saved $alis)) -ceq $imageAliSha))
    $g = & $mkMailFx 'n_ml2' '{"version":2,"mails":[{"from":"Zed","text":"gold from Ali for you","gold":5,"sent":1}]}'
    $r = Invoke-Rs $g -Apply -Extra $slotX
    Check 'rs_mail' 'text_mention_of_slot_counts_as_trace' (($r.Code -eq 1) -and ($r.Out -like '*code=mail_trace*') -and ($r.Out -like '*reason=partial_state_requires_acknowledge*'))
    $g = & $mkMailFx 'n_ml3' 'this is not json'
    $r = Invoke-Rs $g -Apply -Extra $slotX
    Check 'rs_mail' 'unparseable_mailbox_is_unverified_and_requires_acknowledge' (($r.Code -eq 1) -and ($r.Out -like '*code=mail_unverified mailbox=MMOMail/MMOMail_ali_baba.json*') -and ($r.Out -like '*reason=partial_state_requires_acknowledge*'))
    $g = & $mkMailFx 'n_ml4' '{"version":9,"mails":[]}'
    $r = Invoke-Rs $g -Apply -Extra $slotX
    Check 'rs_mail' 'unknown_mail_schema_version_is_unverified' (($r.Code -eq 1) -and ($r.Out -like '*code=mail_unverified*'))
    $g = & $mkMailFx 'n_ml5' '{"version":2,"mails":[{"from":"Zed","text":"hello","gold":0,"sent":1}]}'
    $r = Invoke-Rs $g -Apply -Extra $slotX
    Check 'rs_mail' 'clean_unrelated_mailbox_needs_no_extra_acknowledge' (($r.Code -eq 0) -and -not ($r.Out -like '*code=mail_*'))

    # Collisions: yalniz pre-restore goruntusunden geri almada kabul edilir
    $g = New-RsFixture 'n_cl'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    Set-Live $g.Saved 'SaveGames\MMOSave_Zed One.sav' (New-SavBytes 120 21)
    Set-Live $g.Saved 'SaveGames\MMOSave_Zed_One.sav' (New-SavBytes 121 22)
    Set-Aged $g.Saved
    $trCl0 = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgePartialState = $true; AcceptCollisions = $true }
    Check 'rs_collisions' 'accept_collisions_refused_for_normal_snapshot_exit_2' (($r.Code -eq 2) -and ($r.Out -like '*reason=accept_collisions_only_for_preimage*'))
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgePartialState = $true }
    $preCl = Get-PreDir $g
    Check 'rs_collisions' 'colliding_live_names_do_not_block_normal_restore_and_land_in_preimage' (($r.Code -eq 4) -and ($preCl -ne '') -and ((Read-TestManifest $preCl).PSObject.Properties['collisions'] -ne $null))
    Set-Aged $g.Saved
    $preClSha = Get-OracleSha (Join-Path $preCl 'manifest.json')
    $r = Invoke-TestRestore -Snap $preCl -Saved $g.Saved -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $preClSha; IncludeInvalid = $true; IncludeSideFiles = $true; AcknowledgePartialState = $true }
    Check 'rs_collisions' 'preimage_with_collisions_refused_by_default' (($r.Code -eq 1) -and ($r.Out -like '*reason=snapshot_has_collisions*'))
    $r = Invoke-TestRestore -Snap $preCl -Saved $g.Saved -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $preClSha; IncludeInvalid = $true; IncludeSideFiles = $true; AcknowledgePartialState = $true; AcceptCollisions = $true }
    Check 'rs_collisions' 'accept_collisions_rolls_back_preimage_with_colliding_names' ((($r.Code -eq 4)) -and ($r.Out -like '*reason=*collisions_accepted*') -and $r.Out.Contains('[MMO_RESTORE_FORCE] flag=AcceptCollisions') -and ((Get-TreeOracle $g.Saved) -ceq $trCl0))

    # Yol iliskisi
    $g = New-RsFixture 'n_pr'
    $trN = Get-TreeOracle $g.Saved
    $r = Invoke-TestRestore -Snap $g.Snap -Saved (Join-Path $g.Snap 'files') -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $g.Sha }
    Check 'rs_relation' 'saved_inside_snapshot_deferred_3' (($r.Code -eq 3) -and ($r.Out -like '*reason=saved_inside_snapshot*'))
    $svInRoot = Join-Path $g.Root 'svroot'
    [void][IO.Directory]::CreateDirectory($svInRoot)
    $r = Invoke-TestRestore -Snap $g.Snap -Saved $svInRoot -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $g.Sha }
    Check 'rs_relation' 'saved_inside_backup_root_deferred_3' (($r.Code -eq 3) -and ($r.Out -like '*reason=saved_inside_backup_root*'))
    $r = Invoke-TestRestore -Snap $g.Snap -Saved $g.Root -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $g.Sha }
    Check 'rs_relation' 'saved_equal_to_backup_root_deferred_3' ($r.Code -eq 3 -and ($r.Out -like '*reason=snapshot_inside_saved*'))
    Check 'rs_relation' 'relation_gates_touched_nothing' ((Get-TreeOracle $g.Saved) -ceq $trN)

    # Kok atasi junction: bayraksiz red, bayrakla gecer
    $g = New-RsFixture 'n_anc'
    $jt = Join-Path $g.Dir 'jt'
    New-TestJunction $jt $g.Dir
    $svJ = Join-Path $jt 'saved'
    $r = Invoke-TestRestore -Snap $g.Snap -Saved $svJ -Root $g.Root -Extra @{ All = $true }
    Check 'rs_ancestor' 'junction_in_saved_ancestors_deferred_without_flag' (($r.Code -eq 3) -and ($r.Out -like '*reason=saved_reparse_root_ancestor*'))
    $r = Invoke-TestRestore -Snap $g.Snap -Saved $svJ -Root $g.Root -Extra @{ All = $true; AllowRootAncestorReparse = $true; AcceptDifferentSource = $true }
    Check 'rs_ancestor' 'allow_flag_lets_dry_run_through' (($r.Code -eq 4) -and ($r.Out -like '*reason=different_source*'))
    Remove-TestJunction $jt

    # Kontrol karakteri temizligi (manifestten gelen serbest metin)
    $g = New-RsFixture 'n_san'
    $evil = "x`n[MMO_RESTORE_APPLIED] written=999 fake" + [string][char]27 + '[31m'
    Edit-TestManifest $g.Snap { param($m) $m.source_saved_dir = $evil } -FixDigest
    $g.Sha = Get-OracleSha (Join-Path $g.Snap 'manifest.json')
    $r = Invoke-Rs $g
    Check 'rs_sanitize' 'manifest_text_cannot_inject_a_log_line' (($r.Code -eq 1) -and (@($r.Lines | Where-Object { $_.StartsWith('[MMO_RESTORE_APPLIED]') }).Count -eq 0) -and $r.Out.Contains('manifest_source=x?[MMO_RESTORE_APPLIED] written=999 fake?[31m'))
    # B3.2 (D9): satir bazli denetim (\x0a artik dislanmaz: her satir tek satirdir) ve C1 araligi \x80-\x9f dahil
    Check 'rs_sanitize' 'no_control_character_reaches_the_console' (@($r.Lines | Where-Object { $_ -cmatch '[\x00-\x1f\x7f-\x9f]' }).Count -eq 0)
    $g = New-RsFixture 'n_san2'
    $evil2 = 'a' + [char]0x202E + 'b' + [char]0x2028 + 'c' + [char]0x2800 + 'd' + [char]0x3164 + 'e' + [char]0x0085 + 'f' + [char]0x009B + 'g' + [char]0x2066 + 'h' + [char]0x2029
    Edit-TestManifest $g.Snap { param($m) $m.source_saved_dir = $evil2 } -FixDigest
    $g.Sha = Get-OracleSha (Join-Path $g.Snap 'manifest.json')
    $r = Invoke-Rs $g
    Check 'rs_sanitize' 'bidi_line_separator_braille_hangul_filler_and_c1_are_neutralized' ($r.Out.Contains('manifest_source=a?b?c?d?e?f?g?h?'))
    Check 'rs_sanitize' 'no_hidden_or_bidi_character_on_any_console_line' (@($r.Lines | Where-Object { $_ -cmatch '[\x00-\x1f\x7f-\x9f\u202a-\u202e\u2066-\u2069\u2028\u2029\u2800\u3164]' }).Count -eq 0)

    # Yazim dongusu: canli yazim belirtisi, yazim penceresi, durum dosyasi
    $g = New-RbFixture 'n_mw'
    $trN = Get-TreeOracle $g.Saved
    $global:rsHook = @{ plant = (Join-Path $g.Saved 'MMOMail\MMOMail_zzz.json') }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ QuietEvery = 1; BeforeFile = { param($rel, $idx) if ($idx -eq 2) { Write-TestText $global:rsHook.plant '{"version":2,"mails":[]}' } }; BeforeRollback = { param($pre) [IO.File]::Delete($global:rsHook.plant) } } }
    Check 'rs_loop' 'live_write_appearing_mid_loop_stops_and_rolls_back' (($r.Code -eq 1) -and ($r.Out -like '*reason=write_failed_rolled_back:recent_write_detected*') -and ((Get-TreeOracle $g.Saved) -ceq $trN))
    $g = New-RsFixture 'n_fc'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ RecentWriteSeconds = 30 }
    $preFc = Get-PreDir $g
    $lrF = (Read-MMOUtf8Text (Join-Path $g.Root 'last_restore.json')).Text | ConvertFrom-Json
    Check 'rs_force' 'short_recent_write_window_logged_as_force_exit_4' (($r.Code -eq 4) -and $r.Out.Contains('[MMO_RESTORE_FORCE] flag=RecentWriteSeconds value=30') -and ($r.Out -like '*reason=forced*') -and ([IO.File]::ReadAllText((Join-Path $preFc 'restore.log'))).Contains('flag=RecentWriteSeconds value=30'))
    Check 'rs_force' 'last_restore_json_records_process_check_and_flags' (($lrF.process_check -ceq 'injected') -and (@($lrF.flags) -contains 'All') -and (@($lrF.flags) -contains 'RecentWriteSeconds=30'))
    Check 'rs_force' 'other_machines_warning_printed_on_apply_and_dry_run' ($r.Out.Contains('code=other_machines') -and (Invoke-Rs $g).Out.Contains('code=other_machines'))
    $g = New-RsFixture 'n_acl'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    Set-Aged $g.Saved
    $aclLogs = Join-Path $g.Saved 'Logs'
    $aclMe2 = $env:USERDOMAIN + '\' + $env:USERNAME
    try {
        & icacls.exe $aclLogs /deny ($aclMe2 + ':(RD)') | Out-Null
        $r = Invoke-Rs $g -Apply
    } finally { & icacls.exe $aclLogs /remove:d $aclMe2 | Out-Null }
    Check 'rs_loop' 'unlistable_saved_subdirectory_defers_3_fail_closed' (($r.Code -eq 3) -and ($r.Out -like '*reason=recent_write_check_failed*') -and (@(Get-SnapDirs $g.Root | Where-Object { $_ -like '*-prerestore' }).Count -eq 0))

    # Fikstur dizinleri icin icerik ozetli "degismedi" kaniti
    $g = New-RsFixture 'n_cd'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    $stCd0 = Get-SavedStamp -SavedRoot $g.Saved -WithContent
    $r = Invoke-Rs $g
    Check 'rs_content_stamp' 'dry_run_leaves_fixture_saved_content_digest_unchanged' (($r.Code -eq 0) -and ((Get-SavedStamp -SavedRoot $g.Saved -WithContent).Digest -ceq $stCd0.Digest))
    $r = Invoke-Rs $g -Apply
    Check 'rs_content_stamp' 'apply_changes_fixture_saved_content_digest' (($r.Code -eq 0) -and ((Get-SavedStamp -SavedRoot $g.Saved -WithContent).Digest -cne $stCd0.Digest))

    # ---- 18o. B3.2: son guvenlik denetimi bulgulari (O1 recover, O2 zincir, O3 kilit/isaret, D1-D10) ---------------------------------------
    $mkP = 'restore_in_progress.json'
    $mkM = 'MMORestoreInProgress.json'
    $hintRx = 'RestoreSaves\.ps1 -Snapshot \x27(?<s>[^\x27]+)\x27(?<f>(?: -[A-Za-z]+)+)(?: (?<sl>[a-z0-9_,-]+))?(?<g>(?: -[A-Za-z]+)*) -ExpectedManifestSha256 (?<h>[0-9a-f]{64}) -Apply'
    function Get-TreeNoMk {
        param([string]$Dir)
        $ls = @((Get-TreeOracle $Dir) -split "`n" | Where-Object { $_ -notlike ('\' + $mkM + '|*') })
        return ($ls -join "`n")
    }
    function Get-LeftoverNames {
        param([string]$Saved)
        return , @(Get-ChildItem -LiteralPath $Saved -Recurse -Force -File | Where-Object { $_.Name -match '\.(restore\.old|restore\.tmp|rollback\.old|rollback-conflict)$' } | ForEach-Object { $_.Name })
    }
    $wrapPath = Join-Path $fx 'kill_wrapper.ps1'
    Write-TestText $wrapPath @'
param([string]$Snap, [string]$Saved, [string]$Root, [string]$Sha, [string]$Ts, [string]$Rel, [string]$Point, [string]$Script, [string]$Extra)
$env:MMO_BACKUP_TESTMODE = '1'
$benign = { @([pscustomobject]@{ Name = 'powershell.exe'; ProcessId = 4; CommandLine = 'x' }, [pscustomobject]@{ Name = 'explorer.exe'; ProcessId = 5; CommandLine = '' }) }
$global:KillRel = $Rel
$hk = @{}
$hk[$Point] = { param($a) if ($global:KillRel -ceq '*' -or $a -ceq $global:KillRel) { Stop-Process -Id $PID -Force } }
$p = @{ Snapshot = $Snap; SavedDir = $Saved; BackupRoot = $Root; All = $true; Apply = $true; ExpectedManifestSha256 = $Sha; Timestamp = $Ts; ProcessProvider = $benign; TestHooks = $hk }
if ($Extra -ne 'none') { foreach ($e in $Extra.Split(',')) { $p[$e] = $true } }
& $Script @p
'@
    function Invoke-KilledRestore {
        # Ayri powershell sureci; kanca noktasinda Stop-Process ile OLDURULUR (temizleme kodu CALISMAZ = gercek cokus benzesimi).
        param($F, [string]$Point, [string]$Rel = '*', [string]$Extra = 'none')
        $ts = Get-RsTs
        $global:LASTEXITCODE = -999
        $o = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $wrapPath -Snap $F.Snap -Saved $F.Saved -Root $F.Root -Sha $F.Sha -Ts $ts -Rel $Rel -Point $Point -Script $rsScript -Extra $Extra *>&1 | ForEach-Object { [string]$_ })
        return [pscustomobject]@{ Code = $global:LASTEXITCODE; Out = ($o -join "`n") }
    }
    function Invoke-Recover {
        param($F, [string]$PreDir, [switch]$Apply, [string]$Sha = '')
        $e = @{ Recover = $true }
        if ($Apply) { $e['Apply'] = $true; $e['ExpectedManifestSha256'] = $(if ($Sha -ne '') { $Sha } else { Get-OracleSha (Join-Path $PreDir 'manifest.json') }) }
        return (Invoke-TestRestore -Snap $PreDir -Saved $F.Saved -Root $F.Root -Extra $e)
    }
    $dailyRel = 'MMODaily/MMOSave_Ali.json'
    $dailyWin = 'MMODaily\MMOSave_Ali.json'

    # --- O1 A: olum AfterDisplace (hedef YOK + .restore.old + .restore.tmp) ---
    $g = New-RbFixture 'o_kA'
    $trA = Get-TreeOracle $g.Saved
    $dPath = Join-Path $g.Saved $dailyWin
    $dLive = Get-OracleSha $dPath
    $dImg = Get-OracleSha (Join-Path $g.Snap ('files\' + $dailyWin))
    $k = Invoke-KilledRestore $g 'AfterDisplace' $dailyRel
    $preA = Get-PreDir $g
    Check 'rs_crash' 'killed_process_leaves_dest_missing_with_old_and_tmp' (($k.Code -ne 0) -and -not (Test-Path -LiteralPath $dPath) -and (Test-Path -LiteralPath ($dPath + '.restore.old')) -and (Test-Path -LiteralPath ($dPath + '.restore.tmp')) -and ((Get-OracleSha ($dPath + '.restore.old')) -ceq $dLive) -and ((Get-OracleSha ($dPath + '.restore.tmp')) -ceq $dImg))
    Check 'rs_crash' 'killed_process_leaves_marker_in_backup_root_and_saved_mirror' ((Test-Path -LiteralPath (Join-Path $g.Root $mkP)) -and (Test-Path -LiteralPath (Join-Path $g.Saved $mkM)) -and (([IO.File]::ReadAllText((Join-Path $g.Root $mkP))) -ceq ([IO.File]::ReadAllText((Join-Path $g.Saved $mkM)))))
    Set-Aged $g.Saved
    $stK = Get-TreeOracle $g.Saved
    $nPreK = @(Get-SnapDirs $g.Root | Where-Object { $_ -like '*-prerestore' }).Count
    $r = Invoke-Rs $g -Apply
    Check 'rs_recover' 'normal_apply_refused_by_marker_exit_3' (($r.Code -eq 3) -and ($r.Out -like '*reason=restore_in_progress_marker*'))
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgeStaleRestoreMarker = $true }
    Check 'rs_recover' 'normal_apply_with_ack_still_refused_by_leftovers_before_preimage' (($r.Code -eq 3) -and ($r.Out -like '*reason=leftover_restore_artifacts_require_recover*') -and ((Get-TreeOracle $g.Saved) -ceq $stK) -and (@(Get-SnapDirs $g.Root | Where-Object { $_ -like '*-prerestore' }).Count -eq $nPreK))
    $stAll = Get-FullStamp $g.Dir
    $r = Invoke-Recover $g $preA
    Check 'rs_recover' 'dry_run_plans_old_restore_and_tmp_discard' (($r.Code -eq 0) -and $r.Out.Contains('action=old_restore path=MMODaily/MMOSave_Ali.json.restore.old') -and $r.Out.Contains('action=tmp_discard path=MMODaily/MMOSave_Ali.json.restore.tmp') -and ($r.Out -like '*result* mode=recover-dry-run*'))
    Check 'rs_recover' 'dry_run_wrote_nothing' ((Get-FullStamp $g.Dir).Digest -ceq $stAll.Digest)
    $r = Invoke-Recover $g $preA -Apply -Sha ('1' * 64)
    Check 'rs_recover' 'apply_with_wrong_digest_refused_nothing_changed' (($r.Code -eq 1) -and ($r.Out -like '*reason=manifest_sha256_mismatch*') -and ((Get-TreeOracle $g.Saved) -ceq $stK))
    $r = Invoke-Recover $g $preA -Apply
    Check 'rs_recover' 'apply_restores_old_discards_tmp_removes_marker_exit_0' (($r.Code -eq 0) -and ((Get-OracleSha $dPath) -ceq $dLive) -and ((Get-LeftoverNames $g.Saved).Count -eq 0) -and -not (Test-Path -LiteralPath (Join-Path $g.Root $mkP)) -and -not (Test-Path -LiteralPath (Join-Path $g.Saved $mkM)) -and ((Get-TreeOracle $g.Saved) -ceq $trA))
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Apply
    Check 'rs_recover' 'normal_apply_works_after_recover' (($r.Code -eq 0) -and ((Get-OracleSha $dPath) -ceq $dImg))

    # --- O1 B: olum AfterWrite (ilk dosya yazildi, .restore.old kaldi): karisik durum, isaret KALIR, geri alma komutu ---
    $g = New-RbFixture 'o_kB'
    $trB = Get-TreeOracle $g.Saved
    $k = Invoke-KilledRestore $g 'AfterWrite' $dailyRel
    $preB = Get-PreDir $g
    $dPathB = Join-Path $g.Saved $dailyWin
    Set-Aged $g.Saved
    $r = Invoke-Recover $g $preB
    Check 'rs_recover' 'completed_file_old_is_planned_for_quarantine' ($r.Out.Contains('action=old_quarantine path=MMODaily/MMOSave_Ali.json.restore.old') -and ($r.Out.Contains('at_planned=1')))
    $r = Invoke-Recover $g $preB -Apply
    $nextLine = @($r.Lines | Where-Object { $_ -like '*geri al: RestoreSaves.ps1*' })
    $mb = [regex]::Match('', 'x')
    if ($nextLine.Count -ge 1) { $mb = [regex]::Match($nextLine[0], $hintRx) }
    Check 'rs_recover' 'mixed_state_keeps_marker_exit_4_and_prints_rollback_command' (($r.Code -eq 4) -and ($r.Out -like '*recover_state_mixed_marker_kept*') -and (Test-Path -LiteralPath (Join-Path $g.Root $mkP)) -and $mb.Success -and ((Get-LeftoverNames $g.Saved).Count -eq 0))
    $rbxB = @{ Apply = $true; ExpectedManifestSha256 = $mb.Groups['h'].Value }
    foreach ($fl in (($mb.Groups['f'].Value + $mb.Groups['g'].Value).Trim() -split ' ')) { if ($fl -ne '') { $rbxB[$fl.TrimStart('-')] = $true } }
    Set-Aged $g.Saved
    $r3 = [pscustomobject]@{ Code = -1; Out = '' }
    if ($mb.Success) { $r3 = Invoke-TestRestore -Snap $mb.Groups['s'].Value -Saved $g.Saved -Root $g.Root -Extra $rbxB }
    Check 'rs_recover' 'recover_then_printed_rollback_command_restores_pre_state_and_closes_marker' ((($r3.Code -eq 0) -or ($r3.Code -eq 4)) -and ((Get-TreeOracle $g.Saved) -ceq $trB) -and -not (Test-Path -LiteralPath (Join-Path $g.Root $mkP)))

    # --- O1 C/D: olum AfterTmp (hedef saglam, tmp var); tmp icerigi farkliysa karantina ---
    $g = New-RbFixture 'o_kC'
    $trC = Get-TreeOracle $g.Saved
    $k = Invoke-KilledRestore $g 'AfterTmp' $dailyRel
    $preC = Get-PreDir $g
    Set-Aged $g.Saved
    $r = Invoke-Recover $g $preC
    Check 'rs_recover' 'tmp_equal_to_planned_content_is_discard_candidate' ($r.Out.Contains('action=tmp_discard path=MMODaily/MMOSave_Ali.json.restore.tmp'))
    $r = Invoke-Recover $g $preC -Apply
    Check 'rs_recover' 'tmp_only_crash_recovers_to_pre_state_exit_0' (($r.Code -eq 0) -and ((Get-TreeOracle $g.Saved) -ceq $trC) -and -not (Test-Path -LiteralPath (Join-Path $g.Root $mkP)))
    $g = New-RbFixture 'o_kD'
    $trD = Get-TreeOracle $g.Saved
    $k = Invoke-KilledRestore $g 'AfterTmp' $dailyRel
    $preD = Get-PreDir $g
    $tmpD = Join-Path $g.Saved ($dailyWin + '.restore.tmp')
    [IO.File]::WriteAllBytes($tmpD, (New-TestBytes 40 7))
    $tmpDSha = Get-BytesSha (New-TestBytes 40 7)
    Set-Aged $g.Saved
    $r = Invoke-Recover $g $preD
    Check 'rs_recover' 'tmp_with_other_content_is_quarantine_not_discard' ($r.Out.Contains('action=tmp_quarantine path=MMODaily/MMOSave_Ali.json.restore.tmp') -and -not $r.Out.Contains('action=tmp_discard'))
    $r = Invoke-Recover $g $preD -Apply
    $qHit = @(Get-ChildItem -LiteralPath $g.Root -Recurse -Force -File | Where-Object { $_.FullName -like '*restore_recover_*\quarantine\MMODaily\MMOSave_Ali.json.restore.tmp' })
    Check 'rs_recover' 'foreign_tmp_moved_to_recover_quarantine_not_deleted' (($r.Code -eq 0) -and ($qHit.Count -eq 1) -and ((Get-OracleSha $qHit[0].FullName) -ceq $tmpDSha) -and (-not (Test-Path -LiteralPath $tmpD)) -and ((Get-TreeOracle $g.Saved) -ceq $trD))

    # --- O1 E: old icerigi goruntude yok -> unresolved, isaret kalir, dokunulmaz ---
    $g = New-RbFixture 'o_kE'
    $k = Invoke-KilledRestore $g 'AfterDisplace' $dailyRel
    $preE = Get-PreDir $g
    $oldE = Join-Path $g.Saved ($dailyWin + '.restore.old')
    [IO.File]::WriteAllBytes($oldE, (New-TestBytes 41 8))
    Set-Aged $g.Saved
    $r = Invoke-Recover $g $preE
    Check 'rs_recover' 'old_with_unverifiable_hash_is_unresolved' ($r.Out.Contains('action=unresolved path=MMODaily/MMOSave_Ali.json.restore.old reason=old_hash_not_in_preimage') -and ($r.Code -eq 4))
    $r = Invoke-Recover $g $preE -Apply
    Check 'rs_recover' 'unresolved_apply_exit_1_marker_kept_old_untouched' (($r.Code -eq 1) -and ($r.Out -like '*reason=recover_incomplete*') -and (Test-Path -LiteralPath (Join-Path $g.Root $mkP)) -and ((Get-OracleSha $oldE) -ceq (Get-BytesSha (New-TestBytes 41 8))))
    $mkTxt = [IO.File]::ReadAllText((Join-Path $g.Root $mkP))
    [IO.File]::WriteAllText((Join-Path $g.Root $mkP), $mkTxt.Replace($preE.Substring($preE.LastIndexOf('\') + 1), '20270101T000000Z-prerestore'))
    $r = Invoke-Recover $g $preE
    Check 'rs_recover' 'marker_naming_another_preimage_refused' (($r.Code -eq 1) -and ($r.Out -like '*reason=recover_marker_mismatch*'))
    $gN = New-RsFixture 'o_kG'
    $r = Invoke-TestRestore -Snap $gN.Snap -Saved $gN.Saved -Root $gN.Root -Extra @{ Recover = $true }
    Check 'rs_recover' 'non_preimage_snapshot_refused_for_recover' (($r.Code -eq 1) -and ($r.Out -like '*reason=recover_snapshot_not_preimage*'))
    $r = Invoke-TestRestore -Snap $gN.Snap -Saved $gN.Saved -Root $gN.Root -Extra @{ Recover = $true; All = $true }
    Check 'rs_recover' 'recover_with_scope_is_usage_error' (($r.Code -eq 2) -and ($r.Out -like '*reason=recover_excludes_scope*'))

    # --- D10: beklenmedik istisnada isaret kalir; recover kapsar ---
    $g = New-RbFixture 'o_kF'
    $trF = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFirstWrite = { param($pre) throw 'boom' } } }
    $preF = Get-PreDir $g
    Check 'rs_unexpected' 'unexpected_exception_exit_1_marker_left_with_recover_hint' (($r.Code -eq 1) -and ($r.Out -like '*reason=unexpected:*') -and ($r.Out -like '*code=marker_left_after_unexpected*') -and ($r.Out -like '*RestoreSaves.ps1 -Recover -Snapshot*') -and (Test-Path -LiteralPath (Join-Path $g.Root $mkP)) -and (Test-Path -LiteralPath (Join-Path $g.Saved $mkM)))
    Set-Aged $g.Saved
    $r = Invoke-Recover $g $preF -Apply
    Check 'rs_unexpected' 'recover_closes_marker_left_by_unexpected_exception' (($r.Code -eq 0) -and -not (Test-Path -LiteralPath (Join-Path $g.Root $mkP)) -and -not (Test-Path -LiteralPath (Join-Path $g.Saved $mkM)) -and ((Get-TreeOracle $g.Saved) -ceq $trF))

    # --- O2: bayat isaret zinciri ---
    $g = New-RbFixture 'o_ch1'
    $k = Invoke-KilledRestore $g 'BeforeFirstWrite'
    $prevPre = [IO.Path]::GetFileName((Get-PreDir $g))
    Check 'rs_chain' 'kill_before_first_write_leaves_marker_and_no_leftovers' (($k.Code -ne 0) -and (Test-Path -LiteralPath (Join-Path $g.Root $mkP)) -and ((Get-LeftoverNames $g.Saved).Count -eq 0))
    Set-Aged $g.Saved
    $global:rsHook = @{ root = $g.Root; mk = '' }
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgeStaleRestoreMarker = $true; TestHooks = @{ BeforeFirstWrite = { param($pre) $global:rsHook.mk = [IO.File]::ReadAllText((Join-Path $global:rsHook.root 'restore_in_progress.json')) } } }
    $mkj = $null
    if ($global:rsHook.mk -ne '') { $mkj = $global:rsHook.mk | ConvertFrom-Json }
    $staleF = @(Get-ChildItem -LiteralPath $g.Root -Force -File | Where-Object { $_.Name -like '*.stale' })
    Check 'rs_chain' 'acknowledged_stale_marker_is_moved_not_deleted_both_copies' (($r.Code -eq 4) -and ($r.Out -like '*reason=stale_marker_acknowledged*') -and ($staleF.Count -eq 2) -and (@($staleF | Where-Object { ([IO.File]::ReadAllText($_.FullName)).Contains($prevPre) }).Count -eq 2) -and (@(Get-ChildItem -LiteralPath $g.Saved -Force -File | Where-Object { $_.Name -like '*.stale' }).Count -eq 0))
    Check 'rs_chain' 'new_marker_carries_old_preimage_as_chain' (($null -ne $mkj) -and (@($mkj.chain).Count -eq 1) -and ($mkj.chain[0].preimage -ceq $prevPre) -and ($mkj.tool -ceq 'RestoreSaves/3') -and ($mkj.plan.Count -ge 1))
    Check 'rs_chain' 'full_successful_restore_closes_chain_and_removes_marker' (-not (Test-Path -LiteralPath (Join-Path $g.Root $mkP)) -and -not (Test-Path -LiteralPath (Join-Path $g.Saved $mkM)))
    $g = New-RbFixture 'o_ch2'
    $trCh = Get-TreeOracle $g.Saved
    $k = Invoke-KilledRestore $g 'BeforeFirstWrite'
    Set-Aged $g.Saved
    $global:rsHook = @{ attempts = (New-Object System.Collections.ArrayList); victim = (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav.restore.tmp') }
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgeStaleRestoreMarker = $true; TestHooks = @{ BeforeFile = $failAt2 } }
    [IO.Directory]::Delete($global:rsHook.victim)
    $mkj2 = $null
    if (Test-Path -LiteralPath (Join-Path $g.Root $mkP)) { $mkj2 = [IO.File]::ReadAllText((Join-Path $g.Root $mkP)) | ConvertFrom-Json }
    Check 'rs_chain' 'full_rollback_with_open_chain_keeps_marker_and_warns' (($r.Code -eq 1) -and ($r.Out -like '*status=rolled_back*') -and ($r.Out -like '*code=restore_chain_open*') -and ($null -ne $mkj2) -and (@($mkj2.chain).Count -eq 1) -and (Test-Path -LiteralPath (Join-Path $g.Saved $mkM)) -and ((Get-TreeNoMk $g.Saved) -ceq ($trCh -split "`n" -join "`n")))
    $g = New-RbFixture 'o_ch3'
    $k = Invoke-KilledRestore $g 'BeforeFirstWrite'
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgeStaleRestoreMarker = $true; Slots = @('ali'); AcknowledgeCrossSlotRisk = $true }
    Check 'rs_chain' 'slot_restore_with_open_chain_keeps_marker_exit_4' (($r.Code -eq 4) -and ($r.Out -like '*restore_chain_open*') -and (Test-Path -LiteralPath (Join-Path $g.Root $mkP)))

    # --- O3: kilit + isaret ---
    $g = New-RbFixture 'o_lk'
    Set-Aged $g.Saved
    $trLk = Get-TreeOracle $g.Saved
    $lkFs = New-Object IO.FileStream((Join-Path $g.Root '.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $r = Invoke-Rs $g -Apply
    $lkFs.Dispose()
    Check 'rs_lock' 'second_apply_while_lock_held_defers_3_and_touches_nothing' (($r.Code -eq 3) -and ($r.Out -like '*[[]MMO_RESTORE] status=deferred reason=lock_held*') -and ($r.Out -notlike '*[[]MMO_RESTORE_PRE]*') -and (Test-Untouched $g $trLk))
    $g = New-RbFixture 'o_bk'
    $global:rsHook = @{ saved = $g.Saved; root = $g.Root; code = -1; out = ''; p = $false; m = $false; same = $false }
    $hkBk = { param($pre)
        $bk = Invoke-TestBackup $global:rsHook.saved $global:rsHook.root @{ Timestamp = (Get-RsTs); Label = 'dur' }
        $global:rsHook.code = $bk.Code; $global:rsHook.out = $bk.Out
        $pp = Join-Path $global:rsHook.root 'restore_in_progress.json'; $mm = Join-Path $global:rsHook.saved 'MMORestoreInProgress.json'
        $global:rsHook.p = (Test-Path -LiteralPath $pp); $global:rsHook.m = (Test-Path -LiteralPath $mm)
        if ($global:rsHook.p -and $global:rsHook.m) { $global:rsHook.same = (([IO.File]::ReadAllText($pp)) -ceq ([IO.File]::ReadAllText($mm))) }
    }
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeFirstWrite = $hkBk } }
    Check 'rs_lock' 'backup_during_restore_apply_defers_on_lock' (($r.Code -eq 0) -and ($global:rsHook.code -eq 3) -and ($global:rsHook.out -like '*reason=lock_held*') -and (@(Get-SnapDirs $g.Root | Where-Object { $_ -like '*-dur' }).Count -eq 0))
    Check 'rs_marker' 'marker_exists_in_both_places_with_same_content_during_apply' (($global:rsHook.p -eq $true) -and ($global:rsHook.m -eq $true) -and ($global:rsHook.same -eq $true))
    $lkProbe = $null
    try { $lkProbe = New-Object IO.FileStream((Join-Path $g.Root '.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None, 4096, [IO.FileOptions]::DeleteOnClose) } catch { $lkProbe = $null }
    Check 'rs_lock' 'lock_released_after_run' ($null -ne $lkProbe)
    if ($null -ne $lkProbe) { $lkProbe.Dispose() }
    $g = New-RsFixture 'o_bm'
    $lastOkSha = Get-OracleSha (Join-Path $g.Root 'last_ok.json')
    Write-TestText (Join-Path $g.Root $mkP) '{"x":1}'
    $r = Invoke-TestBackup $g.Saved $g.Root @{ Timestamp = (Get-RsTs); Label = 'dur' }
    Check 'rs_marker' 'backup_defers_3_when_primary_marker_present' (($r.Code -eq 3) -and ($r.Out -like '*reason=restore_in_progress*') -and (@(Get-SnapDirs $g.Root | Where-Object { $_ -like '*-dur' }).Count -eq 0))
    [IO.File]::Delete((Join-Path $g.Root $mkP))
    Write-TestText (Join-Path $g.Saved $mkM) '{"x":1}'
    $r = Invoke-TestBackup $g.Saved $g.Root @{ Timestamp = (Get-RsTs); Label = 'dur' }
    Check 'rs_marker' 'backup_defers_3_when_only_saved_mirror_present' (($r.Code -eq 3) -and ($r.Out -like '*reason=restore_in_progress*'))
    $r = Invoke-TestBackup $g.Saved $g.Root @{ Timestamp = (Get-RsTs); Label = 'prerestore' }
    Check 'rs_marker' 'prerestore_label_allowed_but_exit_4_and_last_ok_not_updated' (($r.Code -eq 4) -and ($r.Out -like '*reason=restore_marker_present*') -and ((Get-OracleSha (Join-Path $g.Root 'last_ok.json')) -ceq $lastOkSha))
    $r = Invoke-Rs $g -Apply
    Check 'rs_marker' 'restore_apply_refused_when_only_saved_mirror_present' (($r.Code -eq 3) -and ($r.Out -like '*reason=restore_in_progress_marker*') -and ($r.Out -like '*copies=saved_mirror_only*'))
    [IO.File]::Delete((Join-Path $g.Saved $mkM))
    $g = New-RbFixture 'o_mx'
    Set-Aged $g.Saved
    $trMx = Get-TreeOracle $g.Saved
    $global:rsHook = @{ p = (Join-Path $g.Root $mkP) }
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ BeforeMarkerWrite = { param($mp) Write-TestText $global:rsHook.p '{"x":1}' } } }
    Check 'rs_marker' 'marker_created_by_a_concurrent_run_before_write_refuses_exit_3_nothing_written' (($r.Code -eq 3) -and ($r.Out -like '*reason=restore_marker_exists*') -and ((Get-TreeOracle $g.Saved) -ceq $trMx) -and (([IO.File]::ReadAllText($global:rsHook.p)) -ceq '{"x":1}'))

    # --- D1: gercek yol (junction / 8.3) iliskisi ---
    $g = New-RsFixture 'o_fp'
    $jtF = Join-Path $g.Dir 'jf'
    New-TestJunction $jtF $g.Dir
    $trFp = Get-TreeOracle $g.Saved
    $r = Invoke-TestRestore -Snap $g.Snap -Saved (Join-Path $jtF 'saved') -Root (Join-Path $g.Dir 'saved\bkroot') -Extra @{ All = $true; AllowRootAncestorReparse = $true; AcceptDifferentSource = $true }
    Check 'rs_final_path' 'backup_root_inside_saved_through_junction_alias_is_caught' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_inside_saved*') -and ((Get-TreeOracle $g.Saved) -ceq $trFp))
    Remove-TestJunction $jtF
    $longDir = Join-Path $g.Dir 'LongDirectoryNameForEightDotThree'
    [void][IO.Directory]::CreateDirectory($longDir)
    $shortDir = $longDir
    try { $shortDir = (New-Object -ComObject Scripting.FileSystemObject).GetFolder($longDir).ShortPath } catch { $shortDir = $longDir }
    $has83 = ($shortDir -ne $longDir) -and ($shortDir -like '*~*')
    Write-Host ('[MMO_BACKUP_INFO] eight_dot_three_available=' + $has83)
    if ($has83) {
        $svLong = Join-Path $longDir 'saved'
        Copy-Item -LiteralPath $g.Saved -Destination $svLong -Recurse
        $r = Invoke-TestRestore -Snap $g.Snap -Saved $svLong -Root (Join-Path $shortDir 'saved\bkroot') -Extra @{ All = $true; AcceptDifferentSource = $true }
        Check 'rs_final_path' 'eight_dot_three_alias_of_saved_is_caught' (($r.Code -eq 3) -and ($r.Out -like '*reason=backup_root_inside_saved*'))
    } else {
        Check 'rs_final_path' 'eight_dot_three_alias_of_saved_is_caught' $true
    }
    $r = Invoke-TestRestore -Snap $g.Snap -Saved (Join-Path $g.Dir 'saved') -Root $g.Root -Extra @{ All = $true }
    Check 'rs_final_path' 'ordinary_layout_not_falsely_refused' (($r.Code -eq 0) -or ($r.Code -eq 4))

    # --- D2: manifest label / tool ---
    $g = New-RsFixture 'o_lb'
    $mLb = Read-TestManifest $g.Snap
    Check 'rs_label' 'backup_manifest_has_label_and_tool' (($mLb.label -ceq 'img') -and ($mLb.tool -ceq 'BackupSaves/2'))
    Check 'rs_label' 'restore_dry_run_snapshot_line_shows_label_and_tool' ((Invoke-Rs $g).Out.Contains(' label=img tool=BackupSaves/2 '))
    $vr = Invoke-TestVerify @{ Snapshot = $g.Snap }
    Check 'rs_label' 'verify_prints_label_and_tool_meta_line' ($vr.Out.Contains('[MMO_VERIFY_META] snap=' + [IO.Path]::GetFileName($g.Snap) + ' label="img" tool=BackupSaves/2'))
    $cp = Join-Path $g.Dir 'lbcopy'
    Copy-Item -LiteralPath $g.Snap -Destination $cp -Recurse
    Edit-TestManifest $cp { param($m) $m.label = 'other' }
    $tl = Test-MMOSnapshot -SnapshotDir $cp
    Check 'rs_label' 'label_is_covered_by_header_digest' ((-not $tl.Ok) -and (HasErr $tl.Errors 'header_digest_mismatch'))
    Edit-TestManifest $cp { param($m) $m.label = 'BAD LABEL' } -FixDigest
    $tl = Read-MMOManifest -SnapshotDir $cp
    Check 'rs_label' 'malformed_label_rejected' ((-not $tl.Ok) -and (HasErr $tl.Errors 'bad_label'))
    $g = New-RsFixture 'o_lc'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    Set-Live $g.Saved 'SaveGames\MMOSave_Zed One.sav' (New-SavBytes 120 21)
    Set-Live $g.Saved 'SaveGames\MMOSave_Zed_One.sav' (New-SavBytes 121 22)
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ AcknowledgePartialState = $true }
    $preLc = Get-PreDir $g
    Check 'rs_label' 'real_preimage_manifest_has_prerestore_label' (($preLc -ne '') -and ((Read-TestManifest $preLc).label -ceq 'prerestore') -and ((Read-TestManifest $preLc).tool -ceq 'BackupSaves/2'))
    Edit-TestManifest $preLc { param($m) $m.label = 'manual' } -FixDigest
    $preLcSha = Get-OracleSha (Join-Path $preLc 'manifest.json')
    Set-Aged $g.Saved
    $r = Invoke-TestRestore -Snap $preLc -Saved $g.Saved -Root $g.Root -Extra @{ All = $true; Apply = $true; ExpectedManifestSha256 = $preLcSha; IncludeInvalid = $true; IncludeSideFiles = $true; AcknowledgePartialState = $true; AcceptCollisions = $true }
    Check 'rs_label' 'accept_collisions_refused_when_manifest_label_is_not_prerestore_even_if_dir_name_is' (($r.Code -eq 1) -and ($r.Out -like '*reason=accept_collisions_label_mismatch*'))
    $g = New-RbFixture 'o_lp'
    $trLp = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ TestHooks = @{ AfterPreimageTaken = { param($pre) Edit-TestManifest $pre { param($m) $m.label = 'manual' } -FixDigest } } }
    Check 'rs_label' 'preimage_with_wrong_label_refused_before_any_write' (($r.Code -eq 1) -and ($r.Out -like '*reason=preimage_label_mismatch*') -and ((Get-TreeOracle $g.Saved) -ceq $trLp))

    # --- D3: komut satirinda tek tirnak kacisi ---
    $qName = 'o_q''uo$te' + [string][char]0x2019 + 'x'
    $g = New-RbFixture $qName
    $r = Invoke-Rs $g
    $nl = @($r.Lines | Where-Object { $_ -like '*[[]MMO_RESTORE_NEXT]*' })
    $parsedNext = ''
    if ($nl.Count -ge 1) {
        $cmdText = $nl[0].Substring($nl[0].IndexOf('RestoreSaves.ps1'))
        $pe2 = $null
        $ast2 = [System.Management.Automation.Language.Parser]::ParseInput($cmdText.Replace('<yedek alirken KAYDETTIGIN ozet>', 'x'), [ref]$null, [ref]$pe2)
        $ce = $ast2.Find({ param($n) $n -is [System.Management.Automation.Language.CommandAst] }, $true)
        if ($null -ne $ce -and ($null -eq $pe2 -or $pe2.Count -eq 0)) { $parsedNext = $ce.CommandElements[2].Value }
    }
    Check 'rs_quote' 'dry_run_next_line_snapshot_path_round_trips_through_the_powershell_parser' (($g.Snap -like '*''*') -and ($parsedNext -ceq [IO.Path]::GetFullPath($g.Snap)))
    $global:rsProcFlip = $false
    $r = Invoke-Rs $g -Apply -Extra @{ ProcessProvider = $flipProv; TestHooks = @{ QuietEvery = 1; BeforeFile = { param($rel, $idx) if ($idx -eq 2) { $global:rsProcFlip = $true } } } }
    $global:rsProcFlip = $false
    $hl = @($r.Lines | Where-Object { $_ -like '*[[]MMO_RESTORE_ROLLBACK_FAILED]*' })
    $parsedHint = ''
    if ($hl.Count -ge 1) {
        $cmdText2 = $hl[0].Substring($hl[0].IndexOf('RestoreSaves.ps1'))
        $pe3 = $null
        $ast3 = [System.Management.Automation.Language.Parser]::ParseInput($cmdText2, [ref]$null, [ref]$pe3)
        $ce3 = $ast3.Find({ param($n) $n -is [System.Management.Automation.Language.CommandAst] }, $true)
        if ($null -ne $ce3 -and ($null -eq $pe3 -or $pe3.Count -eq 0)) { $parsedHint = $ce3.CommandElements[2].Value }
    }
    Check 'rs_quote' 'rollback_command_snapshot_path_round_trips_with_apostrophe_dollar_and_curly_quote' (($parsedHint -ceq [IO.Path]::GetFullPath((Get-PreDir $g))) -and ($parsedHint -ne ''))

    # --- D4: rollback komutu kapsami + uyarilar ---
    $g = New-RbFixture 'o_sl'
    $global:rsProcFlip = $false
    $r = Invoke-Rs $g -Apply -Extra @{ Slots = @('ali'); AcknowledgeCrossSlotRisk = $true; ProcessProvider = $flipProv; TestHooks = @{ QuietEvery = 1; BeforeFile = { param($rel, $idx) if ($idx -eq 2) { $global:rsProcFlip = $true } } } }
    $global:rsProcFlip = $false
    $hl4 = @($r.Lines | Where-Object { $_ -like '*[[]MMO_RESTORE_ROLLBACK_FAILED]*' })
    Check 'rs_hint' 'slots_run_rollback_command_is_slot_scoped_not_all' (($hl4.Count -ge 1) -and ($hl4[0] -like '* -Slots ali -AcknowledgeCrossSlotRisk -IncludeInvalid*') -and ($hl4[0] -notlike '* -All *'))
    Check 'rs_hint' 'failure_prints_not_routine_and_server_ran_warnings_and_recover_first' (($r.Out -like '*code=rollback_command_not_routine*') -and ($r.Out -like '*code=rollback_command_stale_if_server_ran*') -and ($r.Out -like '*code=recover_first*-Recover -Snapshot*'))

    # --- D7: karantina ---
    $g = New-RbFixture 'o_q1'
    Set-Live $g.Saved 'MMODaily\MMOSave_Ali.json.tmp' '{"version":1,"changed":'
    Set-Aged $g.Saved
    $trQ1 = Get-TreeOracle $g.Saved
    $r = Invoke-Rs $g -Apply -Extra @{ QuarantineStaleSideFiles = $true; TestHooks = @{ BeforeQuarantineMove = { param($from, $to) Write-TestText $to 'squatter' } } }
    Check 'rs_quarantine' 'move_failing_with_both_files_present_is_reported_and_nothing_lost' (($r.Code -eq 1) -and ($r.Out -like '*write_failed_rolled_back:quarantine_move_failed*') -and ($r.Out -like '*code=quarantine_both_present path=MMODaily/MMOSave_Ali.json.tmp*') -and ((Get-TreeOracle $g.Saved) -ceq $trQ1))
    $g = New-RbFixture 'o_q2'
    Set-Live $g.Saved 'MMODaily\MMOSave_Ali.json.tmp' '{"version":1,"changed":'
    Set-Aged $g.Saved
    $global:rsHook = @{ attempts = (New-Object System.Collections.ArrayList); victim = (Join-Path $g.Saved 'SaveGames\MMOSave_Ali Baba.sav.restore.tmp') }
    $r = Invoke-Rs $g -Apply -Extra @{ QuarantineStaleSideFiles = $true; TestHooks = @{ BeforeFile = $failAt2; BeforeRollback = { param($pre) [IO.File]::Delete((Join-Path $pre 'quarantine\MMODaily\MMOSave_Ali.json.tmp')) } } }
    [IO.Directory]::Delete($global:rsHook.victim)
    Check 'rs_quarantine' 'missing_quarantined_file_at_rollback_is_a_reported_failure_not_silent' (($r.Code -eq 1) -and ($r.Out -like '*status=rollback_failed*') -and ($r.Out -like '*rollback MMODaily/MMOSave_Ali.json.tmp:quarantine_missing*'))

    # --- D5: posta .bak / .tmp taramasi ---
    $g = New-RsFixture 'o_ml1'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    Set-Live $g.Saved 'MMOMail\MMOMail_ali_baba.json.bak' '{"version":2,"mails":[{"from":"Ali","text":"x","gold":5,"sent":1}]}'
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Extra $slotX
    Check 'rs_mail' 'bak_mailbox_sender_trace_is_reported' (($r.Code -eq 0) -and ($r.Out -like '*code=mail_trace mailbox=MMOMail/MMOMail_ali_baba.json.bak slot=ali*'))
    $r = Invoke-Rs $g -Apply -Extra $slotX
    Check 'rs_mail' 'bak_mailbox_trace_requires_acknowledge' (($r.Code -eq 1) -and ($r.Out -like '*reason=partial_state_requires_acknowledge*'))
    $g = New-RsFixture 'o_ml2'
    Set-Live $g.Saved $alis (New-SavBytes 300 77)
    Set-Live $g.Saved 'MMOMail\MMOMail_ali_baba.json.tmp' '{"version":2,"mai'
    Set-Aged $g.Saved
    $r = Invoke-Rs $g -Apply -Extra $slotX
    Check 'rs_mail' 'unparseable_tmp_mailbox_is_warning_only' (($r.Code -eq 0) -and ($r.Out -like '*code=mail_side_unverified mailbox=MMOMail/MMOMail_ali_baba.json.tmp*'))

    # ---- 13. "Saved degismedi" kanitinin kendi testi (sentetik Saved, gercek degil) ----------------------------------------
    $fakeSaved = Join-Path $fx 'fake_saved'
    Write-TestBytes (Join-Path $fakeSaved 'SaveGames\MMOSave_A.sav') (New-SavBytes 100 1)
    [void][IO.Directory]::CreateDirectory((Join-Path $fakeSaved 'MMOMail'))
    $st0 = Get-SavedStamp -SavedRoot $fakeSaved
    Check 'stampself' 'present_dirs_counted' ($st0.DirsPresent -eq 2)
    Check 'stampself' 'stable_when_untouched' ((Get-SavedStamp -SavedRoot $fakeSaved).Digest -ceq $st0.Digest)
    Write-TestBytes (Join-Path $fakeSaved 'SaveGames\MMOSave_B.sav') (New-SavBytes 100 2)
    $st1 = Get-SavedStamp -SavedRoot $fakeSaved
    Check 'stampself' 'new_file_detected' ($st1.Digest -cne $st0.Digest)
    [IO.File]::Delete((Join-Path $fakeSaved 'SaveGames\MMOSave_B.sav'))
    Check 'stampself' 'deleted_file_restores_digest' ((Get-SavedStamp -SavedRoot $fakeSaved).Digest -ceq $st0.Digest)
    [IO.File]::Delete((Join-Path $fakeSaved 'SaveGames\MMOSave_A.sav'))
    Check 'stampself' 'removed_file_detected' ((Get-SavedStamp -SavedRoot $fakeSaved).Digest -cne $st0.Digest)
    Write-TestBytes (Join-Path $fakeSaved 'SaveGames\MMOSave_A.sav') (New-SavBytes 100 1)
    # Her degisiklik, ondan HEMEN once alinan damgayla karsilastirilir (onceki adimlarin farki sonucu gizlemesin)
    $stBeforeSub = Get-SavedStamp -SavedRoot $fakeSaved
    [void][IO.Directory]::CreateDirectory((Join-Path $fakeSaved 'SaveGames\emptysub'))
    Check 'stampself' 'new_empty_subdir_detected' ((Get-SavedStamp -SavedRoot $fakeSaved).Digest -cne $stBeforeSub.Digest)
    [IO.Directory]::Delete((Join-Path $fakeSaved 'SaveGames\emptysub'))
    Check 'stampself' 'removed_subdir_restores_digest' ((Get-SavedStamp -SavedRoot $fakeSaved).Digest -ceq $stBeforeSub.Digest)
    [IO.File]::SetLastWriteTimeUtc((Join-Path $fakeSaved 'SaveGames\MMOSave_A.sav'), [datetime]'2001-01-01')
    $stBeforeTs = Get-SavedStamp -SavedRoot $fakeSaved
    [IO.File]::SetLastWriteTimeUtc((Join-Path $fakeSaved 'SaveGames\MMOSave_A.sav'), [datetime]'2002-02-02')
    Check 'stampself' 'timestamp_change_detected' ((Get-SavedStamp -SavedRoot $fakeSaved).Digest -cne $stBeforeTs.Digest)
    $stPre = Get-SavedStamp -SavedRoot $fakeSaved
    [void][IO.Directory]::CreateDirectory((Join-Path $fakeSaved 'MMODaily'))
    $stPost = Get-SavedStamp -SavedRoot $fakeSaved
    Check 'stampself' 'new_target_dir_detected' (($stPost.DirsPresent -eq 3) -and ($stPost.Digest -cne $stPre.Digest))
    $stNone = Get-SavedStamp -SavedRoot (Join-Path $fx 'no_such_saved')
    Check 'stampself' 'absent_targets_reported_vacuous' ($stNone.DirsPresent -eq 0)
    # Icerik ozetli surum (yalniz fikstur): ayni boyut + ayni zaman damgasiyla icerik degisikligini yakalar; metadata surumu yakalayamaz (bilinen sinir)
    $scFile = Join-Path $fakeSaved 'SaveGames\MMOSave_A.sav'
    $scTs = [IO.File]::GetLastWriteTimeUtc($scFile)
    $scMeta0 = Get-SavedStamp -SavedRoot $fakeSaved
    $scCont0 = Get-SavedStamp -SavedRoot $fakeSaved -WithContent
    $scBytes = [IO.File]::ReadAllBytes($scFile)
    $scBytes[10] = [byte](($scBytes[10] + 1) % 256)
    [IO.File]::WriteAllBytes($scFile, $scBytes)
    [IO.File]::SetLastWriteTimeUtc($scFile, $scTs)
    Check 'stampself' 'metadata_stamp_misses_same_size_same_time_content_change' ((Get-SavedStamp -SavedRoot $fakeSaved).Digest -ceq $scMeta0.Digest)
    Check 'stampself' 'content_stamp_catches_same_size_same_time_content_change' ((Get-SavedStamp -SavedRoot $fakeSaved -WithContent).Digest -cne $scCont0.Digest)
    $scBytes[10] = [byte](($scBytes[10] + 255) % 256)
    [IO.File]::WriteAllBytes($scFile, $scBytes)
    [IO.File]::SetLastWriteTimeUtc($scFile, $scTs)
    Check 'stampself' 'content_stamp_restores_digest_when_content_restored' ((Get-SavedStamp -SavedRoot $fakeSaved -WithContent).Digest -ceq $scCont0.Digest)

    # ---- 14. Gercek Saved degismedi kaniti ---------------------------------------------------------------------------
    $savedAfter = Get-SavedStamp
    Write-Host ('[MMO_BACKUP_INFO] real_saved dirs_present=' + $savedBefore.DirsPresent + ' entries=' + $savedBefore.Count + ' digest_before=' + $savedBefore.Digest.Substring(0, 16) + ' digest_after=' + $savedAfter.Digest.Substring(0, 16))
    if ($savedBefore.DirsPresent -eq 0) { Write-Host '[MMO_BACKUP_WARN] real Saved target dirs absent: the unchanged proof below is VACUOUS (pass -AllowVacuousSavedProof to accept)' }
    Check 'realsaved' 'proof_not_vacuous' (($savedBefore.DirsPresent -gt 0) -or $AllowVacuousSavedProof)
    Check 'realsaved' 'dirs_present_unchanged' ($savedBefore.DirsPresent -eq $savedAfter.DirsPresent)
    Check 'realsaved' 'entry_count_unchanged' ($savedBefore.Count -eq $savedAfter.Count)
    Check 'realsaved' 'name_size_timestamp_digest_unchanged' ($savedBefore.Digest -ceq $savedAfter.Digest)
    Check 'realsaved' 'fixture_isolated' (-not (Test-MMOPathUnder -Path $fx -Root (Join-Path $RepoRoot 'Saved') -AllowEqual))
}
catch {
    # Betik hatasi (istisna) sessizce cokmesin: ayrintisi yazilir ve benzersiz bir FAIL satiri uretilir (rapor satirlari yine basilir; mutasyon sinamasi cokmeyi `yakalandi` saymaz).
    Write-Host ('[MMO_BACKUP_INFO] fatal_exception ' + $_.Exception.Message)
    Check 'harness' 'fatal_exception' $false
}
finally {
    if ($null -eq $script:prevTestMode) { Remove-Item Env:\MMO_BACKUP_TESTMODE -ErrorAction SilentlyContinue } else { $env:MMO_BACKUP_TESTMODE = $script:prevTestMode }
    # Yalniz bu betigin olusturdugu fikstur dizinini siler: ad kalibi + TEMP altinda olmali. Once junction'lar (hedef dosyalara dokunmasin).
    foreach ($j in $script:junctions) { try { Remove-TestJunction $j } catch { } }
    # B1: reddedilmesi gereken repo-ici yedek koku yanlislikla olusturulduysa (regresyon) kaldirilir; yalniz bu betigin ad kalibi.
    $rejName = '_mmo_bk_reject_' + $guard
    $rejPath = Join-Path $RepoRoot $rejName
    if ([IO.Directory]::Exists($rejPath)) { try { Remove-Item -LiteralPath $rejPath -Recurse -Force } catch { } }
    $root = $script:fixtureRoot
    if ($null -ne $root -and (Test-MMOPathUnder -Path $root -Root $tempFull) -and ([IO.Path]::GetFileName($root) -like 'mmo_backup_test_*') -and [IO.Directory]::Exists($root)) {
        try { Remove-Item -LiteralPath $root -Recurse -Force } catch { Write-Host ('[MMO_BACKUP_INFO] cleanup_failed ' + $root) }
    }
}

# Sayim BAGIMSIZ: basilan CASE satirlari metin olarak yeniden ayristirilir (Check sayaclarina bakilmaz); sayaclar yalniz capraz kontrol.
$parsedLabels = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
$parsedLines = 0
$parsedFail = 0
foreach ($ln in $script:emitted) {
    if ($ln -cmatch '^\[MMO_BACKUP_CASE\] (?<t>PASS|FAIL) case=(?<c>\S+) check=(?<k>\S+)( duplicate_label)?\z') {
        $parsedLines++
        if ($Matches['t'] -ceq 'FAIL') { $parsedFail++ }
        [void]$parsedLabels.Add($Matches['c'] + '/' + $Matches['k'])
    } else { $parsedFail++ }
}
$unique = $parsedLabels.Count
$reported = $parsedLines
$consistent = ($reported -eq $expected) -and ($script:caseLines -eq $reported) -and ($unique -eq $reported) -and ($parsedFail -eq $script:failures)
if ($script:failures -eq 0) { Write-Host ('[MMO_BACKUP_TEST] PASS checks=' + $reported + ' reason=ok') }
else { Write-Host ('[MMO_BACKUP_TEST] FAIL checks=' + $reported + ' failures=' + $script:failures) }
Write-Host ('[MMO_BACKUP_RUNNER] reported=' + $reported + ' case_lines=' + $script:caseLines + ' unique_labels=' + $unique + ' expected=' + $expected + ' consistent=' + $consistent)
if ($script:failures -eq 0 -and $consistent) {
    Write-Host ('[MMO_BACKUP_RUNNER] PASS checks=' + $reported)
    exit 0
}
Write-Host ('[MMO_BACKUP_RUNNER] FAIL failures=' + $script:failures + ' consistent=' + $consistent)
exit 1
