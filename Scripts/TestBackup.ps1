# Kayit dosyalari yedekleme kutuphanesi testi (docs/DESIGN_BACKUP.md, dilim B0): Scripts/MMOSaveFilesLib.ps1 saf mantik + fikstur.
# Her sey gecici fikstur dizininde calisir (sentetik .sav/JSON); GERCEK Saved/ okunmaz ve yazilmaz. Tek istisna: "degismedi" kaniti
# icin gercek Saved\{SaveGames,MMOAchievements,MMODaily,MMOMail,MMORace} altinin YALNIZ dosya adi + boyut + zaman damgasi listesi
# (icerik okunmaz) testin basinda ve sonunda alinip karsilastirilir.
# Windows PowerShell 5.1 ve PowerShell 7. Dosya yalniz ASCII; ASCII disi karakterler [char] ile uretilir.
# Cikis: 0 PASS, 1 FAIL. Kalip: TestServerLifecycle.ps1 / TestFusion.ps1 (reported = case_lines = unique_labels = $expected).
param([string]$RepoRoot = '')
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
if ($RepoRoot -eq '') { $RepoRoot = Split-Path -Parent $PSScriptRoot }
. (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')

# Beklenen = benzersiz case/check etiketi sayisi (asagidaki bolumlerin toplami; degisirse burada guncelle).
$expected = 447

$script:labels = @{}
$script:caseLines = 0
$script:failures = 0
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
    Write-Host ('[MMO_BACKUP_CASE] ' + $tag + ' case=' + $Case + ' check=' + $Check + $note)
}

function CheckThrows {
    param([string]$Case, [string]$Check, [scriptblock]$Block, [string]$Prefix)
    $msg = $null
    try { & $Block | Out-Null } catch { $msg = $_.Exception.Message }
    Check $Case $Check (($null -ne $msg) -and $msg.StartsWith($Prefix))
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
    $saved = Join-Path $RepoRoot 'Saved'
    $lines = New-Object System.Collections.ArrayList
    foreach ($d in 'SaveGames', 'MMOAchievements', 'MMODaily', 'MMOMail', 'MMORace') {
        $p = Join-Path $saved $d
        if (Test-Path -LiteralPath $p -PathType Container) {
            foreach ($f in (Get-ChildItem -LiteralPath $p -Recurse -Force -File)) {
                [void]$lines.Add($f.FullName + '|' + $f.Length + '|' + $f.LastWriteTimeUtc.Ticks)
            }
        }
    }
    $arr = [string[]]@($lines.ToArray())
    [Array]::Sort($arr, [StringComparer]::Ordinal)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $h = $sha.ComputeHash((New-Object System.Text.UTF8Encoding($false)).GetBytes(($arr -join "`n"))) } finally { $sha.Dispose() }
    return [pscustomobject]@{ Count = $arr.Length; Digest = (($h | ForEach-Object { $_.ToString('x2') }) -join '') }
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
    foreach ($e in $list) { $text += ([string]$e.path + [char]0 + [string]$e.sha256 + [char]0 + [string]$e.size + "`n") }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return (($sha.ComputeHash((New-Object System.Text.UTF8Encoding($false)).GetBytes($text)) | ForEach-Object { $_.ToString('x2') }) -join '') } finally { $sha.Dispose() }
}

