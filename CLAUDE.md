# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

This is an Unreal Engine 5.7 project (`MyProject.uproject`), started from Epic's Third Person Blueprint template (`ProjectName=Third Person BP Game Template` in `Config/DefaultGame.ini`) and being extended into an MMORPG (Knight Online / Rise Online style). It now has a **C++ gameplay module** (`Source/MyProject/`) alongside the Blueprint content under `Content/`. Enabled plugins in `MyProject.uproject`: `PythonScriptPlugin` (editor automation), `ModelingToolsEditorMode`, and `GameplayStateTree`.

Gameplay logic lives in two places: (1) the C++ module for systems (stats, combat, progression, replication), and (2) Blueprint assets for content and visuals. Blueprint graph logic is not greppable — `.uasset` files are binary, so understanding/changing Blueprint behavior requires opening the asset in the Unreal Editor. C++ logic under `Source/` is normal text and can be grepped/edited directly.

## C++ module (`Source/MyProject/`)

The MMORPG systems are written in C++ so they can be edited and compiled without the editor. Current classes:
- `UMMOStatsComponent` (`Public/Components/`, `Private/Components/`) — the stat system core: primary stats (STR/DEX/INT/VIT), level/experience with an exponential curve, derived vitals (MaxHealth/MaxMana/AttackPower/Defense), and a server-authoritative combat/progression API (`ApplyDamage`, `Heal`, `ConsumeMana`, `GainExperience`, `SpendStatPoint`, `Revive`). Fully replicated with `OnRep_*` handlers that fire `BlueprintAssignable` delegates for UI.
- `AMMOCharacter` (`Public/Characters/`, `Private/Characters/`) — `ACharacter` subclass that owns a `UMMOStatsComponent`, routes engine damage (`TakeDamage`) into it, and exposes an `OnCharacterDied` BlueprintImplementableEvent. `BP_ThirdPersonCharacter` is reparented to this class so the existing character gains stats.

