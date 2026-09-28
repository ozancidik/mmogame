# Oynayarak Test Kontrol Listesi

Bu dosya, Claude'un otonom (sen yokken) geliştirdiği özelliklerin **senin PIE'de test etmen** gereken maddelerini biriktirir.
Her özellik derlenip commit'lendiğinde buraya eklenir. Test edip onayladıkça maddeleri işaretle (`[x]`).

> Not: Claude kodu yazar, derler ve derlendiğini garanti eder. Oynanış/his/görsel doğrulaması sana kalır.

---

# ÖNCE BUNLARI TEST ET (2026-09-18 durumu — dosya 1500+ satır oldu, en kritik/en riskli 6 madde)

Bu dosyada biriken onlarca maddeden gerçekten önemli olan, sırayla test edilmesi gereken 6 madde:

1. **[EN ÖNEMLİ, YENİ] Karakter oluşturunca/öldükten sonra dogus noktasinda güvende misin?** Spawner'ları haritaya sığdırma düzeltmesi (madde 2) bir yan etki yarattı: Lv8-12/Lv16-22 mob bölgeleri dogus noktasına aggro menziliyle ulaşabiliyordu — taze bir Lv1 karakter 2-3 vuruşta ölüp aynı yere yeniden doğup ölüm döngüsüne girebiliyordu. Commit `fd13bd9` ile aggro menzilleri ve spawner yarıçapları küçültüldü, Şifacı Lyra güvenli tarafa taşındı (python ile ≥300u güvenlik payı doğrulandı, ama PIE'de HİÇ görülmedi). Yeni bir karakterle oyuna gir, birkaç dakika dogus noktasında/çevresinde bekle → hiçbir canavar sana saldırmamalı; sonra öl ve yeniden doğ → aynı şekilde güvende olmalısın.
2. **Spawner'lar artık haritanın içinde mi?** Haritanın (Summer_scene) gerçek zemini sadece 4000x4000u — 10 spawner'dan 6'sı bunun DIŞINDAYDI (oynanamaz içerik). İKİ kez düzeltildi (commit `2ca4d7a` + `0353f01`, matematiksel olarak doğrulandı ama PIE'de HİÇ görülmedi). Sırayla Golge Suikastçı/İkinci-Üçüncü-Dördüncü bölge/Buz Lordu/Magma Tiranı spawner'larına yürü → hepsi zemin ÜZERİNDE, boşluğa düşmeden görünmeli. Biri bile boşluktaysa o zincirin tüm görevleri kilitli demektir.
3. **Lv80 capstone skilleri artık atılabiliyor mu?** Warrior/Archer'ın Lv80 nihai skilleri (400 mana) matematiksel olarak atılamıyordu (mana tavanı yetersizdi) — `ManaPerLevel` 5.5'e çıkarıldı. Yüksek seviyeye çık (`MMOGainXP`), capstone'u öğren, kullan → "Yeterli mana yok" almamalı.
4. **XP eğrisi artık makul mü?** Eskiden Lv80'e ulaşmak ~2.66 MİLYON kill gerektiriyordu (imkansız). Artık Lv25'e kadar aynı, sonrası doğrusal (~8.251 kill toplam, bağımsız QA ile de doğrulandı). Birkaç canavar öldürüp XP barının makul hızda ilerlediğini gözle kontrol et.
5. **Kademeli yanma + MaxPlus 8 doğru hissettiriyor mu?** +0-+2 güvenli, +3'ten itibaren yanma riski artıyor, +8 tavan. Bir item'ı upgrade ederek dene.
6. **Skill kitabı artık 2 sütunlu — okunabilir mi?** 57 skil'e çıkınca eski tek-sütun tasıyordu, artık ağaçlar 2 sütuna dengeli dağıtılıyor. `K` ile aç, taşma/üst üste binme var mı bak.

Aşağıdaki tarihli bölümlerin çoğu bunlardan ÖNCEKİ, daha küçük özellikler için — bu 6 madde onaylanmadan diğerlerine geçmene gerek yok.

---

# QA DENETIMI: kademeli yanma (170f87c) + 57 skill (2e401c2) — 6 bulgu (2026-09-14)

> QA gecisi. Derleme ZORLA yeniden dogrulandi (4 degisen .cpp dosyasinin zaman damgasi
> guncellenip tam derleme yapildi): **`Result: Succeeded`, sifir uyari.**
>
> **SAGLAM cikanlar (bunlari test etmene gerek yok, kodda dogrulandi):**
> Yanma roll sirasi DOGRU — `GetUpgradeChancePct(Inst.Plus)` ve `GetBurnChancePct(Inst.Plus)`
> ayni indeksi (deneme ONCESI arti) kullaniyor ve yanma dali `else if` oldugu icin `Inst.Plus++`
> calismadan degerlendiriliyor, **off-by-one yok**. Sunucu-otoriterlik saglam: `TryUpgrade` yetki
> yoksa `ServerTryUpgrade`'e yonleniyor, `RandRange` yalnizca sunucuda donuyor, `ClientUpgradeResult`
> sadece sonucu bildiriyor. Array sinirlari saglam: iki tablo da `[MaxPlus]` boyutunda + `<0 || >=MaxPlus`
> guardli, `TryUpgrade` zaten `Plus >= MaxPlus`'i eliyor — **out-of-bounds imkansiz**. Yanan item'da
> dangling-reference yok (`BurnedId`, `RemoveAt`'ten ONCE kopyalaniyor). HUD "YANMA %X" etiketi
> TryUpgrade ile **birebir ayni** cagriyi kullaniyor, uyumlu. Skill tarafinda: 57 skill, **duplicate
> SkillId/isim YOK**, tum `AppliedEffectId`'ler mevcut 6 efektin (Poison/Slow/Regen/Rage/IronSkin/Haste)
> icinde — hayalet efekt referansi YOK, **hicbir skilde `RequiredMasteryRank > RequiredLevel` YOK**,
> her agacta rutbeler 1,2,3... bosluksuz ve minimum seviye rutbeyle birlikte monoton artiyor →
> **"hicbir zaman acilamayan skil" YOK**. `Special3` (enum 4) < `MasteryTreeCount` (5) ve HUD
> `Trees[5]`/`TreeNames[5]` ile uyumlu. Skill puani ekonomisi rahat (en pahali sinif Warrior 14 rutbe
> istiyor, Lv80'de 158 puan var).

- [x] **FIXED (bkz. altta yeni commit) — BULGU #1 — HIGH — 6 yeni buff skili, Lv3-4'teki atasinin BIREBIR AYNISI (kesin downgrade).**
  `MMOSkillComponent.cpp::ExecuteSkill` icinde `BuffSelf` dali sadece `ApplyEffect(Def.AppliedEffectId, Caster)`
  cagiriyor — **`Def.Power` tamamen yok sayiliyor**, siddet duz olarak `MMOEffectDatabase`'den geliyor.
  Ustelik `MMOStatsComponent.cpp:492-503` ayni EffectId'yi **yigmiyor, sadece sureyi tazeliyor**.
  Sonuc: su 6 yeni skil oyun-ici olarak eski skille TAMAMEN ayni seyi yapiyor, sadece 8-10 kat mana
  ve daha uzun cooldown ile: `War_BloodLord` (Lv33, 145 mana) = `War_BattleShout` (Lv3, 15 mana);
  `War_SteelArmor` (Lv45, 120) ve `War_Unbreakable` (Lv58, 150) = `War_IronSkin` (Lv4, 15);
  `Arc_ShadowStep` (Lv16, 70) ve `Arc_WindsBreath` (Lv33, 145) = `Arc_Sprint` (Lv3, 12);
  `Pri_ResiliencePrayer` (Lv58, 150) = `Pri_Renew` (Lv3, 18).
  Yani **Warrior'in butun "Ustalik 3" agaci (3 skilden 2'si) hicbir sey katmiyor.**
  (`Pri_HolyShield` istisna — Priest'in daha once IronSkin kaynagi yoktu, o gercekten yeni.)
  **Test:** Warrior ile Lv58'e cik, once `War_IronSkin` (15 mana) bas → C ekraninda Defense artisini not al.
  Efekt bitince `War_Unbreakable` (150 mana) bas → **Defense artisi birebir ayni cikmali**. Cikiyorsa bulgu dogrulandi.
  **Yon (kodu mmo-developer yazar):** ya skil basina magnitude override (`FMMOSkillDef`'e alan) ya da
  ayri efekt ID'leri (`IronSkin2`/`Rage2` gibi) gerekiyor; su haliyle 6 skill bos.

- [x] **FIXED (bkz. altta yeni commit) — BULGU #2 — MEDIUM — Mana tavani seviyeyle BUYUMUYOR; yeni Lv24+ skillerin cogu klasik build'de kullanilamaz.**
  `MMOStatsComponent.cpp:159`: `MaxMana = 25 + Intelligence * ManaPerIntelligence` (=5). `MaxHealth`'in aksine
  **seviye terimi YOK**. Baslangic INT=12 ile MaxMana = **85** ve INT'ye puan basmayan bir karakterde Lv80'de de 85 kalir.
  Sonuc: Warrior'in Lv24'te actigi ILK yeni skil `War_TempestSlash` (95 mana) daha ilk gunden **kullanilamaz**.
  Capstone'lar 320-420 mana istiyor → INT 75-79 gerekiyor = **63-67 stat puani** (Lv80'de toplam 237'nin ~%28'i),
  Warrior/Archer icin baska hicbir ise yaramayan bir stata. Mage/Priest dogal olarak INT bastigi icin onlarda sorun hafif.
  **Test:** `MMOGainXP` ile Warrior'i Lv24'e cikar, stat puanlarini hep STR/VIT'e yatir → `War_TempestSlash`'i
  ogren ve hotbar'dan kullanmayi dene → **"Yeterli mana yok!"** cikmali. Sonra Lv80'e cikip yine INT'ye hic
  basmadan `War_DoomsdayBlow`'u (400 mana) dene → yine kullanilamamali.

- [x] **FIXED (bkz. altta yeni commit) — BULGU #3 — MEDIUM — Skill kitabi (K) paneli TASIYOR; satirlar panelin disina cikiyor.**
  `MMOHUD.cpp:758-765`'teki "shrink-to-fit" sistemi `RowH`'i **20px tabanina** sikistiriyor, dolayisiyla
  57 skilde artik yetmiyor. Hesap: `AvailableH`=364, `OverheadH`=5*(22+6)=140.
  Warrior (16 skill): `RowH` = Clamp(14, 20, 32) = **20 (tabana takiliyor)**; icerik 140+320=460 > 364;
  cizim y=172'de baslayip **y=632'de bitiyor**, panelin alt kenari ise `PY+PH`=**576** → **~56px panel disinda**,
  son 2-3 satir karanlik panel arkaplani olmadan dogrudan oyun goruntusunun uzerine biniyor.
  Archer ve Priest (14 skill): bitis 592 → ~16px disarida. Mage (13 skill): bitis 572, panel icinde ama
  `PY+PH-26`=550'deki "Bir hotbar slotuna tikla" mesajinin uzerine biniyor.
  Koddaki `// ... asla tasmaz` yorumu artik **dogru degil**.
  **Test:** Warrior ile oyuna gir, `K` ile skill kitabini ac → asagi dogru bak: **"Ustalik 4" agacinin
  son 1-2 satiri mavi-siyah panelin ALT KENARININ ALTINDA**, arkaplansiz sekilde gorunmeli. Ayni testi
  Mage ile yap → son satir "hotbara ata" yazisiyla ust uste binmeli.

- [x] **FIXED (bkz. altta yeni commit) — BULGU #4 — LOW — Mage'in Lv10 skilli, Lv8 skilinden ZAYIF.**
  `Mag_ChainLightning` (Lv8, rutbe 3) Power=130; bir sonraki rutbe olan `Mag_FlameStorm` (Lv10, rutbe 4) Power=**115**.
  Ayrica `Mag_Meteor` (Lv6) Radius=420 iken FlameStorm Radius=**400** — yaricap da KUCULMUS, mana ise artmis (38 → 45).
  (Tek-hedef vs AoE farki var ama ayni sinifin ayni agacinda bir sonraki rutbe.)
  **Test:** Mage ile Lv10'a cik, ikisini de bir mob'a kullan → FlameStorm'un vurusu ChainLightning'den dusuk gorunmeli.

> BULGU #1-4 duzeltildi (bkz. altta yeni commit). #5 ve #6 bilerek DUZELTILMEDI — ikisi de
> sayisal balans/taste konusu (bug degil), ayri bir ekonomi/denge gecisine birakildi.

- [ ] **BULGU #5 — LOW — Warrior'in Lv80 capstone'u, Lv70 capstone'undan neredeyse hic guclu degil.**
  `War_TitanWrath` (Lv70) Power=1150, CD=20s → `War_DoomsdayBlow` (Lv80) Power=**1200** (+%4.3), mana +%25, CD=**26s**.
  Saniye basina: 57.5 → **46** (yani DUSUYOR); tek kazanci AoE olmasi.
  Diger siniflarda ayni sicrama cok daha buyuk: Archer 860→1600 (+%86), Mage 900→1350 (+%50), Priest 950→1650.
  **Test:** Warrior ile Lv80'e cik, iki capstone'u ayni mob'a kullan → hasar farki neredeyse hissedilmemeli.

- [ ] **BULGU #6 — LOW — Warrior ve Priest icin Lv24 → Lv70 arasi HASAR PLATOSU (46 seviye).**
  Warrior'in son yeni hasar skili Lv24 `War_TempestSlash` (270); Lv33/45/58'de eklenenlerin hepsi buff (Power=0).
  Priest'te de ayni: son hasar skili Lv24 `Pri_WrathOfAngels` (290), Lv45/58 buff.
  Archer (Lv45: 510, Lv58: 660) ve Mage (Lv45: 520, Lv58: 680) bu araligi doldurmus — iki sinif dolu, iki sinif bos.
  **Test:** Warrior ile Lv33/45/58'e cik → her seferinde acilan yeni skilin bir buff oldugunu ve mob oldurme
  hizinin Lv24'ten beri degismedigini gozlemle.

- [ ] **Gozlem (LOW, ONCEDEN VARDI) — Priest sifa zincirinin ortasindaki skil muttefik iyilestiremiyor.**
  `Pri_DivineRestoration` (Lv7, rutbe 2) efekti `HealSelf`; ayni zincirdeki `Pri_GreaterHeal` (rutbe 1),
  `Pri_HolyRain` (rutbe 3) ve `Pri_LifeBond` (rutbe 4) ise `HealTarget`. Bu commit'in getirdigi bir hata degil
  ama artik yeni skillerin ortasinda siritiyor.
  **Test:** Partide bir muttefigi sec → `Pri_GreaterHeal` ONU iyilestirmeli, `Pri_DivineRestoration` ise KENDINI.

- [ ] **Gozlem (LOW) — MaxPlus 9→8 oncesi kayitlardaki +9 item'lar +9 olarak kaliyor.**
  `MMOInventoryComponent.cpp:597` (`ApplySaveData`) `EquipmentBag`'i hicbir dogrulama yapmadan kopyaliyor.
  **Cokme/ out-of-bounds YOK** (TryUpgrade ve HUD [MAX] etiketi dogru davraniyor), ama oyunda artik
  uretilemeyen, kalici olarak tavan-ustu bir item olusur.
  **Test:** Elinde bu degisiklikten onceki bir kayit varsa yukle → varsa +9 item hala +9 gorunmeli ve
  uzerinde [MAX] yazmali, upgrade teklif edilmemeli. (Yeni kayitla oynuyorsan bu madde gecersiz, atla.)

- [ ] **Nuans (bug degil, bilmen iyi) — HUD'daki "YANMA %X", tiklama basina degil, BASARISIZLIK HALINDE olasilik.**
  +7'de ekranda "YANMA %100" yazar ama basari sansi %20 oldugu icin **tiklama basina gercek yanma ihtimali %80**'dir.
  Kodda hata yok; sadece etiketin ne anlattigini bilerek okuman gerekiyor.
  **Test:** Bir item'i +7'ye getir → satirda "%20" (basari) ve "YANMA %100" birlikte gorunmeli.

---

# Skill agaci Lv80'e genisletildi: 4 sinifa 8'er yeni skill (2026-09-14)

> MaxLevel 80'e cikarildiktan sonra Lv9-80 arasinda ogrenilecek yeni skill yoktu (en yuksek RequiredLevel=8'di).
> Her sinifa (Warrior/Archer/Mage/Priest) `Source/MyProject/Private/Skills/MMOSkillDatabase.cpp` icine 8'er yeni
> skill eklendi (Lv10/16/24/33/45/58/70/80), toplam 25 -> 57 skill. **ONEMLI:** bu isimler/degerler Knight Online'dan
> literal import DEGIL — KO'nun sadece mastery-rutbe YAPISI uyarlandi, isimler/sayilar KO-STYLE uydurma icerik
> (bkz. commit mesaji). Derleme temiz.

- [ ] **Yeni skiller ogrenilebiliyor:** Herhangi bir siniftan bir karakteri `MMOGainXP` ile Lv10, Lv16, Lv24 gibi esik seviyelere getir → Skill (K) ekraninda o sinifin ilgili agacinda (Special0/1/2) yeni skill SP ile acilabilir olmali (once o agacin rutbesini bir yukseltmen gerekebilir — mevcut mastery-rutbe kurali degismedi).
- [ ] **Special3 (yeni agac) ilk kez kullaniliyor:** Bir karakteri Lv70 ve Lv80'e getir → her sinifin "Ustalik 4" (Special3) agacinda iki capstone skill gorunmeli ve rutbe yatirilinca acilmali: Warrior "Kiyamet Darbesi", Archer "Avcinin Sonu", Mage "Mutlak Yikim", Priest "Cennetin Eli".
- [ ] **Capstone hasar/sifa hissi:** Lv70-80 capstone skillerinden birini kullan (ornegin Mag_AbsoluteAnnihilation veya Pri_HandOfHeaven) → hasar/sifa miktari Lv8 civari eski skillerden (orn. Zincir Yildirim Power=130) belirgin sekilde daha yuksek hissettirmeli (kaba egri, hassas balans ayri bir geciste yapilacak).
- [ ] **Priest ikili capstone kimligi:** Priest'in Lv70 skilli (Tanrisal Yargi) saldiri, Lv80 skilli (Cennetin Eli) ise HealTarget (secili muttefigi iyilestirir, hedef yoksa kendini) olmali — Priest'in hem savasci hem sifaci ucu bu iki skille test edilebilir.
- [ ] **Yeni AoE/debuff skilleri (Arc_SpikeTrap, Arc_PoisonCloud) Zehir uyguluyor:** Bu iki skil bir hedefe/alana isabet ettiginde hedefte "Zehir" (Poison) periyodik hasar debuff'i gorunmeli (mevcut Zehirli Ok ile ayni efekt, yeni bir efekt ID'si eklenmedi).
- [ ] **Regresyon:** Mevcut 25 eski skill (Lv1-8) davranisi, mastery-rutbe kurali (`RequestInvestMastery`, rutbe asla karakter seviyesini gecemez) ve HUD skill listesi (K ekrani) degismemis olmali.

---

# Kademeli yanma riski + MaxPlus 8 + MaxLevel 80 — commit 170f87c (2026-09-14)

> Derleme temiz. Bir alt sectiondaki (commit 8532890) yanma-esigi ve MaxLevel=20
> degisiklikleri bu commit ile SUPERSEDE edildi (kullanicinin acik istegiyle) —
> o maddeler artik gecerli DEGIL, asagidakiler gecerli.

- [ ] **Yanma artik kademeli:** +0, +1, +2'den deneme yaparken basarisizlikta item HICBIR ZAMAN yanmamali. +3'ten itibaren (yani +3->+4 denemesi) basarisizlikta yanma OLASI olmali (garanti degil): sirasiyla ~%15/%30/%50/%70/%100 (son adim +7->+8, MaxPlus=8 oldugu icin bu artik erisilebilecek en yuksek deneme ve HER basarisizlikta yakiyor).
- [ ] **MaxPlus artik 8:** Bir item'i +8'e kadar yukselt → +8'de [MAX] etiketi gorunmeli, daha fazla upgrade denemesi sunulmamali (eskiden +9'a kadar cikiyordu).
- [ ] **HUD yanma etiketi:** Envanter cantasinda yukseltilebilir bir item satirinda, yanma riski olan artilarda (+3 ve sonrasi) "YANMA %X" etiketi gorunmeli; +0-+2 arasinda hic gorunmemeli.
- [ ] **MaxLevel tekrar 80:** Karakter artik Lv80'e kadar XP kazanabilmeli (eskiden Lv20'de duruyordu). NOT: mevcut skill/gorev/mob icerigi hala sadece ~Lv15-16'ya kadar derinlik tasiyor — bu bilinen bir bosluk, ayri bir "skill agaci genisletme" gorevi arka planda calisiyor (bkz. asagida eklenecek yeni bolum).
- [ ] **Regresyon:** +0-+2 upgrade davranisi (fiyat, basari sansi) degismedi, sadece yanma mekanigi.

---

# Ekonomi denge duzeltmeleri 2: upgrade yanma esigi + MaxLevel — commit 8532890 (2026-09-14) — SUPERSEDE EDILDI, yukariya bakin

> Derleme temiz.

- [ ] **Upgrade artik +6'ya kadar guvenli:** Bir item'i +6'ya kadar yukseltmeye calis (basarisiz olsa bile item KAYBOLMAMALI, sadece altin gider) → +6'dan +7'ye denerken artik yanma riski BASLAMALI (basarisizlikta item kaybolabilir).
- [ ] **MaxLevel 20:** Karakteri Lv20'ye getir (MMOGainXP ile hizlica test edilebilir) → Lv20'de "Level Up" tetiklenmemeli/XP kazanimi durmali (eskiden 80'e kadar devam ederdi).
- [ ] **Regresyon:** +0-+5 arasi upgrade davranisi (fiyat, sans) degismedi, sadece yanma esigi kaydi — bunu da dogrula.

---

# Ekonomi denge duzeltmeleri (elit loot sansi + yan gorev altini) — commit beee9f9 (2026-09-14)

> Bir ekonomi-denge inceleme ajaninin raporuna dayanarak 2 sayisal degisiklik yapildi. Derleme temiz, oynanarak dogrulanmasi gerekiyor.

- [ ] **Elit/boss loot artik garanti degil:** Bir elit/boss'u (or. Kadim Golem, Ork Reisi) birkac kez oldur → ekipman dusmeyen olumler de OLMALI (once her zaman dusuyordu, simdi ~%50 sans). Altin/XP hala her olumde gelmeli, sadece ekipman garantisi kalkti.
- [ ] **Yan gorev altini arttı:** Q_Warlocks/Q_OrcWarriors/Q_DireWolves gorevlerini tamamla → altin odulu eskisinden (500/480/480) daha yuksek (650/620/600) gelmeli.
- [ ] **Regresyon:** Diger gorevlerin ve elit-olmayan mob'larin altin/loot davranisi degismemis olmali.

---

# QA taramasi: "istemci int32 -> carpma/birikim, ust sinir yok" hata sinifi — TUM 4 BULGU DUZELTILDI (2026-09-14)

> QA notu: `3e35942`'deki BuyPotion/BuyConsumable int32-tasma acigi emsal alinarak TUM `UFUNCTION(Server, ...)` ve `UFUNCTION(Exec)` fonksiyonlari tarandi (Inventory, Stats, Skill, Party, Quest, Character, Mob, PlayerController, LootPickup, ItemDatabase). **Ikinci bir "bedava altin" acigi BULUNAMADI** — fiyat hesaplarina giren tek istemci-kontrollu sayi `Count`'tu ve o artik clamp'li. Ayni sinifta bulunan 4 madde de duzeltildi: Bulgu 1 → `72b9b63`, Bulgu 2 (XP egrisi) → `2a590f1`, Bulgu 3 (AddGold) → `50fd9c0`, Bulgu 4 (ApplySaveData) → `f39da5f`. Asagidaki test maddeleri artik gecerli/test edilebilir durumda (host-guard onerisi (Bulgu 1'in "b" maddesi) BILEREK uygulanmadi — MMOGainXP/MMODamage/MMOBuff/MMOSpawnMob gibi diger tum debug komutlariyla ayni "kendine-ozel hile, herkese acik" tutarliligini korumak icin; asil veri-bozulma riski olan tasma zaten kapatildi).

### BULGU 1 (YUKSEK) — `ServerGiveItem` sunucu tarafinda HIC clamp'lenmiyor
- Dosya: `Source/MyProject/Private/Characters/MMOCharacter.cpp:1103` (`ServerGiveItem_Implementation`), istemci yolu `:1086-1101` (`MMOGiveItem`).
- Sorun: `MMOGiveItem` istemcide `FMath::Max(1, Count)` uyguluyor (sadece ALT sinir), ama Server RPC gövdesi `Count`'u **oldugu gibi** `Inventory->AddItem(ItemId, Count)`'a veriyor. BuyPotion'daki duzeltmenin (`Clamp(Count, 1, 999)`) tersine, clamp **RPC'nin sunucu tarafinda tekrar uygulanmiyor** — hileli istemci `MMOGiveItem`'i hic kullanmadan dogrudan `ServerGiveItem(Id, 2000000000)` atabilir.
- Tetikleyen girdi: aynı çağriyi 2 kez yap → `MMOInventoryComponent.cpp:70` `Stack.Count += Count` int32'yi tasirip stack'i NEGATIFE cevirir; `RemoveItem` (`:94`) bir daha asla o itemi harcayamaz (item kalici olarak kullanilamaz hale gelir), `GetTotalItemCount` (`:122`) de tasar.
- Ikinci (ve daha ciddi) yon: bu bir DEBUG/hile komutu ama Server RPC olarak aga acik ve **hicbir yetki kontrolu yok** — uzak istemci kendine sinirsiz iksir/parsomen verebilir. Emsal cozum ayni dosyada zaten var: `ServerSetPvP_Implementation` (`MMOCharacter.cpp:725-729`) `GetController()->IsLocalController()` ile sadece HOST'a izin veriyor.
- Onerilen yon (kodu QA yazmaz): (a) `ServerGiveItem_Implementation` icinde `Count = FMath::Clamp(Count, 1, 999)`, (b) `ServerSetPvP`'deki `IsLocalController()` host-guard'ini buraya da ekle.
- [ ] **Duzeltildikten sonra regresyon:** Play bas → konsola `MMOGiveItem HealthPotion 3` → envanterde (I) iksir sayaci **+3** olmali (normal debug kullanimi BOZULMAMALI).
- [ ] **Ust sinir calisiyor:** Konsola `MMOGiveItem HealthPotion 999999` → sayac **999'da (veya secilen tavanda) durmali**, negatif/garip bir sayi GORUNMEMELI.
- [ ] **Ust uste cagri tasmiyor:** `MMOGiveItem HealthPotion 999` komutunu 5 kez arka arkaya calistir → sayac duzgun artmali, negatife DONMEMELI, F ile iksir kullanimi calismaya devam etmeli.
- [ ] **Host-guard (2 pencereli PIE):** Listen server ac; ISTEMCI penceresinin konsoluna `MMOGiveItem HealthPotion 5` yaz → (host-guard eklendiyse) istemciye item GELMEMELI. Host penceresinde ayni komut calismali.

### BULGU 2 (ORTA, hile gerektirmiyor — normal oynanista tetiklenir) — XP egrisi Level 77'de int32'yi tasiyor
- Dosya: `Source/MyProject/Private/Components/MMOStatsComponent.cpp:132-139` (`ComputeExperienceForLevel`), kullanildigi yerler `:96`, `:286-290` (`GainExperience`), `:307` (`ApplyDeathPenalty`), `:731` (`OnRep_Level`).
- Sorun: egri `100 * 1.25^(Level-1)`. Hesap: L76 = 1.854.603.075 (sinir ici), **L77 = 2.318.253.844 > INT32_MAX (2.147.483.647)**. `FMath::RoundToInt` ile sinir disi float->int32 donusumu tanimsiz davranis; MSVC/x64'te INT32_MIN (-2147483648) verir. `MaxLevel = 80` oldugu icin bu bolge **tasarlanan seviye araliginin ICINDE**.
- Tetiklenen davranis: `ExperienceToNextLevel` negatif olur →
  1. `GetExperiencePercent()` (`MMOStatsComponent.h:260`) negatif → HUD XP bari bozulur.
  2. `GainExperience`'daki `while (Level < MaxLevel && Experience >= ExperienceToNextLevel)` kosulu **her zaman dogru** → oyuncu ilk XP kirintisinda 77'den dogrudan 80'e firlar; **Level 77/78/79 tamamen atlanir** (ayrica `Experience -= ExperienceToNextLevel` isaretli tasma uretir).
  3. `ApplyDeathPenalty` (`:307`) negatif `Loss` hesaplar → `if (Loss > 0)` yanlis → o seviyelerde olum XP cezasi sessizce devre disi.
- Onerilen yon: egriyi `int64`'te hesaplayip `FMath::Clamp`/`FMath::Min` ile INT32_MAX altina sikistirmak, ya da yuksek seviyelerde egriyi duzlestirmek (denge karari mmo-developer'a ait, QA sadece tasmayi raporlar).
- Not: (2) ve (3) maddeleri, tasmanin UB olmasi nedeniyle derleyiciye bagli; **tasmanin kendisi kesin** (hesapla dogrulandi), sonucun tam sekli dogrulanmali.
- [ ] **Duzeltildikten sonra:** `MMOGainXP 2000000000` gibi komutlarla (veya save dosyasindan) karakteri Level 70'e getir, sonra kucuk XP'lerle yukari cik → **77, 78, 79 seviyelerinin her biri tek tek gorunmeli**, XP bari her seviyede 0'dan dolmali.
- [ ] **XP bari negatif/bos degil:** Level 77-80 arasinda HUD'daki XP bari 0-100% arasinda mantikli gorunmeli (negatif dolgu / ters bar OLMAMALI).
- [ ] **Olum cezasi yuksek seviyede calisiyor:** Level 77+ iken bilerek ol → XP'nin bir kismi gitmeli (o seviyelerde ceza sessizce kaybolmamali).
- [ ] **Dusuk seviye regresyonu yok:** Level 1-10 arasi normal XP kazanimi/level atlama eskisi gibi calismali.

### BULGU 3 (KUCUK / sertlestirme) — `AddGold` tasma korumasi yok, tasarsa altini SIFIRLAR
- Dosya: `Source/MyProject/Private/Components/MMOInventoryComponent.cpp:48-57`.
- `Gold = FMath::Max(0, Gold + Amount);` — `Gold + Amount` int32'yi asarsa negatife doner, `Max(0, ...)` de onu **0 yapar**: oyuncunun tum altini sessizce silinir. Ust sinir (altin tavani) da yok.
- Bugun istemciden erisilebilir DEGIL (tum `Amount` degerleri sunucu-tarafi ve kucuk), ama `ApplySaveData` (`:590`) `In.Gold`'u **hic dogrulamadan** kopyaliyor — elle duzenlenmis/bozuk bir kayitta Gold INT_MAX'a yakinsa, ilk satistan sonra altin tamamen ucar. Onerilen yon: `int64` ara hesap + makul bir altin tavani (or. 2.000.000.000) ve `ApplySaveData`'da `Gold`/`Level`/ana statlar icin clamp.
- [ ] (duzeltilirse) **Normal altin akisi bozulmadi:** Mob oldur → altin topla; ekipman sat → altin gelsin; iksir al → altin dussun. Hicbirinde altin sifirlanmamali.

### BULGU 4 (KUCUK / sertlestirme) — kayittan gelen `Level`/`Experience` dogrulanmiyor
- Dosya: `Source/MyProject/Private/Components/MMOStatsComponent.cpp:653-667` (`ApplySaveData`).
- `Level = In.Level;` hicbir kontrol yok. Bozuk/eski bir kayitta `Level` 76 ustuyse dogrudan BULGU 2'nin tasma bolgesine duser; cok buyuk bir degerde (`Pow` -> +inf) `RoundToInt` yine tanimsiz. Onerilen yon: `Level = FMath::Clamp(In.Level, 1, MaxLevel)` ve `Experience = FMath::Max(0, In.Experience)` (ayni dosyada `RecalculateDerivedStats` icin zaten benzer savunmaci clamp'ler var, `:155-159`).

### TEMIZ CIKANLAR (bu hata sinifi acisindan — sessiz kalmamak icin)
Asagidakiler tek tek okundu, **istemci-kontrollu sayinin fiyat/odul ile carpildigi baska bir yer yok**:
- `BuyPotion` / `BuyConsumable` (`MMOInventoryComponent.cpp:428`, `:476`): `Clamp(Count, 1, 999)` RPC'ye gonderilmeden ONCE yapiliyor **ve** sunucu yolu (`ServerBuyPotion_Implementation` -> `BuyPotion`) ayni clamp'ten tekrar geciyor → hileli istemci atlatamaz. **Duzeltme dogru.**
- `BuyEquipment` / `SellEquipment` / `TryUpgrade`: fiyatlar `FMMOItemDatabase`'den (`GetBuyPrice`/`GetSellPrice`/`GetUpgradeCost`), `Plus` `MaxPlus=9` ile sinirli, `BagIndex` `IsValidIndex` ile dogrulanmis. Istemciden gelen sayi fiyata girmiyor.
- `MMOSkillComponent`: `ManaCost`/`Cooldown`/`Power` tamamen `FMMOSkillDatabase`'den; Server RPC'ler yalniz `FName` ve `IsValidIndex`'li `Slot` aliyor.
- `MMOQuestComponent`: `RewardXP`/`RewardGold`/`RewardConsumableCount` gorev tanimindan; istemci-kontrollu carpan yok. `AcceptQuest`/`ClaimReward` mesafe dogrulamali.
- `MMOPartyComponent`: Server RPC'lerinin hic sayisal parametresi yok.
- `MMOMob` / `MMOLootPickup`: XP/altin arketipten, `HasAuthority()` kapili; loot tek-sefer guard'li.
- `MMOPlayerController::ServerRequestHeal` (`:712`): `Cost = FMath::Max(0, Npc->HealCost)`, carpma yok, sunucu-tarafi mesafe + cooldown var.
- `SpendStatPoint` / `SpendSkillPoints` (`MMOStatsComponent.cpp:329`, `:392`): `Amount`/`Cost` kalan puanla karsilastiriliyor, Server RPC zaten sabit `1` gonderiyor.
- `AttackTarget` / `ServerAttackTarget`: hasar sunucudaki `Stats->AttackPower`'dan; istemci sayi gondermiyor.

### YAN BULGU (ayri sinif, ayni "sunucu RPC'sinde dogrulama yok" ailesi)
- `ServerSpawnMob_Implementation` (`MMOCharacter.cpp:890`) ve `ServerApplyBuff_Implementation` (`:753`) de debug Exec komutlari ama **host-guard'siz Server RPC**. Uzak istemci istedigi arketipte sinirsiz mob spawnlayip (XP/loot ciftligi) veya efekt veritabanindaki herhangi bir buff'i kendine verebilir. Sayisal tasma degil, ama ekonomi/ilerleme acisindan istismar edilebilir. `ServerSetPvP`'deki `IsLocalController()` deseni buralara da uygulanmali.
- [ ] (duzeltilirse, 2 pencereli PIE) **Istemci konsolundan** `MMOSpawnMob OrcWarlord` ve `MMOBuff Rage` → istemcide ETKI ETMEMELI; host penceresinde ikisi de calismali.

---

# Save/load race + envanter canta sinirlamasi + yeni tuketilebilir — commit 373cabd, 70f7286, c4e9b85 (2026-09-12)

> QA notu: Bu 3 duzeltme dogrudan (subagent olmadan) kod incelemesiyle bulundu/uygulandi; derleme temiz. Save-race testi ozel bir zamanlama gerektirir (asagida adimlar). Canta sinirlamasi normal genisletilmis oynanista (birden fazla elit/mob oldurup satmadan biriktirme) kolayca tetiklenir.

