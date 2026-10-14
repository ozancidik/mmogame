<#>
.SYNOPSIS
    Build script for MyProject Unreal Engine 5.7 project
.DESCRIPTION
    Builds Editor, Server, and Client targets with proper configuration
.NOTES
    Requires: Visual Studio 2022 with Game Development workload, UE 5.7 installed

    -Clean    deletes ONLY <project>\Binaries and <project>\Intermediate.
    -CleanDdc deletes <project>\DerivedDataCache (explicit, separate flag; combine with -Clean for all three).
    Saved\ is NEVER deleted (character .sav files, Saved\MMOAchievements, MMODaily, MMOMail, LLM budget counter).
    Before deleting anything the script stops with an error if Unreal Editor or UnrealBuildTool is running.

    Project lookup: -ProjectPath if given, else MyProject.uproject next to this script's parent folder
    (Scripts\..), else -DefaultProjectPath (the owner's desktop checkout).
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('Editor', 'Server', 'Client', 'All')]
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

$ProjectPath = Resolve-ProjectFile

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
    # A running editor / build holds files in Binaries/Intermediate/DDC; deleting under it leaves a half state.
    $names = @('UnrealEditor', 'UnrealEditor-Cmd', 'UnrealBuildTool', 'LiveCodingConsole')
    $running = @(Get-Process -ErrorAction Stop | Where-Object { $names -contains $_.ProcessName })
    if ($running.Count -gt 0) {
        $list = ($running | ForEach-Object { "$($_.ProcessName) (pid $($_.Id))" }) -join ', '
        throw "Clean aborted: close these first: $list"
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
        $dir = Join-Path $root $name
        if (Test-Path -LiteralPath $dir -PathType Container) {
            Write-Log "Removing $dir"
            Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction Stop
        }
    }
    Write-Log "Clean completed"
}

# Main execution
Write-Log "=== MyProject Build Script ==="
Write-Log "Target: $Target | Configuration: $Configuration"

if (-not (Test-Prerequisites)) {
    exit 1
}

if ($Clean -or $CleanDdc) {
    Clean-Build -IncludeBuild ([bool]$Clean) -IncludeDdc ([bool]$CleanDdc)
}

$success = $true
$targets = @()

switch ($Target) {
    'Editor' { $targets += @{ Name = 'MyProjectEditor'; Type = 'Editor' } }
    'Server' { $targets += @{ Name = 'MyProjectServer'; Type = 'Server' } }
    'Client' { $targets += @{ Name = 'MyProjectClient'; Type = 'Client' } }
    'All' {
        $targets += @{ Name = 'MyProjectEditor'; Type = 'Editor' }
        $targets += @{ Name = 'MyProjectServer'; Type = 'Server' }
        $targets += @{ Name = 'MyProjectClient'; Type = 'Client' }
    }
}

foreach ($t in $targets) {
    $result = Invoke-Build -TargetName $t.Name -Config $Configuration -TargetType $t.Type
    if (-not $result) {
        $success = $false
        Write-Log "$($t.Type) build failed, stopping" 'ERROR'
        break
    }
}

if ($success) {
    Write-Log "=== ALL BUILDS SUCCEEDED ===" 'SUCCESS'
    exit 0
} else {
    Write-Log "=== BUILD FAILED ===" 'ERROR'
    exit 1
}