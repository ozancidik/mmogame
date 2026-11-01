# Tasarim: Karakter .sav atomik yazim ve bozuk dosya kurtarma (S6)

**Tarih:** 2026-11-01 · **Taban:** `acdcae2` (master) · **Durum: TASARIM, kod yok.** Yazan: `oyun-tasarimcisi`.
Kaynak: `docs/DESIGN_SHUTDOWN_SMOKE.md` §1.3 + §7.3 (karar K-S6). Bu belge K-S6'yi **evet** olarak uygular (gerekce §0).

## 0. Oyuncu deneyimi ve neden simdi

Oyuncu "ilerlemem silindi / karakterim acilmiyor" durumuyla hic karsilasmamali: sunucu cokerse, guc giderse ya da cikis kodu 2 yolu (`TerminateProcess`) isleyip yazim ortasinda surec olurse karakter ya eski ya yeni haliyle gelir, asla yarim kalmis hali ile degil.

Neden simdi: (1) sunucu kapanis zinciri (L0-L3) ve gercek surec duman testi (S0-S4) bitti; kapanista **her oyuncu tek tur daha yazilir** ve hata durumunda surec `TerminateProcess` ile olur, yani yazim penceresi artik sistematik olarak gercek. (2) Yan dosyalar (posta/basarim/gunluk/irk) zaten `.tmp`+Move+`.bak` desenini kullanir; **tek korumasiz kayit karakterin kendisi**. (3) DB modu (S04/Supabase) kapali alfaya yetismezse yerel mod varsayilan kalir; karar bekleyen S04 catismasi bu dosyaya dokunmaz (`MMOLocalFileCharacterStore.cpp` bu dali `20bd02c` ile degismiyor, 2026-11-01 `git diff master 20bd02c` bos). DB modu gelirse bu dilim yerel/offline yolu icin hala gecerli kalir.

## 1. Kod gercegi (grep + motor kaynagi, 2026-11-01)

- `MMOLocalFileCharacterStore.cpp:27` `UGameplayStatics::SaveGameToSlot` -> `FGenericSaveGameSystem::SaveGame` dogrudan son yola yazar. `SaveGameToMemory` ayri ve herkese acik (`GameplayStatics.cpp:2371`), `LoadGameFromMemory` (`:2457`) de.
- **Dosya silme/tasima gercegi (motor, `FileManagerGeneric.cpp:299-323`):** `IFileManager::Move(Dest, Src, Replace=true)` once hedefi **siler**, sonra `MoveFileW` yapar; `EvenIfReadOnly` parametresi hic kullanilmaz. Yani `.tmp -> ana` Move'u da "ana dosyanin hic olmadigi" kucuk bir pencere birakir (mail deposunun `.bak` kurtarma deseninin nedeni). Salt-okunur hedef: `DeleteFileW` basarisiz -> Move basarisiz (bu, duman testi S3 enjeksiyonunu etkiler, bkz. §6.3).
- `MMOCharacterStoreSubsystem.cpp:151` tek yerde `FMMOLocalFileCharacterStore` olusturur; `Load/Save/Exists/ListSlotNames` arayuzu `IMMOCharacterStore` (`MMOCharacterStore.h`) ile sabit. Cagiranlar degismez.
- Disk adi: `Saved/SaveGames/<Yuva>.sav` (yuva = `MMOSave_<ad>`, ad ASCII kurali `MMONameRules`). `ListSlotNames` glob'u `MMOSave_*.sav`: `.sav.tmp`/`.sav.bak`/`.sav.bad` eslesmez (iyi).
- **Yedek araci** (`Scripts/MMOSaveFilesLib.ps1:313`) karakteri `^MMOSave_(?<n>.+)\.sav\z` ile siniflar: `.sav.tmp/.bak/.bad` **beyaz liste disi (ignored)**, yarim dosya yanlislikla yedege girmez; fakat yazim Move penceresinde ana dosya yoksa o karakter o turda **yedeklenmez** (R2).
- `MMOMailComponent.cpp:82` cevrimdisi alici varligini `FileExists(<ana .sav>)` ile bakar (kirli degil, ama bu dilimin disinda): yalniz `.bak` kalmis alici "yok" sayilir (R3).
- `MMOCharacter.cpp:204` (kirli, dokunulmaz) yalniz self-test fixture'inda `DeleteGameInSlot` cagirir; uretim silme yolu yok.

