# Supabase'i veritabanı olarak bağlama tasarımı (kapalı alfa)

**Tarih:** 2026-10-17 (tasarım `71c548f`; durum bloğu aynı gün güncellendi).

## DURUM (2026-10-17 sonu; her satır git'te ve dosyada doğrulandı)

**Özet: SQL ve UE depo katmanı YAZILDI; UE tarafı yalnız sahte sunucuya karşı test edildi. 2026-10-17'de V2 migration kullanıcının DEV Supabase projesine uygulandı ve ilk gerçek doğrulamalar geçti (aşağıdaki satır); `run_all.sql` testleri, UE <-> gerçek DB ve 11 RPC'nin HTTPS çağrısı HENÜZ DOĞRULANMADI. Oyunun hiçbir yolu bu depoyu çağırmıyor.**

**Dev projesinde ilk gerçek doğrulama [Doğrulandı: dev, 2026-10-17]** (hesap: FREE katman, bölge West EU (Ireland), tek proje `main` dev olarak; referans `<ref>`):
(1) `supabase/migrations/20261017120000_v2_character_store.sql` (824 satır) SQL Editor'de tek seferde HATASIZ çalıştı (`Success. No rows returned`); (2) katalog: `mmo_api`'de 12 fonksiyon, hepsinde `anon=false`, `authenticated=false`, `service_role=true`; `mmo`'da 6 tablo, hepsinde RLS açık; `anon`/`authenticated`/PUBLIC tablo yetkisi 0; `healthcheck` -> `{outcome:'ok', api_version:2, contract_version:2, schema:'v2'}`;
(3) Data API Exposed schemas yalnız `mmo_api`, anahtarsız `GET /rest/v1/character_saves` REDDEDİLDİ; (4) gerçek HTTPS RPC `POST /rest/v1/rpc/healthcheck` (`apikey: sb_secret_...`, `Content-Profile: mmo_api`, Bearer YOK) BAŞARILI, yani `sb_secret_` anahtarı yalnız `apikey` ile kabul edilir;
(5) **KURAL:** tarayıcı gibi görünen `User-Agent` (Windows PowerShell varsayılanı `Mozilla/5.0 ... WindowsPowerShell`) ile secret key REDDEDİLİR (`{"message":"Forbidden use of secret API key in browser","hint":...}`); açık UA (`MMOGameServer/0.1`) ile aynı çağrı başarılı. Sonuç: oyun sunucusu istemcisi `Mozilla/Chrome/Safari/Gecko/AppleWebKit/Edge` içermeyen AÇIK bir UA göndermeli (UE düzeltmesi ayrı ajanda: Dilim 2 UA, yapılmadı).
**Hâlâ doğrulanmadı:** `run_all.sql` (psql kurulu değil), eşzamanlılık testleri, `vectors.json` senaryolarının gerçek PostgREST'e karşı koşusu, UE `FHttpModule`/WinHttp'nin varsayılan UA'sı, Bearer'li `sb_secret_`, FREE katmanda yedek/duraklatma davranışı.

