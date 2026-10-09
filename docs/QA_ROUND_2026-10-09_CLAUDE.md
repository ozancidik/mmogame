# QA turu 2026-10-09: Claude oturumunun commit'leri (bağımsız denetim)

Denetçi: qa-kontrolcu. **Salt okunur**: derleme, editör, oyun, smoke çalıştırılmadı; kaynak/Config/Content/Scripts değiştirilmedi;
commit/push yok. Codex'in commit edilmemiş `MMOPlayerController.cpp` ve `Scripts/TestPartyConsent.ps1` dosyalarına dokunulmadı
(StaticLint çalışma ağacını okurken onları da taradı, yalnız okuma). PIE'de oynanmadı: "doğrulanmalı" işaretli her şey doğrulanmamıştır.

Denetlenen: `e5ab5d6`, `06d9394`, `ebaf532`, `d09772d`, `eee54e4`, `5209a63`, `dcea234`, `97a5e95`, `cdb95fa`, `54de52b`,
`9fdd2ea`, `9424e87`, `c45953b`, `6f25aed` (HEAD = `699bd3f`).

## 0. Bu turda gerçekten çalıştırılan (kanıt)

- `Scripts/ValidateData.py` (UE'nin gömülü Python'u, `-I`): **"Veri denetimi temiz"**; 59 mob / 57 eşya / 90 görev / 93 beceri / 35 spawner.
  32 beceri boşluğu UYARISI (5-7 seviye), commit mesajıyla tutarlı.
- `Scripts/StaticLint.py`: **temiz** (84 dosya; derleme değildir).
- `Scripts/ContentMap.py` (yazmadan, stdout): `docs/CONTENT_MAP.md` ile **birebir aynı** (yalnız sonda bir boş satır farkı).
- `Scripts/GenQuestDialogue.py`, çıktısı scratchpad'e yönlendirilmiş bir kopya ile: **commit'teki `MMOQuestDialogue.cpp` ile FARKLI** (ORTA-1).
- Mojibake taraması (`Source/ Scripts/ docs/ CLAUDE.md .github/`, hem literal `â€ Ã¼ Ä± ÅŸ Ã§ Ã¶ ÄŸ Ä°` hem bayt deseni): **0 eşleşme**.
  `git diff 06d9394^ HEAD -- MMOStatsComponent.cpp` yalnız amaçlanan kod/yorum değişikliklerini gösteriyor: 9 bozuk yorum satırı
  `ebaf532`'de **orijinaliyle bayt bayt aynı** geri getirilmiş. Bugün dokunulan 44 dosyada BOM yok, satır sonları tutarlı (tamamı CRLF).
