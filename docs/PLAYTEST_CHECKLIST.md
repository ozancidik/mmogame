# Oynayarak Test Kontrol Listesi

Bu dosya, Claude'un otonom (sen yokken) geliştirdiği özelliklerin **senin PIE'de test etmen** gereken maddelerini biriktirir.
Her özellik derlenip commit'lendiğinde buraya eklenir. Test edip onayladıkça maddeleri işaretle (`[x]`).

> Not: Claude kodu yazar, derler ve derlendiğini garanti eder. Oynanış/his/görsel doğrulaması sana kalır.

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

## Onaylanmış (test edilip çalıştığı doğrulandı)

<!-- Test edip onayladığın maddeleri buraya taşı -->
