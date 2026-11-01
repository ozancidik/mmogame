# Kapanis duman testi KOSUCUSU regresyonu (docs/DESIGN_SHUTDOWN_SMOKE.md, dilim S2/S3 + guvenlik turu S2b): Scripts/ShutdownSmokeRunnerLib.ps1 saf yardimcilari +
# Scripts/TestShutdownSmoke.ps1 kabul/erteleme/kullanim kapilari + kaynak taramasi (ad ile surec oldurme yasagi vb.).
# UE, oyun, sunucu, istemci BASLATILMAZ; gercek Saved/ okunmaz/yazilmaz. Yalniz %TEMP% altinda gecici dosya/dizin, sahte surec nesneleri ve testin KENDI
# baslattigi zararsiz cocuk surecler (powershell Start-Sleep) kullanilir.
# Her vaka ayri CASE satiri basar: [MMO_SMOKE_CASE] PASS|FAIL case=<vaka> check=<kontrol>. Kalip: TestShutdownSmokeEvidence.ps1 (N: bu dosya, CASE sayaci, commit mesaji).
# Windows PowerShell 5.1 icin dogrulandi; PowerShell 7 DOGRULANMADI. Dosya yalniz ASCII. Cikis: 0 PASS, 1 FAIL.
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ShutdownSmokeEvidence.ps1')
. (Join-Path $PSScriptRoot 'ShutdownSmokeRunnerLib.ps1')

$expected = 331

$script:labels = @{}
$script:caseLines = 0
$script:failures = 0
$script:emitted = New-Object System.Collections.ArrayList
$script:events = New-Object System.Collections.ArrayList
$evList = $script:events
$script:spawned = New-Object System.Collections.ArrayList

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
    # HasExited/Id/Kill/WaitForExit/StartTime/ProcessName taklit eden nesne; Killed sayaci kosuyu, $script:events sirayi gorunur kilar.
    param([int]$Id, [bool]$Exited = $false, [bool]$KillThrows = $false, $Start = $null, [string]$Name = 'UnrealEditor-Cmd', [bool]$NoStart = $false)
    if ($null -eq $Start) { $Start = [datetime]::UtcNow.AddMinutes(-5) }
    $o = New-Object psobject -Property @{ Id = $Id; HasExited = $Exited; Killed = 0; KillThrows = $KillThrows; StartTime = $(if ($NoStart) { $null } else { $Start }); ProcessName = $Name }
    $o | Add-Member -MemberType ScriptMethod -Name Kill -Value { if ($this.KillThrows) { throw 'kill failed' }; $this.Killed++; $this.HasExited = $true; [void]$script:events.Add('kill:' + $this.Id) }
    $o | Add-Member -MemberType ScriptMethod -Name WaitForExit -Value { param($ms) return $true }
    return $o
}
function New-TestState {
    $s = New-SmokeState
    $s.Inventory = { @() }
    return $s
}
function Wait-Until {
    param([scriptblock]$Cond, [int]$Seconds = 15)
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $Seconds) { if (& $Cond) { return $true }; Start-Sleep -Milliseconds 250 }
    return [bool](& $Cond)
}
function Test-PidAlive {
    param([int]$ProcId)
    try { $p = [Diagnostics.Process]::GetProcessById($ProcId); return (-not $p.HasExited) } catch { return $false }
}
function Start-Sleeper {
    # Testin KENDI zararsiz cocugu; temizlik icin $script:spawned'a yazilir.
    param([string]$Exe = 'powershell.exe', [int]$Seconds = 120)
    $p = Start-Process -FilePath $Exe -ArgumentList @('-NoProfile', '-Command', ('Start-Sleep -Seconds ' + $Seconds)) -PassThru -WindowStyle Hidden
    $null = $p.Handle
    [void]$script:spawned.Add($p)
    return $p
}

