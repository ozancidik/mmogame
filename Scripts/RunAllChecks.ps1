# Tum otomatik kontrolleri sirayla calistirir ve ozet verir. Editor KAPALI olmali. Kullanim:
#   powershell -File Scripts/RunAllChecks.ps1 [-SkipBuild] [-SkipNet]
# Sirasi: veri denetimi -> derleme -> Summer duman -> Winter duman -> cok oyunculu baglanti.
# Cikis 0 = hepsi gecti.
param(
    [switch]$SkipBuild,
    [switch]$SkipNet
)

$root = Split-Path -Parent $PSScriptRoot
$py = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
$ue = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat"
$results = [ordered]@{}

function Step($name, [scriptblock]$body) {
    Write-Output ">> $name"
    try { $ok = & $body } catch { $ok = $false }
    $results[$name] = [bool]$ok
    Write-Output ("   -> " + $(if ($ok) { "GECTI" } else { "KALDI" }))
}

Step "Veri denetimi (ValidateData.py)" {
    & $py (Join-Path $PSScriptRoot "ValidateData.py") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Statik C++ kontrolu (StaticLint.py; derleme degildir)" {
    & $py (Join-Path $PSScriptRoot "StaticLint.py") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "PlaytestMetrics fixture testi (TestPlaytestMetrics.py)" {
    & $py (Join-Path $PSScriptRoot "TestPlaytestMetrics.py") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Denetim negatif testleri (TestStaticLint/TestValidateData)" {
    & $py (Join-Path $PSScriptRoot "TestStaticLint.py") | Out-Host
    $a = $LASTEXITCODE
    & $py (Join-Path $PSScriptRoot "TestValidateData.py") | Out-Host
    ($a -eq 0) -and ($LASTEXITCODE -eq 0)
}

if (-not $SkipBuild) {
    Step "Derleme (MyProjectEditor)" {
        $out = & $ue MyProjectEditor Win64 Development "-Project=$root\MyProject.uproject" -WaitMutex 2>&1
        ($out | Select-String "Result: Succeeded") -ne $null
    }
}

Step "Karakter JSON gidis-donus testi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestCharacterSaveJson.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Asenkron kayit onayli bolge gecisi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestZoneTravel.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

foreach ($map in @("Summer_scene", "Winter_scene")) {
    Step "Duman testi $map" {
        & powershell -NoProfile -File (Join-Path $PSScriptRoot "SmokeTest.ps1") -Map $map | Out-Host
        $LASTEXITCODE -eq 0
    }
}

if (-not $SkipNet) {
    Step "Cok oyunculu baglanti (NetSmokeTest)" {
        & powershell -NoProfile -File (Join-Path $PSScriptRoot "NetSmokeTest.ps1") | Out-Host
        $LASTEXITCODE -eq 0
    }
}

Write-Output ""
Write-Output "=== OZET ==="
$failed = 0
foreach ($k in $results.Keys) {
    Write-Output ("{0,-40} {1}" -f $k, $(if ($results[$k]) { "GECTI" } else { "KALDI" }))
    if (-not $results[$k]) { $failed++ }
}
exit $(if ($failed -eq 0) { 0 } else { 1 })
