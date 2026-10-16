# S02.02 — Summer/Winter layout runtime proof, 2026-10-16

**PASS: Summer Listen once, then Winter Listen once, sequentially.** Actual server summaries request 18 pockets/5 NPCs and 17/4 respectively. Each run has join/welcome 1/1, errors server/client 0/0, native wrapper/child script exits 0, successful owned cleanup and Saved preservation. These are **requested row counts, not successful spawn counts or live NPC/mob counts**.

The parent authorized this sole-runtime-owner task after Franklin accepted S02.01 and closed P2/P3. Only this new note plus runner-owned TEMP/fresh UserDirs were written. No source/config/script changes, new/custom runner, second build, runtime retry, Saved deletion, unowned process kill, RunAllChecks, PIE, commit or push. Parent main-board/S02.01 note-copy work is separate.

## Frozen provenance and preservation

Candidate: `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject`; unchanged detached HEAD `5f962ea5a4cc567b4b5585a4315ddbf0b089f30c`.

Build receipt: candidate `Saved/S00_01_Build/receipt.json`, SHA256 `F8876DA207CBDD8F71357D32AC0F238F550B9A1E5A7EE8BB783C0751A8EF474F`. The coordinator installed this fresh receipt and actual build log before runtime; this task did not replace them. Build log SHA256 `2377F46FD6B2B66C20DF4CDE082AAFE53672DA12DF940494CC231F566710BBCE`. DLL SHA256 `BAA8E20DC05F16B31B06A4A16DEB913F2EF6E281743C82299A65BC02FDE7DC49`, unchanged, build mtime UTC `2026-10-16T15:27:35.2135625Z`.

The existing wrapper verified provenance before and after both runs. Independent byte comparisons confirm **143 Source/Config/.uproject inputs, delta 0; all eight frozen S02.01 files, delta 0**, including the four Python files and S02.01 note. Receipt, DLL and HEAD are preserved. Full per-file hashes are in the evidence inventory below; the prior S02.01 final-manifest remains unchanged. The earlier parent preflight `mmo_winter_net_b8fa1004dc554a77892d058cc35781d9` was intentionally NOT_RUN and is not counted as a runtime pass here.

## Actual execution

