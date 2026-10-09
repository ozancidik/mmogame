# Oynayarak Test Kontrol Listesi

Bu dosya, Claude'un otonom (sen yokken) geliştirdiği özelliklerin **senin PIE'de test etmen** gereken maddelerini biriktirir.
Her özellik derlenip commit'lendiğinde buraya eklenir. Test edip onayladıkça maddeleri işaretle (`[x]`).

> Not: Claude kodu yazar, derler ve derlendiğini garanti eder. Oynanış/his/görsel doğrulaması sana kalır.
>
> **Sayım (2026-10-05):** 992 açık (`- [ ]`), 4 işaretli (`- [x]`) madde. Hepsini sırayla deneme; önce `docs/PLAYTEST_RUNBOOK.md`.

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
- [ ] **Regresyon - normal birlesme:** Toplam <=8 olacak sekilde iki kucuk partinin LIDERLERI arasinda (or. 3+3=6) birlesmeyi dene → tum uyeler tek partide gorunmeli (2026-10-07: grup birlestirmede lider onayi zorunlu).
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

## Hub saldırı girişiminde kaynak koruması — 2026-09-29

- [ ] PlayerStart çevresindeki 700 birim içinde yakın dövüş saldırısı bekleme süresini başlatmıyor.
- [ ] Aynı alanda hasar veren hedefli/AoE becerileri mana ve bekleme süresi harcamadan reddediliyor; ekranda güvenli bölge mesajı görülüyor.
- [ ] Kendine şifa ve buff becerileri hub içinde çalışıyor.
- [ ] Hub sınırının hemen dışında normal saldırı ve beceri kullanımı devam ediyor; istemci ve host sunucuda aynı sınırı uyguluyor.

UE 5.7 `MMOTargeting.cpp`, `MMOCharacter.cpp` ve `MMOSkillComponent.cpp` yeniden derlendi; PIE henüz yapılmadı. Tasarımda mobun `HubSafeRadius` varsayılanı da 700; Blueprint'te başka bir değer verilirse bu ortak saldırı kapısı ayrıca uyarlanmalı.

## Ölü parti üyesine görev kredisi verilmemesi — 2026-09-29

- [ ] İki oyuncu aynı öldürme görevini alsın. İkinci oyuncu mobun 3000 birim yakınında ölü yatarken birinci oyuncu mobu öldürsün: ölünün XP'si ve görev sayacı artmasın.
- [ ] İkinci oyuncu canlandıktan sonra menzil içindeki yeni öldürmede iki oyuncunun XP'si ve görev sayacı artsın.
- [ ] Öldüren oyuncu geçerli ve hayattayken tek oyunculu ödül akışı değişmesin.

UE 5.7 `MMOMob.cpp` yeniden derlendi (`Result: Succeeded`); PIE henüz yapılmadı.

## Dolu çantada ekipman ganimeti — 2026-09-29

- [ ] Ekipman çantasını 12 parçaya doldurup yerdeki ekipmana yaklaş: eşya çantaya eklenmesin ve yerde kalmaya devam etsin.
- [ ] Çantadan bir parça satıp pickup alanından çıkıp yeniden gir: yerdeki eşya yalnızca bir kez alınsın.
- [ ] Çanta doluyken altın ve istiflenebilir tüketilebilir ganimet normal şekilde alınsın.
- [ ] Host ve uzak istemcide aynı sunucu kararı uygulansın; dolu çanta mesajının istemcide görünürlüğü ayrıca gözlensin.

UE 5.7 `MMOLootPickup.cpp` yeniden derlendi (`Result: Succeeded`); PIE henüz yapılmadı. Yerdeki eşyanın mevcut yaşam süresi dolarsa normal şekilde kaybolur.

## Uzak istemcide ganimet rengi — 2026-09-29

- [ ] İki pencereli PIE'de altın, tüketilebilir ve ekipman ganimeti oluştur: uzak istemci sırasıyla sarı, mavi ve yeşil küre görsün.
- [ ] Ganimet ortaya çıkışında kısa süreli yanlış renk ve sonradan görselin düzelmesi olup olmadığını gözlemle.
- [ ] Host görünümü, pickup toplama ve 30 saniyelik yok olma davranışı değişmesin.

UE 5.7 `MMOLootPickup.h/.cpp` UHT dahil yeniden derlendi (`Result: Succeeded`); PIE henüz yapılmadı.

## PlayerStart olmayan haritada yanlış spawn engeli — 2026-09-29

- [ ] Summer_scene üzerinde PlayerStart bulunurken mevcut mob ve hub NPC'leri her zamanki yerlerinde doğsun.
- [ ] PlayerStart bulunmayan Winter_scene açıldığında GameMode orijinde mob/NPC üretmesin; günlükte `SpawnMobArea skipped` hatası görülsün.
- [ ] Winter_scene için PlayerStart ve gezinme ağı eklendikten sonra o haritanın ayrı içerik yerleşimi ayrıca doğrulansın.

UE 5.7 `MMOGameMode.cpp` yeniden derlendi (`Result: Succeeded`); PIE henüz yapılmadı. Bu kontrol Winter_scene'i oynanabilir hâle getirmez; yalnızca geçersiz orijin yerleşimini önler.

## Winter_scene'e Summer_scene moblarının taşınmaması — 2026-09-29

- [ ] Winter_scene'de kaydedilen PlayerStart bulunsun; Lv1–22 spawner'ları ve Summer hub NPC'leri doğmasın, `no configured gameplay layout` uyarısı görünsün.
- [ ] Summer_scene'de mevcut tüm spawner ve hub NPC yerleşimi sürsün.
- [ ] PlayerStart içeren başka bir harita açıldığında da Summer mob/NPC yerleşimi oluşmasın; yalnız Summer_scene (PIE adı dâhil) bu yerleşimi kullansın.

UE 5.7 `MMOGameMode.cpp` yeniden derlendi (`Result: Succeeded`); PIE henüz yapılmadı. Winter_scene için PlayerStart ve NavMeshBoundsVolume kaydedildi; navmesh bağlantısı ve ayrı Lv41+ yerleşimi hâlâ doğrulanmalı.

## Lv41–50 veri ve Winter_scene ilk yerleşimi — 2026-09-29

- [ ] Winter_scene açıldığında `MMO_Winter_PlayerStart` `(0,-1500,150)` ve `MMO_Winter_NavBounds` `(0,-1500,0)` aktörleri yalnız birer kez bulunsun.
- [ ] PlayerStart çevresinde zemin/çarpışma güvenli olsun; NavMeshBounds içinde yeşil navmesh ve aday `(-3000,-1500)` / `(3000,-1500)` merkezlerine tam yürüyüş yolu ayrıca doğrulansın.
- [ ] Dört Winter normali (Lv42/44/46/48) ile Lv50 `WinterSovereign` arketipleri, dört Lv42 ekipman ve beş bağlı görev kimliği veri tablosunda görülsün.
- [ ] Winter spawner'ları henüz eklenmediği için yeni öldürme görevleri oynanışta tamamlanabilir sayılmasın; bölge geçişi de henüz yok.
- [ ] Harita görselinde mevcut ARSkyLight import uyarısının etkisi kontrol edilsin; kaydetmeden önceki haritanın kopyası `Saved/Winter_scene_before_navigation.umap` altında tutuldu.

UE 5.7 üç C++ veri tablosu derlendi (`Result: Succeeded`); headless editör harita kaydetme işlemini başarılı bildirdi. PIE/navmesh yolu henüz doğrulanmadı.

## Kayıt konumu: navmesh hiç oluşmazsa sonsuz beklemeyi bitir — 2026-09-29

- [ ] Kaydedilen konumun haritasında navmesh geç açıldığında konum en fazla 15 adet bir saniyelik denemede geri yüklensin.
- [ ] Navmesh hiç oluşmazsa 15. denemeden sonra karakter PlayerStart'a alınsın; sonraki otomatik ve çıkış kaydı kalıcı olarak kilitlenmesin.
- [ ] Bozuk/kaybolmuş kayıt dosyasının mevcut kaydı ezmesini önleyen `bInitialLoadComplete` koruması çalışmaya devam etsin.

Bu yol kaynak kodda sınırlandı; PIE'de uygulanmış senaryo henüz doğrulanmadı.

## Varsayılan harita ve paketleme yolu — 2026-09-29

- [ ] Proje editörde açıldığında klasör yerine gerçek `/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene` haritası yüklensin; boş Untitled haritasına düşmesin.
- [ ] Paketleme ayarlarının harita listesinde Summer ve Winter sahneleri tam uzun paket adlarıyla bulunsun.
- [ ] Paketlenmiş yapıda iki harita da mevcut olsun; Winter bölgesinin oynanabilirliği ayrı navmesh/spawner/harita geçişi işidir.

Diskteki `.umap` yolları ile eski `DefaultEngine.ini` ayarları farklı klasörleri gösteriyordu; ayarlar dosya yollarına eşitlendi. Paketleme ve editör açılışı henüz çalıştırılarak doğrulanmadı.

## Yeni karakter için tıklanabilir sınıf seçimi — 2026-09-29

- [ ] Yeni sunucu ve istemci karakterinde üstte Savaşçı, Okçu, Büyücü ve Rahip düğmeleri görünsün; seçilen sınıfın statları ve başlangıç skilleri gelsin.
- [ ] Varsayılan Savaşçı düğmesine tıklamak da seçim ipucunu kapatsın; panelde boş alana tıklamak karakteri yürütmesin.
- [ ] Satıcı/görev/envanter gibi pencere açıkken sınıf paneli gizlensin; pencere kapanınca seçim yapılmadıysa geri gelsin.
- [ ] Kayıtlı karakterde seçim paneli görünmesin; konsoldaki `MMOClass` yolu ve sunucu tarafı savaş/cooldown kısıtları çalışmaya devam etsin.

HUD düğmeleri aynı `RequestSetupForClass` sunucu yolunu kullanıyor. UE 5.7 C++ derlemesi başarılı; PIE etkileşimi henüz doğrulanmadı.

---

# Lv51–57 Frostbound dilimi (Winter_scene) — 2026-10-03

> Derleme temiz. PIE'de hiç oynanmadı. Winter'a geçmek için host HUD'undaki "Tüm oyuncuları Kış'a taşı" düğmesi.

- [ ] **Spawner'lar görünüyor mu:** Winter'da doğudaki `(3000,0)` cebinde 5 Buzbağlı Gözcü (Lv51), kuzeybatı `(-3500,3000)` cebinde (3 Ekim duman testi sonrası taşındı) 6 Buzbağlı Nöbetçi (Lv55), `(2000,2800)`'de tek Buzbağlı Muhafız elit (Lv57) zemin üzerinde doğmalı. Output Log'da `Spawner ... skipping spawn` uyarısı çıkmamalı.
- [ ] **Hub güvenliği:** Winter hub'ında (PlayerStart çevresi) beklerken hiçbir yeni mob saldırmamalı.
- [ ] **Görev zinciri:** `Q_WinterSovereign` tamamlandıktan sonra Lv51'de `Q_FrostboundScouts` alınabilmeli → Buzbağlı Bot; ardından Sentinels (Kask), Line (Zırh, 6 Nöbetçi), Warden (Kılıç). Her ödül alındığında Lv51 seti kuşanılabilmeli.
- [ ] **Denge gözlemi (not al):** Lv51 bir karakterle Gözcü öldürme süresi, alınan hasar, seviye başına kill (hedef 12–25).

# Lv58–65 Rift dilimi (Summer_scene dış cepleri) — 2026-10-03

> Derleme temiz, PIE yok. `Q_FrostboundWarden` bitince oyuncu Summer'a döner (host düğmesi).

- [ ] **Spawner'lar (QA sonrası taşındı):** Summer'da `(800,-4800)` Yarık Arayıcısı (Lv58), `(-4800,4800)` Yarık Bekçisi (Lv62), `(4800,4000)` tek Yarık Habercisi elit (Lv65) zeminde doğmalı. Summer navmesh'i bunun için ±8500'e genişletildi (`Scripts/ExtendSummerNavigation.py`).
- [ ] **Summer navmesh:** Editörde Summer_scene'i aç, `P` ile navmesh'i göster — hub, eski Lv1–40 bölgeleri ve üç yeni Rift cebi yeşil, aralarındaki yollar kesintisiz olmalı.
- [ ] **Erken oyun güvenliği:** Rift cepleri artık tüm erken/orta cep merkezlerinden en az ~3400u uzakta (eski 1450–1660u'ya karşı). Lv7 karakterle Gölge Suikastçı, Lv31 ile Alacakaranlık cebi, Lv40 ile Alacakaranlık Hükümdarı farm'ı yaparken hiçbir Lv58+ mob savaşa katılmamalı.
- [ ] **Görev zinciri:** `Q_RiftSeekers` (Bot) → `Q_RiftKeepers` (Kask) → `Q_RiftWatch` (Zırh) → `Q_RiftHerald` (Kılıç); Lv58 seti kuşanılabilmeli.

# Winter navmesh genişletmesi + Lv66–80 (Kül / Uçurum) — 2026-10-03

> Winter_scene haritası (umap) yeniden kaydedildi: navmesh sınırı X±8000 / Y −8500..5500'e büyüdü. Derleme temiz, PIE yok.

- [ ] **Navmesh sağlam mı:** Editörde Winter_scene'i aç, `P` ile navmesh'i göster — hub çevresi ve yeni uzak bölgeler yeşil olmalı; eski Lv42–57 cepleri bozulmamış olmalı.
- [ ] **Spawner'lar:** Kül Öncüsü `(1200,-5200)`, Kül Bağlayıcı `(-2000,-6800)`, Kül Tiranı elit `(4400,-5200)`, Uçurum Gezgini `(-5500,4500)` (duman testi sonrası taşındı), Uçurum Koruyucusu `(4400,3600)` (yüksek arazi), Uçurum Naibi elit `(6800,2000)` (Z~580 yüksek arazi; ilk konum `(-7600,-1200)` navmesh kenarındaydı, QA sonrası taşındı — oraya yürüyerek çıkılabilmeli).
- [ ] **Görev zincirleri:** `Q_RiftHerald` → Kül zinciri (Lv66 set) → `Q_AshTyrant` → Uçurum zinciri (Lv71'de açılır, Lv72 set) → `Q_AbyssRegent` (Lv79'da alınır; Naip Lv80).
- [ ] **Lv80 son elit:** Uçurum Naibi loot'u +4..+6 Lv72 setinden düşmeli.

## QA denetimi: `627e79a` Frostbound dilimi — 2026-10-03

> **Derleme:** `627e79a` izole bir git worktree'de sıfırdan derlendi (editör/Live Coding etkisi yok, "up to date" yanılgısı yok): `Result: Succeeded`, 0 uyarı, 0 hata. Ana çalışma kopyası derlenmedi (o sırada commit'lenmemiş Rift işi + çalışan `UnrealEditor-Cmd` vardı).
>
> **Sağlam bulunanlar (kodla doğrulandı):** görev hedefleri arketiplerle birebir eşleşiyor (`FrostboundScout` / `FrostboundSentinel` / `FrostboundWarden`); 4 ödül item ID'si veritabanında var; ön koşul zinciri `Q_WinterSovereign → Scouts → Sentinels → Line → Warden` geçerli. Band 51-51 yalnız Gözcü'yü, 55-55 yalnız Nöbetçi'yi seçiyor, elitler dışlanıyor, bant hiç boş kalmıyor (hata logu tetiklenmez). Elit forced-id doğru. Muhafız (Lv57) elit loot havuzu tam olarak 4 Lv51 parça (MaxReq=51); Kış Hükümdarı (Lv50) havuzu değişmedi (hâlâ 4 Lv42 parça; Lv51 sızmıyor). Lambda `[&]` yalnız yerel `World`'ü senkron kullanıyor, güvenli; parametre adları GameMode üyelerini gölgelemiyor. Kayıtlar FName tabanlı ve yalnız ekleme yapıldı, eski kayıtlar bozulmaz. Tüm değişiklik sunucu tarafında (GameMode spawn + veri tabloları).

- [ ] **[ORTA] Elit/mob aggro alanları çakışıyor — yanlışlıkla çekme riski.** Spawn yarıçapı + 450u gezinme ile mob'lar merkezden en fazla ~600u (elit) / ~675u (normal) uzaklaşabiliyor; aggro 600u. Merkezler arası: Muhafız–Hükümdar 1414u, Hükümdar–Nöbetçi 1442u, Muhafız–Nöbetçi 1811u. Bu yüzden iki mob arası en kısa mesafe 167–536u'ya inebiliyor (iki mob'un aynı anda aggro alması için ≤1200u yetiyor). Lv50 karakter `Q_WinterSovereign` yaparken Lv57 Muhafız'ı ya da Lv55 Nöbetçileri çekebilir. **Test:** Lv50 karakterle (`MMOGainXP` ile seviye atla) Winter'da `(-3000,0)`'daki Kış Hükümdarı'na saldır; savaşı 1–2 dk sürdür, Hükümdar'ın etrafında dön. Muhafız `(-2000,1000)` ya da Nöbetçiler `(-3800,1200)` savaşa katılıyor mu, not al. Ardından Muhafız'ın etrafında aynı testi yap.
- [ ] **[ORTA, doğrulanmalı] Nöbetçi cebi navmesh sınırının kenarında.** Commit'lenmiş `MMO_Winter_NavBounds` X ±4000, Y −4500..1500 (bkz. `Scripts/ExtendWinterNavigation.py` açıklaması). Nöbetçi cebi `(-3800,1200)` r225 → X −4025'e, Y 1425'e kadar uzanıyor; 450u gezinmeyle X ≈ −4475 / Y ≈ 1875'e gidebiliyor. Bu noktalar sınırın dışında. Prob 16/16 OK dedi ama gezinme ve kovalama alanı denenmedi. Ayrıca çalışma kopyasında commit'lenmemiş bir `Winter_scene.umap` değişikliği (navmesh genişletme) var; prob sonuçları commit'lenmiş haritayla aynı olmayabilir. **Test:** Nöbetçi cebinde 2–3 dk bekle; batı/kuzey kenara giden Nöbetçi takılıyor ya da yerinde titriyor mu? Bir Nöbetçiyi çekip batıya (X<−4000) doğru kaç; düzgün kovalayıp leash ile eve dönüyor mu?
- [ ] **[ORTA, bilgi/denge verisi] Lv51→55 arasında görev boşluğu.** `Q_FrostboundSentinels` Lv55 istiyor. Lv51→55 toplam ~2.40M XP; Gözcü görevinin ödülü + 5 kill düşülünce ~2.17M XP kalıyor, bu da **~72 Gözcü kill'i** demek. Bu aralıkta tek kaynak 5 mob'luk cep (4 sn top-up). Seviye başına kill sayısı: Gözcü 18→26 (Lv51→57; Lv57'de plan hedefi 25'i biraz aşıyor), Nöbetçi 15→22, Muhafız 10→15. Görev ödül XP'si Lv51→57 toplamının (3.83M) %9'u; görev + zorunlu kill'ler birlikte %25. Lv50'de Kış zincirini bitiren oyuncu Gözcü görevi için de ~509K XP kasmalı (17 Hükümdar ya da 23 Kış Hayaleti kill'i). **Test:** Lv51'de 10 dk Gözcü kas; dakikadaki kill sayısını ve aldığın XP'yi not al; 72 kill'in kaç dakika süreceği buradan hesaplanır.
- [ ] **[KÜÇÜK, bilerek mi?] Normal loot üst kademeye sızıyor.** Normal havuz `RequiredLevel <= MobLevel+3`: Lv48 Kış Hayaleti → Lv51 seti dahil (41 parçalık havuzun 4'ü, ekipman şansı %25 → kill başına ~%2.4). Aynı şekilde Lv55 Nöbetçi → Lv58 Rift seti (527ca1f'den sonra). Emsal tutarsız: Lv33 Dusk seti bilerek Lv29+3'ün dışına konmuştu, Lv42 Kış seti ise Lv39 Dusk Hayaleti'nden düşüyor. Kuşanmada seviye kontrolü olduğu için güç sızıntısı yok; zaten satıcı tüm kademeleri seviye şartı olmadan satıyor (Lv51 parçalar 1.7K–2.8K altın; Gözcü ortalama 4.5K altın/kill bırakıyor, bu satıcı davranışı bu commit'ten önce de vardı). **Test:** Lv48'de 40–50 Kış Hayaleti kes, düşen Buzbağlı parçaları say; satıcıda "Buzbağlı Kılıç" Lv48 karakterle satın alınabiliyor ama kuşanılamıyor mu, kontrol et.
- [ ] **[KÜÇÜK] Spawn olmayan yedek yol.** `GetRandom()` (bandsız/forced'sız) havuzu artık Lv51/55 normalleri de içeriyor. Koddaki hiçbir spawner bu yolu kullanmıyor; ama haritaya elle yerleştirilmiş, arketipi boş bir `AMMOMob` varsa Lv55 Nöbetçi olarak doğabilir (önceden de Lv42-48 için geçerliydi). **Test:** Summer ve Winter'da Play bas, Output Log'da arketipi boş mob kalmadığını kontrol et; hub yakınında beklenmedik yüksek seviyeli bir mob görülmemeli.
- [ ] **[KÜÇÜK, zararsız] Lambda'nın `TopUp` parametresi band spawner'larında yok sayılıyor** (0.f geçiliyor ama atanmıyor; varsayılan 4 sn kalıyor). Davranış doğru, sadece yanıltıcı. Test gerekmez.
- [ ] **Elit loot doğrulaması:** Lv57 karakterle Muhafız'ı 4–6 kez öldür. Düşen ekipmanların hepsi Buzbağlı (Lv51) ve +3..+5 olmalı. Lv50 karakterle Kış Hükümdarı'nı 4–6 kez öldür: hiçbir Buzbağlı parça düşmemeli, yalnız Kış (Lv42) seti düşmeli.
- [ ] **Eski kayıt uyumluluğu:** `Q_WinterSovereign`'ı önceden tamamlamış eski bir kayıtla Winter'a gir, `MMOGainXP` ile Lv51'e çık. Görev penceresinde "Buzbağlı Gözcüler" alınabilir görünmeli; kayıt/yükleme (`MMOSave`/`MMOLoad`) sonrası aktif Frostbound görevinin ilerlemesi korunmalı.

## QA denetimi: `527ca1f` Rift + `580ffc7` Kül/Uçurum + `c040e18` Frostbound düzeltmesi — 2026-10-03

> **Derleme:** Editör kapalıyken dört değişen `.cpp`'nin zaman damgası güncellenip `Build.bat MyProjectEditor Win64 Development` zorla çalıştırıldı. Dördünün de bulunduğu iki unity dosyası (`Module.MyProject.3/4.cpp`) yeniden derlendi, DLL yeniden bağlandı: `Result: Succeeded`, 0 uyarı, 0 hata.
>
> **Sağlam bulunanlar (python ile kaynaktan ayrıştırılıp doğrulandı):** 48 arketip, 53 item, 66 görevde yinelenen ID yok. `Q_WinterSovereign → FrostboundScouts → Sentinels → Line → Warden → RiftSeekers → RiftKeepers → RiftWatch → RiftHerald → AshVanguard → AshBinders → AshLine → AshTyrant → AbyssStriders → AbyssGuardians → AbyssLine → AbyssRegent` zinciri kesintisiz: dal ya da kırık ön koşul yok. 17 görevin hepsinde hedef arketip ve ödül item ID'si veritabanında var. Bantlar tam olarak tek arketip seçiyor: 58→RiftSeeker, 62→RiftKeeper, 66→AshVanguard, 68→AshBinder, 72→AbyssStrider, 76→AbyssGuardian (51/55 de doğru). Eski bantlar (0-5, 6-7, 8-12, 16-22, 25-29, 31-39, 42-48) değişmedi. `RiftHerald`, `AshTyrant` ve `AbyssRegent` forced ID'leri tabloda var ve `bIsElite=true`, bu yüzden hiçbir banda sızmıyorlar. Elit loot havuzları: RiftHerald (Lv65) yalnız 4 Rift/Lv58 parça, AshTyrant (Lv70) yalnız 4 Ashen/Lv66 parça, AbyssRegent (Lv80) yalnız 4 Abyss/Lv72 parça veriyor. Eski 10 elitin havuzu birebir aynı; FrostboundWarden hâlâ yalnız Lv51 seti veriyor, Rift seti sızmıyor. `EliteMaxItemPlus=6` < `MaxPlus=8`; ayrıca `MMOMob.cpp:769`'da clamp var. Lv51–79 her seviye kendi bandının normal mob'uyla **18.1–22.2 kill** tutuyor, 12–25 hedefinin tamamen içinde (tablo aşağıda). Winter'daki 6 yeni cep birbirine ve eski ceplere ≥2530u, hub'a ≥3890u uzakta (commit iddiası doğru). Değişikliklerin tamamı sunucu tarafında: GameMode spawn ve statik veri tabloları. Sır ya da anahtar yok.
>
> **XP tablosu (seviye → sonraki seviye için gereken XP / doğru mob XP'si = kill):** 51: 543K/30K=18.1 · 52: 19.3 · 53: 20.6 · 54: 21.8 · 55: 695K/36K=19.3 · 56: 20.4 · 57: 21.6 · 58: 821K/45K=18.2 · 59: 19.2 · 60: 20.2 · 61: 21.3 · 62: 1.005M/52K=19.3 · 63: 20.3 · 64: 21.2 · 65: 22.2 · 66: 1.208M/60K=20.1 · 67: 21.0 · 68: 1.317M/66K=20.0 · 69: 20.8 · 70: 21.7 · 71: 1.489M/82K=18.2 (Strider; görev Lv71'de açılıyor) · 72: 18.9 · 73: 19.6 · 74: 20.4 · 75: 21.2 · 76: 1.799M/95K=18.9 · 77: 19.6 · 78: 20.3 · 79: 2.000M/95K=21.0. Görev XP'sinin bant toplamına oranı: Frostbound %7.5, Rift %6.5, Kül %11.9, Uçurum %5.6. Lv80'de alınan 350K kullanılamadığı için Uçurum'da bu oran %5.6'da kalıyor. Görevlerin zorunlu kill'leri de eklenince oranlar %20.5 / %17.6 / %29.0 / %15.8.

- [ ] **[ORTA — en önemli bulgu] Summer'daki Rift cepleri düşük seviyeli bölgelerle örtüşüyor; Lv7–40 oyuncu yanlışlıkla Lv58–65 mob çekebilir.** Commit'teki "≥1400u uzak" ölçütü yetmiyor. Rift mob'unun algılama erişimi spawn yarıçapı + 450u gezinme + 600u aggro olarak hesaplanıyor: Arayıcı/Bekçi için 1275u, Haberci için 1200u. Erken bölgede savaşan oyuncu da o cebin mob'larının dolaştığı alanda duruyor: yarıçap + 450 + 160u vuruş menzili (elit cebi 760u, r225 cebi 835u). Bu iki erişimin toplamı cepler arası mesafeden büyükse örtüşme var. Örtüşen çiftler:
  - Gölge Suikastçı (Lv7 elit, `(1400,1350)`) ile Yarık Arayıcısı (Lv58, `(2400,2400)`): mesafe 1450u, **585u örtüşme**. Monte Carlo anlık olasılığı %1.35. Gezinme 4–8 sn'de yenilendiği için bu dakikada yaklaşık %13 eder. Lv7 karakter, vuruşu 222 olan Arayıcı'dan 1–2 vuruşta ölür (kaba tahmin, PIE'de doğrulanmalı).
  - Alacakaranlık cebi (Lv31–39, `(1000,3200)`) ile Arayıcı: 1612u, **498u örtüşme**. Anlık %0.34, dakikada ~%3. 30 dakikalık farmda en az bir çekiş olasılığı ~%64. Hub'dan Alacakaranlık cebine düz yürüme yolu da Arayıcı merkezine 1575u'ya kadar yaklaşıyor; 300u pay kalıyor.
  - Alacakaranlık Hükümdarı (Lv40 elit, `(-3200,1800)`) ile Yarık Bekçisi (Lv62, `(-4000,3200)`): 1612u, **423u örtüşme**. Anlık %0.22, dakikada ~%2.
  - Ork Reisi (Lv6, `(1540,0)`) ile Yarık Habercisi (Lv65 elit, `(3200,0)`): 1660u, **300u örtüşme**. Haberci tek mob olduğu için olasılık düşük: anlık %0.02. Ana alan (Lv1–5) ile Haberci arasında sınırda 10u örtüşme var, olasılığı ~0. Bu yüzden yukarıdaki Rift bölümünde "Ork Reisi çevresinde Haberci seni fark etmemeli (~1660u)" maddesi **tam güvenli değil**, yalnız düşük olasılıklı.
  - Güvenli çıkanlar: Örümcek Ana–Arayıcı (+93u pay), ana alan–Arayıcı, Golem/Magma/Örümcek/Suikastçı–Haberci (+235..+290u), Buz Lordu/Obsidyen Reisi–Bekçi (≥+987u).
  - **Öneri yönü (mmo-developer):** Örtüşmeyi sıfırlamak için Arayıcı cebi Suikastçı ve Alacakaranlık merkezlerinden ≥2110u, Bekçi Alacakaranlık Hükümdarı'ndan ≥2035u, Haberci Ork Reisi'nden ≥1960u uzakta olmalı. Alternatif: bu üç cebin `WanderRadius`'unu düşür.
  - **Test:** `MMOClass` ile yeni bir Lv7 karakter aç. `(1400,1350)`'deki Gölge Suikastçı'ya saldır ve dövüşü kuzeydoğuya, yani +X/+Y yönüne doğru 1–2 dk sürdür. Bir **Yarık Arayıcısı** savaşa katılırsa adını ve aldığın hasarı not al. Sonra `MMOGainXP` ile Lv31'e çık ve Alacakaranlık cebinde `(1000,3200)` 5 dk farm yap; Arayıcı yaklaşıyor mu? Lv40'ta Alacakaranlık Hükümdarı'nı öldürürken Bekçi geliyor mu?
- [ ] **[ORTA, bilgi/denge] Uzun görevsiz grind aralıkları.** Seviye başına kill sayısı hedefte, ama görev kapıları arasında uzun boşluklar var: Lv54→57 (~2.09M XP; Gözcü/Nöbetçi karışık ~55 kill), **Lv61→65** (~4.12M; ~80 Arayıcı/Bekçi kill'i), Lv71→74 (~4.65M; ~48 Gezgin), **Lv75→80** (~9.33M; ~92 kill). Lv75'ten sonra hiçbir görev XP vermiyor. Ayrıca Lv80'de verilen `Q_AbyssRegent` 350K XP'si ile Naip'in kendi 140K XP'si `GainExperience` tavanında kayboluyor. Kodda bunun bilinçli olduğu yazıyor. Hata değil, denge verisi. **Test:** Lv62'de 10 dk Bekçi kas, dakikadaki kill'i not al. ~80 kill'in kaç dakika süreceği buradan hesaplanır.
- [ ] **[KÜÇÜK, doğrulanmalı] Uçurum Naibi cebi navmesh sınırını aşıyor.** Cep `(-7600,-1200)`, r150 + 450 gezinme ile X −8200'e kadar uzanıyor; nav sınırı X −8000. Sınır dışına düşen gezinme hedefleri sessizce başarısız olur, çökme yok. Asıl risk şu: oyuncu yayla kenarında navmesh'in bittiği yerde durursa Naip ona yol bulamayabilir. Bu durumda okçu/büyücü onu karşılıksız öldürebilir. **Test:** Naibi çek, batıya (X<−8000) doğru kaç. Kovalamaya devam ediyor mu, yoksa kenarda takılıp sana vuramıyor mu? Takılıyorsa menzilli sınıfla karşılıksız hasar verilebiliyor mu, not al.
- [ ] **[KÜÇÜK, doğrulanmalı] `Scripts/ExtendWinterNavigation.py` navmesh'in bitmesini beklemeden kaydediyor.** `RebuildNavigation` asenkron, ama `save_current_level()` hemen ardından çağrılıyor. Hub kontrolü eski navmesh'le de geçeceği için zayıf; `far` noktası yalnız yazdırılıyor, kontrol edilmiyor. Commit sonrası ayrı bir probun %100 bulduğu bildiriliyor ve umap 8.98MB'tan 9.22MB'a büyümüş. Kaydedilen navmesh büyük olasılıkla tam, ama editörde gözle doğrulanmalı. **Test:** Editörde Winter_scene'i aç, `P`'ye bas. Yeşil navmesh şu bölgelerde **kesintisiz** olmalı: `(1200,-5200)`, `(-2000,-6800)`, `(4400,-5200)`, `(-5200,3600)`, `(4400,3600)`, `(-7600,-1200)` (yayla) ve hub'dan bunlara giden yollar. Build Paths sonrası "NAVMESH NEEDS TO BE REBUILT" uyarısı çıkmamalı.
- [ ] **[KÜÇÜK, bilgi] `Q_AbyssStriders` Lv71'de alınıyor ama ödülü `AbyssBoots` Lv72 istiyor.** Görev bittiğinde oyuncu neredeyse kesin hâlâ Lv71 olur: 5 kill + 260K = 670K XP, Lv71→72 için 1.49M gerekiyor. Bot çantaya gelir ama kuşanılamaz. Aynı desen `Q_Boss`, `Q_CursedKnights` ve `Q_StoneGolem`'de de var. **Test:** Lv71'de görevi teslim et; Bot çantaya düşüyor ve "seviye yetersiz" ile kuşanılamıyor olmalı, kaybolmamalı. Çanta doluyken teslimde ödül kayboluyor mu, bak.
- [ ] **[KÜÇÜK, bilgi] Lv80 kapısı.** `Q_AbyssRegent` Lv80 istiyor. Lv79 oyuncu Naibi görevi almadan öldürürse kredi alamaz ve 60 sn sonra yeniden doğmasını bekler. **Test:** Lv79'da Naibi öldür, sonra Lv80'e çıkıp görevi al. Yeni doğan Naip'i öldürünce görev tamamlanmalı.
- [ ] **[KÜÇÜK] Yukarıdaki eski maddeler güncel değil.** "Lv51–57 Frostbound" bölümündeki Spawner maddesi Nöbetçi'yi `(-3800,1200)`'de, Muhafız'ı `(-2000,1000)`'de gösteriyor. `c040e18` sonrası doğru konumlar şunlar: **Nöbetçi `(-2800,4400)`, Muhafız `(2000,2800)`**. Ayrıca 627e79a QA'sındaki iki ORTA madde (Hükümdar–Muhafız/Nöbetçi örtüşmesi ve Nöbetçi'nin nav kenarında olması) bu taşımayla kapanmış görünüyor. Yeni çiftler Nöbetçi–Gezgin 2530u (+420u pay) ve Muhafız–Koruyucu 2530u (+495u). **Test:** Winter'da Nöbetçi'yi yeni konumda bul. Kış Hükümdarı'yla 2 dk savaşırken hiçbir Lv55/57 mob'u savaşa katılmamalı.
- [ ] **Bant/elit doğrulaması (PIE):** Summer'da Yarık cebinde yalnız Arayıcı (Lv58) ya da yalnız Bekçi (Lv62) doğmalı. Winter'da 4 Kül/Uçurum cebinin her birinde tek tür mob (66/68/72/76) doğmalı, eskiden 5 olan cep başına 6 mob bulunmalı. Kül Tiranı'nı 4–6 kez öldür: yalnız Kül (Lv66) seti +3..+5 düşmeli. Uçurum Naibi'nden yalnız Uçurum (Lv72) seti +4..+6 düşmeli. Yarık Habercisi'nden yalnız Yarık (Lv58) seti düşmeli, Buzbağlı seti düşmemeli.
- [ ] **Bölge geçişiyle zincir:** Winter'da `Q_FrostboundWarden`'ı teslim et ve Lv58'e çık (`MMOGainXP`). `Q_RiftSeekers`'ı Winter hub'ında al, host düğmesiyle Summer'a geç: görev ilerlemesi korunmalı ve Arayıcı kill'leri sayılmalı. `Q_RiftHerald` bitince Lv66'da `Q_AshVanguard`'ı alıp Winter'a dön; Kül Öncüsü kill'leri sayılmalı.

# Lv61–80 grind aralığı yan görevleri — 2026-10-03

> Derleme temiz, PIE yok. 7 yeni yan görev; ana zinciri kilitlemez.

- [ ] **Görünürlük:** Lv61'de `Q_RiftWatch` teslim edilince görev NPC'sinde hem `Q_RiftHerald` (Lv65'te) hem `Q_RiftPurge` ("Yarık Temizliği") görünmeli; Purge bitince Lv63'te `Q_RiftDepths` açılmalı.
- [ ] **Uçurum yan zinciri:** `Q_AbyssStriders` bitince Lv71'de `Q_AbyssPatrol`, Lv73'te `Q_AbyssEdge`; `Q_AbyssLine` bitince Lv75'te `Q_AbyssVigil` → Lv77 `Q_AbyssDescent` → Lv78 `Q_AbyssBastion` alınabilmeli.
- [ ] **Çift sayım:** `Q_AbyssStriders` ile `Q_AbyssPatrol` aynı anda aktifken bir Gezgin kill'i ikisine de sayılıyor mu? (Beklenen: evet, zararsız.) Not al.
- [ ] **Ödül:** Her yan görev altın + 5 Büyük Can/Mana İksiri vermeli; çanta doluyken iksirler kayboluyor mu, kontrol et.
- [ ] **Kayıt uyumu:** Eski kayıtta (Lv75+, `Q_AbyssLine` tamamlanmış) yükleyince `Q_AbyssVigil` hemen alınabilir olmalı.

## QA denetimi: `f9d3b30` Rift taşıma + Naip — 2026-10-03

> **Yeniden derlenmedi.** Commit derlenmiş olarak bildirildi; bu denetimde yapılan C++ değişikliği yalnız sabit koordinatlar ve bir tamsayı.
>
> **Erişim modeli:** Rift mob'unun oyuncuyu fark etme erişimi = spawn yarıçapı + 450 (gezinme) + 600 (aggro). Bu, r225 ceplerde 1275u, Haberci'de 1200u eder. Erken cepte savaşan oyuncunun alanı = o cebin yarıçapı + 450 + 160. Summer konumları `MobAreaDistance=700` (`MMOGameMode.h:38`), PlayerStart (0,0), Facing +X, RightDir +Y varsayımıyla hesaplandı.
>
> **Sağlam bulunanlar:**
> - **Summer – Rift ile 14 erken/orta cep:** Örtüşme kalmadı. En dar üç pay şöyle:
>   - Bekçi – Alacakaranlık Hükümdarı: 3400u, **+1365u pay**
>   - Arayıcı – Magma Tiranı: 3502u, +1467u
>   - Arayıcı – Dördüncü bölge (r400): 3788u, +1503u
>
>   Haberci'ye en yakın cep Alacakaranlık cebi: 3883u, +1848u. Commit'teki "~3500 / ~3400 / ~3880u" iddiaları doğru. Önceki QA'nın ORTA bulgusu kapandı: Suikastçı–Arayıcı, Alacakaranlık–Arayıcı, Hükümdar–Bekçi ve Ork Reisi–Haberci örtüşmeleri artık yok.
> - **Summer – hub:** Rift cepleri hub'a en az 4866u uzakta. 700u güvenli yarıçapa göre pay ≥ +2891u. Hub'dan erken ceplere giden düz yollar hiçbir Rift erişimine girmiyor (en dar pay +2125u).
> - **Summer – navmesh sınırı (±8500):** Ev + gezinme mesafesi sınırdan ≥3025u içeride. 1800u leash bile sınıra ≥1675u pay bırakıyor.
> - **Winter – Naip `(6800,2000)` r150:** En yakın cep Uçurum Koruyucusu `(4400,3600)`, aradaki mesafe 2884u.
>   - Naip, Koruyucu cebindeki oyuncuya +849u pay bırakıyor. Ters yönde Koruyucu mob'u da Naip'le savaşan oyuncuya +849u pay bırakıyor; örtüşme yok.
>   - Diğer cepler ≥4294u uzakta, hub 7648u uzakta.
>   - Hub'dan Koruyucu cebine giden yol Naip'e en fazla 2884u yaklaşıyor; erişim 1200u, güvenli.
>   - Nav sınırı (X ±8000, Y −8500..5500): ev + gezinme X 7400'e kadar uzanıyor, doğu kenarına 600u pay var. Y ve Z (629, sınır ±1500) rahat.
> - **`Q_AbyssRegent` Lv79:** Ön koşul (`Q_AbyssLine`, Lv75) değişmedi. Seviye kontrolü `MMOQuestComponent.cpp:113/198`'de `Level < RequiredLevel` ile yapılıyor. `AbyssBlade` Lv72 istiyor, yani kuşanılabilir. Kodda başka `Q_AbyssRegent` referansı yok. Görev tanımı yeni yan zincirle çakışmıyor: `Q_AbyssBastion` Lv78'de ayrı bir dal. Lv79'da teslim edilen 350K görev XP'si ve Naip'in 140K kill XP'si artık işe yarıyor; ikisi birlikte Lv79→80 ihtiyacının (~2.0M) ~%24'ü. Görev açıklaması `f658894`'te "Doğu yaylası" olarak düzeltildi; Lore.md de tutarlı.
> - **`Scripts/ExtendSummerNavigation.py`:** Editör logu (`Saved/Logs/MyProject-backup-2026.10.02-21.37.12.log`) şunları doğruluyor:
>   - Eski hacim `(-490,30,30)` merkezli, ölçek (40,40,10). Yeni sınır uzanımı tam olarak (8500, 8500, 1500).
>   - `UNavigationSystemV1::Build` senkron çalışıyor: "total execution time" satırı Python kontrolünden önce basılıyor.
>   - `far` kontrolü (−6000,4000) z≈541'de navmesh buldu; kayıt yalnız bu kontrol geçince yapılıyor.
>
>   Mantık hatası bulunmadı. **Önceki QA düzeltmesi:** `ExtendWinterNavigation.py` için yazılan "RebuildNavigation asenkron" bulgusu büyük olasılıkla yanlıştı. Winter logunda da `far` noktası aynı koşuda bulunuyor; build senkron.
> - **Yukarıda artık geçersiz olan maddeler:** "Uçurum Naibi cebi navmesh sınırını aşıyor", "Lv80 kapısı" ve "Summer'daki Rift cepleri düşük seviyeli bölgelerle örtüşüyor". Bu commit üçünü de kapattı; PIE'de doğrulandığında işaretlenebilir.

- [ ] **[KÜÇÜK, doğrulanmalı] Naip doğu nav kenarında "kiting" riski.** Naip'in evi nav kenarına (X=8000) yalnız 1200u uzakta, leash ise 1800u. Oyuncu doğuya kaçıp X>8000'de navmesh dışındaki araziye çıkarsa `MoveToActor` hedefi navmesh'e iz düşüremeyebilir. Bu durumda Naip kenarda durur ve menzilli bir sınıf onu karşılıksız vurabilir. Eski batı konumundaki riskin çok daha hafif hâli. **Test:** Lv79+ menzilli karakterle (`MMOClass`) Naibi çek, doğuya (+X) koşup X≈8300'e geç. Naip seni kovalamaya devam ediyor mu, kenarda takılıyor mu? Takılıyorsa oradan büyüyle hasar verilebiliyor mu ve 1800u'dan sonra eve dönüp tam can yeniliyor mu, not al.
- [ ] **[KÜÇÜK] Eski yorum ve doküman satırları.** `MMOQuestTypes.cpp:237`'deki "Lv80'de XP kullanilmaz" yorumu artık yanlış; görev Lv79'da teslim edilebiliyor ve XP sayılıyor. `docs/PROGRESSION_PLAN.md:101` tablosu hâlâ "`Q_AbyssRegent` … / Lv80" diyor. İşlevsel etkisi yok; mmo-developer düzeltebilir.
- [ ] **[BİLGİ] Script ilk koşuda başarısız olmuş.** 21:36'daki ilk koşuda `far=None` çıktı ve harita kaydedilmedi; 21:37'deki koşu geçti. Aradaki farkın script'teki `far` sorgu uzanımının büyütülmesi olduğu düşünülüyor; commit'teki sürüm geçen sürüm. Script, varsayılan 200u fırça boyutuna güveniyor (ölçek 85 → 8500); uzanımı yazdırıyor ama assert etmiyor. `far` kontrolü yalnız iz düşümü yapıyor, hub'a yol aramıyor; commit'te ayrı probla 48/48 yol bildiriliyor. Kayıt sırasında çıkan "Revizyon kontrolünden kullanıma alınamıyor" iletisi zararsız, dosya yazıldı.
- [ ] **[BİLGİ, doğrulanmalı] Hub iz düşümü z=30.** `(0,0,300)` noktasının navmesh'e iz düşümü z=30 çıkıyor. PlayerStart gerçekten Z≈302'deyse hub'ın üstü navmesh'siz bir platform olabilir. Mob'lar hub'a girmediği için oynanışı etkilemez. **Test:** Editörde `P` ile hub'a bak; görev, satıcı ve şifacı NPC'leri ile doğuş noktası çevresinde yeşil alan olup olmadığını not al.
- [ ] **Summer navmesh (editör):** Summer_scene'i aç ve `P`'ye bas. Yeşil alan ±8500'e kadar uzanmalı. Hub'dan `(800,-4800)`, `(-4800,4800)` ve `(4800,4000)`'e (yüksek arazi, Z≈500) giden yollar kesintisiz olmalı. "NAVMESH NEEDS TO BE REBUILT" uyarısı çıkmamalı.
- [ ] **Rift cepleri (PIE):** `MMOGainXP` ile Lv58'e çık. Hub'dan yürüyerek üç cebe ulaş. Her cepte doğru mob doğmalı: 6 Arayıcı, 6 Bekçi ve 1 Haberci. Output Log'da `skipping spawn` çıkmamalı. Haberci'nin yüksek arazisine yürüyerek çıkılabilmeli.
- [ ] **Erken bölge güvenliği (yeni en dar çiftler):**
  1. Lv24 karakterle Magma Tiranı `(1400,-1350)`'yi öldür. Dövüşü güneye (−Y) doğru 1–2 dk sürdür; hiçbir Yarık Arayıcısı gelmemeli.
  2. Lv40'ta Alacakaranlık Hükümdarı `(-3200,1800)` ile savaşırken Yarık Bekçisi gelmemeli.
  3. Lv31'de Alacakaranlık cebinde `(1000,3200)` 5 dk farm yaparken Haberci gelmemeli.
- [ ] **Regresyon – Obsidyen Reisi `(-4000,0)`:** Bu elit eski nav kenarına (X −4490) ~490u mesafedeydi; artık batıya serbestçe gezinip kovalayabilir ve aggro'su 1200u. Lv30 karakterle onu batıya doğru çek. 1800u'dan sonra eve dönüp tam can yenilemeli, takılmamalı. Hub'a ya da Obsidyen cebine istenmeyen bir yaklaşma olmamalı.
- [ ] **Naip (PIE):** Winter'da Lv79'a çık. Görev NPC'sinde `Q_AbyssRegent` ("[Lv79]") görünmeli ve alınabilmeli. `(6800,2000)`'e yürüyerek ulaşabilmelisin (Z≈580 yüksek arazi). Naibi öldür; görev tamamlanmalı, teslimde 350K XP Lv79'da sayılmalı ve `AbyssBlade` kuşanılabilmeli. Ayrıca Koruyucu cebinde `(4400,3600)` 3 dk farm yaparken Naip savaşa katılmamalı.

# Seviye farkı XP ayarı + "+N EXP" göstergesi — 2026-10-03

> `31acd21` + `9f42532`. Derleme temiz, PIE yok.

- [ ] **Gösterge:** Mob öldürünce karakterin üstünde mavi "+N EXP" yazısı yükselip solmalı (host'ta ve istemcide, yalnız XP'yi alan oyuncunun ekranında). Lv80'de çıkmamalı.
- [ ] **Gri mob:** `MMOGainXP` ile Lv12 ol, Lv1–5 bölgesinde Goblin (Lv1) öldür: yazı "+N EXP (%40)" olmalı (Lv16+ ise %10 taban) ve EXP barı ona göre az dolmalı. Lv10'da Lv4 Haydut → %90.
- [ ] **Bant içi tam XP:** Lv55 karakter Lv51 Gözcü öldürünce yüzde yazmamalı (fark 4 ≤ 5).
- [ ] **Parti leech:** Lv1 karakter Lv76 partiyle Uçurum Koruyucusu (Lv76) kesimine katılırsa (3000u içinde) "+19000 EXP (%20)" civarı almalı; parti üyesi Lv70 ise tam XP.
- [ ] **Görev kredisi değişmedi:** Gri mob öldürmek görev sayacını yine artırmalı; altın düşüşü değişmemeli.

# Sohbet (genel + parti) — 2026-10-03

> Derleme temiz, PIE yok. Slate `SEditableTextBox` + Canvas kaydı; sunucu üzerinden dağıtılır.

- [ ] **Açma/gönderme:** Enter'a bas → sol altta yazı kutusu açılmalı ve imleç içinde olmalı. "merhaba" yaz, Enter → kutu kapanmalı; host'ta ve istemcide sol altta "İsim: merhaba" görünmeli.
- [ ] **Yazarken oyun tuşları:** Kutu açıkken W/A/S/D, 1–0, K, I, Z, R, Space karakteri hareket ettirmemeli / skill atmamalı / pencere açmamalı.
- [ ] **Kapatma:** Boş Enter ya da oyun alanına tıklama kutuyu kapatmalı. ⚠ Editör PIE'de Esc oturumu durdurabilir — Esc'i en son dene.
- [ ] **Parti:** `MMOInvite`/`MMOPartyAccept` ile parti kur; "/p selam" yalnız parti üyelerinde açık mavi "[Parti] İsim: selam" olarak görünmeli; partide olmayan üçüncü oyuncu görmemeli. Partisizken "/p" → turuncu "Bir partide değilsin" uyarısı.
- [ ] **Solma:** Kutu kapalıyken satırlar ~15 sn sonra solmalı; Enter ile açınca son 8 satır yarı saydam zeminde geri gelmeli.
- [ ] **Spam/uzunluk:** Arka arkaya hızlı mesajlarda 0.7 sn içindeki ikinci mesaj düşmeli; 120 karakterden uzun mesaj kesilmeli.
- [ ] **Dünya duyurusu:** Bir elit (ör. Ork Reisi) öldüğünde tüm oyuncuların sohbet kaydında turuncu "[Dunya] ... devrildi!" satırı çıkmalı.
- [ ] **Konum:** Kutu ve kayıt sol altta hotbar'ın soluna/üstüne binmemeli (dar pencerede kontrol et; gerekirse konum ayarı yapılacak).
- [ ] **Bölge geçişi:** Summer↔Winter geçişinden sonra Enter ile sohbet yine çalışmalı; açık kutu geçişte ekranda asılı kalmamalı.
- [ ] **Klan QA düzeltmeleri (sonraki commit):** (a) Lider A klan kurar, B katılır, A çıkış yapar → B "artık klan lideri" (vekil). B kaydı yazarken `bClanLeader` kayda `false` yazılmalı; A geri girince A lider olmalı, B'ye "Asıl klan lideri geri döndü" gelmeli ve B artık atamamalı. (b) A klan kurar, tüm üyeler çıkış yapar; X aynı adla `/klan kur` dener → "Bu klan adı kullanımda (çevrimdışı üyeleri var)". (c) Yanında klanlı üye varken biraz uzaktaki klansız oyuncuya `/klan davet` → klansıza gitmeli; aynı hedefe 3 sn sonra tekrar → "davet zaten gönderildi". (d) Kur/kabul/ayrıl/at sonrası oyunu kapatıp aç: klan durumu korunmalı. (e) Mob'larda ticaret/klan bileşeni replike edilmemeli (davranış değişikliği yok).
- [ ] **Fısıltı (`aa6a6aa`):** İki oyuncuyla "/w DigerOyuncuAdi merhaba": hedefte pembe "[Ad fısıldıyor] merhaba", gönderende pembe "[Hedef adlı oyuncuya] merhaba" çıkmalı; üçüncü oyuncu görmemeli. Olmayan isim → turuncu "bulunamadı". "/w" ya da "/w Ad" (mesajsız) → "Kullanım: /w Isim mesaj". Büyük/küçük harf fark etmemeli. Kendine fısıldamak "bulunamadı" demeli.
- [ ] **Yakındaki tehdit göstergesi:** Ekranın üst ortasında "Yakinda: <mob> LvN" yazısı çıkmalı (3000u içindeki en yakın canlı mob). Renk: oyuncudan >10 seviye yüksek kırmızı "[COK TEHLIKELI]", >5 turuncu "[tehlikeli]", <-5 gri "[dusuk XP]", aralarda yeşil. Elitlerde "(elit)". Ölüyken ve yakında mob yokken görünmemeli. İstemcide de (host olmayan oyuncuda) çalışmalı. Yazı sohbet/uyarı satırlarıyla üst üste binerse konumu değiştirilecek.
- [ ] **PvP kuralları (`CanHarmPlayer`):** `MMOPvP 1` açıkken iki oyuncu: (a) aynı partideki oyuncuya vurma → "Parti üyesine vuramazsın"; (b) aynı klandakine → "Klan üyesine vuramazsın"; (c) saldıran ya da hedef PlayerStart'a 700u içindeyken → "Güvenli bölgede PvP yok"; (d) hub dışında partisiz/klansız iki oyuncu birbirine vurabilmeli; (e) mob'lara vurma etkilenmemeli. Not: PvP genel anahtarı `bAllowPvP` varsayılan **açık** (test için) — yayına çıkmadan kapatılmalı ya da bölge bazlı yapılmalı.
- [ ] **Görev repliği balonu:** Görev penceresinde bir görevi "Kabul" ettiğinde alt ortada "Yasli Doran" balonu alış repliğiyle, ödülü "Teslim" ettiğinde dönüş repliğiyle çıkmalı (ör. `Q_FrostboundScouts`, `Q_AbyssDescent`). Replik metinleri `docs/Lore.md`'den `Scripts/GenQuestDialogue.py` ile üretilir (ASCII Türkçe); metni değiştirirken Lore.md'yi düzenleyip scripti yeniden çalıştır. Balon 5 satırı aşan metni "..." ile keser — çok uzun görünen replik var mı bak. Repliği olmayan görevde balon çıkmamalı.
- [ ] **/yardim ve /who:** "/yardim" (ya da "/help") komut listesini turuncu sistem satırı olarak yalnız yazana göstermeli. "/who" çevrimiçi oyuncuları "Ad (LvN)" biçiminde sıralamalı; ölü/pawn'sız oyuncu Lv0 görünür (bilinen, zararsız).

# Depocu NPC + depo penceresi — 2026-10-03

> `065cf40`. Derleme temiz, PIE yok. Depo 24 slot, yalnız ekipman (`EquipmentBag` ↔ `Warehouse`), `InstanceId` ile taşınır, Yaz ve Kış hub'ında "Depocu Rana" `(250,350)`. Kayıtla saklanır; bu yüzden Yaz↔Kış arasında ortaktır.

- [ ] **NPC görünümü:** Summer hub'ında satıcının karşısında (satıcı +200Y, depocu +350Y, şifacı −350Y) toprak renkli bir NPC, başında "Depocu Rana [depo]" yazısı olmalı; minimapte toprak rengi nokta. Winter hub'ında da aynı. NPC'lerin birbirine ya da PlayerStart'a binmediğini kontrol et.
- [ ] **Açma/menzil:** 500u içinden depocuya tıkla → ortada "DEPO" penceresi ve karşılama balonu. 500u dışından tıkla → "Depocuya yaklaş" yazısı, pencere açılmamalı.
- [ ] **Koy/al:** Cantada 2–3 ekipman varken sol sütunda birine tıkla → depo sütununa geçmeli, canta sayacı azalmalı. Depo sütunundan tıkla → çantaya dönmeli. Hızlı çift tıklamada eşya çoğalmamalı ya da kaybolmamalı (toplam sayı hep aynı).
- [ ] **Kapasite:** Çanta 12 doluyken depodan alma → "Canta dolu!" ve eşya depoda kalmalı. Depo 24'e gelince koyma → "Depo dolu!" ve eşya çantada kalmalı. (Bu mesajlar yalnız host'ta çıkıyor; istemcide mesaj çıkmaması şu an beklenen, aşağıdaki bulguya bak.)
- [ ] **Kuşanılan eşya:** Üzerindeki eşya depo listesinde görünmemeli; önce çıkarıp çantaya alınca koyulabilmeli.
- [ ] **Kalıcılık:** 3 eşya koy, `MMOSave`, oyunu kapat/aç (ya da `MMOLoad`) → depo aynı eşyalarla (nadirlik rengi ve `+N` aynı) dolu gelmeli; çantadaki/kuşanılandaki hiçbir eşya depodakiyle aynı `InstanceId`'ye düşmemeli (aynı eşya iki yerde görünmemeli).
- [ ] **Eski kayıt:** Bu commit'ten ÖNCE alınmış bir kayıtla (`Saved/SaveGames/MMOSave_*.sav`'ın yedeği) yükle → depo boş gelmeli, çanta/ekipman/altın/XP etkilenmemeli, yeni eşya düşünce `InstanceId` çakışması olmamalı.
- [ ] **Bölgeler arası ortaklık:** Summer'da koy → butonla Winter'a geç (Lv42+ gerekir, `MMOGainXP`) → Winter depocusundan aynı eşyayı al. Geri dönüp koy → Summer'da alabil.
- [ ] **Sunucu yakınlık kontrolü (çok oyunculu):** Host + istemci. İstemci depo penceresi açıkken depocudan 900u+ uzaklaşıp eşyaya tıklasın: hiçbir şey taşınmamalı (sunucu reddeder). Yakına dönünce çalışmalı.
- [ ] **İki oyuncu:** Her oyuncunun deposu ayrı olmalı; birinin koyduğu eşya diğerinin depo listesinde çıkmamalı (`OwnerOnly`). İstemci deposu host'a değil kendi ekranına yansımalı.
- [ ] **Pencere çakışması:** Envanter (`I`) açıkken depo aç: iki pencere üst üste biniyorsa tıklama hangisine gidiyor, X düğmesi çalışıyor mu not al.

## QA denetimi: XP ayarı + sohbet + depo (`31acd21`..`065cf40`) — 2026-10-03

> **Derleme:** Yeniden derlenmedi (kod değişikliği yok). `%LOCALAPPDATA%\UnrealBuildTool\Log.txt` son derlemeyi `Result: Succeeded` olarak gösteriyor; `MMOWarehouseNPC.cpp`, `MMOInventoryComponent.cpp`, `MMOGameMode.cpp`, `MMOHUD.cpp` ve `MMOPlayerController.cpp` derlenmiş, log'da uyarı/hata satırı yok.
>
> **Gizlilik:** Dört commit'te anahtar/parola/token benzeri bir şey yok.
>
> **Sağlam bulunanlar:**
> - **`ScaleXPForLevelGap` matematiği:** Sınırlar doğru. Gri (fark = oyuncu−mob): 5 → %100, 6 → %90, 13 → %20, ≥14 → %10 (float `1−0.1·9` bile `Max(0.1)` ile tam %10'a oturuyor). Leech (fark = mob−oyuncu): 10 → %100, 11 → %90, ≥18 → %20. İki dal aynı anda etkin olamaz (biri `Oyuncu−Mob>5`, diğeri `Mob−Oyuncu>10`). `Max(1, …)` alt sınırı 0 XP'yi engelliyor, `int32` taşması yok (en büyük taban 140000 × ≤1). Fonksiyon statik ve saf, ayrı test edilebilir.
> - **XP uygulama sırası:** `ClientNotifyXPGain` yalnız `Level < MaxLevel` iken çağrılıyor, `GainExperience` de aynı koşulda (`Level >= MaxLevel`, `bIsDead`, `Amount <= 0`) erken dönüyor; gösterilen sayı ile işlenen sayı aynı. Ölü alıcı atlanıyor. RPC `Client, Unreliable` ve yalnız görsel; sunucu otoritesi bozulmuyor.
> - **Görev kapıları ↔ XP ayarı (bulgu 3):** `MMOQuestTypes.cpp`'deki 59 `KillSpecificMob` görevi, `MMOMobArchetype.cpp`'deki hedef mob seviyesiyle script'le çaprazlandı. **Hiçbir görevde `ReqLevel − MobSeviyesi > 5` yok, hiçbir görevde mob oyuncudan >10 yüksek değil.** En geniş fark `Q_AbyssDescent` (Lv77, hedef Uçurum Gezgini Lv72) = tam 5. Yani görevi kabul ettiği seviyede oyuncu hâlâ %100 XP alır; ama Lv78'de %90, Lv79'da %80 olur (Lv80'de XP zaten yok). Görev kredisi ve ödül XP'si ölçeklenmediği için görev akışı bozulmuyor. Diğer yan görevler (`Q_RiftPurge` +3, `Q_AbyssBastion` +2, `Q_RiftDepths`/`Q_AbyssEdge` +1) bandın içinde. `PROGRESSION_PLAN.md`'deki örnekler (Lv40→Lv31 %60, Lv50→Lv42 %70) elle doğrulandı.
> - **Sohbet sunucu tarafı:** Gönderen adı sunucuda `PlayerState`'ten geliyor, istemci sahte ad gönderemiyor. `MMOSetName` yalnız harf/rakam/`_`/`-`/boşluk ve ≤24 karakter kabul ediyor; bu yüzden `[Parti]`/`[Dunya]` gibi ön ek taklidi mümkün değil. Sistem rengi (kanal 2) yalnız sunucudan gidiyor. 0.7 sn throttle yalnız geçerli mesajda yenileniyor; boşluk-only mesaj sayaç yakmıyor. Parti kanalı gönderenin kendisine de gidiyor (`AddUnique`).
> - **Slate yaşam döngüsü:** `SBox` varsayılan olarak `SelfHitTestInvisible`; tam ekran kaplayan kap fare tıklamalarını oyuna geçirmeye devam eder. Ac/kapa bir sonraki `PlayerTick`'te yapıldığı için commit callback'i içinde widget yok edilmiyor. `EndPlay` kutuyu kapatıyor, sunucudaki uzak `PlayerController`'larda `ChatContainer` boş olduğundan zararsız. Delegate `CreateUObject` (zayıf) olduğu için kontrolcü ölse bile çökmez. Kutu açıkken ölüm/yeniden doğma sohbeti engellemiyor (chat blok `PlayerTick`'in ölüm erken-dönüşünden önce).
> - **Depo sunucu tarafı:** Taşıma `InstanceId` ile yapılıyor (indeks kayması yok); hızlı çift RPC'de ikincisi `INDEX_NONE` ile düşüyor, eşya çoğalmıyor. Ekle-sonra-sil işlemi aynı sunucu çerçevesinde; arada başka kod araya giremiyor. Yakınlık (800u, 3B mesafe) ve kapasite sunucuda doğrulanıyor; istemci menzili (500u) sunucudan dar, tutarlı. Ticaret/yere atma sistemi olmadığı için karakterler arası kopya yolu yok. Eski kayıtta `Warehouse` alanı yoksa `UPROPERTY(SaveGame)` varsayılan boş dizi kalıyor. `NextInstanceId` depodaki en büyük kimliğin üstüne çekiliyor. `OwnerOnly` replikasyon doğru.
>
> **Bulgular (önem sırasıyla):**

- [ ] **[ORTA] Depo penceresi `IsAnyUIWindowOpen()` listesinde yok (`MMOPlayerController.cpp:397-404`).** HUD tarafı (`IsAnyWindowOpen`) depoyu biliyor ama kontrolcünün kendi listesi bilmiyor. Sonuç: depo açıkken WASD yürüyüşü, zıplama ve `bShowMouseCursor` zorlaması diğer pencerelerdeki gibi engellenmiyor; oyuncu pencere açıkken depocudan uzaklaşabilir. Pencere kapanmıyor (menzil kontrolü yok), uzaklaşınca tıklamalar sunucuda sessizce reddediliyor ve uzak istemci hiçbir mesaj görmüyor (`Notify` yalnız `IsLocallyControlledOwner()` için çıkıyor). Önerilen yön: `IsWarehouseOpen()` eklenip `IsAnyUIWindowOpen`'a katılması, HUD'da depocuya >600u uzaklaşınca otomatik kapanma ve reddedilen taşımada istemciye ayrı bir bildirim. **Test:** Depoyu aç, W ile yürü. (a) Karakter yürüyor mu? (b) 900u+ uzakta bir eşyaya tıkla: sessizce hiçbir şey mi oluyor? Diğer pencerelerle (envanter, dükkân) karşılaştır.
- [ ] **[ORTA, doğrulanmalı] Açık sohbet kutusu seamless travel'da kontrolcüde "açık" kalıp ekrandan düşebilir (`MMOPlayerController.cpp:211-262, 824`).** `ChatBox` `TSharedPtr`'ı kontrolcüde duruyor; kontrolcü `ServerTravel` ile korunursa `EndPlay` çağrılmaz (doğrulanmalı) ve viewport widget'ları dünya değişiminde temizlenirse `IsChatInputOpen()` sonsuza dek `true` kalır: WASD kilitli, Enter (`OnChatKey`, `!ChatBox.IsValid()` şartı) ölü. Benzer şekilde `AddViewportWidgetContent` içindeki `ensure(ViewportOverlayWidget)` başarısız olursa kutu hiç ekrana gelmeden "açık" sayılır. Bu, host butonla bölge değiştirirken istemcinin kutusu açıksa gerçekçi. Önerilen yön: `PlayerTick`'te `ChatBox`'ın hâlâ klavye odağında/ebeveyni olup olmadığını denetleyip yoksa `CloseChatInput`; `PostSeamlessTravel`/`ClientReset`'te kapatma. **Test:** İki oyuncu; istemcide Enter ile kutuyu aç ve boş bırak. Host bölge geçiş düğmesine bassın. Geçişten sonra istemcide: WASD çalışıyor mu, Enter kutuyu açıyor mu, ekranda asılı kalmış kutu var mı?
- [ ] **[ORTA, tasarım] Parti leech tabanı büyük farkta power-leveling'i engellemiyor.** Taban %20 ama Uçurum Koruyucusu `XPReward=95000` → 19.000 XP. Eğri Lv1'de ~100·1,25ⁿ olduğundan Lv1 karakter tek öldürmede ≈Lv18'e fırlar (1→18 toplamı ≈17,4K XP). Ayar "mob >10 seviye yüksekse düşür" niyetini gerçekleştirmiyor; yalnız küçük farklarda (Lv60'taki üyeye Lv76) anlamlı. Doküman bunu "Lv1 karakterin aldığı XP %20'ye iner" diye yazıyor, yani bilinen; ama beklenti buysa yeterli değil. Önerilen yön: alıcının `ExperienceToNextLevel`'i cinsinden mutlak tavan (ör. tek kill ≤ N × bir sonraki seviye eşiği) ya da farkla birlikte tabanın da 0'a yaklaşması; ya da parti XP'sinde minimum seviye şartı. **Test:** `MMOGainXP` ile bir hesabı Lv76 yap, ikinciyi Lv1 bırak, parti kur, Uçurum Koruyucusu cebinde kes. Lv1 üyenin kaç seviye atladığını not al.
- [ ] **[KÜÇÜK] "/p" tek başına genel kanala "/p" olarak düşüyor (`MMOPlayerController.cpp`, `ServerSendChat_Implementation`).** `TrimStartAndEnd()` sonrası `"/p "` (sonunda boşluk) ön ek kontrolü eşleşmiyor; mesaj herkese `İsim: /p` olarak gidiyor. Üstteki sohbet bölümündeki "Partisizken "/p"" adımı bu yüzden beklendiği gibi çalışmayabilir. `"/p\tmesaj"` gibi sekmeli yazımlar da genel kanala düşer. Önerilen yön: ön eki `Msg == "/p"` veya `StartsWith("/p ")` ile eşleyip boşsa kanal 1 uyarısı vermek. **Test:** Partisizken yalnız `/p` yaz+Enter, sonra `/p selam`. Birincisi herkese mi gidiyor, ikincisi uyarı mı veriyor?
- [ ] **[KÜÇÜK] `/p` yalnız pawn'a bağlı parti bileşeni üzerinden çalışıyor (`Cast<AMMOCharacter>(GetPawn())`).** Ölüm→canlanma arasında pawn yoksa partili oyuncuya yanlışlıkla "Bir partide değilsin" denir. Doğrulanmalı. **Test:** Partideyken öl, ölü iken `/p test` yaz.
- [ ] **[KÜÇÜK] `ServerSendChat`'te sınır doğrulaması yok.** `Server, Reliable` RPC uzunluğu istemciye bırakıyor; 120 sınırı string zaten kopyalanıp işlendikten sonra uygulanıyor. Zarar sınırlı (throttle var), ama kötü niyetli istemci 64KB'a yakın string'lerle sunucuya iş yükletebilir. Önerilen yön: `WithValidation` ile ör. >512 karakterde reddet/bağlantıyı kes. Test yok (kod incelemesi).
- [ ] **[KÜÇÜK] Sohbet kutusunda odak kaybı yazılan mesajı sessizce siliyor.** `OnTextCommitted` yalnız `OnEnter`'da gönderiyor; `OnUserMovedFocus` ve `OnCleared` kapatıp metni atıyor. Tasarım olabilir, ama oyun alanına yanlışlıkla tıklamak uzun mesajı kaybettirir. **Test:** Mesaj yaz, oyun alanına tıkla, Enter'la yeniden aç: metin boş mu? Alt-Tab ile başka pencereye geçip dön: kutu hâlâ açık mı, yazı duruyor mu (not al).
- [ ] **[KÜÇÜK] XP yazısı: kod yorumu ile davranış farklı.** `AddFloatingLabel` başlık yorumu "hasar sayısından biraz daha uzun kalır" diyor ama `DrawFloatingTexts` aynı `Lifetime = 1.1f`'i kullanıyor. Etiket kill anındaki dünya konumuna sabit (karakter koşarsa geride kalır) ve `Screen.X − 30` ile ortalanmıyor, uzun "+140000 EXP (%20)" sağa kayar. Kozmetik. **Test:** Mob'u öldürüp koşmaya devam et: yazı karakterle birlikte yükselmeli mi, yerinde mi kalıyor; uzun sayıda ortalı görünüyor mu?
- [ ] **[KÜÇÜK] Depoda geçersiz/bilinmeyen eşya sessizce yutuluyor.** `ApplySaveData` `IsValidInstance()` olmayanları (`InstanceId==0` ya da `ItemId` boş) log'suz atıyor; `DrawEntry` ise `FMMOItemDatabase::Find` null döndürürse satırı çizmiyor ve tıklanamaz kılıyor (eşya depoda duruyor ama görünmüyor, 24 kapasiteden yiyor). Eşya veritabanından bir kimlik silinirse oyuncu "depo dolu" der ama boş görür. Önerilen yön: `UE_LOG(Warning)` ve bilinmeyen satırı "(bilinmeyen eşya)" olarak çizmek. Test yok.
- [ ] **[KÜÇÜK] Ölü oyuncu depo kullanabiliyor.** `DepositEquipment`/`WithdrawEquipment` `bIsDead` kontrolü yapmıyor. Zararsız ama diğer sunucu eylemleriyle tutarsız. **Test:** Ölüyken (hayaletken) depocuya yaklaşıp pencereyi aç/taşı.
- [ ] **[BİLGİ] Depo adı-anahtarlı kayıtta duruyor, kimlik doğrulama yok.** Kayıt slotu oyuncu adından türüyor (`MMOSave_<ad>`); aynı adı çevrimdışıyken kullanan biri o karakterin deposunu da yükler. `MMOSetName` çevrimiçi çakışmayı engelliyor, çevrimdışı kayıt sahipliğini değil. Deponun değeri arttığı için not edildi; mevcut kayıt tasarımının doğal sonucu, bu commit'e özgü değil.
- [ ] **[BİLGİ] Mevcut checklist notu:** Üstteki gri mob adımındaki "(Lv16+ ise %10 taban)" ifadesi bir seviye geç: kod Lv1 Goblin için Lv15'te (`gap=9`) zaten %10'a iniyor. Önemsiz; test sonucunu bozmaz.

**Ek test (XP ayarı, kenar durumlar):**
- [ ] **Sınır değerleri:** `MMOGainXP` ile sırasıyla Lv6, Lv7 ol ve Lv1 Goblin kes. Lv6 → yazı "+N EXP" (yüzdesiz, fark 5), Lv7 → "(%90)" (fark 6). Lv15'te "(%10)" olmalı ve Lv16/Lv20'de aynı kalmalı (taban).
- [ ] **Parti sınırı:** Lv66 üyeyle Lv76 Koruyucu: fark 10 → yüzdesiz tam XP. Lv65 üyeyle: "(%90)". İki üye aynı öldürmede kendi seviyelerine göre farklı yüzde görmeli.
- [ ] **Lv79 → Lv80 geçişi:** Lv79'da öldürülen mob son seviyeyi atlatırsa yazı çıkmalı, Lv80'de sonraki öldürmelerde çıkmamalı; XP barı Lv80'de sıfırlanmış kalmalı.
- [ ] **Çok oyunculu görünürlük:** İstemci mob öldürdüğünde yazı yalnız XP alan oyuncuların ekranında çıkmalı; menzil dışı (3000u+) parti üyesinde çıkmamalı.
- [ ] **Q_AbyssDescent sınırı:** Lv77'de Gezgin kes (yüzdesiz), Lv78'de (%90). Görev sayacı iki durumda da aynı hızla artmalı.

# Oyuncular arası ticaret — 2026-10-03

> **Derleme:** `Result: Succeeded`. Yeni `UMMOTradeComponent` (sunucu-otoriter, durum yalnız iki tarafa replike), `AMMOCharacter`'a takıldı. Komutlar: `MMOTrade`, `MMOTradeAccept`, `MMOTradeCancel`, `MMOTradeGold <n>`, `MMOTradeConfirm`; sohbette `/trade`, `/trade kabul`, `/trade iptal`. **İki oyunculu (host + istemci) test gerekir**; tek kişiyle yalnız pencere görünmez.

- [ ] **Davet:** İki oyuncu 600u içinde. A sohbette `/trade` yazar → A'ya "ticaret daveti gönderildi", B'ye "A seninle ticaret etmek istiyor. Kabul: /trade kabul" sistem satırı. 601u+ uzakta `/trade` → "Yakında ticaret edilecek oyuncu yok". 3 sn içinde ikinci `/trade` sessizce yok sayılmalı.
- [ ] **Kabul:** B `/trade kabul` (ya da konsol `MMOTradeAccept`) → iki ekranda da TİCARET penceresi açılır (üstte karşı tarafın adı). 30 sn'den eski davet "süresi doldu" demeli; davet yokken kabul "Bekleyen ticaret daveti yok".
- [ ] **Pencere:** Açıkken WASD/zıplama/tıkla-git kilitli (diğer pencereler gibi). Sol: çanta (teklifteki eşyalar gizlenir); orta: senin teklifin; sağ: karşı tarafın teklifi.
- [ ] **Eşya teklifi:** Çantadan eşyaya tıkla → ortada belirir, karşı tarafta sağ sütunda anında görünür. Teklifteki eşyaya tıkla → çıkar. 5. eşya eklenmeye çalışılınca "En fazla 4 ekipman" mesajı. Hızlı arka arkaya tıklama (0.3 sn içinde) bazı tıklamaları yutabilir (rate limit; beklenen).
- [ ] **Altın:** `+100/+1K/+10K/+100K` düğmeleri toplar, `Sifirla` sıfırlar; cüzdandan fazlası otomatik cüzdana kırpılır (`MMOTradeGold 99999999` ile de dene). Karşı tarafta "Altın: N" güncellenir.
- [ ] **Onay sıfırlama:** A onaylar ("Sen: ONAYLADIN", B'de "Karşı taraf: ONAYLADI"). Sonra B teklifte eşya/altın değiştirir → iki onay da sıfırlanmalı. A onaylayıp B'nin değiştirmesini ve tersini dene.
- [ ] **Başarılı takas:** İki tarafta da teklif var, ikisi de ONAYLA → pencereler kapanır, "Ticaret tamamlandı." Eşyalar karşı çantaya geçmiş (Plus/nadirlik korunmuş), gönderenin çantasından silinmiş; altınlar karşılıklı aktarılmış. Çıkıştan sonra tekrar girince (kayıt) durum aynı olmalı.
- [ ] **Boş takas:** İki teklif de boşken ONAYLA → "Teklifte hiçbir şey yok", takas tamamlanmamalı.
- [ ] **Çanta dolu:** B'nin çantası 12/12, A tek eşya verip hiçbir şey almıyor → onayda "bir tarafın çantasında yer yok", takas iptal, kimse bir şey kaybetmemeli. B de bir eşya verirse (net yer açılır) takas başarılı olmalı.
- [ ] **Yeniden doğrulama (kötüye kullanım):** A eşyayı teklif edip onayladıktan sonra, B onaylamadan önce A (a) eşyayı satıcıya satsın, (b) kuşansın, (c) +1 yükseltmeyi denesin, (d) altını harcasın. B onaylayınca takas "teklif edilen eşya veya altın değişti" ile iptal olmalı; hiçbir eşya çoğalmamalı/kaybolmamalı.
- [ ] **İptal:** `IPTAL` düğmesi, `X`, `/trade iptal`, `MMOTradeCancel` — herhangi biri iki tarafta da pencereyi kapatmalı ("Ticaret iptal edildi."), hiçbir şey taşınmamalı.
- [ ] **Uzaklaşma:** Ticaret açıkken biri 800u+ uzaklaşırsa (ya da ışınlanırsa) ~0.25 sn içinde "oyuncular birbirinden uzaklaştı" ile iptal.
- [ ] **Ölüm:** Ticaret açıkken bir taraf ölürse iptal ("bir taraf öldü"). Ölü oyuncu `/trade` ile davet edememeli, ölü hedef davet alamamalı.
- [ ] **Bağlantı / bölge:** Ticaret açıkken istemci oyundan çıkarsa ya da host bölge değiştirirse (Summer↔Winter) diğer tarafta pencere kapanmalı, takılı kalmamalı.
- [ ] **Meşgul:** A–B ticarette iken C, A'ya `/trade` atsın → "Oyuncu şu an başka bir ticarette". C'nin `/trade kabul` ile ticarete sızamaması gerekir. Ticaretteyken `/trade` atan taraf "Zaten bir ticaretin içindesin" görmeli.
- [ ] **Gizlilik (replikasyon):** Üçüncü bir oyuncu (C) iki tarafın teklifini görememeli (pencere yok); teklif durumu yalnız A ve B'de.
- [ ] **Çakışan InstanceId:** İki karakterde de aynı InstanceId'li eşya olacak şekilde (ör. ikisi de yeni karakter) takas et; alıcıdaki mevcut eşya silinmemeli/üzerine yazılmamalı, gelen eşya yeni kimlik almalı. Gelen eşyayı kuşan/sat/yükselt sorunsuz çalışmalı.

## QA denetimi: ticaret (`6186506`) — 2026-10-03

> **Kapsam:** `MMOTradeComponent.cpp/.h`, `MMOInventoryComponent` (`TakeEquipmentByInstanceId`/`ReceiveTradedEquipment`), `MMOHUD` `DrawTrade`/`HandleTradeClick`, `MMOPlayerController` `/trade`. Yalnız kod okuma; PIE'de çalıştırılmadı. **Derleme:** son UBT günlüğü (17:43) `Result: Succeeded`, uyarı yok; QA ayrıca derlemedi (başka oturum `MMOHUD.cpp`/`.h`'de `DrawNearbyThreat` üzerinde çalışıyor, çakışma riski) — ticaret dosyaları için temiz derleme doğrulanmalı.
>
> **Sağlam bulunanlar:** (1) Çift teklif: `ServerAddItem` aynı InstanceId'yi reddediyor + `TradeSideStillValid` `Seen` ile tekrar kontrol ediyor. (2) Takas anında eşya/Plus/Rarity/altın/çanta yeri/altın taşması (int64) yeniden doğrulanıyor; kuşan/sat/depoya koy/yükselt/yak/altın harca sonrası takas iptal oluyor, çoğalma yok. (3) Çift onay yarışı: Server RPC'ler sıralı işleniyor, takas sonrası `bTradeActive=false` olduğundan ikinci `Confirm` boşa düşüyor. (4) Alıcı eşyayı yeni InstanceId ile alıyor. (5) İptal yolları (ölüm, 800u, `EndPlay`, kopma, ortak durum eşleşmesi `GetValidPartnerComp`) tutarlı; HUD kilidi `IPTAL`/`X` ile her zaman çıkışlı. (6) Replikasyon `COND_OwnerOnly`; istemci yalnız kendi bileşenine RPC atabiliyor, tüm girdiler (negatif altın, kötü InstanceId) sunucuda kırpılıyor/reddediliyor. (7) Koda gömülü sır yok.

### Bulgular (ciddiyet sırasıyla)

- **[YÜKSEK] Onay, görülen teklife bağlı değil (gecikme/yanıp-sönme dolandırıcılığı)** — `MMOTradeComponent.cpp:398-420` (`ServerConfirm`), `MMOHUD.cpp` `HandleTradeClick` (`RequestConfirm()` parametresiz). B ekranda değerli eşyayı görüp ONAYLA'ya basarken A teklifi değiştirirse (eşyayı çıkar / çöp ekle → onaylar sıfırlanır), B'nin gecikmeli `Confirm` RPC'si YENİ teklife onay sayılır; A hemen kendi `Confirm`'ini gönderir ve takas B'nin hiç görmediği teklifle tamamlanır. A'nın "eşya çıkar/ekle + Confirm" döngüsünü ≤0.3 sn aralıkla tekrarlaması durumu sömürülebilir kılar (`Confirm`'de hız sınırı da yok). Öneri: `FMMOTradeOffer`/bileşene replike bir `OfferRevision` (her `OnMyOfferChanged`'de iki tarafa da artsın); `ServerConfirm(int32 SeenRevision)` revizyon uyuşmazsa reddetsin; HUD gördüğü revizyonu göndersin. Ek olarak: son teklif değişikliğinden sonra ~1-2 sn "onay kilidi" (sunucuda `LastAnyChangeTime`, HUD'da pasif ONAYLA).
- **[ORTA] Alma başarısızlığı eşya yutabilir** — `MMOTradeComponent.cpp:609-610` `ReceiveTradedEquipment` dönüş değerini yok sayıyor; `MMOInventoryComponent.cpp:722` `FMMOItemDatabase::Find(ItemId)` yoksa false döner, ama `TradeSideStillValid`/`TakeEquipmentByInstanceId` veritabanı varlığını kontrol etmiyor → veritabanından kalkmış/eski kayıtlı bir eşya teklif edilirse çantadan çıkar, karşıya girmez (yok olur). Öneri: `TradeSideStillValid`'e `FMMOItemDatabase::Find` ekle; ayrıca takas öncesi iki `EquipmentBag`/`NextInstanceId` kopyasını al, herhangi bir `Receive` false dönerse ikisini de geri yaz ve iptal et (gerçek atomiklik).
- **[ORTA-DÜŞÜK] İki ayrı kayıt, yarım durum penceresi** — `MMOTradeComponent.cpp:620-621`: `A->SaveCharacter(); B->SaveCharacter();` sıralı ve dönüş değerleri yok sayılıyor. İlk dosya yazıldıktan sonra sunucu çökerse / ikinci yazma diskte başarısız olursa bir taraf eski durumda kalır → eşya ÇOĞALIR (gönderen eski kayıtta) ya da KAYBOLUR. Pencere milisaniyelik ama RMT/dupe için hedef. Öneri: dönüş değerlerini kontrol et, `UE_LOG(Error)` + kısa gecikmeyle yeniden dene; mümkünse kayıtlardan önce "bekleyen takas" işareti yaz. Kısa vadede en azından başarısızlığı günlüğe düş.
- **[ORTA-DÜŞÜK] Davet "son gelen kazanır" — davet kapma** — `MMOTradeComponent.cpp:249` `TC->PendingInviteFrom = Self` mevcut geçerli daveti ezer; `ServerAccept` kimin daveti olduğunu belirtmez. B, A'nın davetini kabul etmek üzereyken yakındaki C sessizce ezip araya girebilir; B `/trade kabul` yazınca C ile ticaret açılır (pencere başlığı adı gösteriyor, dikkatsiz oyuncu fark etmeyebilir). Öneri: geçerli (30 sn) bir bekleyen davet varsa başkasının davetini reddet ya da `/trade kabul <isim>` iste; kabulde "X ile ticaret başlıyor" onayı.
- **[DÜŞÜK] Hız sınırı paylaşımlı ve sessiz** — `OfferChangeCooldown=0.3` Add/Remove/SetGold'u tek sayaçta topluyor ve tıklamayı geri bildirimsiz yutuyor; `+1K` düğmesine hızlı basış ya da iki eşyaya hızlı tık kaybolur (HUD `MyOffer.Gold` bayat değerden hesaplıyor). Güvenlik sorunu değil, UX. Öneri: HUD'da kısa yerel bekleme/bildirim, ya da sunucuda sıra tutma.
- **[DÜŞÜK] Bildirim spam'i** — `Confirm` için hız sınırı yok; teklif değiştir (0.3 sn) + `Confirm` döngüsüyle karşıya her turda "X teklifi onayladı" sohbet satırı basılabilir. `ServerInviteNearest` yalnız davet eden başına 3 sn sınırlı; birden çok oyuncu aynı kişiye davet yağdırabilir, engelleme/reddetme yok. Öneri: `Confirm` cooldown'ı, hedef başına davet kısıtı, `/trade red`.
- **[DÜŞÜK] Boşta kalan ticaretin zaman aşımı yok** — Karşı taraf AFK ise pencere sonsuza dek açık kalır; `IsAnyUIWindowOpen` yüzünden diğer oyuncunun tıkla-git/zıplama/kamera kilitli (WASD ayrıca doğrulanmalı), yalnız `IPTAL`/`X`/`/trade iptal` ile çıkabilir. Takılma değil ama sinir bozucu. Öneri: sunucuda son etkileşimden ~120 sn sonra otomatik iptal; ESC ile iptal.
- **[DÜŞÜK] Mob'lar da `TradeComponent` taşıyor** — `AMMOCharacter` mob tabanı olduğundan her mob'da replike bileşen oluşuyor (gereksiz ağ/bellek). Öneri: mob'larda `SetIsReplicated(false)` (BeginPlay'de `!IsPlayerControlled()` ise) ya da bileşeni yalnız oyuncu pawn'ına ekle.
- **[DÜŞÜK / doğrulanmalı] `NextInstanceId` yalnız depo için yükseltiliyor** — `MMOInventoryComponent.cpp:~763-773` `ApplySaveData` çanta/kuşanılanlardaki en büyük kimliğe göre `NextInstanceId`'yi yükseltmiyor. Bozuk/eski kayıtta `ReceiveTradedEquipment`'ın verdiği yeni kimlik çantadaki bir eşyayla çakışabilir (kimlikle arama ilk eşleşmeyi bulur). Önceden var olan risk; ticaret kimlikle çalıştığı için etkisi arttı. Öneri: yüklemede çanta+kuşanılan+depo için `Max(Id)+1` uygula.
- **[DÜŞÜK] Denetim izi yok** — Tamamlanan takas hiçbir yere günlüklenmiyor. Öneri: `UE_LOG(LogTemp, Log, ...)` ile iki oyuncu adı, eşyalar (ItemId/Plus/Rarity), altın.
- **[BİLGİ] Küçük** — `/yardim` metni `/trade`'i listelemiyor (`MMOPlayerController.cpp:~149`). Ticaretteyken diğer pencerelerden (envanter/dükkan/depo) yapılan işlem takası sürpriz iptal ediyor (güvenli ama sessiz); istenirse `IsTrading()` ile Equip/Sell/Upgrade/Deposit engellenip mesaj verilebilir.

### Test adımları (iki oyunculu: host + istemci)

- [ ] **Onay yarışı / yanıp-sönme (YÜKSEK bulgu):** A değerli eşya teklif edip ONAYLA'ya bassın. A tarafında, B ONAYLA'ya basmadan hemen önce eşyayı teklifinden çıkarıp "çöp" eşya eklesin ve tekrar ONAYLA'ya bassın (ardışık). B'nin ekranı eski teklifi gösterirken B ONAYLA'ya basarsa takas B'nin görmediği teklifle tamamlanıyorsa hata doğrulanır; düzeltmeden sonra "teklif değişti, tekrar onayla" gibi reddedilmeli. Konsolla hızlandır: A'da `MMOTradeConfirm` ve HUD tıklamasını art arda; B'ye ağ gecikmesi için `Net PktLag=200` (konsol) uygula.
- [ ] **Veritabanında olmayan eşya:** (Mümkünse kayıt dosyasında bir `ItemId`'yi geçersiz yap ya da `FMMOItemDatabase`'den kaldır.) O eşyayı teklif et, iki taraf onaylasın; eşya yok olmamalı, takas iptal olmalı ve gönderenin çantasında kalmalı. (Şu an yutuyor olabilir — beklenen hata.)
- [ ] **Kayıt tutarlılığı:** Başarılı takastan hemen sonra (≤1 sn) host oyunu `taskkill` ile kapat; yeniden açıp iki karakteri de yükle: eşya iki karakterde birden olmamalı / ikisinde de olmamalı. (Pencere dar; birkaç kez dene, çoğalma ya da kayıp görürsen kaydet.)
- [ ] **Davet kapma:** A, B'ye `/trade` atsın; 2 sn sonra yakındaki C de B'ye `/trade` atsın; B `/trade kabul` yazsın. Pencere başlığı hangi oyuncuyu gösteriyor? (A bekleniyor; C çıkıyorsa kapma doğrulanır.)
- [ ] **Bildirim spam'i:** A teklifte altını `+100` / `Sifirla` ile 0.3 sn aralıkla değiştirip her seferinde ONAYLA'ya bassın; B'nin sohbetinde "A teklifi onayladı" satırları yağıyor mu bak (hız sınırı yoksa evet).
- [ ] **Hızlı tıklama:** `+1K` düğmesine 5 kez hızlı bas; altın 5000 olmalı mı yoksa 1000-2000 mi? (0.3 sn sınırı yüzünden tıklamalar yutulur; kabul edilen/etmeyeceğine karar ver.)
- [ ] **AFK ticaret:** A ticaret penceresini açık bıraksın, B hareketsiz kalsın: A 2-3 dk sonra hâlâ kilitli mi? A'da WASD ile yürüyebiliyor mu, tıkla-git/zıplama/kamera kilitli mi? `IPTAL`/`X` her durumda çıkış veriyor mu?
- [ ] **Dolu çanta + takas yönü:** Her iki çanta 12/12. Her iki taraf 1 eşya teklif etsin → takas başarılı olmalı (net 0). Bir taraf 2, diğeri 1 eşya teklif etsin → 12'den fazla alan taraf için iptal ve iki tarafta da kayıp yok.
- [ ] **Altın taşması:** A'nın altınını ~2.100.000.000 yap (özel komut yok: `MMOSave_<ad>.sav` kaydını düzenle ya da `AddGold` çağıran bir test hook'u gerekir; yoksa bu adımı atla); B 100.000.000 altın teklif etsin, A hiçbir şey vermesin → "altın sınırı aşılır" ile iptal, B altınını kaybetmemeli.
- [ ] **Teklif sonrası depoya koy / kuşan / yükselt / sat:** Her biri için teklifli eşyayla dene (depocu ve satıcı yakınında). Her durumda ONAYLA sonrası takas iptal olmalı; eşya depoda/kuşanılı/satılmış hâlde TEK kopya kalmalı (çoğalma yok). Ayrıca eşyayı kuşanıp hemen çıkarırsan (teklifte kalmış) takas geçerli kalmalı ve eşya doğru çantaya geçmeli.
- [ ] **Davet spam'i:** Üç oyuncu aynı hedefe 3 sn aralıkla `/trade` atsın; hedefin sohbeti taşıyor mu, hedef tek `/trade kabul` ile hangi daveti kabul ediyor?
- [ ] **Bölge geçişi sırasında ticaret:** Ticaret açıkken host `MMOTravel` (bölge değişimi) yapsın; yeni haritada iki oyuncuda da pencere kapalı, eşya/altın sayıları takas öncesi duruma eşit (hiçbiri kaybolmamış) olmalı.
- [ ] **İstemci kopması takas anında:** İstemci ONAYLA'ya bastığı anda oyunu kapatsın (alt+F4); host'ta eşya ya tamamen geçmiş ya da hiç geçmemiş olmalı, host ve yeniden giren istemcinin kayıtları tutarlı olmalı.
- [ ] **QA düzeltmeleri (`f08ed47`, `d8a9782`) – onay revizyonu:** A ve B ticarette; B "ONAYLA"ya basar. B'nin onayı sunucuya ulaşmadan önce A teklifi değiştirir (ör. altın +1). Beklenen: B'nin onayı "Teklif değişti; yeni teklifi görüp tekrar onayla" ile reddedilir, takas TAMAMLANMAZ. Ardından A teklifi değiştirdikten sonra ilk 1 sn içinde onay → "bir saniye bekleyip tekrar onayla"; 1 sn sonra onay kabul edilmeli.
- [ ] **Davet kapma:** A, C'ye davet atar; C kabul etmeden B de C'ye `/trade` atar → B "Oyuncunun bekleyen başka bir daveti var" almalı, C'nin `/trade kabul`'u A ile ticareti başlatmalı. 30 sn sonra davet düşer, B yeniden davet edebilmeli.
- [ ] **Boşta zaman aşımı:** Ticaret penceresi açık, iki taraf da 3 dk hiçbir şey yapmasın → "uzun süredir işlem yapılmadı" ile iki tarafta kapanmalı.
- [ ] **Eşya teslim hatası geri alma:** (Doğrudan test edilemez; kod yolu.) Takas sırasında çanta kapasitesi/veritabanı hatası beklenmez; görülürse iki çantanın takas öncesi hâlinde kaldığı doğrulanmalı.
- [ ] **NextInstanceId:** Eski kayıtla giriş yapıp ticaretle yeni eşya al; çantada aynı InstanceId'yi taşıyan iki eşya olmamalı (yükseltme/satma tek eşyayı etkilemeli).

# Klan sistemi — 2026-10-03

Yeni: `UMMOClanComponent` (klan adı herkese, lider bayrağı sahibine replike), sohbet komutları `/klan ...` ve `/g mesaj`, isim plakasında yeşil `[KLAN]`, kayıtta `ClanName` / `bClanLeader`. Prototip kuralları: klan = üyelerin ortak adı (ayrı klan nesnesi yok); uyelik karakter kaydında durur; çakışma kontrolü yalnız ÇEVRİMİÇİ klan adlarına bakar. İki oyunculu (host + istemci) test et; liderlik/çakışma adımları için 3. oyuncu (ya da yeniden giriş) gerekir.

### Test adımları

- [ ] **Kur:** A `/klan kur Demir` → "Klan kuruldu: [Demir]". A'nın başında isim plakası YOK (kendini çizmiyoruz); B ekranında A'nın isim satırının üstünde yeşil `[Demir]` görünmeli. Geçersiz adlar: `/klan kur ab` (2 harf), `/klan kur AbcdefghijklM` (13), `/klan kur De mir` (boşluklu), `/klan kur De-mir` (tire) → hepsi reddedilmeli. Aynı anda B `/klan kur demir` (küçük harf) → "Bu klan adı kullanımda".
- [ ] **Davet / kabul:** A (lider), B'ye 600 birimden yakın dururken `/klan davet` → B'de "seni [Demir] klanına davet etti", A'da "davet gönderildi". B `/klan kabul` → iki tarafta "B klana katıldı", B'nin plakası `[Demir]`. B'nin `/klan davet` yazması "Yalnız klan lideri davet edebilir" vermeli.
- [ ] **Davet güvenlik:** (a) A 3 sn içinde ikinci `/klan davet` → "Çok hızlı davet". (b) Zaten klandaki C'ye davet → "Oyuncu zaten bir klanda". (c) Klansız D'ye A davet etsin, D kabul etmeden başka bir klanın lideri E de D'ye davet atsın → E "bekleyen başka bir daveti var" almalı; D `/klan kabul` A'nın klanına girmeli. (d) 30 sn sonra `/klan kabul` → "süresi doldu". (e) 600 birimden uzaktaki oyuncuya davet → "Yakında davet edilecek oyuncu yok". (f) Davet eden klandan ayrıldıktan sonra kabul → "Davet geçersiz".
- [ ] **Klan sohbeti:** A, B klanda; C klansız. B `/g selam` → A ve B ekranında yeşil `[Klan] B: selam`; C hiçbir şey görmemeli (genel sohbet de değil). C `/g x` → "Bir klanda değilsin". Başka klandaki F klan mesajını görmemeli. Yalnız `/g` yazmak genel sohbete sızmamalı.
- [ ] **Liste:** `/klan liste` → "[Demir] çevrimiçi üyeler (2): A (Lv?) [lider], B (Lv?)" — seviyeler doğru, katılma sırasına göre dizili.
- [ ] **At:** B (lider değil) `/klan at A` → "Yalnız klan lideri üye atabilir". A `/klan at B` → B "klanından atıldın", B'nin plakasında `[Demir]` kalkmalı; kendini atmaya çalışmak (`/klan at A`) "bulunamadı" demeli; çevrimdışı/olmayan isim → "bulunamadı".
- [ ] **Ayrıl + liderlik devri:** A (lider), B, C klanda (katılma sırası B, sonra C). A `/klan ayril` → B'de "B artık klan lideri" (B en eski çevrimiçi üye); `/klan davet` B'de artık çalışmalı. Son kalan üye de ayrılınca klan çevrimiçi kalmaz; yeni biri aynı adla `/klan kur` yapabilmeli.
- [ ] **Ölü oyuncu:** Ölüyken `/klan kur|davet|kabul|ayril|at|liste` (liste hariç tüm değiştirenler) "Olu iken klan komutu kullanilamaz" demeli; durum değişmemeli. Ölüyken `/g` yazılabiliyor (bilinçli; `/p` ile aynı) — beklenmedik değilse sorun değil.
- [ ] **Kalıcılık:** A klan kurup `MMOSave` / oyundan çık, yeniden gir → plaka `[Demir]` geri gelmeli, A hâlâ lider (`/klan davet` çalışmalı). Eski (klansız) kayıtla giriş → klansız, hata yok. B (üye) çıkıp girince aynı klanda olmalı, lider değil (A çevrimiçiyken).
- [ ] **Liderlik çakışması (bilinçli basit kural, doğrula):** A (lider) oyundan çıksın → B'ye liderlik geçmeli (B'de "artık klan lideri"). A yeniden girsin → kayıtlı lider A liderliği geri alır, B üyeye döner (aynı anda iki lider olmamalı; A'da "artık klan lideri" mesajı gelmez, `/klan davet` çalışır). Klanın tüm üyeleri çıkıp yalnız bir üye (B) tekrar girerse B lider olur (liderlik boş kalmaz). Bölge geçişinde de yükleme sırası fark etmemeli (asıl lider her zaman lider çıkar).
- [ ] **Bilinen sınırlar (hata değil):** Çevrimdışı bir üye atılamaz; klan çevrimdışıyken aynı ad başkası tarafından kurulabilir (çakışma kontrolü yalnız çevrimiçi). Kimse çevrimiçi değilken klan yalnızca karakter kayıtlarında yaşar. `/yardim` yeni komutları listelemeli.
- [ ] **Bölge geçişi:** Klanlı iki oyuncu `MMOTravel` ile bölge değiştirsin → yeni haritada plaka/klan sohbeti çalışmalı; liderlik tek kişide kalmalı.

## QA denetimi: klan (`b5a66a6`) — 2026-10-03

Kapsam: `MMOClanComponent.cpp/.h`, `ServerSendChat` klan/`/g` dalları, `ClientReceiveChat` kanal 5, HUD isim plakası, kayıt alanları. Derleme: editör kapalıyken `Build.bat MyProjectEditor Win64 Development` → **Result: Succeeded**, derleme günlüğünde uyarı yok. Sunucu otoritesi sağlam: `ClanName` / `bIsLeader` yalnız `Cmd*` ve `ApplySaveData` içinde, `HasAuthorityInternal()` kapısı arkasında yazılıyor; klan için ayrı istemci RPC'si yok, tek giriş `ServerSendChat` (zaten sunucuda, 0,7 sn hız sınırı var). `bIsLeader` `COND_OwnerOnly`, `ClanName` herkese. `/p`, `/w`, `/who`, `/trade` sırası ve erken dönüşleri bozulmamış; `/g` ve `/klan` bu komutlardan sonra/önce çakışmasız. Bölge geçişi: kayıt `SetTravelSaveCommitted` ile kilitli, `EndPlay` liderlik devri kayda yazılmıyor, yeni haritada kayıttan liderlik doğru geliyor.

### Bulgular (önem sırasıyla — kod değişikliği YAPILMADI, düzeltmeyi mmo-developer yapar)

**ORTA-1: Kayıtlı `bClanLeader` çevrimiçi lideri sessizce düşürüyor ("son giren lider olur").** `MMOClanComponent.cpp` `ApplySaveData` (satır ~537-556): `In.bClanLeader` doğruysa çevrimiçi lider `CC->bIsLeader=false` yapılıyor. Vekil olarak terfi eden B oturumda otomatik kayıt yaptıysa B'nin kaydında da `bClanLeader=true` kalır; asıl lider A geri dönüp lider olduktan SONRA B giriş yaparsa B liderliği A'dan alır, A'ya hiç bildirim gitmez (yalnız `MarkChanged`). B bundan sonra `/klan at A` ile kurucuyu atabilir. Yukarıdaki "asıl lider her zaman lider çıkar" varsayımı yalnız A ikinci giriyorsa doğrudur. Öneri: vekil terfisini kalıcı yazma (ayrı çalışma-zamanı `bActingLeader` bayrağı; `FillSaveData` yalnız asıl lideri `true` yazsın) ya da kayıtlı lider olsa bile çevrimiçi bir asıl lider varsa önüne geçme; düşürülen kişiye `NotifyOwner` ver.
- [ ] **Test (ORTA-1):** A kur, B davet+kabul. A oyundan çıksın (B'ye vekil terfi, B `MMOSave` yapsın). Sonra A girsin (lider), A çıkmadan B oyundan çıkıp YENİDEN girsin. Beklenen: A hâlâ lider kalmalı ve `/klan davet` çalışmalı. Hata: B yeniden girince `/klan at A` işe yarıyorsa bulgu doğrulanır.

**ORTA-2: Çevrimdışı klan adı kapma yalnız "kurulabilir" değil, mevcut üyeleri devralıyor.** `ApplySaveData` kayıtlı klan adına sahip birini, o adı çevrimiçi taşıyan herkesin klanına davetsiz katıyor (`GatherOnlineMembers` + `bIsLeader = In.bClanLeader || !bOtherLeader`). Yani lider+üyeler çevrimdışıyken X aynı adla `/klan kur Demir` yaparsa, eski üye Y girdiğinde otomatik X'in klanına üye olur (X artık Y'yi atabilir); eski lider A girerse hem liderliği X'ten alır hem X'in toplayıp kattığı herkes A'nın olur, X'e bildirim yok. Checklist'teki "Bilinen sınırlar" yalnız "ad kurulabilir" diyor, bu sonucu yazmıyor. Öneri (en az iş): `CmdCreate`'te kayıtlı klan adlarını da kontrol et (yalnız-ad kaydı: `SaveGame` dizinindeki `ClanName` değerlerini tarayan ya da küçük bir `UMMOClanRegistry` kayıt dosyası) ya da en azından `ApplySaveData`'da "çevrimiçi aynı adlı klan var ve kayıtlı lider yok ise otomatik katma, klansız bırak + bildirim" kuralı. Tasarım notu olarak ayrıca checklist'e yazıldı (aşağıda).

**ORTA-3: `/klan davet` hedefi "en yakın oyuncu" ve kilit kolayca kullanılabiliyor.** `CmdInvite` (satır ~262-297): en yakın canlı oyuncuyu seçiyor, o zaten klandaysa "Oyuncu zaten bir klanda" deyip bitiyor — yani klan arkadaşı yanında dururken 600 birim içindeki klansız oyuncuya davet GİTMEZ. Ayrıca aynı lider aynı hedefe her 3 sn'de davet yenileyince `TC->PendingInviteTime = Now` tazelenir ve "ezilmez" koruması hedefi diğer klan liderlerine süresiz kapatır (griefing). Öneri: hedef adayını önce `!IsInClan()` ile süz (ya da `/klan davet <isim>` ekle); aynı davet edenin yenilemesi süreyi uzatmasın (`PendingInviteTime` yalnız hedefte bekleyen davet yoksa/sürmüşse yazılsın).
- [ ] **Test (ORTA-3):** A (lider), B (zaten klanda) yanında, C (klansız) biraz daha uzakta (<600u). A `/klan davet` → C'ye davet gitmeli (sorun varsa "Oyuncu zaten bir klanda" görülür). Sonra A, C'ye 3 sn arayla davet tekrarlasın; 30 sn geçmeden başka bir klan lideri E, C'ye davet atmayı denesin → E'nin davetinin bloke olup olmadığına bak (süresiz bloke = bulgu).

**DÜŞÜK-1: Ad doğrulaması `FChar::IsAlnum` — Unicode harfleri kabul edebilir (doğrulanmalı).** `IsValidClanName` satır 88-102. `iswalpha` tabanlı olduğu için Kiril/Yunan/Latin-1 harfleri de geçebilir; "Dеmir" (Kiril `е`) ile "Demir" gözle aynı ama farklı ad → taklit/aldatma. Türkçe `ı/İ` için `Equals(IgnoreCase)` `ı`↔`I` eşlemez ve `İ`'nin davranışı platforma bağlı: "KIRMIZI" ile "kırmızı" farklı klan sayılabilir (beklenen/istenen davranışı kullanıcı belirlesin). HUD fontunda Türkçe/Kiril glif eksikse kare görünebilir. Öneri: izin listesi (`A-Z a-z 0-9` + `çğıöşüÇĞİÖŞÜ`), eşsizlik anahtarını Türkçe farkındalıklı katla (`İ→i, I→ı` değil; basit: ASCII'ye indir ve karşılaştır).
- [ ] **Test (DÜŞÜK-1):** `/klan kur Çelik`, `/klan kur Işık`, `/klan kur Dеmir` (Kiril `е`) dene → hangileri kabul ediliyor not et; kabul edilenlerin isim plakasında doğru çizildiğini (kare/boş değil) doğrula. Biri `/klan kur ışık`, diğeri `/klan kur IŞIK` ile çakışıp çakışmadığını gör.

**DÜŞÜK-2: `ApplySaveData` zaten klandaki karakterde (manuel `MMOLoad`) eski klanı liderli bırakabilir.** Satır ~516-557: oyuncu klandayken `MMOLoad` ile kayıtta farklı/boş klan gelirse eski klan için `EnsureLeader` çağrılmıyor → eski klanın çevrimiçi üyeleri liderin gittiği klanda liderli kalmaz; ayrıca klana "ayrıldı" bildirimi gitmez. `ServerLoadCharacter` yalnız yerel kontrolcü için çalışıyor (host hata ayıklama), bu yüzden düşük. Öneri: `ApplySaveData` başında mevcut klan farklıysa `CmdLeave` benzeri temizlik (`EnsureLeader(OldName, true)` + bildirim).

**DÜŞÜK-3: `ClientReceiveChat` `Sender.IsEmpty()` ise kanal 5'i bile turuncu sistem satırı gibi çiziyor.** `MMOPlayerController.cpp` ~satır 360: `Channel == 2 || Sender.IsEmpty()`. Oyuncu adı boşsa (`ServerSetName` boşu reddediyor; ama URL `?Name=` ile giren oyuncu için `PlayerState` adı doğrulanmalı) klan/genel mesajı sistem duyurusu gibi görünür. Sistem mesajı taklidi. Öneri: koşulu `Channel == 2` yap; boş gönderen adını `?` ile değiştir. (Klan öncesinden var olan davranış, klan kanalı da etkileniyor.)

**DÜŞÜK-4: Moblar da `UMMOClanComponent` taşıyor ve replike ediyor.** `AMMOMob : AMMOCharacter` (`MMOCharacter.cpp` ctor `CreateDefaultSubobject`) — her mob için ek replike alt nesne; davranışsal sorun yok, hafif bant/CPU maliyeti. Öneri: `AMMOMob` yapıcısında `ClanComponent->SetIsReplicated(false)` ya da bileşeni yalnız oyuncuya kur.

**DÜŞÜK-5: Üye sınırı yok, kick/leave kayda anında yazılmıyor, lider devri komutu yok.** `CmdAccept`'te üye üst sınırı yok; sunucu çökerse atılan/ayrılan oyuncu bir sonraki otomatik kayda kadar eski klanında kalır (doğrulanmalı: `AutoSaveTimerHandle` süresi); çevrimdışı üye atılamaz ve lider isteyerek devredemez (`/klan lider <isim>` yok). Ağ yükü açısından kritik değil. Öneri: `CmdLeave/CmdKick/CmdAccept` sonunda `SaveCharacter()` çağır; üye üst sınırı (örn. 30) ekle.

**DÜŞÜK-6: Uzun metin yankısı.** `ServerSendChat` `Msg`'i `/klan` dalına gelmeden kırpmıyor; `/klan at <çok uzun isim>` istemcinin kendi ekranına 'bulunamadı' yankısı olarak döner (yalnız kendine, `/w` ile aynı desen). Öneri: `Msg = Msg.Left(200)` komut dallarından önce.

### Doğrulanan sağlam noktalar (sessiz kalınmasın)
- İstemci kendini lider yapamaz: `ClanName/bIsLeader` hiçbir `UFUNCTION(Server)` ile yazılmıyor; `Cmd*` doğrudan çağrılmıyor, `GetClanComponent()` yalnız sunucudaki pawn için `ServerSendChat`'ten çağrılıyor. Başkası adına davet/kabul/at mümkün değil: `Self` her zaman `GetPawn()`'ın klan bileşeni.
- Davet: 600u menzil, 3 sn hız sınırı, 30 sn geçerlilik, tüketim her kabul denemesinde (`CmdAccept` başında), davet eden hâlâ aynı klanda mı kontrolü, ölü/kendine/zaten klandaki hedefler eleniyor; `PendingInviteFrom` `TWeakObjectPtr`.
- Ad doğrulaması: boşluk, tire, 2 ve 13 karakter reddediliyor; çevrimiçi "Ab"/"ab" çakışması `IgnoreCase` ile yakalanıyor; klan adı yalnız harf/rakam olduğundan sohbet satırı enjeksiyonu (`[Parti]` taklidi) klan adından mümkün değil; `/g` metni satır başına konamıyor (sabit `[Klan] Ad:` öneki, `\r\n` zaten temizleniyor, 120 karakter).
- `/g` yalnız aynı klan adındaki (`IgnoreCase`) çevrimiçi oyunculara gidiyor; klansızda "Bir klanda degilsin", boş `/g` erken `return` (genel sohbete sızmaz, hız sınırı tüketmez); kanal 5 yeşil `[Klan] Ad: mesaj`.
- Liderlik: `EndPlay` kaydı yazıldıktan sonra devri tetikliyor (`AMMOCharacter::EndPlay` kaydı önce yapıyor, bileşen EndPlay'i `Super::EndPlay` içinde); `bIsTearingDown` korumalı; kendini atma "bulunamadı"; lider olmayan davet/at "Yalnız klan lideri"; son üye ayrılınca `EnsureLeader` aday bulamayıp sessiz dönüyor (çökmez).
- Kayıt: `FillSaveData` yalnız klandaysa lider yazıyor; eski kayıtta boş → klansız; geçersiz kayıtlı ad klansız sayılıyor.
- İsim plakası: `ClanName` herkese replike; kendi plakası çizilmiyor (bilinçli); mob atlanıyor.

### Ek manuel test adımları
- [ ] **Sunucu otoritesi:** Uzak istemcide konsoldan `Say /klan kur Hack` yazılırsa yalnız o istemcinin kendi klanı kurulur; başka bir oyuncunun adına işlem yapılamamalı (başkasının adını kullanan `/klan at X` yalnız kendi klanındakini etkiler).
- [ ] **Çevrimdışı ad kapma (ORTA-2 doğrulama):** A klan kurup `MMOSave`+çıkış; B (başka karakter) aynı adla `/klan kur` yapıp C'yi davet etsin. A yeniden girince: A'nın liderliği X'ten alıp almadığına, C'nin A'nın klanında kalıp kalmadığına, B'ye bildirim gidip gitmediğine bak ve not et. Beklenen (düzeltme sonrası): çakışma engellenir ya da kimse sessizce devralınmaz.
- [ ] **`/yardim` ve gerileme:** `/yardim`, `/who`, `/p mesaj`, `/w isim mesaj`, `/trade`, `/trade iptal` hâlâ eskisi gibi çalışmalı; `/klanlar` ya da `/gelmek` gibi komut olmayan metinler genel sohbete düz metin olarak gitmeli.
- [ ] **Hız sınırı:** Sohbet kutusundan hızlıca art arda `/klan liste` (0,7 sn'den kısa) → fazla olanlar sessizce düşmeli, çökme/yığılma olmamalı.

# Arkadaş listesi — 2026-10-03

Aşağıdakiler derleme sonrası PIE/listen-server (en az 2 oyuncu) ile denenecek:

- [ ] **Ekleme:** A, B çevrimiçiyken `/arkadas ekle B` (büyük/küçük harf farkı olsa da, örn. `/arkadas ekle b`) → "B arkadas listene eklendi." Aynı komut tekrar → "zaten arkadas listende"; `/arkadas ekle A` (kendi adı) → reddedilmeli; `/arkadas ekle YokBoyleBiri` → "cevrimici oyuncu bulunamadi"; boş isim / 24+ karakter → kullanım / "cok uzun" uyarısı.
- [ ] **Liste:** `/arkadas` ve `/arkadas liste` aynı çıktıyı vermeli: B çevrimiçiyken "B - cevrimici (LvN)", B çıkınca "B - cevrimdisi". Boş listede "Arkadas listen bos".
- [ ] **Silme:** `/arkadas sil b` (küçük harf) → silinmeli; listede olmayan ad → "arkadas listende yok".
- [ ] **Tek yönlülük:** A, B'yi eklediğinde B'ye hiçbir bildirim gitmemeli ve B'nin listesi değişmemeli.
- [ ] **Durum bildirimi:** A listesinde B varken B oyuna girince A'ya "B cevrimici oldu." (kanal 2 sistem satırı), B oyundan çıkınca (pencereyi kapat / bağlantıyı kes) "B cevrimdisi oldu." gelmeli. Listesinde B olmayan üçüncü oyuncuya bildirim gitmemeli. Not: bölge geçişinde (travel) çevrimdışı satırı atlanıyor, geçiş sonrası "cevrimici oldu" satırı gelebilir; bunu not et.
- [ ] **Kalıcılık:** Arkadaş ekle, `MMOSave`, oyundan çık, tekrar gir → `/arkadas` aynı isimleri göstermeli. Eski kaydı olan (Friends alanı olmayan) karakterle giriş → liste boş, çökme yok.
- [ ] **Sınırlar:** 30 arkadaştan sonra ekleme → "Arkadas listesi dolu (en fazla 30)". (Çok oyuncu gerektirdiğinden istenirse sadece kod okuyarak doğrula.)
- [ ] **Sahibine replikasyon:** İstemci oyuncuda `/arkadas ekle` çalışmalı (sunucuya gider), başka oyuncunun listesi görünmemeli.
- [ ] **Gerileme:** `/yardim` metninde `/arkadas` görünmeli; `/klan`, `/g`, `/p`, `/w`, `/who`, `/trade` aynen çalışmalı; `/arkadaslar` gibi komut olmayan metin genel sohbete düz metin gitmeli; hızlı art arda komut (0,7 sn) sessizce düşmeli.

## QA denetimi: arkadaş listesi (`9e8b8d5`) — 2026-10-03

Kapsam: `MMOFriendsComponent.cpp/.h`, `ServerSendChat` `/arkadas` dalı, `AMMOCharacter::AutoLoadDeferred / EndPlay / SaveCharacterForTravel / LoadCharacter`, `MMOMob.cpp` replikasyon satırı, `FMMOCharacterSaveData::Friends`. Derleme: `UnrealBuildTool/Log.txt` → `[1/11] Compile MMOFriendsComponent.cpp` ... **Result: Succeeded**, günlükte uyarı yok (DLL 18:26:45, kaynak 18:25:52: derleme güncel). Kod değişikliği YAPILMADI; düzeltmeyi mmo-developer yapar.

### Bulgular (önem sırasıyla)

**ORTA-1: Her bölge geçişinde (`MMOTravel`) arkadaşlara sahte "çevrimici oldu" bildirimi gidiyor.** `MMOCharacter.cpp` `AutoLoadDeferred` her yeni pawn'da (seamless travel sonrası yeni pawn `PossessedBy` → 0,6 sn → `AnnouncePresence(true)`) çalışıyor; `EndPlay` ise `bTravelSaveCommitted` yüzünden çevrimdışı satırını bilerek atlıyor. Sonuç: geçişte hiç "çevrimdışı" gelmeden herkes "X cevrimici oldu." alıyor; üstelik tüm oyuncular birlikte taşındığı için arkadaş çiftlerinin hepsi (zaten yan yanayken) bu satırı alıyor. Alıcının `Friends` listesi kendi `AutoLoadDeferred`'ında (0,6 sn) yüklendiği için sonuç sıraya bağlı: 0-2 satır, tutarsız. Commit mesajı/checklist bunu "not et" diyerek bilinen davranış sayıyor ama kullanıcıya görünen bir hata. Öneri: "şu an duyurulmuş çevrimiçi adlar" kümesini `UGameInstance` (sunucuda seamless travel'dan sağ çıkar) ya da `PlayerState::CopyProperties` ile taşınan bir bayrakta tut; `AutoLoadDeferred`'da ad kümede varsa duyurma, yoksa ekle+duyur; gerçek çıkışta kümeden çıkar+çevrimdışı duyur. Daha sağlamı: gerçek çıkış sinyali olarak pawn `EndPlay(Destroyed)` yerine `AMMOGameMode::Logout` (travel'da tetiklenmez) kullanmak; böylece `bTravelSaveCommitted` sezgisine ve travel sırasında kopan oyuncunun çevrimdışı mesajının kaybolmasına gerek kalmaz.
- [ ] **Test (ORTA-1):** A ve B karşılıklı olarak birbirini listesine eklesin. İkisi de çevrimiçiyken `MMOTravel` (Summer → Winter). Beklenen: hiçbir "cevrimici oldu" satırı gelmemeli. Hata: geçişten sonra 1-2 "X cevrimici oldu." geliyorsa bulgu doğrulanır. Üç kez tekrarla (sıra rastgele).

**ORTA-2: Çevrimiçi/çevrimdışı duyurusu giriş anındaki oyuncu adına bağlı; `MMOSetName` akışında yanlış/eksik çalışıyor.** Misafir oyuncu `PlayerN` gibi varsayılan adla girer, `AutoLoadDeferred` o adla "çevrimici" duyurur (kimsenin listesinde yoktur) ve 0,6 sn'de yalnız `MMOSave_PlayerN` yuvasından liste yükler. Oyuncu sonra `MMOSetName Ali` yapınca (`ServerSetName_Implementation`) hiçbir duyuru/yeniden yükleme olmaz: "Ali" izleyenler hiç "Ali cevrimici oldu" görmez; ama `EndPlay`'de PlayerState adı artık "Ali" olduğu için "Ali cevrimdisi oldu" gider (öncesinde çevrimici yok). Eski ad "PlayerN" izleyicilerde kalır. Ayrıca listeler hep ada bağlı olduğundan `PlayerN` biçimli (oturumdan oturuma değişen) adlar arkadaş olarak kaydedilirse sonraki oturumda başka bir kişiyi gösterir. Öneri: duyuruyu "kimlik oturdu" olayına bağla (ad değişimi + başarılı `LoadCharacter` sonrası), `ServerSetName`'de eski ad için çevrimdışı/yeni ad için çevrimici duyur (ORTA-1'deki küme ile tekilleştir); `CmdAdd`'de `PlayerN` biçimli adı uyarıyla reddet ya da "önce MMOSetName kullan" mesajı ver.
- [ ] **Test (ORTA-2):** A `MMOSetName Ali`, B `MMOSetName Bora` yapsın; A `/arkadas ekle Bora`. B `MMOSetName Bora2` yazsın. Beklenen: A'ya "Bora cevrimdisi oldu"; `/arkadas` → "Bora - cevrimdisi". Hata: Bora hâlâ çevrimici görünüyorsa ya da A'ya hiç mesaj gitmiyorsa bulgu doğrulanır. Not: B yeniden `MMOSetName Bora` ile eski adına dönerse A'ya "Bora cevrimici oldu" gelip gelmediğine de bak (gelmemesi ORTA-2'yi doğrular).
- [ ] **Test (ORTA-2b):** Misafir olarak (ad vermeden) giren C'yi A listesine ekle (`/arkadas ekle Player3`), ikisi de çıkıp yeniden girsin: aynı adın başka birine ait olup olmadığına bak ve not et.

**ORTA-3: Rıza/engelleme yok + ad sahipliği yok → izlenme ve sahte "arkadaş çevrimici" mümkün.** (a) `CmdAdd` tek yönlü ve karşı tarafa bildirim göndermiyor; herhangi bir oyuncu başkasını sessizce listesine ekleyip o kişinin tüm giriş/çıkış zamanlarını anlık görebilir; hedefin reddetme/gizlenme/engelleme yolu yok (`/who` zaten çevrimiçileri gösteriyor, yeni olan giriş-çıkış zamanlama izleme). (b) Kimlik = görünen ad: `ServerSetName_Implementation` yalnız ÇEVRİMİÇİ oyuncularla yuva çakışmasını denetliyor (`MMOCharacter.cpp` ~660-675); B çevrimdışıyken X `MMOSetName B` yaparsa `/arkadas liste` ve `FindOnlineByName` X'i "B - cevrimici (LvN)" gösterir ve X çıkınca B'yi izleyenlere "B cevrimdisi oldu" gider (sahte durum). Klan denetimindeki "çevrimdışı ad kapma" ile aynı kök neden (kayıt sahipliği doğrulanmıyor), bu commit'e özgü değil ama arkadaş listesi bunu görünür bir güvenlik yüzeyine çeviriyor. Öneri: kısa vadede `/arkadas gizli` (duyuruları kapat) bayrağı + kaydı `FMMOCharacterSaveData`'ya ekle; ya da karşılıklı onay (davet/kabul) akışı; uzun vadede gerçek kimlik/hesap kimliği (GUID) ile eşleştir.
- [ ] **Test (ORTA-3):** B'yi A listesine ekle. B oyundan çıksın. C `MMOSetName B` yapsın (B çevrimdışıyken). A `/arkadas` → "B - cevrimici" görünüyorsa sahte durum doğrulanır; C çıkınca A'ya "B cevrimdisi oldu" gelirse not et. Ayrıca B'ye (izlenen) hiçbir bildirim/engelleme yolu olmadığını gör.

**ORTA-4: Her `/arkadas ekle|sil` senkron tam karakter kaydı yazıyor; yalnız 0,7 sn sınırı var.** `CmdAdd`/`CmdRemove` sonunda `Me->SaveCharacter()` → `SaveGameToSlot` (disk). Uzak istemcinin manuel kaydı `RemoteManualSaveCooldown = 10 sn` ile sınırlanmışken bu yol onu aşıyor: bir istemci `ekle X` / `sil X` dönüşümlü gönderip ~1,4 yazma/sn üretebilir (çok oyunculu sunucuda disk G/Ç ve donma). Not: klan komutları (`SaveCharacter()` ×4) aynı kalıbı kullanıyor. Öneri: değişiklikte yalnız `bFriendsDirty` işaretle (otomatik kayıt 60 sn zaten yazıyor) ya da sunucu tarafında "son komut-kaydı < 5 sn ise atla" koruması ekle; ek olarak `SaveCharacter()`'ın false dönüşünü (yükleme bitmedi/kayıt kapalı) kullanıcıya bildir ("eklendi" ama kalıcı değil).
- [ ] **Test (ORTA-4):** Konsolda/sohbette `/arkadas ekle B` ve `/arkadas sil B`'yi 0,7 sn aralıkla 30 kez dön; `Saved/SaveGames/MMOSave_<ad>.sav` yazma zamanına ve karelere bak (takılma var mı). Beklenen (düzeltme sonrası): dakikada en fazla birkaç yazma.

**DÜŞÜK-1: Bildirim sönümlemesi yok; giriş penceresinde (0,6 sn) kayıp/yetim bildirim ve kaybolan komut.** (i) Aynı ad için art arda gir/çık (flapping) her seferinde tüm izleyicilere satır gönderir; debounce yok. (ii) `EndPlay` duyurusu `bInitialLoadComplete`/çevrimici-duyurulmuş mu kontrol etmiyor: possess'ten sonra 0,6 sn dolmadan kopan oyuncu için hiç "çevrimici" gitmeden "çevrimdisi" gider. (iii) Yeni giren oyuncunun `Friends` listesi 0,6 sn'ye kadar boş: bu sürede başka biri girerse yeni gelen onu duymaz; ayrıca bu pencerede `/arkadas ekle X` "eklendi" der ama `bInitialLoadComplete=false` olduğu için `SaveCharacter()` reddeder ve sonra `ApplySaveData` `Friends.Reset()` ile eklemeyi siler. (iv) Giren oyuncuya çevrimiçi arkadaşlarının özeti verilmiyor (yalnız `/arkadas` ile). Öneri: ad başına en az 10-30 sn debounce (aynı yönde mesajı bastır, çevrimici→çevrimdışı→çevrimici kısa dalgalanmayı birleştir); yetim çevrimdışı için duyuruldu bayrağı; `CmdAdd/Remove` yükleme bitmeden "birkaç saniye sonra dene" desin.
- [ ] **Test (DÜŞÜK-1):** B pencereyi 5 kez art arda aç-kapat (A'nın listesinde B var): A'ya kaç satır geldiğini say (beklenen düzeltme sonrası: en fazla birkaç). Ayrıca B'yi başlatır başlatmaz (ilk 1 sn) kapat: A'ya "B cevrimdisi" gelip gelmediğine bak (çevrimici gelmemişse yetim). B yeni girerken ilk saniyede `/arkadas ekle A` yaz → listede kalıp kalmadığına bak.

**DÜŞÜK-2: `Friends` replike ediliyor ama istemcide tüketen yok (OnRep/HUD yok).** `DOREPLIFETIME_CONDITION(..., COND_OwnerOnly)` doğru ve sızıntı yok (başkası görmez), ama kullanılmayan bant/yüzey. `Friends` ayrıca `BlueprintReadOnly`. Öneri: HUD gelene kadar `Replicated`'ı kaldır (ya da `OnRep_Friends` + `BlueprintAssignable` delegesi ekle). Mob satırı (`SetIsReplicated(false)`) derleniyor, Trade/Clan ile aynı kalıp, yan etki yok.

**DÜŞÜK-3: `CmdList` her arkadaş için ayrı güvenilir (Reliable) `ClientReceiveChat`.** 30 arkadaşla 31 RPC/komut (0,7 sn'de bir → ~44/sn); yalnız komutu gönderen istemciyi etkiler (yavaş istemcide kendi güvenilir tamponunu taşırıp bağlantısını düşürebilir). Öneri: tek mesajda birleştir (`, ` ile) ya da en çok ~8 satır/RPC.
- [ ] **Test (DÜŞÜK-3):** 30 ad kaydı olan bir karakterde (kaydı elle doldurmadan test edilemezse kod okumasıyla yetin) `/arkadas` art arda yaz; sohbet sırasının bozulup bozulmadığına bak.

**DÜŞÜK-4: `?Name=` ile giren oyuncuların adı doğrulanmıyor → sistem satırı (turuncu) sahteciliği ve eşleşmeyen adlar.** Bildirimler "{ad} cevrimici oldu." biçiminde kanal 2'de (sistem rengi) çiziliyor; ad `ServerSetName`'in karakter filtresinden geçmediyse (URL `?Name=` ≤20 karakter) satır sistem duyurusu gibi okunabilir ("Sunucu kapanacak -" gibi). Sondaki/baştaki boşluklu bir ad `CmdAdd`'de `N` kırpıldığı için hiç eşleşmez ve kayıt yükleyicisi de kırpar (hiçbir zaman eklenemez). `/who` da aynı adı basıyor (öncesinden var). Öneri: oyuncu adını girişte (`PreLogin/InitNewPlayer`) `ServerSetName` kuralıyla normalize et; bildirimlerde adı `[ ]` ya da farklı renkle ayır.

**DÜŞÜK-5: Küçükler.** (i) `CmdAdd` dolu listede zaten-ekli adı "dolu" diye reddeder ("zaten listende" demez) — sıra değişsin. (ii) `ApplySaveData` kendi adını/karakter kümesini filtrelemiyor; kayıt elle bozulursa kendi adı listede kalabilir (zararsız). (iii) `/arkadas liste foo` fazla argümanı sessizce yutar. (iv) Listede olmayan ada "silmek" 24 karaktere kırpılmış yankı veriyor, sorun yok. (v) Ölü pawn (`bIsDead`) komut ve bildirimi alıyor — istenen davranış olabilir, bilgi.

### Doğrulanan sağlam noktalar (sessiz kalınmasın)
- **Sunucu otoritesi / başkasının listesi:** `Cmd*` ve `AnnouncePresence` `UFUNCTION` değil (BP/RPC ile çağrılamaz), tek giriş `ServerSendChat_Implementation` (sunucuda) ve `Me = Cast<AMMOCharacter>(GetPawn())` yalnız çağıranın kendi pawn'ı; istemci başka birinin listesini değiştiremez. `Friends` yazımı yalnız `Cmd*`/`ApplySaveData` içinde ve `HasAuthorityInternal()` kapısı arkasında. Replikasyon `COND_OwnerOnly` → başka oyuncunun listesi sızmıyor.
- **Komut sıralaması:** `/arkadas` yalnız tam eşleşme ya da `"/arkadas "` öneki; `/arkadasX`, `/a`, `/arkadaslar` düz metin kalıyor. Dal `/klan` sonrası, `/g`, `/p `, `/w `, `/f ` öncesinde ve hiçbirini gölgelemiyor (`/f` ile `/arkadas` farklı önek). Boşluklu adlar (`Ali Veli`) `Split` ilk boşluktan böldüğü için doğru geliyor. Hız sınırı aynı kapıdan.
- **EndPlay güvenliği:** `HasAuthority && IsPlayerControlled` kapısı; `GetWorld()` null, `bIsTearingDown`, boş ad, `GetPawn()`/`Controller` null kontrolleri var; yalnız `Destroyed` + `!bTravelSaveCommitted` duyuruluyor → host kapanışı (`Quit`/`EndPlayInEditor`) ve travel'da çökme yok, kendine bildirim yok (`Other != Me`).
- **Kayıt:** `Friends` `SaveGame` alanı, eski kayıtta boş dizi; `ApplySaveData` boş/çok uzun/yinelenen adları eler, 30'da keser; `bInitialLoadComplete` kapısı yükleme bitmeden boş listeyle kaydı ezmeyi önlüyor; yükleme başarısız (`bValid=false`) ise liste uygulanmıyor ve kayıt kapalı kalıyor. Büyük/küçük harf `IgnoreCase`, kayıtta gerçek (canonical) PlayerState adı tutuluyor.
- **Mob:** `GetFriendsComponent()->SetIsReplicated(false)` derlendi (include eklenmiş), `AutoLoadDeferred`/`EndPlay` duyuruları `IsPlayerControlled()` kapılı olduğundan mob etkilenmiyor; ölüm/canlanma pawn'ı yok etmediği (`RespawnAtStart` → `Revive`) için respawn'da sahte "çevrimici" yok.

### Ek manuel test adımları
- [ ] **Sunucu otoritesi:** Uzak istemcide konsoldan `Say /arkadas ekle <host adı>` yaz: yalnız o istemcinin listesine yazılmalı; host'un `/arkadas` çıktısı değişmemeli. Hosttan `/arkadas` → istemcinin listesi görünmemeli.
- [ ] **Gerçek bağlantı kopması:** İstemci penceresini görev yöneticisinden sonlandır (zarif çıkış değil): izleyen oyuncuya "X cevrimdisi oldu." gelmeli ve kayıt dosyası güncellenmeli (EndPlay sırasında PlayerState/Controller bağlı mı doğrulanmalı; gelmiyorsa pawn EndPlay yerine `GameMode::Logout` kullanılmalı).
- [ ] **Host kapanışı:** Host PIE'yi durdururken çökme/uyarı (`Accessed None`) olmamalı; çıkış günlüğünde `AnnouncePresence` ile ilgili hata olmamalı.
- [ ] **Eski kayıt ve bozuk kayıt:** `Friends` alanı olmayan eski bir `.sav` ile giriş → liste boş, çökme yok. (İsteğe bağlı) aynı adı iki kez ya da boş adı içeren elle üretilmiş kayıt → yükleyici elemeli.
- [ ] **Boşluklu ad ve büyük/küçük harf:** `MMOSetName Ali Veli` olan oyuncu `/arkadas ekle ali veli` ile eklenebilmeli; liste çıktısı gerçek yazımı (`Ali Veli`) göstermeli.
- [ ] **Gerileme (komutlar):** `/arkadasX`, `/arkadaslar`, `/a` genel sohbete düz metin gitmeli; `/arkadas` sonrası aynı 0,7 sn içinde yazılan `/who` sessizce düşmeli (sınır ortak), sonrasında `/who`, `/p`, `/w`, `/g`, `/klan liste`, `/trade` normal çalışmalı.

## QA denetimi: PvP kuralları + görev repliği + spawner refactor — 2026-10-03

Kapsam: `4adf1a5` + `360d815` (`AMMOCharacter::CanHarmPlayer`, `MMOTargeting::IsInsideHubSafeZone`), `25684ae` + `639fee9` (`Scripts/GenQuestDialogue.py`, `MMOQuestDialogue.cpp`, `MMOHUD::HandleQuestClick`), `19ba3c9` (`AMMOMobSpawner::FindSafeSpawnLocation`, `MMOProbeSpawn`/`MMOProbeSurface`, iki Winter cebi). Derleme: `Build.bat MyProjectEditor` -> "Target is up to date / Result: Succeeded" (DLL 18:59:51, kaynak değişikliklerinden sonra). Uyarı görmek için ayrıca `MMOCharacter.cpp`, `MMOQuestDialogue.cpp`, `MMOMobSpawner.cpp`, `MMOGameMode.cpp`, `MMOTargeting.cpp` `-SingleFile` ile PCH'siz derlendi: hata ve uyarı yok. Yalnız `MMOHUD.cpp` PCH'siz derlemede `GEngine` (satır 1158/1160) için C2065 veriyor (aşağıda DÜŞÜK-6).

### Bulgular (önem sırasıyla)

**ORTA-1: Dost-ateşi/hub koruması yalnız `AttackTarget` içinde; hasarın geçtiği merkezi kapı (`AMMOCharacter::TakeDamage`) PvP'den habersiz.** Bugün oyuncuya oyuncu hasarı veren tek yol `AttackTarget` -> `ApplyDamage` (doğrulandı: `MMOSkillComponent::FindNearestEnemy` yalnız `AMMOMob` döndürüyor, AoE yalnız `TActorIterator<AMMOMob>` gezer, oyuncu DoT'u yalnız mobun `ActiveEffects`'ine yazılır, `MMOBuff/MMODamage` yalnız kendine). Yani bugün atlanan yol yok. Ama `MMOSkillComponent.cpp:~976` yorumu "PvP şimdilik kapalı" diyor ve `bAllowPvP` varsayılanı **true**; ilk PvP'ye açılan skill/AoE/DoT `CanHarmPlayer`'ı çağırmayı unutursa parti/klan/hub koruması sessizce kalkar. Öneri: `TakeDamage` başında "saldıran = `DamageCauser` bir `AMMOCharacter` ve oyuncu kontrollü, hedef = bu ve oyuncu kontrollü, saldıran != bu" ise `IsPvPAllowed() && CanHarmPlayer()` zorunlu kıl (kendine hasar/mob hasarı serbest); `AttackTarget`'taki kontrol kullanıcı geri bildirimi için kalsın.
- [ ] **Test (ORTA-1, kod okuma + oyun):** PvP açıkken (`MMOPvP 1`) iki oyuncu aynı partide, Savaşçı/Okçu/Büyücü AoE ve zehirli ok skill'lerini (hotbar 1-9) partidaşın hemen yanında kullan: partidaşın canı **düşmemeli**, yalnız mob hasar almalı. Hub'da (PlayerStart 700u içinde) duran oyuncunun yakınında AoE kullan: oyuncu hasar almamalı.

**ORTA-2: Reddedilme sebebi uzak istemciye hiç gitmiyor; parti/klan/hub engelinde istemci sessizce "vuramıyor".** `AttackTarget` sunucu dalında geri bildirim `bLocalFeedback = IsLocallyControlled()` ile veriliyor; uzak istemcinin pawn'ı sunucuda local değil, istemci tarafı ise RPC'yi gönderip dönüyor (`ServerAttackTarget`). Sonuç: "Parti uyesine vuramazsin / Klan uyesine vuramazsin / Guvenli bolgede PvP yok / PvP kapali" yalnız **host**'ta görünür (ölçüm: kod yolu, doğrulanmalı). Öneri: sebebi kısa bir `Client` RPC'siyle (sebep enum'u/kısa metin) sahibine gönder; spam için ~1 sn sınır.
- [ ] **Test (ORTA-2):** Host + 1 uzak istemci aynı partide. **İstemci**, hosta tıklayıp saldırsın. Beklenen: "Parti uyesine vuramazsin" yazısı istemcinin ekranında çıkmalı; hostun canı düşmemeli. Hata: istemcide hiçbir yazı yoksa bulgu doğrulanır. Aynı testi klan ve hub (host PlayerStart yakınında dururken) için tekrarla.

**ORTA-3: Görev balonu kabul/teslim *başarısız* olsa da çıkıyor.** `HandleQuestClick` (`MMOHUD.cpp:2492-2511`) `AcceptQuest/ClaimReward` dönüş değeri olmadan, tıklar tıklamaz replik gösteriyor (istemcide bunlar sunucuya RPC; sonuç belli değil). Gerçek senaryolar: (a) ekipman ödüllü görevi teslim ederken **ekipman çantası dolu** -> sunucu `ClientClaimRewardFailed("Ekipman cantasi dolu...")` der, ama balon "Al su Kilic'i..." diyerek ödül verilmiş gibi konuşur; (b) görev penceresi açıkken NPC'den >MaxDist uzaklaşıp tıklama -> "Gorevli yakininda degilsin", balon yine çıkar (pencere uzaklaşınca kapanmıyor); (c) 0,25 sn RPC sınırına takılan çift tık -> ikinci balon/boşa. Öneri: balonu `OnRep`/`ClientNotify` ile başarı onayında göster (ör. `ActiveQuests`'e eklendi / `CompletedQuests`'e eklendi değişimini HUD'da yakala) ya da en azından ekipman çantası dolu/uzak durumunu istemcide ön kontrol et.
- [ ] **Test (ORTA-3a):** Ekipman çantasını doldur (MaxBagSize), ekipman ödüllü bir görevi (ör. `Q_FrostboundScouts`, ödül bot) teslim et. Beklenen: ödül verilmemeli ve "Ekipman cantasi dolu" uyarısı; **balon çıkmamalı** (şu an çıkması bekleniyor = bulgu). Çanta boşaltıp tekrar teslim -> balon + ödül.
- [ ] **Test (ORTA-3b):** Görev penceresi açıkken NPC'den 600u+ uzaklaş (pencere açık kalır), "Kabul"a tıkla: "Gorevli yakininda degilsin" yazısı çıkar; balonun çıkıp çıkmadığına bak ve not et.

**DÜŞÜK-1: "Oyuncu" tespiti `IsPlayerControlled()` — kontrolcüsü o an olmayan oyuncu pawn'ı mob gibi muamele görür.** `AttackTarget` ve `CanHarmPlayer` hedefin oyuncu olduğuna yalnız `Victim->IsPlayerControlled()` ile karar veriyor; seamless travel/bağlantı kopması/yeniden sahiplenme anında controller null iken `IsPvPAllowed`, parti, klan ve hub kontrolleri tümden atlanır (doğrulanmalı; pencere çok kısa). Öneri: kalıcı bir işaret (`GetPlayerState()` != null veya `AMMOMob` olmamak: `!Cast<AMMOMob>(Victim)`).

**DÜŞÜK-2: Replik kapsamı beklenenden dar; commit mesajındaki "otomatik" iddiası yanlış.** Üretilen tabloda 61 görev var (Lore'daki `*(Q_...)*` sayısıyla birebir, yinelenen/eşleşmeyen kimlik yok, tüm kimlikler `MMOQuestTypes.cpp`'de var, çıktıda ASCII dışı karakter ve `\"` yok). Eksikler: (1) `Q_Level5/20/30/40/50/65/80` ve `Q_Gold500/5000/15000/30000/50000` (12 görev) repliksiz; bunlar da **tıkla-teslim** (otomatik değil), üstelik `docs/NpcDialogue.md`'de Q_Level30/40/80 için Doran repliği zaten yazılı ama oyuna bağlı değil; (2) `Q_SpiderQueen`, `Q_StoneGolem`, `Q_ShadowAssassin`, `Q_Warlocks` için yalnız alış var, **teslim repliği yok** (Lore'da "Dönüşte" satırı eksik). Öneri: metin yazarı bu 4 görevin dönüşünü ve milestone görevlerini eklesin; `GenQuestDialogue.py` Lore'da alışı olup dönüşü boş görevleri uyarı olarak yazdırsın.
- [ ] **Test (DÜŞÜK-2):** `Q_Level5`'i (5. seviyede) teslim et: balon çıkmamalı (şu an bekleniyor). `Q_SpiderQueen`'i alış sonrası bitirip teslim et: alışta balon, teslimde balon yok.

**DÜŞÜK-3: Balon konuşmacısı "Yasli Doran" HUD'da sabit string.** Şimdilik `AMMOQuestGiverNPC` kurucusu da `NpcName = "Yasli Doran"` atıyor ve Summer+Winter'daki tüm görevli NPC'ler aynı sınıf (`MMOGameMode.cpp:287, 564`, Winter hub'da `SpawnActor<AMMOQuestGiverNPC>`), bu yüzden bugün tutarsızlık yok. Biri bir görevlinin `NpcName`'ini değiştirirse (ya da ikinci bir görev veren eklenirse) tıklanan NPC ile balondaki ad ayrışır. Öneri: HUD, `IsOwnerNearQuestGiver` ile bulunan en yakın `AMMOQuestGiverNPC`'nin `NpcName`'ini kullansın (ya da paylaşılan bir sabit). Ayrıca Winter görevleri (`Q_Abyss*`, `Q_Rift*`) herhangi bir görevliden (Summer hub dahil) alınabiliyor mu bak: replikler konum içeriyor ("Kuzey Yolu'ndan geri in...").

**DÜŞÜK-4: Belge/yorum tutarsızlıkları (yanlış yönlendirir).** (1) `MMOGameMode.cpp:~259` yorumu Gezgin için "64/64" diyor, commit mesajı ve `PROGRESSION_PLAN.md:145` **59/64** diyor. (2) `MMOGameMode.cpp:233` ("Nobetci (Lv55) kuzey (-2800,4400)...") ve `:271` ("Ucurum Gezgini Lv72 (-5200,3600)... tum cepler >=1800u, hub'dan >=3800u") eski konumları taşıyor. (3) `PLAYTEST_CHECKLIST.md` ~2682'deki "eski maddeler güncel değil" notu hâlâ açık. Öneri: yorumları yeni koordinatlara ve gerçek ölçüme çek.

**DÜŞÜK-5: `MMOProbeSpawn` "64/64" gerçek doğum başarısını değil, *konum geçerliliğini* ölçüyor.** (a) `SpawnOne`'daki `DontSpawnIfColliding` çarpışma testi ve cepteki diğer mobların çarpışması probda yok; (b) prob `Ignore=nullptr` kullanıyor (spawner aktörü yok sayılmıyor, `SpawnOne` `this`'i yok sayıyor); (c) nav yoksa `FindSafeSpawnLocation` hiçbir sayaç artırmadan false döner -> "0/64, trace_miss=0, nav=0" belirsiz; `son zemin Z=0.0 nz=0.00` yalnız eğim reddinde dolar, yanıltıcı olabilir; (d) `MMOProbeSurface` yukarıdan ilk yüzeyi bulur (çatı/kaya çıkıntısı olabilir); (e) iki Exec komutu `#if !UE_BUILD_SHIPPING` içinde değil. Kötüye kullanım riski düşük: Exec komutlarını yalnız host konsolundan çalıştırır (GameMode istemcide yok), `HasAuthority()` kapısı var, yalnız log yazar, 64 iz ile sınırlı; uzak istemci erişemez (mevcut debug Exec'leri `IsLocalController` ile aynı çizgide). Öneri: `#if !UE_BUILD_SHIPPING` ile sar; prob raporuna "nav yok" sayacı ekle.

**DÜŞÜK-6: `MMOHUD.cpp` include hijyeni.** `Engine/Engine.h` yok; normal (PCH'li) derlemede `GEngine` paylaşılan PCH'den geliyor, PCH'siz `-SingleFile` derlemesinde C2065 (satır 1158/1160, bu denetimin commit'lerinden önce var). Unity/PCH ayarı değişirse derleme kırılır. Öneri: `#include "Engine/Engine.h"` ekle.

### Winter cepleri (D: Nöbetçi `(-3500,3000)`, I: Gezgin `(-5500,4500)`) ölçümü
Merkez mesafeleri (hesaplandı): D-Kış Hükümdarı `(-3000,0)` 3041u; D-Lv42-48 cebi `(-3000,-1500)` 4528u; D-Gözcü `(3000,0)` 7159u; D-Muhafız `(2000,2800)` 5504u; D-hub `(0,-1500)` 5701u; I-D 2500u; I-Hükümdar 5148u; I-hub 8139u; kalan tüm cepler >=7690u. Rift erişim modeli (spawn yarıçapı 225 + gezinme 450 + aggro 600 = 1275u; oyuncu duruş erişimi 835u -> en az 2110u) **tüm çiftlerde sağlanıyor** (en dar çift: I-D 2500u, marj 390u). Navmesh sınırı marjı (spawn yarıçapı + 450 gezinme): D: X 3825 / Y(üst) 1825u; **I: X 1825 / Y(üst) 325u** -> Gezgin için kuzey kenar (Y=5500) çok dar; gezinme ve ızgara probu Y 5175'e kadar nav olduğunu *doğrulamalı* (eski Naib/`(-7600,-1200)` hatasının aynı türü). Hub'dan I'ya düz hat D cebine ~460u, Kış Hükümdarı'na ~1625u yaklaşıyor (Hükümdar erişimi 1200u): oyuncu gerçek navmesh yolunda Hükümdar'ı çekmemeli (doğrulanmalı).
- [ ] **Test (Winter cepleri):** `powershell -File Scripts/SmokeTest.ps1 -Map Winter_scene -Cmds "MMOProbeSurface -5500 5100 225,MMOProbeSurface -5500 4500 225,quit"` (editör kapalı): `[MMOPROBE]` satırlarında `ok` değeri >=56/64 olmalı; Y=5100'de belirgin düşüş varsa Gezgin cebini Y<=4100'e çek. PIE'de Winter: hub'dan D cebine yürü; Kış Hükümdarı çekilmemeli. D cebinde 6 Lv55 mob doğmalı (her seferinde); I cebinde 6 Lv72 mob (kısa süre bekle, kalan her dolum). D'de dövüşürken I'daki Lv72 mobların çekilmediğini ve tersini doğrula. Mobları cepte gezinirken nav kenarına çıkıp takılmadığını izle.

### Doğrulanan sağlam noktalar (sessiz kalınmasın)
- **`CanHarmPlayer` mantığı:** null/mob hedefte `true` (mob etkilenmez); parti kıyası saldıranın sunucu-otoriter `Members` listesinde `TObjectPtr` karşılaştırması (kendisi hariç tutulan liste, `ApplyPartySet` her üyeye yazıyor; liste `ReplicatedUsing` ama karar yalnız sunucuda veriliyor); klan kıyası `IsInClan()` kapılı olduğundan iki **klansız** oyuncu eşit sayılmıyor (boş ad eşleşmez), `IgnoreCase` klan oluşturma tekilliğiyle (`SavedClanNameInUse` IgnoreCase) tutarlı; `ClanComponent`/`PartyComponent`/`GetClanComponent()` null korumalı. Hub koruması iki taraf için (`this` ve `Victim` konumu); saldıranın hub'da olması zaten `AttackTarget` başında engelleniyor (aynı 700u, çift kontrol zararsız). Engellenen vuruş `EndSpawnProtection`/`MarkCombatActivity` öncesinde döndüğü için savaş durumu/koruma bozulmuyor; engelde en-yakın-moba düşülmüyor (hedefli tıklama doğru şekilde etkisiz). `if (const TCHAR* ...; ...)` init-deyimi derleniyor.
- **Spawner refactor:** davranışsal olarak aynı: ilk deneme artık `FindSafeSpawnLocation` içinde yeniden örnekleniyor (aynı dağılım: açı 0-2π, yarıçap 150..SpawnRadius), Z +100/iz Base.Z±200/±60u yükseklik ve 0.7 eğim, nav 50/35u eşikleri birebir; `bValidateSpawnOnNavGround=false` yolu hiç değişmedi (`FindSafeSpawnLocation` çağrılmıyor); `bSpawnLocationWarningReported` ayarlama/sıfırlama aynı; uyarı artık neden sayaçlarını da yazıyor (format belirteçleri `%d/%.1f/%.2f` argümanlarla eşleşiyor). `SpawnOne`'da artık gereksiz kalan ilk rastgele `Offset` hesabı zararsız (yalnız doğrulamasız yolda kullanılıyor).
- **GenQuestDialogue.py:** kaçışlar doğru sırada (`\` önce, sonra `"`); `%`, `{`, `}`, `?` çıktıda yok ve metin `FString`'e doğrudan veriliyor (printf biçimi değil); en uzun replik 252 karakter (5 satır sınırında kesilmiyor, 760px kutuda ~3-4 satır; pencere çok dar ise `...` ile kısalır); `TMap<FName, FEntry>` işlev-yerel statik, `FName` çalışma anında oluşturuluyor (statik-başlatma sırası sorunu yok), oyun iş parçacığından çağrılıyor; Türkçe -> ASCII harf tablosu Lore'daki tüm karakterleri kapsıyor.
- **Winter cepleri:** yeni iki cep hub'a, birbirine ve diğer tüm Winter cebine Rift erişim modelinin gerektirdiği >=2110u mesafede; Kış Hükümdarı ile çakışmıyor.

### Ek manuel test adımları
- [ ] **PvP gerileme (klan/parti):** A ve B **klansız ve partisiz**, hub dışında, PvP açık: birbirine vurabilmeli (boş klan adı eşitlenmesin). Sonra A klan kursun: B (klansız) yine vurulabilmeli. A, B'yi klana alsın: vuruş engellenmeli. B klandan ayrılsın (`/klan ayril`): vuruş tekrar mümkün. Aynısı parti için: A-B partide, C partisiz: A, C'ye vurabilmeli; B partiden çıkarılınca A-B vurabilmeli.
- [ ] **Hub çift taraflı:** B, PlayerStart'a 700u içinde duruyor; A hub dışında 300u uzakta ama menzilde: A'nın vuruşu engellenmeli ("Guvenli bolgede PvP yok"); A hub'a girince A kendi vuruşunu da yapamamalı; B hub'dan çıkınca (700u dışı) vuruş mümkün olmalı.
- [ ] **Ölü/hedefsiz:** Ölü bir oyuncu hedef seçiliyken saldır: komut en yakın mobu seçmeli ya da "Menzilde dusman yok"; çökme/`Accessed None` yok. Hedef seçili olmadan `MMOAttack`: parti üyesi yanındaysa ona **vurmamalı** (otomatik hedef yalnız mob).
- [ ] **Balon kuyruğu:** İki görevi arka arkaya hızla kabul et: ikinci balon ilkinin yerine geçmeli (9 sn süre sıfırlanır), taşma/üst üste binme yok. Uzun replikli bir görevi (`Q_AbyssStriders` alış, 252 karakter) küçük pencerede (<=1024 genişlik) aç: metin 5 satırda `...` ile kesiliyorsa not et.
- [ ] **Spawner gerileme (Summer/Winter):** `Scripts/SmokeTest.ps1 -Map Summer_scene` ve `-Map Winter_scene`: çıkış kodu 0, "Hata satiri: 0", mob doğumu > 0; log'da yeni uyarı biçimi (`trace iskaladi=..., egim/yukseklik=..., navmesh=...`) yalnız gerçekten doğamayan cepte görünmeli. Boss spawner'lar (`SpawnRadius=150`) ve `bValidateSpawnOnNavGround=false` olan alanlar önceki gibi doğmalı.

## PvP ret geri bildirimi düzeltmesi — 2026-10-04

Uzak istemcinin sunucuda reddedilen yakın dövüş isteği artık sessiz kalmıyor. PvP kapalı, parti/klan dost ateşi, hedefin güvenli bölgede olması ve geçerli hedef bulunmaması durumları saldıran istemciye mevcut owner-only sohbet RPC'siyle iletiliyor. Saldıran doğrudan hub içindeyse sebep istemcide anında gösteriliyor. C++ derlemesi temiz geçti; aşağıdaki davranışlar PIE bekliyor.

- [ ] Host + uzak istemci aynı partideyken istemci hosta saldırsın: istemcide `Parti uyesine vuramazsin`, hasar 0.
- [ ] İki oyuncu aynı klandayken istemci saldırsın: istemcide `Klan uyesine vuramazsin`, hasar 0.
- [ ] Hedef hub içinde, saldıran hub dışında ve menzildeyken saldırı: istemcide `Guvenli bolgede PvP yok`, hasar 0.
- [ ] Saldıran hub içindeyken saldırı tuşu: yerel ekranda aynı güvenli bölge mesajı hemen görünmeli.
- [ ] `MMOPvP 0` durumunda uzak istemci oyuncuya saldırsın: istemcide `PvP kapali`, hasar 0.
- [ ] Mob saldırısı ve hub dışındaki izinli PvP vuruşu eskisi gibi hasar vermeli; reddedilme mesajı çıkmamalı.

# QA düzeltmeleri turu 2 — 2026-10-04

Kapsam: ORTA-2 (1 sn sınırı; ret sebebi aktarımı `0f03437`'de gelmişti), ORTA-3 (görev balonu), DÜŞÜK-1/4/5/6 ve `GenQuestDialogue.py` uyarısı. Derleme: `Build.bat MyProjectEditor` -> Result: Succeeded. Aşağıdakilerin hiçbiri oyunda denenmedi.

- [ ] **ORTA-2 (spam):** Host + uzak istemci aynı partide; istemci hostun üstüne saldırı tuşunu basılı tutsun/sık tıklasın. `Parti uyesine vuramazsin` sohbette/ekranda saniyede en fazla 1 kez görünmeli (sohbet satırı yağmamalı). 1 sn bekleyip tekrar saldırınca yine bir kez görünmeli.
- [ ] **ORTA-3 (kabul):** NPC'den görev kabul et: "Yasli Doran" balonu görev listede aktif olduktan hemen sonra (≤1 sn) çıkmalı. Host'ta (listen-server) da çıkmalı. Uzak istemcide de çıkmalı (sunucu yanıtı gelince).
- [ ] **ORTA-3 (teslim):** Tamamlanmış görevi teslim et: balon ödül gerçekten verilince çıkmalı. Ekipman ödüllü görevi (ör. `Q_FrostboundScouts`) ekipman çantası DOLUYKEN teslim et: "Ekipman cantasi dolu" uyarısı çıkar, balon **çıkmamalı**; çantayı boşalt, tekrar teslim et: balon + ödül.
- [ ] **ORTA-3 (uzak/limit):** Görev penceresi açıkken NPC'den 800u+ uzaklaş, "Kabul"a tıkla: "Gorevli yakininda degilsin", balon **çıkmamalı**. Aynı butona çok hızlı çift tık: en fazla tek balon, görev bir kez alınmalı.
- [ ] **DÜŞÜK-1 (kontrolcüsüz pawn):** (Elle tetiklemesi zor.) Mantık kodu: `IsPlayerPawn` = mob olmayan `AMMOCharacter`. Normal PvP/parti/klan/hub senaryoları (yukarıdaki maddeler) değişmeden çalışmalı; moblar hâlâ hasar vermeli/almalı (mob->oyuncu, oyuncu->mob).
- [ ] **DÜŞÜK-5 (Exec):** Development derlemede `MMOProbeSurface -5500 4500 225` hâlâ `[MMOPROBE]` satırı yazmalı (Shipping'de komut boş gövde, log yok).
- [ ] **Script:** `Scripts/GenQuestDialogue.py` çalıştır: Lore.md'de "Dönüşte" satırı olmayan görev varsa çıktı sonunda `UYARI: Q_... icin teslim repligi yok` satırları görünmeli; çıkış kodu 0 kalmalı (2026-10-04 itibarıyla Lore'da 73/73 görevin teslim repliği var, uyarı beklenmez).

## QA denetimi: turu-2 düzeltmeleri — 2026-10-04

Kapsam: `0f03437`, `4582d75`, `d2cb588`, `b57fa1c`, `9a390b8`, `0130e65`, `912e373` (kod okuma + derleme). **Oyunda (PIE) hiçbir şey denenmedi**; aşağıda "doğrulanmalı" yazan her madde kod okumasından çıkarımdır.

Derleme (denetçi çalıştırdı, editör kapalıydı): `Build.bat MyProjectEditor Win64 Development -WaitMutex` -> `Result: Succeeded`, uyarı/hata yok. PCH'siz tek dosya (`-NoPCH -NoLink -SingleFile`, `/W4`, rsp'de `/Yu` yok): `MMOCharacter.cpp`, `MMOHUD.cpp`, `MMOGameMode.cpp`, `MMOQuestDialogue.cpp` temiz (önceki turdaki `MMOHUD.cpp` `GEngine` C2065'i `Engine/Engine.h` ile gitti). Ek olarak **Shipping yapılandırması** (`MyProject Win64 Shipping -NoPCH -NoLink`, UHT `-WarningsAsErrors` ile): `MMOGameMode.cpp`, `MMOCharacter.cpp`, `MMOHUD.cpp`, `MMOStatsComponent.cpp` derlendi, hata/uyarı yok (yani `MMOProbe*` gövde sarması UHT'den geçiyor; tam Shipping paketi/bağlama yapılmadı). Not: UE `/wd4100` ile kullanılmayan-parametre uyarısını kapattığı için Shipping'deki boş gövdelerde `X,Y,Z,Radius` uyarısı zaten üretilmez.

### Bulgular (önem sırasıyla)

**ORTA-1: Tıkla-saldır hiçbir zaman hedefi `AttackTarget`'a geçirmiyor; oyuncuya tıklayınca PvP yok, en yakın moba vuruluyor.** `MMOPlayerController.cpp:1147` menzildeki `TargetEnemy` için her karede `MyChar->MMOAttack()` çağırır, o da `AttackTarget(nullptr)` (`MMOCharacter.cpp:911-915`); `nullptr` -> `FindNearestMob` (yalnız `AMMOMob`). Oyuncuya tıklamak `TargetEnemy`'yi oyuncuya ayarlıyor (`MMOPlayerController.cpp:839-840`, `OnToggleAttack`), yani yürüyüp yaklaşır ama vuruş ya yakındaki alakasız moba gider ya da mob yoksa "Menzilde dusman yok - yaklas" der. `CanHarmPlayer`/`IsPvPAllowed`/`TakeDamage` PvP kapısı yalnız `ServerAttackTarget(Target=oyuncu)` doğrudan çağrılırsa (hileli istemci, Blueprint) çalışır; Content'te `AttackTarget` geçen BP yok (`.uasset` taraması). Sonuç: `0f03437`/`d2cb588`/önceki checklist'teki "istemci hosta tıklayıp saldırsın" testleri arayüzden **yapılamaz** ve kodun test edilmiş sayılması yanıltıcı. (Doğrulanmalı: PIE.) Öneri: PvP istenen davranışsa menzilde `MyChar->AttackTarget(Enemy)` (null değil) çağrılsın; o zaman `bAllowPvP=true` varsayılanı da bilinçli karara bağlanmalı. PvP şimdilik kapalı kalacaksa checklist'teki tıkla-saldır PvP maddeleri "yalnız RPC ile" diye işaretlensin ve `bAllowPvP` varsayılanı `false` olsun.
- [ ] **Test (ORTA-1):** İki oyuncu (host + istemci), hub dışında, PvP açık. İstemci hostun karakterine **sol tıkla**, yanında dur. Beklenen (kod okumasına göre): hostun canı düşmez; istemcide sohbette "Menzilde dusman yok - yaklas" (yakında mob yoksa) ya da yakındaki bir mob hasar alır. Hostun canı düşerse bu bulgu yanlıştır, not et. Yanında mob varken tekrar et: vuruş moba mı gidiyor?

**ORTA-2: Hub'daki yerel "Guvenli bolgede PvP yok" mesajı her karede basılıyor (`0f03437`, `MMOCharacter.cpp:924-931`).** Hub kontrolü cooldown kapısından (`Now - LastMeleeTime`) **önce**; tıkla-saldır döngüsü (`PlayerTick`) menzilde her karede `MMOAttack()` çağırdığı için hub içinde (PlayerStart 700u) bir hedefe (mob ya da oyuncu) tıklayıp yanında durunca her karede `AddOnScreenDebugMessage(-1, 1.f, ...)` eklenir. `Key=-1` her çağrıda yeni satır ekler (yenilemez), yani 1 sn içinde ~60 satır yığılması beklenir (doğrulanmalı). Ayrıca mesaj "PvP" diyor ama kapı moblara saldırıyı da engelliyor (hub'da hiçbir şeye vurulamıyor). Yalnız Development'ta görünür (Shipping'de ekran mesajı derlenmez). Öneri: kontrolü cooldown'dan sonraya al ya da sabit pozitif anahtar kullan (`AddOnScreenDebugMessage(0x4D4D0001, ...)` gibi, yenilesin), metni "Guvenli bolgede saldiri yok" yap.
- [ ] **Test (ORTA-2):** Summer hub'ında PlayerStart yakınında bir hedefe (diğer oyuncu ya da 700u içindeki mob) sol tıkla, yaklaş ve 3 sn bekle. Beklenen kusur: ekranın üstünde yığılan ya da titreyen çok sayıda "Guvenli bolgede PvP yok" satırı. Tek satır görünüyorsa bulgu yanlıştır.

**ORTA-3: Uzak istemcide kalıcı sohbet satırı spamı: "Menzilde dusman yok - yaklas" (`d2cb588`, `MMOCharacter.cpp:957-977`).** Geri bildirim kanalı `ClientReceiveChat(..., 2)` = sohbet kutusuna **kalıcı** satır (geçici ipucu değil). ORTA-1 yüzünden bir oyuncuya tıklayıp yanında duran uzak istemci, mob yoksa her vuruş denemesinde (cooldown 0,6 sn, 1 sn aynı-metin sınırı -> yaklaşık 1,2 sn'de bir) satır alır; sınır spamı azaltıyor ama durdurmuyor. Ağ tarafında `Reliable`. (`docs/NETWORK_AUDIT.md` satır 136 aynı kanalı geçici ipuçları için kullanmayı işaretlemişti.) Öneri: bu "ipucu" sınıfı mesajları (menzil/PvP kapalı) ekran mesajı gibi geçici bir kanala ya da yeni bir `ClientShowHint` (Unreliable) RPC'sine taşı; sınırı 1 sn yerine metin başına 3-5 sn yap.
- [ ] **Test (ORTA-3):** Host + istemci; istemci bir oyuncuya tıklayıp (mob yok) 10 sn yanında dursun. Sohbet kutusunda "Menzilde dusman yok - yaklas" satırı kaç kez çıktı? (Beklenen kusur: ~8 satır; 1-2 satırdan fazlası istenmez.)

**ORTA-4: Merkezi hasar kapısı (`TakeDamage`) iki durumda yine atlanıyor (`4582d75` + `d2cb588`, `MMOCharacter.cpp:766-784`).** (a) Saldıran bulunamayınca (`EventInstigator->GetPawn()` null **ve** `DamageCauser` bir `AMMOCharacter` değil) kontrol tümden atlanır: kaynak `P.Instigator`'ı (zayıf işaretçi) geçersizleşmiş bir oyuncu DoT'u (`MMOStatsComponent.cpp:751-756`; saldıran çıkış yapınca/ölüp yok olunca) sonradan PvP kapatılsa, parti kurulsa ya da hedef hub'a girse bile tick etmeye devam eder; ayrıca ileride eklenecek mermi/tuzak aktörü `DamageCauser` olursa `Cast<AMMOCharacter>` null döner. Bugün oyuncu->oyuncu DoT/AoE/mermi yolu **yok** (`FindNearestEnemy` ve AoE yalnız `AMMOMob`; DoT yalnız moba), yani şu an erişilebilir değil; `NETWORK_AUDIT.md` D2 aynı noktayı işaretliyor. (b) Bu kapı yalnız `Attacker` bir oyuncuysa devreye giriyor; mob->oyuncu, `MMODamage` (kendine), çevre/düşme (düşme hasarı kodda yok) hasarı **engellenmiyor** (doğru). Öneri: `FMMOActiveEffect`'e saldıranın **kalıcı oyuncu kimliği** (ör. `PlayerState`/GUID ya da "kaynak oyuncu" bayrağı) yaz; `TakeDamage`'de "kaynak oyuncu bayraklı ama saldıran çözülemedi" ise PvP-reddet. Bugün için çalışan test yok; ileride oyuncu->oyuncu skill/DoT eklenirken zorunlu madde olarak kalsın.

**DÜŞÜK-1: Görev balonu bekleme listesi: tekrar eden kayıt ve sıra (`b57fa1c`, `MMOHUD.cpp:2492-2547`).** (a) Aynı satıra 2 sn içinde ikinci tık (uzak istemcide durum henüz replike olmadığından `IsQuestActive`/`IsCompleted` hâlâ false) **aynı `{QuestId, bClaim}` için ikinci kayıt** ekler; durum gelince `ShowNpcDialogue` iki kez çağrılır (aynı metin, görsel sorun yok ama `PendingQuestLines` büyür; tek tıkta sorun yok). Aynı durum "ekipman çantası dolu -> 2 sn içinde boşalt, tekrar tıkla" için de geçerli. (b) `UpdatePendingQuestLines` listeyi **sondan başa** gezer; iki kayıt aynı karede tamamlanırsa en eskisinin metni son yazıldığı için ekranda kalır (beklenen: en yeni). (c) Süre dolumu kontrolü `bDone`'dan **sonra**: pawn yokken (`DrawHUD` erken çıkışı) 2 sn geçmiş kayıt, pawn dönünce görev etkinse geç balon basar. Üçü de yalnız kozmetik. Öneri: `Add` öncesi aynı `{QuestId,bClaim}` varsa `ExpireTime`'ı yenile; döngüyü baştan sona yap (ya da yalnız en son kaydı göster); önce süre dolumunu, sonra `bDone`'u kontrol et.
- [ ] **Test (DÜŞÜK-1a):** Uzak istemcide (ping düşük) aynı görev satırına çok hızlı çift tık: görev bir kez alınmalı, balon tek kez görünmeli (çift görünse bile aynı metin; kaçırılan metin yok).

**DÜŞÜK-2: Balon 2 sn replikasyon penceresi.** Sunucu yanıtı 2 sn'den geç gelirse (yüksek ping, paket kaybı sonrası reliable yeniden gönderim, sunucu takılması) görev yine alınır/teslim edilir ama balon çıkmaz; kabul edilebilir (yalnız kozmetik, ödül/ilerleme etkilenmez). Listen-server host'ta `AcceptQuest/ClaimReward` senkron çalıştığı ve kayıt çağrıdan **önce** eklendiği için yarış yok (doğrulandı: kod). Görev penceresi kapanınca kayıt silinmiyor (HUD'a bağlı, pencereye değil); 2 sn içinde durum gelirse pencere kapalı olsa bile balon çıkar, bu istenen davranış gibi duruyor. Pawn sahip değiştirmesi/travel'da HUD yeniden yaratıldığı için kayıt sızıntısı yok. Öneri (isteğe bağlı): süreyi 3-4 sn yap.

**DÜŞÜK-3: Balon konuşmacısı hâlâ sabit "Yasli Doran" (`MMOHUD.cpp:2541`).** Önceki DÜŞÜK-3 açık. `AMMOQuestGiverNPC` kurucusu aynı adı veriyor, yani bugün tutarsızlık yok; Winter hub'da başka bir ad verilirse ayrışır. `MMOGameMode.cpp:577` "Muhafiz Kael" adlı farklı bir NPC var (görev vermiyorsa sorun değil, doğrulanmalı).

**DÜŞÜK-4: `Scripts/GenQuestDialogue.py` — kararlı ama kendi kendini doğrulamıyor.** Denetçi betiği kopya dizinde iki kez çalıştırdı: çıktı **bayt bayt aynı** (`sorted()`, yalnız `Lore.md`'ye bağımlı), depodaki `MMOQuestDialogue.cpp` ile **birebir aynı**; 73 girdi, 73'ünde teslim repliği, uyarı yok; çıktıda ASCII dışı karakter ve `\"` yok; en uzun alış 252, teslim 155 karakter; `MMOQuestTypes.cpp`'deki 73 görev kimliğiyle küme olarak **özdeş** (eksik/fazla yok). Zayıf noktalar: (1) betik görev veritabanıyla karşılaştırmıyor ve uyarılarda çıkış kodu 0; `Scripts/RunAllChecks.ps1` betiği hiç çağırmıyor, `ValidateData.py` de replik kapsamını denetlemiyor -> yeni görev eklenip Lore'a yazılmazsa kimse fark etmez. (2) `write_text` Windows'ta CRLF yazıyor (depo `i/lf w/crlf`); Linux/CI'da çalışırsa tüm dosya farklı çıkar. (3) `—` (em dash) ve `*Dönüşte:*` biçimi bozulursa girdi sessizce düşer. Öneri: `ValidateData.py`'ye "her `MakeQuest` kimliğinin `MMOQuestDialogue.cpp`'de alış repliği var, fazla kimlik yok" kontrolü ekle; betikte `write_text(..., newline="\n")` kullan; `RunAllChecks.ps1`'e betiği çalıştırıp `git diff --exit-code` ile üretilmiş dosyanın güncel olduğunu denetleyen adım ekle.
- [ ] **Test (DÜŞÜK-4):** `Scripts/GenQuestDialogue.py` çalıştır, ardından `git status`: `MMOQuestDialogue.cpp` değişmemiş olmalı (satır sonu dahil). Çıktı "73 gorev girdisi (73 tanesinde teslim repligi)" ve UYARI satırı olmamalı.

**DÜŞÜK-5: Eski yorum/belge kalıntıları.** `MMOGameMode.cpp` Winter yorumları güncellendi (Nöbetçi `(-3500,3000)`, Gezgin `(-5500,4500)` 59/64); `-2800,4400` ve `-5200,3600` yalnız "ilk konum/tarihçe" notlarında kaldı (doğru). Açık kalanlar: `docs/PROGRESSION_PLAN.md:137` hâlâ Uçurum Gezgini `(-5200,3600)` ve "11 Winter cebinin tamamı %100" diyor (satır 145 düzeltmeyi anlatıyor ama 137 işaretlenmemiş); `PLAYTEST_CHECKLIST.md` ~2682 "[KÜÇÜK] Yukarıdaki eski maddeler güncel değil" notu duruyor. `MMOProbe*` için önceki DÜŞÜK-5'in (a)-(d) maddeleri (prob `DontSpawnIfColliding`'i ve `Ignore` aktörünü taklit etmiyor; "nav yok" sayacı yok; `MMOProbeSurface` çatıyı bulabilir) açık; yalnız Shipping sarması kapandı.

### Önceki bulguların (2026-10-03 "PvP kuralları + görev repliği + spawner refactor") durumu
- ORTA-1 (merkezi hasar kapısı): **kapandı** (`4582d75`; kalan boşluklar yukarıda ORTA-4).
- ORTA-2 (ret sebebi uzak istemciye): **kısmen kapandı**: mekanizma var (`0f03437`) ama arayüzden erişilemiyor (ORTA-1 yeni) ve kalıcı sohbet kanalını kullanıyor (ORTA-3 yeni).
- ORTA-3 (balon başarısız işlemde çıkıyor): **kapandı** (`b57fa1c`), kozmetik kalıntılar DÜŞÜK-1/2.
- DÜŞÜK-1 (`IsPlayerControlled` ile oyuncu tespiti): **kapandı** (`d2cb588`); `AMMONpcBase` bir `ACharacter`'dır (`AMMOCharacter` değil), yani satıcı/görevli/şifacı/depocu/konuşmacı NPC'ler `IsPlayerPawn`'a ve `TakeDamage` kapısına hiç girmiyor (yanlış pozitif yok); `AMMOCharacter`'dan türeyen tek BP `BP_ThirdPersonCharacter` (oyuncu), seviyede yerleştirilmiş örneği yok (`Content` taraması).
- DÜŞÜK-2 (replik kapsamı): **kapandı** (73/73, DB ile özdeş).
- DÜŞÜK-3 (sabit "Yasli Doran"): **açık** (yukarıda).
- DÜŞÜK-4 (yorum tutarsızlığı): **büyük ölçüde kapandı**, kalıntılar DÜŞÜK-5.
- DÜŞÜK-5 (`MMOProbe*`): **kısmen** (Shipping sarması tamam; (a)-(d) açık).
- DÜŞÜK-6 (`Engine/Engine.h`): **kapandı** (PCH'siz derleme temiz).

### Doğrulanan sağlam noktalar
- **`TakeDamage` yanlış engelleme yapmıyor:** mob->oyuncu (saldıran `AMMOMob` -> `IsPlayerPawn` false), oyuncu->mob (`this` mob), kendine hasar (`MMODamage`, `Attacker==this`), saldıransız/ortam hasarı (`Attacker==null`) ve DoT'un mob kaynaklısı geçer. Blok, `Super::TakeDamage` ve `Stats->ApplyDamage`'den önce `0.f` döndürüyor: ölüm/XP/kredi/yüzen sayı tetiklenmez. Yalnız `HasAuthority()` altında; istemcide `TakeDamage` zaten çalışmaz.
- Doğum koruması kontrolü PvP bloğundan önce; sıra tutarlı.
- Mob tarafındaki hub kapısı (`MMOMob::TakeDamage`) ve DoT hub yarıçapı (`MobSourceHubRadius`) bu değişiklikten etkilenmedi.
- `GenQuestDialogue.py` çıktısı deterministik, sıralı ve depodakiyle özdeş (yukarıda). `docs/MCP_PILOT.md` ve `docs/ACCOUNT_BACKEND_COMPARISON.md`: yalnız belge; sır/anahtar yok (regex taraması); oyun içi LLM anahtarı hâlâ yalnız ortam değişkeninden okunuyor (`MMODialogueSubsystem.cpp:22-25`).
- `SendAttackFeedback` kısa devresi `return`'ü lambda içinde yapıyor; çağıranlar zaten hemen `return` ediyor (akış bozulmuyor). `LastAttackFeedback*` alanları UPROPERTY değil, yalnız sunucuda kullanılıyor (replikasyon gerekmez).
- [ ] **Regresyon (mob dövüşü):** Hub dışında bir mobu tıkla-saldır ile öldür; hasar sayıları, XP, görev ilerlemesi, ganimet eskisi gibi çalışmalı. Mob oyuncuya vururken can düşmeli; zehirli mob DoT'u tick etmeli (PvP bloğu bunları sıfırlamamalı).
- [ ] **Regresyon (NPC):** Satıcı/görevli/şifacı NPC'ye tıkla-saldır dene: NPC `AMMOCharacter` olmadığı için hedef olamamalı (yürüyüp yanına gider ya da etkileşim penceresi açılır); çökme yok.

# Ağ denetimi düzeltmeleri — 2026-10-04

Kapsam: `docs/NETWORK_AUDIT.md` Y2/Y4/Y5 ve O1/O2/O3/O9/O10. Her grup ayrı commit; derleme ve `Scripts/RunAllChecks.ps1 -SkipBuild` (veri + Summer/Winter duman + NetSmokeTest) geçti. Aşağıdakilerin hiçbiri PIE'de oynanarak denenmedi; ağ değerleri (Hz, cull) ölçülmedi.

## Y2 / O9 — özel alanlar yalnız sahibe (listen-server + en az 1 uzak istemci)
- [ ] **Kendi envanterin istemcide çalışıyor mu:** Uzak istemcide `I` ile envanteri aç: altın, eşya listesi, kuşanılan 4 slot, çanta, iksir sayısı eskisi gibi görünmeli; mob öldürüp loot al, altın/eşya anında güncellenmeli (OnRep sahip tarafında tetiklenmeli). Depo ve satıcı pencereleri de aynı şekilde.
- [ ] **Başkasının envanteri sızmıyor:** Host + istemci; istemcide başka oyuncunun pawn'ının `Inventory.Gold` değerini `showdebug`/BP ile oku: 0 (varsayılan) kalmalı, host altın kazansa bile değişmemeli.
- [ ] **Görev/skill/stat HUD'u:** Uzak istemcide görev takibi (aktif görev satırları), görev penceresi ve teslim balonu, skill kitabı (K), hotbar, stat penceresi (nokta dağıtma), MP ve EXP barı normal çalışmalı. Seviye atlayınca istemcide "dağıtılacak puan" artmalı.
- [ ] **Herkese açık alanlar korunuyor:** Parti listesinde diğer üyenin adı, seviyesi, sınıfı, canı ve ölü durumu görünmeli; isim plakası seviyesi doğru olmalı. Başkasının MP/EXP'si görünmemeli (zaten HUD'da yok).
- [ ] **Not:** `Party.Members/Leader` herkese replike kalmaya devam ediyor (düşük risk, dokunulmadı).

## Y5 — mob bileşenleri ve ağ sıklığı
- [ ] **Mob davranışı değişmedi:** Mobları çek, dövüş, öldür: hasar, XP, loot düşüşü, görev ilerlemesi ve parti XP paylaşımı eskisi gibi olmalı (Inventory/Skill/Quest/Party yalnız replikasyondan çıkarıldı, sunucu mantığı aynı).
- [ ] **Mob hareketi akıcılığı (DOĞRULANMADI):** Uzak istemcide koşan/saldıran moba bak: titreme/sıçrama varsa mob `NetUpdateFrequency` (30) / `MinNetUpdateFrequency` (10) değerlerini `AMMOMob` kurucusunda artır.
- [ ] **Cull mesafesi (DOĞRULANMADI):** Mob `NetCullDistanceSquared` = 12000². Uzak istemciyle bir moba 12000uu'dan uzaklaşıp geri dön: mob kaybolup geri gelirken eksik/hayalet görünüm olmamalı. Uzak oyuncunun hareket akıcılığı için de bak (oyuncu 60/20 Hz, cull motor varsayılanı).
- [ ] **Ölçüm:** 2 istemci + ~50 mob ile `stat net` / Network Profiler: istemci başına giriş B/sn hedefi < 15 KB/sn; değerler buna göre ayarlanmalı.

## Y4 / O10 — InstanceId tabanlı kuşan/yükselt/sat
- [ ] **Kuşan:** Çantada ekipmana tıkla: kuşanılmalı, slottaki eski eşya çantaya dönmeli. İstemcide ve host'ta dene.
- [ ] **Yükselt (sağ tık):** Çanta eşyasına sağ tıkla: altın düşmeli, sonuç mesajı (BASARILI/Basarisiz/YANDI) gelmeli. Hızlı çift sağ tık (+7 ve üzeri bir eşya): ikinci istek ya reddedilmeli (arti seviyesi uyuşmazlığı veya 0.3 sn soguma) ya da yalnız bir kez altın harcamalı; **başka bir eşyaya yükseltme/yanma uygulanmamalı**.
- [ ] **Bayat indeks (kritik):** `p.NetPktLag=200` benzeri gecikmeyle (örn. konsolda `Net PktLag=200`) çantanın ortasındaki eşyayı hızlıca iki kez sat/yükselt: ikinci istek bir sonraki eşyayı etkilememeli (eşya sayısı yalnız bir azalmalı).
- [ ] **Sat:** Satıcının yanında çanta eşyasına tıkla: altın artmalı, eşya gitmeli; satıcıdan 800u+ uzaktayken satış reddedilmeli. Sat penceresinde sayfalama (ShopSellPage) ikinci sayfada da doğru eşyayı satmalı.
- [ ] **Çanta dolu iken çıkarma:** Çantayı 12'ye doldur (`CANTA 12/12`), kuşanılan bir eşyaya tıkla (çıkar): reddedilmeli, sohbette "Canta dolu! Once bir esya sat veya depoya koy." çıkmalı (uzak istemcide de), çanta 12'yi aşmamalı. Çantada boş yer varken çıkarma normal çalışmalı.
- [ ] **Takas/depo gerilemesi:** Depoya koy/al ve ticaret (2 oyuncu) eski gibi çalışmalı (bu yollar zaten InstanceId kullanıyordu).

## O1 / O2 / O3 — sohbet ve klan
- [ ] **Uzunluk sınırı:** 300+ karakterlik bir mesajı sohbetten gönder: mesaj ~120 karaktere kırpılmış gelmeli (200'de ön kırpma + dallardaki 120 sınırı); `/klan kur` ya da `/w isim` ile çok uzun argüman: hata/kullanım mesajı kısa kalmalı, bağlantı kopmamalı.
- [ ] **Ortak throttle:** 0.7 sn'den hızlı art arda `/yardim`, `/who`, `/trade`, `/klan liste`, `/arkadas liste`, `/w` (hatalı kullanım dahil) gönder: aralarında 0.7 sn geçmeyenler sessizce düşmeli (hata yankısı yağmamalı). 0.7 sn bekleyince komut çalışmalı.
- [ ] **Boş komut regresyonu:** Boş mesaj ve tek başına `/p`, `/g` genel sohbete sızmamalı.
- [ ] **`/arkadas liste`:** 3+ arkadaşla: başlık + her arkadaş ayrı satırda ("X - cevrimici (Lv..)" / "X - cevrimdisi") görünmeli; yalnız tek sistem mesajı olarak gelir. Arkadaş yokken "Arkadas listen bos" çıkmalı. Diğer sistem mesajları (klan, ticaret, yardım) tek satır olarak eskisi gibi görünmeli.
- [ ] **`/klan kur` önbelleği:** Yeni bir ad kur → başarılı. Başka oyuncu aynı adı hemen dener → "kullanimda" (online ya da önbellekten). Klandan ayrıl, 30 sn içinde başka biri aynı adı kurmayı denerse "kullanimda (cevrimdisi uyeleri var)" görebilir (bilinen taviz); 30 sn sonra kurulabilmeli. Çevrimdışı bir üyenin kayıtlı klan adı kurulmaya çalışılınca reddedilmeli.

## Yapılmadı (bu turda kapsam dışı)
- Y3 (Can/Mana adımlama, `ActiveEffects.RemainingTime`), Y1 (kimlik = ad), O4-O8/O11/O12 ve D maddeleri. `Party.Members/Leader` kısıtlanmadı; `Stats` Strength/Dexterity/Intelligence/Vitality da herkese kaldı (OnRep_PrimaryStats ve istemci hesapları riski).

# QA turu-2 düzeltmeleri + ölçüm + başlangıç paketi — 2026-10-04

Kapsam: `1cf9577` (QA turu-2 ORTA-1/2/3), `dc4ad90` (`[MMOMETRIC]` günlüğü + `Scripts/PlaytestMetrics.py`) ve başlangıç paketi commit'i. Her grup derlendi (Development; Shipping tek-dosya `-NoPCH -NoLink` temiz) ve `RunAllChecks.ps1 -SkipBuild` (veri + Summer/Winter duman + NetSmokeTest) geçti. **PIE'de oynanarak hiçbir şey denenmedi.** Tek doğrulanan: ağ duman testinin sunucu logunda yeni karakter için `[MMODBG] Starter pack granted ... weapon=1 armor=1 bag=0 potions=3` satırı bir kez çıktı (headless, oynanış değil). `PlaytestMetrics.py` yalnız sahte satırlarla denendi.

## PvP varsayılanı + saldırı ipuçları (ORTA-1/2/3 karşılığı)
- [ ] **PvP varsayılan kapalı:** Yeni oturumda iki oyuncu (host + istemci), hub dışı, `MMOPvP` yazmadan: tıkla-saldır zaten oyuncuyu hedeflemiyor; `AttackTarget(oyuncu)` RPC/BP ile çağrılırsa "PvP kapali" gelmeli. Host konsolunda `MMOPvP 1` yazınca "PvP ACIK" görünmeli. (Not: yukarıdaki eski maddelerdeki "bAllowPvP varsayılan açık" ifadeleri artık geçersiz.)
- [ ] **Hub mesajı yığılmıyor:** Summer hub'ında (PlayerStart 700u içi) bir mob/hedefe sol tıkla, yaklaş, 3 sn bekle: ekranın üstünde tek satır "Guvenli bolgede saldiri yok" (sabit anahtar 4101) görünüp sürekli yenilenmeli; satır sayısı artmamalı. Metinde artık "PvP" geçmemeli.
- [ ] **Menzil ipucu geçici:** Uzak istemci, yanında mob yokken bir hedefe tıklayıp 10 sn yanında dursun. Sohbet kutusuna **hiç** "Menzilde dusman yok - yaklas" satırı düşmemeli; karakterin üstünde yukarı süzülen kısa mavi balon görünmeli ve en çok ~3 sn'de bir yenilenmeli (aynı metin 3 sn sınırı). Listen-server hostunda (yerel) ekranın üstünde tek yenilenen satır (anahtar 4102) beklenir.

## `[MMOMETRIC]` ölçüm günlüğü
- [ ] **Satırlar çıkıyor mu:** Listen-server host ile 5 dk oyna (mob öldür, loot al, satıcıdan sat/al, görev teslim et, en az bir kez öl, mümkünse seviye atla). `Saved/Logs/MyProject.log` içinde `[MMOMETRIC]` ara; olay türleri: `levelup` (from/to), `kill` (lv/mob/mlv/xp/party/elite), `death` (lv/x/y/z/by/xploss), `gold` (src/amount/balance). Her olay tek satır, `anahtar=deger` biçiminde, oyuncu adındaki boşluk `_` olmalı.
- [ ] **Altın kaynak etiketleri:** mob altını/loot -> `src=loot`; görev ödülü -> `src=gorev`; ekipman satışı -> `src=satis`; satıcıdan eşya/iksir alma -> `src=satin_alma`; yükseltme -> `src=yukseltme` (negatif); şifacı -> `src=iyilestirme` (negatif). **Bilinen kısıt:** oyuncular arası ticaret `src=diger` görünür (`MMOTradeComponent.cpp` başkasının değişiklikleri sürerken dokunulmadı); her iki taraf için +/- yazılır, net sıfır.
- [ ] **Parti kredisi:** 2 oyunculu partide mob öldür: öldüren için `party=0`, partidaşı için `party=1` satırı (aynı mob) çıkmalı; istemci tarafında hiç `[MMOMETRIC]` basılmamalı (yalnız sunucu).
- [ ] **Ölüm nedeni:** Mob öldürürse `by=<arketip>` (ör. `Goblin`); ortam/başka kaynakta `by=none`. `xploss` ekrandaki "Oldun! -N EXP" ile aynı olmalı.
- [ ] **Betik:** `python Scripts/PlaytestMetrics.py` (UE Python: `C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe`) gerçek logda oyuncu/seviye başına süre, öldürme, XP, ölüm, altın+/-/net tablosu, ölüm oranı ve altın kaynak dökümünü basmalı; hata vermemeli. `--who Oyuncu_Adi` ile filtre. İlk gerçek koşudan sonra sütunlar anlamlı mı, eksik metrik var mı not et.
- [ ] **Davranış değişmedi (regresyon):** Mob dövüşü, XP, loot, satış/yükseltme, görev teslimi eskisi gibi; sunucu logu dışında hiçbir fark olmamalı.

## Yeni karakter başlangıç paketi (VERTICAL_SLICE madde 2)
- [ ] **Yeni karakter:** Hiç kaydı olmayan bir adla (`MMOSetName YeniDeneme` ya da `Saved/SaveGames/MMOSave_<ad>.sav` olmadan) gir; ilk ~2 sn içinde envanterde (I) **Kısa Kılıç (Weapon) ve Deri Zırh (Armor) kuşanılı**, çantada başka eşya yok, **Can İksiri sayısı 3** (HUD "Iksir: 3"), altın 0 olmalı. Saldırı gücü çıplak değerden büyük olmalı. Sınıf seçim paneli (varsayılan Savaşçı ipucu) yine görünmeli; sınıf değiştirince kuşanılı eşyalar ve iksirler kaybolmamalı.
- [ ] **Çift verme yok:** Aynı karakterle 60 sn+ bekle (otomatik kayıt) ya da `MMOSave`, çık, tekrar gir: iksir hâlâ 3 (6 değil), kuşanılı eşya aynı, çanta boş. Kayıt-yükle-kayıt sonrası envanter sayısı sabit.
- [ ] **Eski kayıtlara verilmez:** Daha önce oynanmış kayıtlı bir karakterle gir: envanter/altın/iksir kaydedildiği gibi kalmalı, paket gelmemeli (sunucu logunda `Starter pack granted` satırı çıkmamalı).
- [ ] **Çok oyunculu:** Host + yeni adlı istemci: istemci kendi envanterinde paketi görmeli; host'un envanteri etkilenmemeli. Sunucu logunda iki ayrı oyuncu için birer `Starter pack granted ... weapon=1 armor=1 bag=0 potions=3` satırı.
- [ ] **Manuel `MMOLoad` (kayıt yokken):** Yeni karakterde ilk kayıttan önce `MMOLoad` yaz: ikinci kez paket verilmemeli (pawn bayrağı). Bilinen sınır: kayıt dosyası silinmiş eski bir karakter de "yeni" sayılır ve paket alır.
- [ ] **Doğuş güvenliği (madde 2 kabul ölçütünün PIE kısmı):** Yeni karakterle doğuş noktasında 3 dk bekle: hiçbir mob saldırmamalı (bu tur doğrulanmadı; `VERTICAL_SLICE_LV1_30.md` §2.3).

# Kayit yukleme dogrulamasi (SECURITY_AUDIT Y-2) — 2026-10-04 (DERLENMEDI, PIE yok)

Kapsam: `UMMOStatsComponent/UMMOInventoryComponent/UMMOQuestComponent::ApplySaveData`. Kayit bicimi degismedi; yalnizca yuklenirken degerler duzeltilir. Bilinmeyen esya ve gorev kimlikleri bilerek silinmez.

- [ ] **Normal kayit ayni kalir (regresyon):** Oynanmis bir karakteri kaydet, cik, gir: seviye, statlar, altin, iksirler, canta, kusanilan 4 slot, depo, aktif/tamamlanan gorevler ve ilerleme birebir ayni olmali.
- [ ] **Stat tavani:** `.sav` dosyasini elle/arac ile duzenle (ya da test kodu): `Vitality=2000000000` yukle; ana statlarin toplami `50 + (Seviye-1)*3` degerini asmamali.
- [ ] **Altin/yigin tavani:** Altin 999.999.999'a, iksir yigini 999'a, yigin turu sayisi 64'e kirpilmali.
- [ ] **Gecersiz nadirlik:** Nadirlik enum disinda bir esya yuklenince Siradan olarak gorunmeli, cokme olmamali.
- [ ] **Yanlis slot:** Silah "Zirh" slotunda kayitliysa yuklemede cantaya donmeli, kusanilan slot bos kalmali.
- [ ] **Yinelenen InstanceId:** Canta/kusanilan/depoda ayni kimlikli iki esya varsa ilki kalmali, digerine yeni kimlik verilmeli; kuşan/sat/yükselt tek esyayi etkilemeli.
- [ ] **Gorev sayaci:** `KillProgress=999999` olan aktif gorev hedef adedine (TargetCount) inmeli; yinelenen gorev kimligi tek kayda inmeli; bos kimlik atilmali; tamamlanan gorev listesindeki yinelenenler tekillenmeli. Ilerlemesi dogru olan gorevler degismemeli.

## Ad degistirme soguma suresi (SECURITY_AUDIT Y-7) — 2026-10-04 (DERLENMEDI)

- [ ] **Uzak istemci:** `MMOSetName A` sonra hemen `MMOSetName B`: ikinci degisim 30 sn dolana kadar sessizce reddedilmeli; arkadas bildirimi yagmamali. 30 sn sonra tekrar calismali.
- [ ] **Host/yerel:** Host konsolunda `MMOSetName` art arda cagrilari kisitlanmamali (test akisi bozulmamali).

## Debug komutlari Shipping'de kapali (SECURITY_AUDIT Y-6) — 2026-10-04 (DERLENMEDI)

`MMODamage/MMOHeal/MMOGainXP/MMOUseMana/MMOGiveItem/MMOBuff/MMOSpawnMob` ve `ServerGiveItem/ServerApplyBuff/ServerSpawnMob` gövdeleri `UE_BUILD_SHIPPING`'de erken doner. `MMOPvP/MMOClass/MMOSave/MMOLoad/MMOSetName/MMOUse` oyun komutu sayildigi icin acik kaldi.

- [ ] **Development/Editor regresyonu:** `MMOGiveItem HealthPotion 3`, `MMOGainXP 5000`, `MMOSpawnMob Goblin`, `MMOBuff Haste`, `MMODamage 20`, `MMOHeal 20`, `MMOUseMana 10` hepsi eskisi gibi calismali (SmokeTest/NetSmokeTest bu komutlara dayaniyorsa gecmeli).
- [ ] **Shipping paketi (opsiyonel, ilk dagitimdan once):** Paketlenmis Shipping build'de bu komutlar etkisiz olmali; ayrica `~` konsolunun Shipping'de gercekten kapali oldugunu dogrula (`DefaultInput.ini`'de `ConsoleKeys=Tilde` var).

## Olu karakter satici/yukseltme kapisi (NETWORK_AUDIT D3) — 2026-10-04 (DERLENMEDI)

- [ ] **Olu karakter:** Oyuncu oldugunda (olum ekrani) saticiya alis/satis ve cantadaki esyaya yukseltme istegi reddedilmeli; altin ve esya degismemeli. Canlanip tekrar denenince normal calismali.
- [ ] **Regresyon:** Canli karakterle satin alma, satis ve yukseltme eskisi gibi calismali. (Yukseltme icin yakinlik kosulu bilerek eklenmedi: tasarim karari bekliyor.)

# Lv11–30 beceri boslugu dolgusu: 4 sinifa 2'ser yeni beceri — 2026-10-04 (DERLENMEDI, PIE yok)

> Veri satirlari (`MMOSkillDatabase.cpp`), mevcut `MakeDef` kalibi. `Scripts/ValidateData.py` temiz: 69 beceri, rutbe/on kosul/efekt tutarli, devami on kosuldan zayif beceri yok. Guc/mana degerleri Lv10 ve Lv24 satirlari arasinda dogrusal ara deger; **PLACEHOLDER**, olcumle ayarlanmali. Yeni beceriler mevcut rutbeyi paylasir (ek puan harcatmaz).

| Sinif | Lv13 | Lv20 |
|---|---|---|
| Savasci | Ezici Darbe (190 guc, 58 mana) | Infaz (250, 85) |
| Okcu | Ok Sagnagi (AoE 140, Zehir) | Keskin Nisan (290, menzil 2000) |
| Buyucu | Buz Mizragi (195, Yavaslatma) | Gizemli Patlama (AoE 270) |
| Rahip | Ilahi Hukum (195) | Kutsal Onarim (muttefik sifa 520) |

- [ ] **Acilis:** Her sinifla `MMOGainXP` ile Lv13 ve Lv20'ye cik; ilgili agacin mevcut rutbesi yatirilmissa beceri Lv13/Lv20'de ogrenilebilir olmali (rutbe <= seviye).
- [ ] **Beceri kitabi (K):** Savasci artik 20 beceri: iki sutunda tasma/ust uste binme yok, son satirlar panel icinde.
- [ ] **His:** Yeni beceri, bir onceki zincir becerisinden belirgin guclu hissettirmeli; mana tavanina (Lv13/Lv20) takilmadan atilabilmeli.
- [ ] **Kutsal Onarim:** Partide muttefik secili iken onu iyilestirmeli, hedef yoksa kendini.
- [ ] **Regresyon:** Mevcut 61 becerinin davranisi degismemeli; hotbar atamasi/kayit-yukle eskisi gibi.

## Lv24–33 beceri boslugu dolgusu: 4 sinifa Lv28 becerisi — 2026-10-04 (DERLENMEDI, PIE yok)

> `ValidateData.py` temiz: 73 beceri. Warrior/Archer/Mage Special0 **rutbe 6**, Priest Special0 **rutbe 4** (her biri 1 beceri puani harcatir; yeni rutbe). Degerler PLACEHOLDER.

| Sinif | Lv28 beceri | Guc / mana |
|---|---|---|
| Savasci | Kudurma (yakin) | 340 / 110 |
| Okcu | Olum Isareti (Zehir) | 430 / 125 |
| Buyucu | Parcalama | 420 / 120 |
| Rahip | Karsilik | 330 / 105 |

- [ ] **Rutbe yatirimi:** Lv28'de Special0 agacina 1 puan yatirinca beceri acilmali; puan yoksa/rutbe seviyeyi asmiyorsa (rutbe <= seviye) reddedilmemeli.
- [ ] **Mana (Lv28):** INT'ye hic puan basmayan Savasci/Okcu bile bu becerileri bir kez atabilmeli (MaxMana ~ 25 + INT*5 + 28*5,5).
- [ ] **Hasar gozlemi:** Her sinifta bir onceki Lv24 becerisinden (Savas Firtinasi 270, Ejder Oku 360, Gok Gurlemesi 380, Meleklerin Ofkesi 290) tek hedefte belirgin yuksek vurmali.
- [ ] **Beceri kitabi (K):** Savasci 21 becerilik duzende tasma yok.

## Sohbet temizligi + arkadas duyuru tablosu siniri (NETWORK_AUDIT D5/D6/D9) — 2026-10-04 (DERLENMEDI)

- [ ] **Kontrol/bidi karakterleri:** Sohbete sekme veya U+202E iceren bir mesaj gonder: karakterler bosluga donmeli, satir gorunumu (gonderen/yon) bozulmamali; normal Turkce karakterler (ç, ğ, ı, ö, ş, ü) aynen kalmali.
- [ ] **Regresyon:** `/p`, `/w`, `/g`, `/klan`, `/arkadas` komutlari eskisi gibi calismali.
- [ ] **Arkadas duyurusu:** 5 dk icinde ayni durumun tekrar duyurulmamasi eskisi gibi; tablo 512 kaydi asmamali (test icin gerek yok, kod incelemesi).

## Cevrimici ad cakismasi baglantiyi reddeder (SECURITY_AUDIT K-1, kismi) — 2026-10-04 (DERLENMEDI)

`AMMOGameMode::PreLogin`: baglanti URL'indeki `?Name=` su an cevrimici bir oyuncunun adiyla (kayit yuvasi esdegeri: harf boyutu ve bosluk/`_` ayni) cakisiyorsa reddedilir. **Cevrimdisi birinin adini kapmayi engellemez** (hesap sistemi gerekir).

- [ ] **Cakisma reddi:** Host `MMOSetName Host`; ikinci bir istemci `open <host>:7777?Name=host` (kucuk harf) ile baglanmayi dene: "Bu ad su an baska bir oyuncu tarafindan kullaniliyor" ile reddedilmeli.
- [ ] **Normal baglanti:** Adsiz (`?Name` yok) ve benzersiz adli istemciler eskisi gibi baglanmali; `Scripts/NetSmokeTest.ps1` gecmeli.
- [ ] **Harita gecisi (MMOTravel):** Summer<->Winter gecisinden sonra oyuncular kendi adlariyla (ayni adla yeniden baglanma hatasi olmadan) dunyada kalmali.
- [ ] **Yeniden baglanma:** Istemciyi kapatip hemen ayni adla tekrar baglan: eski baglanti kopma zaman asimina kadar "ad kullaniliyor" gorulebilir (bilinen taviz).

## Kayit ezme korumasi: ad degisince baska yuvanin uzerine yazilmaz (SECURITY_AUDIT K-1, kismi) — 2026-10-04 (DERLENMEDI)

`AMMOCharacter::ActiveSaveSlot`: son yuklenen/olusturulan/yazilan yuva. Ad degisip hedef yuva farkli **ve zaten kayitliysa** kayit (otomatik, manuel, cikis, harita gecisi) reddedilir. Bos bir yuvaya (yeni ad) yazmak serbest; mevcut kaydi sahiplenmek icin o adla `MMOLoad` gerekir.

- [ ] **Ezme engeli:** Karakter A ile oyna ve kaydet. Yeni bir karakter B icin `MMOSetName A` (kucuk/buyuk harf fark etmez) dene: ad cakismasi cevrimici A varsa zaten reddedilir; A cevrimdisiyken B `MMOSetName A` yapip `MMOSave` yazsin: "Bu ad baska bir kayda ait" uyarisi cikmali, A'nin kayit dosyasi (`MMOSave_A.sav`) DEGISMEMELI (degistirilme zamanini kontrol et). Oyundan cikinca da A'nin dosyasi ezilmemeli.
- [ ] **Sahiplenme yolu:** Ayni durumda `MMOLoad` yaz: A'nin kaydi yuklenir (bilinen sinir: kimlik dogrulama yok, bu yol hala acik); ardindan `MMOSave` calismali.
- [ ] **Yeni ada tasima:** Kayitli A karakteri `MMOSetName YeniAd` (dosyasi olmayan ad) + `MMOSave`: yeni dosya yazilmali, hata olmamali; eski dosya yetim kalir (onceki davranis).
- [ ] **Regresyon:** Normal oynanista otomatik kayit (60 sn), cikista kayit ve `MMOTravel` ile harita gecisi eskisi gibi calismali (`SaveCharacterForTravel` hata dondurmemeli).

## Istege bagli sunucu katilim parolasi (SECURITY_AUDIT K-1/B-13) — 2026-10-04 (DERLENMEDI)

`AMMOGameMode::PreLogin`: sunucu komut satirinda `-MMOPassword=xxx` varsa baglanti URL'inde `?Password=xxx` zorunlu. Verilmezse davranis ayni. **Kimlik dogrulamanin yerini tutmaz.**

- [ ] **Parolasiz sunucu (varsayilan):** `-MMOPassword` olmadan host + istemci eskisi gibi baglanmali (`NetSmokeTest.ps1` gecmeli).
- [ ] **Parolali sunucu:** Host `UnrealEditor ... -game ?listen -MMOPassword=test123`; istemci `open <host>:7777` (parolasiz) reddedilmeli ("Sunucu parolasi gerekli veya yanlis"), `?Password=test123` ile baglanmali, yanlis parola reddedilmeli.
- [ ] **Not:** Parola komut satirinda gorunur (surec listesi); halka acik sunucu icin yeterli degil, yalniz tanidik kisilerle test icin.

## Varsayilan oyuncu adi OS kullanici adini sizdirmaz (SECURITY_AUDIT Y-5) — 2026-10-04 (DERLENMEDI)

`AMMOGameMode::ChangeName` (ilk atama, `bNameChange=false`): ad `<ad>-<8+ hex hane>` kalibindaysa (Null OSS varsayilani, ornek `CIDIK-3F2A...`) rastgele `Gezgin######` ile degistirilir. Kasitli adlar ve sonraki ad degisiklikleri etkilenmez.

- [ ] **Host ve istemci adi:** Adsiz oyuna gir (host ve istemci); `/who` ve sohbet gonderen etiketinde Windows kullanici adin **gorunmemeli**, `Gezgin######` gorunmeli.
- [ ] **Kasitli ad:** `open <host>:7777?Name=Ozan` ile baglan: ad `Ozan` kalmali. `MMOSetName Ozan2` da calismali.
- [ ] **Kayit:** `Saved/SaveGames` altinda yeni `MMOSave_Gezgin######.sav` olusur (onceki `MMOSave_<KULLANICI>-<hex>.sav` dosyalari yetim kalir).
- [ ] **Derleme:** `AGameModeBase::ChangeName` imzasi (`AController*, const FString&, bool`) UE 5.7'de dogrulanmali; fark varsa derleme hatasi verir.

## Can/mana yenilenmesi adimli (NETWORK_AUDIT Y3, madde 1) — 2026-10-04 (DERLENMEDI)

`UMMOStatsComponent::TickComponent`: yenilenme `HealthRegenAccum`/`ManaRegenAccum` ile biriktirilir, >=1 puan olunca yazilir. Toplam miktar ayni; `Health`/`Mana` her karede degismez, replikasyon ve HUD yayini azalir. Efekt dizisi (`ActiveEffects.RemainingTime`, madde 2) hala her karede degisiyor (yapilmadi).

- [ ] **Yenilenme hizi ayni:** Hasar al, savas disinda bekle: can ~6/sn (`HealthRegenPerSec`) hizinda dolmali; savasta `CombatRegenMult` ile yavas. Mana seviye ile artan hizda dolmali. Eski davranisla gozle fark etmemeli (can 1'er puan atlar).
- [ ] **Sinirlar:** Can/mana maksimumu asmamali; tam dolunca yenilenme durmali; olu oyuncuda yenilenme olmamali.
- [ ] **HUD:** Can ve mana cubugu 1 puanlik adimlarla akici dolmali (titreme/geri sarma yok), uzak istemcide de (can).
- [ ] **Olcum (opsiyonel):** `stat net` ile can yenilenirken oyuncu basina giris trafigi, onceki turlara gore azalmis olmali.

## Efekt suresi replikasyonu: RemainingTime -> EndServerTime (NETWORK_AUDIT Y3, madde 2) — 2026-10-04 (DERLENMEDI)

`FMMOActiveEffect::RemainingTime` artik `NotReplicated` (yalniz sunucuda azalir); replike alan `EndServerTime` (sunucu dunya saati, uygulama/tazelemede degisir). HUD kalan sureyi `EndServerTime - GetServerWorldTimeSeconds()` ile hesaplar. Beklenen etki: buff/DoT/yavaslatma olan oyuncu ve mobun `ActiveEffects` dizisi artik her karede yeniden gonderilmez.

- [ ] **HUD efekt kutusu:** Bir buff al (`MMOBuff Haste` ya da beceri): kutuda kalan saniye dogru geri saymali (host'ta ve uzak istemcide), cubuk suzulmeli; sure bitince kutu kaybolmali. Tazelenince (ayni buff'i tekrar al) sure basa donmeli.
- [ ] **Uzak istemci saat farki:** Istemcide sayac sunucudakiyle ~1 sn icinde uyumlu olmali; negatif veya sicrayan deger olmamali (`Max(0, ...)`).
- [ ] **Efekt mantigi degismedi:** Zehir (DoT) hasari, yavaslatma, saldiri/savunma/hiz buff'lari ve suresi dolunca kalkmasi eskisi gibi (yalniz sunucu mantigi RemainingTime kullanir).
- [ ] **Olcum (opsiyonel):** Buff'li oyuncu varken `stat net` trafigi, onceki turlara gore dusmeli.

## Sunucu mesajlari uzak istemciye ulasir: MMONotify::ToOwner (NETWORK_AUDIT O11 madde 3) — 2026-10-04 (DERLENMEDI)

Yeni `Core/MMONotify.h/.cpp`. Envanter (15 mesaj) ve gorev (7 mesaj) bilesenlerindeki `GEngine && IsLocallyControlledOwner()` bloklari `MMONotify::ToOwner(GetOwner(), ...)` oldu: yerel pawn'da ekrana, sunucuda uzak oyuncunun pawn'inda sahibine owner-only sistem satiri (kanal 2) olarak gider. Gorev ilerleme satiri ("X: 3/5") bilerek yerel kaldi (her oldurmede sohbeti doldurmasin). **Beceri bileseni (`bLocalFeedback`), karakter ve ganimet mesajlari ve parti bileseni (Codex'te) degismedi.**

- [ ] **Uzak istemci geri bildirimi:** Host + uzak istemci. Istemcide: cantayi doldurup satin al / ganimet al ("Canta dolu!"), altini yetmeden satin al ("Altin yetersiz"), saticidan uzakken satin al/sat ("Satici yakininda degilsin"), gorev al ("Gorev alindi: ..."), gorev teslim et ("Gorev tamam: ..."), seviyesi yetmeyen gorev ("Level N gerekli"): her biri **istemcinin sohbet kutusunda** sistem satiri olarak gorunmeli (daha once hic gorunmuyordu).
- [ ] **Host regresyonu:** Ayni islemlerde host'ta mesaj eskisi gibi ekran ustunde gorunmeli (sohbet satiri DEGIL), cift mesaj olmamali.
- [ ] **Sohbet spami:** Cantayi doldurup ust uste ganimet toplamayi dene (loot yolu degismedi); kisa surede ayni satirin tekrar tekrar sohbeti doldurup doldurmadigini not al (gerekirse hiz siniri eklenmeli).
- [ ] **Derleme:** Yeni dosyalar (`MMONotify.cpp/.h`) icin proje dosyalari yeniden uretilmesi gerekebilir (`Generate Visual Studio project files`); `AMMOPlayerController::ClientReceiveChat` erisilebilir olmali.

## Ganimet mesajlari toplayana gider (NETWORK_AUDIT O11 madde 3 / D7 komsusu) — 2026-10-04 (DERLENMEDI)

`AMMOLootPickup`: "Canta dolu!" ve ekipman dustu mesaji `MMONotify::ToOwner(Char, ...)` ile toplayana (uzak istemci dahil) gider. Altin ve yigin ("+N altin", "+N Esya") mesajlari artik yalniz toplayan **yerel** oyuncunun ekraninda; eskiden sunucuda herkesin ganimeti host ekranina basiliyordu ve uzak istemci hicbir sey gormuyordu.

- [ ] **Host ekrani temiz:** Uzak istemci ganimet toplarken host ekraninda "+N altin" ve "+N Esya" satirlari belirmemeli; host kendi ganimetinde eskisi gibi gormeli.
- [ ] **Uzak istemci ekipman:** Uzak istemci bir ekipman toplayinca sohbetinde "[Nadirlik] Esya +N dustu" satiri gorunmeli. Canta doluyken ayni ganimete yaklasinca "Canta dolu!" gorunmeli ve ganimet yerde kalmali.
- [ ] **Bilinen sinir:** Uzak istemci altin/yigin satirini artik hic gormuyor (envanter penceresinden/HUD'dan gorur); gerekirse ayri bir "ucan etiket" ile eklenmeli.

## LLM NPC diyalogu: sunucu geneli hiz siniri (SECURITY_AUDIT Y-4 / NETWORK_AUDIT O11-7) — 2026-10-04 (DERLENMEDI)

`UMMODialogueSubsystem`: dakikada en fazla `MaxRequestsPerMinute` (30) ve ayni anda en fazla `MaxConcurrentRequests` (4) HTTP istegi. Sinir asilirsa istek atilmaz, NPC kendi yedek replige duser (oyuncu basina 2 sn cooldown aynen duruyor). Anahtar yoksa davranis ayni. `MaxTokens` ayrica dusurulmedi (dusunme tokenlari ayni tavana sayiliyor).

- [ ] **Normal kullanim:** Konusan NPC'ye (Muhafiz Kael) tikla: LLM cevabi (anahtar varsa) ya da yedek replik eskisi gibi gelmeli.
- [ ] **Sinir:** Birkac oyuncu/pencereden dakikada 30'dan fazla NPC konusmasi tetikle (ya da `MaxRequestsPerMinute`'u editorde 2 yap): sinir asilinca Output Log'da `[MMODialogue] Hiz siniri asildi` uyarisi cikmali ve NPC yedek replik soylemeli, HTTP istegi gitmemeli. Bir dakika sonra yeniden calismali.
- [ ] **Es zamanli sayac:** Basarisiz/zaman asimina ugrayan isteklerden sonra sayac sifirlanmali (sinir sonsuza dek kilitlenmemeli): LLM anahtarini bilerek yanlis yapip 5 istek at, anahtari duzelt, istek yeniden calismali.

## Oldurme kredisi: hasar payina orantili XP (VERTICAL_SLICE_LV1_30 madde 10, kismi) — 2026-10-04 (DERLENMEDI)

`AMMOMob`: sunucuda oyuncu basina verilen toplam hasar izlenir (`DamageContributions`). Mob oldugunde XP, **hasar payina orantili** dagitilir: toplam hasarin %1'ini veren XP'nin %1'ini alir (esik yok). **Parti tek birim sayilir:** partinin toplam hasar payi **parti uye sayisina bolunur** (parti bonusu +%10/ekstra uye sonrasi 8 kisilik partide herkes payin 1,7/8'ini alir); payi yalniz menzildeki (3000u) uyeler alir (uzaktaki uyenin payi kaybolur). Gorev ilerlemesi paya bagli degil: payi olan herkes ve son vurusu yapan **tam** gorev kredisi alir. Hasar kaydi yoksa (olum oyuncu hasari disindan) eski yol: olduren/partisi tum payi alir (parti ise uye sayisina bolunur). Seviye farki carpani (`ScaleXPForLevelGap`) ve parti power-leveling tavani pay uygulandiktan SONRA aynen geçerli. Mob tam canla eve donunce katkilar sifirlanir. **Bu degisiklik eski "parti uyesi tam XP alir" davranisini kaldirir.** Ganimet sahipligi ve elit parti-olcekli can yapilmadi.

- [ ] **Kill stealing / oran:** Host ve istemci partisiz. Host moba canin ~%60'ini, istemci ~%40'ini vursun (istemci son vurusu yapsin): XP yaklasik %60 / %40 bolunmeli (`+N EXP` etiketi ve `[MMOMETRIC] ev=kill ... xp=` satirlari; ikinci oyuncu `party=1` etiketli). Toplam XP, mobun tam XP'sine (seviye farki carpani oncesi) yakin olmali.
- [ ] **%1 bile:** Istemci yalniz tek bir hafif vurus yapip cekilsin, host oldursun: istemci **en az 1 XP** almali (`FMath::Max(1, ...)`), host ~%99'unu.
- [ ] **Parti bolme:** 2 kisilik partide mobu birlikte oldurun (ornegin %50/%50 hasar): partinin hasar payi %100, parti bonusu (+%10/ekstra uye) ile her uye **%55** alir (=1,1/2; parti yok iken ikisi toplam %100 alirdi, burada toplam %110). 8 kisilik partiyle (gerekirse `MMOInvite` ile) bir uye tek basina oldursun: **her uye payin 1,7/8'ini (~%21) almali, fazlasini degil**; bolen menzil disindakiler dahil uye sayisi (8); menzil disindaki uyenin payi dagitilmamali.
- [ ] **Gorev kredisi tam:** Ayni mobu oldururken payi olan her oyuncunun gorev sayaci (ornegin `Q_Goblins`) **1 artmali** (bolunmemeli); son vurusu yapan, hic hasar vermemis olsa bile sayac artmali ama XP almamali.
- [ ] **Menzil/olu:** 3000u'dan uzaktaki ya da olu katki sahibi XP/kredi almamali.
- [ ] **Leash sifirlamasi:** Mobu cek, katki yaptir, mobu eve kacir ve tam canlansin; sonra baskasi oldursun: onceki katki sahibi pay almamali.
- [ ] **Regresyon:** Tek oyuncu oldurmeleri eskisi gibi tam XP; `[MMOMETRIC]` satirlari ve `GainExperience` cift cagrilmamali.

## Ganimet rezervasyonu: ilk 10 sn yalniz XP payi olanlar (VERTICAL_SLICE_LV1_30 madde 10, kismi) — 2026-10-04 (DERLENMEDI)

`AMMOMob` olurken XP payi olan herkesi (`LootOwners`; parti uyeleri dahil) o mobun TUM ganimetine (altin, item, ekipman) **10 sn** rezerve eder (`AMMOLootPickup::SetReserved`). Sure icinde baska oyuncu uzerinden gecse bile toplayamaz; sure dolunca (`OnReservationExpired`) pickup'in uzerinde duran oyuncular otomatik toplar (BeginOverlap tekrar tetiklenmedigi icin zamanlayiciyla yeniden denenir). Hak sahibi yoksa (oyuncu hasari yok) rezervasyon yok. Ganimet omru 30 sn (`LifeSpanSeconds`) degismedi.

- [ ] **Rezervasyon:** Host bir mob oldursun; istemci hemen cesedin uzerinden gecsin (hasar vermemis): ilk 10 sn **hicbir ganimet toplayamamali**. Host toplayabilmeli.
- [ ] **Sure dolunca:** Istemci cesedin uzerinde (pickup yaricapi 90u icinde) dursun, hareket etmesin: ~10 sn sonra ganimet **kendiliginden** istemciye gecmeli (zamanlayici).
- [ ] **Parti:** Partideki iki oyuncu birlikte oldursun: ikisi de 10 sn icinde toplayabilmeli. Partiden olmayan ucuncu oyuncu toplayamamali.
- [ ] **Kill stealing ile uyumlu:** Ikisi de partisiz ve ikisi de hasar vermis: ikisi de ilk 10 sn toplayabilmeli; hic hasar vermeyen ucuncu oyuncu toplayamamali.
- [ ] **Regresyon:** Tek oyuncu: ganimet eskisi gibi hemen toplanmali; cift toplama (dupe) olmamali; canta doluyken ekipman yerde kalip sonra toplanabilmeli; 30 sn sonra kaybolmali.

## Bagimsiz kod incelemesi (code-review, yuksek) duzeltmeleri — 2026-10-04 (DERLENMEDI)

- **Olumcul vurus payi:** `AMMOMob::HandleDeath` `TakeDamage` icinde senkron calistigindan olumcul vurusun hasari katki tablosuna eklenmiyordu (son vurusu yapan XP ve ganimet payi alamiyordu). Artik `UMMOStatsComponent::LastAppliedDamage` ile hesaba katiliyor. [ ] **Test:** Host canin %70'ini, istemci kalan %30'u vurup oldursun: XP ~%70 / %30 bolunmeli; istemci (olduren) ganimet rezervasyonunda olmali.
- **Kayit yukleme/canta:** Yanlis slottaki esya cantaya, canta doluysa depoya tasinir (depo kapasitesi yuklemede asilabilir; esya silinmez, yanlis slotta kalmaz; canta 12'yi asmaz). [ ] **Test:** Dolu canta (12) + yanlis slotlu kayitli esya yukle: canta 12'de kalmali, esya depoda gorunmeli; Output Log'da `yanlis slottaki esya` uyarisi cikmali.
- **NPC diyalogu sayaci:** `ProcessRequest()` baslatilamazsa eszamanli sayac sizmaz (NPC konusmalari kilitlenmez). [ ] **Test:** LLM anahtarini bozup birkac kez konus, sonra duzelt: konusma yeniden calismali.
- **Baslangic adi:** `?Name=` secenegi `ServerSetName` ile ayni karakter kuralina sokulur (satir sonu/kontrol karakteri atilir, en cok 24 karakter); PreLogin cakisma denetimi de temizlenmis adla yapilir; rastgele `Gezgin######` ayni zamanda diskte var olan bir kayit yuvasiyla cakismaz. [ ] **Test:** `open <host>:7777?Name=Test%0AFake` ile baglan: ad `TestFake` olmali, sohbette ek satir olusmamali.

### Ikinci kod incelemesi duzeltmeleri (2026-10-04, DERLENMEDI)
- **Olumcul vurus hasari `UMMOStatsComponent::LastAppliedDamage` uzerinden** (mob'da yan kanal/sifirlama kalmadi; dogrudan `ApplyDamage` yollari da kapsanir). [ ] Test: onceki "olumcul vurus payi" maddesiyle ayni.
- **Yanlis slottaki esya** cantaya, doluysa depoya (kapasite yuklemede asilabilir; esya silinmez, yanlis slotta kalmaz); `NextInstanceId` depodakileri de sayar.
- **`?Name=` 20 karakter:** motor ham ad degerini 20 karakterle keser; PreLogin ayni kesimle karsilastirir. 16 rastgele ad cakisirsa GUID tabanli benzersiz yedek ad verilir.
- **NPC hiz siniri:** baslatilamayan istek dakika sinirini tuketmez.

# XP hiz ayari, parti bonusu, Q_Gold* — 2026-10-04 (DERLENMEDI, PIE yok)

> Ayrintilar ve model sonuclari: `docs/BALANCE_MODEL.md` ("Uygulanan ayar"). Degerler `[PLACEHOLDER]`; **asil dogrulama `[MMOMETRIC]` olcumu**.

- [ ] **Carpan egrisi calisiyor mu:** Yeni karakter, solo Savasci. Lv1-5 mob/gorev XP'si eskisi gibi hizli olmali. `[MMOMETRIC] ev=kill` satirlarinda ayni mob icin `xp=` degeri eskisinden dusuk: Lv8 mobu ~%70, Lv12 ~%40, Lv20 ~%25, Lv34+ eskisi gibi (%100). (`MMOGainXP` ile seviye atlayip ayni arketipi ayni seviye farkiyla oldurerek karsilastir.)
- [ ] **Seviye basina oldurme (Lv20-30):** Lv21-30 arasi seviye basina ~13-20 oldurme olmali (model 12,9 / 18,5). Lv11-20 hala cok hizli cikabilir (modelde 1,2 / 3,2): bu **beklenen sinir**, olcumle karar ver.
- [ ] **Lv34 gecisi:** Lv30 -> Lv34 arasi dogrusal 0,55 -> 1,0 rampa (Lv31-33 mob carpani ~0,66 / 0,78 / 0,89; gorev 0,26 -> 1,0): mob XP'sinde ani siçrama/dusus olmamali.
- [ ] **Gorev odulleri:** `Q_Gold500` 150, `Q_Gold1000` 250 XP; `Q_Gold5000` kapi Lv16 oldugundan ~1200 XP; Lv8+ gorev odulleri eskisinden dusuk (gorev penceresi ve teslim mesajinda gorunen XP).
- [ ] **Parti bonusu:** 2 kisilik partide ayni mobu birlikte (%50/%50 hasar) oldurun: her uye payin **%55'ini** (=1,1/2) almali; toplam parti XP'si solo oldurmenin ~1,1 kati. 4 kisilik partide kisi basi 1,3/4 = %32,5. Tek basina oldurme: degisiklik yok.

# QA iterasyon 4: kayit yukleme kelepceleri, ganimet spami, uzak istemci mesajlari — 2026-10-05 (DERLENMEDI, PIE yok)

- [ ] **Stat kaydi (sinif tabani):** Lv10 Buyucu (taban 46) kaydini elle duzenle: `UnspentStatPoints=999`, INT'i +50 artir; `MMOLoad`: (statlar - 46) + harcanmamis <= 9*StatPointsPerLevel olmali; Output Log'da `harcanmamis stat puani ... kirpildi`. Gecerli kayit (her 4 sinif, puan harcanmis/harcanmamis) yuklemede **degismemeli**; sinif degistirip kaydet/yukle de degismemeli.
- [ ] **Canta 12 siniri:** Kayitta `EquipmentBag` 15 esya: yuklemede canta 12, 3 esya depoda; depo da doluysa (24) depo 27 olur ve `depo dolu` uyarisi cikar; hicbir esya kaybolmamali.
- [ ] **Bozuk kimlik:** Kayitta bir esyanin `InstanceId` degeri 0 ya da 2147483647, `NextInstanceId=2147483647`: esya yuklenmeli (yeni kimlikle), kusan/sat/yukselt calismali, yeni loot kimligi cakismamali. Bos `ItemId`'li girdi atilmali.
- [ ] **Yigin:** Ayni `ItemId`'li iki yigin (or. 2x HealthPotion 10) tek yigin (20) olmali; 70 farkli yiginli kayitta 6'si icin `yigin siniri` uyarisi.
- [ ] **Gorev kaydi:** Ayni gorev hem `ActiveQuests` hem `CompletedQuests`'te: yuklemede aktif listede gorunmemeli, odulu ikinci kez alinamamali; tamamlanmis listede tekrar olmamali.
- [ ] **"Canta dolu" spami:** Canta 12 doluyken yerdeki ekipmanin uzerinde ileri-geri yuru: mesaj en cok **3 sn'de bir** gelmeli (ikinci oyuncu icin bagimsiz).
- [ ] **Mob olumu:** Mob oldugunde Blueprint `OnCharacterDied` (olum animasyonu/efekti) yine **bir kez** oynamali (host ve istemcide); XP/ganimet degismemeli.
- [ ] **Uzak istemci mesajlari (B5):** Istemci olarak su mesajlarin sohbet/sistem satirinda gorundugunu dogrula (host'ta eskisi gibi ekranda): sinif degisimi (`Zaten bu siniftasin`, `... sn beklemelisin`, `Savasta sinif degistirilemez`, `Sinif: ... Puanlar iade edildi`), `Ustalik rutbesi: N`, hedefsiz skill'de `Menzilde hedef yok`, `Oldun! -N EXP`, `Iksir icildi`, `Kullanildi: ...`, `Karakter yuklendi`, `Adin '...' olarak ayarlandi`. Host'ta mesaj **cift** cikmamali.

## QA denetimi: bulut oturumu C++ (cdb8fe0) — 2026-10-05

Kapsam: `git diff 86a3b9c cdb8fe0 -- Source Scripts` (28 commit, 39 dosya) + `c2c5518` (yigin tavani, sonradan gelen). PIE'de OYNANMADI; asagidakiler kaynak okuma, betik ve derleme kanitidir. **Dogrulanmis** = kodda/motor kaynaginda gorulen; **dogrulanmali** = PIE/ag testi gerekir.
Arac kaniti: `MMOStatsComponent/MMOCharacter/MMOInventoryComponent/MMOMob/MMOGameMode/MMONotify/MMOXPTuning/MMOSkillComponent/MMOQuestComponent/MMOLootPickup/MMOMobArchetype/MMOQuestTypes.cpp` tek tek `-NoPCH -NoLink -SingleFile` ile derlendi: 12/12 `Result: Succeeded`, **sifir uyari**. `ValidateData.py` (73 beceri, 48 mob, 73 gorev; yalniz beceri-bosluk UYARI'lari, hepsi Lv28 sonrasi), `StaticLint.py` (71 dosya), `TestStaticLint.py` (15), `TestValidateData.py` (14), `TestPlaytestMetrics.py` gecti. Sabitler: `GetClassBaseStats` toplamlari Savasci/Okcu 50, Rahip 48, Buyucu 46 (kayit kelepcesinin varsayimi dogru); `MMOXPTuning.cpp` dugumleri = `docs/BALANCE_MODEL.md:153` = `ProgressionModel.py` (kodu okur, `floor(x+0.5)` = `FMath::RoundToInt`).

### Bulgular (ciddiyet sirali)

- [ ] **[YUKSEK, dogrulanmis kodda / PIE yok] Motor `SetName` / `ServerChangeName` yolu tum ad korumalarini atliyor.** `PlayerController.cpp:1996` (`ServerChangeName_Implementation`) dogrudan `GameMode->ChangeName(PC, S, true)` cagirir; `AMMOGameMode::ChangeName` (`MMOGameMode.cpp:124`) yalniz `bNameChange==false` dalinda temizler/cakisma arar, `true` ise `Super`'e aynen gecirir. Sonuc: uzak istemci konsolda `SetName X` (ya da elle RPC) ile ad degistirebilir ve `ServerSetName`'in tum kapilarini (24 karakter, harf/rakam, 30 sn soguma, cevrimici ad cakismasi, K-1 diskteki yuva reddi, arkadas duyurusu) **atlar**; kontrol karakterli ad (satir sonu) sohbet satiri taklidine yarar; ayni adi iki oyuncu alirsa `RequestZoneTravel` "Iki oyuncunun kayit adi ayni" ile host'un bolge gecisini kilitler (`MMOGameMode.cpp:~290`). Onerilen duzeltme: `ChangeName`'de `bNameChange==true` ise **ignore et** (proje kendi yolu `ServerSetName` zaten `PS->SetPlayerName` ile dogrudan yazar) ya da ayni `SanitizeInitialName` + cevrimici/yuva cakisma kontrolunden gecir. **Test:** Istemci konsolunda `SetName Ozan` (host'ta `Ozan` cevrimici): ad degismemeli ya da cakisma nedeniyle reddedilmeli; `SetName A<Enter>B` gibi kontrol karakteri satiri bozmamali.
- [ ] **[ORTA, dogrulanmis] `ActiveSaveSlot` navmesh-bekleme dalinda atanmiyor (K-1 korumasi bos/bayat kalir).** `MMOCharacter.cpp:~655-661` `LoadCharacter`, `bNavPending` ise `return true` ile `ActiveSaveSlot = Slot` satirindan (`~669`) ONCE cikar; `RetrySavedLocationRestore` de atamaz. (a) Yeni pawn'da `ActiveSaveSlot` bos kalir -> `SaveCharacterForTravel` korumasi (`:487`) **pasif** (baska birinin yuvasi ezilebilir). (b) Daha once baska yuva yukluyse bayat deger kalir -> hedef yuva diskte varsa **her kayit reddedilir** (otomatik kayit, `EndPlay` kaydi, bolge gecisi "Bir karakter kaydedilemedi; gecis iptal edildi"); uzak oyuncu icin red yalniz `UE_LOG` + yerel ekran mesajidir, oyuncu hic ogrenmez ve cikista ilerlemesini kaybeder. Oneri: `ActiveSaveSlot = Slot;` ataması `ApplySaveData` dizisinden hemen sonra (nav-bekleme dalindan once); red mesajini `MMONotify::ToOwner` ile gonder. **Test:** Navmesh yokken (`-NullRHI` hizli baslangic ya da Winter erken `MMOLoad`) mevcut kayitli karakterle gir; `MMOSetName <diskte kayitli baska ad>` host'ta, sonra `MMOSave`: ya acik mesajla reddedilmeli ya da yazmamali; kayit dosyasi bozulmamali.
- [ ] **[ORTA, dogrulanmis] `LooksLikeOsDefaultName` mesru adlari sessizce `Gezgin######` yapar (karakter "kaybolur").** `MMOGameMode.cpp:98-120`: `<ad>-<>=8 onaltilik hane>` ya da (`Len>=20` ve kuyruk >=4 onaltilik hane) kalibi. `Ozan-20261005`, `Kral-12345678`, `Ali-deadbeef` ya da kesilince `...-Fade`/`...-Dead` biten uzun adlar eslesir; oyuncu farkli (rastgele) yuvaya duser, eski kaydi yuklenmez, uyari gormez. Gercek Null-OSS adi `ad-<14 hane>` (`docs/SECURITY_AUDIT.md:118`, `CIDIK-<14 hex>`; 20 karakter siniriyla tam 20). Oneri: tam kalipta kuyruk >=12 hane iste; kesik kalipta `Len==20` ve kuyrukta en az 1 rakam; yeniden adlandirilan oyuncuya `ToOwner` ile "Adin Gezgin###### yapildi" bildir. **Test:** `?Name=Ozan-20261005` ve `?Name=Ali-deadbeef` ile baglan: ad **degismemeli** (su an degisir). `?Name=<OS adi>-<14 hex>` ise Gezgin olmali.
- [ ] **[ORTA, dogrulanmali] `PreLogin` cevrimici-ad reddi mesru yeniden baglanmayi kilitler.** `MMOGameMode.cpp:194+`: cokme/Alt+F4 sonrasi eski `PlayerController` sunucu zaman asimina kadar (varsayilan ~30-60 sn, dogrulanmali) cevrimici sayilir; ayni `?Name=` ile donen oyuncu "Bu ad su an baska bir oyuncu tarafindan kullaniliyor." alir. Oneri: ayni IP/UniqueId'li eski baglantiyi at ve kabul et, ya da mesajda "birkac saniye bekle" de. **Test:** Istemciyi gorev yoneticisinden oldur, hemen ayni adla yeniden baglan; ne kadar sure reddedildigini olc ve yaz.
- [ ] **[ORTA, dogrulanmis] `ServerSetName` 24 karakter kabul eder, ama `?Name=` motorda 20'ye kesilir** (`GameModeBase.cpp:815` `.Left(20)`; `MMOCharacter.cpp:748` `>24`). 21-24 karakterli adla kaydedilen karakter bir daha `?Name=` ile secilemez (kayit yuvasi `MMOSave_<24>` vs `<20>`); uzak oyuncu `MMOLoad` yapamaz. Oneri: `ServerSetName` ve `SanitizeInitialName` siniri 20 olsun. **Test:** `MMOSetName` ile 22 karakterlik ad ver, `MMOSave`, cik, `?Name=<ayni 22 karakter>` ile gir: ayni karakter gelmeli (su an gelmez); ya da 21+ adim reddedilmeli.
- [ ] **[ORTA, dogrulanmali] Parti payi: bolen TUM uye sayisi, odul yalniz menzildeki/canli uyeye.** `MMOMob.cpp:~836` `PartyMultiplier(Group.Num())/Group.Num()`, `Group` = `GetPartyInRange(..., 1.0e9f)` (uzak/olu/AFK dahil); XP yalniz 3000 uu icindeki canli uyeye gider, kalan pay kaybolur. 8 kisilik partide 1 kisi tek basina keserse payin 1.7/8 = %21'ini alir (solo %100). Belge ("bonus/uye sayisi") ile uyumlu ama uzak/olu uyenin payi dusurmesi belgelenmemis. Oneri: bolen = menzildeki canli uye sayisi (bonus da ayni sayiyla), ya da kasitliysa BALANCE_MODEL'e yaz. **Test:** 3 kisilik parti, biri sehirde AFK: ikisi mob keser; her biri payin 1.2/3 mu (%40) 1.1/2 mi (%55) aldigini `[MMOMETRIC] kill` / "+N EXP (%P)" ile karsilastir.
- [ ] **[ORTA-DUSUK, dogrulanmis] `MMONotify::ToOwner` uzak yolu gecici ipuclarini kalici sohbet satirina + Reliable RPC'ye cevirir.** `MMONotify.cpp:29` `ClientReceiveChat(..., 2)` (Reliable); renk/sure uzak istemcide atilir, host'ta ekran mesaji. `ClientShowHint` (Unreliable, kalici satir birakmayan) mevcut desene ters (`MMOCharacter.h:286-288`). Her satis/iksir/`Satici yakininda degilsin`/`Menzilde hedef yok` sohbeti doldurur; yetkisiz tekrar eden istek (rate-limit'siz ret yolu) sunucudan istemciye reliable kuyrugu sisirir (yalniz istismar eden baglanti etkilenir). Oneri: gecici ipuclari `ClientShowHint`/Unreliable, sonuc satirlari (satin alma, sinif, gorev) chat; ayni metin icin ~0.5 sn dedupe. **Test:** Istemci olarak satici disindayken satin al tusuna 20 kez bas / 12 esyayi art arda sat: sohbet tasmamali, baglanti kopmamali.
- [ ] **[DUSUK, dogrulanmis] B5 gecisi eksik kalanlar (uzak istemci sessiz):** `MMOQuestComponent.cpp:~186` "Once '...' gorevini bitir" hala `GEngine && IsLocallyControlledOwner()`; `ClaimReward` `CompletedQuests.Contains` erken donusu mesajsiz; `SaveCharacterForTravel` red mesaji (`:491`) yalniz yerel; `AMMOLootPickup` altin/yigin satiri uzak toplayana gitmiyor (HUD altin sayaci degisse de mesaj yok); `ServerSetName` reddi (karakter/uzunluk/soguma/cevrimici cakisma) sessiz. Oneri: ayni `ToOwner` kalibina tasi.
- [ ] **[DUSUK, dogrulanmis] Altin kayit tavani calisma zamani tavaniyla uyusmuyor:** `ApplySaveData` `SaveGoldCap=999999999`, `AddGold` ise `MAX_int32`'ye kadar. 1 milyar ustu altin yuklemede kirpilir (mevcut ekonomi gelirinde ~270k ulasilamaz, ama uyumsuz). Oneri: ikisi ayni sabit. (Yigin tavani `c2c5518` ile kapandi; yeniden bulgu degil.)
- [ ] **[DUSUK, dogrulanmis] "(%P)" etiketi anlam kaymasi:** `ClientNotifyXPGain` yorumu "seviye farki indirimi" der; artik pay (hasar/parti) de yuzdeye girer (2 kisilik partide seviye farki yokken "(%55)"). Oyuncu ceza sanir. Oneri: yorum/etiket guncelle ya da yuzde yalniz seviye farkini gostersin.
- [ ] **[DUSUK, tasarim] Esiksiz kredi:** Katki esigi yok; 1 puanlik vurus kisiye gorev kill kredisi + en az 1 XP + 10 sn ganimet ortakligi verir (`MMOMob.cpp` `LootOwners`, `ReserveLoot`). VERTICAL_SLICE madde 10 kasitli; yine de dusuk-seviye "etiketleyici" ile ganimet yarisi acilir. Karar sende.
- [ ] **[DUSUK, dogrulanmis] `Pri_Mending` (Lv20) `Pri_HolyRain`'i (Lv16) domine eder:** 120 mana/14 sn/520 guc vs 110/16/380; yorum "dogrusal ara deger" der ama bekleme iki komsudan da kisa. `ValidateData` yalniz ayni-etki-turu guc sirasini bakar. PLACEHOLDER; dengeleme olcumle.
- [ ] **[DUSUK, dogrulanmali] `-MMOPassword=` komut satirinda gorunur ve `?Password=` motorun `LogNet` "Login request: ..." satirina duser** (`World.cpp:7379`; sunucu log dosyasi). Yerel host icin kabul edilebilir; kamuya acik kullanimda ortam degiskeni + loglama disi tut.
- [ ] **[DUSUK, dogrulanmali] Istemci HUD efekt suresi `GetServerWorldTimeSeconds` ile:** ilk eslemeden once buyuk deger gorunebilir (`MMOHUD.cpp:~300-309`). Katilim sonrasi ilk saniyelerde buff yokken sorun yok; buff'li baglanma/seamless gecis sonrasi sure sapmasini gozle.

### Saglam bulunanlar (sessiz gecilmesin)

- **Stat/puan kelepcesi (`MMOStatsComponent.cpp:827-900`)**: degisken `(statlar + harcanmamis) <= taban + 3*(Level-1)` ile ChangeClass iadesi (`ResetPointsForClassChange`: `Max(unspent, Levels*3)`), `HandleLevelUp` (+3/seviye; `MMOGainXP` ile atlanan her seviye ayni yoldan) ve `SpendStatPoint` ayni degismezi tasir. Ekipman/buff yalniz turetilmis degerlere (AttackPower/Defense) yazilir, `FillSaveData` ham ana statlari kaydeder -> mesru kayit bozulmaz. Yukleme sirasi (`MMOCharacter.cpp:~621-625`: Skill -> Stats) ve `In.CharacterClass` geri-donus (Savasci) tutarli. Beceri puani tavani (`Levels*2`) harcanmisla birlikte kontrol edilmez: elle duzenlenmis kayitta mastery rutbeleri + harcanmamis puan toplami asabilir (onceden var, hile yuzeyi; dusuk).
- **Envanter (`ApplySaveData`)**: `IsValidInstance` `InstanceId != 0` oldugu icin `NeedsNewId(-1)` gecerli sayilir ve `Dedupe` yeni kimlik verir; fazla canta esyasi sondan depoya gider, silinmez; depo 24'u asarsa `Withdraw` yine calisir, yalniz yeni depolama reddedilir; `NextInstanceId` `MAX_int32/2` ile sinirli, `+1` tasmaz. Ticaret `MaxBagSize` on-kontrolu var (`MMOTradeComponent.cpp:623`).
- **Gorev**: tamamlanmis aktiften duser, `CompletedQuests` degismez -> zincir on kosulu (`IsCompleted(Prereq)`) bozulmaz; odul ikinci kez alinamaz (`ClaimReward` guard + `AddUnique`).
- **XP**: dugumler/yuvarlama/Lv34 rampasi dogrulandi; `LastAppliedDamage` yalniz `ApplyDamage` yolunda yazilir ve yalniz `HandleDeath`'te okunur (tek olum yolu); olumcul vurus `TakeDamage`'de tekrar eklenmez (`!bIsDead`); `bDeathHandled` Super'den once -> Blueprint `OnCharacterDied` tek sefer; PvP merkezi kontrolu (`CanHarmPlayer`) yalniz oyuncu hedefte calisir, mob->oyuncu / oyuncu->mob yolunu etkilemez; `EndServerTime` replikasyonu: `ProcessEffects` yalniz otoritede (`MMOStatsComponent.cpp:31`), istemci `RemainingTime=0` ile efekti silmez; OwnerOnly replikasyonla cakisma yok.
- **Derleme uyarisi**: yukaridaki 12 dosyada /W4 temiz; `StaticLint` `NotReplicated` alanini yanlis pozitif saymiyor.

### Eklenen oynanis testleri (yalniz yeni; onceki "QA iterasyon 4" maddeleri tekrarlanmadi)

- [ ] **`MMOGainXP` ile coklu seviye + yukle:** Yeni Savasci'da `MMOGainXP 5000` (birden cok seviye atlar): harcanmamis stat puani `3*(Level-1)` olmali; `MMOSave` -> `MMOLoad`: stat ve puanlar **ayni** kalmali; Output Log'da `kirpildi` yok. Sonra bir sinif degistir (`Buyucu`), 60 sn bekle, kaydet/yukle: puanlar yine ayni.
- [ ] **Ekipman/buff kayit etkisi:** Ekipman giy + `MMOBuff` (Hizli/Guclu) acikken `MMOSave`, `MMOLoad`: ana statlar degismemeli (buff/ekipman statlara karismamali).
- [ ] **Coklu istemci XP payi:** Host + 2 istemci, parti yok: iki istemci ayni mobu ~%50/%50 vursun, host son vurusu yapsin: her istemci ~%50 XP almali (host sifir XP, gorev kredisi alir); ekranda "+N EXP (%50)" benzeri. Sonra partiye girip tekrarla (1.1/2).
- [ ] **Olumcul vurus payi:** Mobu cok yuksek hasarli tek vurusla (host'ta) oldur: o oyuncu tam XP almali (katki kaydi bos olsa bile).
- [ ] **Katki suresi dolumu:** Istemci A mobu %60'a kadar vursun ve 100 sn beklesin; host mobu oldursun: A'nin payi sayilmamali (host ~tam XP); 80 sn beklerse payi sayilmali.
- [ ] **Ganimet rezervasyonu:** Mob olunce XP payi olmayan 3. oyuncu 10 sn icinde ganimete yurusun: alamamali; **tam o ganimetin ustunde dururken** 10 sn dolunca otomatik alabilmeli (yeniden yurumeden).
- [ ] **Uzak istemci mesaj yogunlugu:** Istemci olarak 12 esyayi satici ile art arda sat: sohbet satirlari okunur kalmali, ping artmamali; bir esya satilirken baska mesaj kaybolmamali.
- [ ] **Efekt suresi istemcide:** Istemci olarak Hizli/Guclu buff al: HUD'daki geri sayim yaklasik sunucuyla (+-1 sn) ayni olmali ve 0'da kalkmali; yeniden atilinca (tazeleme) sure basa donmeli.

## Tıkla-saldır PvP hedef aktarımı — 2026-10-05

`AMMOPlayerController::PlayerTick` menzildeki seçili `TargetEnemy` öğesini artık `AttackTarget(Enemy)` ile gönderir. Önceki `MMOAttack()` çağrısı hedefi `nullptr` yapıp her durumda en yakın moba düşüyordu. Varsayılan PvP kapalı kalır; host açıkça `MMOPvP 1` yapmadan oyuncu hasarı verilmez. C++ derlemesi temiz geçti, davranış PIE bekliyor.

- [ ] Host + uzak istemci hub dışında dursun; host `MMOPvP 1` yazsın. İstemci hosta sol tıklayıp menzile gelsin: host canı yakın dövüş cooldown'unda azalmalı.
- [ ] Aynı oyuncular partiye veya aynı klana girsin: tıkla-saldır hasar vermemeli ve saldıran istemci geçici ret ipucunu görmeli.
- [ ] Host `MMOPvP 0` yazsın: oyuncuya tıkla-saldır hasar vermemeli; mob hedefleme ve otomatik saldırı eskisi gibi çalışmalı.
- [ ] Hub içinde oyuncu veya mob hedefle: hasar olmamalı ve tek yenilenen `Guvenli bolgede saldiri yok` satırı görünmeli; ekran mesajı yığılmamalı.

## Sunucu genelinde elit duyurusu — 2026-10-05

Elit ölüm duyurusu mob aktöründeki NetMulticast'tan çıkarıldı. Sunucu artık bağlı `AMMOPlayerController` listesini dolaşıp her oyuncunun sahip istemcisine Reliable sohbet RPC'si gönderir. Böylece mobun ağ ilgililik/cull mesafesi dışındaki oyuncular da duyuruyu alır. C++ derlemesi temiz geçti; çoklu istemci PIE doğrulaması bekliyor.

- [ ] Host + iki istemci aç; istemci B elit mobdan ağ cull mesafesinin (12000 uu) dışında dursun. Host/istemci A eliti öldürsün: üç oyuncunun sohbetinde de tek `[Dunya]` satırı görünmeli.
- [ ] Aynı testi listen server hostu öldürücü vuruşu yaparken tekrarla: hostta satır bir kez görünmeli, çift duyuru olmamalı.
- [ ] Normal mob öldür: dünya duyurusu oluşmamalı.

## Mob hedef tarama maliyeti — 2026-10-05

Sunucu her mob için bağlı oyuncu listesini her karede taramak yerine en yakın hedefi 0,2 saniyede bir yeniler. Aradaki karelerde önbellekteki hedefin ölüm, canlanma koruması ve hub güvenli bölgesi kuralları yine kontrol edilir; geçersiz hedef taramayı hemen yeniden zorlar. Zorla aggro, leash, navmesh hareketi ve saldırı cooldown'u aynı kalır. Zorlanmış C++ derlemesi temiz geçti; Summer/Winter headless smoke sırasıyla 47/33 mobla, hata olmadan tamamlandı.

- [ ] İki istemci bir mobun zıt taraflarında dursun; daha yakın oyuncu değiştiğinde mob en geç yaklaşık 0,2 saniye içinde yeni yakına yönelmeli.
- [ ] Hedeflenen oyuncu ölür veya canlanma korumasına girerse mob beklemeden saldırıyı bırakmalı; eski MoveTo komutu oyuncunun dibine taşımamalı.
- [ ] Hedef hub güvenli alanına girince mob kovalamayı bırakıp leash davranışıyla dönmeli.
- [ ] 40+ mob ve iki istemciyle `stat game` / `stat unit` ölç; Game Thread süresini önceki yapıyla karşılaştır. Beklenen kazanç oyuncu-listesi taramasında yaklaşık 60 Hz -> 5 Hz'dir; gerçek toplam kazanç PIE'de ölçülmedi.

## Karşılıklı arkadaşlık onayı — 2026-10-05

`/arkadas ekle isim` artık hedefin listesine sessizce erişim vermek yerine 20 saniyelik sunucu taraflı istek oluşturur; hedef `/arkadas kabul` dediğinde iki liste birlikte güncellenir. Aynı hedefte bekleyen isteğin yenilenmesi veya başka davetçi tarafından ezilmesi engellenir. Çevrimiçi durum ve seviye yalnız iki listede de ilişki varsa gösterilir; tek yönlü eski kayıtlar bilgi sızdırmaz. Çevrimiçi arkadaş silinince iki taraf da güncellenir. Zorlanmış C++ derlemesi temiz geçti; çoklu istemci PIE doğrulaması bekliyor.

- [ ] A, `/arkadas ekle B` yazsın: A yalnız “istek gönderildi”, B kabul komutunu görmeli; kabulden önce A/B listelerinde karşı tarafın çevrimiçi seviyesi görünmemeli.
- [ ] B, 20 saniye içinde `/arkadas kabul` yazsın: iki oyuncu da birbirini çevrimiçi ve doğru seviyede görmeli; kaydet/yükle sonrası ilişki iki tarafta kalmalı.
- [ ] Aynı istek beklerken A tekrar göndersin ve C de B'ye göndersin: süre yenilenmemeli, C isteği A'nın isteğini ezmemeli.
- [ ] İstek 20 saniye geçtikten sonra kabul edilsin: iki liste de değişmemeli ve süre doldu mesajı görünmeli.
- [ ] A, B'yi silsin: çevrimiçiyse iki listeden de kalkmalı. B çevrimdışıyken eski tek yönlü kayıt kalsa bile B daha sonra A'nın çevrimiçi durumunu/seviyesini görememeli.
- [ ] Liste sınırı 30 olan tarafla istek/kabul dene: taşma olmamalı, iki liste yarım güncellenmemeli.

## Ticaret denetim günlüğü — 2026-10-05

Başarılı ticaretler sunucu loguna `LogMMOTradeAudit` kategorisinde tek yapılandırılmış `event=trade_complete` satırı yazar: iki oyuncu adı, iki yöndeki altın, ekipman kimliği/InstanceId/+ seviyesi/nadirliği ve sonuç bakiyeleri. Aktif ticaretin iptali `event=trade_cancel`, revizyon ve sunucunun iptal nedeni ile kaydedilir. Başarısız ve geri alınan işlemler tamamlanmış gibi loglanmaz. Zorlanmış C++ derlemesi temiz geçti; log içeriği PIE'de doğrulanacak.

- [ ] İki oyuncu farklı +/nadirlikte ekipman ve altın takas etsin: Output Log'da tam bir `event=trade_complete` satırı oluşmalı; iki taraf, InstanceId'ler ve sonuç bakiyeleri gerçek envanterle eşleşmeli.
- [ ] Teklif onayından önce iptal et ve ayrıca oyuncuları 800 uu'dan fazla uzaklaştır: her işlem için yalnız bir `event=trade_cancel` satırı oluşmalı, neden doğru olmalı.
- [ ] Çanta doluluğu veya değişmiş teklif nedeniyle sunucu takası reddetsin: `trade_cancel` görünmeli, `trade_complete` görünmemeli ve hiçbir eşya/altın kaybolmamalı.
- [ ] Tamamlanan ticarette yalnız bir `trade_complete` görünmeli; ayrıca yanıltıcı `trade_cancel` satırı oluşmamalı.

## Klan denetim günlüğü — 2026-10-05

Klan durumunu değiştiren sunucu işlemleri `LogMMOClanAudit` kategorisinde yapılandırılmış satırlar üretir: klan kurma, davet kabulüyle katılma, ayrılma, lider tarafından atılma ve çevrimiçi lidere otomatik yetki devri. Klan/oyuncu/davetçi/hedef ve liderlik bilgileri yalnız işlem başarıyla uygulandıktan sonra yazılır. Zorlanmış C++ derlemesi temiz geçti; ilk derleme paralel NetSmokeTest DLL'yi tuttuğu için başarısız oldu, test kapanınca yeniden derleme başarıyla tamamlandı.

- [ ] `/klan kur Deneme` sonrasında tek `event=clan_create` satırı görünmeli; klan, kurucu ve kayıt yuvası eşleşmeli.
- [ ] Daveti kabul eden oyuncu için tek `event=clan_join` oluşmalı; klan, katılan ve davetçi doğru olmalı. Süresi dolmuş/reddedilmiş davette log oluşmamalı.
- [ ] Üye `/klan ayril` yaptığında `event=clan_leave` ve doğru `was_leader` değeri görünmeli.
- [ ] Lider `/klan at Isim` yaptığında tek `event=clan_kick` oluşmalı; başarısız veya çevrimdışı hedefte log oluşmamalı.
- [ ] Lider ayrılır/bağlantısı kopar ve çevrimiçi üye varsa `event=clan_leader_transfer` yalnız bir kez oluşmalı; seçilen en eski çevrimiçi üye olmalı.

## Lv31 ve Lv39 beceri boşlukları — 2026-10-05

Dört sınıfa Lv31 alan/rol becerisi ve Lv39 tek hedef veya acil şifa becerisi eklendi: Savaşçı `Yer Sarsıntısı`/`Kafa Yarıcı`, Okçu `Dikenli Sağnak`/`Mızraklı Ok`, Büyücü `Alev Patlaması`/`Yıldız Düşü`, Rahip `Işıltılı Nova`/`Kurtuluş Işığı`. Mevcut mastery rütbelerini kullandıkları için ek rütbe puanı istemeden seviye kapısında açılırlar. Değerler `docs/CONTENT_PLAN_LV11_45.md` modelinden alınan PLACEHOLDER değerlerdir. ValidateData 81 beceriyi temiz doğruladı, 14 negatif senaryo geçti ve zorlanmış C++ derlemesi başarılı oldu; oynanış/denge PIE bekliyor.

- [ ] Dört sınıfta rütbe 6 (Rahipte ilgili ağaçta rütbe 4) yatırıp Lv30→31 geç: yeni Lv31 becerisi otomatik öğrenilmeli, fazladan puan eksilmemeli.
- [ ] Lv38→39 geçişinde sınıfın ikinci yeni becerisi otomatik öğrenilmeli; kaydet/yükle sonrası öğrenilmiş ve hotbar'a atanabilir kalmalı.
- [ ] `Yer Sarsıntısı` 460 yarıçapta hasar + Slow; `Dikenli Sağnak` hedef alanında hasar + Poison; `Alev Patlaması` 470 yarıçapta hasar; `Işıltılı Nova` 440 yarıçapta hasar vermeli.
- [ ] `Kafa Yarıcı`, `Mızraklı Ok` ve `Yıldız Düşü` yalnız seçili tek hedefi vurmalı; menzil dışı ve mana yetersizliği sunucuda reddedilmeli.
- [ ] `Kurtuluş Işığı` seçili parti üyesini 1100 iyileştirmeli; geçerli hedef yoksa mevcut HealTarget kuralıyla kendini iyileştirmeli; 260 mana ve 30 sn cooldown uygulanmalı.
- [ ] Lv28/Lv33/Lv45 komşu becerilerle hasar, mana ve cooldown hissini karşılaştır; yeni beceriler eski becerileri bütün eksenlerde geçmemeli. Ölçüm sonrası PLACEHOLDER değerleri güncelle.

## Lv17 ve Lv19 Alev Vadisi yan görevleri — 2026-10-05

Ana görev zincirini bloke etmeyen iki yan görev eklendi. `Kor Sürüsü`, mevcut Kor Tazısı spawner'ını; `Golem Ocağı`, mevcut Magma Golemi spawner'ını kullanır. ValidateData 75 görevi temiz doğruladı, 14 negatif senaryo geçti ve zorlanmış C++ derlemesi başarılı oldu. Görev akışı ve ödül teslimi PIE bekliyor.

- [ ] `Q_EmberHounds` tamamlandıktan ve karakter Lv17 olduktan sonra `Kor Sürüsü` görünmeli; önce görünmemeli.
- [ ] `Kor Sürüsü` kabul edildikten sonra 6 `EmberHound` öldür; yalnız doğru arketip sayılmalı ve ödül olarak 1800 XP, 1400 altın, 5 Büyük Can İksiri verilmelidir.
- [ ] `Q_MagmaGolems` tamamlandıktan ve karakter Lv19 olduktan sonra `Golem Ocağı` görünmeli; 6 `MagmaGolem` öldürülünce 2100 XP, 1600 altın, 5 Büyük Mana İksiri verilmelidir.
- [ ] İki yan görevi kabul etmeden ana zincirde `Q_AshOrcs` görevine ilerle; yan görevler ana zinciri engellememelidir.
- [ ] Her iki görevin kısmi ilerlemesinde kaydet/yükle yap; sayaç korunmalı, ödül yalnız bir kez alınabilmelidir.

## Lv13-15 Çözülme Yolu içerik dilimi — 2026-10-06

Çözülme Avcısı (Lv13), Kabuk Bekçisi (Lv14) ve Kavrulmuş Akıncı (Lv15) ile bunları üreten Summer_scene cebi eklendi. Cep `(-5000,-3000,50)` konumunda 225 u yarıçapla on ardışık headless probda 64/64 güvenli zemin ve nav sonucu verdi. Ayazdiş Devriyesi ile üç Çözülme yan görevi ana zinciri kilitlemeden eklendi. ValidateData 51 mob/79 görev/29 spawner ile temiz; 14 negatif senaryo, statik kontroller, Summer/Winter duman testleri, çok oyunculu bağlantı testi ve zorlanmış UE 5.7 derlemesi geçti. Görsel/oynanış PIE bekliyor.

- [ ] Summer_scene'de `(-5000,-3000)` çevresine git: beş normal mob yalnız `ThawStalker`, `CrustWarden`, `ScorchedRaider` bandından doğmalı; boşlukta, dik yüzeyde veya navmesh dışında doğum olmamalı.
- [ ] Çözülme Avcısı vurunca yaklaşık %35 Zehir; Kabuk Bekçisi %45 can altında Öfke; Kavrulmuş Akıncı vurunca yaklaşık %30 Yavaşlatma ve %40 can altında Öfke uygulamalı.
- [ ] Kavrulmuş Akıncı Warrok mesh/animasyonuyla zemine doğru oturmalı; diğer iki mobun renk, ölçek ve saldırı animasyonları okunaklı olmalı.
- [ ] `Q_VenomSpiders` tamamlanınca Lv11'de `Ayazdiş Devriyesi` görünmeli; herhangi 14 mob sayılmalı ve ödül 3000 XP, 800 altın, 3 Büyük Can İksiri olmalı.
- [ ] `Q_FrostLich` tamamlandıktan sonra Lv13/14/15 kapılarında üç Çözülme görevi görünmeli; görevler paralel kabul edilebilmeli ve yalnız kendi arketiplerini saymalıdır.
- [ ] Hedef/ödüller: 8 Çözülme Avcısı → 4700 XP/1000 altın/3 Büyük Mana; 8 Kabuk Bekçisi → 4950/1100/4 Büyük Can; 10 Kavrulmuş Akıncı → 5200/1200/4 Büyük Mana.
- [ ] Çözülme görevlerini almadan `Q_EmberHounds` ana zincirine ilerle; yan görevler ilerlemeyi engellememelidir.
- [ ] PlayerStart ve hub NPC'lerinde 3 dakika bekle: Çözülme mobları hub'a gelmemeli. Çözülme cebinden en yakın eski cebe mob sürükleyip iki grubun istemsiz birlikte aggro olup olmadığını kontrol et.
- [ ] Kısmi görev ilerlemesinde kaydet/yükle yap; sayaçlar korunmalı, her ödül yalnız bir kez alınabilmelidir.

## Karakter kaydı JSON şeması — 2026-10-06

Bulut kayıt hazırlığının üçüncü dilimi eklendi: `FMMOCharacterSaveData`, sürüm 1 JSON zarfına
çevriliyor ve geri okunuyor. Otomatik fixture bütün üst alanları, tüketilebilirleri, üç ekipman
konumunu, öğrenilmiş beceri/hotbar/mastery verisini, `TMap<FName,int64>` cooldown'larını,
görev/klan/arkadaş ve harita dönüşümünü karşılaştırdı. Örnek blob 1158 UTF-8 bayt; bilinmeyen
ileri şema sürümü reddedildi. Zorlanmış UE 5.7 derlemesi, JSON testi, veri/statik/negatif
kontroller, Summer/Winter duman testleri ve çok oyunculu bağlantı testi geçti.

- [ ] Gerçek bir karakteri yerel `.sav` dosyasından yükle, `MMOSave` ile yeniden yaz ve yeniden
  bağlan: mevcut yerel kayıt davranışı JSON yardımcı sınıfı eklendikten sonra değişmemeli.
- [ ] Geliştirme konsolunda `MMOTestSaveJson` çalıştır: Output Log'da tek
  `[MMO_SAVE_JSON_TEST] PASS schema=1 bytes=...` satırı görünmeli.
- [ ] İleride PlayFab uygulaması bağlanınca aynı karakterin yerel `.sav` ve bulut JSON sonuçlarını
  karşılaştır; başarısız HTTP yazımında yerel kayıt okunabilir kalmalı.

## Asenkron kayıt onaylı bölge geçişi — 2026-10-06

Summer/Winter bölge geçişi, bağlı bütün oyuncuların kayıt callback'leri başarılı olmadan
`ServerTravel` başlatmayacak şekilde yeniden kuruldu. Yerel dosya mağazası aynı karede tamamlanır;
gelecekteki PlayFab mağazası aynı sözleşmeyi gecikmeli tamamlayabilir. İstek seri numarası eski
callback'leri etkisiz kılıyor; kayıt sırasında kaybolan karakter ya da tek bir yazım hatası bütün
geçişi kaynak haritada iptal ediyor. `Scripts/TestZoneTravel.ps1` headless Summer -> Winter
geçişinde isteğin kabulünü, kayıt sonrasındaki travel başlangıcını ve Winter dünyasının açılışını
doğruladı. Gelmeyen callback için 15 sn güvenlik zaman aşımı eklendi;
`Scripts/TestZoneTravelTimeout.ps1` tek callback'i bilinçli düşürüp kaynak haritada iptali,
kilidin temizlenmesini ve aynı oturumdaki sonraki isteğin Winter'a geçmesini doğruladı.
Zorlanmış UE 5.7 derlemesi geçti.

- [ ] İki oyunculu PIE oturumunda geçişi başlat: ikinci oyuncunun kayıt callback'i bitmeden
  hiçbir istemci harita değiştirmemeli; ikisi de tamamlanınca birlikte Winter'a geçmeli.
- [ ] Gecikmeli mağaza taklidiyle bir oyuncunun kaydını başarısız yap: herkes Summer'da kalmalı,
  geçiş kilidi temizlenmeli ve sonraki başarılı istek çalışmalı.
- [ ] Gerçek HTTP mağazasında isteği yanıtsız bırak: 15 sn sonra iptal mesajı görünmeli; aynı
  oturumdaki sonraki geçiş isteği kabul edilmeli ve geç gelen eski callback yeni isteği etkilememeli.
- [ ] Kayıt callback'i beklerken bir istemciyi ayır: geçiş iptal edilmeli ve kalan oyuncuların
  `bTravelSaveCommitted` durumu sonraki normal kaydı engellememeli.
- [ ] Başarılı seamless travel sonrasında seviye, altın, envanter, görevler, beceri cooldown'ları
  ve konumun tek kez korunup korunmadığını iki istemcide kontrol et.

## Dedicated uyumlu hasar/iyileştirme sayıları — 2026-10-06

Sunucu tarafındaki `TakeDamage` ve `Heal` yollarından doğrudan ilk PlayerController HUD'ına
çizim kaldırıldı. Kozmetik sayı artık hasar alan/iyileşen karakterin `Unreliable NetMulticast`
RPC'siyle ilgili istemcilere gider; her süreç yalnız kendi yerel HUD'ına çizer ve dedicated
sunucu çizim yapmaz. `OnRep_Health` içindeki eski hasar sayısı kaldırıldığı için aynı vuruşun
iki kez görünme yolu kapandı. Zorlanmış UE 5.7 Editor derlemesi geçti. Kurulu Epic binary motor
Server target derlemesini desteklemediği için gerçek dedicated executable doğrulaması kaynak
UE 5.7 motoruna kaldı.

- [ ] İki oyunculu PIE'de A, B'ye tek vuruş yapsın: iki pencerede de B'nin üstünde aynı miktar
  yalnız bir kez görünmeli; A'nın HUD'ında uzaktaki ilgisiz vuruşlar görünmemeli.
- [ ] B can iksiri, şifa becerisi ve Şifacı Lyra kullansın: yeşil `+N` aktif iyileştirmesi iki
  ilgili pencerede bir kez görünmeli; pasif can yenilenmesi uçan sayı üretmemeli.
- [ ] `Net PktLoss=20` ile birkaç vuruş/şifa dene: kozmetik sayı kaçabilir fakat can ve ölüm
  replike durumuyla doğru kalmalı; oyun mantığı multicast sonucuna bağlı olmamalı.

## Ekonomi ve kimlik denetim logları — 2026-10-06

Takas ve klan loglarına envanterdeki geri döndürülemez hareketler ile ad değişimi eklendi.
Yükseltme; sonuç, maliyet, `InstanceId`, artı ve bakiye; satış ve depo hareketleri eşya kimliği
ile kayıt yuvasını; ad değişimi eski/yeni ad ve kayıt yuvalarını yapılandırılmış tek satırlar
halinde `Saved/Logs` altındaki sunucu loguna yazar. Zorlanmış UE 5.7 Editor derlemesi,
ValidateData/StaticLint ve negatif testler, JSON/travel/timeout testleri, Summer/Winter duman
testleri ve NetSmoke geçti. Gerçek işlem satırlarının üretilmesi PIE bekliyor.

- [ ] Bir eşyayı yükselt: Output Log'da `LogMMOInventoryAudit` / `event=item_upgrade` satırı,
  doğru sonuç (`success`, `failed`, `burned`), maliyet, eski/yeni artı ve son bakiye görünmeli.
- [ ] Bir ekipman satıp başka bir ekipmanı depoya koy/geri al: `item_sell`,
  `warehouse_deposit`, `warehouse_withdraw` satırlarında aynı `InstanceId`, eşya ve yuva olmalı.
- [ ] İki oyunculu ticareti tamamla ve bir ticareti iptal et: `LogMMOTradeAudit` satırlarında iki
  oyuncu, iki yönlü eşya/altın ve iptal sebebi bulunmalı; tamamlanan işlemden sonra iki kayıt yazılmalı.
- [ ] Klan kur/katıl/ayrıl/at/lider devri akışlarını dene: `LogMMOClanAudit` her kalıcı değişimi
  yapan oyuncu ve hedefle yazmalı.
- [ ] `MMOSetName` ile adı değiştir: `LogMMOIdentityAudit` eski/yeni ad, eski/yeni yuva ve
  `active_slot` değerlerini yazmalı. Aynı ad, geçersiz ad ve cooldown reddi başarı satırı üretmemeli.

## Sunucuya özel mob spawner'ları — 2026-10-06

`AMMOMobSpawner` artık replike edilmez ve haritadan istemciye yüklenmez. Runtime spawner'ları
GameMode yalnız sunucuda oluşturur; mobların kendisi replike kalır. Bu, her oyuncu için 29 boş
spawner aktör kanalını kaldırır. Editör derlemesi ve `RunAllChecks.ps1 -SkipBuild` (ValidateData,
statik/negatif kontroller, kayıt ve bölge geçişleri, Summer/Winter smoke, NetSmoke) geçti.
Gerçek aktör kanal sayısı Network Profiler/PIE ile ölçülmedi.

- [ ] İki oyunculu PIE'de Summer ve Winter haritalarında normal/elit mob sayıları eskisi gibi
  oluşmalı; istemci mobları görmeli, hareketlerini ve ölümlerini almalıdır.
- [ ] İstemci konsolunda `Net.ListActorChannels` çalıştır: `MMOMobSpawner` kanalı olmamalı;
  `AMMOMob` kanalları yakınlık/cull davranışına göre görünmelidir.

## Tüketilebilir kullanım sınırı — 2026-10-06

Can/mana iksirleri ve buff parşömenleri artık aynı sunucu-otoriter 1 saniyelik kullanım
kapısını paylaşır. Sahip istemci de aynı kapıyı uygulayarak sunucuya Reliable RPC yığmaz;
sunucu doğrudan gönderilen çağrıları yeniden sınırlar. Editör derlemesi ve
`RunAllChecks.ps1 -SkipBuild` (ValidateData, statik/negatif kontroller, kayıt ve bölge
geçişleri, Summer/Winter smoke, NetSmoke) geçti.

- [ ] Canı azaltıp `F` tuşuna çok hızlı bas: ilk Can İksiri tüketilmeli, sonraki 1 saniye
  içinde başka iksir eksilmemeli; süre dolunca yeniden kullanılabilmelidir.
- [ ] Can iksirinden hemen sonra `G` ile Mana İksiri veya envanterden bir parşömen kullan:
  ortak 1 saniye dolmadan ikinci eşya tüketilmemeli; dolduktan sonra normal çalışmalıdır.
- [ ] Host ve uzak istemcide aynı davranışı doğrula; hızlı basışlar sohbeti/Reliable RPC
  kuyruğunu doldurmamalı ve bağlantıyı düşürmemelidir.

## Hub güvenli bölge merkezi önbelleği — 2026-10-06

`IsInsideHubSafeZone`, her saldırı ve beceri kontrolünde bütün `PlayerStart` aktörlerini
taramak yerine merkezleri dünya başına ilk çağrıda önbelleğe alır. Travel yeni `UWorld`
oluşturduğu için eski harita merkezi kullanılmaz; geçersiz dünya kayıtları temizlenir.
Editör derlemesi, StaticLint, Summer/Winter smoke ve NetSmoke geçti.

- [ ] Summer haritasında PlayerStart çevresinde 700 birim içindeyken normal saldırı ve
  saldırı becerileri engellenmeli; sınırın dışına çıkınca tekrar çalışmalıdır.
- [ ] Winter haritasına geçip aynı kontrolü tekrarla: Summer merkezi taşınmamalı, Winter
  PlayerStart çevresi güvenli bölge olmalıdır.
- [ ] İki oyunculu PIE'de oyunculardan biri hub içinde, diğeri dışında olsun: iki yöndeki
  PvP saldırısı da reddedilmeli; iki oyuncu dışarı çıktığında mevcut PvP kuralları çalışmalıdır.

## Bölge geçişi sırasında ertelenen kayıtlar — 2026-10-06

Gecikmeli bulut mağazası hazırlığında, bölge geçişi kaydı uçuşta olan karakterin otomatik,
elle, klan veya ticaret kaynaklı ek kaydı artık geçiş yazımını bayatlatmaz. Ek kayıt geçiş
başarılı olduğunda veya iptal edildiğinde bir kez yazılır. UE 5.7 derlemesi ve bütün otomatik
kontroller geçti; timeout testi erteleme ve iptal sonrası yazımı doğruladı. Gerçek iki oyunculu
PIE ve gecikmeli mağaza ile oynanmadı.

- [ ] Gecikmeli mağazada Summer -> Winter geçişini başlatıp callback beklerken `MMOSave` çalıştır:
  oyuncu erteleme mesajı görmeli, geçiş sırf yeni yazım nedeniyle iptal olmamalı ve kayıt geçiş
  tamamlanınca yazılmalıdır.
- [ ] Callback zaman aşımında oyuncular Summer'da kalmalı; ertelenen kayıt iptalden sonra yazılmalı,
  kilit temizlenmeli ve ikinci geçiş denemesi çalışmalıdır.
- [ ] Geçiş kaydı sürerken uzak istemciyi kapat: sunucu geçişi güvenli biçimde iptal etmeli, kalan
  oyuncuların ilerlemesi kaybolmamalı ve bir sonraki geçiş isteği kabul edilmelidir.

## JSON kayıt sınırları — 2026-10-07

JSON dönüştürücüde ayrıştırmadan önce 16 düzey sınırı, yazma/okuma için ortak doğrulama
ve sayısal enum dönüşüm sınırı eklendi. Derleme, 12 otomatik kontrol ve son fixture
iyileştirmeleri sonrası JSON (111 negatif senaryo) / mağaza testleri geçti. 178 gerçek
kayıt alan alan eşit döndü; en yüksek gerçek karakter seviyesi 6. JSON bugün oyun kayıt
yolunda kullanılmıyor; gelecekteki bulut mağazası hazırlığıdır. PIE oynanmadı.

- [ ] İki oyunculu PIE'de kaydet, çıkıp yeniden gir ve Summer/Winter arasında geç:
  mevcut yerel karakter ilerlemesi korunmalı ve kayıt/yükleme çalışmalıdır.
- [ ] Lv30+ ve depo/klan/görev dolu gerçek karakter kaydını oluştur; mağaza testinde
  `json mismatch=0` ve `maxLevel>=30` kanıtını al.
- [ ] Bulut mağazası eklendiğinde geçersiz JSON okuma/yazma reddinin mevcut kaydı
  ezmediğini ve hatanın çağırana iletildiğini doğrula; bugün entegrasyon bulunmuyor.

## Bölge geçişi öncesi oyuncuları yeniden doğrulama — 2026-10-07

Geçiş kayıtları tamamlandığında, sunucu oyuncu/pawn kümesini ve canlı/savaş dışı
koşullarını yeniden denetler. Bekleme sırasında koşullar değişirse geçiş iptal edilir.
UE derlemesi ve tek oyunculu headless savaş enjeksiyonu geçti: onay beklerken savaş,
iptal, boş bariyer, temiz kilit/zamanlayıcı. Gerçek gecikmeli mağaza ve iki oyunculu
PIE oynanmadı. Anlık görüntü sonrası XP/altın kaybı konusu bu değişiklikle çözülmedi.
`RunAllChecks.ps1 -SkipBuild -FullCompile` yeni regresyon dahil 13/13 geçti;
Summer/Winter ve iki süreçli NetSmoke bağlantısı hatasızdı. NetSmoke oynanış kanıtı değildir.

- [ ] Gecikmeli mağaza ile iki oyunculu PIE'de kayıt onayı beklerken bir oyuncuyu savaşa
  sok: herkes Summer'da kalmalı ve savaş bittikten sonra yeni geçiş çalışmalıdır.
- [ ] Aynı bekleme penceresinde ölüm, respawn/pawn değişimi, bağlantı kopması ve yeni
  oyuncu katılımını ayrı ayrı dene: kayıt kümesi değişmişse toplu geçiş iptal edilmelidir.
- [ ] İki oyuncu da canlı ve savaş dışı kalırsa normal Summer/Winter geçişi çalışmalıdır.

## Ertelenen geçiş kaydının yazım hatası — 2026-10-07

Geçiş bekleme penceresinde ertelenen ek kayıt yazılamazsa artık harita geçişi iptal
edilir. Başarısız yazım bekleyen işareti silmez; sonraki başarılı kayıt temizler.
Headless test iki flush hatası enjekte etti: iptal, temiz geçiş kilidi, korunan kayıt
işareti ve başarılı yeniden kayıtta temizleme geçti. Gerçek disk arızası/bulut mağazası
ve iki oyunculu PIE oynanmadı; anlık görüntü sonrası XP/altın penceresi hâlâ açıktır.

- [ ] Gecikmeli mağazada iki oyuncu ile geçiş sırasında ek kaydı ertele, bu yazımı
  başarısız yap: herkes kaynak haritada kalmalı ve geçiş kilidi temizlenmelidir.
- [ ] İptal sonrası tekrar yazım da başarısız olsun: bekleyen işaret korunmalı; mağaza
  düzelince `MMOSave` veya otomatik kayıt başarılı olup işareti temizlemelidir.
- [ ] Yazım hatası giderildikten sonra yeni Summer/Winter geçişi normal çalışmalıdır.
- [ ] İptal sonrası disk/bulut yazımı hâlâ başarısızken `MMOSave` dene: "Karakter
  kaydedilemedi" mesajı görülmeli, "bölge geçişi sürüyor" mesajı gösterilmemelidir.

## Parti birleştirmede lider yetkisi — 2026-10-07

Davet edilen mevcut grubun lideri olmalıdır; davetçinin de grubu varsa o grubun lideri
olmalıdır. Yetki kabul anında tekrar denetlenir. Sıradan üye partisiz oyuncu davet
edebilir. Headless sunucu yolunda 10 senaryo geçti; gerçek istemci RPC/HUD/replikasyon
oynanmadı. Dört oyunculu test gereklidir; iki oyuncu ile iki mevcut grup kurulamaz.

- [ ] Dört oyuncuyu A+B ve C+D gruplarına ayır. A/C liderken A'nın D'ye, B'nin C'ye
  daveti reddedilmeli; üyelik/liderlik iki tarafta değişmemelidir.
- [ ] A, C'ye davet gönderdikten sonra A ayrılsın; C kabul edince eski A grubundaki B
  taşınmamalıdır (A partisizse kendi adına C grubuna katılması geçerlidir).
- [ ] A, C'ye davet gönderdikten sonra C ayrılsın; C'nin eski grubundaki D taşınmamalıdır
  (C partisizse kendi adına A grubuna katılması geçerlidir).
- [ ] İki lider hâlâ yerindeyken birleşmeyi kabul et: dört oyuncu bütün istemcilerde
  aynı üyeleri ve davetçi liderini görmelidir; HUD güncellenmelidir.
- [ ] Bir üyenin solo oyuncu daveti normal çalışmalı; eski lider korunmalıdır. Toplam
  dokuz kişiye taşma reddedilmeli; sekiz kişinin listesi simetrik kalmalıdır.

## Bölge geçişi callback bütünlüğü — 2026-10-07

Headless test gerçek GameMode callback girişini 9 senaryoda denetledi. Fixture pending
karakter kümesini kurar; gerçek bulut mağazası, iki oyunculu gecikmiş callback veya
motorun yükleme fazı oynanmadı. Başarı onayı sonrası ikinci hata aynı oyuncunun onayını
geri almamalı; eski/yabancı/geç callback bariyere dokunmamalıdır.

- [ ] Gecikmeli mağaza ile iki oyunculu PIE'de A başarı, A ikinci hata, B başarı sırasını
  uygula: ikinci A sonucu kilidi açmamalı; B tamamlanınca tek geçiş başlamalıdır.
- [ ] A'nın ilk sonucu hata olsun: herkes kaynak haritada kalmalı; kilit/zamanlayıcı
  temizlenmeli, yeni geçiş isteği kabul edilmelidir.
- [ ] İptal sonrası yeni isteği başlatırken eski isteğin sonucunu gönder; yeni bariyer
  bozulmamalıdır. Ayrılmış pawn için gelen başarı da ilerleme olarak sayılmamalıdır.
- [ ] Motor geçişi başlattıktan sonra aynı isteğin başarı/hata sonucunu tekrar gönder:
  kaynak GameMode geçiş kilidi erken açılmamalı, ikinci ServerTravel başlamamalıdır.

## Klan davetinde kabul anında lider yetkisi — 2026-10-07

Kabul anında davetçi aynı klanda ve hâlâ lider olmalıdır. Yetkili asıl/vekil lider
davetleri çalışır; yetkisini kaybeden üyenin eski daveti reddedilip tüketilir. Hata
düzeltme öncesi UE'de üretildi; sonrası izole sunucu yolunda 9 senaryo geçti. Gerçek
yeniden bağlantı, sohbet RPC, rol replikasyonu/HUD oynanmadı; en az üç oyuncu gerekir.

- [ ] A uzak istemci, B host, C diğer istemci olsun. A klan lideri, B üye iken A çıksın;
  B vekil lider olmalı. B, klansız C'ye davet göndersin ve C kabul etmeden A geri girsin.
  B'nin liderliği düştükten sonra C'nin `/klan kabul` komutu reddedilmeli; C klansız,
  A lider, B sıradan üye kalmalıdır. Eski davet tekrar kabul edilememelidir.
- [ ] Asıl lider A yeni davet göndersin: C kabul edip üye olarak katılmalı; isim plakası,
  klan listesi ve sahibin lider bayrağı tüm ilgili istemcilerde doğru görünmelidir.
- [ ] A gelmeden, yetkili vekil B'nin daveti kabul edildiğinde normal üyelik kurulmalıdır.
- [ ] Davetçi ayrılsın veya bağlantısı kopsun: bekleyen davet kabul edilmemeli. Üyelik
  değişimi kaydet/yeniden giriş sonrasında korunmalıdır.

## Bölge geçişinde ilk snapshot sonrası ilerleme — 2026-10-07

Tüm ilk kayıt onayları gelince `ServerTravel` öncesi her oyuncunun son durumu tekrar
yazılır. UE headless testinde ek kayıt isteği olmadan kazanılan XP/altının diskte
korunması doğrulandı; test ilk yerel yazımın onayını düşürerek beklemeyi kurar.
Gerçek gecikmeli mağaza ve iki oyunculu PIE oynanmadı.

- [ ] İki oyuncuyla gecikmeli kayıt onayı beklerken pasif parti XP'si, altın, görev
  ilerlemesi ve hareket değişsin. Ek elle kayıt olmadan geçiş tamamlandıktan sonra
  yüklenen değerleri son kaynak harita durumuyla karşılaştır.
- [ ] İlk kaydı erken tamamlanan oyuncu, diğer oyuncuyu beklerken elle kayıt yapsın;
  ardından yeniden XP/altın kazansın. Son yazım bu ikinci değişimi de korumalıdır.
- [ ] Son snapshot yazımı başarısızsa kimse hedef haritaya geçmemeli; iptal ve başarılı
  tekrar kayıt sonrasında güncel değerler korunmalıdır. Disk/bulut arızası oynanmadı.
- [ ] Son yazım ile pawn yok oluşu arasındaki oynanışı ayrıca ölç: bu kalan pencere
  kapanmadı. Motor yükleme hatası, dedicated server ve bulut yazım sırası/iptal açık.

## Ölüyken görev ödülü ve diriliş sonrası tek teslim — 2026-10-07

Sunucu ölü karakterin görev kabul/teslimini reddeder. Tamamlanmaya hazır görev aktif
kalır; XP/altın/iksir verilmez. Dirilişten sonra ödül bir kez ve bütün olarak alınır.
UE izole testinde 8 kontrol geçti; ölüm bayrağı/ilerleme testte kuruldu. Gerçek ölüm,
sahip-istemci RPC, HUD ve kalıcılık PIE'de oynanmadı.

- [ ] Uzak istemci Q_Goblins'i bitirsin; ödülü almadan Doran yakınında ölsün. Ölüyken
  teslim isteği reddedilmeli; görev aktif, XP/altın/iksir değişmeden kalmalıdır.
- [ ] Dirilsin ve NPC'ye dönsün: 200 XP, 120 altın ve 3 can iksiri bir kez verilmeli;
  tekrar teslim ek ödül vermemelidir. Seviye atlandıysa XP eşiği düşümünü hesaba kat.
- [ ] Ölü istemcinin yeni görev kabulü de reddedilmeli; diriliş sonrası normal kabul
  çalışmalıdır. NPC'den uzak teslim ve dolu çanta/yığın kontrolleri korunmalıdır.
- [ ] Kaydet/yeniden giriş: ölü teslim denemesi görevi tüketmemeli; başarılı teslim
  tamamlanmış listede kalmalı ve ödül tekrar alınamamalıdır. Host ve uzak HUD'u karşılaştır.

## Ölüyken ganimet ve rezervasyon bitimi — 2026-10-07

Sunucudaki ortak toplama kapısı ölü karakteri reddeder; altın/sarf/ekipman değişmez,
ganimet normal ömrü boyunca yerde kalır. UE izole testinde 10 kontrol geçti; test
ölüm bayrağını/bitmiş rezervasyonu kurar ve girişleri senkron çağırır. Gerçek fiziksel
overlap listesi, rezervasyon timer'ı, sahip-istemci HUD ve kalıcılık oynanmadı.

- [ ] Uzak oyuncu altın, iksir ve ekipman ganimetinin üzerinde/yakınında ölsün. Ceset
  overlap'i envanteri değiştirmemeli ve ganimeti tüketmemelidir. Host ile karşılaştır.
- [ ] Rezervasyon süresi ceset overlap alanındayken dolsun: zamanlayıcı ölü oyuncuya
  grant vermemeli; alandaki canlı oyuncu süre sonunda mevcut kurallarla toplayabilmeli.
- [ ] Ganimet ömrü bitmeden dirilip yeniden overlap oluştur: hak sahibi canlı oyuncu
  üç türü tam ve bir kez almalı; yinelenen overlap ilave eşya/altın vermemelidir.
  Dirilişte otomatik tekrar deneme eklenmedi; gerekirse alandan çıkıp yeniden gir.
- [ ] Dolu ekipman çantası ve dolu/kısmen dolu iksir yığını ganimeti eski kurallarla
  korumalıdır; rezervasyon bitmeden hak sahibi olmayan canlı oyuncu alamamalıdır.
- [ ] Kaydet/yeniden giriş: ölü deneme bakiye/çanta/yığını artırmamalı, canlı toplama
  sonrasındaki kazanım bir kez kalıcı olmalıdır. Normal ganimet ömrü uzatılmadı.

## Saf iksir faydası ve ortak kullanım beklemesi — 2026-10-07

Can/mana zaten doluysa ilgili saf iksir harcanmaz; ortak cooldown başlamaz. Etki
parşömenleri tam can/manada kullanılabilir. İzole UE testinde 11 kontrol geçti;
gerçek uzak istemci, gecikme/HUD ve kayıt kalıcılığı PIE'de oynanmadı.

- [ ] Host ve uzak istemcide tam can ile büyük can iksirine, tam mana ile normal ve
  büyük mana iksirine envanterden tıkla/G kullan: adet düşmemeli, dolu mesajı görünmeli.
- [ ] Can dolu, mana eksikken büyük can iksirini reddettir; hemen mana iksiri kullan.
  Reddedilen istek beklemeyi harcamamalı, mana tek iksir kadar yenilenmelidir.
- [ ] İhtiyaç anında büyük can/mana iksirleri bir adet tüketmeli; hemen ikinci iksir
  ortak cooldown nedeniyle reddedilmeli. Gecikmeli uzak istemciyi ayrıca dene.
- [ ] Tam can/manada Ofke/Hiz/Koruma/Yenilenme parşömenleri çalışmalı; etki yenileme,
  ölümde ret ve normal can iksiri davranışı korunmalıdır.
- [ ] Kaydet/yeniden giriş: reddedilen iksir adetleri korunmalı, geçerli kullanımın
  harcadığı tek adet kalıcı olmalıdır. Mana havuzu sıfır olan fixture'da mana harcanmamalı.

## Kayıtlı ekipmanda seviye şartı ve kayıpsız kurtarma — 2026-10-07

Bilinen kayıtlı ekipman, karakter seviyesi yetmiyorsa bonus vermeden çantaya veya
depoya taşınır. UE izole testinde 11 kontrol geçti; FillSaveData/ApplySaveData turu
bellekte yapıldı. Gerçek .sav/JSON yeniden giriş, RPC, HUD ve replikasyon oynanmadı.
Denemelerde gerçek oyuncu kayıtları yerine ayrı test karakteri ve kayıt yedeği kullan.

- [ ] Lv1 test kaydında kuşanılmış LongSword (Lv3) ve PlateArmor (Lv5) ile yeniden
  gir: iki slot boş, saldırı/savunma bonusu yok; eşya kimliği, + seviyesi ve nadirliği
  çantada korunmalıdır. Uzak istemci ile sunucu değerlerini karşılaştır.
- [ ] Çanta doluyken aynı yükleme ekipmanı depoya taşımalı. Depo da doluysa mevcut
  kayıpsız kurtarma politikası kapasiteyi aşabilir; eşya silinmemeli. Yeni depolama
  kapasite kuralı korunmalı; kurtarılan eşyayı alıp kaydet/yeniden girerek karşılaştır.
- [ ] Lv3 LongSword ve Lv5 PlateArmor yüklendiğinde normal kuşanım ve bonus çalışmalı;
  düşük seviyede elle yeniden kuşanma RPC'si reddedilmeli. HUD güncellemelerini izle.
- [ ] Yanlış slottaki bilinen ekipman geri taşınmalı. Veritabanında artık bulunmayan
  eşyanın kimliği kayıtta korunmalı ve bonus üretmemeli; normal Lv1 LeatherArmor
  davranışı korunmalıdır. Aynı InstanceId bulunan fixture'da kimlikler ayrılmalıdır.
- [ ] Kurtarılan ekipmanı kaydet/çık/yeniden yükle: ikinci yükleme eşya çoğaltmamalı
  veya silmemeli. Bu kontrol kayıt imzası/hesap doğrulaması sağlandığı anlamına gelmez.

## Satıcı ekonomi kapasite ve yetki kapıları — 2026-10-07

Mevcut kurallar izole UE testinde 14 kontrol ile geçti. Test sunucu metotlarını
doğrudan çağırır; gerçek RPC, gecikmeli HUD ve kayıt/yeniden giriş oynanmadı.

- [ ] Host ve uzak istemcide satıcı yakınında tek ekipman al: doğru fiyat düşmeli,
  tek eşya gelmeli. Çanta 12 doluyken tekrar dene: altın ve eşya sayısı değişmemeli.
- [ ] Test karakteri satıcı yakınında ölsün veya açık dükkanla uzaklaşsın. Ekipman,
  can/mana iksiri alış ve ekipman satış istekleri sunucuda reddedilmeli; istemci
  görüntüsü son sunucu durumuna dönmeli. Diriliş/yaklaşma sonrası normal işlem çalışmalı.
- [ ] 999 dolu iksir yığınında alış ucret almamalı. 998 adette çoklu alış yalnız
  bir adet ekleyip bir adet fiyat düşürmeli; can ve mana iksirini ayrı dene.
- [ ] 64 farklı yığında yeni iksir türü alınamamalı; mevcut yığının boşluğu
  doldurulabilmeli. Yetersiz altın/bozuk tanım isteği altın veya eşya üretmemeli.
- [ ] 200 ms gecikmeyle aynı ekipmanı hızlı iki kez satmayı dene: aynı InstanceId
  yalnız bir kez satılmalı. Kaydet/yeniden girişte bakiye ve çanta aynen korunmalı.

## Görev kabul/teslim telemetrisi — 2026-10-07 (PIE açık)

UE'de izole sunucu fixture'ı 8 görev kuralını ve 2 başarılı kabul / 1 teslim
logunu kontrol etti; ölü, eksik, uzak ve tekrar teslimler olay üretmedi.
Fixture geçici PlayerController ile aynı senkron karede çalışır; ilk kayıt
yükleme timer'ı çalışmadan aktörler yok edilir, oyuncu kaydı okunmaz/yazılmaz.
Bu kontrol gerçek istemci RPC'si, HUD veya Lv1–30 oynanış ölçümü değildir.

- [ ] Listen server + uzak istemcide NPC'den görev al/teslim et: her başarılı
  işlem sunucu logunda tek `quest_accept`/`quest_claim`; istemci logunda kopya yok.
- [ ] Ölü, menzil dışı, tamamlanmamış veya zaten teslim edilmiş görevi dene:
  görev/ödül korunmalı, başarılı işlem telemetrisi oluşmamalı.
- [ ] `PlaytestMetrics.py <sunucu_logu> --who <ad>`: kabul→teslim süresi makul;
  seviye atlatan ödül teslim öncesi seviyede, sonraki altın yeni seviyede;
  görev altını iki kez sayılmıyor.
- [ ] Önceki oturumdan aktif görev yükleyip teslim et: kabul satırı bu logda
  yoksa süre `?`/eşleşmeyen; sıfır saniyelik görev diye gösterilmiyor.
- [ ] Seviye tavanındaki teslim `capped=1`; nominal XP net bakiye diye
  yorumlanmıyor. Gerçek 20 dk solo/parti ölçümü ve `first_*` olayları açık.


## Kayıttan stat/XP/puan bütçesi — 2026-10-07 (PIE açık)

`TestStatsSaveBudget.ps1`: **15 UE kontrolü** geçti. Dört sınıfta geçerli Lv10
stat dağılımı korunur; aşırı/negatif stat, XP ve harcanmamış puan sınırları,
geçersiz sınıfın Warrior bütçesi, bellek roundtrip'i, VIT harcarken can/mana
korunması, geçersiz harcama ve tekrarlanan sınıf iadesi sınanır. Oynanış kuralları
zaten mevcuttu; bu ekleme regresyon testidir. Fixture controllersizdir ve
`ApplySaveData`/`FillSaveData` kullanır; gerçek dosya ve istemci RPC testi değildir.

- [ ] Dört sınıfta statları dağıt, kaydet/çık/yeniden gir: seviye, XP, dağılım ve
  harcanmamış stat/skill puanları sunucu ile sahip istemcide aynı kalmalı.
- [ ] Can/mana düşükken VIT/INT puanı harca; mevcut can/mana artmamalı, maksimum
  değerler ve kalan puan istemci HUD'ına gelmeli.
- [ ] Sınıf değişimini normal UI yolu ve bekleme kurallarıyla tekrarla; iade edilen
  toplam puan büyümemeli, ücretsiz can/mana yenilenmesi olmamalı. Fixture yalnız
  Stats iadesini kontrol etti; mastery yatırımı/iadesi uçtan uca ayrıca oynanmalı.
- [ ] Gerçek Lv30+ kayıtları da doğrula. İmza/hesap yetkilendirmesi bu testle
  tamamlanmış sayılmaz; aşırı değerli bellek fixture'ı gerçek kullanıcı kaydı değildir.

## Mastery ve sınıf iadesinde toplam puan — 2026-10-07 (PIE açık)

`TestMasteryBudget.ps1`: 12 izole UE kontrolü geçti. Gerçek yükleme sırasıyla
Skill → Stats, geçerli Lv10 dağılımı, bütçeyi aşan/negatif/kısa rütbe dizileri,
Lv1/seviye tavanı, eski beceri/hotbar korunması, sınıf değişimi, bozuk iade,
normal yatırım ve bellek roundtrip'i sınandı. Controllersiz fixture oyuncu
dosyasını okumaz/yazmaz; gerçek ağ RPC'si veya HUD kanıtı değildir.

- [ ] Host ve uzak istemcide Lv10: toplam yatırılmış + harcanmamış puan en fazla
  18; yatırımda biri artarken diğeri azalmalı, rütbe/bakiye HUD'a gelmeli.
- [ ] Puan yatır, kaydet/çık/yeniden gir, sınıf değiştir: yatırım sıfırlanmalı,
  harcanmamış skill puanı 18 olmalı; mevcut can/mana artmamalı. Normal sınıf
  bekleme ve savaş kuralları geçerli kalmalı.
- [ ] Ayrı yedek test kaydında aşırı rütbe/bakiye yükle: Basic → Special0..3
  sırasında bütçeye kırpılmalı; tekrar kaydet/yükle aynı durumu korumalı.
- [ ] Eski mastery alanı boş kayıt: geçerli öğrenilmiş beceri ve hotbar korunmalı.
  İmza/hesap ve LearnedSkills'in mastery maliyetine göre tam denetimi ayrı iş.

## İlk görev kabul/teslim ölçümü — 2026-10-07 (PIE açık)

`TestFirstQuestMetrics.ps1` 6 UE kontrolünde boş pawn/geçersiz olay reddi,
tek kabul, yeni pawn'da tekilliğin korunması, controller aktarımı ve bağımsız
yeni oturumu sınadı. Mevcut 8 görev kontrolü gerçek başarılı kod yolundan
1 ilk kabul + 1 ilk teslim, aynı sid üretti; iki normal kabulden yalnız ilki
ilk olaydır. Senkron fixture süreleri oynanış ölçümü değildir; disk/RPC/gerçek
ServerTravel kontrolü değildir. İlk olay kaydı sunucudadır; replike görev
durumu ve ödül kuralları değişmedi.

- [ ] Host + uzak istemcide iki görev kabul et, birini teslim et: sunucuda her
  controller sid'si için yalnız 1 `first_quest_accept`, 1 `first_quest_claim`;
  istemci loglarında kopya olmamalı. Ölü/uzak/eksik/tekrar teslim üretmemeli.
- [ ] Öl/diril veya pawn değiştir, sonra başka görev al: aynı controller/sid
  altında ilk olay yeniden yazılmamalı. Görev HUD/ödül normal çalışmalı.
- [ ] Gerçek Summer→Winter seamless geçişten sonra olay tekrarlanmamalı;
  controller değişirse sid/start/tekillik taşınmalı. Yeniden bağlantı ise yeni
  controller/sid olarak ayrı görünmeli (kalıcı karakter ilk eylemi değil).
- [ ] Logu `PlaytestMetrics.py` ile oku: ilk süre controller BeginPlay'den
  ölçülmeli; first olayları XP/altın/görev sayısını iki katına çıkarmamalı.
  Önceki oturumdaki aktif görevin ilk teslimi ilk kabulden önce gelebilir.
- [ ] İlk olay olmayan/eski loglarda `?`/bilinmiyor; ilk öldürme/loot/ekipman ve
  gerçek 20 dk sınıf/parti süreleri ayrıca açık.

## Görev ödülünde çanta/yığın kapasitesi — 2026-10-07 (PIE açık)

`TestQuestRewardCapacity.ps1` 10 izole UE kontrolünde dolu 999'luk yığın,
ödülden bir adet eksik yer, tam sığma, 64 farklı yığın sınırı, mevcut yığına
ekleme, dolu 12 ekipmanlık çanta, yer açıp tekrar teslim ve çift teslimi sınadı.
Ret/tekrarda Stats + Inventory + Quest kayıt alanları ve can/mana/ölülük aynı
kalır. Hazır görev doğrudan bellekten kurulur; gerçek av/kabul, disk kaydı,
uzak istemci RPC'si ve HUD doğrulaması değildir. Mevcut kapasite kuralları
değişmedi. Fixture controllersizdir ve aynı çağrıda yok edilir.

- [ ] Host ve uzak istemcide Q_Goblins hazırken can iksiri 999 veya 997 olsun:
  teslim reddedilmeli, XP/altın/iksir ve görev durumu korunmalı; HUD nedeni
  göstermeli. 996'ya düşürüp teslim et: 3 iksir ve tam XP/altın bir kez gelsin.
- [ ] Sarf listesi 64 türle doluyken yeni ödül türü sığmamalı; yer açınca teslim
  edilmeli. Aynı ödül türü zaten varsa yığın sınırına kadar eklenebilmeli.
  Otomatik fixture sentetik türler kullanır; gerçek içerik/HUD kapasitesi ayrıca
  doğrulanmalı, oyuncu kaydına sentetik test türleri eklenmemeli.
- [ ] Q_Boss hazırken 12 ekipmanlık çanta: ret sonrası görev/XP/altın ve bütün
  eşya kimlikleri kalmalı; bir yer açınca GreatSword +0/Common bir kez gelsin.
- [ ] Gecikmeli ağda teslimi tekrar tıkla; yalnız bir ödül ve tamamlanmış görev
  görünmeli. Reddedilen teslimden sonra kaydet/yeniden girip yer aç: hazır görev
  korunmalı. Gerçek dosya ve iki istemcili koşu henüz oynanmadı.

## İlk gerçek ganimet ölçümü — 2026-10-07 (PIE açık)

`TestFirstLoot.ps1` 6 UE kontrolü ve log doğrulaması: altın tavanında sıfır
kazanç olay üretmez; 17 altından yalnız 2 sığınca amount=2, dolu iksir yığınında
ret sonrası bir yer açınca amount=1 ve kalan yerde, dolu ekipman çantasında ret
sonrası yer açınca amount=1. Üç controller/sid için toplam 3 ilk olay; tekrar
toplama yeni olay üretmez. Mevcut 10 ölüm/tek toplama kontrolünde dirilme sonrası
yalnız altın amount=17 olayı; 6 controller kontrolünde pawn/aktarım tekilliği de
geçti. Fiziksel overlap, gerçek ağ ve disk kaydı bu fixture'larda çalıştırılmadı.

- [ ] Host + uzak istemci normal av/ganimet: sunucuda controller sid başına
  yalnız bir `first_loot`; kind/item/amount gerçek teslimi göstermeli. Başlangıç
  paketi, görev ödülü, satın alma ve debug eşya verme ilk loot sayılmamalı.
- [ ] Ölü/rezervasyona yetkisiz/dolu çanta veya yığında reddedilen pickup ilk
  olay üretmemeli. Altın tavanında sıfır gerçek kazanç da üretmemeli; mevcut
  altın tavanı/kırpma oyun kuralı değişmedi.
- [ ] Kısmi iksir toplaması ilk olay olmalı; miktar yalnız sığan adet. Kalan
  pickup'ta yer açıp yeniden toplama veya gecikmeli çift overlap yeni ilk olay
  üretmemeli. İlk ekipman loot'u kuşanma olarak yorumlanmamalı.
- [ ] Öl/diril ve gerçek Summer→Winter seamless geçişte sid/tekillik korunmalı;
  yeniden bağlantı yeni controller/sid. Parser süreyi controller BeginPlay'den
  göstermeli, first olayını altın/XP/görev toplamına tekrar eklememeli.
- [ ] Eski logda ilk loot yoksa `sure=?`/bilinmiyor. İlk kuşanma ve gerçek
  20 dk sınıf/parti süreleri açık; sentetik fixture süreleri gerçek koşu değildir.

## İlk öldürme kredisi ölçümü — 2026-10-07 (PIE açık)

`TestFirstKill.ps1`: **10 UE kontrolü**, gerçek TakeDamage→ApplyDamage→OnDeath→
HandleDeath→XP yolu; 5 sid'de ilk olaylar contribution=2 XP, party=2 XP,
fatal=14 XP, fallback=20 XP ve dirilen oyuncunun fatal=20 XP kredisi. Sonuncu
Lv1→2 yükseltir; ilk olay lv=1 kalır. Parti üyeliği ve stale katkı yalnız
izole fixture'da seed edilir; gerçek davet, gecikme, hareket veya remote RPC
oynanışı değildir. Ölü alıcı/ölümden önce vuruş/ikinci ölüm ve kill yeni ilk
olay üretmez; katkısız çevre ölümü ve süresi geçmiş katkı kredi vermez.
Mevcut 6 controller kontrolü kill için de pawn/aktarım tekilliğini sınar.

- [ ] Host + uzak istemci gerçek mob avında sid başına yalnız bir first_kill
  yazılmalı; mob/mlv/XP/elit doğru, lv ödül öncesi olmalı. Yalnız vuruş
  yapmak olay üretmemeli; seviye yükselten son vuruş eski seviyede raporlanmalı.
- [ ] İki bağımsız oyuncu hasarı paylaşsın: son vuruş fatal, diğer hasar sahibi
  contribution; partiye katılıp hiç vurmayan yakındaki canlı üye party olsun.
  Normal kill adedi ve XP first satırı nedeniyle iki kez sayılmamalı.
- [ ] Katkı sahibi çıkış yaptıktan sonra DoT/çevre ölümü: sunucu gerçekten
  yedek öldüren seçerse fallback; geçerli katkı yoksa ilk kredi olmamalı.
  Ölü/menzil dışı veya katkısı süresi dolmuş oyuncunun gerçek davranışını
  kontrol et; fixture mevcut paylaşım kurallarının tüm kombinasyonlarını kapsamaz.
- [ ] Öl/diril, pawn değişimi ve Summer→Winter gerçek seamless geçişte aynı
  controller oturumunun tekilliği korunmalı; yeni bağlantıda yeni sid.
  Controller aktarımı fixture'ı gerçek ServerTravel kanıtı değildir.
- [ ] Eski logda eksik first_kill `sure=?`; sıfır XP kredisi geçerli ama
  son vuruşla aynı anlama gelmez. Gerçek kuşanma ve 20 dk solo/parti süreleri açık.

## İlk başarılı kuşanma isteği — 2026-10-07 (PIE açık)

`TestFirstEquip.ps1`: **8 UE kontrolü**; otomatik başlangıç kuşanması için
native giriş, bellekte ApplySaveData, eskimiş kimlik, seviye şartı, geçersiz
slot, başarılı çanta değişimi/bonus, tekrar ve ikinci slot, bağımsız controller.
2 sid'de yalnız ShortSword instance=4 slot=0 ve LeatherArmor instance=1 slot=1
ilk olayları. Tüm GrantStarterPack dalı veya gerçek kayıt dosyası çalıştırılmadı.
6 controller kontrolü first_equip için de bos pawn/geçersiz payload, pawn
değişimi, doğrudan SeamlessTravelTo ve yeni sid'i sınar. Sentetik süreler gerçek
oyuncu süresi değildir. Kuşanmanın mevcut ölüm/replication kuralları değişmedi.

- [ ] Host + uzak istemci yeni kayıtla doğsun: otomatik Kısa Kılıç/Deri Zırh
  first_equip üretmemeli; mevcut kaydı yüklemek de üretmemeli.
- [ ] Loot veya satın alma ile ekipmanı çantaya al: kuşanmadan olay olmamalı.
  I panelinden ilk başarılı kuşanma sunucu logunda tek first_equip üretmeli;
  item/instance/slot ve controller başlangıcından geçen süre doğru olmalı.
- [ ] Düşük seviyeli, olmayan/eskimiş kimlik ve gecikmeli çift tıklama: retler
  ilk ölçümü tüketmemeli. Başarılı ikinci kuşanma yeni ilk olay üretmemeli;
  eski eşya çantaya dönmeli, bonus ve istemci HUD/slotları eşleşmeli.
- [ ] Öl/diril, pawn değişimi ve gerçek Summer→Winter seamless geçişinde
  tekillik korunmalı; yeniden bağlantıda yeni sid başlamalı. Envanter RPC'si
  gerçek sahip istemciden çalışmalı; fixture doğrudan sunucu çağrısıdır.
- [ ] Eski logda eksik first_equip `sure=?`; ilk loot ve ilk kuşanma ayrı
  sürelerdir. First satırı XP/altın/görev/kill toplamını artırmamalı. Gerçek
  20 dk dört sınıf solo/parti koşuları ve tüm ilk eylemler hâlâ açık.

## Kayıtlı öğrenilmiş beceriler ve mastery bütçesi — 2026-10-07 (PIE açık)

`TestLearnedSkillBudget.ps1`: **12 izole UE kontrolü**. Düzeltme öncesi ilk
kontrol FAIL checks=0: Lv10 War_ShatterBlow öğrenilmiş, Special0=0, bakiye=18.
Sonrasında Special0=4, bakiye=14; geçerli beceri/hotbar korunuyor. Aynı ağacın
bütün becerileri ayrı ayrı ücretlendirilmez; maksimum gereken rütbe maliyettir.
Legacy boş rütbe dizisi taşınır, ücretsiz dört sınıf starter'ı ve Priest'in
Smite tamamlama yolu ücretsizdir. Aşırı rütbede Basic→Special0..3 önceliği
korunur; bütçesiz beceri/hotbar kaldırılır. Roundtrip bellekte, gerçek .sav yok;
stats-first ve skill-first yükleme aynı toplamı verir. Sınıf iadesi bütçeyi aşmaz.

- [ ] Dosyayı yedekleyerek geçerli eski öğrenilmiş-beceri kaydıyla yeniden giriş:
  ücretli beceriler yeterli bütçe içinde korunmalı, minimum ağaç rütbeleri ve
  kalan puan doğru olmalı; yeniden kaydet/girişte ikinci kez puan kesilmemeli.
- [ ] Host ve uzak istemci beceri kitabı, hotbar ve puan paneli aynı sonucu
  göstermeli. Korunan beceri gerçek ServerUseSkill ile çalışmalı; budget repair
  ile kaldırılan beceri RPC/hotbar üzerinden kullanılamamalı.
- [ ] Bozuk kayıtta rütbe + öğrenilmiş beceri gereksinimleri bütçeyi aşarsa
  deterministic Basic→Special0..3 onarımı uygulanmalı; kaldırılan slot boşalmalı.
  Ücretsiz başlangıç becerileri korunmalı; bu, tüm eski kayıtların kayıpsız
  göçüne dair kanıt değildir. Zaman damgalı cooldown davranışı da kontrol edilmeli.
- [ ] Sınıf değişimi sonrası eski ücretli beceriler gitmeli, yalnız kazanılmış
  skill bütçesi iade edilmeli. Kaydın kimliği/imzası ve iki taraflı ticaret
  kalıcılığı ayrı açık işlerdir.

## NPC LLM kota ve tablo kapasitesi — 2026-10-07 (PIE/HTTP acik)

`TestDialogueBudget.ps1`: 25 bellek ici UE kontrolu; once FAIL checks=11,
duzeltme sonrasi PASS. Gercek API/HTTP, butce dosyasi, callback zamanlamasi
ve uzak istemci kullanilmadi. Genel tavan atlama degil, 4096 oyuncu anahtari
dolunca yeni anahtar icin oyuncu kotasinin uygulanmamasi duzeltildi.

- [ ] Izole test Saved dizininde, mevcut butce dosyasini yedekleyerek host ve
  uzak istemcide NPC konus: server mesafe/cooldown kapisi ve cevap yalniz
  isteyen oyuncunun HUD'ina gitmeli. Ret halinde NPC'nin kendi yedegi gelmeli.
- [ ] Kota acik ve izole dosyada 4096 sifir sayacli anahtar varken yeni adla
  konus: sunucu `Oyuncu kota tablosu dolu` reddi, provider istegi/sayac artisi
  olmamali; dosyadaki mevcut ad kotasi dahilinde LLM kullanabilmeli. API
  denemesi saglayici kotasina tabi; sadece ayri test ortaminda dogrula.
- [ ] Gercek baslatma hatasi, baglanti hatasi, HTTP200/bozuk200, HTTP!=200 ve
  timeout: uygun iade/ucret koruma, bir callback ve es-zamanli yuva temizligi.
  Bellek fixture'i bu HTTP siniflandirmasini veya cift callback'i dogrulamaz.
- [ ] UTC gun degisimi, bekleyen eski-gun cevabi, host yeniden acma ve gecici
  dosya kurtarmasi: yeni gunun sayaci iade nedeniyle dusmemeli, kalicilik
  kurallari korunmali. Oyuncu adini degistirme hesap kotasi degildir; coklu
  host ayni Saved dizinini paylasirsa merkezi toplam garanti edilmez.

## 2026-10-07 — İlk kabul edilen sunucu becerisi (PIE açık)

Otomatik `TestFirstSkill.ps1`: 17 izole UE kontrolü ve yedi ayrı controller'da
0..6 türündeki yedi gerçek `UseSkill` çağrısı geçti. Sekiz ret kapısı sonrası
ilk log sırası, metadata, ikinci kullanım, native server cooldown reddi ve
controllerless şifa kontrol edildi. Oturum fixture'ının 6 kontrolü skill için
pawn/aktarım/yeni controller tekilliğini de sınar. Bunlar gerçek ağ RPC testi
değildir; sınıf/seviye/öğrenme fixture'da kurulur, gerçek öğrenme akışı sınanmaz.
Üretilen yedi kullanım ve iki aktarım kaydı ortak parser ile who/saat/payload
yönünden de doğrulandı; sentetik parser testleri 150/150 geçti.

- [ ] Dört sınıfı host + uzak istemcide gerçek hotbar ile kullan: sunucu kabul
  edince sid başına tek `first_skill`; skill/effect/mana DB ve etki öncesi lv ile
  eşleşmeli. İstemcinin iyimser cooldown'u veya ClientSkillRejected olayı sayılmamalı.
- [ ] Önce mana yetersizliği, cooldown, ölü karakter, yanlış/öğrenilmemiş beceri,
  düşük seviye, hub güvenliği ve hedefsiz tek-hedef/alan-hedef reddini dene.
  İlk başarılı kullanımın süresi erken reddedilen istekten başlamamalı.
- [ ] Tam canla şifa ve boş alanda kendine AoE mevcut kabul kuralına göre ilk
  beceri olabilir; HealTarget'ın kendine dönüşü de kabul olabilir. Hasar veya
  şifa faydası şartı yok; bunu oyuncu faydası metriği olarak yorumlama.
- [ ] Öldürücü beceriyle seviye atla: first_skill etki öncesi seviyeyi taşımalı;
  kill/first_kill/levelup ayrı olaylar olmalı, parser toplamları iki katına çıkarmamalı.
- [ ] Respawn/pawn değişimi, gerçek Summer→Winter seamless travel, controller
  değişimi ve reconnect: mevcut sid korunurken tekrar yok; yeni sid için yeni
  ilk beceri. Gerçek süreyi 20 dk koşuda ölç, fixture sürelerini kullanma.
- [ ] Eski logda eksik first_skill `sure=?`; parser `--who` ve tüm ilk eylemler
  gerçek logda eşleşmeli. Ayrı istemci loglarının yokluğu sunucu kanıtı değildir.

## 2026-10-07 — İlk kabul edilen temel saldırı (PIE açık)

`TestFirstBasicAttack.ps1`: 21 izole native UE kontrolü, sekiz controller/sekiz
gerçek `first_basic_attack`. Dört sınıf, ilk başarılı native server çağrısı,
pozitif hasar şartı olmayan sıfır hasarlı kabul, PvP, en yakın moba dönüş,
öldürücü vuruşla tam 200 XP (Lv2, kalan XP100); on ret kapısı, tekrar vuruş/cooldown, controllerless
saldırı kapsamı. Altı controller oturum kontrolü pawn/aktarım/yeni sid için
genişletildi. Native loglar ortak parser'dan geçti; sentetik parser 312/312
assert. Bunlar gerçek kullanıcı girdisi veya ağ RPC teslimi değildir. Sınıf
kurulumu/sahiplenme statları yeniden hesapladığı için fixture sabit saldırı
gücünü bundan sonra kurar; oyun denge hatası olarak yorumlanmaz.

- [ ] Host ve uzak istemcide dört sınıfla normal saldır: sunucu onayında bir
  `first_basic_attack`; target mob/player, gerçek arketip veya literal Player,
  hedef seviyesi ve saldıranın XP öncesi seviyesi doğru olmalı. Client logu,
  animasyon veya input zamanı sunucu kabul zamanı yerine kullanılmamalı.
- [ ] Önce ölü/cooldown/hedefsiz/menzil dışı/hub reddini dene; ilk olay çıkmamalı.
  Geçersiz veya ölü seçili hedef canlı en yakın moba dönerse kabul edilen gerçek
  hedef ölçülmeli. Ret kapısı ilk olay süresini erkenden kaydetmemeli.
- [ ] İki oyuncuda PvP kapalı, aynı parti/klan, hedef hub'da ve doğum koruması
  durumlarını dene. Reddedilen vuruş ölçüm olmamalı; serbest PvP vuruşunda
  `targetid=Player` olmalı, hedef oyuncunun görünen adı yazılmamalı.
- [ ] İlk normal vuruş öldürücü olsun ve seviye artırsın: first_basic_attack
  eski lv ile first_kill/kill/levelup öncesinde; sayaçlar iki katına çıkmamalı.
  İlk beceri ayrı first_skill olmalı, basic_attack yerine geçmemeli.
- [ ] Otomatik saldırı/uzun basma, gecikmeli çift RPC, respawn/pawn değişimi ve
  gerçek Summer→Winter aktarımı: aynı controller sid'de yalnız ilk kayıt.
  Reconnect/yeni controller'da yeni sid; eski karakterin ömürlük ilki sayılmaz.
- [ ] 20 dk sınıf koşusunda gerçek süreyi ölç; eksik eski log `sure=?` olmalı.
  Kabul edilen saldırı pozitif hasarı kanıtlamaz; görsel sayı, HP ve kullanıcı
  girdisi gecikmesi ayrıca doğrulanmalı. Native süreler oyuncu süresi değildir.

## 2026-10-07 — Temel saldırıda tek cooldown kapısı (PIE açık)

`TestMeleeCooldown.ps1`: 89 native UE kontrolü. Yedi sentetik dünya zamanı
(100, 600, 1000, 3600, 86400, 604800, 10000000 saniye), süresi bitmiş RPC,
anlık tekrar, erken istek, mevcut %10 tolerans, doğrudan saldırının tam beklemesi,
iki eşiğin ±1 mikrosaniye/tam sınırı ve retlerin damga davranışı. Dünya saati
aynı frame içinde değiştirilip geri alınır; tick/timer veya gerçek oyuncu kaydı
kullanılmaz. Uzun oturum gerçekten çalıştırılmadı; gerçek uzak RPC teslimi yok.
Komut `-MMOAllowSyntheticWorldTime` olmadan fixture oluşturmadan reddedilir;
bayrak yalnız testin başlatıp hemen kapattığı ayrı süreçte kullanılır. Normal
editöre bu bayrağı ekleme: saat geri alınsa da sentetik HUD yazıları geri alınmaz.

- [ ] Summer'da 100 saniyeden sonra host ve uzak istemciyle normal saldır;
  menzil/geçerli hedef aynıyken cooldown'u bitmiş uzak vuruşlar kaybolmamalı.
  HP ve animasyonları iki pencerede karşılaştır.
- [ ] Paket gecikmesi/jitter ile uzaktaki otomatik saldırıyı dene; erken ve
  aynı karede tekrar istekler çift hasar vermemeli. Sunucu toleransı mevcut
  %10'dur (varsayılan 0,6 sn için yaklaşık 0,54 sn); bu ayrı bir denge değişimi değildir.
- [ ] Host/doğrudan normal saldırı tam 0,6 sn beklemeyi sürdürmeli. Ölüm ve
  hub giriş/çıkışında geçerli saldırı yeniden çalışmalı; ret nedeniyle koruma
  bitmesi, combat'a girme veya ilk kabul metriği olmamalı.
- [ ] Kendisi/uzak/hedefsiz hedef retleri denendikten sonra geçerli hedefe dön;
  cooldown'u geçen deneme gerçek zamanı tüketmeli. Respawn ve Summer→Winter
  geçişi sonrası saldırıyı ayrıca doğrula; sentetik saat testi bunun kanıtı değildir.

## 2026-10-07 — Takas sunucu regresyonu (PIE açık)

`TestTrade.ps1` / `MMOTestTrade`: 79 native kontrol geçti. İki controller'sız
karakter, aynı frame, gerçek teklif/onay sunucu gövdeleri; zaman kapıları özel
damgalarla yaşlandırılır, dünya saati değiştirilmez. İki envanter snapshot'ı
eşya payload'ı, alıcının yeni InstanceId sayacı, altın, kuşanım, depo ve yığınları
karşılaştırır. Fixture oyuncu kaydı yüklemez; completion save çağrıları ilk yükleme
kapısından geçmez. Bu test RPC sahipliği, ağ taşınması, owner-only replication,
HUD/davet veya iki karakter kaydının birlikte kalıcılaşması için kanıt değildir.
Eşya aynasının adedi ve onay bayrakları denetlenir; `TheirOffer.Items` payload'ı
bağımsız karşılaştırılmadı. Runner sabit TEMP log adıyla sırayla çalıştırılır;
eşzamanlı iki TestTrade süreci doğrulanmadı.

- [ ] İki PIE penceresinde davet/kabul; add/remove/altın değişikliklerinin iki
  HUD aynasına ve ortak revizyona ulaşmasını, dört onay bayrağının temizlenmesini
  doğrula. Tek onay ve tekrar tıklaması aktarım yapmamalı.
- [ ] Paket gecikmesi/jitter ile eski revizyon onayı yolla; teklif değiştikten
  sonra reddedilmeli. Yeni teklif görüldükten sonra 1 saniyelik kilit çalışmalı.
- [ ] İki yönde aynı tür/+nadirlikte birden fazla eşya ve farklı altınları takas
  et; alıcının kimlikleri kendi sayacından ayrılmalı, altın/eşya toplamı korunmalı.
  Dolu çantada bir ver/bir al başarılı; yalnız gelen eşya ve tavanı aşan altın reddedilmeli.
- [ ] Teklif sonrası kuşan/sat/upgrade/altın harca; ölüm, 800 birimden uzaklaşma,
  AFK, disconnect ve bölge geçişini dene. İptal meşru ara değişimi geri sarmamalı;
  iki pencere, envanter ve timer/partner kilitleri temizlenmeli.
- [ ] Eski takası iptal edip aynı revizyon sayısına ulaşan yeni takas aç; eski
  HUD/onay olayının yeni oturuma taşınmasını araştır. Başlangıç revizyonu 0'a
  döner; eksik oturum kimliği §55'te giderildi ve eski onay kabulü native UE'de
  yeniden üretildi. Gerçek istemci teslimi/HUD doğrulanmadı.
- [ ] Takasın iki ayrı kayıt çağrısı arasında hata/çökme, ardından iki oyuncuda
  yeniden giriş: duplikasyon/kayıp kontrolü. Native test bunu çalıştırmaz; mevcut
  iki Save çağrısı ortak kalıcı işlem sağlamaz. Synchronous envanter delegate'i
  sırasında mutasyonla savunma amaçlı extraction/delivery rollback dallarını
  ayrıca sınamak gerekir; yeni fixture bu dallara fault injection yapmaz.

## 2026-10-07 — Takas onayını oturum ve görülen revizyona bağlama (PIE açık)

Takas testi şimdi 96 native kontrol: önceki79 +17 oturum kontrolü. Eski takasın
revizyon1 onayı yeni revizyon1 takası onaylıyordu; düzeltme öncesi FAIL80/check81.
Sunucu pawn ömrü içinde artan uint64 oturum kimliği üretir; onay her iki tarafın
session/revision eşliğiyle doğrulanır. HUD son çizdiği weak component/session/revision
çiftini gönderir. Native test bunun sunucu wrapper/gövdesini çalıştırır; Canvas,
fare tıklaması, RPC taşıma, owner-only replication veya gerçek oyuncu kaydı yok.

- [ ] Her iki uçta aynı yeni DLL ile iki PIE oyuncusu: takas aç/iptal/aç,
  yeni takasta eski oturumun aynı revizyonlu onayını gecikmeli gönder. Karşı
  taraf önce onaylı olsa bile eski onay aktarım/tamamlama yapmamalı; yeni çifti
  yeniden görüp onaylayınca takas bir kez tamamlanmalı.
- [ ] Teklif çizildikten sonra, click'ten önce replication güncellensin: HUD
  click'i çizilen eski çifti göndermeli; sunucu reddetmeli. Yeniden çizim sonrası
  fresh onay çalışmalı. 1 sn teklif kilidi ve mevcut onay hız sınırı korunmalı.
- [ ] HUD yaşarken pawn değişsin/respawn/bölge geçişi olsun; yeni pawn'ın sayıları
  eski cache ile çakışsa bile weak component kontrolü eski onayı kullanmamalı.
  Trade draw atlanınca/eski pencere kapalıyken cache temizliğini kontrol et.
  Canvas/pawn/stats eksikliğiyle DrawHUD erken çıkarsa da eski onay cache'i
  kullanılmamalı; bu render/input dalı native fixture'da çalıştırılmadı.
- [ ] Owner-only session/revizyon/teklif her iki sahibine ulaşmalı, üçüncü istemci
  özel takas verisi görmemeli. Paket kaybında cancel/restart birleşebilir; yeni
  session OnRep_Trade bildirimi redraw sağlamalı. İki owner görünümünün aynı
  karede/atomik güncellendiği varsayılmamalı.
- [ ] Console `MMOTradeConfirm` en son yerel snapshot'ı onaylar; HUD draw cache'ini
  kullanmaz. Host ve uzak console davranışını bu semantikle karşılaştır.
- [ ] Eski cancel/X/add/remove/gold tıklamalarının yeni takası etkilemesini ayrı
  dene; §55'te bu RPC'ler session taşımıyordu. Add/remove/gold §56'da düzeltildi;
  aktif cancel/X §57'de session'a bağlandı; gerçek HUD/ağ kontrolü aşağıda açık.
  Aşağıdaki yeni offer ve cancel kontrol listeleri de oynanmalı.
  Reentrant rollback ve iki kaydın ortak kalıcılık/çökme kontrolleri de açık.

## 2026-10-07 — Takas add/remove/altın için görülen teklif (PIE açık)

Native162 kontrol (önceki96 +66) ve normal Editor DLL derlemesi geçti. Eski
altın aksiyonunun yeni takasta kabulü önce FAIL79/check80 ile üretildi. Yeni
sunucu gövdeleri session/revision doğrular; fixture RPC taşıma, input/Canvas,
oyuncu kayıt kalıcılığı veya remote bağlantının sürmesini çalıştırmaz.

- [ ] İki uç aynı yeni DLL: iptal/yeniden aç sonrası eski altın/add/remove
  olaylarını gönder. Yeni takas ve iki HUD teklifi değişmemeli; remove'un hedefi
  yeni teklifte bulunmalı, add eşyası geçerli ve çantada olmalı. Bağlantı sürmeli.
- [ ] Aynı session'da iki taraftan herhangi biri teklifi değiştirsin; önceki
  revizyonla altın/add/remove reddedilmeli. Onaylar/cooldown/güncel aynalar
  reddedilen istek yüzünden değişmemeli; redraw + uygun cooldown sonrası yeni
  aksiyon başarılı olmalı. Hızlı ardışık inputta yeniden çizim deneyimini ölç.
- [ ] Altın reset ve +/- düğmeleri draw token'ını göndersin; delta son çizilen
  altından hesaplansın. Draw ile click arasındaki replication eski inputu yeni
  session/revizyona taşımamalı. Canvas/pawn/stats yokluğu veya replacement pawn
  eski bölgeden add/remove/gold dispatch etmemeli. Native fixture HUD'ı çalıştırmaz.
- [ ] Yeni session revizyon0'da fresh altın/add isteği kabul edilmeli; console
  MMOTradeGold güncel yerel snapshot semantiğini korumalı. Host ve uzak istemci
  aynı güncel teklifi görmeli. Owner-only gizlilik ve büyük uint64 taşımasını ölç.
- [ ] Cancel düğmesi/X §57'de session'a bağlandı; aşağıdaki gerçek input/ağ adımlarını
  çalıştır. Bekleyen davet kabul/reddi §58'de gönderim kimliğine bağlandı;
  aşağıdaki gerçek sohbet/bildirim kontrolleri ayrıca çalıştırılmalı.
  Crash/iki kayıt atomikliği/reentrant rollback önceki açık maddelerdir.

## 2026-10-07 — Aktif takas iptalini görülen oturuma bağlama (PIE açık)

Native182 kontrol (önceki162 +20) ve normal Editor DLL derlemesi geçti. Eski
aktif cancel yeni takası sonlandırıyordu: düzeltme öncesi FAIL145/check146.
Fixture yalnız controller'sız sunucu wrapper/gövdesini sınar; Canvas/input,
Reliable taşıma/ownership, uzak bağlantı veya iki kalıcı kayıt kanıtı değildir.

- [ ] İki uç aynı yeni DLL: takası iptal et/yeniden aç; önceki oturumun iptal
  düğmesi ve X olayını gecikmeli gönder. Yeni takas/teklif/onay/timer korunmalı,
  iki oyuncunun bağlantısı sürmeli. Yeni görünümü çizip iptal edince iki pencere
  ve partner/timer durumu temizlenmeli; altın/eşya aktarılmamalı.
- [ ] Aynı session'da draw'dan sonra herhangi bir oyuncu teklifini değiştirsin:
  eski çizimin iptal/X'i session geçerli olduğu için çalışmalı. Revizyon değişimi
  oyuncuyu takasta tutmamalı. Console MMOTradeCancel güncel yerel session'ı kullanır.
- [ ] DrawHUD/DrawTrade atlanması, Canvas/pawn/stats yokluğu, respawn/pawn değişimi
  ve Summer→Winter sonrası eski hitbox/cache iptal dispatch etmemeli. Native test
  render/input dalını çalıştırmaz; host ve uzak HUD'da ayrı doğrula.
- [ ] İnaktifken verilen davet reddi aktif takas başladıktan sonra ulaşsın:
  aktif takası sonlandırmamalı. Eski aktif cancel takas bittikten ve yeni davet
  geldikten sonra ulaşsın: bekleyen daveti silmemeli. Normal inaktif iptal yalnız
  kendi davetini reddetmeli; owner-only redraw iki tarafta kontrol edilmeli.
- [ ] Daha yeni bekleyen davete gecikmiş inaktif ret/kabul ulaşmasını ayrıca
  araştır: davet generation'ı sonraki §58'de eklendi; gerçek ağ ve eski görünür
  bildirim/yeni replike kimlik sınırı aşağıda açık. Payload/token atomikliği,
  gerçek RPC kayıp/gecikmesi ve kalıcı takas/rollback testleri açık kalır.

## 2026-10-07 — Takas daveti ve sohbet gönderim kimliği (PIE açık)

Native218 (önceki182 +28 invite +8 controller chat) ve normal Editor DLL geçti.
Ek chat testleri üç native controller/pawn, aynı frame ile endpoint ve Say/
OnChatCommitted gövdelerini çağırır. Slate klavye/Enter, gerçek remote RPC,
owner-only teslim, paket sırası veya kalıcı oyuncu kaydı doğrulanmadı.

- [ ] İki/üç oyuncu aynı DLL: ilk daveti reddet, farklı oyuncudan yeni davet al;
  ilk kimlikle gecikmiş accept/decline gönder. Yeni davet ve iki tarafın envanteri
  korunmalı. Güncel kimlikle kabul doğru davetçiyle tek takas açmalı; tekrar kabul
  aktif takası değiştirmemeli. MMOTradeAccept/Cancel console ve chat Say/Enter yollarını dene.
- [ ] /trade iptal inaktifken gönderilip aktif takas başladıktan sonra gelsin:
  aktif takası bitirmemeli. Aktif iptal yoldayken takas bitip yeni davet gelsin:
  daveti silmemeli. Güncel aktif session iptali iki pencereyi/timer'ı temizlemeli.
- [ ] Sohbet gönderimi ile teslim arasına respawn/pawn replacement/bölge geçişi
  koy. Yeni pawn aynı sayısal invite/session'a ulaşsa bile eski pawn bağlamı yeni
  takası değiştirmemeli. Null/geçersiz pawn yeni snapshot'a fallback yapmamalı.
  Native test yalnız farklı/null actor bağlamını kurar, gerçek possession geçişi değildir.
- [ ] Bildirim metni inviteId replikasyonundan önce gelsin: eski/0 ID reddi sonrası
  güncel kimlik gelince tekrar kabul çalışmalı; bağlantı sürmeli. Owner-only ID
  yalnız alıcısına ulaşmalı. Yeni RPC alanlarının chat bant maliyetini profiler ile ölç.
- [ ] Eski davet sohbet satırını açık tut, yeni ID replike olsun ve ardından kabul
  yaz: gönderim latest snapshot'ı kullanır. Görülen eski satıra bağlanmış davet
  kimliği bu düzeltmede yoktur; bu ayrı görünüm/UX riski açık kaydedilmeli.
  Genel/parti/klan/fısıltı metni, 200 karakter ve 0,7 sn sınırlaması ayrıca doğrulanmalı.

## 2026-10-07 — Parti daveti ve chat kimliği (PIE açık)

Native46 (eski10 lider rızası +28 component +8 native controller chat) ve normal
Editor DLL geçti. Endpoint/OnChatCommitted/Say aynı frame çağrılır; Slate/Enter,
Reliable taşıma/ownership/owner-only replikasyon veya gerçek kayıt kanıtı yoktur.

- [ ] Üç oyuncu aynı yeni DLL: ilk parti davetini reddet, farklı oyuncudan yenisini
  al; ilk kimlikle gecikmiş accept/decline gönder. Yeni pending, bütün üyelik/lider
  ve envanter korunmalı. Güncel kabul yalnız doğru davetçiyle parti kurmalı;
  duplicate kabul tekrar birleştirmemeli. MMOPartyAccept/Decline ve /parti kabul/ret
  Say/Enter yollarını host ve uzak istemcide dene.
- [ ] Chat gönderimi ile teslim arasına respawn/possession/bölge geçişi koy: eski
  pawn bağlamı, yeni pawn aynı sayısal ID'ye sahip olsa da daveti değiştirmemeli.
  Null/yanlış pawn veya eski/0/ileri ID güncel pending'e fallback yapmamalı;
  istemcinin bağlantısı sürmeli. Native test possession değiştirip aynı nonce kurar; gerçek remote respawn/yükleme yapmaz.
- [ ] Davet yazısı ID replikasyonundan önce gelsin: eski/0 ret sonrası güncel ID
  gelince tekrar deneme çalışmalı. ID yalnız alıcıya replike olmalı; bütün chatteki
  ek parti kimliği maliyeti profiler ile ölçülmeli. 200 karakter/0,7 sn kontrolü,
  genel/party/clan/whisper ve önceki trade komutları ayrıca doğrulanmalı.
- [ ] Eski görünür parti bildirimi dururken yeni latest ID replike olsun: kabul
  güncel snapshot'ı kullanır, eski metnin kimliğine bağlanmaz. Bildirim/ID ortak
  görünümü bu değişiklikte çözülmedi. Mevcut lider/rıza/sekiz kişi ve süre/mesafe
  kapıları ile yeni ret/generation davranışını beraber dene; leave/kick eski
  protokolü kullanır ve ayrı stale oturum incelemesi açık kalır.


## 2026-10-07 — İlk oyuncu ölümü ölçümü (PIE açık)

Native14 aynı frame içinde TakeDamage → stats OnDeath → HandleDeath → gerçek XP
cezasını sınar; 3 first_death/5 death, 6 ret ve 3 controller. Genel oturum6 testi
sentetik dead/health durumu, native pawn değişimi ve doğrudan SeamlessTravelTo
aktarımı kullanır. Timer tick, gerçek seamless travel, save/load, Blueprint death
callback'i, remote ağ/ownership veya Slate/input oynanmış sayılmadı.

- [ ] Host ve uzak istemciyi mob öldürsün: sunucu logunda controller sid için tek
  first_death, aynı ölümün by/gerçek xploss ve lv bilgisi doğru olmalı. İstemci
  aynı ölçümü üretmemeli. XP=0 iken xploss=0 da ilk olay sayılmalı; XP ceza
  miktarından azsa gerçek mevcut XP kadar yazılmalı. Yeni marker ölüm veya
  XP kaybını raporda iki kez saymamalı; normal death satırları toplamları taşır.
- [ ] Gerçek respawn/sifacı dirilişi sonrası tekrar öl: aynı sid'de yeni first_death
  olmamalı, her gerçek ölümün regular death/XP cezası devam etmeli. Canlanma
  koruması ve ölümcül olmayan hasar ilk olayı tüketmemeli. Yeni controller
  oturumunda yeniden ilk ölüm ölçülmeli; kalıcı karakterin ilk ölümü değildir.
- [ ] Native pawn replacement ve aktarımın gerçek uzak respawn/seamless karşılığını
  dene: sid/start/tekillik korunmalı. Death Blueprint'i veya XP callback'i
  ownership/health değiştiriyorsa ayrıca logu incele; hook mevcut pawn eşleşmesini
  ve helper ölü/finite sıfır health'i şart koşar, bu callback senaryosu native
  fixture'da yürütülmedi, olay atlanması/etiket değişimi doğrulanmadı.
- [ ] PvP/DoT/çevresel hasar ile ölümde by yorumunu kontrol et: mob hasar kaynağı
  biliniyorsa arketip, diğer/bilinmeyen kaynak none'dır; PvP oyuncu kimliği veya
  tam killer attribution iddiası yok. DoT sahibi, kaynak yok edilmesi ve Blueprint
  reentrancy bu fixture'ın kanıtı değildir. Eski logda first_death sure=? olmalı;
  sıradan death zamanından first elapsed türetilmemeli. Dört sınıf/20 dk koşusu
  ve gerçek Shipping/Server paket derlemesi açık kalır.


## 2026-10-08 — Klan daveti kimliği ve chat pawn bağlamı (PIE açık)

Native43 (eski9 lider +23 component/helper +11 native controller chat) ve normal
Editor DLL geçti. ID ve pawn guard negatifleri FAIL12 / FAIL38 ile yakalandı,
restore edildi. Aynı-frame native nearest, possession, Say/OnChatCommitted ve
endpoint kanıtıdır; gerçek Slate/Enter, remote RPC/ownership/owner-only teslim,
expiry zamanı, kayıt/yeniden giriş veya Server/Shipping paketi oynanmadı.

- [ ] Üç oyuncu aynı yeni DLL: A klan davetini kabul et, ayrıl; B liderinden yeni
  davet al. Eski A kimliğiyle gecikmiş /klan kabul gönder: B pending/üyelik/lider
  ve envanter korunmalı. Güncel kabul yalnız B klanına katılmalı; duplicate kabul
  üyeliği değiştirmemeli. Aynı davetçi yeni klana geçtiğinde de yeniden dene.
- [ ] Geçerli daveti başka lider ezemesin; aynı lider tekrarında süre uzamasın.
  Gerçek 30 sn expiry sonrası yenileme, davetçinin ayrılması/düşmesi/yetkisini
  kaybetmesi, alıcının ölmesi veya zaten klanda olması kurallarını birlikte dene.
  Native helper testi gerçek cooldown/expiry bekleme kanıtı değildir.
- [ ] Chat gönderim–teslim arasında gerçek respawn/possession/bölge geçişi olsun:
  eski pawn, yeni pawn'da aynı sayısal ID olsa da kabul etmemeli. Null/yanlış
  pawn ve eski/0/ileri ID güncel pending'e fallback yapmamalı, bağlantı sürmeli.
  Güncel Say/Enter kabulü, native possession fixture'ının remote karşılığını
  doğrulamalı. Diğer komutların pawn değişikliği O30 bölümünde; aynı pawn
  üyelik değişimi için generation bağlamı hâlâ açık.
- [ ] Davet metni ID'den önce gelsin; ilk 0/eski ret sonrası kimlik replike olunca
  yeniden deneme çalışmalı. PendingInviteId yalnız alıcıda görünmeli. Eski metin
  dururken yeni latest ID gelirse bu değişiklik eski görünür satıra bağlamaz;
  bildirim/ID ortak görünümü hâlâ açık, yanlış klanı kabul etme UX riskini ölç.
- [ ] Kabul/ayrıl sonrası gerçek save/logout/login üyeliği korunmalı; runtime
  davet kimliği kayda yazılmamalı. Genel/party/clan/whisper/trade chat ve 200
  karakter/0,7 sn sınırı sürmeli; bütün chat trafiğindeki ek uint64 maliyetini
  Network Profiler ile ölç. Gerçek Server/Shipping hedefini ayrı doğrula.


## 2026-10-08 — Parti ayrılmasında üyelik ömrü (O25; gerçek PIE açık)

ECC plan/review/verify ile sınırlı dört C++ dosyası ve mevcut parti runner'ı.
Native kontroller 46'dan 73'e çıktı: 18 üyelik/oturum +9 leave chat kontrolü.
Bunlar aynı-frame component ve native controller fixture'larıdır; gecikmiş
paket, remote ownership, Slate veya property teslim sırası kanıtı değildir.

- [ ] Host + uzak istemci, aynı DLL: A-B partisindeyken ayrılma isteğinin eski
  üyelik kimliğini yakala; sunucuda eski parti dağılsın, A-C kurulsun. Eski
  istek yeni üyeliği kaldırmamalı; güncel ayrılma normal çalışmalı. Aynı A-B
  ikilisinin yeniden kurulması ve eski isteğin tekrarı da korunmalı.
- [ ] Üç oyuncu: B ayrılsın, A-C devam etsin, B aynı partiye yeniden katılsın.
  Ortak session korunurken B'nin kişisel membership kimliği yenilenmeli;
  ilk B üyeliğinden kalan leave yeni B üyeliğini kaldırmamalı. Başka üyenin
  katılması/ayrılması ve lider değişimi sürekli üyelerin güncel leave'ini
  geçersiz yapmamalı; son iki kişinin ayrılması her iki kimliği sıfırlamalı.
- [ ] İki grubun lider onaylı birleşmesinde davetçi grubun session'ı korunmalı;
  taşınan grubun kişisel membership kimlikleri yenilenmeli. Eski taşınan
  grubun leave isteği birleşmiş üyeliğe uygulanmamalı. Partisiz davetçinin
  mevcut gruba katılmasında devam eden üyelerin kimlikleri korunmalı.
- [ ] /parti leave ve /parti ayril: null/eski pawn, 0, farklı üyenin kimliği veya
  eski üyelik güncel sunucu kimliğine fallback yapmamalı, bağlantı sürmeli.
  Gerçek possession/respawn sonrası yeni pawn'ın güncel kimliği eşleşse bile
  eski pawn niyeti uygulanmamalı. Güncel Say ve Enter yolu çalışmalı.
- [ ] Membership/session owner-only teslimi ile Members/Leader atomik değildir.
  Kimlik gelmeden 0/eski istek ret sonrası yeniden deneme çalışmalı. Eski
  görünür roster + yeni latest membership ID yanlış görülen gruptan ayrılma
  UX riski hâlâ açık; bu düzeltme çizilmiş roster snapshot'ına bağlamaz.
  Bütün chat trafiğindeki ek FGuid ve owner-only alan maliyetini ölç;
  genel/party/clan/whisper/trade, 200 karakter ve 0,7 sn sınırı korunmalı.
- [ ] Gerçek logout/destroy/harita geçişi, unpossess öncesi/sonrası parti cleanup
  ayrıca doğrulanmalı. Mevcut Character EndPlay IsPlayerControlled kapısı
  ulaşılmadığında cleanup boşluğu bu işte çözülmedi; native leave testi bu
  lifecycle yolunu kanıtlamaz. Kick'in eski hedef/üyelik bağlamı ayrı açık.
  Session/membership GUID'leri runtime'dır; save şeması değişmedi.


## 2026-10-08 — Parti EndPlay cleanup (O26; gerçek PIE açık)

Parti test runner'ı artık83 native kontrol: önceki73 +10 gerçek native
possession/UnPossess/Destroy/RouteEndPlay lifecycle. Eski gate FAIL75 verdi;
düzeltme PASS83. Bunlar aynı-frame engine çağrılarıdır; gerçek bağlantı
kopması, property teslimi, harita yükleme ve kayıt kalıcılığı doğrulanmadı.
O25'teki EndPlay gate maddesinin parti kısmı native düzeltildi; bu bölüm gerçek
ağ karşılığını tamamlar. Yeni sayılar coverage yüzdesi veya oyuncu sayısı değildir.

- [ ] Host + uzak istemci ikili parti: istemci bağlantısını kapat, host'ta
  server destruction'dan sonra parti Members boş/Leader null olmalı. GUID'ler
  sıfırlanmalı ve HUD kapanmalı; GC bekleyerek düzelmesi yeterli değildir.
  Tekrar bağlanıp fresh parti kurma/leave normal çalışmalı.
- [ ] Üç kişi, ayrılan lider: lider pawn'ını gerçek possession varken destroy et.
  Kalan iki kişi yok edilen actor'u içermemeli, sıradaki üye lider olmalı;
  sürekli üyelerin session ve personal membership kimlikleri korunmalı.
  Güncel leave istekleri çalışmalı; gerçek uzak HUD/XP paylaşımını doğrula.
- [ ] Üç kişi, sıradan üye: gerçek UnPossess sonrası Destroy; mevcut lider ve
  survivor kimlikleri korunmalı. Salt UnPossess, EndPlay olmadan üyeliği
  kaldırmamalı (bu değişiklik controller değişiminde otomatik kick yapmaz).
- [ ] Partili pawn ile gerçek Summer→Winter ServerTravel/seamless/level unload
  ve editor Stop: pending-kill üyeler kalmamalı, istemci crash/ensure vermemeli.
  Native RouteEndPlay(LevelTransition) map loading veya yeni dünyada grup
  devamlılığını kanıtlamaz; yeniden kurulma davranışını ayrıca kaydet.
- [ ] Partisiz NPC ve oyuncu destruction güvenli olmalı. Bu authority cleanup
  istemci EndPlay'de yeni RPC göndermemeli. Diğer üyelerin envanter/altın/görev
  değerleri değişmemeli. Save/friends player-control gate'i korunmuştur;
  destruction sonrası son kaydın yazılması ve presence ayrı açık ölçümlerdir.
  Bu düzeltmeyi save garantisi sayma; yok olmuş PlayerState yerine Default
  yuvaya yazma gibi bir fallback bu işe eklenmedi.


## 2026-10-08 — Possession bitisinde karakter kaydi (O27; gercek PIE acik)

Native26 son-snapshot/timer/tek-yazim kontrolleri, authority ayni-kare local
store fixture'idir. Baseline FAIL4, reset negatifi FAIL9, tekrar-yazim negatifi
FAIL17, arm edilmis nav-retry iptal negatifi FAIL10 ve restore PASS26.
Remote logout/travel veya tum persistence garantisi
olarak sayilmaz; GUID'li gecici slotlar test sonunda silinir.

- [ ] Host + uzak istemci: son autosave sonrasi altin/item/gorev/XP degistir,
  istemciyi kapat; sunucuda Character detach kendi slotuna en son snapshot'i
  yazmali. Ayni adla yeniden giriste stat/envanter/gorev dogru gelmeli;
  Default ve baska oyuncunun kaydi degismemeli. Iki taraftaki gercek loglari tut.
- [ ] Load0,6 sn oncesi veya saved-position nav beklerken disconnect: ilk load
  tamamlanmadigindan var olan kayit varsayilan veriyle ezilmemeli. Yuva read/load
  hatasi ve disk yazim reddini de gercek dosyayla dogrula; hata Warning'i basari
  degildir. Kayit hatasinda dayanıklı retry bu degisiklikte yok.
- [ ] Canli pawn UnPossess/controller degisimi: son snapshot eski PlayerState
  varken yazilmali; detached load/autosave/nav-retry timer'lari kapali kalmali,
  Default yuklememeli/yazmamali. Baska oyuncu possess ettiginde kendi yuklemesi
  tamamlanmadan onceki oyuncunun verisini kaydetmemeli; yeni controller HUD/RPC
  ve load davranisi normal olmali. Salt UnPossess parti uyeligini kaldirmaz.
- [ ] Summer->Winter gercek travel ve editor Stop: committed snapshot yeniden
  yazilmamali; non-destroy EndPlay + nested UnPossessed tek yazar. Gecikmeli
  travel callback, forced detach write ve superseded kararini gercek backend
  ile dogrula. Native RouteEndPlay map yukleme kaniti degildir. Custom seamless
  pawn retention/PlayerState yenileme ayrica kimlik rebinding gerektirir.
- [ ] Isim degisiminde mevcut baska yuvaya son kayit K-1 tarafindan reddedilmeli;
  otomatik old-slot/Default fallback olmamali. Yeni bos ada yazim ve tekrar
  giris ayrica dogrulanmali. Isim hala hesap kimligi degildir.
- [ ] Client EndPlay/UnPossessed yeni save RPC veya yerel disk yazimi baslatmamali;
  NPC destruction kayit yazmamali. Arkadas offline presence player-control gate
  halen ayrica acik; son save'in duzelmesi presence kaniti degildir.


## 2026-10-08 — Arkadas offline lifecycle (O28; gercek PIE acik)

Native24 standalone client endpoint/HUD buffer'i kontrol eder; rendering ve
uzak RPC kaniti degildir. Baseline/load-gate FAIL6, EndPlay-gate FAIL18 ve restore
PASS24. Sentetik abort/teardown/RouteEndPlay actual world travel/shutdown degildir.

- [ ] Host + uzak istemci, karsilikli arkadas olan A/B ve one-way/unrelated C:
  A disconnect veya pawn destruction sonrasi B'de tam bir offline sistem satiri,
  dogru isim ve turuncu kanal2 rengi gorulmeli. A/C'ye presence sizmamali. Gercek
  server/client log ve HUD goruntulerini tut; payload/reflection tek basina yetmez.
- [ ] A hemen ayni adla (buyuk/kucuk harf varyanti da dahil) yeniden girsin:
  B bir online gormeli, ikinci offline/online spam olmamali. Ayni pawn controller
  degisimi/repossess ve post-UnPossess Destroy'i de ayri kontrol et. Yeni possessor
  gercek autoload/arkadas listesi ile devam etmeli; native manual Announce online
  nav/load timers'in gercek teslimini kanitlamaz.
- [ ] Initial-load pending/failed veya disk save reddi sonrasi disconnect:
  previously-online oyuncunun offline'i save basarisina baglanmamali; hic online
  olmamis isim icin offline cikmamali. Kayit hatasi persistence acik maddesi kalir.
- [ ] Gercek Summer->Winter travel/seamless/abort/editor Stop: sahte offline ve
  duplicate online olmamali; abort sonrası normal disconnect offline gondermeli.
  Travel committed olduktan sonraki gercek disconnect penceresi halen acik:
  offline baskilanabilir ve reconnect online 300 sn gecikebilir; sonucu kaydet.
- [ ] 300 sn uzeri oturum, iki world/PIE same-process ve isim yeniden kullanimi:
  process static cache'i world/account izolasyonuna sahip degil. Cache >512 iken
  fresh kayitlar hard cap ile kesilmez; test bu limitleri duzeltti iddiasi tasimaz.
- [ ] Gercek teardown'da crash/ensure/RPC spam olmamali; NPC ve client-side detach
  sunucu offline fan-out veya save RPC baslatmamali. Friends listesi sahibine
  replike olmali, riza ve diger oyuncularin save/party/envanter/altini korunmali.


## 2026-10-08 — DoT kaynak omru (D2/O29; gercek PIE acik)

Native33 direct authoritative ApplyEffect/ProcessEffects ve gercek Actor Destroy
kullanir. Mevcut offensive skills mob-only; fixture normal oyuncu PvP skill yolu
veya remote exploit erisimi kaniti degildir. Baseline FAIL10, raw-pending FAIL32,
refresh FAIL15, final restore PASS33. Elapsed scheduler/HUD ayrica acik.

- [ ] Host + uzak istemci, izinli player->mob Poison: source pawn yok olunca
  sonraki tick can dusurmemeli/anonim kill-XP uretmemeli; effect normal expiry'ye
  gitmeli. Canli kaynak ve mob kill kredi/XP onceki kurali korumali. Source logout,
  pawn replacement/travel ve sadece UnPossess'i ayri kaydet: source actor hayattayken
  mevcut policy yolu korunur; account/controller rebinding bu fix'te cozulmedi.
- [ ] PvP effect yolu sunucu tarafindan yetkili testte uygulanirsa canli kaynak
  PvP-off/party/clan ve attacker-victim hub kapilarina uymali. Kaynak Destroy
  sonrasi damage reddi tum hedeflere uygulansin. Production player-target skill
  yoksa testi normal skill oynanisinda yapilmis gibi isaretleme.
- [ ] Same-ID refresh player->mob/environment ve tersi: tek icon, yeni duration/
  tick accumulator/source/cache; detached mob poison cached hub icinde zarar
  vermemeli, disarida eski davranisi korumali. Null environment ve Regen degismemeli.
- [ ] Iki pending tick arasinda health/death callback source Destroy: ilk kabul
  dogru hasar/attribution verir, sonraki player tick skip; crash/ensure yok.
  Fatal catch-up death/XP/loot bir kez olmali. Native health-delegate callback
  ayni-frame kanitidir; gercek gameplay callback ve actor GC zamanlamasini da olc.
- [ ] Uzak client effect EndServerTime/icon/health ve spawn protection gorunumu,
  normal expired duration ve uzun elapsed tick'ler; network delay/loss altinda
  server mutasyonu ve client HUD tutarli olmali. Profiler sonuclari olmadan
  performans/bant veya tum replication gecti iddiasi yapma.


## 2026-10-08 — Bütün klan komutlarında gönderim pawn bağlamı (O30, PIE açık)

Native73/chat41 actual authority possession switch ve standalone HUD buffer
kontrolleridir. Baseline eski ayrıl isteği replacement klanını boşalttı; /g
kapısı kaldırılınca stale mesaj teslimi yakalandı. Gerçek uzak RPC/Canvas/Slate,
respawn/travel, gecikme/kayıp, Server/Shipping veya save/login yapılmadı.

- [ ] Host + uzak istemci aynı DLL: /klan ayril, at isim, davet, kur ad, liste
  ve /g mesaj gönderimi ile sunucu teslimi arasında pawn değişsin. Yeni pawn
  başka klan/liderlik taşısın: eski/null/başka pawn isteği üyelik/lider/davet/
  envanter/kayıt veya HUD mesajı üretmemeli. Gerçek respawn ve bölge geçişini
  ayrı dene; native doğrudan possession bunun ağ karşılığı değildir.
- [ ] Güncel pawn replike olduktan sonra Say/Enter ile normal ayrıl, yetkili at,
  davet, geçerli yeni klan kurma, liste ve /g çalışmalı; liste yalnız sahibine,
  /g yalnız kendi klanına gitmeli. Native yeni valid kur veya gerçek 3sn bekleme
  yapmadı. Ölü/nonleader ret, 0,7sn chat sınırı ve 3sn davet sınırı korunmalı.
- [ ] Possession teslim sırası/gecikmesi yüzünden ilk sessiz ret oluşursa fresh
  bağlamla tekrar gönderim çalışmalı; eski isteği otomatik yeni pawn'a taşımamalı.
  Genel/parti/whisper ve diğer komutlar ayrı regresyon olarak çalışmalı.
- [ ] Aynı pawn klan ayrıl/rejoin veya hedef isim/pawn/üyelik değişimi: O30
  bunlar için membership nonce sağlamaz. Hedef kick yenidenkatılım, offline
  üye kalıcılığı, roster/bildirim ortak görünümü ve gerçek save/login açık kalsın.


## 2026-10-09 — Arkadaş chat gönderim pawn bağlamı (O31, gerçek PIE açık)

Native28 gerçek authority possession switch + standalone client HUD buffer;
eski sil yeni pawn ve karşı taraf ilişkisini sildi; drop-all mutant fresh rızayı
reddetti. Gerçek remote RPC, replication/Slate/Canvas, respawn/travel ve save/login
oynanmadı. SaveThrottled çağrısı var, fixture initial-load kapısı disk yazısını keser.

- [ ] Host + uzak istemci aynı DLL: /arkadas sil, ekle, kabul/accept, liste ve
  çıplak /arkadas gönderimi ile teslim arasında pawn değişsin. Yeni pawn'ın
  listesi ve ayrı pending daveti olsun; eski/null/başka pawn isteği iki listede
  değişiklik veya HUD satırı üretmemeli. Pending, sonraki fresh kabul için kalmalı.
  Native ret snapshot'ı private pending'i doğrudan karşılaştırmadı; fresh kabul
  doğru bekleyen kişiyi seçti. Gerçek respawn ve bölge geçişinde ayrı doğrula.
- [ ] Güncel pawn istemciye geldikten sonra Say/Enter ekle→hedef rızası→mutual
  liste→sil→yenidenekle akışı çalışmalı. Duplicate kabul ilişkiyi çoğaltmamalı;
  kendi adını/boş adını ekleme ve dolu liste kuralları korunmalı. Liste sadece
  sahibi görmeli; sistem newline satırları okunmalı, presence gizliliği sürmeli.
- [ ] Paket gecikmesi/sahiplik geçişinde ilk sessiz ret sonrası fresh bağlamla
  yeniden dene; 0,7sn ortak throttle eski isteği de sayar. Genel/parti/klan ve
  fısıltı regresyonunu çalıştır; eski komutu otomatik yeni pawn'a taşımamalı.
- [ ] Aynı pawn'da A daveti tüketilip B daveti geldiğinde eski kabul gönder:
  O31 recipient generation sağlamazdı; O32 server observed-generation kapısı
  eklendi. Mesaj/ID ortak görünümünden gelen UX riski hâlâ açık.
  ID/metin ortak görünümü, account kimliği, gerçek reconnect ve offline rıza
  kalıcılığı ayrı iş olarak kalsın. Yeni Server/Shipping hedefini doğrula.


## 2026-10-09 — Arkadaş davet nesli (O32, gerçek PIE açık)

Native53 authority controller/component testidir; controller43, private expiry
ve MAX boundary10. Expiry geriye ayarlanır, gerçek20sn beklenmez. Restore DLL
ve headless HUD buffer gerçek remote replikasyon/Slate testi değildir.
O31'in aynı pawn A→B eksikliği sunucu observed-generation kapısıyla giderildi;
görünen mesaj/ID ortak sunumu hâlâ açık.

- [ ] Host + uzak istemci aynı yeni DLL: A davetini kabul et, ilişkiyi sil,
  aynı pawn B daveti alsın. Eski A ID'siyle gecikmiş kabul/accept B'yi kabul
  etmemeli ve pending B kalmalı; güncel owner ID ulaştığında fresh Enter/Say
  yalnız B ile iki karşılıklı listeyi güncellemeli. Sıfır/yanlış ID de ret.
- [ ] Aynı kişinin daveti süresi dolsun veya tüketilip yeniden verilsin:
  önceki ID kabul edilemesin. Gerçek20sn sona erme ve yeni generation owner
  replication gözlensin; duplicate çağrı iki listeyi çoğaltmasın. O31 eski
  pawn/null/foreign pawn, capacity/ad, liste ve ortak0,7sn sınırını tekrar dene.
- [ ] ID replikasyonu gecikirse sessiz ilk ret sonrası fresh bağlamla tekrar
  çalışmalı. Başka oyuncular alıcının pending ID'sini almamalı; host yerel Say
  ve uzak Enter aynı akışı kullanmalı. Trade/party/clan kabul regresyonu çalışsın.
- [ ] Bildirim metni ile observed ID ayrı teslim edilir: kullanıcı eski A
  satırını okurken B'nin ID'si gelmiş olabilir; generic kabul doğru görülen
  daveti garanti etmez. Mesaj/kimlik/ID ortak sunumu ayrı iş olarak kalsın.
  Rename/controller rebinding, reconnect/offline consent ve gerçek save/login
  açık; yeni Shipping ve Dedicated Server hedeflerini ayrıca doğrula.

O32 otomatik final: linked restore DLL + native53;
`RunAllChecks.ps1 -SkipBuild -FullCompile`: 41/41 exit0, 19 NoPCH CPP /W4
warning0; Summer50/Winter29/error0,
Net1join/1welcome/error0. Bunlar yukarıdaki gerçek PIE kutularını kapatmaz.


## 2026-10-09 — Parti sohbeti gizliliği (O33, gerçek PIE açık)

Native104 authority/HUD buffer kontrolüdür; controller38 + diğer66.
Grup setup pending seed + native accept; gerçek davet/packet gecikmesi değildir.

- [ ] Host + uzak istemci + dış oyuncu: A'da yazılmış gecikmiş `/p` isteği aynı
  pawn A'dan çıkıp B'ye geçtiğinde B'ye gitmemeli; B owner-only ID ulaştığında
  fresh Say/Enter yalnız B üyelerine gitmeli. Sıfır/yanlış ID sessiz ret.
- [ ] Possession değişiminde eski/null/foreign pawn ve güncel replacement ID
  birleşimi fanout yapmamalı. İlk ret throttle'a sayılır; 0,7sn sonrası fresh
  bağlam çalışmalı. Başka oyuncu üyelik ID'sini owner-only dışında almamalı.
- [ ] `/p /w DışOyuncu sır` ve `/p /f DışOyuncu sır` literal parti metni olarak
  yalnız partiye gitmeli; dış oyuncuda whisper veya sistem yanıtı olmamalı.
  Eski üyelik ID'siyle aynı nested gövde hiç teslim edilmemeli.
- [ ] Sürekli üyeye üçüncü kişi katılınca fresh mesaj güncel roster'a gitmeli;
  ayrılan oyuncu yalnız kendi partisiz hint'ini görmeli. `/P`, boş `/p`, normal
  genel sohbet ve `/w`/`/f`, host HUD ve uzak istemci Canvas/Slate tekrar denensin.
- [ ] Roster/metin/gönderim ID atomikliği, account/name/controller rebinding,
  disconnect/travel/yeniden giriş ve eski hedef kick sonrası rejoin ayrı açık
  işlerdir. Shipping ve gerçek Server hedefi ayrıca derlenip çalıştırılmalı.

O33 otomatik final: actual linked restore DLL/PASS104, iki değişen CPP
NoPCH/W4 warning0; RunAllChecks -SkipBuild40/40 exit0. Summer49/Winter31/error0,
Net1join/1welcome/error0. Bu sonuçlar gerçek PIE kutularını kapatmaz.


## 2026-10-09 — Klan üyelik sohbeti/ayrılma bağlamı (O34, gerçek PIE açık)

Native97 = önceki73 +24; controller65 + component32. Test load yalnız component
in-memory ApplySaveData'dır. Native HUD buffer uzak RPC/Canvas/Slate kanıtı değil.

- [ ] Host + uzak istemci aynı yeni build: aynı pawn A→B sonrası eski A ID'siyle
  /g metni B'ye gitmesin; /klan ayril B'den çıkarmasın. B owner-only ID geldiğinde
  fresh Say/Enter mesajı yalnız güncel B üyelerine, leave yalnız kendi üyeliğine
  uygulansın. Sıfır/foreign/unknown ID ret, mevcut ortak0,7sn throttle korunsun.
- [ ] Aynı B'den çıkıp tekrar katılınca veya atılıp tekrar katılınca sender
  üyelik ID'si yenilensin; önceki /g ve ayril tekrar geçmesin. Başka üye katılır/
  ayrılırken ve liderlik aktarılırken continuous sender ID korunmalı; yeni
  roster'a fresh mesaj ulaşmalı. Target kick rejoin bağlamı bu işte çözülmedi.
- [ ] Başarılı /klan kur, davet kabulü ve gerçek kayıt/re-login/load sonrasında
  valid ID owner'a ulaşsın; boş/geçersiz clan kaydı/leave/kick ID'yi geçersiz
  kılsın. Native test başarılı create/disk login çalıştırmadı; Server/Shipping
  ayrı derlensin, owner dışındaki oyuncu ID'yi almamalı.
- [ ] ID teslimi gecikirse sessiz ret sonrası fresh bağlamla tekrar dene;
  klansız kişisel hint, host ve uzak /G, normal party/whisper/genel sohbet
  regresyonu çalışsın. Görünen roster/metin/ID atomikliği, account/name/controller
  rebinding, davet eden generation ve /klan davet/at/liste/kur bağlamı açık.

O34 otomatik final: UHT/linked DLL + native97/chat65;
RunAllChecks -SkipBuild -FullCompile41/41 exit0,19 NoPCH CPP /W4 warning0.
Summer49/Winter29/error0; Net1join/1welcome/error0. Bu sonuçlar gerçek PIE,
başarılı create UI veya disk login kutularını kapatmaz.

## 2026-10-09 — Klan davet eden üyelik bağlamı (O35, gerçek PIE açık)

Native112/component47/controller65 yalnız standalone headless doğrulamasıdır;
source rejoin ve reload doğrudan component in-memory ApplySaveData kullanır.

- [ ] Host + uzak istemci: lider A alıcı B'ye davet göndersin; A aynı pawn'da
  klanından ayrılıp aynı klana tekrar lider olarak katılsın. B'nin eski davet
  kabulü üyelik vermesin; matching ret eski pending snapshot'ı tüketsin. Sonraki
  fresh davet/owner-only nonce geldikten sonra B kabul edebilsin.
- [ ] Gerçek kayıt/re-login, travel ve aynı klanın yeniden yüklenmesi sırasında
  eski source daveti kabul edilmesin. Normal fresh davet, continuous lider ve
  vekil lider geçerli olsun; demoted/ayrılmış/yok olmuş lider reddedilsin.
- [ ] Stale/zero recipient nonce yeni pending'i tüketmesin; source değişince
  doğru nonce ret ve temizleme yapsın. UI davet metni/nonce birlikte görünsün;
  gecikme/kayıp/tekrar istek ve host/uzak Canvas/Slate denensin.
- [ ] Source revocation sonrası davet kilidi mevcut30sn sona erme politikasına
  uysun; timer veya anında unlock bu düzeltmede eklenmedi. Account/controller
  rebinding, leader loss/regain epoch ve gerçek Server/Shipping ayrıca denensin.

O35 final: RunAllChecks -SkipBuild40/40 exit0; değişen CPP NoPCH/W4 warning0,
klan112/chat65, parti104, arkadaş53/presence24/save26/DoT33/trade218.
Parser576/576; Store468/schema2/negative111. Summer49/Winter30/error0;
Net1join/1welcome/error0. Hash151 fark0/exact restore aynı; §72 ayrıntıları.
Bu paket gerçek remote/disk login/PIE/Shipping/Server testini kapatmaz.
