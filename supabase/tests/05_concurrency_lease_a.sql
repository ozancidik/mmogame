-- 05 (kira zamani, A oturumu): "zaman kilit beklendikten SONRA alinir" kuralinin ESZAMANLI kaniti. 05_concurrency_lease_b.sql ile
-- BIRLIKTE, iki ayri terminalde kosulur. KALICI veri yazar (kendi anahtari zz-conc-lease); yalniz dev projede kos.
--
-- Senaryo: kira 10 sn (en kisa izinli sure). A, satir kilidini ~13 sn tutar (SELECT ... FOR UPDATE). B, kira HALA gecerliyken
-- save_character cagirir ve satir kilidinde bekler. A commit edince kira ARTIK dolmustur. Dogru davranis: B ownership_lost alir
-- (zaman kilit sonrasi alinir). Eski/yanlis davranis (zaman cagri basinda): B kirayi gecerli sanip commit ederdi (revision 1).
--
-- Sira:
--   1) Terminal 1:  psql "$DB" -v ON_ERROR_STOP=1 -f supabase/tests/05_concurrency_lease_a.sql
--   2) "A: kilit tutuluyor" yazisini gorur gormez (6 sn icinde) Terminal 2:
--        psql "$DB" -v ON_ERROR_STOP=1 -f supabase/tests/05_concurrency_lease_b.sql
--   B "PASS 05_concurrency_lease_b" yazar ve zz-conc-lease verisini siler.
-- DURUM: HIC CALISTIRILMADI.

\set ON_ERROR_STOP on

delete from mmo.write_ops where canonical_key = 'zz-conc-lease';
delete from mmo.character_saves where canonical_key = 'zz-conc-lease';

select mmo_api.create_character('zz-conc-lease', 'zz-conc-lease', 'srvA:boot1', 'c0000000-0000-4000-8000-000000000011', 2,
  '{"SchemaVersion":2,"Data":{"Level":1,"Experience":0,"Gold":100,"NextInstanceId":1}}'::jsonb) as setup_result;
-- kirayi 10 sn'ye indir (ayni sahip, gecerli kira: epoch 1 kalir)
select mmo_api.acquire_lease('zz-conc-lease', 'srvA:boot1', 10) ->> 'lease_epoch' as epoch_after_acquire;

\echo 'A: kilit tutuluyor (13 sn; kira 10 sn sonra biter). Simdi B oturumunu baslat (6 sn icinde).'
begin;
select 1 from mmo.character_saves where canonical_key = 'zz-conc-lease' for update;
select pg_sleep(13);
commit;
\echo 'A: commit edildi.'
