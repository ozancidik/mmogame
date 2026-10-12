# O69: bounded mail charge/refund accounting

Date: 2026-10-11. Current candidate base: `f70e5ad65331f709a575f3edf16ca0b22c40a666`.
Worktree: `C:\Users\OZAN CIDIK\.codex\worktrees\mail-refund-current\MyProject`.
Evidence root: `C:\Users\OZAN CIDIK\AppData\Local\Temp\mmo_mail_refund_accounting_20261011`.
Status: **Verification/integration COMPLETE; PIE OPEN; documentation frozen for parent delivery**.

## Ownership and resume

Runtime type=`default`, explicit specialists; custom catalog NOT_RELOADED.
`Claude: NOT_RUN`. Read project AGENTS/workflow, relevant project MMO skills,
`agents.json`, `plan.json`, transfer plan, source, and baseline raw evidence.

| Specialist | Actual ID | Scope |
| --- | --- | --- |
| UE / Sagan | `01a12936-fba9-71b3-8e3b-cbd2d5237aee` | Three reconciled production paths; released after peer baseline |
| QA / sole UE+DevOps / Avicenna | `01a12937-670e-7f51-ac75-5325e6d9bde8` | Historical regression107; current133 preserves peer130+3; serialized UE owner |
| Network/security / Boyle | `01a12937-6996-74e1-b556-2b7c3927f375` | Independent read-only review |
| Backend / Singer | `01a128c4-4b9f-7ff0-a8d5-ad4c4185ffdf` | Independent read-only accounting/persistence review |
| Documentation | `01a12940-5ddd-7c21-be76-322fbbdfc93e` | This NEW note and appended O69 checklist only |

UE paths: `Source/MyProject/Private/Components/MMOMailComponent.cpp`,
`Source/MyProject/Private/Components/MMOInventoryComponent.cpp`,
`Source/MyProject/Public/Components/MMOInventoryComponent.h`.
QA: `Source/MyProject/Private/Tests/MMOMailRefundSelfTest.cpp`,
`Scripts/TestMailRefund.ps1`; legacy `MMOMailSelfTest.cpp`/`TestMail.ps1` separately assigned.
Docs run no UE and change no source/main/Saved/automation; no commit.
Doc ownership transferred to current tree; both previous trees are read-only.

During the usage gap peer commits `2bb2023`/`d031b90` changed MailComponent,
MailSelfTest/TestMail and checklist content. Preserve those peer changes.
Preliminary d031 peer-160 Editor build 0 was reported; runtime NOT_RUN on hold.
That preserved preparation is distinct from the accepted peer-176 run below.

## Intended correction and limits

Net-before/after accounting mixes synchronous callback spending/gifts with mail
charges. Intended fix records each direct clamped debit before its broadcast,
uses only those receipts for acceptance/refund, and validates owner/identity.
Backend source blocker reported by parent: completed same-name LoadCharacter can
restore Gold/increment LoadSerial; the earlier O69 guard omitted that generation.
Capture/recheck LoadSerial during charging, refund and post-refund save.
QA generation seam increments serial and calls real inventory ApplySaveData;
it does NOT execute actual LoadCharacter, disk reload or a remote exploit path.

Historical bounded acceptance oracles are recorded with the fixed raw results below.

Current legacy133 preserves committed peer130+3; no TestMail runner change planned.
New 176 = original 160 labels + two generation cases with eight checks each.
It includes fresh .bak namespace/cleanup checks. Later O68 narrow24 requires a
disposable empty isolated mail directory under lock, omits .bak coverage, and
cannot establish complete sidecar behavior. No live-data fault injection.

## Historical original baseline (a0cb86d; not current-peer proof)

Read parent-baseline-acceptance.json/qa_baseline_1ea30f0b2350489baa57aef884f7f2b9 raw.
Historical a0 build0, native0/runner1, 160 unique:127PASS/33FAIL,20observations.
Clamp520/505, short-fee mail, gift10684/11000 are historical; see O68 task for log-only triage.

## Historical d031 peer-176 baseline

