# Kapanis duman testi (docs/DESIGN_SHUTDOWN_SMOKE.md, dilim S0): SAF kanit degerlendiricisi.
# Dunya, surec, dosya sistemi YOK: girdi yalniz metin/sayi/nesne; cikti PASS | FAIL | INCONCLUSIVE + neden listesi.
# Kullanim: . .\ShutdownSmokeEvidence.ps1 ; $r = Get-ShutdownEvidenceVerdict -ServerLogText ... ; $r.Lines | Write-Host
# Anlam: FAIL = kanit bir seyle CELISIYOR (yanlis kod, bayat dosya, sahte/bozuk satir, sira bozuk, yinelenen olay);
#        INCONCLUSIVE = kanit EKSIK/yetersiz (yarim log, istemci katilamadi, GIsEditor atlama izi, enjeksiyon etkisiz); asla PASS sayilmaz.
#        PASS yalniz hicbir FAIL ve hicbir INCONCLUSIVE nedeni yokken doner.
# Girdi sozlesmesi (kosucu S2 bu bicimde besler; hepsi UTC):
#   ServerLogText / ClientLogText / ClientBLogText : -ABSLOG dosyalarinin HAM metni (null = dosya yok).
#   ResultJsonText : Saved/Logs/MMOShutdownResult.json HAM metni (BOM dahil; null = dosya yok).
#   ProcessExitCode: Process.ExitCode (tamsayi); surec bitmediyse null.
#   SaveFileInfo   : Exists, Length, LastWriteUtc, ServerStartUtc alanli nesne/hashtable (yalniz metadata; icerik okunmaz).
#   Scenario       : clean | savefail | loginclosed.
# Log satir bicimleri src: Source/MyProject/Private/Core/MMOServerLifecycle.cpp (LogMMOLifecycle; [MMOMETRIC] satirlari). Tek karar noktasi bu dosya.
# Bilincli sapma (belge 4.1 madde 6): .sav yazim zamani penceresinin ust siniri flush_done DEGIL, cikis zamanidir (JSON time_utc + tolerans):
#   atma aninda yeniden yazim (shutdown_save_retry) ve EndPlay kaydi flush_done'dan SONRA da dosyayi yazar; flush_done siniri dogru kosuda yanlis FAIL verirdi.
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII. Dot-source edilir; kendi basina bir sey calistirmaz.

function Get-SmokeSafeText {
    # Cikti satirina giren degiskenler (log/dosya kaynakli) makine-okunur satiri bozamasin: yalniz guvenli karakterler, uzunluk sinirli.
    param([AllowNull()][object]$Text, [int]$Max = 60)
    $s = ''
    if ($null -ne $Text) { $s = [string]$Text }
    $s = [regex]::Replace($s, '[^A-Za-z0-9_.:=,/()-]', '?')
    if ($s.Length -gt $Max) { $s = $s.Substring(0, $Max) }
    return $s
}

function Get-SmokeProp {
    param([AllowNull()][object]$Obj, [string]$Name)
    if ($null -eq $Obj) { return $null }
    if ($Obj -is [System.Collections.IDictionary]) {
        if ($Obj.Contains($Name)) { return $Obj[$Name] }
        return $null
    }
    $p = $Obj.PSObject.Properties[$Name]
    if ($null -eq $p) { return $null }
    return $p.Value
}

function ConvertTo-SmokeUtc {
    param([AllowNull()][object]$Value)
    if ($Value -is [datetime]) { return ([datetime]$Value).ToUniversalTime() }
    if ($Value -is [datetimeoffset]) { return ([datetimeoffset]$Value).UtcDateTime }
    return $null
}

