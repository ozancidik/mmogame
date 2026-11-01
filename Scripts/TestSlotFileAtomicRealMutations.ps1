# Karakter .sav atomik yazim GERCEK uygulama + magaza baglantisi icin mutasyon sinamasi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md 6.2, dilim S6.1).
# Her mutasyon ilgili kaynak dosyayi BU CALISMA AGACINDA bozar, derler (Build.bat), Scripts/TestSlotFileAtomicReal.ps1'i kosar
# ve BELIRLI bir FAIL satirinin (case/check) ciktigini bekler. Derleme hatasi / runner satirinin olmamasi / test cokmesi = CRASHED, yakalanmis SAYILMAZ.
# Kaynaklar her mutasyondan sonra (hata olsa da) geri yuklenir ve LastWriteTime dokunulur; sonda temiz kaynak bir kez daha derlenir.
# Yalniz izole (bagli) git worktree'de calisir: ana checkout reddedilir (cikis 3). UE / UBT / MSBuild calisiyorsa ertelenir (cikis 3).
# Kullanim (worktree kokunden): powershell.exe -NoProfile -ExecutionPolicy Bypass -File Scripts/TestSlotFileAtomicRealMutations.ps1 [-Only o1,s2] [-NoControl]
# Cikis: 0 = control PASS + her mutasyon beklenen FAIL satiriyla yakalandi; 1 = yakalanmayan / cokme / uygulanamayan / control kirik; 3 = ertelendi.
# Windows PowerShell 5.1. Dosya yalniz ASCII.
param([string[]]$Only = @(), [switch]$NoControl)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ -ne '' })
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'MyProject.uproject'
$build = 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat'
$runner = Join-Path $PSScriptRoot 'TestSlotFileAtomicReal.ps1'
$fStore = 'Source\MyProject\Private\Save\MMOLocalFileCharacterStore.cpp'
$fOps = 'Source\MyProject\Private\Save\MMOSlotFileOpsFileManager.cpp'
$fPure = 'Source\MyProject\Private\Save\MMOSlotFileAtomic.cpp'

$gd = (& git -C $root rev-parse --absolute-git-dir 2>$null)
$gc = (& git -C $root rev-parse --path-format=absolute --git-common-dir 2>$null)
if (-not $gd -or -not $gc -or ((Resolve-Path $gd).Path -ieq (Resolve-Path $gc).Path)) { Write-Host 'DEFER main_checkout_or_git_unresolved'; exit 3 }
$busy = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(UnrealEditor|UnrealBuildTool|MSBuild|UnrealTraceServer)' })
if ($busy.Count -gt 0) { Write-Host ('DEFER busy ' + (($busy | ForEach-Object { $_.ProcessName }) -join ',')); exit 3 }

