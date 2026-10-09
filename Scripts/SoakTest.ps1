# Soak testi: haritayi -game -NullRHI ile -Seconds boyunca acik birakir; takilma ('is stuck and failed to move'), hata ve
# istege bagli bir cebin ilk doldurmasini sayar. Cep kalitesini olcmek icin (MMOProbeSurface yalan soyleyebilir). Editor KAPALI olmali.
# Ornek: powershell -File Scripts/SoakTest.ps1 -Tag t1 -Map Winter_scene -Seconds 90 -PocketPrefix "V(X=4750.00, Y=-5750.00" -Cx 4750 -Cy -5750
# Tum spawner'larin ozeti: python Scripts/SpawnAudit.py %TEMP%\mmo_soak_<Tag>.log [...]
param([string]$Tag, [int]$Seconds = 90, [string]$Map = "Summer_scene", [string]$PocketPrefix = "", [double]$Cx = [double]::NaN, [double]$Cy = 0, [double]$Radius = 1200)
$project = Join-Path (Split-Path -Parent $PSScriptRoot) "MyProject.uproject"
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$log = Join-Path $env:TEMP ("mmo_soak_" + $Tag + ".log")
if ([IO.File]::Exists($log)) { [IO.File]::Delete($log) }
$mapPath = "/Game/HandPaintedEnvironment/Maps/$Map/$Map"
$p = Start-Process -FilePath $exe -ArgumentList @("`"$project`"", $mapPath, "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=$log") -PassThru -WindowStyle Hidden
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) { $p.Kill() }
Start-Sleep -Seconds 2
$stuck = (Select-String -Path $log -Pattern 'is stuck and failed to move' | Measure-Object).Count
$errs = (Select-String -Path $log -Pattern 'Error:|Fatal error|Ensure condition' | Measure-Object).Count
$fill = ""
if ($PocketPrefix) {
    $l = Get-Content $log
    $i = ($l | Select-String -SimpleMatch ("Spawner BeginPlay @ " + $PocketPrefix) | Select-Object -First 1).LineNumber
    if ($i) { $j = $i; $t = 0; while ($j -lt $l.Count -and $l[$j] -notmatch 'after TopUp') { if ($l[$j] -match 'SpawnOne: requested') { $t++ }; $j++ }; $fill = ($l[$j] -replace '^.*live=', 'live=') + "/tries=$t" }
}
$near = ""
if (-not [double]::IsNaN($Cx)) {
    $n = 0
    Select-String -Path $log -Pattern 'is stuck and failed to move' | ForEach-Object {
        if ($_.Line -match 'Location: X=(-?[\d.]+) Y=(-?[\d.]+)') { if ([math]::Sqrt([math]::Pow([double]$Matches[1] - $Cx, 2) + [math]::Pow([double]$Matches[2] - $Cy, 2)) -le $Radius) { $n++ } }
    }
    $near = " stuckNear=$n"
}
Write-Output ("{0}: stuck={1}{4} errors={2} pocketFill={3}" -f $Tag, $stuck, $errs, $fill, $near)
