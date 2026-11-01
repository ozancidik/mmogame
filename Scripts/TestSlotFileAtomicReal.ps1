# Karakter .sav atomik yazim GERCEK dosya sistemi testi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md 6.1, S6.1): IFileManager + FMMOLocalFileCharacterStore uctan uca.
# YALNIZ Intermediate/MMOSlotAtomicRealTest gecici dizininde calisir; Saved/ yazilmaz: kosucu Saved/SaveGames oncesi/sonrasi parmak izini karsilastirir.
# Cikis: 0 PASS, 1 FAIL, 2 zaman asimi. Calisan UE/UBT varsa ertelemez (yalniz bu betigin surecini baslatir).
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'slotreal')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_slotreal_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestSlotAtomicReal,quit"')
function Get-SavedPrint {
    $d = Join-Path $root 'Saved\SaveGames'
    if (-not (Test-Path -LiteralPath $d)) { return 'absent' }
    return ((Get-ChildItem -LiteralPath $d -File -Force | Sort-Object Name | ForEach-Object { $_.Name + '|' + $_.Length + '|' + $_.LastWriteTimeUtc.Ticks }) -join ';')
}
$savedBefore = Get-SavedPrint
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
Write-Output "[MMO_SLOTREAL_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_SLOTREAL_CASE\] (PASS|FAIL) case=\S+ check=\S+')
$summaries = @($lines | Select-String '\[MMO_SLOTREAL_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_SLOTREAL_TEST\] PASS checks=(\d+) reason=')
$errors = @($lines | Select-String '\[MMO_SLOTREAL_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation' | Where-Object { $_.Line -notmatch 'test_injected=1' })  # QA-3 testi kasitli Error basar (ReportFormatSelfTest(false,test_injected=1)); yalniz o satir haric
$lines | Select-String -SimpleMatch '[MMO_SLOTREAL_R1]' | ForEach-Object { Write-Output $_.Line }
$lines | Select-String -SimpleMatch '[MMO_SLOTREAL_OBS]' | ForEach-Object { Write-Output $_.Line }
$lines | Select-String -SimpleMatch '[MMO_SLOTREAL_INFO]' | ForEach-Object { Write-Output $_.Line }
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
# Kontrol sayisi, ozet satiri ve tek tek CASE satirlari birbiriyle tutarli olmali (sessizce eksik kosan test gecmesin).
# Benzersiz etiket = case/check cifti.
$reported = 0
if ($passes.Count -eq 1 -and $passes[0].Line -match 'checks=(\d+)') { $reported = [int]$Matches[1] }
$unique = @($cases | ForEach-Object { if ($_.Line -match 'case=(\S+) check=(\S+)') { $Matches[1] + '/' + $Matches[2] } } | Sort-Object -Unique)
# Beklenen N (SelfTest.cpp SlotRealTestExpectedChecks ve commit mesajiyla ayni).
$expected = 100
$consistent = ($reported -eq $expected) -and ($cases.Count -eq $reported) -and ($unique.Count -eq $reported)
Write-Output "[MMO_SLOTREAL_RUNNER] reported=$reported case_lines=$($cases.Count) unique_labels=$($unique.Count) consistent=$consistent"
$savedUntouched = ((Get-SavedPrint) -eq $savedBefore)
Write-Output "[MMO_SLOTREAL_RUNNER] saved_savegames_untouched=$savedUntouched"
$tmpLeft = Test-Path -LiteralPath (Join-Path $root 'Intermediate\MMOSlotAtomicRealTest\*')
if ($timedOut) { Write-Output 'FAIL watchdog_90s'; exit 2 }
if ($nativeExit -eq 0 -and $consistent -and $savedUntouched -and -not $tmpLeft -and $summaries.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output "[MMO_SLOTREAL_RUNNER] PASS checks=$reported"; exit 0
}
Write-Output "[MMO_SLOTREAL_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count)"
exit 1
