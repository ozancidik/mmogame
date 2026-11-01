# Kapanis duman testi kanit degerlendiricisi (Scripts/ShutdownSmokeEvidence.ps1) fikstur testi (docs/DESIGN_SHUTDOWN_SMOKE.md, dilim S0).
# UE, oyun, sunucu, gercek surec, gercek Saved/ YOK: yalniz bellekte uretilen log/JSON/dosya-bilgisi fiksturleri.
# Her vaka ayri CASE satiri basar: [MMO_SMOKE_CASE] PASS|FAIL case=<vaka> check=<kontrol>.
# Olumsuz vakalar hem verdict (FAIL/INCONCLUSIVE, asla PASS) hem NEDEN kodu icin ayri kontrol uretir.
# Kalip: TestBackup.ps1 / TestServerLifecycle.ps1 (reported = case_lines = unique_labels = $expected; N: bu dosya, CASE sayaci, commit mesaji).
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII; ASCII disi karakterler [char] ile uretilir.
# Cikis: 0 PASS, 1 FAIL.
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ShutdownSmokeEvidence.ps1')

# Beklenen = benzersiz case/check etiketi sayisi.
$expected = 254

$script:labels = @{}
$script:caseLines = 0
$script:failures = 0
$script:emitted = New-Object System.Collections.ArrayList

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
    $line = '[MMO_SMOKE_CASE] ' + $tag + ' case=' + $Case + ' check=' + $Check + $note
    [void]$script:emitted.Add($line)
    Write-Host $line
}

# ---- fikstur kurucular ----------------------------------------------------------------------------------------------------
$runId = '20261101T120000Z-4242'
$startUtc = New-Object DateTime 2026, 11, 1, 12, 0, 0, ([DateTimeKind]::Utc)
$LC = 'LogMMOLifecycle: Display: '
$LW = 'LogMMOLifecycle: Warning: '

function LI {
    param([string]$K, [string]$T, [bool]$Raw = $false)
    return [pscustomobject]@{ K = $K; T = $T; Raw = $Raw }
}

function New-LogItems {
    param([string]$Scn)
    $a = New-Object 'System.Collections.Generic.List[object]'
    $failed = ($Scn -ceq 'savefail')
    $code = 0; $nfail = 0; $saveRes = 'saved'; $saved = 1; $ffail = 0
    if ($failed) { $code = 2; $nfail = 1; $saveRes = 'failed'; $saved = 0; $ffail = 1 }
    $a.Add((LI 'init' 'LogInit: Display: Log file open, C:\Fixture\Saved\Logs\smoke.log'))
    $a.Add((LI 'ready' ($LC + 'event=lifecycle_ready netmode=2 phase=Idle')))
    $a.Add((LI 'stale' ($LC + 'event=stale_result_cleared ok=1 run_id=' + $runId)))
    $a.Add((LI 'sched' ($LC + 'event=shutdown_scheduled seconds=210 reason=bakim')))
    $a.Add((LI 'join' 'LogNet: Join succeeded: SmokeL4A'))
    $a.Add((LI 'p_closing' ($LC + 'event=shutdown_phase phase=Closing remaining=60 players=1')))
    $a.Add((LI 'm_closing' ($LC + '[MMOMETRIC] t=100.0 ev=shutdown phase=Closing players=1')))
    if ($Scn -ceq 'loginclosed') { $a.Add((LI 'prelogin' ($LC + 'event=prelogin_rejected phase=Closing'))) }
    $a.Add((LI 'p_flushing' ($LC + 'event=shutdown_phase phase=Flushing remaining=20 players=1')))
    $a.Add((LI 'm_flushing' ($LC + '[MMOMETRIC] t=140.0 ev=shutdown phase=Flushing players=1')))
    $a.Add((LI 'flush_begin' ($LC + 'event=shutdown_flush_begin players=1')))
    $a.Add((LI 'save' ($LC + 'event=shutdown_save player=SmokeL4A result=' + $saveRes)))
    if ($failed) { $a.Add((LI 'save_failed' ($LW + '[MMOMETRIC] t=141.0 ev=shutdown_save_failed player=SmokeL4A'))) }
    $a.Add((LI 'flush_done' ($LC + '[MMOMETRIC] t=141.5 ev=shutdown phase=flush_done players=1 saved=' + $saved + ' failed=' + $ffail + ' skipped=0')))
    $a.Add((LI 'p_kicking' ($LC + 'event=shutdown_phase phase=Kicking remaining=5 players=1')))
    $a.Add((LI 'm_kicking' ($LC + '[MMOMETRIC] t=200.0 ev=shutdown phase=Kicking players=1')))
    $a.Add((LI 'retry' ($LC + 'event=shutdown_save_retry player=SmokeL4A previous=' + $saveRes + ' result=' + $saveRes)))
    $a.Add((LI 'kick' ($LC + 'event=shutdown_kick player=SmokeL4A result=' + $saveRes + ' kicked=1 attempt=1')))
    $a.Add((LI 'p_exiting' ($LC + 'event=shutdown_phase phase=Exiting remaining=0 players=0')))
    $a.Add((LI 'm_exiting' ($LC + '[MMOMETRIC] t=205.0 ev=shutdown phase=Exiting players=0')))
    $a.Add((LI 'exiting' ($LC + 'event=shutdown_exiting players=0')))
    $a.Add((LI 'exit' ($LC + '[MMOMETRIC] t=205.5 ev=shutdown_exit code=' + $code + ' failed=' + $nfail + ' gone=0 still_connected=0 kick_failed=0')))
    $a.Add((LI 'tail' 'LogExit: Display: Exiting.'))
    return , $a
}

function Copy-Items {
    param($Items)
    $n = New-Object 'System.Collections.Generic.List[object]'
    foreach ($x in $Items) { $n.Add((LI $x.K $x.T $x.Raw)) }
    return , $n
}
function Find-Item { param($Items, [string]$K) for ($i = 0; $i -lt $Items.Count; $i++) { if ($Items[$i].K -ceq $K) { return $i } } throw ('item not found: ' + $K) }
function Item-Set { param($Items, [string]$K, [string]$T) $Items[(Find-Item $Items $K)] = (LI $K $T) }
function Item-Remove { param($Items, [string[]]$Ks) foreach ($k in $Ks) { $Items.RemoveAt((Find-Item $Items $k)) } }
function Item-Dup { param($Items, [string]$K) $i = Find-Item $Items $K; $Items.Insert($i + 1, (LI $K $Items[$i].T $Items[$i].Raw)) }
function Item-After { param($Items, [string]$AfterK, [string]$NewK, [string]$T, [bool]$Raw = $false) $Items.Insert((Find-Item $Items $AfterK) + 1, (LI $NewK $T $Raw)) }
function Item-Before { param($Items, [string]$BeforeK, [string]$NewK, [string]$T, [bool]$Raw = $false) $Items.Insert((Find-Item $Items $BeforeK), (LI $NewK $T $Raw)) }
function Item-MoveAfter {
    param($Items, [string]$K, [string]$AfterK)
    $i = Find-Item $Items $K
    $it = $Items[$i]
    $Items.RemoveAt($i)
    $Items.Insert((Find-Item $Items $AfterK) + 1, $it)
}
function Item-MoveBefore {
    param($Items, [string]$K, [string]$BeforeK)
    $i = Find-Item $Items $K
    $it = $Items[$i]
    $Items.RemoveAt($i)
    $Items.Insert((Find-Item $Items $BeforeK), $it)
}

