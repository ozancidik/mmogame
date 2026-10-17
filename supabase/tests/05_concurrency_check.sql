-- 05 (kontrol + temizlik): A ve B kosulduktan sonra durumu dogrular ve zz-conc verisini siler.
-- Beklenen: karakter revision 1 (iki esozamanli yazimdan YALNIZ biri etki etti), A'nin verisi duruyor (Gold 150),
-- write_ops'ta 'create' + 'save' = tam 2 kayit (reddedilen/tekrarlanan islem ek kayit uretmedi).
-- DURUM: HIC CALISTIRILMADI.

\set ON_ERROR_STOP on

do $do$
declare
  r mmo.character_saves;
  n int;
begin
  select * into r from mmo.character_saves where canonical_key = 'zz-conc';
  if not found then raise exception 'FAIL: zz-conc yok (A calistirildi mi?)'; end if;
  if r.revision <> 1 then raise exception 'FAIL: revision % (1 beklenir)', r.revision; end if;
  if r.data #>> '{Data,Gold}' <> '150' then raise exception 'FAIL: A verisi degisti: %', r.data; end if;
  select count(*) into n from mmo.write_ops where canonical_key = 'zz-conc';
  if n <> 2 then raise exception 'FAIL: write_ops kayit sayisi % (2 beklenir)', n; end if;
  delete from mmo.write_ops where canonical_key = 'zz-conc';
  delete from mmo.character_saves where canonical_key = 'zz-conc';
end
$do$;
\echo 'PASS 05_concurrency_check (zz-conc temizlendi)'
