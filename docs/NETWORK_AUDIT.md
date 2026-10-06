# Ağ Yüzeyi Denetimi (Replikasyon / RPC / Dedicated Server Hazırlığı)

Tarih: 2026-10-04 · Kapsam: yalnız okuma denetimi (kod değiştirilmedi, derleme/PIE çalıştırılmadı).
Model: listen-server, sunucu-otoriter (CLAUDE.md). Hedef: dedicated server'a geçişe hazırlık.

İşaretler: **[doğrulanmalı]** = kod okumasından çıkarım, PIE / `stat net` / Network Profiler ile ölçülmedi.
Satır numaraları 2026-10-04 çalışma ağacına göredir (başka bir oturum dosyaları değiştirirse kayabilir).

Kapsam dosyaları: `MMOTradeComponent`, `MMOClanComponent`, `MMOFriendsComponent`, `MMOInventoryComponent`,
`MMOPlayerController`, `MMOCharacter`, `MMOGameMode`, `MMOMobSpawner`. Soruların gerektirdiği yerlerde
`MMOMob`, `MMOStatsComponent`, `MMOEffectTypes.h`, `MMOQuestComponent`, `MMOSkillComponent`, `MMOPartyComponent`,
`MMOLootPickup`, `MMOTargeting` de okundu.

---

## 0. Özet tablo

| # | Ciddiyet | Başlık | Yer |
|---|---|---|---|
| Y1 | YÜKSEK | Kimlik = oyuncu adı; ad değiştirerek / `?Name=` ile başkasının kayıt dosyası ezilir ya da devralınır | MMOCharacter.cpp:665-710, 407-438, 253-256 |
| Y2 | YÜKSEK | Envanter (Gold, Items, EquipmentBag, EquippedItems) koşulsuz herkese replike: gizlilik sızıntısı + bant | MMOInventoryComponent.cpp:31-34 |
| Y3 | YÜKSEK | Can/Mana her sunucu karesinde değişen float + `ActiveEffects.RemainingTime` her karede dirty: sürekli replikasyon | MMOStatsComponent.cpp:58-70, 698; MMOEffectTypes.h:81-82 |
| Y4 | YÜKSEK | Equip / Upgrade / Sell RPC'leri çanta İNDEKSİ ile çalışır: bayat indeks yarışı, yanlış eşya yanar/satılır | MMOInventoryComponent.cpp:220-264, 297-347, 535-575; MMOHUD.cpp:1397, 1443, 2006 |
| Y5 | YÜKSEK | Mob'larda Inventory/Skill/Quest/Party bileşenleri hâlâ replike; hiçbir yerde NetUpdateFrequency / cull / dormancy yok | MMOMob.cpp:43-45; (grep: proje genelinde yok) |
| O1 | ORTA | `ServerSendChat`: girdi boyutu sınırı, `_Validate`, tüm dallarda throttle yok | MMOPlayerController.cpp:128-141, 326-333 |
| O2 | ORTA | Genel sohbet O(N²) reliable fan-out; `/arkadas liste` ve `/who` toplu reliable; reliable-buffer taşma riski | MMOPlayerController.cpp:388-394, 153-172; MMOFriendsComponent.cpp:217-229 |
| O3 | ORTA | `/klan kur` her çağrıda tüm kayıt dosyalarını senkron okur, cooldown yok | MMOClanComponent.cpp:46-64, 250 |
| O4 | ORTA | Ticaret davetinde aynı davetçi sınırsız tekrar bildirim; Parti davetinde "davet kapma" koruması yok | MMOTradeComponent.cpp:251-260; MMOPartyComponent.cpp:194-196 |
| O5 | ORTA | Arkadaş listesi rızasız: başkasının giriş/çıkış ve seviyesini izlemek mümkün | MMOFriendsComponent.cpp:107-164, 206-231, 233-276 |
| O6 | ORTA | `bTravelSaveCommitted` penceresinde ilerleme kaybı (geri sarma); `bZoneTravelInProgress` sıfırlanmıyor; tek oyuncu toplu geçişi kilitleyebilir | MMOCharacter.cpp:383; MMOGameMode.cpp:118-168 |
| O7 | ORTA | "Dünya duyurusu" NetMulticast yalnız ilgili (relevant) istemcilere gider | MMOMob.cpp:350-366, 702-710 |
| O8 | ORTA | Mob `Tick` her karede, oyuncu olmasa da, O(mob × oyuncu); spawner gereksiz replike | MMOMob.cpp:384-490; MMOMobSpawner.cpp:17 |
| O9 | ORTA | Stats/Quest/Skill/Party alanları da herkese (gizlilik + bant) | MMOStatsComponent.cpp:77-92; MMOQuestComponent.cpp:26-27; MMOSkillComponent.cpp:42-45; MMOPartyComponent.cpp:19-20 |
| O10 | ORTA | `UnequipSlot` çanta sınırını (12) aşar; invariant bozulunca takas/geri alma/ödül reddedilir | MMOInventoryComponent.cpp:285 |
| O11 | ORTA | Dedicated için engeller: Server target yok, admin yolu `IsLocalController`'a bağlı, geri bildirim `IsLocallyControlled`'a bağlı, HUD'a bağlı sunucu kodu | bkz. bölüm 6 |
| O12 | ORTA | Denetim (audit) logu yok: takas, altın, klan, ad değişimi | bölüm 1 |
| D1-D10 | DÜŞÜK | Bkz. bölüm 7 | |

---

## 1. Server RPC envanteri: sahiplik / yetki / rate-limit / girdi doğrulaması

Genel: hiçbir `UFUNCTION(Server, Reliable)` `WithValidation` kullanmıyor (Trade, Inventory, Character, Controller, Party). Tüm RPC'ler bir `PlayerController`
sahipli aktör/bileşen üzerinde olduğu için sahiplik motor tarafından sağlanıyor (uzak istemci başkasının bileşenine RPC atamaz) — bu olumlu.
Eksik olan: geçersiz girdiyi bağlantıyı koparacak şekilde reddetmek (`_Validate`) ve RPC başına hız sınırı.
Hız sınırı deseni zaten var: `UMMOQuestComponent::PassesRpcRateLimit` (MMOQuestComponent.cpp:218-228); diğer bileşenlerde yok.

### Y1 · YÜKSEK · Kimlik = oyuncu adı (kayıt ezme / devralma)
- **Yer:** `AMMOCharacter::ServerSetName_Implementation` (MMOCharacter.cpp:665-710), `GetSaveSlotName` (407-438), `AutoLoadDeferred` (263-273, 255-256 zamanlayıcılar), `SaveCharacterForTravel` (496).
  Klan (`ClanName`, MMOClanComponent.cpp:567-640) ve arkadaş listesi (`Friends`, MMOFriendsComponent.cpp:143-161) de ad anahtarlı.
