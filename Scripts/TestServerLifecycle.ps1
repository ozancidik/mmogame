# Planli kapatma durum makinesi (UMMOServerLifecycleSubsystem) native testi (docs/DESIGN_SERVER_LIFECYCLE.md 7, L1); canli listen sunucu dunyasi, diske yazmaz.
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'lifecycle')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_lifecycle_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$port = Get-Random -Minimum 20000 -Maximum 40000
$ueArgs = @("`"$project`"", ($map + '?listen'), "-port=$port", '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestServerLifecycle,quit"')
$process = $null
$timedOut = $false
$nativeExit = $null
try {
    $process = Start-Process -FilePath $exe -ArgumentList $ueArgs -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(90000)) {
        $timedOut = $true
        if (-not $process.HasExited) { $process.Kill() }
        if ($process.WaitForExit(5000)) { $nativeExit = $process.ExitCode }
    } else { $nativeExit = $process.ExitCode }
} finally {
    # Yalniz bu betigin baslattigi sureci oldurur.
    if ($null -ne $process) {
        try {
            if (-not $process.HasExited) {
                $process.Kill()
                if (-not $process.WaitForExit(5000)) { throw 'Owned UE process did not stop' }
            }
        } finally { $process.Dispose() }
    }
}
Write-Output "[MMO_LIFECYCLE_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_LIFECYCLE_CASE\] (PASS|FAIL) case=\S+ check=\S+')
$summaries = @($lines | Select-String '\[MMO_LIFECYCLE_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_LIFECYCLE_TEST\] PASS checks=(\d+) reason=')
# Test kasitli olarak 2 komut satiri reddi (cmdapply: aralik disi + gecersiz deger) provoke eder; bu 2 beklenen Error satiri hata sayilmaz,
# ama sayilari TAM 2 olmali (eksik = log Error'a dusurulmemis, fazla = beklenmeyen).
$expectedErrorPattern = 'LogMMOLifecycle: Error: event=shutdown_cmdline_rejected'
$expectedErrorLines = @($lines | Select-String $expectedErrorPattern)
$errors = @($lines | Select-String '\[MMO_LIFECYCLE_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation' | Where-Object { $_.Line -notmatch $expectedErrorPattern })
if ($expectedErrorLines.Count -ne 2) { $errors += [pscustomobject]@{ Line = "expected_cmdline_error_lines=2 observed=$($expectedErrorLines.Count)" } }
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
# Kontrol sayisi, ozet satiri ve tek tek CASE satirlari birbiriyle tutarli olmali (sessizce eksik kosan test gecmesin).
# Benzersiz etiket = case/check cifti.
$reported = 0
if ($passes.Count -eq 1 -and $passes[0].Line -match 'checks=(\d+)') { $reported = [int]$Matches[1] }
$unique = @($cases | ForEach-Object { if ($_.Line -match 'case=(\S+) check=(\S+)') { $Matches[1] + '/' + $Matches[2] } } | Sort-Object -Unique)
# Beklenen = LifecycleTestExpectedChecks (MMOServerLifecycleSelfTest.cpp); CASE sayisi = benzersiz case/check etiketi.
$expected = 137
$consistent = ($reported -eq $expected) -and ($cases.Count -eq $reported) -and ($unique.Count -eq $reported)
Write-Output "[MMO_LIFECYCLE_RUNNER] reported=$reported case_lines=$($cases.Count) unique_labels=$($unique.Count) consistent=$consistent"
if ($timedOut) { Write-Output 'FAIL watchdog_90s'; exit 2 }
if ($nativeExit -eq 0 -and $consistent -and $summaries.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output "[MMO_LIFECYCLE_RUNNER] PASS checks=$reported"; exit 0
}
Write-Output "[MMO_LIFECYCLE_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count)"
exit 1
