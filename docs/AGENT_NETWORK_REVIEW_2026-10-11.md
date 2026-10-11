# Multiplayer network/security integration review — 2026-10-11

Owner: network/security review agent; ownership is limited to this new document. Source review only; no gameplay edits, builds, runtime/PIE tests, or commits. Applied `.claude/agents/ag-guvenlik-denetcisi.md` and `.agents/skills/ecc-mmo-review/SKILL.md`. O66 reentrant death QA is excluded.

Root: `C:/Users/OZAN CIDIK/Desktop/ozan online/MyProject`.
Baseline: `a70259097b2439e9d13f4bdcbd214583e9db6796`.
Latest observed HEAD: `b93a456d2d0a9770a10b9127adecbc712740ed2c` at 2026-10-11 02:00:32 UTC (05:00:32 Istanbul).
HEAD advanced during review through a peer commit affecting EconomyModel.py, DECISIONS_NEEDED.md and ACCOUNT_IDENTITY_DESIGN.md; `git diff a702590 HEAD --` the four finding files below was empty. Findings compare the named baseline with active working bytes, not with a proposed immutable patch.

## Findings for parent integration

### NR1 — High: mailbox recovery can deliver an already refunded attachment

**Status:** **resolved-in-unverified-working-tree** for the specific sequence below, after a peer fix observed at 02:00:40 UTC. This recovery/refund interaction is absent from committed a702590.

**Earlier working evidence (store SHA256 `662571205fb22291a047e47350f37265f5438e63504aeac841dda4623a9f7de7`):** `Source/MyProject/Private/Mail/MMOMailStore.cpp:117` promotes a surviving temporary file when the destination is absent; `:149` writes that temporary file before moving it. `Source/MyProject/Private/Components/MMOMailComponent.cpp:253` deducts/saves the sender, then `:264-267` refunds/saves on Append failure. Collection loads the recovered box at `:294` and credits it at `:360`.

**Concrete failure sequence:** sender A sends 100g to an existing recipient B with room/capacity; sender debit saves successfully; mailbox temporary write succeeds; final move fails through its retries, leaving no destination and a valid temporary file; Append returns false; A's refund saves successfully. After the filesystem fault clears, B's next Load promotes the temporary file and collects the 100g. A retains the refunded funds and B receives 100g. Requires a filesystem failure; this is a source-confirmed failure path, not a demonstrated remote-only exploit.

Installed UE `FileManagerGeneric.cpp:303,323,350-353` deletes the destination before moving and can return false after move retries. Recovery cannot infer sender transaction status from the presence of a temporary file.

**Committed distinction:** a702590 `MMOMailStore.cpp:105-125` has no temporary-file recovery; its `MMOMailComponent.cpp:194-205` refunds failed Append without immediate character persistence. Do not attribute the new recovery/refund sequence to that commit. The peer's new sender-save ordering mitigates the older crash duplication window but does not close NR1.

**Peer resolution:** latest store SHA256 `f156381a9f4a121ce7dce6bea2ae414c877f8ca10311446087ae27aceb607432`, WriteBox `:154-174`, now attempts a direct destination write after Move failure. If the destination is absent and the complete temporary file survives, it returns success, so SendMail does not refund the transfer that Load can recover. With a destination present it attempts to remove the temporary file before returning failure. The cited no-destination/temporary-file/refund sequence is therefore no longer supported by these bytes. This is source-only closure of that sequence, not validation of all fallback-write, cleanup-failure or crash cases. Parent should verify recovery/settlement conservation before treating mail persistence as complete.

### NR2 — Medium: completion and character rewards commit separately

**Status:** open for crash consistency in both committed a702590 and reviewed working bytes; the previous unchecked-save subfinding is **resolved-in-unverified-working-tree**.

**Committed evidence:** `Source/MyProject/Private/Components/MMOAchievementComponent.cpp:374,384,402` and `Source/MyProject/Private/Components/MMODailyComponent.cpp:350,358,379,386` save completion before granting rewards. Their SaveNow functions return void (`:270` and `:235`).

