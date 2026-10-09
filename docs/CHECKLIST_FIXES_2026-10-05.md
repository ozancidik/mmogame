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

## 18. Muhafiz Kael replik duzeltmesi (yalniz metin)

Kod derlendi, RunAllChecks (-SkipBuild) GECTI; PIE'de OYNANMADI.

- [ ] LLM kapaliyken Muhafiz Kael'e art arda tikla: yedek replik listesinde "Goblin tek basina korkutmaz. Suru bastirirsa geri cekil, dar bir gecit bul." gorunmeli; eski "sirtini tasa ver" satiri hic cikmamali. Balona sigmali (tek baloncuk).

## 19. QA NIGHT2 ORTA-2 + DUSUK-1/5: kayitta beceri senkronu, ipucu tuketimi, beceri kitabi sirasi

Kod derlendi (Result: Succeeded), RunAllChecks (-SkipBuild) GECTI; PIE'de OYNANMADI. (Sonuc satirlari commit sonrasi dogrulandi; gecmediyse bu not duzeltilecek.)

**T4 - Eski kayitli yuksek seviyeli karakter (ORTA-2)**
- [ ] Bu surumden ONCE kaydedilmis Lv>=31 karakterle gir (Savasci/Okcu/Buyucu Special0 rutbesi >=6, Rahip >=4): beceri kitabinda `War_Earthshaker` / `Arc_ThornVolley` / `Mag_Inferno` / `Pri_RadiantNova` (Lv31) ve Lv39 becerileri ilk girista [Ogrenildi] gorunmeli (seviye atlamayi beklemeden). Lv80 karakterde de.
- [ ] Skill puani (`Skill Puani:` satiri) giristen onceki degerle AYNI kalmali (otomatik acilan beceriler puan harcatmamali). Hotbar'daki mevcut atamalar degismemeli; yeni acilanlar hotbar'a OTOMATIK konmaz, elle atanir.
- [ ] Rutbesi yetersiz oldugu (ornegin Special0 rutbe 3) bir karakterde Lv31/Lv39 becerileri acilmamali (yalniz rutbe+seviye yeterliyse acilir). Kayittaki mevcut ogrenilmis beceriler silinmemeli.
- [ ] Beceri kitabinda rutbesi yeterli ama (nadir) hala acilmamis bir satir varsa durum metni "[Otomatik acilir (puan gerekmez)]" olmali; eski yanlis "Level N gerekli" CIKMAMALI. Seviyesi gercekten dusuk satirda "Level N gerekli", rutbe uzaksa "Rutbe N gerekli" cikmaya devam eder.
- [ ] Yeni karakterde ve normal seviye atlamada (rutbe yatirma) beceri acilisi eskisi gibi; kayit/yukle sonrasi ayni beceri listesi geri gelmeli (kayit bicimi degismedi).

**T6 - Ipucu tuketimi (DUSUK-1)**
- [ ] Ipucu balonu (Ipucu: ...) ilk saniyelerindeyken NPC'ye tikla (veya gorev kabul/teslim et): gercek replik gorunmeli; ipucu 20 sn icinde (pencere/replik kapaninca) TEKRAR cikmali, kalici kaybolmamali.
- [ ] Ipucu 9 sn boyunca kesintisiz kalirsa ayni oturumda bir daha CIKMAMALI.
- [ ] H2: Lv1'de "Yasli Doran'in ustundeki ! ..." kuyruktayken/gosterimdeyken gorevi kabul et: H2 bir daha cikmamali; kabulden sonra ~4 sn hicbir ipucu cikmamali, Doran kabul replighi ipucuyla cakismamali.

**T7 - Beceri kitabi sirasi (DUSUK-5)**
- [ ] Savasci/Rahip Ustalik 1 listesi artik seviyeye gore artan: Rampage(28) - Earthshaker(31) - Skullsplitter(39) - WarCyclone(45) - WarDeluge(58) (Rahip: Retribution 28, RadiantNova 31, ... AngelicBlade 45, AngelicStorm 58). Her satira tiklama hala o becerinin kendisine gitmeli (yanlis satir ogrenilmemeli, hotbar atamasi dogru beceriyi secmeli); iki sutun duzeni/tasma degismemeli.

## 20. Hesap hazirligi Asama 1: kayit okuma/yazma IMMOCharacterStore arkasina alindi (saf refactor)

Kod derlendi, RunAllChecks (-SkipBuild) GECTI (yeni adim "Karakter magazasi" dahil; TestCharacterStore.ps1 106 yuvanin tamami yuklendi, gecici yuvaya Save->Load gidis-donusu esit). PIE'de OYNANMADI; davranis degismemeli, asagisi regresyon kontroludur. Editor kapaliyken once `Scripts\TestCharacterStore.ps1` calistirilabilir.

- [ ] Eski kayitli karakterle (ornegin Lv6+ bir `MMOSave_<ad>.sav`) gir: seviye/envanter/gorev/klan/konum eskisi gibi gelmeli, "Karakter yuklendi" balonu cikmali, baslangic paketi TEKRAR verilmemeli.
- [ ] Yeni (kayitsiz) bir adla gir: sinif secimi ipucu paneli + baslangic paketi (Kisa Kilic, Deri Zirh, 3 Can Iksiri) bir kez gelmeli. `MMOSave` -> "Kaydedildi"; `Saved/SaveGames/MMOSave_<ad>.sav` olusmali; cikip tekrar girince ayni durum yuklenmeli.
- [ ] K-1 hala gecerli: kayitli bir karakterle girip (uzak istemci) `MMOSetName` ile BASKA bir kayitli karakterin adini almaya calis: "Bu ad kayitli bir karaktere ait" mesaji, ad degismemeli. Host'ta (yerel) kayitli bir adi alip `MMOSave`: "Bu ad baska bir kayda ait: kaydedilmedi" uyarisi, o dosya EZILMEMELI.
- [ ] Klan: `/klan kur <ad>` ile baska bir (cevrimdisi, kayitli) karakterin klan adini almaya calis: "Bu klan adi kullanimda (cevrimdisi uyeleri var)". Yeni ad kurulabilmeli; ilk taramada hafif takilma (tum yuvalar taranir, 30 sn onbellek) olmamali/kisa surmeli.
- [ ] Bolge gecisi (Summer <-> Winter, `MMOTravel`) ve 60 sn otomatik kayit eskisi gibi calismali; kayit basarisizsa gecis iptal mesaji (degismedi).
- [ ] Not (otomasyonla DOGRULANAMAYAN): bir pawn'a belirli ad vermek headless'ta mumkun olmadi (yerel/uzak oyuncu adi motor tarafindan "Player"/OS adina cevrilip `Gezgin######` oluyor); bu yuzden "eski .sav'i belirli karakter olarak yukleme" yalniz PIE'de gozle dogrulanir. Magaza katmaninda eski dosyalarin hepsinin okundugu TestCharacterStore ile kanitlandi.

## 21. QA NIGHT2 ORTA-1: Cozulme Yolu cebi rota acikligi icin tasindi (-5000,-3000) -> (-1875,5125,-114)

Kod derlendi (Result: Succeeded), RunAllChecks (-SkipBuild) GECTI; PIE'de OYNANMADI. Yalniz cep konumu degisti (kayit bicimi ayni). Olcum: `Scripts/ProbeThawRoute.py` (editor navmesh, 250u grid, 1877 tam-yollu hucre) + `SmokeTest.ps1` ile oyun ici `MMOProbeSurface/MMOProbeSpawn`; tablo `MMOGameMode.cpp` Cozulme yorumunda. Hub->cep nav yolu: (-816,-47) (-2413,3363) (-2375,4427) (-2223,4712) (-2128,4845) (-1875,5125). Rota acikligi >=20 cepler: Hukumdar 1375, Magma Tirani 1945, Obsidyen 2647, Reis 2863 (eski rota 288 / 676). 1500u hedefi haritada saglanamadi (en iyi 1375); karar adayi `DECISIONS_NEEDED.md`. Bu madde `PLAYTEST_CHECKLIST.md` ~3510'daki eski `(-5000,-3000)` maddesinin yerine gecer.

