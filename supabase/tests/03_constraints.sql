-- 03: tablo CHECK / FK / UNIQUE kisitlari, tetikleyiciler (revision/epoch geri gitmez, anahtar degismez, updated_at, cascade)
-- ve boyut tavani. DOGRUDAN SQL ile (tablo sahibi / superuser olarak); RPC yolunun reddi 02'de.
-- Kullanim (depo kokunden):  psql "$DB" -v ON_ERROR_STOP=1 -f supabase/tests/03_constraints.sql
-- Tum test islem icinde calisir ve ROLLBACK ile biter. DURUM: HIC CALISTIRILMADI.
-- Beklenen SQLSTATE: 23514 check_violation, 23503 foreign_key_violation, 23505 unique_violation, 00000 = basarili.

\set ON_ERROR_STOP on

begin;

do $do$
declare
  envs jsonb := jsonb_build_object(
    'ENV',      '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":0,"NextInstanceId":1}}'::jsonb,
    'ENV_GOLD_NEG',   '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":-1,"NextInstanceId":1}}'::jsonb,
    'ENV_GOLD_STR',   '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":"12","NextInstanceId":1}}'::jsonb,
    'ENV_GOLD_FRAC',  '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":1.5,"NextInstanceId":1}}'::jsonb,
    'ENV_LEVEL_ZERO', '{"SchemaVersion":2,"Data":{"Level":0,"Experience":0,"Gold":0,"NextInstanceId":1}}'::jsonb,
    'ENV_V1',         '{"SchemaVersion":1,"Data":{"Level":1,"Experience":0,"Gold":0,"NextInstanceId":1}}'::jsonb,
    'ENV_BIG',  jsonb_build_object('SchemaVersion', 2, 'Data',
                  jsonb_build_object('Level', 1, 'Experience', 0, 'Gold', 0, 'NextInstanceId', 1, 'pad', repeat('x', 2200000)))
  );
  t record;
  nm record;
  stmt text;
  v_state text;
  v_msg text;
  n int := 0;
