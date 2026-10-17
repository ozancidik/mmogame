# supabase/ - V2 karakter deposu (Dilim 1, SQL)

**Durum: YAZILDI, HICBIR PostgreSQL'DE CALISTIRILMADI.** Makinede psql / docker / supabase CLI yok; SQL soz dizimi bile bir ayristiriciyla dogrulanmadi
(yalniz `vectors.json` bir Python betigiyle denetlendi, asagida). Ilk gercek kosu kullanicinin **dev** projesindedir; hata cikarsa once burada duzeltilir.
Tasarim: `docs/DESIGN_SUPABASE.md` (3.2 sema, 3.3 RPC, 3.4 idempotency, 3.6 vektorler, 5 guvenlik, 7.2 Dilim 1).

**Henuz uygulanmadi, v2 dosyasi yerinde duzeltildi (2026-10-17, QA + guvenlik denetimi turu 1):** migration hicbir veritabanina uygulanmadigi icin
`20261017120000_v2_character_store.sql` ayni dosyada duzeltildi (yeni migration dosyasi ACILMADI). Bir veritabanina uygulandiktan SONRA bu dosya degistirilmez;
her degisiklik yeni zaman damgali dosyayla gelir. Degisenler: `lease_epoch` tek ad, `get_op.found`, `p_op_id` kapsami, `purge_write_ops`, varsayilan yetkiler,
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

Tasima: `POST {url}/rest/v1/rpc/{fonksiyon}`, govde = parametre adlari ile JSON nesnesi, basliklar `Content-Type: application/json`, `Content-Profile: mmo_api`, `apikey` + `Authorization: Bearer` (secret key).
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
| `purge_write_ops` | `p_older_than_days` int (7; 1..3650) | yok |

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
- `purge_write_ops`: `{outcome:'ok', deleted, more}`; `invalid/bad_days`.
- `p_data` = `FMMOCharacterSaveJson::ToJson` zarfi `{"SchemaVersion":n,"Data":{...}}`; DB yalniz tip/boyut/surum eslesmesi ve Level 1..999, Experience>=0, Gold>=0, NextInstanceId>=1 (int32, tam sayi) denetler.

### create_character ve sonuc belirsizligi (yanit kayboldu / zaman asimi)
- Yeniden deneme **AYNI `p_op_id` + AYNI yukle** yapilir: ilk deneme commit olduysa kayitli sonuc `replayed:true` ile doner (test: `create_rules`, `create_ambiguous_result`).
- **Yeni** op_id ile tekrar edilirse ve ilk deneme commit olduysa `already_exists` doner: bu, "baska biri ayni adi aldi" durumundan **ayirt edilemez**. Ayirmak icin ilk op_id ile `get_op` sorulur
  (`found:true` + `op_kind:'create'` = karakteri biz olusturduk; `found:false` = baska biri ya da gunluk temizlendi).
- `not_found` (`get_op`) kesin "olmayacak" demek degildir (R1, asagida). `purge_write_ops` ile silinen (>=7 gun) op_id de `found:false` doner.

