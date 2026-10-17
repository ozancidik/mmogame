-- 02: RPC sozlesme senaryolari. vectors.json "scenarios" dizisini sirayla calistirir (kucuk bir yorumlayici).
-- Her adim service_role olarak (SET LOCAL ROLE) cagrilir: yetki modeli de dolayli olarak denenir.
-- Adim bicimi ve yorum kurallari: vectors.json "_doc". Beklenen: sonuc, expect'i KAPSAR (jsonb @>); "exact": true ise TAM ESIT.
-- Ek sozlesme denetimleri (contract_version 2):
--   a) vectors.json rpc_params == mmo_api katalogu (fonksiyon kumesi, parametre adlari/sirasi/tipleri, zorunlu sayisi);
--   b) her adimin argumanlari rpc_params'a uyar (fazla/eksik parametre yok) -- yalniz "expect_error" adimlari bilerek uymaz;
--   c) "expect_error" adimlari beklenen SQLSTATE ile hata verir (42883 = fonksiyon/parametre kumesi yok = HTTP 404 PGRST202);
--   d) her yanitin anahtar kumesi ve deger tipleri vectors.json "responses" ornegiyle ayni (replayed haric; null = herhangi tip);
--   e) her "invalid" yanitin reason'i "invalid_reasons" listesinde.
-- UE native testi ayni dosyadaki ayni senaryolari sahte ulasimla kosar; sahte ile gercek ayrismasin diye tek kaynak.
-- Kullanim (depo kokunden, psql >= 13):  psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -v vectors="$(cat supabase/tests/vectors.json)" -f supabase/tests/02_rpc_scenarios.sql
-- Tum test islem icinde calisir ve ROLLBACK ile biter. DURUM: HIC CALISTIRILMADI.

\set ON_ERROR_STOP on
\if :{?vectors}
\else
  \echo 'HATA: vectors degiskeni yok. -v vectors="$(cat supabase/tests/vectors.json)" ver ya da run_all.sql kullan.'
  \quit
\endif
select (length(:'vectors') > 200) as vectors_ok \gset
\if :vectors_ok
\else
  \echo 'HATA: vectors degiskeni bos/cok kisa (dosya okunamadi).'
  \quit
\endif

begin;
select set_config('mmo_test.vectors', :'vectors', true);

do $do$
declare
  v jsonb := current_setting('mmo_test.vectors')::jsonb;
  fx jsonb := v -> 'fixtures';
  allowed text[] := array(select jsonb_object_keys(v -> 'rpc_params'));
  sc jsonb;
  st jsonb;
  arg record;
  val jsonb;
  lit text;
  call_sql text;
  res jsonb;
  expect jsonb;
  step_no int;
  n_steps int := 0;
  n_scen int := 0;
  n_rows int;
  is_exact boolean;
  passed boolean;
  params jsonb;
  sample jsonb;
  err_state text;
  want_state text;
  k text;
  fn_name text;
  cat_names text[];
  cat_types text[];
  cat_nargs int;
  cat_ndef int;
  want_names text[];
  want_types text[];
  want_required int;
  n_fn int := 0;
