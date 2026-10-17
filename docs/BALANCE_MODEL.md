# İlerleme hızı modeli (statik)

**Tarih:** 2026-10-04. Betik: `Scripts/ProgressionModel.py` (`python3` yeterli, UE gerekmez; bulutta çalıştırıldı). Bu bir **model**dir, oynanış ölçümü değildir: yürüme, ölüm, parti ve gerçek öldürme süresi yok. Hedefler `[PLACEHOLDER]` (`VERTICAL_SLICE_LV1_30.md` 3. iş).

## Yöntem
- Her seviye L için gereken XP: `ComputeExperienceForLevel` kopyası.
- XP/öldürme: seviye penceresindeki ([L-8, L+3]) **tüm normal mobların ortalaması** (spawner banttan eşit olasılıkla seçer; `BalanceReport.py` en yüksek XP'li mobu alıyordu, iyimser).
- Görevler: kapısı L'ye kadar açılmış tüm görevlerin ödül XP'si kümülatif olarak düşülür ("hepsi yapıldı", iyimser); görevlerin kendi öldürme hedefleri kapı seviyesine atanır ve XP'leri de sayılır. `Q_Gold*` (6 görev) hesaba katılmaz, ayrı sütunda gösterilir.

## Sonuç (Lv1–30, ortalama öldürme/seviye)

| Bant | Görevsiz | Görevli (model) | Hedef `[PLACEHOLDER]` |
|---|---:|---:|---|
| 1–5 | 1,5 | 6,4 | 3–5 |
| 6–10 | 2,2 | 6,0 | 6–9 |
| 11–15 | 3,5 | 1,2 | 9–13 |
| 16–20 | 3,2 | 3,2 | 11–15 |
| 21–25 | 4,9 | 2,2 | 13–18 |
| 26–30 | 8,9 | 3,4 | 14–20 |

**Lv1→30 toplam: görevsiz ~121, görevli ~112 öldürme.**

## Okuma
- [Muhtemel] Lv11–30'da hedefin 3–10 kat altındasın: görev ödülleri (özellikle `Q_Level20` 9.000, `Q_Level30` 16.000, boss görevleri) ve yüksek seviyeli mobların XP'si eğriyi çok hızlı geçiriyor. `VERTICAL_SLICE_LV1_30.md`'nin "~80–85 öldürme" bulgusuyla uyumlu (orada yalnız ana zincir sayılmıştı).
- Lv1–5 modelde yavaş görünüyor çünkü gerekli görev öldürmeleri (15 öldürme) kapı seviyesi 1'e yığılıyor; gerçekte bunlar Lv1–3'e yayılır. Bant sayısına değil toplama bak.
- `Q_Gold*` görevleri (kümülatif 48.800 XP, Lv28+) eşiğin büyük kısmını tek tıkla karşılıyor; bu model hesaba katmıyor.
- **Sınırlar:** parti, yürüme/geri dönüş süresi, ölüm cezası, mob doğma beklemesi ve gerçek öldürme süresi yok. Sonuçları `[MMOMETRIC]` ölçümüyle karşılaştır; ayar kararı ölçümden sonra.

## Kullanım
```
python3 Scripts/ProgressionModel.py            # Lv1-79 tablo + bant özeti
python3 Scripts/ProgressionModel.py --max 30   # yalnız Lv1-30
```

## Kilometre taşı ve `Q_Gold*` görevlerinin XP ağırlığı

`ReachLevel` görevleri hedef seviyeye ulaşıldığında, `HaveGold` görevleri ise kapı seviyesinde oyuncu gerekli altını **taşıyorsa** (altın harcanmaz) anında teslim edilir. Aşağıdaki tablo, ödül XP'sinin teslim seviyesindeki bir seviye eşiğine oranını gösterir. **[KOD+MODEL]**

| Görev | Tür | Kapı Lv | Teslim Lv | Ödül XP | Teslim seviyesi eşiği | Oran (seviye) |
|---|---|---:|---:|---:|---:|---:|
| `Q_Level5` | ReachLevel | 1 | 5 | 400 | 244 | 1,64 |
| `Q_Gold500` | HaveGold | 1 | 1 | 300 | 100 | 3,00 |
| `Q_Gold1000` | HaveGold | 3 | 3 | 500 | 156 | 3,21 |
| `Q_Level20` | ReachLevel | 16 | 20 | 9.000 | 6.939 | 1,30 |
| `Q_Gold5000` | HaveGold | 16 | 16 | 6.000 | 2.842 | 2,11 |
| `Q_Level30` | ReachLevel | 24 | 30 | 16.000 | 59.576 | 0,27 |
| `Q_Gold15000` | HaveGold | 28 | 28 | 14.000 | 40.670 | 0,34 |
| `Q_Gold30000` | HaveGold | 28 | 28 | 28.000 | 40.670 | 0,69 |
| `Q_Level40` | ReachLevel | 30 | 40 | 24.000 | 225.026 | 0,11 |
| `Q_Level50` | ReachLevel | 40 | 50 | 35.000 | 508.676 | 0,07 |
| `Q_Gold50000` | HaveGold | 40 | 40 | 45.000 | 225.026 | 0,20 |
| `Q_Level65` | ReachLevel | 50 | 65 | 55.000 | 1.155.776 | 0,05 |
| `Q_Level80` | ReachLevel | 65 | 80 | 90.000 | 2.068.826 | 0,04 |

- [Muhtemel] `Q_Gold5000` (Lv16, ~2,1 seviye), `Q_Gold500/1000` (~3 seviye) ve `Q_Level5` (~1,6 seviye) erken oyunda bir seviyenin çok üstünde XP veriyor. Model, oyuncunun Lv30'a kadar 80.000+ altın biriktirdiğini söylüyor (`VERTICAL_SLICE_LV1_30.md`), yani `Q_Gold*` görevleri kapı açıldığı anda bedava XP.
- Lv28 sonrası oranlar küçük (≤0,7 seviye); asıl hız etkisi Lv1–20'de.
- **Seçenekler (karar bekliyor):** (a) `HaveGold` görevlerinde teslimde altını düş ("harca"), (b) `RewardXP`'yi kapı seviyesi eşiğinin ≤%50'sine indir, (c) olduğu gibi bırak ve yalnız dokümante et. Ölçüm (`[MMOMETRIC]`) olmadan (b) körlemesine olur.

## Kümülatif görev XP'si / kümülatif gereken XP (model)

`kum.gorev%` ≥ %100: o seviyeye kadar kapısı açılmış görevlerin ödülü tek başına oyuncuyu o seviyeye taşır (öldürme/grind gerekmez). `ReachLevel` ödülleri hedef seviyede teslim sayıldı; `Q_Gold*` hariç. **[KOD+MODEL]**

| Seviye | Kümülatif görev XP | Kümülatif gerekenin % |
|---:|---:|---:|
| 5 | 5.350 | %652 |
| 10 | 16.450 | %495 |
| 15 | 24.350 | %222 |
| 20 | 53.050 | %155 |
| 25 | 83.450 | %79 |
| 30 | 140.450 | %45 |
| 40 | 297.450 | %17 |
| 50 | 593.450 | %11 |
| 65 | 2.503.450 | %14 |
| 79 | 8.658.450 | %22 |

- [Muhtemel] Lv1–10 görev ödülleri eşiklerin 5–8 katı: bilinçli bir "hızlı giriş" olabilir. Lv15'te %222, Lv20'de ~%150: görevler hâlâ grind'ın çoğunu karşılıyor. Lv25'te %79, Lv30'da %45'e düşüyor: görev ödülleri eşiklerin gerisine ancak Lv25 civarında düşüyor, yani yavaşlama o noktadan sonra başlıyor.
- Ayar denemek için: `python3 Scripts/ProgressionModel.py --mob-mult "1:1.0,10:0.6,30:0.4" --quest-mult "1:1.0,16:0.5,30:0.4"` (kodu değiştirmez; yalnız modele uygular). Not: görev öldürme sayıları sabit olduğundan Lv1–5 bandı ne olursa olsun ~6 öldürmenin altına inmez; bu modelin sınırıdır.

## Altın ekonomisi (statik model)

Betik: `python3 Scripts/EconomyModel.py [--upgrade-to N]`. Gelir = öldürme (ilerleme modelinden) × pencere ortalama mob altını + görev altın ödülleri; gider = kademenin 4 parçasının alış fiyatı + parça başına +N yükseltme. İksir, şifacı, loot satışı, ölüm, yanma ve parti **yok**; sonuç yön gösterir, ölçüm değildir. **[KOD+MODEL]**

Tablo 2026-10-17'de güncel `EconomyModel.py` çıktısıyla yenilendi (eski satırlar 2026-10-05'ti: gelir/gider 9–224×; yükseltme maliyeti o zaman seviyeden bağımsızdı). Yükseltme maliyeti artık `50×(artı+1)×(100+Lv²)/100`, bağış görevleri gelirden düşülüyor. **[KOD+MODEL]**

