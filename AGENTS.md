# AGENTS.md

## Mandatory specialist agents (owner instruction, 2026-10-11)

The main session coordinates work. Use real subagent dispatch for every substantive development task; role files alone do not count as running agents. Skip this only when the owner explicitly permits it for that task. Read `docs/AGENT_WORKFLOW.md` for the twelve specialist roles and their routing. Existing project agents and skills remain applicable.

- Before delegation, inspect Git status, recent commits, current worktrees and peer changes. Give each worker a concrete task, an explicit file write scope and acceptance criteria. Review its diff and evidence yourself before integration.
- Assign implementation to the UE C++ specialist, independent review to QA, and network/security review to RPC, identity, economy or persistence changes. Use backend, design, NPC, assets, DevOps, performance and documentation specialists when their scope is involved.
- Keep implementation in an isolated worktree. A shared checkout is allowed only for explicitly disjoint file scopes. Never overwrite a peer's active edits. Serialize UE builds and runtime tests; review-only agents may run in parallel. Do not stop unowned processes.
- Use `ecc-mmo-plan`, `ecc-mmo-review`, `ecc-mmo-verify`, `mmo-feature` and `ue-build` as appropriate. Compile changed C++ and run relevant existing checks before committing. Documentation or agent configuration changes alone do not require a game build.
- Routine local commits are already authorized. Stage only owned files or reviewed owned hunks. Do not push, deploy or send messages to another chat without applicable user authorization. Record actual results and open PIE checks honestly.
- Continue the existing autonomous development when instructed; on `durdur`, start no new work and stop the automation. Do not create duplicate schedulers or claim that Claude or an external service ran unless it actually did.

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project overview

This is an Unreal Engine 5.7 project (`MyProject.uproject`), started from Epic's Third Person Blueprint template (`ProjectName=Third Person BP Game Template` in `Config/DefaultGame.ini`) and being extended into an MMORPG (Knight Online / Rise Online style). It now has a **C++ gameplay module** (`Source/MyProject/`) alongside the Blueprint content under `Content/`. Enabled plugins in `MyProject.uproject`: `PythonScriptPlugin` (editor automation), `ModelingToolsEditorMode`, and `GameplayStateTree`.

Gameplay logic lives in two places: (1) the C++ module for systems (stats, combat, progression, replication), and (2) Blueprint assets for content and visuals. Blueprint graph logic is not greppable — `.uasset` files are binary, so understanding/changing Blueprint behavior requires opening the asset in the Unreal Editor. C++ logic under `Source/` is normal text and can be grepped/edited directly.

## C++ module (`Source/MyProject/`)

The MMORPG systems are written in C++ so they can be edited and compiled without the editor. Current classes:
- `UMMOStatsComponent` (`Public/Components/`, `Private/Components/`) — the stat system core: primary stats (STR/DEX/INT/VIT), level/experience with an exponential curve, derived vitals (MaxHealth/MaxMana/AttackPower/Defense), and a server-authoritative combat/progression API (`ApplyDamage`, `Heal`, `ConsumeMana`, `GainExperience`, `SpendStatPoint`, `Revive`). Fully replicated with `OnRep_*` handlers that fire `BlueprintAssignable` delegates for UI.
- `AMMOCharacter` (`Public/Characters/`, `Private/Characters/`) — `ACharacter` subclass that owns a `UMMOStatsComponent`, routes engine damage (`TakeDamage`) into it, and exposes an `OnCharacterDied` BlueprintImplementableEvent. `BP_ThirdPersonCharacter` is reparented to this class so the existing character gains stats.

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