function New-BaseFiles {
    param([string]$FilesRoot)
    $j = '{"version":1,"name":"Ali","note":"' + $trWord + '"}'
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
    Write-TestText (Join-Path $FilesRoot 'MMORace\MMORace_ali.json') '{"version":1,"race":2}'
    Write-TestText (Join-Path $FilesRoot 'MMORace\MMORace_ali.json.bad') 'lock'
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
    if ($FixDigest) { $m.entries_digest = Get-MMOEntriesDigest $m.entries }
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
        @('lt', 'a<b', 'invalid_char'), @('empty', '', 'empty'), @('long', ('a' * 201), 'too_long')
    )
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
        @('race_json', 'MMORace/MMORace_ali.json', 'side', 'race', 'ali', '', ''),
        @('race_bad', 'MMORace/MMORace_ali.json.bad', 'side-lock', 'race', 'ali', '.bad', ''),
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
        'MMOMail/MMOMail_ali_baba.json.bak', 'MMORace/MMORace_ali_baba.json.bad', 'SaveGames/MMOSave_Veli.sav',
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
    CheckThrows 'sha' 'missing_file_throws' { Get-MMOFileSha256 (Join-Path $shaDir 'nope.bin') } ''

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
        @('gvat', ([byte[]](@(0x47, 0x56, 0x41, 0x54) + (New-TestBytes 96 7))), $false, 'bad_magic')
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

    # Sinif bilgisine gore butunluk karari
    $intDir = Join-Path $fx 'integrity'
    Write-TestText (Join-Path $intDir 'garbage.txt') 'not json at all'
    Write-TestText (Join-Path $intDir 'good.txt') '{"version":1}'
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
    CheckThrows 'resolve' 'root_missing' { Resolve-MMOSafePath -Root (Join-Path $fx 'no_such_root') -RelPath 'a/b' } 'root_missing'

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
    $lockEntry = @($mf.entries | Where-Object { $_.path -ceq 'MMORace/MMORace_ali.json.bad' })
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
        @('dup_path_case', { param($m) $m.entries[1].path = $m.entries[0].path.ToUpperInvariant() }, 'entry2:'),
        @('missing_field', { param($m) $m.entries[0].PSObject.Properties.Remove('sha256') }, 'entry1:missing_field:sha256'),
        @('version_2', { param($m) $m.format_version = 2 }, 'bad_format_version'),
        @('mode_unknown', { param($m) $m.mode = 'weird' }, 'bad_mode'),
        @('mode_case', { param($m) $m.mode = 'Live' }, 'bad_mode'),
        @('run_id_bad', { param($m) $m.shutdown_proof = [pscustomobject]@{ run_id = 'bad id!'; exit_code = 0 } }, 'bad_shutdown_proof'),
        @('run_id_missing', { param($m) $m.shutdown_proof = [pscustomobject]@{ exit_code = 0 } }, 'bad_shutdown_proof'),
        @('entries_object', { param($m) $m.entries = [pscustomobject]@{ a = 1 } }, 'entries_not_array'),
        @('field_removed', { param($m) $m.PSObject.Properties.Remove('entries_digest') }, 'missing_field:entries_digest'),
        @('source_dir_empty', { param($m) $m.source_saved_dir = '' }, 'bad_source_saved_dir')
    )
    foreach ($c in $contract) {
        $s = New-TestSnapshot ('c_' + $c[0])
        if (@('missing_field', 'entries_object', 'field_removed') -contains $c[0]) { Edit-TestManifest $s $c[1] }
        else { Edit-TestManifest $s $c[1] -FixDigest }
        $rdc = Read-MMOManifest -SnapshotDir $s
        Check 'contract' ($c[0] + '.rejected') (-not $rdc.Ok)
        Check 'contract' ($c[0] + '.reason') (HasErr $rdc.Errors $c[2])
        Check 'contract' ($c[0] + '.snapshot_rejected') (-not (Test-MMOSnapshot -SnapshotDir $s).Ok)
    }
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

    # ---- 12. Gercek Saved degismedi kaniti ---------------------------------------------------------------------------
    $savedAfter = Get-SavedStamp
    Write-Host ('[MMO_BACKUP_INFO] real_saved files=' + $savedBefore.Count + ' digest_before=' + $savedBefore.Digest.Substring(0, 16) + ' digest_after=' + $savedAfter.Digest.Substring(0, 16))
    Check 'realsaved' 'file_count_unchanged' ($savedBefore.Count -eq $savedAfter.Count)
    Check 'realsaved' 'name_size_timestamp_digest_unchanged' ($savedBefore.Digest -ceq $savedAfter.Digest)
    Check 'realsaved' 'fixture_isolated' (-not (Test-MMOPathUnder -Path $fx -Root (Join-Path $RepoRoot 'Saved') -AllowEqual))
}
finally {
    # Yalniz bu betigin olusturdugu fikstur dizinini siler: ad kalibi + TEMP altinda olmali. Once junction'lar (hedef dosyalara dokunmasin).
    foreach ($j in $script:junctions) { try { Remove-TestJunction $j } catch { } }
    $root = $script:fixtureRoot
    if ($null -ne $root -and (Test-MMOPathUnder -Path $root -Root $tempFull) -and ([IO.Path]::GetFileName($root) -like 'mmo_backup_test_*') -and [IO.Directory]::Exists($root)) {
        try { Remove-Item -LiteralPath $root -Recurse -Force } catch { Write-Host ('[MMO_BACKUP_INFO] cleanup_failed ' + $root) }
    }
}

$unique = $script:labels.Count
$reported = $script:caseLines
$consistent = ($reported -eq $expected) -and ($script:caseLines -eq $reported) -and ($unique -eq $reported)
if ($script:failures -eq 0) { Write-Host ('[MMO_BACKUP_TEST] PASS checks=' + $reported + ' reason=ok') }
else { Write-Host ('[MMO_BACKUP_TEST] FAIL checks=' + $reported + ' failures=' + $script:failures) }
Write-Host ('[MMO_BACKUP_RUNNER] reported=' + $reported + ' case_lines=' + $script:caseLines + ' unique_labels=' + $unique + ' expected=' + $expected + ' consistent=' + $consistent)
if ($script:failures -eq 0 -and $consistent) {
    Write-Host ('[MMO_BACKUP_RUNNER] PASS checks=' + $reported)
    exit 0
}
Write-Host ('[MMO_BACKUP_RUNNER] FAIL failures=' + $script:failures + ' consistent=' + $consistent)
exit 1
