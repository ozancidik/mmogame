# O67: offline mailbox compatibility export preparation

Date: 2026-10-11. Base: `941860d36383241cda0ce7240a140f7af54e4b5c`.
Worktree: `C:\Users\OZAN CIDIK\.codex\worktrees\mail-legacy-compatibility\MyProject`.
Hume implemented `Scripts/ExportMigratedMailboxes.ps1` and froze its bytes;
Hypatia owns `Scripts/TestMailMigrationExport.ps1`. Hume released documentation
ownership; the documentation specialist owns only this document. Backend and
security review the exporter read-only. Parent owns integration and the routine
authorized local commit; parent integration/commit was PENDING at specialist handoff. Specialists have
not committed. Gameplay, live player data, other scripts/docs, Content, UE
execution, push, reset and deployment are outside this delivery.

Actual dispatched specialists use `runtime type=default` with explicit specialist
role instructions; this is not evidence of native custom-role loading.
`Claude: NOT_RUN`.

| Specialist | Actual agent ID | Responsibility |
| --- | --- | --- |
| Hume | `01a128c6-d825-7b03-b571-2ce0a1eea607` | DevOps implementation and author smoke checks |
| Hypatia | `01a128c7-2f4c-75e3-b368-a48e7ff31df5` | Independent QA and fixture runner |
| Dirac | `01a128c4-4caf-7263-9d98-daff41651245` | Independent security source review |
| Singer | `01a128c4-4b9f-7ff0-a8d5-ad4c4185ffdf` | Independent backend source review |
| Documentation specialist | `01a12926-4bcb-7ff2-8dda-be71170d87e2` | Evidence review and this document only |

## Why runtime fallback is unsafe

The historical mailbox filename changed from `<sanitizedName>.json` (initially
preserving ASCII case, subsequently lowercase) to
`MMOMail_<lowercaseSanitizedName>.json`. On Windows, legacy owner `MMOMail_Ali`
and canonical owner `Ali` can refer to the same case-insensitive filename.
Filenames and mailbox JSON do not establish the recipient's identity. An
automatic fallback could deliver another owner's mail or duplicate value.

The exporter requires explicit operator provenance. It never infers ownership,
merges two mailboxes, chooses a newer file, rewrites JSON, or activates data.
Explicit mappings can safely separate a legacy `MMOMail_Ali.json` belonging to
`MMOMail_Ali` (output `MMOMail_mmomail_ali.json`) and an `Ali` mailbox in a
separately reconciled snapshot. A single Windows snapshot cannot contain both
historical meanings of one physical filename. If provenance is ambiguous,
resolve it externally or stop; do not guess. Even identical bytes do not prove
that two mailboxes belong to one owner.

## Input contract

PowerShell 5.1 or 7 on Windows; local absolute drive paths only. Supply:

- `-SourceDirectory`: an external, already frozen snapshot directory containing
  only top-level mailbox JSON files. No subdirectories, hidden sidecars, unknown
  files, alternate data streams, reparse points, symlinks, or junctions.
- `-ProvenanceManifest`: external UTF-8 JSON file, outside the source/staging,
  with exactly the following schema. Every source file must be mapped once.
- `-StagingDirectory`: a new, absent directory whose parent already exists.
- `-AcknowledgeFrozenSnapshot`: explicit assertion that the source was frozen
  externally, with no live writer or operator changing it during export. A false
  switch value is rejected. This assertion is required for plan and apply.
- `-Apply`: opt in to creating staging. Omitting it validates and emits a plan
  without creating any file or directory. `-Apply -WhatIf` also validates fully
  and creates nothing. Standard `-Confirm` is supported.

