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

Kontrol: seviye ve rütbe şartıyla **otomatik açılıyor mu**, mana/bekleme hissi (mana ×0,25 savaşta yenilenir: havuzun yaklaşık %40'ını
sürdürülebilir kılıyor `[Hesap]`), alan becerileri ikinci hedefte tek hedef becerilerini geçiyor mu.

## Bilinen açıklar (test etmeden önce bil)

- **Zehir DoT'u etkisiz** (savunmaya karşı fiilen 1 hasar): `DOT_SCALING_DESIGN.md`. Zehir veren mob/beceri hasarı boştur, bug değil.
- Lv73–80 için **yeni ekipman yok**; satıcı seviye kapısı kararı bekliyor (`DECISIONS_NEEDED.md` madde 3, CONTENT_PLAN G6).
- Kül mobları lore'da "yakar", kodda yavaşlatır/vurur; `Burn` efekti yok.
- Elit/boss karşılaşmaları hâlâ "daha çok can + Öfke"; fazlı boss yok.
