# Basarimlar ve unvanlar (2026-10-11)

Durum: **kodda, derlendi/test edildi (headless); oyun icinde (PIE) denenmedi.** Bu alan daha once hic yoktu; Codex'in dosyalarindan bagimsiz,
yeni dosyalarla eklendi.

## Ne yapar

- 28 basarim, 6 tur: toplam oldurme, belirli mob (boss/elit), seviye, tamamlanan gorev sayisi, cuzdanda tepe altin, olum sayisi.
- Acilinca: `Basarim: <ad> (+N altin)  - unvan: <unvan>` bildirimi, kucuk altin odulu ve unvan. **XP odulu yok** (ilerleme egrisi bozulmasin).
- Unvanlar: `/basarim unvan <ad>` ile secilir, `/basarim unvan` ile kaldirilir. Secili unvan `ActiveTitle` olarak herkese replike olur
  (HUD isim etiketinde henuz gosterilmiyor).
- Sohbet: `/basarim` ozet verir (acilan sayisi, toplam oldurme/olum, unvanlar, en yakin 5 hedef ve ilerleme).

## Dosyalar

| Dosya | Gorev |
|---|---|
| `Public/Achievements/MMOAchievementTypes.h`, `Private/Achievements/MMOAchievementDatabase.cpp` | veri tablosu (`MakeAch`), yeni satir = tek satir |
| `Public/Components/MMOAchievementComponent.h`, `Private/Components/MMOAchievementComponent.cpp` | sunucu-otoriteli izleme, odul, kalicilik, komutlar |
| `Private/Tests/MMOAchievementSelfTest.cpp`, `Scripts/TestAchievements.ps1` | native test (61 kontrol: tablo/mantik/JSON + GUID'li yuvalarla disk kaliciligi, B1 ad degisimi, LoadSerial, A5, C3, O1, .tmp) ve calistirici |
| `Scripts/ValidateData.py`, `Scripts/TestValidateData.py` | tablo denetimi + 4 negatif senaryo |

Mevcut dosyalarda yalniz kucuk baglanti: `MMOCharacter.h/.cpp` (alt nesne), `MMOQuestComponent.cpp::NotifyMobKilled` (olduruldu sinyali,
gorev ilerlemesinden SONRA), `MMOPlayerController.cpp` (`/basarim` sohbet komutu).

## Tasarim kararlari

- **Kimlik/kalicilik:** karakter kayit semasina (`MMOSaveGame.h`, korunan yol) dokunmamak icin ayri dosya: `Saved/MMOAchievements/<kayit-yuvasi>.json`.
  Kayit yuvasi = oyuncu adi (projedeki bilinen kimlik acigi aynen gecerli, `SECURITY_AUDIT.md`). JSON yuklenirken dogrulanir: negatifler 0'a,
  asiri buyukler tavana cekilir, bilinmeyen/yinelenen kimlikler ve sahip olunmayan unvan atilir; bozuk dosya `.bad` olarak ayrilir.
- **Yeniden giris korumasi:** yeni acilanlar once belirlenir, `Unlocked` ve disk guncellenir, SONRA oduller dagitilir (altin callback'i bileseni
  tekrar cagirsa da ayni basarim ikinci kez verilmez; `bEvaluating` guard).
- **Mob:** `AMMOCharacter` mobun da tabani oldugu icin bilesen mobda izleme yapmaz ve replikasyonu kapatir.
- **Yukleme / yuva (2026-10-11 QA O1/O3/D5/A6 + guvenlik B1):** karakter kaydi yuklenmeden (`IsInitialLoadComplete`) yuklenmez; yuva oyuncunun
  canli adi DEGIL, karakterin `GetActiveSaveSlot()` degeridir ve dosya yolu yukleme aninda sabitlenir. Boylece:
  - EndPlay'de PlayerState kopmus olsa da dogru dosyaya yazilir (eski surum `MMOSave_Default.json`'a yaziyordu); yuklenmemis durum hic yazilmaz.
  - Ad degistirmek bos bir dosya yukletmez (eskiden `MMOSetName Yeni1, Yeni2...` ile seviye/gorev basarim altini tekrar tekrar alinabiliyordu).
    Karakter yeni ada kaydedilince (ActiveSaveSlot degisir) basarimlar da yeni yola TASINIR, sifirlanmaz.
  - Karakter diskten yeniden yuklenirse (`MMOLoad`, `AMMOCharacter::GetLoadSerial()` artar) once eski durum eski yola yazilir, sonra yeniden yuklenir.
  - Oldurme/olum olaylari da yuklemeyi tetikler (zamanlayicidan once gelen olay kaybolmaz). Yarim kalan yazim (`.tmp`, ana dosya yok) yuklemede geri konur.
  - **2026-10-11 (K-1 F2, 2. tur; derlendi, mevcut 61 kontrol gecti, yeni davranislar icin native test YOK):**
    - EndPlay, yuklu ise `SaveNow`'dan once `TryLoad()` cagirir: ad degistirip (sonraki 5 sn taramasindan once) cikan karakterin dosyasi da
      yeni yuvaya tasinir. Sira motor kodundan dogrulandi: karakter kaydi (`UnPossessed` / `AMMOCharacter::EndPlay`) bilesen EndPlay'inden ONCE.
    - `.tmp` var ama geri konamiyorsa ya da ana dosya var ama okunamiyorsa yukleme ERTELENIR (`TryLoad` false, 5 sn sonra yeniden); bos dosya
      sanilip odul tekrar verilmez, diskteki durumun ustune yazilmaz.
    - Lv>1 karakterin dosyasi yoksa (ya da bozuktu): o an saglanan seviye/gorev/altin basarimlari ODULSUZ acik isaretlenip yazilir. Bedeli:
      basarim sisteminden once Lv>1 olmus kayitli karakterler bu basarimlarin altinini geriye donuk ALMAZ (unvanlar acilir). Denge karari
      kullanicinin; istenmezse `MarkCurrentProgressUnlockedSilently` cagrisi kaldirilir (uzak istemci yolu F1 ad kilidiyle zaten kapali).