Rest of the module (all server-authoritative; see `docs/` for design/progress):
- **Components** (`Components/`): `UMMOInventoryComponent` (gold, consumables, equipment bag of 12, 4 equipped slots, +N upgrade/burn, vendor buy/sell, 24-slot warehouse), `UMMOSkillComponent` (4 classes, mastery trees, hotbar, cooldowns), `UMMOQuestComponent` (kill/level/gold quests, prerequisite chains, claim rewards), `UMMOPartyComponent` (party membership, invites; kill XP is split by damage share in `AMMOMob::HandleDeath` — a party counts as one unit, gets a +10%/extra-member bonus (`MMOXPTuning::PartyMultiplier`), and its share is divided by member count). XP pace curve is tuned in one place (Q_Gold* early raw XP also reduced in the table): `Private/Core/MMOXPTuning.cpp` (mob XP by mob level, quest XP by gate level; applied inside `MakeMob`/`MakeQuest`).
- **Characters**: `AMMOMob` + `FMMOMobArchetype` table (`MMOMobArchetype.cpp`, Lv1-80 normals/elites; XP scaled by level gap in `ScaleXPForLevelGap`), `AMMOMobSpawner` (level-band or forced-archetype spawner with navmesh/ground validation).
- **World NPCs** (`World/`, share `AMMONpcBase`): vendor, quest giver, talker (LLM dialogue via `UMMODialogueSubsystem`), healer, warehouse; `AMMOLootPickup`.
- **Data tables in code**: items `MMOItemDatabase.cpp`, quests `MMOQuestTypes.cpp`, skills `MMOSkillDatabase.cpp`, effects `MMOEffectDatabase.cpp`. Add new rows following the existing `Make*` factory pattern; IDs must be unique.
- **Core/UI/Player**: `AMMOGameMode` (spawns hub NPCs and all spawners; Summer_scene and Winter_scene layouts live here), `AMMOPlayerController` (click-to-move, hotbar, chat via Enter: `/p` party, `/w` whisper, `/who`, `/yardim`), `AMMOHUD` (Canvas-only HUD and windows), `UMMOSaveGame` (per-player-name slot).
- **Net smoke test**: `Scripts/NetSmokeTest.ps1` starts a headless listen server + one client and checks the join and zero errors (replication of Trade/Clan/Friends/Warehouse components). 
- **Static checks (no Unreal needed, `python3`)**: `Scripts/ValidateData.py` (mob/item/quest/skill/effect tables), `Scripts/StaticLint.py` (brace/paren balance, generated.h — not a compile), `Scripts/ProgressionModel.py` / `EconomyModel.py` / `ContentMap.py` (XP pace, gold economy, per-level content map; models, not playtest data). `.github/workflows/checks.yml` runs the first two plus the models on every push/PR, a fixture test of `Scripts/PlaytestMetrics.py` (`Scripts/TestPlaytestMetrics.py`), and on PRs `Scripts/CheckPrPaths.py` (error if the diff touches `Content/`, `Config/` or `MMOSaveGame.h`; warning for `MMOPartyComponent.*` / `MMOTradeComponent.*`, which Codex owns locally). `Scripts/RunAllChecks.ps1` runs them locally together with the build and smoke tests.
- **Smoke test**: `Scripts/SmokeTest.ps1 -Map Summer_scene|Winter_scene -Cmds "MMOProbeSurface X Y 225,quit"` runs the game headless (`-game -NullRHI`) and prints spawner validation results; use it to verify any new spawn pocket in the real runtime (editor probes alone were wrong once). Summer PlayerStart is (-816,-47,113).
- **Maps**: `Summer_scene` (Lv1-40 hub area + Lv58-65 outer pockets) and `Winter_scene` (Lv41-57 + Lv66-80). Navmesh bounds were enlarged by `Scripts/ExtendSummerNavigation.py` / `ExtendWinterNavigation.py`; new spawn spots must be probed headless (see `docs/PROGRESSION_PLAN.md`).
- **Docs**: `docs/PLAYTEST_RUNBOOK.md` (start here for a local test session: build -> `PR1_VERIFY` 1-18 -> first 7 tests -> `[MMOMETRIC]` run, pass/fail template), `docs/PR1_VERIFY.md` (PR #1 build + 18 behaviour checks), `docs/FIRST_PLAYTEST.md` (keys/commands for the first 7 tests), `docs/PLAYTEST_PRIORITY.md` (current status, known open issues, first-playtest order), `docs/BALANCE_MODEL.md` / `docs/CONTENT_MAP.md` (model outputs), `docs/SECURITY_AUDIT.md` / `docs/NETWORK_AUDIT.md` (audits; identity = player name is the known critical gap), `docs/VERTICAL_SLICE_LV1_30.md` (Lv1-30 plan), `docs/ACCOUNT_BACKEND_COMPARISON.md`, `docs/PROGRESSION_PLAN.md` (Lv1-80 plan), `docs/PLAYTEST_CHECKLIST.md` (untested-in-PIE checklist, QA findings), `docs/Lore.md`, `docs/NpcDialogue.md`, `docs/CLOUD_TASKS.md` (what a cloud session may do: no compile, no binary assets).

Design intent: authority (server/host) mutates stats; values replicate to clients; `OnRep` handlers drive UI. Keep new gameplay systems server-authoritative and replication-ready from the start.

## Building the C++ module

- **The editor must be CLOSED to compile** (open editor holds a Live Coding lock — build fails with "Unable to build while Live Coding is active"). Alternatively, use Live Coding (Ctrl+Alt+F11) from inside the editor for iterating.
- Build command (PowerShell, note the quoting for spaces in paths):
  `& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64 Development "-Project=<abs path>\MyProject.uproject" -WaitMutex`
- Toolchain: Visual Studio 2022 Community with the "Game development with C++" workload (MSVC 14.44), Windows 10 SDK. Compiled output lands in `Binaries/Win64/UnrealEditor-MyProject.dll`.
- Headless editor automation (e.g. reparenting a Blueprint) runs via:
  `& "...\UnrealEditor-Cmd.exe" "<uproject>" -ExecutePythonScript="<script.py>" -unattended -nopause -nosplash -NullRHI -stdout`

## Working with this repo

- C++ systems are compiled with the Build command above (see "Building the C++ module"). There is still no unit test runner — verifying gameplay behavior means opening the project in Unreal Editor 5.7 (`MyProject.uproject`) and testing in PIE (Play In Editor).
- `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, and `Build/` are all git-ignored, regenerated-on-demand editor artifacts — never hand-edit or worry about their contents.
- `Config/*.ini` files are the project-wide settings (input, rendering, maps, editor). Notable current settings:
  - `GameDefaultMap` / `EditorStartupMap`: `/Game/ThirdPerson/Lvl_ThirdPerson`
  - `GlobalDefaultGameMode`: `BP_ThirdPersonGameMode`
  - Enhanced Input is the active input system (`DefaultPlayerInputClass=EnhancedPlayerInput`), not the legacy input system.

## Content structure

- `Content/ThirdPerson/` — core gameplay Blueprints: `BP_ThirdPersonCharacter`, `BP_ThirdPersonGameMode`, `BP_ThirdPersonPlayerController`, and the main level `Lvl_ThirdPerson.umap`.
- `Content/Input/` — Enhanced Input assets: Input Actions (`IA_Jump`, `IA_Look`, `IA_MouseLook`, `IA_Move`) and Input Mapping Contexts (`IMC_Default`, `IMC_MouseLong`/`IMC_MouseLook`). `Content/Input/Touch/` holds the on-screen touch interface (`BPI_TouchInterface`, thumbstick/touch UI widgets) for mobile input.
- `Content/LevelPrototyping/` — reusable greybox/prototyping kit: primitive meshes, grid materials, and interactable actors (`BP_DoorFrame`, `BP_JumpPad`, `BP_WobbleTarget`) each with their own mesh/material/material-instance/niagara assets colocated under the actor's folder.
- `Content/Characters/Mannequins/` — the standard UE5 mannequin skeletal meshes, animations, materials, and rigs used by the third-person character.
- `Content/__ExternalActors__/` and `Content/__ExternalObjects__/` — UE5's One File Per Actor (OFPA) data for level actors, keyed by level path (currently only `ThirdPerson/Lvl_ThirdPerson`). These are managed automatically by the editor when the level is saved — don't hand-edit.
- `Content/Developers/OZANCIDIK/` — per-developer editor-only content (collections, etc.), not shipped with the game.

## Notes for making changes

- Any change to gameplay behavior (movement, interactables, game mode rules, input handling) means editing a Blueprint graph in-editor; there's no equivalent source file to patch directly.
- New Input Actions must be added to the relevant Input Mapping Context (`IMC_Default` or `IMC_MouseLook`) to take effect, following the existing Enhanced Input pattern.
- Interactable actors under `Content/LevelPrototyping/Interactable/` follow a per-actor folder convention (Blueprint + `Assets/` or `Meshes/` subfolder for that actor's meshes/materials/effects) — follow this convention when adding new interactables.
