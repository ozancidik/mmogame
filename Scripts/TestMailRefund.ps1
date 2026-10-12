# O69 regression only. Does not build. Contract pinned to 176 native assertions, including two generation test seams.
# Exit: 0 PASS, 1 rejected/failed, 2 owned-child watchdog, 3 deferred (shared UE resources busy).
[CmdletBinding()]
param(
    [ValidateSet(176)][int]$ExpectedChecks = 176,
    [ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'mail-refund',
    [ValidateRange(1, 90)][int]$TimeoutSeconds = 90,
    [string]$EvidenceRoot = (Join-Path $env:TEMP 'mmo_mail_refund_20261011')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$project = Join-Path $root 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$id = [guid]::NewGuid().ToString('N')
$proof = Join-Path ([IO.Path]::GetFullPath($EvidenceRoot)) ($Label + '_' + $id)
$saved = Join-Path $root 'Saved'
$userDir = Join-Path $saved ('O69_' + $id)
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
function Test-MailRefundLog([string[]]$Lines, [int]$Count) {
    $scenarios=@('normal','savefail','clamp_savefail','clamp_saveok','spend_savefail','spend_saveok',
        'gift_saveok','gift_savefail','fee_spend_savefail','fee_gift_savefail','fee_gift_saveok',
        'appendfail','refund_spend','reentrant_charge','reentrant_refund','inventory_destroy',
        'owner_destroy','mail_destroy','travel_lock','name_change','reload_seam_attachment','reload_seam_fee')
    $suffixes=@('namespace','fixture','result','wallet','mailbox','saves','callback','cleanup')
    $expectedLabels=@(foreach($scenario in $scenarios) {foreach($suffix in $suffixes) {"$scenario.$suffix"}})
    $markers=@($Lines|Select-String '\[MMO_MAIL_REFUND_CASE\]')
    $cases=@($Lines|Select-String '\[MMO_MAIL_REFUND_CASE\] (PASS|FAIL) check=([A-Za-z0-9_.-]+)\s*$')
    $labels=@($cases|ForEach-Object {$_.Matches[0].Groups[2].Value})
    $unique=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach($label in $labels) {$null=$unique.Add($label)}
    $summaries=@($Lines|Select-String '\[MMO_MAIL_REFUND_TEST\]')
    $passSummary=@($Lines|Select-String ("\[MMO_MAIL_REFUND_TEST\] PASS checks=$Count\b reason="))
    $failSummary=@($Lines|Select-String ("\[MMO_MAIL_REFUND_TEST\] FAIL checks=$Count\b reason="))
    $observations=@($Lines|Select-String '\[MMO_MAIL_REFUND_OBSERVATION\] scenario=([A-Za-z0-9_]+) ')
    $errors=@($Lines|Select-String '\bError:|Fatal error|Ensure condition|Access violation')
    $failLabels=@($cases|Where-Object {$_.Matches[0].Groups[1].Value -ceq 'FAIL'}|ForEach-Object {$_.Matches[0].Groups[2].Value})
    $passCases=$cases.Count-$failLabels.Count
    $labelSetMatches=$labels.Count -eq $expectedLabels.Count -and @(Compare-Object -ReferenceObject $expectedLabels -DifferenceObject $labels -CaseSensitive).Count -eq 0
    $startup=@($Lines|Select-String 'LogWorld: Bringing World .+ up for play').Count -ge 1
    $shutdown=@($Lines|Select-String 'LogExit: Exiting\.').Count -eq 1
    $complete=$Count -eq 176 -and $markers.Count -eq $Count -and $cases.Count -eq $Count -and $unique.Count -eq $Count -and $labelSetMatches -and $summaries.Count -eq 1 -and $observations.Count -eq 22 -and $startup -and $shutdown -and $errors.Count -eq 0
    return [pscustomobject]@{valid=($complete -and $passCases -eq $Count -and $passSummary.Count -eq 1);complete=$complete;
        expected=$Count;caseMarkers=$markers.Count;parsedCases=$cases.Count;uniqueLabels=$unique.Count;passCases=$passCases;
        failLabels=$failLabels;summaries=$summaries.Count;passSummaries=$passSummary.Count;failSummaries=$failSummary.Count;
        labelSetMatches=$labelSetMatches;observations=$observations.Count;startup=$startup;shutdown=$shutdown;
        errors=@($errors|ForEach-Object {$_.Line});labels=$labels}
}
function Get-InputInventory {
    $paths=@(& git -C $root ls-files --cached --others --exclude-standard -- Source Config Scripts MyProject.uproject)
    if($LASTEXITCODE -ne 0) {throw 'Source inventory failed'}
    return @(foreach($relative in @($paths|Sort-Object -Unique)) {
        [pscustomobject]@{path=$relative;sha256=(Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash}
    })
}

$null = [IO.Directory]::CreateDirectory($proof)
$lock = $null
$process = $null
$before = @()
$hasBefore = $false
$inputsBefore = @()
$timedOut = $false
$exitCode = 1
$result = [ordered]@{ label = $Label; id = $id; project = $project; map = $null; proof = $proof;
    userDir = $userDir; expectedChecks = $ExpectedChecks; timeoutSeconds = $TimeoutSeconds;
    nativeExit = $null; ownedPid = $null; timedOut = $false; deferred = $false;
    inputsUnchanged = $null; binaryHash = $null; savedPreserved = $null; coverage = $null; failure = $null; runnerExit = 1 }
try {
    if (-not (Test-Path -LiteralPath (Join-Path $root '.git') -PathType Leaf)) {
        throw 'Registered isolated Git worktree required; main checkout refused.'
    }
    foreach ($required in @($project, $exe, (Join-Path $root 'Source/MyProject/Private/Tests/MMOMailRefundSelfTest.cpp'))) {
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
    $hashes = @(foreach ($relative in @('Source/MyProject/Private/Components/MMOMailComponent.cpp',
        'Source/MyProject/Private/Components/MMOInventoryComponent.cpp', 'Source/MyProject/Public/Components/MMOInventoryComponent.h', 'Source/MyProject/Private/Tests/MMOMailRefundSelfTest.cpp',
        'Scripts/TestMailRefund.ps1', 'Config/DefaultEngine.ini')) {
        [pscustomobject]@{ path = $relative; sha256 = (Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash }
    })
    Save-Json (Join-Path $proof 'runtime-source-hashes.json') $hashes
    $inputsBefore = @(Get-InputInventory)
    Save-Json (Join-Path $proof 'inputs-before.json') $inputsBefore
    $result.binaryHash = (Get-FileHash -LiteralPath (Join-Path $root 'Binaries/Win64/UnrealEditor-MyProject.dll') -Algorithm SHA256).Hash
    $null = [IO.Directory]::CreateDirectory($userDir)
    $arguments = @("`"$project`"", $map, '-game', '-MMOMailRefundTest', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
        "-UserDir=`"$userDir`"", "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestMailRefund,quit"')
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
    $result.coverage = Test-MailRefundLog @(Get-Content -LiteralPath $log) $ExpectedChecks
    if ($timedOut) { throw "Owned-child watchdog exceeded ${TimeoutSeconds}s." }
    if ($null -eq $result.nativeExit -or $result.nativeExit -ne 0 -or -not $result.coverage.valid) { throw 'Native exit or exact log coverage rejected.' }
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
        if ($inputsBefore.Count -gt 0) {
            $inputsAfter=@(Get-InputInventory)
            Save-Json (Join-Path $proof 'inputs-after.json') $inputsAfter
            $result.inputsUnchanged=(ConvertTo-Json -InputObject $inputsBefore -Compress) -ceq (ConvertTo-Json -InputObject $inputsAfter -Compress)
            if(-not $result.inputsUnchanged) {throw 'Frozen inputs changed during runtime'}
            if((Get-FileHash -LiteralPath (Join-Path $root 'Binaries/Win64/UnrealEditor-MyProject.dll') -Algorithm SHA256).Hash -cne $result.binaryHash) {throw 'Editor DLL changed during runtime'}
        }
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
Write-Output "[MMO_MAIL_REFUND_RUNNER] label=$Label native_exit=$($result.nativeExit) timeout=$timedOut exit=$exitCode proof=$proof"
if ($null -ne $result.coverage) { Write-Output (ConvertTo-Json -InputObject $result.coverage -Compress -Depth 5) }
if ($result.failure) { Write-Output $result.failure }
exit $exitCode
