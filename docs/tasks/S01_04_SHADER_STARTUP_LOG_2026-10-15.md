# S01.04: bounded shader startup logging preparation

Task date: 2026-10-15; completed handoff: 2026-10-16 (Europe/Istanbul).
Implementation status: frozen for parent review and Franklin QA. UE/runtime: NOT_RUN.

This opening status describes the worker handoff. Coordinator acceptance,
recovered test-exit evidence and the subsequent diagnostic measurement are
recorded separately below; the historical worker results are retained as given.

## Ownership and baseline

Worktree: `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject`.
HEAD: `4e0f0c5a5ede948b69c324953f96b88a3ec61dc9`.
Main checkout supplied by parent: `142507bacc103762ef0d28ab14457369bce80ac7`.
The accepted S01.02 scripts and local 141-input module were already present.
The implementation continued the partial S01.04 changes across the resume;
no duplicate implementation or descendant dispatch was performed.

Owned writes are exactly these four scripts and this new note:

- `Scripts/NetSmokeTest.ps1`
- `Scripts/TestNetSmokeRunner.ps1`
- `Scripts/InvokeWinterNetSmoke.ps1`
- `Scripts/TestWinterNetSmokeWrapper.ps1`
- `docs/tasks/S01_04_SHADER_STARTUP_LOG_2026-10-15.md`

Inherited `build.ps1`, `TestBuildScript.ps1`, S00/S01.02 notes, S00.05,
peer Race/Trade edits, and the new unowned MMOZoneLayout files are outside
this task. No main, Source, Config, assets, engine, or actual worktree Saved
writes were performed. Test-generated repositories, fake Saved/DLL/assets,
and logs remain under TEMP, using the existing fixture harnesses. Fixture
Git commits are synthetic test setup; no project commit was made.

Pre-S01.04 SHA256 values, captured before editing:

| Script | Accepted starting SHA256 |
| --- | --- |
| NetSmokeTest.ps1 | `4286BF38A4DD8491D1E1348C98B0C6C4BB830C9E03727B2BA0A87909C96A12DC` |
| TestNetSmokeRunner.ps1 | `42B2BFFC5B4CC4AF277C073D5ABE350A567E68A1EC39774C612ACCD37DB7598E` |
| InvokeWinterNetSmoke.ps1 | `B04DF0FC6E4FC7FE0CBA4A09F4D3530B605F56FF62BB7BF4CB3ED4CE99EA3460` |
| TestWinterNetSmokeWrapper.ps1 | `AC13180EBEDB0E9391B182CA19D91F78FF8A26DD658BCC1B94B763A453C0C3B1` |

## Delivered behavior

`-ShaderStartupDiagnostics` is an opt-in switch, accepted only with
`-ServerMode EditorDedicated`. Listen plus diagnostics is rejected before
launch; the wrapper rejects before proof-directory creation, lock, or child.
The invocation helper also rejects that combination directly.

The child adds exactly one fixed server argv element:
`-LogCmds="LogShaders Log"`. Client argv receives no LogCmds element.
There is no arbitrary argument passthrough or additional diagnostic option.
Omitting the switch preserves launch argv and timing defaults.

Child `processes.json` and wrapper `result.json` record boolean
`shaderStartupDiagnostics`; wrapper `requestedChildArguments` includes
`-ShaderStartupDiagnostics` only when requested. The child contract is checked
for that parameter when diagnostics are selected. Raw arrays remain the
arrays passed to Start-Process. Acceptance checks require diagnostic intent,
one exact server token, no client token, and matching native server command
evidence. Missing/altered/duplicate native LogCmds evidence is rejected.

On the diagnostic path, polling retains the native `LogInit: Command Line:`
line even if readiness never arrives. `serverCommandLineVerified` still
requires the existing readiness and command checks; recording argv does not
prove readiness or runtime NM_DedicatedServer.

The original total clock remains 95 seconds, with 30 seconds server
observation, 45 seconds client observation, and 5 seconds reserved startup
overhead; no fresh clock or deadline extension was added. Original exact
map/gameplay-port checks, provenance, common lock, isolated UserDir, Saved
protection, and owned-handle cleanup remain in place. No fallback was added.

