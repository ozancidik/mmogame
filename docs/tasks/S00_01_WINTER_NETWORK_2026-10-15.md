# S00.01 / S00.02 Winter network adapter — 2026-10-15

**S00.02 runnable adapter delivered for independent review. Actual Winter network: NOT_RUN.**
The original S00.01 preparation was extended in place, not duplicated. The wrapper
can now invoke the isolated child after its gates pass. This worker ran only script
fixtures and read-only provenance helpers; no UE/build/DLL copy or gameplay test.
Franklin's final review and the parent's sole-owner Winter runtime remain outstanding.

## Ownership and baseline

- Worker: QA-automation/DevOps S00.01/S00.02; no descendant agents dispatched.
- Worktree: `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject`.
- Base/HEAD: `4e0f0c5a5ede948b69c324953f96b88a3ec61dc9` (detached).
- Parent reports main moved to `52d947e`; isolated worktree base stays unchanged.
- Exact five-file write scope: `Scripts/NetSmokeTest.ps1`,
  `Scripts/TestNetSmokeRunner.ps1`, `Scripts/InvokeWinterNetSmoke.ps1`,
  `Scripts/TestWinterNetSmokeWrapper.ps1`, this task record. The first two are
  modified tracked files; the last three are new relative to the worktree base.
- DevOps Wegener owns `Scripts/build.ps1` and `Scripts/TestBuildScript.ps1` separately.
  Those files, module-preparation outputs/task record, and main checkout edits
  (including Race) are outside this worker's write scope. No project stage/commit/push.
- Read root `AGENTS.md`, `docs/AGENT_WORKFLOW.md`, existing `NetSmokeTest.ps1`,
  `TestNetSmokeRunner.ps1`, `TestMailRefund.ps1`, build/checker lock patterns,
  and applied `ecc-mmo-verify`. Parent supplies independent QA/network review later.

## Current runtime contract

`InvokeWinterNetSmoke.ps1` accepts `-BuildReceipt` and optional `-PreflightOnly`;
derives the project from its own location; invokes the fixed local child with
`-Map Winter_scene`, a new isolated UserDir, and a fresh explicit log directory.
It preserves the child's 30-second server / 45-second client observations. Default
execution now reaches the reviewed child; `-PreflightOnly` retains the preparation
path with `NOT_RUN`. There is one synchronous child script invocation, no new
PowerShell process, outer kill watchdog, nested common lock, or process-tree kill.

It rejects main, copied/unregistered Git metadata, root mismatches, non-local paths,
alternate data streams, and reparse paths before using them. Git's common directory
holds the same persistent `agent-build.lock`, opened with `FileShare.None`. The
handle is disposed in `finally`; the lock file is never removed. Under that lock,
CIM enumeration must succeed; Editor/Cmd, LiveCoding, UBT, AutomationTool, MSBuild,
UAT, UE-associated dotnet, and dotnet with unknown command line defer the run.
Busy processes are left untouched. Only the child's two returned native process
objects may be stopped, never processes found by name or PID.

Proof directories are unique GUID directories below local, non-reparse `TEMP`,
outside registered worktrees. Existing isolated worktree Saved is inventoried and
its files held with read-only `FileShare.Read` handles through child completion and
the final audit. The child creates only a fresh `Saved/WinterNet_<GUID>` UserDir and
passes quoted `-UserDir` to both native launches. Existing files cannot be overwritten
through these handles; all additions/deletions outside the owned UserDir also fail
the final inventory comparison. This is an audit/protection contract, not an OS
sandbox: a new forbidden directory/file is retained as failure evidence, not erased.
No Saved backup/copy/delete or fixture game-project staging is implemented.

Child `UserDir`/`LogDirectory` must be supplied together. UserDir must be new,
directly under a registered isolated worktree's Saved, with the GUID name above.
LogDirectory must be new, local, non-reparse, and outside all registered worktrees.
Legacy calls without these parameters retain Summer/default TEMP logs and the
existing observation/port interface used by RunAllChecks. Real isolated execution
uses the wrapper; the child intentionally does not acquire the common lock itself.

