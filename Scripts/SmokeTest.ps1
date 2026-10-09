# Headless duman testi: haritayi -game ile goruntusuz baslatir, birkac saniye calistirir, log'u ozetler.
# Editor KAPALI olmali (derleme/DLL kilidi icin ayni kural). Kullanim:
#   powershell -File Scripts/SmokeTest.ps1 [-Map Summer_scene|Winter_scene] [-Cmds "MMOSpawnMob Goblin,quit"]
# Cikis 0 = hata/cokme yok. Satir sayilari: Error:, Fatal, Ensure, Access violation.
param(
    [string]$Map = "Summer_scene",
    [string]$Cmds = "MMOSpawnMob Goblin,quit"
)

$project = Join-Path (Split-Path -Parent $PSScriptRoot) "MyProject.uproject"
$exe = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$log = Join-Path $env:TEMP "mmo_smoke_$Map.log"
if (Test-Path $log) { Remove-Item $log }

$mapPath = "/Game/HandPaintedEnvironment/Maps/$Map/$Map"
$argList = @("`"$project`"", $mapPath, "-game", "-NullRHI", "-nosound", "-unattended", "-nosplash", "-stdout", "-ABSLOG=$log", "-ExecCmds=`"$Cmds`"")
$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit(280000)) { $p.Kill(); Write-Output "ZAMAN ASIMI (oyun kapanmadi)"; exit 2 }

$errs = (Select-String -Path $log -Pattern "Error:|Fatal error|Ensure condition|Access violation" | Measure-Object).Count
$spawn = (Select-String -Path $log -Pattern "SpawnOne:.*result=MMOMob" | Measure-Object).Count
Write-Output "Harita: $Map  Cikis: $($p.ExitCode)  Hata satiri: $errs  Mob dogumu: $spawn"
Select-String -Path $log -Pattern "layout spawned|skipping spawn|SpawnMobArea" | Select-Object -First 6 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(200, $_.Line.Length)) }

# Cep kalitesi (UYARI, cikis kodunu etkilemez): normal spawner 5+ deneme yapip yarisindan azini doguruyorsa cep
# engebeli/hatali olabilir. MMOProbeSurface 64/64 verip gercek doguma 1/6 veren bir cep olmustu (probe yalan soyler);
# gercek doguma sayisi olcuttur. Elit spawner'lar tek denemelik oldugu icin (60 sn'de bir yeniden dener) elenir.
$blocks = @(); $cur = $null
foreach ($line in (Get-Content $log)) {
    if ($line -match "Spawner BeginPlay @ (V\([^)]*\))") { $cur = @{ Loc = $Matches[1]; Tries = 0; Live = -1 }; $blocks += $cur }
    elseif ($cur -and $line -match "SpawnOne: requested") { $cur.Tries++ }
    elseif ($cur -and $line -match "Spawner after TopUp: live=(\d+)") { $cur.Live = [int]$Matches[1]; $cur = $null }
}
foreach ($b in $blocks) {
    # Spawner mob basina ayni turda en fazla 4 deneme yapar (cakismada baska noktada tekrar); normal cep 6 mob ister.
    # 5+ denemeye ragmen 4'ten az mob dogduysa cep zayiftir (iyi cepler ilk doldurmada 5-6 verir).
    if ($b.Tries -ge 5 -and $b.Live -ge 0 -and $b.Live -lt 4) {
        Write-Output "UYARI: cep $($b.Loc) ilk doldurmada yalniz $($b.Live) mob dogdu ($($b.Tries) denemede; zemin engebeli olabilir, MMOProbeSurface'e guvenme)"
    }
}
if ($errs -gt 0 -or $p.ExitCode -ne 0) { exit 1 } else { exit 0 }