begin
  for t in
    select * from (values
      ('gecerli satir',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-ok', 'ZZ-C-OK', 2, {ENV})$q$, '00000'),
      ('buyuk harfli anahtar',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('ZZ-C', 'ZZ-C', 2, {ENV})$q$, '23514'),
      ('anahtar-ad uyusmazligi',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-1', 'zz-c-2', 2, {ENV})$q$, '23514'),
      ('ayrilmis anahtar default',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('default', 'Default', 2, {ENV})$q$, '23514'),
      ('ayrilmis anahtar player12',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('player12', 'Player12', 2, {ENV})$q$, '23514'),
      ('bos ad',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-e', '', 2, {ENV})$q$, '23514'),
      ('ASCII disi ad (O-umlaut)',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('mer', chr(214) || 'mer', 2, {ENV})$q$, '23514'),
      ('33 karakterli ad',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values (repeat('a', 32), repeat('a', 33), 2, {ENV})$q$, '23514'),
      ('revision negatif',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, revision, data) values ('zz-c-r', 'zz-c-r', 2, -1, {ENV})$q$, '23514'),
      ('Gold negatif',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-g', 'zz-c-g', 2, {ENV_GOLD_NEG})$q$, '23514'),
      ('Gold metin',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-g', 'zz-c-g', 2, {ENV_GOLD_STR})$q$, '23514'),
      ('Gold ondalik',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-g', 'zz-c-g', 2, {ENV_GOLD_FRAC})$q$, '23514'),
      ('Level 0',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-l', 'zz-c-l', 2, {ENV_LEVEL_ZERO})$q$, '23514'),
      ('zarf surumu sutunla uyusmuyor',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-v', 'zz-c-v', 2, {ENV_V1})$q$, '23514'),
      ('v1 zarf, v1 surum sutunu gecerli',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-v1', 'zz-c-v1', 1, {ENV_V1})$q$, '00000'),
      ('veri dizi',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-a', 'zz-c-a', 2, '[]'::jsonb)$q$, '23514'),
      ('schema_version 0',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-s', 'zz-c-s', 0, {ENV})$q$, '23514'),
      ('kira suresi cok kisa',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data, lease_ttl_s) values ('zz-c-t', 'zz-c-t', 2, {ENV}, 5)$q$, '23514'),
      ('kira suresi cok uzun',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data, lease_ttl_s) values ('zz-c-t', 'zz-c-t', 2, {ENV}, 601)$q$, '23514'),
      ('irk gecersiz karakter',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data, race) values ('zz-c-race', 'zz-c-race', 2, {ENV}, 'bad race')$q$, '23514'),
      ('kira cifti: sunucu var, sure yok',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data, active_server) values ('zz-c-p', 'zz-c-p', 2, {ENV}, 'srv:1')$q$, '23514'),
      ('kira cifti: sure var, sunucu yok',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data, lease_until) values ('zz-c-p', 'zz-c-p', 2, {ENV}, now())$q$, '23514'),
      ('sunucu kimligi gecersiz',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data, active_server, lease_until) values ('zz-c-h', 'zz-c-h', 2, {ENV}, 'bad holder', now())$q$, '23514'),
      ('ice aktarma ozeti gecersiz',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data, import_sha256) values ('zz-c-i', 'zz-c-i', 2, {ENV}, 'xyz')$q$, '23514'),
      ('ayni anahtar ikinci kez',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-ok', 'ZZ-C-OK', 2, {ENV})$q$, '23505'),
      ('veri 2 MiB ustu',
       $q$insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-big', 'zz-c-big', 2, {ENV_BIG})$q$, '23514'),
      ('yan veri gecerli',
       $q$insert into mmo.character_side (canonical_key, kind, data) values ('zz-c-ok', 'daily', '{"a":1}'::jsonb)$q$, '00000'),
      ('yan veri bilinmeyen tur',
       $q$insert into mmo.character_side (canonical_key, kind, data) values ('zz-c-ok', 'bogus', '{}'::jsonb)$q$, '23514'),
      ('yan veri dizi',
       $q$insert into mmo.character_side (canonical_key, kind, data) values ('zz-c-ok', 'mail', '[]'::jsonb)$q$, '23514'),
      ('yan veri sahipsiz anahtar',
       $q$insert into mmo.character_side (canonical_key, kind, data) values ('zz-c-yok', 'daily', '{}'::jsonb)$q$, '23503'),
      ('yan veri ayni (anahtar, tur)',
       $q$insert into mmo.character_side (canonical_key, kind, data) values ('zz-c-ok', 'daily', '{}'::jsonb)$q$, '23505'),
      ('yan veri revision negatif',
       $q$insert into mmo.character_side (canonical_key, kind, data, revision) values ('zz-c-ok', 'achievements', '{}'::jsonb, -1)$q$, '23514'),
      ('write_ops gecerli',
       $q$insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result) values ('d0000000-0000-4000-8000-000000000001', 'zz-c-ok', 'save', repeat('a', 64), '{"outcome":"committed"}'::jsonb)$q$, '00000'),
      ('write_ops ayni op_id',
       $q$insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result) values ('d0000000-0000-4000-8000-000000000001', 'zz-c-ok', 'save', repeat('a', 64), '{}'::jsonb)$q$, '23505'),
      ('write_ops ozet gecersiz',
       $q$insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result) values ('d0000000-0000-4000-8000-000000000002', 'zz-c-ok', 'save', 'xyz', '{}'::jsonb)$q$, '23514'),
      ('write_ops tur gecersiz',
       $q$insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result) values ('d0000000-0000-4000-8000-000000000003', 'zz-c-ok', 'bogus', repeat('a', 64), '{}'::jsonb)$q$, '23514'),
      ('write_ops sonuc dizi',
       $q$insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result) values ('d0000000-0000-4000-8000-000000000004', 'zz-c-ok', 'save', repeat('a', 64), '[]'::jsonb)$q$, '23514'),
      ('hesap gecerli',
       $q$insert into mmo.accounts (provider, external_id) values ('steam', '76561198000000000')$q$, '00000'),
      ('hesap ayni (saglayici, dis kimlik)',
       $q$insert into mmo.accounts (provider, external_id) values ('steam', '76561198000000000')$q$, '23505'),
      ('hesap saglayici gecersiz',
       $q$insert into mmo.accounts (provider, external_id) values ('x', '1')$q$, '23514'),
      ('hesap dis kimlik bosluklu',
       $q$insert into mmo.accounts (provider, external_id) values ('dev', 'bad id')$q$, '23514'),
      ('presence gecerli',
       $q$insert into mmo.presence (canonical_key, server_id) values ('zz-c-ok', 'srvA:boot1')$q$, '00000'),
      ('presence sahipsiz anahtar',
       $q$insert into mmo.presence (canonical_key, server_id) values ('zz-c-yok', 'srvA:boot1')$q$, '23503'),
      ('bilet ozeti gecersiz',
       $q$insert into mmo.travel_tickets (ticket_sha256, canonical_key, from_server, to_server, expires_at) values ('xyz', 'zz-c-ok', 'a', 'b', now())$q$, '23514'),
      ('bilet gecerli',
       $q$insert into mmo.travel_tickets (ticket_sha256, canonical_key, from_server, to_server, expires_at) values (repeat('b', 64), 'zz-c-ok', 'a', 'b', now())$q$, '00000')
    ) as x(label, tmpl, expect_state)
  loop
    stmt := t.tmpl;
    for nm in select key, value from jsonb_each(envs) loop
      -- Uzun adlari once degistirmek icin {ADI} suslu parantezli; ENV, ENV_* adlarindan ayri.
      stmt := replace(stmt, '{' || nm.key || '}', quote_literal(nm.value::text) || '::jsonb');
    end loop;
    v_state := '00000';
    v_msg := null;
    begin
      execute stmt;
    exception when others then
      v_state := sqlstate;
      v_msg := sqlerrm;
    end;
    n := n + 1;
    if v_state <> t.expect_state then
      raise exception 'FAIL [%]: beklenen SQLSTATE %, gelen % (%)', t.label, t.expect_state, v_state, v_msg;
    end if;
  end loop;
  raise notice 'OK % kisit senaryosu', n;