Child PowerShell commands from this candidate; `$LASTEXITCODE` was captured immediately after each. No ShaderStartupDiagnostics or PreflightOnly switch:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject/Scripts/InvokeWinterNetSmoke.ps1" -BuildReceipt "C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject/Saved/S00_01_Build/receipt.json" -Map Summer_scene -ServerMode Listen
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject/Scripts/InvokeWinterNetSmoke.ps1" -BuildReceipt "C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject/Saved/S00_01_Build/receipt.json" -Map Winter_scene -ServerMode Listen
```

Winter started only after Summer's native exit, wrapper verdict, expected layout summary, join/welcome and zero-error checks passed. The unchanged wrapper owns common Git agent-build.lock, checks CIM/assets/provenance/isolation, protects old Saved files, and invokes the existing NetSmokeTest child. Its 95-second observation deadline plus bounded owned cleanup remain in force. Successful script exits do not claim naturally clean UE process exits: cleanup stops the runner's owned process handles.

| Map | UTC start → finish | Port | Wrapper/child exits | Requested pockets/NPCs | Join/welcome | Errors server/client |
| --- | --- | --- | --- | --- | --- | --- |
| Summer_scene | 2026-10-16T15:37:08.1453260Z → 2026-10-16T15:38:29.6464813Z | 17028 | 0/0 | 18/5 | 1/1 | 0/0 |
| Winter_scene | 2026-10-16T15:38:59.3340541Z → 2026-10-16T15:40:19.8502853Z | 17293 | 0/0 | 17/4 | 1/1 | 0/0 |

Both result.json files report PASSED, runnerExit=0, childExit=0, savedPreserved=true and no reason. Both process manifests report observed=true, cleanupOk=true, timedOut=false, watchdogSeconds=95. Route=EDITOR_LISTEN; acceptanceScope=DEVELOPMENT_COMMAND_AND_CONNECTION_ONLY; runner netModeEvidence=NOT_PROVEN.

| Map | Server/client observed seconds | Readiness / remaining client budget seconds | Owned server/client PIDs | Saved protected before |
| --- | --- | --- | --- | --- |
| Summer_scene | 30.1779606/45.1871504 | 31.4200836/63.5654313 | 25860/3892 | 5 entries, 3 files |
| Winter_scene | 30.2386870/45.1254100 | 30.7012980/64.2866411 | 26784/25168 | 37 entries, 16 files |

All four owned process rows report stopped=true, cleanupError=null. A final read-only CIM inventory found 0 matching UE/build processes. Its empty JSON inventory was serialized explicitly as []; no runtime rerun occurred. Winter protected the preceding Summer UserDir as pre-existing data. Neither fresh UserDir was deleted.

## Raw proof and log locations

**Summer_scene** proof directory: `C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_winter_net_49aab37b74cb416f88c6340638d0b731`. Fresh UserDir: `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject/Saved/WinterNet_03a2ad87bb0a47a1ac59da0b912594a9`.

- `result.json`: wrapper verdict/provenance/preservation; SHA256 `30C2A7EED27F287D54D2B9E144A7266633EDDB044809A424EF03E4BEB307FF55`.
- `child/processes.json`: raw executable/arguments, observation, cleanup; SHA256 `0F011080D5082A483EDD6BDCE5E759A99C4EF3059EEF200566EEB03F4F4B6AB5`.
- `child/server.log:1185`: `[MMOLAYOUT] map=Summer_scene requested_pockets=18 requested_npcs=5`; join at line 1222. SHA256 `6F19167741357950E48B83B184C59EFDD18B5F2A8EC5DD48EDF05F26D42D24BB`.
- `child/client.log:903`: Welcomed by server for the expected map. SHA256 `5DD25ACCBE69A717554F775C6FB19B6CF602A1505F28B527BDD9A2EA15475B88`.
- `child-output.log` records the actual connection/error summary. `process-preflight.json`, `child-contract.json`, `saved-before.json` and `saved-after.json` retain gate and preservation evidence.

**Winter_scene** proof directory: `C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_winter_net_055f9a93657e4eb5b8dd9a35c94c9525`. Fresh UserDir: `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject/Saved/WinterNet_054a750bdf7643c981d3e164987dd416`.

- `result.json`: wrapper verdict/provenance/preservation; SHA256 `6007E9C7E7F20B5C1EBC0872BD891C9142DD28582418260C16C4350680397E37`.
- `child/processes.json`: raw executable/arguments, observation, cleanup; SHA256 `F5A4C48E50D9BDABECB2AE6ABFF2FECE17783720FE720ECD06EC13EA72F2BC91`.
- `child/server.log:1238`: `[MMOLAYOUT] map=Winter_scene requested_pockets=17 requested_npcs=4`; join at line 1292. SHA256 `41D67FE4BCDBD4EF153E635462B4D91A9796879F35E1AB51F955DF4172BCD72A`.
- `child/client.log:903`: Welcomed by server for the expected map. SHA256 `675CA84BFD868ABD95D363CDC846AB5ADA643EEA4D4744D682A42737661149AD`.
- `child-output.log` records the actual connection/error summary. `process-preflight.json`, `child-contract.json`, `saved-before.json` and `saved-after.json` retain gate and preservation evidence.

Capture directory: `C:/Users/OZANCI~1/AppData/Local/Temp/S02_02_runtime_cd5eeda3235249e7a1247d29edcee916`. `before.json`, `summer-summary.json` / `winter-summary.json`, each `*-wrapper.stdout.json` and empty `*-wrapper.stderr.log` retain exact native exits, timestamps and wrapper output. Consolidated `preservation-and-evidence.json` SHA256 `F471AF9284A278CD6410074C00F3FF775C32CCD45688D3F57A8CC7368C02C879` contains all raw-proof hashes, frozen file hashes, observed intervals and log lines. `process-postflight.json` SHA256 `4F53CDA18C2BAA0C0354BB5F9A3ECBE5ED12AB4D8E11BA873C2F11161202B945`.

Other actual commands: git status/log, Get-Content of the existing runner/receipt/proofs, Get-FileHash, Select-String for layout/join/welcome, inline `py -3 -B -` for byte-hash/input-manifest and raw-proof checks, and read-only Get-CimInstance Win32_Process. No test reruns or compilation were used as substitutes for runtime evidence.

## Remaining acceptance

This proves the required requested-layout summaries and a real client connection during each separate bounded development Listen-route run. It does not prove successful/live NPC or mob counts, every ground/nav location, Kael service/gameplay/RPC behavior, simultaneous maps, PIE, NM_DedicatedServer or a packaged/source-engine Server target. **PIE remains open/NOT_RUN.** Parent owns independent frozen-note/raw-proof acceptance, main integration and any subsequent commit. This task starts no further work.
