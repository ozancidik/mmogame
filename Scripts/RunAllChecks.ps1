# Tum otomatik kontrolleri sirayla calistirir ve ozet verir. Editor KAPALI olmali. Kullanim:
#   powershell -File Scripts/RunAllChecks.ps1 [-SkipBuild] [-SkipNet] [-FullCompile] [-NetPort 17779] (verilmezse bos rastgele port)
# Sirasi: veri denetimi -> derleme -> (istege bagli -FullCompile: PCH siz tek-dosya /W4) -> Summer duman -> Winter duman -> cok oyunculu baglanti.
# Cikis 0 = hepsi gecti.
param(
    [switch]$SkipBuild,
    [switch]$SkipNet,
    [switch]$FullCompile,
    [ValidateRange(1, 65535)]
    [int]$NetPort
)

$root = Split-Path -Parent $PSScriptRoot
$py = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
$ue = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat"
$results = [ordered]@{}
$netArgs = @()
if ($PSBoundParameters.ContainsKey("NetPort")) { $netArgs = @("-Port", "$NetPort") }

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

Step "Uretilmis gorev diyalogu Lore.md ile eslesiyor (GenQuestDialogue.py --check)" {
    & $py (Join-Path $PSScriptRoot "GenQuestDialogue.py") --check | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Statik C++ kontrolu (StaticLint.py; derleme degildir)" {
    & $py (Join-Path $PSScriptRoot "StaticLint.py") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Soak runner hata/cikis/log regresyonu (UE baslatmaz)" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestSoakTest.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "NetSmoke runner surec/log/cleanup regresyonu (UE baslatmaz)" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestNetSmokeRunner.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "PlaytestMetrics fixture testi (TestPlaytestMetrics.py)" {
    & $py (Join-Path $PSScriptRoot "TestPlaytestMetrics.py") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Gold log dogrulayici negatif fixture testi" {
    & $py (Join-Path $PSScriptRoot "ValidateGoldMetricLog.py") --self-test | Out-Host
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
            "Components\MMOStatsComponent.cpp", "Components\MMOSkillComponent.cpp", "Components\MMOPartyComponent.cpp", "Components\MMOClanComponent.cpp", "Components\MMOFriendsComponent.cpp",
            "Components\MMOQuestComponent.cpp", "Components\MMOTradeComponent.cpp", "Tests\MMOTradeSelfTest.cpp", "World\MMOLootPickup.cpp",
            "Player\MMOPlayerController.cpp", "UI\MMOHUD.cpp",
            "Characters\MMOMob.cpp", "AI\MMODialogueSubsystem.cpp"
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

Step "Possession bitisinde karakter kaydi ve timer kapilari" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestSaveLifecycle.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Arkadas presence possession bitisi ve yeniden giris" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFriendsPresence.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Arkadas komutlarinda gonderim pawn baglami" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFriendsChatContext.ps1") | Out-Host
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

Step "Gorev odulunde yigin ve canta kapasitesi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestQuestRewardCapacity.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Gorev odulunde senkron delegate tekrar teslimi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestQuestRewardCallbacks.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ilk gorev kontrolcu oturumu ve tekillik" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFirstQuestMetrics.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ilk sunucu hareket gozlemi ve kontrolcu tekilligi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFirstMove.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ilk gercek XP seviye gecisi ve kontrolcu tekilligi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFirstLevelUp.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ilk oyuncu olumu ve gercek XP cezasi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFirstDeath.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ilk oldurme kredisi ve gercek olum akisi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFirstKill.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ilk kusanma istegi ve otomatik kurulum ayrimi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFirstEquip.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ilk kabul edilen sunucu becerisi ve ret kapilari" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFirstSkill.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Beceri health callback sonrasi ek etki ve alan devamligi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestSkillEffectContinuation.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Beceri hedefinin EndPlay ve mana callback yasam dongusu" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestSkillTargetLifecycle.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Beceri caster EndPlay ve mana callback yasam dongusu" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestSkillLifecycle.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Beceri hedefinin ic ice cagrida korunmasi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestSkillTargets.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Beceri mana/can callback tekrar kullanimi ve cooldown" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestSkillCallbacks.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ilk kabul edilen temel saldiri ve ret kapilari" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFirstBasicAttack.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "DoT kaynak yok olusu ve reentrant pending tick" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestDotSourceLifecycle.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Temel saldiri tek cooldown kapisi ve uzun oturum" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestMeleeCooldown.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ganimet toplamada oluluk ve tek sefer grant" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestLootLifeState.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ganimet envanter callback yeniden giris" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestLootCallbacks.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}
Step "Saf iksir faydasi ve ortak bekleme suresi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestConsumableBenefit.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Ilk ganimet gercek teslim miktari ve tekillik" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestFirstLoot.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Kayittan ekipman seviye sarti ve kayipsiz kurtarma" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestSavedEquipmentLevel.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Kayittan stat XP ve puan butcesi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestStatsSaveBudget.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Seviye/refill callback XP odullerinin kayipsiz islenmesi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestReentrantExperience.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Mastery ve sinif iadesinde toplam puan butcesi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestMasteryBudget.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Satici ekonomi kapasite ve yetki kapilari" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestVendorEconomy.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Satin alma delegate odeme ve teslim sirasi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestPurchaseCallbacks.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Gold callback telemetri kaynak miktar bakiye ve sira" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestGoldMetrics.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Yukseltme altin delegate esya kimligi ve snapshot" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestUpgradeCallbacks.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Takas revizyon onay ve ekonomi butunlugu" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestTrade.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Takas kapanis delegate oturum ve yasam dongusu" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestTradeTeardown.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Takas baslangic delegate ve yasam dongusu" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestTradeStart.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}
Step "Takas onay callback baglami" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestTradeConfirm.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Takas teklif callback baglami" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestTradeOffer.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Takas aktarimi callback ve esya kimligi butunlugu" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestTradeTransfer.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Kayitli ogrenilmis becerilerin mastery butcesi" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestLearnedSkillBudget.ps1") | Out-Host
    $LASTEXITCODE -eq 0
}

Step "Diyalog LLM kota rezervasyon ve iade sinirlari" {
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "TestDialogueBudget.ps1") | Out-Host
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
        & powershell -NoProfile -File (Join-Path $PSScriptRoot "NetSmokeTest.ps1") @netArgs | Out-Host
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