end
$do$;

-- Tetikleyiciler
do $do$
declare
  v_state text;
  t0 timestamptz;
  t1 timestamptz;
  s0 timestamptz;
  s1 timestamptz;
  n int;
begin
  -- revision artabilir, azalamaz
  update mmo.character_saves set revision = 3 where canonical_key = 'zz-c-ok';
  v_state := '00000';
  begin
    update mmo.character_saves set revision = 2 where canonical_key = 'zz-c-ok';
  exception when others then v_state := sqlstate;
  end;
  if v_state <> '23514' then raise exception 'FAIL: revision azaldi (state %)', v_state; end if;

  -- epoch artabilir, azalamaz
  update mmo.character_saves set lease_epoch = 4 where canonical_key = 'zz-c-ok';
  v_state := '00000';
  begin
    update mmo.character_saves set lease_epoch = 3 where canonical_key = 'zz-c-ok';
  exception when others then v_state := sqlstate;
  end;
  if v_state <> '23514' then raise exception 'FAIL: lease_epoch azaldi (state %)', v_state; end if;

  -- anahtar ve character_id degismez
  v_state := '00000';
  begin
    update mmo.character_saves set canonical_key = 'zz-c-ok2', display_name = 'zz-c-ok2' where canonical_key = 'zz-c-ok';
  exception when others then v_state := sqlstate;
  end;
  if v_state <> '23514' then raise exception 'FAIL: canonical_key degisti (state %)', v_state; end if;
  v_state := '00000';
  begin
    update mmo.character_saves set character_id = gen_random_uuid() where canonical_key = 'zz-c-ok';
  exception when others then v_state := sqlstate;
  end;
  if v_state <> '23514' then raise exception 'FAIL: character_id degisti (state %)', v_state; end if;

  -- updated_at tetikleyicisi (saves + side)
  select updated_at into t0 from mmo.character_saves where canonical_key = 'zz-c-ok';
  select updated_at into s0 from mmo.character_side where canonical_key = 'zz-c-ok' and kind = 'daily';
  perform pg_sleep(0.05);
  update mmo.character_saves set race = 'r2' where canonical_key = 'zz-c-ok';
  update mmo.character_side set revision = revision + 1 where canonical_key = 'zz-c-ok' and kind = 'daily';
  select updated_at into t1 from mmo.character_saves where canonical_key = 'zz-c-ok';
  select updated_at into s1 from mmo.character_side where canonical_key = 'zz-c-ok' and kind = 'daily';
  if not (t1 > t0) then raise exception 'FAIL: character_saves.updated_at ilerlemedi'; end if;
  if not (s1 > s0) then raise exception 'FAIL: character_side.updated_at ilerlemedi'; end if;

  -- silme: yan veri, presence, bilet CASCADE ile gider; write_ops kalir (FK yok, gunluk korunur)
  delete from mmo.character_saves where canonical_key = 'zz-c-ok';
  select count(*) into n from mmo.character_side where canonical_key = 'zz-c-ok';
  if n <> 0 then raise exception 'FAIL: yan veri cascade ile silinmedi'; end if;
  select count(*) into n from mmo.presence where canonical_key = 'zz-c-ok';
  if n <> 0 then raise exception 'FAIL: presence cascade ile silinmedi'; end if;
  select count(*) into n from mmo.travel_tickets where canonical_key = 'zz-c-ok';
  if n <> 0 then raise exception 'FAIL: bilet cascade ile silinmedi'; end if;
  select count(*) into n from mmo.write_ops where canonical_key = 'zz-c-ok';
  if n <> 1 then raise exception 'FAIL: write_ops silindi/degisti (%)', n; end if;
  raise notice 'OK tetikleyici ve cascade senaryolari';
