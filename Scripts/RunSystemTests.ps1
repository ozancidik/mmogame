# Basarim, gunluk gorev ve posta sistemlerinin native testlerini tek komutta calistirir (derlemeden SONRA; editor kapali olmali).
# RunAllChecks.ps1 Codex ile ortak oldugu icin bu testler orada adim degil; Codex dosyayi commit'leyince oraya tasinabilir.
# Kullanim: powershell -File Scripts\RunSystemTests.ps1
$ErrorActionPreference = 'Stop'
$tests = @(
    @{ Name = 'Basarim ve unvan (TestAchievements)'; Script = 'TestAchievements.ps1' },
    @{ Name = 'Gunluk gorev (TestDaily)';            Script = 'TestDaily.ps1' },
    @{ Name = 'Posta (TestMail)';                    Script = 'TestMail.ps1' }
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
