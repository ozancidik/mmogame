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
