# S04 ADR ve Bağımsız Kabul Planı (2026-10-18)

**Durum: PLAN. Bu belge bir KARARIN planıdır, KABUL DEĞİLDİR.** Hiçbir mimari kabul edilmedi; hiçbir kontrol listesi kutusu işaretlenmedi. Bu belge kod yazmaz, test koşmaz, Codex panosuna (`docs/SIX_MAP_SPRINT_BOARD.md`) ve `docs/tasks/` kayıtlarına dokunmaz. Panoya işleme: koordinatör/Codex panoya işler.

Kaynak karar: kullanıcı, 2026-10-18, başka bir yapay zeka analizi doğrultusunda "bu karar doğrultusunda hareket et ve plan çıkar" dedi. Bu belge o kararı olduğu gibi kayda geçirir ve kabul yolunu tanımlar.

Okunan kaynaklar (bu oturumda dosyadan/`git show`dan okundu): `docs/DESIGN_SUPABASE.md` (§10), `docs/DECISIONS_NEEDED.md` (Kalıcılık mimarisi maddesi, S1), `docs/SIX_MAP_SPRINT_BOARD.md` (S04, S05, S06, S12, S13 satırları ve S04 inceleme notları), `supabase/migrations/20261017120000_v2_character_store.sql` (tablo adları), Codex adayı `bfc197d` (`git show` ile). Derlenmedi, koşulmadı.

---

## 1. ADR-S04-001 — Kalıcılık mimarisi

**Durum: KABUL EDİLMEDİ. KARAR: hedef mimari A; kabul, bağımsız doğrulamaya bağlı.**