try {
    # ================= KOMUT SATIRI =================
    $proj = 'C:\Users\Test User\.claude\worktrees\wt\MyProject.uproject'
    $sa = Get-SmokeServerArgs -Project $proj -Map 'Summer_scene' -LogPath 'C:\Temp\x y\server.log' -Port 18123 -ShutdownAfter 240
    Check 'srv_args' 'exact_list' (SameArr $sa @('"C:\Users\Test User\.claude\worktrees\wt\MyProject.uproject"', '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene', '-game', '-server', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout', '-ABSLOG="C:\Temp\x y\server.log"', '-port=18123', '-MultiHome=127.0.0.1', '-MMOShutdownAfterSeconds=240', '-MMOMaintenanceReason=bakim'))
    Check 'srv_args' 'has_game_and_server' ((@($sa) -ccontains '-game') -and (@($sa) -ccontains '-server'))
    Check 'srv_args' 'binds_loopback_only_via_multihome' (@($sa) -ccontains '-MultiHome=127.0.0.1')
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
    $cs = Get-SmokeClientArgs -Project $proj -Port 18123 -LogPath 'C:\Temp\client.log' -MaxSeconds 420
    Check 'cli_args' 'self_exit_seconds_appended' ((@($cs) -ccontains '-seconds=420') -and $cs.Count -eq ($ca.Count + 1))
    Check 'cli_args_neg' 'bad_port_throws' (Throws { Get-SmokeClientArgs -Project $proj -Port 0 -LogPath 'C:\l.log' })
    Check 'cli_args_neg' 'max_seconds_below_60_throws' (Throws { Get-SmokeClientArgs -Project $proj -Port 18123 -LogPath 'C:\l.log' -MaxSeconds 30 })

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

    # ================= SUNUCU HAZIR / KAPANIS / DINLENEN PORT =================
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
    # ---- S4 (loginclosed) yardimcilari: faz goruldu mu / tum katilim adlari / PreLogin red sayisi ----
    $flushLine = $ts + 'LogMMOLifecycle: Display: event=shutdown_phase phase=Flushing remaining=20 players=1' + "`r`n"
    Check 'phase_seen' 'flushing_logged_is_seen' (Test-SmokePhaseSeen $flushLine 'Flushing')
    Check 'phase_seen' 'closing_logged_is_seen' (Test-SmokePhaseSeen ($ts + 'LogMMOLifecycle: Display: event=shutdown_phase phase=Closing remaining=60 players=1' + "`n") 'Closing')
    Check 'phase_seen_neg' 'closing_only_log_does_not_show_flushing' (-not (Test-SmokePhaseSeen ($ts + 'LogMMOLifecycle: Display: event=shutdown_phase phase=Closing remaining=60 players=1' + "`n") 'Flushing'))
    Check 'phase_seen_neg' 'phase_name_prefix_is_not_a_match' (-not (Test-SmokePhaseSeen ($ts + 'event=shutdown_phase phase=FlushingX remaining=1' + "`n") 'Flushing'))
    Check 'phase_seen_neg' 'regex_metacharacters_in_phase_name_refused' (-not (Test-SmokePhaseSeen $flushLine 'Fl.shing') -and -not (Test-SmokePhaseSeen $flushLine '.*'))
    Check 'phase_seen_neg' 'null_and_empty_log_give_false' ((-not (Test-SmokePhaseSeen $null 'Flushing')) -and (-not (Test-SmokePhaseSeen '' 'Flushing')))
    $jnA = Get-SmokeJoinedNames ($ts + 'LogNet: Join succeeded: Gezgin111111' + "`r`n")
    Check 'joined_names' 'single_join_counts_one' ($jnA.Count -eq 1 -and $jnA.Names[0] -ceq 'Gezgin111111' -and -not $jnA.Invalid)
    $jnB = Get-SmokeJoinedNames ($ts + 'LogNet: Join succeeded: Gezgin111111' + "`r`n" + $ts + 'LogNet: Join succeeded: Gezgin222222' + "`r`n")
    Check 'joined_names' 'second_player_makes_count_two' ($jnB.Count -eq 2 -and (SameArr $jnB.Names @('Gezgin111111', 'Gezgin222222')))
    Check 'joined_names' 'same_name_twice_counts_one' ((Get-SmokeJoinedNames ("LogNet: Join succeeded: Abc`nLogNet: Join succeeded: Abc`n")).Count -eq 1)
    Check 'joined_names_neg' 'invalid_name_sets_invalid_and_is_not_counted' ((Get-SmokeJoinedNames "LogNet: Join succeeded: Evil result=saved`n").Invalid -and (Get-SmokeJoinedNames "LogNet: Join succeeded: Evil result=saved`n").Count -eq 0)
    Check 'joined_names_neg' 'other_log_category_not_a_join' ((Get-SmokeJoinedNames "LogTemp: Join succeeded: Abc`n").Count -eq 0)
    Check 'joined_names_neg' 'null_and_empty_give_zero' (((Get-SmokeJoinedNames $null).Count -eq 0) -and ((Get-SmokeJoinedNames '').Count -eq 0))
    $plLine = $ts + 'LogMMOLifecycle: Display: event=prelogin_rejected phase=Closing' + "`r`n"
    Check 'prelogin_count' 'one_line_counts_one' ((Get-SmokePreloginRejectedCount $plLine) -eq 1)
    Check 'prelogin_count' 'two_lines_count_two' ((Get-SmokePreloginRejectedCount ($plLine + $plLine)) -eq 2)
    Check 'prelogin_count' 'last_line_without_eol_counts' ((Get-SmokePreloginRejectedCount ($ts + 'LogMMOLifecycle: Display: event=prelogin_rejected phase=Flushing')) -eq 1)
    Check 'prelogin_count_neg' 'other_category_forgery_not_counted' ((Get-SmokePreloginRejectedCount 'LogTemp: Display: event=prelogin_rejected phase=Closing') -eq 0)
    Check 'prelogin_count_neg' 'trailing_junk_not_counted' ((Get-SmokePreloginRejectedCount ($ts + 'LogMMOLifecycle: Display: event=prelogin_rejected phase=Closing extra' + "`n")) -eq 0)
    Check 'prelogin_count_neg' 'null_and_empty_give_zero' (((Get-SmokePreloginRejectedCount $null) -eq 0) -and ((Get-SmokePreloginRejectedCount '') -eq 0))
    $lpLine = $ts + 'LogNet: Name:GameNetDriver Def:GameNetDriver IpNetDriver_0 IpNetDriver listening on port 18994' + "`r`n"
    Check 'listen_port' 'real_log_line_gives_port' (SameArr @(Get-SmokeListeningPort $lpLine) @(18994))
    Check 'listen_port' 'engine_moved_to_next_port_is_reported_as_logged' (SameArr @(Get-SmokeListeningPort ($ts + 'LogNet: Name:GameNetDriver Def:GameNetDriver IpNetDriver_0 IpNetDriver listening on port 18995' + "`n")) @(18995))
    Check 'listen_port' 'same_port_twice_is_one_entry' (@(Get-SmokeListeningPort ($lpLine + $lpLine)).Count -eq 1)
    Check 'listen_port_neg' 'two_ports_are_both_reported_so_caller_can_reject' (@(Get-SmokeListeningPort ($lpLine + $lpLine.Replace('18994', '18996'))).Count -eq 2)
    Check 'listen_port_neg' 'absent_gives_empty' (@(Get-SmokeListeningPort "LogNet: Display: hello`n").Count -eq 0)
    Check 'listen_port_neg' 'other_driver_name_is_not_counted' (@(Get-SmokeListeningPort 'LogNet: Name:DemoNetDriver Def:DemoNetDriver IpNetDriver_1 IpNetDriver listening on port 1234').Count -eq 0)
    Check 'listen_port_neg' 'null_gives_empty' (@(Get-SmokeListeningPort $null).Count -eq 0)

    # ================= ERTELEME (baska UE/UBT/derleyici) =================
    function Row { param([string]$Name, [string]$Cmd = '') return [pscustomobject]@{ ProcessId = 1; Name = $Name; CommandLine = $Cmd } }
    Check 'busy' 'unreal_editor_cmd_is_busy' (@(Get-SmokeBusyProcesses @((Row 'UnrealEditor-Cmd.exe'))).Count -eq 1)
    Check 'busy' 'unreal_editor_is_busy' (@(Get-SmokeBusyProcesses @((Row 'UnrealEditor.exe'))).Count -eq 1)
    Check 'busy' 'unreal_build_tool_is_busy' (@(Get-SmokeBusyProcesses @((Row 'UnrealBuildTool.exe'))).Count -eq 1)
    Check 'busy' 'msbuild_is_busy' (@(Get-SmokeBusyProcesses @((Row 'MSBuild.exe'))).Count -eq 1)
    Check 'busy' 'live_coding_console_is_busy' (@(Get-SmokeBusyProcesses @((Row 'LiveCodingConsole.exe'))).Count -eq 1)
    Check 'busy' 'cl_exe_is_busy' (@(Get-SmokeBusyProcesses @((Row 'cl.exe'))).Count -eq 1)
    Check 'busy' 'link_exe_is_busy' (@(Get-SmokeBusyProcesses @((Row 'link.exe'))).Count -eq 1)
    Check 'busy' 'unreal_trace_server_is_busy' (@(Get-SmokeBusyProcesses @((Row 'UnrealTraceServer.exe'))).Count -eq 1)
    Check 'busy' 'packaged_shipping_game_is_busy' (@(Get-SmokeBusyProcesses @((Row 'MyProject-Win64-Shipping.exe'))).Count -eq 1)
    Check 'busy' 'packaged_game_plain_exe_is_busy' (@(Get-SmokeBusyProcesses @((Row 'MyProject.exe'))).Count -eq 1)
    Check 'busy' 'packaged_server_exe_is_busy' (@(Get-SmokeBusyProcesses @((Row 'MyProjectServer-Win64-Development.exe'))).Count -eq 1)
    Check 'busy' 'dotnet_running_ubt_is_busy' (@(Get-SmokeBusyProcesses @((Row 'dotnet.exe' 'dotnet UnrealBuildTool.dll -Mode=Build'))).Count -eq 1)
    Check 'busy' 'dotnet_with_unknown_command_line_is_busy' (@(Get-SmokeBusyProcesses @((Row 'dotnet.exe' ''))).Count -eq 1)
    Check 'busy' 'cmd_running_build_bat_is_busy' (@(Get-SmokeBusyProcesses @((Row 'cmd.exe' 'cmd /c "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64'))).Count -eq 1)
    Check 'busy' 'cmd_running_ubt_command_line_is_busy' (@(Get-SmokeBusyProcesses @((Row 'cmd.exe' 'cmd /c UnrealBuildTool.exe MyProject'))).Count -eq 1)
    Check 'busy' 'dotnet_unrelated_is_not_busy' (@(Get-SmokeBusyProcesses @((Row 'dotnet.exe' 'dotnet tool run something'))).Count -eq 0)
    Check 'busy' 'unrelated_cmd_is_not_busy' (@(Get-SmokeBusyProcesses @((Row 'cmd.exe' 'cmd /d /s /c echo hello'), (Row 'cmd.exe' ''))).Count -eq 0)
    Check 'busy' 'ordinary_processes_are_not_busy' (@(Get-SmokeBusyProcesses @((Row 'notepad.exe'), (Row 'powershell.exe' '-File x.ps1'), (Row 'bash.exe'), (Row 'claude.exe'))).Count -eq 0)
    Check 'busy' 'similar_names_are_not_busy' (@(Get-SmokeBusyProcesses @((Row 'clx.exe'), (Row 'linker.exe'), (Row 'MyProjectNotes.exe'))).Count -eq 0)
    Check 'busy' 'null_and_empty_inventory_are_not_busy' ((@(Get-SmokeBusyProcesses $null).Count -eq 0) -and (@(Get-SmokeBusyProcesses @()).Count -eq 0))
    $mixed = @((Row 'notepad.exe'), (Row 'UnrealEditor-Cmd.exe'), (Row 'MSBuild.exe'))
    Check 'busy' 'mixed_inventory_reports_only_busy_rows' (@(Get-SmokeBusyProcesses $mixed).Count -eq 2)

    # ================= TORUN AGACI (olusum zamani dogrulamali) =================
    $t0 = [datetime]::SpecifyKind([datetime]'2026-01-01T10:00:00', [DateTimeKind]::Utc)
    function IRow { param([int]$Id, [int]$Parent, $Created) $o = [pscustomobject]@{ ProcessId = $Id; ParentProcessId = $Parent; CreationDate = $Created }; return $o }
    $inv = @(
        (IRow 10 1 $t0), (IRow 11 10 $t0.AddSeconds(1)), (IRow 12 11 $t0.AddSeconds(2)),
        (IRow 20 2 $t0), (IRow 21 20 $t0.AddSeconds(1))
    )
    Check 'descendants' 'chain_of_owned_root' (SameArr (@(Get-SmokeDescendants $inv 10 $t0) | ForEach-Object { $_.Pid } | Sort-Object) @(11, 12))
    Check 'descendants' 'unrelated_tree_not_included' (-not (@(Get-SmokeDescendants $inv 10 $t0) | Where-Object { $_.Pid -eq 20 -or $_.Pid -eq 21 }))
    Check 'descendants' 'root_itself_excluded' (-not (@(Get-SmokeDescendants $inv 10 $t0) | Where-Object { $_.Pid -eq 10 }))
    Check 'descendants' 'leaf_has_none' (@(Get-SmokeDescendants $inv 12 $t0.AddSeconds(2)).Count -eq 0)
    Check 'descendants' 'creation_time_is_returned_for_identity_check' ((@(Get-SmokeDescendants $inv 10 $t0) | Where-Object { $_.Pid -eq 11 }).Creation -eq $t0.AddSeconds(1))
    Check 'descendants' 'child_created_at_same_instant_as_parent_is_included' (@(Get-SmokeDescendants @((IRow 11 10 $t0)) 10 $t0).Count -eq 1)
    $cyc = @((IRow 10 11 $t0), (IRow 11 10 $t0.AddSeconds(1)))
    Check 'descendants' 'cycle_terminates' (@(Get-SmokeDescendants $cyc 10 $t0).Count -ge 0)
    Check 'descendants' 'empty_inventory_gives_none' ((@(Get-SmokeDescendants @() 10 $t0).Count -eq 0) -and (@(Get-SmokeDescendants $null 10 $t0).Count -eq 0))
    $invOld = @((IRow 13 10 $t0.AddHours(-1)), (IRow 11 10 $t0.AddSeconds(1)), (IRow 14 11 $t0.AddMilliseconds(500)))
    $dOld = @(Get-SmokeDescendants $invOld 10 $t0)
    Check 'descendants_neg' 'reused_parent_pid_child_older_than_parent_excluded' (-not ($dOld | Where-Object { $_.Pid -eq 13 }))
    Check 'descendants_neg' 'grandchild_older_than_its_parent_excluded' (-not ($dOld | Where-Object { $_.Pid -eq 14 }) -and ($dOld | Where-Object { $_.Pid -eq 11 }))
    Check 'descendants_neg' 'row_without_creation_date_excluded' (@(Get-SmokeDescendants @([pscustomobject]@{ ProcessId = 11; ParentProcessId = 10 }, (IRow 12 10 $null)) 10 $t0).Count -eq 0)
    Check 'descendants_neg' 'unknown_root_start_fails_closed_with_nothing' (@(Get-SmokeDescendants $inv 10 $null).Count -eq 0)

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
    # bilesen zinciri: Saved\SaveGames bir junction ise oyuncu kaydi yolu reparse sayilir
    $rs = Join-Path $work 'rsaved'; [void][IO.Directory]::CreateDirectory($rs)
    $outside = Join-Path $work 'outside_savegames'; [void][IO.Directory]::CreateDirectory($outside)
    $sgJ = Join-Path $rs 'SaveGames'
    $sgMade = $false
    try { New-Item -ItemType Junction -Path $sgJ -Target $outside | Out-Null; $sgMade = $true } catch { }
    Check 'reparse_chain' 'junction_component_in_save_path_detected' ($sgMade -and (Test-SmokePathHasReparse $rs (Join-Path $sgJ 'MMOSave_A.sav')))
    if ($sgMade) { [IO.Directory]::Delete($sgJ) }
    [void][IO.Directory]::CreateDirectory((Join-Path $rs 'SaveGames'))
    Check 'reparse_chain_neg' 'plain_chain_has_no_reparse' (-not (Test-SmokePathHasReparse $rs (Join-Path $rs 'SaveGames\MMOSave_A.sav')))
    Check 'reparse_chain_neg' 'path_outside_root_fails_closed' (Test-SmokePathHasReparse $rs (Join-Path $work 'elsewhere\x.sav'))
    Check 'reparse_chain_neg' 'empty_inputs_fail_closed' ((Test-SmokePathHasReparse '' 'C:\a\b') -and (Test-SmokePathHasReparse 'C:\a' ''))

    # ================= KAYITLI WORKTREE (git worktree list --porcelain) =================
    $porc = "worktree C:/Users/x/Main`nHEAD aaa`nbranch refs/heads/master`n`nworktree C:/Users/x/.claude/worktrees/wt1`nHEAD bbb`ndetached`n`nworktree C:/Users/x/other wt/two`nHEAD ccc`ndetached`n"
    Check 'worktree' 'second_entry_is_registered_linked_worktree' (Test-SmokeIsRegisteredLinkedWorktree 'C:\Users\x\.claude\worktrees\wt1' $porc)
    Check 'worktree' 'path_with_space_and_case_slash_differences_ok' (Test-SmokeIsRegisteredLinkedWorktree 'c:/users/x/OTHER WT/two/' $porc)
    Check 'worktree' 'crlf_porcelain_ok' (Test-SmokeIsRegisteredLinkedWorktree 'C:\Users\x\.claude\worktrees\wt1' $porc.Replace("`n", "`r`n"))
    Check 'worktree_neg' 'first_entry_main_worktree_is_not_linked' (-not (Test-SmokeIsRegisteredLinkedWorktree 'C:\Users\x\Main' $porc))
    Check 'worktree_neg' 'unlisted_path_is_not_registered' (-not (Test-SmokeIsRegisteredLinkedWorktree 'C:\Users\x\.claude\worktrees\fake' $porc))
    Check 'worktree_neg' 'prefix_of_registered_path_is_not_registered' (-not (Test-SmokeIsRegisteredLinkedWorktree 'C:\Users\x\.claude\worktrees\wt' $porc))
    Check 'worktree_neg' 'only_main_listed_means_no_linked_worktree' (-not (Test-SmokeIsRegisteredLinkedWorktree 'C:\Users\x\Main' "worktree C:/Users/x/Main`nHEAD aaa`n"))
    Check 'worktree_neg' 'empty_or_null_porcelain_fails_closed' ((-not (Test-SmokeIsRegisteredLinkedWorktree 'C:\a' '')) -and (-not (Test-SmokeIsRegisteredLinkedWorktree 'C:\a' $null)))
    Check 'worktree_neg' 'garbage_text_gives_no_entries' (@(Get-SmokeWorktreeEntries "hello`nworld").Count -eq 0)

    # ================= PORT SAHIPLENME (TOCTOU) =================
    $pp = 43000 + (Get-Random -Minimum 0 -Maximum 2000)
    $hold = New-SmokeUdpHold $pp
    Check 'port_hold' 'free_port_is_held' ($null -ne $hold)
    Check 'port_hold_neg' 'second_hold_on_same_port_fails' ($null -eq (New-SmokeUdpHold $pp))
    Close-SmokeUdpHold $hold
    $hold2 = New-SmokeUdpHold $pp
    Check 'port_hold' 'port_is_free_again_after_release' ($null -ne $hold2)
    Close-SmokeUdpHold $hold2
    Close-SmokeUdpHold $null

    # ================= OLUSAN DOSYALAR (tam ad desenleri) =================
    $sr = 'C:\R\Saved'
    $before = @("$sr\SaveGames\MMOSave_Old.sav", "$sr\Logs\MMOShutdownResult.json")
    $after = $before + @(
        "$sr\SaveGames\MMOSave_Gezgin1.sav", "$sr\MMODaily\MMOSave_Gezgin1.json", "$sr\MMODaily\MMOSave_Gezgin1.json.tmp", "$sr\MMOAchievements\MMOSave_Gezgin1.json",
        "$sr\MMOMail\MMOMail_gezgin1.json", "$sr\MMOMail\MMOMail_gezgin1.json.bak", "$sr\MMORace\MMORace_mmosave_gezgin1.json",
        "$sr\SaveGames\MMOSave_Gezgin10.sav", "$sr\SaveGames\MMOSave_Other.sav", "$sr\MMOMail\MMOMail_gezgin10.json", "$sr\Gezgin1\other.txt", "$sr\Logs\Gezgin1.log",
        "$sr\SaveGames\Gezgin1.sav", "$sr\SaveGames\MMOSave_Gezgin1.sav.old", "D:\Other\SaveGames\MMOSave_Gezgin1.sav", "$sr\MMOSave_Gezgin1.sav",
        "$sr\SaveGames\MMOSave_Gezgin1.sav.tmp", "$sr\SaveGames\MMOSave_Gezgin1.sav.bak", "$sr\SaveGames\MMOSave_Gezgin1.sav.bad", "$sr\SaveGames\MMOSave_Gezgin1.sav.bad.2", "$sr\SaveGames\MMOSave_Gezgin1.sav.bad.9", "$sr\SaveGames\MMOSave_Gezgin10.sav.tmp"
    )
    $cf = @(Get-SmokeCreatedFiles -Before $before -After $after -PlayerName 'Gezgin1' -SavedRoot $sr)
    $expectHit = @("$sr\SaveGames\MMOSave_Gezgin1.sav", "$sr\MMODaily\MMOSave_Gezgin1.json", "$sr\MMODaily\MMOSave_Gezgin1.json.tmp", "$sr\MMOAchievements\MMOSave_Gezgin1.json", "$sr\MMOMail\MMOMail_gezgin1.json", "$sr\MMOMail\MMOMail_gezgin1.json.bak", "$sr\MMORace\MMORace_mmosave_gezgin1.json", "$sr\SaveGames\MMOSave_Gezgin1.sav.tmp", "$sr\SaveGames\MMOSave_Gezgin1.sav.bak", "$sr\SaveGames\MMOSave_Gezgin1.sav.bad", "$sr\SaveGames\MMOSave_Gezgin1.sav.bad.2")
    Check 'created' 'exactly_the_players_own_pattern_files_are_selected' (SameArr (@($cf) | Sort-Object) (@($expectHit) | Sort-Object))
    Check 'created_neg' 'preexisting_never_included' (-not ($cf -contains "$sr\SaveGames\MMOSave_Old.sav"))
    Check 'created_neg' 'new_file_of_someone_else_not_included' (-not ($cf -contains "$sr\SaveGames\MMOSave_Other.sav"))
    Check 'created_neg' 'longer_name_with_same_prefix_not_included' ((-not ($cf -contains "$sr\SaveGames\MMOSave_Gezgin10.sav")) -and (-not ($cf -contains "$sr\MMOMail\MMOMail_gezgin10.json")))
    Check 'created_neg' 'player_name_only_in_directory_not_included' (-not ($cf -contains "$sr\Gezgin1\other.txt"))
    Check 'created_neg' 'logs_directory_never_included' (-not ($cf -contains "$sr\Logs\Gezgin1.log"))
    Check 'created_neg' 'not_whitelisted_names_not_included' ((-not ($cf -contains "$sr\SaveGames\Gezgin1.sav")) -and (-not ($cf -contains "$sr\SaveGames\MMOSave_Gezgin1.sav.old")) -and (-not ($cf -contains "$sr\MMOSave_Gezgin1.sav")))
    Check 'created_neg' 'file_outside_saved_not_included' (-not ($cf -contains 'D:\Other\SaveGames\MMOSave_Gezgin1.sav'))
    Check 'created' 'atomic_character_side_files_tmp_bak_bad_are_cleaned_up_after_a_run' (@('.sav.tmp', '.sav.bak', '.sav.bad', '.sav.bad.2' | Where-Object { $cf -contains ("$sr\SaveGames\MMOSave_Gezgin1" + $_) }).Count -eq 4)
    Check 'created_neg' 'bad_quota_overflow_and_longer_name_tmp_not_included' ((-not ($cf -contains "$sr\SaveGames\MMOSave_Gezgin1.sav.bad.9")) -and (-not ($cf -contains "$sr\SaveGames\MMOSave_Gezgin10.sav.tmp")))
    Check 'created_neg' 'invalid_player_name_deletes_nothing' ((@(Get-SmokeCreatedFiles -Before $before -After $after -PlayerName 'bad name' -SavedRoot $sr).Count -eq 0) -and (@(Get-SmokeCreatedFiles -Before $before -After $after -PlayerName '' -SavedRoot $sr).Count -eq 0) -and (@(Get-SmokeCreatedFiles -Before $before -After $after -PlayerName 'Gezgin1' -SavedRoot '').Count -eq 0))
    Check 'created' 'case_insensitive_before_set' (@(Get-SmokeCreatedFiles -Before @('c:\r\saved\savegames\mmosave_gezgin1.sav') -After @("$sr\SaveGames\MMOSave_Gezgin1.sav") -PlayerName 'Gezgin1' -SavedRoot $sr).Count -eq 0)
    $afterShort = $before + @("$sr\SaveGames\MMOSave_a.sav", "$sr\SaveGames\MMOSave_ab.sav", "$sr\SaveGames\MMOSave_ba.sav", "$sr\SaveGames\MMOSave_Alice.sav", "$sr\Logs\Game.log", "$sr\SaveGames\MMOSave_MMO.sav", "$sr\SaveGames\MMOSave_Result.sav", "$sr\SaveGames\MMOSave_Results.sav")
    Check 'created_neg' 'short_name_a_does_not_match_ab_ba_alice_or_logs' (SameArr @(Get-SmokeCreatedFiles -Before $before -After $afterShort -PlayerName 'a' -SavedRoot $sr) @("$sr\SaveGames\MMOSave_a.sav"))
    Check 'created_neg' 'name_MMO_matches_only_its_own_save' (SameArr @(Get-SmokeCreatedFiles -Before $before -After $afterShort -PlayerName 'MMO' -SavedRoot $sr) @("$sr\SaveGames\MMOSave_MMO.sav"))
    Check 'created_neg' 'name_Result_never_touches_shutdown_result_json_or_results' (SameArr @(Get-SmokeCreatedFiles -Before $before -After ($afterShort + @("$sr\Logs\MMOShutdownResult.json")) -PlayerName 'Result' -SavedRoot $sr) @("$sr\SaveGames\MMOSave_Result.sav"))

    # ---- gercek dosyalarla silme (gecici Saved) ----
    $rm = Join-Path $work 'rm_saved'
    foreach ($d in 'SaveGames', 'Logs', 'MMOMail', 'MMODaily') { [void][IO.Directory]::CreateDirectory((Join-Path $rm $d)) }
    [IO.File]::WriteAllText((Join-Path $rm 'SaveGames\MMOSave_Old.sav'), 'old')
    [IO.File]::WriteAllText((Join-Path $rm 'Logs\MMOShutdownResult.json'), '{}')
    $snapRm = @(Get-ChildItem -LiteralPath $rm -Recurse -File | ForEach-Object { $_.FullName })
    foreach ($f in 'SaveGames\MMOSave_a.sav', 'SaveGames\MMOSave_ab.sav', 'SaveGames\MMOSave_MMO.sav', 'SaveGames\MMOSave_Result.sav', 'Logs\Game.log', 'Logs\MMOShutdownResult.json.new', 'MMODaily\MMOSave_a.json', 'MMOMail\MMOMail_a.json', 'MMOMail\MMOMail_ab.json') {
        [IO.File]::WriteAllText((Join-Path $rm $f), 'x')
    }
    $removedA = @(Remove-SmokeCreatedFiles -SavedRoot $rm -Before $snapRm -PlayerName 'a')
    Check 'remove' 'players_own_files_are_deleted' ((-not (Test-Path (Join-Path $rm 'SaveGames\MMOSave_a.sav'))) -and (-not (Test-Path (Join-Path $rm 'MMODaily\MMOSave_a.json'))) -and (-not (Test-Path (Join-Path $rm 'MMOMail\MMOMail_a.json'))) -and $removedA.Count -eq 3)
    Check 'remove_neg' 'short_name_a_leaves_every_other_file' ((Test-Path (Join-Path $rm 'SaveGames\MMOSave_ab.sav')) -and (Test-Path (Join-Path $rm 'SaveGames\MMOSave_MMO.sav')) -and (Test-Path (Join-Path $rm 'SaveGames\MMOSave_Result.sav')) -and (Test-Path (Join-Path $rm 'Logs\Game.log')) -and (Test-Path (Join-Path $rm 'MMOMail\MMOMail_ab.json')) -and (Test-Path (Join-Path $rm 'SaveGames\MMOSave_Old.sav')))
    $null = Remove-SmokeCreatedFiles -SavedRoot $rm -Before $snapRm -PlayerName 'MMO'
    $null = Remove-SmokeCreatedFiles -SavedRoot $rm -Before $snapRm -PlayerName 'Result'
    Check 'remove_neg' 'names_MMO_and_Result_keep_shutdown_result_and_logs' ((Test-Path (Join-Path $rm 'Logs\MMOShutdownResult.json')) -and (Test-Path (Join-Path $rm 'Logs\MMOShutdownResult.json.new')) -and (Test-Path (Join-Path $rm 'Logs\Game.log')) -and (-not (Test-Path (Join-Path $rm 'SaveGames\MMOSave_MMO.sav'))) -and (-not (Test-Path (Join-Path $rm 'SaveGames\MMOSave_Result.sav'))) -and (Test-Path (Join-Path $rm 'SaveGames\MMOSave_ab.sav')))
    # reparse bileseni: Saved\MMOMail bir junction ise icindeki dosya SILINMEZ
    $rj = Join-Path $work 'rj_saved'; [void][IO.Directory]::CreateDirectory($rj)
    $rjOut = Join-Path $work 'rj_outside'; [void][IO.Directory]::CreateDirectory($rjOut)
    $rjSnap = @()
    $rjMade = $false
    try { New-Item -ItemType Junction -Path (Join-Path $rj 'MMOMail') -Target $rjOut | Out-Null; $rjMade = $true } catch { }
    [IO.File]::WriteAllText((Join-Path $rjOut 'MMOMail_zed.json'), 'x')
    $null = Remove-SmokeCreatedFiles -SavedRoot $rj -Before $rjSnap -PlayerName 'Zed'
    Check 'remove_neg' 'junction_component_file_not_deleted' ($rjMade -and (Test-Path (Join-Path $rjOut 'MMOMail_zed.json')))
    if ($rjMade) { [IO.Directory]::Delete((Join-Path $rj 'MMOMail')) }
    Check 'remove_neg' 'no_player_name_or_missing_root_deletes_nothing' ((@(Remove-SmokeCreatedFiles -SavedRoot $rm -Before $snapRm -PlayerName '').Count -eq 0) -and (@(Remove-SmokeCreatedFiles -SavedRoot (Join-Path $work 'nope_saved') -Before @() -PlayerName 'a').Count -eq 0))

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

    # ================= KANIT TOPLAMA (temizlikten ONCE) =================
    $ev = Join-Path $work 'ev'; [void][IO.Directory]::CreateDirectory($ev)
    $evSrv = Join-Path $ev 'server.log'; $evCli = Join-Path $ev 'client.log'; $evJson = Join-Path $ev 'MMOShutdownResult.json'; $evCopy = Join-Path $ev 'copy'
    [void][IO.Directory]::CreateDirectory($evCopy)
    [IO.File]::WriteAllText($evSrv, 'server text'); [IO.File]::WriteAllText($evCli, 'client text')
    $jsonBytes = (New-Object Text.UTF8Encoding($true)).GetPreamble() + [Text.Encoding]::UTF8.GetBytes('{"exit_code":0}')
    [IO.File]::WriteAllBytes($evJson, $jsonBytes)
    $bundle = Get-SmokeEvidenceBundle -ServerLog $evSrv -ClientLog $evCli -ResultJson $evJson -CopyDirectory $evCopy
    Check 'evidence' 'texts_are_read' ($bundle.ServerText -ceq 'server text' -and $bundle.ClientText -ceq 'client text')
    Check 'evidence' 'json_text_keeps_bom_for_the_evaluator' ($null -ne $bundle.JsonText -and $bundle.JsonText.Length -eq 16 -and [int][char]$bundle.JsonText[0] -eq 0xFEFF)
    $copyBytes = if ($null -ne $bundle.JsonCopy -and (Test-Path -LiteralPath $bundle.JsonCopy)) { [IO.File]::ReadAllBytes($bundle.JsonCopy) } else { [byte[]]@() }
    Check 'evidence' 'json_copy_bytes_equal' ((($copyBytes -join ',') -ceq ($jsonBytes -join ',')) -and $copyBytes.Length -gt 0)
    # sira: kanit alindiktan sonra temizlik (oyuncu adi 'Result') kanit dosyasini silse bile elde metin + kopya var
    $evSnap = @()
    $null = Remove-SmokeCreatedFiles -SavedRoot $ev -Before $evSnap -PlayerName 'Result'
    Check 'evidence' 'captured_evidence_survives_cleanup' ($null -ne $bundle.JsonText -and $bundle.JsonText.Length -eq 16 -and (Test-Path -LiteralPath $bundle.JsonCopy))
    $bn = Get-SmokeEvidenceBundle -ServerLog (Join-Path $ev 'no1.log') -ClientLog (Join-Path $ev 'no2.log') -ResultJson (Join-Path $ev 'no3.json') -CopyDirectory $evCopy
    $evCliB = Join-Path $ev 'clientB.log'; [IO.File]::WriteAllText($evCliB, 'client b text')
    $bundleB = Get-SmokeEvidenceBundle -ServerLog $evSrv -ClientLog $evCli -ResultJson $evJson -CopyDirectory $evCopy -ClientBLog $evCliB
    Check 'evidence' 'client_b_text_is_read_when_requested' ($bundleB.ClientBText -ceq 'client b text' -and $bundleB.ClientText -ceq 'client text')
    Check 'evidence_neg' 'client_b_text_is_null_when_not_requested_or_missing' (($null -eq $bundle.ClientBText) -and ($null -eq (Get-SmokeEvidenceBundle -ServerLog $evSrv -ClientLog $evCli -ResultJson $evJson -ClientBLog (Join-Path $ev 'nob.log')).ClientBText))
    Check 'evidence_neg' 'missing_files_give_null_texts_and_no_copy'(($null -eq $bn.ServerText) -and ($null -eq $bn.ClientText) -and ($null -eq $bn.JsonText) -and ($null -eq $bn.JsonCopy))

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
    $st.Inventory = { @([pscustomobject]@{ ProcessId = 7; ParentProcessId = 301; CreationDate = $t0.AddSeconds(1) }, [pscustomobject]@{ ProcessId = 9; ParentProcessId = 7; CreationDate = $t0.AddSeconds(2) }) }
    $parent = New-FakeProc 301 $false $false $t0
    Add-SmokeOwnedProcess $st $parent
    Invoke-SmokeCleanup $st
    Check 'cleanup_proc' 'child_missing_at_open_time_is_not_a_problem' ($parent.Killed -eq 1 -and $st.Problems.Count -eq 0)

    # ---- temizlik: acik tutulan (FileShare.None) kilit + salt-okunur .tmp; kilit salt-okunur geri almadan ONCE kapanir (S6.3 savefail enjeksiyonu) ----
    $lkDir = Join-Path $env:TEMP ('mmo_smoke_lock_test_' + [guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($lkDir)
    try {
        $lkFile = Join-Path $lkDir 'MMOSave_Gezgin1.sav.tmp'
        [IO.File]::WriteAllBytes($lkFile, [byte[]](1..16))
        $stLk = New-TestState
        Set-SmokeReadOnly $stLk $lkFile
        $lkHandle = New-Object IO.FileStream($lkFile, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
        Add-SmokeLock $stLk $lkHandle
        $delBlocked = $false
        try { [IO.File]::Delete($lkFile) } catch { $delBlocked = $true }
        $writeBlocked = $false
        try { $w = New-Object IO.FileStream($lkFile, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read); $w.Dispose() } catch { $writeBlocked = $true }
        Check 'cleanup_lock' 'held_lock_blocks_delete_and_other_opens' ($delBlocked -and $writeBlocked -and (Test-Path -LiteralPath $lkFile))
        Invoke-SmokeCleanup $stLk
        $attrCleared = (([IO.File]::GetAttributes($lkFile)) -band [IO.FileAttributes]::ReadOnly) -eq 0
        Check 'cleanup_lock' 'lock_released_before_readonly_restore_no_problems' (($stLk.Problems.Count -eq 0) -and $attrCleared -and ($stLk.Locks.Count -eq 0) -and ($stLk.Done -eq $true))
        $canDelete = $true
        try { [IO.File]::Delete($lkFile) } catch { $canDelete = $false }
        Check 'cleanup_lock' 'file_deletable_after_cleanup' ($canDelete -and -not (Test-Path -LiteralPath $lkFile))
        $stNoLocks = New-TestState
        $stNoLocks.Remove('Locks')
        Invoke-SmokeCleanup $stNoLocks
        Check 'cleanup_lock' 'state_without_locks_key_still_cleans_up' (($stNoLocks.Problems.Count -eq 0) -and ($stNoLocks.Done -eq $true))
    } finally {
        if (Test-Path -LiteralPath $lkDir) { try { Remove-Item -LiteralPath $lkDir -Recurse -Force } catch { } }
    }

    # ---- torun agaci: kimlik dogrulama ve sira (sahte nesnelerle) ----
    $script:events.Clear()
    $st = New-TestState
    $kids = @{ 7 = (New-FakeProc 7 $false $false $t0.AddSeconds(1)); 9 = (New-FakeProc 9 $false $false $t0.AddSeconds(2)) }
    $st.Inventory = { @([pscustomobject]@{ ProcessId = 7; ParentProcessId = 301; CreationDate = $t0.AddSeconds(1) }, [pscustomobject]@{ ProcessId = 9; ParentProcessId = 7; CreationDate = $t0.AddSeconds(2) }) }
    $st.OpenProcess = { param($ProcId) [void]$evList.Add('open:' + $ProcId); $kids[[int]$ProcId] }.GetNewClosure()
    $parent = New-FakeProc 301 $false $false $t0
    Add-SmokeOwnedProcess $st $parent
    Invoke-SmokeCleanup $st
    Check 'cleanup_tree' 'child_and_grandchild_killed' ($kids[7].Killed -eq 1 -and $kids[9].Killed -eq 1 -and $parent.Killed -eq 1)
    $ev1 = @($script:events)
    Check 'cleanup_tree' 'handles_opened_before_parent_is_killed' (($ev1.IndexOf('open:7') -ge 0) -and ($ev1.IndexOf('open:9') -ge 0) -and ($ev1.IndexOf('open:7') -lt $ev1.IndexOf('kill:301')) -and ($ev1.IndexOf('open:9') -lt $ev1.IndexOf('kill:301')))
    Check 'cleanup_tree' 'descendants_killed_after_parent' (($ev1.IndexOf('kill:301') -ge 0) -and ($ev1.IndexOf('kill:7') -gt $ev1.IndexOf('kill:301')) -and ($ev1.IndexOf('kill:9') -gt $ev1.IndexOf('kill:301')))
    Check 'cleanup_tree' 'no_problem_recorded' ($st.Problems.Count -eq 0)
    # PID yeniden kullanimi: envanter pid 7'yi t0+1 gosterir ama acilan surecin baslangici cok sonra
    $script:events.Clear()
    $st = New-TestState
    $reused = New-FakeProc 7 $false $false $t0.AddHours(3)
    $st.Inventory = { @([pscustomobject]@{ ProcessId = 7; ParentProcessId = 301; CreationDate = $t0.AddSeconds(1) }) }
    $st.OpenProcess = { param($ProcId) $reused }.GetNewClosure()
    $parent = New-FakeProc 301 $false $false $t0
    Add-SmokeOwnedProcess $st $parent
    Invoke-SmokeCleanup $st
    Check 'cleanup_tree' 'pid_reuse_start_mismatch_not_killed' ($reused.Killed -eq 0 -and $parent.Killed -eq 1)
    Check 'cleanup_tree' 'pid_reuse_is_noted_not_a_problem' (@($st.Notes | Where-Object { $_ -like 'descendant_identity_mismatch:7' }).Count -eq 1 -and $st.Problems.Count -eq 0)
    # eski ebeveyn baglantisi: cocuk satiri ebeveynden ONCE olusmus -> hic acilmaz, oldurulmez
    $script:events.Clear()
    $st = New-TestState
    $stale = New-FakeProc 8 $false $false $t0.AddHours(-2)
    $st.Inventory = { @([pscustomobject]@{ ProcessId = 8; ParentProcessId = 301; CreationDate = $t0.AddHours(-2) }) }
    $st.OpenProcess = { param($ProcId) [void]$evList.Add('open:' + $ProcId); $stale }.GetNewClosure()
    $parent = New-FakeProc 301 $false $false $t0
    Add-SmokeOwnedProcess $st $parent
    Invoke-SmokeCleanup $st
    Check 'cleanup_tree' 'child_older_than_parent_never_opened_or_killed' ($stale.Killed -eq 0 -and @($script:events | Where-Object { $_ -eq 'open:8' }).Count -eq 0 -and $parent.Killed -eq 1)
    # kok baslangic zamani bilinmiyor: torun ARANMAZ (kapali basarisiz), kok yine oldurulur
    $script:events.Clear()
    $st = New-TestState
    $kid = New-FakeProc 7 $false $false $t0.AddSeconds(1)
    $st.Inventory = { @([pscustomobject]@{ ProcessId = 7; ParentProcessId = 301; CreationDate = $t0.AddSeconds(1) }) }
    $st.OpenProcess = { param($ProcId) $kid }.GetNewClosure()
    $parent = New-FakeProc 301 $false $false $null 'UnrealEditor-Cmd' $true
    Add-SmokeOwnedProcess $st $parent
    Invoke-SmokeCleanup $st
    Check 'cleanup_tree_neg' 'unknown_root_start_kills_only_the_root' ($kid.Killed -eq 0 -and $parent.Killed -eq 1 -and $st.Problems.Count -eq 0)
    # envanter okunamiyor: kok yine oldurulur
    $st = New-TestState
    $st.Inventory = { throw 'cim failed' }
    $parent = New-FakeProc 301 $false $false $t0
    Add-SmokeOwnedProcess $st $parent
    Invoke-SmokeCleanup $st
    Check 'cleanup_tree_neg' 'inventory_failure_still_kills_the_root' ($parent.Killed -eq 1 -and $st.Problems.Count -eq 0)

    # ---- torun agaci: GERCEK surecler (testin kendi zararsiz cocuklari) ----
    $rootArgs = @('-NoProfile', '-Command', "Start-Process powershell.exe -ArgumentList '-NoProfile','-Command','Start-Sleep -Seconds 100' -WindowStyle Hidden; Start-Sleep -Seconds 100")
    $realRoot = Start-Process -FilePath 'powershell.exe' -ArgumentList $rootArgs -PassThru -WindowStyle Hidden
    $null = $realRoot.Handle
    [void]$script:spawned.Add($realRoot)
    $unrelated = Start-Sleeper
    $grandPid = 0
    $found = Wait-Until -Seconds 30 -Cond {
        $rows = @(Get-CimInstance -ClassName Win32_Process -Filter ('ParentProcessId=' + $realRoot.Id) -ErrorAction SilentlyContinue)
        if ($rows.Count -gt 0) { $script:grandPid = [int]$rows[0].ProcessId; return $true }
        return $false
    }
    Check 'real_tree' 'harness_started_a_real_grandchild' ($found -and $script:grandPid -gt 0 -and (Test-PidAlive $script:grandPid))
    $stReal = New-SmokeState
    Add-SmokeOwnedProcess $stReal $realRoot
    Invoke-SmokeCleanup $stReal
    Check 'real_tree' 'real_root_is_dead' (Wait-Until -Seconds 10 -Cond { -not (Test-PidAlive $realRoot.Id) })
    Check 'real_tree' 'real_grandchild_killed_by_tree_cleanup' (($script:grandPid -gt 0) -and (Wait-Until -Seconds 10 -Cond { -not (Test-PidAlive $script:grandPid) }))
    Check 'real_tree_neg' 'unrelated_sleeper_survives' (Test-PidAlive $unrelated.Id)
    Check 'real_tree' 'real_cleanup_has_no_problems' ($stReal.Problems.Count -eq 0)
    try { $unrelated.Kill() } catch { }

    # ================= JOB OBJECT =================
    $job = New-SmokeJob
    Check 'job' 'job_object_is_created' ($null -ne $job -and $job -ne [IntPtr]::Zero)
    $jp = Start-Sleeper
    $jstate = New-SmokeState
    $jstate.Job = $job
    $jstate.Inventory = { @() }
    Add-SmokeOwnedProcess $jstate $jp
    Check 'job' 'owned_process_is_in_job' ($null -ne $job -and [MMOSmokeJobNative]::InJob($job, $jp.Handle) -and (@($jstate.Notes | Where-Object { $_ -like 'job_assigned:*' }).Count -eq 1))
    $jp2 = Start-Sleeper
    [void][MMOSmokeJobNative]::Assign($job, $jp2.Handle)
    Close-SmokeJob $jstate
    Check 'job' 'close_kills_assigned_process_even_without_handle_kill' (Wait-Until -Seconds 15 -Cond { $jp2.HasExited })
    Check 'job' 'close_clears_state_job_handle' ($null -eq $jstate.Job)
    try { if (-not $jp.HasExited) { $jp.Kill() } } catch { }
    # kosucu OLDURULURSA (TerminateProcess) cocuk de olur: yardimci betik is nesnesini kurar, cocugu atar, test yardimciyi oldurur.
    $pidFile = Join-Path $work 'job_child.pid'
    $libPath = Join-Path $PSScriptRoot 'ShutdownSmokeRunnerLib.ps1'
    $helperText = ". '" + $libPath + "'`r`n" +
        "`$j = New-SmokeJob`r`n" +
        "`$p = Start-Process powershell.exe -ArgumentList '-NoProfile','-Command','Start-Sleep -Seconds 120' -PassThru -WindowStyle Hidden`r`n" +
        "`$null = `$p.Handle`r`n" +
        "`$ok = [MMOSmokeJobNative]::Assign(`$j, `$p.Handle)`r`n" +
        "[IO.File]::WriteAllText('" + $pidFile + "', (`$p.Id.ToString() + ' ' + `$ok))`r`n" +
        "Start-Sleep -Seconds 120`r`n"
    $helperPath = Join-Path $work 'job_helper.ps1'
    [IO.File]::WriteAllText($helperPath, $helperText)
    $helper = Start-Process -FilePath 'powershell.exe' -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $helperPath + '"')) -PassThru -WindowStyle Hidden
    $null = $helper.Handle
    [void]$script:spawned.Add($helper)
    $gotPid = Wait-Until -Seconds 60 -Cond { Test-Path -LiteralPath $pidFile }
    $childPid = 0
    $childAssigned = $false
    if ($gotPid) { Start-Sleep -Milliseconds 300; $parts = ([IO.File]::ReadAllText($pidFile)).Trim().Split(' '); $childPid = [int]$parts[0]; $childAssigned = ($parts[1] -eq 'True') }
    Check 'job' 'helper_runner_put_its_child_in_the_job' ($gotPid -and $childAssigned -and (Test-PidAlive $childPid))
    try { $helper.Kill() } catch { }
    Check 'job' 'runner_killed_children_die' ($childPid -gt 0 -and (Wait-Until -Seconds 20 -Cond { -not (Test-PidAlive $childPid) }))
    if ($childPid -gt 0) { try { [Diagnostics.Process]::GetProcessById($childPid).Kill() } catch { } }
    # yedek: Job yoksa (New-SmokeJob $null) sahip surec yine tutamacla oldurulur
    $stNoJob = New-TestState
    $fp = New-FakeProc 555
    Add-SmokeOwnedProcess $stNoJob $fp
    Invoke-SmokeCleanup $stNoJob
    Check 'job_neg' 'without_job_object_handle_kill_still_works_and_nothing_is_noted' ($fp.Killed -eq 1 -and @($stNoJob.Notes | Where-Object { $_ -like 'job_*' }).Count -eq 0)
    $stBadJob = New-TestState
    $stBadJob.Job = [IntPtr]::Zero
    $fp2 = New-FakeProc 556
    Add-SmokeOwnedProcess $stBadJob $fp2
    Check 'job_neg' 'failed_assignment_is_reported_as_a_note' (@($stBadJob.Notes | Where-Object { $_ -like 'job_assign_failed:556' }).Count -eq 1)
    $stBadJob.Job = $null

    # ================= ISARET DOSYASI (marker) ve YETIM KURTARMA =================
    $mk = Join-Path $work 'marker'; [void][IO.Directory]::CreateDirectory($mk)
    $mkSaved = Join-Path $work 'mk_saved'
    foreach ($d in 'SaveGames', 'Logs') { [void][IO.Directory]::CreateDirectory((Join-Path $mkSaved $d)) }
    function New-MarkerFile {
        param([string]$Dir, [string]$Name, $Runner, $Procs, $RO, [string]$SavedRoot)
        $o = [ordered]@{ format = 1; repo_root = 'x'; saved_root = $SavedRoot; runner = $Runner; processes = @($Procs); readonly_paths = @($RO) }
        [IO.File]::WriteAllText((Join-Path $Dir $Name), (ConvertTo-Json -InputObject $o -Depth 6), (New-Object Text.UTF8Encoding($false)))
    }
    function MkId { param([int]$ProcId, [string]$Name, $Start) return [ordered]@{ role = 'owned'; pid = $ProcId; name = $Name; start_utc = $Start.ToString('o') } }
    $deadRunner = [ordered]@{ pid = 2000001; name = 'powershell'; start_utc = $t0.ToString('o') }
    $script:lk = @{}
    $lookup = { param($ProcId) if ($script:lk.ContainsKey([int]$ProcId)) { $script:lk[[int]$ProcId] } else { $null } }
    # marker yazma / silme yasam dongusu
    $stM = New-TestState
    $stM.MarkerPath = Join-Path $mk 'smoke_run_marker_0000aaaa.json'
    $stM.Marker = [ordered]@{ format = 1; repo_root = 'x'; saved_root = $mkSaved; runner = (Get-SmokeProcIdentity ([Diagnostics.Process]::GetCurrentProcess()) 'runner') }
    Check 'marker' 'initial_write_succeeds' (Update-SmokeMarker $stM)
    $fpm = New-FakeProc 777 $false $false $t0
    Add-SmokeOwnedProcess $stM $fpm
    $mj = [IO.File]::ReadAllText($stM.MarkerPath) | ConvertFrom-Json
    Check 'marker' 'process_identity_is_recorded' (@($mj.processes).Count -eq 1 -and $mj.processes[0].pid -eq 777 -and $mj.processes[0].name -ceq 'UnrealEditor-Cmd' -and $mj.runner.pid -eq $PID)
    $roFile = Join-Path $mkSaved 'SaveGames\MMOSave_Mk.sav'
    [IO.File]::WriteAllBytes($roFile, [byte[]](1..16))
    Set-SmokeReadOnly $stM $roFile
    $mj = [IO.File]::ReadAllText($stM.MarkerPath) | ConvertFrom-Json
    Check 'marker' 'readonly_path_is_recorded' (@($mj.readonly_paths).Count -eq 1 -and ([string]$mj.readonly_paths[0]) -ceq $roFile)
    Check 'marker' 'no_tmp_file_left_behind' (-not (Test-Path -LiteralPath ($stM.MarkerPath + '.tmp')))
    Invoke-SmokeCleanup $stM
    Check 'marker' 'removed_after_clean_cleanup' (-not (Test-Path -LiteralPath $stM.MarkerPath))
    $stM2 = New-TestState
    $stM2.MarkerPath = Join-Path $mk 'smoke_run_marker_0000bbbb.json'
    $stM2.Marker = [ordered]@{ format = 1 }
    [void](Update-SmokeMarker $stM2)
    Add-SmokeOwnedProcess $stM2 (New-FakeProc 778 $false $true $t0)
    Invoke-SmokeCleanup $stM2
    Check 'marker_neg' 'kept_when_cleanup_has_problems' ((Test-Path -LiteralPath $stM2.MarkerPath) -and $stM2.Problems.Count -gt 0)
    [IO.File]::Delete($stM2.MarkerPath)
    Check 'marker_neg' 'unwritable_marker_location_reports_false' (-not (Update-SmokeMarker (@{ MarkerPath = (Join-Path $evSrv 'sub\m.json'); Marker = [ordered]@{ format = 1 }; PlayerName = $null; MarkerProcs = (New-Object System.Collections.ArrayList); ReadOnly = (New-Object System.Collections.ArrayList) })))
    Check 'marker' 'path_is_stable_per_repo_and_differs_between_repos' (((Get-SmokeMarkerPath $mk 'C:\Repo\A') -ceq (Get-SmokeMarkerPath $mk 'c:\repo\a\')) -and ((Get-SmokeMarkerPath $mk 'C:\Repo\A') -cne (Get-SmokeMarkerPath $mk 'C:\Repo\B')) -and ((Split-Path -Leaf (Get-SmokeMarkerPath $mk 'C:\Repo\A')) -cmatch '^smoke_run_marker_[0-9a-f]{8}\.json$'))

    # ---- kurtarma: sahte surec nesneleriyle ----
    function Reset-Mk { Get-ChildItem -LiteralPath $mk -File | ForEach-Object { [IO.File]::Delete($_.FullName) }; $script:lk.Clear() }
    # 1) gercek yetim: kimlik eslesir -> oldurulur, salt-okunur kalkar, isaret kenara alinir
    Reset-Mk
    $orph = New-FakeProc 4242 $false $false $t0 'UnrealEditor-Cmd'
    $script:lk[4242] = $orph
    $ro1 = Join-Path $mkSaved 'SaveGames\MMOSave_Orph.sav'
    [IO.File]::WriteAllBytes($ro1, [byte[]](1..16)); [IO.File]::SetAttributes($ro1, [IO.FileAttributes]::ReadOnly)
    New-MarkerFile $mk 'smoke_run_marker_00000001.json' $deadRunner @((MkId 4242 'UnrealEditor-Cmd' $t0)) @($ro1) $mkSaved
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T1'
    Check 'recovery' 'orphan_with_matching_identity_is_killed' ($orph.Killed -eq 1 -and $rec.Killed.Count -eq 1)
    Check 'recovery' 'readonly_flag_cleared' ((([IO.File]::GetAttributes($ro1)) -band [IO.FileAttributes]::ReadOnly) -eq 0 -and $rec.Restored.Count -eq 1)
    Check 'recovery' 'marker_moved_aside_as_stale' ((-not (Test-Path -LiteralPath (Join-Path $mk 'smoke_run_marker_00000001.json'))) -and (Test-Path -LiteralPath (Join-Path $mk 'smoke_run_marker_00000001.json.stale.T1')) -and $rec.Problems.Count -eq 0 -and -not $rec.RunnerAlive)
    # 2) PID yeniden kullanimi: ayni PID ama baska baslangic zamani
    Reset-Mk
    $reusedP = New-FakeProc 4243 $false $false $t0.AddHours(5) 'UnrealEditor-Cmd'
    $script:lk[4243] = $reusedP
    New-MarkerFile $mk 'smoke_run_marker_00000002.json' $deadRunner @((MkId 4243 'UnrealEditor-Cmd' $t0)) @() $mkSaved
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T2'
    Check 'recovery_neg' 'pid_reuse_start_mismatch_not_killed' ($reusedP.Killed -eq 0 -and $rec.Killed.Count -eq 0 -and @($rec.Skipped | Where-Object { $_ -like 'identity_mismatch:4243' }).Count -eq 1)
    # 3) ayni PID, baska ad
    Reset-Mk
    $other = New-FakeProc 4244 $false $false $t0 'notepad'
    $script:lk[4244] = $other
    New-MarkerFile $mk 'smoke_run_marker_00000003.json' $deadRunner @((MkId 4244 'UnrealEditor-Cmd' $t0)) @() $mkSaved
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T3'
    Check 'recovery_neg' 'name_mismatch_not_killed' ($other.Killed -eq 0)
    # 4) isaret dosyasi UnrealEditor olmayan bir ad iddia eder (kurcalanmis): asla oldurulmez
    Reset-Mk
    $victim = New-FakeProc 4245 $false $false $t0 'explorer'
    $script:lk[4245] = $victim
    New-MarkerFile $mk 'smoke_run_marker_00000004.json' $deadRunner @((MkId 4245 'explorer' $t0)) @() $mkSaved
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T4'
    Check 'recovery_neg' 'tampered_marker_naming_a_non_unreal_process_never_kills' ($victim.Killed -eq 0 -and @($rec.Skipped | Where-Object { $_ -like 'bad_identity:4245' }).Count -eq 1)
    # 5) kendi PID'imiz
    Reset-Mk
    $selfFake = New-FakeProc $PID $false $false $t0 'UnrealEditor-Cmd'
    $script:lk[$PID] = $selfFake
    New-MarkerFile $mk 'smoke_run_marker_00000005.json' $deadRunner @((MkId $PID 'UnrealEditor-Cmd' $t0)) @() $mkSaved
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T5'
    Check 'recovery_neg' 'own_pid_is_never_killed' ($selfFake.Killed -eq 0)
    # 6) kosucu hala yasiyor
    Reset-Mk
    $liveRunner = New-FakeProc 4300 $false $false $t0 'powershell'
    $script:lk[4300] = $liveRunner
    $orph2 = New-FakeProc 4301 $false $false $t0 'UnrealEditor-Cmd'
    $script:lk[4301] = $orph2
    New-MarkerFile $mk 'smoke_run_marker_00000006.json' ([ordered]@{ pid = 4300; name = 'powershell'; start_utc = $t0.ToString('o') }) @((MkId 4301 'UnrealEditor-Cmd' $t0)) @() $mkSaved
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T6'
    Check 'recovery_neg' 'live_runner_marker_not_touched' ($rec.RunnerAlive -and $orph2.Killed -eq 0 -and (Test-Path -LiteralPath (Join-Path $mk 'smoke_run_marker_00000006.json')))
    # 7) kosucu PID'i yeniden kullanilmis (baska baslangic zamani): yetim sayilir
    Reset-Mk
    $reusedRunner = New-FakeProc 4302 $false $false $t0.AddHours(9) 'powershell'
    $script:lk[4302] = $reusedRunner
    $orph3 = New-FakeProc 4303 $false $false $t0 'UnrealEditor-Cmd'
    $script:lk[4303] = $orph3
    New-MarkerFile $mk 'smoke_run_marker_00000007.json' ([ordered]@{ pid = 4302; name = 'powershell'; start_utc = $t0.ToString('o') }) @((MkId 4303 'UnrealEditor-Cmd' $t0)) @() $mkSaved
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T7'
    Check 'recovery' 'reused_runner_pid_is_an_orphan_marker' ((-not $rec.RunnerAlive) -and $orph3.Killed -eq 1)
    # 8) salt-okunur yol beyaz liste disinda
    Reset-Mk
    $roLog = Join-Path $mkSaved 'Logs\keep.log'
    [IO.File]::WriteAllBytes($roLog, [byte[]](1..8)); [IO.File]::SetAttributes($roLog, [IO.FileAttributes]::ReadOnly)
    $roOut = Join-Path $work 'mk_outside.sav'
    [IO.File]::WriteAllBytes($roOut, [byte[]](1..8)); [IO.File]::SetAttributes($roOut, [IO.FileAttributes]::ReadOnly)
    New-MarkerFile $mk 'smoke_run_marker_00000008.json' $deadRunner @() @($roLog, $roOut) $mkSaved
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T8'
    Check 'recovery_neg' 'non_character_readonly_path_untouched' ((([IO.File]::GetAttributes($roLog)) -band [IO.FileAttributes]::ReadOnly) -ne 0)
    Check 'recovery_neg' 'readonly_path_outside_saved_root_untouched' ((([IO.File]::GetAttributes($roOut)) -band [IO.FileAttributes]::ReadOnly) -ne 0)
    [IO.File]::SetAttributes($roLog, [IO.FileAttributes]::Normal); [IO.File]::SetAttributes($roOut, [IO.FileAttributes]::Normal)
    # 9) oldurme basarisiz: isaret yerinde kalir, sorun bildirilir
    Reset-Mk
    $stubborn = New-FakeProc 4246 $false $true $t0 'UnrealEditor-Cmd'
    $script:lk[4246] = $stubborn
    New-MarkerFile $mk 'smoke_run_marker_00000009.json' $deadRunner @((MkId 4246 'UnrealEditor-Cmd' $t0)) @() $mkSaved
    $rec = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T9'
    Check 'recovery_neg' 'kill_failure_is_a_problem_and_marker_stays' ($rec.Problems.Count -eq 1 -and (Test-Path -LiteralPath (Join-Path $mk 'smoke_run_marker_00000009.json')))
    # 10) bozuk isaret
    Reset-Mk
    [IO.File]::WriteAllText((Join-Path $mk 'smoke_run_marker_0000000a.json'), '{not json')
    $recC = $null
    $threwC = $false
    try { $recC = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup -Stamp 'T10' } catch { $threwC = $true }
    Check 'recovery_neg' 'corrupt_marker_does_not_throw_and_is_moved_aside' ((-not $threwC) -and (Test-Path -LiteralPath (Join-Path $mk 'smoke_run_marker_0000000a.json.corrupt.T10')))
    # 11) alakasiz dosyalar ve bos / olmayan dizin
    Reset-Mk
    [IO.File]::WriteAllText((Join-Path $mk 'other.json'), '{}')
    $recN = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -ProcessLookup $lookup
    Check 'recovery_neg' 'unrelated_files_are_ignored' ($recN.Found -eq 0 -and (Test-Path -LiteralPath (Join-Path $mk 'other.json')))
    Check 'recovery_neg' 'missing_directory_is_nothing_to_do' ((Invoke-SmokeOrphanRecovery -MarkerDirectory (Join-Path $work 'no_marker_dir')).Found -eq 0)

    # ---- kurtarma: GERCEK surec (UnrealEditor-Cmd.exe adli zararsiz bir kopya) ----
    Reset-Mk
    $standDir = Join-Path $work 'standin'; [void][IO.Directory]::CreateDirectory($standDir)
    $standExe = Join-Path $standDir 'UnrealEditor-Cmd.exe'
    Copy-Item -LiteralPath (Join-Path $PSHOME 'powershell.exe') -Destination $standExe
    $stand = Start-Sleeper -Exe $standExe
    $standOk = Wait-Until -Seconds 10 -Cond { (Test-PidAlive $stand.Id) -and ([string]$stand.ProcessName -ceq 'UnrealEditor-Cmd') }
    Check 'real_recovery' 'stand_in_process_runs_under_the_unreal_name' $standOk
    $idReal = Get-SmokeProcIdentity $stand 'owned'
    $idWrong = [ordered]@{ role = 'owned'; pid = $idReal.pid; name = $idReal.name; start_utc = ([datetime]$idReal.start_utc).ToUniversalTime().AddHours(-1).ToString('o') }
    New-MarkerFile $mk 'smoke_run_marker_0000000b.json' $deadRunner @($idWrong) @() $mkSaved
    $recW = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -Stamp 'T11'
    Check 'real_recovery_neg' 'wrong_start_time_leaves_the_real_process_alone' ((Test-PidAlive $stand.Id) -and $recW.Killed.Count -eq 0)
    Reset-Mk
    New-MarkerFile $mk 'smoke_run_marker_0000000c.json' $deadRunner @($idReal) @() $mkSaved
    $recR = Invoke-SmokeOrphanRecovery -MarkerDirectory $mk -Stamp 'T12'
    Check 'real_recovery' 'matching_pid_name_and_start_time_kills_the_real_orphan' ($standOk -and $recR.Killed.Count -eq 1 -and (Wait-Until -Seconds 10 -Cond { -not (Test-PidAlive $stand.Id) }))

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
    $mkGate = Join-Path $work 'marker_gate'
    $g = Run-Runner @('-Scenario', 'bogus', '-RepoRoot', $rootOk, '-ExePath', $fakeExe)
    Check 'gate' 'unknown_scenario_is_usage_error_2' ($g.Code -eq 2 -and $g.Out -like '*usage Scenario*')
    $g = Run-Runner @('-Scenario', 'loginclosed', '-RepoRoot', $rootOk, '-ExePath', $fakeExe)
    Check 'gate' 'loginclosed_is_accepted_then_main_checkout_refused_defer_3' ($g.Code -eq 3 -and $g.Out -like '*main_checkout_refused*' -and $g.Out -notlike '*usage Scenario*')
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
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $rootOk, '-ExePath', $fakeExe, '-MarkerDirectory', (Join-Path $rootOk 'marker'))
    Check 'gate' 'marker_directory_inside_repo_is_usage_error_2' ($g.Code -eq 2 -and $g.Out -like '*MarkerDirectory*')
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

    # ---- gercek (gecici) git deposu: kayitli worktree / sahte '.git' dosyasi ----
    $gm = Join-Path $work 'gitmain'; [void][IO.Directory]::CreateDirectory($gm)
    $ErrorActionPreference = 'Continue'
    & git -C $gm init -q 2>&1 | Out-Null
    & git -C $gm -c user.name=t -c user.email=t@t.t commit -q --allow-empty -m init 2>&1 | Out-Null
    $gwt = Join-Path $work 'gitwt'
    & git -C $gm worktree add -q --detach $gwt 2>&1 | Out-Null
    $gitOk = ($LASTEXITCODE -eq 0)
    $ErrorActionPreference = 'Stop'
    foreach ($rt in @($gwt)) {
        [void][IO.Directory]::CreateDirectory((Join-Path $rt 'Binaries\Win64'))
        [IO.File]::WriteAllText((Join-Path $rt 'MyProject.uproject'), '{}'); [IO.File]::WriteAllText((Join-Path $rt 'Binaries\Win64\UnrealEditor-MyProject.dll'), 'x')
    }
    $gfake = Join-Path $work 'gitfake'
    [void][IO.Directory]::CreateDirectory((Join-Path $gfake 'Binaries\Win64'))
    [IO.File]::WriteAllText((Join-Path $gfake '.git'), ('gitdir: ' + ((Join-Path $gm '.git').Replace('\', '/'))))
    [IO.File]::WriteAllText((Join-Path $gfake 'MyProject.uproject'), '{}'); [IO.File]::WriteAllText((Join-Path $gfake 'Binaries\Win64\UnrealEditor-MyProject.dll'), 'x')
    Check 'gate' 'harness_created_a_real_linked_worktree' ($gitOk -and (Test-Path -LiteralPath (Join-Path $gwt '.git') -PathType Leaf))
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $gfake, '-ExePath', $fakeExe, '-MarkerDirectory', $mkGate)
    Check 'gate' 'unregistered_worktree_defers_3' ($g.Code -eq 3 -and $g.Out -like '*worktree_not_registered*' -and $g.Out -like '*nothing started*')
    $udp = New-Object Net.Sockets.UdpClient(0)
    $udpPort = ([Net.IPEndPoint]$udp.Client.LocalEndPoint).Port
    $g = Run-Runner @('-Scenario', 'clean', '-RepoRoot', $gwt, '-ExePath', $fakeExe, '-Port', [string]$udpPort, '-MarkerDirectory', $mkGate)
    $udp.Close()
    Check 'gate' 'registered_worktree_passes_the_registration_gate_then_defers_on_busy_or_port' ($g.Code -eq 3 -and $g.Out -notlike '*worktree_not_registered*' -and $g.Out -notlike '*main_checkout_refused*' -and $g.Out -notlike '*git_common_dir_unresolved*' -and $g.Out -like '*nothing started*')
    Check 'gate' 'deferred_gate_runs_leave_no_marker_behind' (@(Get-ChildItem -LiteralPath $mkGate -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'smoke_run_marker_*.json' }).Count -eq 0)
    Check 'gate' 'gates_create_no_log_directory_before_deferring' (@(Get-ChildItem -LiteralPath $env:TEMP -Directory -Filter 'mmo_l4_*' -ErrorAction SilentlyContinue).Count -eq $l4Before)

    # ================= KAYNAK TARAMASI (ad ile surec oldurme, kod degistirme, dogrudan silme yasagi, sira) =================
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
    Check 'lint' 'main_has_no_direct_file_deletion' (($mainSrc -notmatch 'Remove-Item') -and ($mainSrc -notmatch '\.Delete\(') -and ($mainSrc -notmatch '\[IO\.File\]::Move'))
    Check 'lint' 'main_deletes_only_through_guarded_helper' ($mainSrc -match 'Remove-SmokeCreatedFiles -SavedRoot \$saved -Before \$snapshotBefore -PlayerName \$script:playerName')
    Check 'lint' 'removal_helper_checks_under_saved_and_reparse_chain' ($libSrc -match 'if \(\(Test-SmokePathUnder \$SavedRoot \$f\) -and -not \(Test-SmokePathHasReparse \$SavedRoot \$f\)\) \{')
    Check 'lint' 'created_selection_uses_exact_name_group_not_substring' (($libSrc -match 'if \(\$c\.Group -cne \$canon\) \{ continue \}') -and ($libSrc -notmatch 'IndexOf\(\$PlayerName'))
    Check 'lint' 'main_uses_guarded_wrapper' ($mainSrc -match 'Invoke-SmokeGuarded -State \$state -Body')
    Check 'lint' 'main_injects_via_set_smoke_readonly' ($mainSrc -match 'Set-SmokeReadOnly \$state \$tmpPath')
    Check 'lint' 'main_savefail_locks_tmp_with_fileshare_none_and_registers_lock' (($mainSrc -match '\[IO\.FileShare\]::None') -and ($mainSrc -match 'Add-SmokeLock \$state \$lockHandle'))
    Check 'lint' 'main_savefail_never_makes_the_main_sav_readonly' ($mainSrc -notmatch 'Set-SmokeReadOnly \$state \$script:savePath')
    Check 'lint' 'main_refuses_main_checkout' ($mainSrc -match 'Test-SmokeIsMainCheckout \$RepoRoot\) -and -not \$AllowMainCheckout')
    Check 'lint' 'main_defers_unregistered_worktree' ($mainSrc -match 'if \(-not \(Test-SmokeIsRegisteredLinkedWorktree \$RepoRoot \(\$wtList -join "`n"\)\)\) \{ Defer ''worktree_not_registered')
    Check 'lint' 'main_defers_when_worktree_list_fails' ($mainSrc -match 'if \(\$wtListCode -ne 0\) \{ Defer ''worktree_list_unresolved''')
    Check 'lint' 'main_honors_agent_build_lock' ($mainSrc -match 'agent-build\.lock')
    Check 'lint' 'main_defers_on_busy_processes' ($mainSrc -match 'Get-SmokeBusyProcesses -Inventory')
    Check 'lint' 'main_defers_when_busy_found' ($mainSrc -match 'if \(\$busy\.Count -gt 0\) \{ Defer \(''ue_or_build_process_active')
    Check 'lint' 'main_defers_when_port_in_use' ($mainSrc -match 'if \(\$used\.ContainsKey\(\$Port\)\) \{ Defer \(''port_in_use')
    Check 'lint' 'main_runs_orphan_recovery_before_busy_check' (($mainSrc.IndexOf('Invoke-SmokeOrphanRecovery -MarkerDirectory') -gt 0) -and ($mainSrc.IndexOf('Invoke-SmokeOrphanRecovery -MarkerDirectory') -lt $mainSrc.IndexOf('Get-SmokeBusyProcesses -Inventory')))
    Check 'lint' 'main_defers_on_recovery_problems_or_live_runner' (($mainSrc -match 'if \(\$rec\.RunnerAlive\) \{ Defer ''another_smoke_runner_active''') -and ($mainSrc -match 'if \(\$rec\.Problems\.Count -gt 0\) \{ Defer \(''orphan_recovery_failed'))
    Check 'lint' 'main_defers_when_port_hold_fails' ($mainSrc -match 'if \(\$null -eq \$portHold\) \{ Defer \(''port_not_bindable')
    Check 'lint' 'main_verifies_real_listening_port' (($mainSrc -match 'Get-SmokeListeningPort') -and ($mainSrc -match 'if \(\$lp\.Count -ne 1 -or \$lp\[0\] -ne \$selected\) \{ \[void\]\$script:runnerInc\.Add\(''listen_port_mismatch'))
    Check 'lint' 'main_rejects_non_loopback_bind' ($mainSrc.Contains("[void]`$script:runnerInc.Add('server_not_loopback_bound')"))
    Check 'lint' 'main_creates_job_object_and_marker_before_starting_processes' (($mainSrc -match '\$state\.Job = New-SmokeJob') -and ($mainSrc.IndexOf('Update-SmokeMarker $state') -gt 0) -and ($mainSrc.IndexOf('Update-SmokeMarker $state') -lt $mainSrc.IndexOf('Start-Process')) -and ($mainSrc.IndexOf('$state.Job = New-SmokeJob') -lt $mainSrc.IndexOf('Start-Process')))
    Check 'lint' 'main_defers_when_marker_unwritable' ($mainSrc -match 'Defer \(''marker_not_writable')
    Check 'lint' 'main_never_builds' ($codeOnly -notmatch '(?i)Build\.bat|UnrealBuildTool\.exe|dotnet ')
    Check 'lint' 'main_passes_player_name_from_log_to_evaluator' ($mainSrc -match '-PlayerA \$script:playerName')
    Check 'lint' 'main_kills_client_itself_after_server_exit' ($mainSrc -match 'Stop-SmokeOwnedProcess \$state \$cli(?!B)')
    Check 'lint' 'main_gives_client_a_self_exit_safety_net' ($mainSrc -match 'Get-SmokeClientArgs -Project \$project -Port \$selected -LogPath \$clientLog -MaxSeconds')
    Check 'lint' 'main_enforces_hard_cap_in_exit_wait' ($mainSrc -match "while \(-not \`$srv\.HasExited\) \{\s*\`$d = Get-SmokeTimeDecision \`$clock\.Elapsed\.TotalSeconds \`$HardCapSeconds\r?\n\s*if \(\`$d -ne 'ok'\) \{ \[void\]\`$script:runnerFails\.Add\('hard_cap_exceeded'\)")
    Check 'lint' 'main_checks_dedicated_netmode' ($mainSrc -match 'NetMode -ne 1')
    Check 'lint' 'main_resolves_final_verdict_with_runner_reasons' ($mainSrc -match 'Resolve-SmokeFinalVerdict -EvidenceVerdict \$evVerdict -RunnerFails')
    Check 'lint' 'save_metadata_captured_before_cleanup_deletes_it' ($mainSrc.Contains('$script:saveInfo = [pscustomobject]') -and $mainSrc.Contains('-SaveFileInfo $script:saveInfo'))
    Check 'lint' 'main_captures_evidence_inside_guarded_body_before_cleanup' ($mainSrc -match '(?s)Invoke-SmokeGuarded -State \$state -Body \{.*\$script:evidence = Get-SmokeEvidenceBundle -ServerLog \$serverLog -ClientLog \$clientLog -ResultJson \$resultJson -CopyDirectory \$LogDirectory.*\r?\n\} catch \{')
    Check 'lint' 'main_verdict_uses_the_captured_evidence_only' ($mainSrc.Contains('-ServerLogText $script:evidence.ServerText -ClientLogText $script:evidence.ClientText -ResultJsonText $script:evidence.JsonText'))
    Check 'lint' 'main_checks_save_path_reparse_chain' ($mainSrc -match 'Test-SmokePathHasReparse \$saved \$script:savePath')
    Check 'lint' 'main_refuses_preexisting_player_save' ($mainSrc -match 'if \(@\(\$snapshotBefore \| Where-Object \{ \$_ -ieq \$script:savePath \}\)\.Count -gt 0\) \{ \[void\]\$script:runnerInc\.Add\(''player_save_preexisting')
    $stopBody = $libSrc.Substring($libSrc.IndexOf('function Stop-SmokeOwnedProcess'))
    $stopBody = $stopBody.Substring(0, $stopBody.IndexOf('function Set-SmokeReadOnly'))
    Check 'lint' 'tree_kill_opens_handles_before_killing_the_parent' (($stopBody.IndexOf('& $State.OpenProcess') -gt 0) -and ($stopBody.IndexOf('& $State.OpenProcess') -lt $stopBody.IndexOf('$Proc.Kill()')))
    Check 'lint' 'tree_kill_verifies_identity_before_holding_a_handle' ($stopBody.Contains('Test-SmokeSameInstant $hs $d.Creation'))
    Check 'lint' 'descendants_require_child_not_older_than_parent' ($libSrc.Contains('if ($cr -lt $cur.Creation.AddMilliseconds(-$script:SmokeTolMs)) { continue }'))
    Check 'lint' 'job_object_uses_kill_on_job_close_flag' ($libSrc.Contains('LimitFlags = 0x2000'))
    # ---- S4 loginclosed: kosucu mantigi (kaynak taramasi; gercek surec yok) ----
    $b4 = $mainSrc.IndexOf('if ($Scenario -ceq ''loginclosed'') {' + "`r`n" + '            while ($true) {')
    Check 'lint' 'loginclosed_block_present' ($b4 -gt 0)
    $startB = $mainSrc.IndexOf('$cliB = Start-Process')
    $closingBreak = $mainSrc.IndexOf('if (Test-SmokeClosingSeen $slogText) { break }')
    Check 'lint' 'client_b_starts_only_after_closing_seen_in_server_log' ($startB -gt 0 -and $closingBreak -gt 0 -and $closingBreak -lt $startB)
    Check 'lint' 'client_b_wait_for_closing_is_hard_capped' ($mainSrc -match "(?s)if \(\`$Scenario -ceq 'loginclosed'\) \{\s*while \(\`$true\) \{\s*\`$d = Get-SmokeTimeDecision \`$clock\.Elapsed\.TotalSeconds \`$HardCapSeconds\s*if \(\`$d -ne 'ok'\) \{ \[void\]\`$script:runnerFails\.Add\('hard_cap_exceeded'\)")
    Check 'lint' 'client_b_registered_in_job_and_marker_via_owned_helper' ($mainSrc.Contains('Add-SmokeOwnedProcess $state $cliB'))
    Check 'lint' 'client_b_stopped_by_runner_itself' ($mainSrc.Contains('Stop-SmokeOwnedProcess $state $cliB'))
    Check 'lint' 'client_b_has_self_exit_safety_net' ($mainSrc.Contains('Get-SmokeClientArgs -Project $project -Port $selected -LogPath $clientBLog -MaxSeconds ($HardCapSeconds + $ClientSelfExitMargin)'))
    Check 'lint' 'client_b_log_is_a_separate_file' ($mainSrc.Contains("Join-Path `$LogDirectory 'clientB.log'") -and $mainSrc.Contains('-ClientBLog $clientBLog'))
    Check 'lint' 'client_b_log_and_reject_pattern_go_to_the_evaluator' ($mainSrc.Contains("`$bArgs['ClientBLogText'] = `$script:evidence.ClientBText") -and $mainSrc.Contains("`$bArgs['ClientRejectPattern'] = `$ClientRejectPattern") -and $mainSrc.Contains("-ClientKickPattern 'ControlChannelClose' @bArgs"))
    Check 'lint' 'client_b_unresolved_before_flushing_is_inconclusive_not_pass' ($mainSrc.Contains("elseif (Test-SmokePhaseSeen `$slogText 'Flushing') { [void]`$script:runnerInc.Add('client_b_no_prelogin_before_flushing'); return }"))
    Check 'lint' 'client_b_wait_has_its_own_budget' ($mainSrc.Contains('$phase.Elapsed.TotalSeconds $ClientBRejectBudgetSeconds'))
    Check 'lint' 'client_b_rejection_is_read_from_server_log_not_assumed' ($mainSrc.Contains('(Get-SmokePreloginRejectedCount $slogText) -ge 1'))
    Check 'lint' 'client_b_extra_join_is_left_to_the_evaluator_to_fail' ($mainSrc.Contains('if ($jn.Invalid -or $jn.Count -gt 1) {'))
    Check 'lint' 'client_b_is_loginclosed_only' (($mainSrc.Split(@('$cliB = Start-Process'), [StringSplitOptions]::None).Count -eq 2) -and ($b4 -gt 0) -and ($b4 -lt $startB))
    Check 'lint' 'sources_are_ascii_only'(([regex]::Matches($bothSrc, '[^\x00-\x7F]')).Count -eq 0)
}
catch {
    Write-Host ('[MMO_SMOKE_INFO] fatal_exception ' + $_.Exception.Message + ' at ' + $_.InvocationInfo.ScriptLineNumber)
    Check 'harness' 'fatal_exception' $false
}
finally {
    foreach ($sp in @($script:spawned.ToArray())) { try { if (-not $sp.HasExited) { $sp.Kill() } } catch { } }
    Start-Sleep -Milliseconds 500
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