end
$do$;

-- Zarf denetimi tetikleyicidir (CHECK degil): yalniz data / schema_version DEGISINCE calisir; kira guncellemeleri tetiklemez.
do $do$
declare
  env jsonb := '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":0,"NextInstanceId":1}}';
  bad jsonb := '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":-5,"NextInstanceId":1}}';
  bad2 jsonb := '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":-6,"NextInstanceId":1}}';
  v_state text;
  n int;
begin
  -- eski CHECK kalmamali; iki tetikleyici etkin olmali
  select count(*) into n from pg_constraint where conrelid = 'mmo.character_saves'::regclass and conname = 'character_saves_envelope';
  if n <> 0 then raise exception 'FAIL: character_saves_envelope CHECK hala var'; end if;
  select count(*) into n from pg_trigger
   where tgrelid = 'mmo.character_saves'::regclass and not tgisinternal
     and tgname in ('character_saves_envelope_ins_trg', 'character_saves_envelope_upd_trg') and tgenabled = 'O';
  if n <> 2 then raise exception 'FAIL: zarf tetikleyicileri etkin degil (%)', n; end if;

  insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-trg', 'zz-c-trg', 2, env);

  -- veri degisince denetlenir
  v_state := '00000';
  begin update mmo.character_saves set data = bad where canonical_key = 'zz-c-trg';
  exception when others then v_state := sqlstate; end;
  if v_state <> '23514' then raise exception 'FAIL: bozuk veri guncellemesi reddedilmedi (state %)', v_state; end if;

  -- schema_version degisince de denetlenir (zarf surumu ile uyusmaz)
  v_state := '00000';
  begin update mmo.character_saves set schema_version = 3 where canonical_key = 'zz-c-trg';
  exception when others then v_state := sqlstate; end;
  if v_state <> '23514' then raise exception 'FAIL: schema_version uyusmazligi reddedilmedi (state %)', v_state; end if;

  -- gecerli veri guncellemesi gecer
  update mmo.character_saves set data = jsonb_set(env, '{Data,Gold}', '77') where canonical_key = 'zz-c-trg';

  -- Kanit: bozuk bir "eski" satir (tetikleyiciler gecici kapali eklenir); kira guncellemeleri bu satirda HATA VERMEZ,
  -- yani zarf kira yenilemede yeniden denetlenmiyor. Veri degisirse yine reddedilir.
  alter table mmo.character_saves disable trigger character_saves_envelope_ins_trg;
  insert into mmo.character_saves (canonical_key, display_name, schema_version, data) values ('zz-c-legacy', 'zz-c-legacy', 2, bad);
  alter table mmo.character_saves enable trigger character_saves_envelope_ins_trg;

  update mmo.character_saves
     set active_server = 'srv:1', lease_until = now() + interval '1 minute', lease_epoch = lease_epoch + 1
   where canonical_key = 'zz-c-legacy';
  get diagnostics n = row_count;
  if n <> 1 then raise exception 'FAIL: kira guncellemesi etkilemedi'; end if;
  update mmo.character_saves set lease_until = now() + interval '2 minutes' where canonical_key = 'zz-c-legacy';
  update mmo.character_saves set active_server = null, lease_until = null where canonical_key = 'zz-c-legacy';
  -- ayni veriyi yeniden yazmak (degisiklik yok) da denetlenmez
  update mmo.character_saves set data = data where canonical_key = 'zz-c-legacy';

  v_state := '00000';
  begin update mmo.character_saves set data = bad2 where canonical_key = 'zz-c-legacy';
  exception when others then v_state := sqlstate; end;
  if v_state <> '23514' then raise exception 'FAIL: legacy satirda veri degisimi reddedilmedi (state %)', v_state; end if;

  select count(*) into n from pg_trigger
   where tgrelid = 'mmo.character_saves'::regclass and not tgisinternal
     and tgname in ('character_saves_envelope_ins_trg', 'character_saves_envelope_upd_trg') and tgenabled = 'O';
  if n <> 2 then raise exception 'FAIL: tetikleyiciler yeniden etkinlestirilemedi (%)', n; end if;
  raise notice 'OK zarf tetikleyicisi (veri degisince) / kira guncellemesi denetimsiz';
