# Lv46–80 içerik: ilk oyun içi test listesi

**Tarih:** 2026-10-09. **Durum:** bu içeriğin hiçbiri oyunda denenmedi; smoke testleri yalnız doğumu ve hatasız yüklemeyi gösterir.
Sayıların hepsi `[PLACEHOLDER]`. Plan: `CONTENT_PLAN_LV46_80.md`, beceriler: `SKILL_PLAN_LV31_45.md`.

## Hazırlık (host penceresi, `Winter_scene`)

Editör kapalıyken derle, sonra oyunu aç. Konsol: `` ` ``. Debug komutları yalnız sunucu yöneticisinde ve Shipping dışı derlemede çalışır.

- Seviye atla: `MMOGainXP 5000000` (gerekirse tekrarla). Sınıf: `MMOClass Warrior|Archer|Mage|Priest`.
- **Işınlan:** `MMOTeleport X Y Z` (Z'yi tabloda verilen değerden **+100** ver, yoksa zemine düşersin).
- Mob çağır: `MMOSpawnMob WinterWolf` (önüne bir tane).

## Yeni cepler (Winter) ve ışınlanma koordinatları

| Cep | Bant | Mobu | `MMOTeleport` | Beklenen |
|---|---|---|---|---|
| P46 | 46–47 | Kış Yağmacısı, **Don Kurdu** | `-5000 -3000 150` | 4–6 mob, hızlı kurt yavaşlatır |
| P48 | 48–49 | Kış Hayaleti, **Buz Devi** | `5000 0 220` | Buz Devi yavaş, can <%40'ta öfkelenir |
| P53 | 53 | **Buzbağlı Yeminli** | `6000 -4000 340` | dengeli vurucu, <%45 öfke |
| P69 | 69 | **Kül Yürüyeni** | `-4000 -6000 150` | ağır vurucu, <%40 öfke |
| P74 | 74 | **Uçurum Yankısı** | `-5500 2500 -20` | hızlı, yavaşlatır |
| P78 | 78 | **Naibin Elçisi** | `-2000 2500 250` | yavaş, ağır; Nöbetçi cebine 1581 u |

Eski cepler değişti: Kış cebi `(-3000,-1500)` artık yalnız Lv42–44. Summer'daki Yarık cepleri (**Lv58–60** ve **Lv62–64**) genişledi
(Summer'da `MMOTeleport 800 -4800 150` ve `-4800 4800 -20`).

## Her cep için kontrol

1. Cepte **4+ mob** var mı (yarısından fazlası canlı)? Hiç doğmayan/aralıklı doğan cep varsa konumu ve ekran görüntüsünü not et.
2. Mobun adı doğru mu, hasarı/öfke eşiği hissi makul mü? `MMOMETRIC` satırları `Saved/Logs/MyProject.log` içinde.
3. Mob bu cepte **hub tarafına kadar kovalıyor mu**? (aggro 600, gezinme 450; 1800 u'dan yakın cepler komşusunu çekmemeli.)
4. Ölünce ışınlanıp tekrar giderken yol uzunluğu: hub'dan 4.4–7.3 bin birim; ileri kamp yok (CONTENT_PLAN G10).

## Yeni görevler (Winter hub'daki görevli NPC'den; `MMOGainXP` ile kapıyı aç)

| Görev | Kapı | Önkoşul | Hedef |
|---|---|---|---|
| `Q_WinterHowls` | 47 | `Q_WinterReavers` | 8 Don Kurdu |
| `Q_WinterColossus` | 49 | `Q_WinterWraiths` | 6 Buz Devi |
| `Q_FrostboundSworn` | 52 | `Q_FrostboundScouts` | 6 Buzbağlı Yeminli |
| `Q_FrostboundWatch` | 55 | `Q_FrostboundSentinels` | 8 Buzbağlı Nöbetçi |
| `Q_FrostboundOath` | 56 | `Q_FrostboundSworn` | 8 Buzbağlı Yeminli |
| `Q_AshMarchers` | 69 | `Q_AshBinders` | 8 Kül Yürüyeni |
| `Q_AbyssEchoes` | 72 | `Q_AbyssStriders` | 8 Uçurum Yankısı |
| `Q_AbyssEnvoys` | 76 | `Q_AbyssEchoes` | 8 Naibin Elçisi |
| `Q_RiftWhispers` (Summer) | 59 | `Q_RiftSeekers` | 8 Yarık Fısıldayanı |
| `Q_RiftSentries` (Summer) | 62 | `Q_RiftWatch` | 8 Yarık Bekçisi |
| `Q_RiftWalkers` (Summer) | 64 | `Q_RiftDepths` | 8 Yarık Yürüyeni |

Kontrol: görev listede çıkıyor mu, alış/teslim repliği mantıklı mı, ödül (5× Büyük Can/Mana İksiri) çantaya giriyor mu, **ana zinciri
(`Q_WinterSovereign`, `Q_FrostboundWarden`, `Q_RiftHerald`, `Q_AshTyrant`, `Q_AbyssRegent`) kilitlemiyor** mu.

## Yeni beceriler (Lv52 / Lv64 / Lv75, her sınıfta üç)

`MMOGainXP` ile seviyeye çık, beceri puanını ilgili ağaca yatır (rütbe 5–7), hotbar'a koy:
Savaşçı `War_BladeStorm`, `War_Obliterate`, `War_WorldBreaker` · Okçu `Arc_ShatterShot`, `Arc_Multishot`, `Arc_SunderingArrow` ·
Büyücü `Mag_Combustion`, `Mag_GlacialSpike`, `Mag_CosmicBlast` · Rahip `Pri_SmitingLight`, `Pri_HealingSurge`, `Pri_Sanctify`.

Kontrol: Savaşçı/Rahip hasar zincirleri seviye ve rütbeyle **otomatik açılır**; Okçu ve Büyücü Special0 (rütbe 7) ile Rahip Pri_HealingSurge (Special1, rütbe 5) bir üst rütbe ister: ilgili ağaca **1 puan yatır** (QA DÜŞÜK-3). Ayrıca, mana/bekleme hissi (mana ×0,25 savaşta yenilenir: havuzun yaklaşık %40'ını
sürdürülebilir kılıyor `[Hesap]`), alan becerileri ikinci hedefte tek hedef becerilerini geçiyor mu.

## Bilinen açıklar (test etmeden önce bil)

- **Zehir DoT'u:** oyuncunun mob üzerindeki Zehir'i artık savunmayı atlıyor ve saldırıyla ölçekleniyor (tick = 8 + saldırı×0,10; `DOT_SCALING_DESIGN.md`);
  **denenmedi**, Zehir becerilerini (`Arc_PoisonArrow`, `Arc_DeathMark`, `Arc_ThornVolley`, `Arc_SpikeTrap`, `Arc_PoisonCloud`) mobda dene: hasar sayıları
  tick başına yaklaşık 8 + saldırı/10 olmalı. **Mob → oyuncu zehri hâlâ etkisiz** (bilerek; savunmaya karşı 1 hasar), bug değil.
- **Yeni (2026-10-09, denenmedi):** Lv78 **Taht seti** (`ThroneBlade/Plate/Helm/Boots`; Naib elitinden düşer, `Q_AbyssRegent` ödülü `ThroneBlade`);
  satıcıdan alım **alıcı seviyesi + 3 ≥ eşya seviyesi** ister ("Bu eşya için çok erken"); Taht seti **satıcıda listelenmez** ("Bu eşya satıcıda satılmaz"); yükseltme maliyeti eşya seviyesiyle büyür (Lv78'de +1 ≈ 3.100 altın);
  `Q_Gold5000+` görevleri teslimde altını **düşürür** (bağış). Kontrol: satıcı reddi mesajı, yükseltme maliyeti HUD'da doğru mu, bağış sonrası bakiye.
- Kül mobları lore'da "yakar", kodda yavaşlatır/vurur; `Burn` efekti yok.
- Elit/boss karşılaşmaları hâlâ "daha çok can + Öfke"; fazlı boss yok.

## Ara seviye yan görevler (Lv21–45; 2026-10-10, denenmedi)

Kapı boşluklarını dolduran yedi yan görev; her biri komşu ana görevin hedefini 8 kez daha avlatır, ana zinciri kilitlemez, ödül 5 iksir.

| Görev | Kapı | Önkoşul | Hedef × adet |
|---|---:|---|---|
| `Q_AshOrcPack` | 21 | `Q_AshOrcs` | 8 Kül Orku |
| `Q_WraithNest` | 23 | `Q_InfernalWraiths` | 8 Cehennem Hayaleti |
| `Q_ObsidianPatrol` | 26 | `Q_ObsidianStalkers` | 8 Obsidyen Avcısı |
| `Q_DuskPack` | 32 | `Q_DuskProwlers` | 8 Alacakaranlık Avcısı |
| `Q_DuskHaunt` | 41 | `Q_DuskOverlord` | 8 Alacakaranlık Hayaleti |
| `Q_WinterScouts` | 43 | `Q_WinterStalkers` | 8 Kış İzcisi |
| `Q_WinterBulwark` | 45 | `Q_WinterGuardians` | 8 Kış Muhafızı |

Kontrol: görev, önkoşul bitince listede çıkıyor mu; XP ~%6 eşik (placeholder) seviye atlama hızını bozuyor mu.
