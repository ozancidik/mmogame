# Kayit yedekleme araci - operator kilavuzu

**Tarih:** 2026-11-01 (B4, yalniz belge). Tasarim ve ayrintilar: `docs/DESIGN_BACKUP.md`. Araclar: `Scripts/BackupSaves.ps1`, `Scripts/VerifySaves.ps1`, `Scripts/RestoreSaves.ps1` (kutuphane `Scripts/MMOSaveFilesLib.ps1`).

**Durum (durust sinirlar):** Windows PowerShell 5.1 ile fiksturda test edildi (`Scripts/TestBackup.ps1`, `Scripts/TestBackupMutations.ps1`). **PowerShell 7 dogrulanmadi.** Arac **gercek `Saved/` uzerinde hic calistirilmadi.** Ilk denemeni sahte bir kopya uzerinde yap (bkz. §9). Yalniz yerel dosya magazasi icindir (`Saved/SaveGames` ... `Saved/MMORace`); otorite DB olursa (S04/Supabase) bu arac yan dosyalarla sinirlanir ya da emekli olur.

Bu belgedeki komutlar proje kokunden calistirilir. Yol/ozet degerleri yer tutucudur (`<...>`).

## 1. Ne zaman yedek alinir

- **Dogru an:** sunucu ve editor KAPALI, tercihen planli/temiz kapanistan hemen sonra (`MMOShutdown`, bkz. §7). Her planli kapanista bir yedek + gunde bir.
- Arac sunucu/editor/UBT sureci gorurse yedek ALMAZ ve cikis 3 verir (surec sorgusu basarisizsa da ayni).
- **`-AllowLive` (sunucu acikken):** bilincli bayrak, "tutarli" yedek DEGILDIR. Dosyalar arasi tutarsizlik olabilir; kararsiz dosya `valid=false notes=unstable` olur; dizin adi `-live` ile biter, cikis 4 (`live`). Yalniz baska care yokken, sonradan temiz yedekle degistirmek uzere kullan. `-AllowLive` ile `-RequireCleanShutdown` birlikte kullanilamaz (cikis 2).
- Arac yalniz BU makinenin sureclerini gorur; ayni `Saved`'e yazan baska makinedeki sunucuyu goremez.

## 2. Yedek al (`BackupSaves.ps1`)

Parametreler (kaynaktan): `-SavedDir`, `-BackupRoot`, `-Label`, `-AllowLive`, `-RequireCleanShutdown`, `-AllowRootAncestorReparse`, `-AllowIgnored`. (`-ProcessProvider`, `-Timestamp`, `-TestHooks` yalniz test icindir, kullanma.)
Varsayilan `-BackupRoot`: `<projenin ust dizini>\MMOBackups` (repo ve `Saved` disinda; `Saved` ya da repo icine yedek koku reddedilir).

```powershell
# Normal (sunucu kapali)
.\Scripts\BackupSaves.ps1

# Onerilen: temiz kapanis kaniti iste (Saved\Logs\MMOShutdownResult.json, run_id gecerli, exit_code 0)
.\Scripts\BackupSaves.ps1 -RequireCleanShutdown -Label bakim

# Sunucu acikken (riskli, tutarli degil, cikis 4 beklenir)
.\Scripts\BackupSaves.ps1 -AllowLive -Label canli
```

**Cikti sonunda `manifest_sha256=<ozet>` satirini ve `[MMO_BACKUP_RESULT]` satirini kaydet.** Ozet ayrica `<BackupRoot>\last_ok.json` ve goruntudeki `backup.log` icindedir; ama asil kayit AYRI bir yere (§4) yazilmalidir.

