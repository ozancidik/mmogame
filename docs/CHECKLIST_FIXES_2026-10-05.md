# Playtest listesi: 2026-10-05 duzeltmeleri

Kod yazildi ve derlendi, PIE'de OYNANMADI. Asagidakiler kullanici donunce denenecek.
(Ana liste `PLAYTEST_CHECKLIST.md` baska oturumun; bu dosya yalniz bu uc duzeltmeyi kapsar.)

## 1. Esya kaybi: yigin tavani 999

- [ ] Depocu/saticida iksir yigini 990'a getir (`MMOGiveItem HealthPotion 990`), 20 iksir satin almayi dene:
      yalniz 9 alinmali, altin yalniz 9 iksirlik dusmeli, mesajda "tavana ulasildi" yazmali.
- [ ] Yigin 999 iken satin alma: "En fazla 999 tasiyabilirsin", altin degismemeli.
- [ ] `MMOGiveItem HealthPotion 5000` (host debug): yigin 999'da durmali. Kaydet (`MMOSave`), cik, yukle: hala 999.
- [ ] Yerdeki sarf loot'u (ornegin 5 adet) yigin 997 iken topla: 2 alinmali, yerde 3 kalmali, "yigini dolu" mesaji 3 sn'de bir gelmeli;
      birkac iksir harcayinca pickup'in ustune tekrar girince kalan alinmali.
- [ ] Sarf odullu gorevi yigin tavanina yakin iken teslim et: odul reddedilmeli (XP/altin verilmemeli, gorev acik kalmali),
      yer acip tekrar teslim edince tam odul gelmeli.
- [ ] Depo: bozuk/eski kayitla depoyu >24 esyaya cikar (kayit elle duzenleme) ya da test icin sabiti gecici artir;
      depo penceresinde TUM satirlar panel icinde cizilmeli, en alttaki esya tiklanip cantaya cekilebilmeli.

## 2. Beceri bekleme geri alma (uzak istemci)

- [ ] Listen-server + 1 istemci: istemcide menzilde dusman yokken hedef gerektiren beceriye bas: "Menzilde hedef yok"
      gelmeli ve bekleme gostergesi (hotbar sayaci) hemen kapanmali, beceri tekrar kullanilabilmeli.
- [ ] Istemcide mana neredeyse bitikken (replike mana gecikmesi yuzunden) beceri dene: "Yeterli mana yok!" gelmeli, bekleme kalmamali.
- [ ] Gecerli kullanimda bekleme normal calismali (reddedilmeyen beceride geri alma OLMAMALI, tam sure beklemeli).
- [ ] Cooldown icindeyken hizli tekrar basma: sunucu reddederse istemci sayaci sunucunun GERCEK kalan suresine inmeli
      (silinmemeli; beceri erkenden acilmamali).

## 3. Uzak istemci mesajlari (ToOwner)

- [ ] Listen-server + 1 istemci, istemcide konsola `MMOSave`: "Kaydedildi" istemcide de gorunmeli (sohbet sistem satiri).
      Host'ta `MMOSave`: ekranda "Kaydedildi". 60 sn'lik OTOMATIK kayitta (ve klan/ticaret kaydinda) mesaj GORUNMEMELI.
- [ ] Istemci altin yigini topla: "+N altin" gorunmeli. Art arda 3-4 altin topla (0.5 sn icinde): TEK satir, toplam tutar dogru olmali.
      Host'ta ayni davranis (ekranda tek "+N altin", ~0.5 sn gecikmeli).
- [ ] Istemci ile moba temel saldiri: ara vuruslarda "VURDUN" satiri GELMEMELI (sohbeti doldurmamali); olduren vurusta
      "VURDUN: ... >>> OLDU!" gelmeli. Hasar sayilari / kritik gostergesi eskisi gibi calismali.
- [ ] Host kendi saldirisinda her vurusta VURDUN mesajini eskisi gibi gormeli (yerel yol degismedi).

## 4. QA bulgu 2: ActiveSaveSlot erken atama + kayit reddi mesaji

Kod yazildi ve derlendi, PIE'de OYNANMADI.

- [ ] Navmesh yokken (`-NullRHI` hizli baslangic ya da Winter erken `MMOLoad`) mevcut kayitli karakterle gir; host'ta
      `MMOSetName <diskte kayitli baska bir ad>`, sonra `MMOSave`: "Bu ad baska bir kayda ait: kaydedilmedi..." mesaji gelmeli,
      diskteki o baska kayit DEGISMEMELI.
