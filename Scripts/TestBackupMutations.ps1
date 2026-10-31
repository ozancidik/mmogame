# B3.2 mutasyon sinamasi: her yeni kapi icin, ISLEYEN KOPYADA kaynak betigi bozulur ve Scripts/TestBackup.ps1 o kopyadan calistirilir.
# Her mutasyon testi BELIRLI bir FAIL satiriyla (case/check etiketi) kirmali; yalniz "cokme" (runner satiri yok) SAYILMAZ.
# Gercek depo dosyalarina DOKUNULMAZ: kopya dizini %TEMP% altinda mmo_mut_<guid>\<id>\Scripts; her mutasyon kendi kopyasinda, paralel (Throttle).
# Kullanim: powershell.exe -NoProfile -ExecutionPolicy Bypass -File Scripts/TestBackupMutations.ps1 [-Only id1,id2] [-Throttle 4] [-KeepWork] [-NoControl]
# Cikis: 0 = control temiz + her mutasyon beklenen FAIL satiriyla yakalandi; 1 = yakalanmayan / cokme / uygulanamayan mutasyon / control kirik.
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII.
param([string[]]$Only = @(), [int]$Throttle = 4, [switch]$KeepWork, [switch]$NoControl)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ -ne '' })   # powershell.exe -File ile virgullu liste tek dize gelir
$here = $PSScriptRoot
$files = 'MMOSaveFilesLib.ps1', 'BackupSaves.ps1', 'RestoreSaves.ps1', 'VerifySaves.ps1', 'TestBackup.ps1'

function M {
    # Id, Dosya, @(Bul, Degistir) ciftleri (her Bul KOPYADA tam olarak bir kez bulunmali; -All ile tum gecisler degistirilir), beklenen FAIL etiketleri
    param([string]$Id, [string]$File, [object[]]$Pairs, [string[]]$Expect, [switch]$All)
    return [pscustomobject]@{ Id = $Id; File = $File; Pairs = $Pairs; Expect = $Expect; All = [bool]$All }
}