Goruntu: `<BackupRoot>\<yyyyMMddTHHmmssZ>-<etiket>\` (`files\...` + en son yazilan `manifest.json`). `manifest.json` yoksa goruntu YARIMDIR; silinmez, `VerifySaves` "incomplete" der.

## 3. Cikis kodlari (BackupSaves / VerifySaves / RestoreSaves ortak sozlesme)

| Kod | BackupSaves | VerifySaves | RestoreSaves |
|---|---|---|---|
| **0** | Tam guvenilir yedek alindi | Tum goruntuler ok (yalniz uyari/live olabilir) | Tamam |
| **1** | Yedek YOK/basarisiz (hata, `copy_drift`, `quiesced_violated`, bos kaynak, kendi dogrulamasi basarisiz) | Dogrulama basarisiz (bozuk/yarim goruntu, `valid=false` giris, ozet uyusmazligi, hic goruntu yok) | Basarisiz/reddedildi (dogrulama, kapi, yazim hatasi + rollback; `rollback_failed`, `rollback_conflict`) |
| **2** | Kullanim hatasi | Kullanim hatasi | Kullanim hatasi (ornegin `-Apply` ozetsiz, `-Slots` risk onaysiz) |
| **3** | **Ertelendi = HICBIR yedek ALINMADI** (surec calisiyor/sorgulanamadi, temiz kapanis kosulu yok, kilit dolu, yol yapisi/reparse, bos alan `< 1,5 x kaynak`) | Goruntu/kok/`-Against` Saved yok, erisilemiyor ya da reparse | Ertelendi: HICBIR dosyaya dokunulmadi (surec, canli yazim, `restore_in_progress_marker`, kilit) |
| **4** | Yedek VAR ama tam guvenilir degil; neden `+` ile: `live`, `invalid_files`, `ignored_player_data`, `collision`, `proof_stale` | (yok) | Basari ama uyarili (`forced`, `stale_side_files`, `live_lock_files`, `partial_state_acknowledged`, ...) |

Onemli: **BackupSaves 3 = yedek ALINMADI** (alarm gerekir). BackupSaves 4 = kullanilabilir yedek var ama nedenini (`reason=`) oku. VerifySaves 1 = icinde geri yuklenmeyecek giris olabilir (`[MMO_VERIFY_INVALID]`).

## 4. Manifest ozetini AYRI yere kaydet (zorunlu, K6 siniri)

Yedek **imzasizdir**: `manifest.json` ve `files\` ayni dizinde, anahtarsiz SHA-256 ile durur. Kaza/bozulma ve tek tarafli degisiklik yakalanir; ama hem dosyalari hem manifesti tutarli yeniden yazabilen biri (yazma erisimi olan herkes) arac icin "gecerli" bir yedek uretebilir. Bunun TEK karsiligi:

1. Yedek alirken cikan `manifest_sha256` degerini yedek dizininin DISINDA bir yere yaz (baska makine, mesaj, kagit/not).
2. `VerifySaves -ExpectedManifestSha256` ve `RestoreSaves -ExpectedManifestSha256` ile bu kayitli degeri ver. RestoreSaves kuru calismada hesapladigi ozeti ekrana basar ama hazir `-Apply` komutu basmaz: ekrandaki ozeti komuta kopyalamak karsilastirmayi anlamsiz kilar.
3. Yedekler baska makineye/buluta gidecekse sifrele/imzala (arac disi; arac sifreleme yapmaz).

## 5. Yedek dizini guvenligi (ACL onerisi)

Yedekler oyuncu adi, altin ve posta metni icerir (kisisel veri). Yedek kokunu yalniz operatore ve gerekirse yedek hesabina yazilabilir yap; diger kullanici/servis hesaplari yazamasin. Ornek (yol ve hesap adini kendine gore degistir; **bu repoda calistirilmadi/doğrulanmadi**):

```powershell
icacls "D:\MMOBackups" /inheritance:r /grant:r "${env:USERNAME}:(OI)(CI)F" "SYSTEM:(OI)(CI)F" "Administrators:(OI)(CI)F"
```

Yedek kokunu git deposuna ya da bulut esitleme klasorune koyma (arac repo icini reddeder; bulut klasoru operator sorumlulugu).

## 6. Dogrula (`VerifySaves.ps1`, salt okuma)

Parametreler: `-Snapshot`, `-BackupRoot`, `-Last`, `-All`, `-ExpectedManifestSha256`, `-Against`, `-AllowRootAncestorReparse`. Hicbir sey yazmaz/silmez.

```powershell
# Tek goruntu + disaridan kaydettigin ozet
.\Scripts\VerifySaves.ps1 -Snapshot D:\MMOBackups\<ts>-<etiket> -ExpectedManifestSha256 <kaydettigin ozet>

# Son 3 goruntu (zaman damgasina gore; -ExpectedManifestSha256 yalniz -Snapshot ile)
.\Scripts\VerifySaves.ps1 -BackupRoot D:\MMOBackups -Last 3