- Sır taraması (bugünkü tüm diff'ler; `nvapi-`, `sk-ant-`, `AKIA`, sabit `api_key=`): **0 eşleşme**.
- Bugün eklenen kaynak satırlarında ASCII dışı karakter: **yok** (`TEXT("...")` içindeki mevcut 5 ASCII dışı satır eski, oyuncuya gösterilmiyor).

## 1. Özet

**KRİTİK bulgu yok.** Sunucu otoritesi korunmuş: bağış/satıcı/yükseltme/ışınlanma/DoT kararlarının hepsi sunucuda, istemcinin
otorite gibi davrandığı yol bulunmadı. `MMOTeleport` yönetici + Shipping-dışı kapılı. Spawner döngüsü sınırlı.

| # | Önem | Başlık |
|---|---|---|
| ORTA-1 | ORTA | Bağış görevleri: NPC repliği hâlâ "biriktir" diyor (`MMOQuestDialogue.cpp` yeniden üretilmemiş) ve teslim mesajı altın düşüşünü söylemiyor |
| ORTA-2 | ORTA | Taht seti "yalnız Naib düşürür" iddiası yanlış: satıcı tüm eşya DB'sini satıyor, Lv75+ normal moblar da düşürüyor; `Q_AbyssRegent` ödülü önceden satın alınabilir |
| ORTA-3 | ORTA | DoT yenileme büyüklüğü son uygulayanın saldırısıyla EZİLİYOR: zayıf oyuncu güçlü oyuncunun zehrini küçültür ve öldürme kredisini alır |
| DÜŞÜK-1 | DÜŞÜK | `TestDotSourceLifecycle`'ın "oyuncu→oyuncu" kontrolü aslında kendi kendine zehir; gerçek PvP DoT yolu test edilmiyor (sahte-geçme riski) |
| DÜŞÜK-2 | DÜŞÜK | `SmokeTest.ps1` cep uyarısı engebeli (zemin-probu başarısız) cepleri göremez: başarısız denemeler sayılmıyor |
| DÜŞÜK-3 | DÜŞÜK | Beceri tablosu: commit "rütbe korundu" diyor ama Okçu/Büyücü Special0 6→7, Rahip Special1 4→5 yeni rütbe istiyor; `Mag_GlacialSpike` adı `Mag_IceLance` ile aynı ("Buz Mizragi") |
| DÜŞÜK-4 | DÜŞÜK | Spawner: kalıcı kötü cepte her 4 sn'de 4 kat deneme + `Log` seviyesinde satır başına log |
| DÜŞÜK-5 | DÜŞÜK | Belge tutarsızlıkları: `DECISIONS_NEEDED.md:172` ve `CLAUDE.md` DoT notu bayat; günlük özet "12 yan görev" (gerçek 11) |
| DÜŞÜK-6 | DÜŞÜK | Aktif `Q_Gold5000+` taşıyan eski kayıtlar: kurallar oyuncunun haberi olmadan "biriktir"den "bağışla"ya döndü |

## 2. Bulgular

### ORTA-1: Bağış görevinin NPC repliği ve teslim mesajı oyuncuyu yanıltıyor
- **Dosya:** `Source/MyProject/Private/Quests/MMOQuestDialogue.cpp:59-63` (üretilmiş dosya), kaynak `docs/Lore.md:979-994`;
  `Source/MyProject/Private/Components/MMOQuestComponent.cpp:637-640` (düşüş), `:678` (bildirim).
- **Gerekçe:** `ebaf532` Lore.md'deki 4 altın görevinin repliğini "bağışla" olarak güncelledi ama `GenQuestDialogue.py` yeniden
  çalıştırılmadı. Scratch'e yeniden üretim ile karşılaştırma: tam olarak bu 4 satır farklı; oyundaki alış repliği "Bes bin altin
  **biriktir**, evlat", teslim repliği "Bes bin altin **tasiyorsun** artik". Görev açıklaması (`MMOQuestTypes.cpp`) doğru ("bagisla
  (teslimde altin dusulur)"), yani aynı pencerede iki çelişkili metin var. Teslim bildirimi `"Gorev tamam: %s (+%d XP, +%d altin)"`
  bağışta "+4000 XP, +0 altin" yazar, **-5000 altını hiç söylemez**.
- **Senaryo:** Lv28 oyuncu `Q_Gold50000`'i alır, NPC "Elli bin altin biriktir" der; 50.000 altınla teslim eder, bakiye 0'a iner,
  ekranda yalnız "+45000 XP, +0 altin" görünür. Bunu hata sanar.
- **Öneri:** `GenQuestDialogue.py`'ı çalıştırıp `MMOQuestDialogue.cpp`'yi yeniden üret; bağış görevinde bildirime "-N altin bagislandi"
  ekle. CI'ya "üretilmiş diyalog Lore.md ile eşleşiyor mu" denetimi (ContentMap'teki gibi stdout karşılaştırması) eklenebilir;
  `RunAllChecks` bu sapmayı yakalamadı.

### ORTA-2: Taht seti münhasır değil; satıcı kapısı yalnız "3 seviye önce" kuralı
- **Dosya:** `MMOItemDatabase.cpp:58-60` (yorum: "Naib ... artik yalniz bu seti dusurur"), `docs/DECISIONS_NEEDED.md:64`,
  `MMOInventoryComponent.cpp:875-931` (`BuyEquipment` DB'deki **her** `ItemId`'yi kabul eder), `MMOHUD.cpp:1551`
  (satıcı listesi = `FMMOItemDatabase::GetAll()`), `MMOMob.cpp:1165-1175` (normal mob havuzu `RequiredLevel <= MobLevel + 3`).
