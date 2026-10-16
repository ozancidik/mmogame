# S02.01 — existing Summer/Winter layout extraction

Candidate: `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject`, detached base `5f962ea`. Implementation is authorized independently of the open S00 PIE/source-engine acceptance gates. Main integration remains deferred: its speculative ZoneLayout paths are unowned and were neither read nor adopted. The frozen six-map-sprint0 candidate was not written. No descendants, runtime, commit, push or deployment.

## Changes and equivalence

The seven code/tool files are `Source/MyProject/Public/World/MMOZoneLayout.h`, `Source/MyProject/Private/World/MMOZoneLayout.cpp`, `Source/MyProject/Private/Core/MMOGameMode.cpp`, `Scripts/ValidateData.py`, `Scripts/PocketSpacing.py`, `Scripts/TestValidateData.py`, and `Scripts/TestPocketSpacing.py`. This note is the eighth owned path.

GameMode delegates SpawnMobArea to the new layout implementation. Literal MakeZone/MakePocket/MakeNpc rows contain only the existing two maps: 18 Summer pockets (10 Facing-relative, 8 absolute), 17 absolute Winter pockets, and 5/4 NPCs. Metadata includes ZoneId/MapName/Race/MinLevel/bPvPZone/ServerPort; NAME_None/0/false/0 are reserved, unused policy fields, not new access, race, travel, port binding or PvP behavior.

`Inherit=-1` means no assignment. It retains first-Summer MinMobLevel=0, all omitted forced levels, default 4-second refill cadence, and default false validation on the first ten Summer pockets. The other 25 pockets explicitly enable validation. Old unforced Frostbound TopUp=0 arguments were ignored; extracted rows inherit 4 rather than assigning 0. Forced refill cadences remain 30/45/50/60 as applicable. Native spawner/header/ground-nav code is unchanged.

Relative centers retain runtime MobAreaDistance, full Facing.Vector(), yaw-only Right and world Z offset. Absolute centers do not use Origin. All spawner rotations remain zero; NPC offsets use world axes and rotation (0,180,0). All per-spawner configuration precedes its FinishSpawningActor; no spawner collision override was added. Eight ordinary NPC calls retain AdjustIfPossibleButAlwaysSpawn; Kael remains deferred without that parameter and receives the identical name, role and seven ordered fallback strings before finish.

Null-world, first-iterated PlayerStart, missing-PlayerStart return, Winter-first suffix match, and unknown-map return are preserved. The former inner Summer Contains guard was redundant after EndsWith(Summer_scene); only the accepted Summer row is selected. Base source confirms **all 17 Winter pockets before its 4 NPCs, and all 18 Summer pockets before its 5 NPCs**. Earlier parent reminders describing interleaving were corrected by source inspection and parent acknowledgment; no such interleaving was introduced. Row order within each branch is preserved, so deferred completion and immediate spawner BeginPlay/TopUp occur before advancing to the next requested actor.

`[MMOLAYOUT] map=... requested_pockets=... requested_npcs=...` counts selected/requested rows even when actor spawn fails. It is not an observed runtime result or successful/live actor count. Existing per-spawner logs remain; old GameMode pocket-specific diagnostics are replaced by this summary. No gameplay/runtime telemetry or terrain/nav/Blueprint outcome is inferred from the table.

Both consumers now use the strict literal reader in PocketSpacing.py, loaded by absolute module path in ValidateData (also works with Python -I). It rejects unknown table rows/arity, duplicate IDs, unknown zone links, unsupported coordinate expressions, invalid max/radius/cadence/nav and incompatible spacing coordinates. ValidateData reads band defaults from the actual native spawner header and keeps the existing data/producibility checks. PocketSpacing groups by ZoneId, retaining the former 15 Frostbound + 3 Rift scope via explicit bCheckSpacing; it still checks playtest teleport coordinates. The other 17 pockets are parsed, not newly subjected to the legacy spacing threshold.

## Verification before build

All commands below ran in the candidate with system `py -3`, not Unreal Python. Required pure checks:

