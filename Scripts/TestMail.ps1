# Posta native testi (ad/JSON/disk deposu, ucret, canli gonderim/toplama, ayristirma, K1 kayit sirasi, kilit, B3, C1, O2 tavan, .tmp kurtarma); tek kullanimlik NullRHI dunyasi.
param([ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Label = 'mail')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = [IO.Path]::GetFullPath((Join-Path $root 'MyProject.uproject'))
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ('mmo_mail_' + $Label + '_' + [guid]::NewGuid().ToString('N') + '.log')
$map = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
$ueArgs = @("`"$project`"", $map, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout',
    "-ABSLOG=`"$log`"", '-ExecCmds="MMOTestMail,quit"')
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
    if ($null -ne $process) {
        try {
            if (-not $process.HasExited) {
                $process.Kill()
                if (-not $process.WaitForExit(5000)) { throw 'Owned UE process did not stop' }
            }
        } finally { $process.Dispose() }
    }
}
Write-Output "[MMO_MAIL_RUNNER] label=$Label native_exit=$nativeExit timeout=$timedOut log=$log"
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { Write-Output 'FAIL log_missing'; exit 1 }
$lines = Get-Content -LiteralPath $log
$cases = @($lines | Select-String '\[MMO_MAIL_CASE\] (PASS|FAIL) check=')
$summaries = @($lines | Select-String '\[MMO_MAIL_TEST\] (PASS|FAIL) checks=')
$passes = @($lines | Select-String '\[MMO_MAIL_TEST\] PASS checks=(\d+) reason=')
$errors = @($lines | Select-String '\[MMO_MAIL_(CASE|TEST)\] FAIL|Error:|Fatal error|Ensure condition|Access violation')
# Dizin olan kutu yolu senaryosu (tmp_first_mail_*): tur 4 (Y-NEW-2) ile WriteBox'un .tmp -> ana Move'u tekrar denemesiz;
# motor "Error moving file" satiri (ve 10x0.5 sn bekleme) URETMEZ. Testin mmomail_ztestmaild_ dosyasina ait bu satir 0 olmali;
# gorulurse tekrar denemeli (sunucuyu donduran) yola donulmustur.
$expectedLockPattern = "LogFileManager: Error: Error moving file '[^']*mmomail_ztestmaild_"
$expectedLock = @($errors | Where-Object { $_.Line -match $expectedLockPattern })
$errors = @($errors | Where-Object { $_.Line -notmatch $expectedLockPattern })
Write-Output "[MMO_MAIL_RUNNER] expected_move_error_lines=$($expectedLock.Count) (beklenen 0)"
@($lines | Select-String '\[MMO_MAIL_NOTE\]') | ForEach-Object { Write-Output $_.Line }
$summaries | ForEach-Object { Write-Output $_.Line }
$errors | ForEach-Object { Write-Output $_.Line }
# Kontrol sayisi, ozet satiri ve tek tek CASE satirlari birbiriyle tutarli olmali (sessizce eksik kosan test gecmesin).
$reported = 0
if ($passes.Count -eq 1 -and $passes[0].Line -match 'checks=(\d+)') { $reported = [int]$Matches[1] }
$unique = @($cases | ForEach-Object { if ($_.Line -match 'check=(\S+)') { $Matches[1] } } | Sort-Object -Unique)
# Esik = bilinen kontrol sayisi (104: 41 eski + 48 ayristirma/K1/kilit/ad/B3/C1/O2/D3 + 15 guvenlik tur 2: .tmp temizligi,
# bozuk kutunun .tmp.bad'i, callback'li iade) + 26 tur 3/4: ASCII ad temizleme, .bak durum makinesi (.bak alinamaz / Move+dogrudan
# yazim basarisiz / .bak-.tmp kurtarma sirasi / bozuk ana + .bak), kilitli dosyada < 1 sn, 60 sn geri cekilme penceresi, bozuk/okunamayan
# alici kutusu, BoxFull/StoreError sogumasi, fee_clamp kontrolu = 130; kontrol silinirse/atlanirsa kosu KALIR.
$consistent = ($reported -ge 130) -and ($cases.Count -eq $reported) -and ($unique.Count -eq $reported) -and ($expectedLock.Count -eq 0)
Write-Output "[MMO_MAIL_RUNNER] reported=$reported case_lines=$($cases.Count) unique_labels=$($unique.Count) consistent=$consistent"
if ($timedOut) { Write-Output 'FAIL watchdog_90s'; exit 2 }
if ($nativeExit -eq 0 -and $consistent -and $summaries.Count -eq 1 -and $errors.Count -eq 0) {
    Write-Output "[MMO_MAIL_RUNNER] PASS checks=$reported"; exit 0
}
Write-Output "[MMO_MAIL_RUNNER] FAIL observed_checks=$($cases.Count) errors=$($errors.Count)"
exit 1
