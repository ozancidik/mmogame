# supabase/ - V2 karakter deposu (Dilim 1, SQL)

**Durum: YAZILDI, HICBIR PostgreSQL'DE CALISTIRILMADI.** Makinede psql / docker / supabase CLI yok; SQL soz dizimi bile bir ayristiriciyla dogrulanmadi
(yalniz `vectors.json` bir Python betigiyle denetlendi, asagida). Ilk gercek kosu kullanicinin **dev** projesindedir; hata cikarsa once burada duzeltilir.
Tasarim: `docs/DESIGN_SUPABASE.md` (3.2 sema, 3.3 RPC, 3.4 idempotency, 3.6 vektorler, 5 guvenlik, 7.2 Dilim 1).

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
| `tests/vectors.json` | Kontrat vektorleri: ad -> anahtar (UE + DB), 16 RPC senaryosu, fixture'lar, limitler, RPC parametre adlari. UE native testi de **bu dosyayi** okuyacak. |
| `tests/00_local_roles.sql` | Yalniz duz (Supabase olmayan) PG: anon/authenticated/service_role yoksa olusturur. |
| `tests/01_canonical_key.sql` | Anahtar vektorleri + dogrulayici birimleri. |
| `tests/02_rpc_scenarios.sql` | `vectors.json` senaryolarini service_role olarak calistiran yorumlayici (eski revision, olu epoch, cift op_id ayni/farkli yuk, kira, ice aktarma...). |
| `tests/03_constraints.sql` | CHECK/FK/UNIQUE, tetikleyiciler (revision/epoch geri gitmez), cascade, boyut tavani. |
| `tests/04_security.sql` | RLS acik+politikasiz, anon/authenticated tabloya ve RPC'ye erisemez, service_role tabloya dogrudan erisemez, `search_path`, dinamik SQL bekcisi. |
| `tests/05_concurrency_a.sql`, `_b.sql`, `_check.sql` | Esozamanli iki islem (op_id kilidi + satir kilidi), IKI terminal ister, kalici veri yazar (yalniz dev). |
| `tests/run_all.sql` | 01-04'u tek komutla kosar (hepsi ROLLBACK; iz birakmaz). |

## RPC imzalari (`mmo_api`, hepsi `jsonb` doner; HTTP 200 + `outcome`)
- `healthcheck()`
- `load_character(p_key)` salt-okunur goruntu, kira ALMAZ (oyun akisi `acquire_lease`)
- `character_exists(p_key)`, `list_keys(p_prefix default '', p_limit default 100)` (en cok 500)
- `create_character(p_key, p_display_name, p_holder, p_op_id uuid, p_schema_version int, p_data jsonb, p_race default null)` kirayi (epoch 1) olusturan tutar
- `acquire_lease(p_key, p_holder, p_ttl_s default 120)` kira al + veri/yan veri yukle; ayni sahip gecerli kirayi alirsa epoch ayni, aksi epoch+1
- `renew_lease(p_key, p_holder, p_epoch, p_ttl_s default null)`, `release_lease(p_key, p_holder, p_epoch)` (tekrar cagri `already_released`)
- `save_character(p_key, p_holder, p_epoch, p_expected_revision, p_op_id, p_schema_version, p_data, p_side default null)` karsilastir-ve-yaz, yan veri ayni islemde
- `get_op(p_op_id)` belirsiz sonuc uzlastirma (op kilidini bekler); `not_found` = su an commit yok, guvenli devam = AYNI op_id + AYNI yukle tekrar
- `import_character(p_key, p_display_name, p_source_sha256, p_schema_version, p_data, p_race default null)` idempotent, oynanmis kaydi ezmez
`outcome`: ok, committed, imported, already_imported, not_found, already_exists, invalid(+reason), revision_conflict, ownership_lost, idempotency_conflict, conflict, internal.
`p_data` = `FMMOCharacterSaveJson::ToJson` zarfi `{"SchemaVersion":n,"Data":{...}}`; DB yalniz tip/boyut/surum eslesmesi ve Level 1..999, Experience>=0, Gold>=0, NextInstanceId>=1 (int32, tam sayi) denetler.

## Kullanicinin dev projede calistiracagi sira
Oncesi: `docs/DESIGN_SUPABASE.md` 6. bolum Dilim 0 (hesap, **dev** projesi, veritabani sifresi parola yoneticisinde; sifreyi/anahtari sohbete YAPISTIRMA).
1. Baglanti dizesi (dev projesi, Dashboard -> Connect) `DATABASE_URL` ortam degiskeni olarak (yalniz kendi makinende; repoya yazma).
2. `psql "$DATABASE_URL" -c "select 1"`  -> `1` (baglanti canli).
3. Migration (iki yoldan biri):
   - psql: `psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -f supabase/migrations/20261017120000_v2_character_store.sql`
   - CLI: `supabase link --project-ref <ref>` ardindan `supabase db push` (once `supabase init`; olusan `supabase/config.toml` icinde `[api] schemas = ["mmo_api"]` olsun).
