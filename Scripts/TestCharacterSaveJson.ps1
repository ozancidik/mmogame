# Surumlu FMMOCharacterSaveData JSON gidis-donusunu gercek UE yansimasi ile test eder.
# Editor kapali olmali. Cikis 0 = tum alanlar esit, ileri sema surumu reddedildi.

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root "MyProject.uproject"
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$log = Join-Path $env:TEMP "mmo_character_save_json.log"
if (Test-Path $log) { Remove-Item $log }

$map = "/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene"
$args = @("`"$project`"", $map, "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash",
    "-stdout", "-ABSLOG=$log", '-ExecCmds="MMOTestSaveJson,quit"')
$process = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(280000)) {
    $process.Kill()
    Write-Output "ZAMAN ASIMI: JSON testi kapanmadi"
    exit 2
}

$pass = Select-String -Path $log -Pattern "\[MMO_SAVE_JSON_TEST\] PASS schema=2 bytes=([1-9][0-9]*) negative=([5-9][0-9]|[1-9][0-9]{2,})"
$enumPolicy = @(Select-String -Path $log -Pattern '\[MMO_SAVE_ENUM_TEST\] PASS variants=18 invalid_writers=2')
$fail = Select-String -Path $log -Pattern "\[MMO_SAVE_JSON_TEST\] FAIL|Error:|Fatal error|Ensure condition|Access violation"
if ($pass -and $enumPolicy.Count -eq 1 -and -not $fail -and $process.ExitCode -eq 0) {
    Write-Output $pass.Line
    Write-Output $enumPolicy[0].Line
    exit 0
}
Write-Output "JSON gidis-donus testi KALDI (exit=$($process.ExitCode))"
if ($fail) { $fail | Select-Object -First 10 | ForEach-Object { Write-Output $_.Line } }
exit 1