# Goruntu ile canli Saved farki (bilgi amacli; sunucu aciksa fark dogaldir)
.\Scripts\VerifySaves.ps1 -Snapshot D:\MMOBackups\<ts>-<etiket> -Against <proje>\Saved
```

Ilk satir `[MMO_VERIFY_SUMMARY] status= ... bad= incomplete=`. Yarim goruntu kokte durdukca `-All`/`-Last` hep 1 doner (bilinen sinir). `.sav` icin yalniz baslik (`GVAS`) ve boyut denetlenir; oyun anlaminda yuklenebilirlik DOGRULANMAZ.

## 7. Zamanlayici ornegi (KURULMADI, yalniz ornek)

Gorev bu depoda kurulmaz; kurmak operatorun karari. Yedek yalniz sunucu KAPALIYKEN alinabilir; sunucu acikken gorev cikis 3 doner ve **o gun yedek alinmamis** demektir.

```powershell
# Kayit-ScheduledTask ornegi (yonetici PowerShell; Windows PowerShell 5.1)
$act = New-ScheduledTaskAction -Execute 'powershell.exe' `
  -Argument '-NoProfile -ExecutionPolicy Bypass -File "C:\...\MyProject\Scripts\BackupSaves.ps1" -RequireCleanShutdown -Label gunluk'
$trg = New-ScheduledTaskTrigger -Daily -At 04:30
Register-ScheduledTask -TaskName 'MMO-BackupSaves' -Action $act -Trigger $trg -Description 'MMO save yedegi (sunucu kapaliyken)'

# schtasks esdegeri
schtasks /Create /TN "MMO-BackupSaves" /SC DAILY /ST 04:30 /TR "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"C:\...\MyProject\Scripts\BackupSaves.ps1\" -RequireCleanShutdown -Label gunluk"
```

Bu komutlar bu projede **calistirilmadi/dogrulanmadi**.

**Izleme (alarm):** Gorev zamanlayicisi "basarili" gosterse bile yedek alinmamis olabilir; bak:
- `<BackupRoot>\last_run.json` her cikista (3 dahil) yazilir: cikis kodu, durum, neden kodu, UTC zaman. Kod 3 ya da 1 ise alarm.
- `<BackupRoot>\last_ok.json` YALNIZ kod 0'da guncellenir. Dosyanin `time_utc` degeri 24-36 saatten eskiyse alarm ("son tam guvenilir yedek eski"). Alan adlari: `snapshot`, `manifest_sha256`, `mode`, `time_utc`.
- Ardisik kod 3 sunucunun surekli acik oldugunu gosterir; yedek penceresi ya da bakim akisi (asagida) gerekir.
- Cikis kodu 4 (canli/gecersiz dosya/yok sayilan oyuncu verisi) icin `reason=` oku; `last_ok.json` guncellenmez.

### Bakim penceresi akisi

1. Sunucuyu planli kapat: konsol `MMOShutdown <saniye> [neden]` ya da baslatirken `-MMOShutdownAfterSeconds=N` (`docs/DESIGN_SERVER_LIFECYCLE.md`). Oyuncular uyarilir, kayit yazilir, surec cikar.
2. `Saved/Logs/MMOShutdownResult.json` dosyasini oku: `exit_code` 0 olmali (2 = en az bir oyuncu kaybedildi: dikkat) ve `run_id` BU kapanista baslattigin calismaya ait olmali (`<surec baslangic UTC>-<PID>`). Dosya yoksa ya da run_id baska bir calismaya aitse temiz kapanis YOKTUR (cokme/TerminateProcess); bayat dosyaya guvenme.
3. `.\Scripts\BackupSaves.ps1 -RequireCleanShutdown -Label bakim`, sonra `VerifySaves -Snapshot ...`. Ozeti ayri yere kaydet.
4. Bakimi yap, sunucuyu baslat.

Arac `run_id`'yi yalniz kalip olarak denetler (`^[0-9TZ-]{1,40}$`) ve manifeste `shutdown_proof` olarak yazar; **hangi calismaya ait oldugunu SEN dogrulamalisin**. Temiz kapanis kaniti tek basina "sunucu simdi kapali" demek degildir; asil kapi surec denetimidir.

