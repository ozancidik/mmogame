# Oyun içi test runbook'u (tek sayfa, sıralı kapılar)

**Tarih:** 2026-10-05. Yerelde (Windows, UE 5.7) ilk oturumda **yukarıdan aşağı** izlenecek tek sayfa. Her kapı bir öncekinin geçmesini ister: bir kapı **KALDI** ise dur, sonucu aşağıdaki şablonla yaz, sonraki kapıya geçme. Ayrıntılar bağlantılı belgelerde; bu sayfa yalnız sırayı ve geçme ölçütünü tutar.

| Kapı | Ne | Ayrıntı | Süre [TAHMİN] | Geçme ölçütü |
|---|---|---|---|---|
| 0 | Derle + otomatik kontroller | `PR1_VERIFY.md` §1 | 10–20 dk | `Result: Succeeded` ve `RunAllChecks.ps1` hatasız |
| 1 | PR #1 davranış testleri 1–18 | `PR1_VERIFY.md` §2 | ≈20 dk | 18 satırın hepsi "Beklenen" sütunuyla aynı |
| 2 | İlk 7 oyun içi test | `PLAYTEST_PRIORITY.md` §2 (ayrıntılı adımlar `FIRST_PLAYTEST.md`) | ≈45 dk | 1–3 kesin geçmeli; 4–7'de veri kaybı/kopyalama yok |
| 3 | `[MMOMETRIC]` ölçüm koşusu | aşağıda | 20 dk oyun + 2 dk | Dört olay türü de logda; özet tablo çıktı |

## Kapı 0: Derle

```powershell
git fetch origin claude/determined-cannon-a8zts0
git checkout claude/determined-cannon-a8zts0
Stop-Process -Name UnrealEditor -ErrorAction SilentlyContinue
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64 Development "-Project=<proje yolu>\MyProject.uproject" -WaitMutex
powershell -File Scripts\RunAllChecks.ps1 -SkipBuild
```

Derleme hatasında olası yerler `PR1_VERIFY.md` §1 tablosunda. Yeni dosya bulunamazsa "Generate Visual Studio project files".

## Kapı 1: PR #1 doğrulaması

`PR1_VERIFY.md` §2 tablosundaki 1–18. adımlar (2 pencere, "Play As Listen Server"). Adım 14–18 çok oyunculu XP payı, parti bonusu, ganimet ve XP eğrisi içindir; bunlarda `[MMOMETRIC] ev=kill` satırlarını logdan oku.

## Kapı 2: İlk 7 test

`PLAYTEST_PRIORITY.md` §2 tablosu (doğuş + başlangıç paketi → temel döngü → uzak istemci pencereleri → spawner zemini → ticaret/depo/klan/arkadaş → `MMOTravel` → ölçüm). 7. test Kapı 3'tür. Komut ve tuş ayrıntıları `FIRST_PLAYTEST.md`'de.

## Kapı 3: `[MMOMETRIC]` koşusu

1. Editörü kapatıp aç (eski logu temizlemek için `Saved/Logs/MyProject.log` yeni başlar).
2. Yeni karakter (konsol komutu **kullanma**: `MMOGainXP`/`MMOGiveItem` ölçümü bozar), tek sınıf, 20 dk normal oyna: görev al/teslim et, öldür, sat/al, gerekirse öl.
3. Oyunu kapat, çalıştır:
   ```powershell
   python Scripts\PlaytestMetrics.py Saved\Logs\MyProject.log
   ```
4. Çıktıyı (seviye başına süre/öldürme/ölüm/altın) olduğu gibi sonuç şablonuna yapıştır. Model karşılaştırması: `python Scripts\ProgressionModel.py --max 30` (model, ölçüm değil).

Ölçüm satırları yalnız sunucuda (host) basılır. Logda hiç satır yoksa Kapı 3 **KALDI**: büyük olasılıkla istemci penceresinin logu okunuyor.

## Sonuç şablonu (kopyala, doldur, bana gönder)

```text
Tarih / commit (git rev-parse --short HEAD):
Kapı 0  Derleme: GEÇTİ | KALDI   RunAllChecks: GEÇTİ | KALDI
        Hata (ilk 5 satır):
Kapı 1  GEÇTİ | KALDI   Kalan adımlar (no + ne gördün):
Kapı 2  1:__ 2:__ 3:__ 4:__ 5:__ 6:__ 7:__   (G = geçti, K = kaldı, A = atlandı)
        Kalan testler (no + ne gördün):
Kapı 3  GEÇTİ | KALDI   Sınıf: ____  Ulaşılan seviye: __  Ölüm: __
        PlaytestMetrics.py çıktısı:
Log satırları ([MMODBG] / Error:, ilgili olanlar):
Genel his (hız, zorluk, kafa karıştıran yer):
```

Bulgular `PLAYTEST_PRIORITY.md` §3'e işlenir; geçen maddeler `PLAYTEST_CHECKLIST.md`'de `[x]` yapılır.
