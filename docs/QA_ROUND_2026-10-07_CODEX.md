# QA turu 2026-10-07 — Codex gece commit'leri (bölge geçişi + kayıt + JSON + denetim logu)

Denetçi: qa-kontrolcu (salt okunur; kod değiştirilmedi, commit/push atılmadı). PIE'de oynanmadı: "PIE gerekli" işaretli her şey doğrulanmamıştır.

## 0. Kapsam ve yöntem

- `git log 275d1d5..HEAD` yalnız 7 commit verir (`e2f7ae4`, `b64afb9`, `7839c02`, `01be8ea`, `85e16e4`, `9121c3d` + belge `9b64adf`). İstenen `07ba76e`, `5102110`, `85d19fd`, `f3161ab` ise `275d1d5`'ten ÖNCE (04:31–04:59). Bu yüzden `c86b4b8` (IMMOCharacterStore) … `HEAD` aralığını okudum: `c86b4b8`, `0e8d10e`, `e7ab1ad`, `07ba76e`, `5102110`, `85d19fd`, `f3161ab`, `ffa12a8`, `e2f7ae4`, `b64afb9`, `7839c02`, `01be8ea`, `9121c3d`. Yazar alanı hepsinde `ozancidik` (Codex ayrı bir yazar olarak görünmüyor).
- **UYARI (eşzamanlı çalışma):** denetim sırasında çalışma ağacı değişti: `MMOCharacter.cpp`, `MMOGameMode.cpp/.h`, `MMOPlayerController.cpp` değiştirildi; `MMOGameState.*` ve `MMOAdmin.*` yeni (izlenmeyen) dosyalar belirdi (PvP aynası / admin işi, bu turun kapsamı dışı). Aşağıdaki "derleme" ve "headless test" sonuçları çalışma ağacı üzerinde alındı; `MMOGameMode.cpp`/`MMOCharacter.cpp` tek-dosya derlemelerinin bu WIP'i içerip içermediğini ayırt edemiyorum. Tüm kod bulguları HEAD'i okuyarak çıkarıldı.
- Derleme/test kanıtı:
  - Tek-dosya `-NoPCH -NoLink -SingleFile` (rsp'de `/W4` var, `/Yu` yok): `MMOGameMode`, `MMOCharacter`, `MMOStatsComponent`, `MMOInventoryComponent`, `MMOTargeting`, `MMOMobSpawner`, `MMOCharacterStoreSubsystem`, `MMOLocalFileCharacterStore`, `MMOClanComponent`, `MMOTradeComponent`: **10/10 Succeeded, 0 uyarı, 0 hata**. **`MMOCharacterSaveJson.cpp`: KIRIK (6 hata) — bkz. ORTA-2.**
  - Normal `Build.bat MyProjectEditor Win64 Development`: `Result: Succeeded` (11 sn; "up to date" — taze derleme kanıtı DEĞİL, tek-dosya sonuçları taze kanıttır).
  - `TestCharacterSaveJson.ps1` GEÇTİ (`schema=1 bytes=1158`), `TestCharacterStore.ps1` GEÇTİ (134/134 yuva yüklendi), `TestZoneTravel.ps1` GEÇTİ, `TestZoneTravelTimeout.ps1` GEÇTİ. (Headless, tek oyuncu, yerel senkron mağaza.)

## 1. Özet (ciddiyet sırası)

| # | Ciddiyet | Başlık |
|---|---|---|
| ORTA-1 | ORTA (örtük; bulut mağazada tetiklenir) | Asenkron kayıt sözleşmesi: yazım sırası/sürüm yok → bayat yazım, eşya çoğaltma/geri sarma; snapshot ile travel arasındaki pencere |
| ORTA-2 | ORTA | `MMOCharacterSaveJson.cpp` PCH'siz/adaptive-unity derlemede kırık (eksik include) |
| ORTA-3 | ORTA | JSON içe aktarma gevşek (strict değil): eksik/bozuk alan sessizce varsayılan → sonraki kayıtta sessiz veri kaybı; int64/enum/NaN kırılganlıkları; belge yanlış |
| ORTA-4 | ORTA | Denetim logları Shipping'de derlenmez (`bUseLoggingInShipping` yok) |
| DÜŞÜK-1 | DÜŞÜK | Bölge geçişi yardımcıları: sayaç tabanlı bariyer, yanıltıcı `true` dönüşü, test kancası durum sızıntısı, bayat yorum |
| DÜŞÜK-2 | DÜŞÜK | Tüketilebilir 1 sn kapısı: sessiz ret, bağımsız istemci/sunucu damgası (jitter'la meşru kullanım düşer), geri bildirim yok |
| DÜŞÜK-3 | DÜŞÜK | Store testi kapsamı zayıf (en yüksek yuva Lv6, kısmi karşılaştırma, 0 yuvada boş geçer) |
| DÜŞÜK-4 | DÜŞÜK | Hub merkez önbelleği: ilk çağrıda boş kalırsa dünya ömrü boyunca güvenli bölge yok; temizlik tembel |
| DÜŞÜK-5 | DÜŞÜK | Hasar/şifa multicast: HUD'da `FloatingTexts` sınırı yok; HoT/leash şifası artık herkese sayı üretir |
| DÜŞÜK-6 | DÜŞÜK | Belgelerde eskimiş/çelişen notlar |

**Kritik (çöker/yanlış çalışır) bulgu yok** — yerel dosya mağazasıyla (bugünkü tek mağaza) kod yolu mantıksal olarak doğru. ORTA-1 bulut mağaza gelince gerçek olur; şimdiden sözleşmeye yazılmalı.

## 2. Bulgular

### ORTA-1 — Asenkron kayıt sözleşmesi bayat yazıma / çoğaltmaya açık (bulut mağazada; yerel senkronda tetiklenmez)

Yer: `MMOGameMode.cpp` `RequestZoneTravel` / `HandleZoneTravelSaveComplete` / `FinishZoneTravel` / `HandleZoneTravelSaveTimeout` / `AbortZoneTravel`; `MMOCharacter.cpp` `SaveCharacterForTravelAsync`, `EndPlay`, `SaveCharacterForTravel`; `MMOCharacterStore.h`.

Sorunlar (hepsi kod okumasıyla; PIE/bulut yok, "doğrulanmalı" bulut uygulaması gelince):

1. **Yazım sırası/sürüm kavramı yok.** `SaveAsync` aynı yuvaya iki yazımı sıraya sokmaz/iptal etmez. Zaman aşımı sonrası (`Abort`) eski yazım uçuşta kalır; oyuncu hemen yeniden deneyebilir ve 60 sn otomatik kayıt (hâlâ senkron `Store->Save`) araya girer. En eski anlık görüntü en son inerse yeni ilerleme ezilir. `bSuccess` yalnız "bu yazım kabul edildi" der; hangi sürümün kalıcı olduğunu söylemez.
2. **Çoğaltma senaryosu (iki oyuncu, takas):** anlık görüntü `RequestZoneTravel` anında alınır; `ServerTravel` `Finish` anında (bulutta saniyeler sonra) çağrılır; arada oyun sürer (kilit yok). A kılıcı B'ye takas ederse `TryExecuteTrade` iki tarafı yeniden senkron kaydeder (yeni, doğru durum). Sonra gelen eski travel yazımları yuvalar üzerinde farklı sırada inerse A eski (kılıç var), B yeni (kılıç var) durumuyla kalabilir → **kılıç ikilenir**. `NETWORK_AUDIT.md` O6'daki "takas bu pencerede tamamlanırsa … tutarlı" cümlesi yalnız senkron mağaza için doğru.
3. **Geri sarma penceresi büyür:** `bTravelSaveCommitted` yalnız `Finish`'te true olur; anlık görüntü ile `Finish` arasında kazanılan her şey (XP, altın, ödül, depo) EndPlay'de kaydedilmediği için kaybolur (EndPlay `!bTravelSaveCommitted` bekler; flag true olduktan sonra zaten atlar, ama flag öncesi değişiklikler snapshot'ta yoktur). Yerelde pencere 0 kare; bulutta 15 sn'ye kadar.
4. **Travel başlamadan önce yeniden doğrulama yok:** `Finish` yalnız weak-pointer'ın hayatta olmasına bakar; bekleme sırasında oyuncu ölürse/savaşa girerse `RequestZoneTravel`'in "canlı ve savaş dışı" koşulu tekrar denetlenmez (yerelde sorun değil).

Önerilen yön (kodu yazma): `IMMOCharacterStore`'a yuva başına artan `Revision`/`ExpectedRevision` (karşılaştır-ve-yaz) ya da yuva başına seri yazım kuyruğu ekle ve sözleşmeye "aynı yuvaya aynı anda tek uçuşta yazım" kuralını yaz; travel anlık görüntüsünden sonra pawn'a "dirty" sayacı bak, `Finish`'te sayaç değiştiyse yeniden kaydet ya da oyuncu RPC'lerini `bTravelLocked` ile kilitle; `SaveCharacter()/EndPlay` yollarını da aynı kuyruğa bağla. Bu iş bulut mağazadan ÖNCE tasarlanmalı.

Olumlu (korunmalı): seri numarası (`ZoneTravelRequestSerial`) eski callback'lerin yeni isteği etkilemesini engelliyor (kodla doğrulandı); tüm hata yollarında zamanlayıcı temizleniyor ve `AbortZoneTravel` `bTravelSaveCommitted`'ı bütün bekleyen karakterler için sıfırlıyor; callback içinde karakter `TWeakObjectPtr` ile tutuluyor (oyuncu kopması → `bSuccess=false` → iptal, çöküş yok); `RequestZoneTravel` yalnız host (`IsLocalController`) ve K-1 yuva çakışmasını (duplicate slot, büyük/küçük harf) önceden reddediyor; K-1 reddi `BuildCharacterSaveRequest`'te korunmuş ve seyahati iptal ediyor.

### ORTA-2 — `MMOCharacterSaveJson.cpp` PCH'siz derlemede KIRIK

Yer: `Source/MyProject/Private/Save/MMOCharacterSaveJson.cpp:39-41`.
Tetik: `Build.bat MyProjectEditor Win64 Development -NoPCH -NoLink -SingleFile=<dosya>` → `error C7568 / C3203 / C2955 / C2665 / C2672` (`TCondensedJsonPrintPolicy` / `TJsonWriter` tanımsız). Neden: `Policies/CondensedJsonPrintPolicy.h` include edilmemiş; yalnız unity/PCH'den geçiyor (commit mesajındaki "zorlanmış derleme geçti" bu yüzden yanıltıcı).
Etki: adaptive unity bu dosyayı (düzenlendiğinde/çalışma kümesindeyken) tek başına derler → bir dahaki dokunuşta derleme kırılır. `RunAllChecks.ps1`'in "Derleme" adımı bunu yakalamaz.
Öneri: `#include "Policies/CondensedJsonPrintPolicy.h"` (ve `Serialization/JsonWriter.h` zaten var) ekle; `RunAllChecks`'e değişen `.cpp` dosyaları için bir `-SingleFile` /W4 adımı ekle.

### ORTA-3 — JSON içe aktarma gevşek; sessiz veri kaybı + kırılgan kodlamalar

Yer: `MMOCharacterSaveJson.cpp` `FromJson` (`JsonObjectToUStruct(..., CPF_SaveGame, 0)` — `bStrictMode` varsayılan false); motor kaynağı `JsonObjectConverter.cpp` ile doğrulandı.

1. **Eksik alan sessizce varsayılan.** `{"SchemaVersion":1,"Data":{}}` başarıyla döner (`bValid=false`, ama `Warehouse`/`Items` gibi alan düşmüş kısmi bir blob `bValid=true` ile gelir ve boş depo/çanta ile yüklenir; bir sonraki kayıt gerçek veriyi ezer). Bozuk tür de çoğunlukla hata vermez: `"Level":"abc"` → `Atoi64` = 0 (motor kodu: string → `FCString::Atoi64`). Üst katman (`LoadCharacter`/`ApplySaveData`) aralık denetimi yapıyorsa kısmen korur; `FromJson` kendisi doğrulamaz. Öneri: v1 için alan-varlığı kontrolü (özellikle `bValid`, `Level`, tüm diziler) ya da strict mod + sürüm başına açık varsayılan tablosu; `ApplySaveData` kelepçelerini `FromJson` çıktısı için de test et.
2. **int64:** motor yazarken `int64`'ü JSON **sayı (double)** olarak yazar (`FJsonValueNumber`); yalnız okurken string'i `Atoi64` ile çözer. Yani `docs/ACCOUNT_BACKEND_COMPARISON.md:75` "int64 JSON'da string'e dönüşür" iddiası bu motor sürümü için yanlış. Bugünkü değerler (UTC saniye ~1.9e9) 2^53'ün çok altında → kayıp yok; ama 2^53'ü aşacak herhangi bir int64 alanı (kimlik, mikro-saniye) sessizce bozulur. Test fixture'ı bu sınırı denemiyor.
3. **Enum ada göre (string) yazılıyor** (`GetAuthoredNameStringByValue`); `EMMOClass`/`EMMOItemRarity` değeri yeniden adlandırılır/silinirse eski blob'un tamamı `false` döner ("Unable to import enum"). Sürüm artırma için bir geçiş (migration) noktası yok: `SchemaVersion < 1 || > Current` dışında dallanma yok.
4. **NaN/Inf konum:** `WriteDouble` `%.17g` → geçersiz JSON üretebilir **[doğrulanmalı]**; yerel `.sav` NaN'ı taşırdı, JSON okunamaz blob olabilir.
5. **Fixture kapsamı:** yalnız ASCII ve ikili-tam-temsil edilen değerler (1234.5, -678.25); `0.1` gibi ikilik olmayan çift, Türkçe karakterli `ClanName`/`Friends`, büyük diziler, negatif testler (bozuk JSON, `SchemaVersion` 0/-1/1.5, `Data` eksik, bilinmeyen enum, dev dizi) yok. **Gerçek bir `.sav` → `ToJson` → `FromJson` eşitliği hiç sınanmıyor** (eski kayıtlarla JSON uyumu kanıtsız). `CompareScriptStruct` FName'de büyük/küçük harf farkını görmez (zararsız).
6. **Dev-dizi/DoS:** `Helper.Resize(ArrLen)` ile bilinmeyen blob boyutu sınırsız; güvenilmeyen kaynak değilse düşük risk, ama bulut blob'una boyut tavanı koy.

Olumlu: sürüm zarfı (`SchemaVersion` + `Data`), ileri sürümün reddi (test geçiyor), `CPF_SaveGame` filtreleme, `OutData` yalnız başarıda atanıyor (`MoveTemp(Candidate)`), bilinmeyen alanlar güvenle yok sayılıyor.

### ORTA-4 — Denetim logları Shipping'de yok

Yer: `MMOCharacter.cpp:50` (`LogMMOIdentityAudit`), `MMOInventoryComponent.cpp:22` (`LogMMOInventoryAudit`), `MMOTradeComponent.cpp:15`, `MMOClanComponent.cpp:15`; `Source/MyProject.Target.cs`.
`UE_LOG` Shipping'de derleme dışı kalır (`bUseLoggingInShipping` hiçbir `.Target.cs`'te yok; ayrıca Server target'ı da yok). `NETWORK_AUDIT.md` O12 "kapandı" diyor; Shipping/dedicated yayında bu loglar üretilmez. Öneri: ya Shipping'de `bUseLoggingInShipping = true` + ayrı log kategorisi/dosya, ya da denetim olaylarını `UE_LOG`'dan bağımsız bir yazıcıya (dosya/HTTP) ver; `NETWORK_AUDIT.md`'ye bu sınırı yaz.

Denetim loglarının diğer yönleri (iyi): biçim dizgisi her yerde sabit (kullanıcı verisi yalnız `%s` argümanı; format-string enjeksiyonu yok); log sahteciliği (`"`/satır sonu ile alan taklidi) kapalı: ad başlangıçta `SanitizeInitialName`'dan, `ServerSetName`'de alnum/`_`/`-`/boşluk filtresinden geçiyor (`ChangeName` motor yolu da `MMOSetName`'e yönlendiriliyor); log hacmi düşük (yükseltme zaten 0,3 sn cooldown'lu, ad değişimi yalnız değişince, satış/depo kullanıcı eylemi başına bir satır); `UpgradeByInstanceId`/`SellByInstanceId` log değerleri `RemoveAt` öncesi kopyalanıyor (asılı referans yok). PII: oyuncu adı + yuva adı oyun takma adıdır (hesap kimliği yok); hesap sistemi gelince `AccountId` ile değiştirilmeli (belgede zaten not edilmiş) ve log saklama süresi/erişimi belirlenmeli.

### DÜŞÜK-1 — Bölge geçişi yardımcıları

- `RequestZoneTravel` senkron mağazada ilk karakter kaydı başarısızsa içeride `AbortZoneTravel` çalışır ama fonksiyon yine `true` döner (`MMOGameMode.cpp` döngü sonu). `MMOTravel` ve test günlüğü "ACCEPTED" der; ekranda iptal mesajı gelir. Eski sözleşme `false` + `OutFailureReason` idi. Öneri: `return bZoneTravelInProgress && serial eşleşiyor` ya da iptal nedenini `OutFailureReason`'a yaz.
- `HandleZoneTravelSaveComplete` karakter/yuva yerine **sayaç** düşürür (`--PendingZoneTravelSaves`). Mağaza bir tamamlanmayı iki kez çağırırsa (bulut tekrar-deneme) bariyer erken açılır; karakter başına "tamamlandı" kümesi tutulmalı.
- Sync mağazada N>1 oyuncuda biri başarısız olunca kalan oyuncular yine de kaydedilmeye devam eder (zararsız, gereksiz yazım).
- Test kancaları (yalnız `!UE_BUILD_SHIPPING`): `RequestZoneTravel` erken reddedilirse `bTestDropNextZoneTravelSaveCallback`/`TestZoneTravelTimeoutOverrideSeconds` sıfırlanmaz; sonraki gerçek istekte ilk callback düşer / zaman aşımı 0,75 sn olur (yalnız Development'ta `MMOTestZoneTravelTimeout` çalıştırılmışsa).
- `MMOCharacter.h:120-122` yorumu hâlâ "GameMode bu durumda seyahati iptal eder" diyor (bu artık `SaveCharacterForTravelAsync` yolu).
- Bilinen/eski (belgelenmiş) açıklar yerinde: `ServerTravel` true döndükten sonra harita yüklenemezse `bZoneTravelInProgress`/`bTravelSaveCommitted` takılı kalır; yükleme penceresinde kopan oyuncu için `AnnouncePresence(false)` atlanır (arkadaşlar çevrimiçi sanır); tek oyuncunun savaşı herkesin geçişini engeller (listen için kabul edilebilir).
- K-1 etkileşimi: yuva reddi tüm geçişi "Bir karakter kaydedilemedi" genel mesajıyla iptal eder (kim olduğu host'a söylenmez; yalnız ilgili oyuncuya `ToOwner` gider).

### DÜŞÜK-2 — Tüketilebilir 1 sn kapısı (`7839c02`)

Yer: `MMOCharacter.cpp` `TryBeginConsumableUse` / `UseHealthPotion` / `UseConsumable`.

- Kapı **sunucuda da** (RPC gövdesi aynı fonksiyonu çağırır) uygulanıyor: doğrudan RPC spam'i kesiliyor, iyi. Ön kontroller (zaten dolu/yok) damga ilerletmiyor, iyi.
- **Geri bildirim yok:** kapı `false` dönünce sessiz `return`; HUD'da bekleme göstergesi/ipucu yok. Can→mana iksirini ≤1 sn içinde basan oyuncu ikinciyi neden çalışmadığını göremez ("iksir zinciri" tasarım kararı; ama sessiz).
- **İstemci ve sunucu damgaları bağımsız** (`LastConsumableUseTime` replike değil): istemci basış anında, sunucu RPC varış anında damgalar. ~1,0 sn aralıkla basılırsa ağ titreşimi (RPC1 geç, RPC2 erken) yüzünden sunucu ikinci geçerli kullanımı sessizce reddedebilir; istemci kendi kilidini çoktan harcadığı için hiçbir şey olmaz ve eşya harcanmaz. Öneri: sunucu cooldown'ını 0,8–0,9 sn yap (istemci 1,0 sn), sunucuda reddedince `ToOwner` hint gönder.
- Üç tüketilebilir tür (can/mana/parşömen) aynı kapıyı paylaşır: tasarım kararı; yalnız not.

### DÜŞÜK-3 — Store testi kapsamı (`ffa12a8`, `TestCharacterStore.ps1`)

- Kapsıyor: diskteki tüm `MMOSave_*.sav` yuvalarının listelenip yüklenmesi (134/134, `loadFailed=0`, `existsMismatch=0`), en yüksek seviyeli geçerli kayıtla geçici yuvaya Save→Load.
- Kapsamıyor / zayıf: bu makinedeki 134 kaydın **en yükseği Lv6** (gidiş-dönüş kaynağı `level=6`) — gerçek eski biçim (Warehouse, MasteryRanks, TMap cooldown, Lv30+ görev/klan/arkadaş) örnek yok. Eşitlik yalnız `bValid/Level/Experience/ClanName/MapName/Location/Friends/CharacterClass` alanlarını karşılaştırır; `Items/Equipment/Warehouse/Skills/Quests/Gold/Cooldown` karşılaştırılmaz. `slots == 0` iken (temiz makine/CI) test boş geçer. Temp yuva test yarıda kesilirse `MMOSave_ZStoreTestTemp.sav` artar ve klan adı taramasına karışır. Exec komutu `MMODebugCommandsEnabled` kapısını kullanmaz (yalnız `!UE_BUILD_SHIPPING`).
- Öneri: `CompareScriptStruct` ile tam eşitlik; sürümlenmiş sabit bir "eski biçim" `.sav` fixture'ı repoya koy; `slots==0` ise "ATLANDI" olarak raporla.

### DÜŞÜK-4 — Hub güvenli bölge merkezi önbelleği (`9121c3d`)

Yer: `MMOTargeting.cpp` `GetHubSafeZoneCenters`.

- Doğru: dünya başına `TWeakObjectPtr<UWorld>` anahtarı (seamless travel yeni `UWorld` yaratır → yeni önbellek; serial'lı zayıf işaretçi yeniden kullanılan adreste yanlış eşleşmez); çoklu `PlayerStart` hepsi merkez (eski davranış aynı); `DistSquared2D < R²` eski `Dist2D < R` ile eşdeğer; istemcide de çalışır (PlayerStart `bNetLoadOnClient`).
- Risk: önbellek **ilk çağrıda** kuruluyor. O an dünyada `PlayerStart` yoksa/yüklenmediyse (ör. ileride World Partition/akış, BeginPlay öncesi çağrı) liste boş kalır ve o dünya ömrü boyunca hub güvenli bölgesi yok (PvP/saldırı güvenliği düşer); yeniden doldurma yok. Mevcut Summer/Winter haritaları tek kalıcı seviye olduğundan bugün tetiklenmez. Öneri: boş sonucu önbelleğe alma (ya da `PlayerStart` sayısı değiştiyse yenile).
- Ölü dünya kayıtları yalnız sonraki çağrıda temizleniyor (belgedeki "temizlenir" doğru ama tembel; sızıntı yok).

### DÜŞÜK-5 — Hasar/şifa multicast (`f3161ab`)

- Doğrulandı: tüm hasar yolları (`UGameplayStatics::ApplyDamage` → `AMMOCharacter::TakeDamage`; DoT dahil) `MulticastCombatText` çağırıyor, `Stats->ApplyDamage` yalnız `TakeDamage`'den çağrılıyor → eski `OnRep_Health` sayısı kalktığı için sayı kaybı yok. Öldürücü vuruşta mob 2 sn sonra yok edildiğinden multicast yetişir. `Unreliable` doğru seçim (kozmetik); `FVector_NetQuantize` + `float` + `bool` küçük; yalnız ilgili (relevant) istemcilere gider; dedicated `NM_DedicatedServer`'da HUD'a dokunmaz; `GetFirstLocalPlayerController` çok-HUD PIE'de her dünya için ayrı. HUD yolu (`AddDamageNumber(WorldLocation, Amount, bIsHeal)`) imza uyumlu.
- Dikkat: `FloatingTexts` dizisinde sınır yok (ömür dolunca `DrawFloatingTexts` içinde siliniyor; Canvas yoksa temizlenmez). Mob leash şifası (`MMOMob.cpp:439` `Heal(MaxHealth)`) ve HoT her tik'te (`MMOStatsComponent.cpp:777`) artık TÜM ilgili istemcilere "+N" üretiyor (önceden yalnız host); ağ maliyeti küçük ama görsel kalabalık olabilir. Öneri: HUD'da 64–128 sınır; mob leash şifasında `bIsHeal` multicast'i atla.
- PIE gerekli: iki istemcide hasar sayısının tek kez görünmesi (çift sayım yok), uzak istemcide kendi hasarının ve mob hasarının görünmesi, aktör ilgisi dışındaki oyuncuda görünmemesi.

### DÜŞÜK-6 — Belge çelişkileri/eskimiş notlar

- `docs/ACCOUNT_BACKEND_COMPARISON.md:20` "Kaydetme senkron. `SaveCharacterForTravel()` `bool` döner ve geçiş buna bağlı" ve `:124` "asenkron kaydın eklenmesi iki seçenekte de aynı refactor" — bölge geçişi artık `SaveAsync` onayına bağlı (`:263-264` "sunucu tarafı senkron API" yorumu da eski). `:75` int64 string iddiası motor kaynağına göre yanlış (bkz. ORTA-3).
- `docs/NETWORK_AUDIT.md`: O6 satırı (`:30`) hâlâ "`bZoneTravelInProgress` sıfırlanmıyor" ve eski satır numaraları (`MMOGameMode.cpp:118-168`, `:146`, `:156`, `:158`); `:179` "kayıt zaten `SaveCharacterForTravel` ile alındığı için veri kaybı yok"; O6 "takas tutarlı" cümlesi yalnız senkron mağaza için geçerli (ORTA-1); `:206` ve `:275` "disk G/Ç … asenkron kayıt/DB" notu bölge geçişi için kısmen eski; D10 "ölü dünya kayıtları temizlenir" tembel temizlik; O12 "kapandı" Shipping sınırını belirtmiyor (ORTA-4).
- `docs/PLAYTEST_CHECKLIST.md`: yeni bölümler çelişmiyor; b64afb9/7839c02/f3161ab/9121c3d/e2f7ae4 için maddeler var (aşağıdaki test adımları yalnız eksik/ek senaryolar).

## 3. İstenen odak başlıklarına kısa yanıt

1. **Bölge geçişi + kayıt:** Yerel senkron mağazada doğru (headless GEÇTİ). Yarış durumları (ORTA-1) yalnız gerçekten asenkron mağazada açılır; zaman aşımı kilidi/bayrakları temizliyor ve eski callback'i seri numarasıyla eliyor; çifte callback ve yarım yazım koruması yok. `SaveCharacterForTravel` senkron sözleşmesi otomatik kayıt/EndPlay/klan/ticaret yollarında hâlâ senkron `Save` (belgelenmiş). Host dışı oyuncular kayda dahil. K-1 refusal bölge geçişini iptal ediyor (doğru, mesaj genel).
2. **JSON:** ORTA-2/3 (ve DÜŞÜK-3).
3. **Denetim logları:** biçim güvenli, hacim düşük, PII sınırlı; Shipping'de yok (ORTA-4).
4. **Spawner sunucuya özel:** güvenli. Spawner yalnız `AMMOGameMode`'da `SpawnActorDeferred` ile (sunucu) üretiliyor; `Content/` içinde spawner referansı yok; `TActorIterator<AMMOMobSpawner>` yok; HUD/diğer kod spawner'a bakmıyor; `MMOProbeSpawn`/`MMOProbeSurface` statik `FindSafeSpawnLocation` kullanıyor, etkilenmiyor. Moblar kendi replike aktörleri.
5. **Tüketilebilir hız sınırı:** DÜŞÜK-2.
6. **Hub önbelleği:** DÜŞÜK-4.
7. **Multicast sayılar:** DÜŞÜK-5 (bant genişliği küçük, Unreliable doğru).
8. **/W4 tek-dosya:** 10/11 temiz; `MMOCharacterSaveJson.cpp` KIRIK (ORTA-2).
9. **Belgeler:** DÜŞÜK-6.

## 4. PIE'de doğrulanması gereken (yapılmadı)

Bu turun yeni/ek adımları (mevcut checklist maddelerini tekrarlamıyor):

- [ ] İki pencere (host + uzak istemci), ikisi de canlı ve savaş dışı: host `MMOTravel` → ikisi de Winter'a geçmeli; ardından host konsolda `Zone travel started` logu ve iki yuvanın `.sav` zaman damgası güncel olmalı.
- [ ] Aynı senaryoda travel anında uzak istemciyi kapat (pencere kapat): geçiş iptal olmalı ("Bir oyuncu kayıt sırasında ayrıldı") ya da kalan oyuncuyla tutarlı sürmeli; `bZoneTravelInProgress` takılı kalmamalı (ikinci `MMOTravel` kabul edilmeli); host loglarında `Zone travel aborted` satırı.
- [ ] Uzak istemciyi `MMOSetName` ile (aynı oturumda) yeni boş bir ada çevir, sonra `MMOTravel`: kayıt reddedilmemeli; kayıtlı bir ada çevirmeye çalış → "Bu ad kayıtlı bir karaktere ait" (K-1) ve geçiş kilitlenmemeli.
- [ ] Travel yükleme penceresinde (haritanın değiştiği ~1–3 sn) altın/XP kazan ve hemen bağlantıyı kes: kayıtta bu kazancın olmadığını (geri sarma) gözle; arkadaş listesinde oyuncunun "çevrimiçi" kaldığını doğrula (bilinen açık, DÜŞÜK-1).
- [ ] 200 ms gecikme (`Net PktLag=200`, `PktLagVariance=100`) ile uzak istemcide Can İksiri'ne ~1,0 sn aralıkla (saniyede bir) bas: bazı basışların sessizce yutulup yutulmadığına ve eşyanın harcanmadığına bak (DÜŞÜK-2). Can→Mana iksirini ≤1 sn içinde bas: ikinci basışta ipucu çıkmıyor mu not et.
- [ ] İki istemcili oyunda mob ve oyuncu hasarı: her istemcide sayı TEK kez, doğru konumda; aktör ilgisi dışındaki uzak oyuncuda sayı yok; HoT (Rahip) ve mob eve dönerken büyük "+N" şifası görünüyor mu (DÜŞÜK-5).
- [ ] Hub (PlayerStart) yakınında PvP/saldırı reddi: ilk saldırı denemesi ilk karede de reddedilmeli; Summer→Winter→Summer geçişlerinden sonra da (önbellek yeni dünya) aynı yarıçapta reddedilmeli.
- [ ] İstemci konsolunda `Net.ListActorChannels`: `MMOMobSpawner` kanalı olmamalı; mob sayısı ve hareketi değişmemeli (mevcut checklist'te var; yalnız uzak istemcide mob respawn'ının hâlâ sürdüğünü ekle: bir mobu öldür, 5–10 sn sonra yenisi çıkmalı).
- [ ] Not: bulut mağaza yokken ORTA-1 PIE'de tetiklenemez; bulut mağaza eklendiğinde "yavaş kayıt" (yapay 2–3 sn gecikme) altında takas+travel senaryosu yeniden test edilmeli.

## 5. Düzeltme önerileri özeti (kodu mmo-developer yazar)

1. `MMOCharacterSaveJson.cpp`'ye `#include "Policies/CondensedJsonPrintPolicy.h"` ekle; `RunAllChecks`'e değişen dosyalar için `-NoPCH -NoLink -SingleFile` /W4 adımı.
2. Store sözleşmesine yuva-başına sürüm/seri yazım kuralı + travel öncesi "dirty" yeniden doğrulama (veya `bTravelLocked`); çifte callback için karakter-başına tamamlanma kümesi; `RequestZoneTravel` dönüş değeri.
3. `FromJson`: alan varlığı/aralık doğrulaması ve negatif testler; gerçek `.sav` → JSON → eşitlik testi; int64 > 2^53 ve enum yeniden adlandırma politikası belgele; blob boyut tavanı.
4. Shipping/dedicated için denetim logu yolu (`bUseLoggingInShipping` ya da ayrı yazıcı).
5. Tüketilebilir: sunucu cooldown'ını istemciden kısa tut, reddedilince hint ver, HUD bekleme göstergesi.
6. Hub önbelleği: boş sonucu önbelleğe alma; HUD `FloatingTexts` sınırı.
7. Belgeleri DÜŞÜK-6'ya göre güncelle.