- [ ] Navmesh bekleme dali bittikten sonra (karakter konuma tasindiktan sonra) normal `MMOSave` calismali ("Kaydedildi").
- [ ] Uzak istemci olarak ayni red durumu: istemcide de mesaj gorunmeli (otomatik kayitta da, ~60 sn'de bir tekrar gelebilir; beklenen).

## 5. QA bulgu 1/3/5: ad korumalari (SetName yolu, OS varsayilan adi, 20 karakter siniri)

Kod yazildi ve derlendi, PIE'de OYNANMADI.

- [ ] Istemci konsolunda `SetName Ozan` (host'ta `Ozan` cevrimici): ad degismemeli, "Bu ad su an baska bir oyuncuda" ipucu cikmali.
      Bos bir ad icin `SetName Yeniad`: ad degismeli ("Adin 'Yeniad' olarak ayarlandi"), arkadaslara duyuru gitmeli.
- [ ] Istemci `SetName` ile 30 sn icinde ikinci kez ad degistirsin: "Ad degistirmek icin N sn beklemelisin" cikmali (host'ta soguma yok).
- [ ] `SetName A<Enter>B` / ozel karakterli ad: reddedilmeli ("Adda yalniz harf, rakam..."), sohbet satiri bozulmamali.
- [ ] Istemcide `SetName <diskte kayitli baska ad>`: "Bu ad kayitli bir karaktere ait" mesaji; ad degismemeli.
- [ ] `MMOSetName` (proje yolu) ile 20 karakterlik ad: kabul; 21 karakter: "Ad 1-20 karakter olmali"; `MMOSave`, cik,
      `?Name=<ayni 20 karakter>` ile gir: ayni karakter gelmeli.
- [ ] `?Name=Ozan-20261005`, `?Name=Kral-12345678`, `?Name=Ali-deadbeef` ile baglan: ad DEGISMEMELI, ayni kayit yuklenmeli.
- [ ] `?Name=<OS adi>-<14 hex>` (Null OSS tarzi, orn. `CIDIK-1a2b3c4d5e6f70`) ile baglan: `Gezgin######` olmali ve ~3 sn sonra
      sohbette "Baglanti adin uygun olmadigi icin adin 'Gezgin######' olarak ayarlandi..." satiri cikmali.
- [ ] Ayni anda iki istemci ayni `?Name=` ile baglansin: ikincisi Gezgin olmali ve ayni bildirimi almali.

## 6. QA bulgu 7/8: MMONotify gecici ipucu (Hint) ayrimi + 0.5 sn dedupe

Kod yazildi ve derlendi, PIE'de OYNANMADI. `MMONotify::ToOwner(..., EMMONotifyKind Kind = Result)`: sonuc satirlari
(satin alma/satis, gorev alindi/tamam, sinif, kayit, olum, iksir, +altin, loot) sohbette kalir; ret/ipuclari
("Altin yetersiz", "Satici yakininda degilsin", "Canta dolu!", "Menzilde hedef yok", "Yeterli mana yok!", "Level N gerekli",
"Once '...' gorevini bitir", sinif ret mesajlari, depo reddi, ad reddi) uzak istemcide karakter ustunde kisa balon olur.

- [ ] Istemci olarak saticidan uzakken satin al tusuna 20 kez bas: sohbet DOLMAMALI (balon cikar), baglanti kopmamali.
- [ ] Istemci olarak 12 esyayi art arda sat: her satis sonucu ("Satildi: ...") sohbette gorunmeli; ayni metin 0.5 sn icinde
      tekrar gelirse tek satir olabilir (beklenen); ping artmamali.
- [ ] Istemci, hedefsiz skill / mana yetersizken beceri tusu: sohbete satir DUSMEMELI, karakter ustunde balon cikmali.
- [ ] Host'ta ayni durumlar: ekranda mesaj eskisi gibi gorunmeli (yerel yol degismedi).
- [ ] Istemci: tamamlanmis gorevin odulunu tekrar almaya calis (gorev paneli): "Bu gorevin odulunu zaten aldin" ipucu.
- [ ] Istemci: on kosulu tamamlanmamis gorevi almaya calis: "Once '...' gorevini bitir" ipucu (eskiden istemcide hic gelmezdi).

## 7. QA bulgu 6: parti XP boleni = menzildeki CANLI uye sayisi (bonus ayni sayiyla)

Kod yazildi ve derlendi, PIE'de OYNANMADI. Dengeye dair karar degil, tutarlilik duzeltmesi (BALANCE_MODEL.md guncellendi).

- [ ] 3 kisilik parti, biri sehirde (3000u disinda) AFK: ikisi mob keser; her biri ~%55 (1.1/2) almali, %40 (1.2/3) degil.
      `[MMOMETRIC] ev=kill` satirlarindaki `xp=` ve "+N EXP (%P)" ile karsilastir.
- [ ] 3 kisilik parti, ucu de menzilde, esit hasar: her biri 1.2/3 = %40 almali (bolunme degismedi).
- [ ] Uyelerden biri mob olurken olu: kalan canli uyeler olu uyeyi saymadan paylasmali; olu uye XP/ganimet almamali.
- [ ] 8 kisilik parti, tek kisi mobu tek basina keser (digerleri uzak): payin 1.0/1 = %100'unu almali (eskiden %21).
- [ ] Solo oyuncu: XP degismemeli (%100). Parti disi iki oyuncu %50/%50: her biri ~%50.
- [ ] `[MMOMETRIC]` satirlarinda parti disi/olu uyeye XP yazilmamali.
- [ ] Kod yorumu: `ClientNotifyXPGain` yuzdesi seviye farki + hasar payi + parti bolumunu icerir ("(%55)" ceza degil, paydir).

## 8. Guvenlik: MMOClass/MMONpcTalk Shipping'de kapali (Y-6)

Kod derlendi, RunAllChecks (Summer/Winter duman + NetSmoke) gecti; PIE'de OYNANMADI.

- [ ] Development/Editor: `MMOClass mage` ve `MMONpcTalk` eskisi gibi calismali (savas ici/disi sinif kurallari degismedi).
- [ ] HUD sinif secim paneli (yeni karakter ipucu) tiklayinca sinifi degistirmeli (konsola bagli degil).
- [ ] Shipping paketi (ilk dagitimdan once, opsiyonel): `MMOClass`/`MMONpcTalk`/`MMOGiveItem` etkisiz; `~` konsolu kapali.

## 9. Altin tavani tek sabit: `UMMOInventoryComponent::MaxGold` = 999999999 (Y-2 tutarsizligi)

Eskiden calisma zamani tavani INT32_MAX (2.1 milyar), kayit tavani 999999999 idi: 1 milyar ustu altin yuklemede sessizce
kirpiliyordu. Artik AddGold, ticaret tasma reddi ve ApplySaveData ayni sabiti kullanir. Kayit formati degismedi.
Kural: loot/gorev/satis kazanci tavani asarsa fazlasi kirpilir (eskiden de oyleydi) ve artik sahibe "Altin siniri (999999999): fazlasi kaybedildi"
ipucu gelir; ticaret tavani asacaksa ONCE reddedilir ("altin siniri asilir"), altin yutmaz. Kod derlendi, PIE'de OYNANMADI.

- [ ] `Gold` degerini 999999990 yap (ornek: `.sav` ile ya da cok loot), 100 altin loot al: bakiye 999999999 olmali, "Altin siniri" ipucu gelmeli.
- [ ] 999999999 altinla kaydet, cik, yukle: bakiye ayni kalmali (eskiden de ayniydi; 1 milyar ustu artik hic olusmaz).
- [ ] Ticaret: bir taraf 999999000, digeri 5000 altin teklif eder -> onayda "altin siniri asilir" iptali; iki taraf da altinini korumali.
- [ ] Normal ticaret/satin alma/satis eskisi gibi; HUD altin gostergesi tasmamali (9 hane).

## 10. Guvenlik: LLM butcesi ve MaxTokens (Y-4)

Kod derlendi, RunAllChecks gecti; PIE'de ve gercek LLM cagrisiyla OYNANMADI (anahtar gerekir).

- [ ] `NVIDIA_API_KEY` ile NPC'ye tikla: replik gelmeli (400 token yeterli, kesilme olmamali; sohbet satiri 240 karakterde kesilir).
- [ ] Ayni NPC'ye 5 sn icinde tekrar tikla: ikinci istek yok sayilmali (sessiz); 5 sn sonra yeniden calismali (eskiden 2 sn).
- [ ] 400 cagri sonrasi (ya da `MaxSessionRequests`'i dusurup) NPC'ler yedek replik soylemeli; log: `Oturum LLM butcesi doldu`. Oyun/sohbet etkilenmemeli.
- [ ] `Provider=Anthropic` denenirse: replikler kesilip yedege dusuyor mu (logda `max_tokens`/`content bos`)? Dusuyorsa `AnthropicMaxTokens` artir.
- [ ] Shipping/Development: `MMONpcTalk` Development'ta eskisi gibi, Shipping'de etkisiz.

## 11. Guvenlik: `MMOSetName` 5 sn yerel soguma (Y-7)

Kod derlendi, PIE'de OYNANMADI.

- [ ] Host konsolunda `MMOSetName A` sonra hemen `MMOSetName B`: ikinci istek reddedilmeli, ekranda "Ad degistirmek icin N sn beklemelisin" ipucu; 5 sn sonra calismali.
- [ ] Uzak istemcide ayni: 30 sn (degismedi), ipucu gelmeli.
- [ ] `ServerChangeName` yolu (motor `SetName`) ile ad degistirme de ayni sogumaya tabi (cift soguma yok: bir basarili degisimden sonra tek bekleme).
- [ ] Gecersiz ad ("a@b") reddedilir ve sogumayi BASLATMAZ (hemen gecerli adla tekrar denenebilir).

## 12. Guvenlik: sohbette DEL/C1 kontrol karakterleri (D-9)

Kod derlendi, PIE'de OYNANMADI.

- [ ] Sohbete `Say` ile `\x7f` (DEL) veya U+0085 iceren metin: satir bosluga cevrilmis halde gelmeli; normal Turkce karakterler (ş ğ ı ö ü ç) etkilenmemeli.

## 13. Varsayilan ad / OS adi (Y-5): kod incelemesi + NetSmoke ile dogrulandi

Degisiklik gerekmedi. Istemci `?Name=CIDIK-<hex>` ile baglandiginda sunucu `Gezgin######` atadi (net smoke logunda). PIE'de elle:

- [ ] `?Name=` olmadan (veya OS adiyla) baglanan ikinci oyuncunun adi karakter ustu etiket, `/who`, sohbet ve parti listesinde `Gezgin######` olarak gorunmeli; bilgisayar/kullanici adi hicbir UI'da gorunmemeli.