Both absolute log paths and `processes.json` are published before native launch.
The manifest records map/port/UserDir, role/PID/log for each returned handle,
completed observation seconds, timeout, cleanup result/error, and script exit.
An unlaunched client has null PID and a reserved log path, not an invented log.
The same child that owns the handles checks its stopwatch every 250 ms and before
launch/readiness transitions. Default observation watchdog is 95 seconds; each
owned cleanup wait is capped at 5 seconds and client cleanup failure does not skip
server cleanup. Deadline expiry returns 2; early exit (including exit 0), bad
port/map/logs, incomplete observation, or cleanup failure cannot return success.
The watchdog is cooperative: synchronous OS startup and filesystem calls are not
forcibly interrupted by killing a parent shell. No unowned process is cleaned up.

Required Winter map and the three ThirdPerson Blueprint package headers are checked;
missing/LFS-pointer packages are rejected. Header checks do not establish package
dependencies, cookability, or gameplay validity. Provenance checks require current
HEAD/worktree, exact current Source/Config/Plugins/project tracked and nonignored
untracked input hashes, matching DLL/build-log hashes, successful local Editor target,
and DLL timestamp within the build interval and newer than every input. A receipt is
local coordinator attestation, not cryptographic proof that UBT produced the DLL.
Never manufacture a receipt for a copied main binary or infer provenance from mtime alone.
Source/HEAD/DLL provenance and child hash are checked again after invocation.
Timestamp conversion accepts JSON strings using invariant round-trip parsing and
PS7-converted DateTime/DateTimeOffset values using UTC conversion.

Wrapper exits: `0` accepted connection/observation/cleanup plus preservation and
provenance evidence; `1` rejected prerequisite or failed evidence; `2` child watchdog;
`3` busy/preflight only. Status remains `NOT_RUN` before invocation, becomes `FAILED`
on an attempted failure, and `PASSED` only after all evidence passes. Wrapper checks
native script exit, exact two-row owned-process manifest, complete 30/45-second
observations, chosen port, Winter readiness, join/welcome, and error-free logs.
Proof retains `result.json`, process preflight, Saved before/after, child hash/
parameters, `child-output.log` even on exception, and `child/server.log`,
`child/client.log`, `child/processes.json` when reached. Both reserved log paths
remain in the result on timeout/failure. Saved handles and common lock release in
finally. Connection smoke does not establish gameplay RPC correctness or clean
native engine shutdown; owned processes are intentionally terminated after observation.

## Actual verification (script evidence only)

Executed in the isolated worktree:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/TestWinterNetSmokeWrapper.ps1 `
  -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
pwsh -NoProfile -File Scripts/TestWinterNetSmokeWrapper.ps1 `
  -ActualBuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/TestNetSmokeRunner.ps1
