# Zaman çizelgesi modeli: Lv1–80 kaç saat? (model, ölçüm değil)

**Tarih:** 2026-10-09. **Kaynak:** `python Scripts/TimelineModel.py --sweep` (yeni betik; `ProgressionModel.py`'nin öldürme sayılarını dakikaya çevirir).
Etiket: hepsi **[Tahmin]**. Yürüme/bölge yolculuğu, ölüm, parti, alışveriş ve mob bekleme sayılmaz; gerçek süre bundan **uzun** olur.

## Varsayımlar

- Öldürme sayısı = `ProgressionModel` "görevli" sütunu (kapısı açılan tüm görevler yapılır; `Q_Gold*` hariç).
- Öldürme başına süre: 15 / **25** / 40 sn (hedef seçme + savaş + kısa yürüme).
- Her görev, kapısının açıldığı seviyede 90 sn ek iş (NPC'ye gidip gelme + konuşma).

## Sonuç (o seviyeye ulaşma, kümülatif saat)

| Seviye | 15 sn/öldürme | **25 sn** | 40 sn |
|---:|---:|---:|---:|
| 10 | 0,6 | **0,8** | 1,0 |
| 20 | 1,2 | **1,6** | 2,1 |
| 30 | 2,2 | **3,0** | 4,3 |
| 40 | 3,1 | **4,6** | 6,7 |
| 50 | 4,2 | **6,1** | 9,1 |
| 60 | 5,3 | **7,9** | 11,7 |
| 70 | 6,4 | **9,4** | 14,0 |
| 80 | 7,3 | **10,7** | 15,9 |

## Ne anlama geliyor

- **Lv30 hedefi tutuyor:** `DECISIONS_NEEDED` madde 6 "Lv30 2–3 saat" diyordu; model 2,2–4,3 saat veriyor.
- **Ama tüm oyun çok kısa:** Lv80'e ~11 saat (7–16 saat aralığı). Knight Online / Rise Online tarzı oyunlarda üst seviye
  aylar sürer. Bu hızla oyuncu tüm içeriği bir hafta sonunda bitirir. Bu bir **tasarım kararı**: ya hedef "kısa,
  yoğun bir PvE yolculuğu"dur (o zaman sorun yok), ya da Lv30+ eğrisi bilerek yavaşlatılmalıdır
  (`MMOXPTuning.cpp` görev/mob çarpanları ve `MMOStatsComponent::ComputeExperienceForLevel` eşikleri).
- Model iyimser: yolculuk (Winter cepleri hub'dan 4–7 bin birim), ölüm ve dinlenme eklenince gerçek süre muhtemelen
  1,5–2 katıdır. Yine de Lv80 için onlarca saatin altında kalır.

## Gerekli veri

`[MMOMETRIC]` `levelup` olaylarının zaman damgaları (`Scripts/PlaytestMetrics.py`): ilk 20 dakikalık koşu
"öldürme başına gerçek saniye" değerini verir; o tek sayı bu tablonun hangi sütunu olduğunu söyler.
