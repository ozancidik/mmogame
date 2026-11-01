# Kapanis duman testi kanit degerlendiricisi icin mutasyon sinamasi (docs/DESIGN_SHUTDOWN_SMOKE.md, dilim S0, bolum 7.2).
# Her mutasyon, ISLEYEN KOPYADA Scripts/ShutdownSmokeEvidence.ps1'i bozar ve Scripts/TestShutdownSmokeEvidence.ps1'i o kopyadan calistirir.
# Her mutasyon testi BELIRLI bir FAIL satiriyla (case/check etiketi) kirmali; yalniz "cokme" (runner satiri yok) SAYILMAZ.
# Gercek depo dosyalarina DOKUNULMAZ: kopya dizini %TEMP% altinda mmo_smutation_<guid>\<id>\Scripts; her mutasyon kendi kopyasinda, paralel (Throttle).
# Kalip: Scripts/TestBackupMutations.ps1. UE/oyun/sunucu BASLATILMAZ.
# Kullanim: powershell.exe -NoProfile -ExecutionPolicy Bypass -File Scripts/TestShutdownSmokeEvidenceMutations.ps1 [-Only id1,id2] [-Throttle 4] [-KeepWork] [-NoControl]
# Cikis: 0 = control temiz + her mutasyon beklenen FAIL satiriyla yakalandi; 1 = yakalanmayan / cokme / uygulanamayan mutasyon / control kirik.
# M9-M11: belge 7.2'de runner (S2) icin yazilmis mutasyonlar yerine, bu dilimin degerlendirici korumalari sinanir (sahte satir, istemci logunda sahte kanit, cikis kodu dogrulamasi);
# runner mutasyonlari (finally'de salt-okunur geri alma, ad ile Stop-Process, ana Saved koku) S2'de TestShutdownSmokeRunner.ps1 ile birlikte gelir.
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII.
param([string[]]$Only = @(), [int]$Throttle = 4, [switch]$KeepWork, [switch]$NoControl)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ -ne '' })   # powershell.exe -File ile virgullu liste tek dize gelir
$here = $PSScriptRoot
$files = 'ShutdownSmokeEvidence.ps1', 'TestShutdownSmokeEvidence.ps1'

function M {
    # Id, Dosya, @(Bul, Degistir) ciftleri (her Bul KOPYADA tam olarak bir kez bulunmali), beklenen FAIL etiketleri (case/check)
    param([string]$Id, [string]$File, [object[]]$Pairs, [string[]]$Expect)
    return [pscustomobject]@{ Id = $Id; File = $File; Pairs = $Pairs; Expect = $Expect }
}

$ev = 'ShutdownSmokeEvidence.ps1'
$mut = @(
    (M 'm1_run_id_compare_removed' $ev @(, @('if ($jrun -cne $logRun) {', 'if ($false) {')) @('neg_json_run_id_stale_file/verdict_fail', 'neg_json_run_id_stale_file/reason_result_json_stale_run_id'))
    (M 'm2_json_vs_process_exit_removed' $ev @(, @('if ($jsonOk -and $null -ne $jsonCode -and $pec -ne $jsonCode) {', 'if ($false) {')) @('neg_process_exit_differs_from_json/reason_process_exit_json_mismatch'))
    (M 'm3_skipped_editor_counts_as_pass' $ev @(, @("if (`$skippedEditor) { AddInc 'skipped_editor_exit_path_not_exercised' }", 'if ($skippedEditor) { }')) @('neg_skipped_editor_trace_is_never_a_pass/verdict_inconclusive', 'neg_skipped_editor_trace_is_never_a_pass/reason_skipped_editor_exit_path_not_exercised'))
    (M 'm4_event_once_rule_loosened' $ev @(, @("if (`$n -gt 1) { AddFail ('duplicate_event:'", "if (`$n -gt 99) { AddFail ('duplicate_event:'")) @('neg_shutdown_exit_twice/verdict_fail', 'neg_shutdown_exit_twice/reason_duplicate_event:exit', 'neg_closing_phase_twice/reason_duplicate_event:p_closing'))
    (M 'm5_sav_start_window_removed' $ev @(, @('if ($lw -lt $st.AddSeconds(-$TimeToleranceSeconds)) {', 'if ($false) {')) @('neg_sav_older_than_server_start/verdict_fail', 'neg_sav_older_than_server_start/reason_save_file_not_written_this_run'))
    (M 'm6_inconclusive_maps_to_pass' $ev @(, @("elseif (`$incs.Count -gt 0) { `$verdict = 'INCONCLUSIVE' }", "elseif (`$incs.Count -gt 0) { `$verdict = 'PASS' }")) @('neg_client_log_empty/verdict_inconclusive', 'neg_server_log_null/verdict_inconclusive', 'neg_skipped_editor_trace_is_never_a_pass/verdict_inconclusive'))
    (M 'm7_error_line_scan_removed' $ev @(, @("Scan-ErrorLines `$lines 'server' `$allowRx", '$null = 0')) @('neg_error_line_in_server_log/verdict_fail', 'neg_error_line_in_server_log/reason_log_error_line', 'neg_ensure_line_in_server_log/reason_log_error_line'))
    (M 'm8_order_check_removed' $ev @(, @('if ($idx[$pr[1]] -lt $idx[$pr[0]]) {', 'if ($false) {')) @('neg_exiting_logged_before_kick/verdict_fail', 'neg_closing_before_scheduled/reason_order_violation:sched<p_closing', 'neg_kick_before_flush_done/reason_order_violation:flush_done<first_kick'))
    (M 'm9_forged_or_malformed_line_accepted' $ev @(, @("if (-not `$matched) { AddFail 'malformed_evidence_line' ('line=' + `$n) }", 'if ($false) { }')) @('neg_forged_exit_line_in_other_log_category/verdict_fail', 'neg_forged_exit_line_in_other_log_category/reason_malformed_evidence_line', 'neg_non_ascii_player_name_in_evidence_line/reason_malformed_evidence_line'))
    (M 'm10_client_log_fake_evidence_accepted' $ev @(, @("if (`$clientEvidence) { AddFail 'client_log_contains_server_evidence' }", 'if ($false) { }')) @('neg_client_log_carries_a_fake_shutdown_exit_line/verdict_fail', 'neg_client_log_carries_a_fake_shutdown_exit_line/reason_client_log_contains_server_evidence'))
    (M 'm11_process_exit_code_range_unchecked' $ev @(, @('if ($null -ne $pec -and ($pec -lt 0 -or $pec -gt 255)) {', 'if ($false) {')) @('neg_process_exit_code_negative/reason_process_exit_code_out_of_range', 'neg_process_exit_code_huge/reason_process_exit_code_out_of_range'))
    (M 'm12_player_checks_not_degraded_without_client_log' $ev @(, @('if ($noClient) { AddInc $Code $Detail } else { AddFail $Code $Detail }', 'AddFail $Code $Detail')) @('neg_playerless_run_without_client_log_is_inconclusive/verdict_inconclusive', 'neg_playerless_run_without_client_log_is_inconclusive/reason_flush_done_saved_zero_degraded', 'neg_playerless_run_without_client_log_is_inconclusive/no_fail_reasons'))
)

if ($Only.Count -gt 0) { $mut = @($mut | Where-Object { $Only -contains $_.Id }) }
$work = Join-Path $env:TEMP ('mmo_smutation_' + [Guid]::NewGuid().ToString('N'))
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
        $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $sd 'TestShutdownSmokeEvidence.ps1')) -RedirectStandardOutput $out -RedirectStandardError ($out + '.err') -PassThru -WindowStyle Hidden
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