4. Dashboard -> Data API -> **Exposed schemas**: yalniz `mmo_api` (`public` ve `mmo` YOK). Bu psql ile yapilamaz, elle.
5. `psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -f supabase/tests/run_all.sql` (depo kokunden, Git Bash; psql >= 13). Her dosya `PASS ...` yazmali.
6. Eszamanlilik (iki terminal, ayrinti `tests/05_concurrency_a.sql` basligi): A + `-v mode=same_op` B + `05_concurrency_check.sql`; sonra A + `-v mode=other_op` B + check.
7. Uc uca HTTP denetimi (secret key yalniz ortam degiskeninden): `curl -s -X POST "$MMO_SUPABASE_URL/rest/v1/rpc/healthcheck" -H "apikey: $MMO_SUPABASE_SECRET_KEY" -H "Authorization: Bearer $MMO_SUPABASE_SECRET_KEY" -H "Content-Type: application/json" -H "Content-Profile: mmo_api" -d "{}"` -> `{"outcome":"ok",...}`. Publishable (anon) anahtarla ayni cagri **reddedilmeli**, `GET /rest/v1/character_saves` de veri DONMEMELI.
Duz yerel PG (Docker'siz kurulum) ile denemek istersen once `tests/00_local_roles.sql`, sonra migration, sonra testler.

## Sapmalar (belge -> bu uygulama)
1. **`write_ops`** adi korundu (belge 3.2); gorev metnindeki `op_log` ayni sey. `op_kind` yalniz `create`/`save`.
2. **FORCE RLS yok**, `ENABLE` var: SECURITY DEFINER fonksiyonlar tablo sahibi olarak calisir; FORCE + sahibin BYPASSRLS'siz olmasi fonksiyonlari sessizce 0 satira mahkum ederdi. Asil koruma: tablolarda hicbir rolun (service_role dahil) yetkisi yok + `mmo` API'ye acik degil. (Belge: FORCE.)
3. **`save_character` SECURITY DEFINER** (belge taslagi `security invoker`): invoker olsaydi `service_role`a tablo yetkisi vermek gerekirdi. Tum fonksiyonlarda `set search_path = ''`, dinamik SQL yok (`04` kaba bekci).
4. **`acquire_lease`'te `p_op_id` yok** (belge var): kayitli sonuc tekrari eski veriyi dondurebilirdi (arada kayit yapilmis olabilir). Ayni sahip icin cagri zaten dogal idempotent.
5. **Kira suresi dolmus + ayni sahip tekrar `acquire_lease`**: epoch +1 (belge yalniz "ayni sahip -> artmaz" diyordu; gecerli kirada artmaz, dolmus/birakilmis kirada artar: olu istek karsi onlemi).
6. **Ek sutunlar:** `character_saves.lease_ttl_s` (acquire/renew/save ayni sureyle uzatir), `import_sha256` (ice aktarma idempotency'si). `character_id` degismez, `canonical_key` degismez, `revision`/`lease_epoch` geri gitmez (tetikleyici).
7. **Boyut tavani 2 MiB** (belge 1 MiB [Tahmin]): UE `MaxJsonChars` = 2 MiB, daha dusuk tavan gecerli bir kaydi reddederdi. Yan veri tavani 256 KiB (belge ile ayni).
8. **Eklenenler:** `load_character`, `healthcheck`, `character_exists`, `list_keys`, `renew_lease`, `create_character` (belge listesinde vardi; `load` gorev metninde). `list_keys` onek aramasi `starts_with` (LIKE degil: `_` joker olurdu).
9. **Ad kurali SQL'de katilastirildi:** bas/son bosluk, 32 karakter ustu ve her ASCII disi karakter `bad_name` ile reddedilir (UE bunlari kirpar/keser: bkz. `vectors.json` `Omer`, `Ali!`). Bu bilincli: kirpma baskasinin kaydini acardi.
10. **JSON invariantlari:** CHECK, `Level 1..999`, `Experience/Gold >= 0`, `NextInstanceId >= 1`, `SchemaVersion` sutunla ayni. Envanter/esya icerigi DB'de denetlenmez (UE `FromJson` sorumlulugu).
11. `purge_write_ops` YOK (islem gunlugu temizligi); kayit silme yetenegi anahtarla acilmasin diye bilincli eklenmedi. Saklama suresi (7 gun [Tahmin]) ve temizlik karari sonraki dilim.

## Bilinen kalan riskler / acik noktalar
- **R1 daralir, kapanmaz:** devirden ONCE gonderilip zaman asimina ugrayan ve DB'ye gec ulasan yazim, hala epoch/revision gecerliyse commit olabilir. Istemci `get_op` ile uzlastirir; `not_found` kesin "olmayacak" demek degildir (bkz. `get_op` aciklamasi).
- Presence ve travel_tickets tablolari var, RPC'si yok (A3/A4). `accounts`/`account_id` kimlik cozmez (K-1 acik: `canonical_key`'i bilen herkes karakteri acar; Steam/A5 gelmeden degismez).
- Irk degeri (`race`) yalniz bicim denetimli; deger kumesi S03 kabulunden sonra CHECK olacak.
- Yan veri ice aktarma (basarim/gunluk/posta) `import_character`'de yok (Dilim 5/7).
- `secret key` harita sunucusunda ortam degiskeni (mimari C'nin bedeli, belge 2 ve 5.1).

## Soz dizimi dogrulamasi (durustce)
- SQL: **calistirilmadi, ayristirilmadi.** `pglast`/`sqlglot` kurulu degil; pip kurulumu sistem degisikligi sayildigi icin yapilmadi. Kod elle iki kez gozden gecirildi (tum nesne adlari niteliklendirilmis, `search_path=''`, SQL fonksiyonlari tanimlanmadan once kullanilmiyor).
- `vectors.json`: Python `json` ile ayristirildi, ASCII, tum fixture/RPC/parametre adlari ve `outcome` degerleri yerel bir betikle capraz denetlendi, 34 anahtar vektoru `MMONameRules` ve SQL regex'inin Python karsiliklariyla eslesti (SQL'in kendisiyle degil).
