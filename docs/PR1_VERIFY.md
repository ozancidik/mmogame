# PR #1 doğrulama rehberi (yerelde derleme + kısa test)

> Güncelleme: 14–17. adımlar çok oyunculu XP/ganimet mantığı için eklendi.

PR: [ozancidik/mmogame#1](https://github.com/ozancidik/mmogame/pull/1). **Kod değişikliklerinin hiçbiri derlenmedi** (bulutta Unreal/MSVC yok). Bu sayfa derleme sonrası neyi, hangi sırayla doğrulayacağını tek yerde toplar. Ayrıntılı maddeler `PLAYTEST_CHECKLIST.md` sonundaki bölümlerde.

## 1. Derle

```powershell
git fetch origin claude/determined-cannon-a8zts0
git checkout claude/determined-cannon-a8zts0
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64 Development "-Project=<proje yolu>\MyProject.uproject" -WaitMutex
powershell -File Scripts\RunAllChecks.ps1 -SkipBuild
```

Yeni dosyalar (`Core/MMONotify.h/.cpp`) eklendi: derleme "dosya bulunamadı"/bağlanmama derse **Generate Visual Studio project files** çalıştır.

### Derleme hatası olursa en olası yerler (benim tahminim)
| Dosya | Şüphe |
|---|---|
| `Core/MMOGameMode.h/.cpp` | `ChangeName(AController*, const FString&, bool)` ve `PreLogin(...)` imzaları UE 5.7'deki `AGameModeBase` ile birebir uyuşmalı. |
| `Core/MMONotify.cpp` | `AMMOPlayerController::ClientReceiveChat` erişimi; `Cast<AMMOPlayerController>` için include. |
| `Effects/MMOEffectTypes.h` | `UPROPERTY(NotReplicated)` struct üyesinde (UHT bunu kabul etmeli). |
| `Components/MMOInventoryComponent.cpp` | `ApplySaveData` içindeki `SanitizeInstance`/`Dedupe` lambdaları, `TSet<int32>`. |
| `Components/MMOStatsComponent.cpp` | `GameStateBase.h` include, `ApplyEffect` içindeki `EffectEnd`. |
| `Characters/MMOMob.cpp/.h` | `HandleDeath` içindeki XP payı bloğu (`TPair`/`TMap<TWeakObjectPtr<>,float>`, `AddShare` lambdası), ganimet `ReserveLoot` lambdası. |
| `World/MMOLootPickup.cpp/.h` | `FTimerHandle` include (`Engine/TimerHandle.h`), `TryCollect`, `GetOverlappingActors`. |

## 2. Hızlı davranış testi (2 pencere, listen server; ≈20 dk)

| # | Test | Beklenen | Değişiklik |
|---|---|---|---|
| 1 | Adsız gir (host + istemci), `/who` | Adlar `Gezgin######`, Windows kullanıcı adı **görünmez** | Y-5 |
| 2 | `?Name=Ozan` ile bağlan | Ad `Ozan` kalır | Y-5 |
| 3 | Aynı (küçük harf) adla ikinci bağlantı | "Bu ad şu an başka bir oyuncu tarafından kullanılıyor" | K-1 |
| 4 | Çevrimdışı bir kayıt adına `MMOSetName` + `MMOSave` | "Bu ad başka bir kayda ait: kaydedilmedi", eski `.sav` değişmez | K-1 |
| 5 | Mob öldür, iksir/ekipman al | Normal; host ekranında **başkasının** ganimet satırı görünmez | O11 |
| 6 | **Uzak istemcide**: çanta doluyken satın al / altın yetmeden satın al / görev al-teslim et | Mesajlar **istemcinin sohbetinde** sistem satırı olarak görünür | O11 |
| 7 | Buff al (`MMOBuff Haste`), host ve istemcide HUD kutusu | Kalan saniye doğru geri sayar, süre bitince kaybolur | Y3 |
| 8 | Hasar al, savaş dışında bekle | Can ~6/sn dolar (1'er puan), mana dolar, tavanı aşmaz | Y3 |
| 9 | Karakteri kaydet, çık, gir | Seviye, stat, altın, eşya, görev aynen geri gelir | Y-2 |
| 10 | Lv13/20/28'e `MMOGainXP` ile çık (her sınıf) | Yeni beceriler (Ezici Darbe, Ok Sağnağı, Buz Mızrağı, İlahi Hüküm, …) öğrenilebilir; `K` ekranında taşma yok | Beceri |
| 11 | Ölü iken satıcıyla işlem / yükseltme | Reddedilir | D3 |
| 12 | Sohbete sekme/U+202E içeren mesaj | Boşluğa dönüşür | D5 |
| 13 | `MMOGiveItem HealthPotion 3`, `MMOGainXP 5000` | Development'ta çalışır (Shipping'de çalışmaz) | Y-6 |
| 14 | Host bir moba canın ~%60'ını, istemci ~%40'ını vursun (istemci öldürsün) | XP yaklaşık %60 / %40 bölünür; ikisinin görev sayacı da 1 artar; `[MMOMETRIC] ev=kill` iki satır | XP payı |
| 15 | İstemci tek hafif vuruş yapıp çekilsin, host öldürsün | İstemci en az 1 XP alır, host ~%99 | XP payı |
| 16 | İki kişilik partiyle %50/%50 hasarla öldür | Her üye partinin payının yarısını alır (toplam %100; 8 kişide herkes 1/8) | Parti bölme |
| 17 | Host mob öldürsün, istemci hemen cesedin üstünden geçsin (hasarsız) | İlk 10 sn ganimet toplayamaz; ~10 sn sonra üstünde duruyorsa otomatik toplar | Ganimet |

İsteğe bağlı: parolalı sunucu (`-MMOPassword=test123`, istemci `?Password=test123`) ve NPC hız sınırı (`MaxRequestsPerMinute`'u 2 yapıp NPC'ye 3 kez tıkla).

## 3. Sonuç

- Derleme + `RunAllChecks.ps1` geçti, 1–13 geçti → PR'ı draft'tan çıkarıp birleştir.
- Başarısız adım varsa: hangi satır, ne gördün, log (`Saved/Logs/MyProject.log`, `[MMODBG]`/`Error:`) → bana yaz.
- Bu PR'ın kapsamı dışında: parti/ticaret bileşenleri (yerelde Codex'te), `Config/`, `Content/`, kayıt biçimi.
