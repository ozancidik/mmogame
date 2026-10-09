# Soak testi: -Seconds baslatmadan itibaren toplam gozlem suresidir (yukleme dahil).
# Cikis 0: sure tamamlandi, istenen harita/spawner logu var, hata yok. Takilma sayisi bir olcumdur, PASS kosulu degildir.
# Editor KAPALI olmali. Ornek: powershell -File Scripts/SoakTest.ps1 -Tag t1 -Map Winter_scene -Seconds 90
# Her kosu ayri log kullanir; SpawnAudit.py'ye ciktidaki tam Log yolunu ver.
# -Cmds baslangic konsol komutlaridir; quit erken cikis olarak KALDI sayilir.
param(
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$Tag = 'soak',
    [ValidateRange(1, 3600)][int]$Seconds = 90,
    [ValidateSet('Summer_scene', 'Winter_scene')][string]$Map = 'Summer_scene',
    [string]$PocketPrefix = '', [double]$Cx = [double]::NaN, [double]$Cy = 0,
    [ValidateRange(0, 1000000)][double]$Radius = 1200, [string]$Cmds = ''
)
$ErrorActionPreference = 'Stop'
$project = Join-Path (Split-Path -Parent $PSScriptRoot) 'MyProject.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $env:TEMP ("mmo_soak_${Tag}_" + [guid]::NewGuid().ToString('N') + '.log')
$mapPath = "/Game/HandPaintedEnvironment/Maps/$Map/$Map"
$argList = @("`"$project`"", $mapPath, '-game', '-NullRHI', '-nosound', '-unattended', '-nosplash', '-stdout', "-ABSLOG=`"$log`"")
if ($Cmds) { $argList += "-ExecCmds=`"$Cmds`"" }
$p = $null
$completed = $false
$cleanupOk = $true
Write-Output "Log: $log"
try {
    $p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        if ($p.HasExited) { throw "Oyun sure dolmadan kapandi (exit=$($p.ExitCode))." }
        Start-Sleep -Milliseconds 250
    }
    if ($p.HasExited) { throw "Oyun gozlem sonunda kapanmis (exit=$($p.ExitCode))." }
    $completed = $true
} catch {
    Write-Output "KALDI: $($_.Exception.Message)"
} finally {
    # Yalniz bu kosunun sahip oldugu Process nesnesi; yabanci editor/soak sureclerine dokunma.
    if ($null -ne $p) {
        try {
            if (-not $p.HasExited) { $p.Kill() }
            if (-not $p.WaitForExit(5000)) { throw 'Soak sureci sonlandirilamadi.' }
        } catch {
            $cleanupOk = $false
            Write-Output "KALDI: cleanup: $($_.Exception.Message)"
        }
    }
}
if (-not $completed -or -not $cleanupOk) { exit 1 }
try {
    $lines = @(Get-Content -LiteralPath $log)
    $errs = @($lines | Select-String -Pattern 'Error:|Fatal error|Ensure condition|Access violation').Count
    $stuckLines = @($lines | Select-String -Pattern 'is stuck and failed to move')
    $ready = @($lines | Select-String -SimpleMatch "LogWorld: Bringing World $mapPath.$Map up for play").Count -gt 0
    $spawners = @($lines | Select-String -SimpleMatch 'Spawner BeginPlay @ ').Count
    $fill = ''
    if ($PocketPrefix) {
        $block = $lines | Select-String -SimpleMatch ("Spawner BeginPlay @ " + $PocketPrefix) | Select-Object -First 1
        if ($null -eq $block) { throw 'Istenen cep logda bulunamadi.' }
        $j = $block.LineNumber
        $tries = 0
        while ($j -lt $lines.Count -and $lines[$j] -notmatch 'after TopUp' -and $lines[$j] -notmatch 'Spawner BeginPlay @ ') {
            if ($lines[$j] -match 'SpawnOne: requested') { $tries++ }
            $j++
        }
        if ($j -ge $lines.Count -or $lines[$j] -notmatch 'after TopUp: live=(\d+)') { throw 'Istenen cebin ilk doldurma blogu eksik.' }
        $fill = "live=$($Matches[1])/tries=$tries"
    }
    $near = ''
    if (-not [double]::IsNaN($Cx)) {
        $n = 0
        foreach ($entry in $stuckLines) {
            if ($entry.Line -match 'Location: X=(-?[\d.]+) Y=(-?[\d.]+)') {
                if ([math]::Sqrt([math]::Pow([double]$Matches[1] - $Cx, 2) + [math]::Pow([double]$Matches[2] - $Cy, 2)) -le $Radius) { $n++ }
            }
        }
        $near = " stuckNear=$n"
    }
    Write-Output ("{0}: stuck={1}{4} errors={2} pocketFill={3} ready={5} spawners={6}" -f $Tag, $stuckLines.Count, $errs, $fill, $near, $ready, $spawners)
    if (-not $ready -or $spawners -eq 0 -or $errs -gt 0) { throw 'Harita/spawner kaniti eksik veya hata satiri var.' }
    Write-Output 'GECTI: gozlem suresi tamamlandi; native temiz cikis veya oynanabilirlik garantisi degildir.'
    exit 0
} catch {
    Write-Output "KALDI: $($_.Exception.Message)"
    exit 1
}
