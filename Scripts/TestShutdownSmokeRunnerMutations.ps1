# Kapanis duman testi KOSUCUSU icin mutasyon sinamasi (docs/DESIGN_SHUTDOWN_SMOKE.md, dilim S2/S3, bolum 7.2 M9-M11 + ek).
# Her mutasyon, ISLEYEN KOPYADA Scripts/ShutdownSmokeRunnerLib.ps1 veya Scripts/TestShutdownSmoke.ps1'i bozar ve Scripts/TestShutdownSmokeRunner.ps1'i o kopyadan calistirir.
# Her mutasyon testi BELIRLI bir FAIL satiriyla (case/check etiketi) kirmali; yalniz "cokme" (runner satiri yok) SAYILMAZ.
# Gercek depo dosyalarina DOKUNULMAZ: kopya dizini %TEMP% altinda mmo_rmutation_<guid>\<id>\Scripts. UE/oyun/sunucu BASLATILMAZ.
# Kalip: Scripts/TestShutdownSmokeEvidenceMutations.ps1.
# Kullanim: powershell.exe -NoProfile -ExecutionPolicy Bypass -File Scripts/TestShutdownSmokeRunnerMutations.ps1 [-Only id1,id2] [-Throttle 3] [-KeepWork] [-NoControl]
# Cikis: 0 = control temiz + her mutasyon beklenen FAIL satiriyla yakalandi; 1 = yakalanmayan / cokme / uygulanamayan mutasyon / control kirik.
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII.
param([string[]]$Only = @(), [int]$Throttle = 3, [switch]$KeepWork, [switch]$NoControl)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ -ne '' })   # powershell.exe -File ile virgullu liste tek dize gelir
$here = $PSScriptRoot
$files = 'ShutdownSmokeEvidence.ps1', 'MMOSaveFilesLib.ps1', 'ShutdownSmokeRunnerLib.ps1', 'TestShutdownSmoke.ps1', 'TestShutdownSmokeRunner.ps1'

function M {
    # Id, Dosya, @(Bul, Degistir) ciftleri (her Bul KOPYADA tam olarak bir kez bulunmali), beklenen FAIL etiketleri (case/check)
    param([string]$Id, [string]$File, [object[]]$Pairs, [string[]]$Expect)
    return [pscustomobject]@{ Id = $Id; File = $File; Pairs = $Pairs; Expect = $Expect }
}

