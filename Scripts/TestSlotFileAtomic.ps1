# Karakter .sav atomik yazim/kurtarma (FMMOSlotFileAtomic) native testi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md 6.1, S6.0); dunyasiz, diske yazmaz (sahte dosya sistemi).
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'slotatomic')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_slotatomic_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestSlotAtomic,quit"')
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
Write-Output "[MMO_SLOTATOMIC_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_SLOTATOMIC_CASE\] (PASS|FAIL) case=\S+ check=\S+')
$summaries = @($lines | Select-String '\[MMO_SLOTATOMIC_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_SLOTATOMIC_TEST\] PASS checks=(\d+) reason=')
$errors = @($lines | Select-String '\[MMO_SLOTATOMIC_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation')
$lines | Select-String -SimpleMatch '[MMO_SLOTATOMIC_MATRIX' | ForEach-Object { Write-Output $_.Line }
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
# Kontrol sayisi, ozet satiri ve tek tek CASE satirlari birbiriyle tutarli olmali (sessizce eksik kosan test gecmesin).
# Benzersiz etiket = case/check cifti.
$reported = 0
if ($passes.Count -eq 1 -and $passes[0].Line -match 'checks=(\d+)') { $reported = [int]$Matches[1] }
$unique = @($cases | ForEach-Object { if ($_.Line -match 'case=(\S+) check=(\S+)') { $Matches[1] + '/' + $Matches[2] } } | Sort-Object -Unique)
# Beklenen = 158 (SelfTest.cpp SlotAtomicTestExpectedChecks ve commit mesajiyla ayni): 11 senaryo x 9 matris kontrolu + 59 adlandirilmis vaka.
$expected = 158
$consistent = ($reported -eq $expected) -and ($cases.Count -eq $reported) -and ($unique.Count -eq $reported)
Write-Output "[MMO_SLOTATOMIC_RUNNER] reported=$reported case_lines=$($cases.Count) unique_labels=$($unique.Count) consistent=$consistent"
if ($timedOut) { Write-Output 'FAIL watchdog_90s'; exit 2 }
if ($nativeExit -eq 0 -and $consistent -and $summaries.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output "[MMO_SLOTATOMIC_RUNNER] PASS checks=$reported"; exit 0
}
Write-Output "[MMO_SLOTATOMIC_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count)"
exit 1
