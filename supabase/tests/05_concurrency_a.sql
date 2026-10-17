-- 05 (A oturumu): ESZAMANLI iki islem testinin "tutucu" tarafi. 05_concurrency_b.sql ile BIRLIKTE, iki ayri terminalde kosulur.
-- Bu test KALICI veri yazar (kendi anahtari zz-conc) ve baslangicta/sonda kendini temizler; yalniz dev projede kos, 'alpha'da KOSMA.
--
-- Senaryo: A, save_character(op ...A2, rev 0) cagirir ve islemi ~10 sn ACIK TUTAR (op_id advisory kilidi + karakter satir kilidi).
-- B bu sirada (a) ayni op_id + ayni yuk: op kilidini bekler, A commit edince "replayed" doner (revision 1, COK etkisi yok) ya da
-- (b) farkli op_id + ayni expected_revision 0: satir kilidini bekler, A commit edince revision_conflict (revision 1) doner.
--
-- Sira:
--   1) Terminal 1:  psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -f supabase/tests/05_concurrency_a.sql
--   2) "A: kilit tutuluyor" yazisini gorur gormez (10 sn icinde) Terminal 2:
--        psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -v mode=same_op  -f supabase/tests/05_concurrency_b.sql
--      sonra A bittikten sonra, A'yi YENIDEN kosup bu kez  -v mode=other_op  ile B'yi calistir.
--   3) HER kosudan (A+B cifti) sonra:  psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -f supabase/tests/05_concurrency_check.sql
-- Beklenen: B "PASS 05_concurrency_b (...)" yazar; check "PASS 05_concurrency_check" yazar.
-- DURUM: HIC CALISTIRILMADI.

\set ON_ERROR_STOP on

-- temizlik (onceki kosudan kalmis olabilir)
delete from mmo.write_ops where canonical_key = 'zz-conc';
delete from mmo.character_saves where canonical_key = 'zz-conc';

-- kurulum (otomatik commit: B oturumu gorebilsin)
select mmo_api.create_character('zz-conc', 'zz-conc', 'srvA:boot1', 'c0000000-0000-4000-8000-000000000001', 2,
  '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":100,"NextInstanceId":1}}'::jsonb) as setup_result;

\echo 'A: kilit tutuluyor (10 sn). Simdi B oturumunu baslat.'
begin;
select mmo_api.save_character('zz-conc', 'srvA:boot1', 1, 0, 'c0000000-0000-4000-8000-000000000002', 2,
  '{"SchemaVersion":2,"Data":{"Level":2,"Experience":10,"Gold":150,"NextInstanceId":2}}'::jsonb) as a_result;
select pg_sleep(10);
commit;
\echo 'A: commit edildi.'
