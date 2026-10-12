<#>
.SYNOPSIS
    Build script for MyProject Unreal Engine 5.7 project
.DESCRIPTION
    Builds Editor, Server, and Client targets with proper configuration
.NOTES
    Requires: Visual Studio 2022 with Game Development workload, UE 5.7 installed
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('Editor', 'Server', 'Client', 'All')]
    [string]$Target = 'Editor',
    
    [Parameter(Mandatory=$false)]
    [ValidateSet('Development', 'Shipping', 'Debug')]
    [string]$Configuration = 'Development',
    
    [Parameter(Mandatory=$false)]
    [string]$ProjectPath = "C:\Users\OZAN CIDIK\Desktop\ozan online\MyProject\MyProject.uproject",
    
    [Parameter(Mandatory=$false)]
    [string]$EnginePath = "C:\Program Files\Epic Games\UE_5.7",
    
    [Parameter(Mandatory=$false)]
    [switch]$Clean,
    
    [Parameter(Mandatory=$false)]
    [switch]$WaitMutex
)

$ErrorActionPreference = 'Stop'

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
    $arguments = @(
        $TargetName,
        'Win64',
        $Config,
        "-Project=$ProjectPath"
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

function Clean-Build {
    Write-Log "Cleaning build artifacts..."
    $dirs = @(
        "$(Split-Path $ProjectPath)\Binaries",
        "$(Split-Path $ProjectPath)\Intermediate",
        "$(Split-Path $ProjectPath)\DerivedDataCache",
        "$(Split-Path $ProjectPath)\Saved"
    )
    
    foreach ($dir in $dirs) {
        if (Test-Path $dir) {
            Write-Log "Removing $dir"
            Remove-Item -Path $dir -Recurse -Force -ErrorAction SilentlyContinue
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

if ($Clean) {
    Clean-Build
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