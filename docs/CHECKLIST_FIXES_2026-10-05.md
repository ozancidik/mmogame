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

## 14. QA Tur-3 orta/dusuk duzeltmeleri (ORTA-1/2/3, DUSUK-1/3)

Kod derlendi, RunAllChecks (-SkipBuild) GECTI; PIE'de OYNANMADI. Host + 1 istemci gerekir.

- [ ] **ORTA-1** PvP kapali (varsayilan), hub disinda: istemci hostun karakterine sol tikla ve yanina yaklas. Beklenen: en fazla bir kez "PvP kapali" balonu, sonra kovalama DURUR (karakter yurumeyi birakir, host cani dusmez). Hedef secili kalir (HUD hedef bari). Ayni hedefe 3 sn icinde tekrar tiklamak yalniz secer, kovalamaz; 3 sn sonra tek vurus denemesi ve yine birakma.
- [ ] Sol tusu hostun uzerinde BASILI tut: RPC/balon spami olmamali (3 sn araliklarla en fazla bir deneme).
- [ ] Ayni testi parti uyesi ve klan uyesi icin tekrarla (PvP acik: `MMOPvP 1`): "Parti uyesine vuramazsin" / "Klan uyesine vuramazsin" sonrasi kovalama durmali.
- [ ] Hub guvenli bolgesinde oyuncuya tikla: "Guvenli bolgede saldiri yok", kovalama durmali. Hub icinde moba tiklayinca hedef BIRAKILMAMALI (cikinca saldirisi surer).
- [ ] Regresyon: oyuncu hedefi birakildiktan sonra yakindaki moba tikla/saldir normal calismali; PvP acikken (`MMOPvP 1`, hub disi, parti/klan yok) iki oyuncu birbirine vurabilmeli ve kovalama kesilmemeli.
- [ ] **DUSUK-1** PvP acik: birini oldur, yeniden doganin uzerine hemen tikla-saldir. Beklenen: "Hedef korumali" balonu, 0 hasar yazisi YOK; saldiranin kendi dogum korumasi ve savas durumu degismez (sinif degistirme/bolge gecisi "savasta" diye reddedilmemeli); kovalama durur.
- [ ] **ORTA-2** Parti: A, B'ye `/parti` (ya da MMOInvite) ile davet atar. B `/parti ret` yazar: B "Parti daveti reddedildi.", A (uzak istemci de olsa) "<B> parti davetini reddetti." gormeli; B `/parti kabul` artik "Bekleyen" daveti bulamamali (parti kurulmaz).
- [ ] B davet beklerken A tekrar davet atarsa A "zaten gonderildi" ipucunu gormeli; C davet atarsa C "bekleyen baska bir daveti var" gormeli; hic oyuncu yakin degilken davet atan "Yakinda davet edilecek oyuncu yok" gormeli (uzak istemcide de).
- [ ] Davet 30 sn sonra gecersiz: B 30 sn bekleyip `/parti kabul` yazinca "Davetin suresi doldu"/parti kurulmamasi; daveti 30 sn sonra baska oyuncu atabilmeli. `/yardim` metninde `/parti (davet; kabul, ret, ayril)` gorunmeli; `/parti xyz` kullanim satirini vermeli. Konsol: `MMOPartyDecline` ayni davranisi vermeli.
- [ ] **ORTA-3** `G` gorev penceresi: Seviye 6+ ile `Q_Warlocks`/`Q_DireWolves`, Lv25+ ile `Q_ObsidianWarlord`, `Q_Gold1000` ve uzun esya adli odullu gorevlerde aciklama + odul satiri panelin (480 px) icinde kalmali; uzun aciklama "..." ile bitmeli ve "Odul: ..." kismi her zaman gorunmeli. Satirlara tiklayarak gorev alma/odul alma hala calismali; cok gorevli durumda satir hizasi bozulmamali. (Ekran goruntusu al.)
- [ ] **DUSUK-3** `?Name=MehmetCelikoglu-2024` (tam 20 karakter) ile baglanan oyuncu `Gezgin######` OLMAMALI; `?Name=CIDIK-3F9A1B7C2D4E5F` ve cok uzun bilgisayar adindan kesilmis `...-1A2B` gibi adlar yine `Gezgin######` olmali.

