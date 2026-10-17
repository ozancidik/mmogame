-- Tek komutla 01..04 (kalici iz birakmaz; hepsi ROLLBACK). 05 eszamanli test iki terminal ister, ayri kosulur.
-- Depo kokunden, psql >= 13, bash/git-bash/Linux/macOS (vectors.json'u `cat` okur):
--   psql "$DB" -v ON_ERROR_STOP=1 -f supabase/tests/run_all.sql
-- Windows cmd/PowerShell'de `cat` yoksa vectors degiskenini disaridan ver ve bu dosya yerine tek tek kos:
--   psql "$env:DB" -v ON_ERROR_STOP=1 -v vectors=(Get-Content -Raw supabase/tests/vectors.json) -f supabase/tests/01_canonical_key.sql
--   (PowerShell'de tirnak tuzaklari olabilir; Git Bash onerilir.)
-- DURUM: HIC CALISTIRILMADI.

\set ON_ERROR_STOP on
\set vectors `cat supabase/tests/vectors.json`

\i supabase/tests/01_canonical_key.sql
\i supabase/tests/02_rpc_scenarios.sql
\i supabase/tests/03_constraints.sql
\i supabase/tests/04_security.sql

\echo 'PASS run_all (01-04). Eszamanlilik: 05_concurrency_a/b/check ayri kosulur.'
