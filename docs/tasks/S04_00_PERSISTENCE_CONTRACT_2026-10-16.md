# S04.00 persistence boundary — frozen preparation, 2026-10-16

Backend specialist; documentation only; Franklin final QA/security audit pending. No implementation, endpoints, schema application, engine/DB/network calls, installs, builds, tests, commits, descendants or chat messages. Only this new note is owned. Parent separately catalogues side-data imports.

Inputs: main `d085c132cb8b58779328b2ffa1a41ba2ee785ac1`; isolated `4e0f0c5a5ede948b69c324953f96b88a3ec61dc9`. Root `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/`; source aliases below resolve beneath `Source/MyProject/`. Canonical P/B resolve beneath main `C:/Users/OZAN CIDIK/Desktop/ozan online/MyProject/docs/`. Read-only receipt rehash: 141/141 accepted inputs match, no mismatches; no new runtime evidence.

**Canonical requirements / open decisions.** P:25–28 and B:12,25–28 require PostgreSQL + intermediate service, Steam-first, asynchronous revision protection, idempotent import and failure without advancement. P:179–194 proposes JSONB using the existing envelope; V1 normalized tables do not represent the complete save. No V2/schema is implemented or applied here. S13 language and S16 mail format remain **OPEN** (P:507,510); no Python selection. StableCharacterId/account ownership belongs to S06, verified Steam identity to S05. SlotName or client-claimed SteamID is never authentication. Production identity/ownership acceptance requires S05/S06; isolated S04 adapter testing remains allowed without a new canonical dependency, and independent S04 preparation does not pass S00 PIE or source-engine gates (B:21–28,36).

**Existing behavior.** I:74–106 exposes synchronous `Load/Save/Exists/ListSlotNames`, bool-only results, and `LoadAsync(bool, Data&&)/SaveAsync(bool)` adapters. Local adapters execute synchronously and invoke completion inline, same frame, before returning. Remote completion MUST run on the game thread; SaveAsync MUST copy `In` into owned lifetime before returning (I:84–87). Tracker operations assert game-thread execution (I:22–35).

L:8–43 uses Unreal `UMMOSaveGame`, UserIndex 0, `Saved/SaveGames/MMOSave_*.sav` (LH:9–11), correcting the older “local JSON” report. JH:14–32 and J:405–525 provide the existing `{SchemaVersion:2, Data}` candidate payload: int64 decimal strings, guarded v1 reading and strict validation; FromJson preserves OutData on failure, while ToJson resets OutJson on entry. This serializer does not change local storage.

S:148–165 always selects local storage; no DB selector exists. SH:16,27–35 owns it with TUniquePtr; Deinitialize resets it. Captured shared tracker survives, store resources do not. A future adapter needs independently owned in-flight state and explicit shutdown/drain handling.

C:1212–1270 builds authority/load-gated component snapshots and map/transform, with synchronous rename-target Exists protection. C:1196–1204 advances ActiveSaveSlot/clears deferred state only after Save succeeds. C:1287–1315 captures weak actor, target slot, shared tracker and ticket; successful newest completion updates ActiveSaveSlot. It captures no LoadSerial/current identity generation and rechecks neither possession nor authority. An obsolete ActiveSaveSlot update after same-actor reload/rename is a potential future delayed-adapter race; current local completion is inline, with no observed runtime finding.

C:1360–1420 distinguishes missing local slot (starter pack, saving enabled) from existing unreadable/invalid data (saving disabled); after early guards it increments LoadSerial. C:1422–1477 applies skill→stats→inventory→quest→clan→friends, then identity/location; nav waiting keeps saving gated. No production LoadAsync caller exists. Remote unavailability MUST NOT become Exists=false or Load=false interpreted as absence; authoritative absence, existing-invalid, and unavailable need distinct outcomes before starter-pack decisions.

G:698–789 installs request serial/pending weak actors before dispatch, accepts inline completions, suppresses duplicates, and checks request membership. G:792–918 aborts timeout, ignores late barrier results, revalidates players/combat, synchronously refreshes every final snapshot before ServerTravel, and conditionally flushes deferred saves on abort (GH:107: 15 seconds). C:942–966,1082–1097 handles detach/EndPlay saves.