> **UYARI:** `-MMOShutdownAfterSeconds` / `-MMOMaintenanceReason` argumanlarini sunucunun KALICI baslatma betigine/servis tanimina YAZMA. Her baslatmada sunucu kendini kapatir ve girisleri reddeder. Yalniz tek seferlik bir baslaticida kullan; kalici servis ayrica otomatik yeniden baslatma KAPALI olmali (kod 2 cikis = kayip oyuncu var).

## 8. Geri yukle (`RestoreSaves.ps1`)

**Oyuncu verisinin uzerine yazan tek arac.** Varsayilan KURU CALISMA. Parametreler: `-Snapshot`, `-SavedDir`, `-BackupRoot`, `-All`, `-Slots`, `-Apply`, `-ExpectedManifestSha256`, `-AcknowledgeCrossSlotRisk`, `-AcknowledgePartialState`, `-AcknowledgeStaleRestoreMarker`, `-QuarantineStaleSideFiles`, `-IncludeInvalid`, `-IncludeSideFiles`, `-AcceptLiveSnapshot`, `-AcceptDifferentSource`, `-AcceptCollisions`, `-RecentWriteSeconds`, `-AllowRootAncestorReparse`, `-Recover`. Kapsam zorunlu: `-All` ya da `-Slots` (ikisi birden/ikisi de yok = 2).

### 8.1 Geri yukleme oncesi kontrol listesi

- [ ] Sunucu GERCEKTEN kapali (gorev yoneticisinde `MyProjectServer`/`UnrealEditor`/UBT yok; son temiz kapanis `MMOShutdownResult.json` ile dogrulandi).
- [ ] **Baska makinede ayni `Saved`'e yazan/ayni oyuncu verisini kullanan sunucu yok** (arac yalniz bu makineyi gorur; her calismada `code=other_machines` uyarisi basar).
- [ ] Goruntu `VerifySaves -Snapshot ... -ExpectedManifestSha256 <kaydettigin ozet>` ile 0 doner.
- [ ] Kuru calisma ciktisinin ozeti (`[MMO_RESTORE_MANIFEST_SHA256]`) yedek alirken AYRI kaydettigin ozetle ayni.
- [ ] **Posta/ticaret cogaltma riski anlasildi:** tek karakteri eski goruntuden geri yuklemek, o karakterden sonra gonderilen posta altininin/takas edilen esyanin alici tarafta kalmasina (cogalma) ya da kaybolmasina yol acabilir. Arac bunu GERI ALMAZ ve tam DOGRULAYAMAZ; `-All` da tutarlilik garantisi vermez. Tercih `-All`.
- [ ] Gerekirse yedek alinmis, pre-restore goruntusu icin bos disk alani var.

### 8.2 Akis: kuru calisma -> ozet kaydi -> `-Apply`

```powershell
# 1) KURU CALISMA: hicbir sey yazmaz; plan + hesaplanan manifest ozeti
.\Scripts\RestoreSaves.ps1 -Snapshot D:\MMOBackups\<ts>-<etiket> -All

# 2) Ozeti, YEDEK ALIRKEN kaydettigin degerle karsilastir (ekrandakini komuta kopyalama)

# 3) Uygula (kaydettigin ozet ile). Arac once <ts>-prerestore goruntusu alir; alinamazsa hicbir dosyaya dokunmaz
.\Scripts\RestoreSaves.ps1 -Snapshot D:\MMOBackups\<ts>-<etiket> -All -ExpectedManifestSha256 <kaydettigin ozet> -Apply
```

Arac uyarilarin gerektirdigi bayragi adiyla soyler ve reddeder: `-AcknowledgePartialState` (canlida goruntude olmayan dosya, ya da posta izi/dogrulanamayan kutu), `-QuarantineStaleSideFiles` (goruntude olmayan/farkli canli `.tmp`/`.bak`; silinmez, pre-restore dizinine TASINIR). `.bad`/`.tmp.bad` kilitleri (irk secim kilidi dahil) tasinmaz, dusurulmez.

### 8.3 `-Slots` ve capraz-slot riski

```powershell
# Tek karakter: risk bayragi ZORUNLU (yoksa kod 2)
.\Scripts\RestoreSaves.ps1 -Snapshot D:\MMOBackups\<ts>-<etiket> -Slots "Ali Baba" -AcknowledgeCrossSlotRisk
# Uygulama icin ayrica: -ExpectedManifestSha256 <kaydettigin ozet> -Apply (ve gerekirse -AcknowledgePartialState)
```