| Kademe Lv | Aralık | Gelir | Gider (+3) | Gelir/gider |
|---:|---|---:|---:|---:|
| 1 | 1–2 | 829 | 1.724 | 0,5 |
| 3 | 3–4 | 960 | 2.224 | 0,4 |
| 5 | 5 | 2.462 | 2.820 | 0,9 |
| 6 | 6 | 3.267 | 968 | 3,4 |
| 7 | 7–9 | 4.167 | 3.644 | 1,1 |
| 10 | 10–15 | 18.806 | 4.848 | 3,9 |
| 16 | 16–24 | 87.057 | 7.728 | 11,3 |
| 25 | 25–32 | 215.456 | 13.400 | 16,1 |
| 33 | 33–41 | 457.885 | 20.176 | 22,7 |
| 42 | 42–50 | 923.161 | 29.600 | 31,2 |
| 51 | 51–57 | 1.076.615 | 40.960 | 26,3 |
| 58 | 58–65 | 1.718.699 | 51.300 | 33,5 |
| 66 | 66–71 | 2.128.032 | 64.592 | 32,9 |
| 72 | 72–79 | 3.059.719 | 75.776 | 40,4 |

- [Muhtemel] Lv1–5 altın **kıt** (gelir/gider 0,4–0,9), Lv7–9 sınırda (1,1): ilk set ancak görev altınıyla alınır, bu "kıtlık yalnız Lv1–8" hedefiyle uyumlu. Lv16'dan itibaren gelir giderin **11–40 katı** (+3 yükseltmede, başarısızlık/yanma yok): ekipman altını zorlamıyor, altın birikir. `VERTICAL_SLICE_LV1_30.md`'nin "faucet bol, sink zayıf" bulgusunu doğruluyor. Ancak bu "gider" kademe başına tek +3 setidir; gerçek sink oyuncunun +N hedefine bağlıdır: aşağıda "Altın sink senaryoları".
- Ekipman fiyatı `12×(atk+def) + 25×seviye + 50` ile büyüyor (Lv1 171 → Lv72 4.106, ×24); ortalama mob altını Lv1 16 → Lv79 10.188 (×640) ve bant başına saatlik gelir 3,5 bin (Lv1–2) → 3,0 milyon (Lv72–79) (×860). Polinom sink (kare/küp) bu üstel gelire yetişemez; bu yüzden tek maliyet düzeltmesi açığı kapatmıyor.
- **Karar bekleyen:** karar 3, bkz. "Altın sink senaryoları" ve `DECISIONS_NEEDED.md` karar 3 (2026-10-17 bloğu).