| Actual command | Exit | Actual result |
| --- | --- | --- |
| `py -3 -B Scripts/ValidateData.py` | 0 | 35 spawners; 59 mobs, 57 items, 97 quests, 93 skills, 12 effects; existing skill-gap warnings only |
| `py -3 -B Scripts/TestValidateData.py` | 0 | 22 scenarios: all previous 18 retained/adapted plus nonliteral coordinate, unknown zone, malformed row, zero refill interval |
| `py -3 -B Scripts/PocketSpacing.py` | 0 | 35 rows/9 NPCs/2 zones; 18 checked pockets, 9 document teleport points; 0 errors, 2 existing warnings (1733u,1581u) |
| `py -3 -B Scripts/TestPocketSpacing.py` | 0 | Clean, near-pocket rejection, document drift rejection/clean document, comment exclusion; added malformed/empty/unknown-zone/coordinate-arity rejection and cross-map coincidence acceptance |
| `py -3 -B Scripts/StaticLint.py` | 0 | 133 C++ files; clean; not a compilation |
| `py -3 -B Scripts/TestStaticLint.py` | 0 | All 15 existing negative scenarios caught |
| `git diff --check` | 0 | No tracked diff whitespace errors |

An additional inline `py -3 -B -` comparison read the accepted S02.00 Markdown and parsed current rows: **35/35 pockets, 9/9 NPCs, all assignment masks, coordinate formulas and per-branch row order matched**. Kael's three initialization assignments matched `git show 5f962ea:Source/MyProject/Private/Core/MMOGameMode.cpp`, including all seven fallback strings. The first ad hoc audit incorrectly stopped its string regex at a dialogue semicolon; its assertion failed, the audit regex was corrected to the initializer boundary, and the rerun passed without changing inputs/code. This is static equivalence evidence, not UE execution.

Initial inspection: `git status --short` (clean), `git log -5 --oneline`, `git worktree list`, AGENTS/workflow, SIX_MAP_PLAN stage 1, accepted baseline, native spawner/header, current parsers/fixtures, Build.cs and build wrapper. Project skills used: ecc-mmo-plan, mmo-feature, ecc-mmo-review, ecc-mmo-verify, ue-build; user/AGENTS limits override destructive/false-attribution snippets. No independent agent was dispatched from this specialist session; parent owns QA coordination. Franklin's parent-reported interim C++ review found no concrete regression; final frozen QA remains separate.

## Frozen code/tool SHA256 before build

| Owned path | SHA256 |
| --- | --- |
| Source/MyProject/Public/World/MMOZoneLayout.h | 0945F61703C3DF790F60EA81E6830B696CF887464857203CFF912B3A4BBA4D0E |
| Source/MyProject/Private/World/MMOZoneLayout.cpp | E9D834A0D52F66455E953E2CC90130CDE96F0E1FC79A546CF9478E82D8B1B6B4 |
| Source/MyProject/Private/Core/MMOGameMode.cpp | 9D668AC408F8A488E2AEB91DF6B7851C5F6D9AE157694605BBC5D2E5C8D9C5F3 |
| Scripts/ValidateData.py | 130FDAC20AEE58EBBD4E2C35327A03FB35FEA529A901DEF6AFCB4C113E85E455 |
| Scripts/PocketSpacing.py | BEB069EBE9B0339E459E431B6E718FC225B976A7847EF73FF17A1BCFC009A526 |
| Scripts/TestValidateData.py | 7C58DCB1BD2F0E1FC0E39DD7C693D3A2EA5921299A764650E2983F78E8276821 |
| Scripts/TestPocketSpacing.py | 6FBE8CAD465BCFBD921B2EF6A17A3C45A773E8472CE66F972F83E5DF02C390AB |

## Build and remaining acceptance

Parent designated this session sole UE build owner after pure checks and freeze. Available C: space before build: 91,927,531,520 bytes (~85.6 GiB). Use the existing Scripts/build.ps1 child PowerShell invocation with -Target Editor and the absolute candidate .uproject, no Clean/CleanDdc and no outer duplicate lock. The wrapper owns the common main/.git/agent-build.lock and fails closed for busy UE/build processes; busy exit 3 is DEFER and must not be retried blindly.

The single authorized build **passed**, native child PowerShell exit **0**, `Result: Succeeded` and `Editor build SUCCEEDED`. Invocation: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject/Scripts/build.ps1" -Target Editor -ProjectPath "C:/Users/OZAN CIDIK/.codex/worktrees/six-map-layout/MyProject/MyProject.uproject"`; stdout/stderr were captured through Tee-Object to fresh TEMP. No second build was started.

