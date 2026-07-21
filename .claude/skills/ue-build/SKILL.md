---
name: ue-build
description: Bu UE5 projesini güvenle derleyip editörü doğru haritayla yeniden açar. C++ değişikliğinden sonra derleme, "editör açıkken derlenmiyor" kilidi, ya da Play'de siyah ekran / boş harita sorunu yaşandığında kullan.
---

# UE Build & Relaunch Loop

Bu projenin C++ modülünü (`Source/MyProject`) derlemek ve editörü test için doğru haritayla açmak için standart, güvenli akış.

## Altın kurallar

- **Header (.h) değişince editör KAPALI olmalı** (Live Coding kilidi → "Unable to build while Live Coding is active"). Sadece .cpp gövde değişikliği için editör açıkken Live Coding (Ctrl+Alt+F11) kullanılabilir; ama garanti için kapalı derle.
- Derleme başarısız olursa log'u oku, kök nedeni bul, düzelt — sleep-döngüsüyle körlemesine tekrar deneme.

## Derleme (editörü kapat → derle)

PowerShell:

```
Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep -Seconds 3; & "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64 Development "-Project=C:\Users\OZAN CIDIK\Desktop\ozan online\MyProject\MyProject.uproject" -WaitMutex -FromMsBuild
```

`Result: Succeeded` görülmeli. Hata olursa çıktı Türkçe/İngilizce karışık olabilir; `error C####` satırlarına odaklan.

## Editörü yeniden aç

```
Start-Process "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList '"C:\Users\OZAN CIDIK\Desktop\ozan online\MyProject\MyProject.uproject"'
```

## ⚠️ Play'de SİYAH EKRAN / "harita açılmadı" sorunu

Editör ani kapatıldıktan sonra bazen **boş "Untitled" haritayla** açılır; Play'e basınca boş harita yüklenir → siyah ekran. Bu bir KOD hatası DEĞİLDİR.

**Teşhis:** `Saved/Logs/MyProject.log` içinde `LoadMap(/Temp/Untitled_0)` görürsen editör boş haritada demektir.

**Çözüm (kullanıcıya söyle):** İçerik Tarayıcısı → `HandPaintedEnvironment/Maps` → **Summer_scene**'e çift tıkla (varsayılan oyun haritası). Sonra Play. PlayerStart + NavMeshBoundsVolume o haritaya kayıtlı, tekrar eklemeye gerek yok.

Varsayılan harita `Config/DefaultEngine.ini` içinde `EditorStartupMap`/`GameDefaultMap` ile ayarlı: `/Game/HandPaintedEnvironment/Maps/Summer_scene`.

## Test notları

- Play şu an **2 pencere** açar (2 oyunculu ListenServer — parti/PvP testi için). Tek pencere isteniyorsa `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` içinde `PlayNumberOfClients=1`.
- Oynanışı Claude test edemez; test maddeleri `docs/PLAYTEST_CHECKLIST.md`'ye biriktirilir, kullanıcı doğrular.
