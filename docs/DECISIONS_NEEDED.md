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
| 3 | Altın sink'i / `Q_Gold*` | (a)+(b) uygulandı; 2026-10-17 sink senaryoları: küçük dilim **P1 = şifacı küp + Lv42+ bağış satırları** (EK ~%25 fazla [Hesap/Tahmin]) + `+N` ölçümü; ayrıntı "Karar 3 sink senaryoları" | [Muhtemel] (model) | Kısmen (+N davranışı ölçülmeden açığın büyüklüğü %5-145 arası belirsiz) |
| 4 | Kimlik/hesap | İlk playtest için parola + tanıdık oyuncu yeter | [Muhtemel] | Evet |
| 5 | Lv28–33 beceri boşluğu (asıl boşluk Lv33→45) | **UYGULANDI (`56de087`): Lv31 + Lv39 becerileri kodda**; denge/zehir/mana bulguları `SKILL_PLAN_LV31_45.md` | [Tahmin] | Evet (tasarım); değerler ölçümle |
| 6 | Hız hedefi (Lv30 kaç saat) | 2–3 saat (B) | [Tahmin] | Evet (hedef); tutup tutmadığı ölçümle |
| 7 | `CheckPrPaths` yalnız `claude/*` dallarına mı | Hayır, tüm PR'larda kalsın | [Muhtemel] | Evet |
| 8 | Ekipman birleştirme ücreti (DESIGN_FUSION A3; ayrıntı aşağıda "Ekipman birlestirme A1-A4" → "A3 modeli") | `{0,5; 1; 2}` × alış fiyatı değişmesin; yüksek seviye sink'i karar 3'te | [Muhtemel] (model) | Kısmen (arbitraj yok kesin-hesap; kullanım için `ev=fusion` gerekli) |

---

## 1. Lv1–5 hızlı başlangıç korunsun mu

> **Model denemesi (2026-10-10, kod değişmedi):** Lv16–20 "görevli" bandı hedefin altında hızlı (5,9 öldürme/seviye; hedef 11–15; görevsiz 16,8).
> `ProgressionModel.py --quest-mult` ile görev XP çarpanını bu aralıkta düşürmek (16→0,04, 20→0,08) bandı yalnız **8,0**'a çıkarıyor ve Lv21–25'i
> hedefin üstüne (18,6 > 18) itiyor; tek çarpan ayarıyla iki bant birden hedefe girmiyor. Bu yüzden değişiklik yapılmadı: Lv16–20 bilinçli "görev
> bandı" kabul edilirse sorun yok, düzeltilecekse Lv16–20'ye öldürme görevi eklemek/ham ödülleri tek tek düşürmek gerekir (ölçüm sonrası).
> Aynı gün eklenen Lv21/23 yan görevleri Lv21–25'i 20,3 → 18,0 ile hedefe soktu.

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
> Yeni Lv78 **Taht seti** eklendi (`ThroneBlade/Plate/Helm/Boots`; Naib elit ganimeti artık bunu düşürür, `Q_AbyssRegent` ödülü `ThroneBlade`; set satıcıdan **çıkarıldı** (FMMOItemDef::bVendorSold=false; sunucu reddeder, satıcı penceresi listelemez); yine de Lv75+ normal mobdan düşer ve takas/depo seviye denetlemez; kuşanma seviyeyi tam denetler, güç istismarı yok).
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

