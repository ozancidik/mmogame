# Kapanis duman testi KOSUCUSU regresyonu (docs/DESIGN_SHUTDOWN_SMOKE.md, dilim S2/S3): Scripts/ShutdownSmokeRunnerLib.ps1 saf yardimcilari +
# Scripts/TestShutdownSmoke.ps1 kabul/erteleme/kullanim kapilari + kaynak taramasi (ad ile surec oldurme yasagi vb.).
# UE, oyun, sunucu, istemci BASLATILMAZ; gercek Saved/ okunmaz/yazilmaz. Yalniz %TEMP% altinda gecici dosya/dizin ve sahte surec nesneleri kullanilir.
# Her vaka ayri CASE satiri basar: [MMO_SMOKE_CASE] PASS|FAIL case=<vaka> check=<kontrol>. Kalip: TestShutdownSmokeEvidence.ps1 (N: bu dosya, CASE sayaci, commit mesaji).
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII. Cikis: 0 PASS, 1 FAIL.
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ShutdownSmokeEvidence.ps1')
. (Join-Path $PSScriptRoot 'ShutdownSmokeRunnerLib.ps1')

$expected = 154

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
function Throws { param([scriptblock]$Sb) try { & $Sb | Out-Null; return $false } catch { return $true } }
function SameArr { param($A, $B) return ((@($A) -join [char]1) -ceq (@($B) -join [char]1)) }