**Migration blockers.** Async overrides alone are insufficient: C:935,974,1198,1488,1552,1575 still uses synchronous autosave/load/manual Save/Load; teardown and final travel refresh also require completion. Returning true for enqueue is invalid. M:178 requires an immediate character-save result; T:687, F:379, K:623,787,824,887 request synchronous saves. Exists name checks (C:1233,1363,1397,1659,1709; G:505) and clan ListSlotNames/Load (K:376–380) need asynchronous admission/lookup migration. Move these callers to completion-driven flows; retain owned teardown snapshots beyond actor lifetime. Mail/trade cross-record economy transactions remain separately pending.

**Proposal, unimplemented.** Minimal language-neutral metadata: operation/request ID, idempotency key bound to operation/character/payload digest, S06-resolved character/account context, authenticated server/session, ownership epoch, ExpectedRevision, deadline, payload/schema version; local actor/load/request generations for completion routing. Result: matching IDs, outcome/error category, durable disposition (committed/rejected/unknown), committed revision/epoch and validated load payload. Categories: authoritative-not-found, invalid/unsupported, unauthenticated/forbidden, revision-conflict, ownership-lost, unavailable, deadline/cancel-outcome-unknown, internal. Do not overload bool failure.

Propose atomic ownership/epoch + revision validation with durable idempotent outcome recording; acknowledge success only after commit. Duplicate identical requests return recorded results; conflicting reuse rejects. Marshal one terminal completion on game thread, supporting inline local reentrancy. Apply results only to matching live actor/load/identity/request generations. Unknown outcomes freeze dependent advancement pending request-outcome reconciliation and authoritative reload; do not blindly retry/rollback or revive travel.

**R1 remains OPEN.** I:63–69 protects callback order only. Local ticket, LoadSerial and RequestSerial are not durable fences. Contrary to P:514's closure suggestion, CAS alone permits a timed-out write if ExpectedRevision still matches and no newer commit/epoch revocation occurred. Cancellation needs an authoritative commit fence/revocation acknowledgement or reconciliation; cancellation racing commit may already have committed. No cancellation guarantee exists today.

Future acceptance matrix (all NOT_RUN; expected future contract):

| Failure / race | Expected memory / durable behavior | Future test |
|---|---|---|
| DB unavailable / invalid existing save | No starter pack or save enablement / unchanged record | Distinct unavailable, invalid, authoritative-absent loads |
| Definite write rejection | No success, travel, or dependent reward / unchanged | Inject rejection at every migrated sync caller |
| Old revision / revoked epoch | No stale completion apply / rejected write | Reorder two writers; remove each guard mutation |
| Timeout, unchanged revision/epoch | Block travel; reconcile / commit may exist | Delay commit beyond timeout without newer write |
| Cancel versus commit / lost acknowledgement | No assumed rollback / recover recorded outcome | Commit before/after revocation; drop response |
| Duplicate / conflicting idempotency reuse | One effect / same result or rejection | Retry identical and altered payloads |
| Reload, rename, detach, shutdown | No obsolete actor mutation / owned operation reconciled | Delay completion across each lifetime boundary |
| Inline local / deferred final refresh | No premature success/travel / final snapshot acknowledged | Same-frame callback and delayed refresh failure |
| Repeated legacy import | No overwrite/duplicate / recorded once | Summer/Winter field equality and restart replay |

Evidence SHA256 manifest (exact baseline paths; references use alias:file-line above):

