#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory)][ValidatePattern('^[a-z][a-z0-9-]{0,47}$')][string]$Task,
    [Parameter(Mandatory)][ValidateSet('codex', 'claude')][string]$Owner,
    [Parameter(Mandatory)][ValidateSet('orchestrator', 'ue5-developer', 'network', 'backend',
        'qa-automation', 'game-designer', 'npc-ai', 'security', 'asset-integration',
        'devops', 'performance', 'documentation')][string]$Role,
    [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$Objective,
    [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string[]]$OwnedPath,
    [string]$Repository = (Split-Path -Parent $PSScriptRoot),
    [string]$WorktreeRoot,
    [string]$BaseRef = 'HEAD'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CheckAgentTask.ps1')

$repo = Get-AgentRepo $Repository
$common = Get-AgentCommonGit $repo
if ([string]::IsNullOrWhiteSpace($WorktreeRoot)) {
    $WorktreeRoot = Join-Path (Split-Path -Parent $repo) ((Split-Path -Leaf $repo) + '-agent-worktrees')
}
$pool = Get-AgentPath $WorktreeRoot
if ($pool -eq $repo -or (Test-AgentChild $pool $repo) -or (Test-AgentChild $repo $pool)) {
    throw 'Worktree pool must be outside the source checkout and cannot contain it.'
}
$leaf = "$Task-$Owner-$Role"
$target = Get-AgentPath (Join-Path $pool $leaf)
if (-not (Test-AgentChild $target $pool)) { throw 'Task path escaped the worktree pool.' }
if (Test-Path -LiteralPath $target) { throw "Refusing existing target: $target" }
$owned = @($OwnedPath | ForEach-Object { Get-AgentOwnedPath $_ } | Sort-Object -Unique)
foreach ($entry in $owned) { $null = Get-AgentPath (Join-Path $target $entry) }
if ([string]::IsNullOrWhiteSpace($BaseRef) -or $BaseRef.StartsWith('-')) { throw 'Invalid base ref.' }
$base = [string](Invoke-AgentGit $repo @('rev-parse', '--verify', '--end-of-options', "$BaseRef^{commit}"))
$branch = "$Owner/$leaf"
$null = Invoke-AgentGit $repo @('check-ref-format', '--branch', $branch)
$existingBranch = @(& git -C $repo show-ref --verify --quiet "refs/heads/$branch" 2>&1)
if ($LASTEXITCODE -eq 0) { throw "Refusing existing branch: $branch" }
if ($LASTEXITCODE -ne 1) { throw "Unable to inspect branch: $existingBranch" }
$startingStatus = @(Invoke-AgentGit $repo @('status', '--short'))
$plan = [pscustomobject]@{ Task = $Task; Role = $Role; Branch = $branch; BaseCommit = $base; Worktree = $target }
if (-not $PSCmdlet.ShouldProcess($target, "Create task worktree on $branch from $base")) { return $plan }

# Serializes task allocation across tools/worktrees using the same protocol.
$allocationLock = [IO.File]::Open((Join-Path $common 'agent-task-create.lock'),
    [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    foreach ($other in @(Get-AgentWorktrees $repo)) {
        $otherPath = Get-AgentPath $other
        if ($target -eq $otherPath -or (Test-AgentChild $target $otherPath) -or
            (Test-AgentChild $otherPath $target) -or $pool -eq $otherPath -or
            (Test-AgentChild $pool $otherPath)) { throw "Worktree path overlaps registered checkout: $otherPath" }
    }
    # Git itself handles a concurrent branch/target collision without --force.
    if (-not (Test-Path -LiteralPath $pool)) { $null = New-Item -ItemType Directory -Path $pool }
    $null = Get-AgentPath $pool
    $null = Invoke-AgentGit $repo @('worktree', 'add', '-b', $branch, $target, $base)
    try {
        $null = Get-AgentRepo $target
        $tasksDir = Get-AgentPath (Join-Path $target 'docs/tasks')
        if (-not (Test-Path -LiteralPath $tasksDir)) { $null = New-Item -ItemType Directory -Path $tasksDir -Force }
        $manifestName = "$Task.$Owner.$Role.agent-task.json"
        $manifestPath = Join-Path $tasksDir $manifestName
        $manifest = [ordered]@{
            SchemaVersion = 1; Task = $Task; Owner = $Owner; Role = $Role; Objective = $Objective
            Repository = $repo; Worktree = $target; Branch = $branch; BaseCommit = $base
            OwnedPaths = $owned; CreatedUtc = [DateTime]::UtcNow.ToString('o')
            SourceStatusAtCreation = $startingStatus; RuntimeDispatch = 'NOT_DISPATCHED'
        }
        Write-AgentNewFile $manifestPath ($manifest | ConvertTo-Json -Depth 6)
        $templatePath = Join-Path $repo 'docs/tasks/AGENT_TASK_TEMPLATE.md'
        $template = if (Test-Path -LiteralPath $templatePath) {
            Get-Content -LiteralPath (Get-AgentPath $templatePath) -Raw -Encoding UTF8
        } else { '# Acceptance, delegation evidence, verification and integration handoff' }
        $record = "# $Task / $Role / $Owner`n`nObjective: $Objective`n`nWorktree: $target`nBranch: $branch`nBase: $base`nOwned paths: $($owned -join ', ')`nRuntime: NOT_DISPATCHED`n`n$template"
        Write-AgentNewFile (Join-Path $tasksDir $manifestName.Replace('.json', '.md')) $record
        if ($startingStatus.Count) {
            Write-Warning 'Source checkout is dirty. Only the selected committed base was copied; peer/untracked changes remain in source.'
        }
        Write-Warning 'This script creates a task only. It does not launch Codex/Claude, build, commit or push. Ensure agent/tooling files exist at the selected base.'
        [pscustomobject]@{ Task = $Task; Role = $Role; Worktree = $target; Branch = $branch
            BaseCommit = $base; TaskManifest = $manifestPath; RuntimeDispatch = 'NOT_DISPATCHED' }
    } catch {
        throw "Worktree retained for inspection at $target (branch $branch); no cleanup was attempted. $($_.Exception.Message)"
    }
} finally { $allocationLock.Dispose() }
