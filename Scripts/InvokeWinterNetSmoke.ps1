# S00.02 Winter adapter. Wrapper alone owns the common Git lock; child owns UE handles.
# Exit 0 = smoke evidence accepted; 1 = rejected/failed; 2 = watchdog; 3 = NOT_RUN.
[CmdletBinding()]
param([string]$BuildReceipt, [switch]$PreflightOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-WinterPath([string]$Path) {
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path.Substring(2) -match '[:"\r\n]') { throw "Absolute local path required: $Path" }
    $full = [IO.Path]::GetFullPath($Path)
    $part = $full
    while ($part) {
        if (Test-Path -LiteralPath $part) {
            if (((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Reparse path refused: $part"
            }
        }
        $parent = Split-Path -Parent $part
        if ($parent -eq $part) { break }
        $part = $parent
    }
    return $full.TrimEnd('\', '/')
}
function Invoke-WinterGit([string]$Root, [string[]]$Arguments) {
    $output = @(& git -C $Root @Arguments)
    if ($LASTEXITCODE -ne 0) { throw "Git failed: $($Arguments -join ' ')" }
    return $output
}
function Get-WinterWorktree([string]$Root) {
    $rootPath = Assert-WinterPath $Root
    if (-not (Test-Path -LiteralPath (Join-Path $rootPath '.git') -PathType Leaf)) {
        throw 'Registered linked worktree required; main checkout refused.'
    }
    $null = Assert-WinterPath (Join-Path $rootPath '.git')
    $top = @(Invoke-WinterGit $rootPath @('rev-parse', '--show-toplevel'))
    $commonRows = @(Invoke-WinterGit $rootPath @('rev-parse', '--path-format=absolute', '--git-common-dir'))
    $gitRows = @(Invoke-WinterGit $rootPath @('rev-parse', '--absolute-git-dir'))
    if ($top.Count -ne 1 -or $commonRows.Count -ne 1 -or $gitRows.Count -ne 1 -or
        (Assert-WinterPath $top[0]) -ine $rootPath) { throw 'Worktree root mismatch.' }
    $common = Assert-WinterPath $commonRows[0]
    $gitDir = Assert-WinterPath $gitRows[0]
    if ($common -ieq $gitDir) { throw 'Main checkout Git directory refused.' }
    $registered = @(Invoke-WinterGit $rootPath @('-c', 'core.quotePath=false', 'worktree', 'list', '--porcelain'))
    $roots = @($registered | Where-Object { $_.StartsWith('worktree ') } | ForEach-Object { Assert-WinterPath $_.Substring(9) })
    if (@($roots | Where-Object { $_ -ieq $rootPath }).Count -ne 1) { throw 'Unregistered worktree refused.' }
    $head = @(Invoke-WinterGit $rootPath @('rev-parse', 'HEAD'))
    if ($head.Count -ne 1) { throw 'HEAD unavailable.' }
    return [pscustomobject]@{ root = $rootPath; common = $common; head = $head[0]; registeredRoots = $roots }
}
function Open-WinterLock([string]$Common) {
    $path = Assert-WinterPath (Join-Path $Common 'agent-build.lock')
    try { return [IO.File]::Open($path, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
    catch [IO.IOException] { throw [IO.IOException]::new('Shared agent-build.lock unavailable.', $_.Exception) }
}
function Get-WinterBusyProcesses([object[]]$Inventory) {
    return @($Inventory | Where-Object {
        $_.Name -match '^(UnrealEditor.*|LiveCoding.*|UnrealBuildTool.*|AutomationTool.*|MSBuild|UAT)\.exe$' -or
        ($_.Name -ieq 'dotnet.exe' -and ([string]::IsNullOrWhiteSpace($_.CommandLine) -or
            $_.CommandLine -match 'UnrealBuildTool|AutomationTool|RunUAT|Epic Games[\\/]UE_'))
    } | Select-Object ProcessId, Name, CommandLine)
}
function Get-WinterInventory([string]$Root) {
    # Includes untracked compilation inputs; missing tracked inputs fail closed.
    $paths = @(Invoke-WinterGit $Root @('-c', 'core.quotePath=false', 'ls-files', '--cached', '--others', '--exclude-standard', '--',
        'Source', 'Config', 'Plugins', 'MyProject.uproject'))
    return @(foreach ($relative in @($paths | Sort-Object -Unique)) {
        $path = Assert-WinterPath (Join-Path $Root $relative)
        if (-not $path.StartsWith($Root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Input escaped worktree.'
        }
        [pscustomobject]@{ path = $relative; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    })
}
function Get-WinterSavedInventory([string]$Root) {
    $saved = Assert-WinterPath (Join-Path $Root 'Saved')
    if (-not (Test-Path -LiteralPath $saved)) { return @() }
    $pending = New-Object 'System.Collections.Generic.Queue[string]'
    $pending.Enqueue($saved)
    while ($pending.Count) {
        $dir = $pending.Dequeue()
        foreach ($entry in @(Get-ChildItem -LiteralPath $dir -Force)) {
            $null = Assert-WinterPath $entry.FullName
            $relative = $entry.FullName.Substring($saved.Length)
            if ($entry.PSIsContainer) {
                [pscustomobject]@{ path = $relative; kind = 'directory'; sha256 = $null }
                $pending.Enqueue($entry.FullName)
            } else {
                [pscustomobject]@{ path = $relative; kind = 'file'; sha256 = (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash }
            }
        }
    }
}
function Assert-WinterAssets([string]$Root) {
    foreach ($relative in @('Content/HandPaintedEnvironment/Maps/Winter_scene/Winter_scene.umap',
        'Content/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.uasset',
        'Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.uasset',
        'Content/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.uasset')) {
        $path = Assert-WinterPath (Join-Path $Root $relative)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required asset missing: $relative" }
        $stream = [IO.File]::OpenRead($path)
        try {
            $header = New-Object byte[] 4
            if ($stream.Read($header, 0, 4) -ne 4 -or [BitConverter]::ToUInt32($header, 0) -ne 0x9E2A83C1L) {
                throw "Unreal asset missing/invalid (including LFS pointer): $relative"
            }
        } finally { $stream.Dispose() }
    }
}
function Assert-WinterProvenance($Worktree, [string]$ReceiptPath) {
    if ([string]::IsNullOrWhiteSpace($ReceiptPath)) { throw 'Local successful Editor build receipt required; never copy a main-checkout DLL.' }
    $receiptFile = Assert-WinterPath $ReceiptPath
    if (-not $receiptFile.StartsWith($Worktree.root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Build receipt must belong to this isolated worktree.'
    }
    $receipt = Get-Content -LiteralPath $receiptFile -Raw | ConvertFrom-Json
    if ($receipt.schema -ne 1 -or (Assert-WinterPath $receipt.worktree) -ine $Worktree.root -or
        $receipt.head -cne $Worktree.head -or $receipt.target -cne 'MyProjectEditor Win64 Development' -or $receipt.exitCode -ne 0) {
        throw 'Build receipt identity/target/exit mismatch.'
    }
    $before = @(Get-WinterInventory $Worktree.root)
    $expected = @($receipt.inputs | ForEach-Object { [pscustomobject]@{ path = $_.path; sha256 = $_.sha256 } })
    if ($before.Count -eq 0 -or (ConvertTo-Json -InputObject $before -Compress) -cne
        (ConvertTo-Json -InputObject $expected -Compress)) { throw 'Build source inventory mismatch.' }
    $dll = Assert-WinterPath (Join-Path $Worktree.root 'Binaries/Win64/UnrealEditor-MyProject.dll')
    $hash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    if ($hash -cne $receipt.dllSha256) { throw 'Build DLL hash mismatch.' }
    $started = Convert-WinterUtc $receipt.startedUtc
    $finished = Convert-WinterUtc $receipt.finishedUtc
    $dllTime = (Get-Item -LiteralPath $dll).LastWriteTimeUtc
    if ($started -gt $finished -or $finished -gt [DateTime]::UtcNow -or $dllTime -lt $started -or $dllTime -gt $finished) {
        throw 'DLL timestamp outside successful local build interval.'
    }
    foreach ($row in $before) {
        if ((Get-Item -LiteralPath (Join-Path $Worktree.root $row.path)).LastWriteTimeUtc -gt $dllTime) {
            throw "DLL older than input: $($row.path)"
        }
    }
    # Receipt is coordinator attestation, checked against the actual successful build log.
    $log = Assert-WinterPath (Join-Path $Worktree.root 'Saved/S00_01_Build/build.log')
    if ((Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash -cne $receipt.buildLogSha256 -or
        -not (Select-String -LiteralPath $log -SimpleMatch 'Result: Succeeded' -Quiet) -or
        -not (Select-String -LiteralPath $log -SimpleMatch $Worktree.root -Quiet)) { throw 'Successful local build log evidence missing.' }
    return [pscustomobject]@{ dllSha256 = $hash; inputs = $before; receipt = $receiptFile }
}
function Convert-WinterUtc($Value) {
    # PS 7 ConvertFrom-Json may materialize ISO timestamps; PS 5 leaves strings.
    if ($Value -is [DateTime]) { return $Value.ToUniversalTime() }
    if ($Value -is [DateTimeOffset]) { return $Value.UtcDateTime }
    return [DateTimeOffset]::ParseExact([string]$Value, 'o', [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::None).UtcDateTime
}
function Save-WinterJson([string]$Path, $Value) {
    $null = Assert-WinterPath $Path
    [IO.File]::WriteAllText($Path, (ConvertTo-Json -InputObject $Value -Depth 12), (New-Object Text.UTF8Encoding($false)))
}
function Invoke-WinterChild([string]$Child, [string]$UserDir, [string]$Logs, [string]$Proof) {
    # Same PowerShell process: no outer shell watchdog, nested lock, or orphanable PID tree.
    $global:LASTEXITCODE = $null
    $output = New-Object 'System.Collections.Generic.List[string]'
    try {
        & $Child -Map Winter_scene -UserDir $UserDir -LogDirectory $Logs | ForEach-Object { $output.Add([string]$_) }
        $nativeExit = $LASTEXITCODE
    } finally {
        $path = Assert-WinterPath (Join-Path $Proof 'child-output.log')
        [IO.File]::WriteAllLines($path, $output.ToArray())
    }
    return $nativeExit
}
function Assert-WinterChildEvidence([string]$Logs, [string]$UserDir) {
    $manifest = Get-Content -LiteralPath (Join-Path $Logs 'processes.json') -Raw | ConvertFrom-Json
    $serverLog = Join-Path $Logs 'server.log'; $clientLog = Join-Path $Logs 'client.log'
    if ($manifest.schema -ne 1 -or $manifest.map -cne 'Winter_scene' -or $manifest.userDir -ine $UserDir -or
        $manifest.exitCode -ne 0 -or -not $manifest.observed -or -not $manifest.cleanupOk -or $manifest.timedOut -or
        $manifest.serverObservedSeconds -lt 30 -or $manifest.clientObservedSeconds -lt 45 -or $manifest.watchdogSeconds -ne 95 -or
        -not $manifest.readinessReady -or -not $manifest.readinessPort -or -not $manifest.readinessMap -or $manifest.startupFailure -or
        $null -eq $manifest.readinessElapsedSeconds -or $manifest.readinessElapsedSeconds -lt 30 -or
        $manifest.startupOverheadSeconds -ne 5 -or $manifest.readinessElapsedSeconds -gt 45 -or
        $manifest.clientBudgetRemainingSeconds -lt 50 -or $manifest.clientBudgetRemainingSeconds -gt (95 - $manifest.readinessElapsedSeconds) -or
        $manifest.serverLog -ine $serverLog -or $manifest.clientLog -ine $clientLog -or $manifest.port -lt 1 -or $manifest.port -gt 65535) {
        throw 'Child observation/cleanup/log manifest rejected.'
    }
    $rows = @($manifest.processes)
    if ($rows.Count -ne 2 -or $rows[0].role -cne 'server' -or $rows[1].role -cne 'client' -or
        $rows[0].pid -le 0 -or $rows[1].pid -le 0 -or $rows[0].pid -eq $rows[1].pid -or
        $rows[0].log -ine $serverLog -or $rows[1].log -ine $clientLog -or
        -not $rows[0].stopped -or -not $rows[1].stopped -or $rows[0].cleanupError -or $rows[1].cleanupError) {
        throw 'Owned process manifest rejected.'
    }
    $server = Get-Content -LiteralPath $serverLog -Raw; $client = Get-Content -LiteralPath $clientLog -Raw
    if ($server -notmatch ("\bLogNet:[^\r\n]*\blistening on port $($manifest.port)\b") -or
        $server -notmatch ('\bLogWorld: Bringing World ' + [regex]::Escape('/Game/HandPaintedEnvironment/Maps/Winter_scene/Winter_scene.Winter_scene') + ' up for play(?:\s|$)') -or
        $server -notmatch 'Join succeeded' -or $client -notmatch 'Welcomed by server' -or
        ($server + $client) -match 'Error:|Fatal error|Ensure condition|Access violation') { throw 'Child connection logs rejected.' }
}
function Invoke-WinterPreflight([string]$Root, [string]$ReceiptPath, [switch]$Execute) {
    $result = [ordered]@{ status = 'NOT_RUN'; runnerExit = 1; map = 'Winter_scene'; proof = $null;
        gate = 'worktree'; reason = $null; childExit = $null; serverLog = $null; clientLog = $null; processManifest = $null;
        childStarted = $false; savedPreserved = $null; provenance = $null; userDir = $null;
        requestedChildArguments = @('-Map', 'Winter_scene'); watchdog = 'NOT_RUN' }
    $lock = $null
    $before = $null
    $worktree = $null
    $savedHandles = New-Object 'System.Collections.Generic.List[System.IO.FileStream]'
    $childHash = $null
    try {
        $worktree = Get-WinterWorktree $Root
        $temp = Assert-WinterPath $env:TEMP
        foreach ($registered in $worktree.registeredRoots) {
            if ($temp -ieq $registered -or $temp.StartsWith($registered + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                throw 'Evidence TEMP must be outside registered worktrees (including Saved).'
            }
        }
        $proof = Assert-WinterPath (Join-Path $temp ('mmo_winter_net_' + [guid]::NewGuid().ToString('N')))
        if (Test-Path -LiteralPath $proof) { throw 'Unique proof directory already exists.' }
        $null = [IO.Directory]::CreateDirectory($proof)
        $result.proof = $proof
        $result.gate = 'lock'
        try { $lock = Open-WinterLock $worktree.common } catch [IO.IOException] {
            $result.runnerExit = 3
            throw 'Shared agent-build.lock held; no process started.'
        }
        $result.gate = 'process-preflight'
        $busy = @(Get-WinterBusyProcesses @(Get-CimInstance -ClassName Win32_Process -ErrorAction Stop))
        Save-WinterJson (Join-Path $proof 'process-preflight.json') $busy
        if ($busy.Count) { $result.runnerExit = 3; throw "Engine/build processes active; untouched PIDs: $(($busy.ProcessId) -join ',')" }
        $result.gate = 'saved-preservation'
        $before = @(Get-WinterSavedInventory $worktree.root | Sort-Object path)
        Save-WinterJson (Join-Path $proof 'saved-before.json') $before
        $result.gate = 'assets'
        Assert-WinterAssets $worktree.root
        $result.gate = 'provenance'
        $result.provenance = Assert-WinterProvenance $worktree $ReceiptPath
        $result.gate = 'child-isolation'
        $child = Assert-WinterPath (Join-Path $worktree.root 'Scripts/NetSmokeTest.ps1')
        $tokens = $null; $errors = $null
        $ast = [Management.Automation.Language.Parser]::ParseFile($child, [ref]$tokens, [ref]$errors)
        if ($errors.Count) { throw 'Child PowerShell parse failed.' }
        $parameters = @($ast.ParamBlock.Parameters | ForEach-Object { $_.Name.VariablePath.UserPath })
        Save-WinterJson (Join-Path $proof 'child-contract.json') ([ordered]@{ path = $child;
            sha256 = (Get-FileHash -LiteralPath $child -Algorithm SHA256).Hash; parameters = $parameters })
        foreach ($required in @('UserDir', 'LogDirectory', 'WatchdogSeconds')) {
            if ($required -notin $parameters) { throw "Child isolation parameter missing: $required" }
        }
        if (-not $Execute) { $result.runnerExit = 3; throw 'NOT_RUN: preflight only; no child invoked.' }
        $result.gate = 'protected-saved'
        foreach ($row in $before) {
            if ($row.kind -ceq 'file') {
                $path = Assert-WinterPath (Join-Path (Join-Path $worktree.root 'Saved') $row.path.TrimStart('\', '/'))
                $savedHandles.Add([IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read))
            }
        }
        $result.userDir = Assert-WinterPath (Join-Path $worktree.root ('Saved/WinterNet_' + [guid]::NewGuid().ToString('N')))
        $logs = Assert-WinterPath (Join-Path $proof 'child')
        $result.serverLog = Join-Path $logs 'server.log'; $result.clientLog = Join-Path $logs 'client.log'
        $result.processManifest = Join-Path $logs 'processes.json'
        $childHash = (Get-FileHash -LiteralPath $child -Algorithm SHA256).Hash
        $result.gate = 'child'
        $result.childStarted = $true
        $result.status = 'FAILED'
        $result.watchdog = '95s observation deadline + at most 10s owned cleanup waits'
        $result.childExit = Invoke-WinterChild $child $result.userDir $logs $proof
        if ($null -eq $result.childExit -or $result.childExit -ne 0) {
            if ($result.childExit -eq 2) { $result.runnerExit = 2 }
            throw "Child failed: exit=$($result.childExit)"
        }
        Assert-WinterChildEvidence $logs $result.userDir
        $result.status = 'PASSED'
        $result.runnerExit = 0
    } catch {
        $result.reason = $_.Exception.Message
    } finally {
        try {
            if ($null -ne $before) {
                $after = @(Get-WinterSavedInventory $worktree.root | Sort-Object path)
                Save-WinterJson (Join-Path $result.proof 'saved-after.json') $after
                $protectedAfter = @($after | Where-Object {
                    -not $result.userDir -or ($_.path -ine ('\' + (Split-Path -Leaf $result.userDir)) -and
                    -not $_.path.StartsWith(('\' + (Split-Path -Leaf $result.userDir) + '\'), [StringComparison]::OrdinalIgnoreCase))
                })
                $result.savedPreserved = (ConvertTo-Json -InputObject $before -Compress) -ceq (ConvertTo-Json -InputObject $protectedAfter -Compress)
                if (-not $result.savedPreserved) { throw 'Saved writes outside owned UserDir detected.' }
            }
            if ($result.childStarted) {
                if ((Get-FileHash -LiteralPath $child -Algorithm SHA256).Hash -cne $childHash -or
                    (Get-WinterWorktree $worktree.root).head -cne $worktree.head) { throw 'Frozen child/HEAD changed during runtime.' }
                $null = Assert-WinterProvenance $worktree $ReceiptPath
            }
        } catch {
            $result.runnerExit = 1
            if ($result.childStarted) { $result.status = 'FAILED' }
            $result.reason += '; preservation: ' + $_.Exception.Message
        } finally {
            try { foreach ($handle in $savedHandles) { $handle.Dispose() } }
            finally { if ($null -ne $lock) { $lock.Dispose() } } # Persistent file retained.
        }
        if ($result.proof) { Save-WinterJson (Join-Path $result.proof 'result.json') $result }
    }
    return [pscustomobject]$result
}
if ($MyInvocation.InvocationName -ne '.') {
    $report = Invoke-WinterPreflight (Split-Path -Parent $PSScriptRoot) $BuildReceipt -Execute:(-not $PreflightOnly)
    Write-Output (ConvertTo-Json -InputObject $report -Depth 12)
    exit $report.runnerExit
}
