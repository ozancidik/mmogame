# S00.03 isolated Editor module preparation - 2026-10-15

Result: PASS. One fresh local Editor build, exit 0, actual UBT `Result: Succeeded`.
Schema-1 receipt passes the current `Assert-WinterProvenance` validator in both
PowerShell 7 and Windows PowerShell 5.1. Native game/network/runtime tests: NOT_RUN.
Parent and Franklin acceptance remain separate from this implementation evidence.

Coordinator acceptance (2026-10-15): current HEAD, exact set and SHA256 of all 141
compilation inputs, fresh DLL SHA256 and timestamp within the recorded build interval,
raw build-log SHA256, actual compile/link lines and `Result: Succeeded` were independently
checked. The common build lock was reacquired and released successfully. This bounded
module-preparation evidence is accepted. An initial coordinator validation used a
culture-sensitive DateTime-to-string conversion; it was corrected to type-aware UTC
handling and rerun successfully. Equivalent portable handling was requested for the
S00.02 wrapper, which remains under its separate writer/review scope.

## Ownership and scope

- Build owner: DevOps Wegener, explicitly authorized for this one build dispatch.
- Worktree and actual launcher cwd:
  `C:\Users\OZAN CIDIK\.codex\worktrees\six-map-sprint0\MyProject`.
- HEAD before and after: `4e0f0c5a5ede948b69c324953f96b88a3ec61dc9` (detached).
- Main was observed at `0fd24a3`; it was not built, copied, or edited by this task.
- Read root AGENTS, agent workflow, Erdos' S00.01 task note and producer/validator,
  and applied ue-build / ecc-mmo-verify within the explicit task restrictions.
- Authored only this note and ignored `Saved/S00_01_Build/build.log` / `receipt.json`.
  UBT generated the isolated Binaries and Intermediate output normally.
- No Source, Config, Plugins, Content, .uproject or script changes by this task.
  Erdos' concurrent script edits are outside the compilation inventory and were
  left untouched. No Clean, DLL copy, install, stage, commit, push or runtime launch.

The accepted build guard remained byte-identical:
`31382CF55F8A8695DAE2434C02F4C7A0DA5D65BA98FF47E7DA4AC859021E9EDB`.
The frozen TestBuildScript remained byte-identical:
`280470342DD5649A0E442F3E4CC10F48C217579F2138259B34FF302C4B849F2C`.

## Actual single build and ownership guard

The actual owned PowerShell child PID was **37492**. Its ProcessStartInfo used
`C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe` with
`-NoProfile -ExecutionPolicy Bypass -EncodedCommand <UTF-16LE base64 of payload below>`.
UseShellExecute=false, CreateNoWindow=true, redirected stdout/stderr, and the cwd
above. The decoded payload was exactly:

```powershell
Set-Location -LiteralPath 'C:\Users\OZAN CIDIK\.codex\worktrees\six-map-sprint0\MyProject'; Write-Output ('ACTUAL_CWD=' + (Get-Location).ProviderPath); & 'C:\Users\OZAN CIDIK\.codex\worktrees\six-map-sprint0\MyProject\Scripts\build.ps1' -Target Editor -Configuration Development -ProjectPath 'C:\Users\OZAN CIDIK\.codex\worktrees\six-map-sprint0\MyProject\MyProject.uproject' -EnginePath 'C:\Program Files\Epic Games\UE_5.7' -WaitMutex; exit $LASTEXITCODE
```

The log's initial `COMMAND=` header is the equivalent readable `powershell.exe
-File ...` invocation; the actual launcher used the encoded payload above. The log
also records `ACTUAL_CWD`, the accepted script's command, and actual UBT invocation:

```text
C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat MyProjectEditor Win64 Development "-Project=C:\Users\OZAN CIDIK\.codex\worktrees\six-map-sprint0\MyProject\MyProject.uproject" -WaitMutex
```

The accepted script alone opened the common-Git `agent-build.lock` with
FileShare.None, checked CIM process inventory under that lock, and held the handle
through its build call. There was no enclosing/nested lock. Preflight found no
busy UE/build process and no process was killed. The outer owner monitored the
returned process handle with a 1800-second bound while draining both output pipes
into the log. Its timeout branch would terminate only the tree rooted at that
owned shell; it was **NOT_TRIGGERED**. No retries were performed.

UBT used the installed UE 5.7, bundled dotnet 8.0.412, VS 2022 MSVC 14.44.35228 and
Windows SDK 10.0.26100.0. UHT reported 88 generated files; UBA executed 13 actions.
Actual UBT total execution time was **76.47 seconds**. The owned shell returned
exit **0**, with `timedOut=False` and UBT `Result: Succeeded` in its captured output.

## Source snapshot and fresh artifact evidence

`Get-WinterInventory` was imported by safe dot-sourcing the wrapper (its autorun
guard was not entered). Before and after, it enumerated tracked and nonignored
untracked Source, Config, Plugins and MyProject.uproject paths. Both complete
ordered `{path,sha256}` snapshots are in the build log. There were **141 inputs**,
**0 nonignored untracked compilation inputs**, identical hashes/path sets and
unchanged HEAD after the build. No main Race edits were imported.

- Started UTC: `2026-10-15T14:17:18.7999536Z`.
- Finished UTC: `2026-10-15T14:18:38.2978236Z`.
- DLL did not exist before this attempt.
- Fresh DLL: `Binaries/Win64/UnrealEditor-MyProject.dll`, **3,563,520 bytes**.
- DLL mtime UTC: `2026-10-15T14:18:36.1443812Z`, inside the build interval and
  no older than any of the 141 input mtimes.
- DLL SHA256: `4845E04198C1CB419514B50FD0E9060822E68A6866D0AC348D13B19A847068E1`.
- Build log SHA256: `04380D54F7FBE0C905B91E6409125017481CE102B0C46324DCC37B4F2E1C33D5`.
- Receipt SHA256: `D57EFF463B6AB89A7C4DC7D2F477F76A8CA56E0F4E620D66B031FF0FE83D4D61`.

## Receipt and release before QA handoff

Evidence files (ignored, local to this worktree):

- `Saved/S00_01_Build/build.log`: full captured build output, exact actual cwd,
  before/after inventories, native exit and DLL freshness evidence.
- `Saved/S00_01_Build/receipt.json`: generated only after the actual successful
  build, unchanged inputs/HEAD, fresh DLL checks and actual UBT success marker.

Receipt fields match the S00.01 producer exactly: `schema=1`, `worktree`, `head`,
`target='MyProjectEditor Win64 Development'`, `exitCode=0`, round-trip UTC
`startedUtc`/`finishedUtc`, ordered `inputs`, `dllSha256`, `buildLogSha256`.
No fields or success evidence were synthesized for a copied binary.

The existing validator rehashed the current inputs, DLL and finalized build log,
checked local worktree/HEAD/target/exit, DLL interval and input mtimes, and found
both the real success marker and isolated absolute path in the log. Validation
passed in PowerShell 7 and independently in Windows PowerShell 5.1 (exit 0).
These were read-only provenance checks, not wrapper/native runtime execution.

After the build, PID 37492 was absent and CIM reported **0 UE/build processes**.
A FileMode.Open/FileShare.None probe of the existing common lock succeeded and
was immediately disposed, confirming the build's lock was released. The persistent
lock file was retained. The process handle and log writer were disposed before
handoff. Build log and receipt are now finalized for parent/Franklin review.

This establishes isolated module preparation only. Winter connection, runtime
watchdog, multi-client/PIE, replication, gameplay and Content behavior: NOT_RUN.