$work = Join-Path $env:TEMP ('mmo_runner_test_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)

function New-FakeProc {
    # HasExited/Id/Kill/WaitForExit taklit eden nesne; Killed sayaci kosuyu gorunur kilar.
    param([int]$Id, [bool]$Exited = $false, [bool]$KillThrows = $false)
    $o = New-Object psobject -Property @{ Id = $Id; HasExited = $Exited; Killed = 0; KillThrows = $KillThrows }
    $o | Add-Member -MemberType ScriptMethod -Name Kill -Value { if ($this.KillThrows) { throw 'kill failed' }; $this.Killed++; $this.HasExited = $true }
    $o | Add-Member -MemberType ScriptMethod -Name WaitForExit -Value { param($ms) return $true }
    return $o
}
function New-TestState {
    $s = New-SmokeState
    $s.Inventory = { @() }
    return $s
}

try {
    # ================= KOMUT SATIRI =================
    $proj = 'C:\Users\Test User\.claude\worktrees\wt\MyProject.uproject'
    $sa = Get-SmokeServerArgs -Project $proj -Map 'Summer_scene' -LogPath 'C:\Temp\x y\server.log' -Port 18123 -ShutdownAfter 240
    Check 'srv_args' 'exact_list' (SameArr $sa @('"C:\Users\Test User\.claude\worktrees\wt\MyProject.uproject"', '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene', '-game', '-server', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout', '-ABSLOG="C:\Temp\x y\server.log"', '-port=18123', '-MMOShutdownAfterSeconds=240', '-MMOMaintenanceReason=bakim'))
    Check 'srv_args' 'has_game_and_server' ((@($sa) -ccontains '-game') -and (@($sa) -ccontains '-server'))
    Check 'srv_args' 'project_and_log_quoted_for_spaces' ($sa[0].StartsWith('"') -and $sa[0].EndsWith('"') -and $sa[9].EndsWith('"'))
    Check 'srv_args' 'no_listen_or_name_in_url' (@($sa | Where-Object { $_ -match '\?listen|\?Name' }).Count -eq 0)
    Check 'srv_args_neg' 'quote_in_path_throws' (Throws { Get-SmokeServerArgs -Project 'C:\a"b\p.uproject' -Map 'M' -LogPath 'C:\l.log' -Port 18000 -ShutdownAfter 240 })
    Check 'srv_args_neg' 'newline_in_path_throws' (Throws { Get-SmokeServerArgs -Project "C:\a`nb\p.uproject" -Map 'M' -LogPath 'C:\l.log' -Port 18000 -ShutdownAfter 240 })
    Check 'srv_args_neg' 'empty_log_path_throws' (Throws { Get-SmokeServerArgs -Project $proj -Map 'M' -LogPath '' -Port 18000 -ShutdownAfter 240 })
    Check 'srv_args_neg' 'privileged_port_throws' (Throws { Get-SmokeServerArgs -Project $proj -Map 'M' -LogPath 'C:\l.log' -Port 80 -ShutdownAfter 240 })
    Check 'srv_args_neg' 'port_above_65535_throws' (Throws { Get-SmokeServerArgs -Project $proj -Map 'M' -LogPath 'C:\l.log' -Port 70000 -ShutdownAfter 240 })
    Check 'srv_args_neg' 'shutdown_below_150_throws' (Throws { Get-SmokeServerArgs -Project $proj -Map 'M' -LogPath 'C:\l.log' -Port 18000 -ShutdownAfter 149 })
    Check 'srv_args_neg' 'shutdown_above_900_throws' (Throws { Get-SmokeServerArgs -Project $proj -Map 'M' -LogPath 'C:\l.log' -Port 18000 -ShutdownAfter 901 })
    Check 'srv_args_neg' 'map_with_slash_throws' (Throws { Get-SmokeServerArgs -Project $proj -Map 'a/b' -LogPath 'C:\l.log' -Port 18000 -ShutdownAfter 240 })
    Check 'srv_args_neg' 'reason_uppercase_throws' (Throws { Get-SmokeServerArgs -Project $proj -Map 'M' -LogPath 'C:\l.log' -Port 18000 -ShutdownAfter 240 -Reason 'BAKIM' })
    $ca = Get-SmokeClientArgs -Project $proj -Port 18123 -LogPath 'C:\Temp\client.log'
    Check 'cli_args' 'exact_list' (SameArr $ca @('"C:\Users\Test User\.claude\worktrees\wt\MyProject.uproject"', '127.0.0.1:18123', '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout', '-ABSLOG="C:\Temp\client.log"'))
    Check 'cli_args' 'no_name_parameter_in_url' (@($ca | Where-Object { $_ -match 'Name=' }).Count -eq 0)
    Check 'cli_args' 'is_not_a_server' (-not (@($ca) -ccontains '-server'))
    Check 'cli_args_neg' 'bad_port_throws' (Throws { Get-SmokeClientArgs -Project $proj -Port 0 -LogPath 'C:\l.log' })

    # ================= OYUNCU ADI (Join succeeded) =================
    $ts = '[2026.11.01-01.45.30:123][  5]'
    $j = Get-SmokeJoinedPlayer ($ts + 'LogNet: Join succeeded: Gezgin273079' + "`r`n")
    Check 'join' 'timestamped_crlf_line_gives_name' ($j.Name -ceq 'Gezgin273079' -and $j.Distinct -eq 1 -and -not $j.Invalid)
    Check 'join' 'no_timestamp_prefix_ok' ((Get-SmokeJoinedPlayer "LogNet: Join succeeded: Abc_1`n").Name -ceq 'Abc_1')
    Check 'join' 'last_line_without_eol_ok' ((Get-SmokeJoinedPlayer 'LogNet: Join succeeded: Abc').Name -ceq 'Abc')
    Check 'join' 'same_name_twice_is_one_name' ((Get-SmokeJoinedPlayer ("LogNet: Join succeeded: Abc`nLogNet: Join succeeded: Abc`n")).Name -ceq 'Abc')
    $j2 = Get-SmokeJoinedPlayer ("LogNet: Join succeeded: Abc`nLogNet: Join succeeded: Xyz`n")
    Check 'join_neg' 'two_distinct_names_is_ambiguous' ($null -eq $j2.Name -and $j2.Distinct -eq 2)
    $j3 = Get-SmokeJoinedPlayer "LogNet: Join succeeded: Evil result=saved`n"
    Check 'join_neg' 'name_with_space_is_invalid' ($null -eq $j3.Name -and $j3.Invalid)
    $j4 = Get-SmokeJoinedPlayer ('LogNet: Join succeeded: ' + [string][char]0xD6 + "mer`n")
    Check 'join_neg' 'non_ascii_name_is_invalid' ($null -eq $j4.Name -and $j4.Invalid)
    Check 'join_neg' 'name_longer_than_32_is_invalid' ((Get-SmokeJoinedPlayer ('LogNet: Join succeeded: ' + ('a' * 33) + "`n")).Invalid)
    Check 'join_neg' 'other_log_category_is_not_a_join' ($null -eq (Get-SmokeJoinedPlayer "LogTemp: Join succeeded: Abc`n").Name)
    Check 'join_neg' 'no_join_line_gives_null' ($null -eq (Get-SmokeJoinedPlayer "LogNet: Display: hello`n").Name)
    Check 'join_neg' 'null_and_empty_input_give_null' (($null -eq (Get-SmokeJoinedPlayer $null).Name) -and ($null -eq (Get-SmokeJoinedPlayer '').Name))

    # ================= SUNUCU HAZIR / KAPANIS =================
    $rdy = $ts + 'LogMMOLifecycle: Display: event=lifecycle_ready netmode=1 phase=Idle' + "`r`n"
    $r1 = Test-SmokeServerReady $rdy
    Check 'ready' 'dedicated_netmode_1' ($r1.Ready -and $r1.NetMode -eq 1)
    $r2 = Test-SmokeServerReady ($rdy.Replace('netmode=1', 'netmode=2'))
    Check 'ready' 'listen_netmode_2_reported_not_1' ($r2.Ready -and $r2.NetMode -eq 2)
    Check 'ready_neg' 'absent_is_not_ready' (-not (Test-SmokeServerReady "LogNet: Display: hello`n").Ready)
    Check 'ready_neg' 'forged_in_other_category_is_not_ready' (-not (Test-SmokeServerReady "LogTemp: Display: event=lifecycle_ready netmode=1 phase=Idle`n").Ready)
    Check 'ready_neg' 'null_is_not_ready' (-not (Test-SmokeServerReady $null).Ready)
    Check 'closing' 'seen_when_phase_closing_logged' (Test-SmokeClosingSeen ($ts + 'LogMMOLifecycle: Display: event=shutdown_phase phase=Closing remaining=60 players=1' + "`n"))
    Check 'closing_neg' 'not_seen_in_counting_only' (-not (Test-SmokeClosingSeen ($ts + 'LogMMOLifecycle: Display: event=shutdown_phase phase=Counting remaining=210 players=0' + "`n")))

    # ================= ERTELEME (baska UE/UBT) =================
    function Row { param([string]$Name, [string]$Cmd = '') return [pscustomobject]@{ ProcessId = 1; Name = $Name; CommandLine = $Cmd } }
    Check 'busy' 'unreal_editor_cmd_is_busy' (@(Get-SmokeBusyProcesses @((Row 'UnrealEditor-Cmd.exe'))).Count -eq 1)
    Check 'busy' 'unreal_editor_is_busy' (@(Get-SmokeBusyProcesses @((Row 'UnrealEditor.exe'))).Count -eq 1)
    Check 'busy' 'unreal_build_tool_is_busy' (@(Get-SmokeBusyProcesses @((Row 'UnrealBuildTool.exe'))).Count -eq 1)
    Check 'busy' 'msbuild_is_busy' (@(Get-SmokeBusyProcesses @((Row 'MSBuild.exe'))).Count -eq 1)
    Check 'busy' 'live_coding_console_is_busy' (@(Get-SmokeBusyProcesses @((Row 'LiveCodingConsole.exe'))).Count -eq 1)
    Check 'busy' 'dotnet_running_ubt_is_busy' (@(Get-SmokeBusyProcesses @((Row 'dotnet.exe' 'dotnet UnrealBuildTool.dll -Mode=Build'))).Count -eq 1)
    Check 'busy' 'dotnet_with_unknown_command_line_is_busy' (@(Get-SmokeBusyProcesses @((Row 'dotnet.exe' ''))).Count -eq 1)
    Check 'busy' 'dotnet_unrelated_is_not_busy' (@(Get-SmokeBusyProcesses @((Row 'dotnet.exe' 'dotnet tool run something'))).Count -eq 0)
    Check 'busy' 'ordinary_processes_are_not_busy' (@(Get-SmokeBusyProcesses @((Row 'notepad.exe'), (Row 'powershell.exe' '-File x.ps1'), (Row 'bash.exe'))).Count -eq 0)
    Check 'busy' 'null_and_empty_inventory_are_not_busy' ((@(Get-SmokeBusyProcesses $null).Count -eq 0) -and (@(Get-SmokeBusyProcesses @()).Count -eq 0))
    $mixed = @((Row 'notepad.exe'), (Row 'UnrealEditor-Cmd.exe'), (Row 'MSBuild.exe'))
    Check 'busy' 'mixed_inventory_reports_only_busy_rows' (@(Get-SmokeBusyProcesses $mixed).Count -eq 2)

    # ================= TORUN AGACI =================
    $inv = @(
        [pscustomobject]@{ ProcessId = 10; ParentProcessId = 1 }, [pscustomobject]@{ ProcessId = 11; ParentProcessId = 10 },
        [pscustomobject]@{ ProcessId = 12; ParentProcessId = 11 }, [pscustomobject]@{ ProcessId = 20; ParentProcessId = 2 },
        [pscustomobject]@{ ProcessId = 21; ParentProcessId = 20 }
    )
    Check 'descendants' 'chain_of_owned_root' (SameArr (@(Get-SmokeDescendantPids $inv 10) | Sort-Object) @(11, 12))
    Check 'descendants' 'unrelated_tree_not_included' (-not (@(Get-SmokeDescendantPids $inv 10) -contains 20))
    Check 'descendants' 'root_itself_excluded' (-not (@(Get-SmokeDescendantPids $inv 10) -contains 10))
    Check 'descendants' 'leaf_has_none' (@(Get-SmokeDescendantPids $inv 12).Count -eq 0)
    $cyc = @([pscustomobject]@{ ProcessId = 10; ParentProcessId = 11 }, [pscustomobject]@{ ProcessId = 11; ParentProcessId = 10 })
    Check 'descendants' 'cycle_terminates' (@(Get-SmokeDescendantPids $cyc 10).Count -ge 0)
    Check 'descendants' 'empty_inventory_gives_none' ((@(Get-SmokeDescendantPids @() 10).Count -eq 0) -and (@(Get-SmokeDescendantPids $null 10).Count -eq 0))

    # ================= SURE TAVANI =================
    Check 'time' 'ok_below_caps' ((Get-SmokeTimeDecision 10 420 5 120) -ceq 'ok')
    Check 'time' 'hard_cap_at_equal' ((Get-SmokeTimeDecision 420 420) -ceq 'hard_cap')
    Check 'time' 'hard_cap_above' ((Get-SmokeTimeDecision 500.5 420) -ceq 'hard_cap')
    Check 'time' 'phase_timeout' ((Get-SmokeTimeDecision 100 420 120 120) -ceq 'phase_timeout')
    Check 'time' 'hard_cap_wins_over_phase' ((Get-SmokeTimeDecision 500 420 999 120) -ceq 'hard_cap')
    Check 'time' 'no_phase_limit_means_only_hard_cap' ((Get-SmokeTimeDecision 100 420 9999 0) -ceq 'ok')
    Check 'time_neg' 'null_input_fails_closed' ((Get-SmokeTimeDecision $null 420) -ceq 'hard_cap')
    Check 'time_neg' 'negative_input_fails_closed' ((Get-SmokeTimeDecision -1 420) -ceq 'hard_cap')
    Check 'time_neg' 'nan_input_fails_closed' ((Get-SmokeTimeDecision ([double]::NaN) 420) -ceq 'hard_cap')
    Check 'time_neg' 'string_input_fails_closed' ((Get-SmokeTimeDecision '10' 420) -ceq 'hard_cap')

    # ================= CIKIS KODU / NIHAI VERDICT =================
    Check 'exitcode' 'pass_is_0' ((Get-SmokeVerdictExitCode 'PASS') -eq 0)
    Check 'exitcode' 'fail_is_1' ((Get-SmokeVerdictExitCode 'FAIL') -eq 1)
    Check 'exitcode' 'inconclusive_is_4' ((Get-SmokeVerdictExitCode 'INCONCLUSIVE') -eq 4)
    Check 'exitcode_neg' 'unknown_is_fail_closed_1' ((Get-SmokeVerdictExitCode 'MAYBE') -eq 1)
    Check 'exitcode_neg' 'null_is_fail_closed_1' ((Get-SmokeVerdictExitCode $null) -eq 1)
    Check 'exitcode_neg' 'lowercase_pass_is_not_pass' ((Get-SmokeVerdictExitCode 'pass') -eq 1)
    Check 'final' 'pass_plus_nothing_is_pass' ((Resolve-SmokeFinalVerdict 'PASS') -ceq 'PASS')
    Check 'final' 'fail_evidence_is_fail' ((Resolve-SmokeFinalVerdict 'FAIL') -ceq 'FAIL')
    Check 'final' 'inconclusive_evidence_is_inconclusive' ((Resolve-SmokeFinalVerdict 'INCONCLUSIVE') -ceq 'INCONCLUSIVE')
    Check 'final_neg' 'runner_fail_beats_evidence_pass' ((Resolve-SmokeFinalVerdict 'PASS' @('hard_cap_exceeded')) -ceq 'FAIL')
    Check 'final_neg' 'runner_inconclusive_blocks_pass' ((Resolve-SmokeFinalVerdict 'PASS' @() @('client_never_joined')) -ceq 'INCONCLUSIVE')
    Check 'final_neg' 'fail_beats_runner_inconclusive' ((Resolve-SmokeFinalVerdict 'FAIL' @() @('client_never_joined')) -ceq 'FAIL')
    Check 'final_neg' 'runner_fail_beats_runner_inconclusive' ((Resolve-SmokeFinalVerdict $null @('x') @('y')) -ceq 'FAIL')
    Check 'final_neg' 'missing_evidence_is_fail_closed' ((Resolve-SmokeFinalVerdict $null) -ceq 'FAIL')
    Check 'final_neg' 'unknown_evidence_is_fail_closed' ((Resolve-SmokeFinalVerdict 'MAYBE') -ceq 'FAIL')

    # ================= YOL GUVENLIGI =================
    Check 'path' 'child_is_under' (Test-SmokePathUnder 'C:\Repo\Saved' 'C:\Repo\Saved\SaveGames\a.sav')
    Check 'path' 'forward_slashes_and_case_ignored' (Test-SmokePathUnder 'C:\Repo\Saved' 'c:/repo/saved/SaveGames/a.sav')
    Check 'path_neg' 'root_itself_is_not_under' (-not (Test-SmokePathUnder 'C:\Repo\Saved' 'C:\Repo\Saved'))
    Check 'path_neg' 'sibling_with_same_prefix_is_not_under' (-not (Test-SmokePathUnder 'C:\Repo\Saved' 'C:\Repo\Saved2\a.sav'))
    Check 'path_neg' 'dotdot_escape_is_not_under' (-not (Test-SmokePathUnder 'C:\Repo\Saved' 'C:\Repo\Saved\..\Config\a.ini'))
    Check 'path_neg' 'empty_inputs_are_not_under' ((-not (Test-SmokePathUnder '' 'C:\a')) -and (-not (Test-SmokePathUnder 'C:\a' '')))
    $mainLike = Join-Path $work 'main'; [void][IO.Directory]::CreateDirectory((Join-Path $mainLike '.git'))
    $wtLike = Join-Path $work 'wt'; [void][IO.Directory]::CreateDirectory($wtLike); [IO.File]::WriteAllText((Join-Path $wtLike '.git'), 'gitdir: C:/nowhere')
    $bare = Join-Path $work 'bare'; [void][IO.Directory]::CreateDirectory($bare)
    Check 'checkout' 'git_directory_is_main_checkout' (Test-SmokeIsMainCheckout $mainLike)
    Check 'checkout' 'git_file_is_isolated_worktree' (-not (Test-SmokeIsMainCheckout $wtLike))
    Check 'checkout_neg' 'no_git_at_all_counts_as_main_fail_closed' (Test-SmokeIsMainCheckout $bare)
    $target = Join-Path $work 'target'; [void][IO.Directory]::CreateDirectory($target)
    $junc = Join-Path $work 'junc'
    $juncMade = $false
    try { New-Item -ItemType Junction -Path $junc -Target $target | Out-Null; $juncMade = $true } catch { }
    Check 'reparse' 'junction_detected' ($juncMade -and (Test-SmokeReparse $junc))
    if ($juncMade) { [IO.Directory]::Delete($junc) }
    Check 'reparse_neg' 'plain_directory_not_reparse' (-not (Test-SmokeReparse $target))
    Check 'reparse_neg' 'missing_path_not_reparse' (-not (Test-SmokeReparse (Join-Path $work 'nope')))

    # ================= OLUSAN DOSYALAR =================
    $before = @('C:\R\Saved\SaveGames\MMOSave_Old.sav', 'C:\R\Saved\Logs\MMOShutdownResult.json')
    $after = $before + @('C:\R\Saved\SaveGames\MMOSave_Gezgin1.sav', 'C:\R\Saved\MMODaily\Gezgin1.json', 'C:\R\Saved\SaveGames\MMOSave_Other.sav', 'C:\R\Saved\Gezgin1\other.txt')
    $cf = @(Get-SmokeCreatedFiles -Before $before -After $after -PlayerName 'Gezgin1')
    Check 'created' 'new_files_with_player_name_only' ($cf.Count -eq 2 -and ($cf -contains 'C:\R\Saved\SaveGames\MMOSave_Gezgin1.sav') -and ($cf -contains 'C:\R\Saved\MMODaily\Gezgin1.json'))
    Check 'created_neg' 'preexisting_never_included' (-not ($cf -contains 'C:\R\Saved\SaveGames\MMOSave_Old.sav'))
    Check 'created_neg' 'new_file_of_someone_else_not_included' (-not ($cf -contains 'C:\R\Saved\SaveGames\MMOSave_Other.sav'))
    Check 'created_neg' 'player_name_only_in_directory_not_included' (-not ($cf -contains 'C:\R\Saved\Gezgin1\other.txt'))
    Check 'created_neg' 'invalid_player_name_deletes_nothing' ((@(Get-SmokeCreatedFiles -Before $before -After $after -PlayerName 'bad name').Count -eq 0) -and (@(Get-SmokeCreatedFiles -Before $before -After $after -PlayerName '').Count -eq 0))
    Check 'created' 'case_insensitive_before_set' (@(Get-SmokeCreatedFiles -Before @('c:\r\saved\savegames\mmosave_gezgin1.sav') -After @('C:\R\Saved\SaveGames\MMOSave_Gezgin1.sav') -PlayerName 'Gezgin1').Count -eq 0)

    # ================= OKUMA =================
    $logf = Join-Path $work 'live.log'
    $fsw = New-Object IO.FileStream($logf, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
    $bytes = (New-Object Text.UTF8Encoding($true)).GetPreamble() + [Text.Encoding]::UTF8.GetBytes('hello')
    $fsw.Write($bytes, 0, $bytes.Length); $fsw.Flush()
    $shared = $null
    try { $shared = Read-SmokeTextShared $logf } catch { $shared = 'THREW' }
    Check 'read' 'shared_read_while_writer_has_file_open' ($shared -ceq 'hello')
    $raw = Read-SmokeRawText $logf
    Check 'read' 'raw_read_keeps_bom' ($raw.Length -eq 6 -and [int][char]$raw[0] -eq 0xFEFF)
    $fsw.Dispose()
    Check 'read_neg' 'missing_file_gives_null' (($null -eq (Read-SmokeTextShared (Join-Path $work 'no.log'))) -and ($null -eq (Read-SmokeRawText (Join-Path $work 'no.json'))))

    # ================= TEMIZLIK: surecler =================
    $st = New-TestState
    $owned = New-FakeProc 101
    $foreign = New-FakeProc 202
    Add-SmokeOwnedProcess $st $owned
    Invoke-SmokeCleanup $st
    Check 'cleanup_proc' 'owned_process_killed' ($owned.Killed -eq 1)
    Check 'cleanup_proc' 'unregistered_foreign_process_untouched' ($foreign.Killed -eq 0)
    Check 'cleanup_proc' 'done_flag_set' ($st.Done -eq $true)
    Check 'cleanup_proc' 'no_problems_when_clean' ($st.Problems.Count -eq 0)
    $st = New-TestState
    $gone = New-FakeProc 103 $true
    Add-SmokeOwnedProcess $st $gone
    Invoke-SmokeCleanup $st
    Check 'cleanup_proc' 'already_exited_process_not_killed_again' ($gone.Killed -eq 0)
    $st = New-TestState
    $bad = New-FakeProc 104 $false $true
    $ok2 = New-FakeProc 105
    Add-SmokeOwnedProcess $st $bad; Add-SmokeOwnedProcess $st $ok2
    Invoke-SmokeCleanup $st
    Check 'cleanup_proc_neg' 'kill_error_is_recorded' (@($st.Problems | Where-Object { $_ -like 'owned_process_kill_error:104' }).Count -eq 1)
    Check 'cleanup_proc_neg' 'kill_error_does_not_stop_the_other_process' ($ok2.Killed -eq 1)
    Check 'cleanup_proc_neg' 'done_even_after_kill_error' ($st.Done -eq $true)
    $st = New-TestState
    $st.Inventory = { @([pscustomobject]@{ ProcessId = 7; ParentProcessId = 301 }, [pscustomobject]@{ ProcessId = 9; ParentProcessId = 7 }) }
    $parent = New-FakeProc 301
    Add-SmokeOwnedProcess $st $parent
    Invoke-SmokeCleanup $st
    Check 'cleanup_proc' 'child_kill_failure_for_missing_pid_is_not_a_problem' ($parent.Killed -eq 1 -and $st.Problems.Count -eq 0)

    # ================= TEMIZLIK: salt-okunur geri alma =================
    $ro = Join-Path $work 'player.sav'
    [IO.File]::WriteAllBytes($ro, [byte[]](1..16))
    $st = New-TestState
    Set-SmokeReadOnly $st $ro
    Check 'readonly' 'injection_sets_readonly_flag' ((([IO.File]::GetAttributes($ro)) -band [IO.FileAttributes]::ReadOnly) -ne 0)
    $writeBlocked = Throws { [IO.File]::WriteAllBytes($ro, [byte[]](2, 2)) }
    Check 'readonly' 'write_is_really_blocked_while_injected' $writeBlocked
    Invoke-SmokeCleanup $st
    Check 'readonly' 'cleanup_clears_readonly_flag' ((([IO.File]::GetAttributes($ro)) -band [IO.FileAttributes]::ReadOnly) -eq 0)
    Check 'readonly' 'file_writable_again_after_cleanup' (-not (Throws { [IO.File]::WriteAllBytes($ro, [byte[]](3, 3, 3)) }))
    Check 'readonly' 'no_restore_problem_recorded' ($st.Problems.Count -eq 0)
    # Govde HATA atsa da geri alinir (finally); istisna yukari cikar.
    $ro2 = Join-Path $work 'player2.sav'
    [IO.File]::WriteAllBytes($ro2, [byte[]](1..16))
    $st = New-TestState
    $threw = $false
    try { Invoke-SmokeGuarded -State $st -Body { Set-SmokeReadOnly $st $ro2; throw 'boom inside body' } } catch { $threw = ($_.Exception.Message -eq 'boom inside body') }
    Check 'guarded' 'body_exception_propagates' $threw
    Check 'guarded' 'readonly_restored_when_body_throws' ((([IO.File]::GetAttributes($ro2)) -band [IO.FileAttributes]::ReadOnly) -eq 0)
    Check 'guarded' 'cleanup_done_flag_after_throw' ($st.Done -eq $true)
    $st = New-TestState
    $p9 = New-FakeProc 909
    Invoke-SmokeGuarded -State $st -Body { Add-SmokeOwnedProcess $st $p9; return }
    Check 'guarded' 'owned_process_killed_on_early_return' ($p9.Killed -eq 1)
    # Salt-okunur kaydi: bayrak koyma ONCE kaydedilir (dosya yoksa Set atar ama yol listede kalir).
    $st = New-TestState
    $missing = Join-Path $work 'missing.sav'
    [void](Throws { Set-SmokeReadOnly $st $missing })
    Check 'readonly_neg' 'path_recorded_before_flag_is_set' (@($st.ReadOnly).Count -eq 1)
    Invoke-SmokeCleanup $st
    Check 'readonly_neg' 'restoring_a_missing_file_is_not_a_problem' ($st.Problems.Count -eq 0)

    # ================= TEMIZLIK: sira (geri alma -> dosya silme) =================
    $ro3 = Join-Path $work 'player3.sav'
    [IO.File]::WriteAllBytes($ro3, [byte[]](1..16))
    $st = New-TestState
    Set-SmokeReadOnly $st $ro3
    $script:orderProbe = 'unset'
    $st.AfterRestore = { $script:orderProbe = if (((([IO.File]::GetAttributes($ro3)) -band [IO.FileAttributes]::ReadOnly) -eq 0)) { 'restored_first' } else { 'readonly_still_set' }; Remove-Item -LiteralPath $ro3 -Force }
    Invoke-SmokeCleanup $st
    Check 'order' 'after_restore_hook_runs_after_readonly_cleared' ($script:orderProbe -ceq 'restored_first')
    Check 'order' 'readonly_file_could_be_deleted_by_the_hook' (-not (Test-Path -LiteralPath $ro3))
    $st = New-TestState
    $st.AfterRestore = { throw 'hook failed' }
    Invoke-SmokeCleanup $st
    Check 'order_neg' 'hook_exception_recorded_not_thrown' (@($st.Problems | Where-Object { $_ -ceq 'after_restore_error' }).Count -eq 1 -and $st.Done)

    # ================= KOSUCU BETIGI: kapilar (cocuk powershell; UE BASLAMAZ) =================
    $runner = Join-Path $PSScriptRoot 'TestShutdownSmoke.ps1'
    function Run-Runner {
        param([string[]]$ExtraArgs)
        $ErrorActionPreference = 'Continue'
        $o = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner @ExtraArgs 2>&1 | Out-String
        $code = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        return [pscustomobject]@{ Code = $code; Out = $o }
    }
    $fakeExe = Join-Path $work 'fake.exe'; [IO.File]::WriteAllText($fakeExe, 'x')
    $rootOk = Join-Path $work 'root_main'
    [void][IO.Directory]::CreateDirectory((Join-Path $rootOk 'Binaries\Win64')); [void][IO.Directory]::CreateDirectory((Join-Path $rootOk '.git'))
    [IO.File]::WriteAllText((Join-Path $rootOk 'MyProject.uproject'), '{}'); [IO.File]::WriteAllText((Join-Path $rootOk 'Binaries\Win64\UnrealEditor-MyProject.dll'), 'x')
    $rootNoBin = Join-Path $work 'root_nobin'
    [void][IO.Directory]::CreateDirectory((Join-Path $rootNoBin '.git')); [IO.File]::WriteAllText((Join-Path $rootNoBin 'MyProject.uproject'), '{}')
    $rootNoProj = Join-Path $work 'root_noproj'; [void][IO.Directory]::CreateDirectory($rootNoProj)
    $rootWt = Join-Path $work 'root_wt'
    [void][IO.Directory]::CreateDirectory((Join-Path $rootWt 'Binaries\Win64'))
    [IO.File]::WriteAllText((Join-Path $rootWt '.git'), 'gitdir: C:/nowhere/not-a-repo')
    [IO.File]::WriteAllText((Join-Path $rootWt 'MyProject.uproject'), '{}'); [IO.File]::WriteAllText((Join-Path $rootWt 'Binaries\Win64\UnrealEditor-MyProject.dll'), 'x')

    $l4Before = @(Get-ChildItem -LiteralPath $env:TEMP -Directory -Filter 'mmo_l4_*' -ErrorAction SilentlyContinue).Count
    $g = Run-Runner @('-Scenario', 'loginclosed', '-RepoRoot', $rootOk, '-ExePath', $fakeExe)
    Check 'gate' 'loginclosed_is_usage_error_2' ($g.Code -eq 2 -and $g.Out -like '*usage Scenario*')
    $g = Run-Runner @('-RepoRoot', $rootOk, '-ExePath', $fakeExe)
    Check 'gate' 'missing_scenario_is_usage_error_2' ($g.Code -eq 2)
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $rootOk, '-ExePath', $fakeExe, '-ShutdownAfter', '60')
    Check 'gate' 'shutdown_after_too_small_is_usage_error_2' ($g.Code -eq 2 -and $g.Out -like '*ShutdownAfter*')
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $rootOk, '-ExePath', $fakeExe, '-ShutdownAfter', '240', '-HardCapSeconds', '250')
    Check 'gate' 'hard_cap_too_small_is_usage_error_2' ($g.Code -eq 2 -and $g.Out -like '*HardCapSeconds*')
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', (Join-Path $work 'does_not_exist'), '-ExePath', $fakeExe)
    Check 'gate' 'missing_repo_root_is_usage_error_2' ($g.Code -eq 2)
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $rootNoProj, '-ExePath', $fakeExe)
    Check 'gate' 'repo_without_uproject_is_usage_error_2' ($g.Code -eq 2 -and $g.Out -like '*MyProject.uproject*')
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $rootOk, '-ExePath', (Join-Path $work 'no_such.exe'))
    Check 'gate' 'missing_unreal_editor_cmd_defers_3' ($g.Code -eq 3 -and $g.Out -like '*unreal_editor_cmd_missing*')
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $rootNoBin, '-ExePath', $fakeExe)
    Check 'gate' 'unbuilt_binaries_defer_3_and_runner_never_builds' ($g.Code -eq 3 -and $g.Out -like '*binaries_not_built*')
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $rootOk, '-ExePath', $fakeExe)
    Check 'gate' 'main_checkout_is_refused_defer_3' ($g.Code -eq 3 -and $g.Out -like '*main_checkout_refused*')
    Check 'gate' 'deferral_says_nothing_started' ($g.Out -like '*nothing started*')
    Check 'gate' 'deferral_never_prints_a_pass_or_result_pass' ($g.Out -notlike '*verdict=PASS*')
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $rootWt, '-ExePath', $fakeExe)
    Check 'gate' 'unresolvable_git_dir_defers_3' ($g.Code -eq 3 -and $g.Out -like '*git_common_dir_unresolved*')
    Check 'gate' 'gates_create_no_log_directory_before_deferring' (@(Get-ChildItem -LiteralPath $env:TEMP -Directory -Filter 'mmo_l4_*' -ErrorAction SilentlyContinue).Count -eq $l4Before)

    # ================= KAYNAK TARAMASI (ad ile surec oldurme, kod degistirme, dogrudan silme yasagi) =================
    $mainSrc = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'TestShutdownSmoke.ps1'))
    $libSrc = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'ShutdownSmokeRunnerLib.ps1'))
    $bothSrc = $mainSrc + "`n" + $libSrc
    $codeOnly = ((($bothSrc -split "`n") | Where-Object { -not $_.TrimStart().StartsWith('#') }) -join "`n")
    Check 'lint' 'no_stop_process' ($codeOnly -notmatch '(?i)\bStop-Process\b')
    Check 'lint' 'no_taskkill' ($codeOnly -notmatch '(?i)\btaskkill\b')
    Check 'lint' 'no_process_lookup_by_name_for_killing' ($codeOnly -notmatch '(?i)\bGet-Process\b' -and $codeOnly -notmatch '(?i)GetProcessesByName')
    Check 'lint' 'main_never_calls_kill_directly' ($mainSrc -notmatch '\.Kill\(')
    Check 'lint' 'main_never_sets_attributes_directly' ($mainSrc -notmatch 'SetAttributes')
    Check 'lint' 'lib_has_no_remove_item' ($libSrc -notmatch 'Remove-Item')
    Check 'lint' 'main_has_single_guarded_remove_item' ((([regex]::Matches($mainSrc, 'Remove-Item')).Count -eq 1) -and ($mainSrc -match 'Test-SmokePathUnder \$saved \$f\) -and -not \(Test-SmokeReparse \$f\)\) \{\s*try \{ Remove-Item'))
    Check 'lint' 'main_uses_guarded_wrapper' ($mainSrc -match 'Invoke-SmokeGuarded -State \$state -Body')
    Check 'lint' 'main_injects_via_set_smoke_readonly' ($mainSrc -match 'Set-SmokeReadOnly \$state \$script:savePath')
    Check 'lint' 'main_refuses_main_checkout' ($mainSrc -match 'Test-SmokeIsMainCheckout \$RepoRoot\) -and -not \$AllowMainCheckout')
    Check 'lint' 'main_honors_agent_build_lock' ($mainSrc -match 'agent-build\.lock')
    Check 'lint' 'main_defers_on_busy_processes' ($mainSrc -match 'Get-SmokeBusyProcesses -Inventory')
    Check 'lint' 'main_never_builds' ($codeOnly -notmatch '(?i)Build\.bat|UnrealBuildTool\.exe|dotnet ')
    Check 'lint' 'main_passes_player_name_from_log_to_evaluator' ($mainSrc -match '-PlayerA \$script:playerName')
    Check 'lint' 'main_kills_client_itself_after_server_exit' ($mainSrc -match 'Stop-SmokeOwnedProcess \$state \$cli')
    Check 'lint' 'main_enforces_hard_cap_in_exit_wait' ($mainSrc -match "Get-SmokeTimeDecision \`$clock\.Elapsed\.TotalSeconds \`$HardCapSeconds\r?\n\s*if \(\`$d -ne 'ok'\) \{ \[void\]\`$script:runnerFails\.Add\('hard_cap_exceeded'\)")
    Check 'lint' 'main_checks_dedicated_netmode' ($mainSrc -match 'NetMode -ne 1')
    Check 'lint' 'main_resolves_final_verdict_with_runner_reasons' ($mainSrc -match 'Resolve-SmokeFinalVerdict -EvidenceVerdict \$evVerdict -RunnerFails')
    Check 'lint' 'save_metadata_captured_before_cleanup_deletes_it' ($mainSrc.Contains('$script:saveInfo = [pscustomobject]') -and $mainSrc.Contains('$saveInfo = $script:saveInfo'))
    Check 'lint' 'sources_are_ascii_only' (([regex]::Matches($bothSrc, '[^\x00-\x7F]')).Count -eq 0)
}
catch {
    Write-Host ('[MMO_SMOKE_INFO] fatal_exception ' + $_.Exception.Message + ' at ' + $_.InvocationInfo.ScriptLineNumber)
    Check 'harness' 'fatal_exception' $false
}
finally {
    try { Remove-Item -LiteralPath $work -Recurse -Force } catch { Write-Host ('[MMO_SMOKE_INFO] cleanup_failed ' + $work) }
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