- Target: `MyProjectEditor Win64 Development`, candidate HEAD `5f962ea5a4cc567b4b5585a4315ddbf0b089f30c`.
- Started UTC: `2026-10-16T15:25:55.8249634Z`; finished UTC: `2026-10-16T15:27:37.4895734Z`.
- Actual build log: `C:/Users/OZAN CIDIK/AppData/Local/Temp/S02_01_layout_e9274d7c877d45d5b0da0253dd9e5a24/build.log`; SHA256 `2377F46FD6B2B66C20DF4CDE082AAFE53672DA12DF940494CC231F566710BBCE`. UBT reported 98.96 seconds and compiled MMOGameMode.cpp/MMOZoneLayout.cpp individually.
- Fresh schema-1 build result: same TEMP directory, `build-result.json`, SHA256 `3294F29D8BD0C9DB8775A54C69F0A4E0548143EAC0A66E13CC986C23EE30DE07`. It carries worktree, head, target, exit, actual timestamps, inputs, DLL and log hash/path. It is evidence for coordinator attestation, not an installed runtime receipt.
- Pre/post Source/Config/.uproject manifest: **143 inputs; delta 0**. `inputs-before.json` SHA256 `2DA7D97E13F8A54D4173AC1627523091E724EF6ED4BBF2A6B56A8B007B0A0D36`. Seven-file pre-build `code-manifest.json` SHA256 `78F341FFD4346B41F3BC188AF3C5471DB6642E86C04017FBDBBD7CBE8A23F53E`.
- New candidate DLL: `Binaries/Win64/UnrealEditor-MyProject.dll`; SHA256 `BAA8E20DC05F16B31B06A4A16DEB913F2EF6E281743C82299A65BC02FDE7DC49`; mtime UTC `2026-10-16T15:27:35.2135625Z` (within this build interval).

Franklin subsequently reported two concrete tooling findings on the pre-build script hashes: **P2**, case-sensitive Python identities disagreeing with runtime FName and the IgnoreCase suffix match; **P3**, record reader accepting missing, doubled and leading commas. Both are fixed within the owned Python files/fixtures. Zone/pocket/NPC identity keys use casefold, map uniqueness compares casefolded names, case-only collisions are rejected and mixed-case zone references remain valid. The record reader now consumes exactly one separator between calls, allows one optional final comma, and rejects missing/double/leading separators. Cross-map coordinate coincidence remains accepted. ValidateData uses this corrected shared reader unchanged; its own file hash stays the same.

After these fixes, actual reruns `py -3 -B Scripts/ValidateData.py`, `py -3 -B Scripts/PocketSpacing.py`, `py -3 -B Scripts/TestValidateData.py`, `py -3 -B Scripts/TestPocketSpacing.py`, and `git diff --check` all exited **0**. ValidateData still parses 35 spawners with existing warnings only. Its suite now passes **29 scenarios**, retaining the 22 previous scenarios and adding seven case/separator failures, plus a valid mixed-case reference control. PocketSpacing still parses 35 rows/9 NPCs/2 zones and checks 18 pockets with 0 errors/2 existing warnings; its suite adds the same seven case/separator failures and valid optional-final-comma/mixed-case controls. StaticLint and its 15 negative scenarios passed before build; unchanged C++ does not require repeating them for these Python-only fixes.

The three C++ hashes above and all 143 build inputs remain unchanged. No second build or runtime was started. Final corrected Python hashes supersede their entries in the pre-build table:

| Owned script | Final SHA256 |
| --- | --- |
| Scripts/ValidateData.py | 130FDAC20AEE58EBBD4E2C35327A03FB35FEA529A901DEF6AFCB4C113E85E455 |
| Scripts/PocketSpacing.py | 6838A476C53A151157F318DD89341AEC8269B746B13EC1B40E8B8F0151696E83 |
| Scripts/TestValidateData.py | 7DB1D10709ED3BC5542D8F9A3DAE965DF9688EE45587A1CA8AB0C5434EAFFC62 |
| Scripts/TestPocketSpacing.py | 446BFF4A2DA5E54378B7646B8219E517694652976EC21E376605F401FD7805D6 |

Final TEMP package in the same evidence directory: `final-manifest.json` (all eight owned file hashes, base/build evidence and frozen-source comparison) and `final.diff` (all eight files, including new/untracked files). These artifacts are produced after this note is finalized; their hashes and this note's hash are returned separately to avoid a self-referential manifest. Full-file/new-file diff creation is a text operation, not staging or integration. Final QA acceptance of these corrected script hashes is pending; no result is attributed to Franklin beyond the interim C++ review and the two reported findings.

No RunAllChecks (it starts UE), runtime smoke or PIE was run. Build success alone cannot accept gameplay, nav placement or client replication. Final frozen independent QA, authorized smoke, S00 PIE and source-engine/Server acceptance remain pending; main path ownership must be resolved before integration. No local commit has been made.
