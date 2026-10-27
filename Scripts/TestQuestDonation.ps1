# Altin bagis gorevleri (HaveGold, hedef >= GoldDonationMinTarget) veri + teslim regresyonu; headless NullRHI otorite dunyasi.
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'quest-donation')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_quest_donation_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$expected = 68   # 11 bagis gorevi x 6 + 2 genel (MMOQuestDonationSelfTest.cpp ile ayni)
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestQuestDonation,quit"')
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
Write-Output "[MMO_QUEST_DONATION_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$caseLines = @($lines | Select-String '\[MMO_QUEST_DONATION_CASE\] (PASS|FAIL) check=')
$labels = @($caseLines | ForEach-Object { if ($_.Line -match 'check=(\S+)') { $Matches[1] } })
$unique = @($labels | Sort-Object -Unique)
$summaries = @($lines | Select-String '\[MMO_QUEST_DONATION_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String "\[MMO_QUEST_DONATION_TEST\] PASS checks=$expected\b reason=")
$errors = @($lines | Select-String '\[MMO_QUEST_DONATION_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation')
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
if ($timedOut) { Write-Output 'FAIL watchdog_60s'; exit 2 }
if ($nativeExit -eq 0 -and $caseLines.Count -eq $expected -and $unique.Count -eq $expected -and $summaries.Count -eq 1 -and $passes.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output "[MMO_QUEST_DONATION_RUNNER] PASS checks=$expected unique_labels=$($unique.Count)"; exit 0
}
Write-Output "[MMO_QUEST_DONATION_RUNNER] FAIL observed_cases=$($caseLines.Count) unique=$($unique.Count) errors=$($errors.Count)"
exit 1
