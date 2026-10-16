# S02.00 existing layout baseline — 2026-10-16 (frozen specialist note)

Documentation-only static extraction of `AMMOGameMode::SpawnMobArea`. **35 pocket call sites: 18 Summer (10 Origin-relative, 8 absolute), 17 Winter (all absolute); 9 NPC call sites: 5 Summer, 4 Winter.** Counts describe requested placements, not successful actors or live mobs. No future six-map placements or S02 implementation/integration are authorized. S00 PIE/source-engine gates remain open. Franklin's independent count/semantics pass agrees; his audit of this frozen note's rows remains pending.

## Frozen inputs and scope

- Worktree: `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject`, detached HEAD `4e0f0c5a5ede948b69c324953f96b88a3ec61dc9`; inspected main HEAD `4076faf`. Existing peer script/doc changes were present and left alone; main Trade/Race/ZoneLayout work is unowned here.
- Only output: this new file. Source, Config, Scripts, Content, Saved, main and old notes were not changed. No build, runtime, tests, commit, push, descendants or other-chat messages were performed.
- `GM` below means baseline `Source/MyProject/Private/Core/MMOGameMode.cpp`, raw SHA256 **FE66331BA1F3CFA3DF4829CBE0DB75A42FF121427B50734CFC81CDE61FA46A45** (locally measured, matching assigned input).
- `SH` = `Source/MyProject/Public/Core/MMOMobSpawner.h`, raw SHA256 `05E2A19D5D461A45F5604F369C351B86BBFFAE0C82FD4E426738D03654EED488`.
- `SC` = `Source/MyProject/Private/Core/MMOMobSpawner.cpp`, raw SHA256 `32B7C8578C24B562D65930D3FD37B860DF9983CF71E16D8DA2A49A24232A8584`.
- `GH` = `Source/MyProject/Public/Core/MMOGameMode.h`, raw SHA256 `EE173D7B51AD57E72E678C5E7B183C7CEA400046CECCACC12D54E422C8572E9B`.
- All file:line references use these worktree files and one-based lines. Parent separately reported matching normalized Git blobs, GameMode Git object `de4e7b796816491b54ffa41b82214f6dfa9f24bc`; that is parent evidence, not a SHA256 or a normalization measurement performed here. Tooling consumers/migration order belong to the parent's separate audit.

## Reading the tables

`E(value)` is explicitly assigned by GameMode (including executed helper assignments); `D(value)` is DEFAULT, left unassigned at that call site. `D(None)` denotes the default empty `FName`: SH:40 declares the member without an explicit initializer. SH:36/46/49/52/56/59 initialize MaxMobs=3, MinMobLevel=0, MaxMobLevel=0, SpawnRadius=500, validation=false, TopUpInterval=4 seconds. SC:14–28 constructs the root, disables tick/replication/client loading and does **not** replace these gameplay defaults. Every listed pocket explicitly sets MaxMobs and radius; default 3/500 are therefore not used in these rows.

Coordinates are Unreal units. `O` = first iterated PlayerStart's actor location; `F` = its full `Facing.Vector()`; `R` = `FRotator(0, Facing.Yaw+90, 0).Vector()` (yaw-only); `V` = world-space `(0,0,50)`; `D` in coordinate formulas only = runtime `MobAreaDistance` (native default 700, GH:123–125, EditDefaultsOnly). Thus do not freeze D-dependent formulas to native-default numbers or assume a Blueprint value. `Z0` = explicit `FRotator::ZeroRotator`, irrespective of PlayerStart rotation. `ABS(x,y,z)` = literal world coordinate, with no Origin addition. NPC offsets use world axes, not F/R. Helper identifiers `SpawnFrostbound@(x,y,z)` / `SpawnRift@(x,y,z)` abbreviate the existing call's first `FVector` argument, with its exact GM call line; they are not new layout IDs.

## Summer pockets — 18, in source execution order

