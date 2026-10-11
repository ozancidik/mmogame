# Posta (2026-10-11)

Durum: **kodda, derlendi ve headless test edildi (41 kontrol, mutasyonla sinandi); oyun icinde (PIE) denenmedi.** Basarim ve gunluk gorevlerle ayni kalip,
Codex'in dosyalarindan bagimsiz yeni dosyalarla eklendi. 2026-10-11 QA duzeltmeleri (K1, O2, O5, D1, D3) asagida; bunlarin yeni test kontrolleri henuz yazilmadi.

## Ne yapar

- `/posta gonder <isim> [<miktar>g] <mesaj>`, `/posta` (bilgi + bekleyen sayisi). Ornek: `/posta gonder Ali 500g selam`.
- **Altin eki yalniz acik bicimde:** rakamlar + `g` soneki (`500g`, `500G`). Duz sayi mesajin parcasidir: `/posta gonder Ali 100 kere tesekkurler`
  altin GONDERMEZ, mesaj "100 kere tesekkurler" olur (eski surum bunu 100 altin sayiyordu, QA D1).
- Bosluklu ad `_` ile yazilir: "Ali Veli" adli oyuncuya `/posta gonder Ali_Veli ...` (ad temizleme boslugu `_` yapar; kutu ve kayit yuvasi ayni).
- Alici **cevrimdisi** olabilir: posta `Saved/MMOMail/MMOMail_<temiz-ad-kucuk-harf>.json` kutusuna yazilir (`MMOMail_` oneki: Windows'ta
  `con`/`nul`/`aux`/`com1` gibi ayrilmis adlar tek basina dosya adi olamaz, QA O5); alici girince ve her 10 sn'de kutusunu bosaltir, altin ekini alir.
- **Gonderim ucreti = altin sink'i:** 10 altin + ekin %2'si (yukari yuvarla). Altin ek en fazla 1.000.000, mesaj 200 karakter, aliciya en fazla 20 posta, iki gonderim arasi 5 sn.
  Sohbet girdisi zaten ~200 karakterde kesildigi icin "mesaj cok uzun" hatasi sohbetten pratikte gorulmez; sinir depoda ve native testte zorlanir.

## Dosyalar

| Dosya | Gorev |
|---|---|
| `Public/Mail/MMOMailStore.h`, `Private/Mail/MMOMailStore.cpp` | disk deposu, ad temizleme, dogrulamali JSON, `TakeAll`, `Replace`, yarim yazim (.tmp) kurtarma |
| `Public/Components/MMOMailComponent.h`, `Private/Components/MMOMailComponent.cpp` | gonderim kurallari, toplama, `ParseSendArgs`, `/posta` komutlari |
| `Private/Tests/MMOMailSelfTest.cpp`, `Scripts/TestMail.ps1` | native test (89 kontrol: ayristirma tablosu, K1 kayit sirasi, kilit/ad degisimi, B3, C1, O2 tavan, .tmp kurtarma dahil) |

Baglanti: `MMOCharacter.h/.cpp` (alt nesne, `IsTravelSaveCommitted()` erisimcisi), `MMOPlayerController.cpp` (`/posta` sohbet komutu, `/yardim`).

## Tasarim kararlari

- **Altin asla kopyalanmaz (QA K1):** posta kutusu diske hemen yazilir, karakter kaydi ise eskiden 60 sn'lik otomatik kayda kaliyordu; arada sunucu
  cokerse gonderen eski altiniyla, alici postayla geri gelirdi. Yeni sira (ticaretin iki tarafi hemen kaydetme deseni):
  1. kayit yuklenmemis (`IsInitialLoadComplete`) veya bolge gecisi kaydi onaylanmis (`IsTravelSaveCommitted`) gonderen reddedilir;
  2. ucret + ek dusulur; 3. gonderen karakter **senkron kaydedilir** (`SaveCharacterForTravel`; bolge gecisi kaydi ucustaysa da false doner) -
     basarisizsa iade, kutuya yazilmaz, "Posta su an gonderilemedi"; 4. kutuya yazilir - basarisizsa iade + tekrar kayit.
  En kotu durumda (kayit sonrasi kutu yazimindan once cokme ya da iade kaydi basarisiz) altin **kaybolur**, kopyalanmaz.
- **Toplama (QA O2):** kutu yuklenir; eki altin tavanini (`UMMOInventoryComponent::MaxGold`) asacak posta **kutuda kalir** (oyuncuya bir kez uyari);
  kalanlarla kutu once diske yazilir (`Replace`), sonra altin verilir, sonra karakter kaydedilir. Bolge gecisi onaylanmissa toplama atlanir
  (posta yeni bolgede alinir). Dongu ortasinda sahip duserse teslim edilmeyenler kutuya geri yazilir (kutu bu arada dolduysa loglanip kaybolur).
