# Native skill effect continuation regression; disposable authority NullRHI world.
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'skill-effect-continuation')
$ErrorActionPreference = 'Stop'
function Test-SkillEffectContinuationCoverage([string[]]$CaseLines) {
    $expected = @(foreach($kind in 0..2) { foreach($route in 0..1) { foreach($who in 0..1) { foreach($mode in 0..2) { "kind${kind}_route${route}_who${who}_mode${mode}" } } } })
    $checks = @('fixture','termination','callout','acceptance','mana','cooldown','primary','effects','remaining','context')
    $records = @($CaseLines | ForEach-Object {
        if ($_ -match ' case=(\S+) check=(\S+)') { [pscustomobject]@{case=$Matches[1];check=$Matches[2]} }
    })
    $actual = @($records.case | Sort-Object -Unique)
    $duplicates = @($records | Group-Object case,check | Where-Object {$_.Count -ne 1})
    $badChecks = @($records | Where-Object {$_.check -notin $checks})
    return $records.Count -eq 360 -and $records.Count -eq $CaseLines.Count -and $actual.Count -eq 36 -and
        @(Compare-Object $expected $actual).Count -eq 0 -and $duplicates.Count -eq 0 -and $badChecks.Count -eq 0
}
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_skill_effect_continuation_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestSkillEffectContinuation,quit"')
$process = $null
$timedOut = $false
$nativeExit = $null
try {
    $process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(60000)) {
        $timedOut = $true
        if (-not $process.HasExited) { $process.Kill() }
        if ($process.WaitForExit(5000)) { $nativeExit = $process.ExitCode }
    } else { $nativeExit = $process.ExitCode }
} finally {
    if ($null -ne $process) {
        try {
            if (-not $process.HasExited) {
                $process.Kill()
                if (-not $process.WaitForExit(5000)) { throw 'Owned UE process did not stop' }
            }
        } finally { $process.Dispose() }
    }
}
Write-Output "[MMO_SKILL_EFFECT_CONTINUATION_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_SKILL_EFFECT_CONTINUATION_CASE\] (PASS|FAIL) case=')
$summaries = @($lines | Select-String '\[MMO_SKILL_EFFECT_CONTINUATION_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_SKILL_EFFECT_CONTINUATION_TEST\] PASS checks=360\b reason=')
$errors = @($lines | Select-String '\[MMO_SKILL_EFFECT_CONTINUATION_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation')
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
$coverage = Test-SkillEffectContinuationCoverage @($cases | ForEach-Object {$_.Line})
Write-Output "[MMO_SKILL_EFFECT_CONTINUATION_RUNNER] coverage=$coverage"
if ($timedOut) { Write-Output 'FAIL watchdog_60s'; exit 2 }
if ($nativeExit -eq 0 -and $coverage -and $summaries.Count -eq 1 -and $passes.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output '[MMO_SKILL_EFFECT_CONTINUATION_RUNNER] PASS checks=360 paths=3 outer_routes=2 ended_identities=2 terminal_modes=3 cases=36'; exit 0
}
Write-Output "[MMO_SKILL_EFFECT_CONTINUATION_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count)"
exit 1