## 2. Kapsam / kapsam disi

**Kapsam:** yalniz yerel karakter magazasinin `Save`/`Load`/`Exists` davranisi: (a) `.tmp` + dogrulama + `.bak` donusumu + Move, (b) bozuk/eksik ana dosyada `.bak`/`.tmp`'den kurtarma, (c) bozuk dosyayi **silmeden** `.bad`'a ayirma, (d) kurtarma ve hata loglari, (e) crash-noktasi matrisi testi.

**Kapsam disi:** dosya formatinin degisimi (`UMMOSaveGame` ayni, eski `.sav` okunur, `MMOSaveGame.h` degismez); DB/Supabase yolu; yan dosyalar (zaten atomik); yedek/geri yukleme aracinin davranisi (yalniz siniflandirma sabitleme testi §6.3); `MMOMailComponent` varlik kontrolu (R3, ayri kucuk is); otomatik yazim sikligi/yazim kuyrugu; bulut senkron.

**Mevcutla iliski:** `IMMOCharacterStore`, `FMMOSlotWriteTracker`, `MMOServerLifecycle` yazim sirasi, `SaveCharacterForTravel` **degismez**. Yan dosya desenini (`MMOMailStore.cpp:189-330`) tekrar yazmak yerine kavramsal olarak izler; kod paylasilmaz (farkli veri, farkli bayt dogrulayici).

## 3. Dosya haritasi

### 3.1 Yeni dosyalar (hepsi su an yok)

| Dosya | Icerik | Dilim |
|---|---|---|
| `Source/MyProject/Public/Save/MMOSlotFileAtomic.h` | `FMMOSlotFileAtomic`: **saf mantik**. `IMMOSlotFileOps` (Exists, ReadAll, WriteAll, Move, Delete; gercek ve sahte uygulama) + `EMMOSlotFileState {Missing, Valid, Corrupt}` + `Plan` fonksiyonlari (Load kaynagi sec, Save adimlari). UObject yok; dogrulayici bir `TFunction<bool(const TArray<uint8>&)>` olarak disaridan gelir | S6.0 |
| `Source/MyProject/Private/Save/MMOSlotFileAtomic.cpp` | uygulama | S6.0 |
| `Source/MyProject/Private/Tests/MMOSlotFileAtomicSelfTest.cpp` | Sahte dosya sistemi ile crash-noktasi matrisi + gercek disk (`%TEMP%` altinda tek kullanimlik dizin) + magaza entegrasyonu (S6.1), konsol komutu `MMOTestSlotAtomic` (`FSelfRegisteringExec` deseni, `MMOMailSelfTest.cpp:691`) | S6.0/S6.1 |
| `Scripts/TestSlotFileAtomic.ps1` | `TestMail.ps1` deseninin kopyasi (tek kullanimlik NullRHI dunya, zaman asimi, cikis 0/1/2/3, `Error:` taramasi). Calisan UE/UBT varsa **ertele (3)** | S6.0 |
| `Scripts/TestSlotFileAtomicMutations.ps1` | Mutasyon betigi (M1..M10 §6.2); kaynagi geri yukler + `LastWriteTime` dokunusu | S6.2 |

### 3.2 Dokunulacak mevcut dosyalar (hepsi temiz; `git status` 2026-11-01'de kirli listede YOK)

| Dosya | Degisiklik | Dilim |
|---|---|---|
| `Source/MyProject/Private/Save/MMOLocalFileCharacterStore.cpp` (+ `.h` yalniz yardimci uye varsa) | `Save/Load/Exists` yeni yardimciyi kullanir; `ListSlotNames` ayni | S6.1 |
| `Scripts/TestShutdownSmoke.ps1` | `savefail` enjeksiyonu (`:276` `Set-SmokeReadOnly` ana `.sav`) yeni yazimda gecersiz kalir -> hedef `.sav.tmp` olur (§6.3) | S6.3 |
| `Scripts/TestBackup.ps1` (ya da kucuk yeni vaka dosyasi) | `MMOSave_X.sav.tmp/.bak/.bad` = `ignored` sabitleme vakalari | S6.3 |
| `docs/DESIGN_SHUTDOWN_SMOKE.md` | §1.3/§7.3'e "S6 uygulandi" tek satir | S6.4 |
| `CLAUDE.md` | tek madde (belge-denetcisi) | S6.4 |

