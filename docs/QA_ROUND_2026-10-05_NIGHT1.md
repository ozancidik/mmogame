# QA turu — 2026-10-05 gece 1

Kapsam: `abe6ad6` (tıkla-saldır oyuncu hedefi), `138c642` + `cb63d6d` (davet kapma korumaları + Lv1-30 görev metinleri; iki commit **aynı içerik**, `git diff 138c642 cb63d6d` boş, merge `d3b75d9` çift eklemeye yol açmamış), `0604bad`, `64c2358` (yalnız belge) ve bunların etkileştiği kod. Denetim **salt okunur**: kod değiştirilmedi, commit atılmadı, `PLAYTEST_CHECKLIST.md`'ye dokunulmadı.

**Oyunda (PIE) hiçbir şey denenmedi.** "Doğrulandı" = kaynakta/araç çıktısında görüldü; "doğrulanmalı" = PIE ya da ağ testi gerekir.

Uyarı: çalışma ağacında Codex'in **commit edilmemiş** değişiklikleri vardı (`MMOCharacter.cpp` `MMODebugCommandsEnabled` kapıları, `MMOInventoryComponent.*` `MaxGold`, `MMOTradeComponent.cpp` `MaxGold` karşılaştırması, iki belge). Tek-dosya derlemeler çalışma ağacını derler, yani bu değişiklikleri de kapsar; "commit'lenmiş durumun derlemesi" ayrıca yapılmadı.

## 1. Derleme kanıtı

`Build.bat MyProjectEditor Win64 Development -WaitMutex -NoPCH -NoLink -SingleFile=<dosya>` (editör kapalıydı; rsp'de `/Yu` yok, `/W4` var — `Intermediate/.../SingleFile/*.rsp` kontrol edildi):

| Dosya | Sonuç | Uyarı |
|---|---|---|
| `MMOCharacter.cpp` | Succeeded | 0 |
| `MMOTradeComponent.cpp` | Succeeded | 0 |
| `MMOPartyComponent.cpp` | Succeeded | 0 |
| `MMOQuestTypes.cpp` | Succeeded | 0 |
| `MMOGameMode.cpp` | Succeeded | 0 |
| `MMOPlayerController.cpp` (ek) | Succeeded | 0 |

Not: UE'nin varsayılan bastırmaları (`/wd4100`, `/wd4244`, `/wd4267`, `/wd4819`) açık; "sıfır uyarı" bu bastırmalar dışındakiler için geçerli. Ek olarak `Scripts/ValidateData.py` ("Veri denetimi temiz", yalnız Lv28 sonrası beceri boşluğu UYARI'ları) ve `Scripts/StaticLint.py` (71 dosya, temiz) çalıştırıldı; ikisi de dosya yazmaz. Tam Shipping/bağlama yapılmadı.

## 2. Bulgular (ciddiyet sırasıyla)

### ORTA-1 — PvP kapalıyken (varsayılan) oyuncuya tıklamak kalıcı "kovala + her 0,6 sn RPC" döngüsü kuruyor (`abe6ad6`, doğrulandı kodda / PIE yok)