end
$do$;

-- purge_write_ops: yalniz eski satirlar, sinirlar, parti tavani (5000). Her sey ROLLBACK ile geri alinir.
do $do$
declare
  r jsonb;
  n int;
begin
  delete from mmo.write_ops where created_at < now() - interval '1 day';   -- deterministik baslangic (islem icinde)
  insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result, created_at) values
    ('d0000000-0000-4000-8000-0000000000a1', 'zz-c-p', 'save', repeat('a', 64), '{}'::jsonb, now() - interval '3 days'),
    ('d0000000-0000-4000-8000-0000000000a2', 'zz-c-p', 'save', repeat('a', 64), '{}'::jsonb, now() - interval '10 days'),
    ('d0000000-0000-4000-8000-0000000000a3', 'zz-c-p', 'save', repeat('a', 64), '{}'::jsonb, now() - interval '2 hours');

  set local role service_role;
  r := mmo_api.purge_write_ops(7);
  reset role;
  if r is distinct from '{"outcome":"ok","deleted":1,"more":false}'::jsonb then raise exception 'FAIL purge(7): %', r; end if;
  select count(*) into n from mmo.write_ops where op_id = 'd0000000-0000-4000-8000-0000000000a2';
  if n <> 0 then raise exception 'FAIL: 10 gunluk satir silinmedi'; end if;
  select count(*) into n from mmo.write_ops where op_id in ('d0000000-0000-4000-8000-0000000000a1', 'd0000000-0000-4000-8000-0000000000a3');
  if n <> 2 then raise exception 'FAIL: yeni satirlar silindi (%)', n; end if;

  set local role service_role;
  r := mmo_api.purge_write_ops(1);
  reset role;
  if r is distinct from '{"outcome":"ok","deleted":1,"more":false}'::jsonb then raise exception 'FAIL purge(1): %', r; end if;
  select count(*) into n from mmo.write_ops where op_id = 'd0000000-0000-4000-8000-0000000000a3';
  if n <> 1 then raise exception 'FAIL: 2 saatlik satir silindi'; end if;

  -- alt sinir ve gecersiz degerler: hicbir sey silinmez
  set local role service_role;
  r := mmo_api.purge_write_ops(0);
  if r is distinct from '{"outcome":"invalid","reason":"bad_days"}'::jsonb then raise exception 'FAIL purge(0): %', r; end if;
  r := mmo_api.purge_write_ops(-1);
  if r is distinct from '{"outcome":"invalid","reason":"bad_days"}'::jsonb then raise exception 'FAIL purge(-1): %', r; end if;
  r := mmo_api.purge_write_ops(3651);
  if r is distinct from '{"outcome":"invalid","reason":"bad_days"}'::jsonb then raise exception 'FAIL purge(3651): %', r; end if;
  r := mmo_api.purge_write_ops(null);
  if r is distinct from '{"outcome":"invalid","reason":"bad_days"}'::jsonb then raise exception 'FAIL purge(null): %', r; end if;
  reset role;

  -- parti tavani: 5001 eski satir -> ilk cagri 5000 siler ve more:true, ikincisi kalan 1'i siler
  insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result, created_at)
  select gen_random_uuid(), 'zz-c-p', 'save', repeat('b', 64), '{}'::jsonb, now() - interval '20 days' - g * interval '1 second'
    from generate_series(1, 5001) g;
  set local role service_role;
  r := mmo_api.purge_write_ops(7);
  if r is distinct from '{"outcome":"ok","deleted":5000,"more":true}'::jsonb then raise exception 'FAIL purge parti 1: %', r; end if;
  r := mmo_api.purge_write_ops();
  reset role;
  if r is distinct from '{"outcome":"ok","deleted":1,"more":false}'::jsonb then raise exception 'FAIL purge parti 2 (varsayilan gun): %', r; end if;
  select count(*) into n from mmo.write_ops where created_at < now() - interval '1 day';
  if n <> 0 then raise exception 'FAIL: eski satir kaldi (%)', n; end if;
  -- p_batch: aralik 1..50000 disi = bad_batch; kucuk parti -> more:true; son 24 saat korunur (p_older_than_days = 1 alt sinir)
  set local role service_role;
  r := mmo_api.purge_write_ops(7, 0);
  if r is distinct from '{"outcome":"invalid","reason":"bad_batch"}'::jsonb then raise exception 'FAIL purge batch 0: %', r; end if;
  r := mmo_api.purge_write_ops(7, -1);
  if r is distinct from '{"outcome":"invalid","reason":"bad_batch"}'::jsonb then raise exception 'FAIL purge batch -1: %', r; end if;
  r := mmo_api.purge_write_ops(7, 50001);
  if r is distinct from '{"outcome":"invalid","reason":"bad_batch"}'::jsonb then raise exception 'FAIL purge batch 50001: %', r; end if;
  r := mmo_api.purge_write_ops(7, null);
  if r is distinct from '{"outcome":"invalid","reason":"bad_batch"}'::jsonb then raise exception 'FAIL purge batch null: %', r; end if;
  reset role;

  insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result, created_at)
  select gen_random_uuid(), 'zz-c-p', 'save', repeat('c', 64), '{}'::jsonb, now() - interval '30 days' - g * interval '1 second'
    from generate_series(1, 7) g;
  insert into mmo.write_ops (op_id, canonical_key, op_kind, payload_sha256, result, created_at)
  values ('d0000000-0000-4000-8000-0000000000a4', 'zz-c-p', 'save', repeat('c', 64), '{}'::jsonb, now() - interval '23 hours');
  set local role service_role;
  r := mmo_api.purge_write_ops(7, 3);
  if r is distinct from '{"outcome":"ok","deleted":3,"more":true}'::jsonb then raise exception 'FAIL purge batch 3: %', r; end if;
  r := mmo_api.purge_write_ops(7, 3);
  if r is distinct from '{"outcome":"ok","deleted":3,"more":true}'::jsonb then raise exception 'FAIL purge batch 3 (2): %', r; end if;
  r := mmo_api.purge_write_ops(1, 50000);
  reset role;
  if r is distinct from '{"outcome":"ok","deleted":1,"more":false}'::jsonb then raise exception 'FAIL purge batch son: %', r; end if;
  select count(*) into n from mmo.write_ops where op_id = 'd0000000-0000-4000-8000-0000000000a4';
  if n <> 1 then raise exception 'FAIL: 23 saatlik satir silindi (son 24 saat korunmali)'; end if;
  raise notice 'OK purge_write_ops';
