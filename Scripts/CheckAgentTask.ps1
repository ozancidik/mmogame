#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$TaskManifest,
    [switch]$Build,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Shared helpers: NewAgentTask dot-sources this file; that never runs a check/build.
function Get-AgentPath {
    param([Parameter(Mandatory)][string]$Path)
    $full = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $root = [IO.Path]::GetPathRoot($full)
    if ($full -eq $root.TrimEnd('\', '/') -or $full.StartsWith('\\')) {
        throw "Drive roots and UNC paths are not task paths: $Path"
    }
    $cursor = $full
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Reparse points/junctions are not allowed: $cursor"
            }
        }
        $parent = [IO.Path]::GetDirectoryName($cursor)
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
    return $full
}

function Test-AgentChild {
    param([string]$Path, [string]$Parent)
    return $Path.StartsWith($Parent.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)
}

function Invoke-AgentGit {
    param([string]$Root, [string[]]$Arguments)
    # Windows PowerShell 5.1 represents native stderr (even Git progress) as
    # ErrorRecord. Inspect the exit code rather than terminating on progress.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& git -C $Root @Arguments 2>&1)
        $gitExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($gitExit -ne 0) {
        throw "git $($Arguments -join ' ') failed ($gitExit): $($output -join ' ')"
    }
    return $output
}

function Get-AgentRepo {
    param([string]$Path)
    $full = Get-AgentPath $Path
    $actual = Get-AgentPath ([string](Invoke-AgentGit $full @('rev-parse', '--show-toplevel')))
    if ($full -ne $actual) { throw "Expected repository/worktree root: $full; actual: $actual" }
    return $full
}

function Get-AgentCommonGit {
    param([string]$Root)
    return Get-AgentPath ([string](Invoke-AgentGit $Root @('rev-parse', '--path-format=absolute', '--git-common-dir')))
}

