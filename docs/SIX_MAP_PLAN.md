# Altı harita ve ırk planı (kapalı alfa)

**Tarih:** 2026-10-12, **revizyon 2026-10-13** (kullanıcı kararları S1/K1/K4/K5/S5 işlendi), **revizyon 2 2026-10-13** (Steam yayını: S12/S13/S17/S18 önerilen kararları, yeni Aşama A5, §12-§13)
· **Durum:** tasarım `[PROPOSED]`, bu plan için kod yok
· **Hazırlayan:** oyun-tasarimcisi ajanı · **Okuyucu:** yerelde çalışan ajanlar (Claude/Codex) ve kullanıcı

## Kullanıcı kararları (2026-10-13)

| # | Karar | Önceki öneri (2026-10-12) | Sonuç (bu plana etkisi) |
|---|---|---|---|
| **S1** | **B: iki ırk aynı anda farklı haritalarda olabilir.** "Oturum ırkı" kuralı **reddedildi** | Oturum ırkı kuralı (tek listen server, herkes birlikte taşınır) | Her harita ayrı bir dedicated sunucu süreci olur. Karakterler paylaşılan PostgreSQL deposunda durur. Geçiş oyuncu başına yapılır (istemci başka sunucuya bağlanır). Bu iş kaynak motor, hesap/kimlik (K-1) ve DB katmanı ister. **Alfa ~12-16 haftadan ~6-10 aya uzar** [Tahmin] (§9.4; revizyon 2 Steam aşamasıyla **~6,5-11 ay**) |
| **K1** | PvP haritasına (H6) giriş **Lv55** | Lv35 | H6 krallık haritasının (Lv25-59) son bölümündeki ve üst bölgedeki oyuncuları buluşturur. PvE bandı Lv55-72'ye kayar (§6 K2) |
| **K4** | PvP'de ölünce **XP kaybı var** | Yok | Oyuncu öldürünce kayıp, mob cezasından küçük olur: `ExperienceToNextLevel`'ın **%3'ü** `[PLACEHOLDER]` (mob: %10, `MMOStatsComponent.h:197`). Seviye düşmez (bugünkü formül zaten `Min(Experience, ...)`). `ApplyDeathPenalty` öldürenin türüne göre ayrılır (§6 K4) |
| **K5** | Ölünce **eşya da altın da düşmez** | "Hiçbir şey" | Önceki öneriyle aynı. Kod işi yok. Yalnız bir test: PvP ölümünde çanta, kuşanılan eşya ve altın değişmez |
| **S5** | **Pazar alfada şart**, ertelenemez | Erteleme seçeneği vardı | Aşama 8 (pazar) alfa kapısının parçası. "Aşama 7 sonrası pazarsız alfa" seçeneği kaldırıldı. Pazar artık PostgreSQL üzerinde çalışır (§3.3) |
| — | Çalışma **yerelde** (kullanıcının makinesi) | Bulut / yerel / editör üçlüsü | §9'da her aşama "Yerel ajan" (Claude/Codex: kod, derleme, headless test) ve "Kullanıcı" (motor kurulumu, editör, PIE, DB kurulumu) diye ayrılır. Derleme yapan ajan aynı anda tek olur (CLAUDE.md) |

**Steam yayını kararları (kullanıcı, 2026-10-13, "işle" dendi).** Aşağıdakiler **önerilen karar** olarak kaydedildi ve plan bunlara göre yazıldı.
Kullanıcı aksini söylemedikçe kabul edilmiş sayılır. Gerekçeler §8'de, Steam ayrıntısı §12'de, kaynaklar §13'te.

| # | Önerilen karar | Önceki durum | Sonuç (bu plana etkisi) |
|---|---|---|---|
| **S12** | **Veri deposu PostgreSQL. Kimlik = SteamID.** Steam sürümü için parola / e-posta / parola sıfırlama hesap sistemi **yazılmaz** | PostgreSQL (Sprint 1) ile PlayFab arasında açık soru; kimlik `ACCOUNT_IDENTITY_DESIGN.md` B-min (ad + parola) | Hesap tablosu `external_id` mantığıyla kurulur: `(provider, external_id)` tekil, `provider='steam'`, `external_id` = 64-bit SteamID; ileride başka mağaza yeni `provider` değeriyle eklenir. K-1 (ad = kimlik) karakter SteamID hesabına bağlanınca kapanır. A3 kısalır (§9.2) |
| **S13** | **(a) ara servis kesinleşti.** Dili hâlâ kullanıcı kararı (varsayılan Python [Tahmin]) | Öneri (a), karar bekliyordu | Steam bilet doğrulaması Web API Publisher Key ister. Bu anahtar gizlidir: 6 harita sunucusuna dağıtılmaz, istemciye girmez, yalnız servis tutar. (b) libpq seçeneği kapandı |
| **S17** | **Alfa: kullanıcının makinesi + Tailscale/ZeroTier ya da Steam Playtest.** Herkese açık Steam sürümü için kiralık 7/24 sunucu **zorunlu**, bu planın **kapsamı dışında** (ayrı iş) | Açık karar | Steam relay ağı (`bAllowP2PPacketRelay`, Steam Sockets) sunucu adresini gizleyip DDoS'a karşı yalıtım sağlar (Unreal Steam Sockets belgesi). UE5.6 için "Steam Networking relay'siz bozuk" forum bildirimi var; **5.7 durumu [Doğrulanmadı]**, A1 (ölçüm) ve A5/A4 (kabul) içinde denenir |
| **S18** (yeni) | **Steam-first:** A3'te doğrudan Steam bileti kullanılır; önce parola sistemi yazıp sonra Steam'e geçilmez | — | Sonradan geçiş, hesap/kimlik tablosunu ve giriş akışını yeniden yazmak demekti (sahte iş). Yeni Aşama **A5 Steam entegrasyonu** A3'ten önce gelir (§9) |

