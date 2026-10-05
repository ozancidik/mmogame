# Güvenlik Denetimi (Salt Okunur) - MyProject MMORPG

Tarih: 2026-10-04. Yöntem: yalnızca statik inceleme (kod okuma, grep, git geçmişi taraması, motor kaynağı doğrulaması). Hiçbir dosya düzenlenmedi, derleme/commit/push yapılmadı, oyun çalıştırılmadı. Bu dosya tek yazma çıktısıdır.

Kapsam: `Source/MyProject/**` (69 dosya, yaklaşık 15 bin satır .cpp), `Config/*.ini`, `Scripts/*`, `.claude/`, `.gitignore`, git geçmişi (236 commit), `Saved/SaveGames`, `Saved/Logs`.

Not: Kod, denetim sırasında başka bir ajan tarafından eş zamanlı değiştiriliyordu (satır numaraları denetim anına göre yaklaşık değerdir, tipik kayma +/- 10 satır). "doğrulanmalı" işareti, koddan çıkarılmış ama çalıştırılarak teyit edilmemiş maddeleri gösterir.

Gizlilik kuralı: Gerçek sır değerleri bu raporda yazılmaz (en fazla ilk 4 karakter).

---

## 1. Özet Tablo

| No | Ciddiyet | Başlık | Konum |
|----|----------|--------|-------|
| K-1 | KRİTİK (herkese açık oturum) / YÜKSEK (LAN) | Kimlik doğrulama yok: kayıt yuvası istemcinin seçtiği ada bağlı; karakter ezme, çalma ve kopyalama | `MMOCharacter.cpp` GetSaveSlotName/ServerSetName/EndPlay, motor `GameModeBase.cpp:815` |
| Y-2 | ORTA | SaveGame yüklemede eksik doğrulama (ana statlar, eşyalar, görevler, klan) ve dosya imzasız | `MMOStatsComponent.cpp:802`, `MMOInventoryComponent.cpp:752-790`, `MMOQuestComponent.cpp:389`, `MMOClanComponent.cpp:567` |
| Y-3 | ORTA | API anahtarları kalıcı kullanıcı ortam değişkeni; AI geliştirme ajanları bypassPermissions ile çalışıyor | kullanıcı ortamı, `.claude/settings.local.json`, global `settings.json` |
| Y-4 | ORTA | LLM maliyet/kota suistimali (genel bütçe sınırı yok, `MaxTokens=2048`) | `MMODialogueSubsystem.h:81`, `MMOPlayerController.cpp:1166-1223` |
| Y-5 | ORTA | PII sızıntısı: varsayılan oyuncu adı işletim sistemi kullanıcı adını içeriyor, herkese görünür | `Saved/SaveGames/MMOSave_CIDIK-*.sav` (59 dosya) |
| Y-6 | ORTA | Debug Exec komutları ve debug RPC gövdeleri Shipping'de derleniyor (yalnızca host'a kapalı) | `MMOCharacter.h/.cpp` (MMOGiveItem, MMOSpawnMob, MMOBuff, MMOPvP...) |
| Y-7 | DÜŞÜK-ORTA | Sunucu DoS yüzeyleri: sohbet uzunluğu/throttle, `/klan kur` senkron disk taraması, `MMOSetName` cooldown'u yok | `MMOPlayerController.cpp:128-360`, `MMOClanComponent.cpp:46-62,250`, `MMOCharacter.cpp:665` |
| Y-8 | DÜŞÜK-ORTA | Envanter/altın/ana stat verisi tüm istemcilere replike ediliyor (bilgi ifşası) | `MMOInventoryComponent.cpp:31-35`, `MMOStatsComponent.cpp:77-92` |
| D-9 | DÜŞÜK | Kullanıcı dizgelerinde kontrol/bidi karakter ve homoglif denetimi yok | `?Name=` yolu, klan adı, arkadaş adı |
| D-10 | DÜŞÜK | HTTP hata gövdesi loglanıyor (anahtar yankısı olasılığı, doğrulanmalı) | `MMODialogueSubsystem.cpp:170` |
| D-11 | DÜŞÜK | `DefaultEngine.ini` içinde commit'li AndroidFileServer `SecurityToken` | `Config/DefaultEngine.ini:89` |
| D-12 | DÜŞÜK | `.gitignore` boşlukları (`.idea/`, `*.sav`, `.env`, `.claude/settings.local.json` yalnızca kullanıcı-global ignore'a bağlı) | `.gitignore` |
| B-13 | BİLGİ | Ağ mimarisi: parolasız listen server, dedicated server hedefi yok, `bAllowPvP=true` varsayılanı | `NetSmokeTest.ps1`, `MMOGameMode.h:40` |

Bulunmayanlar (olumlu sonuçlar): repoda ve git geçmişinde gerçek API anahtarı/token yok; FString::Printf'e kullanıcı verisinin biçim dizgisi olarak geçtiği yer yok; yol enjeksiyonu (`../`) mümkün değil; NPC LLM'ine oyuncu serbest metni gitmiyor (prompt injection sömürülemiyor).

---

## 2. Bulgular (Ayrıntılı)

### K-1. Kimlik doğrulama yok: kayıt yuvası = istemcinin seçtiği ad

Dosya:satır (yaklaşık):
- `Source/MyProject/Private/Characters/MMOCharacter.cpp:407` `GetSaveSlotName()` (yuva adı `MMOSave_` + PlayerState adı)
- `MMOCharacter.cpp:665` `ServerSetName_Implementation` (herhangi bir istemci, cooldown'suz, adı değiştirebilir)
- `MMOCharacter.cpp:374` `EndPlay` (çıkışta otomatik `SaveCharacter()`), `MMOCharacter.cpp:627` `ServerSaveCharacter_Implementation` (10 sn cooldown)
- Motor doğrulaması: `C:\Program Files\Epic Games\UE_5.7\Engine\Source\Runtime\Engine\Private\GameModeBase.cpp:815` `InitNewPlayer`: bağlantı URL'indeki `?Name=` seçeneği `.Left(20)` ile doğrudan oyuncu adı olur; `ChangeName` (satır 1416) benzersizlik veya karakter denetimi yapmaz.
- Proje `MMOGameMode`'da `PreLogin`/`Login`/`ChangeName` override'ı yok (grep sonucu boş).
- Kendi yorumu da riski kabul ediyor: `MMOCharacter.h` içindeki `MMOSetName` açıklaması ("Kalici bir kimlik sistemi olmadan tam duzeltilemez").

Ciddiyet: KRİTİK, oyun host'u tanımadığı kişilere açıksa; YÜKSEK, yalnızca tanıdık kişilerle LAN/Hamachi testinde.

Sömürü senaryoları:
1. Ezme (hasar verme): Mallory bağlanır, `MMOSetName Victim` yazar (Victim çevrimdışıysa isim çakışma denetimini geçer), sonra `MMOSave` veya sadece oyundan çıkar. `EndPlay` -> `SaveCharacter()` yuvayı Victim adına yazar; Victim'in gerçek karakteri Mallory'nin karakteriyle kalıcı olarak silinir. Yedek/sürüm yok.
2. Çalma: Mallory `open <host>:7777?Name=Victim` ile bağlanır (motor kodu adı doğrulamadan kabul eder). `AutoLoadDeferred` -> `LoadCharacter` Victim'in kaydını yükler: seviye, ekipman, altın, klan liderliği Mallory'ye geçer; ticaretle başka karaktere aktarılabilir. (Bağlantı URL'indeki `Name` yolu koddan ve motor kaynağından doğrulandı; uçtan uca oyun testi yapılmadı, doğrulanmalı.)
3. Kopyalama (duplikasyon): Aynı slot adıyla iki oyuncu aynı anda çevrimiçi olabilir: `ServerSetName` çakışmayı denetler, `?Name=` yolu denetlemez. İki bağlantı da aynı kaydı yükler; biri eşyaları ticaretle verirken diğeri hâlâ elinde tutar. `RequestZoneTravel` yalnızca bölge geçişinde yinelenen yuvayı reddeder (`MMOGameMode.cpp` yaklaşık satır 125), o zamana kadar pencere açık kalır.

Düzeltme (öncelik sırasıyla):
- Slot adını görünen addan ayır. Sunucuda saklanan kalıcı kimlik kullan: ilk girişte sunucu 128 bit rastgele "karakter anahtarı" üretip istemciye verir (istemci yerel dosyada saklar), yuva = `MMOSave_` + SHA-256(anahtar) ilk 16 hex. Ya da gerçek hesap/OSS `UniqueNetId`.
- `AMMOGameMode::PreLogin` veya `Login` override et: aynı kimlikle ikinci bağlantıyı reddet; `?Name=` değerini yok say/temizle; görünen adı ayrıca `ChangeName` override'ında doğrula.
- `ServerSetName` yalnızca görünen adı değiştirsin, yuvayı değiştiremesin; çağrıya 30 sn cooldown ekle.
- Yazmadan önce `.bak` döndür (son 3 sürüm), ezme olayını geri alınabilir yap.

---

### Y-2. SaveGame yükleme doğrulaması eksik, dosya imzasız

Sömürü ön koşulu: kayıt dosyasını değiştirebilmek (host makinesi, paylaşılan/bulut senkronize `Saved/`, ileride dedicated sunucuya taşınan kayıtlar, veya K-1 ile başka birinin yuvasına yönlendirme). Dosya biçimi düz UE SaveGame (binary ama imzasız, dizgeler okunabilir).

Doğrulama yapılmayanlar:
- `MMOStatsComponent.cpp:802-805`: `Strength/Dexterity/Intelligence/Vitality = In.*` hiçbir sınır/toplam kontrolü olmadan kopyalanıyor (seviye başına verilebilecek toplam puanla karşılaştırma yok). Kodun kendi yorumu bunu kabul ediyor (`RecalculateDerivedStats` içinde: "ApplySaveData hicbir dogrulama yapmadan kopyaliyor"). Düzenlenmiş kayıt ile `Vitality=2000000000` gibi değerler sınırsız can/saldırı verir.
- `MMOInventoryComponent.cpp:752-790`: `Gold` yalnızca `Max(0)` (üst sınır yok); `Items` içinde eşya kimliğinin veritabanında var olup olmadığı ve `Count` üst sınırı denetlenmiyor; `EquipmentBag` için `IsValidInstance()`/`ItemId` varlığı/`Rarity` enum aralığı denetlenmiyor (`Warehouse` için var); `EquippedItems[i]` öğesinin gerçekten `i` slotuna ait olduğu ve `RequiredLevel`'ın sağlandığı yüklemede doğrulanmıyor (normal `EquipFromBag` bunları denetler); yinelenen `InstanceId` engellenmiyor; dizi uzunluğu sınırsız (replikasyon şişmesi, bellek).
- `MMOQuestComponent.cpp:389-390`: `ActiveQuests`/`CompletedQuests` olduğu gibi alınıyor. Düzenlenmiş `KillProgress` ile görev anında "tamam" görünür ve `ClaimReward` ödülleri verir (XP, altın, ekipman); `CompletedQuests` ile ön koşullar atlanır; veritabanında olmayan kimlikler birikir.
- `MMOClanComponent.cpp:567-640`: klan adı yalnızca `IsValidClanName` (uzunluk + alfasayısal) ile doğrulanıyor; kayıtta yazan klana davet olmadan üye olunur. `bClanLeader=true` ve o klanın çevrimiçi gerçek lideri yoksa kayıt sahibi lider olur (lider çevrimdışıyken klan devralma).
- `Friends` yalnızca uzunluk/yineleme denetimi alıyor.

Pozitif: Level, Experience, UnspentStat/SkillPoints, mastery, öğrenilmiş yetenekler, cooldown'lar, Warehouse ve konum güvenli şekilde sıkıştırılıyor/denetleniyor; bozuk yükleme mevcut dosyayı ezmiyor (`bInitialLoadComplete`).

Düzeltme:
- Tek bir `SanitizeSaveData(FMMOCharacterSaveData&)` fonksiyonu: ana stat toplamı <= taban + (Level-1) * puan/seviye; negatif/aşırı değerleri sıfırla; `Gold <= GoldCap`; `Items` yalnızca bilinen kimlikler ve `Count <= 999` ve dizi <= N; `EquipmentBag/EquippedItems` için `IsValidInstance + Find(ItemId) + slot eşleşmesi + RequiredLevel + Plus/Rarity aralığı + benzersiz InstanceId`; görevler yalnızca bilinen kimlik, `KillProgress <= TargetCount`, ön koşul zinciri tutarlı; klan için sunucu tarafı klan kaydı (var olan klana yalnızca kayıtlı üyelik tablosundan katılım).
- Dosya içine sürüm numarası + HMAC (anahtar sunucuda, repoda değil) ekle; imza uyuşmazsa yüklemeyi reddet ve olayı logla.

---

### Y-3. API anahtarı kalıcı ortam değişkeninde; geliştirme ajanları geniş yetkiyle çalışıyor

Gözlem (değerler okunmadı/yazılmadı, yalnızca varlık kontrolü yapıldı):
- `NVIDIA_API_KEY`: kullanıcı düzeyinde kalıcı (HKCU\Environment) ve bu oturumun süreç ortamında tanımlı. `ANTHROPIC_API_KEY`: kullanıcı düzeyinde kalıcı tanımlı.
- Kodda anahtar doğru şekilde yalnızca ortamdan okunuyor (`MMODialogueSubsystem.cpp:16-25`), loga değeri yazılmıyor (satır 64 yalnızca değişken adını yazıyor), istemciye giden yol yok (anahtar yalnızca sunucuda HTTP başlığına konuyor, satır 135/140).
- Risk: ortam değişkeni, o kullanıcı altında çalışan her sürece görünür: Unreal Editor, derleme betikleri, Python betikleri ve AI ajanları. Genel `~/.claude/settings.json`: `defaultMode=bypassPermissions`, `skipDangerousModePermissionPrompt=true`. `.claude/settings.local.json`: sınırsız `PowerShell`, `Agent`, `Bash(gh auth *)`, `Bash(gh api *)`, `Bash(curl ...)`, `WebFetch(domain:github.com)`; ayrıca üçüncü taraf bir depodan `bash scripts/install.sh --tool claude-code` çalıştırılmış (çok sayıda üçüncü taraf ajan tanımı kurulmuş). Ajanlar GitHub'dan (Open-KO, karpathy vb.) güvenilmeyen içerik okuyor; bu içerikte gizli talimat olursa (dolaylı prompt injection) onay istemeden komut çalıştırıp ortam değişkenini okuyup dışarı gönderebilir. Bu, doğrulanmış bir sömürü değil, kontrol zayıflığıdır (doğrulanmalı: pratikte ağ çıkışı kısıtı var mı).

Sömürü senaryosu: güvenilmeyen bir README/ajan tanımı "ortamdaki anahtarı şu adrese gönder" der; ajan bypassPermissions ile `curl` veya `Invoke-WebRequest` çalıştırır.

Düzeltme:
- Anahtarı kalıcı kullanıcı ortamından kaldır; yalnızca oyunu/ sunucuyu başlatan betikte süreç kapsamında ver (örn. Windows Credential Manager'dan okuyup yalnızca o sürece ekleyen `Scripts/StartServer.ps1`). Mevcut anahtarları, ajan oturumlarında göründükleri için rotasyona almayı değerlendir (NVIDIA ve Anthropic panelinden yenile). Yeni anahtarlara harcama limiti koy.
- `defaultMode=bypassPermissions`'ı kapat veya en azından `permissions.deny` ile `Bash(printenv*)`, `Bash(env*)`, `PowerShell(Get-ChildItem Env:*)`, `PowerShell(Invoke-WebRequest*)`, `Bash(curl*)` (yazma/POST) reddet. Genel `PowerShell` iznini kaldırıp gerekli komutları tek tek listele.
- Üçüncü taraf ajan/skill paketlerini kurmadan önce içeriğini incele; güvenilmeyen depo içeriğini okuyan oturumlarda ağ erişimini kapat.

---

### Y-4. LLM maliyet / kota suistimali

Konum: `MMOPlayerController.cpp:1166-1223` (`ServerRequestNpcDialogue_Implementation`), `MMODialogueSubsystem.h:81` (`MaxTokens = 2048`, varsayılan Anthropic modeli `claude-opus-5`), `MMODialogueSubsystem.cpp:81/107`.

Mevcut koruma: PlayerController başına 2 sn sunucu cooldown'u ve 700 birim mesafe denetimi (iyi).

Eksikler:
- Genel (oturum/gün) bütçe veya eşzamanlı istek sınırı yok. Cooldown denetleyici başına tutulduğu ve kimlik doğrulama olmadığı için (K-1) çok sayıda bağlantı/yeniden bağlantı ile istek hızı çarpılır; her istek faturalanır (Anthropic seçilirse gerçek para; NVIDIA ücretsiz katmanda kota tükenmesiyle özelliğin kapanması).
- `MaxTokens=2048` "en fazla 2 cümle" isteyen bir replik için aşırı yüksek (NVIDIA yolunda düşünme yok; maliyet tavanı gereksiz yüksek).
- Model çıktısı sınırsız uzunlukta güvenilir RPC (`ClientReceiveNpcDialogue`) ile yollanıyor; kontrol karakteri/uzunluk süzgeci yok.

Prompt injection değerlendirmesi (sonuç: sömürülemiyor):
- Prompt'a giren dizgeler: `NpcName`, `NpcRole` (tasarımcı tanımlı, oyuncu yazamaz) ve `PlayerInfo` (`"seviye %d bir maceraci"`, sunucu tarafında üretilen sayı). Oyuncu adı, sohbet, görev metni LLM'e gitmiyor. Sistem istemi ile kullanıcı iletisi ayrı rollerde gidiyor. Araç/fonksiyon çağrısı yok. Çıktı yalnızca isteyen oyuncunun HUD balonuna gidiyor; sunucu mantığını (altın, eşya, görev) etkilemiyor.
- Uyarı: ileride oyuncu serbest metni (sohbet, ad, görev notu) bu prompt'a eklenirse gerçek bir prompt injection yüzeyi doğar; o zaman metin kullanıcı rolünde, uzunluk sınırlı ve süzgeçli geçmeli, çıktı hiçbir zaman oyun mantığına bağlanmamalı.

Düzeltme: genel token-bucket (ör. dakikada 20 istek, günlük tavan, eşzamanlı en fazla 3); `MaxTokens` 150-256; çıktıyı 240 karakterle kırp, kontrol/bidi karakterlerini sil; Anthropic seçiliyken daha küçük model; kota hatasında (429) geri çekilme.

---

### Y-5. PII sızıntısı: oyuncu adı işletim sistemi kullanıcı adını içeriyor

Kanıt: `Saved/SaveGames/` altında 59 adet `MMOSave_CIDIK-<14 hex>.sav` var. `MMOSetName` kullanılmadığında varsayılan ad (çevrimiçi alt sistem "Null" kaynaklı) Windows kullanıcı adı + rastgele sonek. Bu ad `PlayerState`'e yazılıyor ve herkese görünüyor (`/who`, sohbet gönderen etiketi, klan listesi).

Etkiler:
- Kullanıcı adı (gerçek ad olabilir) tüm oyunculara açılıyor.
- Her oturumda yeni rastgele ad = yeni kayıt yuvası: ilerleme sürekli kayboluyor ve diskte yetim .sav birikiyor (şu an 59). `SavedClanNameInUse` (Y-7) bunların hepsini her `/klan kur` çağrısında yüklüyor.

Düzeltme: istemci tarafında OS kullanıcı adını bağlantı seçeneğine koyma (`GameInstance::GetGameLoginOptions`'ı override edip `Name=Gezgin####` üret veya oyuncuya ad seçtir); K-1'deki kalıcı kimlikle birlikte adı yuvadan bağımsız yap.

---

### Y-6. Debug Exec komutları Shipping'de derleniyor

Gözlem:
- Yalnızca `MMOGameMode.h` (`MMOProbeSpawn/MMOProbeSurface`) `#if !UE_BUILD_SHIPPING` ile korunuyor. `grep UE_BUILD_SHIPPING|WITH_EDITOR` başka sonuç vermedi.
- Korumasız Exec komutları: `MMOGiveItem`, `MMOGainXP`, `MMODamage`, `MMOHeal`, `MMOUseMana`, `MMOBuff`, `MMOSpawnMob`, `MMOPvP`, `MMOClass`, `MMOLoad`, `MMONpcTalk` vb. (`MMOCharacter.h` satır 86-285).
- Pozitif: uzak istemci tarafı güvenli. `ServerGiveItem` (satır ~1613), `ServerSpawnMob` (~1393), `ServerApplyBuff` (~1163), `ServerSetPvP` (~1126), `ServerLoadCharacter` (~640) `IsLocalController()` ile yalnızca host'a açık; `UMMOStatsComponent` değiştiricileri `HasAuthorityInternal()` ile korunuyor, dolayısıyla uzak istemcide `MMOGainXP/MMODamage/MMOHeal` etkisiz. `ServerGiveItem` `Count`'u 1-999 sıkıştırıyor.
- Kalan risk: (a) host, listen-server modelinde otorite olduğu için tüm komutları kullanabilir. Paylaşılan bir ekonomide (host = oyuncu) bu hile yüzeyidir. (b) Shipping'de konsol motor tarafından normalde kapalıdır (`ALLOW_CONSOLE`), ama `DefaultInput.ini`'de `ConsoleKeys=Tilde` ve `bShowConsoleOnFourFingerTap=True` var; Shipping paketinde gerçekten kapalı olduğu doğrulanmalı. (c) Kötü niyetli değiştirilmiş istemci, RPC gövdelerindeki `IsLocalController()` kapılarına güvenmek zorunda; bunlar doğru ama tek savunma.

Düzeltme: debug Exec fonksiyonlarını ve ilgili `Server*_Implementation` gövdelerini `#if !UE_BUILD_SHIPPING` (veya `WITH_EDITOR`) ile sar; ya da GameMode'da `bool bAllowDebugCommands` (varsayılan false, yalnızca editor/PIE'de true) ile kapıla. Paketlenmiş Shipping build'de `~` tuşu ve `MMOGiveItem 1 1` ile test edip kapalı olduğunu doğrula. RPC'lere `_Validate` ekleyerek bariz geçersiz parametrede istemciyi düşürmeyi düşün (şu an hiçbir RPC'de `WithValidation` yok).

---

### Y-7. Sunucu DoS yüzeyleri

1. `MMOPlayerController.cpp:128-360` `ServerSendChat`: Mesaj uzunluğu yalnızca normal sohbet/fısıltı/klan dalında `Left(120)` ile kırpılıyor; komut dalları (`/klan`, `/w` hedef adı, `/arkadas`) ham ve sınırsız dizgeyi işliyor, hata iletilerinde yankılıyor (yalnızca gönderene). Kullanım hatası dönüşleri (`/w` argümansız, hedef bulunamadı) `LastChatServerTime`'ı güncellemediği için 0.7 sn throttle atlanıyor. Güvenilir RPC ile yüzlerce KB dizge gönderilebilir. Düzeltme: fonksiyonun başında `if (Message.Len() > 256) return;` ve throttle'ı fonksiyon girişinde güncelle.
2. `MMOClanComponent.cpp:46-62,250` `SavedClanNameInUse`: `/klan kur` çağrısında `Saved/SaveGames/MMOSave_*.sav` dosyalarının hepsi oyun thread'inde senkron `LoadGameFromSlot` ile okunuyor. Şu an 59 dosya var, sınırsız büyür (Y-5). Bir oyuncu geçersiz/kullanımdaki adlarla sürekli tetikleyerek oyun thread'ini bloke edebilir (0.7 sn throttle ile bile). Düzeltme: klan adlarını bellekte/ayrı küçük bir indeks dosyasında tut.
3. `MMOCharacter.cpp:665` `ServerSetName`: cooldown yok. Her başarılı çağrı `AnnouncePresence(false)`+`AnnouncePresence(true)` ile arkadaşlara bildirim yağdırır (iki adı döngüyle değiştirerek sohbet spamı). Düzeltme: 30 sn cooldown.
4. Hiçbir RPC için genel hız sınırlaması yok (yalnızca bazılarında özel throttle var); güvenilir RPC kuyruğu taşırılarak bağlantı düşürme/yük oluşturma denenebilir.

---

### Y-8. Replikasyon bilgi ifşası

`MMOInventoryComponent.cpp:31-34`: `Gold`, `Items`, `EquipmentBag`, `EquippedItems` koşulsuz (`DOREPLIFETIME`) replike edilir; yalnızca `Warehouse` `COND_OwnerOnly`. `MMOStatsComponent.cpp:81-82`: `UnspentStatPoints/UnspentSkillPoints` de herkese. Değiştirilmiş bir istemci, ilgililik alanındaki başka oyuncuların altınını ve çanta içeriğini okuyabilir (hedefli hırsızlık/dolandırıcılık ve ticaret sahtekarlığı için istihbarat). Aşağıdakiler doğrulanmalı: ticaret/inceleme arayüzü bu alanları başkalarının verisi için kullanıyor mu (kullanıyorsa ayrı, sunucu kontrollü bir kanal gerekir).

Düzeltme: `Gold`, `Items`, `EquipmentBag` için `COND_OwnerOnly`; `EquippedItems` görsel için gerekliyse yalnızca görsel için gereken alanları (ItemId) herkese, `Plus/Rarity` sahibe.

---

### D-9. Kullanıcı dizgelerinde karakter denetimi

- Sohbet: yalnızca `\r`/`\n` temizleniyor (`MMOPlayerController.cpp:141`). Sekme, sıfır genişlikli, çift yönlü geçersiz kılma (U+202E) karakterleri geçer; satırın görünümü taklit/yanıltma için kullanılabilir.
- `?Name=` yolundan gelen ad (K-1) `ServerSetName` süzgecinden geçmez (yalnızca `Left(20)`); `/who`, sohbet göndereni, klan listesi, fısıltı hedef eşleşmesinde kullanılır. Log enjeksiyonu: kullanıcı verisi `UE_LOG` biçim dizgesine girmiyor (yalnızca sanitize edilmiş slot adı loglanıyor, `MMOCharacter.cpp` ~537; bu yüzden log enjeksiyonu riski düşük).
- Klan adı `FChar::IsAlnum` ile doğrulanıyor: Unicode harfler geçer, Kiril/Latin homoglif ile var olan klanı taklit etmek mümkün.
- Biçim dizgisi enjeksiyonu: tüm `FString::Printf` çağrıları `TEXT("...")` sabit biçim kullanıyor; kullanıcı verisi yalnızca `%s` argümanı. SORUN YOK.
- Yol enjeksiyonu: `GetSaveSlotName` yalnızca `[A-Za-z0-9_-]`+boşluk(`_`) bırakıyor, 32 karakter; `MMOSave_` öneki Windows ayrılmış aygıt adlarını (CON, NUL...) engelliyor. SORUN YOK.

Düzeltme: ortak `SanitizeDisplayText()` (kontrol karakterleri + bidi biçim karakterleri sil, uzunluk sınırı) ve `AMMOGameMode::ChangeName` override'ında uygula.

---

### D-10. HTTP hata gövdesinin loglanması

`MMODialogueSubsystem.cpp:170`: `UE_LOG(... "HTTP %d: %s", Code, *RespBody)`. Bazı sağlayıcılar hata iletisinde anahtarın maskeli bir parçasını yankılar; ayrıca motorun `LogHttp` kategorisi VeryVerbose'ta istek başlıklarını (Authorization) yazabilir (doğrulanmalı). `Saved/Logs` yerelde kalıyor (ignore'da), fakat çökme raporları/oyuncu log paylaşımı ile dışarı çıkabilir. Düzeltme: gövdeyi 200 karakterle kırp, `Bearer`/`nvapi-`/`sk-` kalıplarını maskele; `LogHttp`'i VeryVerbose'a çıkarma.

Not: `Saved/Logs/*.log` ve `Saved/Config` içinde anahtar kalıbı bulunmadı.

---

### D-11. `Config/DefaultEngine.ini:89` AndroidFileServer SecurityToken

`[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` bölümünde `SecurityToken=323F...` (32 hex) commit'li ve `bAllowNetworkConnection=True`. Bu, editörün Android cihaza dosya göndermesi için kullanılan geliştirme token'ıdır (`bIncludeInShipping=False`, `bAllowExternalStartInShipping=False`, `ConnectionType=USBOnly` ile etkisi sınırlı). Düşük risk; depo herkese açık olursa token herkesçe bilinir. Android dağıtımı yapılmıyorsa bölümü kaldır veya `bAllowNetworkConnection=False` yap; token'ı yeniden üret.

---

### D-12. `.gitignore` boşlukları

Olumlu: `Saved/`, `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Build/`, `Art/`, `*.sln`, `UpgradeLog*.htm` ignore'da; git geçmişinde hiç `.sav`, log, `.env`, `Saved/` dosyası yok (doğrulandı).

Eksikler:
- `.idea/` (JetBrains/Rider; `workspace.xml` yerel yollar ve ayarlar içerir) ignore'da değil, şu an `??` (izlenmeyen); bir `git add -A` ile yanlışlıkla commit edilebilir.
- `*.sav`, `*.log`, `.env*`, `*.pem`, `*.key` için emniyet kuralı yok.
- `.claude/settings.local.json` ignore'u yalnızca bu makinenin kullanıcı-global `~/.config/git/ignore` dosyasında; başka bir klonda/CI'da korumasız (içinde makineye özgü yollar ve geniş izinler var; sır bulunmadı).
- `.agents/`, `.codex/`, `AGENTS.md` izlenmiyor; commit edilecekse içeriğinde sır olmadığı doğrulandı (grep temiz).

Düzeltme: `.gitignore`'a `.idea/`, `*.sav`, `*.log`, `.env*`, `*.pem`, `*.key`, `.claude/settings.local.json` ekle.

---

### B-13. Ağ ve yapılandırma bilgileri

- `Scripts/NetSmokeTest.ps1`: `?listen` ile 7777 portunda tüm arayüzlerde dinleme; parola/`PreLogin` denetimi yok. İnternete açık bir host için K-1 ile birleşince risk büyür.
- `Source/*.Target.cs`: yalnızca `Game` ve `Editor` hedefi var; dedicated `Server` hedefi yok. Tüm otorite ve kayıt host makinesinde; host güvenilir kabul ediliyor (listen-server modelinin doğası).
- `Source/MyProject/Public/Core/MMOGameMode.h:40`: `bAllowPvP = true` ("Test için varsayılan açık"); yayın öncesi `false` olmalı. PvP kuralları (parti/klan/hub) merkezi girişte uygulanıyor (iyi).
- Bağımlılıklar (`MyProject.Build.cs`): `Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule, NavigationSystem, NetCore, Slate, SlateCore, HTTP, Json, JsonUtilities`; üçüncü taraf kütüphane yok. `JsonUtilities`'in kullanımı doğrulanmalı (kullanılmıyorsa kaldırarak yüzey küçültülebilir). `PythonScriptPlugin` ve `ModelingToolsEditorMode` yalnızca `Editor` hedefinde. HTTP trafiği `https://` ile gidiyor (TLS sertifika doğrulaması motor varsayılanı; `Config` içinde devre dışı bırakan ayar yok).

---

## 3. Sır Taraması Sonuçları

- Repo (Source, Config, Scripts, docs, .claude/agents|skills, CLAUDE.md, AGENTS.md, .agents, .codex, .idea): `nvapi-`, `sk-ant-`, `sk-...`, `AKIA...`, `ghp_...`, özel anahtar blokları, `Bearer <değer>`, `Authorization: <değer>` kalıpları bulunmadı.
- Git geçmişi (`git log --all -p -G...`, 236 commit, `Content/` hariç patch taraması + `Content` ikili dosyaları için grep): gerçek sır yok. Geçmişte `.env`, `settings.local.json`, `.sav`, `Saved/` dosyası eklenmemiş.
- `.claude/settings.local.json` (192 satır): API anahtarı/parola/token yok; fakat Y-3'teki geniş izinler var.
- `Scripts/*` (Python/PowerShell): ağ çağrısı yok, sır yok; `Remove-Item` yalnızca kendi log dosyasına.
- `docs/PLAYTEST_CHECKLIST.md:2788`: "anahtar/parola/token benzeri bir şey yok" notu doğrulandı.
- Tek bulgu: `Config/DefaultEngine.ini:89` AndroidFileServer geliştirme token'ı (D-11, ilk 4 karakter `323F`).
- LLM anahtarının yolu: ortam değişkeni -> sunucu süreci -> HTTPS başlığı. İstemciye replike edilmiyor, loga değeri yazılmıyor, koda gömülü değil (kodun kendi yorumu da aynısını belirtiyor).

---

## 4. Güvenli Bulunan Alanlar (inceleme notu)

- Sunucu tarafı mesafe/yakınlık denetimleri: satıcı, depocu, görevli, şifacı, NPC diyalog RPC'leri.
- Tamsayı taşması korumaları: `AddGold` (int64 + clamp), `AddItem`, `BuyPotion/BuyConsumable` (1-999 sıkıştırma), ticarette altın toplam denetimi.
- Ticaret (`MMOTradeComponent`): teklif revizyonu, altın üst sınırı, yeni `InstanceId` atama.
- Parti atma (`ServerKick`): lider ve üyelik denetimi sunucuda.
- Saldırı/yetenek: sunucu otoriter cooldown, menzil, PvP/parti/klan/hub kuralları (`CanHarmPlayer`).
- Sınıf değişimi: enum doğrulaması + cooldown + savaş denetimi.
- Bozuk kayıt: mevcut dosya ezilmiyor (`bInitialLoadComplete`), konum doğrulanıyor.

---

## 5. Önerilen Düzeltme Sırası

1. K-1 (kalıcı kimlik, `PreLogin` ile tekrar eden slot reddi, `?Name=` yok sayma, `ServerSetName` slotu değiştirmesin, `.bak`)
2. Y-3 (anahtarları kalıcı ortamdan kaldır, bypassPermissions/PowerShell iznini daralt)
3. Y-2 (merkezi `SanitizeSaveData` + HMAC)
4. Y-6 (debug komutlarını Shipping'den çıkar; paketlenmiş build'de konsolu doğrula)
5. Y-4 (LLM genel bütçe + `MaxTokens` + çıktı süzgeci)
6. Y-5, Y-7, Y-8, D-9 (ad/PII, DoS, replikasyon koşulları, metin süzgeci)
7. D-10, D-11, D-12, B-13 (log maskeleme, token, gitignore, PvP varsayılanı)

## 6. Doğrulanmalı Listesi

- `?Name=` ile başkasının kaydının yüklenmesi (uçtan uca oyun testi; motor kaynağı ve proje kodu bunu destekliyor).
- Paketlenmiş Shipping build'de konsolun gerçekten kapalı olması.
- `LogHttp` ayrıntı seviyesinde Authorization başlığının loglanıp loglanmadığı ve NVIDIA/Anthropic hata gövdelerinin anahtar yankılayıp yankılamadığı.
- Ticaret/inceleme arayüzünün replike `Gold/Items/EquipmentBag` alanlarına bağımlılığı (Y-8 düzeltmesinden önce).
- Ajan oturumlarında ağ çıkışının kısıtlı olup olmadığı (Y-3).
- `JsonUtilities` modülünün kullanılıp kullanılmadığı.
- Kayıt dosyası düzenlemesinin fiilen kabul edildiği (Y-2 sömürüsü kod okumasından çıkarıldı; düzenlenmiş .sav ile test edilmedi).

---

## 7. Uygulama durumu (2026-10-04 güncellemesi)

PR [ozancidik/mmogame#1](https://github.com/ozancidik/mmogame/pull/1) üzerindeki değişiklikler **derlenmedi** (bulutta derleyici yok); yerelde `RunAllChecks.ps1` gerekir.

| No | Durum | Ne yapıldı / ne kaldı |
|----|-------|------------------------|
| K-1 | **Kısmen** | Çevrimiçi bir oyuncunun adıyla bağlanma reddedilir (`AMMOGameMode::PreLogin`); ad değişince başka kayıt yuvasının üzerine yazma engellenir (`ActiveSaveSlot`); isteğe bağlı sunucu parolası `-MMOPassword=`. **Açık:** çevrimdışı birinin adını `?Name=` ile kapıp kaydını yüklemek, `MMOLoad` ile sahiplenme. Kalıcı çözüm hesap sistemi (`ACCOUNT_BACKEND_COMPARISON.md`). |
| Y-2 | **Büyük kısmı** | Ana stat toplam tavanı, altın/yığın/nadirlik tavanı, yanlış slot, yinelenen `InstanceId`, görev sayacı ve yinelenen kimlikler. **Açık:** klan alanı (sunucu kaydı gerekir), görev ön koşul zinciri tutarlılığı, dosya imzası/HMAC. |
| Y-3 | Açık | Yerelde kullanıcı kararı (anahtar rotasyonu, ajan izinleri). |
| Y-4 | **Büyük kısmı** | LLM çıktısı 240 karakter + kontrol karakteri temizliği; sunucu geneli hız sınırı (dakikada 30, eşzamanlı 4; sınır aşılınca NPC yedek replik söyler). **Açık:** `MaxTokens` (düşünme tokenları aynı tavana sayıldığı için ölçümsüz düşürülmedi), günlük bütçe tavanı. |
| Y-5 | **Kod kapandı** | `<ad>-<hex>` varsayılan adı `Gezgin######` olur (`AGameModeBase::ChangeName`, derlenmedi; imza UE 5.7'de doğrulanmalı). Eski `MMOSave_<KULLANICI>-<hex>.sav` dosyaları yetim kalır. |
| Y-6 | **Kod kapandı** | Yedi debug Exec komutu ve üç Server RPC gövdesi `UE_BUILD_SHIPPING`'de erken döner. Shipping paketinde konsolun kapalı olduğu doğrulanmadı. |
| Y-7 | **Kapandı** | `ServerSetName` uzak istemciler için 30 sn soğuma; sohbet girişi ve `/klan kur` önbelleği önceki turda. |
| Y-8 | Önceki turda kapandı | Envanter/görev/beceri/stat özel alanları yalnız sahibe. |
| D-9 | **Kısmen** | Sohbette kontrol ve bidi karakterleri temizlenir. **Açık:** klan adı homoglif denetimi. |
| D-10 | Kapandı | HTTP hata gövdesi 200 karakterle loglanır. |
| D-11 | Açık | `DefaultEngine.ini` AndroidFileServer token'ı (`Config/` bulutta düzenlenmez). |
| D-12 | Önceki turda kapandı | `.gitignore` (`.idea/`, `*.sav`, `.env`). |

## 8. Ek not (2026-10-05, QA denetimi): komut satiri parolasi

- `-MMOPassword=` sunucu komut satirinda gorunur (islem listesi, kisayol/betik gecmisi) ve istemci `?Password=` degeri motorun `LogNet` "Login request: ..." satirina duser (sunucu log dosyasi). Kod DEGISTIRILMEDI.
- Yerel/ozel host icin kabul edilebilir; kamuya acik sunucuda parolayi ortam degiskeninden okuyup `Login request` logunu kapat ya da maskele (LogNet verbosity), log dosyalarini paylasma.
- Bu parola kimlik dogrulamanin yerini tutmaz (K-1 notu gecerli); kalici cozum hesap sistemi.

## 9. Uygulama notu (2026-10-05): kod gerektiren dusuk riskli maddeler

### 9.1 Debug/hile Exec komutlari (Y-6) - son liste

`MMODebugCommandsEnabled()` (`MMOCharacter.cpp`) Shipping'de `false` doner; asagidaki komut/RPC govdeleri erken cikar:

| Komut / RPC | Dosya | Shipping'de |
|---|---|---|
| `MMODamage`, `MMOHeal`, `MMOGainXP`, `MMOUseMana` | `MMOCharacter` | etkisiz |
| `MMOGiveItem` + `ServerGiveItem` | `MMOCharacter` | etkisiz |
| `MMOBuff` + `ServerApplyBuff` | `MMOCharacter` | etkisiz |
| `MMOSpawnMob` + `ServerSpawnMob` | `MMOCharacter` | etkisiz |
| `MMOClass` (konsol kisayolu; HUD sinif paneli `RequestSetupForClass` ile calismaya devam eder) | `MMOCharacter` | etkisiz (bu turda eklendi) |
| `MMONpcTalk` (ucretli LLM test istegi) | `MMOCharacter` | etkisiz (bu turda eklendi) |
| `MMOProbeSpawn`, `MMOProbeSurface` | `MMOGameMode` | `#if !UE_BUILD_SHIPPING` ile bos |

Oyun komutu sayilip ACIK birakilanlar: `MMOSave`, `MMOLoad`, `MMOSetName`, `MMOTravel` (host), `MMOUse`, `MMOAttack`, `MMOStat`, parti (`MMOInvite/PartyAccept/PartyLeave/PartyKick`), ticaret (`MMOTrade*`), `Say`; `MMOPvP` yalniz host (`ServerSetPvP` `IsLocalController` kapisi) oldugu icin acik (PvP'yi acmanin tek yolu). Not: UHT `UFUNCTION`'i `#if` icinde kabul etmedigi icin fonksiyonlar derlenir, govdeler Shipping'de bostur. Paketlenmis Shipping'de `~` konsolunun kapali oldugu hala dogrulanmadi.

### 9.2 Altin tavani (Y-2 tutarsizligi)

Tek sabit: `UMMOInventoryComponent::MaxGold = 999999999`. `AddGold` (int64 toplam + clamp `[0, MaxGold]`), ticaret tasma reddi (`MMOTradeComponent`) ve `ApplySaveData` AYNI sabiti kullanir (eskiden calisma zamani INT32_MAX, kayit 999999999: 1 milyar ustu yuklemede sessizce kirpiliyordu). Kayit formati (`MMOSaveGame.h`) degismedi.

Tasma kurali (mevcut davranis korundu, sessizlik giderildi): loot/gorev/satis gibi tek tarafli kazanc tavani asarsa fazlasi KIRPILIR (kaybolur) ve sahibe "Altin siniri (999999999): fazlasi kaybedildi" ipucu gider; ticaret tavani asacaksa takas ONCE reddedilir ("altin siniri asilir"), iki taraf da altinini korur. Negatif/harcama yolu ve int64 koruma bozulmadi.
