# O68: existing mailbox read failure

Date: 2026-10-11. Current integration base: `a1f614581e794d2d7df8e18a139dfc7d11c68efb`.
Historical regression base: `aac6dffb0c7dec13873409a2e9f96d78269357eb`.
Shared isolated worktree:
`C:\Users\OZAN CIDIK\.codex\worktrees\mail-read-failure-integration\MyProject`.
Evidence root:
`C:\Users\OZAN CIDIK\AppData\Local\Temp\mmo_mail_read_failure_20261011`.

Status: **NINE_ISOLATED_CHECKS_AND_MAIN_EDITOR_BUILD_PASS; parent commit PENDING**.
All nine current-base result records pass: EditorBuild, GameBuild, Fixed24,
Mail104_retry1, ValidateData, StaticLint, Summer, Winter and NetSmoke.
Documentation independently read the result records, raw test/error/connection
lines, invocation/launch records, source hashes and binary hashes.
Historical aac6dff baseline: 15 PASS / 9 FAIL, native exit 0 / runner exit 1;
current a1f6145 candidate: fixed24 PASS, native/runner exit 0. No current-base
unfixed FAIL run is claimed.

Parent integrated the four source/runner files, byte-identical to the compiled
candidate BC7/824/776/FD5; preintegration guarding preserved the 1043-path
baseline and peer changes before applying those owned changes. Main remains
at a1f6145. QA's final acceptance confirms main Editor build PASS without
runtime. Documentation read that acceptance and matched the fresh main DLL.
Documentation has not copied these docs to main; parent copy/commit remain
PENDING. No main runtime, PIE, restart, NR1 or NR3 closure is claimed.

Main advanced through peer commits afba349 and a1f6145 while the earlier
worktree was in use. The new isolated worktree starts at a1f6145. Sagan merges
only the minimal Load correction into the peer Store; retain peer WriteBox,
quarantine, refund and 104-check test changes. Do not overwrite a current
source file or checklist with the historical whole file. The historical
worktree remains frozen; this documentation task reads it only.

Parent supplied Singer's final independent raw-evidence approval: PASS.
Singer confirmed exactly 24 unique PASS labels matching the same-label
historical test, denied_reads=4 and write/move/delete=0, preserved original
bytes and restored old/new mail order, native/runner exit 0, and exact matching
of all four candidate source hashes. This is an independent backend acceptance
reported by parent; documentation's direct artifact checks are recorded below.

UE, network/security and backend specialists completed and were closed after
current compiled-source/native proof. QA completed its final acceptance and
stopped; documentation freezes this scoped handoff. Claude: NOT_RUN
for this O68 specialist workflow.

## Ownership and actual specialists

Runtime type=`default`, explicit specialist instructions. `Claude: NOT_RUN`.
The actual specialist assignments are recorded in evidence-root `agents.json`.

| Specialist | Actual agent ID | Scope |
| --- | --- | --- |
| UE / Sagan | `01a12936-fba9-71b3-8e3b-cbd2d5237aee` | Store correction and native regression |
| QA + sole DevOps / Avicenna | `01a12937-670e-7f51-ac75-5325e6d9bde8` | Independent acceptance, runner, serialized builds/runtime |
| Network/security / Boyle | `01a12937-6996-74e1-b556-2b7c3927f375` | Read-only safety/security review |
| Backend / Singer | `01a128c4-4b9f-7ff0-a8d5-ad4c4185ffdf` | Read-only persistence review |
| Documentation specialist | `01a12940-5ddd-7c21-be76-322fbbdfc93e`, runtime type=`default` | New task note and appended O68 checklist only |

Implementation/QA candidate paths, owned by those specialists:
`Source/MyProject/Private/Mail/MMOMailStore.cpp`,
`Source/MyProject/Public/Mail/MMOMailStore.h`,
`Source/MyProject/Private/Tests/MMOMailReadFailureSelfTest.cpp`,
`Scripts/TestMailReadFailure.ps1`.