$mut = @(
    (M 'o1_leftover_gate' 'RestoreSaves.ps1' @(, @("Stop-Restore 3 'leftover_restore_artifacts_require_recover'", '$null')) @('rs_recover/normal_apply_with_ack_still_refused_by_leftovers_before_preimage', 'rs_rollback/leftover_restore_tmp_blocks_write_and_is_not_deleted'))
    (M 'o1_old_hash_verify' 'RestoreSaves.ps1' @(, @('([string]$pe.sha256 -cne $it.Sha)', '$false')) @('rs_recover/old_with_unverifiable_hash_is_unresolved'))
    (M 'o1_tmp_always_discard' 'RestoreSaves.ps1' @(, @("if (`$planned -ne '' -and `$planned -ceq `$it.Sha)", 'if ($true)')) @('rs_recover/tmp_with_other_content_is_quarantine_not_discard'))
    (M 'o1_marker_always_consistent' 'RestoreSaves.ps1' @(, @('$consistent = ($null -ne $stAfter) -and ($planMap.Count -eq $expectedPlan) -and ($stAfter.Other -eq 0) -and (($stAfter.New -eq 0) -or ($stAfter.Pre -eq 0))', '$consistent = $true')) @('rs_recover/mixed_state_keeps_marker_exit_4_and_prints_rollback_command'))
    (M 'o1_no_old_rename' 'RestoreSaves.ps1' @(, @('[IO.File]::Move($it.Full, $it.Dest)', '$null')) @('rs_recover/apply_restores_old_discards_tmp_removes_marker_exit_0'))
    (M 'o1_unresolved_not_failing' 'RestoreSaves.ps1' @(, @("Stop-Restore 1 'recover_incomplete'", '$null')) @('rs_recover/unresolved_apply_exit_1_marker_kept_old_untouched'))
    (M 'o1_marker_mismatch_ignored' 'RestoreSaves.ps1' @(, @("Stop-Restore 1 'recover_marker_mismatch'", '$null')) @('rs_recover/marker_naming_another_preimage_refused'))
    (M 'o2_stale_marker_deleted' 'RestoreSaves.ps1' @(, @("try { [IO.File]::Move(`$mp, `$dst) } catch", 'try { [IO.File]::Delete($mp) } catch')) @('rs_chain/acknowledged_stale_marker_is_moved_not_deleted_both_copies'))
    (M 'o2_chain_not_carried' 'RestoreSaves.ps1' @(, @('[void]$chain.Insert(0, $entry)', '$null = 0')) @('rs_chain/new_marker_carries_old_preimage_as_chain'))
    (M 'o2_rollback_closes_open_chain' 'RestoreSaves.ps1' @(, @('        if ($script:ChainOpen -gt 0) {', '        if ($false) {')) @('rs_chain/full_rollback_with_open_chain_keeps_marker_and_warns'))
    (M 'o2_slots_close_open_chain' 'RestoreSaves.ps1' @(, @('if ($script:ChainOpen -gt 0 -and ($useSlots -or $notRestored -gt 0)) {', 'if ($false) {')) @('rs_chain/slot_restore_with_open_chain_keeps_marker_exit_4'))
    (M 'o3_no_apply_lock' 'RestoreSaves.ps1' @(, @("if (-not (Open-RsLock)) { Stop-Restore 3 'lock_held' }`n    Assert-RsQuiet`n    `$haveLive", "`$null = 0`n    Assert-RsQuiet`n    `$haveLive")) @('rs_lock/second_apply_while_lock_held_defers_3_and_touches_nothing'))
    (M 'o3_lock_not_handed_to_child' 'RestoreSaves.ps1' @(, @("    Close-RsLock`n    `$global:LASTEXITCODE = -999", "    `$global:LASTEXITCODE = -999")) @('rs_lock/backup_during_restore_apply_defers_on_lock', 'rs_apply/exit_4_partial_state_acknowledged'))
    (M 'o3_marker_present_gate_removed' 'RestoreSaves.ps1' @(, @("if (-not `$AcknowledgeStaleRestoreMarker) { Stop-Restore 3 'restore_marker_exists' }", '$null = 0')) @('rs_marker/marker_created_by_a_concurrent_run_before_write_refuses_exit_3_nothing_written'))
    (M 'o3_backup_ignores_marker' 'BackupSaves.ps1' @(, @("if (`$Label -cne 'prerestore') { Stop-Backup 3 'restore_in_progress' }", '$null = 0')) @('rs_marker/backup_defers_3_when_primary_marker_present', 'rs_marker/backup_defers_3_when_only_saved_mirror_present'))
    (M 'o3_backup_marker_note_removed' 'BackupSaves.ps1' @(, @('if ($script:RestoreMarkerSeen) { [void]$script:Notes.Add(''restore_marker_present'') }', '$null = 0')) @('rs_marker/prerestore_label_allowed_but_exit_4_and_last_ok_not_updated'))
    (M 'd6_mirror_not_checked' 'MMOSaveFilesLib.ps1' @(, @("Present = ((`$a1.State -ne 'missing') -or (`$a2.State -ne 'missing'))", "Present = (`$a1.State -ne 'missing')")) @('rs_marker/backup_defers_3_when_only_saved_mirror_present', 'rs_marker/restore_apply_refused_when_only_saved_mirror_present'))
    (M 'd6_mirror_not_written' 'RestoreSaves.ps1' @(, @('$wr2 = Write-RsMarkerFile -Path $script:MirrorPath -Json $markerJson', "`$wr2 = ''")) @('rs_crash/killed_process_leaves_marker_in_backup_root_and_saved_mirror', 'rs_marker/marker_exists_in_both_places_with_same_content_during_apply'))
    (M 'd1_final_path_relation' 'RestoreSaves.ps1' @(, @("if (Test-MMOPathUnder -Path `$froot -Root `$fsav -AllowEqual -ErrorIsViolation) { Stop-Restore 3 'backup_root_inside_saved' }", '$null = 0')) @('rs_final_path/backup_root_inside_saved_through_junction_alias_is_caught'))
    (M 'd2_label_not_in_header_digest' 'MMOSaveFilesLib.ps1' @(, @("'process_check', 'label') {", "'process_check') {")) @('rs_label/label_is_covered_by_header_digest'))
    (M 'd2_collision_label_gate' 'RestoreSaves.ps1' @(, @("{ Stop-Restore 1 'accept_collisions_label_mismatch' }", '{ }')) @('rs_label/accept_collisions_refused_when_manifest_label_is_not_prerestore_even_if_dir_name_is'))
    (M 'd2_preimage_label_gate' 'RestoreSaves.ps1' @(, @("{ Stop-Restore 1 'preimage_label_mismatch' }", '{ }')) @('rs_label/preimage_with_wrong_label_refused_before_any_write'))
    (M 'd2_backup_omits_label' 'BackupSaves.ps1' @(, @(" -Label `$Label -Tool 'BackupSaves/2'", '')) @('rs_label/backup_manifest_has_label_and_tool', 'rs_label/real_preimage_manifest_has_prerestore_label', 'rs_apply/exit_4_partial_state_acknowledged'))
    (M 'd3_quote_not_escaped' 'RestoreSaves.ps1' @(, @('if ($c -eq 39 -or ($c -ge 0x2018 -and $c -le 0x201B)) { [void]$sb.Append($ch) }', '$null = 0')) @('rs_quote/dry_run_next_line_snapshot_path_round_trips_through_the_powershell_parser', 'rs_quote/rollback_command_snapshot_path_round_trips_with_apostrophe_dollar_and_curly_quote'))
    (M 'd4_slot_scope_dropped' 'RestoreSaves.ps1' @(, @("if (`$script:ScopeSlots -ne '') { `$scope =", "if (`$false) { `$scope =")) @('rs_hint/slots_run_rollback_command_is_slot_scoped_not_all'))
    (M 'd4_warnings_removed' 'RestoreSaves.ps1' @(, @('code=rollback_command_stale_if_server_ran', 'code=renamed')) @('rs_hint/failure_prints_not_routine_and_server_ran_warnings_and_recover_first') -All)
    (M 'd7_quarantine_missing_silent' 'RestoreSaves.ps1' @(, @("throw 'quarantine_missing'", 'continue')) @('rs_quarantine/missing_quarantined_file_at_rollback_is_a_reported_failure_not_silent'))
    (M 'd7_both_present_not_reported' 'RestoreSaves.ps1' @(, @("code=quarantine_both_present path=' + `$qrel", "code=none path=' + `$qrel")) @('rs_quarantine/move_failing_with_both_files_present_is_reported_and_nothing_lost'))
    (M 'd9_invisible_chars_not_replaced' 'RestoreSaves.ps1' @(, @(('\' + 'u3164\u2800\u115F\u1160\uFFA0\u034F\u17B4\u17B5'), '')) @('rs_sanitize/bidi_line_separator_braille_hangul_filler_and_c1_are_neutralized', 'rs_sanitize/no_hidden_or_bidi_character_on_any_console_line'))
    (M 'd5_side_mailboxes_not_scanned' 'RestoreSaves.ps1' @(, @("`$isSide = (`$c.Suffix -ceq '.bak' -or `$c.Suffix -ceq '.tmp')", '$isSide = $false')) @('rs_mail/bak_mailbox_sender_trace_is_reported', 'rs_mail/unparseable_tmp_mailbox_is_warning_only'))
    (M 'd10_no_recover_hint_after_unexpected' 'RestoreSaves.ps1' @(, @('code=marker_left_after_unexpected', 'code=nohint')) @('rs_unexpected/unexpected_exception_exit_1_marker_left_with_recover_hint') -All)
)

if ($Only.Count -gt 0) { $mut = @($mut | Where-Object { $Only -contains $_.Id }) }
$work = Join-Path $env:TEMP ('mmo_mut_' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$utf8 = New-Object System.Text.UTF8Encoding($false)
$jobs = New-Object System.Collections.ArrayList
$queue = New-Object System.Collections.ArrayList
$results = New-Object System.Collections.ArrayList
$bad = 0

function New-WorkCopy {
    param([string]$Id)
    $sd = Join-Path (Join-Path $work $Id) 'Scripts'
    [void][IO.Directory]::CreateDirectory($sd)
    foreach ($f in $files) { [IO.File]::Copy((Join-Path $here $f), (Join-Path $sd $f), $true) }
    return $sd
}

if (-not $NoControl) { [void]$queue.Add([pscustomobject]@{ Id = 'control'; Mut = $null }) }
foreach ($m in $mut) { [void]$queue.Add([pscustomobject]@{ Id = $m.Id; Mut = $m }) }

function Start-One {
    param($Item)
    $sd = New-WorkCopy $Item.Id
    $applied = $true
    $note = ''
    if ($null -ne $Item.Mut) {
        $tp = Join-Path $sd $Item.Mut.File
        $txt = [IO.File]::ReadAllText($tp)
        foreach ($pr in $Item.Mut.Pairs) {
            $find = [string]$pr[0]
            $cnt = 0
            $ix = 0
            while (($ix = $txt.IndexOf($find, $ix, [StringComparison]::Ordinal)) -ge 0) { $cnt++; $ix += $find.Length }
            if ($cnt -eq 0 -or ($cnt -gt 1 -and -not $Item.Mut.All)) { $applied = $false; $note = 'find_count=' + $cnt; break }
            $txt = $txt.Replace($find, [string]$pr[1])
        }
        if ($applied) {
            [IO.File]::WriteAllText($tp, $txt, $utf8)
            # UBT/derleme tuzagi yok (betik), ama zaman damgasi yine de dokunulur (calisan kopyanin degistigi acik olsun)
            [IO.File]::SetLastWriteTimeUtc($tp, [datetime]::UtcNow)
        }
    }
    $out = Join-Path (Join-Path $work $Item.Id) 'out.txt'
    $proc = $null
    if ($applied) {
        $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $sd 'TestBackup.ps1'), '-AllowVacuousSavedProof') -RedirectStandardOutput $out -RedirectStandardError ($out + '.err') -PassThru -WindowStyle Hidden
    }
    return [pscustomobject]@{ Item = $Item; Proc = $proc; Out = $out; Applied = $applied; Note = $note }
}

function Complete-One {
    param($J)
    $id = $J.Item.Id
    $fails = New-Object System.Collections.ArrayList
    $runner = $false
    if ($J.Applied -and (Test-Path -LiteralPath $J.Out)) {
        # cikti dosyasi surec bittikten hemen sonra kisa sure kilitli kalabilir: bitisi bekle, ortak okuma + yeniden deneme
        try { $J.Proc.WaitForExit() } catch { }
        $allLines = @()
        for ($try = 0; $try -lt 20; $try++) {
            try {
                $fsr = New-Object IO.FileStream($J.Out, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
                $sr = New-Object IO.StreamReader($fsr, [Text.Encoding]::UTF8)
                try { $allLines = @($sr.ReadToEnd() -split "`r?`n") } finally { $sr.Dispose(); $fsr.Dispose() }
                break
            } catch { Start-Sleep -Milliseconds 500 }
        }
        foreach ($ln in $allLines) {
            if ($ln -cmatch '^\[MMO_BACKUP_CASE\] FAIL case=(?<c>\S+) check=(?<k>\S+)') { [void]$fails.Add($Matches['c'] + '/' + $Matches['k']) }
            if ($ln.StartsWith('[MMO_BACKUP_RUNNER] reported=')) { $runner = $true }
        }
    }
    $verdict = 'FAIL'
    $detail = ''
    if ($id -ceq 'control') {
        if ($runner -and $fails.Count -eq 0) { $verdict = 'CLEAN'; $detail = 'no FAIL lines' } else { $detail = 'runner=' + $runner + ' fails=' + ($fails.ToArray() -join ',') }
    } elseif (-not $J.Applied) { $verdict = 'NOT_APPLIED'; $detail = $J.Note }
    elseif (-not $runner) { $verdict = 'CRASHED'; $detail = 'no runner line (a crash does NOT count as caught)' }
    else {
        $hit = @($J.Item.Mut.Expect | Where-Object { $fails -contains $_ })
        if ($hit.Count -gt 0) { $verdict = 'CAUGHT'; $detail = 'by ' + ($hit -join ',') }
        else { $verdict = 'SURVIVED'; $detail = 'expected one of: ' + ($J.Item.Mut.Expect -join ',') + ' ; fails seen: ' + ($fails.ToArray() -join ',') }
    }
    Write-Host ('[MMO_MUTATION] id=' + $id + ' verdict=' + $verdict + ' ' + $detail)
    if ($fails.Count -gt 0 -and $id -cne 'control') { Write-Host ('[MMO_MUTATION_FAILS] id=' + $id + ' ' + ($fails.ToArray() -join ' ')) }
    if ($verdict -cne 'CAUGHT' -and $verdict -cne 'CLEAN') { $script:bad++ }
}

try {
    $running = New-Object System.Collections.ArrayList
    while ($queue.Count -gt 0 -or $running.Count -gt 0) {
        while ($queue.Count -gt 0 -and $running.Count -lt $Throttle) {
            $item = $queue[0]
            $queue.RemoveAt(0)
            $j = Start-One $item
            if ($null -eq $j.Proc) { Complete-One $j } else { [void]$running.Add($j) }
        }
        foreach ($j in @($running.ToArray())) {
            if ($j.Proc.HasExited) { [void]$running.Remove($j); Complete-One $j }
        }
        Start-Sleep -Milliseconds 700
    }
} finally {
    if (-not $KeepWork) { try { Remove-Item -LiteralPath $work -Recurse -Force } catch { Write-Host ('[MMO_MUTATION_INFO] cleanup_failed ' + $work) } }
    else { Write-Host ('[MMO_MUTATION_INFO] work_kept ' + $work) }
}
$total = $mut.Count
Write-Host ('[MMO_MUTATION_SUMMARY] mutations=' + $total + ' not_caught=' + $bad)
if ($bad -eq 0) { exit 0 }
exit 1
