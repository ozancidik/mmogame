# O68 regression only. Does not build. Contract pinned to the reviewed 24 native assertions.
# Exit: 0 PASS, 1 rejected/failed, 2 owned-child watchdog, 3 deferred (shared UE resources busy).
[CmdletBinding()]
param(
    [ValidateSet(24)][int]$ExpectedChecks = 24,
    [ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'mail-read-failure',
    [ValidateRange(1, 90)][int]$TimeoutSeconds = 90,
    [string]$EvidenceRoot = (Join-Path $env:TEMP 'mmo_mail_read_failure_20261011')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$id = [guid]::NewGuid().ToString('N')
$proof = Join-Path ([IO.Path]::GetFullPath($EvidenceRoot)) ($Label + '_' + $id)
$saved = Join-Path $root 'Saved'
$userDir = Join-Path $saved ('O68_' + $id)
$log = Join-Path $proof 'ue.log'
$utf8 = New-Object Text.UTF8Encoding($false)

function Save-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, (ConvertTo-Json -InputObject $Value -Depth 12), $utf8)
}
function Get-SavedInventory {
    # Reject links before traversing them; all reads/writes must stay in this worktree.
    $rows = New-Object 'System.Collections.Generic.List[object]'
    if (-not (Test-Path -LiteralPath $saved)) { return $rows.ToArray() }
    $pending = New-Object 'System.Collections.Generic.Queue[string]'
    $pending.Enqueue($saved)
    while ($pending.Count -gt 0) {
        $dir = $pending.Dequeue()
        if (((Get-Item -LiteralPath $dir -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Saved reparse point refused: $dir"
        }
        foreach ($entry in @(Get-ChildItem -LiteralPath $dir -Force)) {
            if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Saved reparse point refused: $($entry.FullName)"
            }
            $relative = $entry.FullName.Substring($saved.Length).TrimStart('\', '/')
            if ($entry.PSIsContainer) {
                $rows.Add([pscustomobject]@{ path = $relative; kind = 'directory'; sha256 = $null; length = $null })
                $pending.Enqueue($entry.FullName)
            } else {
                $rows.Add([pscustomobject]@{ path = $relative; kind = 'file';
                    sha256 = (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash; length = $entry.Length })
            }
        }
    }
    return $rows.ToArray()
}
function Test-MailReadFailureLog([string[]]$Lines, [int]$Count) {
    $expectedLabels = @(
        'guid_namespace_unoccupied', 'missing_load_success_resets_output', 'seed_real_store_append',
        'normal_load_original_mail', 'snapshot_real_mailbox_bytes', 'unreadable_mailbox_still_exists',
        'existing_read_failure_false_resets_output', 'load_reached_exact_read_fault', 'load_preserves_bytes',
        'append_rejects_read_failure', 'append_reached_exact_read_fault', 'append_preserves_original_bytes',
        'take_all_read_failure_false_resets_output', 'take_all_reached_exact_read_fault', 'take_all_preserves_bytes',
        'count_read_failure_returns_zero', 'count_reached_exact_read_fault', 'count_preserves_bytes',
        'read_failure_no_write_move_delete_attempts', 'read_failure_no_tmp_or_quarantine',
        'platform_file_restored', 'restored_read_retains_original_mail', 'restored_normal_append_succeeds',
        'restored_append_retains_old_and_new_mail_in_order'
    )
    $caseMarkers = @($Lines | Select-String '\[MMO_MAIL_READ_FAILURE_CASE\]')
    $cases = @($Lines | Select-String '\[MMO_MAIL_READ_FAILURE_CASE\] (PASS|FAIL) check=([A-Za-z0-9_.-]+)\s*$')
    $summaryMarkers = @($Lines | Select-String '\[MMO_MAIL_READ_FAILURE_TEST\]')
    $passSummary = @($Lines | Select-String ("\[MMO_MAIL_READ_FAILURE_TEST\] PASS checks=$Count\b reason="))
    $errors = @($Lines | Select-String '\[MMO_MAIL_READ_FAILURE_(CASE|TEST)\] FAIL|\bError:|Fatal error|Ensure condition|Access violation')
    $labels = @($cases | ForEach-Object { $_.Matches[0].Groups[2].Value })
    $unique = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($name in $labels) { $null = $unique.Add($name) }
    $labelSetMatches = $labels.Count -eq $expectedLabels.Count -and
        @(Compare-Object -ReferenceObject $expectedLabels -DifferenceObject $labels -CaseSensitive).Count -eq 0
    $passCases = @($cases | Where-Object { $_.Matches[0].Groups[1].Value -ceq 'PASS' })
    $valid = $Count -eq 24 -and $labelSetMatches -and $caseMarkers.Count -eq $Count -and $cases.Count -eq $Count -and
        $unique.Count -eq $Count -and $passCases.Count -eq $Count -and $summaryMarkers.Count -eq 1 -and
        $passSummary.Count -eq 1 -and $errors.Count -eq 0
    return [pscustomobject]@{ valid = $valid; expected = $Count; caseMarkers = $caseMarkers.Count;
        parsedCases = $cases.Count; uniqueLabels = $unique.Count; passCases = $passCases.Count;
        summaries = $summaryMarkers.Count; passSummaries = $passSummary.Count;
        labelSetMatches = $labelSetMatches; errors = @($errors | ForEach-Object { $_.Line }); labels = $labels }
}

$null = [IO.Directory]::CreateDirectory($proof)
$lock = $null
$process = $null
$before = @()
$hasBefore = $false
$timedOut = $false
$exitCode = 1
$result = [ordered]@{ label = $Label; id = $id; project = $project; map = $null; proof = $proof;
    userDir = $userDir; expectedChecks = $ExpectedChecks; timeoutSeconds = $TimeoutSeconds;
    nativeExit = $null; ownedPid = $null; timedOut = $false; deferred = $false;
    savedPreserved = $null; coverage = $null; failure = $null; runnerExit = 1 }
try {
    if (-not (Test-Path -LiteralPath (Join-Path $root '.git') -PathType Leaf)) {
        throw 'Registered isolated Git worktree required; main checkout refused.'
    }
    foreach ($required in @($project, $exe, (Join-Path $root 'Source/MyProject/Private/Tests/MMOMailReadFailureSelfTest.cpp'))) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file missing: $required" }
    }
    $commonOutput = @(& git -C $root rev-parse --path-format=absolute --git-common-dir)
    if ($LASTEXITCODE -ne 0 -or $commonOutput.Count -ne 1) { throw 'Cannot resolve shared Git directory.' }
    $common = [IO.Path]::GetFullPath($commonOutput[0])
    try {
        $lock = [IO.File]::Open((Join-Path $common 'agent-build.lock'),
            [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    } catch [IO.IOException] {
        $result.deferred = $true
        $exitCode = 3
        throw 'DEFER: common agent-build.lock is held; no UE child started.'
    }
    $inventory = @(Get-CimInstance -ClassName Win32_Process -ErrorAction Stop)
    $busy = @($inventory | Where-Object {
        $_.Name -match '^(UnrealEditor.*|LiveCoding.*|UnrealBuildTool.*|AutomationTool.*|MSBuild|UAT)\.exe$' -or
        ($_.Name -eq 'dotnet.exe' -and ([string]::IsNullOrWhiteSpace($_.CommandLine) -or
            $_.CommandLine -match 'UnrealBuildTool|AutomationTool|RunUAT|Epic Games[\\/]UE_'))
    } | Select-Object ProcessId, Name, CommandLine)
    Save-Json (Join-Path $proof 'process-preflight.json') $busy
    if ($busy.Count -gt 0) {
        $result.deferred = $true
        $exitCode = 3
        throw "DEFER: UE/build process active; untouched PIDs: $(($busy.ProcessId) -join ',')"
    }
    $mapRows = @(Get-Content -LiteralPath (Join-Path $root 'Config/DefaultEngine.ini') |
        Where-Object { $_ -match '^GameDefaultMap=' })
    if ($mapRows.Count -ne 1) { throw 'Expected exactly one GameDefaultMap in current config.' }
    $map = (($mapRows[0] -split '=', 2)[1].Trim() -split '\.', 2)[0]
    if ($map -notmatch '^/Game/[A-Za-z0-9_/]+$' -or
        -not (Test-Path -LiteralPath (Join-Path $root ('Content/' + $map.Substring(6) + '.umap')) -PathType Leaf)) {
        throw "Configured map missing or invalid: $map"
    }
    $result.map = $map
    $before = @(Get-SavedInventory)
    $hasBefore = $true
    Save-Json (Join-Path $proof 'saved-before.json') $before
    $hashes = @(foreach ($relative in @('Source/MyProject/Private/Mail/MMOMailStore.cpp',
        'Source/MyProject/Public/Mail/MMOMailStore.h', 'Source/MyProject/Private/Tests/MMOMailReadFailureSelfTest.cpp',
        'Scripts/TestMailReadFailure.ps1', 'Config/DefaultEngine.ini')) {
        [pscustomobject]@{ path = $relative; sha256 = (Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash }
    })
    Save-Json (Join-Path $proof 'runtime-source-hashes.json') $hashes
    $null = [IO.Directory]::CreateDirectory($userDir)
    $arguments = @("`"$project`"", $map, '-game', '-MMOMailReadFailureTest', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
        "-UserDir=`"$userDir`"", "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestMailReadFailure,quit"')
    Save-Json (Join-Path $proof 'invocation.json') ([ordered]@{ executable = $exe; arguments = $arguments })
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $root -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $proof 'stdout.log') -RedirectStandardError (Join-Path $proof 'stderr.log')
    # Windows PowerShell 5.1 must retain the native handle before waiting; otherwise ExitCode can be null.
    $null = $process.Handle
    $result.ownedPid = $process.Id
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $timedOut = $true
        $exitCode = 2
        if (-not $process.HasExited) { $process.Kill() } # Only the UE child returned above.
        if (-not $process.WaitForExit(5000)) { throw 'Owned watchdog child did not stop.' }
    }
    $process.Refresh()
    $result.nativeExit = $process.ExitCode
    if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { throw 'UE log missing.' }
    $result.coverage = Test-MailReadFailureLog @(Get-Content -LiteralPath $log) $ExpectedChecks
    if ($timedOut) { throw "Owned-child watchdog exceeded ${TimeoutSeconds}s." }
    if ($result.nativeExit -ne 0 -or -not $result.coverage.valid) { throw 'Native exit or exact log coverage rejected.' }
    $exitCode = 0
} catch {
    $result.failure = $_.Exception.Message
} finally {
    # Never enumerate-and-kill. Dispose/stop only the process this invocation created.
    if ($null -ne $process) {
        try {
            if (-not $process.HasExited) {
                $process.Kill()
                if (-not $process.WaitForExit(5000)) { throw 'Owned child did not exit during cleanup.' }
            }
        } catch {
            $result.failure = [string]$result.failure + '; cleanup: ' + $_.Exception.Message
            if ($exitCode -eq 0) { $exitCode = 1 }
        } finally { $process.Dispose() }
    }
    try {
        if ($hasBefore) {
            $after = @(Get-SavedInventory)
            Save-Json (Join-Path $proof 'saved-after.json') $after
            $afterByPath = @{}
            foreach ($row in $after) { $afterByPath[$row.path] = $row }
            $changed = @(foreach ($row in $before) {
                if (-not $afterByPath.ContainsKey($row.path) -or
                    $row.kind -cne $afterByPath[$row.path].kind -or
                    $row.sha256 -cne $afterByPath[$row.path].sha256 -or $row.length -ne $afterByPath[$row.path].length) { $row.path }
            })
            $result.savedPreserved = $changed.Count -eq 0
            Save-Json (Join-Path $proof 'saved-existing-changes.json') $changed
            if (-not $result.savedPreserved) {
                $result.failure = [string]$result.failure + '; existing Saved content changed.'
                if ($exitCode -eq 0) { $exitCode = 1 }
            }
        }
    } catch {
        $result.failure = [string]$result.failure + '; Saved evidence: ' + $_.Exception.Message
        if ($exitCode -eq 0) { $exitCode = 1 }
    } finally {
        if ($null -ne $lock) { $lock.Dispose() } # Lock file intentionally retained.
    }
    $result.timedOut = $timedOut
    $result.runnerExit = $exitCode
    Save-Json (Join-Path $proof 'result.json') $result
}
Write-Output "[MMO_MAIL_READ_FAILURE_RUNNER] label=$Label native_exit=$($result.nativeExit) timeout=$timedOut exit=$exitCode proof=$proof"
if ($null -ne $result.coverage) { Write-Output (ConvertTo-Json -InputObject $result.coverage -Compress -Depth 5) }
if ($result.failure) { Write-Output $result.failure }
exit $exitCode