### 3.3 Codex cakisma riski

`git status` 2026-11-01: kirli `MMOCharacter.*`, `MMOSkillComponent.cpp`, `MMOPlayerController.cpp`, `MMOHUD.*`, `RunSystemTests.ps1`, `TestNetSmokeRunner.ps1`, `PlaytestMetrics.py`, `PLAYTEST_CHECKLIST.md`; izlenmeyen `MMORace*`, `MMOZoneLayout*`, `TestRace.ps1`. **Bu tasarimin hicbir dosyasi bunlarla ortusmez.** `RunSystemTests.ps1` / `RunAllChecks.ps1`'e **baglanmaz** (S5 ertelenmis sayilir; `RunSystemTests.ps1` kirli). Test elle: `Scripts/TestSlotFileAtomic.ps1`. Derleme: ana dizin `MMOZoneLayout*`/`MMORace*` yuzunden derlenmeyebilir -> derleyici ajan izole worktree kullanir (`shutdown-l0` deseni; kosucu o worktree'nin `.uproject`ini alir). Acik `s04-p0-hardening` dalinda `MMOLocalFileCharacterStore.cpp` degismiyor; S6.1 baslamadan once yeniden `git diff master <dal> -- <dosya>` bakilir.

## 4. Davranis

Ad kuralari: `<A>=<Yuva>.sav`, `<A>.tmp`, `<A>.bak`, `<A>.bad[.<utc>]`, hepsi `Saved/SaveGames/` icinde. **Yuva adi savunmasi (S6.1):** yuva `^MMOSave_[A-Za-z0-9 _-]+$` degilse Save/Load/Exists fail-closed (yol gecisi yok; bugunku motor yolu bunu denetlemiyordu).

### 4.1 Save(Yuva, Veri) (sunucu oyun is parcacigi, senkron)

1. `SaveGameToMemory` -> `Bytes`; bos ya da `Bytes > 8 MB` (`MMOSaveFilesLib.ps1:24` ile ayni tavan) ise **false**.
2. Dogrulayici(Bytes) = `LoadGameFromMemory` -> `UMMOSaveGame` -> `bValid`'in `Veri.bValid` ile esit olmasi. Basarisiz = false (hicbir dosyaya dokunulmaz).
3. Eski `.tmp` varsa silinir; **silinemezse false** (dogrudan-yazim yedegi YOK; mail deposundan bilincli sapma, cunku atomik olmayan yol geri gelir).
4. `.tmp` yazilir, **geri okunur, bayt-bayt esit olmali**. Degilse `.tmp` silinir (silinemezse `.tmp.bad`), false.
5. Ana dosya var ve **gecerli** ise `ana -> .bak` (Move, replace). Ana var ama gecersiz ise `ana -> .bad` (kanit korunur, `.bak` EZILMEZ). Ana yoksa adim atlanir (`.bak` varsa korunur).
6. `.tmp -> ana` (Move, **tekrarsiz**: `bDoNotRetryOrError=true`, motorun 10x0.5 sn beklemesi sunucuyu dondurmasin). Basarisizsa: `.bak` varsa ana yoluna geri konur, `.tmp` silinir, **false**.
7. Basari: `.bak` silinir (silinemezse uyari logu; bayat `.bak` yalniz ana dosya kaybolursa devreye girer, bkz. R4). **true** doner ancak 6 basariliysa.

### 4.2 Load(Yuva) (yan etkisiz: diski DEGISTIRMEZ)

