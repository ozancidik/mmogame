-- =====================================================================================================
-- V2 karakter deposu (Supabase / PostgreSQL). Tasarim: docs/DESIGN_SUPABASE.md (3.2 sema, 3.3 RPC, 3.4 idempotency).
--
-- DURUM: YAZILDI, HIC CALISTIRILMADI (makinede psql/docker/supabase CLI yok). Ilk uygulama dev projede, ardindan
-- supabase/tests/run_all.sql ile dogrulanir. Bkz. supabase/README.md.
-- Bu dosya HENUZ HICBIR veritabanina uygulanmadi; 2026-10-17 QA/guvenlik turu bulgulari (lease_epoch adi, get_op.found,
-- purge_write_ops, varsayilan yetkiler, kilit sonrasi zaman, envelope tetikleyicisi, sozlesme surumu 2) YERINDE duzeltildi
-- (yeni migration dosyasi acilmadi). Bir veritabanina uygulandiktan SONRA bu dosya degistirilmez; yeni dosya acilir.
--
-- GECICI VARSAYIMLAR (kullanici henuz onaylamadi, geri alinabilir):
--   * Mimari C: Supabase = barindirilan PostgreSQL + PostgREST RPC. Auth / Realtime / Storage kullanilmaz.
--   * Tablolar ozel `mmo` semasinda; bu sema API'ye ACIK DEGIL (Dashboard "exposed schemas" listesine EKLENMEZ).
--   * RPC fonksiyonlari `mmo_api` semasinda; yalniz service_role EXECUTE eder (anon / authenticated / public REVOKE).
--   * Tablolarda RLS acik ve politikasiz. Fonksiyonlar SECURITY DEFINER (sahibi migration rolu); service_role
--     tablolara DOGRUDAN yetkili degil, yalniz fonksiyonlar uzerinden erisir.
--   * Ad kurali = MMONameRules::CanonicalKey (Source/MyProject/Public/Core/MMONameRules.h): bosluk -> '_',
--     ASCII kucuk harf. UE'den FARKLI olarak SQL tarafi ASCII disi / izinsiz karakterli adi REDDEDER (kirpmaz).
--   * Migration V1 (Migrations/V1__initial_schema.sql) UYGULANMAZ; bu dosya tek kaynaktir.
--
-- Belgeden bilincli sapmalar (ayrinti: supabase/README.md "Sapmalar"):
--   write_ops adi korundu (gorev metnindeki op_log ayni sey); character_saves'e lease_ttl_s ve import_sha256 eklendi;
--   FORCE RLS yerine ENABLE RLS; save_character SECURITY DEFINER (belgede invoker); acquire_lease'te p_op_id yok;
--   veri boyut tavani 2 MiB (UE MaxJsonChars ile ayni; belgede 1 MiB tahmindi); load_character / healthcheck eklendi;
--   purge_write_ops eklendi (yalniz service_role, en az 1 gun, parti p_batch 1..50000, varsayilan 5000).
--
-- SOZLESME (contract_version 2): kira donemi alani TEK ad `lease_epoch`. Eski `epoch` alani geriye donuk uyumluluk icin
--   yanitlarda KALIR (ayni deger); yeni istemci yalniz `lease_epoch` okur. p_op_id YALNIZ create_character, save_character
--   ve get_op'ta vardir (acquire_lease / renew_lease / release_lease / character_exists / import_character'da YOK).
-- =====================================================================================================

begin;

create schema if not exists mmo;
create schema if not exists mmo_api;
revoke all on schema mmo from public, anon, authenticated, service_role;
revoke all on schema mmo_api from public, anon, authenticated;
grant usage on schema mmo_api to service_role;

-- Gelecek migration'lar icin: bu iki semada SEMA DUZEYINDE varsayilan yetkiler fonksiyonlarda EXECUTE vermez.
-- DURUST SINIR: PostgreSQL'de sema duzeyindeki varsayilan yetkiler KUMELIDIR (global varsayilanlara eklenir); bu REVOKE,
-- Supabase'in ya da PostgreSQL'in yerlesik/GLOBAL varsayilan EXECUTE'unu (yeni fonksiyonda PUBLIC'e EXECUTE) kaldirmaz.
-- Gercek koruma: her migration sonunda 6. bolumdeki revoke/grant dongusu + supabase/tests/04_security.sql (katalogdaki HER
-- fonksiyonu denetler). Rol-genisligindeki (tum semalar) kati hali sahip karari: asagidaki satir acilirsa migration rolunun
-- olusturdugu HER yeni fonksiyon PUBLIC'e kapali olur (Supabase public semasini etkileyebilir, bu yuzden KAPALI birakildi):
--   alter default privileges revoke execute on functions from public;
alter default privileges in schema mmo_api revoke execute on functions from public, anon, authenticated;
alter default privileges in schema mmo revoke execute on functions from public, anon, authenticated, service_role;

-- -----------------------------------------------------------------------------------------------------
-- 1) Yardimci fonksiyonlar (mmo semasi, hicbir rol dogrudan cagiramaz; yalniz sahip-hakli fonksiyon/CHECK kullanir)
-- -----------------------------------------------------------------------------------------------------

-- Kanonik anahtar. NULL = ad gecersiz (bos, 32 karakterden uzun, ASCII disi/izinsiz karakter, bas/son bosluk).
-- Izinli: [A-Za-z0-9 _-]; bas ve son karakter bosluk olamaz. UE SlotKey().ToLower() ile ayni sonucu verir
-- (kontrat vektorleri: supabase/tests/vectors.json), farki: UE gecersiz karakteri atar, burada ad REDDEDILIR.
create function mmo.canonical_key(p_name text) returns text
language sql immutable set search_path = '' as $$
  select case
    when p_name is null then null
    when p_name !~ '^[A-Za-z0-9_-]([A-Za-z0-9 _-]{0,30}[A-Za-z0-9_-])?$' then null
    else lower(replace(p_name, ' ', '_'))
  end
