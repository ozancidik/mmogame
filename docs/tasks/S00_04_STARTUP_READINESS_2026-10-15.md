# S00.04 bounded startup readiness — 2026-10-15

Script-only implementation for final review. **No new real Winter runtime was run.**
The previous real attempt failed; these fixture results do not turn it into a pass.

The opening status describes the worker handoff. Coordinator acceptance and any
subsequent actual runtime result are recorded separately at the end of this note.

## Scope and observed failure

- Sole script writer: DevOps/QA specialist; applied `ecc-mmo-verify`; no descendants.
- Reused `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject` at detached
  `4e0f0c5a5ede948b69c324953f96b88a3ec61dc9`. Parent main baseline: `721f183`.
- Exact write scope: `Scripts/NetSmokeTest.ps1`, `Scripts/TestNetSmokeRunner.ps1`,
  `Scripts/InvokeWinterNetSmoke.ps1`, `Scripts/TestWinterNetSmokeWrapper.ps1`, and
  this new task note. Old S00.01 note, S01 build files, Source/Config/assets/Saved,
  main Race and staged Trade edits are outside this worker's write scope.
- No project stage/commit, UE/build/runtime launch, DLL copy, or main write.

Read-only prior proof:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_winter_net_d5d03dce0be9490a91a0f97d43188bd2`.
Its server stayed alive for 30.1353413 seconds, without the requested raw Winter
world or gameplay listener on 17916. The only listener marker was LogTrace control
port 1985. Engine/Python initialization appeared near the end. There were no listed
engine error markers. Client PID was null; the owned server was stopped and
`savedPreserved=true`. This establishes missing readiness at the old single check,
not eventual readiness or a successful connection.

## Bounded change

Legacy defaults remain server 30 seconds, client 45 seconds, total watchdog
`ServerSeconds + ClientSeconds + 20` = **95 seconds**. The original total stopwatch
is started once and never reset for readiness or client observation.

After the complete minimum server observation, the child polls every 250 ms for
**both** the exact requested raw world marker and the selected gameplay listener.
The listener must have the `LogNet:` tag; Trace control listeners do not qualify.
Port-number prefixes and world-object-name prefixes do not qualify.

Client launch also requires at least `ClientSeconds + 5` seconds remaining on the
original clock. The defined five-second overhead reserve comes from the existing
20-second slack. With defaults, readiness must arrive by approximately total-clock
second 45, allowing at most about 15 additional startup seconds after the minimum
observation. The budget is checked again after readiness-manifest I/O immediately
before client launch. The full unchanged 45-second client observation must finish.
No new deadline, shorter observation, or readiness fallback was added.

Manifest additions: `readinessReady`, `readinessPort`, `readinessMap`,
`readinessElapsedSeconds` (original-clock elapsed at the last poll),
`clientBudgetRemainingSeconds` (remaining total-clock budget checked before launch),
`startupOverheadSeconds=5`, and `startupFailure`.

Startup failures retain exit 1 and emit `[NET_SMOKE_STARTUP] FAIL` with reason
`readiness-timeout`, `insufficient-client-budget`, or `server-exited-before-ready`.
Existing total-watchdog expiry remains exit 2. Server liveness is checked before
and after readiness log reads and again before client launch; early exit 0 fails.
Missing readiness/budget never launches a client. Both reserved log paths and the
owned-process manifest survive failure; only returned process objects are cleaned
up, with the existing independent five-second waits in finally.

Wrapper acceptance additionally requires both readiness flags, no startup failure,
readiness elapsed in [30,45], exactly five seconds overhead, at least 50 seconds
launch budget, and budget no greater than `95 - readinessElapsedSeconds`. It also
rechecks the exact raw world and `LogNet:` gameplay listener in retained logs.
Existing 30/45 observations, native script exit, join/welcome, error rejection,
cleanup, common lock, isolated UserDir/log paths, Saved protection, and fresh
source/HEAD/DLL provenance checks remain required.

## Verification

Captured results: child suite **1,132 checks / 77 cases / zero failures / exit 0**;
wrapper suite **135 checks / zero failures / exit 0** in each of Windows PowerShell
5.1 and PowerShell 7. Both wrappers accepted the unchanged actual 141-input receipt
and DLL SHA256 `4845E04198C1CB419514B50FD0E9060822E68A6866D0AC348D13B19A847068E1`
through the read-only helper. These are mocked script results, not network evidence.
All four scripts and both documented command blocks passed AST parsing; owned
files passed whitespace checks including CRLF handling. No extra broad/repeat suite.

Commands executed only against script fixtures/read-only provenance:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/TestNetSmokeRunner.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/TestWinterNetSmokeWrapper.ps1 `
  -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
pwsh -NoProfile -File Scripts/TestWinterNetSmokeWrapper.ps1 `
  -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
```

The existing harness supplies mock process objects, UDP endpoints and logs.
Readiness scenarios use one-second server/client observations, both markers
appearing after two seconds, and bounded eight/nine-second fixture watchdogs.
Production constants remain unchanged. Cases cover late healthy readiness,
never/only-map/only-port/wrong-port readiness, map/port prefixes, selected Trace
port 1985, early server exit during polling, insufficient budget and the last-poll
boundary. Existing missing join, early client exit, cleanup-failure and ownership
regressions remain. A mocked client-clock stall tests total-watchdog cleanup
without a long fixture sleep or production timing seam.