## Altın sink senaryoları (2026-10-17, **[MODEL]**, oynanışta ölçülmedi)

Betik: `python -I Scripts/EconomyModel.py --sink-scenarios` (normal çıktı ve `--check-fusion` bozulmadı; çıktı bayt bayt aynı). Formüller koddan okunur: `GetUpgradeCost`, yükseltme şansı `90/80/70/55/45/35/25/20`, yanma `0/0/0/15/30/50/70/100`, `ComputeHealCost`, `PotionPrice`, `GoldDonationMinTarget`, birleştirme tabloları. Varsayımlar `SINK_*` sabitleridir **[TAHMIN]** (4 ölüm/sa, yarısı sifa atlatır; 30 Büyük Can İksiri/sa; 4 parçalı tek kademe/bant).

- **Yükseltme gerçek sink'tir ve +N'ye aşırı duyarlıdır [Hesap]:** her deneme ücret öder, yanan parça yeniden alınır. 4 parça, Lv72–79: +3 hedefi 95.822, +5 hedefi 369.490, +7 hedefi 3.066.539 altın (bant geliri 3.059.719). Yani +7 kovalamak bandın tüm gelirini yer, +3 ise %3'ünü. Oyuncuların çoğu hangi +N'de duruyor: **veri yok**.
- Kodda olan sink toplamı, Lv16–79 (gelir brüt 9,77 M): ALT (+3, şifacı, iksir, bağış) **%5,2**, ORTA (+5, birleştirme D) **%22,1**, ÜST (+7, birleştirme S) %144,9 (üst sınır; bütçe yetmeyince gerçekleşmez). Gelir/sink: ALT 4,1–30,1×, ORTA 1,3–6,2×.
- İksir (30/sa × 40 altın) gelirin Lv16–24'te %1,5'i, Lv42+'da %0,2'sinden azı; şifacı (kare, yarısı atlatılıyor) %0,15–0,6. İkisi de açığı kapatmaz.
- Gelir tarafı kötümser: ganimet satışı modelde yok; ortalama mob altınına +%2 (Lv51+) ile +%13 (Lv16–24) eklerdi.