| Parça | Durum | Kanıt (commit / dosya) | Doğrulama sınırı |
|---|---|---|---|
| Tasarım (bu belge) | Tamam | `71c548f` | Supabase limit/fiyat/özellikleri hâlâ [Doğrulanmadı] (§9) |
| Dilim 1 SQL | **Yazıldı; 2026-10-17 dev Supabase'e UYGULANDI (hatasız), katalog/HTTPS doğrulandı; `run_all.sql` testleri KOŞULMADI** (migration dosyası artık değiştirilmez, yeni zaman damgalı dosya) | `c8d81c1`; düzeltmeler `bcde6a2` (tur 1), `1ae80c7` (tur 2); `supabase/migrations/20261017120000_v2_character_store.sql` (823 satır), `supabase/tests/00..05*.sql`, `run_all.sql`, `vectors.json` (sözleşme sürümü 2, 20 senaryo), `supabase/README.md` | Makinede psql / docker / supabase CLI yok. `vectors.json` Python ile ayrıştırıldı. Migration'ın ilk gerçek koşusu dev projesinde SQL Editor ile yapıldı (yukarıdaki doğrulama); testler psql ister |
| Dilim 2 UE | **Yazıldı, sahte ulaşımlı native test geçti, izole worktree'de** | `e0fffdf` -> `43be747` -> `932b565` -> `342ab17`; `MMOHttpTransport.h` (`IMMOHttpTransport`), `MMOUnrealHttpTransport.*`, `MMOSupabaseRestClient.*`, `MMOSupabaseCharacterStore.*` (`FMMOSupabaseCharacterStore`, arayüz `IMMOAsyncCharacterStore`), `Private/Tests/MMOSupabaseStoreSelfTest.cpp`, `Scripts/TestSupabaseStore.ps1` | 556 kontrol (`SupabaseTestExpectedChecks = 556`); sahte sunucu `vectors.json`'dan türetilir (gerçek PostgREST/SQL değil: sahte ile gerçek ayrışabilir); mutasyon sınaması 62 mutasyonun 62'si yakalandı (9'u ilk turda kaçmıştı, test güçlendirildi). Ana kopya başkasının `MMOZoneLayout*` / `MMORace*` dosyaları yüzünden derlenmediği için izole worktree'de koşuldu. `RunSystemTests.ps1` / `RunAllChecks.ps1` adımı DEĞİL. Gerçek `FHttpModule` / WinHttp davranışı (LogHttp çıktısı dahil) doğrulanmadı |
| Dilim 3 (depo seçimi `-MMOCharacterStore`, açılışta `HealthcheckEx`) | **YAPILMADI** | `grep` ile `Source/` içinde bayrağı okuyan kod yok | -- |
| Dilim 4 (çağıranları DB'ye geçirme, periyodik `RenewLeaseEx`, çıkışta `ReleaseLeaseEx`) | **YAPILMADI** | `MMOCharacter.cpp` vb. Codex'te kirli | Depoyu oyundaki hiçbir kod çağırmıyor (yalnız test) |
| Yan veri (başarım / günlük / posta) | Depo YOK | `import_character` yan veriyi almaz | Dilim 5 / 7 |
| Gerçek Supabase hesabı / bağlantısı | **VAR (dev, 2026-10-17)**: FREE, West EU (Ireland), proje `main`; anahtar yerel kullanıcı ortam değişkenlerinde | bu belge, `supabase/README.md` "Dev projesinde doğrulananlar" | Yalnız `healthcheck` HTTPS çağrıldı; UE istemcisi gerçek DB'ye hiç bağlanmadı; PIE yok |

**Güvenlik turları:** iki bağımsız QA + iki güvenlik turu yapıldı. KAPANDI: `p_op_id` fazlalığı, `lease_epoch` adı, `get_op` `found`, `SlotToKey` baş/son boşluk, Shipping `NO_LOGGING` koruması, Bearer yalnız JWT anahtarı, `CreateEx` kirası, donmuş yuva `release -> acquire`, `write_ops` purge `p_batch`, `LogHttp` Warning. **AÇIK:** K-1 kimlik (tek paylaşılan secret key, DB kimlik çözmez), kira yenilemenin çağrılmaması (Dilim 4), yan veri deposu yok, SQL testleri (`run_all.sql`) koşulmadı (migration dev'e uygulandı), R1 daralır ama kapanmaz, yedek/silme (KVKK) RPC'si yok, motorun `LogHttp` / WinHttp davranışı doğrulanmadı.

**Bu belgenin gövdesi hakkında:** §3.2 SQL ve §3.3 `save_character` kodları 71c548f taslağıdır; **gerçek kaynak `supabase/migrations/...v2_character_store.sql` ve `supabase/README.md` "Sapmalar" listesidir** (FORCE RLS yok, `save_character` SECURITY DEFINER, `clock_timestamp()` kilit sonrası, 2 MiB tavan, ek sütunlar). Çelişkide migration kazanır.

**Kullanıcı yapacaklar** (ajan yapamaz): Supabase hesabı + `myproject-dev` projesi (sonra alpha), anahtarı SOHBETE YAPIŞTIRMADAN yalnız yerel ortam değişkenine (`MMO_SUPABASE_URL`, `MMO_SUPABASE_SECRET_KEY`), Dashboard güvenlik ayarları (`supabase/README.md` listesi), psql / Supabase CLI kurulumu (makinede yok). Adımlar: §6 ve `docs/PLAYTEST_CHECKLIST.md` "Supabase dev projesi ilk kurulum".

**Kaynaklar (okundu, tasarım anında):** `docs/SIX_MAP_PLAN.md` (§1.4, §1.5, A2/A3/A5, S12, S13, S15), `docs/SIX_MAP_SPRINT_BOARD.md` (S04-S07, S12), `docs/tasks/S04_00_PERSISTENCE_CONTRACT_2026-10-16.md`,
`Source/MyProject/Public/Save/MMOCharacterStore.h`, `MMOCharacterStoreSubsystem.h/.cpp`, `MMOCharacterSaveJson.h`, `Public/Core/MMONameRules.h`, `Migrations/V1__initial_schema.sql`,
`MyProject.Build.cs` (HTTP/Json zaten var), `.gitignore` (`.env`, `.env.*`, `Saved/` ignore), `MMODialogueSubsystem.cpp:155-161` (LLM anahtarı zaten ortam değişkeninden okunuyor).
**Etiketler:** [Kesin] = bu repoda okundu. [Tahmin] = benim çıkarımım. **[Doğrulanmadı]** = Supabase'in güncel limit/özellik bilgisi; internet erişimi yasak olduğu için doğrulanmadı, hesap açmadan önce Supabase fiyat/dokümantasyon sayfasından kontrol edilmeli. Hiçbir Supabase sayısına kesinlik katılmadı.

---

## 0. Özet ve öneri

**Öneri (tek paragraf):** Supabase'i yalnız **barındırılan PostgreSQL + PostgREST RPC katmanı** olarak kullan; Supabase Auth, Realtime, Storage kullanma. Tüm karakter mantığı (revision karşılaştır-ve-yaz, kilit-kira/fencing, idempotent işlem günlüğü, içe aktarma) **veritabanındaki SQL fonksiyonlarında** durur (Seçenek C); UE harita sunucusu bu fonksiyonları HTTPS ile çağırır, yalnız sunucu makinesinde ortam değişkeninde duran **secret key** ile (oyun ikilisine, istemciye, `Config/*.ini`'ye girmez). Tablolar `public` değil özel `mmo` şemasında, RLS açık ve politikasız (varsayılan reddet), fonksiyonlar `mmo_api` şemasında yalnız `service_role`'e açık. Steam bilet doğrulaması (A5) ve sahiplik/bilet (A3) **ara serviste** kalır (S13 kararı bozulmaz); o servisin dili için Supabase Edge Function (TypeScript) bir aday olarak sunulur. Bu, S13(a)'nın DB yarısını alfa için gevşetir (harita sunucusu DB fonksiyonlarına doğrudan gider): **kullanıcı onayı gerekir (Karar 1)**. UE tarafı mevcut `IMMOCharacterStore` arayüzünü bozmadan eklenen `FMMOSupabaseCharacterStore` ile gelir; varsayılan yol (bayraksız) bugünkü yerel dosya deposudur; DB seçili iken DB kapalıysa durum ilerlemez ve **sessizce dosyaya düşülmez** (veri çatallanması).

**Hemen yapılabilir:** kullanıcı Supabase hesabı + boş dev projesi + şema uygulama + `select 1` (§6, ilk dilim). Dilim 1 (SQL) ve Dilim 2 (UE depo) yazıldı (bkz. DURUM); çağıran geçişi (Dilim 3-4) S04 uygulama kapısı (S03 + kararlar) açılana kadar BEKLER.

---

## 1. Supabase bu proje için ne ise, ne değil

Supabase = barındırılan PostgreSQL + onun üstüne eklenmiş paketler: Auth, PostgREST (tablolar/fonksiyonlar için otomatik REST API), RLS, Edge Functions (Deno/TypeScript), Storage, Realtime. **UE için resmi bir Supabase eklentisi/SDK'sı yok** [Doğrulanmadı; bildiğim kadarıyla resmi yok, topluluk eklentileri olabilir; kullanmayız]. Gerek de yok: `Build.cs`'te `HTTP` ve `Json` modülleri zaten var [Kesin], PostgREST düz HTTPS+JSON'dur.

| Supabase parçası | Bu projede | Neden |
|---|---|---|
| PostgreSQL | **Kullan** | Plan zaten PostgreSQL (S12, A2); barındırma/yedek işini kullanıcıdan alır. |
| PostgREST (REST API) + RPC fonksiyonları | **Kullan, yalnız RPC** | UE `FHttpModule` ile çağırır. Tablo uçlarını doğrudan kullanmayız (çok adımlı atomik işlem tek istekte olmaz). |
| RLS | **Kullan (varsayılan reddet)** | Savunma derinliği. Asıl koruma: tablolar API'ye açık olmayan şemada + yetki geri alma. |
| Supabase Auth | **Kullanma** | Oyuncu kimliği Steam bilet doğrulamasıyla çözülür (S12/S18: `ISteamUser::GetAuthTicketForWebApi` -> servis `AuthenticateUserTicket`). Auth'un oturum/JWT modeli tarayıcı/uygulama kullanıcısı içindir; Steam'in oyun içi web-api bileti akışına oturmaz. Auth ayrıca Steam'i yerleşik sağlayıcı olarak sunmuyor olabilir (Steam OpenID 2.0) [Doğrulanmadı]. Parola/e-posta zaten kapsam dışı (S12). Auth'u açmak ikinci bir kimlik tablosu (`auth.users`) demektir: `accounts(provider, external_id)` ile çift kaynak. |
| Edge Functions | **Aday (S13 servis dili)** | Steam Publisher Key'i Edge Function gizlisinde tutup bilet doğrulayan küçük bir TypeScript fonksiyonu, "ayrı sunucu işletme" yükünü kaldırır. Soğuk başlatma/süre sınırı [Doğrulanmadı]; giriş yolunda (nadir çağrı) kabul edilebilir, kayıt yolunda (sık çağrı) kullanılmaz. Karar S13'te açık (Karar 2). |
| Storage | Kullanma | Yedek dışa aktarımı için ileride düşünülebilir, şimdi gerekmez. |
| Realtime | Kullanma | Süreçler arası sosyal (`/who`, fısıltı) A4'te ayrıca tasarlanacak; DB tabanlı yoklama yeter. |
| Dashboard SQL Editor | Yalnız elle `select 1` ve ilk şema | Geçmiş tutmaz; kalıcı yol CLI migration (§3.5). |

---

## 2. Mimari seçenekleri

Ortak gerçekler: UE sunucusu zaten sunucu-otoriter; oyun kodu `IMMOCharacterStore` arkasında [Kesin]. DB'ye yazan her şey **sunucu**dur (istemci asla). Bu yüzden asıl soru "hangi kimlik/anahtarla, hangi mantık katmanıyla" konuşulduğudur.

| | A) UE -> PostgREST **tablo** uçları (secret key) | B) UE -> **kendi ara servisi** (S13) -> Postgres | C) UE -> PostgREST **RPC** (mantık SQL'de) [+ isteğe Edge Function] |
|---|---|---|---|
| Anahtar | Publishable (eski anon) ile RLS: **olmaz**; çünkü çağıran "kullanıcı" değil "oyun sunucusu", `auth.uid()` yok, istemci de aynı anahtarı görür. Pratikte secret/service_role gerekir. | Harita sunucuları **DB anahtarı görmez**; servis DB bağlantı dizesini/pooler'ı tutar, sunucular yalnız servis bileti ile konuşur. | Secret key sunucu makinesinde ortam değişkeninde. 6 harita süreci aynı anahtarı görür (ayrı secret key üretilebilirse süreç başına bir tane [Doğrulanmadı]). |
| Güvenlik sonucu | RLS atlanır (secret key RLS'i atlar); mantık UE'de; bir sunucu ele geçerse tüm tablolara yazar. En zayıfı. | En güçlü: en az ayrıcalık, her sunucunun yetkisi servis kurallarıyla sınırlı, hız sınırı/denetim tek noktada. Ama servis kendisi saldırı yüzeyi ve işletme yükü. | Orta: ele geçirilen sunucu yalnız `mmo_api` fonksiyonlarını çağırabilir (tablolara doğrudan değil, çünkü `mmo` şeması API'ye açık değil); fonksiyonlar lease/epoch ile "başkasının karakterini yazma"yı reddeder. Ama secret key hâlâ geniş bir kimlik bilgisidir. |
| Atomiklik / revision | CAS tek istekte olur (`PATCH ?revision=eq.N`), ama lease + işlem günlüğü + yan veri birlikte olmaz. | Serviste transaction (dile bağlı). | Fonksiyon tek transaction: kilit, revision, işlem günlüğü, yan veri birlikte. Pazar/posta (S12/S16) için aynı yol. |
| Gecikme | 1 HTTPS gidiş-dönüş (Supabase bölgesine, TLS) | 2 sıçrama (UE->servis->DB), servis aynı makinede olursa küçük | 1 HTTPS gidiş-dönüş + PostgREST ek yükü. Bölge Frankfurt gibi AB ise Türkiye'den onlarca ms [Tahmin]; kayıt asenkron ve periyodik, oyun döngüsü beklemez. |
| Ücretsiz/Pro sınırları | PostgREST istek sayısı pratikte sınır değil; DB boyutu, bant genişliği, otomatik duraklatma, yedek kapsamı plana bağlı. Boş projenin duraklatılması (ücretsiz katman) DB'yi kapatır **[Doğrulanmadı]**. | Doğrudan bağlantı IPv6 olabilir, IPv4 için pooler/eklenti gerekebilir; bağlantı sayısı sınırı vardır **[Doğrulanmadı]**. Servis kısa ömürlü bağlantı havuzu tutar. | PostgREST bağlantıyı kendi havuzlar; harita sunucuları bağlantı sayısını tüketmez (bu B'ye göre bir avantaj). |
| Arızada davranış | Aynı: HTTP hata/zaman aşımı = belirsiz sonuç; S04 kuralı: ilerleme yok. | Aynı + servis çökmesi ek arıza noktası. | Aynı. DB duraklatılmış/kapalı = HTTP hata -> durum ilerlemez. |
| Sunucu çökmesi + idempotent yazım | `Prefer`/`on_conflict` ile kısmen; işlem günlüğü yok. | Serviste uygulanır. | `op_id` + yük özeti işlem günlüğünde; aynı `op_id` tekrarı kayıtlı sonucu döner (§3.4). |
| Test edilebilirlik | HTTP taklidi kolay; mantık UE'de olduğu için UE testinde. | Servis ayrı test; UE tarafı mock servisle. | İki katmanlı: (1) UE native testi sahte ulaşım katmanıyla (headless), (2) SQL sözleşme testi yerel PostgreSQL'e karşı (roller test başlığında yaratılır), (3) gerçek Supabase entegrasyon betiği (yalnız kullanıcı anahtarlarıyla). |
| İşletme yükü | Düşük | Yüksek (dil kararı S13 açık, barındırma, güncelleme) | Düşük (SQL migration + CLI) |

**Seçim: C**, gerekçeleri: (1) alfa için en az hareketli parça, ek sunucu yok; (2) kritik mantık (revision, fencing, idempotency) tek transaction'da ve dilden bağımsız; S04.00 kabul tablosundaki "eski revision / iptal edilmiş epoch / yinelenen idempotency anahtarı" satırları doğrudan SQL'de uygulanır; (3) UE tarafı ince kalır (HTTP + JSON), `IMMOCharacterStore` mimarisi değişmez; (4) B'ye geçiş kolay: aynı sözleşme (§4) servis URL'sine yönlendirilir, UE kodu aynı kalır. **C'nin bedeli:** harita sunucuları DB düzeyinde geniş bir anahtar taşır. Kapalı alfada tüm süreçler tek kullanıcının makinesinde olduğu için kabul edilebilir [Tahmin]; açık/halka açık sürümden önce B'ye geçiş şartı koy (Karar 1).

**S13 ile ilişki:** S13(a) gerekçesi "Steam Publisher Key harita sunucularına dağıtılmaz". C bunu bozmaz: Publisher Key yine yalnız giriş servisinde (Edge Function ya da Python). C'nin gevşettiği tek şey "harita sunucusu hiç DB anahtarı taşımaz" ilkesidir. Bu yüzden S13'ü yeniden açmıyoruz; yalnız DB erişim yolu (plan §4.3 D4) için "alfa: RPC, hedef: servis" ek kararı istiyoruz.

---

## 3. Veri modeli

### 3.1 V1 şeması ve bugünkü kayıt (özet; ayrıntı `SIX_MAP_PLAN.md` §1.5)
Fark listesi (ırk, kanonik ad, `accounts`, lease/revision, eksik alanlar, başarım/günlük/posta, int32 vs BIGINT) [Kesin]. Plan kararı: karakter verisi ilişkisel bölünmez, `FMMOCharacterSaveJson` **SchemaVersion 2 zarfı** `jsonb`'de saklanır; V1'in normalize karakter tabloları alfada kullanılmaz.

**UYARI (V1'i Supabase'e UYGULAMA):** `Migrations/V1__initial_schema.sql` tabloları `public` şemasında RLS'siz yaratır. Supabase'de `public` şeması varsayılan olarak PostgREST'e açıktır [Doğrulanmadı ama yaygın bilinen varsayılan]; V1 uygulanırsa publishable key'e sahip herkes (anahtar istemci/ağ görünür olabilir) tabloları okuyup yazabilir. V1 hiçbir DB'ye uygulanmadı [Kesin]; Supabase'e uygulanmamalı. Ek: V1 `uuid-ossp` ister (Supabase'de `gen_random_uuid()` yerleşik, uzantı gerekmez).

### 3.2 V2 taslak şema (tasarım anı taslağı; gerçek dosya `supabase/migrations/20261017120000_v2_character_store.sql` YAZILDI ama HİÇBİR DB'ye UYGULANMADI)

> Bu bölümdeki SQL 71c548f taslağıdır ve migration'dan SAPAR (sapma listesi: `supabase/README.md` "Sapmalar" 1-13): `force row level security` yok (yalnız `enable`; SECURITY DEFINER fonksiyonlar sahip olarak çalışır), `lease_ttl_s` / `import_sha256` ek sütunları, veri tavanı 2 MiB (aşağıdaki 1 MiB değil), zarf denetimi `check` değil tetikleyici, revision/lease_epoch geri gitmez tetikleyicisi, fonksiyonlar `set search_path = ''`. Gerçek şema için migration'a bak.

Tasarım kuralları: tablolar `mmo` şemasında (API'ye açık değil), fonksiyonlar `mmo_api` şemasında, yalnız `service_role` EXECUTE; her tabloda RLS **açık + zorlanmış + politika yok**; `anon`/`authenticated` hiçbir yetkiye sahip değil. Yeni fonksiyonlarda PostgreSQL varsayılanı EXECUTE'u PUBLIC'e verir: açıkça geri alınır.

```sql
create schema if not exists mmo;
create schema if not exists mmo_api;
revoke all on schema mmo, mmo_api from public, anon, authenticated;
grant usage on schema mmo, mmo_api to service_role;

-- Kanonik anahtar: MMONameRules::CanonicalKey ile ayni kural (SlotKey + kucuk harf).
-- Iki dilde ayni kural = surukleme riski: kontrat vektorleri ile test edilir (3.6).
create function mmo.canonical_key(p_name text) returns text
language sql immutable set search_path = '' as $$
  select left(lower(regexp_replace(replace(p_name, ' ', '_'), '[^A-Za-z0-9_-]', '', 'g')), 32)
$$;
-- Not: UE SlotKey gecersiz karakterleri atar, bosluk -> '_' yapar, 32 karakterde durur (okumayla
-- left(temizlenmis, 32) ile esdeger gorunuyor; SQL calistirilmadi, kontrat vektorleri dogrular).
-- Bos sonuc UE'de "default" olur; ayrilmis ad oldugu icin DB'de reddedilir.

create table mmo.accounts (
  account_id    uuid primary key default gen_random_uuid(),
  provider      text not null check (provider in ('steam', 'dev')),
  external_id   text not null check (external_id ~ '^[0-9A-Za-z_-]{1,64}$'),  -- Steam: 64-bit SteamID ondalik METIN
  is_admin      boolean not null default false,
  created_at    timestamptz not null default now(),
  last_login_at timestamptz,
  unique (provider, external_id)
);

create table mmo.character_saves (
  canonical_key  text primary key
    check (canonical_key ~ '^[a-z0-9_-]{1,32}$'
           and canonical_key <> 'default' and canonical_key !~ '^player[0-9]+$'),  -- IsReservedName
  character_id   uuid not null unique default gen_random_uuid(),   -- S06 StableCharacterId icin hazirlik
  display_name   text not null check (char_length(display_name) between 1 and 32
                                       and canonical_key = mmo.canonical_key(display_name)),
  account_id     uuid references mmo.accounts(account_id),         -- NULL = sahipsiz/eski (S10 eski karakter karari)
  race           text,                                             -- deger kumesi S03 kabulunden sonra CHECK olur
  schema_version int  not null check (schema_version >= 1),
  revision       bigint not null default 0 check (revision >= 0),
  data           jsonb not null check (jsonb_typeof(data) = 'object'
                                       and octet_length(data::text) <= 1048576),  -- 1 MiB [Tahmin]; MMOTestStore maxJsonChars ile olculur
  active_server  text,                    -- kira tutan: '<server_id>:<boot_guid>'
  lease_epoch    bigint not null default 0,
  lease_until    timestamptz,
  created_at     timestamptz not null default now(),
  updated_at     timestamptz not null default now()
);
create index on mmo.character_saves (account_id) where account_id is not null;
create index on mmo.character_saves (lease_until) where active_server is not null;

create table mmo.character_side (       -- basarim / gunluk / (posta: S16 karar bekliyor)
  canonical_key text not null references mmo.character_saves(canonical_key) on delete cascade,
  kind          text not null check (kind in ('achievements', 'daily', 'mail')),
  data          jsonb not null check (jsonb_typeof(data) = 'object' and octet_length(data::text) <= 262144),
  revision      bigint not null default 0,
  updated_at    timestamptz not null default now(),
  primary key (canonical_key, kind)
);

create table mmo.write_ops (            -- idempotency gunlugu
  op_id          uuid primary key,
  canonical_key  text not null,
  op_kind        text not null,
  payload_sha256 text not null,
  result         jsonb not null,
  created_at     timestamptz not null default now()
);
create index on mmo.write_ops (created_at);   -- temizlik; saklama suresi tekrar-deneme ufkundan uzun olmali (7 gun [Tahmin])

create table mmo.presence (             -- A4'te kullanilir
  canonical_key text primary key references mmo.character_saves(canonical_key) on delete cascade,
  server_id     text not null,
  heartbeat_at  timestamptz not null default now()
);

create table mmo.travel_tickets (       -- A3/S07'de kullanilir; duz bilet DEGIL, ozeti saklanir
  ticket_sha256 text primary key,
  canonical_key text not null references mmo.character_saves(canonical_key) on delete cascade,
  account_id    uuid references mmo.accounts(account_id),
  from_server   text not null,
  to_server     text not null,
  expires_at    timestamptz not null,
  used_at       timestamptz,
  created_at    timestamptz not null default now()
);

do $$ declare t text; begin
  for t in select unnest(array['accounts','character_saves','character_side','write_ops','presence','travel_tickets']) loop
    execute format('alter table mmo.%I enable row level security', t);
    execute format('alter table mmo.%I force row level security', t);   -- politika yok = herkese ret (service_role BYPASSRLS ile gecer)
    execute format('revoke all on mmo.%I from public, anon, authenticated', t);
  end loop;
end $$;
```

Notlar: (a) `character_id` ve `account_id` NULL'lanabilirliği: K-1 kapanana kadar (S05/S06) sahiplik uygulanmaz, bkz. §5.2. (b) Yeniden adlandırma anahtarı değiştirir: ilk dilimde yok; ayrı `rename_character` RPC (tek transaction, hedef anahtar yoksa) S04 sonrası. (c) Irk `CHECK`'i S03 (ırk deposu) kabulüne bağlı; `MMORaceStore` commit edilmemiş, burada kopyalanmadı [Kesin: git status'ta `??`].

### 3.3 RPC sözleşmesi (`mmo_api`, hepsi `jsonb` döner; HTTP 200 + `outcome` alanı)

HTTP hata kodları yalnız altyapı arızası içindir; **iş sonucu** `outcome`'da gelir. Bu, S04.00'ın "bool başarısızlığı aşırı yüklemeyin" ilkesidir. `outcome` kümesi (S04.00 §Proposal ile hizalı): `ok`/`committed`, `not_found` (otoriter yokluk), `already_exists`, `invalid`, `revision_conflict`, `ownership_lost`, `idempotency_conflict`, `forbidden`, `internal`. "Belirsiz (zaman aşımı)" DB'de değil istemcide oluşur (§4.3).

| Fonksiyon | Girdi | Davranış |
|---|---|---|
| `acquire_lease` | `p_key, p_holder, p_ttl_s` (**`p_op_id` YOK**) | Yok: `not_found`. Başka kiracı ve kira geçerli: `ownership_lost` (+`retry_after_s`). Aksi: `lease_epoch+1`, `active_server=p_holder`; **veri + side + revision + `lease_epoch` + schema_version** döner (yükleme budur). Aynı holder geçerli kirayı yeniden alırsa epoch artmaz, kira uzar; süresi dolmuş/bırakılmış kirada artar. Kayıtlı sonuç tekrarı eski veriyi döndürebileceği için `p_op_id` yoktur; çağrı aynı sahip için doğal idempotent. |
| `create_character` | `p_key, p_display_name, p_holder, p_op_id, p_schema_version, p_data, p_race` | `insert ... on conflict do nothing`; varsa `already_exists`. Başlangıç paketi kararı yalnız `acquire_lease -> not_found` sonrası verilir (S04.00: DB kapalı = "yok" DEĞİL). **Sonuç belirsizliği:** yeniden deneme AYNI `p_op_id` ile yapılır (kayıtlı sonuç `replayed:true`); yeni op_id ile `already_exists` "kendi önceki commit'imiz" ile "başkası aldı"yı ayırt ettirmez, ayırmak için ilk op_id ile `get_op` (`found:true`, `op_kind:create`). |
| `save_character` | `p_key, p_holder, p_epoch, p_expected_revision, p_op_id, p_schema_version, p_data, p_side jsonb` | Aşağıdaki gövde. İsteğe bağlı `p_side` ile başarım/günlük aynı transaction'da. |
| `renew_lease` | `p_key, p_holder, p_epoch, p_ttl_s` | Yalnız kira uzatır; veri yazmaz. |
| `release_lease` | `p_key, p_holder, p_epoch` | Çıkışta kira bırakır (kaydı `save_character` ile bitirdikten sonra). Çökmede kira süresiyle düşer. |
| `character_exists` / `list_keys(p_prefix)` | | Ad çakışması / klan adı taraması (şu an `Exists`/`ListSlotNames`). Otoriter; yalnız `outcome` + liste. |
| `get_op` | `p_op_id` | Zaman aşımı sonrası uzlaştırma: işlem kayıtlı mı, sonucu ne. Bulunursa `{outcome:'ok', found:true, op_kind, canonical_key, result, created_at}`; bulunamazsa `{outcome:'not_found', found:false}` (temizlenmiş >= 7 gün eski op_id de `found:false`). |
| `import_character` | `p_key, p_display_name, p_source_sha256, p_schema_version, p_data, p_race` (**`p_op_id` YOK**; idempotency `p_source_sha256` ile) | İdempotent içe aktarma (§4.6). Mevcut ve `revision > 0` ise **asla ezmez**. |
| `purge_write_ops` | `p_older_than_days` (varsayılan 7; 1..3650), `p_batch` (varsayılan 5000; 1..50000, dışı `invalid/bad_batch`) | İşlem günlüğü (`write_ops`) temizliği. Yalnız `service_role`; son 24 saat korunur; tek çağrıda en çok `p_batch` satır, `{outcome:'ok', deleted, more}`; yalnız `write_ops` silinir. Fonksiyon döngü yapmaz (tek işlem = kısa kilit; `purge_write_ops_all` bilinçli yok): `more:true` ise çağıran yeniden çağırır. Boyut gerekçesi: dakikada bir kaydeden 4 oyuncu ≈ günde 5760 satır, ücretsiz katman 500 MB; bu yüzden günde bir 5000 satır yetmez, `pg_cron` 5 dakikada bir `purge_write_ops(7, 20000)` (örnek `supabase/README.md`). |
| Ölçüm notu | boyut tavanları | `data_max_octets` (2 MiB) ve yan veri 256 KiB SQL'de `octet_length(jsonb::text)` ile ölçülür (bayt; `jsonb::text` ayraçlardan sonra boşluk koyar, UE'nin yoğun JSON'undan uzun). UE karakter sayıyorsa tavana yakın kayıt UE'de geçip DB'de `data_too_large` alabilir; davranış değişmedi, `vectors.json` `limits.data_octet_limit_note`. |
| Dondurulmuş yuva | `acquire_lease` / `release_lease` | `acquire_lease` canlı kirada aynı sahip için epoch'u ARTIRMAZ (bilinçli). Geç yazım / donmuş yuva durumunda epoch'u artırmak için UE `release_lease(p_epoch)` sonra `acquire_lease` çağırır: `lease_epoch + 1`, eski epoch ile `save_character` -> `ownership_lost` (senaryo `frozen_slot_epoch_bump`, `vectors.json`). |

**Sözleşme sürümü 2 (2026-10-17, henüz hiçbir DB'ye uygulanmadı; tek doğruluk kaynağı `supabase/tests/vectors.json` + `supabase/README.md`):**
- Kira dönemi alanı TEK ad **`lease_epoch`** (`load_character`, `acquire_lease`, `renew_lease`, `release_lease`, `create_character`, `save_character`, `ownership_lost` gövdeleri). Geriye dönük `epoch` aynı değerle yanıtlarda kalır; yeni kod yalnız `lease_epoch` okur.
- `p_op_id` yalnız `create_character`, `save_character`, `get_op`'ta. `acquire_lease`, `renew_lease`, `release_lease`, `character_exists`, `import_character` almaz; UE bu çağrılara `p_op_id` göndermeyecek. Fazla/eksik parametre = HTTP 404 `PGRST202` (SQL `42883`).
- `release_lease` yanıtı tek şekil: `{outcome:'ok', already_released:bool, lease_epoch, epoch}`. `healthcheck`: `{api_version:2, contract_version:2, ...}`.
- Zaman kararları (kira süresi dolmuş mu, yeni bitiş) satır/op_id kilidi beklendikten SONRA `clock_timestamp()` ile alınır; zarf (`data`) denetimi CHECK değil tetikleyicidir (yalnız veri değişince çalışır).
- Gelecek migration'lar: yeni fonksiyon eklenirse dosya sonunda revoke/grant döngüsü tekrarlanır; sema düzeyi `alter default privileges` PUBLIC'in yerleşik EXECUTE'unu kaldırmaz (ayrıntı `supabase/README.md` sapma 12).

`save_character` çekirdeği (71c548f taslağı; düzen önemli. Gerçek fonksiyon `SECURITY DEFINER`, zamanı kilit SONRASI `clock_timestamp()` ile alır, `lease_epoch` döner, doğrulama hataları `invalid/<reason>`; aşağıdaki `security invoker` / `now()` / `epoch` taslak dilidir):

```sql
create function mmo_api.save_character(p_key text, p_holder text, p_epoch bigint, p_expected_revision bigint,
                                       p_op_id uuid, p_schema_version int, p_data jsonb, p_side jsonb default null)
returns jsonb language plpgsql security invoker set search_path = '' as $$
declare r mmo.character_saves; prev mmo.write_ops; dig text; res jsonb;
begin
  -- 1) SATIR KILIDI ILK: ayni op_id'nin es zamanli iki kopyasi burada sirayla gecer.
  select * into r from mmo.character_saves where canonical_key = p_key for update;
  if not found then return jsonb_build_object('outcome','not_found'); end if;
  dig := encode(sha256(convert_to(p_key||'|'||p_expected_revision::text||'|'||p_data::text||'|'||coalesce(p_side::text,''),'utf8')),'hex');
  -- 2) Kayitli islem: kira/revision'a BAKMADAN kayitli sonucu don (commit oldu, sonradan kira bitmis olabilir).
  select * into prev from mmo.write_ops where op_id = p_op_id;
  if found then
    if prev.canonical_key = p_key and prev.payload_sha256 = dig then return prev.result; end if;
    return jsonb_build_object('outcome','idempotency_conflict');
  end if;
  -- 3) Fencing: eski/iptal edilmis epoch ve sure dolmus kira reddedilir.
  if r.active_server is distinct from p_holder or r.lease_epoch <> p_epoch
     or r.lease_until is null or r.lease_until <= now() then
    return jsonb_build_object('outcome','ownership_lost');
  end if;
  -- 4) Karsilastir-ve-yaz.
  if r.revision <> p_expected_revision then
    return jsonb_build_object('outcome','revision_conflict','revision',r.revision);
  end if;
  update mmo.character_saves set data = p_data, schema_version = p_schema_version,
         revision = r.revision + 1, lease_until = now() + interval '120 seconds', updated_at = now()
   where canonical_key = p_key;
  -- (p_side varsa: character_side satirlari ayni transaction'da upsert)
  res := jsonb_build_object('outcome','committed','revision', r.revision + 1);
  insert into mmo.write_ops(op_id, canonical_key, op_kind, payload_sha256, result)
       values (p_op_id, p_key, 'save', dig, res);
  return res;
end $$;
revoke execute on function mmo_api.save_character(text,text,bigint,bigint,uuid,int,jsonb,jsonb) from public, anon, authenticated;
grant  execute on function mmo_api.save_character(text,text,bigint,bigint,uuid,int,jsonb,jsonb) to service_role;
```
Kira süresi 120 sn [Tahmin], `renew_lease` ile/yazımla uzar. **R1 (S04.00) ile ilişki:** epoch fencing, kira devri sonrası geç gelen eski sunucu yazımını kesin reddeder; ama devirden ÖNCE gönderilip zaman aşımına uğrayan bir yazım commit olmuş olabilir. Bu "belirsiz" durum istemcide `get_op` ile uzlaştırılır; R1 tamamen kapanmaz, daralır (not düşülür).

### 3.4 İdempotency ve belirsiz sonuç
`op_id` istemcide bir mantıksal yazım için BİR KEZ üretilir (UUID), tüm tekrar denemelerde aynı kalır. Yük aynı: kayıtlı sonuç; yük farklı ama aynı `op_id`: `idempotency_conflict`. Reddedilen işlemler kaydedilmez (tekrar aynı kuralı verir).

### 3.5 Migration akışı: Flyway adı mı, Supabase CLI mi
Çakışma: `Migrations/V1__initial_schema.sql` Flyway adı (`V<n>__ad.sql`); plan A2 metni `V2__character_store.sql`'i `Migrations/` altında öngörür. Supabase CLI `supabase/migrations/<YYYYMMDDHHMMSS>_ad.sql` ister. **Öneri: tek kaynak `supabase/migrations/`** (Flyway kullanılmıyor; makinede psql/Flyway yok [Kesin]). `Migrations/V1__...sql` "kullanılmıyor, UYGULAMA" olarak kalır (S15 kararı: silme/işaretleme); plan A2 metni belge güncellemesiyle düzeltilir (`belge-denetcisi`). Akış: `supabase init` -> `supabase link --project-ref <ref>` -> `supabase db push` (uzak projeye uygular; DB şifresi ister; Docker gerekmeyebilir [Doğrulanmadı]) ; Docker bu makinede yok, yerel `supabase start` mümkün değil -> geliştirme için ayrı bulut projesi (§6). Yedek yol: SQL Editor'e yapıştırma (geçmiş tutmaz, yalnız ilk deneme). `supabase/.temp/` ve olası `supabase/.env*` `.gitignore`'a eklenir (ilk uygulama diliminde; `.env` zaten ignore).

### 3.6 Kontrat vektörleri
Tek bir `Scripts/`-altı JSON vektör dosyası (ör. `supabase/tests/vectors.json`, ilk uygulama diliminde): ad -> beklenen kanonik anahtar (`MMONameRules::CanonicalKey`), ayrılmış adlar, ve RPC senaryoları (girdi -> `outcome`). Hem UE native testi (sahte ulaşım) hem SQL testi hem entegrasyon betiği aynı dosyayı okur; böylece sahte ile gerçek ayrışmaz.

### 3.7 İlk yedek ve geri dönüş
- İçe aktarma kaynağa (.sav / yan JSON) **yazmaz/silmez**; `Saved/` kopyası alınır, hash'lenir (S04 sınırı). Aracı canlı oyuncu `Saved/` dosyasında koşmak ayrı onay ister.
- DB yedeği: ücretsiz katmanda otomatik günlük yedek olmayabilir; Pro'da günlük yedek ve PITR eklentisi vardır **[Doğrulanmadı]**. Plan bağımsız: yerel `export_all` aracı (service key ile `list_keys` + `acquire`'sız salt-okuma RPC) JSON dosyalarına döker. Alfa için yedeği sağlayana kadar DB'yi tek kopya sayma.
- Geri dönüş (rollback) iki yönlüdür: bayrak dosya deposuna döndürülürse DB'de yazılan ilerleme dosyada YOKTUR. Bu yüzden DB moduna geçiş **tek yönlü** kabul edilir ya da ayrı bir "DB -> .sav dışa aktarma" aracı (S04 dışında, isteğe bağlı) yazılır. Karar 7.

---

## 4. UE tarafı

### 4.1 Sınıflar (hepsi YENİ dosya, `Public/Save/` + `Private/Save/`; Dilim 2'de YAZILDI, `e0fffdf`..`342ab17`)
- `IMMOHttpTransport` (`MMOHttpTransport.h`; ince arayüz: `Send(Request, TFunction<void(Response)>)`), gerçeği `FMMOUnrealHttpTransport` (`FHttpModule`), sahtesi `MMOSupabaseStoreSelfTest.cpp` içinde. Oyun ipliği sözleşmesi: tamamlanma geri çağrıları oyun iş parçacığında döner (`FMMOSlotWriteTracker` `check(IsInGameThread())` ister [Kesin]).
- `FMMOSupabaseRestClient`: URL, anahtar (ortam), zaman aşımı, yeniden deneme, anahtar maskeleme; JSON kur/çöz.
- `FMMOSupabaseCharacterStore : IMMOCharacterStore`: `LoadAsync`/`SaveAsync`/`Exists`... asenkron; dahili `TMap<CanonicalKey, FSlotState{Epoch, Revision, Holder}>`; yuva başına seri yazım kuyruğu (aynı anda tek uçuştaki yazım; yenisi kuyruktakini birleştirir).

### 4.2 Arayüz sorunu (kritik, S04.00 ile uyumlu)
`IMMOCharacterStore` senkron `Load/Save/Exists/ListSlotNames` (bool) ister [Kesin]; HTTP oyun ipliğini bloklayamaz. Uzak depoda senkron `Load` -> `false` döndürmek "kayıt yok" ile karışır ve **başlangıç paketi/ezme** hatasına yol açar (S04.00: kapalı DB "yok" değildir). Bu yüzden:
1. Arayüze **eklemeli** (kırmayan) sonuç-türlü asenkron yöntemler: `LoadAsyncEx(Slot, Completion(EMMOStoreOutcome, Data&&))`, `SaveAsyncEx(...)` vb. Varsayılan gerçeklemeleri mevcut bool yöntemlerine düşer -> yerel depo ve mevcut çağıranlar değişmez. **Gerçekleşen (Dilim 2) farkı:** `IMMOCharacterStore` DEĞİŞTİRİLMEDİ; sonuç-türlü yöntemler ayrı arayüz `IMMOAsyncCharacterStore`'da (`MMOSupabaseCharacterStore.h`: `LoadEx`, `CreateEx`, `SaveEx`, `RenewLeaseEx`, `ReleaseLeaseEx`, `ReconcileEx`, `HealthcheckEx`, `IsSlotFrozen`; sonuç türü `EMMOSupabaseStoreOutcome`). `IMMOCharacterStore`'a ortak bir `*AsyncEx` eklemek ve depo seçimi Dilim 3'ün işidir (yapılmadı).
2. `FMMOSupabaseCharacterStore` senkron yöntemlerde **kapalı-başarısız** davranır (`false` + `LogMMOStore` Error; üretimde çağrılmamalı); doğru çağıranlar `*Ex` yollarına taşınır. Bu çağıranların listesi S04.00'da (MMOCharacter.cpp otomatik kayıt/ilk yükleme/manuel Save, MMOGameMode.cpp geçiş, Clan/Friends/Mail/Trade) [Kesin]; **dilim 4** onların işidir ve en ağır parçadır (§7).
3. Yazma-arkada (write-behind) önbellek **reddedilir**: sync `Save`'in önbelleğe yazıp `true` dönmesi "DB kapalıyken durum ilerlemez" kuralını (S04) bozar; özellikle takas/posta/pazar için.

### 4.3 Zaman aşımı, yeniden deneme, iptal
- İstek başına zaman aşımı [Tahmin] 8 sn; mantıksal işlem bütçesi (tüm denemeler) 20 sn. Yeniden deneme yalnız ağ hatası, 408, 429, 5xx; en çok 3 deneme, üstel bekleme + jitter. 4xx (401/403/400/404) yeniden denenmez (anahtar/şema hatası).
- **Yeniden deneme güvenli:** her deneme aynı `op_id`, aynı yük, aynı `expected_revision`. DB ya zaten commit etmişse kayıtlı sonucu döner; etmemişse yazar.
- Bütçe bitince sonuç **belirsiz**: önce `get_op(op_id)` ile uzlaştırma denenir; o da ulaşamazsa durum "unknown": bağımlı ilerleme (bölge geçişi, ödül, takas) **donar**, kör geri alma/yeniden yazma yok (S04.00). Çözüm: DB'ye ulaşılınca uzlaştırma, aksi hâlde yetkili yeniden yükleme.
- İptal: `FHttpRequest::CancelRequest()` sunucu tarafındaki commit'i iptal etmez; yalnız yerel geri çağrıyı keser (`NETWORK_AUDIT.md` O6 sınırı ile aynı). Kapanışta (`Deinitialize`) uçuştaki istekler sahipli duruma alınıp bekletilir/uzlaştırılır; `TUniquePtr` ile mağaza silinirken geri çağrı süresi içinde `TWeakPtr` ile korunur.

### 4.4 Yapılandırma ve gizli anahtar
- **Anahtar yalnız ortam değişkeni** (örn. `MMO_SUPABASE_SECRET_KEY`), mevcut desen: `MMODialogueSubsystem` `FPlatformMisc::GetEnvironmentVariable` [Kesin]. **Komut satırı ile verilmez**: UE komut satırını log dosyasına ve çökme raporuna yazar [Doğrulanmadı ama bilinen davranış]; süreç listesinde de görünür.
- URL gizli değildir: `-MMOSupabaseUrl=https://<ref>.supabase.co` ya da `MMO_SUPABASE_URL`. Depo seçimi: `-MMOCharacterStore=file|supabase` (varsayılan **file**). `supabase` seçili ve URL/anahtar yok veya doğrulanamıyor -> sunucu **başlamaz** (kapalı-başarısız); sessizce dosyaya DÜŞMEZ (aynı karakter iki depoda çatallanır). Tersine "geri dönüş bayrağı" = bilinçli `-MMOCharacterStore=file` ile yeniden başlatma, otomatik değil.
- `Config/*.ini`'ye URL/anahtar YAZILMAZ (`Config/` repo'da; ayrıca `Saved/Config/` yedeklenip paylaşılabilir). Anahtar dosyası gerekirse `.env`/`.env.*` (zaten `.gitignore`'da [Kesin]) repo dışında bir yerde; sunucu başlatma betiği ortama yükler.
- Log: her giden istek/URL/başlık `Redact()` ile geçer; anahtar `sb_secret_****(len)` biçiminde; `LogHttp` Verbose/VeryVerbose üretimde KAPALI (başlıkları basabilir). Yük gövdesi Log seviyesinde basılmaz (karakter verisi). Test: anahtarın log dosyasında geçmediğini arayan negatif test.
- Ağ yüzeyi: yalnız `https`; sertifika doğrulaması kapatılmaz; DNS/zaman aşımı hatası = kapalı.
- **`holder` üretimi (N1, güvenlik turu 3):** donmuş yuva düzeltmesi (release -> acquire ile epoch artırma) `holder`'ın **süreç başına benzersiz** olduğunu varsayar. İki süreç aynı holder'ı kullanırsa B'nin aldığı kira (yeni epoch) A tarafından devralınır ve çit (fencing) aşılır; DB iki süreci ayırt edemez. Kural: `holder` = sunucu kimliği + açılış rastgeleliği (boot nonce/GUID), örn. `map01-3f9c2a1be07d4c55`; her süreç açılışta yeni üretir, yeniden başlatılan süreç eski holder'ı kullanmaz. UE deposu açılışta holder'ı `^[A-Za-z0-9_.:-]{1,96}$` ile doğrular (geçersizse depo açılmaz). **Aynı holder iki süreçte KULLANILMAZ (işletme kuralı)**; DB bunu zorlayamaz. Önerilen uygulama (Dilim 3): holder'ı elle verilen sabit bir dize olarak değil, kimlik + GUID'den depo üretsin (`-MMOServerId=map01` yalnız önek); elle `-MMOHolder=` verilirse uyarı basılsın. Çift başlatma tespiti için ek öneri: açılışta `acquire_lease` yanıtı beklenmedik biçimde "başkasının epoch'u" ya da `ownership_lost` döndürürse ikinci süreç kendini durdurur (best-effort; aynı dize ayrımı DB'de mümkün olmadığından kesin tespit değildir).
- **`purge_write_ops(p_batch)` tip notu (N3):** `p_batch` (ve `p_older_than_days`) **tam sayı** gönderilmelidir; ondalıklı değer PostgREST'te `bad_batch` değil tip dönüştürme hatası (HTTP 4xx, `outcome` yok) verir. Çağıran tam sayı üretir.

### 4.5 Anahtar türü
Yeni API anahtarı modeli: `sb_publishable_...` (eski anon yerine) ve `sb_secret_...` (eski service_role yerine); eski JWT anon/service_role anahtarları da bir süre var **[Doğrulanmadı: güncel durum ve kaldırma takvimi]**. Öneri: **secret key**, `apikey` başlığında; ayrı secret key üretilebiliyorsa harita sunucusu başına bir tane (biri sızarsa yalnız o iptal) [Doğrulanmadı]. Eski `service_role` JWT'si seçilirse `apikey` + `Authorization: Bearer` ikisi de gerekir; süresiz geçerlidir, döndürmesi proje JWT sırrını değiştirmeyi gerektirebilir (daha ağır) [Doğrulanmadı]. **Publishable/anon anahtar oyun sunucusu için kullanılmaz.** İstemciye hiçbir Supabase anahtarı girmez.

### 4.6 İçe aktarma aracı (.sav + yan veri -> DB)
- Biçim: mevcut `FMMOCharacterSaveJson::ToJson` (v2, int64 metin) çıktısı = `data`. Araç ayrı bir editör/komutlet (`UCommandlet`) ya da salt-okunur UE komutu; çalıştırılmadan önce tutarlı **kopya** üzerinde çalışır (S04 sınırı: `Saved/` canlı dosyaya yazma/silme yok; `TryLoad` yan etkileri nedeniyle başarım/günlük/posta yükleyicileri canlıda çağrılmaz [Kesin, board]).
- İdempotent: `source_sha256` (kaynak dosya baytlarının özeti) + `canonical_key`; aynı özet ikinci kez: `already_imported`; farklı özet ve hedef `revision > 0`: `conflict` (ezmez, rapor eder). Tekrar çalıştırma ve yarım kalma güvenli.
- Yan veri (başarım/günlük): `character_side` `kind` ile; posta S16 kararına kadar içe aktarılmaz (kutular ayrı raporlanır, sessizce boş sayılmaz).
- Ayrılmış adlar (`default`, `player<rakam>`) ve ASCII dışı eski adlar (CLAUDE.md: eski ASCII dışı kayıtlara migrasyon yok) **atlanır ve raporlanır**.

---

## 5. Güvenlik

### 5.1 Tehdit modeli

| Tehdit | Etki | Önlem | Kalan risk |
|---|---|---|---|
| Secret key sızması (repo, log, çökme raporu, sohbet, ekran görüntüsü) | Tüm DB okunur/yazılır; fonksiyonlar çağrılır | Yalnız ortam değişkeni; `.env*` ignore; log maskeleme + negatif test; komut satırına yazma; sohbete yapıştırma yasağı; sızarsa Dashboard'dan **döndür** (rotate); mümkünse süreç başına ayrı key | Ele geçirilen sunucu makinesi anahtarı okur: alfada kabul, halka açıktan önce B (Karar 1) |
| RLS atlama | Tablo okuma/yazma | `service_role`/secret key RLS'i zaten atlar; bu yüzden **ilk savunma** tabloların API'ye açık olmayan `mmo` şemasında olması + `anon/authenticated`'dan yetki geri alma; RLS ikinci | Dashboard'da `mmo`'yu "exposed schemas"a elle eklemek her şeyi açar: kontrol listesinde yasak (§6) |
| SQL enjeksiyonu | Veri sızması/bozulma | UE tarafında SQL yok; yalnız parametreli RPC (PostgREST parametreleri tiplidir); fonksiyonlarda dinamik SQL yok (`execute format` yalnız sabit tablo adı listesinde, migration içinde); `search_path=''` | Gelecek fonksiyonda dinamik SQL eklenirse QA kapısı |
| İstemci hilesi | Sahte kayıt | İstemci DB'ye hiç gitmez; yük sunucuda üretilir; `FromJson` sıkı doğrulama; DB `check` (tip/boyut) | Sunucu mantığı hatası (ayrı denetimler) |
| Replay / yineleme | Çift ödül, çift yazım | `op_id` + yük özeti; `expected_revision`; epoch | Posta/takas çapraz kayıt işlemleri (S16/S12) ayrı transaction ister |
| Eski sunucu / çift giriş | Eşya kopyalama (iki sunucu aynı karakteri yazar) | Kira + epoch fencing (§3.3) | R1 daralır, kapanmaz (§3.3 notu) |
| Aynı `holder` iki süreçte (N1) | Donmuş yuva release->acquire düzeltmesinde B'nin epoch'u A'ya devrolur, çit aşılır, çift yazım/kopyalama | `holder` = sunucu kimliği + açılış GUID'i (süreç başına benzersiz); UE açılışta `^[A-Za-z0-9_.:-]{1,96}$` doğrular; işletme kuralı: aynı holder iki süreçte kullanılmaz (§4.4) | DB aynı dizeyi ayırt edemez; kural ihlali (elle sabit holder, kopyalanmış başlatma betiği) açık risk. Dilim 3 testi: ikinci süreç reddedilir/uyarı (§7.2) |
| DoS / hız | DB ve istek kotası | Sunucu tarafı yazım birleştirme (yuva başına tek uçuş); autosave aralığı; Supabase tarafı hız/rate koruması planıyla gelir **[Doğrulanmadı]**; PostgREST istek gövde boyutu sınırı ve `check(octet_length)` | Kötü niyetli oyuncu yalnız UE sunucu kurallarına takılır; DB'ye doğrudan ulaşamaz |
| Yedekleme sızması | Kişisel veri | Yedek dışa aktarımı repo/`Saved` dışında; erişim kısıtlı | Operasyonel disiplin |
| Duraklatma/DB kapalı (ücretsiz katman) | Oyun durur | Alfa için Pro ya da düzenli etkinlik **[Doğrulanmadı: duraklatma kuralı]**; durum ilerlemez (S04) | Planlı bakım penceresi |
| Browser-UA reddi [Doğrulandı: dev, 2026-10-17] | Tarayıcı gibi görünen `User-Agent` (Windows PowerShell varsayılanı `Mozilla/5.0 ... WindowsPowerShell`) ile secret key REDDEDİLİR: `{"message":"Forbidden use of secret API key in browser",...}`. UE istemcisi varsayılan UA ile giderse (motorun varsayılanı DOĞRULANMADI) tüm RPC'ler düşer ve "anahtar yanlış" sanılıp anahtar boşuna döndürülebilir; ayrıca anahtarın tarayıcıda kullanılmasına karşı Supabase koruması olduğunu gösterir (anahtar istemciye/web'e girmemeli) | İstemci `Mozilla/Chrome/Safari/Gecko/AppleWebKit/Edge` içermeyen AÇIK UA gönderir (`MMOGameServer/0.1`); `vectors.json` `transport.user_agent_note`; sahte sunucu/UE testi UA kuralını doğrular (Dilim 2 UA, ayrı ajan); öneri: `HealthcheckEx` hatasında "Forbidden use of secret API key in browser" için ayrı teşhis mesajı (yapılmadı) | UA ayarı yapılana kadar UE istemcisi gerçek DB'ye bağlanamayabilir; kural Supabase'in davranışına bağlı, değişirse yeniden doğrula |
| Yanlış migration (V1'i uygulamak, `mmo` şemasını açmak) | Veri herkese açık | Uygulama öncesi kontrol listesi + `select` ile "anon anahtarla `character_saves` 401/404" testi | İnsan hatası |

### 5.2 K-1 (kimlik = oyuncu adı) bu kararla nasıl değişir
**Kendi başına çözmez.** DB tek başına kimlik çözmez: `canonical_key`'i bilen herkes (şimdilik oyuncu adını yazan herkes) o karakteri açar; `accounts` ve `account_id` yalnız Steam doğrulaması (S05) ve bilet (S06/A3) gelince anlam kazanır. Değişenler: (1) PK ile büyük/küçük harf ve boşluk/`_` çakışması DB'de imkânsız (bugün NTFS'e bağlı); (2) çift giriş/çift sunucu yazımı kira+epoch ile reddedilir; (3) sahiplik sütunu hazır: `acquire_lease` ileride `p_account_id` ile `account_id` eşleşmesini ister (NULL=sahipsiz eski karakter, S10 kararına göre sahiplenme). Kapanma şartı hâlâ A5 + A3 (plan I5). **Sonuç: Supabase'e geçmek K-1'i kapatmaz; kapalı alfa bilinen oyuncu adlarıyla ve K-1'in açık olduğu bilinerek yapılır.**

### 5.3 KVKK / GDPR notu (hukuki tavsiye DEĞİL)
Steam ID, oyuncu adı, oyun içi veri kişisel veri sayılabilir. Dikkat edilecekler: (a) proje bölgesi seçimi veri nerede tutulduğunu belirler; AB bölgesi (örn. Frankfurt) GDPR için, Türkiye'de yerleşik veri sorumlusu için KVKK'nın yurt dışına aktarım kuralları ayrıca gündemdedir; gerekirse hukuki danışmanlık al. (b) Silme talebi: `delete_account(account_id)` (karakter, yan veri, sahiplik; `write_ops`/yedek saklama süresi açıklanır) ayrı RPC olarak tasarlanmalı; yedeklerde silme gecikmesi olur. (c) Veri işleyici olarak Supabase (ve Steam) için aydınlatma metni/ gizlilik politikası, kapalı alfa katılımcılarına bildirim. (d) Gereksiz veri toplanmaz: e-posta/parola yok [Kesin, S12]. (e) IP/log saklama süreleri belirlenir. Bu bir gereksinim listesidir, uyum kanıtı değildir.

---

## 6. Kullanıcının yapacakları (ajan yapamaz: hesap, ödeme, anahtar)

**İLKE: anahtarlar, DB şifresi ve proje URL'si dışındaki hiçbir sır SOHBETE YAPIŞTIRILMAZ.** Anahtarı yalnız kendi makinenizde ortam değişkenine / `.env` dosyasına koyun. Yanlışlıkla yapıştırırsanız Dashboard'dan anahtarı hemen döndürün. Proje URL'si (`https://<ref>.supabase.co`) ve proje referansı sır değildir, paylaşılabilir.

Hemen yapılacak 5 madde:
1. Supabase hesabı aç (kendi e-postan; iki adımlı doğrulama aç).
2. **İki ayrı proje** oluştur: `myproject-dev` (çöp veri, test) ve `myproject-alpha` (gerçek). Bölgeyi seçmeden önce Karar 4'ü ver (öneri AB/Frankfurt). Katman: Karar 5.
3. Her proje için **veritabanı şifresini** bir parola yöneticisine kaydet (sohbete yazma).
4. Dashboard -> API Keys bölümünden `secret` anahtarı oluştur/göster ve **yalnız** makinene ortam değişkeni olarak ata (Windows: kullanıcı ortam değişkenleri, `MMO_SUPABASE_SECRET_KEY`); URL'yi `MMO_SUPABASE_URL`. Dashboard arayüz adları değişebilir [Doğrulanmadı].
5. Faturalandırma/katman ve harcama sınırı (spend cap) ayarını kendin gör ve onayla.

Adım adım kontrol listesi (ilk dilim, Dilim 0). **İşaret: [x] = 2026-10-17'de dev projesinde doğrulandı (Doğrulandı: dev); [ ] = yapılmadı/doğrulanmadı.**
- [~] Hesap açıldı (FREE, West EU (Ireland), tek proje `main` dev olarak; ayrı `myproject-alpha` YOK). 2FA/MFA, şifrelerin parola yöneticisinde olduğu doğrulanmadı.
- [x] Dashboard -> Data API ayarında **exposed schemas** listesinde yalnız `mmo_api`; `public` ve `mmo` yok. (`graphql_public`/pg_graphql ayrıca kontrol edilmedi.)
- [x] Ortam değişkenleri ayarlı (`MMO_SUPABASE_URL`, `MMO_SUPABASE_SECRET_KEY`, yerel kullanıcı ortam değişkeni, `Read-Host -AsSecureString`); `.env*` dosyası kullanılmadı.
- [ ] SQL Editor'de `select 1;` -> `1` (psql ile bağlantı denenmedi; SQL Editor/HTTPS zaten çalıştı).
- [x] V2 şemasını uygula: SQL Editor ile hatasız çalıştı. `select count(*) from mmo.character_saves;` -> 0 AYRICA denenmedi (katalog sorgusu ve healthcheck geçti).
- [x] **Güvenlik doğrulaması:** anahtarsız `GET /rest/v1/character_saves` REDDEDİLDİ. (Publishable key ile ayrı deneme YAPILMADI.)
- [x] Secret key ile `healthcheck` HTTPS'den çağrıldı (`apikey` yalnız, `Content-Profile: mmo_api`, Bearer yok, **açık User-Agent**): `{outcome:'ok', api_version:2, contract_version:2, schema:'v2'}`. Adım adım komutlar `supabase/README.md`.
- [x] Katalog: 12 fonksiyon `anon=false/authenticated=false/service_role=true`, 6 tablo RLS açık, anon/authenticated/PUBLIC tablo yetkisi 0.
- [ ] `run_all.sql` (01-04) ve eşzamanlılık testleri (psql gerekir; kurulu değil).
- [ ] Yedek stratejisi: katman ve yedek kapsamı Dashboard'da okunup yazılır; ilk elle dışa aktarım alınır.
- [ ] V1 (`Migrations/V1__initial_schema.sql`) Supabase'e UYGULANMAZ.

---

## 7. Dilimler, çakışma planı, kabul ölçütleri

### 7.1 Çakışma durumu (tasarım anı anlık görüntüsü, 2026-10-17 `git status` / worktree; salt-okunur. Güncel durum için `git status`; Dilim 1-2 yalnız yeni dosya ekledi, listedeki kirli dosyalara dokunmadı)
- `MMOCharacterStore.h`, `MMOCharacterStoreSubsystem.h/.cpp`, `MMOLocalFileCharacterStore.*`, `MMOCharacterSaveJson.*`: **temiz** (status'ta yok).
- **Kirli / başkasında:** `MMOCharacter.cpp/.h` (M, Codex; çağıran geçişi dilim 4 buraya dokunur), `MMOPlayerController.cpp`, `MMOHUD.*`, `Scripts/RunSystemTests.ps1` (M); `MMOTradeComponent.cpp`, `MMOTradeTransferSelfTest.cpp`, `Scripts/TestTradeTransfer.ps1` (staged, Codex takası; KATMA); `MMORace*`, `MMOZoneLayout*`, `Scripts/TestRace.ps1` (untracked, Codex). Bu tasarım belgesi dışında hiçbirine dokunulmadı.
- Worktree'ler: `six-map-sprint0` (`4e0f0c5`, S04.00 taban) mevcut; S04 uygulaması için **yeni, ayrı worktree** gerekir (ortak dosyada tek sahip).
- S04 **uygulaması kapalı** (pano: S03 ve kararlar bekleniyor); bu belge uygulamayı açmaz.

### 7.2 Dilimler

| # | Durum (2026-10-17) | Dilim | Dosyalar | Paylaşılan dosyaya dokunur mu | Kabul ölçütü | Test | Tahmin [Tahmin] |
|---|---|---|---|---|---|---|---|
| 0 | **KISMEN YAPILDI (kullanıcı, 2026-10-17; §6 işaretli liste; kalan: MFA, yedek, psql testleri)** | Kullanıcı: hesap, boş proje, V2 şeması, `select 1`, anon erişim negatifi (§6) | yok (kod yok) | Hayır | Kontrol listesi tamam; publishable key ile `mmo` tablolarına erişim YOK | El ile (kullanıcı) | 0.5-1 gün |
| 1 | **YAZILDI, ÇALIŞTIRILMADI** (`c8d81c1`, `bcde6a2`, `1ae80c7`) | SQL: `supabase/migrations/*_v2_character_store.sql`, `supabase/tests/*.sql` + `vectors.json`, `.gitignore` satırları | yeni dosyalar; `.gitignore` | `.gitignore` (küçük) | RPC sözleşmesi: eski revision, ölü epoch, çift `op_id` aynı/farklı yük, eşzamanlı iki `op_id` (kilit sırası), `check` kısıtları, kanonik anahtar vektörleri | Yerel PostgreSQL (kullanıcı kurar; `psql` yok [Kesin]) ya da dev projede entegrasyon betiği | 2-3 gün |
| 2 | **YAZILDI, sahte ulaşımlı test 556 kontrol geçti, izole worktree** (`e0fffdf`, `43be747`, `932b565`, `342ab17`) | UE: `IMMOHttpTransport`, `FMMOSupabaseRestClient`, `FMMOSupabaseCharacterStore` (sync yöntemler kapalı-başarısız) + `MMOSupabaseStoreSelfTest.cpp` sahte ulaşımla | yalnız yeni dosyalar | `Scripts/RunSystemTests.ps1` kaydı (şu an Codex'te kirli: sonra) | Zaman aşımı sonrası aynı `op_id` ile tek etki; belirsiz sonuç donar; anahtar log'da yok; kapalı-başarısız; geri çağrı oyun ipliğinde | Native test (headless, izole worktree + `Scripts/CheckAgentTask.ps1 -Build` kilidi), mutasyon sınaması | 4-6 gün |
| 3 | **YAPILMADI** | Arayüz eki: `EMMOStoreOutcome`, `LoadAsyncEx/SaveAsyncEx` (varsayılan = bool'a düşer) + depo seçimi (`-MMOCharacterStore`) | `MMOCharacterStore.h`, `MMOCharacterStoreSubsystem.h/.cpp` (temiz) | Evet, bu üç dosya | Yerel depo davranışı değişmez (bugünkü testler yeşil), bayraksız yol aynı. **Ek (güvenlik turu 3):** (N1) holder = sunucu kimliği + açılış GUID'i, açılışta `^[A-Za-z0-9_.:-]{1,96}$` doğrulanır, geçersizse depo açılmaz; aynı holder iki süreçte kullanılmaz; test: iki süreç (ya da iki depo örneği) aynı holder ile başlatılırsa ikincisi REDDEDİLİR (öneri: açılışta `acquire_lease` başkasının epoch'u/`ownership_lost` görürse ikinci süreç durur; DB aynı dizeyi ayıramadığından en azından açık `LogMMOStore` Error uyarısı + elle verilen sabit holder için uyarı). (N4) `RevisionConflict` alan çağıran `LoadEx` tetikler (yetkili yeniden yükleme) ve "kaydedilmemiş ilerleme kaybı" uyarısı/log'u basar; kör yeniden yazma yok | Mevcut `MMOTestStore` + yerel depo testleri; aynı-holder ret/uyarı testi; `revision_conflict` -> `LoadEx` + log testi (sahte sunucu) | 1-2 gün |
| 4 | **YAPILMADI** (periyodik `RenewLeaseEx`, çıkışta `ReleaseLeaseEx` dahil) | Çağıran geçişi (S04.00 listesi): otomatik kayıt, ilk yükleme, ad kontrolü, bölge geçişi, klan/arkadaş/posta/takas | `MMOCharacter.*`, `MMOGameMode.*`, `MMOClanComponent.*`, `MMOFriendsComponent.*`, `MMOMailComponent.*`, `MMOTradeComponent.*` | **Evet, sıcak; hepsi şu an Codex'te kirli ya da staged** | S04.00 kabul tablosunun her satırı; DB kapalıyken ilerleme yok | Fault-injection ile her senkron çağıran noktasında ret; reconnect | 1.5-3 hafta (en büyük risk) |
| 5 | **YAPILMADI** (yalnız SQL `import_character` var) | İçe aktarma aracı + `import_character` | yeni komutlet dosyaları + SQL | Hayır | İdempotent yeniden koşu, çakışmada ezmeme, ayrılmış/ASCII-dışı atlama raporu | Fixture `.sav` kopyalarıyla native test; canlı `Saved/` ayrı onay | 2-3 gün |
| 6 | **YAPILMADI** (`Scripts/TestSupabaseIntegration.ps1` yok) | Entegrasyon betiği: `Scripts/TestSupabaseIntegration.ps1` (yalnız kullanıcı anahtarları verilirse çalışır, yoksa atlar ve "çalışmadı" der) | yeni betik | Hayır | Gerçek dev projeye karşı vektörler geçer; test anahtarları `zz-test-*` ön ekli ve temizlenir | Kullanıcı anahtarıyla | 1-2 gün |
| 7 | **YAPILMADI** | Yan veri (başarım/günlük) ve posta (S16 sonrası), pazar (S12) | S16/S12 kararlarına bağlı | Evet | Ayrıca tasarlanır | | -- |

Dilim 1-2 ve 5-6 yeni dosyalarla yapılabilir ve Codex'in kirli dosyalarına dokunmaz; Dilim 3 temiz üç dosyaya dokunur (koordinatör onayı); Dilim 4 Codex ile aynı dosyalara dokunur: **Codex'in MMOCharacter/Trade işleri commit edilip yeni temel alınmadan başlanmaz**. Her dilim için mutasyon sınaması (`test-muhendisi`) ve bağımsız QA/güvenlik (`qa-kontrolcu`, `ag-guvenlik-denetcisi`) zorunlu. Bu tasarım için S04 kapısı: S03 ve S13/S16 kararları (pano).

---

## 8. Açık kararlar (kullanıcıya sorulacak, önerilerle)

**Durum: hepsi BEKLİYOR (kullanıcı onaylamadı).** Dilim 1-2 koduna gömülü geçici varsayımlar (mimari C, Auth yok, `mmo_api` yalnız `service_role`) geri alınabilir. Hesap, proje, anahtar ve Dashboard ayarları kullanıcıdadır (karar 4-5-6 hesap/faturayla da ilgili). Kısa özet ve öneriler `docs/DECISIONS_NEEDED.md` "Supabase veritabanı".

| # | Soru | Öneri | Gerekçe / etki |
|---|---|---|---|
| 1 | Mimari: A / B / C | **C** (RPC), halka açık sürümden önce **B** | Alfa için en az parça; anahtar riski kapalı alfada kabul. Reddederseniz B'ye gidilir (+3-5 hafta, plan A2 süresi). S13(a)'nın DB yarısını gevşetir. |
| 2 | S13 servis dili (Steam bilet doğrulaması, A5/A3) | **TypeScript (Supabase Edge Function)** ya da Python | Edge Function ayrı sunucu işletmeyi kaldırır, ama Supabase'e daha çok bağımlılık ve limitleri [Doğrulanmadı]; Python daha taşınabilir ama barındırma ister. Karar A5'ten önce. |
| 3 | Supabase Auth kullanılsın mı | **Hayır** | Steam bileti ile çakışır, çift kimlik tablosu. |
| 4 | Bölge | **AB (Frankfurt)** | Türkiye'ye yakınlık ve GDPR; KVKK aktarım notu için hukuki görüş al. |
| 5 | Katman / ücret | **Pro** alfa projesi, Free dev projesi | Free'de duraklatma/yedek yok olabilir **[Doğrulanmadı]**; güncel fiyat/limiti Supabase sayfasından kullanıcı doğrular. Harcama sınırı açık kalsın. |
| 6 | Yedekleme | Pro günlük yedek + kendi `export_all` | Tek kopya riski; yedeği geri yükleme provası S13 alfa kapısı ile birleşir |
| 7 | DB moduna geçiş tek yönlü mü, DB -> .sav dışa aktarma yazılsın mı | Tek yönlü + `export_all` | Geri dönüşte DB ilerlemesi kaybolmasın |
| 8 | Kimlik | Alfa K-1 açıkken yalnız bilinen oyuncularla; Steam (A5/A3) önce DB dilim 1-4'ten sonra | DB kimlik çözmez (§5.2) |
| 9 | Migration kaynağı | `supabase/migrations/`; `Migrations/V1` "kullanılmıyor" | Flyway kullanılmıyor; S15 ile birlikte karar |
| 10 | Yazma-arkada önbellek | **Reddet** | S04 "DB kapalıyken ilerleme yok" kuralı |

## 9. Bu belgenin doğrulama sınırları
Okunan kaynaklar [Kesin]; Supabase özellikleri, limitler, fiyatlar, anahtar modeli, duraklatma, yedek, CLI Docker gereksinimi, Edge Function sınırları, Auth/Steam desteği **[Doğrulanmadı]** (ağ erişimi yasaktı, hesap yok). SQL (migration + testler) hiçbir PostgreSQL'de çalıştırılmadı/sözdizimi denenmedi (makinede psql yok); `vectors.json` yalnız Python ile ayrıştırıldı. UE tarafı (Dilim 2) izole worktree'de derlendi ve sahte sunucuya karşı 556 kontrol geçti; sahte sunucu SQL'in kendisi değil `vectors.json` kontratıdır, yani gerçek PostgREST/SQL davranışıyla uyum KANITLANMADI (ilk gerçek kanıt kullanıcının dev projesinde `run_all.sql` + entegrasyon betiği). Ana kopyada derleme başkasının dosyalarıyla engelli. Bu belgenin SQL taslakları 71c548f anıdır (bkz. DURUM). S04 uygulama kapısı, S13/S16 kararları ve R1 açık kalır.
