# Karar bekleyen konular (proje sahibi için)

**Tarih:** 2026-10-05, commit `6d5e1d0`. Bu sayfa yalnız **karar** listesidir; kod değişmedi.

**Sayıların kaynağı:** `python3 Scripts/ProgressionModel.py`, `Scripts/EconomyModel.py` (`--upgrade-to 5/7/9`, `--min-kill-frac 0`) ve `BALANCE_MODEL.md` aday eğrileri (A/B/C) bu commit'te bulutta **yeniden çalıştırıldı**. Hepsi **statik model çıktısı**; oyun içi ölçüm değil. Yürüme, ölüm, parti, mob doğma beklemesi ve gerçek öldürme süresi yok. Hiçbir karar PIE'de sınanmadı.

**Etiketler:** **[Kesin]** kodda/model çıktısında doğrudan okundu · **[Muhtemel]** model ve kod buna işaret ediyor, ölçülmedi · **[Tahmin]** varsayıma dayalı (ör. saniye/öldürme).

**Ölçüm:** `[MMOMETRIC]` satırları yalnız sunucuda basılır; özet `python Scripts/PlaytestMetrics.py Saved/Logs/MyProject.log` (olaylar: `levelup`, `kill`, `death`, `gold`). Koşu adımları: `PLAYTEST_RUNBOOK.md` Kapı 3.

## Özet

| # | Karar | Önerim | Etiket | Ölçümden önce verilebilir mi? |
|---|---|---|---|---|
| 1 | Lv1–5 hızlı başlangıç korunsun mu | **UYGULANDI (2026-10-05): B eğrisi `MMOXPTuning.cpp`ta**; ince ayar ölçümle | [Muhtemel] | Evet |
| 2 | Ölüm cezası | `max(%10 eşik, 2 öldürme XP'si)` | [Muhtemel] | Hayır, `death` olayı gerekli |
| 3 | Altın sink'i / `Q_Gold*` | `Q_Gold*` altını harcatsın + yükseltme maliyeti seviyeyle ölçeklensin | [Muhtemel] | Kısmen (Q_Gold evet, ölçek hayır) |
| 4 | Kimlik/hesap | İlk playtest için parola + tanıdık oyuncu yeter | [Muhtemel] | Evet |
| 5 | Lv28–33 beceri boşluğu (asıl boşluk Lv33→45) | **UYGULANDI (`56de087`): Lv31 + Lv39 becerileri kodda**; denge/zehir/mana bulguları `SKILL_PLAN_LV31_45.md` | [Tahmin] | Evet (tasarım); değerler ölçümle |
| 6 | Hız hedefi (Lv30 kaç saat) | 2–3 saat (B) | [Tahmin] | Evet (hedef); tutup tutmadığı ölçümle |
| 7 | `CheckPrPaths` yalnız `claude/*` dallarına mı | Hayır, tüm PR'larda kalsın | [Muhtemel] | Evet |

---

## 1. Lv1–5 hızlı başlangıç korunsun mu

**Neden önemli:** Lv1–20'nin 20 seviyesinin **tamamı** modelde "grind=0" (görev XP'si ihtiyacı karşılıyor) **[Kesin, model]**. Lv1–5 görev ödülleri ihtiyacın %347–616'sı; bu fazla XP ileri taşınıp Lv11–20'yi bedava yapıyor. Lv11–15'i grind'e sokmanın tek model yolu Lv1–5 hızından vazgeçmek (C).

| Seçenek | Ne değişir | Model etkisi (görevli öldürme/seviye; parantezde grind=0 seviye / 5) | Lv1→30 öldürme |
|---|---|---|---:|
| Mevcut (kod) | — | 11–15: 1,2 (5) · 16–20: 3,2 (5) · 21–25: 12,9 · 26–30: 18,5 | ~241 |
| **A** yalnız görev Lv6–20 | Görev çarpanı Lv6–20 | 16–20: 4,7 (4) · 21–25: 14,2 | ~255 |
| **B** mob+görev Lv6–20, Lv1–5 korunur | Mob ve görev çarpanı Lv5–20 | 11–15: 1,2 (5) · 16–20: 9,6 (1) · 21–25: 15,8 · 26–30: 18,7 | ~289 |
| **C** Lv1'den itibaren | Lv1–5 görev XP ×0,5, Lv4+ mob ×0,8 | 11–15: 4,7 (3) · 16–20: 12,9 (0) · 21–25: 15,8 | ~323 |