```

Windows PowerShell 5.1 and PowerShell 7 wrapper suites: **107 checks each, zero
failures**. These retain the original 57 preparation checks, add mock runtime adapter
checks, and accept the actual receipt with 141 current compilation inputs and DLL
SHA256 `4845E04198C1CB419514B50FD0E9060822E68A6866D0AC348D13B19A847068E1`.
Receipt hash before/after matches. This worker only read the parent-provided module;
Wegener's separate build result is not claimed as this worker's build.

Child runner's captured 66-case suite: **878 checks, zero failures, exit 0**.
The original 52-case regression passed earlier: 678 checks, zero failures, exit 0.
New isolated cases cover healthy quoted paths, server/client watchdogs, early exit,
wrong port/map, cleanup ownership/failure, stale/foreign/unpaired/reparse paths,
unknown UDP inventory refusal, and PID/log manifest publication before each launch.
The first expanded run exposed the fixture's Windows 8.3 TEMP alias versus canonical
path comparison; fixed the mock's canonicalization, without changing runtime code.

Captured final wrapper logs:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/s00_02_frozen_4b35471bcec81/wrapper-powershell.log`,
`C:/Users/OZAN CIDIK/AppData/Local/Temp/s00_02_frozen_4b35471bcec81/wrapper-pwsh.log`.
Their fixture roots are recorded in the logs. Child suite log is in that same
proof directory as `child-powershell.log`; its exit is recorded separately.
Final child fixture root:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_net_regression_6c4151717d164005b5bb6a7235933221`.
Tests use a new temporary repo/registered worktree, synthetic DLL/package headers,
real exclusive lock handles/junctions, and mocked CIM inventory. No UE/build/network
process is invoked. Git add/commit operations are confined to that disposable fixture
repository, never the project. Fixture data is retained for review.

Wrapper coverage: AST refusal of outer process/copy/kill/delete commands and exactly
one dynamic local child invocation; main and
copied `.git` refusal; local/UNC paths; exclusive lock contention/release; engine and
dotnet classification; missing/LFS asset refusal; receipt identity/target/exit/hash/
time/source mismatches, untracked inputs and stale DLL; Saved changes and junction
refusal; preflight-only isolation gate; null child evidence; unique
proofs; actual entry refusal on held lock, busy process and failed CIM; finally
releasing the handle after failure. Execute-mode mocks also cover same-lock lifetime,
read-only Saved protection, forbidden new Saved writes, provenance changed after
child, short observations, duplicated PIDs, missing manifest, cleanup failure,
wrong port, actual helper argument/exit/output retention, and partial stdout on
exception. Independent QA/network acceptance: pending Franklin/parent.

This worker: UE build, DLL copy, UE launch, Winter connection, PIE and gameplay:
**NOT_RUN**. Watchdog/cleanup results here are mocked script evidence only.
The five owned files passed whitespace checks (including CRLF handling), all four
PowerShell scripts parsed, and all three documented command blocks parsed.
Frozen SHA256/byte counts and the exact owned diff are recorded in the proof
directory as `frozen-files.json` and `owned.patch`; peer build files remain unchanged.

## Receipt producer contract (already fulfilled by separate module owner)

Fresh isolated Editor module/receipt is accepted; **no rebuild is needed** for this
script-only package. The following exact schema-1 producer is retained as the
receipt contract, not a request to run another build. `Saved/S00_01_Build` is the
separate owner's isolated build-evidence directory. Wrapper never builds, copies
DLLs, produces receipts, or mutates this receipt.

```powershell
$wt = 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject'
. "$wt/Scripts/InvokeWinterNetSmoke.ps1"
$identity = Get-WinterWorktree $wt
$wt = $identity.root
$buildEvidence = Assert-WinterPath (Join-Path $wt 'Saved/S00_01_Build')
$null = [IO.Directory]::CreateDirectory($buildEvidence)
$log = Join-Path $buildEvidence 'build.log'
$receiptPath = Join-Path $buildEvidence 'receipt.json'
$inputs = @(Get-WinterInventory $wt)
$started = [DateTime]::UtcNow
# Historical producer contract: sole module owner only; do not rerun for S00.02.
& powershell -NoProfile -ExecutionPolicy Bypass -File "$wt/Scripts/build.ps1" `
    -Target Editor -Configuration Development -ProjectPath "$wt/MyProject.uproject" `
    -WaitMutex 2>&1 | Tee-Object -FilePath $log
$buildExit = $LASTEXITCODE
$finished = [DateTime]::UtcNow
if ($buildExit -ne 0) { throw "Local build failed: $buildExit" }
if ((ConvertTo-Json -InputObject $inputs -Compress) -cne
    (ConvertTo-Json -InputObject @(Get-WinterInventory $wt) -Compress) -or
    (Get-WinterWorktree $wt).head -cne $identity.head) { throw 'Build inputs changed' }
