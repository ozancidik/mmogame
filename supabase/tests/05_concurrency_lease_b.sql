-- 05 (kira zamani, B oturumu): 05_concurrency_lease_a.sql calisirken ikinci terminalde kosulur. Ayrinti: A dosyasinin basligi.
-- B, kira hala gecerliyken save_character cagirir (A satir kilidini tutuyor -> B bekler). Beklenen sonuc: ownership_lost
-- (A commit edince kira dolmus; zaman kilit SONRASI alindigi icin). B en az ~3 sn beklemeli.
-- DURUM: HIC CALISTIRILMADI.

\set ON_ERROR_STOP on

select (lease_until > clock_timestamp()) as started_in_time
  from mmo.character_saves where canonical_key = 'zz-conc-lease' \gset
\if :started_in_time
\else
  \echo 'FAIL 05_concurrency_lease_b: kira B baslamadan once zaten bitmis (A calisiyor muydu? 6 sn icinde baslat). Testi bastan tekrarla.'
  \quit 1
\endif

select clock_timestamp() as t0 \gset
select mmo_api.save_character('zz-conc-lease', 'srvA:boot1', 1, 0, 'c0000000-0000-4000-8000-000000000012', 2,
  '{"SchemaVersion":2,"Data":{"Level":9,"Experience":99,"Gold":999,"NextInstanceId":9}}'::jsonb) as b_result \gset
select clock_timestamp() as t1 \gset

select (:'b_result'::jsonb = '{"outcome":"ownership_lost","lease_epoch":1,"epoch":1}'::jsonb) as shape_ok \gset
select (extract(epoch from (:'t1'::timestamptz - :'t0'::timestamptz)) >= 3) as waited_ok \gset
select (revision = 0 and (data #>> '{Data,Gold}') = '100') as untouched_ok
  from mmo.character_saves where canonical_key = 'zz-conc-lease' \gset

\echo 'B sonucu:' :b_result
\if :shape_ok
\else
  \echo 'FAIL 05_concurrency_lease_b: ownership_lost beklenirdi (zaman kilit oncesi alinmis olabilir)'
  \quit 1
\endif
\if :waited_ok
\else
  \echo 'FAIL 05_concurrency_lease_b: cagri A''nin kilidini beklemedi'
  \quit 1
\endif
\if :untouched_ok
\else
  \echo 'FAIL 05_concurrency_lease_b: kayit B tarafindan degistirilmis'
  \quit 1
\endif

delete from mmo.write_ops where canonical_key = 'zz-conc-lease';
delete from mmo.character_saves where canonical_key = 'zz-conc-lease';
\echo 'PASS 05_concurrency_lease_b (zz-conc-lease temizlendi)'
