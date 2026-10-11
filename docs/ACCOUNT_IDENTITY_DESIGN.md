# Hesap kimliği: buluttan bağımsız en küçük güvenli ara adım (tasarım)

Tarih: 2026-10-11. Durum: **tasarım**. Kod yazılmadı, derlenmedi. Sayılar `[PLACEHOLDER]`, ölçülecek.
İlgili belgeler: `docs/SECURITY_AUDIT.md` K-1/Y-5/§8, `docs/NETWORK_AUDIT.md` Y1, `docs/DECISIONS_NEEDED.md` §4,
`docs/ACCOUNT_BACKEND_COMPARISON.md` (PlayFab/OWS, Aşama 1-3).

## 1. Oyuncu deneyimi ve neden şimdi

Oyuncu sunucuya hesap adı ve parolasıyla bağlanır ve yalnız kendi karakterlerini açabilir. Başka biri onun karakter adını
yazıp karakterini, altınını, başarımlarını ya da postasını alamaz. Aynı hesapla ikinci kez girilemez.

Neden şimdi: son günlerde eklenen üç sistem (başarım, günlük görev, posta) da oyuncu adına bağlı dosyalar yazıyor. Posta
**altın taşıyor** (`/posta gonder isim [altin]`), yani ad kapma açığı artık başkasının cüzdanını boşaltmak anlamına da geliyor.
Ad kimliğine bağlı her yeni sistem açığı biraz daha büyütüyor. Bulut hesap (PlayFab) ise dedicated server ister, o da
kaynaktan derlenmiş motor ister. Yani kısa vadede bulut hesap yok ve ara adımın yerel olması gerekiyor.

## 2. Kimlik bugün nereden geliyor (dosya:satır)

