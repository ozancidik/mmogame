# S01.01 — Build hedefleri ve ortak UE kilidi

Durum: **betik paketi doğrulandı, ana checkout'a entegre edildi**. S01 dedicated sprinti tamamlanmadı; gerçek Editor/Server build ve kapasite kabulü açık.

## Sahiplik ve değişiklik

- İzole uygulama tabanı: `4e0f0c5`; worktree `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject`.
- DevOps yazarı: Wegener `01a13fdd-f402-70e1-89fb-88abf0c4adef`, runtime `default`.
- Bağımsız QA/network-security: Franklin `01a13fe0-dc73-77b3-a84c-3352cc3a5d65`, runtime `default`, salt okunur. Son frozen sürüm kabul edildi, somut engel bulunmadı.
- Kod kapsamı yalnız `Scripts/build.ps1` ve yeni `Scripts/TestBuildScript.ps1`. Erdos'un S00 dosyaları, staged O70 takası ve ana klasördeki başka araca ait ırk kodları alınmadı.

`build.ps1` artık gerçek `.Target.cs` dosyalarındaki `TargetType` tanımlarını seçer. Olmayan Client isteği önkoşul/kilit/temizleme/derleme başlamadan reddedilir; All gerçek Editor/Game/Server hedeflerini kapsar. Ortak Git dizinindeki `agent-build.lock` handle'ı build/clean boyunca tutulur ve finally ile bırakılır. Meşgul kaynaklar DEFER/exit 3 verir; dotnet üzerinden UBT dahil süreç ön kontrolü hiçbir yabancı süreci kapatmaz. Temizleme izin listesi ve ancestor/descendant reparse kontrolü Saved'e dokunmayı engeller; EnginePath parametresi korunur.

Frozen SHA256:

- build.ps1: `31382CF55F8A8695DAE2434C02F4C7A0DA5D65BA98FF47E7DA4AC859021E9EDB`
- TestBuildScript.ps1: `280470342DD5649A0E442F3E4CC10F48C217579F2138259B34FF302C4B849F2C`

## Gerçek doğrulama

Yazarın ilk 72 kontrolüne, incelemede bulunan boşluk için 28 kontrol eklendi. Ek kontroller üretim `Clean-Build` gövdesini çağırır; `Remove-Item` yalnız literal hedefleri kaydeden mock'tur. Hiçbir gerçek temizleme yapılmaz. İzinli klasörler, Saved dışlama, linked ancestor/descendant retleri, dosya sistemi okuma hataları ve değişmeyen fixture envanteri sınanır.

Koordinatör frozen hash'leri doğruladı ve aşağıdaki komutları izole worktree'de bağımsız çalıştırdı:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Scripts/TestBuildScript.ps1
pwsh.exe -NoProfile -File Scripts/TestBuildScript.ps1
```

İkisi de **100/100 PASS, exit 0**. Ana checkout'a byte olarak aktarımdan sonra Windows PowerShell 5.1 testi de **100/100 PASS, exit 0**. Diff whitespace kontrolü temiz.

Kanıt logları (`C:/Users/OZAN CIDIK/AppData/Local/Temp`):

- `s01-parent-ps51-849dfcb674084b45af65015ffa635c60.log`
- `s01-parent-pwsh-1985d6a319704533ab95e00698c61576.log`
- `s01-main-ps51-0ff9093f25b142dc8568f5a816d1f5f0.log`

UE çalıştırılmadı, gerçek Clean yapılmadı, oyuncu Saved verisi silinmedi. Bu sonuç gerçek Unreal derlemesi, Windows ACL/yaris koşulları, dedicated bağlantısı veya PIE başarısı değildir.

## Sonraki iş

S00.02: NetSmoke child'ına izole UserDir, açık log/süreç sahipliği ve sınırlı watchdog desteği; mock regresyon ve bağımsız kabul sonrası tek UE sahibiyle taze izole Editor build + Winter ağı. S01 için kaynak motor/dedicated ikilisi ve iki süreç kapasitesi ayrıca açık kalır.