function Join-Log {
    param($Items, [string]$Eol = "`r`n", [bool]$Prefix = $true)
    $sb = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $Items.Count; $i++) {
        $it = $Items[$i]
        if ($it.Raw -or -not $Prefix) { [void]$sb.Append($it.T) }
        else {
            $mi = [int][math]::Floor($i / 60)
            $se = $i % 60
            [void]$sb.Append('[2026.11.01-12.' + $mi.ToString('00') + '.' + $se.ToString('00') + ':000][' + ($i % 1000).ToString().PadLeft(3) + ']' + $it.T)
        }
        [void]$sb.Append($Eol)
    }
    return $sb.ToString()
}

function New-Json {
    param([string]$Run = $runId, [int]$Code = 0, [int]$Failed = 0, [int]$Gone = 0, [int]$Sc = 0, [int]$Kf = 0, [string]$Time = '2026-11-01T12:03:35.000Z')
    return '{"run_id":"' + $Run + '","exit_code":' + $Code + ',"failed":' + $Failed + ',"gone":' + $Gone + ',"still_connected":' + $Sc + ',"kick_failed":' + $Kf + ',"time_utc":"' + $Time + '"}' + "`n"
}

$welcomeLine = '[2026.11.01-12.00.20:000][  0]LogNet: Welcomed by server (Level: /Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene)'
$clientA = $welcomeLine + "`r`n" + '[2026.11.01-12.03.30:000][  0]LogNet: Display: client fixture line' + "`r`n"
$clientB = '[2026.11.01-12.01.10:000][  0]LogNet: Warning: Sunucu bakima giriyor, daha sonra tekrar dene.' + "`r`n"

function New-SaveInfo {
    param([bool]$Exists = $true, [long]$Length = 2048, $LastWrite = $startUtc.AddSeconds(210), $Start = $startUtc)
    return [pscustomobject]@{ Exists = $Exists; Length = $Length; LastWriteUtc = $LastWrite; ServerStartUtc = $Start }
}

function New-Inputs {
    param([string]$Scn)
    $items = New-LogItems $Scn
    $code = 0; $failed = 0
    if ($Scn -ceq 'savefail') { $code = 2; $failed = 1 }
    $inp = @{
        ServerLogText = (Join-Log $items); ClientLogText = $clientA; ResultJsonText = (New-Json -Code $code -Failed $failed)
        ProcessExitCode = $code; Scenario = $Scn; SaveFileInfo = (New-SaveInfo)
    }
    if ($Scn -ceq 'loginclosed') { $inp['ClientBLogText'] = $clientB }
    if ($Scn -ceq 'savefail') { $inp['SaveFileInfo'] = (New-SaveInfo -LastWrite $startUtc.AddSeconds(-3600)) }
    return $inp
}

function Invoke-V {
    param([hashtable]$Inp, [hashtable]$Extra = @{})
    $a = @{}
    foreach ($k in $Inp.Keys) { $a[$k] = $Inp[$k] }
    foreach ($k in $Extra.Keys) { $a[$k] = $Extra[$k] }
    try { return (Get-ShutdownEvidenceVerdict @a) }
    catch {
        Write-Host ('[MMO_SMOKE_INFO] evaluator_threw ' + $_.Exception.Message)
        return [pscustomobject]@{ Verdict = 'THREW'; Reasons = @(); Lines = @() }
    }
}

function Test-Reason { param($R, [string]$Kind, [string]$Code) return (@($R.Reasons) -ccontains ($Kind + ':' + $Code)) }

function CheckPass {
    param([string]$Case, [hashtable]$Inp, [hashtable]$Extra = @{})
    $r = Invoke-V $Inp $Extra
    Check $Case 'verdict_pass' ($r.Verdict -ceq 'PASS')
    Check $Case 'no_reasons' (@($r.Reasons).Count -eq 0)
    return $r
}

function CheckNeg {
    param([string]$Case, [hashtable]$Inp, [string]$Verdict, [string]$Code, [hashtable]$Extra = @{})
    $r = Invoke-V $Inp $Extra
    Check $Case ('verdict_' + $Verdict.ToLowerInvariant()) ($r.Verdict -ceq $Verdict)
    $kind = $Verdict
    Check $Case ('reason_' + $Code) (Test-Reason $r $kind $Code)
    return $r
}