**Working evidence:** achievement `:447,465,483`; daily `:439,456,477,484` still persist completion before granting gold/XP. Character autosave is separate (`Source/MyProject/Private/Characters/MMOCharacter.cpp:933`, 60 seconds). Inventory gold mutation at `Source/MyProject/Private/Components/MMOInventoryComponent.cpp:392-420` changes memory/notifies; it does not persist the character. The active store remains local (`Source/MyProject/Private/Save/MMOCharacterStoreSubsystem.cpp:151`; local Save at `MMOLocalFileCharacterStore.cpp:19-27`).

**Concrete failure sequence:** complete an eligible achievement/daily bounty; completion JSON saves; terminate the server before Grant, or after Grant but before any character save. Restart: completion remains claimed, while the older character save lacks that reward; evaluation cannot award it again. No hostile identity selection is required.

**Peer resolution since initial review:** achievement SaveNow now returns bool (`:329`) and EvaluateAll rolls back new unlocks on failure (`:447-454`); daily does the equivalent at `:313,439-446`. Therefore the earlier statement that both continue paying after a reported SaveNow failure is stale and withdrawn for these snapshots. These changes have not been built or executed by this agent. They do not reconcile a successfully saved completion with an unpaid reward after a crash.

**Next fix:** pending rewards with stable IDs and idempotent settlement, or an atomic character/completion commit.

### NR3 — Medium: pending mailbox path change lacks migration

**Status:** open-in-reviewed-working-tree; absent from committed a702590.

**Committed evidence:** `Source/MyProject/Private/Mail/MMOMailStore.cpp:38` uses `Saved/MMOMail/ali.json`.
**Working evidence:** the same function at `:39` uses `Saved/MMOMail/MMOMail_ali.json`; Load reads only that new path (`:113-128`).

**Concrete failure sequence:** retain a valid committed-format `ali.json` containing an uncollected 100g attachment; deploy the working path change; Ali connects and collection reads the missing prefixed path as an empty mailbox. The old file remains on disk, but its attachment is inaccessible through the current API. No runtime migration/fallback is present in the reviewed store/component source. This is an integration requirement for an active peer change, not a claim of a shipped regression.

**Next fix:** migrate legacy boxes once, with explicit conflict handling when both paths exist; preserve message uniqueness and attachments across restart.

## Known identity gap and reviewed network boundary

K-1/Y1 is pre-existing, not a new finding: name selects save identity. Committed and working `Source/MyProject/Private/Characters/MMOCharacter.cpp:1113-1143` derive the slot from PlayerState name; committed load at `:1381,1401` (working `:1382,1402`) loads it without account ownership proof. `MMOGameMode.cpp:540-580` checks admission password/online collisions, not offline account ownership. Sequence: join with an offline victim's accepted `?Name=` (and server password if configured), then automatic load selects that victim's character; mail uses the same name (`MMOMailComponent.cpp:40-51`). See existing SECURITY_AUDIT K-1 and NETWORK_AUDIT Y1. Production mitigation requires verified account/character identity independent of display name.

The reviewed feature APIs are chat RPC routes, not separate HTTP endpoints. In both baseline and working `MMOPlayerController.cpp:1606,1634,1649`, commands require a valid SeenCommandPawn matching GetPawn; `Public/Player/MMOPlayerController.h:98-99` declares the owning controller's Server/Reliable RPC. Shared handler throttle/input truncation is at `:1384-1392`. Achievement replication is owner-only Unlocked/public ActiveTitle (working component `:58-59`; committed `:46-47`); daily/mail disable replication (working `:47`/`:21`). No new ownership bypass was confirmed in these paths. This does not establish real remote-client behavior or serialization/flood resilience.

## Bounded next development task

Parent follow-up: assign **mail persistence recovery and compatibility** to the mail owner, scoped to MMOMailStore, MMOMailComponent and their existing tests/docs. First verify the peer's NR1 failure/recovery change against the acceptance cases below and address any remaining settlement inconsistency; include the still-missing legacy filename migration. Introduce stable transfer IDs/durable status if needed for replay-safe settlement. Acceptance: injected final-move failure followed by recovery cannot refund and deliver the same transfer; restart at each send/collect boundary conserves attachments and charges the fee once; old-only and both-path mailbox fixtures preserve each message once. Future checks are proposed here, not run. Keep NR2 reward settlement and the account identity project as separate follow-ups.

## Exact source snapshots