### write_ops temizligi ve pg_cron (BURADA CALISTIRILMADI)
`mmo_api.purge_write_ops(p_older_than_days default 7)`: yalniz `service_role`; en az 1 gun, en cok 3650; tek cagrida en cok **5000** satir (en eskiden), kalan varsa `more:true`.
Yalniz `mmo.write_ops` satirlari silinir. Saklama suresi istemcinin AYNI op_id ile yeniden deneme ufkundan uzun olmali. Zamanlama (Supabase'de `pg_cron` eklentisi etkinlestirilirse, kullanici elle):
```sql
select cron.schedule('mmo-purge-write-ops', '17 3 * * *', 'select mmo_api.purge_write_ops(7)');
```
`more:true` doner ise oyun yoneticisi ayni cagriyi tekrarlar (ya da cron gunde birkac kez calisir).

## Kullanicinin dev projede calistiracagi sira
Oncesi: `docs/DESIGN_SUPABASE.md` 6. bolum Dilim 0 (hesap, **dev** projesi, veritabani sifresi parola yoneticisinde; sifreyi/anahtari sohbete YAPISTIRMA).
1. Baglanti dizesi (dev projesi, Dashboard -> Connect) `DATABASE_URL` ortam degiskeni olarak (yalniz kendi makinende; repoya yazma).
2. `psql "$DATABASE_URL" -c "select 1"`  -> `1` (baglanti canli).
3. Migration (iki yoldan biri):
   - psql: `psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -f supabase/migrations/20261017120000_v2_character_store.sql`
   - CLI: `supabase link --project-ref <ref>` ardindan `supabase db push` (once `supabase init`; olusan `supabase/config.toml` icinde `[api] schemas = ["mmo_api"]` olsun).
4. Dashboard -> Data API -> **Exposed schemas**: yalniz `mmo_api` (`public` ve `mmo` YOK). Bu psql ile yapilamaz, elle.
5. `psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -f supabase/tests/run_all.sql` (depo kokunden, Git Bash; psql >= 13). Her dosya `PASS ...` yazmali.
6. Eszamanlilik (iki terminal): `05_concurrency_a` + `-v mode=same_op` `05_concurrency_b` + `05_concurrency_check.sql`; sonra `other_op`; sonra `05_concurrency_lease_a` + `05_concurrency_lease_b` (basliklarda ayrinti).
7. Uc uca HTTP denetimi (secret key yalniz ortam degiskeninden): `curl -s -X POST "$MMO_SUPABASE_URL/rest/v1/rpc/healthcheck" -H "apikey: $MMO_SUPABASE_SECRET_KEY" -H "Authorization: Bearer $MMO_SUPABASE_SECRET_KEY" -H "Content-Type: application/json" -H "Content-Profile: mmo_api" -d "{}"` -> `{"outcome":"ok","contract_version":2,...}`. Publishable (anon) anahtarla ayni cagri **reddedilmeli**, `GET /rest/v1/character_saves` de veri DONMEMELI. `acquire_lease` cagrisina `p_op_id` eklenirse `404 PGRST202` gelmeli.
Duz yerel PG (Docker'siz kurulum) ile denemek istersen once `tests/00_local_roles.sql`, sonra migration, sonra testler.

## Sapmalar (belge -> bu uygulama)
1. **`write_ops`** adi korundu (belge 3.2); gorev metnindeki `op_log` ayni sey. `op_kind` yalniz `create`/`save`.
2. **FORCE RLS yok**, `ENABLE` var: SECURITY DEFINER fonksiyonlar tablo sahibi olarak calisir; FORCE + sahibin BYPASSRLS'siz olmasi fonksiyonlari sessizce 0 satira mahkum ederdi. Asil koruma: tablolarda hicbir rolun (service_role dahil) yetkisi yok + `mmo` API'ye acik degil. (Belge: FORCE.)
3. **`save_character` SECURITY DEFINER** (belge taslagi `security invoker`): invoker olsaydi `service_role`a tablo yetkisi vermek gerekirdi. Tum fonksiyonlarda `set search_path = ''`, dinamik SQL yok (`04` kaba bekci).
4. **`acquire_lease`'te `p_op_id` yok** (belge var): bkz. yukaridaki sozlesme tablosu. Belge 3.3 bu sozlesmeyle guncellendi.
5. **Kira suresi dolmus + ayni sahip tekrar `acquire_lease`**: epoch +1 (belge yalniz "ayni sahip -> artmaz" diyordu; gecerli kirada artmaz, dolmus/birakilmis kirada artar: olu istek karsi onlemi).
6. **Ek sutunlar:** `character_saves.lease_ttl_s` (acquire/renew/save ayni sureyle uzatir), `import_sha256` (ice aktarma idempotency'si). `character_id` degismez, `canonical_key` degismez, `revision`/`lease_epoch` geri gitmez (tetikleyici).
7. **Boyut tavani 2 MiB** (belge 1 MiB [Tahmin]): UE `MaxJsonChars` = 2 MiB, daha dusuk tavan gecerli bir kaydi reddederdi. Yan veri tavani 256 KiB (belge ile ayni).
8. **Eklenenler:** `load_character`, `healthcheck`, `character_exists`, `list_keys`, `renew_lease`, `create_character`, `purge_write_ops` (belge listesinde vardi; `load` gorev metninde). `list_keys` onek aramasi `starts_with` (LIKE degil: `_` joker olurdu).
9. **Ad kurali SQL'de katilastirildi:** bas/son bosluk, 32 karakter ustu ve her ASCII disi karakter `bad_name` ile reddedilir (UE bunlari kirpar/keser: bkz. `vectors.json` `Omer`, `Ali!`). Bu bilincli: kirpma baskasinin kaydini acardi.
10. **JSON invariantlari:** `Level 1..999`, `Experience/Gold >= 0`, `NextInstanceId >= 1`, `SchemaVersion` sutunla ayni. Envanter/esya icerigi DB'de denetlenmez (UE `FromJson` sorumlulugu). **Denetim CHECK degil tetikleyicidir** (`character_saves_envelope_ins_trg` / `_upd_trg`): CHECK her UPDATE'te (kira yenileme dahil) 2 MiB'a kadar jsonb'yi yeniden metne cevirirdi; tetikleyici yalniz `data`/`schema_version` DEGISINCE calisir. SQLSTATE hala `23514`. Bedel: veri hic degismeyen eski bozuk satir kira yenilemede yakalanmaz (kasitli; 03'te kanitli).
11. **`purge_write_ops` VAR** (QA/guvenlik turu 1; onceki surumde bilincli yoktu): yalniz `service_role`, en az 1 gun, parti tavani 5000, yalniz `write_ops` silinir; kayit silme yetenegi anahtarla ACILMAZ.
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