| Aday (EK altın / bant fazlası, Lv16–79 toplamı) | Etki | Not |
|---|---:|---|
| A1 şifacı küp `0,09×(Lv−1)³` | %2,5 | 4 ölüm/sa, yarısı atlatılıyor |
| A2 şifacı `10 × ort. mob altını` | %8,3 | mob tablosuna bağlı yeni yardımcı gerekir |
| B1 bağış `0,5 sa gelir`, katılım %50 | %22,3 | yalnız veri satırı; katılım **[TAHMIN]** |
| B2 bağış `1 sa gelir`, katılım %50 | %45,5 | aynı |
| C1 yükseltme dik eğri `×(1+(Lv−30)/30)^1`, hedef +3 / +5 / +7 | %3,2 / %13,3 / %111,8 | hedefe aşırı duyarlı, talep esnekliği modellenmedi |
| C2 aynı, üs 2: +3 / +5 / +7 | %9,9 / %41,7 / %351,5 | +7 oynanamaz hale gelir |
| D1 3 opsiyonel satın alma (Lv20/40/60) | %2,4 | kodda yok |
| (karşılaştırma) birleştirme D / S | %6,9 / %25,7 | |

Paket sonuçları `--sink-scenarios` çıktısının (3) bölümünde; hedef "gelir ≤ 1,5 × sink" **[TAHMIN]**. Hiçbir küçük paket tek başına ALT tabanında hedefi tutturmuyor (P1 = A1+B1: ALT 0/8, ORTA 1/8 bant, EK %24,8); P3 = A2+B2+C2+D1 ORTA'da 8/8. Öneri ve dosya çakışması: `DECISIONS_NEEDED.md` karar 3.

## Ekipman birleştirme (nadirlik füzyonu) ekonomisi (2026-10-17, **[MODEL]**, oynanışta ölçülmedi)

Betik: `python Scripts/EconomyModel.py` çıktısının sonundaki "Birleştirme" bölümü; `--check-fusion` yalnız denetim (CI), `--fusion-full` 57 ekipmanın tamamı. Fiyat (`(Atk+Def)×12 + Lv×25 + 50`), satış (`0,4 × alış × nadirlik çarpanı`), nadirlik zarı ve ücret çarpanı (`MMOFusion.cpp`) kaynaktan okunur. Tasarım: `DESIGN_FUSION.md` §6.1; karar: `DECISIONS_NEEDED.md` #8 (A3 modeli).

| Sonuç (kod çarpanı `{0,5; 1; 2}`) | Değer |
|---|---|
| Arbitraj (satış kârı > 0 kombinasyon) | **yok**; adım başına net altın yok oluşu 1,20 / 1,85–1,87 / 3,06–3,09 × B |
| 27 Sıradan → 1 Destansı toplam ücret | 9,50 B (57 ekipmanda 9,50–9,55 B); satıcıdan alınırsa 36,50 B |
| Lv25 ObsidianBlade (B 1.587) | ücret 15.081, satıcı yolu 57.930, çıktı satış değeri 1.334 |
| Bant fazlasını emme, 4 parçalık set, yalnız ücret (D) / satıcı yolu (S) | Lv16–24 %41 / %155, Lv25–32 %22 / %83, Lv42–50 %7,7 / %29, Lv72–79 %3,9 / %15 |
| Her bantta set yapılsa toplam fazla payı | D %6,9; S %25,7 |