$$;

create function mmo.valid_key(p_key text) returns boolean
language sql immutable set search_path = '' as $$
  select coalesce(p_key ~ '^[a-z0-9_-]{1,32}$', false)
$$;

-- MMONameRules::IsReservedName: "default" ve "player" + yalniz rakam.
create function mmo.is_reserved_key(p_key text) returns boolean
language sql immutable set search_path = '' as $$
  select coalesce(p_key = 'default' or p_key ~ '^player[0-9]+$', false)
$$;

create function mmo.valid_holder(p_holder text) returns boolean
language sql immutable set search_path = '' as $$
  select coalesce(p_holder ~ '^[A-Za-z0-9_.:-]{1,96}$', false)
$$;

-- Ad sorunu: NULL = sorun yok. create_character ve import_character ortak kullanir.
create function mmo.name_problem(p_key text, p_name text) returns text
language sql immutable set search_path = '' as $$
  select case
    when p_key is null or p_name is null then 'bad_name'
    when mmo.canonical_key(p_name) is null then 'bad_name'
    when mmo.canonical_key(p_name) <> p_key then 'key_mismatch'
    when mmo.is_reserved_key(p_key) then 'reserved_name'
    else null
  end
$$;

-- JSON sayisi tam sayi ve [lo, hi] araliginda mi. Tip yanlis / eksik / NULL ise false (asla hata firlatmaz).
create function mmo.json_int_in_range(p_val jsonb, p_lo numeric, p_hi numeric) returns boolean
language sql immutable set search_path = '' as $$
  select case
    when jsonb_typeof(p_val) = 'number'
      then coalesce(
             ((p_val #>> '{}')::numeric between p_lo and p_hi)
             and ((p_val #>> '{}')::numeric = trunc((p_val #>> '{}')::numeric)),
             false)
    else false
  end
$$;

-- Karakter zarfi (FMMOCharacterSaveJson::ToJson ciktisi: {"SchemaVersion":n,"Data":{...}}) sorunu. NULL = sorun yok.
-- Yalnis JSON icinde denetlenebilen invariantlar: tip, boyut, surum eslesmesi, Level/Experience/Gold/NextInstanceId.
-- (Gold, Experience, Level, NextInstanceId UE tarafinda int32 = JSON sayisi.)
-- BOYUT OLCUMU: tavan `octet_length(jsonb::text)` ile olculur. jsonb'nin metin cikisi ANAHTAR/DEGER ayiraclarindan sonra bosluk
-- koyar ({"a": 1, "b": [1, 2]}), yani UE'nin yogun JSON'undan (bosluksuz) UZUNDUR; ayrica olcu bayt'tir, karakter degil (ASCII disi
-- UTF-8'de >1 bayt). UE tarafi 2 MiB'a yakin kayitlarda DB'nin reddedebilecegini bilmeli (bkz. vectors.json limits.data_octet_limit_note).
create function mmo.envelope_problem(p_data jsonb, p_schema_version int) returns text
language sql immutable set search_path = '' as $$
  select case
    when p_data is null or jsonb_typeof(p_data) <> 'object' then 'data_not_object'
    when octet_length(p_data::text) > 2097152 then 'data_too_large'
    when not mmo.json_int_in_range(p_data -> 'SchemaVersion', p_schema_version, p_schema_version) then 'schema_version_mismatch'
    when jsonb_typeof(p_data -> 'Data') is distinct from 'object' then 'data_missing'
    when not mmo.json_int_in_range(p_data #> '{Data,Level}', 1, 999) then 'bad_level'
    when not mmo.json_int_in_range(p_data #> '{Data,Experience}', 0, 2147483647) then 'bad_experience'
    when not mmo.json_int_in_range(p_data #> '{Data,Gold}', 0, 2147483647) then 'bad_gold'
    when not mmo.json_int_in_range(p_data #> '{Data,NextInstanceId}', 1, 2147483647) then 'bad_next_instance_id'
    else null
  end
$$;

-- Yan veri ({"achievements":{...},"daily":{...},"mail":{...}}) sorunu. NULL = sorun yok (p_side NULL da gecerli).
create function mmo.side_problem(p_side jsonb) returns text
language sql immutable set search_path = '' as $$
  select case
    when p_side is null then null
    when jsonb_typeof(p_side) <> 'object' then 'side_not_object'
    when exists (select 1 from jsonb_each(p_side) e where e.key not in ('achievements', 'daily', 'mail')) then 'side_unknown_kind'
    when exists (select 1 from jsonb_each(p_side) e
                  where jsonb_typeof(e.value) <> 'object' or octet_length(e.value::text) > 262144) then 'side_bad_value'
    else null
  end
$$;

-- Yuk ozeti: bir mantiksal islemin TUM etkili girdilerinin JSON dizisinin SHA-256'si (kesin, belirsizlik yok).
create function mmo.payload_digest(p_parts jsonb) returns text
language sql immutable set search_path = '' as $$
  select encode(sha256(convert_to(p_parts::text, 'utf8')), 'hex')
$$;

-- op_id basina islem kilidi: ayni op_id'nin es zamanli kopyalari (ve get_op) sirayla gecer.
-- Kilit sirasi HER YERDE: once op_id advisory kilidi, sonra karakter satiri (FOR UPDATE). Dongu yok.
create function mmo.op_lock(p_op_id uuid) returns void
language sql set search_path = '' as $$
  select pg_advisory_xact_lock(hashtextextended('mmo.op:' || p_op_id::text, 0))
$$;

-- Standart hata/sonuc govdesi.
create function mmo.err(p_outcome text, p_reason text default null, p_extra jsonb default null) returns jsonb
language sql immutable set search_path = '' as $$
  select jsonb_build_object('outcome', p_outcome)
         || case when p_reason is null then '{}'::jsonb else jsonb_build_object('reason', p_reason) end
         || coalesce(p_extra, '{}'::jsonb)
$$;

-- -----------------------------------------------------------------------------------------------------
-- 2) Tablolar
-- -----------------------------------------------------------------------------------------------------

create table mmo.accounts (
  account_id    uuid primary key default gen_random_uuid(),
  provider      text not null check (provider in ('steam', 'dev')),
  external_id   text not null check (external_id ~ '^[0-9A-Za-z_-]{1,64}$'),   -- Steam: 64-bit SteamID ondalik METIN
  is_admin      boolean not null default false,
  created_at    timestamptz not null default now(),
  last_login_at timestamptz,
  unique (provider, external_id)
);

create table mmo.character_saves (
  canonical_key  text primary key,
  character_id   uuid not null unique default gen_random_uuid(),
  display_name   text not null,
  account_id     uuid references mmo.accounts(account_id),      -- NULL = sahipsiz/eski karakter (K-1 acik)
  race           text,                                          -- deger kumesi S03 kabulunden sonra CHECK olur
  schema_version int not null,
  revision       bigint not null default 0,
  data           jsonb not null,
  active_server  text,                                          -- kira tutan: '<server_id>:<boot_guid>'
  lease_epoch    bigint not null default 0,
  lease_until    timestamptz,
  lease_ttl_s    int not null default 120,
  import_sha256  text,                                          -- son ice aktarilan kaynagin ozeti (import_character)
  created_at     timestamptz not null default now(),
  updated_at     timestamptz not null default now(),
  constraint character_saves_key_format     check (canonical_key ~ '^[a-z0-9_-]{1,32}$'),
  constraint character_saves_key_reserved   check (not mmo.is_reserved_key(canonical_key)),
  constraint character_saves_display_name   check (char_length(display_name) between 1 and 32
                                                   and canonical_key is not distinct from mmo.canonical_key(display_name)),
  constraint character_saves_race           check (race is null or race ~ '^[A-Za-z0-9_-]{1,32}$'),
  constraint character_saves_schema_version check (schema_version >= 1),
  constraint character_saves_revision       check (revision >= 0),
  constraint character_saves_epoch          check (lease_epoch >= 0),
  constraint character_saves_ttl            check (lease_ttl_s between 10 and 600),
  constraint character_saves_lease_pair     check ((active_server is null) = (lease_until is null)),
  constraint character_saves_holder         check (active_server is null or mmo.valid_holder(active_server)),
  constraint character_saves_import_sha     check (import_sha256 is null or import_sha256 ~ '^[0-9a-f]{64}$')
  -- Zarf (data) denetimi CHECK DEGIL, tetikleyicidir (3. bolum): CHECK her UPDATE'te (kira yenileme dahil) 2 MiB'a kadar
  -- jsonb'yi yeniden metne cevirip tarardi; tetikleyici yalniz data / schema_version DEGISINCE calisir. Hata kodu ayni (23514).
);
create index character_saves_account_idx on mmo.character_saves (account_id) where account_id is not null;
create index character_saves_lease_idx   on mmo.character_saves (lease_until) where active_server is not null;

create table mmo.character_side (       -- basarim / gunluk / (posta: S16 karari bekliyor)
  canonical_key text not null references mmo.character_saves(canonical_key) on delete cascade,
  kind          text not null check (kind in ('achievements', 'daily', 'mail')),
  data          jsonb not null check (jsonb_typeof(data) = 'object' and octet_length(data::text) <= 262144),
  revision      bigint not null default 0 check (revision >= 0),
  updated_at    timestamptz not null default now(),
  primary key (canonical_key, kind)
);

create table mmo.write_ops (            -- idempotency gunlugu (gorev metnindeki "op_log")
  op_id          uuid primary key,
  canonical_key  text not null,
  op_kind        text not null check (op_kind in ('create', 'save')),
  payload_sha256 text not null check (payload_sha256 ~ '^[0-9a-f]{64}$'),
  result         jsonb not null check (jsonb_typeof(result) = 'object'),
  created_at     timestamptz not null default now()
);
create index write_ops_created_idx on mmo.write_ops (created_at);   -- temizlik: mmo_api.purge_write_ops; saklama suresi tekrar-deneme ufkundan uzun olmali (varsayilan 7 gun [Tahmin])

create table mmo.presence (             -- A4'te kullanilir (henuz RPC yok)
  canonical_key text primary key references mmo.character_saves(canonical_key) on delete cascade,
  server_id     text not null check (char_length(server_id) between 1 and 96),
  heartbeat_at  timestamptz not null default now()
);

create table mmo.travel_tickets (       -- A3/S07'de kullanilir (henuz RPC yok); duz bilet DEGIL, ozeti saklanir
  ticket_sha256 text primary key check (ticket_sha256 ~ '^[0-9a-f]{64}$'),
  canonical_key text not null references mmo.character_saves(canonical_key) on delete cascade,
  account_id    uuid references mmo.accounts(account_id),
  from_server   text not null check (char_length(from_server) between 1 and 96),
  to_server     text not null check (char_length(to_server) between 1 and 96),
  expires_at    timestamptz not null,
  used_at       timestamptz,
  created_at    timestamptz not null default now()
);

-- -----------------------------------------------------------------------------------------------------
-- 3) Tetikleyiciler
-- -----------------------------------------------------------------------------------------------------

create function mmo.touch_updated_at() returns trigger
language plpgsql set search_path = '' as $$
begin
  new.updated_at := clock_timestamp();
  return new;
end
$$;

-- Savunma derinligi: fonksiyon hatasi olsa bile anahtar degismez, revision ve epoch geri gitmez.
create function mmo.character_saves_guard() returns trigger
language plpgsql set search_path = '' as $$
begin
  if new.canonical_key <> old.canonical_key then
    raise exception 'character_saves.canonical_key is immutable' using errcode = 'check_violation';
  end if;
  if new.character_id <> old.character_id then
    raise exception 'character_saves.character_id is immutable' using errcode = 'check_violation';
  end if;
  if new.revision < old.revision then
    raise exception 'character_saves.revision must not decrease' using errcode = 'check_violation';
  end if;
  if new.lease_epoch < old.lease_epoch then
    raise exception 'character_saves.lease_epoch must not decrease' using errcode = 'check_violation';
  end if;
  new.updated_at := clock_timestamp();
  return new;
end
$$;

create trigger character_saves_guard_trg before update on mmo.character_saves
  for each row execute function mmo.character_saves_guard();

-- Zarf denetimi (eski CHECK character_saves_envelope'un yerine). Ekleme: her zaman. Guncelleme: yalniz data ya da
-- schema_version SET listesinde VE degeri degismisse. Kira yenileme / birakma (yalniz lease_*) zarfa dokunmaz ve bu
-- denetimi tetiklemez. Hata: SQLSTATE 23514 (check_violation), kisit adi character_saves_envelope (eskisiyle ayni).
create function mmo.character_saves_envelope_guard() returns trigger
language plpgsql set search_path = '' as $$
declare
  v_problem text;
begin
  v_problem := mmo.envelope_problem(new.data, new.schema_version);
  if v_problem is not null then
    raise exception 'character_saves envelope invalid: %', v_problem
      using errcode = 'check_violation', constraint = 'character_saves_envelope';
  end if;
  return new;
end
$$;

create trigger character_saves_envelope_ins_trg before insert on mmo.character_saves
  for each row execute function mmo.character_saves_envelope_guard();
create trigger character_saves_envelope_upd_trg before update of data, schema_version on mmo.character_saves
  for each row when (old.data is distinct from new.data or old.schema_version is distinct from new.schema_version)
  execute function mmo.character_saves_envelope_guard();
create trigger character_side_touch_trg before update on mmo.character_side
  for each row execute function mmo.touch_updated_at();

-- -----------------------------------------------------------------------------------------------------
-- 4) RLS: acik, politika YOK (varsayilan reddet). FORCE kullanilmaz: SECURITY DEFINER fonksiyonlar tablo sahibi olarak
--    calisir; FORCE olsaydi ve sahip BYPASSRLS degilse fonksiyonlar sessizce 0 satir gorurdu. Asil koruma yetki geri almadir.
-- -----------------------------------------------------------------------------------------------------

do $$
declare t text;
begin
  for t in select unnest(array['accounts', 'character_saves', 'character_side', 'write_ops', 'presence', 'travel_tickets']) loop
    execute format('alter table mmo.%I enable row level security', t);
  end loop;
end
$$;

revoke all on all tables in schema mmo from public, anon, authenticated, service_role;

-- -----------------------------------------------------------------------------------------------------
-- 5) RPC (mmo_api). Hepsi jsonb doner; is sonucu `outcome` alaninda (HTTP hep 200). Dinamik SQL YOK, parametrik.
--    SECURITY DEFINER + sabit bos search_path. Is kurali ihlali istisna FIRLATMAZ; yalniz beklenmeyen hata firlatir.
-- -----------------------------------------------------------------------------------------------------

create function mmo_api.healthcheck() returns jsonb
language sql security definer set search_path = '' as $$
  select jsonb_build_object('outcome', 'ok', 'api_version', 2, 'contract_version', 2, 'schema', 'v2',
                            'server_time', clock_timestamp())
$$;

-- Salt-okunur anlik goruntu: kira ALMAZ. Oyun akisinda yukleme acquire_lease iledir; bu export/denetim icindir.
create function mmo_api.load_character(p_key text) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  r mmo.character_saves;
  v_side jsonb;
begin
  if not mmo.valid_key(p_key) then
    return mmo.err('invalid', 'bad_key');
  end if;
  select * into r from mmo.character_saves where canonical_key = p_key;
  if not found then
    return mmo.err('not_found');
  end if;
  select coalesce(jsonb_object_agg(s.kind, s.data), '{}'::jsonb) into v_side
    from mmo.character_side s where s.canonical_key = p_key;
  return jsonb_build_object(
    'outcome', 'ok', 'canonical_key', r.canonical_key, 'character_id', r.character_id,
    'display_name', r.display_name, 'race', r.race, 'schema_version', r.schema_version,
    'revision', r.revision, 'data', r.data, 'side', v_side,
    'active_server', r.active_server, 'lease_epoch', r.lease_epoch, 'lease_until', r.lease_until);
end
$$;

create function mmo_api.character_exists(p_key text) returns jsonb
language plpgsql security definer set search_path = '' as $$
begin
  if not mmo.valid_key(p_key) then
    return mmo.err('invalid', 'bad_key');
  end if;
  return jsonb_build_object('outcome', 'ok',
    'exists', exists (select 1 from mmo.character_saves c where c.canonical_key = p_key));
end
$$;

create function mmo_api.list_keys(p_prefix text default '', p_limit int default 100) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  v_keys jsonb;
begin
  if p_prefix is null then
    p_prefix := '';
  end if;
  if p_limit is null or p_limit < 1 or p_limit > 500 then
    return mmo.err('invalid', 'bad_limit');
  end if;
  if p_prefix !~ '^[a-z0-9_-]{0,32}$' then
    return mmo.err('invalid', 'bad_prefix');
  end if;
  select coalesce(jsonb_agg(s.k order by s.k), '[]'::jsonb) into v_keys
    from (select c.canonical_key as k from mmo.character_saves c
           where starts_with(c.canonical_key, p_prefix)
           order by c.canonical_key limit p_limit + 1) s;
  if jsonb_array_length(v_keys) > p_limit then
    return jsonb_build_object('outcome', 'ok', 'keys', v_keys - p_limit, 'truncated', true);
  end if;
  return jsonb_build_object('outcome', 'ok', 'keys', v_keys, 'truncated', false);
end
$$;

-- Yeni karakter. Olusturan sunucu kirayi (epoch 1) hemen tutar; ayri acquire gerekmez.
-- Ayni op_id + ayni yuk tekrari kayitli sonucu doner (replayed=true). Zaten varsa already_exists.
-- SONUC BELIRSIZLIGI (yanit kayboldu / zaman asimi): yeniden deneme MUTLAKA AYNI op_id ile yapilir -> kayitli sonuc doner.
-- Yeni op_id ile tekrar edilirse, ilk deneme commit olmussa already_exists doner ve bu, "baska biri olusturdu" ile
-- AYIRT EDILEMEZ; ayirt etmek icin ilk op_id ile get_op(found=true, op_kind=create) sorulur.
-- Zaman (kira bitisi): op_id kilidi beklendikten SONRA, insert'ten ONCE alinir (v_now := clock_timestamp() asagida). Insert unique-indeks
-- beklemesine takilirsa (ayni anahtara es zamanli create) bu zaman kullanilmaz: kaybeden already_exists doner (kira yazmaz).
create function mmo_api.create_character(p_key text, p_display_name text, p_holder text, p_op_id uuid,
                                         p_schema_version int, p_data jsonb, p_race text default null)
returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  prev mmo.write_ops;
  v_problem text;
  v_digest text;
  v_now timestamptz;
  v_rows int;
  v_res jsonb;
begin
  if p_op_id is null or p_holder is null or p_schema_version is null then
    return mmo.err('invalid', 'null_argument');
  end if;
  if not mmo.valid_holder(p_holder) then
    return mmo.err('invalid', 'bad_holder');
  end if;
  if p_race is not null and p_race !~ '^[A-Za-z0-9_-]{1,32}$' then
    return mmo.err('invalid', 'bad_race');
  end if;
  v_problem := mmo.name_problem(p_key, p_display_name);
  if v_problem is not null then
    return mmo.err('invalid', v_problem);
  end if;
  v_problem := mmo.envelope_problem(p_data, p_schema_version);
  if v_problem is not null then
    return mmo.err('invalid', v_problem);
  end if;

  perform mmo.op_lock(p_op_id);
  v_digest := mmo.payload_digest(jsonb_build_array('create', p_key, p_display_name, p_holder, p_schema_version, p_race, p_data));
  select * into prev from mmo.write_ops where op_id = p_op_id;
  if found then
    if prev.op_kind = 'create' and prev.canonical_key = p_key and prev.payload_sha256 = v_digest then
      return prev.result || jsonb_build_object('replayed', true);
    end if;
    return mmo.err('idempotency_conflict');
  end if;

  v_now := clock_timestamp();   -- op_id kilidi beklendikten SONRA
  insert into mmo.character_saves
    (canonical_key, display_name, race, schema_version, revision, data, active_server, lease_epoch, lease_until, lease_ttl_s)
  values
    (p_key, p_display_name, p_race, p_schema_version, 0, p_data, p_holder, 1, v_now + interval '120 seconds', 120)
  on conflict (canonical_key) do nothing;
  get diagnostics v_rows = row_count;
  if v_rows = 0 then
    return mmo.err('already_exists');
  end if;

  v_res := jsonb_build_object('outcome', 'committed', 'canonical_key', p_key, 'revision', 0,
                              'lease_epoch', 1, 'epoch', 1);
  insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result)
  values (p_op_id, p_key, 'create', v_digest, v_res);
  return v_res;
end
$$;

-- Kira al + yukle. Ayni sahip gecerli kirayi tekrar alirsa epoch ARTMAZ (kira uzar). Kira bitmis/birakilmis ise epoch+1.
-- Baska sahibin gecerli kirasi: ownership_lost (+ retry_after_s). op_id yok: kayitli sonuc tekrari eski veriyi dondurebilirdi;
-- ayni sahip icin cagri zaten dogal idempotent. Yanit: `lease_epoch` (asil ad) + `epoch` (geriye donuk, ayni deger).
-- Zaman (kira dolmus mu karari, yeni bitis) satir kilidi beklendikten SONRA alinir.
create function mmo_api.acquire_lease(p_key text, p_holder text, p_ttl_s int default 120) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  r mmo.character_saves;
  v_now timestamptz;
  v_epoch bigint;
  v_until timestamptz;
  v_side jsonb;
begin
  if p_holder is null or p_ttl_s is null then
    return mmo.err('invalid', 'null_argument');
  end if;
  if not mmo.valid_key(p_key) then
    return mmo.err('invalid', 'bad_key');
  end if;
  if not mmo.valid_holder(p_holder) then
    return mmo.err('invalid', 'bad_holder');
  end if;
  if p_ttl_s < 10 or p_ttl_s > 600 then
    return mmo.err('invalid', 'bad_ttl');
  end if;

  select * into r from mmo.character_saves where canonical_key = p_key for update;
  if not found then
    return mmo.err('not_found');
  end if;
  v_now := clock_timestamp();   -- FOR UPDATE beklendikten SONRA

  if r.active_server is not null and r.lease_until > v_now then
    if r.active_server <> p_holder then
      return mmo.err('ownership_lost', null,
                     jsonb_build_object('retry_after_s', ceil(extract(epoch from (r.lease_until - v_now)))::int));
    end if;
    v_epoch := r.lease_epoch;
  else
    v_epoch := r.lease_epoch + 1;
  end if;
  v_until := v_now + p_ttl_s * interval '1 second';

  update mmo.character_saves
     set active_server = p_holder, lease_epoch = v_epoch, lease_until = v_until, lease_ttl_s = p_ttl_s
   where canonical_key = p_key;

  select coalesce(jsonb_object_agg(s.kind, s.data), '{}'::jsonb) into v_side
    from mmo.character_side s where s.canonical_key = p_key;

  return jsonb_build_object(
    'outcome', 'ok', 'canonical_key', r.canonical_key, 'character_id', r.character_id,
    'display_name', r.display_name, 'race', r.race, 'schema_version', r.schema_version,
    'revision', r.revision, 'lease_epoch', v_epoch, 'epoch', v_epoch, 'lease_until', v_until,
    'data', r.data, 'side', v_side);
end
$$;

-- Yalniz kirayi uzatir. Sahip/epoch uyusmazsa ya da kira bitmisse ownership_lost (bitmis kirayi yeniden acquire_lease alir).
create function mmo_api.renew_lease(p_key text, p_holder text, p_epoch bigint, p_ttl_s int default null) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  r mmo.character_saves;
  v_now timestamptz;
  v_ttl int;
  v_until timestamptz;
begin
  if p_holder is null or p_epoch is null then
    return mmo.err('invalid', 'null_argument');
  end if;
  if not mmo.valid_key(p_key) then
    return mmo.err('invalid', 'bad_key');
  end if;
  if not mmo.valid_holder(p_holder) then
    return mmo.err('invalid', 'bad_holder');
  end if;
  if p_epoch < 1 then
    return mmo.err('invalid', 'bad_epoch');
  end if;
  if p_ttl_s is not null and (p_ttl_s < 10 or p_ttl_s > 600) then
    return mmo.err('invalid', 'bad_ttl');
  end if;

  select * into r from mmo.character_saves where canonical_key = p_key for update;
  if not found then
    return mmo.err('not_found');
  end if;
  v_now := clock_timestamp();   -- FOR UPDATE beklendikten SONRA
  if r.active_server is distinct from p_holder or r.lease_epoch <> p_epoch
     or r.lease_until is null or r.lease_until <= v_now then
    return mmo.err('ownership_lost', null, jsonb_build_object('lease_epoch', r.lease_epoch, 'epoch', r.lease_epoch));
  end if;

  v_ttl := coalesce(p_ttl_s, r.lease_ttl_s);
  v_until := v_now + v_ttl * interval '1 second';
  update mmo.character_saves set lease_until = v_until, lease_ttl_s = v_ttl where canonical_key = p_key;
  return jsonb_build_object('outcome', 'ok', 'lease_epoch', r.lease_epoch, 'epoch', r.lease_epoch, 'lease_until', v_until);
end
$$;

-- Cikista kirayi birakir (kaydi save_character ile bitirdikten sonra). Epoch korunur ve artar (sonraki acquire epoch+1).
-- Zaten birakilmis (ayni epoch) ise tekrar cagri: ok + already_released=true (zaman asimi sonrasi guvenli tekrar).
-- Yanit sekli tek tiptir: {outcome:'ok', already_released:bool, lease_epoch, epoch}.
create function mmo_api.release_lease(p_key text, p_holder text, p_epoch bigint) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  r mmo.character_saves;
begin
  if p_holder is null or p_epoch is null then
    return mmo.err('invalid', 'null_argument');
  end if;
  if not mmo.valid_key(p_key) then
    return mmo.err('invalid', 'bad_key');
  end if;
  if not mmo.valid_holder(p_holder) then
    return mmo.err('invalid', 'bad_holder');
  end if;
  if p_epoch < 1 then
    return mmo.err('invalid', 'bad_epoch');
  end if;

  select * into r from mmo.character_saves where canonical_key = p_key for update;
  if not found then
    return mmo.err('not_found');
  end if;
  if r.active_server is null and r.lease_epoch = p_epoch then
    return jsonb_build_object('outcome', 'ok', 'already_released', true, 'lease_epoch', r.lease_epoch, 'epoch', r.lease_epoch);
  end if;
  if r.active_server is distinct from p_holder or r.lease_epoch <> p_epoch then
    return mmo.err('ownership_lost', null, jsonb_build_object('lease_epoch', r.lease_epoch, 'epoch', r.lease_epoch));
  end if;

  update mmo.character_saves set active_server = null, lease_until = null where canonical_key = p_key;
  return jsonb_build_object('outcome', 'ok', 'already_released', false, 'lease_epoch', r.lease_epoch, 'epoch', r.lease_epoch);
end
$$;

-- Karsilastir-ve-yaz. Sira (onemli): 1) girdi dogrulama 2) op_id kilidi 3) kayitli islem (kira/revision'a BAKMADAN)
-- 4) satir kilidi 5) fencing (sahip, epoch, kira suresi) 6) revision karsilastirma 7) yaz + yan veri + gunluk (tek islem).
-- Reddedilen islemler gunluge yazilmaz (tekrar ayni kurali verir).
create function mmo_api.save_character(p_key text, p_holder text, p_epoch bigint, p_expected_revision bigint,
                                        p_op_id uuid, p_schema_version int, p_data jsonb, p_side jsonb default null)
returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  r mmo.character_saves;
  prev mmo.write_ops;
  v_problem text;
  v_digest text;
  v_now timestamptz;
  v_res jsonb;
  v_kind text;
  v_val jsonb;
begin
  if p_op_id is null or p_holder is null or p_epoch is null or p_expected_revision is null or p_schema_version is null then
    return mmo.err('invalid', 'null_argument');
  end if;
  if not mmo.valid_key(p_key) then
    return mmo.err('invalid', 'bad_key');
  end if;
  if not mmo.valid_holder(p_holder) then
    return mmo.err('invalid', 'bad_holder');
  end if;
  if p_epoch < 1 or p_expected_revision < 0 then
    return mmo.err('invalid', 'bad_epoch_or_revision');
  end if;
  v_problem := mmo.envelope_problem(p_data, p_schema_version);
  if v_problem is not null then
    return mmo.err('invalid', v_problem);
  end if;
  v_problem := mmo.side_problem(p_side);
  if v_problem is not null then
    return mmo.err('invalid', v_problem);
  end if;

  perform mmo.op_lock(p_op_id);
  v_digest := mmo.payload_digest(jsonb_build_array(
    'save', p_key, p_holder, p_epoch, p_expected_revision, p_schema_version, p_data, p_side));
  select * into prev from mmo.write_ops where op_id = p_op_id;
  if found then
    if prev.op_kind = 'save' and prev.canonical_key = p_key and prev.payload_sha256 = v_digest then
      return prev.result || jsonb_build_object('replayed', true);
    end if;
    return mmo.err('idempotency_conflict');
  end if;

  select * into r from mmo.character_saves where canonical_key = p_key for update;
  if not found then
    return mmo.err('not_found');
  end if;
  v_now := clock_timestamp();   -- op_id kilidi VE satir kilidi beklendikten SONRA (kira suresi karari bu zamanla)
  if r.active_server is distinct from p_holder or r.lease_epoch <> p_epoch
     or r.lease_until is null or r.lease_until <= v_now then
    return mmo.err('ownership_lost', null, jsonb_build_object('lease_epoch', r.lease_epoch, 'epoch', r.lease_epoch));
  end if;
  if r.revision <> p_expected_revision then
    return mmo.err('revision_conflict', null, jsonb_build_object('revision', r.revision));
  end if;

  update mmo.character_saves
     set data = p_data, schema_version = p_schema_version, revision = r.revision + 1,
         lease_until = v_now + r.lease_ttl_s * interval '1 second'
   where canonical_key = p_key;

  if p_side is not null then
    for v_kind, v_val in select e.key, e.value from jsonb_each(p_side) e loop
      insert into mmo.character_side as s (canonical_key, kind, data, revision)
      values (p_key, v_kind, v_val, 1)
      on conflict (canonical_key, kind) do update set data = excluded.data, revision = s.revision + 1;
    end loop;
  end if;

  v_res := jsonb_build_object('outcome', 'committed', 'revision', r.revision + 1,
                              'lease_epoch', r.lease_epoch, 'epoch', r.lease_epoch);
  insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result)
  values (p_op_id, p_key, 'save', v_digest, v_res);
  return v_res;
