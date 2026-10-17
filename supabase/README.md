# supabase/ - V2 karakter deposu (Dilim 1, SQL)

**Durum (2026-10-17 dev projesinde ilk gercek kosu): migration Supabase SQL Editor'de HATASIZ uygulandi; katalog ve HTTPS RPC dogrulamasi gecti (asagida "Dev projesinde dogrulananlar").
`tests/run_all.sql` ve 05_* esozamanlilik testleri HENUZ KOSULMADI (psql kurulu degil); sozlesme senaryolari (02) gercek DB'de DOGRULANMADI.** Makinede psql / docker / supabase CLI yok;
SQL soz dizimi bir ayristiriciyla denenmedi (yalniz `vectors.json` bir Python betigiyle denetlendi, asagida). Hata cikarsa once burada duzeltilir.
Tasarim: `docs/DESIGN_SUPABASE.md` (3.2 sema, 3.3 RPC, 3.4 idempotency, 3.6 vektorler, 5 guvenlik, 7.2 Dilim 1).

**v2 dosyasi yerinde duzeltildi (2026-10-17, QA + guvenlik denetimi turu 1-2), SONRA dev projesine uygulandi (ayni gun, SQL Editor):** uygulamadan once
`20261017120000_v2_character_store.sql` ayni dosyada duzeltildi (yeni migration dosyasi ACILMADI). **Dev projesine uygulandigi icin bu dosya artik DEGISTIRILMEZ;**
her degisiklik yeni zaman damgali dosyayla gelir (dev'de tablolari silip yeniden uygulamak ayri, bilincli bir karardir). Degisenler: `lease_epoch` tek ad, `get_op.found`, `p_op_id` kapsami, `purge_write_ops`, varsayilan yetkiler,
kilit sonrasi zaman, zarf denetimi CHECK -> tetikleyici, `contract_version` 2 ve kati `vectors.json`.

**Gecici varsayimlar (kullanici onaylamadi, geri alinabilir):** mimari C (Supabase = barindirilan PostgreSQL + PostgREST RPC; Auth/Realtime/Storage yok);
`mmo` semasi API'ye ACIK DEGIL; `mmo_api` fonksiyonlari yalniz `service_role`; tablolarda RLS acik + politikasiz; ad kurali `MMONameRules::CanonicalKey`
(SQL tarafi ASCII disi adi ayrica REDDEDER).

## V1 UYGULANMAZ
`Migrations/V1__initial_schema.sql` (Flyway adi, `public` semasi, RLS yok) Supabase'e **uygulanmaz**: `public` PostgREST'e aciktir, V1 tablolari anon anahtarla okunur/yazilir olurdu.
Tek kaynak `supabase/migrations/`.

## Dosyalar
| Dosya | Ne |
|---|---|
| `migrations/20261017120000_v2_character_store.sql` | Sema (`mmo`), RPC (`mmo_api`), yetkiler, RLS, tetikleyiciler. Tek islem (begin/commit). |
| `tests/vectors.json` | **Kati kontrat** (`contract_version` 2): `transport`, `rpc_params` (her fonksiyonun TAM parametre kumesi/tip/zorunlu), `responses` (her fonksiyon x outcome ornek tam yanit), `scenarios` (adim adim istek -> beklenen yanit; `expect_error` adimlari dahil), `canonical_key` ve `holder` vektorleri, limitler. UE native testi / sahte sunucu **bu dosyayi** okur. |
| `tests/00_local_roles.sql` | Yalniz duz (Supabase olmayan) PG: anon/authenticated/service_role yoksa olusturur. |
| `tests/01_canonical_key.sql` | Anahtar + holder vektorleri, `limits` regex'leri, dogrulayici birimleri. |
| `tests/02_rpc_scenarios.sql` | `vectors.json` senaryolarini service_role olarak calistiran yorumlayici + sozlesme denetimleri: `rpc_params` == katalog, arguman adlari, `expect_error` (42883), yanit anahtar/tip sekli, `invalid_reasons`. |
| `tests/03_constraints.sql` | CHECK/FK/UNIQUE, tetikleyiciler (revision/epoch geri gitmez), zarf tetikleyicisi (kira guncellemesinde calismaz), `purge_write_ops` (sinirlar, 5000 parti), cascade, boyut tavani. |
| `tests/04_security.sql` | RLS acik+politikasiz, anon/authenticated tabloya ve RPC'ye erisemez, `purge_write_ops` yalniz service_role, varsayilan yetkiler (`pg_default_acl`), zaman-kilit sirasi kaynak bekcisi, `search_path`, dinamik SQL bekcisi. |
| `tests/05_concurrency_a.sql`, `_b.sql`, `_check.sql` | Esozamanli iki islem (op_id kilidi + satir kilidi), IKI terminal ister, kalici veri yazar (yalniz dev). |
| `tests/05_concurrency_lease_a.sql`, `_b.sql` | Kira suresi satir kilidi beklerken biterse yazim `ownership_lost` (zaman kilit sonrasi alinir), IKI terminal, yalniz dev. |
| `tests/run_all.sql` | 01-04'u tek komutla kosar (hepsi ROLLBACK; iz birakmaz). |

## Sozlesme (contract_version 2) - UE tarafi icin

Tasima: `POST {url}/rest/v1/rpc/{fonksiyon}`, govde = parametre adlari ile JSON nesnesi, basliklar `Content-Type: application/json`, `Content-Profile: mmo_api`, `apikey: <secret key>`. **`Authorization: Bearer` yalniz legacy JWT anahtari icin**; yeni `sb_secret_...` anahtari JWT degildir, Bearer'a KONMAZ (sozlesme listesindeki `Authorization` basligi, yeni anahtar modeline gecildiginde UE'de kaldirilir; Dilim 2 teyidi).
**[Dogrulandi: dev, 2026-10-17]** `sb_secret_...` anahtari gercek HTTPS cagrisinda YALNIZ `apikey` ile (Bearer YOK) kabul edildi.
**User-Agent kurali [Dogrulandi: dev, 2026-10-17]:** istek `User-Agent`'i tarayici gibi gorunurse secret key REDDEDILIR (`Forbidden use of secret API key in browser`). Istemci `Mozilla`, `Chrome`, `Safari`, `Gecko`, `AppleWebKit`, `Edge` icermeyen ACIK bir UA gondermeli (ornek `MMOGameServer/0.1`); `vectors.json` `transport.user_agent_note`.
HTTP 200 + `outcome` alani is sonucudur. **Fazla veya eksik (zorunlu) parametre HTTP 404 `PGRST202`** (SQL'de `42883`); UE sahte sunucusu bunu aynen taklit eder. Opsiyonel parametre hic gonderilmeyebilir.
`bigint` alanlari (`revision`, `lease_epoch`, `epoch`, `deleted`, ...) JSON **sayi**dir.

| Fonksiyon | Parametreler (sirali; `*` = zorunlu) | `p_op_id` |
|---|---|---|
| `healthcheck` | (yok) | yok |
| `load_character` | `p_key`* text | yok |
| `character_exists` | `p_key`* text | **yok** |
| `list_keys` | `p_prefix` text (''), `p_limit` int (100; 1..500) | yok |
| `create_character` | `p_key`* text, `p_display_name`* text, `p_holder`* text, `p_op_id`* uuid, `p_schema_version`* int, `p_data`* jsonb, `p_race` text (null) | **var** |
| `acquire_lease` | `p_key`* text, `p_holder`* text, `p_ttl_s` int (120; 10..600) | **yok** |
| `renew_lease` | `p_key`* text, `p_holder`* text, `p_epoch`* bigint, `p_ttl_s` int (null) | yok |
| `release_lease` | `p_key`* text, `p_holder`* text, `p_epoch`* bigint | yok |
| `save_character` | `p_key`* text, `p_holder`* text, `p_epoch`* bigint, `p_expected_revision`* bigint, `p_op_id`* uuid, `p_schema_version`* int, `p_data`* jsonb, `p_side` jsonb (null) | **var** |
| `get_op` | `p_op_id`* uuid | **var** |
| `import_character` | `p_key`* text, `p_display_name`* text, `p_source_sha256`* text, `p_schema_version`* int, `p_data`* jsonb, `p_race` text (null) | **yok** |
| `purge_write_ops` | `p_older_than_days` int (7; 1..3650), `p_batch` int (5000; 1..50000) | yok |

`p_op_id` YALNIZ `create_character`, `save_character`, `get_op`'ta: `acquire_lease` (kayitli sonuc tekrari eski veriyi dondurebilirdi; ayni sahip icin cagri zaten dogal idempotent) ve `character_exists`/`import_character`
(ice aktarma `p_source_sha256` ile idempotent) `p_op_id` ALMAZ. UE bu fonksiyonlara `p_op_id` **gondermeyecek**.

Kural vektorleri (UE dogrular, `vectors.json` `limits` + `holder` + `canonical_key`): holder `^[A-Za-z0-9_.:-]{1,96}$`; anahtar `^[a-z0-9_-]{1,32}$`; ad `^[A-Za-z0-9_-]([A-Za-z0-9 _-]{0,30}[A-Za-z0-9_-])?$`
(`" Ali"`, `"Ali "`, `"   "`, ASCII disi, 33+ karakter RED); ayrilmis adlar `default`, `player<rakam>`.

**Kira donemi alani TEK ad: `lease_epoch`** (`load_character`, `acquire_lease`, `renew_lease`, `release_lease`, `create_character`, `save_character`, ve `ownership_lost` govdelerinde). Geriye donuk `epoch`
(ayni deger) `acquire/renew/release/create/save` yanitlarinda **kalir**; yeni kod yalniz `lease_epoch` okur, `epoch` bir sonraki sozlesme surumunde kalkabilir.

`outcome` kumesi: ok, committed, imported, already_imported, not_found, already_exists, invalid(+reason), revision_conflict, ownership_lost, idempotency_conflict, conflict, internal.

Yanit ozeti (tam ornekler `vectors.json` `responses`):
- `get_op`: bulunursa `{outcome:'ok', found:true, op_kind, canonical_key, result, created_at}`, bulunamazsa `{outcome:'not_found', found:false}`.
- `release_lease`: `{outcome:'ok', already_released:bool, lease_epoch, epoch}` (tekrar cagri `already_released:true`).
- `import_character`: `imported` her zaman `replaced_import:bool` tasir.
- `healthcheck`: `{outcome:'ok', api_version:2, contract_version:2, schema:'v2', server_time}` (UE bunu `contract_version` ile karsilastirir).
- `purge_write_ops`: `{outcome:'ok', deleted, more}`; `invalid/bad_days` (gun araligi disi), `invalid/bad_batch` (`p_batch` 1..50000 disi).
- **Boyut tavani semantigi:** `data_max_octets` (2097152) ve `side_value_max_octets` (262144) SQL'de `octet_length(jsonb::text)` ile olculur: `jsonb::text` ayiraclardan sonra bosluk koyar, yani UE'nin yogun JSON'undan **uzundur**, ve olcu **bayttir** (UTF-8), karakter degil. UE karakter sayarsa 2 MiB'a yakin bir kayit UE'de gecip DB'de `invalid/data_too_large` alabilir (yeniden denemeyle duzelmez). Davranis degismedi; `vectors.json` `limits.data_octet_limit_note`.
- `p_data` = `FMMOCharacterSaveJson::ToJson` zarfi `{"SchemaVersion":n,"Data":{...}}`; DB yalniz tip/boyut/surum eslesmesi ve Level 1..999, Experience>=0, Gold>=0, NextInstanceId>=1 (int32, tam sayi) denetler.

### create_character ve sonuc belirsizligi (yanit kayboldu / zaman asimi)
- Yeniden deneme **AYNI `p_op_id` + AYNI yukle** yapilir: ilk deneme commit olduysa kayitli sonuc `replayed:true` ile doner (test: `create_rules`, `create_ambiguous_result`).
- **Yeni** op_id ile tekrar edilirse ve ilk deneme commit olduysa `already_exists` doner: bu, "baska biri ayni adi aldi" durumundan **ayirt edilemez**. Ayirmak icin ilk op_id ile `get_op` sorulur
  (`found:true` + `op_kind:'create'` = karakteri biz olusturduk; `found:false` = baska biri ya da gunluk temizlendi).
- `not_found` (`get_op`) kesin "olmayacak" demek degildir (R1, asagida). `purge_write_ops` ile silinen (>=7 gun) op_id de `found:false` doner.

### write_ops temizligi ve pg_cron (BURADA CALISTIRILMADI)
`mmo_api.purge_write_ops(p_older_than_days default 7, p_batch default 5000)`: yalniz `service_role`; gun 1..3650 (son 24 saat korunur), `p_batch` 1..50000 (aralik disi `bad_batch`);
tek cagrida en cok `p_batch` satir (en eskiden), kalan varsa `more:true`. **Fonksiyon kendi icinde dongu yapmaz** (tek islem uzun kilit tutardi; bir `purge_write_ops_all` bilincli YOKTUR).
Yalniz `mmo.write_ops` satirlari silinir. Saklama suresi istemcinin AYNI op_id ile yeniden deneme ufkundan uzun olmali.
Boyut: dakikada bir kaydeden 4 oyuncu = gunde ~5760 satir; ucretsiz katmanda 500 MB. Gunde bir 5000 satirlik cron tabloyu buyutur; bu yuzden **5 dakikada bir, 20000 satirlik parti**
(gunde 288 cagri x 20000 = ~5,7 milyon satir/gun kapasite, yuke gore bol) ve backlog varsa tek seferlik elle dongu (asagida). Zamanlama (Supabase'de `pg_cron` etkinlestirilirse, kullanici elle; BURADA CALISTIRILMADI):
```sql
-- TEK is: her 5 dakikada bir parti (her cron calismasi AYRI islemdir). Ikinci/paralel is KURMA:
-- iki is ayni en eski satirlari secer (bos is yapar, gereksiz kilit), kapasite artmaz.
select cron.schedule('mmo-purge-write-ops', '*/5 * * * *', 'select mmo_api.purge_write_ops(7, 20000)');
```
**`p_batch` tam sayi olmalidir** (`20000`, `20000.5` degil): ondalikli deger PostgREST'te `bad_batch` degil, tip donusturme hatasi (HTTP 4xx, `outcome` yok) verir; istemci tam sayi gondermeli.
`more:true` ise bir sonraki cron calismasi devam eder; birikmis backlog icin tek seferlik elle dongu kullan (asagida); HIZLA bosaltmak gerekirse (elle, psql) her iterasyonu AYRI bir cagri olarak gonder, **tek `BEGIN`/`DO` blogu icinde dongu kurma**
(`DO` bloku tek islemdir: tum silmeler tek COMMIT'te kilitli kalir). psql ile ornek (her `-c` ayri islemdir):
```
psql -v ON_ERROR_STOP=1 -c "select mmo_api.purge_write_ops(7, 20000)"    # sonucta more=true ise tekrarla
```
Dongu istersen kabuktan (her cagri ayri psql = ayri islem) `more` gorunene kadar tekrarla; baglanti dizesi asagidaki parolasiz kurala uyar. Bir `DO` bloku dongusu ONERILMEZ (tek islem).
Silinen satirlar >= 1 gun eskidir; yazim yollari bu satirlari degistirmez, bu yuzden kaydetme ile silme arasinda satir kilidi cakismasi beklenmez. Tek cron isi oldugundan purge-purge cakismasi da beklenmez
(bir calisma 5 dakikadan uzun surerse sonraki calisma ust uste binebilir: ayni satirlari silmeye calisan iki islem birbirini bekler, hata vermez; parti 20000 oldugundan pratikte beklenmez) [Tahmin, olculmedi].

### `holder` benzersizligi (isletme kurali; N1)
`p_holder` kira sahibini tanimlar. Donmus yuva duzeltmesi (release -> acquire ile epoch artirma) **`holder`'in SUREC BASINA BENZERSIZ oldugunu varsayar**: iki surec AYNI holder ile
calisirsa B'nin aldigi kira (yeni epoch) A tarafindan devralinir (DB ikisini ayirt edemez) ve cit (fencing) asilir; iki surec ayni karakteri yazabilir (kopyalama riski).
- Kural: `holder` = sunucu kimligi + acilis rastgeleligi (boot nonce/GUID), ornek `map01-3f9c2a1be07d4c55`. Her surec acilista yeni uretir; yeniden baslatilan surec ESKI holder'i kullanmaz.
- UE deposu acilista holder'i `^[A-Za-z0-9_.:-]{1,96}$` ile dogrular (uzunluk 1..96, karakter kumesi; `vectors.json` `holder` vektorleri); gecersizse depo acilmaz.
- Ayni holder iki surecte KULLANILMAZ. DB bunu zorlayamaz (iki surec ayni dizeyi gonderir); onlem UE/isletme tarafindadir (DESIGN_SUPABASE.md §4.4/§5.1, Dilim 3 kabul olcutu).

## Dev projesinde dogrulananlar [Dogrulandi: dev, 2026-10-17]
Hesap: Supabase **FREE** katman, bolge **West EU (Ireland)**, tek proje `main` (dev olarak kullaniliyor; ayri `myproject-dev` / `myproject-alpha` projeleri YOK). Proje referansi/URL bu belgelerde yazilmaz (`<ref>`).
Anahtar yerel kullanici ortam degiskenlerinde (`MMO_SUPABASE_URL`, `MMO_SUPABASE_SECRET_KEY`); PowerShell `Read-Host -AsSecureString` ile kaydedildi; sohbete/repoya yazilmadi.

| # | Dogrulanan | Kanit turu |
|---|---|---|
| 1 | `20261017120000_v2_character_store.sql` (824 satir) SQL Editor'de tek seferde **HATASIZ** calisti (`Success. No rows returned`). Bu ilk gercek kosudur. | Dashboard ciktisi |
| 2 | Katalog sorgusu: `mmo_api`'de **12 fonksiyon**, HEPSINDE `anon=false`, `authenticated=false`, `service_role=true`. `mmo` semasinda **6 tablo**, hepsinde **RLS acik**. `anon` / `authenticated` / PUBLIC icin tablo yetkisi **0**. | Katalog sorgusu (SQL Editor) |
| 3 | `healthcheck` -> `{outcome:'ok', api_version:2, contract_version:2, schema:'v2'}`. | SQL Editor + HTTPS |
| 4 | Data API **Exposed schemas: yalniz `mmo_api`**. Anahtarsiz `GET /rest/v1/character_saves` **REDDEDILDI**. | Dashboard + HTTPS |
| 5 | Gercek HTTPS RPC cagrisi `POST /rest/v1/rpc/healthcheck` (`apikey: sb_secret_...`, `Content-Profile: mmo_api`, **Bearer YOK**) **BASARILI**: `sb_secret_` anahtari yalniz `apikey` ile kabul ediliyor. | HTTPS |
| 6 | **User-Agent kurali:** tarayici gibi UA (Windows PowerShell varsayilani `Mozilla/5.0 ... WindowsPowerShell`) ile secret key REDDEDILIR: `{"message":"Forbidden use of secret API key in browser","hint":...}`. Acik UA (`MMOGameServer/0.1`) ile ayni cagri basarili. | HTTPS |

**DOGRULANMADI (hala):** `tests/run_all.sql` (01-04) ve 05_* esozamanlilik testleri hicbir yerde kosulmadi; `vectors.json` 20 senaryosu gercek PostgREST/PG'ye karsi calistirilmadi
(UE sahte sunucusu ile gercek ayrisabilir); `healthcheck` disindaki 11 RPC HTTPS ile cagrilmadi; Bearer'li `sb_secret_` cagrisi denenmedi; UE `FHttpModule` / WinHttp'nin varsayilan User-Agent'i
ve basliklari ne gonderdigi; publishable (anon) anahtarla RPC/tablo negatifi ayri bir anahtarla denenmedi (madde 4 anahtarsiz GET'tir); Dashboard listesindeki diger maddeler (asagida).

**Sonuc (UE tarafi icin):** oyun sunucusu istemcisi `User-Agent`'i acikca ayarlamali (tarayici kelimesi icermeyen, ornek `MMOGameServer/0.1`) ve secret key'i yalniz `apikey` ile gondermeli. Bu duzeltme ayri ajanda (Dilim 2 UA); bu belge kod degistirmez.

Elle dogrulama ornekleri (anahtar komut satirina YAZILMAZ; ortam degiskeninden okunur; ciktiyi/anahtari sohbete yapistirma):
```powershell
# PowerShell: -UserAgent ZORUNLU (varsayilan UA "Mozilla/5.0 ... WindowsPowerShell" secret key'i reddettirir). Bearer YOK.
$h = @{ 'apikey' = $env:MMO_SUPABASE_SECRET_KEY; 'Content-Profile' = 'mmo_api'; 'Content-Type' = 'application/json' }
Invoke-RestMethod -Method Post -Uri "$env:MMO_SUPABASE_URL/rest/v1/rpc/healthcheck" -Headers $h -Body '{}' -UserAgent 'MMOGameServer/0.1'
```
```bash
# curl: -A ZORUNLU degil ama acik UA'yi kesinlestirir (curl varsayilani curl/x.y, tarayici gibi degil). Basliklar dosyadan (adim 7).
curl -s -A 'MMOGameServer/0.1' -X POST "$MMO_SUPABASE_URL/rest/v1/rpc/healthcheck" -H @$HOME/.mmo_headers -d "{}"
```

### Sorun giderme
| Belirti | Neden / cozum |
|---|---|
| `{"message":"Forbidden use of secret API key in browser","hint":...}` | Istek `User-Agent`'i tarayici gibi (Mozilla/Chrome/Safari/Gecko/AppleWebKit/Edge). Acik UA gonder: PowerShell `-UserAgent 'MMOGameServer/0.1'`, curl `-A 'MMOGameServer/0.1'`, UE istemcisi `User-Agent` basligini ayarlar. Anahtar dogru olabilir; anahtari degistirme/dondurme gerekmez. |
| HTTP 404 `PGRST202` | Fazla/eksik parametre (ornek: `acquire_lease`'e `p_op_id`), ya da `Content-Profile: mmo_api` eksik. Sozlesme tablosuna bak. |
| 401 / `Invalid API key` | `apikey` basligi yok/yanlis; `sb_secret_...` anahtarini `Authorization: Bearer` ile gonderme. |
| `GET /rest/v1/<tablo>` anahtarsiz/anon ile veri donuyor | **ACIL:** Exposed schemas'ta `mmo`/`public` var demektir; yalniz `mmo_api` birak. |

## Kullanicinin dev projede calistiracagi sira
Oncesi: `docs/DESIGN_SUPABASE.md` 6. bolum Dilim 0 (hesap, **dev** projesi, veritabani sifresi parola yoneticisinde; sifreyi/anahtari sohbete YAPISTIRMA).
**Sir kurali (komut satirina sir YAZMA):** parola/anahtar komut satirinda `ps`, kabuk gecmisi ve CI loglarinda gorunur. Bu yuzden:
- psql icin baglanti dizesinde **parola olmaz**; parola `PGPASSWORD` ortam degiskeninden (oturum basina, gecmise yazmadan, ornegin `read -rs PGPASSWORD; export PGPASSWORD`) ya da `~/.pgpass` (Windows: `%APPDATA%\\postgresql\\pgpass.conf`, yalniz kendi kullanicin okuyabilsin) gelir.
  Parolasiz baglanti dizesi, `sslmode=verify-full` ile (Supabase CA sertifikasini Dashboard -> Database -> SSL'den indir):
  `export DB="host=<host> port=5432 dbname=postgres user=<kullanici> sslmode=verify-full sslrootcert=/yol/prod-ca-2021.crt"` ve cagri `psql "$DB" ...` (parola `PGPASSWORD`/pgpass'tan).
  `DATABASE_URL` icinde `postgresql://kullanici:PAROLA@...` KULLANMA.
- curl icin anahtar `-H` ile komut satirina yazilmaz: basliklari kendi kullanicinin okuyabildigi bir dosyaya koy (`chmod 600`) ve `-H @dosya` (curl >= 7.55) ile ver, ya da `-K -` ile stdin'den yapilandirma oku
  (ornek asagida, adim 7). Anahtari asla `echo`/`export KEY=...` satirina elle yazip gecmise birakma; ortam degiskeni gerekiyorsa `read -rs`.
**Durum (2026-10-17):** adim 3 (migration, SQL Editor yoluyla) ve 4 (Exposed schemas) ve 7'nin healthcheck kismi YAPILDI/dogrulandi (yukaridaki bolum); adim 1-2 psql gerektirir (psql YOK, yapilmadi);
**adim 5 (`run_all.sql`) ve 6 (esozamanlilik) KOSULMADI.**
1. Baglanti bilgisi (dev projesi, Dashboard -> Connect): host/kullanici `DB` degiskeninde (yukaridaki bicim, PAROLASIZ), parola `PGPASSWORD`/pgpass'ta; yalniz kendi makinende, repoya yazma.
2. `psql "$DB" -c "select 1"`  -> `1` (baglanti canli).
3. Migration (iki yoldan biri):
   - psql: `psql "$DB" -v ON_ERROR_STOP=1 -f supabase/migrations/20261017120000_v2_character_store.sql`
   - CLI: `supabase link --project-ref <ref>` ardindan `supabase db push` (once `supabase init`; olusan `supabase/config.toml` icinde `[api] schemas = ["mmo_api"]` olsun).
4. Dashboard -> Data API -> **Exposed schemas**: yalniz `mmo_api` (`public` ve `mmo` YOK). Bu psql ile yapilamaz, elle.
5. `psql "$DB" -v ON_ERROR_STOP=1 -f supabase/tests/run_all.sql` (depo kokunden, Git Bash; psql >= 13). Her dosya `PASS ...` yazmali.
6. Eszamanlilik (iki terminal): `05_concurrency_a` + `-v mode=same_op` `05_concurrency_b` + `05_concurrency_check.sql`; sonra `other_op`; sonra `05_concurrency_lease_a` + `05_concurrency_lease_b` (basliklarda ayrinti).
7. Uc uca HTTP denetimi (secret key komut satirinda GORUNMEZ). Basliklari `chmod 600` bir dosyaya yaz (repo disinda, orn. `~/.mmo_headers`; icerigi: `apikey: <sb_secret_...>`, `Content-Type: application/json`, `Content-Profile: mmo_api` satirlari; secret key'i `Authorization: Bearer` olarak **koyma**, bkz. asagidaki kontrol listesi) ve:
   `curl -s -A 'MMOGameServer/0.1' -X POST "$MMO_SUPABASE_URL/rest/v1/rpc/healthcheck" -H @$HOME/.mmo_headers -d "{}"`  (`-A` acik User-Agent: tarayici gibi UA secret key'i reddettirir, bkz. sorun giderme; alternatif: `-K -` ile yapilandirmayi stdin'den ver; `header = "apikey: ..."` satiri komut satirinda degil stdin'de kalir)
   -> `{"outcome":"ok","contract_version":2,...}`. Publishable (anon) anahtarla ayni cagri **reddedilmeli**, `GET /rest/v1/character_saves` de veri DONMEMELI. `acquire_lease` cagrisina `p_op_id` eklenirse `404 PGRST202` gelmeli.
### Kalan kullanici adimlari (2026-10-17 sonrasi; sirayla)
1. **Testleri kosmak icin** iki yoldan biri: psql >= 13 kur (PostgreSQL istemcisi), `PGPASSWORD`/pgpass ile `run_all.sql` (adim 5) ve iki terminalli 05_* (adim 6). **Testler psql'e baglidir:** `run_all.sql` `\set vectors \`cat ...\``, `\i`, `\echo` kullanir
   (01/02 dosyalari psql meta-komutlari ve `:vectors` degiskeni icerir; her dosyada ROLLBACK gecer), bu yuzden SQL Editor'e yapistirilarak kosulamaz [okunan kaynaktan; denenmedi]. psql'siz secenek yalniz kisa smoke:
   (SQL Editor, salt okuma) `select mmo_api.healthcheck();`, `select proname from pg_proc p join pg_namespace n on n.oid=p.pronamespace where n.nspname='mmo_api';` (12 satir) ve
   `select tablename, rowsecurity from pg_tables where schemaname='mmo';` (6 satir, hepsi true).
2. Dashboard Auth: **signup kapali, anonim giris kapali**, e-posta/OAuth saglayicilari kapali.
3. **Legacy anon / service_role JWT anahtarlarini devre disi birak** (API Keys -> Legacy); once sb_secret_ ile calistigini teyit et (teyit edildi: madde 5 yukarida).
4. **Yedek / PITR:** FREE katmanda otomatik yedek olmayabilir [Dogrulanmadi]; ilk elle `pg_dump` (psql ile birlikte kurulur) veya plan yukseltme karari (`docs/DECISIONS_NEEDED.md` S5/S6).
5. **MFA** (Dashboard hesabi), kurtarma kodlari parola yoneticisinde.
6. Digerleri asagidaki listede: pg_graphql, Realtime, SSL zorunlu, Network Restrictions, `write_ops` temizlik cron'u.
7. Opsiyonel: Bearer'siz/Bearer'li `sb_secret_` karsilastirmasi ve publishable anahtarla RPC negatifi (ayri ve kasitli denemeler; sonucu not et).

### Dashboard guvenlik kontrol listesi (GUNCEL, elle; hicbiri psql/migration ile yapilamaz; yalniz isaretli olanlar dogrulandi, kalani kullanici panelde teyit eder)
- [x] Data API -> **Exposed schemas: YALNIZ `mmo_api`** (`public`, `mmo`, `graphql_public` YOK). [Dogrulandi: dev, 2026-10-17; "yalniz mmo_api" soylendi; `graphql_public` ayrica kontrol edilmedi, pg_graphql maddesine bak]
- [ ] **pg_graphql kapali** (Database -> Extensions; `graphql_public` semasi acik kalmasin).
- [ ] API anahtarlari: **yeni `sb_secret_...` anahtari** sunucuya; **legacy `service_role` / `anon` JWT anahtarlari DEVRE DISI** (API Keys -> Legacy). Publishable anahtar kullanilacaksa istemciye bile verilmez (oyun istemcisi DB'ye dogrudan baglanmaz).
- [x] **Anahtar basligi:** `sb_secret_` anahtarlari JWT DEGILDIR; `Authorization: Bearer <sb_secret_...>` olarak GONDERME (JWT sanilip reddedilir ya da yanlis yola duser); yalniz **`apikey: <sb_secret_...>`** basligi. [Dogrulandi: dev, 2026-10-17: yalniz apikey ile basarili; Bearer'li hali denenmedi.] `vectors.json` `transport.headers` hala Authorization satirini listeler: UE tarafi `apikey`-only ile uyumlu mu, Dilim 2'de teyit edilecek.
- [x] **User-Agent:** tarayici gibi UA secret key'i reddettirir; UE/curl/PowerShell acik UA gondermeli (`MMOGameServer/0.1`). [Dogrulandi: dev, 2026-10-17] UE tarafi duzeltmesi: ayri ajan (Dilim 2 UA), henuz yapilmadi.
- [ ] Auth: **signup kapali, anonim giris kapali**, e-posta/OAuth saglayicilari kapali (Auth kullanilmiyor).
- [ ] Realtime: yayinda (`supabase_realtime` publication) **`mmo.*` tablolari YOK**; Realtime hic kullanilmiyor.
- [ ] **SSL zorunlu** (Database -> Settings -> SSL enforcement) ve baglanti `sslmode=verify-full`.
- [ ] **Network Restrictions**: veritabani dogrudan baglantisi yalniz bilinen IP'lere (sunucu/kendi makinen).
- [ ] Dashboard hesabinda **MFA** (ve organizasyon sahibi icin); kurtarma kodlari parola yoneticisinde.
- [ ] **Yedek / PITR**: plan neyi destekliyorsa etkin; ucretsiz katmanda otomatik yedek olmayabilir [Dogrulanmadi]; ilk migration oncesi elle `pg_dump`.
- [ ] **Spend cap acik** (Pro plana gecildiyse, beklenmedik fatura korumasi); kullanim/disk (500 MB ucretsiz) uyarilari acik, `write_ops` temizlik cron'u kurulu.

Duz yerel PG (Docker'siz kurulum) ile denemek istersen once `tests/00_local_roles.sql`, sonra migration, sonra testler.

## Sapmalar (belge -> bu uygulama)
1. **`write_ops`** adi korundu (belge 3.2); gorev metnindeki `op_log` ayni sey. `op_kind` yalniz `create`/`save`.
2. **FORCE RLS yok**, `ENABLE` var: SECURITY DEFINER fonksiyonlar tablo sahibi olarak calisir; FORCE + sahibin BYPASSRLS'siz olmasi fonksiyonlari sessizce 0 satira mahkum ederdi. Asil koruma: tablolarda hicbir rolun (service_role dahil) yetkisi yok + `mmo` API'ye acik degil. (Belge: FORCE.)
3. **`save_character` SECURITY DEFINER** (belge taslagi `security invoker`): invoker olsaydi `service_role`a tablo yetkisi vermek gerekirdi. Tum fonksiyonlarda `set search_path = ''`, dinamik SQL yok (`04` kaba bekci).
4. **`acquire_lease`'te `p_op_id` yok** (belge var): bkz. yukaridaki sozlesme tablosu. Belge 3.3 bu sozlesmeyle guncellendi.
5. **Kira suresi dolmus + ayni sahip tekrar `acquire_lease`**: epoch +1 (belge yalniz "ayni sahip -> artmaz" diyordu; gecerli kirada artmaz, dolmus/birakilmis kirada artar: olu istek karsi onlemi). **Donmus/gec-yazim yuvasi:** CANLI kirada ayni sahip icin epoch artmaz (bilincli); sunucu epoch'u artirmak isterse `release_lease(p_epoch)` sonra `acquire_lease` cagirir (`lease_epoch + 1`), eski epoch'lu `save_character` `ownership_lost` doner. Senaryo: `vectors.json` `frozen_slot_epoch_bump` (02 testi calistirir).
6. **Ek sutunlar:** `character_saves.lease_ttl_s` (acquire/renew/save ayni sureyle uzatir), `import_sha256` (ice aktarma idempotency'si). `character_id` degismez, `canonical_key` degismez, `revision`/`lease_epoch` geri gitmez (tetikleyici).
7. **Boyut tavani 2 MiB** (belge 1 MiB [Tahmin]): UE `MaxJsonChars` = 2 MiB, daha dusuk tavan gecerli bir kaydi reddederdi. Yan veri tavani 256 KiB (belge ile ayni).
8. **Eklenenler:** `load_character`, `healthcheck`, `character_exists`, `list_keys`, `renew_lease`, `create_character`, `purge_write_ops` (belge listesinde vardi; `load` gorev metninde). `list_keys` onek aramasi `starts_with` (LIKE degil: `_` joker olurdu).
9. **Ad kurali SQL'de katilastirildi:** bas/son bosluk, 32 karakter ustu ve her ASCII disi karakter `bad_name` ile reddedilir (UE bunlari kirpar/keser: bkz. `vectors.json` `Omer`, `Ali!`). Bu bilincli: kirpma baskasinin kaydini acardi.
10. **JSON invariantlari:** `Level 1..999`, `Experience/Gold >= 0`, `NextInstanceId >= 1`, `SchemaVersion` sutunla ayni. Envanter/esya icerigi DB'de denetlenmez (UE `FromJson` sorumlulugu). **Denetim CHECK degil tetikleyicidir** (`character_saves_envelope_ins_trg` / `_upd_trg`): CHECK her UPDATE'te (kira yenileme dahil) 2 MiB'a kadar jsonb'yi yeniden metne cevirirdi; tetikleyici yalniz `data`/`schema_version` DEGISINCE calisir. SQLSTATE hala `23514`. Bedel: veri hic degismeyen eski bozuk satir kira yenilemede yakalanmaz (kasitli; 03'te kanitli).
11. **`purge_write_ops` VAR** (QA/guvenlik turu 1; onceki surumde bilincli yoktu): yalniz `service_role`, en az 1 gun, parti `p_batch` 1..50000 (varsayilan 5000; tur 2), yalniz `write_ops` silinir; kayit silme yetenegi anahtarla ACILMAZ.
12. **Varsayilan yetkiler:** `alter default privileges in schema mmo_api|mmo revoke execute ...`. **DURUST SINIR:** PostgreSQL'de sema duzeyi varsayilanlar global varsayilanlara eklenir; bu REVOKE, yeni fonksiyonda PUBLIC'in yerlesik EXECUTE'unu kaldirmaz. Asil koruma: gelecek her migration sonunda 6. bolumdeki revoke/grant dongusu + `04_security.sql` (katalogdaki her fonksiyonu denetler, unutani yakalar). Rol-genisligi `alter default privileges revoke execute on functions from public;` migration'da yorum olarak duruyor (Supabase `public` semasini etkileyebilir; sahip karari).
13. **Zaman kilit sonrasi:** kira bitisi kararlari (acquire/renew/save/import) `FOR UPDATE`/`op_lock` beklendikten SONRA `clock_timestamp()` ile alinir. Kaniti: `04` kaynak bekcisi + `05_concurrency_lease_*` (iki terminal).

## Bilinen kalan riskler / acik noktalar
- **R1 daralir, kapanmaz:** devirden ONCE gonderilip zaman asimina ugrayan ve DB'ye gec ulasan yazim, hala epoch/revision gecerliyse commit olabilir. Istemci `get_op` ile uzlastirir; `not_found` kesin "olmayacak" demek degildir (bkz. `get_op` aciklamasi).
- Presence ve travel_tickets tablolari var, RPC'si yok (A3/A4). `accounts`/`account_id` kimlik cozmez (K-1 acik: `canonical_key`'i bilen herkes karakteri acar; Steam/A5 gelmeden degismez).
- Irk degeri (`race`) yalniz bicim denetimli; deger kumesi S03 kabulunden sonra CHECK olacak.
- Yan veri ice aktarma (basarim/gunluk/posta) `import_character`'de yok (Dilim 5/7).
- `secret key` harita sunucusunda ortam degiskeni (mimari C'nin bedeli, belge 2 ve 5.1).
- `write_ops` saklama suresi (7 gun) bir tahmin; temizlik cron'u kullanici tarafinda kurulmadikca tablo buyur.

## Soz dizimi dogrulamasi (durustce)
- SQL: **calistirilmadi, ayristirilmadi.** `pglast`/`sqlglot` kurulu degil; pip kurulumu sistem degisikligi sayildigi icin yapilmadi. Kod elle gozden gecirildi (tum nesne adlari niteliklendirilmis, `search_path=''`, SQL fonksiyonlari tanimlanmadan once kullanilmiyor) ve kaba dengeleme denetimi yapildi (`$$` / `$do$` cifti, `begin`-`end` dengesi, parantez dengesi).
- `vectors.json`: Python `json` ile ayristirildi, ASCII, `rpc_params` / `responses` / `invalid_reasons` / `outcomes` capraz denetlendi (her senaryo adimi parametre kumesine uyuyor, `exact` adimlarin anahtar kumesi `responses` ornegiyle ayni), 34 anahtar ve 15 holder vektoru Python regex karsiliklariyla eslesti (SQL'in kendisiyle degil).