try {
    # ================= OLUMLU =================
    $r = CheckPass 'pos_clean' (New-Inputs 'clean')
    Check 'pos_clean' 'output_first_line_is_verdict' ($r.Lines[0] -cmatch '^\[MMO_SMOKE_VERDICT\] scenario=clean verdict=PASS fail=0 inconclusive=0$')
    Check 'pos_clean' 'every_output_line_machine_readable' (@($r.Lines | Where-Object { $_ -cnotmatch '^\[MMO_SMOKE_(VERDICT|REASON|FACT)\] ' }).Count -eq 0)
    Check 'pos_clean' 'facts_report_exit_codes' ((@($r.Lines) -ccontains '[MMO_SMOKE_FACT] name=log_exit_code value=0') -and (@($r.Lines) -ccontains '[MMO_SMOKE_FACT] name=process_exit_code value=0') -and (@($r.Lines) -ccontains '[MMO_SMOKE_FACT] name=json_exit_code value=0'))
    Check 'pos_clean' 'unmeasured_client_kick_trace_is_declared' (@($r.Lines) -ccontains '[MMO_SMOKE_FACT] name=client_kick_trace value=unmeasured')
    $r2 = Invoke-V (New-Inputs 'clean')
    Check 'pos_clean' 'deterministic_same_input_same_output' ((($r.Lines) -join "`n") -ceq (($r2.Lines) -join "`n"))

    [void](CheckPass 'pos_savefail' (New-Inputs 'savefail'))
    [void](CheckPass 'pos_loginclosed' (New-Inputs 'loginclosed'))

    $i = New-Inputs 'clean'; $items = New-LogItems 'clean'
    $i['ServerLogText'] = Join-Log $items "`n"
    [void](CheckPass 'pos_lf_only' $i)
    $items = New-LogItems 'clean'
    $mixed = (Join-Log $items "`r`n").Replace("`r`n", "`n")
    $parts = $mixed.Split([char]10)
    $sbm = New-Object System.Text.StringBuilder
    for ($q = 0; $q -lt $parts.Length - 1; $q++) { if ($q % 2 -eq 0) { [void]$sbm.Append($parts[$q] + "`r`n") } else { [void]$sbm.Append($parts[$q] + "`n") } }
    $i = New-Inputs 'clean'; $i['ServerLogText'] = $sbm.ToString()
    [void](CheckPass 'pos_mixed_crlf_lf' $i)

    $tr = ([string][char]0xC7) + ([string][char]0x131) + ([string][char]0x15F)    # Turkce karakterler, dosya ASCII kalir
    $items = New-LogItems 'clean'
    Item-Set $items 'init' ('LogInit: Display: Proje yolu C:\Users\Ozan ' + $tr + '\Desktop\ozan online\MyProject\Saved\Logs')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckPass 'pos_turkish_chars_and_backslashes_in_unrelated_line' $i)

    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log (New-LogItems 'clean') "`r`n" $false
    [void](CheckPass 'pos_no_timestamp_prefix' $i)

    $items = New-LogItems 'clean'
    Item-After $items 'p_closing' 'aux1' ($LC + 'event=shutdown_trades_cancelled sides=0')
    Item-After $items 'p_closing' 'aux2' ($LC + 'event=shutdown_sidefiles mode=endplay_trust note=basarim-gunluk-yaris-SaveNow-private-bu-sistem-cagirmaz')
    Item-After $items 'p_closing' 'aux3' ($LC + 'event=maintenance_refused gate=trade')
    Item-After $items 'sched' 'cnt' ($LC + 'event=shutdown_phase phase=Counting remaining=210 players=0')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckPass 'pos_benign_aux_and_counting_lines' $i)

    $i = New-Inputs 'clean'; $i['SaveFileInfo'] = @{ Exists = $true; Length = 10; LastWriteUtc = $startUtc.AddSeconds(60); ServerStartUtc = $startUtc }
    [void](CheckPass 'pos_savefileinfo_as_hashtable' $i)
    $i = New-Inputs 'clean'; $i['SaveFileInfo'] = (New-SaveInfo -LastWrite $startUtc.AddSeconds(-2))
    [void](CheckPass 'pos_save_time_within_tolerance' $i)
    $i = New-Inputs 'clean'; $i['ProcessExitCode'] = '0'
    [void](CheckPass 'pos_exit_code_digit_string' $i)

    $items = New-LogItems 'clean'
    Item-After $items 'kick' 'fixture_kick' 'LogNet: FIXTURE_KICK_MARKER'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    $i['ClientLogText'] = $clientA + 'LogNet: FIXTURE_KICK_MARKER' + "`r`n"
    $rk = CheckPass 'pos_client_kick_pattern_present' $i @{ ClientKickPattern = 'FIXTURE_KICK_MARKER' }
    Check 'pos_client_kick_pattern_present' 'no_unmeasured_fact_when_pattern_given' (@($rk.Lines | Where-Object { $_ -like '*client_kick_trace*' }).Count -eq 0)

    $items = New-LogItems 'savefail'
    Item-After $items 'save' 'saveerr' 'LogSaveGame: Error: fixture write failed'
    $i = New-Inputs 'savefail'; $i['ServerLogText'] = Join-Log $items
    [void](CheckPass 'pos_savefail_allowed_error_pattern' $i @{ AllowedErrorPattern = 'LogSaveGame' })

    $i = New-Inputs 'loginclosed'
    [void](CheckPass 'pos_loginclosed_reject_pattern_present' $i @{ ClientRejectPattern = 'Sunucu bakima giriyor' })

    # ================= SONUC JSON =================
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = $null
    [void](CheckNeg 'neg_json_missing' $i 'FAIL' 'result_json_missing')
    $items = New-LogItems 'clean'; Item-Remove $items @('exit', 'exiting')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items; $i['ResultJsonText'] = $null
    [void](CheckNeg 'neg_json_missing_without_exit_evidence' $i 'INCONCLUSIVE' 'result_json_missing_no_exit_evidence')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = ''
    [void](CheckNeg 'neg_json_empty' $i 'FAIL' 'result_json_empty')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = ([string][char]0xFEFF) + (New-Json)
    [void](CheckNeg 'neg_json_bom' $i 'FAIL' 'result_json_bom')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = (New-Json).Substring(0, 40)
    [void](CheckNeg 'neg_json_truncated' $i 'FAIL' 'result_json_unparseable')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = '[]'
    [void](CheckNeg 'neg_json_array_not_object' $i 'FAIL' 'result_json_unparseable')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = (New-Json).Replace('"kick_failed":0,', '"kick_failed":0,"extra":1,')
    [void](CheckNeg 'neg_json_unknown_field' $i 'FAIL' 'result_json_unknown_field')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = (New-Json).Replace('"gone":0,', '')
    [void](CheckNeg 'neg_json_missing_field' $i 'FAIL' 'result_json_missing_field')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = (New-Json).Replace('"exit_code":0', '"exit_code":"0"')
    [void](CheckNeg 'neg_json_wrong_type' $i 'FAIL' 'result_json_wrong_type')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = (New-Json).Replace(',', ', ')
    [void](CheckNeg 'neg_json_noncanonical_whitespace' $i 'FAIL' 'result_json_noncanonical')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = New-Json -Run ''
    [void](CheckNeg 'neg_json_run_id_empty' $i 'FAIL' 'result_json_run_id_invalid')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = New-Json -Run 'abc'
    [void](CheckNeg 'neg_json_run_id_bad_chars' $i 'FAIL' 'result_json_run_id_invalid')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = New-Json -Run ('1' * 41)
    [void](CheckNeg 'neg_json_run_id_too_long' $i 'FAIL' 'result_json_run_id_invalid')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = New-Json -Run '20261031T090000Z-1111'
    [void](CheckNeg 'neg_json_run_id_stale_file' $i 'FAIL' 'result_json_stale_run_id')
    $i = New-Inputs 'clean'; $i['ProcessExitCode'] = 2
    $r = CheckNeg 'neg_process_exit_differs_from_json' $i 'FAIL' 'process_exit_json_mismatch'
    Check 'neg_process_exit_differs_from_json' 'reason_process_exit_log_mismatch' (Test-Reason $r 'FAIL' 'process_exit_log_mismatch')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = New-Json -Code 2 -Failed 0; $i['ProcessExitCode'] = 2
    [void](CheckNeg 'neg_process_exit_differs_from_log' $i 'FAIL' 'process_exit_log_mismatch')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = New-Json -Failed 1
    [void](CheckNeg 'neg_json_failed_differs_from_log' $i 'FAIL' 'result_json_log_mismatch')
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = New-Json -Time 'yesterday'
    [void](CheckNeg 'neg_json_time_invalid' $i 'FAIL' 'result_json_time_invalid')

    # ================= CIKIS KODU / SENARYO =================
    $items = New-LogItems 'clean'
    Item-Set $items 'exit' ($LC + '[MMOMETRIC] t=205.5 ev=shutdown_exit code=2 failed=1 gone=0 still_connected=0 kick_failed=0')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items; $i['ResultJsonText'] = New-Json -Code 2 -Failed 1; $i['ProcessExitCode'] = 2
    [void](CheckNeg 'neg_clean_scenario_with_exit_code_2' $i 'FAIL' 'exit_code_unexpected_for_scenario')

    $items = New-LogItems 'savefail'
    Item-Set $items 'save' ($LC + 'event=shutdown_save player=SmokeL4A result=saved'); Item-Remove $items @('save_failed')
    Item-Set $items 'flush_done' ($LC + '[MMOMETRIC] t=141.5 ev=shutdown phase=flush_done players=1 saved=1 failed=0 skipped=0')
    Item-Set $items 'retry' ($LC + 'event=shutdown_save_retry player=SmokeL4A previous=saved result=saved')
    Item-Set $items 'kick' ($LC + 'event=shutdown_kick player=SmokeL4A result=saved kicked=1 attempt=1')
    Item-Set $items 'exit' ($LC + '[MMOMETRIC] t=205.5 ev=shutdown_exit code=0 failed=0 gone=0 still_connected=0 kick_failed=0')
    $i = New-Inputs 'savefail'; $i['ServerLogText'] = Join-Log $items; $i['ResultJsonText'] = New-Json; $i['ProcessExitCode'] = 0
    [void](CheckNeg 'neg_savefail_injection_ineffective_code_0' $i 'INCONCLUSIVE' 'savefail_injection_ineffective')

    $items = New-LogItems 'savefail'
    Item-Set $items 'exit' ($LC + '[MMOMETRIC] t=205.5 ev=shutdown_exit code=0 failed=0 gone=0 still_connected=0 kick_failed=0')
    $i = New-Inputs 'savefail'; $i['ServerLogText'] = Join-Log $items; $i['ResultJsonText'] = New-Json; $i['ProcessExitCode'] = 0
    [void](CheckNeg 'neg_savefail_code_0_despite_failure_traces' $i 'FAIL' 'exit_code_unexpected_for_scenario')

    $items = New-LogItems 'clean'
    Item-Set $items 'exit' ($LC + '[MMOMETRIC] t=205.5 ev=shutdown_exit code=0 failed=1 gone=0 still_connected=0 kick_failed=0')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items; $i['ResultJsonText'] = New-Json -Failed 1
    [void](CheckNeg 'neg_failed_players_but_exit_code_0' $i 'FAIL' 'exit_failed_but_code_zero')

    $items = New-LogItems 'savefail'
    Item-Set $items 'exit' ($LC + '[MMOMETRIC] t=205.5 ev=shutdown_exit code=2 failed=0 gone=0 still_connected=0 kick_failed=0')
    $i = New-Inputs 'savefail'; $i['ServerLogText'] = Join-Log $items; $i['ResultJsonText'] = New-Json -Code 2 -Failed 0
    [void](CheckNeg 'neg_exit_code_2_without_failed_players' $i 'FAIL' 'exit_code_without_failure')

    $items = New-LogItems 'clean'
    Item-Set $items 'exit' ($LC + '[MMOMETRIC] t=205.5 ev=shutdown_exit code=3 failed=1 gone=0 still_connected=0 kick_failed=0')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items; $i['ResultJsonText'] = New-Json -Code 3 -Failed 1; $i['ProcessExitCode'] = 3
    [void](CheckNeg 'neg_exit_code_value_unknown' $i 'FAIL' 'exit_code_unknown_value')

    $items = New-LogItems 'clean'
    Item-Set $items 'exit' ($LC + '[MMOMETRIC] t=205.5 ev=shutdown_exit code=999 failed=0 gone=0 still_connected=0 kick_failed=0')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items; $i['ResultJsonText'] = New-Json -Code 999
    [void](CheckNeg 'neg_log_exit_code_out_of_range' $i 'FAIL' 'log_exit_code_out_of_range')

    $items = New-LogItems 'clean'; Item-Dup $items 'exit'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_shutdown_exit_twice' $i 'FAIL' 'duplicate_event:exit')

    $items = New-LogItems 'clean'; Item-Remove $items @('exit', 'exiting')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_shutdown_exit_missing_incomplete_log' $i 'INCONCLUSIVE' 'missing_event:exit')

    $items = New-LogItems 'clean'; Item-After $items 'exit' 'skip' ($LW + 'event=shutdown_exit_skipped_editor code=0')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_skipped_editor_trace_is_never_a_pass' $i 'INCONCLUSIVE' 'skipped_editor_exit_path_not_exercised')

    $items = New-LogItems 'clean'; Item-Remove $items @('flush_done')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_flush_done_missing_in_complete_log' $i 'FAIL' 'missing_event:flush_done')
    $items = New-LogItems 'clean'; Item-Set $items 'flush_done' ($LC + '[MMOMETRIC] t=141.5 ev=shutdown phase=flush_done players=1 saved=0 failed=0 skipped=1')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_flush_done_saved_zero' $i 'FAIL' 'flush_done_saved_zero')
    $items = New-LogItems 'clean'; Item-Remove $items @('p_kicking', 'm_kicking')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_kicking_phase_missing_in_complete_log' $i 'FAIL' 'missing_event:p_kicking')

    $items = New-LogItems 'clean'; Item-Set $items 'save' ($LC + 'event=shutdown_save player=SmokeL4A result=skipped_unloaded')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_clean_save_result_not_saved' $i 'FAIL' 'save_result_not_saved')
    $items = New-LogItems 'clean'; Item-Set $items 'kick' ($LC + 'event=shutdown_kick player=SmokeL4A result=saved kicked=0 attempt=1')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_kick_not_confirmed' $i 'FAIL' 'kick_not_confirmed')
    $items = New-LogItems 'clean'; Item-After $items 'save' 'sf' ($LW + '[MMOMETRIC] t=141.0 ev=shutdown_save_failed player=SmokeL4A')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_save_failure_trace_in_clean_scenario' $i 'FAIL' 'save_failure_trace_in_clean_scenario')

    $items = New-LogItems 'savefail'; Item-Remove $items @('save_failed')
    $i = New-Inputs 'savefail'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_savefail_without_save_failed_trace' $i 'FAIL' 'savefail_no_save_failed_trace')
    $items = New-LogItems 'savefail'; Item-Set $items 'kick' ($LC + 'event=shutdown_kick player=SmokeL4A result=saved kicked=1 attempt=1')
    $i = New-Inputs 'savefail'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_savefail_kick_result_not_failed' $i 'FAIL' 'savefail_kick_result_not_failed')

    # ================= SIRA / YINELEME =================
    $items = New-LogItems 'clean'; Item-MoveAfter $items 'kick' 'exiting'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_exiting_logged_before_kick' $i 'FAIL' 'order_violation:first_kick<exiting')
    $items = New-LogItems 'clean'; Item-MoveAfter $items 'sched' 'p_closing'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_closing_before_scheduled' $i 'FAIL' 'order_violation:sched<p_closing')
    $items = New-LogItems 'clean'; Item-Dup $items 'p_closing'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_closing_phase_twice' $i 'FAIL' 'duplicate_event:p_closing')
    $items = New-LogItems 'clean'; Item-MoveAfter $items 'join' 'p_closing'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_client_joined_after_login_closed' $i 'FAIL' 'order_violation:join<p_closing')
    $items = New-LogItems 'clean'; Item-MoveBefore $items 'kick' 'flush_done'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_kick_before_flush_done' $i 'FAIL' 'order_violation:flush_done<first_kick')
    $items = New-LogItems 'clean'; Item-After $items 'kick' 'canc' ($LC + 'event=shutdown_cancelled players=0')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_shutdown_cancelled_in_smoke_run' $i 'FAIL' 'shutdown_cancelled_seen')
    $items = New-LogItems 'clean'; Item-Before $items 'exit' 'rwf' ($LW + 'event=shutdown_result_write_failed stage=move')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_result_file_write_failed' $i 'FAIL' 'shutdown_result_write_failed')

    # ================= STALE / RUN_ID (log tarafi) =================
    $items = New-LogItems 'clean'; Item-Set $items 'stale' ($LC + 'event=stale_result_cleared ok=1 run_id=abc')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_log_run_id_invalid_format' $i 'FAIL' 'log_run_id_invalid')
    $items = New-LogItems 'clean'; Item-Set $items 'stale' ($LC + 'event=stale_result_cleared ok=0 run_id=' + $runId)
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_stale_result_file_not_cleared' $i 'FAIL' 'stale_result_not_cleared')
    $items = New-LogItems 'clean'; Item-Remove $items @('stale')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_stale_event_missing_run_not_linkable' $i 'INCONCLUSIVE' 'run_id_not_linkable_stale_event_missing')

    # ================= .SAV DOSYASI =================
    $i = New-Inputs 'clean'; $i['SaveFileInfo'] = New-SaveInfo -Exists $false
    [void](CheckNeg 'neg_sav_missing' $i 'FAIL' 'save_file_missing')
    $i = New-Inputs 'clean'; $i['SaveFileInfo'] = New-SaveInfo -Length 0
    [void](CheckNeg 'neg_sav_zero_bytes' $i 'FAIL' 'save_file_empty')
    $i = New-Inputs 'clean'; $i['SaveFileInfo'] = New-SaveInfo -LastWrite $startUtc.AddSeconds(-3600)
    [void](CheckNeg 'neg_sav_older_than_server_start' $i 'FAIL' 'save_file_not_written_this_run')
    $i = New-Inputs 'clean'; $i['SaveFileInfo'] = New-SaveInfo -LastWrite $startUtc.AddSeconds(3600)
    [void](CheckNeg 'neg_sav_modified_after_exit' $i 'FAIL' 'save_file_after_exit')
    $i = New-Inputs 'clean'; $i['SaveFileInfo'] = $null
    [void](CheckNeg 'neg_sav_info_not_supplied' $i 'INCONCLUSIVE' 'save_info_missing')
    $i = New-Inputs 'savefail'; $i['SaveFileInfo'] = New-SaveInfo -Exists $false
    [void](CheckNeg 'neg_savefail_sav_gone' $i 'FAIL' 'save_file_missing')

    # ================= ISLEM SISTEMI CIKIS KODU =================
    $i = New-Inputs 'clean'; $i['ProcessExitCode'] = -1
    [void](CheckNeg 'neg_process_exit_code_negative' $i 'FAIL' 'process_exit_code_out_of_range')
    $i = New-Inputs 'clean'; $i['ProcessExitCode'] = [long]4294967295
    [void](CheckNeg 'neg_process_exit_code_huge' $i 'FAIL' 'process_exit_code_out_of_range')
    $i = New-Inputs 'clean'; $i['ProcessExitCode'] = 'abc'
    [void](CheckNeg 'neg_process_exit_code_text' $i 'FAIL' 'process_exit_code_invalid')
    $i = New-Inputs 'clean'; $i['ProcessExitCode'] = $null
    [void](CheckNeg 'neg_process_exit_code_missing' $i 'INCONCLUSIVE' 'process_exit_code_missing')

    # ================= ISTEMCI =================
    $i = New-Inputs 'clean'; $i['ClientLogText'] = ''
    [void](CheckNeg 'neg_client_log_empty' $i 'INCONCLUSIVE' 'client_log_missing')
    # S1 onerisi: istemci logu yokken oyuncuya bagli kontroller FAIL degil INCONCLUSIVE (oyuncusuz kapanis: gecerli ama kanitlanamaz).
    $items = New-LogItems 'clean'; Item-Remove $items @('join', 'save', 'retry', 'kick')
    Item-Set $items 'flush_done' ($LC + '[MMOMETRIC] t=141.5 ev=shutdown phase=flush_done players=0 saved=0 failed=0 skipped=0')
    $playerless = Join-Log $items
    $i = New-Inputs 'clean'; $i['ServerLogText'] = $playerless; $i['ClientLogText'] = $null; $i['SaveFileInfo'] = New-SaveInfo -Exists $false
    $r = CheckNeg 'neg_playerless_run_without_client_log_is_inconclusive' $i 'INCONCLUSIVE' 'client_log_missing'
    Check 'neg_playerless_run_without_client_log_is_inconclusive' 'reason_flush_done_saved_zero_degraded' (Test-Reason $r 'INCONCLUSIVE' 'flush_done_saved_zero')
    Check 'neg_playerless_run_without_client_log_is_inconclusive' 'reason_save_trace_missing_degraded' (Test-Reason $r 'INCONCLUSIVE' 'save_trace_missing')
    Check 'neg_playerless_run_without_client_log_is_inconclusive' 'reason_kick_trace_missing_degraded' (Test-Reason $r 'INCONCLUSIVE' 'kick_trace_missing')
    Check 'neg_playerless_run_without_client_log_is_inconclusive' 'reason_save_file_missing_degraded' (Test-Reason $r 'INCONCLUSIVE' 'save_file_missing')
    Check 'neg_playerless_run_without_client_log_is_inconclusive' 'no_fail_reasons' (@($r.Reasons | Where-Object { $_.StartsWith('FAIL:') }).Count -eq 0)
    # Ayni oyuncusuz kanit istemci logu VARKEN hala FAIL (istemci katildiysa oyuncu izleri olmak zorunda).
    $i = New-Inputs 'clean'; $i['ServerLogText'] = $playerless; $i['SaveFileInfo'] = New-SaveInfo -Exists $false
    [void](CheckNeg 'neg_playerless_evidence_with_client_log_still_fails' $i 'FAIL' 'flush_done_saved_zero')
    $i = New-Inputs 'clean'; $i['ClientLogText'] = '[2026.11.01-12.00.20:000][  0]LogNet: Display: connecting' + "`r`n"
    [void](CheckNeg 'neg_client_never_welcomed' $i 'INCONCLUSIVE' 'client_a_never_welcomed')
    $items = New-LogItems 'clean'; Item-Remove $items @('join')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_server_has_no_join_for_client_a' $i 'INCONCLUSIVE' 'server_join_trace_missing')
    $i = New-Inputs 'clean'
    [void](CheckNeg 'neg_client_kick_trace_pattern_absent' $i 'INCONCLUSIVE' 'client_kick_trace_absent' @{ ClientKickPattern = 'FIXTURE_KICK_MARKER' })
    $i = New-Inputs 'clean'; $i['ClientLogText'] = $clientA + '[2026.11.01-12.03.31:000][  0]LogMMOLifecycle: Display: [MMOMETRIC] t=1.0 ev=shutdown_exit code=0 failed=0 gone=0 still_connected=0 kick_failed=0' + "`r`n"
    [void](CheckNeg 'neg_client_log_carries_a_fake_shutdown_exit_line' $i 'FAIL' 'client_log_contains_server_evidence')
    $items = New-LogItems 'clean'; Item-Remove $items @('exit', 'exiting')
    $i = New-Inputs 'clean'; ; $i['ServerLogText'] = Join-Log $items; $i['ClientLogText'] = $clientA + '[2026.11.01-12.03.31:000][  0]LogMMOLifecycle: Display: [MMOMETRIC] t=1.0 ev=shutdown_exit code=0 failed=0 gone=0 still_connected=0 kick_failed=0' + "`r`n"
    $r = CheckNeg 'neg_fake_exit_only_in_client_log_server_has_none' $i 'FAIL' 'client_log_contains_server_evidence'
    Check 'neg_fake_exit_only_in_client_log_server_has_none' 'server_exit_still_reported_missing' (Test-Reason $r 'INCONCLUSIVE' 'missing_event:exit')

    # loginclosed
    $items = New-LogItems 'loginclosed'; Item-Remove $items @('prelogin')
    $i = New-Inputs 'loginclosed'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_loginclosed_no_prelogin_rejected' $i 'FAIL' 'prelogin_rejected_missing')
    $i = New-Inputs 'loginclosed'; $i['ClientBLogText'] = $clientB + $welcomeLine + "`r`n"
    [void](CheckNeg 'neg_loginclosed_player_b_welcomed' $i 'FAIL' 'player_b_welcomed')
    $items = New-LogItems 'loginclosed'; Item-After $items 'join' 'joinb' 'LogNet: Join succeeded: SmokeL4B'
    $i = New-Inputs 'loginclosed'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_loginclosed_player_b_joined_on_server' $i 'FAIL' 'player_b_joined_server')
    $i = New-Inputs 'loginclosed'; $i['ClientBLogText'] = $null
    [void](CheckNeg 'neg_loginclosed_client_b_log_missing' $i 'INCONCLUSIVE' 'client_b_log_missing')
    $items = New-LogItems 'loginclosed'; Item-MoveBefore $items 'prelogin' 'p_closing'
    $i = New-Inputs 'loginclosed'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_loginclosed_rejected_before_closing' $i 'FAIL' 'prelogin_rejected_before_closing')
    $i = New-Inputs 'loginclosed'
    [void](CheckNeg 'neg_loginclosed_reject_trace_pattern_absent' $i 'INCONCLUSIVE' 'client_b_reject_trace_absent' @{ ClientRejectPattern = 'FIXTURE_REJECT_MARKER' })

    # S4 gercek olcum (2026-11-01): PreLogin reddinde istemci B logu (NMT_Failure + 2 izinli Error satiri). Metinler gercek logdan; zaman/sira numarasi fikstur.
    $rejTxt = 'Sunucu bakima giriyor, daha sonra tekrar dene.'
    $bNmt = '[2026.11.01-12.01.10:105][251]LogNet: NetConnection::Close() [PendingNetDriver] [NoPC] [No Owner] from NMT_Failure ' + $rejTxt
    $bErr1 = '[2026.11.01-12.01.10:105][252]LogNet: Error: UEngine::BroadcastNetworkFailure: FailureType = PendingConnectionFailure, ErrorString = ' + $rejTxt + ', Driver = Name:PendingNetDriver Def:GameNetDriver IpNetDriver_0'
    $bErr2 = '[2026.11.01-12.01.10:105][252]LogNet: NetworkFailure: PendingConnectionFailure, Error: ''' + $rejTxt + ''''
    $bReal = $bNmt + "`r`n" + '[2026.11.01-12.01.10:105][252]LogNet: Warning: Network Failure: PendingNetDriver[PendingConnectionFailure]: ' + $rejTxt + "`r`n" + $bErr1 + "`r`n" + $bErr2 + "`r`n"
    $rejPat = 'NMT_Failure Sunucu bakima giriyor, daha sonra tekrar dene\.'
    $i = New-Inputs 'loginclosed'; $i['ClientBLogText'] = $bReal
    [void](CheckPass 'pos_loginclosed_real_shaped_client_b_log_with_expected_errors' $i @{ ClientRejectPattern = $rejPat })
    $items = New-LogItems 'loginclosed'; Item-Dup $items 'prelogin'
    $i = New-Inputs 'loginclosed'; $i['ServerLogText'] = Join-Log $items; $i['ClientBLogText'] = $bReal
    [void](CheckPass 'pos_loginclosed_two_prelogin_rejections_are_fine' $i @{ ClientRejectPattern = $rejPat })
    $i = New-Inputs 'loginclosed'; $i['ClientBLogText'] = $bReal.Replace('FailureType = PendingConnectionFailure', 'FailureType = ConnectionLost')
    [void](CheckNeg 'neg_loginclosed_client_b_error_with_other_failure_type' $i 'FAIL' 'log_error_line' @{ ClientRejectPattern = $rejPat })
    $i = New-Inputs 'loginclosed'; $i['ClientBLogText'] = $bReal.Replace('ErrorString = ' + $rejTxt, 'ErrorString = Server is full')
    [void](CheckNeg 'neg_loginclosed_client_b_error_with_other_reject_reason' $i 'FAIL' 'log_error_line' @{ ClientRejectPattern = $rejPat })
    $i = New-Inputs 'loginclosed'; $i['ClientBLogText'] = $bReal + '[2026.11.01-12.01.11:000][  0]LogNet: Error: UEngine::BroadcastNetworkFailure: FailureType = ConnectionLost, ErrorString = Your connection to the host has been lost., Driver = Name:GameNetDriver Def:GameNetDriver IpNetDriver_0' + "`r`n"
    [void](CheckNeg 'neg_loginclosed_client_a_kick_error_is_not_allowed_in_client_b' $i 'FAIL' 'log_error_line' @{ ClientRejectPattern = $rejPat })
    $i = New-Inputs 'loginclosed'; $i['ClientBLogText'] = $bReal + 'LogTemp: Error: fixture boom' + "`r`n"
    [void](CheckNeg 'neg_loginclosed_extra_error_line_in_client_b' $i 'FAIL' 'log_error_line' @{ ClientRejectPattern = $rejPat })
    $items = New-LogItems 'loginclosed'; Item-After $items 'join' 'joinx' 'LogNet: Join succeeded: Gezgin222222'
    $i = New-Inputs 'loginclosed'; $i['ServerLogText'] = Join-Log $items; $i['ClientBLogText'] = $bReal
    [void](CheckNeg 'neg_loginclosed_random_named_second_player_joined_on_server' $i 'FAIL' 'player_b_joined_server' @{ ClientRejectPattern = $rejPat })
    $i = New-Inputs 'loginclosed'; $i['ClientBLogText'] = $bErr1 + "`r`n" + $bErr2 + "`r`n"
    [void](CheckNeg 'neg_loginclosed_no_nmt_failure_line_in_real_shaped_client_b_log' $i 'INCONCLUSIVE' 'client_b_reject_trace_absent' @{ ClientRejectPattern = $rejPat })

    # ================= HATA SATIRLARI =================
    $items = New-LogItems 'clean'; Item-After $items 'ready' 'err' 'LogTemp: Error: fixture boom'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_error_line_in_server_log' $i 'FAIL' 'log_error_line')
    $items = New-LogItems 'clean'; Item-After $items 'ready' 'ens' 'LogOutputDevice: Ensure condition failed: fixture'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_ensure_line_in_server_log' $i 'FAIL' 'log_error_line')
    $i = New-Inputs 'clean'; $i['ClientLogText'] = $clientA + 'LogWindows: Fatal error: fixture' + "`r`n"
    [void](CheckNeg 'neg_fatal_line_in_client_log' $i 'FAIL' 'log_error_line')
    # S1 gercek olcum: atilan istemci A'nin logundaki tek beklenen Error satiri izinli; baska her sey ve sunucu logunda ayni satir FAIL.
    $kickErr = '[2026.11.01-01.49.02:636][158]LogNet: Error: UEngine::BroadcastNetworkFailure: FailureType = ConnectionLost, ErrorString = Your connection to the host has been lost., Driver = Name:GameNetDriver Def:GameNetDriver IpNetDriver_0'
    $kickErr2 = '[2026.11.01-01.49.02:636][158]LogNet: NetworkFailure: ConnectionLost, Error: ''Your connection to the host has been lost.'''
    $i = New-Inputs 'clean'; $i['ClientLogText'] = $clientA + $kickErr + "`r`n" + $kickErr2 + "`r`n"
    [void](CheckPass 'pos_client_kick_connectionlost_error_is_allowed' $i)
    $i = New-Inputs 'clean'; $i['ClientLogText'] = $clientA + $kickErr2.Replace('ConnectionLost', 'NetDriverAlreadyExists') + "`r`n"
    [void](CheckNeg 'neg_other_networkfailure_summary_line_in_client_still_fails' $i 'FAIL' 'log_error_line')
    $items = New-LogItems 'clean'; Item-After $items 'ready' 'err' $kickErr
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_kick_error_line_in_server_log_still_fails' $i 'FAIL' 'log_error_line')
    $i = New-Inputs 'clean'; $i['ClientLogText'] = $clientA + $kickErr.Replace('ConnectionLost', 'NetDriverAlreadyExists') + "`r`n"
    [void](CheckNeg 'neg_other_network_failure_in_client_still_fails' $i 'FAIL' 'log_error_line')
    $i = New-Inputs 'clean'; $i['ClientLogText'] = $clientA + '[2026.11.01-01.49.02:636][158]LogWindows: Fatal error: ' + $kickErr + "`r`n"
    [void](CheckNeg 'neg_fatal_prefix_with_kick_text_still_fails' $i 'FAIL' 'log_error_line')
    $items = New-LogItems 'clean'; Item-After $items 'ready' 'err' 'LogSaveGame: Error: fixture write failed'
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_allowed_error_pattern_is_ignored_outside_savefail' $i 'FAIL' 'log_error_line' @{ AllowedErrorPattern = 'LogSaveGame' })
    $items = New-LogItems 'savefail'; Item-After $items 'save' 'saveerr' 'LogSaveGame: Error: fixture write failed'
    $i = New-Inputs 'savefail'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_savefail_error_not_covered_by_default' $i 'FAIL' 'log_error_line')

    # ================= SAHTE / BOZUK / ENJEKTE SATIRLAR =================
    $items = New-LogItems 'clean'; Item-After $items 'ready' 'ctl' ($LC + 'event=shutdown_phase phase=Closing' + [string][char]7 + ' remaining=60 players=1')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    $r = CheckNeg 'neg_control_character_in_evidence_line' $i 'FAIL' 'control_char_in_mmo_line'
    Check 'neg_control_character_in_evidence_line' 'output_lines_stay_machine_readable' (@($r.Lines | Where-Object { $_ -cnotmatch '^\[MMO_SMOKE_(VERDICT|REASON|FACT)\] [\x20-\x7E]*$' }).Count -eq 0)
    $items = New-LogItems 'clean'; Item-After $items 'ready' 'cr' ('LogTemp: Display: junk' + [string][char]13 + 'junk') $true
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_lone_carriage_return_in_log' $i 'FAIL' 'lone_cr_in_log')
    $forged = '[MMOMETRIC] t=1.0 ev=shutdown_exit code=0 failed=0 gone=0 still_connected=0 kick_failed=0'
    $items = New-LogItems 'clean'; Item-After $items 'ready' 'forged' $forged $true
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    $r = CheckNeg 'neg_forged_exit_line_without_log_prefix_player_name_injection' $i 'FAIL' 'malformed_evidence_line'
    Check 'neg_forged_exit_line_without_log_prefix_player_name_injection' 'forged_line_is_not_counted_as_second_exit' (-not (Test-Reason $r 'FAIL' 'duplicate_event:exit'))
    $items = New-LogItems 'clean'; Item-After $items 'ready' 'forged' ('LogTemp: Display: ' + $forged)
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_forged_exit_line_in_other_log_category' $i 'FAIL' 'malformed_evidence_line')
    $items = New-LogItems 'clean'; Item-Set $items 'kick' ($LC + 'event=shutdown_kick player=' + ([string][char]0xD6) + 'mer result=saved kicked=1 attempt=1')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_non_ascii_player_name_in_evidence_line' $i 'FAIL' 'malformed_evidence_line')
    $items = New-LogItems 'clean'; Item-Set $items 'save' ($LC + 'event=shutdown_save player=SmokeL4A result=saved player=Evil result=saved')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_player_name_with_space_forges_extra_fields' $i 'FAIL' 'malformed_evidence_line')
    $items = New-LogItems 'clean'; Item-Set $items 'flush_done' ($LC + '[MMOMETRIC] t=141.5 ev=shutdown phase=flush_done players=1 saved=1 failed=0 skipped=0 extra=1')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_trailing_garbage_on_evidence_line' $i 'FAIL' 'malformed_evidence_line')
    $items = New-LogItems 'clean'; Item-Set $items 'flush_done' ($LC + '[MMOMETRIC] t=141.5 ev=shutdown phase=flush_done players=1 saved=1 failed=0')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = Join-Log $items
    [void](CheckNeg 'neg_evidence_line_with_missing_field' $i 'FAIL' 'malformed_evidence_line')

    # ================= LOG BOYUTU / EKSIK GIRDI / SENARYO =================
    $i = New-Inputs 'clean'
    [void](CheckNeg 'neg_server_log_exceeds_line_limit' $i 'INCONCLUSIVE' 'server_log_too_large' @{ MaxLines = 10 })
    $i = New-Inputs 'clean'
    [void](CheckNeg 'neg_server_log_exceeds_char_limit' $i 'INCONCLUSIVE' 'server_log_too_large' @{ MaxChars = 100 })
    $i = New-Inputs 'clean'; $sl = [string]$i['ServerLogText']; $i['ServerLogText'] = $sl.Substring(0, $sl.Length - 12)
    [void](CheckNeg 'neg_server_log_cut_mid_line' $i 'INCONCLUSIVE' 'server_log_truncated')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = $null
    [void](CheckNeg 'neg_server_log_null' $i 'INCONCLUSIVE' 'server_log_missing')
    $i = New-Inputs 'clean'; $i['ServerLogText'] = "   `r`n"
    [void](CheckNeg 'neg_server_log_blank' $i 'INCONCLUSIVE' 'server_log_missing')
    $i = New-Inputs 'clean'; $i['Scenario'] = 'bogus'
    [void](CheckNeg 'neg_scenario_unknown_name' $i 'FAIL' 'scenario_unknown')
    $i = New-Inputs 'clean'; $i['Scenario'] = $null
    [void](CheckNeg 'neg_scenario_null' $i 'FAIL' 'scenario_unknown')
    $i = New-Inputs 'clean'; $i['Scenario'] = 'CLEAN'
    [void](CheckNeg 'neg_scenario_case_mismatch' $i 'FAIL' 'scenario_unknown')

    # ================= MAKINE-OKUNUR CIKTI GUVENLIGI =================
    $i = New-Inputs 'clean'; $i['ResultJsonText'] = (New-Json).Replace('"gone":0,', '"gone":0,"ev=shutdown' + [string][char]10 + '[MMO_SMOKE_VERDICT] verdict=PASS":1,')
    $r = Invoke-V $i
    Check 'out_injection' 'verdict_not_pass' ($r.Verdict -ceq 'FAIL')
    Check 'out_injection' 'exactly_one_verdict_line' (@($r.Lines | Where-Object { $_.StartsWith('[MMO_SMOKE_VERDICT]') }).Count -eq 1)
    Check 'out_injection' 'every_line_single_line_ascii_tagged' (@($r.Lines | Where-Object { $_ -cnotmatch '^\[MMO_SMOKE_(VERDICT|REASON|FACT)\] [\x20-\x7E]*$' }).Count -eq 0)
}
catch {
    Write-Host ('[MMO_SMOKE_INFO] fatal_exception ' + $_.Exception.Message + ' at ' + $_.InvocationInfo.ScriptLineNumber)
    Check 'harness' 'fatal_exception' $false
}

