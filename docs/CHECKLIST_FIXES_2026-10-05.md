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