Slot grubu: `.sav` + basarim + gunluk + posta kutusu + irk dosyasi. Sonekli yan dosyalar yalniz `-IncludeSideFiles` ile. `-Slots` basarisinda yarida kalan zincir isareti acik kalabilir (cikis 4 `restore_chain_open`); ciktidaki uyariyi oku.

### 8.4 Yarida kalan kosu: `-Recover`

Uygulama sirasinda surec oldurulduyse/elektrik gittiyse `Saved\` altinda `*.restore.old` / `*.restore.tmp` kalir ve `<BackupRoot>\restore_in_progress.json` isareti durur. Yeni `-Apply` bu durumda 3 ile reddedilir. Once kurtarma (`-Snapshot` = o kosunun PRE-RESTORE goruntusu):

```powershell
# Kuru calisma (varsayilan)
.\Scripts\RestoreSaves.ps1 -Recover -Snapshot D:\MMOBackups\<ts>-prerestore
# Uygula (ozet = pre-restore manifestinin ozeti; backup.log / last_restore.json / isaret)
.\Scripts\RestoreSaves.ps1 -Recover -Snapshot D:\MMOBackups\<ts>-prerestore -ExpectedManifestSha256 <pre-restore ozeti> -Apply
```

Hedefte canli dosya yokken baslayan kosuda (`preimage=none`) recover kullanilamaz.

### 8.5 Rollback (geri alma)

- Yazim hatasinda arac DENENEN dosyalari pre-restore goruntusunden kendisi geri alir (cikis 1, `rolled_back`).
- `rollback_failed`: ciktidaki `[MMO_RESTORE_ROLLBACK_FAILED]` satirinin komutunu AYNEN calistir (once sunucunun kapali oldugunu dogrula, `-RecentWriteSeconds` kadar bekle). Komut guvenlik kapilarini gevsetir (`-IncludeInvalid -IncludeSideFiles -AcceptCollisions -AcknowledgePartialState -QuarantineStaleSideFiles -AcknowledgeStaleRestoreMarker`); arada sunucu calistiysa KULLANMA.
- `rollback_conflict`: biri dosyayi arada degistirmis; arac UZERINE YAZMAZ, pre-restore baytlarini `<ad>.rollback-conflict` olarak yana koyar. Elle incele; komut basilmaz.
- Basarili bir uygulamayi geri almak: `<ts>-prerestore` goruntusunu `-All` ile ve ayni bayraklarla geri yukle (tasarim belgesi §10 "Operator kullanim ornegi" adim 5; ozet = pre-restore manifest ozeti).

## 9. Bilinen sinirlar ve ilk deneme

- **PowerShell 7 dogrulanmadi.** Yalniz Windows PowerShell 5.1.
- **Arac gercek `Saved/` uzerinde hic denenmedi.** Fikstur testleri (`TestBackup.ps1` 1460 vaka, `TestBackupMutations.ps1` 32 mutasyon) gercek veriyi okumaz/yazmaz.
- Gercek `.sav` anlam dogrulamasi yok (yalniz GVAS basligi); `File.Move` yariya-kesilme (guc kaybi) gercek donanimda sinanmadi; canli `-AllowLive` kopyasinin tutarliligi garanti degil.
- Posta/ticaret izi taramasi sezgiseldir; baska makinedeki sunucu gorunmez.
- Atasi junction olan yedek kokunde yazim bu makinede sinanamadi (`-AllowRootAncestorReparse` yalniz yapi kapisi olarak sinandi).
- Test artigi (`Gezgin######`, `CIDIK-*`) gercek oyuncudan ayirt edilemez; hepsi yedege girer.

**Ilk deneme sirasi (kullanici isi):**
1. `Saved`in SAHTE bir kopyasini (ayri dizin) hazirla; `-SavedDir`/`-BackupRoot` ile ona yonelt. Gercek `Saved`'e `-Apply` YAPMA.
2. Gercek veride ilk adim yalniz `BackupSaves` + `VerifySaves` (ikisi de kaynagi degistirmez). Restore'a ancak sahte kopyada prova sonrasi gec.
3. Cikis kodlarini ve `last_run.json`/`last_ok.json` dosyalarini bu belgedeki tabloyla karsilastir.

## 10. Asla