| Alias | Path | SHA256 |
|---|---|---|
| I | Public/Save/MMOCharacterStore.h | 2AA0ADA2D3C3F3A49F5387CA170B4BFD4DC129DA0132B710647DC81B91DDBF9E |
| LH | Private/Save/MMOLocalFileCharacterStore.h | 08AFB27DE0F38AA5EDFEC48ED560FFACDDD5BEF4728B864DC835A1630D0B48F4 |
| L | Private/Save/MMOLocalFileCharacterStore.cpp | F7EDCC2143EB8822F8FB07FFD61DD94D3EFA37BE316AB0A5310A22885B91372E |
| SH | Public/Save/MMOCharacterStoreSubsystem.h | 71A243D6CFE3C94BAB91DF30AB38E9499F80F0BAD3F53918A19172077A6CBFCE |
| S | Private/Save/MMOCharacterStoreSubsystem.cpp | 75F5FE6897BF685D24CD046F710DA7D1C4FF7A28E0BA8F691981A246E5793BA9 |
| D | Public/Save/MMOSaveGame.h | 01943F7689E9396AAB6299B054D056BD19F2ACE90900E85EFD0A9E9DF3BAE45D |
| JH | Public/Save/MMOCharacterSaveJson.h | 50CE4D8822AD7FBC5E8D0427B8F1DC367F07DF96476E8C934F52EA147F471F16 |
| J | Private/Save/MMOCharacterSaveJson.cpp | CBA63E4FB0DA5F295297275611230FA550894CA00E48482D50C5289AE6D35214 |
| CH | Public/Characters/MMOCharacter.h | 5363DA2759C6D18A457F5C494635802DFDEC7E62C7C81D16D3640388D65688CE |
| C | Private/Characters/MMOCharacter.cpp | 029DBC437E7687230AED9A13412DB6522D2A16F006F4DD70567A888EED7F3E56 |
| GH | Public/Core/MMOGameMode.h | EE173D7B51AD57E72E678C5E7B183C7CEA400046CECCACC12D54E422C8572E9B |
| G | Private/Core/MMOGameMode.cpp | FE66331BA1F3CFA3DF4829CBE0DB75A42FF121427B50734CFC81CDE61FA46A45 |
| K | Private/Components/MMOClanComponent.cpp | 6747DB626DE83F5DF79D594F8783A2280CBCC40586BC8FD63CFF2D90838D160C |
| F | Private/Components/MMOFriendsComponent.cpp | 4A814143BB1E9B4A2BB5DCEE7C162BBC0949DD790C7C3283ACD226743E806C28 |
| M | Private/Components/MMOMailComponent.cpp | BB416F03066FF54A2409B7EC58963AD5AE2FBE8A5618D779A98203E47A53492E |
| T | Private/Components/MMOTradeComponent.cpp | 41F3484D7F4B044DA1B979967482FD25CEC4BD54AF80854FA765C99B04DE2542 |
| P | SIX_MAP_PLAN.md (main) | F3F35A5DD2E6BCA1ED5740073F531914EB60F4E0B55AEE85FC84E7B4EBA5C256 |
| B | SIX_MAP_SPRINT_BOARD.md (main snapshot) | 2AFC0C1A92A38AD13DC4BAFFCF2BF3BDC69476BAFE2C50F016FDA9EFC0B37601 |

## Coordinator acceptance — 2026-10-16

Franklin (`01a13fe0-dc73-77b3-a84c-3352cc3a5d65`) accepted the refrozen
specialist snapshot SHA256
`2808F5B8DD7644CF60BBA672D7295433EB37CD45EC459C8DAB1C453D2FA8F3B7`
after three wording corrections. Its opening pending-audit statement records
the earlier handoff. Dependency boundaries, converter output behavior and the
potential future delayed-adapter race are now distinguished; no blocker remains.
The board's separate side-data/import table also received independent acceptance.

The coordinator read the complete snapshot, verified all 18 evidence rows and
all 141 accepted inputs, and checked the sync/save, absence and travel-refresh
boundaries in source. P/B refer to pre-integration inputs retained at
`C:/Users/OZAN CIDIK/AppData/Local/Temp/s04_00_review_inputs_4503c02408224a15aeb5010e7b0b674c/`;
their hashes above remain snapshot hashes after the live plan/board changes.
The live plan's R1 closure assumption was corrected as part of this delivery.

Only this note, the canonical plan and the sprint board are integrated. Peer
staged Trade and unowned Race/ZoneLayout changes remain excluded. No new build,
tests, runtime, DB/HTTP/Steam calls or import were performed. This accepts static
preparation only; R1, S13 language, S16 mail and the existing sprint gates remain
open. It does not add an S05/S06 prerequisite to isolated S04 adapter testing.