function M { param([string]$Id, [string]$File, [object[]]$Pairs, [string[]]$Expect) return [pscustomobject]@{ Id = $Id; File = $File; Pairs = $Pairs; Expect = $Expect } }
$mut = @(
    (M 'o1_delete_even_readonly' $fOps @(, @('return IFileManager::Get().Delete(*Path, false, false, true);', 'return IFileManager::Get().Delete(*Path, false, true, true);')) @('ops/readonly_delete_fails_file_kept', 'inject/readonly_tmp_save_false_no_direct_write'))
    (M 'o2_read_without_size_cap' $fOps @(, @('if (Size < 0 || Size > FMMOSlotFileAtomic::MaxBytes)', 'if (Size < 0)')) @('ops/read_over_cap_refused'))
    (M 'o3_move_without_replace' $fOps @(, @('return IFileManager::Get().Move(*Dest, *Src, true, false, false, true);', 'return IFileManager::Get().Move(*Dest, *Src, false, false, false, true);')) @('ops/move_replace_existing_dest_replaces_content', 'store/second_save_replaces_and_leaves_single_file'))
    (M 'o4_write_appends_instead_of_truncating' $fOps @(, @('PF.OpenWrite(*Path, /*bAppend*/ false, /*bAllowRead*/ false)', 'PF.OpenWrite(*Path, /*bAppend*/ true, /*bAllowRead*/ false)')) @('ops/write_new_overwrites_existing'))
    (M 's1_structure_walk_accepts_missing_tail' $fStore @(, @('return Ar.Tell() + 4 <= Ar.TotalSize();', 'return true;')) @('codec/truncated_prefixes_never_reach_engine_parser_structure_walk'))
    (M 's2_canonical_check_dropped' $fStore @(, @('return ParseBytes(Bytes, Parsed) && SerializeBytes(Parsed, Again) && Again == Bytes;', 'return ParseBytes(Bytes, Parsed);')) @('codec/one_byte_short_and_one_byte_extra_not_canonical', 'tmp/noncanonical_tmp_only_rejected_but_same_bytes_as_main_accepted'))
    (M 's3_load_uses_loose_validator_for_tmp' $fStore @(, @('FMMOSlotFileAtomic::Load(Ops, Paths, MainValid, &TmpValidator)', 'FMMOSlotFileAtomic::Load(Ops, Paths, MainValid, nullptr)')) @('tmp/noncanonical_tmp_only_rejected_but_same_bytes_as_main_accepted'))
    (M 's4_bad_cap_metric_renamed' $fStore @(, @('ev=save_bad_cap player=%s', 'ev=save_bad_capx player=%s')) @('bad_cap/quota_full_metric_names_player'))
    (M 's5_exists_checks_main_only' $fStore @(, @('return FMMOSlotFileAtomic::Exists(Ops, FMMOSlotAtomicPaths::From(SlotMainPath(SlotName)));', 'return Ops.Exists(SlotMainPath(SlotName));')) @('exists/bak_alone_exists_true_and_loads'))
    (M 's6_class_name_check_dropped' $fStore @(, @('if (ClassName != UMMOSaveGame::StaticClass()->GetPathName()) return false;', '')) @('codec/foreign_class_name_rejected_before_loadobject'))
    (M 's7_magic_check_dropped' $fStore @(, @('if (Tag != MMOLocalFileStoreSaveTag) return false;', '')) @('codec/all_zero_and_bad_magic_files_rejected_without_crash'))
    (M 's8_main_path_loses_sav_extension' $fStore @(, @('return FPaths::Combine(Directory, SlotName + TEXT(".sav"));', 'return FPaths::Combine(Directory, SlotName + TEXT(".savx"));')) @('store/save_leaves_only_main_file', 'legacy/direct_written_sav_loads_via_new_store'))
    (M 's9_recovered_log_renamed' $fStore @(, @('event=recovered slot=%s source=%s', 'event=recoveredx slot=%s source=%s')) @('recover/recovered_log_and_metric_emitted'))
    (M 's10_load_failed_log_renamed' $fStore @(, @('event=load_failed slot=%s reason=corrupt', 'event=load_failedx slot=%s reason=corrupt')) @('recover/load_failed_corrupt_log_emitted'))
    (M 's11_bvalid_false_record_rejected' $fStore @(, @('return SerializeBytes(Parsed, Again) && Again == Bytes;', 'return false;')) @('codec/bvalid_false_full_record_is_valid_not_corrupt'))
    (M 'p1_pure_load_ignores_tmp_validator' $fPure @(, @('Classify(Ops, Paths.Tmp, TmpValidate ? *TmpValidate : Validate, &R.Bytes, bIo)', 'Classify(Ops, Paths.Tmp, Validate, &R.Bytes, bIo)')) @('tmp/noncanonical_tmp_only_rejected_but_same_bytes_as_main_accepted'))
)
if ($Only.Count -gt 0) { $mut = @($mut | Where-Object { $Only -contains $_.Id.Split('_')[0] -or $Only -contains $_.Id }) }

$utf8 = New-Object System.Text.UTF8Encoding($false)
$originals = @{}
foreach ($f in @($fStore, $fOps, $fPure)) { $originals[$f] = [IO.File]::ReadAllText((Join-Path $root $f)) }
$bad = 0