All three paths must be external to the checkout and must have no `Saved`
path component. This deliberately excludes even an offline directory named
`Saved`; put the snapshot in a distinctly named external directory. Source and
staging cannot contain each other. Manifest cannot be inside either. Existing
reparse ancestors, UNC/device paths, drive roots, traversal, empty path
components, alternate stream syntax, reserved DOS names, trailing dots/spaces,
Windows-invalid characters, wildcard brackets, and tildes (including DOS short
path aliases) are rejected before normalization. Do not use a
live project Saved directory, even with the acknowledgment. This tool has no
live-data discovery or automatic snapshot-copy step.

Existing source, staging parent, provenance file and checkout are resolved using
Windows file/directory handles and `GetFinalPathNameByHandleW` with normalized
volume GUID paths before containment and Saved/checkout exclusion checks. The
absent staging leaf is appended to its physically resolved existing parent.
This detects static SUBST aliases and drive spelling aliases even when lexical
paths appear disjoint. Unresolvable physical identities fail closed. The native
declarations use in-memory Reflection.Emit on both hosts, with no compiler
temporary files. Physical identities and boundaries are checked again before
staging creation. Tilde rejection remains a conservative input restriction;
physical containment does not depend on accepting DOS short path aliases.

Example manifest (replace hashes with independently recorded SHA256 values):

```json
{
  "version": 1,
  "files": [
    {"filename":"Ali.json","format":"legacy","owner":"Ali","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
    {"filename":"MMOMail_veli.json","format":"canonical","owner":"Veli","sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}
  ]
}
```

Keys and formats are case-sensitive, with no additional fields. `format` is
exactly `legacy` or `canonical`. SHA256 accepts 64 hexadecimal characters and
is normalized to lowercase in the receipt. Manifest version is integer `1`.

Owners are conservatively limited to ASCII letters, digits, spaces, underscores,
and hyphens, 1..32 UTF-16 units, with no outer spaces. Space maps to underscore;
ASCII lowercasing is invariant. Unsupported characters are rejected, never
removed or truncated. `Default` and all case variants and reserved DOS owner
names are rejected as a deliberately narrower supported subset. Runtime
`FChar` classification and Unicode lowercasing are not emulated with .NET
Unicode classification. Non-ASCII owners/basenames require external review;
Unicode sender/message text is supported. This is a partial compatibility scope,
not proof of full sanitizer parity for every historical player name.

The exact legacy filename must equal either `<spaceMappedOwner>.json` or its
ASCII lowercase variant. Canonical filenames must exactly equal
`MMOMail_<lowercaseSpaceMappedOwner>.json`. Mixed/guessed case variants are not
accepted. Two entries for one canonical owner, `Ali`/`ali`, `Ali Veli`/`Ali_Veli`,
both old/new paths, duplicate filenames, or output collisions fail, even when
hashes match. Sidecars `.tmp`/`.bad` are always rejected, including when listed
in the manifest: they need separate operator reconciliation.

Provenance is trusted operator input. The script verifies its internal
consistency and byte hashes, not its truth, authenticity, or recipient identity.
It cannot prove ownership from JSON, determine whether gold was already paid,
reconcile character saves, or decide which crash artifact is authoritative.

## Lossless supported mailbox schema

UTF-8 with an optional UTF-8 BOM; invalid encoding, UTF-16/ANSI, malformed JSON,
duplicate decoded keys, trailing data, missing/unknown/case-mismatched fields,
and unsupported versions fail. The parser is recursive descent with bounds:
1 MiB per mailbox, 4 MiB manifest, depth 8, 50,000 nodes per JSON document,
at most 10,000 files (the node bound may reject a smaller manifest).

Mailbox root has exactly `version` (integer `1`) and `mails` (array, 0..20
records). Each record has exactly these typed fields:

| Field | Accepted value |
| --- | --- |
| `from` | String, 1..32 UTF-16 units, valid surrogate pairs, no NUL |
| `text` | String, 0..200 UTF-16 units, valid surrogate pairs, no NUL |
| `gold` | Nonnegative integer token, 0..1,000,000 |
| `sent` | Nonnegative integer token, 0..9,007,199,254,740,991 (`2^53-1`) |