Hedef bantları `[PLACEHOLDER]`: 11–15: 9–13, 16–20: 11–15, 21–25: 13–18. Hiçbir aday Lv11–15'i hedefe sokmuyor (C'de bile 4,7) **[Kesin, model]**.

- **Önerim [Muhtemel]:** **B.** İlk dakikalardaki kanca korunur, Lv16–30 hedefe girer. Lv11–15'i bilinçli "hikâye/görev bandı" kabul et; düzeltmek istersen ayrı iş: Lv1–10 görevlerinin **ham** `RewardXP`'sini düşürmek ya da Lv11–15'e öldürme görevi eklemek (modelde denenmedi).
- **Değişecek dosya:** `Source/MyProject/Private/Core/MMOXPTuning.cpp` (mob ve görev çarpan düğümleri; B'nin değerleri `BALANCE_MODEL.md` "ÖNERİ" tablosunda). `ProgressionModel.py` eğriyi bu dosyadan okur, betik değişmez.
- **Gereken veri:** `levelup` olaylarından Lv1→5, 5→10, 10→20 **süresi** (dk) ve `kill` sayısı. Lv5'e ~7–10 dk'dan hızlı varılıyorsa ve oyuncu Lv11–20'de hiç grind yapmıyorsa B/C'nin gerekçesi doğrulanır.

## 2. Ölüm cezası

**Neden önemli:** Bugün ceza `min(mevcutXP, %10 × eşik)`; seviye düşmez, altın/eşya kaybı yok, 5 sn sonra doğuş **[Kesin]** (`MMOStatsComponent.cpp:391`, `MMOStatsComponent.h:191`, `MMOCharacter.h:128`). Seviye atladıktan hemen sonra (XP≈0) ölüm bedava.

Ceza, **öldürme eşdeğeri** olarak (mevcut eğri, gereken XP ÷ ortalama XP/öldürme; `ProgressionModel.simulate` ile hesaplandı) **[Kesin, model]**:

| Seçenek | Lv5 | Lv10 | Lv20 | Lv30 | XP=0 anında |
|---|---:|---:|---:|---:|---|
| Mevcut %10 | 0,18 | 0,43 | 1,25 | 1,89 | 0 |
| %20 | 0,35 | 0,85 | 2,50 | 3,78 | 0 |
| `max(%10 eşik, 2 × ort. mob XP'si)` (taban XP=0'da da kesilir, seviye düşürmez) | 2,00 | 2,00 | 2,00 | 2,00 | 2 öldürme (birikmiş XP kadar) |

- **Önerim [Muhtemel]:** üçüncü satır. Erken seviyede de hissedilir, Lv20+ mevcutla aynı ölçekte; `VERTICAL_SLICE_LV1_30.md` kabul ölçütü (1–3 öldürme eşdeğeri) ile uyumlu. Seviye/eşya kaybı yok.
- **Değişecek dosya:** `Source/MyProject/Private/Components/MMOStatsComponent.cpp:384-396` (`ApplyDeathPenalty`), gerekirse `Public/Components/MMOStatsComponent.h:191` (`DeathXPPenaltyPct`). İsteğe bağlı: `MMOCharacter.h:128` `RespawnDelay`.
- **Gereken veri:** `death` olayları (`xploss`, `by`, `lv`) ve `kill` sayısıyla **ölüm/öldürme oranı**. Hedef `[PLACEHOLDER]` 1 ölüm / 30–80 öldürme. 20 dk koşuda hiç ölüm yoksa önce mob tehdidi (savunma formülü) ele alınmalı; ceza tek başına anlamsız kalır.

## 3. Altın sink'i / `Q_Gold*` yeniden tasarımı

> **UYGULANDI (2026-10-09, öneri (a)+(b) + satıcı kapısı; değerler placeholder, PIE'de denenmedi):**
> (a) `Q_Gold5000/15000/30000/50000` artık teslimde altını **bağışlatır** (düşer; `MMOQuestComponent::GoldDonationMinTarget=5000`; `Q_Gold500/1000` erken altın kıtlığı yüzünden birikim kaldı).
> (b) Yükseltme maliyeti eşya seviyesiyle ölçeklenir: `50 × (+N) × (100 + Lv²) / 100` (`FMMOItemDatabase::GetUpgradeCost(Plus, RequiredLevel)`; Lv1 ≈ eski, Lv30 10×, Lv78 ≈ 62×).
> Satıcı: alıcı seviyesi + 3 ≥ eşyanın `RequiredLevel`'i (`UMMOInventoryComponent::VendorLevelSlack`), aksi halde "çok erken" reddi.
> Yeni Lv78 **Taht seti** eklendi (`ThroneBlade/Plate/Helm/Boots`; Naib elit havuzu artık yalnız bunu düşürür, `Q_AbyssRegent` ödülü `ThroneBlade`).
> Model (`EconomyModel.py`, bağış + ölçekli maliyet): Lv1→30 gelir/gider **12,4× → 6,8×** (+3 yükseltmede), +7'de **2,2×**; Lv78 kademesi +7'de 2,5×. Hâlâ erken kıt (Lv1–9: 0,4–1,1×) ve +3'te orta/yüksek seviyede 10–33× fazla: gerçek sink oyuncunun yüksek `+` peşinde olmasına bağlı.
> Aşağıdaki bölüm uygulama öncesi analizdir.

**Neden önemli:** Lv1→30 model geliri **~276.168 altın**, Lv≤30 kademe giderinin (4 parça + her biri +3) **11,3 katı** **[Kesin, model]** (yan öldürme tabanı kapalıyken ~269.883, 11,0x). Lv1–9 kıt (0,5–1,4x), Lv16'dan sonra 16–224x. Yükseltme `50 × (artı+1)` seviyeden bağımsız (`MMOItemDatabase.cpp:129-133`); şifacı sabit 100 (`MMOHealerNPC.h:28`), iksir sabit 20 (`MMOItemDatabase.h:36`) **[Kesin]**. `Q_Gold*` altını yalnız **taşımayı** ister, harcatmaz.

| Seçenek | Ne değişir | Lv1→30 gelir/gider (model) |
|---|---|---:|
| Mevcut (+3) | — | 11,3x |
| Daha yüksek yükseltme hedefi (oyuncu davranışı, kod değişmez) | +5 / +7 / +9 | 7,4x / 4,9x / 3,4x |
| (a) `Q_Gold*` teslimde altını düşsün | Lv≤30'da açılan 5 görev: 500+1.000+5.000+15.000+30.000 = 51.500 harcama | ≈3,6x (276.168 ÷ (24.488 + 51.500); elle hesap) |
| (b) Yükseltme/şifacı/iksir maliyeti seviyeyle ölçeklensin | Formül kararı gerekir | Modellenmedi |

- **Güncel sayılar (2026-10-06, `EconomyModel.py`, yeni içerik sonrası):** Lv10–15 kademesi gelir/gider **5,2x** (18.806 / 3.648; plan 4,1x ≈ 15.100 demişti; fark yeni üç mobun altınından, +3.685). Lv16–24 19,7x (91.494). Lv1→30 toplam gelir ~303.578, oran **12,4x** (yukarıdaki 276.168 / 11,3x yeni içerikten ÖNCEKİ değerdi). Karar değişmedi, yalnız sayılar güncellendi.
- **Önerim [Muhtemel]:** (a) + (b) birlikte. (a) tek başına oranı ~3,6x'e indiriyor ama hedef (her bant 0,5–1,5x) için yetmiyor; Lv16+ açığı mob altınının ~×1,2/seviye büyümesinden geliyor, bunu ancak seviyeyle ölçeklenen bir sink kapatır. `Q_Gold*` görevleri altın harcatırsa XP ödülü artık "bedava" değil.
- **Değişecek dosya:** (a) `Source/MyProject/Private/Components/MMOQuestComponent.cpp:136,152` (`HaveGold` tamamlama/teslim: teslimde `Inventory` altını düş) ve `Quests/MMOQuestTypes.cpp:295-399` (görev metni "biriktir" → "bağışla"). (b) `Items/MMOItemDatabase.cpp:129-133` (`GetUpgradeCost`), `Public/World/MMOHealerNPC.h:28`, `Public/Items/MMOItemDatabase.h:36`. `EconomyModel.py` (b)'yi ancak formül seçilince modelleyebilir.
- **Gereken veri:** `gold` olayları kaynağa göre (`loot`, `gorev`, `satis`, `satin_alma`, `yukseltme`, `iyilestirme`) ve Lv başına cüzdan bakiyesi. Model loot satışını ve iksiri hiç saymıyor; gerçek oran daha yüksek ya da düşük olabilir.

## 4. Kimlik/hesap sistemi

**Neden önemli:** Kayıt yuvası oyuncunun seçtiği ada bağlı (SECURITY_AUDIT K-1, KRİTİK/YÜKSEK). PR #1'de kısmi hafifletme: çevrimiçi oyuncunun adıyla bağlanma reddi + isteğe bağlı `-MMOPassword=` (`MMOGameMode.cpp:194-205`, **derlenmedi**). Açık kalan: çevrimdışı birinin adını `?Name=` ile alıp kaydını yüklemek **[Kesin, belge]**.

| Seçenek | Kapsadığı risk | Maliyet / süre |
|---|---|---|
| (a) Parola + yalnız tanıdık oyuncular (mevcut kod) | Yabancıyı dışarıda tutar; tanıdıklar arası ad kapma açık | Sıfır kod; derleme bekleyen PR #1 |
| (b) Sunucu üretimli 128 bit karakter anahtarı, yuva = hash (SECURITY_AUDIT K-1 önerisi) | Ad kapmayı kapatır; hesap yok | Küçük-orta, istemci yerel dosya |
| (c) PlayFab, `ACCOUNT_BACKEND_COMPARISON.md` Aşama 1–2 | Gerçek hesap, parola güvenliği, bulut kayıt | Büyük; dedicated server, ücretsiz katman 1.000 ömür boyu oyuncu (belgedeki fiyat [?] işaretli) |

- **Önerim [Muhtemel]:** İlk playtest için **(a) yeterli**: host sen, oyuncular tanıdık, oturum herkese açık değil. Kayıtları test öncesi yedekle. Yabancıya açmadan önce en az (b); hesap sistemi (c) içerik doğrulandıktan sonra.
- **Değişecek dosya:** (a) yok (host `-MMOPassword=` ile başlatır). (b) `Characters/MMOCharacter.cpp` (`GetSaveSlotName`, `ServerSetName`), `Core/MMOGameMode.cpp` (`PreLogin`); `MMOSaveGame.h`'a dokunursa kayıt uyumu kırılır (CheckPrPaths hata verir). (c) yeni `IMMOCharacterStore` arayüzü.
- **Gereken veri:** ölçüm değil, **karar girdisi**: test kaç kişiyle, kimlerle, internete açık mı? `[MMOMETRIC]` bunu ölçmez.

## 5. Lv28–33 beceri boşluğu

> **GÜNCEL DURUM (2026-10-09): seçenek (c) kodda ZATEN UYGULANDI** (commit `56de087`; her sınıfta Lv31 ve Lv39 becerisi `MMOSkillDatabase.cpp:101-290` civarında). Bu bölümün aşağısı o commit öncesi analizdir; satır numaraları ve "8 satır ekle" önerisi bayat. Kalan açık konu 33→39 ve 39→45 (6'şar seviye) uyarısı ile yeni becerilerin dengesi/zehir/mana bulguları: bkz. `SKILL_PLAN_LV31_45.md`.

**Neden önemli:** `ValidateData.py` dört sınıfta da "beceri boslugu Lv28 -> Lv33 (Lv29-32 arasi yeni beceri yok, 5 seviye)" UYARI basıyor **[Kesin]**. Lv28 ve Lv33 becerileri: `MMOSkillDatabase.cpp:99,101` (Savaşçı), `150,152` (Okçu), `201,203` (Büyücü), `270,272` (Rahip). Kod okununca daha büyük boşluk çıktı: dört sınıfta da Lv33'ten sonraki ilk beceri **Lv45** (12 seviye) **[Kesin]**; `ValidateData.py` artık bunu uyarıyor: denetim tüm seviye aralığına (Lv1 → tablodaki en yüksek beceri seviyesi) genişletildi, eşik `SKILL_GAP_LEVELS = 5` (fark ≥ 5), yalnız UYARI (CI kırmaz). Dört sınıfta da uyarılan aralıklar: 28→33 (5), 33→45 (12), 45→58 (13), 58→70 (12), 70→80 (10) **[Kesin]**. Lv30'da 49–51 harcanmamış beceri puanı birikiyor (`VERTICAL_SLICE_LV1_30.md`).

Ödülsüz aralığın uzunluğu (görevli model öldürme toplamı, mevcut eğri) **[Kesin, model]**: Lv28→33 ≈ 96 öldürme, Lv33→45 ≈ 245 öldürme.

| Seçenek | Etki | Risk |
|---|---|---|
| (a) Bırak | 28→33 (≈96 öldürme) ve 33→45 (≈245 öldürme) yeni becerisiz | En uzun ödülsüz aralıklar |
| (b) Lv33 becerilerini Lv31'e çek | 4 satırda tek sayı; uyarı kaybolur | 31→45 boşluğu 14 seviyeye **büyür**; sorunu taşır, çözmez |
| (c) Her sınıfa Lv31 ve Lv39 civarı birer yeni beceri (8 satır) | İki aralık 3–6 seviyeye iner | Değerler `[PLACEHOLDER]`; puan fazlası artar |

- **Önerim [Tahmin]:** (c), ama önce 33→45 boşluğunun kabul edilip edilmediğine karar ver; yalnız 28→33'ü kapatmak (b) yanlış hedef. `ValidateData.py` kontrolü Lv80'e genişletildi (yapıldı); (c) sonrası 33→39 ve 39→45 (6'şar seviye) hâlâ uyarı verir.
- **Değişecek dosya:** `Source/MyProject/Private/Skills/MMOSkillDatabase.cpp` (her sınıf bloğuna `MakeDef` satırı, mevcut kalıp; Savaşçı 99–101, Okçu 150–152, Büyücü 201–203, Rahip 270–272 civarı). (b) seçilirse yalnız satır 101, 152, 203, 272'de seviye alanı.
- **Gereken veri:** `levelup` olaylarından Lv28→33 ve Lv33→45 süresi (dk). Beceri kullanımı için `[MMOMETRIC]` olayı **yok**; "oyuncu sıkılıyor mu" ancak test notuyla (runbook "Genel his") anlaşılır.

## 6. Hız hedefi: Lv30 kaç saat

**Neden önemli:** Bütün XP/ekonomi ayarı bu hedefe göre kalibre ediliyor; hedef yok. `VERTICAL_SLICE_LV1_30.md` üç seçenek veriyor ve B'yi varsayıyor.

Süre = model öldürme sayısı × **20–30 sn/öldürme** (yürüyüş, mana, ganimet, teslim dahil; **ölçülmemiş varsayım**):

| Seçenek | Gerekli öldürme (Lv1→30) | Bu sayılarla süre [Tahmin] |
|---|---:|---|
| Mevcut eğri | ~241 | 80–121 dk |
| Eğri B | ~289 | 96–145 dk |
| Eğri C | ~323 | 108–162 dk |
| Hedef "KO tarzı" 8+ saat | 960–1.440 gerekir (8 sa ÷ 30/20 sn) | Bugünkü içerikle mümkün değil; yeni bölge/görev ister |

- **Önerim [Tahmin]:** **2–3 saat** (VERTICAL_SLICE seçenek B). Mevcut içerik buna yakın; 8+ saat içerik üretimi gerektirir. Kabul ölçütü zaten yazılı: Lv5 ≥ 6 dk, Lv10 ≥ 18 dk, Lv20 ≥ 55 dk, Lv30 ≥ 110 dk (`VERTICAL_SLICE_LV1_30.md` madde 4).
- **Değişecek dosya:** karar belgede (`VERTICAL_SLICE_LV1_30.md` §1.5 hedef bantları); uygulama karar 1 ile aynı dosya (`MMOXPTuning.cpp`).
- **Gereken veri:** **gerçek saniye/öldürme**: `kill` olayları arası `t` farkı ve `levelup` zamanları. Tablodaki tek bilinmeyen bu; 20–30 sn yanlışsa bütün süreler orantılı kayar.

## 7. `CheckPrPaths` yalnız `claude/*` dallarına mı sınırlansın

**Neden önemli:** `.github/workflows/checks.yml` son adımı her PR'da `Scripts/CheckPrPaths.py` çalıştırıyor; `Content/`, `Config/`, `MMOSaveGame.h` değişikliği **hata** **[Kesin]**. Yerelde bilerek yapılan değişiklik için PR'a `protected-paths-ok` etiketi gerekiyor. Not: `CLOUD_TASKS.md` bulut dallarını `cloud/<konu>` diye tanımlıyor, gerçek dallar `claude/*`.

| Seçenek | Etki |
|---|---|
| (a) Tüm PR'larda kalsın (mevcut) | Yerel PR'lar etiket ister; kayıt biçimi kazara değişirse her durumda yakalanır |
| (b) Yalnız `claude/*` (ve `cloud/*`) dallarında | Yerel dallarda sürtünme yok; yerel kazara `MMOSaveGame.h` değişikliği yakalanmaz |
| (c) Bulut dışı dallarda hata → uyarı | Ara yol; uyarı kolay gözden kaçar |

- **Önerim [Muhtemel]:** (a). Etiket bir tıklık; `MMOSaveGame.h` koruması (eski `.sav` uyumu) yerelde de değerli. Sürtünme gerçekten can sıkıyorsa (c).
- **Değişecek dosya:** (b)/(c) `.github/workflows/checks.yml` son adımın `if:` koşulu (`startsWith(github.head_ref, 'claude/')`) ya da `ALLOW_PROTECTED_PATHS` ifadesi; `Scripts/CheckPrPaths.py` docstring; `docs/CLOUD_TASKS.md` dal adı.
- **Gereken veri:** ölçüm değil; son PR'larda kaç kez etiket gerektiği (GitHub PR geçmişi).

---

## Bilinen küçük açık işler

Kod okunarak bulundu, PIE'de görülmedi. Satırlar `6d5e1d0` içindir.

| Sorun | Yer | Ne oluyor | Etki |
|---|---|---|---|
| **ÇÖZÜLDÜ (commit)** Yığın 999'u aşıyor, yüklemede kırpılıyor (eşya kaybı) | `Private/Components/MMOInventoryComponent.cpp:89-111` (`AddItem`, tavan `MAX_int32`, satır 100-101) ↔ `:841` (`SaveStackCap = 999`), `:853-861` (`ApplySaveData` kırpma) | Çalışma zamanında yığın sınırsız büyür (iksir alımı satır 546/581'de **istek başına** 999'a kırpılır, toplam değil); kaydet-yükle sonrası 999'a iner | Kalıcı eşya kaybı; ya `AddItem` 999'da durmalı ya yükleme tavanı kalkmalı |
| **ÇÖZÜLDÜ (commit)** Beceri reddinde istemci bekleme süresi geri alınmıyor | `Private/Components/MMOSkillComponent.cpp:597-607` (istemci iyimser `CooldownEndTimes.Add`) ↔ red yolları `:654-659` (sunucu cooldown, sessiz `return`), `:617-622` (hedef yok), `:625-628` (mana yetersiz) | Sunucu reddedince istemciye geri alma RPC'si yok | Uzak istemcide beceri boşa "bekleme"de görünür, tam süre kullanılamaz |
| **ÇÖZÜLDÜ (commit)** Uzak istemciye "Kaydedildi" / "VURDUN" / "+N altın" mesajı gitmiyor (VURDUN yalnız öldürücü vuruşta uzak istemciye gider: her vuruş sohbet spam'i olurdu) | `Private/Characters/MMOCharacter.cpp:541-544` (`IsLocallyControlled` + `GEngine`), `:1052` + `:1128-1136` (`bLocalFeedback`, `ServerAttackTarget` yolunda sunucuda false), `Private/World/MMOLootPickup.cpp:250-255` | Mesajlar sunucuda yalnız yerel oyuncu için basılıyor; `MMONotify::ToOwner` kullanılmıyor | Yalnız geri bildirim; oynanış doğru, uzak oyuncu sonucu görmez |
| Depo 24 sınırı aşılabiliyor | `Public/Components/MMOInventoryComponent.h:156` (`MaxWarehouseSize = 24`); çalışma zamanı denetimi `MMOInventoryComponent.cpp:727` var; `ApplySaveData` `:902` (`Warehouse = In.Warehouse`, tavan yok), `:929-936` ve `:953-959` (taşan çanta/yanlış slot eşyası depoya kapasite üstü eklenir) | Yalnız yükleme yolunda; "eşya silinmez" kuralı bilerek böyle | Depo >24 kalır; yeni depolama reddedilir ama UI'nın >24 satırı çizip çizmediği doğrulanmadı. **Güncelleme (commit):** HUD satır yüksekliği listeye göre küçülüyor, >24 satır panel içinde kalıp tıklanabilir (PIE'de görülmedi) |

---

## Yeni karar adaylari (QA denetimi, 2026-10-05; tasarim/denge, KOD DEGISTIRILMEDI)

- **Katki esigi:** 1 puanlik vurus bile gorev kill kredisi + en az 1 XP + 10 sn ganimet ortakligi verir (`AMMOMob::HandleDeath`, `LootOwners`); dusuk seviyeli "etiketleyici" ganimet yarisi acabilir. Minimum hasar payi (orn. %5) istenir mi? (VERTICAL_SLICE madde 10 kasitli esiksiz.)
- **Parti XP:** bolen artik menzildeki canli uye sayisi (bu turda tutarlilik duzeltmesi yapildi). Karar: +%10/ekstra uye bonusu 8 kisilik partide yeterli mi; uzakta duran uye bonusa sayilmiyor (kasitli mi kalsin)?
- **`Pri_Mending` (Lv20) / `Pri_HolyRain` (Lv16):** Mending 120 mana/14 sn/520 guc, HolyRain 110/16/380; Mending bekleme ve guc olarak iki komsuyu da geciyor (yorum "dogrusal ara deger" der). PLACEHOLDER; olcumle dengelenmeli.
- **Lv39 beceri dengesi (QA_ROUND_2026-10-06_NIGHT2 DUSUK-4; karar adayi, KOD DEGISTIRILMEDI):** `Mag_Starfall` (540 guc, 17 sn, 170 mana; guc/sn 31,8, guc/mana 3,18) komsusu `Mag_Disintegrate` Lv28'in (35,0 / 3,50) gerisinde; `Arc_Impaler` (470, 16 sn, 160; 29,4 / 2,94) `Arc_DeathMark` Lv28'in (28,7 / 3,44, Zehir) mana verimi gerisinde ve Zehir yok; `Pri_SalvationLight` (1100, 30 sn, 260; 36,7 / 4,23) `Pri_Mending` Lv20'nin (37,1 / 4,33) gerisinde. Secenekler: Starfall CD 17->14 ya da guc 540->600; Impaler'a Zehir/Yavaslatma ya da CD 16->14; SalvationLight icin guc/CD/mana ince ayari. Olcum (kullanim <%2, metrik M12) gelmeden dokunulmamasi da gecerli bir secim.
- **Cozulme Yolu konumu / Lore celiskisi (QA NIGHT2 ORTA-1; karar adayi, Lore DEGISTIRILMEDI):** Cep `(-5000,-3000)`ten `(-1875,5125,-114)` (hub'un kuzeyi, ~5280 u) tasindi, cunku hub'dan duz hat Obsidyen (Lv25-29) cebinin icinden geciyordu. `Lore.md` "Cozulme Yolu"nu "Buzulun dogu ucundan Alev Vadisi'nin kapisina inen dar serit" olarak anlatiyor ve `Q_ThawStalkers` vb. `Q_FrostLich` sonrasi acilan Lv13-15 icerigi; harita ise Alev Vadisi (Dorduncu bolge) hub'un guneybatisinda (`-1466,-1347`), yeni cep ona 6485 u, Buz Lordu'ne (-2466,1252) ~3900 u kuzeyde. Yani "Alev Vadisi kapisi / dogu ucu" anlatisi yeni konumda gorsel/cografi olarak tutmuyor (eski konum da tutmuyordu, 3901 u). Secenekler: (a) Lore'u "buzulun kuzey ucu / Ayazdis yamaclari" diye guncelle (Q_FrostWraiths "kuzeyden esen ruzgar" ile uyumlu); (b) cep konumunu oldugu gibi birak, anlatiyi belirsiz tut; (c) NPC/gorev metnine yon tarifi ekle. Ek: rota acikligi hedefi (>= 1500 u) haritada saglanamadi; en iyi 1375 u (Alacakaranlik Hukumdari, fark etme zarfi ~1200 u disinda), rota Buz Lordu'ne (Lv13 elit, aggro 1300) 943 u yaklasiyor: kabul edilebilir mi, yoksa Buz Lordu aggro/konumu mu ayarlansin?
- **Altin tavani:** kayit yukleme `SaveGoldCap=999999999`, calisma zamani `AddGold` `MAX_int32` (~2.1 milyar). Ikisi ayni sabit mi olsun, tavan kac olsun? (Mevcut ekonomide ~270k ulasilamaz; yalniz tutarlilik.)

## Yeni karar adaylari (2026-10-09: Lv46-80 icerik ve beceri dolgusu; KOD kararla degismedi)

- **Zehir/DoT olcekleme:** `Poison` 8/sn x 8 sn, savunmaya karsi fiilen 1 hasar. A secenegi (savunma atlayan + `taban + AttackPower x 0,10`) YALNIZ oyuncu -> mob icin UYGULANDI (2026-10-09, `TestDotSourceLifecycle` 36 kontrol); mob -> oyuncu zehri bilerek eskisi gibi (etkisiz) kaldi. Karar: mob -> oyuncu yonu de acilsin mi, `k` degeri ne olsun? Olcum ister: `DOT_SCALING_DESIGN.md`.
- **Saticida seviye kapisi ve Lv73-80 ekipman (Taht seti):** `BuyEquipment` seviye sartini yalniz kusanmada denetler; yeni set/elit/gorev odulu altinla atlanabilir (CONTENT_PLAN_LV46_80.md G6, S3). Karar madde 3 ile birlikte; Lv73-80'e yeni ekipman, kapi karari verilene kadar eklenmedi.
- **Kul mobu efekti (Burn):** Lore "kor gibi yakar" der, kod yavaslatma/zehir verir; yakma efekti DoT sorunu cozulmeden anlamsiz kalir, eklenmedi. Lore'u kodla mi, kodu Lore'la mi uyumlu tutalim?
- **Lv46-80 yeni icerigin sayilari:** mob/gorev/beceri degerlerinin hepsi PLACEHOLDER; ilk oyun testi `PLAYTEST_LV46_80.md`. Yan gorev XP'leri bilerek ~%6-7 esikte (pace-notr); mevcut Lv61+ grind gorevleri ~%40-50 verir. Yeni yan gorevler de o kalibi mi izlesin?
- **Zayif cep:** Uçurum Gezgini cebi (`-5500,4500`) ilk doldurmada 1-3/6 veriyor (4 kosu); spawner 4 sn'de bir yeniden dener, bos kalmaz ama gec dolar. Tasimak icin uygun aday bulunamadi (kuzeybati sinirli).