| Existing variable/call identifier | Requested center | Rotation | Min / Max level | ForcedArchetypeId | MaxMobs | Radius | TopUp seconds | Nav/ground validation | Baseline file:line |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Spawner | O + F*(D*1.5) + V | Z0 | D(0) / E(5) | D(None) | E(6) | E(350) | D(4) | D(false) | GM:1106–1116 |
| BossSpawner | O + F*(D*2.2) + V | Z0 | D(0) / D(0) | E(OrcWarlord) | E(1) | E(150) | E(30) | D(false) | GM:1124–1132 |
| EliteSpawner | O + F*(D*1.6) + R*700 + V | Z0 | D(0) / D(0) | E(SpiderQueen) | E(1) | E(150) | E(45) | D(false) | GM:1137–1146 |
| GolemSpawner | O + F*(D*1.6) - R*700 + V | Z0 | D(0) / D(0) | E(StoneGolem) | E(1) | E(150) | E(50) | D(false) | GM:1150–1158 |
| AssassinSpawner | O + F*1400 + R*1350 + V | Z0 | D(0) / D(0) | E(ShadowAssassin) | E(1) | E(150) | E(45) | D(false) | GM:1174–1182 |
| SecondZone | O - F*650 + R*1300 + V | Z0 | E(6) / E(7) | D(None) | E(5) | E(400) | D(4) | D(false) | GM:1190–1198 |
| ThirdZone | O - F*1300 + V | Z0 | E(8) / E(12) | D(None) | E(5) | E(400) | D(4) | D(false) | GM:1205–1213 |
| LichSpawner | O - F*1650 + R*1300 + V | Z0 | D(0) / D(0) | E(FrostLich) | E(1) | E(150) | E(60) | D(false) | GM:1218–1226 |
| FourthZone | O - F*650 - R*1300 + V | Z0 | E(16) / E(22) | D(None) | E(5) | E(400) | D(4) | D(false) | GM:1232–1240 |
| TyrantSpawner | O + F*1400 - R*1350 + V | Z0 | D(0) / D(0) | E(MagmaTyrant) | E(1) | E(150) | E(60) | D(false) | GM:1246–1254 |
| ObsidianSpawner | ABS(-2800,-1800,50) | Z0 | E(25) / E(29) | D(None) | E(5) | E(225) | D(4) | E(true) | GM:1265–1274 |
| ThawSpawner | ABS(-1875,5125,-114) | Z0 | E(13) / E(15) | D(None) | E(5) | E(225) | D(4) | E(true) | GM:1299–1308 |
| ObsidianBossSpawner | ABS(-4000,0,50) | Z0 | D(0) / D(0) | E(ObsidianWarlord) | E(1) | E(150) | E(60) | E(true) | GM:1311–1320 |
| DuskSpawner | ABS(1000,3200,238) | Z0 | E(31) / E(39) | D(None) | E(5) | E(225) | D(4) | E(true) | GM:1328–1337 |
| DuskBossSpawner | ABS(-3200,1800,50) | Z0 | D(0) / D(0) | E(DuskOverlord) | E(1) | E(150) | E(60) | E(true) | GM:1340–1349 |
| SpawnRift@(800,-4800,50) | ABS(800,-4800,50) | Z0 | E(58) / E(60) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1386; helper GM:1363–1383 |
| SpawnRift@(-4400,5400,-92) | ABS(-4400,5400,-92) | Z0 | E(62) / E(64) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1387; helper GM:1363–1383 |
| SpawnRift@(4800,4000,550) | ABS(4800,4000,550) | Z0 | D(0) / D(0) | E(RiftHerald) | E(1) | E(150) | E(60) | E(true) | GM:1388; helper GM:1363–1383 |

## Winter pockets — 17, in source execution order