An empty message requires positive gold. Numeric strings, booleans, null,
fractions, negative values (including `-0`), decimal/exponent notation, missing
fields, empty sender, overlong strings, a dropped record, or a 21st record fail.
This strict integer-token subset avoids coercion. The timestamp bound is
narrower than int64: UE reads JSON numbers through double before int64, so
larger integers cannot generally be preserved exactly. Historical files outside
these bounds require separate review rather than silent repair. Text lengths
match Windows UE `FString` UTF-16 length, including two units per supplementary
character. Message strings are decoded only to validate; output is the exact
original byte sequence, including whitespace, escapes, key order, and BOM.

## Exact usage

Create and freeze the snapshot and provenance using an external operational
procedure first. These example directories are outside every live Saved tree;
`C:\MailboxExport` must already exist. Each staging name must be absent.

```powershell
# Validation and plan only; no output writes.
& .\Scripts\ExportMigratedMailboxes.ps1 `
  -SourceDirectory 'C:\MailboxExport\snapshot-20261011' `
  -ProvenanceManifest 'C:\MailboxExport\provenance-20261011.json' `
  -StagingDirectory 'C:\MailboxExport\stage-20261011-a' `
  -AcknowledgeFrozenSnapshot

# Full validation plus standard ShouldProcess preview; still no writes.
& .\Scripts\ExportMigratedMailboxes.ps1 `
  -SourceDirectory 'C:\MailboxExport\snapshot-20261011' `
  -ProvenanceManifest 'C:\MailboxExport\provenance-20261011.json' `
  -StagingDirectory 'C:\MailboxExport\stage-20261011-a' `
  -AcknowledgeFrozenSnapshot -Apply -WhatIf

# Explicit staging creation only. No activation command exists.
& .\Scripts\ExportMigratedMailboxes.ps1 `
  -SourceDirectory 'C:\MailboxExport\snapshot-20261011' `
  -ProvenanceManifest 'C:\MailboxExport\provenance-20261011.json' `
  -StagingDirectory 'C:\MailboxExport\stage-20261011-a' `
  -AcknowledgeFrozenSnapshot -Apply
```

Read-only handles on source files and provenance are retained throughout the
operation (`FileShare.Read`, denying write/delete on normal Windows files).
The script validates every file and the full inventory before any staging write.
It checks source hashes against provenance, again before/after copies, and
checks destination hashes immediately and across the full export before issuing
a completion receipt. Exact byte preservation preserves mail count and gold
totals. Runtime process-name inventory is diagnostic: it warns about known
UnrealEditor/MyProject processes, never kills anything, and cannot prove that
renamed, remote, or external writers are absent. Process inventory failure
terminates the operation. This is not a filesystem security boundary: external
freeze, protected parent directories, and exclusive operator control must keep
paths/inventory stable and prevent hostile path swaps between checks. Static
drive aliases are resolved before validation; concurrent remapping, directory
replacement or reparse insertion after a check remains an operational TOCTOU
limit. The tool does not reserve directory namespaces against hostile actors.

## Receipts, failure and rerun behavior

Plan returns `Status=ValidatedPlan` and an in-memory receipt with `complete=false`.
`-Apply -WhatIf` returns `ValidatedNoWrite`. Successful apply returns
`ExportedStagingOnly` and writes:

- Exact mailbox copies at canonical filenames.
- `EXPORT_INCOMPLETE.json`: deterministic initial intent/plan receipt, retained
  for audit even after success, always `complete=false` and `activatable=false`.
- `EXPORT_COMPLETE.json`: deterministic final receipt, `complete=true` and
  `activatable=false`. Contains provenance SHA256, ordinal output order,
  per-file source/output SHA256, byte lengths, owners/formats, mail counts and
  gold totals, plus whole-export mail/gold totals. No timestamp or machine path.
  It is written and hash-verified as `EXPORT_COMPLETE.json.tmp`, then renamed
  without overwrite only after all copies pass verification.

