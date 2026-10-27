# Planli kapatma / bakim modu (sunucu yasam dongusu) - tasarim

**Tarih:** 2026-10-27 · **Taban:** `1a67ec1` (master) · **Durum: TASARIM, kod yok, derleme yok.**
**L1 UYGULANDI (2026-10-27, izole worktree, PIE degil):** `Core/MMOServerLifecycle.*` (alt sistem, komut satiri + konsol, duyuru, PreLogin reddi, R1 kapisi), `Scripts/TestServerLifecycle.ps1` (137 vaka; ilk 90 vakada mutasyon 10 + R1 kapisi, QA/guvenlik turu vakalarinda 7 mutasyon yakalandi). Kayit turu/atma/cikis (L2/L3) yok; `Exiting`da surec SONLANMAZ. R1 sonucu: uzak istemci FSelfRegisteringExec komutuna ulasamaz (kaynak satirlari `MMOServerLifecycle.cpp` basindaki notta). Surec-geneli statik plan (bolge gecisi geri sayimi sifirlamaz).
**QA/guvenlik turu (2026-10-27, kararlar sahip adina):** (1) Statik plan yalniz gercek ServerTravel'da tasinir: `WorldType==PIE` dunyasinin `Initialize`/`Deinitialize`'inda plan, son tick zamani ve komut satiri bayragi sifirlanir (`ResetsPlanWithWorldType`); ilk tick (son tick zamani 0) yalniz saati kaydeder, iki oturum arasi bosta sure sayilmaz (`TickWithWallClock`). (2) **Kapanmayan sunucu onlemi:** `-MMOShutdownAfterSeconds` / `-MMOMaintenanceReason` yalniz `!UE_BUILD_SHIPPING`'de calisir (`IsCmdLineTriggerSupported`); Shipping'de anahtar varsa `Warning: ... bu surumde desteklenmez, L3 bekleniyor` loglanir, plan kurulmaz. `Exiting`e giriste `Warning` (`shutdown_exiting_not_implemented`) loglanir ve `IsTickable` `Exiting`de false. **L3 (cikis kodu) gelince bu Shipping kapisi ve Exiting uyarisi kaldirilacak.** (3) `OnWorldBeginPlay` PreLogin aboneligini eklemeden once eski handle'i kaldirir. (4) Duyuru, bu alt sistemin dunyasi + `GEngine` dunya baglamlarindaki tum istemci olmayan oyun dunyalarina gider (ServerTravel penceresinde duyuru kaybolmaz). (5) Komut satiri ayristirmasi saf `MMOServerLifecycle::ParseCmdLineShutdown` (bos/tirnakli/yalniz neden/aralik disi/gecersiz neden); reddedilen komut satiri `Error` loglanir, anahtar yine tuketilir. (6) Tick yolu `TickWithWallClock` ile sahte saatle test edilir. (7) Alici sayisi vakasi `MMOControllers>=1` on kosullu. (8) Konsol komutu game olmayan (editor) dunyadan gelirse oyun/PIE dunyasi aranir. `TestServerLifecycle.ps1` testin kasitli urettigi TAM 2 `shutdown_cmdline_rejected` Error satirini hata saymaz (sayi 2 degilse FAIL).
**Yazan:** `oyun-tasarimcisi`. Sayilar `[PLACEHOLDER]`; oynanista/uzun kosuda olculmedi.

## 1. Oyuncu deneyimi ve neden simdi

Sunucu sahibi bir bakim ya da surum guncellemesi icin sunucuyu kapatacagi zaman oyuncular once sohbet sistem satirinda
geri sayim duyurusu gorur ("Sunucu 10 dk sonra bakima girecek"), son dakikada yeni giris reddedilir ("Sunucu bakimda"),
kapanistan hemen once her karakter **kaydedilir** ve oyuncular nazikce atilir. Bugun bunun karsiligi yok: sunucuyu kapatmak
isleme oldurmektir; en iyi durumda son 60 sn'lik otomatik kayit (`AMMOCharacter` `AutoSaveTimerHandle`, 60 sn) kaybolur, en kotu
durumda yazim sirasinda kesilen kayit olur.

