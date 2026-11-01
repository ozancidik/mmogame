# Karakter .sav atomik yazim mantigi (FMMOSlotFileAtomic) icin mutasyon sinamasi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md 6.2, dilim S6.0/S6.2).
# Her mutasyon Private/Save/MMOSlotFileAtomic.cpp'yi BU CALISMA AGACINDA bozar, derler (Build.bat), Scripts/TestSlotFileAtomic.ps1'i kosar
# ve BELIRLI bir FAIL satirinin (case/check) ciktigini bekler. Derleme hatasi / runner satirinin olmamasi = CRASHED, yakalanmis SAYILMAZ.
# Kaynak her mutasyondan sonra (hata olsa da) geri yuklenir ve LastWriteTime dokunulur; sonda temiz kaynak bir kez daha derlenir.
# Yalniz izole (bagli) git worktree'de calisir: ana checkout reddedilir (cikis 3). UE / UBT / MSBuild calisiyorsa ertelenir (cikis 3).
# Kullanim (worktree kokunden): powershell.exe -NoProfile -ExecutionPolicy Bypass -File Scripts/TestSlotFileAtomicMutations.ps1 [-Only m1,m2] [-NoControl]
# Cikis: 0 = control PASS + her mutasyon beklenen FAIL satiriyla yakalandi; 1 = yakalanmayan / cokme / uygulanamayan / control kirik; 3 = ertelendi.
# Windows PowerShell 5.1. Dosya yalniz ASCII.
param([string[]]$Only = @(), [switch]$NoControl)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ -ne '' })
$root = Split-Path -Parent $PSScriptRoot
$target = Join-Path $root 'Source\MyProject\Private\Save\MMOSlotFileAtomic.cpp'
$project = Join-Path $root 'MyProject.uproject'
$build = 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat'
$runner = Join-Path $PSScriptRoot 'TestSlotFileAtomic.ps1'

# Ana checkout reddi: bagli worktree'de git-dir ile git-common-dir farkli olur.
$gd = (& git -C $root rev-parse --absolute-git-dir 2>$null)
$gc = (& git -C $root rev-parse --path-format=absolute --git-common-dir 2>$null)
if (-not $gd -or -not $gc -or ((Resolve-Path $gd).Path -ieq (Resolve-Path $gc).Path)) { Write-Host 'DEFER main_checkout_or_git_unresolved'; exit 3 }
$busy = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(UnrealEditor|UnrealBuildTool|MSBuild|UnrealTraceServer)' })
if ($busy.Count -gt 0) { Write-Host ('DEFER busy ' + (($busy | ForEach-Object { $_.ProcessName }) -join ',')); exit 3 }