| Existing variable/call identifier | Requested center | Rotation | Min / Max level | ForcedArchetypeId | MaxMobs | Radius | TopUp seconds | Nav/ground validation | Baseline file:line |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WinterSpawner | ABS(-3000,-1500,42) | Z0 | E(42) / E(44) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:974–984 |
| WinterBoss | ABS(-3000,0,50) | Z0 | D(0) / D(0) | E(WinterSovereign) | E(1) | E(150) | E(60) | E(true) | GM:987–997 |
| SpawnFrostbound@(3250,500,232) | ABS(3250,500,232) | Z0 | E(51) / E(51) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1038 |
| SpawnFrostbound@(-3500,3000,-120) | ABS(-3500,3000,-120) | Z0 | E(55) / E(55) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1039 |
| SpawnFrostbound@(2000,2800,236) | ABS(2000,2800,236) | Z0 | D(0) / D(0) | E(FrostboundWarden) | E(1) | E(150) | E(60) | E(true) | GM:1040 |
| SpawnFrostbound@(-5000,-3000,50) | ABS(-5000,-3000,50) | Z0 | E(46) / E(47) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1049 |
| SpawnFrostbound@(4500,-700,120) | ABS(4500,-700,120) | Z0 | E(48) / E(49) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1050 |
| SpawnFrostbound@(6000,-4000,240) | ABS(6000,-4000,240) | Z0 | E(53) / E(53) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1051 |
| SpawnFrostbound@(1200,-5200,50) | ABS(1200,-5200,50) | Z0 | E(66) / E(66) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1062 |
| SpawnFrostbound@(-2000,-6800,49) | ABS(-2000,-6800,49) | Z0 | E(68) / E(68) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1063 |
| SpawnFrostbound@(-4000,-6000,50) | ABS(-4000,-6000,50) | Z0 | E(69) / E(69) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1065 |
| SpawnFrostbound@(4750,-5750,179) | ABS(4750,-5750,179) | Z0 | D(0) / D(0) | E(AshTyrant) | E(1) | E(150) | E(60) | E(true) | GM:1066 |
| SpawnFrostbound@(-5000,4800,-92) | ABS(-5000,4800,-92) | Z0 | E(72) / E(72) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1067 |
| SpawnFrostbound@(-5500,2500,-120) | ABS(-5500,2500,-120) | Z0 | E(74) / E(74) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1073 |
| SpawnFrostbound@(-2000,2500,151) | ABS(-2000,2500,151) | Z0 | E(78) / E(78) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1074 |
| SpawnFrostbound@(4400,3600,550) | ABS(4400,3600,550) | Z0 | E(76) / E(76) | D(None) | E(6) | E(225) | D(4) | E(true) | GM:1075 |
| SpawnFrostbound@(6800,2000,629) | ABS(6800,2000,629) | Z0 | D(0) / D(0) | E(AbyssRegent) | E(1) | E(150) | E(60) | E(true) | GM:1076 |

All `SpawnFrostbound` rows also reference helper GM:1011–1032. It assigns Min/Max **only when Forced.IsNone()**. Unforced calls' final `0.f` argument is unused, so TopUp is D(4), not E(0). Forced calls' `0,0` level arguments are unused, so those levels are D(0)/D(0), not explicit assignments; only the forced branch assigns TopUp (60 here). `SpawnRift` GM:1370–1378 follows the same conditional level/forced split but hardcodes 60 in its forced branch. Unforced `NAME_None` arguments do not assign the member; they select the branch that leaves it at default.

## NPCs — 9 call sites

Rotation below is explicit world-space `FRotator(Pitch=0,Yaw=180,Roll=0)`, independent of Facing. `Adjust` = explicit `AdjustIfPossibleButAlwaysSpawn` through WinterNpcParams (GM:1080–1082) or NpcParams (GM:1392–1393). `Unspecified` means that deferred call supplies no collision override; do not substitute the shared NPC parameter.

| Branch / existing variable or call | Native class | Requested location | Rotation (P,Y,R) | Spawn / collision | Baseline file:line |
| --- | --- | --- | --- | --- | --- |
| Winter / SpawnActor<AMMOVendorNPC> | AMMOVendorNPC | O + (250,200,60) world axes | (0,180,0) | Immediate / Adjust | GM:1083–1084 |
| Winter / SpawnActor<AMMOQuestGiverNPC> | AMMOQuestGiverNPC | O + (250,-200,60) world axes | (0,180,0) | Immediate / Adjust | GM:1085–1086 |
| Winter / SpawnActor<AMMOHealerNPC> | AMMOHealerNPC | O + (250,-350,60) world axes | (0,180,0) | Immediate / Adjust | GM:1087–1088 |
| Winter / SpawnActor<AMMOWarehouseNPC> | AMMOWarehouseNPC | O + (250,350,60) world axes | (0,180,0) | Immediate / Adjust | GM:1089–1090 |
| Summer / VendorLoc | AMMOVendorNPC | O + (250,200,60) world axes | (0,180,0) | Immediate / Adjust | GM:1395–1396 |
| Summer / QuestLoc | AMMOQuestGiverNPC | O + (250,-200,60) world axes | (0,180,0) | Immediate / Adjust | GM:1399–1400 |
| Summer / Kael, TalkerLoc, TalkerTM | AMMOTalkerNPC | O + (400,0,60) world axes | (0,180,0) | Deferred / Unspecified | GM:1404–1420 |
| Summer / HealerLoc | AMMOHealerNPC | O + (250,-350,60) world axes | (0,180,0) | Immediate / Adjust | GM:1429–1430 |
| Summer / WarehouseLoc | AMMOWarehouseNPC | O + (250,350,60) world axes | (0,180,0) | Immediate / Adjust | GM:1433–1434 |

Kael alone has assignments before FinishSpawningActor: name `Muhafiz Kael`, role text and **seven** ordered fallback lines (GM:1408–1419), then finish at GM:1420. Winter has no Kael call. No dialogue-service or Blueprint behavior was executed or inferred.