Satır numaraları 2026-10-11 tarihli çalışma ağacından alındı. Başarım, günlük ve posta dosyaları şu an başka bir ajan
tarafından düzenleniyor (commit'lenmemiş). Bu dosyalarda satırlar kayabilir; davranış aşağıdaki gibi.

### 2.1 Kaynak: istemcinin seçtiği ad

| Nerede | Ne yapıyor |
|---|---|
| Motor `Engine/Source/Runtime/Engine/Private/GameModeBase.cpp:815-821` (`InitNewPlayer`) | Bağlantı URL'indeki `?Name=` değerini `.Left(20)` ile alıp doğrudan `ChangeName(..., false)` çağırıyor. Doğrulama yapmıyor. |
| Motor `World.cpp:7379` | `LogNet: Login request: <URL seçenekleri>` satırını yazıyor. `?Password=` dahil tüm seçenekler sunucu loguna düşüyor. |
| Motor `World.cpp:7425` | İstemci girişinde `GameMode->PreLoginAsync(Options, RemoteAddress, UniqueId, OnComplete)` çağrılıyor. Asenkron doğrulama için motorun hazır kancası. |
| `Private/Core/MMOGameMode.cpp:381-392` | `SanitizeInitialName`: yalnız ASCII harf/rakam, `_`, `-` ve boşluk kalıyor (`MMONameRules::IsAllowedNameChar`, tur 4; Türkçe harf atılır: `Ömer` -> `mer`), en çok 20 karakter. |
| `MMOGameMode.cpp:437-549` (`ChangeName`) | `bNameChange == true` ise karakterin `MMOSetName` yoluna yönlendiriliyor (443-450). Başlangıç adı çevrimiçi biriyle kanonik anahtarla (`MMONameRules::CanonicalKey`: boşluk -> `_`, ASCII küçük harf) çakışıyorsa (459-481), boş kalıyorsa ya da işletim sistemi adı kalıbındaysa `Gezgin######` veriliyor (482-519). Rastgele ad, var olan bir yuvayla çakışmasın diye `Store->Exists` ile kontrol ediliyor (494). Ad değiştiyse 3 sn sonra sohbet bildirimi; "yeni karakter" uyarısı yalnız `Gezgin` atamasında (526-548). |
| `MMOGameMode.cpp:551-599` (`PreLogin`) | İsteğe bağlı `-MMOPassword=` / `?Password=` herkes için **tek ortak parola** (561-570). Düz `Equals` ile karşılaştırılıyor, deneme sınırı yok. Yalnız **çevrimiçi** bir adla (kanonik anahtarla) çakışan bağlantı reddediliyor (577-598); temizlenince boş kalan ad burada reddedilmiyor, `ChangeName`'de `Gezgin` alıyor. |
| `Private/Core/MMOAdmin.cpp:6-20` | Yönetici, listen-server host'unun yerel denetleyicisi. Dedicated sunucuda yönetici yok. |

### 2.2 Ad, kayıt yuvasına dönüşüyor

| Nerede | Ne yapıyor |
|---|---|
| `Private/Characters/MMOCharacter.cpp:1114-1131` (`GetSaveSlotName`) | Yuva `MMOSave_` + PlayerState adı (`MMONameRules::SlotKey`, tur 4). Boşluk `_` oluyor, yalnız ASCII harf, rakam, `_`, `-`, en çok 32 karakter; harf büyüklüğü korunuyor. **Tek kimlik kaynağı bu fonksiyon.** (2026-10-11 tur 4 satırı; aşağıdaki diğer `MMOCharacter.cpp` satırları önceki çalışma ağacındandır, kaymış olabilir.) |
| `MMOCharacter.cpp:932, 969-979` | Possess'ten 0,6 sn sonra `AutoLoadDeferred` -> `LoadCharacter` çalışıyor. Adı bilen herkes o karakteri yüklüyor (K-1 senaryo 2). |
| `MMOCharacter.cpp:1381-1397` | Yuva yoksa yeni karakter oluşturuluyor, başlangıç paketi veriliyor, `ActiveSaveSlot` atanıyor. |
| `MMOCharacter.cpp:1241-1252` | K-1 kısmi koruma: yüklenen yuva ile şimdiki yuva farklıysa ve hedef yuva doluysa yazmıyor. |
| `MMOCharacter.cpp:1589-1725` (`ServerSetName`, tur 4 satırları) | Ad değiştirme. ASCII dışı harf reddi (1606-1610); soğuma her disk kontrolünden önce (1627-1647); uzak, yönetici dışı istemci kaydı olan karakterin adını değiştiremiyor (ad kilidi, 1648-1663); çevrimiçi çakışma kanonik anahtarla (1664-1681); uzak istemci kayıtlı bir adı alamıyor, ret soğuma başlatıyor (1682-1698). Host/yönetici ad kilidine ve kayıtlı-ad kontrolüne tabi değil (ASCII kuralı, çevrimiçi çakışma ve 5 sn yerel soğuma onlar için de geçerli). |
| `MMOCharacter.cpp:1551-1561` | `ServerLoadCharacter` yalnız yöneticiye açık. "MMOLoad ile sahiplenme" uzak istemci için zaten kapalı. |
| `MMOCharacter.cpp:952-957` (`UnPossessed`), `933` (60 sn otomatik kayıt) | Kayıt her zaman `GetSaveSlotName()` yuvasına yazılıyor. |
| `Public/Save/MMOCharacterStore.h:47-49, 71-113` | `IMMOCharacterStore` yuva adını opak anahtar olarak kullanıyor. Yorumda "hesap sistemi gelince karakter kimliğine dönüşecek" yazıyor. |
| `Private/Save/MMOLocalFileCharacterStore.cpp:8-44` | `Saved/SaveGames/<yuva>.sav`. `ListSlotNames` yalnız `MMOSave_*.sav` dosyalarını listeliyor (39). |
| `MMOGameMode.cpp:658-668` | Bölge geçişinde aynı yuvaya sahip iki oyuncu reddediliyor. |

### 2.3 Ada bağlı diğer sistemler

| Sistem | Nerede | Anahtar |
|---|---|---|
| Başarım | `Private/Components/MMOAchievementComponent.cpp` (HEAD 220-243; düzenleniyor: `GetLiveSlot`, `StoragePath`/`LoadedSlot` yüklemede sabitleniyor) | `Saved/MMOAchievements/<GetSaveSlotName>.json` |
| Günlük görev | `Private/Components/MMODailyComponent.cpp` (HEAD 186-209; aynı düzeltme) | `Saved/MMODaily/<GetSaveSlotName>.json` |
| Posta (kutu) | `Public/Mail/MMOMailStore.h:17-33`, `Private/Mail/MMOMailStore.cpp:13-39` (düzenleniyor: `MMOMail_<küçük harf ad>.json`) | Alıcı **adı**, temizlenmiş ve küçük harfe çevrilmiş |
| Posta (gönderen, alıcı var mı) | `Private/Components/MMOMailComponent.cpp:40-54` (`GetMyName` = PlayerState adı), `56-80` (`RecipientExists`; 78. satır `MMOSave_<ad>.sav` dosyasını mağazayı **atlayarak** doğrudan diskte arıyor) | Ad |
| Klan | `Private/Components/MMOClanComponent.cpp:357-394` (`SavedClanNameInUse`, bütün yuvaları yüklüyor), `584, 595` (yuva), `328-336` (görünen ad) | Yuva + ad |
| Arkadaş | `Public/Save/MMOSaveGame.h:108` (`TArray<FString> Friends`), `MMOFriendsComponent.cpp:30-37` | Ad (kayıt içinde) |
| Fısıltı | `Private/Player/MMOPlayerController.cpp:1719-1736` | Çevrimiçi ad |
| LLM oyuncu kotası | `MMOPlayerController.cpp:2660` -> `MMODialogueSubsystem.cpp:414-430` | PlayerState adı |
| Ticaret, parti, metrik | `MMOTradeComponent.cpp:25`, `MMOPartyComponent.cpp:29`, `Public/Core/MMOMetrics.h:24`, `MMOInventoryComponent.cpp:341-347` | Görüntü ve log için ad/yuva |

### 2.4 Diskteki gerçek veri (2026-10-11, bu makine)

`Saved/SaveGames`: **724 dosya**. 665'i `MMOSave_Gezgin######`, 59'u `MMOSave_CIDIK-<hex>`. Elle seçilmiş bir karakter adı
**yok**. `Saved/MMOAchievements` ve `Saved/MMODaily` klasörleri yok, `Saved/MMOMail` boş. Sonuç: bu makinede taşınacak
gerçek oyuncu verisi **pratikte sıfır**. Göç tasarımı başka host'lar ve gelecek için yapılıyor, bugünkü iş yükü değil (§4.6).

## 3. Kapsam ve kapsam dışı

**Kapsam (B adımı, §4):** yerel hesap dosyası (tuzlu, yavaş parola özeti), girişte doğrulama, hesabın karakter adlarına sahip
olması, hesap modunda ad değiştirmenin kapanması, aynı hesaba çift girişin reddi, deneme sınırı, eski kayıtları yöneticinin
sahiplendirmesi. Hepsi `-MMOAccounts` sunucu bayrağının arkasında. Bayrak yoksa davranış **bugünküyle aynı** kalır (bütün
self-test'ler ve NetSmokeTest `SetPlayerName` ile çalışmaya devam eder).

**Kapsam dışı:** giriş ekranı veya UI (bağlantı URL'i kullanılıyor), e-posta, parola sıfırlama akışı, ağ şifreleme (TLS/DTLS),
bulut (PlayFab/OWS), dedicated server, karakter silme, ad değiştirme, `CharacterId` ile yeniden anahtarlama (ertelenen Adım 6),
`MMOSaveGame.h` değişikliği (korunan yol, **dokunulmaz**).

**Mevcut sistemlerle ilişki:** var olan hiçbir sistem yeniden yazılmıyor. `IMMOCharacterStore`, başarım, günlük ve posta
depoları **olduğu gibi kalıyor**. Değişen tek şey adın nasıl elde edildiği: ad artık bir hesaba ait, parolayla açılıyor ve
değiştirilemiyor.

## 4. Tasarım: B-min ("ad = hesaba ait, değişmez karakter anahtarı")

### 4.1 Ana fikir ve dosyaları yeniden anahtarlamanın neden şimdi değmediği

İstenen şey, kayıt, başarım, günlük ve posta dosyalarının "ad" yerine "hesap/karakter kimliği"ne bağlanmasıydı. İki yol var:

- **B-min (öneri):** karakter adı **hesaba ait ve değişmez** olur. Dosyalar bugünkü gibi ada göre anahtarlanmaya devam eder,
  ama o ada yalnız sahibi hesap parolasıyla ulaşabilir. Bu durumda ad, fiilen sabit bir karakter kimliği olur. Dört deponun
  hiçbirine dokunmak gerekmez. Dosya taşıma ve göç riski yoktur.
- **B-full (Adım 6, ertelendi):** her karakter için rastgele bir `CharacterId` üretilir ve dosyalar bu kimliğe taşınır.

K-1'in üç saldırısı da (çalma, ezme, kopyalama; `SECURITY_AUDIT.md:83-86`) **adı kimin kullanabildiği** ile ilgili. Dosya
adının biçimiyle ilgili değil. Ad sahipli ve değişmez olduktan sonra dosyaları rastgele kimliğe taşımak **güvenliğe bir şey
katmaz**. B-full yalnız iki durumda gerekir: (a) ad değiştirme özelliği istenirse, (b) PlayFab Aşama 2 (`CharacterId`)
başlarsa. Bu yüzden B-full'u bugün yapmak sahte iş olur. Tasarımı §4.7'de hazır duruyor.

B-min'in yan kazancı: girişten sonra tutulması gereken **tek kimlik bilgisi PlayerState adı**. Motor bu adı seamless travel'da
zaten taşıyor. Seamless travel'da yeni bir PlayerController oluşturuluyor (`GameModeBase.cpp:607-622`), ama denetleyici veya
bağlantıya göre ayrı bir "oturum eşlemesi" tutmamız gerekmiyor. Summer/Winter geçişine dokunulmuyor.

### 4.2 Veri (yerel dosyalar, `MMOSaveGame.h` dışında)

Kök klasör `Saved/MMOAccounts/` (git-ignored `Saved/` altında). Yazımlar, başarım bileşeninin kullandığı yöntemle
(`.tmp` + `IFileManager::Move`) atomik yapılır.

```
Saved/MMOAccounts/acc_<hesapanahtari>.json
{ "version": 1, "account": "ozan", "algo": "pbkdf2-hmac-sha1",
  "iter": 0, "salt": "<32 hex>", "hash": "<40 hex>",
  "created": 1760000000, "fails": 0, "lockUntil": 0 }

Saved/MMOAccounts/names/<yuvaanahtari-kucuk-harf>.json      (TEK DOGRULUK KAYNAGI: ad -> hesap)
{ "version": 1, "account": "ozan", "name": "Ozan", "created": 1760000000, "claimedBy": "self|admin" }
```

- Hesap anahtarı: küçük harf, `[a-z0-9_-]`, 3-20 karakter.
- Yuva anahtarı **`GetSaveSlotName` ile aynı kurala** uyar: boşluk `_` olur, harf, rakam, `_`, `-` kalır, küçük harfe çevrilir.
  Bu yüzden "Ali Veli" ve "ali_veli" aynı adı ifade eder. Self-test bu eşitliği gerçek bir pawn'ın `GetSaveSlotName()`
  sonucuyla karşılaştırarak doğrular. Ayrı bir ikinci kural yazılmaz, sapma testle yakalanır.
- Ad sahipliği yalnız `names/` dosyasında tutulur. Hesap dosyası karakter listesi tutmaz, çünkü iki dosyanın birlikte
  güncellenmesi gerekseydi bir çökme sonrası tutarsızlık doğabilirdi. Hesabın karakterleri `names/` klasörü taranarak bulunur.
  Bu tarama yalnız karakter oluştururken yapılır, yani nadirdir.
- Hesap başına en çok `[PLACEHOLDER 3]` karakter.
- **Bozuk hesap dosyası** `.bad` uzantısıyla kenara alınır ve giriş **reddedilir**. Asla "hesap yok" sayılıp yeniden kayıt
  açılmaz, çünkü bu hesap devralma demek olurdu. Bu koruma mutasyon testiyle sınanır (§7.2).

### 4.3 Giriş akışı (sunucu otoritesi, replikasyon yok)

Bağlantı URL'i:

```
open 192.168.1.10:7777?Name=Ozan?Account=ozan?AccountPassword=<parola>
```

`?Password=` adı mevcut ortak sunucu parolası (`-MMOPassword`) için kullanılıyor. Hesap parolası ayrı bir anahtardır ve ikisi
birlikte kullanılabilir.

1. **`PreLogin` (uzak istemci, mevcut override genişletilir):** bayrak yoksa bugünkü kod çalışır. Bayrak varsa saf bir karar
   fonksiyonu `EvaluateLogin(Options, Address)` sırayla şunları yapar:
   1. IP başına deneme sınırı. Aşılmışsa özet **hesaplanmadan** ret.
   2. Hesap adı ve parola biçimi.
   3. Hesap dosyası. Yoksa ve kayıt açıksa (karar 1) hesap oluşturulur.
   4. Hesap kilidi (`lockUntil`).
   5. PBKDF2 doğrulaması ve sabit zamanlı karşılaştırma. Başarısızsa `fails++`, eşik aşılırsa `lockUntil` yazılır.
   6. `?Name=` (aynı `SanitizeInitialName` + `Left(20)` ile) bu hesaba mı ait? Değilse sahipsiz ve yeni mi? Yeni ise:
      başka hesapta değil, **diskte `.sav` olarak yok** (eski kayıt, §4.6) ve işletim sistemi adı kalıbında değil
      (`LooksLikeOsDefaultName`).
   7. Bu hesabın başka bir adı şu an çevrimiçi mi? Çevrimiçi PlayerState adları `names/` üzerinden hesaba çevrilerek bakılır.

   Hata metni istemciye `ErrorMessage` olarak döner. Başarılı girişte `fails=0` yazılır.
2. **`InitNewPlayer` override (yeni, `MMOGameMode`):** iki eşzamanlı giriş, ikisi de PreLogin'den geçmiş olabilir. Bu yüzden
   ad veya hesap çevrimiçiyse `ErrorMessage` döndürülür. Motor bu durumda PC'yi yok edip girişi reddeder
   (`GameModeBase.cpp:731-736`). Hesap modunda bu çakışma `Gezgin######` adına **düşürülmez**, giriş reddedilir.
   Seçenek dizesi PreLogin'deki ile aynı bağlantının `RequestURL`'i olduğu için özet burada yeniden hesaplanmaz.
3. **Host (listen server):** host için PreLogin çağrılmaz. Host sahipli bir adla açılmak isterse `InitNewPlayer` içinde kendi
   URL seçenekleriyle aynı doğrulama eşzamanlı yapılır. Başarısız olursa ya da parola verilmemişse host `Gezgin######` adını
   alır, hata almaz. Host diske zaten tam erişimi olduğu için bir güvenlik sınırı değildir. Bu kural yalnız kazara ezmeyi önler.
4. **`ChangeName(bNameChange=false)`:** `Gezgin######` adayı yuvası **ve** `names/` dosyası olmayan bir ad olmalı.
   Bugün yalnız yuva kontrol ediliyor (`MMOGameMode.cpp:494`). Sahiplenilmiş ama henüz kaydı yazılmamış bir ad da dolu sayılır.
5. **`ServerSetName` (`MMOCharacter.cpp:1589`):** hesap modunda uzak istemci için **her zaman** ret ("Hesap modunda ad
   değiştirilemez"). Host için sahipli bir ada geçiş reddedilir. Bunun için gövdenin başına **tek bir çağrı** eklenir:
   `if (!MMOAccount::AllowsRename(this, Trimmed, Reason)) { Reject(Reason); return; }`. Mantık yeni dosyada durur.
6. Karakter yüklemesi, kaydı, `ActiveSaveSlot`, başarım, günlük ve posta **değişmez**. Hepsi zaten `GetSaveSlotName()` ya da
   PlayerState adını kullanıyor ve o ad artık doğrulanmış.

**Replikasyon:** yeni replike alan ve yeni RPC yok. Hesap adı, tuz ve özet istemciye hiç gitmez. Hata metinleri motorun
giriş reddi yoluyla iletilir.

**Kalıcılık:** hesap dosyaları karakter kaydından ayrı. `MMOSaveGame.h` ve `FMMOCharacterSaveData` değişmiyor.

### 4.4 Yönetici komutları (yalnız sunucu süreci konsolu)

Komutlar `FSelfRegisteringExec` ile kaydedilir (`Private/Tests/MMOMailSelfTest.cpp:157-171` deseni). Komut, yazıldığı
süreçte çalışır. Uzak istemci kendi konsolunda yazarsa yalnız kendi makinesindeki `Saved/` klasörünü etkiler, sunucuyu
etkilemez.

- `MMOAccountClaim <hesap> <karakter>`: eski (sahipsiz) bir `.sav` adını bir hesaba bağlar. Hedef ad başka bir hesaba aitse ret.
- `MMOAccountUnlock <hesap>`: `fails` ve `lockUntil` alanlarını sıfırlar.
- `MMOAccountList`: hesapları ve adları listeler (özet ve tuz yazılmaz).

Parola konsoldan **girilmez**, çünkü konsol geçmişi ve log riski var. Parola sıfırlamanın tek yolu bugün host'un hesap
dosyasını elle silmesi. Bunu kalıcı bir akışla çözmek ileriye bırakıldı (§9).

### 4.5 Güvenlik

**Parola özeti seçenekleri (koddan doğrulananlar):**

| Seçenek | Kanıt | Artı | Eksi |
|---|---|---|---|
| **H1: PBKDF2-HMAC-SHA1, Core `FSHA1` üzerinde kendimiz** (öneri) | `Engine/Source/Runtime/Core/Public/Misc/SecureHash.h:313` `class FSHA1` (`Update`/`Final`/`GetHash`), `:392` `static void HMACBuffer(const void* Key, uint32 KeySize, const void* Data, uint64 DataSize, uint8* OutHash)` | Yeni bağımlılık yok, `Build.cs` değişmez. RFC 6070 test vektörleri var. HMAC-SHA1 PBKDF2 içinde hâlâ kabul görüyor (SHA-1 çakışmaları HMAC'i kırmıyor). | Kendi uygulamamız: test vektörleri şart. `HMACBuffer` her çağrıda anahtar dolgusunu yeniden hesaplıyor, bu yüzden iterasyon başına maliyeti yüksek (ölçülmeli). |
| H2: OpenSSL `PKCS5_PBKDF2_HMAC` (SHA-256) | Motor `Engine/Source/ThirdParty/OpenSSL/1.1.1t/include/Win64/VS2015/openssl/evp.h:1090` | Standart ve hızlı, aynı sürede daha çok iterasyon | `MyProject.Build.cs`'e üçüncü taraf bağımlılık gerekir (`AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL")`; **doğrulanmalı**). 1.1.1 serisi üst akışta destek dışı. |
| H3: `PlatformCrypto` eklentisi | `Engine/Plugins/Experimental/PlatformCrypto/.../EncryptionContextOpenSSL.h:228` `CreateRandomBytes`, `:256` `CalcSHA256` | Güvenli rastgele bayt, SHA-256 | PBKDF2 yok. Deneysel eklenti `.uproject`'te açılmalı. **Önerilmez.** |
| Windows `BCryptDeriveKeyPBKDF2` | (motor dışı) | Hızlı | Yalnız Windows. İleride Linux dedicated server hedefine uymaz. **Önerilmez.** |
| Tek tur tuzlu SHA | | | Hızlı özet, parola saklamaya uygun değil. **Reddedildi.** |

- **Tuz:** 16 bayt, `FGuid::NewGuid()` ile. Windows'ta `CoCreateGuid` (`WindowsPlatformMisc.cpp:1444`), Unix'te `getrandom`
  (`UnixPlatformMisc.cpp:1702-1727`). Tuzun gizli olması gerekmez, benzersiz olması yeter.
- **Kayıtta iterasyon sayısı tutulur** (`iter` alanı). Sayı ileride artırılırsa, başarılı girişte parola yeni sayıyla yeniden
  özetlenir. Göç gerekmez.
- **İterasyon `[PLACEHOLDER]`, ölçülecek.** Bu sürüm doğrulamayı oyun iş parçacığında, eşzamanlı `PreLogin` içinde yapar.
  Hedef, host CPU'sunda tek doğrulamanın en çok `[PLACEHOLDER 50 ms]` sürmesi. Bu süre OWASP'ın PBKDF2-HMAC-SHA1 önerisinin
  (yaklaşık 1,3 milyon iterasyon, 2023 rehberi; **doğrulanmalı**) çok altında kalır. Bu yalnız LAN kapsamında kabul edilebilir
  (bkz. "Kabul kapsamı"). İnternete açılırken doğrulama `PreLoginAsync`'e (`GameModeBase.h:303`, motor "OnComplete MUTLAKA
  çağrılmalı" diyor) ve iş parçacığı havuzuna taşınır, iterasyon artırılır. Bu, Adım 7 ile aynı kanca.
- **Karşılaştırma** sabit zamanlıdır: bütün baytlar XOR'lanıp OR'lanır, uzunluklar farklıysa sonuç false.
- **Parola biçimi:** `[PLACEHOLDER 8]`-64 karakter, yazdırılabilir ASCII. `?`, `=`, `#`, boşluk ve `"` yasak, çünkü URL
  seçenek ayrıştırıcısı (`ParseOption`) `?` ve `=` ile bölüyor. `#` işaretinin URL'de portal ayıracı olması **doğrulanmalı**.

**Kaba kuvvet sınırları (hepsi `[PLACEHOLDER]`):**
- IP başına: 5 dakikada 5 başarısız denemeden sonra o IP'nin istekleri özet hesaplanmadan reddedilir. Bellekteki tablo en çok
  1024 IP tutar, dolunca en eski kayıt atılır. LLM kotasındaki "tablo dolunca yeni anahtar reddi" davranışı burada
  **kullanılmaz**, çünkü saldırı altında herkesi dışarıda bırakırdı. Adres, `PreLogin`'in `Address` parametresinden alınır.
  Motorun `LowLevelGetRemoteAddress()` çağrısının port eklemeden yalnız IP verdiği **doğrulanmalı**.
- Hesap başına: art arda 10 başarısızlıktan sonra üstel kilit (1, 2, 4, ... en çok 15 dk), dosyada saklanır. Bunun bedeli
  şu: biri başkasının hesabını kilitleyebilir. LAN'da kabul edilir, `MMOAccountUnlock` ile açılır.
- Kayıt seli: IP başına saatte 3 yeni hesap, sunucu genelinde en çok 500 hesap.
- Eşzamanlı giriş: aynı hesaba ikinci giriş **reddedilir** (öneri). Çöken istemcinin bağlantısı motorun bağlantı zaman aşımına
  kadar açık kalır. Bu süre yaklaşık 60 sn, **doğrulanmalı**.

**Parolanın ağda düz gönderilmesi (TLS yok):**
- `NMT_Login` URL'i UE'nin şifresiz UDP ağ katmanında açık gidiyor. Aynı ağdaki biri (Wi-Fi, aynı LAN, VPN aktarıcısı)
  parolayı okuyabilir.
- Parola sunucu tarafında üç yerde görünür:
  - `World.cpp:7379` satırı onu sunucu log dosyasına yazar. `-MMOPassword` için aynı sorun `SECURITY_AUDIT.md` §8'de var.
  - Bağlantı ömrü boyunca `UNetConnection::RequestURL` içinde kalır. Başarılı girişten sonra bu alandan parola seçeneğini
    silmek ucuz bir sertleştirme. Etkisi ve yan etkisi **doğrulanmalı**.
  - İstemcinin kendi logunda da bağlantı URL'i görünebilir (**doğrulanmalı**).
- `-LogCmds="LogNet Warning"` ile logu kapatmak **önerilmez**: `NetSmokeTest` gibi testler `LogNet` "Join succeeded"
  satırına bakıyor.
- Motorda `AESGCMHandlerComponent` ve `DTLSHandlerComponent` paket işleyici eklentileri var
  (`Engine/Plugins/Runtime/PacketHandlers/`). Açmak `Config/` değişikliği ve anahtar değişimi ister. **Kapsam dışı,
  doğrulanmalı.**
- İstemci tarafında özet almak (pass-the-hash) bir şey kazandırmaz: özet parolanın yerine geçer.

**Kabul kapsamı:** yalnız kendi LAN'ın ya da şifreli bir VPN (Hamachi/ZeroTier/Tailscale; şifreleme iddiaları
**doğrulanmalı**), tanıdık oyuncular, sunucu loglarının paylaşılmaması. Oyunculara başka yerde kullandıkları parolayı
kullanmamaları söylenir.

**Kapsam dışında kalan açıklar:**
- Ağ dinleme.
- Host'un kötü niyetli olması: hesap dosyalarına ve kayıtlara tam erişimi var.
- Hesap dosyasının sızması halinde zayıf iterasyonla çevrimdışı kırma.
- Hesap kilitleme ile taciz.
- Parola sıfırlama yok.
- Halka açık internet. İnternete açılmadan önce en az `PreLoginAsync` + yüksek iterasyon + ağ şifrelemesi, ya da doğrudan C
  seçeneği gerekir.

**Kapanan açıklar:**
- K-1 senaryo 1 (ad değiştirerek ezme).
- K-1 senaryo 2 (`?Name=` ile çevrimdışı karakteri çalma).
- K-1 senaryo 3 (aynı yuvayla çift giriş).
- Posta altınının ad kapılarak alınması.
- Başarım ve günlük ilerlemesinin ele geçirilmesi.
- LLM kotası "ad değiştir, kotayı sıfırla" yolu (hesap modunda ad değişmiyor).

### 4.6 Göç ve geri uyumluluk

- **Bayrak kapalı (varsayılan):** her şey bugünkü gibi. Hesap dosyası okunmaz bile.
- **Bayrak açık:** var olan `.sav` adları **sahipsiz eski kayıt** sayılır. Hiçbir hesap bu adları kendiliğinden alamaz
  (§4.3 adım 1.6). Tek yol yöneticinin `MMOAccountClaim` komutu. "İlk gelen alır" türünde bir sahiplenme penceresi
  **tasarlanmadı**: bu makinedeki 724 kaydın hepsi `Gezgin`/`CIDIK` artığı (§2.4), böyle bir pencere yalnız yeni bir açık
  yaratırdı.
- Başarım, günlük ve posta dosyaları adla anahtarlı olduğu için sahiplendirilen karakter **dosya taşımadan** kendi
  ilerlemesini bulur.
- Dosya taşınmadığı ve silinmediği için geri dönüş tek adım: bayrak kaldırılır.

### 4.7 Ertelenen Adım 6 (B-full: `CharacterId`) için hazır tasarım

Ne zaman: ad değiştirme istendiğinde ya da PlayFab Aşama 2 başladığında.

- **Yuva biçimi:** `MMOSave_C_<24 hex>`. Sonek 26 karakter: en uzun oyuncu adı olan 20 karakteri geçtiği için bir oyuncu
  adıyla çakışamaz. `GetSaveSlotName`'in 32 karakter kesiminin de altında kalır. 96 bit rastgelelik yeterli.
  `ListSlotNames` (`MMOSave_*`) ve klan taraması değişmeden çalışır.
- **`names/` dosyası** `characterId` alanını kazanır. `GetSaveSlotName` bu alan doluysa kimliği döndürür. Başarım ve günlük
  yolları yuvadan türediği için kendiliğinden geçer. Posta adla adreslenmeye devam eder ve ad -> `CharacterId` çevirisini
  `names/` üzerinden yapar (`MMOMailComponent.cpp:78`'deki doğrudan `.sav` kontrolü de buraya taşınır).
- **Göç:** dosyalar kopyalanır, taşınmaz (`.sav`, başarım, günlük ve posta kutusu). İşlem tekrar çalıştırılabilir: hedef
  varsa atlanır, `names/` dosyasına `migrated` işareti yazılır. Eski dosyalar `[PLACEHOLDER 30 gün]` salt okunur tutulur.
  Geri dönüş: işaret silinir. Bu durumda eski ad dosyası bayatlamış olur, bu açıkça belgelenmeli.
- **Maliyet:** B-min'in yaklaşık 2-3 katı. Sıcak dosya `MMOCharacter.cpp` (`GetSaveSlotName`) değişir, posta bileşeni değişir,
  klan ve arkadaş listeleri hâlâ ad tutar. Güvenlik kazancı yok.

## 5. Dosya haritası ve Codex/ajan çakışma riski

**Yeni dosyalar (çakışma yok):**
- `Source/MyProject/Public/Account/MMOAccountStore.h`, `Private/Account/MMOAccountStore.cpp`: saf C++. Ad ve hesap
  normalizasyonu, hesap ve ad dosyaları, kilit alanları, `EvaluateLogin`, `AllowsRename`. `FMMOMailStore` desenini izler.
- `Public/Account/MMOPasswordHash.h`, `Private/Account/MMOPasswordHash.cpp`: PBKDF2-HMAC-SHA1 ve sabit zamanlı karşılaştırma.
- `Private/Account/MMOAccountExec.cpp`: `MMOAccountClaim/Unlock/List` (`FSelfRegisteringExec`).
- `Private/Tests/MMOAccountSelfTest.cpp`: `MMOTestAccounts` komutu.
- `Scripts/TestAccounts.ps1` (native self-test), `Scripts/TestAccountLogin.ps1` (listen server ve istemcilerle gerçek giriş).

**Dokunulacak mevcut dosyalar:**

| Dosya | Değişiklik | Risk |
|---|---|---|
| `Public/Core/MMOGameMode.h`, `Private/Core/MMOGameMode.cpp` | `PreLogin` genişler, `InitNewPlayer` override eklenir, `ChangeName`'de aday kontrolü genişler | Sıcak listede değil. Son commit'ler spawner ceplerinden. Aynı dosyada bölge geçişi kodu var, satırlar karışmaz. Düşük risk. |
| `Private/Characters/MMOCharacter.cpp` | `ServerSetName_Implementation` başına **tek çağrı** | **Sıcak.** `MMOCharacter.h` şu an çalışma ağacında değişmiş (commit'lenmemiş `IsTravelSaveCommitted`). Bu adım **en sona** bırakılır ve Codex/geliştirici commit'inden **sonra** ayrı, tek satırlık bir commit olarak yapılır. Adım 1-3 bu satır olmadan da test edilebilir. |
| `Scripts/StaticLint.py` | `OVERRIDE_WHITELIST`'e `InitNewPlayer` imzası: `APlayerController*`, `const FUniqueNetIdRepl&`, `const FString&`, `const FString&` (`GameModeBase.h:536`) | Lint uyarısı vermesin diye şart. Çakışma riski düşük. |
| `Scripts/RunSystemTests.ps1` | `TestAccounts.ps1` satırı | `RunAllChecks.ps1`'e **dokunulmaz** (sıcak ve şu an değişmiş). |
| `MMOMailComponent.cpp` (ileride) | Hesap modunda alıcı sahipli bir ad olmalı (eski sahipsiz kutulara altın gitmesin) | Şu an başka ajan düzenliyor. Bu iş onun commit'inden **sonra**, ayrı bir küçük adım. B-min'in güvenliği buna bağlı değil. |

**Dokunulmayacaklar:** `MMOSaveGame.h` (korunan), `MMOStatsComponent.*`, `MMOSkillComponent.*`, `MMOPlayerController.*`,
`RunAllChecks.ps1`, `PLAYTEST_CHECKLIST.md`, `Config/`, `Content/`, `.uproject`, `MyProject.Build.cs` (H1 seçilirse),
başarım, günlük ve posta bileşenleri.

## 6. Ekonomi ve denge etkisi

Altın ve XP girişine ya da çıkışına doğrudan etki yok. Dolaylı etkiler:
- Kopyalama vektörü kapanıyor (K-1 senaryo 3: aynı yuvayla çift giriş ve ticaret).
- Hırsızlık yolu kapanıyor (çevrimdışı karakterin altınını ticaret veya posta ile aktarmak).
- Posta göndermek artık hesap gerektiriyor.

Ölçüm ihtiyacı, `[MMOMETRIC]` deseninde ayrı bir log satırı (**parola, tuz ve özet asla yazılmaz**):

```
[MMOACCOUNT] ev=login_ok acc=<hesap> name=<ad>
[MMOACCOUNT] ev=login_fail reason=bad_pass|locked|ip_limit|name_owned|online|legacy_slot acc=<hesap>
[MMOACCOUNT] ev=register acc=<hesap>   ev=claim acc=<hesap> name=<ad>   ev=lock acc=<hesap> until=<unix>
```

Playtest'te bakılacaklar: başarısız giriş oranı (parola unutma yükü), kilitlenme sayısı, ölçülen doğrulama süresi
(`hash_ms=` alanı).

## 7. Adımlar, kabul ölçütleri ve test planı

### 7.1 Adımlar ("bitti" tanımları)

| Adım | İş | Bitti tanımı |
|---|---|---|
| 0 | `StaticLint.py` beyaz listesi (`InitNewPlayer`) | `python3 Scripts/StaticLint.py` override uyarısı vermiyor. `TestStaticLint.py` geçiyor. |
| 1 | `MMOPasswordHash` | `MMOTestAccounts`: RFC 6070 vektörleri ve özet vakaları PASS. Host'ta ölçülen `hash_ms` değeri belgeye yazıldı ve iterasyon buna göre seçildi. |
| 2 | `MMOAccountStore` (dosyalar, normalizasyon, kilit, `EvaluateLogin`, `AllowsRename`) | Self-test'in tüm depo vakaları PASS. §7.2'deki mutasyonların her biri FAIL üretiyor. |
| 3 | `MMOGameMode` entegrasyonu (`-MMOAccounts`) | `TestAccountLogin.ps1`'in 6 senaryosu PASS. Bayraksız `NetSmokeTest.ps1` ve `RunSystemTests.ps1` değişmeden GEÇTİ. |
| 4 | Yönetici komutları | Self-test claim/unlock vakaları PASS. Host konsolunda `MMOAccountClaim` ile eski bir `.sav` sahiplenilip o hesapla girilince karakter yükleniyor. |
| 5 | `ServerSetName` tek satır (sıcak dosya, en son) | Self-test `AllowsRename` PASS. `TestAccountLogin` 7. senaryo: hesap modunda uzak `MMOSetName` reddediliyor. |
| 6 | (ertelendi) B-full `CharacterId` | §4.7 |
| 7 | (ertelendi) `PreLoginAsync` + yüksek iterasyon, sonra PlayFab doğrulayıcısı | `ACCOUNT_BACKEND_COMPARISON.md` Aşama 2 |

Yalnız "ileride PlayFab'a takılsın" diye bugün bir doğrulayıcı arayüzü yazılmaz (YAGNI). `EvaluateLogin` saf bir fonksiyon
olduğu için o gün değiştirmek küçük bir iş olur.

### 7.2 Native self-test (`MMOTestAccounts`, editör kapalı, `-ExecCmds="MMOTestAccounts,quit"`)

Kontroller:
1. PBKDF2-HMAC-SHA1 RFC 6070 vektörleri, P=`password`, S=`salt`, dkLen=20. Değerleri uygulama sırasında RFC metninden teyit et:
   - c=1 -> `0c60c80f961f0e71f3a9b524af6012062fe037a6`
   - c=2 -> `ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957`
   - c=4096 -> `4b007901b765489abead49d926f721d065a429c1`
2. Özet ve doğrulama gidiş-dönüşü. Yanlış parola false. Aynı parola ve farklı tuz farklı özet verir.
3. Sabit zamanlı karşılaştırma: tek bayt farkı ve uzunluk farkı false döner.
4. Hesap adı normalizasyonu: büyük/küçük harf, geçersiz karakter, 2 ve 21 karakter sınırları.
5. Parola biçimi: kısa, uzun, `?`, `=`, `#` reddedilir.
6. JSON gidiş-dönüşü. Düşmanca JSON reddedilir: özet eksik, `iter` 0 ya da tavan üstü, tuz yanlış uzunlukta.
   **Bozuk dosyada giriş reddedilir ve yeniden kayıt açılmaz.**
7. Ad sahipliği: "Ali Veli" A hesabındayken B hesabı "ali_veli" adını alamaz. Diskte `.sav` olan sahipsiz ad yeni karakter
   olarak alınamaz. `Claim` sonrası A alır. Hesap başına karakter tavanı çalışır.
8. Yuva anahtarı ile gerçek pawn'ın `GetSaveSlotName()` sonucu eşit. Fikstür deseni `MMOCharacter.cpp:113-120`.
9. IP sınırı: N başarısızlıktan sonra (N+1). deneme **özet sayacını artırmadan** reddedilir. Pencere dolunca yeniden denenebilir.
10. Hesap kilidi dosyada kalıcı. Başarılı giriş `fails` alanını sıfırlar.
11. `EvaluateLogin`: bayrak kapalıyken boş hata döner (geri uyum). Hesap yok, yanlış parola, başka hesabın adı ve çevrimiçi
    ad durumları hata verir.
12. `AllowsRename`: hesap modunda uzak istemciye false. Host sahipli ada false, sahipsiz ada true. Bayrak kapalıyken true.
13. `Gezgin######` adayı hiçbir zaman sahipli bir ad olmuyor.

**Mutasyonla sınanacak korumalar.** Her biri geçici olarak bozulup derlenir, test FAIL vermeli, sonra geri alınır. CLAUDE.md'deki
"mutasyondan sonra touch" notu geçerli.
- Özet girdisinden tuzu çıkar -> kontrol 2.
- Karşılaştırmayı yalnız ilk bayta indir -> kontrol 3.
- Kilit kontrolünü atla -> kontrol 10.
- Bozuk hesap dosyasını "yok" say -> kontrol 6.
- Ad anahtarında küçük harfe çevirmeyi kaldır -> kontrol 7 ve 8.
- Eski `.sav` sahiplik kontrolünü kaldır -> kontrol 7.
- IP sınırını özetten sonra uygula -> kontrol 9.
- Bayrağın varsayılanını açık yap -> kontrol 11.

### 7.3 Gerçek giriş testi (`Scripts/TestAccountLogin.ps1`)

Headless listen server `-MMOAccounts` ile ve ardışık istemcilerle çalışır, `NetSmokeTest.ps1` desenini izler:
1. Yeni hesap ve yeni ad: giriş başarılı, `ev=register` ve `ev=login_ok` satırları var.
2. Aynı hesap ve yanlış parola: giriş reddedildi, `reason=bad_pass`.
3. Başka hesap, birinci hesabın adıyla: `reason=name_owned`.
4. Birinci hesap ikinci istemciden aynı anda: `reason=online`.
5. Eski `MMOSave_<ad>.sav` adıyla yeni hesap: `reason=legacy_slot`.
6. N+1 hatalı deneme: `reason=ip_limit` ve özet süresi loglanmadı.
7. (Adım 5'ten sonra) uzak `MMOSetName`: reddedildi.

Her senaryoda log taranır: `[MMOACCOUNT]` satırlarında parola metni **geçmemeli**. Motorun `LogNet Login request` satırındaki
sızıntı bilinen ve belgelenmiş bir durum, test bunu yalnız raporlar. Bayraksız `NetSmokeTest.ps1` regresyonu ayrıca koşulur.

### 7.4 Oyun içi ilk test listesi (standalone; PIE istemcisine URL seçeneği vermek doğrulanmadı)

Komut deseni `NETWORK_AUDIT.md:485`:
`UnrealEditor.exe <proje> 127.0.0.1?Name=..?Account=..?AccountPassword=.. -game`

1. Host `-MMOAccounts` ile açılır. İstemci yeni hesapla girer, sınıf seçer, seviye atlar, çıkar. Aynı hesapla tekrar girince
   seviye, eşya, başarım ve günlük görev yerinde.
2. Yanlış parolada istemci ekranında anlaşılır bir ret metni çıkıyor.
3. Sahiplendirilen eski karakterin altını, eşyası ve başarımları geliyor.
4. A, çevrimdışı B'ye posta ve altın gönderiyor. B girince alıyor. Üçüncü biri B'nin adıyla giremiyor.
5. Summer -> Winter bölge geçişinden sonra ad ve yuva aynı kalıyor, tekrar kayıt yerinde. PIE'de seamless travel kapalıysa
   istemcilerin yeniden bağlanırken yeniden doğrulanması **doğrulanmalı**.
6. İstemci süreci öldürülüp hemen yeniden giriliyor: zaman aşımı dolana kadar "çevrimiçi" reddi ve süresi not edilir.
7. Hesap modunda `MMOSetName` reddediliyor. Host sahipli bir ada geçemiyor.
8. Bayraksız oturum: bugünkü davranış aynen sürüyor (`?Name=` ASCII temizleme, `Gezgin######`, `MMOSetName` + §10 ad kilidi).

## 8. Karar seçenekleri

| | A) Yalnız `-MMOPassword`'u sertleştir | B) Yerel hesap dosyası (B-min, bu belge) | C) Doğrudan PlayFab/OWS |
|---|---|---|---|
| İş | Parolayı ortam değişkeninden oku, sabit zamanlı karşılaştır, IP deneme sınırı ekle. ~1 küçük adım. | Adım 0-5: 2 yeni modül, 1 self-test, 1 ağ testi. `MMOGameMode` ve bir satırlık sıcak dosya değişikliği. | Dedicated server için kaynaktan motor derlemesi, istemci giriş UI'ı, HTTP, gizli anahtar yönetimi, çevrimiçi hesap. Haftalar. |
| Çözdüğü | Tanımadık kişileri dışarıda tutar (bugün de tutuyor). Ortak parolanın denemeyle bulunmasını yavaşlatır. | K-1'in üç senaryosu, posta, başarım ve günlük devralma, çift giriş. Tanıdıklar arasında da ad kapmayı kapatır. | B'nin hepsi, ayrıca gerçek parola güvenliği, ağ şifrelemesi (HTTPS), sıfırlama, bulut kayıt. |
| Çözmediği | **K-1'i hiç kapatmaz.** Ortak parolayı bilen herkes herhangi bir ada girer. Log sızıntısı (`World.cpp:7379`) sürer. | Ağ dinleme, host güveni, zayıf iterasyon, internete açık kullanım, parola sıfırlama, ad değiştirme. | Sunucu-sunucu karakter kilidi gibi kendi işlerimiz yine bize kalır (`ACCOUNT_BACKEND_COMPARISON.md` §3.10). |
| Risk | Yanlış güven hissi | Kendi kripto uygulamamız (test vektörleriyle sınırlanır). Kilitleme ile taciz. Hesap modunda ad değiştirme kapalı. | Motor kaynak derlemesi, satıcı kota ve fiyat değişikliği, KVKK. Bugün yapılamıyor. |
| Değer | Tek başına **değmez**. IP sınırı ve sabit zamanlı karşılaştırma zaten B'nin içinde. Ortam değişkeni okuma tek satır, B ile birlikte yapılabilir. | Yüksek. Bulut gelince de boşa gitmez: `names/` dosyası ad rezervasyonu olarak, `EvaluateLogin` ise doğrulayıcı değişim noktası olarak kalır. | Hedef mimari, ama şimdilik ulaşılamaz. |

**Önerim: B (B-min, Adım 0-5).** A'nın işe yarayan iki parçası B'ye dahil. C, `ACCOUNT_BACKEND_COMPARISON.md`'deki plana göre
dedicated server mümkün olunca, Adım 7 üzerinden gelir. B-full (Adım 6) yalnız ad değiştirme istenirse ya da Aşama 2
başlarsa yapılır.

## 9. Riskler ve açık sorular

**Riskler**
- Kendi yazdığımız PBKDF2'de hata olması. Azaltma: RFC 6070 vektörleri ve mutasyon testleri.
- Eşzamanlı doğrulamanın oyun iş parçacığını dondurması. Azaltma: ölçülmüş iterasyon, IP sınırının özetten önce uygulanması,
  internete açılmadan önce `PreLoginAsync`.
- İki host sürecinin aynı `Saved/` klasörünü kullanması. Ad sahipliği tek süreç varsayıyor (oyun iş parçacığında
  kontrol-sonra-yaz). LLM sayaç dosyasıyla aynı sınır (`NETWORK_AUDIT.md:460`).
- Diskteki 724 artık kayıt, hesap modunda sahipsiz kalır. Klan taraması (`MMOClanComponent.cpp:373-383`) hâlâ hepsini yüklüyor.
  Bu ayrı bir iş, bu tasarım değiştirmiyor.
- Parola sıfırlama akışı yok. Unutulan parola host'un elle müdahalesini gerektirir.

**Doğrulanmalı (uygulama sırasında)**
- `#` karakterinin URL ayrıştırmasındaki davranışı.
- `LowLevelGetRemoteAddress()` çağrısının portsuz IP döndürmesi.
- `RequestURL` alanından parolanın silinmesinin yan etkisi.
- İstemci logunda URL'in görünüp görünmediği.
- Bağlantı zaman aşımı süresi.
- PIE'de bölge geçişinde yeniden doğrulama.
- OpenSSL bağımlılık satırı (H2 seçilirse).

**Kullanıcının vermesi gereken kararlar (3)**
1. **Hesap nasıl açılsın?** (a) İlk girişte otomatik: LAN'da kolay, IP ve sunucu tavanı ile sınırlı. (b) Yalnız host açsın:
   güvenli ama her oyuncu için host'un elle işlem yapması gerekir. Önerim (a).
2. **Test oturumları internete veya yabancılara açılacak mı, ne zaman?** "Hayır, LAN veya VPN ve tanıdıklar" ise B yeterli.
   "Evet" ise B'den önce Adım 7 (asenkron doğrulama, yüksek iterasyon) ve ağ şifrelemesi gerekir, ya da C beklenir.
3. **Hesap modunda ad değiştirme kapalı kalabilir mi?** "Evet" ise B-full ertelenir (öneri). "Hayır, oyuncu adını
   değiştirebilmeli" ise B-full (`CharacterId`, §4.7) bu işin parçası olur ve maliyet yaklaşık 2-3 katına çıkar.

## 10. Kisa vadeli ara onlem (2026-10-11, hesap modundan once)

Durum: **uygulandi (2026-10-11)**: commit `afba349` (tur 2: F1-F3/F5), `2bb2023` (tur 3: ad kilidi sogumasi her disk
kontrolunden once, F1 reddi de soguma baslatir, yan dosya temizligi), `d031b90` (tur 4: ASCII ad kurali + kanonik cakisma
anahtari `Public/Core/MMONameRules.h`, guvenli eski yuva silme, kilitten sonra kilit metni). Native test `Scripts/TestNameLock.ps1`
**58 kontrol GECTI** (`7932979`); PIE ve gercek uzak istemci YOK. F4 / §10.3 (Linux harf varyanti) hala ACIK. Ayrinti `SECURITY_AUDIT.md` K-1 yeni yuz "Durum" satirlari. Kaynak: `SECURITY_AUDIT.md` K-1 "yeni yuz (2026-10-11): Ad degistirerek
karakter catallama", bulgular F1-F5 (satirlar commit `43e87ca`). Karar adayi: `DECISIONS_NEEDED.md` "Ad degistirme kisiti
(uzak istemci)".

### 10.1 Neden B-min'i beklemeden

B-min (§4) bayrak arkasinda ve Adim 0-5 is ister. O gelene kadar bugunku bayraksiz modda tek bir uzak istemci, kayitli
karakterinin adini yeni bir ada degistirip cikarak karakteri iki yuvaya kopyalayabiliyor (F1: `MMOCharacter.cpp:1631-1640`
yeni ada izin veriyor, `:952-957` cikista yeni yuvaya tam envanterle yaziyor, `:1243-1252` K-1 korumasi yalniz dolu yuvayi
koruyor, eski yuva silinmiyor). Posta/ticaret/depo ile altin her turda ikiye katlanir. Bu, §4.5'teki "kapanan aciklar"
listesinde olmayan, ad degistirmenin kendisinden dogan bir yol: hesap modunda ad degismedigi icin B-min onu da kapatir, ama
bayrak kapaliyken acik kalir.

### 10.2 Onlem: uzak istemci yalniz hic kaydedilmemis karakterin adini degistirebilir

- `ServerSetName_Implementation` (`MMOCharacter.cpp:1576`, kontrol `:1631-1640`'in yerine): `!IsLocallyControlled()` ise ve `ActiveSaveSlot` yuvasi
  diskte VARSA ret ("Kayitli karakterin adi degistirilemez"). Kontrol `Store->Exists(ActiveSaveSlot)` olmali; yeni karakter
  dali (`:1394`) dosya yokken de `ActiveSaveSlot`'u atadigi icin `IsEmpty()` yetmez.
- Yeni karakter ilk otomatik kayda (60 sn, `:933`) ya da ilk `MMOSave`'e kadar adini secebilir; sonra ad kilitlenir.
- Host (yerel denetleyici) ve yonetici muaf: self-test'ler, NetSmokeTest ve host test akisi bozulmaz. Host diske zaten tam
  erisimli oldugu icin bu bir guvenlik siniri degil (§4.3 madde 3 ile ayni gerekce).
- Kayitli-ad reddi de soguma damgasini vursun (F5: bugun `:1635-1639` damgasiz donuyor, cevrimdisi adlar sinirsiz
  sorgulanabiliyor).
- Bu onlem §4.3 madde 5'in (hesap modunda uzak `ServerSetName` her zaman ret) bayraksiz ve daha gevsek hali. B-min gelince
  `MMOAccount::AllowsRename` cagrisi bu kosulun yerine gecer; ikisi cakismaz.
- Etkisi: kayitli bir karakterin adi uzaktan degistirilemez. Bugun diskte elle secilmis karakter adi yok (§2.4: 724 dosyanin
  hepsi `Gezgin######` / `CIDIK-<hex>`), yani mevcut oyuncu verisine pratik etkisi yok. Oyuncu `Gezgin######` adiyla
  kaydedildiyse adini degistiremez; yeni karakterle (yeni `?Name=`) baslamasi gerekir. Kabul edilip edilmeyecegi kullanici
  karari.
- **Uygulama (guncel satirlar, `d031b90` sonrasi; yukaridaki maddeler oneri metnidir):** `ServerSetName_Implementation`
  `MMOCharacter.cpp:1589`; soguma (uzak 30 sn, yerel 5 sn) her disk kontrolunden once `:1627-1647`; ad kilidi
  (`Store->Exists(ActiveSaveSlot)`, magaza yoksa da ret) `:1648-1663`, ret `bNameLockRejected` koyar ve soguma suresince
  sonraki denemeler "X sn bekle" yerine kilit metnini gorur; kayitli-ad reddi soguma damgasi vurur `:1682-1698`. Kaydi
  olmayan yeni karakter ayni yuvayla yeniden possess edilirse kayit kapisi yeniden acilir (Y-NEW-6, `LoadCharacter` `:1364-1373`).

### 10.3 Linux harf varyanti notu

- Bugun yuva adi harf buyuklugunu koruyor (`GetSaveSlotName`, `MMOCharacter.cpp:1114-1131`, `MMONameRules::SlotKey`) ve
  `Exists` dosya sisteminin kuralina uyuyor (`MMOLocalFileCharacterStore.cpp:32`, `DoesSaveGameExist`). Windows'ta `MMOSave_Ali`
  ve `MMOSave_ALI` ayni dosya; harf duyarli bir dosya sisteminde (ileride Linux sunucu) iki ayri karakter olur (F4). Posta
  kutusu ise zaten kucuk harfle anahtarli (`MMOMailStore.cpp:108` `GetBoxPath`), yani Linux'ta `ALI` adini alan biri `Ali`'nin
  kutusunu toplayabilir [Muhtemel, kod okumasi; Linux'ta denenmedi].
- **Tur 4 (`d031b90`) etkisi:** adlar artik yalniz ASCII oldugu icin Windows/NTFS'teki Unicode harf varyanti (buyuk/kucuk
  O-umlautlu iki "farkli" ad ayni `.sav`'a yaziliyordu, Y-NEW-1) KAPANDI. Cevrimici cakisma kontrolleri (PreLogin, ChangeName,
  ServerSetName) kanonik anahtarla (ASCII kucuk harf) yapiliyor, yani `Ali` cevrimiciyken `ALI` giremez. Ama yuva harf
  buyuklugunu korudugu ve `Exists` dosya sistemine uydugu icin **Linux'ta cevrimdisi `Ali`'nin `ALI` varyanti hala ayri yuva
  acar**: F4 ACIK.
- §4.2'deki `names/` anahtari zaten kucuk harf, ama `.sav` yuvasi oyle degil. Yuva adini kanonik kucuk harfe cevirmek icin
  **tek seferlik goc** gerekir: mevcut karisik harfli `MMOSave_*.sav` dosyalari kucuk harfli ada kopyalanir (tasinmaz, §4.6
  ve §4.7 ilkesi), ayni kucuk harf adina iki dosya dusuyorsa (Linux'ta `Ali` ve `ALI`) otomatik birlestirilmez, yoneticiye
  raporlanir. Basarim (`Saved/MMOAchievements/<yuva>.json`) ve gunluk (`Saved/MMODaily/<yuva>.json`) dosyalari da ayni
  gocle kopyalanmali. Windows'ta goc pratikte yalniz dosya adi harflerini degistirir.
- Goc yapilana kadar ara yol: `Exists`'i harf duyarsiz yapmak (`ListSlotNames` + `IgnoreCase`); goc gerektirmez ama Linux'ta
  zaten var olan iki varyanti birlestirmez.
- §7.2 kontrol 8 (yuva anahtari = `GetSaveSlotName()`) kucuk harf goc yapilmazsa yalniz harf duyarsiz karsilastirmayla gecer;
  uygulama sirasinda bu secim acikca yazilmali.

### 10.4 Basarim/gunluk/posta dosyalarinin kimlige baglanmasi (bulgularin aciklamasi)

§4.1 "dosyalar ada gore anahtarli kalabilir, ad sahipli ve degismez olunca yeterli" diyor. 2026-10-11 bulgulari bunun
**ad degistirilebildigi surece** tutmadigini gosteriyor:
- Basarim ve gunluk bilesenleri `43e87ca`'da canli ada degil `GetActiveSaveSlot()`'a bagli (`MMOAchievementComponent.cpp:232-244`,
  `MMODailyComponent.cpp:199-211` ayni desen; §2.3 tablosundaki `<GetSaveSlotName>.json` notu duzenleme sirasinda yazilmisti, guncel
  baglama budur). Ad degisip karakter yeni yuvaya yazilinca dosyanin yeni yola tasinmasi yalniz `TryLoad` icinde
  (`MMOAchievementComponent.cpp:278-290`, `MMODailyComponent.cpp:245-258`). Bilesen `EndPlay`'i (`:99-110`, `:80-91`)
  `TryLoad` cagirmadigi icin ad degistirip hemen cikan oyuncunun yan dosyasi eski adda kalir, yeni yuvada bos baslar ve
  seviye/gorev basarimlari hemen yeniden verilir (F2: Lv80 + 90 gorev icin ~25.900 altin/dongu [Hesap]).
- `MMOLoad` "kayit yok" dali (`MMOCharacter.cpp:1382-1397`) yuklu karakterde `ActiveSaveSlot`'u degistirip `LoadSerial`
  artisiyla basarim/gunlugu sifirliyor (F3; bugun yalniz host/yonetici yolu).
- Posta canli PlayerState adina bagli (`MMOMailComponent.cpp:40-54`), yuvaya degil; B2 korumasi (`:155-163`) yalniz ad
  degisip yuva yazilmamis aradaki pencereyi kapatiyor.
- Ders: yan dosyalari "karakterle birlikte tasimak" yerine **degismeyen bir kimlige** baglamak gerekir. B-min'de bu kimlik
  sahipli ve degismez ad (yeterli, cunku ad degismez); ad degistirme istenirse B-full `CharacterId` (§4.7) zorunlu olur ve
  basarim/gunluk/posta yollari ad yerine `CharacterId`'den turetilir. Bu durumda ad degisimi yalniz `names/` dosyasini
  gunceller, hicbir oyun dosyasi tasinmaz; F1-F3 tasarim geregi olusamaz.
- Ara onlem (§10.2) bu sorunu uzak istemci icin yan dosyalara dokunmadan kapatir, cunku kayitli karakterin adi degismez.
  Savunma derinligi icin bilesen `EndPlay`'inde tasima adimi ayri bir kucuk duzeltme olarak onerildi (`SECURITY_AUDIT.md` K-1
  yeni yuz).
- **Durum (2026-10-11, tur 2-4; native test GECTI, PIE YOK):** yukaridaki maddeler `43e87ca` icindir. Simdi: bilesen `EndPlay`'i
  yuklu ise `SaveNow`'dan once `TryLoad()` cagirir (F2, `MMOAchievementComponent.cpp:155`); yuva tasimasi yeni yola basariyla
  yazildiktan SONRA eski yuvanin `.json` ve `.json.tmp` dosyalari silinir (Y7), ama eski yol yeni yolla kanonik olarak ayni
  dosyaysa ya da eski yuva su an cevrimici baska bir oyuncunun yuvasiysa silinmez (Y-NEW-3/4, `AchDeleteOldSlotFiles`
  `:66-88`, gunlukte `DailyDeleteOldSlotFiles`); bozuk dosyanin eski `.tmp`'si `.tmp.bad` olur (Y6); `.tmp` kurtarma Move'u
  tekrar denemesiz, basarisizsa yukleme 10 sn ertelenir (tur 4, eskiden 60 sn). F3 dali kapali (`LoadCharacter` `:1359-1376`).
  Posta hala canli ada bagli; kutu yolu ASCII kucuk harf `Saved/MMOMail/MMOMail_<ad>.json`. Testler: basarim 96, gunluk 81,
  posta 130, ad kilidi 58 kontrol (`7932979`).