Sira: ana gecerli ise o. Ana yok/gecersiz ise `.bak` gecerliyse o. O da degilse (yalniz ana ve `.bak` yokken) `.tmp` gecerliyse o. Hicbiri degilse **false**. Kurtarma kaynagi ana dosya degilse `[MMOSAVE] event=recovered slot= source=bak|tmp` (Warning) ve `[MMOMETRIC] ev=save_recovered src=...`; tamamen basarisizsa Warning `event=load_failed reason=corrupt|missing`. **Dosyalar ezilmez** (kanit, §5). Yan etkisiz olmasinin nedeni: `Scripts/TestCharacterStore.ps1` gercek `Saved/` icindeki tum slotlari `Load` eder; Load disk yazsaydi test gercek oyuncu dosyasini degistirirdi. Onarim bir sonraki basarili `Save`'in adim 5-6'sindadir.
"Gecerli" = `LoadGameFromMemory` basarili ve `bValid==true` (+ boyut >0). **Sinir:** bozuk ama `bValid=true` kalan kesik dosya motor serilestirmesinde yakalanmayabilir; asil koruma yazim tarafindadir (adim 4 geri okuma) ve bu artik risk R1'de acik yazilidir.

### 4.3 Exists(Yuva)

`true` = ana dosya var **veya** `.bak` var. (`.tmp` tek basina yeterli degil: yarim olabilir.) Boylece "ana kayip + `.bak` var" durumunda cagiran yeni karakter yaratip `.bak`'i ezmez.

### 4.4 Crash-noktasi degismezleri (test edilen sozlesme)

Save'in her dosya isleminden hemen sonra surec oldurulse (N nokta) ve ardindan `Load` cagrilsa: (I1) sonuc **eski veri ya da yeni veri**, ikisinin karisimi asla; (I2) gecerli bir kopya (ana/`.bak`) diskte varken `Load` false donmez; (I3) `Save` false dondugunde `Load` eski veriyi verir; (I4) `Save` true dondugunde `Load` yeni veriyi verir; (I5) bozuk dosya hicbir yolda silinmez (en kotu `.bad`); (I6) ilk-ever kayit (ana ve `.bak` yok) yarida kesilirse `Load` ya yeni gecerli veriyi ya da false verir, kesik veriyi asla.

## 5. Sunucu-otorite, replikasyon, kimlik, kalicilik, guvenlik

