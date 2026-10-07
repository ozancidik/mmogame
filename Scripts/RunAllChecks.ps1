# Tum otomatik kontrolleri sirayla calistirir ve ozet verir. Editor KAPALI olmali. Kullanim:
#   powershell -File Scripts/RunAllChecks.ps1 [-SkipBuild] [-SkipNet] [-FullCompile]
# Sirasi: veri denetimi -> derleme -> (istege bagli -FullCompile: PCH siz tek-dosya /W4) -> Summer duman -> Winter duman -> cok oyunculu baglanti.
# Cikis 0 = hepsi gecti.
param(
    [switch]$SkipBuild,
    [switch]$SkipNet,
    [switch]$FullCompile
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

# PCH'siz tek-dosya /W4 derleme (adaptive unity / eksik include yakalar). Normal derleme PCH+unity
# kullandigi icin eksik #include'lari gizler (ORTA-2: MMOCharacterSaveJson.cpp). Sabit liste: en cok
# degisen dosyalar + Save/*.cpp. Varsayilan KAPALI (dosya basina ~5-60 sn); CI/manuel: -FullCompile.
if ($FullCompile) {
    Step "PCH'siz tek-dosya /W4 derleme (-FullCompile)" {
        $src = Join-Path $root "Source\MyProject\Private"
        $files = @(
            "Characters\MMOCharacter.cpp", "Core\MMOGameMode.cpp", "Components\MMOInventoryComponent.cpp",
            "Components\MMOStatsComponent.cpp", "Components\MMOPartyComponent.cpp", "Components\MMOClanComponent.cpp",
            "Components\MMOQuestComponent.cpp",
            "Player\MMOPlayerController.cpp", "UI\MMOHUD.cpp",
            "Characters\MMOMob.cpp"
        ) | ForEach-Object { Join-Path $src $_ }
        $files += (Get-ChildItem (Join-Path $src "Save") -Filter *.cpp | ForEach-Object { $_.FullName })
        $allOk = $true
        $totalWarn = 0
        foreach ($f in $files) {
            $out = & $ue MyProjectEditor Win64 Development "-Project=$root\MyProject.uproject" -WaitMutex -NoPCH -NoLink "-SingleFile=$f" 2>&1
            $ok = ($out | Select-String "Result: Succeeded") -ne $null
            $warn = @($out | Select-String "warning C\d+").Count
            $totalWarn += $warn
            Write-Host ("   {0,-34} {1}  (warning C: {2})" -f (Split-Path -Leaf $f), $(if ($ok) { "OK" } else { "HATA" }), $warn)
            if (-not $ok) {
                $allOk = $false
                $out | Select-String "error C" | Select-Object -First 5 | ForEach-Object { Write-Host ("      " + $_.Line) }
            }
        }
        Write-Host "   toplam 'warning C': $totalWarn"
        $allOk
    }
}

Step "Karakter JSON gidis-donus testi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestCharacterSaveJson.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Karakter magazasi (yerel .sav) okuma + gidis-donus testi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestCharacterStore.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Asenkron kayit onayli bolge gecisi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestZoneTravel.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Bolge gecisi kayit zaman asimi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestZoneTravelTimeout.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Bolge gecisi beklerken oyuncu durumu degisimi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestZoneTravelStateChange.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Bolge gecisi ertelenen kayit hatasi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestZoneTravelDeferredSave.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Bolge gecisi snapshot sonrasi XP/altin" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestZoneTravelProgress.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Parti birlesme lider onayi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestPartyConsent.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Bolge gecisi callback bariyeri" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestZoneTravelCallbacks.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Klan daveti kabul aninda lider yetkisi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestClanInviteAuthority.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Gorev odulunde oluluk ve tek sefer teslim" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestQuestRewardLifeState.ps1") | Out-Host
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
