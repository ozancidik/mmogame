-- 04: yetki / RLS / fonksiyon guvenligi. anon ve authenticated hicbir tabloya ya da RPC'ye erisemez;
-- service_role yalniz mmo_api fonksiyonlarini cagirabilir (tablolara dogrudan YETKISI YOK, en az ayricalik).
-- RLS ikinci savunma hatti: yanlislikla GRANT verilse bile politikasiz RLS satir gostermez.
-- Kullanim (depo kokunden):  psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -f supabase/tests/04_security.sql
-- Roller (anon, authenticated, service_role) onceden var olmali: Supabase'de var, duz PG'de 00_local_roles.sql olusturur.
-- Tum test islem icinde calisir ve ROLLBACK ile biter. DURUM: HIC CALISTIRILMADI.

\set ON_ERROR_STOP on

begin;

-- 1) Katalog denetimi
do $do$
declare
  r record;
  n int;
  expected_fns text[] := array['acquire_lease', 'character_exists', 'create_character', 'get_op', 'healthcheck',
                               'import_character', 'list_keys', 'load_character', 'purge_write_ops', 'release_lease',
                               'renew_lease', 'save_character'];   -- "C" siralamasi (asagidaki order by ile ayni)
  got_fns text[];
  src text;
begin
  -- Alti tablo, hepsinde RLS acik
  select count(*) into n from pg_class c join pg_namespace s on s.oid = c.relnamespace
   where s.nspname = 'mmo' and c.relkind = 'r';
  if n <> 6 then raise exception 'FAIL: mmo tablo sayisi % (6 beklenir)', n; end if;
  for r in select c.relname, c.relrowsecurity from pg_class c join pg_namespace s on s.oid = c.relnamespace
            where s.nspname = 'mmo' and c.relkind = 'r' loop
    if not r.relrowsecurity then raise exception 'FAIL: RLS kapali: mmo.%', r.relname; end if;
  end loop;

  -- Politika YOK (varsayilan reddet)
  select count(*) into n from pg_policies where schemaname in ('mmo', 'mmo_api');
  if n <> 0 then raise exception 'FAIL: beklenmeyen RLS politikasi sayisi %', n; end if;

  -- anon / authenticated / service_role: mmo tablolarinda HICBIR yetki, mmo semasinda usage/create yok
  for r in select c.oid::regclass::text as tbl from pg_class c join pg_namespace s on s.oid = c.relnamespace
            where s.nspname = 'mmo' and c.relkind = 'r' loop
    if has_table_privilege('anon', r.tbl, 'SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER')
       or has_table_privilege('authenticated', r.tbl, 'SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER')
       or has_table_privilege('service_role', r.tbl, 'SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER') then
      raise exception 'FAIL: bir rol % uzerinde yetkili', r.tbl;
    end if;
  end loop;
  if has_schema_privilege('anon', 'mmo', 'USAGE, CREATE') or has_schema_privilege('authenticated', 'mmo', 'USAGE, CREATE')
     or has_schema_privilege('service_role', 'mmo', 'USAGE, CREATE') then
    raise exception 'FAIL: bir rol mmo semasinda yetkili';
  end if;
  if has_schema_privilege('anon', 'mmo_api', 'USAGE, CREATE') or has_schema_privilege('authenticated', 'mmo_api', 'USAGE, CREATE')
     or has_schema_privilege('service_role', 'mmo_api', 'CREATE') then
    raise exception 'FAIL: mmo_api sema yetkisi yanlis';
  end if;
  if not has_schema_privilege('service_role', 'mmo_api', 'USAGE') then
    raise exception 'FAIL: service_role mmo_api USAGE yok';
  end if;

  -- mmo_api: tam beklenen fonksiyon kumesi
  select array_agg(p.proname::text order by p.proname::text collate "C") into got_fns
    from pg_proc p join pg_namespace s on s.oid = p.pronamespace where s.nspname = 'mmo_api';
  if got_fns is distinct from expected_fns then
    raise exception 'FAIL: mmo_api fonksiyon kumesi farkli: %', got_fns;
  end if;

  -- her fonksiyon (mmo + mmo_api): sabit bos search_path; mmo_api: SECURITY DEFINER, yalniz service_role EXECUTE
  for r in select p.oid, p.proname, p.prosecdef, p.proconfig, s.nspname, p.prosrc
             from pg_proc p join pg_namespace s on s.oid = p.pronamespace where s.nspname in ('mmo', 'mmo_api') loop
    if r.proconfig is null or not exists (select 1 from unnest(r.proconfig) c where c ~ '^search_path=("")?$') then
      raise exception 'FAIL: %.% sabit bos search_path yok (%)', r.nspname, r.proname, r.proconfig;
    end if;
    if has_function_privilege('anon', r.oid, 'EXECUTE') or has_function_privilege('authenticated', r.oid, 'EXECUTE') then
      raise exception 'FAIL: anon/authenticated %.% calistirabilir', r.nspname, r.proname;
    end if;
    if r.nspname = 'mmo_api' then
      if not r.prosecdef then raise exception 'FAIL: mmo_api.% SECURITY DEFINER degil', r.proname; end if;
      if not has_function_privilege('service_role', r.oid, 'EXECUTE') then
        raise exception 'FAIL: service_role mmo_api.% calistiramaz', r.proname;
      end if;
      -- dinamik SQL yok: govdede EXECUTE sozcugu gecmemeli (kaba ama etkili bekci)
      if r.prosrc ~* '\mexecute\M' then
        raise exception 'FAIL: mmo_api.% govdesinde EXECUTE sozcugu (dinamik SQL?)', r.proname;
      end if;
    else
      if has_function_privilege('service_role', r.oid, 'EXECUTE') then
        raise exception 'FAIL: service_role mmo.% calistirabilir (yardimci olmali)', r.proname;
      end if;
    end if;
  end loop;
  raise notice 'OK katalog denetimi';

  -- Gelecek migration'lar icin varsayilan yetkiler: mmo / mmo_api semalarinda anon/authenticated/PUBLIC'e
  -- fonksiyon EXECUTE'u veren sema duzeyi varsayilan YOK (pg_default_acl). Not: PUBLIC'in yerlesik global varsayilani
  -- bu tabloda gorunmez; onu yukaridaki "her fonksiyon" denetimi yakalar (bkz. migration notu).
  select count(*) into n
    from pg_default_acl d
    join pg_namespace s on s.oid = d.defaclnamespace
    cross join lateral aclexplode(d.defaclacl) a
   where s.nspname in ('mmo', 'mmo_api') and d.defaclobjtype = 'f' and a.privilege_type = 'EXECUTE'
     and (a.grantee = 0 or a.grantee in (select oid from pg_roles where rolname in ('anon', 'authenticated')));
  if n <> 0 then raise exception 'FAIL: mmo/mmo_api icin anon/authenticated/PUBLIC varsayilan EXECUTE var (%)', n; end if;
  select count(*) into n
    from pg_default_acl d
    join pg_namespace s on s.oid = d.defaclnamespace
    cross join lateral aclexplode(d.defaclacl) a
   where s.nspname = 'mmo' and d.defaclobjtype = 'f' and a.privilege_type = 'EXECUTE'
     and a.grantee = (select oid from pg_roles where rolname = 'service_role');
  if n <> 0 then raise exception 'FAIL: mmo yardimci semasinda service_role varsayilan EXECUTE var (%)', n; end if;
  raise notice 'OK varsayilan yetkiler (pg_default_acl)';

  -- Zaman kararlari kilit beklendikten SONRA alinmali (kaba kaynak bekcisi): v_now bildirimde ilklenmez,
  -- ve atama satir kilidinden (FOR UPDATE) / op_id kilidinden (op_lock) sonra gelir.
  for r in select p.proname::text as nm, p.prosrc from pg_proc p join pg_namespace s on s.oid = p.pronamespace
            where s.nspname = 'mmo_api' and p.proname::text in ('acquire_lease', 'renew_lease', 'save_character', 'import_character', 'create_character') loop
    src := lower(r.prosrc);
    if src ~ 'v_now\s+timestamptz\s*:=' then
      raise exception 'FAIL: mmo_api.% v_now bildirimde ilkleniyor (kilit oncesi zaman)', r.nm;
    end if;
    if position('v_now := clock_timestamp()' in src) = 0 then
      raise exception 'FAIL: mmo_api.% v_now atamasi yok', r.nm;
    end if;
    if r.nm = 'create_character' then
      if position('v_now := clock_timestamp()' in src) < position('mmo.op_lock' in src) then
        raise exception 'FAIL: mmo_api.create_character zamani op_lock oncesi aliyor';
      end if;
    elsif r.nm = 'import_character' then
      if position('v_now := clock_timestamp()' in src) < position('for update' in src) then
        raise exception 'FAIL: mmo_api.import_character zamani FOR UPDATE oncesi aliyor';
      end if;
    else
      if position('v_now := clock_timestamp()' in src) < position('for update' in src) then
        raise exception 'FAIL: mmo_api.% zamani FOR UPDATE oncesi aliyor', r.nm;
      end if;
    end if;
  end loop;
  raise notice 'OK zaman kilit sonrasi (kaynak bekcisi)';