## Branches, ordering and nonuniformities to preserve

- GM:621–627 calls Super::StartPlay, sets seamless travel, logs, then SpawnMobArea. SpawnMobArea itself returns for null World (GM:946–950). It takes the **first TActorIterator PlayerStart**, location and full rotation, then breaks (GM:953–962); this is not a player-specific spawn selection. Missing PlayerStart logs and returns (GM:963–966): the initialized `(0,0,200)`/zero-facing values are **not a spawn fallback**, even for Winter's absolute pockets.
- Winter `EndsWith("Winter_scene")` is checked first (GM:972); its 17 pockets precede four NPCs and an unconditional branch return (GM:1091–1092). Non-Winter maps must `EndsWith("Summer_scene")` or log/return (GM:1097–1100). There is no unknown-map Summer fallback. Summer's eight absolute pockets additionally sit under `Contains("Summer_scene")` (GM:1262–1389), reached only after the suffix guard. Prefixes such as PIE do not change the suffix comparison; actual world names were not observed here.
- Each pocket uses native `AMMOMobSpawner::StaticClass()` deferred spawn, checks its pointer, assigns settings, then FinishSpawningActor with the same transform. Helper invocations perform that sequence individually; failures skip that pocket and execution continues. All 35 transforms explicitly use zero rotation. No spawner call supplies an explicit collision-handling override. Do not transfer the NPC collision parameters to them.
- SC:30–50 guards BeginPlay by authority, supplies native AMMOMob if MobClass is absent, immediately TopUp's, then sets the repeating TopUp timer using TopUpInterval. Therefore per-pocket settings must precede FinishSpawningActor; the interval is a periodic refill cadence, not a guaranteed exact delay after death. MaxMobs is a target, not proof of initial fill. SC:53–78 removes invalid weak references and retries failed placement up to three additional times per needed mob, dropping those extra retries after five consecutive empty top-ups.
- The first Summer band is **D(0)/E(5)**, not explicitly 1–5. Forced IDs take precedence over level bands (SC:181–200); omitted levels stay at 0. Summer's four distinct forced cadence values must stay as shown (30,45,50,60 seconds); no uniform boss interval is implied. FourthZone actually assigns radius 400 (GM:1237), despite a nearby comment mentioning 500.
- Validation is **D(false) for exactly the first ten Summer pockets**, E(true) for the last eight and all seventeen Winter pockets. Moving Origin changes the ten relative centers and every NPC, but does not translate the other twenty-five centers. F uses the full facing vector while R uses yaw only; the extra V is world Z. None of these formulas rotate the spawner itself, or rotate the NPC offset/180-degree yaw with PlayerStart.

## Static nav/ground contract; runtime remains open

For D(false), SC:139–145 samples a world XY angle/radius `[150,SpawnRadius]` and adds world Z=100 to the spawner center. There is no safe-ground/nav validation on that path. For E(true), SC:146–179 requires nav/main data and the MobClass default capsule, then requests up to eight attempts from FindSafeSpawnLocation. SC:81–128 samples the same radial range, traces ECC_Visibility from Base.Z+200 to Base.Z-200, requires normal.Z >=0.7 and ground height within 60 of Base.Z-50, projects ground+20 with extent (50,50,60), limits projection XY displacement to 50 and Z displacement to 35, then requests nav location + capsule half-height +2. Radius 150 therefore makes the sampled radial range 150–150, not a disk from the center.

SC:203–222 uses DontSpawnIfColliding for validated mobs and AdjustIfPossibleButAlwaysSpawn for unvalidated mobs; chosen archetype is assigned to a deferred mob before its finish. This is the **mob** collision policy, distinct from the spawner call and NPC policies. No runtime test of actual terrain, nav connectivity, capsule collision, fill counts, aggro/leash, Blueprint overrides, dialogue or NPC adjusted positions was made. Historical probe comments in GM were not promoted to current evidence. PIE, actual spawned world transforms and source-engine acceptance remain open with the parent.

## Actual command record (inspection/document creation only)

PowerShell commands were run through exec_command. Main was inspected once using `Get-Location`, `git status --short`, `git log -5 --oneline`, `git worktree list`, `Get-Content AGENTS.md`, `Get-Content docs/AGENT_WORKFLOW.md` and `Get-Content 'C:/Users/OZAN CIDIK/Desktop/ozan online/MyProject/.agents/skills/ecc-mmo-plan/SKILL.md'`. Remaining source inspection used the assigned worktree. Commands below record the actual reads/searches; repeated reads are grouped, not represented as tests.