Read `parent-peer-baseline-acceptance.json` (accepted 14:22:21Z), suite
`qa_peer_baseline_56524916b6cc49f1808ddc6729c707cc` Build/Run results,
invocations/freeze manifests and raw `Run/original_29339a248c874c9eb0e36314b7a3efcb/ue.log`.
Editor native command exit 0, `Result: Succeeded`, fresh DLL SHA256
`BE247CAF1239E6AE58D29B14623A58D08726CAA6F6BF9BA315DD68453881409F`.
Independently counted 176 unique exact-set labels: 148 PASS/28 FAIL,
22 observations, one FAIL summary; native 0/runner 1, normal shutdown/errors[].
Test SHA256 `B28BB961B95525B156138A55E50BD1AF654EA2B1D1BEB83C67A62D0444703DB3`;
runner `84C9FE2D0D554A587E24B4DD2E4DE99074A919136BA6B5CBF1A2B9EBA246997A`.

Raw clamp-savefail/saveok both final 505, result 10, saves0/mails0: peer fixed.
Gift-saveok wrongly refuses: final11000 vs10684, result10/saves0/mails0.
Gift-savefail wallet11000 already matches, but saves0; no failed-save execution.
Generation attachment seam final1500/expected1000, fee seam1520/1000;
both result10/saves0/mails0. These are seam-only stale-refund observations.
Build/Run saved-existing-changes and main-saved-existing-changes are empty;
results record inputs/Saved preserved. Parent acceptance records original746
and current763 main live files preserved; no new claim about all worktree changes.


## Historical d031 fixed acceptance

Read qa_fixed_96d88750fa4a4364b8cce53380e1b52e/RECOVERY.json and all listed result/raw
artifacts; every raw hash matched. This suite verifies d031 only.
Editor native0/Result Succeeded, DLL69C54...; wrapper1 postflight unowned PID15076
retained and native compilation accepted. Game native0/wrapper0, EXE4A32... .
Refund176/Mail107/O68 retry24/Gold60/Trade230/Purchase174/Upgrade132 PASS;
ValidateData/StaticLint/Summer/Winter0. NetSmoke runner0, intentional native-1/-1,
port17916 connection/welcome1/errors0; no clean-exit claim. Recorded data preserved.
Original O68 TEMP FullName prelaunch failure (native PID/exit null) retained;
projection retry passed only with exclusive empty fixture-parent; no full sidecar proof.
Source freeze/legacy107 hashes and raw provenance remain in historical manifests.

## Current f70 source and accepted full-suite results