end
$$;

-- Belirsiz sonuc uzlastirma. Islem op_id kilidini bekler: sunucuda ucusta olan islem bitince kesin cevap verilir.
-- not_found = ISLEM COMMIT OLMADI (su an); henuz DB'ye ulasmamis gecikmis bir istek sonradan commit olabilir.
-- Guvenli devam yolu: AYNI op_id + AYNI yuk ile yeniden gondermek (idempotent), kor yeniden yazma degil.
-- Yanit: bulundu -> {outcome:'ok', found:true, ...}; yok -> {outcome:'not_found', found:false}.
-- Not: purge_write_ops ile silinmis (varsayilan 7 gunden eski) bir op_id de found=false doner.
create function mmo_api.get_op(p_op_id uuid) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  prev mmo.write_ops;
begin
  if p_op_id is null then
    return mmo.err('invalid', 'null_argument');
  end if;
  perform mmo.op_lock(p_op_id);
  select * into prev from mmo.write_ops where op_id = p_op_id;
  if not found then
    return mmo.err('not_found', null, jsonb_build_object('found', false));
  end if;
  return jsonb_build_object('outcome', 'ok', 'found', true, 'op_kind', prev.op_kind, 'canonical_key', prev.canonical_key,
                            'result', prev.result, 'created_at', prev.created_at);