end
$do$;

-- RPC yolunda boyut tavani ve bos/yanlis girdiler (service_role olarak)
do $do$
declare
  env jsonb := '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":0,"NextInstanceId":1}}';
  big jsonb := jsonb_build_object('SchemaVersion', 2, 'Data',
                 jsonb_build_object('Level', 1, 'Experience', 0, 'Gold', 0, 'NextInstanceId', 1, 'pad', repeat('x', 2200000)));
  r jsonb;
begin
  set local role service_role;
  r := mmo_api.create_character('zz-c-rpc', 'zz-c-rpc', 'srvA:boot1', 'd0000000-0000-4000-8000-000000000010', 2, env);
  if r ->> 'outcome' <> 'committed' then raise exception 'FAIL create: %', r; end if;
  r := mmo_api.save_character('zz-c-rpc', 'srvA:boot1', 1, 0, 'd0000000-0000-4000-8000-000000000011', 2, big);
  if r is distinct from '{"outcome":"invalid","reason":"data_too_large"}'::jsonb then raise exception 'FAIL buyuk veri: %', r; end if;
  r := mmo_api.save_character('zz-c-rpc', 'srvA:boot1', 1, 0, 'd0000000-0000-4000-8000-000000000012', 2,
         env, jsonb_build_object('daily', jsonb_build_object('pad', repeat('x', 300000))));
  if r is distinct from '{"outcome":"invalid","reason":"side_bad_value"}'::jsonb then raise exception 'FAIL buyuk yan veri: %', r; end if;
  r := mmo_api.create_character('zz-c-big', 'zz-c-big', 'srvA:boot1', 'd0000000-0000-4000-8000-000000000013', 2, big);
  if r is distinct from '{"outcome":"invalid","reason":"data_too_large"}'::jsonb then raise exception 'FAIL buyuk create: %', r; end if;
  r := mmo_api.save_character('zz-c-rpc', 'srvA:boot1', 1, 0, 'd0000000-0000-4000-8000-000000000014', 2, null);
  if r is distinct from '{"outcome":"invalid","reason":"data_not_object"}'::jsonb then raise exception 'FAIL null veri: %', r; end if;
  r := mmo_api.acquire_lease('zz-c-rpc', 'srvB:boot2', 9);
  if r is distinct from '{"outcome":"invalid","reason":"bad_ttl"}'::jsonb then raise exception 'FAIL ttl 9: %', r; end if;
  r := mmo_api.acquire_lease('zz-c-rpc', 'srvB:boot2', 601);
  if r is distinct from '{"outcome":"invalid","reason":"bad_ttl"}'::jsonb then raise exception 'FAIL ttl 601: %', r; end if;
  reset role;
  raise notice 'OK RPC boyut tavani / girdi dogrulama';
end
$do$;

rollback;
\echo 'PASS 03_constraints'
