# Temporary Git worktree fixtures + AST checks only. No engine/build/network/process launches.
[CmdletBinding()]
param([string]$ActualBuildReceipt)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$wrapper = Join-Path $PSScriptRoot 'InvokeWinterNetSmoke.ps1'
. $wrapper
$checks = 0
$failures = New-Object 'System.Collections.Generic.List[string]'
function Check([bool]$Condition, [string]$Label) {
    $script:checks++
    if (-not $Condition) { $script:failures.Add($Label); Write-Output "FAIL: $Label" }
}
function Reject([scriptblock]$Action, [string]$Pattern, [string]$Label) {
    $message = ''
    try { $null = & $Action } catch { $message = $_.Exception.Message }
    Check ($message -match $Pattern) "$Label ($message)"
}
function Write-FixtureText([string]$Path, [string]$Value) {
    $null = [IO.Directory]::CreateDirectory((Split-Path -Parent $Path))
    [IO.File]::WriteAllText($Path, $Value, (New-Object Text.UTF8Encoding($false)))
}
function Write-FixtureReceipt {
    $script:fixtureWorktree = Get-WinterWorktree $linked
    $inputs = @(Get-WinterInventory $linked)
    $now = [DateTime]::UtcNow
    foreach ($row in $inputs) { (Get-Item -LiteralPath (Join-Path $linked $row.path)).LastWriteTimeUtc = $now.AddMinutes(-3) }
    $dllPath = Join-Path $linked 'Binaries/Win64/UnrealEditor-MyProject.dll'
    Write-FixtureText $dllPath 'FAKE DLL; not a game binary; NEVER EXECUTE'
    (Get-Item -LiteralPath $dllPath).LastWriteTimeUtc = $now.AddMinutes(-1)
    $buildLog = Join-Path $linked 'Saved/S00_01_Build/build.log'
    Write-FixtureText $buildLog ("FIXTURE ONLY $linked`nResult: Succeeded")
    $script:receipt = [ordered]@{ schema = 1; worktree = $fixtureWorktree.root; head = $fixtureWorktree.head;
        target = 'MyProjectEditor Win64 Development'; exitCode = 0;
        startedUtc = $now.AddMinutes(-2).ToString('o'); finishedUtc = $now.AddSeconds(-30).ToString('o');
        inputs = $inputs; dllSha256 = (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash;
        buildLogSha256 = (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash }
    Save-WinterJson $receiptPath $receipt
}

$scratch = Assert-WinterPath (Join-Path $env:TEMP ('winter_wrapper_fixture_' + [guid]::NewGuid().ToString('N')))
$main = Join-Path $scratch 'main'
$linked = Join-Path $scratch 'linked worktree'
$receiptPath = Join-Path $linked 'Saved/S00_01_Build/receipt.json'
$null = [IO.Directory]::CreateDirectory($main)
$null = Invoke-WinterGit $main @('init', '-q')
$null = Invoke-WinterGit $main @('config', 'user.name', 'Winter fixture')
$null = Invoke-WinterGit $main @('config', 'user.email', 'fixture@example.invalid')
Write-FixtureText (Join-Path $main '.gitignore') "Saved/`nBinaries/"
Write-FixtureText (Join-Path $main 'MyProject.uproject') '{}'
Write-FixtureText (Join-Path $main 'Source/MyProject/fixture.cpp') '// fixture'
Write-FixtureText (Join-Path $main 'Config/DefaultEngine.ini') '[fixture]'
$null = Invoke-WinterGit $main @('add', '--', '.gitignore', 'MyProject.uproject', 'Source', 'Config')
$null = Invoke-WinterGit $main @('commit', '-qm', 'temporary fixture only')
$null = Invoke-WinterGit $main @('worktree', 'add', '--detach', '-q', $linked, 'HEAD')
# Copy only the tracked child SCRIPT for contract inspection. No DLL/assets copied from any checkout.
Write-FixtureText (Join-Path $linked 'Scripts/NetSmokeTest.ps1') ([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'NetSmokeTest.ps1')))
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($wrapper, [ref]$tokens, [ref]$errors)
Check ($errors.Count -eq 0) 'wrapper AST parses'
$testTokens = $null; $testErrors = $null
$null = [Management.Automation.Language.Parser]::ParseFile($PSCommandPath, [ref]$testTokens, [ref]$testErrors)
Check ($testErrors.Count -eq 0) 'fixture AST parses'
$commands = @($ast.FindAll({ param($node) $node -is [Management.Automation.Language.CommandAst] }, $true) |
    ForEach-Object { $_.GetCommandName() })
Check (@($commands | Where-Object { $_ -match '^(Start-Process|Stop-Process|Get-Process|taskkill|Copy-Item|Remove-Item|Invoke-Expression)$' }).Count -eq 0) 'no runtime/copy/kill/delete commands'
Check (@($ast.FindAll({ param($node) $node -is [Management.Automation.Language.CommandAst] -and
    $null -eq $node.GetCommandName() }, $true)).Count -eq 1) 'one isolated child invocation, no outer shell'
Check (@($ast.FindAll({ param($node) $node -is [Management.Automation.Language.InvokeMemberExpressionAst] -and
    $node.Member.Extent.Text -match '^(Kill|Start|WaitForExit)$' }, $true)).Count -eq 0) 'no hidden process invocation/cleanup'
Reject { Get-WinterWorktree $main } 'main checkout refused' 'main refused'
$unregistered = Join-Path $scratch 'unregistered'
$null = [IO.Directory]::CreateDirectory($unregistered)
Write-FixtureText (Join-Path $unregistered '.git') ([IO.File]::ReadAllText((Join-Path $linked '.git')))
Reject { Get-WinterWorktree $unregistered } 'root mismatch|Unregistered' 'copied .git cannot register another root'
Reject { Assert-WinterPath 'relative/path' } 'Absolute local' 'relative path refused'
Reject { Assert-WinterPath '\\localhost\share\fixture' } 'Absolute local' 'UNC refused'
$worktree = Get-WinterWorktree $linked
Check ($worktree.root -ieq [IO.Path]::GetFullPath($linked)) 'registered linked worktree accepted'

$held = Open-WinterLock $worktree.common
try { Reject { Open-WinterLock $worktree.common } 'Shared agent-build.lock unavailable' 'exclusive handle blocks contender' }
finally { $held.Dispose() }
$released = Open-WinterLock $worktree.common
$released.Dispose()
Check (Test-Path -LiteralPath (Join-Path $worktree.common 'agent-build.lock')) 'persistent lock file retained after release'

foreach ($name in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe', 'LiveCodingConsole.exe', 'UnrealBuildTool.exe',
    'AutomationTool.exe', 'MSBuild.exe', 'UAT.exe')) {
    Check (@(Get-WinterBusyProcesses @([pscustomobject]@{ Name = $name; CommandLine = ''; ProcessId = 42 })).Count -eq 1) "$name busy"
}
foreach ($line in @('', 'dotnet UnrealBuildTool.dll', 'dotnet AutomationTool.dll', 'dotnet RunUAT', 'C:\Program Files\Epic Games\UE_5.7\dotnet')) {
    Check (@(Get-WinterBusyProcesses @([pscustomobject]@{ Name = 'dotnet.exe'; CommandLine = $line; ProcessId = 43 })).Count -eq 1) "UE/unknown dotnet busy: $line"
}
Check (@(Get-WinterBusyProcesses @([pscustomobject]@{ Name = 'dotnet.exe'; CommandLine = 'dotnet unrelated.dll'; ProcessId = 44 })).Count -eq 0) 'known unrelated dotnet not treated as UE'
Reject { Assert-WinterAssets $linked } 'Required asset missing' 'missing Winter asset refused'
$assetPaths = @('Content/HandPaintedEnvironment/Maps/Winter_scene/Winter_scene.umap',
    'Content/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.uasset',
    'Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.uasset',
    'Content/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.uasset',
    'Content/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene.umap')