The final receipt supersedes the initial plan marker for *export completion
only*. Compact JSON, LF termination, UTF-8 without BOM, ordered properties and
ordinal filename sorting avoid host-dependent pretty printing. Byte-identical
inputs and provenance give byte-identical receipts across
equivalent supported PowerShell runs. A receipt is not a signature, authorization,
activation token, or guarantee against subsequent staging tampering. An existing
directory is always rejected, even for a validation-only rerun and even if it
contains a successful export. No resume, cleanup, deletion, overwrite, or reuse
path exists. Choose another new absent staging path after separate inspection.

Before staging creation, validation failure leaves no output. After creation,
failure leaves the directory, initial marker if already written, copied bytes,
and any unfinished receipt for inspection. A failure during initial marker
creation may leave only an empty/partial directory. It emits a terminating error
and warning; there is no final completion receipt. Never activate partial output.
The script never mutates, renames, quarantines or removes a source file. Snapshot
and provenance hashes must still be independently checked at handoff.

## Open runtime and operational follow-ups

**NR3 remains OPEN in runtime/activation scope.** This tooling prepares staging
copies from a trusted, externally frozen snapshot. Actual activation requires
external trusted ownership provenance, pausing live mailbox writers, reviewing
all sidecars and old/new collisions, and reconciling mailbox gold against
character-save/refund history. An authorized operator must independently verify
and reconcile staged contents before any separate activation procedure. Do not
copy receipt files or an entire staging directory into a live mailbox directory.
There is no automatic runtime fallback in this delivery. Parent's read-only
inventory observed the actual live `Saved/MMOMail` directory empty; no live
player mailbox was migrated and that observation is not an ownership proof.

**NR1 remains OPEN as a separate conditional, source-only follow-up.** Dirac's independent
recheck found the old specific no-destination/temp/refund sequence resolved in
`43e87ca`. A different conditional chain remains: `WriteBox` fallback fails with
a partial invalid destination; deleting the complete `.tmp` also fails and its
failure is ignored; the function returns false and the sender refund is persisted;
`Load` quarantines the invalid destination; a second `Load` then recovers that
refunded `.tmp`. An initial `SaveStringToFile` failure could also leave a complete
temporary file. These paths have not been reproduced. Exact filesystem fault
injection and any gameplay fix are deferred to a separately scoped task.
No claim of conservation across every crash/failure sequence is made here.

## Delivery evidence and acceptance

Hume's implementation self-review covers strict schema/duplicate handling, historical
filename formulas, collision rejection, path boundaries, source handles,
CreateNew files, deterministic receipts, and failure retention. Author-reported
smoke checks on 2026-10-11: PowerShell 7 and Windows PowerShell 5.1 AST parse passed
(exit 0). A retained external temporary fixture passed plan/no-write,
`-Apply -WhatIf`/no-write, successful apply, exact source/destination SHA256,
existing-output rejection, Windows PowerShell 5.1 apply, and byte-identical
completion receipts across both hosts (exit 0). Fixture evidence:
`C:\Users\OZAN CIDIK\AppData\Local\Temp\mail_export_devops_b470551544f548aca37aa9adae4e3e73`.
This is an implementer smoke check, not independent QA acceptance or exhaustive
fault injection. After the physical-path fix, both hosts passed source-alias and
staging-alias SUBST overlap rejection (four attempts, unchanged source inventory
and hashes), and SUBST aliases hiding Saved/checkout staging parents were rejected
before output (four attempts). Post-fix apply passed in both hosts with identical
mailbox, initial intent and completion receipt bytes, and preserved source SHA256.
Only newly allocated test drive mappings were removed; fixture files were
retained. An in-memory parser check also rejected malformed/duplicate-key,
coerced/clamped/overflow, filtered-record, bad-surrogate/NUL and excess-record
inputs, while accepting a valid Unicode message and timestamp zero.
These historical author checks remain attributed to Hume; independent acceptance
is recorded separately below.