**Bu kararlarla ne değişti, kısaca:** §1.4 (mimari), yeni §1.5 (Sprint 1 altyapısının durumu), §2.6 (kapılar oyuncu başına), §3.1 (ırk DB'de),
§3.3 (pazar DB ile), yeni §4.3 (harita başına sunucu için kod noktaları), §6 (K1, K2, K4, K5), §7, §8, §9 (dört yeni altyapı aşaması
A1-A4 ve yeni sıra), §10, yeni §11 (yerelde ilk adımlar).
**Revizyon 2 (Steam):** §1.2 (Steam satırı), §1.4 (giriş servisi = Steam bilet doğrulaması), §1.5 (V2 `accounts`), §3.1 I5, §4.3 D2/D4/D6 ve yeni D12,
§5 (kimlik testi), §7, §8 (S12, S13, S17 güncellendi; yeni S18, S19; R10, yeni R12-R14), §9 (yeni A5, A3 kısaldı, A1/A4/9 eklemeleri, süre yeniden hesap),
§11, yeni §12 (Steam yayını: kapsam içi/dışı, Playtest, mağaza işleri), yeni §13 (kaynaklar).

---

**Gereksinim (kullanıcı, 2026-10-12, uygulanacak kapsam):**

| # | Harita | Kim girer | Seviye | İçerik |
|---|---|---|---|---|
| H1 | Ortak merkez (KO Moradon / Rise Dorion) | İnsan + Ork | 1-25 | Kasma, görev, **pazar kurma**, eşya alış-satışı |
| H2 | İnsan krallığı (KO El Morad / Rise Lunaskar) | Yalnız İnsan | 25-59 | Kasma, görev |
| H3 | Ork krallığı (KO Luferson / Rise Protean) | Yalnız Ork | 25-59 | Kasma, görev |
| H4 | İnsan üst bölge (KO Eslant / Rise Haddar-Hagard) | Yalnız İnsan | 59-80 | Kasma, görev |
| H5 | Ork üst bölge | Yalnız Ork | 59-80 | Kasma, görev |
| H6 | Ortak PvP | İnsan + Ork | **55+** (K1) | PvP + PvE |

Her haritada NPC ve mob zorunlu.

Etiketler: **[Kesin]** = koddan/dosyadan okundu (dosya:satır) · **[Muhtemel]** = koddan çıkarım, oyunda doğrulanmadı · **[Tahmin]** = öngörü ·
**[Doğrulanmadı]** = derlenip/çalıştırılıp görülmedi. `[PLACEHOLDER]` sayılar ölçümden önce değişir.

---

## 0. Oyuncu deneyimi ve neden şimdi

Oyuncu karakterini yaratırken İnsan ya da Ork seçer. İlk 25 seviyeyi iki ırkın paylaştığı merkez kasabada geçirir: kasar, görev yapar,
pazar kurar, ticaret yapar. Lv25'te kendi ırkının krallığına geçer, Lv59'da ırkının üst bölgesine çıkar. Karşı ırkla yalnız
merkezde (barış) ve Lv55'ten sonra PvP haritasında (savaş) karşılaşır. İki krallık **aynı anda yaşar**: bir İnsan Ravenmoor'da kasarken
bir Ork kendi yurdunda kasar (S1 = B). Bu, Knight Online'ın iki ulus döngüsüdür ve oyunun kimliğini taşır.
Neden şimdi: kapalı alfanın kapsamı bu. Irk, kayıt, geçiş, PvP ve sosyal sistemlerin hepsine dokunuyor. Alfa öncesinde
tasarlanmazsa her sistem ikinci kez açılır.

---

## 1. Mevcut durum (kod gerçeği)

### 1.1 Haritalar [Kesin]

| Harita | Seviye | Spawner | NPC | Kaynak |
|---|---|---:|---|---|
| `Summer_scene` | Lv1-40 + Lv58-65 dış cepler | **18** (Lv1-24: 11, Lv25-30: 2, Lv31-40: 2, Lv58-65: 3) | 5: satıcı, görevli, konuşan (Kael), şifacı, depocu | `MMOGameMode.cpp:1094-1434` |
| `Winter_scene` | Lv42-57 + Lv66-80 | **17** (Lv42-57: 8, Lv66-80: 9) | 4: satıcı, görevli, şifacı, depocu | `MMOGameMode.cpp:972-1093` |
| `Lvl_ThirdPerson`, `Overview` | — | 0 | 0 | `SpawnMobArea` uyarı yazıp çıkar (`:1097-1101`) |

- NPC sınıfları haritadan bağımsız: satıcı tüm stoğu satar, görevli tüm görevleri gösterir (`MMOVendorNPC.h`, `MMOQuestGiverNPC.h`).
  Yerleşim PlayerStart'a göre sabit ofsetle yapılır (`MMOGameMode.cpp:1395-1434`). İlk bulunan PlayerStart kullanılır (`:956-962`).
- İçerik tek doğrusal zincir (`CONTENT_MAP.md`): **59 mob (13 elit), 97 görev**. Bantlara bölünce (CONTENT_MAP satırlarından sayıldı):
  Lv1-24 = 19 normal + 6 elit + 37 görev, Lv25-58 = 17 + 4 + 35, Lv59-80 = 10 + 3 + 25.

### 1.2 Bu kapsam için **olmayan** sistemler [Kesin, grep]

| Sistem | Bugün | Kanıt |
|---|---|---|
| Irk / ulus | **Yok.** `Source`'ta Race/Faction/Nation kavramı yok | — |
| Karakter yaratma ekranı | Yok. Ad `?Name=` ile gelir, sınıf oyun içinde ilk girişte seçilir (`bClassChoicePending`, `MMOCharacter.h:136`; HUD paneli `ClassChoiceRegions`) | `MMOGameMode.cpp:445-619` |
| Pazar kurma (oyuncu tezgâhı) | **Yok.** Alış-satış = NPC satıcı + oyuncular arası takas (`UMMOTradeComponent`). V1 şemasında `auction_house` tablosu var ama kod onu kullanmıyor (§1.5) | grep `stall/merchant/pazar` boş |
| Harita başına PvP | Yok. PvP **sunucu genelinde tek anahtar**: `AMMOGameMode::bAllowPvP` (`MMOGameMode.h:86`), ayna `AMMOGameState::bPvPEnabled`, konsol `MMOPvP` | `MMOCharacter.cpp:2221-2254` |
| Ölüm cezası ayrımı | Ölüm her zaman XP kaybı: `ApplyDeathPenalty` (`MMOStatsComponent.cpp:544-557`), oran `DeathXPPenaltyPct = 0.1f` (`MMOStatsComponent.h:197`). Çağrı `MMOCharacter.cpp:1891`. Öldüren oyuncu mu mob mu ayrımı ceza tarafında yok (yalnız metrik satırı `KillerMob`'u ayırıyor) | [Kesin] |
| Irka göre doğuş noktası | Yok, ilk PlayerStart | `MMOGameMode.cpp:956` |
| Hesap / parola | Yok. `ACCOUNT_IDENTITY_DESIGN.md` B-min tasarımı var, kodu yok (grep `MMOAccountStore`/`MMOPasswordHash` boş). **S12 (önerilen karar): Steam sürümünde parola sistemi yazılmaz, kimlik SteamID** | [Kesin] |
| Steam entegrasyonu | Yok. `Config/*.ini`, `MyProject.uproject` ve `Source/**/*.cs` içinde `OnlineSubsystem`/`Steam`/`NetDriver` geçmiyor (grep, 2026-10-13). Etkin eklentiler yalnız `PythonScriptPlugin`, `ModelingToolsEditorMode`, `GameplayStateTree` | [Kesin] |
| Dedicated sunucuda yönetici | Yok: `MMOAdmin::IsServerAdmin` dedicated'da her zaman false (`MMOAdmin.cpp:13-16`) | [Kesin] |

### 1.3 Geçiş bugün [Kesin]

- **Yalnız Summer ↔ Winter aç/kapa** (`MMOGameMode.cpp:647-662`). Diğer haritalarda "Bu haritadan bolge gecisi yok".
- **Bütün oturum birlikte taşınır.** Yalnız listen server hostu tetikler (`MMOAdmin::IsServerAdmin`, `MMOGameMode.cpp:635`), tüm oyuncular canlı ve savaş
  dışında olmalı, her karakter asenkron kaydedilir (15 sn bariyer, `MMOGameMode.h:107`), sonra `World->ServerTravel` (`:867`). `bUseSeamlessTravel = true` (`:40`, `:625`).
- Varışta kayıtlı `MapName` farklıysa karakter PlayerStart'a konur (`MMOCharacter.cpp:1456-1475`). `MapName` kayıtta zaten var (`MMOSaveGame.h:113`).
- Karakter kayıtları **host makinesinde** yerel dosya (`FMMOLocalFileCharacterStore`). Kayıt erişimi zaten bir arayüzün arkasında:
  `IMMOCharacterStore` (`Public/Save/MMOCharacterStore.h:71-113`; `Load/Save/Exists/ListSlotNames` + `LoadAsync/SaveAsync` + yuva başına
  yazım sırası). Arayüz yorumu bulut/DB mağazası için "yuva başına seri yazım ya da karşılaştır-ve-yaz (Revision)" şartını zaten yazıyor (`:63-69`).
- Başarım, günlük ve posta kendi JSON dosyalarında (`Saved/MMOAchievements`, `Saved/MMODaily`, `Saved/MMOMail`), yine host diskinde.

### 1.4 Mimari: harita başına sunucu (S1 = B kararı)

**Bugünkü model neden yetmiyor [Kesin + çıkarım]:** bir sunucu süreci aynı anda **yalnız bir harita** yükler ve geçişte herkes birlikte
taşınır (§1.3). Bu modelde İnsan oyuncu H2'de kasarken Ork oyuncu aynı anda H3'te olamaz. Kullanıcı bu sınırı kabul etmedi (S1 = B).
2026-10-12 sürümündeki "oturum ırkı" kuralı **kaldırıldı**.

**Hedef mimari (alfa):**

```
                    +---------------------------+        +---------------------------+
 Istemci --(1)----> | Giris servisi (yeni, S13a)| <----> | PostgreSQL                |
 (Steam web bileti) | Steam bileti -> SteamID,  |        | hesap (provider,          |
    |               | oturum + gecis bileti,    |        | external_id), karakter    |
    |               | sunucu dizini,            |        | (JSON), irk, posta, pazar,|
    |               | gizli Publisher Key burada|        | kilit, cevrimici varlik   |
    |               +-------------+-------------+        +---------------------------+
    |                             | (1b) AuthenticateUserTicket (+ istege bagli CheckAppOwnership)
    |                             v      partner.steam-api.com
    |                       Steam Web API
    |  (2) open <adres>:<port>?Name=..?Ticket=<servis bileti>           ^
    v                                                              |
  H1 Hub     dedicated (Summer_scene)        port 7777 [PLACEHOLDER] --+
  H2 Human25 dedicated (Winter_scene)        port 7778               --+
  H3 Orc25   dedicated (OrcLand_scene)       port 7779               --+  her sunucu karakteri
  H4 Human59 dedicated (HumanHigh_scene)     port 7780               --+  yalniz bilet + kilitle
  H5 Orc59   dedicated (OrcHigh_scene)       port 7781               --+  yukler ve yazar
  H6 PvP     dedicated (PvP_scene)           port 7782               --+
```

| Parça | Görev | Bugün | Aşama |
|---|---|---|---|
| Dedicated sunucu ikilisi | Her harita için ayrı süreç (aynı ikili, farklı harita argümanı) | `MyProjectServer.Target.cs` var, **hiç derlenmedi** [Doğrulanmadı]; kurulu motor Server hedefini reddediyor (`NETWORK_AUDIT.md:370`) | A1 |
| PostgreSQL | Tek doğruluk kaynağı: karakter, ırk, başarım/günlük, posta, pazar, karakter kilidi, çevrimiçi varlık | V1 şeması var, **bağlantı kodu yok** (§1.5) | A2 |
| Karakter mağazası (DB) | `IMMOCharacterStore`'un yeni uygulaması; asenkron; `Revision` ile karşılaştır-ve-yaz | Arayüz hazır, yalnız yerel dosya uygulaması var | A2 |
| Steam kimliği (S12, S18) | İstemci Steam'den web API bileti alır (`ISteamUser::GetAuthTicketForWebApi`), giriş servisine yollar; servis `ISteamUserAuth/AuthenticateUserTicket` ile doğrulayıp 64-bit SteamID alır. Steam bileti harita sunucularına **gitmez** | Yok (§1.2) | A5 |
| Hesap + bilet (K-1) | Karakter bir SteamID hesabına bağlıdır, adı yalnız sahibi kullanabilir. Servis kendi kısa ömürlü bileti verir, harita sunucusu `PreLogin`'de servise doğrulatır | Yok (K-1 açık, `SECURITY_AUDIT.md`) | A3 |
| Karakter kilidi | Bir karakter aynı anda yalnız **bir** sunucuda açık. Geçişte kilit kaynaktan hedefe devredilir | Yok | A2 (tablo) + A4 (devir) |
| Oyuncu başına geçiş | Kaynak sunucu oyuncuyu kaydeder, kilidi devreder, `ClientTravel` ile hedef adrese yollar | Toplu `ServerTravel` | A4 |
| Sunucular arası sosyal | `/who`, fısıltı, arkadaş varlığı, klan, posta: DB üzerinden | Süreç içi (`static GAnnounced`, `MMOFriendsComponent.cpp:250`) | A4 (+ Aşama 6 ırk kuralları) |

**Steam relay (S17):** Steam Sockets net sürücüsüyle `bAllowP2PPacketRelay` açıkken bağlantı Steam'in relay ağından geçer ve dedicated sunucunun
IP'si istemciye açılmaz (Unreal Steam Sockets belgesi; Steam Datagram Relay belgesi). Bu, alfa makinesini (S17) doğrudan port açmadan korumanın
yoludur. **[Doğrulanmadı]:** UE5.6 için "relay'siz Steam Networking bozuk" forum bildirimi var; 5.7'de relay'li ve relay'siz bağlantı A1 ölçümünde
ve A5 kabulünde denenir. Çalışmazsa yedek: Steam yalnız kimlik için kullanılır, taşıma bugünkü IP sürücüsünde kalır (Tailscale/ZeroTier, §12.1).

**Alfada kalan sınırlar (bilerek):** parti yalnız aynı harita sunucusunda olur (bugün de geçişte dağılır, `NETWORK_AUDIT.md` §5). Aynı haritanın
ikinci kopyası (kanal/instance) yok. Otomatik sunucu aç/kapa yok; altı süreç elle ya da bir betikle başlatılır.

**Geliştirmede ara yol [Muhtemel, Doğrulanmadı]:** dedicated topolojisi, kaynak motor hazır olmadan editör ikilisiyle denenebilir
(`UnrealEditor.exe MyProject.uproject <Harita> -server -log`, pişirilmemiş içerik). Bu yol yalnız geliştirme testidir, alfa dağıtımı değildir.
Kurulu (Launcher) motorun bunu kabul edip etmediği A1'in ilk ölçümüdür.

### 1.5 Sprint 1 altyapısı (commit `9855256`, 2026-10-12): ne hazır, ne değil

Okunan dosyalar ve durumları. "Derlendi" bilgisi yok: commit mesajında derleme kaydı yok, bu ortamda derleme yapılamaz. `StaticLint.py`
(derleme değildir) 131 dosyada temiz geçti (2026-10-13). `Source` içinde bu sınıfları **kullanan tek satır yok** (grep: yalnız kendi dosyaları).

| Dosya | Ne var | Bağlı mı | Test | Değerlendirme | Plandaki kullanımı |
|---|---|---|---|---|---|
| `Source/MyProject/MyProjectServer.Target.cs` | `TargetType.Server`, `BuildSettingsVersion.V5`, `Unreal5_7`, unity + PCH | — | **[Doğrulanmadı]** derlenmedi. Kurulu motor Server hedefini derlemeden önce reddediyor (`NETWORK_AUDIT.md:370`, 2026-10-06 denemesi) | Diğer hedefler `Source/` kökünde, bu dosya `Source/MyProject/` içinde. UBT'nin bu konumu taradığı **[Doğrulanmadı]**; A1'de `Source/` köküne taşınması önerilir. `bUseLoggingInShipping` eklenmedi (`NETWORK_AUDIT.md:201` bunu bu dosyaya önerir) | A1'in başlangıç noktası |
| `Migrations/V1__initial_schema.sql` | PostgreSQL: `characters`, `character_inventory/equipment/warehouse/skills/quests`, `parties`, `clans`, `friends`, `mail`, `auction_house`, `game_events`, `updated_at` tetikleyicileri | Hiçbir kod okumuyor/yazmıyor | Hiç bir DB'ye uygulanmadı [Doğrulanmadı] | Ad biçimi Flyway kuralı (`V<n>__ad.sql`). Bizim kayıtla **eşleşmeyen yerler** (aşağıda) var | A2'de temel; eksikler `V2__...sql` ile |
| `Private/Database/DatabaseMigrator.cpp` + `.h` | `UDatabaseMigrator` UObject: dosya bulma, sürüm sıralama | Hiçbir yerden çağrılmıyor | Yok | **Gerçek DB çağrısı yok:** `ExecuteSQL` SQL'i yalnız loglayıp `true` döner (`:177-191`), `GetAppliedVersion` her zaman 0 (`:50-55`), `RecordMigration`, `IsDatabaseInitialized`, `ValidateSchema` hep `true`. `Build.cs`'te libpq/PostgreSQL bağımlılığı yok. Yani "migration başarılı" sonucu **sahte başarıdır**. Ayrıca `Migrations/` klasörü paketli sunucuya kopyalanmaz [Muhtemel] | **Önerilen:** migration'lar oyun sürecinin dışında uygulanır (psql ya da Flyway; ad kuralı zaten Flyway). Bu sınıf kullanılmaz; silmek ya da tutmak kullanıcı kararı (S15) |
| `Private/Security/PacketValidator.cpp` + `.h` | `FPacketValidator`: hareket hızı/çarpışma/ışınlanma, RPC yük boyutu tablosu, paket bütünlüğü, oyuncu başı hata sayacı | Hiçbir yerden çağrılmıyor | Yok | UE RPC'lerinde ham yük baytı (`TArray<uint8> Payload`) oyun koduna gelmez, yani `ValidateRPC` doğal bir kancaya bağlanamaz [Muhtemel]. `ValidateRPCFrequency` her zaman `true` (TODO, `:286-292`), `ValidatePacketIntegrity` checksum/sıra/şifreleme TODO. Hareket doğrulaması `CharacterMovementComponent`'in sunucu düzeltmesiyle çakışır. Tablodaki bazı adlar gerçek RPC'lerle eşleşir (`ServerAttackTarget`, `ServerUseConsumable`; `ServerAddItem`/`ServerSetGold` takas bileşeninde), bazıları kodda yok | **Alfa için değmez:** RPC oran sınırları bugünkü desenle (`_Validate` + bileşen içi bekleme süresi, ör. iksir 1 sn, `NETWORK_AUDIT.md` D4) yazılır. Paket bütünlüğü motorun ağ katmanının işi |
| `Scripts/build.ps1` | Editor/Server/Client derleme sarmalayıcısı | — | [Doğrulanmadı] | `ProjectPath` kullanıcının masaüstü yoluna sabit, motor yolu Launcher yolu. `MyProjectClient.Target.cs` **yok**, yani `-Target Client`/`All` başarısız olur [Kesin, dosya yok]. **`-Clean` `Saved/` klasörünü siler** (`Remove-Item ... Saved`): karakter `.sav`, başarım, günlük ve posta dosyaları orada. Veri kaybı riski | A1'de `-EnginePath` ile kaynak motora yöneltilir. `-Clean` `Saved/`'i silmemeli (A1 düzeltmesi) |
| `Scripts/k6_load_test.js` | 500 sanal kullanıcı, HTTP `POST /api/auth/login`, `/api/character/move`, `/api/combat/attack`, `/api/inventory` ... | — | Çalıştırılamaz: bu uç noktalar yok | Oyun trafiği UE'nin UDP ağ sürücüsünden geçer, HTTP değil. Hareket/saldırı HTTP uç noktası sunucu-otoriter UE tasarımında olmayacak. Script en fazla ileride giriş servisinin `/login` ucunu yükleyebilir | A3'te yalnız giriş servisi yük testi olarak uyarlanır. Oyun içi yük testi için çoklu headless istemci (NetSmoke deseni) |

**V1 şeması ile bugünkü kayıt (`FMMOCharacterSaveData`, `MMOSaveGame.h:17-120`) arasındaki farklar [Kesin, iki dosya karşılaştırıldı]:**
- Irk sütunu yok. Kanonik ad anahtarı yok: `name VARCHAR(32) UNIQUE` büyük/küçük harf duyarlı, oysa çakışma kuralı `MMONameRules::CanonicalKey` (boşluk → `_`, küçük harf).
- `accounts` tablosu yok (`account_id UUID NOT NULL` hiçbir tabloya bağlı değil). Karakter kilidi (aktif sunucu, kira süresi) ve `revision` sütunu yok.
- Kayıtta olup şemada olmayan alanlar: `NextInstanceId`, `MasteryRanks`, `ClassChangeCooldownExpiryUtcSeconds`, `SkillCooldownExpiryUtcSeconds`, `Hotbar` sırası.
  `character_inventory` örnek alanı (instance/plus/rarity) taşımıyor, oysa çanta `TArray<FMMOEquipmentInstance>`.
- Başarım ve günlük için tablo yok. `mail` şeması bugünkü posta deposundan (`FMMOMailStore`, altın + mesaj) farklı.
- Kodda `Gold`/`Experience` `int32`, şemada `BIGINT` (zararsız, ama dönüşümde aralık kontrolü gerekir).

**Öneri (A2):** alfa için karakter verisini ilişkisel tablolara bölmek yerine **mevcut JSON zarfını** (`FMMOCharacterSaveJson`,
`Public/Save/MMOCharacterSaveJson.h`; SchemaVersion 2, sıkı doğrulama, 100+ negatif test, `ACCOUNT_BACKEND_COMPARISON.md` §6) bir `JSONB`
sütununda sakla. `V2__character_store.sql` şunları ekler: `accounts`, `character_saves (canonical_key PK, display_name, account_id, race,
schema_version, revision, data JSONB, active_server, lease_until, updated_at)`, `character_side (canonical_key, kind, data JSONB)` (başarım/günlük),
`presence`, `travel_tickets`. **`accounts` (S12):** `account_id` PK, `provider TEXT NOT NULL` (`'steam'`), `external_id TEXT NOT NULL`
(64-bit SteamID ondalık metin olarak; `BIGINT` işaretli olduğu için metin önerilir [Tahmin]), `UNIQUE (provider, external_id)`, `is_admin`, `created_at`,
`last_login_at`. Parola/e-posta sütunu **yok**. V1'in normalize karakter tabloları alfada **kullanılmaz** (yalnız `auction_house` pazar için uyarlanır, `mail` için karar S16).
Gerekçe: aynı veriyi iki biçimde tutmak çift bakım ve senkron hatası üretir; JSON yolu zaten test edilmiş.

---

## 2. Harita yapısı ve mevcut içeriğin eşlenmesi

### 2.1 Arazi kaynağı: kopyala + yeniden giydir

- Mevcut iki harita **yeniden adlandırılmaz** (kayıtlardaki `MapName`, betikler, CI bu adlara bağlı).
- Yeni haritalar editörde kopya olarak çıkar: `/Game/HandPaintedEnvironment/Maps/<Ad>_scene/<Ad>_scene`. Bu yol kalıbını
  `SmokeTest.ps1:15` değiştirmeden okur. Arazi ve çarpışma aynı kaldığı için **ölçülmüş cep koordinatları kopyada geçerli kalır**
  [Muhtemel; kopya başına `MMOProbeSurface` ile yeniden ölçülür].
- **İnsan/Ork çiftinin aynı araziyi paylaşması türe uygun:** KO'da El Morad/Luferson ve Eslant'ın iki yarısı ayna tasarımlardır.
  Fark giydirme (renk, bayrak, mimari prop) ve NPC'lerdedir.
- Winter'da **17** ölçülmüş cep konumu var (`MMOGameMode.cpp:1038-1076`). Summer'da hub'a ≥2.600 u uzak ve birbirinden ≥1.800 u
  açık **8** ölçülmüş konum var: Obsidyen, Obsidyen Reisi, Çözülme, Alacakaranlık, Hükümdar, Arayıcı, Bekçi, Haberci
  (PlayerStart (-816,-47)'ye 2.648-6.922 u).

### 2.2 Harita → dosya → cep eşlemesi

| # | Kimlik (kod) | Görünen ad `[PLACEHOLDER]`, `metin-yazari` | Dosya | Yapım | Spawner | Cepler (mevcut koordinatlar) |
|---|---|---|---|---|---:|---|
| H1 | `Hub` | Akçakule (ortak merkez) | `Summer_scene` | Var | **11** | Summer Lv1-24 cepleri yerinde: ana 1-5, 6-7, 8-12, 13-15, 16-22 + 6 elit (SpiderQueen, OrcWarlord, StoneGolem, ShadowAssassin, FrostLich, MagmaTyrant) |
| H2 | `Human25` | Ravenmoor (Lore'da adı geçen İnsan sınırı, `Lore.md:5`) | `Winter_scene` | Var, İnsan giydirmesi | **16** | Winter A-konumları (bugünkü 42-57, 8 cep) yerinde. Winter B-konumlarında (bugünkü 66-80, 9 konum): Obsidyen 25-27, 27-29, Reis 30, Alacakaranlık 31-33, 36, 39, Hükümdar 40, Yarık Arayıcısı 58 (8) |
| H3 | `Orc25` | Ork yurdu (adı yok, Lore yazılacak) | `OrcLand_scene` (yeni) | Winter kopyası, Ork giydirmesi | **16** | H2 ile aynı koordinat ve bantlar (ayna) |
| H4 | `Human59` | İnsan üst bölge | `HumanHigh_scene` (yeni) | Winter kopyası | **13** | B-konumları: Kül 66/68/69 + Tiran, Uçurum 72/74/76/78 + Naib (9, bugünkü koordinatlar **aynen**). A-konumlarında: Yarık 59-60, 61-62, 63-64 + Haberci (4) |
| H5 | `Orc59` | Ork üst bölge | `OrcHigh_scene` (yeni) | Winter kopyası | **13** | H4 ile aynı (ayna) |
| H6 | `PvP` | Savaş alanı | `PvP_scene` (yeni) | Summer kopyası | 5 + 1 elit `[PLACEHOLDER]` + iki ırk kampı | Summer'ın 8 uzak konumundan PvE cepleri (bant Lv55-72, §6 K2). İki kamp için iki PlayerStart (ırk etiketi) |

Toplam **~70 spawner** (bugün 35). Her sunucu sürecinde en çok 16 (bugün Summer'da 18). **Not (S1 = B):** artık altı harita aynı anda
yüklü olabilir, yani **makine** başına yük altı sürecin toplamıdır. Alfa makinesinin bellek/CPU ölçümü A1/A4 kabulüne eklendi.
Winter arazisi 4 haritada (H2-H5) kullanılır. Ayna çift türe uygun ama H2 ile H4'ün aynı arazide olması **gerçek bir tekrar**.
Giydirme farkı büyük tutulmalı ya da H4/H5 sonra özgün araziye taşınmalı (§7, alfa sonrası).

**H2/H3 rota riski [Muhtemel]:** Lv25 oyuncusu hub'dan Lv25-41 ceplerine (B-konumları, hub'a 3.800-6.800 u) yürürken
Lv42-57 ceplerinin (A-konumları, hub'a 3.000-6.000 u) yanından geçebilir. Winter için `ProbeThawRoute.py` eşdeğeri yok
(`CONTENT_PLAN_LV46_80.md` §3.5). Seçilen eşlemede rota açıklığı ölçülmeli. Gerekirse düşük bantla yüksek bant konum takası yapılır.

### 2.3 Mevcut içeriğin yeni yapıya eşlenmesi

| Bant | Bugün nerede | Yeni yer | Mob (normal+elit) | Görev |
|---|---|---|---:|---:|
| Lv1-24 | Summer | H1 (ortak) | 19 + 6 | 37 |
| Lv25-41 (Obsidyen, Alacakaranlık) | Summer | H2 **ve** H3 | 7 + 2 | ~20 |
| Lv42-57 (Kış, Buzbağlı) | Winter | H2 **ve** H3 | 9 + 2 | ~14 |
| Lv58 (Yarık Arayıcısı) | Summer | H2/H3 (59 kapısına ulaşmak için) | 1 | ~1 |
| Lv59-65 (Yarık) | Summer | H4 **ve** H5 | 3 + 1 | ~7 |
| Lv66-80 (Kül, Uçurum) | Winter | H4 **ve** H5 | 7 + 2 | ~18 |
| Lv55-72 karma (H6 PvE) | — | H6 (mevcut arketiplerin kopya cepleri) | 5 + 1 | 0 (alfa) |

Görev sayıları `~` ile verildi: görev kapı seviyesine göre sayıldı (`CONTENT_MAP.md`). Lv25-58 toplamı 35, Lv59-80 toplamı 25 [Kesin].

**İki paralel zincir nasıl kurulur. Seçenekler:**

| Seçenek | Ne yapılır | Yeni tablo satırı | İş [Tahmin] | Sonuç |
|---|---|---:|---|---|
| **P1 (önerilen alfa)** | Aynı mob ve aynı görev kimlikleri iki ırk haritasında da doğar. Görevler karakter başına zaten ayrı, zincir kodda tek kalır. Irka özgü olan: NPC adları, konuşan NPC replikleri, harita giydirmesi | 0 mob, 0 görev | 1-2 gün (yerleşim satırı + metin) | İki ırk aynı hikâyeyi farklı kampta yaşar. Görev metni ırk-nötr olmalı |
| P2 | Görev metinleri ırka göre iki sürüm. Kimlik ve mob aynı | 0 kod satırı; `Lore.md`/diyalog ~60 görev × 2 | 4-6 gün (`metin-yazari` + `GenQuestDialogue.py`'ye ırk alanı) | Lore zenginleşir, kod değişmez |
| P3 | Lv25+ için Ork tarafına ayrı mob ve görev seti (yeni kimlikler) | +34 mob, +60 görev, +eşya kademeleri | 10-15 gün + `ValidateData`/`ContentMap`/denge modeli iki katı | **Alfa için değmez:** aynı dövüşü ad değiştirerek kopyalamak test edilecek yeni sistem kazandırmaz, bakım yükünü ikiye katlar |

**Lore çatışması (P1/P2 için zorunlu metin işi) [Kesin]:** Mob tablosunda Ork düşmanlar var (Orc, OrcWarrior, OrcWarlord, OrcBerserker,
AshOrc ve Obsidyen/Alacakaranlık "ork akıncıları", `Lore.md:365-410`). Ork oyuncu Ork mob kesecek. `metin-yazari` bunları
"Kırık Klan / Fısıltıya uymuş dönek orklar" gibi bir fraksiyona bağlamalı. Mob kimlikleri değişmez, yalnız görünen ad ve Lore değişir.
Akçakule ve Kül Muhafızları İnsan-merkezli. H1'in "iki ırkın ortak sınır pazarı" olarak yeniden anlatılması gerekir.

### 2.4 Summer'ın Lv1-40'tan Lv1-25'e inmesinin etkisi

| Etki | Ayrıntı | Etiket |
|---|---|---|
| Summer'dan çıkan spawner | Obsidyen 25-29, Obsidyen Reisi, Alacakaranlık 31-39, Hükümdar, Yarık ×3 = **7**. Summer **11** spawner'a iner | [Kesin] |
| Rota riski kalkar | Summer'da Lv58+ ve Lv25+ cep kalmaz. Lv7-40 oyuncunun Yarık mobu çekmesi (QA 527ca1f) ve Çözülme rotası sorunu (`MMOGameMode.cpp:1277-1298`) ortadan kalkar | [Kesin] |
| Boşalan 8 uzak konum | Lv13-24 için ek cep olabilir (merkezde kalış uzar) ya da pazar alanı için boş bırakılır | [Tahmin] |
| Merkezde kalış süresi | Lv25'e ~2,4-2,6 saat (TimelineModel 25 sn/öldürme; Lv20 = 1,6 s, Lv30 = 3,0 s) | [Tahmin] |
| Eski kayıtlar | Summer'da Lv25+ konumdaki karakter boş arazide doğar, yürür. Zararsız | [Kesin] `MMOCharacter.cpp:1459` |
| Görevli NPC | Lv25+ görevleri de gösterir (tek görev penceresi). Alfada kabul. Seviye kuralı listeyi zaten süzüyor [Muhtemel] | — |
| `Q_Level30`, `Q_Gold15000` vb. | Kilometre taşları haritadan bağımsız, etkilenmez | [Kesin] |

### 2.5 NPC seti (her harita)

| Rol | Sınıf | H1 | H2-H5 | H6 |
|---|---|---|---|---|
| Satıcı | `AMMOVendorNPC` | Zorunlu | Zorunlu | Zorunlu (her kampta) |
| Görev veren | `AMMOQuestGiverNPC` | Zorunlu | Zorunlu | Zorunlu (PvE cepleri için) |
| Şifacı | `AMMOHealerNPC` | Zorunlu | Zorunlu | Zorunlu (her kampta) |
| Depocu | `AMMOWarehouseNPC` | Zorunlu | Zorunlu | Önerilir |
| Konuşan | `AMMOTalkerNPC` | Var (Kael) | Önerilir (ırk sesi) | İsteğe bağlı |

**Zorunlu minimum: 4 NPC/harita. H6'da iki kampın her birinde 3.** H6 kamp NPC'leri ırka bağlıdır: karşı ırkın satıcısı
kullanılamaz. Bu yeni bir kontrol (§3.2).
**Değmez (alfa):** harita başına ayrı satıcı stoğu. Seviye kuralı (`VendorLevelSlack` 3) zaten süzüyor.
**Depo notu (S1 = B):** depo karakter kaydının içinde (`Warehouse`, `MMOSaveGame.h:72`), yani DB'ye taşınınca her haritadaki depocu aynı depoyu görür. Ek iş yok.

### 2.6 Geçiş grafiği

```
                         +--(Lv25, Insan)--> H2 Human25 --(Lv59)--> H4 Human59
H1 Hub (Lv1-25, ortak) --+
                         +--(Lv25, Ork)----> H3 Orc25 ----(Lv59)--> H5 Orc59
   ^      |                   |                    |                    |
   |      +--(Lv>=55)-------> H6 PvP <-------------+--------------------+
   +------------------- her haritadan H1'e donus ------------------------+
```

Kapılar **oyuncu başınadır** (S1 = B): her oyuncu kendi geçişini ister, yalnız kendi ırkı ve seviyesi denetlenir, başkasını taşımaz.

| Kenar | Kapı (kaynak sunucuda, istekte bulunan oyuncu için) | Etiket |
|---|---|---|
| H1 → H2/H3 | Oyuncunun ırkı = hedefin ırkı. Lv ≥ 25 `[PLACEHOLDER]` | Öneri |
| H2 → H4, H3 → H5 | Aynı ırk + Lv ≥ 59 `[PLACEHOLDER]` | Öneri |
| Herhangi → H6 | Lv ≥ **55** (K1). Irk serbest | **Karar** |
| Herhangi → H1 | Kapı yok | Öneri |
| H2 ↔ H3, H4 ↔ H5 | **Yasak** (karşı ırk) | Öneri |
| Ortak ön koşullar | Canlı, savaş dışında, takas/tezgâh açık değil, ırk seçilmiş (`None` ise yalnız H1) | Öneri |

Irk kapısında alternatif yok, çünkü karşı ırk haritasına girmek gereksinimi çiğner. Seviye kapısı zorlayıcıdır (S8).
Hedef sunucu, biletteki hedef bölgeyi ve kapıyı **yeniden** denetler (kaynak sunucuya körü körüne güvenilmez; §4.3).
Irk haritasındaki **varış**: tek PlayerStart. H6'da varış ırk etiketli iki PlayerStart'tan seçilir.
**Geçiş yolu nasıl tetiklenir:** alfada HUD hedef listesi ya da `MMOTravel <Bolge>` (hub NPC'si veya kapı aktörü alfa sonrası, §7).

---

## 3. Irk sistemi (yeni)

### 3.1 Bileşenler

| # | Parça | Tasarım | Sunucu otoritesi / replikasyon |
|---|---|---|---|
| I1 | Tür | `enum class EMMORace : uint8 { None, Human, Orc }` (ASCII, yeni başlık `Public/Core/MMORace.h`) | — |
| I2 | Kalıcılık | **Hedef: DB sütunu** `character_saves.race` (A2, `V2__character_store.sql`). `MMOSaveGame.h`'e **dokunulmaz**. **Ara yol (Aşama 2, DB'den önce):** küçük bir arayüz (`IMMORaceStore`, `Load/Save(SlotKey)`) arkasında yan dosya `Saved/MMORace/<SlotKey>.json` (başarım/günlük deseni: `.tmp` → Move, bozuk dosya `.tmp.bad`). A2'de DB uygulaması eklenir, yan dosyalar bir kez içe aktarılır | Yalnız sunucu yazar. Okuma `AMMOCharacter::IsInitialLoadComplete()` sonrasında |
| I3 | Seçim | İlk girişte, sınıf seçimi gibi HUD paneli (`ClassChoiceRegions` deseni). Irk yoksa seçilene kadar H1 dışına geçiş yok | `ServerChooseRace(EMMORace)`: yalnız `Race == None` iken, `_Validate` ile değer aralığı. Kalıcı yazım başarılı olunca uygulanır. **Değiştirilemez** (alfa) |
| I4 | Görünürlük | `AMMOCharacter` üzerinde (ya da `UMMORaceComponent`'te) `ReplicatedUsing=OnRep_Race`. Ad etiketi rengi ve hedef imleci için | `DOREPLIFETIME`, `StaticLint` kuralları |
| I5 | Kimlik bağı | Irk karakter anahtarına bağlı. K-1 açığı ırkı da etkiler. **S1 = B ile K-1 alfadan önce kapanmak zorunda** (A5 + A3): bilet olmadan harita sunucusu kimin bağlandığını bilemez | S12: karakter → `accounts` (SteamID). `ACCOUNT_IDENTITY_DESIGN.md` B-min'in parola kısmı Steam sürümünde kullanılmaz; sahiplik/bilet/kilit fikri kalır |
| I6 | Eski karakterler | Irksız kayıt → ilk girişte seçim paneli (H1'e konur) | Göç kodu yok, yalnız `None` durumu |

**Yan dosya riski (yalnız ara yolda) [Muhtemel]:** ırk dosyası kaybolur ya da bozulursa karakter `None` olur ve yeniden seçebilir.
Önlem: `.bad` durumunda seçimi kilitle ("ırk kaydı okunamadı, yöneticiye bildir"). A2'den sonra risk kalkar (ırk karakter satırında, tek işlemde yazılır).

### 3.2 Irk kuralları (her sistem için)

| Sistem | Dosya (bugün) | Kural (öneri, KO) | Dokunuş | Çakışma |
|---|---|---|---|---|
| Geçiş | `MMOGameMode.cpp:630` | §2.6 kapıları (oyuncu başına) | Orta | Sıcak dosya |
| Hasar / PvP | `MMOCharacter.cpp:2215-2260` (PvP kapısı) | H6'da yalnız **karşı ırka** hasar. Diğer haritalarda oyuncu-oyuncu hasarı yok. Global `bAllowPvP` yerine bölge tablosundan `bPvPZone` (konsol anahtarı test için kalır) | Orta | `MMOCharacter` sıcak |
| Parti | `UMMOPartyComponent` | Yalnız aynı ırk, yalnız aynı harita sunucusu (§1.4) | Küçük kontrol | **Codex'in dosyası** (`CheckPrPaths.py` uyarısı). Codex'le sıra planla |
| Klan | `UMMOClanComponent` | Yalnız aynı ırk. Klan adı çakışma taraması DB'den (bugün `ListSlotNames`, `MMOClanComponent.cpp:368-369`) | Küçük | — |
| Takas | `UMMOTradeComponent` | H1'de karşı ırkla **serbest** (S4). Diğer yerlerde aynı ırk. Takas iki taraf da aynı sunucudayken olur (bugünkü gibi) | Küçük | **Codex'in dosyası** |
| Posta | `UMMOMailComponent` + `FMMOMailStore` | Aynı ırk. Alıcı başka haritada ya da çevrimdışı olabilir: kutu DB'de (A2/A4). Irk okunamazsa gönderim reddedilir ve iade edilir (bugünkü `StoreError` deseni) | Orta | — |
| Fısıltı `/w` ve sohbet | `MMOPlayerController.cpp` | Genel sohbet ırka göre kanal (sunucu içi). Fısıltı **sunucular arası** (A4, S14). Karşı ırka fısıltı yok. H1/H6'da "yakın" sohbet herkes | Orta | `MMOPlayerController` sıcak |
| Arkadaş | `UMMOFriendsComponent` | Yalnız aynı ırk. Çevrimiçi durumu DB `presence` tablosundan (bugünkü süreç içi `GAnnounced` yerine) | Küçük-orta | — |
| NPC | `AMMONpcBase` | H6 kamp NPC'si yalnız kendi ırkına hizmet eder | Küçük | — |
| `/who` | `MMOPlayerController.cpp` | Irk + harita sütunu, tüm sunucular (DB `presence`) | Küçük | — |

### 3.3 Pazar kurma (H1, **alfada şart**: S5)

Bugün yok (§1.2). En küçük uygulanabilir tasarım (KO "pazar kurma" = çevrimiçi oyuncu tezgâhı):
- `UMMOMerchantComponent`: oyuncu H1'in pazar alanında (hub güvenli bölgesi) tezgâh açar. Çantasından en çok 8 `[PLACEHOLDER]`
  eşya ve fiyat koyar. Tezgâh açıkken hareket edemez. Diğer oyuncu tıklayıp satın alır. Satıcı ve alıcı **aynı sunucuda** (H1) olduğu için
  işlem sunucu içidir; DB yalnız kalıcılık, denetim izi ve ikileme koruması için kullanılır.
- **DB kullanımı (V1 `auction_house` tablosu uyarlanır, `V3__market.sql`):** tezgâh açılınca her satır için bir kayıt (`seller_id`, `item_id`,
  `instance_id`, `plus_level`, `rarity`, `price`). Eksikler: `seller_id` karakter anahtarına bağlanmalı (V1 `characters(id)`'e bağlı, biz
  `character_saves` kullanıyoruz), `fee` sütunu, `status` (`open/sold/closed`), `server_id`. Satın almada önce
  `UPDATE auction_house SET buyer=.., sold_at=now(), status='sold' WHERE id=$1 AND status='open' RETURNING ...` (tek satır döndüyse devam),
  sonra bellek içinde altın/eşya takası, sonra **iki karakter kaydı tek DB işleminde** (`revision` karşılaştırmasıyla). Herhangi biri
  başarısızsa işlem geri alınır, satır `open`'a döner. Bu, takastaki `TryExecuteTrade` deseninin DB karşılığıdır.
- Sunucu otoritesi: satın alma tek RPC'dir (`ServerBuyFromStall(SellerPawn, ListingId, ExpectedPrice)`). Sunucu satıcının hâlâ orada,
  eşyanın hâlâ çantada ve fiyatın değişmemiş olduğunu doğrular (fiyat değişince eski istek reddedilir).
- Çevrimdışı satış **yok** (alfa): satıcı bağlantıyı kesince ya da harita değiştirince tezgâh kapanır, satırlar `closed` olur. Offline
  pazar/açık artırma alfa sonrası karar (S5b).
- Ekonomi: satıştan **%2-3 `[PLACEHOLDER]` pazar vergisi** altın sink'i olur (postadaki %2 ile tutarlı). Yeni altın kaynağı yok.
- Güvenlik: eşya ikileme (aynı eşya hem tezgâhta hem takasta, postada, depoda ya da satıcıda) en büyük risk. Tezgâhtaki eşya kilitlenir.
  Geçiş isteği tezgâh açıkken reddedilir (§2.6). `ag-guvenlik-denetcisi` zorunlu.

---

## 4. Kodda değişecek noktalar

### 4.1 İki haritayı varsayan kod ve betikler [Kesin]

| # | Yer | Bugün | Yeni |
|---|---|---|---|
| K1 | `MMOGameMode.cpp:647-662` | Summer↔Winter aç/kapa | Bölge tablosundan hedef + ırk/seviye kapısı + paket-var kontrolü |
| K2 | `MMOGameMode.h:72` | `RequestZoneTravel(APlayerController*, FString&)` (yönetici, toplu) | Yeni `RequestPlayerZoneTravel(APlayerController*, FName ZoneId, FString&)` (oyuncu başına, §4.3). Eski imza listen-server test yolu olarak kalır, böylece 6 `TestZoneTravel*.ps1` değişmez |
| K3 | `MMOGameMode.cpp:972`, `1097`, `1262` | Harita adına göre if/else | Bölge tablosu |
| K4 | `MMOGameMode.cpp:944-1435` `SpawnMobArea` (~490 satır) | Koordinatlar fonksiyonda, iki lambda | Yeni `World/MMOZoneLayout.{h,cpp}`, `MakeZone`/`MakePocket`/`MakeNpc` satırları |
| K5 | `MMOGameMode.cpp:956-962` | İlk PlayerStart | H6'da ırk etiketli PlayerStart seçimi (`ChoosePlayerStart` override; yeni motor override → `StaticLint.py` `OVERRIDE_WHITELIST`) |
| K6 | `MMOPlayerController.h:28-30`, `.cpp:58-78` | `MMOTravel()` | `MMOTravel <Bolge>` (argümansız eski davranış kalır) |
| K7 | `MMOHUD.cpp:189-210`, `3102` | Tek düğme "Kış'a/Yaz'a taşı" | Hedef listesi + ırk seçim paneli |
| K8 | `MMOCharacter.cpp:2215-2260` | Global PvP | Bölge `bPvPZone` + karşı ırk kuralı |
| K9 | `MMOCharacter.cpp:1458-1460` | Harita adsız eski kayıt = Summer | **Değişmez** |
| K10 | `Config/DefaultGame.ini:12-13` `MapsToCook` | 2 harita | 6 harita (Config korunan yol: yerel oturum, kullanıcı onayı) |
| K11 | `MMOCharacter.cpp:1889-1891` + `MMOStatsComponent` | Tek ölüm cezası | Öldüren türüne göre oran (§6 K4) |
| S1 | `SoakTest.ps1:9`, `NetSmokeTest.ps1:6` ValidateSet, `RunAllChecks.ps1:408` dizi | 2 harita | 6 harita |
| S2 | `PocketSpacing.py:28,45` | Harita = lambda adı | Harita = `MakePocket` bölge alanı. `TestPocketSpacing.py` |
| S3 | `ValidateData.py:167-277` | `MMOGameMode.cpp` regex | `MakePocket` satırları + yeni kurallar (§5). `TestValidateData.py` |
| S4 | `docs/PLAYTEST_LV46_80.md` üçlüleri | Haritasız | Harita etiketi |
| S5 | **68 betik** (`Scripts/*.ps1`, `.py`) Launcher motor yolunu (`Epic Games\UE_5.7`) sabit yazıyor (grep, 2026-10-13) | Sabit yol | Kaynak motora geçince tek bir yerden (`MMO_ENGINE_DIR` ortam değişkeni ya da ortak `Scripts/EnginePath.ps1`) okunur (A1) |

### 4.2 Sıcak dosya / Codex çakışma planı

`git status` (2026-10-13) temiz. Son commit `9855256` (Sprint 1) `MMOInventoryComponent.*`, `MMOMailComponent.cpp` ve testlere de dokundu;
öncekiler `MMOGameMode.cpp`'nin `ChangeName/PreLogin` bölgesine (445-619). Her aşamada yerel ajan **önce** `git status` bakar, Codex'in
commit'lenmemiş dosyalarına dokunmaz.

| Dosya | Dokunuş | Plan |
|---|---|---|
| `MMOGameMode.cpp` | `SpawnMobArea` taşınır (bir kerelik büyük diff), `RequestZoneTravel` başı, `PreLogin`/`Login` (A3/A4: bilet + DB yükleme) | Tek ajan, tek commit. 445-619 A3'te değişir: o aşamada Codex'le pencere ayarlanır |
| `MMOCharacter.cpp/.h` (sıcak) | Irk alanı + RPC + PvP kapısı + ölüm cezası ayrımı | Irk mantığı yeni `UMMORaceComponent`'te. Karakterde yalnız bileşen oluşturma, PvP kapısında tek çağrı, ceza çağrısında tek parametre |
| `MMOStatsComponent` (sıcak) | `ApplyDeathPenalty` oran parametresi (K4) | Yeni fonksiyon eklenir, eskisi ona yönlenir. Tek küçük commit |
| `MMOPlayerController` (sıcak) | `MMOTravel` argümanı, sohbet kanalı, sunucular arası fısıltı | En sona, küçük commit'ler |
| `MMOPartyComponent.*`, `MMOTradeComponent.*` | Irk kontrolü | **Codex'e devredilir** ya da Codex'in temiz anında tek satırlık çağrı (`MMORace::AreAllied(A,B)`) |
| `RunAllChecks.ps1`, `PLAYTEST_CHECKLIST.md` (sıcak) | Dizi satırı, yeni adımlar, PIE maddeleri | Yalnız ekleme |
| `MMOSaveGame.h` | **Dokunulmaz** | Irk DB sütununda (ara yolda yan dosyada) |
| `MyProject.uproject`, `Config/DefaultEngine.ini` (A5, Steam) | Eklenti listesi, online alt sistem, net sürücü | Korunan yol: yalnız yerel oturumda, kullanıcı onayıyla, tek commit. Steam App ID dışında gizli değer **girmez** (Publisher Key servisin yapılandırmasında, depo dışında) |
| `MMOCharacterStore.h` / `MMOCharacterStoreSubsystem.cpp` | DB mağazasının seçimi (komut satırı bayrağı) | A2'de tek ajan; arayüze yalnız gerekiyorsa `Revision` alanı eklenir |

### 4.3 Harita başına sunucu için değişecek noktalar (S1 = B)

| # | Yer | Bugün [Kesin] | Yeni (öneri) | Aşama |
|---|---|---|---|---|
| D1 | `MyProjectServer.Target.cs` | `Source/MyProject/` içinde, derlenmedi | `Source/` köküne taşı, kaynak motorla derle, `bUseLoggingInShipping` değerlendir | A1 |
| D2 | `MMOAdmin.cpp:13-16` | Dedicated'da yönetici yok | Sunucu konsolu (`Exec`) + DB'de hesap bayrağı `accounts.is_admin` (SteamID hesabına bağlı, A3). Test komutları `!UE_BUILD_SHIPPING` | A1/A3 |
| D3 | `IMMOCharacterStore` | Yalnız `FMMOLocalFileCharacterStore` | `FMMODbCharacterStore`: `LoadAsync/SaveAsync` gerçekten asenkron (oyun iş parçacığına geri döner), `revision` ile karşılaştır-ve-yaz, `ListSlotNames` → DB sorgusu. Mağaza seçimi komut satırıyla (`-MMOStore=db`), varsayılan yerel dosya (bugünkü testler değişmez) | A2 |
| D4 | DB erişim yolu | Yok (`Build.cs`'te `HTTP`/`Json` var, libpq yok) | **S13 önerilen karar: (a)** küçük bir arka uç servisi (HTTP + JSON, PostgreSQL'e ve Steam Web API'ye yalnız o bağlanır; Publisher Key yalnız onda). (b) libpq seçeneği kapandı. Servis dili kullanıcı kararı (varsayılan Python [Tahmin]) | A2 |
| D5 | Başarım/günlük/posta depoları | `Saved/MMOAchievements`, `Saved/MMODaily`, `Saved/MMOMail` JSON | Aynı JSON gövdeleri DB'de (`character_side`, `mail`). Mevcut `.bak/.tmp/.bad` mantığı yerel modda kalır | A2 |
| D6 | `AMMOGameMode::PreLogin` (445-619 bölgesi) | `?Name=` + ad kuralları | **Servis** bileti doğrulama (`?Ticket=`; Steam biletinin kendisi değil), biletteki hesabın (SteamID) karakterin sahibi olduğu kontrolü, karakter kilidi alma; DB gecikmesi için `PreLoginAsync` (`ACCOUNT_IDENTITY_DESIGN.md:28`, motor kancası) | A3 |
| D7 | `RequestZoneTravel` + `ServerTravel` (`MMOGameMode.cpp:630-884`) | Yönetici tetikler, herkes birlikte, `ServerTravel` | `RequestPlayerZoneTravel`: (1) kaynak sunucu oyuncunun kapısını denetler; (2) `SaveAsync` (mevcut bariyer mantığının **tek oyunculu** hali, 15 sn zaman aşımı korunur); (3) DB'de kilit devri + tek kullanımlık geçiş bileti (`travel_tickets`: karakter, hedef bölge, son kullanma 30 sn `[PLACEHOLDER]`); (4) `PC->ClientTravel("<adres>:<port>?Name=..?Ticket=..", TRAVEL_Absolute)`; (5) kaynak sunucu pawn'ı **kaydetmeden** kaldırır (son kayıt zaten yazıldı). Hedef sunucu: bileti tüketir, kapıyı yeniden denetler, `LoadAsync`, kilidi alır. Eski toplu yol listen-server geliştirme yolu olarak Aşama 9'a kadar kalır | A4 |
| D8 | Sunucu dizini | Yok | `zone_servers (zone_id, map_name, address, port, heartbeat_at)`. Her sunucu açılışta kendini yazar, 10 sn'de bir kalp atışı `[PLACEHOLDER]`. Kapalı hedefe geçiş reddedilir ("Bölge kapalı") | A4 |
| D9 | `MMOFriendsComponent.cpp:250` `static GAnnounced`, `/who`, `/w` | Süreç içi | DB `presence (character, zone_id, server_id, last_seen)`. Fısıltı: S14 kararına göre DB üzerinden kuyruk + 1-2 sn yoklama `[PLACEHOLDER]` ya da alfa için yalnız aynı sunucu | A4 |
| D10 | Çökme | Tek süreç, kayıt yerel | Kilit kira süresi (`lease_until`, 60 sn `[PLACEHOLDER]`) dolunca karakter başka sunucuda açılabilir. Çöken sunucunun son otomatik kaydı geçerli olur (en çok 60 sn ilerleme kaybı, bugünkü otomatik kayıt aralığı) | A4 |
| D11 | Başlatma | `NetSmokeTest.ps1` tek listen server | `Scripts/StartZoneServers.ps1` (yeni): 6 süreç, port ve harita tablosu, log dosyaları; `Scripts/TestCrossServerTravel.ps1` (yeni) | A4 |
| D12 | Steam (S12/S18) | Yok (§1.2) | `MyProject.uproject`'e `OnlineSubsystemSteam` + `SteamSockets` eklentileri; Steamworks SDK (motorla gelen sürüm ya da güncelleme, [Doğrulanmadı]); `Config/DefaultEngine.ini`: online alt sistem = Steam, App ID, net sürücü = Steam Sockets (Config korunan yol: **yalnız yerelde**, kullanıcı onayıyla; `CheckPrPaths.py` PR'da hata verir). İstemcide yeni küçük sınıf (öneri `UMMOSteamAuthSubsystem`, ad [Tahmin]) bileti alır ve servise yollar. **Not:** Steam Sockets'li yapı yalnız Steam Sockets'li yapıyla bağlanır; bayraksız headless testler (`NetSmokeTest`, `RunAllChecks`) bugünkü IP sürücüsünde kalmalı (§8 R13) | A5 |

---

## 5. Doğrulama (her harita + ırk + çok sunucu)

| Kontrol | Araç | "Bitti" |
|---|---|---|
| Harita açılıyor, mob ve NPC doğuyor | `SmokeTest.ps1 -Map <Ad>_scene` | Çıkış 0, hata 0, `Mob dogumu` > 0, NPC log satırları (H6'da iki kamp) |
| Cep kalitesi | `MMOProbeSurface X Y 225` (elit 150) | ok ≥ 60/64 |
| Takılma ve ilk doldurma | `SoakTest.ps1 -Seconds 90` + `SpawnAudit.py` | Takılma ≤ 1, her cep ilk doldurmada ≥ 4/6 |
| Cep aralığı | `PocketSpacing.py` (bölge bazlı) | Hata yok |
| Veri | `ValidateData.py` yeni kurallar: (1) her bölgede ≥1 normal spawner ve zorunlu NPC'ler; (2) cep bandı bölge bandında; (3) her `KillSpecificMob` hedefi görevin seviyesinde **her iki ırkın** erişebildiği bir bölgede doğuyor; (4) Lv1-80 her seviye her ırk için bir bölgede; (5) ırk haritası çiftleri (H2/H3, H4/H5) aynı arketip kümesini üretiyor (ayna eşitliği); (6) H6 bandı giriş seviyesinin (55) altına inmiyor | Hata yok + her kural için negatif test |
| Irk native testi | Yeni `Scripts/TestRace.ps1` (`TestAchievements.ps1` deseni) | Seçim bir kez; ikinci seçim reddedilir; `None` iken H1 dışı geçiş reddedilir; karşı ırk haritasına geçiş reddedilir; yan dosya `.tmp`/`.bad` (ara yol); karşı ırka posta, parti ve fısıltı reddedilir; H6'da karşı ırka hasar var, aynı ırka yok |
| Ölüm cezası | `TestRace` PvP senaryosu + mevcut `MMOReentrantDeathSelfTest` gerilemesi | Oyuncu öldürünce kayıp = %3 `[PLACEHOLDER]` × `ExperienceToNextLevel`, mob öldürünce %10; ikisinde de seviye düşmez; çanta/kuşanılan/altın değişmez (K5) |
| DB mağazası | Yeni `Scripts/TestDbStore.ps1` (yerel PostgreSQL'e karşı) | JSON gidiş-dönüş eşitliği (mevcut `TestCharacterSaveJson` fikstürü), eski `revision` ile yazım reddi, DB kapalıyken kayıt başarısız ve oyuncu durumu ilerlemiyor, `.sav` içe aktarma bir kez |
| Kimlik (Steam, S12) | `Scripts/TestAccountLogin.ps1` (`ACCOUNT_IDENTITY_DESIGN.md` §7.3 fikri, Steam'e uyarlanmış). Servis testleri Steam Web API'yi **sahte yanıtlayıcıyla** çalıştırır; sahte yanıtlayıcı yalnız test yapılandırmasında açılabilir | Steam'in reddettiği bilet, Steam'e ulaşılamıyorken giriş (kapalı başarısızlık: reddedilir), sahte/süresi geçmiş/tekrar kullanılan **servis** bileti, aynı SteamID'ye ikinci giriş, başka SteamID'nin karakter adı, (açıksa) `CheckAppOwnership` olumsuz: hepsi reddedilir. Mutasyon: sahte yanıtlayıcı üretim yapılandırmasında açılmıyor |
| Steam gerçek uçtan uca | Elle (A5 kabulü), Steam istemcisi açık iki hesapla | Gerçek bilet → servis → SteamID → dedicated'a giriş; Publisher Key istemci ve harita sunucusu dosyalarında yok (grep) |
| Çok sunucu geçişi | Yeni `Scripts/TestCrossServerTravel.ps1` (iki headless sunucu + iki istemci) | İki oyuncu **aynı anda** iki farklı haritada; A H1→H2 geçerken B H1'de kalıyor; seviye/altın/envanter/görev/ırk korunuyor; aynı bilet ikinci kez reddediliyor; kilitli karakterle ikinci sunucuya giriş reddediliyor; hedef sunucu kapalıyken geçiş reddediliyor ve oyuncu kaynakta kalıyor |
| Mutasyonlar | `test-muhendisi` | Irk kapısı, `Race == None` kontrolü, H6 aynı-ırk hasar kontrolü, posta ırk kontrolü, paket-var kontrolü, **bilet tek kullanım**, **karakter kilidi**, **`revision` karşılaştırması**, **pazar `status='open'` koşulu**, **PvP ceza oranı ayrımı** silinince ilgili test **kalmalı** |
| Güvenlik | `ag-guvenlik-denetcisi` | `ServerChooseRace`, bilet/kilit, `PreLogin`, pazar satın alma, posta ırk kontrolü, DB kimlik bilgilerinin ve **Steam Publisher Key**'in depoya, istemciye ve harita sunucusuna girmemesi |

---

## 6. H6 PvP haritası: karar listesi

| # | Karar | Seçenekler | Durum / değer `[PLACEHOLDER]` |
|---|---|---|---|
| K1 | Giriş seviyesi | 25 / 35 / 55 / 59 | **KARAR (2026-10-13): Lv55.** Krallık haritasının son bölümü (Buzbağlı, Lv51-57) ve üst bölgedeki oyuncular buluşur |
| K2 | PvE mob bandı | Tek bant / karışık / oyuncu seviyesine göre | **Revize öneri: Lv55-72 karışık, 5 cep + 1 elit.** Cepler (mevcut arketipler, `CONTENT_MAP.md`): FrostboundSentinel (Lv55), RiftSeeker (58), RiftKeeper (62), AshBinder (68), AbyssStrider (72). Elit: **RiftHerald (Lv65)** iki kampa eşit uzaklıkta, çekişme noktası. Lv73-80 cep yok: H4/H5'in son bandını H6'ya kopyalamak üst bölgeyi boşaltır. Lv55 altı cep yok: giriş 55. XP seviye farkıyla zaten ölçekleniyor (`ScaleXPForLevelGap`), Lv55 oyuncusu Lv72 cebinde düşük verimle karşılaşır [Muhtemel]. Ölçüm: `[MMOMETRIC]` öldürme satırlarında `zone=PvP` oranı |
| K3 | Irk düşmanlığı | Yalnız karşı ırk / herkes herkese | Yalnız karşı ırk. Aynı ırka hasar yok, aynı ırk parti kurabilir |
| K4 | PvP ölüm cezası | XP kaybı var / yok | **KARAR (2026-10-13): XP kaybı var.** Öneri: oyuncu öldürünce `ExperienceToNextLevel × 0,03` `[PLACEHOLDER]`, mob öldürünce bugünkü `× 0,10` (`DeathXPPenaltyPct`). Tavan: mevcut `Experience` (seviye düşmez, bugünkü `FMath::Min`, `MMOStatsComponent.cpp:551`). Öldüren XP **kazanmaz** (işbirlikçi XP transferi yok). Ayrım: son hasar veren (`LastDamageInstigator`, `MMOCharacter.cpp:1893` civarı) bir oyuncu karakteriyse PvP oranı, değilse (mob, kaynağı çözülemeyen DoT, düşme) mob oranı. Uygulama: `UMMOStatsComponent`'e `PvPDeathXPPenaltyPct` alanı + `ApplyDeathPenalty(bool bKilledByPlayer)` (eski imza mob oranına yönlenir); `[MMOMETRIC] ev=death` satırına `byplayer=0/1` alanı. Neden mobdan küçük: PvP haritası Lv55 oyuncusunu Lv70'e karşı koyar, aynı ceza H6'yı ilk haftada boşaltır [Tahmin]; ölçümle ayarlanır |
| K5 | Ganimet | Eşya düşer / altın düşer / hiçbir şey | **KARAR (2026-10-13): hiçbir şey düşmez** (eşya da altın da). Kod işi yok; yalnız test (§5 "Ölüm cezası") |
| K6 | PvP ödülü | Ulus puanı / başarım / altın | **Yalnız başarım sayacı** (mevcut `UMMOAchievementComponent`'e "karşı ırktan N öldürme"). Altın ödülü işbirlikçi öldürme sömürüsüne açık. **Değmez** |
| K7 | Yeniden doğuş | Kendi kampı | Kendi ırkının kampı (etiketli PlayerStart) + mevcut doğuş koruması (`OnRep_SpawnProtected`) |
| K8 | Kamp güvenliği | Kamp güvenli bölge mi | Evet, iki kamp güvenli bölge. Bugünkü PlayerStart tabanlı güvenli bölge iki PlayerStart'la kendiliğinden çalışır [Muhtemel, `NETWORK_AUDIT.md` D10] |
| K9 | Global `bAllowPvP` | Kalsın / kalksın | Test anahtarı olarak kalır. Bölge kuralı önceliklidir |
| K10 | Kamp başında bekleme (spawn camping) | Önlem / yok | Alfada gözlem. Kamp güvenli bölgesi yeterli olabilir |

---

## 7. Kapsam: risk notu

Kullanıcı kararları (2026-10-12 ve 2026-10-13): **6 harita + ırk sistemi + pazar aynen uygulanır, iki ırk aynı anda farklı haritalarda olur,
kapsam küçültülmez, gerekirse süre uzar.**
Kalan riskler:
- **Altyapı yükü içerik yükünü geçti:** planın en uzun ve en belirsiz kısmı artık harita değil; kaynak motor, dedicated derleme, DB katmanı,
  Steam kimliği, hesap ve oyuncu başına geçiş (A1-A5, toplam 11-21 hafta [Tahmin]). Bunların hiçbiri bugüne kadar bu projede derlenip çalıştırılmadı.
- **Yeni teknoloji yığını:** PostgreSQL işletimi (kurulum, yedek, şema göçü), ayrı bir arka uç servisi (S13 (a), önerilen karar) ve Steamworks
  (partner hesabı, App ID, Publisher Key, Steam Sockets). Ajan rolleri (`mmo-developer`) bugün yalnız UE C++ ve Python betiklerini kapsıyor.
- **Steam'e bağımlılık:** alfa girişinin tamamı Steam'e bağlı olur (S12). Steam Web API erişilemezse kimse giremez (kapalı başarısızlık, bilerek).
  Steam Sockets 5.7'de relay'siz çalışmazsa (§1.4) taşıma IP sürücüsünde kalır.
- **Arazi tekrarı:** Winter arazisi 4 haritada (H2-H5), Summer 2'de (H1, H6). Özgün arazi sonradan harita harita yapılabilir
  (harita başına +3-6 gün [Tahmin]). Bölge tablosu değişmez, yalnız `MapAsset` ve o haritanın cep koordinatları yeniden ölçülür.
- **İçerik ikizliği:** İnsan/Ork haritaları aynı mob ve görevi taşır (P1). Irka özgü fark, NPC/metin/giydirme ile sınırlı kalır.
- **Alfa sonrasına kalanlar (bilerek):** aynı haritanın kanalları/kopyaları, otomatik sunucu aç/kapa, sunucular arası parti, offline pazar,
  hub NPC'siyle geçiş (alfa: menü/komut), internete açık sunucu sertleştirmesi (ağ şifrelemesi, DDoS).
- **Bu planın kapsamı dışında (açıkça, S17):** herkese açık Steam sürümü için kiralık 7/24 sunucu ve maliyeti, yük testi, destek/moderasyon,
  DDoS korumasının doğrulanması, Steam mağaza işleri (§12.3). Bunlar ayrı bir "yayın planı" ister.

---

## 8. Riskler ve açık sorular (kullanıcı kararları)

| # | Soru / risk | Durum / öneri |
|---|---|---|
| S1 | Oturum ırkı kuralı mı, harita başına sunucu mu? | **KARAR (2026-10-13): harita başına sunucu (B).** Oturum ırkı kuralı reddedildi. Sonuç: A1-A4 aşamaları, alfa ~6-10 ay [Tahmin]; revizyon 2 (Steam, A5): ~6,5-11 ay |
| S2 | Kopya + giydirme (Winter 4 haritada, Summer 2'de) kabul mü? | Öneri: kabul, sonra harita harita özgün araziye geçilir |
| S3 | Irk nerede saklanır? | **Revize:** hedef DB sütunu (`character_saves.race`). DB'den önceki ara yol: yan dosya + `.bad` durumunda kilit. `MMOSaveGame.h`'e dokunulmaz. Onay gerekir: ara yol kabul mü, yoksa ırk işi A2'ye kadar beklesin mi (o zaman Aşama 2 A2'den sonraya kayar, toplam süre değişmez, yalnız sıra) |
| S4 | H1'de karşı ırkla takas ve pazar serbest mi? | Öneri: serbest (ortak pazar = merkez haritanın anlamı). Parti/klan/posta yine aynı ırk |
| S5 | Pazar alfada şart mı? | **KARAR (2026-10-13): şart.** Aşama 8 alfa kapısında |
| S5b | Pazar yalnız çevrimiçi tezgâh mı, offline satış/açık artırma da mı? | Öneri: alfa = çevrimiçi tezgâh (KO). Offline satış alfa sonrası (satıcıya altın teslimi posta tablosu üzerinden yapılabilir) |
| S6 | İçerik P1 (aynı mob/görev) mi, P2 (iki metin sürümü) mi? | Öneri: P1, ardından P2 metin işi. P3 değmez |
| S7 | Ork oyuncunun Ork mob kesmesi | Öneri: "dönek ork" fraksiyonu (yalnız metin) |
| S8 | Seviye kapısı zorlayıcı mı? | Öneri: zorlayıcı, oyuncu başına. `[PLACEHOLDER]` 25/59, H6 55 (karar) |
| S9 | H6 kararları K1-K10 (§6) | K1, K4, K5 karar verildi. K2 revize öneri (Lv55-72), diğerleri öneri |
| S10 | Eski karakterler (ırksız) ilk girişte seçer | Öneri: kabul |
| S11 | 4 yeni `.umap` (~9-18 MB her biri, LFS yok) → repo ~50-70 MB büyür | Kullanıcı kararı: LFS ya da doğrudan |
| **S12** | **Arka uç ve kimlik: PostgreSQL mi PlayFab mı; parola mı Steam mi?** | **ÖNERİLEN KARAR (2026-10-13, kullanıcı "işle" dedi): PostgreSQL + kimlik = SteamID.** Parola/e-posta/sıfırlama yazılmaz (Steam sürümü). `accounts (provider, external_id)` tekil; `provider='steam'`, ileride başka mağaza yeni satır türüyle. K-1, karakter SteamID hesabına bağlanınca kapanır. Eski belgeler (`ACCOUNT_BACKEND_COMPARISON.md` §5 PlayFab önerir, `ACCOUNT_IDENTITY_DESIGN.md` parola tasarlar) bununla çelişir; `belge-denetcisi` "Steam sürümünde geçersiz" notu düşmeli |
| **S13** | **Oyun sunucusu DB'ye nasıl erişir?** (a) küçük arka uç servisi (HTTP+JSON; giriş servisiyle aynı süreç), (b) libpq'yu UE sunucu modülüne bağlamak | **ÖNERİLEN KARAR (2026-10-13): (a) kesinleşti.** Yeni ve belirleyici gerekçe: Steam bilet doğrulaması (`AuthenticateUserTicket`, `CheckAppOwnership`) Web API **Publisher Key** ister; bu anahtar gizlidir, 6 harita sunucusuna dağıtılmaz, istemciye girmez, tek servis tutar. Önceki gerekçeler de geçerli (pazar/kilit devri SQL işlemi tek yerde, DB parolası dağılmaz, `Build.cs`'teki `HTTP`/`Json` yeterli). **Açık kalan:** servis dili (varsayılan Python [Tahmin]; kullanıcı kararı, A2'den önce) |
| **S14** | Fısıltı ve klan sohbeti alfada sunucular arası mı? | Öneri: fısıltı ve `/who` sunucular arası (DB üzerinden, 1-2 sn gecikme kabul). Klan sohbeti alfa sonrası |
| **S15** | `UDatabaseMigrator`, `FPacketValidator`, `k6_load_test.js` ne olacak? | Öneri: migrator ve validator kullanılmaz (§1.5, sahte başarı / bağlanamaz); silmek ya da `docs`'ta "kullanılmıyor" diye işaretlemek kullanıcı kararı. k6 betiği A3'te giriş servisine uyarlanır |
| **S16** | Posta DB'de V1 `mail` tablosuna mı, mevcut `FMMOMailStore` JSON gövdesine mi? | Öneri: alfada JSON gövdesi (`character_side`, kind=`mail`), çünkü `.bak`/iade mantığı ve 130 kontrollük test bu gövdeyle yazılı. V1 `mail` alfa sonrası |
| **S17** | Alfa sunucuları nerede çalışır: kullanıcının makinesi mi, kiralık sunucu mu? | **ÖNERİLEN KARAR (2026-10-13): alfa = kullanıcının makinesi + Tailscale/ZeroTier ya da Steam Playtest** (§12.1). Altı süreç + PostgreSQL + servis tek makinede alfa grubu için yeterli olabilir [Tahmin]; A1/A4'te bellek ölçülür. Steam relay (`bAllowP2PPacketRelay`) sunucu adresini gizler ve port açma ihtiyacını azaltır; 5.7'de çalıştığı **[Doğrulanmadı]** (UE5.6 forum bildirimi), A1/A5'te denenir. **Herkese açık Steam sürümü: kiralık 7/24 sunucu zorunlu, bu planın kapsamı dışında** (§7, §12.3) |
| **S18** | **Steam-first mi?** Önce parola hesabı yazıp sonra Steam'e geçmek mi, doğrudan Steam bileti mi? | **ÖNERİLEN KARAR (2026-10-13): Steam-first.** A3 doğrudan Steam bileti üstüne kurulur (önce A5). Gerekçe: sonradan geçiş hesap tablosunu, giriş akışını ve kimlik testlerini yeniden yazmak demek; parola sistemi (özet, sıfırlama, kaba kuvvet koruması) Steam sürümünde atılacak iştir (sahte iş) |
| **S19** | Steamworks partner hesabı ve Steam Direct ücreti (100 USD) ne zaman? | Karar (kullanıcı, maliyet). A5'in gerçek kabulü (kendi App ID'si, Publisher Key, Playtest) bunu gerektirir. Ücretsiz deneme App ID'si (480, Spacewar) Steam Sockets denemesi için yeterli olabilir ama kendi Publisher Key'imizle bilet doğrulamaya yetmez **[Doğrulanmadı]**. Öneri: en geç A5 başında |
| R1 | O6 penceresi: zaman aşımından sonra uçuşta kalan yazım iptal edilemez (`NETWORK_AUDIT.md:303-331`, `MMOCharacterStore.h:69`) | **Revize:** DB'de `revision` karşılaştırması bu riski kapatır (eski yazım reddedilir). A2 kabul ölçütü |
| R2 | Yanlış harita yolu → geçiş kilidi kalıcı (`NETWORK_AUDIT.md:325`) | **Revize:** oyuncu başına geçişte hedef sunucu dizinden okunur; kapalı/yanlış hedef **geçiş başlamadan** reddedilir. Paket-var kontrolü listen-server yolunda kalır |
| R3 | Seamless travel'da parti/klan sürekliliği doğrulanmadı | **Revize:** `ClientTravel` ile sunucu değişince parti zaten dağılır (alfada kabul). Klan üyeliği kayıtta (`ClanName`), DB'den gelir. Test: geçişten sonra klan adı ve `/who` doğru |
| R4 | Codex'in parti/takas dosyalarına ırk kontrolü | Codex'le sıra veya devretme |
| R5 | Boş `TransitionMap` (`DefaultEngine.ini:7`) paketli yapıda | **Revize:** `ClientTravel` istemcide tam harita yüklemesidir; paketli istemcide bir geçiş testi Aşama 9 kabulünde |
| R6 | Kaynak motor derlemesi kullanıcının makinesinde başarısız olur ya da çok uzun sürer | A1 ilk iş; başarısızsa ara yol: editör ikilisiyle `-server` (§1.4) geliştirme sürer, alfa dağıtımı bekler |
| R7 | Server hedefinde derleme hataları (Slate sohbet kutusu, HUD, `IsLocallyControlled` geri bildirim kalıntıları; `NETWORK_AUDIT.md` O11 madde 2, 3, 8) | A1 kapsamında; süre aralığı bu belirsizliği taşır |
| R8 | 68 betikte sabit Launcher motor yolu: kaynak motora geçince `RunAllChecks` kırılır | A1'de tek noktadan motor yolu (§4.1 S5) |
| R9 | Çift giriş / eşya ikileme: aynı karakter iki sunucuda açık | Karakter kilidi + tek kullanımlık bilet + mutasyon testi (§5) |
| R10 | DB kimlik bilgisi, servis gizli anahtarı ya da **Steam Publisher Key** depoya, istemci paketine veya harita sunucusu yapılandırmasına girer | Yapılandırma `Saved/` ya da ortam değişkeni, yalnız servis makinesinde; `CheckPrPaths.py`'ye gizli anahtar deseni uyarısı önerilir. Publisher Key sızarsa Steamworks panelinden yenilenir [Doğrulanmadı] |
| R11 | `build.ps1 -Clean` `Saved/`'i siler (karakter verisi) | A1'de düzeltilir; o zamana kadar `-Clean` kullanılmaz |
| R12 | UE 5.7'de Steam Sockets/relay çalışmıyor (5.6 forum bildirimi) | A1 ölçümü + A5 kabulü. Çalışmazsa: Steam yalnız kimlik için (bilet Web API üzerinden, taşımadan bağımsız [Muhtemel]), taşıma IP sürücüsü + Tailscale/ZeroTier. Kimlik planı (S12/S18) bundan etkilenmez |
| R13 | Steam Sockets'e geçiş headless test zincirini kırar (Steam Sockets'li yapı yalnız Steam Sockets'li yapıyla bağlanır; headless süreçte Steam istemcisi yok) | Net sürücü değişikliği bayrak/yapılandırma arkasında; `NetSmokeTest`, `RunAllChecks`, `TestCrossServerTravel` IP sürücüsünde kalır. Steam yolu ayrı elle test listesiyle sınanır. Motorun IP sürücüsüne geri düşme (fallback) tanımı **[Doğrulanmadı]** |
| R14 | Steam Web API erişilemez ya da yavaş: kimse giriş yapamaz | Kapalı başarısızlık bilerek seçildi (güvenlik). Servis zaman aşımı ve istemciye anlaşılır hata ("Steam doğrulaması şu an yapılamıyor"). Ölçüm: `[MMOMETRIC] login ok/ms` |

---

## 9. Aşamalı yol haritası (asıl çıktı)

### 9.0 Kurallar

- **Kapı:** her aşama kısa bir kabul listesini geçmeden kapanmaz. İçerik aşamaları **iki pencereli PIE** (Editor Preferences > Play:
  *Number of Players = 2*, *Net Mode = Play As Listen Server*) ya da A4'ten sonra **iki dedicated sunucu + iki istemci** ile sınanır.
  Kapanmayan aşamadan sonrakine geçilmez. Sonuç geçti/kaldı şablonuyla `PLAYTEST_CHECKLIST.md` sonuna yazılır (yalnız ekleme).
- **Her aşama sonunda oyun uçtan uca oynanabilir:** yeni karakter Lv1'den başlar, mevcut en yüksek içeriğe kadar bir yol vardır. **Eski kayıtlar yüklenir:**
  her aşamada eski bir Summer ve bir Winter kaydıyla giriş yapılır, seviye/altın/envanter/görev korunur. A2'den sonra bu "eski `.sav` DB'ye
  içe aktarılır ve aynı kalır" demektir.
- **Mevcut Summer/Winter bozulmaz:** cep silmek ancak içeriğin yeni yeri kendi kapısını geçtikten sonra yapılır (önce ekle, sonra taşı, en son sil).
- **Yerel mod bozulmaz:** DB ve dedicated işi bayrak arkasındadır (`-MMOStore=db`, `-MMOAccounts`). Bayraksız listen-server yolu ve bugünkü
  `RunAllChecks` adımları her aşamada yeşil kalır.
- Ajan sırası CLAUDE.md'deki gibidir. Derleyen ajanlar (`mmo-developer`, `test-muhendisi`, `seviye-tasarimcisi`) aynı anda tek. Her kod
  aşamasında `qa-kontrolcu`; RPC/kayıt/altın/kimlik/DB değiştiyse paralel olarak `ag-guvenlik-denetcisi`.
- İki iş yeri (S1 = B ile çalışma yerelde): **Yerel ajan** = kod, derleme, headless test (Smoke/Soak/Probe/NetSmoke, native self-test),
  Python betikleri, metin (`metin-yazari`), belgeler. **Kullanıcı** = kaynak motor kurulumu, PostgreSQL kurulumu, editörde harita kopyalama/giydirme,
  PlayerStart, PIE oynama, `Config/` ve `.umap` onayı, kararlar.
- Süreler tek geliştirici ajan zinciri + kullanıcının editör/PIE zamanı içindir. Hepsi **[Tahmin]**. Kapıda kalan bir aşama +0,5-1 hafta ekler.

### 9.1 Sıranın gerekçesi

1. **Önce taban (Aşama 0):** Summer/Winter ve Summer↔Winter geçişi PIE'de hiç doğrulanmadı (`PROGRESSION_PLAN.md`, `PLAYTEST_PRIORITY.md`).
2. **En riskli bilinmeyen erken (A1):** kaynak motor + Server hedefi bu projede hiç derlenmedi. Bu başarısız olursa planın geri kalanı değişir.
   Erken öğrenmek ucuzdur. Ayrıca motor değişimi (motor yolu, `EngineAssociation`) bütün betikleri etkiler; harita işinin ortasında yapmak daha pahalı.
3. **Davranışı değiştirmeyen altyapı (Aşama 1):** yerleşim tablosu, sonraki her harita için tek eklenti noktası.
4. **Irk veri modeli (Aşama 2) DB'den önce ama arayüz arkasında:** geçiş, PvP ve sosyal kurallar ırka bağlı. Ara yan dosya A2'de DB'ye taşınır (S3).
5. **DB → Steam kimliği → hesap/bilet → çok sunucu (A2 → A5 → A3 → A4):** çok sunucu paylaşılan depo olmadan anlamsız; bilet/kilit kimlik olmadan güvensiz.
   A2 kaynak motor gerektirmez (editör yapısında, bayrakla sınanır), bu yüzden A1 takılırsa A2 yine ilerleyebilir.
   **A5 neden A3'ten önce (S18):** A3'ün hesap tablosu ve `PreLogin` doğrulaması SteamID üzerine kurulur; önce parola ile yazılsaydı Steam'e
   geçişte yeniden yazılırdı. "A5" adı eklenme sırasıdır, yürütme sırası değil.
6. **Kapılı geçiş (Aşama 3) A4'ten sonra:** kapı oyuncu başına yazılır; A4'ten önce yazılsaydı listen-server'daki "herkes birlikte" kapısı
   bir kez yazılıp atılırdı (sahte iş).
7. **Önce tek ırk (İnsan) uçtan uca (Aşama 4-5):** mevcut Lore İnsan-merkezli. Ork seçimi bu sırada **kapalı** (sunucu reddeder).
8. **Ork + sosyal (Aşama 6), PvP (Aşama 7), pazar (Aşama 8), sertleştirme (Aşama 9).** Pazar şart (S5), alfa kapısında.
9. **Paralel yürüyebilen kullanıcı/metin işi:** harita kopyaları ve giydirme (editör), Lore ve NPC metinleri (`metin-yazari`) altyapı
   aşamalarıyla aynı anda yapılabilir; derleme gerektirmez. Bu, toplam süreyi en iyi durumda 2-4 hafta kısaltır [Tahmin].

**Sıra (bağımlılık zinciri):**

```
0 Taban -> A1 Kaynak UE + Server -> 1 Yerlesim -> 2 Irk -> A2 PostgreSQL depo -> A5 Steam entegrasyonu
        -> A3 Hesap (SteamID) + bilet -> A4 Harita basina sunucu
        -> 3 Kapili gecis -> 4 HumanHigh -> 5 Insan yolu -> 6 Ork + sosyal -> 7 PvP -> 8 Pazar -> 9 Alfa sertlestirme
Paralel (derlemesiz): harita kopya/giydirme (4,5,6,7 icin), Lore/metin (5,6,7), ekonomi modeli (8),
                      Steamworks hesap/App ID/Playtest basvurusu (kullanici, A5'ten once; S19)
A2, A1 tamamlanmadan baslayabilir (editor yapisinda sinanir); A5 icin A1 (dedicated) + A2 (servis) sart;
A4 icin A1 + A2 + A5 + A3 sart.
```

### 9.2 Aşamalar

#### Aşama 0. Taban: yerelde derle + mevcut Summer/Winter PIE doğrulaması
- **Amaç:** bugünkü iki haritanın ve geçişin gerçekten çalıştığını kanıtlamak. Ayrıca Sprint 1 dosyalarının (§1.5) mevcut motorla **editör hedefinde** derlendiğini görmek.
- **Kapsam:** kod değişikliği yok. `PLAYTEST_RUNBOOK.md` akışı: derleme → `PR1_VERIFY` 1-18 → `FIRST_PLAYTEST` ilk 7 test → `[MMOMETRIC]`.
- **Yerel ajan:** `Build.bat` (editör), `RunAllChecks.ps1` (6 `TestZoneTravel*` dahil), `NetSmokeTest.ps1` her iki harita. Sonuçları `PLAYTEST_CHECKLIST.md`'ye ekler.
- **Kullanıcı:** iki pencereli PIE.
- **Bağımlılık:** yok.
- **Kabul:** derleme 0 hata (Sprint 1 dosyaları dahil); `RunAllChecks` yeşil. PIE: (1) iki pencerede Summer'da doğuş, 5 NPC tıklanıyor; (2) iki oyuncu mob kesiyor,
  görev sayacı artıyor, parti XP paylaşıyor; (3) host "Tüm oyuncuları Kış'a taşı" → iki oyuncu Winter'da, seviye/altın/envanter/görev aynı;
  (4) Winter'da 4 NPC ve mob; (5) Yaz'a dönüş; (6) oyun kapatılıp açılınca iki karakter son haritada yükleniyor; (7) parti geçişten sonra var mı, not.
- **Süre:** 0,5-1 hafta [Tahmin].

#### Aşama A1. Kaynak motor + Server hedefi + dedicated hazırlık (YENİ)
- **Amaç:** `MyProjectServer` ikilisinin derlenip Summer'ı dedicated olarak açması ve bir istemcinin bağlanması.
- **Kapsam:**
  - Kullanıcı: Epic hesabını GitHub'a bağlama, UE 5.7 kaynağını indirme, `Setup.bat`, `GenerateProjectFiles.bat`, motoru derleme; projeyi kaynak motora bağlama
    (`EngineAssociation` değişir). Disk ve süre ölçülüp belgeye yazılır (yüzlerce GB ve saatler mertebesi [Tahmin]).
  - Yerel ajan: `MyProjectServer.Target.cs`'i `Source/` köküne taşıma; betiklerde tek noktadan motor yolu (§4.1 S5); `build.ps1` düzeltmeleri (yol parametresi,
    `-Clean`'in `Saved/`'i silmemesi, olmayan Client hedefi); Server derleme hatalarını düzeltme (O11 madde 2, 3, 8; `R7`); dedicated'da yönetici için sunucu konsolu yolu;
    yeni `Scripts/DedicatedSmokeTest.ps1` (NetSmoke deseni: `MyProjectServer.exe Summer_scene` + bir headless istemci).
  - Ara yol ölçümü: kurulu motorda `UnrealEditor.exe ... -server` çalışıyor mu (§1.4). Çalışıyorsa A1 takılsa bile A4 geliştirmesi bununla sürer.
  - **Steam relay ön ölçümü (S17, isteğe bağlı, ana dala alınmayan deneme dalı):** kaynak motorla `OnlineSubsystemSteam` + `SteamSockets` açık bir deneme
    yapısında, deneme App ID'siyle (480) dedicated'a bir istemcinin relay'li bağlanıp bağlanamadığı. Sonuç yalnız belgeye yazılır (evet/hayır + log satırı);
    `Config/` değişikliği ana dala alınmaz. Amaç R12'yi erken öğrenmek **[Doğrulanmadı]**.
- **Bağımlılık:** Aşama 0.
- **Kabul:** (1) `MyProjectServer` Development Win64 derlemesi 0 hata; (2) editör hedefi kaynak motorla derleniyor ve `RunAllChecks` yeşil (motor yolu değişikliğinden sonra);
  (3) `DedicatedSmokeTest`: sunucu Summer'ı açıyor, mob/NPC doğuyor, istemci bağlanıyor, hata 0; (4) Winter için aynı; (5) iki sunucu süreci aynı makinede farklı
  portlarda aynı anda açık, bellek kullanımı ölçülüp yazıldı; (6) dedicated'da uzak istemci geri bildirim mesajlarını (kayıt, satın alma, ölüm) görüyor.
- **Süre:** 2-4 hafta [Tahmin] (motor derlemesi kullanıcının makinesinde; hata düzeltme sayısı bilinmiyor).
- **Kararlar:** S17 önerilen karar (2026-10-13): alfa kullanıcının makinesinde (Tailscale/ZeroTier ya da Steam Playtest). Bu aşamanın sonunda
  ölçülen bellek/CPU ile teyit edilir; relay ön ölçümü olumsuzsa Tailscale/ZeroTier yolu öne çıkar.

#### Aşama 1. Yerleşim tablosu (davranış eşdeğer)
- **Amaç:** harita eklemeyi tek tablo satırına indirmek. Oyuncu farkı yok.
- **Kapsam:** yeni `World/MMOZoneLayout.{h,cpp}` (`MakeZone`/`MakePocket`/`MakeNpc`; bölge satırında `ZoneId`, `MapName`, `Race`, `MinLevel`, `bPvPZone`, `ServerPort` alanları
  şimdiden), `SpawnMobArea` tek çağrıya iner. Yalnız Summer ve Winter satırları. `ValidateData.py`/`PocketSpacing.py` yeni kalıbı okur; negatif testleri güncellenir.
- **Yerel ajan:** C++ taşıma, derleme, Smoke iki harita, eşdeğerlik diff'i, Python betikleri. **Kullanıcı:** PIE.
- **Bağımlılık:** Aşama 0 (A1 ile sıra yalnız tek-derleyici kuralından).
- **Kabul:** headless: `[MMOLAYOUT]` Summer **18**, Winter **17** satır. Koordinat, bant ve MaxMobs refaktör öncesiyle bire bir. NPC 5/4. `RunAllChecks` yeşil.
  PIE: Aşama 0 listesinin 1-5. maddeleri aynen geçer.
- **Süre:** 1 hafta [Tahmin].

#### Aşama 2. Irk: veri modeli + seçim (iki mevcut haritada)
- **Amaç:** her karakterin değişmez bir ırkı olsun, kalıcı ve replike. Henüz harita veya kural etkisi yok.
- **Kapsam:** `EMMORace`, `UMMORaceComponent`, `IMMORaceStore` + yan dosya uygulaması (§3.1 I2), `ServerChooseRace`, HUD seçim paneli.
  Ork **seçilebilir değil** (panelde "yakında"; sunucu `Orc` isteğini reddeder). Irk ad etiketinde görünür. `Scripts/TestRace.ps1` (çekirdek senaryolar).
- **Yerel ajan:** kod, `TestRace`, mutasyonlar (`None` kontrolü, ikinci seçim reddi), NetSmoke (istemci ırkı görüyor). **Kullanıcı:** PIE.
- **Bağımlılık:** Aşama 1.
- **Kabul (PIE):** (1) yeni karakterde panel çıkıyor, İnsan seçiliyor; (2) öbür pencerede ırk etiketi görünüyor; (3) yeniden bağlanınca ırk aynı, panel yok;
  (4) **eski kayıt** (ırksız) yükleniyor, panel bir kez çıkıyor, seviye/altın/envanter korunuyor; (5) Summer↔Winter geçişinden sonra ırk aynı.
- **Süre:** 1-1,5 hafta [Tahmin].
- **Kararlar (önce):** S3 (ara yan dosya kabul mü), S10, ırkın değiştirilemezliği.

#### Aşama A2. PostgreSQL karakter deposu (YENİ; Sprint 1 şemasının üstüne)
- **Amaç:** karakter, ırk, başarım, günlük ve posta verisinin tek paylaşılan DB'de, yerel dosya yoluyla **aynı davranışla** durması. Hâlâ tek sunucu.
- **Kapsam:**
  - Kullanıcı: yerel PostgreSQL kurulumu (sürüm kararı), bir veritabanı ve kullanıcı; bağlantı bilgisi depo dışında (ortam değişkeni ya da `Saved/` altı yapılandırma).
  - Yerel ajan: S12/S13 önerilen kararına göre DB erişim katmanı = küçük arka uç servisi (A5'te Steam doğrulaması da bu servise eklenir; dil kararı
    gelene kadar varsayılan Python [Tahmin]); `V1` uygulanır (psql/Flyway), `V2__character_store.sql` (§1.5 önerisi, `accounts (provider, external_id)` dahil);
    `FMMODbCharacterStore` (`IMMOCharacterStore` uygulaması, asenkron, `revision`); `IMMORaceStore`, başarım/günlük/posta depoları için DB uygulamaları;
    `-MMOStore=db` bayrağı; tek seferlik içe aktarma (`.sav` + yan dosyalar → DB, hangi dosyanın aktarıldığı kayıtlı); `Scripts/TestDbStore.ps1`.
- **Bağımlılık:** Aşama 2 (ırk arayüzü). A1'in bitmesi **gerekmez** (editör yapısında sınanır).
- **Kabul:** (1) `TestDbStore`: JSON gidiş-dönüş eşit, eski `revision` reddediliyor, DB kapalıyken kayıt başarısız ve durum ilerlemiyor (mevcut "kayıttan sonra ödül" desenleri
  DB'de de geçerli: `TestAchievements`/`TestDaily`/`TestMail` DB modunda da yeşil); (2) eski bir Summer ve Winter `.sav`'ı içe aktarılıyor, alanlar bire bir;
  (3) PIE (`-MMOStore=db`): kayıt/yükleme, Summer↔Winter geçişi, posta gönderme; (4) bayraksız `RunAllChecks` değişmeden yeşil; (5) mutasyon: `revision` koşulu silinince test kalıyor.
- **Süre:** 3-5 hafta [Tahmin] (yeni servis projesi dahil; S13 (a) önerilen karar).
- **Kararlar (önce):** S12 ve S13 önerilen karar olarak verildi; servis dili, S15, S16 açık.

#### Aşama A5. Steam entegrasyonu (YENİ, revizyon 2; yürütme sırası A2'den sonra, A3'ten önce: S18)
- **Amaç:** istemci Steam ile oturum açmış oyuncunun SteamID'sini servise **kanıtlasın**; servis bunu Steam'e doğrulatsın. Taşıma, uygunsa Steam Sockets'e geçsin.
- **Kapsam:**
  - Kullanıcı: Steamworks partner hesabı, Steam Direct ücreti (S19), App ID, **Web API Publisher Key** (yalnız servis makinesine, depo dışı), Steamworks SDK indirme
    (motorla gelen Steamworks sürümüyle uyum **[Doğrulanmadı]**), test için iki Steam hesabı.
  - Yerel ajan, istemci/sunucu (UE): `MyProject.uproject`'e `OnlineSubsystemSteam` ve `SteamSockets` eklentileri; `Config/DefaultEngine.ini`'de online alt sistem = Steam,
    App ID, net sürücü = Steam Sockets, `bAllowP2PPacketRelay` (Config korunan yol: **yalnız yerelde**, kullanıcı onayıyla, tek commit; §4.2). İstemcide
    `ISteamUser::GetAuthTicketForWebApi` ile web API bileti alıp servise yollayan küçük sınıf (§4.3 D12). Bayraksız/headless yol IP sürücüsünde kalır (R13).
  - Yerel ajan, servis: `/login` ucu: istemciden gelen bileti `ISteamUserAuth/AuthenticateUserTicket` ile (`partner.steam-api.com`, Publisher Key) doğrular,
    64-bit SteamID alır; isteğe bağlı `ISteamUser/CheckAppOwnership` ile oyunun sahipliğini (Playtest için ayrı App ID durumu **[Doğrulanmadı]**) denetler;
    `accounts` satırını `(provider='steam', external_id=SteamID)` ile bulur ya da yaratır; **kendi** kısa ömürlü oturum biletini verir. Steam Web API'si için
    test yapılandırmasında sahte yanıtlayıcı (üretimde kapalı; mutasyonla sınanır).
- **Bağımlılık:** A1 (dedicated + kaynak motor), A2 (servis + `accounts`). S19 (kendi App ID'si ve Publisher Key; yoksa yalnız Steam Sockets kısmı 480 ile denenir).
- **Kabul:** (1) servis testi (sahte yanıtlayıcı): geçerli bilet → doğru SteamID; Steam reddi → giriş reddi; Steam'e ulaşılamıyor → giriş reddi (kapalı başarısızlık);
  (2) **gerçek uçtan uca (elle):** Steam açık iki hesap, iki istemci; ikisi de servisten bilet alıyor, servis logunda iki farklı SteamID; (3) Steam Sockets ile dedicated
  Summer'a **relay'li** bağlantı ve (ayrı denemede) relay'siz bağlantı sonucu belgeye yazıldı (5.7 durumu, R12); çalışmazsa IP sürücüsü kararı yazıldı;
  (4) `grep`: Publisher Key istemci paketinde, harita sunucusu yapılandırmasında ve depoda yok; (5) bayraksız `RunAllChecks` ve `NetSmokeTest` değişmeden yeşil;
  (6) `ag-guvenlik-denetcisi` bulgusu kapalı.
- **Süre:** 2-4 hafta [Tahmin] (Steamworks hesap/onay bekleme süresi ve 5.7 Steam Sockets durumu bilinmiyor; üst sınır bunu taşır).
- **Kararlar (önce):** S18 (önerilen: Steam-first), S19 (ücret zamanı), `CheckAppOwnership` açık mı (öneri: alfada açık, Playtest App ID'siyle uyumu doğrulanınca).

#### Aşama A3. Hesap (SteamID), karakter sahipliği ve geçiş bileti (K-1, YENİ; revizyon 2'de kısaldı)
- **Amaç:** bir adı yalnız sahibi (SteamID hesabı) kullanabilsin; harita sunucusu bağlananın kim olduğunu bilsin. K-1 kapanır.
- **Kapsam:** parola, e-posta, parola sıfırlama, kaba kuvvet koruması **yok** (S12). `ACCOUNT_IDENTITY_DESIGN.md` B-min'den yalnız sahiplik/bilet/kilit fikri,
  DB'de: karakter → `account_id` bağı; servis oturum biletiyle karakter listesi/yaratma ve karakterin harita sunucusu adresi; `PreLogin`/`PreLoginAsync` servis bileti
  doğrulaması (§4.3 D6); karakter kilidi alma; eski karakterleri bir SteamID'ye sahiplendirme (yönetici komutu); istemci giriş ekranı **parolasız** (Steam oturumu
  kendiliğinden; en küçük: karakter seç/yarat, Canvas HUD deseni); `accounts.is_admin` ile dedicated yöneticisi (D2); `k6` betiğinin `/login` ucuna uyarlanması
  (sahte yanıtlayıcıyla).
- **Bağımlılık:** A5 (SteamID), A2, A1 (dedicated'da `PreLogin` testi için).
- **Kabul:** (1) `TestAccountLogin` (§5 "Kimlik" satırı): sahte/süresi geçmiş/tekrar kullanılan servis bileti, aynı SteamID'ye ikinci giriş, başka SteamID'nin karakter adı reddediliyor;
  (2) dedicated Summer'a gerçek Steam hesabıyla giriş, karakter DB'den yükleniyor; (3) eski karakter bir SteamID'ye sahiplendirilip girişte aynı geliyor;
  (4) bayraksız listen-server yolu değişmedi; (5) mutasyon: `PreLogin` sahiplik kontrolü silinince test kalıyor; (6) güvenlik denetimi bulgusu kapalı.
- **Süre:** 1-2 hafta [Tahmin] (önce 2-4; parola sistemi düştü, Steam kısmı A5'te).
- **Kararlar:** hesap (SteamID) başına karakter sayısı (öneri alfa: 1-2), karakter yaratma ekranında ırk seçimi mi yoksa ilk girişte mi (öneri: ilk giriş paneli kalır).

#### Aşama A4. Harita başına sunucu + oyuncu başına geçiş (YENİ; S1 = B'nin kendisi)
- **Amaç:** Summer ve Winter **iki ayrı dedicated sunucuda aynı anda** çalışsın, oyuncular birbirinden bağımsız geçsin.
- **Kapsam:** §4.3 D7-D11: `RequestPlayerZoneTravel`, kilit devri + tek kullanımlık bilet, `ClientTravel`, sunucu dizini ve kalp atışı, kilit kira süresi ve
  çökme kurtarma, `presence` ile `/who`, arkadaş durumu ve (S14) fısıltı; `StartZoneServers.ps1`, `TestCrossServerTravel.ps1`. Eski toplu geçiş listen-server'da kalır.
- **Bağımlılık:** A1, A2, A5, A3.
- **Kabul:** headless `TestCrossServerTravel` (§5 satırı) tamamen yeşil (IP sürücüsü, R13); mutasyon (bilet tek kullanım, kilit, hedefte kapı yeniden denetimi). İki makine gerekmez.
  Steam yolu A5'te çalıştıysa ek elle test: Steam Sockets + relay ile iki dedicated arasında `ClientTravel` (Steam adresiyle hedef) geçiyor **[Doğrulanmadı]**; geçmiyorsa
  sonuç ve kullanılan yedek (IP + Tailscale/ZeroTier) belgeye yazılır.
  Oyun içi (iki istemci penceresi, iki dedicated): (1) A Summer'da, B Winter'da aynı anda kasıyor; (2) A Winter'a geçiyor, B'yi görüyor; (3) B Summer'a dönüyor,
  `/who` ikisini doğru haritada gösteriyor; (4) geçiş sırasında hedef sunucu kapatılınca A kaynakta kalıyor, kayıp yok; (5) sunucu süreci zorla kapatılınca
  karakter kira süresinden sonra diğer sunucuya girebiliyor, kayıp ≤ son otomatik kayıt aralığı.
- **Süre:** 3-6 hafta [Tahmin].
- **Bu aşama sonunda:** teknik iç test noktası. İki ırk henüz yok ama "iki oyuncu aynı anda farklı haritada" gereksinimi kanıtlanmış olur.

#### Aşama 3. Bölge tablosu ve oyuncu başına kapılı geçiş (hâlâ 2 harita)
- **Amaç:** N-haritalı geçiş: hedef menüsü, ırk ve seviye kapısı (oyuncu başına), hedef sunucuda yeniden denetim.
- **Kapsam:** `MMOTravel <Bolge>`, HUD hedef listesi (her oyuncunun kendi menüsü, yönetici gerekmez), §2.6 kapıları, sunucu dizininden hedef. Yeni self-test `MMOTestZoneTable`.
  Summer = H1 rolü, Winter = geçici İnsan krallığı rolü. İçerik ve cep değişmez.
- **Bağımlılık:** A4.
- **Kabul:** (1) her istemci kendi menüsünü görüyor; (2) Lv<25 oyuncu Winter'a gidemiyor, aynı anda Lv25+ oyuncu gidebiliyor (birbirini etkilemiyor); (3) ırk seçmemiş oyuncu reddediliyor;
  (4) kaynak sunucu atlatılıp sahte biletle hedefe bağlanılınca hedef reddediyor; (5) eski kayıtla giriş korunuyor.
  **Not:** Winter içeriği hâlâ Lv42-80. Lv25-41 oyuncusu Summer'daki 25-40 ceplerinde kalır, oyun uçtan uca oynanabilir.
- **Süre:** 0,5-1 hafta [Tahmin] (taşıma A4'te yazıldı, burada yalnız kural ve menü).
- **Kararlar:** S8.

#### Aşama 4. H4 İnsan üst bölge (`HumanHigh_scene`), ilk yeni harita
- **Amaç:** ilk yeni haritayı mevcut haritalara dokunmadan, kendi sunucu sürecinde eklemek. Lv59-80 İnsan içeriği burada.
- **Kapsam:** Winter kopyası, 13 cep (Kül/Uçurum B-konumları aynen + Yarık 59-64 ve Haberci A-konumlarında), 4 NPC; bölge satırı + sunucu dizini satırı.
  Winter ve Summer'daki eski cepler **yerinde kalır** (geçici ikizlik serbest; `ValidateData` ikizliği uyarı sayar).
- **Yerel ajan:** tablo satırları, `MapsToCook` (Config, kullanıcı onayıyla), genelleştirilmiş navmesh betiği, Smoke/Probe/Soak, giydirme prompt'ları, metin.
- **Kullanıcı (editör):** `Winter_scene` → `Maps/HumanHigh_scene/HumanHigh_scene` kopyası (derlemesiz, A-aşamalarıyla paralel yapılabilir), giydirme, PCG dışlama
  (her cep merkezinde yarıçap + 450 u boş), tek PlayerStart, oyun içi test.
- **Bağımlılık:** Aşama 3.
- **Kabul:** headless: 13 cebin her biri probe ≥ 60/64, Soak 90 sn takılma ≤ 1, ilk doldurma ≥ 4/6, Smoke 0 hata, üç sunucu aynı anda açık.
  Oyun içi: (1) Lv59+ İnsan Winter→HumanHigh geçiyor, başka oyuncu Winter'da kalıyor; (2) 4 NPC çalışıyor; (3) Lv66 ve Lv72 cebinde mob kesiliyor, görev sayacı artıyor;
  (4) HumanHigh→Summer dönüşünde kayıt korunuyor; (5) çıkış/giriş sonrası karakter HumanHigh sunucusuna yönleniyor.
- **Süre:** 1-1,5 hafta [Tahmin].
- **Kararlar:** S2, S11, harita görünen adları.

#### Aşama 5. İnsan yolu tamam: Winter = Ravenmoor (Lv25-58), Summer = merkez (Lv1-24)
- **Amaç:** İnsan için 3 haritada Lv1-80 uçtan uca, kopya içerik yok.
- **Kapsam:** Winter yerleşimi §2.2 H2. Summer'dan 7 cep çıkar (11 kalır). Winter'dan 66-80 çıkar. Winter için rota açıklığı ölçümü.
- **Yerel ajan:** tablo değişiklikleri, probe (8 yeni bant-konum ataması), Soak iki harita, `ContentMap.py --write`, `DocCounts`, rota ölçüm betiği. **Kullanıcı:** oyun içi test.
- **Bağımlılık:** Aşama 4.
- **Kabul:** headless: Summer 11, Winter 16, HumanHigh 13 spawner. `ValidateData` kural (3)-(4) İnsan için hata yok. Probe ≥ 60/64. Takılma ≤ 1.
  Oyun içi: (1) Lv25 olan oyuncu Summer'dan Ravenmoor'a geçiyor; (2) Lv25 Obsidyen ve Lv31 Alacakaranlık cebi Ravenmoor'da; (3) Lv58 Arayıcı cebinde
  Lv59'a çıkış, HumanHigh'a geçiş; (4) **eski kayıt**: Summer'da Lv30 konumunda kaydedilmiş karakter yükleniyor ve Ravenmoor'a geçebiliyor; (5) Summer'da Lv25+ mob görünmüyor.
- **Süre:** 1 hafta [Tahmin].
- **Kararlar:** §2.2 bant eşlemesinin onayı, S6.
- **Bu aşama sonunda:** yalnız İnsan için 3 harita ve Lv1-80, ayrı sunucularda. İç test noktası, alfa değil.

#### Aşama 6. Ork: iki harita + ırk sosyal kuralları + metin
- **Amaç:** iki ırk, aynı anda. H1 ortak, H3/H5 Ork haritaları, karşı ırk kuralları.
- **Kapsam:** `OrcLand_scene` (H3) ve `OrcHigh_scene` (H5): Winter kopyaları, aynı cep satırları (ayna), kendi sunucu süreçleri. Ork seçimi açılır.
  Sosyal kurallar §3.2 (parti, klan, posta, fısıltı/sohbet kanalı, arkadaş, `/who`; H1 takası S4'e göre), sunucular arası olanlar DB üzerinden.
  Codex'in parti/takas dosyaları sırayla. `ValidateData` kural (5). Lore: dönek ork fraksiyonu, Ork yurdu adı, ırk-nötr görev metinleri (P1).
- **Yerel ajan:** tablo, sosyal kural kodu, `TestRace` sosyal senaryoları, mutasyonlar, `TestMail`/`TestTrade*`/`TestPartyConsent` gerilemesi, 2 harita probe/soak, metin.
- **Kullanıcı (editör):** 2 kopya + Ork giydirmesi, oyun içi test.
- **Bağımlılık:** Aşama 5.
- **Kabul:** headless: H3 16, H5 13 cep, probe/soak ölçütleri, ayna kuralı yeşil, beş sunucu aynı anda açık.
  Oyun içi: (1) bir pencere İnsan, bir pencere Ork; Summer'da ikisi de doğuyor; (2) **İnsan Ravenmoor'da, Ork OrcLand'de aynı anda** kasıyor (S1 = B kanıtı);
  (3) karşı ırka parti daveti, posta ve fısıltı reddediliyor; (4) Ork Ravenmoor'a gidemiyor; (5) Ork zinciri Summer→OrcLand→OrcHigh→Summer, kayıt korunuyor;
  (6) Summer'da karşı ırkla takas S4 kararına uygun.
- **Süre:** 2,5-4 hafta [Tahmin] (sunucular arası sosyal kurallar dahil).
- **Kararlar:** S4, S7, S14, Ork yurdu adı.

#### Aşama 7. H6 PvP haritası
- **Amaç:** iki ırkın Lv55'ten sonra savaştığı ve PvE yapabildiği ortak harita.
- **Kapsam:** `PvP_scene` (Summer kopyası), kendi sunucusu, ırk etiketli iki PlayerStart (`ChoosePlayerStart` override; `StaticLint` whitelist), iki kamp (kamp başına
  satıcı/görevli/şifacı, ırka bağlı hizmet), 5 PvE cebi + 1 elit (K2, Lv55-72), giriş kapısı Lv55 (K1), bölge PvP kuralı (`bPvPZone` + karşı ırk), PvP ölümünde
  %3 `[PLACEHOLDER]` XP kaybı ve mob ölümünde %10 (K4), hiçbir şey düşmez (K5), başarım sayacı (K6).
- **Yerel ajan:** kod, `TestRace` PvP senaryoları, mutasyon (aynı ırk hasarı, ceza oranı ayrımı, eşya/altın değişmezliği), probe/soak, `TestAchievements` gerilemesi, başarım metni.
- **Kullanıcı (editör):** kopya + giydirme, iki kamp alanı + iki PlayerStart (etiketli), oyun içi test.
- **Bağımlılık:** Aşama 6.
- **Kabul (oyun içi):** (1) Lv54 oyuncu reddediliyor, Lv55 giriyor; (2) İnsan ve Ork kendi kampında doğuyor; (3) kampta saldırı yok; (4) kamp dışında karşı ırka hasar var,
  aynı ırka yok; (5) PvP'de ölen `ExperienceToNextLevel`'ın %3'ünü kaybediyor, seviye düşmüyor, çanta/altın aynı; mobla ölen %10 kaybediyor; (6) kendi kampında
  yeniden doğuyor; (7) PvE cebinde mob kesiliyor; (8) diğer haritalarda oyuncu-oyuncu hasarı yok.
- **Süre:** 1,5-2 hafta [Tahmin].
- **Kararlar:** S9 (K2, K3, K6-K10).

#### Aşama 8. Pazar kurma (H1) — **alfa kapısının parçası (S5)**
- **Amaç:** merkez haritada oyuncu tezgâhı, DB destekli.
- **Kapsam:** `UMMOMerchantComponent` (§3.3), `V3__market.sql` (`auction_house` uyarlaması), tezgâh UI, eşya kilidi (tezgâhtaki eşya takas/posta/satıcı/depoya gidemez,
  tezgâh açıkken geçiş yok), satışta iki karakter kaydı tek DB işleminde, vergi sink'i, `EconomyModel.py` varsayımı, `[MMOMETRIC] market_sale`.
- **Yerel ajan:** ekonomi modeli (`ekonomi-tasarimcisi`: vergi, slot sayısı), kod, native test (ikileme senaryoları), mutasyonlar (eşya kilidi, `status='open'`, fiyat doğrulaması),
  güvenlik denetimi. **Kullanıcı:** oyun içi test (pazar alanı için prop isteğe bağlı).
- **Bağımlılık:** Aşama 6 (S4 kararı uygulanmış), A2 (DB).
- **Kabul (oyun içi):** (1) A tezgâh açıyor, B görüyor; (2) B satın alıyor, altın/eşya doğru, vergi düşülmüş; (3) tezgâhtaki eşya takasa/postaya/depoya konamıyor; (4) A bağlantıyı
  kesince tezgâh kapanıyor, satırlar `closed`, eşya A'da; (5) iki oyuncu aynı satırı aynı anda alınca yalnız biri alıyor; (6) sunucu kapatılıp açılınca iki kayıt doğru;
  (7) satış sırasında DB kapatılınca işlem geri alınıyor, ikileme yok.
- **Süre:** 2-3 hafta [Tahmin].
- **Kararlar:** S5b, vergi, slot sayısı, H1'de pazar alanı.

#### Aşama 9. Alfa sertleştirme
- **Amaç:** altı sunucunun birlikte, paketli yapıda, kayıp olmadan çalışması.
- **Kapsam:** paketli dedicated sunucu + paketli istemci (`MapsToCook` 6), `StartZoneServers.ps1` ile altı süreç, DB yedeği (günlük `pg_dump` `[PLACEHOLDER]`) ve geri yükleme denemesi,
  `RunAllChecks` 6 harita döngüsü, Soak 6 harita, `PLAYTEST_RUNBOOK` alfa sürümü, `CLAUDE.md` harita/sunucu listesi, `[MMOMETRIC]` `zone/race/server` alanları,
  eski toplu `ServerTravel` yolunun kaderi (yalnız test yolu olarak kalır ya da kaldırılır).
  **Dağıtım (S17, §12.1):** kapalı alfa istemcisi Steam Playtest ile (SteamPipe ile build yükleme) ya da Tailscale/ZeroTier ağındaki oyunculara doğrudan paket.
  Sunucular kullanıcının makinesinde; servis ve PostgreSQL de orada.
- **Yerel ajan:** tüm kontroller, paketleme, belgeler. **Kullanıcı:** iki makinede (sunucu makinesi + istemci) oturum; Steamworks panelinde Playtest ve SteamPipe yüklemesi.
- **Bağımlılık:** Aşama 7 ve 8.
- **Kabul:** `RunAllChecks` yeşil. Paketli yapıda iki makinede, altı sunucu açıkken: İnsan zinciri H1→H2→H4→H1, Ork zinciri H1→H3→H5→H1 **aynı anda**, karışık H1→H6→H1;
  pazarda bir satış; her geçişten sonra kayıt korunuyor; hata satırı 0; DB yedeğinden geri yükleme sonrası karakterler aynı;
  **dağıtım kanalından** (Playtest ya da Tailscale/ZeroTier) kurulan istemciyle, sunucu makinesi dışındaki bir makineden Steam girişi ve H1'e bağlantı.
- **Süre:** 2-3 hafta [Tahmin].

### 9.3 Kararların zamanlaması

| Karar | En geç bu aşamadan önce |
|---|---|
| S17 alfa sunucu yeri (önerilen karar verildi; A1 ölçümüyle teyit) | A1 sonu |
| S3 ırk ara yolu, S10 eski karakterler, ırk değiştirilemez | Aşama 2 |
| Servis dili (S13 kalan kısmı), S15 Sprint 1 artıkları, S16 posta biçimi | A2 |
| S19 Steamworks hesabı + Steam Direct ücreti, `CheckAppOwnership` açık mı | A5 |
| Hesap (SteamID) başına karakter sayısı | A3 |
| S14 sunucular arası fısıltı | A4 |
| S8 seviye kapısı | Aşama 3 |
| S2 kopya + giydirme, S11 LFS, harita adları | Aşama 4 |
| §2.2 bant eşlemesi, S6 içerik P1 | Aşama 5 |
| S4 karşı ırkla ticaret, S7 dönek ork, Ork yurdu adı | Aşama 6 |
| S9 (K2, K3, K6-K10) | Aşama 7 |
| S5b, pazar vergisi/slot/alan | Aşama 8 |

Verilmiş kararlar: S1 = B, K1 = 55, K4 = XP kaybı var, K5 = hiçbir şey düşmez, S5 = pazar şart (2026-10-13).
Önerilen kararlar (2026-10-13, "işle" ile kabul sayıldı): S12 = PostgreSQL + SteamID (parola yok), S13 = (a) ara servis, S17 = alfa kendi makine +
Tailscale/ZeroTier ya da Steam Playtest (herkese açık sürüm kapsam dışı), S18 = Steam-first.

### 9.4 Toplam süre ve alfa noktası (yeniden hesap, dürüst)

| Aşama | Süre [Tahmin] | Kümülatif |
|---|---|---|
| 0 Taban | 0,5-1 hf | 0,5-1 |
| **A1 Kaynak UE + Server** | **2-4 hf** | 2,5-5 |
| 1 Yerleşim tablosu | 1 hf | 3,5-6 |
| 2 Irk çekirdeği | 1-1,5 hf | 4,5-7,5 |
| **A2 PostgreSQL depo + servis** | **3-5 hf** | 7,5-12,5 |
| **A5 Steam entegrasyonu (yeni)** | **2-4 hf** | 9,5-16,5 |
| **A3 Hesap (SteamID) + bilet (K-1)** | **1-2 hf** (önce 2-4) | 10,5-18,5 |
| **A4 Harita başına sunucu** | **3-6 hf** | 13,5-24,5 |
| 3 Kapılı geçiş | 0,5-1 hf | 14-25,5 |
| 4 HumanHigh | 1-1,5 hf | 15-27 |
| 5 İnsan yolu tamam | 1 hf | 16-28 |
| 6 Ork + sosyal | 2,5-4 hf | 18,5-32 |
| 7 PvP | 1,5-2 hf | 20-34 |
| 8 Pazar | 2-3 hf | 22-37 |
| 9 Sertleştirme + Playtest dağıtımı | 2-3 hf | **24-40 hafta** |

Kapıda kalma payıyla (15 aşama × ortalama +0,25-0,5 hf ≈ +3,75-7,5 hf) gerçekçi aralık **~28-48 hafta, yani ~6,5-11 ay [Tahmin]**.
Revizyon 1'de ~26-45 hafta (~6-10 ay) idi. **Net Steam etkisi: +1-2 hf çekirdek, kapı payıyla +2-3 hf:** A5 +2-4 hf ekler, A3 parola sistemi düştüğü için
1-2 hf kısalır. Önceki plan ~12-16 haftaydı; fark neredeyse tamamen A1-A5'ten (11-21 hf) ve sunucular arası sosyal/pazar işinin büyümesinden geliyor.
Paralel editör ve metin işiyle (ve Steamworks hesabının A5'ten önce kullanıcı tarafından açılmasıyla) en iyi durumda 2-4 hafta kısalabilir.
Alt sınır; A1'in ilk denemede derlenmesine, servisin küçük tutulmasına ve Steam Sockets'in 5.7'de çalışmasına bağlı; bunlar tutmazsa üst sınır geçerlidir.
Steamworks tarafındaki onay/bekleme süreleri **[Doğrulanmadı]** ve bu aralığa dahil değil.

- **Alfa kapısı.** Kullanıcının tanımladığı kapalı alfa (6 harita, iki ırk aynı anda, PvP Lv55, pazar, **Steam ile giriş**): **yalnız Aşama 9 sonunda.**
  Pazar (S5) ve eşzamanlı ırklar (S1) karar olduğu için daha erken bir alfa noktası **yok**. Alfa kapısının şartları:
  Aşama 9 kabulü + A5 kabulü (SteamID ile giriş, Publisher Key yalnız serviste) + dağıtım kanalı hazır (Steam Playtest ya da Tailscale/ZeroTier, §12.1).
  Sunucular kullanıcının makinesindedir (S17). **Herkese açık Steam sürümü alfa kapısı değildir** ve bu plandan sonra ayrı bir işle gelir (§12.3).
- **İç test noktaları (alfa değil):** A3 sonu (~2,5-4,5 ay): Steam hesabıyla dedicated'a giriş, karakter DB'de. A4 sonu (~3-6 ay): iki oyuncu aynı anda iki haritada,
  kimlikli, DB'de. Aşama 5 sonu (~4-6,5 ay): İnsan Lv1-80 üç sunucuda. Aşama 7 sonu (~4,5-8 ay): altı harita ve PvP, pazar yok; S5 gereği alfa sayılmaz.
- **Alfa sonrasına kalan:** kanal/instance, otomatik sunucu ölçekleme, sunucular arası parti, offline pazar, internete açık sertleştirme (§7).

---

## 10. Ekonomi / denge etkisi

- **Altın kaynağı eklenmez.** P1 içerikte aynı moblar birden çok haritada doğar ama oyuncu yalnız birinde oynar. Kişi başı gelir değişmez.
  H6 PvE cepleri mevcut arketiplerin kopyasıdır; Lv55-72 oyuncusu için yeni bir kasma yeri, yani kasma **seçeneği** artar, kişi başı saatlik gelir artmaz [Muhtemel].
- **Yeni sink'ler:** pazar vergisi %2-3 `[PLACEHOLDER]` (Model: `EconomyModel.py` pazar hacmi varsayımı; ölçüm `[MMOMETRIC] market_sale`).
  **PvP XP kaybı (K4)** bir XP sink'idir: kaybedilen XP kimseye geçmez, böylece işbirlikçi XP basma yolu yoktur.
- **K5 etkisi:** ölümde eşya/altın düşmediği için PvP'de ekonomi sızıntısı yok; ikileme riski de yok.
- **PvP XP kaybı ölçüsü [Tahmin]:** %3 × `ExperienceToNextLevel`, Lv55-70 arasında bir seviyenin %3'ü. Bu, aynı seviyede ~1-2 mob öldürmeye denk gelir
  (`ProgressionModel.py` ile doğrulanmalı). Ölçüm: `[MMOMETRIC] ev=death byplayer=1 xploss=N` sayısı ve H6'da saat başı ölüm.
- **XP hızı [Tahmin]:** ırk haritalarında cepler hub'a 3-7 bin u (bugünkü Winter mesafeleri). Lv80 toplam süresi (~11 saat, `TIMELINE_MODEL.md`) değişmez.
  Harita geçişi artık oyuncu başına ve yükleme ekranlı; geçiş sayısı az (4-5 kez) olduğu için etkisi küçük.
- **Ölçüm:** `[MMOMETRIC]` `levelup` satırına `zone`, `race`, `server` alanı; `zone_travel from/to/ok/ms` olayı (oyuncu başına geçiş süresi); H6'da `pvp_kill` sayımı.
- **Steam (S12):** oyun içi ekonomiye etkisi yok (gerçek para, mikro ödeme, Steam envanteri/pazarı yok). Steam'in %30 payı ve 100 USD uygulama ücreti
  iş maliyetidir, oyun içi altın/XP dengesine girmez (§12.3).

---

## 11. Yerelde sıradaki ilk adımlar (sıralı)

| # | Adım | Kim | "Bitti" |
|---|---|---|---|
| 1 | `git pull` + `git status` temiz; editör kapalı; mevcut motorla editör hedefini derle (Sprint 1 dosyaları dahil) | Yerel ajan (`mmo-developer`), kullanıcının makinesinde | `Build.bat MyProjectEditor` 0 hata. Hata varsa önce o düzeltilir (Sprint 1 dosyaları hiç derlenmemiş olabilir) |
| 2 | Aşama 0 headless: `RunAllChecks.ps1`, `NetSmokeTest.ps1` Summer ve Winter | Yerel ajan (`test-muhendisi`) | Yeşil; sonuç `PLAYTEST_CHECKLIST.md`'ye eklendi |
| 3 | Aşama 0 PIE: `PLAYTEST_RUNBOOK.md` akışı, iki pencere, Summer↔Winter | **Kullanıcı** | Aşama 0 kabul listesi 1-7 geçti/kaldı yazıldı |
| 4 | Ara yol ölçümü: kurulu motorda `UnrealEditor.exe "<uproject>" Summer_scene -server -log` ile dedicated açılıyor ve bir `-game` istemci bağlanabiliyor mu | Yerel ajan (komut + log okuma), kullanıcının makinesinde | Evet/hayır + log satırı belgeye yazıldı (§1.4 "ara yol" etiketi güncellenir) |
| 5 | Kaynak motor kurulumu: Epic hesabı ↔ GitHub bağlantısı, UE 5.7 kaynağı, `Setup.bat`, `GenerateProjectFiles.bat`, motor derlemesi | **Kullanıcı** (uzun sürer; bu sırada 6-7 yürüyebilir) | Kaynak motorla boş bir proje ya da bu proje editörde açılıyor; disk/süre not edildi |
| 6 | Önerilen kararların (S12, S13, S17, S18) `DECISIONS_NEEDED.md`'ye işlenmesi; açık kararlar: servis dili (S13 kalan), S15 (Sprint 1 artıkları), S3 (ırk ara yolu), S19 (Steamworks ücreti zamanı). Eski belgelere (`ACCOUNT_BACKEND_COMPARISON.md`, `ACCOUNT_IDENTITY_DESIGN.md`) "Steam sürümünde parola yok" notu | **Kullanıcı** (karar), `belge-denetcisi` (yazım) | Karar satırları yazıldı; çelişen iki belgede not var |
| 7 | Yerel PostgreSQL kurulumu + `V1__initial_schema.sql`'i psql ile boş bir veritabanına uygulama (oyun kodu yok) | Kullanıcı kurar; yerel ajan uygular ve `\dt` çıktısını okur | Tablolar oluştu; şema farkları (§1.5) `V2` taslağı için listelendi (`accounts (provider, external_id)` dahil) |
| 8 | A1 başlangıcı (5 bittikten sonra): `MyProjectServer.Target.cs`'i `Source/` köküne taşı, motor yolunu tek noktaya al, `build.ps1 -Clean`'i düzelt, `MyProjectServer` derle | Yerel ajan (`mmo-developer`), sonra `qa-kontrolcu` | Server derlemesi sonucu (0 hata ya da hata listesi) belgeye yazıldı; `RunAllChecks` kaynak motorla yeşil |
| 9 | Steamworks hazırlığı (derlemesiz, 5-8 ile paralel): partner hesabı, Steam Direct ücreti ve App ID (S19), Web API Publisher Key'in nerede saklanacağı (servis makinesi, depo dışı); Playtest ücret/onay şartları ve `CheckAppOwnership`'in Playtest App ID'siyle davranışı panelden okunur | **Kullanıcı** | App ID var ya da S19 "sonra" kararı yazıldı; §12.1'deki **[Doğrulanmadı]** satırları panel bilgisiyle güncellendi |
| 10 | Steam relay ön ölçümü (A1 içinde, 8'den sonra; isteğe bağlı): deneme App ID'si (480) ile Steam Sockets'li deneme yapısı, dedicated + bir istemci, relay'li ve relay'siz | Yerel ajan (`mmo-developer`), kullanıcının makinesinde (Steam istemcisi açık) | Evet/hayır + log satırı §1.4'e yazıldı (R12). Deneme dalı ana dala alınmadı |

Notlar: adım 1-4 bugünkü motorla yapılır ve kaynak motora bağlı değildir. Adım 5 saatler sürer ve makineyi meşgul eder: o sırada derleyen ajan çalıştırılmaz,
yalnız okuyan ajanlar (tasarım, metin, denetim) çalışır. Adım 9 tamamen kullanıcı işidir ve her an yapılabilir; A5 bunu bekler.
Codex aynı ağaçta çalışıyorsa her adımdan önce `git status` bakılır, Codex'in dosyalarına dokunulmaz.

---

## 12. Steam yayını (revizyon 2, 2026-10-13)

### 12.1 Kapalı alfa dağıtımı (kapsam içi)

| Kanal | Ne | Şart / belirsizlik |
|---|---|---|
| **Steam Playtest** (öneri) | Steam'in yerleşik özelliği: ana uygulamaya bağlı ayrı bir Playtest uygulaması; oyuncular davetle/başvuruyla alınır, anahtar dağıtmak gerekmez (Steamworks duyurusu, §13) | Ana App ID gerekir (S19). Playtest'in ayrıca ücret/onay isteyip istemediği **[Doğrulanmadı]**: Steamworks panelinden bakılacak (§11 adım 9). Build SteamPipe ile yüklenir |
| Tailscale / ZeroTier | Oyuncular kullanıcının özel ağına katılır, sunucu makinesine doğrudan IP ile bağlanır; port yönlendirme yok | Her oyuncu istemci kurar. Steam girişi yine gerekir (S12); paketi elden dağıtmak Steam sahipliği kontrolüyle (`CheckAppOwnership`) çakışır, o yüzden bu kanalda sahiplik kontrolü kapalı ya da Playtest'le birlikte kullanılır **[Tahmin]** |
| Sunucu yeri | Kullanıcının makinesi: 6 harita süreci + servis + PostgreSQL (S17) | Bellek/CPU A1/A4'te ölçülür. Makine kapalıyken oyun kapalı (alfada kabul) |

### 12.2 Steam teknik entegrasyonu (kapsam içi, Aşama A5)

- Akış: istemci `ISteamUser::GetAuthTicketForWebApi` → servis `/login` → servis `ISteamUserAuth/AuthenticateUserTicket` (`partner.steam-api.com`, Publisher Key) →
  64-bit SteamID → (isteğe bağlı) `ISteamUser/CheckAppOwnership` → `accounts` satırı → servis oturum bileti → harita sunucusu `PreLogin` servis biletini doğrular.
- Harita sunucuları Steam biletini ve Publisher Key'i **hiç görmez**. Bu yüzden S13 (a).
- UE tarafı: `OnlineSubsystemSteam` + `SteamSockets` eklentileri, `Config/DefaultEngine.ini` (Steam online alt sistemi, App ID, Steam Sockets net sürücüsü,
  `bAllowP2PPacketRelay`). Steam Sockets'li yapı yalnız Steam Sockets'li yapıyla bağlanır (Unreal belgesi); bayraksız testler IP sürücüsünde kalır (R13).
- Bilinmeyenler: UE 5.7'de Steam Sockets relay'siz/relay'li durumu (R12), motorun dahili Steamworks SDK sürümü, Playtest App ID'siyle sahiplik kontrolü **[Doğrulanmadı]**.

### 12.3 Kapsam dışı (bu plan yapmaz; listelenir, ayrı iş)

| İş | Not |
|---|---|
| Steam Direct uygulama ücreti | 100 USD, uygulama başına; uygulama 1.000 USD brüt gelire ulaşınca geri ödenir (Steamworks App Fee belgesi, §13). Ödeme zamanı S19 |
| Gelir payı | Steam %30 [Tahmin: standart oran; kademeli indirim eşikleri bu belgede doğrulanmadı] |
| Mağaza sayfası | Açıklama, görseller, fragman, sistem gereksinimleri |
| Yaş derecelendirme | Steamworks'teki içerik anketi / derecelendirme [Tahmin: ayrıntılar panelden] |
| EULA ve gizlilik politikası | Hukuki metin; SteamID ve oyun verisi saklandığı için gizlilik politikası gerekir [Tahmin] |
| Vergi ve banka bilgisi | Steamworks partner kaydında |
| SteamPipe ile build yükleme | Alfada Playtest için de kullanılır (§12.1); herkese açık sürümde depolar/şubeler |
| Herkese açık sürüm altyapısı | Kiralık 7/24 sunucu ve maliyeti, yük testi (çoklu istemci + servis `/login`), destek/moderasyon, DDoS korumasının doğrulanması (relay dahil), yedek/izleme. Ayrı "yayın planı" gerekir (S17) |

---

## 13. Kaynaklar (revizyon 2)

Kullanıcının 2026-10-13'te doğrulanmış olarak verdiği kaynaklar. Bu belge yazılırken ajan tarafından yeniden açılıp okunmadı; belgedeki Steam bilgisi bu
kaynakların kullanıcı özetine dayanır. Özette olmayan ayrıntılar **[Doğrulanmadı]** ya da **[Tahmin]** etiketlidir.

- Steam kimlik doğrulama (Session Tickets + Web API; `GetAuthTicketForWebApi`, `ISteamUserAuth/AuthenticateUserTicket` → 64-bit SteamID, Publisher Key,
  `partner.steam-api.com`): https://partner.steamgames.com/doc/features/auth
- Steam Datagram Relay: https://partner.steamgames.com/doc/features/multiplayer/steamdatagramrelay
- Unreal Engine, Steam Sockets kullanımı (`bAllowP2PPacketRelay`, Steam Sockets'li yapıların yalnız birbirine bağlanması):
  https://dev.epicgames.com/documentation/unreal-engine/using-steam-sockets-in-unreal-engine
- Steam Direct uygulama ücreti (100 USD, 1.000 USD brüt gelirden sonra iade): https://partner.steamgames.com/doc/gettingstarted/appfee
- Steam Playtest duyurusu: https://steamcommunity.com/groups/steamworks/announcements/detail/3119280855648142184
- UE5.6 "Steam Networking relay'siz bozuk" forum bildirimi: bağlantısı kullanıcı özetinde yok **[Doğrulanmadı]**; A1/A5'te kendi ölçümümüzle yerine konur.