> **Yeni kalemler modeli (2026-10-11, `python -I Scripts/EconomyModel.py`, çıktının sonundaki "Yeni kalemler" bölümü; üstteki eski tablo değişmedi; kod değişmedi, PIE'de denenmedi):**
> Katsayılar koddan okunuyor **[Kesin]**: şifacı `100 + (L−1)²·2/5`; günlük 3 görev, altın `15 × L × (1/1/2)` = **60 × L/gün**, XP eşiğin `%2 × (1/1/2)` = **%8/gün**; posta `10 + ceil(%2 × ek)` (tavan 1.000.000); başarım 28 satır, toplam **65.050** altın, XP yok.
> Varsayımlar **[Tahmin]**: 25 sn/öldürme, 90 sn/görev (Lv1→80 ~10,9 sa = 2 sa/gün'de ~5,5 gün); 1–3 sa/gün oyun, üç günlük görevin hepsi biter; 2–6 ölüm/sa, ölüm başına bir şifa.
>
> | Kalem | Model sonucu **[Hesap]** | Bayrak | Öneri **[Tahmin]**, kod değişmedi |
> |---|---|---|---|
> | Günlük altın (kaynak) | Set altını bandın 1 saatlik gelirinin %0–2'si; bant gelirine payı 1–3 sa/gün'de %0,1–2,2 (Lv1–15 en yüksek, Lv42+ %0,1–0,4) | Tüm bantlar "etkisiz" (eşik: 1 saatlik gelirin <%10'u) | **Değişmesin.** Ekonomi zaten 4–40x fazlada; artırmak enflasyon. Lv1–9 kıtlığı için gerekirse yalnız erken bant (ör. L≤10 için ×3) düşünülebilir |
> | Günlük XP (kaynak) | Set eşiğin %6,6–8,0'i; bant eşik toplamına payı 1–3 sa/gün'de %0,1–1,5 (@2 sa/gün %0,2–0,7) | Tüm bantlar "etkisiz" (eşik: <%2) | %2 → **%5–10** görev başı: @2 sa/gün bant payı %0,5–1,9 (%5) / %1,0–3,7 (%10); ilerleme hızını bozmuyor. Karar 6 eğriyi yavaşlatırsa pay orantılı artar; o zaman %2 kalabilir |
> | Başarım altını (kaynak, tek sefer) | Lv1→80 model gelirinin %0,67'si; bant payı Lv5 %20,3, Lv1–2 %6,0, Lv10–15 %5,3, Lv16+ ≤%1,7. Leveling'de açılmayan: `Ach_Kill2000`, `Ach_Kill10000` (model Lv80'e ~1.245 öldürme), `Ach_AbyssRegent` (Lv80 boss) = 23.000 altın endgame'e kalıyor | Enflasyon riski yok | Değişmesin. Erken kıtlığa yardım Lv5'te yoğun; istenirse `Ach_Kill10`/`Ach_Lv10` biraz artırılabilir |
> | Posta ücreti (sink) | 100 altın ekte %12; 1.000'de %3,0; ≥10.000'de ~%2,0. Bir bandın 1 saatlik gelirini postalamak %2,0–2,3 | Hacim bilinmiyor | Değişmesin; sink değil spam/kaçak transfer freni. Hacim ölçülmeden katsayı anlamsız |
> | Şifacı (sink) | Lv5–79 saatlik gelirin <%5'i (2–6 ölüm/sa); ayrıntı ve küp önerisi "Yeni karar adaylari (2026-10-09)" şifacı satırında | Lv5–79 YETERSİZ | `0,09 × (L−1)³` |
>
> **Net etki [Hesap]:** (gelir + günlük + başarım − şifacı) / kademe gideri, eski orana göre en fazla ±0,3x oynuyor (orta senaryo 2 sa/gün + 4 ölüm/sa: Lv1–2 0,5→0,5, Lv10–15 3,9→4,1, Lv42–50 31,2→31,2, Lv72–79 40,4→40,5). Yeni kalemler mevcut haliyle ne fazlayı kapatıyor ne kıtlığı açıyor; asıl kaldıraç hâlâ yükseltme hedefi (+N) ve seviyeyle ölçeklenen sink'ler.
>
> **Gereken `[MMOMETRIC]`:** `ev=gold src=gunluk`, `src=basarim`, `src=iyilestirme`, `src=posta` / `src=posta-iade` toplamları; oturum süresi/gün; `death`/sa. Posta ücreti ayrı satır değil: gönderimde `src=posta` eki + ücreti birlikte düşer, alıcı eki `src=posta` ile alır; ücret = −Σ(`posta`) − Σ(`posta-iade`) (tüm oyuncular, toplanmamış posta hariç). Ayrı `src=posta_ucret` ve günlük görev XP'si için `ev=xp src=gunluk` olayı yok: ölçüm için mmo-developer eklemeli.

### Karar 3 sink senaryoları (2026-10-17, `[MODEL]`, kod değişmedi, PIE'de denenmedi)

Kaynak: `python -I Scripts/EconomyModel.py --sink-scenarios` (D0 birleştirme dahil; formüller koddan okunur; varsayımlar `SINK_*` **[Tahmin]**). Ayrıntı: `BALANCE_MODEL.md` "Altın sink senaryoları".

**Mevcut sink ve açık [Hesap]** (4 parçalı tek kademe/bant, gelir brüt = ana tablodaki gelir + Q_Gold bağışı):

| Bant | Gelir | Gider +3 (ana tablo) | EV +3 | EV +5 | EV +7 | Gelir/sink ALT / ORTA / ÜST |
|---|---:|---:|---:|---:|---:|---|
| 16–24 | 87.057 | 7.728 | 9.079 | 28.918 | 230.662 | 5,8 / 1,3 / 0,3 |
| 25–32 | 215.456 | 13.400 | 16.149 | 55.298 | 448.253 | 4,1 / 1,8 / 0,4 |
| 33–41 | 457.885 | 20.176 | 24.686 | 87.931 | 718.652 | 6,5 / 2,6 / 0,5 |
| 42–50 | 923.161 | 29.600 | 36.671 | 134.745 | 1.108.081 | 22,4 / 4,4 / 0,7 |
| 51–57 | 1.076.615 | 40.960 | 51.206 | 192.280 | 1.587.885 | 19,3 / 3,9 / 0,6 |
| 58–65 | 1.718.699 | 51.300 | 64.441 | 244.716 | 2.025.210 | 24,7 / 5,0 / 0,7 |
| 66–71 | 2.128.032 | 64.592 | 81.497 | 312.650 | 2.592.345 | 24,6 / 5,0 / 0,7 |
| 72–79 | 3.059.719 | 75.776 | 95.822 | 369.490 | 3.066.539 | 30,1 / 6,2 / 0,9 |

EV = 4 parçanın alışı + beklenen yükseltme ücreti; **her deneme (başarılı/başarısız) ücret öder, yanan parça saticidan yeniden alınır** (`MMOInventoryComponent.cpp:837-874`, şans/yanma `MMOItemDatabase.cpp:151,165`). ALT = EV+3 + şifacı + iksir + bağış; ORTA = EV+5 + ... + birleştirme D; ÜST = EV+7 + ... + birleştirme S (üst sınır; Lv1–15'te ve ÜST'te bütçe yetmez).

- **Asıl bulgu [Hesap]:** açığın büyüklüğü oyuncunun +N hedefine bağlı ve bunu hiçbir veri söylemiyor. Lv16–79'da kodda olan sink gelirin ALT'ta **%5,2**'si, ORTA'da **%22,1**'i; +7'yi kovalamak bandın tüm gelirini yer (Lv72–79: 3,07 M / 3,06 M). Eski "11–40×" sayısı yalnız "+3, hiç başarısızlık yok" tabanıdır; ALT'ta gerçek oran 4,1–30,1×, ORTA'da 1,3–6,2×.
- İksir (Lv16–24'te gelirin %1,5'i, Lv42+'da <%0,2) ve şifacı (%0,15–0,6) açığı kapatmaz; birleştirme (D %6,9 / S %25,7) kısmen emer.
- Gelir kötümser: ganimet satışı yok (+%2 ile +%13 eklerdi). Gelir tarafının kendisi (ort. mob altını Lv1 16 → Lv79 10.188, ×640; ekipman fiyatı ×24) açığın kökü; sink'ler polinom, gelir üstel. Mob altınını düzleştirmek (`MMOMobArchetype.cpp`) bir gelir düşürme seçeneğidir, **modellenmedi**.

**Adaylar** (EK altın / bant fazlası = gelir − gider+3, Lv16–79 toplamı; birleştirme D %6,9 ile aynı payda):

| Aday | Etki | Oyuncu deneyimi riski | Kod etkisi | Dosya:satır ve çakışma |
|---|---:|---|---|---|
| **A** şifacı/iksir büyüsün: A1 küp `0,09×(Lv−1)³`; A2 `10×ort. mob altını` | A1 %2,5; A2 %8,3 (4 ölüm/sa, yarısı atlatılır; bypass 0 ise ×2) | Düşük-orta: ölüm cezası (karar 2) ile birlikte düşünülmeli; pahalı sifa iksire/ölüp doğmaya kaçırır | A1 tek satır (`(L*L*L*9)/100`, int64). A2 yeni seviye→altın yardımcısı | `MMOHealerNPC.h:36` (temiz; çağıran `MMOPlayerController.cpp:2728` formülden bağımsız, controller şu an commit'siz ama dokunulmaz). `EconomyModel.py` ComputeHealCost regex'i (`load_new_items`) küp için güncellenir. Potion: `MMOItemDatabase.h:37`, `MMOConsumableDatabase.cpp:34` |
| **B** Q_Gold bağış görevleri seviyeyle ölçeklensin (Lv42+ bantlarına bağış satırı; hedef = bandın 0,5–1 saatlik geliri) | B1 (0,5 sa, katılım %50) %22,3; B2 (1 sa) %45,5; katılım %100'de iki katı, %10'da beşte biri | Orta: isteğe bağlı olduğu için "vergi" hissi yok, ama katılım **[Tahmin]**; ödül (XP pace-nötr + unvan/iksir) cazip olmalı; 1,5 M altın taşımak gerekir (`Gold` int32 sınırı içinde) | Yalnız veri satırı (`HaveGold`, hedef ≥ 5000 zaten bağış, `MMOQuestComponent.cpp:662`); bileşen değişmez | `Quests/MMOQuestTypes.cpp:510-536` (Q_Gold5000..50000 zinciri). **Dosya `lv31-40-content` worktree'sinde commit'siz (+24 satır)**: sıra/çakışma Codex ile ayarlanmalı. Önerilen hedefler (B1): Lv42 310.000, Lv51 440.000, Lv58 710.000, Lv66 1.100.000, Lv72 1.500.000 |
| **C** yükseltme eğrisi yüksek seviyede dikleşsin (`× (1+(Lv−30)/30)^γ`) | γ=1: hedef +3 %3,2 / +5 %13,3 / +7 %111,8; γ=2: %9,9 / %41,7 / %351,5 | **Yüksek:** +7 kovalayanı cezalandırır (kumar zaten yanma içeriyor); talep esnekliği (oyuncunun +N'den vazgeçmesi) modellenmedi, gerçek etki daha düşük olabilir; +7 oynanamaz hale gelir | Tek formül satırı + 3 sabit assertion | `MMOItemDatabase.cpp:140-146` (+ yorum `MMOItemDatabase.h:21-23`), assertion `MMOInventoryComponent.cpp:216-218` (temiz); test `MMOUpgradeCallbackSelfTest.cpp:89` sembolik. **`MMOItemDatabase.cpp` `lv31-40-content` worktree'sinde commit'siz** (yalnız Lv33 satırları, farklı bölge; yine de aynı dosya). `EconomyModel.upgrade_cost` aynasını güncelle; `sink_report` formül değişirse kendiliğinden hata verir |
| **D** opsiyonel/kozmetik sink | D1 (Lv20/40/60'ta 3 satın alma, her biri 0,25 sa, katılım %50) %2,4 | Düşük; ama ürün yok | **Kodda hiçbiri yok:** isim değiştirme ücreti, depo/çanta genişletme (`MaxWarehouseSize` 24, `MaxBagSize` 12 sabit), beceri/stat sıfırlama, binek, kozmetik, onarım/dayanıklılık (grep boş). Bölge geçidi ücreti için alan var ama 0 | `MMOInventoryComponent.h:118,192` (`fusion-d1` ve `a3cc3` worktree'lerinde commit'siz); `MMOZoneLayout.h:119` `GoldCost` (Codex, izlenmeyen dosya). Yeni sistem = yeni dosya + `MMOSaveGame.h` yasağı + replikasyon: bu karar kapsamında önerilmez |
| **E** ilan/pazar vergisi | Modellenmedi | — | Pazar yok (`SIX_MAP_PLAN.md` S5/S12: Aşama 8, PostgreSQL); işlem hacmi olmadan oran hesaplanamaz | Not: pazar açılınca %2–5 işlem vergisi + ilan ücreti, posta ücreti (`%2`) emsal |

**Paketler (Lv16–79, gelir/sink ≤ 1,5 hedefi [Tahmin]; tabanlar: ALT hedef 0/8 bant, ORTA 1/8):**

| Paket | EK sink (toplam fazlanın %'si) | Hedefi tutan bant (ALT / ORTA) |
|---|---|---|
| P1 = A1 + B1 | %24,8 | 0/8 / 1/8 (gelir/sink: ALT 2,8–4,3, ORTA 1,1–2,4) |
| P2 = P1 + C1 | ALT %27,9; ORTA %38,0 | 0/8 / 1/8 |
| P3 = A2 + B2 + C2 + D1 | ALT %66,2; ORTA %98,0 | 4/8 / 8/8 |

**Karar sahibine seçenekler:**
- **A: Önce ölç, kod değiştirme.** Gerekçe: ALT ile ÜST arasındaki fark (%5 vs %145) +N davranışından geliyor; yanlış yöne düzeltme riski. Dezavantaj: ilk playtestte altın birikimi ölçümsüz kalır.
- **B: Küçük dilim şimdi (önerim [Tahmin]).** P1 = A1 + B1: iki bağımsız dosya (`MMOHealerNPC.h` tek satır, `MMOQuestTypes.cpp` ~5 veri satırı), yükseltme eğrisine dokunmaz. Model [Hesap]: Lv16–79 fazlasının ~%25'ini emer (A1 %2,5, B1 %22,3 katılım %50 **[Tahmin]** varsayımıyla; katılım %25'te B1 ≈ %11). ALT'ta gelir/sink 4,1–30,1× → 2,8–4,3×; hedef (≤1,5×) tutmaz, yani P1 açığı kapatmaz, yarıya yaklaştırır. Aynı anda `[MMOMETRIC]` ölçümü toplanır; C yalnız ölçüm "medyan hedef +N ≤ 4, yükseltme sink'i kullanılmıyor" derse açılır. A2 ve D şimdilik yok.
- **C: Tam paket (P3).** Hedefe ulaşır (ORTA 8/8) ama C2 +7 kovalayanı yok eder, D yeni sistemler ister ve her şey katılım/davranış varsayımıdır.
- **Gelir düşürme** (mob altınını düzleştirmek) ayrı seçenek, modellenmedi; kaynak sorununu çözer ama ekonomiyi ilk haftada kıtlaştırma riski (Lv1–9 zaten 0,4–1,1×).

**Gereken `[MMOMETRIC]` / veri:** (1) yükseltmede `ev=gold src=yukseltme` zaten var; **eksik:** `to_plus` / sonuç (success/failed/burned) `[MMOMETRIC]` satırı (şu an yalnız `LogMMOInventoryAudit event=item_upgrade`, `MMOInventoryComponent.cpp:877`), mmo-developer eklemeli. (2) Bant başına gold `src` toplamı: `Scripts/PlaytestMetrics.py` oyuncu başına `src` giriş/çıkış veriyor; seviye bandı için `levelup` zamanlarıyla kesmek gerekir (gold olayında `lv` yok). (3) `death`/sa ve `src=iyilestirme` (A için), Q_Gold teslim oranı (B katılımı: `gorev_bagis` src'si var), `src=satis` (ganimet satışı geliri). Karar kuralı **[Tahmin, eşikler keyfi]**: ölçülen medyan sink/gelir (Lv16–79 bantları) ≥ %60 ise yeni sink açılmaz; < %30 ise P1'e geç; arada A + ölçüm.

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

> **Güncel model (2026-10-09, `Scripts/TimelineModel.py`, ayrıntı `TIMELINE_MODEL.md`):** 25 sn/öldürme + 90 sn/görev ile Lv30 **~3,0 sa**
> (15–40 sn aralığında 2,2–4,3), Lv80 **~10,7 sa** (7,3–15,9). Lv30 hedefi tutuyor; **asıl yeni soru Lv80'in ~11 saat olması**:
> KO/Rise tarzı bir oyun için tüm içerik bir hafta sonunda tükenir. Hedef "kısa yoğun PvE" mi, yoksa Lv30+ eğrisi bilerek yavaşlatılsın mı?

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

- **Zehir/DoT olcekleme:** `Poison` 8/sn x 8 sn, savunmaya karsi fiilen 1 hasar. A secenegi (savunma atlayan + `taban + AttackPower x 0,10`) YALNIZ oyuncu -> mob icin UYGULANDI (2026-10-09, `TestDotSourceLifecycle` 38 kontrol; zayif oyuncu yenilemesi guclu DoT'u ezmez); mob -> oyuncu zehri bilerek eskisi gibi (etkisiz) kaldi. Karar: mob -> oyuncu yonu de acilsin mi, `k` degeri ne olsun? Olcum ister: `DOT_SCALING_DESIGN.md`.
- **Satici seviye kapisi ve Taht seti (UYGULANDI 2026-10-09, QA ORTA-2 sonrasi netlestirildi):** `BuyEquipment` alici seviyesi + 3 >= esya seviyesi ister; Lv78 Taht seti eklendi. Satici stogu sorusu icin varsayilan uygulandi: `FMMOItemDef::bVendorSold` (varsayilan true); Taht seti false, yani yalniz ganimet (Naib + Lv75+ normal mob) ve `Q_AbyssRegent` odulu. Diger tum setler eskisi gibi satilir. Daha fazla seti (orn. her kademenin 'Blade'i) satistan cikarmak istenirse yalniz `MMOItemDatabase.cpp`te ilgili satirin altina bayrak eklenir; ValidateData/EconomyModel bayragi otomatik okur.
- **Sifaci bedeli seviyeyle buyur (UYGULANDI 2026-10-11, denenmedi):** sabit 100 altin Lv58+ bantlarinda (EconomyModel gelir/gider ~30x) fiilen bedavaydi, "altin sink zayif / olum bedava" maddelerinin parcasi. `AMMOHealerNPC::ComputeHealCost(taban, seviye) = taban + 0,4 x (seviye-1)^2`: Lv1 100, Lv10 132, Lv30 436, Lv60 **1.492**, Lv80 **2.596** [Kesin, tamsayi formul `(L*L*2)/5`; onceki metinde ve `MMOHealerNPC.h:31` yorumunda 1.503 / 2.587 yaziyordu, yorum duzeltmesi mmo-developer'a] (`MMOPlayerController::ServerRequestHeal`). Yeterli altin yoksa bedel ipucu olarak gosterilir, odenince `Sifa: -N altin` bildirimi cikar. Katsayi (0,4) PLACEHOLDER; ayarlamak icin yalniz `MMOHealerNPC.h`. Olcum: `death`/`gold` olaylari (`[MMOMETRIC]`); olum sikligi x bedel, seviye basina gelirin ~%5-10'unu gecmemeli.
  - **Model sonucu (2026-10-11, `EconomyModel.py` "Yeni kalemler" (b)/(e); kod degismedi, PIE'de denenmedi):** varsayim [Tahmin]: 25 sn/oldurme, 90 sn/gorev (Lv1->80 ~10,9 sa), olum basina bir tam sifa, 2-6 olum/sa. Dogusta `Revive` cani/manayi doldurur, yani sifaci zorunlu degil: asagidaki yuzdeler **ust sinirdir**.
  - Sifaci / saatlik gelir, 2-6 olum/sa [Hesap]: Lv1-2 %5,8-17,3 · Lv3-4 %2,1-6,3 · Lv5-15 %0,9-3,8 · Lv16-24 %0,6-1,9 · Lv25-41 %0,4-1,4 · Lv42-79 %0,2-0,9. Hedef %5-10 yalniz Lv1-4'te tutuyor; **Lv5-79 YETERSIZ** (6 olum/sa'te bile <%5). Lv25+ bantlarinda bir sifa 0,2-0,3 oldurmenin altini. Kademe gelir/gider orani (+3) hicbir bantta 0,3x'ten fazla oynamiyor (Lv72-79 40,4 -> 40,4-40,6).
  - **Katsayi onerisi [Tahmin, kod degistirilmedi]:** kare yerine kup, `taban + 0,09 x (Lv-1)^3` (C++: `(L*L*L*9)/100`, int64; Lv10 165, Lv30 2.295, Lv60 18.584, Lv80 44.473). Model [Hesap]: 4 olum/sa'te Lv16-79 bantlari %3,9-6,2 (2-6 olum/sa: %2,0-9,3); Lv1-15 neredeyse degismez (altin orada kit, gelir/gider 0,4-1,1x). Denenen alternatifler: kare katsayi 2,0 (Lv80 12.582) Lv42-79'da @4 yalniz %1,5-2,7 (gelir ustel buyuyor, kare yetismiyor); `100 + 2 x seviyedeki ort. mob altini` Lv16-79 @4 %2,5-5,7 ama mob tablosuna baglanir. Daha yumusak secenek `0,06 x (Lv-1)^3` (Lv80 29.682): Lv16-79 @4 %2,8-4,1.
  - **Uyari [Muhtemel]:** iksir sabit 20 altin (`MMOItemDatabase.h:37`) ve dogus bedava; sifa pahalanirsa oyuncu iksire ya da olup-dogmaya kayar, sink gerceklesmez. Kup oneriyi iksir fiyatinin seviyeyle olceklenmesiyle (karar 3 (b)) birlikte ele al.
  - **Gereken `[MMOMETRIC]`:** bant basina `death` sayisi/sa; `ev=gold src=iyilestirme` adet ve toplam; `src=satin_alma` icindeki iksir payi; `src=loot` + `src=gorev` saatlik gelir. Karar olcutu: `iyilestirme` toplami / (`loot` + `gorev`) bant basina %5-10 araliginda mi.
- **Kul mobu efekti (Burn):** Lore "kor gibi yakar" der, kod yavaslatma/zehir verir; yakma efekti DoT sorunu cozulmeden anlamsiz kalir, eklenmedi. Lore'u kodla mi, kodu Lore'la mi uyumlu tutalim?
- **Lv46-80 yeni icerigin sayilari:** mob/gorev/beceri degerlerinin hepsi PLACEHOLDER; ilk oyun testi `PLAYTEST_LV46_80.md`. Yan gorev XP'leri bilerek ~%6-7 esikte (pace-notr); mevcut Lv61+ grind gorevleri ~%40-50 verir. Yeni yan gorevler de o kalibi mi izlesin?
- **Zayif cep (COZULDU, 2026-10-09):** Uçurum Gezgini cebi (`-5500,4500`) ilk doldurmada 1-3/6 veriyordu; neden `DontSpawnIfColliding` ile cakisan noktalar ve 4 sn'lik bekleme idi. `AMMOMobSpawner::TopUp` artik mob basina ayni turda 3'e kadar baska nokta dener: tum Winter cepleri 5-6/6, elitler ilk turda dogdu (mob sayisi ~55 -> 79).

## Yeni karar adayi (2026-10-11: guvenlik denetimi; denetim aninda kod degistirilmedi, sonra (a) uygulandi)

- **Ad degistirme kisiti (uzak istemci):** DURUM: secenek (a) GECICI ONLEM OLARAK UYGULANDI (2026-10-11, commit `afba349`, tur 3
  `2bb2023` ve tur 4 `d031b90` ile sikilastirildi; native test `Scripts/TestNameLock.ps1` 58 kontrol GECTI (`7932979`); guncel: tur 5
  (`f70e5ad`, `76ceed9`) sonrasi 82 kontrol GECTI (2026-10-14, `RunAllChecks` icinde), PIE/gercek
  uzak istemci YOK; `SECURITY_AUDIT.md` K-1 yeni yuz "Durum" satirlari). Kullanici karari hala ACIK: kayitli karakter icin ad
  degistirme oyun ozelligi olarak gerekli mi? Ayrica yeni karar: basarim dosyasi olmayan Lv>1 karakterde seviye/gorev/altin basarimlari
  odulsuz acik sayiliyor (`ACHIEVEMENTS.md`); sistem oncesi karakterlerin geriye donuk odul almamasi kabul mu?
  - **Sorun [Kesin, kod okumasi; PIE'da denenmedi]:** uzak istemci kayitli karakterinin adini kaydi olmayan yeni bir ada
    degistirip cikinca karakter yeni yuvaya tam envanterle yaziliyor, eski yuva silinmiyor: ayni karakter iki adda.
    Posta/ticaret/depo ile tek istemcide altin ve esya her turda ikiye katlanir. Ayrica basarim/gunluk dosyasi eski adda
    kaldigi icin yeni adla seviye/gorev basarimlari tekrar alinir (Lv80 + 90 gorev icin ~25.900 altin/dongu [Hesap]; bu
    kaynak `EconomyModel.py`'de yok, model basarimi tek seferlik sayiyor). Ayrinti ve dosya:satir (commit `43e87ca`):
    `SECURITY_AUDIT.md` K-1 "yeni yuz (2026-10-11)", F1-F5.
  - **Oneri [Muhtemel]:** uzak istemci yalniz **hic kaydedilmemis yeni karakterin** adini degistirebilsin (kosul: karakterin
    `ActiveSaveSlot` yuvasi diskte yok). Host ve yonetici muaf. Kayitli-ad reddi de 30 sn soguma baslatsin (ad sorgulamayi
    yavaslatir). Kalici cozum hesap modu (`ACCOUNT_IDENTITY_DESIGN.md` B-min; ara onlem §10).
  - **Etkisi:** kaydedilmis bir karakterin adi uzaktan degistirilemez; yeni karakter ilk kayda (60 sn otomatik kayit ya da
    `MMOSave`) kadar adini secebilir. Bugun diskte elle secilmis karakter adi yok (`ACCOUNT_IDENTITY_DESIGN.md` §2.4), yani
    mevcut veriye pratik etkisi yok; `Gezgin######` adiyla kaydedilen oyuncu adini degistiremez, yeni karakter acmasi gerekir.
    Host'un kendi `MMOSetName`'i ve self-test'ler degismez.
  - **Secenekler:** (a) oneri (yalniz yeni karakter); (b) uzak istemcide ad degistirmeyi tamamen kapat (en basit, yeni
    oyuncu da `?Name=` ile baglanmak zorunda); (c) ad degistirmeye izin ver ama karakteri ve yan dosyalari tasi, eski yuvayi
    sil (dosya tasima/silme hatasi = veri kaybi riski; B-full `CharacterId` olmadan onerilmez); (d) hesap moduna kadar bekle
    (catallama acik kalir; LAN'da tanidiklarla kabul edilebilir mi?).
  - **Kullanicinin vermesi gereken karar:** kayitli karakter icin ad degistirme oyun ozelligi olarak gerekli mi? "Hayir" ise
    (a) ya da (b) uygulanir (kucuk, sicak dosya `MMOCharacter.cpp`'de ayri commit). "Evet" ise kalici yol B-full
    (`ACCOUNT_IDENTITY_DESIGN.md` §4.7, karar 3) ve o gelene kadar yine (a) gecici onlem olarak onerilir.
  - **Gereken veri:** olcum degil, karar girdisi. Duzeltme sonrasi dogrulama: iki istemcili NetSmokeTest varyanti (kayitli
    karakterin uzak ad degisimi reddedilmeli; ad degistir + cik + yeni adla gir sonrasi basarim altini ikinci kez gelmemeli).
    Ilgili `[MMOMETRIC]`: bir karakterin `ev=gold src=basarim` toplami tablonun tek seferlik toplamini (65.050 altin,
    28 satir [Kesin, `EconomyModel.py` ciktisi]) asarsa ya da ayni basarim iki kez gorulurse istismar isaretidir.

## Yeni karar adayi (2026-10-11: guvenlik tur 4, Y-NEW-1)

- **Turkce harfli oyuncu/klan adi:** DURUM: adlar artik yalniz ASCII `[A-Za-z0-9 _-]` (tek yardimci `Public/Core/MMONameRules.h`;
  ad cakismasi kontrolleri kanonik anahtarla: bosluk -> `_`, ASCII kucuk harf). Neden: `FChar::IsAlnum` Turkce harfleri
  geciriyor, `ESearchCase::IgnoreCase` yalniz ASCII'de harf duyarsiz, NTFS ise Unicode'da da harf duyarsiz: buyuk ve kucuk
  O-umlautlu iki ad farkli sayilip AYNI `MMOSave_*.sav` dosyasina yaziliyordu (ayni kayda ikinci oturum). Bedeli: Turkce harfli ad
  secilemez (~~baglanti adindan harf atilir~~ tur 5'ten beri `PreLogin` reddeder / `ChangeName` `Gezgin######` verir, `MMOSetName` reddeder); ASCII disi eski kayit/posta kutusu icin migrasyon YOK (o adlar
  erisilemez kalir; eski Turkce harfli klan adlari yuklemede korunur, yenisi kurulamaz). Karar: Turkce harfli ad gerekli mi?
  "Evet" ise sonra ICU tabanli kanonik anahtar (tam Unicode case-fold + normalizasyon, dosya adi = anahtarin guvenli kodlamasi)
  ve eski kayitlar icin tek seferlik tasima.
- **Baglanti adinda izinsiz harf: kirpmak mi, reddetmek mi? [Kesin, kod okumasi; PIE'da denenmedi]:** bugun `?Name=` degerinden
  ASCII disi harfler sessizce atilir (`MMOGameMode.cpp:381-392` `SanitizeInitialName`): `?Name=Ömer` -> `mer`. Kalan ad bos
  degilse oyuncu o adin kaydini yukler (`mer` adli bir kayit varsa o karakter), yalniz 3 sn sonra "izinsiz karakterler atildi"
  bildirimi gelir (`:537`). Kimlik zaten ad oldugu icin (K-1) yeni bir yetki acmaz, ama kazara baska kayda girme ve kafa
  karisikligi riski var. Secenekler: (a) bugunku gibi kirp + bildir; (b) `PreLogin`'de ASCII disi harf iceren adi
  "Adda Turkce harf kullanilamaz" diye reddet (oyuncu dogru adla yeniden baglanir); (c) kirp ama kalan adin kaydi varsa
  rastgele `Gezgin######` ver. Oneri [Muhtemel]: (b), en az sasirtan.
  - **DURUM: UYGULANDI (tur 5, `f70e5ad`; testler `76ceed9`, ad kilidi 82 kontrol; PIE'da denenmedi).** Yukaridaki "bugun" paragrafi
    tur 5 oncesi davranistir. Simdi izinsiz karakterli ad **kirpilmaz**: `PreLogin` temizlemenin (bas/son bosluk disinda) degistirdigi
    `?Name=`'i "Gecersiz ad: Turkce harf kullanilamaz ..." ile reddeder (`MMOGameMode.cpp:595-599`, `InitialNameWasAltered` `:397-400`),
    `ChangeName` ayni durumda `Gezgin######` verir (`:466-468`, `:502`), "izinsiz karakterler atildi" bildirimi kalkti. Ek olarak
    ayrilmis adlar (`MMONameRules::IsReservedName`: kanonik `default`, `player`+yalniz rakam) `PreLogin`/`ServerSetName`'de reddedilir,
    `ChangeName`'de `Gezgin` alir, posta alamaz/gonderemez. Yani (b) + ayrilmis adlar icin (c) secildi; karar yalniz "Turkce harfli ad
    gerekli mi" olarak acik kaldi (ustteki madde).
- **Uzak ad degistirme kilidinin kapsami:** kilit "kaydi olan karakter adini degistiremez" (`MMOCharacter.cpp:1655-1670`; satirlar `1f4a557`, eskiden 1648-1663) ve
  yukaridaki "Ad degistirme kisiti" kararina bagli. Tur 4 ile kilitten sonraki denemeler soguma boyunca kilit metnini gorur
  (`bNameLockRejected`, `:1644-1647`; eskiden 1637-1640). Acik soru ayni: kayitli karakter icin ad degistirme ozelligi istenirse kalici yol
  B-full (`ACCOUNT_IDENTITY_DESIGN.md` §4.7) ve ICU anahtari birlikte tasarlanmali; istenmiyorsa "Ad degistirme kisiti"
  maddesinin (b) secenegi (uzak istemcide ad degistirmeyi tamamen kapat) daha basit.

## Yeni karar adaylari (2026-10-14: Sprint 1 altyapisi, yerel QA/guvenlik tur 6)

Kaynak: bulut PR #3 (`9855256`) yerelde denetlendi, duzeltmeler `1f4a557`'de. Durum ayrintisi `SIX_MAP_PLAN.md` §1.5. PIE'da denenmedi.

- **S15: `UDatabaseMigrator` / `FPacketValidator` silinsin mi, tutulsun mu? (ACIK, kullanici karari)** DURUM [Kesin, grep]: ikisini de
  `Source` icinde **kimse cagirmiyor**. Tur 6'da sahte basari kaldirildi: migrator'in tum DB islemleri `false` +
  `LastError = "not implemented: no database backend"` (`DatabaseMigrator.cpp:195-203` `ExecuteSQL`), `*_rollback.sql` ileri yonde elenir;
  validator'da `ValidatePacketIntegrity` ve `ValidateRPCFrequency` (`PacketValidator.cpp:286-292`) `false` doner, yani `ValidateRPC`
  (`:58`) cagrilirsa her RPC'yi reddeder. Editor derlemesi 0 hata. Secenekler: (a) sil (bakimi yok, yaniltici "anti-cheat" adi kalkar);
  (b) "kullanilmiyor" diye tut (bugunku hal; dosya basinda yorum var). Oneri [Muhtemel]: migration'lar oyun disinda psql/Flyway ile
  (`SIX_MAP_PLAN.md` A2), RPC oran siniri bugunku `_Validate` + bilesen ici bekleme deseniyle; yani iki sinifin da yeri yok, (a).
  Karar A2 basina kadar verilebilir; vermemenin bedeli yalniz kafa karisikligi.
- **Takas altini hep-ya-hic degil (ACIK; karar + Codex devri) [Kesin, kod okumasi; tetiklenebilirligi PIE'da denenmedi]:**
  `9855256` (Sprint 1 commit'i; bu degisiklik mesajda yazmayan Codex O69 posta iade muhasebesi isinden gelir, gorev kaydi
  `docs/tasks/MAIL_REFUND_ACCOUNTING_2026-10-11.md`) `AddGoldInternal`'i `ApplyGoldInternal`'a cevirdi ve yeni bir kosul ekledi: sahip aktor yok ediliyorsa ya da
  bilesen yok ediliyorsa altin degisikligi **sessizce yok sayilir**, 0 doner (`MMOInventoryComponent.cpp:407-413`, kosul `:410`).
  Takas tamamlama dort altin cagrisini arka arkaya yapar ve donus degerine bakmaz (`MMOTradeComponent.cpp:880-883`; esya cantalari
  hemen once `:875-878`'de atanir). Onay ve baslangic kapilari (`:548-549` `CurrentApproval`, `:577-578` `StartTradeServer`) iki aktorun
  yok edilmedigini kontrol eder; `TryExecuteTrade`'in tek cagiricisi (`:566`) hemen oncesinde `CurrentApproval(true)`'yu yeniden cagirir.
  Ama `TryExecuteTrade` (`:788`) icinde ve aktarim satirlarinda bu kontrol tekrarlanmaz, envanter bileseninin kendi `IsBeingDestroyed`'i
  hic kontrol edilmez. Dort cagridan biri 0 donerse (or. bir tarafin envanteri/aktoru yok ediliyor) A'nin altini B'ye gecer, B'ninki A'ya
  gecmez ya da tersi: **altin yaratilabilir veya yok olabilir**, kalem sessizdir. Bugunku yolda araya senkron callback girmiyor
  (`bNotify=false`), yani pratik pencere dar [Muhtemel]; tetiklenebildigi gosterilmedi. Asil risk, ileride araya bir callback girerse
  sessiz yarim takas.
  Secenekler: (a) takas, dort cagridan once iki envanterin de `ApplyGoldInternal` kosulunu gecebilecegini dogrulasin, gecmezse takasi
  iptal etsin; (b) `AppliedDelta` donuslerini toplayip tutarsizlikta geri alsin; (c) takas yolu icin "yok ediliyor" kosulunu atlayan
  ayri bir dahili cagri. `MMOTradeComponent.*` Codex'in dosyasi (`CheckPrPaths.py` uyarisi): duzeltme Codex'e devredilmeli ya da
  Codex'le sira alinmali. Dogrulama maddesi: `PLAYTEST_CHECKLIST.md` "Takas onayi aninda cikis (A-1)". Gereken log: sunucuda
  `event=trade_complete` satirindaki `balance_a`/`balance_b` + `[MMOMETRIC] ev=gold src=` satirlari; iki oyuncunun toplam altini
  takas oncesi ve sonrasi esit olmali.
- **`Scripts/k6_load_test.js` (ACIK, karar gerekmez, not):** HTTP `/api/auth/login`, `/api/character/move`, `/api/combat/attack` ...
  uclarina istek atar; bu uclar yok, oyun trafigi UE'nin UDP ag surucusunden gecer. Bugun calistirilamaz. A3'te (Steam bileti +
  giris servisi) yalniz giris servisi yuk testi olarak **yeniden yazilacak**; oyun ici yuk testi coklu headless istemciyle (NetSmoke deseni).

## Kullanıcı kararı (2026-10-15: H2/H3 krallık erişimi)

- **KESİNLEŞTİ:** H2 İnsan krallığı ve H3 Ork krallığı, doğru ırk için **Lv25+**; girişte veya kalışta üst seviye sınırı yok. Lv59/Lv80'de zorunlu çıkış yok; H4/H5'in Lv59+ açılması krallığa erişimi kaldırmaz. Gelecekte genel seviye tavanı yükselse de H2/H3 uygunluk kuralı değişmez.
- **Kapsam:** yalnız harita uygunluk kararı; mevcut içerik bantları, mob seviyeleri, genel seviye tavanı ve H4/H5 Lv59+ eşiği değişmedi. `SIX_MAP_PLAN.md` gereksinim tablosu, §2.6, §5 ve Aşama 3/6 kabulü güncellendi; kod uygulanmadı, oyun testi yapılmadı.
- **Kabul:** iki krallıkta doğru ırk için Lv24 ret, Lv25/59/80 kabul; mevcut genel tavanı değiştirmeyen gelecekteki Lv>80 uygunluk fikstürü de kabul. Yanlış ırk tüm bu seviyelerde ret; seviye artışı oyuncuyu krallıktan çıkarmaz, H4/H5 açıldıktan sonra kalış/yeniden giriş korunur.

## Yeni karar adaylari (2026-10-17: Ekipman birlestirme A1-A4)

Kaynak: `docs/DESIGN_FUSION.md` §10, §11. D1 kurallari master'da (`d7bb1b1`..`d7c75ed`, `TestFusion.ps1` 180 kontrol, izole worktree'de
GECTI); hicbir komut/RPC/HUD bunu cagirmiyor, **PIE'da denenmedi**. Kod asagidaki onerilerle yazildi; karar degisirse yalniz
`MMOFusion.cpp`/`.h` sabitleri ve test degisir. Ucret sayilari `[PLACEHOLDER]`; D0 ekonomi modeli 2026-10-17'de calisti (`EconomyModel.py` "Birlestirme" bolumu, `[MODEL]`), A3 icin asagida.

| # | Soru | Oneri | Kodda bugun [Kesin] | Etiket |
|---|---|---|---|---|
| A1 | Efsanevi birlestirmeyle uretilebilsin mi? | **Hayir**, yalniz ganimet | `MaxOutputRarity = Epic` (`MMOFusion.h:51`); 3 Destansi/Efsanevi -> `RarityCapped` | [Muhtemel] |
| A2 | Artili (+N) esya birlestirilebilsin mi? | **Hayir** (ilk dilim): +N'ye harcanan altin kaybolmasin, +N aklanmasin | `Plus != 0` -> `PlusNotZero` (`MMOFusion.cpp:153-161`) | [Muhtemel] |
| A3 | Ucret neye bagli? | **Satici alis fiyati** x nadirlik carpani (tek formul; `GetBuyPrice` seviyeyle buyur) | `FusionFeeMult = {0.5, 1.0, 2.0}` (`MMOFusion.cpp:19`); Lv1 Kisa Kilic 86/171/342, Lv25 Obsidyen 794/1.587/3.174 | [Muhtemel] (D0 modeli destekliyor: `{0.5, 1.0, 2.0}` degismesin, asagidaki A3 modeli); oyuncu verisi icin `ev=fusion` olcumu ister |
| A4 | Satici yerine ayri "Demirci" NPC'si? | **Sonra**; NPC yerlesimi S02 dosyalarinda (`MMOZoneLayout`, `MMOGameMode.cpp`) | Herhangi bir saticiya 800 u yakinlik (`IsOwnerNearVendor`) | [Muhtemel] |

Ek acik nokta (kucuk, tasarim): `FindFromSlot` takastaki esdegerleri atlamiyor (en kucuk kimlikli kopya takastaysa ucuncu serbest kopya
olsa bile `TradeLocked`); oneri: atla. Gereken olcum (A3): `[MMOMETRIC] ev=fusion` sayisi ve `ev=gold src=birlestirme` toplami / bant saatlik gelir.

### A3 modeli (D0, 2026-10-17, `[MODEL]`, oyuncu verisi degil)

Kaynak: `python Scripts/EconomyModel.py` "Birlestirme" bolumu; fiyat/zar/ucret kaynaktan okunur, elle sayi yok. Ayrinti: `DESIGN_FUSION.md` §6.1, `BALANCE_MODEL.md` "Ekipman birlestirme".

| Carpan `{Siradan, Nadir, Ender}` | 27->1 toplam ucret | Satici yolu | Min. adim net sink | Anlamli bant D / S (14 bant, esik %10 [Tahmin]) | Her bantta set yapilsa toplam fazla payi D \| S | Lv16-32 S yolu / bant fazlasi |
|---|---:|---:|---:|---:|---:|---:|
| `{0.25, 0.5, 1}` | 4.75 B | 31.75 B | 0.95 B | 5 / 11 | %3.4 \| %22.3 | %72-134 |
| **`{0.5, 1, 2}` (kod)** | 9.50 B | 36.50 B | 1.20 B | 6 / 11 | %6.9 \| %25.7 | %83-155 |
| `{1, 2, 4}` | 19.00 B | 46.00 B | 1.69 B | 10 / 11 | %13.8 \| %32.6 | %105-196 |
| `{2, 4, 8}` | 38.00 B | 65.00 B | 2.69 B | 11 / 11 | %27.5 \| %46.4 | %149-279 |

(B = esyanin alis fiyati. D = girdiler ganimetten, yalniz ucret; S = 26 girdi saticidan alinir + ucret. "Fazla" = bant geliri - kademe gideri; Lv1-5 bantlarinda fazla yok, birlestirme kapali.)

- **Arbitraj/bedava terfi: dort carpanda da yok** [Hesap]: 57 ekipman x 3 adim + zincirler + saticidan al-birlestir-sat; kodun carpani icin en iyi adim -120 altin. Makineli: `python Scripts/EconomyModel.py --check-fusion` (cikis 1 = hata); CI `EconomyModel.py`'yi zaten calistiriyor.
- **Onerim [Muhtemel]: `{0.5, 1, 2}` degismesin, ucret alis fiyatina bagli kalsin.** Gerekce (model): (1) Lv1-15 fiilen kapali (Lv10-15'te D %167, S %623), Lv16'dan itibaren ganimet yolu bant icinde odenebilir (%41); (2) Lv16-32'de satici yolu bant fazlasinin %83-155'i, `{1,2,4}` bunu %105-196'ya cikarip kullanilamaz yapar; (3) yalniz ucret Lv72'de +5 yukseltmeyle (~ +%100 stat) ayni altin (39.007 vs 39.630), Lv25'te 2.8x (15.081 vs 5.436): kesin sonuc icin makul prim.
- **Carpanla cozulmeyen:** Lv66+ bantlarinda ganimet yolu fazlanin %3.9-5.1'ini, satici yolu %15-19'unu emer (`{2,4,8}` ile bile D %16-21); Lv16+ gelir/gider farkini birlestirme kapatmaz. Bu karar 3'un isi; birlestirme onu tamamlar, yerini almaz. Seviyeye bagli ek terim (or. `x (1 + Lv/40)`) **modellenmedi**.
- **Ganimet yolu:** normal mob zariyla kendi kademenin 3 ayni parcasi seviyelenirken gelmez (belirli esya 3 adet 494-1.341 oldurme = 3.4-9.3 sa [Hesap; oldurme 25 sn Tahmin]; bant suresi 0.1-1.5 sa). Elit ganimeti ve gercek dusme sikligi: **veri yok**.
- **Gereken `[MMOMETRIC]` verisi:** `ev=fusion` sayisi + `ev=gold src=birlestirme` toplami / bant saatlik geliri (kod hazir); `ev=item_pickup rarity=..` (henuz yok). Karar kurali: oyuncularin cogu Lv16-32'de hic birlestirmiyor ve altin birikiyorsa `{0.25, 0.5, 1}`; Lv66+ hala >100x gelir/gider ise karar 3.
- **Degisecek dosya:** yalniz olcum sonrasi gerekirse `MMOFusion.cpp::FusionFeeMult` (tek satir, mmo-developer).

## Yeni karar adaylari (2026-10-17: Supabase veritabani baglama; hepsi BEKLIYOR)

Kaynak: `docs/DESIGN_SUPABASE.md` (§8 tam tablo, DURUM blogu). **Durum: kimse onaylamadi.** Kodda gecici varsayim: mimari C, Auth yok. SQL (`supabase/`) ve UE depo katmani
(`Source/MyProject/Public/Save/MMOSupabase*`) yazildi; **SQL hicbir PostgreSQL'de calistirilmadi, UE tarafi yalniz sahte sunucuya karsi (556 kontrol, izole worktree) test edildi,
oyundaki hicbir yol depoyu cagirmiyor, gercek Supabase hesabi/baglantisi yok, PIE yok.** Hesap, proje, anahtar ve Dashboard ayarlari **kullanicidadir** (anahtari SOHBETE
YAPISTIRMA; yalniz yerel ortam degiskeni `MMO_SUPABASE_URL` / `MMO_SUPABASE_SECRET_KEY`). Etiket hepsi [Tahmin]/oneri: Supabase limit/fiyat bilgisi dogrulanmadi.

| # | Soru | Oneri | Not |
|---|---|---|---|
| S1 | Mimari A (tablo uclari) / B (kendi ara servisi) / C (RPC, mantik SQL'de) | **C**, halka acik surumden once **B** | C'nin bedeli: harita sunucularinda genis bir secret key; kapali alfada kabul. Reddedilirse B (+3-5 hafta) |
| S2 | S13 servis dili (Steam bilet dogrulama) | TypeScript (Supabase Edge Function) ya da Python | A5'ten once karar |
| S3 | Supabase Auth kullanilsin mi | **Hayir** | Steam bileti ile cakisir, cift kimlik tablosu |
| S4 | Bolge | **AB (Frankfurt)** | KVKK yurt disi aktarimi icin hukuki gorus |
| S5 | Katman / ucret | alfa **Pro**, dev Free | Duraklatma/yedek kurali dogrulanmadi; harcama siniri acik |
| S6 | Yedek | Pro gunluk yedek + kendi `export_all` | `export_all` ve silme/KVKK RPC'si YOK (yazilmadi) |
| S7 | DB moduna gecis tek yonlu mu / DB -> dosya donus araci yazilsin mi | tek yonlu + `export_all` | Dosyaya donuste DB ilerlemesi kaybolur |
| S8 | Kimlik | K-1 acikken yalniz bilinen oyuncularla; Steam (A5/A3) DB dilim 1-4'ten sonra | DB kimlik cozmez (tek paylasilan secret key, `canonical_key`'i bilen karakteri acar) |
| S9 | Migration kaynagi | `supabase/migrations/`; `Migrations/V1__initial_schema.sql` "kullanilmiyor, UYGULANMAZ" (S15 ile birlikte) | V1 `public` semasinda RLS'siz: Supabase'e uygulanirsa anon anahtarla acik |
| S10 | Yazma-arkada (write-behind) onbellek | **Reddet** | "DB kapaliyken ilerleme yok" kurali |
| S11 | `holder` uretimi (kira sahibi dizesi) | holder = sunucu kimligi + acilis GUID'i (surec basina benzersiz), UE `^[A-Za-z0-9_.:-]{1,96}$` dogrular; ayni holder iki surecte KULLANILMAZ; elle sabit holder yasak/uyarili; ikinci surec ayni holder ile baslarsa reddedilir (Dilim 3 testi) | Donmus yuva release->acquire duzeltmesi benzersizlige dayanir; ihlalde cit asilir ve cift yazim olur. DB zorlayamaz, isletme kurali (`DESIGN_SUPABASE.md` §4.4, §5.1) |

Kapanan guvenlik bulgulari (iki bagimsiz QA + iki guvenlik turu; kod/SQL'de): `p_op_id` fazlaligi, `lease_epoch` adi, `get_op` `found`, `SlotToKey` bas/son bosluk, Shipping `NO_LOGGING` korumasi,
Bearer yalniz JWT, `CreateEx` kirasi, donmus yuva release->acquire, `write_ops` purge `p_batch`, `LogHttp` Warning.
**ACIK notlar:** (a) K-1 kimlik; (b) kira yenilemesini hicbir kod cagirmiyor (`RenewLeaseEx` var, periyodik cagri Dilim 4); (c) yan veri (basarim/gunluk/posta) deposu yok;
(d) R1 daralir, kapanmaz (`get_op` ile uzlastirma; `not_found` kesin "olmayacak" degil); (e) `data_max_octets` 2 MiB SQL'de `jsonb::text` bayti ile olculur, UE karakter sayar:
tavana yakin kayit UE'de gecip DB'de `invalid/data_too_large` alabilir (`supabase/README.md`); (f) motorun `LogHttp`/WinHttp davranisi (basliklar log'a sizar mi) dogrulanmadi;
(g) `MMOSupabaseCharacterStore` / `RestClient` / `UnrealHttpTransport` hicbir oyun yoluna bagli degil, `RunSystemTests.ps1`/`RunAllChecks.ps1` adimi degil
(`Scripts/TestSupabaseStore.ps1` izole worktree'de elle); (h) Dilim 3 (`-MMOCharacterStore`, acilista `HealthcheckEx`) ve Dilim 4 (cagiranlar) yapilmadi,
Dilim 4 Codex'in kirli `MMOCharacter.*` / `MMOTradeComponent.*` dosyalarina dokunur: onlar commit edilmeden baslanmaz.

## Not (2026-10-17): proje ajanlari modeli

`f9b71c2`: `.claude/agents/` altindaki 9 Turkce proje ajaninin (`oyun-tasarimcisi`, `mmo-developer`, `test-muhendisi`, `qa-kontrolcu`,
`ag-guvenlik-denetcisi`, `belge-denetcisi`, `ekonomi-tasarimcisi`, `seviye-tasarimcisi`, `metin-yazari`) `model:` alani `opus` -> `sonnet`
(limit tuketimini azaltmak icin). Diger 12 Ingilizce ajan (`backend`, `devops`, `ue5-developer` ...) `model: inherit` kaldi. Karar gerekmez;
geri almak tek satirlik degisiklik. Bilinen etkisi olculmedi (kalite/hiz karsilastirmasi yok).