end
$$;

-- Idempotent ice aktarma (kaynak .sav/JSON ozeti ile). ASLA mevcut oynanmis kaydi ezmez.
--   yok                                   -> imported
--   ayni ozet                             -> already_imported
--   farkli ozet, onceki ice aktarma, hic kaydedilmemis (revision 0), kira yok/bitmis -> imported (replaced_import)
--   diger her durum                       -> conflict (rapor edilir, ezilmez)
create function mmo_api.import_character(p_key text, p_display_name text, p_source_sha256 text,
                                         p_schema_version int, p_data jsonb, p_race text default null)
returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  r mmo.character_saves;
  v_problem text;
  v_now timestamptz;
  v_rows int;
begin
  if p_source_sha256 is null or p_schema_version is null then
    return mmo.err('invalid', 'null_argument');
  end if;
  if p_source_sha256 !~ '^[0-9a-f]{64}$' then
    return mmo.err('invalid', 'bad_sha');
  end if;
  if p_race is not null and p_race !~ '^[A-Za-z0-9_-]{1,32}$' then
    return mmo.err('invalid', 'bad_race');
  end if;
  v_problem := mmo.name_problem(p_key, p_display_name);
  if v_problem is not null then
    return mmo.err('invalid', v_problem);
  end if;
  v_problem := mmo.envelope_problem(p_data, p_schema_version);
  if v_problem is not null then
    return mmo.err('invalid', v_problem);
  end if;

  insert into mmo.character_saves
    (canonical_key, display_name, race, schema_version, revision, data, import_sha256)
  values
    (p_key, p_display_name, p_race, p_schema_version, 0, p_data, p_source_sha256)
  on conflict (canonical_key) do nothing;
  get diagnostics v_rows = row_count;
  if v_rows = 1 then
    return jsonb_build_object('outcome', 'imported', 'revision', 0, 'replaced_import', false);
  end if;

  select * into r from mmo.character_saves where canonical_key = p_key for update;
  if not found then
    return mmo.err('internal', 'row_vanished');
  end if;
  v_now := clock_timestamp();   -- FOR UPDATE beklendikten SONRA (kira bitmis mi karari)
  if r.import_sha256 is not distinct from p_source_sha256 then
    return jsonb_build_object('outcome', 'already_imported', 'revision', r.revision);
  end if;
  if r.import_sha256 is not null and r.revision = 0
     and (r.active_server is null or r.lease_until <= v_now) then
    update mmo.character_saves
       set display_name = p_display_name, race = p_race, schema_version = p_schema_version,
           data = p_data, import_sha256 = p_source_sha256
     where canonical_key = p_key;
    return jsonb_build_object('outcome', 'imported', 'revision', 0, 'replaced_import', true);
  end if;
  return mmo.err('conflict', null, jsonb_build_object('revision', r.revision));
