# Supabase karakter deposu native testi (docs/DESIGN_SUPABASE.md 4, Dilim 2); sahte ulasim, gercek ag/Supabase YOK, diske yazmaz.
# Tek kullanimlik NullRHI dunyasi. RunSystemTests.ps1 / RunAllChecks.ps1 adimi DEGILDIR.
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'supabase')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_supabase_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestSupabaseStore,quit"')
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
Write-Output "[MMO_SUPABASE_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_SUPABASE_CASE\] (PASS|FAIL) case=\S+ check=\S+')
$summaries = @($lines | Select-String '\[MMO_SUPABASE_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_SUPABASE_TEST\] PASS checks=(\d+) reason=')
# Kapali-basarisiz senkron yontemler bilerek Error loglar (test tam 5 satir bekler: Load, Save, Exists, ListSlotNames, LoadAsync->Load).
$closedFail = @($lines | Select-String 'LogMMOSupabase: Error: Senkron')
$errors = @($lines | Select-String '\[MMO_SUPABASE_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation' | Where-Object { $_.Line -notmatch 'LogMMOSupabase: Error: Senkron' })
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
# Kontrol sayisi, ozet satiri ve tek tek CASE satirlari birbiriyle tutarli olmali (sessizce eksik kosan test gecmesin).
# Benzersiz etiket = case/check cifti.
$reported = 0
if ($passes.Count -eq 1 -and $passes[0].Line -match 'checks=(\d+)') { $reported = [int]$Matches[1] }
$unique = @($cases | ForEach-Object { if ($_.Line -match 'case=(\S+) check=(\S+)') { $Matches[1] + '/' + $Matches[2] } } | Sort-Object -Unique)
# Beklenen sayi MMOSupabaseStoreSelfTest.cpp'deki SupabaseTestExpectedChecks ile ayni olmali.
$expected = 531
# Test, vectors.json'ta bilmedigi senaryo bulursa (sonradan eklenmis) adimlarini genel yurutucuyle kosar ve "extra" sayar (FAIL degil);
# NOTE satiri: unknown_scenarios / extra_checks / skipped_admin. Raporlanan toplam = expected + extra olmali.
$note = @($lines | Select-String '\[MMO_SUPABASE_NOTE\] unknown_scenarios=\d+ extra_checks=\d+ skipped_admin=\d+')
$unknownScn = 0; $extra = 0; $skippedAdmin = 0
if ($note.Count -eq 1 -and $note[0].Line -match 'unknown_scenarios=(\d+) extra_checks=(\d+) skipped_admin=(\d+)') { $unknownScn = [int]$Matches[1]; $extra = [int]$Matches[2]; $skippedAdmin = [int]$Matches[3] }
$consistent = ($note.Count -eq 1) -and ($closedFail.Count -eq 5) -and ($reported -eq ($expected + $extra)) -and ($cases.Count -eq $reported) -and ($unique.Count -eq $reported)
Write-Output "[MMO_SUPABASE_RUNNER] reported=$reported expected=$expected extra=$extra unknown_scenarios=$unknownScn skipped_admin=$skippedAdmin case_lines=$($cases.Count) unique_labels=$($unique.Count) closed_fail_logs=$($closedFail.Count) consistent=$consistent"
if ($timedOut) { Write-Output 'FAIL watchdog_90s'; exit 2 }
if ($nativeExit -eq 0 -and $consistent -and $summaries.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output "[MMO_SUPABASE_RUNNER] PASS checks=$reported"; exit 0
}
Write-Output "[MMO_SUPABASE_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count)"
exit 1