foreach ($relative in $assetPaths) {
    $path = Join-Path $linked $relative
    $null = [IO.Directory]::CreateDirectory((Split-Path -Parent $path))
    [IO.File]::WriteAllBytes($path, [byte[]]@(0xC1, 0x83, 0x2A, 0x9E, 0))
}
Assert-WinterAssets $linked
Check $true 'synthetic Unreal headers accepted (not dependency/game verification)'
Write-FixtureText (Join-Path $linked $assetPaths[0]) 'version https://git-lfs.github.com/spec/v1'
Reject { Assert-WinterAssets $linked } 'asset missing/invalid' 'LFS asset pointer refused'
[IO.File]::WriteAllBytes((Join-Path $linked $assetPaths[0]), [byte[]]@(0xC1, 0x83, 0x2A, 0x9E, 0))
Reject { Assert-WinterProvenance $worktree '' } 'receipt required' 'unattested DLL refused'
Write-FixtureReceipt
$valid = Assert-WinterProvenance $fixtureWorktree $receiptPath
Check ($valid.dllSha256 -ceq $receipt.dllSha256) 'synthetic local provenance accepted'
foreach ($field in @('worktree', 'head', 'target', 'exitCode', 'dllSha256', 'buildLogSha256', 'startedUtc', 'finishedUtc')) {
    Write-FixtureReceipt
    switch ($field) {
        'worktree' { $receipt[$field] = $main }
        'head' { $receipt[$field] = '0000000000000000000000000000000000000000' }
        'target' { $receipt[$field] = 'MyProjectServer Win64 Development' }
        'exitCode' { $receipt[$field] = 1 }
        'startedUtc' { $receipt[$field] = [DateTime]::UtcNow.ToString('o') }
        'finishedUtc' { $receipt[$field] = [DateTime]::UtcNow.AddHours(1).ToString('o') }
        default { $receipt[$field] = 'bad hash' }
    }
    Save-WinterJson $receiptPath $receipt
    Reject { Assert-WinterProvenance $fixtureWorktree $receiptPath } 'mismatch|interval|evidence' "receipt $field mismatch refused"
}
Write-FixtureReceipt
Write-FixtureText (Join-Path $linked 'Source/MyProject/fixture.cpp') '// changed since build'
Reject { Assert-WinterProvenance $fixtureWorktree $receiptPath } 'source inventory mismatch' 'changed source refused'
Write-FixtureReceipt
Write-FixtureText (Join-Path $linked 'Source/MyProject/untracked.cpp') '// omitted untracked compilation input'
Reject { Assert-WinterProvenance $fixtureWorktree $receiptPath } 'source inventory mismatch' 'untracked source refused'
Write-FixtureReceipt
(Get-Item -LiteralPath (Join-Path $linked 'Source/MyProject/fixture.cpp')).LastWriteTimeUtc = [DateTime]::UtcNow
Reject { Assert-WinterProvenance $fixtureWorktree $receiptPath } 'older than input' 'timestamp-stale DLL refused'
Write-FixtureReceipt
$inventoryBefore = @(Get-WinterSavedInventory $linked)
Write-FixtureText (Join-Path $linked 'Saved/fixture.sav') 'owner fixture save'
$inventoryAfter = @(Get-WinterSavedInventory $linked)
Check ((ConvertTo-Json -InputObject $inventoryBefore -Compress) -cne (ConvertTo-Json -InputObject $inventoryAfter -Compress)) 'Saved mutation visible to inventory'