Documentation owns only this new file and the appended O68 section in
`docs/PLAYTEST_CHECKLIST.md`; preserve all previous checklist bytes, including the peer's 17 K1F lines.
The documentation specialist does not run UE, touch Saved, commit, change
automation or edit main. Parent owns integration and final acceptance.

Read in main: `AGENTS.md`, `docs/AGENT_WORKFLOW.md`, project
`ecc-mmo-plan`, `ecc-mmo-review`, `ecc-mmo-verify`, `mmo-feature`
and `ue-build` skills. Inspected Git status, recent commits and registered
worktrees; main's pre-existing untracked `.agents/` is outside this scope.
Read the candidate diff, complete native regression and runner, store callers,
prior compatibility note and preparation evidence. Source review is distinct
from executing the proposed regression.

## Failure and minimal correction

After the existing temporary-file recovery branch, baseline `Load` returns
true with empty output when `LoadFileToString` fails, including an existing
mailbox whose contents cannot be read. `Append` can then write only the new
mail, overwriting original messages and attachments if writes remain allowed.

The reviewed candidate keeps missing-box success: after recovery,
`FileExists == false` returns true with empty output. An observed existing
file whose `LoadFileToString` fails returns false with empty output instead.
`Append` and `TakeAll` consequently stop before `WriteBox`. The existing
`CollectMail` Load guard also returns before Replace/reward delivery.
No UObject, RPC, component, identity, schema or WriteBox change is proposed.

Limits:

- `Count` still returns 0 for failed Load, indistinguishable from empty mail.
  A zero count alone is not a successful-read signal.
- Explicit `Replace` still calls WriteBox directly; it is not guarded by this
  Load correction and is unchanged.
- Existence/read races remain; a false FileExists result can also hide
  permission or filesystem errors. This fix covers the observed-existing-file
  read-failure branch, not every inaccessible-path condition.
- Native store checks do not prove sender refund persistence, component
  SendMail/CollectMail behavior, HUD/RPC delivery or restart conservation.
- No remote exploit is demonstrated or claimed.

In SendMail, Append failure follows sender debit/save and invokes refund then
SaveOwner. Refund-save failure can leave the debit on disk; O68 does not repair
that settlement behavior. CollectMail's failed Load returns 0 without a new
error notification; observation of the actual failed read is required in PIE.

## Candidate regression and safe reproduction plan

The nonshipping `MMOTestMailReadFailure` command requires
`-MMOMailReadFailureTest -game -unattended -NullRHI`, an authority game world,
game thread, atomic platform-file publication and one invocation per process.
It refuses editor/PIE/client contexts. It is not a PIE fault-injection tool.

The test seeds a fresh GUID mailbox through the real store and records its
bytes. A wrapper denies synchronous OpenRead only for that mailbox while
FileExists and writes remain forwarded/allowed. It counts denied reads and
fixture write/move/delete attempts and forwards other I/O. RAII restores the
platform-file manager; the immutable wrapper remains allocated until process
exit so outstanding I/O pointers are not retired on restoration. Thread-safe
counters and the atomic-publication guard bound this to a disposable process.

The 24 assertions cover missing-box success/output reset, readable
seed mail, an existing exact read fault in Load/Append/TakeAll/Count,
preserved bytes, no fixture write/move/delete attempts or .tmp/.bad creation,
manager restoration, and normal read/append preserving old and new mail.
The new test's missing/readable controls passed on the historical baseline and
current-base fixed run; empty-existing-box/.tmp controls passed in Mail104_retry1.
Empty-existing-box and .tmp recovery controls belong to the current peer
`Source/MyProject/Private/Tests/MMOMailSelfTest.cpp` / `Scripts/TestMail.ps1` 104-check suite,
not to the new 24-assertion matrix; its current run passed 104 distinct checks.

QA uses the same native test against historical original and current fixed Store,
with source/binary hashes and build logs identifying each variant. Baseline
must fail for the actual existing read-error/overwrite behavior; a missing
command, startup failure, crash or a lock denying writes is not that proof.
The fixed run must pass all 24 exact distinct labels, one PASS summary and
native exit 0, with actual denied-read counters and byte-preservation evidence.

