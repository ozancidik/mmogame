<#>
.SYNOPSIS
    Build script for MyProject Unreal Engine 5.7 project
.DESCRIPTION
    Builds defined Editor, Game, Server, and Client targets with proper configuration
.NOTES
    Requires: Visual Studio 2022 with Game Development workload, UE 5.7 installed

    -Clean    deletes ONLY <project>\Binaries and <project>\Intermediate.
    -CleanDdc deletes <project>\DerivedDataCache (explicit, separate flag; combine with -Clean for all three).
    Saved\ is NEVER deleted (character .sav files, Saved\MMOAchievements, MMODaily, MMOMail, LLM budget counter).
    Build and clean share the common Git agent-build.lock and refuse active UE/build processes.
    Undefined targets fail before prerequisites, lock creation, cleaning, or building.
    Exit: 0 success, 1 failure, 3 deferred (shared build resources busy).

    Project lookup: -ProjectPath if given, else MyProject.uproject next to this script's parent folder
    (Scripts\..), else -DefaultProjectPath (the owner's desktop checkout).
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('Editor', 'Game', 'Server', 'Client', 'All')]
    [string]$Target = 'Editor',

    [Parameter(Mandatory=$false)]
    [ValidateSet('Development', 'Shipping', 'Debug')]
    [string]$Configuration = 'Development',

    [Parameter(Mandatory=$false)]
    [string]$ProjectPath = '',

    [Parameter(Mandatory=$false)]
    [string]$DefaultProjectPath = "C:\Users\OZAN CIDIK\Desktop\ozan online\MyProject\MyProject.uproject",

    [Parameter(Mandatory=$false)]
    [string]$EnginePath = "C:\Program Files\Epic Games\UE_5.7",

    [Parameter(Mandatory=$false)]
    [switch]$Clean,

    [Parameter(Mandatory=$false)]
    [switch]$CleanDdc,

    [Parameter(Mandatory=$false)]
    [switch]$WaitMutex
)

$ErrorActionPreference = 'Stop'