### 1.1 Bağlam
- Karakter, envanter, para, görev, yan veri (başarım/günlük/posta) ve ırk bugün yerel dosyada (`Saved/`). Kapalı alfa ve 2000+ eşzamanlı oyuncu hedefi için sunucu-yetkili, geri yüklenebilir, tek sahipli veritabanı kalıcılığı gerekir (S04; S05/S06/S07/S12 buna dayanır).
- İki hat birbirinden bağımsız ilerledi (ayrıntı: `DESIGN_SUPABASE.md` §10):
  - **Codex S04 (aday `bfc197d`, `master`'da YOK):** UE `FMMODbCharacterStore` -> Python HTTP servisi `Scripts/MMOPersistenceService.py` -> PostgreSQL (veya test için SQLite).
  - **Claude Supabase (`master`, `c8d81c1`..`1399123`, sonrasında belge `3aa9e8c`):** UE `FMMOSupabaseCharacterStore` -> HTTPS -> Supabase PostgREST `mmo_api` RPC (mantık PL/pgSQL'de).
- Aynı kavramlar (revision CAS, sahiplik epoch'u, idempotency, içe aktarma) iki şema, iki UE istemcisi ve iki sözleşmeyle çözülüyor.

### 1.2 Karar (kullanıcı kararı, 2026-10-18)
1. **Hedef mimari A.** Oyun sunucuları (UE dedicated server) veritabanına DOĞRUDAN erişmez:
   `UE dedicated server -> DbPersistenceClient (UE) -> Python kalıcılık servisi (MMOPersistenceService.py, Codex S04) -> PostgreSQL`.
   Supabase = barındırılan PostgreSQL (yedek vb. özellikler plan katmanına bağlı, bu belgede doğrulanmadı).
2. **Kesin kabul yok.** Önce bağımsız doğrulama (seçenek 3). Doğrulama sonrası tek mimariye geçilir. A kabul kriterlerini (§2) geçmezse sınırlı bir düzeltme süresi tanınır; sonra B (Claude'un Supabase RPC mimarisi, `docs/DESIGN_SUPABASE.md`) AYNI kriterlerle değerlendirilir.
3. **Paralel kalıcılık geliştirmesi durur** (S04 kapsamında): Claude Supabase RPC mimarisini GENİŞLETMEZ ve mevcut RPC/UE REST kodunu SİLMEZ; Codex yeni persistence özelliği EKLEMEZ, yalnız doğrulama/düzeltme yapar. Hiçbir ajan diğerinin kodunu kanıtsız devralmaz. Ana dala birleşme için bağımsız QA kabulü zorunludur.
4. **Rol yapısı:** tek ADR sahibi + veri modeli/API sözleşme sahibi atanır (§4.5). Geliştiren ajan kendi kodunu TEK BAŞINA onaylamaz; bağımsız QA kanıt üretir; test SAYISI tek başına başarı ölçütü değildir.
5. **Sonuç dili:** PASS / FAIL / BLOCKED / NOT TESTED. Her kanıt: commit SHA, ortam, komut, sonuç, log.
6. **Üretim DB'sinde test YOK.** Gerçek PostgreSQL (SQLite DEĞİL) kabul testleri ayrı dev/test ortamında.
7. **Hedef:** 2000+ eşzamanlı oyuncuya büyüyebilir, güvenli, gözlemlenebilir, geri yüklenebilir, TEK sahipli kalıcılık.

### 1.3 Alternatifler (kısa)
| Seçenek | Anlamı | Başlıca artı | Başlıca eksi |
|---|---|---|---|
| **A (hedef)** | UE -> Python servis -> PostgreSQL | Harita sunucusu DB anahtarı görmez; servis Steam anahtarı/bilet için zaten gerekecek; yan veri, posta, `.sav` içe aktarma, çağıran bağlama adayda mevcut | Gerçek PostgreSQL dalı hiç koşulmadı; servis ~5200 satır tek dosya; sıkı-ACK/CAS/importer düzeltmeleri kabul bekliyor |
| **B** | Supabase RPC (mantık SQL'de), Python servis yalnız Steam/bilet | Az hareketli parça; SQL sözleşmesi gerçek Supabase dev'e karşı 132 PASS / 7 SKIP | Yan veri, posta, içe aktarma aracı, çağıran bağlama YOK (~2-3 hafta [TAHMİN]); her harita sunucusunda geniş paylaşılan secret key; Codex emeği atılır |
| **C** | İkisi paralel | -- | Önerilmez: çift şema, çift sözleşme, aldatıcı "iki kanıt" |
| **D** | Karar verilene kadar yalnız dosya deposu | Sıfır risk, geri almasız | DB şartı ertelenir; bu karar D'nin geçici durumunu korur: `-MMOStore=file` varsayılan kalır |

### 1.4 Sonuçlar
- Olumlu: DB kimlik bilgisi tek yerde (serviste); Steam/bilet/kira devri aynı güvenlik sınırında; UE tarafı ince istemci.
- Olumsuz / bedel: ek bir servis işletilecek (barındırma, güncelleme, izleme, çökme kurtarma); servis tek hata noktası olur (ölçeklenme/HA planı gerekir); Supabase'e doğrudan psycopg bağlantısı IPv4/pooler kısıtı olabilir [DOĞRULANMADI].
- Ödenen: B hattının UE katmanı (`e0fffdf`..`d3d56ec`) ve RPC SQL'i donuk kalır (silinmez); "Claude 132 PASS" kavram kanıtıdır, A'nın kanıtı değildir.

### 1.5 Geri alma
- `-MMOStore=file` varsayılan kalır; DB modu bayrakla açılır. A reddedilirse: bayrak kapalı kalır, B değerlendirmesine geçilir.
- B donuk olduğundan (silinmedi) dönüş mümkündür; pano ve S05/S06 tasarımı servise göre yazılı olduğundan B'ye geçiş panoyu etkiler (koordinatör/Codex panoya işler).

### 1.6 Gerekçe
- **Güvenlik:** DB anahtarı yalnız serviste. B'de tek paylaşılan `sb_secret_` anahtarı her harita sunucusunda bulunur; bir harita sunucusunun ele geçirilmesi tüm karakter verisine geniş yazma/okuma demektir. A'da bu risk servis yetkilendirmesine taşınır (aşağıda: servis yetkilendirmesi DOĞRULANMADI, bu yüzden kabul maddesi 6 zorunlu).
- **Servis zaten gerekli:** Steam bilet doğrulaması ve bilet/kira devri anahtar tutan sunucu tarafı bir bileşen ister; anahtar harita sunucusuna dağıtılmaz. Kalıcılığın aynı sınırda olması ek sunucu maliyetini artırmaz.
- **Kapsam:** yan veri, posta, `.sav` içe aktarma, 4 durumlu varlık sorgusu adayda var; B'de yeniden yazılır.

### 1.7 Adil kanıt durumu (sayılar panodan/görev kayıtlarından, bu belgeyi yazan koşmadı)
| | A (Codex aday `bfc197d`) | B (Claude, `master`) |
|---|---|---|
| Servis/SQL testi | `TestPersistenceService.py`: 97 kontrol, hermetik SQLite (pano/önceki denetim notu) | `vectors.json` 20 senaryo, gerçek Supabase dev'e karşı **132 PASS / 7 SKIP** (kullanıcı koşusu, `Scripts/TestSupabaseIntegration.ps1`) |
| UE testi | `TestDbStore.ps1` 118/119 native, **SQLite'a ve farklı tarihsel DLL'lere bağlı**; güncel aday için değil | 599 kontrol, **yalnız sahte sunucuya** |
| Gerçek PostgreSQL | **ACIK** (canlı PG kabulü yok; servisin PG dalı koşulmadı) | SQL tarafı: dev'de koşuldu; `run_all.sql`, eşzamanlılık, kira süresi dolma ve UE <-> gerçek DB koşulmadı |
| Bilinen açıklar (pano) | `expectedRevision` yok/null/-1 CAS açığı; `SaveWithOptions` bilinmeyen outcome; R1 kayıp ACK; async WinHTTP tick bloklama; importer eksik enumeration | R1 daralır ama kapanmaz (`get_op` + aynı `op_id`); UE depo hiçbir oyun yoluna bağlı değil |

Bunların hiçbiri "A kabul edildi" veya "B kabul edildi" demek değildir.

---

## 2. S04 KABUL KONTROL LİSTESİ (8 madde)

Her madde: ölçüt, kanıt türü, doğrulayan, durum kutusu. Durum başlangıçta hepsi **NOT TESTED**. Kutu yalnız bağımsız QA tarafından, kanıt kaydıyla (§2.9) değiştirilir. Geliştiren ajan kendi maddesini işaretleyemez.

Ortak şart: SQLite kanıtı bu listedeki hiçbir maddeyi tek başına kapatmaz.

### K1 — Gerçek PostgreSQL entegrasyon testleri
- **Ölçüt:** Servisin `--backend postgres` dalı, ayrı dev/test PostgreSQL'inde (SQLite değil) şu işlemleri doğru yapar: karakter yazma/okuma, `expectedRevision` CAS (null/-1/eksik dahil REDDEDİLİR), `writerEpoch`, idempotency, yan veri, ırk, posta op, içe aktarma geri alma. SQLite ile PG davranış farkı varsa FAIL.
- **Kanıt türü:** Otomatik test günlüğü + SQL katalog çıktısı (tablolar, kısıtlar, RLS durumu).
- **Doğrulayan:** Bağımsız QA (Antigravity/ayrı oturum); Codex koşar ama onaylamaz.
- **Durum:** [ ] PASS  [ ] FAIL  [ ] BLOCKED (PG ortamı yok)  [x] NOT TESTED

### K2 — Eşzamanlı veri tutarlılığı (envanter / para / karakter)
- **Ölçüt:** Gerçek PG üzerinde, iki bağlantı/iki harita sunucusu aynı anda: (a) aynı karakter için iki yazma: biri kazanır, eski revision reddedilir; (b) AYNI eşya iki oyuncuya aktarılırsa yalnız biri sahip olur, çoğaltma/kayıp yok; (c) para transferi sırasında servis çökerse toplam para korunur (çift sayma/kayıp yok), yeniden başlatma sonrası durum tutarlı.
- **Kanıt türü:** Çok bağlantılı test betiği (iki ayrı işlem), servis öldürme (kill) senaryosu, sonrasında DB sayımı (toplam altın, eşya InstanceId benzersizliği).
- **Doğrulayan:** Bağımsız QA.
- **Durum:** [ ] PASS  [ ] FAIL  [ ] BLOCKED  [x] NOT TESTED

### K3 — Idempotency + epoch
- **Ölçüt:** Aynı `idempotencyKey` + aynı yük tekrarında yan etki YOK ve aynı sonuç döner; aynı anahtar + farklı yük REDDEDİLİR; eski `writerEpoch` / `ownershipEpoch` sahibinin yazısı REDDEDİLİR (fencing); epoch geriye gitmez.
- **Kanıt türü:** Otomatik test + DB satır sayımı (tekrar sonrası satır/revision değişmedi).
- **Doğrulayan:** Bağımsız QA.
- **Durum:** [ ] PASS  [ ] FAIL  [ ] BLOCKED  [x] NOT TESTED

### K4 — Hata ve kurtarma
- **Ölçüt:** (a) DB bağlantı kesintisi: servis `503 unavailable` döner, oyun durumu "kaydedildi" saymaz (DB kapalıyken durum ilerlemez); (b) zaman aşımı, (c) servis restart, (d) retry güvenli (yinelenmez), (e) **R1 kayıp ACK:** yanıt kaybolduktan sonra istemci sonucu uzlaştırır (aynı anahtarla yeniden deneme ya da sorgu), yazım ne kaybolur ne ikilenir; "bilinmeyen outcome" bool başarı sayılmaz; (f) Async WinHTTP oyun thread'ini tick'te bloklamaz (kare süresi ölçümü).
- **Kanıt türü:** Fault-injection (test ortamında), servis/DB kesme, UE günlüğü, kare süresi ölçümü.
- **Doğrulayan:** Bağımsız QA; fault-injection yalnız test ortamında (K6 ile bağlantılı).
- **Durum:** [ ] PASS  [ ] FAIL  [ ] BLOCKED  [x] NOT TESTED

### K5 — UE dedicated server uçtan uca
- **Ölçüt:** Gerçek bir UE dedicated server süreci (sahte sunucu/mock DEĞİL), gerçek servise ve gerçek PG'ye karşı: karakter oluştur -> oyna -> kaydet -> sunucuyu kapat -> aç -> geri yükle; envanter, para, görev, ırk, başarım, günlük, posta aynı. İki sunucu aynı karakteri aynı anda sahiplenemez.
- **Kanıt türü:** UE günlüğü + servis günlüğü + DB önce/sonra satırları (hash karşılaştırması).
- **Doğrulayan:** Bağımsız QA. PIE/istemci tarafı kısmı ayrıca kullanıcı zamanı gerektirir (PIE'da denenmedi notu korunur).
- **Durum:** [ ] PASS  [ ] FAIL  [ ] BLOCKED  [x] NOT TESTED

### K6 — Güvenlik ve yetkilendirme
- **Ölçüt:** (a) harita sunucusu <-> servis karşılıklı kimlik doğrulama vardır (yetkisiz çağrı REDDEDİLİR); **şu an DOĞRULANMADI:** adayda istemci/servis kodunda Bearer/jeton ifadesi bulunamadı, tam okuma yapılmadı. (b) Secret yönetimi: DB parolası, Steam anahtarı, servis sırrı depoda/günlükte/komut satırında YOK; yalnız ortam değişkeni/gizli depo. (c) Ağ izolasyonu: DB yalnız servisten erişilebilir; harita sunucusunda DB bağlantı bilgisi YOK. (d) `/v1/admin/fault` benzeri hata-enjeksiyon uçları üretim yapılandırmasında KAPALI (varsayılan kapalı + üretim profili testi). (e) Harita sunucusuna geniş DB yetkisi verilmez. (f) Şema: `public` API'ye açıksa RLS veya erişim kapatma (Codex V2'de RLS yok, ayrıntı K7).
- **Kanıt türü:** Kod incelemesi + yetkisiz istek testi + yapılandırma/ağ taraması + günlük gizli-bilgi taraması.
- **Doğrulayan:** Bağımsız QA + `ag-guvenlik-denetcisi` (salt okunur).
- **Durum:** [ ] PASS  [ ] FAIL  [ ] BLOCKED  [x] NOT TESTED

### K7 — Şema ve migration uyumu
- **Ölçüt:** (a) Tablo adı tutarsızlığı giderilmiş (§6.2); (b) V2 -> V3 -> V4 temiz DB'de sırayla uygulanır, tekrar uygulama zararsız; (c) sürüm geçişleri (şema sürümü) ve geri alma yolu belgeli; (d) eski kayıt (`.sav` + yan veri) içe aktarma idempotent: aynı girdiyle ikinci koşu satırı değiştirmez; (e) **eksik veya hatalı enumeration** (importer bir dosyayı atlar/yanlış sayar) tespit edilir ve rapor eder, sessiz atlama YOK; (f) kısmi hata batch'i geri alır.
- **Kanıt türü:** Temiz DB'de migration koşusu, katalog dökümü, içe aktarma iki kez koşu + sayım, kasıtlı bozuk girdi.
- **Doğrulayan:** Bağımsız QA.
- **Durum:** [ ] PASS  [ ] FAIL  [ ] BLOCKED  [x] NOT TESTED

### K8 — Yük ve gecikme (2000 eşzamanlı oyuncu hedefi)
- **Ölçüt (ölçülecek, SAYI UYDURULMAZ):** saniyedeki kalıcı yazma sayısı, p95/p99 gecikme, DB bağlantı sayısı, hata oranı, servis CPU/bellek, kuyruk derinliği. Bu belgede ölçülmüş yük verisi YOK.
- **Hedef değerler (hepsi [TAHMİN], kullanıcı/ADR sahibi onaylayana kadar bağlayıcı değil):**
  - yazma/s = CCU / kaydetme_aralığı_sn. Kaydetme aralığı bugün kodda tek sayı olarak bulunmadı [DOĞRULANMADI]; aralık parametre olarak seçilir, ölçüm raporunda açıkça yazılır.
  - p95/p99 gecikme hedefi, hata oranı hedefi, bağlantı üst sınırı: [TAHMİN, belirlenecek: ADR sahibi ölçüm sonrası].
- **Yöntem:** Yük üreteci (sahte oyuncular, gerçek servis HTTP'si) + gerçek PG dev/test; kademeli (örn. 100, 500, 1000, 2000 [TAHMİN]) artış; her kademe sabit süre; ölçüm: servis metrikleri + PG istatistikleri; ortam özellikleri (CPU/RAM/Supabase katmanı) rapora yazılır. Free katman sonuçları üretim kapasitesini temsil ETMEZ. Mevcut `Scripts/k6_load_test.js` var olmayan HTTP uçlarını hedefler (yeniden yazılmalı).
- **Doğrulayan:** Bağımsız QA + `performance` (profiler kanıtı olmadan kapasite sözü verilmez).
- **Durum:** [ ] PASS  [ ] FAIL  [ ] BLOCKED  [x] NOT TESTED

### 2.9 Kanıt kaydı biçimi (her kutu için zorunlu)
```
Madde: K#   Sonuç: PASS|FAIL|BLOCKED|NOT TESTED
Commit SHA: <test edilen aday>      Ortam: <PG sürümü/barındırma/katman, UE sürümü, işletim sistemi>
Komut: <tam komut, sırsız>          Sonuç özeti: <geçen/kalan, açık bulgular>
Günlük: <dosya yolu veya özet; gizli bilgi içermez>     Doğrulayan: <kim, geliştirici değil>     Tarih
```
BLOCKED = ortam/önkoşul eksik (neden yazılır). NOT TESTED = hiç koşulmadı. Koşulmayan şey PASS yazılmaz.

---

## 3. TEST MATRİSİ (senaryo x ortam)

Ortamlar: **SQLite** (hermetik servis), **PG** (gerçek PostgreSQL, yerel/izole), **Supabase dev** (barındırılan PG, ayrı test ortamı), **UE sahte** (sahte sunucuya karşı UE testi), **UE gerçek** (gerçek dedicated server + gerçek servis).
Hücre: mevcut kanıt (kaynak: pano/önceki denetim notları, bu belge koşmadı) veya `AÇIK` = hiç yok.

| Senaryo | SQLite | PG | Supabase dev | UE sahte | UE gerçek |
|---|---|---|---|---|---|
| Karakter yaz/oku, revision CAS | Var (servis 97, UE 118/119, tarihsel) | AÇIK | B hattı: SQL sözleşmesi 132 PASS (B'nin RPC'si; A servisi değil) | B hattı 599 (sahte) | AÇIK |
| CAS null/-1/eksik (P2 açığı) | Düzeltme teslimi pano notunda, kabul bekliyor | AÇIK | B: kaynakta `invalid` döner, gerçek DB vektörü belirsiz | AÇIK | AÇIK |
| Eşzamanlı iki bağlantı | Sınırlı (tek süreç) | AÇIK | B: `05_concurrency_*` betikleri var, koşulmadı | -- | AÇIK |
| Aynı eşya iki oyuncuya aktarım | AÇIK | AÇIK | AÇIK | AÇIK | AÇIK |
| Para transferi sırasında servis çökmesi | AÇIK | AÇIK | AÇIK | -- | AÇIK |
| Idempotency tekrar `op_id`/`idempotencyKey` | Var (SQLite) | AÇIK | B: SQL vektörleri var | B: sahte | AÇIK |
| Eski epoch sahibi reddi | Var (SQLite) | AÇIK | B: vektör var, kira süresi dolma koşulmadı | B: sahte | AÇIK |
| DB bağlantı kesintisi / restart / retry | Kısmi (fault bayrağı) | AÇIK | AÇIK | B: sahte | AÇIK |
| R1 kayıp ACK uzlaştırma | Pano: AÇIK (normal bool API anahtar tutmuyor) | AÇIK | B: `get_op` + aynı `op_id` (daralır, kapanmaz) | B: sahte | AÇIK |
| Async istemci tick bloklama | -- | -- | -- | Pano: gerçek async/R1 AÇIK | AÇIK |
| Yan veri (başarım/günlük/posta/ırk) | Var (SQLite) | AÇIK | A servisine uygulanmadı | -- | AÇIK |
| `.sav` + yan veri içe aktarma, idempotent | Var (SQLite), eksik enumeration bulgusu açık | AÇIK | AÇIK (B'de UE aracı yok; SQL `import_character` var) | -- | AÇIK |
| Sunucu-servis kimlik doğrulama | DOĞRULANMADI | AÇIK | -- | -- | AÇIK |
| Fault-injection üretimde kapalı | DOĞRULANMADI | AÇIK | -- | -- | AÇIK |
| Şema V2/V3/V4 temiz uygulama | -- | AÇIK | AÇIK (Codex V2 uygulanmadı) | -- | -- |
| RLS / `public` açıklığı | -- | AÇIK | B şeması dev'de katalogla doğrulandı; Codex V2'de RLS yok | -- | -- |
| 2000 CCU yük | -- | AÇIK | AÇIK | -- | AÇIK |
| Yedekten geri yükleme | -- | AÇIK | AÇIK [plan katmanına bağlı, DOĞRULANMADI] | -- | AÇIK |

---

## 4. ROLLER VE GÖREVLER

Ortak: hiçbir ajan diğerinin kodunu kanıtsız devralmaz; hiçbir ajan kendi çıktısını tek başına onaylamaz.

### 4.1 Codex
1. Adayı (`bfc197d` ve ona bağlı S04 commitleri) **izole ortamda** doğrulamaya hazırlar (temiz worktree, ana ağaç kirli dosyalarına dokunmaz).
2. Servisi gerçek PostgreSQL'de çalıştırır (`--backend postgres`), sonuçları ham günlükle verir.
3. Tablo adı tutarsızlığını raporlar (§6.2) ve görev kaydı/şema arasında hangisinin doğru olduğunu belirtir.
4. SQLite testlerini korur; PG testleri **ekler** (yeni persistence özelliği eklemez).
5. Servis güvenliği, transaction sınırları ve modülerlik değerlendirmesini (§6) yazar.
6. Kendi çıktısını kabul sayamaz; kanıtı QA'ya teslim eder.

### 4.2 Claude
1. 132 adımlık paketi (`Scripts/TestSupabaseIntegration.ps1`, `supabase/tests/vectors.json`) analiz eder; Python servis için yeniden kullanılabilir kısmı belirler (kavramsal test vektörleri ve senaryolar; uç noktalar farklı olduğundan doğrudan koşmaz).
2. Supabase dev DB'yi PG kabul testlerine hazırlar (ayrı şema `s04_accept` ya da ikinci ücretsiz proje; kullanıcı kararı §8). Üretim DB'sine dokunmaz.
3. RPC mimarisini GENİŞLETMEZ ve SİLMEZ.
4. Belge denetimi: kabul kayıtlarını ve sayıları belgelerle eşitler; panoya işlemez (koordinatör/Codex panoya işler).

### 4.3 Antigravity / bağımsız QA
1. İki uygulamanın (A ve B) kanıtlarını **bağımsız** yeniden üretir; geliştirici günlüğünü kanıt saymaz.
2. Senaryolar: karakter, envanter, ekonomi, görev, posta, içe aktarma (§2 K1-K8).
3. Rapor: madde başına PASS / FAIL / BLOCKED / NOT TESTED + §2.9 kanıt kaydı.
4. Karar önerisi vermez; kanıt ve açık bulgu verir (karar ADR sahibi + kullanıcıda).

### 4.4 Kullanıcı
- PostgreSQL ortamını sağlar (Supabase dev'de AYRI şema `s04_accept` ya da ikinci ücretsiz proje; bkz. §8).
- DB bağlantı bilgisini YALNIZ yerel ortam değişkeninde tutar; sohbete, belgeye, depoya, günlüğe YAZILMAZ. Bu belgede gerçek proje referansı/anahtar/parola yoktur ve olmayacaktır.
- Açık kararları (§8) verir.

### 4.5 Architect / ADR sahibi + TPM (öneri; açık karar)
Bu projede Claude orkestratördür; Codex'in kendi panosunda "orchestrator" rolü vardır. İki hat da geliştirici olduğundan çıkar çatışması var.
**Öneri [karar kullanıcıda]:**
- **ADR sahibi + veri modeli/API sözleşme sahibi:** Claude (orkestratör oturumu), çünkü ana kopya ve belge hattı ondadır; ama **kendi B hattı için karar vermez**: B değerlendirmesinde kabul kararı kullanıcıda, kanıt QA'dadır. Alternatif: Codex sahipliği (A'nın geliştiricisi olduğu için A'yı kendi kabul edemez kuralı korunur).
- **TPM (zamanlama/bağımlılık/pano):** koordinatör/Codex panoya işler; ADR sahibinden ayrı kalır.
- Hangi seçilirse seçilsin: ADR sahibi geliştirici olduğu hattın kabul kutusunu işaretleyemez.

---

## 5. KARAR AKIŞI VE ZAMANLAMA

Süreler bu belgede **önerilir [ÖNERİ]**, kullanıcı onaylamadı (§8). Kesin takvim sözü verilmez; bloklayıcı yoksa sıra şöyledir.

| Adım | İçerik | Girdi | Çıktı |
|---|---|---|---|
| 1. Dondurma | S04 persistence kodu dondurulur: Claude RPC'yi genişletmez/silmez, Codex yeni özellik eklemez; yalnız düzeltme | Bu belge + kullanıcı onayı | Dondurulmuş aday SHA'ları kayda geçer |
| 2. Aday doğrulama | Adayın izole worktree'de derleme + mevcut testler (SQLite) yeniden koşusu; tablo adı tutarsızlığı; açık bulgular listesi | `bfc197d` ve sonrası | Başlangıç raporu (kanıt kaydı) |
| 3. PG kabul | K1, K2, K3, K4(a-e), K7 gerçek PG'de | Ayrı PG ortamı (kullanıcı) | PASS/FAIL/BLOCKED raporu |
| 4. UE uçtan uca | K5 (+K4f, K6 kısmen) gerçek dedicated server | 3'ün geçmesi | Rapor |
| 5. Güvenlik + yük | K6 tam, K8 ölçüm | 3-4 | Rapor + ölçüm tablosu |
| 6. Bağımsız QA raporu | Tüm kutular, açık bulgular | 2-5 | Tek rapor |
| 7. Karar | ADR durumu: **KABUL** veya **RED** (yetki: kullanıcı, ADR sahibi önerir) | 6 | ADR güncellenir |
| 8a. A geçmezse | **Sınırlı düzeltme penceresi** (süre §8; [ÖNERİ]: yalnız bulgulara yönelik, yeni özellik yok) sonra adım 3-6 yalnız FAIL olanlar için tekrar | FAIL listesi | Yeni rapor |
| 8b. Pencere sonrası da geçmezse | B, **AYNI K1-K8 kriterleriyle** değerlendirilir (B için eksik olan kısımlar -yan veri, içe aktarma, çağıran bağlama- BLOCKED/NOT TESTED yazılır, eksik gizlenmez) | B kodu | B raporu, karşılaştırmalı karar |
| 9. Tek mimariye geçiş | Seçilen mimari tek sahipli olur; diğer hat donuk/arşiv; pano ve S05/S06/S12/S13 bağımlılıkları güncellenir | 7 | Birleşme için bağımsız QA kabulü şart |

Bağımlılık notu (pano satırlarından): S05 Steam (A5) ve S06 sahiplik/bilet (A3) S04'e bağlı; S12 pazar "tek DB satış işlemi" için S04'e, S13 alfa sertleştirme DB yedeğinden dönüşe bağlı. Bu yüzden S04 kabulü gecikirse bunlar da gecikir.

---

## 6. Servis modüler yapı incelemesi ve Steam/bilet ayrımı

### 6.1 Gözlem (adayda `git show bfc197d:Scripts/MMOPersistenceService.py` ile okundu)
- Dosya **5211 satır**. En büyük sınıf `PersistenceEngine` (satır ~601'den ~4750'ye, yaklaşık 4100 satır [yaklaşık, sınır elle sayılmadı]); ayrıca `PersistenceError`, `ThreadingHTTPServer`, HTTP işleyici fabrikası. Testi `Scripts/TestPersistenceService.py` 1307 satır.
- Aynı motor sınıfı şu sorumlulukları taşıyor gibi görünüyor: karakter/yan veri CAS, idempotency, `.sav` GVAS ayrıştırma ve içe aktarma, posta op'ları, hesap/oturum jetonu (`account_sessions`), Steam Web API bilet doğrulaması (`steam_web_api_key`, `steam_app_id`), bilet üretimi (`secrets.token_hex`). İki veritabanı arka ucu (SQLite + psycopg) tek sınıfta.
- Servis yetkilendirmesi (istemciden gelen çağrıya Bearer/jeton) bu okumada **bulunamadı**; `account_sessions` oyuncu oturumları içindir, sunucu-servis kimliği ayrı soru. **DOĞRULANMADI** (tam okuma yapılmadı).

### 6.2 Görev: kod incelemesi (ayırma emri DEĞİL)
Amaç, sınırların nerede olduğunu raporlamak; bölme kararı ADR sahibi + kullanıcıdadır. İncelenecekler:
1. Sorumluluk haritası: kalıcılık çekirdeği / içe aktarma / posta / hesap-oturum / Steam doğrulama / bilet-kira (S06/S07) / yönetici ve hata enjeksiyon uçları. Her biri için satır aralığı, bağımlılığı, ortak durumu (global veya paylaşılan bağlantı).
2. SQLite/PG ikili dalının PG'yi gizleyip gizlemediği (test yalnız SQLite'ı kapsıyorsa PG hata riski).
3. Transaction sınırları: tek HTTP isteği = tek transaction mı; çoklu tablo yazımı (karakter + yan veri + idempotency) aynı transaction'da mı; kilit sırası ve kilitlenme riski.
4. Eşzamanlılık: `ThreadingHTTPServer` + bağlantı havuzu (havuz var mı; 2000 CCU'da bağlantı sayısı).
5. Hata eşlemesi (`503 unavailable`, `422 corrupt_record`), günlükte gizli bilgi.
6. **Steam anahtarı/bilet/oturum/harita geçişi servislerinin kalıcılık servisinden AYRI tutulması sorusu:** artıları (Steam anahtarı ile DB parolası ayrı sınırlarda; kalıcılık servisi küçük yüzey; ayrı ölçekleme/yeniden başlatma), eksileri (iki servis işletmek, servisler arası kimlik doğrulama, bilet-kira devrinin kalıcılıkla atomik olma ihtiyacı). Karar verilmedi; inceleme çıktısı ADR'ye girdi olur (§8).

### 6.3 Tablo adı tutarsızlığı (okundu, düzeltilmedi)
| Kaynak | Adlar |
|---|---|
| `docs/tasks/S04_01_PERSISTENCE_SERVICE_2026-10-17.md` (kayıt) | `characters`, `write_idempotency`, `save_import_log` (+ `character_side`) |
| `Migrations/V2__character_store.sql` (`bfc197d`'de `CREATE TABLE`) | `character_saves`, `persistence_idempotency`, `legacy_import_ledger` (+ `character_side`, `accounts`, `presence`, `travel_tickets`) |
Hangisi servis kodunda gerçekten kullanılıyor: **DOĞRULANMADI** (servis SQL'i satır satır okunmadı). Codex'in görev kayıtlarına bu belge dokunmaz. Ayrıca: V2'de `ROW LEVEL SECURITY` yok (`public` şeması); Supabase'e uygulanırsa API'ye açıklık kapatılmalı (K6f, K7).

---

## 7. KESİN KURALLAR
1. Hedef mimari A; **kesin kabul yoktur**. Kabul = §2'nin tamamı bağımsız QA tarafından kanıtlı PASS + kullanıcı/ADR sahibi kararı.
2. Önce bağımsız doğrulama (seçenek 3); sonra tek mimari. A geçmezse sınırlı düzeltme, sonra B aynı kriterlerle.
3. S04 kapsamında paralel kalıcılık geliştirmesi DURUR. Claude RPC'yi genişletmez/silmez; Codex yeni özellik eklemez; kimse diğerinin kodunu kanıtsız devralmaz; ana dala birleşme için bağımsız QA kabulü zorunlu.
4. Geliştiren ajan kendi kodunu tek başına onaylamaz; test sayısı tek başına başarı ölçütü değildir.
5. Sonuç etiketleri: PASS / FAIL / BLOCKED / NOT TESTED. Kanıt: commit SHA, ortam, komut, sonuç, log.
6. Üretim DB'sinde test yok. Kabul testleri gerçek PostgreSQL'de (SQLite değil), ayrı dev/test ortamında. Gerçek proje referansı, anahtar, parola belgeye/sohbete/depoya YAZILMAZ.
7. Hedef: 2000+ eşzamanlı oyuncu, güvenli, gözlemlenebilir, geri yüklenebilir, TEK sahipli kalıcılık. Ölçülmemiş şey "çalışıyor/dengeli" diye yazılmaz.

---

## 8. AÇIK KARARLAR (kullanıcıya)
| # | Karar | Seçenekler / öneri |
|---|---|---|
| O1 | ADR sahibi + veri modeli/API sözleşme sahibi | Öneri: Claude (orkestratör), kendi hattının kabulünü işaretlemeden; ya da Codex. TPM ayrı (koordinatör/Codex panosu) |
| O2 | PG ortamı | (a) Supabase dev projesinde AYRI şema `s04_accept` (hızlı, aynı proje; ama dev projesindeki `mmo` şemasıyla karışma riski ve Free katman sınırları), (b) ikinci ücretsiz proje (tam izolasyon, ek kurulum). Öneri: (b) kabul testleri için; yerel PostgreSQL de olası ek hat |
| O3 | Servis dili / modül sınırı | Python fiilen seçilmiş (S13/S2 kullanıcı onayı yok); modül sınırı §6 incelemesinden sonra |
| O4 | Dondurma süresi ve düzeltme penceresi uzunluğu | [ÖNERİ, onay bekler] süreleri kullanıcı koyar; bu belge takvim sözü vermez |
| O5 | B'ye geçiş eşiği | Hangi FAIL/BLOCKED birleşimi B değerlendirmesini tetikler (öneri: K1, K2, K3, K4, K6 maddelerinden birinde pencere sonunda hâlâ FAIL) |
| O6 | Steam/bilet servisinin kalıcılıktan ayrılması | §6.2/6 incelemesi sonrası |
| O7 | K8 hedef sayıları ve kaydetme aralığı | Ölçüm sonrası ADR sahibi + kullanıcı |

---

## 9. YAPILAN / YAPILMAYAN
**Yapıldı:** kararın kaydı, ADR taslağı, 8 maddelik kabul listesi, test matrisi, roller, akış, servis inceleme görevi, açık kararlar.
**Yapılmadı:** hiçbir kod yazılmadı veya silinmedi; hiçbir test koşulmadı; hiçbir PG ortamı kurulmadı; hiçbir mimari kabul edilmedi; `SIX_MAP_SPRINT_BOARD.md` ve `docs/tasks/` değişmedi.
**Bu belge kararın PLANIDIR, kabul değildir.**

**DOĞRULANAMADI (Codex ayrıntıları):** servis yetkilendirmesinin varlığı/biçimi; servis SQL'inin gerçekten hangi tablo adlarını kullandığı; `PersistenceEngine` sınıfının tam satır sınırları; servisin PostgreSQL dalının çalıştığı; V3/V4 şemalarının içeriği ve Steam kodunun gerçek davranışı; 97 / 118-119 sayılarının güncel adayla geçerliliği; `/v1/admin/fault` benzeri uçların varsayılan durumu; Codex'in düzeltme teslimlerinin (sıkı ACK, ordinary CAS, importer ACK, filesystem discovery) kabul durumu (pano kabul bekliyor diyor).