During preparation, the writer restored the exact baseline Store blob for QA:
SHA256 `F156381A9F4A121CE7DCE6BEA2AE414C877F8CA10311446087AE27ACEB607432`.
Native test SHA256:
`77699B59D08A3F39C755D66824AB912BD51B061F40BBA3380EAAF19600AF6EBC`.
These were independently read locally. They identify the historical baseline,
whose compiled evidence is below; they do not identify the current fixed Store.

Current integration source hashes independently read by documentation:

| File | SHA256 |
| --- | --- |
| `Private/Mail/MMOMailStore.cpp` | `BC7CDC3B41FFC8CD64BE5D87DDA3D64734FAB844D14AB36A884FEF75E9C5A964` |
| `Public/Mail/MMOMailStore.h` | `824E7632553855CC3995578B38954BE735DB009E1094A1903A0D99470441E0FA` |
| `Private/Tests/MMOMailReadFailureSelfTest.cpp` | `77699B59D08A3F39C755D66824AB912BD51B061F40BBA3380EAAF19600AF6EBC` |
| `Scripts/TestMailReadFailure.ps1` | `FD5F8FA14A43942B4D8389EAC3031C665710596758CF99B44C222AF7D9637F1D` |

C++ paths in this table are relative to `Source/MyProject/`. These hashes
identify the current candidate. Build inputs stayed unchanged and fixed runtime
source hashes match. Documentation checked all four hashes against all nine
steps' inputs-before/after records and current files: unchanged. Current binary
hashes match both isolated build records; QA's final acceptance is linked below.
The integration Store diff preserves peer persistence changes.

## Accepted historical baseline and runner repair

Evidence subdirectory: `baseline_f897606e6d284429a8abdf76aa24361a` under the
evidence root. Documentation independently read the acceptance JSON, case
lines, invocation, source/binary hashes and build result/log.

- `editor-build-result.json` and `editor-build.log`: historical
  MyProjectEditor Win64 Development, native exit 0, Result: Succeeded,
  source hashes unchanged. DLL SHA256
  `F850CCAF0C3C68A7A1BB32A49B05FCFE31A52FFEDD04F8EDD080FB0F3198362A`.
- Accepted `original-nativeexit2-acceptance.json`: behaviorFailureProven=true,
  native exit 0, runner exit 1, no timeout; 24 cases / 24 unique labels,
  exact expected set, 15 PASS / 9 FAIL and one FAIL summary. Read denial reached
  Load/Append/TakeAll/Count: atomic=1, denied_reads=4, writes=1, moves=1,
  deletes=1. Append accepted the failed read and original-byte preservation
  failed; restoration could not recover the original mail. This is the
  intended behavioral reproduction, not an engine crash.
  `nativeexit2` is the retry label in the evidence filename, not native exit 2.
- Raw `original-nativeexit2-cases.log` and runtime directory
  `original-nativeexit2_298b03dc3aa34189be1d972f4d870316` retain ue.log,
  invocation.json, result.json, source hashes and preservation inventories.
  Acceptance reports savedPreserved=true, engineErrors=[]; the existing-entry
  changes JSON is empty. This is historical isolated-worktree evidence only.
- First `baseline-acceptance.json` / `original_cb64f15690a94bb8a6c18f2576c171fc`
  is retained **UNACCEPTED**: nativeExit=null, behaviorFailureProven=false.
  Its similar failing cases do not replace numeric-exit acceptance.
- QA repaired runner exit capture by retaining the child process Handle before
  waiting. Runner SHA256
  `FD5F8FA14A43942B4D8389EAC3031C665710596758CF99B44C222AF7D9637F1D`.
  `ps51-native-exit-retained-fixtures.json` and
  `pwsh-native-exit-retained-fixtures.json`: PowerShell 5.1.26100.9549 and
  7.6.5 each observe numeric exit 0 and 7 before/after Refresh, all PASS,
  engineStarted=false. These fixtures prove exit capture, not gameplay.

