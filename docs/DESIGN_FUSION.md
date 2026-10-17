# Ekipman birleştirme (nadirlik füzyonu) — tasarım

**Tarih:** 2026-10-17 · **Taban:** `29a44b1` (master) · ~~Durum: TASARIM, kod yok, derleme yok.~~ (ilk yazım, `26deb59`)
**Yazan:** `oyun-tasarimcisi`. Sayıların hepsi `[PLACEHOLDER]`; `ekonomi-tasarimcisi` modeli (Dilim D0) bitmeden kesinleşmez.

> **Güncel durum (2026-10-17, `f9b71c2` üzerinde, belge denetçisi):** **D1 TAMAM (180 kontrol)**, master'da: `d7bb1b1` (kurallar + 129 kontrol),
> `df4c3c6` (135), `1a8ec3f` (QA/güvenlik düzeltmeleri, 162), `d7c75ed` (180). Kanıt: `%TEMP%\mmo_fusion_fusion_1aba1ee2….log` (2026-10-17 10:44)
> `[MMO_FUSION_TEST] PASS checks=180`, 180 `CASE PASS`, 0 `FAIL`; koşu **izole worktree** `C:\Users\OZAN CIDIK\.claude\worktrees\fusion-d1`'de
> (ana kopya, sahibi belirsiz izlenmeyen `MMOZoneLayout*`/`MMORace*` dosyaları yüzünden derlenmiyor; `d7bb1b1` mesajı: `MMOZoneLayout.h` UHT hatası).
> **Oyun içi yüzeyi yok:** hiçbir komut, RPC veya HUD `FMMOFusion`'u çağırmıyor; `TestFusion.ps1` `RunSystemTests.ps1`/`RunAllChecks.ps1` adımı değil (D2).
> **PIE'de denenmedi.** D0 (ekonomi modeli) yapılmadı: `FeeMult` ve `MaxOutputRarity` hâlâ PLACEHOLDER. Açık/ertelenen maddeler §11.

## 1. Oyuncu deneyimi ve neden şimdi

