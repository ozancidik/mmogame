# Gercek surec dogrulamasi: kapanis duman testi (sunucu yasam dongusu L4) - tasarim

**Tarih:** 2026-11-01 · **Taban:** `c874f03` (master) · **Durum: TASARIM; S0 UYGULANDI (bkz. §11), S1-S6 baslamadi.** Yazan: `oyun-tasarimcisi`.
Sayilar `[PLACEHOLDER]`; hicbiri bu makinede olculmedi. Ust belge: `docs/DESIGN_SERVER_LIFECYCLE.md` (L0-L3 uygulandi; §7 L4 ve §12 "Bilinen sinirlar").

## 0. Alan secimi ve elenenler (kisa)

Secilen alan: **sunucu yasam dongusunun gercek-surec dogrulamasi (L4)**. Gerekce: L0-L3'un butun degeri (atma, kayit turu, cikis kodu 0/2,
`MMOShutdownResult.json`) yalnizca process-ici sahte saat ve sahte denetleyici ile sinandi. Su uc iddia **hic gercek surecte denenmedi** ve kapali alfada
operatorun guvendigi seyler bunlar: (1) `RequestExitWithStatus` + `OnPreExit` `TerminateProcess(2)` Windows'ta isletim sistemi cikis kodunu gercekten tasiyor mu,
(2) gercek `KickPlayer` / `UNetConnection` atmasi, (3) `PreLogin` reddi gercek ikinci istemciye ulasiyor mu. Ayrica bu tasarimi yazarken **kodda yeni bir bulgu** cikti (§1.2: mevcut
dogrulanmis sunucu yolu cikis kodunu hic calistiramaz).

| Aday | Karar | Gerekce |
|---|---|---|
| (a) merkezi mutasyon kapisi (`MMOMob.cpp`, `MMOLootPickup.cpp`) | **Degmez** | Atma aninda her oyuncu yeniden yazilir (`shutdown_save_retry`, Saved dahil) ve `EndPlay` bir kez daha `SaveCharacterForTravel(true)` yazar (DESIGN_SERVER_LIFECYCLE §5.3, §12). Kapisiz yol ancak "yeniden yazim ile atma arasindaki tek kare"de ve yalniz KAYIP (cogalma degil) uretir; takas/posta/birlestirme/alisveris (cogalma yollari) zaten kapili. Kapi ayrica Flushing'in 20 sn'sinde oyuncunun mob XP/altin/ganimetini boşuna keser (oyuncuya zarar, kazanc yok). Sayi/etiket cilasi. Mob loot icin ayri `MMOLootPickup.cpp` okuma-dogrulamasi gerekirse L4 sonrasi bir cumlelik not yeter |
| (b) operator saglik/telemetri ozeti | **Degmez (simdi)** | `PlaytestMetrics.py` zaten `[MMOMETRIC]` toplar (Codex'te commit'siz, dokunulamaz); yeni dis arac ayni veriyi ikinci kez ozetler. Kayit basarisi/oyuncu sayisi `ev=shutdown*` ve `MMOShutdownResult.json` icinde; bunlari tuketen tek tuketici L4 testidir (asagida) |
| (d) 120 gorevlik fabrika serisinden entegrasyon (MMO-082 audit ledger, MMO-083 hiz filtresi) | **Simdi alinmaz** | Bkz. §0.1 |
| (e) baska baslanmamis sistem | **Gerekmedi** | Pazar (S12) S10+S04'e bagli ve panoda atanmis; olum cezasi/altin sink `MMOStatsComponent`/kararlara bagli ve zaten kararlarda; irk/bolge/S04 atanmis; birlestirme D2/D4 sicak dosyalarda bekliyor. Atanmamis, sicak dosyaya dokunmayan ve olculebilir tek is bu |
| (c) L4 gercek surec testi | **SECILDI** | Yeni dosyalar, sicak dosya baglantisi sifir, "bitti" tanimi makine-okunur |

### 0.1 Fabrika serisi: neden MMO-082/083 simdi oyuna alinmaz (kanit: `C:\Users\OZAN CIDIK\MMO-AI-Factory\agent-test\factory\`)

- Bunlar oyun deposunun disinda, bagimsiz C++17 bilesenleri (`transaction_velocity_abuse_filter.*`, `administrative_privilege_audit_ledger.*`); `work\MMO-0xx\development-manifest.json`
  `executionMode: NOT_STARTED, qaApproved: false` der, `runtime-state.json` ise ikisi icin `verdict: PASS` ve `agent: operator-reviewer, manual: true` kaydeder. Fabrikanin kendi
  `INTEGRATION-NEXT-STEPS.md` dosyasi "onayli bir Factory bileseni calisan bir UE oyununun kaniti degildir" ve "iki bagimsiz gozden gecirici kimligi" ister: MMO-082/083 tek elle (operator) incelemeyle gecti. **Bagimsiz QA yapilmadan oyuna alinmaz.**