## 15. Ilk 10 dakika ipuclari (CONTENT_PLAN_LV11_45 bolum C)

Kod derlendi, RunAllChecks (-SkipBuild) GECTI; PIE'de OYNANMADI. Tamami `AMMOHUD::UpdateTutorialHints` icinde yerel HUD durumu; sunucu/RPC/kayit degismedi. Balon: alt-orta NPC balonu (baslik "Ipucu", 9 sn). Yeni karakterle, konsol komutu kullanmadan dene; host + 1 istemci.

- [ ] **H1** Yeni karakter (Lv1, 0 EXP): sinif panelini kapat, 8 sn kipirdama: "Zemine sol tikla: yuru..." cikmali. Ilk adimdan sonra (>80 birim) bu oturumda hic cikmamali; 8 sn dolmadan yurursen hic cikmamali.
- [ ] **H2** Lv1-2, gorev yokken Yasli Doran'a <=900 birim yaklas: "Yasli Doran'in ustundeki ! isaretine tikla" cikmali. Gorev aldiktan/tamamladiktan sonra cikmamali.
- [ ] **H3** Ilk gorevi kabul et ve 20 sn hic oldurme yapma: "Z: hedef sec  R: saldir..." cikmali (Doran kabul replighi 9 sn sonra, ipucu ondan SONRA/ustune binmeden). 20 sn icinde bir mob oldurursen cikmamali.
- [ ] **H4** Lv<=5, ilk oldurmede esya/altin yere dusunce (<=1500 birim): "Esya dustu: uzerinden gec..." cikmali; bir daha cikmamali.
- [ ] **H5** Lv1 -> Lv2 atla (puan var): "Seviye atladin! C: stat puani..." cikmali; Lv11+ seviye atlamada cikmamali.
- [ ] **H6** Gorev hedefi dolunca (tracker "(tamam!)"): "Gorev tamam! Yasli Doran'a don..." cikmali; Lv>10 karakterde cikmamali.
- [ ] **H7** Lv<=20 iken ilk kez ol: olum ekraninda (OLDUN) alt-ortada "Oldun: 5 sn sonra dogarsin..." cikmali; ikinci olumde cikmamali.
- [ ] **H8** Cantayi 12/12 yap (bos -> dolu gecisi): "Canta dolu! Satici NPC'ye esya sat..." cikmali; Lv>20 karakterde cikmamali.
- [ ] **Tek ipucu / spam yok**: iki tetikleyici ayni anda olursa (or. Lv2 + ilk ganimet) once biri, en az ~12 sn sonra digeri cikmali; hic ust uste binmemeli. Ipucu sirasinda NPC diyalogu acilirsa diyalog onceliklidir (ipucu kaybolur, 20 sn icinde yer bulursa sonra cikar).
- [ ] **Pencere**: envanter/gorev/beceri/stat/depo/dukkan acikken ipucu cikmamali; pencere kapaninca (20 sn icinde) cikmali.
- [ ] **Oturumluk**: ayni ipucu ayni oturumda ikinci kez cikmamali (bolge degisimi/olum-canlanma dahil). Oyundan cikip girince Lv<=sinir karakterde bir kez daha cikabilir (kabul edilen sinirlama).
- [ ] **Istemci**: ikinci pencere (listen-server istemcisi) kendi ipuclarini gormeli; host penceresinde istemcinin ipucu cikmamali.
- [ ] **Yuksek seviye**: Lv25+ kayitli karakterle girince (gorev/canta dolu/olum dahil) hicbir ipucu cikmamali; girisin ilk 3 sn icinde replikasyon yuklenmesi yanlis tetik uretmemeli.
- [ ] Planin H5 "puan >=6 birikince Lv<=15 ikinci ipucu" kismi UYGULANMADI (yalniz ilk seviye atlama).