$lib = 'ShutdownSmokeRunnerLib.ps1'
$main = 'TestShutdownSmoke.ps1'
$mut = @(
    (M 'r1_cleanup_kills_by_name' $lib @(, @('foreach ($p in @($State.Procs.ToArray())) { Stop-SmokeOwnedProcess $State $p }', 'foreach ($p in @($State.Procs.ToArray())) { Stop-SmokeOwnedProcess $State $p }; Stop-Process -Name MmoNoSuchProcessForMutation -Force -ErrorAction SilentlyContinue')) @('lint/no_stop_process'))
    (M 'r2_cleanup_skips_readonly_restore' $lib @(, @('Restore-SmokeReadOnly $State', '$null = 0')) @('readonly/cleanup_clears_readonly_flag', 'guarded/readonly_restored_when_body_throws', 'order/after_restore_hook_runs_after_readonly_cleared'))
    (M 'r3_guarded_finally_emptied' $lib @(, @('finally { Invoke-SmokeCleanup $State }', 'finally { }')) @('guarded/readonly_restored_when_body_throws', 'guarded/owned_process_killed_on_early_return', 'guarded/cleanup_done_flag_after_throw'))
    (M 'r4_inconclusive_exit_code_is_zero' $lib @(, @("'INCONCLUSIVE' { return 4 }", "'INCONCLUSIVE' { return 0 }")) @('exitcode/inconclusive_is_4'))
    (M 'r5_runner_inconclusive_ignored' $lib @(, @("if (@(`$RunnerInconclusives).Count -gt 0) { return 'INCONCLUSIVE' }", "if (`$false) { return 'INCONCLUSIVE' }")) @('final_neg/runner_inconclusive_blocks_pass'))
    (M 'r6_hard_cap_off_by_one' $lib @(, @('if ([double]$TotalElapsed -ge [double]$HardCap) { return ''hard_cap'' }', 'if ([double]$TotalElapsed -gt [double]$HardCap) { return ''hard_cap'' }')) @('time/hard_cap_at_equal'))
    (M 'r7_unknown_dotnet_not_busy' $lib @(, @('([string]::IsNullOrWhiteSpace($cmd) -or $cmd -match', '($cmd -match')) @('busy/dotnet_with_unknown_command_line_is_busy'))
    (M 'r8_created_files_exact_name_group_removed' $lib @(, @('if ($c.Group -cne $canon) { continue }', 'if ($false) { continue }')) @('created_neg/short_name_a_does_not_match_ab_ba_alice_or_logs', 'created_neg/name_MMO_matches_only_its_own_save', 'created_neg/longer_name_with_same_prefix_not_included'))
    (M 'r9_path_under_prefix_sibling_accepted' $lib @(, @("return `$p.StartsWith(`$r + '\', [StringComparison]::OrdinalIgnoreCase)", 'return $p.StartsWith($r, [StringComparison]::OrdinalIgnoreCase)')) @('path_neg/sibling_with_same_prefix_is_not_under', 'path_neg/root_itself_is_not_under'))
    (M 'r10_main_checkout_guard_removed' $main @(, @('if ((Test-SmokeIsMainCheckout $RepoRoot) -and -not $AllowMainCheckout) {', 'if ($false) {')) @('gate/main_checkout_is_refused_defer_3', 'lint/main_refuses_main_checkout'))
    (M 'r11_hard_cap_removed_from_exit_wait' $main @(, @("if (`$d -ne 'ok') { [void]`$script:runnerFails.Add('hard_cap_exceeded'); Say ('hard cap exceeded at '", "if (`$false) { [void]`$script:runnerFails.Add('hard_cap_exceeded'); Say ('hard cap exceeded at '")) @('lint/main_enforces_hard_cap_in_exit_wait'))
    (M 'r12_main_body_not_guarded' $main @(, @('Invoke-SmokeGuarded -State $state -Body {', 'Invoke-SmokeUnguarded -State $state -Body {')) @('lint/main_uses_guarded_wrapper'))
    (M 'r13_client_not_stopped_by_runner' $main @(, @('Stop-SmokeOwnedProcess $state $cli     #', '$null = $cli     #')) @('lint/main_kills_client_itself_after_server_exit'))
    (M 'r14_main_kills_by_name' $main @(, @("Say 'client stopped by runner'", "Stop-Process -Name MmoNoSuchProcessForMutation -Force -ErrorAction SilentlyContinue; Say 'client stopped by runner'")) @('lint/no_stop_process'))
    (M 'r15_injection_bypasses_recording_helper' $main @(, @('Set-SmokeReadOnly $state $script:savePath', '[IO.File]::SetAttributes($script:savePath, [IO.FileAttributes]::ReadOnly)')) @('lint/main_injects_via_set_smoke_readonly', 'lint/main_never_sets_attributes_directly'))
    (M 'r16_player_name_not_passed_to_evaluator' $main @(, @('-PlayerA $script:playerName', '-PlayerA ''SmokeL4A''')) @('lint/main_passes_player_name_from_log_to_evaluator'))
    (M 'r17_busy_rows_never_reported' $lib @(, @('if ($hit) { [void]$busy.Add($p) }', 'if ($false) { [void]$busy.Add($p) }')) @('busy/unreal_editor_cmd_is_busy', 'busy/msbuild_is_busy'))
    (M 'r18_cl_and_link_not_busy' $lib @(, @('UnrealTraceServer|cl|link|MyProject', 'UnrealTraceServer|MyProject')) @('busy/cl_exe_is_busy', 'busy/link_exe_is_busy'))
    (M 'r19_main_ignores_busy_processes' $main @(, @('if ($busy.Count -gt 0) { Defer (', 'if ($false) { Defer (')) @('lint/main_defers_when_busy_found'))
    (M 'r20_main_skips_worktree_registration' $main @(, @('if (-not (Test-SmokeIsRegisteredLinkedWorktree $RepoRoot ($wtList -join "`n"))) { Defer', 'if ($false) { Defer')) @('gate/unregistered_worktree_defers_3', 'lint/main_defers_unregistered_worktree'))
    (M 'r21_descendant_parent_time_filter_removed' $lib @(, @('if ($cr -lt $cur.Creation.AddMilliseconds(-$script:SmokeTolMs)) { continue }', 'if ($false) { continue }')) @('descendants_neg/reused_parent_pid_child_older_than_parent_excluded', 'descendants_neg/grandchild_older_than_its_parent_excluded'))
    (M 'r22_tree_kill_identity_check_removed' $lib @(, @('if (-not (Test-SmokeSameInstant $hs $d.Creation)) {', 'if ($false) {')) @('cleanup_tree/pid_reuse_start_mismatch_not_killed'))
    (M 'r23_tree_kill_collects_no_descendants' $lib @(, @('$inv = & $State.Inventory', '$inv = @()')) @('cleanup_tree/child_and_grandchild_killed', 'real_tree/real_grandchild_killed_by_tree_cleanup'))
    (M 'r24_job_without_kill_on_close' $lib @(, @('info.BasicLimitInformation.LimitFlags = 0x2000;', 'info.BasicLimitInformation.LimitFlags = 0;')) @('job/runner_killed_children_die', 'job/close_kills_assigned_process_even_without_handle_kill'))
    (M 'r25_job_assignment_skipped' $lib @(, @('try { $ok = [MMOSmokeJobNative]::Assign($State.Job, $Proc.Handle) } catch { $ok = $false }', '$ok = $true')) @('job/owned_process_is_in_job'))
    (M 'r26_recovery_start_time_check_removed' $lib @(, @('if ($liveName -cne $eName -or -not (Test-SmokeSameInstant (Get-SmokeProcStartUtc $p) $eStart)) {', 'if ($liveName -cne $eName) {')) @('recovery_neg/pid_reuse_start_mismatch_not_killed', 'real_recovery_neg/wrong_start_time_leaves_the_real_process_alone'))
    (M 'r27_recovery_readonly_not_restored' $lib @(, @('[IO.File]::SetAttributes($path, (([IO.File]::GetAttributes($path)) -band (-bnot [IO.FileAttributes]::ReadOnly)))', '$null = 0')) @('recovery/readonly_flag_cleared'))
    (M 'r28_recovery_readonly_class_check_removed' $lib @(, @('if ((Get-MMOSaveFileClass $rel).Class -eq ''character'' -and', 'if ($true -and')) @('recovery_neg/non_character_readonly_path_untouched'))
    (M 'r29_recovery_ignores_live_runner' $lib @(, @('if (Test-SmokeSameInstant (Get-SmokeProcStartUtc $rp) $rs) { $res.RunnerAlive = $true;', 'if ($false) { $res.RunnerAlive = $true;')) @('recovery_neg/live_runner_marker_not_touched'))
    (M 'r30_recovery_unreal_name_rule_removed' $lib @(, @('if ($eName -cnotmatch ''^UnrealEditor[A-Za-z0-9_\-]*$'' -or $null -eq $eStart) {', 'if ($null -eq $eStart) {')) @('recovery_neg/tampered_marker_naming_a_non_unreal_process_never_kills'))
    (M 'r31_recovery_own_pid_not_protected' $lib @(, @('-or [int]$ePid -eq $PID', '')) @('recovery_neg/own_pid_is_never_killed'))
    (M 'r32_removal_reparse_check_removed' $lib @(, @('if ((Test-SmokePathUnder $SavedRoot $f) -and -not (Test-SmokePathHasReparse $SavedRoot $f)) {', 'if ($true) {')) @('remove_neg/junction_component_file_not_deleted', 'lint/removal_helper_checks_under_saved_and_reparse_chain'))
    (M 'r33_evidence_json_copy_empty' $lib @(, @('try { $in.CopyTo($out) } finally { $out.Dispose() }', 'try { } finally { $out.Dispose() }')) @('evidence/json_copy_bytes_equal'))
    (M 'r34_marker_never_removed' $lib @(, @('if ($State.Problems.Count -eq 0) { Remove-SmokeMarker $State }', 'if ($false) { Remove-SmokeMarker $State }')) @('marker/removed_after_clean_cleanup'))
    (M 'r35_marker_removed_despite_problems' $lib @(, @('if ($State.Problems.Count -eq 0) { Remove-SmokeMarker $State }', 'if ($true) { Remove-SmokeMarker $State }')) @('marker_neg/kept_when_cleanup_has_problems'))
    (M 'r36_multihome_flag_dropped' $lib @(, @('''-MultiHome=127.0.0.1'',', '')) @('srv_args/exact_list', 'srv_args/binds_loopback_only_via_multihome'))
    (M 'r37_client_self_exit_dropped' $lib @(, @('if ($MaxSeconds -gt 0) { $a += (''-seconds='' + $MaxSeconds) }', '')) @('cli_args/self_exit_seconds_appended'))
    (M 'r38_worktree_match_always_true' $lib @(, @('if ($entries[$i] -ieq $r) { return $true }', 'if ($true) { return $true }')) @('worktree_neg/unlisted_path_is_not_registered', 'worktree_neg/prefix_of_registered_path_is_not_registered'))
    (M 'r39_worktree_main_entry_counted' $lib @(, @('for ($i = 1; $i -lt $entries.Count; $i++) {', 'for ($i = 0; $i -lt $entries.Count; $i++) {')) @('worktree_neg/first_entry_main_worktree_is_not_linked'))
    (M 'r40_port_hold_does_not_bind' $lib @(, @('$s.Bind((New-Object Net.IPEndPoint([Net.IPAddress]::Loopback, $Port)))', '$null = 0')) @('port_hold_neg/second_hold_on_same_port_fails'))
    (M 'r41_listen_port_any_driver' $lib @(, @('(?m)LogNet: Name:GameNetDriver [^\r\n]*IpNetDriver', '(?m)LogNet: Name:[A-Za-z]+ [^\r\n]*IpNetDriver')) @('listen_port_neg/other_driver_name_is_not_counted'))
    (M 'r42_reparse_chain_not_walked' $lib @(, @('if (Test-SmokeReparse $cur) { return $true }', 'if ($false) { return $true }')) @('reparse_chain/junction_component_in_save_path_detected'))
    (M 'r43_evidence_json_text_dropped' $lib @(, @('$b.JsonText = Read-SmokeRawText $ResultJson', '$b.JsonText = $null')) @('evidence/json_text_keeps_bom_for_the_evaluator'))
    (M 'r44_tree_kill_skips_descendants' $lib @(, @('try { if (-not $k.HasExited) { $k.Kill() } } catch { }', 'try { } catch { }')) @('cleanup_tree/child_and_grandchild_killed', 'real_tree/real_grandchild_killed_by_tree_cleanup'))
    (M 'r45_cmd_build_bat_not_busy' $lib @(, @('elseif ($name -ieq ''cmd.exe'' -and', 'elseif ($false -and')) @('busy/cmd_running_build_bat_is_busy', 'busy/cmd_running_ubt_command_line_is_busy'))
    (M 'r46_main_recovery_renamed_away' $main @(, @('Invoke-SmokeOrphanRecovery -MarkerDirectory $MarkerDirectory', 'Invoke-SmokeOrphanRecoveryDisabled -MarkerDirectory $MarkerDirectory')) @('lint/main_runs_orphan_recovery_before_busy_check'))
    (M 'r47_main_ignores_listen_port_mismatch' $main @(, @('if ($lp.Count -ne 1 -or $lp[0] -ne $selected) {', 'if ($false) {')) @('lint/main_verifies_real_listening_port'))
    (M 'r48_main_ignores_failed_port_hold' $main @(, @('if ($null -eq $portHold) { Defer (', 'if ($false) { Defer (')) @('lint/main_defers_when_port_hold_fails'))
    (M 'r49_main_evidence_not_in_guarded_body' $main @(, @('$script:evidence = Get-SmokeEvidenceBundle', '$script:ev2 = Get-SmokeEvidenceBundle')) @('lint/main_captures_evidence_inside_guarded_body_before_cleanup'))
    (M 'r50_main_accepts_preexisting_player_save' $main @(, @('if (@($snapshotBefore | Where-Object { $_ -ieq $script:savePath }).Count -gt 0) {', 'if ($false) {')) @('lint/main_refuses_preexisting_player_save'))
    (M 'r51_main_no_reparse_check_on_save_path' $main @(, @('if (Test-SmokePathHasReparse $saved $script:savePath) {', 'if ($false) {')) @('lint/main_checks_save_path_reparse_chain'))
    (M 'r52_main_without_job_object' $main @(, @('$state.Job = New-SmokeJob', '$state.Job = $null')) @('lint/main_creates_job_object_and_marker_before_starting_processes'))
    (M 'r53_main_marker_failure_ignored' $main @(, @('Defer (''marker_not_writable ', 'Say (''marker_not_writable ')) @('lint/main_defers_when_marker_unwritable'))
    (M 'r54_main_live_runner_not_deferred' $main @(, @('if ($rec.RunnerAlive) { Defer ''another_smoke_runner_active'' }', 'if ($false) { }')) @('lint/main_defers_on_recovery_problems_or_live_runner'))
    (M 'r55_main_worktree_list_failure_ignored' $main @(, @('if ($wtListCode -ne 0) { Defer ''worktree_list_unresolved'' }', 'if ($false) { }')) @('lint/main_defers_when_worktree_list_fails'))
    (M 'r56_main_port_in_use_ignored' $main @(, @('if ($used.ContainsKey($Port)) { Defer (', 'if ($false) { Defer (')) @('lint/main_defers_when_port_in_use'))
    (M 'r57_main_client_without_self_exit' $main @(, @('-LogPath $clientLog -MaxSeconds ($HardCapSeconds + $ClientSelfExitMargin)', '-LogPath $clientLog')) @('lint/main_gives_client_a_self_exit_safety_net'))
    # S4 (loginclosed): kosucu mantigi + yardimcilar
    (M 'r58_client_b_started_without_waiting_for_closing' $main @(, @('if (Test-SmokeClosingSeen $slogText) { break }', 'if ($true) { break }')) @('lint/client_b_starts_only_after_closing_seen_in_server_log'))
    (M 'r59_client_b_not_stopped_by_runner' $main @(, @('Stop-SmokeOwnedProcess $state $cliB', '$null = $cliB')) @('lint/client_b_stopped_by_runner_itself'))
    (M 'r60_client_b_not_in_job_or_marker' $main @(, @('Add-SmokeOwnedProcess $state $cliB', '$null = $cliB')) @('lint/client_b_registered_in_job_and_marker_via_owned_helper'))
    (M 'r61_client_b_unresolved_before_flushing_not_inconclusive' $main @(, @("elseif (Test-SmokePhaseSeen `$slogText 'Flushing') { [void]`$script:runnerInc.Add('client_b_no_prelogin_before_flushing'); return }", 'elseif ($false) { }')) @('lint/client_b_unresolved_before_flushing_is_inconclusive_not_pass'))
    (M 'r62_client_b_rejection_assumed_not_read' $main @(, @('(Get-SmokePreloginRejectedCount $slogText) -ge 1', '$true')) @('lint/client_b_rejection_is_read_from_server_log_not_assumed'))
    (M 'r63_client_b_log_not_given_to_evaluator' $main @(, @('$bArgs[''ClientBLogText''] = $script:evidence.ClientBText', '$bArgs[''ClientBLogText''] = $null')) @('lint/client_b_log_and_reject_pattern_go_to_the_evaluator'))
    (M 'r64_client_b_reject_pattern_dropped' $main @(, @('$bArgs[''ClientRejectPattern''] = $ClientRejectPattern', '$bArgs[''ClientRejectPattern''] = ''''')) @('lint/client_b_log_and_reject_pattern_go_to_the_evaluator'))
    (M 'r65_client_b_without_self_exit' $main @(, @('-LogPath $clientBLog -MaxSeconds ($HardCapSeconds + $ClientSelfExitMargin)', '-LogPath $clientBLog')) @('lint/client_b_has_self_exit_safety_net'))
    (M 'r66_closing_wait_without_hard_cap' $main @(, @('{ [void]$script:runnerFails.Add(''hard_cap_exceeded''); Say (''hard cap exceeded waiting for Closing at ''', '{ Say (''hard cap exceeded waiting for Closing at ''')) @('lint/client_b_wait_for_closing_is_hard_capped'))
    (M 'r67_extra_join_not_surfaced' $main @(, @('if ($jn.Invalid -or $jn.Count -gt 1) {', 'if ($false) {')) @('lint/client_b_extra_join_is_left_to_the_evaluator_to_fail'))
    (M 'r68_scenario_allow_list_widened' $main @(, @("@('clean', 'savefail', 'loginclosed') -cnotcontains `$Scenario", "@('clean', 'savefail', 'loginclosed', 'bogus') -cnotcontains `$Scenario")) @('gate/unknown_scenario_is_usage_error_2'))
    (M 'r69_phase_seen_prefix_match' $lib @(, @("'event=shutdown_phase phase=' + `$Phase + ' ')", "'event=shutdown_phase phase=' + `$Phase)")) @('phase_seen_neg/phase_name_prefix_is_not_a_match'))
    (M 'r70_phase_name_not_validated' $lib @(, @("-or `$Phase -cnotmatch '^[A-Za-z]{1,12}`$'", '-or $false')) @('phase_seen_neg/regex_metacharacters_in_phase_name_refused'))
    (M 'r71_joined_names_count_zero' $lib @(, @('$res.Count = $names.Count', '$res.Count = 0')) @('joined_names/single_join_counts_one', 'joined_names/second_player_makes_count_two'))
    (M 'r72_prelogin_count_loose' $lib @(, @('event=prelogin_rejected phase=[A-Za-z?]{1,12}[ \t]*\r?$''', 'event=prelogin_rejected''')) @('prelogin_count_neg/trailing_junk_not_counted'))
    (M 'r73_bundle_never_reads_client_b' $lib @(, @('if (-not [string]::IsNullOrEmpty($ClientBLog)) { $b.ClientBText = Read-SmokeTextShared $ClientBLog }', '$null = 0')) @('evidence/client_b_text_is_read_when_requested'))
)

