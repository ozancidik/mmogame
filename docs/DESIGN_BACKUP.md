# Kayit dosyalari yedekleme / dogrulama / geri yukleme araci - tasarim

**Tarih:** 2026-10-27 · **Taban:** `ddcdadd` (master) · **Durum: TASARIM; kod, derleme, test YOK.** Yazan: `oyun-tasarimcisi`. Sayilar `[PLACEHOLDER]`; gercek oyuncu verisi uzerinde hicbir sey calistirilmadi.
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
| Irk (Codex, **commit edilmemis**) | `Saved/MMORace/MMORace_<slot anahtari, kucuk harf>.json` (+`.tmp`,`.bad`) (`MMORaceStore.cpp:39-47`) | **`.bad` = secim kilidi** (`:144`): yedekten/geri yuklemeden dusurulemez. Dosya Codex'te; arac yalniz **dosya adi kalibina** gore calisir, `MMORace*` koduna bagimli degildir |
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
files\MMORace\MMORace_<anahtar>.json[.tmp|.bad]      (varsa)
manifest.json                                          (EN SON, .tmp -> Move)
```
`manifest.json`: `format_version` 1, `created_utc`, `tool` adi/surumu, `source_saved_dir` (tam yol), `mode` (`quiesced` | `live`), `shutdown_proof` (`{run_id, exit_code, time_utc}` ya da `null`),
`entries[]` = `{path (goreli, / ayracli), class, size, sha256, valid (bool), notes}` ve `entries_digest` = sirali `path\0sha256\0size\n` satirlarinin SHA-256'si.
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
- Kontroller: (a) `.sav`: boyut >= `MinSavBytes` `[PLACEHOLDER 64]`, ilk 4 bayt `GVAS`; (b) JSON: katı UTF-8 (BOM'suz kabul, BOM'lu uyari), `ConvertFrom-Json` basarili, kok nesne, `version` alani tam sayi (daily/ach/mail icin ornekte goruldu; tablo `B1`'de gercek dosya ornekleriyle kilitlenir, uydurma alan listesi yazilmaz); (c) ad kurali (`MMONameRules`: ASCII `[A-Za-z0-9 _-]`); (d) buyuk/kucuk harf yalniz-farkli cift (Windows'ta ayni dosya, baska makinede iki dosya) uyarisi; (e) yetim: yan dosya var ama `.sav` yok (uyari; irk/posta yetimi dogal olabilir) ve `.sav` var yan dosya yok (bilgi); (f) `.bad`/`.tmp.bad`/`.tmp`/`.bak` varligi **uyari olarak listelenir** (ozellikle irk `.bad` = kilit); (g) manifest modunda: her giris boyut+hash, `entries_digest`, manifestte olmayan fazladan dosya (karisik goruntu).
- Cikti: ozet satirlari (`ok= warn= bad= ignored=`), sonra ayrintilar; makine okunur `-Json` secenegi; cikis 0 (bad=0), 1 (bad>0), 2 kullanim.

### 4.4 Geri yukle (`RestoreSaves.ps1`)
- Varsayilan **kuru calisma** (neyi nereye yazacagini yazar). Uygulamak icin `-Apply`. **Sunucu/editor calisiyorsa yazim yok, istisna bayragi yok** (canli bir sunucunun uzerine yazmak sonraki otomatik kayitla geri ezilir ve ad kilidini/lease'i bozar).
- Sira: (1) goruntuyu tam dogrula (B2 mantigi; tek hash uyusmazligi = ret, kismen uygulama yok); (2) kapsam: `-All` ya da `-Slots a,b` (slot grubu: `.sav` + `MMOAchievements/<slot>` + `MMODaily/<slot>` + `MMOMail/MMOMail_<ad kucuk>` + `MMORace/MMORace_<ad kucuk>`; harf eslemesi `MMONameRules::CanonicalKey` mantigi: bosluk -> `_`, ASCII kucuk harf; hedef Windows'ta buyuk/kucuk harfe duyarsiz, goruntu icinde ayni anahtarli iki dosya = ret); (3) **geri-yukleme-oncesi goruntu**: etkilenecek canli dosyalar (yan dosyalar dahil) `<BackupRoot>\pre-restore-<ts>-<etiket>\` altina ayni bicimde yedeklenir; basarisizsa **hicbir dosyaya dokunulmaz**, cikis 1; (4) her hedef dosya `.restore.tmp` olarak yazilir, hash dogrulanir, sonra Move (uzerine yazma) ile yerine konur; (5) kapsamdaki slot icin goruntude OLMAYAN canli yan dosya (`.tmp`/`.bak`/`.bad`) pre-restore dizinine **tasinir** (silinmez) - aksi halde oyunun `.tmp`/`.bak` kurtarma mantigi eski artigi geri getirebilir; (6) uygulama sonrasi tum yazilanlar yeniden hash'lenir, uyusmazlikta cikis 1 ve rapor "geri al: `RestoreSaves.ps1 -Snapshot <pre-restore dizini> -All -Apply`".
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
  1. *Yol kacisi:* manifest ve komut satiri yollari `..`, mutlak yol, surucu harfi, ADS (`:`), ayrilmis aygit adlari (`CON/NUL/COMn...`), sonda nokta/bosluk, kontrol karakteri iceremez; yalniz beyaz liste dizin+regex; birlestirilmis tam yol hedef kokun altinda olmali (canonical karsilastirma).
  2. *Reparse point:* kaynak/hedef kok ve her dosya reparse point ise ret (S03 QA'sinda junction yolu gercek bulgu idi).
  3. *Manifest sahtekarligi:* hash/boyut/digest uyusmazligi = ret. Saldirgan hem manifesti hem dosyalari degistirebilirse (yerel yonetici) bu arac onu **engellemez**; tehdit modeli: kaza ve bozulma, kotu niyetli yerel yonetici degil. Operatör imzasi/anahtari yok (§11 K6).
  4. *Fail-closed:* surec sorgusu, kilit, kok dogrulamasi ya da manifest okuma hatasi = yazimsiz cikis (3 ya da 1).
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

Yeni 2 dosya: `Scripts/MMOSaveFilesLib.ps1`, `Scripts/TestBackup.ps1`. Mevcut dosya degisikligi sifir; C++ ve derleme yok; Unreal calismasi gerekmez (cevrimdisi PowerShell).
B0 kapsami: siniflandirma + ad kurali + guvenli goreli yol + manifest uretme/dogrulama + GVAS/JSON butunluk kontrolu + slot grubu cikarimi, hepsi fikstur dizininde. `mmo-developer` yazar (PowerShell), `test-muhendisi` M1-M8'i kosar, `qa-kontrolcu` + `ag-guvenlik-denetcisi` salt-okunur paralel inceler (yol kacisi, reparse, fail-closed). Gercek `Saved/` hicbir adimda okunmaz.
Ajan agi sirasi: tasarim (bu belge) -> `mmo-developer` B0 -> `test-muhendisi` -> `qa-kontrolcu` + `ag-guvenlik-denetcisi` -> B1..B3 ayni sirayla -> `belge-denetcisi` (B4).