function Get-ShutdownEvidenceVerdict {
    [CmdletBinding()]
    param(
        [AllowNull()][object]$ServerLogText,
        [AllowNull()][object]$ClientLogText,
        [AllowNull()][object]$ResultJsonText,
        [AllowNull()][object]$ProcessExitCode,
        [AllowNull()][object]$Scenario,
        [AllowNull()][object]$SaveFileInfo,
        [AllowNull()][object]$ClientBLogText = $null,
        [string]$PlayerA = 'SmokeL4A',
        [string]$PlayerB = 'SmokeL4B',
        [string]$ClientKickPattern = '',
        [string]$ClientRejectPattern = '',
        [string]$AllowedErrorPattern = '',
        [int]$MaxLines = 200000,
        [int]$MaxChars = 67108864,
        [double]$TimeToleranceSeconds = 5
    )
    Set-StrictMode -Version 2
    $fails = New-Object 'System.Collections.Generic.List[string]'
    $incs = New-Object 'System.Collections.Generic.List[string]'
    $notes = New-Object 'System.Collections.Generic.List[string]'
    $facts = New-Object 'System.Collections.Generic.List[string]'

    function AddFail {
        param([string]$Code, [string]$Detail = '')
        $fails.Add($Code)
        $notes.Add('[MMO_SMOKE_REASON] kind=FAIL code=' + (Get-SmokeSafeText $Code 80) + ' detail=' + (Get-SmokeSafeText $Detail))
    }
    function AddInc {
        param([string]$Code, [string]$Detail = '')
        $incs.Add($Code)
        $notes.Add('[MMO_SMOKE_REASON] kind=INCONCLUSIVE code=' + (Get-SmokeSafeText $Code 80) + ' detail=' + (Get-SmokeSafeText $Detail))
    }
    function AddFact {
        param([string]$Name, [AllowNull()][object]$Value)
        $facts.Add('[MMO_SMOKE_FACT] name=' + (Get-SmokeSafeText $Name 40) + ' value=' + (Get-SmokeSafeText $Value 60))
    }

    # ---- ortak desenler ------------------------------------------------------------------------------------------------
    $tsPre = '(?:\[\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3}\]\[[ 0-9]{1,4}\])?'
    $lcPre = '^' + $tsPre + 'LogMMOLifecycle: (?:Display|Warning|Error): '
    $tok = '[A-Za-z0-9_()\-]{1,32}'
    $mt = '\[MMOMETRIC\] t=\d{1,9}(?:\.\d+)? '
    $grammarSrc = @(
        @('ready', 'event=lifecycle_ready netmode=\d{1,3} phase=[A-Za-z?]{1,12}$'),
        @('stale', 'event=stale_result_cleared ok=(?<ok>[01]) run_id=(?<run>\S*)$'),
        @('sched', 'event=shutdown_scheduled seconds=(?<sec>\d{1,9}) reason=(?<reason>[a-z]{1,12})$'),
        @('phase', 'event=shutdown_phase phase=(?<phase>[A-Za-z?]{1,12}) remaining=-?\d{1,9} players=\d{1,6}$'),
        @('mphase', ($mt + 'ev=shutdown phase=[A-Za-z]{1,12} players=\d{1,6}$')),
        @('flush_begin', 'event=shutdown_flush_begin players=\d{1,6}$'),
        @('flush_done', ($mt + 'ev=shutdown phase=flush_done players=(?<pl>\d{1,6}) saved=(?<saved>\d{1,6}) failed=(?<failed>\d{1,6}) skipped=(?<skipped>\d{1,6})$')),
        @('save', ('event=shutdown_save player=(?<p>' + $tok + ') result=(?<r>[a-z_]{1,24})$')),
        @('save_failed', ($mt + 'ev=shutdown_save_failed player=(?<p>' + $tok + ')(?: stage=(?<stage>[a-z_]{1,24}))?$')),
        @('retry', ('event=shutdown_save_retry player=(?<p>' + $tok + ') previous=(?<prev>[a-z_]{1,24}) result=(?<r>[a-z_]{1,24})$')),
        @('rewrite_failed', ($mt + 'ev=shutdown_rewrite_failed player=(?<p>' + $tok + ')$')),
        @('kick', ('event=shutdown_kick player=(?<p>' + $tok + ') result=(?<r>[a-z_]{1,24}) kicked=(?<k>[01]) attempt=(?<a>\d{1,3})$')),
        @('exiting', 'event=shutdown_exiting players=\d{1,6}$'),
        @('exit', ($mt + 'ev=shutdown_exit code=(?<code>-?\d{1,12}) failed=(?<failed>\d{1,9}) gone=(?<gone>\d{1,9}) still_connected=(?<sc>\d{1,9}) kick_failed=(?<kf>\d{1,9})$')),
        @('skipped_editor', 'event=shutdown_exit_skipped_editor code=-?\d{1,12}$'),
        @('prelogin', 'event=prelogin_rejected phase=[A-Za-z?]{1,12}$'),
        @('cancelled', 'event=shutdown_cancelled players=\d{1,6}$'),
        @('result_write_failed', 'event=shutdown_result_write_failed stage=[a-z]{1,12}$'),
        @('aux', ('event=shutdown_(?:trades_cancelled sides=\d{1,6}|sidefiles mode=\S{1,40} note=\S{1,200}|save_gone player=' + $tok + ' previous=[a-z_]{1,24}|forced_exit code=-?\d{1,12}|exit_wait_timeout waiting=\d{1,6} max_seconds=\d{1,9}(?:\.\d+)?)$')),
        @('cmdline_rejected', 'event=shutdown_cmdline_rejected error=.{0,300}$')
    )
    $grammar = New-Object System.Collections.ArrayList
    foreach ($g in $grammarSrc) { [void]$grammar.Add(@($g[0], (New-Object System.Text.RegularExpressions.Regex(($lcPre + $g[1]), [System.Text.RegularExpressions.RegexOptions]::CultureInvariant)))) }
    $sensRx = New-Object System.Text.RegularExpressions.Regex('event=(?:shutdown|prelogin_rejected|stale_result_cleared|lifecycle_ready)|ev=shutdown')
    $ctrlRx = New-Object System.Text.RegularExpressions.Regex('[\x00-\x08\x0B\x0C\x0E-\x1F\x7F\u0085\u2028\u2029]')
    $errRx = New-Object System.Text.RegularExpressions.Regex('Error:|Fatal error|Ensure condition|Access violation|Assertion failed|Unhandled Exception')
    $joinRx = New-Object System.Text.RegularExpressions.Regex('^' + $tsPre + 'LogNet: Join succeeded: (?<n>.+?)\s*$')
    $welcomeRx = New-Object System.Text.RegularExpressions.Regex('^' + $tsPre + 'LogNet: Welcomed by server')
    $runIdRx = New-Object System.Text.RegularExpressions.Regex('^[0-9TZ-]{1,40}$')

    function Split-SmokeLog {
        # Dondurur: @{ Lines; EndsWithLf; LoneCr; TooLarge }
        param([string]$Text)
        $res = @{ Lines = @(); EndsWithLf = $false; LoneCr = $false; TooLarge = $false }
        if ($Text.Length -gt $MaxChars) { $res.TooLarge = $true; return $res }
        $norm = $Text.Replace("`r`n", "`n")
        if ($norm.Contains("`r")) { $res.LoneCr = $true }
        $res.EndsWithLf = $norm.EndsWith("`n")
        $arr = $norm.Split([char]10)
        if ($arr.Length -gt ($MaxLines + 1)) { $res.TooLarge = $true; return $res }
        $res.Lines = $arr
        return $res
    }

    function Scan-ErrorLines {
        param([string[]]$Lines, [string]$Who, [AllowNull()][object]$AllowRx)
        for ($i = 0; $i -lt $Lines.Length; $i++) {
            if ($errRx.IsMatch($Lines[$i])) {
                if ($null -ne $AllowRx -and $AllowRx.IsMatch($Lines[$i])) { continue }
                AddFail 'log_error_line' ($Who + ':line=' + ($i + 1))
                return
            }
        }
    }

    function Analyze {
        # ---- senaryo ---
        $scn = ''
        if ($Scenario -is [string]) { $scn = $Scenario }
        if (@('clean', 'savefail', 'loginclosed') -cnotcontains $scn) { AddFail 'scenario_unknown' $scn; return }
        AddFact 'scenario' $scn

        $allowRx = $null
        if ($scn -ceq 'savefail' -and $AllowedErrorPattern.Length -gt 0) {
            try { $allowRx = New-Object System.Text.RegularExpressions.Regex($AllowedErrorPattern) } catch { AddFail 'allowed_error_pattern_invalid'; return }
        }

        # ---- sunucu logu ---
        if ($null -eq $ServerLogText -or $ServerLogText -isnot [string] -or ([string]$ServerLogText).Trim().Length -eq 0) { AddInc 'server_log_missing'; return }
        $sp = Split-SmokeLog ([string]$ServerLogText)
        if ($sp.TooLarge) { AddInc 'server_log_too_large'; return }
        if ($sp.LoneCr) { AddFail 'lone_cr_in_log' }
        if (-not $sp.EndsWithLf) { AddInc 'server_log_truncated' }
        $lines = [string[]]$sp.Lines

        $ev = @{}
        $joinA = New-Object System.Collections.ArrayList
        $joinB = New-Object System.Collections.ArrayList
        for ($i = 0; $i -lt $lines.Length; $i++) {
            $line = $lines[$i]
            if ($line.Length -eq 0) { continue }
            $n = $i + 1
            $sens = $sensRx.IsMatch($line)
            $mmoLine = $sens -or $line.Contains('LogMMO') -or $line.Contains('MMOMETRIC') -or $line.Contains('event=')
            if ($mmoLine -and $ctrlRx.IsMatch($line)) { AddFail 'control_char_in_mmo_line' ('line=' + $n) }
            if ($line.Contains('Join succeeded')) {
                $jm = $joinRx.Match($line)
                if ($jm.Success) {
                    $jn = $jm.Groups['n'].Value
                    if ($jn -ceq $PlayerA) { [void]$joinA.Add($n) }
                    if ($jn -ceq $PlayerB) { [void]$joinB.Add($n) }
                }
            }
            if (-not $sens) { continue }
            $matched = $false
            foreach ($g in $grammar) {
                $m = $g[1].Match($line)
                if (-not $m.Success) { continue }
                $matched = $true
                $key = $g[0]
                if ($key -ceq 'phase') { $key = 'p_' + $m.Groups['phase'].Value.ToLowerInvariant() }
                if (-not $ev.ContainsKey($key)) { $ev[$key] = New-Object System.Collections.ArrayList }
                [void]$ev[$key].Add([pscustomobject]@{ Idx = $n; M = $m })
                break
            }
            if (-not $matched) { AddFail 'malformed_evidence_line' ('line=' + $n) }
        }
        # Hata taramasi (Error:/Fatal/Ensure/Access violation): savefail'de yalniz AllowedErrorPattern'e uyan satirlar izinlidir.
        Scan-ErrorLines $lines 'server' $allowRx

        function Cnt { param([string]$K) if ($ev.ContainsKey($K)) { return $ev[$K].Count } return 0 }
        $logComplete = ((Cnt 'exit') -ge 1) -and $sp.EndsWithLf
        function Absent {
            # Olmasi gereken kanit yok: log tamamsa celiski (FAIL), yarimsa eksik kanit (INCONCLUSIVE).
            param([string]$Code, [string]$Detail = '')
            if ($logComplete) { AddFail $Code $Detail } else { AddInc $Code $Detail }
        }

        # ---- olay sayilari: her biri tam bir kez ---
        foreach ($k in @('sched', 'p_closing', 'p_flushing', 'p_kicking', 'p_exiting', 'flush_begin', 'flush_done', 'exiting', 'exit')) {
            $n = Cnt $k
            if ($n -gt 1) { AddFail ('duplicate_event:' + $k) ('count=' + $n) }
            elseif ($n -eq 0) {
                if ($k -ceq 'exit') { AddInc 'missing_event:exit' } else { Absent ('missing_event:' + $k) }
            }
        }

        # ---- yan etki izleri ---
        if ((Cnt 'cancelled') -gt 0) { AddFail 'shutdown_cancelled_seen' }
        if ((Cnt 'result_write_failed') -gt 0) { AddFail 'shutdown_result_write_failed' }
        $skippedEditor = (Cnt 'skipped_editor') -gt 0
        if ($skippedEditor) { AddInc 'skipped_editor_exit_path_not_exercised' }

        # ---- sira ---
        $idx = @{}
        foreach ($k in @($ev.Keys)) { $idx[$k] = $ev[$k][0].Idx }
        if ($joinA.Count -gt 0) { $idx['join'] = $joinA[0] }
        if ($ev.ContainsKey('kick')) {
            $idx['first_kick'] = $ev['kick'][0].Idx
            $idx['last_kick'] = $ev['kick'][$ev['kick'].Count - 1].Idx
        }
        $pairs = @(
            @('sched', 'p_closing'), @('join', 'p_closing'), @('p_closing', 'p_flushing'), @('p_flushing', 'p_kicking'), @('p_kicking', 'p_exiting'),
            @('p_flushing', 'flush_begin'), @('flush_begin', 'flush_done'), @('flush_done', ('first_kick'), @('p_kicking', 'first_kick')),
            @('first_kick', 'exiting'), @('p_exiting', 'exiting'), @('exiting', 'exit'), @('last_kick', 'exit')
        )
        foreach ($pr in $pairs) {
            if (-not ($idx.ContainsKey($pr[0]) -and $idx.ContainsKey($pr[1]))) { continue }
            if ($idx[$pr[1]] -lt $idx[$pr[0]]) { AddFail ('order_violation:' + $pr[0] + '<' + $pr[1]) }
        }

        # ---- istemci A ---
        $clientText = $null
        if ($ClientLogText -is [string]) { $clientText = [string]$ClientLogText }
        if ($null -eq $clientText -or $clientText.Trim().Length -eq 0) { AddInc 'client_log_missing' }
        else {
            $cp = Split-SmokeLog $clientText
            if ($cp.TooLarge) { AddInc 'client_log_too_large' }
            else {
                $cl = [string[]]$cp.Lines
                if ($cp.LoneCr) { AddFail 'lone_cr_in_log' 'client' }
                $welcomed = 0
                $clientEvidence = $false
                foreach ($l in $cl) {
                    if ($welcomeRx.IsMatch($l)) { $welcomed++ }
                    if ($sensRx.IsMatch($l)) { $clientEvidence = $true }
                }
                if ($clientEvidence) { AddFail 'client_log_contains_server_evidence' }
                # S1 olcumu (2026-11-01, gercek istemci logu): sunucu atinca istemci TEK bir Error satiri yazar (kick'in beklenen sonucu); yalniz istemci A'da izinli.
                $kickErrRx = New-Object System.Text.RegularExpressions.Regex('^' + $tsPre + '(?:LogNet: Error: UEngine::BroadcastNetworkFailure: FailureType = ConnectionLost, ErrorString = Your connection to the host has been lost\.|LogNet: NetworkFailure: ConnectionLost, Error: ''Your connection to the host has been lost\.''\s*$)')
                $clientAllowRx = $kickErrRx
                if ($null -ne $allowRx) { $clientAllowRx = New-Object System.Text.RegularExpressions.Regex('(?:' + $kickErrRx.ToString() + ')|(?:' + $allowRx.ToString() + ')') }
                Scan-ErrorLines $cl 'client' $clientAllowRx
                if ($welcomed -eq 0) { AddInc 'client_a_never_welcomed' }
                if ($joinA.Count -eq 0) { AddInc 'server_join_trace_missing' $PlayerA }
                if ($ClientKickPattern.Length -gt 0) {
                    $kickSeen = $false
                    try { $kickSeen = [regex]::IsMatch($clientText, $ClientKickPattern) } catch { AddFail 'client_kick_pattern_invalid' }
                    if (-not $kickSeen) { AddInc 'client_kick_trace_absent' }
                } else { AddFact 'client_kick_trace' 'unmeasured' }
            }
        }

        # ---- istemci B (yalniz loginclosed) ---
        if ($scn -ceq 'loginclosed') {
            if ($joinB.Count -gt 0) { AddFail 'player_b_joined_server' }
            $bText = $null
            if ($ClientBLogText -is [string]) { $bText = [string]$ClientBLogText }
            if ($null -eq $bText -or $bText.Trim().Length -eq 0) { AddInc 'client_b_log_missing' }
            else {
                $bp = Split-SmokeLog $bText
                if ($bp.TooLarge) { AddInc 'client_b_log_too_large' }
                else {
                    $bl = [string[]]$bp.Lines
                    $bWelcomed = 0
                    foreach ($l in $bl) { if ($welcomeRx.IsMatch($l)) { $bWelcomed++ } if ($sensRx.IsMatch($l)) { AddFail 'client_log_contains_server_evidence' 'clientB' } }
                    Scan-ErrorLines $bl 'clientB' $allowRx
                    if ($bWelcomed -gt 0) { AddFail 'player_b_welcomed' }
                    $rej = Cnt 'prelogin'
                    if ($rej -eq 0) { AddFail 'prelogin_rejected_missing' }
                    elseif ($idx.ContainsKey('p_closing') -and $ev['prelogin'][0].Idx -lt $idx['p_closing']) { AddFail 'prelogin_rejected_before_closing' }
                    AddFact 'prelogin_rejected' $rej
                    if ($ClientRejectPattern.Length -gt 0) {
                        $rejSeen = $false
                        try { $rejSeen = [regex]::IsMatch($bText, $ClientRejectPattern) } catch { AddFail 'client_reject_pattern_invalid' }
                        if (-not $rejSeen) { AddInc 'client_b_reject_trace_absent' }
                    } else { AddFact 'client_b_reject_trace' 'unmeasured' }
                }
            }
        }

        # ---- cikis olayi (sunucu logu) ---
        $logCode = $null
        $logFailed = $null
        $logExit = $null
        if ((Cnt 'exit') -ge 1) {
            $logExit = $ev['exit'][0].M
            $c64 = [int64]0
            $f64 = [int64]0
            if (-not [int64]::TryParse($logExit.Groups['code'].Value, [ref]$c64) -or $c64 -lt 0 -or $c64 -gt 255) { AddFail 'log_exit_code_out_of_range' }
            else {
                $logCode = $c64
                if ($c64 -ne 0 -and $c64 -ne 2) { AddFail 'exit_code_unknown_value' ('code=' + $c64) }
            }
            if ([int64]::TryParse($logExit.Groups['failed'].Value, [ref]$f64)) { $logFailed = $f64 }
            AddFact 'log_exit_code' $logExit.Groups['code'].Value
            AddFact 'log_exit_failed' $logExit.Groups['failed'].Value
        }
        $flushDone = $null
        if ((Cnt 'flush_done') -ge 1) { $flushDone = $ev['flush_done'][0].M }
        $savedFlush = $null
        $failedFlush = $null
        if ($null -ne $flushDone) {
            $savedFlush = [int64]$flushDone.Groups['saved'].Value
            $failedFlush = [int64]$flushDone.Groups['failed'].Value
            AddFact 'flush_saved' $savedFlush
            AddFact 'flush_failed' $failedFlush
        }

        # A'nin satirlari
        $saveA = @(); if ($ev.ContainsKey('save')) { $saveA = @($ev['save'] | Where-Object { $_.M.Groups['p'].Value -ceq $PlayerA }) }
        $sfA = @(); if ($ev.ContainsKey('save_failed')) { $sfA = @($ev['save_failed'] | Where-Object { $_.M.Groups['p'].Value -ceq $PlayerA }) }
        $kickA = @(); if ($ev.ContainsKey('kick')) { $kickA = @($ev['kick'] | Where-Object { $_.M.Groups['p'].Value -ceq $PlayerA }) }
        $anyFailTrace = ((Cnt 'save_failed') -gt 0) -or ((Cnt 'rewrite_failed') -gt 0)

        # ---- senaryo beklentisi ---
        $expectedCode = 0
        if ($scn -ceq 'savefail') { $expectedCode = 2 }
        $injectionIneffective = $false
        if ($null -ne $logCode) {
            if ($null -ne $logFailed -and $logFailed -gt 0 -and $logCode -eq 0) { AddFail 'exit_failed_but_code_zero' }
            if ($null -ne $logFailed -and $logFailed -eq 0 -and $logCode -ne 0) { AddFail 'exit_code_without_failure' }
            if ($logCode -ne $expectedCode) {
                if ($scn -ceq 'savefail' -and $logCode -eq 0 -and $logFailed -eq 0 -and $failedFlush -eq 0 -and -not $anyFailTrace) {
                    $injectionIneffective = $true
                    AddInc 'savefail_injection_ineffective'
                } else { AddFail 'exit_code_unexpected_for_scenario' ('expected=' + $expectedCode + ' got=' + $logCode) }
            }
        }
        if ($scn -ceq 'savefail') {
            if (-not $injectionIneffective -and (Cnt 'exit') -ge 1) {
                if ($sfA.Count -eq 0) { Absent 'savefail_no_save_failed_trace' }
                if ($null -ne $failedFlush -and $failedFlush -lt 1) { AddFail 'flush_done_failed_zero' }
                if ($null -ne $logFailed -and $logFailed -lt 1) { AddFail 'exit_failed_zero_in_savefail' }
                if ($kickA.Count -eq 0) { Absent 'kick_trace_missing' $PlayerA }
                elseif (@($kickA | Where-Object { $_.M.Groups['r'].Value -ceq 'failed' }).Count -eq 0) { AddFail 'savefail_kick_result_not_failed' }
            }
        } else {
            if ($null -ne $flushDone) {
                if ($savedFlush -lt 1) { AddFail 'flush_done_saved_zero' }
                if ($failedFlush -gt 0) { AddFail 'flush_done_failed_nonzero' }
            }
            if ($anyFailTrace) { AddFail 'save_failure_trace_in_clean_scenario' }
            if ($null -ne $logExit -and $null -ne $logFailed) {
                if ([int64]$logExit.Groups['sc'].Value + [int64]$logExit.Groups['kf'].Value -gt $logFailed) { AddFail 'exit_counters_inconsistent' }
            }
            if ((Cnt 'exit') -ge 1) {
                if ($saveA.Count -eq 0) { Absent 'save_trace_missing' $PlayerA }
                elseif (@($saveA | Where-Object { $_.M.Groups['r'].Value -ceq 'saved' }).Count -eq 0) { AddFail 'save_result_not_saved' }
                if ($kickA.Count -eq 0) { Absent 'kick_trace_missing' $PlayerA }
                elseif (@($kickA | Where-Object { $_.M.Groups['k'].Value -ceq '1' }).Count -eq 0) { AddFail 'kick_not_confirmed' }
            }
        }

        # ---- stale_result_cleared / run_id ---
        $logRun = $null
        $staleCount = Cnt 'stale'
        if ($staleCount -gt 1) { AddFail 'stale_result_cleared_repeated' }
        if ($staleCount -ge 1) {
            $sm = $ev['stale'][0].M
            $logRun = $sm.Groups['run'].Value
            if ($sm.Groups['ok'].Value -cne '1') { AddFail 'stale_result_not_cleared' }
            if (-not $runIdRx.IsMatch($logRun)) { AddFail 'log_run_id_invalid'; $logRun = $null }
        } else { AddInc 'run_id_not_linkable_stale_event_missing' }
        if ($null -ne $logRun) { AddFact 'run_id' $logRun }

        # ---- sonuc JSON ---
        $jsonCode = $null
        $jsonTime = $null
        $jsonOk = $false
        if ($null -eq $ResultJsonText) {
            if ((Cnt 'exit') -ge 1) { AddFail 'result_json_missing' } else { AddInc 'result_json_missing_no_exit_evidence' }
        }
        elseif ($ResultJsonText -isnot [string]) { AddFail 'result_json_wrong_input_type' }
        else {
            $jt = [string]$ResultJsonText
            if ($jt.Length -gt 0 -and [int][char]$jt[0] -eq 0xFEFF) { AddFail 'result_json_bom' }
            elseif ($jt.Trim().Length -eq 0) { AddFail 'result_json_empty' }
            else {
                $obj = $null
                try { $obj = $jt | ConvertFrom-Json } catch { $obj = $null }
                if ($null -eq $obj -or $obj.GetType().FullName -cne 'System.Management.Automation.PSCustomObject') { AddFail 'result_json_unparseable' }
                else {
                    $want = @('run_id', 'exit_code', 'failed', 'gone', 'still_connected', 'kick_failed', 'time_utc')
                    $have = @($obj.PSObject.Properties | ForEach-Object { $_.Name })
                    $shapeOk = $true
                    foreach ($w in $want) { if ($have -cnotcontains $w) { AddFail 'result_json_missing_field' $w; $shapeOk = $false } }
                    foreach ($h in $have) { if ($want -cnotcontains $h) { AddFail 'result_json_unknown_field' $h; $shapeOk = $false } }
                    if ($shapeOk) {
                        foreach ($w in @('exit_code', 'failed', 'gone', 'still_connected', 'kick_failed')) {
                            $val = $obj.PSObject.Properties[$w].Value
                            if ($val -isnot [int] -and $val -isnot [long]) { AddFail 'result_json_wrong_type' $w; $shapeOk = $false }
                        }
                        foreach ($w in @('run_id', 'time_utc')) {
                            $val = $obj.PSObject.Properties[$w].Value
                            if ($val -isnot [string] -and $val -isnot [datetime]) { AddFail 'result_json_wrong_type' $w; $shapeOk = $false }
                        }
                    }
                    $canon = '^\{"run_id":"(?<run>[^"\\]*)","exit_code":(?<ec>-?\d{1,12}),"failed":(?<f>\d{1,9}),"gone":(?<g>\d{1,9}),"still_connected":(?<sc>\d{1,9}),"kick_failed":(?<kf>\d{1,9}),"time_utc":"(?<t>[^"\\]*)"\}(?:\r?\n)?\z'
                    $cm = [regex]::Match($jt, $canon)
                    if ($shapeOk -and -not $cm.Success) { AddFail 'result_json_noncanonical'; $shapeOk = $false }
                    if ($shapeOk) {
                        $jsonOk = $true
                        $jrun = $cm.Groups['run'].Value
                        if (-not $runIdRx.IsMatch($jrun)) { AddFail 'result_json_run_id_invalid' }
                        elseif ($null -ne $logRun) {
                            if ($jrun -cne $logRun) { AddFail 'result_json_stale_run_id' }
                        }
                        $jc = [int64]0
                        if (-not [int64]::TryParse($cm.Groups['ec'].Value, [ref]$jc) -or $jc -lt 0 -or $jc -gt 255) { AddFail 'result_json_exit_code_out_of_range'; $jsonOk = $false }
                        else {
                            $jsonCode = $jc
                            AddFact 'json_exit_code' $jc
                        }
                        if ($null -ne $logExit) {
                            if ($cm.Groups['ec'].Value -cne $logExit.Groups['code'].Value) { AddFail 'result_json_log_mismatch' 'exit_code' }
                            if ($cm.Groups['f'].Value -cne $logExit.Groups['failed'].Value) { AddFail 'result_json_log_mismatch' 'failed' }
                            if ($cm.Groups['g'].Value -cne $logExit.Groups['gone'].Value) { AddFail 'result_json_log_mismatch' 'gone' }
                            if ($cm.Groups['sc'].Value -cne $logExit.Groups['sc'].Value) { AddFail 'result_json_log_mismatch' 'still_connected' }
                            if ($cm.Groups['kf'].Value -cne $logExit.Groups['kf'].Value) { AddFail 'result_json_log_mismatch' 'kick_failed' }
                        }
                        $ts = $cm.Groups['t'].Value
                        if ([regex]::IsMatch($ts, '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,7})?Z$')) {
                            $dt = [datetime]::MinValue
                            if ([datetime]::TryParse($ts, [Globalization.CultureInfo]::InvariantCulture, ([Globalization.DateTimeStyles]::AdjustToUniversal -bor [Globalization.DateTimeStyles]::AssumeUniversal), [ref]$dt)) { $jsonTime = $dt }
                            else { AddFail 'result_json_time_invalid' }
                        } else { AddFail 'result_json_time_invalid' }
                    }
                }
            }
        }

        # ---- islem sistemi cikis kodu ---
        $pec = $null
        if ($null -eq $ProcessExitCode -or ($ProcessExitCode -is [string] -and ([string]$ProcessExitCode).Trim().Length -eq 0)) { AddInc 'process_exit_code_missing' }
        else {
            if ($ProcessExitCode -is [int] -or $ProcessExitCode -is [long] -or $ProcessExitCode -is [uint32] -or $ProcessExitCode -is [uint16] -or $ProcessExitCode -is [byte] -or $ProcessExitCode -is [int16]) { $pec = [int64]$ProcessExitCode }
            elseif ($ProcessExitCode -is [string] -and [regex]::IsMatch([string]$ProcessExitCode, '^-?\d{1,18}$')) { $pec = [int64]::Parse([string]$ProcessExitCode, [Globalization.CultureInfo]::InvariantCulture) }
            else { AddFail 'process_exit_code_invalid' }
            if ($null -ne $pec -and ($pec -lt 0 -or $pec -gt 255)) { AddFail 'process_exit_code_out_of_range' ('code=' + $pec); $pec = $null }
            if ($null -ne $pec) { AddFact 'process_exit_code' $pec }
        }
        if (-not $skippedEditor -and $null -ne $pec) {
            if ($null -ne $logCode -and $pec -ne $logCode) { AddFail 'process_exit_log_mismatch' ('proc=' + $pec + ' log=' + $logCode) }
            if ($jsonOk -and $null -ne $jsonCode -and $pec -ne $jsonCode) { AddFail 'process_exit_json_mismatch' ('proc=' + $pec + ' json=' + $jsonCode) }
        }

        # ---- kayit dosyasi (.sav) ---
        if ($null -eq $SaveFileInfo) { AddInc 'save_info_missing' }
        else {
            $exists = Get-SmokeProp $SaveFileInfo 'Exists'
            $len = Get-SmokeProp $SaveFileInfo 'Length'
            $lw = ConvertTo-SmokeUtc (Get-SmokeProp $SaveFileInfo 'LastWriteUtc')
            $st = ConvertTo-SmokeUtc (Get-SmokeProp $SaveFileInfo 'ServerStartUtc')
            if ($exists -isnot [bool] -or $exists -ne $true) { AddFail 'save_file_missing' }
            elseif (($len -isnot [int] -and $len -isnot [long]) -or $len -le 0) { AddFail 'save_file_empty' }
            elseif ($scn -cne 'savefail') {
                # savefail'de yazim BASARISIZ olur: dosya onceki kosudan kalir (zaman penceresi uygulanmaz, yalniz var ve bos degil).
                if ($null -eq $lw -or $null -eq $st) { AddInc 'save_time_info_missing' }
                else {
                    if ($lw -lt $st.AddSeconds(-$TimeToleranceSeconds)) { AddFail 'save_file_not_written_this_run' }
                    if ($null -ne $jsonTime -and $lw -gt $jsonTime.AddSeconds($TimeToleranceSeconds)) { AddFail 'save_file_after_exit' }
                }
            }
        }
    }

    Analyze

    $verdict = 'PASS'
    if ($fails.Count -gt 0) { $verdict = 'FAIL' }
    elseif ($incs.Count -gt 0) { $verdict = 'INCONCLUSIVE' }
    $scnOut = ''
    if ($Scenario -is [string]) { $scnOut = [string]$Scenario }
    $out = New-Object 'System.Collections.Generic.List[string]'
    $out.Add('[MMO_SMOKE_VERDICT] scenario=' + (Get-SmokeSafeText $scnOut 20) + ' verdict=' + $verdict + ' fail=' + $fails.Count + ' inconclusive=' + $incs.Count)
    foreach ($x in $notes) { $out.Add($x) }
    foreach ($x in $facts) { $out.Add($x) }
    $reasons = New-Object 'System.Collections.Generic.List[string]'
    foreach ($x in $fails) { $reasons.Add('FAIL:' + $x) }
    foreach ($x in $incs) { $reasons.Add('INCONCLUSIVE:' + $x) }
    return [pscustomobject]@{ Verdict = $verdict; Reasons = $reasons.ToArray(); Lines = $out.ToArray() }
}
