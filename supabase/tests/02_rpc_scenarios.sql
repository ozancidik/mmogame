-- 02: RPC sozlesme senaryolari. vectors.json "scenarios" dizisini sirayla calistirir (kucuk bir yorumlayici).
-- Her adim service_role olarak (SET LOCAL ROLE) cagrilir: yetki modeli de dolayli olarak denenir.
-- Adim bicimi ve yorum kurallari: vectors.json "_doc". Beklenen: sonuc, expect'i KAPSAR (jsonb @>); "exact": true ise TAM ESIT.
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
begin
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
      if not (st ? 'expect') then
        raise exception 'FAIL [%] adim %: expect yok', sc ->> 'id', step_no;
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

      set local role service_role;
      execute 'select mmo_api.' || quote_ident(st ->> 'rpc') || '(' || call_sql || ')' into res;
      reset role;

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

  if n_scen < 16 then
    raise exception 'FAIL: senaryo sayisi beklenenden az (%)', n_scen;
  end if;
  raise notice 'OK % senaryo, % adim', n_scen, n_steps;
end
$do$;

rollback;
\echo 'PASS 02_rpc_scenarios'
