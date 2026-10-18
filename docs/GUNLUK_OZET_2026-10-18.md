# Günlük özet — 2026-10-18 (Claude, Supabase hattı)

Önceki özet: `GUNLUK_OZET_2026-10-17.md` (Supabase bölümü 17 Ekim'e kadar). Bu belge yalnız Supabase hattının 17 Ekim akşamı ve 18 Ekim gelişmelerini toplar. **Oyun içinde (PIE) hiçbir şey denenmedi. Belge denetçisi derleme, native test veya Supabase betiği KOŞMADI**; sonuçlar sahibinin bildirimi ve commit içeriğiyle eşlendi.

## Yapılanlar

| Alan | Ne | Kanıt |
|---|---|---|
| Güvenlik turu 3 belgeleri | holder benzersizliği (N1), tek purge cron işi (N2), `p_batch` tam sayı (N3), `revision_conflict -> LoadEx` (N4), karar S11 | `47814b0` |
| Dev projesinde ilk doğrulama | Migration SQL Editor'de hatasız; katalog (12 RPC yalnız `service_role`, 6 tablo RLS açık); anahtarsız tablo GET reddi; HTTPS `healthcheck`; tarayıcı-UA reddi kuralı belgelendi | `7ad8cbc` |
| UE açık User-Agent | `FMMOSupabaseConfig::DefaultUserAgent` `MMOGameServer/0.1` her isteğe eklenir; `Validate` tarayıcı kelimeli/boş/uzun UA'yı reddeder; 401/403'te sabit ipucu logu; test 556 -> 599 kontrol (kodda `SupabaseTestExpectedChecks = 599`, `TestSupabaseStore.ps1` `$expected = 599`). Yalnız sahte sunucuya karşı | `d3d56ec` |
| Dilim 6 betiği | `Scripts/TestSupabaseIntegration.ps1`, `supabase/tests/zz_cleanup.sql`, README bölümü | `1399123` |
| Dilim 6 SONUCU (kullanıcı koşusu, gerçek dev) | `[MMO_SUPABASE_INTEGRATION] PASS checks=132 skipped=7`; 20/20 senaryo PASS; `healthcheck` `api_version=2 contract_version=2`. Kısmi: `expired_lease` (1 adım koştu, 5 SKIP), `idempotent_same_payload` (5 koştu, 2 SKIP); SKIP nedeni `expire_lease` yönetici adımının API ile yapılamaması | kullanıcı bildirimi; `supabase/README.md` "dev'de 2026-10-18 sonucu" |
| Temizlik | `zz_cleanup.sql` SQL Editor'de koştu: `character_side` 2, `presence` 0, `travel_tickets` 0, `write_ops` 28, `character_saves` 19 satır silindi; gerçek veriye dokunulmadı | kullanıcı bildirimi |
| Hesap durumu | FREE katman, West EU (Ireland), tek proje (dev) | kullanıcı bildirimi, `DECISIONS_NEEDED.md` S4/S5 |
| Belge (bu tur) | `DESIGN_SUPABASE.md` (DURUM bloğu, Dilim 6 = koştu, yeni §10 "S04 mimari çakışması"), `supabase/README.md`, `DECISIONS_NEEDED.md` (yeni karar "Kalıcılık mimarisi", S1 notu), `PLAYTEST_CHECKLIST.md` (madde 7, 9), `CLAUDE.md` (Supabase satırı), bu özet | bu commit |
| Denetim | `ContentMap.py --check` "koddan üretilenle aynı"; `DocCounts.py` 59 mob / 97 görev / 57 ekipman / 93 beceri, sapma 0; `PocketSpacing.py` 18 cep, 0 hata, 2 bilinen uyarı | 2026-10-18, belge denetçisi |

## YENİ BULGU: S04 mimari çakışması (karar bekliyor)

Codex/Antigravity hattı S04'ü kendi mimarisiyle uyguladı: UE `FMMODbCharacterStore` (`-MMOStore=db`) -> Python servisi `Scripts/MMOPersistenceService.py` -> PostgreSQL/SQLite, şema `Migrations/V2__character_store.sql`. Bu kod **`master`'da değil** (`c4a18d6`, `b9297b4`, `e481114`, `bfc197d` `master` atası değil); aday worktree'lerde. Belge denetçisi `git show bfc197d:<yol>` ile okudu, çalıştırmadı.

- **Aynı kavramlar iki kez çözülmüş:** revision CAS, sahiplik epoch'u, idempotency anahtarı, içe aktarma. İki şema (`public.character_saves` ve `mmo.character_saves`), iki UE istemcisi, iki sözleşme dosyası.
- **Kanıt durumu:** Codex: servis 97 kontrol, UE 118/119 native, hepsi SQLite; canlı PostgreSQL kabulü pano: AÇIK. Claude: SQL sözleşmesi gerçek Supabase'e karşı 132 PASS / 7 SKIP; UE 599 yalnız sahte sunucuya. Codex'in sayıları panodan aktarıldı, koşulmadı.
- **Fark:** Codex'in kapsamı daha geniş (ırk/başarım/günlük/posta yan verisi, `.sav` içe aktarma, çağıran bağlama, Steam/bilet kodu serviste); benimki yalnız karakter + kira + `import_character` SQL'i, hiçbir oyun yoluna bağlı değil. Codex V2 şemasında RLS yok (dosyada 0 eşleşme).
- **Öneri (karar kullanıcıda): önce D (karar gelene kadar dosya deposu, yeni kod yok), sonra A (Supabase = barındırılan PostgreSQL; Codex servisi ona bağlanır).** Tam tablo ve gerekçe: `DESIGN_SUPABASE.md` §10; soru metni: `DECISIONS_NEEDED.md` "Kalıcılık mimarisi".
- Codex belge tutarsızlığı not edildi, düzeltilmedi: `S04_01` kaydı tablo adlarını `characters`/`write_idempotency`/`save_import_log` diye yazıyor, SQL `character_saves`/`persistence_idempotency`/`legacy_import_ledger`.

## Kim neye dokundu

- **Claude, Supabase (trailer'lı):** `47814b0`, `7ad8cbc`, `d3d56ec` (`MMOSupabaseRestClient.*`, `MMOUnrealHttpTransport.cpp`, `MMOSupabaseStoreSelfTest.cpp`, `TestSupabaseStore.ps1`), `1399123` (`Scripts/TestSupabaseIntegration.ps1`, `zz_cleanup.sql`, README), bu belge commit'i (yalnız belge).
- **Codex:** S04 kabul/düzeltme kayıtları (`docs/SIX_MAP_SPRINT_BOARD.md`, `docs/tasks/`), aday worktree'lerde servis/istemci kodu; bu tur dosyalarına dokunulmadı.
- **Commit'siz, ana kopyada (dokunulmadı):** `MMOCharacter.cpp/.h`, `MMOPlayerController.cpp`, `MMOHUD.*`, `Scripts/RunSystemTests.ps1`, staged Trade dosyaları (`TestTradeTransfer.ps1`, `MMOTradeComponent.cpp`, `MMOTradeTransferSelfTest.cpp`), izlenmeyen `MMORace*`/`MMOZoneLayout*`/`TestRace.ps1`, `.agents/`, `.codex/agents/*`.

## Dürüst riskler

- UE <-> gerçek Supabase hiç denenmedi; 599 kontrol sahte sunucuya karşı. Motorun fiilen gönderdiği UA başlığı doğrulanmadı.
- `run_all.sql` (psql), eşzamanlılık testleri ve kira süresi dolma yolu gerçek DB'de koşulmadı; 132 PASS'in 7 SKIP'i tam bu yolu kapsıyor.
- Dilim 6 bir PowerShell betiğidir: HTTPS sözleşmesini sınar, UE istemcisini ve oyun akışını sınamaz.
- Free katman: duraklatma/yedek davranışı, Supabase limit/fiyat [Doğrulanmadı]. Tek paylaşılan secret key (K-1) ve R1 açık.
- İki mimari paralel büyürse çifte iş ve çakışan çağıran geçişleri doğar; karar gecikmesinin bedeli bu.
- Ana kopya `MMOZoneLayout*`/`MMORace*` sahipsiz dosyalar yüzünden hâlâ derlenmiyor olabilir (bu tur derleme denenmedi).

## Açık kararlar

- **Kalıcılık mimarisi A / B / C / D** (`DECISIONS_NEEDED.md`), S1 buna bağlı; S2 (servis dili) Codex'in fiili Python seçimiyle bağlantılı.
- Supabase: S5 alfa katmanı (Pro?), S6 yedek, S7 tek yönlü geçiş, S8 kimlik, S9 migration kaynağı, S11 holder üretimi; kullanıcı adımları (psql kurulumu + `run_all.sql`, MFA, legacy JWT kapatma, Auth signup kapatma).
- Önceki günlerden: Birleştirme A1-A4, sahipsiz `MMOZoneLayout*`/`MMORace*`, push hook, S15.
