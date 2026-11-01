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
    (M 'o1_delete_even_readonly' $fOps @(, @('IFileManager::Get().Delete(*Path, false, bEvenReadOnly, true)', 'IFileManager::Get().Delete(*Path, false, true, true)')) @('ops/readonly_delete_fails_file_kept', 'ops2/delete_even_readonly_removes_readonly_file'))
    (M 'o2_read_without_size_cap' $fOps @(, @('if (Size < 0 || Size > FMMOSlotFileAtomic::MaxBytes)', 'if (Size < 0)')) @('ops/read_over_cap_refused'))
    (M 'o3_move_without_replace' $fOps @(, @('IFileManager::Get().Move(*Dest, *Src, true, false, false, true)', 'IFileManager::Get().Move(*Dest, *Src, false, false, false, true)')) @('ops/move_replace_existing_dest_replaces_content', 'store/second_save_replaces_and_leaves_single_file'))
    (M 'o4_write_appends_instead_of_truncating' $fOps @(, @('PF.OpenWrite(*Path, /*bAppend*/ false, /*bAllowRead*/ false)', 'PF.OpenWrite(*Path, /*bAppend*/ true, /*bAllowRead*/ false)')) @('ops/write_new_overwrites_existing'))
    (M 's1_structure_walk_accepts_missing_tail' $fStore @(, @('return Ar.Tell() + 4 <= Ar.TotalSize();', 'return true;')) @('codec/truncated_prefixes_never_reach_engine_parser_structure_walk'))
    (M 's2_canonical_check_dropped' $fStore @(, @('return ParseBytes(Bytes, Parsed) && SerializeBytes(Parsed, Again) && Again == Bytes;', 'return ParseBytes(Bytes, Parsed);')) @('codec/one_byte_short_and_one_byte_extra_not_canonical', 'tmp/noncanonical_tmp_only_rejected_but_same_bytes_as_main_accepted'))
    (M 's3_load_uses_loose_validator_for_tmp' $fStore @(, @('FMMOSlotFileAtomic::Load(Ops, Paths, MainValid, &TmpValidator)', 'FMMOSlotFileAtomic::Load(Ops, Paths, MainValid, nullptr)')) @('tmp/noncanonical_tmp_only_rejected_but_same_bytes_as_main_accepted'))
    (M 's4_bad_cap_metric_renamed' $fStore @(, @('ev=save_bad_cap player=%s', 'ev=save_bad_capx player=%s')) @('bad_cap/quota_full_metric_names_player'))
    (M 's5_exists_checks_main_only' $fStore @(, @('return FMMOSlotFileAtomic::Exists(Ops, FMMOSlotAtomicPaths::From(SlotMainPath(SlotName)), &TmpValidator);', 'return Ops.Exists(SlotMainPath(SlotName));')) @('exists/bak_alone_exists_true_and_loads'))
    (M 's6_class_name_check_dropped' $fStore @(, @('if (ClassName != UMMOSaveGame::StaticClass()->GetPathName()) return false;', '')) @('codec/foreign_class_name_rejected_before_loadobject'))
    (M 's7_magic_check_dropped' $fStore @(, @('if (Tag != MMOLocalFileStoreSaveTag) return false;', '')) @('codec/all_zero_and_bad_magic_files_rejected_without_crash'))
    (M 's8_main_path_loses_sav_extension' $fStore @(, @('return FPaths::Combine(Directory, SlotName + TEXT(".sav"));', 'return FPaths::Combine(Directory, SlotName + TEXT(".savx"));')) @('store/save_leaves_only_main_file', 'legacy/direct_written_sav_loads_via_new_store'))
    (M 's9_recovered_log_renamed' $fStore @(, @('event=recovered slot=%s source=%s', 'event=recoveredx slot=%s source=%s')) @('recover/recovered_log_and_metric_emitted'))
    (M 's10_load_failed_log_renamed' $fStore @(, @('event=load_failed slot=%s reason=%s', 'event=load_failedx slot=%s reason=%s')) @('recover/load_failed_corrupt_log_emitted', 'io/unreadable_main_load_false_no_bak_fallback_metric_io'))
    (M 's11_bvalid_false_record_rejected' $fStore @(, @('return SerializeBytes(Parsed, Again) && Again == Bytes;', 'return false;')) @('codec/bvalid_false_full_record_is_valid_not_corrupt'))
    (M 'o5_move_without_source_guard' $fOps @(, @('if (!IFileManager::Get().FileExists(*Src)) { return false; }', 'if (false) { return false; }')) @('ops2/move_with_missing_source_keeps_existing_dest'))
    (M 'o6_delete_not_retried' $fOps @(, @('if ((!bInjected && !IFileManager::Get().FileExists(*Path)) || Attempt + 1 >= RetryAttempts || !SleepForRetry())', 'if (true)')) @('ops3/delete_transient_failure_is_retried_until_success'))
    (M 'o7_move_not_retried' $fOps @(, @('if (!IFileManager::Get().FileExists(*Src) || Attempt + 1 >= RetryAttempts || !SleepForRetry())', 'if (true)')) @('ops3/move_transient_failure_is_retried_then_renamed'))
    (M 'o8_read_not_retried' $fOps @(, @('if (!IFileManager::Get().FileExists(*Path) || Attempt + 1 >= RetryAttempts || !SleepForRetry())', 'if (true)')) @('ops3/read_transient_failure_is_retried_content_intact'))
    (M 'o9_retry_budget_unbounded' $fOps @(, @('if (SleptSeconds + RetrySleepSeconds > RetryBudgetSeconds + KINDA_SMALL_NUMBER)', 'if (false)')) @('ops3/total_retry_wait_stays_under_200ms_across_many_failing_ops'))
    (M 'o10_write_flush_not_full' $fOps @(, @('bOk = bOk && Handle->Flush(true);', 'bOk = bOk && Handle->Flush(false);')) @('ops3/write_new_requests_full_flush'))
    (M 'o11_write_flush_result_ignored' $fOps @(, @('bOk = bOk && Handle->Flush(true);', 'bOk = bOk && (Handle->Flush(true) || true);')) @('ops3/write_new_fails_when_flush_fails'))
    (M 'o12_write_tell_not_checked' $fOps @(, @('bOk = bOk && (Handle->Tell() == Bytes.Num());', 'bOk = bOk && true;')) @('ops3/write_new_fails_when_tell_does_not_match_length'))
    (M 'o13_clear_readonly_is_noop' $fOps @(, @('FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Path, false);', ';')) @('ops2/clear_readonly_makes_file_deletable_without_flag'))
    (M 's12_round_trip_ignores_bvalid' $fStore @(, @('return ParseBytes(Bytes, Check) && Check.bValid == In.bValid;', 'return ParseBytes(Bytes, Check);')) @('codec/verify_round_trip_compares_bvalid_and_rejects_garbage'))
    (M 's13_selftest_counter_removed' $fStore @(, @('++GMMOLocalFileStoreFormatSelfTestRuns;', '')) @('selftest/format_round_trip_passes_and_ran_exactly_once_in_process'))
    (M 's14_selftest_error_downgraded' $fStore @(, @('UE_LOG(LogMMOSaveFile, Error, TEXT("[MMOSAVE] event=format_selftest_failed', 'UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOSAVE] event=format_selftest_failed')) @('selftest/failure_report_is_error_level_with_event_and_metric'))
    (M 's15_save_failed_metric_renamed' $fStore @(, @('ev=save_failed reason=%s player=%s', 'ev=save_failedx reason=%s player=%s')) @('inject/save_failure_emits_metric_with_reason_and_player'))
    (M 's16_stale_bak_metric_renamed' $fStore @(, @('ev=save_stale_bak player=%s', 'ev=save_stale_bakx player=%s')) @('stale/undeletable_bak_save_ok_logged_with_separate_metric'))
    (M 's17_list_skips_bak_only_slots' $fStore @(, @('IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("MMOSave_*.sav.bak")), true, false);', ';')) @('list/main_bak_only_and_valid_tmp_only_slots_listed'))
    (M 's18_list_accepts_torn_tmp' $fStore @(, @('if (Ops.ReadAll(FPaths::Combine(Directory, File), Bytes) && IsCanonical(Bytes))', 'if (Ops.ReadAll(FPaths::Combine(Directory, File), Bytes))')) @('list/torn_tmp_bad_and_wildcard_quirk_files_not_listed_and_no_duplicates'))
    (M 's19_list_duplicates_not_removed' $fStore @(, @('if (!bAlready)', 'if (true)')) @('list/torn_tmp_bad_and_wildcard_quirk_files_not_listed_and_no_duplicates'))
    (M 's20_save_failed_metric_missing_player_name_guard' $fStore @(, @('bNameOk ? *MMOLocalFileStorePlayerOf(SlotName) : TEXT("-")', '*MMOLocalFileStorePlayerOf(SlotName)')) @('slot_name/invalid_name_save_failure_metric_has_no_player_name'))
    (M 'p1_pure_load_ignores_tmp_validator' $fPure @(, @('Classify(Ops, Paths.Tmp, TmpValidate ? *TmpValidate : Validate, &R.Bytes, bTmpIo)', 'Classify(Ops, Paths.Tmp, Validate, &R.Bytes, bTmpIo)')) @('tmp/noncanonical_tmp_only_rejected_but_same_bytes_as_main_accepted'))
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