The accepted historical process used the old worktree's project,
`/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene`,
`-game -MMOMailReadFailureTest -NullRHI -nosound -unattended -nosplash -stdout`,
unique nested `-UserDir`, evidence `-ABSLOG`, and
`-ExecCmds="MMOTestMailReadFailure,quit"`; exact argv is retained in its
invocation.json. New fixed proof must identify the current a1f6145 integration
candidate and rebuilt binaries. Peer changes mean this is a historical
baseline comparison, not a claim that every other source file is identical.

## Verified current-base QA evidence

Current run subdirectory:
`integration_3c133391f56b44f88d3d828ff0ae617e/run_193fb236eade4baaaac3dfbb307f770d`
under the evidence root. Each named step has result.json, invocation.json,
command.log and input/preservation evidence. All nine records have exit=0,
nativeCommandExit=0, inputsUnchanged=true, savedPreserved=true and failure=null.
Documentation independently read the raw results, test/connection/error lines,
launch native exits and source/binary hashes. This is bounded isolated-candidate
acceptance. The separately accepted main Editor build is recorded below;
parent documentation copy/commit remain PENDING.

| Step | Actual bounded PASS |
| --- | --- |
| EditorBuild | MyProjectEditor Win64 Development; nativeCommandExit=0, exit=0, Result: Succeeded, fresh DLL |
| GameBuild | MyProject Win64 Development; nativeCommandExit=0, exit=0, Result: Succeeded, fresh exe |
| Fixed24 | Nested `fixed-current_8e1f156f87c345009281ffef4d102104`: nativeExit=0, runnerExit=0, no timeout; exact 24 unique PASS labels and one PASS summary |
| Mail104_retry1 | 104 cases / 104 unique / 104 PASS, one PASS summary; native child=0, runner/harness=0; exactly one expected fixture Move error, no unexpected errors |
| ValidateData | Engine bundled Python, exit 0, "Veri denetimi temiz."; existing skill-gap warnings remain in raw output |
| StaticLint | Engine bundled Python, exit 0, 125 files, "Statik kontrol temiz"; static result, not a build |
| Summer | Summer_scene smoke, child=0 / runner=0, zero error lines, 57 spawn lines |
| Winter | Winter_scene smoke, child=0 / runner=0, zero error lines, 79 spawn lines |
| NetSmoke | Summer_scene listen/server + client, runner/harness=0, selected port 17918, server join=1/client welcome=1, zero errors in both logs; owned server/client cleanup exits=-1/-1 |

Fixed24 raw `ue.log` contains atomic=1, denied_reads=4, writes=0, moves=0,
deletes=0; original bytes/mail survive and restored read/append retains old/new
mail in order. `runtime-source-hashes.json` matches the exact four source
hashes above, including the same native test hash/label set as the historical
baseline. Fixed24 result records native=0, runner=0 and savedPreserved=true.

Fresh Editor DLL SHA256:
`2ADDBE8E8BADDE96F51AC52E9B7DAE29679FA16E6888112B4BD20F4D6BBABDA2`.
Fresh Game exe SHA256:
`26D84338C591B0C4847622B7D2300425E29A8D6BE135B5A6648A19738552F809`.
Both build records have binaryBefore=null and fresh binaryAfter. Documentation
read the current binaries' hashes and matched these records. All nine steps'
before/after input records match the four current source/runner hashes and have
zero entries in saved-existing-changes.json. This preserves pre-existing
isolated-worktree data while allowing unique generated run entries; it is not
an assertion that all main/live paths stayed unchanged.

Actual Fixed24 invocation used bundled PowerShell 7.6.5,
`Scripts/TestMailReadFailure.ps1 -ExpectedChecks 24 -Label fixed-current`,
with `-EvidenceRoot` at this run's Fixed24 directory. Exact host/argv are in
Fixed24/invocation.json. Native invocation uses the configured Summer map,
unique UserDir/ABSLOG and reviewed opt-in headless authority command.
A Development Game build is not packaged/cooked runtime proof.

### Mail104 instrumentation retry and expected error

Initial `Mail104/result.json` is retained as an unaccepted prelaunch TEMP
harness failure, nativeCommandExit=null, exit=1:
`The variable '$script:O68Launches' cannot be retrieved because it has not been set.`
QA repaired TEMP harness scope to retain launch records; `Mail104_retry1`
then passed. Product and repository runner bytes did not change for this
retry; the harness records the unchanged original argv plus private UserDir
and retained process handle. This initial instrumentation failure is not a
gameplay failure.