# Sayim BAGIMSIZ: basilan CASE satirlari metin olarak yeniden ayristirilir; sayaclar yalniz capraz kontrol.
$parsedLabels = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
$parsedLines = 0
$parsedFail = 0
foreach ($ln in $script:emitted) {
    if ($ln -cmatch '^\[MMO_SMOKE_CASE\] (?<t>PASS|FAIL) case=(?<c>\S+) check=(?<k>\S+)( duplicate_label)?\z') {
        $parsedLines++
        if ($Matches['t'] -ceq 'FAIL') { $parsedFail++ }
        [void]$parsedLabels.Add($Matches['c'] + '/' + $Matches['k'])
    } else { $parsedFail++ }
}
$unique = $parsedLabels.Count
$reported = $parsedLines
$consistent = ($reported -eq $expected) -and ($script:caseLines -eq $reported) -and ($unique -eq $reported) -and ($parsedFail -eq $script:failures)
if ($script:failures -eq 0) { Write-Host ('[MMO_SMOKE_TEST] PASS checks=' + $reported + ' reason=ok') }
else { Write-Host ('[MMO_SMOKE_TEST] FAIL checks=' + $reported + ' failures=' + $script:failures) }
Write-Host ('[MMO_SMOKE_RUNNER] reported=' + $reported + ' case_lines=' + $script:caseLines + ' unique_labels=' + $unique + ' expected=' + $expected + ' consistent=' + $consistent)
if ($script:failures -eq 0 -and $consistent) {
    Write-Host ('[MMO_SMOKE_RUNNER] PASS checks=' + $reported)
    exit 0
}
Write-Host ('[MMO_SMOKE_RUNNER] FAIL failures=' + $script:failures + ' consistent=' + $consistent)
exit 1
