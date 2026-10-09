# Runner regression: mocked process/logs only; this script never launches Unreal.
param([string]$Runner = (Join-Path $PSScriptRoot 'SoakTest.ps1'))
$ErrorActionPreference = 'Stop'
$scratch = Join-Path $env:TEMP ('mmo_soak_regression_' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $scratch
$child = Join-Path $scratch 'mock.ps1'
$childSource = @'
param([string]$Runner, [string]$CaseName, [string]$CaseRoot)
$ErrorActionPreference = 'Stop'
$env:TEMP = Join-Path $CaseRoot 'space temp'
$null = New-Item -ItemType Directory -Path $env:TEMP -Force
$global:SoakMockCaseName = $CaseName
$global:SoakMockCaseRoot = $CaseRoot
function Start-Process {
    param($FilePath, $ArgumentList, [switch]$PassThru, $WindowStyle)
    [IO.File]::WriteAllText((Join-Path $global:SoakMockCaseRoot 'args.txt'), ($ArgumentList -join [Environment]::NewLine))
    if ($global:SoakMockCaseName -eq 'launch_fail') { throw 'Injected launch failure' }
    $logArg = @($ArgumentList | Where-Object { $_ -like '-ABSLOG=*' })[0]
    # Simulate the native command line losing a path with spaces when unquoted.
    $quoted = $logArg -match '^-ABSLOG="(.+)"$'
    $logPath = if ($quoted) { $Matches[1] } else { $logArg.Substring(8).Split(' ')[0] }
    [IO.File]::WriteAllText((Join-Path $global:SoakMockCaseRoot 'logpath.txt'), $logPath)
    $ready = 'LogWorld: Bringing World /Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene.Summer_scene up for play'
    $block = "Spawner BeginPlay @ V(X=1.00, Y=2.00, Z=3.00)`nSpawnOne: requested=V(X=1,Y=2)`nSpawner after TopUp: live=1"
    $text = "$ready`n$block"
    switch ($global:SoakMockCaseName) {
        'empty' { $text = '' }
        'wrong_map' { $text = $text.Replace('Summer_scene', 'Winter_scene') }
        'no_spawner' { $text = $ready }
        'error' { $text += "`nLogTemp: Error: injected" }
        'fatal' { $text += "`nFatal error: injected" }
        'ensure' { $text += "`nEnsure condition failed: injected" }
        'access' { $text += "`nAccess violation injected" }
        'pocket_missing' { $text = "$ready`nSpawner BeginPlay @ V(X=5,Y=6)`nSpawner after TopUp: live=1" }
        'pocket_truncated' { $text = "$ready`nSpawner BeginPlay @ V(X=1.00, Y=2.00, Z=3.00)`nSpawnOne: requested=V(X=1,Y=2)" }
        'pocket_other_block' { $text = "$ready`nSpawner BeginPlay @ V(X=1.00, Y=2.00, Z=3.00)`nSpawner BeginPlay @ V(X=5,Y=6)`nSpawner after TopUp: live=1" }
        'stuck' { $text += "`nLogCharacterMovement: MMOMob_1 is stuck and failed to move! Location: X=1.00 Y=2.00" }
    }
    if ($global:SoakMockCaseName -ne 'missing' -and $quoted) { [IO.File]::WriteAllText($logPath, $text) }
    if ($global:SoakMockCaseName -eq 'locked' -and $quoted) {
        $global:SoakMockLock = [IO.File]::Open($logPath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    }
    $p = [pscustomobject]@{
        HasExited = ($global:SoakMockCaseName -in @('early_zero','early_one'))
        ExitCode = $(if ($global:SoakMockCaseName -eq 'early_one') { 1 } else { 0 })
        Root = $global:SoakMockCaseRoot
        FailKill = ($global:SoakMockCaseName -eq 'kill_fail')
        FailWait = ($global:SoakMockCaseName -eq 'wait_fail')
    }
    $p | Add-Member ScriptMethod Kill {
        [IO.File]::WriteAllText((Join-Path $this.Root 'kill.txt'), 'owned process only')
        if ($this.FailKill) { throw 'Injected kill failure' }
        $this.HasExited = $true
    }
    $p | Add-Member ScriptMethod WaitForExit {
        param($milliseconds)
        [IO.File]::WriteAllText((Join-Path $this.Root 'wait.txt'), [string]$milliseconds)
        return -not $this.FailWait
    }
    return $p
}
function Start-Sleep {
    param($Seconds, $Milliseconds)
    if ($global:SoakMockCaseName -eq 'sleep_fail') { throw 'Injected sleep failure' }
    if ($Milliseconds) { Microsoft.PowerShell.Utility\Start-Sleep -Milliseconds $Milliseconds }
}
$argsForRunner = @{ Tag = 'regression'; Seconds = 1; Map = 'Summer_scene' }
if ($CaseName -like 'pocket_*' -or $CaseName -eq 'healthy') { $argsForRunner.PocketPrefix = 'V(X=1.00, Y=2.00' }
if ($CaseName -eq 'stuck') { $argsForRunner.Cx = 1; $argsForRunner.Cy = 2; $argsForRunner.Radius = 10 }
if ($CaseName -eq 'bad_tag') { $argsForRunner.Tag = '../bad' }
if ($CaseName -eq 'default_tag') { $argsForRunner.Remove('Tag') }
if ($CaseName -eq 'bad_seconds') { $argsForRunner.Seconds = 0 }
if ($CaseName -eq 'bad_map') { $argsForRunner.Map = 'Missing_scene' }
& $Runner @argsForRunner
exit $LASTEXITCODE
'@
[IO.File]::WriteAllText($child, $childSource, [Text.UTF8Encoding]::new($false))
$cases = @('healthy','healthy_again','default_tag','stuck','missing','empty','locked','wrong_map','no_spawner',
    'error','fatal','ensure','access','early_zero','early_one','launch_fail','sleep_fail',
    'kill_fail','wait_fail','pocket_missing','pocket_truncated','pocket_other_block',
    'bad_tag','bad_seconds','bad_map')
$checks = 0
$failures = 0
function Check([bool]$Condition, [string]$Label) {
    $script:checks++
    if (-not $Condition) { $script:failures++; Write-Output "FAIL: $Label" }
}
foreach ($case in $cases) {
    $caseRoot = Join-Path $scratch $case
    $null = New-Item -ItemType Directory -Path $caseRoot
    # Expected child parameter failures write stderr in Windows PowerShell 5.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & powershell -NoProfile -File $child -Runner $Runner -CaseName $case -CaseRoot $caseRoot 2>&1
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    $output | Out-File -LiteralPath (Join-Path $caseRoot 'output.txt')
    $shouldPass = $case -in @('healthy','healthy_again','default_tag','stuck')
    Check ($code -eq $(if ($shouldPass) { 0 } else { 1 })) "$case exit=$code"
    if ($shouldPass) { Check (($output -join "`n") -match 'GECTI:') "$case positive verdict" }
    if ($case -notin @('launch_fail','bad_tag','bad_seconds','bad_map','early_zero','early_one')) {
        Check (Test-Path -LiteralPath (Join-Path $caseRoot 'kill.txt')) "$case owned cleanup attempted"
    }
    if ($case -notin @('launch_fail','bad_tag','bad_seconds','bad_map','kill_fail')) {
        Check ((Test-Path -LiteralPath (Join-Path $caseRoot 'wait.txt')) -and (Get-Content -LiteralPath (Join-Path $caseRoot 'wait.txt')) -eq '5000') "$case bounded cleanup wait"
    }
    if ($case -in @('early_zero','early_one')) {
        Check (-not (Test-Path -LiteralPath (Join-Path $caseRoot 'kill.txt'))) "$case already exited process not killed"
    }
    if ($case -in @('bad_tag','bad_seconds','bad_map')) {
        Check (-not (Test-Path -LiteralPath (Join-Path $caseRoot 'args.txt'))) "$case rejected before process launch"
    } else {
        $launchArgs = Get-Content -LiteralPath (Join-Path $caseRoot 'args.txt')
        Check (@($launchArgs | Where-Object { $_ -match '^-ABSLOG=".+space temp.+"$' }).Count -eq 1) "$case quoted log argument"
    }
    if ($case -eq 'healthy') { Check (($output -join "`n") -match 'pocketFill=live=1/tries=1') 'correct pocket summary' }
    if ($case -eq 'stuck') { Check (($output -join "`n") -match 'stuck=1 stuckNear=1 errors=0') 'stuck warnings remain measurements' }
}
$firstLog = Get-Content -LiteralPath (Join-Path $scratch 'healthy/logpath.txt')
$nextLog = Get-Content -LiteralPath (Join-Path $scratch 'healthy_again/logpath.txt')
Check ((Split-Path -Leaf $firstLog) -ne (Split-Path -Leaf $nextLog)) 'same tag uses different log filenames'
Write-Output "SoakTest runner: checks=$checks failures=$failures cases=$($cases.Count) (mocked; no Unreal)"
Write-Output "Evidence: $scratch"
if ($failures -gt 0) { exit 1 }
exit 0