**Neden simdi:** (1) Kapali alfa yakin: tek sunucu, gercek oyuncular, kayip ilerleme en pahali hata. (2) Kalicilik katmani
(Supabase `ReleaseLeaseEx`/`RenewLeaseEx` slice 4, `docs/DESIGN_SUPABASE.md`; Codex S04) cikista **duzgun serbest birakma** ister;
bu sistem o hook'un yasayacagi yerdir. (3) Tamamen yeni dosyalarda yazilabilir; sicak dosyalara baglanti **sifir** (asagida).
(4) Kodda hic karsiligi yok (grep 2026-10-27: `shutdown|maintenance|bakim|MOTD|RequestExit` oyun kodunda 0 isabet; yalniz
arkadas "cevrimici" duyurusu ve mob dunya duyurusu var).

## 2. Kapsam / kapsam disi / mevcutla iliski

**Kapsam:** zamanlanmis kapatma durum makinesi; sistem sohbet satiri duyurulari; bakimda `PreLogin` reddi; kapanista toplu
kayit + atma + cikis; iptal; metrik/denetim satirlari; native self-test; dedicated duman testi.

**Kapsam disi:** HUD sayac bandi (`MMOHUD.*` kirli, S2 sonrasi); otomatik surum guncelleme; harita basina sunucular arasi
koordinasyon (A4); yonetici web paneli; zamanlayici takvimi (cron) - disaridan komut/arguman yeter; "bakim bitti" bildirimi.

**Mevcut sistemlerle iliski (var olan tekrar yazilmaz):**
- Kayit: `AMMOCharacter::SaveCharacterForTravel(bool bForce)` (public, `MMOCharacter.h:175`), `IsInitialLoadComplete()`,
  `IsTravelSaveCommitted()`, `IsSaveDeferredByTravel()`, `FlushDeferredSave(bool)`. `EndPlay` zaten `SaveCharacterForTravel(true)`
  yapar (`MMOCharacter.cpp:1097-1100`), yani kapanis **son savunma** olarak kayit yazar; bu sistem onun **onune** gecer ve basarisizligi gorunur kilar.
- Sohbet: `AMMOPlayerController::ClientReceiveChat(Sender, Message, 2)` (kanal 2 = sistem; mob dunya duyurusu `MMOMob.cpp:1058` ayni yolu kullanir). Yalniz cagrilir, PC dosyasi degismez.
- Bildirim: `MMONotify::ToOwner` pawn bazli; toplu duyuru icin kullanilmaz (PC dongusu daha dogru).
- Yetki: `MMOAdmin::IsServerAdmin(PC)` dedicated'da her zaman `false` (`MMOAdmin.h`); bu yuzden tetikleyici komut satiri/sunucu konsoludur (§5).
- Giris engeli: `AMMOGameMode::PreLogin` (`MMOGameMode.cpp:558`) zaten parola ve ad kontrolu yapar. **Dokunulmaz**; motorun
  `FGameModeEvents::OnGameModePreLoginEvent()` olayina (`GameModeBase.h:660`, `GameModeBase.cpp:697` `Super::PreLogin` icinde yayinlanir)
  abone olunur ve `ErrorMessage` doldurulur. Not: `AMMOGameMode::PreLogin` `Super::PreLogin` sonrasi `ErrorMessage` doluysa hemen doner (`:560-564`), yani bizim reddimiz parola/ad mantigindan once calisir ve onu atlamaz-bozmaz.
- Metrik: `Public/Core/MMOMetrics.h` yalniz `WhoName`/`Seconds` yardimcisidir; satirlar `UE_LOG` ile `[MMOMETRIC] t=.. ev=..` basilir (`ApplyGoldInternal` deseni). `PlaytestMetrics.py`'ye `ev=shutdown` ayri kucuk is (L5).

## 3. Dosya haritasi

### 3.1 Yeni dosyalar (hepsi su an var olmayan yollar)