end
$$;

-- Islem gunlugu (write_ops) temizligi. YALNIZ service_role (bkz. 6. bolum); anon/authenticated cagiramaz.
-- Sinirlar: p_older_than_days 1..3650 (varsayilan 7; son 24 saatin islemleri ASLA silinmez); p_batch 1..50000 (varsayilan 5000),
-- aralik disi = invalid/bad_batch. Tek cagrida EN FAZLA p_batch satir silinir (en eskiden baslayarak); kalan varsa `more:true`.
-- Fonksiyon DONGU YAPMAZ: bir cagri = bir islem = kisa kilit. Cagiran `more:true` oldukca YENIDEN cagirir; her cagri AYRI islem
-- olmalidir (tek BEGIN icinde dongu uzun kilit tutar; README cron ornegi her cagriyi ayri cron isi/COMMIT olarak calistirir).
-- Sadece write_ops satirlari silinir (karakter / yan veri / kira dokunulmaz). Saklama suresi, istemcinin AYNI op_id ile yeniden
-- deneme ufkundan UZUN olmali: silinen op_id ile gelen gec tekrar "yeni islem" gibi islenir
-- (save: revision/epoch denetimi yine korur; create: already_exists).
-- Zamanlama ornegi (pg_cron; BURADA CALISTIRILMAZ; ayrinti supabase/README.md):
--   select cron.schedule('mmo-purge-write-ops', '*/5 * * * *', 'select mmo_api.purge_write_ops(7, 20000)');
create function mmo_api.purge_write_ops(p_older_than_days int default 7, p_batch int default 5000) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare
  v_cutoff timestamptz;
  v_deleted int;
  v_more boolean;