# Real temporary junctions: no recursion/delete; fixture is retained for review.
$junction = Join-Path $scratch 'linked junction'
$null = New-Item -ItemType Junction -Path $junction -Target $linked
Reject { Assert-WinterPath (Join-Path $junction 'Saved/new/path') } 'Reparse' 'junction ancestor refused before write'
$savedJunction = Join-Path $linked 'Saved/foreign'
$null = New-Item -ItemType Junction -Path $savedJunction -Target $main
Reject { Get-WinterSavedInventory $linked } 'Reparse' 'nested Saved junction refused before traversal'
# Remove only the junction itself with nonrecursive .NET API; never its target.
[IO.Directory]::Delete($savedJunction)
Check (Test-Path -LiteralPath (Join-Path $main 'MyProject.uproject')) 'junction target untouched'

# Production entry uses real Git/file handles; only CIM inventory is mocked. It must never invoke UE.
function Get-CimInstance { param($ClassName, $ErrorAction) return @() }
Write-FixtureReceipt
$report = Invoke-WinterPreflight $linked $receiptPath
Check ($report.status -ceq 'NOT_RUN' -and $report.runnerExit -eq 3 -and $report.gate -ceq 'child-isolation') 'valid prep reaches explicit NOT_RUN isolation gate'
Check (-not $report.childStarted -and $null -eq $report.childExit -and $null -eq $report.serverLog -and $null -eq $report.clientLog) 'no false child result or invented log paths'
Check ($report.savedPreserved -eq $true) 'pre/post Saved preservation recorded'
Check (($report.requestedChildArguments -join ' ') -ceq '-Map Winter_scene -ServerMode Listen') 'legacy Winter/Listen defaults; child timeout defaults never overridden'
Check (Test-Path -LiteralPath (Join-Path $report.proof 'saved-after.json')) 'post-inventory retained'
Check (Test-Path -LiteralPath (Join-Path $report.proof 'child-contract.json')) 'child contract retained'
$second = Invoke-WinterPreflight $linked $receiptPath
Check ($second.proof -ne $report.proof) 'unique proof directories'
$held = Open-WinterLock $worktree.common
try {
    $busyReport = Invoke-WinterPreflight $linked $receiptPath
    Check ($busyReport.runnerExit -eq 3 -and $busyReport.gate -ceq 'lock' -and -not $busyReport.childStarted) 'held lock refuses entry without runtime'
} finally { $held.Dispose() }
function Get-CimInstance { param($ClassName, $ErrorAction) throw 'Injected inventory unavailable' }
$unavailable = Invoke-WinterPreflight $linked $receiptPath
Check ($unavailable.runnerExit -eq 1 -and $unavailable.reason -match 'inventory unavailable') 'CIM failure refuses entry'
$released = Open-WinterLock $worktree.common
$released.Dispose()
Check $true 'finally releases common handle after failed inventory'
function Get-CimInstance { param($ClassName, $ErrorAction) [pscustomobject]@{ Name = 'UnrealEditor.exe'; ProcessId = 999999; CommandLine = 'fixture only' } }
$busyReport = Invoke-WinterPreflight $linked $receiptPath
Check ($busyReport.runnerExit -eq 3 -and $busyReport.gate -ceq 'process-preflight' -and -not $busyReport.childStarted) 'busy engine refuses entry without touching process'
function Get-CimInstance { param($ClassName, $ErrorAction) return @() }
$refused = Invoke-WinterPreflight $main $receiptPath
Check ($refused.runnerExit -eq 1 -and $null -eq $refused.proof -and $refused.gate -ceq 'worktree') 'main entry refuses before evidence writes'
$contractChild = Join-Path $scratch 'fake contract child.ps1'
Write-FixtureText $contractChild 'param($Map,$UserDir,$LogDirectory,$ServerMode) Write-Output "$Map|$UserDir|$LogDirectory|$ServerMode"; exit 2'
$code = Invoke-WinterChild $contractChild 'fixture user dir' 'fixture logs' $scratch
Check ($code -eq 2 -and [IO.File]::ReadAllText((Join-Path $scratch 'child-output.log')).Contains('Winter_scene|fixture user dir|fixture logs')) 'real invocation helper retains exact args/output and child exit'
$code = Invoke-WinterChild $contractChild 'fixture' 'logs' $scratch 'Summer_scene' 'EditorDedicated'
Check ($code -eq 2 -and [IO.File]::ReadAllText((Join-Path $scratch 'child-output.log')).Contains('Summer_scene|fixture|logs|EditorDedicated')) 'helper propagates selected map/mode without launch overrides'
Write-FixtureText $contractChild 'param($Map,$UserDir,$LogDirectory) Write-Output "partial output"; throw "Injected fake child exception"'
Reject { Invoke-WinterChild $contractChild 'fixture' 'fixture' $scratch } 'Injected fake child' 'real invocation helper propagates child exception'
Check ([IO.File]::ReadAllText((Join-Path $scratch 'child-output.log')).Contains('partial output')) 'partial child output survives exception'
# Execute-mode orchestration uses an explicitly fake child. Real child handle/deadline
# behavior is exercised separately by TestNetSmokeRunner; never run UE with fixture DLLs.
$script:childInvocations = 0
$script:runtimeMode = 'healthy'
$script:lockHeldDuringChild = $false
$script:protectedWriteBlocked = $false
function Invoke-WinterChild([string]$Child, [string]$UserDir, [string]$Logs, [string]$Proof, [string]$Map = 'Winter_scene', [string]$ServerMode = 'Listen') {
    $script:childInvocations++
    try {
        $contender = Open-WinterLock $worktree.common
        $contender.Dispose()
        throw 'Wrapper released lock before child'
    } catch [IO.IOException] { $script:lockHeldDuringChild = $true }
    $null = [IO.Directory]::CreateDirectory($Logs)
    $null = [IO.Directory]::CreateDirectory($UserDir)
    Write-FixtureText (Join-Path $UserDir 'fixture.sav') 'new disposable native output'
    if ($script:runtimeMode -eq 'missing_manifest') { return 0 }
    if ($script:runtimeMode -eq 'protected_write') {
        try { [IO.File]::WriteAllText((Join-Path $linked 'Saved/fixture.sav'), 'must not overwrite') }
        catch [IO.IOException] { $script:protectedWriteBlocked = $true }
    }
    if ($script:runtimeMode -eq 'outside_saved') { Write-FixtureText (Join-Path $linked ('Saved/rogue_' + [guid]::NewGuid().ToString('N'))) 'forbidden' }
    if ($script:runtimeMode -eq 'source_changed') { Write-FixtureText (Join-Path $linked 'Source/MyProject/fixture.cpp') ('// changed ' + [guid]::NewGuid().ToString('N')) }
    $server = Join-Path $Logs 'server.log'; $client = Join-Path $Logs 'client.log'
    $mapPath = "/Game/HandPaintedEnvironment/Maps/$Map/$Map"
    $project = Join-Path (Split-Path -Parent (Split-Path -Parent $UserDir)) 'MyProject.uproject'
    $modeArgs = if ($ServerMode -eq 'EditorDedicated') { @($mapPath, '-server') } else { @("${mapPath}?listen", '-game') }
    $serverArgs = @("`"$project`"") + $modeArgs + @('-port=17779', "-ABSLOG=`"$server`"", "-UserDir=`"$UserDir`"")
    $clientArgs = @("`"$project`"", '127.0.0.1:17779', '-game', "-ABSLOG=`"$client`"", "-UserDir=`"$UserDir`"")
    $cmd = ($serverArgs | Select-Object -Skip 1) -join ' '
    Write-FixtureText $server "LogInit: Command Line: $cmd`nLogNet: listening on port 17779`nLogWorld: Bringing World $mapPath.$Map up for play`nJoin succeeded"
    Write-FixtureText $client 'Welcomed by server'
    $fake = [ordered]@{ schema = 1; map = $Map; port = 17779; userDir = $UserDir;
        serverMode = $ServerMode; route = $(if ($ServerMode -eq 'EditorDedicated') { 'EDITOR_SERVER' } else { 'EDITOR_LISTEN' });
        executable = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe';
        serverArguments = $serverArgs; clientArguments = $clientArgs; serverCommandLine = $cmd;
        serverCommandLineVerified = ($ServerMode -eq 'EditorDedicated'); netModeEvidence = 'NOT_PROVEN';
        serverLog = $server; clientLog = $client; watchdogSeconds = 95; timedOut = $false;
        observed = $true; serverObservedSeconds = 30; clientObservedSeconds = 45; cleanupOk = $true; exitCode = 0;
        readinessReady = $true; readinessMap = $true; readinessPort = $true; readinessElapsedSeconds = 30;
        clientBudgetRemainingSeconds = 65; startupOverheadSeconds = 5; startupFailure = $null;
        processes = @([ordered]@{role = 'server'; pid = 800001; log = $server; stopped = $true; cleanupError = $null},
            [ordered]@{role = 'client'; pid = 800002; log = $client; stopped = $true; cleanupError = $null}) }
    switch ($script:runtimeMode) {
        'watchdog' { $fake.timedOut = $true; $fake.observed = $false; $fake.exitCode = 2 }
        'short_observation' { $fake.clientObservedSeconds = 1 }
        'wrong_pid' { $fake.processes[1].pid = $fake.processes[0].pid }
        'cleanup_failed' { $fake.cleanupOk = $false; $fake.processes[0].stopped = $false }
        'wrong_port' { Write-FixtureText $server ([IO.File]::ReadAllText($server).Replace('17779', '17780')) }
        'late_ready' { $fake.readinessElapsedSeconds = 35; $fake.clientBudgetRemainingSeconds = 60 }
        'too_late_ready' { $fake.readinessElapsedSeconds = 46; $fake.clientBudgetRemainingSeconds = 49 }
        'missing_ready' { $fake.readinessMap = $false; $fake.readinessReady = $false }
        'short_budget' { $fake.clientBudgetRemainingSeconds = 49 }
        'reset_clock_budget' { $fake.readinessElapsedSeconds = 35; $fake.clientBudgetRemainingSeconds = 65 }
        'trace_port' { Write-FixtureText $server ([IO.File]::ReadAllText($server).Replace('LogNet: listening on port 17779', 'LogTrace: listening on port 17779')) }
        'map_prefix' { Write-FixtureText $server ([IO.File]::ReadAllText($server).Replace('Winter_scene.Winter_scene', 'Winter_scene.Winter_scene_extra')) }
        'dedicated_mode_mismatch' { $fake.serverMode = 'Listen' }
        'dedicated_raw_fallback' { $fake.serverArguments[1] += '?listen'; $fake.serverArguments[2] = '-game' }
        'dedicated_native_fallback' {
            $fake.serverCommandLine = $cmd.Replace(' -server ', '?listen -game ')
            Write-FixtureText $server ([IO.File]::ReadAllText($server).Replace($cmd, $fake.serverCommandLine))
        }
        'dedicated_client_server' { $fake.clientArguments[2] = '-server' }
        'dedicated_unproven_cmd' { $fake.serverCommandLineVerified = $false }
    }
    Save-WinterJson (Join-Path $Logs 'processes.json') $fake
    return $fake.exitCode
}
foreach ($mode in @('healthy', 'protected_write', 'watchdog', 'missing_manifest', 'short_observation',
    'wrong_pid', 'cleanup_failed', 'wrong_port', 'outside_saved', 'source_changed',
    'late_ready', 'too_late_ready', 'missing_ready', 'short_budget', 'reset_clock_budget', 'trace_port', 'map_prefix',
    'listen_summer', 'dedicated_winter', 'dedicated_summer', 'dedicated_mode_mismatch', 'dedicated_raw_fallback',
    'dedicated_native_fallback', 'dedicated_client_server', 'dedicated_unproven_cmd')) {
    Write-FixtureReceipt
    $script:runtimeMode = $mode
    $requestedMap = if ($mode.EndsWith('_summer')) { 'Summer_scene' } else { 'Winter_scene' }
    $requestedMode = if ($mode.StartsWith('dedicated_')) { 'EditorDedicated' } else { 'Listen' }
    $runtime = Invoke-WinterPreflight $linked $receiptPath -Execute -Map $requestedMap -ServerMode $requestedMode
    $expected = if ($mode -in @('healthy', 'protected_write', 'late_ready', 'listen_summer', 'dedicated_winter', 'dedicated_summer')) { 0 } elseif ($mode -eq 'watchdog') { 2 } else { 1 }
    Check ($runtime.runnerExit -eq $expected -and $runtime.childStarted) "fake child $mode expected exit=$expected"
    Check ($runtime.status -ceq $(if ($expected -eq 0) { 'PASSED' } else { 'FAILED' })) "$mode status matches full orchestration"
    Check ($runtime.serverLog -and $runtime.clientLog -and $runtime.processManifest) "$mode both log paths/manifest retained even on failure"
    $handle = Open-WinterLock $worktree.common
    $handle.Dispose()
    Check $true "$mode releases common handle after owned child"
}
Reject { Invoke-WinterPreflight $linked $receiptPath -Execute -ServerMode 'Dedicated' } 'ServerMode' 'invalid mode rejected before child'
Reject { Invoke-WinterPreflight $linked $receiptPath -Execute -Map '../Winter_scene' } 'Map' 'invalid map rejected before child'
Check $script:lockHeldDuringChild 'wrapper holds the one common lock through child invocation'
Check $script:protectedWriteBlocked 'read-only Saved handle blocks overwrite attempt'
Check ([IO.File]::ReadAllText((Join-Path $linked 'Saved/fixture.sav')) -ceq 'owner fixture save') 'protected save contents preserved'
$saveItem = Get-Item -LiteralPath (Join-Path $linked 'Saved/fixture.sav')
$saveItem.IsReadOnly = $true
try {
    Write-FixtureReceipt
    $script:runtimeMode = 'healthy'
    $readonly = Invoke-WinterPreflight $linked $receiptPath -Execute
    Check ($readonly.runnerExit -eq 0 -and $readonly.savedPreserved) 'existing read-only save permits owned UserDir output'
} finally { $saveItem.IsReadOnly = $false }
Write-FixtureReceipt
$receipt.dllSha256 = 'stale'
Save-WinterJson $receiptPath $receipt
$callsBefore = $script:childInvocations
$rejected = Invoke-WinterPreflight $linked $receiptPath -Execute
Check ($rejected.runnerExit -eq 1 -and -not $rejected.childStarted -and $script:childInvocations -eq $callsBefore) 'bad provenance cannot reach child'
if ($ActualBuildReceipt) {
    $receiptBeforeHash = (Get-FileHash -LiteralPath $ActualBuildReceipt -Algorithm SHA256).Hash
    $actualIdentity = Get-WinterWorktree (Split-Path -Parent $PSScriptRoot)
    $actualEvidence = Assert-WinterProvenance $actualIdentity $ActualBuildReceipt
    Check ($actualEvidence.inputs.Count -gt 0 -and $actualEvidence.dllSha256) 'actual build receipt accepted by read-only helper'
    Check ((Get-FileHash -LiteralPath $ActualBuildReceipt -Algorithm SHA256).Hash -ceq $receiptBeforeHash) 'actual receipt unchanged by helper'
    Write-Output "Actual provenance helper only: inputs=$($actualEvidence.inputs.Count) DLL=$($actualEvidence.dllSha256); UE=NOT_RUN"
}
Write-Output "Winter wrapper SCRIPT tests: checks=$checks failures=$($failures.Count); network=NOT_RUN (57 prep checks + mocked adapter checks)"
Write-Output "Evidence: $scratch"
if ($failures.Count) { exit 1 }
exit 0
