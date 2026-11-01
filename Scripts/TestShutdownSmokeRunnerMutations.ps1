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
$files = 'ShutdownSmokeEvidence.ps1', 'ShutdownSmokeRunnerLib.ps1', 'TestShutdownSmoke.ps1', 'TestShutdownSmokeRunner.ps1'

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
    (M 'r8_created_files_name_filter_removed' $lib @(, @('if ([IO.Path]::GetFileName($s).IndexOf($PlayerName, [StringComparison]::OrdinalIgnoreCase) -lt 0) { continue }', 'if ($false) { continue }')) @('created_neg/new_file_of_someone_else_not_included', 'created_neg/player_name_only_in_directory_not_included'))
    (M 'r9_path_under_prefix_sibling_accepted' $lib @(, @("return `$p.StartsWith(`$r + '\', [StringComparison]::OrdinalIgnoreCase)", 'return $p.StartsWith($r, [StringComparison]::OrdinalIgnoreCase)')) @('path_neg/sibling_with_same_prefix_is_not_under', 'path_neg/root_itself_is_not_under'))
    (M 'r10_main_checkout_guard_removed' $main @(, @('if ((Test-SmokeIsMainCheckout $RepoRoot) -and -not $AllowMainCheckout) {', 'if ($false) {')) @('gate/main_checkout_is_refused_defer_3', 'lint/main_refuses_main_checkout'))
    (M 'r11_hard_cap_removed_from_exit_wait' $main @(, @("if (`$d -ne 'ok') { [void]`$script:runnerFails.Add('hard_cap_exceeded')", "if (`$false) { [void]`$script:runnerFails.Add('hard_cap_exceeded')")) @('lint/main_enforces_hard_cap_in_exit_wait'))
    (M 'r12_main_body_not_guarded' $main @(, @('Invoke-SmokeGuarded -State $state -Body {', 'Invoke-SmokeUnguarded -State $state -Body {')) @('lint/main_uses_guarded_wrapper'))
    (M 'r13_client_not_stopped_by_runner' $main @(, @('Stop-SmokeOwnedProcess $state $cli', '$null = $cli')) @('lint/main_kills_client_itself_after_server_exit'))
    (M 'r14_main_kills_by_name' $main @(, @("Say 'client stopped by runner'", "Stop-Process -Name MmoNoSuchProcessForMutation -Force -ErrorAction SilentlyContinue; Say 'client stopped by runner'")) @('lint/no_stop_process'))
    (M 'r15_injection_bypasses_recording_helper' $main @(, @('Set-SmokeReadOnly $state $script:savePath', '[IO.File]::SetAttributes($script:savePath, [IO.FileAttributes]::ReadOnly)')) @('lint/main_injects_via_set_smoke_readonly', 'lint/main_never_sets_attributes_directly'))
    (M 'r16_player_name_not_passed_to_evaluator' $main @(, @('-PlayerA $script:playerName', '-PlayerA ''SmokeL4A''')) @('lint/main_passes_player_name_from_log_to_evaluator'))
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
