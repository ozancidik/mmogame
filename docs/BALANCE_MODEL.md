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

| Kademe Lv | Aralık | Gelir | Gider (+3) | Gelir/gider |
|---:|---|---:|---:|---:|
| 1 | 1–2 | 829 | 1.716 | 0,5 |
| 3 | 3–4 | 960 | 2.120 | 0,5 |
| 5 | 5 | 2.462 | 2.524 | 1,0 |
| 7 | 7–9 | 4.130 | 3.060 | 1,3 |
| 10 | 10–15 | 8.008 | 3.648 | 2,2 |
| 16 | 16–24 | 42.148 | 4.656 | 9,1 |
| 25 | 25–32 | 143.441 | 5.904 | 24,3 |
| 33 | 33–41 | 457.479 | 7.112 | 64,3 |
| 42 | 42–50 | 931.425 | 8.432 | 110,5 |
| 72 | 72–79 | 3.038.495 | 13.568 | 223,9 |

- [Muhtemel] Lv1–9 altın **kıt** (gelir/gider 0,5–1,3): ilk set ancak görev altınıyla alınır, bu "kıtlık yalnız Lv1–8" hedefiyle uyumlu. Lv16'dan itibaren gelir giderin 9–224 katı: ekipman ve +3 yükseltme altını hiç zorlamıyor, altın birikir. `VERTICAL_SLICE_LV1_30.md`'nin "faucet bol, sink zayıf" bulgusunu doğruluyor.
- Yükseltme maliyeti `50×(artı+1)`, seviyeden bağımsız; ekipman fiyatı yalnız `12×(atk+def) + 25×seviye + 50` ile büyüyor. Gelir ise mob altınıyla ~×1,2/seviye büyüyor, bu yüzden oran açılıyor.
- **Karar bekleyen:** yeni altın sink'i (ör. yükseltme/iksir/şifacı maliyetini seviyeyle ölçekle) ya da gelir düşürme. Ölçüm (`[MMOMETRIC]` `gold` olayları) olmadan körlemesine olur. Q_Gold* görevleri birikimi ödüllendirdiği için bu dengesizliği büyütüyor.

## Uygulanan ayar (2026-10-04, DERLENMEDI): XP çarpan eğrisi, parti bonusu, Q_Gold

**Tek ayar noktası:** `Source/MyProject/Private/Core/MMOXPTuning.cpp`. Mob XP'si mob seviyesine, görev ödül XP'si görev kapı seviyesine göre çarpılır (tablo değerleri ham kalır; çarpan `MakeMob`/`MakeQuest` içinde uygulanır). `Scripts/ProgressionModel.py` varsayılan olarak **aynı eğriyi dosyadan okur** (`--raw` ile çarpansız).

| Seviye | 1 | 5 | 8 | 10 | 12 | 16 | 20 | 25 | 30 | 34+ |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Mob XP çarpanı | 1,00 | 1,00 | 0,70 | 0,50 | 0,40 | 0,30 | 0,25 | 0,33 | 0,55 | 1,00 |
| Görev XP çarpanı | 1,00 (≤4) | | 0,60 | | 0,40 | 0,30 | 0,26 | 0,25 | 0,26 | 1,00 |

Lv1–5 aynen (hızlı giriş), Lv34+ kalibre içerik değişmedi. Eğri, modelde seviye başına öldürme sayısını hedef bantlara yaklaştırmak için rasgele arama ile bulundu ve yuvarlandı.

**Model sonucu (görevli, öldürme/seviye; önce → sonra):**

| Bant | Önce | Sonra | Hedef `[PLACEHOLDER]` |
|---|---:|---:|---|
| 1–5 | 6,4 | 6,4 | 3–5 (modelin tabanı: görev öldürmeleri) |
| 6–10 | 6,0 | 6,0 | 6–9 |
| 11–15 | 1,2 | 1,2 | 9–13 |
| 16–20 | 3,2 | 3,2 | 11–15 |
| 21–25 | 5,4 | 12,9 | 13–18 |
| 26–30 | 4,4 | 18,5 | 14–20 |

Lv1→30 toplam: ~117 → ~241 öldürme. Lv31–45 modelde 17–25 öldürme/seviye ve kesintisiz (Lv34'te çarpan 1,0'a döner).

**Dürüst sınır [Muhtemel]:** Lv11–20 modelde hâlâ "bedava" kalıyor. Sebep: Lv5–10 görevlerinin **zorunlu öldürme sayıları** (≈70 öldürme × yüksek seviyeli mob XP'si) ve ödülleri, Lv11–20 eşiklerinin üstünde birikim sağlıyor; çarpan eğrisini ne kadar düşürsem de (denenen yüzlerce kombinasyon) bu bantlar kıpırdamıyor. Bunu düzeltmek için Lv6–12 mob/görev XP'sini çok daha agresif düşürmek (yani Lv6–10'u da yavaşlatmak) ya da görev öldürme sayılarını artırmak gerekir. Ölçüm (`[MMOMETRIC]`) olmadan yapılmadı.

**Parti bonusu:** Parti hasar payı artık ekstra üye başına +%10 çarpılır (`AMMOMob::HandleDeath`): 2 kişide toplam ×1,1, 8 kişide ×1,7; kişi başı pay `bonus/üye sayısı`. Tek başına oynayan için değişiklik yok.

**`Q_Gold*`:** Ham XP ödülleri düşürüldü: `Q_Gold500` 300→150, `Q_Gold1000` 500→250, `Q_Gold5000` 6000→4000 (üstüne kapı-seviyesi çarpanı uygulanır). Altını harcatma seçeneği uygulanmadı: ekonomi modeline göre bu tutarlar toplam gelirin yanında önemsiz, yani gerçek bir altın çıkışı üretmez.

**Ekonomi yan etkisi (model):** XP eğrisi yavaşlatılınca daha çok öldürme = daha çok altın. `Scripts/EconomyModel.py` yeni eğriyle Lv1→30 gelirini ~153.000'den **~270.000**'e çıkarıyor (kademe giderinin ~11 katı; önceki ~6,3). Yani XP yavaşlatması altın sink'i ihtiyacını artırdı; altın sink'i (yükseltme/iksir/şifacı maliyetini seviyeyle ölçekleme) ayrı ve hâlâ açık bir karar.