The existing child fixture runner now accepts optional `-Case [string[]]`.
It runs only explicit selected cases, rejects empty/blank/unknown/duplicate
selections, and reports FILTERED versus FULL. With no filter it retains the
full case catalog. Unknown/duplicate selections can create the existing
temporary Git fixture before rejection, but launch no mocked cases.

## Recorded verification

Evidence root:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/s01_04_script_checks_512cd1e455ca444e8b37c6beb6c57c66`.
All commands below ran from the isolated worktree. These are script fixtures,
not UE, network, PIE, build, or runtime acceptance.

| Check | Actual result | Exit evidence | Retained log |
| --- | --- | --- | --- |
| Child runner, FILTERED | 327 checks, 12 of 93 cases, 0 failures | Observed shell exit 0 | `child-filtered.log` |
| Wrapper, Windows PowerShell 5 | FULL wrapper fixture suite, 262 checks, 0 failures; actual receipt 141 inputs | Completion summary retained; original shell exit NOT_RETAINED after resume | `wrapper-ps5.log` |
| Wrapper, PowerShell 7 | FULL wrapper fixture suite, 262 checks, 0 failures; actual receipt 141 inputs | Completion summary retained; original shell exit NOT_RETAINED after resume | `wrapper-ps7.log` |
| Filter rejection | 4 checks, 0 failures; no mocked cases launched | Observed shell exit 0 | `filter-validation.log` |
| Final static verification | 4 AST + 4 trailing-whitespace + 1 tracked diff checks, 0 failures | Observed shell exit 0 | `static-freeze.log` |

The wrapper logs reach the final zero-failure summary and Evidence line;
their source exits 0 after that branch. This is not a retained observation of
the original native exit. Both sessions had expired when resumed; they were
not replayed merely to recover exit metadata. The 1,423-check S01.02 child
suite was NOT rerun, and its result is not claimed for S01.04.

Exact child test command (PowerShell 7 host; existing harness launches its
mock children under Windows PowerShell):

```powershell
$caseList=@('isolated_dedicated_diag_summer','isolated_dedicated_diag_missing_logcmd','isolated_dedicated_diag_wrong_logcmd','isolated_dedicated_diag_duplicate_logcmd','isolated_dedicated_diag_server_sleep_fail','isolated_dedicated_diag_readiness_never','bad_diag_listen','isolated_dedicated_summer','isolated_dedicated_winter','isolated_listen_summer','healthy','healthy_again'); & './Scripts/TestNetSmokeRunner.ps1' -Case $caseList 2>&1 | Tee-Object -FilePath 'C:\Users\OZAN CIDIK\AppData\Local\Temp\s01_04_script_checks_512cd1e455ca444e8b37c6beb6c57c66/child-filtered.log'; exit $LASTEXITCODE
```

Selected cases exercise fixed server-only argv, absent default flags,
native missing/wrong/duplicate LogCmds rejection, observation failure cleanup,
readiness-timeout native-argv retention and cleanup, invalid Listen refusal,
normal Summer/Winter dedicated routes, and legacy Listen/fresh-log behavior.
Per-case native script exits and mock cleanup evidence are retained in:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_net_regression_bbadbf80680d4875ae2c4064de4e1d06`.

Exact wrapper commands:

```powershell
& powershell -NoProfile -File './Scripts/TestWinterNetSmokeWrapper.ps1' -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json' 2>&1 | Tee-Object -FilePath 'C:\Users\OZAN CIDIK\AppData\Local\Temp\s01_04_script_checks_512cd1e455ca444e8b37c6beb6c57c66/wrapper-ps5.log'; exit $LASTEXITCODE
& pwsh -NoProfile -File './Scripts/TestWinterNetSmokeWrapper.ps1' -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json' 2>&1 | Tee-Object -FilePath 'C:\Users\OZAN CIDIK\AppData\Local\Temp\s01_04_script_checks_512cd1e455ca444e8b37c6beb6c57c66/wrapper-ps7.log'; exit $LASTEXITCODE
```

Wrapper fixtures include direct helper propagation, pre-launch Listen
refusal, boolean intent mismatch, raw missing/wrong/duplicate/server-to-client
leak, native missing/wrong/duplicate/log mismatch, unrequested diagnostics,
diagnostic watchdog/cleanup failure, and inherited guard cases. Actual receipt
validation reads the local receipt, input inventory, DLL and build log;
it does not rebuild or execute them. Both retained logs report
DLL SHA256 `4845E04198C1CB419514B50FD0E9060822E68A6866D0AC348D13B19A847068E1`.

