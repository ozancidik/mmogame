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

## 10. S6.0 durum notu (2026-11-01, uygulandi, izole worktree'de derlendi ve kosuldu; S6.1-S6.4 YOK)

- Yazildi: `Public/Save/MMOSlotFileAtomic.h`, `Private/Save/MMOSlotFileAtomic.cpp` (saf mantik, `IMMOSlotFileOps` soyut dosya sistemi; **gercek IFileManager uygulamasi S6.1'e birakildi**, burada yok), `Private/Tests/MMOSlotFileAtomicSelfTest.cpp` (bellek ici sahte FS, `MMOTestSlotAtomic`), `Scripts/TestSlotFileAtomic.ps1` (144 kontrol), `Scripts/TestSlotFileAtomicMutations.ps1` (16 mutasyon, yalniz bagli worktree'de, ana checkout reddeder).
- Matris: 11 senaryo; 76 cokme noktasi (her alt adimdan sonra + yazma adiminda bos/yarim dosya), 87 tek hata, 229 cift hata kosusu; her biri I1/I2/I3/I4/I5 ile denetlenir. Sahte FS `Move(Replace)`'i "once hedefi sil, sonra yeniden adlandir" iki adim olarak modeller.
- Tasarimdan SAPMALAR: (1) `.bad` yuvalari `.bad`, `.bad.1`..`.bad.4` (zaman damgasi yok, saf mantik deterministik); yuva kalmazsa Save fail-closed (`BadCapReached`, hicbir dosya silinmez). (2) Bozuk `.bak` de silinmez: `.bad`'e ayrilir. (3) Ana ve `.bak` yokken GECERLI `.tmp` (Load'un verdigi tek kopya) Save adim 3'te silinmez, `.bak`'a yeniden adlandirilir (matris `tmp_valid_first` senaryosu bu acigi yakaladi). (4) Okunamayan ana/.bak Save'i fail-closed reddeder (`MainUnreadable`). (5) Yuva adinda sondaki bosluk reddedilir (Windows sondaki boslugu atar).
- Bilinen sinir: `bValid=true` kalan kesik dosya motorda yakalanmayabilir (R1); test dogrulayicisi sikidir, gercek `LoadGameFromMemory` dogrulayicisi S6.1'de baglanir.

## 11. S6.1 durum notu (2026-11-01, uygulandi, izole worktree'de derlendi ve kosuldu; S6.3 ve S6.4 YOK)

