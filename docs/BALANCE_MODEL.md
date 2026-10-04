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