- **Gerekçe:** (a) Satıcı Taht setini de satıyor; kapı yalnız `Level + 3 >= 78`, yani **Lv75'te altınla alınır**. (b) Lv75+ normal
  moblar (ör. Koruyucu 76, Naibin Elçisi 78) Taht setini normal düşüş havuzunda taşır. (c) `Q_AbyssRegent` (kapı 79) ödülü
  `ThroneBlade`; oyuncu bunu Lv75'te satıcıdan zaten almış olabilir, ödül değersizleşir.
- **Kapı atlama yolları (bilerek açık, ama belgede söylenmemiş):** takas ve depo seviye denetlemez; yüksek seviyeli bir arkadaş
  satın alıp takasla verebilir. Kuşanma `RequiredLevel`'i tam denetlediği için **güç istismarı yok**; yalnız "altınla kademe
  atlama" sink'i kısmen delinir. Bu bir hata değil, iddia ile kodun uyuşmaması.
- **Öneri:** Ya yorum/DECISIONS/Lore'u "satıcıda Lv75+ satılır, normal mobdan da düşer" diye düzelt, ya da satıcıya `bVendorSellable`
  benzeri bir liste ekleyip Taht setini dışarıda bırak (tasarım kararı; sahibine).

### ORTA-3: DoT yenilemesi büyüklüğü ve krediyi son uygulayana devrediyor
- **Dosya:** `MMOStatsComponent.cpp:780-809`.
- **Gerekçe:** Aynı efekt yenilenirken `E.Magnitude = EffectMagnitude` ve `E.Instigator = EffectInstigator` koşulsuz yazılır. Eskiden
  büyüklük sabitti (8), kimin yenilediği hasarı değiştirmezdi; artık saldırıyla ölçekli olduğu için **düşürülebilir**.
- **Senaryo:** AttackPower 1000 olan Okçu A mobu zehirler (tick 108). Partisiz Lv20 Okçu B (AP 80) aynı moba `Arc_PoisonArrow`
  atar: tick 16'ya düşer, sonraki tüm tick'ler B'nin `DamageContributions` hanesine yazılır (`MMOMob.cpp:314-316`). Partide de iki
  okçu birbirinin zehrini küçültür. Mob → oyuncu yenilemesi oyuncu zehrinin üzerine gelirse büyüklük 8'e iner (bu kısmen eski davranış).
- **Öneri:** Yenilemede `FMath::Max(E.Magnitude, EffectMagnitude)` ya da kaynak başına ayrı kayıt; kredi devri tasarım kararı.
  Test: iki farklı AP'li oyuncu kaynağıyla yenileme kontrolü.

### DÜŞÜK-1: "Oyuncu→oyuncu DoT" testi gerçek PvP yolunu sınamıyor
- **Dosya:** `MMOStatsComponent.cpp:1290-1291`.
- **Gerekçe:** `V->ApplyEffect(TEXT("Poison"), Victim)`; `V` zaten `Victim`'in statları, yani bu **kendi kendine zehir**
  (`:1262`'deki "live self poison" ile aynı durum; PvP kapalıyken de vurur). Başka bir oyuncu kaynağı yok (Pawns[1-3] önceden
  yok edildi). Bu kontrol `ApplyEffect`'teki `Cast<AMMOMob>(GetOwner())` koşulu silinse bile geçer mi: **geçer**, çünkü hedef
  oyuncu olduğu sürece yalnız `ProcessEffects`'teki tür seçimi önemli ve `TakeDamage` hedef mob değilse savunmayı zaten uygular.
  Mutasyon yalnız ilk yeni kontrolde denenmiş (commit mesajı).
- **Öneri:** Testi "self poison scaling/defense" diye yeniden adlandır ya da PvP açıkken yeni bir oyuncu kaynağıyla (Source != Victim)
  kontrol ekle. Diğer iki yeni kontrol (oyuncu→mob 82/18, mob→oyuncu 99/8) ilgili davranışı gerçekten yakalıyor.