- MMO-083 (hiz filtresi) hesap kimligi ister; bu oyunda kimlik hala oyuncu adi (K-1, Steam/S06 bekliyor). Esik (`rolling tick`, `value ceiling`) icin gercek veri yok; kapali alfada 5-20 tanidik oyuncu icin
  enforce eden filtre yanlis pozitifle oyuncuyu keser. Gozlem (shadow) kismi zaten `[MMOMETRIC] ev=gold src=...` satirlariyla var.
- MMO-082 (yonetici denetim defteri): dedicated'da `MMOAdmin::IsServerAdmin` hep `false`; denetlenecek yonetici eylemi yok.
- Alinabilir olsa bile en erken sira: S06 (kimlik/bilet) sonrasi, once MMO-083'u **yalniz gozlem modunda** (log) baglayan, adaptoru ve `Source/` kopyasini ayri kimlikli bir QA turundan gecirecek ayri tasarim.
  Bu belge o karari ONERMEZ ve onunla cakisan hicbir dosyaya dokunmaz.

## 1. Oyuncu deneyimi ve neden simdi

Oyuncu hicbir sey gormez; operator gorur: bakim komutunda sunucu geri sayar, oyuncular nazikce atilir, son ilerleme diskte durur, surec **bilinen bir cikis koduyla** kapanir
ve otomatik yeniden baslatma yanlislikla dongu yaratmaz. Bunlarin hicbiri "bir sey bozulmadi" diye dogrulanamaz; ya gercek surecte calistirilir ya da inanilir.
**Neden simdi:** (1) kapali alfa yaklasiyor, ilk gercek bakim penceresinde kayip ilerleme en pahali hata; (2) S04/Supabase slice 4 `FinishExit`'e `ReleaseLeaseEx` ekleyecek (DESIGN_SERVER_LIFECYCLE §12): temel cikis yolunun gercekte calistigi bilinmeden ustune yazilmaz;
(3) yedekleme araci (B0-B4) "temiz kapanis kaniti"ni (`MMOShutdownResult.json`) tuketiyor: dosyanin gercekten uretildigi ve dogru icerdigi hic gorulmedi.

### 1.1 Mevcut durum (kod gercegi, grep 2026-11-01)

- Cikis: `Private/Core/MMOServerLifecycle.cpp` (`StepExit`, `FinishExit`, `LifecycleForceExitAtPreExit` ~satir 112, JSON yazimi ~958). Olaylar: `event=lifecycle_ready`, `event=shutdown_phase`, `event=shutdown_flush_begin`, `ev=shutdown phase=flush_done`, `event=shutdown_kick`, `event=shutdown_exiting`, `[MMOMETRIC] ev=shutdown_exit code=N ...`, `event=prelogin_rejected`, `event=stale_result_cleared ok= run_id=`.
- Test: `Scripts/TestServerLifecycle.ps1` 280 vaka ve `TestShutdownPlan.ps1` 82 vaka: ikisi de `UnrealEditor-Cmd.exe -ExecCmds` ile tek surec, `SetExitHookForTest` kodu yakalar; gercek `RequestExit` cagrilmaz.
- Hazir iskelet: `Scripts/NetSmokeTest.ps1` (sunucu + istemci baslatma, port seçimi, sahip-surec temizligi, `-UserDir` izolasyonu) ve `Scripts/TestMailRefund.ps1` (UE/UBT calisirken ertele = cikis 3). **Degistirilmez**, kopyalanir.

### 1.2 Yeni bulgu: kanitli sunucu yolu cikis kodunu calistiramaz [Kesin, kod]

`MMOServerLifecycle.cpp:965-970`: `#if !UE_BUILD_SHIPPING` altinda `if (GIsEditor)` ise `event=shutdown_exit_skipped_editor` loglanir ve **cikilmaz**.
S01.04/S01.05'te kanitlanan "editor-dedicated" yolu `NetSmokeTest.ps1:198` `@($mapPath, '-server')` ile baslar ve `:166` **`-game` iceren komut satirini reddeder**
(`EDITOR_SERVER command-line evidence ...`). Editor ikilisi `-game` olmadan `GIsEditor=true` calisir; yani bugun gecen iki bağlanti kaniti L4'te **kullanilamaz**: cikis atlanir, cikis kodu/JSON asla uretilmez.
Cozum adayi `-game -server` (editor ikilisini oyun modunda dedicated olarak calistirmanin olagan yolu) - **[Dogrulanmadi]**: bu makinede hic denenmedi, `NM_DedicatedServer` olup olmadigi ve `GIsEditor`'un false olup olmadigi dilim S1'de OLCULECEK (§6). Tahmin dogru cikmazsa yedek plan §3.2.

