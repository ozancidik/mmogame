# Oyun içi test runbook'u (tek sayfa, sıralı kapılar)

**Tarih:** 2026-10-05. Yerelde (Windows, UE 5.7) ilk oturumda **yukarıdan aşağı** izlenecek tek sayfa. Her kapı bir öncekinin geçmesini ister: bir kapı **KALDI** ise dur, sonucu aşağıdaki şablonla yaz, sonraki kapıya geçme. Ayrıntılar bağlantılı belgelerde; bu sayfa yalnız sırayı ve geçme ölçütünü tutar.

| Kapı | Ne | Ayrıntı | Süre [TAHMİN] | Geçme ölçütü |
|---|---|---|---|---|
| 0 | Derle + otomatik kontroller | `PR1_VERIFY.md` §1 | 10–20 dk | `Result: Succeeded` ve `RunAllChecks.ps1` hatasız |
| 1 | PR #1 davranış testleri 1–18 | `PR1_VERIFY.md` §2 | ≈20 dk | 18 satırın hepsi "Beklenen" sütunuyla aynı |
| 2 | İlk 7 oyun içi test | `PLAYTEST_PRIORITY.md` §2 (ayrıntılı adımlar `FIRST_PLAYTEST.md`) | ≈45 dk | 1–3 kesin geçmeli; 4–7'de veri kaybı/kopyalama yok |
| 3 | `[MMOMETRIC]` ölçüm koşusu | aşağıda | 20 dk oyun + 2 dk | Yapılan eylemlerin olayları logda; görev kabul/teslim ve özet tablo çıktı |

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
4. Çıktıyı (seviye başına süre/öldürme/ölüm/altın/nominal görev XP'si; görev kabul→teslim süresi) olduğu gibi sonuç şablonuna yapıştır. Model karşılaştırması: `python Scripts\ProgressionModel.py --max 30` (model, ölçüm değil).

**7 Ekim görev ölçüm notu:** `quest_accept`/`quest_claim` yalnız başarılı sunucu
işlemleridir. Reddedilen/tekrarlanan teslim olay üretmez. `XP(quest*)` veri
tablosundaki nominal ödüldür; net XP bakiyesi veya seviye tavanındaki gerçek kazanç
değildir. Altın ayrıca `ev=gold` ile sayılır. Kabul kaydı aynı log/süreç/oyuncu adı
altında yoksa `ort_sure=?`; önceki oturumdan yüklenen görevin süresi tahmin edilmez.
Eski loglarda görev sütununun 0 olması sıfır görev kazancı kanıtı değildir.
Bu araç henüz kalıcı hesap kimliğiyle farklı oturumları birleştirmez.

**İlk görev eylemi ölçümü:** Sunucu `first_quest_accept` / `first_quest_claim`
olaylarını başarılı kod yolunda birer kez yazar. `scope=controller`, `sid`
rastgele geçici oturum kimliği, `start` controller BeginPlay zamanı, `elapsed`
bu başlangıçtan geçen saniyedir. Yeni pawn aynı controller altında tekrar olay
üretmez; controller seamless aktarımında durum taşınır. Yeniden bağlanma/yeni
controller yeni sid demektir; kalıcı karakterin ilk görevi ölçülmez. Kaydedilmiş
aktif görev teslimi, bu oturumun ilk kabulünden önce ilk teslim olabilir.
Parser sid bazında ilk süreyi korur; tekrar/geçersiz kayıtları sayar. Eksik olay
`sure=?`, eski logda ilk olay yoksa süre bilinmiyor; sıfır varsayılmaz. `--who`
oyuncu adını filtreler, ad değiştiren oyuncunun farklı ad altındaki satırlarını
birleştirmez. İlk olaylar görev adedi, XP ve altını tekrar toplamaz.

**İlk gerçek ganimet:** `first_loot` aynı controller sid/start/elapsed kapsamını
kullanır. Yalnız pickup'ın gerçekten verdiği ilk pozitif miktar kaydedilir:
`kind=gold|item|equipment item=<id> amount=<teslim>`. Kısmi sarf toplaması ilk
olaydır; kalan yerde olabilir. Dolu çanta/yığın, ölü/rezervasyona yetkisiz
karakter ve sıfır altın kazancı ilk olay değildir. İlk altın mı, iksir mi,
ekipman mı olduğundan bağımsız olarak sid başına tek loot olayı vardır.
Başlangıç paketi/görev/satıcı/debug verme bu olayı tetiklemez. Ekipman loot'u
kuşanma değildir; onun ölçümü açık. Eski logda eksik loot `sure=?`; first
satırı normal `gold` akışını veya XP/görev sayısını yeniden toplamaz.

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

Kapı 3 çıktısı, `docs/DECISIONS_NEEDED.md`'deki 7 kararın (XP hızı, ölüm cezası, altın sink'i, hız hedefi vb.) her birinin "Gereken veri" satırını besler; karar vermeden önce o sayfaya bak.