Wrapper mocks cover valid late readiness and rejection of missing/too-late markers,
short or reset-clock budgets, Trace listener tags and world prefixes. Its original
preparation, isolation, read-only Saved, provenance and cleanup tests remain.
The actual parent-provided 141-input receipt is read only; this worker neither
changes it nor claims its preexisting DLL as a new build.

Evidence directory:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/s00_04_frozen_388a714505f718`.
Per-suite `.log` and `.exit.txt` files retain actual results. `frozen-files.json`
records the four script hashes and this note; `owned.patch` is the five-file delta
against parent's `721f183` S00.02 baseline. Fixture roots are printed in suite logs.

## Remaining real verification

Final independent review and an explicitly assigned sole runtime owner remain.
No blind real retry was performed. The unchanged existing receipt/DLL require no
rebuild for this script-only change. If the parent authorizes the next real run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Scripts/InvokeWinterNetSmoke.ps1' `
  -BuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
```

The bounded poll may still fail if initialization consumes the reserved startup
budget. That must remain a startup failure, not a reason to raise the deadline or
waive full client observation. Actual Winter connection/gameplay/PIE: **NOT_RUN
for S00.04**; the prior real attempt remains **FAILED**.

## Coordinator acceptance and actual Winter connection — 2026-10-15

Erdos froze the five owned files. Franklin independently accepted the exact four
script hashes and note hash against the S00.02 baseline, including actual contents
of the untracked wrappers. The coordinator reviewed the delta and read all three
captured suite logs/exits: child 1,132 checks / 77 cases and both wrappers 135 checks,
zero failures, exit 0. The coordinator also ran the read-only provenance helper:
141 current inputs, HEAD `4e0f0c5`, accepted DLL hash unchanged; no rebuild.

After independent code acceptance, the coordinator was the sole runtime owner for
one guarded run of the command above. It used the shared common-Git lock, the
accepted isolated module and a fresh owned UserDir. No script/source changes,
binary copy, deadline increase or repeated runtime attempt occurred during this run.

**Actual Winter connection smoke: PASSED, runner and child exit 0.**

| Check | Observed result |
|---|---|
| Raw world | `/Game/HandPaintedEnvironment/Maps/Winter_scene/Winter_scene.Winter_scene` ready |
| Gameplay listener | `LogNet:` selected port **17490** |
| Connection | server `Join succeeded` **1**; client `Welcomed by server` **1** |
| Error/Fatal/Ensure/Access-violation matches | server **0**, client **0** |
| Minimum server observation | **30.1359899 s**, alive |
| Readiness elapsed / remaining client budget | **30.807805 s / 64.1189232 s** on the original clock |
| Client observation | **45.1627883 s**, both processes alive |
| Deadline / overhead reserve | **95 s / 5 s**, no timeout or startup failure |
| Owned cleanup | server PID **19916**, client PID **29168**, both stopped; no cleanup error |
| Data and module audit | `savedPreserved=true`; all **141** inputs and accepted DLL unchanged |

Retained raw proof:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_winter_net_1f45beef760845728990b91f3dbcb0fd`.
The coordinator read the raw map/listener/join/welcome records and independently
counted the error markers. After completion, no UnrealEditor/ShaderCompileWorker/
UBT/LiveCoding/MyProject process was observed; the common lock was independently
reacquired and released. The main checkout's player data was not used.

| Evidence | SHA256 |
|---|---|
| `result.json` | `9031FAD5CCD2B5D9439FDAAB656400BF688C3C1EB76285AEAF49FF08D9A8E385` |
| `child/processes.json` | `B07458202CF8CC938FB4B2A8B6FA5A091C0131C62F47BCD8A6B3560801EA8CE8` |
| `child/server.log` | `6B9DF2809D5B02BA07006D0154C4DA8C13D6CEC3AE4DBABB8AB6D534BE907C20` |
| `child/client.log` | `589DB6E69048D1C92B5611BE68B7891488C7EC6703818B44371D4639D5D647B6` |
| `child-output.log` | `D036D591F3ACE6F86D735E28323B8861224E097C47F8A4131A3C6724FF23256C` |

Four reviewed scripts were copied to main with exact hash equality; only this note
and the sprint board received coordinator result updates. The three staged peer
trade blobs and unowned Race edits are excluded. A command-scoped absent TEMP
`core.hooksPath` prevents the existing post-commit auto-push hook from running for
this local delivery; no hook/config file is changed.

This establishes a local two-process Winter connection smoke with the isolated
committed baseline. It does not establish dedicated Server support, PIE/HUD,
gameplay RPC correctness, clean native shutdown or validation of main's unowned
Race changes. The earlier failed run remains failed. Cold-cache startup remains
only a candidate explanation for that failure; this success does not prove its
cause. S00 remains open for its two-player gameplay/PIE acceptance; source-engine
and other independent S01 preparation may continue.

Franklin independently accepted the final runtime evidence: exact raw world/port,
join/welcome, zero configured errors, the observation/budget/cleanup manifest,
four frozen script hashes, all 141 current input hashes, receipt/DLL/build-log
matches and the 29 original Saved inventory entries. His read-only review did not
rerun UE or query live processes. The coordinator's lock/process checks above are
separate. The 45-second window observes client-process lifetime from launch; it is
**not** 45 seconds of gameplay or observation after joining.
