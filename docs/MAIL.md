# Posta (2026-10-11)

Durum: **kodda, derlendi ve headless test edildi (`Scripts/TestMail.ps1` 144 kontrol, `76ceed9`; 2026-10-14'te `1f4a557` uzerinde GECTI; mutasyonla sinandi); oyun icinde (PIE) denenmedi.** Basarim ve gunluk gorevlerle ayni kalip,
Codex'in dosyalarindan bagimsiz yeni dosyalarla eklendi. 2026-10-11 QA/guvenlik duzeltmeleri (K1, O2, O5, D1, D3; tur 3 `2bb2023`: `.bak` yedekli yazim,
bozuk kutuda false + iade, tekrar denemesiz kurtarma + 60 sn geri cekilme; tur 4 `d031b90`: tekrar denemesiz yazim `Move`'u, ilk-posta `.bad` politikasi,
`BoxFull`/`StoreError` sogumasi, ASCII ad kurali; tur 5 `f70e5ad`: ayrilmis adlar; Codex O69 (`9855256` icinde) + tur 6 `1f4a557`: iade muhasebesi) asagida;
native test kontrolleri 41 -> 89 -> 104 -> 130 -> 144 ile bunlari kapsar (144 = 130 + 11 tur 5 + 3 iade hesabi (O69, `9855256`), `76ceed9` mesaji). Codex'in ayri okuma hatasi testi
`Scripts/TestMailReadFailure.ps1` (O68, 24 kontrol) ve iade testi `Scripts/TestMailRefund.ps1` (`MMOMailRefundSelfTest`, 176 kontrol; ana checkout'u reddeder,
kayitli izole git worktree ister, UE/UBT calisirken erteler, cikis 3) `RunAllChecks.ps1` adimi degil.

## Ne yapar

- `/posta gonder <isim> [<miktar>g] <mesaj>`, `/posta` (bilgi + bekleyen sayisi). Ornek: `/posta gonder Ali 500g selam`.
- **Altin eki yalniz acik bicimde:** rakamlar + `g` soneki (`500g`, `500G`). Duz sayi mesajin parcasidir: `/posta gonder Ali 100 kere tesekkurler`
  altin GONDERMEZ, mesaj "100 kere tesekkurler" olur (eski surum bunu 100 altin sayiyordu, QA D1).
- Bosluklu ad `_` ile yazilir: "Ali Veli" adli oyuncuya `/posta gonder Ali_Veli ...` (ad temizleme boslugu `_` yapar; kutu ve kayit yuvasi ayni).
  Tur 4'ten beri ad yalniz ASCII (`MMONameRules::SlotKey`, `MMOMailStore.cpp:96-102`); Turkce harfli eski kutular icin migrasyon yok.
- Alici **cevrimdisi** olabilir: posta `Saved/MMOMail/MMOMail_<temiz-ad-kucuk-harf>.json` kutusuna yazilir (`MMOMail_` oneki: Windows'ta
  `con`/`nul`/`aux`/`com1` gibi ayrilmis adlar tek basina dosya adi olamaz, QA O5); alici girince ve her 10 sn'de kutusunu bosaltir, altin ekini alir.
- **Gonderim ucreti = altin sink'i:** 10 altin + ekin %2'si (yukari yuvarla). Altin ek en fazla 1.000.000, mesaj 200 karakter, aliciya en fazla 20 posta, iki gonderim arasi 5 sn.
  Sohbet girdisi zaten ~200 karakterde kesildigi icin "mesaj cok uzun" hatasi sohbetten pratikte gorulmez; sinir depoda ve native testte zorlanir.

## Dosyalar

| Dosya | Gorev |
|---|---|
| `Public/Mail/MMOMailStore.h`, `Private/Mail/MMOMailStore.cpp` | disk deposu, ad temizleme, dogrulamali JSON, `TakeAll`, `Replace`, `.bak`/`.tmp`/`.bad` durum makinesi ve kurtarma |
| `Public/Components/MMOMailComponent.h`, `Private/Components/MMOMailComponent.cpp` | gonderim kurallari, toplama, `ParseSendArgs`, `/posta` komutlari |
| `Private/Tests/MMOMailSelfTest.cpp`, `Scripts/TestMail.ps1` | native test (144 kontrol: ayrilmis ad alici/gonderen (tur 5), hediye/harcama callback'li iade hesabi (O69), ayristirma tablosu, K1 kayit sirasi, kilit/ad degisimi, B3, C1, O2 tavan, .tmp kurtarma, iade callback'leri, ASCII ad/yol, `.bak` alinamazsa yazim yok, Move + dogrudan yazim basarisizsa kutu bayt-bayt ayni ve < 1 sn, `.bak` > `.tmp` kurtarma sirasi, bozuk ana + `.bak`, ilk-posta `.tmp` politikasi, 60 sn geri cekilme, bozuk/okunamayan alici kutusu, `BoxFull`/`StoreError` sogumasi, fee_clamp); calistirici `Error moving file` satiri 0 bekler |

