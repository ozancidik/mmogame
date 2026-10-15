# S01.02 installed-editor server route preparation — 2026-10-15

**Script preparation only; real route/connection: NOT_RUN.** Implements the bounded
development route for SIX_MAP_PLAN §11 step 4. That step's real yes/no measurement
remains outstanding; this package does not claim MyProjectServer target compilation,
packaged-server validation, alpha acceptance, or proven live `NM_DedicatedServer`.

This opening status describes the worker handoff. The coordinator's independent
acceptance and subsequent actual route measurement are recorded separately below.

## Scope and baseline

- Sole DevOps/QA script writer; applied `ecc-mmo-verify`; no descendant agents.
- Reused `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject`, detached
  HEAD `4e0f0c5a5ede948b69c324953f96b88a3ec61dc9`. Parent accepted baseline: `5808c75`.
- Five owned files: `Scripts/NetSmokeTest.ps1`, `Scripts/TestNetSmokeRunner.ps1`,
  `Scripts/InvokeWinterNetSmoke.ps1`, `Scripts/TestWinterNetSmokeWrapper.ps1`, this
  new note. Existing guard/helper functions and fixture harness are reused.
- Old notes, build scripts, C++/Config/assets/Saved and main Race/staged Trade edits
  were not edited. No UE/build/runtime, project stage/commit, DLL copy, downloads,
  account/credential changes, or main write. Temporary fixture Saved output is
  confined to disposable fixture repositories, never the actual project Saved.
- Existing 141-input receipt/DLL remains the build gate. No fresh compile is needed
  for these script-only changes.

## Minimal additive interface

Child gains `-ServerMode Listen|EditorDedicated`, default **Listen**. Its historical
Summer/default CLI remains unchanged. The existing guarded wrapper gains the same
mode and `-Map Summer_scene|Winter_scene`; wrapper defaults remain **Winter/Listen**.
Selected-map package checks and raw-world readiness are used for both maps.

`EditorDedicated` server argv uses the quoted project, raw `/Game/.../<Map>/<Map>`,
and `-server`. It omits `?listen` and server-side `-game`. Client argv retains the
quoted project, `127.0.0.1:<chosen-port>` and `-game`. The existing fixed
`UnrealEditor-Cmd.exe` executable and headless/unattended options are retained.

Child manifest records `serverMode`, `route`, executable, the exact server/client
argument arrays passed to Start-Process, and native server command-line evidence.
Wrapper verifies requested mode/map against these arrays, executable/project,
port, quoted UserDir/log arguments, client mode, and manifest route. Dedicated
results/output use **EDITOR_SERVER** and scope **DEVELOPMENT_COMMAND_AND_CONNECTION_ONLY**.
`netModeEvidence=NOT_PROVEN` is explicit, including when command/connection checks pass.

## Evidence boundary and fallback refusal

Local installed-engine evidence read only:

- `Engine/Build/InstalledBuild.txt` exists (`True`).
- [StaticDedicatedServerCheck](</C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Private/Misc/CoreMisc.cpp:249>) parses the SERVER/RUN=SERVER switches for editor builds.
- [Editor launch mode selection](</C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:2238>) selects the dedicated startup branch, setting GIsClient false and GIsServer true.
- [Engine command-line logging](</C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Private/Misc/App.cpp:455>) logs the engine command line through GetForLogging.

These source paths support the requested development route; they are not execution
evidence from this task. Before dedicated client launch the child requires exactly
one native `LogInit: Command Line:` line containing the selected raw map, `-server`,
and selected `-port`, without `?listen` or `-game`. Missing/conflicting evidence fails
and stops only the owned server. Wrapper rechecks the retained native log against
the manifest. A requested `-server` flag alone is insufficient; a native listen
command line or mismatched raw argv cannot produce a pass.

This proves command intent reached the engine plus connection evidence only when
the eventual real run passes. It does **not** dynamically sample the world's
`GetNetMode()` and cannot exclude every in-engine/custom-code mode change. No C++
probe was added within this script-only scope. Actual `NM_DedicatedServer` therefore
remains **NOT_PROVEN**, and no target/package/alpha acceptance follows from this route.
Missing or filtered native command-line evidence fails closed for EditorDedicated.

## Preserved safety and timing

Same original 95-second total observation clock, minimum 30-second server
observation, full 45-second client observation and five-second launch overhead
reserve. Both exact raw world and LogNet gameplay-port readiness remain required;
Trace listeners and map/port prefixes do not qualify. No timer reset or enlarged
deadline. Server/client must stay alive; early exit 0, missing join/welcome, error
markers, wrong port/map, incomplete observation and cleanup failure still fail.

The wrapper alone holds common-Git `agent-build.lock` across the same-process
child invocation, audits/protects existing Saved, and revalidates source/HEAD/DLL
provenance. Fresh isolated UserDir and log directory, retained proof/log paths,
owned process-object cleanup and bounded independent waits remain unchanged.

## Actual fixture verification

Wrapper PS5.1 and PS7 suites: **170 checks each, zero failures, exit 0**. Both
accepted the unchanged actual 141-input receipt via the read-only helper, with DLL
SHA256 `4845E04198C1CB419514B50FD0E9060822E68A6866D0AC348D13B19A847068E1`.
Child suite: **1,423 checks / 86 cases / zero failures / exit 0**. No fixture result
is real network evidence. All four scripts and both documented command blocks
passed AST parsing; all five owned files passed whitespace checks including CRLF.