function M { param([string]$Id, [object[]]$Pairs, [string[]]$Expect) return [pscustomobject]@{ Id = $Id; Pairs = $Pairs; Expect = $Expect } }
$mut = @(
    (M 'm1_skip_new_bytes_validation' @(, @('if (!Validate(Bytes)) return Fail(EMMOSlotSaveFailure::InvalidBytes);', 'if (false) return Fail(EMMOSlotSaveFailure::InvalidBytes);')) @('save_reject/invalid_bytes_rejected_by_validator'))
    (M 'm2_skip_readback_compare' @(, @('if (!Ops.ReadAll(Paths.Tmp, Back) || Back != Bytes)', 'if (!Ops.ReadAll(Paths.Tmp, Back))')) @('inject/readback_mismatch_fails_keeps_old_cleans_tmp'))
    (M 'm3_skip_bak_rotation' @(, @('else if (MainState == EMMOSlotFileState::Valid)', 'else if (false)')) @('main_only/fail_I3_failed_save_keeps_old', 'main_only/steps_pinned'))
    (M 'm4_corrupt_main_deleted_not_moved_to_bad' @(, @('if (!Ops.Move(BadForMain, Paths.Main))', 'if (!Ops.Delete(Paths.Main))')) @('main_corrupt_only/crash_I5_evidence_kept', 'bad_files/existing_bad_gets_next_slot_and_is_not_overwritten'))
    (M 'm5_corrupt_main_replaces_valid_bak' @(@('if (MainState == EMMOSlotFileState::Corrupt)', 'if (false)'), @('else if (MainState == EMMOSlotFileState::Valid)', 'else if (MainState != EMMOSlotFileState::Missing)')) @('main_corrupt_bak_valid/crash_I2_no_usable_loss', 'main_corrupt_bak_valid/fail_I3_failed_save_keeps_old', 'main_corrupt_bak_valid/fail_I1_I2_I5'))
    (M 'm6_load_uses_tmp_when_main_corrupt' @(, @('MainState == EMMOSlotFileState::Missing && BakState == EMMOSlotFileState::Missing)', 'true)')) @('load_order/tmp_not_used_when_main_exists_but_corrupt', 'load_order/tmp_not_used_when_bak_exists_but_corrupt'))
    (M 'm7_load_repairs_disk' @(, @('R.Source = EMMOSlotLoadSource::Bak;', 'R.Source = EMMOSlotLoadSource::Bak; Ops.Move(Paths.Main, Paths.Bak);')) @('load/no_side_effects_in_any_scenario'))
    (M 'm8_exists_checks_main_only' @(, @('return Ops.Exists(Paths.Main) || Ops.Exists(Paths.Bak);', 'return Ops.Exists(Paths.Main);')) @('exists/bak_alone_true'))
    (M 'm9_no_8mb_cap' @(, @('if (Bytes.Num() <= 0 || Bytes.Num() > MaxBytes)', 'if (Bytes.Num() <= 0)')) @('save_reject/over_8mb_cap_even_if_validator_accepts'))
    (M 'm10_slot_name_not_checked' @(, @('if (!bAllowed) return false;', 'if (false) return false;')) @('slot_name/traversal_rejected', 'slot_name/colon_and_dot_rejected'))
    (M 'm11_bak_deleted_before_promote' @(, @('if (!Ops.Move(Paths.Main, Paths.Tmp))', 'Ops.Delete(Paths.Bak); if (!Ops.Move(Paths.Main, Paths.Tmp))')) @('main_only/fail_I3_failed_save_keeps_old', 'main_only/fail_I1_I2_I5', 'bak_only/fail_I3_failed_save_keeps_old'))
    (M 'm12_undeletable_tmp_falls_back_to_direct_write' @(, @('if (Ops.Exists(Paths.Tmp) && !Ops.Delete(Paths.Tmp)) return Fail(EMMOSlotSaveFailure::TmpDeleteFailed);', 'if (Ops.Exists(Paths.Tmp) && !Ops.Delete(Paths.Tmp)) { Ops.WriteNew(Paths.Main, Bytes); R.bOk = true; return R; }')) @('inject/readonly_tmp_fails_without_direct_write'))
    (M 'm13_bak_not_put_back_on_promote_failure' @(, @('Ops.Move(Paths.Main, Paths.Bak);', ';')) @('inject/promote_failure_puts_old_back_as_main'))
    (M 'm14_bak_never_cleaned_after_success' @(, @('if (Ops.Exists(Paths.Bak) && !Ops.Delete(Paths.Bak)) R.bStaleBakLeft = true;', '')) @('main_only/clean_layout', 'main_only/steps_pinned'))
    (M 'm15_bad_cap_ignored' @(, @('if (Found < BadNeeded) return Fail(EMMOSlotSaveFailure::BadCapReached);', '')) @('save_reject/bad_cap_full_fails_closed_untouched'))
    (M 'm16_valid_tmp_deleted_not_kept' @(, @('&& Classify(Ops, Paths.Tmp, TmpValidate ? *TmpValidate : Validate, nullptr, bIo) == EMMOSlotFileState::Valid)', '&& false)')) @('tmp_valid_first/crash_I2_no_usable_loss', 'tmp_valid_first/steps_pinned'))
)
if ($Only.Count -gt 0) { $mut = @($mut | Where-Object { $Only -contains $_.Id.Split('_')[0] -or $Only -contains $_.Id }) }

