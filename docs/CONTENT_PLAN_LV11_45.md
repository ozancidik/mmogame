# İçerik planı: Lv11–45 (uygulamaya hazır)

**Tarih:** 2026-10-05, `master` 3a758dd üzerinde. **Durum:** yalnız plan; kod, veri, mevcut belge ve git'e dokunulmadı. Tek çıktı bu dosya.

**Yöntem (sayılar nereden):** Önerilen veri satırları repo **kopyasına** işlendi (gerçek depoya değil) ve `ValidateData.py`, `TestValidateData.py`, `StaticLint.py`, `BalanceReport.py`, `ProgressionModel.py`, `ContentMap.py` kopyada çalıştırıldı. Aşağıdaki "beklenen çıktı" satırları o koşulardan birebir alınmıştır. Betikler: `"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/X.py` (bu belgede `$PY Scripts/X.py` diye kısaltıldı).

**Etiketler:** `[KOD]` dosyadan okundu · `[MODEL]` betikle hesaplandı, oynanış değil · `[VARSAYIM]` PIE'de görülmedi · `[PLACEHOLDER]` ölçümle değişecek sayı.

> **Önemli (paralel çalışma):** Şu an çalışma ağacında başka bir ajanın değişmemiş (commit'lenmemiş) düzenlemeleri var: `MMOGameMode.cpp`, `MMOHUD.cpp`, `MMOCharacter.cpp/.h`, `MMOPlayerController.cpp/.h`, `MMOPartyComponent`, `MMOClanComponent`. Bu planın C++ ekleri bu dosyalara **satır numarasıyla değil yorum çapasıyla** yerleştirilmeli (aşağıda her ek için çapa verildi). `MMOGameMode.cpp`/`MMOHUD.cpp`/`MMOCharacter.cpp` parçalarını, o düzenlemeler commit'lenmeden başlatma.

## 0. Özet

| Parça | Ne | Dosya | Sayı | Bağımlılık |
|---|---|---|---|---|
| A1 | 3 yeni normal mob (Lv13/14/15) | `MMOMobArchetype.cpp` | +3 mob | A2 ile **aynı commit** (öksüz mob uyarısı) |
| A1b | Çözülme Yolu spawner'ı (yalnız ROL; koordinat probla) | `MMOGameMode.cpp` | +1 spawner | koordinat probu (bloklayıcı) |
| A2 | 4 çekirdek yan görev (Lv11/13/14/15) | `MMOQuestTypes.cpp` | +4 görev | A1 + A1b |
| A3 | 2 opsiyonel yan görev (Lv17/19) | `MMOQuestTypes.cpp` | +2 görev | A2 |
| B | Lv31 ve Lv39 için sınıf başına 1 beceri | `MMOSkillDatabase.cpp` | +8 beceri | bağımsız |
| C | Yeni karakter ilk 10 dk için 8 ipucu | `MMOHUD.cpp`/`MMOCharacter.cpp` | 8 metin | bağımsız, en son |
| D | Sıra + doğrulama | — | — | — |

Kapsam dışı (sonraki tur): kapı seviyesi boş kalan Lv21, 23, 26; Lv23–24 normal mob boşluğu; Lv34–35/37–38/41/43/45 görev boşlukları; Lv45→58 beceri boşluğu (`ContentMap.py` çıktısına bak).

---

## A. Lv11–15 içerik boşluğu

### A.0 Bulgu `[KOD]` `[MODEL]`

- `ContentMap.py`: Lv11, Lv14–15 satırı "yeni içerik yok"; Lv13'te yalnız elit (`FrostLich`). Normal mob Lv12 (`OrcBerserker`) → Lv16 (`EmberHound`) arası **yok**.
- Görev kapısı olmayan seviyeler: 11, 14, 15, 17, 19, 21, 23, 26. Model (`ProgressionModel.py --max 30`): Lv11–15 "görevli" öldürme 1,2/seviye (hedef 9–13 `[PLACEHOLDER]`), yani bu seviyelerde oyuncunun **yapacak işi** yok, XP görev ödüllerinden geliyor.
- Spawner havuzu: `GetRandomInLevelRange` banttaki arketiplerden **eşit olasılıkla** seçer. Hedef mobu 1/N olasılıkla çıkar: `Q_FrostWraiths` (5 hedef) bugün 4'lü havuzda ≈20 öldürme ister. Bu, yeni mobları mevcut bir bölgeye eklemenin maliyetidir (A.2).
- Bölge hikâyesi (`Lore.md`): Buz Lordu düşünce Ayazdiş'in rüzgârı dinip doğuda ateş dağı uyanıyor. Lv13–15 mobları tam bu geçişin yeri: **Çözülme Yolu** (buz çözülüyor, ilk kor görünüyor).

### A.1 Yeni normal mob arketipleri

Ölçekleme: Lv12 `OrcBerserker` (31/30/44, ham XP 660) ile Lv16 `EmberHound` (41/38/58, 1300) arasında dogrusal ara değer: Str +2,5, Vit +2, hasar +3,5, XP/altın seviye başına ≈×1,18 (`MMOMobArchetype.cpp` Lv8–12 ve Lv16–22 yorumundaki kural). XP çarpanı `MMOXPTuning` eğrisi B'de Lv12–16 arası **0,20** düz `[KOD]`. HP = 50 + 8×Vit + 12×Lv, savunma = 0,5×Vit `[KOD: MMOStatsComponent.cpp:217-222]`.

| ID | Ad (ASCII) | Lv | Str | Vit | Hasar | Ham XP | **Etkin XP** | Altın | Hız | Aggro | Ölçek | HP | Savunma | Etki | Görsel |
|---|---|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---:|---|---|
| `ThawStalker` | Cozulme Avcisi | 13 | 33 | 29 | 47 | 780 | **156** | 130–310 | 430 | 600 | 1,15 | 438 | 14,5 | vuruşta Zehir %35 | Manny + renk |
| `CrustWarden` | Kabuk Bekcisi | 14 | 35 | 42 | 50 | 925 | **185** | 150–360 | 245 | 600 | 1,40 | 554 | 21 | Öfke <%45 can | Manny + renk |
| `ScorchedRaider` | Kavrulmus Akinci | 15 | 39 | 37 | 55 | 1100 | **220** | 175–415 | 345 | 600 | 1,30 | 526 | 18,5 | vuruşta Yavaşlatma %30, Öfke <%40 | Warrok ork |

Komşu karşılaştırma `[KOD+MODEL]`: Lv12 `OrcBerserker` HP 434, hasar 44, etkin XP 132; Lv16 `EmberHound` HP 546, hasar 58, etkin XP 260. Yeni üçlü hasar 44→47→50→55→58, etkin XP 132→156→185→220→260 ile **tek yönlü artıyor**. Tek başına her mob eşiğini 9,3 / 9,8 / 10,3 öldürmede karşılar (Lv13/14/15 eşiği ÷ etkin XP). Roller bölgeler 5–7'nin kalıbını tekrarlar: hızlı akıncı / yavaş tank / Warrok ork vurucu.

Notlar:
- `AggroRange` **600** zorunlu (2026-09-18 QA notu: bölge mobları 600'ü aşmaz).
- Yavaşlatma yalnız `ScorchedRaider`'da; `Slow` zaten yaygın (OrcWarrior, DireWolf, FrostWraith, OrcBerserker...), yeni mob başına bir tane yeter.
- Loot (`MMOMob.cpp` ~satır 1017): normal düşüş havuzu `RequiredLevel <= MobLevel+3`; Lv13 mob Lv16 Adamantin ekipmanını **düşürebilir** (şimdiye dek en erken Lv13 elitten/Lv16 mobtan). Bilinçli kabul edildiyse dokunma `[VARSAYIM]`.

**C++ (yapıştırmaya hazır).** Çapa: `// --- Dorduncu bolge NORMAL moblari` yorumunun hemen **üstü**. Kopyada derleme dışı doğrulandı (`ValidateData.py` temiz):

```cpp
		// --- Gecis kademesi NORMAL moblari (Lv13-15; Cozulme Yolu, Ayazdis ile Alev Vadisi arasi) ---
		// Olcekleme Lv12 (OrcBerserker) -> Lv16 (EmberHound) arasi dogrusal ara deger: +2.5 Str, +2 Vit, +3.5 hasar,
		// XP/altinda seviye basina ~x1.18 (660 -> 780 / 925 / 1100 -> 1300). Hassas denge degil, tutarli kademe.
		// AggroRange 600 (bolge mob'lari icin guvenlik siniri, bkz. yukaridaki 2026-09-18 notu).
		// Cozulme Avcisi: hizli akinci; eriyen buzun zehirli suyu vurusta zehirler.
		Table.Add(MakeMob(TEXT("ThawStalker"), TEXT("Cozulme Avcisi"), 13, 33, 29, 47.f, 780, 130, 310, 430.f, 600.f, 1.15f, FLinearColor(0.38f, 0.62f, 0.62f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.35f;

		// Kabuk Bekcisi: yari buz yari curuf kabuklu yavas tank; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("CrustWarden"), TEXT("Kabuk Bekcisi"), 14, 35, 42, 50.f, 925, 150, 360, 245.f, 600.f, 1.4f, FLinearColor(0.55f, 0.38f, 0.28f)));
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.45f;

		// Kavrulmus Akinci: bandin en guclu normali; Warrok ork gorseli, yavaslatir ve ofkelenir.
		Table.Add(MakeMob(TEXT("ScorchedRaider"), TEXT("Kavrulmus Akinci"), 15, 39, 37, 55.f, 1100, 175, 415, 345.f, 600.f, 1.3f, FLinearColor(0.7f, 0.3f, 0.12f)));
		SetWarrokVisual(Table.Last());
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.3f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;
```

### A.2 Spawner (yalnız rol; koordinat sonra runtime probla ölçülecek)

**Rol:** "Çözülme Yolu" geçiş cebi. Oyuncunun Ayazdiş'ten (Üçüncü bölge, Lv8–12) Alev Vadisi'ne (Dördüncü bölge, Lv16–22) yürürken uğradığı, ikisinin arasında kalan tampon alan. Yalnız bu üç mobu üretir (1/3'er olasılık).

| Alan | Değer | Gerekçe |
|---|---|---|
| `MinMobLevel` / `MaxMobLevel` | 13 / 15 | `ValidateData.py` bandı bu iki literalden okur |
| `MaxMobs` / `SpawnRadius` | 5 / 225 | Obsidyen/Dusk kalıbı (kıt zeminde ek sızıntı yok) |
| `bValidateSpawnOnNavGround` | `true` | Obsidyen/Rift kalıbı |
| `ForcedArchetypeId` | yok (bant) | normal moblar elit gibi sabitlenmez |
| Harita | yalnız `Summer_scene` | Obsidyen bloğu içinde (`GetMapName().Contains(TEXT("Summer_scene"))`) |
| Çapa | Obsidyen spawner'ından sonra, `const FVector ObsidianBossLoc(` satırından önce | mutlak koordinat bloğu |

**Probe ölçütleri (koordinat uydurmayın; mevcut Obsidyen/Rift probunun aynısı):** halka (225/250 u) 16/16 ya da 64/64 nokta zeminde (±60 u) ve bağlı navmesh'te, eğim sınırı içinde, PlayerStart'a **tam yol**; en yakın eski cep ve doğuş noktası/NPC'lerden **≥ ~1300 u** (mob doğma yarıçapı 225 + gezinme 450 + aggro 600 = ~1275 u'dan oyuncu fark edilir; `MMOGameMode.cpp` Rift yorumu); Üçüncü ve Dördüncü bölgeye komşu, ikisine de aggro menzilinde değil. `ExtendSummerNavigation.py` ile navmesh ±8500'e genişletildi, zemin var `[KOD]`.

**C++ iskeleti** (koordinat `TODO`; ölçmeden commit etme, aksi hâlde mob (0,0,0)'da doğar):

```cpp
		// Cozulme Yolu (Lv13-15): ThawStalker/CrustWarden/ScorchedRaider bandi. KOORDINAT OLCULMEDEN COMMIT ETME
		// (Obsidyen/Rift kalibi: headless Summer_scene grid probu, halka 16/16 + hub'a tam yol, bkz. docs/CONTENT_PLAN_LV11_45.md A.2).
		const FVector ThawLoc(0.f, 0.f, 0.f); // TODO: probla olculen X, Y, Z (zemin + 50u)
		const FTransform ThawTM(FRotator::ZeroRotator, ThawLoc);
		if (AMMOMobSpawner* ThawSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), ThawTM))
		{
			ThawSpawner->MaxMobs = 5;
			ThawSpawner->SpawnRadius = 225.f;
			ThawSpawner->MinMobLevel = 13;
			ThawSpawner->MaxMobLevel = 15;
			ThawSpawner->bValidateSpawnOnNavGround = true;
			UGameplayStatics::FinishSpawningActor(ThawSpawner, ThawTM);
		}
```

**Yedek (koordinat gecikirse; GEÇİCİ):** `ThirdZone->MaxMobLevel = 12;` → `15;` (koordinat gerektirmez). Bedeli:
1. Üçüncü bölge havuzu 4 → 7 arketip: yeni üçlü spawn'ların %43'ünü alır; Lv8–12 oyuncu Lv15 mobla karşılaşır; mevcut `Q_FrostWraiths/CursedKnights/VenomSpiders/OrcBerserkers` hedef başına öldürme sayısı ≈×1,75 (5 hedef ≈20 → ≈35 öldürme).
2. **`Scripts/TestValidateData.py` KALIR:** "Bos spawner bandi" senaryosu `ThirdZone->MinMobLevel = 13` yapınca artık boş bant üretmez (13–15'te mob var). Kopyada doğrulandı: `KALDI: [Bos spawner bandi] beklenen satir yok: 'Lv13-12 bandinda elit olmayan mob yok'`, çıkış 1. Yedek kullanılırsa o senaryonun deseni (`TestValidateData.py` `CASES`) güncellenmeli (ör. `MinMobLevel = 16`) ve sonra geri alınmalı.
3. Dördüncü bölgeyi 13'e açmak (`FourthZone->MinMobLevel = 16;` → `13;`) da seçenek: Lv16–22 oyuncu, havuzun %43'ünde düşük XP'li (156–220) moblarla karşılaşır; önerilmez.

### A.3 Yeni görevler (`MakeQuest` kalıbı)

Hepsi **yan görev**: ana zinciri kilitlemez (`Q_EmberHounds` hâlâ `Q_FrostLich`'e bağlı). Üç Çözülme görevi aynı önkoşula (`Q_FrostLich`) bağlı, kapıları 13/14/15: oyuncu üçünü paralel tutup tek avda üç mobu da keser (havuz seyrelmesini azaltır). Etkin XP = ham × `QuestMultiplier(kapı)` (kapıda ×0,085–0,095, Lv17: ×0,12, Lv19: ×0,16 `[KOD]`).

| ID | Ad | Kapı | Önkoşul | Tür / hedef | Adet | Ham XP | **Etkin XP** | Eşiğin % | Altın | Tüketilebilir ödül | Ekipman |
|---|---|---:|---|---|---:|---:|---:|---:|---:|---|---|
| `Q_AyazdisPatrol` | Ayazdis Devriyesi | 11 | `Q_VenomSpiders` (kapı 10) | `KillAnyMob` | 14 | 3000 | 255 | %27 | 800 | 3× Büyük Can İksiri | — |
| `Q_ThawStalkers` | Cozulme Avcilari | 13 | `Q_FrostLich` (13) | `KillSpecificMob` `ThawStalker` | 8 | 4700 | 400 | %27 | 1000 | 3× Büyük Mana İksiri | — |
| `Q_CrustWardens` | Kabuk Bekcileri | 14 | `Q_FrostLich` | `KillSpecificMob` `CrustWarden` | 8 | 4950 | 446 | %25 | 1100 | 4× Büyük Can İksiri | — |
| `Q_ScorchedRaiders` | Kavrulmus Akincilar | 15 | `Q_FrostLich` | `KillSpecificMob` `ScorchedRaider` | 10 | 5200 | 494 | %22 | 1200 | 4× Büyük Mana İksiri | — |
| `Q_EmberPack` *(P2)* | Kor Surusu | 17 | `Q_EmberHounds` (16) | `KillSpecificMob` `EmberHound` | 6 | 1800 | 216 | %6 | 1400 | 5× Büyük Can İksiri | — |
| `Q_GolemQuarry` *(P2)* | Golem Ocagi | 19 | `Q_MagmaGolems` (18) | `KillSpecificMob` `MagmaGolem` | 6 | 2100 | 336 | %6 | 1600 | 5× Büyük Mana İksiri | — |

Tasarım gerekçeleri:
- **Ham XP merdiveni:** VenomSpiders 2700 (kapı 10) → OrcBerserkers 3400 (12) → FrostLich 4500 (13) → EmberHounds 5400 (16). Yeni 11/13/14/15 değerleri bu merdivenin içinde (3000 / 4700 / 4950 / 5200). P2 görevleri bilerek düşük (1800/2100): Lv16–20 hızını daha fazla kaydırmasın (A.4).
- **Altın:** bilinçli olarak ana merdivenin ~%50'si (FrostLich 3000, EmberHounds 3500). Sebep: `EconomyModel.py` Lv10–15 kademesini bugün 11.021 gelir / 3.648 gider = 3,0× hesaplıyor ve altın sink'i hâlâ açık karar (`DECISIONS_NEEDED.md` 3). Yeni görevler Lv11–15 geliri +4.100 (≈ **15.100 / 3,6k = ~4,1×**, elle hesap `[MODEL]`), P2 +3.000 (Lv16–24 kademesi 93.035'in %3'ü).
- **Ekipman ödülü bilerek yok** ("kapı+2" kuralı etkisiz): Lv11–15 aralığında yeni ekipman kademesi yok. Mithril seti Lv13'te `Q_FrostLich` ile tamamlanıyor, Adamantin (Lv16) Lv18–24 zincirinin ödülü; ikinci kopya vermek yerine tüketilebilir. İstenirse tek ödül: `Q_ScorchedRaiders` → `AdamantineBoots` (Lv16 ≤ kapı 15+2, `ValidateData` geçer) ama `Q_MagmaGolems` ödülüyle **kopya** olur.
- `KillAnyMob` (kapı 11): hedef mobu havuz seyrelmesi yok; `Q_Cleanup` emsali. Zayıflık: gri (düşük seviyeli) mobların öldürmesi de sayılır `[KOD: MMOQuestComponent.cpp:248]`; kabul edilmiş desen.
- **Havuz seyrelmesi `[MODEL]`:** Çözülme spawner'ında 3 arketip → bir hedef için ≈3× öldürme, ama üç görev açıksa kesilen her mob bir göreve sayılır (8/8/10 hedefi için ≈ 30 öldürme, 26 değil). Dördüncü bölgede (P2) havuz 4 → 6 hedef ≈ 24 öldürme.

**C++ (yapıştırmaya hazır).** Çapa: `// --- Dorduncu bolge zinciri (Lv16-24, Alev Vadisi)` yorumunun **üstü** (çekirdek 4 + P2 2; P2 ayrı commit'e alınabilir). Kopyada `ValidateData.py` temiz:

```cpp
		// --- Lv11-15 bosluk dolgusu (2026-10, docs/CONTENT_PLAN_LV11_45.md): ana zinciri KILITLEMEYEN yan gorevler ---
		// Lv11/14/15'te hic gorev kapisi yoktu. Ham XP/altin PLACEHOLDER: kapi seviyesi carpani (~x0.09) sonrasi ~esigin %22-27'si.
		// Ekipman odulu bilerek yok (Mithril seti Lv13'te tamam, Adamantin Lv16 zincirinde); tuketilebilir + az altin.
		Table.Add(MakeQuest(TEXT("Q_AyazdisPatrol"), TEXT("Ayazdis Devriyesi"), TEXT("Buzulun kenarinda devriye at, muhafizlar nefes alsin: 14 canavar oldur."),
			T::KillAnyMob, TEXT(""), 14, 3000, 800, 11, TEXT("Q_VenomSpiders")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterHealthPotion"));
		Table.Last().RewardConsumableCount = 3;

		Table.Add(MakeQuest(TEXT("Q_ThawStalkers"), TEXT("Cozulme Avcilari"), TEXT("Buz Lordu'nun dususuyle cozulen yamaclarda avlanan sinsi yaratiklari durdur: 8 Cozulme Avcisi oldur."),
			T::KillSpecificMob, TEXT("ThawStalker"), 8, 4700, 1000, 13, TEXT("Q_FrostLich")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterManaPotion"));
		Table.Last().RewardConsumableCount = 3;

		Table.Add(MakeQuest(TEXT("Q_CrustWardens"), TEXT("Kabuk Bekcileri"), TEXT("Yari buz yari curuf kabuklu bekcileri kir: 8 Kabuk Bekcisi oldur."),
			T::KillSpecificMob, TEXT("CrustWarden"), 8, 4950, 1100, 14, TEXT("Q_FrostLich")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterHealthPotion"));
		Table.Last().RewardConsumableCount = 4;

		Table.Add(MakeQuest(TEXT("Q_ScorchedRaiders"), TEXT("Kavrulmus Akincilar"), TEXT("Alev Vadisi'ne yuruyen kavrulmus ork akincilarini dagit: 10 Kavrulmus Akinci oldur."),
			T::KillSpecificMob, TEXT("ScorchedRaider"), 10, 5200, 1200, 15, TEXT("Q_FrostLich")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterManaPotion"));
		Table.Last().RewardConsumableCount = 4;

		// Opsiyonel (P2): Lv17/19 kapi bosluklari. Dorduncu bolge havuzu 4 arketip (1/4 sans) -> 6 hedef ~24 oldurme; ham XP bilerek dusuk.
		Table.Add(MakeQuest(TEXT("Q_EmberPack"), TEXT("Kor Surusu"), TEXT("Gecidin ardindaki surunun artigini temizle: 6 Kor Tazisi daha oldur."),
			T::KillSpecificMob, TEXT("EmberHound"), 6, 1800, 1400, 17, TEXT("Q_EmberHounds")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterHealthPotion"));
		Table.Last().RewardConsumableCount = 5;

		Table.Add(MakeQuest(TEXT("Q_GolemQuarry"), TEXT("Golem Ocagi"), TEXT("Golemlerin lav ocagini sondur: 6 Magma Golemi daha oldur."),
			T::KillSpecificMob, TEXT("MagmaGolem"), 6, 2100, 1600, 19, TEXT("Q_MagmaGolems")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterManaPotion"));
		Table.Last().RewardConsumableCount = 5;
```

**`ValidateData.py` kural uyumu:**

| Kural | Durum |
|---|---|
| Kimlik tekilliği, ayrıştırma sayısı (`MakeQuest` çağrısı = ayrıştırılan) | Açıklamada çift tırnak yok, sayılar tamsayı → eşleşir |
| `KillSpecificMob` hedefi mob tablosunda | `ThawStalker/CrustWarden/ScorchedRaider` A1 ile eklenir (aksi hâlde `HATA`) |
| Hedef bir spawner'da üretiliyor (HATA) | A1b şart; spawner yoksa kopyada: `HATA: Q_ThawStalkers: hedef ThawStalker (Lv13) hicbir spawner'da uretilmiyor (gorev tamamlanamaz)` ×3 |
| Önkoşul görev var | `Q_VenomSpiders`, `Q_FrostLich`, `Q_EmberHounds`, `Q_MagmaGolems` mevcut |
| Zincir geri gitmiyor (UYARI) | 11≥10, 13/14/15≥13, 17≥16, 19≥18 |
| Kapı − hedef mob seviyesi ≤ 5 (UYARI) | 0–1 fark |
| Ödül ekipmanı kapı+2 / MaxLevel / 15 seviye eski | ekipman ödülü yok, geçerli |
| `RewardConsumableId` | `ValidateData` denetlemiyor; `GreaterHealthPotion`/`GreaterManaPotion` `MMOConsumableDatabase.cpp:30,34`'te var `[KOD]` |

### A.4 Model sonucu `[MODEL]` (bu veriyle `ProgressionModel.py --max 30`)

Satır: gorevsiz / gorevli öldürme-seviye (parantez: bandın kaç seviyesinde grind=0).

| Bant | Hedef `[PLACEHOLDER]` | Bugün | + 3 mob | + 3 mob + 4 görev | + 3 mob + 6 görev |
|---|---|---|---|---|---|
| 11–15 | 9–13 | 10,3 / **1,2** (5/5) | 9,7 / 1,2 (5/5) | 9,7 / **9,2** (5/5) | 9,7 / 9,2 (5/5) |
| 16–20 | 11–15 | 14,5 / **9,6** (1/5) | 16,8 / 11,4 (1/5) | 16,8 / **5,9** (3/5) | 16,8 / 5,9 (4/5) |
| 21–25 | 13–18 | 17,4 / 15,8 | 20,3 / 18,5 | 20,3 / 18,5 | 20,3 / 18,5 |
| Lv1→30 toplam (gorevsiz / gorevli) | | ~340 / ~289 | ~362 / ~311 | ~362 / **~323** | ~362 / ~324 |

Okuma (dürüst):
1. **Kazanç:** Lv11–15 "görevli" öldürme 1,2 → 9,2: hedef bandın alt sınırı. Bu seviyelerde artık her seviyede bir görev kapısı ve avlanacak mob var (`ContentMap.py`: Lv11, 14, 15 satırları dolar, 3+ seviyelik boş aralık kalmaz). Lv11–20 toplamı 54 → 75 zorunlu öldürme.
2. **Bedel:** Lv16–20 "görevli" 9,6 → 5,9 (hedef 11–15'in altı, grind=0 seviyesi 1 → 3). Sebep: yeni görevlerin zorunlu öldürmeleri ve ödülleri öne XP taşıyor; model fazla XP'yi sonraki bantlara aktarır (`BALANCE_MODEL.md` "Lv11–20 grind=0" bulgusuyla aynı mekanizma). **Hızı artırmaz** (Lv11–20'de toplam öldürme artar) ama modelin "grind" etiketini bozar.
3. **Model artefaktı:** "gorevsiz" Lv16–25 yükselişi (14,5 → 16,8; 17,4 → 20,3) ve Lv21–25 `> hedef (cok yavas)` bayrağı yeni mobların düşük XP'si pencere ortalamasına (`[L-8, L+3]`) girdiği içindir. Gerçekte Lv21–23 oyuncu Çözülme spawner'ına gitmez; etki modelde olduğundan küçük `[VARSAYIM]`.
4. **Düzeltme seçenekleri (karar bekliyor, uygulanmadı):** (a) kabul et ve `[MMOMETRIC]` ile ölç; (b) adetleri 10/6/6/8'e düşür (görevli 11–15: 7,2; 16–20: 6,7); (c) `GQuestKnots` `{16, 0.10f}` → `{16, 0.05f}`, `{20, 0.18f}` → `{20, 0.10f}` (16–20: 5,9 → 7,1; toplam 331); (d) (c) + mob knot `16:0.15, 20:0.17` (16–20: 8,7; ama gorevsiz 20,2, Lv21–25 gorevli 21,1). Hiçbiri 16–20'yi 11'e getirmiyor; önce ölçüm.
5. `BalanceReport.py` (en iyi mob): yalnız Lv11 (`CrustWarden L14` 185 XP → 6 öldürme; bugün `DireWolf`) ve Lv12 (`ScorchedRaider L15` 220 XP → 6; bugün `Warlock` 8) değişir; Lv13–15 aynı (pencerede `EmberHound`/`MagmaGolem` daha iyi). Rapor bu iş için doğru ölçü değil; **doğrulama aracı `ProgressionModel.py`.**

### A.5 Metin taslağı (opsiyonel; `metin-yazari`, `Lore.md` → `GenQuestDialogue.py`)

Replik yoksa `FMMOQuestDialogue` boş döner (oyun bozulmaz). `Lore.md` biçimi: `- **Başlık** *(Q_ID)* — "alış"` + `  - *Dönüşte:* "teslim"`. Bölüm başlığı önerisi: `## Çözülme Yolu (Lv13-15)`: "Buz Lordu'nun düşüşüyle ilk kez çözülen yamaçlar: eriyen buzun altından zehirli sular, çurufa dönmüş bekçiler ve Alev Vadisi'ne yürüyen kavrulmuş orklar çıkar." Mob satırları: **Çözülme Avcısı** *(ThawStalker, Lv13)*, **Kabuk Bekçisi** *(CrustWarden, Lv14)*, **Kavrulmuş Akıncı** *(ScorchedRaider, Lv15)*.

- **Ayazdiş Devriyesi** *(Q_AyazdisPatrol)* — "Buz Lordu henüz ayakta evlat, ama sürüleri buzulun kenarına taşıyor. Devriye at; önüne çıkan on dört canavarı temizle ki muhafızlar soluk alsın." — *Dönüşte:* "İyi devriye. Kenar sessiz; ama merkezdeki soğuk hâlâ nefes alıyor."
- **Çözülme Avcıları** *(Q_ThawStalkers)* — "Buz Lordu düştü, yamaçlar çözülmeye başladı; ama çözülen her şey iyiye işaret değil. Eriyen suyun zehrini taşıyan avcılar yol kesiyor. Sekizini durdur." — *Dönüşte:* "Yamaç sakin. Su yine de akıyor; bu yolun sonu sıcak, evlat."
- **Kabuk Bekçileri** *(Q_CrustWardens)* — "Eriyen buzun altından yarı buz yarı çuruf kabuklu bekçiler kalktı. Yavaşlar ama kolay devrilmezler; canları azaldıkça öfkelenirler. Sekizini kır." — *Dönüşte:* "Kabuklar çatladı, içlerinde kor vardı. Alev Vadisi çok yakın."
- **Kavrulmuş Akıncılar** *(Q_ScorchedRaiders)* — "Doğuya yürüyen ork akıncıları kavrulmuş, gözleri kor gibi. Vadiye varmadan onunu dağıt; yavaşlatır, yaralandıkça çıldırırlar." — *Dönüşte:* "Yol açıldı. Vadiye girmeden soluklan, evlat; ateş buz kadar bağışlamaz."
- **Kor Sürüsü** *(Q_EmberPack)* — "Geçidin ardından sürünün artığı geri dönmüş. Altı Kor Tazısı daha avla; bu sefer izini kaybetme." — *Dönüşte:* "Sürü dağıldı. Vadi biraz daha nefes alıyor."
- **Golem Ocağı** *(Q_GolemQuarry)* — "Magma golemleri lav ocağında çoğalıyor. Altısını daha kır, ocak sönsün." — *Dönüşte:* "Ocak söndü, vadi biraz serinledi."

---

## B. Dört sınıf için Lv31 ve Lv39 becerileri

### B.0 Bulgu `[KOD]`

`ValidateData.py` dört sınıfta da `28 -> 33 (5)` ve `33 -> 45 (12)` uyarısı veriyor (`DECISIONS_NEEDED.md` karar 5, öneri (c)). Lv33'ün üç sınıfta (Savaşçı/Okçu: buff, Büyücü: AoE, Rahip: şifa) farklı rolü var; yeni becerilerin **boş rolü** doldurması gerekir:

| Sınıf | Tek hedef hattı | Alan hattı | Şifa/buff | Boşluk |
|---|---|---|---|---|
| Savaşçı | Rampage Lv28 → WarCyclone Lv45 | TempestSlash Lv24 → WarDeluge Lv58 (**34 seviye**) | BloodLord Lv33 (buff) | Lv31 alan, Lv39 tek hedef |
| Okçu | DeathMark Lv28 → (Lv80) | Volley Lv13 → SpikeTrap Lv45 (**32 seviye**) | WindsBreath Lv33 (buff) | Lv31 alan, Lv39 tek hedef |
| Büyücü | Disintegrate Lv28 → (sonrası yok) | ArcaneBlast Lv20 → ArcaneBurst Lv45 (**25 seviye**) | FreezingTempest Lv33 (alan+Yavaşlat) | Lv31 alan, Lv39 tek hedef |
| Rahip | Retribution Lv28 → AngelicBlade Lv45 | WrathOfAngels Lv24 → AngelicStorm Lv58 | LifeBond Lv33 (şifa) | Lv31 alan, Lv39 **acil şifa** (tek hedef şifa hattı zaten dolu) |

### B.1 Kurallar (baskın beceri sorununu önlemek için)

1. **Ara değer, yan-grade:** Güç, mana, bekleme komşu (aynı kategori) iki becerinin arasında. Hiçbir yeni beceri komşusunu "tüm eksenlerde" geçmez, hiçbiri geçilmez (eksenler: güç/sn, güç/mana, tek vuruş gücü, yarıçap/menzil, etki).
2. **Her beceri ayrı bir niş taşır:** Savaşçı alan becerisi Yavaşlatma + daha geniş alan; Okçu Zehir; Büyücü tek vuruşta en yüksek patlama; Rahip en büyük tek vuruş şifa ama en uzun bekleme.
3. **Şifa:** `Pri_Mending` (Lv20) bugün `Pri_HolyRain`'in (Lv16) güç/sn (37,1 vs 23,8), güç/mana (4,33 vs 3,45), tek vuruş (520 vs 380) hepsini geçiyor; yeni şifa **tek vuruşta üstün, süre/mana'da eşit değil** (aşağıda).
4. **Rütbe:** yeni beceriler ağacın **zaten gereken** rütbesinde (Savaşçı/Okçu/Büyücü `Special0` rütbe 6, Rahip `Special0` rütbe 4 ve `Special1` rütbe 4); ek puan istemez. Seviye gelince `SyncLearnedFromMastery` otomatik açar `[KOD: MMOSkillComponent.cpp:320-345]` (PIE'de görülmedi). `rütbe ≤ seviye` (6 ≤ 31) ve ağaç rütbeleri ardışık kalır.
5. **Etkiler yalnız mevcut kimliklerden:** `Slow`, `Poison` (`MMOEffectDatabase.cpp`). `PrereqSkillId` öğrenmede zorlanmıyor, yalnız tutarlılık için ata beceriye bağlı; yeni becerinin gücü önkoşulundan düşük değil (`ValidateData` "devami daha zayif" uyarısı çıkmaz).
6. Mana: INT'siz havuz tabanı = 25 + 5×INT + 5,5×Seviye (sınıf tabanı INT: Savaşçı 6, Okçu 8, Rahip 18, Büyücü 22) `[KOD]`.

### B.2 Beceri tablosu (`MakeDef` kalıbı)

| ID | Ad (ASCII) | Sınıf / Ağaç | Rütbe | Lv | SP* | Önkoşul | Etki | Mana | Bekl. | Güç | Menzil | Yarıçap | Efekt |
|---|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---:|---|
| `War_Earthshaker` | Yer Sarsintisi | Savaşçı / Special0 | 6 | 31 | 2 | `War_TempestSlash` | AoESelf | 130 | 14 | 330 | 0 | 460 | Slow |
| `War_Skullsplitter` | Kafa Yarici | Savaşçı / Special0 | 6 | 39 | 2 | `War_Rampage` | MeleeDamage | 165 | 16 | 445 | 240 | 0 | — |
| `Arc_ThornVolley` | Dikenli Sagnak | Okçu / Special0 | 6 | 31 | 2 | `Arc_Volley` | AoETarget | 125 | 14 | 330 | 1700 | 420 | Poison |
| `Arc_Impaler` | Mizrakli Ok | Okçu / Special0 | 6 | 39 | 2 | `Arc_DeathMark` | RangedDamage | 160 | 16 | 470 | 2000 | 0 | — |
| `Mag_Inferno` | Alev Patlamasi | Büyücü / Special0 | 6 | 31 | 2 | `Mag_ArcaneBlast` | AoETarget | 130 | 14 | 360 | 1900 | 470 | — |
| `Mag_Starfall` | Yildiz Dusu | Büyücü / Special0 | 6 | 39 | 2 | `Mag_Disintegrate` | RangedDamage | 170 | 17 | 540 | 2000 | 0 | — |
| `Pri_RadiantNova` | Isiltili Nova | Rahip / Special0 | 4 | 31 | 2 | `Pri_WrathOfAngels` | AoETarget | 135 | 14 | 340 | 1500 | 440 | — |
| `Pri_SalvationLight` | Kurtulus Isigi | Rahip / Special1 | 4 | 39 | 3 | `Pri_LifeBond` | HealTarget | 260 | 30 | 1100 | 0 | 0 | — |

*SP: tablo alanı; kod kullanmıyor (`RequestInvestMastery` her rütbeye 1 puan), mevcut komşu satırlarla aynı seviye (2–3) bırakıldı. Tüm değerler `[PLACEHOLDER]`.

### B.3 Eğriye göre doğrulama `[MODEL]` (güç/sn = güç ÷ bekleme; güç/mana = güç ÷ mana)

| Yeni beceri | güç/sn | güç/mana | mana/sn | INT'siz havuzda mana % | Alt komşu (Lv) | Üst komşu (Lv) |
|---|---:|---:|---:|---:|---|---|
| `War_Earthshaker` | 23,6 | 2,54 | 9,3 | %58 (Savaşçı) | TempestSlash 22,5 / 2,84 / 270 / r440 (24) | WarDeluge 33,0 / 2,64 / 660 / r560 (58) |
| `War_Skullsplitter` | 27,8 | 2,70 | 10,3 | %61 | Rampage 26,2 / 3,09 / 340 (28) | WarCyclone 28,3 / 2,62 / 510 (45) |
| `Arc_ThornVolley` | 23,6 | 2,64 | 8,9 | %53 (Okçu) | Volley 12,7 / 2,26 / 140 (13) | SpikeTrap 28,3 / 2,62 / 510 (45) |
| `Arc_Impaler` | 29,4 | 2,94 | 10,0 | %57 | DeathMark 28,7 / 3,44 / 430 Zehir (28) | ArrowTempest 39,1 (70, alan) |
| `Mag_Inferno` | 25,7 | 2,77 | 9,3 | %43 (Büyücü) | ArcaneBlast 20,8 / 2,84 / 270 / r450 (20) | ArcaneBurst 32,5 / 2,67 / 520 / r500 (45) |
| `Mag_Starfall` | 31,8 | 3,18 | 10,0 | %49 | Disintegrate 35,0 / 3,50 / 420 (28) | MeteorShower 45,0 (70, alan) |
| `Pri_RadiantNova` | 24,3 | 2,52 | 9,6 | %47 (Rahip) | WrathOfAngels 22,3 / 2,76 / 290 / r420 (24) | AngelicStorm 33,0 / 2,64 / 660 / r560 (58) |
| `Pri_SalvationLight` | 36,7 | 4,23 | 8,7 | **%79** (Rahip) | Mending 37,1 / 4,33 / 520 (20) · LifeBond 37,5 / 3,57 / 750 Regen (33) | HandOfHeaven 68,8 / 3,93 / 1650 (80) |

Bulgular:
- **Baskınlık (kodla):** aynı sınıf+kategori, ≤10 seviye farkı: yeni beceriyi **içeren baskın çift yok** (tarama: 62 → 70 beceri, baskın çift sayısı 6 → 6; hepsi mevcut: Rampage≥Execute, DeathMark≥Snipe, Thunderclap≥IceChain, Judgement≥HolyFlame, Mending≥HolyRain, AbsoluteAnnihilation≥MeteorShower). Taslak betik Ek A'da. >10 seviye farkla yeni beceriyi geçen/geçilen: `Arc_ThornVolley` Lv13 `Arc_Volley`'yi, `Pri_SalvationLight` Lv16 `Pri_HolyRain`'i geçer; bu yeni halkaların eski seviyeli beceriyi geçmesi mevcut kalıpla aynı (Rampage≥Execute).
- **Mana (`Pri_SalvationLight` %79):** INT'siz havuzun yaklaşık dörtte üçü; mevcut üst sınır `Pri_LifeBond` %71, `Pri_HandOfHeaven` %76. INT'ye puan basan Rahip'te pay düşer; bu rahip için "tek vuruşa hazırlan" kimliği. Çok sıkışırsa mana 260 → 240 (güç/mana 4,58: Mending'i geçer, yine de güç/sn 36,7 < 37,1) `[PLACEHOLDER]`.
- **Savaşçı mana yükü:** `War_Earthshaker` %58 / `War_Skullsplitter` %61; mevcut Rampage %53, BloodLord %61, WarCyclone %64 aralığında.
- **Hotbar 10 yuva:** Savaşçı'nın Lv33'te 15 becerisi var; yeni becerilerle 17 olur. Seçim yapmak zorunda; kabul (M12 metriği `ölü beceri <%2` ölçecek).
- **Opsiyonel (uygulanmadı):** `Pri_Mending` bekleme 14 → 18, mana 120 → 135: güç/sn 28,9 ve güç/mana 3,85 ile iki komşusunun **arasına** girer (`DECISIONS_NEEDED.md` "Yeni karar adayları"). Yeni becerilerden bağımsız; karar bekliyor.
- **Uyarı beklentisi:** ValidateData'da `Lv28 → Lv33` uyarısı kaybolur; `Lv33 → Lv45` yerine **`Lv33 → Lv39 (6)` ve `Lv39 → Lv45 (6)`** gelir. Dört sınıfta toplam uyarı sayısı **20 → 20** (sayı aynı, metin değişir). Tamamen susturmak için o aralığa iki beceri gerekir (ör. Lv37 ve Lv41: 4/4/4); bu plan karar 5(c)'ye uyarak tek beceri koyar.

**C++ (yapıştırmaya hazır).** Çapalar (her sınıfta o sınıfın Lv33 satırından **sonra**, sonraki satır başlığından önce): Savaşçı `// Capstone 1/2: Titan Ofkesi.`, Okçu `// Civili Tuzak: alandaki hedeflere zehir birakan yeni okcu hatti.`, Büyücü `// Arcane Patlama: buyucunun yeni arkane hatti, elementsiz saf mana hasari.`, Rahip `// Kutsal Kalkan: Priestin yeni koruma hatti (Special2, ilk defa kullaniliyor).` yorumlarının **üstü**:

```cpp
		// --- Savaşçı (War) ---
		// Lv31/Lv39 bosluk dolgusu (docs/CONTENT_PLAN_LV11_45.md B): ayni agacta ZATEN gereken rutbeler (puan harcatmaz); PLACEHOLDER degerler.
		Table.Add(MakeDef(TEXT("War_Earthshaker"), TEXT("Yer Sarsintisi"), C::Warrior, T::Special0, 6, 31, 2, TEXT("War_TempestSlash"),
			E::AoESelf, 130.f, 14.f, 330.f, 0.f, 460.f, TEXT("Slow")));
		Table.Add(MakeDef(TEXT("War_Skullsplitter"), TEXT("Kafa Yarici"), C::Warrior, T::Special0, 6, 39, 2, TEXT("War_Rampage"),
			E::MeleeDamage, 165.f, 16.f, 445.f, 240.f, 0.f));

		// --- Okçu (Arc) ---
		Table.Add(MakeDef(TEXT("Arc_ThornVolley"), TEXT("Dikenli Sagnak"), C::Archer, T::Special0, 6, 31, 2, TEXT("Arc_Volley"),
			E::AoETarget, 125.f, 14.f, 330.f, 1700.f, 420.f, TEXT("Poison")));
		Table.Add(MakeDef(TEXT("Arc_Impaler"), TEXT("Mizrakli Ok"), C::Archer, T::Special0, 6, 39, 2, TEXT("Arc_DeathMark"),
			E::RangedDamage, 160.f, 16.f, 470.f, 2000.f, 0.f));

		// --- Büyücü (Mag) ---
		Table.Add(MakeDef(TEXT("Mag_Inferno"), TEXT("Alev Patlamasi"), C::Mage, T::Special0, 6, 31, 2, TEXT("Mag_ArcaneBlast"),
			E::AoETarget, 130.f, 14.f, 360.f, 1900.f, 470.f));
		Table.Add(MakeDef(TEXT("Mag_Starfall"), TEXT("Yildiz Dusu"), C::Mage, T::Special0, 6, 39, 2, TEXT("Mag_Disintegrate"),
			E::RangedDamage, 170.f, 17.f, 540.f, 2000.f, 0.f));

		// --- Rahip (Pri) ---
		Table.Add(MakeDef(TEXT("Pri_RadiantNova"), TEXT("Isiltili Nova"), C::Priest, T::Special0, 4, 31, 2, TEXT("Pri_WrathOfAngels"),
			E::AoETarget, 135.f, 14.f, 340.f, 1500.f, 440.f));
		// Kurtulus Isigi: tek-vurus "acil sifa" (en yuksek tek vurus, uzun bekleme); Mending/LifeBond ile guc/sn ve mana verimi ESIT DEGIL (baskin degil).
		Table.Add(MakeDef(TEXT("Pri_SalvationLight"), TEXT("Kurtulus Isigi"), C::Priest, T::Special1, 4, 39, 3, TEXT("Pri_LifeBond"),
			E::HealTarget, 260.f, 30.f, 1100.f, 0.f, 0.f));
```

(Sınıf başlıklarındaki `// --- ... ---` satırları anlatım içindir; tek bir ortak yorum yeter.)

---

## C. İlk 10 dakika rehberliği: 8 ASCII ipucu

Altyapı `[KOD]`: `MMONotify::ToOwner(Pawn, Metin, Renk, Süre, EMMONotifyKind::Hint)` yerelde ekran mesajı, uzak istemcide karakter üstünde kısa balon (`ClientShowHint`, Unreliable, aynı metin 0,5 sn içinde tekrarlanmaz). Mevcut sunucu hook'ları: `Stats->OnLevelUp` (`MMOCharacter.cpp:156`), `HandleDeath` (`:154`), çanta dolu mesajı `MMOInventoryComponent.cpp:226`, ganimet `AMMOLootPickup::OnOverlap`. Tuşlar: sol tık = yürü/saldır, `Z` en yakın düşman, `R` saldırı aç/kapa, `1`–`0` hotbar, `F` can iksiri, `G` mana iksiri, `I` çanta (satıra **sol tık = kuşan, sağ tık = yükselt**, `MMOHUD.cpp:1402-1455`), `K` beceri, `C` stat `[KOD]`. NPC adı oyunda **"Yasli Doran"** (`MMOQuestGiverNPC.cpp:10`); NPC üstünde `! <ad> [tikla]` etiketi var; "?" etiketi **yok** (teslim aynı NPC'ye tıklayarak).

| # | Tetikleyici | Metin (ASCII, ≤52 karakter) | Kanal / koşul |
|---|---|---|---|
| H1 | Yeni karakter (Lv1, 0 EXP, kayıt yok); sınıf paneli kapandıktan 8 sn sonra hâlâ hareket etmedi | `Zemine sol tikla: yuru. Sag tik surukle: kamera` | Hint; ilk hareketle iptal; oturumda 1 kez |
| H2 | Lv1–2, kabul edilmiş görev yok, Yasli Doran'a ≤900 u | `Yasli Doran'in ustundeki ! isaretine tikla` | Hint; oturumda 1 kez |
| H3 | İlk görev kabul edildi, 20 sn içinde öldürme yok | `Z: hedef sec  R: saldir  1-0: beceri  F: iksir` | Hint; oturumda 1 kez |
| H4 | Karakterin ilk ganimeti yere düştü (ilk öldürme) | `Esya dustu: uzerinden gec. I: canta, tikla = kusan` | Hint; oturumda 1 kez; Lv ≤ 5 |
| H5 | İlk seviye atlama ve harcanmamış stat/beceri puanı > 0; Lv ≤ 10 | `Seviye atladin! C: stat puani, K: beceri puani` | Hint; oturumda 1 kez; ayrıca puan ≥ 6 birikince Lv ≤ 15'te bir kez |
| H6 | Görev ilerlemesi hedefe ulaştı, teslim edilmedi | `Gorev tamam! Yasli Doran'a don, tikla, odulu al` | Hint; ilk görev için; Lv ≤ 10 |
| H7 | İlk ölüm (`HandleDeath`); Lv ≤ 20 | `Oldun: 5 sn sonra dogarsin, biraz EXP kaybettin` | Hint; oturumda 1 kez |
| H8 | İlk "Canta dolu!" (12/12) | `Canta dolu! Satici NPC'ye esya sat veya depoya koy` | Mevcut "Canta dolu!" hint'inin yerine ilk seferde; oturumda 1 kez |

Uygulama notları:
- **Kayıt biçimine dokunma:** `MMOSaveGame.h` korumalı yol (`CheckPrPaths.py` hata verir) ve VERTICAL_SLICE iş 3 önerisindeki `TutorialStepMask` gerekmez. Çözüm: karakter üzerinde **kaydedilmeyen, replike edilmeyen** `uint16 TutorialHintMask` (oturum başı sıfır) + seviye koşulu (`Level <= 10`/`15`/`20`); eski, yüksek seviyeli kayıtta hiç görünmez, düşük seviyeli yeniden girişte bir kez daha görünebilir (kabul edilen zayıflık).
- Pencere açıkken (envanter, görev, beceri, ölüm ekranı) **gösterme** (`IsAnyWindowOpen()` ve `S->bIsDead`; yalnız yerel/host pawn). Uzak istemcide `ClientShowHint` balonu pencereyi örtmez.
- H1–H2 sunucuda `PlayerTick` benzeri 1 sn'lik sayaçla (konum farkı/NPC mesafesi); H3–H8 olay tabanlı. Hiçbiri sunucu otoritesini değiştirmez, RPC eklemez (`ToOwner` mevcut).
- Çakışma riski: bu dosyalar şu an başka bir ajanın değişmemiş düzenlemesinde (yukarıdaki not). Bu parçayı en sona bırak.

PIE doğrulaması (hepsi `[VARSAYIM]`, oynanmadı): yeni karakter, konsol komutu yok; (1) H1 8 sn sonra çıkıyor, ilk adımda duruyor; (2) H2 Doran'a yaklaşınca; (3) görev al → 20 sn → H3; (4) ilk öldürme → H4; (5) Lv2 → H5; (6) hedef dolunca H6; (7) Z, R ile Lv1'de bilerek öl → H7; (8) çantayı 12'ye doldur → H8; (9) her biri oturumda bir kez, kayıt-yükle sonrası Lv11+ karakterde görünmüyor; (10) 2. pencere (listen server istemci) balonu görüyor; (11) pencere açıkken ipucu çıkmıyor.

---

## D. Uygulama sırası ve doğrulama

Önkoşul (her adım): editör **kapalı**; adımlar küçük commit'lerle; `ValidateData.py` çıkış 0. Çalışma ağacında başkasının değişmemiş dosyaları varsa yalnız kendi hunk'ını stage'le.

| Sıra | Parça | Doğrulama komutu | Beklenen çıktı (kopyada alındı) |
|---:|---|---|---|
| 0 | Taban çizgisi (bugün) | `$PY Scripts/ValidateData.py` · `$PY Scripts/BalanceReport.py` · `$PY Scripts/ProgressionModel.py --max 30` | `Mob: 48 (elit 13)  Item: 53 ... Gorev: 73  Beceri: 73  Efekt: 12  Spawner: 28`; 20 `UYARI: ... beceri boslugu`; `Veri denetimi temiz.` · `Normal mob: 35  Elit: 13`, `Uyari sayisi: 0` · bant 11-15 `10.3 / 1.2`, 16-20 `14.5 / 9.6 [1/5 seviyede grind=0]`, toplam `~340, ~289` |
| 1 | **A1 + A1b**: 3 mob + spawner (+ A2 ile birlikte; koordinat probu bloklayıcı) | `$PY Scripts/ValidateData.py` · `$PY Scripts/TestValidateData.py` · `$PY Scripts/ContentMap.py` | `Mob: 51 (elit 13) ... Spawner: 29` · `Veri denetimi temiz.` · uyarı satırı **hâlâ 20**, yeni `olu icerik` UYARISI **yok** (spawner varken) · `ValidateData negatif testi gecti (14 senaryo).` · ContentMap: Lv13 `ThawStalker`, Lv14 `CrustWarden`, Lv15 `ScorchedRaider`, "3+ seviyelik bos aralik yok" |
| 1' | A1 (yalnız mob), spawner **yok** (yanlış yol) | aynı komut | `UYARI: Mob ThawStalker (Lv13) hicbir spawner'da uretilmiyor (olu icerik)` ×3 (→ A1 yalnız bırakılmaz) |
| 2 | **A2**: 4 çekirdek görev | `$PY Scripts/ValidateData.py` · `$PY Scripts/ProgressionModel.py --max 30` · `$PY Scripts/EconomyModel.py` | `Gorev: 77` · `Veri denetimi temiz.` · bant 11-15 `9.7 / 9.2` (`gorev XP'si yeterli ...`), 16-20 `16.8 / 5.9 < hedef (cok hizli)  [3/5 seviyede grind=0]`, 21-25 `20.3 / 18.5 > hedef (cok yavas)`, toplam `~362, ~323` · EconomyModel Lv10–15 kademesi gelir ≈ 11.021 → ≈ 15.100 (yeni mob altını ayrıca küçük artar) |
| 2' | A2'yi A1/A1b olmadan koymak | aynı | `HATA: Q_ThawStalkers: hedef ThawStalker (Lv13) hicbir spawner'da uretilmiyor (gorev tamamlanamaz)` ×3; çıkış 1 |
| 3 | **A3** (opsiyonel): Lv17/19 görevleri | `$PY Scripts/ValidateData.py` · `$PY Scripts/ProgressionModel.py --max 30` · `$PY Scripts/ContentMap.py` | `Gorev: 79` · 16-20 `16.8 / 5.9 ... [4/5 seviyede grind=0]` · toplam `~362, ~324` · ContentMap Lv17 `Q_EmberPack`, Lv19 `Q_GolemQuarry` |
| 4 | **B**: 8 beceri | `$PY Scripts/ValidateData.py` · `$PY Scripts/TestValidateData.py` · Ek A betiği | `Beceri: 81` · beceri boşluğu uyarısı 20 (Lv28→33 yok; `Lv33 -> Lv39 (6)` ve `Lv39 -> Lv45 (6)` var) · `HATA` ve "devami daha zayif" yok · negatif test 14 senaryo geçer · baskınlık: `70 hasar/sifa becerisi tarandi, pencere 10 seviye: 6 baskin cift.` (yeni beceri içeren satır yok) |
| 5 | **Lore/diyalog** (A.5) | `$PY Scripts/GenQuestDialogue.py` sonra `git diff` | yalnız `MMOQuestDialogue.cpp` değişir; yeni `Q_` kimlikleri girdi olur; bloklayıcı değil |
| 6 | **C**: 8 ipucu | `$PY Scripts/StaticLint.py` · derleme (`ue-build` skill'i, editör kapalı) · `powershell -File Scripts\RunAllChecks.ps1 -SkipBuild` | `Statik kontrol temiz (derleme degildir).` · `Result: Succeeded` · tüm adımlar `GECTI` |
| 7 | **PIE + ölçüm** (`PLAYTEST_RUNBOOK.md` Kapı 3) | `python Scripts\PlaytestMetrics.py Saved\Logs\MyProject.log` | `levelup`/`kill` olaylarından Lv11–15 seviye başına öldürme/süre; karar eşiği aşağıda |

Ölçüm eşikleri `[PLACEHOLDER]` (metrik M1/M2, `VERTICAL_SLICE_LV1_30.md` §4): Lv11–15 öldürme/seviye 9–13 hedef, **<6 veya >18 "bozuk"** (içerik ya bedava ya kas); Çözülme spawner'ında hedef mobu görev başına öldürme sayısı beklentiden (≈ adet×3, A.3) >%50 sapıyorsa havuz/`MaxMobs` incelenir; Lv16–20 hız kayması (A.4 madde 2) ölçümde >%30 hızlı çıkarsa A.4 seçenek (c)/(d).

### PIE'de doğrulanacaklar (hepsi `[VARSAYIM]`; oynanmadı)

1. Çözülme spawner'ı yalnız 13/14/15 mobunu üretiyor; ilk karşılaşmada aggro 600 u ile hub/NPC'lere ulaşmıyor; doğuşta (Lv1) 3 dk güvenli.
2. Üç mobun saldırı/animasyonu (Manny + renk; `ScorchedRaider` Warrok ile `-89/-90` hizası) görsel olarak doğru.
3. `Q_AyazdisPatrol` Lv11'de teslim alınıyor; `Q_ThawStalkers/CrustWardens/ScorchedRaiders` `Q_FrostLich` bittikten sonra, kendi kapı seviyesinde listede; üçü paralel ilerliyor, ödül tüketilebilir envantere giriyor.
4. Lv31'de `War_Earthshaker`, Lv39'da `War_Skullsplitter` (ve diğer sınıflar) beceri kitabında **ek puan harcamadan** açılıyor, hotbar'a atanıyor; Yavaşlatma/Zehir uygulanıyor; mana yetmeyince reddi doğru.
5. `Pri_SalvationLight` seçili parti üyesini 1100 iyileştiriyor, 30 sn bekleme, istemcide bekleme geri alınıyor (reddedilirse).
6. Gerçek yürüyüş hızıyla Ayazdiş → Çözülme → Alev Vadisi geçişi 1–2 dk'yı aşmıyor (yürüme ölçülmedi).

## Riskler ve karar bekleyenler

| Konu | Not | Etiket |
|---|---|---|
| Çözülme spawner koordinatı | Probsuz commit yok; yedek `ThirdZone` genişletmesi öğrenme/Test kırar (A.2) | bloklayıcı |
| Lv16–20 hız kayması | Çekirdek görevler bandı 9,6 → 5,9 yapıyor (A.4) | model; ölçüm şart |
| Altın | Yeni görev altını +%37 (Lv10–15 kademesi); sink kararı açık | `DECISIONS_NEEDED.md` 3 |
| Lv13 Adamantin düşme | Lv13 mob Lv16 ekipmanı düşürebilir | bilgi |
| Tek başına devriye (`KillAnyMob`) | gri mob kill'i sayılır | `Q_Cleanup` emsali |
| Beceri uyarıları | 33→39 ve 39→45 6'şar seviye kalır | karar 5(c) |
| `Pri_Mending` | opsiyonel CD/mana ayarı (B.3) | karar bekliyor |

---

## Ek A. `Scripts/SkillDominance.py` (taslak; repoya eklenmedi, çıkış hep 0)

Kopyada çalıştırıldı (taban: `62 ... 6 baskin cift`, B sonrası: `70 ... 6 baskin cift`). `ValidateData.py` ile aynı `MakeDef` kalıbını okur:

```python
import argparse, re
from pathlib import Path
SRC = Path(__file__).resolve().parent.parent / "Source/MyProject/Private/Skills/MMOSkillDatabase.cpp"
PAT = re.compile(r'Table\.Add\(MakeDef\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*C::(\w+),\s*T::\w+,\s*\d+,\s*(\d+),\s*\d+,\s*'
                 r'TEXT\("[^"]*"\),\s*E::(\w+),([^;]*?)\)\);', re.S)
KIND = {"MeleeDamage": "tek", "RangedDamage": "tek", "AoESelf": "alan", "AoETarget": "alan", "HealSelf": "sifa", "HealTarget": "sifa"}

def load():
    rows = []
    for m in PAT.finditer(SRC.read_text(encoding="utf-8", errors="replace")):
        sid, cls, lvl, eff, rest = m.groups()
        if eff not in KIND: continue
        n = [float(x) for x in re.findall(r"([\d.]+)f", rest)] + [0.0] * 5
        e = re.search(r'TEXT\("([^"]*)"\)\s*$', rest.strip())
        mana, cd, power, rng, rad = n[:5]
        if cd <= 0 or mana <= 0 or power <= 0: continue
        rows.append(dict(id=sid, cls=cls, lvl=int(lvl), kind=KIND[eff], pps=power / cd, ppm=power / mana,
                         pc=power, rad=rad, rng=rng, eff=e.group(1) if e else ""))
    return rows

def dominates(a, b):
    if a["kind"] != b["kind"] or (b["eff"] and a["eff"] != b["eff"]): return False
    return all(a[k] >= b[k] for k in ("pps", "ppm", "pc", "rad", "rng")) and any(a[k] > b[k] for k in ("pps", "ppm", "pc"))

ap = argparse.ArgumentParser(); ap.add_argument("--window", type=int, default=10); ap.add_argument("--min-level", type=int, default=10)
args = ap.parse_args(); rows = load(); n = 0
for a in rows:
    for b in rows:
        if a is b or a["cls"] != b["cls"] or b["lvl"] < args.min_level or not 0 <= a["lvl"] - b["lvl"] <= args.window: continue
        if dominates(a, b):
            n += 1; print(f"BASKIN: {a['cls']}: {a['id']} (Lv{a['lvl']}) >= {b['id']} (Lv{b['lvl']}); fark {a['lvl'] - b['lvl']} sev.")
print(f"{len(rows)} hasar/sifa becerisi tarandi, pencere {args.window} seviye: {n} baskin cift.")
```

## Ek B. Model sayılarını yeniden üretme (kod eklenmeden önce)

Yeni veri koda girince `ProgressionModel.py` kendisi okur. Girmeden önce, aynı sayıları şu parçayla üret (`ProgressionModel` modül düzeyindeki `normals`/`quests` listelerine ekler; `scaled` ve çarpanlar kodla aynı):

```python
import sys; sys.path.insert(0, "Scripts")
import ProgressionModel as PM
base_n, base_q = list(PM.normals), list(PM.quests)
mobs = [dict(id="ThawStalker", lv=13, xp=780, elite=False), dict(id="CrustWarden", lv=14, xp=925, elite=False),
        dict(id="ScorchedRaider", lv=15, xp=1100, elite=False)]
core = [dict(id="Q_AyazdisPatrol", type="KillAnyMob", xp=3000, req=11, count=14),
        dict(id="Q_ThawStalkers", type="KillSpecificMob", xp=4700, req=13, count=8),
        dict(id="Q_CrustWardens", type="KillSpecificMob", xp=4950, req=14, count=8),
        dict(id="Q_ScorchedRaiders", type="KillSpecificMob", xp=5200, req=15, count=10)]
PM.normals[:] = base_n + mobs; PM.quests[:] = base_q + core
out = PM.simulate(30)
for a, b, lo, hi, nq, q, free, tot in PM.band_avgs(out): print(a, b, round(nq, 1), round(q, 1), f"{free}/{tot}")
```

Beklenen: `11 15 9.7 9.2 5/5`, `16 20 16.8 5.9 3/5`, `21 25 20.3 18.5 0/5`.