- **Yazildi:** `Private/Save/MMOSlotFileOpsFileManager.cpp` (`FMMOFileManagerSlotOps`, gercek `IFileManager`/`IPlatformFile` uygulamasi; sinif bildirimi `MMOSlotFileAtomic.h`'de), `Private/Save/MMOLocalFileCharacterStore.{h,cpp}` (Save/Load/Exists artik `FMMOSlotFileAtomic` uzerinden; `ListSlotNames` ayni; yeni `FMMOLocalFileCharacterStore(const FString& DirectoryOverride = "")` yalniz test icin), `Private/Tests/MMOSlotFileAtomicRealSelfTest.cpp` (komut `MMOTestSlotAtomicReal`, 62 kontrol), `Scripts/TestSlotFileAtomicReal.ps1` (kosucu; `Saved/SaveGames` oncesi/sonrasi parmak izi + `Intermediate/MMOSlotAtomicRealTest` artik dizin denetimi), `Scripts/TestSlotFileAtomicRealMutations.ps1` (16 mutasyon: 4 gercek dosya islemi, 11 magaza/dogrulayici, 1 saf katman; hepsi belirli FAIL satiriyla yakalandi). Saf katman `Load`/`Save`'e **opsiyonel `TmpValidate`** parametresi aldi (varsayilan `nullptr` = S6.0 davranisi aynen; S6.0 testi 144/144 degismedi).
- **Gercek motor davranisi (kaynak + olcum):** `IFileManager::Move(Replace=true)` hedefi once siler (`bDoNotRetryOrError=true` iken silme basarisiz olsa da `MoveFileW` hedef var diye basarisiz olur; bekleme/tekrar yok). `FFileManagerGeneric::Delete(EvenReadOnly=false)` salt-okunur dosyayi silmez. `IPlatformFile::OpenWrite` (CREATE_ALWAYS) salt-okunur mevcut dosyada acilamaz. Hepsi gercek diskte testle dogrulandi (`ops/*`, `inject/*`).
- **R1 gercegi (olculdu, tasarimdaki varsayimdan DAHA KOTU):** `LoadGameFromMemory` kesik dosyada "hata vermeyip varsayilan birakmaz", **Development'ta COKER**: (a) `GVAS` etiketi olmayan/bozuk baslik -> sinif adi FString uzunlugu ilk 4 bayttan okunur -> `FName` 1023 karakter assert'i (denemede 8 baytlik sacma dosya motoru dusurdu); (b) kesik govde -> `UStruct::SerializeVersionedTaggedProperties` `checkf(Tag.Size == Loaded)` (Shipping'de `check` yok, orada yalniz kesik veri sessiz yuklenir). Yani eski yoldaki (`LoadGameFromSlot`) bozuk/kesik bir `.sav` Development sunucusunu dusurur. Cozum (format DEGISMEDI): **`LoadGameFromMemory`'ye girmeden once** `FMMOLocalFileCharacterStore::HasSafeStructure`: `FSaveGameHeader::Read`'in sinirli kopyasi (GVAS, surum 2-3, Optimized custom-version bicimi, sinif adi == `UMMOSaveGame` yolu, kisa FString siniri) + ust duzey property etiketi yuruyusu (her `Size` dosyaya sigmali, akis `None` + son int32 ile bitmeli). Bu, kesik dosyayi motora hic vermeden "gecersiz" yapar. 3477 baytlik ornek kayitta **her kesik onek (0..3476 bayt) reddedilir** (`[MMO_SLOTREAL_R1] ... structure_walk_accepted=0`), motor cagrisi yok. 821 gercek eski `.sav` (hepsi surum 3 / UE4 522 / UE5 1018) hepsi yeniden yukleniyor (asagida).
- **Dogrulayicilar (gerekce):** ana/`.bak` = `HasSafeStructure && LoadGameFromMemory ok && (bValid || kanonik)` (eski dosyalar kanonik olmayabilir -> `bValid=true` yeterli; `bValid=false` yalniz yeniden serilestirme bayt-bayt esitse kabul = gercek bos kayit, bValid'den once kesilmis dosya degil). `.tmp` = **kanonik** zorunlu (parse + `SaveGameToMemory` + bayt esitligi; `.tmp`'yi her zaman bu kod yazar, bu yuzden sikiyi karsilar). Neden ek isaret/karma/uzunluk onegi YOK: format degismezdi ve gerek kalmadi — yazim tarafi geri okuma + Move ile ana dosya hic yarim yazilmaz; yarim `.tmp` yapi yuruyusuyle (kesik) ve kanonik denetimle (fazla bayt) elenir; ayni baytlar ANA dosya olarak yuklenir (eski dosya toleransi, test `tmp/noncanonical_tmp_only_rejected_but_same_bytes_as_main_accepted`).
- **Tasarimdan sapmalar:** (1) Dogrulayici `bValid==true` DEGIL: `IMMOCharacterStore` sozlesmesi ("magaza veriyi yorumlamaz", `bValid` cagiranda) ve `MMOTestStore`'un `invalid` sayaci geregi `bValid=false` kayit gecerli veridir (yukaridaki kosul). (2) Kesik-denetim motorun `FPropertyTag`'ini kullanmaz (`COREUOBJECT_API` degil, baglanmaz): 5.7 etiket bicimi elle yurunur; `PROPERTY_TAG_COMPLETE_TYPE_NAME` oncesi bicimler ve `HasPropertyExtensions` bayrakli etiketler dogrulanamaz (onceki yalniz baslik denetimi; sonuncusu reddedilir) — repodaki tum 821 dosya surum 1018'dir. (3) `ReadAll` 8 MB'tan buyuk dosyayi okumaz (I/O hatasi = Save fail-closed).
- **Loglar:** `LogMMOSaveFile`: `[MMOSAVE] event=recovered slot= source=bak|tmp` + `[MMOMETRIC] ev=save_recovered player= src=`; `event=load_failed slot= reason=corrupt`; `event=save_failed slot= reason=<ad>`; `[MMOMETRIC] ev=save_bad_cap player=<izin listeli ad>`; `event=stale_bak_left`; `event=slow_save ms=` (>50 ms `[PLACEHOLDER]`); gecersiz yuva adi uzunlukla loglanir, adla DEGIL.
- **Gozlem (S6.3 icin):** ANA `.sav` salt-okunur yapilirsa (eski duman testi `savefail` enjeksiyonu) birinci `Save` BASARILI olur (salt-okunur dosya yeniden adlandirilir, bayat salt-okunur `.bak` kalir, `event=stale_bak_left`), ikinci `Save` `.bak`'i yerine koyamaz -> `rotate_failed`, false, veri kaybi YOK (`[MMO_SLOTREAL_OBS] readonly_main first_save=1 second_save=0`). Kapanis zincirindeki kick-time yeniden yazimi bu yuzden yine hata verir ama ilk yazim gecer; §6.3'teki gibi enjeksiyonun `.sav.tmp` salt-okunur dosyasina tasinmasi gerekir.
- **Kalan riskler:** (a) govdesi boyut-tutarli ama icerigi bit-bozulmus dosya yapi yuruyusunu gecer ve motor ayristiricisina girer (Development'ta `check` riski; Shipping'de sessiz); disk bozulmasi kapsami disi. (b) Yapi yuruyusu motor surumune bagli (5.7); motor yukseltmesinde `codec/*` ve `TestCharacterStore` yeniden kosulmali, yoksa tum dosyalar "bozuk" gorunur (Save yine eski dosyayi `.bad`'e ayirir, silmez). (c) Tam flush (`Flush(true)`) davranisi testle gozlenemez (guc kesintisi simulasyonu yok). (d) `MMOMailComponent.cpp:82` (R3) ve yedek arac (R2) degismedi.
- **Kosu sonuclari (2026-11-01, worktree `shutdown-l0`, editor kapali):** `TestSlotFileAtomicReal.ps1` PASS checks=62 (reported = case_lines = unique_labels = 62, `saved_savegames_untouched=True`); `TestSlotFileAtomic.ps1` (S6.0) PASS 144; `TestSlotFileAtomicRealMutations.ps1` control CLEAN + 16/16 CAUGHT; `TestSlotFileAtomicMutations.ps1 -Only m16` (guncellenen eslesme) CAUGHT. Mevcut testler (hepsi PASS, cikis 0): `TestCharacterStore` (worktree `Saved/SaveGames`'e 821 gercek eski `.sav` kopyalanip: slots=826 loaded=826 invalid=0 loadFailed=0 existsMismatch=0, json ok=826; kopyalar test sonrasi bayt-bayt ayni, `cmp` farki 0, sonra kaldirildi), `TestNameLock` 82, `TestMail` 144, `TestAchievements` 102, `TestDaily` 87, `TestServerLifecycle` 290, `TestShutdownPlan` 82, `TestSupabaseStore` 599, `TestFusion` 180, `TestTradeConfirm` 352, `TestTrade` 230, `TestTradeTransfer` 2644, `TestSaveLifecycle` 26, `TestStatsSaveBudget` 15, `TestZoneTravelDeferredSave` GECTI, `TestRemoteExecGuard` 82, `TestMailRefund` ve `TestCharacterSaveJson` cikis 0. Calistirilmadi: PIE/uzak istemci, gercek guc kesintisi, `TestShutdownSmoke.ps1` (S6.3).

## 12. S6.2 durum notu (2026-11-01, bagimsiz guvenlik denetimi + QA bulgularinin kapatilmasi; izole worktree'de derlendi ve kosuldu)

Kaynak: `ag-guvenlik-denetcisi` + `qa-kontrolcu` bulgulari (`8ba18e7` uzerine). Kararlar sahip adina uygulandi. Her madde: ne yapildi.

**Davranis degisiklikleri**
- **G-ORTA-1 (salt-okunur .bak/.tmp kilidi):** `IMMOSlotFileOps::Delete(Path, bEvenReadOnly)`; `true` YALNIZ kodun kendi `.tmp` ve `.bak` dosyalari icin (adim 3, `DiscardTmp`, adim 5 on-silme, adim 7). Ana dosya ve `.bad*` HICBIR yolda `bEvenReadOnly` ile silinmeye calisilmaz (test `inject/even_readonly_delete_scope_is_only_tmp_and_bak`). Ana->`.bak` tasimasindan sonra `ClearReadOnly(.bak)`. Eski gecerli `.bak` varsa adim 5'te once acikca silinir (Move(Replace) hedefi salt-okunursa silemez); ana dosya o sirada gecerli ve yerinde oldugundan kayip penceresi yok, temiz adim sayisi ayni kalir (5). `stale_bak_left` icin ayri metrik `ev=save_stale_bak`. Sonuc: salt-okunur ana `.sav` ile ardisik iki Save de BASARILI, `.bak` kalmaz (eski beklenti "ikinci Save fail-closed" kalkti; `inject/readonly_main_second_save_succeeds_bak_cleaned_no_data_loss`).
- **G-ORTA-2 (kisa tekrar + metrik):** `FMMOFileManagerSlotOps` `ReadAll/Move/Delete` icin en fazla 3 deneme, aralarinda 20 ms, bir nesne icin toplam OLCULEN bekleme butcesi 120 ms (Windows `Sleep` ~15.6 ms'ye yuvarlar; olculen sure birikir; gozlem: 6 hatali islemde ~125-130 ms, `[MMO_SLOTREAL_INFO] retry_budget`). Kalici durumlar (dosya yok, 8 MB ustu, kaynagi olmayan Move) yeniden denenmez. Her Save basarisizligi `[MMOMETRIC] ev=save_failed reason=<kod> player=<izin listeli ad>` basar (gecersiz yuva adinda `player=-`); kodlar: `bad_slot_name not_game_thread serialize roundtrip bad_size invalid_bytes noncanonical main_unreadable bad_cap_reached tmp_delete_failed tmp_write_failed tmp_readback_mismatch rotate_failed promote_failed`. Cagiranlar DEGISMEDI (`AMMOCharacter.*` dokunulmadi; kapanis zinciri zaten `Failed`'i yakalar, gercek kosu bolum 13).
- **G-ORTA-3 (okunamayan ana dosya):** saf katman `EMMOSlotLoadFailure::IoError`: ana dosya VAR ama `ReadAll` basarisizsa `.bak`'a DUSULMEZ (eski veri yeni gibi gelirdi); `.bak` okunamiyorsa (ana yok) da IoError. Magaza `Load` false + `[MMOSAVE] event=load_failed ... reason=io` + `[MMOMETRIC] ev=load_failed reason=io` (icerik bozuksa `reason=corrupt`). Ana dosya yoksa ya da ICERIGI gecersizse `.bak`/`.tmp` kurtarma aynen devam. `Save` zaten `MainUnreadable` ile fail-closed (tutarli). Cagiran akis (`AMMOCharacter::LoadCharacter`): `Exists` true + `Load` false = "exists but could not be loaded; saving disabled", yeni karakter YARATILMAZ.
- **QA-2 (kanonik bayt denetimi):** `Save`, YAZMADAN once uretilen baytlari `.tmp` (siki) dogrulayicisindan (parse + yeniden serilestir + bayt esitligi) gecirir; gecmezse `reason=noncanonical`, ONCEKI dosyaya dokunulmaz (`EMMOSlotSaveFailure::NonCanonical`). Test: 30 girdili, silinmis girisli (delikli) `TMap<FName,int64>` + 20 stack/12+24 ekipman/30 gorev-arkadas-mastery'li gercekci kayit kanonik ve gidis-donus esit.
- **QA-3 (acilis oz-sinamasi):** magazanin ilk `Load/Save/Exists/ListSlotNames` cagrisinda (surec basina bir kez) ornek `UMMOSaveGame` serilestir -> yapi yuruyusu -> ayristir -> kanonik; basarisizsa `Error` log `event=format_selftest_failed` + `[MMOMETRIC] ev=save_selftest_failed` (motor surumu yukseltme uyarisi); davranis DEGISMEZ. Test: `selftest/*`.
- **D1 (Exists/Load tutarliligi):** `Exists` = ana VEYA `.bak` VEYA GECERLI `.tmp` (Load ile ayni kural: `.tmp` yalniz ana ve `.bak` hic yokken kullanilir). `ListSlotNames` yalniz-`.bak` ve gecerli yalniz-`.tmp` yuvalari da listeler (klan adi cakisma taramasi), tekrarsiz; yarim `.tmp`, `.bad*`, `.tmp.bad` yuva sayilmaz. **`MMOMailComponent::RecipientExists`** cevrimdisi dalda artik `UMMOCharacterStoreSubsystem::Get(this)->Exists("MMOSave_<ad>")` kullanir (yalniz `.bak` kalmis alici "yok" sayilmaz; magaza yoksa eski dogrudan dosya bakisi). `MMOMailComponent.cpp` commit'liydi, tek dosya degisti.
- **D2 (parcacik korumasi):** `Save` oyun is parcacigi disinda REDDEDILIR (`reason=not_game_thread`, ensure/check YOK, cokme yok); `Load/Exists` salt-okunur oldugundan korunmaz. **Cok surecli sunucu riski:** `.tmp` adi (`<Yuva>.sav.tmp`) yuva basinadir, surece ozel degildir; ayni `Saved/SaveGames` klasorunu paylasan IKI sunucu sureci ayni oyuncuyu ayni anda kaydederse birbirinin `.tmp`'sini silebilir/yazabilir (kilit yok). Bu surum TEK sunucu sureci varsayar; cok surecli dagitim (S01 / harita basina sunucu) icin yuva basina surec sahipligi (lease) ya da surece ozel `.tmp` soneki (`.tmp.<pid>`) AYRI is olarak gerekir ve bu belgede cozulmus degildir.
- **QA-12 (Move kaynak korumasi):** motor `Move(Replace)` hedefi kaynagin varligina bakmadan ONCE siler (`FileManagerGeneric.cpp:299-323`). `FMMOFileManagerSlotOps::Move` kaynak yoksa HIC denemez (false). Test: `ops2/move_with_missing_source_keeps_existing_dest`.

**Yedek araci (QA-5):** `Scripts/MMOSaveFilesLib.ps1` karakter yan dosyalarini siniflar: `.sav.tmp`/`.sav.bak` = `side`, `.sav.bad`/`.bad.1`..`.bad.4`/`.sav.tmp.bad` = `side-lock` (kota disi `.bad.5` ve buyuk harf `ignored`). Yedege GIRER; varsayilan geri yuklenmez (`side_file_requires_flag`, `-IncludeSideFiles`). Butunluk: `.tmp`/`.bak` JSON degil `.sav` bicimi (boyut + GVAS), yarim `.tmp` `valid=true notes=tmp_unparsable`, bozuk `.bak` `valid=false`. Yan etki: duman testi temizligi (`Get-SmokeCreatedFiles`) artik oyuncunun `.sav.tmp/.bak/.bad` artigini da siler. Testler: `TestBackup.ps1` 1460 -> 1500.

**Operator (D3):** `.bad` kotasi prosedur `docs/BACKUP_OPERATOR_GUIDE.md` bolum 9.

**Mutasyon bosluklari (QA-9):** eklenen testler: WalkStructure surum araligi (1/4/99), eski ozel-surum bicimi, `CustomCount` 4096 kabul / 4097 ret (sentetik tablo), sifir olmayan kontrol uzantisi, `HasPropertyExtensions` bayragi, devasa `PropSize`; `WriteNew` tam flush / `Flush` hatasi / `Tell` uyusmazligi (`TestOpenWrite` sahte tutamagi); `VerifyRoundTrip` bValid karsilastirmasi (Save'in eski satir-ici `Check.bValid` denetimi test edilebilir yardimciya tasindi); `ListSlotNames` (.bak-only / gecerli .tmp / yarim .tmp / .bad / tekrar); `IsValidSlotName` uzunluk sinirlari (128/129), karakter sinifi sinirlari, tam onek. **Esdeger / denenemeyen mutantlar (durust not):** (a) `Start + PropSize > TotalSize` siniri: kalksa da `Seek` sonrasi sonraki `FString` okumasi `Ar.IsError()` ile ayni sonucu verir (motor `FMemoryArchive::Seek` kontrol etmez); davranissal olarak esdeger, mutant yazilmadi (test yine de devasa `PropSize`'in reddedildigini dogrular). (b) `CustomCount < 0` alt siniri: negatif sayi `Seek`'i geri aldirir, sinif adi karsilastirmasi (`ClassName != UMMOSaveGame`) ayni sonuca varir; esdeger. (c) `ListSlotNames` icindeki `.EndsWith(".sav")` joker 8.3 filtresi: `FindFiles` bu makinede `MMOSave_.savx` dosyasini DONDURMEDI (`[MMO_SLOTREAL_INFO] wildcard_quirk_active=0`, UE sonuc adlarini kendisi suzuyor); filtre savunma derinligi, mutant bu ortamda esdeger. Test yine de dosyanin listelenmedigini dogrular. (d) `not_game_thread` korumasinin kaldirilmasi mutanti: korumasiz Save baska parcacikta UObject olusturur (motor `check` ile cokebilir), "yakalandi" yerine "cokme" verir; yazilmadi.

**Tasarimdan sapmalar:** (1) `Delete` imzasi iki bagimsiz degiskenli, `ClearReadOnly` sanal islemi eklendi (S6.0 sahte FS'ye `Locked` kumesi ve salt-okunur bitinin yeniden adlandirmayla tasinmasi eklendi). (2) Eski `.bak` on-silme adimi (Move(Replace)'in ic silmesinin acik hali; sayilan adim sayisi degismedi). (3) `NonCanonical` ve `IoError` yeni durum kodlari. (4) `FMMOLocalFileCharacterStore::TestOpsConfigurator` (yalniz test) gercek `Ops` nesnesine hata enjekte eder (bayat `.bak` silinemedi testi).

## 13. S6.3: duman testi `savefail` enjeksiyonu (2026-11-01, GERCEK kosu yapildi)

- **Neden degisti:** eski enjeksiyon ana `.sav`'i salt-okunur yapiyordu. Atomik Save'de bu Save'i BASARISIZ kilmaz (S6.1 gozlemi: yeniden adlandirma gecer) ve G-ORTA-1 sonrasi kodun kendi salt-okunur `.tmp`/`.bak` dosyalari silinir; yani salt-okunur bayrak `.tmp`'de de yetmez. Yeni enjeksiyon (`Scripts/TestShutdownSmoke.ps1`): ana `.sav` var ve dolu (kanit icin; salt-okunur DEGIL) + oyuncunun `.sav.tmp`'si SALT-OKUNUR ve `FileShare.None` ile ACIK TUTULUR (`Add-SmokeLock`). Save'in "eski .tmp'yi sil" adimi paylasim ihlaliyle 3 denemede basarisiz olur: `reason=tmp_delete_failed`, kapanis zinciri `Failed` -> cikis kodu 2. Temizlikte (`Invoke-SmokeCleanup`) kilit salt-okunur geri almadan ONCE kapatilir, sonra bu kosuda olusan dosyalar silinir.
- **Testler:** `TestShutdownSmokeRunner.ps1` 323 -> 331 (kilit temizligi 4, oyuncunun `.sav.tmp/.bak/.bad/.bad.2` artiklarinin temizlenmesi 2, enjeksiyon lint'i 2); `TestShutdownSmokeEvidence.ps1` 254 (degerlendirici degismedi; `savefail` kaniti zaten `exit 2 + save_failed izi + kick failed`).
- **Gercek kosu:** `TestShutdownSmoke.ps1 -RepoRoot <worktree> -Scenario savefail`: `[MMO_SMOKE_RUN] savefail injection tmp_readonly=True tmp_locked=True`, `server exited on its own after 253s process_exit_code=2`, `[MMO_SMOKE_VERDICT] scenario=savefail verdict=PASS fail=0 inconclusive=0`, `json_exit_code=2`, `flush_failed=1`. Sunucu logundan (oyuncu `Gezgin638721`): `[MMOSAVE] event=save_failed slot=MMOSave_Gezgin638721 reason=tmp_delete_failed bytes=4085` + `[MMOMETRIC] ev=save_failed reason=tmp_delete_failed player=Gezgin638721` (otomatik yazimlar t=86.8/146.8/206.8 s, kapanis turu t=233.1 s, `event=shutdown_save ... result=failed`, atma aninda yeniden yazim t=248.0/248.1 s `shutdown_save_retry ... result=failed`), son satir `[MMOMETRIC] t=253.0 ev=shutdown_exit code=2 failed=1 gone=0 still_connected=0 kick_failed=0`; `MMOShutdownResult.json`: `{"exit_code":2,"failed":1,...}`.

## 14. PIE/elle adimlar (OYNANMADI)

`docs/PLAYTEST_CHECKLIST.md` sonundaki "Karakter .sav atomik yazim (S6.0-S6.2)" basligi: kalinti yok, surec olumu, kesik ana dosya, okunamayan ana dosya (I/O), yalniz-.bak karakter + posta/klan taramasi, salt-okunur ana `.sav`, kilitli `.tmp`, `.bad` kotasi, yedek araci, oz-sinama logu. "Kayit hatasi -> kod 2" maddesi `.sav.tmp` salt-okunur + kilitli enjeksiyonuna guncellendi.
