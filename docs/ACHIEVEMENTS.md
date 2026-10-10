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
| `Private/Tests/MMOAchievementSelfTest.cpp`, `Scripts/TestAchievements.ps1` | native test (36 kontrol) ve calistirici |
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
- **Yukleme:** PlayerState adi gelmeden yuklenmez; o ana kadar gelen olaylar sayilmaz (ilk saniyelerde oldurme olmaz).

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