## 16. Y3: can/mana yenilenmesi 0.5 sn adimlarla (NETWORK_AUDIT "Y3 cozum notu")

Kod derlendi, RunAllChecks (-SkipBuild) GECTI; PIE'de OYNANMADI. Host + 1 istemci gerekir.

- [ ] Savasta olmayan, cani eksik oyuncu: can cubugu ~0.5 sn adimlarla (yaklasik +3 HP/adim; savasta 1/4 hizla) dolmali; toplam sure eskisiyle ayni (6 HP/sn). Mana ayni sekilde (7 + 0.25*Lv /sn).
- [ ] Istemcide (uzak pencere) HOSTUN canini izle: 0.5 sn adimlarla artmali, takilmamali. Hasar alan oyuncuda dusus yine ANINDA gorunmeli (hasar/iyilestirme yolu adimlanmadi); iksir/Heal ani artis vermeli.
- [ ] Dolu cana ulasinca yenilenme durmali; bir sonraki hasardan sonra beklenmedik +1 HP sicramasi olmamali (can/mana doluyken birikim sifirlanir).
- [ ] Efekt: bir buff/zehir uygula; HUD efekt sayaci (EndServerTime) geri sayimi akici ve dogru, sure bitince efekt kalkmali (uzak istemcide de).
- [ ] (Istege bagli) 2 istemci + `stat net`: yenilenen oyuncu basina trafigin eskisine gore dustugunu olc (kazanc su an DOGRULANMADI).

## 17. Uzak istemci geri bildirimleri MMONotify'a tasindi (NETWORK_AUDIT O11 madde 3 devam notu)

Kod derlendi, RunAllChecks (-SkipBuild) GECTI; PIE'de OYNANMADI. Host + 1 UZAK istemci gerekir; her satirda mesaj UZAK istemcide gorunmeli (host pencerede degil). Hint = karakter ustunde kisa balon, Result = sohbet sistem satiri.

- [ ] **Parti**: yakinda baska oyuncu yokken `/parti` (veya MMOInvite): istemcide "Yakinda davet edilecek oyuncu yok" balonu. Daveti 30 sn+ beklettikten sonra `/parti kabul`: "Davetin suresi doldu"; davet edenden cok uzaklasip kabul: "Davet eden cok uzakta" (InviteAcceptMaxDistance asildiginda).
- [ ] Uye olmayan istemcide parti uyesi secip kick denemesi (yetkisiz): "Sadece parti lideri uye cikarabilir" balonu. Lider istemcide basarili kick: sohbette "Uye partiden cikarildi" satiri; atilan oyuncuda "Parti lideri seni partiden cikardi" (degismedi).
- [ ] 8 kisilik iki partiyi birlestirmeye calis (veya 8+ kisi): "Parti dolu (maksimum 8)" balonu davet edende.
- [ ] **Ganimet**: istemcide sarf malzemesi (iksir vb.) toplayinca "+N <esya>" sohbet satiri gorunmeli (host ekraninda istemcinin satiri CIKMAMALI). Cok hizli ardisik toplamada spam olmamali (0.5 sn ayni metin tekrar etmez).
- [ ] **Iksir**: istemcide cani doluyken `F`: "Canin zaten dolu"; iksiri yokken `F`/`G`: "Iksir yok!" / "<Esya> yok!" (yerelde ekran mesaji; ayni davranis host icin de).
- [ ] **Beceri**: hedefsiz/menzil disi iken hedef gerektiren skill (sunucu ExecuteSkill yolu nadiren; ana yol UseSkill "Menzilde hedef yok" zaten vardi): istemcide balon gorunmeli, mana harcanmamali.
- [ ] Regresyon: host kendi pawn'inda tum bu mesajlari eskisi gibi ekranda gormeli (cift mesaj olmamali).
