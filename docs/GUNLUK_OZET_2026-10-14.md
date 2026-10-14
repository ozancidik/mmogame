# Günlük özet — 2026-10-14 (Claude)

Önceki özet: `GUNLUK_OZET_2026-10-11.md` (commit `22bc1d0`). Bu belge `22bc1d0` sonrasını toplar: 11 Ekim akşamı (tur 5),
12-13 Ekim bulut işi (PR #3, PR #4) ve 14 Ekim yerel doğrulama + tur 6. **Oyun içinde (PIE) ve gerçek uzak istemciyle hiçbir şey
denenmedi.** Test sayıları ve headless sonuçlar bu özeti yazan belge denetçisi tarafından `%TEMP%` loglarından okundu
(native testleri ve RunAllChecks'i kendisi yeniden koşmadı; yalnız aşağıdaki Python denetimlerini koştu).

## Yapılanlar

| Alan | Ne | Kanıt |
|---|---|---|
| Güvenlik tur 5 | İzinsiz karakterli ad **kırpılmaz**: `PreLogin` temizlemenin değiştirdiği `?Name=`'i reddeder (`Ömer`, `Ali!`), `ChangeName` `Gezgin######` verir; ayrılmış adlar (`MMONameRules::IsReservedName`: kanonik `default`, `player` + yalnız rakam) `PreLogin`/`ServerSetName`'de ret, postada teslim edilemez; pawn'ı olmayan bağlı oyuncu (`GameState->PlayerArray`) `ServerSetName` ve başarım/günlük eski yuva silmede çevrimiçi sayılır | `f70e5ad` (2026-10-11 21:24) |
| Tasarım (bulut) | `docs/SIX_MAP_PLAN.md`: 6 harita + ırk + 10 aşamalı yol haritası; kullanıcı kararları 2026-10-13 (harita başına sunucu, PvP Lv55, pazar şart); revizyon 2 Steam (S12 SteamID, S13, S17, S18 Steam-first, S19, A5) | `5a0154c`, `3c26b58`, `5f9c266`; PR #4 birleşmesi `7b6c4a2` |
| Altyapı (bulut) | Sprint 1: `MyProjectServer.Target.cs`, `Migrations/V1__initial_schema.sql`, `UDatabaseMigrator`, `FPacketValidator`, `Scripts/build.ps1`, `Scripts/k6_load_test.js` | `9855256`; PR #3 birleşmesi `266197e` |
| Posta iade muhasebesi | Aynı `9855256` commit'i, mesajı söylemese de Codex'in O69 işini taşıyor: `ApplyGoldInternal` + `AddGoldFromWithAppliedDelta` (uygulanan, kırpılmış tutar), posta yalnız bu denemenin gerçek kesintisini iade eder, hediye korunur, tam ödenmeyen posta gönderilmez; `MMOMailRefundSelfTest` (176) + `Scripts/TestMailRefund.ps1` (izole worktree ister) | `9855256`; görev kaydı `docs/tasks/MAIL_REFUND_ACCOUNTING_2026-10-11.md` |
| Testler tur 5 | Başarım 96 -> **102**, günlük 81 -> **87**, posta 130 -> **144** (+11 tur 5, +3 iade hesabı), ad kilidi 68 -> **82**; 9 mutasyonun 9'u yakalandı | `76ceed9` (mesaj: RunAllChecks 70/70) |
| SIX_MAP §11 adım 1 | Editör hedefi Sprint 1 dosyaları dahil kurulu motorla **0 hata** | UBT `Log.txt` 2026-10-14 03:32 `Result: Succeeded`; `1f4a557` mesajı "Build Succeeded" |
| SIX_MAP §11 adım 2 | `RunAllChecks.ps1 -SkipBuild` çıkış 0, **70/70 adım GEÇTİ**; Summer duman 0 hata / 56 mob; Winter duman 0 hata / 81 mob; `NetSmokeTest` port 17850 (yalnız Summer): bağlantı 1, `Welcomed` 1, hata 0/0; native 102/87/144/82 | `%TEMP%\runall_orch_20261014.log`, `mmo_smoke_*_scene.log`, `mmo_net_*_20261014_034420_*.log`, `mmo_{achievements,daily,mail,namelock}_*.log` (`PASS checks=N`) |
| Güvenlik tur 6 | Sprint 1'in yerel QA/güvenlik denetimi: `build.ps1 -Clean` artık `Saved/`'i silmez (yalnız `Binaries/` + `Intermediate/`, DDC ayrı `-CleanDdc`, editör/UBT çalışırken durur, `-ProjectPath` doğrulaması); Target `bUseLoggingInShipping` + `BuildEnvironment = Unique`; migrator/validator sahte başarısı kaldırıldı (`false` + "not implemented"; `_rollback.sql` elenir); V1: `character_warehouse.updated_at` + `CHECK (>= 0)`; posta iadesi ödenemezse `[MMOMETRIC] ev=gold_lost` | `1f4a557` |
| Belge (bu tur) | CLAUDE.md (test sayıları, ad kuralı, Sprint 1 satırı, `TestMailRefund.ps1`), `SIX_MAP_PLAN.md` (§1.5 tur 6, §11 adım 1-3 durumu, D1, R11, S15, S16), `PLAYTEST_CHECKLIST.md` (adım 2 sonucu + takas A-1 maddesi; K1H `?Name=Ömer` beklentisi), `DECISIONS_NEEDED.md` (tur 5 durumu, S15, takas altını, k6), `SECURITY_AUDIT.md` (tur 5/6 tablosu), `MAIL.md`, `ACHIEVEMENTS.md`, `DAILY_BOUNTIES.md`, `ACCOUNT_IDENTITY_DESIGN.md` (satır kaymaları) | bu commit |
| Denetim | `ContentMap.py --check` "koddan üretilenle aynı"; `DocCounts.py` 59 mob / 97 görev / 57 ekipman / 93 beceri, sapma 0; `PocketSpacing.py` 18 cep, 0 hata, 2 bilinen uyarı (`MMOGameMode.cpp:1038`<->`:1050` 1733 u, `:1039`<->`:1074` 1581 u) | 2026-10-14, belge denetçisi |

**Düzeltilen sayı:** adım 2 için "140 adım GEÇTİ" denmişti; logda 70 adım başlığı ve 70 `-> GEÇTİ` var, 140 = adım satırları + özet
satırlarındaki "GEÇTİ" toplamı. Doğru sayı **70/70** (`76ceed9` mesajıyla da aynı).

## Kim neye dokundu

- **Claude (yerel, trailer'lı):** `f70e5ad` (tur 5 kod: `MMOGameMode.*`, `MMOCharacter.cpp`, `MMONameRules.h`, başarım/günlük
  bileşenleri, `MMOMailStore.*`), `76ceed9` (tur 5 testleri), `1f4a557` (tur 6: `build.ps1`, `MyProjectServer.Target.cs`,
  `DatabaseMigrator.*`, `PacketValidator.*`, `V1__initial_schema.sql`, `MMOMailComponent.cpp` metrik satırı), bu belge commit'i.
- **Claude (bulut, PR #4):** `5a0154c`, `3c26b58`, `5f9c266` (yalnız `docs/SIX_MAP_PLAN.md`).
- **Bulut PR #3 (`9855256`, trailer yok, yazar `ozancidik`):** Sprint 1 dosyaları + Codex O69 posta iade muhasebesi
  (`MMOInventoryComponent.*`, `MMOMailComponent.cpp`, `MMOMailRefundSelfTest.cpp`, `MMOMailSelfTest.cpp`, `Scripts/TestMailRefund.ps1`,
  `docs/tasks/MAIL_REFUND_ACCOUNTING_2026-10-11.md`, `PLAYTEST_CHECKLIST.md` O69 bölümü).
- **Codex:** `22bc1d0` sonrası ayrı commit yok. Commit'siz `.agents/` klasörüne dokunulmadı.

## Dürüst riskler

- **PIE yok:** SIX_MAP §11 adım 3 (iki pencereli PIE, Summer<->Winter) yapılmadı, kullanıcıyı bekliyor. Tur 5 ad kuralları, ayrılmış
  adlar, iade muhasebesi ve takas yalnız native/headless kanıtlı; sohbet, HUD, gerçek RPC ve iki istemci kanıtı yok.
- **NetSmoke yalnız Summer:** Aşama 0 "her iki harita" diyordu; Winter'da yalnız tek süreçli duman koşuldu.
- **Takas altını hep-ya-hiç değil (AÇIK):** `ApplyGoldInternal` sahip/bileşen yok ediliyorsa sessizce 0 döner
  (`MMOInventoryComponent.cpp:410`); takasın dört altın çağrısı dönüşe bakmaz (`MMOTradeComponent.cpp:880-883`). Pratik pencere dar
  [Muhtemel], tetiklenebildiği gösterilmedi. `MMOTradeComponent.*` Codex'in dosyası. PIE maddesi: `PLAYTEST_CHECKLIST.md` A-1.
- **Server hedefi derlenmedi** (kaynak motor gerekir); Target.cs değişiklikleri yalnız metin. `V1__initial_schema.sql` psql ile
  uygulanmadı. `build.ps1` düzeltmesi bu turda betik çalıştırılarak denenmedi (kod okundu). `MyProjectClient.Target.cs` yok:
  `build.ps1 -Target Client/All` başarısız olur.
- **`FPacketValidator::ValidateRPC` artık her RPC'yi reddeder** (kapalı-başarısız); bugün kimse çağırmıyor, gerçek uygulama olmadan
  bağlanırsa oyunu kilitler.
- **`TestMailRefund.ps1` (176)** ana checkout'ta koşmaz (izole worktree ister); bu turda koşulmadı. Son PASS kaydı Codex görev
  kaydındaki izole koşudur.
- **PL1-PL3 plan riskleri:** tur 6 denetim raporunda geçtiği bildirildi; içerikleri hiçbir belgede/commit'te yok, doğrulanamadı
  (`SECURITY_AUDIT.md` S1-8).
- Kimlik = oyuncu adı (K-1) hâlâ temel açık; Linux harf varyantı (F4) açık.

## Açık kararlar (`DECISIONS_NEEDED.md`, `SIX_MAP_PLAN.md` §8)

- **S15:** `UDatabaseMigrator` / `FPacketValidator` silinsin mi, tutulsun mu (öneri: sil; migration'lar psql/Flyway ile).
- **Takas altını:** düzeltme yolu (a) aktarımdan önce iki envanterin uygunluğunu doğrula, (b) `AppliedDelta` ile tutarsızlıkta geri al,
  (c) ayrı dahili çağrı; Codex'e devir ya da sıra.
- `k6_load_test.js` A3'te giriş servisi yük testi olarak yeniden yazılacak (karar gerekmez).
- Türkçe harfli ad gerekli mi (ICU anahtarı + göç); kayıtlı karakter için ad değiştirme özelliği gerekli mi.
- SIX_MAP açıkları: servis dili (S13 kalan), S16 posta biçimi, S3 ırk ara yolu, S19 Steamworks ücreti zamanı.
- Önceki günlerden: ölüm cezası, şifacı katsayısı, yan görev XP kalıbı, mob -> oyuncu zehri, Burn/Lore.