Per-file captures/rechecks: 2026-10-11 01:57:54–02:00:40 UTC. These are not an atomic workspace snapshot; peers remained active. Hashes identify this persistence recheck, not the earlier chat's unrecorded bytes. Line references above bind to the working SHA256 below, except the explicitly historical NR1 pre-fix references. Rehash before implementation; a mismatch requires rereading that file before reusing the claim. Raw-byte SHA256 preserves line endings, so a hash difference alone does not prove a semantic change. Baseline bytes were read with `git show a702590:<path>` through a byte stream.

| File relative to root | Committed a702590 SHA256 | Reviewed working SHA256 |
| --- | --- | --- |
| `Source/MyProject/Private/Mail/MMOMailStore.cpp` | `314e31bf31cf966d43f7d4d13380c67f39d12c7908c5d687c89325ad7c5ddfbd` | `f156381a9f4a121ce7dce6bea2ae414c877f8ca10311446087ae27aceb607432` |
| `Source/MyProject/Private/Components/MMOMailComponent.cpp` | `5585d0b34373151ffabbbad7b14846932d0bdf945fcfd8e577f309a429fc09ca` | `2d876b06ba9cb02a1a2af3c9d702ad24b55241f73f5c93691962c1e6c1b147fe` |
| `Source/MyProject/Private/Components/MMOAchievementComponent.cpp` | `0a26d4e879727e5634ecc820039f0484d3954c24dd17d1dcff0d1e04c78185b7` | `086359ac43d0ca20efa425d924a630059a7e81993ae5e0fa4a7249cc2a8c7c6a` |
| `Source/MyProject/Private/Components/MMODailyComponent.cpp` | `f25b80e4da07ed377ac7f472b9d9d1bc369760962fdb527065d7601b57e722fd` | `4f72c789b8b22960f8d523d9bf1750884769304c1ace593aba59bcea1d08cc07` |
| `Source/MyProject/Private/Characters/MMOCharacter.cpp` | `1f4aaa57b473208a81364a08e4c7f8d7109a5c4bcf833564ca64dc822d65ac88` | `8a56e7c7d40ea96da908d5b61b1ee573e5d2d761550bff4c08c59fa870693e09` |
| `Source/MyProject/Private/Core/MMOGameMode.cpp` | `388f60b93be2d462235523f231f65f68c6a00229c6f24b897239fc05dae0a675` | `388f60b93be2d462235523f231f65f68c6a00229c6f24b897239fc05dae0a675` |
| `Source/MyProject/Private/Player/MMOPlayerController.cpp` | `0f95bd6fd2d215b65d35107f7e06a7e524b6cc61a5ceaceaed7d954f362d0596` | `f39642a05d2cdc39e5e111db8708cae016d69b3b880de32ecaa90762463228dc` |
| `Source/MyProject/Private/Components/MMOInventoryComponent.cpp` | `c643c35ae03ea533d088fc83226adb43c91ed50d73ee46f156071e07d467250d` | `232adccae8c5fc689f017b2649b258431ffd48826c9fa701892221f9f7353d36` |
| `Source/MyProject/Private/Save/MMOCharacterStoreSubsystem.cpp` | `4620bbacb6ec72e43888e7e4d1f0fdf6e90adf24f373214ea505727b35dc8d2e` | `4620bbacb6ec72e43888e7e4d1f0fdf6e90adf24f373214ea505727b35dc8d2e` |
| `Source/MyProject/Private/Save/MMOLocalFileCharacterStore.cpp` | `2a1cec2ac5d455799499820bf02defe8ed1e8ab0f032e4dc53f635d4ef40103b` | `2a1cec2ac5d455799499820bf02defe8ed1e8ab0f032e4dc53f635d4ef40103b` |
| `Source/MyProject/Public/Player/MMOPlayerController.h` | `79d321ab5b9b13a9b15490398e282fdbbe48e77b1f6616479cdcdf725042d433` | `7a25f68cb4ea5a0c8199e2ffc15a62dc226517e5e90e8c014777fb471f9f48b7` |

Engine evidence outside the repo: `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Private/HAL/FileManagerGeneric.cpp`, SHA256 `9154eb240b9cb447ffbd71bad498d3a79975af3882e3ee06061ee2816f5ce7bb`.

Validation limits: source reads, git show/diff/status/log and SHA256 only. No disk-fault injection, restart, PIE, build, or runtime checks were performed. No peer-owned file was edited by this agent.