Save-WinterJson $receiptPath ([ordered]@{
    schema = 1; worktree = $wt; head = $identity.head
    target = 'MyProjectEditor Win64 Development'; exitCode = $buildExit
    startedUtc = $started.ToString('o'); finishedUtc = $finished.ToString('o')
    inputs = $inputs
    dllSha256 = (Get-FileHash -LiteralPath "$wt/Binaries/Win64/UnrealEditor-MyProject.dll" -Algorithm SHA256).Hash
    buildLogSha256 = (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash
})
Assert-WinterProvenance $identity $receiptPath
```

Schema fields are exactly those in this producer. `inputs` is the ordered array of
`{path,sha256}` rows returned by `Get-WinterInventory`; hashes use the standard
uppercase SHA256 representation. `startedUtc` and `finishedUtc` use round-trip UTC
timestamps. The hashed log must contain `Result: Succeeded` and this worktree's
absolute path. The receipt must be within this worktree. No existing build script
is claimed to emit this schema automatically.

## Exact remaining runtime command (not executed)

After Franklin's final review of the frozen five-file diff, the parent assigns one
sole runtime owner. With the accepted existing receipt, that owner executes:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Scripts/InvokeWinterNetSmoke.ps1' `
  -BuildReceipt 'C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject/Saved/S00_01_Build/receipt.json'
```

No unconditional NOT_RUN block remains on the default runtime path. A busy engine/
build process still defers without launching children. UserDir handling of the
actual game's persistence paths and the real Winter map/connection remain unverified
until that one authorized run. Any unexpected Saved write or changed provenance
fails the run. Do not bypass the wrapper or count fixture success as Winter network
completion. No implementation blocker remains beyond final review and the assigned
runtime; no additional harness or compiler retry is proposed.

## Coordinator acceptance and actual run — 2026-10-15

The preceding NOT_RUN statements describe the worker handoff. Erdos froze the five
owned files; Franklin independently accepted the four exact script hashes after
reviewing isolation, cooperative watchdog, owned cleanup and false-PASS prevention.
The coordinator read the captured 878-check/66-case child log (exit 0), both
107-check wrapper logs, and verified the frozen hashes before execution. These are
script fixtures, separate from the real runtime result below.

The coordinator then ran the exact command above once, as sole UE runtime owner
under the common lock, using the already accepted isolated Editor module. No build,
binary copy, source change or timeout adjustment occurred.

**Actual Winter network result: FAILED, exit 1.** The server remained alive for
30.1353413 seconds but had not logged listening on selected port **17916** or Winter
world readiness. The child reported `Sunucu port 17916 dinlemiyor`; it correctly
withheld the client launch. Server PID 2712 was stopped through its owned process
handle; client PID is null. Cleanup succeeded, watchdog did not expire, and no
specified Error/Fatal/Ensure/Access-violation marker appeared in the server log.
Neither join nor welcome was established. This is a startup/readiness failure,
not a successful connection or evidence of an occupied port.

Raw evidence directory:
`C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_winter_net_d5d03dce0be9490a91a0f97d43188bd2`.

| Evidence | SHA256 |
|---|---|
| `result.json` | `DA7A9CEBF52B2532A983BCF4918014023E7790CB27DB84F55822D2F7ADDE8B41` |
| `child/processes.json` | `945FFCC2ACC0AC4360523309B6E9E6478D6E1096537EAAE36CFDECE80F534F2F` |
| `child/server.log` | `4EAD1A6F9FFD466C82EBEDF9553D91FA334351F51F164E5C852E6165916D7B1F` |
| `child-output.log` | `CD39229761E2F092FEFE3E939231E2CFA37A65860A8DC6A8B695A4E900B12D84` |

The wrapper's final audit reports `savedPreserved=true`; only the new owned
`Saved/WinterNet_ce83396beb3b43239348cc0b1dd80862` is excluded from the comparison.
All 141 compilation input hashes, HEAD `4e0f0c5`, build-log hash and DLL hash remained
accepted. The coordinator independently reacquired/released the common lock and
observed no remaining UnrealEditor/ShaderCompileWorker/UBT/LiveCoding/MyProject
processes after completion. The main checkout's player data was not used.

The final server log reaches game-engine initialization and Python plugin startup,
but contains no `LoadMap`/Winter world/listening evidence. Cold startup is a
candidate explanation, not a proven root cause. **S00.04** is the bounded next task:
independently diagnose this exact startup trace before changing readiness handling
or attempting another run. No blind retry or larger timeout is authorized by this
record. S00 network acceptance, two-player gameplay/PIE, dedicated Server and the
main checkout's unowned Race changes remain unverified.

Franklin's independent runtime evidence review agrees: no bind/crash error is
demonstrated; port 1985 is trace control, not the gameplay listener. His bounded
S00.04 recommendation is readiness polling for the exact world and gameplay port
inside the existing 95-second observation deadline, reserving the unchanged
45-second client window. Record readiness elapsed time and distinguish startup
admission failure from network failure. Required fixtures: readiness after the old
30-second checkpoint, never-ready and early exit. Isolation, cleanup, error checks
and actual join/welcome requirements must remain intact. This recommendation has
not yet been implemented or runtime-tested.

Integration uses only the six named owned paths (four scripts, this note and the
board). The existing main post-commit hook launches a background push; the
coordinator disables hook lookup for this commit only with a command-scoped
`core.hooksPath` pointing to a fresh absent TEMP directory. No hook/config file is
changed, and no push is performed for this delivery. The three staged peer trade
blobs remain outside the commit; unowned Race files remain untouched.
