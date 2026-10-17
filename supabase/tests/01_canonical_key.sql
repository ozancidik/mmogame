-- 01: kanonik anahtar + dogrulama yardimcilari (vectors.json "canonical_key" dizisi + satir ici birim kontrolleri).
-- Kosul: migration uygulanmis olmali. Duz psql, pgTAP YOK. Hata = istisna (psql ON_ERROR_STOP ile sifirdan farkli cikis).
-- Kullanim (depo kokunden, psql >= 13):  psql "$DB" -v ON_ERROR_STOP=1 -v vectors="$(cat supabase/tests/vectors.json)" -f supabase/tests/01_canonical_key.sql
-- Tum test islem icinde calisir ve ROLLBACK ile biter (veritabanina kalici iz birakmaz).
-- DURUM: HIC CALISTIRILMADI.

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
  e jsonb;
  n int := 0;
  got text;
  want_problem text;
  got_problem text;
begin
  for e in select value from jsonb_array_elements(v -> 'canonical_key') loop
    n := n + 1;
    got := mmo.canonical_key(e ->> 'name');
    if got is distinct from (e ->> 'db_key') then
      raise exception 'FAIL canonical_key(%): beklenen %, gelen %', e ->> 'name', e ->> 'db_key', got;
    end if;
    if got is not null then
      if mmo.is_reserved_key(got) is distinct from (e ->> 'db_reserved')::boolean then
        raise exception 'FAIL is_reserved_key(%): beklenen %', got, e ->> 'db_reserved';
      end if;
      if got is distinct from (e ->> 'ue_canonical_key') then
        raise exception 'FAIL ue/db anahtari ayrisiyor (%): ue %, db %', e ->> 'name', e ->> 'ue_canonical_key', got;
      end if;
      want_problem := case when (e ->> 'db_reserved')::boolean then 'reserved_name' else null end;
      got_problem := mmo.name_problem(got, e ->> 'name');
    else
      want_problem := 'bad_name';
      got_problem := mmo.name_problem('x', e ->> 'name');
    end if;
    if got_problem is distinct from want_problem then
      raise exception 'FAIL name_problem(%): beklenen %, gelen %', e ->> 'name', want_problem, got_problem;
    end if;
  end loop;
  if n < 30 then
    raise exception 'FAIL: vektor sayisi beklenenden az (%)', n;
  end if;
  raise notice 'OK canonical_key vektorleri: %', n;

  -- p_holder kurali (UE ayni regex'i onceden dogrular): vectors.json "holder"
  n := 0;
  for e in select value from jsonb_array_elements(v -> 'holder') loop
    n := n + 1;
    if mmo.valid_holder(e ->> 'value') is distinct from (e ->> 'valid')::boolean then
      raise exception 'FAIL valid_holder(%): beklenen %', e ->> 'value', e ->> 'valid';
    end if;
    -- regex sozlesmesi: limits.holder_regex ile ayni karar
    if ((e ->> 'value') ~ (v -> 'limits' ->> 'holder_regex')) is distinct from (e ->> 'valid')::boolean then
      raise exception 'FAIL limits.holder_regex(%): beklenen %', e ->> 'value', e ->> 'valid';
    end if;
  end loop;
  if n < 10 then
    raise exception 'FAIL: holder vektor sayisi beklenenden az (%)', n;
  end if;
  raise notice 'OK holder vektorleri: %', n;

  -- ad regex'i (limits.name_regex): SQL canonical_key ile ayni karar (null <=> regex eslesmez)
  for e in select value from jsonb_array_elements(v -> 'canonical_key') loop
    if ((e ->> 'name') ~ (v -> 'limits' ->> 'name_regex')) is distinct from ((e ->> 'db_key') is not null) then
      raise exception 'FAIL limits.name_regex(%): canonical_key ile ayrisiyor', e ->> 'name';
    end if;
  end loop;
  raise notice 'OK name_regex == canonical_key';

  -- satir ici ek kontroller
  if mmo.canonical_key(null) is not null then raise exception 'FAIL canonical_key(null)'; end if;
  if mmo.canonical_key(E'Ali\n') is not null then raise exception 'FAIL: sonda satir sonu kabul edildi'; end if;
  if mmo.canonical_key(E'Ali\nVeli') is not null then raise exception 'FAIL: ortada satir sonu kabul edildi'; end if;
  if mmo.canonical_key('Ali' || chr(160) || 'Veli') is not null then raise exception 'FAIL: NBSP kabul edildi'; end if;
  if mmo.canonical_key(chr(65313) || 'li') is not null then raise exception 'FAIL: tam genislikli A kabul edildi'; end if;
  if mmo.is_reserved_key(null) then raise exception 'FAIL is_reserved_key(null)'; end if;
  if not mmo.valid_key('zz-ok_1') or mmo.valid_key('ZZ') or mmo.valid_key('') or mmo.valid_key(null)
     or mmo.valid_key(repeat('a', 33)) or mmo.valid_key('a b') then
    raise exception 'FAIL valid_key';
  end if;
  if not mmo.valid_holder('srvA:boot-1.x_y') or mmo.valid_holder('srv A') or mmo.valid_holder('') or mmo.valid_holder(null)
     or mmo.valid_holder(repeat('a', 97)) then
    raise exception 'FAIL valid_holder';
  end if;
  raise notice 'OK satir ici yardimci kontroller';
end
$do$;

-- envelope / side dogrulayicilari (birim)
do $do$
declare
  ok jsonb := '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":0,"NextInstanceId":1}}';
begin
  if mmo.envelope_problem(ok, 2) is not null then raise exception 'FAIL envelope ok'; end if;
  if mmo.envelope_problem(ok, 1) is distinct from 'schema_version_mismatch' then raise exception 'FAIL envelope surum'; end if;
  if mmo.envelope_problem(ok, null) is distinct from 'schema_version_mismatch' then raise exception 'FAIL envelope null surum'; end if;
  if mmo.envelope_problem(null, 2) is distinct from 'data_not_object' then raise exception 'FAIL envelope null'; end if;
  if mmo.envelope_problem('[]'::jsonb, 2) is distinct from 'data_not_object' then raise exception 'FAIL envelope dizi'; end if;
  if mmo.envelope_problem('null'::jsonb, 2) is distinct from 'data_not_object' then raise exception 'FAIL envelope json null'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data}', '"x"'), 2) is distinct from 'data_missing' then raise exception 'FAIL Data metin'; end if;
  if mmo.envelope_problem(ok #- '{Data,Gold}', 2) is distinct from 'bad_gold' then raise exception 'FAIL Gold eksik'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Gold}', '-1'), 2) is distinct from 'bad_gold' then raise exception 'FAIL Gold negatif'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Gold}', '2147483648'), 2) is distinct from 'bad_gold' then raise exception 'FAIL Gold int32 tasmasi'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Gold}', '2147483647'), 2) is not null then raise exception 'FAIL Gold int32 siniri'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Gold}', '1.5'), 2) is distinct from 'bad_gold' then raise exception 'FAIL Gold ondalik'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Gold}', '"12"'), 2) is distinct from 'bad_gold' then raise exception 'FAIL Gold metin'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Gold}', 'null'), 2) is distinct from 'bad_gold' then raise exception 'FAIL Gold null'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Gold}', 'true'), 2) is distinct from 'bad_gold' then raise exception 'FAIL Gold bool'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Level}', '0'), 2) is distinct from 'bad_level' then raise exception 'FAIL Level 0'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Level}', '1000'), 2) is distinct from 'bad_level' then raise exception 'FAIL Level 1000'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,Experience}', '-5'), 2) is distinct from 'bad_experience' then raise exception 'FAIL Experience'; end if;
  if mmo.envelope_problem(jsonb_set(ok, '{Data,NextInstanceId}', '0'), 2) is distinct from 'bad_next_instance_id' then raise exception 'FAIL NextInstanceId'; end if;
  if mmo.side_problem(null) is not null then raise exception 'FAIL side null'; end if;
  if mmo.side_problem('{}'::jsonb) is not null then raise exception 'FAIL side bos'; end if;
  if mmo.side_problem('{"daily":{}}'::jsonb) is not null then raise exception 'FAIL side daily'; end if;
  if mmo.side_problem('[]'::jsonb) is distinct from 'side_not_object' then raise exception 'FAIL side dizi'; end if;
  if mmo.side_problem('{"x":{}}'::jsonb) is distinct from 'side_unknown_kind' then raise exception 'FAIL side kind'; end if;
  if mmo.side_problem('{"daily":[]}'::jsonb) is distinct from 'side_bad_value' then raise exception 'FAIL side deger'; end if;
  if mmo.side_problem(jsonb_build_object('daily', jsonb_build_object('pad', repeat('x', 300000)))) is distinct from 'side_bad_value' then
    raise exception 'FAIL side boyut';
  end if;
  raise notice 'OK envelope/side birim kontrolleri';
end
$do$;

rollback;
\echo 'PASS 01_canonical_key'