- **Disk yazilamazsa odul yok:** acilma diske yazilamazsa `Unlocked` geri alinir ve odul verilmez; 5 sn sonra yeniden denenir. Ayni unvan
  tekrar secilirse diske yazilmaz.

## Bilinen sinirlar (durust)

- **PIE'da denenmedi.** Test tek senkron karede mantik, tablo ve JSON'u dogrular; PlayerState, diske yazma, ag ve zamanlayici kaniti degildir.
- Kalicilik yalniz yerel sunucu dosyasi; bulut magaza (`IMMOCharacterStore`) ile entegre degil, hesap sistemi gelince tasinmali.
- HUD penceresi ve isim etiketi yok (yalniz sohbet satirlari); tum liste degil ozet gosterilir.
- Sayilar placeholder: altin odulu toplami 65.050 (Lv58+ tek seviyenin geliri milyonlarda, ekonomiyi bozmaz); hedefler (10/100/500/2000/10000 oldurme) olcum ister.
- Tur eklemek icin `EMMOAchievementKind`'e deger + `IsSatisfied/ProgressOf` + testte bir kontrol gerekir.

## Oyun ici ilk test listesi

1. Birkac mob oldur, `/basarim`: toplam oldurme artiyor mu, "En yakin hedefler" dogru mu.
2. 10. olduruste `Basarim: Ilk Kan (+50 altin)  - unvan: Acemi` bildirimi ve altin +50 mi.
3. `/basarim unvan Acemi`, sonra `/basarim unvan`: secildi/kaldirildi mesajlari.
4. Cik-gir (ayni ad): sayaclar ve unvan korunuyor mu; `Saved/MMOAchievements/<ad>.json` olusuyor mu.
5. Iki oyunculu oturumda ikinci oyuncunun `ActiveTitle` degeri istemcide gorunuyor mu (replikasyon; HUD yok, log/BP ile bak).