### DÜŞÜK-2: SmokeTest cep uyarısı en kötü cepleri kaçırabilir
- **Dosya:** `Scripts/SmokeTest.ps1:31,37`; `MMOMobSpawner.cpp:163-172, 221`.
- **Gerekçe:** `Tries` yalnız `"SpawnOne: requested"` satırlarını sayar; bu satır zemin/nav noktası **bulunduktan sonra** yazılır.
  `FindSafeSpawnLocation` başarısız olan denemeler (engebeli yayla: 8 rastgele nokta tutmadı) hiç sayılmaz ve uyarı tek seferliktir
  (`bSpawnLocationWarningReported`). Tamamen engebeli bir cepte `Tries` 0-3 kalır, `Tries -ge 5` şartı yüzünden **UYARI basılmaz**.
  Uyarının amacı tam olarak bu cepleri yakalamaktı ("probe 64/64, gerçek 1/6").
- **Öneri:** Blok için `live < 4` tek başına (MaxMobs 6 olan normal spawner'larda) ya da "no nearby ground/nav" satırlarını da say.

### DÜŞÜK-3: Beceri tablosu küçük tutarsızlıklar
- **Dosya:** `MMOSkillDatabase.cpp:180, 242` (Okçu/Büyücü Special0 rütbe 7; önceki en yüksek 6, `:172`, `:234`), `:325` (Rahip
  Special1 rütbe 5; önceki en yüksek 4, `:314`), `:244` vs `:219` (iki beceri "Buz Mizragi").
- **Gerekçe:** Commit ve satır 116 yorumu "mevcut zincirin rütbesi (7) korunur" diyor; bu yalnız Savaşçı ve Rahip Special0 için doğru.
  Etki küçük: puan bütçesi `(Level-1) x SkillPointsPerLevel` (`MMOSkillComponent.cpp:999-1003`) bu seviyelerde ~100+, toplam rütbe
  ihtiyacından çok büyük; bu üç beceri **otomatik açılmaz**, 1 puan yatırım ister. `PLAYTEST_LV46_80.md:60` "otomatik açılıyor mu"
  sorusu bu üçü için "hayır, bir rütbe yatır" beklemeli. Kimlik çakışması yok (93 benzersiz `SkillId`, ValidateData temiz).
- **Güç eğrisi (bilgi):** Lv75 halkaları Lv70 capstone'ları güç/sn'de az geçiyor, Lv80'in altında: Savaşçı 1400/24 = 58,3 vs 1150/20 =
  57,5; Büyücü 1100/24 = 45,8 vs 900/20 = 45; Rahip `Pri_Sanctify` 1100 artık Rahibin en güçlü hasar becerisi (Lv80 capstone'u sifa).
  Yorumdaki "doğrusal ara değer" ile tutarlı; denge kararı değil.
- **Öneri:** Yorumu düzelt; `Mag_GlacialSpike` için farklı görünen ad.

### DÜŞÜK-4: Spawner yeniden denemesi: sonsuz döngü yok, ama maliyet/log
- **Dosya:** `MMOMobSpawner.cpp:61-74, 221`.
- **Denetlendi, sorun yok:** döngü `Needed x 4` ile sınırlı; `LiveMobs` yalnız başarıda eklenir, `MaxMobs` aşılamaz; `Needed` döngü
  başında bir kez hesaplanır, başarıda `break` var: elit/boss (MaxMobs 1) **çift doğamaz**. "Tekrar anlamsız" dalları `true` döner.
- **Risk:** Kalıcı kötü bir cepte (bilinen zayıf Abyss Strider cebi `(-5500,4500)`) her 4 sn'de mob başına 4x(8 trace + nav projeksiyonu);
  her çarpışma denemesi `Log` seviyesinde satır yazar. 35 spawner'da ölçülmedi (commit de "yük ölçümü açık" diyor). **Doğrulanmalı.**
- **Öneri:** Başarısız TopUp sayacı ile geri çekilme (backoff) veya `SpawnOne` logunu `Verbose`'a indirmek.

### DÜŞÜK-5: Belge tutarsızlıkları
- `docs/DECISIONS_NEEDED.md:172` hâlâ "`BuyEquipment` seviye şartını yalnız kuşanmada denetler ... Lv73-80'e yeni ekipman eklenmedi"
  diyor; `:63-64` ve kod bunun tersini söylüyor.
- `CLAUDE.md:27` `DOT_SCALING_DESIGN.md` için "decision pending" diyor; `06d9394` seçenek A'yı oyuncu→mob için uyguladı.
- `docs/GUNLUK_OZET_2026-10-09.md:17` "12 yan görev"; bugünkü commit'lerde 11 yeni yan görev var (2+3+2+1+2+1).
- `MMOItemDatabase.cpp:136-137` yorumu "Lv80 +9 = 32.500" diyor; `MaxPlus = 8`, +9 yok (testteki `GetUpgradeCost(8,80)` da oyunda
  ulaşılamaz bir değer). Kozmetik.
- **Kanıtsız kesin iddia taraması:** yeni belgelerin hepsi "PIE'de denenmedi / PLACEHOLDER" diye işaretli; "doğrulandı" kelimesi
  PIE davranışı için kullanılmamış. Günlük özetteki "Kanıt" sütunu headless testlere atıf yapıyor, doğru.

### DÜŞÜK-6: Aktif altın görevi taşıyan eski kayıtlarda kural değişimi
- **Dosya:** `MMOQuestComponent.cpp:637-640`.
- **Gerekçe:** `Q_Gold5000/15000/30000/50000`'i önceden kabul etmiş bir oyuncu, "biriktir" diye aldığı görevi teslim ettiğinde altını
  kaybeder. ORTA-1 ile birleşince sürpriz büyür. Tek oyunculu test aşamasında etkisi küçük.
- **Öneri:** Sürüm notu ya da bağış görevlerini yeni kimlikle yayınlama (tasarım).

## 3. Denetlenen ve sorun bulunmayan

- **Bağış (yarış/çift düşme/negatif):** `ClaimReward` tek oyun iş parçacığında; `IsQuestComplete` bakiye kontrolü ile düşüş aynı
  çağrıda, arada bekleme yok. `CompletedQuests` kontrolü ikinci teslimi engeller; RPC hız sınırı var. `AddGold` int64 toplayıp
  `[0, MaxGold]`'a sıkıştırır: **negatife inemez**. Açık takasta teklif edilen altın bağışla azalırsa `TradeSideStillValid`
  (`MMOTradeComponent.cpp:41`) takası iptal eder: **çift harcama yok**. Bağış görevlerinin eşya ödülü yok, kapasite reddi sırası sorun değil.
- **`GetUpgradeCost`:** `50ll * ... / 100ll` int64; `RequiredLevel` 0-100'e kırpılı; `Inst.Plus` çağrıdan önce `[0, MaxPlus)` ile
  denetleniyor (`MMOInventoryComponent.cpp:775`). En büyük değer ~45 bin: **taşma yok**. İki çağıran (sunucu + HUD) aynı parametreyi
  geçiyor; varsayılan parametreyle eski maliyeti kullanan unutulmuş çağıran yok.
- **Satıcı kapısı:** sunucuda, satıcıya yakınlık denetiminden sonra; reddedince altın/çanta değişmez. HUD sönük satırı yalnız görünüm,
  istemci tıklasa da sunucu reddeder. Test çifti (sınırın 1 altı reddedilir, sınırda alınır) gerçekten ayırt edici; mesafe reddi yüzünden
  sahte geçme olamaz çünkü ikinci kontrol satın almanın başarılı olmasını şart koşuyor.
- **`MMOTeleport`:** istemci→`ServerTeleport`; sunucuda tekrar `MMODebugCommandsEnabled()` (Shipping'de false) + `MMOAdmin::IsServerAdmin`
  (dedicated'da kimse, listen-server'da yalnız host) + ölü değil + NaN/sınır denetimi. Uzak istemci kendini ışınlayamaz.
- **DoT sunucu otoritesi ve kapılar:** savunma atlama yalnız `bPlayerSource && hedef AMMOMob` iken (`MMOStatsComponent.cpp:973`,
  `MMOCharacter.cpp:1705`). PvP/parti/klan/hub/spawn koruması oyuncu hedefli yolda değişmedi (`MMOCharacter.cpp:1670-1693`); mob hedefte
  hub kapısı (`MMOMob.cpp:301-305`) ve leash dokunulmazlığı geçerli. Yetim oyuncu DoT'u hâlâ atlanıyor (`:962-965`). Reentrancy: `Pending`
  kopyası + `bIsDead` kırılması korunmuş. `Magnitude` zaten `ActiveEffects` ile replike; HUD büyüklüğü göstermiyor, UI etkisi yok.
- **Mojibake onarımı:** tam ve temiz (bkz. bölüm 0).
- **Görev zinciri:** 90 benzersiz görev kimliği. Yeni 11 yan görevden hiçbiri bir ana zincir görevinin ön koşulu değil (yalnız
  `Q_FrostboundOath`←`Q_FrostboundSworn`, `Q_AbyssEnvoys`←`Q_AbyssEchoes` yan zincir). `Q_Gold*` zincirine başka görev bağlı değil.
  Ödül eşyası `ThroneBlade` (Lv78) kapı 79 ile uyumlu. Yeni görev hedeflerinin hepsi bir spawner bandında (ValidateData).
- **ContentMap** güncel; yeni görevlerin alış/teslim replikleri `MMOQuestDialogue.cpp`'de mevcut (yalnız 4 altın görevi bayat).
- **Spawn yerleşimleri** (`MMOGameMode.cpp`): bant değişiklikleri ve yeni cepler geçerli bantlar; `c45953b` geçici bant genişletmelerini
  geri aldı, çift tanım kalmadı.

## 4. Doğrulanamayan (PIE / ölçüm gerekli)

- Zehir tick'lerinin oyunda gerçekten `8 + AP x 0,10` görünmesi ve hasar sayısının (MulticastCombatText) bunu göstermesi.
- Yeni cepler gerçek oyunda dolu mu; spawner ek yükü (35 spawner, kötü cep) sunucu karesinde ölçülebilir mi.
- `Q_WinterReavers`/`Q_WinterWraiths` hedefleri artık yalnız hub'dan 5000+ birim uzaktaki P46/P48'de (`MMOGameMode.cpp`, eski Kış cebi
  42-44'e indi): Lv46-49 akışında yürüme süresi kabul edilebilir mi (pacing regresyonu olabilir).
- Satıcı penceresinde sönük satırların okunabilirliği (NullRHI HUD çizmez).

## 5. Oyun içi test adımları (PLAYTEST_CHECKLIST'e eklenmek üzere; bu tur dosyaya yazılmadı)

1. **Bağış repliği/mesajı (ORTA-1):** Play → `MMOGainXP` ile Lv16 → altın görevlisinden `Q_Gold5000`'i al → NPC repliğini oku
   (beklenen: "bagisla"; şu an "biriktir" çıkarsa ORTA-1 duruyor) → 5.700 altınla teslim et → bakiye 700 olmalı, ekranda altının
   düştüğü yazmalı.
2. **Satıcı kapısı:** Lv74'te satıcıyı aç → Taht satırları sönük, tıklayınca "Bu esya icin cok erken: Lv78 gerekir"; `MMOGainXP` ile
   Lv75 → Taht Kılıcı satın alınabiliyor (ORTA-2: bilinçli mi?) → kuşanmaya çalış: Lv78 şartıyla reddedilmeli.
3. **Yükseltme maliyeti:** Lv72 Uçurum Kılıcı +0 → envanterde "[+1: 2642a ...]" görünmeli (50 x 1 x (100+5184)/100) → altın 2641 iken
   yükseltme "altın yetersiz" vermeli.
4. **DoT yenileme (ORTA-3):** iki oyunculu PIE (listen + 1 istemci), biri `MMOGainXP` ile yüksek seviye Okçu, diğeri Lv5 Okçu → aynı moba
   önce güçlü, sonra zayıf `Zehirli Ok` → hasar sayıları küçülüyorsa ORTA-3 doğrulanmış olur.
5. **Işınlanma güvenliği:** iki oyunculu PIE → istemci penceresinde `MMOTeleport 0 0 300` → hiçbir şey olmamalı; host penceresinde
   çalışmalı.
6. **Lv52/64/75 becerileri:** Okçu Lv52, Special0 rütbe 6 iken `Arc_ShatterShot` otomatik açılmamalı, bir rütbe yatırınca açılmalı;
   Büyücüde iki "Buz Mizragi" satırı görünüyor mu not et.

## 6. Düzeltme durumu (Claude, aynı gün; `RunAllChecks -FullCompile` ile doğrulandı)

| Bulgu | Durum |
|---|---|
| ORTA-1 | **Düzeltildi:** `GenQuestDialogue.py` yeniden çalıştırıldı (4 satır); teslim bildirimi artık `-N altin bagislandi` yazıyor; `GenQuestDialogue.py --check` (üretilmiş diyalog Lore.md ile eşleşiyor mu) `RunAllChecks.ps1`'e adım olarak eklendi, bayat dosyada başarısız olduğu elle gösterildi. |
| ORTA-2 | **Sonradan kodla da çözüldü:** FMMOItemDef::bVendorSold eklendi, Taht seti satıcıdan çıkarıldı (sunucu reddi + HUD filtresi + ValidateData/EconomyModel; TestVendorEconomy 18). Önce: **Belgeler/yorum düzeltildi** (Taht seti münhasır değil; satıcı tüm tabloyu satıyor, Lv75+ normal mob düşürür, takas/depo denetlemez). **Tasarım sorusu açık:** satıcı stoğunu sınırlamak (`DECISIONS_NEEDED.md`). |
| ORTA-3 | **Düzeltildi:** daha zayıf oyuncu yenilemesi mobdaki daha güçlü oyuncu DoT'unun büyüklüğünü ve kredisini ezmez (yalnız süre tazelenir); `TestDotSourceLifecycle` 36 → **38** kontrol, korumayı kapatan mutant ilgili kontrolde başarısız oldu. |
| DÜŞÜK-1 | **Düzeltildi:** gerçek PvP DoT kontrolü eklendi (kaynak ≠ hedef, savunma 50 → tick 1 hasar, büyüklük 8); eski kontrol "self DoT" diye yeniden adlandırıldı. |
| DÜŞÜK-2 | **Düzeltildi:** `SmokeTest.ps1` `no nearby ground/nav location` satırlarını da uyarı olarak basar; ayrıca ilk dolumda 5+ denemede 4'ten az mob uyarısı. |
| DÜŞÜK-3 | **Düzeltildi:** yorum doğrulandı (hangi zincirler rütbe ister), `Mag_GlacialSpike` adı "Buz Sutunu"; `PLAYTEST_LV46_80.md` "otomatik açılmaz, 1 puan yatır" der. |
| DÜŞÜK-4 | **Yapılmadı:** başarısız TopUp için geri çekilme (backoff) ve `SpawnOne` logunu `Verbose`a indirme; yük ölçümü açık (smoke ve NetSmoke hatasız). `SmokeTest.ps1` log satırlarına dayandığı için log seviyesi bilerek değişmedi. |
| DÜŞÜK-5 | **Düzeltildi:** `DECISIONS_NEEDED.md` ve `CLAUDE.md` DoT/satıcı notları güncel, günlük özet "11 yan görev". |
| DÜŞÜK-6 | **Yapılmadı:** `Q_Gold5000+` aktif taşıyan eski kayıtlar için ayrı bir bilgilendirme yok; teslim bildirimi artık düşen altını gösterir. İlk oyuncu kaydı geldiğinde yeniden değerlendirilir. |