- [Hesap] Birleştirme Lv16+ gelir/gider farkını (yukarıdaki "Altın ekonomisi" tablosunda 11–40×) kapatmaz; yüksek seviyede etkisi küçük. Lv1–15 fiilen kapalı. Seviyeyle ölçeklenen yükseltme/şifacı sink'i (karar 3) hâlâ gerekli.
- [Hesap] Ganimetten kendi kademenin 3 aynı parçası seviyelenirken gelmez (belirli eşya 3 adet 494–1.341 öldürme, bant süresi 0,1–1,5 sa); elit ganimeti ve gerçek düşme sıklığı: veri yok.
- Not: "Altın ekonomisi" tablosu 2026-10-17'de güncel çıktıyla yenilendi (Lv16'da 11,3×, Lv72'de 40,4×).

## Uygulanan ayar (2026-10-04, DERLENMEDI): XP çarpan eğrisi, parti bonusu, Q_Gold

**Tek ayar noktası:** `Source/MyProject/Private/Core/MMOXPTuning.cpp`. Mob XP'si mob seviyesine, görev ödül XP'si görev kapı seviyesine göre çarpılır (tablo değerleri ham kalır; çarpan `MakeMob`/`MakeQuest` içinde uygulanır). `Scripts/ProgressionModel.py` varsayılan olarak **aynı eğriyi dosyadan okur** (`--raw` ile çarpansız).

| Seviye | 1 | 5 | 8 | 10 | 12 | 16 | 20 | 25 | 30 | 34+ |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Mob XP çarpanı | 1,00 | 1,00 | 0,70 | 0,50 | 0,40 | 0,30 | 0,25 | 0,33 | 0,55 | 1,00 |
| Görev XP çarpanı | 1,00 (≤4) | | 0,60 | | 0,40 | 0,30 | 0,26 | 0,25 | 0,26 | 1,00 |

Lv1–5 aynen (hızlı giriş). Lv30 → Lv34 arası doğrusal 0,55 → 1,0 rampa (mob; Lv31–33 ≈ 0,66 / 0,78 / 0,89; görev 0,26 → 1,0); yalnız Lv34+ kalibre içerik çarpansız. Eğri, modelde seviye başına öldürme sayısını hedef bantlara yaklaştırmak için rasgele arama ile bulundu ve yuvarlandı.

**Model sonucu (görevli, öldürme/seviye; önce → sonra):**

| Bant | Önce | Sonra | Hedef `[PLACEHOLDER]` |
|---|---:|---:|---|
| 1–5 | 6,4 | 6,4 | 3–5 (modelin tabanı: görev öldürmeleri) |
| 6–10 | 6,0 | 6,0 | 6–9 |
| 11–15 | 1,2 | 1,2 | 9–13 |
| 16–20 | 3,2 | 3,2 | 11–15 |
| 21–25 | 5,4 | 12,9 | 13–18 |
| 26–30 | 4,4 | 18,5 | 14–20 |