```powershell
git status --short
git log -5 --oneline
Get-Content AGENTS.md
Get-Content docs/AGENT_WORKFLOW.md
Get-Content 'C:/Users/OZAN CIDIK/Desktop/ozan online/MyProject/.agents/skills/ecc-mmo-plan/SKILL.md'
Get-FileHash Source/MyProject/Private/Core/MMOGameMode.cpp -Algorithm SHA256
rg --files -g AGENTS.md -g '*MobSpawner*' -g '*NPC*' docs Source
Test-Path docs/tasks/S02_00_LAYOUT_BASELINE_2026-10-16.md
rg --files --hidden -g AGENTS.md -g '!Content/**' -g '!Saved/**' -g '!.git/**'
rg -n 'BeginPlay|SpawnMobArea|Origin|PlayerStart|SpawnActor.*NPC|SpawnLv|SpawnRift|SpawnIce|Spawn.*Pocket|IsWinter|Winter|Summer' Source/MyProject/Private/Core/MMOGameMode.cpp
rg -n -C 4 'MobAreaDistance|bSpawn|StartPlay|SpawnMobArea' Source/MyProject/Public/Core/MMOGameMode.h
Get-FileHash Source/MyProject/Public/Core/MMOMobSpawner.h,Source/MyProject/Private/Core/MMOMobSpawner.cpp,Source/MyProject/Public/Core/MMOGameMode.h -Algorithm SHA256 | Format-List Path,Hash
git diff --name-only
git diff --cached --name-only
rg -n '^\s*Spawn(Frostbound|Rift)\(|SpawnActor(?:Deferred)?<AMMO(?:Vendor|QuestGiver|Healer|Warehouse|Talker)NPC>' Source/MyProject/Private/Core/MMOGameMode.cpp
git rev-parse HEAD
git diff -- Source/MyProject/Private/Core/MMOGameMode.cpp Source/MyProject/Private/Core/MMOMobSpawner.cpp Source/MyProject/Public/Core/MMOMobSpawner.h Source/MyProject/Public/Core/MMOGameMode.h
```

Line-numbered `Get-Content`/`ForEach-Object` reads covered GM, SH and SC; bounded `Get-Content` array loops re-read GM lines 821–1140, 610–634 and 1137–1435 after the combined full-file output truncated. An inline PowerShell regex inventory sliced `$taskLines[971..1092]` (Winter) and `[1093..1434]` (Summer), joined with newlines, and counted `SpawnActorDeferred<AMMOMobSpawner>`, anchored `^\s*SpawnFrostbound\(` / `^\s*SpawnRift\(` and NPC expressions. Winter returned 3 spawner expressions (one helper definition), 15 Frostbound calls and 4 NPC calls: **3-1+15=17**. Summer returned 16 spawner expressions (one helper definition), 3 Rift calls and 5 NPC calls: **16-1+3=18**. Manual per-block extraction supplies the rows above. Source diff output was empty. These are static inspection results, not test passes.

The sole written file was this note, created and finalized with apply_patch. Final freeze reads use `Get-FileHash` for the note and GM plus `git status --short`; the resulting note SHA256 is returned separately to avoid a self-referential hash. No inputs are modified for hash normalization.

## Coordinator acceptance — 2026-10-16

The preceding specialist snapshot was frozen as SHA256
`6B61F66C40E8DA60DDEF8DADA9A3EF84AB0D6E6FC6D5240BCFA9A41CE44B5538`.
Franklin (`01a13fe0-dc73-77b3-a84c-3352cc3a5d65`) subsequently accepted every
35-pocket and 9-NPC row and the supporting branch/default/lifecycle/nav descriptions;
no corrections were required. His review was read-only and ran no tests or UE.
The opening pending-audit statement records the earlier specialist handoff.

The coordinator read the complete snapshot, checked its hash and all four source
hashes, independently counted 18 Summer/17 Winter/9 NPC data rows, and inspected
the affected tooling consumers. Main and baseline GameMode/parser Git blobs match
after Git's line-ending normalization; raw byte hashes are not conflated.
Only this note, the canonical plan's preparation status and the sprint board are
integrated. Existing staged Trade and unowned Race/ZoneLayout edits are excluded.

This accepts S02.00 static preparation only. S02 gameplay integration awaits S00
PIE; the source-engine gate belongs to S01. No new build, fixture, runtime or PIE
result is claimed. Relevant future parser/negative-test adaptation and the existing
gameplay acceptance sequence are recorded in the sprint board.
