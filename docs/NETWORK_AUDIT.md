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
| O6 | ORTA | **Kısmen düzeltildi:** tüm onaylardan sonra son ilerleme kaydı yenilenir; son kayıt–pawn yok oluşu penceresi ve mağaza yazım sırası/iptal açık (güncel durum aşağıda) | `AMMOCharacter::EndPlay`, `AMMOGameMode::RequestZoneTravel/FinishZoneTravel/AbortZoneTravel` |
| O7 | ORTA | **Kapandı:** elit ölüm duyurusu sunucuda tüm PlayerController'lara sahip-istemci `ClientReceiveChat` ile gider | MMOMob.cpp:916-934 |
| O8 | ORTA | **Büyük kısmı kapandı:** hedef taraması 5 Hz önbellekli; spawner sunucuya özel ve replike değil. Mob animasyon/hareket tick'i karelik kalıyor | MMOMob.cpp; MMOMobSpawner.cpp |
| O9 | ORTA | Stats/Quest/Skill/Party alanları da herkese (gizlilik + bant) | MMOStatsComponent.cpp:77-92; MMOQuestComponent.cpp:26-27; MMOSkillComponent.cpp:42-45; MMOPartyComponent.cpp:19-20 |
| O10 | ORTA | `UnequipSlot` çanta sınırını (12) aşar; invariant bozulunca takas/geri alma/ödül reddedilir | MMOInventoryComponent.cpp:285 |
| O11 | ORTA | Dedicated için engeller: Server target yok, admin yolu `IsLocalController`'a bağlı, geri bildirim `IsLocallyControlled`'a bağlı, HUD'a bağlı sunucu kodu | bkz. bölüm 6 |
| O12 | ORTA | **Kapandı (yerel log; Shipping'de derlenmez, bkz. O12 bölümü):** takas/iptal, klan, yükseltme/yanma, satış, depo ve ad değişimi yapılandırılmış sunucu loguna yazılır | bölüm 1 |
| O13 | ORTA | **Kod düzeltildi, PIE bekliyor (2026-10-07):** üye kabulü mevcut grubun tamamını lider izni olmadan birleştiriyordu; mevcut gruplar için lider yetkisi davet ve kabul anında denetleniyor | `UMMOPartyComponent::CanCombineParties/FormOrJoin` |
| O14 | ORTA | **UE'de yeniden üretildi ve düzeltildi, PIE bekliyor (2026-10-07):** eski/vekil liderin yetkisini kaybetmiş bekleyen klan daveti kabul ediliyordu; lider rolü kabul anında yeniden denetleniyor | `UMMOClanComponent::CmdAccept` |
| O15 | ORTA | **UE'de yeniden üretildi ve düzeltildi, PIE bekliyor (2026-10-07):** ölü karakter görevi teslim ederek XP ödülünü kaybediyordu; sunucuda kabul/teslim için canlılık denetlenir | `UMMOQuestComponent::AcceptQuest/ClaimReward` |
| O16 | ORTA | **UE'de yeniden üretildi ve düzeltildi, PIE bekliyor (2026-10-07):** ölü karakter yerdeki altın/sarf/ekipmanı toplayabiliyordu; ortak sunucu toplama kapısı canlılık denetler | `AMMOLootPickup::TryCollect` |
| O17 | ORTA | **UE'de yeniden üretildi ve düzeltildi, PIE bekliyor (2026-10-07):** dolu can/mana saf iksiri ve ortak kullanım beklemesini harcıyordu; ihtiyaç yoksa tüketim öncesi reddedilir | `AMMOCharacter::UseConsumable` |
| O18 | ORTA | **UE'de yeniden üretildi ve düzeltildi, PIE bekliyor (2026-10-07):** temel saldırı RPC'si sentetik zaman damgasından sonra ikinci cooldown kapısında geçerli vuruşu reddediyordu | `AMMOCharacter::AttackTargetInternal/ServerAttackTarget_Implementation` |
| O19 | ORTA | **Native UE'de yeniden üretildi ve düzeltildi, gerçek ağ/HUD bekliyor (2026-10-07):** aynı revizyonlu yeni takasta eski oturumun onayı kabul ediliyordu; onay artık sunucu oturum kimliği + görülen revizyonla doğrulanır | `UMMOTradeComponent::ServerConfirm/StartTradeServer`, `AMMOHUD::DrawTrade/HandleTradeClick` |
| O20 | ORTA | **Native UE'de yeniden üretildi ve düzeltildi, gerçek ağ/HUD bekliyor (2026-10-07):** eski teklif aksiyonunun altını yeni takasa yazılıyordu; add/remove/gold RPC'leri de görülen session/revision ile doğrulanır | `UMMOTradeComponent::IsSeenOfferCurrent/ServerSetGold/ServerAddItem/ServerRemoveItem`, `AMMOHUD::HandleTradeClick` |
| O21 | ORTA | **Native UE'de yeniden üretildi ve düzeltildi, gerçek ağ/HUD bekliyor (2026-10-07):** eski aktif takas iptali yeni takası sonlandırıyordu; cancel/X görülen session ile doğrulanır, inaktif davet reddi ayrı RPC'dir | `UMMOTradeComponent::ServerCancel/ServerDeclineInvite`, `AMMOHUD::HandleTradeClick/HandleWindowClick` |
| O22 | ORTA | **Native UE'de yeniden üretildi ve sınırlı olarak düzeltildi, gerçek ağ/bildirim görünümü bekliyor (2026-10-07):** eski inaktif ret yeni daveti siliyordu; accept/decline davet generation'ı, sohbet ise gönderim pawn/kimlik/iptal niyetini taşır | `UMMOTradeComponent::ServerAccept/ServerDeclineInvite/SetPendingInviteServer`, `AMMOPlayerController::SendChatWithContext/ServerSendChat` |
| O23 | ORTA | **Native UE'de yeniden üretildi ve sınırlı düzeltildi, gerçek ağ/bildirim görünümü bekliyor (2026-10-07):** parametresiz eski parti reddi yeni daveti siliyordu; accept/decline recipient generation, chat gönderim pawn/ID bağlamı kullanır | `UMMOPartyComponent::ServerAccept/ServerDecline/SetPendingInviteServer`, `AMMOPlayerController::SendChatWithContext/ServerSendChat` |
| O24 | ORTA | **Native UE'de yeniden üretildi ve sınırlı düzeltildi, gerçek ağ/bildirim görünümü bekliyor (2026-10-08):** eski klan kabulü replacement pending klanına katılabiliyordu; kabul recipient generation ve gönderim pawn bağlamını doğrular | `UMMOClanComponent::CmdAcceptSeen/SetPendingInviteServer`, `AMMOPlayerController::SendChatWithContext/ServerSendChat` |
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

### O13 · ORTA · Parti birleştirmede lider rızası — kod düzeltildi (2026-10-07)

`FormOrJoin` iki tarafın bütün `Members` kümelerini birleştiriyordu. Davet edilen sıradan
üye kabul edince kendi grubunun tamamını taşıyabiliyor; davetçi sıradan üye de başka grubu
kendi grubuna ekleyebiliyordu. Sekiz kişi sınırı, grubun lider yetkisini korumuyordu.
`CanCombineParties` artık davet edilen mevcut grubun liderini ve davetçinin mevcut
grubunun liderini denetler. Davet-kabul arasında liderlik değişirse kabul reddedilir.
Partisiz davetçi kendi adına karar verir; üyelerin partisiz oyuncu davet etmesi korunur.
Aynı grubun bayat davetini kabul etmek idempotenttir, liderliği değiştirmez.

Normal UE editor ve PCH'siz `MMOPartyComponent.cpp` derlemeleri geçti. Headless gerçek
UE dünyasında `TestPartyConsent.ps1` **10 senaryo geçti**: iki solo grup kurulumu,
davet edilen üye/davetçi üye reddi, iki tarafta bekleme sırasında lider değişimi, aynı
grup kabulü, liderlerin onaylı birleşmesi, üyenin solo daveti, sekiz kişi ve taşma reddi.
Kontrolsüz test karakterleri kullanılır; pending daveti test kodu sunucuda kurar.
Gerçek sahip-istemci RPC, çoğaltma/HUD ve dedicated server doğrulaması değildir.
Bu bulgu, O4'teki eski davet kapma bulgusundan ayrıdır; mevcut parti kodunda pending
davet ezmeme/aynı daveti yenilememe koruması zaten vardır.

### O14 · ORTA · Bekleyen klan davetinde bayat lider yetkisi — düzeltildi (2026-10-07)

`CmdInvite` yalnız lider için açıkken `CmdAccept` davetçinin sadece aynı klanda kalmasını
denetliyordu. `ApplySaveData` asıl lider dönünce vekil lideri sıradan üyeye indirir; bu
oyuncunun bekleyen daveti 30 saniyelik pencere içinde hâlâ üye alabiliyordu. Kabul anında
`IC->bIsLeader` denetimi eklendi. Davet eski rolü taşımıyor; yetkisiz kabul, pending
daveti tüketip üyeliğe dokunmadan reddediliyor. Yetkili vekil lider davet edebilir.

`TestClanInviteAuthority.ps1` düzeltme ÖNCESİ gerçek UE'de `FAIL checks=0 reason=demoted
inviter accepted` verdi (test çıkışı 1, motor çıkışı 0). Düzeltme sonrası **9 senaryo
geçti**: demotion reddi, asıl/vekil lider kabulü, mevcut üyelik korunması, klandan ayrılan
davetçi, geçersiz zaman damgası, kendi kendine davet, eksik ve yok edilmiş davetçi.
Controllersiz fixture'lar kullanılır; pending ve rol değişimini test kodu kurar. Diskte
oyuncu kaydı yazılmaz. Gerçek yeniden bağlantı/lider devri, sohbet RPC ve HUD/replikasyon
PIE'de denenmedi. Bu bulgu O4 davet kapma ve O13 parti birleştirmeden ayrıdır.

### O15 · ORTA · Ölüyken görev teslimi kalıcı XP kaybı — düzeltildi (2026-10-07)

`ClaimReward` canlılık denetlemiyordu. `GainExperience` ölü oyuncuda erken döndüğü
halde altın/sarf ödülü verilip görev aktif listeden çıkarılıyor ve tamamlanmış
sayılıyordu; XP yeniden alınamıyordu. NPC yakınlığı ve RPC sahipliği bu durum için
yeterli değildir. Kabul ve ödül kapıları artık sunucuda karakter/stat varlığını ve
`!bIsDead` koşulunu denetler. Ölüyken görev ve bütün ödüller korunur; dirilişten sonra
normal yakınlık/kapasite/tek sefer ödül kontrolleri uygulanır.

`TestQuestRewardLifeState.ps1` düzeltme öncesi gerçek UE'de `FAIL checks=3 reason=dead
claim consumed quest or granted partial reward` verdi (motor exit=0, script exit=1).
Düzeltme sonrası **8 kontrol geçti**: canlı kabul, eksik ilerleme reddi, tamamlanmış
görev hazırlığı, ölü teslim reddi/tüm ödüllerin korunması, ölü kabul reddi, `Revive`
sonrası tam XP/altın/iksir ödülü, tekrar teslim reddi ve NPC'den uzak teslim reddi.
İzole controllersiz karakterler ve NPC kullanılır; öldürme sayacı/`bIsDead` testte
kurulur. Gerçek ölüm animasyonu, istemci RPC/HUD, kayıt ve yeniden giriş oynanmadı.
Fixture'lar disk kaydı yazmaz; Shipping'de test komutu derlenmez.

### O16 · ORTA · Ölü karakterin ganimet toplaması — düzeltildi (2026-10-07)

`TryCollect` sadece otorite/oyuncu kontrolünü denetliyordu. Ölü karakter altın,
sarf ve ekipman alabiliyordu; rezervasyon zamanlayıcısı da aynı kapıyı kullanır.
Sunucuda stat varlığı ve `!bIsDead` koşulu bütün grant işlemlerinden önce denetlenir.
Reddedilen ganimet normal 30 saniyelik ömrü boyunca yerde kalır; dirilişte yeniden
overlap oluşursa veya canlı oyuncu toplarsa mevcut rezervasyon/kapasite kuralları geçerlidir.
Diriliş anında otomatik tekrar deneme veya ganimet ömrünü uzatma eklenmedi.

`TestLootLifeState.ps1` düzeltme öncesi gerçek UE'de `FAIL checks=0 reason=dead gold
collected` verdi (motor exit=0, script exit=1). Düzeltme sonrası **10 kontrol geçti**:
ölü altın reddi, ölü sarf overlap reddi, ölü ekipman reddi, süresi bitmiş rezervasyon
reddi; `Revive` sonrası üç türün tam grant'i ve her biri için tekrar grant reddi.
İzole karakter/test PlayerController kullanılır, collision kapalıyken toplama/overlap
girişleri senkron çağrılır; ölüm bayrağı ve bitmiş rezervasyon zamanı testte kurulur.
Zamanlayıcının gerçek overlap listesi, fiziksel ceset, sahip-istemci replication/HUD,
gerçek respawn ve yeniden giriş PIE'de oynanmadı. Fixture aynı çağrıda unpossess edilip
yok edilir; ilk yükleme/autosave çalışmaz, oyuncu kaydı yazılmaz. Shipping'de test
komutu derlenmez. Bu bulgu O15 görev ödülünden ayrıdır.

### O17 · ORTA · Dolu can/manada saf iksir kaybı — düzeltildi (2026-10-07)

Normal can iksiri dolu kaynakta reddedilirken genel tüketilebilir yolundaki büyük can,
normal/büyük mana iksirleri envanterden çıkarılıyor, ortak cooldown harcanıyor ve
iyileştirme clamp ile etkisiz kalıyordu. `UseConsumable`, eşya ve cooldown tüketmeden
önce tanımın can veya mana yenileyebileceğini denetler. Sadece `EffectId` içermeyen
saf iksirler etkisizse reddedilir; buff parşömenleri tam kaynakta kullanılabilir.
Kontrol sahip istemcinin ön denetiminde ve sunucu RPC uygulamasında aynı yoldadır.
Mana havuzu sıfırsa mana iksiri korunur. Birden fazla kaynak yenileyen saf tanımda
herhangi bir kaynak eksikse kullanım izinlidir; mevcut veride böyle tanım yoktur.

`TestConsumableBenefit.ps1` düzeltme öncesi gerçek UE'de `FAIL checks=0 reason=full
health consumed potion or cooldown` verdi (motor exit=0, script exit=1). Sonrasında
**11 kontrol geçti**: dolu büyük can, normal/büyük mana, sıfır mana havuzu; ihtiyaç
anında büyük can ve mana; ortak cooldown; ölü kullanım reddi; tam can/manada Rage
parşömeni; geçersiz tanım ve normal dolu can iksirinin reddi. İzole controllersiz
karakter kullanılır, vitaller/cooldown testte kurulur ve sunucu girişleri doğrudan
çağrılır. Gerçek istemci RPC ulaşımı, gecikme, HUD ve kayıt/yeniden giriş oynanmadı.
Fixture oyuncu kaydı yazmaz, Shipping'de test komutu derlenmez.

### O18 · ORTA · Temel saldırı RPC'sinde yuvarlamayla kaybolan vuruş — düzeltildi (2026-10-07)

Eski RPC, %10 tolerans kapısını geçince `LastMeleeTime=Now-MeleeCooldown` yazıp
doğrudan saldırının tam cooldown kapısını tekrar çalıştırıyordu. Float saatin
100 saniye olduğu UE fixture'ında süresi bitmiş ilk istek reddedildi:
FAIL checks=0 uptime=100 (UE exit0/PS exit1). Ortak private gövde artık doğrudan
yol için tam, RPC için sunucudan seçilen `0.9f` eşiğini tek kez denetler; gerçek
double saati yazar. Authority/hasar ve diğer hedef kuralları korunur.

Yedi sentetik uptime ve sınır/ret damgaları için 89 native kontrol geçti.
Fixture uzun oturum veya gerçek RPC taşınması değildir; tam ağ/jitter/PIE
kontrolleri açıktır. Sentetik zaman komutu ayrı otomasyon sürecinde açık
başlatma bayrağı ister; normal editörde fixture kurmadan reddedilir. HUD'a
sentetik zaman bırakma riskini gidermek için normal editöre bu bayrak eklenmez.
Ayrıntılı derleme/paket ve negatif kanıt: `CHECKLIST_FIXES_2026-10-05.md` §53.

### O12 · ORTA · Denetim logu — kapandı (2026-10-06, yerel sunucu logu; Shipping sınırı aşağıda)
- **Shipping sınırı (QA 2026-10-07 ORTA-4, araştırıldı, kodlanmadı):** `UE_LOG` Shipping'de derleme dışı kalır; bu loglar (`LogMMO*Audit`) Shipping/dedicated yayında ÜRETİLMEZ, yalnız Development/Editor ve listen-server geliştirme oturumlarında vardır. `bUseLoggingInShipping = true` Target.cs'e EKLENMEDİ: UBT'de `[RequiresUniqueBuildEnvironment]` işaretli (`TargetRules.cs`, `RequiresUniqueEnvironment` taban `UnrealGame`/`UnrealEditor` ile karşılaştırır), yani Game/Server hedefi `BuildEnvironment = Unique` olmak ve motoru kaynaktan derlemek zorunda; kurulu (Launcher) motorda doğrulanamaz ve Shipping derlemesi bu makinede denenmedi. Dedicated engeli 1 (kaynak motor + `MyProjectServer.Target.cs`) çözülürken `bUseLoggingInShipping = true` o dosyaya eklenmeli; alternatif: denetim olaylarını `UE_LOG`'dan bağımsız bir yazıcıya (dosya/HTTP) vermek. O zamana kadar "denetim logu" Shipping'de garanti değildir.
- `LogMMOTradeAudit`: tamamlanan ve iptal edilen takaslar; iki taraf, altın bakiyeleri ve eşya `InstanceId`/artı/nadirlikleri.
- `LogMMOClanAudit`: klan kurma, katılma, ayrılma, atma ve lider devri.
- `LogMMOInventoryAudit`: yükseltme başarı/başarısızlık/yanma, satma, depoya koyma ve depodan alma; karakter yuvası, eşya kimliği ve son bakiye/sayaçlar.
- `LogMMOIdentityAudit`: eski/yeni görünen ad, eski/yeni kayıt yuvası ve etkin kayıt yuvası.
- Satın alma, görev/ganimet ve diğer altın değişimleri mevcut `[MMOMETRIC] ev=gold` satırlarında kaynak etiketi ve bakiye ile izlenir. UE logları `Saved/Logs` altında kalıcıdır. Hesap backend'i gelince görünen ad/kayıt yuvası yerine doğrulanmış `AccountId` yazılmalı ve merkezi sorgulanabilir log toplayıcısına aktarılmalıdır.

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
- `MulticastMeleeFX` / `MulticastSkillFX` / `MulticastPlayCreatureAttack`: Unreliable (doğru). Elit ölüm dünya duyurusu multicast kullanmıyor; sunucu tüm PlayerController'lara sahip-istemci reliable sohbet satırı yollar (O7 kapandı).
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
- **Spawner — kapandı (2026-10-06):** `bReplicates=false; bNetLoadOnClient=false`; replike alanı/görseli olmayan spawner istemcide yüklenmez ve aktör kanalı açmaz. GameMode runtime spawner'ları yalnız sunucuda üretir; doğan moblar kendi replike aktörleridir.
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
- **Seamless travel'da pawn yok edilirken `IsPlayerControlled()` hâlâ true mu?** `EndPlay` bloğu (MMOCharacter.cpp:377-403) buna bağlı; false ise parti çıkışı ve kayıt atlanır **[doğrulanmalı: PIE'de iki oyunculu `MMOTravel`, log ile]**. Kayıt bölge geçişinde `SaveCharacterForTravelAsync` ile (geçiş istenirken) alındığı için o ana kadarki veri kaybı yok (anlık görüntü ile `ServerTravel` arasındaki ilerleme penceresi için O6'ya bak), ama parti dağılması garanti değil.

### O6 · ORTA · `bTravelSaveCommitted` penceresi ve toplu geçiş kilidi

**Güncel sınır (2026-10-07):** `FinishZoneTravel`, tüm ilk kayıt onaylarından sonra
her karakter için `FlushDeferredSave(true)` ile güncel durumu senkron yeniden yazar.
XP/altın değişiminin ayrıca `SaveCharacter` çağırmasına bağlı değildir. Başarısız son
yazım geçişi iptal eder; bekleyen işaret korunur ve iptal sonrası yeniden deneme yolu
çalışır. Yerel mağazada karakter başına geçişte ikinci bir yazım yapılır. Bu mevcut
senkron mağaza sözleşmesine dayanır; gelecekte bulut için son snapshot da ayrı async
bariyere alınmalı ve depoda yazım sırası/iptal uygulanmalıdır.

Yeni `TestZoneTravelProgress.ps1`, ilk yerel yazımın onayını düşürür; kayıt isteği
olmadan 37 XP/23 altın ekleyip onayı gerçek GameMode yoluna gönderir. Düzeltme öncesi
`FAIL changed=1 xp=0 gold=0 started=1 cleared=1`; sonrası aynı diskteki kaydın seviye/XP
ve altını güncel değerlerle eşleşti (`PASS`, tüm alanlar 1). Gerçek bulut gecikmesi
değildir. Son yazım hatası testi de ek kayıt isteği olmadan iki hata enjeksiyonu ile
iptal/işaret korunması/başarılı yeniden kaydı denetler.

**Açık:** son senkron yazımdan `ServerTravel`/pawn yok oluşuna kadar devam eden
oynanış, iki oyunculu gerçek yeniden yükleme ve motor travel hatası. Aşağıdaki ilk
denetim ve önceki düzeltme notları tarihsel bağlamdır; O6 tümden kapanmadı.
- `RequestZoneTravel` her karakter için `SaveCharacterForTravelAsync` çağırır → tüm callback'ler başarılıysa `FinishZoneTravel` `SetTravelSaveCommitted(true)` yapar → `ServerTravel`. Harita değişimi *asenkron*: bayrak true iken `EndPlay` kaydı atlanır ama oyun sürer (mob'lar, loot, görev teslimi, depo, ticaret). Anlık görüntü ile pawn'ın gerçekten yok edilmesi arasındaki saniyelerde olan her şey (altın, XP, ödül, depo hareketi) **geri sarılır**. Yerel (senkron) mağazada pencere 0 kare; bulut mağazada 15 sn'ye kadar. Çoğaltma değil yalnız geri sarma (her karakter için tutarlı anlık görüntü), ama oyuncu için algılanan kayıp. Takas bu pencerede tamamlanırsa `TryExecuteTrade` iki tarafı yeniden kaydediyor (MMOTradeComponent.cpp) — **yalnız senkron/yerel mağaza için tutarlı; bulut mağaza için yuva başına yazım sırası/kilit gerekir** (aşağıdaki 2026-10-07 notu): bulutta eski travel anlık görüntüsü yeni takas kaydından SONRA inerse A eski (kılıç var), B yeni (kılıç var) durumuyla kalıp kılıç ikilenebilir.
  - **Düzeltme:** Travel başlayınca oyuncu girdisini/RPC'leri kilitle (`bTravelLocked` → Server RPC'ler erken döner, hasar bağışıklığı) ya da `EndPlay` içinde `EndPlayReason == LevelTransition` ise de ikinci kez kaydet (aynı slot, idempotent).
- `bZoneTravelInProgress` yalnız `AbortZoneTravel` yollarında (kayıt hatası, zaman aşımı, oyuncu ayrılması, `ServerTravel` false) sıfırlanıyor; `ServerTravel` true dönüp harita sonradan yüklenemezse bayrak kalıcı true → geçiş sunucu yeniden başlayana dek kapalı. Seamless travel yeni GameMode örneği yarattığından başarı yolunda sorun yok. **[doğrulanmalı]**
- **2026-10-06 sertleştirmesi:** kayıtlar artık `IMMOCharacterStore::SaveAsync` callback bariyerinden
  geçiyor. Tek bir hata/oyuncu ayrılması geçişi iptal ediyor; hiç gelmeyen bulut callback'i 15 sn
  sonra kilidi ve `bTravelSaveCommitted` bayraklarını temizliyor. Headless hata enjeksiyonu callback'i
  düşürerek kaynak haritada iptali doğruladı. `ServerTravel` başladıktan sonraki motor yükleme hatası
  ve kayıt ile pawn yok oluşu arasındaki ilerleme penceresi hâlâ PIE/motor düzeyinde açık konular.
- **2026-10-07 (QA ORTA-1 güvenli kısmı, kodlandı):** (1) `IMMOCharacterStore::GetWriteTracker()` ile yuva başına artan yazım numarası: `SaveCharacterForTravel` (senkron) ve `SaveCharacterForTravelAsync` her yazımdan önce numara alır; asenkron callback geldiğinde aynı yuvaya daha yeni bir yazım başlamışsa eski yazımın sonucu yok sayılır (`false`, `ActiveSaveSlot` güncellenmez) ve geçiş iptal olur; (2) `HandleZoneTravelSaveComplete` sayaç yerine karakter başına "tamamlandı" kümesi tutar (çift callback bariyeri erken açmaz); (3) `RequestZoneTravel` aynı karede (senkron mağaza) iptal olduysa `false` + neden döner ve kalan karakterleri yazmaz; reddedilen/iptal edilen istekte test kancaları sıfırlanır. **Bilinçli olarak kodlanmayanlar (mimari):** a) takip numarası yalnız callback tarafını korur, verinin depoda hangi sırayla indiğini belirlemez; bulut mağaza yuva başına seri yazım kuyruğu veya karşılaştır-ve-yaz (`Revision`/`ExpectedRevision`) uygulamalıdır; b) zaman aşımından sonra uçuştaki yazım iptal EDİLEMEZ (arayüzde iptal yok; eski yazım arkada inebilir, a) bunun çözümü); c) anlık görüntü ile `Finish` arasındaki ilerleme penceresi (dirty sayacı / `bTravelLocked`) ve `Finish`'te canlı/savaş dışı yeniden doğrulama; d) `SaveCharacter()/EndPlay/otomatik kayıt` hâlâ senkron `Save`. Bulut mağaza gelmeden a-c için tasarım yapılmalı.
- `Character->GetStats()->IsInCombat()` tüm oyuncular için aranıyor: dedicated/halka açık sunucuda tek oyuncu mob'a vurarak herkesin geçişini engelleyebilir (griefing). Host-only listen modelinde kabul edilebilir.
- **2026-10-07 yeniden doğrulama:** `FinishZoneTravel`, `ServerTravel` öncesinde güncel
  PlayerController/pawn kümesinin kaydı beklenen karakterlerle aynı olduğunu ve hepsinin
  canlı/savaş dışı kaldığını denetler. Bekleme sırasında yeni oyuncu, pawn değişimi, ayrılma,
  ölüm veya savaş varsa geçiş iptal edilir. `TestZoneTravelStateChange.ps1` yerel kayıt
  onayını test kancasıyla tutup ardından savaş durumunu değiştirerek iptali, kilit/bariyer
  ve zamanlayıcı temizliğini doğruladı. Gerçek gecikmeli bulut mağazası ve iki oyunculu
  PIE denenmedi. Önceki notta açık olan canlı/savaş dışı yeniden doğrulama kodlandı;
  ilerleme anlık görüntüsü ile pawn yok oluşu arasındaki pencere, depoya yazım sırası,
  motor yükleme hatası hâlâ açıktır. Ertelenen kayıt hatası için aşağıdaki ek düzeltmeye bak.
- **2026-10-07 ertelenen yazım hatası:** `FlushDeferredSave` başarısızsa artık `false`
  döner ve bekleyen kayıt işaretini korur. `FinishZoneTravel` bu sonucu toplar; tek bir
  ertelenen yazım hatasında geçişi iptal eder, eski geçiş kaydını güncelmiş gibi işaretlemez.
  İptal sonrası tekrar yazım da başarısızsa işaret kalır; sonraki başarılı elle/otomatik/
  zorunlu kayıt temizler. `TestZoneTravelDeferredSave.ps1` iki ardışık flush hatasını
  enjekte edip iptali, işaretin korunmasını ve başarılı yeniden kayıtta temizliğini doğrular.
  Gerçek disk arızası, gecikmeli bulut mağazası ve iki oyunculu PIE doğrulanmadı.
- **2026-10-07 callback bütünlüğü:** başarılı karakter kaydının ikinci hata sonucu artık
  başarıyı geri alamaz. `HandleZoneTravelSaveComplete` önce istek seri numarası, pending
  karakter üyeliği ve completed kümesini denetler; bariyer dışındaki sonuçlar yok sayılır.
  `ServerTravel` başladıktan sonra boşalan pending kümesi, gecikmiş sonucun geçiş kilidini
  sıfırlamasını önler. İlk gerçek kayıt hatası/ayrılmış karakter geçişi hâlâ iptal eder.
  Oyun iş parçacığı sözleşmesi `check(IsInGameThread())` ile denetlenir. Yeni headless
  `TestZoneTravelCallbacks.ps1`, doğrudan gerçek GameMode girişinde **9 senaryo geçti**:
  yabancı başarı/hata, eski istek, ilk başarı, yinelenen başarı/hata, geçiş başladı fazında
  geç sonuç, ilk hata ve geçersiz karakter. Test pending kümesini controllersiz fixture
  karakterlerle kurar; gerçek iki oyunculu bulut callback'i/travel başlatmaz. Diğer normal
  geçiş/timeout testleri ayrı kanıttır. Depoda yazım sırası/iptal ve XP/altın anlık görüntü
  penceresi bu değişiklikle çözülmedi; O6'nın bu bölümleri açık kalır.
- Seamless sonrası yeni pawn 0.6 sn boyunca varsayılan durumda (kayıt yüklenmedi; `bInitialLoadComplete=false` olduğu için kayıt yazılmıyor — iyi). Bu pencerede girilen `/klan`, `/arkadas`, ticaret komutları `ApplySaveData` ile ezilir (MMOClanComponent.cpp:567-640, MMOFriendsComponent.cpp:287-308) — zararsız ama komutlar "başarılı" görünür. İstersen `bInitialLoadComplete` olmadan RPC'leri reddet.
- Bağlantı kopması: Trade ve Klan EndPlay'ı doğru; Friends eski `AnnouncePresence(false)` yalnız EndPlay `Destroyed` + `!bTravelSaveCommitted` yolundaydı; identity detach kapısı offline kaybettirdi (O28: UnPossessed içinde sınırlı düzeltildi). `static GAnnounced` (MMOFriendsComponent.cpp:250) süreç ömrü boyunca büyür, PIE'de sunucu/istemci aynı süreçte paylaşılır (D6).
- Parti, bölge geçişinde dağılır (EndPlay `RequestLeave`, MMOCharacter.cpp:393-396); kalıcı değil — tasarım notu.

---

## 6. Dedicated server geçişi engelleri

### O11 · ORTA · Madde madde
1. **Build hedefi yok:** `Source/` altında yalnız `MyProject.Target.cs` ve `MyProjectEditor.Target.cs`; `MyProjectServer.Target.cs` yok → `-server` paketlenemez. **2026-10-06 doğrulaması:** standart Server target tanımıyla UBT çağrıldı; kurulu Epic binary dağıtımı `Server targets are not currently supported from this engine distribution` diyerek derlemeden önce reddetti. Hedef dosyası doğrulanamadığı için commit edilmedi; gerçek dedicated doğrulaması UE 5.7 kaynak motor derlemesi gerektiriyor.
2. **Admin yolu `IsLocalController()`'a kilitli:** `MMOTravel` (MMOPlayerController.cpp:47), `RequestZoneTravel` (MMOGameMode.cpp:90), `ServerSetPvP` / `ServerApplyBuff` / `ServerSpawnMob` / `ServerGiveItem` / `ServerLoadCharacter` (MMOCharacter.cpp:640-651, 1126-1145, 1163-1176, 1393-1418, 1613-1633). Dedicated'da hiç yerel controller yok → bölge geçişi, PvP anahtarı, test komutları kullanılamaz. Gerekli: sunucu konsolundan (`Exec` GameMode'da zaten var) ya da yetkili (admin listesi / token) bir RPC.
3. **Geri bildirim `IsLocallyControlled()`'e bağlı (uzak istemcilere hiç gitmiyor — listen-server'da da!):** Inventory (MMOInventoryComponent.cpp:172, 392, 402, 415, 424, 453, 463, 472, 498, 514, 523, 550, 570, 620, 665), Party (MMOPartyComponent.cpp:181, 198, 224, 232, 258, 274, 373), Quest (MMOQuestComponent.cpp:178-371), Skill (MMOSkillComponent.cpp:129, 457, 475, 566, 791), Character (MMOCharacter.cpp:498, 596, 705, 835, 928, 1026, 1482-1662), LootPickup (MMOLootPickup.cpp:168 ve **koşulsuz** 183-207). Sunucuda uzak oyuncunun pawn'ı `IsLocallyControlled()==false` olduğundan "Canta dolu!", "Altin yetersiz", "Satin alindi", "Satildi", görev alındı, loot mesajı, "Kaydedildi/Karakter yuklendi", "Oldun! -N EXP", vurma geri bildirimi **yalnız host'a** görünür. Bu bugün listen-server'da uzak oyuncuların deneyimini bozuyor (kod okumasıyla kesin; PIE'de iki oyunculu gözle doğrulanmalı) ve dedicated'da tamamen yok olur. `LootPickup`'ın koşulsuz mesajları ise host ekranında başkalarının toplama olaylarını gösteriyor.
   - **Düzeltme:** Tek bir `AMMOPlayerController::ClientNotify(FText/uint8 Kod, int32 Arg...)` (Unreliable, oran sınırlı) ya da mevcut kanal-2 sohbet satırı; sunucu kodu `GEngine->AddOnScreenDebugMessage` yerine bunu çağırsın.
4. **HUD'a bağlı sunucu kodu — kapandı (2026-10-06):** `TakeDamage` ve `Heal` artık sunucuda HUD aramıyor; ilgili karakter üzerinden `Unreliable NetMulticast` ile miktar/konum/iyileştirme bayrağı gönderiliyor. Her istemci kendi yerel controller/HUD'ında çiziyor, dedicated örneği `NM_DedicatedServer` dalında çizimi atlıyor. Eski `OnRep_Health` sayı üretimi kaldırıldı; aynı vuruş iki kez görünmüyor. Aktör ilgililiği dışındaki oyunculara kozmetik sayı gönderilmemesi kasıtlı. İki istemcili görsel PIE doğrulaması bekliyor.
   - Elit ölüm duyurusu da kapandı: mob relevancy multicast'i yerine sunucu `PlayerController` döngüsü ve `ClientReceiveChat` kullanıyor; uzaktaki oyuncu da satırı alır.
5. **Disk G/Ç oyun iş parçacığında:** `SaveGameToSlot`/`LoadGameFromSlot` artık `FMMOLocalFileCharacterStore` içinde (çağıranlar `IMMOCharacterStore` üzerinden); yerel mağaza hâlâ SENKRON, yalnız bölge geçişi arayüz düzeyinde asenkron sözleşmeli (`SaveAsync` callback'i; yerel mağazada aynı karede biter, gerçek diski iş parçacığından çıkarmaz). `SavedClanNameInUse` (bkz. O3). 100 oyuncu × 60 sn otomatik kayıt dağınık olsa da takas/klan/arkadaş anlık `SaveCharacter()` çağırıyor. Asenkron kayıt (`AsyncSaveGameToSlot`) ya da DB.
6. **`bAllowPvP` — replikasyon kısmı kapandı (2026-10-06, `AMMOGameState::bPvPEnabled`; bkz. §9 sonu); varsayılan artık `false`.** Eski not: `bAllowPvP = true` varsayılan idi (MMOGameMode.h:40, "Test icin varsayilan acik"); halka açık sunucuda `false` olmalı. `bAllowPvP` replike değildi: istemci `IsPvPAllowed()` her zaman `false` döner (`GetAuthGameMode` istemcide null) — istemci tarafı kapılar buna güvenmemeli (`AttackTarget` istemci yolu güvenli: yalnız sunucu karar veriyor).
7. **LLM NPC diyaloğu:** `ServerRequestNpcDialogue` yalnız oyuncu başına 2 sn (MMOPlayerController.cpp:1183-1187); sunucu geneli eşzamanlı/dakikalık üst sınır yok → 100 oyuncu × 0.5 istek/sn = ücretli istek patlaması. Küresel token-bucket ekle. Sunucu ortamında API anahtarı yönetimi gerekir.
8. **Slate/HUD bağımlılıkları:** `AMMOPlayerController` Slate (`SEditableTextBox`) kullanıyor; `FSlateApplication::IsInitialized()` koruması var (448) — dedicated hedefinde modül bağımlılıkları (`MyProject.Build.cs`) ve `HUDClass` yüklemesi **[doğrulanmalı: Server target derlemesi]**.
9. **Exec komutları:** `MMOProbeSpawn/Surface` `!UE_BUILD_SHIPPING` içinde (iyi, MMOGameMode.cpp:31, 58). Diğer `Exec`'ler (`MMODamage`, `MMOHeal`, `MMOGainXP`, `MMOGiveItem`, `MMOSpawnMob`, `MMOPvP`, `MMOBuff`, MMOCharacter.h:188-279) Shipping'de de derleniyor; konsol Shipping'de kapalı olsa da `ServerGiveItem/SpawnMob/SetPvP/ApplyBuff` RPC'leri kodda kalıyor. Host-only kilidi nedeniyle uzaktan istismar edilemez; yine de `#if !UE_BUILD_SHIPPING` ile sar (D1).

---

## 7. Düşük ciddiyetli bulgular

- **D1** Debug RPC'leri Shipping'e giriyor (MMOCharacter.cpp:640, 1126, 1163, 1393, 1613): `IsLocalController` kilitli; `#if !UE_BUILD_SHIPPING`.
- **D2** DoT source weak pointer expired -> anonim damage -> PvP/party/clan/hub kapisi atlama native yeniden uretildi. §66/O29 server-only source classification ve HER pending tick'te weak resolution ile sinirli duzeltildi. Current offensive skills mob-only; normal PvP skill exploit erisimi kanitlanmadi. Gercek remote/rebinding/elapsed scheduling acik.
- **D3** Satıcı ve `TryUpgrade`: ölü kontrol yok (MMOInventoryComponent.cpp:354-372; depo ölüyü reddediyor, 590-597); `TryUpgrade` NPC gereksinimi/yakınlık istemiyor (297-347) — KO'da NPC gerekir; tasarım kararı değilse ekle.
- **D4 — kısmen kapandı (2026-10-06):** `UseConsumable/UseHealthPotion` istemci ve sunucuda ortak 1 sn kullanım kapısından geçer; hem Reliable RPC yığılması hem aynı karede çoklu iksir tüketimi engellenir. Trade `Accept/Cancel` ve Klan komutlarının RPC başına sınırı hâlâ açık; Trade `RemoveItem` mevcut 0,3 sn teklif değişikliği sınırını kullanır.
- **D5** Sohbet: genel kanalda mesaj "[Dunya] ..." / "[Klan] ..." öneki taşıyarak sistem satırı taklit edebilir (satır biçimi `Sender: Mesaj`, MMOPlayerController.cpp:433); kontrol karakterleri temizlenmiyor.
- **D6** `static TMap GAnnounced` (MMOFriendsComponent.cpp:250) sınırsız büyür, PIE'de sunucu+istemci aynı süreçte paylaşır.
- **D7** Loot: ilk-gelen alır, sahiplik/kilitlenme yok; çanta doluyken loot yerde kalır (iyi) ama `AddEquipmentInstance` doluysa ödül/loot sessizce kaybolur (MMOInventoryComponent.cpp:170-177).
- **D8** Klan: çevrimdışı üye atılamaz (`CmdKick` yalnız çevrimiçi, MMOClanComponent.cpp:468-484); travel sırasında lider `EndPlay`'i "X artık klan lideri" duyurusunu gereksiz yollar (207) **[doğrulanmalı]**.
- **D9** `ServerSetName` cooldown'u yok (bkz. Y1).
- **D10 — kapandı (2026-10-06):** `IsInsideHubSafeZone`, sabit harita `PlayerStart` merkezlerini zayıf `UWorld` anahtarlı cache'e ilk çağrıda bir kez toplar; sonraki saldırı/beceri kontrolleri aktör taraması yerine kare mesafe karşılaştırır. Travel sonrası yeni dünya ayrı cache alır, ölü dünya kayıtları sonraki çağrıda (tembel) temizlenir. **2026-10-07:** boş sonuç artık önbelleğe ALINMAZ (PlayerStart henüz yüklenmemişse güvenli bölge dünya ömrü boyunca kaybolmasın); boş kaldıkça en fazla saniyede bir yeniden taranır, dolu liste kalıcıdır.

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
8. **O6** — travel sonrası motor yükleme hatası ve kayıt-pawn yok oluş penceresi.

Önerilen PIE doğrulamaları (2+ oyunculu listen-server, ardından `-server` hedefi):
- Uzak istemcide "Canta dolu / Altin yetersiz / Satin alindi / loot" mesajı görünüyor mu (bölüm 6.3).
- İstemcide diğer oyuncunun `Inventory.Gold`'unu `showdebug`/BP ile oku (Y2).
- `stat net` + Network Profiler: 1 oyuncu yenilenirken ve buff'lıyken izleyici başına B/sn (Y3).
- Hızlı çift tık ile `TryUpgrade`/`Sell` + 200 ms gecikme (`p.NetPktLag=200` / `Net PktLag=200`) (Y4).
- `open ip?Name=<mevcut kayıt adı>` ile bağlan (Y1-B).
- Elit boss'u uzak bir oyuncu varken öldür: duyuru uzak oyuncuya ulaşmalı (O7 kodda kapandı; PIE doğrulaması bekliyor).
- İki oyuncuyla `MMOTravel` sırasında pawn `EndPlay` logları ve parti durumu (bölüm 5).

---

## 9. Uygulama durumu (2026-10-04 güncellemesi)

PR [ozancidik/mmogame#1](https://github.com/ozancidik/mmogame/pull/1) değişiklikleri **derlenmedi**. Önceki yerel turda kapananlar: Y2/O9, Y4/O10, Y5, O1, O2, O3.

- **Bu turda (derlenmedi):** D1 (debug RPC'leri Shipping'de erken döner), D3 (ölü karakter satıcıdan işlem ve yükseltme yapamaz; yükseltmede NPC yakınlık koşulu tasarım kararı olarak eklenmedi), D5/D9 (sohbet kontrol ve bidi karakter temizliği), D6 (`GAnnounced` >512 iken en az 300 sn eski kayıtları temizler; fresh kayıtlar için hard cap yok, O28 sınırı), D9/Y1 ad değiştirme soğuması (30 sn, yalnız uzak istemci).
- **Y3 madde 1** (can/mana her karede kirlenir): yenilenme ≥1 puanlık adımlara bağlandı (derlenmedi). Madde 2 (`ActiveEffects.RemainingTime`): `NotReplicated` yapıldı, replike `EndServerTime` eklendi, HUD kalan süreyi ondan hesaplıyor (derlenmedi).
- **O11 madde 3** (geri bildirim `IsLocallyControlled()`'e bağlı): `MMONotify::ToOwner` eklendi, envanter ve görev bileşeni mesajları uzak istemciye de gider (derlenmedi). Beceri/karakter/ganimet/parti mesajları açık.
- **Açık:** Y1 (kimlik; kısmi hafifletmeler `SECURITY_AUDIT.md` K-1'de), O4 (parti ve ticaret davet kapma), O5 (arkadaş rızası), O6, O11, D2, D4'ün Trade `Accept/Cancel` ve Klan komutları, D7 (mevcut yollar çanta dolu durumunu zaten koruyor), D8. O8'in hedef taraması ve spawner kanalı kapandı; yalnız mob tick/ölçüm ayarı açık. O7, O12 ve D10 kodda kapandı; PIE/log doğrulaması aşağıdaki checklist'te bekliyor.

### Y3 çözüm notu (2026-10-06)

- **Doğrulama:** Bulut turu madde 2'yi zaten yapmıştı (`git log -S EndServerTime`: 496cdc9): `RemainingTime` `NotReplicated`, replike `EndServerTime`, HUD kalan süreyi ondan hesaplar. Madde 1 yalnız ≥1 puanlık adımdı; HP 6/sn ve mana 7+0.25·Lv/sn olduğundan Health hâlâ saniyede ~6 kez, Mana ~7-27 kez kirleniyordu.
- **Çözüm:** `UMMOStatsComponent::TickComponent` yenilenmeyi her karede biriktirir ama Health/Mana'ya yalnız 0.5 sn adımlarla (`RegenApplyInterval`) ve tam puan olarak yazar → en fazla 2 Hz replikasyon (Mana zaten `COND_OwnerOnly`). Kesir birikimde kalır; toplam yenilenme, formüller (MaxHealth/AttackPower/Defense), SaveGame ve `ApplyDamage`/`Heal`/ölüm yolu (anlık, tam hassasiyet) değişmedi; `OnHealthChanged`/`OnManaChanged` ve OnRep delegeleri aynı çalışır. Yenilenirken HUD çubuğu 0.5 sn adımlarla atlar (kozmetik; istemci yumuşatması eklenmedi).
- **Efekt süresi:** Değişiklik gerekmedi; dizi yalnız uygulama/tazeleme/bitişte kirlenir (sunucu `RemainingTime`'ı her karede düşürür ama alan replike değil).
- **Kazanç: doğrulanmadı** (Network Profiler / `stat net` ile ölçülmedi). Kuramsal: yenilenen oyuncu başına can güncellemesi tick hızından (~30 Hz) 2 Hz'e, mana 2 Hz'e (yalnız sahibe).
- **Kalan:** `MobName`'in replikasyondan çıkarılması (küçük kazanç) yapılmadı.

### O11 madde 3 devam notu (2026-10-06): uzak istemcide kaybolan geri bildirimler

- **`MMONotify::ToOwner`'a taşındı:** Parti (`ServerInviteNearest` "Yakinda davet edilecek oyuncu yok", `ServerAccept` "Davetin suresi doldu"/"Davet eden cok uzakta", `ServerKick` "Sadece parti lideri…"/"Uye partiden cikarildi", `FormOrJoin` "Parti dolu"), ganimet sarf malzemesi satırı ("+N Item", `MMOLootPickup`), beceri `ExecuteSkill` "Menzilde hedef yok" (iki yer), karakter `UseHealthPotion`/`UseConsumable` "Canin zaten dolu"/"Iksir yok!"/"X yok!". Bunlar sunucuda uzak pawn için çalışan yollardı (`IsLocallyControlled` ile kayboluyordu); `ToOwner` yerelde ekran, uzakta Hint/Result'tır. Ağ trafiği: Hint Unreliable, aynı metin 0.5 sn içinde tekrar gitmez.
- **Dokunulmadı (bilinçli):** (a) yalnız istemcide çalışan ön kontrol/geri bildirimler: `UseSkill` "Guvenli bolgeden saldiramazsin"/cooldown/">> Skill" (istemci aynı kontrolü kendi makinesinde yapıp RPC'den önce gösterir), `AttackTarget` "Guvenli bolgede saldiri yok" (4101), `AMMOCharacter::MMOPartyKick` ön kontrolleri, `LearnSkill` "Ogrenilemedi", `AMMOPlayerController` tıkla-yürü ipuçları ("Saticiya yaklas" vb.), HUD "Hotbar dolu"; (b) `OnLevelUpFeedback`/`OnRep_SpawnProtected` (OnRep ile her makinede yerel çalışır); (c) `Client` RPC gövdeleri (`ClientNotifyInvited/Kicked`, `ClientUpgradeResult`); (d) `QuestComponent::NotifyMobKilled` ilerleme satırı (kasıtlı yerel: HUD takipçisi uzakta zaten gösterir, sohbeti doldurmasın); (e) yalnız konsol/debug: `MMOPvP`, `MMOClass`, `MMONpcTalk`, `MMOTravel` hata satırı.
- **Dedicated server için kalan engeller:** (1) `MyProjectServer.Target.cs` yok ve kurulu Epic binary motor Server target'ını desteklemiyor; kaynak UE 5.7 motoru gerekiyor; (2) admin/debug yolu `IsLocalController()`'a kilitli (`MMOTravel`, `RequestZoneTravel`, `ServerSetPvP/ApplyBuff/SpawnMob/GiveItem/LoadCharacter`); (3) `SaveGameToSlot`/`LoadGameFromSlot` oyun iş parçacığında (asenkron kayıt/DB); (4) `bAllowPvP` replike değil; (5) Slate/HUD modül bağımlılıkları kaynak motor Server target'ında doğrulanmadı; (6) `Exec` komutları/debug RPC'leri Shipping'de (D1 RPC tarafı kapatıldı); (7) listen-server'a özgü `GetFirstPlayerController` kullanan başka istemci-yerel yollar dedicated'da çalışmayabilir. Hasar/iyileştirme sayılarına ait doğrudan sunucu-HUD bağı ve elit dünya duyurusu kapandı. LLM NPC için küresel hız/eşzamanlılık sınırı eklendi; günlük bütçe tavanı ayrı açık kalemdir (aşağıdaki 2026-10-06 notunda kapandı).

### Dedicated server hazırlığı turu (2026-10-06): engellerin güncel durumu

Derlendi ve her commit öncesi `RunAllChecks.ps1 -SkipBuild` GEÇTİ; PIE'de iki oyunculu **oynanmadı** (test adımları `CHECKLIST_FIXES_2026-10-05.md` §22-24).

| # | Engel | Durum |
|---|-------|-------|
| 1 | `MyProjectServer.Target.cs` yok | **Açık** (kaynak motor gerekli, aşağıya bak) |
| 2 | Admin yolu `IsLocalController()`'a kilitli | **Merkezileştirildi:** `MMOAdmin::IsServerAdmin(const APlayerController*)` (`Core/MMOAdmin.h/.cpp`). Şimdilik listen-server host'unun yerel denetleyicisi; dedicated'da her zaman `false` (token/hesap izin listesi ileride yalnız bu fonksiyona eklenecek). `MMOTravel`, `RequestZoneTravel`, HUD bölge geçişi düğmesi, `ServerSetPvP/ApplyBuff/SpawnMob/GiveItem/LoadCharacter` ona bağlandı. Dedicated'da bölge geçişi/PvP anahtarı/test komutları hâlâ kullanılamaz (tasarım gereği; çözüm: yetkili RPC ya da sunucu konsolu `Exec`'i). |
| 3 | Disk G/Ç oyun iş parçacığında | Açık (bu tur kapsamı dışı; bölge geçişi kaydı yalnız arayüz sözleşmesi olarak asenkron, yerel mağaza senkron: gerçek disk G/Ç hâlâ oyun iş parçacığında; otomatik kayıt/EndPlay/klan/ticaret yolları senkron `Save`) |
| 4 | `bAllowPvP` replike değil | **Kapandı:** `AMMOGameState::bPvPEnabled` (replike), tek kaynak `AMMOGameMode::bAllowPvP`, `SetPvPAllowed` ikisini birlikte yazar, `InitGameState` başlangıç değerini yansıtır. `AMMOCharacter::IsPvPAllowed` sunucuda GameMode'dan, istemcide GameState'ten okur. İstemci ön kontrolü: PvP kapalı / parti-klan-hub kuralı varken oyuncu hedefine tıklayınca kovalama başlamaz ve `ServerAttackTarget` RPC'si gönderilmez (`ClientReleaseAttackTarget` akışı duruyor, karar sunucuda). `BP_ThirdPersonGameMode.uasset` ve haritaların isim tablosunda `GameStateClass` yok (taramayla doğrulandı) → C++ varsayılanı geçerli; ileride BP'de ezilirse `AMMOGameState` türevi olmalı. |
| 5 | Slate/HUD modül bağımlılıkları | Açık (Server target derlemesi gerekir) |
| 6 | `Exec`/debug RPC Shipping'de | RPC tarafı D1'de kapalıydı; değişmedi |
| 7 | `GetFirstPlayerController` vb. istemci-yerel yollar | **Tarandı** (aşağıda) |
| LLM | Günlük bütçe | **Kapandı:** `UMMODialogueSubsystem::MaxDailyRequests` (varsayılan 1500, 0=sınırsız; değer bir maliyet kararıdır). Sayaç UTC gün değişince sıfırlanır ve `Saved/MMO/LLMDailyBudget.txt`'te saklanır (yeniden başlatmada korunur). Dolunca istek atılmaz, NPC yedek replike düşer; dakikalık/eşzamanlı sınır ve `MaxSessionRequests` ile birlikte çalışır. Otomatik testlerde LLM isteği atılmadığından tavan **doğrulanmadı**. 2026-10-08 turu: tavan `-MMOLlmDaily=N` / DefaultGame.ini `[MMO] LlmDailyRequests` ile ayarlanır; **oyuncu başı kota** eklendi (tavanın 1/4'ü, en az 50; anahtar = PlayerState adı, yani kimlik zaafı burada da geçerli); sayaç yalnız yanıt üretmiş olabilecek isteklerde sayılır (HTTP 200 + zaman aşımı; bağlantı hatası / HTTP ≠ 200 iade); dosya geçici dosya + taşıma ile yazılır, 10 değişiklik/30 sn'de bir ve kapanışta. **Sınır:** sayaç dosyası tek sürece aittir; aynı `Saved/` dizinini paylaşan iki host süreci birbirini ezer (son yazan kazanır) ve toplam tavanı aşabilir. |

### Engel 7 tarama sonucu: istemci-yerel yollar (2026-10-06)

Kaynak taraması (`GetFirstPlayerController`, `GetFirstLocalPlayerController`, `GetPlayerController(…,0)`, `GetPlayerCharacter/Pawn(0)`, `GetLocalPlayer`, `IsLocallyControlled`, sunucuda `GetHUD()`): motor "ilk oyuncu" yardımcılarının tek kullanımı var; `GetPlayerController(0)`/`GetPlayerPawn(0)` kullanımı yok. Dedicated'da sunucuda çalışıp yanlış oyuncuya giden **yol bulunmadı**; riskler:

1. `Private/Characters/MMOCharacter.cpp:1017` (`MulticastCombatText_Implementation`): `GEngine->GetFirstLocalPlayerController(World)` ile HUD'a sayı basar. Dedicated'da fonksiyon `NM_DedicatedServer` dalında erken çıkar (doğru); istemcide yerel tek oyuncu için doğru. **Risk: düşük** (bölünmüş ekran/aynı süreçte çok yerel oyuncuda yalnız ilk denetleyicinin HUD'ı sayı görür). Öneri: ihtiyaç doğarsa `GEngine->GetLocalPlayerIterator` ile tüm yerel denetleyicilere dağıt; dokunulmadı.
2. `Private/Core/MMOGameMode.cpp:582` (`AbortZoneTravel`) `GEngine->AddOnScreenDebugMessage`: bölge geçişi iptal nedeni yalnız sunucu ekranına (host) gider; dedicated'da kimse görmez (log satırı kalır). **Risk: düşük** (geçiş zaten yalnız admin yolundan tetiklenir). Öneri: yetkili RPC eklendiğinde isteyen denetleyiciyi sakla ve `ClientReceiveChat` ile bildir.
3. `Private/Characters/MMOCharacter.cpp:1390` (`MMOPvP` yetkili yol): "PvP ACIK/KAPALI" ekran mesajı yalnız sunucu ekranında. **Risk: düşük.** Öneri: PvP durumu artık GameState'te; HUD'da küçük bir PvP rozeti çizilebilir (tasarım kararı).
4. `Private/Characters/MMOCharacter.cpp:1713` (`MMONpcTalk`), `MMOPartyKick` ön kontrolleri (1503 civarı), `MMOPlayerController.cpp` tıkla-yürü ipuçları: bilerek istemci-yerel (RPC değil); dedicated'da istemcide çalışır. **Risk yok.**
5. `Private/Components/MMOPartyComponent.cpp:333/344`, `MMOInventoryComponent.cpp:1114` (`Client*_Implementation` gövdeleri): Client RPC, sahibe gider; doğru hedef. **Risk yok.**
6. Önceki turlarda kapatılan `IsLocallyControlled()` kapılı geri bildirimlerin kalanı bilinçli bırakıldı (bkz. yukarıdaki "Dokunulmadı" listesi): `MMOSkillComponent.cpp:472/563/571/581/604/640`, `MMOQuestComponent.cpp:262`, `MMOCharacter.cpp:192/265/1149/1182/1289`. Hepsi istemci ön kontrolü ya da OnRep ile her makinede yerel çalışan gösterim; uzak istemcide sunucu yolundan kaybolma yok.

Bu taramada "küçük ve risksiz" düzeltme gerektiren, hedef nesneden denetleyici alınabilecek bir yer çıkmadığı için kod değişikliği yapılmadı.

### Server hedefi (engel 1 / 5): durum ve kaynak motor edinildiğinde yapılacaklar

- **Durum (2026-10-06):** `MyProjectServer.Target.cs` **oluşturulmadı/derlenmedi.** Kurulu `C:\Program Files\Epic Games\UE_5.7` Epic Launcher ikili dağıtımıdır; önceki denemede UBT `Server targets are not currently supported from this engine distribution` diyerek derlemeden reddetti. Dosya doğrulanamadığı için depoya eklenmedi (yanlış/denenmemiş hedef kodu bırakmamak için). Modül bağımlılıkları (`Slate`, `SlateCore`, HUD) Server hedefinde **doğrulanmadı**.
- **Kaynak motor edinilince, sırayla (komutlar tahmini yoldur, makineye göre uyarlayın):**
  1. Epic hesabını GitHub'a bağla, `EpicGames/UnrealEngine` erişimini al; `git clone --depth 1 -b 5.7 https://github.com/EpicGames/UnrealEngine.git D:\UE57Src` (ya da 5.7.x release etiketi).
  2. `D:\UE57Src\Setup.bat` ve `D:\UE57Src\GenerateProjectFiles.bat` çalıştır.
  3. `MyProject.uproject` dosyasını bu motora bağla (sağ tık > Switch Unreal Engine version, ya da `EngineAssociation` değerini kaynak motor kimliğine çevir). **Bu `.uproject` değişikliği repoda herkesi etkiler; ayrı bir dalda yapın.**
  4. `Source/MyProjectServer.Target.cs` ekle (`MyProject.Target.cs` ile aynı ayarlar, `Type = TargetType.Server`, `ExtraModuleNames.Add("MyProject")`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`).
  5. Motoru ve hedefi derle: `D:\UE57Src\Engine\Build\BatchFiles\Build.bat MyProjectServer Win64 Development -Project="<proje>\MyProject.uproject" -WaitMutex`. Çıkan Slate/HUD/UMG bağımlılık hatalarını `MyProject.Build.cs` içinde `Target.Type == TargetType.Server` koşuluyla ayır (engel 5).
  6. Paketle: `D:\UE57Src\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -project="<proje>\MyProject.uproject" -noP4 -platform=Win64 -serverconfig=Development -server -noclient -build -cook -stage -pak -archive -archivedirectory=<çıktı>`.
  7. Çalıştır ve test et: `MyProjectServer.exe /Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene -log -MMOPassword=<parola>`; ayrı süreçten `UnrealEditor.exe <proje> 127.0.0.1?Name=<ad>?Password=<parola> -game`. Dedicated'da bölge geçişi/PvP anahtarı/test komutlarının **kapalı** olduğunu doğrula (`MMOAdmin::IsServerAdmin` false) ve yönetim yolu (token/hesap) eklenene dek bunun kabul edilebilir olduğuna karar ver.
  8. `Config/DefaultEngine.ini` içindeki `GlobalDefaultServerGameMode=None` değerini `BP_ThirdPersonGameMode`'a çevir (dedicated varsayılan oyun modu); `RunAllChecks`'e server hedefi duman testini ekle.

### Takas regresyon kanıtı ve açık sınırlar (2026-10-07)

`MMOTestTrade` / `Scripts/TestTrade.ps1`: 79 native sunucu kontrolü. Teklif
revizyon/ayna, değişimde iki onay temizliği, eski revizyon reddi, throttle/kilit,
çift onay/tek aktarım, alıcıya yeni InstanceId, tam envanter snapshot'ı ve altın
korunumu; iki yönde altın tavanı/net bakiye, dolu çanta/net kapasite, değişmiş
eşya/gold, ölüm/mesafe, ValidateTick/idle ve 800 birim sınırı kapsanır.
Geçici revizyon koruması mutasyonu testte FAIL checks=12/check13 üretti (UE exit0,
PS exit1); koruma geri alındı ve normal DLL yeniden derlendi, 79 PASS doğrulandı.
Üretim takas kuralı/RPC/replication değişmedi. QA ve ekonomi Agency rolleri
okunarak salt okunur yan ajan incelemesi yapıldı.

Controller'sız, aynı-frame çağrılar; RPC sahipliği/transport, davet keşfi ve
COND_OwnerOnly/HUD doğrulanmadı. İlk yüklemesi olmayan fixture completion save
kapısından geçmez; gerçek iki oyuncunun kalıcı kaydı/çökme testi değildir.
Eşya aynasında adet/onay kontrolü vardır; `TheirOffer.Items` payload'ının tüm
alanları bağımsız karşılaştırılmadı. Runner sabit TEMP log adı kullanır ve
pakette sırayla çalışır; eşzamanlı runner süreçleri kapsam dışıdır. Salt okunur
QA son kaynak/runner incelemesinde engelleyici sorun bildirmedi.

İncelemede kalanlar (doğrulanmış yeni exploit olarak sınıflandırılmadı):

- §54 sırasında StartTradeServer revizyonu 0'a döndürüyordu ve confirm yalnız
  revizyon taşıyordu. Sonraki O19/§55 turunda aynı revizyonlu yeni oturumda eski
  onay kabulü native UE'de yeniden üretildi; sunucu oturum kimliği eklenerek
  giderildi. Gerçek istemci taşıma/HUD kontrolü hâlâ açık.
- Extraction-failure dalı çıkardıklarını ReceiveTradedEquipment ile geri verir
  (yeni kimlik); delivery-failure çantaları geri alırken allocation sayacını
  ve envanter bildirimi durumunu aynı şekilde geri almaz. Ön doğrulama sonrası
  synchronous delegate mutasyonu gibi koşullar fault injection ile sınanmadı.
- İki SaveCharacter çağrısı ortak kalıcı işlem değildir; başarısızlık/çökme
  ve yeniden girişte ekonomik bütünlük hâlâ açık. Development denetim logu
  Shipping/dedicated log garantisi sağlamaz (O12 sınırı sürer).

### O19 · ORTA · Eski takas onayının yeni oturumda kabulü (2026-10-07)

Eski kodla native fixture: 25 altın teklifli oturumu iptal et, yeni oturumda
yine revizyon1'e ulaş, eski revizyon1 onayını gönder. Yeni takasta kendi onay
bayrağı true oldu: **FAIL checks=80, previous session confirmation accepted
(check=81)**, UE exit0/PS exit1. Eski kodda uzaktan ekonomik exploit/oyuncu kaybı
ve gerçek gecikmeli paket yürütülmedi; kanıt native sunucu kabulüdür.

Sunucu `TradeSessionId` üretir: iki tarafın önceki kimliğinin max'ı +1, her iki
tarafa aynı değer, reset/iptal/tamamlama sonrası korunur. MAX_uint64 sonrası
başlatma reddedilir; wrap/reuse yok. Başlatma iki farklı, inactive, authority
bileşen ve aynı world ister; iki reciprocal durum delegelerden önce kurulur.
Kimlik pawn ömründedir; global/kalıcı hesap kimliği veya gizli yetki token'ı değildir.

Owner-only `uint64`, C++ ReplicatedUsing OnRep_Trade; Blueprint numeric alanı
değildir. Reliable confirm RPC `(SeenSessionId, SeenRevision)` alır; authority,
reciprocal partner, iki tarafın session/revision eşliği ve görülen çift, onay
bayrağı/cooldown değişmeden denetlenir. Aynı oturumdaki 1 sn lock ve 0,5 sn
confirm throttle korunur. İki uç yeni RPC imzasını içeren aynı derlemeyi kullanmalı.

HUD son başarılı DrawTrade'ın weak component kimliğini ve session/revision'unu
tutar; tıklama bu çifti `RequestConfirmSeen` üzerinden aynen gönderir. Skip/başarısız
draw cache'i temizler, pawn/bileşen değişimi eski cache'i kullanamaz. Console/C++
`RequestConfirm()` ise bilinçli olarak en son yerel snapshot'ı onaylar; her kullanıcı
onayının çizilen teklife bağlı olduğu iddia edilmez.

Takas native testi **96 PASS**: önceki79 +17 session kontrolü. Eski/sıfır/ileri
session, stale revision, partner mismatch, cooldown/ekonomi değişmezliği, fresh
pair ile bir kez tamamlama, peer önce onaylıyken eski onayla tamamlanmama,
farklı geçmişler/ters sıra/ilk argümandaki max, partner revision uyuşmazlığı,
active session'ı yeniden açmama, MAX-1→MAX→ret. Son QA'da DrawHUD erken çıkış
cache'i de girişte temizlenerek giderildi; Canvas/input native testte çalışmadı.
Render/input, replacement pawn HUD, gerçek RPC transport ve owner-only teslim
native fixture'da çalıştırılmadı. Ayrı replike alanlar bütün ağ koşullarında
atomik payload/token görünümü garantisi olarak yorumlanmaz.

Bu §55 çalışmasında Cancel/X, add/remove ve gold RPC'leri session taşımıyordu;
add/remove/gold sonraki O20/§56'da, aktif Cancel/X O21/§57'de düzeltildi.
Görülen davet bildirimi/kimliği ve gerçek HUD/ağ doğrulaması açıktır. Reentrant inventory
rollback, ortak kalıcı işlem, gerçek Server/Shipping ve PIE kontrolleri açık.
Agency Unreal Multiplayer Architect/QA rehberi ve yerel UE5.7 UHT/RepLayout
kaynak incelemesi kullanıldı. Son paket36/36 ve takas96 geçti; derleme/smoke/NetSmoke
sonuçları düzeltme kaydı §55'te. Bu bağlantı testi takas RPC/HUD oynanış kanıtı değildir.

## O20 — Teklif aksiyonlarını görülen oturum/revizyona bağlama (2026-10-07)

HEAD97b9f76 ile native reproducer: takası iptal et/yeniden aç, eski altın25
aksiyonunu gönder. Yeni teklif altını25 ve revizyon1 oldu; beklenen değişmezlik
**FAIL checks=79 / check80** verdi (UE exit0, PS exit1). Bu server-body kabul
kanıtıdır; Reliable mesaj sırası, gerçek eski HUD tıklaması veya uzak exploit
bu fixture'da oluşturulmadı. Mevcut altın clamp'i ve çift onay sonraki kapılardır.

Add/remove/gold RPC'leri artık `(SeenSessionId, SeenRevision, değer/InstanceId)`
taşır. IsSeenOfferCurrent authority, aktif karşılıklı partner, sıfır olmayan
session, iki tarafın session/revision eşliği ve görülen revizyonu denetler.
Ret cooldown, teklif, dört onay/ayna veya envanter mutasyonundan önce olur;
beklenen eski girdi normal return ile reddedilir, bağlantı koparılmaz.
Confirm aynı helper'ı kullanır. Kayıt şeması ve owner-only replication değişmedi.

HUD add/remove/altın reset/delta son başarılı draw'ın weak component/session/
revision çiftini kullanır; delta çizilen altından hesaplanır. Skipped draw veya
pawn değişimi offer dispatch'ini engeller. Console/C++ wrapper'ları güncel yerel
snapshot'ı taşır. Yeni RPC imzaları iki uçta aynı DLL gerektirir. Revision0 yeni
aktif session için geçerlidir; hızlı ardışık eski-revizyon istekleri reddedilir,
kullanıcı güncel teklifi yeniden görmelidir. Gerçek latency UX ölçülmedi.

Test162 = önceki96 +66: iki yönde üç aksiyonun eski/sıfır/ileri session,
eski/ileri/negatif revizyon ve partner session/revision uyuşmazlıkları; fresh0,
fresh Seen/current wrapper, tekrar eski revizyon ve inactive ret. Ret snapshot'ı
iki teklifin tüm Items payload'ı/altın/onay aynaları, revizyon ve üç saat damgası
ile iki envanteri karşılaştırır. Remove hedefi yeni teklifte gerçekten vardır;
add geçerli ve henüz teklif edilmemiş çanta eşyasıdır. Native162, DLL ve son36/36
paket geçti; 18 PCH'siz dosyada derleyici uyarısı0. Tam paket kaydı §56'da tutulur.
Canvas/input/takas RPC taşıma çalıştırılmadı; NetSmoke bağlantı kontrolüdür.

§56 sırasında Cancel/X açıktı; aktif iptal §57/O21'de session'a bağlandı.
Bekleyen davet kabul/reddine §58'de generation eklendi; görülen bildirimle ortak
kimlik görünümü hâlâ açıktır. Genel replike
payload/token atomikliği, reentrant rollback, ortak kalıcı işlem, Server/Shipping
ve gerçek iki oyunculu PIE kanıtı bu sınırlı düzeltmeyle tamamlanmaz.

### O21 · ORTA · Aktif takas iptalinde görülen oturum (2026-10-07)

HEAD `ddea095` üzerinde eski parametresiz cancel'ın sunucu gövdesi, iptal/yeniden
açılmış takası sonlandırdı: native **FAIL checks=145, check=146**, UE exit0/PS exit1.
Bu fixture gerçek gecikmeli Reliable paket veya uzak oyuncu exploit'i oluşturmaz;
aktif iptal isteğinin yeni oturuma uygulanabildiğini sunucu gövdesinde kanıtlar.

`ServerCancel(SeenSessionId)` authority, aktif durum, sıfır olmayan eşleşen session
ve geçerli karşılıklı partner varsa onun aynı session'da olduğunu denetler.
Teklif revizyonu iptali engellemez: teklif değişmiş olsa da oyuncu aynı oturumdan
çıkabilmelidir. Bozuk karşılıklılıkta kendi yerel oturumunu temizlemek mümkündür.
HUD iptal düğmesi/X son çizilen weak component ve session'ı gönderir. Console/C++
`RequestCancel` aktifken güncel yerel session'ı, inaktifken ayrı `ServerDeclineInvite`
isteğini gönderir. İnaktif ret yeni başlamış aktif takası iptal edemez; eski aktif
cancel da artık inaktif durumdaki yeni bekleyen daveti silemez. Yeni RPC imzaları
iki uçta aynı derlemeyi gerektirir; owner-only alanlar/kayıt şeması değişmedi.

Native **182 kontrol = önceki162 +20**: kurulum, iki yönde eski/sıfır/ileri session,
aktifken inaktif ret ve partner session uyuşmazlığı; revizyon değişince geçerli
iptal; iki current wrapper; inaktif eski cancel'ın daveti koruması ve kendi
davetinin reddi; bozuk karşılıklılıkta iki tarafın temizliği. Ret snapshot'ı tam
teklif/ayna payload'larını, tüm envanteri, partner/davet/name/session/revision,
saat damgalarını, aktiflik ve timer durumunu karşılaştırır. Normal DLL ve doğrudan
182 testi geçti. Son tam paket kanıtı §57'de tutulur. Controller'sız/aynı frame;
dünya saati değiştirilmez, gerçek RPC/Canvas veya kalıcı save kapısı çalıştırılmaz.

§57 sırasında bekleyen davet nonce'u yoktu; §58/O22 accept/decline'a generation
ve sohbet gönderim bağlamı ekledi. Eski görünür bildirimin yeni replike kimlikle
kabul edilmesi ayrı açık iştir. Gerçek owner-only teslim,
HUD input/pawn değişimi, bağlantının sürmesi, payload/token atomikliği, reentrant
rollback, ortak kalıcı işlem ve gerçek Server/Shipping doğrulamaları açıktır.

### O22 · ORTA · Bekleyen takas daveti ve sohbet bağlamı (2026-10-07)

HEAD `e1cf40e` üzerinde parametresiz decline, yeniden kurulmuş pending daveti
sildi: native FAIL165/check166, UE exit0/PS exit1. Gerçek gecikmiş paket veya
uzak exploit çalıştırılmadı. Alıcı pawn ömründe artan owner-only uint64
PendingInviteId tüketim/oturum temizliğinde korunur; MAX wrap reddedilir.
Accept/decline authority/aktiflik/nonzero eşleşmesini pending temizliğinden önce
denetler. Güncel daveti kabul ederken eski süre/canlılık/mesafe kapıları korunur.

İki chat gönderimi (Say ve OnChatCommitted) artık pawn + latest inviteId +
aktifse sessionId yakalar. ServerSendChat'in /trade accept/cancel dalları bu
Seen değerlerini kullanır; güncel sunucu snapshot'ına fallback yapmaz. Session0
inaktif decline niyetidir; aktif session ile iptal ayrı kalır. Seen pawn mevcut
GetPawn ile eşleşmezse, yeni pawn'da sayısal ID aynı olsa da mutasyon olmaz.
Sohbetin 200 karakter/0,7 sn kapısı sürer. RPC imzaları iki uçta aynı DLL ister;
chat metadatasının ek bant maliyeti gerçek profiler ile ölçülmedi.

218 native kontrol = önceki182 +28 invite +8 controller chat. Component retleri
iki tarafın tüm offer/envanter/partner/pending/token/saat/timer snapshot'ını
korur; iki yönde yeni generation, eski/0/ileri ret, fresh accept, duplicate,
inaktif/aktif intent ve MAX sınırı çalıştırılır. Ayrı üç native PC+Pawn ile gerçek
nearest invite, OnChatCommitted/Say, eski chat accept/decline, wrong/null pawn
ve fresh cancel sınanır. Normal DLL ve doğrudan218 PASS; son tam paket §58'de.
Slate/input, gerçek ownership/RPC/owner-only teslim veya kalıcı save kanıtı yok.

Gönderimde alınan latest replicated ID, oyuncunun gördüğü eski sohbet satırının
ID'si değildir. Davet metni ile kimlik farklı teslimlerde gelebilir; ID gelmeden
0/eski ID reddi sonrası yeniden denemek gerekir. Eski satırı okuyup yeni ID
replike olduktan sonra kabul yazma, bildirim ile kimliğin ortak görünümü, gerçek
pawn replacement/ağ, reentrant rollback/ortak kalıcılık/Server/Shipping açıktır.

### O23 · ORTA · Eski parti kabul/reddinin yeni davete taşınması — sınırlı düzeltildi

O13 lider rızası güncel lideri denetler; davetin hangi generation olduğu ayrı bir
sınırdır. Önceki parametresiz ServerDecline, replacement pending alanını temizledi:
UE FAIL10 (19:27:17 UTC), script1/UE0. Yeni owner-only uint64 PendingInviteId alıcı
pawn ömründe artar, tüketim/leave sonrası kalır, MAX'ta wrap reddedilir. Kabul/ret
nonzero Seen eşleşmesini pending temizliğinden önce denetler. Gerçek nearest yolu
mevcut cooldown/duplicate/takeover/lider kapıları sonrası generation helper kullanır.
Süre/mesafe/lider rızası/sekiz kişi kontrolü korunur; mevcut leave/kick değiştirilmedi.

Chat /parti kabul ve ret, Say/OnChatCommitted gönderim anında alınan pawn/ID'yi
kullanır; eski controller mesajı yeni pawn'a sayısal ID çakışmasıyla yönlenmez.
Trade aynı dispatch'i kullanır; ek parti metadata'sı bütün chat trafiğinde taşınır,
bant ölçülmedi. ClientNotifyInvited metniyle ID replikasyonu atomik değildir;
latest ID'ye dayanır. Eski görünen bildirimden yeni daveti kabul etme UX riski,
kimlik gelmeden eski/0 ret ve gerçek bağlantının sürmesi PIE'de açık.

46 native kontrol: eski10 birleşme,28 component ID/ret snapshot/MAX,8 controller
chat. Component retlerinde dokuz tarafın tam Members/Leader/pending/ID/saatler ve
envanter korunur. Controller testlerinde üç native PC+Pawn gerçek nearest daveti,
Say/OnChatCommitted ve ham endpoint'i çalıştırır; gerçek klavye/Slate/ağ/remote
ownership/owner-only teslim kanıtı değildir. Native PC possession değişimi/matching nonce fixture'ı da ret snapshot'ına dahildir;
gerçek remote respawn/yükleme değildir. Normal DLL/doğrudan46 geçti; son
negatif ve tam paket kanıtı CHECKLIST_FIXES §59'dadır. Party RPC ve chat sözleşmeleri
iki uçta aynı yeni DLL ister. Kayıt/hesap/dedicated/kick/leave oturum kimliği bu
sınırlı değişiklikle tamamlanmış sayılmaz.


### O24 · ORTA · Eski klan kabulünün yeni davete taşınması — sınırlı düzeltildi

Önceki HEAD3df7a3e üzerinde parametresiz CmdAccept, doğrudan kurulan geçerli
replacement pending klanına katıldı: native FAIL8, 20:23:07 UTC, UE0/PS1.
Gerçek gecikmiş RPC veya unexpired takeover kanıtı yok; CmdInvite'ın mevcut
başka-pending engeli korunur. Tüketim/expiry/invalidation sonrası yeni davete
eski niyetin uygulanması ayrı bir sınırdır.

Alıcı pawn'da artan owner-only uint64 PendingInviteId, tüketim/leave sonrası
korunur; MAX wrap reddedilir ve kayda yazılmaz. CmdAcceptSeen nonzero/eşitliği
pending temizliğinden önce denetler; süre/canlılık/güncel lider yetkisi kuralları
aynıdır. Yerel sunucu CmdAccept convenience güncel ID kullanır; /klan kabul
bu fallback'i kullanmaz. Say/OnChatCommitted gönderimde pawn/latest ID yakalar,
server chat mevcut pawn eşleşmesini şart koşar. Yeni pawn aynı sayısal ID'ye
sahip olsa da eski pawn kabulü yönlenmez. Diğer klan komutları kapsam dışıdır;
leave/kick/create/invite için ayrı stale-context incelemesi açık kalır.

43 native kontrol (9 lider +23 generation/helper +11 native controller chat)
ve normal DLL geçti. ID guard kaldırılması FAIL12, pawn guard kaldırılması
FAIL38/check39 ile yakalandı, her ikisi restore edildi. Component tam private
clan/pending/kimlik/saat ve inventory snapshot'ı; controller public clan/save ve
inventory, gerçek nearest ve native possession/matching ID yürütür. Aynı-frame
fixture, gerçek remote ownership/RPC/owner-only teslim/input/save kanıtı değildir.
Son tam paket kanıtı CHECKLIST_FIXES §61'de; gerçek kontroller PLAYTEST_CHECKLIST'te.

Davet yazısı ile ID replikasyonu atomik değildir. Eski görünür satır okunurken
yeni latest ID geldiyse kabul yeni snapshot'ı kullanır; görülmüş satır kimliği
iddiası yok. ID gelmeden 0/eski ret sonrası yeniden deneme gerekebilir. Ortak
bildirim/kimlik görünümü ve bütün chatte ek uint64 bant maliyeti gerçek PIE ve
profiler ile açık. Chat RPC iki uçta aynı DLL ister; Server/Shipping/persistence
ve gerçek expiry/respawn/bölge geçişi bu native kanıtla tamamlanmış sayılmaz.


### O25 · ORTA · Eski parti ayrılmasının yeni üyeliğe taşınması — sınırlı düzeltildi

04d2921 parametresiz ServerLeave üzerinde native eski ayrılma niyeti, yeniden
kurulan A-C partisini kaldırdı: baseline FAIL38, 8 Ekim 15:21:10.421 UTC,
UEexit0/PS1. Aynı-frame doğrudan endpoint kanıtıdır, canlı gecikmiş paket değil.

Owner-only FGuid PartySessionId grup devamlılığını, ayrı PartyMembershipId
kişisel katılım ömrünü tanımlar. Solo oluşum yeni session; davetçinin mevcut
session'ı, yoksa davet edilenin mevcut session'ı korunur. Birleşmede taşınan
üyeler ve ayrılıp aynı gruba yeniden katılan kişi yeni membership alır.
Sürekli üyeler roster/lider değişiminde kimliklerini korur. Ayrılan ve tek
kalan kişinin iki kimliği delegate öncesi temizlenir. RequestLeave convenience
çağırıldığı yerdeki membership'ı yakalar; ServerLeave authority/inparty/nonzero/
eşitliği mutasyondan önce ister. /parti leave/ayril yedinci chat argümanındaki
membership ve gönderim pawn'ını kullanır, sunucuda güncel kimliğe fallback yok.

73 native kontrol = önceki46 +18 component +9 leave chat. Ret snapshot'larında
üyeler/lider/pending/invite kimliği/saatler/session/membership/envanter korunur
(controller chat snapshot'ı private invite cooldown'ı içermez). Equality guard
kaldırılması FAIL40; pawn guard kaldırılması FAIL65/check66 ile yakalandı ve
byte exact restore edildi. İlk FAIL50 yeni fixture'da yanlış lider beklentisiydi:
[0,2,1] roster'dan 0 ayrılınca mevcut sıra kuralıyla lider2 olur; yalnız test
beklentisi düzeltildi. Tam son paket kanıtı CHECKLIST_FIXES §62'de tutulur.

Members/Leader ve GUID teslimi atomik değil; yakalanan latest membership ID,
çizilmiş roster kimliği değildir. Kimlik gelmeden ret/yeniden deneme ve eski
roster + yeni ID UX riski PIE'de açık. Native possession testi remote respawn/
RPC ownership testi değildir; GUID'ler save'e eklenmedi, yeni timer/Tick yok.
Chat sözleşmesi iki uçta aynı DLL ister; ek FGuid bant maliyeti ölçülmedi.
Kick stale bağlamı ve Character EndPlay IsPlayerControlled gate sonrası gerçek
unpossess/destroy cleanup ayrı açık. Bu iş disconnect cleanup garantisi vermez.


### O26 · ORTA · Unpossess/destroy sonrası kalan hayalet parti — sınırlı düzeltildi

967800c sonrası gerçek native PC UnPossess(); Pawn Destroy() fixture'ında
survivor boşalmadı: FAIL75, 8 Ekim 15:44:38.825 UTC, UEexit0/PS1. Eski testler
Destroy öncesi RequestLeave ile temizlediğinden bu engine lifecycle boşluğunu
ölçmüyordu. Yerel UE5.7 Pawn.cpp IsPlayerControlled için PlayerState ister;
UnPossessed PlayerState'i temizler. Destroyed detach'i Character EndPlay'den
önce çalıştırır. Böylece save/friends ile ortak player-control gate içindeki
party cleanup çalışmıyordu; native baseline aynı yolu gerçekten yürüttü.

Character EndPlay ayrı HasAuthority + PartyComponent kapısından, Super'den
önce RequestLeave çağırır. Component bu aşamada canlıdır; authority yerel
implementation'a gider, PlayerState/connection gerektirmez. §62 üyelik kimliği
ve survivor devamlılığı korunur. Save/friends gate genişletilmedi; controller/
kayıt kimliği kaybında kalıcılık ve presence ayrı açık. Yeni RPC/property,
Tick/timer veya save şeması yok. Salt UnPossess üyeliği bozmaz; cleanup EndPlay'de.

83 native = önceki73 +10 lifecycle. Possessed pair → gerçek UnPossess → Destroy;
possessed leader doğrudan Destroy; ordinary member possess/unpossess/Destroy;
survivor leader/session/personal ID; engine RouteEndPlay(LevelTransition) sonra
Destroy ve solo inventory kontrolü. Destroy edilmiş pawn/component'i testte
yeniden okumadan canlı survivors hemen denetlenir; GC/manual teardown beklenmez.
Transition route gerçek harita geçişi değildir; daha sonra Destroy motorun
begun-play gate'i nedeniyle ikinci Character EndPlay çağrısı yapmaz. Tekrar
cleanup çalışması, remote logout veya kalıcı save kanıtı olarak yorumlanmaz.

Player-control gate tekrar eklenen derlenmiş negatif FAIL75 verdi; byte exact
restore sonrası normal DLL ve PASS83 geçti. Son paket kanıtı CHECKLIST_FIXES §63.
Gerçek host/uzak istemci destroy/logout/travel/replication/parti XP ve GUID
owner-only teslimi PLAYTEST_CHECKLIST'te açık. Kick ve eski roster/latest ID
UX sınırları sürer. O25'te açık bırakılan party EndPlay gate'i bu kapsamda
native düzeltildi; genel player identity/save/presence lifecycle çözülmedi.


### O27 · ORTA · Identity detachment son kaydi atliyor — sinirli native duzeltme

O26'daki party fix save kapisini korumustu. Native possessed pawn Destroy
b9ae6e0 kodunda kendi slotuna son altini kaydetmedi (FAIL4 / UE0/PS1). Yerel
UE5.7 Pawn Destroyed -> Detach -> controller UnPossess -> Character UnPossessed
-> APawn PlayerState temizligi -> Character EndPlay sirasinda eski gate false.
Canli UnPossess sonrasi load/save/nav retry timer'i da kimliksiz pawn'da Default
slotunu secebiliyordu; bu timer etkisi kaynak incelemesiyle tespit edildi,
fixture elapsed timer'i degil handle kapanisini ve explicit save reddini olcer.

Authority UnPossessed, Super oncesi player + initial-load + !committed-travel
+ !EndPlay-started iken mevcut force save yolunu kullanir. Kayit kimligi hala
canli PlayerState'tendir, yeni fallback yok. Ardindan authority detach'inde
uc persistence timer'i ve initial load kapisi kapanir. Non-destroy EndPlay
once kendi save'ini yazar; started bayragi nested UnPossessed duplicate'ini keser.
K-1/slot tracker ayni API'de korunur. Save hatasi Warning, basari varsayilmaz.

Native26: GUID disposable slotlar, possessed/explicit/controller destruction,
unloaded/committed-travel ret, transition route tracker tek yazim, detached save
ret/timer handle ve existing-target K-1. Default ve iki rename fixture slotu
verileri korunur. Reset-negatifi FAIL9, EndPlay-gate-negatifi FAIL17, arm edilmis nav-retry
iptal negatifi FAIL10; restore
PASS26 ve normal DLL. Ayrintili build/paket kaniti CHECKLIST_FIXES §64.
Save schema/RPC/replication degismedi; yeni timer/Tick yok. Local synchronous
store icin lifecycle iyilesmesidir; disk failure dayanıklı retry, depoda CAS/
HTTP yazim sirasi, retained seamless pawn identity, gercek remote reconnect ve
PIE acik. Friends presence destruction kapisi bu iste cozulmedi. O26 parti
cleanup korunur; bu degisiklik salt UnPossess'te parti leave cagirmaz.


### O28 · ORTA · Possession bitisinde arkadas offline kaybi — sinirli native duzeltme

O27 sonrasi native possessed Destroy'da offline satiri kayboldu: baseline FAIL6.
Controller detach EndPlay'den once PlayerState'i temizledigi icin eski player
control gate'i false olur. Authority UnPossessed, canli eski PlayerState varken
Friends offline duyurur; initial-load/save sonucu gate'e katilmaz. EndPlay-started
ve committed-travel kapilari nested/gercek gecis niyetinde sahte offline'i keser.
Karsilikli riza, owner-only Friends replication ve mevcut notification RPC ayni.

Native24 actual PC/pawn + standalone client endpoint/HUD buffer: destroy,
unpossess/post-destroy/repossess, case-varied reconnect, tekillik/gizlilik/reng,
never-online, committed/transition replacement; sentetik abort/teardown ve NPC.
Load-gate negatifi FAIL6, EndPlay-gate negatifi FAIL18, restore normal DLL/PASS24.
Fixture case comparator ve spawn hatalari production bug sayilmaz. Ayrintili
kanit CHECKLIST_FIXES §65; gercek remote/PIE PLAYTEST_CHECKLIST'te acik.

Bu lifecycle iyilesmesi tum presence garantisi degildir: 300 sn expiry, process
static ad anahtarlari/cross-world collision, committed travel sonra disconnect
penceresi ve backend/account identity acik. D6'nin eski 512 hard-cap iddiasi
kaynakla uyusmuyordu: yalniz eski kayitlar prune edilir, fresh tablo buyuyebilir.
RPC/save schema yeni alan/timer/Tick eklenmedi. O27 save ve O26 party davranisi
korunur; slot/hesap kimligi fallback'i eklenmedi.


### O29 · ORTA · DoT player kaynagi kaybolunca anonim hasar (D2; sinirli native duzeltme)

Source Destroy sonrasi ProcessEffects'in weak.Get() null donerken anonymous
ApplyDamage, Character TakeDamage'deki attacker policy'sini atliyordu. Native
baseline FAIL10 (PvP-off victim Health dustu); mevcut offensive skill hedefleri
mob-only, dolayisiyla runtime PvP erisimi bu kanitta yok. Orphan player->mob tick
creditsiz hasar/kill uretebilir; ayni ret tum hedefler icin uygulanir.

Plain/nonreplicated bPlayerSource (non-mob Character) apply ve refresh'te cache
edilir. Pending weak source + flag'i tasir, her damage oncesi cozer; orphan
player damage skip edilir. Canli source normal ApplyDamage/TakeDamage kapilarina
gider. Mob cached hub/outside, environment/null, Regen ve expiry korunur.
Reentrant health delegate ilk accepted damage'da source Destroy eder: ikinci
queued tick source'u yeniden cozer. Native33, raw-pending negatif FAIL32, refresh
negatif FAIL15; restore DLL/PASS33. Source-controller/hesap rebinding degismedi.
Yeni RPC/save schema/replicated property/Tick yok. Editor-only callback fixture
helper'i production damage hook'u degildir; Shipping hedefi derlenmedi.

ECC readonly Unreal/security/QA incelemesi ve ayrintili kanit CHECKLIST_FIXES
§66; gercek host/remote/HUD/elapsed scheduling PLAYTEST_CHECKLIST'te acik. Pending
clear/refresh cancellation ve duration overshoot bu sinirli fix'in disindadir.
