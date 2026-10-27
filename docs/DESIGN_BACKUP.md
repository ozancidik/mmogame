# Kayit dosyalari yedekleme / dogrulama / geri yukleme araci - tasarim

**Tarih:** 2026-10-27 · **Taban:** `ddcdadd` (master) · **Durum: TASARIM + B0 (kutuphane + test) + B1 (`BackupSaves.ps1`) + B2 (`VerifySaves.ps1`, bkz. §10); B3-B5, derleme yok.** Yazan: `oyun-tasarimcisi`. Sayilar `[PLACEHOLDER]`; gercek oyuncu verisi uzerinde hicbir sey calistirilmadi.
Alan secimi: ilk uc aday arasindan (a) secildi; (b) ve (c) neden elendi: §9.

## 1. Oyuncu deneyimi ve neden simdi

Oyuncu bir gun sunucuya girer ve karakteri yoktur, yarim kalmistir ya da bir hata yuzunden bozulmustur (altin kaybi, bozuk dosya, yanlis ad ezmesi).
Operator (sunucu sahibi) bugun "dosyayi elle kopyala" disinda bir secenege sahip degildir ve geri yukleme bilgisi/provasi yoktur. Bu arac, operatore
**dogrulanmis anlik goruntu al -> bozulmayi erken fark et -> yalniz gereken karakteri guvenle geri koy** imkani verir. Oyuncuya gorunen sey: "kayip ilerleme" olayinin
bir kez olup geri alinabilir olmasi (oyun ici UI yok).

**Neden simdi:**
1. Kapali alfa yakin; kalicilik DB'ye gecmedi (Supabase istemcisi hicbir yerden cagrilmiyor, S04 servisi master'da yok): oyuncu verisi bugun yalniz
   `Saved/` altindaki dosyalardir ve **tek kopyadir**. `docs/DECISIONS_NEEDED.md` Karar 4 onerisi "Kayitlari test oncesi yedekle" der ama bunu yapacak arac yoktur.