Current f70 prod3+legacy133 source: parent/Singer/Boyle APPROVE per parent.
Read current-peer-five-file-port.json and qa_current_compat_8698f86d302b453f8d9c258c47e83206
readiness/frozen7.json; all seven current hashes independently match.
Legacy133 native `5C893696113077EF5D7DC2F224309D16E29B998495B995A444B5295DA280147C`;
unchanged runner `12A3DD8E8DA10AC98FEF8D31C4596DEAE4B89834356CEEC553BF7E632B3C92CA`.
Current suite: qa_current_fixed_9e6edbecce7f4309bdf7648e9019a0b3; read result/raw files
and parent-current-f70-176-133-raw-acceptance.json. Editor/Game native0/wrapper0,
Result Succeeded; fresh DLL/EXE SHA256 respectively:
`C776D862E2AA591E956F6DACE39E1582370055ED1BC89EDC655C63595622236C`;
`37D59E7664D7217740855522F2AFBE77C96FFE589CCF2787FCF5DDDEC5CEE045`.
Refund raw: Refund176/fixed_d81ed7b75bf24ac4a1038b6fc738d05d/ue.log;
176 unique PASS/22 observations, baseline-identical labels; native/runner/wrapper0.
SHA256 `859266A106E084687989EA495039EF9536CB42D6D37CBED0573F2334905A3179`.
Mail raw: Mail133/Temp/mmo_mail_fixed133_324cebd5300841858419e692d070efda.log;
133 unique PASS, exact expected set per wrapper; native/runner/wrapper0, Move-errors0.
SHA256 `102B7A761EB53EBC2947D6C672E0FFFCB80C818F01D7EA12901AC464BB3263B6`.
Current clamp505/save0/mail0; gift10684/save1/mail1; gift-savefail11000/save1/mail0;
both serial seams1000/save0/mail0. Singer/Boyle independent176 acceptance per parent.
Conditional Fixed24 raw24PASS/IO4/0/0/0; Gold60/Purchase174 raw60/174PASS;
Trade230 PASS summary (no per-case count claimed); native/runner/wrapper0 throughout.
Inputs before/after match; recorded main787/live and5123/Saved unchanged. Mail133 raw
manifests also preserve original746/prior763 and all28 existing isolated entries (26new).
Read SUMMARY.json/REPORT.md and parent-current-f70-full-suite-acceptance.json;
summary SHA256 `872C34999390601DB216C8A7EF365CD134529DA686C6AF2E651E9A9F825BD808`.
All14 steps accepted, all29 listed raw hashes matched; Upgrade132/data/lint/Summer/Winter0.
NetSmoke runner/wrapper0, intentional native-1/-1; port17765 connection/welcome1/errors0.
Recorded input/Saved/live inventories preserved throughout; frozen7 unchanged, no owned UE pending.
Read parent-main-code-integration.json: guarded6 done19:41:39Z; legacy144 SHA256 `173B3744E90EFD11711607BA69F2608DD6A51620C97C6C90C5891811EEA69BF7`.
Read qa_main_fixed_cfc5d77ad4e2402384df081880cb0a06 MAIN_ACCEPTANCE.json/REPORT.md and parent-main-final-acceptance.json; peer11/eight peer files+runner141 preserved.
Main Editor native/wrapper0, Result Succeeded; fresh DLL SHA256 `08BF52886C8A4EFB9709D513BC74B943F8A2AD99F2A8A433FDA0BC5627410393`.
Main Mail144 raw144 unique PASS/exact expected set, native/runner/wrapper0, errors0/normal shutdown; gift10684/clamp505.
Raw Mail144 SHA256 `606379A60F962298A35B4B3BE83F26DA3B66816D0E4EF5FCEBB4ABB5BB9864A8`; all listed raw hashes independently matched.
Main Refund176 POLICY_NOT_RUN (evidence NOT_RUN_POLICY): runner/wrapper1, native N/A, no UE launch; isolation policy honored, no bypass/retry or gameplay failure.
Isolated current176 PASS above remains separate; main246 inputs/live787/original746/prior763 and all5123 existing Saved preserved (26 new artifacts).
Strong references/no forcedGC are not UAF proof; SaveOwner mock/append capacity are not disk-fault proof.

| Check | Status |
| --- | --- |
| Historical a0 original160 baseline | Accepted historical behavioral failure |
| Historical d031 peer176 Editor/native baseline | Accepted build0, native0/runner1, 148 PASS/28 FAIL |
| Historical d031 Editor command / wrapper | Native0/Succeeded; wrapper1 postflight peer-busy DEFER |
| Historical d031176 + legacy107 | PASS bounded evidence; all exits0 |
| Current f70 Editor/Game,176/133,24/60/230/174 | PASS bounded actual results; all exits0 |
| Current full14 / main6 integration / main QA | COMPLETE; main Editor+144 PASS; main176 POLICY_NOT_RUN, isolated176 PASS |
| Listen-host/remote PIE | PENDING; five items unchecked |

NR1/NR3, refund-savefail, crash/restart settlement and full refund durability
remain OPEN; no confirmed remote exploit or actual reload proof.
Current checklist prefix: 577185 CRLF bytes, SHA256
`8CBA50CC98D0F9D240D63C05A2C2FA0CEB4A4054D3998E1CE7B044F575ABA230`.
Parent normalized-prefix PASS: current577185/8CBA... equals main571628LF/3C334...; preserve peer bytes.
Transferred only O69 after historical573794 bytes, never whole old checklist.
Parent later fresh-guards main and transplants only the suffix after577185, normalizing LF only.
Frozen doc payload: parent owns Git/TEMP delivery and commit-status record; no commit claimed here.