function Invoke-BuildAndTest {
    param([string]$Id)
    $blog = Join-Path $env:TEMP ('mmo_slotrealmut_build_' + $Id + '.log')
    & $build MyProjectEditor Win64 Development "-Project=$project" -WaitMutex -FromMsBuild *> $blog
    $built = ($LASTEXITCODE -eq 0)
    $res = [pscustomobject]@{ Built = $built; Runner = $false; Pass = $false; Fails = @(); BuildLog = $blog }
    if (-not $built) { return $res }
    $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner
    $fails = New-Object System.Collections.ArrayList
    foreach ($ln in $out) {
        if ($ln -cmatch '\[MMO_SLOTREAL_CASE\] FAIL case=(?<c>\S+) check=(?<k>\S+)') { [void]$fails.Add($Matches['c'] + '/' + $Matches['k']) }
        if ($ln -cmatch '^\[MMO_SLOTREAL_RUNNER\] reported=' -and $ln -cmatch 'case_lines=([1-9]\d*)') { $res.Runner = $true }
        if ($ln -cmatch '^\[MMO_SLOTREAL_RUNNER\] PASS checks=') { $res.Pass = $true }
    }
    $res.Fails = @($fails.ToArray() | Sort-Object -Unique)
    return $res
}

function Restore-Sources {
    foreach ($f in $originals.Keys) {
        $p = Join-Path $root $f
        [IO.File]::WriteAllText($p, $originals[$f], $utf8)
        [IO.File]::SetLastWriteTimeUtc($p, [datetime]::UtcNow)
    }
}

try {
    if (-not $NoControl) {
        $r = Invoke-BuildAndTest 'control'
        if ($r.Built -and $r.Runner -and $r.Pass -and $r.Fails.Count -eq 0) { Write-Host '[MMO_MUTATION] id=control verdict=CLEAN runner PASS' }
        else { Write-Host ('[MMO_MUTATION] id=control verdict=BROKEN built=' + $r.Built + ' runner=' + $r.Runner + ' pass=' + $r.Pass + ' fails=' + ($r.Fails -join ',')); $bad++ }
    }
    if ($bad -eq 0) {
        foreach ($m in $mut) {
            $orig = $originals[$m.File]
            $txt = $orig
            $applied = $true
            $note = ''
            foreach ($pr in $m.Pairs) {
                $find = [string]$pr[0]
                $cnt = 0
                $ix = 0
                while (($ix = $txt.IndexOf($find, $ix, [StringComparison]::Ordinal)) -ge 0) { $cnt++; $ix += $find.Length }
                if ($cnt -ne 1) { $applied = $false; $note = 'find_count=' + $cnt + ' for: ' + $find; break }
                $txt = $txt.Replace($find, [string]$pr[1])
            }
            if (-not $applied) { Write-Host ('[MMO_MUTATION] id=' + $m.Id + ' verdict=NOT_APPLIED ' + $note); $bad++; continue }
            $target = Join-Path $root $m.File
            [IO.File]::WriteAllText($target, $txt, $utf8)
            [IO.File]::SetLastWriteTimeUtc($target, [datetime]::UtcNow)
            try { $r = Invoke-BuildAndTest $m.Id } finally { Restore-Sources }
            if (-not $r.Built) { Write-Host ('[MMO_MUTATION] id=' + $m.Id + ' verdict=CRASHED build_failed log=' + $r.BuildLog + ' (does NOT count as caught)'); $bad++; continue }
            if (-not $r.Runner) { Write-Host ('[MMO_MUTATION] id=' + $m.Id + ' verdict=CRASHED no_runner_line_or_no_case_lines (does NOT count as caught)'); $bad++; continue }
            $hit = @($m.Expect | Where-Object { $r.Fails -contains $_ })
            if ($hit.Count -gt 0) { Write-Host ('[MMO_MUTATION] id=' + $m.Id + ' verdict=CAUGHT by ' + ($hit -join ',')); Write-Host ('[MMO_MUTATION_FAILS] id=' + $m.Id + ' ' + ($r.Fails -join ' ')) }
            else { Write-Host ('[MMO_MUTATION] id=' + $m.Id + ' verdict=SURVIVED expected one of: ' + ($m.Expect -join ',') + ' ; fails seen: ' + ($r.Fails -join ',')); $bad++ }
        }
    }
} finally {
    Restore-Sources
    & $build MyProjectEditor Win64 Development "-Project=$project" -WaitMutex -FromMsBuild *> (Join-Path $env:TEMP 'mmo_slotrealmut_build_final.log')
    Write-Host ('[MMO_MUTATION_INFO] sources restored, final clean build exit=' + $LASTEXITCODE)
}
Write-Host ('[MMO_MUTATION_SUMMARY] mutations=' + $mut.Count + ' not_caught=' + $bad)
if ($bad -eq 0) { exit 0 }
exit 1
