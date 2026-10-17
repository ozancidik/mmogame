-- 05 (B oturumu): 05_concurrency_a.sql calisirken ikinci terminalde kosulur. Ayrinti ve sira: 05_concurrency_a.sql basligi.
--   -v mode=same_op   ayni op_id + ayni yuk  -> A commit edince BEKLEMEDEN SONRA replayed:true, revision 1 (cift etki yok)
--   -v mode=other_op  farkli op_id + ayni expected_revision 0 -> A commit edince revision_conflict, revision 1
-- Her iki modda cagri A'nin kilidini BEKLEMELI (>= 2 sn); beklemeden donerse kilit sirasi bozuktur (test BASARISIZ).
-- DURUM: HIC CALISTIRILMADI.

\set ON_ERROR_STOP on
\if :{?mode}
\else
  \echo 'HATA: -v mode=same_op veya -v mode=other_op ver.'
  \quit
\endif

select (:'mode' = 'same_op') as is_same \gset
select (:'mode' = 'other_op') as is_other \gset
select (:'mode' in ('same_op', 'other_op')) as mode_ok \gset
\if :mode_ok
\else
  \echo 'HATA: mode same_op veya other_op olmali.'
  \quit
\endif

select clock_timestamp() as t0 \gset

\if :is_same
  select mmo_api.save_character('zz-conc', 'srvA:boot1', 1, 0, 'c0000000-0000-4000-8000-000000000002', 2,
    '{"SchemaVersion":2,"Data":{"Level":2,"Experience":10,"Gold":150,"NextInstanceId":2}}'::jsonb) as b_result \gset
  select (:'b_result'::jsonb @> '{"outcome":"committed","revision":1,"epoch":1,"replayed":true}'::jsonb) as shape_ok \gset
\else
  select mmo_api.save_character('zz-conc', 'srvA:boot1', 1, 0, 'c0000000-0000-4000-8000-000000000003', 2,
    '{"SchemaVersion":2,"Data":{"Level":9,"Experience":99,"Gold":999,"NextInstanceId":9}}'::jsonb) as b_result \gset
  select (:'b_result'::jsonb = '{"outcome":"revision_conflict","revision":1}'::jsonb) as shape_ok \gset
\endif

select clock_timestamp() as t1 \gset
select (extract(epoch from (:'t1'::timestamptz - :'t0'::timestamptz)) >= 2) as waited_ok \gset

\echo 'B sonucu:' :b_result
\if :shape_ok
\else
  \echo 'FAIL 05_concurrency_b: sonuc beklenen sekilde degil'
  \quit 1
\endif
\if :waited_ok
\else
  \echo 'FAIL 05_concurrency_b: cagri A''nin kilidini beklemedi (A calisiyor muydu? 10 sn icinde baslatildi mi?)'
  \quit 1
\endif
\echo 'PASS 05_concurrency_b (' :mode ')'