Raw accepted log:
`Mail104_retry1/Temp/mmo_mail_current104_7789b25c41bf41ab88b34117a047b194.log`.
Exactly one expected LogFileManager Error moving file occurred for
`MMOMail_ztestmaild_01edaf5d.json.tmp` -> `MMOMail_ztestmaild_01edaf5d.json`
inside the unique `O68Integration_1c1a9a5d42484beeb40e71817b1944d8` test UserDir.
That GUID fixture makes the mailbox destination a directory, exercising the
peer's failed first-mail write/refund/cleanup path. The runner reports
expected_move_error_lines=1; the raw log has 104 unique PASS cases and no other
engine errors. This exact expected failure does not justify ignoring arbitrary
Move errors or claiming coverage of failed cleanup combinations.

### Refund coverage caveat and follow-up candidate

Mail104 raw informational notes, not conservation assertions:

- `[MMO_MAIL_NOTE] fee_clamp_refund result=10 final_gold=520 no_creation_max=505`
- `[MMO_MAIL_NOTE] gift_callback final_gold=10684 start=10000 gift=1000 charge=316`

The 104-check PASS does not assert fee-clamp refund conservation. The printed
520 exceeds the printed no_creation_max=505; this is a follow-up candidate
requiring independent source review and targeted reproduction. Peer MAIL.md's
broad no-extra-refund statement is not validated or fixed by O68. This record
does not establish a confirmed remote exploit or a security closure.
MAIL.md and security documentation are outside this specialist's write scope.

### Network and smoke limits

NetSmoke raw server log confirms listening on 17918 and Join succeeded once;
client log confirms 127.0.0.1:17918 and Welcomed by server once. The unchanged
runner observes the default 30-second server startup and 45-second client
window, then stops only its owned children. `ue-launches.json` records
server PID 6076/client PID 50772 with nativeExit=-1/-1 after owned cleanup.
Runner exit 0 is a connection/observation PASS, not clean native termination
or mailbox RPC/HUD/PIE proof. NetSmoke ended at 2026-10-11 09:18:06 UTC.
Its process-after.json, and those of the other runtime steps, are empty.

Summer/Winter smoke prove their narrow headless startup/command/error checks;
spawn counts are not full navigation, pocket quality or playability proof.
No PIE or main runtime was run.

## Accepted main Editor build and preservation

Evidence subdirectory:
`MainEditorBuild_1b045e94d3154f9f8376ddc3520fb9bc` under the evidence root.
Documentation read `FINAL_ACCEPTANCE.json`, `HANDOFF.md`, `result.json`,
`command.log` and before/after manifests. Final state:
`ACCEPTED_9_INTEGRATION_CHECKS_PLUS_MAIN_EDITOR_BUILD`.
The separate integration `SUMMARY.json` retains all nine PASS checks,
unchanged inputs/existing Saved and zero remaining UE processes.