- **Senaryo A (oturum içi):** Saldırgan `MMOSetName AliceLider` çağırır (`ServerSetName` herhangi bir istemciden kabul edilir; yalnız *şu an çevrimiçi* aynı slotu kullanan oyuncu varsa reddedilir, 679-693). Alice çevrimdışıdır. 60 sn'lik otomatik kayıt (256) saldırganın envanter/klan/konum verisini `MMOSave_AliceLider` dosyasına yazar → Alice'in karakteri kalıcı olarak ezilir. Aynı yolla klan liderinin kaydı da ezilebilir.
- **Senaryo B (bağlanırken):** UE, `AGameModeBase::InitNewPlayer` içinde bağlantı URL'sindeki `?Name=` seçeneğini PlayerState adına uygular **[doğrulanmalı: 5.7'de `ChangeName`/benzersizlik davranışı]**. İstemci `open ip?Name=Alice` ile bağlanırsa `AutoLoadDeferred` (0.6 sn) Alice'in kaydını yükler → kimlik doğrulaması olmadan hesap devralma.
- `MMOCharacter.h:103-109` yorumu bunu zaten kabul ediyor ("login/kimlik sistemi kullanmiyor"); listen-server/arkadaşlar arası testte kabul edilebilir, dedicated/herkese açık sunucuda **engelleyici**. (`docs/ACCOUNT_BACKEND_COMPARISON.md` bu konuyla ilgili görünüyor; okunmadı.)
- **Düzeltme:** Kayıt slotunu istemcinin verdiği addan değil sunucunun doğruladığı kimlikten türet (OnlineSubsystem `UniqueNetId` ya da `PreLogin`'de doğrulanan imzalı token → `SlotId = Hash(AccountId)`); görünen ad ayrı alan olsun. Geçişe kadar asgari önlem: `ServerSetName`'i yalnız `!bInitialLoadComplete` öncesi / yeni karakterde (kayıt dosyası yok) kabul et, mevcut bir kayıt dosyasına denk gelen adı reddet (`DoesSaveGameExist(CandidateSlot)`), `Login`'de `?Name=` seçeneğini yok say, ad değişimine cooldown (10 dk) koy.
- **Ek (D9):** `ServerSetName` hız sınırsız; her çağrı iki `AnnouncePresence` yayını tetikler (701-702) → arkadaşlarına sınırsız "çevrimici/çevrimdışı oldu" spam'i.

### Y4 · YÜKSEK · İndeks tabanlı çanta RPC'leri (bayat indeks yarışı)
- **Yer:** `EquipFromBag(int32 BagIndex)` (MMOInventoryComponent.cpp:220-264), `TryUpgrade(int32 BagIndex)` (297-347), `SellEquipment(int32 BagIndex)` (535-575). HUD bunları `R.Slot` (çanta indeksi) ile çağırır (MMOHUD.cpp:1397, 1443, 2006). Oysa Deposit/Withdraw ve Trade `InstanceId` kullanıyor (doğru desen).
- **Senaryo:** İstemci 120-200 ms gecikmede çanta[2] (+7 kılıç) üzerinde hızlı çift tıklar. İlk `ServerTryUpgrade(2)` item'ı yakar (338: `EquipmentBag.RemoveAt`) → sunucu dizisi kayar; istemcinin eski görünümüyle gelen ikinci `ServerTryUpgrade(2)` artık başka bir item'a (eski çanta[3]) uygulanır ve onu da yükseltir/yakar. Satışta aynı: iki hızlı tık iki farklı item satar. Ayrıca loot/ticaret çantayı arka planda değiştirirken de oluşur (sunucu sırası ≠ istemci görünümü).
- Sunucu yalnız `IsValidIndex` bakıyor (222, 299, 537); "bu istemcinin gördüğü item bu mu" bilgisi yok.
- **Düzeltme:** RPC parametresini `InstanceId` yap (`TakeEquipmentByInstanceId` deseni, MMOInventoryComponent.cpp:700-718); HUD `R.Slot` yerine `Bag[i].InstanceId` göndersin. Yükseltme için ek olarak `ExpectedPlus` parametresi (sunucu uyuşmazsa reddeder) ve 0.3 sn RPC cooldown.

### O1 · ORTA · `ServerSendChat` doğrulaması
- **Yer:** MMOPlayerController.cpp:128-141 (+ 150, 170, 296, 326-333, 357).
- 141: `Message.Replace(...).TrimStartAndEnd()` ham ve **sınırsız uzunluktaki** metin üzerinde çalışır; `Left(120)` yalnız son dallarda uygulanır (296, 329, 357). Komut yolları (`/klan kur <ad>`, `/klan at <isim>`, `/arkadas ...`, `/w` hedef adı) sınırsız `Arg` taşır; `CmdKick` (MMOClanComponent.cpp:482) ve `/w` hata cevabı (MMOPlayerController.cpp:350) bu metni geri **reliable** olarak istemciye yansıtır → ~64 KB'a kadar bant yansıtma (0.7 sn throttle ile ~90 KB/sn, yalnız kendine). 
- Throttle (`LastChatServerTime`) yalnız bazı dallarda güncelleniyor: `/w` kullanım hatası (326, 332) ve boş-mesaj dönüşleri (143, 292, 298) hiç güncellemiyor → sınırsız hata yankısı (her biri bir reliable `ClientReceiveChat`).
- **Düzeltme:** `ServerSendChat`'i `WithValidation` yap: `Message.Len() <= 256` (aksi hâlde `false` → bağlantı kopar). Fonksiyon başında tek yerde `Now - Last < 0.7` kontrolü ve `Last = Now` (başarı/hata fark etmeksizin). `Msg = Msg.Left(256)` hemen parse'tan önce. Kontrol karakterlerini (tab, U+202E vb.) ayıkla.

### O3 · ORTA · `/klan kur` senkron disk taraması
- **Yer:** `SavedClanNameInUse` (MMOClanComponent.cpp:46-64) → `CmdCreate` (250).
- `FindFiles(MMOSave_*.sav)` + her biri için `LoadGameFromSlot` (tam UMMOSaveGame deserializasyonu) **oyun iş parçacığında**. Çevrimiçi klan çakışması yoksa her çağrıda çalışır. Çağrı yalnız chat throttle'ı (0.7 sn) ile sınırlı; `Kur → Ayrıl → Kur` döngüsü veya çevrimdışı bir klan adını tekrar tekrar denemek, 1000 kayıtlı sunucuda her çağrıda yüzlerce ms'lik takılma üretir (DoS).
- **Düzeltme:** Sunucu başlangıcında bir `UMMOClanRegistry` (GameInstance subsystem) kur: ad→üye sayısı; kayıt/silmede güncelle. `CmdCreate` için oyuncu başına 30 sn + küresel 1 sn cooldown. (Kalıcı depo veritabanına taşınınca doğal çözülür.)

### O4 · ORTA · Davet spam / kapma
- **Trade:** `ServerInviteNearest` (MMOTradeComponent.cpp:251-260): "başka davetçi varsa ezme" koruması var, ama aynı davetçi için engel yok → aynı kişi her 3 sn'de hedefe yeni sohbet satırı yollar (sınırsız taciz; engelleme/yoksayma listesi yok). Klan'daki "Bu oyuncuya davet zaten gönderildi" kontrolü (MMOClanComponent.cpp:329-334) Trade'e taşınmalı.
- **Party:** `ServerInviteNearest` (MMOPartyComponent.cpp:194-196) hedefin `PendingInviteFrom`'unu koşulsuz ezer (Trade/Klan'daki kapma koruması yok): saldırgan gerçek bir daveti, kurbanın `kabul`'üne kadar kendi davetiyle değiştirebilir. Bu bileşen kapsam dışıydı; yalnız not.
- Trade `Accept/Cancel/RemoveItem` ve Clan komutlarında RPC başına hız sınırı yok (yalnız chat throttle'ı); `ServerAddItem/SetGold` 0.3 sn cooldown'lu (iyi).

### O12 · ORTA · Denetim logu yok
- Takas (altın/eşya), klan kuruluş/atma, ad değişimi, `TryUpgrade` yanması, depo hareketleri için kalıcı ve sorgulanabilir kayıt yok. Dedicated ortamda eşya çoğaltma/dolandırıcılık şikâyetlerini incelemek imkânsız. `TryExecuteTrade` (MMOTradeComponent.cpp:665-675) bir satır `UE_LOG(LogMMOAudit, ...)` + hesap kimliğiyle yazmalı.

### Olumlu bulgular (korunmalı)
- Takas: sahip-yalnız replikasyon, atomik yürütme, yürütmede yeniden doğrulama, `OfferRevision` + 1 sn onay kilidi (MMOTradeComponent.cpp:406-448, 576-676); gönderen kimliği yerine yeni `InstanceId` (MMOInventoryComponent.cpp:729).
- Satın alma adet kelepçesi (443, 488), satıcı/depocu yakınlık doğrulaması (354-372, 582-608).
- `ServerSaveCharacter` cooldown'u (MMOCharacter.cpp:627-638); `ServerLoadCharacter/SetPvP/ApplyBuff/SpawnMob/GiveItem` `IsLocalController` ile host'a kilitli.
- NPC diyalog/şifacı RPC'lerinde sunucu tarafı mesafe + throttle (MMOPlayerController.cpp:1166-1257).
- Merkezi hasar girişi `TakeDamage` (MMOCharacter.cpp:756-812): spawn koruması, parti/klan/hub kuralları; tüm beceriler `UGameplayStatics::ApplyDamage` kullanıyor (MMOSkillComponent.cpp:941, 989) — atlayan yol bulunmadı.

---

## 2. Replike alan seçimleri ve gizlilik

| Alan | Şu an | Olması gereken | Not |
|---|---|---|---|
| `Inventory.Gold/Items/EquipmentBag` | herkese (MMOInventoryComponent.cpp:31-33) | `COND_OwnerOnly` | **Y2.** Hileli istemci tüm oyuncuların altınını/çantasını okur; ayrıca her değişiklik tüm izleyicilere gider. |
| `Inventory.EquippedItems` | herkese (34) | Görsel kullanılmıyorsa `COND_OwnerOnly`; kullanılıyorsa yalnız `ItemId` (+`Rarity`) ayrı hafif dizi | `Plus`/`InstanceId` başkasına gerekmez. |
| `Inventory.Warehouse` | `COND_OwnerOnly` (35) | doğru | Depo sızıntısı yok. |
| `Trade.*` | hepsi `COND_OwnerOnly` (MMOTradeComponent.cpp:75-79) | doğru | Karşı teklif sunucuda sahibinin aynasına yazılıyor; üçüncü kişi görmez. |
| `Clan.ClanName` | herkese (MMOClanComponent.cpp:76) | doğru (isim plakası) | Kasıtlı. |
| `Clan.bIsLeader` | `COND_OwnerOnly` (77) | doğru | Lider bayrağı sızmıyor. (Not: `/klan liste` yalnız kendi klanına "[lider]" gösterir, 526-527 — kabul.) |
| `Friends` | `COND_OwnerOnly` (MMOFriendsComponent.cpp:62) | doğru | Liste sızmıyor; asıl sorun O5 (rızasız izleme). |
| `Stats` (Str/Dex/Int/Vit/UnspentStat/UnspentSkill/Exp/Mana/MaxMana) | herkese (MMOStatsComponent.cpp:77-92) | Sahibe: hepsi; herkese: Level, Health, MaxHealth, bIsDead, (görsel efektler) | **O9.** `Mana` için parti çubuğu HUD'a bağlıysa `COND_OwnerOnly`+parti kuralı; HUD kullanımı **[doğrulanmalı]**. |
| `Quest.ActiveQuests/CompletedQuests` | herkese (MMOQuestComponent.cpp:26-27) | `COND_OwnerOnly` | `CompletedQuests` sınırsız büyür; her oyuncunun tamamladığı görev listesi herkese yayınlanıyor. |
| `Skill.LearnedSkills/Hotbar/MasteryRankArray` | herkese (MMOSkillComponent.cpp:43-45) | `COND_OwnerOnly` (`CharacterClass` herkese) | Başkasının build'i okunabilir. |
| `Party.Members/Leader` | herkese (MMOPartyComponent.cpp:19-20) | Üyelere + sahibe | Düşük risk. |
| `AMMOCharacter.bClassChoicePending` | `COND_OwnerOnly` (MMOCharacter.cpp:202) | doğru | |
| `AMMOCharacter.bSpawnProtected` | herkese (201) | doğru (HUD etiketi) | |

### Y2 · YÜKSEK · Envanter herkese replike
- **Senaryo:** İki oyunculu PIE (listen server + client): client'ta `showdebug`/Blueprint ile diğer oyuncunun `Inventory.Gold` değeri okunabilir (ya da hileli client). Hiçbir sunucu kuralı bu bilgiyi korumuyor; saldırgan hedef seçimi için en zengin oyuncuları bulur. Aynı zamanda bant: çanta değişince (özellikle `RemoveAt(0)` sonrası tüm indeksler kayar) dizi yeniden yollanır ve **tüm izleyicilere** gider.
- **Düzeltme:**
  ```cpp
  DOREPLIFETIME_CONDITION(UMMOInventoryComponent, Gold, COND_OwnerOnly);
  DOREPLIFETIME_CONDITION(UMMOInventoryComponent, Items, COND_OwnerOnly);
  DOREPLIFETIME_CONDITION(UMMOInventoryComponent, EquipmentBag, COND_OwnerOnly);
  DOREPLIFETIME_CONDITION(UMMOInventoryComponent, EquippedItems, COND_OwnerOnly); // görsel için ayrı public alan
  ```
  `OnRep_Items` hâlâ sahipte tetiklenir. Listen-server host'un kendi karakteri etkilenmez (otorite). Mob'lar için bileşeni tamamen kapat (Y5).

---

## 3. Reliable / Unreliable ve bant genişliği

### Y3 · YÜKSEK · Sürekli replikasyon üreten alanlar
1. **Can/Mana yenilenmesi:** `TickComponent` her karede `Health` ve `Mana`'yı `DeltaTime` ile float artırır (MMOStatsComponent.cpp:58-70). Sunucu tick 30 Hz **[doğrulanmalı: `NetServerMaxTickRate` ayarlanmamış, varsayılan]** olduğundan, can/mana dolu olmayan HER oyuncunun bileşeni HER karede kirli → tüm izleyicilere ~15 B × 30 Hz ≈ 450 B/sn **oyuncu başına izleyici başına**. 50 oyuncunun yarısı yenileniyorsa istemci başına ≈ 11 KB/sn, bütçe (15 KB/sn) tek başına dolar **[tahmin; Network Profiler ile ölç]**. Ayrıca `OnRep_Health` → `OnHealthChanged.Broadcast` her güncellemede HUD'u tetikler (844).
   - **Düzeltme:** Yenilenmeyi birikimli yap, `Health`'i yalnız ≥1 HP değiştiğinde (ya da 0.5 sn adımlarla) yaz; `Mana` `COND_OwnerOnly`. Alternatif: replikasyon `REPNOTIFY` yerine `int32` yuvarlanmış kopya.
2. **`ActiveEffects.RemainingTime`:** `FMMOActiveEffect.RemainingTime` `UPROPERTY()` (MMOEffectTypes.h:81-82) ve `ProcessEffects` her karede azaltır (MMOStatsComponent.cpp:698) → herhangi bir buff/DoT/slow'u olan oyuncu veya mob için `ActiveEffects` dizisi **her karede** yeniden gönderilir (FName `EffectId` her elemanda tam string olarak serileştirilir).
   - **Düzeltme:** `RemainingTime` yerine `float EndServerTime` (`AGameStateBase::GetServerWorldTimeSeconds` bazlı) replike et, kalan süreyi istemcide hesapla; ya da alanı özel `NetSerialize` ile dışarıda bırakıp süreyi 1 Hz ayrı (veya hiç) gönder.
3. **Küçük kazanımlar:** `AMMOMob::MobName` (FString, MMOMob.cpp:98) `ArchetypeId`'den türetilebilir (replikasyondan çıkar); `UnspentStatPoints/UnspentSkillPoints/Experience` `COND_OwnerOnly`.

### Boyutlar (başlangıç / ilgililik girişi demetinde, kabaca) **[tahmin]**
- `EquipmentBag` ≤12 × ~25 B ≈ 300 B (invariant bozulursa daha fazla: O10); `EquippedItems` 4 × 25 B; `Items` (yığın sayısı DB ile sınırlı) ~20 B/yığın; `Warehouse` ≤24 × 25 B ≈ 600 B (yalnız sahibe); `LearnedSkills`/`Hotbar` ~15 B × birkaç düzine; `CompletedQuests` görev sayısı × ~14 B (yüzlerce görevde KB'lar) — hepsi şu an herkese. Oyuncu başına ilk demet ≈ 3-5 KB; oyuncu başına izleyici başına bir kez (ilgililik her yeniden açıldığında tekrar).

### Reliable / Unreliable değerlendirmesi
- `ClientNotifyXPGain` (`Client, Unreliable`, MMOCharacter.h:283, çağrı MMOMob.cpp:690): **doğru seçim** (yalnız görsel; kaybolursa XP zaten replike `Experience`'tan görünür). Kaydı: parti üyelerine her öldürmede bir paket; küçük. `Amount/Percent` `int32` → `uint16/uint8` yapılabilir (kozmetik).
- `ClientReceiveChat` (`Client, Reliable`, MMOPlayerController.h:47): sohbet satırı için doğru, **ama aynı kanal geçici ipuçları için de kullanılıyor**: saldırı ret nedeni (MMOCharacter.cpp:974, ~2/sn), ticaret teklif değişimi bildirimleri, `NotifyOwner` çağrılarının tamamı. Geçici ipuçları için ayrı `Client, Unreliable ClientShowHint(...)` tanımla; reliable'ı yalnız gerçek sohbet/sistem mesajlarına sakla.
- `MulticastMeleeFX` / `MulticastSkillFX` / `MulticastPlayCreatureAttack`: Unreliable (doğru). `MulticastEliteDeathAnnounce` Reliable (doğru ama bkz. O7).
- Server RPC'lerin hepsi Reliable: düşük frekanslı olanlar için uygun. `ServerAttackTarget` (≈1.6/sn/oyuncu) ve `ServerUseSkill` cooldown'la sınırlı; kabul edilebilir.

### O2 · ORTA · Sohbet fan-out ve reliable-buffer taşması
- **Yer:** MMOPlayerController.cpp:388-394 (genel kanal: tüm `PlayerController`'lara `ClientReceiveChat`), 153-172 (`/who`), MMOFriendsComponent.cpp:217-229 (`CmdList`: N+1 ayrı RPC), MMOClanComponent.cpp:548-554.
- **Senaryo:** 100 çevrimiçi oyuncu, her biri sohbet limitinde (0.7 sn → 1.43 msg/sn): her istemci saniyede ~143 reliable RPC alır; toplam sunucu çıkışı N²: 100×143 = 14.300 RPC/sn (~1.5-2 MB/sn). Paket kaybı/gecikme sıçramasında istemci başına 256 reliable-bunch sınırı aşılırsa UE bağlantıyı **kapatır** ("reliable buffer overflow") **[doğrulanmalı: eşik ve tetikleme koşulu]**. `/arkadas liste` 30 arkadaşla tek karede 31 reliable üretir.
- **Düzeltme:** Genel kanalı bölge/mesafe ile sınırla; alıcı başına saniyede bütçe (ör. 4 satır/sn, fazlası düşer); `/who` ve `/arkadas liste` tek bir birleştirilmiş string (ve `/who` için sayfalama/üst sınır); sohbet "tick'te toplu" tek RPC ile `TArray<FChatLine>`.

### O5 · ORTA · Arkadaş listesi rızasız izleme
- **Yer:** `CmdAdd` (MMOFriendsComponent.cpp:107-164), `CmdList` (206-231), `AnnouncePresence` (233-276; 271).
- **Senaryo:** B, A'yı onay almadan listesine ekler (tek yönlü; MMOFriendsComponent.h:12 "karsilikli onay yok; prototip" diyor). Artık A'nın her girişi/çıkışı B'ye sistem satırı olarak gider; `/arkadas liste` ile B, A'nın çevrimiçi durumunu ve seviyesini istediği an sorgular. A engelleyemez. Ad, kimlik olduğundan (Y1) ad kapma ile devralınan listeler de aynı sorunu taşır.
- **Düzeltme:** Karşılıklı onaylı istek (PendingFriendRequest, Trade/Klan deseni) ya da en azından "yoksay/görünmez" bayrağı; ad yerine hesap kimliği anahtarı.

---

## 4. NetUpdateFrequency / ilgililik / ölçek riski

**Bulgu (Y5):** `grep NetUpdateFrequency|NetCullDistance|SetNetDormancy|bAlwaysRelevant|NetPriority` tüm `Source/` içinde **sıfır sonuç** veriyor; `Config/*.ini` içinde ağ ayarı yok (`NetServerMaxTickRate`, bant genişliği, vb.). Yani:
- `AMMOCharacter`/`AMMOMob`/`AMMOLootPickup`/`AMMONpcBase`/`AMMOMobSpawner` motor varsayılanında (NetUpdateFrequency ≈100, `NetCullDistanceSquared` = 15000² uu) **[doğrulanmalı: 5.7 varsayılanları ve `net.UseAdaptiveNetUpdateFrequency`]**. Haritalar X/Y ±8500 uu (≈17000 uu) → varsayılan ilgililik yarıçapı neredeyse tüm haritayı kapsıyor: pratikte HER oyuncu ve HER mob HER istemciye relevant. Kırpma/optimizasyon etkisiz.
- **Mob bileşenleri:** `AMMOMob` kurucusu yalnız Trade/Clan/Friends bileşenlerinin replikasyonunu kapatıyor (MMOMob.cpp:43-45); `Inventory`, `SkillComponent`, `QuestComponent`, `PartyComponent` mob'larda da replike. Şu an ≈45-55 mob × 5-7 alt nesne × izleyici sayısı; kayıp dizileri (EquippedItems 4 boş eleman, vb.) her mob spawn demetine girer.
- **Spawn/despawn churn:** `TopUp` her 4 sn (MMOMobSpawner.cpp:117-130); ölen mob 2 sn sonra yok edilip yenisi açılıyor (MMOMob.cpp:869) → her yeni mob = aktör kanalı + ~8 alt nesnenin ilk durumu × tüm istemciler.
- **Mob başına iş:** `Tick` her karede (MMOMob.cpp:384) tüm `PlayerController`'ları dolaşır (458-490) → O(mob × oyuncu); yakında oyuncu olmasa da çalışır (uyku/aktivasyon yarıçapı yok).
- **Spawner:** `bReplicates = true` (MMOMobSpawner.cpp:17) ama replike mülkü yok; her istemci için gereksiz aktör kanalı (17 Summer + 11 Winter spawner). `bReplicates=false` yapılırsa seviye içinde yerleştirilmiş/spawn edilmiş aktör istemcide `ROLE_Authority` olur ve `BeginPlay`/`TopUp` istemcide de mob üretir; doğru çözüm: `bReplicates=false; bNetLoadOnClient=false;` (istemcide hiç var olmasın). Spawner'lar GameMode `StartPlay`'de sunucuda `SpawnActorDeferred` ile üretildiğinden istemciye zaten yüklenmiyor; yine de bayrağı kapatmak yeterli **[doğrulanmalı]**.
- **Loot/NPC:** `AMMOLootPickup` (MMOLootPickup.cpp:19) ve `AMMONpcBase` (MMONpcBase.cpp:13) statik aktörler; varsayılan sıklıkta, dormancy yok. `AMMOLootPickup::Tick` (123-135) sunucuda da dekoratif dönüş/salınım yapıyor (dedicated'da gereksiz).

**Öneriler (sırayla):**
1. Mob kurucusunda: `Inventory/SkillComponent/QuestComponent/PartyComponent->SetIsReplicated(false)`; `Stats` kalsın.
2. Mob: `SetNetUpdateFrequency(10-15)`, `SetMinNetUpdateFrequency(2-3)`, `NetCullDistanceSquared = 6000²` (en büyük AggroRange ≈1200); oyuncu: 30-60 Hz.
3. NPC / loot: `NetUpdateFrequency=1`, `DORM_Initial` (+gerekirse `FlushNetDormancy`); loot'a `SetLifeSpan`.
4. Mob AI'sını "yakında oyuncu var mı" ile uyut (ya da `SetActorTickInterval(0.2)` ve oyuncu listesini döngüden önce ızgara/yarıçap sorgusuyla daralt).
5. Oyuncu sayısı 50+ olacaksa Replication Graph / Iris uzamsal filtre değerlendir (`net.RepGraph.PrintAllNodes`).
6. Ölçüm: 2 istemci + 50 mob ile `stat net`, `Net.ListActorChannels`, Network Profiler; hedef <15 KB/sn/oyuncu.

---

## 5. Seamless travel / bağlantı kopması tutarlılığı

- **EndPlay sıralaması iyi:** `AMMOCharacter::EndPlay` önce kaydeder (MMOCharacter.cpp:383-386), sonra `Super::EndPlay` bileşenlerin `EndPlay`'ini çağırır; Trade `EndPlay` takası iptal eder (MMOTradeComponent.cpp:82-90), Clan lider devri yapar (MMOClanComponent.cpp:80-90). Kopmada pawn `Destroy` anında hâlâ possess'li olduğundan `IsPlayerControlled()` doğru **[doğrulanmalı]**.
- **Seamless travel'da pawn yok edilirken `IsPlayerControlled()` hâlâ true mu?** `EndPlay` bloğu (MMOCharacter.cpp:377-403) buna bağlı; false ise parti çıkışı ve kayıt atlanır **[doğrulanmalı: PIE'de iki oyunculu `MMOTravel`, log ile]**. Kayıt zaten `SaveCharacterForTravel` ile alındığı için veri kaybı yok, ama parti dağılması garanti değil.

### O6 · ORTA · `bTravelSaveCommitted` penceresi ve toplu geçiş kilidi
- `RequestZoneTravel` kaydeder (MMOGameMode.cpp:146) → `SetTravelSaveCommitted(true)` (156) → `ServerTravel` (158). Harita değişimi *asenkron*: bayrak true iken `EndPlay` kaydı atlanır (MMOCharacter.cpp:383) ama oyun sürer (mob'lar, loot, görev teslimi, depo, ticaret). Kayıt ile pawn'ın gerçekten yok edilmesi arasındaki saniyelerde olan her şey (altın, XP, ödül, depo hareketi) **geri sarılır**. Çoğaltma değil yalnız geri sarma (her karakter için tutarlı anlık görüntü), ama oyuncu için algılanan kayıp. Takas bu pencerede tamamlanırsa `TryExecuteTrade` iki tarafı yeniden kaydediyor (MMOTradeComponent.cpp:674-675) — tutarlı.
  - **Düzeltme:** Travel başlayınca oyuncu girdisini/RPC'leri kilitle (`bTravelLocked` → Server RPC'ler erken döner, hasar bağışıklığı) ya da `EndPlay` içinde `EndPlayReason == LevelTransition` ise de ikinci kez kaydet (aynı slot, idempotent).
- `bZoneTravelInProgress` yalnız hata yollarında sıfırlanıyor (MMOGameMode.cpp:150, 164); `ServerTravel` true dönüp harita sonradan yüklenemezse bayrak kalıcı true → geçiş sunucu yeniden başlayana dek kapalı. Seamless travel yeni GameMode örneği yarattığından başarı yolunda sorun yok. **[doğrulanmalı]**
- `Character->GetStats()->IsInCombat()` (118-119) tüm oyuncular için aranıyor: dedicated/halka açık sunucuda tek oyuncu mob'a vurarak herkesin geçişini engelleyebilir (griefing). Host-only listen modelinde kabul edilebilir.
- Seamless sonrası yeni pawn 0.6 sn boyunca varsayılan durumda (kayıt yüklenmedi; `bInitialLoadComplete=false` olduğu için kayıt yazılmıyor — iyi). Bu pencerede girilen `/klan`, `/arkadas`, ticaret komutları `ApplySaveData` ile ezilir (MMOClanComponent.cpp:567-640, MMOFriendsComponent.cpp:287-308) — zararsız ama komutlar "başarılı" görünür. İstersen `bInitialLoadComplete` olmadan RPC'leri reddet.
- Bağlantı kopması: Trade ve Klan EndPlay'ı doğru; Friends `AnnouncePresence(false)` yalnız `Destroyed` + `!bTravelSaveCommitted` (MMOCharacter.cpp:399) — doğru. `static GAnnounced` (MMOFriendsComponent.cpp:250) süreç ömrü boyunca büyür, PIE'de sunucu/istemci aynı süreçte paylaşılır (D6).
- Parti, bölge geçişinde dağılır (EndPlay `RequestLeave`, MMOCharacter.cpp:393-396); kalıcı değil — tasarım notu.

---

## 6. Dedicated server geçişi engelleri

### O11 · ORTA · Madde madde
1. **Build hedefi yok:** `Source/` altında yalnız `MyProject.Target.cs` ve `MyProjectEditor.Target.cs`; `MyProjectServer.Target.cs` yok → `-server` paketlenemez.
2. **Admin yolu `IsLocalController()`'a kilitli:** `MMOTravel` (MMOPlayerController.cpp:47), `RequestZoneTravel` (MMOGameMode.cpp:90), `ServerSetPvP` / `ServerApplyBuff` / `ServerSpawnMob` / `ServerGiveItem` / `ServerLoadCharacter` (MMOCharacter.cpp:640-651, 1126-1145, 1163-1176, 1393-1418, 1613-1633). Dedicated'da hiç yerel controller yok → bölge geçişi, PvP anahtarı, test komutları kullanılamaz. Gerekli: sunucu konsolundan (`Exec` GameMode'da zaten var) ya da yetkili (admin listesi / token) bir RPC.
3. **Geri bildirim `IsLocallyControlled()`'e bağlı (uzak istemcilere hiç gitmiyor — listen-server'da da!):** Inventory (MMOInventoryComponent.cpp:172, 392, 402, 415, 424, 453, 463, 472, 498, 514, 523, 550, 570, 620, 665), Party (MMOPartyComponent.cpp:181, 198, 224, 232, 258, 274, 373), Quest (MMOQuestComponent.cpp:178-371), Skill (MMOSkillComponent.cpp:129, 457, 475, 566, 791), Character (MMOCharacter.cpp:498, 596, 705, 835, 928, 1026, 1482-1662), LootPickup (MMOLootPickup.cpp:168 ve **koşulsuz** 183-207). Sunucuda uzak oyuncunun pawn'ı `IsLocallyControlled()==false` olduğundan "Canta dolu!", "Altin yetersiz", "Satin alindi", "Satildi", görev alındı, loot mesajı, "Kaydedildi/Karakter yuklendi", "Oldun! -N EXP", vurma geri bildirimi **yalnız host'a** görünür. Bu bugün listen-server'da uzak oyuncuların deneyimini bozuyor (kod okumasıyla kesin; PIE'de iki oyunculu gözle doğrulanmalı) ve dedicated'da tamamen yok olur. `LootPickup`'ın koşulsuz mesajları ise host ekranında başkalarının toplama olaylarını gösteriyor.
   - **Düzeltme:** Tek bir `AMMOPlayerController::ClientNotify(FText/uint8 Kod, int32 Arg...)` (Unreliable, oran sınırlı) ya da mevcut kanal-2 sohbet satırı; sunucu kodu `GEngine->AddOnScreenDebugMessage` yerine bunu çağırsın.
4. **HUD'a bağlı sunucu kodu:** `AMMOCharacter::TakeDamage` (MMOCharacter.cpp:799-808) ve `UMMOStatsComponent::Heal` (MMOStatsComponent.cpp:281-293) `World->GetFirstPlayerController()->GetHUD()->AddDamageNumber` çağırıyor. Listen-server'da host'a *dünyanın her yerindeki* hasarı gösterir; dedicated'da ilk uzak `PlayerController`'ın HUD'u sunucuda yok (null) → sessizce hiç gösterilmez, iyileştirme sayıları istemcilere hiç ulaşmaz (hasar sayıları `OnRep_Health` ile istemcide üretiliyor, 821-845). Düzeltme: iyileştirme/hasar sayısını `Unreliable` multicast ya da `OnRep` farkıyla üret; sunucu kodunda HUD referansı kalmasın.
   - `MulticastEliteDeathAnnounce` (MMOMob.cpp:350-366) `GEngine->GetFirstLocalPlayerController` kullanıyor — multicast istemcide çalıştığından sorun değil; sunucuda null döner (zararsız).
5. **Disk G/Ç oyun iş parçacığında:** `SaveGameToSlot`/`LoadGameFromSlot` (MMOCharacter.cpp:496, 534), `SavedClanNameInUse` (bkz. O3). 100 oyuncu × 60 sn otomatik kayıt dağınık olsa da takas/klan/arkadaş anlık `SaveCharacter()` çağırıyor. Asenkron kayıt (`AsyncSaveGameToSlot`) ya da DB.
6. **`bAllowPvP = true` varsayılan** (MMOGameMode.h:40, "Test icin varsayilan acik"); halka açık sunucuda `false` olmalı. `bAllowPvP` replike değil: istemci `IsPvPAllowed()` her zaman `false` döner (`GetAuthGameMode` istemcide null) — istemci tarafı kapılar buna güvenmemeli (`AttackTarget` istemci yolu güvenli: yalnız sunucu karar veriyor).
7. **LLM NPC diyaloğu:** `ServerRequestNpcDialogue` yalnız oyuncu başına 2 sn (MMOPlayerController.cpp:1183-1187); sunucu geneli eşzamanlı/dakikalık üst sınır yok → 100 oyuncu × 0.5 istek/sn = ücretli istek patlaması. Küresel token-bucket ekle. Sunucu ortamında API anahtarı yönetimi gerekir.
8. **Slate/HUD bağımlılıkları:** `AMMOPlayerController` Slate (`SEditableTextBox`) kullanıyor; `FSlateApplication::IsInitialized()` koruması var (448) — dedicated hedefinde modül bağımlılıkları (`MyProject.Build.cs`) ve `HUDClass` yüklemesi **[doğrulanmalı: Server target derlemesi]**.
9. **Exec komutları:** `MMOProbeSpawn/Surface` `!UE_BUILD_SHIPPING` içinde (iyi, MMOGameMode.cpp:31, 58). Diğer `Exec`'ler (`MMODamage`, `MMOHeal`, `MMOGainXP`, `MMOGiveItem`, `MMOSpawnMob`, `MMOPvP`, `MMOBuff`, MMOCharacter.h:188-279) Shipping'de de derleniyor; konsol Shipping'de kapalı olsa da `ServerGiveItem/SpawnMob/SetPvP/ApplyBuff` RPC'leri kodda kalıyor. Host-only kilidi nedeniyle uzaktan istismar edilemez; yine de `#if !UE_BUILD_SHIPPING` ile sar (D1).

---

## 7. Düşük ciddiyetli bulgular

- **D1** Debug RPC'leri Shipping'e giriyor (MMOCharacter.cpp:640, 1126, 1163, 1393, 1613): `IsLocalController` kilitli; `#if !UE_BUILD_SHIPPING`.
- **D2** DoT'u uygulayan pawn geçersizleşince (`P.Instigator` weak) `InstController=null`, `DamageCauser=null` → `TakeDamage`'de `Attacker==null` olduğu için parti/klan/hub kuralları atlanır (MMOStatsComponent.cpp:751-756, MMOCharacter.cpp:768-783). Örn. hub'a sığınan kurbanı, saldırganı ayrıldıktan sonra da zehir vurmaya devam eder. DoT instigator'ı `TWeakObjectPtr<AController>` + cache'lenmiş "oyuncu-kaynaklı" bayrağı ile sakla **[doğrulanmalı]**.
- **D3** Satıcı ve `TryUpgrade`: ölü kontrol yok (MMOInventoryComponent.cpp:354-372; depo ölüyü reddediyor, 590-597); `TryUpgrade` NPC gereksinimi/yakınlık istemiyor (297-347) — KO'da NPC gerekir; tasarım kararı değilse ekle.
- **D4** Trade `Accept/Cancel/RemoveItem`, Klan komutları, `UseConsumable/UseHealthPotion` (MMOCharacter.cpp:1473-1594) için RPC başına hız sınırı yok; iksir tüketimi cooldown'suz (oyun dengesi + RPC yükü).
- **D5** Sohbet: genel kanalda mesaj "[Dunya] ..." / "[Klan] ..." öneki taşıyarak sistem satırı taklit edebilir (satır biçimi `Sender: Mesaj`, MMOPlayerController.cpp:433); kontrol karakterleri temizlenmiyor.
- **D6** `static TMap GAnnounced` (MMOFriendsComponent.cpp:250) sınırsız büyür, PIE'de sunucu+istemci aynı süreçte paylaşır.
- **D7** Loot: ilk-gelen alır, sahiplik/kilitlenme yok; çanta doluyken loot yerde kalır (iyi) ama `AddEquipmentInstance` doluysa ödül/loot sessizce kaybolur (MMOInventoryComponent.cpp:170-177).
- **D8** Klan: çevrimdışı üye atılamaz (`CmdKick` yalnız çevrimiçi, MMOClanComponent.cpp:468-484); travel sırasında lider `EndPlay`'i "X artık klan lideri" duyurusunu gereksiz yollar (207) **[doğrulanmalı]**.
- **D9** `ServerSetName` cooldown'u yok (bkz. Y1).
- **D10** `IsInsideHubSafeZone` her çağrıda `TActorIterator<APlayerStart>` (MMOTargeting.cpp:18); `AttackTarget`, `TakeDamage`, `CanHarmPlayer` her vuruşta birkaç kez çağırıyor — PlayerStart'ı `BeginPlay`'de önbelleğe al.

### O10 · ORTA · `UnequipSlot` çanta sınırını aşar
- MMOInventoryComponent.cpp:285 `EquipmentBag.Add(...)` `MaxBagSize` (12) kontrolü yok. Çanta doluyken 4 ekipmanı çıkaran oyuncu çantayı 16'ya çıkarır; bundan sonra `ReceiveTradedEquipment` (723) ve `Withdraw` (684) reddeder, `TryExecuteTrade` kapasite kontrolü (612-618) her takası iptal eder, HUD yalnız ≤12 göstermek üzere boyutlanmış (MMOInventoryComponent.h:78-85) → fazla item'lar erişilemez kalabilir. Düzeltme: `Unequip` çanta doluysa reddet ve ipucu gönder.

---

## 8. Önerilen öncelik sırası

1. **Y2 + O9** — tek satırlık `COND_OwnerOnly` değişiklikleri (Inventory, Quest, Skill, Stats private alanlar): gizlilik + bant, ucuz.
2. **Y3** — Can/Mana yenilenme adımlama + `ActiveEffects.RemainingTime` yerine bitiş zamanı: en büyük bant kalemi.
3. **Y5** — Mob bileşen replikasyonu, `NetUpdateFrequency`/cull, spawner `bReplicates` düzeltmesi, NPC/loot dormancy.
4. **Y4** — `InstanceId` tabanlı Equip/Upgrade/Sell (HUD de güncellenecek): kullanıcı eşya kaybı yaşıyor/yaşayabilir.
5. **O1/O2/O3** — chat `_Validate` + uzunluk + tek throttle, alıcı başına bütçe, klan kayıt kaydı.
6. **O11 (bölüm 6)** — `ClientNotify` kanalı, Server target, admin yolu, HUD'dan bağımsız sayılar.
7. **Y1** — hesap kimliği (dedicated/herkese açık yayın öncesi **zorunlu**); o zamana dek `ServerSetName` kısıtlaması ve `?Name=` yok sayma.
8. **O6, O7, O12** — travel kilidi, dünya duyurusu için GameState/PC döngüsü, denetim logu.

Önerilen PIE doğrulamaları (2+ oyunculu listen-server, ardından `-server` hedefi):
- Uzak istemcide "Canta dolu / Altin yetersiz / Satin alindi / loot" mesajı görünüyor mu (bölüm 6.3).
- İstemcide diğer oyuncunun `Inventory.Gold`'unu `showdebug`/BP ile oku (Y2).
- `stat net` + Network Profiler: 1 oyuncu yenilenirken ve buff'lıyken izleyici başına B/sn (Y3).
- Hızlı çift tık ile `TryUpgrade`/`Sell` + 200 ms gecikme (`p.NetPktLag=200` / `Net PktLag=200`) (Y4).
- `open ip?Name=<mevcut kayıt adı>` ile bağlan (Y1-B).
- Elit boss'u uzak bir oyuncu varken öldür: duyuru uzak oyuncuya ulaşıyor mu (O7).
- İki oyuncuyla `MMOTravel` sırasında pawn `EndPlay` logları ve parti durumu (bölüm 5).

---

## 9. Uygulama durumu (2026-10-04 güncellemesi)

PR [ozancidik/mmogame#1](https://github.com/ozancidik/mmogame/pull/1) değişiklikleri **derlenmedi**. Önceki yerel turda kapananlar: Y2/O9, Y4/O10, Y5, O1, O2, O3.

- **Bu turda (derlenmedi):** D1 (debug RPC'leri Shipping'de erken döner), D3 (ölü karakter satıcıdan işlem ve yükseltme yapamaz; yükseltmede NPC yakınlık koşulu tasarım kararı olarak eklenmedi), D5/D9 (sohbet kontrol ve bidi karakter temizliği), D6 (`GAnnounced` tablosu 512 kayıtla sınırlı), D9/Y1 ad değiştirme soğuması (30 sn, yalnız uzak istemci).
- **Y3 madde 1** (can/mana her karede kirlenir): yenilenme ≥1 puanlık adımlara bağlandı (derlenmedi). Madde 2 (`ActiveEffects.RemainingTime`): `NotReplicated` yapıldı, replike `EndServerTime` eklendi, HUD kalan süreyi ondan hesaplıyor (derlenmedi).
- **O11 madde 3** (geri bildirim `IsLocallyControlled()`'e bağlı): `MMONotify::ToOwner` eklendi, envanter ve görev bileşeni mesajları uzak istemciye de gider (derlenmedi). Beceri/karakter/ganimet/parti mesajları açık.
- **Açık:** Y1 (kimlik; kısmi hafifletmeler `SECURITY_AUDIT.md` K-1'de),  O4 (parti ve ticaret davet kapma — `MMOPartyComponent.cpp`/`MMOTradeComponent.cpp` yerelde Codex'te olduğu için bulutta dokunulmadı), O5 (arkadaş rızası), O6, O7, O8, O11, O12 (denetim logu; Trade dosyası), D2, D4, D7 (mevcut yollar çanta dolu durumunu zaten koruyor), D8, D10.

### Y3 çözüm notu (2026-10-06)

- **Doğrulama:** Bulut turu madde 2'yi zaten yapmıştı (`git log -S EndServerTime`: 496cdc9): `RemainingTime` `NotReplicated`, replike `EndServerTime`, HUD kalan süreyi ondan hesaplar. Madde 1 yalnız ≥1 puanlık adımdı; HP 6/sn ve mana 7+0.25·Lv/sn olduğundan Health hâlâ saniyede ~6 kez, Mana ~7-27 kez kirleniyordu.
- **Çözüm:** `UMMOStatsComponent::TickComponent` yenilenmeyi her karede biriktirir ama Health/Mana'ya yalnız 0.5 sn adımlarla (`RegenApplyInterval`) ve tam puan olarak yazar → en fazla 2 Hz replikasyon (Mana zaten `COND_OwnerOnly`). Kesir birikimde kalır; toplam yenilenme, formüller (MaxHealth/AttackPower/Defense), SaveGame ve `ApplyDamage`/`Heal`/ölüm yolu (anlık, tam hassasiyet) değişmedi; `OnHealthChanged`/`OnManaChanged` ve OnRep delegeleri aynı çalışır. Yenilenirken HUD çubuğu 0.5 sn adımlarla atlar (kozmetik; istemci yumuşatması eklenmedi).
- **Efekt süresi:** Değişiklik gerekmedi; dizi yalnız uygulama/tazeleme/bitişte kirlenir (sunucu `RemainingTime`'ı her karede düşürür ama alan replike değil).
- **Kazanç: doğrulanmadı** (Network Profiler / `stat net` ile ölçülmedi). Kuramsal: yenilenen oyuncu başına can güncellemesi tick hızından (~30 Hz) 2 Hz'e, mana 2 Hz'e (yalnız sahibe).
- **Kalan:** `MobName`'in replikasyondan çıkarılması (küçük kazanç) yapılmadı.

### O11 madde 3 devam notu (2026-10-06): uzak istemcide kaybolan geri bildirimler

- **`MMONotify::ToOwner`'a taşındı:** Parti (`ServerInviteNearest` "Yakinda davet edilecek oyuncu yok", `ServerAccept` "Davetin suresi doldu"/"Davet eden cok uzakta", `ServerKick` "Sadece parti lideri…"/"Uye partiden cikarildi", `FormOrJoin` "Parti dolu"), ganimet sarf malzemesi satırı ("+N Item", `MMOLootPickup`), beceri `ExecuteSkill` "Menzilde hedef yok" (iki yer), karakter `UseHealthPotion`/`UseConsumable` "Canin zaten dolu"/"Iksir yok!"/"X yok!". Bunlar sunucuda uzak pawn için çalışan yollardı (`IsLocallyControlled` ile kayboluyordu); `ToOwner` yerelde ekran, uzakta Hint/Result'tır. Ağ trafiği: Hint Unreliable, aynı metin 0.5 sn içinde tekrar gitmez.
- **Dokunulmadı (bilinçli):** (a) yalnız istemcide çalışan ön kontrol/geri bildirimler: `UseSkill` "Guvenli bolgeden saldiramazsin"/cooldown/">> Skill" (istemci aynı kontrolü kendi makinesinde yapıp RPC'den önce gösterir), `AttackTarget` "Guvenli bolgede saldiri yok" (4101), `AMMOCharacter::MMOPartyKick` ön kontrolleri, `LearnSkill` "Ogrenilemedi", `AMMOPlayerController` tıkla-yürü ipuçları ("Saticiya yaklas" vb.), HUD "Hotbar dolu"; (b) `OnLevelUpFeedback`/`OnRep_SpawnProtected` (OnRep ile her makinede yerel çalışır); (c) `Client` RPC gövdeleri (`ClientNotifyInvited/Kicked`, `ClientUpgradeResult`); (d) `QuestComponent::NotifyMobKilled` ilerleme satırı (kasıtlı yerel: HUD takipçisi uzakta zaten gösterir, sohbeti doldurmasın); (e) yalnız konsol/debug: `MMOPvP`, `MMOClass`, `MMONpcTalk`, `MMOTravel` hata satırı.
- **Dedicated server için kalan engeller:** (1) `MyProjectServer.Target.cs` yok; (2) admin/debug yolu `IsLocalController()`'a kilitli (`MMOTravel`, `RequestZoneTravel`, `ServerSetPvP/ApplyBuff/SpawnMob/GiveItem/LoadCharacter`); (3) `AMMOCharacter::TakeDamage` ve `UMMOStatsComponent::Heal` hasar/iyileştirme sayısını `World->GetFirstPlayerController()->GetHUD()` ile çiziyor (MMOStatsComponent.cpp ~322, ~926; dedicated'da HUD yok → sessizce yok; bilerek bozulmadı, çözüm: Unreliable multicast ya da OnRep farkı); (4) `AMMOMob::MulticastEliteDeathAnnounce` `GetFirstLocalPlayerController` (istemcide çalıştığı için zararsız, sunucuda null); (5) `SaveGameToSlot`/`LoadGameFromSlot` oyun iş parçacığında (asenkron kayıt/DB); (6) `bAllowPvP=true` varsayılan ve replike değil; (7) LLM NPC için küresel hız sınırı yok; (8) Slate/HUD modül bağımlılıkları Server target'ta doğrulanmadı; (9) `Exec` komutları/debug RPC'leri Shipping'de (D1 RPC tarafı kapatıldı); (10) listen-server'a özgü `GetFirstPlayerController` kullanan başka istemci-yerel yollar (HUD çizimleri) dedicated'da çalışmayabilir.