- **Ad degisimi (guvenlik B2):** canli ad (`GetSaveSlotName`) karakterin `GetActiveSaveSlot()` degerinden farkliysa (ad degisti, karakter yeni ada
  henuz yazilmadi) gonderim reddedilir ("Adin degisti: posta icin yeniden gir") ve toplama atlanir: eski/yeni yuvada ayni altinla catallama olmasin.
- **Ad tarama (B3):** "alici yok" ve "altin yetersiz" ayni genel mesaji verir, altin kontrolu varlik kontrolunden once yapilir ve basarisiz
  alici denemesi de 5 sn cooldown baslatir.
- **Hedef adi (C1):** temizleme adi sessizce degistiremez (`Al!i` -> `Ali` reddedilir; yalniz bosluk = `_`). Onay mesaji gercek kutu adini yazar.
- **Disk yazimi (A4):** `.tmp` -> `Move` basarisizsa dogrudan nihai yola yazma denenir. Ikisi de basarisizsa: ana dosya duruyorsa `.tmp` silinir
  ve yazim basarisiz sayilir; ana dosya yok ve `.tmp` tamsa yazim gerceklesmis sayilir (bir sonraki `Load` onu geri koyar - aksi halde iade edilen
  posta kurtarmayla geri gelir ve altin kopyalanirdi).
- **Kayit yuklenmeden altin verilmez:** `AMMOCharacter::IsInitialLoadComplete()` sart; yoksa karakter kaydi yuklenirken altin ustune yazilir, kutu bosaldigi icin altin yok olurdu.
- **Yarim yazim kurtarma (QA D3):** atomik yazim `.tmp` -> `Move` hedefi once siler; arada cokme olursa yalniz `.tmp` kalir. `Load` ana dosya yoksa `.tmp`'yi geri koyar.
- **Alici varligi:** cevrimici oyuncu ya da `Saved/SaveGames/MMOSave_<ad>.sav` dosyasi. Bu, yerel depoya baglidir.
  **Not (QA D4):** dogru yol `IMMOCharacterStore::Exists` kullanmaktir (bulut magazaya gecince dosya kontrolu yanlis "yok" der); henuz degistirilmedi.
- **Kimlik = oyuncu adi** (projedeki bilinen acik, `SECURITY_AUDIT.md`): baskasinin adini kapan, onun postasini okur ve altinini alir. Hesap sistemi gelene kadar bu risk gecerli.
- Replikasyon yok; mobda izleme yok (AMMOCharacter mobun da tabani).

## Bilinen sinirlar (durust)

- **PIE'da, /posta komutunda, PlayerState adinda, 10 sn zamanlayicisinda ve iki gercek istemcide denenmedi.** Test tek senkron karede mantik, depo ve dosya sistemini dogrular.
- Esya eki yok (yalniz altin): esya aktarimi takas bilesenindeki sertlestirilmis yolu gerektirir; kasitli disarida birakildi.
- Her gonderim gonderenin karakterini senkron diske yazar (5 sn cooldown sinirlar); bulut magazada gecikme olursa yeniden bakilmali.
- Acik tasarim sorulari (QA D2, D6, D7, D8, D12): kutu doldurma tacizi (20 postalik kutuyu ucuz postayla doldurma) vb. - `DECISIONS_NEEDED.md` adayi.
- Sayilar placeholder (ucret, limitler).

## Oyun ici ilk test listesi

1. Iki oyuncu: A, B'ye `/posta gonder B 500g merhaba`; A'nin altini 500 + 20 azaldi mi; B 10 sn icinde mesaji ve +500 altini aldi mi.
2. `/posta gonder B 100 kere tesekkurler`: altin GITMEMELI (yalniz 10 ucret), B mesaji "100 kere tesekkurler" olarak almali.
3. B cevrimdisiyken gonder, B girince al; `Saved/MMOMail/MMOMail_<ad>.json` olusup bosaliyor mu.
4. Olmayan isim, kendine, 5 sn icinde ikinci gonderim, yetersiz altin: dogru hata mesajlari, altin dusmuyor mu.
5. Alici girerken (yuklenme sirasinda) gelen posta: altin kayit yukleme sonrasi dogru ekleniyor mu (kaybolmuyor mu).
6. Gonderdikten hemen sonra editoru/sunucuyu zorla kapat (gorev yoneticisi): yeniden acinca A'nin altini dusmus VE B'nin kutusunda posta var mi (cift altin yok).
7. "Ali Veli" adli oyuncuya `/posta gonder Ali_Veli 10g selam` ulasiyor mu.
