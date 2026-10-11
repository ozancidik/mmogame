# Gunluk gorevler (2026-10-11)

Durum: **kodda, derlendi ve headless test edildi (53 kontrol); oyun icinde (PIE) denenmedi.** `docs/ACHIEVEMENTS.md` ile ayni kalip, Codex'in
dosyalarindan bagimsiz yeni dosyalarla eklendi.

## Ne yapar

Her **UTC gunu** icin 3 gorev, gece yarisi UTC'de yenilenir:

| Gorev | Hedef (seviye L) | Odul |
|---|---|---|
| Gunluk Avci: herhangi bir canavar | 15 + L/2 | 15 x L altin, sonraki seviye esiginin %2'si kadar XP |
| Uygun Av: seviyene uygun canavar (mob seviyesi [L-8, L+3]) | 10 + L/4 | ayni |
| Gorev Adami: 1 gorev tamamla | 1 | iki kati |

- Tamamlaninca odul **otomatik** verilir (`Gunluk gorev tamam! (+N altin, +M XP)`). `/gunluk` listeyi gosterir.
- Hedef ve oduller gorev olusturuldugu andaki seviyeye sabitlenir; gun icinde seviye atlamak degistirmez.
- Toplam gunluk XP ~%8 seviye esigi (Lv80'e giden ~11 saatlik yolun cok kucuk bir parcasi), altin 15 x L x 4: ilerleme egrisini ve ekonomiyi bozmaz.

## Dosyalar

`Public/Components/MMODailyComponent.h`, `Private/Components/MMODailyComponent.cpp` (bilesen, saf mantik, JSON), `Private/Tests/MMODailySelfTest.cpp` +
`Scripts/TestDaily.ps1` (native test, 53 kontrol; kalicilik/B1/LoadSerial/A5/.tmp dahil). Baglanti: `MMOCharacter.h/.cpp` (alt nesne + `IsInitialLoadComplete()` erisimcisi),
`MMOQuestComponent.cpp::NotifyMobKilled` (olduruldu sinyali, basarimlardan sonra), `MMOPlayerController.cpp` (`/gunluk`).

## Tasarim kararlari

- **Replikasyon yok:** durum yalniz sunucuda ve sohbet komutuyla gorunur; ag maliyeti sifir. HUD gerekirse sonradan replike edilir.
- **Gorev referansi kayit yuklendikten sonra:** "gorev tamamla" hedefi, gun basindaki tamamlanmis gorev sayisini referans alir. Bilesen
  `AMMOCharacter::IsInitialLoadComplete()` olmadan yuklenmez; yoksa kayit yuklenince sayi sicrar ve gorev yanlis tamamlanirdi.
- **Once kaydet, sonra odul:** tamamlanan gorevler isaretlenip diske yazilir, sonra altin/XP verilir (odul callback'i bileseni tekrar cagirsa da iki kez odenmez).
- **Kalicilik:** `Saved/MMODaily/<kayit-yuvasi>.json`; karakter kayit semasina (`MMOSaveGame.h`) dokunulmadi. Yuklemede dogrulama: tam 3 gorev, bilinen tur,
  hedef 1..10000, ilerleme [0, hedef], oduller [0, 1e7]; gecersizse dosya `.bad` olarak ayrilir ve gorevler yeniden olusur.
- **Yuva (2026-10-11 QA O1/O3/D3 + guvenlik B1):** yuva oyuncunun canli adi DEGIL, karakterin `GetActiveSaveSlot()` degeridir; dosya yolu
  yukleme aninda sabitlenir (EndPlay'de `MMOSave_Default.json`'a yazma yok). Ad degistirmek bos dosya yukletip gunlukleri tekrar aldirmaz;
  karakter yeni ada kaydedilince durum yeni yola tasinir. Karakter diskten yeniden yuklenirse (`MMOLoad`) eski durum eski yola yazilir, yeniden
  yuklenir ve `QuestBaseline` bugunku ilerleme korunarak simdiki gorev sayisina gore yeniden alinir. Yarim kalan `.tmp` yazimi yuklemede geri konur.
- **2026-10-11 (K-1 F2, 2. tur; derlendi, mevcut 53 kontrol gecti, yeni davranislar icin native test YOK):** EndPlay, yuklu ise `SaveNow`'dan
  once `TryLoad()` cagirir (ad degistirip hemen cikan karakterin dosyasi da yeni yuvaya tasinir; karakter kaydi bilesen EndPlay'inden ONCE
  calisir, motor kodundan dogrulandi). `.tmp` geri konamiyorsa ya da ana dosya okunamiyorsa yukleme ERTELENIR (bugunun gorevleri sifirdan
  acilmaz, dosyanin ustune yazilmaz). Basarimdaki "dosya yoksa odulsuz acik say" savunmasi gunlukte YOK: kayip dosya ayni gunun gorevlerini
  yeniden acar (uzak istemci yolu `SECURITY_AUDIT.md` K-1 F1 ad kilidiyle kapali; host/dosya silme yolu acik).
- **Disk yazilamazsa odul yok:** tamamlanma diske yazilamazsa `bDone` geri alinir, odul verilmez, sonraki degerlendirmede yeniden denenir.
- Mobda izleme yok (AMMOCharacter mobun da tabani).

## Bilinen sinirlar (durust)

- PIE'da, gercek saatte (gece yarisi gecisi), diske yazmada ve PlayerState zamanlamasinda denenmedi; testte gun anahtari zorlanir.
- Gun, sunucunun UTC saatine gore (Turkiye'de 03:00'te yenilenir); oyuncuya yerel saat gosterilmez.
- Oyuncu adi kimligi: baskasinin adiyla girip gunluk odulu sifirlayamaz ama kendi dosyasini silip tekrar alabilir (yerel dosya; hesap sistemi yok).
- Hedef/odul sayilari placeholder.

## Oyun ici ilk test listesi

1. Yeni karakterle birkac saniye bekle, `/gunluk`: 3 gorev ve hedefler goruluyor mu.
2. Uygun seviyede mob oldur: "Uygun Av" sayaci, cok dusuk seviye mobda artmiyor mu.
3. Hedefe ulasinca bildirim ve altin/XP artisi; ikinci kez odul gelmiyor mu.
4. Bir gorev tamamla (odulu al): "Gorev Adami" tamamlaniyor mu; onceden tamamlanmis gorevler sayilmiyor mu.
5. Cik-gir: sayaclar korunuyor mu; sistem saatini ertesi gune al (UTC): gorevler yenileniyor mu.