Actual main command:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' MyProjectEditor Win64 Development '-Project=C:\Users\OZAN CIDIK\Desktop\ozan online\MyProject\MyProject.uproject' -WaitMutex
```

Main Editor build finished 2026-10-11 09:23:07 UTC: nativeExit=0, exit=0,
Result: Succeeded, freshBinary=true, runtimeLaunched=false. Fresh main DLL:
`B906C238D0E1044756704DAD9B4A0D7BD34D683EC7D40A2A8ED254AB5E548FA7`.
Documentation independently read the current main DLL hash and matched it.
Shared lock and idle preflights passed according to QA's handoff.

Before/after manifest JSON is byte-identical for 250 main input files
(Source/Config/Scripts/project), the entire 3277-entry main Saved inventory and
746 live records. QA records allInputsUnchanged/allSavedUnchanged/
live746Unchanged=true. Documentation compared those TEMP manifests, without
enumerating live Saved. Main and integration are separate checkouts:
FINAL_ACCEPTANCE records byteIdenticalAllInputs=false, 173 CRLF/LF-only
differences and seven ignored existing main Python caches, unexplained=[].
The four candidate files match byte-for-byte. Do not claim every raw main input
matches the isolated checkout or that this main build ran the nine runtime
checks again.

Parent's final audit at 2026-10-11 09:26:30 UTC:
`parent-final-unowned-live-audit.json`, HEAD=a1f6145, baselineCount=1043,
excluding Store/header/checklist owned paths, unownedChanged=[],
liveCount=746, liveChanged=[]. Documentation read the reported audit fields;
this is parent-produced preservation evidence. Parent also independently
matched the main DLL to QA's B906... hash. These records preserve peer/unowned
data while allowing owned integration changes; docs copy and commit remain
PENDING.

## Current-base commands and parent handoff gates

**Not executed by this documentation specialist.** All UE work belongs to
Avicenna under the common Git `agent-build.lock`, idle-process preflight and
owned-child cleanup. Do not stop unowned processes. Existing runners require
QA's external serialization harness. The new runner acquires its own lock;
do not nest that lock.

From the isolated worktree, target commands are listed below. Actual step
invocation.json and ue-launches.json retain exact host/argv, private TEMP,
evidence root and harness-added UserDir. Each current step passed above:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' MyProjectEditor Win64 Development '-Project=C:\Users\OZAN CIDIK\.codex\worktrees\mail-read-failure-integration\MyProject\MyProject.uproject' -WaitMutex
# Fixed target validation, then fixed native regression.
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' MyProject Win64 Development '-Project=C:\Users\OZAN CIDIK\.codex\worktrees\mail-read-failure-integration\MyProject\MyProject.uproject' -WaitMutex
& .\Scripts\TestMailReadFailure.ps1 -ExpectedChecks 24 -Label fixed-current -EvidenceRoot 'C:\Users\OZAN CIDIK\AppData\Local\Temp\mmo_mail_read_failure_20261011'
& .\Scripts\TestMail.ps1 -Label current104
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' .\Scripts\ValidateData.py
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' .\Scripts\StaticLint.py
& .\Scripts\SmokeTest.ps1 -Map Summer_scene
& .\Scripts\SmokeTest.ps1 -Map Winter_scene
& .\Scripts\NetSmokeTest.ps1
```

Current configured game map is
`/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene`, read from
`Config/DefaultEngine.ini`; the new runner resolves it from current config.

New runner uses an isolated registered worktree, unique nested UserDir, GUID
fixture, bounded owned-child watchdog and per-run evidence directory. Exit 0
requires exact coverage, native exit 0 and preservation of pre-existing worktree
Saved entries; exit 1 rejects/fails, 2 is watchdog, 3 is resource deferral.
It records invocation, process preflight, runtime source hashes, logs, Saved
before/after inventories, existing-entry changes and result.json. The bounded
nine-step results and separate main Editor build are verified above; parent
documentation copy and routine commit remain PENDING. Preserve evidence and distinguish new entries
from changed pre-existing files. Use synthetic data; no destructive live Saved
reproduction, ACL change, mailbox replacement or forced live-server termination.

Preparation artifacts read:
`agents.json`, `initial-main.json`,
`prep_c53dcd0b04ea493eb46a94d6272a5661/plan.txt`,
`ps51.json` and `pwsh.json` beneath that prep directory.
The parser fixtures report engineStarted=false and zero parser errors for an
earlier runner hash `CAD8420D...`; they are not native proof and do not validate
the later runner bytes. The initial historical main inventory contains 1041
paths. Parent reports the long-gap live-data comparison retained all original
733 files byte-for-byte (changed original hashes=0), with 13 new peer-interval
files, for 746 current files. This does not mean every worktree/main path was
unchanged. Parent froze a fresh 746-file live-data baseline and 1043
tracked/original-.agents paths against a1f6145 for integration. Final QA main
build and parent audit preservation results are recorded above. Documentation
read their TEMP evidence; it has not enumerated live Saved or repeated live
collection itself.

