# IMMOCharacterStore (yerel dosya) testi: diskteki tum MMOSave_*.sav yuvalari magaza uzerinden
# listelenir ve yuklenir (eski .sav uyumu, yalniz okuma), ardindan gecici yuvaya Save -> Load gidis-donusu
# yapilip gecici dosya silinir. Editor kapali olmali. Cikis 0 = hepsi yuklendi + gidis-donus esit.

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root "MyProject.uproject"
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$log = Join-Path $env:TEMP "mmo_character_store_test.log"
if (Test-Path $log) { Remove-Item $log }

$map = "/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene"
$args = @("`"$project`"", $map, "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash",
    "-stdout", "-ABSLOG=$log", '-ExecCmds="MMOTestStore,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(280000)) {
    $process.Kill()
    Write-Output "ZAMAN ASIMI: magaza testi kapanmadi"
    exit 2
}

$summary = Select-String -Path $log -Pattern "\[MMO_STORE_TEST\] slots=(\d+) loaded=(\d+) invalid=(\d+) loadFailed=(\d+) existsMismatch=(\d+)"
$round = Select-String -Path $log -Pattern "\[MMO_STORE_TEST\] roundtrip .* saved=1 loaded=1 equal=1 existsAfterSave=1"
$fail = Select-String -Path $log -Pattern "\[MMO_STORE_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation"
$jsonLine = Select-String -Path $log -Pattern "\[MMO_STORE_TEST\] json ok=(\d+) mismatch=(\d+) maxLevel=(\d+)"
$ok = $false
if ($summary) {
    $m = $summary.Matches[0].Groups
    $slots = [int]$m[1].Value; $loaded = [int]$m[2].Value; $failed = [int]$m[4].Value; $mismatch = [int]$m[5].Value
    $ok = ($failed -eq 0) -and ($mismatch -eq 0) -and (($slots -eq 0) -or [bool]$round)
    # Gercek .sav -> JSON -> yapi esitligi: her gecerli yuva alan alan esit olmali.
    if ($loaded -gt 0) {
        $jm = if ($jsonLine) { $jsonLine.Matches[0].Groups } else { $null }
        $ok = $ok -and $jm -and ([int]$jm[2].Value -eq 0) -and ([int]$jm[1].Value -eq $loaded)
    }
}
if ($ok -and -not $fail -and $process.ExitCode -eq 0) {
    Write-Output $summary.Line
    if ($jsonLine) { Write-Output $jsonLine.Line }
    if ($round) { Write-Output $round.Line }
    if ($slots -eq 0) { Write-Output "UYARI: hic karakter yuvasi yok -> okuma/JSON/gidis-donus ATLANDI (bos gecti, kanit degildir)" }
    exit 0
}
Write-Output "Magaza testi KALDI (exit=$($process.ExitCode))"
if ($summary) { Write-Output $summary.Line }
if ($jsonLine) { Write-Output $jsonLine.Line }
if ($fail) { $fail | Select-Object -First 10 | ForEach-Object { Write-Output $_.Line } }
exit 1