Baglanti: `MMOCharacter.h/.cpp` (alt nesne, `IsTravelSaveCommitted()` erisimcisi), `MMOPlayerController.cpp` (`/posta` sohbet komutu, `/yardim`).

## Tasarim kararlari

- **Altin asla kopyalanmaz (QA K1):** posta kutusu diske hemen yazilir, karakter kaydi ise eskiden 60 sn'lik otomatik kayda kaliyordu; arada sunucu
  cokerse gonderen eski altiniyla, alici postayla geri gelirdi. Yeni sira (ticaretin iki tarafi hemen kaydetme deseni):
  1. kayit yuklenmemis (`IsInitialLoadComplete`) veya bolge gecisi kaydi onaylanmis (`IsTravelSaveCommitted`) gonderen reddedilir;
  2. ucret + ek dusulur; 3. gonderen karakter **senkron kaydedilir** (`SaveCharacterForTravel`; bolge gecisi kaydi ucustaysa da false doner) -
     basarisizsa iade, kutuya yazilmaz, "Posta su an gonderilemedi"; 4. kutuya yazilir - basarisizsa iade + tekrar kayit.
  En kotu durumda (kayit sonrasi kutu yazimindan once cokme ya da iade kaydi basarisiz) altin **kaybolur**, kopyalanmaz.
- **Iade = gercekten dusulen miktar (2026-10-11, tur 3 guncel):** her dusum (ek, sonra ucret) yalniz bakiye o an yetiyorsa yapilir ve tam tutari
  `Deducted`'a eklenir (`MMOMailComponent.cpp:252-294`); iade = `Deducted`. Altin callback'i (Blueprint `OnGoldChanged` vb.) araya altin eklese ya da
  harcasa da iade fazladan altin uretmez. Tam ucret+ek dusulemediyse ya da bakiye net o kadar dusmediyse posta gonderilmez, dusulen kadar iade
  edilir (`:295-301`). Native test: `refund_spend_callback_*`, `refund_gift_callback_*`, `fee_clamp_refund_no_creation`.
  (Satirlar tur 3 icindir; guncel yerler asagidaki madde.)