- Yedek dizinindeki `files\` ya da `manifest.json` dosyalarini ELLE duzenleme/yeniden adlandirma (ozet tutmaz; imzasiz oldugu icin "duzeltilmis" yedek kanit olmaz).
- Restore (`-Apply` / `-Recover`) calisirken sunucuyu ya da editoru BASLATMA; arac cikmadan `Saved\` ile ugrasma.
- `restore_in_progress.json` ya da `*.restore.*` / `*.rollback.*` dosyalarini elle silme (once `-Recover`).
- Ekrandaki hesaplanan ozeti `-ExpectedManifestSha256`'ya kopyalama (yedek alirken AYRI kaydettigin ozeti kullan).
- Rollback komutunu rutin komut gibi calistirma; sunucu arada calistiysa hic calistirma.
- Ayni `Saved`'e yazan baska bir makinedeki sunucu aciksa geri yukleme yapma.

## 9. Karakter yan dosyalari ve bozuk dosya (.bad) kotasi (S6 atomik karakter kaydi)

Karakter kaydi artik atomik yazilir (`docs/DESIGN_CHARACTER_SAVE_ATOMIC.md`). `Saved/SaveGames/` icinde `MMOSave_<ad>.sav` yaninda su dosyalar gorulebilir:

| Dosya | Anlami | Yedek araci | Geri yukleme |
|---|---|---|---|
| `MMOSave_<ad>.sav.tmp` | yazimda gecici dosya (yarim olabilir) | yedege GIRER (`side`, yarim ise `tmp_unparsable` notu) | varsayilan HAYIR (`-IncludeSideFiles`) |
| `MMOSave_<ad>.sav.bak` | yazim sirasinda bir onceki gecerli kayit (normalde saniyeler icinde silinir) | yedege girer (`side`; bozuksa valid=false) | varsayilan HAYIR (`-IncludeSideFiles`) |
| `MMOSave_<ad>.sav.bad`, `.bad.1` .. `.bad.4`, `.sav.tmp.bad` | bozuk bulunup SILINMEDEN ayrilan dosya (kanit) | yedege girer (`side-lock`, icerik denetlenmez) | varsayilan HAYIR; kilit sayilir, tasinmaz |

**Kota:** her karakter icin en fazla 5 bozuk dosya (`.bad`, `.bad.1`..`.bad.4`) saklanir. Kota dolunca o karakterin Save'i REDDEDILIR (`event=save_failed reason=bad_cap_reached`, `[MMOMETRIC] ev=save_bad_cap player=<ad>`): ilerleme kaydedilmez, sunucu hicbir dosyayi KENDILIGINDEN silmez. Isleyecek prosedur:

1. `Saved/Logs`'ta `ev=save_bad_cap player=<ad>` gorulur. Once NEDEN bu kadar bozuk dosya oldugunu ara (`ev=load_failed reason=corrupt|io`, `event=recovered`, disk/antivirus/guc kesintisi).
2. Oyuncu cevrimdisi iken (ya da sunucu kapaliyken) `BackupSaves.ps1` ile yedek al (bozuk dosyalar da yedege girer).
3. `Saved/SaveGames/MMOSave_<ad>.sav.bad*` dosyalarini `LastWriteTime`'a gore sirala. **Silinecek/tasinacak olan yalniz `.bad`, `.bad.1`..`.bad.4` ve `.sav.tmp.bad` dosyalarindan EN ESKI olan(lar)dir.** Ana `.sav` ve `.sav.bak` dosyalarina ASLA dokunma (`.bak` gecerli tek kopya olabilir).
4. Silmek yerine `Saved/SaveGames/_bad_archive/<ad>/` gibi bir alt klasore TASI (alt klasor yedek aracinca yok sayilir (`Reason=subdir`); yani arsivi AYRICA yedekle).
5. Oyuncunun bir sonraki otomatik yazimi (60 sn) gecmeli; `ev=save_bad_cap` tekrar etmemeli. Tekrar ediyorsa kok neden hala vardir (kota yalniz belirtidir).

**Bilinen sinir (R2):** yazim sirasinda ana dosya bir an `.bak`'a yeniden adlandirilir; yedek araci tam o anda calisirsa o karakteri o turda gormeyebilir. Sunucu calisirken yedek zaten tutarli degildir (bkz. bolum 1).