begin
  -- (a) katalog <-> rpc_params
  if (v ->> 'contract_version') is distinct from '2' then
    raise exception 'FAIL: contract_version % (2 beklenir)', v ->> 'contract_version';
  end if;
  for fn_name in select jsonb_object_keys(v -> 'rpc_params') loop
    n_fn := n_fn + 1;
    select coalesce(p.proargnames, '{}'::text[]), p.pronargs, p.pronargdefaults,
           coalesce((select array_agg(format_type(u.t::oid, null) order by u.o)
                       from unnest(string_to_array(p.proargtypes::text, ' ')) with ordinality as u(t, o)
                      where u.t <> ''), '{}'::text[])
      into cat_names, cat_nargs, cat_ndef, cat_types
      from pg_proc p join pg_namespace s on s.oid = p.pronamespace
     where s.nspname = 'mmo_api' and p.proname::text = fn_name;
    if not found then
      raise exception 'FAIL: rpc_params.% katalogda (mmo_api) yok', fn_name;
    end if;
    params := v -> 'rpc_params' -> fn_name;
    select coalesce(array_agg(x.value ->> 'name' order by x.ord), '{}'::text[]),
           coalesce(array_agg(x.value ->> 'pg_type' order by x.ord), '{}'::text[]),
           count(*) filter (where (x.value ->> 'required')::boolean)
      into want_names, want_types, want_required
      from jsonb_array_elements(params) with ordinality as x(value, ord);
    if cat_names is distinct from want_names then
      raise exception 'FAIL: % parametre adlari katalog % <> vectors %', fn_name, cat_names, want_names;
    end if;
    if cat_types is distinct from want_types then
      raise exception 'FAIL: % parametre tipleri katalog % <> vectors %', fn_name, cat_types, want_types;
    end if;
    if cat_nargs - cat_ndef <> want_required then
      raise exception 'FAIL: % zorunlu parametre sayisi katalog % <> vectors %', fn_name, cat_nargs - cat_ndef, want_required;
    end if;
  end loop;
  select count(*) into n_rows
    from pg_proc p join pg_namespace s on s.oid = p.pronamespace
   where s.nspname = 'mmo_api' and p.proname::text <> all (allowed);
  if n_rows <> 0 then
    raise exception 'FAIL: mmo_api icinde rpc_params''ta olmayan % fonksiyon var', n_rows;
  end if;
  raise notice 'OK rpc_params == katalog (% fonksiyon)', n_fn;

  for sc in select value from jsonb_array_elements(v -> 'scenarios') loop
    n_scen := n_scen + 1;
    step_no := 0;
    for st in select value from jsonb_array_elements(sc -> 'steps') loop
      step_no := step_no + 1;
      n_steps := n_steps + 1;

      if st ? 'admin' then
        -- Yonetici adimi: yalniz test duzeneginde (rolsuz) yapilir.
        reset role;
        if st ->> 'admin' = 'expire_lease' then
          update mmo.character_saves
             set lease_until = clock_timestamp() - interval '1 second'
           where canonical_key = st ->> 'key' and active_server is not null;
          get diagnostics n_rows = row_count;
          if n_rows <> 1 then
            raise exception 'FAIL [%] adim %: expire_lease % satiri etkiledi (1 beklenir)', sc ->> 'id', step_no, n_rows;
          end if;
        else
          raise exception 'FAIL [%] adim %: bilinmeyen admin adimi %', sc ->> 'id', step_no, st ->> 'admin';
        end if;
        continue;
      end if;

      if not ((st ->> 'rpc') = any (allowed)) then
        raise exception 'FAIL [%] adim %: bilinmeyen rpc %', sc ->> 'id', step_no, st ->> 'rpc';
      end if;
      if not (st ? 'expect') and not (st ? 'expect_error') then
        raise exception 'FAIL [%] adim %: expect ya da expect_error yok', sc ->> 'id', step_no;
      end if;

      -- (b) arguman adlari rpc_params ile birebir uyumlu olmali (expect_error adimlari bilerek uymaz)
      if not (st ? 'expect_error') then
        params := v -> 'rpc_params' -> (st ->> 'rpc');
        for k in select jsonb_object_keys(coalesce(st -> 'args', '{}'::jsonb)) loop
          if not exists (select 1 from jsonb_array_elements(params) x where x ->> 'name' = k) then
            raise exception 'FAIL [%] adim % (%): rpc_params disi parametre %', sc ->> 'id', step_no, st ->> 'rpc', k;
          end if;
        end loop;
        for k in select x ->> 'name' from jsonb_array_elements(params) x where (x ->> 'required')::boolean loop
          if not (coalesce(st -> 'args', '{}'::jsonb) ? k) then
            raise exception 'FAIL [%] adim % (%): zorunlu parametre % eksik', sc ->> 'id', step_no, st ->> 'rpc', k;
          end if;
        end loop;
      end if;

      -- Adlandirilmis parametrelerle cagri metni (degerler quote_literal; tip cozumu fonksiyon imzasindan).
      call_sql := '';
      for arg in select * from jsonb_each(coalesce(st -> 'args', '{}'::jsonb)) loop
        val := arg.value;
        if jsonb_typeof(val) = 'string' and left(val #>> '{}', 1) = '@' then
          val := fx -> substr(val #>> '{}', 2);
          if val is null then
            raise exception 'FAIL [%] adim %: bilinmeyen fixture %', sc ->> 'id', step_no, arg.value;
          end if;
        end if;
        lit := case jsonb_typeof(val)
                 when 'null'   then 'null'
                 when 'string' then quote_literal(val #>> '{}')
                 when 'number' then quote_literal(val #>> '{}')
                 else quote_literal(val::text)
               end;
        call_sql := call_sql || case when call_sql = '' then '' else ', ' end || quote_ident(arg.key) || ' => ' || lit;
      end loop;

      err_state := null;
      res := null;
      begin
        set local role service_role;
        execute 'select mmo_api.' || quote_ident(st ->> 'rpc') || '(' || call_sql || ')' into res;
      exception when others then
        err_state := sqlstate;
        res := null;
      end;
      reset role;

      -- (c) sozlesme ihlali adimi: beklenen SQLSTATE ile hata vermeli, vermemeli baska bir sey
      if st ? 'expect_error' then
        want_state := st -> 'expect_error' ->> 'sqlstate';
        if err_state is distinct from want_state then
          raise exception 'FAIL [%] adim % (%): beklenen SQLSTATE %, gelen % (sonuc %)',
            sc ->> 'id', step_no, st ->> 'rpc', want_state, err_state, res;
        end if;
        continue;
      end if;
      if err_state is not null then
        raise exception 'FAIL [%] adim % (%): beklenmeyen istisna SQLSTATE %', sc ->> 'id', step_no, st ->> 'rpc', err_state;
      end if;

      -- (d) yanit sekli: anahtar kumesi + tipler vectors.json "responses" ornegiyle ayni
      sample := v -> 'responses' -> (st ->> 'rpc') -> (res ->> 'outcome');
      if sample is null then
        raise exception 'FAIL [%] adim % (%): outcome % icin responses ornegi yok (yanit %)',
          sc ->> 'id', step_no, st ->> 'rpc', res ->> 'outcome', res;
      end if;
      if (select coalesce(array_agg(x order by x), '{}'::text[]) from jsonb_object_keys(res - 'replayed') x)
         is distinct from (select coalesce(array_agg(x order by x), '{}'::text[]) from jsonb_object_keys(sample - 'replayed') x) then
        raise exception 'FAIL [%] adim % (%): yanit anahtarlari ornekten farkli: yanit %, ornek %',
          sc ->> 'id', step_no, st ->> 'rpc', res::text, sample::text;
      end if;
      for k in select jsonb_object_keys(sample - 'replayed') loop
        if jsonb_typeof(res -> k) <> 'null' and jsonb_typeof(sample -> k) <> 'null'
           and jsonb_typeof(res -> k) <> jsonb_typeof(sample -> k) then
          raise exception 'FAIL [%] adim % (%): % tipi % (ornek %)',
            sc ->> 'id', step_no, st ->> 'rpc', k, jsonb_typeof(res -> k), jsonb_typeof(sample -> k);
        end if;
      end loop;
      -- (e) invalid -> reason bilinen listede
      if res ->> 'outcome' = 'invalid' and not ((v -> 'invalid_reasons') ? (res ->> 'reason')) then
        raise exception 'FAIL [%] adim % (%): reason % invalid_reasons listesinde yok', sc ->> 'id', step_no, st ->> 'rpc', res ->> 'reason';
      end if;

      expect := (select coalesce(jsonb_object_agg(e.key,
                  case when jsonb_typeof(e.value) = 'string' and left(e.value #>> '{}', 1) = '@'
                       then fx -> substr(e.value #>> '{}', 2) else e.value end), '{}'::jsonb)
                   from jsonb_each(st -> 'expect') e);
      is_exact := coalesce((st ->> 'exact')::boolean, false);
      passed := case when is_exact then res = expect else res @> expect end;
      if not passed then
        raise exception 'FAIL [%] adim % (%): beklenen% %, gelen %',
          sc ->> 'id', step_no, st ->> 'rpc', case when is_exact then ' (tam)' else '' end, expect::text, res::text;
      end if;
    end loop;
    raise notice 'OK senaryo %', sc ->> 'id';
  end loop;

  if n_scen < 19 then
    raise exception 'FAIL: senaryo sayisi beklenenden az (%)', n_scen;
  end if;
  raise notice 'OK % senaryo, % adim', n_scen, n_steps;
end
$do$;

rollback;
\echo 'PASS 02_rpc_scenarios'
