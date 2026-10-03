# Bulut oturumu devir notu

Bu dosya, Claude Code **bulut oturumunun** (claude.ai/code) bu depoda neyi yapıp neyi yapmayacağını tanımlar. Yerelde (Windows PC) Unreal Engine 5.7 ve Visual Studio ile derleme yapan ayrı bir oturum çalışıyor.

## Ortam sınırları

- Bulutta **Unreal Engine, Visual Studio ve Windows araç zinciri yok**. C++ derlenemez, harita (`.umap`) ve Blueprint (`.uasset`) açılamaz.
- Bu yüzden yalnızca **metin, belge, veri ve okuyarak yapılan incelemeler** yapılır. Derleme gerektiren kod değişikliği yapılacaksa küçük, yerelde kolayca doğrulanabilir tutulur ve PR açıklamasında "DERLENMEDİ" yazılır.
- `Content/`, `Binaries/`, `Intermediate/`, `Saved/`, `Config/*.ini` dosyalarına dokunulmaz. İkili dosya (`.uasset`, `.umap`) asla düzenlenmez.
- `.agents/`, `.codex/`, `.idea/`, `AGENTS.md` bu deponun parçası değildir, dokunulmaz.

## Çalışma kuralları

- `master` dalına doğrudan commit atılmaz. Her iş `cloud/<konu>` adlı ayrı bir dalda yapılır ve PR olarak açılır. Birleştirmeyi yerel oturum yapar (derleyip doğrulayarak).
- Yanıtlar ve dokümanlar **Türkçe** yazılır. Kaynak koddaki yorumlar mevcut kodun üslubunu (ASCII Türkçe, aksansız) izler.
- Commit mesajları Türkçe ve aksansızdır; birden çok satırlıysa dosyadan verilir.
- Sunucu-otoriter tasarım korunur: stat, XP, envanter, görev ve ganimet değişiklikleri yalnız sunucuda yapılır; istemci yalnız ister ve gösterir.
- Yeni bir görev, eşya veya mob kimliği eklenecekse `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`, `Items/MMOItemDatabase.cpp`, `Characters/MMOMobArchetype.cpp` içindeki mevcut kayıt kalıbı birebir izlenir; kimlikler yinelenmez.

## Bulutta yapılabilecek işler (öncelik sırasıyla)

1. **Metin ve lore** (`docs/Lore.md`, `docs/NpcDialogue.md`): Lv51–80 bölgeleri için NPC replikleri, eşya açıklamaları, görev metinleri. Mevcut tonla uyumlu, kısa, balona sığar.
2. **Kod incelemesi (yalnız okuma)**: `git log` ile son commit'leri (`31acd21`..`HEAD`) incele; sunucu otoritesi, RPC güvenliği, replikasyon ve kayıt uyumluluğu hatalarını bul. Bulguları `docs/PLAYTEST_CHECKLIST.md` sonuna "## Bulut incelemesi — <tarih>" başlığıyla ekle. Kodu değiştirme.
3. **Denge tabloları**: `docs/PROGRESSION_PLAN.md` içindeki XP/altın/mob verilerinden seviye başına öldürme sayısı ve ekonomi tabloları üret (hesapları betikle göster).
4. **Dokümantasyon**: `CLAUDE.md` ile `docs/` arasındaki tutarsızlıkları ve eski bilgileri listele; düzeltmeyi PR'da öner.

## Yapılmayacaklar

- Derleme gerektiren büyük özellik (yeni bileşen, RPC, UI penceresi) yazılmaz; bunlar yerel oturumda yapılır.
- Harita yerleşimi (spawner koordinatları, navmesh) değiştirilmez; bunlar yalnız yerelde headless probla ölçülerek yapılır.
- Kayıt formatı (`MMOSaveGame.h`) değiştirilmez.