- **Iade muhasebesi (guncel: Codex O69, `docs/tasks/MAIL_REFUND_ACCOUNTING_2026-10-11.md`, Sprint 1 commit'i `9855256` icinde geldi;
  + tur 6 `1f4a557`; PIE'da denenmedi):** her dusum
  `UMMOInventoryComponent::AddGoldFromWithAppliedDelta` ile yapilir; bu, `ApplyGoldInternal`'in gercekten uyguladigi (tavana/sifira
  kirpilmis) `AppliedDelta`'yi dondurur (`MMOInventoryComponent.cpp:402-449`). `Deducted` yalniz **bu denemenin** uygulanan dusumlerini
  toplar (`MMOMailComponent.cpp:310-319`); araya giren altin callback'inin harcamasi iade edilmez, verdigi hediye de geri alinmaz
  (**hediye korunur**). Tam ucret + ek dusulmediyse (`Deducted != Charge`) posta gonderilmez ve dusulen kadar iade edilir (`:325-331`);
  yani posta yalniz **tam odendiginde** gonderilir. Ornek (posta testi, beklenti `9855256`'da degisti; `MMOMailSelfTest.cpp:632-642`):
  10000 altin, 300 ek + 16 ucret, callback +1000 hediye; son bakiye 10684 ve bir 300-altinlik posta (eskiden posta reddedilir, bakiye
  <= 11000 beklenirdi). Callback bakiyeyi ucretten azina indirirse (1000, 500 ek, callback -495) posta kayittan once reddedilir, son bakiye tam 505.
  Iade, uygunluk kontrolunden (`GetEligibleInventory`, `:263-278`: sahip/envanter/posta bileseni ayni ve yasiyor, `LoadSerial` ve
  yuva ayni, kilit/ad degisimi yok) gecmezse **altin kaybolur, kopyalanmaz**; tur 6'dan beri bu sessiz degil:
  `[MMOMETRIC] ev=gold_lost who=<ad> src=posta-iade amount=N` + `LogMMOMail` uyarisi (`:294-303`). Olcumde `gold_lost` satiri
  gorulurse incelenmeli. Iade yolunun ayri native testi `TestMailRefund.ps1` (176) yalniz izole worktree'de kosar.
- **Ayrilmis adlar (tur 5, `f70e5ad`):** `FMMOMailStore::IsDeliverableName` (`MMOMailStore.cpp:104-107`) `MMONameRules::IsReservedName`
  (kanonik `default`, `player` + yalniz rakam) adlari reddeder: bu adlara posta gonderilemez (`MMOMailComponent.cpp:200`), bu adli
  gonderen gonderemez (`:191`) ve toplayamaz (`:368`). `PlayerOne`, `Player` + harf, `Gezgin2` teslim edilir.
- **Toplama (QA O2):** kutu yuklenir; eki altin tavanini (`UMMOInventoryComponent::MaxGold`) asacak posta **kutuda kalir** (oyuncuya bir kez uyari);
  kalanlarla kutu once diske yazilir (`Replace`), sonra altin verilir, sonra karakter kaydedilir. Bolge gecisi onaylanmissa toplama atlanir
  (posta yeni bolgede alinir). Dongu ortasinda sahip duserse teslim edilmeyenler kutuya geri yazilir (kutu bu arada dolduysa loglanip kaybolur).
- **Ad degisimi (guvenlik B2):** canli ad (`GetSaveSlotName`) karakterin `GetActiveSaveSlot()` degerinden farkliysa (ad degisti, karakter yeni ada
  henuz yazilmadi) gonderim reddedilir ve toplama atlanir: eski/yeni yuvada ayni altinla catallama olmasin. Mesaj (2026-10-11): "Adin degisti ve
  karakter yeni adla henuz kaydedilmedi: posta icin once MMOSave ile kaydet ya da eski adina don." (eski "yeniden gir" yanlis yonlendiriyordu).
  Not: uzak istemci artik kayitli karakterin adini degistiremez (`SECURITY_AUDIT.md` K-1 yeni yuz F1); bu durum yalniz henuz kaydedilmemis yeni
  karakterde ya da host'ta olusur. B2 korumasi tek basina catallamayi kapatmiyordu (yeni adla yeniden girince ad ve yuva eslesir); kapatan F1 kuralidir.
- **Ad tarama (B3):** "alici yok" ve "altin yetersiz" ayni genel mesaji verir, altin kontrolu varlik kontrolunden once yapilir ve basarisiz
  alici denemesi de 5 sn cooldown baslatir. Tur 4 (Y-NEW-5): alici VAR iken olusan `StoreError` (kutu okunamadi/bozuk) ve `BoxFull` retleri de
  cooldown'a yazilir (`MMOMailComponent.cpp:241-254`, `1f4a557`); aksi halde bu cevaplar damgasiz kalip cevrimdisi ad varligi sinirsiz hizda sorgulanabilirdi.
- **Hedef adi (C1):** temizleme adi sessizce degistiremez (`Al!i` -> `Ali` reddedilir; yalniz bosluk = `_`). Onay mesaji gercek kutu adini yazar.
- **Disk yazimi (A4, tur 3/4 guncel; `WriteBox`, `MMOMailStore.cpp:252-337`, `1f4a557`):** degismez kural: `false` donerse kutu yazimdan onceki haliyle
  kalir (ya da sonraki `Load`'da o hale doner), `true` = yeni icerik diskte. Sira:
  1. Ana dosya varsa `.bak` kopyasi alinir; alinamazsa **hic yazilmaz** (`false`, cagiran iade eder).
  2. Icerik `.tmp`'ye yazilir. Olmazsa `.tmp` ve `.bak` silinir, `false`. Ilk postada (ana dosya yok) silinemeyen `.tmp` `.tmp.bad`'e ayrilir;
     o da olmazsa yazim **basarili sayilir** (kurtarmada geri gelecegi icin; iade yok: altin kopyalanmaz, en kotu kaybolur).
  3. `.tmp` -> ana yol `Move`, **motor tekrar denemesi olmadan** (tur 4; eskiden kilitli dosyada 10x0,5 sn ~5,5 sn donma).
  4. `Move` olmazsa dogrudan ana yola yazma denenir. Biri basarirsa `.tmp` ve `.bak` silinir, `true`.
  5. Ikisi de olmazsa: kutu varken `.bak` ana yola geri konur (konamazsa `.bak` durur, sonraki `Load` onu kurtarir), `.tmp` silinir, `false`.
     Ilk postada ana dosya **kosulsuz silinmez**, `.bad`'e ayrilir (QA DUSUK-3); `.tmp` silinir/`.tmp.bad` olur; geriye teslim edilebilir dosya
     kalmazsa `false`, kalirsa yazim basarili sayilir.
- **Bozuk kutu (tur 3 guncel; `Load`, `:181-243`):** bozuk ana dosya `.bad`'e ayrilir ve bu `Load` **false** doner (bos kutu sayilip ustune yazilmaz);
  yanindaki eski `.tmp` `.tmp.bad` olur (olmazsa silinir); `.bak` varsa (basarisiz yazimin gecerli eski kutusu) geri konup okunur ve `true` doner.
  Okunamayan (kilitli) ana dosya da `false` doner. `SendMail` alici kutusunu `Load` ile okur: `false`ta hicbir altin dusulmez (`StoreError`).
  Basarili okumada kalmis eski `.bak` silinir (ileride eski kutuyu diriltmesin).
- **Geri cekilme (Y1):** kurtarma/karantina `Move`'lari da tekrar denemesiz; biri basarisiz olursa ayni kutu **60 sn** boyunca denenmez ve `Load`
  `false` doner (`:17-36`). Harita surec ici `static`: birden cok sunucu sureci arasinda kilit yok.
- **Kayit yuklenmeden altin verilmez:** `AMMOCharacter::IsInitialLoadComplete()` sart; yoksa karakter kaydi yuklenirken altin ustune yazilir, kutu bosaldigi icin altin yok olurdu.
- **Yarim yazim kurtarma (QA D3, tur 3 guncel):** atomik yazim `.tmp` -> `Move` hedefi once siler; arada cokme olursa ana dosya olmayabilir. `Load` ana
  dosya yoksa once `.bak`'i (basarisiz/yarim yazimdan onceki gecerli kutu), yoksa `.tmp`'yi geri koyar; `.bak` secilirse ayni yazimin `.tmp`'si
  `.tmp.bad` olur (`:65-93`). Geri konamazsa `false` + 60 sn geri cekilme.
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
- Acik dusuk maddeler (kod okumasi; `SECURITY_AUDIT.md` K-1 "Kalan acik / dusuk maddeler"): ikinci bozulmada `.bad` ustune yazilir (onceki kanit
  gider); kutu varken `Move` + dogrudan yazim "basarisiz" sayilip `.bak` geri konamazsa ve icerik aslinda yazildiysa iade + teslim olabilir;
  gonderen kaydi ile kutu yazimi arasinda cokme altini kaybettirir (kopyalamaz); birden cok sunucu sureci icin kutu kilidi yok.

## Oyun ici ilk test listesi

1. Iki oyuncu: A, B'ye `/posta gonder B 500g merhaba`; A'nin altini 500 + 20 azaldi mi; B 10 sn icinde mesaji ve +500 altini aldi mi.
2. `/posta gonder B 100 kere tesekkurler`: altin GITMEMELI (yalniz 10 ucret), B mesaji "100 kere tesekkurler" olarak almali.
3. B cevrimdisiyken gonder, B girince al; `Saved/MMOMail/MMOMail_<ad>.json` olusup bosaliyor mu.
4. Olmayan isim, kendine, 5 sn icinde ikinci gonderim, yetersiz altin: dogru hata mesajlari, altin dusmuyor mu.
5. Alici girerken (yuklenme sirasinda) gelen posta: altin kayit yukleme sonrasi dogru ekleniyor mu (kaybolmuyor mu).
6. Gonderdikten hemen sonra editoru/sunucuyu zorla kapat (gorev yoneticisi): yeniden acinca A'nin altini dusmus VE B'nin kutusunda posta var mi (cift altin yok).
7. "Ali Veli" adli oyuncuya `/posta gonder Ali_Veli 10g selam` ulasiyor mu.
