# Runner regression: mocked processes, UDP endpoints and fresh logs only; never launches Unreal.
# -Runner also accepts the historical runner (without ServerSeconds) to reproduce false positives.
param([string]$Runner = (Join-Path $PSScriptRoot 'NetSmokeTest.ps1'), [string[]]$Case)
$ErrorActionPreference = 'Stop'
if ($PSBoundParameters.ContainsKey('Case') -and (@($Case).Count -eq 0 -or
    @($Case | Where-Object { [string]::IsNullOrWhiteSpace($_) }).Count)) { throw 'Case selection must not be empty.' }
$Runner = (Resolve-Path -LiteralPath $Runner).Path
$scratch = Join-Path $env:TEMP ('mmo_net_regression_' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $scratch
$child = Join-Path $scratch 'mock.ps1'
$childSource = @'
param([string]$Runner, [string]$CaseName, [string]$CaseRoot, [string]$IsolatedRunner, [string]$IsolatedRoot)
$ErrorActionPreference = 'Stop'
# All cases share a TEMP containing spaces, so filename uniqueness cannot come from directories.
$env:TEMP = Join-Path (Split-Path -Parent $CaseRoot) 'space temp'
$null = New-Item -ItemType Directory -Path $env:TEMP -Force
$env:TEMP = [IO.Path]::GetFullPath($env:TEMP) # Normalize Windows 8.3 TEMP aliases before comparing canonical child paths.
$global:NetMockCase = $CaseName -replace '^isolated_', ''
if ($global:NetMockCase.StartsWith('dedicated_')) { $global:NetMockCase = $global:NetMockCase.Substring(10) }
if ($global:NetMockCase.StartsWith('diag_')) { $global:NetMockCase = $global:NetMockCase.Substring(5) }
$global:NetMockRoot = $CaseRoot
$global:NetMockProcesses = @{}
$global:NetMockClientAttempted = $false
$global:NetMockSleeps = @{ server = 0; client = 0 }
$global:NetMockLocks = @()
function Write-MockEvidence([string]$Name, [string]$Value) {
    [IO.File]::AppendAllText((Join-Path $global:NetMockRoot $Name), $Value + [Environment]::NewLine)
}
function Get-NetUDPEndpoint {
    [CmdletBinding()]
    param()
    Write-MockEvidence 'udp.txt' 'mock endpoint query'
    if ($global:NetMockCase -eq 'udp_unknown') { throw 'Injected UDP inventory failure' }
    # This unrelated process must never be killed, even when its port blocks the run.
    [pscustomobject]@{ LocalPort = 17780; OwningProcess = 999998 }
    if ($global:NetMockCase -eq 'occupied') {
        [pscustomobject]@{ LocalPort = 17779; OwningProcess = 999999 }
    }
}
function Stop-Process {
    [CmdletBinding()]
    param($Id, $Name, $InputObject, [switch]$Force)
    Write-MockEvidence 'foreign-cleanup.txt' 'Stop-Process called'
    throw 'Cleanup must use only the process objects returned by this run.'
}
function Get-Process {
    [CmdletBinding()]
    param($Id, $Name)
    Write-MockEvidence 'foreign-cleanup.txt' 'Get-Process called'
    throw 'A process-name or PID lookup is outside owned process cleanup.'
}
function taskkill {
    Write-MockEvidence 'foreign-cleanup.txt' 'taskkill called'
    throw 'Cleanup must use only owned process objects.'
}
function Start-Process {
    [CmdletBinding()]
    param($FilePath, $ArgumentList, [switch]$PassThru, $WindowStyle)
    $role = if (@($ArgumentList | Where-Object { $_ -like '*?listen' }).Count -or '-server' -in $ArgumentList) { 'server' } else { 'client' }
    Write-MockEvidence "$role-args.txt" ($ArgumentList -join [Environment]::NewLine)
    Write-MockEvidence "$role-exe.txt" $FilePath
    Write-MockEvidence 'launches.txt' $role
    if ($role -eq 'server' -and $global:NetMockCase -eq 'server_launch_fail') {
        throw 'Injected server launch failure'
    }
    if ($role -eq 'client') {
        $global:NetMockClientAttempted = $true
        if ($global:NetMockCase -eq 'client_launch_fail') { throw 'Injected client launch failure' }
    }
    $logArgs = @($ArgumentList | Where-Object { $_ -like '-ABSLOG=*' })
    if ($logArgs.Count -ne 1 -or $logArgs[0] -notmatch '^-ABSLOG="(.+)"$') {
        throw 'Mock native launch requires one quoted ABSLOG argument.'
    }
    $logPath = $Matches[1]
    Write-MockEvidence "$role-logpath.txt" $logPath
    if ($CaseName.StartsWith('isolated_')) {
        $published = Get-Content -LiteralPath (Join-Path (Split-Path -Parent $logPath) 'processes.json') -Raw | ConvertFrom-Json
        if (-not $published.serverLog -or -not $published.clientLog) { throw 'Both log paths must be published before native launch.' }
        Write-MockEvidence 'prelaunch-manifest.txt' $role
    }
    if (-not [IO.Path]::IsPathRooted($logPath) -or
        -not $logPath.StartsWith($env:TEMP + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Mock log path must be absolute and inside the case TEMP.'
    }
    if ([IO.File]::Exists($logPath)) { throw 'Runner reused a log from an earlier launch.' }
    $text = if ($role -eq 'server') {
        "LogNet: GameNetDriver listening on port 17779`nLogWorld: Bringing World /Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene.Summer_scene up for play`nLogNet: Join succeeded: MockPlayer"
    } else {
        'LogNet: Welcomed by server (Level: /Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene)'
    }
    if ($global:NetMockCase -eq 'healthy_winter' -or ($CaseName.StartsWith('isolated_') -and -not $CaseName.EndsWith('_summer'))) { $text = $text.Replace('Summer_scene', 'Winter_scene') }
    if ($role -eq 'server') {
        switch ($global:NetMockCase) {
            'empty_server' { $text = '' }
            'wrong_port' { $text = $text.Replace('17779', '17780') }
            'port_prefix' { $text = $text.Replace('17779', '177790') }
            'missing_join' { $text = $text.Replace("`nLogNet: Join succeeded: MockPlayer", '') }
            'wrong_map' { $text = if ($CaseName.StartsWith('isolated_')) { $text.Replace('Winter_scene', 'Summer_scene') } else { $text.Replace('Summer_scene', 'Winter_scene') } }
            'missing_readiness' { $text = "LogNet: GameNetDriver listening on port 17779`nLogNet: Join succeeded: MockPlayer" }
        }
        if ($global:NetMockCase -like 'readiness_*') {
            $global:NetMockReadyText = $text
            $global:NetMockServerLog = $logPath
            $global:NetMockStartupClock = [Diagnostics.Stopwatch]::StartNew()
            switch ($global:NetMockCase) {
                'readiness_only_map' { $text = ($text -split "`n" | Where-Object { $_ -notmatch 'LogNet: GameNetDriver' }) -join "`n" }
                'readiness_only_port' { $text = ($text -split "`n" | Where-Object { $_ -notmatch 'LogWorld:' }) -join "`n" }
                'readiness_wrong_port' { $text = $text.Replace('17779', '17780') }
                'readiness_port_prefix' { $text = $text.Replace('17779', '177790') }
                'readiness_map_prefix' { $text = $text.Replace('Winter_scene.Winter_scene', 'Winter_scene.Winter_scene_extra') }
                'readiness_trace' { $text = $text.Replace('LogNet: GameNetDriver listening on port 17779', 'LogTrace: Store listening on port 1985') }
                'readiness_budget' { }
                default { $text = '' }
            }
        }
    } else {
        switch ($global:NetMockCase) {
            'empty_client' { $text = '' }
            'missing_welcome' { $text = 'LogNet: Client connecting' }
        }
    }
    if ($role -eq 'server' -and $CaseName.StartsWith('isolated_dedicated_') -and $global:NetMockCase -ne 'missing_cmd') {
        $nativeCmd = ($ArgumentList | Select-Object -Skip 1) -join ' '
        if ($global:NetMockCase -eq 'fallback') { $nativeCmd = $nativeCmd.Replace(' -server ', '?listen -game ') }
        if ($global:NetMockCase -eq 'wrong_cmd_map') { $nativeCmd = $nativeCmd.Replace('Winter_scene', 'Summer_scene') }
        if ($global:NetMockCase -eq 'missing_logcmd') { $nativeCmd = $nativeCmd.Replace(' -LogCmds="LogShaders Log"', '') }
        if ($global:NetMockCase -eq 'wrong_logcmd') { $nativeCmd = $nativeCmd.Replace('-LogCmds="LogShaders Log"', '-LogCmds="LogShaders Error"') }
        if ($global:NetMockCase -eq 'duplicate_logcmd') { $nativeCmd += ' -LogCmds="LogShaders Log"' }
        $text += "`nLogInit: Command Line: $nativeCmd"
    }
    foreach ($kind in @('error', 'fatal', 'ensure', 'access')) {
        if ($global:NetMockCase -eq "${role}_$kind") {
            $line = switch ($kind) {
                'error' { 'LogTemp: Error: injected' }
                'fatal' { 'Fatal error: injected' }
                'ensure' { 'Ensure condition failed: injected' }
                'access' { 'Access violation injected' }
            }
            $text += "`n$line"
        }
    }
    if ($global:NetMockCase -ne "missing_$role") { [IO.File]::WriteAllText($logPath, $text) }
    if ($global:NetMockCase -eq "unreadable_$role") {
        $global:NetMockLocks += [IO.File]::Open($logPath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        Write-MockEvidence "$role-locked.txt" 'exclusive fixture lock'
    }
    $p = [pscustomobject]@{
        Id = $(if ($role -eq 'server') { 900001 } else { 900002 })
        Handle = [IntPtr]::Zero
        HasExited = $false
        ExitCode = 0
        Role = $role
        Root = $global:NetMockRoot
        FailKill = ($global:NetMockCase -eq "${role}_kill_fail")
        FailWait = $(if ($global:NetMockCase -eq "${role}_wait_false") { 'false' }
            elseif ($global:NetMockCase -eq "${role}_wait_throw") { 'throw' } else { '' })
    }
    $p | Add-Member ScriptMethod Kill {
        [IO.File]::AppendAllText((Join-Path $this.Root 'cleanup-order.txt'), "$($this.Role):Kill`n")
        [IO.File]::AppendAllText((Join-Path $this.Root ($this.Role + '-kill.txt')), "owned $($this.Role)`n")
        if ($this.HasExited) { throw 'Already exited process must not be killed.' }
        if ($this.FailKill) { throw "Injected $($this.Role) kill failure" }
        $this.HasExited = $true
    }
    $p | Add-Member ScriptMethod WaitForExit {
        param($milliseconds)
        [IO.File]::AppendAllText((Join-Path $this.Root 'cleanup-order.txt'), "$($this.Role):WaitForExit`n")
        [IO.File]::AppendAllText((Join-Path $this.Root ($this.Role + '-wait.txt')), ([string]$milliseconds + "`n"))
        if ($null -eq $milliseconds -or $milliseconds -ne 5000) { throw 'Cleanup wait must be bounded to 5000 ms.' }
        if ($this.FailWait -eq 'throw') { throw "Injected $($this.Role) wait failure" }
        return ($this.FailWait -ne 'false')
    }
    $global:NetMockProcesses[$role] = $p
    Write-MockEvidence "$role-created.txt" 'owned mock process'
    return $p
}
function Start-Sleep {
    [CmdletBinding()]
    param($Seconds, $Milliseconds)
    Write-MockEvidence 'sleeps.txt' "seconds=$Seconds milliseconds=$Milliseconds"
    $phase = if ($global:NetMockClientAttempted) { 'client' } else { 'server' }
    $global:NetMockSleeps[$phase]++
    if ($global:NetMockCase -eq 'server_sleep_fail' -and -not $global:NetMockClientAttempted) {
        throw 'Injected server observation sleep failure'
    }
    if ($global:NetMockCase -eq 'sleep_fail' -and $global:NetMockClientAttempted) {
        throw 'Injected client observation sleep failure'
    }
    # Exit only after logs contain successful join/welcome evidence; exit 0 is still a failed observation.
    $exitRole = $null
    if (-not $global:NetMockClientAttempted -and $global:NetMockCase -match '^server_early_(zero|one)$') {
        $exitRole = 'server'
    } elseif ($global:NetMockClientAttempted -and $global:NetMockCase -match '^(client|server)_after_join_(zero|one)$') {
        $exitRole = $Matches[1]
    }
    if ($exitRole -and $global:NetMockProcesses.ContainsKey($exitRole) -and
        ($Seconds -gt 0 -or $global:NetMockSleeps[$phase] -eq 2)) {
        $p = $global:NetMockProcesses[$exitRole]
        $p.HasExited = $true
        $p.ExitCode = if ($global:NetMockCase -like '*_one') { 1 } else { 0 }
        Write-MockEvidence "$exitRole-natural-exit.txt" ([string]$p.ExitCode)
        Write-MockEvidence 'observation-exit.txt' "phase=$phase sleep=$($global:NetMockSleeps[$phase])"
    }
    # Historical fixed sleeps are skipped; updated stopwatch loops advance in real bounded increments.
    if ($Milliseconds -gt 0) {
        Microsoft.PowerShell.Utility\Start-Sleep -Milliseconds ([math]::Min([int]$Milliseconds, 250))
    }
    if ($phase -eq 'server' -and $global:NetMockCase -in @('readiness_late', 'readiness_boundary') -and
        $global:NetMockStartupClock.Elapsed.TotalSeconds -ge 2) {
        [IO.File]::WriteAllText($global:NetMockServerLog, $global:NetMockReadyText)
        Write-MockEvidence 'ready-published.txt' 'both markers after 2s; minimum server observation 1s'
    }
    if ($phase -eq 'server' -and $global:NetMockCase -eq 'readiness_early_exit' -and
        $global:NetMockStartupClock.Elapsed.TotalSeconds -ge 1.25) {
        $global:NetMockProcesses.server.HasExited = $true
        $global:NetMockProcesses.server.ExitCode = 0
        Write-MockEvidence 'server-natural-exit.txt' '0'
    }
    if ($phase -eq 'client' -and $global:NetMockCase -eq 'timeout_client') {
        # Inject a stalled total clock without a long fixture sleep or production timing seam.
        $clock | Add-Member -MemberType ScriptProperty -Name Elapsed -Value { [TimeSpan]::FromSeconds(4001) } -Force
        Write-MockEvidence 'watchdog-injected.txt' 'total clock stall after client launch'
    }
}
$argsForRunner = @{ Map = 'Summer_scene'; ClientSeconds = 1; Port = 17779 }
# Do not let the newly added parameter prevent the old runner's false positives from being reproduced.
$hasServerSeconds = (Get-Command -Name $Runner).Parameters.ContainsKey('ServerSeconds')
Write-MockEvidence 'runner-capabilities.txt' "ServerSeconds=$hasServerSeconds"
if ($hasServerSeconds) { $argsForRunner.ServerSeconds = 1 }
if ((Get-Command -Name $Runner).Parameters.ContainsKey('WatchdogSeconds')) { $argsForRunner.WatchdogSeconds = 8 }
if ($CaseName.StartsWith('isolated_')) {
    $Runner = $IsolatedRunner
    $argsForRunner.Map = 'Winter_scene'
    if ($CaseName.EndsWith('_summer')) { $argsForRunner.Map = 'Summer_scene' }
    if ($CaseName.StartsWith('isolated_dedicated_')) { $argsForRunner.ServerMode = 'EditorDedicated' }
    if ($CaseName.StartsWith('isolated_dedicated_diag_')) { $argsForRunner.ShaderStartupDiagnostics = $true }
    $argsForRunner.UserDir = Join-Path $IsolatedRoot ('Saved/WinterNet_' + [guid]::NewGuid().ToString('N'))
    $argsForRunner.LogDirectory = Join-Path $env:TEMP ($CaseName + '_' + [guid]::NewGuid().ToString('N') + '/child logs')
    switch ($global:NetMockCase) {
        'timeout_server' { $argsForRunner.ServerSeconds = 2; $argsForRunner.WatchdogSeconds = 1 }
        'timeout_client' { $argsForRunner.WatchdogSeconds = 8 }
        'readiness_late' { $argsForRunner.WatchdogSeconds = 9 }
        'readiness_budget' { $argsForRunner.WatchdogSeconds = 7 }
        'readiness_trace' { $argsForRunner.Port = 1985 }
        'reused_user' { $null = [IO.Directory]::CreateDirectory($argsForRunner.UserDir) }
        'reused_log' { $null = [IO.Directory]::CreateDirectory($argsForRunner.LogDirectory) }
        'foreign_user' { $argsForRunner.UserDir = Join-Path $env:TEMP ('WinterNet_' + [guid]::NewGuid().ToString('N')) }
        'foreign_log' { $argsForRunner.LogDirectory = Join-Path $IsolatedRoot 'Saved/forbidden_logs' }
        'unpaired' { $argsForRunner.Remove('LogDirectory') }
        'reparse_log' {
            $link = Join-Path $CaseRoot 'junction'
            $null = New-Item -ItemType Junction -Path $link -Target $env:TEMP
            $argsForRunner.LogDirectory = Join-Path $link 'foreign_log'
        }
    }
    Write-MockEvidence 'requested-userdir.txt' $argsForRunner.UserDir
}
switch ($CaseName) {
    'healthy_winter' { $argsForRunner.Map = 'Winter_scene' }
    'bad_map' { $argsForRunner.Map = 'Missing_scene' }
    'bad_map_path' { $argsForRunner.Map = '../Summer_scene' }
    'bad_map_empty' { $argsForRunner.Map = '' }
    'bad_server_zero' { $argsForRunner.ServerSeconds = 0 }
    'bad_server_negative' { $argsForRunner.ServerSeconds = -1 }
    'bad_server_high' { $argsForRunner.ServerSeconds = 301 }
    'bad_client_zero' { $argsForRunner.ClientSeconds = 0 }
    'bad_client_negative' { $argsForRunner.ClientSeconds = -1 }
    'bad_client_high' { $argsForRunner.ClientSeconds = 3601 }
    'bad_port_zero' { $argsForRunner.Port = 0 }
    'bad_port_negative' { $argsForRunner.Port = -1 }
    'bad_port_high' { $argsForRunner.Port = 65536 }
    'bad_server_mode' { $argsForRunner.ServerMode = 'Dedicated' }
    'bad_diag_listen' { $argsForRunner.ShaderStartupDiagnostics = $true }
}
& $Runner @argsForRunner
exit $LASTEXITCODE
'@
[IO.File]::WriteAllText($child, $childSource, [Text.UTF8Encoding]::new($false))
$cases = @('healthy', 'healthy_again', 'healthy_winter',
    'server_early_zero', 'server_early_one', 'client_after_join_zero', 'client_after_join_one',
    'server_after_join_zero', 'server_after_join_one',
    'missing_server', 'empty_server', 'wrong_port', 'port_prefix', 'wrong_map', 'missing_readiness',
    'missing_client', 'empty_client', 'unreadable_server', 'unreadable_client',
    'missing_join', 'missing_welcome',
    'server_error', 'client_error', 'server_fatal', 'client_fatal', 'server_ensure', 'client_ensure',
    'server_access', 'client_access', 'server_launch_fail', 'client_launch_fail', 'server_sleep_fail', 'sleep_fail',
    'server_kill_fail', 'client_kill_fail', 'server_wait_false', 'client_wait_false',
    'server_wait_throw', 'client_wait_throw', 'occupied',
    'bad_map', 'bad_map_path', 'bad_map_empty', 'bad_server_zero', 'bad_server_negative', 'bad_server_high',
    'bad_client_zero', 'bad_client_negative', 'bad_client_high', 'bad_port_zero', 'bad_port_negative', 'bad_port_high')
$isolatedRoot = ''; $isolatedRunner = ''
if ((Get-Command -Name $Runner).Parameters.ContainsKey('UserDir')) {
    # Separate temporary fixture registration; never edit project Git metadata or copy game assets/DLLs.
    $fixtureMain = Join-Path $scratch 'fixture-main'
    $isolatedRoot = Join-Path $scratch 'isolated worktree'
    $null = [IO.Directory]::CreateDirectory($fixtureMain)
    function Fixture-Git([string]$Root, [string[]]$GitArgs) {
        $null = & git -C $Root @GitArgs
        if ($LASTEXITCODE) { throw 'Fixture Git command failed.' }
    }
    Fixture-Git $fixtureMain @('init', '-q')
    Fixture-Git $fixtureMain @('config', 'user.name', 'Smoke fixture')
    Fixture-Git $fixtureMain @('config', 'user.email', 'fixture@example.invalid')
    [IO.File]::WriteAllText((Join-Path $fixtureMain 'MyProject.uproject'), '{}')
    Fixture-Git $fixtureMain @('add', '--', 'MyProject.uproject')
    Fixture-Git $fixtureMain @('commit', '-qm', 'temporary fixture only')
    Fixture-Git $fixtureMain @('worktree', 'add', '--detach', '-q', $isolatedRoot, 'HEAD')
    $null = [IO.Directory]::CreateDirectory((Join-Path $isolatedRoot 'Scripts'))
    $isolatedRunner = Join-Path $isolatedRoot 'Scripts/NetSmokeTest.ps1'
    [IO.File]::WriteAllText($isolatedRunner, [IO.File]::ReadAllText($Runner), (New-Object Text.UTF8Encoding($false)))
    $cases += @('isolated_healthy', 'isolated_timeout_server', 'isolated_timeout_client',
        'isolated_client_after_join_zero', 'isolated_wrong_port', 'isolated_wrong_map', 'isolated_client_kill_fail',
        'isolated_reused_user', 'isolated_reused_log', 'isolated_foreign_user', 'isolated_foreign_log',
        'isolated_unpaired', 'isolated_reparse_log', 'isolated_udp_unknown')
    $cases += @('isolated_readiness_late', 'isolated_readiness_never', 'isolated_readiness_only_map',
        'isolated_readiness_only_port', 'isolated_readiness_wrong_port', 'isolated_readiness_port_prefix',
        'isolated_readiness_map_prefix', 'isolated_readiness_trace', 'isolated_readiness_early_exit',
        'isolated_readiness_budget', 'isolated_readiness_boundary')
    if ((Get-Command -Name $Runner).Parameters.ContainsKey('ServerMode')) {
        $cases += @('bad_server_mode', 'isolated_listen_summer', 'isolated_dedicated_winter', 'isolated_dedicated_summer',
            'isolated_dedicated_fallback', 'isolated_dedicated_missing_cmd', 'isolated_dedicated_wrong_cmd_map',
            'isolated_dedicated_missing_join', 'isolated_dedicated_client_kill_fail')
        if ((Get-Command -Name $Runner).Parameters.ContainsKey('ShaderStartupDiagnostics')) {
            $cases += @('bad_diag_listen', 'isolated_dedicated_diag_summer',
                'isolated_dedicated_diag_missing_logcmd', 'isolated_dedicated_diag_wrong_logcmd',
                'isolated_dedicated_diag_duplicate_logcmd', 'isolated_dedicated_diag_server_sleep_fail',
                'isolated_dedicated_diag_readiness_never')
        }
    }
}
$availableCases = $cases.Count
$selection = 'FULL'
if ($PSBoundParameters.ContainsKey('Case')) {
    foreach ($selected in $Case) {
        if ($selected -cnotin $cases) { throw "Unknown case selection: $selected" }
    }
    if (@($Case | Select-Object -Unique).Count -ne $Case.Count) { throw 'Duplicate case selection refused.' }
    $cases = @($Case)
    $selection = 'FILTERED'
}
$checks = 0
$failures = 0
function Check([bool]$Condition, [string]$Label) {
    $script:checks++
    if (-not $Condition) { $script:failures++; Write-Output "FAIL: $Label" }
}
function EvidenceLines([string]$Root, [string]$Name) {
    $path = Join-Path $Root $Name
    if (Test-Path -LiteralPath $path) { Get-Content -LiteralPath $path }
}
foreach ($case in $cases) {
    $caseRoot = Join-Path $scratch $case
    $null = New-Item -ItemType Directory -Path $caseRoot
    $previousPreference = $ErrorActionPreference
    try {
        # Expected parameter/exception failures write stderr under Windows PowerShell 5.
        $ErrorActionPreference = 'Continue'
        $output = & powershell -NoProfile -File $child -Runner $Runner -CaseName $case -CaseRoot $caseRoot -IsolatedRunner $isolatedRunner -IsolatedRoot $isolatedRoot 2>&1
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    $output | Out-File -LiteralPath (Join-Path $caseRoot 'output.txt')
    [IO.File]::WriteAllText((Join-Path $caseRoot 'exit.txt'), [string]$code)
    $shouldPass = $case -in @('healthy', 'healthy_again', 'healthy_winter', 'isolated_healthy', 'isolated_readiness_late',
        'isolated_listen_summer', 'isolated_dedicated_winter', 'isolated_dedicated_summer', 'isolated_dedicated_diag_summer')
    $expectedCode = if ($shouldPass) { 0 } elseif ($case -like 'isolated_timeout_*') { 2 } else { 1 }
    Check ($code -eq $expectedCode) "$case exit=$code"
    Check (-not (Test-Path -LiteralPath (Join-Path $caseRoot 'foreign-cleanup.txt'))) "$case only owned process cleanup"
    $launches = @(EvidenceLines $caseRoot 'launches.txt')
    $rejectBeforeLaunch = $case -eq 'occupied' -or $case -like 'bad_*' -or
        $case -in @('isolated_reused_user', 'isolated_reused_log', 'isolated_foreign_user', 'isolated_foreign_log',
            'isolated_unpaired', 'isolated_reparse_log', 'isolated_udp_unknown')
    if ($rejectBeforeLaunch) {
        Check ($launches.Count -eq 0) "$case rejected before process launch"
        if ($case -eq 'bad_diag_listen') { Check (($output -join "`n") -match 'requires EditorDedicated') "$case explicit diagnostic mode rejection" }
    } else {
        Check (@($launches | Where-Object { $_ -eq 'server' }).Count -eq 1) "$case one server launch"
    }
    if ($case -in @('wrong_port', 'port_prefix', 'wrong_map', 'missing_readiness', 'missing_server', 'empty_server', 'unreadable_server',
        'server_early_zero', 'server_early_one', 'server_launch_fail', 'server_sleep_fail')) {
        Check (@($launches | Where-Object { $_ -eq 'client' }).Count -eq 0) "$case client not launched on invalid/exited server"
    }
    if ($case -eq 'server_launch_fail') {
        Check (($output -join "`n").Contains('Injected server launch failure')) "$case launch exception reported"
        Check (-not (Test-Path -LiteralPath (Join-Path $caseRoot 'server-created.txt'))) "$case no process acquired after launch exception"
    }
    if ($case -in @('healthy', 'healthy_again', 'healthy_winter', 'client_after_join_zero',
        'client_after_join_one', 'server_after_join_zero', 'server_after_join_one',
        'missing_client', 'empty_client', 'unreadable_client', 'missing_join', 'missing_welcome', 'client_launch_fail', 'sleep_fail',
        'server_kill_fail', 'client_kill_fail', 'server_wait_false', 'client_wait_false', 'server_wait_throw', 'client_wait_throw')) {
        Check (@($launches | Where-Object { $_ -eq 'client' }).Count -eq 1) "$case client launch exercised"
    }
    foreach ($role in @('server', 'client')) {
        $created = Test-Path -LiteralPath (Join-Path $caseRoot "$role-created.txt")
        $kills = @(EvidenceLines $caseRoot "$role-kill.txt")
        $waits = @(EvidenceLines $caseRoot "$role-wait.txt")
        if ($created) {
            $exitedNaturally = Test-Path -LiteralPath (Join-Path $caseRoot "$role-natural-exit.txt")
            Check ($kills.Count -eq $(if ($exitedNaturally) { 0 } else { 1 })) "$case $role owned cleanup / exited process not killed"
            if ($case -eq "${role}_kill_fail" -or $case -eq "isolated_${role}_kill_fail" -or $case -eq "isolated_dedicated_${role}_kill_fail") {
                # A thrown Kill may skip this object's wait; the other object must still be cleaned up.
                Check ($waits.Count -le 1) "$case $role cleanup not retried indefinitely"
            } else {
                Check ($waits.Count -eq 1) "$case $role cleanup wait attempted"
            }
            Check (@($waits | Where-Object { $_ -ne '5000' }).Count -eq 0) "$case $role bounded wait=5000"
        } else {
            Check ($kills.Count -eq 0 -and $waits.Count -eq 0) "$case $role no cleanup without owned process"
        }
        if ($role -in $launches) {
            $launchArgs = @(EvidenceLines $caseRoot "$role-args.txt")
            Check (@($launchArgs | Where-Object { $_ -match '^-ABSLOG=".+space temp.+"$' }).Count -eq 1) "$case $role quoted ABSLOG with space TEMP"
            Check (@($launchArgs | Where-Object { $_ -like '*MyProject.uproject*' -and $_ -match '^".+"$' }).Count -eq 1) "$case $role quoted project path"
            $logPaths = @(EvidenceLines $caseRoot "$role-logpath.txt")
            if ($created) {
                Check ($logPaths.Count -eq 1 -and [IO.Path]::IsPathRooted($logPaths[0])) "$case $role absolute log path"
            }
        }
    }
    if ($case -like '*_after_join_*') {
        $phaseEvidence = @(EvidenceLines $caseRoot 'observation-exit.txt')
        $updatedRunner = @(EvidenceLines $caseRoot 'runner-capabilities.txt') -contains 'ServerSeconds=True'
        Check ($phaseEvidence.Count -gt 0) "$case exit injected during client observation"
        if ($updatedRunner) {
            Check ($phaseEvidence -contains 'phase=client sleep=2') "$case exit after second live observation poll"
        }
        foreach ($role in @('server', 'client')) {
            $paths = @(EvidenceLines $caseRoot "$role-logpath.txt")
            $marker = if ($role -eq 'server') { 'Join succeeded' } else { 'Welcomed by server' }
            $fixtureValid = $false
            if ($paths.Count -eq 1 -and (Test-Path -LiteralPath $paths[0])) {
                $fixtureValid = [IO.File]::ReadAllText($paths[0]).Contains($marker)
            }
            Check $fixtureValid "$case $role valid connection evidence retained after native exit"
        }
    }
    if ($case -like 'unreadable_*') {
        $role = $case.Substring('unreadable_'.Length)
        Check (Test-Path -LiteralPath (Join-Path $caseRoot "$role-locked.txt")) "$case unreadable fixture exercised"
    }
    if ((Test-Path -LiteralPath (Join-Path $caseRoot 'client-created.txt')) -and
        (Test-Path -LiteralPath (Join-Path $caseRoot 'server-created.txt'))) {
        $cleanupOrder = @(EvidenceLines $caseRoot 'cleanup-order.txt')
        Check ($cleanupOrder.Count -ge 2 -and $cleanupOrder[0] -like 'client:*' -and $cleanupOrder[-1] -like 'server:*') "$case both processes cleaned client then server despite first cleanup failure"
    }
    if ($shouldPass) {
        $map = if ($case -eq 'healthy_winter' -or ($case.StartsWith('isolated_') -and -not $case.EndsWith('_summer'))) { 'Winter_scene' } else { 'Summer_scene' }
        $metrics = "Harita: $map  Port: 17779  Baglanti(server)=1  Welcomed(istemci)=1  Hata(server)=0  Hata(istemci)=0"
        Check (($output -join "`n").Contains($metrics)) "$case historical metric output retained"
        Check (($output -join "`n") -match 'Loglar: .+ ; .+') "$case both log paths reported"
        $serverLog = @(EvidenceLines $caseRoot 'server-logpath.txt')
        $clientLog = @(EvidenceLines $caseRoot 'client-logpath.txt')
        Check ($serverLog.Count -eq 1 -and $clientLog.Count -eq 1 -and $serverLog[0] -ne $clientLog[0]) "$case separate server/client logs"
    }
    if ($case.StartsWith('isolated_') -and -not $rejectBeforeLaunch) {
        $manifestLines = @($output | ForEach-Object { [string]$_ } | Where-Object { $_.StartsWith('Process manifest: ') })
        Check ($manifestLines.Count -eq 1) "$case publishes manifest before native launch"
        Check (@(EvidenceLines $caseRoot 'prelaunch-manifest.txt').Count -eq $launches.Count) "$case manifest exists before each created process"
        if ($manifestLines.Count -eq 1) {
            $manifestFile = $manifestLines[0].Substring('Process manifest: '.Length)
            $manifest = Get-Content -LiteralPath $manifestFile -Raw | ConvertFrom-Json
            Check ($manifest.exitCode -eq $expectedCode) "$case manifest matches native script exit"
            Check ($manifest.serverLog -and $manifest.clientLog -and $manifest.serverLog -ne $manifest.clientLog) "$case both log paths retained on failure"
            foreach ($row in @($manifest.processes)) {
                $created = Test-Path -LiteralPath (Join-Path $caseRoot ($row.role + '-created.txt'))
                Check (($null -ne $row.pid) -eq $created) "$case $($row.role) PID only for created handle"
                if ($created -and $case -notin @('isolated_client_kill_fail', 'isolated_dedicated_client_kill_fail')) { Check $row.stopped "$case $($row.role) stopped by owner" }
            }
            if ($case -like 'isolated_timeout_*') {
                Check ($manifest.timedOut -and -not $manifest.observed) "$case deadline cannot produce successful observation"
            }
            if ($case -like 'isolated_readiness_*') {
                Check ($manifest.serverObservedSeconds -ge 1) "$case minimum server observation retained"
                Check ($manifest.startupOverheadSeconds -eq 5) "$case defined overhead retained"
                if ($case -eq 'isolated_readiness_late') {
                    Check ($manifest.readinessReady -and $manifest.readinessMap -and $manifest.readinessPort -and
                        $manifest.readinessElapsedSeconds -ge 2 -and $manifest.clientObservedSeconds -ge 1) "$case late readiness keeps full client window"
                    Check ($manifest.clientBudgetRemainingSeconds -ge 6 -and $manifest.watchdogSeconds -eq 9) "$case uses original total clock and reserved budget"
                    Check (Test-Path -LiteralPath (Join-Path $caseRoot 'ready-published.txt')) "$case startup markers appeared during polling"
                } else {
                    Check (@($launches | Where-Object { $_ -eq 'client' }).Count -eq 0 -and -not $manifest.observed) "$case no client on failed readiness/budget"
                    $reason = if ($case -eq 'isolated_readiness_early_exit') { 'server-exited-before-ready' }
                        elseif ($case -in @('isolated_readiness_budget', 'isolated_readiness_boundary')) { 'insufficient-client-budget' }
                        else { 'readiness-timeout' }
                    Check ($manifest.startupFailure -ceq $reason -and ($output -join "`n") -match '\[NET_SMOKE_STARTUP\] FAIL') "$case distinct startup failure reason=$reason"
                }
            }
            $requestedMode = if ($case.StartsWith('isolated_dedicated_')) { 'EditorDedicated' } else { 'Listen' }
            Check ($manifest.serverMode -ceq $requestedMode -and $manifest.netModeEvidence -ceq 'NOT_PROVEN') "$case mode intent recorded without fabricated net mode"
            $serverArgv = @(EvidenceLines $caseRoot 'server-args.txt')
            $rawMap = "/Game/HandPaintedEnvironment/Maps/$($manifest.map)/$($manifest.map)"
            Check (($manifest.serverArguments -join "`n") -ceq ($serverArgv -join "`n")) "$case manifest records exact server argv"
            $diagnostic = $case.StartsWith('isolated_dedicated_diag_')
            Check ($manifest.shaderStartupDiagnostics -is [bool] -and $manifest.shaderStartupDiagnostics -eq $diagnostic) "$case diagnostic intent recorded as boolean"
            $logCmds = @($serverArgv | Where-Object { $_ -like '-LogCmds*' })
            Check ($logCmds.Count -eq $(if ($diagnostic) { 1 } else { 0 })) "$case diagnostic opt-in count; default absent"
            if ($diagnostic) { Check ($logCmds[0] -ceq '-LogCmds="LogShaders Log"') "$case one fixed quoted server diagnostic token" }
            if ($requestedMode -eq 'EditorDedicated') {
                Check ($serverArgv[1] -ceq $rawMap -and $serverArgv[2] -ceq '-server' -and '-game' -notin $serverArgv -and
                    @($serverArgv | Where-Object { $_ -like '*?listen' }).Count -eq 0) "$case raw map/server flag only"
                if ($case -in @('isolated_dedicated_fallback', 'isolated_dedicated_missing_cmd', 'isolated_dedicated_wrong_cmd_map',
                    'isolated_dedicated_diag_missing_logcmd', 'isolated_dedicated_diag_wrong_logcmd', 'isolated_dedicated_diag_duplicate_logcmd',
                    'isolated_dedicated_diag_server_sleep_fail', 'isolated_dedicated_diag_readiness_never')) {
                    Check (-not $manifest.serverCommandLineVerified -and 'client' -notin $launches) "$case fallback/native mode evidence refused before client"
                } else { Check $manifest.serverCommandLineVerified "$case native command-line evidence confirmed (not runtime NM)" }
                if ($case -eq 'isolated_dedicated_diag_readiness_never') {
                    Check ($manifest.serverCommandLine -cmatch '(?:^|\s)-LogCmds="LogShaders Log"(?:\s|$)' -and
                        $manifest.startupFailure -ceq 'readiness-timeout' -and $manifest.cleanupOk) "$case actual native diagnostic argv retained without readiness proof; owned cleanup succeeds"
                }
            } else { Check ($serverArgv[1] -ceq "${rawMap}?listen" -and $serverArgv[2] -ceq '-game' -and '-server' -notin $serverArgv) "$case legacy Listen retained" }
            if ('client' -in $launches) {
                $clientArgv = @(EvidenceLines $caseRoot 'client-args.txt')
                Check (($manifest.clientArguments -join "`n") -ceq ($clientArgv -join "`n") -and $clientArgv[2] -ceq '-game' -and '-server' -notin $clientArgv) "$case unchanged exact game client argv"
            }
            Check (@($manifest.clientArguments | Where-Object { $_ -like '-LogCmds*' }).Count -eq 0) "$case no diagnostic in planned client argv"
        }
        foreach ($role in @('server', 'client')) {
            if ($role -notin $launches) { continue }
            $nativeArgs = @(EvidenceLines $caseRoot "$role-args.txt")
            Check (@($nativeArgs | Where-Object { $_ -match '^-UserDir=".+isolated worktree.+WinterNet_[0-9a-f]{32}"$' }).Count -eq 1) "$case $role isolated UserDir quoted"
        }
    }
    Write-Output "Case: $case exit=$code expected=$expectedCode"
}
if ('healthy' -in $cases -and 'healthy_again' -in $cases) { foreach ($role in @('server', 'client')) {
    $first = @(EvidenceLines (Join-Path $scratch 'healthy') "$role-logpath.txt")
    $next = @(EvidenceLines (Join-Path $scratch 'healthy_again') "$role-logpath.txt")
    Check ($first.Count -eq 1 -and $next.Count -eq 1 -and $first[0] -ne $next[0]) "$role repeated runs use fresh unique logs in the same TEMP"
}
}
Write-Output "NetSmokeTest runner: checks=$checks failures=$failures cases=$($cases.Count) selection=$selection available=$availableCases (mocked; no Unreal/network/build)"
Write-Output "Runner: $Runner"
Write-Output "Evidence: $scratch"
if ($failures -gt 0) { exit 1 }
exit 0