Lv1→30 toplam: ~117 → ~241 öldürme. Lv31–45 modelde 17–25 öldürme/seviye ve kesintisiz (Lv30–34 doğrusal rampa, Lv34'te çarpan 1,0).

**Dürüst sınır [Muhtemel]:** Lv11–20 modelde hâlâ "bedava" kalıyor. Sebep: Lv5–10 görevlerinin **zorunlu öldürme sayıları** (≈70 öldürme × yüksek seviyeli mob XP'si) ve ödülleri, Lv11–20 eşiklerinin üstünde birikim sağlıyor; çarpan eğrisini ne kadar düşürsem de (denenen yüzlerce kombinasyon) bu bantlar kıpırdamıyor. Bunu düzeltmek için Lv6–12 mob/görev XP'sini çok daha agresif düşürmek (yani Lv6–10'u da yavaşlatmak) ya da görev öldürme sayılarını artırmak gerekir. Ölçüm (`[MMOMETRIC]`) olmadan yapılmadı.

**Parti bonusu:** Parti hasar payı artık ekstra üye başına +%10 çarpılır (`AMMOMob::HandleDeath`): 2 kişide toplam ×1,1, 8 kişide ×1,7; kişi başı pay `bonus/üye sayısı`. **Üye sayısı = mobun 3000 birim çevresindeki CANLI parti üyeleri** (uzakta/ölü/AFK üye böleni büyütmez; ör. 3 kişilik partide biri uzaktaysa kalan ikisi 1,1/2 = %55 alır, 1,2/3 = %40 değil). Uzak üyenin verdiği hasar grup payına yine girer. `MMOXPTuning::PartyMultiplier` aynı sayıyla çağrılır. Tek başına oynayan için değişiklik yok.

**`Q_Gold*`:** Ham XP ödülleri düşürüldü: `Q_Gold500` 300→150, `Q_Gold1000` 500→250, `Q_Gold5000` 6000→4000 (üstüne kapı-seviyesi çarpanı uygulanır). Altını harcatma seçeneği uygulanmadı: ekonomi modeline göre bu tutarlar toplam gelirin yanında önemsiz, yani gerçek bir altın çıkışı üretmez.

**Ekonomi yan etkisi (model):** XP eğrisi yavaşlatılınca daha çok öldürme = daha çok altın. `Scripts/EconomyModel.py` yeni eğriyle Lv1→30 gelirini ~153.000'den **~270.000**'e çıkarıyor (kademe giderinin ~11 katı; önceki ~6,3). Yani XP yavaşlatması altın sink'i ihtiyacını artırdı; altın sink'i (yükseltme/iksir/şifacı maliyetini seviyeyle ölçekleme) ayrı ve hâlâ açık bir karar.

## Model düzeltmesi (2026-10-05): "grind=0" bantları ayrı etiketlenir

**Sorun (dürüst kayıt):** Görev XP'si kümülatif ihtiyacı aştığında modelin `grind` terimi 0 olur ve "kill (görevli)" sütunu yalnız görevlerin **zorunlu öldürme hedefleri** kalır (Lv11, 14, 15, 17, 19'da bu 0). Önceki bant özetindeki Lv1–5 "çok yavaş" ve Lv11–20 "çok hızlı" bayrakları bu yüzden **yanıltıcıydı**: o bantlarda sayı XP hızını değil, görev metnindeki öldürme sayısını ölçüyor. Yukarıdaki "Sonuç" ve "önce → sonra" tablolarındaki Lv1–20 "görevli" değerlerini bu gözle oku. **[MODEL]**

**Değişiklik (`Scripts/ProgressionModel.py`):**
- Seviye satırında grind=0 ise sonda `G` işareti.
- Bant özetinde bandın **tüm** seviyelerinde grind=0 ise hız bayrağı yerine `gorev XP'si yeterli (grind gerekmiyor; sayi = yalniz gorev oldurme hedefleri)` etiketi; kısmen grind=0 ise hız bayrağı + `[n/5 seviyede grind=0]`.
- Mevcut eğriyle sonuç: **Lv1–20'nin 20 seviyesinin tamamı grind=0** (Lv1–5, 6–10, 11–15, 16–20 hepsi "görev XP'si yeterli"). Lv21–25 12,9 öldürme/seviye (hedefin hemen altı), Lv26–30 hedefte.

**`Scripts/EconomyModel.py`:** grind=0 seviyelerinde gelir için öldürme sayısı artık 0 kabul edilmez: `max(görev hedefleri, 0,5 × görevsiz öldürme)` (`--min-kill-frac`, varsayılan 0,5). Gerekçe: oyuncu görev hedefine giderken ve ganimet için yine mob keser; "görevsiz grind'in yarısı" kadar yan öldürme **varsayımdır** `[PLACEHOLDER]`, `[MMOMETRIC]` `kill` olaylarıyla düzeltilmeli. Etkisi küçük: 9 seviye tabana çekildi, Lv1→30 gelir ~269.900 → **~276.200** (gider oranı 11,0x → 11,3x); Lv10–15 kademesi 8.008 → 9.908, Lv16–24 70.497 → 74.868. Ekonomi sonucu (sink zayıf) değişmez. `--min-kill-frac 0` eski hesabı verir.

## Lv11–20 için aday XP eğrileri (B7) — **B UYGULANDI (2026-10-05, `MMOXPTuning.cpp`)**

Eğri **B** koda uygulandı (kalan adaylar A/C karşılaştırma için durur). Değerler hâlâ `[PLACEHOLDER]`; `[MMOMETRIC]` ölçümüyle ayarlanır. Komutla tekrar üretilebilir:

```
python3 Scripts/ProgressionModel.py --max 30 --mob-mult "<mob>" --quest-mult "<görev>"
```

| Aday | `--mob-mult` | `--quest-mult` |
|---|---|---|
| Mevcut (kod) | `1:1.0,5:1.0,8:0.7,10:0.5,12:0.4,16:0.3,20:0.25,25:0.33,30:0.55,34:1.0` | `1:1.0,4:1.0,8:0.6,12:0.4,16:0.3,20:0.26,25:0.25,30:0.26,34:1.0` |
| A – yalnız görev Lv6–20 | mevcut | `1:1.0,4:1.0,6:0.35,8:0.2,12:0.15,16:0.2,20:0.26,25:0.25,30:0.26,34:1.0` |
| B – mob+görev Lv6–20 | `1:1.0,5:1.0,8:0.4,10:0.25,12:0.2,16:0.2,20:0.22,25:0.33,30:0.55,34:1.0` | `1:1.0,4:1.0,5:0.5,8:0.1,12:0.08,16:0.1,20:0.18,25:0.25,30:0.26,34:1.0` |
| C – Lv1'den itibaren | `1:1.0,4:0.8,6:0.35,8:0.2,10:0.15,12:0.18,16:0.2,20:0.22,25:0.33,30:0.55,34:1.0` | `1:0.5,4:0.4,6:0.2,8:0.1,12:0.08,16:0.1,20:0.18,25:0.25,30:0.26,34:1.0` |

Sonuç (görevli öldürme/seviye; parantezde grind=0 seviye sayısı / 5):

| Bant | Hedef | Mevcut | A | B | C |
|---|---|---:|---:|---:|---:|
| 1–5 | 3–5 | 6,4 (5) | 6,4 (5) | 6,4 (5) | 6,4 (5) |
| 6–10 | 6–9 | 6,0 (5) | 6,0 (5) | 6,0 (5) | 6,0 (5) |
| 11–15 | 9–13 | 1,2 (5) | 1,2 (5) | 1,2 (5) | 4,7 (3) |
| 16–20 | 11–15 | 3,2 (5) | 4,7 (4) | 9,6 (1) | **12,9 (0)** |
| 21–25 | 13–18 | 12,9 | **14,2** | **15,8** | **15,8** |
| 26–30 | 14–20 | 18,5 | 18,5 | 18,7 | 18,7 |
| Lv1→30 toplam | | ~241 | ~255 | ~289 | ~323 |

**Bulgular [MODEL]:**
- Lv11–20 düğümlerini tek başına düşürmek (A) neredeyse etkisiz: Lv11–20'yi "bedava" yapan şey Lv1–10'da biriken fazla XP (Lv1–5 görev ödülleri ihtiyacın %350–600'ü + Lv1–10 görevlerinin ~75 zorunlu öldürmesi).
- B, Lv16–20'yi hedefe yaklaştırır ve Lv21–25'i hedefe sokar; Lv11–15 yine bedava.
- Lv11–15'i bile kısmen grind'e sokmak için C gibi **Lv1–5 "hızlı giriş"ten vazgeçmek** gerekir (görev XP ×0,5, Lv4+ mob ×0,8). Bu bir tasarım kararıdır; C'de bile Lv11–15 hedefin (9–13) altında (4,7).
- Kalan seçenekler (model dışı, karar bekliyor): Lv1–10 görevlerinin **ham** ödül XP'sini düşürmek, Lv11–15'e yeni öldürme görevleri eklemek, ya da Lv1–15'i bilinçli "hikâye/görev bandı" kabul edip hedef bandını değiştirmek.
- Adayların ekonomi etkisi ölçülmedi (`EconomyModel.py` koddaki eğriyi okur); B/C daha çok öldürme = biraz daha çok altın demektir.
