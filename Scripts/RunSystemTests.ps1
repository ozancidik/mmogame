# Basarim, gunluk gorev, posta ve ad kilidi native testlerini tek komutta calistirir (derlemeden SONRA; editor kapali olmali).
# Ayni dort calistirici Scripts/RunAllChecks.ps1'de de adimdir; bu betik yalniz bu dordunu hizlica kosmak icindir.
# Kullanim: powershell -File Scripts\RunSystemTests.ps1
$ErrorActionPreference = 'Stop'
$tests = @(
    # Kontrol sayisi esikleri her calistiricinin icinde (96 / 81 / 130 / 58); kalicilik testleri Saved/ altina GUID'li dosya yazip siler.
    @{ Name = 'Basarim ve unvan (TestAchievements)'; Script = 'TestAchievements.ps1' },
    @{ Name = 'Gunluk gorev (TestDaily)';            Script = 'TestDaily.ps1' },
    @{ Name = 'Posta (TestMail)';                    Script = 'TestMail.ps1' },
    @{ Name = 'Ad kilidi (TestNameLock)';            Script = 'TestNameLock.ps1' }
)
$failed = @()
foreach ($t in $tests) {
    Write-Output ">> $($t.Name)"
    & powershell -NoProfile -File (Join-Path $PSScriptRoot $t.Script) | Out-Host
    if ($LASTEXITCODE -eq 0) { Write-Output "   -> GECTI" } else { Write-Output "   -> KALDI"; $failed += $t.Name }
}
Write-Output ''
Write-Output '=== OZET ==='
foreach ($t in $tests) {
    $s = if ($failed -contains $t.Name) { 'KALDI' } else { 'GECTI' }
    Write-Output ("{0,-42} {1}" -f $t.Name, $s)
}
if ($failed.Count -gt 0) { exit 1 }
exit 0