### Frozen artifacts and source acceptance

The exporter frozen for final acceptance has SHA256
`DE24E9211D4B387D59909D60D702839263D2415884F203B0E911544124A0127A`.
The independent QA runner has SHA256
`7745CA312E54EAF8840A0F4016D4A858B26557792B196C1CD916292900AB48BF`.
Both final suite result files record these exact hashes, and documentation review
confirmed the current files match them. The exporter hash identifies frozen
product bytes, not a Git commit or the mutable documentation.

Parent reports Singer's final backend source acceptance PASS and Dirac's final
security source acceptance PASS for the frozen exporter. The static SUBST P2
finding is CLOSED. These are source-only reviews, not executed fixture proof or
closure of NR1/NR3. Concurrent path remapping/replacement remains subject to the
documented external freeze and exclusive operator control.

### Independent QA final acceptance: PASS

Hypatia's final independent acceptance is PASS. Documentation review read the
actual final result files; each contains 103 cases with 101 PASS, 0 FAIL and
2 SKIP. Final raw runner AST parses on both hosts returned exit 0, as recorded
in Hypatia's command history. Both full suite launches also returned exit 0.

| Host | Version | PASS | FAIL | SKIP | Suite elapsed seconds |
| --- | --- | --- | --- | --- | --- |
| PowerShell | `7.6.5` | 101 | 0 | 2 | `52.3507272` |
| Windows PowerShell | `5.1.26100.9549` | 101 | 0 | 2 | `43.5229645` |

Exact retained result paths:

- `C:\Users\OZAN CIDIK\AppData\Local\Temp\mmo_mail_export_qa_7.6.5_4faccc615ed5427ba482d20cc1e53632\results.json`
- `C:\Users\OZAN CIDIK\AppData\Local\Temp\mmo_mail_export_qa_5.1.26100.9549_899612944cdd43fd801931df2e750468\results.json`

The recorded host executables are
`C:\Users\OZAN CIDIK\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe`
and `C:\WINDOWS\System32\WindowsPowerShell\v1.0\powershell.exe`.
The following are the successful suite launch statements from Hypatia's command
history, rendered as PowerShell rather than tool/JSON shell escaping. Both ran
from the worktree recorded above, under the recorded PowerShell 7 executable.
The runner used its defaults: `CaseTimeoutSeconds=10`, `SuiteTimeoutSeconds=600`,
and the sibling exporter path. Documentation did not rerun these commands.

```powershell
# Final PowerShell 7 suite; exit 0.
if ((Get-FileHash Scripts/ExportMigratedMailboxes.ps1).Hash -cne 'DE24E9211D4B387D59909D60D702839263D2415884F203B0E911544124A0127A') { throw 'Exporter freeze mismatch' }
if ((Get-FileHash Scripts/TestMailMigrationExport.ps1).Hash -cne '7745CA312E54EAF8840A0F4016D4A858B26557792B196C1CD916292900AB48BF') { throw 'Runner freeze mismatch' }
& (Get-Command pwsh).Source -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File Scripts/TestMailMigrationExport.ps1
exit $LASTEXITCODE

# Final Windows PowerShell 5.1 suite; separate invocation, exit 0.
if ((Get-FileHash -LiteralPath Scripts/ExportMigratedMailboxes.ps1).Hash -cne 'DE24E9211D4B387D59909D60D702839263D2415884F203B0E911544124A0127A') { throw 'Exporter freeze mismatch' }
(Get-FileHash Scripts/TestMailMigrationExport.ps1).Hash
& "$env:SystemRoot/System32/WindowsPowerShell/v1.0/powershell.exe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File Scripts/TestMailMigrationExport.ps1
exit $LASTEXITCODE
```

