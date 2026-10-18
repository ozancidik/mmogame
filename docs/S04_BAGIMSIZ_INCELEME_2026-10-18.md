# S04 Bağımsız İnceleme (2026-10-18)

Kapsam: mimari A (UE `FMMODbCharacterStore` -> `Scripts/MMOPersistenceService.py` -> PostgreSQL/SQLite), Codex adayı `bfc197d` (ana dalda yok; aday aynı ağaç). Bu belge `docs/S04_ADR_VE_KABUL_PLANI.md` K1-K8 kutularının kanıt kaydıdır. Kabul DEĞİLDİR. Kod değiştirilmedi; `docs/SIX_MAP_SPRINT_BOARD.md` ve `docs/tasks/` bu belgeye dahil değildir (Codex'in).

Kaynaklar: (1) bağımsız salt okunur QA incelemesi (SQLite'ta çalıştırdı), (2) `ag-guvenlik-denetcisi` (kodu okudu, ÇALIŞTIRMADI), (3) orkestratörün kendi kod taraması. `svc:` = `Scripts/MMOPersistenceService.py`. Gerçek proje ref'i/anahtarı bu belgede yoktur.

---

## 1. Özet

- A'nın SQLite motoru ve HTTP sözleşmesi çalışıyor: `TestPersistenceService.py` (UE python 3.11.8) `PASS 152 checks across 15 cases`, exit 0.
- Ama aday kabule hazır değil: **güvenlik kriteri FAIL**. Servisde sunucu kimlik doğrulaması yok (tüm uçlar açık), `expectedRevision` null/-1 ile CAS atlanıyor, `ownershipEpoch` istemciden geliyor, şemada RLS yok.
- PostgreSQL dalının hiçbiri koşmadı (sürücü/DB yok). 152 kontrol PG hakkında hiçbir şey kanıtlamaz.
- UE tarafı (118/119) bu incelemede doğrulanmadı; UE derlemesi yapılmadı. `SaveWithOptions` kodu sıkı doğrulama yapmıyor.
- Test sayıları belgelerde üç zaman diliminden karışık: S04_01 97, S04_02 97 ve 118/119, güncel kod 152. Sayı tek başına ölçüt değildir.

## 2. K1-K8 son durum

| Kutu | Durum | Kanıt (özet) |
|---|---|---|
| K1 PostgreSQL entegrasyon | BLOCKED | Sürücü/DB yok (psycopg/psycopg2/pg8000 yok); PG dalı hiç koşmadı; testte postgres geçmiyor |
| K2 Eşzamanlılık | NOT TESTED (kodla riskli) | Çok süreçli test yok; satır yokken `SELECT FOR UPDATE` kilitlemez; advisory lock yok; READ COMMITTED |
| K3 Idempotency + epoch | FAIL (SQLite'ta açık revizyonla PASS) | CAS: expected=None -> 200 rev3, -1 -> rev4 (svc:1275, 1414); epoch istemci girdisi |
| K4 Hata/kurtarma | FAIL / NOT TESTED | R1 uzlaştırma yok, kira yenileme yok, `SaveWithOptions` bilinmeyen outcome, senkron WinHTTP |
| K5 UE uçtan uca | NOT TESTED | Koşu yok, UE derlenmedi |
| K6 Güvenlik | FAIL | Kimlik doğrulama yok, admin uçları açık, shutdown kapısız |
| K7 Şema/migration | FAIL | RLS yok, tablo adı tutarsız, sürüm/down yok, import tamlığı kanıtsız |
| K8 Yük | NOT TESTED | Ölçüm yok |

Aynı satırlar ADR belgesinde (`docs/S04_ADR_VE_KABUL_PLANI.md` K1-K8 "Son durum") işlendi. Kanıt kaydı: aday `bfc197d`, ortam: Windows 11, UE 5.7 python 3.11.8, SQLite; doğrulayan: bağımsız QA + güvenlik denetçisi; tarih 2026-10-18.

### 2.1 152 kontrolün KANITLADIĞI ve KANITLAMADIĞI
- Kanıtladı: SQLite motoru + HTTP sözleşmesi; açık revizyonla CAS; epoch çit; R1 idempotency tekrarı; import idempotency; mock Steam.
- Kanıtlamadı: PostgreSQL dallarının hiçbiri; expected None/-1 reddi (tersine, kabul ettiği koşuldu); kimlik doğrulama yokluğu; eşzamanlılık/çok süreç; UE tarafı 118/119.

### 2.2 Orkestratörün kendi doğruladığı noktalar (kod taraması)
- Servis dosyasında Authorization/Bearer/HMAC/compare_digest/mTLS geçen satır yok (yalnız `steam_web_api_key`).
- `POST /v1/admin/shutdown` (svc:4795-4797) hiçbir kapıya bağlı değil; `allow_fault_injection` bile aranmıyor.
- CAS koşulu `expected_revision is not None and expected_revision >= 0` (svc:1275 SQLite, svc:1414 PostgreSQL): null/-1 karşılaştırmayı atlıyor.
- Varsayılan bind 127.0.0.1:18080 (svc:5151-5152); yani varsayılanda yalnız yerel makine, ama aynı makinedeki her süreç tüm uçlara erişir.

### 2.3 QA'nın SQLite'ta çalıştırdığı kanıtlar
rev2 iken expected=None -> 200 rev3; expected=-1 -> rev4; expected=1 -> 409. Varsayılan bayraklarla loopback'te `set-admin` 200 (olmayan hesabı admin yaptı), `GET /v1/accounts` 200, `shutdown` 200 ve servisi kapattı.

## 3. Codex'e iletilecek DÜZELTME LİSTESİ

Satır numaraları `bfc197d` içindir; kaymış olabilir. Her maddenin kabul ölçütü bağımsız QA tarafından yeniden koşulur; geliştiren kendi maddesini işaretlemez.

### P0 (kapı koşulları; kapanmadan A kabul edilmez)
| # | Madde | Yer | Kabul ölçütü / test |
|---|---|---|---|
| P0-1 | Sunucu-servis kimlik doğrulaması (mTLS veya HMAC/Bearer, sabit zamanlı karşılaştırma). Tüm uçlar | HTTP handler svc:4755-5143 | Kimliksiz/yanlış kimlikli istek her uçta 401/403; negatif test her uç için |
| P0-2 | Oyuncu / sunucu / admin uç ayrımı (set-admin, claim-character, `GET /v1/accounts`, release-session, servers/register, consume-ticket, `/v1/import/snapshot` yalnız ilgili role) | svc:4253-4650, 3801-4252 | Rol matrisi testi: sunucu anahtarıyla admin ucu reddedilir |
| P0-3 | `expectedRevision` ZORUNLU; null/-1/eksik/tipi yanlış -> 400/409, CAS atlama yok | svc:1275, 1414 (+ eski `/v1/characters/save`) | expected=None/-1/"x" reddi; SQLite VE PG |
| P0-4 | `ownershipEpoch` sunucuda kiradan türetilir (`body.get("ownershipEpoch",1)` kalkar); yazan `server_id` kira sahibi olmalı; hesap doğrulanır | karakter yazma svc:1180-1463 | INT64_MAX epoch gönderen istek epoch'u yükseltemez; kira sahibi olmayan sunucu yazamaz |
| P0-5 | `/v1/admin/shutdown` kaldır veya admin kimliği + açık bayrakla kapıla | svc:4795-4797 | Varsayılan yapılandırmada 404/403 |
| P0-6 | `allow_fault_injection` ve `--allow-mock-steam` üretim profilinde BAŞLAMAYI reddeder (`MMOSTEAM:<steamId>` ve skip* bayrakları) | svc:2498-2721, bayrak ayrıştırma | Üretim profili + bayrak -> süreç hata ile çıkar; test |
| P0-7 | Şema güvenliği: ayrı şema (public değil), en az yetkili rol, RLS + public kapalı; DSN yalnız ortam değişkeni (komut satırında değil); `_pg_connect` hata metni istemciye dönmez, genel hata gövdesi | `Migrations`/V1-V4 SQL, svc `_pg_connect`, genel handler | Anon anahtarla `session_token` okunamaz; 503/500 gövdesinde host/kullanıcı/db yok; Steam/DB sırları günlükte yok |
| P0-8 | Steam çağrısı global RLock DIŞINA; HTTP istek zaman aşımı + bağlantı/hız sınırı (ThreadingMixIn sınırsız iş parçacığı) | steam_login, handler | Yavaş Steam'de diğer isteklerin gecikmesi ölçülür (kilit tutulmaz) |
| P0-9 | `POST /v1/import/snapshot` istemcinin verdiği yolu okumaz (yol beyaz listesi/yapılandırma kökü); hata mesajı yol içermez | svc:2197-2491, 4652-4747 | Yol dışı girdi reddi; dizin varlığı sızmaz |

### P1
| # | Madde | Yer | Kabul ölçütü / test |
|---|---|---|---|
| P1-1 | UE `SaveWithOptions` sıkı doğrulama: `Committed == Expected+1` aksi halde reddet (beklenen 3 iken 50 dönerse başarı + cache 50 olmaz) | `MMODbPersistenceClient.cpp:925` | Native test: yanlış committed -> fail, cache değişmez |
| P1-2 | Bilinmeyen outcome bool başarı sayılmaz | aynı dosya | Test: unknown -> başarısız/uzlaştırma bekler |
| P1-3 | Async WinHTTP: SaveAsync/LoadAsync FTSTicker içinde senkron çağrı yapmaz (oyun thread'i 3000 ms x 4 aşama) | istemci | Kare süresi ölçümü (K4f) |
| P1-4 | R1 kayıp ACK uzlaştırma: aynı idempotency anahtarıyla yeniden deneme ya da sorgu; zaman aşımı sonrası yeni GUID üretme | istemci + svc idempotency 959-1072 | Test: ACK kaybı -> yazım ne kaybolur ne ikilenir (`reconcil` testi yok, eklenmeli) |
| P1-5 | Kira yenileme servis tarafında (şu an yalnız `consume_entry_ticket`'te, 60 sn); aynı hesabın iki sunucuda online olması doğrulanmalı | svc:2723-3800 | İki sunucu aynı karakteri sahiplenemez testi |
| P1-6 | PostgreSQL yarışları: ilk kayıtta PK ihlali -> 409 (503 değil); idempotency anahtarı için aynı; advisory lock ya da `INSERT ... ON CONFLICT`; bağlantı havuzu | svc:1326-1463 | Gerçek PG'de iki süreçli test (K2) |
| P1-7 | Migration: şema sürüm tablosu, V2-V4 her başlangıçta yeniden çalışmaz, V1 atlama şema nitelemeli, down betiği, SQLite DDL ile `.sql` tek kaynak | svc:601-958 | Temiz DB'de V2->V3->V4, tekrar zararsız, sürüm kaydı (K7b-c) |
| P1-8 | İçe aktarma: kaynak SHA-256 serviste yeniden hesaplanır; eksik/hatalı enumeration RAPORLANIR (klasör yoksa sessiz atlama yok, boş snapshot `ok 0 öğeler` olmaz); batch geri alma | svc:2197-2491 | Kasıtlı bozuk girdi testi (K7e-f) |
| P1-9 | Eski `/v1/characters/save`: `ownershipEpoch` sayısal değilse 500 -> 400; kanonik anahtar `parse_slot_identifier` ile aynı | svc (legacy save) | Negatif test |
| P1-10 | `consume_entry_ticket` `serverId` istemci gövdesinden alınmaz (kimlikten) | svc:3801-4252 | P0-1 ile birlikte test |

### P2
- Modülerlik: 5211 satır tek dosya; `PersistenceEngine` ~4000 satır (601-4650). Önerilen sınırlar: sabitler/doğrulayıcılar 37-600, motor çekirdeği+DDL 601-958, idempotency 959-1072, karakter/ırk/yan veri/posta depoları 1074-2196, import 2197-2491 ve 4652-4747, Steam 2498-2721, oturum/bilet/kira 2723-3800, bölge sunucusu/seyahat 3801-4252, sahiplik/admin/hesap 4253-4650, HTTP handler 4755-5143. Her işlemde SQLite ve PG dalı kopya (örn. `save_character` 1180-1323 ve 1326-1463): PG'ye özgü hata SQLite testinde görünmez; ortak soyutlama + PG testi.
- Erişim/denetim logu (`log_message` boş); gizli bilgi taraması.
- Bağlantı havuzu (her çağrı yeni bağlantı).
- `/health` PostgreSQL'e bakmaz.
- Idempotency özeti `expectedRevision`/`ownershipEpoch` kapsamaz; kayıt hesap/sunucuya bağlı değil (başka istemci anahtarı yoklayabilir).
- Tablo adı tutarsızlığı: S04_01 kaydı `characters`/`write_idempotency`/`save_import_log` + `writer_epoch`; `V2__character_store.sql` `character_saves`/`persistence_idempotency`/`legacy_import_ledger` + `ownership_epoch` (`characters` V1 tablosu; `write_idempotency` ve `save_import_log` hiçbir yerde yok). Tek isim seti seçilip belgeler düzeltilir.
- Kullanılmayan `presence`/`travel_tickets` tabloları: kullanılacaksa bağla, değilse kaldır.
- UE istemcisi kimlik göndermiyor (P0-1 ile birlikte), TLS pinning yok, `http://` kabul, WinHTTP senkron 3000 ms; `requirements`/sürüm sabitleme yok.
- O4: ad kuralı `MMONameRules` ile uyumlu görünüyor; Unicode kenar durumları iki taraflı fixture ile sınanmadı.
- Test sayısı belge uyumu: S04_01 97, S04_02 97 ve 118/119 eski; güncel 152 tek yerde yazılır.

### Güvenlik kapı koşulları (7, hepsi geçilmeden K6 PASS olmaz)
1. Sunucu kimliği mTLS/HMAC. 2. Oyuncu/sunucu/admin uç ayrımı. 3. `expectedRevision` zorunlu + epoch sunucuda kiradan türetilir + yazan `server_id` kira sahibi. 4. Zaman aşımı/bağlantı sınırı/Steam çağrısı kilit dışı. 5. Ayrı şema + en az yetkili rol + RLS + public kapalı + DSN yalnız ortam + genel hata gövdesi. 6. Fault-injection/mock-steam üretimde başlamayı reddeder. 7. Denetim logu.

### Güvenlik bulguları (denetçi; kodu okudu, çalıştırmadı)
- KRİTİK: K-1 sunucu-servis kimlik doğrulaması yok (PUT /v1/characters/<slot>, set-admin, claim-character, GET /v1/accounts, release-session [serverId opsiyonel, kira koşulsuz silinir], servers/register [sahte harita sunucusu], consume-ticket); K-2 admin/shutdown kapısız; K-3 epoch istemci girdisi, INT64_MAX ile kalıcı kilitleme (DoS).
- YÜKSEK: Y-1 expectedRevision null/-1 CAS atlama (QA koşuyla doğruladı); Y-2 Steam girişi global RLock altında 3 sn harici HTTP, handler timeout yok, sınırsız iş parçacığı; Y-3 import/snapshot istemci yolu.
- ORTA: O-1 `allow_fault_injection` varsayılan kapalı ve istekle açılamaz, ama bayrak açıksa kimliksiz mutasyon; `--allow-mock-steam` açıksa `MMOSTEAM:<steamId>` ile her bilet; O-2 DSN `--pg-dsn` komut satırında, `_pg_connect` hata metni 503 gövdesinde, genel handler `str(exc)` ile 500; O-3 servis her başlangıçta V1-V4 DDL çalıştırır (DDL yetkili rol), V1-V4'te RLS/POLICY/GRANT/REVOKE yok, tablolar `public`'te (`session_token CHAR(64)` dahil): Supabase'de public PostgREST'e açıksa anon anahtarla okunur/yazılır; Claude hattının `mmo` şeması RLS açık/`mmo_api`'de, Python V2-V4 değil; O-4 ad kuralı.
- DÜŞÜK: SQL enjeksiyonu YOK (parametrik; tek dinamik SQL sabit liste). Diğerleri P2'de.

## 4. Claude Supabase hattından A'da yeniden kullanılabilecekler (B kararı değildir)

- `mmo` şeması + RLS açık + `mmo_api` deseni (P0-7 için model).
- Kilit sırası, SECURITY DEFINER + `search_path` kuralı.
- `supabase/tests/vectors.json` sözleşme vektörleri (20 senaryo), `05_concurrency` testleri (K2 için taban), `zz_cleanup`.
- `Scripts/TestSupabaseIntegration.ps1`: kullanıcı tarafından çalıştırıldı, `PASS checks=132 skipped=7`, 20 senaryo (QA incelemesi commit mesajına bakıp "çalıştırılmadı" demişti; bu ARTIK ESKİ). Bu SQL sözleşmesi içindir, A'nın kanıtı değildir.
- UE HTTP soyutlaması ve sahte sunucu (599 kontrol; yalnız sahte sunucuyla).
- Not: `supabase/tests/vectors.json` `_doc` alanı hâlâ "HICBIR gercek DB'ye karsi calistirilmadi" diyor (satır 3); bu ESKİ/yanlış. **vectors.json `_doc` güncellenmeli (belge/SQL sahibi); bu belge o dosyaya dokunmaz.**

B için Claude'un açık borçları (tarafsız): UE'den gerçek Supabase'e bağlantı YOK; tek paylaşılan secret key riski gerçek ama anahtarsız istek reddedilir (RLS, `mmo_api`, `User-Agent` kuralı ile kanıtlı); yan veri/posta/içe aktarma aracı/çağıran bağlama yok.

## 5. Doğrulanamayanlar ve sınırlar

- Okunan, çalıştırılmayan: güvenlik denetçisi bulguları (kod okuma); orkestratör doğrulamaları (kod taraması).
- Çalıştırılan: `TestPersistenceService.py` (152 kontrol, SQLite) ve QA'nın elle HTTP koşuları (SQLite, loopback).
- Hiç koşmayan: PostgreSQL dallarının tamamı; çok süreçli/eşzamanlılık; servis öldürme senaryosu; UE derlemesi ve `TestDbStore.ps1` (118/119 bu aday için doğrulanmadı); yük; gerçek dedicated server uçtan uca.
- Supabase üzerinde servisin psycopg bağlantısı (IPv4/pooler kısıtı) DOĞRULANMADI.
- Satır numaraları `bfc197d` içindir.

## 6. Sonraki adımlar

1. Kullanıcı kararı: PostgreSQL test ortamı (ayrı dev/test, üretim değil) sağlanır mı (K1, K2 BLOCKED'ı açar).
2. Codex: yalnız P0 düzeltmeleri (yeni özellik yok), sonra P1.
3. P0 sonrası aynı kriterlerle yeniden bağımsız QA + güvenlik denetimi; kutular yalnız onların kanıt kaydıyla değişir.
4. P0'lar pencere sonunda hâlâ FAIL ise ADR §1.2 gereği B aynı K1-K8 ile değerlendirilir (O5 eşiği).