Wrapper fixture roots:

- PS5: `C:/Users/OZAN CIDIK/AppData/Local/Temp/winter_wrapper_fixture_2a95b277c3a548b6ade95f37b2cb1c0c`
- PS7: `C:/Users/OZAN CIDIK/AppData/Local/Temp/winter_wrapper_fixture_89b6775f2b87489cab9c3a3009275b20`

Exact filter rejection actions ran under try/catch, requiring the stated
exception text; the four successful assertions were tee'd to
`filter-validation.log`:

```powershell
& './Scripts/TestNetSmokeRunner.ps1' -Case @() # Case selection must not be empty.
& './Scripts/TestNetSmokeRunner.ps1' -Case @(' ') # Case selection must not be empty.
& './Scripts/TestNetSmokeRunner.ps1' -Case @('no_such_s01_04_case') # Unknown case selection.
& './Scripts/TestNetSmokeRunner.ps1' -Case @('isolated_dedicated_summer','isolated_dedicated_summer') # Duplicate case selection refused.
```

Static checks used `Management.Automation.Language.Parser.ParseFile` on all
four scripts, scanned each for trailing whitespace, and ran
`git diff --check -- Scripts/NetSmokeTest.ps1 Scripts/TestNetSmokeRunner.ps1`.
The two untracked wrapper scripts were checked directly, not represented as
covered by Git's tracked diff. The final `static-freeze.log` records each hash.

## Frozen handoff

| Script | Current SHA256 |
| --- | --- |
| NetSmokeTest.ps1 | `CE750576AD3D5D862F6A853A08A227D7AAC3CB02D31F4EB7BF57A68902695355` |
| TestNetSmokeRunner.ps1 | `0D496578BD638AF30A723BD78B1464B37154360E7006D8F66314A484A382CEAC` |
| InvokeWinterNetSmoke.ps1 | `625AD15A90E671E5E6E5EA7F167235470A42D2594B22B01B0B3AF508DC6EE8C0` |
| TestWinterNetSmokeWrapper.ps1 | `BAE22B81D20EAC1DAE4DC78B6D2A01DEBDA3A7D5E2BBF3F58BB93E84B669E528` |

Parent review and independent Franklin QA: PENDING at this handoff.
No descendant agents, UE/build/PIE/network runtime, project commits, or main
checkout mutations were performed. Source review is not independent QA.
No cause for the S01.03 startup timeout has been established.

After parent review and QA, the parent owns the one targeted guarded capture:

```powershell
& './Scripts/InvokeWinterNetSmoke.ps1' -BuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json' -Map Summer_scene -ServerMode EditorDedicated -ShaderStartupDiagnostics
```

That command is a handoff only and was NOT executed here. Inspect the native
server argv plus `InitializeShaderTypes() begin` / `end` log markers. Their
presence or absence narrows the observed stage; it does not prove a particular
shader task/file-read blocker or project-module initialization. Deadline and
readiness failure remain failures, and `netModeEvidence` remains NOT_PROVEN.

## Coordinator acceptance — 2026-10-16

Franklin independently accepted the exact frozen four-script delta and note:
fixed opt-in server-only diagnostic argv, default absence, Listen rejection,
consistent boolean/raw/native evidence, nonempty validated case selection,
unchanged timing, isolation, provenance and owned cleanup. The coordinator
reviewed the actual deltas and verified the four script hashes before runtime.
The child's retained result is **FILTERED: 327 checks / 12 of 93 cases**, not a
claim that the full child catalog ran for this change.