begin
  if p_older_than_days is null or p_older_than_days < 1 or p_older_than_days > 3650 then
    return mmo.err('invalid', 'bad_days');
  end if;
  if p_batch is null or p_batch < 1 or p_batch > 50000 then
    return mmo.err('invalid', 'bad_batch');
  end if;
  v_cutoff := clock_timestamp() - p_older_than_days * interval '1 day';
  with doomed as (
    delete from mmo.write_ops w
     where w.op_id in (select o.op_id from mmo.write_ops o where o.created_at < v_cutoff order by o.created_at limit p_batch)
    returning 1
  )
  select count(*)::int into v_deleted from doomed;
  select exists (select 1 from mmo.write_ops w where w.created_at < v_cutoff) into v_more;
  return jsonb_build_object('outcome', 'ok', 'deleted', v_deleted, 'more', v_more);
end
$$;

-- -----------------------------------------------------------------------------------------------------
-- 6) Fonksiyon yetkileri. PostgreSQL varsayilani EXECUTE'u PUBLIC'e verir: her fonksiyondan acikca geri alinir,
--    sonra yalniz mmo_api fonksiyonlari service_role'e acilir. mmo.* yardimcilari hicbir role acik degildir.
--    GELECEK MIGRATION'LAR: yeni fonksiyon eklerlerse bu dongunun AYNISINI (revoke all ... + grant execute) dosya sonunda
--    tekrarlamalidir; 04_security.sql katalogdaki her fonksiyonu denetler ve unutulani yakalar.
-- -----------------------------------------------------------------------------------------------------

do $$
declare f record;
begin
  for f in
    select p.oid::regprocedure::text as sig
      from pg_catalog.pg_proc p
      join pg_catalog.pg_namespace n on n.oid = p.pronamespace
     where n.nspname in ('mmo', 'mmo_api')
  loop
    execute format('revoke all on function %s from public, anon, authenticated, service_role', f.sig);
  end loop;
end
$$;

grant execute on all functions in schema mmo_api to service_role;

commit;
