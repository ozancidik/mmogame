# İçerik planı: Lv46–80 (Winter Lv41–57 + Lv66–80, Summer dış cepleri Lv58–65)

**Tarih:** 2026-10-09, `master` e812fa9 üzerinde. **Durum (plan yazıldığında):** yalnız plan. Biçim ve üslup `CONTENT_PLAN_LV11_45.md` ile aynı.

> **UYGULAMA DURUMU (2026-10-09, aynı gün):** W1 (Lv46–50), W2 (Lv51–57), R (Lv58–65), A (Lv66–70) paketlerinin mob/görev/Lore kısımları ve G11 beceri dolgusu (12 beceri: Lv52/64/75) kodda; ayrı Winter cepleri P46 (-5000,-3000), P48 (4500,-700; 2026-10-10: eski (5000,0) ağaçta takılma yaptı), P53 (6000,-4000), P69 (-4000,-6000) 289 noktalı runtime taramayla seçildi (hepsi ≥63/64). Lv58–65 Summer bandları genişledi (yeni koordinat yok). **Yapılmayanlar:** U (Lv71–80) paketi ve Lv73–80 ekipmanı, Burn efekti (DoT savunmaya karşı etkisiz: `SKILL_PLAN_LV31_45.md`), elit/boss fikirleri, S1–S4 sistemik işler, Lv62 görev boşluğu. Tüm sayılar `[PLACEHOLDER]`; PIE'de doğrulanmadı. Aşağıdaki bölümler plan metnidir ve uygulamayı birebir yansıtmayabilir (ör. Efekt Zehir yerine Slow).

**Yöntem (sayılar nereden):**
- Bugünkü durum sayıları, gerçek depoda **salt okunur** çalıştırılan `Scripts/ValidateData.py`, `ProgressionModel.py`, `BalanceReport.py`, `EconomyModel.py`, `ContentMap.py` çıktılarından ve kaynak okumadan gelir (2026-10-09).
- **Önerilen satırlar hiçbir kopyada çalıştırılmadı.** Bölüm 5'teki "beklenen çıktı" sütunu elle sayımdır (`[Hesap]`), `ValidateData.py` kodunu okuyarak yorumlanmıştır; uygulamadan önce gerçek çalıştırmayla doğrulanmalıdır.
- Komut kısaltması: `$PY` = `"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64/python.exe"`.

**Etiketler:** `[Kesin]` kodda/belgede okundu · `[Model]` betik çıktısı, oynanış değil · `[Hesap]` elle/küçük betikle hesaplandı · `[Tahmin]` varsayım, ölçülmedi · `[PLACEHOLDER]` ölçümle değişecek sayı.

