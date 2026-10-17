-- zz_cleanup.sql - Scripts/TestSupabaseIntegration.ps1'in yazdigi test verisini siler.
--
-- !!! UYARI !!!  YALNIZ DEV PROJESI. Supabase SQL Editor'de (postgres rolu) calistirilir.
-- Yalniz canonical_key'i 'zz-test-' ile BASLAYAN satirlara dokunur. On eksiz / baska onekli silme YOK (starts_with, LIKE degil:
-- '_' ve '%' joker olmasin). Bu dosyayi baska bir sorguyla birlestirme; dev disi projede calistirma.
--
-- Tek ifade (tek islem): ya hepsi silinir ya hicbiri. Calismadan once ne silinecegini gormek istersen ayri bir sorgu olarak:
--   select count(*) from mmo.character_saves where starts_with(canonical_key, 'zz-test-');
--
-- Sira (FK): character_side / presence / travel_tickets -> character_saves (bunlar ON DELETE CASCADE; yine de acik silinir ve
-- sayilir). write_ops'un FK'si yok, canonical_key kolonuyla eslesir (idempotency gunlugu: op_id -> canonical_key iliskisi).
-- mmo.accounts'a DOKUNULMAZ (test accounts yazmaz).
-- Donus: tek satir, silinen satir sayilari.

with
del_side as (
  delete from mmo.character_side
  where starts_with(canonical_key, 'zz-test-')
  returning 1
),
del_presence as (
  delete from mmo.presence
  where starts_with(canonical_key, 'zz-test-')
  returning 1
),
del_tickets as (
  delete from mmo.travel_tickets
  where starts_with(canonical_key, 'zz-test-')
  returning 1
),
del_ops as (
  delete from mmo.write_ops
  where starts_with(canonical_key, 'zz-test-')
  returning 1
),
del_saves as (
  delete from mmo.character_saves
  where starts_with(canonical_key, 'zz-test-')
  returning 1
)
select
  (select count(*) from del_side)     as character_side_deleted,
  (select count(*) from del_presence) as presence_deleted,
  (select count(*) from del_tickets)  as travel_tickets_deleted,
  (select count(*) from del_ops)      as write_ops_deleted,
  (select count(*) from del_saves)    as character_saves_deleted;
