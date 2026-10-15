#Requires -Version 5.1
<#
.SYNOPSIS
    Isolated build-script contract tests. Never launches UE or deletes files.
.NOTES
    Uses AST-loaded helpers, mocked side effects, and a new temporary Git fixture.
    Does not stage, commit, install, or delete fixture files. Retains evidence in TEMP.
#>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:checks = 0
$buildScript = Join-Path $PSScriptRoot 'build.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('mmo-build-script-' + [guid]::NewGuid().ToString('N'))
$utf8 = New-Object Text.UTF8Encoding($false)

function Assert-Case([string]$Label, [bool]$Condition) {
    if (-not $Condition) { throw "FAIL: $Label" }
    $script:checks++
    Write-Output "PASS: $Label"
}
function Assert-Throws([string]$Label, [scriptblock]$Action, [string]$Pattern) {
    $message = ''
    try { $null = & $Action } catch { $message = $_.Exception.Message }
    Assert-Case $Label ($message -like $Pattern)
}
function Write-Fixture([string]$Path, [string]$Text) {
    $null = [IO.Directory]::CreateDirectory((Split-Path -Parent $Path))
    [IO.File]::WriteAllText($Path, $Text, $utf8)
}
function Get-FixtureSnapshot {
    $rows = @(foreach ($entry in @(Get-ChildItem -LiteralPath $fixture -Force -Recurse | Sort-Object FullName)) {
        $relative = $entry.FullName.Substring($fixture.Length)
        if ($entry.PSIsContainer) { "D $relative" } elseif ($entry.Name -eq 'agent-build.lock') {
            # FileShare.None intentionally forbids reading the held lock's contents.
            'L {0} {1} {2}' -f $relative, $entry.Length, $entry.LastWriteTimeUtc.Ticks
        } else {
            'F {0} {1} {2} {3}' -f $relative, $entry.Length, $entry.LastWriteTimeUtc.Ticks,
                (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash
        }
    })
    return ($rows -join "`n")
}
function Assert-LockReleased([string]$Label) {
    $probe = Enter-BuildLock $ProjectPath
    try { Assert-Case $Label ($null -ne $probe) } finally { $probe.Dispose() }
}

try {
    # Parse both files, then load only function definitions: no top-level build code runs.
    $buildAst = $null
    foreach ($path in @($buildScript, $PSCommandPath)) {
        $tokens = $null
        $parseErrors = $null
        $ast = [Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$parseErrors)
        Assert-Case ('AST parse ' + [IO.Path]::GetFileName($path)) (@($parseErrors).Count -eq 0)
        Assert-Case ('ASCII ' + [IO.Path]::GetFileName($path)) (-not [regex]::IsMatch([IO.File]::ReadAllText($path), '[^\x00-\x7f]'))
        if ($path -eq $buildScript) { $buildAst = $ast }
    }
    foreach ($node in $buildAst.FindAll({ param($item) $item -is [Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
        . ([scriptblock]::Create($node.Extent.Text))
    }
    # Any accidental native launch or removal fails the test before touching files.
    function Start-Process { throw 'TEST FORBIDS process launch' }
    function Remove-Item { throw 'TEST FORBIDS removal' }
    function Stop-Process { throw 'TEST FORBIDS process kill' }
    function Write-Log { param($Message, $Level) }

    $repo = Join-Path $fixture 'repo'
    $null = [IO.Directory]::CreateDirectory($repo)
    $null = & git -C $repo init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Fixture git init failed.' }
    $ProjectPath = Join-Path $repo 'MyProject.uproject'
    $DefaultProjectPath = $ProjectPath
    $EnginePath = Join-Path $fixture 'missing-engine'
    $Target = 'All'
    $Configuration = 'Development'
    $Clean = $false
    $CleanDdc = $false
    $WaitMutex = $false
    Write-Fixture $ProjectPath '{}'
    Write-Fixture (Join-Path $repo 'Source/MyProjectEditor.Target.cs') 'Type = TargetType.Editor;'
    Write-Fixture (Join-Path $repo 'Source/MyProject.Target.cs') 'Type = TargetType.Game;'
    Write-Fixture (Join-Path $repo 'Source/MyProject/MyProjectServer.Target.cs') "// Type = TargetType.Client;`n/* Type = TargetType.Game; */`nType = TargetType.Server;"
    foreach ($name in @('Binaries', 'Intermediate', 'DerivedDataCache', 'Saved')) {
        Write-Fixture (Join-Path $repo "$name/sentinel.txt") "preserve-$name"
    }
    $targets = @(Resolve-BuildTargets $ProjectPath 'All')
    Assert-Case 'All resolves Editor/Game/nested Server in type order' (($targets.Name -join ',') -ceq 'MyProjectEditor,MyProject,MyProjectServer')
    foreach ($type in @('Editor', 'Game', 'Server')) {
        $resolved = @(Resolve-BuildTargets $ProjectPath $type)
        Assert-Case ("Explicit $type resolves only its type") ($resolved.Count -eq 1 -and $resolved[0].Type -ceq $type)
    }
    Assert-Throws 'Undefined Client rejected' { Resolve-BuildTargets $ProjectPath 'Client' } "Target 'Client' has no target definition*"
    Assert-Throws 'Unknown target rejected' { Resolve-BuildTargets $ProjectPath 'Bogus' } "Target 'Bogus' has no target definition*"
    Assert-Case 'Real worktree All uses actual definitions' ((@(Resolve-BuildTargets (Join-Path (Split-Path -Parent $PSScriptRoot) 'MyProject.uproject') 'All').Name -join ',') -ceq 'MyProjectEditor,MyProject,MyProjectServer')

    # A renamed definition proves selection uses TargetType, not a guessed name suffix.
    $typedProject = Join-Path $fixture 'typed/Typed.uproject'
    Write-Fixture $typedProject '{}'
    Write-Fixture (Join-Path $fixture 'typed/Source/OddEditor.Target.cs') 'Type = TargetType.Server;'
    Write-Fixture (Join-Path $fixture 'typed/Source/ActualClient.Target.cs') 'Type = TargetType.Client;'
    Assert-Case 'Type assignment overrides filename suffix' ((@(Resolve-BuildTargets $typedProject 'Server')[0].Name) -ceq 'OddEditor')
    Assert-Case 'Defined Client participates in All' ((@(Resolve-BuildTargets $typedProject 'All').Name -join ',') -ceq 'OddEditor,ActualClient')
    Assert-Throws 'Missing Editor is rejected' { Resolve-BuildTargets $typedProject 'Editor' } "Target 'Editor' has no target definition*"
    $emptyProject = Join-Path $fixture 'empty/Empty.uproject'
    Write-Fixture $emptyProject '{}'
    Assert-Throws 'No definitions cannot report All success' { Resolve-BuildTargets $emptyProject 'All' } "Target 'All' has no target definition*"
    Write-Fixture (Join-Path $fixture 'empty/Source/Bad.Target.cs') 'Type = TargetType.Game; Type = TargetType.Server;'
    Assert-Throws 'Ambiguous definitions fail closed' { Resolve-BuildTargets $emptyProject 'All' } 'Expected one literal TargetType assignment:*'

    & {
        $Target = 'Client'
        $Clean = $true
        $CleanDdc = $true
        function Test-Prerequisites { throw 'TEST: prerequisites reached before rejection' }
        function Enter-BuildLock { throw 'TEST: lock reached before rejection' }
        function Get-CimInstance { throw 'TEST: process scan reached before rejection' }
        function Clean-Build { throw 'TEST: clean reached before rejection' }
        function Invoke-Build { throw 'TEST: build reached before rejection' }
        $before = Get-FixtureSnapshot
        Assert-Throws 'Invalid Client with both clean flags rejects before side effects' { Invoke-ProjectBuild } "Target 'Client' has no target definition*"
        Assert-Case 'Invalid target leaves every fixture file and directory unchanged' ((Get-FixtureSnapshot) -ceq $before)
        & $buildScript -ProjectPath $ProjectPath -EnginePath $EnginePath -Target Client -Clean -CleanDdc
        Assert-Case 'Script entry point exits 1 for missing Client' ($LASTEXITCODE -eq 1)
        Assert-Case 'Invalid Client entry point leaves fixture unchanged' ((Get-FixtureSnapshot) -ceq $before)
    }

    # Synthetic linked-worktree metadata needs no commit, index, branch or real worktree writes.
    $common = Join-Path $repo '.git'
    $peer = Join-Path $fixture 'peer'
    $metadata = Join-Path $common 'worktrees/fixture-peer'
    Write-Fixture (Join-Path $peer 'MyProject.uproject') '{}'
    Write-Fixture (Join-Path $peer '.git') ('gitdir: ' + $metadata.Replace('\', '/'))
    Write-Fixture (Join-Path $metadata 'commondir') '../..'
    Write-Fixture (Join-Path $metadata 'HEAD') 'ref: refs/heads/fixture-peer'
    Write-Fixture (Join-Path $metadata 'gitdir') ((Join-Path $peer '.git').Replace('\', '/'))
    $held = Enter-BuildLock $ProjectPath
    try {
        $before = Get-FixtureSnapshot
        Assert-Throws 'Linked worktree contends on common Git lock' { Enter-BuildLock (Join-Path $peer 'MyProject.uproject') } 'DEFER: common agent-build.lock*'
        foreach ($wantClean in @($false, $true)) {
            & {
                $Clean = $wantClean
                $CleanDdc = $wantClean
                function Test-Prerequisites { throw 'TEST: prerequisites reached while locked' }
                function Get-CimInstance { throw 'TEST: process scan reached while locked' }
                function Clean-Build { throw 'TEST: clean reached while locked' }
                function Invoke-Build { throw 'TEST: build reached while locked' }
                $timer = [Diagnostics.Stopwatch]::StartNew()
                Assert-Throws "Busy lock defers immediately (clean=$wantClean)" { Invoke-ProjectBuild } 'DEFER: common agent-build.lock*'
                Assert-Case "Busy lock does not wait (clean=$wantClean)" ($timer.Elapsed.TotalSeconds -lt 5)
            }
        }
        Assert-Case 'Lock contention leaves fixture inventory unchanged' ((Get-FixtureSnapshot) -ceq $before)
        & $buildScript -ProjectPath $ProjectPath -EnginePath $EnginePath -Target All -Clean -CleanDdc
        Assert-Case 'Script entry point exits 3 for busy shared lock' ($LASTEXITCODE -eq 3)
        Assert-Case 'Busy entry point leaves fixture unchanged' ((Get-FixtureSnapshot) -ceq $before)
    } finally { $held.Dispose() }
    Assert-LockReleased 'Released lock can be reacquired despite retained lock file'

    foreach ($processName in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe', 'LiveCodingConsole.exe',
        'UnrealBuildTool.exe', 'AutomationTool.exe', 'MSBuild.exe', 'UAT.exe', 'MyProjectServer.exe', 'MyProject-Win64-Shipping.exe')) {
        & {
            function Get-CimInstance { [pscustomobject]@{ Name = $processName; ProcessId = 123; CommandLine = '' } }
            Assert-Throws "Process preflight refuses $processName" { Assert-NoUnrealRunning } 'DEFER: UE/build process active*'
        }
    }
    foreach ($commandLine in @('dotnet UnrealBuildTool.dll', 'dotnet AutomationTool.dll', 'dotnet RunUAT', 'dotnet "C:\Epic Games\UE_5.7\tool.dll"', '', $null)) {
        & {
            function Get-CimInstance { [pscustomobject]@{ Name = 'dotnet.exe'; ProcessId = 456; CommandLine = $commandLine } }
            Assert-Throws "Process preflight refuses relevant/unknown dotnet ($commandLine)" { Assert-NoUnrealRunning } 'DEFER: UE/build process active*'
        }
    }
    & {
        function Get-CimInstance { [pscustomobject]@{ Name = 'dotnet.exe'; ProcessId = 789; CommandLine = 'dotnet unrelated-app.dll' } }
        Assert-NoUnrealRunning
        Assert-Case 'Unrelated dotnet does not block' $true
    }
    & {
        function Get-CimInstance { throw 'inventory unavailable' }
        function Test-Prerequisites { throw 'TEST: prerequisites after unreadable process inventory' }
        Assert-Throws 'Unreadable inventory fails closed' { Invoke-ProjectBuild } 'inventory unavailable'
        Assert-LockReleased 'Inventory failure releases lock in finally'
    }
    & {
        $Clean = $true
        $CleanDdc = $true
        function Get-CimInstance { [pscustomobject]@{ Name = 'dotnet.exe'; ProcessId = 999; CommandLine = 'dotnet UnrealBuildTool.dll' } }
        function Test-Prerequisites { throw 'TEST: prerequisites with active UBT' }
        Assert-Throws 'Active UBT blocks build and clean before prerequisites' { Invoke-ProjectBuild } 'DEFER: UE/build process active*'
        Assert-LockReleased 'Active process deferral releases lock'
    }
    & {
        function Get-CimInstance { }
        function Test-Prerequisites { return $false }
        Assert-Case 'Prerequisite failure returns failure' (-not (Invoke-ProjectBuild))
        Assert-LockReleased 'Prerequisite failure releases lock'
    }
    & {
        $Clean = $true
        $CleanDdc = $true
        $script:events = @()
        function Assert-NoUnrealRunning { $script:events += 'idle' }
        function Test-Prerequisites { $script:events += 'prereq'; return $true }
        function Clean-Build {
            param([bool]$IncludeBuild, [bool]$IncludeDdc)
            Assert-Throws 'Clean runs under shared lock' { Enter-BuildLock $ProjectPath } 'DEFER: common agent-build.lock*'
            Assert-Case 'Both clean flags forwarded' ($IncludeBuild -and $IncludeDdc)
            $script:events += 'clean'
        }
        function Invoke-Build {
            param($TargetName, $Config, $TargetType)
            Assert-Throws "$TargetType build runs under shared lock" { Enter-BuildLock $ProjectPath } 'DEFER: common agent-build.lock*'
            if ($Config -ne 'Development') { throw 'Wrong build config' }
            $script:events += "build:$TargetName"
            return $true
        }
        # Send mock assertions to the host so they cannot pollute the boolean build result.
        function Assert-Case([string]$Label, [bool]$Condition) {
            if (-not $Condition) { throw "FAIL: $Label" }
            $script:checks++
            Write-Host "PASS: $Label"
        }
        Assert-Case 'Mocked All flow succeeds' (Invoke-ProjectBuild)
        Assert-Case 'Idle preflight precedes prerequisites and every build' (($script:events -join ',') -ceq 'idle,prereq,clean,idle,build:MyProjectEditor,idle,build:MyProject,idle,build:MyProjectServer')
        Assert-LockReleased 'Successful mocked flow releases lock'
    }
    foreach ($failure in @('false', 'throw')) {
        & {
            $script:buildCalls = 0
            function Get-CimInstance { }
            function Test-Prerequisites { return $true }
            function Invoke-Build {
                $script:buildCalls++
                if ($failure -eq 'throw') { throw 'fixture build error' }
                return $false
            }
            if ($failure -eq 'throw') {
                Assert-Throws 'Build exception propagated' { Invoke-ProjectBuild } 'fixture build error'
            } else { Assert-Case 'Build failure returns failure' (-not (Invoke-ProjectBuild)) }
            Assert-Case "Build $failure stops later targets" ($script:buildCalls -eq 1)
            Assert-LockReleased "Build $failure releases lock"
        }
    }
    & {
        $Clean = $true
        function Get-CimInstance { }
        function Test-Prerequisites { return $true }
        function Clean-Build { throw 'fixture clean error' }
        Assert-Throws 'Clean exception propagated without real deletion' { Invoke-ProjectBuild } 'fixture clean error'
        Assert-LockReleased 'Clean exception releases lock'
    }
    & {
        $WaitMutex = $true
        $script:launch = $null
        function Start-Process {
            param($FilePath, $ArgumentList, [switch]$Wait, [switch]$PassThru, [switch]$NoNewWindow)
            $script:launch = [pscustomobject]@{ Path = $FilePath; Arguments = $ArgumentList; Wait = $Wait.IsPresent }
            return [pscustomobject]@{ ExitCode = 0 }
        }
        Assert-Case 'Build command can be checked with mocked process only' (Invoke-Build -TargetName MyProject -Config Development -TargetType Game)
        Assert-Case 'EnginePath parameter controls executable' ($script:launch.Path -ceq "$EnginePath\Engine\Build\BatchFiles\Build.bat")
        Assert-Case 'Project argument is quoted and WaitMutex retained' (($script:launch.Arguments -join '|') -ceq ('MyProject|Win64|Development|"-Project={0}"|-WaitMutex' -f $ProjectPath))
        Assert-Case 'Build command waits for process' $script:launch.Wait
    }
    & {
        # Exercise the production Clean-Build body; only its removal primitive is mocked.
        # Real directories exercise ancestor checks and breadth-first descendant traversal.
        function Get-CimInstance { }
        function Remove-Item {
            [CmdletBinding()]
            param([Parameter(Mandatory)][string]$LiteralPath, [switch]$Recurse, [switch]$Force)
            $script:removeCalls += [pscustomobject]@{
                Path = $LiteralPath; Recurse = $Recurse.IsPresent; Force = $Force.IsPresent
                ErrorAction = $PSBoundParameters['ErrorAction']
            }
        }
        Write-Fixture (Join-Path $repo 'Binaries/nested/sentinel.txt') 'preserve-nested'
        $before = Get-FixtureSnapshot
        foreach ($selection in @(
            @{ Build = $false; Ddc = $false; Names = @(); Label = 'neither flag' },
            @{ Build = $true; Ddc = $false; Names = @('Binaries', 'Intermediate'); Label = 'build only' },
            @{ Build = $false; Ddc = $true; Names = @('DerivedDataCache'); Label = 'explicit DDC only' },
            @{ Build = $true; Ddc = $true; Names = @('Binaries', 'Intermediate', 'DerivedDataCache'); Label = 'both flags' }
        )) {
            $script:removeCalls = @()
            Clean-Build -IncludeBuild $selection.Build -IncludeDdc $selection.Ddc
            $expected = @($selection.Names | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $repo $_)) })
            $actual = @($script:removeCalls | ForEach-Object { $_.Path })
            Assert-Case ("Actual Clean-Build selects exactly $($selection.Label)") (($actual -join '|') -ceq ($expected -join '|'))
            Assert-Case ("Actual Clean-Build excludes Saved ($($selection.Label))") (@($actual | Where-Object {
                $_ -eq (Join-Path $repo 'Saved') -or $_ -like ((Join-Path $repo 'Saved') + '\*')
            }).Count -eq 0)
            Assert-Case ("Removal uses absolute literal paths and safe switches ($($selection.Label))") (@($script:removeCalls | Where-Object {
                -not [IO.Path]::IsPathRooted($_.Path) -or -not $_.Recurse -or -not $_.Force -or $_.ErrorAction -ne 'Stop'
            }).Count -eq 0)
        }
        Assert-Case 'Actual clean selection tests leave all fixture files unchanged' ((Get-FixtureSnapshot) -ceq $before)

        foreach ($linkedPath in @((Join-Path $repo 'Binaries'), $repo)) {
            & {
                $script:removeCalls = @()
                function Get-Item {
                    [CmdletBinding()]
                    param([string]$LiteralPath, [switch]$Force)
                    if ($LiteralPath -eq $linkedPath) {
                        return [pscustomobject]@{ Attributes = [IO.FileAttributes]::Directory -bor [IO.FileAttributes]::ReparsePoint }
                    }
                    Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath -Force:$Force
                }
                Assert-Throws "Actual clean rejects linked artifact/ancestor ($linkedPath)" {
                    Clean-Build -IncludeBuild $true -IncludeDdc $true
                } 'Refusing clean through reparse point:*'
                Assert-Case 'Linked artifact/ancestor rejected before any Remove-Item' ($script:removeCalls.Count -eq 0)
            }
        }
        foreach ($directoryLink in @($false, $true)) {
            & {
                $script:removeCalls = @()
                $script:visitedCleanDirectories = @()
                $nested = Join-Path $repo 'Binaries/nested'
                function Get-ChildItem {
                    [CmdletBinding()]
                    param([string]$LiteralPath, [switch]$Force)
                    $script:visitedCleanDirectories += $LiteralPath
                    if ($LiteralPath -eq $nested) {
                        return [pscustomobject]@{
                            FullName = (Join-Path $nested 'controlled-link'); PSIsContainer = $directoryLink
                            Attributes = [IO.FileAttributes]::ReparsePoint
                        }
                    }
                    Microsoft.PowerShell.Management\Get-ChildItem -LiteralPath $LiteralPath -Force:$Force
                }
                Assert-Throws "Actual clean rejects descendant reparse point (directory=$directoryLink)" {
                    Clean-Build -IncludeBuild $true -IncludeDdc $false
                } 'Refusing clean containing reparse point:*'
                Assert-Case 'Descendant link rejected before any Remove-Item' ($script:removeCalls.Count -eq 0)
                Assert-Case 'Actual clean traversed nested directory before rejection' ($nested -in $script:visitedCleanDirectories)
            }
        }
        & {
            $script:removeCalls = @()
            function Get-Item {
                [CmdletBinding()]
                param([string]$LiteralPath, [switch]$Force)
                throw 'fixture metadata read denied'
            }
            Assert-Throws 'Actual clean fails closed on ancestor metadata read failure' {
                Clean-Build -IncludeBuild $true -IncludeDdc $true
            } 'fixture metadata read denied'
            Assert-Case 'Metadata read failure occurs before any Remove-Item' ($script:removeCalls.Count -eq 0)
        }
        & {
            $script:removeCalls = @()
            $nested = Join-Path $repo 'Binaries/nested'
            function Get-ChildItem {
                [CmdletBinding()]
                param([string]$LiteralPath, [switch]$Force)
                if ($LiteralPath -eq $nested) { throw 'fixture descendant enumeration denied' }
                Microsoft.PowerShell.Management\Get-ChildItem -LiteralPath $LiteralPath -Force:$Force
            }
            Assert-Throws 'Actual clean fails closed on descendant filesystem read failure' {
                Clean-Build -IncludeBuild $true -IncludeDdc $true
            } 'fixture descendant enumeration denied'
            Assert-Case 'Descendant read failure occurs before any Remove-Item' ($script:removeCalls.Count -eq 0)
        }
        Assert-Case 'All actual Clean-Build body tests preserve fixture inventory' ((Get-FixtureSnapshot) -ceq $before)
    }
    Assert-Case 'Saved sentinel preserved' ((Get-Content -LiteralPath (Join-Path $repo 'Saved/sentinel.txt') -Raw) -ceq 'preserve-Saved')
    Write-Output "[BUILD_SCRIPT_TEST] PASS checks=$script:checks fixture=$fixture UE=NOT_RUN CLEAN=NOT_RUN CLEAN_BODY=MOCK_REMOVAL_TESTED"
    exit 0
} catch {
    Write-Output "[BUILD_SCRIPT_TEST] FAIL checks=$script:checks fixture=$fixture reason=$($_.Exception.Message)"
    exit 1
}