end
$do$;

-- 2) Davranissal denetim: rol -> ifade -> beklenen SQLSTATE (42501 = yetki yok)
do $do$
declare
  t record;
  v_state text;
  v_msg text;
  n int := 0;
begin
  for t in
    select * from (values
      ('anon',          'select count(*) from mmo.character_saves',                     '42501'),
      ('anon',          'insert into mmo.accounts (provider, external_id) values (''dev'', ''x'')', '42501'),
      ('anon',          'select mmo_api.healthcheck()',                                  '42501'),
      ('anon',          'select mmo_api.load_character(''zz-s-none'')',                  '42501'),
      ('anon',          'select mmo_api.purge_write_ops(7)',                             '42501'),
      ('authenticated', 'select mmo_api.purge_write_ops(7)',                             '42501'),
      ('authenticated', 'select mmo_api.get_op(gen_random_uuid())',                      '42501'),
      ('anon',          'select mmo_api.acquire_lease(''zz-s-none'', ''srv:1'')',        '42501'),
      ('service_role',  'select mmo_api.purge_write_ops(7)',                             '00000'),
      ('anon',          'select mmo.canonical_key(''x'')',                               '42501'),
      ('authenticated', 'select count(*) from mmo.character_saves',                     '42501'),
      ('authenticated', 'delete from mmo.write_ops',                                     '42501'),
      ('authenticated', 'select mmo_api.healthcheck()',                                  '42501'),
      ('authenticated', 'select mmo_api.save_character(''a'',''b'',1,0,gen_random_uuid(),2,''{}''::jsonb)', '42501'),
      ('service_role',  'select count(*) from mmo.character_saves',                     '42501'),
      ('service_role',  'insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result) values (gen_random_uuid(), ''a'', ''save'', repeat(''a'', 64), ''{}''::jsonb)', '42501'),
      ('service_role',  'delete from mmo.character_saves',                              '42501'),
      ('service_role',  'select mmo.canonical_key(''x'')',                               '42501'),
      ('service_role',  'select mmo_api.healthcheck()',                                  '00000'),
      ('service_role',  'select mmo_api.load_character(''zz-s-none'')',                  '00000')
    ) as x(role_name, stmt, expect_state)
  loop
    v_state := '00000';
    v_msg := null;
    begin
      execute 'set local role ' || quote_ident(t.role_name);
      execute t.stmt;
    exception when others then
      v_state := sqlstate;
      v_msg := sqlerrm;
    end;
    reset role;
    n := n + 1;
    if v_state <> t.expect_state then
      raise exception 'FAIL [% : %]: beklenen SQLSTATE %, gelen % (%)', t.role_name, t.stmt, t.expect_state, v_state, v_msg;
    end if;
  end loop;
  raise notice 'OK % rol/ifade denetimi', n;