Added cases cover exact dedicated server/client argv, both Summer/Winter readiness,
legacy Listen retention, invalid mode/map, native listen fallback/missing command
line/wrong command map, manifest mode/raw-argv/client-mode mismatch, and unchanged
join/cleanup failure rejection. Prior bounded startup, clock/budget, isolation,
provenance, protected Saved and ownership cases remain in the same suites.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/TestNetSmokeRunner.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/TestWinterNetSmokeWrapper.ps1 `
  -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
pwsh -NoProfile -File Scripts/TestWinterNetSmokeWrapper.ps1 `
  -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
```

Evidence: `C:/Users/OZAN CIDIK/AppData/Local/Temp/s01_02_frozen_eb66f836faafd`.
It contains per-suite logs/exits, `frozen-files.json`, and `owned.patch` against
the parent's `5808c75` accepted S00.04 baseline. Fixture roots appear in the logs.

## Remaining parent-owned runtime

Final independent QA and explicit assignment of one sole runtime owner remain.
The preparation is independent of pending user PIE and a future source-engine /
MyProjectServer build. After QA, the authorized owner can measure the Summer route:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Scripts/InvokeWinterNetSmoke.ps1' `
  -Map Summer_scene -ServerMode EditorDedicated `
  -BuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
```

Winter uses `-Map Winter_scene` with the same mode. Omitting the new options retains
the existing guarded Winter/Listen route. No command above launched UE in this task.

## Coordinator integration and measurement — 2026-10-15

Franklin independently accepted the frozen four-script delta and note against
`5808c75`: raw server/client argv, native command evidence and fallback refusal,
validated map/mode selection, default Listen behavior, timing, isolation, common
lock, provenance and owned cleanup. The coordinator verified all five hashes,
reviewed actual wrapper contents and read the three suite logs and exit files:
child 1,423 checks / 86 cases; PS5.1 and PS7 wrappers 170 checks each, zero failures,
all exit 0. The read-only helper again accepted all 141 inputs and the existing
fresh isolated DLL; no new compile was performed.

The four scripts and original task note were copied to main with hash equality.
Only this note and the sprint board receive coordinator result updates. Main's
peer Race edits and three staged Trade blobs are excluded. Integration is local;
the existing auto-push post-commit hook is disabled for this commit only through
a command-scoped absent TEMP `core.hooksPath`, with no persistent hook/config edit.

### Actual route measurement: FAILED at startup admission

The coordinator, as sole UE runtime owner under the common lock, ran the documented
Summer/EditorDedicated command **once** with the unchanged isolated module. Runner
and child exit **1**. No deadline increase, fallback, compile or blind retry.

| Evidence | Observed result |
|---|---|
| Requested route | `EDITOR_SERVER`, Summer raw map + `-server`; no `?listen` or server `-game` |
| Native command log | Correct raw map, `-server`, selected `-port=17262` and owned UserDir |
| Minimum server observation | **30.0708353 s**, server remained alive |
| Startup admission | `readiness-timeout` at **45.1095114 s**, map/port both false |
| Remaining budget | **49.8904886 s**; full 45 s client + 5 s reserve no longer fit |
| Client / connection | **NOT_LAUNCHED**; no client log, join/welcome not established |
| Cleanup | owned server PID **16136** stopped; no cleanup error, no overall watchdog expiry |
| Error markers | server **0** Error/Fatal/Ensure/Access-violation matches |
| Data / provenance | `savedPreserved=true`; **141** current inputs and accepted DLL unchanged |

The server log ends during DDC/shader-autogen initialization. Its native command
line is present, but readiness never reached the command-verification transition,
so manifest `serverCommandLineVerified=false` correctly remains false. This shows
failure to become ready within the bounded startup window. It does **not** prove
that the installed editor cannot run a server, nor that cold cache is the cause.
Dynamic `NM_DedicatedServer`, actual connection and packaged Server remain unproven.

Raw proof:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_winter_net_545008b0764049c6b0329d5234000b4d`.

| File | SHA256 |
|---|---|
| `result.json` | `2DBF86F969CC11433894B5A4A7E0DECBD2E60B4CD6CFAD812D51DEB89E4C4EF3` |
| `child/processes.json` | `F87E02EBBC9C4ABA55861AE05F7E75D114867CE2B7447E6C00E6BC0D11195029` |
| `child/server.log` | `13DE115445B2472A765BF370A8310FD208533CA2D0239910A3F2788368EAD1DA` |
| `child-output.log` | `62F97B772BC2D2B4FE9D9F19157ED7CCD63060410275F04F3B14C5DCB3C9A00F` |

The coordinator read the raw failure/command evidence, counted errors, observed no
remaining UnrealEditor/ShaderCompileWorker/UBT/LiveCoding/MyProject processes and
independently reacquired/released the common lock. Main player data was not used.
The next bounded diagnostic must examine this exact startup trace and engine path
before any additional runtime. Missing source-engine/PIE acceptance stays open;
this preparation is useful but does not close S01.

Franklin independently accepted this as an honest failed measurement. He verified
the raw correct command, budget-triggered client refusal (not overall timeout),
owned cleanup, zero configured errors, all four frozen script hashes, all 141 input
and receipt/DLL hashes, and the 61 pre-existing Saved entries unchanged outside the
owned UserDir. He found no explicit unsupported-server/module-incompatibility
diagnostic; Zen reports healthy and shader autogen writing completes after the
deletion warning. Unknown startup delay/stall is the supported conclusion.

The bounded follow-up is read-only inspection of existing run-owned logs/trace/dump
artifacts near that transition. If no blocking operation is evidenced, record
insufficient evidence and propose a targeted capture; do not silently modify the
harness, increase the deadline or rerun the game. No additional runtime is claimed.