### Save-clobber-on-early-disconnect (373cabd)
- [ ] **Normal join/quit akisi bozulmadi:** Karaktere gir (join), birkac saniye bekle (0.6sn'lik ilk-yukleme penceresi gectikten SONRA), sonra normal sekilde cik (Stop/disconnect) → tekrar gir → onceki ilerleme (level/altin/ekipman) KORUNMUS olmali (regresyon yok).
- [ ] **Erken-disconnect senaryosu (asil duzeltme, zor tetiklenir):** Daha once kaydedilmis (gercek ilerlemesi olan) bir karakterle join et ve possess olur olmaz **0.6 saniye icinde** PIE penceresini kapat/disconnect ol → tekrar join et → **eski ilerleme hala orada olmali** (Level 1'e / bos envantere DUSMEMELI). Bu zamanlama zor yakalanir; kaciriirsan "olmadi" degil "tetiklenmedi" say.
- [ ] **MaxHealth/MaxMana negatif olmuyor:** Normal oynanista can/mana barlari hep pozitif/mantikli gorunmeli (defansif clamp, normal oyunda zaten tetiklenmemesi beklenir).

### Envanter cantasi 8 uzeri item erisilemezligi (70f7286)
- [ ] **9+ ekipman biriktir:** Birden fazla elit/mob oldurup ekipman lootunu SATMADAN biriktir (9, 10, 11, 12 item) → envanteri ac (I) → **TUM** itemler gorunmeli ve tiklanabilir olmali (eskiden 8'den fazlasi "... ve N daha" diyip gizleniyordu, tiklanamiyordu).
- [ ] **13. item reddediliyor + mesaj:** Canta TAM 12 iken (MaxBagSize) bir mob/elit oldur → yeni ekipman DUSMEMELI/EKLENMEMELI, ekranda "Canta dolu!" mesaji cikmali (item kaybolmuyor, sadece verilmiyor).
- [ ] **Kusanma/cikarma canta doluyken calisir:** Canta tam 12 iken kusanili bir itemi CIKAR (unequip) → bu, cantaya "geri koyma" oldugu icin REDDEDILMEMELI (oyuncunun zaten sahip oldugu item, yeni kazanim degil).
- [ ] **Dukkan SAT sutunu tum cantayi gosteriyor:** Canta 9-12 item ile doluyken dukkani ac (E) → SAT sutununda tum itemler gorunmeli (bu sutun zaten MaxRows=12 idi, simdi tam ortusuyor).

### Yeni tuketilebilir: Buyuk Can Iksiri (c4e9b85)
- [ ] **Satin alinabiliyor + kullanilabiliyor:** Dukkani ac → "Buyuk Can Iksiri" 40 altina listelenmeli → satin al → envanterde item sayaci artmali → kullaninca (consumable kullanim tusu/UI'siyla) can +150 artmali (tam canda israf olmamali, mevcut heal-clamp davranisiyla tutarli).

---

# Ekonomi bug-hunt: loot dupe guard + satici mesafe dogrulamasi — commit 78d7dbc (2026-09-09)

> QA notu: Iki duzeltme de kod incelemesinde **dogru bulundu** (QA-clean; asagida detay). Fix 1 (loot dupe) yalniz ayni frame'de iki-govde/iki-oyuncu ayni loota deginince tetikleniyordu — tek pencerede fark edilmez, dogrulama icin 2 pencereli PIE gerekir. Fix 2 (satici mesafe) sunucu-tarafi gate; kritik test dogruST alisverisin FAZLA REDDEDILMEDIGI. Satici 500u'da dukkan aciyor, gate 800u — 300u tampon var, normal alisveris gecmeli.

### Fix 1 — Loot tek-sefer toplama (dupe onlemi)
- [ ] **Normal toplama bozulmadi (tek pencere regresyon):** Play bas → moba vur, olsun → dusen altin/item/ekipman ustune git → TAM 1 kez toplanmali (altin +X, item envantere), loot yere yok olmali. Cift ekleme OLMAMALI.
- [ ] **Ekipman lootu tek kez:** Elit oldur → dusen ekipmani topla → ekipman cantasinda (I) yalniz 1 kopya olmali (ayni +N/nadirlik iki kez GIRMEMELI).
- [ ] **2 pencereli PIE (Listen Server) dupe testi:** Iki oyuncuyu ayni loot yiginina AYNI ANDA bindir → loot yalniz BIR oyuncuya, BIR kez gitmeli; ikinci oyuncu ayni loottan ikinci kez alamamali, loot iki pencerede de aninda kaybolmali.

### Fix 2 — Satici yakinlik dogrulamasi (al-sat exploit'i kapandi)
- [ ] **Dogru alisveris GECIYOR (KRITIK — fazla reddetme yok):** Saticiya (E) yaklas, dukkan acilinca (500u) bir sey satin al (iksir/ekipman) → altin dusmeli, item gelmeli, "Satici yakininda degilsin" mesaji CIKMAMALI. Ekipman sat → altin gelmeli.
- [ ] **Saticinin dibindeyken tum islemler calisir:** Iksir al, tuketilebilir al, ekipman al, ekipman sat — dordu de saticinin yaninda sorunsuz gecmeli.
- [ ] **Dukkan acikken hafif geri cekilme sorun degil:** Saticidan ~2-3m geri cekil (dukkan hala acik) → yine alisveris gecmeli (800u tampon). Cok uzaklas (dukkan acik kalirsa) → artik "Satici yakininda degilsin" cikip reddetmeli (beklenen davranis).
- [ ] **Sat sonrasi item kaybi yok:** Ekipman sat → satilan item cantadan gitmeli + altin eklenmeli; reddedilen bir satista (uzaktaysan) item cantada KALMALI (kaybolmamali).
- [ ] **2 pencereli PIE — uzak istemci normal alisveris:** Uzak istemci saticinin yanina gidip alisveris yapsin → sunucu kabul etmeli, altin/item replike gelmeli (sunucudaki pawn konumu dogru olmali).
- [ ] **Exploit kapandi (dogrulama, konsol/hile gerektirir):** Saticidan cok uzaktayken satin-al RPC'si atilirsa (hileli istemci senaryosu) sunucu reddetmeli — normal oyunda test edilemez, kod-seviyesinde dogrulandi.

---

# Sunucu-otoriterlik düzeltmeleri (3 exploit/bug) — commit 364ab72 (2026-09-07)

> QA notu: Bu 3 düzeltme QA'da kod incelemesiyle **doğru bulundu** (aşağıdaki maddeler görsel/oynanış teyidi içindir). Bug 1-2 yalnızca hileli/laggı istemcide, Bug 3 yalnızca listen/dedicated server + uzak istemcide görünür — **tek pencereli PIE'de fark edilmez**, o yüzden aşağıda çoğu madde 2 pencereli PIE gerektirir.

### Bug 1 — Melee cooldown (dürüst oyuncu bozulmadı mı — KRİTİK)
- [ ] **Normal saldırı hâlâ hasar veriyor:** Tek pencere PIE, bir moba saldır (sol tık / R). Her ~0.6 sn'de bir vuruş çıkmalı, hasar HUD'a düşmeli. (Regresyon kapısı: sunucu artık çift-kooldown uyguluyor; dürüst saldırı yine geçmeli.)
- [ ] **Saldırı ritmi eskisiyle aynı hissettirmeli:** Art arda basılı tut / spam yap → atış hızı 0.6 sn cooldown'a oturmalı, "hiç vurmuyor" ya da "gözle görülür yavaşladı" OLMAMALI.
- [ ] **2 pencereli PIE (Listen Server, 2 oyuncu):** İstemci penceresindeki oyuncu bir moba saldırsın → mob canı düşmeli, öldürebilmeli. İstemcinin saldırısı sunucuda işlenip hasar replike gelmeli.

### Bug 2 — Skill cooldown (dürüst oyuncu bozulmadı mı)
- [ ] **Skill cooldown'u bittikten sonra tekrar atılabiliyor:** Bir skill kullan (ör. Warrior `War_PowerStrike` CD 2 sn, hotbar 1). Cooldown dolunca ikinci kez basınca **tekrar çalışmalı** (mana harcanmalı + efekt oynamalı). Cooldown dolmadan basınca "bekliyor" çıkmalı — bu normal.
- [ ] **En kısa CD'li skill'de test:** Archer `Arc_QuickShot` (CD 1 sn) → 1 sn arayla üst üste kullanınca her seferinde atılmalı, arada bir "yenmiyor/atılmıyor" takılması olmamalı.
- [ ] **2 pencereli PIE:** İstemci oyuncusu skill kullansın → efekt/hasar sunucuda işlenip iki pencerede de görünmeli.

### Bug 3 — Mob ölüm görselinin korunması + loot yalnızca sunucuda
- [ ] **2 pencereli PIE (Listen Server, 2 oyuncu) — mob ölüm görseli istemcide korunuyor:** İstemci penceresinden bir mob öldür → mob **istemci penceresinde de** durmalı, gizlenmeli (özel-mesh ork'ta ~2 sn ölüm animasyonu oynayıp sonra kaybolmalı), yani ölüm görseli iki pencerede de oynamalı.
- [ ] **Loot hayalet DEĞİL (sadece sunucu loot'u):** İstemci penceresinden mob öldür → düşen altın/item'i **topla**; toplanabilmeli ve toplayınca iki pencerede de kaybolmalı. İstemci ekranında toplanamayan/hayalet ikinci bir loot yığını OLMAMALI.
- [ ] **Mob tek sefer yok oluyor:** Öldükten ~2 sn sonra mob iki pencerede de kaybolmalı; istemcide erken/çift silme, titreme ya da "bir pencerede duruyor diğerinde yok" tutarsızlığı olmamalı.
- [ ] **XP yalnızca öldürene (sunucu-otoriter):** İstemci oyuncusu mob öldürünce XP **o oyuncuya** gitmeli; host oyuncusu boşuna XP almamalı (aynı party'de değilse). Party menzil paylaşımı varsa menzildeki üyeler almalı.
- [ ] **Tek pencere PIE regresyon:** Standalone/tek oyuncuda mob öldürme, loot düşme, XP alma, mob'un yok olması eskisi gibi çalışmalı (guard'lar tek-pencerede host=authority olduğu için hiçbir şeyi kesmemeli).

---

# Ork görseli artık VERİ-ODAKLI (2026-09-07 — sabah ayarı burada)

- [ ] **Orklar hâlâ Warrok görünüyor (regresyon):** Ork / Ork Savascisi / Ork Reisi mob'ları eskisi gibi Mixamo Warrok mesh'i + kendi anim'leriyle (idle/koşu/saldırı/ölüm) çıkmalı. Görsel/animasyon davranışı ÖNCEKİYLE bire bir aynı olmalı (kaynak sabit→veri oldu, davranış değişmedi).
- [ ] **Diğer moblar hâlâ tint'li Manny:** Goblin/Kurt/Haydut/Trol vb. (MeshPath boş olanlar) eskisi gibi renkli Manny olmalı — değişmemeli.
- [ ] **Warrok hizası artık arketip verisinde (sabah tek satır ayarı):** Warrok mesh hizalaması gözle DOĞRULANMADI. Ayak/dönüş/boyut yanlışsa `Source/MyProject/Private/Characters/MMOMobArchetype.cpp` içindeki `SetWarrokVisual()` yardımcısında `MeshOffsetZ` (ayak hizası, şu an -89), `MeshYaw` (bakış yönü, şu an -90), `MeshScaleMultiplier` (ek boyut çarpanı, şu an 1) alanlarını tek satırda değiştir → editör kapat → Build.bat. Üç ork da aynı setten beslendiği için tek düzenleme hepsini düzeltir.
- [ ] **Yeni yaratık = saf veri:** Yeni bir mob'a özel mesh vermek için kod GEREKMEZ; arketip satırında `MeshPath` + `AnimIdle/Run/Attack/DeathPath` doldurulur (bkz. aşağıdaki örnek). MeshPath boş bırakılırsa tint'li Manny'ye düşer. Yanlış path → güvenle Manny'ye düşer + log uyarısı (çökme yok).
- [ ] **Multiplayer görsel senkron (Listen Server/2 istemci):** Play modunu "Number of Players = 2, Net Mode = Play As Listen Server" yap → hem host hem istemci penceresinde orklar Warrok görünmeli; ork saldırınca saldırı animasyonu HER İKİ pencerede de oynamalı; ork ölünce ölüm animasyonu (mesh 2sn kaybolmadan) her iki pencerede de görünmeli. (Mesh/anim artık her makinede `LoadObject` ile ayrı yükleniyor — istemcide sessizce Manny'ye düşerse bu adım yakalar.)
- [ ] **PAKETLENMİŞ BUILD riski (DOĞRULANMALI — PIE'de görünmez):** Warrok mesh/anim'leri artık koda gömülü asset referansı DEĞİL, sadece FString path. PIE/editörde her asset yüklü olduğu için sorunsuz. Ama oyunu bir gün **Package/Shipping** olarak alırsan bu asset'ler hiçbir yerden referanslanmadığı için cook'a girmeyebilir → LoadObject null döner → orklar sessizce tint'li Manny'ye düşer (çökme yok, sadece görsel regresyon). Paketleme yaparsan: `/Game/Mixamo/Warrok` klasörünü Project Settings → Packaging → "Additional Asset Directories to Cook" listesine ekle (veya bir referans zinciri kur). PIE testinde bu maddeyi atla.

---

# SABAH HIZLI TEST KILAVUZU (2026-08-27 gecesi eklenenler)

> Bu bölüm gece eklenen 9 özelliği önceliğe göre özetler. Detay/eski batch maddeleri aşağıdaki "Bekleyen testler"de duruyor — bir madde patlarsa ilgili batch bölümüne inip ayrıntıya bak.
> Gece eklenenler: Şifacı Lyra healer NPC, içerik paketi (Örümcek Ana elit), dünya elitleri (StoneGolem/ShadowAssassin), ikinci savaş bölgesi (Karakaya Geçidi, Lv6-7), görev ödül eşyaları, elit ganimet tabloları, Ejder seti + Çelik kask/bot, nadirlik-stat doğrulaması, kümülatif HUD taşma düzeltmeleri + Lyra/Kael minimap.

## ⚠️ ÖNCE BUNLARA BAK (en riskli 3 — davranış değiştiren commit'ler)
- [ ] **Spawner seviye-bandı sızması (regresyon):** Play bas → ana mob alanında (yakın halka, ~700u) sadece Lv1-5 normal (Goblin/Kurt/Ork/Haydut/Trol) çıkmalı; Lv6-7 (Ork Savascisi/Korku Kurdu/Kara Buyucu) veya elit ASLA sızmamalı. Uzak ikinci bölge (Karakaya, ileri+sağ) yalnız Lv6-7 doğurmalı. → Bandlar birbirine karışıyorsa yeni içerik dengeyi bozar. (MMOGameMode.cpp:56 ana=MaxLv5, :124-125 ikinci=Lv6-7)
- [ ] **Görev ödül eşyası çift-düşme / düşmeme:** Ödül-eşyalı görevi tamamla → görev penceresinden "ödül al" tıkla → ilgili item envantere TAM 1 kez düşmeli, görev aktif listeden kalkmalı, tekrar tıklanamamalı. → Çift kopya veya hiç düşmeme = ekonomi/envanter bug'ı.
- [ ] **Görev penceresi taşması (çok görev aktifken):** Mümkün olduğunca çok görevi aynı anda aktif tut (8+), G ile görev penceresini aç → tüm satırlar altın çerçeve İÇİNDE kalmalı, "ödül al" satırları tıklanabilir olmalı. Not: shrink-to-fit eklendi (RowH 24-46px clamp), ama toplam ~15+ satırda min 24px taşmayı yine geçebilir — çok görevle sınırı zorla. (MMOHUD.cpp:1592-1605)

## P0 — Kritik yol / smoke test (~5 dk) — patlarsa gerisi anlamsız
- [ ] **PIE açılıyor mu:** Play > ListenServer, 2 client ile başlat → hata/çökme olmadan iki pencere de açılmalı.
- [ ] **Oyuncu spawn + HUD:** Her iki pencerede karakter doğmalı, HUD çizilmeli (can/mana barı, altın, hotbar görünür).
- [ ] **Temel döngü:** Bir moba saldır (sol tık) → mob ölmeli → yere loot düşmeli → üstüne git, toplanmalı.
- [ ] **Stat/level:** Mob öldür → XP artmalı; level atlayınca "dağıtılacak puan" +3 olmalı (C penceresi).
- [ ] **Kaydet/yükle:** Kaydet → yükle → level, statlar, envanter, altın, kuşanılan eşya + nadirlik korunmalı (bozulma yok).

## P1 — Bu gecenin YÜKSEK RİSKLİ değişiklikleri (davranış/sistem)
- [ ] **Ana spawner Lv1-5 sabit:** Ana alanda uzun süre bekle → yalnız Goblin..Trol çıkmalı, Lv6-7/elit yok. (üstteki "ÖNCE BAK" ile aynı — kritik)
- [ ] **İkinci bölge Lv6-7:** Karakaya alanına git → yalnız Ork Savascisi (Lv6) / Korku Kurdu (Lv6) / Kara Buyucu (Lv7) çıkmalı, ~5 mob.
- [ ] **Görev ödül eşyası envantere düşüyor:** Q_SpiderQueen→Plaka Zirh, Q_StoneGolem→Ejder Kilici, Q_ShadowAssassin→Ejder Kask, Q_Warlocks→Savas Baltasi, Q_Boss→Buyuk Kilic; Q_Goblins→3 Can Iksiri, Q_Wolves→3 Mana Iksiri, Q_Level5→2 Buyuk Mana Iksiri. Ödül al → doğru item gelmeli.
- [ ] **Çift-ödül YOK:** Ödül alındıktan sonra görev listeden kalkmalı, item tek kez eklenmeli, tekrar tıklanamamalı.
- [ ] **XP/gold hâlâ veriliyor:** Ödül-eşyalı görevler item'i EK olarak vermeli; XP + altın eskisi gibi de gelmeli (item yerine geçmemeli).
- [ ] **Ödülsüz görev regresyonu:** Q_Orcs/Q_Bandits/Q_Cleanup/Q_Gold500/Q_Gold1000/Q_Trolls hiç item vermemeli, sadece XP+altın.
- [ ] **Elit garantili drop:** Elit öldür (SpiderQueen/StoneGolem/ShadowAssassin) → HER seferinde ≥1 ekipman düşmeli; +N yüksek (SpiderQueen +1..2, Golem/Assassin +1..3, OrcWarlord +2..3, boss 2 ayrı ekipman).
- [ ] **Normal mob loot regresyonu (kritik):** Normal moblar (bIsElite=false) → altın aynı, ~%25 ekipman şansı, çoğu +0, tek drop; DEĞİŞMEMELİ.
- [ ] **Görev penceresi taşma düzeltmesi:** (üstte) çok görevle satırlar panel içinde + tıklanabilir.
- [ ] **Görev takipçisi (sağ-üst) taşması:** 8-12 aktif görevle sağdaki tracker minimapin altından ne kadar uzuyor, parti/hotbar ile çakışıyor mu? (DrawQuestTracker, list-clamp doğrulanmalı)
- [ ] **Loot/heal yalnız sunucuda (host oyunda):** client tarafında çift/hayalet loot, kendi başına altın/can değişimi olmamalı.

## P2 — Yeni içerik doğrulaması (kozmetik/varlık)
- [ ] **Şifacı Lyra:** Doğuş yanında lavanta/mor NPC spawn olmalı → can/mana eksikken tıkla → full heal + altın 100 düşmeli + onay balonu. Altın yetersiz/zaten tam → heal yok, altın düşmez. Menzil dışı → "yaklaş" uyarısı.
- [ ] **4 elit spawn + efekt:** Örümcek Ana (mor, YVS/slow), Kadim Golem (gri devasa, düşük canda Rage), Golge Suikastci (ince hızlı, %50 zehir), Ork Reisi boss (enrage+zehir) — her biri kendi köşesinde, normal spawn'a karışmadan.
- [ ] **Karakaya mobları:** Ork Savascisi (%30 slow), Korku Kurdu (çok hızlı, %35 slow), Kara Buyucu (%40 poison, düşük canda rage) — renk/scale/efekt doğru.
- [ ] **Yeni item'lar shop'ta + stat'lı:** Satıcı (E) → Ejder Zirh (def 21, Lv7), Ejder Bot (def 12, Lv7), Ejder Kilici/Kask, Çelik Kask (def 7), Çelik Bot (def 8) listede; kuşan → stat yansımalı. Tam Ejder seti (Kilic/Kask/Zirh/Bot) kuşanılabilmeli.
- [ ] **Shop 24 satır taşmıyor:** Satın-al sütunu artık 24 satır (1 Can Iksiri + 6 tüketilebilir + 17 ekipman). Shrink-to-fit ile en alttaki Ejder Bot okunabilir + tıklanabilir, hepsi mavi çerçeve içinde. (MMOHUD.cpp:1124-1126)
- [ ] **Nadirlik stat farkı:** Aynı item'in Siradan vs Nadir/Efsanevi hâli → nadir olan daha çok Atk/Def (Uncommon ~%25, Legendary ~3x). Common çarpanı 1.0, regresyon yok.
- [ ] **Minimap yeni NPC'ler (doğrulanmalı):** Minimapte satıcı (mavi) + görevli (altın) var; **Lyra ve Kael minimapte görünüyor mu?** Önceki QA notu: DrawMinimap yalnız Vendor/QuestGiver çiziyordu, Lyra'nın nameplate'i de yoktu. Bu gece nokta/plaka eklendiyse görünmeli — eklenmediyse eksik olarak işaretle.

---

## Bekleyen testler

### KUMULATIF entegrasyon taramasi (8 ozellik bir arada) — QA batch
> Her ozellik tek basina QA'landi; bu maddeler ozelliklerin BIRLIKTE yarattigi etkilesimleri test eder.
- [ ] **Gorev penceresi tasmasi (KUMULATIF, en yuksek oncelik):** Lv6-7'ye kadar oyna, mumkun oldugunca cok gorevi ayni anda AKTIF tut (yan gorevler Temizlik/Guclen/Tuccar Ruhu/Buyuk Servet + Trol Avi + Kara Buyuculer parallel alinabilir; ana zincir + elit zincir de birer aktif ekler). Sonra G/gorev tusuyla gorev penceresini ac. Aktif gorev listesi 8+ satira ciktiginda satirlar panelin (PH=466) **alt kenarindan tasip** hotbar/ekran dibine binmeye basliyor mu? Alt gorevlerin "tikla = odul al" satiri hala tiklanabilir mi, yoksa panel disina mi dusuyor? (MMOHUD.cpp DrawQuestWindow ~1592-1605: GUNCELLEME — artik shrink-to-fit VAR, RowH FMath::Clamp(RowSpace/rows, 24, 46). Yani orta sayida gorevde sigar; ama min 24px oldugu icin ~15+ toplam satirda yine tasabilir. Sinir bu civarda test edilmeli.)
- [ ] **Gorev takipcisi (sag) tasmasi:** Cok sayida (8-12) aktif gorevle sag-ust surekli gorev takipcisi (DrawQuestTracker) minimapin altindan asagi ne kadar uzuyor? Dusuk cozunurlukte parti paneli/hotbar ile cakisiyor mu? (Satir 1450-1480, list clamp yok.)
- [ ] **Satici sutunu kumulatif (24 satir):** Saticiyi ac (E). Satin-al sutunu artik **24 satir** (1 Can Iksiri + 6 tuketilebilir + 17 ekipman; panel PH=548). Kucultme (shrink-to-fit) sonrasi en alttaki Ejder Bot **okunabilir ve tiklanabilir** mi, ust uste binmis mi? Tum 24 satir mavi cerceve icinde kaliyor mu? (MMOHUD.cpp:1124-1126)
- [ ] **Minimap eksik NPC'ler (kozmetik):** Minimapte satici (mavi) ve gorevli (altin) noktalari var ama **Sifaci Lyra ve Muhafiz Kael minimapte HIC gorunmuyor** (DrawMinimap yalniz Vendor/QuestGiver iteratoru cizer). Lyra'nin ayrica **nameplate'i de yok** (DrawVendorNameplates healer'i atlar) — sadece cok yakininda tiklanarak bulunuyor. Beklenen mi, yoksa Lyra/Kael icin de nokta/plaka isteniyor mu?
- [ ] **Kasaba NPC hub'i mob spawn'i icinde mi:** Play bas. Ana mob spawner (Facing×700, yaricap 500) mob'lari x≈200-1200 arasinda dogurur; NPC'ler (Kael x=400, Satici/Gorevli x=250) bu halkanin **icinde** kaliyorsa moblar NPC'lerin ustunde dogabilir ve oyuncu NPC'lerdeyken aninda aggro yiyebilir. NPC hub'inin cevresi mob'suz/guvenli hissettiriyor mu?
- [ ] **Toplam mob yuku (perf notu):** Kararli halde ~12 mob (ana 3 + boss 1 + 3 elit + ikinci bolge 5). Tum spawner'lar dolayken FPS/hitch sorunu var mi (bloke degil, gozlem)?

### Nadirlik-stat dogrulamasi + Ejder seti tamamlama — batch #6
> Doğrulama sonucu: nadirlik ZATEN stat'a etki ediyor (RecomputeEquipmentBonuses içinde GetRarityMultiplier, Common=1.0/Uncommon=1.25/Rare=1.6/Epic=2.1/Legendary=3.0). Bu yüzden çift çarpan EKLENMEDİ; onun yerine 2 yeni üst-tier item (Ejder Zirh, Ejder Bot) eklendi.
- [ ] **Nadirlik stat farki:** Ayni itemin (or. iki Uzun Kilic) Siradan (gri) vs yuksek nadirlik (mavi/mor/turuncu) halini kusan. Nadir olan DAHA COK saldiri/savunma vermeli (Uncommon ~%25 fazla, Legendary ~3x). Envanterde/statta fark gozle.
- [ ] **Common regresyon YOK:** Siradan (gri) bir ekipmanin verdigi stat, nadirlik sistemi oncesiyle BIREBIR ayni kalmali (carpan 1.0). Sayilar sismis/degismiş olmamali.
- [ ] **Plus + nadirlik birlikte:** Yuksek nadirlikli bir itemi +3/+5'e yukselt, sonra tekrar kusan. Stat = temel × (1+0.2×plus) × nadirlik_carpani olmali; carpanlar dogru carpilmali (biri digerini sifirlamamali/cift saymamali). Cikar-tak sonrasi stat tutarli.
- [ ] **Client'ta dogru gorunuyor:** (host oyunda) client kusandiginda equip stat bonusu server'da hesaplanip replike olmali; client'ta ekranda dogru deger gorunmeli, client kendi basina stat sismemeli.
- [ ] **Yeni Ejder seti:** Magazadan/loottan **Ejder Zirh** (Armor, req Lv7, def 21) ve **Ejder Bot** (Boots, req Lv7, def 12) elde edilebilmeli. Ejder Kilici/Kask/Zirh/Bot ile tam set kusanilabiliyor mu, stat'lar mantikli mi bak.
- [ ] **Yeni item regresyonu:** Yeni 2 item eklenince magaza satin-alma listesi ve mob loot havuzu bozulmamali (magaza satirlari tasmiyor, Lv7 elit loot bandinda yeni parcalar da cikabiliyor).

### Elit ganimet iyilestirmesi (garantili + daha iyi ekipman) — batch #5
- [ ] **Elit garantili ekipman:** Bir eliti oldur (SpiderQueen/StoneGolem/ShadowAssassin). HER seferinde en az 1 ekipman lootu (silah/zirh) yere dusmeli — %25 sansa artik takilma yok.
- [ ] **Boss cift drop:** Ork Reisi (OrcWarlord) oldurulunce **2 ayri ekipman** dusmeli (+ altin + iksir sansi). Ayrica ganimeti en comert olmali (+2..+3).
- [ ] **Daha yuksek +N:** Elit dropu +0 gelmemeli. SpiderQueen +1..+2; StoneGolem/ShadowAssassin +1..+3; OrcWarlord +2..+3 aralikta olmali (loot toplayinca ekipman cantasinda +N gozle).
- [ ] **Seviyeye uygun ust-tier:** Elit droplari mob seviyesine yakin ust-tier itemler olmali (or. Lv7 elit: Ejder Kilici / Buyuk Kilic / Plaka Zirh / Ejder Kask bandindan), baslangic Kisa Kilic/Deri Baslik degil.
- [ ] **Nadirlik hala yuksek:** Elit ekipmani cift-zar sayesinde ortalama daha nadir gelmeli (mavi/mor/turuncu isim daha sik).
- [ ] **Normal mob regresyonu (kritik):** Goblin..Trol + OrcWarrior/DireWolf/Warlock (bIsElite=false) ganimeti DEGISMEMELI: altin ayni, ~%25 ekipman sansi, cogu +0, nadiren +1, tek drop. Yeni alanlar default=0 oldugu icin hicbir sey degismemeli.
- [ ] **Loot sadece server'da:** (host oyunda) loot yalniz otoritede basiliyor; client tarafinda cift/hayalet loot olusmamali.

### Gorev odul-esyalari (quest reward items) — batch #4
- [ ] **Ekipman odulu geliyor mu:** Odul-esyali bir elit gorevi tamamla (or. Q_SpiderQueen). Gorev penceresinde tikla → odul al. Envanter/canta'ya ilgili ekipman (Plaka Zirh) +0, Siradan nadirlikte GELMELI. Hangi gorev hangi item: Q_SpiderQueen=Plaka Zirh, Q_StoneGolem=Ejder Kilici, Q_ShadowAssassin=Ejder Kask, Q_Warlocks=Savas Baltasi, Q_Boss=Buyuk Kilic.
- [ ] **Tuketilebilir odulu geliyor mu:** Q_Goblins tamamla → envantere **3 Can Iksiri** (F ile kullanilan HealthPotion sayaci 3 artmali). Q_Wolves → 3 Mana Iksiri. Q_Level5 → 2 Buyuk Mana Iksiri.
- [ ] **Cift-odul YOK:** Odul alindiktan sonra gorev aktif listeden kalkmali; tekrar tiklanamaz olmali. Envantere item yalniz BIR kez eklenmeli (2 kopya gelmemeli).
- [ ] **XP/gold hala dogru:** Odul-esyali gorevler eskisi gibi XP + altini da vermeli (item EK olarak, XP/gold yerine degil).
- [ ] **Odulsuz gorevler regresyonu:** Item odulu ATANMAYAN gorevler (Q_Orcs, Q_Bandits, Q_Cleanup, Q_Gold500, Q_Gold1000, Q_Trolls) eskisi gibi calismali; hicbir item vermemeli, sadece XP+altin.
- [ ] **HUD odul satiri:** Gorev penceresinde odul-esyali gorevlerin alt satirinda "Odul: X XP, Y altin, +ItemAdi" (ekipman) / "+N Can Iksiri" (tuketilebilir) gorunmeli; odulsuz gorevlerde ek olmamali.

### Ikinci savas bolgesi (Lv6-7 normal moblar) — batch #3
- [ ] **Ikinci bolge uzakta doguyor mu:** Play bas, baslangictan eksende ileri (Facing×4 ≈ 2800u) + saga ~1600u kaydirilmis ayri bir alanda ~5 mob dogmali. Diger spawner/NPC'lerle cakismamali (en yakini Golge Suikastci, ~1695u uzakta).
- [ ] **Sadece Lv6-7 normal cikiyor mu:** Ikinci bolgede yalniz **Ork Savascisi (Lv6)**, **Korku Kurdu (Lv6)**, **Kara Buyucu (Lv7)** dogmali. Lv1-5 (Goblin/Kurt/Ork/Haydut/Trol) veya elit ASLA cikmamali.
- [ ] **Baslangic alani regresyonu (kritik):** Ana mob alaninda (Facing×1, ~700u) hala YALNIZ Lv1-5 normaller (Goblin..Trol) cikmali. Yeni Lv6-7 moblar baslangic havuzuna SIZMAMALI.
- [ ] **Ork Savascisi:** koyu kirmizi-kahve renk, Scale ~1.2, orta hiz. Vurusta %30 sansla **Slow (YVS)** — oyuncu yavaslamali.
- [ ] **Korku Kurdu:** celik grisi, Scale ~1.1, **cok hizli** (Speed ~470), genis aggro. Vurusta %35 sansla **Slow (YVS)**.
- [ ] **Kara Buyucu:** mor renk, Scale ~1.05. Vurusta %40 sansla **Poison (ZHR)** — periyodik hasar. Cani %35 altina inince **Rage (OFK)** ile ofkelenmeli.
- [ ] **Q_Warlocks görevi:** Lv6+ iken görevliden "Kara Buyuculer" alinabilmeli. Ikinci bolgede 4 Kara Buyucu öldürünce tamamlanip 800 XP + 500 altin vermeli. (Target = Warlock; sadece o mob sayilmali.)
- [ ] **Q_Warlocks temposu (denge notu):** Ikinci bolgede aninda en fazla ~1-2 Kara Buyucu bulunur (5 slot, 3 mob turu esit sansla; band'da tek Warlock var). 4 Warlock icin respawn beklenir (TopUp her 4sn). Gorevin bitmesi makul surede mumkun mu, yoksa cok mu yavas — gozle.

### Dünya eliti batch #2 (Kadim Golem + Golge Suikastci)
- [ ] **Kadim Golem** doğuyor mu: mob alanının **sol** köşesinde (Facing yönünün soluna ~700u, Orumcek Ana'nın tam karşı tarafı), tek başına. Devasa (Scale ~2.1), gri taş renkli, çok yavaş hareket. Lv7.
- [ ] Golem canı %45 altına inince **öfkelenir** (Rage): saldırı gücü artmalı (üstünde OFK ikonu/efekti).
- [ ] **Golge Suikastci** doğuyor mu: eksende çok daha ileride (Facing×3.2 ≈ 2240u), tek başına. Küçük/ince (Scale ~1.35), koyu renk, çok hızlı hareket (Speed ~480). Lv7.
- [ ] Suikastci vuruşta **%50 sansla zehir** (Poison/ZHR) uygulamalı — vurulunca üstünde zehir ikonu ve periyodik hasar.
- [ ] Her iki elit de random spawn havuzuna KARIŞMAMALI (başlangıç mob alanında normal moblar arası golem/suikastci çıkmamalı) — sadece kendi forced spawner'larında.
- [ ] Spawner konumları çakışmıyor: 4 forced spawner (Ork Reisi, Orumcek Ana, Kadim Golem, Golge Suikastci) birbirinden görünür şekilde ayrı yerlerde.
- [ ] Görev **Q_StoneGolem** ("Kadim Golem"): Orumcek Ana görevini (Q_SpiderQueen) bitirdikten sonra, Lv6+ iken görevliden alınabilmeli. Golem'i öldürünce tamamlanıp 1000 XP + 700 altın vermeli.
- [ ] Görev **Q_ShadowAssassin** ("Golge Suikastci"): Q_StoneGolem'den sonra, Lv7 iken alınabilmeli. Suikastci'yi öldürünce 1100 XP + 750 altın vermeli.
- [ ] Yeni ekipman satıcıda görünmeli: **Ejder Kilici** (Weapon, 38 atk, Lv7) ve **Ejder Kask** (Helmet, 9 def, Lv7). Kuşanınca stat bonusu doğru yansımalı.
- [ ] **Satıcı paneli taşması (regresyon):** Satıcıyı aç (E). NOT: bu batch'ta 20 satırdı; sonraki batch'lerde item eklendi, liste artık **24 satır** (yukarıdaki kümülatif maddeye bak) ve shrink-to-fit eklendi. En alttaki item'lar (Ejder Bot vb.) hâlâ tıklanabiliyor + çerçeve içinde mi?

### Konuşan NPC + LLM diyaloğu (Muhafız Kael)
- [ ] Play bas. Satıcı (mavi) ve görevli (altın) NPC'lerin ilerisinde **mor** bir NPC olmalı: "Muhafiz Kael [konus]".
- [ ] A/D tuşları: karakteri sola/sağa **döndürmeli** (yerinde dönüş), yana kaydırmamalı. W/S ileri-geri.
- [ ] Kael'e yaklaş, üstüne sol tık → alt-ortada mor çerçeveli bir diyalog balonu çıkmalı (konuşan adı + replik), ~9 sn sonra solup kaybolmalı.
- [ ] **Kredi/anahtar YOKKEN** (şu anki durum): balon Kael'in yedek repliklerinden birini göstermeli (örn. "Dur. Adini ve derdini soyle..."). Output Log'da `[MMODialogue] HTTP 400` (kredi düşük) görünür — beklenen.
- [ ] **Kredi eklendikten SONRA**: balon LLM'den üretilen, seviyene göre değişen özgün bir replik göstermeli. Output Log'da `[NPC]` satırı.
- [ ] Uzaktan tıklayınca "NPC'ye yaklas" uyarısı çıkmalı (menzil dışı).
- [ ] Üst üste hızlı tıklama yeni istek atmamalı (1.5 sn spam koruması).
- [ ] 2 pencereli PIE: istemci ekranında da Kael'in ismi "Muhafiz Kael" görünmeli (varsayılan "Kasabali" DEĞİL) ve diyalog balonu çalışmalı.

### NPC karşılama replikleri (Satıcı Merek / Yaşlı Doran)
- [ ] Satıcıya tıkla → dükkan açılır VE alt-ortada Merek'ten bir karşılama repliği balonu (örn. "Celigim namusludur..."). İsim plakası "Satici Merek" olmalı.
- [ ] Görevliye tıkla → görev penceresi açılır VE Doran'dan bir karşılama repliği. İsim plakası "Yasli Doran" olmalı.
- [ ] Balon pencereyi kapatmıyor, ~9 sn sonra soluyor. Farklı tıklamalarda farklı replikler gelmeli (rastgele).

### PvP sistemi
- [ ] 2 pencereli PIE aç (Play > ListenServer, 2 client).
- [ ] Bir pencerede diğer oyuncunun karakterine sol tık → yaklaşıp saldırır, canı düşer.
- [ ] Konsola `MMOPvP 0` → artık oyuncuya vuramazsın ("PvP kapali"), ama moblara vurmaya devam.
- [ ] `MMOPvP 1` → PvP tekrar açılır.

### Kontrol düzeltmeleri
- [ ] Sağ tuşa **basmadan** fareyi oynat → kamera dönmemeli.
- [ ] Sağ tuş **basılı** + fare → kamera etrafında dönmeli.
- [ ] **WASD** ile kameranın baktığı yöne göre hareket.
- [ ] **Space** ile zıplama.
- [ ] Görevciye/satıcıya tıklayınca imleç panelin **üstünde** kalıyor.

### Buff/Debuff (durum efekti) sistemi
- [ ] Konsol: `MMOBuff Rage` → sağ altta yeşil "OFK" ikonu + geri sayım; AttackPower artmalı (bir moba vur, hasar arttı mı).
- [ ] `MMOBuff IronSkin` → "ZRH" ikonu; savunma artmalı (aldığın hasar azaldı mı).
- [ ] `MMOBuff Haste` → "HIZ" ikonu; belirgin şekilde daha hızlı koşmalısın.
- [ ] `MMOBuff Poison` → kırmızı "ZHR" ikonu; her saniye canın azalmalı (DoT).
- [ ] `MMOBuff Regen` → "YEN" ikonu; her saniye canın artmalı (HoT).
- [ ] `MMOBuff Slow` → "YVS" ikonu; yavaşlamalısın.
- [ ] Süre bitince ikon kaybolmalı ve stat/hız eski haline dönmeli.
- [ ] Ölünce tüm ikonlar temizlenmeli.
- [ ] Skill üzerinden: Okçu `Arc_PoisonArrow` ile moba vur → mobun üstünde zehir işlemeli (mob HP zamanla düşer).
- [ ] Büyücü `Mag_FrostNova` → alandaki moblar yavaşlamalı.
- [ ] Savaşçı `War_BattleShout` (Ofke), `War_IronSkin`; Okçu `Arc_Sprint` (Hız); Priest `Pri_Renew` (Yenilenme) → kendine buff bindirmeli.

### Elit/Boss moblar + mob yetenekleri
- [ ] Konsol: `MMOSpawnMob OrcWarlord` → önünde büyük kırmızı "Ork Reisi" (boss) belirmeli.
- [ ] Boss'la dövüş: sana vurunca ara sıra zehir (sağ altta "ZHR" ikonu) bulaşmalı.
- [ ] Boss'un canı %50'nin altına inince öfkelenmeli (vuruşları sertleşir — enrage).
- [ ] `MMOSpawnMob SpiderQueen` → mor "Örümcek Ana" (elit); vurunca sık sık seni yavaşlatmalı ("YVS").
- [ ] Normal `Haydut` mobu ara sıra zehir bulaştırmalı; `Trol` düşük canda öfkelenmeli.
- [ ] Elit/boss'lar normal spawn'da rastgele çıkmamalı (sadece komutla veya özel yerleştirmeyle).

### Mob leash / eve dönüş
- [ ] Bir mobu kışkırt, sonra ondan uzağa (spawn noktasından ~18m+) koş.
- [ ] Mob kovalamayı bırakıp spawn noktasına geri dönmeli.
- [ ] Eve varınca canı **tam dolmalı** (ve varsa öfke/efektleri sıfırlanmalı).
- [ ] Eve dönerken sana saldırmamalı; eve varınca tekrar normal aggro yapmalı.

### Hasarda aggro (menzilli pull)
- [ ] Uzaktaki bir moba menzilli skille vur (örn. Okçu `Arc_QuickShot`) → mob AggroRange dışında olsa bile sana doğru gelip saldırmalı.
- [ ] Vurmayı bırakınca ~8 sn sonra zorla-aggro düşer (leash/normal aggro kuralları devreye girer).

### Tüketilebilirler (mana iksiri + buff scroll'ları)
- [ ] Satıcıya git → alım listesinde "Mana Iksiri" ve 4 parşömen (Ofke/Hız/Koruma/Yenilenme) görünmeli.
- [ ] Bir parşömen satın al (altın düşmeli, envantere eklenmeli).
- [ ] Hızlı test: `MMOGiveItem ScrollRage 3`, `MMOGiveItem ManaPotion 5` ile envantere ekle.
- [ ] **G** tuşu → mana iksiri içilir, MP artar ("Kullanildi: Mana Iksiri").
- [ ] `MMOUse ScrollRage` → Öfke buff'ı biner (sağ altta "OFK" ikonu, saldırı gücü artar).
- [ ] `MMOUse ScrollHaste` / `ScrollIronSkin` / `ScrollRegen` → ilgili buff binmeli.
- [ ] Envanterde yokken kullanınca "yok!" uyarısı vermeli.

### Parti (grup) sistemi — 2 pencereli PIE gerekir
- [ ] İki oyuncu birbirine yaklaşsın. Oyuncu A konsola `MMOInvite` yazsın → en yakın oyuncu (B) davet edilmeli.
- [ ] Oyuncu B ekranında "Parti daveti aldin!" mesajı çıkmalı; B `MMOPartyAccept` yazsın.
- [ ] Her iki oyuncunun sol tarafında **PARTI** paneli belirmeli (diğer üyenin adı + HP barı).
- [ ] Bir üye hasar alınca partnerin panelindeki HP barı düşmeli (replikasyon).
- [ ] Ortak XP: A bir mob öldürsün, B menzildeyken (~30m) **B de XP almalı**.
- [ ] `MMOPartyLeave` → panel kaybolmalı, iki oyuncu da partisiz olmalı.
- [ ] Tek üye kalınca parti otomatik dağılmalı.

### Mob gezinme (idle wander) + World Boss
- [ ] Kışkırtılmamış moblar spawn noktaları çevresinde ağır ağır rastgele gezinmeli (yerinde donmamalı).
- [ ] Bir oyuncu aggro menziline girince gezinmeyi bırakıp saldırıya geçmeli.
- [ ] Oyuncu başlangıcının epey ilerisinde tek bir **Ork Reisi (world boss)** bulunmalı.
- [ ] Boss öldürülünce ~30 sn sonra aynı bölgede yeniden doğmalı.
- [ ] Normal spawn alanında elit/boss çıkmamalı (sadece bu özel boss noktasında).

### Eşya nadirlik (rarity) sistemi
- [ ] Moblardan ekipman düşür → düşerken "[Siradan/Nadir/Ender/Destansi/Efsanevi] X +N dustu" mesajı, nadirliğe göre renkli.
- [ ] Envanteri aç (I) → çantadaki ve kuşanılan eşya isimleri nadirlik rengiyle (gri/yeşil/mavi/mor/turuncu) görünmeli.
- [ ] Nadir bir eşya kuşan → Atk/Def değerleri Common'a göre daha yüksek olmalı (çarpan: Nadir 1.25x … Efsanevi 3.0x), karakter statlarına yansımalı.
- [ ] Elit/boss mobların loot'u ortalamada daha yüksek nadirlikte olmalı (iki zar, iyisini seçer).
- [ ] Satıcıda nadir eşya daha pahalıya satılmalı; satış satırı renkli görünmeli.
- [ ] Kaydet/yükle sonrası eşyaların nadirliği korunmalı (SaveGame).

### Görev zinciri + daha çok içerik
- [ ] Görevciye tıkla → başta sadece "Goblin Avi" ve yan görevler (Temizlik/Güçlen/Tüccar Ruhu) görünmeli; zincir görevleri (Kurt/Ork/Haydut/Boss) **görünmemeli** (kilitli).
- [ ] "Goblin Avi"yi bitir ve ödülü al → "Kurt Surusu" görev listesinde belirmeli.
- [ ] Zinciri takip et: Goblin → Kurt → Ork → Haydut → **Ork Reisini Yen** (boss görevi, 1500 XP + 1000 altın).
- [ ] Ön koşulu bitmemiş bir görevi almaya çalışırsan (normalde gizli) "Once '...' gorevini bitir" uyarısı.
- [ ] Boss görevi: world boss'u (Ork Reisi) öldürünce görev tamamlanmalı.

### Karakter/Stat penceresi (stat puanı dağıtma) — ÖNEMLİ boşluk kapandı
- [ ] **C** tuşu → Karakter penceresi açılmalı (STR/DEX/INT/VIT + türetilmiş statlar + dağıtılacak puan).
- [ ] Level atla → "Dagitilacak puan" artmalı (her level +3).
- [ ] Puan varken bir statın **[+]** butonuna tıkla → o stat +1, puan -1, türetilmiş statlar güncellenmeli (ör. VIT → Max HP artar).
- [ ] Puan bitince [+] butonları pasifleşmeli (tıklama iş görmemeli).
- [ ] `MMOStat str` / `dex` / `int` / `vit` konsol komutu da çalışmalı.
- [ ] Kaydet/yükle sonrası dağıtılmış statlar korunmalı.

### Pencere yönetimi: X kapat + çoklu pencere
- [ ] Her pencerenin (satıcı/görev/envanter/skill/karakter) sağ üstünde kırmızı **X** butonu olmalı; tıklayınca o pencere kapanmalı.
- [ ] **Aynı anda birden çok pencere** açılabilmeli (ör. K skill + I envanter + C karakter birlikte).
- [ ] Pencereler farklı konumlarda (skill sol, envanter sağ, görev sol-orta, karakter sağ-orta) — üst üste binmemeli.
- [ ] Üst üste binen iki pencerede tıklama, **üstteki** pencereye gitmeli.
- [ ] Bir pencereye tıklamak karakteri hareket ettirmemeli; sağ tık pencere üstündeyken kamerayı döndürmemeli.

### Saldırı ve skill animasyonları (KO tarzı)
- [ ] Bir moba saldır (sol tık / R) → karakter **kılıç savurma hareketi** yapmalı (her vuruşta 3 varyasyondan biri).
- [ ] Animasyon oynarken yürüme/koşma bozulmamalı (üst katmanda oynuyor).
- [ ] Yakın dövüş skill'i kullan (ör. Savaşçı `War_PowerStrike`, hotbar 1) → vuruş animasyonu oynamalı.
- [ ] Menzilli/büyü/şifa/buff skill'i kullan (ör. Büyücü `Mag_IceBolt`, Priest `Pri_HolyLight`) → **büyü okuma (charged)** animasyonu oynamalı.
- [ ] Mob sana vurduğunda **mob da** savurma animasyonu yapmalı.
- [ ] 2 pencereli PIE: diğer oyuncunun/mobun animasyonu senin ekranında da görünmeli (replikasyon).

### Şifacı Lyra (iyileştirici NPC / altın gideri)
- [ ] Oyun başında Şifacı Lyra, oyuncu doğuş noktasının yanında (satıcının karşı yanında) **spawn olmalı**; gövdesi açık mor/lavanta tonunda görünmeli.
- [ ] Bir moba can/mana kaybettir, sonra Lyra'ya (menzil içinde) **tıkla** → can **ve** mana tam dolmalı (full heal).
- [ ] İyileştirme sonrası **altın 100 (HealCost) azalmalı**; HUD'daki altın göstergesi güncellenmeli.
- [ ] İyileştirme sonrası Lyra'nın **onay balonu** çıkmalı (şefkatli bir replik).
- [ ] **Altın yetersizken** (Gold < HealCost) tıkla → iyileştirme olmamalı, altın düşmemeli, "yeterli altın yok" balonu çıkmalı.
- [ ] **Zaten tam sağlıklıyken** (can+mana dolu) tıkla → altın **düşmemeli**, "yaran yok, altınına kıyma" balonu çıkmalı.
- [ ] Menzil dışından tıkla → "Sifaciya yaklas" uyarısı çıkmalı, iyileştirme olmamalı.
- [ ] Ölüyken Lyra'ya tıkla (altın yeterliyse) → **dirilmeli** (tam can/mana), altın düşmeli.
- [ ] 2 pencereli PIE: Lyra'nın **tint rengi diğer client'ta da** aynı görünmeli (replikasyon).
- [ ] 2 pencereli PIE: iyileştirme + altın düşüşü yalnızca **sunucuda** olmalı; client'ta değer sunucudan replike gelmeli (client tek başına altın/canı değiştirememeli).
- [ ] **Tam can ama düşük mana** (canı dolu, manası eksik) iken tıkla → mana dolmalı **ve** altın 100 düşmeli (bu durum "zaten tam sağlıklı" sayılmamalı; sadece ikisi de doluyken bedava-geçiş olmalı).
- [ ] **Hızlı üst üste tıkla** (partial can iken 5-6 kez art arda) → altın **yalnızca bir kez** (100) düşmeli, çift/üçlü harcama olmamalı (ilk iyileştirmeden sonra oyuncu tam olduğu için sonraki istekler bedava-geçişe düşmeli).

### Örümcek Ana elit spawn + yeni içerik paketi
- [ ] Oyun başında mob alanının **yan köşesinde** (world boss'tan ayrı, farklı yönde) tek bir mor **Örümcek Ana (elit)** doğmalı.
- [ ] Örümcek Ana ile dövüş: vurunca sık sık seni **yavaşlatmalı** ("YVS" ikonu), can/hareket etkilenmeli.
- [ ] Örümcek Ana öldürülünce ~45 sn sonra aynı köşede yeniden doğmalı.
- [ ] Normal spawn alanında hâlâ elit/boss çıkmamalı; Örümcek Ana yalnızca kendi köşesinde.
- [ ] Görevciye tıkla: "Kurt Surusu"nu bitirdikten sonra **"Örümcek Ana"** yan görevi listede belirmeli (RequiredLevel 5).
- [ ] Örümcek Ana'yı öldür → görev tamamlanmalı; ödül **900 XP + 600 altın** olmalı.
- [ ] **"Trol Avi"** yan görevi (bağımsız, Lv5): 3 Trol öldür → tamamlanmalı, ödül 550 XP + 380 altın.
- [ ] **"Buyuk Servet"** yan görevi (bağımsız, Lv3): 1000 altın biriktir → tamamlanmalı, ödül 500 XP.
- [ ] Satıcıya git: alım listesinde yeni **"Buyuk Kilic"** (Lv6, 30 atk), **"Plaka Zirh"** (Lv5, 15 def) ve **"Buyuk Mana Iksiri"** (120 MP) görünmeli.
- [ ] Yeni ekipmanları kuşan → Atk/Def değerleri karakter statlarına yansımalı; nadirlik rengiyle görünmeli.
- [ ] `MMOGiveItem GreaterManaPotion 3` ile ekle, kullan → MP 120 artmalı.

### Yeni üst seviye yetenekler (her sınıfa 1 tane)
- [ ] Skill kitabını aç (her sınıf için ayrı test): yeni skill listede görünmeli ve panelden **taşmamalı** (Savaşçı'da satır en kalabalık olan sınıf).
  - Savaşçı → "Kan Firtinasi" (Ustalik 1 / Special0, Lv7, SP2)
  - Okçu → "Delici Atis" (Ustalik 1 / Special0, Lv7, SP2)
  - Büyücü → "Zincir Yildirim" (Ustalik 1 / Special0, Lv8, SP2)
  - Priest → "Ilahi Sifa" (Ustalik 2 / Special1, Lv7, SP2)
- [ ] **Prereq kontrolü**: önkoşul skill öğrenilmeden yeni skill'e tıkla → "Once '<skill>' gerekli" yazmalı, öğrenilmemeli.
  (önkoşullar: Kan Firtinasi←Olumcul Darbe, Delici Atis←Ok Yagmuru, Zincir Yildirim←Meteor, Ilahi Sifa←Buyuk Sifa)
- [ ] **Level kontrolü**: gerekli seviyenin altındayken → "Level 7/8 gerekli" yazmalı, öğrenilmemeli.
- [ ] **SP kontrolü**: skill puanı 2'den azken → "2 skill puani gerekli" yazmalı; öğrenince **2 SP** düşmeli.
- [ ] Öğrendikten sonra skill'i bir **hotbar slotuna ata** → slotta görünmeli, tuşla kullanılabilmeli.
- [ ] **Kan Firtinasi** (Savaşçı): etrafındaki (340 yarıçap) **tüm** moblara ~55 hasar vermeli, alan halkası FX'i çıkmalı, 30 mana düşmeli, 14 sn cooldown.
- [ ] **Delici Atis** (Okçu): en yakın moba ~95 hasar + hedefte **"YVS" (Slow)** ikonu belirmeli, mob yavaşlamalı; 26 mana, 9 sn cooldown.
- [ ] **Zincir Yildirim** (Büyücü): tek hedefe ~130 hasar + hedefte **"YVS" (Slow)**; 42 mana, 10 sn cooldown.
- [ ] **Ilahi Sifa** (Priest): anında ~180 can dolmalı **ve** üzerine **"YEN" (Regen)** HoT binmeli (birkaç saniye can artmaya devam etmeli); 55 mana, 20 sn cooldown.
- [ ] Cooldown dolmadan tekrar bas → "bekliyor (x.x s)" uyarısı, skill çalışmamalı.
- [ ] Mana yetersizken bas → "Yeterli mana yok!" uyarısı.
- [ ] 2 pencereli PIE: client'ta kullanılan yeni skill'in hasarı/iyileşmesi **sunucuda** işlenmeli, FX+animasyon her iki ekranda da görünmeli.
- [ ] Kaydet/yükle (logout-login): yeni öğrenilen skill ve hotbar ataması korunmalı.

### 3. bug-hunt turu doğrulamaları (parti / skill-effect / görev)

**NOT (2026-09-12):** Aşağıdaki 3. tur bug-hunt (bir önceki oturumda kesintiye uğramış,
bulguları bu listeye taslak olarak yazılmıştı) tamamlandı ve **6 gerçek hata koddan
doğrulanıp düzeltildi + derlendi** (görev zinciri ayrıca incelendi, temiz bulundu — düzeltme
gerekmedi): (1) mob hızı — `Stats->BaseWalkSpeed` arketip hızı atanmadan ÖNCE cache'leniyordu,
`RefreshBaseWalkSpeed()` eklendi; (2) skill hedefleme artık seçili hedefi (Z/tıkla) önceliklendiriyor,
sadece en yakın moba gitmiyor; (3) hedef gerektiren skiller artık hedefsizken mana/cooldown
harcamıyor; (4) parti daveti artık 3sn cooldown'lu (spam koruması); (5) davet kabul 60sn süre +
3000u mesafe doğrulaması istiyor; (6) `EndPlay`'de artık partiden otomatik ayrılıyor (disconnect
temizliği). Aşağıdaki maddeler artık **doğru davranışı doğrulamak** için — "bug" bulursan gerçek
regresyondur.

**Mob hızı — Slow sonrası arketip hızı bozulması (DÜZELTILDI — artık dogru davranmali)**
- [ ] `MMOSpawnMob Troll` → Trol'ü gözle takip et, ne kadar **yavaş** yürüdüğünü not al (tasarım: 220).
- [ ] Trol'e **Slow** uygula (Büyücü "Buz Patlamasi" / Okçu "Delici Atis"), "YVS" ikonu çıksın.
- [ ] Slow **süresi bitene kadar bekle** (6 sn) → Trol şimdi Slow öncesine göre **belirgin daha hızlı** yürüyorsa bug doğrulandı (220 yerine 300'e sıçrıyor).
- [ ] Aynı testi `MMOSpawnMob DireWolf` ile yap (tasarım: 470, en hızlı mob) → Slow bitince **kalıcı yavaşlamış** kalıyorsa bug doğrulandı.
- [ ] Kontrol grubu: `MMOSpawnMob Orc` (tasarım hızı zaten 300) → Slow bitince hızı **değişmemeli**. Sadece Orc'un normal kalması = teşhis doğru.
- [ ] Oyuncu tarafı kontrolü: Haydut/OrcWarrior seni yavaşlatsın, süre bitsin → **senin** hızın normale dönmeli (oyuncuda sorun beklenmiyor).

**Skill hedefleme — seçili hedef vs en yakın mob**
- [ ] İki mob doğur (`MMOSpawnMob Goblin` x2); birine **çok yakın** dur, diğerini **Z / farenle uzaktan seç** (hedef çerçevesi uzaktakinde olsun).
- [ ] Tek hedefli skill bas (ör. "Guclu Vurus" / "Buz Oku") → hasar **seçtiğin** moba mı, **yanındaki** moba mı gitti? Yanındakine gidiyorsa bug.
- [ ] Aynı kurulumda **normal saldırı** (sağ tık / attack toggle) yap → bu seçili hedefe gitmeli. Normal saldırı ile skill'in farklı hedefe vurması = tutarsızlık doğrulandı.

**Boşa harcanan mana/cooldown**
- [ ] Etrafta **hiç mob yokken** tek hedefli skill bas → ekranda "Menzilde hedef yok" çıkmalı; **mana barını ve cooldown'u izle**: mana düşüyor + cooldown başlıyorsa bug.
- [ ] Aynısını "Ok Yagmuru"/"Meteor" (AoETarget) ile hedefsiz dene → aynı sonuç.

**Parti — davet akışı (2 pencereli PIE şart)**
- [ ] Client1 `MMOInvite` yazsın, Client2'de davet mesajı çıksın → Client2 `MMOPartyAccept` → iki tarafta da HUD'da "PARTI" listesi ve karşı tarafın can barı görünmeli.
- [ ] **Davet spam'i**: Client1 `MMOInvite`'ı arka arkaya 10+ kez çalıştırsın → Client2'nin ekranı davet mesajıyla dolup taşıyor mu, oyun oynanmaz hâle geliyor mu? (throttle yok)
- [ ] **Süresiz davet**: Client1 davet göndersin, Client2 **kabul etmeden** haritanın ta öbür ucuna yürüsün, sonra `MMOPartyAccept` yazsın → parti **yine de kuruluyorsa** bug (mesafe/süre kontrolü yok).
- [ ] **Kendine davet**: Client1 yalnızken `MMOInvite` → "Yakinda davet edilecek oyuncu yok" çıkmalı, kendini partiye almamalı.
- [ ] **Zaten partideyken tekrar davet**: parti kurulduktan sonra Client1 tekrar `MMOInvite` + Client2 `MMOPartyAccept` → parti listesi bozulmamalı, tekrarlanan üye olmamalı.
- [ ] **Ayrılma**: Client2 `MMOPartyLeave` → **her iki** ekranda da "PARTI" paneli kaybolmalı (tek kişi kalınca da parti dağılmalı).
- [ ] **Çıkış temizliği**: parti kuruluyken Client2'nin PIE penceresini **kapat** → Client1'de "PARTI" başlığı hâlâ duruyorsa (altında hiç üye yokken) bug; Client1 partisiz görünmeli.
- [ ] **XP paylaşımı**: parti kuruluyken Client2 yakında dururken Client1 mob öldürsün → Client2 de XP + görev sayacı almalı. Client2 çok uzaktayken (3000 birim üstü) öldür → Client2 XP **almamalı**.

**Görev zinciri (regresyon kontrolü — çalışması bekleniyor)**
- [ ] "Goblin Avi" tamamla + ödül al → "Kurt Surusu" açılmalı. Ödül aldıktan sonra görevciye tekrar tıkla → "Goblin Avi" **listede olmamalı** (ikinci ödül alınamamalı).
- [ ] Önkoşulu bitirmeden "Ork Tehdidi"ni almaya çalış → "Once 'Kurt Surusu' gorevini bitir" uyarısı, görev alınmamalı.
- [ ] 2 pencereli PIE: Client'tan görev kabul et → ActiveQuests **sunucuda** oluşup client'a replike olmalı, görev penceresi güncellenmeli.
- [ ] Kaydet/yükle: aktif + tamamlanmış görevler korunmalı, tamamlanmış görev tekrar kabul edilememeli.

**Şifacı Lyra — tıklamada debounce yok (BUG-HUNT #4, KRİTİK şüphe)**
- [ ] Altınını not al. Lyra'ya yaklaş, canını eksilt (mob vursun / yüksekten atla), sol tuşa **BASILI TUT** ve imleci Lyra'nın üstünde 2-3 saniye tut.
- [ ] Altın **tek seferde 100** mü düştü, yoksa **birden fazla 100'lük** düşüş mü oldu? Birden fazla düştüyse bug doğrulandı.
- [ ] Kesin senaryo: Haydut/Kara Büyücü seni **zehirlesin** (ZHR ikonu), sonra Lyra'nın üstünde sol tuşu basılı tut → her zehir tiki canı düşürdüğü için her seferinde 100 altın daha gidiyorsa bug doğrulandı.
- [ ] Canın **tam doluyken** Lyra'ya basılı tut → alt-ortadaki "Yaran yok evladim" balonu her karede yeniden tetikleniyor/titriyor mu (sunucuya kare başına RPC işareti)?
- [ ] 2 pencereli PIE (client): aynı basılı-tutmayı **client** penceresinde yap, 5-10 sn sürdür → client bağlantısı düşüyor mu / "RELIABLE BUFFER OVERFLOW" logu var mı (Output Log'da ara).
- [ ] Karşılaştırma (çalışması beklenen): Muhafiz Kael'in üstünde aynı basılı-tutmayı yap → balon en fazla 1.5-2 sn'de bir yenilenmeli (throttle var).

**Mob leash — eve dönerken karşılık veremiyor (BUG-HUNT #4)**
- [ ] `MMOSpawnMob Troll` (yavaş mob). Pull et, spawn noktasından **1800 birimden fazla** uzaklaş → mob dönüp eve yürümeye başlasın.
- [ ] Mob eve yürürken **arkasından vur** (Okçu/Büyücü menzilli skill ya da yakın dövüş) → mob geri dönüp sana saldırıyor mu, yoksa hiç tepki vermeden yürümeye devam mi ediyor? Tepki vermiyorsa "risksiz hasar penceresi" doğrulandı.
- [ ] Aynı denemeyi **canını iyice düşürdükten sonra** yap → eve dönerken mob **ölüyor mu** (XP + loot düşüyor mu)? Ölüyorsa: karşılık veremeyen mobu öldürme exploiti doğrulandı.
- [ ] Mob eve **varırsa** canı tam dolmalı ve efektleri (zehir/yavaşlatma) temizlenmeli — doluyorsa reset doğru çalışıyor demektir.
- [ ] Yo-yo kontrolü: mobu leash ettir, eve varmasını bekle, sen **hiç yaklaşmadan** (2500+ birim uzakta) izle → mob eve varır varmaz tekrar sana doğru mu koşuyor? Koşuyorsa "leash reset ForcedTarget'ı temizlemiyor" doğrulandı.

**Skill kitabı → hotbar atama akışı (BUG-HUNT #4)**
- [ ] K ile kitabı aç. Öğrenilmiş bir skill'e **tek tık** yap (sürükleme yok, aynı yerde bırak) → skill "ele alındı".
- [ ] Şimdi alt-ortadaki bir **hotbar slotuna tıkla** (kitabın yazdığı akış: "Sonra bir hotbar slotuna tikla = ata") → slota skill atandı mı? **Hiçbir şey olmuyorsa bug doğrulandı** (ekrandaki talimat çalışmıyor).
- [ ] Kontrol: aynı skill'i **basılı tutup sürükleyerek** hotbar slotunun üstünde bırak → bu yol atamalı (çalışıyorsa sorun sadece tık-tık akışında).
- [ ] Kontrol: kitapta skill'e **sağ tık** → ilk boş slota atamalı (çalışması bekleniyor).
- [ ] Hiçbir pencere açık DEĞİLKEN bir hotbar slotuna sol tık → karakter hotbar'ın **arkasındaki zemine doğru yürüyor mu**? Yürüyorsa HUD tıklamayı yutmuyor demektir (bug).

**Görev NPC'si — mesafe doğrulaması (BUG-HUNT #4)**
- [ ] Görevliye tıkla, görev penceresini aç. Pencere **açık kalsın**, görevliden **çok uzağa** yürü (harita öbür ucu, 3000+ birim).
- [ ] Oradan "Alinabilir gorevler" listesinden bir göreve tıkla → görev **kabul ediliyorsa** sunucuda mesafe kontrolü yok demektir (beklenen: kabul edilmeli, yani bu açık doğrulanacak).
- [ ] Aynı testi tamamlanmış bir görevin **ödülünü almak** için yap (uzaktan tıkla) → XP/altın geliyorsa doğrulandı.
- [ ] Karşılaştırma (çalışması beklenen): satıcıdan uzaklaşıp dükkan penceresi açıkken bir şey satın almaya çalış → "Satici yakininda degilsin" çıkmalı ve altın gitmemeli. Çıkıyorsa vendor tarafı sağlam.

**Spawner — geçersiz arketip / sayaç (BUG-HUNT #4, çalışması bekleniyor)**
- [ ] Boss alanına git (ileri yön, ~2.2x mob alanı mesafesi) → "Ork Reisi" spawn olmuş mu? Öldür, ~30 sn bekle → yerine yenisi geliyor mu?
- [ ] Elit alanları (sağ: Örümcek Ana, sol: Kadim Golem, ileri uzak: Gölge Suikastçı) → her birinde **tek** elit olmalı, öldürünce respawn etmeli.
- [ ] `MMOSpawnMob` ile geçersiz bir id dene (ör. `MMOSpawnMob Zzzz`) → oyun **çökmemeli**, sessizce rastgele normal mob çıkmalı.

---

# Ikinci bolge mob'lari icin eksik gorevler eklendi — commit 509bdec (2026-09-12)

> Not: Kod incelemesiyle (MMOQuestTypes.cpp) OrcWarrior ve DireWolf'un hicbir gorevde hedef olmadigi tespit edildi (sadece Warlock'un Q_Warlocks'u vardi). Ayni pattern'le 2 yeni bagimsiz yan gorev eklendi. Item id'leri (SteelHelmet/SteelBoots) MMOItemDatabase.cpp'te dogrulandi, derleme temiz.

- [ ] **Q_OrcWarriors gorevi goreve gidilebilir listede gorunuyor mu:** Lv6+ karakterle gorev NPC'sine tikla, "Ork Savascilari" gorevi listede olmali, kabul et.
- [ ] **Sayac dogru artıyor mu:** Ikinci bolgedeki bir Ork Savascisi'ni oldur → gorev penceresinde sayac (1/4) artmali.
- [ ] **Odul dogru veriliyor mu:** 4 Ork Savascisi oldurup odulu al → 750 XP, 480 altin, ve **SteelHelmet** ekipmani envantere eklenmeli.
- [ ] **Q_DireWolves ayni sekilde:** "Korku Kurtlari" gorevini kabul et, 4 DireWolf oldur, odulu al → 750 XP, 480 altin, **SteelBoots** envantere eklenmeli.
- [ ] **Cakisma yok:** Iki gorev ayni anda acik tutulabiliyor mu (bagimsiz yan gorevler, birbirini engellememeli).

---

# Sifaci-erken-diriltme sonrasi zorla isinlanma hatasi duzeltildi — commit 2dcc67a (2026-09-12)

> QA notu: Bug-hunt #7, dogrudan kod incelemesiyle (oyuncu olum/respawn akisi, daha once hic incelenmemis alan) bulundu. Derleme temiz.

- [ ] **Ana senaryo (bug tekrar edilebiliyor mu — duzeltme oncesi olurdu):** Olene kadar bir mob'a saldir, oldukten HEMEN sonra (5 saniyelik dogal respawn suresi dolmadan) sifaci NPC'ye git, altin ode ve dirilt. Dirildikten sonra **5 saniye kadar normal oyna/yuru** → karakter kendiliginden baslangic noktasina isinlanMAMALI (duzeltme oncesi isinlanirdi).
- [ ] **Dogal respawn hala calisiyor mu (regresyon kontrolu):** Sifaciya UGRAMADAN normal olarek 5 saniye bekle → oyuncu baslangic noktasinda kendiliginden dirilmeli (eskisi gibi calismali).
- [ ] **Sifaci sonrasi hareket kilidi acik mi:** Sifaciyla dirildikten hemen sonra hareket edebiliyor musun (WASD tepki veriyor mu)?

---

# Skill kitabi penceresi tasma riski duzeltildi — commit bebec01 (2026-09-12)

> QA notu: Skill kitabi sabit RowH=32 kullaniyordu (shop/quest/envanter'de daha once 3 kez bulunup duzeltilen ayni hata sinifi). Warrior sinifi (en kalabalik agac) zaten mevcut alanin neredeyse tamamini kullaniyordu — bir skill daha eklenince panel disina tasardi. Artik satir yuksekligi mevcut alana gore otomatik daralir. Derleme temiz, ama gorsel dogrulama (metin okunabilirligi) gerekiyor.

- [ ] **Normal gorunum (regresyon yok):** Skill kitabini ac (K veya ilgili tus), her 4 sinifi da (MMOClass ile degistirip) kontrol et → satirlar eskisi gibi net okunabilir, ust uste binmiyor.
- [ ] **Warrior ozel kontrolu:** Warrior sinifiyla skill kitabini ac → 4 agac (Temel/Ustalik 1/2/3) ve toplam 8 skill tam sigmali, panelin alt kenarindan tasmamali, "hotbara ata" mesaji (bir skill secili haldeyken) skillerin ustune binmemeli.
- [ ] **Tiklama hala dogru calisiyor mu:** Bir skil ogren, sonra hotbara ata → eskisi gibi calismali (satir pozisyonlari degisti ama tiklama kutulari her zaman cizilen satirla birebir eslesiyor).

---

# Parti birlesmesi 8 kisiyi asinca hayalet-parti hatasi duzeltildi — commit 15b55fb (2026-09-12)

> QA notu: Bug-hunt turu, dogrudan kod incelemesiyle (parti birlesme yolu, daha once hic incelenmemis alan) bulundu. Derleme temiz. Test icin en az 9 oyuncu (veya iki kalabalik parti) gerekiyor — normal 2-3 kisilik oyunda tetiklenmez, ama coklu grup senaryosunda gercek bir riskti.

- [ ] **Ana senaryo:** Iki ayri parti olustur (ideal: biri 5, digeri 5 kisi, toplam 10 > 8 sinir). Bir partinin lideri digerinden birini davet edip kabul ettirmeye calissin → **hicbir tarafin partisi degismemeli**, davet eden ekraninda "Parti dolu (maksimum 8)" mesaji gormeli.
- [ ] **Regresyon - normal birlesme:** Toplam <=8 olacak sekilde iki kucuk parti (or. 3+3=6) birlesmeyi dene → eskisi gibi calismali, tum uyeler tek partide gorunmeli.
- [ ] **Regresyon - tekli davet:** Partisiz iki oyuncu birbirini davet etsin → normal calismali (degisiklik bu yolu etkilemedi).

---

# MMOPvP artik sadece host tarafindan acilabiliyor — commit 9247873 (2026-09-12)

> QA notu: onceki Exec-komut denetimi sadece "sunucu otoritesine dogru yonlendiriliyor mu" kontrol etmisti, "bu sunucu-genelindeki kurali kim degistirebilir" sorusunu degil. MMOPvP bu ikinci eksende gercek bir aciktı: herhangi bir uzak istemci, host'un onayi olmadan TUM oyuncular icin PvP'yi acabiliyordu. Derleme temiz; test icin en az 2 makine/istemci (host + misafir) gerekiyor.

- [ ] **Ana senaryo (aciktan tekrar edilebiliyor mu):** Misafir (host olmayan) istemciden konsola `MMOPvP 1` yaz → PvP **ACILMAMALI** (duzeltme oncesi acilirdi, host mesaji bile gormezdi).
- [ ] **Host hala calisabiliyor mu (regresyon):** Host'un kendi konsolundan `MMOPvP 1` yaz → PvP **ACILMALI** ("PvP ACIK" mesaji host ekraninda gorunmeli), sonra `MMOPvP 0` ile tekrar kapat.
- [ ] **PvP acikken hasar gercekten calisiyor mu:** Host PvP'yi actiktan sonra iki oyuncu birbirine saldirabilmeli (bu zaten var olan davranis, sadece kimin actigini kontrol ediyoruz).

---

# Canta doluyken satin alma artik altin kaybettirmiyor — commit d5ccde9 (2026-09-12)

> QA notu: BuyEquipment altini once dusup ekipmani sonra eklemeye calisiyordu; canta doluysa ekleme sessizce basarisiz oluyor ama altin geri verilmiyordu. Artik canta kontrolu altin dususunden ONCE yapiliyor. Derleme temiz.

- [ ] **Ana senaryo:** Cantani tam 12 esyaya kadar doldur (satin alarak veya loot toplayarak), sonra saticidan bir ekipman daha almaya calis → **altin dusmemeli**, "Canta dolu!" mesaji gorunmeli.
- [ ] **Regresyon:** Cantada yer varken normal satin alma yap → eskisi gibi altin dusup esya cantaya girmeli.

---

# Tuketilebilirler artik envanterden kullanilabiliyor — commit fe2ab44 (2026-09-12)

> ONEMLI bulgu: satici Buyuk Can/Mana Iksiri ve 4 parsomeni satiyordu ama BUNLARI kullanmanin (hotbar dahil) hicbir yolu yoktu — satin alinip sonsuza dek envanterde kilitli kaliyorlardi. Envanter penceresine yeni bir "TUKETILEBILIRLER" bolumu eklendi. Derleme temiz ama gorsel olarak HENUZ test edilmedi (bu oturumda editor acilamiyor) — asagidaki adimlar oncelikli.

- [ ] **Gorunum:** Envanteri ac (I) → CANTA bolumunun altinda yeni "TUKETILEBILIRLER (N) - sol tik: kullan" basligi ve altinda sahip oldugun her tuketilebilir turu icin bir satir (isim x adet) gorunmeli. Metin okunabilir olmali, panelin altindan tasmamali.
- [ ] **Buyuk Can Iksiri:** Saticidan Buyuk Can Iksiri satin al, envanterde goruns�n, uzerine sol tikla → can yenilenmeli, adet 1 azalmali, ekranda "+150 HP" turu bir mesaj gorunmeli.
- [ ] **Parsomen (or. Ofke Parsomeni):** Satin al, envanterde tikla → Rage buff'i uygulanmali (AttackPower artmali, ekranda sure/etki mesaji).
- [ ] **Can Iksiri (F ile ayni yol):** Envanterdeki Can Iksiri satirina tikla → F tusuyla aynen ayni sekilde calismali (can yenile, adet azalt).
- [ ] **Regresyon - Canta bolumu:** Ekipman cantasi hala eskisi gibi calismali (sol tik kusan, sag tik yukselt), yeni bolum eklenmesi bunu bozmamali.
- [ ] **Bos durumlar:** Hem canta hem tuketilebilirler bosken pencere duzgun gorunmeli (canta icin mevcut "canta bos" mesaji, tuketilebilirler icin bolum hic gorunmemeli).

---

# Elit mob'lar artik [ELIT] etiketiyle isaretleniyor — commit f9d92ed (2026-09-12)

> Icerik eksikligi: elit mob'lar (Orumcek Ana, Kadim Golem, Golge Suikastci, Ork Reisi) normal moblarla ayni gorunuyordu. Derleme temiz, GORSEL OLARAK TEST EDILMEDI (editor bu oturumda acilamiyor).

- [ ] **Nameplate:** Bir elit mob'a yaklas (or. Kadim Golem) → basinin ustunde turuncu "[ELIT] Kadim Golem Lv6" yazisi gorunmeli (normal mob'larda bu etiket olmamali).
- [ ] **Target bar:** Elit mob'u Z ile hedefle → ekran ustu orta hedef panelinde de "[ELIT] ..." yazisi turuncu renkte gorunmeli.
- [ ] **Regresyon:** Normal (elit olmayan) moblarin nameplate/target bar'i eskisi gibi (etiketsiz, eski renk) kalmali.

---

# Priest artik parti uyesini iyilestirebiliyor (Buyuk Sifa) — commit cbe38e2 (2026-09-12)

> ONEMLI tasarim eksikligi kapatildi: Priest'in TUM iyilestirme skilleri sadece kendine etki ediyordu, parti icinde baskasini iyilestirmenin hicbir yolu yoktu. Bu 2 oyuncu (biri Priest) ve editor gerektirir — bu oturumda test edilemedi, ONCELIKLI test maddesi.

- [ ] **Ana senaryo:** Priest ile baska bir oyuncuya (dusuk canli) tikla (hedef sec), Buyuk Sifa'yi kullan (hotbar) → hedef aldigin oyuncunun canı yenilenmeli (KENDI canın degil).
- [ ] **Self-fallback:** Hicbir hedef secili degilken (veya kendine tiklayip) Buyuk Sifa kullan → eskisi gibi KENDI canın yenilenmeli (regresyon yok).
- [ ] **Mob hedefteyken:** Bir moba tikla (hedef sec), Buyuk Sifa kullan → mob DEGIL, kendin iyilesmelisin (moba "iyilestirme" uygulanmamali).
- [ ] **Diger Priest skilleri degismedi:** Kutsal Isik ve Ilahi Sifa/Yenilenme hala eskisi gibi SADECE kendine etki etmeli (bu skiller kasitli olarak degistirilmedi).

---

# Yeni karaktere sinif secimi ipucu + minimap'te elit noktalari — commit 362889c (2026-09-12)

> Derleme temiz, gorsel test edilmedi.

- [ ] **Sinif ipucu:** Hic kaydı olmayan YENI bir karakterle oyuna gir → ekranda "Yeni karaktersin (varsayilan: Savasci)... MMOClass warrior|archer|mage|priest" mesaji gorunmeli. Var olan (kayitli) bir karakterle girince bu mesaj CIKMAMALI.
- [ ] **Minimap elit noktalari:** Minimap'i ac, elit bir mob'un (or. Kadim Golem) menziline gir → onun noktasi turuncu ve normal mob noktalarindan biraz daha buyuk gorunmeli.

---

# Sinif adi artik ana HUD'da surekli gorunuyor — commit 0df5a32 (2026-09-13)

> Derleme temiz, gorsel test edilmedi.

- [ ] **Gorunum:** Oyuna gir → sol ust "Lv N" yazisinin yaninda sinifin (or. "Lv 3  Savasci") gorunmeli, K basmadan.
- [ ] **Regresyon:** Skill kitabini ac (K) → basliktaki "SKILL KITABI - Savasci" yazisi eskisi gibi dogru gorunmeli (ayni isim kaynagini paylasiyorlar).
- [ ] **Karakter penceresi (commit 95fb138):** Karakter/stat penceresini ac (C) → baslik "KARAKTER - Savasci" seklinde sinif adini da gostermeli.
- [ ] **Parti listesi (commit dc57d88):** Bir partiye gir → her uyenin satirinda "Isim LvN Sinif" gorunmeli (kendi sinifi DEGIL, o uyenin GERCEK sinifi - baska bir istemciden de dogru gorunmeli).

---

# ONEMLI BULGU: misafir oyuncular arasinda save-dosyasi cakismasi + MMOSetName araci — commit 07d0749 (2026-09-13)

> Bu bir "gorsel test edilecek" degil, **oyun tasarimi/mimari** bulgusu — okuman lazim. Save dosyasi adi PlayerState'in adina dayaniyor; bu proje login/kimlik sistemi kullanmadigi icin motor varsayilaninda oyuncu adi baglanti SIRASINA gore "Player0/Player1/..." oluyor. Host her zaman Player0 oldugu icin guvenli, ama MISAFIRLER degil: farkli gecelerde "ikinci baglanan" farkli arkadaslarin HEPSI ayni "MMOSave_Player1" dosyasini paylasip birbirinin karakterini SESSIZCE EZEBILIR. Gercek arkadaslarinla coklu-oyunculu test etmeye basladiginda bunu bil.
>
> Tam duzeltme icin gercek bir kimlik sistemi (platform login veya yerel profil dosyasi) gerekir — bunu sen istemeden kurmadim. Bunun yerine her oyuncunun kendi adini sabitlemesini saglayan bir arac ekledim: `MMOSetName <isim>` konsol komutu.

- [ ] **MMOSetName testi:** Misafir istemciden konsola `MMOSetName Ahmet` yaz → ekranda onay mesaji gorunmeli. Sonra `MMOSave` ile kaydet, cik, tekrar gir, `MMOSetName Ahmet` + `MMOLoad` yap → karakterin (level/altin/ekipman) korunmus olmali.
- [ ] **Cakisma senaryosu (bilgi amacli, muhtemelen test etmeyeceksin):** Iki farkli "misafir" ile ayri gecelerde, hicbiri MMOSetName kullanmadan baglanip ilerleme kaydet → sonraki gece FARKLI bir kisi ikinci baglanirsa onceki misafirin kaydini gorebilir/ezebilir. Bunu gormek istersen bilerek tetikleyebilirsin, ama gercek bir tehlike oldugu icin oncelik MMOSetName'i alistirmak.

---

# Elit/boss dustugunde sunucu-genelinde duyuru — commit 133ef36 (2026-09-14)

> Derleme temiz, gorsel test edilmedi. 2 oyuncu gerekiyor (biri uzakta olmali).

- [ ] **Ana senaryo:** Bir elit/boss'u (or. Kadim Golem) oldur, o an UZAKTA/farkli bir bolgede olan ikinci bir oyuncu ekraninda turuncu "[DUNYA] Kadim Golem ... tarafindan devrildi!" mesaji gormeli (sadece olduren degil, HERKES).
- [ ] **Normal mob'larda tetiklenmemeli:** Elit olmayan bir mob (Goblin vb.) oldur → bu mesaj CIKMAMALI.

---

# Ucan "+N" iyilesme sayilari eklendi — commit 7a118f5 (2026-09-14)

> Derleme temiz, gorsel test edilmedi. BILINEN SINIRLAMA: su an sadece HOST'un kendi ekraninda calisir (misafir istemciler henuz gormez) - hasar sayilarinin misafirlerde nasil calistigindan (OnRep_Health) farkli bir yol kullaniyor, kasitli olarak (pasif can yenilenmesiyle spam olmasin diye).

- [ ] **Ana senaryo (HOST ekraninda):** Can Iksiri ic (F) veya bir iyilestirme skili kullan → yesil "+75"/"+150" gibi bir sayi yukari dogru yukselerek gorunmeli (hasar sayisiyla ayni animasyon, farkli renk).
- [ ] **Spam kontrolu:** Dusuk canla bekleyip pasif can yenilenmesinin (regen) calismasini izle → hicbir "+N" sayisi CIKMAMALI (bu kasitli, regen ayri bir yoldan gecer).
- [ ] **Misafir istemci (bilinen sinirlama, dogrulamak icin):** Misafir istemciden bir potion ickinde/iyilestirildiginde HENUZ yesil sayi gormemesi bekleniyor — bu bir bug degil, dokumante edilmis bir sonraki-adim.

---

# CIDDI GUVENLIK ACIGI: satin alma int32 tasmasi duzeltildi — commit 3e35942 (2026-09-14)

> Bu bir gorsel test degil, guvenlik dogrulamasi. Normal oynanista bu acigi TETIKLEYEMEZSIN (UI Count=1 gonderir) - sadece hileli/modifiye bir istemci ServerBuyPotion/ServerBuyConsumable'i dogrudan devasa bir Count ile cagirirsa ortaya cikardi. Duzeltme sonrasi Count sunucu tarafinda [1,999] araligina sikistiriliyor, tasma imkansiz.

- [ ] **Normal alisveris regresyon testi:** Saticidan normal sekilde (UI'dan tikla) potion/tuketilebilir satin al → eskisi gibi calismali, altin dogru dusmeli.
- [ ] (Opsiyonel, teknik test) Eger konsoldan Exec ile manuel test etmek istersen: cok yuksek bir Count denemesi artik 999'a sikisip normal fiyatlandirma ile calismali, altin kazandirmamali.

---

# Parti lideri + uye cikarma (kick) — commit 641ff9e (2026-09-14)

> Derleme temiz, gorsel test edilmedi. En az 2 (tercihen 3) oyuncu gerekiyor. Partiyi KURAN (ilk daveti gonderen) kisi lider olur; sadece lider uye cikarabilir. Konsol komutu: hedefi sec (Z / sol tik), sonra `MMOPartyKick`.

- [ ] **Lider isareti:** A ve B parti kursun (A davet eder, B `MMOPartyAccept`). A'nin ekranindaki PARTI basligi "PARTI  (lider sensin)" yazmali; B'nin ekraninda basligin yaninda bu yazi OLMAMALI ama listede A'nin adinin onunde `[L]` gorunmeli.
- [ ] **Ana senaryo (lider kick atar):** A, B'yi hedef alsin (Z veya uzerine sol tik) → konsola `MMOPartyKick` → A'nin ekraninda "Uye partiden cikarildi", B'nin ekraninda "Parti lideri seni partiden cikardi" mesaji cikmali ve IKI tarafta da PARTI paneli kaybolmali.
- [ ] **Lider olmayan kick atamaz:** Yeniden parti kurun (A lider). B, A'yi hedef alip `MMOPartyKick` yazsin → "Sadece parti lideri uye cikarabilir" mesaji cikmali, parti BOZULMAMALI (iki tarafta da panel durmali).
- [ ] **Hedef secili degilken:** Lider hicbir sey secmeden `MMOPartyKick` yazsin → "Once cikarilacak parti uyesini sec" mesaji, baska bir sey olmamali.
- [ ] **3 kisilik partide kick:** A(lider)+B+C parti olsun. A, C'yi kick etsin → C partisiz kalmali, A ve B ikili parti olarak DEVAM etmeli (ikisinin de panelinde digeri gorunmeli, A hala lider).
- [ ] **Lider ayrilirsa liderlik gecer:** A(lider)+B+C partisinde A `MMOPartyLeave` yazsin → B ve C ikili parti olarak kalmali ve ikisinden biri (B veya C) artik lider olmali: kalanlardan birinin ekraninda "(lider sensin)", digerinin listesinde `[L]` gorunmeli. Yeni lider `MMOPartyKick` ile digerini cikarabilmeli.
- [ ] **Regresyon — normal ayrilma:** Eski davranis bozulmamali: ikili partide biri `MMOPartyLeave` yazinca iki taraf da partisiz kalmali; XP paylasimi (parti icindeyken mob oldurunce menzildeki uyelere XP) eskisi gibi calismali.

---

# KO mastery-rutbe skill sistemi — commit f2f3139 (2026-09-14)

> Derleme temiz, gorsel test edilmedi. **Bu, skill sisteminin ogrenme mekanigini degistiren buyuk bir davranis degisikligi** — hotbar/cooldown/UseSkill yollari kasitli olarak HIC degistirilmedi, o yuzden asagidaki regresyon maddeleri en az yeni ozellik kadar onemli. Skill kitabi: `K` (veya mevcut skill kitabi tusu). Seviye atlamak icin `MMOGainXP` konsol komutu kullanilabilir.
>
> Yeni kural (KO): skill puani tek tek skillere degil AGACA (Temel / Ustalik 1-4) yatirilir. Bir agactaki rutbe karakter seviyesini gecemez. Rutbe yukselince o agacta rutbesi yeten TUM skiller birlikte acilir.

- [ ] **Ana senaryo (rutbe yatirma):** Yeni bir Savasci ile basla, skill kitabini ac. Agac basliklarinda "Temel (Rutbe: 0)", "Ustalik 1 (Rutbe: 0)" gibi yazmali. "Ustalik 1" altindaki **Savurma**'ya tikla → 1 skill puani dusmeli, baslik "Ustalik 1 (Rutbe: 1)" olmali ve Savurma `[Ogrenildi]` olmali.
- [ ] **Toplu acilma:** Ayni rutbeyi paylasan skiller BIRLIKTE acilmali. Priest ile "Temel" agacina 1 rutbe yatir → hem **Kutsal Isik** hem **Ceza** ayni anda `[Ogrenildi]` olmali (ikisi de rutbe 1). Bu kasitli, bug degil.
- [ ] **Kilitli skill metni:** Savasci'da hic yatirim yapmadan "Ustalik 1" altindaki **Olumcul Darbe**'ye bak → durumu `[Rutbe 2 gerekli]` yazmali (eski `[Once 'Savurma' gerekli]` degil). **Kan Firtinasi** icin `[Rutbe 3 gerekli]` yazmali.
- [ ] **Rutbe seviyeyi gecemez (KO kurali):** Level 1 bir karakterle bir agaca 1 rutbe yatir (rutbe 1 olur). AYNI agaca tekrar yatirmayi dene (o agactaki bir sonraki skile tikla) → **reddedilmeli**, "Level 2 gerekli" benzeri bir mesaj cikmali, skill puani DUSMEMELI. Level 2'ye cik, tekrar dene → simdi calismali.
- [ ] **Skill puani yoksa:** Puani biten bir karakterde bir skile tikla → "1 skill puani gerekli" mesaji cikmali, rutbe ARTMAMALI.
- [ ] **Seviye gelince otomatik acilma:** Savasci ile "Ustalik 1" agacini rutbe 3'e cikar ama karakter Level 7'nin ALTINDA olsun → **Kan Firtinasi** (Lv7) HENUZ acilmamali. Sonra `MMOGainXP` ile Level 7'ye cik → **kitaba tekrar tiklamadan, hicbir sey yatirmadan** Kan Firtinasi kendiliginden `[Ogrenildi]` olmali. (Bu, rutbe-yeterli-ama-seviye-yetersiz skiller icin eklenen otomatik senkron.)
- [ ] **REGRESYON — hotbar atama:** Ogrenilmis bir skile tikla → sari "ele alindi" durumu + "Bir hotbar slotuna tikla" mesaji cikmali; bir hotbar slotuna tikla → skill oraya atanmali. Suruklemeyle (basili tutup birakma) atama da eskisi gibi calismali.
- [ ] **REGRESYON — skill kullanimi:** Hotbar'daki skilleri numara tuslariyla kullan → mana dusmeli, cooldown calismali, hasar/iyilestirme/buff eskisi gibi olmali. Ozellikle hedef gerektiren skillerde (Guclu Vurus, Ok Yagmuru) "Menzilde hedef yok" davranisi degismemeli.
- [ ] **REGRESYON — baslangic skili:** Yeni karakter olustur → sinifin ilk Temel skili BEDAVA gelmeli ve hotbar slot 0'da olmali (rutbe 0 iken bile). Bu eski davranis korundu.
- [ ] **REGRESYON — kayit/yukleme:** Birkac rutbe yatir, kaydet, oyunu kapat/ac, yukle → hem ogrenilmis skiller hem de agac basliklarindaki rutbe sayilari AYNEN geri gelmeli.
- [ ] **ESKI KAYIT UYUMU (onemli):** Bu degisiklikten ONCE olusturulmus bir kayit dosyasi varsa onu yukle → ogrenilmis skiller ve hotbar eskisi gibi gelmeli, sadece agac rutbeleri "(Rutbe: 0)" gorunmeli. Skiller KAYBOLMAMALI. (Rutbeler 0 oldugu icin o karakter yeni yatirimlarda bastan baslar — kabul edilebilir bir gecis davranisi, ama gorup onaylaman lazim.)
- [ ] **Cok oyunculu (misafir istemci):** Misafir istemciden skill kitabini ac ve bir skile tikla → rutbe ve ogrenilen skiller MISAFIRIN ekraninda da guncellenmeli (sunucuda islenip replike olmasi gerekiyor). Misafirin hotbar'a atayip skili kullanabilmesi lazim.

---

# Ucuncu bolge: Lv8-15 icerik dilimi (4 normal mob + 1 elit + 5 gorev + Mithril tier) — 2026-09-14

> Derleme temiz (`Result: Succeeded`), oynanis test EDILMEDI. **Kapsam notu:** bu, Lv80 hedefinin sadece **Lv8-15** kismini doldurur; Lv16-80 hala bos ve bu bilerek boyle (tum bosluk tek seferde doldurulmuyor).
>
> Yeni icerik: normal moblar `FrostWraith` (Buz Hayaleti, Lv8), `CursedKnight` (Lanetli Sovalye, Lv9), `VenomSpider` (Zehir Orumcegi, Lv10), `OrcBerserker` (Ork Berserk, Lv12); elit `FrostLich` (Buz Lordu, Lv13). Gorev zinciri: `Q_Boss` → Q_FrostWraiths → Q_CursedKnights → Q_VenomSpiders → Q_OrcBerserkers → Q_FrostLich. Yeni esya tier'i: Mithril (Lv10, 4 parca).
>
> Hizli seviye atlamak icin `MMOGainXP` konsol komutu kullanilabilir (zincirin ilk gorevi Lv8 istiyor).

## Spawner yerlesimi / cakisma

- [ ] **Ucuncu bolge yerinde mi:** Oyuna gir, PlayerStart'in baktigi yonde ilerle ve SOLA kay (eksende ~2800u, sola ~1600u). 5 mob'luk bir grup olmali ve **sadece** Buz Hayaleti / Lanetli Sovalye / Zehir Orumcegi / Ork Berserk cikmali.
- [ ] **Band sizintisi yok (onemli):** Baslangictaki ANA mob alaninda (PlayerStart'in ~1050u onunde) hicbir Lv8+ mob cikmamali — orasi hala Lv1-5. Ikinci bolgede (eksende ~2800u, SAGA ~1600u) hala sadece Lv6-7 (Ork Savascisi / Korku Kurdu / Kara Buyucu) cikmali.
- [ ] **Bolgeler ust uste binmiyor:** Ucuncu bolge ile ikinci bolge arasinda 3200u, en yakin elite (Golge Suikastci) ~1695u mesafe var. Ucuncu bolge moblari ile ikinci bolge/Golge Suikastci moblarinin ayni yigin icinde karismadigini gozle dogrula (spawn yaricapi 500u).
- [ ] **Buz Lordu tek ve uzakta:** Eksende en uzak nokta (~4200u). Tek bir Buz Lordu olmali, yaninda baska spawner mob'u olmamali. Oldurunce ~60sn sonra tekrar dogmali.

## Mob davranisi / olcekleme

- [ ] **Guc egrisi mantikli mi:** Lv7 Kara Buyucu'yu rahat oldurebilen bir karakter Lv8 Buz Hayaleti'ni biraz daha zor, Lv12 Ork Berserk'i belirgin sekilde zor oldurmeli. Hicbiri "tek vurusta olen" veya "hic olmeyen" hissi vermemeli.
- [ ] **Efektler calisiyor:** Buz Hayaleti ve Ork Berserk vurusta **Slow**, Zehir Orumcegi **Poison** uygulayabilmeli (HUD'da debuff ikonu). Lanetli Sovalye ve Ork Berserk cani %40-50 altina dusunce **Rage** ile ofkelenmeli (hasari artmali).
- [ ] **Ork Berserk gorseli:** Warrok (ork) mesh'i ve animasyonlariyla cikmali — mor/gri Manny degil. Yurume/saldiri/olum animasyonlari oynamali.
- [ ] **Buz Lordu elit muamelesi:** Oldurunce **tum sunucuya** "elit dustu" duyurusu gitmeli (mevcut elit duyuru sistemi).

## Elit loot orani (ekonomi kurali)

- [ ] **%50 — garantili DEGIL:** Buz Lordu'nu **en az 6-8 kez** oldur ve kac seferinde ekipman dustugunu say. Yaklasik yarisinda dusmeli; her seferinde dusuyorsa `EliteEquipDropChance` yanlis okunuyor demektir (0.5 olmali, 1.0 degil).
- [ ] **Dustugunde 2 parca, +2..+4:** Dusus oldugunda **2 ekipman** birden dusmeli ve arti seviyeleri +2 ile +4 arasinda olmali.
- [ ] **Mithril tier dusuyor:** Buz Lordu Lv13 oldugu icin loot havuzu Lv8-10 bandindan seciliyor → dusen esyalar **Mithril** parcalari olmali (Ejder degil).
- [ ] **REGRESYON — eski elitler degismedi:** Orumcek Ana / Ork Reisi / Kadim Golem / Golge Suikastci hala **Ejder/Celik** tier dusurmeli, Mithril DEGIL (onlarin seviyesi <= 7). Dusus oranlari da eskisi gibi ~%50 kalmali.

## Gorev zinciri

- [ ] **Zincir kilidi:** Lv8 olmadan / `Q_Boss` bitmeden gorevli NPC'de "Buz Hayaletleri" GORUNMEMELI. Q_Boss bitip Lv8 olunca listede belirmeli.
- [ ] **Sirali acilis:** Her gorev bir oncekini gerektirmeli: Q_FrostWraiths → Q_CursedKnights (Lv9) → Q_VenomSpiders (Lv10) → Q_OrcBerserkers (Lv12) → Q_FrostLich (Lv13). Bir ara halkayi atlayip sonrakini alabiliyorsan prereq bozuk demektir.
- [ ] **Sayac dogru mob'u sayiyor:** Ornegin Q_VenomSpiders aktifken Buz Hayaleti oldur → sayac ARTMAMALI; Zehir Orumcegi oldur → artmali.
- [ ] **Oduller:** Sirasiyla Buyuk Can Iksiri x3, Mithril Bot, Mithril Kask, Mithril Zirh, Mithril Kilic gelmeli; XP/altin 1800/1200'den 4500/3000'e kademeli artmali (hepsi Q_Boss'un 1500/1000'inin uzerinde).
- [ ] **Gorev paneli tasmiyor:** Zincir acikken gorevli NPC panelinde tum alinabilir gorevler ekrana sigmali, "odul al" satirlari erisilebilir olmali (daha once overflow duzeltmesi yapilmisti — 5 yeni gorev sonrasi tekrar bak).

## Mithril tier (yeni esya kademesi)

- [ ] **Dukkan listesi tasmiyor (onemli):** Saticiyi ac — satin-alma sutununda artik 4 esya daha var (toplam 21 ekipman + 7 tuketilebilir). Tum satirlar panele sigmali ve tiklanabilir olmali; satir yuksekligi okunamayacak kadar kuculmemeli. Kucultuyorsa HUD'da kaydirma/sutun bolme gerekebilir.
- [ ] **Statlar ve fiyat:** Mithril Kilic 48 atak, Mithril Zirh 28 def, Mithril Kask 12 def, Mithril Bot 16 def; hepsi Lv10 gerektirmeli. Lv10 altinda kusanmayi dene → reddedilmeli.
- [ ] **Upgrade calisiyor:** Bir Mithril parcasini +1/+2'ye yukselt → mevcut upgrade maliyeti/sansi/yanma kurallari aynen islemeli (yeni tier ozel bir durum degil).

---

# Dorduncu bolge: Lv16-24 icerik dilimi (4 normal mob + 1 elit + 5 gorev + Adamantin tier) — 2026-09-14

> Derleme temiz (`Result: Succeeded`), oynanis test EDILMEDI. **Kapsam notu:** bu, Lv80 hedefinin **Lv16-24** kismini doldurur; **Lv31-80 hala bos ve bu bilerek boyle** (kademeli ilerliyoruz, tum bosluk tek seferde doldurulmuyor).
>
> Yeni icerik (tema: **Alev Vadisi**, ucuncu bolgenin buzul temasinin volkanik karsiti): normal moblar `EmberHound` (Kor Tazisi, Lv16), `MagmaGolem` (Magma Golemi, Lv18), `AshOrc` (Kul Orku, Lv20), `InfernalWraith` (Cehennem Hayaleti, Lv22); elit `MagmaTyrant` (Magma Tirani, Lv24). Gorev zinciri: `Q_FrostLich` → Q_EmberHounds → Q_MagmaGolems → Q_AshOrcs → Q_InfernalWraiths → Q_MagmaTyrant. Yeni esya tier'i: **Adamantin** (Lv16, 4 parca).
>
> Hizli seviye atlamak icin `MMOGainXP` konsol komutu kullanilabilir (zincirin ilk gorevi Lv16 istiyor).

## Spawner yerlesimi / cakisma

- [ ] **Dorduncu bolge yerinde mi:** Oyuna gir, PlayerStart'in baktigi yonde COK ileri git ve SAGA kay (eksende ~5600u, saga ~2400u — Buz Lordu'nun 4200u'sunun otesinde). 5 mob'luk bir grup olmali ve **sadece** Kor Tazisi / Magma Golemi / Kul Orku / Cehennem Hayaleti cikmali.
- [ ] **Magma Tirani tek ve karsi tarafta:** Ayni eksende (~5600u) ama SOLA ~2400u → tek bir Magma Tirani olmali, yaninda baska spawner mob'u olmamali. Oldurunce ~60sn sonra tekrar dogmali.
- [ ] **Band sizintisi yok (onemli, regresyon):** Ana alan (~1050u one) hala sadece Lv1-5; ikinci bolge (2800u one + 1600u saga) hala sadece Lv6-7; ucuncu bolge (2800u one + 1600u sola) hala sadece Lv8-12 olmali. Yeni Lv16-22 moblari bu uc havuzun HICBIRINE sizmamali.
- [ ] **Bolgeler ust uste binmiyor:** Kodda hesaplanan mesafeler — dorduncu bolgeden en yakin spawner'lar: Buz Lordu ~2778u, ikinci bolge ~2912u; Magma Tirani ile dorduncu bolge arasi 4800u. Hepsi >700u. Yerinde gozle dogrula: dorduncu bolge moblari Buz Lordu'nun alanina karismamali (spawn yaricapi 500u).

## Mob davranisi / olcekleme

- [ ] **Guc egrisi mantikli mi:** Lv13 Buz Lordu'nu yenebilen bir karakter Lv16 Kor Tazisi'ni zorlanarak, Lv22 Cehennem Hayaleti'ni ancak iyi ekipmanla yenebilmeli. Hicbiri "tek vurusta olen" veya "hic olmeyen" hissi vermemeli.
- [ ] **Efektler calisiyor:** Kor Tazisi ve Cehennem Hayaleti vurusta **Poison**, Kul Orku **Slow** uygulayabilmeli (HUD'da debuff ikonu). Magma Golemi, Kul Orku ve Cehennem Hayaleti cani %35-45 altina dusunce **Rage** ile ofkelenmeli (hasari artmali).
- [ ] **Kul Orku gorseli:** Warrok (ork) mesh'i ve animasyonlariyla cikmali — mor/gri Manny degil. Yurume/saldiri/olum animasyonlari oynamali.
- [ ] **Magma Tirani elit muamelesi:** Nameplate'te turuncu `[ELIT] Magma Tirani Lv24` gorunmeli; oldurunce **tum sunucuya** "elit dustu" duyurusu gitmeli.
- [ ] **Boyut:** Magma Tirani Scale=2.0 ile oyundaki en buyuk mob olmali (Kadim Golem 2.1'in hemen altinda) — zeminden batmis/havada asili gorunmemeli.

## Elit loot orani (ekonomi kurali)

- [ ] **%50 — garantili DEGIL:** Magma Tirani'ni **en az 6-8 kez** oldur ve kac seferinde ekipman dustugunu say. Yaklasik yarisinda dusmeli; her seferinde dusuyorsa `EliteEquipDropChance` yanlis okunuyor demektir (0.5 olmali, 1.0 degil — onceki ekonomi raporunun bulgusu).
- [ ] **Dustugunde 2 parca, +3..+5:** Dusus oldugunda **2 ekipman** birden dusmeli ve arti seviyeleri +3 ile +5 arasinda olmali (Buz Lordu'nun +2..+4'unun bir kademe ustu).
- [ ] **Adamantin tier dusuyor:** Magma Tirani Lv24 oldugu icin loot havuzu (`RequiredLevel <= 24 && >= MaxReq-2`, MaxReq=16) sadece **Adamantin** parcalarini icermeli — Mithril DUSMEMELI.
- [ ] **REGRESYON — eski elitler degismedi:** Buz Lordu hala **Mithril** dusurmeli (Lv13, MaxReq=10 — Adamantin onun havuzuna girmemeli). Orumcek Ana / Ork Reisi / Kadim Golem / Golge Suikastci hala Ejder/Celik dusurmeli. Dusus oranlari eskisi gibi ~%50 kalmali.

## Gorev zinciri

- [ ] **Zincir kilidi:** Lv16 olmadan / `Q_FrostLich` bitmeden gorevli NPC'de "Kor Tazilari" GORUNMEMELI. Q_FrostLich bitip Lv16 olunca listede belirmeli.
- [ ] **Sirali acilis:** Her gorev bir oncekini gerektirmeli: Q_EmberHounds (Lv16) → Q_MagmaGolems (Lv18) → Q_AshOrcs (Lv20) → Q_InfernalWraiths (Lv22) → Q_MagmaTyrant (Lv24). Bir ara halkayi atlayip sonrakini alabiliyorsan prereq bozuk demektir.
- [ ] **Sayac dogru mob'u sayiyor:** Ornegin Q_AshOrcs aktifken Kor Tazisi oldur → sayac ARTMAMALI; Kul Orku oldur → artmali.
- [ ] **Oduller:** Sirasiyla Buyuk Can Iksiri x4, Adamantin Bot, Adamantin Kask, Adamantin Zirh, Adamantin Kilic gelmeli; XP/altin 5400/3500'den 12000/8000'e kademeli artmali (hepsi Q_FrostLich'in 4500/3000'inin uzerinde).
- [ ] **Gorev paneli tasmiyor (onemli):** Zincir acikken gorevli NPC panelinde artik 5 gorev daha var (toplam 20 gorev tanimi). Tum alinabilir gorevler ekrana sigmali, "odul al" satirlari erisilebilir olmali — daha once iki kez overflow duzeltmesi yapilmisti, bu dilimden sonra TEKRAR bak.

## Adamantin tier (yeni esya kademesi)

- [ ] **Dukkan listesi tasmiyor (onemli):** Saticiyi ac — satin-alma sutununda 4 esya daha var (toplam 25 ekipman + 7 tuketilebilir). Tum satirlar panele sigmali ve tiklanabilir olmali; satir yuksekligi okunamayacak kadar kuculmemeli. Kucultuyorsa HUD'da kaydirma/sutun bolme gerekiyor demektir — bu, bu dilimin en olasi gorsel sorunu.
- [ ] **Statlar ve seviye:** Adamantin Kilic 62 atak, Adamantin Zirh 38 def, Adamantin Kask 16 def, Adamantin Bot 22 def; hepsi Lv16 gerektirmeli. Lv16 altinda kusanmayi dene → reddedilmeli.
- [ ] **Upgrade calisiyor:** Bir Adamantin parcasini +1/+2'ye yukselt → mevcut upgrade maliyeti/sansi/yanma kurallari aynen islemeli (yeni tier ozel bir durum degil).
- [ ] **Envanter/karakter paneli:** Adamantin parcasini kusan → karakter penceresinde atak/def artisi dogru yansimali, isim panelden tasmamali ("Adamantin Kilic +5" gibi uzun metin).

---

## Onaylanmış (test edilip çalıştığı doğrulandı)

<!-- Test edip onayladığın maddeleri buraya taşı -->

---

# QA DENETIMI: ucuncu bolge / Lv8-15 dilimi (2c6fd23) — 5 bulgu (2026-09-14)

> Bagimsiz QA gecisi. Derleme ZORLA yeniden dogrulandi (commit'in 4 .cpp dosyasinin zaman
> damgasi guncellenip tam derleme yapildi — "up to date" yalanci-basarisi elendi):
> **`Result: Succeeded`, sifir uyari/hata** (2 ceviri birimi gercekten yeniden derlendi).
>
> Asagidaki 4 iddia bagimsiz olarak **DOGRULANDI, bulgu yok** — tekrar test etmeye gerek yok:
> - **Spawner geometrisi:** 8 spawner'in konumu `MobAreaDistance=700` ile yeniden hesaplandi.
>   Ucuncu bolge (F*2800, R*-1600) icin en yakin komsu **1695.2u** (Golge Suikastci) — commit'in
>   ~1695u iddiasi dogru. Buz Lordu (F*4200) icin en yakin **1960u**. Yaricaplar dusuldugunde bile
>   (500+150) en dar bosluk **1045u**, yani 700u kuralinin cok uzerinde. Cakisma YOK.
> - **Level-band:** Lv8-12 bandinda 4 non-elit mob var (8/9/10/12) → havuz bos donmuyor,
>   crash/sonsuz dongu riski yok. `GetRandomInLevelRange` zaten bos band icin `GetRandom()`
>   fallback'ine sahip. (Not: Lv11'de mob yok — bosluk zararsiz, sadece bilgi.)
> - **Elit loot havuzu:** `EliteEquipDropChance = 0.5` (1.0 DEGIL) — dogrulandi. FrostLich
>   MobLevel=13 → MaxReq=10 → filtre `RequiredLevel >= 8` → havuz **tam olarak 4 Mithril parcasi**.
>   Bos/yanlis havuz olusmuyor. Eski elitlerin havuzlari (Lv5/6/7) tek tek yeniden hesaplandi:
>   **degismemis**, Mithril sizmiyor.
> - **Gorev zinciri:** 5 gorevin hedef ArchetypeId'si (`FrostWraith`/`CursedKnight`/`VenomSpider`/
>   `OrcBerserker`/`FrostLich`) mob tablosuyla **birebir** esitleniyor — bu tur uyumsuzluk YOK.
>   Prereq zinciri gecerli (`Q_Boss` mevcut), odul id'leri (`GreaterHealthPotion` + 4 Mithril) mevcut.
>   Mob efekt id'leri (`Slow`/`Poison`/`Rage`) efekt veritabaninda var → a0262a6 buff fix'iyle catisma yok.

## MEDIUM — Buz Lordu haritanin disinda/navmesh disinda kalabilir (ONCE BUNU TEST ET)

Buz Lordu `PlayerStart + Facing*4200u`'e konuyor. Bu, projede **bugune kadar yerlestirilmis en uzak
nokta** (onceki rekor: ikinci bolge ~3225u radyal, Golge Suikastci 2240u eksende). `Lvl_ThirdPerson`
sablon haritasinin o mesafede zemini/navmesh'i olup olmadigi editor acilmadan dogrulanamadi.
Onemli olmasinin sebebi: `Q_FrostLich` zincirin SON halkasi ve tek hedefi o tek forced-spawn mob —
mob dunyadan duserse veya pathlenemezse **zincir kalici olarak tamamlanamaz hale gelir**.
(Ilgili: `MMOGameMode.cpp:156` LichLoc, `MMOMobSpawner.cpp:71` spawn Z ofseti +100.)
Zemin yoksa bu bulgu **HIGH**'a cikar.

- [ ] **Zemin var mi (KRITIK ilk adim):** Play bas → PlayerStart'in baktigi yonde duz ilerle (~4200u).
      Yol boyunca zemin bitip **bosluga dusuyor musun**? Dusuyorsan Buz Lordu da orada demektir.
- [ ] **Buz Lordu gercekten duruyor mu:** O noktaya varinca tek bir Buz Lordu **zeminde duruyor**
      olmali — havada asili, zemine gomulu veya asagi duser halde OLMAMALI.
- [ ] **Pathleniyor mu:** Buz Lordu'na vur ve geri kac → pesinden **yuruyerek** gelmeli. Yerinde
      kayiyor/titriyor/hic hareket etmiyorsa navmesh yok demektir.
- [ ] **Ucuncu bolge icin ayni kontrol:** Eksende ~2800u + SOLA ~1600u noktasinda 5 mob'un hepsi
      zeminde mi, yoksa bir kismi boslukta mi doguyor?

## LOW — `Q_CursedKnights` odulu verildigi anda kusanilamiyor

`Q_CursedKnights` gorevi Lv9'da aliniyor (`MMOQuestTypes.cpp:65-67`) ama odulu **Mithril Bot**,
ve Mithril tier'i **Lv10 gerektiriyor** (`MMOItemDatabase.cpp:60`). Yani oyuncu odulu Lv9'da
aliyor ama cantada bekletmek zorunda. Coken bir sey yok (odul cantaya giriyor, sadece `Equip`
reddediyor). Zincirin diger 3 ekipman odulu bu sorunu tasimiyor (Lv10/12/13 > Lv10).
Oneri (kod bana ait degil): ya gorevin `RequiredLevel`'ini 10 yap, ya odulu Ejder Bot'a cek.

- [ ] **Tetikleme:** Zinciri Lv9'dayken `Q_CursedKnights`'a kadar getir ve tamamla → Mithril Bot
      cantaya gelmeli ama kusanmaya calisinca **"Level 10 gerekli"** reddi almalisin. Odul
      KAYBOLMAMALI — Lv10 olunca kusanilabilmeli.

## LOW — Dukkan satin-alma sutununda yazi artik satir kutusuna sigmiyor — FIXED (bkz. altta yeni commit)

Panel TASMIYOR (otomatik kuculme calisiyor, liste tam sigiyor) — ama **yazi boyutu satirla birlikte
kuculmuyor**. Hesap (`MMOHUD.cpp:1308-1311`): satir sayisi 24 → **29** oldu (21 ekipman + 7
tuketilebilir + 1 iksir). Kullanilabilir yukseklik 446px sabit → `BuyRowH = 446/29 = 15.38px`,
kutu yuksekligi `BuyCellH = 11.38px`. Yazi ise sabit 0.72 olcekte (~12-13px) ciziliyor
(`:1333`, `:1345`) → yazi kendi kutusundan tasip alt satira degiyor. Commit oncesi 24 satirda
`446/24 = 18.6px` idi ve siginiyordu — **esigi asan tam olarak bu commit**. Tiklama bolgeleri
kutularla ayni hesaplandigi icin **tiklama hala dogru calisir**; sorun sadece okunabilirlik.
Bir sonraki tier (4-5 esya daha) durumu belirgin kotulestirir.

- [ ] **Gozle dogrula:** Saticiyi ac (E) → satin-alma sutununda satirlarin yazilari **ust uste
      biniyor / kutu disina tasiyor mu**? Ozellikle listenin alt yarisina bak.
- [ ] **Tiklama hala dogru mu (onemli):** Listenin EN ALTINDAKI esyaya tikla → **tam o esya**
      satin alinmali (bir ust/alt komsusu degil). Kaymis alim varsa bulgu LOW'dan yukari cikar.

## LOW — Bantsiz spawner'lar artik Lv12 mob dogurabilir (latent regresyon, dogrulanmali)

`AMMOMobSpawner` varsayilanlari `MinMobLevel=0 / MaxMobLevel=0` (`MMOMobSpawner.h:39-42`). Bu
durumda arketip secilmiyor ve mob kendi `BeginPlay`'inde `FMMOMobDatabase::GetRandom()`'a dusuyor
(`MMOMob.cpp:103`) — bu fonksiyon **tum non-elit havuzdan** seciyor, yani artik Lv12 Ork Berserk de
dahil. Commit oncesi bu yolun tavani Lv7 idi. **Su an canli bir yol yok**: GameMode'un actigi
spawner'larin hepsi ya `ForcedArchetypeId` ya da bantli. Risk su iki durumda gercege doner:
(a) `Lvl_ThirdPerson` icine **elle yerlestirilmis** bir `AMMOMobSpawner`/`AMMOMob` varsa,
(b) `MMOSpawnMob` debug komutu **bos id** ile cagrilirsa.

- [ ] **Baslangic alani temiz mi:** Play bas, PlayerStart cevresinde ve ana mob alaninda (~1050u)
      dolas → **hicbir Lv8+ mob** (Buz Hayaleti / Lanetli Sovalye / Zehir Orumcegi / Ork Berserk)
      gormemelisin. Gorursen haritada elle konmus bir spawner var demektir.
- [ ] **Debug komutu:** Konsola argumansiz `MMOSpawnMob` yaz → cikan mob Lv12 Ork Berserk olabiliyor
      mu? (Sadece bilgi; debug yolu, oyuncuya acik degil.)

## LOW — Dusuk seviye moblar artik Mithril dusurebiliyor (olcum istegi)

Elit-OLMAYAN mob loot'u item tablosundan **duz rastgele** seciyor (`MMOMob.cpp:701`) — tablo artik
Mithril tier'i de icerdigi icin **Lv1 Goblin bile Mithril Kilic dusurebilir**. Kusanilamaz (Lv10
gerekir) ama **satilabilir**: Mithril Kilic satis ~350 altin, onceki tavan Ejder Kilici ~272 altin
(+%29 tavan artisi). Bu mevcut tasarimin dogal sonucu, yeni bir hata degil — ama ekonomi raporunun
"altin cabuk anlamsizlasiyor" bulgusuyla ayni yone bakiyor, bu yuzden olculmesi isteniyor.

- [ ] **Olcum:** Baslangic alaninda (Lv1-5) ~20 mob oldur → kac tanesinden **Mithril** parcasi dustu?
      2-3'ten fazlaysa erken oyun altin akisi beklenenden hizli demektir, not al.

---

# Lv16-40 kilometre tasi gorevleri (5 gorev, SPAWNER/MOB GEREKTIRMEZ) — 2026-09-15

> Derleme temiz (`Result: Succeeded`), oynanis test EDILMEDI. **Kapsam notu:** bu dilim bilerek
> **hicbir yeni mob, spawner veya harita yerlesimi icermiyor** — sadece `MMOQuestTypes.cpp`'ye
> 5 gorev tanimi eklendi. Sebep: son iki icerik diliminde eklenen elit spawner'lar PlayerStart'tan
> 4200-5600 birim uzaga konuldu ve o mesafede gercek zemin/navmesh olup olmadigi henuz PIE'de
> dogrulanmadi (bkz. yukaridaki "MEDIUM — Buz Lordu haritanin disinda/navmesh disinda kalabilir"
> bulgusu). Bu risk buyutulmeden icerigi derinlestirmek icin sadece oyuncunun mevcut durumunu
> (seviye/altin) kontrol eden gorev tipleri kullanildi.
>
> Yeni gorevler — seviye zinciri: `Q_Level5` → **Q_Level20** (Kidemli Savasci, Lv16'da alinir,
> 9000XP/5500g + Buyuk Can Iksiri x5) → **Q_Level30** (Usta Savasci, Lv24, 16000XP/10000g +
> Buyuk Mana Iksiri x5) → **Q_Level40** (Efsanevi Sovalye, Lv30, 24000XP/15000g + Buyuk Can
> Iksiri x8). Altin zinciri: `Q_Gold1000` → **Q_Gold5000** (Vadinin Tuccari, Lv16, 6000XP) →
> **Q_Gold15000** (Efsanevi Servet, Lv28, 14000XP). Altin gorevleri mevcut desene uyup
> altin odulu VERMEZ (sadece XP) — `Q_Gold500`/`Q_Gold1000` de boyle.
>
> Hizli seviye atlamak icin `MMOGainXP`, altin icin mevcut debug/satis yollari kullanilabilir.

- [ ] **Zincir kilidi (seviye):** `Q_Level5` bitmeden gorevli NPC'de **Kidemli Savasci** GORUNMEMELI.
      Q_Level5 bitip Lv16 olunca listede belirmeli. Ayni sekilde Usta Savasci sadece Kidemli
      Savasci bitince (ve Lv24'te), Efsanevi Sovalye sadece Usta Savasci bitince (ve Lv30'da) cikmali.
- [ ] **Zincir kilidi (altin):** `Q_Gold1000` bitmeden **Vadinin Tuccari** gorunmemeli; Vadinin
      Tuccari bitmeden **Efsanevi Servet** gorunmemeli (ve Lv28 gerekmeli).
- [ ] **Anlik tamamlanma (seviye):** Kidemli Savasci'yi Lv16'da al → panelde ilerleme `16/20`
      gibi gorunmeli. `MMOGainXP` ile Lv20'ye cik → **hicbir mob oldurmeden** gorev tamamlanmis
      olmali ve "odul al" satiri aktiflesmeli.
- [ ] **Anlik tamamlanma (altin):** Vadinin Tuccari aktifken 5000 altina ulas → gorev aninda
      tamamlanmali. **ONEMLI:** bu gorev altini HARCAMAZ ve mevcut bakiyeye bakar — odulu almadan
      once altini 5000'in altina harcarsan gorev tekrar "tamamlanmadi"ya donmeli (bu kasitli,
      mevcut `HaveGold` davranisi).
- [ ] **Oduller dogru geliyor:** Kidemli Savasci → 9000XP + 5500 altin + Buyuk Can Iksiri x5;
      Usta Savasci → 16000XP + 10000 altin + Buyuk Mana Iksiri x5; Efsanevi Sovalye → 24000XP +
      15000 altin + Buyuk Can Iksiri x8. Altin gorevleri (Vadinin Tuccari 6000XP / Efsanevi Servet
      14000XP) **altin VERMEMELI** — bu kasitli, bug degil.
- [ ] **Gorev paneli tasmiyor (en olasi gorsel sorun):** Gorev tanimi sayisi 20 → **25** oldu.
      Gorevli NPC panelini ac (ozellikle Lv16+ bir karakterle, yeni gorevlerin hepsi acikken) →
      tum alinabilir gorevler ekrana sigmali, "odul al" satirlari tiklanabilir olmali. Daha once
      bu panelde iki kez overflow duzeltmesi yapilmisti; 5 gorev daha eklendigi icin TEKRAR bak.
      Satirlar okunamayacak kadar kuculuyorsa veya alttakiler ekran disinda kaliyorsa bildir.
- [ ] **Denge sorusu (sana ait karar):** Lv30/Lv40 gorevleri su an mob icerigi olmayan bir aralikta
      (en yuksek mob Lv24 Magma Tirani). Yani bu iki gorev simdilik uzun bir grind demek. Odul
      egrisi (9000 → 16000 → 24000 XP) sana mantikli geliyor mu, yoksa Lv30/Lv40'i mob icerigi
      gelene kadar geri mi cekelim?
- [ ] **REGRESYON — eski milestone gorevleri:** `Q_Level5` (Guclen) ve `Q_Gold500`/`Q_Gold1000`
      eskisi gibi calismali: Q_Level5 hala Lv1'de alinabilmeli ve Lv5'te tamamlanmali; altin
      gorevleri hala altin odulu vermemeli.

## BULGU #6 duzeltmesi — Warrior/Priest Special0 hasar zinciri Lv45/58'e uzatildi

Onceki denetimde tespit edilen bulgu: Warrior ve Priest'in Special0 (fiziksel / kutsal hasar)
zinciri Lv24'te tikaniyordu (`War_TempestSlash`, `Pri_WrathOfAngels`) — Lv70 capstone'a kadar
46 seviye boyunca bu iki sinifa YENI hicbir hasar skili gelmiyordu (Special1/Special2'deki
Lv45/58 eklemeleri sadece buff, Power=0). Archer ve Mage ayni aralikta (Special2: SpikeTrap/
PoisonCloud, ArcaneBurst/ManaStorm) yeni hasar skilleri kazandigi icin bu iki sinif "dolu"
kalirken Warrior/Priest "bos" kaliyordu. Duzeltme olarak her iki sinifin Special0 zincirine
Lv45 ve Lv58'de birer yeni hasar skili eklendi (`MMOSkillDatabase.cpp`):

- **Warrior** — `War_WarCyclone` "Savas Kasirgasi" (rutbe 6, Lv45, MeleeDamage, Power 510,
  Mana 195, CD 18s) ve `War_WarDeluge` "Savas Tufani" (rutbe 7, Lv58, AoESelf, Power 660,
  Radius 560, Mana 250, CD 20s). `War_TempestSlash`'tan (rutbe 5) devam eden zincir.
- **Priest** — `Pri_AngelicBlade` "Meleklerin Kilici" (rutbe 4, Lv45, RangedDamage, Power 510,
  Range 1500, Mana 195, CD 18s) ve `Pri_AngelicStorm` "Meleklerin Firtinasi" (rutbe 5, Lv58,
  AoETarget, Power 660, Range 1600, Radius 560, Mana 250, CD 20s). `Pri_WrathOfAngels`'tan
  (rutbe 3) devam eden zincir.

Power/Mana/CD degerleri Archer'in `Arc_SpikeTrap`/`Arc_PoisonCloud` ve Mage'in
`Mag_ArcaneBurst`/`Mag_ManaStorm` skilleriyle (ayni seviyedeki hasar skilleri) ayni olcekte
tutuldu. NOT: gorev talimatinda bu referans skillerin "Power"i 195/250 olarak anildi, ancak
gercek deger bu (Mana=195/250, Power=510-520/660-680) — dolayisiyla yeni skiller gercek Power
olcegine (510/660) hizalandi, boylece Warrior/Priest'in DPS'i Archer/Mage'den kopuk kalmadi.
`RequiredMasteryRank` her iki sinifta da onceki zincir halkasindan buyuk (6>5, 7>6, 4>3, 5>4)
ve karakter seviyesini asmiyor (`RequiredMasteryRank <= RequiredLevel`), bu yuzden BULGU #5/#6
tarzi bir rutbe-seviye ihlali yok. `AppliedEffectId` bos birakildi (saf hasar skili, buff degil).

- [ ] **Ogrenme sirasi dogrulama:** Bir Warrior'i Lv45'e getir, Special0 rutbesini 6'ya
      yatirmayi dene → **basarili olmali**. Ayni karakteri Lv44'te dene → **basarisiz olmali**
      ("Level 45 gerekli" / rutbe hatasi).
- [ ] **Zincir kirilmiyor mu:** `War_WarCyclone`'u rutbe 5 (TempestSlash acilmadan) yatirmaya
      calis → mastery sistemi rutbeleri sirali actigindan rutbe 6'ya once rutbe 1-5'in
      yatirilmis olmasi gerekir, atlama olmamali.
      Ayni kontrolu Priest'te `Pri_AngelicBlade` (rutbe 4) icin tekrarla.
- [ ] **Hasar gozle dogrulama:** Lv45+ bir Warrior ile Savas Kasirgasi'ni bir mob'a kullan →
      hasar sayisi diger Lv45 hasar skilleriyle (~500-520 bandi) ayni buyuklukte gorunmeli,
      asiri dusuk/yuksek olmamali. Lv58'de Savas Tufani icin de ayni kontrol (~650-680 bandi).
      Priest'te Meleklerin Kilici/Firtinasi icin ayni kontrolu tekrarla.
- [ ] **UI:** Skill agacinda (HUD skill paneli) Warrior/Priest Special0 sutununda artik 7 (Warrior)
      / 5 (Priest) satir gorunmeli, isimler ("Savas Kasirgasi", "Savas Tufani", "Meleklerin
      Kilici", "Meleklerin Firtinasi") kesilmeden/tasmadan okunabilir olmali.

---

# BAGIMSIZ QA DENETIMI - Dorduncu bolge (Lv16-24, commit e6ba822) - 2026-09-15

> Denetci: qa-kontrolcu (kod DEGISTIRILMEDI, sadece okundu + derlendi + hesaplandi).
> **Derleme:** Build.bat ilk calistirmada "Target is up to date" dedi (gercekten derlemedi); commit'in
> 4 dosyasinin zaman damgasi guncellenip ZORLA yeniden derlendi -> `Result: Succeeded`, **0 uyari,
> 0 hata**. (Not: derleme sirasinda calisma agacinda baska bir ajanin devam eden isi vardi -
> `MMOQuestTypes.cpp` + `MMOSkillDatabase.cpp` icinde Lv16-40 kilometre tasi gorevleri; derleme
> o degisiklikleri de icerecek sekilde temiz gecti.)
>
> **PIE calistirilamadi** (editor/oyun erisimi yok). Asagidaki bulgular kod ve `.uasset` ikili
> verisinden hesaplandi; "dogrula" adimlari editorde/PIE'de 1-2 dakikada teyit edilebilir.

> Asagidaki 6 iddia bagimsiz olarak **DOGRULANDI, bulgu yok** - tekrar test etmeye gerek yok:
> - **Spawner geometrisi (10 spawner):** Kod yorumundaki 9 mesafe degeri tek tek yeniden hesaplandi ve
>   **birebir tutuyor**: Buz Lordu 2778.5u, Ikinci bolge 2912.0u, Golge Suikastci 4129.1u, Ork Reisi
>   4716.3u, Orumcek Ana 4791.7u, Ucuncu bolge 4882.6u, Ana alan 5144.2u, Kadim Golem 5448.0u,
>   Magma Tirani 4800.0u. En dar ikili tum haritada (bu commit'e ait DEGIL, eskiden beri var):
>   Ana alan <-> Orumcek Ana = **703.5u** - 700u kuralinin hemen uzerinde, yaricaplar (350+150=500)
>   dusuldugunde cakisma yok. Yeni iki spawner icin cakisma/uzerine binme YOK.
> - **Level-band filtreleme:** Lv16-22 bandinda **4 non-elit** mob var (16/18/20/22) ->
>   `GetRandomInLevelRange` bos donmuyor, `GetRandom()` fallback'ine dusmuyor, elit sizintisi yok.
>   (Lv17/19/21 bos - Lv11 boslugunun ayni deseni, zararsiz.)
> - **MagmaTyrant elit loot orani:** `EliteEquipDropChance = 0.5f` (1.0 DEGIL) - dogrulandi.
>   `EliteEquipDropCount=2`, `EliteMinItemPlus=3`, `EliteMaxItemPlus=5`; `MaxPlus=8` sinirini asmiyor.
> - **Elit loot havuzu (REGRESYON YOK):** Filtre `RequiredLevel <= MobLevel && >= MaxReq-2`
>   (`MMOMob.cpp:678-691`). Magma Tirani Lv24 -> MaxReq=16 -> havuz `>=14` -> **tam olarak 4 Adamantin
>   parcasi** (Mithril=10 girmiyor). Buz Lordu Lv13 -> MaxReq=10 -> havuz `>=8` -> **hala tam olarak 4
>   Mithril parcasi** (Adamantin=16 > 13 oldugu icin havuza giremez). Lv5/6/7 elitlerinin havuzu da
>   degismedi. **Adamantin hicbir eski elite sizmiyor.**
> - **Gorev zinciri:** 5 gorevin hedef ArchetypeId'si (`EmberHound`/`MagmaGolem`/`AshOrc`/
>   `InfernalWraith`/`MagmaTyrant`) mob tablosuyla **birebir** esitleniyor; kill-credit zaten
>   ArchetypeId uzerinden isliyor (`MMOMob.cpp:605` -> `MMOQuestComponent.cpp:230`). Prereq zinciri
>   gecerli (`Q_FrostLich` mevcut), odul id'leri (`GreaterHealthPotion` + 4 Adamantin parcasi) mevcut.
>   **Onceki turun `Q_CursedKnights` hatasi TEKRARLANMAMIS:** her Adamantin odulu (RequiredLevel=16)
>   Lv18/20/22/24 gorevlerinden veriliyor -> odul verildigi anda kusanilabiliyor.
> - **Onceki QA fix'leriyle catisma YOK:** a0262a6 (buff/mana/skill kitabi), ffd2cc4 (dukkan yazi
>   olcegi), 75d0277 (DivineRestoration) - hicbirinin dokundugu kod bu commit'te degismedi; kullanilan
>   efekt id'leri (`Poison`/`Slow`/`Rage`) efekt veritabaninda mevcut. Ekipman kusanma seviye kontrolu
>   sunucu tarafinda (`MMOInventoryComponent.cpp:240`) - Adamantin icin de gecerli.

## CRITICAL - FIXED, IKI ASAMADA (commit 2ca4d7a + 0353f01) - Haritanin zemini sadece 4000x4000u: 10 spawner'in 6'si (YENI IKISI DAHIL) bosluga konuyor

> Ek not: ilk duzeltme (2ca4d7a) sadece spawner MERKEZ noktalarini sinira aldi, SpawnRadius'u
> (bolge spawner'larinda 500u) hesaba katmamisti - kendi bagimsiz python dogrulamamda 6
> spawner'in hala tastigini buldum. 0353f01 ile hem merkez HEM SpawnRadius sinir icine alindi,
> tum 45 ikili mesafe (sadece degismemis eski "Ana alan-Ork Reisi" haric) dogrulandi. Asagidaki
> orijinal bulgu metni tarihi kayit olarak birakildi, guncel koordinatlar icin MMOGameMode.cpp'yi
> (commit 0353f01) esas al.

Onceki turda "dogrulanamadi" diye MEDIUM birakilan Buz Lordu off-map riski, `Content/__ExternalActors__`
ikili verisi cozulerek **olculdu** ve risk gerceklesiyor gorunuyor:

- Zemin tek bir aktor: `SM_Template_Map_Floor`, aktor olcegi **(4, 4, 1)**, mesh yari-boyu **(500, 500, 25)**
  -> zemin **X ve Y'de sadece -2000 .. +2000** (toplam 4000x4000u).
- Duvarlar: `SM_Cube` (yari-boy 50) aktorleri olcek 40 (=4000u uzunluk) ile +-2000'de.
  Haritadaki HICBIR statik mesh aktorunun koordinati **2000u'yu asmiyor** (66 OFPA dosyasi tarandi).
- `PlayerStart` = **(0, 0, 302)**, rotasyon sifir -> `Facing = +X`, `RightDir = +Y`.
- `NavMeshBoundsVolume` (0,0,200) olcek (140,140,30) -> hacim +-14000u, yani **hacim buyuk ama altinda
  zemin yok**; navmesh sadece gercek geometri uzerinde uretilir.

Buna gore spawner'lar (`MMOGameMode.cpp:51-207`):

| Spawner | Konum (X, Y) | Zemin uzerinde mi |
|---|---|---|
| Ana alan | (1050, 0) | EVET |
| Ork Reisi | (1540, 0) | EVET |
| Orumcek Ana | (1120, +700) | EVET |
| Kadim Golem | (1120, -700) | EVET |
| Golge Suikastci | (2240, 0) | **HAYIR** (240u disarida) |
| Ikinci bolge | (2800, +1600) | **HAYIR** |
| Ucuncu bolge | (2800, -1600) | **HAYIR** |
| Buz Lordu | (4200, 0) | **HAYIR** (2200u disarida) |
| **Dorduncu bolge (YENI)** | **(5600, +2400)** | **HAYIR (X'te 3600u, Y'de 400u disarida)** |
| **Magma Tirani (YENI)** | **(5600, -2400)** | **HAYIR** |

Sonuc - dogruysa: bu commit'in **tum icerigi oynanamaz** (5 normal mob + elit bosluga doguyor, oyuncu
+-2000'deki duvarlari asamiyorsa oraya yuruyemez bile), `Q_EmberHounds` -> `Q_MagmaTyrant` zincirinin
**5 gorevi de tamamlanamaz**. Ayni sey Lv6-15 icerigi (ikinci/ucuncu bolge, Buz Lordu, Golge Suikastci)
icin de gecerli. Ek risk: dusen moblar kill-Z'de yok olunca spawner `TopUpInterval` (dorduncu bolge icin
**varsayilan 4sn**) ile surekli yenilerini uretir -> sunucuda sonu gelmez spawn/yok-olma dongusu.

**Duzeltme YONU (kodu ben yazmiyorum):** ya mesafe carpanlari harita sinirina (+-2000u) sigacak sekilde
kucultulmeli, ya haritaya gercek zemin/navmesh eklenmeli (docs/MapOptions.md'deki kasaba haritasi isi),
ya da spawn noktalari zemine raycast ile oturtulmali. **Yeni bolge eklemeden once bu cozulmeli** -
her tier ayni hatayi buyuterek tekrarliyor.

- [ ] **EN KRITIK ADIM - zemin sinirini bul:** Play bas, PlayerStart'in baktigi yonde (+X) duz kos.
      Yaklasik **2000u**'de gorunmez bir duvara mi carpiyorsun / zemin bitip bosluga mi dusuyorsun?
      (Referans: ana mob alani ~1050u'de, Ork Reisi ~1540u'de - oralar zemin uzerinde olmali.)
- [ ] **Dorduncu bolge gercekten var mi:** Editorde Simulate (veya PIE'de serbest kamera) ile
      X=5600, Y=+2400 noktasina git. 5 mob **zeminde duruyor** mu, yoksa **asagi mi dusuyorlar /
      hic yoklar mi**? Dusuyorsa bulgu ONAYLANDI.
- [ ] **Magma Tirani ayni kontrol:** X=5600, Y=-2400 -> tek Magma Tirani zeminde mi?
- [ ] **Gorev zinciri gercekten tamamlanabiliyor mu (asil test):** `MMOGainXP` ile Lv16'ya cik,
      `Q_EmberHounds`'u al ve **5 Kor Tazisi oldurebiliyor musun**? Mob bulamiyorsan zincir kilitli demektir.
- [ ] **Regresyon - eski bolgeler:** Ayni kontrolu Buz Lordu (X=4200), ucuncu bolge (X=2800, Y=-1600) ve
      Golge Suikastci (X=2240) icin de yap. Bunlar da bostaysa sorun bu commit'e ozel degil, **6 spawner'i
      birden** kapsiyor ve oncelik bu commit'ten daha yuksek.
- [ ] **Log kontrolu:** PIE cikisinda `[MMODBG] SpawnOne: requested=... actualLoc=...` satirlarina bak.
      `actualLoc` ile `requested` ayni kaliyorsa mob havada doguyor; surekli yeni SpawnOne satirlari
      akiyorsa dusup yok olma dongusu var demektir.

## MEDIUM - Dukkan satin-alma sutunu 33 satira cikti: yazi ~5px, tiklama kutusu 9.5px

ffd2cc4 (yazi olcegini satirla orantili kucultme) tasmayi durdurdu ama **liste buyumeye devam ediyor**.
Hesap (`MMOHUD.cpp:1306-1314`): satir sayisi Adamantin ile **29 -> 33** (1 iksir + 7 tuketilebilir +
**25** ekipman). Kullanilabilir yukseklik sabit 446px ->
`BuyRowH = 446/33 = 13.5px`, `BuyCellH = 9.5px`, `BuyTextScale = 13.5/26 = 0.52`
-> ekipman yazilari **0.72 x 0.52 = 0.374** olcekte, yani kabaca **5 piksel** yuksekliginde.
Panel tasmiyor ve tiklama bolgeleri hala dogru hesaplaniyor, ama:
1. Liste okunamayacak siniri zorluyor (Lv gereksinimi ve fiyat 5px yazidan okunmali),
2. `BuyTextScale`'in alt siniri **0.45** - bir sonraki tier (37+ satir) bu tabana carpar ve
   **ffd2cc4'un duzelttigi tasma geri gelir** (yazi `LY+4`'ten baslayip 8px'lik kutuyu asar).

Duzeltme yonu: dukkan listesine kaydirma/sayfalama, ya da sutun bolme, ya da seviye filtresi
(oyuncunun seviyesinin cok ustundeki tier'leri gizle).

- [ ] **Okunabilirlik:** Saticiyi ac (E) -> satin-alma sutunundaki **Adamantin** satirlarini (en altta)
      okuyabiliyor musun? Fiyat ve "(Lv16)" secilebiliyor mu, yoksa lekeye mi donusmus?
- [ ] **Tiklama hala dogru mu (onemli):** Listenin **EN ALTINDAKI** esyaya (Adamantin Bot) tikla ->
      tam o esya alinmali, bir ust/alt komsusu degil. Kaymis alim varsa bulgu HIGH'a cikar.
- [ ] **Satir ust uste binmiyor:** Listenin alt yarisinda yazilar birbirine degiyor mu?

## LOW - Adamantin tier'i, Lv1 moblarinin "piyango" tavanini ~%37 yukseltti

Elit-OLMAYAN mob loot'u item tablosundan **duz rastgele** seciyor (`MMOMob.cpp:701`; `EquipDropChance`
varsayilan 0.25). Tablo artik Adamantin'i de icerdigi icin **Lv1 Goblin bile Adamantin Kilic dusurebilir**.
Kusanilamaz (Lv16 gerekir, sunucu reddediyor) ama **satilabilir**: Adamantin Kilic +0 Siradan satis
~478 altin, Efsanevi nadirlikte ~1433 altin. Onceki tavan (Mithril Kilic) ~350 / ~1051 altin idi.
Onceki turda Mithril icin acilan ayni bulgunun bir kademe buyugu; yeni bir hata degil, **her tier ile
buyuyen** bir ekonomi kaymasi. Ilgili: ekonomi raporunun "altin cabuk anlamsizlasiyor" bulgusu.

- [ ] **Olcum:** Baslangic alaninda (Lv1-5) ~20 mob oldur -> kac tanesinden **Adamantin** parcasi dustu?
      2-3'ten fazlaysa erken oyun altin akisi beklenenden hizli demektir, not al.
- [ ] **Kusanma kilidi calisiyor:** Dusen Adamantin parcasini Lv16 altinda kusanmayi dene -> **reddedilmeli**,
      esya cantada KALMALI.

## LOW (bilgi) - Lv16-22 bandinda seviye bosluklari

Band 4 mob iceriyor: Lv16 / 18 / 20 / 22. **Lv17, 19, 21 bos.** Spawner banddan rastgele sectigi icin
bu bir hata degil (Lv11 boslugunun ayni deseni) - sadece "seviye atladikca yeni mob gorme" hissi
kademeli degil sicramali oluyor. Kod degisikligi gerektirmez.

- [ ] **Gozlem:** Dorduncu bolgede 10-15 mob'un nameplate'ine bak -> sadece Lv16/18/20/22 gorunmeli;
      araya Lv17/19/21 veya **Lv13 ve alti** bir mob KARISMAMALI (band sizintisi kontrolu).

---

# QA denetimi 2026-09-15 - yeni kilometre tasi gorevleri (5f92908) + 32 yeni skill (2e401c2, 0cb5744)

Kapsam: `EMMOQuestType::ReachLevel` / `HaveGold` tamamlanma akisi (client-authority / cift-odul),
yeni skillerin overflow / mastery-rutbe / dizi-sinir taramasi, bu turda eklenen tum Server RPC'leri.
**Derleme:** `Build.bat MyProjectEditor Win64 Development` -> **Result: Succeeded**, 0 uyari.
(Ilk cagri "Target is up to date" dedi; 3 cpp'nin zaman damgasi guncellenip GERCEK derleme
zorlandi - link + `UnrealEditor-MyProject.dll` uretildi.)

## HIGH - FIXED (commit e57a472) - Warrior ve Archer'in Lv80 nihai skilleri MATEMATIKSEL OLARAK kullanilamaz (mana tavani < mana maliyeti)

Bu, daha onceki "BULGU #2" duzeltmesinin (`ManaPerLevel = 4.f` eklenmesi, `MMOStatsComponent.h:162`)
**yetersiz kaldigi** durum - sorun kapandi saniliyor ama iki sinif icin hala acik.

Formul (`MMOStatsComponent.cpp:159`): `MaxMana = 25 + Intelligence*5 + Level*4`.
Ekipman **mana/INT vermiyor** - `SetEquipmentBonuses` sadece Attack/Defense aliyor
(`MMOStatsComponent.cpp:415`, `MMOInventoryComponent.cpp:212`). Yani mana tavani SADECE INT ve seviyeye bagli.

Sinif baslangic INT'leri (`MMOSkillComponent.cpp:156-159`): Warrior 6, Archer 8, Priest 18, Mage 22.
**INT'ye hic yatirim yapmayan** (yani sinifin tasarlandigi build'i oynayan) bir Lv80 karakter icin:

| Sinif | Lv80 MaxMana | Lv80 capstone | Maliyet | Sonuc |
|---|---|---|---|---|
| Warrior | 25+30+320 = **375** | `War_DoomsdayBlow` | **400** | **ASLA kullanilamaz** (-25) |
| Archer  | 25+40+320 = **385** | `Arc_HuntersReckoning` | **400** | **ASLA kullanilamaz** (-15) |
| Priest  | 25+90+320 = **435** | `Pri_HandOfHeaven` | 420 | ancak %96.5 mana ile, tek sefer |
| Mage    | 25+110+320 = **455** | `Mag_AbsoluteAnnihilation` | 420 | ancak %92 mana ile |

Warrior'in Lv70 capstone'u da sinirda: `War_TitanWrath` 320 mana, Lv70 MaxMana = 25+30+280 = **335**
(%4.5 pay). Mana rejeni 7/sn oldugu icin 320 mana ~46 saniyede doluyor ama skilin CD'si 20sn -
skill fiilen CD ile degil mana ile kilitli.

Neden onemli: `CanLearn` mana kontrolu YAPMIYOR - oyuncu 3 skill puanini harcayip skili ogrenebiliyor,
sonra kullanmaya calisinca `UseSkill` (`MMOSkillComponent.cpp:452`) "Yeterli mana yok!" deyip her
seferinde reddediyor. Yani **geri alinamayan bir puan harcamasi + calismayan bir icerik**.
Duzeltme yonu (mmo-developer karar verir): `ManaPerLevel` 4 -> ~5, veya capstone mana maliyetlerini
dusurmek, veya ekipmana INT/MaxMana bonusu eklemek. Hangisi olursa olsun **INT'si 6 olan bir Lv80
Warrior kendi nihai skilini atabilmeli.**

- [ ] **Warrior dogrulama (asil test):** Yeni Warrior ac -> `MMOGainXP` ile Lv80'e cik ->
      **stat puanlarinin HICBIRINI INT'ye verme** -> HUD'da `MP  x / y` satirina bak: ikinci sayi (MaxMana)
      **375** mi? Ise bulgu ONAYLANDI.
- [ ] Ayni karakterde Ustalik 4 agacina 2 rutbe yatirip `Kiyamet Darbesi`ni ogren -> hotbara at ->
      **tam mana ile** tusa bas. Ekranda "Yeterli mana yok!" cikiyorsa skill **hic kullanilamiyor** demektir.
- [ ] **Archer dogrulama:** Ayni akisi Archer'da tekrarla -> MaxMana **385**, `Avcinin Sonu` (400 mana)
      basilamamali.
- [ ] **Kontrol grubu (fix dogru mu):** Ayni Warrior'da STR/VIT yerine **INT'ye 5 puan** ver ->
      MaxMana 400'e cikmali ve skill **tam mana iken tam olarak 1 kez** atilabilmeli.
- [ ] **Priest/Mage regresyon:** Priest'te `Cennetin Eli` (420) ve Mage'de `Mutlak Yikim` (420) tam mana
      ile atilabiliyor mu? Atilabiliyorsa bulgu sadece Warrior/Archer'a ozel demektir.

## HIGH - FIXED (commit e57a472, egri Lv25+ icin dogrusala donusturuldu) - Lv45+ yeni skiller ve Q_Level40 XP egrisi yuzunden pratikte ULASILAMAZ

32 yeni skilin buyuk kismi Lv45 / Lv58 / Lv70 / Lv80 kapilarinin arkasinda, ama XP egrisi
(`MMOStatsComponent.cpp:145-149`: `100 * 1.25^(Level-1)`) ile **oyundaki en yuksek XP kaynagi**
(Magma Tirani eliti, **6800 XP**, `MMOMobArchetype.cpp:213`; en iyi normal mob InfernalWraith **3700 XP**)
arasinda kapanmayacak bir ucurum var. Mob XP'si seviye farkina gore **olceklenmiyor** -
`MMOMob.cpp:601` duz `GainExperience(XPReward)` cagiriyor, yani Lv70'te de Lv24 elitinden 6800 aliniyor.

Kumulatif XP (`400 * (1.25^(N-1) - 1)`) ve 6800 XP'lik elit kille karsiligi:

| Hedef | Gereken toplam XP | ~Elit kill |
|---|---|---|
| Lv24 (mevcut icerik tavani) | 67.000 | 10 |
| Lv30 (`Q_Level30`) | 258.000 | 38 |
| **Lv40 (`Q_Level40`)** | **2.4 milyon** | **354** |
| **Lv45** (War_WarCyclone / Arc_SpikeTrap / Mag_ArcaneBurst / Pri_AngelicBlade / Pri_HolyShield) | **7.3 milyon** | **1.080** |
| **Lv58** (WarDeluge / PoisonCloud / ManaStorm / AngelicStorm / Unbreakable / ResiliencePrayer) | **133 milyon** | **19.600** |
| **Lv70** (4 capstone) | **1,95 milyar** | **287.000** |
| **Lv80** (4 nihai skill) | **18 milyar** | **2,66 milyon** |

Yani commit `2e401c2` + `0cb5744` ile eklenen skillerin **yarisindan fazlasi** (Lv45 ve uzeri, 20 skil)
ve `Q_Level40` gorevi, mevcut icerikle oynanarak **hicbir zaman acilamaz**. Bu bir denge/zevk
karari degil, aritmetik: gerekli kill sayisi Lv70'te alti haneye, Lv80'de yedi haneye cikiyor.
Ayrica Lv77'den itibaren tek seviyenin XP'si `MAX_int32`'ye kirpiliyor (`ComputeExperienceForLevel`
icindeki onceki duzeltme), yani son 4 seviye tek basina ~8,6 milyar XP.

Not (ilgili, yeni degil): parti XP'si **bolunmuyor** - `MMOMob.cpp:598-606` menzildeki HER uyeye TAM
`XPReward` veriyor. 8 kisilik parti = 8x XP. Yukaridaki rakamlar tek oyuncu icin.

Duzeltme yonu (mmo-developer/kullanici karari): XP egrisinin buyume katsayisini seviyeye gore
kademelendirmek (or. Lv30+ icin 1.25 -> 1.10), ve/veya Lv25-80 araligina gercek mob/XP icerigi
eklemek. **Simdilik en azindan bilinmesi gereken: Lv45+ skiller test edilemez, sadece `MMOGainXP`
debug komutuyla dogrulanabilir.**

- [ ] **Egriyi dogrula:** `MMOGainXP` ile Lv30'a cik -> stat penceresinde "sonraki seviye icin gereken XP"
      **~65.000** civari mi? Lv40'ta ayni degere bak -> **~600.000** olmali.
- [ ] **Gercek kazanim olcumu:** Lv24 bir karakterle Magma Tirani'ni oldur -> tek kille **6800 XP**
      geliyor mu (HUD XP bari)? Geliyorsa tablodaki kill sayilari gecerli demektir.
- [ ] **Seviye farki cezasi var mi:** `MMOGainXP` ile Lv40'a cik, sonra bir **Lv1 Goblin** oldur ->
      hala **30 XP** mi aliyorsun? Aliyorsan "mob XP'si olceklenmiyor" ONAYLANDI.
- [ ] **Yeni skiller calisiyor mu (debug yoluyla):** `MMOGainXP` ile Lv58'e cik -> her sinifta Lv45 ve
      Lv58 skillerini ogren -> hepsi hasar/buff veriyor mu? (Bu, "ulasilamaz ama BOZUK DA degil"i ayirir.)
- [ ] **Parti XP kontrolu:** 2 oyuncuyla parti kur, menzilde dur, 1 mob oldur -> **ikisi de TAM XP** mi
      aldi? Aldilarsa bolunmeme ONAYLANDI (notu yukaridaki tabloyla birlikte degerlendir).

## MEDIUM - Gorev panelinde 13+ satir metin yerlesimi (2026-09-28 kod duzeltmesi; PIE bekliyor)

`DrawQuestWindow` icin `RowSpace=360px`, `RowH=min(46, 360/ToplamSatir)` ve `CellH=RowH-4`.
ToplamSatir = alinabilir + aktif gorev sayisi. 13 satirda `RowH` yaklasik 27.7px.
Onceki `QuestTextScale` yaziyi kucultuyordu, ancak ikinci metnin Y konumu olculen ilk metin
yuksekligine bagli degildi. Font/olcek degisiminde iki yazi cakisabilir veya odul yazisi hucre
altina gecebilirdi. Yeni yerlesim her iki metnin gercek yuksekligini `GetTextSize` ile olcuyor,
gerekirse ikisini birlikte kucultuyor ve ikinci metni ilk metnin altina, hucre icinde koyuyor.
Gorev tiklama bolgelerinin boyutlari degismedi. MMOHUD.cpp yeniden derlendi ve UE 5.7
`Result: Succeeded` verdi; bu degisiklik icin PIE calistirilmadi.

- [ ] **13+ satir okunurlugu:** Lv16+ karakterle `J` gorev penceresini ac; alinabilir ve aktif
      gorevlerin toplaminda 13+ satira ulas. Gorev adi, aciklama/odul ve komsu satirlar birbirine
      girmemeli; yazilar koyu hucrelerin disina tasmamali.
- [ ] **Yuksek satir sayisi:** Alinabilir gorevleri kabul edip teslim etmeden biriktir; en sikisik
      gorunumde ilk ve son satiri, iki bolum basligini ve panelin alt kenarini kontrol et.
- [ ] **Tiklama:** En alttaki tamamlanmis gorevi tikla; tam o gorevin odulu alinmali. Alinabilir
      bir gorevi de tiklayip dogru gorevin aktif listeye tasindigini kontrol et.

## LOW - "Altin biriktir" gorevleri aslinda "su an cebinde X altin olsun" demek; altin harcayinca geri bozuluyor

`MMOQuestComponent.cpp:135-137` - `HaveGold` tamamlanma kosulu `Inventory->Gold >= TargetCount`.
Altin **dusulmuyor** ve bir kere tamamlandi diye **isaretlenmiyor**: gorev ancak `ClaimReward` ile
`CompletedQuests`'e giriyor. Sonuclari:

1. `Q_Gold5000` "TAMAM" gorunurken oyuncu dukkandan alisveris yapip 5000'in altina duserse gorev
   sessizce **tekrar tamamlanmamis** hale doner. HUD'daki "- TAMAM, tikla!" yazisi bir sonraki
   cizime kadar yanlis kalir.
2. Oyuncu "TAMAM" gorup tiklarken arada altini harcamissa sunucu istegi **hicbir mesaj vermeden**
   reddeder (`ClaimReward` 272. satir `return`). Oyuncu neden olmadigini anlayamaz.
3. `Q_Level20/30/40` oduleri (5500 / 10000 / 15000 altin) `Q_Gold5000` ve `Q_Gold15000`'i **aninda**
   tamamliyor. Tasarim boyleyse sorun yok, ama "biriktirme" hissi kayboluyor - not olarak burada.

**Cift-odul riski YOK** (asagidaki "saglam" bolumune bak) - bu sadece bir davranis/UX bulgusu.

- [ ] **Geri bozulma:** 5000 altina ulas, `Q_Gold5000`'i al, gorev panelinde "TAMAM" gorundugunu dogrula ->
      **pencereyi kapatmadan** dukkana gidip 1000 altinlik bir sey al -> gorev panelini tekrar ac ->
      satir artik "4000/5000" gostermeli, "TAMAM" **kaybolmali**.
- [ ] **Sessiz red:** Ayni durumda (altin 5000'in altinda) "TAMAM" satiri hala ekrandayken tikla ->
      hicbir sey olmuyor ve **hicbir mesaj cikmiyor** mu? Cikmiyorsa bulgu ONAYLANDI.
- [ ] **Zincir dogrulama:** `Q_Level20`'yi teslim et (+5500 altin) -> `Q_Gold5000` aninda TAMAM oluyor mu?

## LOW - `ServerAcceptQuest` / `ServerClaimReward` her cagrida tum dunyayi tariyor (spam edilebilir)

`MMOQuestComponent.cpp:51` - `IsOwnerNearQuestGiver()` her cagrida `TActorIterator<AMMOQuestGiverNPC>`
ile **tum seviyeyi** dolasiyor. Bu iki Server RPC **Reliable** ve istemciden **kisitlamasiz**
cagrilabiliyor; hileli bir istemci saniyede yuzlerce `ServerClaimReward` gonderip sunucuda bos yere
dunya taramasi yaptirabilir. Su an haritada cok az gorevli NPC oldugu icin etkisi kucuk, bu yuzden
LOW - ama NPC sayisi artarsa buyur. (Ayni desenin vendor tarafinda da oldugunu not edelim.)
Duzeltme yonu: gorevli NPC listesini bir kez cache'lemek ve/veya RPC'ye kisa bir cooldown koymak.

- [ ] **Gozlem (opsiyonel, profil gerektirir):** `stat unit` acikken gorev penceresini acip odul-al
      satirina hizlica 20-30 kez tikla -> `Game` suresi gozle gorulur sekilde ziplyor mu?

## LOW - Olu alan: `FMMOQuestProgress::bClaimed` hicbir yerde okunmuyor

`MMOQuestTypes.h:85`'te tanimli ve `AcceptQuest`'te `false` atanip kaydediliyor (`SaveGame` isaretli),
ama hicbir kod **okumuyor**. Cift-odul korumasi aslinda "odul alinca `ActiveQuests`'ten silinir +
`CompletedQuests`'e eklenir" mekanigiyle saglaniyor, `bClaimed` ile degil. Zararsiz ama yaniltici;
ileride biri "cift-odul korumasi bClaimed'de" diye varsayarsa hataya yol acar. Kod degisikligi
aciliyeti yok, temizlik notu.

- [ ] (Test adimi gerekmiyor - kod okuma notu.)

## LOW (latent) - Lv77+ `Experience += Amount` int32 tasmasina karsi korumasiz

`MMOStatsComponent.cpp:295`. Onceki turda `ComputeExperienceForLevel` `MAX_int32`'ye kirpildi, ama
`Experience` birikimi kirpilmadi. Lv77'den itibaren `ExperienceToNextLevel = 2.147.483.647` oldugu
icin `Experience` seviye atlamadan bu degere kadar birikebilir; ~2,1 milyar XP biriktikten sonra
gelen her XP **isaretli tamsayi tasmasi** (UB) yapip `Experience`'i negatife dondurur.
**Pratikte ulasilamaz** (yukaridaki HIGH bulgusuna gore Lv77'ye ulasmak zaten milyonlarca kill
gerektiriyor), bu yuzden LOW - ama XP egrisi duzeltilirse bu kapi acilir, birlikte ele alinmali.

- [ ] (Normal oynanisla test edilemez. XP egrisi duzeltilirse `Experience` birikimine `FMath::Min`
      koruması eklendigini kod incelemesiyle dogrula.)

## SAGLAM - denetlenip TEMIZ cikanlar (regresyon icin burada duruyor)

Asagidakiler bu turda tek tek kontrol edildi ve **sorun bulunmadi**; ileride biri ayni yerden
supheye duserse tekrar bastan incelemesin diye kayda geciyor.

1. **`ReachLevel` / `HaveGold` gorevlerinde client-authority acigi YOK.**
   `IsQuestComplete` (`MMOQuestComponent.cpp:125-139`) degeri **sunucunun kendi** `Stats->Level` ve
   `Inventory->Gold` alanlarindan okuyor. Istemci RPC ile sadece bir `FName QuestId` gonderiyor -
   gonderebilecegi **hicbir ilerleme/seviye/altin degeri yok**. Sahte tamamlama imkansiz.
2. **Cift-odul (double-claim) mumkun degil.** `ClaimReward` odulu verdikten sonra `CompletedQuests.Add`
   + `ActiveQuests.RemoveAll` yapiyor; ikinci cagri `FindActive` null dondugu icin cikiyor, `AcceptQuest`
   da `IsCompleted` ile yeniden almayi engelliyor. Iki RPC ayni karede gelse bile sunucu onlari
   **sirayla** isledigi icin yaris kosulu yok.
3. **`ClaimReward` odul yollari dogru:** `RewardXP` -> `GainExperience` (kirpilmis egri),
   `RewardGold` -> `AddGold` (onceki turda tasmaya karsi sertlestirildi), `RewardConsumableCount`
   pozitif kontrollu, `RewardEquipmentId` item tablosunda dogrulaniyor.
4. **Yeni skillerde int32 tasmasi YOK.** `ManaCost` / `Power` / `Cooldown` hepsi `float`
   (`MMOSkillTypes.h:93-106`); hasar `ApplyDamage(float)` ile akiyor. `int32`'ye donusen tek yer
   HUD'daki `FMath::RoundToInt` cagrilari (en buyuk deger 1725) - tasma mesafesinde degil.
5. **`MasteryRankArray` dizi sinir asimi YOK.** Dizi **agac** ile indeksleniyor (5 eleman), **rutbe**
   ile degil. Warrior Special0'in rutbe 7'ye cikmasi sadece `MasteryRankArray[1]`'in degerini
   buyutur. `RequestInvestMastery` hileli bir enum degeri gelse bile `IsValidIndex` ile eliyor
   (`MMOSkillComponent.cpp:300-307`).
6. **Mastery rutbeleri kesintisiz - ogrenme kilitlenmesi YOK.** `CanLearn` `RequiredMasteryRank > Rank+1`
   ise reddettigi icin her agacta rutbelerin **1,2,3...** diye bosluksuz gitmesi sart. Dort sinifin
   bes agacinin hepsi kontrol edildi: Warrior Special0 1-7, Special1 1-3, Special2 1-3, Special3 1-2;
   Archer/Mage/Priest de bosluksuz. Ulasilamayan skill yok.
7. **Yeni buff/debuff id'lerinin hepsi tanimli.** `RageII`, `IronSkinII`, `IronSkinIII`, `HasteII`,
   `HasteIII`, `RegenII` -> hepsi `MMOEffectDatabase.cpp:49-54`'te var. (Tanimsiz olsaydi 120-150
   mana'lik buff skilleri sessizce hicbir sey yapmayacakti.)
8. **Skill kitabi paneli 18 skile ragmen tasmiyor.** En kalabalik sinif Warrior (18 skil / 5 agac).
   Iki sutunlu dagitim sonrasi Col0 = 9 skil / 3 agac -> `RowH = 31.1`, toplam yukseklik 364px;
   kullanilabilir alan 364px; alt mesaj satirina (PY+460) carpmiyor. Col1 = 9 skil / 2 agac -> `RowH`
   32'de tavanliyor, 344px. **Sinirda ama iciride** - bir sonraki skill eklemesinde tekrar hesaplanmali.
9. **Bu turda yeni/denetlenmemis Server RPC yok.** `git diff 966283a..HEAD` ile tarandi: tek RPC
   degisikligi `ServerLearnSkill` -> **`ServerInvestMastery`** donusumu. Denetlendi: sunucu tarafi
   skill puanini, `rutbe+1 <= Level` kuralini ve dizi sinirini yeniden dogruluyor; istemci tarafi
   `CanLearn` sadece on-kontrol. `ServerAssignToSlot` / `ServerClearSlot` de slot indeksini ve
   `IsLearned`'i sunucuda tekrar kontrol ediyor. Client-authority ihlali yok.
   - Tek kucuk not: `ServerInvestMastery` yatirimin **o sinifta skili olan** bir agaca yapilip
     yapilmadigini kontrol etmiyor - oyuncu puanini bos bir agaca gomebilir. Sadece kendine zarar,
     istismar degil; yine de HUD zaten bos agaci cizmiyor, bu yuzden normal oynanista tetiklenmez.
10. **`GetLifetimeReplicatedProps` eksiksiz.** Yeni `MasteryRankArray` `DOREPLIFETIME` listesinde
    (`MMOSkillComponent.cpp:31`); gorev bileseninin `ActiveQuests`/`CompletedQuests` de kayitli.

- [ ] **Regresyon (bu bolum icin tek test):** Play -> `K` ile skill kitabini ac -> Ustalik agacina
      1 rutbe yatir -> **istemci HUD'inda** rutbe ve yeni skil aninda gorunuyor mu (replikasyon calisiyor)?
      Sonra `J` ile gorev penceresinden bir gorev alip teslim et -> XP/altin/esya odulunun **ucunun de**
      geldigini envanter + stat penceresinden dogrula.

---

# ENTEGRASYON DENETIMI: 2026-09-14/15 degisikliklerinin BIRBIRIYLE etkilesimi (2026-09-18)

> Bu tur tek tek ozellikleri degil, **art arda yapilan degisikliklerin ARASINDAKI** etkilesimi
> denetledi (XP egrisi <-> olum cezasi <-> eski kayitlar <-> mana formulu <-> spawner konumlari
> <-> yeni mob tier'leri). Her degisiklik kendi basina zaten denetlenmisti; burada yalnizca
> **birlesince ortaya cikan** sorunlar var.
>
> **Derleme:** `Target is up to date` yalanci-basarisi alindigi icin `MMOStatsComponent.cpp`'nin
> zaman damgasi guncellenip TAM derleme zorlandi: **5 aksiyon derlendi, `Result: Succeeded`, sifir
> uyari, sifir hata.**

## HIGH #1 - FIXED (commit fd13bd9, AggroRange 600'e cekildi + SpawnRadius 400'e indirildi, python ile dogrulandi) - Lv16-22 moblar oyuncunun DOGUS NOKTASINI aggro menzilinde tutuyor (yeni bolge tasinmasinin yan etkisi)

`MMOGameMode.cpp:131-183` (spawner konumlari) + `MMOMobArchetype.cpp:109-137` (Lv16-22 tier'i) +
`MMOMob.h:51,58,62` (AggroRange/LeashRange/WanderRadius).

Spawner'larin harita icine cekilmesi (`2ca4d7a`/`0353f01`) zeminin **PlayerStart'in arkasindaki**
yarisini kullanima acti. Ayni turda eklenen Lv8-12 (`2c6fd23`) ve Lv16-22 (`e6ba822`) tier'leri de
oraya kondu. Iki degisiklik ayri ayri dogru, **birlesince** oyuncunun dogdugu noktayi yuksek seviye
moblarin aggro menziline sokuyor. PlayerStart = (0,0), spawner merkezleri PlayerStart'a gore:

| Spawner | Merkez (F,R) | Yaricap | PlayerStart'a EN YAKIN spawn noktasi | Mob seviyesi | Mob AggroRange |
|---|---|---|---|---|---|
| Ucuncu bolge | (-1300, 0) | 500 | **800u** | Lv8-12 | 1000-1250 |
| Ikinci bolge | (-650, +1300) | 500 | **953u** | Lv6-7 | 950-1250 |
| **Dorduncu bolge** | (-650, -1300) | 500 | **953u** | **Lv16-22** | **950-1300** |

Aggro saf mesafe kontrolu; **seviye farki ve gorus hatti kontrolu YOK** (`MMOMob.cpp:436-466`).
Ustune her mob kendi HomeLocation'i cevresinde `WanderRadius = 450` ile geziniyor, yani uzak
kenarda spawn eden mob bile oyuncuya 450u daha yaklasabiliyor. `LeashRange = 1800` oldugu icin
mob PlayerStart'a kadar gelip **leash'lenmeden orada dovusmeye devam ediyor** (evine uzaklik
en fazla ~1453u).

Somut senaryo: taze Lv1 karakter (MaxHealth = 50 + 12x8 + 1x12 = **158**, Defense = 6).
Cehennem Hayaleti (Lv22, hasar 82, aggro 1300, hiz 430) -> vurus basina 82-6 = **76 hasar**,
**3 vurusta olduruyor**. Magma Tirani (Lv24, hasar 95) yakinda degil ama Kor Tazisi (Lv16, hiz 460)
kacilamayacak kadar hizli. Olum -> 5sn -> `RespawnAtStart` ayni PlayerStart'a koyuyor
(`MMOCharacter.cpp:519-534`) -> mob hala 953-1400u otede ve aggro menzilinde -> **olum dongusu**.
(Olu oyuncu hedeflenmiyor, bu yuzden ceset kampi yok; ama `Revive` aninda tekrar hedeflenebilir.)

- [ ] **Play bas, HICBIR SEY YAPMADAN dogdugun yerde 60 saniye bekle.** Arkandan (kameranin
      arkasi, PlayerStart'in -X yonu) mob gelip sana saldiriyor mu? Geliyorsa **hangi mobun adi**
      ekranda yaziyor not et (Kor Tazisi / Magma Golemi / Kul Orku / Cehennem Hayaleti = Lv16-22 = HIGH).
- [ ] **Play bas -> kamerayi 180 derece cevir -> geriye dogru yuru.** Kac adim sonra Lv16-22 bir mob
      sana kosmaya basliyor? 950-1400u arasinda basliyorsa bulgu dogrulandi.
- [ ] **Oldukten sonra:** respawn olur olmaz ayni mob tekrar uzerine geliyor mu? (Olum dongusu testi -
      arka arkaya 2 kez olursen bulgu kesinlesir.)

## HIGH #2 - FIXED (commit fd13bd9, Lyra guvenli on tarafa tasindi) - Sifaci Lyra NPC'si Lv8-12 bolgesinin aggro menzilinin ICINDE

`MMOGameMode.cpp:229` -> Lyra `Origin + (-250, +200, 60)` = PlayerStart'in **arkasinda**.
Ucuncu bolge spawner'inin (Lv8-12) merkezine uzaklik 1069u, yaricap 500 dusuldugunde
**en yakin spawn noktasina 569u**. Bu moblarin aggro menzili 1000-1250. Ikinci bolgeye (Lv6-7)
en yakin spawn noktasi da **670u**.

Yani "ol, dirilme, Lyra'ya git, altin verip tam canlan" dongusunun **iyilesme noktasi dusman
bolgesinin icinde**. Lyra'nin konumu spawner'lar tasinmadan ONCE yazilmisti ve yorumu hala
"mob alanina ters yon" diyor - o yorum artik gecersiz, cunku mob alani da o tarafa tasindi.

Ek not (LOW, ayni satirlar): NPC'ler `Origin + FVector(...)` ile **ham dunya eksenine** gore
konumlaniyor, spawner'lar ise `Facing.Vector()` / `RightDir` ile. PlayerStart'in rotasyonu
sifir oldugu surece ikisi ortusuyor; biri haritada PlayerStart'i **dondururse** NPC'lerle
spawner'lar sessizce ayrisir.

- [ ] **Play -> arkani don -> Sifaci Lyra'ya yuru -> yaninda 30 saniye dur.** Mob geliyor mu?
- [ ] **Play -> kasten ol -> dirildikten sonra Lyra'ya kosup iyilesmeyi dene.** Islem yarida
      kesiliyor mu (mob saldirisi yuzunden) yoksa rahatca tamamlanabiliyor mu?

## MEDIUM #3 - Olum cezasi Lv25+ icin cok agir DEGIL ama oyun-suresi maliyeti ~87 kat artti

`MMOStatsComponent.cpp:335-349`. Sordugun sorunun **net cevabi: bu bir REGRESYON DEGIL.**
Formul `Min(Experience, ExperienceToNextLevel * 0.1)` -> kayip her zaman **bar'in %10'u**,
her seviyede ayni oran; clamp sayesinde **seviye kaybi imkansiz** ve seviye atladiktan hemen
sonra olursen kayip ~0. Ustelik yeni DOGRUSAL egri, eski ustel egriye gore cezayi **kuculttu**:
Lv50'de eski bar 5.605.000 XP -> ceza 560.500; yeni bar 946.176 -> ceza **94.618** (yaklasik 6 kat AZ).

**Ama** asil sorun cezanin kendisi degil, **XP GELIRININ sabit kalmasi**. Oyundaki en yuksek
XP veren mob Magma Tirani (Lv24, 6800 XP) ve en iyi normal mob Cehennem Hayaleti (Lv22, 3700 XP);
Lv25 ustu icin **hic mob yok**. Cezanin "kac kill" karsiligi:

| Seviye | ExperienceToNextLevel | Olum cezasi (%10) | = kac Cehennem Hayaleti (3700 XP) |
|---|---|---|---|
| Lv10 | 745 | 75 | 0,02 |
| Lv24 | 16.941 | 1.694 | **0,46** |
| Lv26 | 58.176 | 5.818 | 1,57 |
| Lv50 | 946.176 | 94.618 | **25,6** |
| Lv79 | 2.019.176 | 201.918 | 54,6 |

Magma Tirani ile olcersen Lv50'de bir olum = **13,9 Magma Tirani kill'i**; o elit `MaxMobs=1` ve
`TopUpInterval=60sn` oldugu icin dakikada en fazla 1 kez oldurulebiliyor -> **tek olum ~14 dakika
saf elit farm'i siliyor**. Lv24'te ayni olum 0,46 kill (yaklasik 1 dakika). Yani **oran 30-55 kat
bozuluyor** ve bu tamamen "Lv25-80 icin icerik yok" probleminin turevi.

- [ ] **`MMOGainXP` ile Lv50'ye cik -> XP barini yarilayacak kadar XP kazan -> kasten ol.**
      Ekranda cikan "Oldun! -N EXP" mesajindaki N degeri **94.618 civarinda** mi? (Cikmiyorsa
      ya `HasAuthority` yolu ya da mesaj yolu bozuk demektir, ayri bulgu.)
- [ ] **Ayni testi seviye atlar atlamaz (Experience ~0 iken) tekrarla** -> kayip **0 veya cok kucuk**
      olmali (clamp calisiyor mu). Negatif deger veya seviye DUSMESI gorursen KRITIK, hemen bildir.
- [ ] **Lv80'de ol** -> `Experience` MaxLevel'da sifirlandigi icin ceza **0** olmali.

## MEDIUM #4 - Lv25 -> Lv26 gecisinde tek seviyede 2,75 kat XP duvari

`ComputeExperienceForLevel` (`MMOStatsComponent.cpp:157-173`). Egri Lv25'e kadar `x1,25`
buyuyor, Lv25'ten sonra `+37.000` sabit ekliyor. Sinirdaki iki deger:

- Lv25 -> Lv26: **21.176 XP** (5,7 Cehennem Hayaleti)
- Lv26 -> Lv27: **58.176 XP** (15,7 kill) -> **tek seviyede x2,75 sicrama**

Oyuncu 24 seviye boyunca "her seviye %25 daha zor" temposuna alistiktan sonra tam Lv26'da
duvara carpiyor. Matematiksel olarak hatali degil (bilincli bir tasarim karari), ama oynanista
**ani ve aciklanamayan bir yavaslama** olarak hissedilecek. Yumusatma karari mmo-developer'a ait.

Ayni bolumden **DOGRULANAN** hesap: commit `e57a472`'nin "Lv25->80 toplami ~8.000 Magma Tirani
kill'i" iddiasi **dogru** - bagimsiz hesapladim: toplam **56.109.680 XP** = 8.251 Magma Tirani
veya **15.165 Cehennem Hayaleti** kill'i. Iddia abartili degil.

- [ ] **`MMOGainXP` ile tam Lv25 ol, XP barini gozle -> Lv26 ol -> tekrar gozle.** Bar'in
      dolma hizi gozle gorulur sekilde yavasliyor mu? (Hissiyat testi; kod dogru.)

## MEDIUM #5 - `ManaPerLevel = 5.5` capstone'u "atilabilir" yapti ama "tekrar atilabilir" yapmadi

`ManaPerLevel` degisikligi (`e57a472`) **kendi hedefini tutturmus**: en dusuk INT'li sinif
(Warrior, INT=6) Lv80'de `MaxMana = 25 + 6x5 + 80x5.5 = 495` -> en pahali skil 400 mana ->
**calisiyor** (dogrulandi, yorumdaki 495 rakami birebir tutuyor). Lv70'te de 440 >= 320 ✓.

**Ama** `ManaRegenPerSec = 7.f` **sabit, seviyeyle hic buyumuyor** ve savasta
`CombatRegenMult = 0.25` ile **1,75/sn**'ye dusuyor (`MMOStatsComponent.h:190-200`).
Skill maliyetleri Lv1'den Lv80'e **42 kat** buyuduğu halde (10 -> 420) regen sabit:

- Lv80 Warrior, Kiyamet Darbesi (400 mana, CD 26sn): 1 atistan sonra 95 mana kalir. CD boyunca
  savasta yenilenen mana **45,5**. Ikinci atis icin gereken 305 mana -> **~174 saniye savas icinde
  bekleme**. Yani nihai skil pratikte "CD'de bir" degil **"dovuste bir"**.
- Lv8 Buyucu, Zincir Yildirim (42 mana, CD 10sn): havuz 234, CD basina regen 17,5 -> **5-6 art
  arda atis**, sonra beklemeli. Oran cok daha saglikli.

- [ ] **Yuksek seviyeye cik, Lv80 capstone'u ogren ve bir dovuste ARKA ARKAYA 2 kez atmayi dene.**
      Ikincisinde "yeterli mana yok" alip cooldown bitmesine ragmen bekliyor musun? (Beklenen
      davranis: evet - bu bulgunun dogrulanmasi.)

## SAGLAM - denetlenip TEMIZ cikanlar (yanlis alarm vermeyelim diye acikca yaziyorum)

1. **Eski kayit dosyalari + yeni XP egrisi: GECERLI BIR BULGU YOK.** `FMMOCharacterSaveData`
   (`MMOSaveGame.h:17-94`) icinde `ExperienceToNextLevel` alani **hic yok**; `FillSaveData`
   onu yazmiyor (`MMOStatsComponent.cpp:676-686`) ve `ApplySaveData` her yuklemede
   `ExperienceToNextLevel = ComputeExperienceForLevel(Level)` ile **Level'dan yeniden hesapliyor**
   (satir 709). Ustune `Level` `Clamp(1, MaxLevel)`, `Experience` `Max(0, ...)` ile dogrulaniyor.
   Egri ne kadar degisirse degissin **eski kayit bozuk esikle yuklenmez**. Bu tasarim dogru.
2. **Mob navigasyonu YON-BAGIMSIZ.** `MMOMob.cpp:368-416` (leash/eve donus) sadece
   `FVector::Dist(GetActorLocation(), HomeLocation)` kullaniyor; `IdleWander` (satir 533-561)
   aciyi `FRandRange(0, 2*PI)` ile tam daireden seciyor. Hicbir yerde sabit yon/isaret varsayimi
   yok -> spawner'larin PlayerStart'in **arkasina** (negatif Facing) tasinmasi navigasyonu
   etkilemiyor. (Navmesh yoksa `IdleWander` hic gezinmiyor, `MoveToLocation` yerine duz-cizgi
   fallback'e dusuyor - ikisi de yonden bagimsiz.)
3. **Dokunulmayan 4 eski spawner gercekten sinir icinde.** `MobAreaDistance = 700` ile bagimsiz
   hesapladim: Ana alan F=1050/r=350 -> 1400; Ork Reisi F=1540/r=150 -> 1690; Orumcek Ana
   F=1120,R=+700/r=150 -> 1270/850; Kadim Golem F=1120,R=-700/r=150 -> ayni. Hepsi
   `|F|+r <= 1850` kuralinin **icinde**. `0353f01`'in "4 eski spawner'a dokunulmadi" karari
   dogru, celiski yok. (Zeminin `[-2000,+2000]` oldugu iddiasi OFPA olcumune dayaniyor,
   bunu bagimsiz **dogrulayamadim** - PIE testi hala sart, bkz. dosya basindaki 1. madde.)
4. **Replikasyon eksiksiz.** `GetLifetimeReplicatedProps` (satir 69-89) `Level` ve `Experience`
   dahil 16 alani kaydediyor. `ExperienceToNextLevel` **kasten** replike edilmiyor; istemcide
   `OnRep_Level` icinde ayni deterministik fonksiyonla yeniden hesaplaniyor (satir 768-774) -
   bant genisligi acisindan dogru, sunucu/istemci ayrismasi imkansiz.
5. **Lv1-24 eski skillerin mana dengesi BOZULMADI.** Zincir Yildirim (Lv8, 42 mana, CD 10sn) ve
   Ilahi Sifa (Lv7, 55 mana, CD 20sn) zaten **mana ile degil COOLDOWN ile** sinirliydi: Lv8'de
   havuz 190 -> 234'e cikti (+%23) ama tek atis maliyeti havuzun sadece %18'i. Lv1'de fark daha
   da kucuk (85 -> 90,5, +%6,5). `ManaPerLevel` terimi seviyeyle **dogrusal** buyudugu icin etkisi
   nerdeyse tamamen gec-oyunda; erken oyun dengesi pratikte ayni. **Spam'lenebilirlik degismedi.**
6. **Loot seviye filtresi (`453b2dc`) yeni tier'lerle celismiyor.** `RequiredLevel <= MobLevel+3`
   (`MMOMob.cpp:710`); esya tablosundaki **en yuksek** `RequiredLevel` = 16 (Adamantin tier).
   Lv16 Kor Tazisi bile filtreden `<= 19` ile geciyor -> **erisilemez esya yok**. Tavan filtre
   degil, esya tablosunun kendisi.
7. **`MaxPlus = 8` ile tablolar tutarli.** `Chances[MaxPlus]` ve `BurnChances[MaxPlus]` ikisi de
   tam 8 elemanli (`MMOItemDatabase.cpp:103,117`); son deneme (+7 -> +8) %20 basari / %100 yanma.
   `9 -> 8` degisikliginden artakalan tutarsizlik yok.

## LOW - kucuk notlar

- **Eski LOW bulgusu ARTIK GECERSIZ:** yukaridaki "Lv77+ `Experience += Amount` int32 tasmasi"
  maddesi yeni egriyle **kapandi**. `ExperienceToNextLevel` artik Lv79'da bile sadece 2.019.176
  (eskiden `MAX_int32` = 2.147.483.647'ye kirpiliyordu), yani `Experience` seviye atlamadan
  2,1 milyara birikemez. Normal oynanista **ulasilamaz**; sadece `MMOGainXP` gibi bir hile
  komutuyla tek seferde ~2 milyar XP verilirse hala tasar. Kod degisikligi gerekmiyor.
- **Eski kayitta "bankada duran XP" (kozmetik).** `ApplySaveData` `Experience`'i oldugu gibi
  yukluyor ama seviye-atlama dongusunu **calistirmiyor**. Eski USTEL egriyle Lv26+ kaydedilmis
  bir dosyada `Experience` yeni bar'i asabilir (or. Lv50, Experience=2.000.000 > bar 946.176).
  Sonuc: HUD'da `EXP 2000000 / 946176` yazar ve **ilk mob oldurusunde tek seferde birkac seviye
  birden atlanir**. Cokme yok, veri kaybi yok; XP bari da `DrawStatBar` icinde
  `Clamp(0,1)`'lendigi icin (`MMOHUD.cpp:44`) tasip cizmiyor. Boyle bir kaydin **var olma
  ihtimali dusuk** (eski egriyle Lv26'ya ulasmak zaten neredeyse imkansizdi, ustelik bir sure
  `MaxLevel=20` idi) -> aciliyet yok.
- **"Oldun! -N EXP" mesaji dedicated server'da hic gorunmez.** `MMOCharacter.cpp:501-512`'de
  `IsLocallyControlled()` kontrolu `HasAuthority()` blogunun **icinde**; listen-server/host'ta
  calisir, dedicated sunucuda istemciye hic ulasmaz. Su an tek-oyunculu/PIE test edildigi icin
  gorunmez degil - ileride dedicated'a gecilirse bir `Client` RPC gerekecek.

- [ ] **(Opsiyonel, eski kayit varsa)** Eski bir `.sav` ile gir -> stat penceresinde XP satiri
      `EXP <buyuk sayi> / <kucuk sayi>` seklinde mi? Oyleyse bir mob oldur -> tek seferde birden
      fazla seviye atliyor mu? (LOW; sadece boyle bir kayit elindeyse test et.)

---

# QA - commit `7bc76d0` (Lv25+ XP egrisi parabol + seviyeyle buyuyen mana yenilenmesi) - 2026-09-24

**Ozet:** Kritik veya orta bulgu YOK. 5 kucuk (LOW) bulgu + 1 duzeltme (eski test beklentileri artik
yanlis). Derleme temiz. Yukaridaki MEDIUM #4 (Lv26 duvari) ve MEDIUM #5 (sabit mana yenilenmesi)
bu commit ile **kapandi** (asagidaki python dogrulamasi).

**Derleme:** editor kapaliyken `MMOStatsComponent.cpp` zaman damgasi guncellenip zorla derlendi ->
`[1/4] Compile Module.MyProject.3.cpp`, link basarili, **`Result: Succeeded`**, uyari/hata yok.

## SAGLAM - bagimsiz python hesabiyla dogrulananlar

1. **Lv1-25 egrisi DEGISMEDI.** Eski ve yeni formul 1..25 araliginda 25 seviyenin hepsinde birebir
   ayni (fark listesi bos). `InLevel <= 0` guard'i da ayni (ikisi de 100 doner).
2. **Lv25 -> Lv26 duvari gitti.** Lv25 bar 21.176, Lv26 bar **26.492** -> oran **1,251** (onceki
   adimlarin hepsi 1,250). Eskiden 58.176 (x2,75) idi. `4725` katsayisi ustel egrinin Lv25'teki
   egimi (`21176 x ln(1,25) = 4725,2`) -> gecis puruzsuz. Sonrasinda oran kademeli dusuyor
   (Lv30 1,20 / Lv50 1,07 / Lv80 1,035) ama mutlak artis her seviyede buyuyor.
3. **Lv1-80 kesin monoton artan** (hicbir seviyede esit/azalan yok).
4. **int32 sinirinin cok uzaginda.** En buyuk bar (Lv80) 2.068.826 = `INT32_MAX`'in **%0,096**'si.
   Kumulatif Lv1->80 toplami bile 40.153.013 (sigar). `MAX_int32` kirpmasi artik hic devreye girmiyor.
5. **Commit mesajindaki rakamlar DOGRU:** Lv26 26.492 (~26,5K), Lv50 508.676 (~509K), Lv79
   1.999.682 (~2,0M), **Lv25->80 toplam 40.068.710 XP (~40M)** = **5.892 Magma Tirani** (~5.900)
   = 10.829 Cehennem Hayaleti. Eski lineer egriye (56.109.680 / 8.251 kill) gore %29 daha az.
6. **Mana yenilenmesi + `CombatRegenMult`:** carpan toplam hiza uygulaniyor
   (`(ManaRegenPerSec + Level*ManaRegenPerLevel) * Mult`, `MMOStatsComponent.cpp:66-67`).
   Lv80: savas disi 27/sn, savas ici 6,75/sn. Dogru.
7. **Moblar etkilenmiyor.** Yenilenme kodu `IsPlayerControlled()` erken-donusunun (`cpp:48-52`)
   ARKASINDA; moblar bu satira hic ulasmiyor.
8. **Sunucu-otoriterlik/replikasyon:** yenilenme sadece `HasAuthorityInternal()` tick'inde; `Mana`
   zaten replike + `OnRep_Mana` HUD'u guncelliyor. Yeni `ManaRegenPerLevel` `EditDefaultsOnly`,
   replike degil - dogru (istemci hesaplamiyor). BP'de yeni alan icin eski override olamaz ->
   C++ varsayilani 0,25 gecerli.

## LOW-1 - Eski lineer-egri kayitlarinda "bankada XP" artik DAHA OLASI

`ApplySaveData` (`cpp:709-721`) `Experience`'i oldugu gibi yukluyor, seviye-atlama dongusunu
calistirmiyor. Yeni bar'lar Lv26-79 araliginda eski lineer bar'lardan **KUCUK** (yalnizca Lv80
buyuk): or. Lv26 eski 58.176 -> yeni 26.492; Lv50 946.176 -> 508.676. Yani `e57a472`..`7bc76d0`
arasinda Lv26+ iken kaydedilmis ve `Experience` yeni bar'i asan bir kayit HUD'da `EXP 40000 / 26492`
gibi gorunur, ilk XP kazaniminda tek seferde 1+ seviye atlar. Onceki turdaki ayni LOW bulgusundan
farki: Lv25->26 sadece 21.176 XP (~3 Magma Tirani) oldugu icin **test kaydinin Lv26'da olmasi
gercekci**. Cokme/veri kaybi yok, bir sonraki XP kazanciyla kendiliginden duzeliyor.
Onerilen yon (mmo-developer): `ApplySaveData` sonunda `Experience >= ExperienceToNextLevel` ise
ayni while dongusunu calistirmak ya da Experience'i bar-1'e kirpmak.

- [ ] **(Lv26+ eski kaydin varsa)** Oyunu ac -> C ile stat penceresi -> XP satirinda ilk sayi ikinciden
      buyuk mu? Buyukse bir mob oldur -> birden fazla seviye mi atladin? (Beklenen: evet, bulgu dogrulanir.)

## LOW-2 - Mana yenilenmesi erken oyunu da degistirdi (commit mesaji sadece gec-oyundan bahsediyor)

`Level * 0.25` terimi Lv1'den itibaren isliyor. Savas disi: Lv1 7,25 (+%3,6), Lv8 **9,0 (+%29)**,
Lv24 **13,0 (+%86)**. Savas ici Lv8 1,75 -> 2,25/sn. Onceki turdaki SAGLAM #5 ("Lv1-24 mana dengesi
bozulmadi") MaxMana icin yazilmisti; yenilenme artik erken oyunda da degisti. Skiller zaten
cooldown-sinirli oldugu icin spam riski dusuk; asil etki **dovusler arasi bekleme suresinin
kisalmasi**. Bilincli bir karar mi **dogrulanmali** (tasarim karari mmo-developer/kullanicinin;
sadece gec-oyun isteniyorsa `Max(0, Level-25)` benzeri bir esik kullanilabilir).

- [ ] **Lv8 Buyucu ile mana'yi sifira indir -> savas disinda bekle -> 234 mana kac saniyede
      doluyor?** Beklenen ~26 sn (eskiden ~33 sn). Hissiyat "fazla hizli" mi not et.

## LOW-3 - Capstone "dovuste bir"den "~1,7 CD'de bir"e indi, "her CD'de bir" degil (bug degil, bilgi)

Lv80 Savasci, Kiyamet Darbesi (`MMOSkillDatabase.cpp:106-107`: 400 mana, CD 26 sn), MaxMana 495:
atistan sonra 95 kalir; 26 sn'lik CD boyunca savas ici yenilenme 26 x 6,75 = 175,5 -> **270,5 < 400**,
yani CD bittiginde ikinci atis hala "yeterli mana yok" der. Tekrar atmak icin gereken sure
305 / 6,75 = **~45 sn** (eskiden ~174 sn). Iyilesme 3,9 kat; commit'in "makul surede" ifadesiyle
uyumlu, tasarim hedefi "her CD'de bir" ise hala tutmuyor.

- [ ] **(Yukaridaki MEDIUM #5 testinin YENI beklentisi)** Lv80 Savasci ile dovus icinde Kiyamet
      Darbesi at -> CD bitince tekrar dene -> "yeterli mana yok" almali; **~45 sn** sonra (yine dovus
      icinde, hasar almaya devam ederken) atilabilmeli. 174 sn bekletiyorsa degisiklik calismiyor.

## LOW-4 - `ComputeExperienceForLevel` ustundeki eski yorum artik yanlis

`MMOStatsComponent.cpp:150-159` hala "Lv25'ten sonra egri DOGRUSALA donusuyor ... ~8.000 Magma
Tirani ... ~55.6M XP" diyor; hemen altindaki yeni yorum (163-166) parabol/40M diyor. Kod dogru,
sadece okuyani yaniltan celiskili dokumantasyon.

## LOW-5 - `ManaRegenPerLevel` / `ManaRegenPerSec` icin alt sinir yok

`MMOStatsComponent.h:193,197` - `meta=(ClampMin=0)` yok. Bir tasarimci BP'de negatif deger girerse
`FMath::Min(MaxMana, ...)` sadece USTTEN kirptigi icin Mana 0'in altina iner. Sadece yanlis
konfigurasyonla tetiklenir; oynanista ulasilamaz.

## DUZELTME - yukaridaki eski bolumlerdeki rakamlar artik GECERSIZ

MEDIUM #3 tablosu ve test adimi lineer egriye gore yazilmisti. Yeni degerler (olum cezasi = bar'in %10'u):

| Seviye | ExperienceToNextLevel | Olum cezasi | = kac Magma Tirani (6800) |
|---|---|---|---|
| Lv24 | 16.941 | 1.694 | 0,25 |
| Lv26 | 26.492 | 2.649 | 0,39 |
| Lv50 | 508.676 | **50.868** | 7,5 |
| Lv79 | 1.999.682 | 199.968 | 29,4 |

- [ ] **(MEDIUM #3 testinin YENI beklentisi)** `MMOGainXP` ile Lv50'ye cik -> XP barini yarilayacak
      kadar XP kazan -> kasten ol -> "Oldun! -N EXP" mesajinda N **~50.868** olmali (eski 94.618 DEGIL).
- [ ] **(MEDIUM #4 testinin YENI beklentisi)** `MMOGainXP` ile Lv25 ol -> C ile stat penceresi, bar
      21.176 -> Lv26 ol -> bar **26.492** olmali (eski 58.176 DEGIL). Lv27'de 32.990, Lv50'de 508.676.
- [ ] **Lv79'a cik** -> stat penceresinde bar **1.999.682** yazmali; XP bari dolu/tasmis cizilmemeli.

---

# Canlanma korumasi (respawn protection) — 4 sn hasar/aggro muafiyeti (2026-09-24)

> Derleme temiz (`Result: Succeeded`). Dogus noktasi yakininda agresif mob varsa olum dongusunu
> engelleyen savunma katmani. Oyuncu canlaninca (dogal respawn VE Sifaci diriltmesi) ve oyuna ilk
> girince (possess) `SpawnProtectionDuration` (varsayilan 4 sn, BP'de `MMO|Combat` altinda) boyunca
> sunucuda hasar almaz, moblar onu hedef almaz. Oyuncu saldirirsa (normal vurus ya da hasar veren
> skill) koruma aninda biter. Iyilestirme/buff skilleri korumayi BOZMAZ. Moblar etkilenmez.

- [ ] **Ilk giris:** Play bas → HP barinin SAGINDA mavi kutuda **"KORUMA"** etiketi gorunmeli, ~4 sn sonra kaybolmali ve ekrana "Canlanma korumasi bitti" yazmali.
- [ ] **Hasar almiyor:** Oyuna girer girmez konsola `MMODamage 50` yaz → can DUSMEMELI (koruma aktif). 4 sn sonra ayni komut → can dusmeli.
- [ ] **Olum sonrasi:** Bir moba kendini oldurt (ya da `MMODamage 9999`, koruma bittikten sonra) → 5 sn sonra dogus noktasinda canlan → "KORUMA" etiketi ~4 sn gorunmeli.
- [ ] **Mob aggro yapmiyor:** Agresif bir mob'un aggro menziline girip orada ol/canlan (ya da mob kovalarken korumali haldeyken yanina git) → koruma suresince mob sana KOSMAMALI/VURMAMALI (etrafta gezinmeli); koruma bitince normal aggro'ya donmeli.
- [ ] **Saldiri korumayi bitirir:** Koruma aktifken bir moba sol tik ile vur → etiket ANINDA kaybolmali, "Canlanma korumasi bitti" yazmali, mob sana karsilik vermeli.
- [ ] **Hasar skili korumayi bitirir:** Koruma aktifken hotbar'dan bir hasar skili (or. Warrior `War_PowerStrike`) kullan → koruma aninda bitmeli. Bir iyilestirme/buff skili (or. `War_IronSkin`, Priest sifasi) kullan → koruma BITMEMELI.
- [ ] **Sifaci diriltmesi:** Olu iken Sifaci Lyra ile erken dirilis → dirilince de "KORUMA" etiketi ~4 sn gorunmeli.
- [ ] **PvP (2 pencereli PIE, Listen Server):** `MMOPvP 1` acikken bir oyuncu olup canlansin → diger oyuncu korumali oyuncuya vurunca HASAR GITMEMELI; 4 sn sonra gitmeli. `MMOPvP 0` iken eskisi gibi "PvP kapali" demeli (degismedi).
- [ ] **Istemcide etiket (2 pencereli PIE):** Istemci penceresindeki oyuncu olup canlaninca KENDI HUD'unda "KORUMA" etiketi gorunmeli (replike bayrak) ve bitince mesaj cikmali.
- [ ] **Regresyon:** Moblar hasar almaya/olmeye devam etmeli; koruma bittikten sonra oyuncunun hasar alma, olum cezasi (-EXP), 5 sn respawn geri sayimi eskisi gibi calismali.
- [ ] **Tasarim notu (karar senin):** Sure 4 sn; Sifaci diriltmesi de korumayi veriyor (sadece dogal respawn istenirse `HandleRevived` yerine `RespawnAtStart`'a tasinabilir).

---

# QA / Game Design - "Yeni oyuncunun ilk 10 dakikasi" denetimi (sadece kod okuma + python) - 2026-09-24

> **Kapsam:** Lv1 dogus (PlayerStart (0,0), Facing=+X), sinif secimi/baslangic skili, hub NPC'leri,
> ilk gorev zinciri (Q_Goblins -> Q_Bandits), baslangic ekonomisi, dogus noktasinin tehlikesi.
> PIE oynanmadi; tum rakamlar kod sabitlerinden + bagimsiz python (Monte Carlo) hesabindan.
> Varsayimlar: harita zemini X/Y [-2000,2000], navmesh var (Summer_scene'de `NavMeshBoundsVolume`
> bulundu -> `IdleWander` aktif), oyuncu yurume hizi BP varsayilani ~500 (**dogrulanmali**).
> Calisma kopyasindaki commit edilmemis **4 sn canlanma korumasi** asagidaki bulgularin HICBIRINI
> cozmuyor (koruma 4 sn; asagidaki riskler 30 sn - 10 dk olceginde).
>
> **Ozet:** 2 HIGH (hub/dogus noktasi yuksek seviye elit ve bolge moblarinin gezinti+aggro
> menzilinde; onceki HIGH#1/#2 duzeltmesi `WanderRadius`'u hesaba katmamis), 4 MEDIUM (ana zincir
> temposu, mob Enrage'i etkisiz, sinif degistirme puan kaybi, sinif-degistirme ile tam-can exploit'i),
> 3 LOW. Ulasilamayan gorev / yanlis mob adi **YOK** (asagida SAGLAM).

## HIGH-1 - Hub NPC'leri ve dogus noktasi, ELITLERIN (Magma Tirani Lv24 dahil) gezinti+aggro menzilinde

`MMOGameMode.cpp:66-198` (elit spawner konumlari) + `MMOMobArchetype.cpp:153-238` (elit AggroRange
1100-1350; HIGH#1 duzeltmesi sadece NORMAL moblari 600'e cekti, elitlere dokunmadi) +
`MMOMob.h:62` `WanderRadius=450` + `MMOMob.cpp:537-565` (`IdleWander`: ev +- 450u) +
`MMOGameMode.cpp:204-238` (hub NPC'leri (250,+-200), (400,0), (250,-350)).

Aggro saf 3B mesafe; seviye farki kontrolu yok. Elit ev noktasi spawner merkezinden tam 150u
(`FRandRange(150,150)`), ustune 450u gezinti. Bagimsiz hesap (en yakin olasi mesafe vs aggro):

| Elit (Lv, hasar) | Nokta | Evdeyken min | Gezinirken min | Aggro |
|---|---|---|---|---|
| **Magma Tirani (Lv24, 95)** | Lyra (250,-350) | 1374 | **924** | 1350 |
| **Magma Tirani** | Gorevli (250,-200) | 1476 | **1026** | 1350 |
| **Magma Tirani** | Kael (400,0) | 1530 | **1080** | 1350 |
| Kadim Golem (Lv7, 26) | Lyra | **788 (EVDE BILE)** | 338 | 850 |
| Ork Reisi (Lv6, 24) | Kael | **990 (EVDE BILE)** | 540 | 1200 |
| Orumcek Ana (Lv5, 16) | Satici / Kael | **853 (EVDE BILE)** | 403 | 1100 |
| Golge Suikastci (Lv7, 25) | Satici | 1476 | 1026 | 1300 |

Monte Carlo (oyuncu noktada 30 sn kipirdamadan dursa, en az bir kez aggro alma olasiligi):
- **Lyra'nin yaninda: Magma Tirani %59**, Kadim Golem %75, Ork Reisi %67.
- **Gorevli'nin yaninda: Magma Tirani %34**, Ork Reisi %73, Kadim Golem %61.
- **Kael'in yaninda: Magma Tirani %24**, Ork Reisi %95, Orumcek Ana %98.
- PlayerStart'ta: Orumcek Ana %47, Ork Reisi %22, Kadim Golem %5.

Lv1'in kaybettigi dovusler (TTK/TTD sn, iksirsiz): Magma Tirani **her sinifi ~2.4-3.3 sn'de**
olduruyor (Lv1 hasari 95-Def = 87-90/vurus). Kadim Golem: 4 sinifin hepsi kaybeder (Warrior
16.2/15.8). Ork Reisi: Buyucu (18.3/11.2) ve Priest kaybeder. Tirani hizi 270, Golem 180 -> fark
edilirse kacilabilir; ama oyuncu NPC penceresi acikken/diyalog okurken fark etmeyebilir. Leash
1800 >= hub'a olan ev mesafesi (~1374-1530) -> Tirani hub'a kadar gelir, leash'lenmez. Oldukten
sonra oyuncu (0,0)'da dogar (Tirani'ya gezinti-min 1345 < 1350) -> **olum dongusu mumkun**.

Onerilen yon (mmo-developer): elitlerin AggroRange'ini ve/veya `WanderRadius`'unu elit spawner'lari
icin kucultmek (or. elitlerde gezinti 0-150), ya da Magma Tirani/Kadim Golem spawner'larini hub'dan
uzaklastirmak. Kural: `|spawner-NPC| - SpawnRadius - WanderRadius >= AggroRange + pay`.

- [ ] **Play -> Sifaci Lyra'nin yanina yuru -> HICBIR SEY YAPMADAN 60 sn bekle.** Magma Tirani (turuncu,
      devasa, Lv24) ya da Kadim Golem (gri, devasa) sana kosuyor mu? Adini not et. Tirani geldiyse HIGH dogrulandi.
- [ ] **Ayni testi Gorevli'nin ve Kael'in yaninda tekrarla** (60'ar sn). Ork Reisi / Orumcek Ana kac saniyede geliyor?
- [ ] **Lv1 iken Magma Tirani seni kovalarken dogus noktasina kac** -> Tirani (0,0)'a kadar geliyor mu? Oldukten
      sonra canlaninca (4 sn koruma bitince) Tirani hala yakinda mi? (Olum dongusu testi.)
- [ ] **Gorevli penceresi acikken** (gorev okurken) bir elit gelip vurursa pencere acik kaliyor mu, hasar yazisi goruluyor mu? (UX notu)

## HIGH-2 - Onceki HIGH#1/#2 "FIXED" (fd13bd9) dogrulamasi `WanderRadius`'u unutmus: Lv8-22 moblar dogus noktasina / Gorevli'ye / Lyra'ya yine ulasiyor

`MMOGameMode.cpp:133-185` (SpawnRadius 400) + `MMOMobArchetype.cpp:66-76` (aggro 600, yorum ">=300u
guvenlik payi, python ile dogrulandi") + `MMOMob.cpp:562` (`FRandRange(0, WanderRadius)`).

">=300u pay" hesabi yalnizca **ev noktasina** gore dogru (3. bolge: 1300-400-600 = 300). Mob evden
450u daha gezinebildigi icin gercek pay NEGATIF:

| Bolge (Lv) | Nokta | Evdeyken min | Gezinirken min | Aggro | Pay |
|---|---|---|---|---|---|
| 3. bolge (Lv8-12) | **PlayerStart (0,0)** | 900 | **450** | 600 | **-150** |
| 4. bolge (Lv16-22) | **Lyra** | 909 | **459** | 600 | **-141** |
| 4. bolge (Lv16-22) | **Gorevli** | 1021 | **571** | 600 | -29 |
| 2. bolge (Lv6-7) | Satici | 1021 | 571 | 600 | -29 |
| 2. bolge (Lv6-7) | PlayerStart | 1053 | 603 | 600 | +3 (sinirda) |

Frekans elitlerden dusuk (30 sn beklemede PlayerStart'ta 3. bolge %3.7, Lyra'da 4. bolge %3.1)
ama 10 dakikalik oturumda ve her respawn'da tekrarlanan bir zar. Sonuc olumcul: Kor Tazisi (Lv16,
hiz 460 - oyuncudan kacilamaz olabilir) Lv1'i ~4-6 sn'de, Cehennem Hayaleti ~3 sn'de olduruyor.
Onerilen yon: ya bu 3 bolgenin moblarina `WanderRadius` <= 150 vermek, ya da aggro'yu
`dist - SpawnRadius - WanderRadius - 150` kuralina gore yeniden hesaplamak. Checklist'teki
HIGH#1/#2 "FIXED" etiketi bu kosul dogrulanana kadar **yeniden acik** sayilmali.

- [ ] **Play -> dogdugun yerde (PlayerStart) 3 dakika hic kipirdamadan bekle** (kamerayi arkaya cevir, -X yonu).
      Bir Buz Hayaleti / Lanetli Sovalye / Zehir Orumcegi / Ork Berserk (Lv8-12) sana gelirse HIGH-2 dogrulandi.
- [ ] **Sifaci Lyra'nin yaninda 3 dakika bekle** -> Kor Tazisi / Magma Golemi / Kul Orku / Cehennem Hayaleti (Lv16-22) geliyor mu?
- [ ] **Gorevli'nin yaninda 3 dakika bekle** -> ayni Lv16-22 moblar geliyor mu?

## MEDIUM-3 - Ana gorev zinciri temposu: gorev seviyeleri anlamsiz, oyuncu zincirin ~10 seviye onunde

`MMOQuestTypes.cpp:36-54` + `MMOGameMode.cpp:52-60` (ana spawner: `MaxMobs=3` varsayilan,
`MaxMobLevel=5` -> havuz Goblin/Kurt/Ork/Haydut/Trol, **esit olasilikla** rastgele) +
`MMOMob.cpp:601` (XP'de seviye-farki azalmasi YOK).

Zincir SERI (Goblin -> Kurt -> Ork -> Haydut), ama havuz ORTAK ve sadece 3 canli mob var: hedef mob
yoksa slotu bosaltmak icin baska mob kesmek ZORUNLU, ve bir sonraki gorevin moblari (or. Q_Goblins
sirasinda Kurtlar) sayilmadan harcaniyor. Simulasyon (4000 kosum; oyuncu hedef mobu varsa onu,
yoksa en dusuk seviyeliyi keser; Q_Cleanup/Q_Level5/Q_Trolls paralel alinir):

| Gorev (ReqLv) | Bitene kadar ort. kill (p90) | Bittiginde ort. seviye |
|---|---|---|
| Q_Level5 (1) | 9 | Lv6.5 |
| Q_Cleanup (1) | 10 | Lv7.0 |
| **Q_Goblins (1)** | **22.8 (37)** | **Lv9.8** |
| Q_Wolves (**2**) | 47.8 (66) | **Lv12.5** |
| Q_Orcs (**3**) | 62.8 (84) | Lv13.7 |
| Q_Bandits (**4**) | 82.6 (107) | **Lv14.9** |

8-12 sn/kill varsayiminda (dogrulanmali): **Lv5 ~1.5-2 dk'da, Lv10 ~3-5 dk'da**; ilk gorev
(5 Goblin) ortalama 23, kotu sansta 37 kill = 3-7 dk. Q_Wolves "Lv2" gorevi Lv~10'da aliniyor; gorev
XP'si (200-650) o seviyelerde bar'in %20-35'i -> gorev odulu anlamsizlasiyor. Q_Boss (Ork Reisi,
ReqLv5) Lv~15'te yapiliyor. Bug degil (ilerleme IMKANSIZ degil, tersine cok hizli), ama "ilk 10 dk"
deneyimi: ana zincir erken oyunu yonlendirmiyor, rastgele grind yonlendiriyor.
Onerilen yon (tasarim karari kullanici/mmo-developer): ana spawner `MaxMobs` 3 -> 6-8; Goblin/Kurt
icin ayri kucuk spawner'lar ya da agirlikli havuz; zincirdeki gorevleri paralel alinabilir yapmak;
ve/veya seviye farkina gore mob XP azalmasi.

- [ ] **Yeni karakter -> Gorevli'den Goblin Avi + Temizlik + Guclen al -> ana alanda sadece savas.**
      5 Goblin icin toplam KAC mob kestin, Q_Goblins'i teslim ettiginde seviyen kac? (Beklenen ~20-25 kill, ~Lv9-10.)
- [ ] **Saati tut:** Play'den Lv5'e kac dakika, Lv10'a kac dakika? (Beklenen ~2 / ~4-5 dk.)
- [ ] **Hissiyat:** Ekranda ayni anda sadece 3 mob mu var? "Goblin bekliyorum ama hep Trol/Haydut cikiyor" hissi var mi?

## MEDIUM-4 - Mob "Enrage" (Rage / OFK) tamamen etkisiz: mob hasari artmiyor

`MMOMob.cpp:361-366` (Enrage -> `Stats->ApplyEffect(Rage)`) + `MMOEffectDatabase.cpp:38` (Rage =
`AttackBuff` +20) + `MMOStatsComponent.cpp:191-192` (buff sadece `AttackPower`'a eklenir) +
`MMOMob.cpp:515` (mob vurusu `ApplyDamage(Player, MobAttackDamage, ...)` -> **`AttackPower`'i HIC
kullanmiyor**, `MobAttackDamage` arketipten sabit).

Sonuc: Trol (ana alan, Lv5), Ork Reisi (Q_Boss), Kadim Golem, Kara Buyucu, Lanetli Sovalye, Ork
Berserk, Magma Golemi, Kul Orku, Cehennem Hayaleti, Buz Lordu, Magma Tirani'nin "can azalinca
ofkelenir" ozelligi sadece HUD'da OFK ikonu gosteriyor; vurus hasari degismiyor. Checklist'teki
satir 392, 452, 865, 910'daki "hasari artmali" beklentileri **bu kodla KALACAK**.
Onerilen yon: mob vurusunda `MobAttackDamage + SumEffectMagnitude(AttackBuff)` (veya Stats->AttackPower
ile harmanlama) kullanmak. (Ayni sebeple oyuncunun mob'a attigi AttackBuff-azaltan debuff'lar da
mob'u etkilemez - dogrulanmali.)

- [ ] **Trol'u %40 canin altina indir** -> OFK ikonu cikmali -> ikon ONCESI ve SONRASI sana vurdugu hasar
      yazisini karsilastir. Ayniysa MEDIUM-4 dogrulandi (beklenen: ayni, 20-Def).

## MEDIUM-5 - Sinif secimi: tek ipucu 8 sn'lik konsol mesaji; sonradan degistirmek harcanan stat/skill puanlarini SILIYOR

`MMOCharacter.cpp:361` (ipucu: `AddOnScreenDebugMessage` 8 sn, sadece kayit yoksa) +
`MMOCharacter.cpp:1216-1240` (`MMOClass` Exec) + `MMOSkillComponent.cpp:138-181` (`SetupForClass`:
`InitializePrimaryStats(...)` taban statlari YAZAR, `LearnedSkills`/`MasteryRankArray` sifirlanir;
`UnspentStatPoints`/`UnspentSkillPoints` **iade edilmez**).

Yeni oyuncu varsayilan Savasci olarak baslar; HUD'da (sinif adi disinda) sinif secme arayuzu yok.
8 sn'lik mesaji kaciran ya da ilk 10 dk'da Lv8-10'a gelip (bkz. MEDIUM-3) sonra "Buyucu olayim"
diyen oyuncu: Lv9'da harcanmis **24 stat + 16 skill puani kalici olarak kaybolur** (seviye kalir,
puanlar geri gelmez). Ayrica konsol tusu `Tilde` (`DefaultInput.ini:85-86`) - Turkce Q klavyede
bu tusun konsolu acip acmadigi **dogrulanmali**.
Onerilen yon: ya sinif degisimini Lv1 / hic puan harcanmamis durumla sinirlamak, ya da degisimde
(Level-1)*StatPointsPerLevel ve (Level-1)*SkillPointsPerLevel'i `Unspent`'e iade etmek; ilk giriste
kalici bir sinif secim paneli.

- [ ] **Yeni karakter (kayit sil) -> Play** -> sinif ipucu mesajini okuyabildin mi (8 sn)? Konsol (`~`/`"` tusu) Turkce klavyede aciliyor mu?
- [ ] **Savasci ile Lv3'e cik, 6 stat puanini STR'ye ver, skill puani harca -> konsola `MMOClass mage`.**
      C ile stat penceresi: STR 6'ya dustu mu, "harcanmamis puan" 0 mi? (Beklenen: evet = puanlar kayboldu, bulgu dogrulandi.)

## MEDIUM-6 - `ServerSetupForClass` dogrulamasiz: istemci her an tam can + tam mana + tum cooldown sifirlama alabilir

`MMOSkillComponent.h:139-140` (`Server, Reliable`, WithValidation yok) + `MMOSkillComponent.cpp:93-96,
138-160`: hicbir olum/savas/seviye/sure kontrolu yok. `InitializePrimaryStats` -> `RecalculateDerivedStats(
bRefill=true)` -> `Health=MaxHealth, Mana=MaxMana`; `CooldownEndTimes.Reset()`.
Tetik: savas ortasinda `MMOClass warrior` (ayni sinif bile) -> aninda full HP/MP + skill CD'leri sifir.
Olu iken yapilirsa `bIsDead=true` kalirken `Health` MaxHealth'e dolar (tutarsiz durum). Exec konsol
komutu shipping'de kapali olabilir (**dogrulanmali**) ama Server RPC modlu bir istemciden her
zaman cagrilabilir -> sunucu-otoriterlik ihlali. Onerilen yon: sunucuda "savasta degil / canli /
cooldown" kapisi, ya da sinif degisimini Lv1'e kilitlemek (MEDIUM-5 ile tek duzeltme).

- [ ] **Bir moba dovus, canin %30'a dussun -> konsola `MMOClass warrior` (zaten Savasci iken).** Can ve mana
      aninda DOLUYOR mu, skill cooldown'u sifirlaniyor mu? (Beklenen: evet = exploit dogrulandi.)
- [ ] **Oluyken (respawn geri sayiminda) `MMOClass warrior`** -> HP bari dolu gorunup karakter olu mu kaliyor? Respawn normal mi?

## LOW-7 - Priest'in Lv1 slot-1 skili bir IYILESTIRME; Okcu/Buyucu temel vurusu yakin dovus

`MMOSkillComponent.cpp:164-180` en dusuk RequiredLevel'li Basic skili verir: Priest -> `Pri_HolyLight`
(HealSelf, `MMOSkillDatabase.cpp:193`); ilk hasar skili `Pri_Smite` Lv2 + Basic agacina puan yatirmayi
ister. Lv1 Priest'in "1" tusu hasar vermez; tek hasari AP 16'lik temel vurus (Trol'e karsi TTK 17.9 sn
/ TTD 20.1 sn - iksirsiz ve sifa basmadan cok sinirda). Ayrica `MeleeRange=220` tum siniflar icin ortak:
Okcu ve Buyucu mana bitince yakin dovuse girmek zorunda. Tasarim karari; bug degil.

- [ ] **`MMOClass priest` -> ana alanda bir Trol'e sadece temel vurus + 1 (sifa) ile dovus.** Kazanabiliyor musun? "1"e basmak hasar veriyor mu? (Beklenen: hayir, sadece sifa.)

## LOW-8 - Ana alanin normal moblari hub'in ICINDEN geciyor (zararsiz ama "guvenli kasaba" hissi yok)

Ana spawner (1050,0) r=350 + gezinti 450 -> Kael'in (400,0) uzerine kadar (-150u), Satici/Gorevli'ye
25u'ya kadar gelebiliyor; Kurt aggro 1150 -> PlayerStart'ta 30 sn beklemede %96 aggro. Lv1 icin
tehlikesiz (Savasci Goblin/Kurttan 1 hasar/vurus alir), ama NPC diyalogu sirasinda surekli saldiri
+ hasar yazisi. `MMOGameMode.cpp:49-50`'deki "moblar meydanda/NPC'lerin arasinda degil" yorumu
aggro/gezinti hesaba katildiginda gecerli degil. Ayrica ana alanda sadece 3 mob var -> 2+ oyunculu
MP'de yeni oyuncular ayni Goblin'ler icin yarisir (dogrulanmali).

- [ ] **Kael'e tiklayip diyalog okurken 30 sn bekle** -> Lv1-5 moblar hub'a girip sana vuruyor mu?

## LOW-9 - Baslangic: 0 altin, 0 iksir (bilgi - su an sorun degil)

`MMOInventoryComponent.h:47` `Gold=0`, baslangic esyasi yok. Hesap: ana alan ort. 22 altin/kill, mob
%35 sansla Can Iksiri dusurur (~3 kill/iksir), Iksir 20g (~1 kill), Lyra 100g (~4.5 kill), Lv1 zirh
parcasi 99-171g (4.5-8 kill). Lv1 normallere karsi hicbir sinif iksir gerektirmiyor (bkz. SAGLAM-3),
bu yuzden sifir baslangic su an bir engel DEGIL. **Ama** HIGH-1/HIGH-2'deki bir elit/yuksek-seviye
temasinda oyuncunun cebinde tek iksir yok. Oneri (tasarim): 2-3 baslangic Can Iksiri.

## SAGLAM - denetlenip TEMIZ cikanlar

1. **Tum gorev hedefleri gercekten spawn oluyor, isimler dogru.** Ana havuz (`MaxMobLevel=5`,
   `GetRandomInLevelRange(0,5)`) = Goblin, WolfPup, Orc, Bandit, Troll -> Q_Goblins/Q_Wolves/Q_Orcs/
   Q_Bandits/Q_Trolls hedefleri havuzda. Q_Boss=OrcWarlord, Q_SpiderQueen/StoneGolem/ShadowAssassin/
   FrostLich/MagmaTyrant = `ForcedArchetypeId`'li spawner'larda birebir ayni ID. Q_Warlocks/OrcWarriors/
   DireWolves -> 2. bolge bandi (6-7), Q_FrostWraiths..Q_OrcBerserkers -> 3. bolge (8-12), Q_EmberHounds..
   Q_InfernalWraiths -> 4. bolge (16-22). `NotifyMobKilled` `ArchetypeId` ile esliyor. **Ulasilamayan gorev yok.**
2. **Lv6+ normal moblar ana havuza sizmiyor** (band filtresi `bIsElite` + `Level<=5`).
3. **Lv1 vs Lv1-5 normaller:** hicbir sinif ana alan normallerine iksirsiz kaybetmiyor (en sinirli: Priest-Trol
   17.9/20.1 sn, Buyucu-Trol 7.7/14.2 sn). Ana alanin kendisi Lv1 icin guvenli; tehlike HIGH-1/2'deki disaridan gelenler.
4. **Baslangic skili + hotbar:** 4 sinifin hepsinde en dusuk seviyeli Basic skil ogrenilip `Hotbar[0]`'a konuyor
   (Savasci Guclu Vurus 8 mana ~7 atis, Okcu Cabuk Atis ~11, Buyucu Buz Oku ~14, Priest Kutsal Isik ~8).
5. **Gorevli dogus noktasina 320u** -> istemci 500u tiklama menzilinde, sunucu 800u (`IsOwnerNearQuestGiver`)
   kontrolunden geciyor; Play'e basar basmaz tiklanabilir. Lyra-Gorevli arasi 150u -> tiklama cakismasi yok (kapsul ~42u).
6. **Lv1'de alinabilir gorevler:** Q_Goblins, Q_Cleanup, Q_Level5, Q_Gold500 - on kosul/seviye kapilari (`IsQuestAvailable`,
   `AcceptQuest`) tutarli; Q_Wolves Q_Goblins'siz alinamiyor.


## QA denetimi — canlanma korumasi (commit a33c18d, 2026-09-24)

> Derleme: ilk deneme "Target is up to date" (yalanci basari) verdi; degisen 5 dosyanin tarihi
> guncellenip ZORLA derlendi → UHT (-WarningsAsErrors) + 4 unity derleme + link, **Result: Succeeded**,
> uyari/hata yok. Kod incelemesi: korumayi sonsuza uzatan yol YOK, istemci bayragi sahteleyemiyor
> (yalniz sunucu yaziyor, RPC degil), TakeDamage disinda hasar yolu YOK (tum `ApplyDamage` cagrilari
> ve DoT `TakeDamage`'den geciyor; `Stats->ApplyDamage` sadece `TakeDamage` icinden cagriliyor),
> 4 hasar skili tipinin hepsi korumayi bitiriyor. Asagidaki bulgular PIE'de dogrulanmali.

### BULGU 1 (ORTA — ozelligin asil senaryosunu bozar) — Otomatik saldiri olumden sonra da suruyor, korumayi hemen bitiriyor
`AMMOPlayerController::TargetEnemy` (otomatik saldiri hedefi) oyuncu olunce TEMIZLENMIYOR
(`MMOPlayerController.cpp` 607-627; sadece hedef olurse/WASD/Z/toggle ile sifirlanir). Oyuncu
canlaninca `PlayerTick` o mob'a yeniden yol bulur, menzile girince `MMOAttack()` →
`AttackTarget(nullptr)` → `EndSpawnProtection()` (`MMOCharacter.cpp:709`). Dogus noktasinin
yanindaki mob'a olup orada canlanan oyuncu (bu ozelligin hedefledigi durum) 1 sn icinde kendi
kendine saldirip korumayi kaybeder → olum dongusu devam eder. Ayrica uzak bir mob'a olunduyse
canlaninca haritayi yuruyerek o mob'a geri gider. Yon: olumde (ya da canlanmada) otomatik saldiri
/ tikla-git hedefini temizlemek (mmo-developer karar versin).
- [ ] Play → dogus noktasina yakin bir mob'a sol tik ile saldir (otomatik saldiri acik) → mob seni
      oldursun (gerekirse `MMODamage 9999`, koruma bittikten sonra) → canlan → **HICBIR tusa basma**.
      BEKLENEN (duzeltme sonrasi): karakter yerinde durmali, "KORUMA" ~4 sn kalmali.
      SU ANKI SUPHE: karakter kendiliginden mob'a yurur/vurur, "Canlanma korumasi bitti" hemen cikar.
- [ ] Ayni testi uzak bir mob ile yap: canlaninca karakter kendiliginden o mob'a dogru yurumeye
      basliyor mu? (Evetse bu bulgunun ayni koku.)

### BULGU 2 (KUCUK / dogrulanmali) — Oncesinden kalan DoT korumayi bozmadan mob'u oyuncuya kilitler
Oyuncu olmeden once mob'a zehir (DoT) attiysa, DoT tick'i mob'un `TakeDamage`'ine instigator=oyuncu
ile gelir (`MMOStatsComponent.cpp:669` → `MMOMob.cpp:135-150`) ve `ForcedTarget` korumali oyuncuya
kurulur; korumayi bitirmez, oldururse XP'yi korumaliyken alir. Koruma bitince `ForcedAggroUntil`
(8 sn) hala aciksa mob AggroRange disindan oyuncuya kosar. Istismar degeri dusuk.
- [ ] Okcu ile mob'a zehirli ok at → hemen `MMODamage 9999` ile ol → canlan → koruma bitince mob'un
      aggro menzilinin DISINDAN sana kosup kosmadigini gozle (kosarsa bu bulgu dogrulandi).

### BULGU 3 (KUCUK / dogrulanmali) — Kovalamaya baslamis mob korumali oyuncunun yanina kadar yurumeye devam edebilir
Korumali hedef atlaninca mob `IdleWander()`'a duser; ama `NextWanderTime` dolmadiysa fonksiyon
`StopMovement` cagirmadan doner (`MMOMob.cpp:465-470, 553-557`), onceki `MoveToActor(oyuncu)`
surer. Mob korumali oyuncunun dibinde bekler, koruma biter bitmez vurur. Ulasilabilirligi dusuk
(olen oyuncudan zaten vazgeciliyor), cogunlukla 2 oyunculu senaryoda.
- [ ] 2 pencereli PIE: A oyuncusu bir mob'u cekip kossun, B korumali (yeni canlanmis) halde yakinda
      dursun → mob B'ye yurumemeli / B'nin yaninda "bekleyip" koruma bitince aninda vurmamali.

### BULGU 4 (KUCUK / UX) — Yaniltici geri bildirimler
- PvP'de korumali oyuncuya vuran saldiran "VURDUN: X hasar" gorur ama hasar 0'dir
  (`MMOCharacter.cpp:722-729` gercek degeri degil `AttackPower`'i yaziyor). Diger oyuncular hedefin
  korumali oldugunu gormuyor (isim plakasinda isaret yok).
- HUD etiketi "KORUMA", satilan savunma buffi "Koruma" parsomeniyle ayni ad — karisabilir.
- [ ] 2 pencereli PIE, `MMOPvP 1`: korumali oyuncuya vur → saldiranin ekraninda ne yaziyor? (0 hasar
      olmasina ragmen "VURDUN: N hasar" yaziyorsa bu bulgu dogrulandi.)
- [ ] "Koruma" parsomeni ic → HUD'da canlanma "KORUMA" etiketiyle karistirilacak bir durum var mi?

### Test kolayligi notu
- 4 sn icinde konsolu acip `MMODamage 50` yazmak zor: test icin `BP_ThirdPersonCharacter` → Class
  Defaults → `MMO|Combat` → **Spawn Protection Duration = 15** yap, testten sonra 4'e geri al.

---

# Ilk-10-dk QA duzeltmeleri: sinif degistirme kapisi + puan iadesi + mob Enrage hasari (2026-09-24)

> Derleme temiz. Kapsanan bulgular: MEDIUM-4, MEDIUM-5, MEDIUM-6 (yukaridaki "ilk 10 dakika" denetimi).
> `MMOClass` artik sunucuda `UMMOSkillComponent::ChangeClass`'a gidiyor: ayni sinif = no-op ("Zaten bu
> siniftasin"), son hasardan 5 sn (`CombatDuration`) gecmeden ya da oluyken reddedilir. Gercek degisimde
> harcanan stat/skill puanlari iade edilir (stat: `(Level-1)*3`, skill: harcanan rutbeler + harcanmamis,
> en az `(Level-1)*2`), ana statlar yeni sinifin baslangic dagilimina, rutbeler/ogrenilenler/hotbar
> sifirlanir, yeni sinifin baslangic skili slot 1'e gelir. Can/mana DOLMAZ (yeni maksimuma kirpilir),
> cooldown'lar SIFIRLANMAZ. Mob vurusu artik `MobAttackDamage + AttackBuff toplami` (Rage +20, Rage II +55).
> Not: "savas" sadece HASAR ALMAYA bakar (moba vurup hasar almadiysan savasta sayilmazsin).
> Not: red mesajlari sunucuda basilir - Listen Server istemcisi (2. pencere) "savasta" reddini GORMEZ
> (sadece "Zaten bu siniftasin" istemcide de cikar).

- [ ] **MEDIUM-6 exploit kapali:** Bir moba dovus, canin ~%30'a dussun → hemen konsola `MMOClass mage` → "Savasta sinif degistirilemez" yazmali, sinif/can/mana DEGISMEMELI.
- [ ] **Ayni sinif no-op:** Savasci iken (savas disi) `MMOClass warrior` → "Zaten bu siniftasin", hicbir sey degismemeli (hotbar, puanlar, can ayni).
- [ ] **Savas disi degisim can/mana doldurmuyor:** Hasar al (`MMODamage 50`, koruma bittikten sonra), 5+ sn bekle (regen azicik doldurabilir), `MMOClass mage` → can/mana ANINDA fullenmemeli; sadece yeni maksimuma gore kirpilmali (Savasci→Buyucu'da MaxHP duser).
- [ ] **Cooldown korunuyor:** Savasci Lv1 `1` (Guclu Vurus) kullan → hemen 5 sn savas-disi olunca degil, CD bitmeden `MMOClass archer` → sonra `MMOClass warrior` (yine 5 sn savas disi) → Guclu Vurus CD'si kaldigi yerden devam etmeli (sifirlanmamali). (Kisa CD'li skilde zor; yapamazsan atla.)
- [ ] **Oluyken reddediliyor:** Ol (`MMODamage 9999`) → geri sayimda `MMOClass mage` → HICBIR sey olmamali (HP bari dolmamali), canlaninca sinif hala Savasci.
- [ ] **MEDIUM-5 puan iadesi:** Savasci ile Lv3'e cik (`MMOGainXP 300`), 6 stat puanini STR'ye ver, skill puanlarini Basic agacina yatir → 5 sn savas disi → `MMOClass mage` → C (stat penceresi): statlar Buyucu tabani (STR 6 / DEX 8 / INT 22 / VIT 10), **harcanmamis stat puani 6**; K (skill penceresi): **skill puani 4**, rutbeler 0, hotbar'da sadece Buyucu baslangic skili (Buz Oku) slot 1'de.
- [ ] **Iade tekrarlanamaz (puan sismesi yok):** Yukaridakinin ardindan hic puan harcamadan `MMOClass warrior` → stat puani yine 6, skill puani yine 4 (artmamali).
- [ ] **Kayit:** Sinif degistir → `MMOSave` → PIE'yi kapat/ac → sinif, statlar, harcanmamis puanlar ve bos rutbeler korunmali.
- [ ] **Regresyon - ilk giris:** Yeni karakter (kayit sil) → Play → Savasci tabani + Guclu Vurus slot 1'de, can/mana dolu (PossessedBy kurulumu degismedi).
- [ ] **MEDIUM-4 Enrage hasari:** Trol'u (ana alan) %40 canin altina indir → OFK ikonu cikmali → ikon ONCESI ve SONRASI sana vurdugu hasar yazisini karsilastir: sonrasi ~20 fazla olmali (Def dusulmeden once +20). Ork Reisi / Kadim Golem ile de dene.
- [ ] **Denge notu (karar senin):** Rage duz +20 - zayif mobda (Trol) hasari ~2-3 katina, elitte (Magma Tirani 95) ~%20 artiriyor. Oransal istenirse `MMOMob.cpp` vurus satiri tek yerde.
- 2 pencereli PIE'de `MMODamage` sadece HOST penceresinde calisir (istemcide otorite yok, hicbir sey
  olmaz) — koruma testini host penceresinde yap.

### Saglam bulunanlar (kod incelemesi)
- Sonsuz koruma yolu yok: baslatma sadece `HandleRevived` (olum→`Revive`; cagiranlar `bIsDead`
  kontrol ediyor) ve `PossessedBy` (C++'ta yeniden possess yolu yok); `LoadCharacter/ApplySaveData`
  `OnRevived` yaymiyor; `SetTimer` ust uste binmiyor, sifirliyor; korumaliyken olunemedigi icin dongu yok.
- Istemci otoritesi: `bSpawnProtected` sadece `HasAuthority` korumali Start/End'de yaziliyor; mob ve
  `TakeDamage` sunucu kopyasini okuyor. Istemci `AttackTarget` RPC'ye dallanmadan once korumaya
  dokunmuyor, sunucu tarafi bitiriyor.
- Hasar skili sinifi eksiksiz (enum'da 4 hasar + 3 sifa/buff tipi). `ExecuteSkill` korumayi hedef
  bulunmadan once bitiriyor → hata yonu guvenli (hedefsiz AoE bile korumayi bitirir).
- Mob: leash/eve donus etkilenmiyor, sonsuz dongu yok; vurus-debuff'i saldiri dalinda oldugu icin
  korumali oyuncuya uygulanmiyor. HUD etiketi (y 50-72) ust-orta hedef barla (y 8-46) cakismiyor.

---

# QA denetimi: fa1630f (sinif degistirme kapisi + puan iadesi + Enrage hasari) (2026-09-24)

> Derleme: zorla yeniden derlendi (degisen 3 .cpp + 2 .h), **0 uyari / 0 hata, Result: Succeeded**.
> Puan iadesi formulu HandleLevelUp ile tutarli (python ile Lv1/Lv9/Lv40 dogrulandi: iade sonrasi
> stat = (Lv-1)*3, skill = (Lv-1)*2; degistir-geri dongusu puan SISIRMIYOR; eski (rutbesiz) kayitta da
> taban dogru). Asagidaki adimlar bu denetimde bulunan ACIK noktalari dogrulamak icin.

- [ ] **[BUG ADAYI - MEDIUM] Sinif degistir + VIT'e puan bas = aninda can doldurma dongusu:**
      Savasci ile `MMOGainXP 600` (Lv5, 12 stat puani) → 12 kez `MMOStat vit` (MaxHP ~334) →
      koruma bittikten sonra `MMODamage 250` (HP ~100'e dussun) → 5 sn bekle → `MMOClass archer`
      (HP ~100 kalmali, dogru) → 12 kez `MMOStat vit` → **HP'ye bak.** Hata varsa HP aninda ~+96
      artar (~196). Sonra 5 sn bekle → `MMOClass warrior` → 12 kez `MMOStat vit` → HP yine ~+96.
      Beklenen (duzeltilmis): puan basmak sadece MAKSIMUMU artirmali, anlik cani DOLDURMAMALI
      (ya da degisim bir sure kilitlenmeli). Regen (6/sn) ile ayni iyilesme ~30+ sn surerdi.
- [ ] **[BUG ADAYI - MEDIUM] Ayni dongu mana icin:** Buyucu ile manayi bosalt (skill spamla),
      5 sn hasar alma → `MMOClass priest` → `MMOStat int` x N → mana aninda +5/puan artiyor mu?
- [ ] **[Bilinen tasarim acigi] Hasar ALMADAN savasta sinif degistirme:** Okcu ile uzak bir moba
      ok at (mob sana ulasamadan / ya da vurup menzilden kac), mob hala seni kovalarken ve
      5 sn hasar almadiysan `MMOClass mage` → degisim KABUL EDILIYOR mu? (Kod bunu kabul eder;
      "savas" sadece son alinan hasara bakiyor.) Kabul edilebilir mi karar ver.
- [ ] **Yeni karakter sinif secimi - can/mana dolu degil (regresyon adayi, LOW):** Kaydi sil →
      Play → koruma bitince `MMOClass mage` → mana **60/140** gibi yarim gorunur (eskiden fullenirdi).
      `MMOClass warrior` (5 sn sonra) → HP **142/190**. Regen birkac sn'de doldurur; Lv1'de hic
      XP yokken ilk secimde doldurma istenir mi karar ver.
- [ ] **Buff'lar sinif degisiminde kaliyor:** Savasci iken Ofke (War_BattleShout) bas → OFK ikonu
      varken 5 sn bekle → `MMOClass mage` → OFK ikonu ve AttackPower bonusu HALA duruyor mu?
      (Kod temizlemiyor; kasitli mi karar ver.)
- [ ] **Kayit - MMOSave OLMADAN:** Sinif degistir → hic `MMOSave` yazmadan PIE'yi Stop ile kapat →
      tekrar Play → yeni sinif, sifir rutbeler, iade edilmis puanlar korunmali (EndPlay kaydi).
- [ ] **Kayit - eski/rutbesiz kayit:** (Varsa) mastery sisteminden ONCE alinmis bir kayitla gir →
      K penceresinde rutbeler 0 ama skiller ogrenilmis gorunur → 5 sn savas disi `MMOClass archer`
      → skill puani tam **(Level-1)*2**, stat puani tam **(Level-1)*3** olmali.
- [ ] **Lv1'de degisim puan vermiyor:** Yeni karakter (Lv1) → `MMOClass archer` → C: stat puani 0,
      K: skill puani 0 (iade tabani Lv1'de 0 olmali).
- [ ] **Priest adi tutarsizligi (kozmetik):** `MMOClass priest` → ekran mesaji "Sinif: **Rahip**",
      HUD/skill penceresinde "**Priest**" yaziyor. Ayrica `MMOClass rahip` CALISMIYOR (diger
      siniflarin Turkce takma adi var: savasci/okcu/buyucu).
- [ ] **2 pencereli PIE (Listen Server) - istemci:** Istemci penceresinde Savasci→Buyucu (dusuk VIT)
      degisimi yapinca istemci ekraninda kirmizi bir "hasar sayisi" ucuyor mu? (OnRep_Health can
      dususunu hasar sanar; sadece gorsel.) Red mesajlari istemcide cikmiyor (bilinen, yukarida not).
- [ ] **Enrage Lv1 riski yok (dogrulama):** Lv1 Buyucu (HP 142, Def 5) Magma Tirani/Cehennem
      Hayaleti ofkeliyken vurus ~110/~97 → **tek vurusta OLMEMELI** (2 vurus). Zaten bu moblarin
      bolgesinde Lv1 ile olmak beklenen; tek vurus olursa baska bir carpan var demektir, bildir.

### Saglam bulunanlar (fa1630f kod incelemesi)
- Puan iadesi: tek kaynak `HandleLevelUp` (3/2 puan/seviye), baska stat/skill puani kaynagi veya
  rutbe yazan baska yol yok → `max(unspent, (Lv-1)*3)` ve `max(unspent+rutbe, (Lv-1)*2)` cift sayim
  yapmiyor; dongu ile sisme yok (python: Lv1 0/0, Lv9 24/16, Lv40 117/78).
- Sunucu otoritesi: `ChangeClass` ve `ResetPointsForClassChange` `HasAuthority` korumali; istemci
  RPC'ye sadece enum yollayabiliyor, puan/stat degeri yollayamiyor. Ayni-sinif / olu / savasta
  kapilari sunucuda.
- Can kirpma: `Clamp(OldHealth, 1, MaxHealth)` (MaxHealth >= 1 garanti) → olu olmayan oyuncu 0'a ya
  da NaN'a dusmez; mana `Clamp(0, MaxMana)`. Olu oyuncu kapidan once elendigi icin 1'e "diriltme" yok.
- Mob Rage: su an sadece pozitif AttackBuff'lar var (Rage +20, RageII +55; mob sadece Rage alir),
  `Max(1, ...)` gelecekteki negatif debuff'a karsi guvenli; Enrage bir kez tetiklenir, yigilmaz.
- Ilk giris (`PossessedBy → SetupForClass`) davranisi ayni; ortak yardimcilar (`GetClassBaseStats`,
  `GrantStarterSkill`) statlari/baslangic skilini birebir koruyor. Blueprint'lerde
  `SetupForClass/RequestSetupForClass` referansi yok (Content taramasi).

---

# Sinif secimi ipucu paneli + Turkce sinif adlari + Rahip Lv1 hasar skili (2026-09-24)

> Derleme temiz. Kapsanan QA "ilk 10 dk" bulgulari: sinif secimi sadece 8 sn'lik mesajdaydi,
> Rahip Lv1'de hasar skilsizdi, `MMOClass rahip` calismiyordu / HUD'da "Priest" yaziyordu.

- [ ] **Ipucu paneli (yeni karakter):** Kaydi sil → Play → ust-ortada (hedef barinin altinda)
      "Sinifini sec: konsolda MMOClass savasci|okcu|buyucu|rahip (varsayilan: Savasci)" paneli
      cikmali ve 8 sn sonra KAYBOLMAMALI. Hedef bari/minimap/pencerelerle cakisiyor mu bak.
- [ ] **Panel sinif secince kapaniyor:** koruma bitince `MMOClass rahip` → panel hemen kaybolmali,
      ekran mesaji "Sinif: Rahip", HUD'da "Lv 1  Rahip", K penceresinde "Rahip".
- [ ] **Varsayilani kabul:** Yeni karakterde `MMOClass savasci` → "Zaten bu siniftasin" + panel
      kaybolmali (komut bir kez kullanildi).
- [ ] **Gecersiz ad:** `MMOClass xyz` → "Gecersiz sinif. Kullanim: ..." mesaji, panel KALMALI.
- [ ] **Kayitli karakter:** Kayitli bir karakterle gir → panel HIC cikmamali.
- [ ] **Eski adlar:** `MMOClass warrior|archer|mage|priest` hala calisiyor mu (60 sn bekleme arasi).
- [ ] **Rahip Lv1 skilleri:** Yeni karakter → `MMOClass rahip` → hotbar slot 1 = Kutsal Isik,
      slot 2 = Ceza; ikisi de Lv1'de kullanilabilir, Ceza bir moba hasar vuruyor mu.
      K penceresinde Ceza "Lv1 Rutbe1" gorunmeli.
- [ ] **Diger siniflar degismedi:** `MMOClass savasci/okcu/buyucu` → hotbar'da yine TEK baslangic
      skili (Guclu Vurus / Cabuk Atis / Buz Oku).
- [ ] **2 pencereli PIE (istemci):** Istemci penceresinde de panel cikmali (bayrak sadece sahibine
      replike), istemci `MMOClass okcu` yazinca panel kapanmali. Host'ta istemcinin paneli gorunmez.
- [ ] **Tasarim notu (karar ver):** Rahip'in Basic agaci artik Lv1'de tamamen bedava geliyor —
      Basic'e rutbe yatirmak Rahip'e yeni skil acmiyor (Buyucu'da da zaten boyle). Eski (bu
      degisiklikten once kaydedilmis) Lv1 Rahip kaydinda Ceza ogrenilmis gelmez; Basic'e 1 rutbe
      yatirinca acilir.

---

# QA denetimi: b5d4567 (sinif ipucu paneli + Turkce sinif adlari + Rahip Lv1 Ceza) (2026-09-24)

> Derleme: editor kapali, degisen 4 .cpp + 2 .h zorla yeniden derlendi (4 unity dosyasi + link),
> **0 uyari / 0 hata, Result: Succeeded**. Kritik (coker/yanlis otorite) bulgu YOK. Asagidakiler
> kod okumasinda cikan ACIK noktalar; yukaridaki bolumdeki adimlari tekrarlamaz.

- [ ] **[BUG ADAYI - LOW/MEDIUM] Reddedilen sinif degisimi paneli yine de kapatiyor:** Kaydi sil →
      Play → koruma bitince bir moba vur ve hasar al (savasta) → hemen `MMOClass buyucu` →
      "Savasta sinif degistirilemez" mesaji cikar, sinif Savasci kalir → **panel KAYBOLDU mu?**
      Kod `bClassHintDismissed`'i sunucu cevabini beklemeden yerelde true yapiyor
      (MMOCharacter.cpp:1253), bu yuzden panel kaybolur ve bir daha gelmez. Ayni durum olu iken
      (`MMODamage 9999` → oluyken `MMOClass okcu`) icin de dene. Beklenen: sinif gercekten
      degismediyse panel kalmali (ya da en azindan bu davranis kabul edilmeli).
- [ ] **Panel acik pencerelerin USTUNE ciziliyor (HUD cakismasi):** Yeni karakterde panel
      gorunurken (a) bir saticiya E ile konus → SATICI penceresi (ust-orta, y=90) acikken panel
      "SATICI" basligini/Altin satirini ve sag ustteki X kapat dugmesinin yakinini ORTUYOR mu?
      (b) Gorev penceresini ac (ust-orta-sol, y=90) → "GOREVLER" basligi ortuluyor mu?
      (c) PIE penceresini kucuk boyutta (~1280 genislik) oynat → K (skill kitabi, sol, y=90) ve
      envanter (sag, y=90) basliklariyla da cakisiyor mu? Panel y=84..~116 arasi ve pencerelerden
      SONRA ciziliyor. Tiklamalar etkilenmez (sadece gorsel) ama baslik okunmuyorsa bildir.
- [ ] **Hedef bari ile cakisma yok (dogrulama):** Yeni karakterde bir mob sec (tikla) → hedef bari
      (y ~8-44) ile panel (y 84+) arasinda bosluk olmali, ust uste binmemeli.
- [ ] **Eski kayitli Rahip (bu commit ONCESI kayit varsa):** Lv1 Rahip kaydiyla gir → hotbar'da
      sadece Kutsal Isik var, Ceza YOK (ApplySaveData GrantStarterSkill cagirmiyor). K'da Ceza'ya
      tikla → Lv1'de skill puani 0 oldugu icin "1 skill puani gerekli" → Ceza ancak Lv2'de puan
      harcayarak gelir; yeni Rahip ise bedava aliyor. Ayrica Lv2+ eski Rahip'in Basic'e yatirdigi
      1 rutbe artik "bosa" (iade yok, sinif degisimi disinda). Kabul mu, migrasyon mu karar ver.
- [ ] **Turkce karakterli ad:** `MMOClass savaşçı`, `MMOClass okçu`, `MMOClass büyücü` →
      "Gecersiz sinif" mesaji mi geliyor? (Kod sadece ASCII takma adlari taniyor.) Oyuncu panelde
      ASCII yazili hali goruyor, ama Turkce klavyede refleksle ş/ç/ü yazilabilir — bildir.
- [ ] **Rahip degistir-geri-degistir (Lv1):** Yeni karakter → `MMOClass rahip` → hotbar 1=Kutsal
      Isik, 2=Ceza → 60 sn bekle → `MMOClass savasci` → hotbar'da SADECE slot 1=Guclu Vurus, slot 2
      BOS olmali (Ceza kalmamali) → 60 sn bekle → `MMOClass rahip` → yine 1=Kutsal Isik, 2=Ceza;
      K penceresinde her skil TEK kez gorunmeli, skill puani 0 kalmali.
- [ ] **Kaydet-yukle Rahip hotbar'i:** Rahip sec → hotbar'da Ceza'yi slot 5'e tasi → `MMOSave` →
      Stop → Play → Ceza slot 5'te olmali (PossessedBy'daki starter yerlestirmesi kaydi EZMEMELI),
      slot 2 kayittaki hali neyse o.
- [ ] **Onceki bolumdeki "Priest adi tutarsizligi" maddesi (fa1630f) bu commit ile kapanmis
      olmali:** `MMOClass priest` → ekran mesaji, HUD sol ust ("Lv 1  Rahip"), K basligi, parti
      paneli ve C/karakter penceresi basligi HEPSI "Rahip" demeli; `MMOClass rahip` calismali.

### Saglam bulunanlar (b5d4567 kod incelemesi)
- Enum `DisplayName "Priest"→"Rahip"`: sadece editor gosterimi; SaveGame/replikasyon/Blueprint
  pinleri enum ADINI (`EMMOClass::Priest`) ve deger byte'ini kullanir → eski kayit uyumu bozulmaz.
  Content'te `EMMOClass`/`MMOSkillComponent` referansi yok; kodda `GetDisplayValueAsText` vb.
  enum DisplayName okuyan yer yok. Tum sinif adlari tek `GetMMOClassDisplayName`'den (5 kullanim).
- `bClassChoicePending`: sadece otoritede yaziliyor (LoadCharacter `HasAuthority` korumali,
  ChangeClass `HasAuthorityInternal` korumali), `COND_OwnerOnly` ile sadece sahibine gidiyor,
  `FillSaveData`'ya girmiyor (kayda yazilmiyor). HUD her kare okudugu icin OnRep gerekmiyor.
  Istemci bayragi sunucuda degistiremez; yerel `bClassHintDismissed` sadece kozmetik.
- `GrantStarterSkill`: Savasci (Guclu Vurus Lv1 / Kalkan Vurusu Lv2), Okcu (Cabuk Atis Lv1 /
  Guclu Atis Lv2), Buyucu (tek Basic: Buz Oku Lv1) → hepsi yine TEK starter, slot 0; davranis
  degismedi. Rahip: Kutsal Isik + Ceza (ikisi Lv1). `AddUnique` + cagri oncesi `LearnedSkills.Reset`
  / `Hotbar.Init` → cift ogrenme / hotbar cakismasi yok (SetupForClass ve ChangeClass'ta).
- `RequiredMasteryRank <= RequiredLevel`: tum 61 skil tarandi, ihlal yok (Ceza 1<=1).
- Yeni Rahip Basic'e yanlislikla puan yatiramaz: tek yatirim yolu HUD'da ogrenilMEMIS skile
  tiklamak (`LearnSkill→CanLearn`), iki Basic skil de ogrenilmis → "Zaten ogrenildi"; bosa puan yok.
- `ServerSetupForClass` gecersiz enum reddi ve 60 sn/savas/olum kapilari degismedi;
  `LastClassChangeTime=-1000` → ilk secim bekleme mesajina takilmiyor.

---

# 2026-09-28 entegrasyon duzeltmeleri (PIE'de henuz oynanmadi)

- [ ] Yeni karakterde `MMOClass buyucu` istegini savasta reddettir: sinif secim ipucu ekranda kalmali. Savas disinda gecerli secim yapinca ipucu kapanmali. Varsayilan `MMOClass savasci` secimi de ipucunu kapatmali.
- [ ] Sinif secim ipucu gorunurken Gorev, Satici, Skill, Envanter ve Stat pencerelerini ayri ayri ac: acik pencerenin basligi ortulmemeli; pencereyi kapatinca ipucu geri gelmeli.
- [ ] `MMOClass savaşçı`, `MMOClass okçu`, `MMOClass büyücü` ve `MMOClass rahip` yazimlarini dene; Turkce karakterli adlar kabul edilmeli.
- [ ] Eski Lv1 Rahip kaydini yukle: Ceza ogrenilmis olmali; 2. hotbar yeri bossa oraya gelmeli, doluysa oyuncunun atamasi korunmali.
- [ ] Sinif degistirip kaydet, cik ve 60 saniye dolmadan ayni slota baglan: ikinci sinif degisimi reddedilmeli. Bekleme bitince kabul edilmeli.
- [ ] Uzun cooldown'lu bir skill kullanip hemen kaydet/cik/gir: skill sunucuda beklemeli, istemci HUD'unda kalan sure gorunmeli; sure bitince tekrar kullanilabilmeli.
- [ ] Okcu ile uzaktan moba vurup hasar almadan `MMOClass buyucu` dene: saldiri sonrasi 5 saniye savasta sayilmali. Hasar skiliyle ayni kontrolu tekrarla.
- [ ] Mob zehrini hub disinda alip 700 birimlik guvenli alana gir: alandaki periyodik mob hasari durmali. Guvenli alan disinda zehir hasari normal surmeli; oyuncunun moba attigi zehir etkilenmemeli.
- [ ] Ekipman cantasini 12/12 doldurup ekipman odullu gorevi teslim et: XP/altin/ekipman verilmemeli, gorev teslim edilebilir kalmali ve neden oyuncuya gorunmeli. Bir yer bosaltip yeniden teslim edince odul tek kez verilmeli.
- [ ] Bos bir seviye bandi (ornegin 25..30) ayarlanmis test spawner'i yarat: Goblin gibi baska seviyeden mob dogmamali, logda band hatasi bir kez gorunmeli. Mevcut dolu band spawner'lari normal calismali.

## Seviye 25–30 Obsidyen veri kademesi (2026-09-28)

Bu kademe derlendi. Summer_scene uzerinde read-only headless zemin/navmesh probundan sonra
normal ve elit spawner'lari etkinlestirildi; PIE oynanis dogrulamasi henuz yapilmadi.

- [ ] Gercek oyun haritasi `Summer_scene` uzerinde yeni normal bolgenin 225 ve elit bolgenin 150 birim yaricapindaki arazi temasini, engelleri ve navmesh baglantisini PIE'de kontrol et.
- [ ] ObsidianStalker Lv25, ObsidianSentinel Lv27, ObsidianRaider Lv29 ve ObsidianWarlord Lv30'un dogdugunu dogrula; hub'a saldirmamalilar.
- [ ] Q_MagmaTyrant sonrasi dort Obsidyen gorevinin sirayla acildigini, hedef ID'lerinin kill credit aldigini ve ekipman odullerinin canta doluysa kaybolmadigini kontrol et.
- [ ] Obsidyen setinin dort parcasini Lv25'te giy; elitin iki bagimsiz %50 loot zarini ve +3..+5 sinirlarini PIE'de gozle.
- [ ] Normal spawner (-2800,-1800), yaricap 225: 150/225u halkalarindaki 64 probda zemin ve bagli navmesh bulundu. Rastgele mob dogumlarini ve bolgeden PlayerStart'a gercek yuru-yolunu PIE'de dogrula.
- [ ] Elit spawner (-4000,0), yaricap 150: 150/200u halkalarindaki 64 probda zemin ve bagli navmesh bulundu. Normal bolgeye 2163u mesafe ve tam nav yolu var; elitin normal alana/hub'a cekilmedigini PIE'de dogrula.

## Stat puani ile anlik can/mana doldurma acigi (2026-09-28)

`SpendStatPoint` artik VIT/INT yatirimindan onceki mevcut can ve manayi koruyup yeni maksimuma kirpiyor. Sinif degisiminde iade edilen puanlarla eski delta hesabini tekrarlayarak can/mana doldurma yolu kapatildi. UE 5.7 derlemesi basarili; PIE oynanis dogrulamasi bekliyor.

- [ ] Hasar almis Lv40 karakterde sinif degistirip iade edilen puanlari VIT'e bas: maksimum can artsin, mevcut can ayni kalsin.
- [ ] Mana harcadiktan sonra INT'e puan yatir: maksimum mana artsin, mevcut mana ayni kalsin.
- [ ] Yeni maksimum mevcut can/manadan kucukse degerler yalnizca yeni maksimuma kirpilsin; negatif deger olmasin.

## XP ve envanter sinir degeri sertlestirmesi (2026-09-28)

Buyuk bir XP odulu artik `int64` ara bakiyede seviye esiklerine dagitiliyor. Esya yiginlari
ve toplam sayi `MAX_int32`'de doyuma ulasiyor; kayittan gelen negatif altin, gecersiz
yigin sayisi ve +0..+8 disi ekipman arti seviyesi temizleniyor. Negatif arti seviyeli
ekipmanin yukseltilmesi sunucuda ayrica reddediliyor. Degisen C++ dosyalari UE 5.7'de
yeniden derlendi; PIE ve eski-kayit davranisi henuz oynanarak dogrulanmadi.

- [ ] Buyuk XP odulunde (ornegin MAX_int32'ye yakin) XP negatif olmasin; seviye 80'de dursun.
- [ ] Bir esya yigini `MAX_int32` sinirina yaklastiginda ekleme onu negatif yapmasin; toplam esya sayisi da negatif olmasin.
- [ ] Negatif altin veya arti seviyesi iceren eski/bozuk kaydi yukle: altin en az 0, ekipman arti seviyesi +0..+8 olmali; yukselterek bedava altin kazanilamamali.

## Kayitli ustalik rutbesi ve puan bakiyesi siniri (2026-09-28)

Kayittan gelen rutbeler 0..kayitli seviye araligina, harcanmamis stat/skill puanlari
ise level-up ile kazanilabilecek toplam araliga kirpiliyor. Ustalik yatirimi artik
`Rank+1` yerine `Rank>=Level` kontrolu kullaniyor; bozuk bir INT32_MAX rutbe toplamada
tasamaz. Degisen dosyalar UE 5.7'de yeniden derlendi; PIE/eski-kayit dogrulamasi bekliyor.

- [ ] Normal Lv1 ve yuksek seviye kayitlarini yukle: mevcut rutbeler ve puanlar degismemeli.
- [ ] Bozuk kayitta negatif veya INT32_MAX rutbe/puan yukle: rutbe seviyeyi, puanlar kazanilan toplami gecmemeli; sinif degisimi ve ustalik yatirimi tasma uretmemeli.

## Obsidyen spawner zemin/navmesh dogrulamasi (2026-09-28)

Yeni iki spawner icin opt-in guvenli konum modu acildi. Sunucu en fazla 8 rastgele
noktada gorunurluk izi ile duz zemin, beklenen zemin yuksekligi ve yakindaki
navmesh katmanini arar; uygun nokta yoksa o tur mob dogurmaz ve ilk hatada log yazar.
Eski spawner'larin varsayilan davranisi degismedi. Normal ve elit merkezlerinde
headless olculen zemin Z sirasiyla -3.52 ve -1.56 oldugundan, bu iki spawner'in
tabani Z=50 ayarlandi. C++ derlemesi basarili; PIE oynanis dogrulamasi bekliyor.

- [ ] Summer_scene PIE'de Obsidyen normal ve elit moblari doguyor, zemin uzerinde yuruyor ve bosluga dusmuyor olmali.
- [ ] Spawn noktasi gecersiz olursa 8 denemeden sonra mob olusmamali; log uyarmali, oyun kilitlenmemeli.
- [ ] Lv1-24 eski spawner'larin dogum ve respawn davranisi degismemeli.

## Kayittan donen cooldown sureleri (2026-09-28)

Sinif degisimi ve beceri bekleme sureleri kayittaki UTC bitisinden hesaplaniyor,
ama tanimli cooldown + 1 saniyeyi gecemiyor. Bilinmeyen skill ID'leri atiliyor;
bozuk cok uzak gelecek zaman damgasi beceriyi/sinifi kalici kilitlemiyor.
UE 5.7 derlemesi basarili, PIE ve eski-kayit oynanisi bekliyor.

- [ ] Normal kayit-yukle akisinda bekleme suresi kalan saniyeyi korumali.
- [ ] Kayitta cok uzak gelecek skill/sinif zaman damgasi varsa yuklenen sure en fazla tanimli cooldown + 1 saniye olmali.
- [ ] Bilinmeyen skill ID'li cooldown kaydi oyuncuya veya UI'a tasinmamali.

## Negatif regen ayari korumasi (2026-09-28)

Can/mana yenilenmesi ve savas carpani olumsuz Blueprint degerlerinde negatif
can/mana uretmeyecek sekilde 0 alt sinirina kirpildi. Bu degisiklikten sonra
UE 5.7 derlemesi basarili; PIE davranisi bekliyor.

- [ ] Varsayilan pozitif yenilenmede can/mana ve savas carpani onceki davranisi surdurmeli.
- [ ] Gecici olarak negatif HealthRegenPerSec, ManaRegenPerSec veya CombatRegenMult ayarlandiginda can/mana sifirin altina inmemeli.

## Satıcı satın alma sayfalaması — 2026-09-28

- [ ] Satıcıda 33+ ürün varken satın alma sütununda 14 okunabilir satır ve önceki/sonraki sayfa düğmeleri görünüyor; hiçbir satır panel dışına taşmıyor.
- [ ] İlk, orta ve son sayfada tıklanan iksir/tüketilebilir/ekipman doğru ürün kimliği ve tekli adetle satın alınıyor; altın ve çanta sonucu doğrulanıyor.
- [ ] Sayfa değiştirirken eski sayfanın satırına hızlı tıklama yanlış ürünü almıyor; satıcıyı kapatıp yeniden açınca sayfa 1 açılıyor.
- [ ] Satma sütununun mevcut davranışı korunuyor. Dar ekranlarda uzun ürün adlarının görünümü ayrıca inceleniyor.

Kod derlemesi başarılı (`MMOHUD.cpp` yeniden derlendi); PIE henüz yapılmadı.

## Alacakaranlık bölgesi Lv31–40 — 2026-09-28

- [ ] Obsidyen Reisi görevi tamamlanınca beş yeni öldürme görevi sırayla açılıyor; hedef ID'leri ve öldürme kredisi tutuyor.
- [ ] Summer_scene'de (1000,3200) çevresinde Lv31/33/36/39 normal moblar, (-3200,1800) çevresinde Lv40 elit zeminde ve navmesh üzerinde doğuyor; oyuncu her iki alana ulaşabiliyor.
- [ ] Yeni normal alanın Obsidyen Reisi alanından yaklaşık 5936u uzaklığı oyun içinde korunuyor; eski (-4100,-1400) konumunda Lv31–39 mob doğmuyor.
- [ ] Lv33 Alacakaranlık setinin dört parçası gereken seviyede kuşanılıyor; eski Lv29/30 düşüş havuzuna girmiyor ve Lv40 elitin ganimet havuzunda bulunuyor.
- [ ] Savaş, zehir/yavaşlatma/öfke, görev ödülleri, altın ve XP akışı solo ile iki oyunculu PIE'de inceleniyor. Sayısal denge henüz onaylanmadı.

Harita için headless zemin/navmesh probu ve UE 5.7 C++ derlemesi başarılı. PIE henüz yapılmadı.

## Satıcı satma sayfalaması — 2026-09-28

- [ ] Ekipman çantası 12'den fazla öğe içerdiğinde satma listesi 12 satırlık sayfalara ayrılıyor; önceki/sonraki düğmeleri çalışıyor.
- [ ] İkinci sayfadaki bir öğe tıklandığında doğru mutlak çanta indeksi satılıyor; altın ve kalan öğeler doğru güncelleniyor.
- [ ] Satış sonrası sayfa sınırı değişirse gösterge geçerli sayfaya dönüyor; satıcı yeniden açılınca ilk sayfa açılıyor.
- [ ] Satın alma sayfalaması ve düğmelerin 80px genişliğindeki etiketleri korunuyor; küçük ekranda taşma kontrol ediliyor.

UE 5.7 C++ derlemesi başarılı; PIE henüz yapılmadı.

## Ölümden sonra eski hareket hedefini temizleme — 2026-09-28

- [ ] Bir mob'a otomatik saldırırken öl ve canlan: karakter eski mob'a kendiliğinden yürümüyor; canlanma koruması saldırıyla istemsiz bitmiyor.
- [ ] Zeminde uzak bir noktaya tıkla, yolda öl ve canlan: eski tıklama hedefi karakteri tekrar tehlikeli bölgeye götürmüyor.
- [ ] Canlandıktan sonra yeni WASD ve yeni zemin tıklaması normal çalışıyor; ölüm sırasında açık UI penceresi çizilmeye devam ediyor.
- [ ] Listen server ile istemci penceresinde aynı davranış gözleniyor.

UE 5.7 `MMOPlayerController.cpp` yeniden derlendi; PIE henüz yapılmadı.

## Kayıttan güvenli konum yükleme — 2026-09-28

- [ ] Summer_scene'de güvenli navmesh üstünde kaydet, oyundan çık ve yeniden gir: aynı bölgede, zeminin üstünde doğ; sınıf/stat/envanter/görev verileri korunuyor.
- [ ] Eski `MapName` alanı olmayan bir kayıt aynı haritada yükleniyor ve geçerli konumu koruyor.
- [ ] Harita dışı/boşlukta veya navmesh dışındaki kayıt konumuyla açılışta karakter PlayerStart'ta kalıyor; düşme veya sıkışma yaşanmıyor.
- [ ] Farklı harita adı taşıyan kayıt, geçerli görünen XY bile olsa mevcut haritada PlayerStart'a dönüyor.
- [ ] Listen server ve istemci için yükleme sonrası konum replikasyonu doğru; yeni kayıt kaydedildiğinde harita adı yazılıyor.

Kodda konum için zemin eğimi, kapsül yüksekliği ve navmesh projeksiyonu doğrulanıyor. UE 5.7 C++ derlemesi başarılı; PIE henüz yapılmadı.

## Bozuk kayıt dosyasını otomatik kaydın ezmemesi — 2026-09-28

- [ ] Geçerli eski ve yeni kayıtlar normal yükleniyor; otomatik ve manuel kaydetme çalışıyor.
- [ ] Kayıt dosyası mevcut fakat okunamaz/`bValid=false` durumunda 60 sn otomatik kayıt, MMOSave ve EndPlay özgün dosyayı değiştirmiyor; logda hata görülüyor.
- [ ] Kayıt dosyası hiç yoksa yeni karakter sınıf seçimiyle başlıyor ve sonrasında kaydediliyor.
- [ ] Başarılı yüklemeden sonra manuel MMOLoad ile bozuk dosya denenirse sonraki otomatik/manuel kayıt dosyayı ezmiyor.

UE 5.7 C++ derlemesi başarılı; bozuk dosya senaryoları PIE'de henüz uygulanmadı.

## Kayıt konumu QA sertleştirmesi — 2026-09-28

- [ ] Navmesh yükleme anında geçici olarak hazır değilse eski dosya konumu otomatik kayıtta ezilmiyor; navmesh hazır olunca konum yeniden deneniyor.
- [ ] Kayıt konumu duvarın içinde veya alçak tavanın altındaysa kapsül çakışması saptanıp PlayerStart'a dönülüyor.
- [ ] Kaydedilen konumun üstündeki çatı, merkezden aşağı yapılan zemin izini yanlış engellemiyor; normal konum korunuyor.
- [ ] MMOLoad ile geçersiz konum elle yüklenirse karakter mevcut konumunda kalmak yerine PlayerStart'a dönüyor.
- [ ] Harita adı olmayan eski kayıt Summer_scene'de güvenli konuma dönebiliyor; başka haritada eski XY uygulanmıyor.
- [ ] Navmesh hiç oluşmayan haritada kaydetme kapısının beklediği ve mevcut disk dosyasının korunması logdan doğrulanıyor.

UE 5.7 `MMOCharacter.cpp` yeniden derlendi; PIE/iki oyunculu doğrulama henüz yapılmadı.

## Yetenek sınıf/seviye kapısı ve kayıt temizliği — 2026-09-28

- [ ] Savaşçı, Okçu, Büyücü ve Rahip kendi öğrendikleri yetenekleri gereken seviyede kullanabiliyor.
- [ ] Eski kayıtta mastery rütbesi sıfır olsa da aynı sınıfa ait, seviyeye uygun öğrenilmiş yetenekler korunuyor.
- [ ] Bozuk kayda başka sınıfın veya daha yüksek seviyenin yeteneği ve hotbar girişi yazılırsa yüklemede temizleniyor; sunucu RPC ile de yeteneği kullandırmıyor.
- [ ] Bilinmeyen yetenek kimliği ve geçersiz sınıf değeri içeren kayıt yüklenirken oyun çökmüyor.

UE 5.7 `MMOSkillComponent.cpp` yeniden derlendi; PIE ve bozuk kayıt senaryoları henüz uygulanmadı.

## Host debug RPC yetkisi — 2026-09-28

- [ ] Listen server'da host `MMOBuff`, `MMOSpawnMob` ve `MMOGiveItem` komutlarını kullanabiliyor.
- [ ] Uzak istemci aynı komutları yazınca veya ilgili Server RPC'leri doğrudan gönderince buff, mob ya da eşya oluşmuyor.
- [ ] Host `MMOPvP` komutunun mevcut yetki davranışı değişmedi.

UE 5.7 `MMOCharacter.cpp` yeniden derlendi; iki oyunculu PIE doğrulaması henüz yapılmadı.

## Manuel kayıt/yükleme RPC sınırları — 2026-09-29

- [ ] Host `MMOLoad` ile kendi karakterini elle geri yükleyebiliyor; otomatik giriş yüklemesi çalışıyor.
- [ ] Uzak istemci `MMOLoad` ya da `ServerLoadCharacter` gönderince eski XP, eşya ve görev durumu geri gelmiyor.
- [ ] Uzak istemci `MMOSave` ile kaydedebiliyor; 10 saniyeden sık Reliable kayıt istekleri diske tekrar tekrar yazmıyor.
- [ ] İlk otomatik yükleme tamamlanmadan gönderilen erken kayıt isteği, yükleme sonrasındaki ilk geçerli isteği 10 saniyeliğine kilitlemiyor.
- [ ] Otomatik 60 saniyelik ve çıkıştaki kayıtlar manuel RPC sınırından etkilenmiyor.

UE 5.7 `MMOCharacter.cpp/.h` yeniden derlendi; çok oyunculu PIE henüz yapılmadı. Oyuncu adı dosya adı olarak kullanıldığı için kalıcı hesap kimliği olmayan misafirlerin ad çakışması ayrıca çözülmeli.

## Kayıt yuvası adının doğrulanması — 2026-09-29

- [ ] `MMOSetName Ozan`, `MMOSetName Ozan Can` ve Türkçe harfli oyuncu adlarıyla kaydet/yükle akışı çalışıyor; boşluk eski `_` yuva biçimine dönüşüyor.
- [ ] `MMOSetName` için boş, 24 karakterden uzun, `/`, `\\`, `..` veya kontrol karakteri içeren adlar reddediliyor.
- [ ] Platformdan gelen beklenmedik PlayerState adında kayıt yuvası yol ayırıcı veya nokta içermiyor; boş güvenli ad `Default` oluyor.
- [ ] Önceki geçerli adlarla oluşturulmuş kayıtlar aynı yuva adıyla okunuyor.

UE 5.7 `MMOCharacter.cpp` yeniden derlendi; PIE ve kayıt uyumu doğrulaması henüz yapılmadı. Bu biçim denetimi oyuncu kimlik doğrulaması sağlamaz: iki kişinin aynı geçerli adı seçip aynı kayda erişmesi için hesap kimliği tabanlı ayrı bir tasarım gerekir.

## Yakın dövüş mesafesi ve gerçek hasar — 2026-09-29

- [ ] Aynı zemindeki normal yakın dövüş menzili çalışıyor; menzil dışındaki hedef vurulamıyor.
- [ ] Yatayda yakın fakat farklı katta/yükseklikte bulunan seçili hedefe yakın dövüş hasarı gitmiyor.
- [ ] Savunmalı mobda ekrandaki `VURDUN` değeri gerçek azalan HP ile tutarlı; ham saldırı gücünü göstermiyor.
- [ ] Canlanma korumasındaki oyuncuya saldırıldığında gerçek hasar 0 gösteriliyor.

UE 5.7 `MMOCharacter.cpp` yeniden derlendi; PIE henüz yapılmadı.

## İstemci beceri hedefinin sunucuda doğrulanması — 2026-09-29

- [ ] İki oyunculu PIE'de istemci iki yakın mobdan ikincisini seçip tek hedefli beceri kullanınca seçilen mob etkileniyor; yakın olan başka mob hedefe geçmiyor.
- [ ] İstemci bir takım arkadaşını seçip Rahip hedefli şifa kullanınca 800 birim içinde arkadaş iyileşiyor; host tarafında da aynı davranış görülüyor.
- [ ] 800 birim dışındaki, ölü, başka dünyadaki veya mob türündeki hedef şifa almıyor; hedefli şifa kendine düşüyor.
- [ ] Hileli istemcinin uzak/ölü düşman hedefi beceriye verilse bile sunucu menzili ve canlılığı doğruluyor; yakın geçerli hedefe düşüş davranışı korunuyor.
- [ ] Mana ve cooldown sunucu tarafında tek kez uygulanıyor; AoE, kendine iyileştirme ve buff becerileri eski davranışını koruyor.

UE 5.7 `MMOSkillComponent.cpp/.h` yeniden derlendi; çok oyunculu PIE henüz yapılmadı.

## Aynı oturumda kayıt adı çakışması — 2026-09-29

- [ ] İki oyunculu PIE'de bir oyuncu `MMOSetName Ozan Can` kullandıktan sonra diğeri `MMOSetName Ozan_Can` deneyince ikinci ad değişmiyor.
- [ ] `Ozan` ve `ozan` aynı kayıt yuvasına eşlendiği için ikinci oyuncunun isteği reddediliyor.
- [ ] Farklı geçerli adlar seçildiğinde her iki oyuncunun otomatik/çıkış kaydı ayrı dosyalarda kalıyor.

UE 5.7 `MMOCharacter.cpp` yeniden derlendi; PIE henüz yapılmadı. Bu koruma yalnızca aynı anda bağlı oyuncuların ad çakışmasını engeller; çevrimdışı karakter sahipliğini doğrulamak için kalıcı hesap kimliği hâlâ gerekli.

## Hub güvenli alanından mob hasarı — 2026-09-29

- [ ] PlayerStart çevresindeki 700 birim güvenli alanda Okçu/Büyücü menzilli saldırıları ve DoT mobun HP'sini düşürmüyor; XP/ganimet kazanılmıyor.
- [ ] Sınırın dışına çıkınca aynı beceriler normal hasar ve zayıflatma uyguluyor; mob saldırganı hedefliyor.
- [ ] Mob hasarı reddedildiğinde Poison/Slow gibi ikincil etkiler uygulanmıyor; eve dönen mob da hasar ve zayıflatma almıyor.
- [ ] Mobların oyuncuya yönelik mevcut hub koruması ve uzak oyuncuların normal dövüşü korunuyor.

UE 5.7 `MMOMob.cpp` ve `MMOSkillComponent.cpp` yeniden derlendi; PIE henüz yapılmadı. Güvenli alan içinden saldırı girişimi halen mana ve bekleme süresi harcayabilir; oyuncu geri bildirimi ayrıca gözlemlenmeli.