> **Paralel çalışma:** Çalışma ağacında başkasının değişmemiş düzenlemeleri var (`MMOPlayerController.cpp`, `TestClanInviteAuthority.ps1`, bazı `docs/`). `MMOGameMode.cpp`, `MMOMobArchetype.cpp`, `MMOQuestTypes.cpp`, `MMOItemDatabase.cpp` eklerini **satır numarasıyla değil yorum çapasıyla** yerleştir (çapalar Bölüm 5'te). Bir ek, o dosyada başkasının bekleyen düzenlemesi varken başlatılmaz.

## 0. Özet

| Paket | Bant / harita | Ne | Tür | Boyut |
|---|---|---|---|---|
| W1 | Lv46–50, Winter | Paylaşılan cebi böl, 2 yeni mob (Lv47/49), 2 yan görev | veri satırı + 2 yeni cep | M |
| W2 | Lv51–57, Winter | 1 yeni mob (Lv53), 3 yan görev, yeni cep | veri satırı + 1 yeni cep | M |
| R | Lv58–65, Summer | 2 yeni mob (Lv60/64), 2 yan görev; **yeni koordinat yok** (mevcut 2 cebin bandı genişler) | veri satırı + küçük GameMode lambda değişikliği | S–M |
| A | Lv66–70, Winter | 1 yeni mob (Lv69), 1 yan görev, `Burn` efekti ile Lore uyumu | veri satırı + 1 yeni cep | M |
| U | Lv71–80, Winter | 2 yeni mob (Lv74/78), 2 yan görev, Lv78 ekipman seti | veri satırı + 2 yeni cep | M |
| S1–S7 | sistemik | elit ayarı, `Burn`, satıcı kapısı, ileri kamp, beceri boşlukları, fazlı boss, Winter rota betiği | karışık | S–L |

Toplam (hepsi uygulanırsa) `[Hesap]`: mob 51 → 59, görev 79 → 89, ekipman 53 → 57, spawner 29 → 35.

Kapsam dışı: Lv41–45 görev boşlukları (`CONTENT_PLAN_LV11_45.md` "sonraki tur" dedi), Lv80 sonrası endgame (Lore: "Uçurum bekliyor"), PvP, yeni görev türü, yeni harita.

---

## 1. Bugünkü Lv46–80 kapsamı

### 1.1 Bant bazında kapsam `[Kesin]`

Mob XP'leri tablo ham değeridir; Lv34+ mob ve görev çarpanı 1,00 (`MMOXPTuning.cpp` `GMobKnots`/`GQuestKnots` son knot `{34, 1.00f}`), yani ham = etkin.

| Bant (bölge, harita) | Normal mob (Lv, ham XP) | Elit (Lv, XP) | Spawner (cep) | Görev kapıları (ödül ekipmanı) | Ekipman seti |
|---|---|---|---|---|---|
| **46–50** Kış Diyarı, Winter | `WinterReaver` 46 (19.000), `WinterWraith` 48 (22.000); aynı cepte Lv42/44 de | `WinterSovereign` 50 (30.000) | `(-3000,-1500,42)` bant **42–48**, MaxMobs 6; elit `(-3000,0,50)` | 46 `Q_WinterReavers` (Helm), 48 `Q_WinterWraiths` (Plate), 50 `Q_WinterSovereign` (Blade), 50 `Q_Level65` | Winter Lv42 |
| **51–57** Buzbağlı Sınır, Winter | `FrostboundScout` 51 (30.000), `FrostboundSentinel` 55 (36.000) | `FrostboundWarden` 57 (52.000) | Gözcü `(3000,0,210)` Lv51; Nöbetçi `(-3500,3000,-120)` Lv55; elit `(2000,2800,236)` | 51 (Boots), 53 (Helm), 54 (Plate), 57 (Blade) | Frostbound Lv51 |
| **58–65** Yarık, **Summer** dış yamaçlar | `RiftSeeker` 58 (45.000), `RiftKeeper` 62 (52.000) | `RiftHerald` 65 (75.000) | `(800,-4800,50)` Lv58; `(-4800,4800,-120)` Lv62; elit `(4800,4000,550)` | 58, 60, 61, 61 (`Q_RiftPurge`), 63 (`Q_RiftDepths`), 65 (+ `Q_Level80` kapısı 65) | Rift Lv58 |
| **66–70** Kül Hattı, Winter güney | `AshVanguard` 66 (60.000), `AshBinder` 68 (66.000) | `AshTyrant` 70 (100.000) | `(1200,-5200,50)` Lv66; `(-2000,-6800,49)` Lv68; elit `(4400,-5200,340)` | 66, 67, 68, 70 | Ashen Lv66 |
| **71–80** Uçurum Yaylası, Winter kuzey/doğu | `AbyssStrider` 72 (82.000), `AbyssGuardian` 76 (95.000) | `AbyssRegent` 80 (140.000) | `(-5500,4500,-92)` Lv72; `(4400,3600,550)` Lv76; elit `(6800,2000,629)` | 71 (×2), 73, 74, 75 (×2), 77, 78, 79 | Abyss Lv72 |

Kaynaklar: `MMOMobArchetype.cpp` (Lv46+ satırları), `MMOGameMode.cpp:936-1034` (Winter) ve `:1293-1326` (Rift), `MMOQuestTypes.cpp:184-313`, `MMOItemDatabase.cpp:50-102`. Lv46+ toplam: 10 normal + 5 elit mob; kapısı ≥46 olan 28 görev (26 öldürme/yan + `Q_Level65`, `Q_Level80`); 16 parça yeni ekipman (Frostbound/Rift/Ashen/Abyss; Winter seti Lv42, parçaları Lv44–50 görevlerinden).

Cep başına tek mob türü olan cepler (Lv51+): her biri 6 eşzamanlı mob; yenilenme `TopUp` ile her **4 sn**de eksiği tamamlar (normal), elitte 60 sn `[Kesin: MMOMobSpawner.cpp:48-66, GameMode]`. Mob `LeashRange` 1800, `WanderRadius` 450 `[Kesin: MMOMob.h:62,66]`.

### 1.2 Cepler ve ölçüm durumu

Hub = Winter `PlayerStart` `(0,-1500,150)`. Hub mesafeleri ve en yakın komşu mesafeleri `[Hesap]` (koordinatlar koddan):

| Cep | Hub'dan | En yakın cep | Ölçüm kaydı `[Kesin: PROGRESSION_PLAN / GameMode yorumları]` |
|---|---:|---|---|
| W1 `(-3000,-1500)` Lv42–48 | 3.000 | Kış Hükümdarı 1.500 | halka 32/32 (editör), oyun içi ayrı kayıt yok |
| Kış Hükümdarı `(-3000,0)` | 3.354 | W1 1.500 | aynı |
| Gözcü `(3000,0)` | 3.354 | Muhafız 2.973 | editör probu 48/48; oyun içi kayıt yok |
| Nöbetçi `(-3500,3000)` | 5.701 | Gezgin 2.500 | oyun içi 64/64 |
| Muhafız `(2000,2800)` | 4.742 | Koruyucu 2.530 | editör 16/16 |
| Kül Öncüsü `(1200,-5200)` | 3.890 | Kül Tiranı 3.200 | editör, 11 cep %100 |
| Kül Bağlayıcı `(-2000,-6800)` | 5.665 | Öncü 3.578 | aynı |
| Kül Tiranı `(4400,-5200)` | 5.749 | Öncü 3.200 | aynı |
| Uçurum Gezgini `(-5500,4500)` | 8.139 | Nöbetçi 2.500 | oyun içi 59/64 |
| Uçurum Koruyucusu `(4400,3600)` | 6.736 | Muhafız 2.530 | editör |
| Uçurum Naibi `(6800,2000)` | 7.648 | Koruyucu 2.884 | editör (QA ile taşındı) |
| Summer Arayıcı/Bekçi/Haberci | – | eski cebe ≥~3.400 | editör 48/48, 48/48, 16/16 |

Not `[Hesap]`: hub → Gezgin düz hattı Nöbetçi cebine 461 u, Kış Hükümdarı cebine 1.198 u yakın geçer (aggro zarfı ~1.275 u). Lv72+ oyuncu için sorun değil; ters yönde (Lv42–50 oyuncusu Lv72 cebine gitmez) risk yok.

### 1.3 Seviye seviye boşluklar `[Kesin: ContentMap.py + ValidateData]`

- **Görev kapısı olmayan 12 seviye:** 47, 49, 52, 55, 56, 59, 62, 64, 69, 72, 76, 80. Kapısı olan 23 seviye var.
- **Hiçbir yeni içerik gelmeyen 7 seviye** (mob, görev, ekipman yok): **47, 49, 52, 56, 59, 64, 69**.
- Mob/elit var ama görev kapısı yok: 55 (Nöbetçi), 62 (Bekçi), 72 (Gezgin + Abyss seti), 76 (Koruyucu), 80 (Naib).
- `ContentMap.py`'nin "3+ seviyelik boş aralık" ölçütü bu yüzden "yok" diyor; boşluklar tek seviyelik ve **dağınık**.

### 1.4 İlerleme modeli `[Model]` (`ProgressionModel.py`, 2026-10-09, kodun mevcut hali)

Hedef `[PLACEHOLDER]` 12–25 öldürme/seviye (`PROGRESSION_PLAN.md`). Bant ortalaması, seviye satırlarından alındı:

| Bant | Görevsiz | Görevli | `G` (grind=0) seviyeleri |
|---|---:|---:|---|
| 41–45 | 22,0 | 20,8 | – |
| **46–50** | 24,5 | 22,3 | – |
| **51–57** | 23,9 | 22,1 | – |
| **58–65** | 22,4 | 18,0 | 61 |
| **66–70** | 22,5 | 19,8 | – |
| **71–79** | 22,9 | 15,2 | 71, 73, 75, 77, 78 |

Okuma: hepsi 12–25 içinde; **boşluk hız değil çeşitlilik ve yoğunluk**. Görevli en düşük seviye Lv63 (11,1) ve Lv73/77/78 (12,0; yalnız yan görev hedefi sayılıyor, `G`). `BalanceReport.py` (en iyi mob, görevsiz) Lv46–79 için 15–22 öldürme, uyarı 0. Altın: `EconomyModel.py` gelir/gider Lv42–50 **110,5×**, 51–57 107,7×, 58–65 154,9×, 66–71 170,9×, 72–79 223,9× (sink açık karar: `DECISIONS_NEEDED.md` madde 3).

Yan görev XP kuralı (kodda): Lv61–78 yan görevleri eşiğin **%44–52'si** (`Q_RiftPurge` 450.000/957.212 …`Q_AbyssBastion` 1.000.000/1.931.720) `[Hesap]`; ana zincir adımları **%13** civarı (`Q_WinterReavers` 51.000/381.032 = %13,4) `[Hesap]`.

### 1.5 Boşluklar (kanıtlı liste)

| # | Boşluk | Kanıt |
|---|---|---|
| G1 | **Lv46–49'da tek paylaşılan cep:** Lv42/44/46/48 mobları aynı cepten eşit olasılıkla (1/4) çıkar; `Q_WinterReavers` (5) ve `Q_WinterWraiths` (5) ≈ 20'şer öldürme ister. Lv47/49'da hiçbir şey yok. | `MMOGameMode.cpp:938-949`; `ContentMap` |
| G2 | **Lv52/55/56:** Buzbağlı bandında görev yok (52, 55, 56), Lv52 ve Lv56 içeriksiz. Cep başına tek tür; Nöbetçi cebi toplam 11 öldürme ister, tür çeşidi yok. | `ContentMap`; görev kapıları 51, 53, 54, 57 |
| G3 | **Yarık:** 2 normal mob, her cepte tek tür; Lv59 ve Lv64 içeriksiz. Summer'da boş cep kapasitesi kıt (aşağıda G9). | `ContentMap`; `GameMode:1323-1325` |
| G4 | **Kül Hattı:** 2 normal mob, Lv69 içeriksiz. | `ContentMap` |
| G5 | **Uçurum:** Yaylada 2 normal mob; cep başına talep: Gezgin cebi 5+10+12+12 = **39**, Koruyucu cebi 5+6+10+12 = **33** öldürme. En yüksek ekipman Lv72; Lv73–80 (8 seviye ve son elit) için ekipman hedefi yok. | `MMOQuestTypes.cpp:259-313`; `ItemDatabase` |
| G6 | **Satıcı kapısızdır:** `BuyEquipment` yalnız fiyat, mesafe, çanta kapasitesi bakar; `RequiredLevel` yalnız **kuşanmada** denetlenir. Set fiyatları `12×(atk+def)+25×Lv+50` ile: Winter 7.232, Frostbound 8.552, Rift 9.732, Ashen 11.120, Abyss 12.368 altın `[Hesap]`; model geliri bunun 107–224 katı. Yeni set/elit/görev ödülü, altınla atlanabilir. | `MMOInventoryComponent.cpp:870-902`, `:654`, `:1363` |
| G7 | **Elit "boss" hissi zayıf:** Lv50+ elitlerin canı band normalinin ×1,09–1,19'u, hasarı ×1,06–1,09'u (HP = 50+8·Vit+12·Lv). Mekanik yalnız Slow/Poison ve `Rage` (+20 **sabit** saldırı): Lv57'de +%9,5, Lv80'de +%5,7. `RageII` (+55) tabloda var, mobda kullanılmıyor. | `MMOMobArchetype.cpp:390-446`; `MMOEffectDatabase.cpp:38,49`; `MMOMob.cpp:738-741` |
| G8 | **Efekt büyüklükleri Lv1 çağından:** `Poison` 8/sn × 8 sn = 64 toplam (sabit); Lv58+ mob vuruşu 222–350. **Lore ↔ kod:** Lore Kül Öncüsü/Kül Tiranı için "kor gibi yakar" der, kod `Poison` verir; `Burn` efekti yok. | `MMOEffectDatabase.cpp:33`; `MMOMobArchetype.cpp:251-253`; `Lore.md` "Kül Hattı" |
| G9 | **Harita kapasitesi:** Winter'da geometrik boşluk bol, Summer'da kıt (Bölüm 3). | PROGRESSION_PLAN notları |
| G10 | **Uzun gidiş-dönüş:** Tüm Winter görevleri hub'daki NPC'den alınıp orada teslim edilir ve ölünce `RespawnAtStart`; Lv66–80 cepleri hub'a 3.890–8.139 u. İleri kamp yok. | `MMOCharacter.cpp:1766`; `MMOQuestComponent.cpp:307-335` |
| G11 | **Beceri boşlukları** (ayrı iş): dört sınıfta 45→58 (13), 58→70 (12), 70→80 (10) seviye; `ValidateData` bunlar için 12 UYARI veriyor. | `ValidateData` çıktısı |

---

## 2. Bant başına öneriler

Genel kural: yeni sayılar komşu arketiplerle **ara değer** (Str/Vit/hasar/XP tek yönlü artar). HP = 50+8·Vit+12·Lv `[Kesin]`. `AggroRange` **600** (QA kuralı). Yalnız mevcut mekanikler: tüm moblar yakın dövüş, `OnHitEffectId/Chance`, `EnrageEffectId/HealthPct`, `SetWarrokVisual` ya da Manny+renk. Yeni mekanik gerektirenler ayrıca işaretli.

### 2.1 Yeni normal mobların tamamı `[PLACEHOLDER]` (MakeMob sırasıyla: Str, Vit, Hasar, Ham XP, Altın min–max, Hız, Ölçek)

| ID | Ad (ASCII) | Lv | Str | Vit | Hasar | Ham XP | Altın | Hız | Ölçek | HP | Efekt | Görsel | Ara değer kontrolü |
|---|---|---:|---:|---:|---:|---:|---|---:|---:|---:|---|---|---|
| `WinterWolf` | Don Kurdu | 47 | 117 | 112 | 165 | 20.500 | 2.800–5.050 | 450 | 1,2 | 1.510 | Slow %35 | Manny+renk | Reaver 46 (115/108/160/19.000) < … < Wraith 48 (120/116/169/22.000) |
| `WinterBehemoth` | Buz Devi | 49 | 123 | 121 | 175 | 25.000 | 3.150–5.600 | 262 | 1,6 | 1.606 | Slow %35, Öfke <%40 | Warrok | Wraith 48 < … < Scout 51 (128/122/182/30.000) |
| `FrostboundSworn` | Buzbağlı Yeminli | 53 | 133 | 131 | 190 | 33.000 | 3.550–6.200 | 350 | 1,4 | 1.734 | Öfke <%45 | Warrok | Scout 51 < … < Sentinel 55 (138/140/198/36.000) |
| `RiftWhisperer` | Yarık Fısıldayanı | 60 | 166 | 166 | 230 | 48.500 | 4.550–7.900 | 420 | 1,3 | 2.098 | Slow %35 | Manny+renk | Seeker 58 (160/154/222/45.000) < … < Keeper 62 (172/178/238/52.000) |
| `RiftWalker` | Yarık Yürüyeni | 64 | 182 | 188 | 250 | 56.000 | 5.100–8.800 | 300 | 1,5 | 2.322 | Zehir %30, Öfke <%40 | Warrok | Keeper 62 < … < Vanguard 66 (190/196/262/60.000) |
| `AshMarcher` | Kül Yürüyeni | 69 | 201 | 216 | 280 | 70.000 | 6.300–10.600 | 330 | 1,45 | 2.606 | `Burn` %35 (S2) , Öfke <%40 | Warrok | Binder 68 (198/214/272/66.000) < … < Strider 72 (208/222/300/82.000) |
| `AbyssEcho` | Uçurum Yankısı | 74 | 216 | 236 | 311 | 88.000 | 7.500–12.200 | 440 | 1,3 | 2.826 | Slow %40 | Manny+renk | Strider 72 < … < Guardian 76 (224/252/322/95.000) |
| `AbyssEnvoy` | Naibin Elçisi | 78 | 235 | 268 | 336 | 104.000 | 8.500–13.800 | 280 | 1,6 | 3.130 | Slow %35, Öfke <%40 | Warrok | Guardian 76 < … < Regent 80 elit (245/285/350/140.000) |

`[Hesap]`: tüm sütunlar komşu satırlar arasında tek yönlü artıyor; `AggroRange` 600 (tablo dışı sabit). Roller mevcut kalıbı tekrar eder (hızlı/ağır/Warrok vurucu). Hızlı hayalet/kurt roller Lore'da var (Kurtlar, Hayaletler). **Buz Devi, Fısıldayan, Yankı, Elçi Lore'da yok**; metni `metin-yazari` yazar (2.7).

### 2.2 Paket W1 — Kış Diyarı iç çemberi (Lv46–50, Winter) · M

**Tema:** fırtınanın merkezi; Hükümdar'ın sürüsü ve devleri. Lore'a uyum: "fırtına hiç dinmez, ufuk görünmez"; Kurtlar ve soğuk dev eklemesi yeni tür sayılır.

- **Cep düzeni (önerilen):** W1 bandı 42–48 → **42–44** (satır: `WinterSpawner->MaxMobLevel = 48;` → `44;`). Yeni iki cep: **P46** bant 46–47 (`WinterReaver` + `WinterWolf`), **P48** bant 48–49 (`WinterWraith` + `WinterBehemoth`). Hedef olasılık 1/4 → **1/2**: `Q_WinterReavers`/`Q_WinterWraiths` ≈ 20 → ≈ 10 öldürme. Koordinat: Bölüm 3, **probla bulunacak**. Bu yeni cepler Kış Hükümdarı (1.500 u, leash 1.800) ve W1'den ≥ 1.800 u, hub'dan ≥ 3.000 u olmalı `[öneri]`.
- **Yedek (koordinatsız, geçici):** W1 bandını 42–49'a çıkar. Bedel `[Hesap]`: havuz 4 → 6 tür, her tür 1/6; `Q_WinterReavers` ≈ 30 öldürme; Lv42–44 oyuncusu Lv49 mob görür.
- **Yan görevler** (ana zinciri kilitlemez):

| ID | Ad | Kapı | Önkoşul | Hedef × adet | Ham XP | Eşiğin % `[Hesap]` | Altın | Ödül |
|---|---|---:|---|---|---:|---:|---:|---|
| `Q_WinterHowls` | Don Sürüsü | 47 | `Q_WinterReavers` | `WinterWolf` × 8 | 26.000 | %6,3 | 8.000 | 5× Büyük Can İksiri |
| `Q_WinterColossus` | Buz Devleri | 49 | `Q_WinterWraiths` | `WinterBehemoth` × 6 | 30.000 | %6,3 | 9.000 | 5× Büyük Mana İksiri |

  XP kararı: **pace-nötr** (~%6,5), çünkü model Lv46–50'de zaten 22,3 öldürme/seviye (hedefin üst yarısı). Hızlandırmak istenirse seçenek ladder'ı: nötr %6,5 → orta ~%25 → Lv61+ yan görev kalıbı ~%45–50. Ölçümsüz karar verme.
- **Elit/boss fikri (opsiyonel, P3):** nadir elit `WinterAlpha` "Don Alfası" Lv48, `ForcedArchetypeId` + `TopUpInterval` 300 `[PLACEHOLDER]`; Slow %50; elit loot havuzunda Winter seti (`MaxReq=42`, `≥40`). Görev yok, "dolaşan nadir" hissi. Yeni cep gerekir.
- **Ekipman:** yeni set yok (Winter seti Lv44–50 görevlerinden tamamlanıyor; sonraki set Lv51).

### 2.3 Paket W2 — Buzbağlı Sınır tamamlayıcı (Lv51–57, Winter) · M

**Tema:** Buz Kapısı'nın yeminli askerleri; "Kapı, terazinin milidir". `FrostboundSworn` "yeminini bozmayan" bir ara kademe: Gözcü (hızlı), **Yeminli (denge)**, Nöbetçi (ağır).

- **Cep:** yeni **P53** (Lv53 tek bant), Gözcü/Nöbetçi/Muhafız'dan ≥ 1.800 u, hub'dan ≥ 3.000 u. Koordinat probla.
- **Yan görevler** (Lv52/55/56 kapı boşluklarını kapatır; 51–57'nin tüm seviyeleri 51, 52, 53, 54, 55, 56, 57 kapılı olur):

| ID | Ad | Kapı | Önkoşul | Hedef × adet | Ham XP | Eşiğin % | Altın | Ödül |
|---|---|---:|---|---|---:|---:|---:|---|
| `Q_FrostboundSworn` | Buzbağlı Yeminliler | 52 | `Q_FrostboundScouts` | `FrostboundSworn` × 6 | 40.000 | %6,9 | 12.000 | 5× Büyük Can İksiri |
| `Q_FrostboundWatch` | Buz Nöbeti | 55 | `Q_FrostboundSentinels` | `FrostboundSentinel` × 8 | 45.000 | %6,5 | 13.500 | 5× Büyük Mana İksiri |
| `Q_FrostboundOath` | Son Yemin | 56 | `Q_FrostboundSworn` | `FrostboundSworn` × 8 | 48.000 | %6,5 | 14.500 | 5× Büyük Can İksiri |

  Nöbetçi cebi talebi 11 → 19 öldürme (cep başına 6 eşzamanlı mob, 4 sn yenilenme: dar boğaz yenilenme değil **eşzamanlı oyuncu sayısı**).
- **Ekipman: bilerek yok.** Elit ganimet havuzu `RequiredLevel ≤ MobLevel && ≥ MaxReq−2` `[Kesin: MMOMob.cpp:1136-1150]`; Lv57 Muhafız için `MaxReq=51`, havuz = Frostbound seti. Lv52–56 arasına yeni set koymak Muhafız'ın havuzunu o sete kaydırır ve Frostbound'u elitten çıkarır.
- **Elit fikri (P3):** nadir `FrostboundCaptain` "Buzbağlı Yüzbaşı" Lv54, 300 sn, Slow %50, yeni cep.

### 2.4 Paket R — Yarık derinleşir (Lv58–65, **Summer**) · S–M

**Tema:** "birbirine fısıldayan cepler" (Lore). Yeni koordinat **yok**; mevcut iki cebin bandı genişler.

- **Düzen:** Arayıcı cebi bandı 58 → **58–60** (`RiftWhisperer`), Bekçi cebi 62 → **62–64** (`RiftWalker`). Bedel `[Hesap]`: `Q_RiftSeekers` (5) + `Q_RiftPurge` (10) = 15 → ≈ 30 öldürme; `Q_RiftKeepers` (5) + `Q_RiftWatch` (6) + `Q_RiftDepths` (10) = 21 → ≈ 42 öldürme (cepte 1/2 olasılık).
- **Kod gerektiren iki küçük nokta (`[Kesin]`):**
  1. `SpawnRift` lambda'sı `Min=Max=Lv` ayarlıyor (`GameMode:1302-1322`). Bandı genişletmek için lambda'ya `LvMax` parametresi eklenir. `ValidateData.py` lambda parametrelerini literal olarak bağlar (`ValidateData.py:224-255`) ve `int32 LvMax` ile çalışmalı.
  2. **`Scripts/TestValidateData.py:27-28` kırılır:** "Lambda spawner bandi bos" senaryosu `SpawnRift\(FVector\(800\.f, -4800\.f, 50\.f\), 58,` desenini `99,` ile değiştirip `Lv99-99 bandinda ...` bekliyor. Çağrı `58, 60,` olunca `99, 60` → mesaj `Lv99-60` olur ve test **KALIR**. Çözüm: aynı commit'te senaryonun beklenen metnini/desenini güncelle (ör. her iki parametreyi de `99` yapan desen). `[Hesap]`, çalıştırılmadı.
- **Yan görevler:**

| ID | Ad | Kapı | Önkoşul | Hedef × adet | Ham XP | Eşiğin % | Altın | Ödül |
|---|---|---:|---|---|---:|---:|---:|---|
| `Q_RiftWhispers` | Yarık Fısıltıları | 59 | `Q_RiftSeekers` | `RiftWhisperer` × 8 | 60.000 | %6,9 | 18.000 | 5× Büyük Can İksiri |
| `Q_RiftWalkers` | Yarık Yürüyenleri | 64 | `Q_RiftDepths` | `RiftWalker` × 8 | 72.000 | %6,5 | 22.000 | 5× Büyük Mana İksiri |

  Model Lv58–65'te görevli 18,0; Lv63'te 11,1 (hedef alt sınırı 12'nin altında). Bu yüzden bantta XP nötr tutuldu. Kapı 62'de görev yok kalır (Bekçi var, 62 için ayrı yan görev gerekirse bu pakette değil).
- **Yedek:** yeni Summer cebi (grid taraması şart, kapasite bilinmiyor `[Tahmin: 0–2]`).
- **Elit fikri:** önerilmez; Summer'da ek nadir elit yeni koordinat ister (kapasite kıt).

### 2.5 Paket A — Kül Hattı (Lv66–70, Winter) · M

**Tema:** "Kül yürüyor." Buz ve ateş aynı bayrak altında.
- **Cep:** yeni **P69** (Lv69). Yedek (koordinatsız): Kül Bağlayıcı cebi bandı 68–69; bedel `Q_AshBinders` 5 + `Q_AshLine` 6 = 11 → ≈ 22 öldürme.
- **Yan görev:** `Q_AshMarchers` "Kül Yürüyenleri", kapı 69, önkoşul `Q_AshBinders`, `AshMarcher` × 8, ham XP 90.000 (%6,6), altın 27.000, 5× Büyük Can İksiri. Lv69 kapı boşluğunu kapatır.
- **Lore uyumu (S2):** `Burn` efekti (`PeriodicDamage`, tek `MakeEffect` satırı; büyüklük ve süre `[PLACEHOLDER]`, Poison'dan yüksek) → `AshVanguard`, `AshTyrant`, `AshMarcher` vuruşu. `OnHitEffectId` ValidateData'da denetlenmez; `Burn` kimliği yazım hatası **sessiz** olur (Bölüm 5).
- **Ekipman:** yok (Ashen Lv66 → Abyss Lv72 aralığı 6).
- **Elit fikri (P3):** nadir `AshCinder` "Kül Kâhyası" Lv68.

### 2.6 Paket U — Uçurum Yaylası (Lv71–80, Winter) · M (+ L opsiyonel)

**Tema:** her kapı aynı boşluğa açılır; fısıltı her yönden gelir. Naib "yeryüzündeki ses": yankılar ve elçiler onun sesini taşır.
- **Cepler:** yeni **P74** ve **P78** (Gezgin/Koruyucu cepleri yoğun: 39 / 33 öldürme). P78 Naib'e giden yolda olmalı ama Naib cebinden ≥ 1.800 u (leash). Koordinat probla. Yedek (koordinatsız): mevcut cepleri genişletme önerilmez (39/33 talep ×2 olur).
- **Yan görevler:**

| ID | Ad | Kapı | Önkoşul | Hedef × adet | Ham XP | Eşiğin % | Altın | Ödül |
|---|---|---:|---|---|---:|---:|---:|---|
| `Q_AbyssEchoes` | Uçurum Yankıları | 72 | `Q_AbyssStriders` | `AbyssEcho` × 8 | 100.000 | %6,5 | 30.000 | 5× Büyük Mana İksiri |
| `Q_AbyssEnvoys` | Naibin Elçileri | 76 | `Q_AbyssEchoes` | `AbyssEnvoy` × 8 | 120.000 | %6,7 | 36.000 | 5× Büyük Can İksiri |

  Lv71–79 modelde zaten 15,2 ve 5 `G` seviyesi: XP'yi **nötr** tut.
- **Lv78 ekipman seti "Taht" (4 parça, yalnız veri):** `ThroneBlade` 214 saldırı, `ThronePlate` 131, `ThroneHelm` 55, `ThroneBoots` 74 savunma, hepsi Lv78 `[PLACEHOLDER]` (önceki kademe farkları 18/22/24 → +26; 11/13/15 → +17; 6/6 → +7; 8/9 → +10). Satıcı fiyatları `[Hesap]`: 4.568 + 3.572 + 2.660 + 2.888 = 13.688. İki **etkileşim** (`[Kesin: MMOMob.cpp:1136-1168]`): (a) Naib (`MobLevel` 80) elit havuzu `MaxReq=78`, `≥76` → yalnız Taht seti; **Abyss seti elitten düşer** (görev ödülü ve satıcıda kalır). (b) Normal moblar `≤ MobLevel+3` havuzundan düşürür: Lv76 Koruyucu Taht parçası düşürebilir. `Q_AbyssRegent` ödülü `AbyssBlade` → `ThroneBlade` (kapı 79, set Lv78 ≤ 79+2: `ValidateData` geçer) ya da Abyss kalıp yalnız elit kapısı olarak Taht seti. **Karar G6 ile bağlı** (satıcı kapısı olmadan bu set satın alınır).
- **Boss fikri (L, mantık):** Naib için fazlı karşılaşma (ör. %60/%30 canda çağrı: `AbyssEnvoy` ekleri). Mob davranışında yok (`FMMOMobArchetype` yalnız Slow/Poison + Öfke); yeni sistem gerekir. **PIE ölçümünden önce önerilmez.**
- **Elit fikri (P3, veri):** nadir `AbyssSteward` "Naibin Kâhyası" Lv78, 300 sn, yeni cep (3 yeni cep olur).

### 2.7 Sistemik paketler

| Kod | Ne | Tür | Boyut | Not |
|---|---|---|---|---|
| S1 | Lv50+ elitlerde `Rage` → `RageII` (+55) ve Vit/hasar artışı | veri | S | `[Hesap]`: Rage +20 Lv57 elitte +%9,5, RageII +55 → +%26. Sayılar için **önce** elit öldürme süresi (TTK) ölç; elit can oranı ×1,09–1,19 → ×1,5–2 hedefi `[PLACEHOLDER]` |
| S2 | `Burn` efekti + Kül mobları | veri | S | G8; HUD etiketi 3 harf (ör. `YNK`) |
| S3 | Satıcıda seviye kapısı veya fiyat ölçekleme | **mantık** | S–M | G6; `BuyEquipment`'e `Level < RequiredLevel-X` reddi ya da fiyat çarpanı; `TestVendorEconomy.ps1` beklentileri güncellenir. **Karar bekler** (DECISIONS 3 ile birlikte) |
| S4 | İleri kamp (Kül ve Uçurum): görevli + satıcı + şifacı + depocu | GameMode spawn satırları (sınıflar var) | M | G10; görev/satıcı/depocu denetimi **tüm NPC'leri tarar** `[Kesin: MMOQuestComponent.cpp:329, MMOInventoryComponent.cpp:850,1096]`, çoklu NPC kodsuz çalışır. Ölünce doğuş hâlâ hub (`RespawnAtStart`); kamp doğuş noktası değil. Konum probla (cep zarfı ≥ 1.300 u) |
| S5 | Beceri boşlukları 45→58→70→80 | veri | M | Ayrı belge. Not `[Hesap]`: `SKILL_GAP_LEVELS=5`, sınıf başına tek beceri 6/7, 6/6, 5/5 bırakır: UYARI **sayısı artar** (3 → 6 satır/sınıf), susturmak için aralık başına ≥ 2 beceri |
| S6 | Fazlı boss (Naib) | **mantık** | L | 2.6 |
| S7 | Winter rota açıklığı betiği (`ProbeThawRoute.py` Summer'a kilitli: `MAP = .../Summer_scene`) | betik | S | Winter için hub → cep nav yolu tam mı |

### 2.8 Lore ve metin işi

`metin-yazari` → `docs/Lore.md` bölümlerine (mevcut biçim: `- **Başlık** *(Q_ID)* — "alış"` + `- *Dönüşte:* "teslim"`), sonra `Scripts/GenQuestDialogue.py` (Lore.md'den `MMOQuestDialogue.cpp` üretir). Eksik replik oyunu bozmaz (boş döner). Eklenecekler: Kış Diyarı (Don Kurdu, Buz Devi), Buzbağlı (Yeminli), Yarık (Fısıldayan, Yürüyen), Kül Hattı (Yürüyen), Uçurum (Yankı, Elçi) mob satırları; 10 yan görev replikleri; Taht seti 4 satırı. Lore tutarlılık notları: Kül mobları "yakar" (S2'ye bağlı); Kuzey Yolu sırası ve Doran'ın "terazi" dili korunur; Naib "taht boş kalmaz" ile Taht seti uyumlu.

---

## 3. Harita ve yerleşim riski

**Kural: Her yeni cep, kabul öncesi oyunun kendi koduyla ölçülür. Editör Python probu tek başına geçerli sayılmaz.** Kanıt `[Kesin: PROGRESSION_PLAN, 3 Ekim]`: editör probu "geçti" dediği Nöbetçi `(-2800,4400)` oyunda **2/64**, Gezgin `(-5200,3600)` **23/64** verdi (iz sürme başlangıcında dağ çıkıntısının basit çarpışması).

1. **Oyun içi doğrulama (zorunlu).** Editör kapalı. Winter için `-Map` **mutlaka verilmeli** (varsayılan Summer):
   `powershell -File Scripts\SmokeTest.ps1 -Map Winter_scene -Cmds "MMOProbeSurface X Y 225,MMOProbeSurface X2 Y2 225,quit"`
   Komutlar virgülle zincirlenir; tek koşuda çok aday taranabilir (grid için ayrı betikle komut listesi üretilebilir). `MMOProbeSurface` yüzeyi kendisi bulur (Z = yüzey + 50) ve `MMOProbeSpawn` çalıştırır: spawner'ın **gerçek** `FindSafeSpawnLocation` kodunu 64 kez koşturur ve `[MMOPROBE] (x,y,z) r=225 ok=N/64 trace_miss=… egim_yukseklik=… nav=…` satırı yazar `[Kesin: MMOGameMode.cpp:328-373]`. Satırlar `SmokeTest.ps1` ekranında **görünmez**: log `%TEMP%\mmo_smoke_Winter_scene.log` içinden `Select-String "MMOPROBE"` ile okunur `[Kesin: SmokeTest.ps1:12,23]`.
2. **Eşik:** `ok ≥ 60/64` (elit için yarıçap 150, aynı eşik). Finalistler **×10** koşturulur (Çözülme kaydı: 59–64, ort. 62,4). Sınırda kalanlar elenir; "komşusu 43/64" gibi dar şeritler bilinen tuzak.
3. **Mesafe kuralları `[Kesin]`:** cep-cep ≥ ~1.400 u; mob fark etme zarfı 225+450+600 = ~1.275 u; `LeashRange` 1.800 (çekilen mob 1.800 u'ya kadar kovalar); Winter hub'a ≥ ~3.000 u (mevcut en yakın cep 3.000); cep merkezi nav sınırından ≥ ~700 u içeride (Naib gezinmesi sınırı aşıp taşındı). Mevcut W1 ↔ Kış Hükümdarı **1.500 u < leash 1.800**: yeni cepler bunu tekrar etmesin; mevcut çift PIE'de gözlenmeli.
4. **Nav sınırları `[Kesin]`:** Winter X ±8.000, Y −8.500…5.500, Z ±1.500 (`ExtendWinterNavigation.py`); Summer X/Y ±8.500. Yeni cepler yalnız bu sınırlarda ve Recast verisi üretilmiş bölgede.
5. **Hub'a tam nav yolu:** editörde ölçülür (Summer için `Scripts/ProbeThawRoute.py` var; **Winter eşdeğeri yok**, S7). Yol kısmi ise aday elenir (Winter doğu `(3000,-1500)` bu yüzden bir kez elendi).
6. **Winter kapasitesi `[Hesap]`:** mevcut 11 cebe ≥ 1.800 u, hub'a ≥ 3.000 u, X ±7.000 / Y −7.800…4.800, 500 u adım ile **321** grid hücresi geometrik olarak kalıyor (bölge dağılımı: batı-güney 76, doğu-orta 53, doğu-güney 44, batı-orta 41, merkez-kuzey 38, batı-kuzey 31, merkez-güney 20, doğu-kuzey 18). Bu yalnız geometri; zemin/nav/çarpışma **ölçülmedi**. 3 Ekim Winter grid probu "onlarca uygun cep" verdi `[Kesin: PROGRESSION_PLAN]`: Winter için 6 yeni cep (W1 ×2, W2, A, U ×2) makul.
7. **Summer kapasitesi kıt `[Tahmin]`:** 3 Ekim grid (±8.000, 800 u adım) yalnız 3 uygun cep verdi, bunlar Rift'e gitti; sonra Çözülme eklendi. Bu yüzden R paketi **koordinatsız** tasarlandı. Summer'da yeni cep gerekirse önce grid taraması (aynı `MMOProbeSurface` zinciri), kabul öncesi.
8. **Rota riski (Summer):** Rift mobları ~1.275 u'dan oyuncuyu fark eder; Lv7–40 oyuncu Lv58+ mob çekmesin (QA'nın düzelttiği hata). Mevcut Rift cepleri eski cebe ~3.400 u; R paketi konumları değiştirmez, yalnız aynı cepte Lv60/64 mob da çıkar. Yeni bir Summer cebi eklenirse bu rota kontrolü (`ProbeThawRoute.py` ölçütleri) yeniden gerekir.
9. **`RunAllChecks.ps1` cep kalitesini ölçmez:** Duman testi yalnız `Error:|Fatal|Ensure|Access violation` sayar ve `SpawnOne … result=MMOMob` dogumunu raporlar `[Kesin: SmokeTest.ps1:20-24]`. 10/64 veren cep de "GEÇTİ" olur. Tek ölçüt `[MMOPROBE]`.
10. **NPC/ileri kamp (S4):** NPC ayak noktası da zemin+nav ister; cep zarfına (≥ ~1.300 u) ve hub kuralına (kayıtlı: mob cebinden ≥ 2.750 u) uy.

PIE doğrulanacaklar (`[Tahmin]`, hepsi oynanmadı): her yeni cebin görünürlüğü ve erişimi; yeni mobların saldırı/animasyonu (Warrok hizası −89/−90); yan görevlerin önkoşul sırasıyla listede çıkması; Taht seti ve satıcı etkileşimi; 6 mob/cep ile iki oyunculu dar boğaz.

---

## 4. Önceliklendirme

Boyut: **S** = tek commit, birkaç satır, probsuz · **M** = birden çok dosya + 1–2 probe turu · **L** = yeni C++ davranışı + PIE. "Veri satırı" = mevcut `MakeMob/MakeQuest/MakeItem/MakeEffect` ve `SpawnFrostbound/SpawnRift` kalıbında satır (hepsi `Source/` altında, **derleme gerekir**, yeni mantık yok). "Mantık" = yeni C++ davranışı.

| Sıra | Paket | Neden önce / sonra | Tür | Boyut | Bağımlılık |
|---:|---|---|---|---|---|
| 0 | Karar D1: satıcı kapısı (S3) ve altın sink'i | Yeni set/ödül anlamı buna bağlı | karar | – | `DECISIONS_NEEDED` 3 |
| 1 | **W1** Lv46–50 | Winter ana hat; G1 en yoğun (1/4 seyrelme) | veri + 2 cep | M | S7 yoksa nav yolu elle |
| 2 | **W2** Lv51–57 | Winter ana hat; 3 kapı boşluğu | veri + 1 cep | M | W1 |
| 3 | **R** Lv58–65 (Summer) | Koordinatsız, en ucuz; ama test dosyası güncellemesi şart | veri + lambda + test | S–M | W2 (Lv58'e geçiş) |
| 4 | S2 + **A** Lv66–70 | Lore uyumsuzluğu (Burn) + Lv69 | veri + 1 cep | M | R |
| 5 | **U** Lv71–80 | Son bant, en çok cep (2) ve Taht seti | veri + 2 cep | M | D1, A |
| 6 | S4 ileri kamp | Uçurum'da G10 gerçek acı verir; PIE ölçümüyle birlikte | spawn satırları | M | A, U |
| 7 | S1 elit ayarı | TTK ölçümü olmadan körlemesine | veri | S | PIE verisi |
| 8 | S5 beceri, S6 fazlı boss | Ayrı belge / yeni sistem | veri / mantık | M / L | PIE verisi |
| P3 | Nadir elitler (Alfa, Yüzbaşı, Kâhya, Kül Kâhyası) | Her biri +1 cep, düşük değer | veri + cep | S–M ×4 | ilgili paket |

Lv46–57 Winter ana hattı (1–2) önce; en ucuz kazanç R (3). W ve A, U için cep probu iş yükünün çoğu.

---

## 5. Uygulama sırası ve doğrulama

Önkoşul her adım için: editör **kapalı**; küçük commit; mob + spawner + görev **aynı commit** (yoksa `UYARI: Mob X hicbir spawner'da uretilmiyor (olu icerik)` ya da `HATA: Q_...: hedef ... hicbir spawner'da uretilmiyor (gorev tamamlanamaz)`, `ValidateData.py:296-301`). Başkasının bekleyen hunk'ı varsa yalnız kendi hunk'ını stage'le.

**`ValidateData.py` neyi denetler `[Kesin: kod okundu]`:** kimlik tekilliği (mob/item/görev/beceri/efekt), `MakeQuest` çağrı sayısı = ayrıştırılan sayı, `KillSpecificMob` hedefi mob tablosunda, hedef bir spawner'da **üretiliyor** (HATA), önkoşul görev var ve döngü yok, `ForcedArchetypeId` tabloda, spawner bandı boş değil (HATA), kapı − hedef mob seviyesi ≤ 5 (UYARI), zincir geri gitmiyor (UYARI), ödül ekipmanı kapı+2 / MaxLevel (UYARI/HATA), satıcı yuva kapsamı (UYARI), beceri alanları, XP knot'ları. **Denetlemez:** koordinat, nav, cep mesafesi, stat monotonluğu, mobun `OnHitEffectId`/`EnrageEffectId` yazımı, `RewardConsumableId` yazımı, XP düzgünlüğü, dengeleme.

**Adım tablosu** (beklenen satırlar `[Hesap]`, çalıştırılmadı; baz çizgisi `[Kesin]`: `Mob: 51 (elit 13)  Item: 53 … Gorev: 79  Beceri: 81  Efekt: 12  Spawner: 29`, 20 beceri UYARI, `Veri denetimi temiz.`):

| # | Adım | Çapa (yorum) | Doğrulama komutu | Beklenen çıktı |
|---:|---|---|---|---|
| 0 | Baz çizgisi | – | `$PY Scripts/ValidateData.py` · `$PY Scripts/ProgressionModel.py` | yukarıdaki satır · bant tablosu 1.4 |
| 1 | **W1**: 2 mob + `Q_WinterHowls/Colossus` + W1 bandı 42–44 + P46/P48 | mob: `// --- ELIT / BOSS'lar` üstü; görev: `// --- Bagimsiz (yan) gorevler ---` üstü; spawner: `SpawnFrostbound(... AbyssRegent ...)` sonrası | `$PY Scripts/ValidateData.py` · `$PY Scripts/TestValidateData.py` · `$PY Scripts/ProgressionModel.py` | `Mob: 53 (elit 13)  … Gorev: 81 … Spawner: 31` · `Veri denetimi temiz.` · negatif test geçer (W1'e dokunan senaryo yok: `TestValidateData.py` senaryoları `ThirdZone`, `SpawnRift(800,-4800 … 58,` ve eşya/knot satırlarını bozuyor) · model Lv46–50 görevli ≈ aynı (±2 öldürme `[Tahmin]`) |
| 1b | W1 **oyun içi cep probu** (adaylar ×10) | – | `SmokeTest.ps1 -Map Winter_scene -Cmds "MMOProbeSurface …"` + `Select-String MMOPROBE` | her cep `ok≥60/64` |
| 2 | **W2**: `FrostboundSworn` + 3 görev + P53 | aynı | `ValidateData` · `TestValidateData` | `Mob: 54 … Gorev: 84 … Spawner: 32` · temiz |
| 2b | P53 probu | – | SmokeTest (Winter) | ≥60/64 |
| 3 | **R**: 2 mob + 2 görev + `SpawnRift` `LvMax` + **`TestValidateData.py` senaryo güncellemesi** | `auto SpawnRift = [&]` ve çağrılar | `ValidateData` · `TestValidateData` | `Mob: 56 … Gorev: 86 … Spawner: 32` (spawner sayısı değişmez) · temiz · **negatif test, güncelleme yapılmazsa KALIR** (2.4) |
| 3b | Rift bant genişlemesi: mevcut 2 cep koordinatı değişmediği için yeni prob gerekmez; **yine de** oyun içi duman: | – | `SmokeTest.ps1 -Map Summer_scene` | hata 0, Mob doğumu > 0 |
| 4 | **S2 + A**: `Burn` efekt satırı, 1 mob, 1 görev, P69 | `MakeEffect` bloğu sonu; mob/görev/spawner aynı | `ValidateData` | `Efekt: 13`, `Mob: 57`, `Gorev: 87`, `Spawner: 33` · temiz · **`Burn` yazım hatası görünmez** (satır grep'le elle doğrula) |
| 4b | P69 probu | – | SmokeTest (Winter) | ≥60/64 |
| 5 | **U**: 2 mob + 2 görev + 4 ekipman + `Q_AbyssRegent` ödül değişikliği + P74/P78 | `MakeItem` bloğu sonu (`return Table;` üstü) | `ValidateData` · `TestValidateData` | `Mob: 59 … Item: 57 … Gorev: 89 … Spawner: 35` · temiz · satıcı yuva UYARISI yok (Lv72→78 farkı 6) |
| 5b | P74/P78 probu | – | SmokeTest (Winter) | ≥60/64 |
| 6 | Lore/diyalog | `docs/Lore.md` | `$PY Scripts/GenQuestDialogue.py` sonra `git diff` | yalnız `MMOQuestDialogue.cpp` değişir; yeni `Q_` kimlikleri girdi |
| 7 | Model ve harita belgeleri | – | `$PY Scripts/ProgressionModel.py` · `$PY Scripts/EconomyModel.py` · `$PY Scripts/ContentMap.py` | 12 görevsiz seviyenin 10'u dolar (47, 49, 52, 55, 56, 59, 64, 69, 72, 76 → kapı alır; 62 ve 80 açık kalır) `[Hesap]`; `ContentMap.py --write` `docs/CONTENT_MAP.md`'yi üretir (elle düzenleme) |
| 8 | **Tam doğrulama** | – | `powershell -File Scripts\RunAllChecks.ps1 -SkipBuild`, sonra derlemeli: `-FullCompile` ve tüm adımlar | Adımlar: veri denetimi, StaticLint, PlaytestMetrics fixture, negatif testler, (derleme), JSON/mağaza/bölge geçişi/görev ödülü vb. testleri, Summer ve Winter duman, NetSmoke. **Hepsi `GECTI` olsa da cep kalitesi kanıtı değil** (Bölüm 3, madde 9) |
| 9 | PIE + ölçüm | – | `PLAYTEST_RUNBOOK.md` Kapı 3; `python Scripts\PlaytestMetrics.py Saved\Logs\MyProject.log` | seviye başına öldürme (hedef 12–25 `[PLACEHOLDER]`); elit TTK (S1 girdisi); cep başına eşzamanlı oyuncu |

Not: adım 7'deki seviye sayımı kapı hesabıdır: 12 görevsiz seviyeden 10'una yeni kapı geliyor (47, 49, 52, 55, 56, 59, 64, 69, 72, 76); 62 (Bekçi) ve 80 (Naib) görevsiz kalır, 80'de XP kullanılmadığı için bilerek.

## 6. Riskler ve karar bekleyenler

| Konu | Not | Etiket |
|---|---|---|
| Satıcı kapısı (G6) | Yeni setlerin değeri buna bağlı; seçenekler: seviye kapısı, fiyat ölçekleme, kabul | karar (DECISIONS 3) |
| Seyrelme | Cep paylaşımı hedef başına öldürmeyi artırır (W1: 20 → 10 iyileşir, R: 15 → 30 kötüleşir) | model; PIE ölçümü |
| `TestValidateData.py` | R paketinde güncellenmezse KALIR | kesin |
| `Rage` zayıflığı | +20 sabit; Lv57 elitte %9,5 | veri, ölçümle |
| W1 ↔ Kış Hükümdarı 1.500 u | leash 1.800'den kısa; mevcut | PIE'de gözlenecek |
| Yeni türler Lore'da yok | Buz Devi, Fısıldayan, Yankı, Elçi, Yeminli | `metin-yazari` onayı |
| Summer kapasitesi | R dışında Summer'a cep eklenmesi önerilmiyor | `[Tahmin]` |
| Ölçülmemiş yürüme hızı | Hub → Lv72 cebi 8.139 u; süre bilinmiyor | `[Tahmin]` |
| Tüm sayılar | `[PLACEHOLDER]`; model oynanış değil; hiçbir paket PIE'de oynanmadı | – |