- `MMOPlayerController.cpp:855` oyuncuya tıklayınca `TargetEnemy = HitChar`. `PlayerTick` (`:1139-1164`) menzildeyken **her karede** `AttackTarget(Enemy)` çağırır.
- `AttackTarget` istemcide yalnız hub + cooldown kapısından geçer, sonra `ServerAttackTarget(Target)` (Reliable) yollar (`MMOCharacter.cpp:1067-1070`). `bAllowPvP` yalnız sunucuda (`GameMode`); istemci PvP durumunu **bilemez**, dolayısıyla reddi önceden öngöremez.
- Sunucu `PvP kapali` ipucunu yollar (`ClientShowHint`, Unreliable, aynı metin 3 sn'de bir) ama **hedefi bırakmaz**. Sonuç (varsayılan `bAllowPvP=false`, yani normal oyunda): oyuncuya tıklayan herkes (ticaret/inceleme niyetiyle bile) o oyuncuyu sonsuza kadar kovalar, yanında durup ~1,7 RPC/sn gönderir, 3 sn'de bir kafasında "PvP kapali" balonu çıkar; `TargetEnemy` açık kaldığı için otomatik mob saldırısına geçmez (eski `nullptr` davranışında en azından yakındaki moba vururdu). Kalıcı sohbet spamı **yok** (`ClientShowHint` kalıcı satır bırakmaz) — önceki ORTA-3 gerçekten kapandı.
- Aynı durum `Parti uyesine vuramazsin` / `Klan uyesine vuramazsin` için de geçerli (parti/klan arkadaşına tıklamak = onu kovalamak + 3 sn'de bir ret balonu).
- Öneri: (a) sunucu PvP/parti/klan reddinde istemciye hedefi bırakma bilgisi yollasın (`ClientClearAttackTarget` gibi Unreliable RPC ya da mevcut `ClientShowHint` yanında), (b) ya da `bAllowPvP`'yi `AGameState`'e replike edip istemci `DoLeftClickAction` içinde oyuncu hedefini PvP kapalıyken hiç `TargetEnemy` yapmasın (yalnız seçili hedef olsun, kovalamasın). Tasarım kararı: oyuncuya tıklamak PvP kapalıyken "seç ama saldırma" olmalı mı?
- **Test (PIE gerekli):** aşağıda T1.

### ORTA-2 — Parti daveti: aldığı daveti reddetme yolu yok, ret sessiz, kilit 60 sn (`138c642`/`cb63d6d`, doğrulandı kodda)

- `MMOPartyComponent.cpp:194-206` iki yeni erken `return`: aynı davetçiden tekrar (sessiz) ve başka davetçinin **60 sn** (`InviteExpirySeconds`) geçerli daveti varken (sessiz). Davet kapmayı gerçekten engelliyor (doğrulandı), ama yan etkileri:
  1. Parti için **ret/iptal komutu yok** (`MMOPartyAccept`, `MMOPartyLeave`, `MMOPartyKick` var; `ServerCancel` karşılığı yok). Yakındaki kötü niyetli biri, hedefe davet atıp 60 sn'de bir yenileyerek hedefin **parti davet kutusunu kalıcı işgal edebilir** (kendi daveti kabul edilene/zaman aşımına kadar kimse onu davet edemez; hedef daveti görmezden gelir). Ticarette bu `/trade iptal` (`ServerCancel` bekleyen daveti de düşürür, `MMOTradeComponent.cpp:~312`) ile aşılır; partide aşılmaz.
  2. Reddedilen davetçi **hiçbir şey görmez**: ret dalları sessiz, başarı mesajı da yalnız `IsLocallyControlled()` içinde (`:215`), yani uzak istemci davetçi zaten hiç geri bildirim almıyor (önceden var olan B5 boşluğu, bu commit sayesinde "davet etti mi, reddedildi mi" ayırt edilemez oldu). Ticarette bu iş `NotifyOwner` ile düzgün yapılmış.
  3. Hedefin zaten aynı partide olması, ölü olması, davetçinin ölü olması kontrol edilmiyor (ticarette `TradeIsDead` var). Kozmetik/spam: ölü ya da zaten partideki oyuncuya davet gider.
- Öneri: `MMOPartyDecline` (ya da `ServerLeave` bekleyen daveti de düşürsün); ret dallarına `MMONotify::ToOwner(Inviter, ...)` ("Oyuncunun bekleyen baska bir daveti var" / "Davet zaten gonderildi"); kilit süresini kısalt (15-20 sn) ya da "daha yeni davet, ≥N sn eskiyi ezer".

### ORTA-3 — Görev penceresi detay satırı uzadı; tek satır, kırpma/sarma yok (`cb63d6d`, doğrulanmalı — görsel)

- `MMOHUD.cpp:2462` her alınabilir görev için `Description + "   Odul: N XP, N altin, +Eşya"` tek `DrawText` (ölçek 0,64). Panel 480 px (`PW`), satır genişliği 452 px (`RowW`), **yatay kırpma/sarma yok** (`DrawQuestRowText` yalnız dikey sığdırır).
- Bu commit açıklamaları ortalama ~46'dan ~63 karaktere çıkardı (örn. `Q_Warlocks` 60→87, `Q_ObsidianWarlord` 58→82, `Q_Gold1000` 20→61, `Q_Goblins` 43→68). Ödül soneki (+ eşya adı) ile 120+ karakterlik tek satır olur; panel dışına taşıp dünyanın üstüne çizilmesi bekleniyor (eski metinlerde de bazıları taşıyor olabilirdi; **ölçülmedi**).
- Öneri: açıklamayı ayrı satıra al ya da `GetTextSize` ile genişliğe göre kısalt/sar; ya da açıklamaları ≤~55 karaktere çek. **Test:** T6.

### DÜŞÜK-1 — Doğum korumalı hedefe PvP vuruşu: sıfır hasarla saldıranın korumasını/combat durumunu etkiler (doğrulanmalı; yalnız PvP açıkken)

- `TakeDamage` doğum koruması dalı 0 döner (`MMOCharacter.cpp:~872`) ama `AttackTarget` bunu bilmez: PvP/parti/klan/hub kontrolleri geçtikten sonra `EndSpawnProtection()` (saldıranın **kendi** korumasını bitirir) + `Stats->MarkCombatActivity()` + `MulticastMeleeFX` çalışır, host saldıranda "VURDUN: 0 hasar" yazısı çıkar (`:1153-1177`). Yani yeni yeniden doğmuş birine tıklayıp vuran oyuncu hasar vermeden savaşa girer (sınıf değiştiremez, bölge geçişi `IsInCombat()` ile bloklanabilir) ve korumasını kaybeder.
- Öneri: `AttackTarget`'ta `Victim->IsSpawnProtected()` ise "Hedef korumali" ipucu verip erken çık (cooldown yine tüketilebilir).

### DÜŞÜK-2 — `TakeDamage` merkezi kapısı: saldıran çözülemezse kontrol atlanıyor (eski ORTA-4, **hâlâ açık**, bugün erişilemez)

- `MMOCharacter.cpp:880-899` değişmedi: `EventInstigator->GetPawn()` null **ve** `DamageCauser` bir `AMMOCharacter` değilse PvP/parti/klan/hub denetimi tümden atlanır. Bugün oyuncu→oyuncu DoT/AoE/mermi yolu yok (skill hedefleri yalnız `AMMOMob`), yani tetiklenemez; ileride ekleneceği zaman zorunlu madde olarak kalsın.

### DÜŞÜK-3 — `LooksLikeOsDefaultName` kalan dar çakışma (eski ORTA, **büyük ölçüde kapandı** `753e930`)

- `MMOGameMode.cpp:100-122`: tam 20 karakter + `-` sonrası ≥4 hane hex + en az bir rakam. Örn. `MehmetCelikoglu-2024` (tam 20 karakter) mesru ad olduğu hâlde `Gezgin######` yapılır. Oyuncuya "Baglanti adin uygun olmadigi icin ..." bildirimi gidiyor (sessiz değil). Düşük olasılık; yalnız bilgi.

### DÜŞÜK-4 — Metin/içerik küçük notlar (görev ID/hedef/ödül değişmedi — bkz. 3)

- `Q_Trolls`: yeni "Kiyilarda dolasan trolleri avla"; `Lore.md:722` NPC repliği "Ormanin kiyisinda", `Lore.md:30` "Bataklik kiyisinda" der; üç yer üç ayrı yer. Yer kararı bekliyor (`CONTENT_TEXT_GAPS.md` §3 soru 5, belge de bunu not ediyor). Bilgi.
- Kael yedek repliği `Goblin tek basina korkutmaz. Besi birden cikarsa sirtini tasa ver.` (`MMOGameMode.cpp:788`): "sırtını taşa ver" yaygın bir Türkçe deyim gibi durmuyor (doğrulanmalı — `metin-yazari` karar versin). Yorum satırı "docs/NpcDialogue.md'den" der ama bu satır `NpcDialogue.md`'de yok (belge-kod kayması).
- `Q_FrostWraiths` açıklaması "Uzun Kis olulerini": ASCII zorunluluğu yüzünden `Uzun Kış'ın` iyelik eki düştü; anlam hâlâ okunur.
- `MMOGameMode.cpp:29` yorum satırında bozuk bayt (mojibake, `Varsay�lan HUD`) — **önceden var**, yalnız yorum, derlemeyi etkilemiyor (`/wd4819`); dosya CP1254 olarak kaydedilmiş olabilir (doğrulanmalı).

### DÜŞÜK-5 — Açık kalan eski bulgular (bu turda değişmedi)

- `PreLogin` çevrimiçi-ad reddi, çökmeden dönen oyuncuyu eski bağlantı zaman aşımına kadar kilitler (`MMOGameMode.cpp:~251-264`, doğrulanmalı).
- Altın kayıt tavanı: **commit'lenmiş** durumda `ApplySaveData` ile `AddGold` tavanları uyuşmaz; Codex'in commit edilmemiş değişikliği (`MMOInventoryComponent::MaxGold`) bunu kapatıyor görünüyor — commit edilince kapanır.
- `MMOQuestComponent.cpp:~262` kill-ilerleme satırı yalnız yerel istemciye (bilinçli: HUD takibi var); `ClaimReward` erken dönüşleri hâlâ mesajsız (doğrulanmalı).
- `MMOHUD.cpp` görev balonu bulguları (`0604bad` içindeki DÜŞÜK-1/2/3) ve `MMOProbe*` (a)-(d) açık.

## 3. Doğrulanan sağlam noktalar (sessiz geçilmesin)

**abe6ad6 (tıkla-saldır)**
- Tek satırlık değişiklik doğru yönde: `AttackTarget(Enemy)` (`:1163`). Eski `MMOAttack()`→`AttackTarget(nullptr)` oyuncu hedefini kaybediyordu; artık oyuncu hedefi sunucuya taşınıyor.
- **PvP kapalı varsayılan korunuyor:** `bAllowPvP=false` (`MMOGameMode.h:52`, `EditDefaultsOnly`); `Content` içinde `bAllowPvP` geçersiz kılması yok (binary tarama, 0 sonuç); `MMOPvP` yalnız host'un yerel controller'ı için (`ServerSetPvP`, `:1261-1280`).
- **Sunucu-otoriter doğrulama eksiksiz** (`ServerAttackTarget` → `AttackTarget`): ölü/yok hedef → en yakın mob'a düşme (oyuncu hedefi **canlıysa** mob'a düşmez); `Victim == this` red; menzil `MeleeRange*1.3` (3B) sunucuda; `IsPvPAllowed()` → `CanHarmPlayer` (parti, klan, hub 700u, hem saldıran hem hedef konumu) → hasar. İstemcinin yalnız `Target` işaretçisini seçmesi var; konum/menzil/cooldown/kurallar sunucuda.
- **Rastgele hedef:** istemci başkasının `AMMOCharacter`'ını hedef gösterebilir ama bunun getirisi yok: aynı kapılardan geçer; menzil dışı sessizce düşer. `AMMONpcBase` bir `AMMOCharacter` değil (`ACharacter`), NPC'ler `ServerAttackTarget` parametresi olamaz (RPC serileştirmede tür uyumsuzluğunda motor null yapar — **doğrulanmalı**, motor davranışı). `IsPlayerPawn = !AMMOMob`: ileride `AMMOCharacter`'dan türeyen başka bir NPC eklenirse oyuncu sayılır (güvenli yönde, PvP kapısına takılır).
- **Merkezi kapı:** `TakeDamage` oyuncu→oyuncu hasarını `IsPvPAllowed && CanHarmPlayer` olmadan işlemiyor (`:880-899`); doğum koruması önde; mob→oyuncu, oyuncu→mob, kendine hasar etkilenmiyor.
- **Cooldown/rate limit:** istemci `LastMeleeTime` kapısı (0,6 sn) + sunucu `0,9×` toleranslı kapısı (`ServerAttackTarget`, `:1170-1186`) — hileli istemci hız aşamaz; reddedilen vuruşlar cooldown tüketir. Geri bildirim spamı: hub mesajı sabit anahtar `4101` (yenilenir, yığılmaz), yerel ret `4102`, uzak ret `ClientShowHint` (Unreliable) + 3 sn aynı-metin sınırı + `MMONotify` 0,5 sn dedupe; kalıcı sohbet satırı yok.
- `PlayerTick` hedef bırakma: hedef ölü/stat yok, saldıran ölü veya doğum korumalı → `TargetEnemy.Reset()` + `StopMovement`.

**Davet korumaları (`MMOTradeComponent`/`MMOPartyComponent`)**
- Ticaret: aynı davetçi yenilemesi reddi + başka davetçinin geçerli davetini ezme reddi **ve** her ikisi de davetçiye bildirimli (`:250-266`). Süre dolumu (`> 30 sn`, kabulde), kendine davet (`FindNearestTradeTarget` `C==Self` ve ölüleri dışlar), ölü, uzaklık (`CancelDistance`), zaten-ticarette (iki yönde) kontrolleri var; `Begin` her iki tarafın bekleyen davetini temizler.
- İki yönlü yarış: A→B ve B→A birlikte atılırsa iki bekleyen davet oluşur; biri kabul edince `StartTradeServer`/`FormOrJoin` diğerinin bekleyen davetini temizler/etkisizleştirir (ticarette `Begin`; partide kabul `PendingInviteFrom=nullptr` + aynı kümeye yeniden `ApplyPartySet` zararsız). Kilitlenme/çift ticaret yok (kod okuması).
- `ServerAccept` parametresizdir: istemci kimlik/davetçi seçemez; kimlik sunucudaki `PendingInviteFrom`'dan gelir (sahte davetçi yok). Zayıf işaretçi: çıkan/yok olan davetçi `IsValid()` false → kilit oluşturmaz.
- Klan davetinde aynı iki koruma zaten vardı (`MMOClanComponent.cpp:348-364`); üç sistem artık tutarlı.
- Zaman damgası karşılaştırmaları tutarlı: kilit `< Expiry`, kabul `> Expiry` (sınırda boşluk yok).

**Görev metinleri (`MMOQuestTypes.cpp`)**
- `git diff 6031677 cb63d6d -- MMOQuestTypes.cpp`: **yalnız açıklama dizeleri** değişmiş (16 satır `-` / 16 satır `+`); görev ID, ad, tür, hedef mob, sayı, XP/altın, seviye, ön koşul, ödül satırları **birebir aynı**.
- ASCII: eklenen tüm dizeler 7-bit; `\` yok; dizelerde çift tırnak yok (yalnız kesme işareti: `Ayazdis'in`, `Karakaya'da`, `Obsidyen Reisi'ni` — C++ `TEXT("...")` için güvenli). Dosyadaki tek ASCII-dışı karakter önceden var olan yorum satırındaki `—` (`:298`). En uzun açıklama 87 karakter (HUD sığma riski ORTA-3).
- Lore tutarlılığı (`docs/Lore.md`): Çakılı vadiler/goblin (`:22`), Ayazdiş + Uzun Kış ölüleri (`:226`), Karakaya Geçidi/kayalıklar/büyücü-bayrak (`:56-120`), Obsidyen Reisi "Karakaya'da düşen reisin kan bağı" (`:760`), Kara camdan kalkanlı akıncılar (`:318`), Solgun Ormanlar oyuğu (`:84`), Kadim Golem (`:92`), "Tüccar Ruhu" pazar repliği (`:863`), "Vadinin Tüccarı" (`:867`) ile uyumlu. Yalnız Q_Trolls yer belirsizliği (DÜŞÜK-4).
- Kael yeni satırı derlendi, dizi sözdizimi doğru (önceki satıra virgül eklenmiş).

**Diğer**
- `ChangeName` (`bNameChange==true` → `MMOSetName`) ve `ActiveSaveSlot` navmesh dalı öncesi atama, `MMONotify::ToOwner` (Hint kind = `ClientShowHint`, sonuç = sohbet) ve parti XP bölenin menzildeki canlı üye sayısıyla hesaplanması kodda doğrulandı (aşağıda eski bulgular).
- Önceki turun `GetClassBaseStats`/XP eğrisi vb. bu turda değişmedi.

## 4. Eski QA bulgularının durumu (kod okumasıyla; PIE doğrulaması yok)

| Eski bulgu (kaynak: `PLAYTEST_CHECKLIST.md` 2026-10-04 tur-2 ve 2026-10-05 bulut oturumu) | Durum |
|---|---|
| Tur-2 ORTA-1: tıkla-saldır oyuncu hedefini kaybediyor | **Kodda kapandı** (`abe6ad6`); yeni yan etki ORTA-1 |
| Tur-2 ORTA-2: hub mesajı her karede yığılıyor | **Kodda kapandı** (sabit anahtar 4101; ret sebebi hub'da yalnız yerel) |
| Tur-2 ORTA-3: kalıcı sohbet spamı | **Kodda kapandı** (`ClientShowHint` Unreliable, 3 sn dedupe) |
| Tur-2 ORTA-4: `TakeDamage` kapısı atlanıyor | **Açık** (DÜŞÜK-2; bugün erişilemez) |
| Tur-2 DÜŞÜK-1..5 (HUD görev balonu, sabit "Yasli Doran", GenQuestDialogue, belge kalıntıları, MMOProbe) | **Açık** (dosyalar değişmedi) |
| Bulut YÜKSEK: `SetName`/`ServerChangeName` korumaları atlıyor | **Kodda kapandı** (`753e930`: `bNameChange` → `MMOSetName`) |
| Bulut ORTA: `ActiveSaveSlot` nav-bekleme | **Kodda kapandı** (`7cd8844`, atama öne alındı; red `ToOwner`) |
| Bulut ORTA: `LooksLikeOsDefaultName` meşru ad | **Büyük ölçüde kapandı** (DÜŞÜK-3 kalıntı) |
| Bulut ORTA: `PreLogin` yeniden bağlanma kilidi | **Açık** (doğrulanmalı) |
| Bulut ORTA: ad 24/20 uyuşmazlığı | **Kapandı** (`ServerSetName` ≤20, `SanitizeInitialName` 20) |
| Bulut ORTA: parti XP böleni | **Kodda kapandı** (`493d4b1`) |
| Bulut ORTA-DÜŞÜK: `MMONotify` kalıcı chat/Reliable | **Kapandı** (`829e202`) |
| Bulut DÜŞÜK: B5 kalıntıları | **Kısmen** (kabul/ön koşul kapandı, `ClaimReward` erken dönüşleri açık) |
| Bulut DÜŞÜK: altın tavanı uyuşmazlığı | **Commit'lenmemiş Codex değişikliğiyle kapanıyor**; commit'te hâlâ açık |
| Bulut DÜŞÜK: "(%P)" etiketi | **Yorum düzeltildi** (`493d4b1`) |
| Bulut DÜŞÜK: eşiksiz kredi, `Pri_Mending` dengesi, `-MMOPassword` log, HUD buff süresi | **Açık/karar** (tasarım ya da PIE; `DECISIONS_NEEDED.md`) |

## 5. Yeni PIE test adımları (yalnız bu turun değişikliklerine ait; mevcut maddeleri tekrarlamaz)

- [ ] **T1 (ORTA-1) PvP kapalı, oyuncuya tıkla:** Host + istemci, hub dışında, `MMOPvP` açılmamış. İstemci hostun karakterine **sol tıkla**, yanında dur, 15 sn izle. Beklenen kusur (kod okumasına göre): karakter hostu kovalamayı sürdürür, ~3 sn'de bir "PvP kapali" balonu çıkar, host canı düşmez, yakındaki mob'a **vurmaz**. Sohbette kalıcı satır olmamalı. Sonra `D`/`W` ile hareket et ya da zemine tıkla: hedef bırakılmalı. Yanında mob varken tekrar et: hâlâ mob'a vurmuyor mu?
- [ ] **T2 PvP açık tıkla-saldır (abe6ad6 ana senaryo):** Host `MMOPvP 1`; hub dışında istemci hosta tıkla-saldır: host canı ~0,6 sn aralıkla düşmeli; host da aynısını istemciye yapsın. `MMOPvP 0` yazınca hasar anında durmalı.
- [ ] **T3 Parti/klan arkadaşı:** İki oyuncu partide (sonra klanda) PvP açıkken birbirine tıkla-saldır: hasar yok; "Parti uyesine vuramazsin"/"Klan uyesine vuramazsin" balonu 3 sn'de bir. Kovalama devam ediyor mu? (ORTA-1 ile ilişkili)
- [ ] **T4 Hub:** Hub (PlayerStart 700u içi) içinde oyuncu ve mob hedefle sol tıkla: hasar yok; ekranın üstünde tek, yenilenen `Guvenli bolgede saldiri yok` satırı (yığılma yok). Hub dışından hub içindeki oyuncuya PvP açıkken vur: hasar yok, "Guvenli bolgede PvP yok" balonu.
- [ ] **T5 (DÜŞÜK-1) Doğum korumalı hedef:** PvP açık; birini öldür, yeniden doğan oyuncuya hemen tıkla-saldır: hasar sıfır olmalı. Beklenen kusur: saldıranda "VURDUN: 0 hasar" (host) ve saldıranın kendi koruması bitiyor, sınıf değiştirme/bölge geçişi "savaşta" diye reddediyor olabilir. (Beklenenin dışındaysa bulgu yanlıştır.)
- [ ] **T6 (ORTA-3) Görev penceresi taşması:** Yeni karakterle `G`: "Goblin Avi" satırının açıklama + `Odul:` satırı panel (480 px) içinde kalıyor mu? Seviye 6+ karakterle `Q_Warlocks`/`Q_DireWolves`, Lv25+ ile `Q_ObsidianWarlord`, `Q_Gold1000` satırlarına bak (ekran görüntüsü al). Taşıyorsa (metin altın çerçevenin dışında) bulgu doğru.
- [ ] **T7 Ticaret daveti kapma:** Üç oyuncu (host + 2 istemci) yan yana. A `/trade` yazsın (B'ye). A 3 sn sonra tekrar `/trade`: "Bu oyuncuya ticaret daveti zaten gonderildi; yanitini bekle." çıkmalı, B'ye **ikinci bildirim gelmemeli** ve davet süresi uzamamalı (30 sn sonra `/trade kabul` → "suresi doldu"). C, B'ye `/trade` desin: "Oyuncunun bekleyen baska bir daveti var" almalı, B hâlâ A'dan kabul edebilmeli. B `/trade iptal` yazınca C artık davet edebilmeli.
- [ ] **T8 (ORTA-2) Parti daveti kapma + sessiz ret:** Aynı üç oyuncu. A `MMOInvite` (B'ye). A tekrar `MMOInvite` (3 sn sonra): B'ye yeni "Parti daveti aldin" **gelmemeli**; A ekranında bir şey çıkmıyor (beklenen eksik: geri bildirim yok). C `MMOInvite` desin: B'ye gitmemeli; C'de de mesaj yok. B kabul etmeden 60 sn bekle: sonra C'nin daveti geçmeli. B'nin daveti **reddetme komutu olmadığını** doğrula (konsolda `MMOParty...` listesi). Uzak istemci davetçi iken başarılı davette de "Parti daveti gonderildi" görünmüyor olmalı.
- [ ] **T9 Davet kabul kenar durumları:** (a) A, B'yi davet etsin; A bölgeden uzaklaşsın (>3000u) sonra B `MMOPartyAccept`: "Davet eden cok uzakta". (b) B 60 sn+ bekleyip kabul etsin: "Davetin suresi doldu". (c) A, B'yi davet edip oyundan çıksın; B `MMOPartyAccept`: çökmemeli, parti kurulmamalı. (d) A↔B aynı anda birbirine `MMOInvite`; ikisi de kabul etsin: tek parti, tek lider, çökme yok.
- [ ] **T10 Görev metinleri:** Görev penceresinde 16 değişen açıklamayı gez; Türkçe karakter olmadığından `?`/kare/bozuk glif çıkmamalı; Kael'e (kapı muhafızı) 10-15 kez tıkla (LLM kapalıysa yedek replikler): "Goblin tek basina korkutmaz..." satırı havuzda çıkmalı, yinelenen/boş satır olmamalı. Görev ödülleri/hedef sayıları eskisiyle aynı (örn. Goblin Avi: 5 Goblin, 200 XP, 120 altın, 3 Can İksiri).
- [ ] **T11 Regresyon (mob dövüşü):** Hub dışında mobu tıkla-saldır ile öldür; hasar sayıları, XP, görev ilerlemesi, ganimet eskisi gibi olmalı; mob ölünce otomatik hedef bırakılmalı; ölü moba tıkla: yürümeli (saldırmamalı).

## 6. Özet

- **Kritik:** yok (çökme/hasar açığı/sır sızıntısı bulunmadı). Tek-dosya derlemeler 6/6 başarılı, uyarı 0.
- **Orta:** ORTA-1 (PvP kapalıyken oyuncuya tıklayınca kalıcı kovalama + RPC döngüsü), ORTA-2 (parti daveti: ret komutu yok, ret sessiz, 60 sn kilit/griefing), ORTA-3 (görev satırı taşması — doğrulanmalı).
- **Düşük:** DÜŞÜK-1..5 yukarıda.
- `abe6ad6`'nin ana amacı (oyuncu hedefini sunucuya aktarmak) kodda doğru çalışıyor ve sunucu kapıları sağlam; yan etki yalnız istemcinin PvP durumunu bilmemesinden doğuyor. Davet korumaları ticarette eksiksiz, partide işlevsel ama kullanıcı geri bildirimi/ret yolu eksik.
