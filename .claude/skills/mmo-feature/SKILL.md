---
name: mmo-feature
description: Bu KO/Rise tarzı MMORPG projesine yeni bir oyun özelliği eklerken izlenecek uçtan-uca akış. "Sıradaki özelliğe geç", yeni sistem/skill/item/mekanik ekleme isteğinde kullan.
---

# MMO Feature Workflow

Bu projede tek bir oyun özelliğini güvenle eklemek için standart döngü. Amaç: derlenen, commit'lenen, test edilebilir bir artış.

## İlkeler

- **Sunucu-otoriter** yaz: otorite (sunucu/host) statları/durumu değiştirir, değerler `DOREPLIFETIME`/`OnRep` ile replike olur, istemci istekleri Server RPC ile gider. Yeni sistemleri baştan replikasyona hazır kur.
- **Bileşen deseni:** yeni sistemler `UActorComponent` (Stats/Inventory/Skill/Quest/Party gibi) olarak, `AMMOCharacter`'a takılır.
- **Veri tabloları:** item/skill/quest/effect/mob tanımları `Private/.../*Database.cpp` içinde. Anonim namespace yardımcılarına **benzersiz isim** ver (ör. `BuildXTable`, `MakeX`) — yoksa unity-build çakışması commit sonrası patlar.
- Küçük, izole, çakışması derlemede yakalanan değişiklikleri tercih et.

## Adımlar

1. **İlgili dosyaları oku** (types/database/component/HUD) — deseni birebir taklit et.
2. **Kodla:** header + cpp. Gerekirse HUD (`MMOHUD`) çizim + tıklama bölgesi, controller tuş bağlama.
3. **Derle:** `ue-build` skill'indeki akışla (editör kapalı). Hataları düzelt.
4. **Test listesine ekle:** `docs/PLAYTEST_CHECKLIST.md`'ye bu özelliğin oynayarak-test maddelerini yaz.
5. **Commit:** çalışan özelliği `master`'a, açıklayıcı mesaj + `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>` ile. (`UpgradeLog.htm`'i hariç tut.)
6. Kullanıcıya kısa özet + nasıl test edeceğini söyle.

## Test için pratik konsol komutları (PIE)

`MMODamage`, `MMOHeal`, `MMOGainXP`, `MMOBuff <id>`, `MMOSpawnMob <archetype>`, `MMOUse <item>`, `MMOGiveItem <id> <n>`, `MMOStat <str|dex|int|vit>`, `MMOInvite`/`MMOPartyAccept`/`MMOPartyLeave`, `MMOPvP 0|1`.

## Otonom mod

Kullanıcı "ben yokken ilerle" derse: döngü sırasında editörü KAPALI tut (hızlı derleme), duraklarken aç. Bkz. hafıza: autonomous-dev-mode.
