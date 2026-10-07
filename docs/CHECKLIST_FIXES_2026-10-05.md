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
