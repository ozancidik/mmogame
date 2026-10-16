# S01.05: Winter installed Editor-server validation

**PASSED**, one guarded invocation on 2026-10-16; native wrapper, runner, and
child exits all 0. Parent independently verified the proof. Franklin's final
independent raw-proof QA PASSED with no blockers, as reported in this session.

## Exact run and scope

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Scripts/InvokeWinterNetSmoke.ps1" -Map Winter_scene -ServerMode EditorDedicated -BuildReceipt "C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json"
```

Diagnostics were omitted; both wrapper and child record
`shaderStartupDiagnostics=false`. Route: `EDITOR_SERVER`. Executable:
`C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe`.
Exact raw map: `/Game/HandPaintedEnvironment/Maps/Winter_scene/Winter_scene`.
Port: **17288**. Server uses `-server`, with no `-game`, `?listen`, or LogCmds.
Client uses `127.0.0.1:17288 -game`. Both retain the reviewed NullRHI, nosound,
unattended, nosplash, stdout, quoted ABSLOG and isolated UserDir arguments.
Complete actual argv arrays and native server command are in `child/processes.json`;
native command evidence is at server.log:322 and client.log:322. Server native
command verification is true; both raw argv arrays contain zero LogCmds elements.

Acceptance is **installed Editor-server development command/connection only**.
`netModeEvidence=NOT_PROVEN` remains explicit. Live NM_DedicatedServer, packaged
MyProjectServer, gameplay/RPC correctness, PIE, capacity, and the previous timeout's
cause are outside this result.

## Proof and observations

Proof root:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_winter_net_6fd8eec2bd984f3f9b7ee9d7ffb9ff6d`.

Fresh owned UserDir:
`C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/WinterNet_04e769110f0e45c49ebcd4166a00cc00`.

| Evidence | Actual result |
| --- | --- |
| Gameplay listener | server.log:1016, 09:21:30.372 UTC, port 17288 |
| Exact Winter world ready | server.log:1109, 09:21:30.463 UTC |
| Server join | 1; server.log:1306, 09:21:39.918 UTC, `Join succeeded: Gezgin696462` |
| Client welcome | 1; client.log:902, 09:21:39.297 UTC, Winter_scene/BP_ThirdPersonGameMode |
| Errors, server/client | 0 / 0 using `Error:|Fatal error|Ensure condition|Access violation` |
| Server/client observations | 30.2338159 / 45.2382632 seconds |
| Readiness on original child clock | 44.1359167 seconds; exact map and gameplay port both ready |
| Remaining client budget before launch | 50.8324129 seconds |
| Extra headroom over 45-second client + 5-second reserve | **0.8324129 seconds** |
| Original observation deadline / reserve | 95 seconds / 5 seconds; timedOut=false |
| Observed / cleanupOk / startupFailure | true / true / null |
| Saved preservation | true; 104 preexisting entries equal 104 after excluding owned UserDir |

Capture: `2026-10-16T09:20:43.3001709Z` through
`2026-10-16T09:22:22.8303610Z`. **Whole-wrapper wall time was 99.5266478 seconds**,
including guards, verification, and cleanup. The contract is the unchanged
95-second child observation deadline plus bounded owned cleanup waits, at most
5000 ms per acquired handle; total wrapper wall <=95 is not claimed. No clock
reset, timeout increase, fallback, or rerun was performed or justified.

The common exclusive `agent-build.lock` guarded execution; busy CIM preflight
was empty. Owned server PID 27044 and client PID 10432 both record stopped=true,
cleanupError=null. Postflight at 09:23:53.2428511 UTC found neither PID remaining
and zero engine/build busy processes. Exclusive open of the existing common
lock succeeded; the verification handle was disposed and the file retained.
Parent independently reacquired/released the lock and verified cleanup.

## Frozen identities and raw evidence

Worktree: `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject`.
HEAD before/after: `4e0f0c5a5ede948b69c324953f96b88a3ec61dc9`.
Parent main context: `83af3b0`; peer Race/Trade/ZoneLayout are excluded.
All 141 receipt inputs, DLL, receipt, build-log evidence, and four script hashes
matched before/after; parent and Franklin independently checked them. Parent
also confirmed current Source/Config status empty.

| Frozen input | SHA256 |
| --- | --- |
| NetSmokeTest.ps1 | `CE750576AD3D5D862F6A853A08A227D7AAC3CB02D31F4EB7BF57A68902695355` |
| TestNetSmokeRunner.ps1 | `0D496578BD638AF30A723BD78B1464B37154360E7006D8F66314A484A382CEAC` |
| InvokeWinterNetSmoke.ps1 | `625AD15A90E671E5E6E5EA7F167235470A42D2594B22B01B0B3AF508DC6EE8C0` |
| TestWinterNetSmokeWrapper.ps1 | `BAE22B81D20EAC1DAE4DC78B6D2A01DEBDA3A7D5E2BBF3F58BB93E84B669E528` |
| Saved/S00_01_Build/receipt.json | `D57EFF463B6AB89A7C4DC7D2F477F76A8CA56E0F4E620D66B031FF0FE83D4D61` |
| Binaries/Win64/UnrealEditor-MyProject.dll | `4845E04198C1CB419514B50FD0E9060822E68A6866D0AC348D13B19A847068E1` |

| Proof-relative file | SHA256 |
| --- | --- |
| result.json | `5E7B8A027D8DE91A003B029FC2FE7599148C1E3BE992C012052C6E8D35B61F64` |
| child/processes.json | `EC233C5185EAC382C1AEE42EE6832AF69EB8BB02A70385057860753475CFC725` |
| child/server.log | `791D5825DE519F16A799332714176C244F980AC8812CF81BEF81B66134FE2DF0` |
| child/client.log | `AC3B4E1359FD5937B0F9FF6B0B018BBF24579A4A85B11D4C79A3B8CFD5C2261A` |
| s01_05-postflight.json | `88AD656610215CD1B7AFBDACBA5D236C9AE1CD90D4D6E2A86ED6CA7904F23E5E` |

The same proof retains child-contract.json, process-preflight.json,
saved-before.json, saved-after.json, child-output.log, unabridged native stdout
in s01_05-native-stdout.json, native exit `0` in s01_05-native-exit.txt, and
one-invocation/wall timestamps in s01_05-capture.json.

Actual server.log:312,325 records installed build 1 and
`5.7.1-48512491+++UE5+Release-5.7`. Parent's separate read-only registry and
bounded known-root inventory confirms the Launcher 5.7 installation. It found
no confirmed UE5.7 source checkout within that search; machine-wide absence
is not established. No engine setup was altered or run.

Only this new repository note was written, plus normal owned runtime/proof
outputs. No scripts, Source, Config, assets, build/main files, or existing Saved
data were edited; no rebuild, DLL copy, descendants, commit, or push occurred.
Parent integrates only this named documentation file. No further work/runtime
is started by this handoff.