### 1.3 Ikinci bulgu (L4 disi, ayri karar): karakter `.sav` yazimi atomik degil [Kesin, kod + motor kaynagi]

`MMOLocalFileCharacterStore.cpp:27` `UGameplayStatics::SaveGameToSlot(...)`; motorun `FGenericSaveGameSystem::SaveGame` (`Engine/Public/SaveGameSystem.h:151-154`) dogrudan
`FFileHelper::SaveArrayToFile(Data, <son yol>)` yapar: gecici dosya + Move yok. Yazim ortasinda surecin oldurulmesi (TerminateProcess, guc kaybi) dosyayi **kesik** birakabilir;
yan dosyalar (yaris/basarim/gunluk/posta) ise `.tmp` -> Move desenini kullanir, karakter dosyasi kullanmaz. Pencere kucuk (tens of KB) ama sifir degil ve bu sunucunun cikis kodu 2 yolu `TerminateProcess` kullanir.
Bu bulgu L4'un kabul olcutu DEGILDIR (rastgele oldurmeyle yakalanmasi olasilik disi); ayri dilim **S6**'da (§3, karar K-S6) ele alinir.

## 2. Kapsam / kapsam disi

**Kapsam (hepsi test/dogrulama):** saf kanit degerlendiricisi; gercek dedicated sunucu + 1-2 istemci ile temiz kapanis; cikis kodu 2 yolunun hata enjeksiyonuyla gercek isletim sistemi cikis kodu olarak gorulmesi; `PreLogin` reddi ve atmanin istemci tarafi kaniti; kendi regresyon (sahte surec) testi.

