# QA turu 2026-10-08 — Dedicated hazırlığı + kayıt/JSON/tüketilebilir düzeltmeleri (tek geliştirici ajan turu)

Denetçi: qa-kontrolcu (salt okunur; kod değiştirilmedi, commit/push atılmadı). PIE'de ve iki-süreçli gerçek oyunda OYNANMADI: "PIE gerekli" / "doğrulanmalı" işaretli her şey doğrulanmamıştır. Denetlenen commit'ler: `0935f8f`, `f1f4d14`, `a8bea14`, `1905253`, `5c1b535`, `9665a09`, `f3228a5`, `8ed06fe`, `9c3addb` (`HEAD` = `9c3addb`; çalışma ağacı temiz, yalnız izlenmeyen `.agents/`, `.codex/`, `AGENTS.md`).

## 0. Kanıt (bu turda gerçekten çalıştırılan)

Editör kapalıydı (süreç listesi kontrol edildi). `Binaries/Win64/UnrealEditor-MyProject.dll` 22:49:19, son kod commit'i (`8ed06fe`, 22:47) sonrası.

- `Scripts/RunAllChecks.ps1 -SkipBuild -FullCompile`: **12/12 adım GEÇTİ** (veri denetimi, StaticLint, PlaytestMetrics, negatif testler, PCH'siz tek-dosya /W4, JSON testi, mağaza testi, bölge geçişi, bölge geçişi zaman aşımı, Summer/Winter duman, NetSmokeTest).
  - PCH'siz tek-dosya /W4 (`-NoPCH -NoLink -SingleFile`): `MMOCharacter`, `MMOGameMode`, `MMOInventoryComponent`, `MMOStatsComponent`, `MMOPlayerController`, `MMOHUD`, `MMOMob`, `MMOCharacterSaveJson`, `MMOCharacterStoreSubsystem`, `MMOLocalFileCharacterStore`: **10/10 Succeeded, 0 `warning C`**.
  - JSON: `PASS schema=2 bytes=1219 negative=109`. Mağaza: `slots=166 loaded=166 invalid=0 loadFailed=0`, `json ok=166 mismatch=0 maxLevel=6`, `writeorder ok=1`, gidiş-dönüş `equal=1`.
  - Bölge geçişi + timeout headless testleri GEÇTİ (tek süreç `-game` = `NM_Standalone`; uzak istemci yok).
  - NetSmokeTest: 1 istemci katıldı, server/istemci logunda `Error:` ve `Warning` sayısı 0. **Ama log'da `GameState`/`MMOGameState` geçen tek satır yok: PvP aynasının replike olduğuna dair KANIT YOK** (test yalnız katılımı sayar).
- Bu turda **ayrıca** (RunAllChecks'in listesinde olmayan) tek-dosya /W4 derlemeleri: `Core/MMOGameState.cpp`, `Core/MMOAdmin.cpp`, `AI/MMODialogueSubsystem.cpp`, `Save/MMOCharacterStoreSubsystem.cpp`, `Core/MMOTargeting.cpp`: **5/5 Succeeded, 0 uyarı, 0 hata**.
- `StaticLint.py` GEÇTİ ama **1 UYARI**: `MMOGameMode.h: AMMOGameMode::InitGameState override beyaz listede yok` (bkz. DÜŞÜK-1). UE 5.7 `GameModeBase.h:69` imzası `ENGINE_API virtual void InitGameState();` — `override` imzası uyuyor.
- İçerik taraması (uasset/umap ham bayt): `Summer_scene.umap`/`Winter_scene.umap` içinde `GameModeOverride/DefaultGameMode/BP_ThirdPersonGameMode/MMOGameMode` yok → dünya ayarı GameMode'u ezmiyor, `GlobalDefaultGameMode` (BP_ThirdPersonGameMode) geçerli. `BP_ThirdPersonGameMode.uasset` içinde `MMOGameMode` var (ebeveyn), `GameStateClass` ve `bAllowPvP` yok. Bu bir isim-tablosu taramasıdır; çalışma zamanında (`GetAll`/log) **doğrulanmalı**.
- API anahtarı sızıntısı: `git grep` ile `nvapi-…`/`sk-ant-…` kalıbı kodda/Config'te yok; `.agents/`, `.codex/`, `AGENTS.md` içinde de yok. Anahtar yalnız ortam değişkeninden okunuyor (`MMODialogueSubsystem.cpp` `GetApiKeyFor`). `Saved/` git-ignore'da.

## 1. Özet (ciddiyet sırası)

**Kritik (çöker / yanlış çalışır / sır sızar) bulgu YOK.** Sunucu karar noktaları korunmuş; uzak istemciye yönetici yetkisi sızdıran bir yol bulunmadı.

| # | Ciddiyet | Başlık |
|---|---|---|
| ORTA-1 | ORTA | LLM günlük tavanı: tek oyuncu tüm tavanı bitirebilir (oyuncu başı kota yok, tavan süreçler arası kalıcı); belge "bağlantı hatası sayılmaz" diyor, kod sayıyor; çoklu süreçte sayaç ayrışır |
| ORTA-2 | ORTA (örtük; bulut mağazada tetiklenir) | Yazım sırası takipçisi: 60 sn otomatik kayıt/klan/ticaret (senkron `SaveCharacterForTravel`) uçuştaki geçiş yazımını "eski" yapar → bulut mağazada geçiş rastgele iptal; takipçi veri sırasını GARANTİ etmez (belgeli); iş parçacığı sözleşmesi kodda zorlanmıyor |
| ORTA-3 | ORTA (örtük; `FromJson` üretimde çağrılmıyor) | JSON: (a) 2 MiB'a kadar derin iç içe `[` ile ayrıştırıcı yığın taşması riski, derinlik tavanı ayrıştırmadan SONRA [doğrulanmalı]; (b) `ToJson` aralık denetimi yapmıyor, `FromJson` yapıyor → kendi yazdığın blob'u okuyamama; (c) sayısal enum yolunda tanımsız-davranış dönüşümü; (d) "yeni alan = sürüm artır" kuralı kodda yok |
| DÜŞÜK-1 | DÜŞÜK | `InitGameState` StaticLint beyaz listesinde yok; `-FullCompile` dosya listesi yeni dosyaları (GameState/Admin/Dialogue/Targeting) kapsamıyor |
| DÜŞÜK-2 | DÜŞÜK | Otomatik regresyon boşluğu: admin kapısı (uzak istemci reddi) ve PvP aynası için headless/iki-süreçli test yok; NetSmokeTest yalnız katılımı sayar |
| DÜŞÜK-3 | DÜŞÜK (UX) | İstemci PvP ön kontrolü: GameState gelmeden (ilk kareler/bölge geçişi) `false` döner = "PvP kapalı gibi" (açık gibi DEĞİL); yeni açılan PvP ~100 ms+ gecikmeli görünür; `bAllowPvP` Blueprint'ten yazılırsa ayna bayatlar |
| DÜŞÜK-4 | DÜŞÜK (UX) | Tüketilebilir ipucu yerelde `AddOnScreenDebugMessage(-1,…)` ile YIĞILIR; sunucu kapısı 0.85 sn → değiştirilmiş istemci ~%15 hızlı iksir (tasarım kararı) |
| DÜŞÜK-5 | DÜŞÜK | `FloatingTexts` 64 sınırı en eskiyi türüne bakmadan atar (XP/ipucu etiketi hasar sayısı yüzünden erken düşebilir) |
| DÜŞÜK-6 | DÜŞÜK | `RequestZoneTravel` `false` döndüğünde `AbortZoneTravel` ekran mesajı + `MMOTravel` mesajı aynı nedeni çift basar |
| DÜŞÜK-7 | DÜŞÜK | Mağaza testi hermetik değil (yuva sayısı 134 → 154 → 166 büyüyor), JSON uyumu yalnız Lv ≤ 6 gerçek kayıtla kanıtlı |
| DÜŞÜK-8 | DÜŞÜK | Belge: `CHECKLIST_FIXES_2026-10-05.md` §24/§27 adımlarında uygulanabilirlik/doğruluk hataları (bkz. §6) |

## 2. Bulgular

### ORTA-1 — LLM günlük tavanı (`a8bea14`)

Yer: `Source/MyProject/Private/AI/MMODialogueSubsystem.cpp:50-80` (`RollDailyBudget`/`SaveDailyBudget`), `:141-152` (tavan + artış), `:293` (iade); `Public/AI/MMODialogueSubsystem.h` (`MaxDailyRequests=1500`); çağıran `MMOPlayerController.cpp:1257` (`ServerRequestNpcDialogue`).

Doğru olanlar (kodla doğrulandı): gün numarası `UtcNow().GetTicks()/TicksPerDay` (UTC; yerel gece yarısı DEĞİL — Türkiye'de UTC 00:00 = 03:00); gün değişince sıfırlama `SendRequest` başında her istekte kontrol ediliyor (süreç gece yarısını aşsa da çalışır); dosya bozuk/eksik/boş ise `Split` başarısız veya `Atoi64`=0 → gün farkı → bugün için 0'dan başlar (çökmez); gelecekteki/eski gün numarası da sıfırlanır; dosya yalnız ilk kullanımda okunur; tavan kontrolü anahtar/dakikalık/eşzamanlı/oturum kontrollerinden SONRA ve artıştan ÖNCE; `ProcessRequest()` başlamazsa üç sayaç da iade ediliyor; `MaxDailyRequests=0` sınırsız.

Sorunlar:
1. **Sayaç "gerçek HTTP denemesi" sayar, "başarılı çağrı" değil.** Artış `ProcessRequest`'ten ÖNCE; iade yalnız `ProcessRequest()` `false` dönerse. Bağlantı hatası/zaman aşımı/HTTP 401/429/ayrıştırma hatası SAYILIR. `CHECKLIST_FIXES` §24 "bağlantı hatası … sayılmaz" diyor → yanlış (aşağıda §6). Tasarım olarak savunulabilir (sağlayıcı tarafında kota harcanmış olabilir) ama belge ile kod çelişiyor.
2. **Oyuncu başına kota yok, tavan süreçler arası KALICI.** Mevcut sınırlar küreseldir: oyuncu başı 2 sn (`NpcTalkServerCooldown`), küresel 30/dk, 4 eşzamanlı, oturum 400, gün 1500. Tek bir kötü niyetli/hileli istemci NPC yakınında `ServerRequestNpcDialogue` yağdırırsa ~13 dk'da oturum tavanını (400), ~4 yeniden başlatmada günlük tavanı bitirir; **diğer oyuncuların NPC'leri gün sonuna dek yedek replikle konuşur** (oyun bozulmaz, yalnız özellik ölür; maliyet koruması çalışır). Önerilen yön: oyuncu başı günlük/oturum kotası (ör. `PlayerState`/yuva adı başına sayaç) ya da tavanın bir kısmını oyuncu başına ayır.
3. **Çoklu süreç/çoklu örnek:** dosya yalnız ilk istekte yüklenir (`bDailyBudgetLoaded`); iki süreç (ör. test host + dev PIE host aynı `Saved/`) bellekte ayrışır, son yazan kazanır → toplam tavanı N kat aşabilir/eksik sayar. Tek host için sorun değil; belgede yalnız "yeniden başlatmada korunur" yazıyor, çoklu süreç sınırı yazılmalı.
4. **Yazım atomik değil**, her istekte ve her iadede diske yazılır (≤ 30/dk, küçük dosya; oyun iş parçacığında senkron). Yazım yarıda kesilirse dosya boş/kısmi kalır → sonraki açılışta bugün için 0'dan başlar = tavan sessizce sıfırlanır (düşük olasılık). Öneri: geçici dosyaya yaz + yeniden adlandır, ya da her N istekte yaz.
5. `MaxDailyRequests` `UPROPERTY(EditAnywhere)` ama sınıf `config` değil ve `UGameInstanceSubsystem` editörde düzenlenebilir bir varlık değil → değer yalnız kodla/derlemeyle değişir. Maliyet kararı "sizin" deniyor; ini'den ayarlanabilir olmalı (`UCLASS(config=Game)` + `config` belirticisi) ya da `-MMOLLMDailyMax=` komut satırı.
6. "1500" anlamı: sunucu başına, UTC günü başına, **HTTP deneme sayısı** (konuşma sayısı değil), her iki sağlayıcı (NVIDIA/Anthropic) için ortak. NVIDIA ücretsiz katman/Anthropic faturası için uygun bir değer olup olmadığı koddan çıkarılamaz (maliyet kararı).

### ORTA-2 — Yazım sırası takipçisi (`f3228a5`)

**2026-10-07 güncellemesi:** `e77ff4e` uçuşta geçiş kaydını bozan ek kayıtları erteledi
ve takipçide oyun iş parçacığı sözleşmesini `check` ile zorladı. Sonraki düzeltmede
`FinishZoneTravel` canlı/savaş dışı durumunu ve güncel oyuncu/pawn kümesini yeniden
doğrular; beklerken savaşa giriş headless regresyonu iptal ve kilit temizliğini geçti.
Ertelenen kayıt yazım hatası da sonraki düzeltmede geçişi iptal eder ve bekleyen işareti
korur; iki başarısız deneme sonrası başarılı kayıt regresyonu geçti. Anlık görüntü sonrası
kazanımlar ve depodaki gerçek yazım sırası/iptal açık. Aşağıdaki metin ilk denetimin tarihsel kaydıdır.

Yer: `Public/Save/MMOCharacterStore.h:8-35` (`FMMOSlotWriteTracker`), `Private/Characters/MMOCharacter.cpp:493` (senkron `BeginWrite`), `:567-615` (`SaveCharacterForTravelAsync`), `Private/Core/MMOGameMode.cpp:381-520`.

Doğru olanlar: sürüm numarası artan, yuva adı büyük/küçük harf duyarsız (`ToLower`); bayat callback `bSuccess=false` ve `ActiveSaveSlot` GÜNCELLENMİYOR (doğru); `WeakThis` ile karakter ömrü korunuyor; `TSharedRef` izleyici mağaza ölse de callback'te geçerli; `HandleZoneTravelSaveComplete` seri numarası + karakter başına tamamlanma kümesi ile eski/çift callback'i eliyor; `RequestZoneTravel` döngüsü iptalde kırılıyor ve `false` + neden dönüyor; test kancaları sıfırlanıyor; `PendingZoneTravelSaves` her seferinde `Pending.Num() - Completed.Num()` ile yeniden hesaplanıyor (sapma yok). Çağıranlar yeni sözleşmeyi doğru ele alıyor: `MMOTravel` (`MMOPlayerController.cpp:54`) `false` + neden basıyor; HUD düğmesi `MMOTravel`'i çağırıyor; headless test `REJECTED` loglıyor.

Sorunlar:
1. **Otomatik kayıt (60 sn) / klan / ticaret / `MMOSave` yolları senkron `SaveCharacterForTravel()`'dır ve her çağrıda `BeginWrite` alır.** Bulut mağazada (gecikmeli `SaveAsync`) bu çağrılardan biri geçiş kaydı uçuştayken gelirse geçiş yazımı `IsNewest=false` olur → `bSuccess=false` → **geçiş iptal** ("Bir karakter kaydedilemedi"). Güvenli hata ama rastgele. Yerel (senkron) mağazada callback aynı karede döndüğü için TETİKLENMEZ (otomatik testler bunu kanıtlayamaz). Öneri: geçiş bekleme penceresinde (`bZoneTravelInProgress`) otomatik kaydı/klan-ticaret kaydını ertele ya da geçiş yazımını "sahip" bayrağıyla ayrı tut.
2. Takipçi **yalnız callback tarafı** korumasıdır; depoya hangi sırada inildiğini belirlemez (başlıkta yazıyor). Bayat yazım sonradan inip yeni veriyi ezebilir: bulut mağazada yuva başına seri kuyruk/Revision şart (belgede var, kodda yok; NETWORK_AUDIT O6).
3. **İş parçacığı güvenliği:** `FMMOSlotWriteTracker` kilitsiz (`TMap` + `uint64`), `TSharedRef` varsayılanı `ThreadSafe` olduğundan referans sayımı güvenli ama içerik değil. `IMMOCharacterStore::SaveAsync` sözleşmesi "callback oyun iş parçacığında çağrılmalı" diyor (`MMOCharacterStore.h:79-82`) → bugün güvenli; sözleşmeyi bozan bir bulut uygulaması sessizce yarışır. Öneri: `BeginWrite/IsNewest` içine `check(IsInGameThread())` veya `FCriticalSection`.
4. Zaman aşımı etkileşimi: zaman aşımı sonrası uçuştaki yazım iptal edilemez (belgeli); kullanıcı hemen yeniden denerse yeni `BeginWrite` eskiyi bayatlaştırır (doğru), ama eski yazım depoya yine de inebilir (madde 2).
5. Açık kalan (bu turda ele alınmadı, ORTA-1 `QA_ROUND_2026-10-07_CODEX` maddeleri 3-4): anlık görüntü ile `Finish` arasındaki kazanımların geri sarılması ve `Finish`'te canlı/savaş dışı yeniden doğrulama yok.

### ORTA-3 — JSON ice aktarma v2 (`9665a09`, `5c1b535`)

**2026-10-07 düzeltme notu:** Aşağıdaki 1–3 bulgularının kod düzeltmeleri ve regresyon
testleri tamamlandı: ayrıştırma öncesi 16 düzey ham sınır (bilinmeyen ek alan dahil),
`ToJson` için ortak aralık/yapısal doğrulama ve int64 dönüşümünden önce sayısal enum
sınırı. UE derlemesi ve 12/12 otomatik kontrol geçti; fixture iyileştirmeleri sonrası
yeniden derleme, JSON `negative=111` ve 178 gerçek kayıt için `mismatch=0` geçti.
4 (şema göçü) ve 5 (Lv30+ gerçek kayıt kanıtı) açık. Ayrıntı:
`CHECKLIST_FIXES_2026-10-05.md` §29. Aşağıdaki tespit metni ilk denetimin kaydıdır.

**Üretimde kullanılıyor mu? HAYIR.** `FMMOCharacterSaveJson::` çağıranları yalnız: `MMOCharacter.cpp:787` (`MMOTestSaveJson` — `MMODebugCommandsEnabled` kapılı test komutu) ve `MMOCharacterStoreSubsystem.cpp:73-135` (store self-test). Gerçek kayıt/yükleme `FMMOLocalFileCharacterStore` → `.sav` (GVAS). Yani bu kod bugün oynanışa dokunmaz; bulut mağaza gelince devreye girecek. `FromJson` reddi için "mevcut kaydı korur" sözleşmesi (başarısızlıkta `OutData` dokunulmaz, çağıran false'ta yazmaz) kodla/test ile doğrulandı: `FromJson` adayı ayrı `Candidate`'ta kurar, yalnız tüm doğrulamalardan sonra `OutData = MoveTemp(Candidate)` yapar (`MMOCharacterSaveJson.cpp` ~423); negatif testler sentinel (`Level=777`) ile `OutData`'nın ezilmediğini kontrol ediyor. Henüz hiçbir çağıran bu sözleşmeye bağlanmadığı için "çağıranlarda gerçekten korunuyor mu" sorusunun cevabı: **çağıran yok → belgelenmeli** (`MMOCharacterSaveJson.h` başlığı zaten uyarıyor; ACCOUNT_BACKEND_COMPARISON'a da yazılsın).

Doğru olanlar: v1 (sayı) hâlâ okunuyor (yalnız |x| ≤ 2^53 tam sayı), v2 int64 ondalıksız METİN (`ParseStrictInt64`: `-0`, `+5`, `05`, boşluk, taşma, boş reddediliyor); 2^53+1 ve `MAX_int64` gidiş-dönüş testli; her `SaveGame` alanı zorunlu; null/yanlış tip/ondalık/aralık dışı/bool-sayı karışımı reddi; `MAX` gizli enum reddi; blob 2 MiB, dizi/harita 4096, string 1024, ad 256 tavanları; `ToJson` NaN/Inf konumu reddediyor; `FVector`/`FRotator` alanları `SaveGame` bayraklı (motor `NoExportTypes.h:543-560`), yani yapısal doğrulama bunları da kapsıyor; 109 negatif senaryo + v1/eski enum/ek alan tolerans testleri GEÇTİ; gerçek 166 `.sav` → `ToJson` → `FromJson` → alan alan eşit.

Sorunlar:
1. **Derin iç içe JSON [doğrulanmalı].** `FromJson` ayrıştırmayı (`FJsonSerializer::Deserialize`) yapısal doğrulamadan ÖNCE çağırır; motor JSON okuyucusunun iç içe derinlik sınırı yok (`Json/` kaynağında `MaxDepth/Nesting` araması boş) ve ayrıştırma özyinelemelidir. `MaxJsonChars` 2 MiB → `"[[[[…"` 1-2 milyon düzey = yığın taşması/çökme olasılığı. `MaxDepth=6` yalnız `ValidateValue` içindedir (çok geç). Negatif testlerde yalnız `{`, `[]`, `null` ham girdileri var, derin iç içe yok. Güvenilmeyen kaynaktan (bulut/istemci) gelmediği sürece risk düşük; bulut mağaza öncesi ön tarama (ham karakter sayacıyla `[`/`{` derinliği tavanı) ya da ayrı `Test` ekle.
2. **`ToJson` ↔ `FromJson` asimetrisi.** `CheckRanges` (Level 1-999, `Plus` ≤ 99, `MasteryRanks` ≤ 1000, Count ≤ 10 000 000, `NextInstanceId` ≥ 1, konum ≤ 1e8…) yalnız `FromJson`'da. `ToJson` yalnız sonlu-konum ve boyut kontrolü yapar. Oyunun yazdığı ama `FromJson`'un reddedeceği bir değer (ör. gelecekte Level tavanı 1000, yeni `Plus` aralığı, `NextInstanceId=0` ile gelen eski bir `.sav`) bulutta saklanır ve **geri okunamaz = oyuncu karakterine erişemez**. Öneri: `ToJson` da aynı `CheckRanges`'i çalıştırsın (reddederse yazma) ve gerçek-kayıt testi bunu zaten kapsıyor olsun; ayrıca sabitleri tek yerde (tek `constexpr`) paylaş.
3. **Tanımsız davranış:** `ValidateEnum` `:84` — `IsIntegralFinite(Number)` yalnız sonluluk/tamsayılık bakar, `1e300` gibi int64'e sığmayan bir değer `static_cast<int64>(Number)`'a giderken tanımsızdır (MSVC'de genelde `INT64_MIN`; reddedilir ama C++ standardı açısından UB). Öneri: önce `Abs(Number) <= 2^53` kontrolü.
4. **Sürüm göçü kodda yok.** `ValidateStruct` yorumu "yeni alan eklenirse şema sürümü artar ve bu işlev o alanı eski sürümler için isteğe bağlı sayar" diyor, kodda sürüme bağlı isteğe-bağlı-alan mekanizması YOK: `FMMOCharacterSaveData`'ya `SaveGame` alanı eklenir de sürüm/geçiş yazılmazsa tüm eski bloblar "zorunlu alan eksik" ile reddedilir. Mevcut korunma: self-test yeni alanı hemen kırar (iyi), ama kural belgede değil CI'de zorlanmalı.
5. Kapsam notu: gerçek-kayıt kanıtı yalnız `maxLevel=6` (166 kayıt). Depo/klan/görev dolu Lv30+ gerçek kayıt `.sav`'ı yok; fixture (Lv39, depo, klan, Türkçe karakter, 2^53+1) bunu kısmen kapatıyor.

### DÜŞÜK-1 — Statik kontrol / FullCompile kapsamı

- `StaticLint.py` `InitGameState` override'ını beyaz listeye almamış (UYARI, hata değil). `Scripts/StaticLint.py` `OVERRIDE_WHITELIST` içine imza (`[]`, `False`) eklenmeli; imza UE 5.7'de doğrulandı.
- `RunAllChecks.ps1 -FullCompile` listesi sabit: yeni/çok değişen dosyalar (`Core/MMOGameState.cpp`, `Core/MMOAdmin.cpp`, `AI/MMODialogueSubsystem.cpp`, `Core/MMOTargeting.cpp`) yok. Ben bu 4+1 dosyayı elle derledim (hepsi temiz), ama bir sonraki turda eksik include bu listeye girmeyen dosyada saklanabilir. Öneri: listeyi `git diff --name-only <taban>..HEAD -- Source` ile üret ya da `Source/**/*.cpp` tamamını bir kez PCH'siz derle.

### DÜŞÜK-2 — Otomatik regresyon boşluğu

`Scripts/NetSmokeTest.ps1` yalnız "katıldı + hata yok" sayar. Güvenlik özelliği olan "uzak istemci `MMOPvP 1`/`MMOTravel`/`MMOGiveItem` çağırınca reddedilir" ve "GameState'te `bPvPEnabled` istemciye ulaşır" için OTOMATİK test yoktur (kod tarama + belge). Headless zone-travel testleri `NM_Standalone`'da çalışır → `IsServerAdmin` için yalnız "pozitif" yol (yerel denetleyici) sınanır; negatif yol (uzak denetleyici, dedicated) sınanmaz. Öneri (kodu mmo-developer yazar): `[MMO_ADMIN_TEST]`/`[MMO_PVP_MIRROR_TEST]` log satırı üreten bir `Exec` + iki-süreçli senaryo (istemci `MMOPvP 1` çağırır; sunucu `bAllowPvP` ve GameState'i logla; istemci `GameState.bPvPEnabled` değerini logla).

### DÜŞÜK-3 — PvP aynası (`0935f8f`) davranış notları

Yer: `Private/Characters/MMOCharacter.cpp:1372` (`IsPvPAllowed`), `:1184-1205` (istemci ön kontrolü), `Private/Player/MMOPlayerController.cpp:910-935`, `Private/Core/MMOGameMode.cpp:28-56`, `Private/Core/MMOGameState.cpp`.

Odak sorusunun cevabı: **GameState henüz replike olmadan istemcide `IsPvPAllowed()` `false` döner** (`GetGameState<AMMOGameState>()` null → son satır `return false`), yani "PvP açıkmış gibi" DEĞİL, "kapalıymış gibi" davranır: oyuncu hedefine tıklayınca kovalama başlamaz, `AttackTarget` "PvP kapali" ipucu verip RPC göndermez. Sunucu açıkken ilk kare/bölge geçişi sonrası bir iki kare boyunca oyuncuya saldırı/kovalama gecikmeli başlar (UX). Hile/güvenlik etkisi yok: sunucu kararı `AttackTarget` otorite dalında (`IsPvPAllowed()` GameMode'dan + `CanHarmPlayer` + spawn koruması) değişmedi.

Doğrulananlar: karar tek kaynak `AMMOGameMode::bAllowPvP` (sunucu `GetAuthGameMode` önce okunur); yazma yolu `SetPvPAllowed` her ikisini yazar; `MMOPvP` ve `ServerSetPvP` ikisi de `SetPvPAllowed` kullanıyor (kodda `bAllowPvP`'ye başka doğrudan yazma yok: `grep` yalnız `SetPvPAllowed` içinde ve okuma yerlerinde); `InitGameState` (`AGameModeBase::PreInitializeComponents` içinde `GameState` oluştuktan sonra çağrılır) başlangıç değerini aynalar; `AMMOGameMode : AGameModeBase` olduğundan `AGameStateBase` türevi sorun değil (`AGameMode` olsaydı `AGameState` gerekirdi); `GetLifetimeReplicatedProps`'ta `DOREPLIFETIME(bPvPEnabled)` var; `SetPvPEnabled` `HasAuthority()` korumalı. Seamless travel: GameState taşınmaz, yeni harita GameMode'u (aynı BP) yeni `AMMOGameState` yaratır; PvP her geçişte varsayılana (false) döner (belgede var, eskisiyle aynı); dünya ayarı GameMode/GameState'i ezmiyor (yukarıdaki ham tarama). Bu iki nokta çalışma zamanında **doğrulanmalı** (PIE).

Notlar:
1. `bAllowPvP` hâlâ `BlueprintReadWrite`: bir Blueprint grafiği doğrudan yazarsa ayna bayatlar (istemci yanlış ön kontrol yapar; sunucu kararı doğru kalır). Öneri: `BlueprintReadOnly` yap (BP'ler `SetPvPAllowed`'i `BlueprintCallable` ile çağırabilir) ya da bir `OnRep`/tick eşitlemesi.
2. İstemci `CanHarmPlayer` ön kontrolü replike veriye bakar: klan adı/parti üyeliği başka oyuncular için istemcide eksik olabilir (yanlış-izin = sunucu reddeder, zararsız); hub güvenli bölge hesabı istemcide hedefin yaklaşık konumuyla yapılır → sınır çizgisinde istemci erken/geç reddedebilir (yalnız UX).
3. `AttackTarget` önce `LastMeleeTime = Now` yazar, sonra istemci ön kontrolü reddederse vuruş cooldown'u harcanmış olur (zararsız, spam'i azaltır).

### DÜŞÜK-4 — Tüketilebilir geri bildirimi ve cooldown (`8ed06fe`)

Yer: `MMOCharacter.cpp:1849-1857` (`TryBeginConsumableUse`), `Private/Core/MMONotify.cpp:56-92`.

Doğru: sunucu kapısı yalnız `HasAuthority() && !IsLocallyControlled()` (uzak istemci pawn'ı) için 0.85, host/yerel/istemci 1.0; `ToOwner` uzak yolda 0.5 sn aynı-metin dedupe'u (pawn başına tek metin; sürekli aynı ret metni → ≤ 2/sn `ClientShowHint`, Unreliable) → ret spam'i ağı doldurmaz; iksir zinciri: can→mana ≤ 1 sn içinde hâlâ ortak kapıdan geçer (tasarım) ve artık "Esya kullanimi bekleniyor" ipucu çıkar.

Notlar:
1. **Yerel yolda yığılma:** `MMONotify::ToOwner` yerel dalı `AddOnScreenDebugMessage(-1, Duration, …)` (`MMONotify.cpp:64`) anahtar `-1` kullanır → her reddedilen basış YENİ satır ekler (1 sn sürer); F/G tuşuna hızlı basan oyuncu satır yığını görür. Öneri: sabit anahtar (kategori başına) ya da yerelde de dedupe.
2. Sunucu kapısı 0.85 → doğrudan RPC gönderen değiştirilmiş istemci ~%15 hızlı iksir içebilir (iksir sayısı yine envanterle sınırlı; denge/tasarım kararı).
3. "Canin zaten dolu"/"Iksir yok!" ön kontrolleri kapıdan ÖNCE çalışır: iksiri ilk içtikten sonra can doluysa ikinci basışta cooldown ipucu değil "Canin zaten dolu" görünür (test adımı için önemli, bkz. §6).

### DÜŞÜK-5 — `FloatingTexts` sınırı (`MMOHUD.cpp:446-532`)

Doğru: 64 aşılınca `RemoveAt(0, fazla)` en eskiyi atar (sıra eklenme sırası, doğru); `Canvas` yokken de sınır işler (`AddDamageNumber` içinde); `EAllowShrinking::No` ile yeniden tahsis yok. Not: hasar sayıları (1.1 sn) ve etiketler (XP/ipucu, 2 sn) aynı kuyrukta; yoğun grup savaşında > 58 sayı/sn üretimi etiketi (öncelikli) erken atabilir. Öneri: etiketler için ayrı kuyruk ya da atarken önce sayıları ele.

### DÜŞÜK-6 — Çift ret mesajı

`AbortZoneTravel` zaten `GEngine->AddOnScreenDebugMessage` basar; `RequestZoneTravel` aynı karede `false` + `LastZoneTravelAbortReason` döndürdüğünde `MMOTravel` (`MMOPlayerController.cpp:54-63`) aynı metni bir kez daha basar (iki satır). Kozmetik.

### DÜŞÜK-7 — Mağaza testi hermetik değil

`TestCharacterStore` her koşuda diskteki tüm `MMOSave_*.sav` yuvalarını okur; duman/ağ testleri her koşuda yeni `CIDIK-<hex>` yuvaları üretiyor (134 → 154 → 166). `json ok=166 maxLevel=6`: bu makinedeki gerçek kayıtların hepsi test kökenli Lv ≤ 6. "Eski kayıtlarla uyumlu" iddiası bu veriyle sınırlı. Hub önbelleği, hub PvP vb. ile ilgisi yok. Öneri: sürümlü, repoya konmuş eski-biçim `.sav` fixture'ı; test yuvalarını koşu sonunda sil.

## 3. Hub önbelleği ve `FloatingTexts` (odak 7-8) — kısa yanıt

- `MMOTargeting.cpp` `GetHubSafeZoneCenters`: önce ölü dünya kayıtları ayıklanır, sonra bu dünya aranır/eklenir; **boş liste saklanmaz**, boş kaldıkça en fazla 1 sn'de bir `TActorIterator<APlayerStart>` taraması (`FPlatformTime::Seconds`, gerçek zaman); dolu liste dünya ömrü boyunca kalır. Maliyet: her çağrıda `Found` araması (≤ birkaç dünya) + en kötü 1 tarama/sn; çağrı başına tarama yok → kabul edilebilir. Kalan sınır: yalnız KISMEN yüklenmiş bir PlayerStart kümesi (ilk tarama ≥ 1 ama hepsi değil) kalıcı olur — mevcut tek kalıcı seviyeli haritalarda tetiklenmez. Dönüş referansı bir sonraki `GetHubSafeZoneCenters` çağrısında (başka dünya için dizi büyürse) geçersizleşebilir; çağıranlar hemen kullanıyor (aynı desen önceki sürümde de vardı) → sorun yok.

## 4. Odak başlık başlık yanıt

1. **AMMOGameState**: DÜŞÜK-3. GameState gelmeden `IsPvPAllowed` = `false` ("kapalı gibi"); sunucu kararı ve GameMode tek kaynak; `SetPvPAllowed` her iki yazım tek yerde (BP yazımı hariç); GameState sınıfı seamless travelde yeni GameMode'dan geliyor (class değil örnek taşınmaz); `InitGameState` sırası doğru; PIE'de doğrulanmadı.
2. **`MMOAdmin::IsServerAdmin`**: `PC == nullptr` → false; `HasAuthority()` yoksa false (istemci dünyasındaki PC); `NM_DedicatedServer` → false; aksi halde `IsLocalController()`. Sonuç: listen-server host'un yerel PC'si ve `NM_Standalone` (tek oyunculu) = admin; uzak istemcinin sunucudaki PC'si `IsLocalController()=false` → admin DEĞİL; dedicated → hiç kimse. Eski kapılar: `grep` ile `IsLocalController` yalnız `MMOAdmin.cpp:19` ve sohbet kutusu (`MMOPlayerController.cpp:504`, farklı amaç) içinde kalıyor; taşınan 9 yerin hepsi (`MMOTravel`, `RequestZoneTravel`, test host seçimi, HUD çizim + tıklama, `ServerSetPvP/ApplyBuff/SpawnMob/GiveItem/LoadCharacter`) eski kapının aynısı + `HasAuthority`/dedicated koşulu; **atlanan kapı yok**. Headless test host seçimi `-game` = `NM_Standalone` → `IsServerAdmin` true → geçiyor (12/12 testle doğrulandı). Dedicated'da `false` doğru (bilinçli; yönetim yolu yok = bölge geçişi/PvP anahtarı dedicated'da kullanılamaz, NETWORK_AUDIT'te açık kalem). Hâlâ **yalnız `!UE_BUILD_SHIPPING`** kapılı olmayan, `MMODebugCommandsEnabled()` ile korunan `MMODamage/MMOHeal/MMOGainXP/MMOUseMana/MMOClass` Exec'leri istemci yerelinde çalışır; durum değişikliği otorite gerektirir, yeni bulgu değil.
3. **LLM günlük tavanı**: ORTA-1.
4. **JSON v2**: ORTA-3 (üretimde kullanılmıyor; belgele).
5. **Yazım sırası takipçisi**: ORTA-2.
6. **Tüketilebilir 0.85/1.0**: DÜŞÜK-4. İksir zinciri ve meşru hızlı kullanım (≈1 sn aralık, jitter) korunuyor; hint spam ağı doldurmaz.
7. **FloatingTexts 64**: DÜŞÜK-5.
8. **Hub önbelleği**: §3.
9. **Tek-dosya /W4**: §0 (15 dosya: 10 `RunAllChecks` + 5 elle; 0 uyarı/hata).
10. **CHECKLIST §22-27**: §6.

## 5. Eski bulguların (`QA_ROUND_2026-10-07_CODEX.md`) kapanma durumu

| Eski | Durum | Not |
|---|---|---|
| ORTA-1 (asenkron kayıt sözleşmesi) | **Kısmen kapandı** | Callback sırası/çift callback/dönüş sözleşmesi/kanca sızıntısı giderildi. AÇIK: depoda sıra garantisi (Revision/kuyruk), anlık görüntü-`Finish` penceresi, `Finish`'te yeniden doğrulama yok, otomatik kayıt etkileşimi (yeni ORTA-2) |
| ORTA-2 (PCH'siz derleme kırığı) | **Kapandı** | Include eklendi; 10/10 taze tek-dosya derleme temiz |
| ORTA-3 (JSON gevşek) | **Büyük ölçüde kapandı** | Sıkı doğrulama, v2, ham derinlik sınırı, simetrik yazma/okuma, güvenli enum dönüşümü, 111 negatif test; AÇIK: sürüm göçü kodu ve Lv30+ gerçek kayıt kanıtı; `FromJson` üretimde hâlâ çağrılmıyor |
| ORTA-4 (denetim logu Shipping) | **Kapanmadı, belgelendi** | `bUseLoggingInShipping` eklenmedi (UBT unique build gerekir); NETWORK_AUDIT O12'ye sınır yazıldı. Dedicated Server target'ı olmadığı için doğrulanamaz |
| DÜŞÜK-1 (bölge geçişi yardımcıları) | **Kapandı** | `false`+neden dönüşü, karakter başına küme, kanca sıfırlama, bayat yorum düzeltildi |
| DÜŞÜK-2 (tüketilebilir geri bildirim) | **Kısmen** | İpucu + 0.85/1.0 yapıldı; HUD bekleme göstergesi yok; yerel ipucu yığılıyor (DÜŞÜK-4) |
| DÜŞÜK-3 (mağaza testi kapsamı) | **Kısmen** | Tam alan karşılaştırması, "ATLANDI" uyarısı, `writeorder`; AÇIK: sürümlü eski-biçim fixture yok, test hermetik değil (DÜŞÜK-7) |
| DÜŞÜK-4 (hub önbelleği) | **Kapandı** | Boş sonuç saklanmaz |
| DÜŞÜK-5 (FloatingTexts sınırı) | **Kısmen** | 64 sınırı eklendi; mob leash şifası multicast'i bu turda değişmedi (commit listesinde yok) |
| DÜŞÜK-6 (bayat belgeler) | **Büyük ölçüde kapandı** (`9c3addb`) | Yeni hatalı not: CHECKLIST §24 (bkz. §6) |

## 6. `CHECKLIST_FIXES_2026-10-05.md` §22-27 uygulanabilirlik denetimi

Genel: adımlar net ve PIE/headless'a uygun; bu turda hiçbiri koşulmadı. Düzeltilmesi gerekenler:

- **§22**: Adımlar uygulanabilir (host + 1 istemci, `MMOPvP 1/0`). Eksik: (a) PvP ZATEN açıkken geç katılan istemci (ilk replikasyon) — aşağıdaki yeni adım 2; (b) "host loguna/ağ trafiğine boş `ServerAttackTarget` akmamalı" nasıl görülecek yazmıyor: `Net.ListActorChannels`/`stat net` veya sunucuda geçici log olmadan doğrulanamaz → "RPC sayısı (stat net) PvP kapalıyken oyuncu tıklamasında artmamalı" diye netleştir.
- **§23**: Uygulanabilir. Eksik: tek oyunculu/standalone (`-game`) ve dedicated davranışı (dedicated test edilemez: Server target yok); aşağıdaki yeni adım 5-6.
- **§24 (HATALI)**: "(bağlantı hatası/anahtarsız istek sayılmaz; ProcessRequest başlamazsa iade edilir)" — kodda yalnız anahtarsız (istek hiç kurulmaz) ve `ProcessRequest()==false` sayılmaz; **bağlantı hatası, zaman aşımı, HTTP ≠ 200 ve ayrıştırma hatası SAYILIR** (`MMODialogueSubsystem.cpp:140-152`, iade yalnız `:288-295`). Adımı "bağlantı hatası sayaç artırır" diye düzelt (ya da kodu değiştir, bkz. ORTA-1). "`MaxDailyRequests`'i 1 yap" seçeneği uygulanamaz (editörde/ini'de ayarlanamıyor); yalnız dosyadaki sayacı 1499 yapmak çalışır. Test için geçersiz bir `NVIDIA_API_KEY` yeterlidir (HTTP 401 de sayılır).
- **§25**: Uygulanabilir (yalnız test betikleri); "bir `#include` sil → HATA vermeli" adımı çalışır (eksik include artık `RunAllChecks -FullCompile`'ta yakalanır — ancak yalnız listedeki 10 dosya için; DÜŞÜK-1).
- **§26**: İlk üç adım uygulanabilir. Dördüncü adım ("`MMOSave` ile geçiş aynı karede") yerel senkron mağazada pratikte uygulanamaz/tetiklenemez (belge bunu kendisi de söylüyor) → "uygulanamaz, bulut mağaza gelince" olarak işaretle.
- **§27**: (1) İlk adım "can iksirini ~0.5 sn arayla iki kez bas": ilk içişten sonra can doluysa ikinci basışta cooldown ipucu değil "Canin zaten dolu" çıkar (ön kontrol kapıdan önce) → ön koşul: canı çok düşür (`MMODamage 400`) ve ≥ 2 iksir bulundur; mana iksiri (`G`) kapıyı tek başına test etmenin daha temiz yoludur. (2) `Net PktLag=200 PktLagVariance=100` ile 1.0 sn aralıkla basış: sunucu eşiği 0.85 sn, jitter ±100 ms ile sınırda — kesin "yutulmaz" değil "çok daha az yutulur" olarak yaz. (3) 64 sınırı "gözle" doğrulanamaz; hasar yağdırma (çoklu mob + HoT) gerekir ve sayı sınırı ekranda görünmez → "FPS/bellek patlamıyor" ile yetin. (4) Hub önbelleği adımı uygulanabilir.

## 7. PIE / iki-süreç test adımları (yeni; mevcut §22-27'yi tekrarlamıyor)

Hepsi `UnrealEditor.exe … -game` host + `127.0.0.1:7777` istemci ya da PIE "Play As Listen Server + 1 oyuncu". Hiçbiri bu turda koşulmadı.

1. **GameState penceresi (DÜŞÜK-3):** Host'ta `MMOPvP 1` yap. Uzak istemci bağlanırken/yükleme bittikten hemen sonra (ilk saniye) başka oyuncuya tıkla: kovalama gecikmeli de olsa BAŞLAMALI; hiçbir zaman PvP kapalıyken başlamamalı. İstemci konsolunda `GetAll MMOGameState bPvPEnabled` → `True` yazmalı.
2. **Geç katılım:** PvP'yi AÇIKKEN yeni bir istemci bağlansın: istemci `GetAll MMOGameState bPvPEnabled` → `True` (ilk replikasyon). `MMOPvP 0` → birkaç sn içinde `False`.
3. **Bölge geçişi + PvP:** PvP açıkken host düğmeyle Kış'a geçsin: geçişten sonra host ve istemcide `GetAll MMOGameState bPvPEnabled` → `False`; oyuncuya tıklama kovalamamalı (PvP varsayılana döner).
4. **Uzak istemci reddi (kritik):** İstemci konsolunda sırayla `MMOPvP 1`, `MMOTravel`, `MMOBuff Rage`, `MMOSpawnMob OrcWarlord`, `MMOGiveItem HealthPotion 5`: host'ta HİÇBİRİ gerçekleşmemeli; host `GetAll MMOGameState bPvPEnabled` → `False`, istemci envanteri değişmemeli, host dünyasında mob doğmamalı.
5. **Tek oyunculu:** Editörde `Play Standalone` (NM_Standalone): "Kış'a taşı" düğmesi görünür ve çalışır, `MMOPvP 1` çalışır (admin = yerel oyuncu).
6. **Dedicated (yalnız kaynak motor + Server target gelince):** `MMOTravel`/`MMOPvP`/debug RPC'ler hiçbir istemciden çalışmamalı; sunucu log'unda hata olmamalı; PvP varsayılan false.
7. **LLM sayaç doğruluğu (kullanıcının kendi uygulaması):** Geçersiz bir `NVIDIA_API_KEY` ile NPC'yle 3 kez konuş: `Saved/MMO/LLMDailyBudget.txt` sayacı 3 olmalı (HTTP 401 de sayılır → belgedeki beklenti buna göre güncellensin). Host'u kapat-aç: sayaç 3'ten devam. Dosyayı sil: sıfırlanır. Dosyayı `abc` yap: çökme yok, sayaç 0'dan.
8. **Oyuncu başı kötüye kullanım (ORTA-1):** İki istemci; biri NPC yanında konsoldan sürekli `MMONpcTalk`/RPC tetiklesin (Development): diğer istemcinin NPC konuşması da yedek replike düşüyor mu? (beklenen: düşer = kota sorunu).
9. **Tüketilebilir:** Canı düşür (`MMODamage 400`), ≥ 3 iksirle `F`'ye art arda (0.2 sn) bas: yalnız 1 iksir harcanmalı, "Esya kullanimi bekleniyor" satırları yığılıyor mu gözle (DÜŞÜK-4 madde 1); `G` ile mana iksirini 0.5 sn arayla dene.
10. **Hub önbelleği:** Summer → Winter → Summer geçişlerinden sonra PlayerStart yakınında PvP/saldırı reddi (DÜŞÜK-4 `QA_ROUND_2026-10-07` ile aynı).

## 8. Düzeltme önerileri (kodu mmo-developer yazar; öncelik sırası)

1. **LLM:** belgeyi koda hizala (bağlantı hatası SAYILIR) ya da callback'te başarısız HTTP'yi iade et; oyuncu başı günlük/oturum kota ekle; `MaxDailyRequests`'i `config`'e/komut satırına bağla; sayaç yazımını atomik yap ya da seyrelt.
2. **Yazım sırası:** geçiş bekleme penceresinde otomatik/klan/ticaret kaydını ertele (ya da geçiş yazımına sahiplik bayrağı); `BeginWrite/IsNewest` içine `check(IsInGameThread())`; bulut mağaza için Revision/seri kuyruk (NETWORK_AUDIT O6).
3. **JSON:** `ToJson`'a `CheckRanges` ekle; ayrıştırmadan önce ham derinlik/uzunluk ön taraması + derin iç içe negatif test; `ValidateEnum` sayısal dalında `|x| ≤ 2^53` kontrolü; "yeni alan = sürüm artır + eski sürümde isteğe bağlı" mekanizmasını kodla/CI'ye bağla; bu kod yolunun üretimde kullanılmadığını `ACCOUNT_BACKEND_COMPARISON`'a yaz.
4. **Test/araç:** `StaticLint` beyaz listesine `InitGameState`; `-FullCompile` listesini değişen dosyalardan üret; iki-süreçli admin-reddi/PvP-ayna testi ekle; mağaza testini hermetik yap (sürümlü eski-biçim fixture, test yuvalarını sil).
5. **UX/küçük:** `ToOwner` yerel ipucunda sabit anahtar; `bAllowPvP`'yi `BlueprintReadOnly` yap; `FloatingTexts` için etiket önceliği; çift ret mesajını tekle.
6. **Belge:** CHECKLIST §24/§26/§27 düzeltmeleri (§6), NETWORK_AUDIT'e "tavan oyuncu başı değil, çoklu süreçte ayrışır" notu, `FromJson` çağıran yok notu.

## 9. Olumlu (korunmalı)

- Tüm yeni karar yolları sunucu-otoriter kaldı: PvP kararı sunucuda GameMode'dan, istemci yalnız ön kontrol; admin kapısı tek noktada ve varsayılan-red (dedicated false).
- Replikasyon doğru kurulmuş (`DOREPLIFETIME`, `HasAuthority` korumalı yazım); `AMMOGameMode : AGameModeBase` ile uyumlu.
- API anahtarı yalnız ortam değişkeninden; commit/Config/belge sızıntısı yok; LLM çıktısı kırpılıyor/kontrol karakterleri temizleniyor.
- Hata yollarında `OutData`/mevcut kayıt korunuyor; negatif testler sentinel ile bunu kanıtlıyor.
- Derleme + 12 otomatik adım temiz (0 uyarı).