| Dosya | Icerik | Dilim |
|---|---|---|
| `Source/MyProject/Public/Core/MMOShutdownPlan.h` + `Private/Core/MMOShutdownPlan.cpp` | **Saf** durum makinesi `FMMOShutdownPlan` (dunya yok, aktor yok): `Idle -> Counting -> Closing -> Flushing -> Kicking -> Exiting`, `Cancel`, esik dizisi, `Advance(DeltaSeconds)` ve olay listesi dondurur | L0 |
| `Source/MyProject/Public/Core/MMOServerLifecycle.h` + `Private/Core/MMOServerLifecycle.cpp` | `UMMOServerLifecycleSubsystem : UTickableWorldSubsystem` (`ShouldCreateSubsystem`: yalniz `NM_DedicatedServer`/`NM_ListenServer`, `IsGameWorld`); plan'i surer, PC'lere duyuru yollar, PreLogin olayina abone, kayit/atma/cikis yurutucusu | L1-L3 |
| `Source/MyProject/Private/Tests/MMOShutdownPlanSelfTest.cpp` | `#if !UE_BUILD_SHIPPING`, `FSelfRegisteringExec` komutu `MMOTestShutdownPlan` (saf kurallar, dunyasiz) | L0 |
| `Source/MyProject/Private/Tests/MMOServerLifecycleSelfTest.cpp` | `MMOTestServerLifecycle` (canli dunya, sahte pawn'lar) | L2-L3 |
| `Scripts/TestShutdownPlan.ps1`, `Scripts/TestServerLifecycle.ps1` | Kosucular, `Scripts/TestDaily.ps1` deseni (`-ExecCmds`, 90 sn watchdog, ozet = vaka sayisi, `Error:`/`Ensure`/`Fatal` = 0) | L0, L2 |
| `Scripts/TestShutdownSmoke.ps1` | Basliksiz dedicated + 1 istemci, `-MMOShutdownAfterSeconds=NN`, cikis kodu/log denetimi (L4) | L4 |

`Scripts/NetSmokeTest.ps1`'nin mevcut dosyasi **degismez**; yeni script onu kopyalayip kendi parametresini ekler.

### 3.2 Dokunulacak mevcut dosyalar

**Zorunlu mevcut-dosya degisikligi: 0.** Aboneliklerin hepsi motor olaylaridir, tetikleyici `FSelfRegisteringExec` + komut satiridir,
test komutlari kendi dosyalarinda kaydolur. Derleme `Build.cs` degisikligi gerektirmez (UBT yeni `.cpp`'leri kendisi bulur; `Source/MyProject/MyProject.Build.cs` bagimliliklari `Engine`/`Core` yeterlidir - dilimde dogrulanacak).

| Dosya | Degisiklik | Dilim | Cakisma durumu |
|---|---|---|---|
| `Scripts/RunSystemTests.ps1` | Diziye `TestShutdownPlan.ps1` + `TestServerLifecycle.ps1` adimi | L5 | **Kirli (baskasinin): bekle**; yerine `RunAllChecks.ps1` de sicak. Bu iki adim kendi basina `Scripts/Test*.ps1` olarak calisir; `RunSystemTests` baglantisi sahibi commit edince |
| `docs/PLAYTEST_CHECKLIST.md`, `Scripts/RunAllChecks.ps1` | Yalniz ekleme | L5 | Sicak, son adim |
| `Scripts/PlaytestMetrics.py` (+ `TestPlaytestMetrics.py` fixture) | `ev=shutdown` ozet satiri | L5 | Temiz (git status'ta yok) |
| `Scripts/StaticLint.py` | `OVERRIDE_WHITELIST`'e `UTickableWorldSubsystem` ezmeleri gerekirse (CLAUDE.md notu) | L1 | Temiz |

**Dokunulmaz (kirli ya da sicak):** `MMOCharacter.*`, `MMOPlayerController.*`, `MMOHUD.*`, `MMOGameMode.cpp`, `MMOStatsComponent.*`,
`MMOSkillComponent.*`, `MMOSaveGame.h`, `MMOTradeComponent.*`, `MMORace*`, `MMOZoneLayout*`, `Config/`, `Content/`.

### 3.3 Codex cakisma riski (`git status` + `git worktree list`, 2026-10-27)

- Kirli ana dizin: `RunSystemTests.ps1`, `MMOCharacter.cpp/.h`, `MMOPlayerController.cpp`, `MMOHUD.*`, staged `MMOTradeComponent.cpp`, `TestTradeTransfer.ps1`, `MMOTradeTransferSelfTest.cpp`; izlenmeyen `MMORace*`, `MMOZoneLayout*`, `TestRace.ps1`, `MMORaceSelfTest.cpp`, `.agents/`, `.codex/agents/*`, `nul`. **Bu tasarimin hicbir dosyasi bunlarla ortusmez** (§3.1 yollari mevcut degil).
- Acik Codex/cline calisma agaclari (`six-map-layout`, `trade-gold-atomicity`, `vendor-*-pagination`, `quest-window-rows`, `obsidian-spawn-validation`, `lv31-40-content`, `s04-p0-hardening`, `feature/warrior-skills`): yeni `Core/MMOShutdownPlan.*`, `Core/MMOServerLifecycle.*` adlari bu daglarda yok (gerekirse L0 oncesi `git -C <worktree> ls-files | grep -i lifecycle` ile yeniden bakilir). `s04-p0-hardening` kalicilik tarafini degistirir: L3'te `SaveCharacterForTravel` imzasi o dalda degisirse (S04) L3 bekler; L0-L2 etkilenmez.
- **Derleme sorunu:** ana dizinde izlenmeyen `MMOZoneLayout*`/`MMORace*` derlemeyi kirdi (bkz. `DESIGN_FUSION.md` §9). Derleyen ajan **izole worktree** kullanir (`fusion-d1` deseni) ve baskasinin kodu icin hata duzeltmez.
- Unity-build tuzagi: anonim namespace adlari `Lifecycle`/`Shutdown` onekli olacak (`ShutdownGraceSeconds`, ...). Log kategorisi `LogMMOLifecycle`.

## 4. Davranis (kurallar)

### 4.1 Durum makinesi (`FMMOShutdownPlan`, L0)

| Durum | Giris | Davranis |
|---|---|---|
| `Idle` | varsayilan, iptalden sonra | hicbir sey |
| `Counting` | `Schedule(Seconds, Reason)` | `Seconds` ∈ [`MinDelay` 30, `MaxDelay` 86400] `[PLACEHOLDER]`; disindaysa ret (kirpma yok). Esiklerde duyuru: `{3600,1800,600,300,120,60,30,10,5,4,3,2,1}` sn, yalniz kalan sure esikten kucukse ve o esik daha once yayinlanmadiysa (bir esik **tam bir kez**; planlama aninda zaten gecmis esikler atlanir, ilk duyuru her zaman gercek kalan sureyi soyler) |
| `Closing` | kalan <= `LoginCloseSeconds` 60 | `PreLogin` reddi baslar: `Sunucu bakima giriyor, daha sonra tekrar dene.` Bu durumda **iptal hala mumkun** (login tekrar acilir) |
| `Flushing` | kalan <= `FlushLeadSeconds` 20 | tek seferlik toplu kayit turu (§5.3); iptal artik **yok** (kayit turu baslamis olabilir, duzensiz durum yaratmamak icin) |
| `Kicking` | kalan <= 5 veya Flush bitti ve kalan <= 5 | tum oyuncular atilir, her biri sonra kayit sonucu raporlanir |
| `Exiting` | kalan <= 0 veya hic oyuncu kalmadiysa Flush sonrasi hemen | `FPlatformMisc::RequestExit(false)`; cikis kodu 0 (tum kayitlar basarili) ya da 2 (en az bir kayit basarisiz, `[MMOMETRIC] ev=shutdown_save_fail`) |

`Advance(Delta)` saf fonksiyondur: ayni (`Seconds`, `Delta` dizisi) girdisinden ayni olay listesi cikar; `Delta` buyuk olabilir (sunucu takilmasi)
ve **ara durumlari atlamaz** (tek `Advance` birden cok olay dondurur, sirasiyla). Negatif/NaN `Delta` yok sayilir.
Zaten `Counting` iken ikinci `Schedule` ret (yeni zamanlama icin once `Cancel`); `Cancel` yalniz `Counting`/`Closing`'de gecer.

### 4.2 Oyuncuya gorunen metin (Turkce ASCII, sabit; kullanici girdisi yankilanmaz)

`Bakim: sunucu N dakika icinde kapanacak. Ilerlemen kaydedilecek.` ·
`Bakim: sunucu N saniye icinde kapaniyor.` ·
`Bakim iptal edildi.` · `Bakim: yeni girisler kapandi.` · atma nedeni `Sunucu bakima girdi. Ilerlemen kaydedildi.` (kayit basarisizsa
`Sunucu bakima girdi.` - **"kaydedildi" yalan soylenmez**, kayit sonucu once okunur). `Reason` sabit sozlukten secilir (`bakim`, `guncelleme`, `acil`), serbest metin degil.

### 4.3 Yuklenmemis / geciste olan karakterler

- `IsInitialLoadComplete()==false`: kayit **yazilmaz** (mevcut dosyayi bos karakterle ezme korumasi `MMOCharacter.cpp:1093-1096` korunur); sayilir (`skipped_unloaded`) ve atilir.
- `IsTravelSaveCommitted()==true` (bolge gecisi onayli): kayit yazilmaz (altin/esya degisikligi kalici olmaz, `MMOCharacter.h:125`); karakter yine **atilir**; gecis kaydi zaten yazilmistir.
- `IsSaveDeferredByTravel()==true`: once `FlushDeferredSave(true)`; basarisizsa `failed`.
- Pawn'siz oyuncu (olu/spectator/henuz possess yok): atilir, kayit icin pawn yok = `skipped_nopawn` sayilir.

## 5. Sunucu-otorite, replikasyon, kimlik, kalicilik

- **Kim degistirir:** yalniz sunucu. Alt sistem `NM_Client`'ta olusmaz (`ShouldCreateSubsystem`). Istemciden hicbir RPC gelmez; **yeni RPC yok, yeni replike alan yok**. Duyuru mevcut `ClientReceiveChat` RPC'siyle gider.
- **Tetikleyici (kimlik/yetki):** (a) komut satiri `-MMOShutdownAfterSeconds=N` ve `-MMOMaintenanceReason=bakim` (sunucu sahibi baslatirken, uzak erisimsiz; L1'de yalniz !Shipping, L3'e kadar Shipping'de kapali); (b) sunucu konsolu/`FSelfRegisteringExec` komutlari `MMOShutdown <saniye> [neden]` ve `MMOShutdownCancel`.
  **Risk R1 (dogrulanacak):** motor `ServerExecRPC_Implementation` Shipping disi her yapida `ConsoleCommand` calistirir (`PlayerController.cpp:1879-1883`) ve `UPlayer::Exec` GameMode/GameState/HUD `ProcessConsoleExec` zincirine gider; uzak istemcinin kuresel `FSelfRegisteringExec`'e ulasip ulasmadigi motor kaynagindan okunarak kesinlestirilmedi. Kural: **uretim komutlari Shipping'de derlenmez** (`#if !UE_BUILD_SHIPPING`), yani Shipping dedicated'da yalniz komut satiri yolu vardir; Shipping disi derlemede L1'de bir test uzak-istemci benzetimiyle (`ServerExec` yolu, headless listen + istemci) komutun reddedildigini ya da dogrulanmis bir yetki kapisindan gectigini gosterir. Gosterilemezse komut `MMOAdmin::IsServerAdmin`'in sonucunu bilen bir kapiya tasinir ve L1 "bitti" sayilmaz.
  Listen-server'da yerel yonetici komutu `MMOAdmin::IsServerAdmin` ile ayni karar noktasini kullanir (yeni yetki mantigi yazilmaz).
- **Kimlik (K-1):** oyuncu adi hala kimlik; bu sistem ad ya da slot ile yeni bir yuzey acmaz. Atma, `PlayerController`/`UNetConnection` uzerinden; log `MMOMetrics::WhoName`.
- **Atma:** `AGameSession::KickPlayer(APlayerController*, const FText&)` (`GameSession.h:155`) ya da yoksa `PC->ClientWasKicked`; her oyuncu icin sonuc loglanir. Atilan istemci ana menuye doner (motor davranisi).
- **Kalicilik:** **yeni dosya/alan/yan JSON yok**, `MMOSaveGame.h`'ye dokunulmaz. Kapatma durumu bellekte; sunucu yeniden acilinca sifirdir (kasitli: yarim kalan geri sayim canlanmaz). Kayit icin yalniz mevcut `SaveCharacterForTravel`/`FlushDeferredSave`. Yan dosyalar (basarim/gunluk/posta) bilesenlerin kendi `EndPlay` yollariyla yazilir; bu sistem onlari ayrica cagirmaz (tekrar yazim ve durum yarisi riski).
- **Ileride (S04/Supabase slice 4):** `Exiting` oncesine `ReleaseLeaseEx` cagrisi eklenir; arayuz `IMMOAsyncCharacterStore` tamamlanma geri cagrimlarina bagli, bu tasarim sadece "Flushing bitince tum yazimlar bitti" sinyalini (`FOnFlushComplete`) tanimlar. Asenkron depoda `Flushing` zaman asimi `FlushTimeoutSeconds` `[PLACEHOLDER 15]`; asimda `failed` sayilir, cikis kodu 2.
- **Denetim:** `LogMMOLifecycle Display: event=shutdown_phase phase=.. remaining=.. players=..`; `[MMOMETRIC] t=.. ev=shutdown phase=.. players=.. saved=.. failed=.. skipped=..`; `ev=shutdown_save_fail who=..` (kayit hatasinda oyuncu basina bir satir).

### 5.3 Toplu kayit turu

Tum `AMMOCharacter` (otorite, `IsPlayerControlled`) uzerinde tek `Tick` icinde degil, **oyuncu basina bir karede** (hitching olmasin): `SaveCharacterForTravel(false)`; `false` donerse bir kez daha `FlushDeferredSave(true)`; sonuc `saved/failed/skipped`.
Kayit turu **bir kez** calisir (idempotent bayrak); `Kicking` sonrasi `EndPlay` kayitlari zaten ikinci savunmadir.

## 6. Ekonomi / denge etkisi

Altin/XP/esya girisi-cikisi **yok**. Oyuncu zaman/ilerleme kaybini onler (denge degil guvenlik/guvenilirlik). Kapanis sirasinda acik takas/posta/satin alma: kayit
turu bu islemleri kesmez; **L3 kabul olcutu:** takas penceresi acikken kapanis olursa (a) iki taraf da atilir, (b) hicbir esya/altin cogalmaz ya da kaybolmaz
(`MMOTradeComponent` onay ani kayit yolu mevcut; staged `MMOTradeComponent.cpp` degisikligi commit edilene kadar bu vaka `MMOTradeComponent`'e bagli bir senaryo icermez, yalniz "acik takas teklifli iki pawn atilir, `FillSaveData` sonrasi toplam altin/esya esit" vakasi yazilir).
Sayilar `[PLACEHOLDER]`: `MinDelay 30`, `MaxDelay 86400`, `LoginCloseSeconds 60`, `FlushLeadSeconds 20`, `KickLeadSeconds 5`, `FlushTimeoutSeconds 15`. Olcum ihtiyaci: gercek bir 10 oyunculu kapanista `ev=shutdown` ile kayit suresi (sn/oyuncu) ve `failed` orani; Supabase async depoda gecikme (L5 sonrasi).

## 7. Kabul olcutleri, dilimler ve test plani

Her dilim ayri dogrulanabilir ve ayri commit.

| Dilim | Is | "Bitti" = |
|---|---|---|
| **L0** | `FMMOShutdownPlan` + `MMOShutdownPlanSelfTest` + `TestShutdownPlan.ps1` | Build 0 hata (izole worktree); `StaticLint.py` + `ValidateData.py` 0 hata; `TestShutdownPlan.ps1` `PASS checks=N` (N = CASE sayisi = benzersiz etiket); asagidaki mutasyonlarin hepsi FAIL verir |
| **L1** | `UMMOServerLifecycleSubsystem`: komut satiri + Exec tetikleyici, duyuru, PreLogin reddi | `MMOTestServerLifecycle` (kismi): duyuru metinleri/sirasi, `PreLogin` olayi `ErrorMessage` doldurur, bakim disinda bos birakir, Exec yetki dogrulamasi (R1) gecer |
| **L2** | Toplu kayit turu + atma | Self-test: 3 kayitli sahte pawn (test yuvalari, `SetSaveIdentityForTest` ile), 1 yuklenmemis, 1 gecis kaydi onayli; sonuclar `saved=3 skipped=2 failed=0`; kayit dosyalari kapanis oncesi/sonrasi tutarli; ikinci tur calismaz |
| **L3** | `Exiting` + cikis kodlari + iptal + kayit hatasi enjeksiyonu | Self-test: `FailDeferredSavesForTest` ile basarisiz kayit -> cikis kodu 2 + `ev=shutdown_save_fail`; `Cancel` `Counting`/`Closing`'de calisir, `Flushing`'de reddedilir; gecmis zamanli/limit disi `Schedule` reddi |
| **L4** | `TestShutdownSmoke.ps1`: dedicated + 1 istemci (`NetSmokeTest` deseni, `-MMOShutdownAfterSeconds=45`) | Istemci duyuru satirlarini alir (log: `ClientReceiveChat` sistem kanali sayisi >= 3), `Closing` sonrasi ikinci bir istemcinin girisi reddedilir, sunucu cikis kodu 0, kayit dosyasi mevcut ve `LoadCharacter`-dogrulanabilir (JSON ayrisiyor), `Error:` 0. Port/tek kullanimlik; calisan UE/UBT varken ertelenir (`TestMailRefund.ps1` deseni) |
| **L5** | `RunSystemTests.ps1`, `RunAllChecks.ps1`, `PlaytestMetrics.py`, `PLAYTEST_CHECKLIST.md` baglantisi | Kirli dosyalar temizlenince; mevcut kosullar yesil |

### 7.1 Native self-test vakalari (L0, dunyasiz)

- Esik dizisi: `Schedule(125)` -> ilk duyuru 125 sn'yi soyler, sonra yalniz 120, 60, 30, 10, 5..1; `Schedule(3600)` ilk duyuru 3600; `Schedule(30)` ilk duyuru 30.
- Her esik tam bir kez (cok kucuk `Delta` dizisi 1000 adim ile ve tek buyuk `Delta` ile ayni olay kumesi).
- Buyuk `Delta` (tek adim 100 sn) -> `Closing`, `Flushing`, `Kicking`, `Exiting` olaylari **sirasiyla ve her biri bir kez**.
- Limit disi `Schedule` (0, 29, 86401, negatif, NaN-benzeri buyuk) ret; durum degismedi.
- Ikinci `Schedule` (`Counting` iken) ret; `Cancel` sonrasi kabul.
- `Cancel`: `Counting`/`Closing`'de `Idle` + "iptal" olayi; `Flushing`/`Kicking`/`Exiting`'de ret ve durum ayni.
- Negatif/NaN `Delta` durum degistirmez.
- `LoginClosed()` yalniz `Closing` ve sonrasinda true; iptal sonrasi false.

### 7.2 Mutasyonla sinanacak korumalar (`test-muhendisi`; geri yukledikten sonra dosyaya touch)

1. Limit kontrolu (`MinDelay`) silinir. 2. Esik "bir kez" bayragi silinir. 3. `Cancel`'in `Flushing`'de reddi silinir. 4. `Closing` esigi 60 -> 0. 5. Buyuk `Delta` durum atlama (ara olaylar yutulur). 6. `LoginClosed()` her zaman false. 7. (L2) kayit turu idempotent bayragi silinir. 8. (L2) `IsInitialLoadComplete` kontrolu silinir (yuklenmemis pawn'a kayit yazilir). 9. (L3) cikis kodu 2 yerine hep 0. 10. (L1) `PreLogin` abonelik kaldirilir. Hepsi `TestShutdownPlan.ps1`/`TestServerLifecycle.ps1` FAIL vermelidir; yakalanmayan mutasyon testin eksigidir.

### 7.3 Oyun ici (PIE) ilk test listesi (kullanici, L4 sonrasi)

1. Listen-server'da `MMOShutdown 60` -> sohbette 60/30/10/5..1 satirlari; ikinci pencere 60 sn sonra katilmayi dener, "Sunucu bakima giriyor" reddi.
2. `MMOShutdownCancel` 40. sn'de -> "Bakim iptal edildi", yeni giris tekrar mumkun.
3. Altin harca/esya al -> `MMOShutdown 35` -> kapanis sonrasi tekrar ac: son durum yuklu (autosave penceresi beklenmeden).
4. Olu karakterle ve takas penceresi acikken kapanis: cokme yok, esya cogalmaz.
5. Dedicated: `MyProjectServer` yok (kaynak motor gerekli); gecici olarak `-server -log` editor yolu (`SIX_MAP_PLAN.md` §11 adim 4) ile L4 betigi.

## 8. Riskler ve acik sorular

| # | Risk / soru | Onlem / oneri |
|---|---|---|
| R1 | Uzak istemcinin `ServerExec` ile kuresel Exec komutuna ulasmasi (Shipping disi) | §5; L1 kabulu icin test; Shipping'de komut derlenmez |
| R2 | `RequestExit` kayit tamamlanmadan surec oldurur (asenkron depo) | `FlushTimeoutSeconds`, cikis kodu 2; asenkron depo baglaninca bekleme tamamlanma geri cagrimina baglanir (slice 4) |
| R3 | Kayit turu sunucuyu kasar (N oyuncu x diske yazim) | Oyuncu basina bir kare; olcum `ev=shutdown` sure; yuksek N icin `FlushLeadSeconds` artirilir |
| R4 | `SaveCharacterForTravel` imzasi/anlami S04 dalinda degisir | L3'ten once `git -C <s04 worktree> diff master -- MMOCharacter.h`; imza degisirse tek kopru fonksiyonu (`LifecycleSaveOne`) uyarlanir |
| R5 | `MMOCharacter.*` kirli: ara sira public API adlari degisebilir | Yalniz `.h`'deki public okuma/kayit API'leri kullanilir; ana dizin temizlenmeden L2 baslamaz, L0-L1 baslar |
| R6 | HUD yok: oyuncu yalniz sohbet satirini gorur (kacirabilir) | Kabul; HUD bandi sonra (kirli HUD sahipleri bitince); sayac sifir deger yaratmaz, deger duyuru+kayittir |
| R7 | Kapanis yarisi: ayni anda bolge gecisi kaydi ucustayken | `IsSaveDeferredByTravel` -> `FlushDeferredSave(true)`; basarisizsa `failed` ve cikis kodu 2; L2 vakasi |

**Kullaniciya kalan kararlar:**
- K1: Kapanista oyuncular **atilsin mi** yoksa sunucu dogrudan kapansin mi (oneri: atil + kaydet; istemci nazik mesaj gorur)?
- K2: `MaxDelay` 24 sa uygun mu, ve kapatma yetkisi yalniz komut satiri/sunucu konsolu mu kalsin (oneri: evet, hesap/token yetkisi A3 sonrasi `MMOAdmin` icinde)?
- K3: Cikis kodu 2 (kayit hatasi) bir surec yoneticisinin yeniden baslatmasini engellemeli mi (oneri: evet, otomatik yeniden baslatma kapali)?

## 9. "Degmez" notu (kapsam daraltma)

HUD geri sayim bandi, bakim takvimi arayuzu, yonetici paneli: **simdi degmez** (sayi/etiket cilasi, deger yaratan kisim duyuru + kayit + giris kapatmadir).
Bildirimin metin cilasi icin `metin-yazari` cagirmaya gerek yok; sabit sozluk 4 satir.

## 10. Ilk dilim (L0) ozeti

Yeni 4 dosya (`MMOShutdownPlan.h/.cpp`, `MMOShutdownPlanSelfTest.cpp`, `TestShutdownPlan.ps1`), mevcut dosya degisikligi sifir, dunya/aktor/kayit
bagimsiz, tek derleme + tek test; mutasyon 1-6 L0 kabulunun parcasi. L0 `mmo-developer` ile yazilir, `test-muhendisi` mutasyonlari kosar,
`qa-kontrolcu` + `ag-guvenlik-denetcisi` paralel okur (L1'den itibaren guvenlik denetimi zorunlu: Exec yetkisi, PreLogin).