**Kapsam disi:** HUD sayac bandi (degmez, bkz. DESIGN_SERVER_LIFECYCLE §9); Linux/`GOverriddenReturnCode` yolu (kaynak motor/Server hedefi, S01'e bagli); Supabase async yazimlarin beklenmesi (slice 4);
cok oyunculu yuk (R3 olcumu ayri); PIE; `MMOHUD`/`MMOPlayerController` degisikligi (istemci sohbet satirinin gorulmesi §4.3'te dürüstce "gozlenemez" diye isaretli); fabrika bilesenleri (§0.1); mutasyon kapisi (§0, (a)).

**Mevcutla iliski:** `UMMOServerLifecycleSubsystem` ve `FMMOShutdownPlan` DEGISMEZ (yalniz S1 sonucu §3.2 yedeginin gerekmesi durumunda `MMOServerLifecycle.cpp` ~965'te tek bir kosul). `NetSmokeTest.ps1` degismez.

## 3. Dosya haritasi

### 3.1 Yeni dosyalar (hepsi su an yok; `git ls-files`/worktree taramasi 2026-11-01: hicbir worktree'de `TestShutdownSmoke*` yok)

| Dosya | Icerik | Dilim |
|---|---|---|
| `Scripts/ShutdownSmokeEvidence.ps1` | **Saf** fonksiyonlar (dunya/surec yok): `Get-ShutdownEvidenceVerdict -ServerLogText -ClientLogText -ResultJsonText -ProcessExitCode -Scenario -SaveFileInfo` -> `PASS | FAIL | INCONCLUSIVE` + neden listesi. Dot-source edilir | S0 |
| `Scripts/TestShutdownSmokeEvidence.ps1` | S0 fikstur testi: olumlu + >= 30 olumsuz vaka (asagida), `PASS checks=N` | S0 |
| `Scripts/TestShutdownSmoke.ps1` | Gercek kosucu: dedicated + istemci(ler) baslatir, kanit toplar, degerlendiriciyi cagirir. Parametreler: `-Scenario clean|savefail|loginclosed`, `-Map`, `-ShutdownAfter`, `-HardCapSeconds` | S2-S4 |
| `Scripts/TestShutdownSmokeRunner.ps1` | Kosucu regresyonu: sahte surec/sahte log (`TestNetSmokeRunner.ps1` desenini KOPYALA, onun dosyasi Codex'te kirli, dokunma). UE baslatmaz | S2 |

### 3.2 Dokunulacak mevcut dosyalar

**Zorunlu: 0.** Kosullu: `Source/MyProject/Private/Core/MMOServerLifecycle.cpp` (temiz, kirli listede yok) yalniz **S1 olcumu `-game -server` ile de `GIsEditor==true` veya cikis atlandi derse**:
`if (GIsEditor)` kosuluna non-Shipping `-MMOShutdownAllowEditorExit` komut satiri anahtari eklenir (Shipping'de derlenmez, varsayilan kapali, yalniz testin baslattigi surecte). Bu bir **uretim davranisi degildir**; ayri commit, `qa-kontrolcu` + `ag-guvenlik-denetcisi` zorunlu.
Anahtar ek bir kimlik/yetki yuzeyi acmaz (komut satiri, uzak erisim yok). S1 olumluysa dosya DEGISMEZ.

| Baglanti (sonra) | Dosya | Durum |
|---|---|---|
| `RunSystemTests.ps1` / `RunAllChecks.ps1` adimi | kirli/sicak | **Bekle**. Saf S0 testi (`TestShutdownSmokeEvidence.ps1`) UE gerektirmez ve sahip temizlenince `RunAllChecks.ps1` yalniz-ekleme adimi olabilir; gercek kosucu (UE acar, dakikalar surer) **hicbir toplu adima girmez** (`TestMailRefund.ps1` gibi elle) |
| `docs/PLAYTEST_CHECKLIST.md`, `CLAUDE.md` | sicak | Yalniz belge; S2 sonucu gelince tek satir, sahibi temizleyince |

### 3.3 Codex cakisma riski (`git status` 2026-11-01)

Kirli: `MMOCharacter.*`, `MMOSkillComponent.cpp`, `MMOPlayerController.cpp`, `MMOHUD.*`, `RunSystemTests.ps1`, `TestNetSmokeRunner.ps1`, `PlaytestMetrics.py`, `PLAYTEST_CHECKLIST.md`; izlenmeyen `MMORace*`, `MMOZoneLayout*`, `TestRace.ps1`, `docs/DEVELOPMENT_PLAN.md` vb.
Bu tasarimin yeni dosyalari bunlardan **hicbiriyle ortusmez**. Derleme: ana dizin `MMOZoneLayout*`/`MMORace*` yuzunden derlenmeyebilir -> derleyen ajan izole worktree (`shutdown-l0`/`fusion-d1` deseni) kullanir; kosucu `-Project` ile **o worktree'nin** `.uproject`ini ve ikili dosyalarini alir.
Acik worktree `s04-p0-hardening` ve `MMOLocalFileCharacterStore.cpp`: `git diff master 20bd02c` bu dosyayi degistirmiyor (2026-11-01), ama S6 baslamadan once yeniden bakilir.

## 4. Davranis: senaryolar ve kanit

Ortak onkosullar (kosucu bunlardan biri saglanmazsa **ERTELER, cikis 3**; PASS uretmez):
UE (`UnrealEditor*`) ve UBT calismiyor; hedef harita acik degil; secilen port bos; `-UserDir` bos/yeni bir dizin (`$env:TEMP\mmo_l4_<guid>`), ana `Saved/` yazilmaz ve okunmaz; test karakter adi `SmokeL4A`/`SmokeL4B` (ASCII, rezerve degil; `MMONameRules`).

### 4.1 Senaryo `clean` (S2)

1. Sunucu: `UnrealEditor-Cmd.exe "<uproject>" <harita> -game -server -NullRHI -nosound -unattended -nosplash -stdout -ABSLOG=<log> -port=<P> -UserDir=<tmp> -MMOShutdownAfterSeconds=210 -MMOMaintenanceReason=bakim` `[PLACEHOLDER 210]`.
   **Neden 210 ve 40 degil:** `LoginCloseSeconds=60` ile kalan <= 60 sn ise girisler hemen kapanir (`FMMOShutdownPlan`); sayim <= 60 ile baslatilirsa istemci hic katilamaz. 210 = istemci acilisi (~45-90 sn) + katilim + Closing penceresi.
2. Hazir olma: sunucu logunda `event=lifecycle_ready` ve port dinleniyor; tam duvar suresi `HardCapSeconds` `[PLACEHOLDER 420]` ile sinirli.
3. Istemci A: `127.0.0.1:<P>?Name=SmokeL4A -game -NullRHI ...` (ayri `-ABSLOG`). `Welcomed`/katilim sunucu logunda gorulmeden Closing'e gecilirse sonuc **INCONCLUSIVE** (PASS degil).
4. Beklenen akis (sunucu logu, sirayla, her biri tam bir kez): `shutdown_scheduled` -> `shutdown_phase phase=closing` -> `flush_begin` -> `[MMOMETRIC] ... phase=flush_done ... saved>=1 failed=0` -> `shutdown_kick` (SmokeL4A, sonuc ok) -> `shutdown_exiting` -> `[MMOMETRIC] ev=shutdown_exit code=0 failed=0`.
5. Surec kendiliginden sonlanir (`WaitForExit(30000)`); isletim sistemi **`Process.ExitCode == 0`** ve `Saved/Logs/MMOShutdownResult.json` (`-UserDir` altinda; konum S1'de olculur) `exit_code:0`, `failed:0`, `run_id` = sunucu logundaki `stale_result_cleared ... run_id=` degeri.
6. Kayit kaniti: `MMOSave_SmokeL4A.sav` mevcut, boyut > 0, son yazim zamani sunucu baslangicindan sonra ve `flush_done` zamanindan once-veya-esit pencerede. (Icerik dogrulamasi mevcut native kayit testlerinin isi; burada **yalniz varlik/zaman/boyut**.)
7. Atma kaniti istemci tarafinda: istemci logunda baglanti kesilmesi/atilma izi (motor satiri; tam metin S1'de okunup degerlendiriciye yazilir - uydurma desen yok). Istemci sohbetinde bakim satirinin **gorulmesi gozlenemez**: istemci tarafinda `ClientReceiveChat` icin log satiri yok; `MMOPlayerController.cpp` kirli, bu yuzden eklenmez. Sunucunun duyuruyu GONDERDIGI sunucu logundan (`Announce` yolu) cikarilir, alinmasi PIE listesine kalir (§7.3).

### 4.2 Senaryo `savefail` (S3): cikis kodu 2'yi gercek surecte gormek

Calistirma 1 `clean` ile `MMOSave_SmokeL4A.sav` uretir. Calistirma 2: dosyaya **salt-okunur** ozniteligi konur (kosucu `finally`'de MUTLAKA kaldirir) ve ayni senaryo koşulur.
Beklenen: `shutdown_save_failed` / `shutdown_save_retry ... result=failed` -> `ev=shutdown_exit code=2 failed>=1` -> `Process.ExitCode == 2` (**asil iddia: `OnPreExit` `TerminateProcess` kodu OS'e tasiyor**), JSON `exit_code:2`, `failed>=1`. `Move` ile yer degistiren bir yazim salt-okunur hedefi gecebilir; yazim yine basarili cikarsa enjeksiyon gecersiz sayilir -> **INCONCLUSIVE** (kosucu `shutdown_save` sonucundan anlar), alternatif enjeksiyon (ayni adli dizin) S3'te secilir. Sahte "2 gormek" icin kodu degistirmek YASAK.

### 4.3 Senaryo `loginclosed` (S4)

`clean` + sunucu logunda `phase=closing` gorulunce istemci B (`SmokeL4B`) baslatilir. Beklenen: sunucu `event=prelogin_rejected` >= 1, B asla `Welcomed` olmaz, istemci B logunda reddi/baglanti hatasi izi.
A ve B ayni anda `PreLogin` reddinden etkilenmez: A zaten icerde.

## 5. Sunucu-otorite, replikasyon, kimlik, kalicilik, guvenlik

- **Yeni RPC yok, yeni replike alan yok, `MMOSaveGame.h`'ye dokunulmaz, yeni oyun kodu yok** (S1 yedegi hariç). Istemciler normal oyun istemcisidir; hicbir test-only istemci yetkisi yok.
- **Kimlik:** test adlari ASCII (`SmokeL4A`); oyuncu adi hala kimlik (K-1) - test bunu cozmez, kapsami disinda.
- **Kosucu guvenligi (zorunlu, `ag-guvenlik-denetcisi` okur):**
  1. Yalniz kendi baslattigi `Process` nesnelerine dokunur; ad ya da PID ile `Stop-Process`/`taskkill` YOK; olusturdugu dosyayi/dizini silmez (kalir; yalniz `-UserDir` icinde).
  2. Ana `Saved/` hicbir yolla okunmaz/yazilmaz (`-UserDir` izole + yol kokleri `Saved` altinda ise ret; reparse/junction ise ret, `BackupSaves.ps1` dersi). Hedef kayit dosyasi adi sabit onekli (`MMOSave_SmokeL4*`).
  3. Salt-okunur enjeksiyonu `try/finally` ile geri alinir; geri alinamazsa kosucu cikis 1 ve acik uyari.
  4. `-MMOShutdownAfterSeconds` yalniz bu tek seferlik baslaticida bulunur (DESIGN_SERVER_LIFECYCLE §5: kalici servis tanimina YAZILMAZ). Belge ve betik basliginda tekrarlanir.
  5. Cikis kodlari: 0 PASS, 1 FAIL, 3 ERTELENDI (calisan UE/UBT, port, kilit; hicbir sey baslatilmadi), 4 INCONCLUSIVE (onkosul saglanamadi: istemci katilamadi, enjeksiyon gecersiz). 4 hicbir yerde "gecti" sayilmaz.
  6. Zaman: her bekleme duvar saatiyle sinirli; `HardCapSeconds` asiminda sahip surecleri oldur, sonuc FAIL.
- **Kalicilik:** test karakterinin `.sav` dosyalari yalniz izole `-UserDir` icinde olusur; ana oyuncu verisine etkisi sifir.

## 6. Ekonomi / denge etkisi

Altin/XP/esya girisi-cikisi **yok**. Sayilar `[PLACEHOLDER]`: `ShutdownAfter 210 sn`, `HardCapSeconds 420`, cikis bekleme `30 sn`, hazir olma butcesi `120 sn`. Olcum ihtiyaci: S1'de (a) `-game -server` ile gercek acilis suresi, (b) istemci acilisi (S01.04: sunucu hazirligi ~6 sn shader ayrintisi, tum bütce ~95 sn), (c) `flush_done`'a kadar sure. Bu degerler olculunce guncellenir.

## 7. Kabul olcutleri, dilimler ve test plani

Her dilim ayri commit ve ayri "bitti". Siralama: **saf mantik once (S0)**, sonra tek-seferlik olcum (S1), sonra gercek kosucu.

| Dilim | Is | "Bitti" = |
|---|---|---|
| **S0** | `ShutdownSmokeEvidence.ps1` + `TestShutdownSmokeEvidence.ps1` (UE yok) | `PASS checks=N` (N = benzersiz vaka etiketi sayisi, 3 yerde tutarli: test, `$expected`, commit mesaji); asagidaki mutasyonlarin hepsi FAIL verir; `StaticLint/ValidateData` etkilenmez (yalniz `.ps1`) |
| **S1** | Tek-seferlik olcum (kod yok): `-game -server` ile elle bir sunucu (`-MMOShutdownAfterSeconds=90`, istemci yok), `test-muhendisi` calistirir | §10 "Olcum kaydi" dolu: (1) komut satiri, (2) `NM_DedicatedServer`/`GIsEditor` kaniti (`shutdown_exit_skipped_editor` var mi), (3) surec kendiliginden bitti mi, `Process.ExitCode`, (4) `MMOShutdownResult.json` ve `.sav` gercek yolu `-UserDir` ile, (5) istemci atilma satirinin metni (S2'ye girdi). Olumsuzsa §3.2 yedegi ayri dilim S1b olur |
| **S2** | `TestShutdownSmoke.ps1 -Scenario clean` + `TestShutdownSmokeRunner.ps1` (sahte surec) | Runner regresyonu PASS; gercek kosu: §4.1 madde 1-7, cikis 0, `Process.ExitCode==0`; `Error:` 0 (testin kendi uretmedikleri) |
| **S3** | `-Scenario savefail` | Gercek `Process.ExitCode==2`, JSON `exit_code:2`, `finally` salt-okunuru kaldirdi (kosu sonrasi dosya yazilabilir); geçersiz enjeksiyon INCONCLUSIVE (4) |
| **S4** | `-Scenario loginclosed` | §4.3; B hic `Welcomed` olmaz, `prelogin_rejected >= 1` |
| **S5** | Baglanti (kirli dosyalar temizlenince): saf S0 testi `RunAllChecks.ps1` adimi; belge satirlari | Sahip commit'ledikten sonra; gercek kosucu toplu adima GIRMEZ |
| **S6** | (Ayri karar K-S6) `MMOLocalFileCharacterStore.cpp::Save` atomik yazim | §7.3 |

### 7.1 S0 vakalari (saf; hepsi `Get-ShutdownEvidenceVerdict` ile)

Olumlu: tam `clean` kaniti PASS; tam `savefail` kaniti PASS (kod 2 beklenir).
Olumsuz (her biri FAIL ya da INCONCLUSIVE, asla PASS): JSON yok; JSON `run_id` bos/uyumsuz/log `run_id`'sinden farkli (bayat dosya); `Process.ExitCode` JSON `exit_code`'dan farkli; `clean`'de `code=2`; `savefail`'de `code=0`;
`shutdown_exit` olayi iki kez; `shutdown_exit` yok; `shutdown_exit_skipped_editor` var (-> INCONCLUSIVE, "yol cikis kodunu calistiramaz"); `flush_done` yok; `flush_done saved=0`; `failed>0` ama kod 0; `kick` oncesi `exiting`; sira bozuk (`closing` sonra `scheduled`); JSON'da bilinmeyen alan/eksik alan/ters tip; JSON bos/kesik/BOM'lu; `.sav` yok/0 bayt/baslangictan once; istemci logu bos; istemci A hic `Welcomed` olmadi (-> INCONCLUSIVE); `loginclosed`'de `prelogin_rejected` 0; B `Welcomed` oldu; log icinde `Error:`/`Fatal`/`Ensure` (izinli: testin kasitli urettigi `savefail` uyarilari yalniz o senaryoda); kod negatif/cok buyuk/metin; log satirlari CRLF/LF karisik; Turkce karakter/ters egik cizgili yollar; cok buyuk log (satir sayisi siniri) ; sahte `ev=shutdown_exit` satiri istemci logunda (yalniz sunucu logu sayilir).

### 7.2 Mutasyonla sinanacak korumalar (`test-muhendisi`; geri yukledikten sonra dosyaya touch)

M1 `run_id` karsilastirmasi silinir. M2 JSON-vs-`Process.ExitCode` karsilastirmasi silinir. M3 `skipped_editor` INCONCLUSIVE yerine PASS sayilir. M4 "olay tam bir kez" sayimi >= 1 yapilir. M5 `.sav` zaman penceresi kontrolu silinir.
M6 INCONCLUSIVE -> PASS esleme. M7 `Error:` taramasi silinir. M8 sira kontrolu silinir. M9 (kosucu regresyonu) `finally`'de salt-okunur geri alma silinir -> sahte surec testi yakalar. M10 (kosucu) ad ile `Stop-Process` eklenir -> `TestShutdownSmokeRunner` `foreign-cleanup` izi yakalar (NetSmoke deseni). M11 ana `Saved` kok kontrolu silinir. Hepsi FAIL vermelidir; yakalanmayan mutasyon testin eksigidir.

### 7.3 S6: yalniz karar sonrasi (K-S6)

Konu: §1.3. Oneri (varsayilan): `Save` bellege serilestirir (`UGameplayStatics::SaveGameToMemory`), `<slot>.sav.tmp` yazar, eski dosyayi `<slot>.sav.bak` yapar, `Move`; `Load` ana dosya gecersizse `.bak`/`.tmp` kurtarir; kurtarma loglanir.
Kabul: native self-test (kesik ana dosya + gecerli `.bak` -> yuklenir; ikisi de bozuk -> `Load` false ve **dosya ezilmez**); mutasyon; `VerifySaves.ps1` yeni yan dosya adlarini "yetim" saymaz (DESIGN_BACKUP ile uyum, ayri kontrol); `qa-kontrolcu` + `ag-guvenlik-denetcisi`. Etkilenen dosya `MMOLocalFileCharacterStore.cpp` (+`.h`); S04 dali ile cakisma yeniden kontrol edilir. Bu dilim L4 kabulunun parcasi DEGIL.

### 7.4 Oyun ici (PIE) ilk test listesi (kullanici; S2 sonrasi, DESIGN_SERVER_LIFECYCLE §7.3 ile birlesir)

1. Iki pencere: bakim komutu -> sohbette 60/30/10/5..1 satirlari gorunuyor mu (kosucu bunu GORMEZ, §4.1/7).
2. Atilan istemcinin ekranda gordugu metin ("Ilerlemen kaydedildi" / "kaydedilemedi").
3. Kapanistan sonra yeniden acilista karakter son durumda.

## 8. Riskler ve acik sorular (varsayilanli)

| # | Risk / soru | Varsayilan karar |
|---|---|---|
| R1 | `-game -server` ile `GIsEditor` hala true veya NM dedicated degil | S1 olcer. Olumsuzsa S1b: non-Shipping `-MMOShutdownAllowEditorExit` (§3.2). O da calismazsa cikis kodu **gercek Server hedefi (S01, kaynak motor) gelene kadar "dogrulanmadi" etiketli kalir**; S2'nin PASS kosullari kod 0/JSON kismina daralir, S3 ertelenir (sahte PASS yok) |
| R2 | Editor ikilisi ile test, paketli sunucunun davranisini birebir kanitlamaz | Kabul; belge ve sonuc satiri bunu soyler. Paketli/Shipping dogrulamasi S01 sonrasi tekrar |
| R3 | UE/UBT calisirken derleme/duman cakisir | Kosucu ertele (3); `one builder at a time` kurali; kosucu derleme yapmaz, yalniz hazir ikiliyi kullanir |
| R4 | Istemci acilis suresi degisken (shader) | 210 sn pencere + INCONCLUSIVE kural; flaky olursa sure artirilir, esik asla gevsetilmez |
| R5 | Salt-okunur enjeksiyonu yazimi basarisiz kilmayabilir | §4.2: INCONCLUSIVE; alternatif enjeksiyon S3'te |
| R6 | Kosucu kalan dosyalari `Temp`te biriktirir | Kabul (silme yok); yol sonda yazdirilir |
| R7 | S04 ile `FinishExit` ucuna yeni kod gelince bu test eskirir | Test davranisi (cikis kodu/JSON/atma) sozlesmesini sinar, ic yapiyi degil; slice 4 ayni testi yeniden calistirir |

**Kullaniciya kalan gercek karar: yok** (hepsi varsayilanli). Tek ayri karar K-S6 (karakter `.sav` atomik yazim): varsayilan **evet, ama L4'ten sonra ayri commit**, cunku cok dar pencere ve S04/DB modu bunu zaten yerine koyacak; yerel mod kapali alfada varsayilan kaliyorsa `S6` yapilmali.

## 9. "Degmez" notu

HUD geri sayim bandi, istemci chat alindi logu (sicak dosya ve deger yok), mob/loot mutasyon kapisi (§0), operator ozet araci (§0), fabrika bilesenleri (§0.1): **simdi degmez**.

## 10. Olcum kaydi (S1 sonrasi doldurulur)

| Olcum | Sonuc |
|---|---|
| `-game -server` komut satiri ve acilis suresi | _bos_ |
| `GIsEditor` / `shutdown_exit_skipped_editor` | _bos_ |
| `Process.ExitCode` (kod 0) | _bos_ |
| `MMOShutdownResult.json` yolu (`-UserDir` ile) | _bos_ |
| `MMOSave_*.sav` yolu (`-UserDir` ile) | _bos_ |
| Istemci atilma log satirinin metni | _bos_ |

## 11. S0 durum notu (2026-11-01)

**S0 UYGULANDI** (UE/oyun/sunucu baslatilmadi; gercek `Saved/` okunmadi/yazilmadi): `Scripts/ShutdownSmokeEvidence.ps1` (saf `Get-ShutdownEvidenceVerdict`, PASS | FAIL | INCONCLUSIVE + `[MMO_SMOKE_VERDICT|REASON|FACT]` satirlari),
`Scripts/TestShutdownSmokeEvidence.ps1` (219 kontrol, `PASS checks=219`; 89 olumsuz + 14 olumlu vaka adi), `Scripts/TestShutdownSmokeEvidenceMutations.ps1` (11 mutasyon, hepsi belirli FAIL etiketiyle yakalandi, control temiz). Windows PowerShell 5.1 ile calistirildi; PowerShell 7 DOGRULANMADI.
Anlam: FAIL = kanit celisiyor (yanlis kod, bayat `run_id`, sahte/bozuk satir, sira bozuk, yinelenen olay); INCONCLUSIVE = kanit eksik (yarim log, istemci katilamadi, `shutdown_exit_skipped_editor`, enjeksiyon etkisiz); PASS yalniz ikisi de yokken.
Belgeden **bilincli sapmalar** (S2 kosucusu bunlara gore beslenir):
1. `.sav` yazim zamani ust siniri `flush_done` degil, cikis zamanidir (JSON `time_utc` + 5 sn): atma aninda yeniden yazim ve `EndPlay` kaydi `flush_done`'dan SONRA da dosyayi yazar. `savefail`'de yazim basarisiz olur: yalniz "var ve > 0 bayt" aranir (zaman penceresi yok).
2. Istemci atilma/ret log metni S1'de olculmedi: `-ClientKickPattern` / `-ClientRejectPattern` bos ise `[MMO_SMOKE_FACT] name=client_kick_trace value=unmeasured` basilir (PASS'i engellemez, ama "gozlenmedi" diye kayitlidir); verilirse ve bulunmazsa INCONCLUSIVE.
3. Savefail'de `Error:` satirlari yalniz `-AllowedErrorPattern`'e uyuyorsa izinli (varsayilan bos = hic Error izinli degil); desen S1/S3 olcumunden sonra verilir.
4. Belge 7.2 M9-M11 (runner mutasyonlari) S2'ye aittir; bu dilimin M9-M11'i degerlendirici korumalaridir (sahte satir, istemci logunda sahte kanit, cikis kodu dogrulamasi).
5. Log satir bicimleri `MMOServerLifecycle.cpp`'den alindi, gercek bir `-ABSLOG` ile karsilastirilmadi (S1 isi): zaman damgasi onegi `[yyyy.MM.dd-HH.mm.ss:fff][n]` istege bagli kabul edilir.
Sonraki: S1 (tek-seferlik olcum) -> S2 kosucu.