- **Yeni RPC yok, yeni replike alan yok, yeni konsol yetkisi yok** (yalniz test icin `MMOTestSlotAtomic` Exec; Y-9 uzak-Exec korumasi altinda, `UE_BUILD_SHIPPING` disi, deseni mevcut `MMOTestMail` ile ayni).
- **Kimlik:** degismez (oyuncu adi hala kimlik, K-1 ayri). Yuva adi dogrulama (§4) yalniz yol-gecisi savunmasi; yeni kimlik iddiasi degil.
- **Kalicilik:** `MMOSaveGame.h`'ye dokunulmaz; format ayni. Eski `.sav` bire bir okunur (`TestCharacterStore.ps1` diskteki tum slotlari yukleyip esitler: regresyon).
- **Guvenlik gereksinimleri (`ag-guvenlik-denetcisi` okur):**
  1. Hicbir yolda oyuncu dosyasi silinmez: yalniz kendi `.tmp`'si, basarili yazim sonrasi `.bak` ve gecici test dosyalari. Bozuk ana dosya `.bad`'e **tasinir**; `.bad` zaten varsa `.bad.<utc-yyyyMMddHHmmss>` ve en fazla `[PLACEHOLDER 5]` adet (asilirsa en eskisine dokunmadan yeni birakilmaz, uyari).
  2. Yol savunmasi (yuva adi kurali + `FPaths::ProjectSavedDir()/SaveGames` altina kilit; `..`, `/`, `\`, `:` ret).
  3. Dogrudan-yazim yedegi yok (atomiklik geri gelmesin).
  4. Move tekrarsiz (sunucu donmasin), tek parcacikli (oyun is parcacigi); async yok.
  5. Tum yeni loglarda oyuncu adi `MMONameRules` kurali ASCII oldugundan log enjeksiyonu yok; boyut/sayi log'a yazilir, icerik yazilmaz.
  6. Yazim bayt tavani 8 MB; asilirsa false (disk doldurma/DoS savunmasi).
  7. Kalan `.tmp/.bak/.bad` dosyalari yedek/geri yukleme aracinca `ignored` kalir (§6.3 testi); ayni araca "yan dosya" sinifi eklemek ayri karar (R2).

## 6. Test plani ve kabul

### 6.1 Dilimler (her biri ayri commit, ayri "bitti")

| Dilim | Is | "Bitti" = |
|---|---|---|
| **S6.0** (L0, saf mantik) | `MMOSlotFileAtomic.*` + sahte dosya sistemi + `MMOSlotFileAtomicSelfTest.cpp` (yalniz sahte FS) + `TestSlotFileAtomic.ps1` | `PASS checks=N` (N tek yerde sayilir: test, betik beklentisi, commit mesaji); §4.4 I1-I6 her crash noktasinda (Save adim 1-7 icin her dosya islemi oncesi/sonrasi + her islemin basarisizligi) saglanir; `Error:` 0; derleme izole worktree'de Succeeded |
| **S6.1** | `FMMOLocalFileCharacterStore`'u yardimciya bagla; gercek disk `%TEMP%` dizini vakalari + magaza entegrasyonu (`MMOSave_ZAtomicTest*` yuvasi, test sonunda yalniz kendi dosyalari silinir) | Mevcut `TestCharacterStore.ps1` (eski `.sav` yukleme + gidis-donus) PASS, `existsMismatch=0`; yeni vakalar PASS; eski bayt-esit: yeni magaza ile yazilan dosya `LoadGameFromSlot` (motor yolu) ile de yuklenir (format ayni) |
| **S6.2** | Mutasyon (§6.2) | Hepsi FAIL verir; yakalanmayan = testin eksigi; kaynak geri yuklenir + dokunulur |
| **S6.3** | Baglanti: duman testi `savefail` enjeksiyonu + yedek siniflandirma sabitleme | `TestShutdownSmokeEvidence.ps1` PASS (degismez); `TestBackup.ps1` PASS + yeni vakalar; gercek `TestShutdownSmoke.ps1 -Scenario savefail` yeni enjeksiyonla yine `Process.ExitCode==2` (elle, tek kosu, calisan UE yokken) |
| **S6.4** | `qa-kontrolcu` + `ag-guvenlik-denetcisi` (paralel, salt okunur), `belge-denetcisi` | Bulgular kapali ya da `DECISIONS_NEEDED.md`'ye yazili; belge satirlari |

### 6.2 Mutasyonla sinanacak korumalar (`test-muhendisi`)

M1 geri okuma esitligi (adim 4) silinir. M2 `ana -> .bak` oncesi gecerlilik denetimi silinir (bozuk ana gecerli `.bak`'i ezer). M3 bozuk ana `.bad` yerine silinir. M4 `Load` sirasi `.tmp` once. M5 `Exists` yalniz ana dosya. M6 basari sonrasi `.bak` silinmez ya da basarisizlikta `.bak` geri konmaz. M7 adim 3 silinemeyen `.tmp`'de devam eder (dogrudan yazim). M8 yuva adi dogrulamasi silinir (`MMOSave_..\x`). M9 8 MB tavani silinir. M10 `Load` diske yazar (yan etkisizlik; test `TestCharacterStore` benzeri dosya zamani/boyutu degismedigini denetler).

### 6.3 Etkilenen mevcut testler (acik liste)

- `Scripts/TestShutdownSmoke.ps1` `savefail`: ana `.sav` salt-okunur yapmak **yeni yazimi basarisiz kilmaz** (Windows'ta `ana -> .bak` yeniden adlandirma salt-okunur dosyada calisir; sonra `.tmp -> ana` basarili). Yeni enjeksiyon: `Saved/SaveGames/MMOSave_SmokeL4A.sav.tmp` onceden **salt-okunur dosya** olarak olusturulur -> adim 3 silme basarisiz -> Save false -> `FinishExit` kod 2. Kosucu `finally`'de geri alir; yazim yine basarili cikarsa mevcut kural: INCONCLUSIVE (4), asla PASS. Bu degisiklik S6.1'le AYNI turda yapilmazsa duman testi `savefail` senaryosu INCONCLUSIVE'e duser (sahte PASS yok, ama kayip kapsam).
- `Scripts/TestCharacterStore.ps1`, `TestSaveLifecycle.ps1`, `TestZoneTravelDeferredSave.ps1`, `TestStatsSaveBudget.ps1`: davranis degismez; S6.1 sonrasi hepsi kosulur (kaydi `.sav` yolu uzerinden okuyan varsa yeni `.bak/.tmp` artigi beklemez).
- `Scripts/VerifySaves.ps1` / `BackupSaves.ps1` / `RestoreSaves.ps1`: `.sav.tmp/.bak/.bad` sinifi `ignored` (regex `\z`); S6.3 sabitleme vakalari.

### 6.4 Oyun ici (PIE/elle) ilk test listesi (kullanici)

1. Karakterle oyna, Alt+F4/gorev yoneticisi ile sunucuyu ortada oldur; yeniden ac: karakter en fazla son otomatik yazim (60 sn) kadar geride mi, acildi mi.
2. `Saved/SaveGames/` klasorunde normal calismada `.tmp/.bak/.bad` kalinti birakmiyor mu (yalniz `.sav`).
3. Bir `.sav`i elle kesip (ilk 100 bayt) acmak: bir onceki yazimdan `.bak` yoksa karakter "yeni" mi basliyor, `.bad` olusuyor mu (silme yok); log `event=load_failed` gorunuyor mu.

## 7. Ekonomi / denge etkisi

Altin/XP/esya girisi-cikisi **yok**. Maliyet: yazim basina ek okuma+karsilastirma (tens of KB) `[PLACEHOLDER; olcum]`: S6.1'de `[MMOMETRIC] ev=save_write_ms` yerine tek seferlik `FPlatformTime` olcumu (log, kalici metrik ekleme yok) ile 0.1-5 ms beklenir, 20 oyuncu x 60 sn otomatik yazimda ihmal edilir. Disk: gecici 2 kopya, yazim sonrasi 1. Olcum esigi: tek yazim > 50 ms `[PLACEHOLDER]` olursa uyari logu (karar degil, bilgi).

## 8. Riskler ve acik sorular (kullaniciya soru yok; varsayilanli)

| # | Risk | Varsayilan karar |
|---|---|---|
| R1 | Motor serilestirmesi kesik dosyada hata vermeyebilir; `bValid=true` kalan kesik ana dosya gecerli sayilir | Yazim tarafi geri okuma + `Move` ile ana dosya hic yarim yazilmaz; `.sav` artik yalniz tam dosya olarak belirir. Kalan risk: disk bozulmasi (kapsam disi). Ek bir icerik karma/trailer **eklenmez** (format degisirdi) |
| R2 | Yazim Move penceresinde yedek araci karakteri o turda gormez | Yedek arac "sunucu calisirken" zaten risk olarak belgeli (`BACKUP_OPERATOR_GUIDE.md`); yan dosya sinifi eklemek ayri karar, simdi **eklenmez** |
| R3 | `MMOMailComponent.cpp:82` yalniz ana `.sav`a bakar; yalniz `.bak` kalmis alici posta alamaz | Ayri, kucuk, sonraki is (`Exists` mantigini paylasmak); yalniz kayip penceresinde, geri alinabilir (posta iade edilir) |
| R4 | Silinemeyen bayat `.bak` ileride kaybolan ana dosyayi eski halle diriltebilir | Mail deposuyla ayni kabul; uyari logu; `.bak` yalniz ana yok/gecersizken kullanilir |
| R5 | Windows disi platformda `Move` semantigi | Kapsam Windows (bugunku yerel sunucu); Linux sunucu hedefi (S01) gelince tekrar |
| R6 | S04 dali `MMOLocalFileCharacterStore.cpp`'yi degistirirse catisma | S6.1 oncesi `git diff` kontrolu; catisma varsa S6.0 (saf yardimci) yine gecerli, baglanti bekler |
| R7 | `Load` yan etkisiz oldugundan bozuk ana dosya ilk `Save`'e kadar diskte kalir | Kasitli (§4.2); ilk basarili `Save` onarir; Load log'u operatore gorunur |

**Kullaniciya kalan gercek karar: yok.** Varsayilan: S6 uygulansin (K-S6 = evet), DB modu kararindan bagimsiz.

## 9. "Degmez" notu

Icerik karmasi/surum zarfi (format degisikligi, kazanc dusuk, R1 ile kapsami buyur), yedek araci icin `.bak` yan dosya sinifi (simdi), asenkron/arka plan yazim (oyun is parcacigi ile durum yarisi acar), HUD "kayit basarisiz" bandi (sicak dosya, deger sinirli): **simdi degmez**.