Each suite retained 195 source-inventory checks and preserved all 1,032 peer
paths in its inventory. Its `peer-before.txt` and `peer-after.txt` have identical
SHA256 within that run; invocation logs retain before/after source inventories
and stdout/stderr. Each invocation also checks provenance preservation.
`all_peer_files_preserved` passed in both final suites. These inventories cover
the runner's enumerated worktree peer paths, not every file on the machine.

Both suites passed `subst_alias_overlap`, `subst_alias_Saved`,
`subst_alias_checkout`, `reparse_source-junction`,
`reparse_output-parent-junction`, and
`interrupted_export_no_complete_marker_and_no_reuse`.
Both skipped only `reparse_source-file-symlink` and `reparse_manifest-symlink`:
the host could not create those links because administrator privilege was
required. Those two scenarios have no executed rejection proof from these runs.

Most cases invoke the exporter in a separate script scope within the same host
process. Their 10-second elapsed threshold is checked after the invocation
returns; it is not a preemptive timeout. The suite threshold is checked before
invocations. Interruption and missing-mandatory-parameter cases use runner-owned
child processes with bounded waits; interruption kills only its owned exporter
child after observing the incomplete marker. This proves that fixture's partial
staging behavior, not runtime crash/refund conservation or live activation.

Earlier failed fixture attempts, watchdog behavior and Windows PowerShell 5.1
parameter/spacing issues were corrected runner defects; their logs remain
retained. They are not classified as exporter product failures. Final acceptance
uses only the frozen runner and exporter identified above.

### Independent comparison using identical bytes across hosts: PASS

The original independently generated fixtures differed in provenance JSON bytes
across hosts, so comparing those receipts alone would not establish determinism
for identical inputs. Hypatia instead reused the Windows PowerShell 5.1
`deterministic-a` source and manifest unchanged for a PowerShell 7 export to a
new `crosshost-ps7` staging directory. The comparison records exporter exit 0,
identical input/provenance bytes, preserved source and peer inventories, and
provenance SHA256
`D38BA74088845229852FB170DBA5B9908DD26B66FB852711A212C0218328C8BC`.

Exact comparison evidence:
`C:\Users\OZAN CIDIK\AppData\Local\Temp\mmo_mail_export_qa_5.1.26100.9549_899612944cdd43fd801931df2e750468\deterministic-a\crosshost-receipt-comparison.json`.
Documentation review read this file. The compared completion receipts are
`deterministic-a\staging\EXPORT_COMPLETE.json` (5.1) and
`deterministic-a\crosshost-ps7\EXPORT_COMPLETE.json` (7) beneath that retained
fixture root. Both are 536 bytes with SHA256
`B02E46690FE0C3CBA6D90D562E9BBE4C50693113163946E20A214A1D67368594`;
the comparison records `RawCompletionBytesEqual=true`.

The exporter launch used for this comparison, from the same worktree and
PowerShell 7 host, was:

```powershell
$qaCase = 'C:\Users\OZAN CIDIK\AppData\Local\Temp\mmo_mail_export_qa_5.1.26100.9549_899612944cdd43fd801931df2e750468\deterministic-a'
$qaSource = Join-Path $qaCase 'snapshot'
$qaManifest = Join-Path $qaCase 'provenance.json'
$qaCrossStage = Join-Path $qaCase 'crosshost-ps7'
& (Get-Command pwsh).Source -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File Scripts/ExportMigratedMailboxes.ps1 -SourceDirectory $qaSource -ProvenanceManifest $qaManifest -StagingDirectory $qaCrossStage -AcknowledgeFrozenSnapshot -Apply *> (Join-Path $qaCase 'crosshost-ps7.log')
```

UE build/PIE, live-data migration, live process stopping, runtime crash/refund
fault injection, push, deployment and activation: NOT RUN. No UE build is needed
for these scripts/documentation. Worker commit: NOT RUN. Parent integration and
routine authorized local commit: PENDING at specialist handoff. The final parent
commit/result will be recorded in parent external proof and Git history. This document records evidence from
the specialists; the documentation specialist ran no exporter or QA tests.