2. Grep (2026-10-27): `Scripts/` altinda yedekleme/geri yukleme araci YOK (yalniz `ExportMigratedMailboxes.ps1`, posta tasima hazirligi, canli agaci okumaz; `TestBuildScript.ps1`'deki "backup" sozu baska konu). `docs/SECURITY_AUDIT.md:84` "Yedek/surum yok" diye kayitli.
3. Yeni sunucu yasam dongusu (L0-L3, `docs/DESIGN_SERVER_LIFECYCLE.md`) temiz kapanis kaniti uretir (`Saved/Logs/MMOShutdownResult.json`, `run_id`, `exit_code`): "tutarli yedek almak icin dogru an" artik olculebilir.
4. Tamamen yeni dosyalardir (`Scripts/` ve `docs/`); C++ gerektirmez, sicak dosyalara baglanti **sifir**; derleyici ana agacin Codex kirliligi yuzunden calismasa da gelistirilir ve test edilir.
5. DB gecisinde de ise yarar: yan dosyalar (basarim/gunluk/posta/irk) DB'ye tasininceye kadar dosyada kalacak (`SIX_MAP_SPRINT_BOARD.md` S3 kararı "gecici JSON, DB hazir olunca tasi") ve DB'ye ilk icerik aktarimindan once tutarli kaynak kopya gerekir (`S04_00` yan veri tablosu).

## 2. Kapsam / kapsam disi / mevcutla iliski

**Kapsam (hepsi operator tarafi, sunucu surecinin disinda, cevrimdisi):**
- **Yedek al:** `Saved/` altindaki oyuncu veri dosyalarinin manifestli (SHA-256) anlik goruntusu.
- **Dogrula:** bir anlik goruntunun ya da canli agacin salt-okunur butunluk raporu (bozuk/yarim dosya, yetim yan dosya, `.bad` kilitleri).
- **Geri yukle:** anlik goruntuden tum agac ya da secili karakter(ler); varsayilan kuru calisma; uygulamadan once otomatik "geri-yukleme-oncesi" goruntusu; geri alma.
- Dogrulama, uygulama sonrasi hash yeniden okuma; kod 0/1/2/3 cikis sozlesmesi (`build.ps1` ile uyumlu).

**Kapsam disi (ve neden):**
- Oyun ici komut/RPC/HUD: sunucu otoritesine yeni yuzey acmaz; **degmez** (deger yaratan sey dosya guvenligidir, UI sus).
- Otomatik zamanlayici/servis/Task Scheduler kaydi: ortam degisikligi, operator karari; belge yalniz ornek komut satiri verir (§11 K4).
- Bulut/uzak yukleme ve sifreleme: yerel guven modeli; cilalama (§9).
- Supabase `export_all`/silme RPC (`DESIGN_SUPABASE.md` §3.7, S6): ayri is; bu arac DB'yi okumaz.
- `.sav`'in oyun anlamli dogrulamasi (seviye/altin makul mu): PowerShell yapamaz; istege bagli native dilim B5.
- `Saved/Logs`, `Autosaves`, `Config`, `SourceControl`, `ShaderDebugInfo`, `Saved/MMO/LLMDailyBudget.txt` (oyuncu verisi degil; yeniden uretilir ya da onemsiz).

**Mevcut sistemlerle iliski (var olan yeniden yazilmaz):**
| Veri | Yol (koddan dogrulandi) | Not |
|---|---|---|
| Karakter | `Saved/SaveGames/MMOSave_<ad>.sav` (`MMOLocalFileCharacterStore.cpp:39`, GVAS baslikli; 2026-10-27'de ornek dosyada `GVAS 03 00 00 00` goruldu) | Ayni klasorde `MMOSave_` olmayan `.sav` olabilir: `ListSlotNames` onlari atlar, arac da atlar ve **raporlar** |
| Basarim | `Saved/MMOAchievements/<slot>.json` (+`.tmp`,`.bad`,`.tmp.bad`) (`MMOAchievementComponent.cpp:359,379`) | Slot = `MMOSave_<ad>` |
| Gunluk | `Saved/MMODaily/<slot>.json` (+ayni yan dosyalar) (`MMODailyComponent.cpp:324,344`); ornek dosya UTF-8 BOM'suz, CRLF, `"version"` alanli | |
| Posta | `Saved/MMOMail/MMOMail_<ASCII ad, kucuk harf>.json` (+`.bak`,`.tmp`,`.bad`,`.tmp.bad`) (`MMOMailStore.cpp:114,189`) | Kurtarma sirasi `.bak` > `.tmp` (`:65-93`): yan dosyalar yedege **birlikte** girmeli |
| Irk (Codex, **commit edilmemis**) | `Saved/MMORace/MMORace_<SlotKey(slot,64).ToLower()>.json` (+`.tmp`,`.bad`) (`MMORaceStore.cpp:39-47`); slot `MMOSave_<ad>` oldugundan **gercek dosya `MMORace_mmosave_<ad kucuk>.json`** (`AMMOCharacter::GetSaveSlotName`, `MMOCharacter.cpp:1117-1134`; anahtar en cok 8+32 = 40 karakter, sinir 64) | **`.bad` = secim kilidi** (`:144`): yedekten/geri yuklemeden dusurulemez. Dosya Codex'te; arac yalniz **dosya adi kalibina** gore calisir, `MMORace*` koduna bagimli degildir. Kutuphane `mmosave_` onekini soyup `<ad>` grubuna esler (karakter/posta ile ayni slot grubu); eski varsayim `MMORace_<ad>.json` de geriye uyumlu kabul edilir ama ikisi birlikte bulunursa cakisma sayilir (§4.1) |
| Temiz kapanis kaniti | `Saved/Logs/MMOShutdownResult.json` (`run_id`, `exit_code`, `time_utc`; `MMOServerLifecycle.cpp:441-488`) | Yalniz OKUNUR (§5) |
| Operator islemleri | `Scripts/build.ps1`, `TestMailRefund.ps1` | UE/UBT calisiyor mu CIM sorgusu + cikis 3 "ertelendi" kalibi alinir (`build.ps1:215,309`); `ExportMigratedMailboxes.ps1`'deki guvenli yol-yaprak denetimi (`Assert-SafeLeaf`) ilke olarak alinir, kod kopyalanmaz (farkli kapsam) |

## 3. Dosya haritasi

### 3.1 Yeni dosyalar (hepsi bugun yok; `git ls-files` ana agac + tum acik worktree'lerde `backup|restore|verifysaves|SaveFilesLib|SaveIntegrity` 0 isabet, 2026-10-27)

| Dosya | Icerik | Dilim |
|---|---|---|
| `Scripts/MMOSaveFilesLib.ps1` | Dot-source edilen kutuphane: dosya siniflandirma (karakter/yan/yan-kilit), ad kurali, guvenli goreli yol dogrulama, manifest olusturma/ayristirma/dogrulama, GVAS ve JSON butunluk denetimi, slot grubu cikarma | B0 |
| `Scripts/TestBackup.ps1` | Fikstur tabanli test (gecici dizin, sentetik `.sav`/JSON; gercek `Saved` okunmaz/yazilmaz); `PASS checks=N` | B0-B3 |
| `Scripts/BackupSaves.ps1` | Yedek al (varsayilan: yalniz sunucu/editor kapaliyken) | B1 |
| `Scripts/VerifySaves.ps1` | Anlik goruntu ya da canli agac icin salt-okunur butunluk raporu | B2 |
| `Scripts/RestoreSaves.ps1` | Kuru calisma / uygula / geri al | B3 |
| `docs/BACKUP_RESTORE.md` | Operator kilavuzu (komutlar, is akisi, kurtarma provasi listesi) | B4 |
| (istege bagli) `Source/MyProject/Private/Tests/MMOSaveVerifySelfTest.cpp` | Native derin dogrulama komutu `MMOVerifySaves` (salt okuma) | B5 |

### 3.2 Dokunulacak mevcut dosyalar
**Zorunlu mevcut dosya degisikligi: 0** (B0-B3). B4'te yalniz: `Scripts/RunAllChecks.ps1` (bir adim ekleme) ve `CLAUDE.md`/`docs/PLAYTEST_CHECKLIST.md` satiri (`belge-denetcisi`).

### 3.3 Codex cakisma riski (`git status` + `git worktree list`, 2026-10-27)
- Kirli ana dizin: `RunSystemTests.ps1`, `MMOCharacter.*`, `MMOPlayerController.cpp`, `MMOHUD.*`; izlenmeyen `MMORace*`, `MMOZoneLayout*`, `TestRace.ps1`, `MMORaceSelfTest.cpp`, `.agents/`, `.codex/agents/*`, `nul`. **Yeni dosyalarin hicbiri bunlarla ortusmez.**
- `RunAllChecks.ps1` ve `RunSystemTests.ps1` sicak: baglanti (B4) **bekler**; B0-B3 `Scripts/TestBackup.ps1` olarak tek basina calisir (kalip: `TestFusion.ps1`, `TestMailRefund.ps1`: baska adim degil).
- `docs/BACKUP_RESTORE.md` (B4) ile `DESIGN_SUPABASE.md` §3.7 / S04 `S04_ADR_VE_KABUL_PLANI.md:167` ("Yedekten geri yukleme: ACIK") ayni konuya deginer ama farkli katman (DB yedegi): cakisma degil, capraz baglanti satiri B4'te.
- S04 (Codex, `s04-p0-hardening`, `Scripts/MMOPersistenceService.py`, `Migrations/V2`) dosya adlariyla cakismaz. **Anlamsal cakisma:** S04/Supabase karari kazanirsa karakter `.sav` yedegi gecersizlesir (§8 R3, sunset).
- Derleme gerekmez; B5 gelirse izole worktree (`fusion-d1`/`shutdown-l0` deseni) ve tek-derleyici kurali.

## 4. Davranis (kurallar)

### 4.1 Anlik goruntu bicimi
`<BackupRoot>\<yyyyMMddTHHmmssZ>-<etiket>\` (etiket: `[a-z0-9-]{0,24}`, varsayilan `manual`):
```
files\SaveGames\MMOSave_<ad>.sav
files\MMOAchievements\<slot>.json[.tmp|.bad|.tmp.bad]
files\MMODaily\<slot>.json[...]
files\MMOMail\MMOMail_<ad>.json[.bak|.tmp|.bad|.tmp.bad]
files\MMORace\MMORace_mmosave_<ad>.json[.tmp|.bad]  (varsa; gercek ad kalibi, bkz. §2 tablo)
manifest.json                                          (EN SON, .tmp -> Move)
```
`manifest.json`: `format_version` 1, `created_utc` (`yyyy-MM-ddTHH:mm:ssZ`), `tool` adi/surumu, `source_saved_dir` (tam yol), `mode` (`quiesced` | `live`), `shutdown_proof` (`null` ya da `{run_id ^[0-9TZ-]{1,40}$, exit_code tam sayi 0..255, time_utc}`),
`entries[]` = `{path (goreli, / ayracli), class, size (0..8 MB), sha256, valid (bool), notes}`, `entries_digest` = sirali (ordinal) `path\0class\0sha256\0size\0valid\0notes\n` satirlarinin SHA-256'si
(**valid ve notes dahil**: yalniz dosya hash'ini tutarli yeniden hesaplayan saldirgan "bozuk" girisi "gecerli" yapamaz) ve `header_digest` = ust duzey alanlarin (`format_version`, `tool`, `created_utc`, `source_saved_dir`, `mode`, `shutdown_proof`) SHA-256'si.
Dogrulama (`Test-MMOSnapshot`) her girisin butunlugunu (`valid`/`notes`) dosya icerigine gore YENIDEN hesaplar; manifestteki `valid` ile farkliysa `valid_mismatch:<yol>` (notes icin `notes_mismatch`).
Her dosya tek kez bellege okunur (8 MB ust sinir); boyut, hash ve gecerlilik o diziden hesaplanir (okuma-kullanma arasinda dosya degisemez). Manifest yazimi `.tmp` (`CreateNew`: var olana yazmaz) -> Move; goruntu dizini ve atalari reparse point olmamali.
**Cakisma kapisi:** ayni (tur, sonek, kanonik grup) icin birden cok yol (`Ali Baba` + `Ali_Baba`, `MMOSave_ALI` + `MMOSave_ali`, irk `MMORace_ali` + `MMORace_mmosave_ali`) manifest uretimini ve okumayi reddeder.
**Yan dosya politikasi (geri yukleme, B3):** sonekli yan dosyalar (`.tmp`, `.bak`, `.bad`, `.tmp.bad`; irk/basarim/gunluk/posta `.bad` kilitleri dahil) varsayilan geri yuklenmez, yalniz `-IncludeSideFiles` ile (`Get-MMORestoreSelection`).
**Manifest yoksa goruntu YARIM'dir:** dogrulama/geri yukleme reddeder, `Verify` "incomplete" raporlar, tasima/silme yapmaz.

### 4.2 Yedek al (`BackupSaves.ps1`)
1. On kosullar (hepsi **fail-closed**; hata = cikis 3 "ertelendi", hicbir sey yazilmaz): `UnrealEditor`/`UnrealEditor-Cmd`/`MyProject*`/`UnrealBuildTool` sureci yok (CIM sorgusu **basarisiz olursa de** ertelenir); `BackupRoot` kilidi (`<BackupRoot>\.lock`, ozel acilis) alinabilir; kaynak ve hedef reparse point (junction/symlink) degil, birbirinin atasi/torunu degil, hedef `Saved\` ya da repo icinde degil.
2. `-AllowLive` (acik bayrak): yukaridaki surec denetimi atlanir; her dosya icin kopya sonrasi kaynak yeniden okunur ve hash/boyut kopyayla eslesmezse en cok 3 kez yeniden denenir; hala tutarsizsa o dosya `valid=false notes=unstable`, goruntu `mode=live`, cikis 1. **Canli goruntu "tutarli" diye isaretlenmez** (§8 R1).
3. `-RequireCleanShutdown` (onerilen is akisi): `Saved/Logs/MMOShutdownResult.json` var, JSON ayrisiyor, `run_id` kalibi `^[0-9TZ-]{1,40}$`, `exit_code==0` degilse ertelenir (cikis 3). Dosyanin tek basina "bu sunucu simdi kapali" kaniti olmadigi (L-§11 uyari: dosya yoksa temiz kapanis yok; eslesmeyen run_id bayat) manifestte `shutdown_proof` olarak **kaydedilir ama kapi yine surec denetimidir**.
4. Kaynak kume: yalniz beyaz liste (§2 tablosu): dizin + dosya adi regex + uzanti. Kalan her sey `ignored` olarak raporlanir (sayi + adlar; adlar yalniz yerel ciktiya).
5. Kopya `files\...` altina `.partial` adiyla yazilir, hash dogrulanir, sonra adlandirilir; sonda manifest `.tmp` -> Move. Herhangi bir hatada goruntu dizini **silinmez**, `manifest.json` yazilmaz (yarim kalir; sonraki `Verify` raporlar). Cikis 1.
6. Cikis: 0 tum dosyalar valid + manifest yazildi; 1 hata/tutarsiz/bozuk dosya var; 2 kullanim; 3 ertelendi.
7. Bozuk (gecersiz) dosya **yine de** yedege girer (`valid=false`) - delil/kurtarma icin - ama toplam sonuc 1 olur; operator "yedek var ama temiz degil" bilgisini alir.

### 4.3 Dogrula (`VerifySaves.ps1`, salt okuma)
- Hedef: bir goruntu dizini (`-Snapshot`) ya da canli agac (`-Live`, yalniz okur, dosyalara dokunmaz: `Move`/`Delete` yok; **oyunun `Load` kurtarma yan etkilerini TETIKLEMEZ** - posta/basarim/gunluk `Load` yollari tasima/silme yapar, `S04_00` notu; bu arac onlari cagirmaz, dosyayi bayt olarak okur).
- Kontroller: (a) `.sav`: boyut >= `MinSavBytes` `[PLACEHOLDER 64]`, ilk 4 bayt `GVAS`; (b) JSON (en cok 8 MB): katı UTF-8 (BOM'suz kabul, BOM'lu uyari), `ConvertFrom-Json` basarili, kok nesne, `version` alani tam sayi **1..1000** ve dizin (tur) basina zorunlu alanlar — kaynak yazan C++ kodudur: basarim `unlocked` (dizi), gunluk `day` (sayi) + `bounties` (dizi), posta `mails` (dizi), irk `race` (sayi) (`MMOAchievementComponent.cpp:213-228`, `MMODailyComponent.cpp:187-202`, `MMOMailStore.cpp:120-131`, `MMORaceStore.cpp:58-59`); `.sav` en cok 8 MB; (c) ad kurali (`MMONameRules`: ASCII `[A-Za-z0-9 _-]`); (d) buyuk/kucuk harf yalniz-farkli cift (Windows'ta ayni dosya, baska makinede iki dosya) uyarisi; (e) yetim: yan dosya var ama `.sav` yok (uyari; irk/posta yetimi dogal olabilir) ve `.sav` var yan dosya yok (bilgi); (f) `.bad`/`.tmp.bad`/`.tmp`/`.bak` varligi **uyari olarak listelenir** (ozellikle irk `.bad` = kilit); (g) manifest modunda: her giris boyut+hash, `entries_digest`, manifestte olmayan fazladan dosya (karisik goruntu).
- Cikti: ozet satirlari (`ok= warn= bad= ignored=`), sonra ayrintilar; makine okunur `-Json` secenegi; cikis 0 (bad=0), 1 (bad>0), 2 kullanim.

### 4.4 Geri yukle (`RestoreSaves.ps1`)
- Varsayilan **kuru calisma** (neyi nereye yazacagini yazar). Uygulamak icin `-Apply`. **Sunucu/editor calisiyorsa yazim yok, istisna bayragi yok** (canli bir sunucunun uzerine yazmak sonraki otomatik kayitla geri ezilir ve ad kilidini/lease'i bozar).
- Sira: (1) goruntuyu tam dogrula (B2 mantigi; tek hash uyusmazligi = ret, kismen uygulama yok); (2) kapsam: `-All` ya da `-Slots a,b` (slot grubu: `.sav` + `MMOAchievements/<slot>` + `MMODaily/<slot>` + `MMOMail/MMOMail_<ad kucuk>` + `MMORace/MMORace_mmosave_<ad kucuk>`; sonekli yan dosyalar yalniz `-IncludeSideFiles` ile; harf eslemesi `MMONameRules::CanonicalKey` mantigi: bosluk -> `_`, ASCII kucuk harf; hedef Windows'ta buyuk/kucuk harfe duyarsiz, goruntu icinde ayni anahtarli iki dosya = ret); (3) **geri-yukleme-oncesi goruntu**: etkilenecek canli dosyalar (yan dosyalar dahil) `<BackupRoot>\pre-restore-<ts>-<etiket>\` altina ayni bicimde yedeklenir; basarisizsa **hicbir dosyaya dokunulmaz**, cikis 1; (4) her hedef dosya `.restore.tmp` olarak yazilir, hash dogrulanir, sonra Move (uzerine yazma) ile yerine konur; (5) kapsamdaki slot icin goruntude OLMAYAN canli `.tmp`/`.bak` yan dosyasi pre-restore dizinine **tasinir** (silinmez) - aksi halde oyunun `.tmp`/`.bak` kurtarma mantigi eski artigi geri getirebilir; `.bad`/`.tmp.bad` kilitleri (irk secim kilidi dahil) **tasinmaz ve dusurulmez** (B0 QA/guvenlik turu karari); (6) uygulama sonrasi tum yazilanlar yeniden hash'lenir, uyusmazlikta cikis 1 ve rapor "geri al: `RestoreSaves.ps1 -Snapshot <pre-restore dizini> -All -Apply`".
- **Ekonomi/butunluk uyarisi:** tek karakteri eski goruntuden geri yuklemek tum-sunucu tutarliligini bozabilir (o karakterden sonra gonderilmis posta altini alici tarafta kalir = altin cogalir; takas edilmis esya iki yerde var olabilir). `-Slots` kullanimi `-AcknowledgeCrossSlotRisk` ister, cikti riski acik yazar. `-All` (tum agac tek goruntuden) tutarlidir.
- `-All` geri yuklemesi goruntude olmayan canli `.sav`'lara **dokunmaz** (silme yok); rapor "N canli dosya goruntude yok" der.
- Geri alma ayrica kod gerektirmez: pre-restore goruntusu normal goruntudur.

## 5. Sunucu-otorite, replikasyon, kimlik, kalicilik

- **Sunucu otoritesi:** bu arac oyun surecinin **disindadir**; oyun koduna, RPC'ye, replikasyona, `MMOSaveGame.h`'ye, `Config/`'e dokunmaz. **Yeni RPC yok, yeni replike alan yok, yeni oyun ici yetki yuzeyi yok.**
- **Kimlik (K-1):** oyuncu adi hala kimlik; arac ad/slot ile bir kimlik karari VERMEZ, yalnizca dosya adi kalibini dogrular ve slot gruplarini birlestirir. Geri yukleme bir ad kilidi kararidir: yalniz **sunucu sahibi** (dosya sistemi yetkisi) yapar; uzaktan tetiklenemez (komut yok).
- **Kalicilik:** oyuncu verisi icin **yeni alan/sema yok**. Arac yalniz yeni bir uretilmis cikti uretir (anlik goruntu dizini) ve `Saved/` icine yalniz `-Apply` ile, ayni dosya adlarina yazar. `.gitignore` zaten `Saved/`'i kapsar; **BackupRoot varsayilani repo ve `Saved/` disindadir** (`<projenin ust dizini>\MMOBackups`, §11 K1); `Saved/` icine yedek yazmak reddedilir (`build.ps1 -Clean` `Saved/`'e dokunmaz ama baska temizlikler dokunabilir; yedek asil verinin yaninda durmaz).
- **Yasam dongusu baglantisi (salt okuma):** `Saved/Logs/MMOShutdownResult.json` yalniz `-RequireCleanShutdown`'da okunur; arac bu dosyayi yazmaz/silmez. Cikis kodu 2 ya da dosya yok = "temiz kapanis yok": operator yine yedek alabilir (`-AllowLive` degil, sunucu kapali), manifestte `shutdown_proof=null` yazar.
- **Asenkron/DB magaza geldiginde:** `-MMOCharacterStore`/`-MMOStore=db` master'da yok (S04 adayi `bfc197d`, master degil). Gelirse arac `.sav` dizininin **otorite olmadigini** bilmez; belge ve B4 yardim metni "yalniz yerel dosya magazasi icin" der. Otorite DB oldugunda B4 kosulu: arac yan dosya (basarim/gunluk/posta/irk) kapsamina daraltilir ya da emekli edilir (§8 R3).
- **Guvenlik gereksinimleri (hepsi test edilir, §7):**
  1. *Yol kacisi:* manifest ve komut satiri yollari `..`, mutlak yol, surucu harfi, ADS (`:`), ayrilmis aygit adlari (`CON PRN AUX NUL CONIN$ CONOUT$ CLOCK$ COM0-9/¹²³ LPT0-9/¹²³`; ¹²³ = U+00B9/B2/B3), sonda nokta/bosluk, kontrol karakteri iceremez; yalniz beyaz liste dizin+regex (`Resolve-MMOSafePath` icinde siniflandirma ZORUNLU, beyaz liste disi = ret); birlestirilmis tam yol hedef kokun altinda olmali (canonical karsilastirma). Genel girisler yolu `ConvertTo-MMOFullPath` ile mutlaklastirir; goreli yol **PowerShell konumuna** gore cozulur (`[Environment]::CurrentDirectory` farkli olsa da .NET API'leri ve cmdlet'ler ayni dosyayi gorur); `\\.\`, `\\?\`, surucu-goreli `C:x` reddedilir.
  2. *Reparse point:* kaynak/hedef kok, **kokun atalari** ve her dosya reparse point ise ret (S03 QA'sinda junction yolu gercek bulgu idi). Atalarda reparse yalniz bilincli `-AllowRootAncestorReparse` parametresiyle kabul edilir (ornek: bulut/baglanmis disk); varsayilan ret. Not: bu makinedeki .NET Framework 4.x'te atasi junction olan yolda `FileMode.CreateNew` hep basarisiz olur; bayrakla `Write-MMOManifest` bos dosya kosuluyla `OpenOrCreate`'e duser (atomik degil).
  3. *Manifest sahtekarligi:* hash/boyut/`entries_digest`/`header_digest` uyusmazligi = ret; `valid`/`notes` icerikten yeniden hesaplanir. Ama bkz. **imzasiz yedek siniri** asagida.
  4. *Fail-closed:* surec sorgusu, kilit, kok dogrulamasi ya da manifest okuma hatasi = yazimsiz cikis (3 ya da 1). Dosya sistemi sorgusu (`Get-MMOAttributes`) "yok" ile "erisilemiyor"u ayirir; erisilemeyen yol reparse point gibi **reddedilir** (`Test-MMOReparse` `$true`); dizin listeleme hatasi (`Get-ChildItem -ErrorAction Stop`) `tree_unreadable` ile durur, sessizce atlanmaz.
  5. *Icerik sinirlari:* `.sav`, JSON yan dosya ve manifest en cok 8 MB (sabit); JSON `version` 1..1000 ve dizin basina zorunlu alanlar; digest hesabi O(n log n) (10000 giris < 5 sn). Bilinen sinir: `ConvertFrom-Json` PS 5.1'de cok buyuk dosyada hata verebilir (hata = `parse_error`, gecersiz sayilir, fail-closed).

  **Imzasiz yedek neyi korur / neyi korumaz (K6 siniri, B0 QA/guvenlik turu):** manifest ve `files\` ayni dizinde, anahtarsiz SHA-256 ile durur; imza yoktur.
  - *Korur:* kaza ve bozulma (kismi yazim, bit bozulmasi, yanlis kopya, karisik goruntu), tek tarafli degisiklik (yalniz dosya ya da yalniz manifest), `valid`/`notes`/`class`/ust duzey alan tek basina kurcalama, yol kacisi, reparse, cakisan ad.
  - *Korumaz:* hem `files\` hem `manifest.json` icerigini tutarli yeniden yazabilen (yerel yonetici, ayni kullanici, yazma erisimi olan herkes) bir saldirgani; tum alanlar yeniden hesaplanip yeni digest yazilirsa `Test-MMOSnapshot` bunu gecerli sayar. Yedegin **kokeni** (kim, ne zaman aldi) kanitlanmaz; `shutdown_proof` ve `source_saved_dir` yalniz bilgidir.
  - *Onlemler (kod disi + B3 gereksinimi):* (a) B3 **kuru calisma** ciktisi manifestin SHA-256 ozetini (manifest dosyasinin tamami) gosterir; operator bunu yedek alinirken kaydettigi ozetle (`backup.log`, ayri ortam/kagit/mesaj) karsilastirir; (b) yedek dizini ACL'i yalniz operatore kisitlanir (diger kullanicilar/servis hesaplari yazamaz; B4 belgesi `icacls` ornegi verir); (c) yedekler baska makineye/buluta gidecekse sifrele/imzala (arac disi, §8 K6); (d) `-Apply` yalniz operatorun onayladigi ozet eslestiginde calistirilir (B3 `-ExpectedManifestSha256` parametresi onerilir).
  5. *Gizlilik:* anlik goruntuler oyuncu adi, altin, posta metni icerir (kisisel veri). Repo/`Saved` disinda tutulur, git'e girmez (`BackupRoot` repo icindeyse ret), bulut klasorune koymak operator sorumlulugu (belge uyarir). Arac log/metrik satirina **oyuncu verisi icerigi** yazmaz; dosya adlari yalniz yerel konsola.
  6. *Gunluk/metrik:* `[MMOMETRIC]` satiri BASILMAZ (arac oyun logu uretmez); sonuc `<goruntu>\backup.log` ve konsol.
  7. *Eszamanlilik:* `BackupRoot` kilidi; ikinci arac ayni kokte "ertelendi" (3).
  8. *Silme yok:* B1-B3 hicbir dosya silmez (yalniz `.partial`/`.restore.tmp` kendi olusturduklari ve basarisiz oldugunda birakilir, adlandirmayla taninir). Budama B4'te ayri, kuru-calisma varsayilanli bayrak.

## 6. Ekonomi / denge etkisi
Altin/XP/esya girisi-cikisi **yok**; arac oyun durumunu degistirmez. Tek ekonomik risk geri yukleme kaynaklidir (§4.4: kismi geri yukleme altin cogaltabilir/kaybettirebilir): onlem bayrak + belge, tum-agac geri yukleme onerilir. `[PLACEHOLDER]` sayilar: `MinSavBytes 64`, yeniden deneme 3, ertelenme bekleme yok (operator tekrar calistirir). Olcum ihtiyaci: gercek bir 10-30 oyunculu alfada anlik goruntu boyutu/suresi (bugun `Saved/SaveGames` ~3.9 MB / 809 dosya, cogu test artigi `Gezgin######` ve `CIDIK-*`; arac bunlari gercek oyuncu ile ayirt edemez, hepsini alir; boyut sorun degil).

## 7. Kabul olcutleri, dilimler ve test plani

Her dilim ayri dogrulanabilir ve ayri commit. **Tum testler gecici fikstur dizininde calisir; gercek `Saved/`'e yazilmaz, ondan okunmaz** (test bunu `Saved` yolunu parametreyle verip gercek yolu fikstur disi ise reddederek ve cikista gercek `Saved\SaveGames` dosya sayisi/en-son-yazim-zamani karsilastirmasiyla dogrular).

| Dilim | Is | "Bitti" = |
|---|---|---|
| **B0** | `MMOSaveFilesLib.ps1` + `TestBackup.ps1` (saf mantik + fikstur) | PS 5.1 ve PS 7'de `PASS checks=N` (N = benzersiz etiketli vaka sayisi, uc yerde tutarli: ps1 `$expected`, test cikti satiri, commit mesaji; `TestServerLifecycle.ps1` deseni), hata 0; asagidaki mutasyon M1-M8 hepsi FAIL verir; gercek `Saved` degismedi kaniti |
| **B1** | `BackupSaves.ps1` | Fikstur agacinda: yedek alir, manifest en son yazilir, hash'ler dogru; surec varken (sahte `Get-CimInstance` kancasi) ertelenir (3); CIM hatasi ertelenir; `-AllowLive` kararsiz dosyayi `valid=false` yapar (kopya sirasinda degisen fikstur); yarim goruntu manifestsiz; kok `Saved` icinde/repo icinde/reparse ise ret; M9-M14 yakalanir |
| **B2** | `VerifySaves.ps1` | Bozuk GVAS, kisa dosya, bozuk/BOM'lu/bos JSON, yetim, harf-cifti, `.bad` uyarisi, fazladan dosya, hash degismis, digest degismis, manifest yok senaryolari dogru siniflanir; `-Live` hicbir dosyayi degistirmez (oncesi/sonrasi hash ve LastWriteTime esit); M15-M18 |
| **B3** | `RestoreSaves.ps1` | Kuru calisma yazmaz; `-Apply` pre-restore goruntusunu **once** alir (alinamazsa dosya degismez); tek hash bozuk -> hic yazim yok; slot grubu dogru dosyalari kapsar, `-Slots` risk bayragi olmadan ret; eski `.tmp`/`.bak`/`.bad` tasinir (silinmez); sonrasi hash dogrular; geri al (pre-restore'dan `-All`) fiksturu eski haline dondurur; surec varken ret; M19-M24 |
| **B4** | `docs/BACKUP_RESTORE.md`, `RunAllChecks.ps1` adimi (sahibi temizleyince), `-RequireCleanShutdown`, istege bagli `-PruneKeep N` (kuru calisma varsayilan, yalniz manifestli + ad kalibi uyan goruntuler), CLAUDE.md satiri | Belge-denetcisi sayi/yol kontrolu; PS5.1'de `RunAllChecks` yesil; yalniz belgelenmis komutlar |
| **B5 (istege bagli)** | Native `MMOVerifySaves` (`FSelfRegisteringExec`, salt okuma; `FMMOLocalFileCharacterStore::Load` + `MMOCharacterSaveJson` zarfi ile her slotu yukleyip makul aralik kontrolu) | Yalniz B2 raporu "GVAS basligi var ama yukleme anlami bilinmiyor" belirsizliginin operatore zarar verdigi kanitlanirsa. Izole worktree derlemesi; uzak Exec korumasi (Y-9) izin listesinde **olmamali** (uzak istemci cagiramaz) |

### 7.1 Mutasyonla sinanacak korumalar (`test-muhendisi`; ps1 mutasyonu: kopya uzerinde ya da dosyada degistir, test calistir, geri yukle, dosyaya `touch`/LastWriteTime)
M1 yol kacisi (`..`) denetimi silinir. M2 reparse-point denetimi silinir. M3 beyaz liste regex'i gevsetilir (`MMOSave_*.sav` yerine `*.sav`). M4 GVAS magic denetimi silinir. M5 manifest `entries_digest` karsilastirmasi silinir. M6 sha256 karsilastirmasi hep dogru. M7 ad kurali (ASCII) silinir. M8 buyuk/kucuk harf cifti denetimi silinir.
M9 surec denetimi atlanir. M10 CIM hatasi "surec yok" sayilir (fail-open). M11 manifest en basa yazilir (yarim goruntu manifestli gorunur). M12 `BackupRoot` `Saved` icinde kabul. M13 `-AllowLive` yeniden okuma denetimi silinir. M14 kilit (`.lock`) alinmaz.
M15 `-Live` dogrulamada bir dosya yazilir/tasinir. M16 `.bad` uyarisi kaldirilir. M17 yetim siniflamasi tersine. M18 fazladan dosya denetimi silinir.
M19 pre-restore goruntusu atlanir. M20 hash dogrulamasi once degil sonra. M21 eski yan dosya tasima kaldirilir. M22 `-AcknowledgeCrossSlotRisk` kapisi silinir. M23 surec denetimi geri yuklemede atlanir. M24 kuru calisma yazar. Yakalanmayan mutasyon testin eksigidir.

### 7.2 Oyun ici (PIE/dedicated) ilk test listesi (kullanici, B3 sonrasi; **gercek verinin kopyasi uzerinde**, canli `Saved/`'in once elle kopyasi alinir)
1. Sunucu kapaliyken `BackupSaves.ps1`: cikis 0, manifest var; `VerifySaves.ps1 -Snapshot`: bad=0.
2. Oyunu ac, karakterle altin harca/esya al, kapat; ikinci yedek; `-Slots <karakter>` kuru calisma: ilk yedege donuste fark listesi mantikli mi.
3. Ilk yedekten `-Slots <karakter> -AcknowledgeCrossSlotRisk -Apply`, oyunu ac: eski altin/esya geldi, cokme/"yeni karakter" yok.
4. Geri al (pre-restore goruntusu): ikinci duruma donus.
5. Planli kapatma (`MMOShutdown 60`) ile cik, `-RequireCleanShutdown` yedek: kabul; sunucu calisirken ayni komut: ertelenir.
6. Bir `.sav`'i elle bozup (kopyada) `VerifySaves`: bad=1, adi dogru.

## 8. Riskler ve acik sorular

| # | Risk / soru | Onlem / oneri |
|---|---|---|
| R1 | Canli (`-AllowLive`) kopya yarim yazim yakalayabilir (UE `SaveGameToSlot` atomikligi bu depoda dogrulanmadi; yan dosyalar `.tmp`->Move kullanir, Move'un hedefi once sildigi gorulur) | Varsayilan yalniz kapaliyken; `-AllowLive` "tutarli" demez, `mode=live` + yeniden okuma; kritik yedek icin `MMOShutdown` + `-RequireCleanShutdown` |
| R2 | Kismi geri yukleme altin/esya cogaltir ya da kaybettirir (posta/takas capraz etkisi) | `-AcknowledgeCrossSlotRisk`, `-All` oner, belge |
| R3 | S04/Supabase kazanirsa `.sav` yedegi anlamsiz olur; DB yedegi/`export_all` ayri is | Sunset sartı: otorite DB olunca arac yan dosyalarla sinirlanir ya da emekli (`DECISIONS_NEEDED` S04 karari); arac maliyeti dusuk (5 dosya, C++ yok) |
| R4 | `MMORace*` Codex'in commit edilmemis kodu; dosya adi kalibi ve `.bad` anlami degisebilir | Arac yalniz ad kalibi + "yan dosya, `.bad` korunur" ile bilir; Codex commit ettiginde B3 irk vakalari yeniden okunup tek satir uyarlanir |
| R5 | PowerShell 5.1 kodlama tuzagi (`Get-Content -Raw` + `WriteAllText` cift kodlar, bellekte "JSON'u yeniden yazmak") | Arac dosya iceriklerini **asla yeniden yazmaz**: bayt kopya + hash; JSON yalniz dogrulama icin ayrı okunur |
| R6 | Gercek `.sav` anlam dogrulamasi yok (GVAS basligi yetmez) | Rapor bunu acikca soyler ("baslik gecerli, yukleme dogrulanmadi"); B5 istege bagli |
| R7 | Test artigi yuzlerce `Gezgin######`/`CIDIK-*` slotu gercek oyuncudan ayirt edilemez | Hepsi yedege girer (boyut ihmal edilebilir); ad filtresi onerilmez (`Gezgin######` PreLogin'in gercek yedek adidir) |
| R8 | Operator yanlis dizine geri yukler / yanlis goruntu secer | Kuru calisma varsayilan; goruntu manifesti `source_saved_dir` ile hedefi karsilastirir, uyusmazlikta ret (`-AcceptDifferentSource` ayri bayrak) |
| R9 | Gercek disk dolulugu/izin hatasi pre-restore sirasinda | Pre-restore basarisizsa dokunmama (4.4); yeterli bos alan on denetimi (kaynak toplam x 2) |

**Kullaniciya kalan kararlar (oneri varsayilanlariyla; "bekleme" degil, onerilen secimle B0 baslar):**
- **K1 BackupRoot varsayilani:** `<projenin ust dizini>\MMOBackups` (`...\ozan online\MMOBackups`), repo ve `Saved` disinda. Baska bir surucu/bulut klasoru ister misin? (oneri: ayri fiziksel disk varsa onu ver; yoksa varsayilan)
- **K2 Saklama:** B1-B3'te silme yok; B4 `-PruneKeep 30` `[PLACEHOLDER]` (≈ 4 MB x 30). Gunluk mu, kapanis basina mi? (oneri: her planli kapanista + gunde bir)
- **K3 Canli yedek (`-AllowLive`)** hic olmasin mi, yoksa acik bayrakla kalsin mi? (oneri: kalsin, uyarili)
- **K4 Zamanlayici:** Windows Task Scheduler gorevi bu depoda kurulmaz; belge ornek komutu verir, kurmak sende. (oneri: kur)
- **K5 Kismi geri yukleme** bayrakla izinli mi, yoksa yalniz `-All` mi? (oneri: bayrakla izinli, tek oyuncu destegi icin gerekli)
- **K6 Sifreleme/imza:** yok (yerel guven modeli). Yedekler baska makineye/buluta gidecekse sifrele (arac disi). (oneri: simdilik yok)

## 9. "Degmez" notu ve elenen adaylar

- **Elenen (b) `MMOMutationGate` (merkezi mutasyon kapisi):** `RefuseWhenFlushing` zaten var ve 16 yolda kullaniliyor (envanter 7, ticaret 6, gorev, posta, birlestirme). Kalan acik yollar (mob altin/XP/ganimet, tuketilebilir/iksir, olum cezasi, kusanma) icin kayip penceresi yalniz `Flushing` ile `Kicking` arasi (<= ~20 sn `[PLACEHOLDER]`) **ve** atma anindaki yeniden yazimin da basarisiz olmasidir (`DESIGN_SERVER_LIFECYCLE.md` §12). Gercek olmayan fayda/maliyet: iksir/olum cezasi/kusanma yollari kirli `MMOCharacter.cpp` icinde; merkezi bir "kapi" yeniden yazimi calisan 16 kapiyi dokunmaya zorlar (regresyon riski) ve cok oyunculu davranisi PIE'siz dogrulanamaz. **Daraltilmis, ileride degerli:** yalniz `MMOMob.cpp` + `MMOLootPickup.cpp` (ikisi temiz) icin bir `RefuseWhenFlushing` satiri; ayrica kucuk is, ayri tasarim gerektirmez, bu belgenin parcasi degil.
- **Elenen (c) operator telemetri/saglik ozeti:** `[MMOMETRIC]` satirlari ve `Scripts/PlaytestMetrics.py` zaten var; "tick suresi/oyuncu sayisi" olcumu sicak dosyalara (`MMOGameMode`/`MMOCharacter`) kanca ister ve gercek oyuncu yokken olculecek veri yok: simdi sayi/etiket cilasi olur. Alfa sonrasi.
- **(a) icinde degmez:** sifreleme, bulut yukleme, HUD/oyun ici komut, zamanlayici servisi, otomatik geri yukleme, oyuncuya acik "yedek var" mesaji. Deger: dogrulanmis kopya + guvenli geri yukleme.
- **Dikkat (sahte is olmasin):** yalnizca `Copy-Item` yeterli degildir; degerin uc parcasi (hash/manifest + yarim goruntu tespiti + pre-restore geri alma) olmadan arac sadece "kopya" olur. B0-B3 hepsi bu uc parcayi sinar.

## 10. Ilk dilim (B0) ozeti

**B0 durumu (2026-10-27):** `Scripts/MMOSaveFilesLib.ps1` + `Scripts/TestBackup.ps1` kodlandi (`417dbbc`) ve bagimsiz QA/guvenlik turuyla sertlestirildi: irk dosya adi gercek kalibina cevrildi, `valid`/`notes`/ust duzey alan korumasi, tek okuma (TOCTOU), fail-closed dosya sistemi sorgusu + mutlak yol, cakisma kapisi, icerik sinirlari + dizin semasi + yan dosya politikasi, yol/aygit sertlestirmesi, test kalitesi (bagimsiz sayim, Saved kaniti). Windows PowerShell 5.1'de calistirildi; **PowerShell 7 dogrulanmadi** (pwsh bu makinede yok). `Scripts/TestBackup.ps1`: 759 benzersiz vaka (ps1 `$expected`, cikti satiri, commit mesaji ayni sayi; sayim basilan satirlardan bagimsiz yeniden yapilir). Mutasyon (calisan kopyalar uzerinde, 61 mutasyon): 59'u yakalandi; yakalanmayan 2: okuma sirasinda dosya degisimi kapisi (`changed_during_read`, deterministik tetiklenemedi) ve `manifest.json` dosya-sembolik-baglanti kontrolu (dosya symlink'i ayricalik ister, bu makinede olusturulamadi). B2-B3 yok. Bilinen B0 sinirlari: okuma sirasinda dosya degisimi (`changed_during_read`) ve `root_inaccessible` dali testle tetiklenemedi (sahip hesabinda ACL ile oznitelik okumasi engellenemedi; gecersiz yol vakalari `root_invalid`/fail-closed davranisini kanitlar; dizin listeleme reddi `RD` ACL'iyle test edilir); imzasiz yedek siniri §5.

**B1 durumu (2026-10-27, bagimsiz QA/guvenlik turu henuz YOK):** `Scripts/BackupSaves.ps1` (yeni) + `Scripts/MMOSaveFilesLib.ps1` (kucuk eklemeler) + `Scripts/TestBackup.ps1` (955 vaka). Cikis kodlari: 0 basarili; 1 hata/kararsiz/bozuk dosya/kopya hatasi/cakisma/bos kaynak/goruntu zaten var/kendi dogrulamasi basarisiz; 2 kullanim (gecersiz etiket/zaman damgasi/yol, `-AllowLive` + `-RequireCleanShutdown`); 3 ertelendi (surec calisiyor ya da sorgu basarisiz/bos, temiz kapanis kosulu saglanmadi, ozyineleme/repo ici yedek koku, reparse, kaynak/ust dizin eksik, kilit dolu). Sira: parametre -> yol yapisi -> surec -> kaynak kok -> kapanis kaniti -> kaynak listesi -> yedek koku -> `.lock` (`DeleteOnClose`, bayat dosya engellemez) -> goruntu dizini (var olana yazilmaz) -> dosya basina tek okuma + `.partial` + hash dogrulama + adlandirma -> manifest EN SON -> `Test-MMOSnapshot` ile kendi dogrulamasi; basarisizsa `manifest.json` -> `manifest.json.failed` (goruntu yarim olur, delil kalir, silinmez). Kopya hatasinda goruntu manifestsiz kalir ve `backup.log` `status=incomplete` yazar. Bozuk kaynak dosya yedege girer (`valid=false`), cikis 1. `-AllowLive`: surec denetimi atlanir, her dosya yeniden okunur (en cok 3 deneme), hala kararsizsa `valid=false notes=unstable` (kutuphane: `New-MMOManifest -UnstablePaths`; `Test-MMOSnapshot` `unstable` notunu yalniz `valid=false` ile kabul eder, yani saldirgan "gecersiz"i "gecerli" yapamaz), `mode=live` + uyari. `-RequireCleanShutdown`: `Read-MMOShutdownResult` (`run_id`, `exit_code` 0..255, `time_utc`; UE'nin milisaniyeli ISO zamani saniyeye indirilir); `exit_code != 0`, dosya yok/gecersiz = 3; kanit manifeste yazilir ama surec denetimi yine kapidir. Kilit/yol/repo/ozyineleme/reparse reddi dosya yazmadan once. Yedek kokunun ust dizini yoksa olusturulmaz (3). Konsol ve `backup.log` yalniz sayi/neden kodu ve manifest SHA-256 ozeti icerir (oyuncu verisi yok; yok sayilan dosya adlari yalniz konsola). Parametreler: `-ProcessProvider` (surec envanteri enjeksiyonu; testte gercek tarama cagrilmaz), `-Timestamp` (sabit ad), `-TestHooks` (`DuringCopy`/`AfterPartial`/`AfterManifest`, yalniz test). Mutasyon: 39 mutasyon calisan kopyalarda; 38 yakalandi, 1 denk (esdeger) mutasyon yasadi (yedek kokunun reparse atasi on-denetimi: ikinci katman `Assert-MMORootSafe` + .NET'in junction altinda dizin olusturmamasi ayni sonucu veriyor). Bilinen sinirlar: `lock_symlink_rejected` bu makinede ayricalik yokken bos gecer (sembolik baglanti olusturulamiyor); PowerShell 7 dogrulanmadi; gercek `Saved/` uzerinde arac hic calistirilmadi; B2 (`VerifySaves.ps1`) ve B3 (`RestoreSaves.ps1`) yok; B1 tek basina geri yukleme saglamaz.

**B2 durumu (2026-10-27, bagimsiz QA/guvenlik turu henuz YOK):** `Scripts/VerifySaves.ps1` (yeni) + `Scripts/TestBackup.ps1` (1059 vaka; ps1 `$expected`, cikti satiri ve commit mesaji ayni sayi). Kutuphane (`MMOSaveFilesLib.ps1`) degismedi. Hedef: `-Snapshot <dizin>` (tek goruntu) ya da `-BackupRoot <kok>` + `-All` | `-Last N` (ad kalibi `[pre-restore-]yyyyMMddTHHmmssZ[-etiket]` olan girdiler zaman damgasina gore sirali; `-Last` damgaya gore en yeni N, ada gore degil; kalip disi girdiler `foreign=` olarak sayilir, dogrulanmaz; `-Live` (canli agac dogrulama) B2'de YOK, yalniz `-Against` farki var). Dogrulama `Test-MMOSnapshot` (boyut, SHA-256, `valid`/`notes` yeniden hesabi, fazladan/eksik dosya, digest'ler, reparse, 8 MB siniri) + manifest.json ozeti (SHA-256; dogrulama sirasinda degisirse `manifest_changed_during_verify`) + `-ExpectedManifestSha256` (disaridan bilinen ozet; yalniz `-Snapshot`; uyusmazlik = red; tutarli yeniden yazilmis manifest yalniz bu ozetle yakalanir, bkz. §5 imzasiz yedek siniri) + yarim goruntu tespiti (`manifest.json` yok / `manifest.json.failed` / `.tmp` / `.partial` kalintisi -> `status=incomplete` ve `[MMO_VERIFY_INCOMPLETE] reasons=`; manifest varken `.partial` kalintisi `bad`) + `mode=live` uyarisi + `shutdown_proof` raporu + `valid=false` girislerin listesi (`[MMO_VERIFY_INVALID]`, "geri yuklenmeyecek") + `.bad`/`.tmp`/`.bak` yan dosya ve yetim grup uyarilari. `-Against <Saved>`: goruntu ile canli agacin farki (same/changed/only_in_snapshot/only_in_saved; yalniz ad/boyut/hash, canli dosyalar bayt olarak okunur, oyunun `Load` yollari cagrilmaz); farklar hata DEGILDIR (exit 0), canli dosya okunamazsa exit 1, Saved reparse = 3. Cikis kodlari BackupSaves ile uyumlu: 0 tamam (yalniz uyari/live olabilir); 1 dogrulama basarisiz (bad, incomplete, `valid=false` giris, ozet uyusmazligi, `-Against` okunamadi, kokte hic goruntu yok); 2 kullanim (hedef yok/cakisan secici, `-All`+`-Last`, bicimsiz ozet, ozet + kok secimi); 3 ertelendi/erisilemiyor (goruntu ya da kok ya da Saved yok/erisilemiyor/reparse). Cikti: ilk satir `[MMO_VERIFY_SUMMARY] status= reason= exit= snapshots= ok= bad= incomplete= with_invalid= invalid_entries= live= warnings= foreign=`, sonra `[MMO_VERIFY_SNAPSHOT|PROOF|INCOMPLETE|ERROR|INVALID|WARN|AGAINST|DIFF]`; dosya (oyuncu) adlari yalniz yerel konsolda, log dosyasi/kilit YOK. **Salt okunurluk kaniti:** betik hicbir yazma cmdlet'i/metodu icermez (AST testi) ve test, B1 ile alinmis fikstur goruntuleri/yedek kokleri/Saved fikstur agaci (B2 fiksturunun TAMAMI, 22 kosuluk batarya: ok/bad/incomplete/-All/-Last/-Against/ozet/kullanim/ertelendi) uzerinde dogrulama calistirmadan once ve sonra tam damga (ad, boyut, LastWriteTime, icerik SHA-256; reparse izlenmez) alip esitligini dogrular; kilit/log/temp dosyasi olusmaz. Not (test tuzagi): NTFS ust dizindeki onbellekli dizin girdisini gec gunceller, `Get-ChildItem` numaralandirmasi bayat dizin zamani verebilir (bu makinede gozlendi: dokunulmayan dizinlerde ~10 ms fark); damga dizin zamanini `[IO.Directory]::GetLastWriteTimeUtc` ile alir (mevcut `Get-SavedStamp` hala numaralandirma kullanir). Test kancasi: `-TestHooks @{ AfterVerify = {param($dir)} }` (yalniz test). Mutasyon (calisan kopyalarda, 21 mutasyon + 1 kontrol; kontrol kosusunun tek farki kopya ortami kaynakli `bk_root/root_inside_repo_rejected`: kopya betiklerin ust dizini gercek depo degil): 21/21 yakalandi (hash karsilastirma, fazladan dosya, beklenen ozet, yarim goruntu tespiti x2, goruntuye yazma, canli dosyaya dokunma, manifest-degisti kapisi, `valid=false` listesi, live uyarisi, `valid=false` cikis kodu, `-Against` reparse, ada gore siralama, `-Last`, bos kok, `-Against` hash, kutuphanede hash, yetim uyarisi, kilit uyarisi, yabanci sayaci, yarim-goruntu agac taramasi). Bilinen sinirlar: PowerShell 7 DOGRULANMADI; gercek `Saved/` ya da gercek yedeklerde calistirilmadi; `-Live` yok; `manifest_changed_during_verify` yalniz kanca ile sinandi; `-RequireQuiesced` gibi "live goruntu basarisiz sayilsin" bayragi yok (live yalniz uyari); incomplete goruntu kokte durdukca `-All`/`-Last` hep 1 doner (B4 budama/temizleme bunu cozer); `-Against` farklari canli sunucuda dogal olarak cikar (sunucu acikken karsilastirma bilgi amaclidir).

Yeni 2 dosya (B0): `Scripts/MMOSaveFilesLib.ps1`, `Scripts/TestBackup.ps1`. Mevcut dosya degisikligi sifir; C++ ve derleme yok; Unreal calismasi gerekmez (cevrimdisi PowerShell).
B0 kapsami: siniflandirma + ad kurali + guvenli goreli yol + manifest uretme/dogrulama + GVAS/JSON butunluk kontrolu + slot grubu cikarimi, hepsi fikstur dizininde. `mmo-developer` yazar (PowerShell), `test-muhendisi` M1-M8'i kosar, `qa-kontrolcu` + `ag-guvenlik-denetcisi` salt-okunur paralel inceler (yol kacisi, reparse, fail-closed). Gercek `Saved/` hicbir adimda okunmaz.
Ajan agi sirasi: tasarim (bu belge) -> `mmo-developer` B0 -> `test-muhendisi` -> `qa-kontrolcu` + `ag-guvenlik-denetcisi` -> B1..B3 ayni sirayla -> `belge-denetcisi` (B4).