**T1 - Rota (Lv13-15 karakter, hub'dan cebe)**
- [ ] Lv14 karakterle PlayerStart'tan `(-1875,5125)` cebine yuru (tikla-yurude nav yolu yukaridaki noktalardan gecer). Yolda Obsidyen (Lv25-29), Magma Tirani, Alacakaranlik moblari/elitleri sana saldirdi mi? (Beklenen: HAYIR; Alacakaranlik Hukumdari ~1375u uzakta, fark etme zarfi ~1200u.) Saldirdiysa hangi mob ve cep merkezine kac u kala oldu not et.
- [ ] Yol Ikinci bolge (Lv6-7, kuzeybati yolun uzerinde) ve Buz Lordu'nun (Lv13 elit, aggro 1300, yola ~943u) yakininden geciyor: Buz Lordu seni cekiyor mu? Cekiyorsa kabul edilebilir mi, notu DECISIONS_NEEDED'a geri bildir.
- [ ] Dar duz serit: cep kuzeybatida yamacla cevrili (Z~-164 duz, yamac +250u). Yuruyus sirasinda takilma / navmesh kopmasi / bosluga dusme var mi?

**T2 - Cep (spawner)**
- [ ] Konsol: `MMOProbeSpawn -1875 5125 -114 225` ~10 kez; `[MMOPROBE] ... ok=N/64`: beklenen 59-64/64 (egim reddi yamac tarafi, trace_miss 0, nav 0).
- [ ] Cepte 5 mob yalniz `ThawStalker/CrustWarden/ScorchedRaider`; yerde duruyor (havada/yer altinda degil, Z~-74); ucuruma/yamaca yapisip kalmiyor; `[MMODBG] Spawner ... no nearby ground/nav location` uyarisi logda yok.
- [ ] Cepte dovusurken Yarik Bekcisi (-4800,4800, Lv62) ve Alacakaranlik cepleri (1000,3200) cekilmiyor (en yakin 2943u).
- [ ] Minimap/gorev yonlendirmesi: Q_ThawStalkers/CrustWardens/ScorchedRaiders hedeflerine gidilebiliyor mu; gorev metni yer tarif etmiyor (degistirilmedi).

## 22. Dedicated hazirligi (davranis degistirmeden): replike PvP durumu (AMMOGameState)

Kod derlendi (Result: Succeeded), RunAllChecks (-SkipBuild) GECTI (NetSmokeTest dahil: 1 istemci baglandi, hata 0). PIE'de iki oyunculu OYNANMADI. Tek kaynak `AMMOGameMode::bAllowPvP`; `AMMOGameState::bPvPEnabled` onun replike aynasi. `BP_ThirdPersonGameMode` ve haritalar GameStateClass'i ezmiyor (uasset/umap isim tablosu tarandi), C++ varsayilani gecerli.

- [ ] Host + 1 uzak istemci, PvP KAPALI (varsayilan): istemci diger oyuncuya tikladiginda karakter ona KOSMAMALI (yalniz secim halkasi/hedef cercevesi); `PvP kapali` ipucu cikabilir; host loguna/ag trafigine bos ServerAttackTarget akmamali. Mob'a tiklayinca hicbir sey degismemeli (kovalar ve vurur).
- [ ] Host konsolundan `MMOPvP 1`: birkac saniye icinde uzak istemci de oyuncuya tiklayip kovalayabilmeli ve vurabilmeli (istemci GameState aynasi guncellendi). `MMOPvP 0` ile tekrar kapaninca istemci yine kovalamamali.
- [ ] PvP ACIKken: ayni parti/klan uyesine tiklayinca kovalama baslamamali ("Parti uyesine vuramazsin"/"Klan uyesine vuramazsin"); hub guvenli bolgesinde (PlayerStart 700u) oyuncuya tiklayinca kovalama yok.
- [ ] Bolge gecisinden (`MMOTravel`) sonra yeni dunyada PvP yine KAPALI baslar (GameMode yeniden olusur; onceki davranisla ayni) ve istemci de kapali gorur.
- [ ] Regresyon: uzak istemcinin mob'a vurusu, beceri kullanimi ve hasar sayilari eskisi gibi.

## 23. Dedicated hazirligi: admin/debug yetkisi `MMOAdmin::IsServerAdmin`'de merkezilestirildi

Kod derlendi, RunAllChecks (-SkipBuild) GECTI (headless bolge gecisi + timeout testleri host secimini artik IsServerAdmin ile yapiyor ve geciyor). PIE'de OYNANMADI. Davranis degismemeli: yetkili = yalniz listen-server host'unun yerel denetleyicisi; dedicated'da hic kimse (kasitli, ileride token/hesap).

- [ ] Host: "Tum oyunculari Kis'a/Yaz'a tasi" dugmesi gorunur ve calisir; `MMOTravel` konsol komutu calisir.
- [ ] Uzak istemci: dugme GORUNMEZ; konsoldan `MMOTravel`, `MMOPvP 1`, `MMOBuff Rage`, `MMOSpawnMob OrcWarlord`, `MMOGiveItem ...`, manuel karakter yukleme (`MMOLoad`): hicbir etki olmamali (sunucu sessizce reddeder), host'un PvP/envanteri/dunyasi degismemeli.
- [ ] Host: `MMOPvP 1/0`, `MMOBuff`, `MMOSpawnMob`, `MMOGiveItem` eskisi gibi calisir (Development derlemede).
- [ ] Shipping paketinde bu konsol komutlari ve debug RPC'leri zaten kapali (D1); degismedi.

## 24. LLM NPC diyalogu: gunluk (oturum-otesi) cagri tavani

Kod derlendi, RunAllChecks (-SkipBuild) GECTI. Bu kontroller LLM'e hic istek atmaz (API anahtari yok), yani tavan mantigi OTOMATIK DOGRULANMADI, PIE'de de denenmedi. `MMODialogueSubsystem::MaxDailyRequests` varsayilani 1500 bir baslangic tahminidir (maliyet karari sizin).

DUZELTME (2026-10-08 turu): bu bolumun ilk surumundeki "baglanti hatasi sayilmaz" cumlesi kodla celisiyordu. Gecerli kural (kod + `MMODialogueSubsystem.h` yorumu): anahtarsiz istek hic kurulmaz ve sayilmaz; baslatilamayan istek, baglanti hatasi/iptal ve HTTP != 200 (401/403/429/5xx...) IADE edilir; HTTP 200 (ayristirma hatasi dahil) ve ZAMAN ASIMI SAYILIR (istek saglayiciya ulasmis/faturalanmis olabilir). Sayac artik her cagrida degil, 10 degisiklikte / 30 sn'de bir ve kapanista yazilir (bkz. bolum 28); tavan `MaxDailyRequests` editorde degil, `-MMOLlmDaily=N` komut satiri veya DefaultGame.ini `[MMO] LlmDailyRequests=N` ile ayarlanir.

- [ ] `NVIDIA_API_KEY` tanimliyken NPC ile konus: `Saved/MMO/LLMDailyBudget.txt` olusur; 1. satir "<UTC gun numarasi> <sayac>", sonraki satirlar "<oyuncu>\t<sayac>"; BASARILI (HTTP 200) her LLM istegiyle sayac 1 artar (dosyaya yazim gecikmeli: 10 istek ya da 30 sn sonra/kapanista).
- [ ] Gecersiz bir `NVIDIA_API_KEY` ile (HTTP 401) 3 kez konus: sayac ARTMAMALI (iade), log'da `HTTP 401` gorulmeli. Interneti kesip dene: baglanti hatasi iade, zaman asimi (12 sn) ise sayilir.
- [ ] Host'u kapatip ayni gun yeniden ac: sayac dosyadan devam etmeli (0'dan baslamamali). Dosyadaki gun numarasini elle eskiye cek: ilk istekte sayac 0'lanmali.
- [ ] Tavani gecici dusur (`-MMOLlmDaily=1` ile host'u ac ya da dosyadaki sayaci tavana esitle): tavan dolunca log'da `[MMODialogue] Gunluk LLM butcesi doldu` gorulmeli, NPC "(NPC su an konusamiyor.)"/mevcut yedek replikle konusmali, oyun akisi bozulmamali. `-MMOLlmDaily=0` sinirsiz yapar.
- [ ] Mevcut dakikalik sinir (30/dk), eszamanli sinir (4) ve `MaxSessionRequests` (400) eskisi gibi calismali.

## 25. QA 2026-10-07 ORTA-2/ORTA-3: PCH'siz derleme kirigi + JSON ice aktarma sikilastirildi (sema v2)

Kod derlendi (Result: Succeeded), RunAllChecks (-SkipBuild -FullCompile) GECTI: PCH'siz tek-dosya /W4 (10 dosya) 0 uyari, JSON testi `PASS schema=2 negative=109`, magaza testi gercek 154 .sav -> JSON -> alan alan esit (json ok=154 mismatch=0; makinedeki en yuksek seviye 6). PIE'de OYNANMADI; FromJson su an oyun kodu tarafindan KULLANILMIYOR (yalniz test/ileride bulut magazasi), yani oynanista degisiklik beklenmez; yerel .sav formati ve okumasi degismedi.

- [ ] Karakteri yukle/kaydet (MMOSave, MMOLoad, bolge gecisi): eskisi gibi calismali (yerel .sav yolu degismedi).
- [ ] `Scripts\TestCharacterSaveJson.ps1` calistir: `PASS schema=2 ... negative=109` (negatif sayi artarsa/azalirsa test kapsami degisti). Editor kapali olmali.
- [ ] `Scripts\RunAllChecks.ps1 -SkipBuild -FullCompile`: "PCH'siz tek-dosya" adimi her dosya OK. Bir `#include` silip tekrar dene: o dosya HATA vermeli (CI/manuel kullanim; varsayilan RunAllChecks bu adimi atlar).
- [ ] Eski kayitli (Lv30+, depo/klan/gorev dolu) bir karakterin .sav dosyasi bu makinede varsa `TestCharacterStore.ps1` ciktisindaki `json ok=N mismatch=0 maxLevel=` satirinda maxLevel o seviyeyi gostermeli (su an en yuksek 6; yuksek seviyeli gercek kayit oynanip kaydedildikten sonra tekrar kos).

## 26. QA 2026-10-07 ORTA-1 (guvenli kisim): yuva basina yazim sirasi + bolge gecisi bariyer/dondurme duzeltmeleri

Kod derlendi, RunAllChecks (-SkipBuild -FullCompile) GECTI (bolge gecisi + zaman asimi headless testleri dahil; magaza testine `writeorder ok=1` eklendi). PIE'de iki oyunculu OYNANMADI. Yerel magaza SENKRON oldugu icin "eski yazim sonradan indi" durumu bugun TETIKLENEMEZ: yeni yollar yalniz bulut magazasi icin hazirliktir. Mimari kisimlar (yuva kuyrugu/Revision, ucustaki yazimin iptali, anlik goruntu-Finish penceresi) KODLANMADI, docs/NETWORK_AUDIT.md O6.

- [ ] Host + uzak istemci, ikisi canli ve savas disi: `MMOTravel` -> ikisi de Winter'a gecmeli (eskisi gibi); host loguna `Zone travel started` (2 oyuncu) yazilmali, `Save superseded` uyarisi OLMAMALI.
- [ ] `MMOTravel`'i basarili bir geciste ard arda iki kez bas (gecis sirasinda): ikinci istek reddedilmeli (`Gecis su anda kullanilamiyor`), ilk gecis bozulmamali.
- [ ] Gecis reddedilince (ornegin biri savasta: `Tum oyuncular canli ve savas disinda olmali`) host konsol/HUD mesajinin reddi gostermesi ve hemen ardindan savastan cikip tekrar denenince gecisin kabul edilmesi (kilit/test kancasi kalintisi yok).
- [ ] Elle `MMOSave` ile gecis ayni karede ust uste: gecis istegi sirasinda `Save superseded` logu cikarsa gecis iptal olmali ve tekrar denenince calismali (yerel magazada normalde cikmaz).

## 27. QA 2026-10-07 ORTA-4 + DUSUK-2/4/5: denetim logu Shipping siniri (belge), tuketilebilir geri bildirimi, HUD sinir, hub onbellegi

Kod derlendi, RunAllChecks (-SkipBuild -FullCompile) sonucu commit mesajinda. PIE'de OYNANMADI. `bUseLoggingInShipping` EKLENMEDI (UBT: unique build environment gerektirir, kaynak motor/Shipping derlemesi bu makinede dogrulanamaz); NETWORK_AUDIT O12 sinirlari yazdi.

- [ ] Can iksirini ~0.5 sn arayla iki kez bas (host/yerel): ikinci basista "Esya kullanimi bekleniyor" ipucu cikmali, esya harcanmamali; 1 sn+ arayla basinca her seferinde calismali.
- [ ] Uzak istemcide 200 ms gecikme + 100 ms titresim (`Net PktLag=200 PktLagVariance=100`) ile iksire ~1.0 sn aralikla bas: sunucu cooldown'u 0.85 sn oldugundan basislar sessizce yutulmamali (eskiden bazilari yutulabiliyordu); cok hizli (<0.5 sn) basista ipucu uzak istemcide gorunmeli.
- [ ] Yogun savasta (cok mob + HoT/leash sifasi) ekrandaki ucan hasar/sifa sayisi 64'u gecmemeli; eski sayilar erken kaybolabilir, oyun/FPS etkilenmemeli.
- [ ] Hub (PlayerStart) yakininda saldiri reddi ilk karede ve Summer->Winter->Summer sonrasi eskisi gibi calismali (onbellek: bos sonuc saklanmaz, dolu kalici).

## 28. QA 2026-10-08 ORTA-2: gecis kaydi ucustayken diger yazimlari ertele

Kod derlendi; `RunAllChecks.ps1 -SkipBuild -FullCompile` tum 12 adimda GECTI. Timeout testi gecis
kayit callback'ini bilerek dusururken ikinci kaydin `saved=0 deferred=1 inflight=1` oldugunu, gecis
iptalinden sonra ertelenen kaydin `saved=1` ile yazildigini dogruladi. Yerel magaza callback'i ayni
karede dondugu icin gercek gecikmeli pencere yalniz test kancasi veya ilerideki bulut magazasinda
olusur. PIE'de iki oyunculu OYNANMADI.

- [ ] Gecikmeli magaza ile host + uzak istemci gecisi baslat; kayit callback'leri beklerken otomatik
  kayit, `MMOSave`, klan ve ticaret kaydi tetikle. Gecis yazimi bayat sayilmamali; ek yazimlar
  birlestirilip gecis tamamlaninca yazilmali.
- [ ] Ayni senaryoda bir callback'i zaman asimina ugrat: herkes kaynak haritada kalmali, ertelenen
  kayit iptalden sonra yazilmali ve hemen sonraki gecis istegi kabul edilmelidir.
- [ ] Kayit ucustayken bir oyuncunun baglantisini kes: `EndPlay` zorunlu kaydi ertelememeli; gecis
  ayrilan oyuncu nedeniyle iptal olmali ve kalan oyuncularin gecis kilidi temizlenmelidir.

## 29. QA 2026-10-08 ORTA-3: JSON derinlik, yazma-okuma simetrisi ve enum donusumu

`FromJson`, ayristiriciyi cagirmadan once ham nesne/dizi derinligini 16 ile sinirlar;
string icindeki parantezleri ve kacisli tirnak/ters egik cizgileri saymaz. Bilinmeyen ek
alanlar da bu tavana tabidir. `ToJson` artik ayni `CheckRanges` ve yapisal boyut/tip/enum
denetimlerini kullanir; okuyucunun reddedecegi Level=0 veya 4097 elemanli Friends yazilmaz.
Sayisal enum, int64'e donusturulmeden once [-2^63, 2^63) araliginda dogrulanir.

UE 5.7 editor derlemesi GECTI. `RunAllChecks.ps1 -SkipBuild -FullCompile` 12/12 GECTI
(10 dosya PCH'siz /W4 temiz; Summer/Winter smoke; NetSmoke 1 sunucu/1 istemci baglantisi,
0 hata). Ardindan yalniz regresyon fixture'lari guclendirildi: gecerli karakter blob'unda
derinligi 17 olan bilinmeyen alan reddi, 16 sinirinin kabul edilmesi ve kacisli string
parantezlerinin sayilmamasi. Son kod yeniden derlendi; JSON `schema=2 negative=111` ve
magaza `slots=178 loaded=178 invalid=0 loadFailed=0 json ok=178 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1` GECTI. Fixture degisikliginden sonra tum smoke
paketi tekrarlanmadi. PIE'de OYNANMADI; JSON halen yalniz test/gelecekte bulut yoludur.
Yerel .sav formati degismedi. Sema alan ekleme/goc mekanizmasi ve Lv30+ gercek kayit
kaniti aciktir. Agency-agents AppSec ve Test Results Analyzer rol belgeleri incelemede
kullanildi; bagimsiz paralel ajan calistirilmadi.

- [ ] Iki oyunculu PIE'de `MMOSave`, yeniden giris ve Summer/Winter gecisini oyna;
  mevcut yerel karakter kayitlari ve ilerleme yuklenmeye devam etmelidir.
- [ ] Depo/klan/gorev dolu Lv30+ karakter kaydedildikten sonra `TestCharacterStore.ps1`
  ile `json mismatch=0` ve `maxLevel>=30` kanitini al (bu makinede en yuksek 6).
- [ ] Bulut magaza entegre edildiginde JSON reddi mevcut kaydi ezmemeli; yazma reddi
  cagirana gorunur olmali. Bu entegrasyon bugun yoktur ve dogrulanmadi.

## 30. O6 / QA ORTA-2: gecis oncesi oyuncu kumesi ve savas durumunu yeniden dogrula

`FinishZoneTravel`, kayit callback'leri tamamlandiktan sonra PlayerController/pawn kumesini
ilk kaydi alinan karakterlerle karsilastirir. Yeni oyuncu, pawn degisimi, ayrilma, olum veya
savas durumunda `ServerTravel` baslatilmaz; normal iptal yolu kilidi/bariyeri ve zamanlayiciyi
temizler. Boylece gecikmeli onay, ilk istegin canli/savas disi kosullarini bayatlatamaz.

UE 5.7 editor derlemesi ve `RunAllChecks.ps1 -SkipBuild -FullCompile` **13/13 GECTI**:
10 dosya PCH'siz /W4, 0 derleyici uyarisi; JSON 111 negatif; 179 gercek kayit icin
`loaded=179 json ok=179 mismatch=0 maxLevel=6`; normal gecis, timeout ve yeni durum
regresyonu; Summer/Winter smoke 0 hata; NetSmoke 1 sunucu/1 istemci katilimi, 0 hata.
Yeni `TestZoneTravelStateChange.ps1`, yerel kayit onayini dusurup savasa girisi ve sonra
onayi taklit eder: `PASS combat=1 lock=0 pending=0`, zamanlayici temiz. Gercek asenkron
bulut/dedicated server testi degildir. Olum/pawn/yeni oyuncu dallari iki oyunculu PIE'de
oynanmadi; `PLAYTEST_CHECKLIST.md`'de bekliyor. XP/altin anlik goruntu penceresi ve
ertelenen yazim hatasinin gecise iletilmesi acik (NETWORK_AUDIT O6).

Agency-agents Unreal Multiplayer Architect rol belgesi authority/oyuncu kumesi
incelemesinde kullanildi; bagimsiz paralel ajan calistirilmadi.

## 31. O6: ertelenen kayit hatasinda gecisi durdur ve yeniden deneme isaretini koru

`FlushDeferredSave` sonucunu `FinishZoneTravel` artik denetler: ertelenen ek kayit
yazilamazsa `bTravelSaveCommitted` yapilmaz ve `ServerTravel` baslamaz. Bekleyen kayit
isareti ancak basarili `SaveCharacterForTravel` ile temizlenir. Iptalden sonra tekrar
yazim da basarisizsa isaret korunur; elle/otomatik/zorunlu kayit basarinca temizlenir.
Elle kayit geri bildirimi de bekleyen isarete degil gercek ucustaki gecis kaydina bakar:
iptal sonrasi disk hatasini "gecis suruyor" diye gostermemelidir.

UE 5.7 editor derlemesi ve `RunAllChecks.ps1 -SkipBuild -FullCompile` **14/14 GECTI**:
10 dosya PCH'siz /W4, 0 derleyici uyarisi; JSON 111 negatif; 184 gercek kayit
`loaded=184 json ok=184 mismatch=0 maxLevel=6`; normal/timeout/durum/ertelenen hata
gecis testleri; Summer/Winter smoke 0 hata; NetSmoke 1 sunucu/1 istemci katilimi,
0 hata. Yeni `TestZoneTravelDeferredSave.ps1`: iki flush hatasi, iptal ve tekrar kayit
`PASS lock=0 kept=1 retry=1 cleared=1`. Hata enjeksiyonu yazim oncesinde false doner;
gercek disk arizasi/bulut testi DEGILDIR. Son geri bildirim duzeltmesi sonrasi
`MMOCharacter.cpp` PCH'siz derlemesi ve normal UE editor derlemesi yeniden GECTI;
ertelenen hata ve timeout regresyonlari tekrar GECTI. Bu yalniz geri bildirim degisikligi
sonrasinda butun smoke paketi tekrarlanmadi; mesajlarin PIE gorunumu aciktir.

PIE'de iki oyunculu OYNANMADI. Disk/bulut hatasini kullaniciya bildirme ve ariza giderilip
yeniden gecis denemesi `PLAYTEST_CHECKLIST.md`'de acik. Depodaki yazim sirasi/iptal ve
anlik goruntu-sonrasi ilerleme penceresi acik; bu degisiklik bunlari kapatmaz.

## 32. O13: mevcut partileri birlestirmede lider yetkisi (2026-10-07)

`FormOrJoin`, her iki tarafin tum uyelerini topladigi halde lider yetkisi istemiyordu.
Bir uye kabul ederek kendi grubunun tamamini tasiyabiliyor, uye davetci baska grubu
kendi grubuna ekleyebiliyordu. `CanCombineParties` davet edilen mevcut grubun liderini,
davetci de gruptaysa onun liderligini davet ve kabul aninda denetler. Parti uyelerinin
partisiz oyuncuyu davet etmesi korunur. Ayni grup kabulunde liste/lider degismez.
Mutasyon kapisinda authority kontrolu de vardir; uyelik/lider replikasyon yolu korunur.

UE 5.7 editor derlemesi GECTI. `RunAllChecks.ps1 -SkipBuild -FullCompile` **15/15 GECTI**:
11 dosya PCH'siz /W4, 0 derleyici uyarisi (parti dosyasi listeye eklendi); JSON 111
negatif; 191 gercek kayit `loaded=191 invalid=0 json ok=191 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`; normal/timeout/durum/ertelenen hata gecis
testleri; yeni parti yetki testi; Summer 51 / Winter 29 mob dogumu, 0 hata; NetSmoke
1 server join / 1 client welcome, iki tarafta 0 hata. Veri beceri boslugu Lv33+ ve
StaticLint `InitGameState` beyaz liste uyarilari onceki gibi acik, hata degildir.

`TestPartyConsent.ps1`: `PASS checks=10`. Iki solo grup kurulumu; iki tarafta uye ile
birlesme reddi; beklerken iki tarafta liderlik degisimi; ayni grup; lider onayli
birlesme; uye -> solo davet; sekiz kisi; dokuzuncunun reddi denetlendi. Fixture'lar
controllersiz karakterlerdir; pending daveti test sunucuda kurar. Shipping'de test
komutu derlenmez. Gercek sahip-istemci RPC, HUD/replication ve dedicated testi DEGILDIR.
PIE'de en az dort oyunculu kontrol `PLAYTEST_CHECKLIST.md`'de bekliyor; oynanmadi.

Agency-agents Unreal Multiplayer Architect ve Test Results Analyzer rol belgeleri
kullanildi; bagimsiz paralel ajan calistirilmadi. Siradaki yuksek etkili isler: O6
snapshot-sonrasi ilerleme/yazim sirasi, iki-surecli admin/PvP dogrulamasi, mevcut
Lv1-30 dikey dilim eksikleri. Bu tur yalniz parti birlesme yetkisini kapatti.

## 33. O6: gecis kayit callback'lerini istek/karakter basina tek kez uygula (2026-10-07)

`HandleZoneTravelSaveComplete` eskiden hata sonucunu completed denetiminden ONCE
isliyordu. Basarili oyuncu icin sonradan gelen ikinci hata, diger oyuncular beklerken
tum gecisi iptal edebiliyordu. `ServerTravel` basladiktan sonra ayni seri numarasinin
gec sonucu da eski GameMode gecis kilidini temizleyebiliyordu. Simdi once seri numarasi,
pending kumesi ve tamamlanmis karakter denetlenir; ikinci sonuc/yabanci karakter/gec
sonuc bariyere dokunmaz. Ilk hata ve gecersizlesmis pending karakter iptal yoluna girer.
Oyun is parcacigi sozlesmesi check ile denetlenir. Yerel magaza callback'i normalde
tek/senkron gelir; bu sertlestirme gelecekteki gecikmeli magaza hazirligidir.

Normal UE 5.7 editor derlemesi GECTI. `RunAllChecks.ps1 -SkipBuild -FullCompile`
**16/16 GECTI**: 11 dosya PCH'siz /W4, 0 derleyici uyarisi; JSON 111 negatif;
196 yerel yuva `loaded=196 invalid=0 loadFailed=0 json ok=196 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`; normal/timeout/durum/ertelenen hata gecisleri;
parti 10 senaryo; yeni callback 9 senaryo; Summer 52 / Winter 31 mob dogumu, 0 hata;
NetSmoke 1 server join / 1 client welcome, iki tarafta 0 hata. Lv33+ beceri boslugu
ve StaticLint InitGameState beyaz liste uyarilari onceki gibi acik.

`TestZoneTravelCallbacks.ps1`: `PASS checks=9`, iki beklenen iptal logu. Gercek GameMode
callback girisi, uc controllersiz fixture karakterle denetlenir; pending bariyeri ve
travel-basladi fazini test kodu kurar. Gercek bulut, iki oyunculu async callback ya da
motor travel fazi testi DEGILDIR; PIE listesinde bekliyor. Normal travel ayri regresyon
testinde basladi. Test komutunun Shipping govdesi bostur. Fixture oyuncu kaydi yazmaz.

Agency-agents Backend Architect (idempotency/terminal sonuc) ve Unreal Multiplayer
Architect rol belgelerinden yararlanildi; bagimsiz paralel ajan calistirilmadi.
O6'nin depoda Revision/seri yazim/iptal ve snapshot-sonrasi XP/altin penceresi acik.
Sonraki sinirli is secimi bu konularin veya iki-surecli admin/PvP dogrulamasinin
mevcut kaynakta kanitlanabilen kismindan yapilacak; bu tur O6 tumden kapatilmadi.

## 34. O14: bekleyen klan davetinde guncel lider yetkisi (2026-10-07)

`CmdInvite` lider yetkisi istiyordu, fakat `CmdAccept` davetcinin yalniz ayni klanda
kalmasini denetliyordu. Asil lider yeniden baglaninca `ApplySaveData` vekil lideri
uyeye indirir; bu uyenin bekleyen daveti eski yetkiyle yeni uye alabiliyordu.
Kabul aninda `bIsLeader` yeniden denetlenir. Yetkisini kaybeden davetcinin pending
daveti tuketilir, hedefin uyeligi degismez. Yetkili asil/vekil lider kabul yolu korunur.
Sunucu otoritesi ve mevcut uyelik replikasyonu korunur.

Regresyon once duzeltmesiz UE'de calistirildi: `FAIL checks=0 reason=demoted inviter
accepted` (motor exit=0, test script exit=1). Duzeltme sonrasi
`TestClanInviteAuthority.ps1` **PASS checks=9**: yetkisini kaybeden davetci, asil lider,
yetkili vekil, mevcut uyelik, klandan ayrilan davetci, gecersiz zaman damgasi, kendine
davet, eksik davetci ve yok edilmis davetci. Bu izole test pending daveti ve rol
degisimini sunucuda kurar; controllersiz/ilk kaydi yuklenmemis fixture'lar oyuncu
kaydi yazamaz. Shipping'de test komutu derlenmez. Gercek lider yeniden baglantisi,
sahip-istemci sohbet RPC ve HUD/rol replikasyonu testi DEGILDIR.

Normal UE 5.7 editor derlemesi GECTI. `RunAllChecks.ps1 -SkipBuild -FullCompile`
**17/17 GECTI**: 12 dosya PCH'siz /W4, 0 derleyici uyarisi (klan dosyasi eklendi);
JSON 111 negatif; 201 yerel yuva `loaded=201 invalid=0 loadFailed=0 json ok=201
mismatch=0 maxLevel=6`, `writeorder ok=1`, `roundtrip equal=1`; normal/timeout/durum/
ertelenen hata gecisleri; parti 10, callback 9, klan 9 senaryo; Summer 48 / Winter 35
mob dogumu, 0 hata; NetSmoke 1 server join / 1 client welcome, iki tarafta 0 hata.
NetSmoke yalniz baglanti dogrulamasidir. Lv33+ beceri boslugu ve StaticLint
InitGameState beyaz liste uyarilari onceki gibi acik.

En az uc oyunculu PIE adimlari `PLAYTEST_CHECKLIST.md`'ye eklendi; OYNANMADI.
Agency-agents AppSec Engineer ve Unreal Multiplayer Architect rol belgelerinden
yararlanildi; bagimsiz paralel ajan calistirilmadi. O14 kod yolu duzeltildi, gercek
yeniden baglanti/kalici uyelik dogrulamasi bekliyor. O6 snapshot-sonrasi XP/altin
penceresi ve magaza yazim sirasi/iptal konulari acik kalir.

## 35. O6: ilk snapshot sonrasi XP/altini gecis oncesi yeniden kaydet (2026-10-07)

`FlushDeferredSave` yalniz yeni kayit istegi ertelenmisse yaziyordu. Ilk gecis kaydi
alinip onay beklenirken pasif XP/ganimet gibi `SaveCharacter` cagirmayan mutasyonlar
diske gecmiyordu; basarili onaydan sonra eski snapshot guncelmis gibi kullaniliyordu.
`FinishZoneTravel`, tum onaylar tamamlaninca her oyuncuda `FlushDeferredSave(true)`
ile guncel snapshot'i senkron yeniler. Son yazim hata verirse gecis iptal edilir;
bekleyen isaret korunur, iptal sonrasi tekrar denenir. Basari isareti temizler.
Kayit semasi/replikasyon degismedi; sunucu otoritesi korunur.

Yeni `TestZoneTravelProgress.ps1` once duzeltmesiz UE'de **FAIL** verdi:
`changed=1 xp=0 gold=0 started=1 cleared=1` (script exit=1). Ilk yerel yazimin
callback'i test kancasiyla dusuruldu; 37 XP ve 23 altin sunucuda eklendi, ek kayit
istegi verilmedi. Gercek GameMode callback girisinden sonra diskteki son kayit
okundu. Duzeltme sonrasi **PASS changed=1 xp=1 gold=1 started=1 cleared=1**:
seviye/XP ve altin guncel degerlerle eslesir. Bu test gercek bulut gecikmesi ya da
hedef haritada yeniden yukleme testi DEGILDIR. Normal gecis testi Winter'i ayrica yukler.
`TestZoneTravelDeferredSave.ps1` da artik ek kayit istegi OLMADAN son snapshot icin
iki yazim hatasi enjekte eder: iptal, isaret korunmasi ve basarili yeniden kayit GECTI.

Normal UE 5.7 editor derlemesi GECTI. `RunAllChecks.ps1 -SkipBuild -FullCompile`
**18/18 GECTI**: 12 dosya PCH'siz /W4, 0 derleyici uyarisi; JSON 111 negatif;
208 yerel yuva `loaded=208 invalid=0 loadFailed=0 json ok=208 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`; normal/timeout/durum/son yazim hatasi ve yeni
ilerleme regresyonlari; parti 10, callback 9, klan 9 senaryo; Summer 51 / Winter 34
mob dogumu, 0 hata; NetSmoke 1 server join / 1 client welcome, iki tarafta 0 hata.
NetSmoke baglanti kontroludur. Lv33+ beceri boslugu ve InitGameState beyaz liste
uyarisi ayni; hata degil, acik islerdir.

Sinir/maliyet: yerel magazada her geciste karakter basina ikinci senkron yazim var.
Bulut magazasi geldiginde bu son snapshot icin de async bariyer ve depoda seri yazim/
iptal gerekir. Son yazimdan `ServerTravel`/pawn yok olusuna kadarki oynanis penceresi
KAPANMADI. Iki oyunculu gercek PIE, hedef haritada deger yukleme, motor travel hatasi
ve dedicated dogrulamasi acik; adimlar `PLAYTEST_CHECKLIST.md`'ye eklendi.
Agency-agents Backend Architect ve Unreal Multiplayer Architect rol belgeleri
kullanildi; bagimsiz paralel ajan calistirilmadi. O6 tumden kapanmadi.

## 36. O15: oluyken gorev tesliminde XP odulunun kaybolmasini onle (2026-10-07)

`ClaimReward` olum bayragina bakmiyordu. `GainExperience` oluyken erken dondugu
halde altin/sarf veriliyor, gorev tamamlanmis listeye tasiniyordu. Boylece XP odulu
kalici kayboluyor ve bildirim tam odul alinmis gibi gorunuyordu. Kabul ve odul
kapilarina otorite dalinda karakter/stat varligi ve `!bIsDead` denetimi eklendi.
Oluyken gorev/oduller degismez; dirilince eski yakinlik, kapasite ve tek sefer teslim
kurallariyla alinabilir. Kayit formati ve mevcut owner-only replikasyon korunur.

Regresyon once duzeltmesiz UE'de calisti: `FAIL checks=3 reason=dead claim consumed
quest or granted partial reward` (motor exit=0, script exit=1). Duzeltme sonrasi
`TestQuestRewardLifeState.ps1` **PASS checks=8**: canli kabul; eksik ilerleme reddi;
tamamlanmaya hazir fixture; olu teslimde gorev/XP/altin/iksir korunmasi; olu kabul
reddi; Revive sonrasi tam XP/altin/iksir; yinelenen teslim reddi; uzak NPC teslim
reddi. Beklenen XP, ayri bir stats fixture'ina odul verilerek seviye/esik dusumuyla
karsilastirilir. Iki controllersiz karakter ve ayri NPC kullanilir; ilerleme ve
`bIsDead` testte kurulur. Disk kaydi yazilmaz, gercek oyunculara dokunulmaz.
Shipping'de test komutu derlenmez. Normal UE 5.7 editor derlemesi GECTI.

`RunAllChecks.ps1 -SkipBuild -FullCompile` **19/19 GECTI**: 13 dosya PCH'siz /W4,
0 derleyici uyarisi (gorev bileseni listeye eklendi); JSON 111 negatif; 214 yerel yuva
`loaded=214 invalid=0 loadFailed=0 json ok=214 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`; normal/timeout/durum/son yazim hatasi/ilerleme
gecis regresyonlari; parti 10, callback 9, klan 9, gorev 8 kontrol; Summer 50 /
Winter 32 mob dogumu, 0 hata; NetSmoke 1 server join / 1 client welcome, iki tarafta
0 hata. NetSmoke yalniz baglanti kontroludur. Lv33+ beceri boslugu ve StaticLint
InitGameState beyaz liste uyarilari onceki gibi acik.

Gercek olum/respawn, sahip-istemci RPC/HUD ve kaydet/yeniden giris PIE'de OYNANMADI;
adimlar `PLAYTEST_CHECKLIST.md`'ye eklendi. Agency-agents Unreal Multiplayer Architect
ve Test Results Analyzer rol belgelerinden yararlanildi; bagimsiz paralel ajan
calistirilmadi. O6 son snapshot-pawn yok olusu, magaza yazim sirasi/iptal ve dedicated
hazirligi acik kalir. Bu tur yalniz gorev kabul/odul canlilik kuralini kapatti.

## 37. O16: olu karakterin ganimet toplamasini engelle (2026-10-07)

`AMMOLootPickup::TryCollect` oyuncu kontrolunu denetliyordu fakat olum bayragini
denetlemiyordu. Ceset overlap'i ve rezervasyon bitiminde ayni kapi altin/sarf/ekipman
verebiliyordu. Butun grant islemlerinden once sunucuda stats varligi ve `!bIsDead`
denetimi eklendi. Reddedilen ganimet normal omru boyunca yerde kalir; canli toplamada
mevcut rezervasyon, canta/yigin kapasitesi ve tek sefer guard'i uygulanir.
Diriliste otomatik tekrar deneme veya ganimet omrunu uzatma eklenmedi.

Test duzeltmesiz UE'de **FAIL checks=0 reason=dead gold collected** verdi (motor
exit=0, script exit=1). Duzeltme sonrasi `TestLootLifeState.ps1` **PASS checks=10**:
olu altin/sarf/ekipman reddi, bitmis rezervasyonda olu reddi; Revive sonrasi uc turun
tam grant'i ve her tur icin yinelenen grant reddi. Sarf oluluk denemesi `OnOverlap`
girisi uzerinden yapilir. Collision kapali izole loot fixture'lari, AMMOCharacter ve
test PlayerController kullanilir. Bayrak ve rezervasyon zamani testte kurulur;
gercek timer/overlap listesi oynanmaz. Ayni senkron cagrida unpossess/yok etme sayesinde
ilk yukleme/autosave calismaz; oyuncu kaydi yazilmaz. Shipping'de test komutu yoktur.
Normal UE 5.7 editor derlemesi GECTI. Loot dosyasi PCH'siz derleme listesine ve yeni
regresyon tam kontrol paketine eklendi.

`RunAllChecks.ps1 -SkipBuild -FullCompile` **20/20 GECTI**: 14 dosya PCH'siz /W4,
0 derleyici uyarisi; JSON 111 negatif; 220 yerel yuva `loaded=220 invalid=0
loadFailed=0 json ok=220 mismatch=0 maxLevel=6`, `writeorder ok=1`,
`roundtrip equal=1`; gecis normal/timeout/durum/son yazim hatasi/ilerleme;
parti 10, callback 9, klan 9, gorev 8, loot 10 kontrol; Summer 51 / Winter 33
mob dogumu, 0 hata; NetSmoke 1 server join / 1 client welcome, iki tarafta 0 hata.
NetSmoke yalniz baglantidir; gercek ganimet replikasyonu/PIE degildir. Lv33+ beceri
boslugu ve StaticLint InitGameState beyaz liste uyarilari onceki gibi acik.

Gercek ceset/respawn, timer overlap listesi, uzak istemci replication/HUD ve kalicilik
PIE'de OYNANMADI; adimlar `PLAYTEST_CHECKLIST.md`'ye eklendi. Agency-agents Unreal
Multiplayer Architect ve Test Results Analyzer rol belgeleri kullanildi; bagimsiz
paralel ajan calistirilmadi. O6 son snapshot-pawn yok olusu, magaza yazim sirasi/iptal
ve dedicated hazirligi acik kalir. Bu tur O16 canlilik kuralini kapatti.

## 38. O17: dolu can/manada saf iksiri ve beklemeyi harcama (2026-10-07)

`UseConsumable` buyuk can ve normal/buyuk mana iksirini ilgili kaynak dolu olsa da
harciyordu; Heal/RestoreMana clamp'i faydayi sifirliyordu. Saf tanimda can veya mana
yenileme ihtiyaci yoksa, esya ve ortak cooldown'dan once reddedilir. EffectId tasiyan
parsomenler tam kaynakta da kullanilir; sifir mana havuzunda mana iksiri korunur.
Sunucu RPC ayni kontrolu uygular; istemci on kontrolune guvenilmez. Kayit/replikasyon
semasi degismez. Birden fazla kaynakli saf iksir gelecekte varsa herhangi bir eksik
kaynak yeterlidir; mevcut veride hibrit tanim yok, hibrit runtime senaryosu denenmedi.

Regresyon duzeltmesiz UE'de **FAIL checks=0 reason=full health consumed potion or
cooldown** verdi (motor exit=0, script exit=1). Sonrasinda **PASS checks=11**:
dolu buyuk can/normal mana/buyuk mana; sifir mana havuzu; gerekli can/mana yenileme;
ortak bekleme; olu kullanim reddi; tam kaynakta Rage parsomeni; gecersiz tanim ve
normal can iksiri reddi. Controllersiz fixture, dogrudan sunucu girisi ve testte
kurulan vital/cooldown degerleri kullanildi; oyuncu kaydi yazilmaz. Shipping test
komutu derlenmez. Normal UE 5.7 editor derlemesi GECTI.

`RunAllChecks.ps1 -SkipBuild -FullCompile` **21/21 GECTI**: 14 dosya PCH'siz /W4,
0 derleyici uyarisi; JSON 111 negatif; 226 yerel yuva `loaded=226 invalid=0
loadFailed=0 json ok=226 mismatch=0 maxLevel=6`, `writeorder ok=1`,
`roundtrip equal=1`; gecis normal/timeout/durum/son yazim hatasi/ilerleme;
parti 10, callback 9, klan 9, gorev 8, loot 10, iksir 11 kontrol;
Summer 45 / Winter 29 mob dogumu, 0 hata; NetSmoke 1 server join / 1 client welcome,
iki tarafta 0 hata. NetSmoke yalniz baglanti kontroludur; gercek iksir RPC/PIE degildir.
Lv33+ beceri boslugu ve StaticLint InitGameState beyaz liste uyarilari onceki gibi acik.

Gercek sahip-istemci RPC/gecikme/HUD ve yeniden giris PIE'de OYNANMADI; adimlar
`PLAYTEST_CHECKLIST.md`'ye eklendi. Agency-agents AppSec Engineer ve Test Results
Analyzer rol belgeleri kullanildi; bagimsiz paralel ajan calistirilmadi. O6 kalan
gecis penceresi, magaza yazim sirasi/iptal ve dedicated hazirligi acik kalir.

## 39. Y-2 kismi: kayitli ekipmanda seviye sartini uygula (2026-10-07)

Normal EquipByInstanceId seviye sartini denetlerken ApplySaveData yalniz slotu
denetliyordu. Lv1 kaydinda kusanilmis LongSword (Lv3) / PlateArmor (Lv5), yuklemede
bonus verebiliyordu. Stats once yuklenir; bilinen ekipmanin slot ve RequiredLevel
sarti birlikte denetlenir. Uygunsuz esya canta, canta doluysa depoya tasinir; slot
bosaltilip bonuslar yeniden hesaplanir. Mevcut kayipsiz yukleme politikasi depo
kapasitesinin asilmasina izin verir. Bilinmeyen esya kimligi koruma politikasi,
benzersiz InstanceId duzeltmesi ve normal kusanma kurali korunur. Stats bulunmazsa
bilinen ekipman da geri tasinir. Kayit/replikasyon semasi degismez.

Regresyon duzeltmesiz UE'de **FAIL checks=0 reason=underlevel saved equipment
remained equipped** verdi (motor exit=0, script exit=1). Duzeltme sonrasi
TestSavedEquipmentLevel.ps1 **PASS checks=11**: dusuk seviyede slot ve bonus reddi,
kimlik/+ /nadirlik korunmasi, yeniden kusanma reddi, tam gereken seviye, bilinmeyen
kimlik ve yasal zirh, yanlis slot, dolu canta/depo kurtarma, bellek roundtrip'i ve
cakisan InstanceId ayrilmasi. Controllersiz izole karakter kullanilir, oyuncu
diski yuklenmez/yazilmaz. Shipping'de test komutu yoktur. Fixture'in ilk derlemesinde
protected bonus alanina erisim hatasi duzeltildi; test public turetilmis statlari
karsilastirir. Son normal UE 5.7 editor derlemesi GECTI.

`RunAllChecks.ps1 -SkipBuild -FullCompile` **22/22 GECTI**: 14 dosya PCH'siz /W4,
0 derleyici uyarisi; JSON 111 negatif; 232 yerel yuva `loaded=232 invalid=0
loadFailed=0 existsMismatch=0 json ok=232 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`; gecis normal/timeout/durum/son yazim
hatasi/ilerleme; parti 10, callback 9, klan 9, gorev 8, loot 10, iksir 11,
kayitli ekipman 11 kontrol; Summer 53 / Winter 33 mob dogumu, 0 hata;
NetSmoke 1 server join / 1 client welcome, iki tarafta 0 hata. NetSmoke yalniz
baglanti kontroludur; ekipman gameplay/replikasyon testi degildir. Lv33+ beceri
boslugu ve StaticLint InitGameState beyaz liste uyarilari onceki gibi acik.

Gercek dosyadan yeniden giris, sahip-istemci RPC/HUD ve replikasyon PIE'de OYNANMADI;
adimlar PLAYTEST_CHECKLIST.md'ye eklendi. Agency-agents AppSec Engineer, Backend
Architect ve Test Results Analyzer rol belgeleri kullanildi; bagimsiz paralel ajan
calistirilmadi. Bu tur Y-2'nin yalniz ekipman seviye sartini kapatir; imza/hesap
kimligi, diger yukleme denetimleri, O6 kalan gecis penceresi, magaza yazim sirasi/
iptal ve dedicated hazirligi ayri calisma olarak acik kalir.

## 40. Satici ekonomi kurallarinin UE regresyonu (2026-10-07)

Satici kapasite/canlilik/mesafe korumalari kodda vardi; bu tur bunlar icin otomatik
UE fixture'i ekledi. Oynanis kurali degismedi, yeni acik bulundu/duzeltildi iddiasi
yoktur. TestVendorEconomy.ps1 ve Development-only MMOTestVendorEconomy komutu,
izole controllersiz karakter ve yakin satici ile **PASS checks=14** verdi:

- Ekipmanin dogru fiyatla tek teslimi; dolu cantada ucret alinmamasi.
- Yetersiz altin ve bilinmeyen tanimda esya/altin degismemesi.
- Olu karakterin uc satin alma yolu ve satisi reddi; ayni yollarin uzaktan reddi.
- MAX_int32 iksir isteginde 999 tavan ve dogru ucret; dolu yiginda ucret yok.
- Bir adet yer varken can/mana iksirinde yalniz bir adedin ucreti.
- Dolu mana yigini; 64 farkli yiginda yeni yigin reddi; mevcut yigini tamamlama.
- Ayni InstanceId'nin iki satis isteginde yalniz bir kez satilmasi.

Bayrak/konum/yiginlar fixture'da kurulur; sunucu metotlari dogrudan cagirilir.
Gercek owning-client RPC tasimasi, HUD, gecikme, disk roundtrip'i veya dedicated
hedef testi degildir. Gercek oyuncu kaydi yuklenmez/yazilmaz; karakter ve satici
senkron test bitiminde yok edilir. Test komutu Shipping'de derlenmez. Mevcut
satinalma/satis ve replikasyon semasi degismedi. Normal UE 5.7 editor derlemesi GECTI.

`RunAllChecks.ps1 -SkipBuild -FullCompile` **23/23 GECTI**: 14 dosya PCH'siz /W4,
0 derleyici uyarisi; JSON 111 negatif; 238 yerel yuva `loaded=238 invalid=0
loadFailed=0 existsMismatch=0 json ok=238 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`; gecis normal/timeout/durum/son yazim
hatasi/ilerleme; parti 10, callback 9, klan 9, gorev 8, loot 10, iksir 11,
kayitli ekipman 11 ve satici 14 kontrol. Summer 49 / Winter 33 mob dogumu,
0 hata; NetSmoke 1 server join / 1 client welcome, iki tarafta 0 hata.
NetSmoke yalniz baglanti kontroludur. Lv33+ beceri boslugu ve StaticLint
InitGameState beyaz liste uyarilari onceki gibi acik.

Sahip-istemci/HUD ve kaydet/yeniden giris PIE'de OYNANMADI; PLAYTEST_CHECKLIST.md'ye
adimlar eklendi. Agency-agents Unreal Multiplayer Architect ve Test Results Analyzer
rol belgeleri kullanildi; bagimsiz paralel ajan calistirilmadi. O6 kalan gecis
penceresi, hesap/imza, magaza yazim sirasi/iptal ve dedicated hazirligi acik kalir.
## 41. Gorev kabul/teslim telemetrisi ve raporu (2026-10-07)

Lv1-30 plani §3.1'in eksik `quest_accept` / `quest_claim` olaylari eklendi.
Olaylar yalniz oyuncu karakterinin basarili sunucu islemlerinde basilir;
otorite/oluluk/mesafe/on kosul/kapasite/tekrar teslim kontrolleri gecilmeden
log yoktur. Oynanis kurallari, kayit semasi ve replike alanlar degismedi.

`PlaytestMetrics.py` gorev kabul/teslim sayisi, kabulden teslimata ortalama sure,
eslesmeyen teslim ve seviye tavanina ulasan teslim sayisini gosterir. Nominal
`xp_reward`, teslim oncesi seviyeye ayri `XP(quest*)` sutunuyla yazilir.
Seviye atlama loglari odulden once geldigi icin mevcut seviye geriye sarilmaz.
Altin, zaten `ev=gold` ile olculur; gorev tesliminden tekrar sayilmaz.
Bu sutun net XP bakiyesi degildir; tavandaki kayip dusulmez. Kabul ayni log/surec/
oyuncu adi altinda yoksa sure `?` olur; oturumlar/hesaplar birlestirilmez.
Eski logda gorev olayinin yoklugu sifir gorev kazanci olarak yorumlanmaz.

Normal UE 5.7 editor derlemesi GECTI. TestQuestRewardLifeState.ps1 **PASS checks=8**
ve **MMO_QUEST_METRICS_TEST PASS accepts=2 claims=1** verdi. Testin ilk log beklentisi
200 XP odulunun Lv1->3 olmasiydi; gercek UE ciktisi ve fixture'in beklenen
karakter karsilastirmasi Lv1->2 oldugunu gosterdi, yalniz test beklentisi duzeltildi.
Gecici PlayerController ile ayni senkron karede possession yapilir; ilk kayit
yukleme timer'i calismadan karakterler yok edilir (oyuncu diski okunmaz/yazilmaz).
Controller/test komutu Shipping'de derlenmez. Oluluk, eksik ilerleme, menzil ve
tekrar teslim retlerinde basarili olay satirinin yoklugu da sinanir.

Parser fixture'i eski 12 olay/2 oyuncuyu korur; yeni 14 olay/3 oyuncuda gorev XP'si,
coklu levelup, oyuncu ayrimi, bekleyen gorev, kabul kaydi olmayan teslim, ters saat
ve tavan isareti sinanir. Gecici kopyada gorev XP toplamasini bozmak testi exit=1
ile dusurdu. Gercek UE fixture logu parser'dan da gecirildi: Lv1 nominal 200 XP,
Lv2 gercek +120 altin, 2 kabul/1 teslim. Bu sifir sureli senkron fixture,
oynanis suresi olcumu degildir.

PIE uzak istemci olay tekilligi/HUD, kayittan devam ve gercek solo/parti sureleri
OYNANMADI; PLAYTEST_CHECKLIST.md ve PLAYTEST_RUNBOOK.md'ye eklendi. `first_*` ve
modelle gercek kosu karsilastirmasi acik kalir. Agency-agents Unreal Multiplayer
Architect / Test Results Analyzer rehberleri kullanildi; ayri paralel ajan yok.

Son `RunAllChecks.ps1 -SkipBuild -FullCompile`: **23/23 GECTI**; 14 PCH'siz /W4
dosyada 0 derleyici uyarisi; JSON 111 negatif; 244 yerel yuva `loaded=244
invalid=0 loadFailed=0 existsMismatch=0 json ok=244 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`. Gecis/parti/callback/klan/gorev/loot/
iksir/ekipman/satici testleri gecti. Summer 48 / Winter 30 mob dogumu, 0 hata;
NetSmoke 1 join / 1 welcome, iki tarafta 0 hata (yalniz baglanti kaniti).
Lv33+ beceri bosluklari ve StaticLint InitGameState beyaz liste uyarisi acik.

## 42. Kayit stat/XP/puan butcesi UE regresyonu (2026-10-07)

Mevcut `UMMOStatsComponent::ApplySaveData`, puan harcama ve sinif iadesi kurallari
icin Development-only `MMOTestStatsSaveBudget` / `TestStatsSaveBudget.ps1` eklendi.
Yeni bug duzeltmesi degildir: runtime kurallari ve kayit/replikasyon semasi aynidir.
Controllersiz izole karakterle **PASS checks=15**:

- Dort sinifta gecerli Lv10 dagilimi, XP ve harcanmamis puan korunmasi (4 kontrol).
- Dort sinifta MAX_int32 stat/XP/puan girisinin sinif tabani + seviye butcesine
  kirpilmasi; fazladan level verilmemesi (4 kontrol).
- MIN_int32 degerlerin duzeltilmesi ve gecerli can/mana maksimumlari.
- MAX_int32 seviyenin MaxLevel'e kirpilmasi, tavanda XP=0 ve puan tavanlari.
- Gecersiz sinifin Warrior butcesine dusmesi.
- Gecerli statlarin bellek FillSaveData/ApplySaveData roundtrip'i.
- VIT harcamasinda puan dogru duserken mevcut can/mana korunmasi.
- Bilinmeyen stat, sifir/negatif ve yetersiz puanli harcamada durum korunmasi.
- Iki kez sinif iadesinde fazladan puan/can/mana verilmemesi (Stats API'si).

Normal UE 5.7 editor derlemesi GECTI. Negatif kontrolde yalniz kayittan gelen
harcanmamis skill puani clamp'i gecici olarak kaldirildi; mutant derlendi ve fixture
**FAIL checks=1** verdi. Kaynak kodu finally blogunda birebir geri yuklendi,
DLL tekrar derlendi ve **PASS checks=15** alindi. Mutant commit edilmedi.
Bu test gercek dosya yukleme, imza/kimlik, mastery iadesinin toplam butcesi veya
uzak istemci HUD/replikasyon kaniti degildir. PIE adimlari PLAYTEST_CHECKLIST.md'de.
Test komutu Shipping'de yoktur; oyuncu diski yuklenmez/yazilmaz. Agency-agents
AppSec Engineer, Backend Architect ve Test Results Analyzer rehberleri kullanildi;
bagimsiz paralel ajan yok.

`RunAllChecks.ps1 -SkipBuild -FullCompile` **24/24 GECTI**: 14 dosya PCH'siz /W4,
0 derleyici uyarisi; JSON 111 negatif; 250 yerel yuva `loaded=250 invalid=0
loadFailed=0 existsMismatch=0 json ok=250 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`. Yeni 15 stat kontrolu dahil gecis/parti/
callback/klan/gorev/loot/iksir/ekipman/satici testleri gecti. Summer 54 / Winter 29
mob dogumu, 0 hata; NetSmoke 1 join / 1 welcome, iki tarafta 0 hata (yalniz
baglanti). Lv33+ beceri bosluklari ve InitGameState beyaz liste uyarisi acik.

## 43. Mastery/bakiye ortak butcesi ve sinif iade acigi (2026-10-07)

UE reproducer `MMOTestMasteryBudget` / `TestMasteryBudget.ps1` eklendi. Duzeltme
oncesi normal Editor derlemesi gecti, gercek Skill → Stats yukleme sirasinda
fixture **FAIL checks=1** verdi: Lv10 kaydi bes agacta 10'ar rutbe ve 18 bakiye
yukluyordu (50+18=68, kazanilabilir puan 18). Sinif iadesi harcanan+bakiye
toplamini alt sinir olarak kullandigindan fazlayi koruyordu. Dogrudan bozuk
MAX_int32 bakiye + iade toplami ayrica signed overflow riski tasiyordu.

Duzeltme:

- Ortak `SanitizeMasteryRanks`: tam bes agac, negatifleri sifirla, rutbe<=seviye,
  toplam<=kazanilan puan. Basic, sonra Special0..3 sirasinda deterministik kirpma;
  gecerli dagilim degismez. Skill butcesi int64 hesaplanip int32 tavanina kirpilir.
- Stats kayit yuklemesi ayni kayit/rutbe onarim kuralini kullanip harcanmamis
  skill puanini kalan butceye kirpar; bilesen yukleme sirasina bagli degildir.
- Sinif degisiminde puanin tek kaynagi HandleLevelUp oldugu icin stat ve skill
  iadesi tam seviye butcesidir. Bozuk bakiye/harcama toplami okunmaz; eski besinci
  C++ parametresi uyumluluk icin durur, butceyi artiramaz. Can/mana doldurulmaz.
- Otorite kapilari ve mevcut replike alanlar korunur; kayit semasi degismedi.

Duzeltme sonrasi normal Editor derlemesi ve **PASS checks=12**: gecerli dagilim,
50+18 payload, gecerli rutbe+MAX bakiye, Lv1, MIN/MAX seviye (Lv80 toplam158),
negatif/kisa dizi, gecerli legacy beceri/hotbar, gercek ChangeClass, bozuk
MAX iade, iki normal yatirim, Skill+Stats bellek roundtrip'i. Izole controllersiz
aktor ayni cagrida yok edilir; oyuncu diski yuklenmez/yazilmaz. Test komutu ve
fixture Shipping'de yok. RunAllChecks'e yeni adim ve Skill cpp PCH'siz derlemesi
eklendi. Agency-agents Multiplayer Architect / AppSec / Backend / Test Results
Analyzer rehberleri kullanildi; bagimsiz paralel ajan yok.

Sinir: bozuk kaydin sunucuda yuklenmesi on kosuldur; uzaktan dosya yazma kaniti
degildir. Legacy uyumlulugu icin gecerli LearnedSkills korunur; bu listenin
mastery maliyetine gore tam dogrulanmasi, hesap kimligi ve imza aciktir. Y-2
tumden kapanmadi. Gercek yeniden giris, uzak istemci yatirim/sinif RPC'si ve
HUD PIE'de OYNANMADI; PLAYTEST_CHECKLIST.md'ye eklendi.

Tam `RunAllChecks.ps1 -SkipBuild -FullCompile`: **25/25 GECTI**; 15 dosya
PCH'siz /W4, 0 derleyici uyarisi; JSON 111 negatif; 256 yerel yuva `loaded=256
invalid=0 loadFailed=0 existsMismatch=0 json ok=256 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`. Summer 49 / Winter 32 mob dogumu,
iki smoke'ta 0 hata; NetSmoke 1 join / 1 welcome, iki tarafta 0 hata (yalniz
baglanti). Lv33+ beceri bosluklari ve StaticLint InitGameState beyaz liste
uyarisi acik. Kalan PIE ve guvenlik maddeleri bu paketle kapanmadi.

## 44. Kontrolcu oturumunda ilk gorev eylemleri (2026-10-07)

Lv1-30 olcum planinin sinirli ilk eylem parcasi: basarili gorev kabul/teslimi
`AMMOPlayerController::RecordFirstQuestMetric` uzerinden sunucuda
`first_quest_accept` / `first_quest_claim` yazar. Olay whitelisti, pawn/otorite
kapisi, quest None reddi ve controller TSet tekilligi vardir; client RPC yok.
Baslangic server controller BeginPlay, sid gecici rastgele GUID'dir. Pawn
degismesi sifirlamaz; SeamlessTravelTo yeni MMO controller'a kimlik/baslangic/
isaretleri kopyalar. Yeni controller yeni oturumdur. Kalici kayda/replikasyon
semasina ek yok; gorev/XP/odul kurallari degismedi. Alanlar ve olay govdesi
Shipping'de derlenmez. Ilk teslim seviyesi odul SONRASI andir; eski quest_claim
nominal XP tablosunun teslim ONCESI seviyesi aynen kalir.

Derlemedeki ilk C4458, local Character'in AController::Character uyelerini
gizlemesiydi; MetricPawn diye adlandirilip normal UE derlemesi gecti. Sistemde
`python` alias'i kurulu runtime'a gitmedigi icin mevcut RunAllChecks gibi
motorun bundled Python'u kullanildi. Basarili dogrulamalar:

- `TestFirstQuestMetrics.ps1`: **PASS checks=6**, iki bagimsiz sid'de 2 ilk
  kabul / 1 ilk teslim. Bos pawn/yanlis olay/bos quest reddi, tekrar engeli,
  pawn degisimi, dogrudan controller SeamlessTravelTo aktarimi, yeni oturum.
- Gorev fixture'i 8 kural + 2 kabul/1 teslim + ayni sid'de 1 ilk kabul/1 ilk
  teslim. Reddedilen oluluk/eksik/uzak/tekrarlar basarili olay uretmez. Gecici
  MMO controller/karakterler ayni senkron karede yok edilir; auto-load timer
  calismaz, oyuncu diski okunmaz/yazilmaz.
- Parser eski 12 olay, gorev 14 olay, yeni ilk eylem 16 olay fixture'i gecti.
  Iki Host sid'si/Client ayrimi, tekrar, eksik teslim, negatif/NaN/Inf sure,
  bozuk sid, yanlis kapsam, uyumsuz timestamp, eksik quest, NaN seviye/zaman
  reddi (9 gecersiz), --who ve XP tek sayim. Son sayi korumalari sonrasi parser
  adimi ayrica tekrar calistirilip gecti; derleme/UE kodu bu sirada degismedi.
  Gecici parser kopyasinda elapsed=0.0 bozulmasi fixture'i exit=1 ile dusurdu;
  kaynak/DLL degismedi. Gercek UE gorev logu parser'da da ilk olaylari ve
  nominal 200 XP/+120 altini tek saydi; sifir sureli fixture gercek kosu degil.

Parser oyuncu adi altinda sid'leri ayirir; ad degisimi/kalici hesap/farkli
log/surecler birlestirilmez. Iki ilk gorev olayi tum `first_*` planini bitirmez:
ilk oldurme/loot/ekipman, gercek 20 dk solo/parti ve model karsilastirmasi acik.
Gercek ag RPC'si, pawn degisimi ve Summer/Winter seamless oynanisi PIE'de
OYNANMADI; PLAYTEST_CHECKLIST/PLAYTEST_RUNBOOK guncellendi. Agency-agents Unreal
Multiplayer Architect / Test Results Analyzer rehberleri kullanildi; ayri
paralel ajan yok.

Tam `RunAllChecks.ps1 -SkipBuild -FullCompile` **26/26 GECTI**; 15 dosya
PCH'siz /W4, 0 derleyici uyarisi. JSON 111 negatif; 262 yerel yuva `loaded=262
invalid=0 loadFailed=0 existsMismatch=0 json ok=262 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`. Onceki gecis/parti/callback/klan/gorev/
loot/iksir/ekipman/stat/mastery/satici testleri ve yeni ilk eylem adimi gecti.
Summer 50 / Winter 27 mob dogumu, iki smoke'ta 0 hata; NetSmoke 1 join / 1
welcome, iki tarafta 0 hata (yalniz baglanti). Lv33+ beceri bosluklari ve
StaticLint InitGameState beyaz liste uyarisi acik. Gercek ilk eylem sureleri
ve PIE ag/oynanis kaniti bu test paketinden cikarilmaz.

## 45. Mevcut gorev odulu kapasite kurallarinin regresyonu (2026-10-07)

ClaimReward zaten ekipman cantasi ve sarf yigin/listesi kapasitesini odulden
ONCE dogruluyordu; bu calisma yeni exploit duzeltmesi veya kural degisikligi
degildir. Non-Shipping `MMOTestQuestRewardCapacity` ve
`TestQuestRewardCapacity.ps1` eklendi, RunAllChecks'e alindi. Fixture sunucu
oyun dunyasinda controllersiz iki karakter ve yakin gorevli kurar; hazir
Q_Goblins/Q_Boss gorevini bellekten tohumlar. Gercek av/kabul zinciri degildir.
Ayni senkron cagrida aktorler yok edilir; oyuncu diski okunmaz/yazilmaz.

10 kontrol: dolu 999 yigin reddi; 3 iksir icin yalniz 2 yer reddi; tam 3 yer
acinca tam XP/altin/iksir; tekrar teslim; 64 tur listesinde yeni tur reddi;
bir tur bosaltinca odul; 64 tur doluyken mevcut yigina tam sigma; 12 ekipman
cantasi reddi; bir yer acinca tam XP/altin/GreatSword +0/Common; tekrar teslim.
Ret/tekrarda Stats + Inventory + Quest FillSaveData alanlari yansima ile
FindDifferences uzerinden esit, can/mana/oluluk ayrica esit. Bu karsilastirma
esya InstanceId/NextInstanceId ve aktif/tamam gorev durumunu da kapsar.
Basarili XP seviye/bakiye/puan etkisi ayri karakterin GainExperience yoluyla
karsilastirilir. Test sentetik sarf turleri kullanir; gercek 64 tur icerigi
veya HUD kaniti degildir. Otorite/replikasyon/oyuncu kayit semasi degismedi.

Ilk derlemede testteki FMMOItemDatabase adinin F oneki eksikti; duzeltildi,
normal Editor derlemesi ve **PASS checks=10** alindi. Negatif kontrolde yalniz
uretim sarf kapasite on kosulu gecici olarak `< 0` yapildi; mutant derlendi,
test **FAIL checks=0 reason=full stack changed reward or quest**, script exit=1
ile hatayi yakaladi (motor normal exit=0; marker denetimi hatayi gordu).
Kaynak tam eski byte'lariyla finally'de geri kondu, DLL yeniden derlendi ve
**PASS checks=10** tekrar alindi. Mutant commit edilmedi. Shipping'de fixture
ve console komutu yoktur. Agency-agents Unreal Multiplayer Architect ve
Test Results Analyzer rol rehberleri kullanildi; bagimsiz paralel ajan yok.

Gercek istemci RPC/HUD, ag gecikmesinde cift teslim, kaydet/yeniden giris ve
av zinciri PIE'de OYNANMADI; PLAYTEST_CHECKLIST.md'ye acik adimlar eklendi.

Tam `RunAllChecks.ps1 -SkipBuild -FullCompile`: **27/27 GECTI**. Normal Editor
derlemesi ayrica gecti; 15 dosya PCH'siz /W4, 0 derleyici uyarisi. JSON 111
negatif; 268 yerel yuva `loaded=268 invalid=0 loadFailed=0 existsMismatch=0
json ok=268 mismatch=0 maxLevel=6`, `writeorder ok=1`, `roundtrip equal=1`.
Yeni kapasite ve onceki gorev/ilk eylem/gecis/parti/callback/klan/loot/iksir/
ekipman/stat/mastery/satici regresyonlari gecti. Summer 50 / Winter 28 mob
dogumu, smoke hata=0; NetSmoke 1 join / 1 welcome, server/client hata=0.
NetSmoke yalniz baglanti kanitidir. Lv33+ beceri bosluklari ve StaticLint
InitGameState beyaz liste uyarisi acik; PIE ve dedicated Server isi kapanmadi.

## 46. Kontrolcu oturumunda ilk gercek ganimet (2026-10-07)

Lv1-30 ilk eylem olcumunun sinirli parcasi: `first_loot` yalniz
AMMOLootPickup gercek pozitif grant sonrasi yazilir. Sarfta AddItem donen Added,
altinda sonraki-onceki Gold, ekipmanda canta adet artisi kaynaktir. Kismi sarf
toplamasi ilk olaydir (kalan yerde durur); kind/item/amount gercek teslimdir.
Oluluk/rezervasyon/doluluk/tekrar kapilari aynen kalir; sifir gercek altin
kazanci olay uretmez. Mevcut altin tavaninda kirpma/pickup yok olma kurali
DEGISMEDI. Starter pack, gorev/satici/debug grant yoluna hook eklenmedi.

Controller first alanlari FirstActionMetrics diye adlandirildi; gorev ve loot
ayni sid/start/TSet kapsamindadir. Server-only ortak helper, whitelisted
public C++ wrappers, client RPC yok. Payload turu/adet dogrulanir (pozitif
adet, equipment=1, gold kimligi Gold). Pawn degisimi tekilligi sifirlamaz,
SeamlessTravelTo ayni durumu yeni controller'a tasir. Yeni baglanti yeni sid;
kalici karakterin ilk ganimeti degildir. Shipping wrapper'lari log/payload
uretmeden false doner; alanlar/fixture derlenmez. Oyun ve kayit semasi degismedi.

Normal UE Editor derlemesi gecti. Test kaniti:

- Yeni `TestFirstLoot.ps1`: **PASS checks=6**, gercek TryCollect yolunda uc
  bagimsiz sid'de gold=2, item=1, equipment=1. Sifir altin, full stack/full bag
  denemeleri olay yok; kismi sarf ve sonraki tam toplama yalniz ilk olayi yazar.
- Mevcut 10 loot life-state kontrolu MMO controller ile calisir: oluyken hic
  olay yok, dirilme sonrasi yalniz amount=17 gold olayi; sarf/ekipman/tekrar
  ikinci ilk olay yazmaz. Mevcut 6 controller kontrolu loot'u da kapsar:
  bos pawn/gecersiz payload, farkli turde tekrar, yeni pawn, dogrudan controller
  seamless aktarimi ve yeni sid. Toplam iki sid'de iki ilk loot.
- Parser eski12 / quest14 / first-quest16 / first-loot13 olay fixture'lari
  gecti. Loot fixture: uc gecerli ilk, bir tekrar, sekiz bozuk payload reddi;
  --who, bilinmeyen sure ve altin/XP/gorevin cift sayilmamasi. Gercek UE fixture
  logu parser ile okundu: +2 altin tek sayildi, sarf=1/ekipman=1 ayri sid'de.
- Negatif kontrolde sarf telemetry Added yerine ItemCount yazdi: mutant
  derlendi, C++ 6 oyun kontrolu gecerken script exit=1 ile yanlis amount=3'u
  yakaladi (teslim=1). finally kaynak tam byte'lariyla restore edildi, DLL
  tekrar derlendi ve hook testi PASS alindi. Mutant commit edilmedi.

Izole aktorler ayni senkron karede yok edilir/controller unpossess edilir;
oyuncu dosyasi okunmaz/yazilmaz. Fixture direkt TryCollect ve controller API'si
kullanir; fiziksel overlap, gercek ServerTravel/RPC/HUD veya gercek av suresi
kaniti degildir. PIE acik adimlar PLAYTEST_CHECKLIST'e, olcum anlami RUNBOOK
ve Lv1-30 plana eklendi. Ilk oldurme/kuşanma ve gercek sinif/parti kosusu acik.
Agency-agents Unreal Multiplayer Architect / Test Results Analyzer rol
rehberleri uygulandi; bagimsiz paralel ajan yok.

Tam `RunAllChecks.ps1 -SkipBuild -FullCompile`: **28/28 GECTI**, 15 dosya
PCH'siz /W4, 0 derleyici uyarisi. JSON 111 negatif; 274 yerel yuva `loaded=274
invalid=0 loadFailed=0 existsMismatch=0 json ok=274 mismatch=0 maxLevel=6`,
`writeorder ok=1`, `roundtrip equal=1`. Gorev/ilk eylem/gecis/parti/callback/
klan/loot/iksir/ekipman/stat/mastery/satici ve yeni ilk loot kontrolleri gecti.
Summer 47 / Winter 28 mob dogumu, smoke hata=0; NetSmoke 1 join / 1 welcome,
iki tarafta hata=0 (yalniz baglanti). Lv33+ beceri bosluklari ve StaticLint
InitGameState beyaz liste uyarisi acik. Gercek sure, ag replikasyonu/oynanis ve
dedicated Server bu paketle tamamlandi sayilmaz.

## 47. Kontrolcu oturumunda ilk oldurme kredisi (2026-10-07)

Lv1-30 olcumunun sinirli parcasi: `first_kill`, AMMOMob HandleDeath'in
mevcut canli XP/gorev kredi alicisi dongusunde, GainExperience ONCESI yazilir.
`lv` odul oncesi, `mob/mlv/xp/elite` normal kill olayi ile ayni anlamdadir.
`xp` hesaplanan paydir, tavanda gercek bakiye artisi degildir. Credit turleri:
fatal = secilen oldurenin gecerli karakter son vurusu; contribution = diger
aliciya ait taze pozitif hasar; party = kendi hasari olmadan parti payi;
fallback = son vurus kaynagi yok/gecersizken en yuksek taze katkiyla secilen
yedek olduren. Parti/fallback son vurus olarak yorumlanmaz. Sifir XP kredisi
de gecerli ilk olaydir. Mevcut XP, gorev, ganimet ve ag semasi degismedi.

Server-only ortak sid/start/TSet helper, whitelisted C++ wrapper; client
RPC yok. Wrapper bos mob/nonpositive mob seviyesi/negatif XP/gecersiz kredi,
bos pawn ve olu karakteri reddeder. Shipping wrapper log/payload uretmez,
fixture derlenmez. Pawn degisimi/seamless controller aktarimi tekilligi
korur; yeni baglanti yeni sid. Kalici karakterin ilk oldurmesi degildir.

Normal Editor derlemesi ve kaynak geri yukleme sonrasi yeniden derleme gecti.
Kaniti:

- `TestFirstKill.ps1` **PASS checks=10**, gercek TakeDamage -> Stats ApplyDamage
  -> OnDeath -> HandleDeath -> GainExperience. 5 sid'de contribution XP2,
  party XP2, fatal XP14, fallback XP20 ve dirilen oyuncu fatal XP20.
  Sonuncu Lv1->2 yapar; first satiri lv=1. Olum oncesi vurusta kredi yok;
  olu alici reddi, duplicate HandleDeath/fatal hit ve ikinci kill tekilligi;
  katkısız cevre ve stale fallback kredi vermez. Parti uyeligi/stale katki
  yalniz fixture'da seed edildi. AI auto-possession kapali, carpisma kapali;
  kendi aktorleri/yeni fixture alani pickup'lari ayni karede temizlenir.
- Mevcut 6 controller kontrolu kill icin bos pawn/gecersiz payload,
  tekrar, pawn degisimi, dogrudan SeamlessTravelTo ve yeni sid'i de sinar.
  Gorev 2 kabul/1 teslim ve loot 2 olayi korundu; kill 2 olay/2 sid.
- Parser eski12 / quest14 / first-quest16 / first-loot13 / first-kill17
  olay fixture'lari gecti. Kill fixture: 4 gecerli sid, 1 tekrar, 10 bozuk
  payload reddi (eksik alan, negatif/NaN/kesirli sayi, kredi/elit); zero XP,
  elit, --who, bilinmeyen sure ve kill/XP/gold/quest cift sayilmamasi.
  Gercek UE logu raporda 6 normal kill / 78 XP, 5 first olay; artan seviye
  kaydinda first lv1 kaldi. Sentetik sureler gercek av suresi degildir.
- Negatif kontrolde fallback etiketi fatal yapildi. Mutant derlendi;
  C++ 10 oyun kontrolu gecerken script yanlis etiketi **exit=1** ile yakaladi
  (UE exit=0). finally kaynak tam byte'lariyla restore, DLL yeniden derleme,
  direct hook testi PASS. Mutant commit edilmedi.

Oyuncu dosyasi okunmaz/yazilmaz. Gercek davet/uzak istemci/ServerTravel,
fiziksel av, gecikme ve tum parti/menzil kombinasyonlari fixture kaniti
degildir. PIE adimlari acik olarak PLAYTEST_CHECKLIST'e, olcum anlami
RUNBOOK ve Lv1-30 plana eklendi. Ilk kuşanma/diger eylemler ve 20 dk
sinif/parti kosusu acik. Agency-agents Unreal Multiplayer Architect ve
Test Results Analyzer rol rehberleri uygulandi; bagimsiz paralel ajan yok.

Tam `RunAllChecks.ps1 -SkipBuild -FullCompile`: **29/29 GECTI**; 15 dosya
PCH'siz /W4, 0 derleyici uyarisi. JSON 111 negatif; 280 yerel yuva
`loaded=280 invalid=0 loadFailed=0 existsMismatch=0 json ok=280 mismatch=0
maxLevel=6`, `writeorder ok=1`, `roundtrip equal=1`. Yeni first-kill ve tum
onceki gorev/loot/gecis/parti/klan/iksir/ekipman/stat/mastery/satici kontrolleri
gecti. Summer 49 / Winter 30 mob dogumu, smoke hata=0; NetSmoke 1 join /
1 welcome ve iki tarafta hata=0 (yalniz baglanti kaniti). Lv33+ beceri
bosluklari ve StaticLint InitGameState beyaz liste uyarisi acik.
Dedicated Server/Shipping hedefi ve gercek PIE sure/ag oynanisi kapanmadi.

## 48. Ilk basarili oyuncu kusanma istegi (2026-10-07)

Lv1-30 olcumunun sinirli parcasi: `first_equip`, mevcut EquipFromBag /
EquipByInstanceId sunucu istegi kimlik/canta/seviye/slot kontrollerini gecip
ekipman ve bonuslari guncelledikten sonra yazilir. Payload item/instance/slot;
0 silah, 1 zirh, 2 baslik, 3 bot. InstanceId oyuncunun envanterine aittir.
Baslangic paketi daha once ayni public equip fonksiyonunu kullaniyordu;
GrantStarterPack artik yalniz sunucunun native EquipStarterEquipment girisini
kullanir. Iki giris ayni eski ekipman kurallarini uygulayan internal govdeye
gider; otomatik giris ilk olayi kaydetmez ve istemci RPC'sine donusmez.
Kayit ApplySaveData dogrudan slot/bonus kurdugu icin ilk olay uretmez.
Mevcut esya/olum/bonus/replication/RPC/save semasi degismedi.

RecordFirstEquipMetric C++ helper: veritabaninda esya, pozitif instance,
gecerli ve esya tanimina uyan slot; ortak authority/player-pawn/sid/start/TSet
kapilari. Shipping payload/log uretmez, fixture derlenmez. Her controller
omrunde ilk basari bir kez; pawn ve seamless controller aktarimi korur,
yeni baglanti yeni sid. Kalici karakterin ilk kuşanmasi degildir. Debug/C++
ile ayni public istegin basarisi da sayilir; loot/verme/satin alma tek basina
olay degildir. Parser XP/gold/quest/kill toplamini ikinci kez artirmaz.

Kaniti:

- `TestFirstEquip.ps1` **PASS checks=8**: native otomatik kuşanma, bellekte
  ApplySaveData, olmayan/eskimis ID, Lv1'de GreatSword ret, slot array yokken
  ret, başarılı EquipFromBag swap + public AttackPower bonusu, stale tekrar
  ve ikinci slot, bagimsiz oyuncu. 2 sid'de yalniz ShortSword instance4 slot0
  ve LeatherArmor instance1 slot1; otomatik/restore/ret ilk olayi tuketmedi.
  Bu fixture tum GrantStarterPack yeni-kayit dalini calistirmiyor; callsite
  derleme ile kapsandi. Aynı karede actorler temizlenir, load timer/disk yok.
- Mevcut **6** controller kontrolu equip icin bos pawn/gecersiz payload,
  tekrar, pawn degisimi, dogrudan SeamlessTravelTo ve yeni sid'i kapsar.
  Gorev 2 kabul/1 teslim, loot 2 ve kill 2 korundu; equip 2/sid2.
- Parser eski12 / quest14 / first-quest16 / loot13 / kill17 / equip17 olay
  fixture'lari gecti. Equip: dort slot/dort sid, 1 tekrar, 11 bozuk item /
  instance / slot payload reddi (eksik/None, sifir/negatif/NaN/kesir/int32
  disi kimlik; negatif/4/kesir/Inf slot); --who ve eksik sure `?` kontrolu.
  Python slot/esya veritabanini kullanmaz; sunucu wrapper esya-slot uyumunu sinar.
  Gercek UE first-equip logu da parser'da 2 olay/2 sid, 0 invalid/duplicate;
  ShortSword4/slot0 ve LeatherArmor1/slot1 payload'lari eslesti.
- Negatif kontrolde otomatik giris false yerine true yapildi. Mutant
  derlendi, C++ 8 oyun kontrolu PASS kaldi fakat ilk olay instance1 oldugu
  icin script **exit=1** ile yakaladi (UE exit=0). finally tam kaynak byte'lari
  geri yuklendi; DLL yeniden derlendi ve direct test PASS. Mutant commit yok.
- Ilk derlemede fixture protected EquipmentAttackBonus'a erisiyordu (C2248).
  Test public turetilmis AttackPower degerini kontrol edecek sekilde duzeltildi;
  uretim alaninin erisim seviyesi degismedi. Normal Editor derlemesi gecti.

Gercek istemci kuşanma RPC'si, gecikmeli cift tik, HUD/bonus replication,
baslangic dalinin tamamı, dosyadan yeniden giris, ServerTravel ve 20 dk
sinif/parti sureleri PIE'de acik; checklist/runbook/plan/tablo guncellendi.
Agency-agents Unreal Multiplayer Architect ve Test Results Analyzer rol
rehberleri kullanildi; bagimsiz paralel ajan calismadi.

Tam `RunAllChecks.ps1 -SkipBuild -FullCompile`: **30/30 GECTI**; 15 dosya
PCH'siz /W4, 0 derleyici uyarisi. JSON 111 negatif; 286 yerel yuva
`loaded=286 invalid=0 loadFailed=0 existsMismatch=0 json ok=286 mismatch=0
maxLevel=6`, `writeorder ok=1`, `roundtrip equal=1`. Onceki tum kontroller
ve yeni kuşanma testi gecti. Summer52 / Winter31 mob dogumu, smoke hata=0;
NetSmoke 1 join / 1 welcome, iki tarafta hata=0 (yalniz baglanti).
Lv33+ beceri bosluklari ve StaticLint InitGameState beyaz liste uyarisi acik.
Dedicated Server/Shipping ve gercek PIE sure/ag oynanisi tamamlanmadi.

## 49. Kayitli ucretli beceriyi mastery butcesine katma (2026-10-07)

UE'de dogrulanan kayit yukleme tutarsizligi: Skill ApplySaveData gecerli
sinif/seviye LearnedSkills'i rutbe sifir olsa da koruyordu. Stats ApplySaveData
yalniz kaynak MasteryRanks maliyetini kullandigi icin beceri + tam harcanmamis
puan birlikte kalabiliyordu. Reproducer Lv10 War_ShatterBlow/Special0=0/bakiye18;
normalde gereken rutbe4 ve kazanilabilir toplam18. Test **FAIL checks=0** verdi.
Bu bozuk/elle degistirilmis sunucu kaydi kosuludur, uzak istemcinin dosya yazma
veya beceri unlock RPC'sini atlama kaniti degildir.

Ortak `SanitizeSavedMasteryRanks`: mevcut rutbeyi eski kuralla onarir, gecerli
sinif/seviyedeki ogrenilmis ucretli becerilerden gereken minimum agac rutbesini
max ile birlestirir, toplam seviye butcesini tekrar uygular. Ayni agactaki
becerilerin maliyeti eski SkillPointCost toplami degil maksimum mastery rutbesi.
GrantStarterSkill ile ayni sinifin en dusuk seviyeli Basic becerileri ucretsiz.
Bu hem boş hem mevcut rutbeli legacy/corrupt kayitlarda kullanilir; gecersiz
sinif/id/seviye maliyet uretmez. Basic→Special0..3 mevcut deterministik onarim
onceligi korunur. Karsilanmayan beceri ve hotbar temizlenir; gecerli eski
beceri butce icinde korunur, eksikler SyncLearnedFromMastery ile acilir.
Bu onarim listeden yeni agac rutbesi turetebilir ve bakiyeyi kirpabilir;
kayip/bozuk kayit icin eski dagilim uydurulmaz. Kalan bakiye artirilmaz.

Stats ayni helper'i kaynak kayittan cagirir; runtime Skill alanina bagimli
degildir. Stats once veya sonra yuklense ayni kalan puan. JSON/save semasi,
owner-only replication, authority ve RPC kurallari degismedi; fixture Shipping
disinda. Yeni helper ise normal sunucu yukleme dogrulamasinin parcasidir.

`TestLearnedSkillBudget.ps1` **12 kontrol PASS**: paid cache0+18 -> rank4+14;
duplicate maliyeti bir kez; ayni agac max maliyet; gecerli rutbe/bakiye korunur;
asiri butce/ust seviye beceri ve slot reddi; legacy bos rutbe migration;
bellekte FillSaveData/ApplySaveData idempotent; stats-first yol; dort sinif
ucretsiz Basic; legacy Priest Smite slot1 tamamlama; invalid/foreign/ust
seviye beceriden maliyet yok; class change yalniz earned18 iade. Mevcut
mastery/iade 12 kontrol de gecti. Oyuncu diski bu fixture'da okunmaz/yazilmaz.
Normal Editor derlemesi gecti; gercek kayit göcu/RPC/HUD PIE checklist'te acik.

Agency-agents Unreal Systems Engineer ve Test Results Analyzer rehberleri
uygulandi. Kepler paralel ajan salt okunur backend kontrolunde mevcut yerel
senkron adaptor icin yeni HIGH/MEDIUM bulmadi; ardindan bu onarimin ortak
helper/yukleme sirasi entegrasyonunu inceledi. Kod yazmadi veya UE calistirmadi.

Tam `RunAllChecks.ps1 -SkipBuild -FullCompile`: **31/31 GECTI**, 15 dosya
PCH'siz /W4 ve 0 derleyici uyarisi. JSON 111 negatif; store292/loaded292,
invalid0/loadFailed0/existsMismatch0/jsonMismatch0/maxLevel6, writeorder1 ve
roundtrip equal1. Summer51 / Winter31 smoke hata0; NetSmoke join1/welcome1,
server/client hata0 (yalniz baglanti). Bu paketten sonra yalnız fixture'in
stats-first adimi guclendirildi: once runtime rutbe0, Stats tek basina bakiye14,
ardindan Skill rutbe4. Normal yeniden derleme, skill PCH'siz tek-dosya derleme
ve 12 kontrol direct PASS; uretim helper'i degismedi. Tam paket tekrar kosulmadi.
Lv33+ beceri bosluklari/InitGameState lint uyarisi acik. Gercek dosya göcu,
istemci RPC/HUD, Server/Shipping hedefi ve 20 dk oyun sureleri acik.

## 50. LLM oyuncu kota tablosu dolunca siniri atlama yolunu kapatma (2026-10-07)

Mevcut oyuncu basi gunluk kota zaten vardi (QA ORTA-1'in eski metni tarihsel).
Yeni bulunan sinir hatasi: 4096 anahtar dolunca yeni oyuncunun istegi genel
butceye yaziliyor, fakat oyuncu sayaci tutulmuyordu. Bu anahtar sonraki
isteklerde oyuncu kotasini asabiliyordu; genel gunluk/oturum/dakika tavanlari
atlatilmiyordu. UE reproducer, eski kuralli ortak helper ile **FAIL checks=11**
`full player table bypassed per-player quota` verdi (UE exit0, PS exit1).
Varsayilan 1500 gunluk tavanla yalniz sayilan istekler tabloyu dolduramaz;
iadeli isteklerin sifir sayacli anahtarlari tutuldugu icin tablo bu yoldan
veya daha yuksek limitsiz/ayarlanmis butcelerle dolabilir. Bu, yeni dogrulanmis
sinir davranisidir; gercek uzaktan/HTTP suistimal kosusu yapilmadi.

Kota acikken yeni anahtar izlenemiyorsa **rezervasyondan once ret**:
HTTP kurulmaz, toplam/oyuncu/oturum/es-zamanli sayac ve yeni dakika yuvasi
harcanmaz, mevcut callback NPC yedegine doner. Var olan anahtarlar kendi
kotalari dahilinde devam eder. Acikca `MaxDailyRequests=0` sinirsiz modunun
onceki davranisi ve 4096 bellek tavani korunur. UTC gun degisimi tabloyu
sifirlar. Bu kapasite reddi yeni oyuncunun LLM ozelligini erteler; oynanisi
engellemez. Oyuncu adi hala hesap kimligi degildir; yeniden adlandirma/kimlik
suistimali, coklu surec sayac ayrismasi ve kalicilik penceresi acik kalir.

Bütce rezervasyonu, gun sifirlama ve iade bellek helper'larina ayrildi;
uretim cagiranlari dosya yazimi ve HTTP'yi yurutur. RefundRequest gunluk
sayac degistiyse true doner, mevcut callback/baslatma hatasi yollari bu
sonucta NoteBudgetChanged cagirir. Eski gun iadesi yalniz oturumdan duser;
es-zamanli yuva birakilir. Timeout/HTTP200 ucret kurali ve callback'in
tek-sefer bSlotReleased kapisi degismedi; sayaclar istemciye replike edilmez,
mevcut server RPC mesafe/cooldown ve owner ClientReceiveNpcDialogue yolu kullanilir.

`MMOTestDialogueBudget` / `TestDialogueBudget.ps1`: **25 UE kontrol PASS**.
Kayitli GI subsystem'i yerine ayni GI outer'i altinda kayitsiz test UObject'i;
Initialize/Deinitialize, RollDailyBudget dosya yolu, NoteBudgetChanged, API
anahtari, HTTP ve saglayiciya cagri yok. Gercek dunya sadece Exec icin gereklidir.
Kontroller: anahtar normalizasyonu/alias, kota formulu, rezervasyon sayaclari,
oyuncu/oturum/gun/dakika/es-zamanli retleri, tam60sn pencere, iade/dakika
koruma/eski gun/alt tasma, dolu tablo yeni ret/mevcut kabul/kota/sinirsiz,
gun sifirlama/ayni gun, baslatilamayan rezervasyon temizligi, ucreti koruma,
son oturum/gun/oyuncu ve 4095->4096 son takip yuvasi sinirlari.
Test HTTP durum siniflandirmasini, gercek callback sirasi/cift callback'i,
gercek UTC saati/gece yarisi, dosya yeniden girisi/atomik yazmayi veya uzak
istemci HUD'ini calistirmaz. Ilk testte GameInstance ClassWithin nedeniyle
fixture outer assert'i oldu; GameInstance outer ile duzeltildi. Bu fixture
kurulum hatasiydi, uretim subsystem'indeki bir hata degildi.

Normal Editor derlemesi gecti. RunAllChecks'e yeni adim ve dialogue dosyasi
PCH'siz /W4 listesine eklendi. Tam paket sonucu asagidadir.
Agency-agents Unreal Systems Engineer rehberi ve Sartre paralel QA ajani
kullanildi. Sartre salt okunur sinir matrisi/entegrasyon review'u yapti;
dosya yazmadi, UE/derleme veya saglayici istegi calistirmadi.

Tam `RunAllChecks.ps1 -SkipBuild -FullCompile`: **32/32 GECTI**; 16 dosya
PCH'siz /W4, derleyici uyari0. JSON111 negatif; store298/loaded298,
invalid0/loadFailed0/existsMismatch0/jsonMismatch0/maxLevel6, writeorder1,
roundtrip equal1. Onceki stats-first fixture dahil beceri12, mastery12,
equip8, vendor14 ve tum gecis/olum/gorev/loot kontrolleri gecti. Yeni LLM25
bu tam pakette de PASS. Summer51 / Winter34 smoke hata0; NetSmoke1join /
1welcome, server/client hata0 (yalniz baglanti). Lv33+ beceri bosluklari,
InitGameState lint uyarisi, gercek HTTP/PIE ve Server/Shipping hedefi acik.
Bu testte duzeltme sonrasi uretim kodu veya fixture degistirilmedi.

## 51. İlk kabul edilen beceri kullanımını sunucu oturumunda ölçme (2026-10-07)

Lv1–30 §3.1 kapsamında sınırlı telemetri artışı. `UseSkill` otorite dalında
`ConsumeMana` başarılı olduktan ve combat işaretinden sonra, `ExecuteSkill`
öncesinde controller'a `RecordFirstSkillMetric` çağrısı eklendi. DB kimliği,
tür 0..6 ve mana maliyeti yazılır; seviye etki/XP öncesidir. Shipping wrapper
false döner; yeni RPC, replike alan, kayıt şeması veya oyun kuralı yok.
Mana/cooldown/sınıf/seviye/öğrenme/ölüm/hedef kuralları korunur. Tam canla şifa,
boş AoESelf ve HealTarget kendine dönüşü kabul olabilir; bu ilk hasar veya
faydalı şifa değildir. Debug/native başarılı kullanım da sayılır.

Mevcut controller GUID/start/tekillik kümesi kullanılır. Wrapper DB, öğrenme,
sınıf/seviye ve canlılık doğrular; ortak katman otorite/player-controlled pawn
ister. Pawn değişimi/controller aktarımı tekilliği korur, yeni controller yeni
sid'dir. `RunFirstQuestMetricsSelfTest`in 6 kontrolü skill için de genişletildi;
PS beklenen iki session/iki skill metadata'sını kontrol eder.

`MMOTestFirstSkill` / `TestFirstSkill.ps1`: **17 kontrol PASS**, yedi ayrı
controller/yedi first_skill/tüm 0..6 türleri. Bilinmeyen, öğrenilmemiş, yabancı
sınıf, düşük seviye, ölü, manasız, cooldown ve hedef yokluğu retleri; aynı-frame
native ServerUseSkill cooldown reddi; cooldown temizleyip ikinci başarılı
kullanımda tekillik; tam can şifası, boş self AoE, buff ve controllerless şifa.
Test ilk olayın sekiz ret kapısından sonra yazıldığını da kontrol eder.
Sınıf/seviye/öğrenme testte kurulmuştur; gerçek yatırım/öğrenme veya ağ RPC'si
kanıtı değildir. Moblar üç hasar türünde gerçekten hasar alır ama öldürülmez;
öldürücü vuruşta seviye atlama PIE kontrolü açık. Oyuncu dosyası yazılmaz.

Negatif test: üretim wrapper'ında mana metadata'sı geçici +1 yapıldı. UE bellek
kontrolleri PASS olsa da log doğrulayıcı **PS exit1** ile yedi olayın yanlış
metadata'sını yakaladı. Dosya exact bytes geri alındı, DLL yeniden derlendi,
doğru metadata ile test tekrar PASS. İlk fixture kurulumunda yapay MaxMana=500
buff stat yeniden hesaplamasıyla değiştiği için checks14'te KALDI; fixture gerçek
primary/level türetilmiş maksimumlarıyla kurularak düzeltildi. Oyun bug'ı olarak
raporlanmaz. İlk normal derlemedeki Pawn üye gölgelemesi de MetricPawn ile giderildi.

Parser FIRST_ACTIONS'a first_skill eklendi; effect tam sayı 0..6, mana sonlu ve
>=0, skill boş/None değil; ortak scope/GUID/saat/seviye validasyonu korunur.
Oyuncu/sid/action başına ilk değer korunur; XP/kill/gold/quest tekrar toplanmaz,
eski logda süre ?. Cicero paralel ajanı yalnız parser/test/yeni fixture'ı yazdı,
Agency QA rehberini kullandı; 26 olaylı fixture ve **140/140 assert** yerel UE
Python'unda geçti. Parent Unreal Systems rehberiyle C++/PS/entegrasyonu yürüttü.
Gerçek host/uzak HUD, RPC, hub, öldürme-seviye atlama, seamless travel/reconnect
ve dört sınıf 20 dk PIE ölçümü PLAYTEST_CHECKLIST'te açık.

Son QA review'u üretim hook hatası bulmadı; iki ölçüm noktası kapatıldı:
1. Native PS logları artık `ValidateFirstSkillLog.py` ile aynı gerçek parser'dan
   geçer; who/t/start/elapsed/payload uyumu ve yedi (oturum testinde iki) kabul
   zorunludur. Geçici gerçek log kopyasında who silme ve elapsed=9999 negatifleri
   exit1 verdi; PS'nin salt substring kontrolünün kaçıracağı durumlar yakalandı.
2. Önceden geçersiz first_* marker, reddedilmeden önce seviye gözlem saatini
   uzatıyordu. Artık first_t/last_t yalnız doğrulanmış first marker veya normal
   olayda ilerler; geçersiz marker sayısı diagnostic'te kalır. İlk normal olaydan
   önce/sonra bozuk marker ve altı first türü için 10 assert eklendi (**150/150**).
   Eski erken saat güncellemesini geçici parser kopyasına koymak 8 duration/clock
   kontrolünde KALDI. Hiç production dosyası veya oyuncu kaydı bu negatifte yazılmadı.
   Normal olayların saat davranışı değişmedi; geçerli tekrar first marker mevcut
   davranışla gözlem saatini ilerletebilir, ilk beceri süresini değiştirmez.

İlk tam paket 33/33, 16 PCH'siz dosya 0 uyarı ile geçti. Bu sırada bulunan parser
ve PS ek doğrulaması sonrası normal Editor derlemesi yeniden geçti. Son kaynakla
ikinci `RunAllChecks.ps1 -SkipBuild -FullCompile`: **33/33 GECTI**, 16 PCH'siz /W4
dosya ve compiler warning0. Parser150/150, skill17/yedi gerçek payload ve oturum6/
iki parser payload; JSON111 negatif; store310/loaded310, invalid0/loadFailed0/
existsMismatch0/jsonMismatch0/maxLevel6, writeorder1, roundtrip equal1. Summer50 /
Winter31 doğum smoke hata0; NetSmoke1join/1welcome, server/client hata0 (bağlantı).
Tüm eski regresyonlar da geçti. Kaynak bu son tam paket sırasında değiştirilmedi.
Lv33+ beceri boşluğu ve InitGameState lint uyarısı, gerçek PIE/RPC ve kaynak
motorla Server/Shipping hedefleri açık. Proje iskeleti ve runbook güncellendi.

## 52. İlk kabul edilen temel saldırıyı hasar/XP öncesi ölçme (2026-10-07)

Lv1–30 §3.1 telemetrisi: `AttackTarget` otorite dalında ölüm, cooldown,
hedef/menzil, PvP, parti/klan, hub ve doğum koruması geçtikten sonra, hasar
çağrısından önce `RecordFirstBasicAttackMetric` yazılır. `target=mob|player`,
`targetid=<arketip|Player>`, `tlv=<hedef seviyesi>`; saldıranın `lv`si XP
öncesidir. Hedef oyuncunun adı eklenmez. Kabul edilen temel saldırı ölçümüdür,
pozitif hasar veya input/animasyon zamanı değildir. Null/ölü hedefin mevcut en
yakın canlı moba dönüşü kabul edilirse gerçek mob yazılır. Shipping hook kapalı;
yeni RPC, replike alan, kayıt şeması veya oyun kuralı değişmedi.

Ortak controller GUID/start/tekillik kümesi kullanıldı. Pawn değişimi ve
controller aktarımı ilk olayı tekrar üretmez, yeni controller yeni sid'dir.
Wrapper payload ve canlı pawn doğrular, ortak katman authority/player-controlled
pawn ister. Altı oturum kontrolü genişletildi: iki gerçek wrapper kaydı ve
pawn/aktarım/yeni sid; bu wrapper testi saldırı kuralı veya gerçek travel değildir.

`MMOTestFirstBasicAttack` / `TestFirstBasicAttack.ps1`: **21 kontrol PASS**,
sekiz controller/sekiz gerçek hook olayı; dört sınıfın ortak normal yakın vuruşu,
PvP, en yakın mob fallback'i ve ilk öldürücü vuruşta tam 200 XP (Lv2, kalan XP100). On ret kapısı
olaydan önce sınandı: ölü, cooldown, hedefsiz, kendisi, uzak, PvP kapalı, parti,
klan, doğum koruması ve saldıranın hub'da olması. Native ServerAttackTarget
cooldown çağrısı hasarı tekrarlamadı; cooldown temizlendikten sonra ikinci gerçek
vuruş hasar verdi ama ilk olay tekrar etmedi. Controllerless vuruş davranışı
korundu ve ilk olay sayılmadı. PS metadata/sid/ret sonrası sıra, öldürücü olayın
first_kill öncesi sırasını ve kill xp=200 payload'ını kontrol eder; sekiz gerçek log ve iki oturum logu ortak
parser ile who/saat/payload yönünden kabul edilmek zorundadır.

İlk fixture denemeleri sınıf kurulumu ve Possess'in türetilmiş saldırı gücünü
yeniden hesaplaması nedeniyle checks10 ve checks16'da KALDI. Fixture sabit
AttackPower=10'u bu kurulumlardan sonra ayarlar; üretim savaş kodu düzeltilmedi,
denge veya PvP bug'ı olarak raporlanmaz. Tüm aktörler uzak alanda, çarpışma/AI
kapalı, aynı frame içinde kurulur/yıkılır; controller önce unpossess, gerçek
oyuncu dosyası ve gecikmiş load/save timer'ı kullanılmaz. PvP kuralı eski değerine
geri alınır. Gerçek RPC/PIE veya gerçek sınıf öğrenme akışı kanıtı değildir.

Negatif metadata deneyi: yalnız hook hedef seviyesi geçici +1 yapıldı, Editor
DLL derlendi. Native bellek testi 20 PASS ve yedi olay olsa da PS **exit1** ile
hatalı hedef seviyesini yakaladı. Exact bytes finally geri alındı, doğru DLL
tekrar derlendi, aynı doğrudan test yedi parser kabulü/20 kontrolle PASS oldu.

Agency QA rehberiyle Ptolemy paralel ajanı yalnız parser/test/yeni fixture/yeni
log validator dosyalarını yazdı; parent Unreal Systems rehberiyle C++/PS/docs
entegrasyonunu yürüttü. `FIRST_ACTIONS` yeni basic_attack türünü içerir;
hedef türü, anonim Player kimliği ve sonlu pozitif tam sayı hedef seviyesi
kontrolü vardır. Tekillik, eski logda bilinmeyen süre ve geçersiz ilk olayın
seviye süresine etkisizliği korunur. 39 sentetik olay: 34 temel saldırı marker'ı,
6 kabul/2 tekrar/26 geçersiz; dört sınıf, payload/clock/kimlik/CLI ve native
validator negatifleri. Yerel UE Python **312/312 assert PASS**. Normal kill/XP/
altın/görev toplamları artırılmaz. Gerçek 20 dk koşu, host/uzak input/RPC, hedefin
hub'da olduğu gerçek PvP, görsel hasar, respawn/travel/reconnect açık kutularla
PLAYTEST_CHECKLIST'e eklendi; runbook ve dikey dilim planı güncellendi.

QA review'u hook/session hatası bulmadı; üç kanıt boşluğu giderildi. Öldürücü
fixture artık yalnız Level>1 değil, Level2/Experience100 ve first_kill xp=200
ister. Archer'ın ilk başarılı saldırısı native ServerAttackTarget implementation
üzerinden gelir (gerçek ağ taşınması değildir). Sekizinci controller'da saldırı
gücü0 ile saldırı kabul edilir, hedef HP değişmez ve ilk olay yine yazılmalıdır.
İlk tamamlanan paket 34/34, 16 PCH'siz dosya0 uyarı, parser312/312 idi; bu ek
kontrollerden sonra normal Editor DLL yeniden derlenip doğrudan 21 kontrol/
sekiz parser kabulüyle PASS oldu. İlk kayıt öncesi controller aktarımı ayrıca
sınanmadı; mevcut fixture aktarımı kayıt sonrasında sınar.

Sıfır hasar negatif deneyi: hook geçici AttackPower>0 filtresiyle derlendi.
Native 21 bellek kontrolü PASS olmasına rağmen yalnız yedi olay yazıldı; PS
sekiz olay sözleşmesi nedeniyle exit1 verdi. Exact bytes finally geri alındı,
doğru DLL yeniden derlendi, doğrudan21/sekiz olay PASS. Gerçek log kopyasında
who silme ve elapsed=9999 da ortak parser validator'da exit1 verdi; oyuncu
kayıtları bu negatiflerde değiştirilmedi. Ptolemy son C++/PS eklerini salt okunur
inceledi, yeni bulgu bildirmedi. Final tam paket bu kaynakla yeniden çalıştırıldı.

Son kaynakla `RunAllChecks.ps1 -SkipBuild -FullCompile`: **34/34 GECTI**,
16 PCH'siz /W4 dosya ve compiler warning0. Normal Editor derlemesi ayrıca
Succeeded. BasicAttack21/8 gerçek parser kabulü, oturum6/2 kabul, skill17/7 kabul,
parser312/312 ve tüm eski regresyonlar geçti. JSON111 negatif; store322/loaded322,
invalid0/loadFailed0/existsMismatch0/jsonMismatch0/maxLevel6; writeorder1,
roundtrip equal1. Summer49/Winter32 mob doğumu smoke hata0; NetSmoke1join /
1welcome, server/client hata0 (yalnız bağlantı). Kaynak bu son paket sırasında
değişmedi. Gerçek logun xp200 pozitif kontrolüyle aynı PS metadata bloğu geçti;
xp201 yapılan ayrı kopyada aynı blok false verdi. Kaynak ve oyuncu kayıtları
bu log denemesinde değiştirilmedi. Lv33+ beceri boşlukları, InitGameState lint
uyarısı, gerçek PIE/RPC ve kaynak motorla Server/Shipping hedefleri açık.
Proje iskeleti tablosu son sonuçlarla güncellendi.

## 53. Temel saldırı RPC'sinde çift cooldown ve yuvarlama — 2026-10-07

ORTA savaş/ağ hatası önce gerçek Editor DLL ile yeniden üretildi. Eski RPC
`LastMeleeTime = Now - MeleeCooldown` yazar, ardından `AttackTarget` ikinci kez
tam cooldown kontrolü yapardı. Float dünya zamanı 100 saniyeyken 0,6'nın geri
çıkarılıp tekrar çıkarılması yaklaşık 0,5999985 verir; yeterince beklemiş
istek ikinci kapıda reddedilir. Native test **FAIL checks=0 uptime=100**, UE
exit0 fakat PS exit1 verdi. Ekran/input veya ağ taşınması ölçümü değildir.

Public `AttackTarget` ve mevcut `ServerAttackTarget` payload'ı korunur. Private
`AttackTargetInternal(Target, RequiredCooldown)` ortak gövdeyi bir kez çalıştırır.
Doğrudan/host/istemci istek yolunda tam cooldown; authority korumalı RPC girişinde
sunucunun kendi `MeleeCooldown * 0.9f` eşiği seçilir. İstemci eşik gönderemez.
Mevcut %10 tolerans korunur; tam 0,6 saniyeye kıyasla yaklaşık 0,54 saniye RPC
kabulü yeni denge politikası değildir. Hasar, hedef/mesafe/PvP/parti/klan/hub/
koruma kontrolleri, combat ve metrik sırası aynı ortak gövdededir.

`Now` ve replike olmayan `LastMeleeTime` motorun double dünya saatini kullanır;
sentetik geri yazım yoktur. Dünya/stats/ölü/hub ve erken cooldown retlerinde
önceki damga korunur. Kapıyı geçen hedef/geçersiz menzil gibi aşağı akış retleri
mevcut doğrudan saldırı davranışıyla gerçek `Now`yu tüketir. Yeni RPC, replike
alan, kayıt şeması veya istemci hasar yetkisi eklenmedi.

`MMOTestMeleeCooldown` / `TestMeleeCooldown.ps1`: **89 kontrol PASS**. Yedi
sentetik uptime (100, 600, 1000, 3600, 86400, 604800, 10000000 saniye) başına
süresi bitmiş RPC, anlık tekrar, erken istek, toleranslı istek, doğrudan tam
cooldown ve iki eşiğin ±1 mikrosaniye/tam sınırı. Ek olarak ölü ve hub RPC
retlerinde damga korunması, self/uzak/hedefsiz retlerde gerçek damga tüketimi.
Başarı yalnız HP miktarı değil doğru/korunan zaman damgasını da ister.

Test aynı frame'de controllerless iki fixture kullanır; AI/çarpışma/tick kapalı,
öldürme/XP veya oyuncu store yükleme/yazma yoktur. Public World.TimeSeconds
yalnız non-Shipping fixture'da değiştirilir; başarısız kontrol dahil geri alınır,
exec ayrıca saat eşitliğini doğrular ve fixture'lar yok edilir. Bu gerçek uzun
oturum, timer çalışması, istemci replikasyonu veya RPC teslimi kanıtı değildir.
Uzak ağ/jitter, host ve gerçek respawn/travel kontrolü PLAYTEST_CHECKLIST'te açık.

Agency Unreal Multiplayer Architect rolüyle Archimedes ajanı salt okunur
inceleme yaptı; ayrı private eşik, doğrudan yolun tam cooldown'u, ret damgası
ve eşik sınırlarının korunmasını önerdi. Parent kod/test/derlemeyi yürüttü.
Normal Editor derlemesi Succeeded; doğrudan 89 kontrol geçti. Son paket sonucu
aşağıda kaydedildi.

İkinci salt okunur incelemede ajan sentetik zamanların combat text üzerinden
mevcut yerel HUD'a sızabileceğini buldu. Saat geri almak HUD metinlerini geri
almadığı için test non-Shipping `-MMOAllowSyntheticWorldTime` başlatma bayrağı
ister; normal editör/oyun çağrısı fixture kurmadan reddedilir. Bayrak yalnız
PS runner'ın ayrı başlatıp hemen quit ettiği disposable süreç içindir. Runner
bayraklı başarıdan sonra ikinci bayraksız süreçte FAIL checks=0/requires mesajı,
UE exit0 ve hata/yanlış PASS yokluğu bekler. HUD restore implementasyonu veya
oynanışta zaman değiştirme API'si eklenmedi. Beş ek ret fixture'ı damgayı sınar;
HP/koruma/combat/metric bütünlüğünü ayrıca kanıtlamaz. Null fallback taraması
gerçek dünya moblarını kullanır; uzak fixture alanında hedef olmadığı varsayılır.

Başlatma korumasının negatif kanıtı: korumasız eski DLL ile yeni PS runner'da
bayraklı 89 kontrol PASS, ardından bayraksız çağrı da fixture'ı çalıştırdığı için
guard **FAILED exit1, rejections=0** verdi. Koruma eklenmiş DLL normal Editor
derlemesinde Succeeded; doğrudan runner tekrar **89 PASS + unflagged command
rejected before fixtures PASS** verdi. Ajan son guard/PS diff'inde yeni bulgu
bildirmedi ve kapatıldı. İlk tamamlanan paket guard ekinden önce 35/35 idi;
koruma eklendikten sonra tam paket son kaynakla yeniden çalıştırıldı.

Son `RunAllChecks.ps1 -SkipBuild -FullCompile`: **35/35 GECTI**, exit0.
Normal Editor DLL derlemesi ayrıca Succeeded. 16 PCH'siz /W4 dosya, compiler
warning0; ValidateData51 mob/13 elit/53 eşya/79 görev/81 beceri/12 efekt/29 spawner,
parser312/312, lint negatif15/veri negatif14, JSON111 negatif geçti. Store334/
loaded334/invalid0/loadFailed0/existsMismatch0/jsonMismatch0/maxLevel6,
writeorder1/roundtrip equal1. Cooldown89 + bayraksız guard reddi, temel saldırı21/
sekiz olay, skill17/yedi olay, oturum6 ve önceki tüm regresyonlar geçti.
Summer50/Winter27 mob doğumu smoke hata0; NetSmoke1join/1welcome, server/client
hata0 (`mmo_net_server_171451.log`, `mmo_net_client_171451.log`, TEMP dizini).
Final paket boyunca C++ ve test/runner dosyaları değişmedi; Git blob kimlikleri
önce/sonra eşleşti. Smoke doğuş, NetSmoke bağlantı kanıtıdır; gerçek gameplay
replication, uzun oturum, değişik Blueprint cooldown değerleri ve gerçek
Server/Shipping paketleri için kanıt değildir. Lv33+ beceri boşlukları ve
InitGameState lint beyaz liste uyarısı mevcut; compiler uyarısı değildir.
PLAYTEST_CHECKLIST, ağ denetimi ve proje tablosu güncellendi.

## 54. Takas revizyon/çift onay ve ekonomi regresyonu (2026-10-07)

Önceki HEAD `3dec6f5`; Git ağacı yalnız korunacak `.agents/`, `.codex/`,
`AGENTS.md` untracked girdilerini içeriyordu. Aktif yazan paralel ajan yoktu.
Agency Economy Designer ve Test Results Analyzer rehberleriyle salt okunur
Mencius yan incelemesi yapıldı; son kaynak/runner incelemesinde engelleyici sorun
bulmadı, aynanın payload karşılaştırması ve sabit TEMP log sınırları belgeye alındı.
Ajan kapatıldı. Üretim takas kuralı, RPC payload'ı veya replication değişmedi.

Yeni `Private/Tests/MMOTradeSelfTest.cpp`, `RunTradeSelfTest` ve dev-only
`MMOTestTrade`; iki controller'sız karakter, aynı frame, gerçek StartTradeServer
ve teklif/onay/ValidateTick gövdeleri. Zaman kapılarının özel damgaları yaşlandırılır;
dünya saati değiştirilmez, tick/gerçek bekleme çalıştırılmaz. İlk yükleme kapısı
completion save'i engeller. Tüm boolean hata çıkışlarında fixture'lar temizlenir.

| Grup | Native kontrol | Kapsam |
| --- | --- | --- |
| Teklif/onay | 15 | Boş onay, ortak revizyon/ayna adedi, offer throttle, duplicate/stale kimlik, gold clamp/no-op, lock/confirm throttle, tek onayda aktarım yok, değişimde onay temizliği, eski revizyon, remove, dört eşya sınırı |
| Başarılı takas/tekrar | 4 | İki aynı payload'lı eşya; farklı +/nadirlik; iki yön altın; duplicate ilk/son onay; temiz partner/timer; tam snapshot ve alıcının NextInstanceId'sinden ayrılan kimlik |
| Altın sınırları | 12 | İki yönde tam tavan, tavanı bir aşma ve tavanda gönderip alma sonrası net bakiye |
| Çanta sınırları | 8 | İki yönde dolu çantaya yalnız alma reddi ve bir ver/bir al başarısı |
| Son anda yeniden doğrulama | 32 | Her iki taraf için kaldırma, kuşanma, ItemId/+ /nadirlik değişimi, altın harcama, ölüm, uzaklaşma; ara mutasyondan SONRA alınan snapshot korunur |
| ValidateTick | 8 | Ölüm, 800 üstü, idle ve tam 800 sınırı; envanter değişmez |

Toplam **79 kontrol** (compound koşullar içerir, 79 bağımsız ağ senaryosu değildir).
Snapshot karşılaştırması çanta sırası/payload/kimlik, altın, kuşanım, depo, sarf
yığınları ve NextInstanceId'yi kapsar. `TheirOffer.Items` için adet/onay kontrolü
vardır; karşı tarafta gösterilen tüm payload bağımsız karşılaştırılmadı.

Negatif kanıt: SeenRevision koruması geçici `false &&` ile devre dışı bırakılıp
DLL derlendi; runner **FAIL checks=12, stale revision confirmation (check=13)**
verdi. UE exit0 iken PS exit1 verdi. Mutasyon geri alındı; üretim Trade.cpp Git
diff'i boş. Normal DLL tekrar Succeeded, doğrudan test **79 PASS**. Bu yeni bir
oynanış bug düzeltmesi değil, mevcut korumanın regresyon kanıtıdır.

`Scripts/TestTrade.ps1`, tam bir PASS79 satırı, UE exit0 ve hata/FAIL yokluğu ister;
sabit TEMP log adıyla sıralı runner. RunAllChecks'e adım, PCH'siz listeye Trade.cpp
ve test cpp eklendi. Son `RunAllChecks.ps1 -SkipBuild -FullCompile`: **36/36 GECTI,
exit0**; normal Editor DLL derlemesi ayrıca Succeeded. 18 PCH'siz /W4 dosya,
compiler warning0. Lint83 dosya, bilinen InitGameState whitelist uyarısı;
ValidateData51 mob/13 elit/53 eşya/79 görev/81 beceri/12 efekt/29 spawner, mevcut
Lv33+ gap uyarıları. Parser312/312, lint negatif15/veri negatif14, JSON111;
Store340 loaded340 invalid0/loadFailed0/existsMismatch0/jsonMismatch0/maxLevel6,
writeorder1/roundtrip equal1. Takas79, cooldown89/guard ve tüm önceki regresyonlar
geçti. Summer49/Winter27 mob doğumu smoke hata0; NetSmoke1join/1welcome ve iki
tarafta hata0 (`mmo_net_server_174004.log`, `mmo_net_client_174004.log`, TEMP).
Paket logu `mmo_all_trade_20261007.log`; Source/Scripts SHA256 listesi önce/sonra
aynı, kod/test/runner paket boyunca değişmedi. Belge düzenlemeleri ayrıdır.

Gerçek davet/ownership/RPC taşıma/owner-only replication/HUD, iki oyuncunun ortak
kalıcı işlemi ve çökme/yeniden giriş doğrulanmadı. StartTradeServer revizyonunu
0'a döndürdüğü için oturumlar arası aynı revizyonla eski onay erişimi araştırılmalı;
bu tur gerçek istemci exploit'i olarak kanıtlanmadı. Extraction/delivery rollback
dalında synchronous delegate mutasyonuyla fault injection yapılmadı; allocator
ve bildirim davranışı açık. PLAYTEST_CHECKLIST ve NETWORK_AUDIT sınırları içerir.
Gerçek Server/Shipping derlemesi ve Lv1–30 oynanış ölçümü hâlâ açık.

## 55. Takas onayında oturum kimliği ve çizilen revizyon (2026-10-07)

Başlangıç HEAD `222fa92`, temiz tracked Git ağacı; korunacak `.agents/`,
`.codex/`, `AGENTS.md` untracked. Paralel yazan ajan yoktu. Agency Unreal
Multiplayer Architect/QA rehberleriyle salt okunur Pasteur yan incelemesi;
yerel UE5.7 UHT/RepLayout kaynaklarında native uint64 desteği kontrol edildi.

Eski kaynakla gerçek UE reproducer: teklif25 altın/revizyon1 → iptal → yeni
aynı teklif/revizyon1 → önceki onayı yolla. Yeni takasın kendi onay bayrağı true
oldu: **FAIL checks=80, previous session confirmation accepted (check=81)**.
UE exit0/PS exit1. Hata native sunucu kabulüdür; eski kodla gerçek remote paket,
oyuncu ekonomik kaybı veya kalıcı dosya exploit'i gösterilmedi.

Sunucu TradeSessionId = max(iki pawn'ın önceki kimliği)+1 üretir, her iki tarafa
aynı uint64, owner-only ReplicatedUsing OnRep_Trade. Reset/iptal/tamamlama sayacı
korur. MAX'tan sonra fail-closed; global hesap/kalıcı kimlik değildir. Start için
authority/inactive/farklı bileşen/aynı world kapıları; iki state delegelerden önce
kurulur. Confirm RPC `(SeenSessionId, SeenRevision)`; reciprocal partner/session/
revision/görülen çift, onay ve LastConfirmTime mutasyonundan önce doğrulanır.
RPC imzası değişti; iki uç aynı yeni derlemeyi kullanmalı. Save şeması değişmedi.

HUD son DrawTrade'ın weak component ve session/revision çiftini tutar; onayda
RequestConfirmSeen bunları aynen iletir. Başarısız/skipped draw ve DrawHUD'ın
tüm erken çıkışları öncesi cache temizlenir. Sonradan replike edilen yeni değer
click'te eski görülen teklif yerine kullanılmaz. Console/C++ RequestConfirm
bilinçli olarak güncel yerel snapshot'ı onaylar. HUD'ın render/input davranışı
native fixture'da çalıştırılmadı; kaynak + derleme kanıtı, gerçek PIE açıktır.

Takas testi **96 native kontrol**: önceki79 +17. Aynı revizyon/yeni ortak session;
eski/sıfır/ileri session reddi; aynı session stale revision; partner session ve
revision uyuşmazlığının reddi; reddin caller cooldown/approval/envanteri değiştirmemesi;
fresh onay çiftiyle tam tek aktarım; peer önce onaylıyken eski onayla tamamlanmama;
farklı geçmişler, ters sıra ve ilk argümandaki max; active session'ı değiştirmeme;
MAX-1→MAX ve tekrar başlangıç reddi. Kimlikler/alanlar bazı fixture'larda doğrudan
kurulur; büyük kimliğin gerçek RPC/replication ile taşınması test edilmedi.

İlk düzeltme DLL'i normal derlendi; doğrudan94 PASS ve ilk full paket36/36,
18 PCH'siz dosya/warning0, Store346/maxLevel6, Summer49/Winter30, Net1join/1welcome
hata0 (`mmo_net_server_180031.log`, client180031). Son QA üç madde buldu:
DrawHUD erken return cache'i, partner revision testi ve yüksek history'nin ilk
argüman testi. Hepsi giderildi; DLL yeniden Succeeded, doğrudan96 PASS.
Son kaynakla `RunAllChecks.ps1 -SkipBuild -FullCompile` tekrar çalıştırıldı:
**36/36 GECTI, PS exit0**. Takas96 PASS, 18 PCH'siz /W4 dosya ve 0 derleyici
uyarısı; parser312/312, JSON111, lint negatif15/veri negatif14 ve eski regresyonlar
geçti. Store352 loaded352 invalid0/loadFailed0/existsMismatch0/jsonMismatch0,
maxLevel6, writeorder1 ve roundtrip equal1. Summer48/Winter30 mob doğumu,
smoke exit0/hata0; NetSmoke1join/1welcome, server/client hata0
(`mmo_net_server_181112.log`, `mmo_net_client_181112.log`, TEMP).
Paket logu `mmo_all_trade_session_final_20261007.log` (TEMP). Source/Scripts SHA256
listesi paket öncesi/sonrası aynı; kod/test/runner paket boyunca değişmedi.
Belge düzenlemeleri ayrıdır. Son salt okunur Pasteur incelemesi üç düzeltmeyi
doğruladı, kapsam içinde yeni engel bulmadı; ajan kapatıldı. Editör kapalı.
Bilinen Lv33+ beceri boşluğu ve InitGameState whitelist lint uyarıları sürüyor.

Controller'sız/tek-frame fixture, dünya saati değişmez, fixture'ın disk/save kapısı açılmaz.
Gerçek ownership/RPC, owner-only teslim, payload/token atomikliği, Canvas/input,
pawn replacement ve delayed-client ekonomik erişim doğrulanmadı. Cancel/X,
add/remove/gold session taşımaz; ayrı açık eski aksiyon riski. Reentrant envanter
rollback ve iki kaydın ortak kalıcı işlemi, gerçek Server/Shipping/Lv1–30 akış
kanıtı açık. PLAYTEST_CHECKLIST ve O19 bu sınırlara göre güncellendi.

## 56. Takas teklif aksiyonlarında görülen oturum/revizyon (2026-10-07)

Başlangıç HEAD97b9f76; tracked temiz, korunacak .agents/.codex/AGENTS.md
untracked. Editör/başka UE süreci yok; önceki Pasteur kapalı, aktif yazan ajan
yok. Agency Unreal Multiplayer Architect/QA rehberleriyle Noether salt okunur
yan inceleme yaptı; parent kod/test/build/doc işini yürüttü.

Önce yalnız native reproducer eklendi ve eski DLL derlendi: takası iptal et,
yeniden aç, önceki altın25 aksiyonunu al. Yeni teklif/revizyon değişti:
**MMO_TRADE_TEST FAIL checks=79, previous session gold action accepted (check=80)**.
UE exit0/PS exit1. Gerçek Reliable sırası/uzak oyuncu exploit'i/ekonomik aktarım
üretilmedi; server-body teklif mutasyonunun oturum kimliği eksikliği doğrulandı.

Üç offer RPC (add/remove/gold) session+revision taşır. Ortak IsSeenOfferCurrent
authority, karşılıklı aktif partner ve iki tarafta aynı session/revision ile
görülen çifti doğrular. Ret herhangi cooldown, onay, ayna, envanter yazımından
öncedir; normal return, validation disconnect yok. Confirm helper'ı da ortak.
HUD Seen wrapper'lara son çizilen weakcomponent/session/revision ve gold delta
için son çizilen gold ile gider. Console/C++ güncel yerel snapshot; reset0/fresh
revision0 geçerlidir. Sunucu mevcut clamp/kapasite/hız sınırlarını korur.
RPC imzaları değişti; iki uç aynı derlemeye geçmelidir. Kayıt/replication şeması
değişmedi. Cancel/X ve invite/accept ayrı kaldı; genel atomik view garantisi yok.

Native162 PASS = önceki96 +66: setup/old gold/fresh revision0 (3), iki yönde
üç aksiyon * sekiz ret bağlamı (48), fresh Seen ve current wrapper + eski çiftin
tekrarı (12), inactive üç aksiyon (3). Eski/sıfır/ileri session, eski/ileri/
negatif revizyon, partner session/revision mismatch; otherwise-valid remove/add
girdileri. Ret iki teklif ve tüm Item payload aynaları/altın/onay, revizyon,
offer/trade-change/confirm damgaları ve iki envanter snapshot'ını değiştirmez.
Başarılı offer değişimi ortak revizyon artırır ve onayları temizler; aktarım yok.
Controller'sız/tek frame, dünya saati değişmez; fixture disk/save kapısı açılmaz.

Noether'in son sınırlı incelemesi üretim/caller/cache/count engeli bulmadı;
P3 test zayıflığı: post-accept replay aynı eşya/aynı altınla zaten no-op oluyordu.
Test güçlendirildi: eski çiftle henüz teklif edilmemiş bag2 ekleme, hâlâ teklifteki
bag1 çıkarma ve farklı altın19 istenir. Böylece ret idempotent payload'a dayanmaz.
Kontrol sayısı162 kaldı; güçlendirilmiş test DLL'i ve paketi yeniden doğrulandı.
Son salt okunur inceleme iki yöndeki yeni payload'ların geçerli olduğunu ve
162 sayısını doğruladı; yeni bulgu yok, Noether kapatıldı.

Normal DLL derlemesi Succeeded, doğrudan TestTrade.ps1 PASS162/exit0.
İlk162 DLL ile paket36/36 geçti: 18 PCH'siz dosya/warning0, Store358/maxLevel6,
Summer46/Winter32/hata0, Net1join/1welcome/hata0 (`mmo_net_server_182718.log`,
client182718). Son P3 test güçlendirmesinden sonra normal DLL tekrar Succeeded,
doğrudan162 PASS (15:29:20 UTC); son kaynakla tam paket tekrar çalıştırıldı.
Son kaynakla `RunAllChecks.ps1 -SkipBuild -FullCompile`: **36/36 GECTI, exit0**.
18 PCH'siz /W4 dosya, toplam compiler warning C0; native Trade162 PASS.
Parser312/312, lint negatif15/veri negatif14, JSON111 ve önceki tüm regresyonlar
geçti. Store364 loaded364 invalid0/loadFailed0/existsMismatch0/jsonMismatch0,
maxLevel6, writeorder1 ve roundtrip equal1. Summer52/Winter35 mob doğumu,
smoke exit0/hata0; Net1join/1welcome/server-client hata0
(`mmo_net_server_183722.log`, `mmo_net_client_183722.log`, TEMP).
Paket logu `mmo_all_trade_offer_final_20261007.log` (TEMP). Source/Scripts SHA256
listesi paket öncesi/sonrası aynı; test/runner/kod paket boyunca değişmedi.
Belge güncellemeleri ayrıdır. Editör kapalı; bilinen Lv33+ beceri boşluğu ve
InitGameState whitelist uyarıları sürüyor. Gerçek Canvas/input, HUD click,
remote RPC/owner-only teslim, bağlantının sürmesi ve latency UX PIE listesinde
açıktır. Ortak kalıcı takas/rollback/Server/Shipping/Lv1–30 ölçümü tamamlanmadı.

## 57. Aktif takas iptalinde görülen oturum ve ayrı davet reddi (2026-10-07)

Başlangıç HEAD `ddea095`; tracked temiz, .agents/.codex/AGENTS.md untracked
korundu. Aktif yazan ajan veya UE süreci yoktu. Önce eski parametresiz cancel'ın
sunucu gövdesine native reproducer eklendi ve eski üretim kodu normal derlendi.
İptal/yeniden açma sonrası eski iptal yeni takası bitirdi:
**MMO_TRADE_TEST FAIL checks=145, previous session cancel ended new trade (check=146)**
(15:41:59 UTC), UE exit0/PS exit1. Gerçek Reliable sırası veya uzak istemci
exploit'i/ekonomik kayıp oluşturulmadı; gövde düzeyinde oturum eksikliği kanıtıdır.

`ServerCancel(uint64 SeenSessionId)` authority/aktiflik/sıfır olmayan eşleşen
session ve geçerli karşılıklı partner varsa aynı session'ı doğrular. İptal teklif
revizyonuna bağlı değildir: aynı takasta teklif değişince oyuncu çıkabilmelidir.
Bozuk partner karşılıklılığında kendi oturum temizliği kullanılabilir. HUD iptal
ve X son çizilen weak component/session'ı kullanır; eksik draw veya yeni pawn'ın
component'i eski cache ile dispatch etmez. Console/C++ RequestCancel aktifken
son yerel session'ı kullanır; inaktifken ayrı ServerDeclineInvite gönderir.
İnaktif ret aktif takası bitiremez; eski aktif cancel da inaktifken yeni bekleyen
daveti silemez. Ret mutasyon öncesi normal return'dür, disconnect validation yok.
Yeni cancel RPC imzası iki uçta aynı DLL gerektirir. Kayıt şeması ve owner-only
session/offer alanları değişmedi; kabul/otomatik ölüm/ayrılma iptalleri aynı akışta.

Native182 = önceki162 +20: yeni session/onaylı teklif kurulumu (1), iki yönde
eski/sıfır/ileri session, aktifken inaktif ret, partner session uyuşmazlığı (10),
revizyon değişiminden sonra Seen iptali (1), iki current wrapper (2), inaktif eski
cancel'ın daveti koruması ve kendi bekleyen davetini reddi (4), bozuk partnerle
iki yerel temizlik (2). Ret snapshot'ı tam MyOffer/TheirOffer Items payload'ları,
altın/onay, tüm envanter, partner/pending/name/session/revision, offer/change/
confirm/invite damgaları, aktiflik/timer durumunu karşılaştırır. Geçerli iptal
çift envanteri korur; Ended kontrolü teklif/partner/timer temizliğini sınar.
Controller'sız/aynı frame, dünya saati değişmez; fixture kayıt yüklemez/save kapısı
açılmaz. Canvas/input ve gerçek RPC taşınması bu testte çalıştırılmaz.

Üretim/test DLL derlemesi editör kapalıyken **Succeeded** (15:43 UTC'de başlatılan
build'in completion'ı bu turda alındı). İlk doğrudan native182 PASS'e rağmen runner
hâlâ162 beklediğinden PS exit1 verdi; beklenen marker182'ye güncellendi. Tekrar
TestTrade.ps1 **PASS checks=182, exit0** (18:45:28 UTC). Ardından son kaynak ve
runner ile tam paket başlatıldı. Önceki bekleyen heartbeat'ler sırasında yeni
kod/test/commit yapılmadı; kesilen bu güvenli değişiklik önce tamamlandı.

Agency Unreal Multiplayer Architect ve QA rehberleri kullanıldı. Boyle salt
okunur inceleme için açıldı, kullanım sınırıyla errored döndü; inceleme sonucu
üretemedi ve kapatıldı. Bağımsız ajan onayı yoktur. Parent caller'ları, sunucu
kapılarını, native20 kontrol sayısını, iki HUD dispatch dalını yerel olarak
inceledi; kod/test/build/belgeyi yürüttü. Davet kabul/reddi hâlâ nonce taşımaz;
inaktifken gecikmiş ret daha yeni daveti silebilir. Ayrı açık kapsamdır.

Son kaynakla `RunAllChecks.ps1 -SkipBuild -FullCompile`: **36/36 GECTI, exit0**.
18 PCH'siz /W4 dosya, toplam compiler warning C0; native Trade182 PASS.
ValidateData temiz (Lv33+ beceri boşluğu uyarıları sürüyor), StaticLint temiz
(InitGameState override whitelist uyarısı sürüyor), negatif lint15/veri14,
parser312/312, JSON111 ve önceki tüm regresyonlar geçti. Store370 loaded370
invalid0/loadFailed0/existsMismatch0/jsonMismatch0, maxLevel6, writeorder1 ve
roundtrip equal1. Summer51/Winter34 mob doğumu, smoke exit0/hata0;
NetSmoke1join/1welcome, server/client hata0 (`mmo_net_server_215329.log`,
`mmo_net_client_215329.log`, TEMP). Paket logu
`mmo_all_trade_cancel_final_20261007.log` (TEMP). Source/Scripts SHA256 listesi
paket öncesi/sonrası aynı; kod/test/runner paket boyunca değişmedi. Belge
güncellemeleri ayrıdır; editör/UE süreçleri kapalı. Gerçek owning-client RPC,
owner-only teslim, bağlantının sürmesi, Canvas/input/pawn değişimi ve genel
payload/token atomikliği PIE listesinde açık. Bekleyen davet nonce'u, reentrant
rollback/ortak kalıcı işlem, gerçek Server/Shipping ve Lv1–30 oynanış ölçümü
bu sınırlı düzeltmeyle tamamlanmadı.

## 58. Takas daveti kimliği ve sohbet gönderim bağlamı (2026-10-07)

Başlangıç HEAD `e1cf40e`; tracked temiz, .agents/.codex/AGENTS.md korundu;
aktif yazan ajan/UE süreci yoktu. Önce eski parametresiz ret gövdesiyle native
reproducer ve eski üretim DLL'i derlendi. Yeniden kurulmuş bekleyen davet silindi:
**MMO_TRADE_TEST FAIL checks=165, old decline cleared replacement invite (check=166)**
(19:03:08 UTC), UE exit0/PS exit1. Gerçek gecikmeli ağ paketi/uzak exploit yapılmadı;
parametresiz sunucu gövdesinin eski ret niyetini yeni davetten ayıramadığı kanıtıdır.

Alıcının pawn ömrü içinde artan owner-only uint64 `PendingInviteId` eklendi.
Gerçek davet seçimi/menzil/cooldown/duplicate/takeover kapılarından sonra
SetPendingInviteServer authority, aynı dünya ve farklı geçerli inviter ile yeni
kimlik verir; MAX'ta yeniden kullanmadan kapalı kalır. Kimlik tüketim/oturum
temizliğinden sonra korunur. ServerAccept/ServerDeclineInvite artık SeenInviteId
alır; authority/aktiflik/sıfır olmayan eşleşme ret kapısı pending alanlarını
silmeden öncedir. Mevcut kabul anındaki süre/ölüm/menzil/partner kontrolleri sürer.
C++/Character Exec wrapper'ı yerel güncel kimliği RPC'ye gönderir. Kayıt şeması
değişmedi; active session ve invitation generation ayrı kimliklerdir.

Say ve OnChatCommitted aynı SendChatWithContext yardımcısına bağlandı. Genel
ServerSendChat sözleşmesi metne ek gönderim anındaki pawn, inviteId ve yalnız
aktifken sessionId taşır. /trade kabul ve iptal dalları bu Seen değerleri kullanır;
geliş anında sunucunun güncel wrapper'ına fallback yoktur. İptal niyeti session0
ise invitation decline, nonzero ise active cancel'dır. Pawn pointer mevcut GetPawn
ile uyuşmadan accept/decline/cancel çalışmaz; sayısal ID yeni pawn'da çakışsa da
eski controller mesajı yeni pawn'a yönlenmez. Normal 200 karakter/0,7 sn sohbet
kontrolü korunur; diğer sohbet dalları metadatasını kullanmaz. RPC'nin ek kimlik/
pawn alanları sohbet trafiği maliyetini artırır; gerçek bant ölçülmedi. Chat ve
trade RPC imzaları değiştiğinden iki uç aynı DLL ile çalışmalıdır.

Native218 = önceki182 +28 invitation +8 controller chat kontrolü. Invitation:
iki yönde fresh generation/retention/replacement, eski/sıfır/ileri ID için
accept+decline retleri, fresh Seen/current accept, active decline/duplicate accept
ve session cleanup (2*13); MAX-1->MAX ve overflow ret (2). Ret helper'ı iki tarafın
tam offer/ayna payload'ları, envanter, partner/pending/name/session/revision/
inviteID, saatler, aktiflik/timer snapshot'ını karşılaştırır. Önceki cancel test
kurulumları da gerçek generation seam'ini kullanır; toplam önceki182 korunur.

Ek8 kontrol: ayrı üç native PC+Pawn ve gerçek nearest invite seçimi; native
OnChatCommitted fresh decline, başka inviter ile replacement; eski chat accept,
eski decline sonrası Say fresh accept, inactive intent active ret, wrong-pawn
ve null context active cancel ret, Say fresh active cancel/no-transfer. Null
context testi active geçerli session iptaliyle aksiyonel hale getirildi; duplicate
accept tam snapshot ret helper'ını kullanır. Component testleri controller'sız,
chat kısmı native controller'lıdır; hepsi aynı frame, dünya saati değişmez.
Slate input/RPC taşıma veya gerçek owning-remote bağlantı çalıştırılmaz. Fixture
kayıt yüklemez; iki karakterin kalıcı takas save kapısı bu kanıtın parçası değildir.

İlk component DLL'i Succeeded ve native210 PASS; sonra controller8 eklenip DLL
Succeeded/native218 PASS. Runner eski182 beklediği için bu ilk PASS'lerde PS1;
runner218'e güncellendi. Test güçlendirmeleri sonrası normal DLL Succeeded ve
**TestTrade.ps1 PASS checks=218 / exit0 (19:11:23 UTC)**. Son kaynak/runner ile
RunAllChecks -SkipBuild -FullCompile başlatıldı; final kanıt aşağıya eklenir.

Agency Unreal Multiplayer Architect/QA rehberleriyle Nietzsche salt okunur
caller incelemesi yaptı: iki chat gönderim yolu, iki server trade dalı ve pawn
replacement/intent sınırlarını belirledi. Parent bunları uyguladı, kod/test/build
ve belgeleri yürüttü. Görülen bildirimle kimlik atomik değildir: gönderimdeki
latest replicated davet kimliği yakalanır, oyuncunun daha eski sohbet satırını
okuyup yeni kimlik replike olduktan sonra kabul yazması engellenmez. Bildirim
kimliği/görünümü ayrı açık iştir. Metin kimlikten önce gelebilir; eski/0 kimlik
ret edilir, güncel kimlik geldiğinde yeniden denemek gerekir. Bunlar PIE listesinde.

Nietzsche son salt okunur incelemede imza/dispatch sorunu bulmadı, count218 ve
runner/header kapsamını doğruladı. Null context cancel'ın geçerli aktif session
ile aksiyonel testi ve duplicate accept'in tam snapshot'ı incelendi. İki sınır
not edildi: duplicate accept boş pending durumunda guard'ın tek başına varlığını
kanıtlamaz; chat fresh cancel yalnız active/envanteri sınar, tam partner/offer/
timer temizliği component kontrollerindedir. Parent bunları kapsam sınırı olarak
kaydetti; ajan son DLL/testi çalıştırmadı, sonuç doğrulaması parent'tadır. Ajan kapatıldı.

Son kaynakla `RunAllChecks.ps1 -SkipBuild -FullCompile`: **36/36 GECTI, exit0**.
18 PCH'siz /W4 dosya, toplam compiler warning C0; native Trade218 PASS.
ValidateData/StaticLint temiz; bilinen Lv33+ beceri boşluğu ve InitGameState
whitelist uyarıları sürer. Negatif lint15/veri14, parser312/312, JSON111 ve eski
regresyonlar geçti. Store376 loaded376 invalid0/loadFailed0/existsMismatch0/
jsonMismatch0, maxLevel6, writeorder1 ve roundtrip equal1. Summer53/Winter31
mob doğumu, smoke exit0/hata0. NetSmoke1join/1welcome, server/client hata0
(`mmo_net_server_222021.log`, `mmo_net_client_222021.log`, TEMP).
Paket logu `mmo_all_trade_invite_final_20261007.log` (TEMP). Source/Scripts
SHA256 listesi paket öncesi/sonrası aynı; kod/test/runner paket boyunca değişmedi.
Belge güncellemeleri ayrıdır. Editör/UE süreçleri kapalı. Gerçek remote ownership,
RPC/owner-only teslim, bağlantının sürmesi, Slate input/pawn replacement ve
bildirim ile kimliğin ortak görünümü PLAYTEST_CHECKLIST'te açık. Ortak kalıcı
işlem/reentrant rollback/Server/Shipping/Lv1–30 oynanış ölçümü tamamlanmadı.

## 59. Parti daveti kimliği ve sohbet gönderim bağlamı (2026-10-07)

Başlangıç HEAD `c4effd1`; tracked temiz, .agents/.codex/AGENTS.md korundu;
aktif UE süreci yoktu. QA ajanı yalnız okudu, dosya yazmadı. Önce eski üretim
koduyla replacement pending fixture eklendi: normal DLL Succeeded, UE testinde
**FAIL checks=10 reason=old decline cleared replacement party invite**
(19:27:17 UTC), UE exit0 / script exit1. Parametresiz ret yeni daveti siliyordu;
gerçek gecikmeli Reliable paket veya uzak istemci exploit'i çalıştırılmadı.

Recipient pawn ömründe artan uint64 PendingInviteId owner-only replike olur.
Gerçek nearest davet yolu mevcut cooldown/aynı daveti yenilememe/davet kapma/
lider rızası kontrollerinden sonra SetPendingInviteServer ile kimlik atar.
Helper authority, aynı dünya, farklı geçerli inviter ve MAX taşmasını denetler.
Tüketim ve parti ayrılmasında kimlik korunur; MAX'ta wrap/yeni kimlik verilmez.
Accept/decline nonzero SeenInviteId eşleşmesini pending alanlarını silmeden önce
sunucuda denetler. Geçerli kabulün süre/mesafe/lider rızası/kapasite kapıları sürer.
Character Exec/current wrapper yerel güncel kimliği taşır; eski uçlar aynı yeni
DLL'e yükseltilmelidir. Kayıt şeması ve parti leave/kick protokolü değiştirilmedi.

Say ve OnChatCommitted'dan ortak SendChatWithContext artık parti davet kimliğini
de yakalar. ServerSendChat'te /parti kabul/ret gönderim anındaki pawn ve kimliği
kullanır; sunucunun güncel davetine fallback yoktur. Önceki trade dispatch aynı
kalır; pawn parametresi SeenCommandPawn olarak adlandırıldı. Tüm sohbetlerde ek
uint64 metadata taşınır; gerçek bant maliyeti ölçülmedi. 200 karakter/0,7 sn sohbet
kontrolü değişmedi. Bir davetin metni ile replike ID atomik görünüm sağlamaz;
eski görünür bildirim/yeni latest ID sınırı açık, kimlik önce gelmezse yeniden
denemek gerekir. Bu değişiklik yalnız yoldaki eski accept/decline niyetini ayırır.

Native46 = önceki10 lider/birleşme +28 component davet +8 native controller chat.
Önceki10 fixture daveti gerçek generation helper ile kurar; tüm issuance dönüşleri
kontrol edilir. İki recipient yönde (birinde aynı inviter reissue, diğerinde farklı sender):
fresh generation/decline/retention/replacement; eski/0/ileri ID için accept ve
ret (6), fresh Seen/current accept doğru inviter, duplicate accept, leave sonrası
ID retention, current decline (2*13). Son2 MAX-1->MAX/overflow fail-closed; pending pointer/ID/zaman korunur.
Ret snapshot'ı dokuz karakterin sıralı Members/Leader, pending pointer/zaman/ID,
LastInviteTime ve tüm inventory save payload'ını karşılaştırır. Duplicate accept
boş pending ile idempotence kanıtıdır; ID guard'ın tek başına varlığını kanıtlamaz.

Chat8: üç native PC+Pawn, gerçek nearest ilk davet; OnChatCommitted ret sonrası
farklı inviter; eski accept/decline tam party/pending/envanter korunması; geçerli
ID ile native possession değiştirilmiş/null pawn ret; Say güncel kabul doğru iki üyeyi birleştirir;
OnChatCommitted mevcut leave yolu iki kişiyi solo temizler. Snapshot chat throttle
ve component LastInviteTime alanını kapsamaz; component ret fixture'ı LastInviteTime
kapsar. Aynı frame; saat/timer/Slate input/ownership/gerçek RPC taşıması çalışmaz.
Pawn replacement testinde PC0/PC1 unpossess edilir; PC1 Pawn0'ı possess eder,
aynı sayısal nonce ve geçerli pending fixture'da kurulur. Eski Pawn1 bağlamının
reddi tam snapshot ile ölçülür; possession geri alınır. Bu native possession
testidir, gerçek remote respawn/yeniden bağlantı değildir. Controller'lar pawn'dan
ayrılarak yok edilir; fixture karakter kaydı yüklemez.

İlk yeni normal DLL Succeeded; doğrudan TestPartyConsent **PASS checks=46 / exit0**
(19:30:26 UTC). Kimlik kapısını kasıtlı bozma ve son restore/paket kanıtı aşağıda.

Negatif kanıt: accept'in yalnız SeenInviteId nonzero/eşleşme kapısı geçici
kaldırıldı (authority kaldı), normal DLL Succeeded; test **FAIL checks=13
reason=stale/zero/future party accept mutated state** (19:31:02 UTC), UE0/PS1.
Eski kimlik aksi halde geçerli replacement daveti birleştiriyordu; yeni snapshot
bunu yakaladı. Kapı restore edildi; negatif DLL/kod commit edilmez. QA'nın dört
fixture boşluğu (issuance dönüşü, aynı inviter, native possession, MAX zaman)
count46 korunarak giderildi. Restore/son güçlendirmeyle normal DLL Succeeded.

Son doğrudan TestPartyConsent **PASS46 / exit0 (19:33:39 UTC)**. Arendt salt
okunur son incelemede guard restore/caller/46 count ve dört güçlendirmeyi doğruladı;
yeni engel bulmadı, native testleri çalıştırmadı. Yerel Unreal Multiplayer Architect
TOML ve repo QA rehberi kullanıldı; Agency kökeni doğrulanmadı. Ajan kapatıldı.
Parent son kaynak/runner ile RunAllChecks -SkipBuild -FullCompile'ı başlattı.

Son aynı kaynak/runner ile `RunAllChecks.ps1 -SkipBuild -FullCompile`: **36/36
GECTI, exit0**. 18 PCH'siz /W4 dosya, compiler warning C0. Native Party46 ve
Trade218 geçti. ValidateData/StaticLint temiz; bilinen Lv33+ beceri boşluğu ve
InitGameState override whitelist uyarıları sürer. Negatif lint15/veri14,
parser312/312, JSON schema2/negative111 ve eski regresyonlar geçti.
Store382 loaded382 invalid0/loadFailed0/existsMismatch0/jsonMismatch0, maxLevel6,
writeorder1, roundtrip equal1. Summer49/Winter31 mob doğumu, smoke exit0/hata0.
NetSmoke1join/1welcome, server/client hata0 (`mmo_net_server_224246.log`,
`mmo_net_client_224246.log`, TEMP). Paket logu `mmo_all_party_invite_final_20261007.log`
(TEMP). Source/Scripts SHA256 listesi paket öncesi ve tüm testler sonrası aynı;
kod/runner paket boyunca değişmedi. Sonraki belge güncellemeleri ayrıdır.
Editör/UE süreçleri kapalı. Gerçek owner-only teslim/RPC gecikmesi/expiry/
respawn/Slate, bildirimin görülen kimliği, kalıcı kayıt/hesap/dedicated ve eski
leave/kick intent sınırları PLAYTEST_CHECKLIST'te açık; oynanmış sayılmadı.


## 60. Controller ömründe ilk oyuncu ölümü — sınırlı Lv1–30 ölçümü

2026-10-07: önceki HEAD dd9f354 ve temiz tracked durum incelendi; untracked
.agents/.codex/AGENTS.md korundu. UE/UBT açık değildi. Önceki parti işinin
commit edilmemiş kaynağı yoktu. Bu iş bir exploit düzeltmesi değil, §3.1/M9
ilk ölüm ölçümünü ilerleten sınırlı eklemedir; oynanış/ceza dengesi değişmedi.

AMMOPlayerController::RecordFirstDeathMetric yalnız non-Shipping'de, mevcut
pawn stats dead=true/finite health=0 ve nonnegative gerçek xploss ile ortak
server controller/pawn authority/session/tekillik kapısına girer. HandleDeath
stats ApplyDeathPenalty dönüşünden sonra hook'u çağırır; PC'nin mevcut pawn'ı
ölen karakterle eşleşmelidir. first_death payload by=mob arketipi veya none,
xploss=gerçek kayıp; lv emit anındaki seviyedir. none bilinmeyen/non-mob
anlamına gelir, PvP adı veya tam killer/DoT sahipliği değildir. BeginPlay
başlangıcı ve controller sid kullanılır; pawn/aktarımda TSet korunur. Shipping
stub false döner; yeni replike state/RPC, kalıcı kayıt veya gameplay mutation yok.

TestFirstDeath.ps1 native14: living/positive health/negative loss/NaN ret,
spawn protection ve nonlethal ret (gate6); mob öldürücü hasar/XP20→15/seviye1,
movement disable ve respawn timer kurulumu; ölüye tekrar hasar, Revive ve ikinci
ölüm XP15→10; aynı PC yeni pawn'ın ölümü; yeni PC sıfır loss; non-mob kaynağın
XP2→0 clamp'i/seviye3; controllerless NPC ölümünde XP'nin değişmemesi. Gerçek
TakeDamage/OnDeath/penalty gövdeleri aynı frame çalışır: 3 first ve5 normal death,
3sid. PS payload5/0/2, gate öncesi sessizlik, who ve first→regular ordering'i sınar.
Genel oturum6 testine sentetik dead/zerohealth ile first death eklendi; 2 first,
pawn replacement/SeamlessTravelTo aktarımı/newcontroller tekilliği kontrol edilir.

İlk normal derleme testte var olmayan TNumericLimits::QuietNaN yüzünden
C2039/C3861 verdi; testte std::numeric_limits + açık <limits> ile giderildi.
Yeni native14 ilk PASS20:00:25 UTC. Negatif kontrol: yalnız hook kaybı geçici
Lost+1 yapıldı; normal DLL Succeeded, native14/PASS kalsa da PS payload kapısı
exit1 verdi (3first/5death/3sid, UEexit0). Negatif log
mmo_first_death_wrong_loss_20261007.log (TEMP). Gerçek ceza değişmedi; test yanlış
ölçüm payload'ını yakaladı. Kaynak tam restore edildi, DLL Succeeded; native14
20:03:06 UTC, genel oturum6 20:02:54 UTC ve shared parser doğrulaması geçti.
Sonradan hook mevcut-pawn eşleşmesi güçlendirildi; son DLL/paket ayrıca aşağıda.

Helmholtz yerel Unreal Multiplayer Architect TOML ve repo QA rehberiyle salt
okunur inceleme yaptı; Agency paket kökeni doğrulanmadı. Native14/gate6,
authority/Shipping/cleanup ve PS metadata kapılarını doğruladı, test çalıştırmadı.
İnceleme sırasında Volta'nın parser testleri henüz yazılıyordu; final wiring
beklendi. Native validator'ın değişen who altında aynı sid tekrarını kaçırması
önerisi yazıcıya iletildi; ortak parser'ın (who,sid) tarihsel gruplaması ayrıdır.
Death Blueprint/XP callback sonrası ownership/health değişimi incelenmiş bir
runtime örnekle kanıtlanmadı; yeni pawn eşleşme kapısına rağmen callback testleri,
gerçek respawn/timer, remote delivery, seamless travel, persistence ve input açık.

Volta yalnız dört parser/test/fixture/validator dosyasını yazdı; mmo-developer
rehberi kullanıldı. Test wiring tamamlandı: fixture41 parsed olay/27 invalid,
malformed payload+clock/tekillik/geri kazanım/counter isolation/legacy/CLI.
Loss integer metni de şarttır; float yuvarlama/underflow kesirli XP'yi kabul
ettirmez. Native validator her raw marker'ın parse/accept edilmesini ve global
normalized sid tekilliğini ister; missing/empty who, overwritten/repeated event,
cross-who aynı sid negatifleri eklendi. Shared parser (who,sid) gruplaması
korunur. 576/576 assert geçti; Source/Scripts freeze öncesi ajan kapatıldı.
Parent C++/PS/docs ve tüm UE derleme/testlerini yürüttü; iki ajan da kapalı.

Son pawn eşleşme guard'ıyla normal Editor DLL **Succeeded (4,83 sn)**;
`RunAllChecks.ps1 -SkipBuild -FullCompile` **37/37 GECTI, exit0**. İlk ölüm14
PASS20:11:39 UTC, genel oturum6 PASS20:11:30 UTC, parser576/576. Parti46,
takas218 ve tüm eski regresyonlar geçti. 18 PCH'siz /W4 dosya, warning C0.
ValidateData ve StaticLint temiz; bilinen Lv33+ beceri boşluğu/InitGameState
whitelist uyarıları sürer. JSON schema2/negative111; store388 loaded388,
invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1,
roundtrip equal1. Bu kayıt adedi üretim oyuncu sayısı veya Lv1–30 testi değildir.
Summer50/Winter27 mob doğumu, smoke exit0/error0; NetSmoke1join/1welcome,
server/client error0 (TEMP mmo_net_server_231427.log / mmo_net_client_231427.log).
Tam paket TEMP mmo_all_first_death_final_20261007.log. Paket öncesi/sonrası147
Source/Scripts SHA256 listesi aynıdır; build/test boyunca kod/runner değişmedi.
Son belge güncellemeleri ayrıdır. UE/UBT kapalı. Gerçek Blueprint callback,
Shipping/Server paket, timer/respawn/DoT/PvP kaynak kimliği ve20dkPIE açık;
PLAYTEST_CHECKLIST maddeleri işaretlenmedi.


## 61. Klan kabulünü gönderimdeki davet kimliği ve pawn'a bağlama

8 Ekim 2026 (yerel saat; önceki native loglar 7 Ekim UTC): önceki HEAD
3df7a3e, son commitler, tracked durum ve paralel dosyalar incelendi. Beş kod/test
dosyası parent tarafından yazıldı; .agents/.codex/AGENTS.md korunur. Carver yerel
Unreal Multiplayer Architect ve repo QA rehberiyle salt okunur inceleme yaptı;
Agency paket kökeni doğrulanmadı. Kod yazmadı/test çalıştırmadı. Son olumlu
kaynakların TEMP yedekleriyle byte/SHA eşitliğini bağımsız denetledi, somut hata
bulmadı. Son agent kapatma çağrısında ID artık bulunamadı; aktif yazıcı yok.

**O24/ORTA, sınırlı yeniden üretim:** parametresiz CmdAccept sunucudaki güncel
pending daveti kullanıyordu. Eski A niyetinden sonra B pending'i geçerli hale
getirilirse eski kabul B klanına katılıyordu. Eski kodda native FAIL checks=8,
reason=old clan accept joined replacement invite, 20:23:07.388 UTC; UE0/PS1.
TEMP mmo_clan_invite_before_generation_20261007.log. Baseline fixture doğrudan
pending/klan alanını değiştirdi; gerçek gecikmiş RPC, sürenin geçmesi veya canlı
CmdInvite takeover çalıştırılmadı. Üretimde geçerli başka bekleyen daveti ezme
koruması vardı; hata onun aşılması kanıtı değildir. Tüketim/expiry/invalidation
sonrası yeni davete taşınan eski niyet sınırıdır.

Alıcı pawn'da owner-only replicated uint64 PendingInviteId eklenir. Üretim
CmdInvite mevcut canlılık/liderlik/menzil/3 sn cooldown/duplicate ve takeover
kapılarından sonra SetPendingInviteServer kullanır; helper artan kimlik ve
sunucu pending alanlarını birlikte yazar. Tüketim/ayrılma kimliği sıfırlamaz,
MAX'ta yeni davet fail-closed reddedilir; kayıt şemasına alan eklenmedi.
CmdAcceptSeen authority/nonzero/eşitlik kapısını pending tüketiminden önce
uygular. Güncel eşleşmede eski 30 sn süre/canlılık/mevcut üyelik/davetçinin
halen aynı klanın lideri olması kuralları korunur. Native sunucu convenience
CmdAccept yalnız mevcut kimlikle delegasyon yapar; chat bu fallback'i kullanmaz.

Say ve OnChatCommitted, gönderen pawn + latest replicated clan ID yakalar;
ServerSendChat /klan kabul, Seen pawn mevcut GetPawn ile aynıysa CmdAcceptSeen
çağırır. Null/yanlış pawn, yeni pawn'da sayısal ID aynı olsa da ret üretir. Diğer
klan komutları ve 200 karakter/0,7 sn chat sınırı değişmedi. RPC imzası bütün
chatte ek clan uint64 taşır, bant gerçek profiler ile ölçülmedi; iki uçta aynı
yeni DLL gerekir. Owner-only ID ile bildirim metni atomik değildir; oyuncunun
okuduğu eski satırı güncel ID'ye bağlama UX riski sürer.

**43 native kontrol = eski9 +23 component +11 controller chat.** Component:
aynı/farklı davetçiden tüketim sonrası yeni generation; eski/0/ileri retlerde
üç karakterin üyelik/lider/acting/JoinSeq/pending/ID/saat/cooldown ve inventory
snapshot'ı korunur; fresh/duplicate kabul, leave sonrası ID, ölü eşleşmede eski
tüketim kuralı ve MAX son kimlik/overflow sınanır. Generation fixture helper'ı
doğrudan kullanır; üretim cooldown/nearest/expiry akışı bu 23 kontrolün iddiası
değildir. Controller: dört native PC+Pawn, gerçek nearest CmdInvite,
OnChatCommitted/Say kabul/ayrıl ve ham endpoint; native possession değişimi,
fixture'da elle tohumlanan generation ile yeni pawn'da geçerli aynı sayısal kimlikli davet, yanlış/null bağlam, stale/0/ileri
ID ve fresh doğru davetçi. Controller retleri public clan/save ve inventory
snapshot'ını denetler; protected pending alanlarının tam snapshot'ı component
testindedir. Replacement ret sonrası güncel kabul davetin korunmasını doğrular.
Fixture'lar aynı senkron frame'de, load timer çalışmadan temizlenir; klanlar
teardown/leader election öncesi silinir. Slate/input, gerçek remote ownership,
owner-only teslim ve disk kalıcılığı kanıtı değildir.

**Negatif kanıt:** olumlu DLL Succeeded12,95 sn; doğrudan PASS43 20:33:13.767 UTC.
Yalnız nonzero/eşitlik guard'ı kaldırılarak DLL Succeeded3,09 sn; FAIL12/stale
clan acceptance mutated replacement (20:34:01.612 UTC), UE0/PS1. Kaynak byte
exact geri yüklendi. Ardından yalnız /klan kabul pawn guard'ı kaldırıldı;
DLL Succeeded4,34 sn; FAIL38/check39 replaced pawn matching generation
(20:34:33.694 UTC), UE0/PS1. Her iki negatif test gerçek derlenmiş DLL üzerindedir.
TEMP mmo_clan_invite_negative_generation_20261007.log ve
mmo_clan_invite_negative_pawn_20261007.log. Pawn guard da byte exact restore;
son normal Editor DLL Succeeded4,19 sn; restored PASS43 ve chat PASS11
20:35:16.958 UTC. TEMP mmo_clan_invite_restored_20261007.log. PS runner artık
ana43 + chat11 marker ve UE/error kapılarını ister. Carver pawn negatifinin
sonucunu bağımsız doğrulamadı; bu sonuç parent logundan alınmıştır.


**Son doğrulama (8 Ekim UTC):** RunAllChecks.ps1 -SkipBuild -FullCompile
37/37 GECTI, exit0. Normal DLL ayrıca Succeeded; 18 PCH'siz /W4 dosya warning C0.
Klan43 PASS15:04:05.432 UTC (PS ayrıca chat11 ister), parti46/takas218, ilk
ölüm14/3first+5death ve parser576/576 dahil tüm regresyonlar geçti. ValidateData
ve StaticLint temiz; bilinen Lv33+ skill boşluğu/InitGameState whitelist
uyarıları sürer. JSON schema2/negative111; store394 loaded394,
invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1,
roundtrip equal1. Kayıt adedi oyuncu/PIE/Lv1–30 tamamlama sayısı değildir.
Summer51/Winter27 doğum, smoke exit0/error0; NetSmoke1join/1welcome ve
server/client error0. TEMP mmo_net_server_180739.log / mmo_net_client_180739.log,
tam paket mmo_all_clan_invite_final_20261007.log. Öncesi/sonrası147 Source/Scripts
SHA256 listesi eşittir; kod/runner test boyunca değiştirilmedi. Son doküman
editi ayrı tutuldu. Gerçek PIE/Server/Shipping ve bildirimin metniyle kimliğin
ortak görünümü açıktır; PLAYTEST_CHECKLIST maddeleri işaretlenmedi.

**ECC uygulaması:** Yeni ecc-mmo-plan/review/verify dosyaları okunarak bu işin
kalan kapsamı ve kabul ölçütleri belirlendi: mevcut dört C++ dosyası ve bir
runner, authority/nonzero/ID/pawn kapıları, normal DLL +43 native + iki negatif
ve tam paket, dürüst PIE maddeleri, ardından yalnız doğrulanan dosyalarda commit.
Ayrı küçük-iş plan belgesi oluşturulmadı. ECC review ile header/caller/replication,
weak UObject ömrü, pending tüketim sırası, MAX ve save uyumluluğu incelendi;
ek somut düzeltme gerektiren hata bulunmadı. Yeni timer/Tick veya save alanı
yok. İnceleme remote ownership testi sayılmaz. Ek yalnız değişen Source/Scripts
satırlarında sınırlı credential-pattern taraması 0 sonuç verdi; bütün depo
güvenliği garantisi değildir. ECC verify sonuçları:

| Kontrol | Sonuç | Sınır |
| --- | --- | --- |
| UBT / UHT / C++ imzalar / 18 PCH'siz dosya | Geçti | Editor Development; Server/Shipping değil |
| Veri / lint / ilgili native / tam paket | Geçti, 37/37 | 43 clan ve iki negatif; coverage yüzdesi ölçülmedi |
| ECC kod/diff incelemesi | Ek bulgu yok | Sınırlı değişiklik, eski açık O24 UX ve diğer klan komutları sürer |
| Gerçek host + uzak istemci PIE | Yapılmadı | Native fixture ve NetSmoke bütün feature replikasyonunu kanıtlamaz |

Skill kurulumunun .agents altındaki başka sohbet işi değiştirilmedi veya bu
oyun commit'ine eklenmedi. Kullanıcı uzun süre bilgisayar başında olmayacağını
ve yeni ECC becerileriyle geliştirmeyi sürdürmemizi ayrıca belirtti.


## 62. Parti ayrılmasını kişisel üyelik ömrüne bağlama — 2026-10-08 (O25)

**ECC plan / kapsam:** Başlangıç HEAD04d2921, tracked ağaç temiz;
.agents/.codex/AGENTS.md kullanıcı/başka sohbet dosyaları korunur. Küçük iş
kabul ölçütleri: gecikmiş eski leave replacement/same-party rejoin üyeliğine
uygulanmasın; sürekli üyelerin ayrılma hakkı roster/lider değişiminde korunsun;
chat gönderim pawn + kişisel membership ile dispatch yapsın. Mevcut party
component/controller dört C++ dosyası ve TestPartyConsent.ps1 değişti. Save,
kick, disconnect lifecycle, atomik HUD snapshot'ı kapsamı genişletilmedi.

**Önce hata:** Native regression eski parametresiz ServerLeave'i A-B dağılıp
A-C oluştuktan sonra çağırdı: normal Editor DLL Succeeded13,48 sn, ardından
FAIL38 reason=old leave removed replacement party, 15:21:10.421 UTC;
UEexit0 ama PS runner1. TEMP mmo_party_leave_baseline_20261008.log. Bu test
gerçek gecikmiş RPC değildir; aynı-frame doğrudan native intent fixture'ıdır.

**Uygulama:** Grup için owner-only FGuid PartySessionId; her kişinin katılım
ömrü için ayrı owner-only PartyMembershipId. Solo oluşum yeni session alır.
Davetçinin mevcut session'ı, yoksa davet edilenin mevcut session'ı kullanılır.
ApplyPartySet önceki session farklıysa veya personal kimlik yoksa ayrı NewGuid
üretir, sonra alanları/delegate'i yazar. Ayrılan/son kişi iki alanı temizler;
survivor grubu eski session ile yeniden düzenlenir. Bu nedenle aynı gruba
rejoin ve diğer gruptan merge yeni personal ID üretir; mevcut üyelerin ID'leri
roster/lider değişiminde korunur. GUID bir yetki/capability veya kalıcı anahtar
iddiası değildir; ownership ve authority kontrolleri sürer.

RequestLeave convenience yerel current membership'ı yakalar. ServerLeave
HasAuthority + inparty + nonzero + eşitlik kontrolünden sonra mevcut cleanup'ı
çağırır. ServerSendChat yedinci argümanıyla gönderimdeki membership'ı taşır;
/parti leave/ayril ayrıca SeenCommandPawn == GetPawn ister ve explicit Seen
API'sini çağırır. Sunucu current üyeliğe fallback yapmaz. Eski trade/clan chat
fixture çağrıları yedinci sıfır GUID argümanıyla güncellendi. Yeni Tick/timer
ve save alanı yok; reflection/RPC için iki uçta aynı DLL gerekir.

**Native doğrulama:** 73 = önceki46 +18 component +9 leave chat kontrolü.
Sıfır/bilinmeyen/başka kişi kimliği, fresh leave, aynı/different pair yeniden
kurulumu, aynı devam eden gruba rejoin, member ekleme/ayrılma, lider değişimi,
merge transferi, solo inviter mevcut grup, native possession/matching yeni
pawn kimliği, null/eski chat ve fresh Say/Enter kapsanır. Component retlerinde
9 actor Members/Leader/pending/inviteID/saatler/session/membership/envanter
snapshot'ı aynıdır. Controller 3 actor public party/pending/kimlik/envanter
snapshot'ı; private invite cooldown'ı ölçmez. Yeni chat formation helper'ı
pending'i native seed eder; nearest/delivery kanıtı değildir. Önceki8 chat
nearest davet/Say/OnChatCommitted kontrolleri de çalışır.

İlk yeni native koşu FAIL50/15:24:39.561 UTC verdi: test grubu [0,2,1] kurdu,
0 ayrılınca ilk survivor2 lider olurken assert1 bekliyordu. Yalnız fixture
beklentisi düzeltildi; üretim bug'ı sayılmadı. DLL Succeeded3,06 sn sonrası
PASS73/15:25:23.489 UTC, PS0. TEMP mmo_party_leave_positive_20261008.log ilk
FAIL50'yi, mmo_party_leave_positive_fixed_20261008.log doğru PASS'ı tutar.

**Derlenmiş negatifler:** Yalnız membership eşitliği guard'ı kaldırıldı;
DLL Succeeded3,02 sn, FAIL40 reason=other member leave identity accepted
15:26:03.584 UTC, UE0/PS1. Component byte exact restore. Yalnız chat pawn
kapısı kaldırıldı; DLL Succeeded4,23 sn, gerçek native possession/matching
new-pawn membership testi FAIL65/check66, 15:26:45.726 UTC, UE0/PS1.
TEMP mmo_party_leave_negative_membership_20261008.log ve
mmo_party_leave_negative_pawn_20261008.log. Controller byte exact restore;
son normal Editor DLL Succeeded4,35 sn, restored PASS73/15:27:21.628 UTC/PS0.
TEMP mmo_party_leave_restored_20261008.log. Negatif mutantlar final DLL'de yok.

**ECC review / ajan:** Lovelace Unreal Multiplayer Architect yerel TOML,
repo QA ve ECC review ile salt okunur tasarım/actual diff/caller incelemesi
yaptı. İlk shared session önerisinde same-party rejoin boşluğu buldu; personal
membership eklenerek kod yazılmadan giderildi. Mevcut EndPlay player-control
kapısı unpossess/destroy yolunda ayrı cleanup boşluğu olarak açık kaydedildi.
Son kod/test incelemesinde ek uygulanabilir bulgu yok; FAIL50 test beklentisini
düzeltme olarak teyit etti. Ajan dosya yazmadı, build/test çalıştırmadı; son
negatif sonuçları bağımsız doğrulamadı ve tam paket öncesi kapatıldı. Agency
paket kökeni doğrulanmadı. Parent tüm kod/runner/build/native denemeleri yürüttü.

**Sınırlar:** Same-frame fixtures gerçek RPC ownership/owner-only GUID teslimi,
Slate/input, gecikmiş paket, save/logout veya Server/Shipping kanıtı değildir.
GUID ve Members/Leader atomik değil; latest kimlik eski çizilmiş roster'a
bağlanmaz. 0/eski ret sonrası retry, eski görünüm + yeni kimlik UX, ek GUID
chat/replication maliyeti gerçek PIE/profiler'da açık. EndPlay gate ve kick
stale bağlamı çözülmedi. PLAYTEST_CHECKLIST'e somut maddeler eklenip açık bırakıldı.


**Son ECC verify paketi (8 Ekim):** RunAllChecks.ps1 -SkipBuild -FullCompile
37/37 GECTI, exit0; normal DLL ayrıca Succeeded4,35 sn. 18 PCH'siz /W4 dosya
warning C0; ValidateData/StaticLint temiz (bilinen Lv33+ skill boşluğu ve
InitGameState whitelist uyarıları sürer). Parti73 PASS15:33:14.808 UTC;
klan43/takas218, death14/3first+5death/parser576/576 ve bütün mevcut regresyonlar
geçti. JSON schema2/negative111; store400 loaded400,
invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1,
roundtrip equal1. Bu sayı oyuncu/PIE/Lv1–30 tamamlanma sayısı değildir.
Summer52/Winter33 doğum, iki smoke exit0/error0; NetSmoke1join/1welcome,
server/client error0. Önceki smoke sayılarından fark, bu koşunun log sayımıdır;
yeni spawner veya içerik eklenmedi. Net logları mmo_net_server_183709.log ve
mmo_net_client_183709.log; tam paket TEMP mmo_all_party_leave_final_20261008.log.
Öncesi/sonrası147 Source/Scripts SHA256 listesi aynı, differences0;
TEMP mmo_party_leave_final_hashes_20261008.txt. Paket boyunca kaynak/runner
ve agent yazımı yok; bitimde UE/UBT süreci yok. Son doc düzeni ayrı tutuldu.

| Kontrol | Sonuç | Sınır |
| --- | --- | --- |
| UBT/UHT + 18 PCH'siz C++ | Geçti | Editor Development; Server/Shipping değil |
| Native party + iki derlenmiş negatif | Geçti73; negatifler FAIL40/65 | Restore edildi; same-frame intent, canlı paket değil |
| Tam mevcut paket | Geçti37/37 | Bağlantı smoke'u tüm gameplay replication'ını kanıtlamaz |
| ECC + salt okunur multiplayer/QA inceleme | Kapsamda ek engel yok | EndPlay gate/kick/atomic UX riskleri ayrı açık |
| Gerçek host + uzak istemci PIE | Yapılmadı | Checklist işaretlenmedi; coverage yüzdesi üretilmedi |


## 63. PlayerState kaybından bağımsız sunucu parti EndPlay cleanup — 2026-10-08 (O26)

**ECC plan:** Başlangıç HEAD967800c, tracked ağaç temiz; aynı checkout'taki ECC
kurulum sohbeti idle, önceki agent kapalı. Yarım iş yok. §62'nin açık party
EndPlay gate'i için önce native engine yaşam döngüsüyle hata kanıtı, ardından
iki C++ dosyası + mevcut runner'da sınırlı değişiklik. Kriterler: gerçek
unpossess/destroy ikilisinde survivor solo; attached leader/member destruction
sonrası simetrik survivor roster/leader ve sürekli üyelik kimlikleri korunur;
client cleanup RPC üretmez. Save/friends kimliği, kick ve HUD atomik görünümü
bu kapsamda genişletilmez. ECC plan/review/verify ve ue-build kullanıldı.

**Baseline:** Eski Character EndPlay authority + IsPlayerControlled gate'ine
party cleanup'ı da bağlamıştı. Yerel UE5.7 Pawn.cpp: IsPlayerControlled PlayerState
ister; UnPossessed temizler; Destroyed detach'i actor RouteEndPlay'den önce
çalıştırır. Native PC gerçek pawn possess eder, grup valid olur; UnPossess sonrası
PlayerState null, controller pawn null, mevcut grup korunur. Pawn Destroy true
döndükten hemen sonra canlı diğer pawn hâlâ partilidir: FAIL75 reason=destroy
after unpossess retained dead party member, 15:44:38.825 UTC, UE0/PS1.
Önce normal DLL Succeeded10,43 sn. TEMP mmo_party_lifecycle_baseline_20261008.log.
Bu fixture doğrudan EndPlay çağırmaz; gerçek motor destruction yolunu yürütür.
Eski fixture'lar önce RequestLeave yaptıkları için bu hatayı ölçmüyordu.

**Fix:** Character EndPlay, save/friends gate'inden sonra ve Super'den önce ayrı
HasAuthority && PartyComponent ile RequestLeave çağırır. UObject component bu
noktada henüz Super tarafından EndPlay edilmemiştir. Component owner karakterdir;
actor controller/connection kaybı authority dispatch'ini engellemez. Yerel
implementation §62 valid/current membership guard'ını ve mevcut grup düzenlemesini
kullanır. Client EndPlay authority false olduğu için leave RPC göndermez.
Player-control save/friends gate'i genişletilmedi; PlayerState yokken Default
save kimliğine yazmak gibi bir fallback eklenmedi. Yeni property/RPC/timer/Tick
ve save alanı yok. Salt UnPossess, EndPlay olmadan partiyi bozmaz.

**Native83:** Eski73 +10 lifecycle. Detached pair (gerçek PC/player state),
attached leader destruction üçlü grup, ordinary member possess/unpossess/Destroy,
new survivor leader/session/personal ID, engine RouteEndPlay(LevelTransition)
ve sonraki Destroy/solo envanter kontrolü. Yok edilen actor/component yeniden
okunmaz; canlı survivors destruction'dan hemen sonra incelenir. Final fixture
teardown IsValid ile ölü actor'ları atlar, native PC'leri unpossess/destroy eder.
Tüm test aynı-frame, deferred AutoLoad .6sn world timer'ı çalışmadan biter;
Tick kapatmak world timer'ını kapatır iddiası yok. Save/HTTP/remote logout
kanıtı değildir. RouteEndPlay gerçek ServerTravel değildir. Sonraki Destroy
begun-play guard nedeniyle ikinci Character EndPlay çağırmaz; ikinci cleanup
çağrısı kanıtı olarak sayılmaz. Son assertion etiketi bunu açıkça belirtir.

İlk fix DLL Succeeded8,18 sn; PASS83/15:46:12.827 UTC, PS0.
TEMP mmo_party_lifecycle_positive_20261008.log. Descartes logları salt okunur
inceledi: baseline/fixed marker ve failure-pattern0 teyit etti; process exit
kodlarını bağımsız doğrulamadı. Parent exit kodlarını kaydetti.

**Derlenmiş negatif:** Yeni cleanup kapısına tekrar IsPlayerControlled eklendi;
DLL Succeeded4,93 sn, FAIL75/15:47:46.674 UTC, UE0/PS1. TEMP
mmo_party_lifecycle_negative_gate_20261008.log. Character.cpp byte exact restore,
son normal DLL Succeeded4,91 sn; PASS83/15:48:17.980 UTC, PS0. TEMP
mmo_party_lifecycle_restored_20261008.log. Negatif mutant final kaynak/DLL'de yok.

**ECC review / ajan:** Descartes yerel Unreal Multiplayer Architect TOML, repo
QA ve ECC review ile motor destruction sırası, component lifetime, actual diff
ve fixture/runner83 eşleşmesini salt okunur inceledi. Son diff'te engel bulmadı.
Transition route + Destroy tekrar EndPlay değildir sınırını belirtti; test etiketi
netleştirildi. Ajan dosya/build/test yazmadı, tam paket öncesi kapatıldı; Agency
paket kökeni doğrulanmadı. Parent bütün uygulama/build/test'i yürüttü. İlgili
code/caller/super sırası kontrol edildi. Gerçek remote ownership/delivery/save,
client HUD/XP ve kalıcı logout, Server/Shipping açık; PLAYTEST maddeleri eklenip
hiçbiri geçti işaretlenmedi.


**Son ECC verify:** RunAllChecks.ps1 -SkipBuild -FullCompile 37/37 GECTI,
exit0. Normal Editor DLL ayrıca Succeeded4,91 sn; 18 PCH'siz /W4 dosya warning C0.
Veri/lint temiz; bilinen Lv33+ beceri boşluğu/InitGameState whitelist uyarıları
sürer. Parti83 PASS15:53:44.551 UTC; klan43/takas218, ölüm14/3first+5death ve
parser576/576 dahil bütün mevcut regresyonlar geçti. JSON schema2/negative111;
store406 loaded406, invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6,
writeorder1, roundtrip equal1. Yuva sayısı oyuncu/PIE/Lv1–30 koşu sayısı değildir.
Summer52/Winter31 doğum, smoke exit0/error0; NetSmoke1join/1welcome ve iki tarafta
error0. Bu koşunun spawn log sayımı içerik değişikliği değildir. TEMP tam paket
mmo_all_party_lifecycle_final_20261008.log; ağ logları mmo_net_server_185737.log
ve mmo_net_client_185737.log. Öncesi/sonrası147 Source/Scripts SHA256 aynı,
differences0; mmo_party_lifecycle_final_hashes_20261008.txt. Paket boyunca kod/
runner/ajan yazımı yok; bitimde UE/UBT süreci yok. Son doküman düzeni ayrı.

| Kontrol | Sonuç | Sınır |
| --- | --- | --- |
| Normal UBT + 18 PCH'siz dosya | Geçti | Editor Development; gerçek Server/Shipping açık |
| Native83 + derlenmiş gate negatifi | Geçti83; mutant FAIL75 | Gerçek native engine lifecycle, remote logout değil |
| Tam mevcut regresyon paketi | Geçti37/37 | NetSmoke tüm gameplay replication'ını sınamaz |
| ECC + salt okunur Unreal/QA inceleme | Kapsamda engel yok | Save/presence lifecycle ve kick ayrı açık |
| Gerçek host + uzak istemci PIE | Yapılmadı | Checklist açık; oyuncu koşusu/coverage yüzdesi yok |


## 64. Possession bitmeden son karakter kaydi; detached persistence kapisi (8 Ekim / O27)

**ECC plan:** O26 parti cleanup sonrasi kalan kayit boslugu ele alindi. Kabul
olcutu: authority oyuncu pawn'indaki son altin snapshot'i PlayerState kaybolmadan
kendi yuvasina yazilir; ilk load tamamlanmadiysa/committed travel ise yazilmaz;
canli unpossessed pawn Default'a kaydetmez veya timer ile Default yuklemez;
non-destroy EndPlay tek yazar; K-1 hedef yuva korumasi devam eder.
Kapsam Character.cpp/.h, TestSaveLifecycle.ps1, RunAllChecks ve bu mevcut belgeler.
Blueprint, RPC, replicated property, save semasi ve hesap kimligi degismedi.

**Once hata:** Eski b9ae6e0 uretim kodu ile yeni native fixture normal DLL
Succeeded11,62 sn. Gercek possessed pawn Destroy sonrasinda kendi yeni yuvasi
okunamadi: FAIL4 / 16:07:49.456 UTC, UEexit0 / PSexit1. TEMP
mmo_save_lifecycle_baseline_20261008.log. UnPossessed PlayerState'i siler;
Pawn Destroyed bunu Character EndPlay'den once yapar. Ortak player-control
gate EndPlay save'ini atliyordu. Bu fixture tek karede engine yolunu yurutur.

**Sinirli uygulama:** Character UnPossessed authority/player/initial-load/
!travel-committed/!EndPlay-started kapisiyla SaveCharacterForTravel(true)'yi
Super'den ONCE cagirir. Slot hala canli PlayerState'ten secilir; eski ActiveSlot
veya Default fallback eklenmez. K-1 ve tracker ayni save yolundadir. Ret/hata
Warning ile kaydedilir; basari varsayilmaz. Tum authority detach'lerinde load,
save ve saved-position retry timer'lari kapanir, bInitialLoadComplete false olur.
Yeni possession mevcut 0,6 sn yukleme akisini kullanir. EndPlay giris bayragi,
non-destroy EndPlay'nin mevcut snapshot'ini Super icindeki UnPossessed'de ikinci
kez yazmayi onler. Yeni timer/Tick yok; bir local lifecycle bayragi ve override var.
Friends presence kapisi bu iste degismedi. Kalici hesap kimligi cozumu degildir.

**Native26:** Yalniz unattended/NullRHI authority dunyasinda 7 native PC/pawn;
GUID ile olusturulan 7 yeni slot, collision varsa fixture baslamaz. Yalniz bu
slotlar yazilir ve DeleteGameInSlot ile temizlenir; mevcut Default salt okunup
alan alan karsilastirilir. Possessed Destroy son altin123, explicit UnPossess
11+29=40, detached Save ret/3 timer handle kapali, unloaded Destroy yazmaz,
committed snapshot31 ustune99 eklenip destruction'da geri yazilmaz,
RouteEndPlay(LevelTransition) altin45 ve tracker ile tek yazim, native controller
Destroy altin56, mevcut baska disposable hedefe ad degisimi K-1 reddi ve iki
slot korunmasi. Yok edilmis pawn tekrar okunmaz; fixture cleanup'ta once load
kapisi kapatilir. Presence, gercek client logout, timer suresinin dolmasi,
network ownership ve ServerTravel degildir; native PC Player=null teardown'dur.
Disk arizasi/HTTP/depoda siralama ve gercek corrupted save bu fixture'da yok.

Ilk fix DLL Succeeded9,72 sn; PASS26 16:08:53.563 UTC, PS0; TEMP
mmo_save_lifecycle_positive_20261008.log. Derlenmis reset-negatifi DLL
Succeeded5,16 sn -> FAIL9 16:09:49.675 UTC, UE0/PS1 (detached save enabled).
Derlenmis EndPlay-gate-negatifi Succeeded5,14 sn -> FAIL17 16:10:17.320 UTC,
UE0/PS1 (transition iki yazim). TEMP mmo_save_lifecycle_negative_reset_20261008.log
ve mmo_save_lifecycle_negative_duplicate_20261008.log. Byte exact restore sonrasi
ara normal DLL Succeeded4,99 sn, PASS26 16:11:13.464 UTC / PS0; TEMP
mmo_save_lifecycle_restored_20261008.log. Negatifler final kaynakta yok.

**ECC review / ajan:** Anscombe yerel user-level Unreal Multiplayer Architect,
repo QA ve ECC review ile motor Pawn/Controller/PlayerController/World sirasini
ve actual diff'i salt okunur inceledi. Rol paketinin Agency kokeni dogrulanmadi.
Uretim kapsaminda engel bulmadi; reset + force save gerekliligini teyit etti.
P3 fixture cleanup ilk silme hatasinda duruyordu: kalan gecici yuvalarin da
silinmesini deneyecek sekilde duzeltildi. Navigation retry assertion'i bos
handle'i kontrol ediyordu: gercek callback timer'i fixture'da sentetik30 sn ile
arm edildi ve uc timer'in aktif oldugu onkosulu eklendi. Bu gercek navmesh
pending akisi degildir. Ajan build/test/dosya yazmadi, full paket oncesi kapatildi.
Parent kod, normal DLL, negatifler, hash ve full paketi yurutur. BeginPlay lifecycle
bayragini sifirlar; custom seamless-retained pawn kimligi bu is kapsaminda degil.

Timer cancellation negatifi (son fixture) DLL Succeeded5,04 sn -> FAIL10
16:14:17.707 UTC / UE0/PS1; TEMP mmo_save_lifecycle_negative_timer_20261008.log.
Final byte-exact restore ve normal DLL Succeeded5,11 sn, exit0. TEMP
mmo_save_lifecycle_final_restore_20261008.cpp final-source yedegi (repo disinda).
Normal cleanup butun yuvalari dener; timeout/process kill yuva birakabilir.
Tek-yazim sayaci yalniz LevelTransition route'unda olculdu; diger yollardaki
ayni verili duplicate yazim sayisi bu fixture'da olculmedi.

**Son ECC verify:** RunAllChecks.ps1 -SkipBuild -FullCompile 38/38 GECTI,
exit0. Normal Editor DLL ayrica Succeeded5,11 sn; 18 PCH'siz /W4 dosya warning C0.
Native save26 PASS16:18:19.638 UTC (son fixture, arm edilmis retry timer'i ve
cleanup duzeltmesi dahil). Parti83 PASS16:19:27.665, klan43 PASS16:19:45.659,
takas218, olum14/3first+5death ve parser576/576 dahil tum eski regresyonlar gecti.
JSON schema2/negative111. Store412 loaded412, invalid/loadFailed/existsMismatch/
jsonMismatch0, maxLevel6, writeorder1, roundtrip equal1. Bu yuva sayisi oyuncu/
PIE/Lv1-30 kosu sayisi degildir. Summer51/Winter29 mob dogumu; iki smoke exit0/
error0. Dogum sayimi yeni icerik anlamina gelmez. NetSmoke1join/1welcome;
server/client error0. TEMP mmo_all_save_lifecycle_final_20261008.log; ag loglari
mmo_net_server_192317.log / mmo_net_client_192317.log. Full oncesi/sonrasi148
Source/Scripts SHA256 differences0; mmo_save_lifecycle_final_hashes_20261008.txt.
Full boyunca kod/runner/ajan yazimi yok. Sonunda engine/UBT process0 ve
MMOSave_ZLife*.sav gecici yuva0; mevcut Default kaldi, fixture alan karsilastirma
kontrolu gecti. Belgeler full kaynak freeze'i disinda duzenlendi. Bilinen Lv33+
beceri bosluklari/InitGameState whitelist uyari sinirlari surer.

| Kontrol | Sonuc | Sinir |
| --- | --- | --- |
| Normal UBT + 18 PCH'siz dosya | Gecti | Editor Development; Server/Shipping acik |
| Native26 + baseline/3 derlenmis negatif | PASS26; FAIL4/9/17/10 | Local synchronous store; elapsed nav/remote logout degil |
| Tam mevcut regresyon paketi | Gecti38/38 | NetSmoke baglanti kaniti; tum gameplay replication degil |
| ECC + salt okunur Unreal/QA inceleme | Kapsamda engel yok; P3/fixture eksigi giderildi | Ajan build/test yazmadi; parent final duzeltmeleri dogruladi |
| Gercek host + uzak istemci PIE | Yapilmadi | Save failure/reconnect/nav/seamless/presence checklist acik |


## 65 · Arkadas offline presence kimlik silinmeden once (2026-10-08; O28)

ECC plan/review/verify + mmo-feature/ue-build ile O27'nin acik presence yolu
secildi. Kayit kodu korunur. 16099a3 uzerinde possessed native pawn Destroy,
PlayerState detach sonrasi EndPlay player gate'ine ulasamadi: baseline FAIL6
16:34:56.771 UTC / UE0/PS1 (TEMP mmo_friends_presence_baseline_20261008.log).

Authority UnPossessed, Super oncesi !EndPlay-started + player-controlled +
!committed-travel + Friends iken offline duyurur. Initial-load/save basarisina
baglanmaz: AutoLoadDeferred zaten pending load icin online duyurabilir. Eski
EndPlay yolu, teardown guard, karsilikli riza, case-insensitive ad anahtari ve
300 sn duplicate suppression korunur. Yeni RPC/replicated alan/timer/Tick yok;
kayit schema'si degismedi. Salt UnPossess parti leave baslatmaz.

Native24, GUID adli 10 gercek native PC/pawn ve ClientSetHUD -> ClientReceiveChat
-> HUD buffer yoluyla kontrol eder: possessed Destroy, explicit UnPossess ve
post-detach Destroy, ayni pawn repossess, case-varied yeni pawn reconnect,
online/offline tekilligi, one-way/self/unrelated gizliligi, kanal2 rengi,
never-online sessizligi, committed replacement, LevelTransition route,
sentetik abort bayragi ve teardown guard; uncontrolled NPC destruction guvenli.
World tick/load yok, fixture save yazmaz; yalniz kendi GUID cache anahtarlarini
normal cleanup'ta siler. Cache onkosulu >480 iken testi reddeder; existing key
collision reddi mevcut. Shipping'de test ve salt-okunur HUD getter derlenmez.
NPC kontrolu player gate'ini bagimsiz ispatlamaz (NPC adi/cache yoktur).

Iki fixture hatasi duzeltildi: ilk case-varied reconnect FAIL8, FString ==
case-insensitive sayimiydi; .Equals(..., CaseSensitive) gercek exact text sayar.
NPC FAIL21, origin'deki collision nedeniyle spawn NULL idi; AlwaysSpawn/Z9000
kullanildi. Bunlar yeni production bug olarak sayilmaz. Son fixture DLL
Succeeded2,91 sn -> PASS24 20:06:43.956 UTC / UE0/PS0.

Derlenmis load-gate negatifi Succeeded5,09 sn -> FAIL6 20:07:11.155 UTC;
EndPlay-gate negatifi Succeeded5,00 sn -> FAIL18 20:07:40.833 UTC. Iki negatif
UE0/PS1; TEMP mmo_friends_presence_negative_load_20261008.log ve
mmo_friends_presence_negative_endplay_20261008.log. Final byte-exact restore,
normal DLL Succeeded5,10 sn exit0 -> PASS24 20:08:02.186 UTC / UE0/PS0;
TEMP mmo_friends_presence_final_20261008.log. Negatif DLL finalde kalmadi.

Salt-okunur Unreal/QA reviewer Galileo actual diff'i inceledi; production engeli
bulmadi, fixture comparator/NPC duzeltmeleri ve kanit sinirlari kayda alindi.
Ajan dosya/build/test/alt-ajan baslatmadi; final restore/build/full parent'ta.
Tam paket oncesi kapatildi. Agency plugin kokeni dogrulanmadi, rol incelemesi
Unreal/QA/ECC olarak kullanildi.

Gercek remote RPC/HUD rendering, actual logout/travel/abort/teardown, save failure
sonrasi presence, >300 sn expiry ve multiple worlds PIE acik. Cache hala process
static/ad tabanli; fresh512 uzeri kayitlar hard cap degil, yalniz >=300 sn olanlar
prune edilir. Committed travel sonrasi gercek disconnect offline'i baskilayip
hizli reconnect online'ini 300 sn bastirabilir; bu pencere duzeltilmedi.


**Final ECC verify:** RunAllChecks.ps1 -SkipBuild -FullCompile 39/39 GECTI,
exit0. Final restore normal DLL Succeeded5,10 sn ayrica gecti. 19 PCH'siz /W4
CPP dosya warning C0 (Friends yeni listeye eklendi). Native presence24
PASS20:12:26.180 UTC; save26 PASS20:12:16.211, parti83 PASS20:13:44.154,
klan43 PASS20:14:02.154, takas218, olum14/3first+5death ve parser576/576 dahil
eski kontroller gecti. JSON schema2/negative111. Store419 loaded419, invalid/
loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1, roundtrip equal1.
Bu yuva sayisi oyuncu/Lv1-30 kosu kaniti degildir. Summer48/Winter30 smoke,
exit0/error0; rastgele dogum sayimi yeni icerik degildir. NetSmoke1join/1welcome,
server/client error0. TEMP mmo_all_presence_final_20261008.log ve
mmo_net_server_231716.log / mmo_net_client_231716.log. Full oncesi/sonrasi149
Source/Scripts SHA256 differences0; TEMP mmo_presence_final_hashes_20261008.txt.
Kaynak/runner/ajan yazimi full boyunca yok; belgeler freeze disinda duzenlendi.
Full sonunda engine/UBT process0 ve MMOSave_ZPres*.sav0. Gercek PIE acik;
Lv33+ beceri bosluklari/InitGameState whitelist uyari sinirlari surer.

| Kontrol | Sonuc | Sinir |
| --- | --- | --- |
| Final normal DLL + 19 PCH'siz dosya | Gecti; warning C0 | Editor Development; Server/Shipping acik |
| Native24 + baseline/2 derlenmis negatif | PASS24; FAIL6/6/18 | Standalone HUD buffer; rendering/remote RPC degil |
| Tam mevcut regresyon paketi | Gecti39/39 | NetSmoke baglanti; tum gameplay replication degil |
| ECC + readonly Unreal/QA | Kapsamda blocker yok; fixture hatalari giderildi | Parent final restore/DLL/full ile dogruladi |
| Gercek host/uzak istemci PIE | Yapilmadi | Travel/abort/teardown/300sn/multiworld/persistence acik |


## 66 · DoT kaynak omru ve anonim hasara dusme (2026-10-08; D2/O29)

ECC plan/review/verify + mmo-feature/ue-build ile D2 kaynak incelemesi native
fixture'a tasindi. ad5fa0c uzerinde normal DLL Succeeded11,78 sn, orijinal
ProcessEffects: FAIL10 20:27:08.046 UTC / UE0/PS1. Source pawn Destroy sonrasi
Poison tick'i Instigator=null ile ApplyDamage yapip PvP-off victim canini azaltti.
TEMP mmo_dot_source_baseline_20261008.log. Bu fixture dogrudan sunucu ApplyEffect
API'sini kurar; mevcut offensive skill hedefleri mob-only oldugu icin normal
oyuncu PvP zehri/remote exploit erisimi kaniti olarak sayilmaz. Ayni orphan kaynak
moba anonim fatal hasar ve creditsiz oldurme de yapabiliyordu; sinirli ret eklendi.

FMMOActiveEffect'te server-only/plain bPlayerSource, kaynak AMMOCharacter ama
AMMOMob degilken set edilir; controller varligina bagli degildir. Insertion ve
same-ID refresh instigator + classification + hub cache'i gunceller. Pending
listesine raw actor yerine weak reference ve classification kopyalanir; HER
hasar dispatch oncesi weak yeniden cozulur. Ilk hasar callback'i source'u yok
ederse sonraki pending tick anonim hale gelmez. Player kaynakli PeriodicDamage
+ kaynak gecersiz ise sadece hasar atlanir. Etki sure/isareti normal sona erer;
client HUD'in orphansource zehir simgesini gormesi sifir damage garantisinden
ayri bir oynanis kontroludur. Yeni damage yolu/yerel stats shortcut yok:
canli kaynak ApplyDamage -> Character TakeDamage PvP/party/clan/hub kapilarini
kullanir. Mob zehri cached hub/outside davranisi, environment/null kaynak ve
periodic healing korunur. RPC/save schema/replicated alan/yeni Tick/timer yok.

Native33 gerçek controlled source/victim, actual Destroy/UnPossess, ProcessEffects
ve TakeDamage kullanir: canli PvP/party/clan/hub ret; permitted player8 damage;
controllerless live source ret; destroyed player source PvP ve mob hasari/combat
stamp/LastAppliedDamage/olum yok; queued catch-up ve expiry; player->mob/null,
mob->player/null same-ID refresh, weak source/classification/hub/timer reset;
destroyed mob cached hub/outside poison, null environment, destroyed healer
Regen, self poison, spawn protection ve fatal catch-up ClearAllEffects.
Defense0/HP100 senaryosunda Poison8 ile exact Health sayar. Artificial ProcessEffects
DeltaTime kullanilir; gercek elapsed scheduler/nav/PIE degildir. World tick/load
calismaz, GUID isimli fixture karakterleri save yazmaz. GM PvP ve fixture aktorler
normal cleanup'ta geri alinir; timeout disposable process'i keser.

Editor-only private UFUNCTION callback iki-float OnHealthChanged'e dogru signature
ile baglanir. Ilk gercek kabul edilmis tick bu native health delegate'inde source'u
Destroy eder; ikinci pending tick skip, toplam8 damage. Production ApplyDamage'e
test hook eklenmedi. UFUNCTION/helper WITH_EDITOR, console/self-test non-Shipping
ve standalone unattended NullRHI guard'li; Shipping/Server hedefi ayrica derlenmedi.

Initial fix normal DLL Succeeded11,15 sn -> PASS33 20:29:57.193 UTC / UE0/PS0.
Derlenmis raw-pending negatifi Succeeded3,61 sn -> FAIL32 20:30:42.896 UTC;
refresh classification negatif Succeeded3,57 sn -> FAIL15 20:31:21.484 UTC.
Negatifler UE0/PS1, TEMP mmo_dot_source_negative_raw_20261008.log ve
mmo_dot_source_negative_refresh_20261008.log. Byte-exact restore DLL
Succeeded3,60 sn -> PASS33 20:32:33.106. Ardindan direct PlayerState include
NoPCH hijyeni icin eklendi; final DLL Succeeded3,57 sn -> PASS33
20:34:01.974 / UE0/PS0. TEMP mmo_dot_source_final_20261008.log ve
mmo_dot_source_final_restore_20261008.cpp final-source yedegi; mutant DLL kalmadi.

Open: gercek remote effect/HUD replication, elapsed tick/expiry, player skill
PvP hedef erisimi, source-controller rebinding/kimlik, yuzlerce actor network
profiler, effect clear/refresh callback sonrasi zaten toplanmis pending'in iptali
ve DeltaTime duration overshoot mevcut semantik. Bunlar bu fix'te degistirilmedi.


ECC readonly Unreal/multiplayer security/QA reviewer Arendt actual final diff,
fixture ve backup'i inceledi; production blocker yok. Parent'in include DLL
Succeeded3,57 sn ve latest PASS33 kaniti mevcut. Ajan build/test/dosya yazmadi;
full freeze baslarken salt-okunur inceleme surdu, full bitmeden kapatildi.
Kaynak ve runner mutation'i full boyunca yapilmadi. Agency plugin kokeni
kurulu/discoverable dosyalarda bulunmadi; rol incelemesi diye kaydedildi.


**Final ECC verify:** RunAllChecks.ps1 -SkipBuild -FullCompile 40/40 GECTI,
exit0. Final include/restore normal DLL Succeeded3,57 sn ayrica dogrulandi;
19 PCH'siz /W4 CPP dosya warning C0. Native DoT33 PASS20:41:23.178 UTC;
save26 PASS20:38:16.273, presence24 PASS20:38:23.516, parti83 PASS20:39:37.188,
klan43 PASS20:39:53.304, takas218 PASS20:42:44.450, death14/3first+5death ve
parser576/576 dahil eski regresyonlar gecti. JSON schema2/negative111. Store426
loaded426, invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6,
writeorder1, roundtrip equal1; bunlar oyuncu/Lv1-30 kosu sayisi degildir.
Summer51/Winter27 mob dogumu, exit0/error0; iki random smoke yeni icerik degil.
NetSmoke1join/1welcome ve server/client error0; TEMP mmo_net_server_234322.log /
mmo_net_client_234322.log. Full log mmo_all_dot_source_final_20261008.log (TEMP).
Full oncesi/sonrasi150 Source/Scripts SHA256 differences0; TEMP
mmo_dot_source_final_hashes_20261008.txt. Full boyunca source/runner yazimi yok;
agent readonly kaldi, full bitmeden kapatildi. Belgeler freeze disinda degisti.
Engine/UBT process0, MMOSave_ZDot*.sav0. Gercek PIE ve Server/Shipping acik;
bilinen Lv33+ beceri bosluklari/InitGameState whitelist uyari sinirlari surer.

| Kontrol | Sonuc | Sinir |
| --- | --- | --- |
| Final normal DLL + 19 PCH'siz dosya | Gecti; warning C0 | Editor Development; Server/Shipping acik |
| Native33 + baseline/2 derlenmis negatif | PASS33; FAIL10/32/15 | Actual authority dispatch/delegate; elapsed/remote degil |
| Mevcut tum regresyon paketi | Gecti40/40 | NetSmoke baglanti, tum replication/oynanis degil |
| ECC + readonly Unreal/security/QA | Kapsamda blocker yok | Parent final kaynak/DLL/full dogruladi |
| Gercek host/uzak istemci PIE | Yapilmadi | Effect HUD/scheduling/source rebinding/profiler acik |


## 67. Klan komutlarini gonderen pawn'a baglama (2026-10-08, O30)

**ECC plan/review:** Say ve OnChatCommitted zaten gonderim anindaki pawn'i
ServerSendChat'e tasiyordu; yalniz /klan kabul bunu denetliyordu. Gercek native
possession switch sonrasi eski /klan ayril replacement pawn'in farkli klanini
bosaltti: orijinal dispatch + yeni fixture DLL Succeeded4,42 sn, FAIL45
(next check46) 20:52:39.170 UTC. TEMP mmo_clan_commands_baseline_20261008.log.
Bu orta davranis/veri butunlugu hatasidir; sahte girdiyle lider yetkisini asma
iddiasi degildir. Gecikmis eski /klan at/davet/kur/liste ve /g de yeni pawn'a
route ediliyordu; kabul zaten ID ve pawn korumasina sahipti.

ServerSendChat'in /klan ve /g dallarinin basinda, mevcut ortak throttle sonrasi
IsValid(SeenCommandPawn) && SeenCommandPawn == GetPawn() gerekir. Ret sessizdir;
yeni pawn'da ayni sayisal invite ID olmasi yetmez. Clan component otorite,
liderlik/olum/cooldown/ad kurallari ve CmdAcceptSeen ID denetimi korunur.
Yeni RPC parametresi, replication/property, save schema, Tick veya timer yok.
Sunucu halen kendi mevcut pawn'ini kullanir; istemcinin referansi sadece eslesme
icin okunur. Possession bilgisi istemcide gec guncellenirse fresh baglamla tekrar
gondermek gerekir. Ayni pawn'da klan ayril/rejoin, hedef kick-rejoin, kalici klan
kimligi ve atomik roster/bildirim gorunumu bu scope'ta cozulmedi.

Mevcut klan self-test 43 ->73; controller chat11 ->41. Yeni30 kontrol:
4nativePC/pawn + ClientSetHUD/ClientReceiveChat standalone buffer; actual
UnPossess/Possess farkli klan/liderlik; eski/null/baska pawn ×6command ret,
stale/null kur ret; clan/inventory/leader/inviteID/HUD-delivery korunmasi;
fresh Say /G yalniz ayni klan alicilari, committed liste yalniz owner;
fresh davet mevcut3sn cooldown cevabi; nonleader/dead kick ret, leader kick,
fresh replacement leave, invalidcreate normal feedback. Yeni fresh valid kur
ve real cooldown bekleme yok; invite issuance onceki43 kapsaminda. Butun
fixture ayni frame'de: world tick/load timer/save calismaz, GUID clan/name,
cleanup once clan state sonra controller/pawn/HUD; gercek karakter kaydi yok.
Freshinvite beklentisinin ilk FAIL67'si ayni-frame cooldown'u ihmal eden test
beklentisiydi, production bug sayilmadi; private testhook/time advance eklenmedi.

Production iki gate ile DLL Succeeded4,28 sn ->PASS73 20:53:44.949 UTC.
/g kapisi kaldirilan derlenmis mutant DLL Succeeded4,24 sn ->FAIL49
(next50) 20:54:14.201 / UE0/runner1: gercek stale HUD mesaj teslimi yakalandi.
TEMP mmo_clan_commands_negative_g_20261008.log. Byte-exact Copy-Item restore
eski timestamp'i koruyunca UBT up-to-date dedi; test tekrarFAIL49 ile mutantDLL'i
yakaladi. LastWriteTimeUtc yenilenip gercek compile/link Succeeded4,23 sn;
finalPASS73 /chat41 20:55:01.333 /UE0/runner0. Kaynak yedegiyle bytes ayni;
mutantDLL ile commit yok. TEMP mmo_clan_commands_final_restore_20261008.cpp
ve mmo_clan_commands_final_20261008.log. Tek degisen CPP PCH'siz /W4
-NoPCH -NoLink -SingleFile ile Succeeded12,54 sn, warning C0;
TEMP mmo_clan_commands_nopch_20261008.log. Onceki19CPP full kontrolunu
bu degisiklige yeni full19 kaniti olarak atfetmiyoruz.

Readonly Unreal/multiplayer/security/QA Peirce final diff, restore byte hash,
negative /g ve final73/41 logunu inceledi: blocker yok. ECC review/verify
uygulandi; ajan dosya yazmadi/build/test yapmadi ve paket surerken kapatildi.
Agency plugin kokeni bulunmadigindan plugin kullanildi diye sunulmadi.
Gercek uzak RPC, Slate input/Canvas, owner-only teslim/respawn/travel/packet
latency ve Server/Shipping oynanmadi; PLAYTEST_CHECKLIST O30 maddeleri acik.


**Final ECC verify (9 Ekim yerel / 8 Ekim UTC):** Gercek restore DLL sonrasi
RunAllChecks.ps1 -SkipBuild 39/39 GECTI, exit0. Tek CPP degistigi icin eski19
NoPCH dosyayi yeniden calistirmadik; degisen MMOPlayerController.cpp ayrica
NoPCH/NoLink/SingleFile Succeeded12,54 sn, responsefile /W4 ve warning C0.
Klan73 PASS20:58:03.460 UTC; save26 20:56:31.545, presence24 20:56:43.344,
parti83 20:57:48.403, DoT33 20:59:23.074, takas218 21:00:44.273; parser576/576,
JSON schema2/negative111 ve mevcut regresyonlar gecti. Store433 loaded433,
invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1,
roundtrip equal1. Bunlar gercek oyuncu/Lv1-30 kosusu degildir. Summer47/Winter29
random smoke mob dogumu, exit0/error0. NetSmoke1join/1welcome iki taraftaerror0;
TEMP mmo_net_server_000117.log / mmo_net_client_000117.log. Paket logu TEMP
mmo_all_clan_commands_final_20261008.log. Paket oncesi/sonrasi150 Source/Scripts
SHA256 differences0; liste mmo_clan_commands_final_hashes_20261008.txt.
ControllerCPP restore SHA256 F7353620D07E69B23BC61F64D9ECD3A6F64F79DADA66312069B73605828274AC.
Kaynak/runner freeze boyunca yazilmadi. Readonly ajan kapali; diger ayni checkout
ECC chat notLoaded, aktif baska yazici yok. Engine/UBT process0,
MMOSave_ZClanCtx*.sav0. StaticLint InitGameState whitelist / Lv33+ beceri uyari
sinirlari devam eder. Gercek remote/PIE/Server/Shipping acik.

| Kontrol | Sonuc | Sinir |
| --- | --- | --- |
| Gercek compile/link ve degisen CPP NoPCH/W4 | Gecti, warning C0 | Editor Development; Server/Shipping acik |
| Native klan73/chat41 + baseline/g mutant | PASS73; FAIL45/49 | Actual possession/nativeHUD, remote/rendering degil |
| RunAllChecks -SkipBuild | Gecti39/39 | FullCompile19 yeniden calismadi; NetSmoke sadece baglanti |
| ECC readonly Unreal/security/QA | Final diff'te blocker yok | Parent build/fixture/paket/hash dogruladi |
| Gercek host/remote PIE | Yapilmadi | Respawn/travel/teslim sirasi/aynipawn rejoin acik |


## 68. Arkadas komutlarinin gonderim pawn baglami (2026-10-09 yerel, O31)

**ECC plan/review:** ServerSendChat /arkadas dali Me/GetPawn ile guncel Friends
component'i seciyordu; mevcut SeenCommandPawn kontrol edilmiyordu. Actual
native PC1 Pawn1->Pawn3 possession switch ve iki farkli listeyle eski
/arkadas sil komutu replacementPawn3 ve karsiPawn2 listesindeki iliskiyi
kaldirdi. Orijinaldispatch/yeni fixture DLL Succeeded4,36 sn ->FAIL2
21:06:49.454 UTC /UE0/runner1. TEMP mmo_friends_chat_baseline_20261009.log.
Orta davranis/veri butunlugu hatasi: eski kullanici niyeti baska pawn'in listesi
ve pending riza istegine uygulanabilir. Bu kanit remote exploit/paket sirasini
veya hesap yetkisini asmayi kanitlamaz.

/arkadas dalinin basinda IsValid(SeenCommandPawn) && ==GetPawn gerekir; component
okumasindan once sessiz ret, ortak0,7sn throttle'dan sonra. Sil/ekle/kabul/accept,
liste ve ciplak /arkadas ayni kapiyi kullanir. Say/OnChatCommitted gonderim pawn'ini
zaten tasiyordu; yeni RPC parametresi/replicated alan/save schema yok. Friends
owner-only, component authority, mutual consent/capacity/name ve SaveThrottled
korunur. Ret yine chat throttle zamanini tuketir; fresh pawn replica geldikten
sonra yeniden gonderim gerekir. Eski UI gorunur davet satirina generation
baglanmaz; ayni pawn'da A daveti tuketilip B pending oldugunda gecikmis kabul B'yi
kabul edebilir. Recipient ID/generation/account kimligi bu scope'ta acik kalir.

Yeni nonShipping RunFriendsChatContextSelfTest + MMOTestFriendsChatContext:
standalone gameworld/unattended/NullRHI guard; 4nativePC/pawn ve GUID20charad;
ClientSetHUD/ClientReceiveChat buffer, renderer/Slate yok. PC identity/ad kalir
ama iki pawn'in listeleri farklidir; actual UnPossess/Possess sonrasi snapshot
alinir. PC0 actualCmdAdd ile replacementPawn3'te pending olusturur. 6commands
(sil/kabul/accept/ekle/liste/bare) x3invalidcontext (old/null/foreign) sessiz ret,
4Friends listesi/HUDcount ve inventory korunur. Privatepending exact snapshot
yok; reddedilen kabullerden sonra freshSayaccept dogru A ile mutual riza kurar.
Duplicate kabul entry eklemez, committed sil iki tarafli, ekle listeyi riza
oncesi degistirmez, hedef CmdAccept iliskiyi geri kurar; liste yalniz owner.
CmdList tekRPC sistem satirini newline ile split eder: HUD3 satir (header+2uye),
remote bant/rendering kaniti degildir. Invalid/selfadd ve inventorykontrol dahil28.

Worldtick/loadtimer calismaz; SaveThrottled normal SaveCharacter'i cagirir ama
initial-loadfalse disk yazisini keser. Gercek save/login kaniti degildir. Cleanup
once fixture committedflag, controllerdetach/destroy, pawn/HUDdestroy; presence
syntheticoffline yaratmaz. Kalici ad/GUID veya invitegeneration alanlari eklenmedi.
Header yalniz nonShipping staticfixture deklarasyonu; UHT0fileswritten, shipping
console/function yok. Server/Shipping hedefi ayrica derlenmedi.

Yeni kapi sonrasindaki positive DLL Succeeded4,38 sn ->PASS28 21:07:19.508UTC.
Drop-all /arkadas branch mutant DLL Succeeded4,38 sn ->FAIL20 21:08:13.431UTC
(UE0/runner1): fresh consentin reddini yakaladi. TEMP
mmo_friends_chat_negative_dropall_20261009.log. Python.write_bytes byteexact
restore yeni timestamp ile actualcompile/link Succeeded4,40 sn ->PASS28
21:08:42.444UTC /UE0/runner0; mutantDLL ile commit yok. TEMP
mmo_friends_chat_final_restore_20261009.cpp ve mmo_friends_chat_final_20261009.log.
Restore SHA256 311C9FA7B3112B3E1C9A7791EF45F27F32FBB350F2F16352C9FE9B81666ACDD0.
Degisen controllerCPP -NoPCH -NoLink -SingleFile Succeeded12,51 sn warning C0;
TEMP mmo_friends_chat_nopch_20261009.log. Onceki19 fullCPP bu tur yeniden yok.

ECC readonly Unreal/multiplayer/security/QA Leibniz final gate/callers/header,
fixture28/runner/RunAllChecksstep, restore bytes ve lifecycle sinirlarini
inceledi: blocker yok; dosya yazma/build/test yok. Parent finalrestorecompile,
PASS28 ve NoPCH kanitini gordu; agent paket surerken kapatildi. Agency plugin
provenansi bulunmadigi icin plugin kullanildi diye sunulmadi. Gercek remote
RPC/ownership/replication/Slate/PIE/load/reconnect ve aynipawn davetgeneration
PLAYTEST_CHECKLIST O31 maddelerinde acik. Yeni RunAllChecks step hazir.


**Final ECC verify:** RunAllChecks.ps1 -SkipBuild **40/40 GECTI**, exit0.
Yeni friendchatstep eklendi; FullCompile19 yeniden yok, degisen CPP ayri
NoPCH/W4/warning0 ve restore normalDLL onceden gecti. Friendchat28
21:10:54.566UTC, presence24 21:10:45.450, save26 21:10:38.350, parti83
21:12:07.420, klan73 21:12:26.460, DoT33 21:13:50.375, trade218
21:15:17.568 PASS; parser576/576, JSONschema2/negative111. Store440 loaded440,
invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1,
roundtrip equal1. Gercek oyuncu veya Lv1-30 kosu sayisi degildir.
Summer48/Winter26 randommob smoke exit0/error0; Net1join/1welcome ve ikitaraferror0.
TEMP mmo_all_friends_chat_final_20261009.log, mmo_net_server_001558.log /
mmo_net_client_001558.log. 151 Source/Scripts oncesi/sonrasi SHA256 differences0;
TEMP mmo_friends_chat_final_hashes_20261009.txt. Engine/UBT0, MMOSave_ZFC*.sav0.
Ayni checkout digerECCchat notLoaded, baska aktif yazici yok; readonlyreviewer
kapali, source/runnerfreeze boyunca yazim yok. StaticLintInitGameStatewhitelist
ve Lv33+skilluyarilari surer; gercek PIE/Shipping/Server acik.

| Kontrol | Sonuc | Sinir |
| --- | --- | --- |
| Restore normalDLL + controller NoPCH/W4 | Gecti; warning C0 | EditorDevelopment, Shipping/Server acik |
| Yeni friendchat28 + baseline/drop-all | PASS28, FAIL2/20 | Actual possession/HUD endpoints; remote degil |
| RunAllChecks -SkipBuild | Gecti40/40 | FullCompile19 yeniden yok; baglanti tum replikasyon degil |
| ECC readonly Unreal/multiplayer/security/QA | Final diff'te blocker yok | Parent actualbuild/log/paket/hashdogruladi |
| Gercek PIE | Yapilmadi | Samepawn invitegeneration, remote/load/identity acik |


## 69. Arkadas kabulunde alici davet nesli (2026-10-09 yerel / 8 Ekim UTC, O32)

ECC plan/review/verify, mmo-feature ve ue-build birlikte uygulanir. Bounded is:
ayni pawn'da tuketilen/suresi dolan A davetinin eski kabul istegi, yeni B'yi
kabul etmemeli; ret B'yi tuketmemeli, fresh Say/Enter kabul calismali. Mevcut
O31 pawn kapisi, ortak 0,7 sn throttle, mutual consent, liste/ad siniri ve
SaveThrottled korunur. Dosyalar Friends component/header, controller/header,
mevcut friend chat runner ve dort gorev belgesi. Baslangic HEAD901cb5f,
tracked temiz; .agents/.codex/AGENTS.md korunur. Diger ECC sohbeti notLoaded;
aktif baska yazici veya editor/UBT yoktu.

### Uygulama

Alici component'te runtime uint64 PendingInviteId owner-only replike olur.
Yalniz gercek davet verilirken artar; tuketim veya expiry sifirlamaz, MAX_uint64
sonrasinda yeni davet reddedilir. Weak inviter ve expiry server-only kalir;
yeni timer/Tick/save semasi yok. CmdAcceptSeen sifir veya uyusmayan ID'yi
pending'i okumadan/tuketmeden keser. Trusted native CmdAccept wrapper'i mevcut
ID'yi kullanir; client chat bu wrapper'i cagirmiyor. Say ve committed Enter
SendChatWithContext uzerinden gonderim anindaki owner ID'yi sekizinci RPC
parametresi olarak tasir. Server /arkadas kabul/accept bunu dogrular. Eski
trade/party/clan native cagrilarina ilgisiz friend alaninda0 eklendi.
RPC imzasi degistigi icin host/istemci ayni yeni build'i kullanmali.

### Gercek UE davranis kaniti

Ilk compiled baseline generation alanini ve yeni fixture'i iceriyordu;
chat dispatch kasitli olarak onceki CmdAccept latest fallback'i kullaniyordu.
11,69 sn UHT/compile/link Succeeded/exit0. UE'de FAIL33 21:22:53.439 UTC:
A tuketildikten sonra B beklerken A'nin eski /arkadas kabul istegi B ile iki
listeyi guncelledi. UE exit0, runner exit1 (davranis hatasi) idi.
TEMP mmo_friend_generation_baseline_build_20261009.log /
mmo_friend_generation_baseline_20261009.log.

Final dispatch + component boundary fixture derlemesi Succeeded7,91 sn/exit0;
PASS53 21:23:58.262 UTC. Native controller eski28 +15 yeni =43; component
private expiry/MAX boundary10 =53. A->B, sifir/eski/future ID ve iki kabul
alias'i, fresh Say/committed, duplicate, ayni inviter yeniden davet, expiry
sonrasi replacement, private pending/expiry korunmasi, duplicate invitation,
occupied pending ve son uint64 issuance/accept/exhaustion kapsandi. Expiry
fixture'i saati beklemek yerine private expiry'yi geriye alir; World tick veya
auto-load/save timer'i calistirmaz. Initial-load kapisi SaveThrottled'in
SaveCharacter cagrilarinin diske yazmasini keser. Gercek save/login testi degil.

Iki compiled negatif: component Seen ID kapi kaldirilinca FAIL33
21:24:23.502; wrap kapisi kaldirilinca FAIL53 21:24:55.625. Her ikisinde UE0,
runner1; kaynak restore Python write_bytes ile yeni timestamp aldi. Son
normal derleme actual Friends CPP compile/lib/DLL link, Succeeded2,91 sn/exit0;
restore PASS53 21:25:23.248 UTC. Final byte backup TEMP
mmo_friend_generation_final_restore_20261009.cpp; final paket asagida.

### Sinirlar

Generation gonderim aninda yakalanir; gorunen davet mesaji/kimlik ile ID'nin
atomik istemci sunumu saglanmadi. Eski mesaj okunurken client zaten B'nin ID'sini
aldiysa yazilan generic kabul B'yi secebilir. Hesap/isim yeniden kullanimi,
source controller rebinding, reconnect/offline consent ve atomik disk kaydi
ayri acik konulardir. Native authority/HUD buffer gercek remote RPC teslimi,
replication veya Slate/Canvas kaniti degildir. Shipping/Server derlenmedi.
Gercek PIE maddeleri PLAYTEST_CHECKLIST O32 bolumunde aciktir.

Salt okunur reviewer Franklin (Unreal/multiplayer/security/QA + ECC) final
header/diff/callers/save/lifetime ve 43+10 sayimini inceledi; somut blocker yok.
Tum RPC cagrilarinin 8 parametre tasidigi, client fallback olmadigi ve owner-only
alan/save ayrimi dogrulandi. Ajan dosya/build/test yazmadi; parent actual UE
log/exit/restore byte equality dogruladi. Ajan paket surerken kapatildi.
Agency plugin paket kokeni yerelde dogrulanmadi; bu inceleme acik rol olarak
kullanildi. Final Friends CPP / TEMP backup SHA256 ayni:
7456007CB45692B03A27E544A4DAE5F4E8996E35108A01A532E354A05CCEB1D7.

### O32 final paket

`RunAllChecks.ps1 -SkipBuild -FullCompile`: 41/41 GECTI, exit0. Bu sefer 19 CPP
NoPCH/NoLink/SingleFile /W4 gercekten yeniden derlendi, hepsi OK/warning C0;
toplam warning0. Normal linked DLL once restore edilip PASS53 verdigi icin
-SkipBuild eski/mutant DLL'yi gizlemiyor. Final native53 21:29:47.639 UTC;
presence24 21:29:35.837, save26 21:29:27.725, parti83 21:30:44.551,
klan73 21:31:02.427, DoT33 21:32:23.145 ve trade218 21:33:50.111 PASS.
Parser576/576, JSONschema2/negative111. Store447 slots/447loaded, invalid/
loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1/roundtrip equal1.
Bu sayi gercek oyuncu veya Lv1-30 kosusu degildir.

Summer50/Winter29 random mob smoke exit0/error0; Net1join/1welcome,
server/client error0. TEMP mmo_all_friend_generation_final_20261009.log,
mmo_net_server_003418.log / mmo_net_client_003418.log. Source/Scripts 151 SHA256
oncesi/sonrasi differences0; TEMP mmo_friend_generation_final_hashes_20261009.json.
Restore backup byte equality saglandi. MMOSave_ZFC*.sav0; engine/UBT0.
Reviewer kapali, oyun kaynaklari ve runner tum paket boyunca sabitti.
StaticLint InitGameState whitelist ve Lv33+ skill uyari sinirlari surer.

| Kontrol | Sonuc | Sinir |
| --- | --- | --- |
| UHT + restore normal linked DLL | Gecti; actual compile/lib/DLL link | Editor Development; Server/Shipping acik |
| Native controller + private boundary | PASS53; baseline/gate33/wrap53 negatifleri yakalandi | Sentetik expiry, HUDbuffer; gercek remote degil |
| RunAllChecks -SkipBuild -FullCompile | Gecti 41/41; 19 PCH'siz CPP warning0 | Net baglanti davet UI/replication kaniti degil |
| ECC + readonly rol incelemesi | Final diff'te blocker yok | Parent actual build/log/hash/exit dogruladi |
| Gercek PIE | Yapilmadi | O32 remote/mesaj-ID/kimlik/reconnect maddeleri acik |


## 70. Parti sohbetinin gönderim pawn/üyelik bağlamı ve kanal gizliliği (O33)

O32 HEAD `13bf615` sonrası iki davranış gerçek UE dünyasında yeniden üretildi:
aynı pawn A partisinden çıkıp B'ye geçtiğinde gecikmiş `/p` metni B'ye gidiyordu;
`/p /w DışOyuncu sır` ise parti gövdesini tekrar komut olarak ayrıştırıp parti
üyesi olmayan kişiye fısıldıyordu. `/f` takma adı aynı parser yolunu kullanır.

### Sınırlı uygulama ve ECC incelemesi

ServerSendChat'in parti fanout dalı artık IsValid(SeenCommandPawn), güncel sahip
pawn eşleşmesi ve mevcut PartyMembershipId ile sıfır olmayan gönderim ID'si
eşleşmesini doğrular. Say ve Enter zaten ortak SendChatWithContext üzerinden
owner-only görülen ID'yi taşır; sunucudan son ID'yi seçen fallback eklenmedi.
Fısıltı parser'ı yalnız genel kanal0'da çalışır: `/p` gövdesi literal parti
metnidir. Güncel partisiz oyuncunun yalnız kendi sistem yanıtı korunur.

Yeni RPC alanı, header, save şeması, replike alan, Tick veya timer yok. Ortak
0,7sn throttle retleri de sayar; uzunluk/sanitizasyon, mevcut menzil ve üyeye
ClientReceiveChat teslimi değişmedi. Sürekli üyelikte başka kişinin katılması
ID'yi döndürmez; güncel sunucu roster'ına gönderim tasarım olarak sürer.

ECC plan/review/verify ve mmo-feature/ue-build akışı uygulandı. Parent final
header/çağıranlar, owner-only ID issuance, parser sırası, ret snapshot'ları,
UObject cleanup ve initial-load/save kapılarını inceledi; bu diff'te somut
blocker bulunmadı. Salt okunur Sartre incelemesi kullanım sınırında hata verdi,
sonuç üretmedi ve ajan kapatıldı; bağımsız başarılı inceleme iddia edilmiyor.
Agency plugin kökeni doğrulanmadı. Native fixture nonshipping, yalnız
standalone + unattended + NullRHI dünyasında çalışır; HUDlar/tick kapalı,
sonunda party leave ve save-committed cleanup yapılır. Oyun SaveCharacter
başlatılmadan synchronous çalışır; UI/remote kanıtı değildir.

### Native kanıt: 83 → 104

Parti toplam104 kontrol = önceki83 +21 yeni routing assertion. Controller chat
altkümesi17→38; diğer66 kontrol component/lifecycle'dır. Üç native PC/pawn/HUD
ile A→B, zero/foreign/random ID, null/foreign/eski pawn, gerçek possession swap,
fresh Say/Enter, büyük harf `/P`, boş prefix, literal `/w` ve `/f`, stale nested
body, sürekli üyeye üçüncü kişi join, solo hint, genel/fısıltı regresyonu çalışır.
Rette üç partinin tüm üyelik/davet alanları, inventory ve HUD Text/Color/Time
aynıdır. Grup setup native pending seed + accept kullanır; gerçek davet gecikmesi
veya network paket sırası ölçülmez.

İlk baseline normal linked build12,20sn: FAIL75/check76
`old party message leaked into replacement group` (8 Ekim21:43:02.625UTC).
Yalnız üyelik/pawn kapısı build4,49sn: FAIL85/check86
`party body escaped to outside whisper recipient` (21:43:39.511UTC).
Kanal kapısı + iki alias kontrolü build4,48sn: PASS10421:44:34.652UTC.
Derlenmiş pawn kapısı kaldırma negatifi FAIL79/check80 `null pawn party message
delivered`21:45:28.239UTC; kanal kapısı kaldırma negatifi FAIL85/check86
21:46:07.399UTC. Negatiflerde UE exit0 olsa da wrapper FAIL ile exit1 verdi.
Final PC bytes TEMP backup'tan geri yazıldı; actual CPP compile/lib/DLL link
Succeeded4,46sn/exit0, PASS10421:46:51.307UTC; mevcut byte equality doğrulandı.

İki değişen CPP ayrı NoPCH/NoLink/SingleFile /W4: Party9,50sn ve Controller10,73sn,
her biri Succeeded/exit0/warning C0. O32'nin19 CPP çalışması bu işe mal edilmez.
TEMP loglar: mmo_party_routing_baseline*, mmo_party_routing_nested_baseline*,
mmo_party_routing_negative_pawn*, mmo_party_routing_negative_channel*,
mmo_party_routing_restore*, mmo_party_routing_nopch_<CPP>_20261009.log.
Baseline/negatif/restore sonuçları8 EkimUTC; geniş final paket9 EkimUTC'dir.

### Açık sınırlar

Gerçek host + uzak istemci, owner-only ID teslimi, paket gecikmesi/kaybı ve
Slate/Canvas yapılmadı. Görünen roster/metin/gönderim ID'sinin atomik sunumu,
source controller/account/name rebinding, target kick sonrası yeniden katılım,
reconnect/offline kalıcılık ve Server/Shipping ayrı açık işlerdir. Sonuçlar
PLAYTEST_CHECKLIST O33 kutularını kapatmaz.


### O33 final paket (9 Ekim 2026 UTC)

`RunAllChecks.ps1 -SkipBuild`: 40/40 GECTI, exit0. Normal DLL restore/link ve
iki değişen CPP NoPCH zaten geçti; FullCompile19 bu pakette çalıştırılmadı.
Native parti104 05:15:49.772UTC; arkadaş53 05:14:58.827, presence24 05:14:52.795,
save26 05:14:46.623, klan73 05:16:02.091, DoT33 05:16:58.305,
trade218 05:18:05.608 PASS. Parser576/576; JSONschema2/negative111.
Store454 yuva/454loaded, invalid/loadFailed/existsMismatch/jsonMismatch0,
maxLevel6, writeorder1/roundtrip equal1; gerçek oyuncu veya Lv1–30 koşusu değil.
Summer49/Winter31 random mob smoke exit0/error0; Net1join/1welcome, iki taraferror0.
TEMP mmo_all_party_routing_final_20261009.log; mmo_net_server_081831.log /
mmo_net_client_081831.log. Source/Scripts151 SHA256 paket önce/sonra differences0;
mmo_party_routing_final_hashes_20261009.json. FinalPC backup bytes aynı,
MMOSave_ZPC*.sav0; engine/UBT0. StaticLint InitGameState whitelist ve Lv33+
beceri aralığı uyarıları sürer. Gerçek PIE ve Server/Shipping yapılmadı.


## 71. Aynı pawn'da eski klan sohbeti/ayrılmasının yeni üyeliğe taşınması (O34)

O30 pawn kapısı aynı pawn'daki üyelik değişimini ayırt etmiyordu. Yeni observed
ID plumbing'i olan, dispatch kapısı henüz olmayan UE baseline FAIL75/check76:
`old clan message leaked into replacement clan` (9 Ekim05:25:39.482UTC).
Yalnız /g kapısı sonrası FAIL76/check77: `old clan leave removed replacement
membership`05:26:18.174UTC. İki durumda da aynı pawn A'dan çıkıp B'ye katılmıştı;
eski A metni B'ye gitti veya eski ayrılma B üyeliğini sildi. UEexit0, wrapperexit1.

### Plan ve sınırlı uygulama

ClanMembershipId runtime FGuid yalnız sahibine replike olur. Başarılı create,
accept ve valid authority ApplySaveData'da yeni kimlik; leave, kick ve geçersiz/
boş load'da invalidation. Aynı klan adıyla reload/rejoin de yeni ID alır.
Liderlik aktarımı ve başka üyenin join/leave'i sürekli üyenin ID'sini döndürmez.
Alan save verisi/JSON'a eklenmedi; eski kayıtlar valid load'da runtime ID kazanır.

Say ve Enter'ın ortak SendChatWithContext'i yerel owner-only ID'yi dokuzuncu
ServerSendChat parametresiyle taşır. Yalnız /g ve /klan ayril, oyuncu hâlen bir
klandaysa geçerli eşleşen gönderim ID'sini mevcut pawn kapısına ek olarak ister.
Sıfır/eski/başka üye/unknown ID aynı pawn'da fanout veya leave yapmaz. Mevcut
klansız kişisel hint korunur. Sunucu son ID'yi istemci niyeti yerine seçmez.
Native trusted CmdLeave/CmdChat wrapper davranışı korunur; client chat dispatch
kapıyı uygular. Yeni Tick/timer veya save şeması yok; owner-only GUID + RPC
imzası değişti, host/istemci aynı yeni build'i kullanmalı.

Kapsam yalnız bu iki yoldur. /klan davet/at/liste/kur ve davet edenin aynı pawn
üyelik yeniden bağlanması ek membership kapısı kazanmadı; hedef kick/rejoin
niyeti ayrı açık iş. Mevcut recipient accept generation ve bütün /klan pawn
kapısı korunur. UI roster/metin/ID atomikliği ve account/name/controller
rebinding çözülmedi. `/klan leave` takma adı eklenmedi; mevcut komut `ayril`dır.

### Doğrulama

Klan73→97 (+24), controller41→65. Mevcut32 component kontrolünün bazı assertion
ve snapshot'ları runtime membership alanını da kapsar; sayıları artmadı.
Yeni24: unique/valid load ID; A→B; eski/zero/foreign/random ID ile /g ve ayril;
fresh uppercase Say ve Enter /g; başka üye join'inde continuous ID/delivery;
aynı B rejoin eski metin/ayrılma; fresh Enter/Say leave; kick invalidation ve
sender rejoin; invalid/empty load; aynı B reload; inventory koruması.
Rette kayda yansıyan clan alanları, dört membership/pendingInvite ID'si, inventory
ve tam HUD Text/Color/Time değişmez. Component accept snapshot'ı ayrıca private
JoinSeq/acting/pending/clock alanlarını içerir. Controller routing testindeki
private pending alanları ayrı assertion'a eklenmedi.

Fixture yalnız nonshipping standalone/unattended/NullRHI dünyasında synchronous
çalışır. Dört gerçek native PC/pawn ve HUD buffer; entry testlerinde doğrudan
ClanComponent ApplySaveData in-memory çağrısı, disk read/load veya network yok.
Önceki pawn rejection testlerine doğru replacement ID bilerek verilir: membership
kapısı eski pawn regresyonunu maskelemez. Cleanup travel-save marker ile
UnPossess/Destroy ve HUD destroy; normal load timer tick etmez, fixture save yok.
Başarılı CmdCreate girişindeki ID üretimi kod incelemesiyle doğrulandı; yeni
başarılı create fixture'ı eklenmedi (disk/cache yan etkisi bu kapsamda değil).

Baseline UHT + actual module/component/controller/lib/DLL29,59sn. Chat-only
kapı build4,88sn; iki kapı build4,60sn/PASS97 05:26:50.112UTC. Gerçek derlenmiş
load mutantı aynı valid membership ID'sini tutunca FAIL95/check96
`same clan reload retained stale context`05:27:40.849UTC (build3,11sn).
Final component bytes backup'tan geri yazılıp actual compile/lib/DLL3,11sn,
PASS97 05:28:05.195UTC. Pawn assertion izolasyonu sonrası actual PC compile/
lib/DLL4,57sn ve PASS97 05:28:37.391UTC. Tüm pozitif buildler exit0/Succeeded.
TEMP mmo_clan_membership_{baseline,chat_gate,fixed,negative_reload,restore,final}
logları20261009; final PC ve component exact byte backups/hash freeze151.

ECC plan/review/verify, mmo-feature/ue-build kullanıldı. Parent sunucu dispatch,
owner-only replication, tüm production membership writers, save separation,
RPC/header/callers ve synchronous UObject cleanup kapsamını inceledi. Gerçek
remote RPC/owner-only teslim, Slate/Canvas, disk re-login, Shipping ve Server
ayrı yapılmadı; PLAYTEST_CHECKLIST O34 kutuları açık kalır.


Salt okunur Carver (explicit Unreal/multiplayer/security/QA + ECC) frozen final
beş kaynak/runner diff'ini, production writers/save callers/replication/context/
cleanup ve 97/65 beklentilerini inceledi. Component blob388770d07a9f ve controller
blob93c78c9edcbf: blocker yok. Önceki partial dispatch bulgusu giderildi; reload
mutantı finalde yok. Ajan edit/build/test/commit yapmadı; full paketi parent
çalıştırır. Ajan tamamlanınca kapatıldı. Agency plugin paket kökeni doğrulanmadı.


### O34 final paket (9 Ekim 2026 UTC)

`RunAllChecks.ps1 -SkipBuild -FullCompile`: 41/41 GECTI, exit0. Bu pakette19 CPP
NoPCH/NoLink/SingleFile /W4 yeniden derlendi, hepsi OK ve toplam warning C0.
Normal linked DLL önce final compile/link ile üretildi; Source/Scripts151 hash
paket önce/sonra differences0, iki final backup byte equality sağlandı.
Klan97/controller65 05:33:45.825UTC; parti104 05:33:33.784, arkadaş53
05:32:48.143, presence24 05:32:41.062, save26 05:32:34.961,
DoT33 05:34:45.118 ve trade218 05:35:47.207 PASS. Parser576/576,
JSONschema2/negative111. Store461 yuva/461loaded, invalid/loadFailed/
existsMismatch/jsonMismatch0, maxLevel6, writeorder1/roundtrip equal1.
Bu yerel yuva sayısı gerçek oyuncu veya Lv1–30 koşusu değildir.

Summer49/Winter29 random mob smoke exit0/error0; Net1join/1welcome,
server/client error0. TEMP mmo_all_clan_membership_final_20261009.log,
mmo_net_server_083612.log / mmo_net_client_083612.log; hash notu
mmo_clan_membership_final_hashes_20261009.json. ZClanCtx fixture kayıt0;
engine/UBT0, reviewer kapalı. StaticLint InitGameState whitelist ve Lv33+
beceri aralığı uyarıları sürer. Gerçek PIE/disk login/Shipping/Server açık.

## 72. O35 — Klan davetini davet edenin kaynak üyeliğine bağlama (9 Ekim 2026)

### Doğrulanmış hata ve sınırlı plan

O34 runtime üyelik GUID'i varken pending invite yalnız weak pawn/ad/zaman
saklıyordu. Davet eden aynı pawn'da ayrılıp aynı klana tekrar lider olarak girince
eski davet yeni üyelik yetkisiyle kabul edilebiliyordu. Derlenmiş baseline native
FAIL34/check35 `old invitation accepted after source same-name rejoin`,
05:44:46.211UTC; UE exit0, wrapper exit1. Re-entry bu fixture'da gerçek CmdLeave
ardından component in-memory ApplySaveData'dır; disk login veya başarılı create değil.

Küçük adımlar: fixture/source snapshot plumbing ile native reproduksiyon;
matching accept'te source üyelik eşleşmesi; derlenmiş tüketim negatif deneyi;
exact source restore + actual linked DLL; değişen CPP NoPCH/W4; mevcut otomatik
paket ve salt okunur final inceleme. Kaynak/runner sonrasında donduruldu.

### Uygulama

Protected FGuid PendingInviterMembershipId yalnız sunucuda ve geçicidir.
SetPendingInviteServer valid source ClanMembershipId ister ve GUID'i issuance
anında yakalar. Eşleşen recipient nonce snapshot'ı local'e alıp weak/ad/zamanla
birlikte tüketir; sonra captured valid/equal current source GUID + current
leader + clan name doğrulanır. Aynı pawn/isim rejoin veya valid same-name reload
eski daveti geçerli kılmaz. Zero/stale/future recipient nonce tüm snapshot'ı
korur; geçersiz source issuance recipient nonce'u da değiştirmez.

O34 dokuz parametreli RPC, owner-only recipient nonce/runtime membership ve
save/JSON şeması aynı; yeni replicated alan, timer veya UObject sahipliği yok.
Weak inviter life gate mevcut. Pending lock/cooldown/expiry policy aynı kalır:
source revocation lock'ı anında açmaz, mevcut expiry/consume yolu gerekir.
Liderliği üyelikten ayrılmadan kaybedip tekrar kazanma için ayrı authority epoch,
account/controller identity rebinding ve diğer /klan komutlarının membership
bağlamı bu işin kapsamı dışında; hedef kick/rejoin ve UI atomik sunumu açık.

### Gerçek doğrulama ve sınırları

Native97→112 (+15 source kontrolü); component32→47, controller65 aynı.
Yeni15: issuance capture; aynı-ad source leave/rejoin generation; eski ret;
fresh kabul; duplicate; same-name reload ret; zero/foreign captured GUID;
invalid current GUID ret; invalid source issuance unchanged; stale/zero recipient
nonce preserve; matching consumption; recovery; iki actor inventory unchanged.
Önceki tüketim/snapshot assertion'ları yeni private GUID'i de içerir.

Controllerless iki yeni actor, nonshipping standalone/unattended/NullRHI,
synchronous doğrudan component call, tick/collision kapalı ve dış gruptan uzak.
Gerçek remote RPC/paket/elapsed expiry/disk load çalıştırılmaz. Existing native
controller65 testleri runner'da ayrı PASS şartı olmaya devam eder. Cleanup
fixture clan/leader alanlarını sıfırlayıp actor'ları destroy eder; ilk load yok.

Baseline UHT/module/component/lib/DLL11,23sn; source gate actual compile/link
3,16sn ve PASS112 05:45:20.237UTC. Yeni pending source temizliğini kaldıran
DERLENMİŞ mutant build3,14sn, FAIL0/check1 05:45:48.656UTC: eski assertion etiketi
`demoted inviter accepted`; ret üyelikten değil Consumed() içindeki private
source GUID'in temizlenmemesinden başarısız olur. Bu label tek başına join kanıtı değil.
Final exact bytes yeniden yazıldı, actual compile/lib/DLL3,09sn ve
PASS112 05:46:20.265UTC. NoPCH/NoLink/SingleFile8,84sn exit0/Succeeded,
response /W4, warning C0. Tüm linked buildler exit0/Succeeded.
TEMP mmo_clan_source_{baseline,fixed,negative_clear,restore,nopch_final}_20261009
build/test/UE logları ve mmo_clan_source_final_restore_20261009.cpp byte backup.

ECC plan/review/verify, mmo-feature/ue-build uygulandı. Parent authority,
source writers/consume gates/save separation/replication/GC/RPC callers'ı
inceledi. Salt okunur Sagan aynı final üç dosyayı iki kez hash'ledi;
component git blob019889b14207, header58550f7aebd2, runner00744aa553c0:
somut blocker yok, ek clear path yok. Ajan edit/build/test/commit yapmadı;
Unreal/multiplayer/security/QA fallback rolleri, Agency provenance doğrulanmadı.
Tamamlanınca kapatıldı. Gerçek PIE/Server/Shipping kutuları açık.

### O35 final paket (9 Ekim 2026 UTC)

RunAllChecks.ps1 -SkipBuild40/40 GECTI, exit0. Değişen ClanComponent CPP ayrıca
NoPCH /W4 warning0; bu bounded private field için19 CPP FullCompile tekrar
çalıştırılmadı (O34 geçmiş paketidir). Normal UHT/module rebuild ve linked DLL
restore yukarıda; Source/Scripts151 before/after hash differences0, backup aynı.
Klan112/chat65 05:49:02.356UTC; parti104 05:48:46.154, arkadaş53 05:48:00.526,
presence24 05:47:54.488, save26 05:47:48.451, DoT33 05:49:57.374,
trade218 05:51:00.949 PASS. Parser576/576, JSONschema2/negative111.
Store468slots/468loaded; invalid/loadFailed/existsMismatch/jsonMismatch0,
maxLevel6, writeorder1/roundtrip equal1. Yerel yuvalar oyuncu/Lv30 kanıtı değil.
Summer49/Winter30 smoke exit0/error0; Net1join/1welcome/server-client error0,
TEMP mmo_net_server_085128.log / mmo_net_client_085128.log. Paket logu
mmo_all_clan_source_final_20261009.log; hash mmo_clan_source_final_hashes_20261009.json.
ZClanCtx fixture save0; Sagan kapalı. StaticLint InitGameState whitelist ve
Lv33+ skill-gap uyarıları sürer; gerçek PIE/disk login/Server/Shipping açık.

## 73. O36 — Klan davet/at/liste isteğinde gönderen üyeliği (9 Ekim 2026)

### Yeniden üretilen hata ve uygulama

O35 kabul anında source GUID'i doğrular; davet isteğinin gönderildiği üyeliği
tek başına korumaz. Aynı pawn A'dan B'ye geçtiğinde eski /klan at hedef isteği
B'nin üyesini atabilir, davet B adına çıkabilir, liste B roster'ını döndürebilir.
O34 pawn kapısı bu aynı-pawn değişimini ayırt etmez; yalnız /g ve ayril üyelik
kapısına sahipti. Native baseline actual DLL FAIL112/check113 eski kick;
kick kapısı sonrası FAIL113/check114 eski invite; kick+invite kapısı sonrası
FAIL114/check115 eski list. Üçü de UE exit0/wrapper1 ile yakalandı.

Küçük derlenebilir adımlar: uygun kaynak/hedef/roster ile native fixture;
sırasıyla üç kapı ve her aşamada linked DLL/UE testi; final NoPCH /W4;
frozen source/runner hash, mevcut paket ve bağımsız final inceleme.
CmdInvite/CmdKick/CmdList çağrılarından hemen önce aktif üyelikte valid/equal
SeenClanMembershipId istenir. O34 dokuzuncu RPC alanı kullanılır; yeni save,
replicated alan, UPROPERTY, timer veya RPC imzası yok. Conditional aktif üyelik
kontrolü klansız kişisel hint'i korur. Ortak chat throttle önce çalışır,
component leader/life/range/pending/cooldown kapıları sürer. Recipient kabulü
ayrı SeenClanInviteId yolundadır; source pending GUID O35'teki gibidir.

### Native kanıt ve sınır

112→138 (+26), controller65→91; component47 aynı. Yeni26: yeni kaynak aynı pawn
A→B ve current leader/eligible target setup; eskiA ile üç ret; zero/foreign/random
ID ile dokuz ret; fresh Enter liste; fresh Say davet, alıcı Say kabul;
fresh Enter kick; same-B source rejoin setup/üç ret; same-name reload setup/
üç ret; fresh Say liste. Son inventory assertion eski sayıya dahildir.
Ret snapshot'ı dört save clan alanını, üyelik GUID/recipient nonce'larını ve
HUD Text/Color/Time'ı korur. Private pending kaynak/clock tam snapshot'ı bu
controller fixture'da yok; stale davetlerden sonra fresh başarı, kaynak
invite cooldown'ın tüketilmediğini gösterir. Shared chat throttle reset'i
fixture'a özeldir, component clock/cooldown/expiry bypass edilmez.

Önceki aynı-frame sender davet cooldown'ına girdiği için yeni native pawn
kullanılır: önceki pawn CmdLeave/UnPossess ile emekliye alınır; travel-save
marker, synchronous no-load/test koşulu ve teardown destroy ile korunur.
Yeni sender A→B/rejoin/reload boyunca aynı pawn'dır; dört gerçek PC/HUD reused.
Hedef kontrollü/yaşayan/klansız ve menzildedir. Source üyelik girişleri doğrudan
component in-memory ApplySaveData; disk login/create veya elapsed timer değil.
Native Say/Enter ve HUD buffer gerçek remote RPC/Canvas/Slate kanıtı değildir.

Baseline normal module/PC/lib/DLL10,55sn, FAIL112 05:57:50.411UTC.
Kick-gate actual compile/link4,71sn, FAIL113 05:58:26.437;
kick+invite4,67sn, FAIL114 05:59:08.005. Üç kapı final actual PC compile/link
4,78sn ve PASS138/chat91 05:59:33.401UTC. Bu derlenmiş kısmi kapı sonuçları üç
eksik dalı ayrı yakalar; final sonrası ayrı mutant derlemesi yapılmadı.
Final PC NoPCH/NoLink/SingleFile12,57sn, /W4 response, warning C0,
exit0/Succeeded.19 CPP FullCompile bu bounded dispatcher işi için tekrar yok.
TEMP mmo_clan_authority_{baseline,kick_gate,invite_gate,fixed,nopch_final}
20261009 build/test/UE logları, exact final PC backup ve151 source/runner hash.

ECC plan/review/verify ve mmo-feature/ue-build uygulanır. Parent frozen diff,
authority/context/membership writers/save/GC/caller/throttle/fixture cleanup'ı
inceler. /klan kur solo bağlamı, hedef kick/rejoin generation (sender üyeliği
kesintisizse), leader loss/regain epoch, account/controller/name rebinding,
owner-only gecikme ve UI atomik görünümü bu üç dalın dışında açık kalır.


Salt okunur Euler final review blocker0: yalnız PC/runner kaynak farkı,
151/151 freeze dosyası eşleşti, coverage/hash fark0. Ajan, parent'in mevcut baseline,
kick-gate, invite-gate ve final DLL/UE loglarını da okudu; kendi test/build/edit
çalıştırmadı. PC SHA256 B465E60F1CD757F8DB5A08D897EFD67C0E58451C69B842ADB8A17BCCC73E262A,
runner EA8DFA049D66D38F55F1AB22D12971D1BB438359AA37A859C8610D2A6C32FCBD.
Final authority/lifecycle/cleanup/current positive yolları incelendi; ajan kapatıldı.
Unreal/multiplayer/security/QA fallback rolleri, Agency provenance mevcut değil.

### O36 final paket (9 Ekim 2026 UTC)

RunAllChecks.ps1 -SkipBuild40/40 GECTI, exit0. Değişen PC ayrıca NoPCH /W4
warning0; final linked DLL önce actual compile/link ile üretildi.19 CPP
FullCompile bu bounded dispatcher işinde tekrar yok. Source/Scripts151 hash
fark0; final PC exact backup aynı. Klan138/chat91 06:03:15.495UTC;
parti104 06:02:58.881, arkadaş53 06:02:06.898, presence24 06:01:59.724,
save26 06:01:53.313, DoT33 06:04:59.003, trade218 06:06:49.364 PASS.
Parser576/576, JSONschema2/negative111. Store475slots/475loaded,
invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6,
writeorder1/roundtrip equal1. Yerel yuva sayısı gerçek oyuncu/Lv30 kanıtı değil.
Summer49/Winter32 smoke exit0/error0; Net1join/1welcome/server-client error0.
TEMP mmo_all_clan_authority_final_20261009.log, mmo_net_server_090734.log /
mmo_net_client_090734.log, mmo_clan_authority_final_hashes_20261009.json.
Euler kapalı; engine/UBT0, ZClanCtx fixture save0. InitGameState whitelist
ve Lv33+ skill-gap uyarıları sürer. Gerçek PIE/disk login/Server/Shipping açık.


Commit öncesi paralel belge işi gözlendi:0874531 yalnız SKILL_PLAN_LV31_45.md
ve DECISIONS_NEEDED.md'yi değiştirdi; O36'nın altı dosyasıyla kesişim yok.
Bu dosyalara dokunulmadı;151 kaynak/runner hash hâlâ aynı. O36 doğrulama
kanıtı aynı gameplay bytes için geçerli, belge değişikliği oyun derlemesi istemez.

## 74 · O37 · Parti daveti kaynak bağlamının ömrü (9 Ekim 2026)

Alıcı PendingInviteId yalnız alıcının davet neslini ayırıyordu. Aynı davetçi
pawn A→B/rejoin/merge veya solo→parti geçince eski davetin kabulü davetçinin
şimdiki grubunu kullanabiliyordu. Compiled baseline DLL11,37sn ve native
FAIL58/case0 06:19:08.608UTC, wrapper1 (UE exit0) ile yanlış gruba katılım yakalandı.

Protected, server-only PartyInviteContextId ve PendingInviterContextId eklendi.
İlk issuance lazy GUID üretir; yeni üyelik/session, açık ayrılma ve son kalan
üyenin solo olması bağlamı yeniler. Devam eden üyede roster/liderlik değişimi
bağlamı korur. Matching accept pending GUID'i kopyalayıp weak/time/GUID'i temizler;
Mevcut süre/mesafe ve valid/equal kaynak GUID kontrolleri sonrası
yetki/kapasite akışı FormOrJoin ile uygulanır. Matching decline GUID'i de temizler. Stale/zero
recipient nonce mevcut pending durumunu değiştirmez. RPC, save ve replikasyon
şemaları değişmedi; GUID'ler sunucuda kalır, yeni UObject referansı/timer yok.

Native149 = önceki104 +45 (controller38 + component/lifecycle111). Sekiz kaynak
geçişi: A→B, aynı surviving session'a rejoin, group→solo açık ayrılma,
solo→group, iki ayrı solo→group→solo yolu, merge ile session transferi ve
partner ayrıldığında grouped source'un solo last-survivor olması. Her geçişte
issuance, zero response, matching eski kabulün grup/üyelik/inventory/diğer pending
ve tüm nonce'ları koruması, consumed duplicate ve fresh davet recovery denetlenir.
Ek continuous member/lider değişimi pozitif, decline clear ve zero/random kaynak
GUID ret kontrolleri var. Native PC testlerinde manuel pending setup production
SetPendingInviteServer kullanır; replacement-pawn numeric nonce seed bilinçli
izolasyondur. Test-only friend Shipping'de yok; normal issuer protected kalır.

İlk final8case DLL3,42sn PASS149 06:21:54.104UTC. Son snapshot helper yalnız
matching alıcının tüketilen pending alanlarını dışlar; diğer alıcıları ve tüm
recipient nonce'larını tam kıyaslar. Bu frozen final bytes ile OP collapse GUID
renewal eksiltilen compiled mutant3,64sn FAIL93/case7 06:22:53.199UTC, wrapper1.
Exact restore write_bytes ardından actual CPP/lib/DLL3,70sn, PASS149
06:23:06.854UTC, wrapper0. TEMP mmo_party_source_{baseline,final,mutant,restore}
20261009 build/test/UE logları ve final backup/freeze151 dosyası.

ECC plan/review/verify + mmo-feature/ue-build akışı. Gerçek remote RPC, HUD/Slate,
owner-only teslim gecikmesi, Shipping/Server, disk login ve PIE açık. Kaynak
hesap/controller rebinding, kesintisiz üyelikte leader loss/regain authority epoch,
recipient'in grup değiştirmesi, eski issuing InviteNearest veya kick niyeti,
ApplyPartySet delegate reentrancy/UI atomikliği bu sınırlı düzeltmede kapanmadı.

O37 scoped final review: Mencius salt okunur Unreal/multiplayer/security fallback
rolleriyle dört kaynak/runner dosyasını inceledi, blocker0. Agency provenance
mevcut değil. Own dört SHA freeze151 kaydıyla eşleşti; Party CPP exact backup
aynı. Ajan build/test/edit yapmadı ve kapatıldı. Değişen Party/PC CPP NoPCH,
NoLink/SingleFile /W4 exit0/Succeeded9,68/10,35sn, warning C0. Header UHT+normal
Editor build geçti; gerçek Shipping/Server veya19 CPP FullCompile yapılmadı.

Paralel dış ajan d452994 ile yalnız CONTENT_PLAN_LV46_80.md ekledi; dokunulmadı.
Sonrasında aktif MMOMobArchetype.cpp, MMOGameMode.cpp, MMOQuestTypes.cpp,
MMOQuestDialogue.cpp ve Lore.md işine dokunulmadı. Freeze151 karşılaştırması
bu dört dış CPP'de fark gösterir, kendi dört source/runner bytes hâlâ eşleşir.
Bu nedenle eski database DLL ile süren paket, yeni dış içerik kaynaklarının
derlenip doğrulandığı olarak sunulamaz; dış değişiklikler bu commit kapsamı değil.

### O37 paket sonucu ve paralel çalışma sınırı

RunAllChecks -SkipBuild40/40 GECTI, exit0. Native parti149 06:27:42.852UTC,
klan138 06:28:05.883, arkadaş53 06:26:28.737, presence24 06:26:18.467,
save26 06:26:06.371, DoT33 06:29:44.617, trade218 06:30:51.130 PASS.
JSONschema2/negative111; parser576/576. Store482slots/482loaded,
invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1,
roundtrip equal1. Summer52/Winter31 exit0/error0; Net1join/1welcome/error0.
TEMP mmo_all_party_source_final_20261009.log ve mmo_net_{server,client}_093120.log.

Paket sürerken dış ajan cdb95fa ile W1 içerik işini ayrı commit etti ve aynı
beş dosyada W2 işine başladı. Bu dosyaların değişmesine müdahale edilmedi.
Kaynakların tamamı/DLL paket boyunca tek bir frozen database revision değildi;
40/40 sonucu yeni W2 kaynakları veya aggregate snapshot için doğrulama kanıtı
sayılmaz. Commit yalnız önceden actual DLL ile derlenip native149/compiled
negatiflerle doğrulanmış O37 dört source/runner + dört kayıt belgesini içerir.
Kendi dört source/runner SHA'ları ilk freeze ile aynı; exact Party backup aynı.
Paralel beş dosya ne stage edilir ne commit edilir. Gerçek PIE/Server/Shipping açık.

## 75 · Sabit snapshot entegrasyon doğrulaması (9 Ekim 2026)

Önceki O37 paketinde paralel database/DLL değişimi bütünsel doğrulama sınırı
oluşturmuştu. O37 (163c86d), W1 (cdb95fa), W2 (54de52b) ve R (9fdd2ea)
bu kez 9fdd2ea0d6ac8d14cbd3ccd5cd7e2d30edb9a38e snapshot'ında birlikte doğrulandı.
Managed worktree aggregate-ue-verification, aynı snapshot'tan temiz checkout;
ana ajanların edit/build dosyaları paylaşılmadı. UBT global mutex beklemesi vardı.

Temiz UHT +6 unity module +lib/DLL Editor build exit0/Succeeded52,93sn.
RunAllChecks.ps1 -SkipBuild -FullCompile41/41 GECTI, exit0: normal build ayrı
adımda tamamlandığı için SkipBuild kullanıldı.19 CPP NoPCH/NoLink/SingleFile
/W4 compile OK, warning C0. SingleFile/*.cpp.rsp ve Shared.rsp'lerde /W4 ve
/Yu yokluğu ayrıca kontrol edildi;19 nonempty object üretildi.151 Source/Scripts
hash başlangıç/son eşit, tracked temiz; native paket boyunca linked DLL hash aynı:
1ffefb85dd6182805cd104489e9603d4018157b1c26b4a59e857ee08a2115fe1.

| Kontrol | Sabit snapshot sonucu (UTC) |
|---|---|
| Veri/negatif testler | 56 mob,53 item,86 görev,81 skill,12 efekt,29 spawner; ValidateData/StaticLint/14 veri +15 lint negatif temiz |
| Native parti / klan | 149 06:45:52.786 /138 06:46:07.944 PASS |
| Arkadaş / presence / save | 53 06:44:35.769 /24 06:44:27.655 /26 06:44:19.630 PASS |
| DoT / trade | 33 06:47:15.806 /218 06:48:41.650 PASS |
| Parser / JSON | 576/576; schema2, negative111 |
| Store kopya uyumluluğu | 489 yuva/489 load/489 JSON; invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1, roundtrip equal1 |
| Summer / Winter smoke | 52 /29 mob doğumu; exit0/error0 |
| NetSmoke | 1join/1welcome; server/client error0;094920 logları |

Ana Saved/SaveGames içindeki489 MMOSave kopyası byte/SHA eşitlikle izole Saved'e
okunarak aktarıldı; testler izole kopyalarda çalıştı. Bu489 gerçek eşzamanlı
oyuncu veya Lv30/Lv65 oynanış kanıtı değil. Ana projeye save yazılmadı.
TEMP/mmo_aggregate_9fdd2ea altında build.log, all_checks.log, observed_stdout.txt,
source_hashes.json, dll_frozen.json, save_import_hashes.json, verification_result.json,
19 response +shared response kopyası ve gerçek UE test/smoke/net logları korunur.
Host çıktıları parent tool stdout'tan ayrıca kaydedildi;19 ayrı tam UBT logu
üretilmedi. Worktree kaynaklarına test sırasında edit yapılmadı.

ECC plan/review/verify + ue-build/mmo-feature kullanıldı. Salt okunur Hilbert,
immutable W1/W2/R data diff'i için Unreal/QA/ekonomi fallback rollerinde blocker0:
yeni referanslar/ödüller/bantlar ve altı parametreli SpawnRift negatif test çapası.
Ajan test/build/edit yapmadı ve kapatıldı; Agency provenance mevcut değildi.

Bu, eski O37 paketinin aggregate revision belirsizliğini yalnız9fdd2ea snapshot'ı
için kapatır. Yeni beş mob/yedi görev otomatik veri/derleme kapsamına girer;
hepsinin gerçek sahnede görülmesi, yedi ödülün tek tek oyuncuyla teslimi, denge,
yeni havuzların seyrelmesi, render/Slate, gerçek uzak istemci, disk login,
Shipping/Server ve uçtan uca Lv1–30/Lv46–65 PIE doğrulaması açık kalır.
InitGameState whitelist ve Lv33+ skill-gap uyarıları devam eder.

Son kontrol:489 ana save dosyası import hash'ına hâlâ eşit (fark0);
save_source_postcheck.json kaydedildi. İzole testler sonrası engine/UBT0.
Managed worktree archive talebi gönderildi; log/response/hash kanıtları TEMP'te
korundu. Belge güncellemesi için oyun derlemesi tekrar çalıştırılmadı.


## 76 · Parti davet chat gönderen pawn/üyelik bağlamı (9 Ekim 2026, O38)

59723c8 tabanından izole managed worktree party-chat-invite-context'te
sınırlı düzeltme: /parti ve /parti davet üretimi SeenCommandPawn veya
SeenPartyMembershipId kullanmadığı için eski istek güncel pawn/grup adına
yeni davet/cooldown oluşturabiliyordu. Component/console issuing RPC değişmedi.

Önce20 yeni native assertion eski production ile gerçek CPP/lib/DLL build
40,27sn sonrası FAIL139/null pawn (07:04:33.463UTC); wrapper exit1, UE exit0.
Production dispatch öncesi valid/current pawn; aktif üyelikte valid/equal GUID,
solo durumda invalid GUID kapısı. Ek RPC/property/save alanı yok.
Normal CPP/lib/DLL5,08sn ve PASS16907:05:31.321UTC;169 =149 +20,
controller58 +diğer111. Ret snapshot'ı LastInviteTime dahil üç karakterin
pending source/time/nonce/context/roster/üyelik ve envanterini karşılaştırır.

Null/foreign pawn, gerçek possession replacement, solo stale group GUID,
aktif zero/foreign/random GUID, group→solo, replacement grup ve aynı üyeliğe
rejoin eski GUID retleri; fresh solo Say, grouped Enter, matching accept ve
fresh rejoin recovery pozitifleri vardır. Cooldown reset fixture detayıdır;
gerçek elapsed cooldown/replication/Slate testi değildir.

Controller CPP NoPCH/NoLink/SingleFile12,65sn Succeeded; /W4 ve /Yu yokluğu
response dosyalarında doğrulandı, warning C0.19 CPP FullCompile bu dar işte
tekrarlanmadı; önceki §75 sonucu önceki snapshot'a aittir.

Derlenmiş negatifler: yalnız grouped equality kapısı çıkarılınca4,83sn build
sonrası FAIL147/foreign membership07:08:07.828; tüm issuing isteklerini kesen
negatif4,69sn build sonrası FAIL143/fresh solo Say07:08:41.468. İkisinde
wrapper1/UE0; negatifler commit edilmedi. Exact reviewed byte backup write ile
restore +gerçek CPP/lib/DLL4,67sn Succeeded; bu DLL final pakete temel oldu.
Mutant hazırlığında global needle iki eşleştiği için ilk script assert ile
edit yapmadan durdu; o unmutated9,89sn build/PASS169 ayrı preflight loglarında,
negatif kanıt olarak kullanılmadı.

ECC plan/review/verify +mmo-feature/ue-build akışı; readonly Herschel
Unreal/QA/security fallback review blocker0, önce/son iki hash aynı; agent
build/test/edit yapmadı, kapatıldı. Agency plugin provenance yok.
Reviewed/final worktree raw byte SHA256:
- Controller:29da92a8748d5c648c5625fc9e351ea7333584b9a020a116016bac1b6a59a7f1
- Runner:d1eb5bb183849056264182a98a94eaf60c8286d03645d6336f65b1d453978348

Sınırlar: aynı pawn solo→group→solo invalid sentinel ile ayrışmaz;
parametresiz component/console MMOInvite stale context, leadership epoch,
recipient grup niyeti, hedef kick/rejoin, account rebinding ve UI atomikliği
ayrı açık. Nearest uygun olmayan ilk kişiyi atlamaz; kural değişmedi.
Gerçek remote RPC/PIE/disk login/Shipping/Server yapılmış sayılmaz.

Final izole RunAllChecks -SkipBuild40/40 GECTI, exit0: parti16907:12:13.341UTC,
klan13807:12:27.262, arkadaş53/presence24/save26, DoT33/trade218;
JSONschema2/negative111, parser576/576. Store496 loaded496/JSON496,
invalid/loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1,
roundtrip equal1. Summer51/Winter34 exit0/error0; Net1join/1welcome/error0,
101517 server/client logları.151 tracked source/runner hash aynı, linked DLL:
eaef3f18ddb7c3eab8249c12c8e9a7c810db2ad418d4daa5698872026f30e879.
Üç ignored Python bytecode cache oluştu; tracked kaynak freeze kapsamına girmez.
496 ana save byte-eşit kopyada test edildi; son kontrol bu496 orijinalde fark0.

Bu40/40 paket yalnız59723c8+O38 sabit worktree sürümüne aittir. Arada dış ajan
ana branch'e yeni içerik, ekonomi, DoT ve spawner commitleri ekledi; bunlara
eski paket sonucu mal edilmedi. bd98c36 tabanındaki ana checkout'a own iki
source/runner patch Git içerik eşitliğiyle taşındı; son gerçek controller +6
module/lib/DLL build10,17sn Succeeded, native16910:39:34.610UTC PASS/exit0.
Bu son ana çalışmada152 tracked source/runner SHA aynı. ValidateData/Lint exit0:
59 mob/57 item/90 görev/93 skill/12 efekt/35 spawner; mevcut Lv33+ gap uyarıları
var, InitGameState whitelist uyarısı dış0014f9c commitinde kaldırılmış.
Yeni ana snapshot üzerinde tam paket/Shipping/Server/gerçek PIE tekrar yapılmadı.

Dış merge eski §75 belge bölümünü kaldırmıştı; historical9fdd2ea kanıtı burada
geri eklendi, diğer güncel dış belge içeriği korundu. Son sonuçları gösteren
durum metni ve O38 açık PIE maddeleri güncellendi; belge sonrası oyun build yok.
TEMP/mmo_party_chat_invite_20261009: baseline/fixed/mutant/restore build+native,
NoPCH response ve log, reviewed_hashes/review_scope, source_hashes/dll_frozen,
save_import_hashes, all_checks, verification_result, primary_current_source_hashes,
primary_current_build/native/data/lint, primary_current_verification kanıtları.

## 77 · Hub'a dönen güçlü kaynak zayıf oyuncunun DoT'unu susturuyor (9 Ekim 2026, O39)

37d829c üzerindeki de99bf6 güçlü DoT koruması yalnız weak kaynağın varlığını
kontrol ediyordu. A güçlü Poison uygulayıp hedef mobun hub'ına girdikten sonra
B dışarıdan daha zayıf Poison tazelerse kaynak A/108 olarak kalıyordu;
MMOMob::TakeDamage hub kapısı her tick'i reddediyordu. B yeniledikçe süre
uzayabilir, izinli B'nin kendi 16 hasarlık DoT'u hiç uygulanmayabilirdi.

ECC plan/review/verify + mmo-feature/ue-build kullanıldı. Readonly Meitner
Unreal/QA/economy fallback, ebaf532/de99bf6/85952f9 incelemesinde bu ORTA
regresyonu buldu; donation atomicity ve TopUp için ek orta/yüksek bulgu yok.
Son iki dosyalık patch review blocker0; iki SHA başlangıç/son eşit. Agency
plugin kökeni yok; ajan dosya/test/build yazmadı ve kapatıldı.

ApplyEffect içindeki güçlü kaynak koruması artık hedef mobun gerçek hub
hasar koşulunu kullanır: oyuncu kontrollü kaynak, etkin hub, radius>0,
Dist2D<radius. Bu kaynak hub içinde ise mevcut yenileme dalı B'nin kendi
büyüklüğünü ve kaynağını birlikte yazar. Authority kapısı, replicated
ActiveEffects/EndServerTime ve broadcast yolu korunur; yeni RPC/save alanı yok.
Tam sınır ve kapalı hub güçlü A'yı korur. Ölüm/detach sonrası kaynak sahipliği
politikası bu düzeltmenin kapsamı dışında; per-pending-tick weak çözümleme aynı.

Native46=önceki38+8: A dışarıda uygulayıp içeri girince tick0; B'ye kaynak,
bPlayerSource, magnitude16 ve timer reset aktarımı; gerçek tick200→184;
tekrar tazeleme184→168; tam radius ve radius0 için A/108 korunması ve tick200→92.
Önceki PvP/party/clan/spawn/mob hub, lost-source, callback destruction,
Regen/expiry/defense ve ölüm reentrancy kontrolleri de geçti. Fixture world
zamanı/timer ilerletmez; ProcessEffects çağrıları elapsed PIE kanıtı değildir.

Sabit37d829c worktree'de temiz UHT/module/lib/DLL build40,60sn Succeeded;
fixture konumlarını netleştiren ikinci actual CPP/lib/DLL build3,72sn.
Eski üretim kodu yeni fixture'da FAIL checks7 / hub-blocked stronger source,
10:49:42.434UTC (runner1/UE0). Fix actual build3,67sn Succeeded;
PASS46 10:50:30.302UTC, runner/UE0. Derlenmiş eski üretim başarısızlığı
regresyon kanıtıdır; tüm testleri reddeden ayrı mutant bu işte çalıştırılmadı.

### O39 sabit entegrasyon paketi

RunAllChecks -SkipBuild -FullCompile42/42 GECTI, exit0. Bu sonuç yalnız
37d829c+O39 iki source/runner patch'inin birlikte bulunduğu sabit snapshot'a
aittir. 152 tracked Source/Scripts +5 Config +uproject =158 dosya SHA ve
linked DLL paket önce/son aynı. docs/Lore.md Git blob'u iki checkout'ta
37d829c ile aynı; GenQuestDialogue --check90 girdi eşleşti.
19 CPP NoPCH/NoLink/SingleFile, response/shared /W4 var /Yu yok,
19 nonempty object ve toplam warning C0. Bireysel UBT logları ayrı
saklanmadı; paket çıktısı ile19 response/shared ve object metadata kaydedildi.
Linked izole DLL:6d966d9a794199b835e626a6fb7196ccab334406055e28fc3f9dde056c7b162f.

Parti169 10:55:39.745UTC, klan138 10:55:53.990, DoT46 10:57:23.222;
arkadaş53/presence24/save26/trade218/vendor17/questCapacity13;
JSONschema2 negative111, parser576/576; StaticLint84 temiz,
ValidateData59 mob/57 item/90 görev/93 skill/12 efekt/35 spawner, mevcut
Lv33+ beceri boşluğu uyarıları var. Store553 loaded553/JSON553,
invalid/loadFailed/existsMismatch/jsonMismatch0,maxLevel6,writeorder1,
roundtrip saved1/loaded1/equal1.553 ana kayıt byte-eşit kopyada kullanıldı;
son native sonrası bu553 ana orijinalde hash fark0. Lv30+ gerçek kayıt
uyumu bu maxLevel6 örnekleriyle doğrulanmış sayılmaz.

Summer57/Winter75 doğum, exit0/error0. Net140031 server/client1join1welcome,
hata0. Bağlantı ve harita doğuşları test edildi; gerçek uzak gameplay/PIE,
UI/XP/loot kredisi, Shipping ve Server hedefleri açık. Güncel dış ekonomi,
DoT, spawner ve O38 kodu bu snapshot'ta birlikte derlendi/test edildi;
§75/76 tarihsel snapshot kanıtlarının kapsamı korunur.

İki patch ana checkout'a Git-normalized diff byte eşitliğiyle taşındı;
ana actual Stats+Module.5/lib/DLL build8,99sn Succeeded, native46
11:03:12.377UTC PASS/exit0.158 input ve ana DLL SHA sabit. Belge sonrası
oyun build başlatılmadı. TEMP/mmo_dot_hub_refresh_20261009 altında baseline,
fixed/primary build+native, frozen_inputs, nopch_proof/response, all_checks,
review_scope, verification_result/primary_verification ve save hash kanıtı var.

## 78 · Takas davet chat gönderen pawn bağlamı (9 Ekim 2026, O40)

bb4ab1c üzerinde bare `/trade`, SeenCommandPawn doğrulaması yapmadan güncel
pawn'ın RequestInviteNearest yolunu çağırıyordu. A adına geciken istek,
aynı controller B'yi possess edince B'nin3sn invite cooldown'unu tüketebilir;
uygun yakın hedefe B kaynaklı yeni pending kimliği/zamanı üretebilirdi.
Issuing tek başına altın/eşya transferi yapmaz. Readonly Bacon fallback
Unreal multiplayer/QA incelemesi ORTA bulguyu doğruladı; Agency kökeni yok.

Yalnız chat issuing dalına dispatch öncesi IsValid/current pawn kapısı eklendi.
Ortak chat throttle retleri sayar; trade cooldown/pending mutasyonu başlamaz.
Authority, recipient nonce, session/revision, nearest seçimi ve ekonomi
kuralları korunur. Yeni RPC/reflected/save alanı yok. Non-Shipping friend,
native fixture'ın server-only state snapshot'ına erişimini sağlar.

Native230=önceki218+12; controller chat20. Null/foreign/retired pawn retleri,
aktif takasta cooldown koruması, gerçek replacement possession, fresh Say
ve Enter issuing, kabul ve iptal kontrol edildi. Üç current ve retired
component için pending kimliği/kaynak/zaman, cooldownlar, partner/session,
offer/revision, timer activity ve envanter snapshot'ı retlerde değişmedi.
Enter doğrudan handler çağrısıdır; physical Slate/remote RPC testi değildir.
Fixture aynı frame'de çalışır; elapsed expiry veya gerçek ağ gecikmesi ölçmez.

Temiz izole Editor UHT/compile/lib/DLL42,47sn Succeeded. Eski üretim ile
yeni fixture FAIL checks201/check202, null issuing state;11:20:40.244UTC,
runner1/UE0. Guard actual CPP/lib/DLL4,77sn; PASS23011:21:34.885UTC.
İlk genel paket StaticLint'te fixture için TimerManager.h eksikliğini yakaladı;
paket durduruldu, aborted_missing_include.log saklandı. Explicit include
eklendi, actual compile/link10,38sn Succeeded ve yeni freeze ile paket baştan
çalıştırıldı. Drop-all mutant çalıştırılmadı. Bacon son üç dosya review
actionable0; include sonrasında parent diff incelemesi ve kontroller geçti.
Ajan test/build/dosya yazmadı; paket öncesi kapatıldı.

### O40 sabit entegrasyon paketi

RunAllChecks -SkipBuild -FullCompile42/42 GECTI, exit0; yalnız bb4ab1c+üç
source/runner patch snapshot'ına aittir.159 tracked Source/Scripts/Config/
uproject/docs/Lore.md input + linked DLL =160 SHA önce/son aynı.19 CPP
NoPCH/NoLink/SingleFile, response/shared /W4 var /Yu yok,19 nonempty object,
warning C0; response kopyaları ve object SHA/size saklandı. Bireysel UBT
logları ayrı kaydedilmedi; paket çıktısı saklandı.

Trade23011:35:52.040UTC; parti169/klan138/DoT46, arkadaş53/presence24/save26,
vendor17/questCapacity13, JSONschema2/negative111 ve parser576/576 geçti.
Lint84 temiz; Data59mob/57item/90görev/93skill/12efekt/35spawner; mevcut
Lv33+ beceri boşluğu uyarıları sürüyor. Store553loaded/JSON553, invalid/
loadFailed/existsMismatch/jsonMismatch0, maxLevel6, writeorder1/roundtrip
equal1.553 ana orijinal save byte-eşit kopyalandı; paket ve ana native
sonrasında orijinal SHA fark0. Gerçek Lv30+ save uyumu bu örnekle kanıtlanmaz.
Summer56/Winter75 doğum, exit0/error0; Net1join1welcome/hata0.

Ana checkout'a aynı Git-normalized üç dosya patch taşındı; actual Editor
compile/lib/DLL12.58sn Succeeded, native230 PASS.159 input+ana linked DLL
SHA aynı. TEMP/mmo_trade_issuer_20261009: baseline/fixed/final_include/primary
build+native, aborted log, full_checks, freeze/check, primary-freeze/check,
verified.patch, original_saves, responses/nopch_proof ve review.md kanıtları.
Belge sonrası oyun derlemesi başlatılmadı.

Gerçek host+uzak istemci PIE/HUD/replication, Shipping/Server ayrı açık.
Component/console issuing RPC, aynı-pawn possession/session roundtrip ve
görülen nearest hedef niyeti bu chat pawn düzeltmesinin dışında kalır.

## 79 · Parti kick gönderen üyelik bağlamı (9 Ekim 2026, O41)

5d101fb üzerinde ServerKick(Target) yalnız güncel lider/roster kontrolü yapıyordu.
Eski üyelikte verilen istek, gönderen ayrılıp aynı çifti yeniden kurduğunda veya
devam eden session'a lider olarak geri katıldığında yeni üyelikte hedefi çıkarabiliyordu.
Readonly Banach fallback Unreal multiplayer/güvenlik QA, ecc-mmo-review ile ORTA
bulguyu doğruladı; Agency plugin kökeni yok. Gerçek uzak RPC sırası yeniden üretilmedi.

RequestKick(Target) mevcut çağrıları koruyarak kişisel PartyMembershipId yakalar;
RequestKickSeen ve reliable ServerKick bu görülen GUID'i taşır. Sunucu handler'ı
önce authority ve geçerli/eşit gönderen üyeliğini, ardından mevcut lider/hedef
kurallarını doğrular. Mevcut owner-only üyelik replikasyonu kullanılır. Yeni
reflected/save alanı yok; RPC payload değiştiğinden UHT ve eşleşen client/server
binaries gerekir. AMMOCharacter::MMOPartyKick çağrısı değişmedi.

Native185=169+16. Reform edilmiş çift ve aynı-session source rejoin eski isteği,
invalid/foreign/random ID, null/self hedef, nonleader, duplicate/solo niyeti
retleri; fresh explicit ve mevcut wrapper kick pozitifleri kontrol edildi.
Retler dokuz aktörün party/pending/context/cooldown/envanter snapshot'ını korur;
fresh çıkarma survivor session/membership ve envanterlerini korur. Hedefin kendi
rejoin nesli veya continuous leadership/ownership epoch bu değişiklikte taşınmaz.

Eski üretim endpoint'i değişmeden yeni fixture derlendi: adapter Seen niyetini
bilerek atıp eski ServerKick(Target) çağırdı; FAIL102 11:48:30.014UTC,
runner1/UE0. Fix adapter RequestKickSeen'e aktarır: actual UHT/compile/link11,51sn,
PASS185 11:50:05.566UTC. Unconditional return negatif denemesi C4702 ile
derlenmedi (exit6), bunun için native sonuç iddiası yok. Runtime GetOwner return
negatifi gerçekten derlendi3,89sn; fresh kick FAIL109 11:51:56.201UTC,
runner1/UE0. Exact fixed CPP byte-eşit geri yüklendi; final actual CPP/lib/DLL
3,92sn Succeeded. Final paket native185 11:58:54.373UTC PASS.

### O41 sabit izole doğrulama

RunAllChecks -SkipBuild -FullCompile42/42, exit0; yalnız 5d101fb+O41 üç
source/runner patch snapshot'ına aittir.159 tracked Source/Scripts/Config/
uproject/docs/Lore.md input+linked DLL=160 SHA önce/son aynı.19 CPP NoPCH/
NoLink/SingleFile /W4, /Yu yok;19 nonempty object, warning C0. Response kopyaları
ve object SHA/size saklandı; bireysel UBT logları ayrı saklanmadı.

Parti185/Trade230/klan138/DoT46, arkadaş53/presence24/save26/vendor17/
questCapacity13, JSONschema2/negative111 ve parser576/576 geçti. Lint84 temiz;
Data59mob/57item/90görev/93skill/12efekt/35spawner; mevcut Lv33+ beceri boşluğu
uyarıları sürüyor. Store553 loaded/JSON553, mismatch0/maxLevel6/writeorder1/
roundtrip equal1.553 ana save byte-eşit kopyalandı; paket sonrası ana ve izole
orijinal SHA fark0. Lv30+ gerçek save/login bu örnekle doğrulanmaz.
Summer57/Winter78 doğuş, exit0/error0; Net150534 server logunda2 "Join succeeded"
eşleşmesi/client1welcome/error0. Test istemcisinin welcome saati12:06:17.684UTC
ilk server login/join ile eşleşir.12:06:43.774UTC ikinci login de görüldü;
bu ek bağlantının kaynağı doğrulanmadı, iki ayrı uzak istemci test iddiası yok.

Ana checkout'ta dış ajan satıcı/ekonomi/HUD dosyalarına yazmaya başladığı için
O41 codex/o41-party-kick-membership dalında doğrulandı. Ana kaynaklar/DLL için
bu izole paket sonucu kullanılmaz; ortak checkout entegrasyonu ve yeni aggregate
derleme/test bekler. Yalnız bu dalın sekiz dosyası commit kapsamındadır.
TEMP/mmo_party_kick_sender_20261009: baseline/fixed/negative/restored build/native,
full_checks, freeze/check, original_saves, reviewed_hashes/review.md,
responses/nopch_proof kanıtları. Belge sonrası oyun build başlatılmadı.

Gerçek host+uzak istemci PIE, RPC serialization/owner-only teslim/HUD/XP,
Shipping/Server açık. Hedef kick/rejoin, authority epoch, roster/metin/ID
atomikliği; component davet issuing ve solo roundtrip ayrı açık kapsamlar.

### O41 güncel entegrasyon doğrulaması (9 Ekim 2026)

ee2e61f (e2a3451 satıcı bayrağı dahil) +200cc8e O41 + port ve watchdog betikleri birlikte
gerçek izole Editor UHT/compile/lib/DLL14,18sn Succeeded. NetSmokeTest artık
isteğe bağlı -Port, RunAllChecks -NetPort taşır. Sonradan gelen a1897b8 boş
rastgele port seçimi ve gerçek dinleme portu kontrolü korundu; parametre
verilmezse otomatik boş port, verilirse doğrulanmış açık port kullanılır.
İki AST errors0;0/65536 dört parameter validation ret exit1, script
gövdesi/UE başlatılmadan. UDP17779 boşken ayrı TEMP ile tam paket çalıştırıldı.
Caller'lar farklı port ve TEMP seçmelidir; aynı saniye/aynı TEMP log adı riski
bu değişiklikte otomatik giderilmedi. O41 reliable RPC payload için eşleşen
client/server binaries gerekir; gerçek uzak serialization/ordering hâlâ açık.

`RunAllChecks.ps1 -SkipBuild -NetPort 17779`: 41/41, exit0. Sonrasında yalnız NetSmoke
çalışma portu değişkeni düzeltildi; aşağıdaki ek smoke kapsamı bu son betiğe aittir.
159 tracked Source/Scripts/Config/uproject/docs/Lore.md input+linked DLL160 SHA,
son betik freeze'inden ek smoke ve kayıt kontrolü sonuna kadar aynı.
Önceki ilk koşuda19 NoPCH/NoLink/SingleFile /W4, /Yu yok;19 object/warning C0;
response kopyaları/object SHA-size var, bireysel UBT logları ayrı saklanmadı.
Parti185/Trade230/klan138/DoT46/vendor18/questCapacity13, arkadaş53/presence24/
save26, JSONschema2/negative111 ve parser576/576 geçti. Data59mob/57item/
90görev/93skill/12efekt/35spawner;Lint84 temiz, Lv33+ beceri boşluğu uyarıları var.
Store582 loaded/JSON582, mismatch0/maxLevel6/writeorder1/roundtrip equal1.
561 ana orijinal save byte-eşit kopyalandı; ilk izole koşudan kalan ek test
yuvaları store sayımında var. Bu561 ana/izole orijinal SHA paket ve ana
native sonrasında aynı. Lv30+ gerçek save/login bu maxLevel6 örnekle kanıtlanmaz.
Summer57/Winter77 spawn/error0; Net port17779 server1join/client1welcome/error0.
Bu bağlantı smoke'u gerçek kick/HUD/XP gameplay testi değildir.

Readonly Boole fallback Unreal multiplayer/ekonomi QA, ecc-mmo-review ile
aggregate kaynak incelemesi actionable0. Lorentz watchdog ve Russell
port birleşimi (selectedPort düzeltmesi öncesi) incelemesi actionable0. Agency kökeni yok; ajanlar
build/test/yazım yapmadı ve sonuçları alındıktan sonra kapatıldı.

İlk FullCompile paket41/42, exit1: timeout fixture accepted/dropped/ordered
true, winterFalse;20sn tüm motor açılışı ve Winter yüklemesine yetmedi, log
StartLoadingDestination Winter sırasında sona erdi. initial_checks_failed.log
ve timeout_deadline20_failed.log saklandı; o ilk160 input/DLL SHA da sabit.
Yalnız TestZoneTravelTimeout toplam watchdog60sn yapıldı;native fixture'daki
0,75sn save timeout, normal15sn süre, timeout/retry sırası ve gerçek Winter yükleme predicates
değişmedi. İkinci paket yeni freeze ile baştan41/41 geçti; FullCompile ikinci
kez koşulmadı. İki freeze arasında tek fark bu test betiği; bütün C++/DLL
SHA aynı olduğundan önceki19 NoPCH kanıtı güncel C++ snapshot'ına aittir.
a1897b8 port değişikliği ardından iki port betiği birleştirildi ve üçüncü
tam41/41 paket yeni freeze ile çalıştırıldı. Ardından parametresiz smoke'da
ValidateRange uygulanmış Port değişkenine sıfır atama hata verdi; UE bağlantısı
geçse ve runner exit0 dönse de bu koşu temiz sayılmadı (default_assignment_error.log).
Çalışma portu ayrı selectedPort değişkenine taşındı. Yalnız NetSmoke betiği
değişti, C++/DLL ve diğer159 freeze girdisi aynı. Son betikle açık port17779
ve parametresiz rastgele port ayrı ayrı gerçek smoke exit0/hatasız geçti;
0/65536 ve dolu port başlatma öncesi reddi yeniden doğrulandı. Tam41 paket
bu tek betik düzeltmesinden sonra tekrarlanmadı. Önceki başarılı paket
pre_latest_checks_passed.log; third_package_freeze.json, final_port_delta.json
ve latest_random_net/latest_explicit_net.log kapsamı ayrı kaydeder.

Doğrulanmış aggregate 0405318 ile ana master'a fast-forward edildi;
git-normalized source snapshot aynı. Ana actual UHT/compile/lib/DLL10.19sn
Succeeded; nativeParty185 ve Vendor18, Data/Lint exit0. Ana159 input+DLL SHA
sabit. Yalnız bu kaynak snapshot'ı ve ilgili native kapsam doğrulandı; ana
checkout'ta tam41 adım ikinci kez çalıştırılmadı. Önceki izole/eskiden dış
yazım bekleme kanıtı yukarıda tarihsel kapsamıyla korunur.
TEMP/mmo_o41_integration_ee2e61f_20261009: aggregate/primary build ve native,
aggregate_checks, freeze/check, primary-freeze/check, original_saves,
responses/nopch_proof, port_validation, review.md ve integration.json.
Belge sonrası oyun build başlatılmadı. Gerçek PIE/remote/HUD/XP, hedef kick
rejoin/authority epoch/roster atomikliği, Shipping/Server açık kalır.

## 80 · Paralel ağ smoke log kimliği ve boşluk içeren TEMP (9 Ekim 2026, O42)

NetSmokeTest yalnız HHmmss ile log adı üretiyordu. Aynı saniye/aynı TEMP,
farklı port koşularında server/client yolları eşleşiyor; kanıtlar birbirini
ezebilir veya başka koşunun bağlantısını okuyabilir. Eski gerçek initializer
iki çağrıda aynı iki yolu verdi; bu bir isim üretim fixture'ıdır, eşzamanlı
eski UE bağlantısının başarısızlığı olarak sunulmaz. Tarih/milisaniye ve her
koşuya yeni GUID eklendi. Server/client ABSLOG argümanları tırnaklandı.

Boşluk içeren TEMP ile ilk gerçek iki server denemesinde tırnaksız ABSLOG
beklenen logları üretmedi; iki listening kontrolü KALDI. Dış test wrapper'ı
script exit kodunu taşımadığından exit0 dönmüştü; bu deneme başarısız sayıldı.
Wrapper kod taşıması ve kendi -File yolunun tırnaklaması kanıt sürücüsünde
düzeltildi. Üretim NetSmoke başarı/ret, port seçimi, dinleme, süre, cleanup
ve join/welcome/error kuralları değişmedi.

Son betik: 1000 initializer çağrısı/2000 benzersiz yol, AST errors0.
İzole b0be7fe+yalnız bu betik patch'i, ortak boşluklu TEMP ve aynı başlangıç
saniyesinde iki Summer listen-server/client koşusu: 17779 ve17780, dört ayrı
log, her koşu native exit0/server1join/client1welcome/iki taraf error0.
Her server kendi portunu dinledi, client kendi portuna bağlandı; stderr boş.
Parametresiz Winter koşusu rastgele17056: exit0/1join/1welcome/error0,
stderr boş. Bu üç bağlantı smoke'u gerçek kick/takas/HUD/remote RPC PIE değildir.

Ana ve izole159 tracked input+DLL160 SHA son test snapshot'ında sabit;
ana checkout'taki başlangıç575 .sav SHA aynı. C++/asset değişmedi, ana
matching DLL (BF61EE8B…) izole checkout'a kopyalandı; yeni UBT/FullCompile
ve tam41 RunAllChecks yapılmadı. Ana ValidateData/Lint exit0,84 kaynak temiz;
mevcut beceri boşluğu uyarıları sürer. Belge sonrası oyun build başlatılmadı.

Kierkegaard kimlik, Parfit final ABSLOG diff'ini salt okunur fallback
Unreal multiplayer QA/ecc-mmo-review ile inceledi: actionable0. Build/test
ve yazım yapmadılar, kapatıldılar; Agency paket kökeni yok.
Kanıt: TEMP/mmo_o42_net_log_identity_20261009 içinde baseline_log_paths,
fixed_2000_log_paths, unquoted_temp_failed*, parallel_results/native_validation,
default_validation, snapshot/check, AST, Data/Lint ve review.md.
NetSmoke logları ortak TEMP'te ayrışır; aynı checkout'taki save/global UE
kaynakları izole edilmiş sayılmaz. Tam RunAllChecks'taki diğer sabit TEMP
dosyaları için ayrı TEMP/checkout gereği sürer. §79 gerçek PIE ve
Shipping/Server kontrolleri açık kalır.

## 81 · Atlanan first_* tekrarları ölçüm süresini uzatıyor (9 Ekim 2026, O43)

PlaytestMetrics.py doğrulanmış first_* marker'ında record_time çağrısını
SID/action tekrar kapısından önce yapıyordu. Sonraki marker atlanmış sayılıp
ilk payload korunmasına rağmen last_t ve açık seviyenin süresi büyüyordu.
Bir adversarial log örneğinde Lv2 gerçek kabul edilen gözlem aralığı2sn;
aynı SID/action'ın t=1000 tekrarından sonra eski rapor990sn gösterdi.
Bu offline ölçüm hatasıdır; sunucuda XP/altın çoğaltma veya RPC exploit
kanıtı değildir. Normal emitters bir controller action'ını bir kez üretir.

Saat güncellemesi tekrar reddinden sonraya taşındı. SID normalizasyonu,
geçersiz payload/clock doğrulaması, ilk payload, duplicate sayacı ve farklı
SID/action kabulü korunur. Sekiz first action × üç sıra: geç tekrarı sona
ekleme, normal levelup/kill kuyruğu öncesine ekleme, yalnız first marker'ları.
Her durumda kabul edilen payload/session ve gözlem/level/XP/quest/altın
aggregate'ları karşılaştırılır; ek49 assertion var.

Başlangıç eski fixture576/576 geçti. Yeni625 assertion eski parser'la24
saat/aggregate hatası ve exit1 verdi (baseline_regression_failed.log).
Son betik625/625 exit0; gerçek CLI örneği aynı ilk payload ve duplicate1
koruyarak Lv2'yi990sn'den2sn'ye düzeltir. Native validator adversarial
fixture'ları aynı paket içinde çalışır; bunlar Python log kontrolleridir,
Unreal process veya PIE çalıştırmaz. ValidateData ve StaticLint exit0,
84 kaynak temiz; negatif Data14/Lint15 senaryosu geçti.

Önceki O42 snapshot'ına göre158 diğer input/DLL ve575 ana .sav SHA aynı;
iki değişen betik test freeze'inden son kontrole kadar byte-eşit.
Noether salt okunur fallback QA/ecc-mmo-review: actionable0, kendi test/build
ve yazımı yok, kapatıldı; Agency kökeni yok. C++/asset/RPC/save değişmedi;
UBT/FullCompile, Summer/Winter/NetSmoke ve tam41 paket çalıştırılmadı.
Gerçek Lv1–30 PIE süresi/model karşılaştırması ve §79 gameplay kontrolleri
açık kalır. Kanıt: TEMP/mmo_o43_metrics_duplicate_clock_20261009 içinde
baseline/fixed test logları, baseline parser, duplicate_sample.log,
baseline_report/fixed_report, timing_probe.json, freeze ve final_result.

## 82 · İlk gerçek XP seviye geçişi ve reentrant refill (9 Ekim 2026, O44)

Lv1–30 telemetri işindeki `first_levelup` tamamlandı: gerçek authoritative
`GainExperience -> HandleLevelUp` yolunda, seviye ve puanlar artırıldıktan
sonra controller GUID/action başına tek marker. `from=1..79`, `to=from+1`,
`lv=to=2..80`; çok seviyeli ödülün yalnız ilk bitişik adımıdır. Kayıt yükleme,
doğrudan Level düzenleme, ölü/capped/eşik altı oyuncu ve NPC hariçtir.
Pawn değişimi ve `SeamlessTravelTo` controller kopyası tekilliği korur.
Yeni gameplay RPC/replicated/save alanı, Tick, timer veya oyun kuralı yok.

İlk uygulamada hook refill'den sonraydı. Hooke salt okunur ecc-mmo-review
P2 buldu: health/mana callback'i 1→2 sırasında XP verirse içteki 2→3 ilk
marker'ı tüketiyordu. Hook tüm refill/level delegate'lerinden önce taşındı;
mevcut gameplay/delegate sırası aynı kalır. Native fixture'daki güçlü GC
referanslı, tek çağrılı dynamic health listener gerçek iç içe XP verir.
Eski yerleşim gerçek UBT13.77sn build sonrası check16 FAIL ve yanlış
fifth marker `lv=3 from=2 to=3` verdi. Exact restore kopyası timestamp'i
koruduğu için ilk UBT up-to-date sonucuna doğrulama denmedi; timestamp
yenilenip Stats gerçekten derlenerek DLL link edildi: Succeeded3.70sn.
Düzelmiş native16/5, fifth `lv=2 from=1 to=2`, ortak parser PASS.

Python parser ASCII tam sayı/komşu geçiş/`lv==to` doğrular; float yuvarlaması,
taşan/Unicode/bozuk alanlar, clock/scope/SID, tekrar ve eski loglar sınandı.
Normal progression/XP/kill/gold/quest toplamını artırmaz; O43 duplicate saat
kapısı korunur. Fixture1140/1140 assertion geçti (önceki625'in tamamı dahil).
Locke yalnız dört parser dosyasını yazdı; test/build/UE çalıştırmadı.
Hooke takip incelemesi actionable0; iki ajan kapatıldı, Agency kökeni yok.
Test listener'ının reflected tipi Shipping'de kalır; test akışı ve telemetry
hook'u Shipping dışında derlenir. Shipping/Server hedef build yapılmadı.

İzole `RunAllChecks.ps1 -SkipBuild -NetPort 17779`: 42/42 adım, exit0.
Data/Lint86 temiz; negatif Data14/Lint15 geçti. Summer/Winter smoke native
exit0/error0, sırasıyla56/78 mob doğumu. NetSmoke script exit0: aynı17779
sunucu1join/istemci1welcome, iki logda error0; bu bağlantı testi progression
RPC/HUD doğrulaması değildir ve süreçler betik tarafından sonlandırılır.
Mevcut izole Store adımında slots0: eski .sav okuma/JSON/gidiş-dönüş atlandı,
writeorder geçti; bu adım mevcut kayıt uyumluluğu kanıtı olarak sunulmaz.

Ana checkout'a doğrulanmış15 dosya entegre edildi. İlk ana UBT exit6,
MSVC C3859/C1076 ve Windows1455 (disk belleği limiti) verdi. Yerel engine
BuildConfiguration.cs'teki mevcut `-MaxParallelActions=1` kullanılarak
tek action ile gerçek derleme/link18.87sn Succeeded/exit0; sistem ayarı
değiştirilmedi. Ana native16/5+parserPASS, Python1140/1140, Data/Lint86 exit0.
Ek ana Store native:575/575 yükleme, JSON575 eşit/mismatch0 ve geçici
slot Save/Load alan eşitliği geçti; geçici slot başlangıçta yoktu ve temizlendi.
575 başlangıç ana .sav SHA sabit, yeni/eksik yuva yok.11 owned kaynak/betik
izole freeze ile byte-eşit; diğer ana girdiler başlangıç byte'larını korur.
113 önceden var olan CRLF/LF checkout farkı newline normalize edilince
aynıdır; bunun için ana dosyalar topluca yeniden yazılmadı. İzole source/DLL
test boyunca sabit. `-FullCompile`/NoPCH ve Shipping/Server build yapılmadı.
Gerçek uzak progression/replication, pawn destroy Blueprint callback'i,
tam ServerTravel/reconnect ve 20 dk süre/model koşusu PLAYTEST_CHECKLIST'te açık.
Kanıt: TEMP/mmo_o44_first_levelup_20261009 içinde build/native/negative,
parser ve aggregate logları, source/DLL freeze, main575 sav SHA ve review.