$original = [IO.File]::ReadAllText($target)
$utf8 = New-Object System.Text.UTF8Encoding($false)
$bad = 0

function Invoke-BuildAndTest {
    param([string]$Id)
    $blog = Join-Path $env:TEMP ('mmo_slotmut_build_' + $Id + '.log')
    & $build MyProjectEditor Win64 Development "-Project=$project" -WaitMutex -FromMsBuild *> $blog
    $built = ($LASTEXITCODE -eq 0)
    $res = [pscustomobject]@{ Built = $built; Runner = $false; Pass = $false; Fails = @(); BuildLog = $blog }
    if (-not $built) { return $res }
    $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner
    $fails = New-Object System.Collections.ArrayList
    foreach ($ln in $out) {
        if ($ln -cmatch '\[MMO_SLOTATOMIC_CASE\] FAIL case=(?<c>\S+) check=(?<k>\S+)') { [void]$fails.Add($Matches['c'] + '/' + $Matches['k']) }
        if ($ln -cmatch '^\[MMO_SLOTATOMIC_RUNNER\] reported=') { $res.Runner = $true }
        if ($ln -cmatch '^\[MMO_SLOTATOMIC_RUNNER\] PASS checks=') { $res.Pass = $true }
    }
    $res.Fails = @($fails.ToArray() | Sort-Object -Unique)
    return $res
}

function Restore-Source {
    [IO.File]::WriteAllText($target, $original, $utf8)
    [IO.File]::SetLastWriteTimeUtc($target, [datetime]::UtcNow)
}

try {
    if (-not $NoControl) {
        $r = Invoke-BuildAndTest 'control'
        if ($r.Built -and $r.Runner -and $r.Pass -and $r.Fails.Count -eq 0) { Write-Host '[MMO_MUTATION] id=control verdict=CLEAN runner PASS' }
        else { Write-Host ('[MMO_MUTATION] id=control verdict=BROKEN built=' + $r.Built + ' runner=' + $r.Runner + ' pass=' + $r.Pass + ' fails=' + ($r.Fails -join ',')); $bad++ }
    }
    if ($bad -eq 0) {
        foreach ($m in $mut) {
            $txt = $original
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
            [IO.File]::WriteAllText($target, $txt, $utf8)
            [IO.File]::SetLastWriteTimeUtc($target, [datetime]::UtcNow)
            try { $r = Invoke-BuildAndTest $m.Id } finally { Restore-Source }
            if (-not $r.Built) { Write-Host ('[MMO_MUTATION] id=' + $m.Id + ' verdict=CRASHED build_failed log=' + $r.BuildLog + ' (does NOT count as caught)'); $bad++; continue }
            if (-not $r.Runner) { Write-Host ('[MMO_MUTATION] id=' + $m.Id + ' verdict=CRASHED no_runner_line (does NOT count as caught)'); $bad++; continue }
            $hit = @($m.Expect | Where-Object { $r.Fails -contains $_ })
            if ($hit.Count -gt 0) { Write-Host ('[MMO_MUTATION] id=' + $m.Id + ' verdict=CAUGHT by ' + ($hit -join ',')); Write-Host ('[MMO_MUTATION_FAILS] id=' + $m.Id + ' ' + ($r.Fails -join ' ')) }
            else { Write-Host ('[MMO_MUTATION] id=' + $m.Id + ' verdict=SURVIVED expected one of: ' + ($m.Expect -join ',') + ' ; fails seen: ' + ($r.Fails -join ',')); $bad++ }
        }
    }
} finally {
    Restore-Source
    # Calisma agacindaki ikili dosyalari temiz kaynakla esle.
    & $build MyProjectEditor Win64 Development "-Project=$project" -WaitMutex -FromMsBuild *> (Join-Path $env:TEMP 'mmo_slotmut_build_final.log')
    Write-Host ('[MMO_MUTATION_INFO] source restored, final clean build exit=' + $LASTEXITCODE)
}
Write-Host ('[MMO_MUTATION_SUMMARY] mutations=' + $mut.Count + ' not_caught=' + $bad)
if ($bad -eq 0) { exit 0 }
exit 1