| Acceptance item | Current result |
| --- | --- |
| Bounded source safety/backend review | PASS; Singer final raw-evidence approval reported by parent, fixed24 independently checked, broader scope open |
| Historical baseline Editor build / 24 native assertions | Build PASS; accepted behavioral FAIL, 15 PASS / 9 FAIL, native exit 0 |
| Current-base fixed Editor and Game Development builds | PASS; nativeCommandExit=0 / Result: Succeeded, fresh binaries |
| Fixed 24 native assertions / raw I/O and preservation evidence | PASS 24/24, native=0/runner=0; denied reads=4, write/move/delete=0 |
| Current peer mail 104-check suite | PASS 104/104 in retry1; one expected GUID Move error, no unexpected errors; refund-note caveat above |
| ValidateData / StaticLint | PASS exit 0; raw ValidateData skill-gap warnings retained |
| Summer / Winter / network smoke | PASS bounded headless checks; NetSmoke port 17918, cleanup native exits -1/-1 |
| Candidate source/binary hash match | PASS across nine isolated steps; final QA acceptance confirms source4 byte match and fresh main DLL |
| Existing-data preservation | PASS isolated records; main build 250 inputs/entire Saved/746 live unchanged; parent final unowned audit unchanged excluding owned paths |
| Parent main integration of source4 | DONE reported by parent, compiled-candidate bytes identical; docs not yet copied |
| Main Editor build | PASS native=0, Result: Succeeded, fresh B906... DLL; no main runtime |
| Parent docs copy / routine commit | PENDING |
| Listen host / remote PIE and fault observation | PENDING; unchecked O68 checklist |
| Restart / settlement / NR1 / NR3 | OPEN, not solved |

## Open persistence and identity work

**NR1 remains OPEN**, with its description reassessed against peer afba349
and current a1f6145 source. The earlier O67 note is historical: its simple
stale-.tmp resurrection chain must not be presented as unchanged.

Peer WriteBox now attempts to delete .tmp after an initial temporary-write
failure. After final-move/direct-write failure with no destination, it attempts
cleanup and returns failure if cleanup succeeds; if complete .tmp survives,
it reports success to avoid a refund followed by recovery. Corrupt-box handling
attempts to move surviving .tmp to .tmp.bad and falls back to Delete. DeleteBox
also removes .tmp.bad. These peer cleanup paths block recovery when cleanup
succeeds. Peer SendMail also measures/clamps the actual deduction before refund;
O68 must retain that component behavior.

Residual conditions remain unproven: a failed temporary write could leave
recoverable complete contents when Delete also fails, yet return false; corrupt
handling could leave recoverable .tmp if both its quarantine and Delete fail.
Whether either full refund/recovery chain occurs under exact filesystem faults
has not been runtime-reproduced here. O68 closes neither NR1 nor general
transaction/crash durability. The current 104-check test source explicitly
excludes the last-resort .tmp-delete-failure branch, real disk-full/antivirus
faults and full settlement persistence; it uses read-only/directory fixtures
and native callback probes. Mail104 passed its asserted 104 checks; the
unasserted refund note above and these multiple-failure cases remain open.

**NR3 remains OPEN** for trusted identity and migration activation. O67 supplies
offline staging with explicit provenance; it does not activate mail or prove
recipient ownership. O68 adds no runtime fallback, identity migration or live
data activation. Stable identity, old/new conflicts and reconciliation with
character-save/refund history still require separate authorized work.

Documentation handoff: only this new task note and the appended O68 checklist
section are owned. All 569026 prior checklist bytes, including the peer's
17 K1F lines, match the current a1f6145 HEAD checkout prefix (Git LF blob
563557 bytes, core.autocrlf=true gives the original CRLF checkout bytes).
Preserved prefix SHA256:
`5F9BD7C28B7DDD149826EBD5BCC5151FE7557657EA70DC54FE74839398CC61B9`.

Parent copies only these owned documentation changes after review and performs
the already authorized local commit. QA final acceptance and main Editor build
are recorded above. Those parent actions are not
completed by this specialist. Leave all six O68 PIE items unchecked. No UE,
Saved writes, main edits, automation changes, stage or commit were performed by
documentation; old worktree documents remain frozen.
