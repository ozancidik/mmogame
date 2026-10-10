# Posta (2026-10-11)

Durum: **kodda, derlendi ve headless test edildi (39 kontrol, mutasyonla sinandi); oyun icinde (PIE) denenmedi.** Basarim ve gunluk gorevlerle ayni kalip,
Codex'in dosyalarindan bagimsiz yeni dosyalarla eklendi.

## Ne yapar

- `/posta gonder <isim> [altin] <mesaj>` (isimde bosluk yok), `/posta` (bilgi + bekleyen sayisi).
- Alici **cevrimdisi** olabilir: posta `Saved/MMOMail/<temiz-ad>.json` kutusuna yazilir; alici girince ve her 10 sn'de kutusunu bosaltir, altin ekini alir.
- **Gonderim ucreti = altin sink'i:** 10 altin + ekin %2'si (yukari yuvarla). Altin ek en fazla 1.000.000, mesaj 200 karakter, aliciya en fazla 20 posta, iki gonderim arasi 5 sn.
- Gonderen once altin+ucreti oder, sonra kutuya yazilir; yazma basarisizsa **iade** edilir.

## Dosyalar

| Dosya | Gorev |
|---|---|
| `Public/Mail/MMOMailStore.h`, `Private/Mail/MMOMailStore.cpp` | disk deposu, ad temizleme, dogrulamali JSON, `TakeAll` (once bosalt) |
| `Public/Components/MMOMailComponent.h`, `Private/Components/MMOMailComponent.cpp` | gonderim kurallari, toplama, `/posta` komutlari |
| `Private/Tests/MMOMailSelfTest.cpp`, `Scripts/TestMail.ps1` | native test (39 kontrol) |

Baglanti: `MMOCharacter.h/.cpp` (alt nesne), `MMOPlayerController.cpp` (`/posta` sohbet komutu).

## Tasarim kararlari

- **Once bosalt, sonra ode:** `TakeAll` kutuyu diske bos yazar, sonra cagiran altini verir; cokme/yeniden giris ayni postayi iki kez odemez (bunun karsiligi: yazma-sonrasi cokmede
  posta kaybolabilir; cift odemeden daha az zararli secildi).
- **Kayit yuklenmeden altin verilmez:** `AMMOCharacter::IsInitialLoadComplete()` sart; yoksa karakter kaydi yuklenirken altin ustune yazilir, kutu bosaldigi icin altin yok olurdu.
- **Alici varligi:** cevrimici oyuncu ya da `Saved/SaveGames/MMOSave_<ad>.sav` dosyasi. Bu, yerel depoya baglidir; bulut magazaya (`IMMOCharacterStore`) gecince degistirilmeli.
- **Kimlik = oyuncu adi** (projedeki bilinen acik, `SECURITY_AUDIT.md`): baskasinin adini kapan, onun postasini okur ve altinini alir. Hesap sistemi gelene kadar bu risk gecerli.
- Replikasyon yok; mobda izleme yok (AMMOCharacter mobun da tabani).

## Bilinen sinirlar (durust)

- **PIE'da, /posta komutunda, PlayerState adinda, 10 sn zamanlayicisinda ve iki gercek istemcide denenmedi.** Test tek senkron karede mantik, depo ve dosya sistemini dogrular.
- Esya eki yok (yalniz altin): esya aktarimi takas bilesenindeki sertlestirilmis yolu gerektirir; kasitli disarida birakildi.
- Isimde bosluk olan oyuncular `/posta gonder` ile adreslenemez (ad bosluksuz sozcuk olarak okunur).
- Sayilar placeholder (ucret, limitler).

## Oyun ici ilk test listesi

1. Iki oyuncu: A, B'ye `/posta gonder B 500 merhaba`; A'nin altini 500 + 20 azaldi mi; B 10 sn icinde mesaji ve +500 altini aldi mi.
2. B cevrimdisiyken gonder, B girince al; `Saved/MMOMail/<ad>.json` olusup bosaliyor mu.
3. Olmayan isim, kendine, 5 sn icinde ikinci gonderim, 201 karakter, yetersiz altin: dogru hata mesajlari, altin dusmuyor mu.
4. Alici girerken (yuklenme sirasinda) gelen posta: altin kayit yukleme sonrasi dogru ekleniyor mu (kaybolmuyor mu).