function Get-AgentOwnedPath {
    param([string]$Path)
    $normalized = $Path.Replace('\', '/').TrimEnd('/')
    if ([string]::IsNullOrWhiteSpace($normalized) -or $normalized -match '[\x00-\x1f<>:"|?*]' -or
        [IO.Path]::IsPathRooted($normalized)) { throw "Unsafe owned path: $Path" }
    foreach ($part in $normalized.Split('/')) {
        if ($part -in @('', '.', '..', '.git') -or $part -match '[. ]$' -or
            $part -match '^(?i:CON|PRN|AUX|NUL|CLOCK\$|COM[1-9]|LPT[1-9])(?:\.|$)') {
            throw "Unsafe owned path segment: $Path"
        }
    }
    return $normalized
}

function Test-AgentOwned {
    param([string]$Path, [string[]]$Owned)
    foreach ($entry in $Owned) {
        if ($Path.Equals($entry, [StringComparison]::OrdinalIgnoreCase) -or
            $Path.StartsWith($entry + '/', [StringComparison]::OrdinalIgnoreCase)) { return $true }
    }
    return $false
}

function Get-AgentWorktrees {
    param([string]$Root)
    foreach ($line in @(Invoke-AgentGit $Root @('-c', 'core.quotePath=false', 'worktree', 'list', '--porcelain'))) {
        if ([string]$line -like 'worktree *') { ([string]$line).Substring(9) }
    }
}

function Write-AgentNewFile {
    param([string]$Path, [string]$Text)
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $bytes = (New-Object Text.UTF8Encoding($false)).GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally { $stream.Dispose() }
}

function Assert-AgentEngineIdle {
    # Fail closed if process inventory cannot be read. Includes dotnet-hosted UBT/UAT.
    $processes = @(Get-CimInstance -ClassName Win32_Process -ErrorAction Stop)
    $busy = @($processes | Where-Object {
        $_.Name -match '^(UnrealEditor.*|LiveCoding.*|UnrealBuildTool|AutomationTool|MSBuild|UAT)\.exe$' -or
        ($_.Name -eq 'dotnet.exe' -and
            ([string]::IsNullOrWhiteSpace($_.CommandLine) -or
             $_.CommandLine -match 'UnrealBuildTool|AutomationTool|RunUAT'))
    })
    if ($busy.Count) {
        throw "UE/editor/build process active; leave it untouched: $(( $busy | ForEach-Object { '{0} PID={1}' -f $_.Name,$_.ProcessId }) -join ', ')"
    }
}

function Invoke-AgentTaskCheck {
    param([string]$ManifestPath, [bool]$RunBuild, [string]$Engine)
    $manifestFile = Get-AgentPath $ManifestPath
    $task = Get-Content -LiteralPath $manifestFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($task.SchemaVersion -ne 1) { throw 'Unsupported task manifest schema.' }
    $worktree = Get-AgentRepo $task.Worktree
    $repo = Get-AgentRepo $task.Repository
    $common = Get-AgentCommonGit $worktree
    if ($common -ne (Get-AgentCommonGit $repo)) { throw 'Task worktree belongs to another repository.' }
    $registered = @(Get-AgentWorktrees $repo | ForEach-Object { Get-AgentPath $_ })
    if ($worktree -notin $registered -or $worktree -eq $repo) { throw 'Task is not a separate registered worktree.' }
    $tasksDir = Get-AgentPath (Join-Path $worktree 'docs/tasks')
    if (-not (Test-AgentChild $manifestFile $tasksDir)) { throw 'Manifest must be in worktree docs/tasks.' }
    $expectedName = '{0}.{1}.{2}.agent-task.json' -f $task.Task,$task.Owner,$task.Role
    if ([IO.Path]::GetFileName($manifestFile) -ne $expectedName) { throw 'Manifest name does not match task identity.' }
    $branch = [string](Invoke-AgentGit $worktree @('branch', '--show-current'))
    if ($branch -ne $task.Branch) { throw "Task branch changed: $branch" }
    if ($task.BaseCommit -notmatch '^[0-9a-f]{40,64}$') { throw 'Invalid base commit.' }
    $null = Invoke-AgentGit $worktree @('merge-base', '--is-ancestor', $task.BaseCommit, 'HEAD')
    $owned = @($task.OwnedPaths | ForEach-Object { Get-AgentOwnedPath $_ })
    if (-not $owned.Count) { throw 'Task has no owned paths.' }
    foreach ($entry in $owned) { $null = Get-AgentPath (Join-Path $worktree $entry) }
    $relativeManifest = 'docs/tasks/' + $expectedName
    $record = $relativeManifest.Replace('.json', '.md')
    $changed = @(
        Invoke-AgentGit $worktree @('-c', 'core.quotePath=false', 'diff', '--no-renames', '--name-only', $task.BaseCommit, 'HEAD', '--')
        Invoke-AgentGit $worktree @('-c', 'core.quotePath=false', 'diff', '--no-renames', '--name-only', '--cached', '--')
        Invoke-AgentGit $worktree @('-c', 'core.quotePath=false', 'diff', '--no-renames', '--name-only', '--')
        Invoke-AgentGit $worktree @('-c', 'core.quotePath=false', 'ls-files', '--others', '--exclude-standard')
    ) | Sort-Object -Unique
    foreach ($entry in $changed) { $null = Get-AgentPath (Join-Path $worktree ([string]$entry)) }
    $outside = @($changed | Where-Object {
        $_ -notin @($relativeManifest, $record) -and -not (Test-AgentOwned ([string]$_) $owned)
    })
    if ($outside.Count) { throw "Changes outside task ownership: $($outside -join ', ')" }
    $result = [ordered]@{
        Task = $task.Task; Role = $task.Role; Worktree = $worktree; Branch = $branch
        BaseCommit = $task.BaseCommit; ChangedPaths = @($changed)
        Ownership = 'PASS'; Build = 'NOT_RUN'; RuntimeDispatch = 'NOT_VERIFIED_BY_SCRIPT'
    }
    if ($RunBuild) {
        $enginePath = Get-AgentPath $Engine
        $buildBat = Get-AgentPath (Join-Path $enginePath 'Engine/Build/BatchFiles/Build.bat')
        $project = Get-AgentPath (Join-Path $worktree 'MyProject.uproject')
        if (-not (Test-Path -LiteralPath $buildBat -PathType Leaf) -or
            -not (Test-Path -LiteralPath $project -PathType Leaf)) { throw 'Engine Build.bat or project missing.' }
        # Persistent lock file; the open handle is the lock. Never delete a stale file.
        $lock = [IO.File]::Open((Join-Path $common 'agent-build.lock'),
            [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        try {
            Assert-AgentEngineIdle
            Push-Location -LiteralPath $worktree
            try {
                & $buildBat MyProjectEditor Win64 Development "-Project=$project" -WaitMutex
                $buildExit = $LASTEXITCODE
            } finally { Pop-Location }
            if ($buildExit -ne 0) { throw "UE build failed: exit=$buildExit" }
            $result.Build = 'PASS exit=0'
        } finally { $lock.Dispose() }
    }
    [pscustomobject]$result
}

if ($MyInvocation.InvocationName -ne '.') {
    if ([string]::IsNullOrWhiteSpace($TaskManifest)) { throw '-TaskManifest is required.' }
    Invoke-AgentTaskCheck $TaskManifest ([bool]$Build) $EngineRoot
}