# Resolve and validate the project file. A wrong path must fail here, before any build or delete
# (a mistyped -ProjectPath must never make -Clean delete folders next to some other file).
function Resolve-ProjectFile {
    $candidate = $ProjectPath
    if ([string]::IsNullOrWhiteSpace($candidate)) {
        $scriptProject = $null
        if ($PSScriptRoot) {
            $scriptProject = Join-Path (Split-Path -Parent $PSScriptRoot) 'MyProject.uproject'
        }
        if ($scriptProject -and (Test-Path -LiteralPath $scriptProject -PathType Leaf)) {
            $candidate = $scriptProject
        } else {
            $candidate = $DefaultProjectPath
        }
    }

    try {
        $resolved = Resolve-Path -LiteralPath $candidate -ErrorAction Stop
    } catch {
        throw "ProjectPath not found: $candidate"
    }
    $full = $resolved.ProviderPath
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
        throw "ProjectPath is not a file: $full"
    }
    if ([System.IO.Path]::GetExtension($full) -ne '.uproject') {
        throw "ProjectPath must point to a .uproject file: $full"
    }
    $root = Split-Path -Parent $full
    if ([string]::IsNullOrWhiteSpace($root) -or ($root.TrimEnd('\') -match '^[A-Za-z]:$')) {
        throw "Refusing to use a drive root as project folder: $root"
    }
    $sameName = Join-Path $root ([System.IO.Path]::GetFileName($full))
    if (-not (Test-Path -LiteralPath $sameName -PathType Leaf)) {
        throw "Project folder $root does not contain $([System.IO.Path]::GetFileName($full))"
    }
    return $full
}

function Resolve-BuildTargets {
    param([string]$ProjectFile, [string]$RequestedTarget)
    $source = Join-Path (Split-Path -Parent $ProjectFile) 'Source'
    $defined = @()
    if (Test-Path -LiteralPath $source -PathType Container) {
        foreach ($file in @(Get-ChildItem -LiteralPath $source -Filter '*.Target.cs' -File -Recurse | Sort-Object FullName)) {
            # Read literal TargetRules assignments, including targets in module subfolders.
            # Ignore commented examples; never infer a type from a filename suffix.
            $text = Get-Content -LiteralPath $file.FullName -Raw
            $text = [regex]::Replace($text, '(?s)/\*.*?\*/|(?m)//[^\r\n]*', '')
            $types = @([regex]::Matches($text, '\bType\s*=\s*TargetType\.(Editor|Game|Server|Client)\s*;'))
            if ($types.Count -ne 1) { throw "Expected one literal TargetType assignment: $($file.FullName)" }
            $name = $file.Name.Substring(0, $file.Name.Length - '.Target.cs'.Length)
            if ($name -notmatch '^[A-Za-z_][A-Za-z0-9_]*$' -or $name -in @($defined | ForEach-Object { $_.Name })) {
                throw "Invalid or duplicate build target name: $name"
            }
            $defined += [pscustomobject]@{ Name = $name; Type = $types[0].Groups[1].Value }
        }
    }
    $selected = @(foreach ($type in @('Editor', 'Game', 'Server', 'Client')) {
        if ($RequestedTarget -eq 'All' -or $RequestedTarget -eq $type) {
            $defined | Where-Object { $_.Type -eq $type }
        }
    })
    if (-not $selected.Count) { throw "Target '$RequestedTarget' has no target definition under $source" }
    return $selected
}

function Enter-BuildLock {
    param([string]$ProjectFile)
    $root = Split-Path -Parent $ProjectFile
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $commonOutput = @(& git -C $root rev-parse --path-format=absolute --git-common-dir 2>&1)
        $gitExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($gitExit -ne 0 -or $commonOutput.Count -ne 1) { throw 'Cannot resolve shared Git directory.' }
    $common = [IO.Path]::GetFullPath([string]$commonOutput[0])
    # The open handle is the lock. Retain the file, even after releasing the handle.
    try {
        return [IO.File]::Open((Join-Path $common 'agent-build.lock'),
            [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    } catch [IO.IOException] {
        throw 'DEFER: common agent-build.lock is unavailable; no build or clean started.'
    }
}

function Write-Log {
    param([string]$Message, [string]$Level = 'INFO')
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[$timestamp] [$Level] $Message"
}

function Test-Prerequisites {
    Write-Log "Checking prerequisites..."
    
    # Check Visual Studio
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vs = & $vsWhere -latest -requires Microsoft.VisualStudio.Workload.NativeGame -property installationPath
        if ($vs) {
            Write-Log "Visual Studio found: $vs"
        } else {
            Write-Log "Visual Studio with Game Development workload NOT found" 'ERROR'
            return $false
        }
    } else {
        Write-Log "vswhere.exe not found" 'ERROR'
        return $false
    }
    
    # Check Unreal Engine
    if (-not (Test-Path "$EnginePath\Engine\Build\BatchFiles\Build.bat")) {
        Write-Log "Unreal Engine not found at $EnginePath" 'ERROR'
        return $false
    }
    Write-Log "Unreal Engine found: $EnginePath"
    
    # Check project
    if (-not (Test-Path $ProjectPath)) {
        Write-Log "Project not found: $ProjectPath" 'ERROR'
        return $false
    }
    Write-Log "Project found: $ProjectPath"
    
    return $true
}

function Invoke-Build {
    param(
        [string]$TargetName,
        [string]$Config,
        [string]$TargetType
    )
    
    $buildScript = "$EnginePath\Engine\Build\BatchFiles\Build.bat"
    # Start-Process joins -ArgumentList with spaces and does not quote: the project path contains
    # spaces ("OZAN CIDIK", "ozan online"), so the whole -Project=... value is quoted here.
    $arguments = @(
        $TargetName,
        'Win64',
        $Config,
        ('"-Project={0}"' -f $ProjectPath)
    )
    
    if ($WaitMutex) {
        $arguments += '-WaitMutex'
    }
    
    Write-Log "Building $TargetType target: $TargetName ($Config)"
    Write-Log "Command: $buildScript $($arguments -join ' ')"
    
    $process = Start-Process -FilePath $buildScript -ArgumentList $arguments -Wait -PassThru -NoNewWindow
    
    if ($process.ExitCode -eq 0) {
        Write-Log "$TargetType build SUCCEEDED" 'SUCCESS'
        return $true
    } else {
        Write-Log "$TargetType build FAILED (Exit code: $($process.ExitCode))" 'ERROR'
        return $false
    }
}

function Assert-NoUnrealRunning {
    # Fail closed if process inventory cannot be read, including dotnet-hosted UBT/UAT.
    $processes = @(Get-CimInstance -ClassName Win32_Process -ErrorAction Stop)
    $running = @($processes | Where-Object {
        $_.Name -match '^(UnrealEditor.*|LiveCoding.*|UnrealBuildTool.*|AutomationTool.*|MSBuild|UAT|MyProject(?:Server|Client)?(?:-.*)?)\.exe$' -or
        ($_.Name -eq 'dotnet.exe' -and
            ([string]::IsNullOrWhiteSpace($_.CommandLine) -or
             $_.CommandLine -match 'UnrealBuildTool|AutomationTool|RunUAT|Epic Games[\\/]UE_'))
    })
    if ($running.Count -gt 0) {
        $list = ($running | ForEach-Object { "$($_.Name) (pid $($_.ProcessId))" }) -join ', '
        throw "DEFER: UE/build process active; leave it untouched: $list"
    }
}

function Clean-Build {
    param([bool]$IncludeBuild, [bool]$IncludeDdc)

    Assert-NoUnrealRunning

    $root = Split-Path -Parent $ProjectPath
    # Only regenerated build output. Saved\ (character saves, achievements, daily, mail, LLM budget) is never listed.
    $names = @()
    if ($IncludeBuild) {
        $names += 'Binaries', 'Intermediate'
    }
    if ($IncludeDdc) {
        $names += 'DerivedDataCache'
    }

    Write-Log "Cleaning build artifacts in $root ($($names -join ', '))..."
    foreach ($name in $names) {
        $dir = [IO.Path]::GetFullPath((Join-Path $root $name))
        $expectedParent = [IO.Path]::GetFullPath($root).TrimEnd('\', '/')
        if ([IO.Path]::GetDirectoryName($dir) -ne $expectedParent -or
            $name -notin @('Binaries', 'Intermediate', 'DerivedDataCache')) {
            throw "Refusing clean outside allowed project artifacts: $dir"
        }
        if (Test-Path -LiteralPath $dir -PathType Container) {
            # Reject linked paths before any recursive removal, including linked ancestors.
            $cursor = $dir
            while ($cursor) {
                if (((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                    throw "Refusing clean through reparse point: $cursor"
                }
                $cursor = [IO.Path]::GetDirectoryName($cursor)
            }
            $pending = New-Object 'System.Collections.Generic.Queue[string]'
            $pending.Enqueue($dir)
            while ($pending.Count) {
                foreach ($entry in @(Get-ChildItem -LiteralPath $pending.Dequeue() -Force)) {
                    if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                        throw "Refusing clean containing reparse point: $($entry.FullName)"
                    }
                    if ($entry.PSIsContainer) { $pending.Enqueue($entry.FullName) }
                }
            }
            Write-Log "Removing $dir"
            Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction Stop
        }
    }
    Write-Log "Clean completed"
}

function Invoke-ProjectBuild {
    $ProjectPath = Resolve-ProjectFile
    # Validate before prerequisites or any persistent mutation, including lock creation.
    $targets = @(Resolve-BuildTargets -ProjectFile $ProjectPath -RequestedTarget $Target)
    Write-Log "=== MyProject Build Script ==="
    Write-Log "Target: $Target | Configuration: $Configuration"
    $lock = Enter-BuildLock -ProjectFile $ProjectPath
    try {
        Assert-NoUnrealRunning
        if (-not (Test-Prerequisites)) { return $false }
        if ($Clean -or $CleanDdc) {
            Clean-Build -IncludeBuild ([bool]$Clean) -IncludeDdc ([bool]$CleanDdc)
        }
        foreach ($t in $targets) {
            Assert-NoUnrealRunning
            if (-not (Invoke-Build -TargetName $t.Name -Config $Configuration -TargetType $t.Type)) {
                Write-Log "$($t.Type) build failed, stopping" 'ERROR'
                return $false
            }
        }
        Write-Log "=== ALL BUILDS SUCCEEDED ===" 'SUCCESS'
        return $true
    } finally { $lock.Dispose() }
}

# Dot-sourcing exposes helpers for isolated tests without starting a build or clean.
if ($MyInvocation.InvocationName -ne '.') {
    try {
        if (Invoke-ProjectBuild) { exit 0 }
        exit 1
    } catch {
        Write-Log $_.Exception.Message 'ERROR'
        if ($_.Exception.Message -like 'DEFER:*') { exit 3 }
        exit 1
    }
}