Oyuncu aynı eşyadan üç tane aynı nadirlikte topladığında satıcının yanında bunları birleştirip bir üst nadirlikte (Sıradan → Nadir → Ender → Destansı) tek bir eşya alır; bedel olarak altın öder. Böylece çantaya düşen "çöp" kopyalar satıcıya 40% fiyatla gitmek yerine hedefe dönüşür (KO'daki demirci/örs döngüsünün nadirlik karşılığı). Yükseltme (+N) şans oyunudur; birleştirme ise **kesin** sonuçlu, yavaş ama güvenilir yoldur.

**Neden şimdi:** (1) Ekonomi modeli Lv16'dan itibaren altın gelirinin kademe giderinin 9–224 katı olduğunu, sink'in yetersiz olduğunu söylüyor (`docs/BALANCE_MODEL.md:96-98`, `docs/DECISIONS_NEEDED.md` karar 3, açık). Birleştirme hem altın hem eşya sink'idir ve yeni altın kaynağı yaratmaz. (2) Kodda hiç karşılığı yok (aşağıda grep). (3) Mevcut sprint/dal işleriyle dosya çakışması olmadan yeni dosyalarda yazılabilir; yeni kayıt alanı, yeni kayıt dosyası veya DB şeması gerektirmez (nadirlik zaten kaydedilen bir alan).

## 2. Alan seçimi: kanıt ve elenenler

### 2.1 Toplanan kanıt (2026-10-17)

- **Kirli main (başkasının çalışan işi):** değişmiş `Scripts/RunSystemTests.ps1`, `MMOCharacter.cpp/.h`, `MMOPlayerController.cpp`, `MMOHUD.cpp/.h`; staged `MMOTradeComponent.cpp`, `TestTradeTransfer.ps1`, `MMOTradeTransferSelfTest.cpp` (O70); izlenmeyen `MMORace*`, `MMORaceStore*`, `MMOZoneLayout*`, `TestRace.ps1`, `MMORaceSelfTest.cpp`, `.agents/`, `.codex/agents/*`.
- **Sprint tahtası** (`docs/SIX_MAP_SPRINT_BOARD.md`): S00 PIE, S01 dedicated/build, S02 yerleşim tablosu (`MMOZoneLayout`, `MMOGameMode.cpp`), S03 kalıcı ırk, S04 DB + servis (`V2__character_store.sql`, `MMOPersistenceService.py` aday `c4a18d6`), S05 Steam, S06–S08 sahiplik/geçiş/kapılar, S09–S10 haritalar + Ork + sosyal, **S11 PvP**, **S12 Pazar**, S13 sertleştirme. Hepsi atanmış ya da planlı.
- **Worktree/dallar:** `six-map-layout` (S02–S04, HUD + ırk + persistence), `trade-gold-atomicity` (Trade), `vendor-buy-pagination` / `vendor-sell-pagination` / `quest-window-rows` (**hepsi `MMOHUD.*` kirli**), `obsidian-spawn-validation` (`MMOGameMode.cpp`, `MMOMobSpawner.*`), `lv31-40-content` (`MMOItemDatabase.cpp`, `MMOMobArchetype.cpp`, `MMOQuestTypes.cpp`, `MMOGameMode.cpp`), `feature/warrior-skills` (cline, beceriler). **Hiçbiri `MMOInventoryComponent.*`'a dokunmuyor.**
- **grep (`Source/MyProject`, büyük/küçük harf duyarsız), 0 dosya:** `craft`, `recipe`, `tarif`, `pet`, `binek`, `leaderboard`, `ranking`, `auction`, `muzayede`, `arena`, `duel`, `socket`, `gem`, `sound`, `USoundBase`, `music`, `gathering`, `mining`, `fishing`. "kozmetik" yalnız hasar sayısı yorumlarında; "mount" yalnız `amount`.

### 2.2 Elenen adaylar

| Aday | Neden elendi |
|---|---|
| Irk sistemi | S03 atanmış; main'de sahipsiz izlenmeyen `MMORace*` dosyaları var |
| Pazar / müzayede | S12 atanmış, S04 (DB) + S10'a bağlı |
| PvP / arena / düello | S11 atanmış; hasar kapısı `MMOCharacter.cpp` (kirli, sıcak) |
| Harita yerleşimi, NPC | S02 (`MMOZoneLayout`, `MMOGameMode.cpp`), iki dal daha GameMode'da |
| Kalıcılık / DB | S04 aday dalda yazılıyor |
| UI işi | `MMOHUD.*` main'de ve üç Codex worktree'sinde kirli |
| Pet / binek / kozmetik / ses-müzik | Binary asset (`Content/`) gerektirir; bulut/ajan doğrulayamaz, `CheckPrPaths.py` korur |
| Soket / taş | `FMMOEquipmentInstance`'a `SaveGame` alanı ekler → kayıt JSON'u ve S04 V2 şeması değişir: S04 ile çakışır |
| Malzemeli zanaat (craft) | Yeni eşya türü (kayıt şeması) + ganimet tablosu (`MMOMob.cpp`) + `MMOItemDatabase.cpp` (`lv31-40-content` dalı) gerektirir. **Birleştirme bunun malzemesiz ilk dilimidir** |
| Lonca deposu | Eşya ikileme riski + klan ırk kuralı S10'da + kalıcılık S04'te: üç çakışma |
| Liderlik tablosu | Tüm kayıtları taramak gerekir; S04'te tek DB sorgusu olur, A4'te sunucular arası olur. Şimdi yazılırsa çöpe gider. **Şimdilik değmez** |
| Bildirim sistemi | Var (`Public/Core/MMONotify.h`) |
| Görev günlüğü | Pratikte HUD işi (kirli) |
| Denge araçları | Var (`EconomyModel.py`, `ProgressionModel.py`, `ContentMap.py`) |

**Seçilen: ekipman birleştirme.** Gerçek bir oyun döngüsü ekler (sadece sayı/etiket cilası değil), açık bir ekonomi kararını (sink) besler, ilk dilim tamamen yeni dosyalarda + tek satırlık bir `friend` bildirimiyle yazılır.

## 3. Kapsam

### 3.1 Kurallar (ilk dilim)

| # | Kural | Değer |
|---|---|---|
| R1 | Girdi | Tam **3** farklı `InstanceId`, hepsi `EquipmentBag`'de (kuşanılmış ve depodaki eşya kabul edilmez) |
| R2 | Aynılık | Aynı `ItemId`, aynı `Rarity` |
| R3 | Artı seviyesi | Üçü de `Plus == 0` `[PLACEHOLDER, karar A2]` (yükseltilmiş eşyaya harcanan altının kaybolmasını ve +N aklamayı önler) |
| R4 | Çıktı | Aynı `ItemId`, `Rarity + 1`, `Plus = 0`, **yeni** `InstanceId` |
| R5 | Tavan | Çıktı en çok **Destansı (Epic)**. Efsanevi yalnız ganimet `[PLACEHOLDER, karar A1]` |
| R6 | Bedel | `Fee = Round(GetBuyPrice(Def) × FeeMult[GirdiNadirligi])`, `FeeMult = {Common 0.5, Uncommon 1.0, Rare 2.0}` `[PLACEHOLDER]`. `bVendorSold=false` eşyalarda da `GetBuyPrice` formülü kullanılır (satılmasa da hesaplanabilir) |
| R7 | Yer | Sahip satıcı NPC'sine 800 u yakın (`IsOwnerNearVendor`, ölü karakteri zaten reddeder) |
| R8 | Altın | `Gold >= Fee` önceden doğrulanır; eksik altında hiçbir şey değişmez |
| R9 | Şans | Yok, sonuç kesin. Durum tutulmaz (sayaç/pity yok) |
| R10 | Atomiklik | Tüm doğrulama önce; mutasyon tek karede, delegeler **mutasyondan sonra** bir kez (`OnInventoryChanged` 1, `OnGoldChanged` 1). Takas yerleşiminin "önce sessiz yerleş, sonra yayınla" deseni (`MMOInventoryComponent.h:214`) |

### 3.2 Kapsam dışı (ilk dilimler)

Malzeme/tarif sistemi, şanslı birleştirme, +N taşıma, Efsanevi üretimi, toplu birleştirme, ayrı NPC (yerleşim S02'de), HUD düğmesi (D4'e kadar), başarım bağlantısı, pazar kilidi (S12 geldiğinde onun işi, §8 R4).

### 3.3 Mevcut sistemlerle ilişki

- `UMMOInventoryComponent`: tek veri sahibi. Birleştirme yeni alan eklemez; `EquipmentBag`, `Gold`, `NextInstanceId`, `AddEquipmentInstanceInternal(.., bNotify=false)`, `ApplyGoldInternal(.., bNotify=false)`, `IsOwnerNearVendor`, `FindBagIndexByInstanceId` kullanılır.
- `FMMOItemDatabase`: yalnız okunur (`Find`, `GetBuyPrice`, `GetRarityName`). **`MMOItemDatabase.cpp`'ye dokunulmaz** (`lv31-40-content` dalı orada).
- Takas: teklif edilen eşya birleştirilirse takas yürütmede eşyanın hâlâ çantada olduğunu yeniden doğrular (`MMOTradeComponent.cpp:38-58`) ve başarısız olur; ikileme yok. ~~Birleştirme takası ayrıca kilitlemez (ilk dilimde).~~ **Güncel (`1a8ec3f`):** aktif takas teklifindeki girdi `TradeLocked` ile reddedilir (`MMOFusion.cpp:22-31` `FusionIsInTradeOffer`, yalnız `UMMOTradeComponent` public alanları okunur; Trade dosyaları değişmedi).
- Satıcı sat/al, yükseltme: aynı satıcı yakınlık kuralı; birbirini etkilemez.
- Sohbet komutları `AMMOPlayerController::ServerSendChat_Implementation` içinde sunucuda ayrıştırılır (`f9b71c2`'de `MMOPlayerController.cpp:1372`; 200 karakter sınırı + 0.7 sn throttle `:1384-1390` (her denemeyi sayar), kontrol/biçim karakteri temizliği `:1392-1403`, `/yardim` `:1409`, `/posta` `:1647`; `SeenCommandPawn` kontrolü). D2 bu kalıba bir dal ekler (§11.1). (İlk yazımdaki "`ServerSay`, `:1380-1700`" adı yanlıştı.)

## 4. Dosya haritası

### 4.1 Yeni dosyalar

| Dosya | İçerik | Dilim |
|---|---|---|
| `Source/MyProject/Public/Items/MMOFusion.h` | `enum class EMMOFusionResult : uint8`, `struct FMMOFusionPreview`, `struct MYPROJECT_API FMMOFusion` (statik) | D1 |
| `Source/MyProject/Private/Items/MMOFusion.cpp` | Saf kurallar + sunucu yürütücüsü, `DEFINE_LOG_CATEGORY_STATIC(LogMMOFusion, ...)` | D1 |
| `Source/MyProject/Private/Tests/MMOFusionSelfTest.cpp` | `#if !UE_BUILD_SHIPPING`, `FSelfRegisteringExec` komutu `MMOTestFusion` (`MMOUpgradeCallbackSelfTest.cpp:150-165` deseni; **hiçbir mevcut dosyada kayıt satırı gerekmez**) | D1 |
| `Scripts/TestFusion.ps1` | Koşucu, `Scripts/TestAchievements.ps1` deseni | D1 |

### 4.2 Dokunulacak mevcut dosyalar

| Dosya | Tam değişiklik | Dilim | Durum |
|---|---|---|---|
| `Source/MyProject/Public/Components/MMOInventoryComponent.h` | Satır 214'ün (`friend class UMMOTradeComponent;`) altına **tek satır**: `friend struct FMMOFusion; // Silent fusion settlement before any observer.` | D1 | **Yapıldı** (`d7bb1b1`, satır 215) |
| `Source/MyProject/Private/Player/MMOPlayerController.cpp` | `/posta` dalından (satır ~1647) önce `/birlestir` dalı + `/yardim` metnine (`:1411`, `ClientReceiveChat` satırı) `/birlestir (canta no)`; gereksinimler §11.1 | D2 | **Kirli (2026-10-17: +4 satır, sahibi başkası): bekle** (§9) |
| `Scripts/RunSystemTests.ps1` | Diziye `@{ Name = 'Birlestirme (TestFusion)'; Script = 'TestFusion.ps1' }` | D2 | **Kirli (2026-10-17: başkası `TestRace.ps1` satırı ekledi): bekle** |
| `Scripts/RunAllChecks.ps1`, `docs/PLAYTEST_CHECKLIST.md` | Yalnız ekleme | D2 | Sıcak, son adımda |
| `Scripts/EconomyModel.py` | Yeni bölüm (d) Birleştirme | D0 | Temiz |
| `Source/MyProject/Public/Components/MMOInventoryComponent.h` + yeni `.cpp` | `UFUNCTION(Server, Reliable) void ServerFuseEquipment(int32 A, int32 B, int32 C);` + `double LastFusionServerTime`. `_Implementation` `MMOFusion.cpp`'de tanımlanır (StaticLint "hiçbir cpp'de" diye arar, `StaticLint.py:315`) | D4 | HUD sahipleri bitince |
| `MMOHUD.*` | Envanter penceresine "Birleştir" düğmesi | D4 | **Kirli: bekle** |

**Dokunulmayanlar:** `MMOSaveGame.h`, `MMOCharacterSaveJson.*`, `MMOCharacter.*`, `MMOGameMode.cpp`, `MMOItemDatabase.cpp`, `MMOTradeComponent.*`, `MMOPartyComponent.*`, `MMOInventoryComponent.cpp`, `Config/`, `Content/`.

### 4.3 API taslağı (D1, ASCII)

> **Güncel (`1a8ec3f`):** gerçek API `Public/Items/MMOFusion.h`'dedir ve taslaktan şu kadar farklıdır: enum'un sonuna `NotPlayer`, `Locked`,
> `TradeLocked` eklendi (mevcut değerler kaymaz); `Check` ve `FindFromSlot` önce ortak `private static GateResult` kapısından geçer
> (yetki → oyuncu kontrollü `AMMOCharacter` → `IsInitialLoadComplete` / `!IsTravelSaveCommitted` → satıcı yakınlığı, eşya kontrollerinden önce);
> `FindFromSlot`'ta seçilen slotun eşyası **her zaman ilk girdi**, kalan ikisi en küçük `InstanceId`'li eşdeğerler (aşağıdaki taslak yorumu
> "en küçük id'ler önce" eskidi; açık semantik soruları §11.4).

```cpp
// Public/Items/MMOFusion.h
enum class EMMOFusionResult : uint8
{
    Ok, NotAuthority, NotNearVendor, BadIds, DuplicateId, NotInBag,
    ItemMismatch, RarityMismatch, PlusNotZero, RarityCapped, UnknownItem,
    NotEnoughGold, InternalError
};

struct FMMOFusionPreview
{
    EMMOFusionResult Result = EMMOFusionResult::BadIds;
    FName ItemId;
    EMMOItemRarity From = EMMOItemRarity::Common;
    EMMOItemRarity To = EMMOItemRarity::Common;
    int32 Fee = 0;
    int32 InstanceIds[3] = {0, 0, 0};
};

struct MYPROJECT_API FMMOFusion
{
    static constexpr int32 InputCount = 3;
    static constexpr EMMOItemRarity MaxOutputRarity = EMMOItemRarity::Epic; // PLACEHOLDER
    // Pure: no world, no inventory.
    static bool NextRarity(EMMOItemRarity In, EMMOItemRarity& Out);
    static int32 FeeFor(const FMMOItemDef& Def, EMMOItemRarity Input);
    // Server read-only check (no mutation). Used by preview and by Execute.
    static FMMOFusionPreview Check(const UMMOInventoryComponent& Inv, int32 A, int32 B, int32 C);
    // Picks 3 matching bag items starting from the 1-based bag slot (lowest InstanceIds first).
    static FMMOFusionPreview FindFromSlot(const UMMOInventoryComponent& Inv, int32 BagSlot1Based);
    // Server only. Re-runs Check, then mutates silently, then broadcasts once.
    static EMMOFusionResult Execute(UMMOInventoryComponent& Inv, int32 A, int32 B, int32 C, int32* OutNewInstanceId = nullptr);
    static const TCHAR* ResultText(EMMOFusionResult R); // Turkish ASCII player message
};
```

`Execute` sırası: (1) `Check` (yetki, satıcı, 3 farklı id, çantada, aynı eşya/nadirlik, Plus 0, tavan, `Gold >= Fee`); (2) üç girdinin kopyasını ve indekslerini sakla; (3) indeksleri büyükten küçüğe `RemoveAt`; (4) `AddEquipmentInstanceInternal(ItemId, 0, To, false)`; başarısızsa kopyaları aynı indekslere geri koy, `InternalError` dön; (5) `ApplyGoldInternal(-Fee, TEXT("birlestirme"), false)`; dönen değer `-Fee` değilse (olmamalı) 3-4'ü geri al, `InternalError`; (6) denetim satırı + metrik; (7) `OnInventoryChanged.Broadcast()`, `OnGoldChanged.Broadcast(Gold)`; (8) `MMONotify::ToOwner(.., Result)`. (5)'teki `ApplyGoldInternal` `[MMOMETRIC] ev=gold src=birlestirme amount=-Fee` satırını kendisi basar.

## 5. Sunucu otoritesi, replikasyon, kimlik, kalıcılık

- **Kim değiştirir:** yalnız sunucu (`Execute` ilk satırda `Inv.GetOwner()->HasAuthority()` + `!IsBeingDestroyed()`; değilse `NotAuthority`, mutasyon yok). İstemci yalnız `InstanceId` üçlüsü ya da çanta numarası yollar; ücret, nadirlik ve çıktı **her zaman sunucuda yeniden hesaplanır**, istemciden gelen hiçbir sayı kullanılmaz.
- **Replikasyon:** yeni replike alan yok. `EquipmentBag` (`ReplicatedUsing=OnRep_Items`) ve `Gold` (`OnRep_Gold`) mevcut; HUD envanteri kendiliğinden yenilenir.
- **RPC:** D1'de yok. D2 sohbet komutu zaten sunucuda çalışan `ServerSendChat_Implementation`'dan gelir (yeni RPC gerekmez). D4'te (ayrıntı §11.2) `ServerFuseEquipment(A,B,C)` (Reliable) + 0.5 sn `[PLACEHOLDER]` bekleme, yükseltmedeki `LastUpgradeServerTime` deseni (`MMOInventoryComponent.h:259`).
- **Bayat görünüm (NETWORK_AUDIT Y4):** D2 iki adımlıdır: `/birlestir 4` yalnız önizleme yazar ("Kisa Kilic Siradan x3 -> Nadir, ucret 86. Onay: /birlestir onay 17 22 31"), mutasyon `/birlestir onay <id> <id> <id>` ile olur. Onay açık `InstanceId` taşır; arada çanta değişirse `Check` reddeder. Sunucuda bekleyen durum tutulmaz.
- **Kimlik:** işlem yalnız komutu veren pawn'ın kendi envanterinde. Oyuncu adı/slot ile hiçbir şey açılmaz; `MMONameRules` bu sistemi etkilemez. Bilinen K-1 açığı (isim = kimlik) satıştaki gibi aynen geçerli, yeni yüzey açılmaz. Metrikte `who` = `MMOMetrics::WhoName`.
- **Kalıcılık:** **yeni dosya, yeni alan, yan JSON yok.** Sonuç normal karakter kaydına (`FillSaveData`) girer; `Rarity` zaten `SaveGame` alanı ve JSON'da doğrulanıyor (`MMOCharacterSaveJson.cpp:153-154`). Çökme penceresi: son kayıttan önceki duruma dönülür (üç girdi + altın geri, çıktı yok): tutarlı, ikileme yok (tek kare, tek anlık görüntü). Başarım/günlüğün "önce kaydet sonra ödül" kuralı burada gereksiz: yeni değer yaratılmıyor, yalnız mevcut değer dönüştürülüyor. S04 DB'ye geçişte şema etkisi sıfır.
- **Denetim:** `LogMMOFusion Display: event=item_fuse actor=.. item=.. from=0 to=1 inputs=17,22,31 output=40 fee=86 balance=..` + `[MMOMETRIC] t=.. ev=fusion who=.. item=.. from=.. to=.. fee=.. lv=..`. `Scripts/PlaytestMetrics.py`'ye `ev=fusion` sayımı D2'de eklenir.

## 6. Ekonomi / denge etkisi

- **Yeni altın kaynağı: yok.** Yeni eşya kaynağı: yok (3 eşya → 1 eşya, net −2).
- **Sink büyüklüğü (formül, [Hesap], `GetBuyPrice = (Atk+Def)×12 + Lv×25 + 50`, `GetSellPrice = 0.4×Buy`):** bir birleştirmenin yok ettiği değer = ücret + vazgeçilen satış = Common→Uncommon **1.2×B**, Uncommon→Rare **1.86×B**, Rare→Epic **3.08×B**.

| Eşya (Lv) | B (alış) | Ücret C→U / U→R / R→E | 27 Sıradandan 1 Destansı: toplam ücret |
|---|---|---|---|
| Kisa Kilic `ShortSword` (1) | 171 | 86 / 171 / 342 | 9×86 + 3×171 + 342 = **1.629** |
| Obsidyen Kilic `ObsidianBlade` (25) | 1.587 | 794 / 1.587 / 3.174 | **15.081** |
| Buzbagli Kilic `FrostboundBlade` (51) | 2.813 | 1.407 / 2.813 / 5.626 | **26.728** |

- **Güç etkisi:** nadirlik çarpanı Uncommon 1.25, Rare 1.6, Epic 2.1 (`MMOItemDatabase.cpp:194-201`). `RequiredLevel` aynı kaldığı için kademe atlanmaz (satıcı seviye kapısının, `MMOInventoryComponent.cpp:947-949`, korumaya çalıştığı şey bozulmaz). Etki: "kendi kademende daha iyi sürüm" altınla satın alınabilir hale gelir.
- **Satıcıdan alıp birleştirme yolu:** Nadir = 3B + 0.5B = **3.5B**, Ender = 11.5B, Destansı = 34.5B + 2B = **36.5B** [Hesap]. Lv25'te Destansı Obsidyen ≈ 57.900 altın. Model gelirine göre Lv16+'da bu bir sink olarak istenen davranış [Muhtemel]; Lv1–9 altın kıt olduğu için orada fiilen kapalıdır.
- **Sömürü kontrolü:** satış arbitrajı yok: her adımda çıktının satış değeri (0.5B / 0.64B / 0.84B) üç girdinin satış değerinden (1.2B / 1.5B / 1.92B) düşük, ücret hariç bile.
- **Ölçüm ihtiyacı:** `ev=fusion` sayısı ve `src=birlestirme` altın toplamı / bant saatlik geliri; eşya toplama hızı bilinmiyor (bugün ganimet alma metriği yok, `ev=` listesi: gold, xp, kill, death, levelup, quest_*). Öneri (ayrı küçük iş): `AMMOLootPickup`'a `ev=item_pickup rarity=..` satırı.
- **D0 (`ekonomi-tasarimcisi`):** `EconomyModel.py`'ye bölüm (d): bant başına "saatte X birleştirme yapan oyuncunun gelir/gider oranı"; hedef: Lv16+ oranı 1 saat oyunda en az 1–2 birim aşağı çekmek [Tahmin], Lv1–15 etkisiz kalmalı. `FeeMult` sayıları buradan çıkar.

## 7. Kabul ölçütleri ve test planı

### 7.1 Dilimler ve "bitti" tanımı

| Dilim | İş | Bitti = |
|---|---|---|
| **D0** | `ekonomi-tasarimcisi`: `EconomyModel.py` (d) bölümü, `FeeMult` önerisi | `python Scripts/EconomyModel.py` çıktısında bölüm (d) tablosu; CI yeşil |
| **D1** — **TAMAM** (`d7bb1b1`, `df4c3c6`, `1a8ec3f`, `d7c75ed`) | `MMOFusion.h/.cpp`, `friend` satırı, `MMOFusionSelfTest.cpp`, `TestFusion.ps1` | Build 0 hata; `StaticLint.py` + `ValidateData.py` 0 hata; `TestFusion.ps1` `PASS checks=N` (N tam, CASE satırı = benzersiz etiket = N); mutasyonların hepsi yakalanır (§7.3). **Sonuç:** `PASS checks=180` (izole worktree; ana kopyada derleme kırık, §9); mutasyonlar §7.3 |
| **D2** — bekliyor (kirli dosyalar) | `/birlestir` sohbet dalı, `/yardim`, `RunSystemTests.ps1` + `RunAllChecks.ps1` adımı, `PLAYTEST_CHECKLIST.md` maddeleri, `PlaytestMetrics.py` `ev=fusion` | D1 testleri + yeni sohbet vakaları geçer; `NetSmokeTest.ps1` hata 0; `qa-kontrolcu` + `ag-guvenlik-denetcisi` engelleyici bulgu yok; güvenlik gereksinimleri §11.1 |
| **D3** — bekliyor (D2'ye bağlı) | PIE (kullanıcı) | §7.4 listesi GEÇTİ (`PLAYTEST_CHECKLIST.md` "Ekipman birleştirme" bölümü) |
| **D4** — bekliyor (HUD sahipleri) | HUD düğmesi + `ServerFuseEquipment` RPC (HUD sahipleri bitince) | RPC vakaları (bekleme, bayat id, uzak istemci, takastaki eşya, seyahat penceresi) + NetSmoke; gereksinimler §11.2 |
| **D0** — yapılmadı | (yukarıda) | `FeeMult` hâlâ PLACEHOLDER |

### 7.2 Native self-test (`MMOTestFusion`, D1)

Koşul: `-game -NullRHI -unattended` tek kullanımlık otorite dünyası (yükseltme testindeki gibi; değilse FAIL). Sahne: uzak bir noktada `AMMOCharacter` + 100 u yanında `AMMOVendorNPC` (`MMOPurchaseCallbackSelfTest.cpp:39` deseni), ikinci "Restore" pawn. Diske kayıt/yükleme yok (`FillSaveData`/`ApplySaveData` bellek içi). Satır biçimi: `[MMO_FUSION_CASE] PASS|FAIL case=.. check=..`, özet `[MMO_FUSION_TEST] PASS|FAIL checks=N reason=..`.

| Grup | Vaka | Doğrulanan |
|---|---|---|
| Saf | `NextRarity` 5 giriş | C→U, U→R, R→E true; E ve L false (tavan) |
| Saf | `FeeFor` 3 nadirlik × 3 eşya | §6 tablosundaki sayılar (tam eşitlik) |
| Başarı | C→U, U→R, R→E (üç ayrı vaka) | Çantadan 3 id gitti, 1 yeni id geldi (tüm eski id'lerden farklı, `NextInstanceId` arttı), `ItemId` aynı, `Rarity` +1, `Plus` 0; `Gold` tam `-Fee`; kuşanılanlar ve `Items` yığınları değişmedi |
| Başarı | Delegeler | Prob: `OnInventoryChanged` tam 1, `OnGoldChanged` tam 1; ikisinde de gözlenen durum **son** durum (çanta zaten 3→1, altın zaten düşük) |
| Başarı | Kayıt gidiş-dönüş | `FillSaveData` → Restore pawn `ApplySaveData` → çıktı eşyası aynı `ItemId/Rarity/Plus` |
| Başarı | `FindFromSlot` | Karışık çantada (farklı eşya/nadirlik/Plus aralarda) doğru 3 id seçilir, en küçük id'ler önce |
| Ret (her biri: anlık görüntü önce = sonra, delegesi 0) | `BadIds` (0 id), `DuplicateId` (A==B), `NotInBag` (yok id), kuşanılmış eşya id'si, `ItemMismatch`, `RarityMismatch`, `PlusNotZero`, `RarityCapped` (3 Epic, 3 Legendary), `NotEnoughGold` (Gold = Fee−1), `NotNearVendor` (satıcıdan 2.000 u), ölü karakter (`bIsDead`) | Doğru `EMMOFusionResult`; hiçbir alan değişmedi |
| Sınır | Gold = Fee tam | Başarılı, `Gold` 0 |
| Sınır | Çanta tam 12 dolu | Başarılı (3 çıkar, 1 girer → 10) |
| Tekrar | Aynı üçlüyle ikinci `Execute` | `NotInBag`, mutasyon yok (bayat onay) |

Kesin kontrol sayısı test yazılınca sabitlenir; koşucu eşiği o sayıya eşit olur (`TestAchievements.ps1` deseni: özet = CASE sayısı = benzersiz etiket sayısı, 90 sn watchdog, `Error:`/`Ensure` satırı 0).

**Güncel (`d7c75ed`): eşik 180** (`Scripts/TestFusion.ps1:46-48`): 5 `NextRarity` + 9 `FeeFor` + 3×16 başarı (C→U, U→R, R→E) + 10 `FindFromSlot`
+ 20×4 ret + 2 mob sahibi + 4×2 sınır + 18 geri çağırım (aynı üçlüyle `OnInventoryChanged` içinden ikinci `Execute` → `NotInBag`, tek ücret;
başka üçlüyle → iki çıktı, iki ücret; gözlemci `OnGoldChanged`'de altın harcar; gözlemci sahibi yok eder → çökme yok). Test yalnız `NM_Standalone`'da
koşar (dedicated/listen ret), pawn gerçek `AMMOPlayerController`'a possess edilir. Kayıt gidiş-dönüşü planlandığı gibi diske değil bellek içi
(`FillSaveData` → Restore pawn `ApplySaveData`, `MMOFusionSelfTest.cpp:248-249`); gerçek disk kaydı/yeniden giriş ve sohbet/RPC yolu test edilmedi (D2/D4 yok).

### 7.3 Mutasyonla sınanacak korumalar (`test-muhendisi`)

Her biri tek başına geri alınınca `TestFusion.ps1` **FAIL** vermeli (geri yükledikten sonra dosyaya touch, CLAUDE.md notu):
1. `Gold >= Fee` kontrolü silinir. 2. Aynı nadirlik kontrolü silinir. 3. Tavan kontrolü silinir (`MaxOutputRarity`). 4. Farklı id kontrolü silinir (A==B). 5. `IsOwnerNearVendor` çağrısı silinir. 6. Ücret 0'a sabitlenir. 7. Delegeler mutasyondan önce yayınlanır. 8. `Plus == 0` kontrolü silinir. 9. Çıktıya girdi `InstanceId`'si verilir (yeni id yerine).

**Sonuç (worktree `fusion-d1`, her mutasyon ayrı derleme + `TestFusion.ps1`, kaynak bayt bayt geri alındı; loglar `%TEMP%\mmo_fusion_mut_*`, `mmo_fusion_m01…m15_*`):**
- `df4c3c6` turu: yukarıdaki 9 + 13 ek mutasyon = **22/22 yakalandı** (ilk turda kaçan `IsOwnerNearVendor(1500)` / `(400)` eşik mutasyonları için 850 u ret, 750 u başarı vakası eklendi).
- `d7c75ed` turu (`1a8ec3f` korumaları, `m01-m15`): **13 yakalandı** (11'i `FAIL checks=180`, `m06`/`m07` dizi sınırı assert'iyle çöktü = koşucu FAIL), **2 beklendiği gibi yakalanmadı** (`m12`, `m13` `PASS checks=180`): ulaşılamaz ücret iade dalı ve dedicated koruması (test yalnız Standalone koşar). Hangi `mNN`'in hangi koruma olduğu commit mesajından okundu; dosya adında yazmıyor.

### 7.4 Oyun içi (PIE) ilk test listesi (D2 sonrası; 2026-10-17'de `PLAYTEST_CHECKLIST.md` "Ekipman birleştirme" bölümüne eklendi, D2 gelene kadar denenemez)

1. Satıcı yanında 3 Sıradan aynı eşya: `/birlestir <no>` önizleme doğru ücreti yazar, altın değişmez.
2. `/birlestir onay ...` sonrası çantada 1 Nadir (yeşil isim), altın tam ücret kadar düştü, HUD anında yenilendi.
3. Satıcıdan uzakta: "Satici yakininda degilsin", hiçbir şey değişmez.
4. Altın yetersiz: ret mesajı, çanta aynı.
5. Önizlemeden sonra girdilerden birini sat, eski onayı gönder: ret.
6. Oturumu kapat-aç: Nadir eşya kayıtta duruyor.
7. Listen-server'da **uzak istemci** ile 1–2'yi tekrarla: mesajlar uzak istemciye sohbet satırı olarak geliyor.
8. Takasa koyduğun eşyayı birleştir, takası onayla: takas başarısız, eşya ikilenmedi.
9. Sunucu logunda `ev=fusion` ve `src=birlestirme` satırları tam bir kez.

## 8. Riskler

| # | Risk | Önlem |
|---|---|---|
| R1 | `FeeMult` yanlış: sink ya etkisiz ya da sistemi kimse kullanmaz | D0 modeli + `ev=fusion` ölçümü; sabitler tek yerde (`MMOFusion.cpp`) |
| R2 | Satıcıdan alıp birleştirme altını güce çevirir | Kademe atlanmaz (aynı `RequiredLevel`); Efsanevi kapalı; ölçümle `FeeMult` |
| R3 | Bayat görünümde yanlış eşya | İki adımlı onay, açık `InstanceId`; D4 RPC'de de id üçlüsü |
| R4 | S12 pazar tezgâhındaki eşya birleştirilir | S12 tasarımı "tezgâhtaki eşya kilitlenir" diyor (`SIX_MAP_PLAN.md:366`); kilit kontrolü `FMMOFusion::Check`'e eklenmeli: S12 kabul listesine not |
| R5 | Unity-build adı çakışması | Anonim namespace adları `Fusion` önekli (CLAUDE.md tuzak notu); log kategorisi `LogMMOFusion` |
| R6 | `friend` satırı Codex'in gelecekteki envanter işiyle çakışır | Tek satır, yalnız ekleme; commit öncesi `git status` ile dosya temiz mi bak |

## 9. Codex / Antigravity çakışma planı

- **D1 dosyaları:** 4 yeni dosya + `MMOInventoryComponent.h`'de 1 satır. Bugün hiçbir worktree/dal ve main'deki kirli küme bu dosyalara dokunmuyor (§2.1). Ajan başlamadan `git -C <repo> status --short` ve `git -C <repo> diff --name-only` ile yeniden kontrol eder; `MMOInventoryComponent.h` kirliyse D1'i durdurur, raporlar.
- **Derleme:** "tek derleyici" kuralı. Codex S03/S04 aday dalda derliyorsa ortak kilit (`agent-build.lock`) ve UE/UBT süreç kontrolü; editör kapalı. Main'deki kirli Race/HUD dosyaları da derlemeye girer: başkasının kodu derlenmezse ajan **dokunmaz**, raporlar ve bekler (derleme hatası kendi dosyalarında değilse D1 "bitti" sayılmaz ama kod commit edilebilir, durum açık yazılır).
- **D2/D4:** `MMOPlayerController.cpp`, `RunSystemTests.ps1`, `MMOHUD.*` temizlenene (sahibi commit edene) kadar bekler. Beklerken D0 ve D1 ilerler; bekleme döngüsü yok, durum tabloya yazılır.
- **Commit:** dosyalar adla (`git -C ... add <yol>`), `git add -A` yok, push yok (post-commit hook).
- **Durum (2026-10-17, `f9b71c2`):** D1 bu plana göre izole worktree'de derlendi/test edildi ve commit'lendi. Ana kopya hâlâ derlenmiyor: izlenmeyen `Public/World/MMOZoneLayout.h` + `Private/World/MMOZoneLayout.cpp` (`StaticLint.py` ana kopyada 7 HATA, hepsi bu iki dosyada: `ServerChangeZone` RPC'si `bool` dönüyor (UHT hatası), `Replicated` olmayan `bIsActive`/`ZonePlayerCounts` `DOREPLIFETIME`'da, eksik `GameplayStatics.h`); worktree'de (master'la aynı fusion dosyaları, Zone dosyaları yok) `StaticLint.py` temiz, 134 dosya. Derleme hatasını belge denetçisi kendisi derleyerek görmedi; `d7bb1b1` mesajına ve StaticLint'e dayanır. Bu dosyaların sahibi belli değil: Codex'in sprint tahtası da main'deki `MMOZoneLayout` çiftine "sahipsiz, farklı bir taslak" diyor (`SIX_MAP_SPRINT_BOARD.md:49`, `:153`). `MMOPlayerController.cpp`, `RunSystemTests.ps1`, `MMOHUD.*` hâlâ kirli → D2/D4 bekliyor.

## 10. Açık sorular (kullanıcı kararı)

| # | Soru | Varsayılan öneri |
|---|---|---|
| A1 | Efsanevi birleştirmeyle üretilebilsin mi? | Hayır (yalnız ganimet) |
| A2 | Artılı (+N) eşya birleştirilebilsin mi? (örn. üçü aynı +N ise çıktı +N) | İlk dilimde hayır |
| A3 | Ücret satıcı alış fiyatına mı bağlı, yoksa seviyeye bağlı ayrı tablo mu? | Alış fiyatı (tek formül, `GetBuyPrice` zaten seviyeyle büyür) |
| A4 | Satıcı yerine ayrı bir "Demirci" NPC'si? | Sonra; NPC yerleşimi S02'nin dosyalarında |

**Durum (2026-10-17):** dördü de **açık**; D1 kodu önerilen varsayılanlarla yazıldı (A1: `MaxOutputRarity = Epic`, A2: `PlusNotZero`, A3: `GetBuyPrice × FeeMult`, A4: satıcı yakınlığı). Karar bloğu `DECISIONS_NEEDED.md` "Ekipman birleştirme A1-A4".

## 11. D1 sonrası gereksinimler ve açık maddeler (QA + güvenlik denetimi, 2026-10-17)

Kod yok; aşağıdakiler D2/D4 yazılırken uyulacak kurallar. Satır numaraları `f9b71c2`'deki commit'li dosyalardır (çalışma kopyası kirli olabilir).

### 11.1 D2: `/birlestir` sohbet komutu

- **Yer:** yeni dal `AMMOPlayerController::ServerSendChat_Implementation` içinde (`MMOPlayerController.cpp:1372`), uzunluk sınırı + 0.7 sn throttle (`:1384-1390`) ve kontrol/biçim karakteri temizliğinden (`:1392-1403`) **sonra**; yani dal bu ortak kapıların hiçbirini atlamaz. `/posta` dalından (`:1647`) önce; `/yardim` metnine (`:1411`) `/birlestir (canta no)`.
- **Pawn:** mevcut `SeenCommandPawn == GetPawn()` deseni (`:1449`): `IsValid(SeenCommandPawn) && SeenCommandPawn == GetPawn()` değilse sessizce dön; envanter yalnız bu pawn'ınki. Oyuncu adı/slot ile başka envanter açılmaz.
- **Katı sayı ayrıştırma:** her belirteç boş değil, **en fazla 10 hane** ve karakter karakter yalnız ASCII `0-9`; `FCString::Atoi("17abc")` gibi önek kabulü **yok**, işaret/boşluk/ondalık yok. Dikkat: motorun `FCString::IsNumeric`'i tek başına **yetmez**: baştaki `-`/`+` işaretini, tek bir `.`'yı ve boş dizgeyi kabul eder (`Engine/Source/Runtime/Core/Public/Misc/CString.h:139-166`); `-5` ve `1.5` bu yüzden ayrıca reddedilmeli. Biçim: tam **1** belirteç (çanta numarası) ya da `onay` + tam **3** belirteç (kimlik). Fazla/eksik belirteç ret. Çanta numarası `[1, 12]` (çanta boyutu) aralığında olmalı; aralık dışı (`0`, `13`, `-5`) **ret**, sessizce sınıra kırpılmaz (kırpma `13`'ü 12. slota çevirip oyuncunun seçmediği eşyayı önizlerdi; `PLAYTEST_CHECKLIST.md` maddesi de ret bekliyor). `FindFromSlot` ayrıca `EquipmentBag.Num()` ile sınar (`MMOFusion.cpp:200`). 10 haneyi aşan ya da `int32`'ye sığmayan kimlik ret (taşma yok).
- **İki adımlı akış:** `/birlestir <no>` yalnız önizleme (`FindFromSlot`, mutasyon yok) ve açık kimlik üçlüsüyle onay satırı yazar; `/birlestir onay <id> <id> <id>` `FMMOFusion::Execute`'u çağırır. **Sunucuda bekleyen durum tutulmaz**; ücret ve nadirlik istemciden **gelmez** (komutta hiç yer almaz), `Execute` her şeyi yeniden hesaplar.
- **Sabit hata mesajları:** oyuncuya yalnız `FMMOFusion::ResultText` ya da sabit bir kullanım satırı döner; kullanıcı girdisi mesaja yankılanmaz (biçim/bant istismarı yok).
- **Debug komutu:** `UFUNCTION(Exec)` bir test komutu eklenirse uzak istemci onu `ServerExec` ile sunucuda çalıştırabilir (motor `PlayerController.cpp:1879-1883`, `ServerExecRPC_Implementation` Shipping dışı tüm yapılarda `ConsoleCommand`); bu yüzden böyle bir komut yalnız komutu verenin **kendi pawn'ında** `Execute` çağırmalı, başka oyuncu/aktör seçememeli; tercihen hiç eklenmemeli (`MMOTestFusion` `FSelfRegisteringExec`'tir, `#if !UE_BUILD_SHIPPING`).
- **D2 ile gelecek diğerleri:** `RunSystemTests.ps1` + `RunAllChecks.ps1` adımı, `PlaytestMetrics.py` `ev=fusion` sayımı, `PLAYTEST_CHECKLIST.md` "Ekipman birleştirme" bölümü (maddeler orada hazır), sohbet ayrıştırma vakaları (`0`, `13`, `-5`, `17abc`, 11 hane, 2/4 kimlik, başkasının pawn'ı).

### 11.2 D4: HUD düğmesi + RPC

- `UFUNCTION(Server, Reliable) void ServerFuseEquipment(int32 A, int32 B, int32 C)` → doğrudan `FMMOFusion::Execute(*this, A, B, C)`. **Ücret/nadirlik/çıktı parametresi yok.**
- **Bekleme:** `double LastFusionServerTime`, `GetWorld()->GetTimeSeconds()` (sunucu saati) ile, **`Check`'ten önce**; bekleme içindeki çağrı sessizce düşer (mesaj yok), `LastUpgradeServerTime` deseni (`MMOInventoryComponent.h:259-260`, `.cpp:831-835`, yükseltmede 0.3 sn). Süre `[PLACEHOLDER]` (§5'te 0.5 sn). Zaman damgasının yalnız başarılı denemede mi her denemede mi güncelleneceği açık (yükseltme yalnız kabul edilen denemede günceller `.cpp:844`; sohbet throttle'ı her denemede `:1390`); öneri: her denemede (ret mesajı yağmuru da sınırlanır).
- **`_Validate`** (eklenirse) yalnız ucuz sınır kontrolü (`A, B, C > 0`); envanter okuması yok (ret = bağlantı kopar, ağır mantık oraya girmez).
- **Bayat görünüm:** HUD önizlemede gösterdiği kimlik üçlüsünü yollar; sunucu `Execute` içinde `Check`'i yeniden koşar, arada çanta değiştiyse ret.
- **Test vakaları:** uzak istemci (listen + istemci, `NetSmokeTest` deseni), bekleme içinde ikinci çağrı düşer, bayat kimlik (`NotInBag`), takastaki eşya (`TradeLocked`), seyahat/yükleme penceresi (`Locked`).

### 11.3 Açık / ertelenen bulgular

| # | Madde | Durum |
|---|---|---|
| F7 | Oyun (`Source/MyProject.Target.cs`) hedefinde `bUseLoggingInShipping` yok (yalnız `MyProjectServer.Target.cs:20`'de var): Shipping listen-server'da `LogMMOFusion` denetim satırı ve `[MMOMETRIC] ev=fusion` (ve diğer tüm `UE_LOG` metrikleri) kaybolur | Ertelendi; birleştirmeye özgü değil, Target değişikliği ayrı iş (Shipping listen-server senaryosu kararına bağlı) |
| D-3 / F2 | Ulaşılamaz geri alma dalları: `AddEquipmentInstanceInternal` başarısızlığı (`MMOFusion.cpp:292-300`) ve kısmi ücret kesintisi (`:304-317`); `Check` çanta yerini (3 çıkar, 1 girer) ve `Gold >= Fee`'yi önceden doğruladığı için tetiklenemez, test edilemez (mutasyonu yakalanmadı, beklenen). İade `ApplyGoldInternal(-Applied, "birlestirme-iade")` ile metrik dengeli | Kabul; dal savunma amaçlı kalır |
| — | **`FindFromSlot` semantiği:** (a) tıklanan slot her zaman ilk girdi; (b) kalan ikisi eşdeğerlerin en küçük kimlikleri, **takastaki eşdeğerler atlanmaz**: en küçük kimlikli kopya takastaysa üçüncü serbest kopya olsa bile sonuç `TradeLocked`; (c) seçilen eşya Destansı/Efsanevi ise eşdeğer yoksa `BadIds`, varsa `RarityCapped`. Takastakileri atlamak mı, bugünkü gibi reddetmek mi? | Açık (D2 önizleme metni buna göre); öneri: takastakileri atla |
| A1-A4 | §10 | Açık, `DECISIONS_NEEDED.md` |
| D0 | `FeeMult`, `MaxOutputRarity` PLACEHOLDER | Yapılmadı |
