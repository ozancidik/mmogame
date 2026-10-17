-- Yalniz DUZ (Supabase olmayan) yerel PostgreSQL icin: Supabase'in hazir getirdigi rolleri yoksa olusturur.
-- Supabase projesinde CALISTIRMA (roller zaten var; kosul koruyucu oldugu icin zararsiz ama gereksiz).
-- Rol olusturma kume capindadir (veritabanindan bagimsiz); test sonrasi istenirse elle silinir.
-- Kullanim: psql "$DB" -v ON_ERROR_STOP=1 -f supabase/tests/00_local_roles.sql
-- Sonra migration: psql "$DB" -v ON_ERROR_STOP=1 -f supabase/migrations/20261017120000_v2_character_store.sql
-- DURUM: HIC CALISTIRILMADI.

\set ON_ERROR_STOP on

do $$
begin
  if not exists (select 1 from pg_roles where rolname = 'anon') then
    create role anon nologin noinherit;
  end if;
  if not exists (select 1 from pg_roles where rolname = 'authenticated') then
    create role authenticated nologin noinherit;
  end if;
  if not exists (select 1 from pg_roles where rolname = 'service_role') then
    create role service_role nologin noinherit bypassrls;
  end if;
end
$$;
