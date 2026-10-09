# Beceri planı: Lv31–45 boşluğu (karar 5, seçenek c)

**Tarih:** 2026-10-09, `master` `e812fa9` üzerinde. **Durum:** yalnız belge; `Source/`, `Scripts/`, `Config/` değişmedi, derleme ve commit yok. Okunan kod: `Skills/MMOSkillDatabase.cpp`, `Skills/MMOSkillTypes.h`, `Effects/MMOEffectDatabase.cpp`, `Effects/MMOEffectTypes.h`, `Components/MMOSkillComponent.cpp/.h`, `Components/MMOStatsComponent.cpp/.h`, `Characters/MMOMob.cpp`, `Characters/MMOMobArchetype.cpp`, `Core/MMOGameMode.cpp`; ayrıca `Scripts/ValidateData.py`, `TestValidateData.py`, `ProgressionModel.py` (yalnız çalıştırıldı, yazılmadı).

**Etiketler:** **[Kesin]** kodda doğrudan okundu · **[Hesap]** kodda okunan değerlerden aritmetik (bu belgenin hesabı; PIE'de görülmedi) · **[Model]** `ProgressionModel.py` / `ValidateData.py` çıktısı, oynanış değil · **[Tahmin]** varsayım · **[PLACEHOLDER]** ölçümle değişecek sayı.

## 0. Önce bilinmesi gereken: seçenek (c) kodda zaten uygulanmış

`DECISIONS_NEEDED.md` madde 5 (2026-10-05) "Lv31 ve Lv39 civarı yeni beceri" önerisini **açık karar** olarak listeliyor. **[Kesin]** Bu öneri uygulanmış: dört sınıfın Lv31 ve Lv39 becerileri `MMOSkillDatabase.cpp`'de duruyor, commit `56de087` ("Dort sinifa Lv31 ve Lv39 beceri ara halkalari ekle"), çalışma ağacında bu dosya temiz. Değerler `CONTENT_PLAN_LV11_45.md` B.2 tablosundaki değerlerle **aynı**. `ValidateData.py` bugün `Beceri: 81`, "Veri denetimi temiz" ve 20 UYARI veriyor; uyarıların hepsi `33→39`, `39→45`, `45→58`, `58→70`, `70→80` aralıkları (4 sınıf × 5), `28→33` uyarısı yok.

Bu yüzden bu belge sekiz **yeni** satır önermiyor. Yaptığı:

1. Sekiz beceriyi (2. bölüm) alan alan kayda geçirir, komşularına göre türetmeyi gösterir.
2. Koddan çıkan, önceki belgelerde **bulunmayan** denge bulgularını yazar (4. bölüm): Zehir'in Lv31+ moblarda fiilen etkisiz olması, alan becerilerin hedef sayısıyla ölçeklenmesi, tek beceriye bağlı olmayan mana kısıtı.
3. Hâlâ açık olanı (33→39 ve 39→45, 6'şar seviye) ve kapatma seçeneklerini (6. bölüm) sunar.

> **Sahibe not:** `DECISIONS_NEEDED.md` satır 19 (özet tablosu) ve madde 5 (satır 88–102) güncel değil; bu belgeye dokunma yetkim yok, güncelleme gerekir (karar 5 → "uygulandı, 56de087; kalan: 6'şar seviyelik iki aralık ve 4. bölümdeki bulgular").

## 1. Mevcut Lv1–45 beceri tablosu

**Okuma:** `Mana / Bekleme(sn) / Güç`. Güç düz değerdir: `ExecuteSkill` hasarı `UGameplayStatics::ApplyDamage(Def.Power)` ile uygular, sahibin `AttackPower`'ı karışmaz **[Kesin: MMOSkillComponent.cpp:1274, 1319]**; hedefin `Defense`'i düşer, en az 1 geçer **[Kesin: MMOStatsComponent.cpp:412]**. ★ = Lv31/Lv39 becerisi (bu belgenin konusu). Rütbe = o ustalık ağacında gereken rütbe (`RequiredMasteryRank`). Menzil/yarıçap uzunluk birimi `u`. Lv45 sonrası beceriler tabloya girmedi (aşağıda tek satır).

Havuz formülleri **[Kesin: MMOStatsComponent.cpp:356-357, MMOStatsComponent.h:157-205]**: `MaxMana = 25 + 5×INT + 5,5×Seviye`; sınıf tabanı INT: Savaşçı 6, Okçu 8, Büyücü 22, Rahip 18 (`MMOSkillComponent.cpp:482-486`); mana yenilenmesi `7 + 0,25×Seviye` /sn, **savaşta ×0,25** (son savaş etkinliğinden sonra 5 sn).

### Savaşçı (19 beceri ≤ Lv45)

| Lv | Id | Ağaç / rütbe | Etki | Mana / Bkl / Güç | Menzil / Yarıçap | Efekt |
|---:|---|---|---|---|---|---|
| 1 | `War_PowerStrike` | Temel / 1 | Yakın hasar | 8 / 2 / 40 | 240 | — |
| 2 | `War_ShieldBash` | Temel / 1 | Yakın hasar | 6 / 3 / 25 | 200 | — |
| 3 | `War_Cleave` | Ustalık 1 / 1 | Alan (kendi etrafı) | 16 / 6 / 28 | r320 | — |
| 3 | `War_BattleShout` | Ustalık 2 / 1 | Buff | 15 / 18 / — | — | `Rage` |
| 4 | `War_BloodDrain` | Ustalık 2 / 1 | Kendini iyileştir | 10 / 10 / 50 | — | — |
| 4 | `War_IronSkin` | Ustalık 3 / 1 | Buff | 15 / 18 / — | — | `IronSkin` |
| 5 | `War_MortalBlow` | Ustalık 1 / 2 | Yakın hasar | 22 / 8 / 75 | 240 | — |
| 7 | `War_BloodStorm` | Ustalık 1 / 3 | Alan (kendi etrafı) | 30 / 14 / 55 | r340 | — |
| 10 | `War_ShatterBlow` | Ustalık 1 / 4 | Yakın hasar | 45 / 10 / 150 | 240 | `Slow` |
| 13 | `War_CrushingBlow` | Ustalık 1 / 4 | Yakın hasar | 58 / 9 / 190 | 240 | — |
| 16 | `War_LifeReap` | Ustalık 2 / 2 | Kendini iyileştir | 70 / 16 / 310 | — | — |
| 20 | `War_Execute` | Ustalık 1 / 5 | Yakın hasar | 85 / 12 / 250 | 240 | — |
| 24 | `War_TempestSlash` | Ustalık 1 / 5 | Alan (kendi etrafı) | 95 / 12 / 270 | r440 | — |
| 28 | `War_Rampage` | Ustalık 1 / 6 | Yakın hasar | 110 / 13 / 340 | 240 | — |
| **31 ★** | `War_Earthshaker` | Ustalık 1 / 6 | Alan (kendi etrafı) | 130 / 14 / 330 | r460 | `Slow` |
| 33 | `War_BloodLord` | Ustalık 2 / 3 | Buff | 145 / 24 / — | — | `RageII` |
| **39 ★** | `War_Skullsplitter` | Ustalık 1 / 6 | Yakın hasar | 165 / 16 / 445 | 240 | — |
| 45 | `War_WarCyclone` | Ustalık 1 / 6 | Yakın hasar | 195 / 18 / 510 | 240 | — |
| 45 | `War_SteelArmor` | Ustalık 3 / 2 | Buff | 120 / 22 / — | — | `IronSkinII` |

### Okçu (16 beceri ≤ Lv45)

| Lv | Id | Ağaç / rütbe | Etki | Mana / Bkl / Güç | Menzil / Yarıçap | Efekt |
|---:|---|---|---|---|---|---|
| 1 | `Arc_QuickShot` | Temel / 1 | Menzilli hasar | 6 / 1 / 25 | 1400 | — |
| 2 | `Arc_PowerShot` | Temel / 1 | Menzilli hasar | 8 / 1,5 / 40 | 1600 | — |
| 3 | `Arc_PoisonArrow` | Ustalık 1 / 1 | Menzilli hasar | 14 / 5 / 60 | 1600 | `Poison` |
| 3 | `Arc_Sprint` | Ustalık 2 / 1 | Buff | 12 / 16 / — | — | `Haste` |
| 5 | `Arc_ArrowRain` | Ustalık 1 / 2 | Alan (hedefte) | 22 / 10 / 40 | 1600 / r360 | — |
| 7 | `Arc_PiercingShot` | Ustalık 1 / 3 | Menzilli hasar | 26 / 9 / 95 | 1700 | `Slow` |
| 10 | `Arc_EagleEye` | Ustalık 1 / 4 | Menzilli hasar | 45 / 10 / 150 | 1800 | — |
| 13 | `Arc_Volley` | Ustalık 1 / 4 | Alan (hedefte) | 62 / 11 / 140 | 1700 / r380 | `Poison` |
| 16 | `Arc_ShadowStep` | Ustalık 2 / 2 | Buff | 70 / 18 / — | — | `HasteII` |
| 20 | `Arc_Snipe` | Ustalık 1 / 5 | Menzilli hasar | 90 / 14 / 290 | 2000 | — |
| 24 | `Arc_DragonArrow` | Ustalık 1 / 5 | Menzilli hasar | 105 / 13 / 360 | 1900 | `Slow` |
| 28 | `Arc_DeathMark` | Ustalık 1 / 6 | Menzilli hasar | 125 / 15 / 430 | 2000 | `Poison` |
| **31 ★** | `Arc_ThornVolley` | Ustalık 1 / 6 | Alan (hedefte) | 125 / 14 / 330 | 1700 / r420 | `Poison` |
| 33 | `Arc_WindsBreath` | Ustalık 2 / 3 | Buff | 145 / 24 / — | — | `HasteIII` |
| **39 ★** | `Arc_Impaler` | Ustalık 1 / 6 | Menzilli hasar | 160 / 16 / 470 | 2000 | — |
| 45 | `Arc_SpikeTrap` | Ustalık 3 / 1 | Alan (hedefte) | 195 / 18 / 510 | 1900 / r480 | `Poison` |

### Büyücü (15 beceri ≤ Lv45)

| Lv | Id | Ağaç / rütbe | Etki | Mana / Bkl / Güç | Menzil / Yarıçap | Efekt |
|---:|---|---|---|---|---|---|
| 1 | `Mag_IceBolt` | Temel / 1 | Menzilli hasar | 10 / 1,5 / 40 | 1400 | — |
| 2 | `Mag_Fireball` | Ustalık 1 / 1 | Menzilli hasar | 12 / 2 / 58 | 1500 | — |
| 3 | `Mag_FrostNova` | Ustalık 2 / 1 | Alan (kendi etrafı) | 20 / 6 / 45 | r360 | `Slow` |
| 6 | `Mag_Meteor` | Ustalık 1 / 2 | Alan (hedefte) | 38 / 12 / 95 | 1500 / r420 | — |
| 8 | `Mag_ChainLightning` | Ustalık 1 / 3 | Menzilli hasar | 42 / 10 / 130 | 1600 | `Slow` |
| 10 | `Mag_FlameStorm` | Ustalık 1 / 4 | Alan (hedefte) | 45 / 11 / 155 | 1800 / r430 | — |
| 13 | `Mag_IceLance` | Ustalık 2 / 2 | Menzilli hasar | 58 / 8 / 195 | 1800 | `Slow` |
| 16 | `Mag_IceChain` | Ustalık 2 / 2 | Menzilli hasar | 70 / 12 / 245 | 1800 | `Slow` |
| 20 | `Mag_ArcaneBlast` | Ustalık 1 / 5 | Alan (hedefte) | 95 / 13 / 270 | 1800 / r450 | — |
| 24 | `Mag_Thunderclap` | Ustalık 1 / 5 | Menzilli hasar | 105 / 13 / 380 | 1900 | `Slow` |
| 28 | `Mag_Disintegrate` | Ustalık 1 / 6 | Menzilli hasar | 120 / 12 / 420 | 1900 | — |
| **31 ★** | `Mag_Inferno` | Ustalık 1 / 6 | Alan (hedefte) | 130 / 14 / 360 | 1900 / r470 | — |
| 33 | `Mag_FreezingTempest` | Ustalık 2 / 3 | Alan (kendi etrafı) | 145 / 16 / 380 | r480 | `Slow` |
| **39 ★** | `Mag_Starfall` | Ustalık 1 / 6 | Menzilli hasar | 170 / 17 / 540 | 2000 | — |
| 45 | `Mag_ArcaneBurst` | Ustalık 3 / 1 | Alan (hedefte) | 195 / 16 / 520 | 1900 / r500 | — |

### Rahip (17 beceri ≤ Lv45)

| Lv | Id | Ağaç / rütbe | Etki | Mana / Bkl / Güç | Menzil / Yarıçap | Efekt |
|---:|---|---|---|---|---|---|
| 1 | `Pri_HolyLight` | Temel / 1 | Kendini iyileştir | 15 / 3 / 60 | — | — |
| 1 | `Pri_Smite` | Temel / 1 | Menzilli hasar | 10 / 2 / 35 | 1200 | — |
| 3 | `Pri_Renew` | Ustalık 1 / 1 | Buff | 18 / 12 / — | — | `Regen` |
| 4 | `Pri_HolyBurst` | Ustalık 1 / 1 | Alan (kendi etrafı) | 25 / 8 / 48 | r320 | — |
| 5 | `Pri_GreaterHeal` | Ustalık 2 / 1 | Hedefi iyileştir | 40 / 12 / 150 | — | — |
| 7 | `Pri_DivineRestoration` | Ustalık 2 / 2 | Hedefi iyileştir | 55 / 20 / 180 | — | `Regen` |
| 10 | `Pri_HolyFlame` | Ustalık 1 / 2 | Menzilli hasar | 45 / 10 / 150 | 1500 | — |
| 13 | `Pri_Judgement` | Ustalık 1 / 2 | Menzilli hasar | 58 / 9 / 195 | 1500 | — |
| 16 | `Pri_HolyRain` | Ustalık 2 / 3 | Hedefi iyileştir | 110 / 16 / 380 | — | — |
| 20 | `Pri_Mending` | Ustalık 2 / 3 | Hedefi iyileştir | 120 / 14 / 520 | — | — |
| 24 | `Pri_WrathOfAngels` | Ustalık 1 / 3 | Alan (hedefte) | 105 / 13 / 290 | 1500 / r420 | — |
| 28 | `Pri_Retribution` | Ustalık 1 / 4 | Menzilli hasar | 105 / 11 / 330 | 1500 | — |
| **31 ★** | `Pri_RadiantNova` | Ustalık 1 / 4 | Alan (hedefte) | 135 / 14 / 340 | 1500 / r440 | — |
| 33 | `Pri_LifeBond` | Ustalık 2 / 4 | Hedefi iyileştir | 210 / 20 / 750 | — | `Regen` |
| **39 ★** | `Pri_SalvationLight` | Ustalık 2 / 4 | Hedefi iyileştir | 260 / 30 / 1100 | — | — |
| 45 | `Pri_AngelicBlade` | Ustalık 1 / 4 | Menzilli hasar | 195 / 18 / 510 | 1500 | — |
| 45 | `Pri_HolyShield` | Ustalık 3 / 1 | Buff | 120 / 22 / — | — | `IronSkinII` |

(Savaşçı/Okçu/Büyücü/Rahip adları `GetMMOClassDisplayName`, `MMOSkillTypes.h:20-30`.) Lv45 sonrası: Savaşçı 4 (Lv58 ×2, Lv70, Lv80), Okçu 3, Büyücü 3, Rahip 4 beceri.

### Boşlukların yeri

**[Kesin: kod + `ValidateData.py`]** Yeni-beceri seviyeleri (Lv1 başlangıç sayılır, eşik `SKILL_GAP_LEVELS = 5` → fark ≥ 5 uyarı, `ValidateData.py:141`):

| Sınıf | Lv1–33 arası yeni-beceri seviyeleri | Sonrası |
|---|---|---|
| Savaşçı | 1,2,3,4,5,7,10,13,16,20,24,28,**31**,33 | **39**, 45, 58, 70, 80 |
| Okçu | 1,2,3,5,7,10,13,16,20,24,28,**31**,33 | **39**, 45, 58, 70, 80 |
| Büyücü | 1,2,3,6,8,10,13,16,20,24,28,**31**,33 | **39**, 45, 58, 70, 80 |
| Rahip | 1,3,4,5,7,10,13,16,20,24,28,**31**,33 | **39**, 45, 58, 70, 80 |

(Lv1–33 aralığında dördünde de en büyük fark 4 seviye.) Kalan UYARI aralıkları: **33→39 (6) ve 39→45 (6)**, sonra 45→58 (13), 58→70 (12), 70→80 (10). Karar 5'in asıl derdi (33→45 = 12 seviye) iki yarıya bölündü.

**Bu boşlukların "ödülsüz süresi" [Model]** (`ProgressionModel.py --max 45`, görevli = iyimser, tüm kapısı açık görevler yapılmış): Lv28→31 ≈ 60 öldürme, Lv31→33 ≈ 43; Lv33→39 ≈ 126 (görevsiz ≈ 134), Lv39→45 ≈ 119 (görevsiz ≈ 133). **[Tahmin]** 20–30 sn/öldürme varsayımıyla (ölçülmemiş) her 6 seviyelik aralık kabaca 40–65 dk; yani oyuncu yeni beceri görmeden yaklaşık bir saat oynayabilir. Model Lv33–45 arası görevli öldürme sayısını 245 verir (`DECISIONS_NEEDED.md` ile tutarlı).

## 2. Sekiz beceri: alanlar ve türetme

Sütun sırası `MakeDef`'in konum sırasıdır **[Kesin: MMOSkillDatabase.cpp:9-12]**: `Id, Ad, Sınıf, Ağaç, Rütbe, ReqLevel, SPCost, Önkoşul, Etki, Mana, Bekleme, Güç, Menzil, Yarıçap, [EfektId]`. `SPCost` sütununu kod **hiç okumaz** (`SkillPointCost` yalnız tanımlı ve atanır; kaynak taraması: `MMOSkillTypes.h:88`, `MMOSkillDatabase.cpp:21`), her rütbe 1 puan harcar (bkz. 4.1); tablodaki 2/3 değerleri yalnız komşu satırlarla tutarlılık içindir. Hepsi **[PLACEHOLDER]**. Yeni `EMMOEffectDef` gerekmedi; kullanılan efektler `Slow` ve `Poison` (mevcut, `MMOEffectDatabase.cpp:33-34`).

| Id | Ad (koddaki) | Sınıf / ağaç | Rütbe | Lv | SP* | Önkoşul | Etki / hedef türü | Mana | Bkl | Güç | Menzil | Yarıçap | Efekt | Kod satırı |
|---|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---:|---|---|
| `War_Earthshaker` | Yer Sarsintisi | Savaşçı / Ustalık 1 | 6 | 31 | 2 | `War_TempestSlash` | Alan, **kendi etrafı** (hedefsiz) | 130 | 14 | 330 | 0 | 460 | `Slow` | 103-104 |
| `War_Skullsplitter` | Kafa Yarici | Savaşçı / Ustalık 1 | 6 | 39 | 2 | `War_Rampage` | Yakın, tek hedef | 165 | 16 | 445 | 240 | 0 | — | 107-108 |
| `Arc_ThornVolley` | Dikenli Sagnak | Okçu / Ustalık 1 | 6 | 31 | 2 | `Arc_Volley` | Alan, **hedef merkezli** | 125 | 14 | 330 | 1700 | 420 | `Poison` | 158-159 |
| `Arc_Impaler` | Mizrakli Ok | Okçu / Ustalık 1 | 6 | 39 | 2 | `Arc_DeathMark` | Menzilli, tek hedef | 160 | 16 | 470 | 2000 | 0 | — | 162-163 |
| `Mag_Inferno` | Alev Patlamasi | Büyücü / Ustalık 1 | 6 | 31 | 2 | `Mag_ArcaneBlast` | Alan, hedef merkezli | 130 | 14 | 360 | 1900 | 470 | — | 213-214 |
| `Mag_Starfall` | Yildiz Dusu | Büyücü / Ustalık 1 | 6 | 39 | 2 | `Mag_Disintegrate` | Menzilli, tek hedef | 170 | 17 | 540 | 2000 | 0 | — | 217-218 |
| `Pri_RadiantNova` | Isiltili Nova | Rahip / Ustalık 1 | 4 | 31 | 2 | `Pri_WrathOfAngels` | Alan, hedef merkezli | 135 | 14 | 340 | 1500 | 440 | — | 286-287 |
| `Pri_SalvationLight` | Kurtulus Isigi | Rahip / Ustalık 2 | 4 | 39 | 3 | `Pri_LifeBond` | Müttefik iyileştirme (seçili parti üyesi, ≤800 u; yoksa kendini) | 260 | 30 | 1100 | 0 (kodda 800 varsayılan) | 0 | — | 290-291 |

Dosya hepsi `Source/MyProject/Private/Skills/MMOSkillDatabase.cpp`. `Önkoşul` alanı gating'de **kullanılmıyor** (yerini rütbe aldı, `MMOSkillTypes.h:96-99`); yalnız tutarlılık ve `ValidateData.py`'nin "devamı daha zayıf" kuralı için.

### 2.1 Komşularına göre türetme **[Hesap]**

Her yeni beceri, aynı sınıf ve aynı etki türündeki alt/üst komşusunun **arasına** yerleştirilmiş. Yüzdeler "yeni beceri, komşuya göre". `güç/sn` = Güç ÷ Bekleme, `güç/mana` = Güç ÷ Mana.

| Yeni beceri | güç/sn | güç/mana | Alt komşu (Lv) → fark | Üst komşu (Lv) → fark |
|---|---:|---:|---|---|
| `War_Earthshaker` | 23,6 | 2,54 | `TempestSlash` (24): güç +22 %, mana +37 %, bkl +17 %, güç/sn +5 %, güç/mana −11 %, yarıçap 460/440 | `WarDeluge` (58): güç −50 %, güç/sn −29 %, yarıçap 460/560 |
| `War_Skullsplitter` | 27,8 | 2,70 | `Rampage` (28): güç +31 %, mana +50 %, güç/sn +6 %, güç/mana −13 % | `WarCyclone` (45): güç −13 %, güç/sn −2 %, güç/mana +3 % |
| `Arc_ThornVolley` | 23,6 | 2,64 | `Volley` (13): güç +136 %, güç/sn +85 %, güç/mana +17 % | `SpikeTrap` (45): güç −35 %, güç/sn −17 %, güç/mana +1 % |
| `Arc_Impaler` | 29,4 | 2,94 | `DeathMark` (28): güç +9 %, mana +28 %, güç/sn +2 %, güç/mana −15 %, **Zehir yok** | `HuntersReckoning` (80): güç −71 % (tek hedef üst komşu çok uzak) |
| `Mag_Inferno` | 25,7 | 2,77 | `ArcaneBlast` (20): güç +33 %, güç/sn +24 %, güç/mana −3 % | `ArcaneBurst` (45): güç −31 %, güç/sn −21 %, güç/mana +4 % |
| `Mag_Starfall` | 31,8 | 3,18 | `Disintegrate` (28): güç +29 %, bkl **+42 %**, güç/sn **−9 %**, güç/mana **−9 %** | `ArcaneBurst` (45, alan): güç +4 %, güç/mana +19 % (farklı tür) |
| `Pri_RadiantNova` | 24,3 | 2,52 | `WrathOfAngels` (24): güç +17 %, güç/sn +9 %, güç/mana −9 % | `AngelicStorm` (58): güç −48 %, güç/sn −26 % |
| `Pri_SalvationLight` | 36,7 | 4,23 | `LifeBond` (33): güç +47 %, bkl +50 %, güç/sn −2 %, güç/mana +18 % (Regen yok) | `HandOfHeaven` (80): güç −33 %, güç/sn −47 % |

**Ortak kurallar (kodda tutarlı):** güç/mana tabanı 2,5–3,2, bekleme 14–17 sn (Lv28: 12–15; Lv45: 16–18). Aynı ağaçta ve **aynı rütbe**de duran yeni halkalar: `Special0` rütbe 6 Savaşçı'da Lv28, **31**, **39**, 45 (dört beceri), Okçu ve Büyücü'de Lv28, **31**, **39** (üç beceri); Rahip `Special0` rütbe 4 (Lv28, **31**, 45) ve `Special1` rütbe 4 (Lv33, **39**). Yani yeni beceriler **ek rütbe istemez** (4.1).

## 3. Sınıf rolü / kimlik gerekçesi

Boşluğun neden "alan Lv31, tek hedef Lv39" bölündüğü `CONTENT_PLAN_LV11_45.md` B.0'da: Lv28–45 aralığında dört sınıfta da aynı kategoride 25–34 seviye atlayan hat vardı (ör. Savaşçı alan hattı `TempestSlash` Lv24 → `WarDeluge` Lv58). **[Kesin]** Aşağıdaki "kimlik" yargıları tasarım niyetidir; oyunda denenmedi.

| Beceri | Sınıf rolü | Neden bu halka | Zayıf kimlik noktası |
|---|---|---|---|
| `War_Earthshaker` | Savaşçı = ön hat, kalabalık kesici; alan hattına **Yavaşlatma** ekler | `Slow` şimdiye dek Savaşçı'da yalnız tek hedef `ShatterBlow`'daydı; alan + Yavaşlatma ön hatta "geri çekil, vur" oyunu verir | `Slow` %35, 6 sn (`MMOEffectDatabase.cpp:34`): kısa; mob hızı düşer, ama Savaşçı'nın yakın menzilinde mob zaten yapışık |
| `War_Skullsplitter` | Savaşçı tek hedef zinciri `Rampage` Lv28 → `WarCyclone` Lv45 köprüsü | Rütbe 6'nın dörtlüsünün orta halkası; en yüksek tek vuruş (445) Lv39 öncesi | Etkisiz (düz hasar); `Rampage` ile aynı rolde, yalnız sayısal artış |
| `Arc_ThornVolley` | Okçu = uzaktan alan; `Volley` (Lv13) → `SpikeTrap` (Lv45) arası 32 seviyelik alan boşluğunu kapatır | Zehirli sağanak kimliği korunur | **Zehir fiilen etkisiz (4.2 c)**; gerçek fark yalnız hasar/yarıçap |
| `Arc_Impaler` | Okçu tek hedef, menzil 2000 (`Snipe`/`DeathMark` ile aynı) | `DeathMark` (Lv28) ile `ArrowTempest`/`HuntersReckoning` (Lv70/80) arası tek hedef köprü | `DeathMark`'a göre Zehir yok ve güç/mana −15 %; **kimliği zayıf** (QA DÜŞÜK-4) |
| `Mag_Inferno` | Büyücü = alan patlaması; `ArcaneBlast` (Lv20) → `ArcaneBurst` (Lv45) arası | Ateş alan hattı, yarıçap 470 | Efekt yok; `ArcaneBlast`'tan farkı yalnız sayısal |
| `Mag_Starfall` | Büyücü tek hedef "en yüksek patlama" (540 > `Disintegrate` 420) | Anlık hasar +%29, menzil +100 u | **Sürekli kullanımda `Disintegrate`'ın gerisinde** (güç/sn −9 %, güç/mana −9 %): kullanım <%2 riski (4.3) |
| `Pri_RadiantNova` | Rahip'in "savaşçı yüzü": alan hasarı | `WrathOfAngels` (Lv24) → `AngelicStorm` (Lv58) arası | Rahip'te alan hasarı yan rol; tek başına parti için işlev yok |
| `Pri_SalvationLight` | Rahip = sifacı; **acil şifa** (en büyük tek vuruş, en uzun bekleme) | Şifa zinciri `Mending` → `LifeBond` → `SalvationLight` → `HandOfHeaven` tam dolu: Lv39 halkası parti için "ölmek üzere olan" çözümü | Havuzun %79'u; `Mending` ile güç/sn eşit; ayrıntı 4.2 (d) |

## 4. Risk ve etkileşim

### 4.1 Puan bütçesi (rütbe + bakiye) — etki **sıfır**

- **[Kesin]** Skill puanı becerilere değil ustalık **rütbesine** gider: `RequestInvestMastery` her yatırımda 1 puan harcar, ağacın rütbesini +1 yapar ve o rütbeyi isteyen tüm becerileri (seviyesi yeterliyse) açar (`MMOSkillComponent.cpp:692-734`, `SyncLearnedFromMastery:594-625`). Kural: rütbe ≤ karakter seviyesi (`:719`).
- **[Kesin]** Kazanılan bütçe `(Seviye−1) × 2` (`GetMasteryPointBudget`, `MMOSkillComponent.cpp:999-1003`; `SkillPointsPerLevel = 2`, `MMOStatsComponent.h:190`): Lv31 = 60, Lv39 = 76, Lv45 = 88 puan.
- **[Kesin]** Sınıf başına azami toplam rütbe (tüm ağaçlar tavanda): Savaşçı **16** (Temel 1, Ustalık 1: 7, Ustalık 2: 3, Ustalık 3: 3, Ustalık 4: 2), Okçu/Büyücü/Rahip **14**. Yani Lv45'te en az 72 puan (Savaşçı) harcanmadan kalır; puan bu aralıkta **kıt değil**.
- **[Kesin]** Sekiz yeni beceri mevcut rütbelerde (Savaşçı/Okçu/Büyücü Ustalık 1 rütbe 6, Rahip Ustalık 1 rütbe 4, Ustalık 2 rütbe 4) duruyor → azami rütbe toplamı **değişmedi**, bütçeye ek maliyet **yok**. `ValidateData.py` "rütbeler boşluksuz" kuralı (`:134-136`) bunu denetliyor ve geçiyor.
- **Kayıt uyumu [Kesin]:** `SanitizeSavedMasteryRanks` kayıtlı rütbeyi `max(rütbe, beceri.RequiredMasteryRank)` ile onarır (`:1019-1036`); yeni beceriler zaten gereken rütbede olduğundan onarım **yeni puan istemez**. Rütbe 6'ya (Rahip: 4) önceden yatırım yapmış, Lv31+/Lv39+ karakter girişte becerileri `SyncLearnedFromMastery(SavedLevel)` ile (`:1093`) kendiliğinden öğrenir; **hotbar'a otomatik atanmaz** **[Muhtemel]** (tek otomatik atama `GrantStarterSkill`). PIE doğrulaması `CHECKLIST_FIXES_2026-10-05.md:211` maddesinde bekliyor.
- **Dolaylı etki:** rütbe 6 Savaşçı'da **dört** (Lv28/31/39/45), Okçu ve Büyücü'de **üç** (Lv28/31/39) beceriyi tek puanla açıyor; oyuncu rütbe 6'yı bir kez Lv6+'da yatırınca bu beceriler seviye geldikçe bedavaya düşer. Bu, "puan harcama kararı"nı ortadan kaldırır (karar 5 (c) satırındaki "puan fazlası artar" riskinin sebebi bu). Anlamlı tercih isteniyorsa ayrı iş: ör. her seviye başına puanı azaltmak veya rütbe 6'yı bölmek; bu belgenin kapsamı dışı.

### 4.2 Denge bulguları (bu belgenin getirdikleri)

**(a) Mana, tek beceriyle sürdürülemez [Hesap, formül Kesin].** Referans mob: Lv31 `DuskProwler` (HP 1014, Def 37), Lv39 `DuskWraith` (HP 1238, Def 45) — `MMOMobArchetype.cpp:190,203`, HP = `50 + 8×Vit + 12×Lv`, Def = `0,5×Vit` (`MMOStatsComponent.cpp:356,360`; mobun `InitializePrimaryStats(Str,10,6,Vit)`, `MMOMob.cpp:392`). Havuz INT'siz taban (stat puanı yok); puan basan oyuncuda pay düşer.

| Beceri | Havuz (INT'siz) | Havuzun % | Doludan atış | Mana/sn (skiller kendi CD'sinde) | Savaşta yenilenme /sn | Sürdürülebilir pay |
|---|---:|---:|---:|---:|---:|---:|
| `War_Earthshaker` | 225,5 | %58 | 1,7 | 9,3 | 3,7 | %40 |
| `War_Skullsplitter` | 269,5 | %61 | 1,6 | 10,3 | 4,2 | %41 |
| `Arc_ThornVolley` | 235,5 | %53 | 1,9 | 8,9 | 3,7 | %41 |
| `Arc_Impaler` | 279,5 | %57 | 1,8 | 10,0 | 4,2 | %42 |
| `Mag_Inferno` | 305,5 | %43 | 2,3 | 9,3 | 3,7 | %40 |
| `Mag_Starfall` | 349,5 | %49 | 2,1 | 10,0 | 4,2 | %42 |
| `Pri_RadiantNova` | 285,5 | %47 | 2,1 | 9,6 | 3,7 | %38 |
| `Pri_SalvationLight` | 329,5 | **%79** | **1,3** | 8,7 | 4,2 | %48 |

Savaş dışı yenilenme (14,8 / 16,8 mana/sn) bu hızı karşılar; **savaşta** (×0,25) tek bir yeni beceri bile sürekli basılamaz. Bu, önceki belgelerin "güç/mana %" analizinin görmediği kısıt: beceri seçimi mana ile sınırlı, hotbar'ın 10 yuvası değil. Mevcut Lv28 ve Lv45 satırları aynı aralıkta (%39–64), yani yeni halkalar kalıbı bozmuyor; ama **kalıbın kendisi** (savaşta sürdürülebilir pay ≈ %40) ölçülmeli.

**(b) Alan becerilerin hedef sayısıyla ölçeklenmesi [Kesin kod + Hesap].** Alan hasarı her moba düz `Def.Power` uygular, hedef sınırı yok (`TActorIterator<AMMOMob>`, `MMOSkillComponent.cpp:1310-1328`). Lv31–39 bandının spawner'ı `MaxMobs = 5`, `SpawnRadius = 225`, Lv31–39 (`MMOGameMode.cpp:1273-1276`) — yarıçap 420–470 u'luk alanlar tüm sürüyü kapsar **[Muhtemel]**. Güç/sn, **hedef başına**:

| Alan becerisi (Lv31) | n=1 | n=3 | n=5 | Sınıfın sonraki tek hedef hasar becerisi | Eşit olduğu n |
|---|---:|---:|---:|---:|---:|
| `War_Earthshaker` | 23,6 | 70,7 | 117,9 | `Skullsplitter` 27,8 | 1,18 |
| `Arc_ThornVolley` | 23,6 | 70,7 | 117,9 | `Impaler` 29,4 | 1,25 |
| `Mag_Inferno` | 25,7 | 77,1 | 128,6 | `Starfall` 31,8 | 1,24 |
| `Pri_RadiantNova` | 24,3 | 72,9 | 121,4 | `AngelicBlade` (Lv45) 28,3 | 1,17 |

İki mobdan itibaren alan beceri tek hedefi geçer (4 sınıfta da). Bu yeni değil (aynı örüntü `TempestSlash` vs `Rampage` vb.) ve `SkillDominance` taslağı (alan vs tek hedef ayrı "tür") bunu görmüyor; ama Lv39 tek hedef becerileri (Skullsplitter, Impaler, Starfall) sürü karşısında bir **boss/elit becerisi** sayılmalı: bandın tek elit hedefi `DuskOverlord` Lv40 (`MMOMobArchetype.cpp:377`), tek mob, yani tek hedef becerilerin gerçek yeri orası. Bu dengeli ise kasıtlı; değilse alan becerilere hedef sınırı ya da hedef başı hasar düşüşü eklemek kod işidir (bu belgenin dışı). **[Tahmin]** ölçümde sürü avında alan becerilerin atış payı tek hedefleri çok geçerse karar gerekir.

**(c) `Poison` Lv31+ moblarda fiilen etkisiz [Kesin kod yolu + Hesap].** `Poison` = 8 hasar/tick × 8 sn (`MMOEffectDatabase.cpp:33`). DoT her tick'te `UGameplayStatics::ApplyDamage(Magnitude)` çağırır (`MMOStatsComponent.cpp:938`); bu da `MMOCharacter::TakeDamage` → `Stats->ApplyDamage` → `max(1, ham − Defense)` (`:412`). Mob Def'i `0,5×Vit`: Lv31 `DuskProwler` 37, Lv39 `DuskWraith` 45 → tick başı **1**, uygulama başı toplam **8 hasar** (vuruşun 330–470 gücünün %2'si). Defans ≥ 7 olan her mobda (Vit ≥ 14) aynıdır. Sonuç: `Arc_ThornVolley`'nin tek ayırt edici özelliği (`Poison`) sayısal olarak boş; `Arc_Volley`, `DeathMark`, `SpikeTrap`, `PoisonCloud` için de geçerli **[Hesap]**; ve `DECISIONS_NEEDED.md` / QA DÜŞÜK-4'ün "Impaler'a Zehir ekle" önerisi mevcut `Poison` ile **hiçbir şey kazandırmaz**. `Slow` ise hız efektidir (savunmadan etkilenmez), çalışır. Detay seçenekleri 4.3'te.

**(d) `Pri_SalvationLight` [Kesin kod + Hesap + Tahmin].** Güç 1100, ama `Heal` `MaxHealth`'e kırpılır (`MMOStatsComponent.cpp:438`). Stat puansız Lv39 temel can ≈ 630–646 (Vit 14–16 → `50+8×Vit+12×39`), tüm 114 stat puanı Vit'e giderse ≈ 1.540–1.560. **[Tahmin]** Çoğu oyuncuda 1100'ün büyük kısmı fazla şifa olur; gerçek değeri müttefik canı ≥ ~900 olunca doğar. Ayrıca `HealTarget` hedef gerektirmez; hedef yoksa/uzaktaysa **kendini** iyileştirir (`MMOSkillComponent.cpp:1220-1251`, `bNeedsTarget` listesinde değil `:889-891`), yani yanlış basış 260 mana (havuzun %79'u) ve 30 sn bekleme harcar. Menzil: `Range = 0` → varsayılan 800 u (`:1232`).

**(e) Hotbar 10 yuva [Kesin + Hesap].** Tüm rütbeler yatırılmış karakterde açık beceri sayısı: Savaşçı Lv30 14 → Lv39 17 → Lv45 19; Okçu 12 → 15 → 16; Büyücü 11 → 14 → 15; Rahip 12 → 15 → 17. 10 slot + mana kısıtı (a) → bazı beceriler hotbar'a hiç girmeyebilir.

**(f) Beceri hasarı stat'tan bağımsız [Kesin].** `Def.Power` düz değer; STR/INT yalnız mana havuzunu (INT) ve temel saldırıyı etkiler. Yeni beceriler Savaşçı'yı/Büyücüyü stat dağılımına göre ayırmaz; sınıf farkı yalnız tablo ve mana havuzundan gelir.

### 4.3 Bilinen dengesiz üçlü (Starfall / Impaler / SalvationLight) ile etkileşim

Bu üçü **sekiz yeni becerinin içinde**; QA `QA_ROUND_2026-10-06_NIGHT2.md` DÜŞÜK-4 ve `DECISIONS_NEEDED.md` "Lv39 beceri dengesi" ile aynı konu. **[Hesap]** Seçenekler (hiçbiri uygulanmadı, hepsi **[PLACEHOLDER]**; tek sayı değişir, kayıt/JSON kimliği değişmez):

| Beceri | Mevcut güç/sn · güç/mana | Seçenek | Sonuç güç/sn · güç/mana | Yan etki |
|---|---|---|---|---|
| `Mag_Starfall` (`MMOSkillDatabase.cpp:217-218`) | 31,8 · 3,18 | CD 17 → 14 | 38,6 · 3,18 | `Disintegrate` (35,0 · 3,50) önüne geçer, mana verimi hâlâ geride |
| | | Güç 540 → 600 | 35,3 · 3,53 | `Disintegrate`'ın her ekseninde ≥ olur (güç, menzil, güç/sn, güç/mana) → baskın çift; seviye farkı 11 olduğundan 10'luk pencerede sayılmaz, 20'lik pencerede sayılır |
| `Arc_Impaler` (`:162-163`) | 29,4 · 2,94 | CD 16 → 14 | 33,6 · 2,94 | `DeathMark` (28,7 · 3,44) güç/sn'de geçilir, güç/mana yine geride |
| | | Mana 160 → 140 | 29,4 · 3,36 | `DeathMark` mana verimine yaklaşır, güç/sn eşit |
| | | `Poison` ekle | — | **Etkisiz (4.2 c)**; denenmemeli. Gerçek Zehir istenirse yeni efekt: aşağı |
| `Pri_SalvationLight` (`:290-291`) | 36,7 · 4,23 | Mana 260 → 240 | 36,7 · 4,58 | Havuz payı %79 → %73; `Mending` (37,1 · 4,33) mana verimini geçer |
| | | CD 30 → 26 | 42,3 · 4,23 | Güç/sn `Mending`'i geçer; "acil" kimliği azalır |

**Gerçek Zehir istenirse [Tahmin, yeni efekt gerekir]:** `MMOEffectDatabase.cpp`'ye bir "II" kademesi (mevcut `RageII`/`IronSkinII` kalıbı, yorum `:42-48`); sayı Def'e göre seçilmeli: `Magnitude 55, Duration 8, Tick 1` → Def 45'te tick 10, toplam 80 (Impaler netinin ≈ %19'u); Def 37'de tick 18, toplam 144. Aynı `EffectId` yenilenir, yığılmaz (`:763-777`). Bu eklenirse **tüm Zehir kullanıcıları** (6 beceri) sayısal olarak farklı kalır; yalnız Impaler'a vermek tutarsız olur. **Öneri [Tahmin]:** ölçümden önce üçüne de dokunma (QA ve bu belge aynı yönde); ölçüm gelirse ilk aday `Mag_Starfall` CD 17 → 14 (tek sayı, mana dengesini bozmaz).

### 4.4 Ölçüm boşluğu

`DECISIONS_NEEDED.md` ve `VERTICAL_SLICE_LV1_30.md` M12 ("beceri başına atış, ölü beceri <%2") önceden planlı, ama **[Kesin]** koddaki metrik yalnız oyuncunun **ilk** beceri kullanımını kaydediyor (`RecordFirstSkillMetric` → `RecordFirstActionMetric("skill")`, `MMOPlayerController.cpp:196-205`). Beceri başına atış sayısı, "mana yetersiz" olayı ve ölü beceri **ölçülemez**. Yeni beceriler için bu, ölçüm yapılana kadar 4.2/4.3'teki tüm kararların elle (test notu) verilmesi demek. `MMOPlayerController.cpp` şu an çalışma ağacında değişmiş (Codex); bu iş ona dokunmuyor, ama bir `cast` olayı eklemek isteyen o dosyaya ve `MMOSkillComponent::UseSkill` sunucu dalına (`:900-916`) bakmalı.

## 5. Uygulama notu (kod yazılmadı)

### 5.1 Bugünkü durum — yeni `Make*` satırı gerekmiyor

Sekiz `Table.Add(MakeDef(...))` satırı zaten var. Dosya `Source/MyProject/Private/Skills/MMOSkillDatabase.cpp`:

| Sınıf | Lv31 satırı | Lv39 satırı | Bağlam |
|---|---|---|---|
| Savaşçı | 103-104 | 107-108 | Lv33 `War_BloodLord` 105-106 arası |
| Okçu | 158-159 | 162-163 | `WindsBreath` 160-161 arası |
| Büyücü | 213-214 | 217-218 | `FreezingTempest` 215-216 arası |
| Rahip | 286-287 | 290-291 | `LifeBond` 288-289 arası |

**Yalnız sayı değiştirme** (4.3 seçenekleri): ilgili satırın `Mana, Bekleme, Güç` literalleri; id değişmez. Dokunma etkileri: (1) kayıt JSON testi `Mag_Starfall` id'sini sabit kullanıyor (`MMOCharacterSaveJson.cpp:583,584,589,884,1013`) — id aynı kaldıkça etkilenmez; (2) kayıttan yüklenen bekleme kırpması `Def->Cooldown`'a bağlı (`MMOSkillComponent.cpp:1114-1118`), yani CD değişimi eski kayıt bekleme süresini en fazla yeni CD+1 sn'ye kırpar **[Kesin, davranış etkisi Tahmin]**.

### 5.2 `ValidateData.py` neyi denetliyor / neyi **denetlemiyor** **[Kesin: Scripts/ValidateData.py:100-149]**

| Kural | Seviye | Satır | Not |
|---|---|---|---|
| Kimlik tekilliği (`Beceri`) | HATA | `:114` | |
| Ayrıştırma sayısı = `MakeDef` çağrı sayısı | HATA | `:115-117` | Açıklama/ad içinde çift tırnak ya da ayrıştırılamayan alan sayıyı bozar |
| Önkoşul beceri tabloda var | HATA | `:127-128` | |
| `EffectId` efekt tablosunda var | HATA | `:129-130` | Yeni efekt gerekirse önce `MMOEffectDatabase.cpp`'ye |
| Rütbe ≤ gereken seviye | HATA | `:131-132` | |
| (Sınıf, ağaç) rütbeleri boşluksuz 1..N | HATA | `:134-136` | Rütbe 7 eklemek ara rütbeyi boş bırakırsa düşer |
| Devamı (aynı etki türü, önkoşul) önkoşuldan zayıf | UYARI | `:119-123` | Güç > 0 için |
| Beceri boşluğu ≥ 5 seviye | UYARI | `:138-149` | Bugün 20 satır |

**Denetlemiyor:** `SPCost`, Mana/Bekleme/Menzil/Yarıçap aralıkları, baskınlık (alan vs tek hedef dahil), önkoşulun sınıf/ağaç tutarlılığı, hotbar kapasitesi, mana havuzu yüzdesi, yarıçapın spawner yarıçapına oranı. Ayrıca `TestValidateData.py`'de beceriyle ilgili senaryo olarak yalnız beceri boşluğu UYARI'sı bulundu (`:44-70`, yapay `ReqLevel` eşlemeleriyle; `beceri|Beceri|MakeDef` taramasıyla); beceri HATA kuralları (önkoşul, efekt, rütbe) ve yeni satırların sayısal makullüğü için negatif test **görmedim**. Baskınlık için `CONTENT_PLAN_LV11_45.md` Ek A taslağı repoya girmemiş (`Scripts/*Dominance*` yok); bu belge aynı mantığı geçici bir betikle (repo dışı) koşturdu: **pencere 10 seviye, 6 baskın çift, hiçbiri yeni beceri içermiyor** (`Rampage≥Execute`, `DeathMark≥Snipe`, `Thunderclap≥IceChain`, `Judgement≥HolyFlame`, `Mending≥HolyRain`, `AbsoluteAnnihilation≥MeteorShower`).

### 5.3 Doğrulama (editör kapalı, derleme `ue-build` skill'i ile)

`powershell -File Scripts\RunAllChecks.ps1 -SkipBuild` (CI aynısını koşar); beklenen ilgili çıktı değişmeden: `Beceri: 81`, 20 UYARI, "Veri denetimi temiz". PIE'de kalanlar (hepsi **[Tahmin]**, oynanmadı): `CHECKLIST_FIXES_2026-10-05.md:211,223` (kitapta otomatik açılma, satır sırası) ve `QA_ROUND_2026-10-06_NIGHT2.md:181-183` (Zehir/Yavaşlatma uygulanıyor, 1100 şifa, "Yeterli mana yok!").

## 6. Hâlâ açık: 33→39 ve 39→45 (6'şar seviye) — seçenekler

| Seçenek | Ne olur | Uyarı sayısı (4 sınıf) | Maliyet / risk |
|---|---|---:|---|
| (d0) Bırak | İki aralık 6'şar seviye, ≈ 120'şer öldürme **[Model]** | 20 (bugün) | Düşük risk; "yeni beceri yok" hissi yarım saat–bir saat sürer **[Tahmin]** |
| (d1) Lv36 ve Lv42'ye sınıf başı birer beceri (8 satır) | Aralıklar 3'er seviye | 20 → 12 **[Hesap]** (33→39 ve 39→45 uyarıları kalkar: 2 × 4 = 8) | Mana kısıtı (4.2 a) ve hotbar (4.2 e) zaten sıkışık: yeni bir **hasar** becerisinin marjinal değeri düşük, ölü beceri riski yüksek |
| (d2) Lv36 / Lv42 için hasar yerine **araç** becerisi (buff, iyileştirme) | Mana-yük/hotbar baskısı azalır | 20 → 12 | Mevcut efekt türleri yalnız hasar-zamanla, iyileştirme-zamanla, saldırı/savunma/hız buff'ı; yeni **kimlik** (sersemletme, çekme, kalkan) yeni `EMMOEffectType` ve replikasyon = kod işi, bu belge kapsamı dışı |
| (d3) Lv45 becerilerini Lv42'ye çek (her sınıfın Lv45 satırlarında seviye sayısı) | Tek sayı, yeni içerik yok | 20 → 16 **[Hesap]** (33→39 kalır, 39→45 → 39→42 uyarısız, 45→58 → 42→58 uyarılı) | Boşluğu 42→58'e taşır (13 → 16); sorunu çözmez, karar 5 (b)'nin aynı yan etkisi |

**Öneri [Tahmin]:** önce ölçüm. `[MMOMETRIC]`'te `levelup` zaman damgaları Lv33→45 süresini verir; hotbar ve mana kısıtı (4.2 a, e) yüzünden sekiz **hasar** becerisi daha eklemek yerine, gerekirse (d2) tarzı araç becerisi ya da 4.1'deki bütçe/rütbe tasarım kararı (puan harcama anlamlı olsun mu) daha değerli olabilir. Kesin karar sahibin; bu belge sayıları yalnız önkoşul olarak verir.

## 7. Kısa özet

1. Seçenek (c) **kodda var** (`56de087`); `DECISIONS_NEEDED.md` madde 5 güncel değil.
2. Sekiz beceri komşu halkaların arasında, ek rütbe ve ek puan istemiyor; yeni efekt gerekmedi; `ValidateData` temiz, kalan UYARI aralığı 33→39 ve 39→45.
3. Yeni bulgu 1: `Poison` (8/tick) Lv31+ mob savunması (37–45) karşısında tick başı 1 hasar, yani `Arc_ThornVolley`'nin Zehir'i boş ve "Impaler'a Zehir ekle" önerisi etkisiz.
4. Yeni bulgu 2: savaşta mana yenilenmesi ×0,25 → tek yeni beceri sürdürülemez (≈ %40); alan becerileri 2+ hedefte tek hedef becerileri geçer.
5. `Starfall` / `Impaler` / `SalvationLight` kararı: ölçümden önce dokunmamak; dokunulacaksa ilk aday `Starfall` CD 17 → 14.