end
$do$;

-- 3) RLS ikinci hat: yanlislikla GRANT verilse bile politikasiz RLS satir gostermez ve yazdirmaz.
--    (Bu islem ROLLBACK ile geri alinir; gercek yetkiler degismez.)
do $do$
declare
  n int;
  v_state text := '00000';
begin
  insert into mmo.character_saves (canonical_key, display_name, schema_version, data)
  values ('zz-s-rls', 'zz-s-rls', 2, '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":0,"NextInstanceId":1}}');

  grant usage on schema mmo to anon, authenticated;
  grant select, insert, update, delete on mmo.character_saves to anon, authenticated;

  set local role anon;
  select count(*) into n from mmo.character_saves;
  if n <> 0 then raise exception 'FAIL: anon RLS arkasinda % satir gordu', n; end if;
  begin
    insert into mmo.character_saves (canonical_key, display_name, schema_version, data)
    values ('zz-s-rls2', 'zz-s-rls2', 2, '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":0,"NextInstanceId":1}}');
  exception when others then v_state := sqlstate;
  end;
  if v_state <> '42501' then raise exception 'FAIL: anon RLS arkasinda yazabildi (state %)', v_state; end if;
  reset role;

  set local role authenticated;
  select count(*) into n from mmo.character_saves;
  if n <> 0 then raise exception 'FAIL: authenticated RLS arkasinda % satir gordu', n; end if;
  reset role;

  -- yazma denemesi de satir etkilemez (UPDATE/DELETE politikasiz RLS ile 0 satir)
  set local role anon;
  update mmo.character_saves set race = 'hack';
  get diagnostics n = row_count;
  if n <> 0 then raise exception 'FAIL: anon RLS arkasinda % satiri guncelledi', n; end if;
  delete from mmo.character_saves;
  get diagnostics n = row_count;
  if n <> 0 then raise exception 'FAIL: anon RLS arkasinda % satiri sildi', n; end if;
  reset role;

  select count(*) into n from mmo.character_saves where canonical_key = 'zz-s-rls' and race is null;
  if n <> 1 then raise exception 'FAIL: test satiri bozuldu'; end if;
  raise notice 'OK RLS ikinci savunma hatti';
end
$do$;

rollback;
\echo 'PASS 04_security'