if ($Only.Count -gt 0) { $mut = @($mut | Where-Object { $Only -contains $_.Id }) }
$work = Join-Path $env:TEMP ('mmo_rmutation_' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$utf8 = New-Object System.Text.UTF8Encoding($false)
$queue = New-Object System.Collections.ArrayList
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
            if ($cnt -ne 1) { $applied = $false; $note = 'find_count=' + $cnt; break }
            $txt = $txt.Replace($find, [string]$pr[1])
        }
        if ($applied) {
            [IO.File]::WriteAllText($tp, $txt, $utf8)
            [IO.File]::SetLastWriteTimeUtc($tp, [datetime]::UtcNow)
        }
    }
    $out = Join-Path (Join-Path $work $Item.Id) 'out.txt'
    $proc = $null
    if ($applied) {
        $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $sd 'TestShutdownSmokeRunner.ps1')) -RedirectStandardOutput $out -RedirectStandardError ($out + '.err') -PassThru -WindowStyle Hidden
    }
    return [pscustomobject]@{ Item = $Item; Proc = $proc; Out = $out; Applied = $applied; Note = $note }
}

function Complete-One {
    param($J)
    $id = $J.Item.Id
    $fails = New-Object System.Collections.ArrayList
    $runner = $false
    $passLine = $false
    if ($J.Applied -and (Test-Path -LiteralPath $J.Out)) {
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
            if ($ln -cmatch '^\[MMO_SMOKE_CASE\] FAIL case=(?<c>\S+) check=(?<k>\S+)') { [void]$fails.Add($Matches['c'] + '/' + $Matches['k']) }
            if ($ln.StartsWith('[MMO_SMOKE_RUNNER] reported=')) { $runner = $true }
            if ($ln.StartsWith('[MMO_SMOKE_RUNNER] PASS checks=')) { $passLine = $true }
        }
    }
    $verdict = 'FAIL'
    $detail = ''
    if ($id -ceq 'control') {
        if ($runner -and $passLine -and $fails.Count -eq 0) { $verdict = 'CLEAN'; $detail = 'no FAIL lines, runner PASS' } else { $detail = 'runner=' + $runner + ' pass=' + $passLine + ' fails=' + ($fails.ToArray() -join ',') }
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
        Start-Sleep -Milliseconds 400
    }
} finally {
    if (-not $KeepWork) { try { Remove-Item -LiteralPath $work -Recurse -Force } catch { Write-Host ('[MMO_MUTATION_INFO] cleanup_failed ' + $work) } }
    else { Write-Host ('[MMO_MUTATION_INFO] work_kept ' + $work) }
}
$total = $mut.Count
Write-Host ('[MMO_MUTATION_SUMMARY] mutations=' + $total + ' not_caught=' + $bad)
if ($bad -eq 0) { exit 0 }
exit 1