The original wrapper shell-exit gap was closed with two current wrapper fixture
runs, one per PowerShell version. Both completed **262 checks, zero failures,
observed exit 0**, and accepted the unchanged real 141-input receipt read-only.
The broader child suite was not repeated. Exact command, run from the isolated
worktree with `powershell.exe` and then independently with `pwsh.exe`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./Scripts/TestWinterNetSmokeWrapper.ps1 -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
pwsh.exe -NoProfile -ExecutionPolicy Bypass -File ./Scripts/TestWinterNetSmokeWrapper.ps1 -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
```

Captured current log and observed exit files:

- `C:/Users/OZAN CIDIK/AppData/Local/Temp/s01_04_parent_ps5_73dc4ead5c744c9bb59c12fdb89027fa.log` and `.log.exit.txt` (0).
- `C:/Users/OZAN CIDIK/AppData/Local/Temp/s01_04_parent_ps7_b537bdbfb4794d9d9c6803b19ac06819.log` and `.log.exit.txt` (0).

The five frozen files were copied to main with exact hash equality; coordinator
result updates affect only this note and the board. The main checkout's unowned
Race/ZoneLayout changes and three staged Trade blobs remain excluded. No source,
config, asset or DLL copy is part of integration. This delivery uses a
command-scoped absent TEMP `core.hooksPath` to suppress the existing auto-push
hook for the local commit only; no persistent hook/config change or push.

## Actual targeted measurement — 2026-10-16

The coordinator was the sole UE runtime owner under the common lock for **one**
guarded Summer/EditorDedicated run with `-ShaderStartupDiagnostics`. It used the
same accepted isolated module and a fresh owned UserDir. There was no rebuild,
deadline increase, fallback or repeated runtime attempt for this measurement.

**Development command/connection smoke PASSED: runner and child exit 0.**

| Evidence | Observed result |
|---|---|
| Native server command | Summer raw map, `-server`, selected port **17342**, one `-LogCmds="LogShaders Log"`; no `?listen` or server `-game` |
| Client argv | `-game`, matching selected endpoint; no diagnostic LogCmds |
| Shader-type initialization | begin **09:11:58.226 UTC**, end **09:12:04.165 UTC**; logged interval **5.939 s** |
| World / gameplay port | exact Summer world ready, `LogNet:` listener **17342** |
| Connection / errors | server join **1**, client welcome **1**, configured errors **0/0** |
| Process observation | server **30.0262217 s**, client **45.1625466 s** |
| Readiness / launch budget | **31.2454378 s / 63.7393476 s** on original **95 s** clock; **5 s** reserve retained |
| Timeout / cleanup | no timeout/startup failure; owned server PID **28804** and client PID **9960** stopped without cleanup error |
| Data / provenance | `savedPreserved=true`, **141** inputs and accepted DLL unchanged |

Retained raw proof:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_winter_net_02a0b0221ce846be93d2cd8e5df81ed6`.

| File | SHA256 |
|---|---|
| `result.json` | `05F28E7E690D61036A8D1C3F759630C804DBF274F0FF30F95394118198F834A8` |
| `child/processes.json` | `FE7905722AC3EF6D837A512E97A95E3E6C6664DDAE7FC693DB843241B0B3B2A9` |
| `child/server.log` | `2504C0BECB2E1EA51011C19DB0E3B9D54D8FC820C699571C53C780FE096A9620` |
| `child/client.log` | `F3F6FFBA1AB29B9CDD52287D53CEC604CA5E5134FD90D7B523D13C6B9C4799D1` |
| `child-output.log` | `8F29C340505F28E664262F38DCA6AB255364968C7B3099384AC06ACD9578D117` |

The coordinator read the raw command, shader begin/end, exact world/listener and
join/welcome records, counted the error markers, observed no remaining
UnrealEditor/ShaderCompileWorker/UBT/LiveCoding/MyProject processes, and independently
reacquired/released the common lock. Main player data was not used.

This run demonstrates completion of shader-type initialization followed by a
Summer editor-server connection. It does not identify the cause of the earlier
startup failure, prove a shader task/file-read blocker, or establish live
`NM_DedicatedServer`. The failed S01.02 run remains failed. The 45-second client
window observes process lifetime from launch, not gameplay time after joining.
Winter in EditorDedicated mode, packaged MyProjectServer/source-engine validation,
capacity, gameplay and PIE remain open. No dump, debugger installation or new
C++/engine logging was needed for this measurement.

Franklin independently accepted the retained final diagnostic/connection evidence:
correct raw argv and server-only option, shader begin/end interval, world/port,
join/welcome, zero configured errors, complete process observation and cleanup,
the 73 pre-existing Saved entries, all 141 inputs/receipt/DLL/build-log matches,
and frozen script hashes in both checkouts. He also read the two current wrapper
logs and exit files, confirming the old exit-evidence gap is closed. His read-only
audit did not rerun UE or query live processes; the coordinator's live-process
and lock checks above are separate.
