# Dikey dilim planı: Lv1–30 oynanabilir deneyim

**2026-10-07 doğrulama eklemesi:** Satıcı ekonomi kuralları artık
`Scripts/TestVendorEconomy.ps1` ile UE'de 14 izole kontrol içerir: doğru fiyat/teslim,
çanta ve yığın tavanları, ölü/uzak karakter reddi ve aynı eşyanın tek satışı.
Mevcut kurallar değişmedi. Bu kanıt gerçek Lv1–30 oynanış süresi, ekonomi dengesi,
uzak istemci RPC/HUD veya kayıt kalıcılığı ölçümü değildir; PIE adımları
`PLAYTEST_CHECKLIST.md`'de açık. Aşağıdaki 2026-10-04 analizi tarihsel halini korur.

**Durum:** Salt okunur analiz, 2026-10-04. Bu belge kod, veri ve mevcut belgeler okunarak yazıldı; **hiçbir şey PIE'de oynanmadı** (`docs/PLAYTEST_CHECKLIST.md` içinde 886 maddenin yalnızca 4'ü işaretli). Kod, belge ve git'e dokunulmadı; tek çıktı bu dosyadır.

**Etiketler** (her sayı/iddia birini taşır):

| Etiket | Anlamı |
| --- | --- |
| `[KOD]` | Kaynak dosyada doğrudan okundu (dosya belirtilir). |
| `[MODEL]` | Kod verisinden betikle hesaplandı; hesap varsayımları Ek A'da. Oynanış ölçümü **değildir**. |
| `[VARSAYIM]` | PIE'de görülmedi; tahmin ya da tasarım hipotezi. |
| `[PLACEHOLDER]` | Önerilen hedef sayı; ölçülünce değişecek. |

**Veri kaynakları:** `Source/MyProject/Private/Characters/MMOMobArchetype.cpp` (48 arketip: 35 normal, 13 elit), `Quests/MMOQuestTypes.cpp` (73 görev), `Items/MMOItemDatabase.cpp` (53 eşya), `Skills/MMOSkillDatabase.cpp` (61 beceri), `Core/MMOGameMode.cpp` (spawner/NPC yerleşimi), `Components/MMOStatsComponent.cpp`, `Components/MMOSkillComponent.cpp`, `Components/MMOInventoryComponent.cpp`, `Characters/MMOMob.cpp`, `Characters/MMOCharacter.cpp`, `Player/MMOPlayerController.cpp`, `UI/MMOHUD.cpp`. Not: `docs/PROGRESSION_PLAN.md` envanter bölümündeki sayılar (36 arketip, 50 görev, 37 eşya) güncel değil; yukarıdaki sayılar koddan sayıldı.

`Scripts/BalanceReport.py` komutu (UE'nin Python'u) çalıştırıldı ve sonuçları §1.5'te kullanıldı. Görev/bölge/ekonomi/sınıf modellerini üreten betikler **depoya eklenmedi** (oturum geçici klasöründe); yöntem Ek A'da, kalıcılaştırma iş listesinin 1. maddesi.

---

## 0. Özet

1. **Bugünkü Lv1–30 yolu çok kısa:** Ana görev zinciri + 3 kilometre taşı ile toplam yaklaşık **80–85 öldürme** `[MODEL]` (görevsiz 100–155). Lv1→30 toplam eşiği 255.163 XP; görev ödülleri tek başına bunun %53'ü (ana zincir + kilometre taşları 134.250 XP), yan/altın görevler dahil %74'ü `[KOD+MODEL]`. Gerçek süre bilinmiyor `[VARSAYIM]`; görevli yolda 8–12 sn/öldürme varsayımıyla 30 dakikanın altında.
2. **"Çok hızlı giriş" bilinçli bir hedef değil, sonuç görünüyor:** Planın kendi hedefi 12–25 öldürme/seviye (`PROGRESSION_PLAN.md`), ama yalnız Lv29+ için tutuyor; Lv1–28 için hiçbir hedef sayı yok. Önerim: ilk ~10 dakika hızlı kalsın (kanca), sonra eğri belirgin yavaşlasın (§1.5).
3. **Beceri puanı karar değil:** Lv30'da 58 puan birikiyor, Lv30'a kadar açılan her şey 7–9 puana mal oluyor (fazla 49–51). Beceri hasarı sabit (`Power`), stat/ekipmanla büyümüyor.
4. **Altın faucet'i bol, sink'i zayıf:** Lv30'a kadar 80.000–150.000 altın `[MODEL]` gelirken tüm kademe setlerinin toplamı ~15.000. Yükseltme maliyeti seviyeden bağımsız (50×(artı+1)), mob altını ~×1,2/seviye büyüyor. `Q_Gold5000..50000` harcama değil birikim görevi: açıldığı anda bedava XP.
5. **Rehberlik yok:** Yeni karakter 0 altın, silahsız, iksirsiz başlıyor; tek yönlendirme sınıf paneli ve `!` NPC işareti. Kontrol tuşlarını anlatan hiçbir arayüz yok `[KOD]`.
6. **Ölüm neredeyse bedava:** En fazla o seviye eşiğinin %10'u kadar XP, seviye düşmez, altın/eşya kaybı yok, 5 sn'de başlangıçta doğuş `[KOD]` → ≈1 öldürmeden az `[MODEL]`.
7. **Savunma tehdidi büyük ölçüde siliyor:** `max(1, hasar − savunma)`; seviye uygun set + Vitality ile normal mob (ve elit) vuruşları 1 hasara yaklaşıyor `[MODEL]`. PIE'de "kill başına HP kaybı" ölçülmeden dövüş tehdidi olup olmadığı bilinmez.
8. **Parti:** Menzildeki her üye tam XP alıyor (güçlü teşvik) ama parti dışı oyuncular için **son vuruş alır** kuralı var; parti bonusu, ganimet kuralı, partiye özel içerik yok `[KOD]`.
9. Aşağıda 10 maddelik iş listesi (§3) ve PIE ölçüm planı (§4) var. İlk madde ölçüm altyapısı: ondan önce hız/ekonomi ayarı yapmak körlemesine olur.

---

## 1. Bugün Lv1–30'da oyuncunun yaşayacağı akış

### 1.1 Dünya ve hizmetler (yalnız Summer_scene; Lv1–30 Winter'a girmez) `[KOD: MMOGameMode.cpp]`

Hub (PlayerStart çevresi): **Yaşlı Doran** (görev), **Satıcı Merek**, **Şifacı Lyra**, **Depocu Rana**, **Muhafız Kael** (konuşan NPC; LLM bağlantısı yok, 6 yedek replik). Satıcı **tüm 53 eşyayı** ve tüm sarf malzemelerini seviyeye bakmadan satar (kuşanma seviye kapılı). Tek görev veren hub'dadır: kabul ve ödül teslimi için sunucu 800u yakınlık ister.

Spawner'lar PlayerStart'a göreli (F=ileri, R=sağ) ya da mutlak. Uzaklıklar koddaki ofsetlerden hesaplandı (yaklaşık, PIE'de ölçülmedi):

| Bölge (Lore) | Seviye bandı | Spawner | Hub'dan yaklaşık | Arketipler (XP) | Elit/boss (XP, yeniden doğuş) |
| --- | --- | --- | --- | --- | --- |
| A Solgun Ormanlar | 1–5 | 6 mob, r350, eşit olasılıkla 5 tür | 1050u | Goblin 30, Kurt 45, Ork 75, Haydut 95, Trol 170 (aggro 800–1150) | Örümcek Ana L5 260 (45 sn), Ork Reisi L6 420 (30 sn), Kadim Golem L7 410 (50 sn), Gölge Suikastçı L7 400 (45 sn) |
| B Karakaya Geçidi | 6–7 | 5 mob, r400 | ~1450u | Ork Savaşçısı 210, Korku Kurdu 220, Kara Büyücü 270 | — |
| C Ayazdiş Buzulu | 8–12 | 5 mob, r400 | ~1300u | Buz Hayaleti 330, Lanetli Şövalye 400, Zehir Örümceği 480, Ork Berserk 660 | Buz Lordu L13 900 (60 sn) ~2100u |
| D Alev Vadisi | 16–22 | 5 mob, r400 | ~1450u | Kor Tazısı 1300, Magma Golemi 1850, Kül Orku 2600, Cehennem Hayaleti 3700 | Magma Tiranı L24 6800 (60 sn) ~1950u |
| E Obsidyen Sırtları | 25–29 | 5 mob, r225, nav doğrulamalı | ~2650u | Avcı 4400, Muhafız 5100, Akıncı 5900 | Obsidyen Reisi L30 11000 (60 sn) ~3200u |
| F Alacakaranlık (Lv31+, süreklilik için) | 31–39 | 5 mob, r225 | ~3700u | 6800–12000 | Hükümdar L40 19000 |

Mob yeniden doğuşu: spawner her 4 sn'de eksik mobu tamamlar (`TopUpInterval=4`), yani mob arzı tek oyuncuya göre sınırsız; çok oyuncuda her bölge 5–6 mob paylaşır. Ganimet toplama ilk gelene aittir, 30 sn'de kaybolur; ekipman çantası 12 yuva `[KOD]`.

### 1.2 Seviye → ne açılıyor (oyuncu akışı tablosu)

Seviye başına sabit: +3 stat puanı, +2 beceri puanı, tam can/mana `[KOD: MMOStatsComponent.cpp HandleLevelUp]`. Başlangıç: 0 altın, kuşanılı eşya yok, 0 iksir `[KOD]`. Temel vuruş 0,6 sn, menzil 220u (tüm sınıflar) `[KOD]`.

| Lv | Mob / bölge | Görev (ödül) | Ekipman | Beceri (S=Savaşçı, O=Okçu, B=Büyücü, R=Rahip) | NPC / ekonomi |
| --- | --- | --- | --- | --- | --- |
| 1 | A; sınıf seçim paneli (varsayılan Savaşçı) | `Q_Goblins` 5 Goblin (200 XP/120 g/3 iksir), `Q_Cleanup` 10 mob, `Q_Level5`, `Q_Gold500` | Yok. Lv1 set toplam 516 g (silah 171) | Başlangıç: S Güçlü Vuruş; O Çabuk Atış; B Buz Oku; R Kutsal Işık+Ceza | Hub'da Doran/Merek/Lyra/Rana. 0 altın |
| 2 | A | `Q_Wolves` 5 Kurt (300/160/3 mana ik.) | — | S Kalkan Vuruşu; O Güçlü Atış; B Ateş Topu | — |
| 3 | A | `Q_Orcs` 3 Ork (500/300), `Q_Gold1000` | Lv3 set (Uzun Kılıç…) toplam 920 g | S Savurma+Savaş Narası; O Zehirli Ok+Koşu; B Buz Patlaması; R Yenilenme | — |
| 4 | A | `Q_Bandits` 4 Haydut (650/400) | — | S Kan Emme+Demir Deri; R Kutsal Patlama | — |
| 5 | A | `Q_Boss` Ork Reisi (1500/1000/Büyük Kılıç Lv6), `Q_SpiderQueen` (900/600/Plaka), `Q_Trolls`; `Q_Level5` tamam | Lv5 set 1324 g | S Ölümcül Darbe; O Ok Yağmuru; R Büyük Şifa | `Q_Level5`: 400 XP, 2 büyük mana ik. |
| 6–7 | B (6–7) | `Q_Warlocks`/`Q_OrcWarriors`/`Q_DireWolves` (yan, ekipman ödüllü), `Q_StoneGolem` (L6), `Q_ShadowAssassin` (L7) | Ejder seti Lv7 (1860 g) | B Meteor (6); S Kan Fırtınası, O Delici Atış, R İlahi Şifa (7) | — |
| 8–9 | C (8–12) | `Q_FrostWraiths` (1800/1200/3 büyük can ik.), `Q_CursedKnights` (Mithril Bot, Lv10 gerekir) | — | B Zincir Yıldırım (8) | — |
| 10 | C | `Q_VenomSpiders` (Mithril Kask) | Mithril seti Lv10 (2448 g) | S Parçalayıcı Darbe; O Kartal Gözü; B Alev Fırtınası; R Kutsal Alev | — |
| 12–13 | C | `Q_OrcBerserkers` (Mithril Zırh), `Q_FrostLich` elit (4500/3000/Mithril Kılıç) | — | — | Ana zincirin 1. yarısı biter |
| **13–15** | Yeni normal mob yok (C'de kalınır) | — | — | **Yeni beceri yok (10→16)** | — |
| 16 | D (16–22) | `Q_EmberHounds` (+4 büyük can ik.), `Q_Level20` açılır, `Q_Gold5000` açılır | Adamantin seti Lv16 (3456 g) | S Yaşam Çalı; O Gölge Adımı; B Buz Zinciri; R Kutsal Yağmur | — |
| 18–22 | D | `Q_MagmaGolems` (Bot), `Q_AshOrcs` (Kask), `Q_InfernalWraiths` (Zırh) | Tier'in 3 parçası görevlerle gelir | — | `Q_Level20`: 9000 XP/5500 g |
| 24 | D elit | `Q_MagmaTyrant` (12000/8000/Adamantin Kılıç), `Q_Level30` açılır | — | Dört sınıfta Lv24 becerisi (S Savaş Fırtınası, O Ejder Oku, B Gök Gürlemesi, R Meleklerin Öfkesi) | — |
| 25–29 | E (25–29) | `Q_ObsidianStalkers/Sentinels/Raiders` (Bot/Kask/Zırh) | Obsidyen seti Lv25 (4704 g) | **25–32 arası yeni beceri yok** | XP eğrisi 25'ten sonra doğrusal-ikinci dereceye geçer |
| 28 | E | `Q_Gold15000`, `Q_Gold30000` açılır | — | — | — |
| 30 | E elit | `Q_ObsidianWarlord` (17000/18000/Obsidyen Kılıç), `Q_Level30` (16000 XP) | Lv33 Alacakaranlık seti ancak 33'te kuşanılır | — | Lv31+ F bölgesi |

Notlar `[KOD]`: Ekipman ödülü görevi bittikten sonra verildiği için çoğu ödül bir sonraki seviyeye hazırlıktır (`Q_Boss`/Büyük Kılıç Lv6, `Q_CursedKnights`/Mithril Bot Lv10). Oyuncuya ekipman ödülünde "bu parça Lv X'te kuşanılır" uyarısı PROGRESSION_PLAN'da öneri olarak duruyor; arayüzde doğrulanmadı.

### 1.3 Beceri açılış ritmi (sınıf başına) `[KOD: MMOSkillDatabase.cpp]`

Bir becerinin açılması için hem seviye hem ustalık rütbesi gerekir; her rütbe 1 puan (tablodaki `SkillPointCost` kullanılmıyor, `RequestInvestMastery` her seferinde 1 harcıyor). Rütbe ≤ karakter seviyesi.

| Sınıf | Lv≤30 beceri | Yeni becerinin açıldığı seviyeler (Lv1–30) | Boşluklar (≥5 seviye) | Lv30'a kadar gereken puan | Birikim (2×29) |
| --- | ---: | --- | --- | ---: | ---: |
| Savaşçı | 11 | 1, 2, 3, 4, 5, 7, 10, 16, 24 | 10→16, 16→24, 24→33 | 9 | 58 |
| Okçu | 9 | 1, 2, 3, 5, 7, 10, 16, 24 | aynı | 8 | 58 |
| Büyücü | 8 | 1, 2, 3, 6, 8, 10, 16, 24 | aynı | 8 | 58 |
| Rahip | 9 | 1, 3, 4, 5, 7, 10, 16, 24 | aynı | 7 | 58 |

Hotbar 10 yuva `[KOD]`. Yani ilk 10 seviyede 6–9 beceri (Savaşçı 9, Okçu 7, Büyücü 6, Rahip 7) akıyor; Lv11–30'da her sınıfa yalnız 2 beceri (Lv16 ve Lv24) geliyor. Lv80'e kadar toplam gereken rütbe Savaşçı 16, Okçu 13, Büyücü 13, Rahip 14; birikim 158 `[KOD]`.

### 1.4 Ekipman kademeleri ve fiyatlar `[KOD+MODEL]`

Alış = `round((saldırı+savunma)×12) + gerekenSeviye×25 + 50`; satış = alışın %40'ı ×(1+0,3×artı)×nadirlik çarpanı (Sıradan 1, Nadir 1,25 … Efsanevi 3; ortalama 1,19). Yükseltme +1 = %20 temel değer; maliyet `50×(artı+1)`; başarı %90/80/70/55/45/35/25/20; yanma +3'ten itibaren %15/30/50/70/100.

| Kademe (gereken Lv) | Silah/zırh/kask/bot alış | Set toplam | Görevle gelen |
| --- | --- | ---: | --- |
| 1 | 171/135/111/99 | 516 | — |
| 3 | 293/245/197/185 | 920 | — |
| 5 (+6) | 439/355/259/271 (Büyük Kılıç 560) | 1324 | `Q_Boss` Büyük Kılıç, `Q_SpiderQueen` Plaka |
| 7 | 681/477/333/369 | 1860 | `Q_StoneGolem`/`Q_ShadowAssassin` Ejder Kılıç/Kask |
| 10 | 876/636/444/492 | 2448 | Mithril: Lv9–13 zinciri (Bot, Kask, Zırh, Kılıç) |
| 16 | 1194/906/642/714 | 3456 | Adamantin: Lv18–24 zinciri |
| 25 | 1587/1227/903/987 | 4704 | Obsidyen: Lv25–30 zinciri |
| 33 | 1979/1535/1151/1247 | 5912 | Lv33–40 zinciri |

Seviye-uygun parça ve gereken seviye arasında kayma: 10→16, 16→25, 25→33 arası satıştan yeni kademe yok; boşluğu zincir ödülleri doldurur.

### 1.5 `BalanceReport.py` sonuçları ve "çok hızlı giriş" değerlendirmesi

**Çıktı (Lv1–30, "en iyi normal mob [L−8, L+3] penceresinde", görev XP'si hariç)** `[KOD+MODEL]`:

| Lv | 1 | 2–9 | 10–20 | 21–22 | 23–24 | 25–26 | 27–28 | 29–30 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| öldürme/seviye | 2 | 1 | 2 | 3 | 4 | 5 | 6 | 8 |

Raporun toplamı Lv1→30 için ≈76 öldürme. Uyarı sayısı 0; çünkü eşik yalnızca ">25" ve "<8 hızlı" notu bilgi amaçlı.

**Raporun üç sınırı:** (a) Her seviyede penceredeki **en çok XP veren** mobu seçer; gerçek spawner ise banttan **eşit olasılıkla** seçer (Lv1–5'te ortalama mob XP'si 83, rapor Lv1 için 95 kullanıyor) ve Lv13–15, 23–24 gibi bölgesiz seviyeleri farklı bölgedeymiş gibi hesaplar → iyimser. (b) **Görev XP'sini içermez** → kötümser; görevler (yalnız ana zincir + 3 kilometre taşı) Lv1→30 eşiğinin yaklaşık %53'ü. (c) Parti (üyelerin hepsi tam XP) yok. Net etki hesapla belirlenemez; ölçüm şart.

**Bölge sınırlı, görevli/görevsiz simülasyon** `[MODEL]` (Ek A): bant başına ortalama öldürme/seviye ve XP'nin görev payı

| Senaryo | 1–5 | 6–10 | 11–15 | 16–20 | 21–25 | 26–30 | Toplam (Lv31'e) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Görevsiz, bölge ortalama seviyesi ≤ L (ihtiyatlı) | 2,0 | 2,4 | 3,4 | 6,8 | 6,4 | 9,8 | 154 |
| Görevsiz, ortalama ≤ L+3 (iddialı) | 1,2 | 1,4 | 3,4 | 2,0 | 4,4 | 8,2 | 103 |
| **Ana zincir + 3 kilometre taşı** | 2,0 | 1,6 | 2,0 | 2,4 | 4,2 | 4,4 | **83** |
| (görevli senaryoda XP'nin görev payı) | %57 | %60 | %61 | %76 | %28 | %42 | — |

Görevli yolun sırası: `Q_Goblins` (5 öldürme) Lv1'de alınır, tesliminde Lv3; `Q_Wolves` ile ≈Lv7; `Q_Orcs`, `Q_Bandits`, `Q_Boss` ile ≈Lv10; yaklaşık **18 öldürmeyle Lv12'de `Q_FrostWraiths` açılır**. `Q_Boss` ödülü (1500 XP) o seviyenin eşiğinin 6 katıdır; ilk dört zincir görevi eşiğin 2–3,3 katıdır `[KOD+MODEL]`. Aynı ölçek Lv16–24'te 0,7–1,9 katına, Lv25+ 0,3–0,45'e iner.

**Bilinçli mi?** Kanıtlar:
- `PROGRESSION_PLAN.md` "bilinçli hızlı giriş" diyor (Denge raporu paragrafı), fakat Lv1–28 için bir hedef sayı vermiyor; plan hedefi (12–25 öldürme) yalnız Lv29+ için tutuyor.
- `Q_Boss` yorumu (`MMOQuestTypes.cpp` ~satır 56–59): zincir, oyuncuyu Lv15–16'da bırakıp **içeriği bitirdiği** için uzatılmış; yani tasarım kaygısı içerik tükenmesiydi, hız değil.
- `PLAYTEST_CHECKLIST.md` MEDIUM-3 (24 Eylül, satır ~1921): "bug değil, tersine çok hızlı"; çözüm kararı "kullanıcı/mmo-developer"a bırakılmış. Kısmen ele alınmış (ana alan `MaxMobs=6`, seviye farkı XP kuralı), hız kararı açık; kutular işaretsiz.
- XP eğrisi `100×1,25^(L−1)` (Lv≤25) ile mob XP tablosu el ile ve bağımsız ayarlanmış; görev ödülleri Lv1–16'da eşiğin 2–6 katı.

**Sonuç (yorum):** Hız bir tasarım kararı olarak alınmamış, ayarsız bir yan etki; "bilinçli" etiketi sonradan eklenmiş görünüyor. Ancak ilk dakikaların hızlı olması **istenen bir şey** (kanca). Ayrıştırma önerisi:

**İlk saat için öneri:** 
1. **İlk ~10 dakika hızlı kalsın:** Lv1→5–6 civarı, her görev tesliminde belirgin ödül (seviye + ekipman parçası). Hedef kanca: ilk elit/boss (`Q_Boss`) ve ilk kuşanılan set bu pencerede.
2. **Sonra yavaşla:** `[PLACEHOLDER]` hedef öldürme/seviye (görevli koşuda): 1–5: 3–5, 6–10: 6–9, 11–15: 9–13, 16–20: 11–15, 21–25: 13–18, 26–30: 14–20 → toplam yaklaşık 330 öldürme (bugün ≈83, yani ~4× yavaş). `[VARSAYIM]` 20–30 sn/öldürme (yürüyüş, mana dinlenmesi, ganimet, teslim gezileri dahil) ile bu: Lv5 ≈ 7–10 dk, Lv10 ≈ 19–29 dk, Lv15 ≈ 38–56 dk, Lv20 ≈ 1–1,5 sa, Lv30 ≈ 2–3 sa. Sürenin kendisi ölçülecek (§4).
3. **Hız kolu tek yerde olsun:** Lv≤28 için `XPReward`'ı 35 arketip satırında tek tek oynamak yerine tek bir `GetXPMultiplier(mobLevel)` benzeri veri kolu + `Q_*` ödüllerinin düzenlenmesi (görev XP'si Lv1–16'da eşiğin ≤1–1,5 katına). Lv29+ bugünkü 8–14 öldürme dokunulmadan kalsın (plan hedefinin 12–25'ine yaklaştırma ayrı karar).
4. Karar proje sahibinde: **A** bugünkü hız (Lv30 ≈ 20–45 dk `[VARSAYIM]`), **B** önerilen (2–3 sa), **C** KO tarzı çok saatli (8+ saat): C için Lv1–30'da yalnız 6 bölge ve ~330 öldürme **yetmez**, yeni bölge/yan görev içeriği gerekir. Bu belge B'yi varsayar.

---

## 2. Boşluklar

### 2.1 Sınıf başına beceri açılış ritmi

- **Dağılım:** ilk 10 seviyede 6–9 beceri, Lv11–30'da yalnız 2 (Lv16 ve Lv24). Lv10→16, 16→24, 24→33 arası "yeni beceri yok" `[KOD]`. 30 seviyenin yalnızca 8–9'unda yeni beceri var.
- **Beceri puanı karar üretmiyor:** Lv30'da birikim 58, gereken 7–9 (Rahip 7, Okçu/Büyücü 8, Savaşçı 9); Lv80'de 158 birikim, 13–16 rütbe gerekir `[KOD]`. Rütbe başına maliyet 1 olduğu için puan seçimi yok, yalnız "hepsini öğren".
- **Beceri hasarı sabit:** `ExecuteSkill` `Def.Power`'ı doğrudan `ApplyDamage`'e verir; `AttackPower`/stat/seviye ölçeklemesi yok `[KOD: MMOSkillComponent.cpp ~satır 941, 989]`. Temel vuruş ise `AttackPower` ile büyür. Her ikisi de mob savunmasıyla `max(1, ham − savunma)` azalır. Sonuç `[MODEL]`: silahlı (Lv5 silahı +22) ve statlara yatırım yapan Savaşçı'da Lv5'te temel vuruş brüt ≈54–72 (Str/Vit dağılımına göre), Trol savunmasından (8) sonra 46–64 net; Güçlü Vuruş 40 brüt, 32 net. Büyücü'de Buz Oku (40) Lv29 mob savunmasında (35) 5 net hasara düşer. Erken beceriler seviye ilerledikçe işlevsizleşir.
- **Menzilli sınıfların temel vuruşu yakın dövüş** (`MeleeRange=220`): Okçu/Büyücü mana bitince yakın dövüşe mecbur `[KOD]` (QA LOW-7).
- **Rahip** Lv1'de hem Kutsal Işık (şifa) hem Ceza (hasar) ile başlar (`GrantStarterSkill`, QA düzeltmesi) `[KOD]`.
- **Sınıf başına öldürme hızı** `[MODEL]` (Ek A; stat dağılımı ve "hazır en yüksek net hasarlı beceriyi kullan" varsayımları belirleyici, sonuçlar hipotezdir). Ortalama öldürme süresi (Lv5 / 10 / 20 / 29): Savaşçı 1,8 / 2,7 / 5,8 / 5,8 sn, Okçu 2,3 / 3,1 / 9,6 / 7,2, Büyücü 1,8 / 2,0 / 4,3 / 4,9, Rahip 2,1 / 3,2 / 7,4 / 8,3. Öldürme başına mana tüketimi Savaşçı/Okçu'da havuza yakın ya da üstünde (Lv10: 111–116 mana, havuz 110–120; Lv29: 232–246, havuz 214–224); Büyücü/Rahip'te havuzun yarısı civarı. Yani Savaşçı/Okçu becerileri her öldürmede peş peşe kullanamaz.

### 2.2 Altın kaynakları ve sink'leri (ilk 30 seviye)

**Kaynaklar** `[KOD]`: mob altını (Goblin 3–12 → Obsidyen Akıncısı 950–1900, ortalama ~×1,2/seviye), görev altını (Lv1 120 → Lv30 18.000), **ekipman satışı** (%25 düşme × seviyeye +3'e kadar havuz; nadirlik çarpanı), iksir düşmesi (%35/öldürme; satış yolu bulunmadı).

**Sink'ler** `[KOD]`: satıcıdan ekipman; yükseltme `50×(artı+1)` (+yanma); iksir 20/40; Şifacı Lyra tam yenileme **sabit 100 altın**; başka sink yok (onarım, ışınlanma, depo/klan ücreti bulunmadı).

| Gözlem | Değer | Etiket |
| --- | --- | --- |
| Lv5'e kadar toplam altın (satın alma yok) | 190–250 (Lv1 silahı 171) | MODEL |
| Aynı koşuda Lv10 / Lv20 / Lv30 | 1.000–1.600 / 9.200–15.400 / 77.000–149.000 | MODEL |
| Tüm kademe setlerini sırayla almak (Lv1…Lv25) | ≈15.000 | MODEL |
| Mob altını Lv5 → Lv16 → Lv29 | 48 → 340 → 1425 ortalama | KOD |
| Beklenen yükseltme maliyeti (yanan eşyayı yeniden alma dahil): +3 / +4 / +5 / +6 / +7 / +8 | 395 / 830–1020 / 1750–2590 / 4400–7500 / 15.000–28.000 / 80.000–150.000 | MODEL |
| Yanmadan +8'e tek seferde ulaşma olasılığı | %2 (+5: %65, +6: %34, +7: %11) | MODEL |
| Beklenen ekipman satış değeri/öldürme ÷ mob altını | Lv1–4: %126–475; Lv10: %32; Lv20: %9; Lv29: %5 | MODEL |
| Şifacı (100 g) ÷ ortalama mob altını | Lv5: 2 öldürme; Lv16: 0,3; Lv29: 0,07 | MODEL |
| Can iksiri 75 HP, büyük 150 HP; Savaşçı MaxHP Lv30 538–1234 | %14–6 / %28–12 | MODEL |

Çıkarımlar:
- **Lv1–8 altın kıt**, ekipman satışı (kaçınılmaz drop) asıl gelir; ilk silahı satın almak ancak ~`Q_Wolves` sonrası mümkün (120 g görev + ~40 g mob = 160 < 171). Başlangıç paketi (iş 2) bu yüzden önerilir.
- **Lv16+ altın aşırı bol**; yükseltme maliyeti seviyeden bağımsız olduğu için +1…+5 anlamsız ucuzluğa iniyor; yalnızca +7/+8 kumarı gerçek sink (ve beklenen maliyet aşırı yüksek: +8 için tek denemede %2). Orta kısım (+5…+6) boş.
- **`Q_Gold5000/15000/30000/50000`** harcama değil birikim koşulu; mevcut cüzdan yeterince doluysa açıldığı an tamamlanır ve 6.000/14.000/28.000/45.000 XP verir `[KOD+MODEL]`. Ekonominin "hedefi" olmaktan çıkmış.
- **İksirler ölçeklenmiyor:** tavan 150 HP/120 mana; F tuşu tek `HealthPotion` (75), bekleme süresi yok `[KOD: MMOCharacter.cpp UseHealthPotion]`; doğal yenilenme 6 HP/sn (dışında) `[KOD]` → şifa/iksir Lv20'den sonra neredeyse gereksiz.
- **Şifacı** 100 g sabit, seviye ölçeksiz; ayrıca doğal 5 sn'lik yeniden doğuş zaten ücretsiz.

### 2.3 İlk saat rehberliği (öğretici yok)

- Var olanlar `[KOD: MMOHUD.cpp]`: sınıf seçim paneli (kayıt yoksa; "varsayılan: Savaşçı"), NPC üstünde `! İsim [tıkla]`, minimap (4000u), görev takibi (en çok 8 satır), "Yakında: <mob> LvN" tehdit yazısı, `/yardim` (yalnız sohbet komutları).
- **Eksikler:** kontrol tuşlarını (tıkla-git, WASD, Z hedef, R saldırı, F iksir, I çanta, K beceri, C stat) gösteren bir yer yok; ilk seviye atlamada stat/beceri puanı harcama yönlendirmesi yok (Lv10'a kadar 27 stat + 18 beceri puanı birikebilir, hızlı seviye atlama bunu büyütür `[MODEL]`); ekipman satın alma/kuşanma, yükseltme ve yanma riski anlatılmıyor; görev hedefinin yönü işaretlenmiyor; silah kuşanma yönlendirmesi yok.
- Başlangıç koşulu: 0 altın + silah yok + iksir yok; ilk hasar kaynağı çıplak `AttackPower` (Savaşçı 32). Ekipman düşmesi %25/öldürme, yani ortalama 4. öldürmede bir eşya `[MODEL]`.
- Başlangıç alanı güvenliği ayrı bir QA başlığı (elitlerin gezinme+aggro ile hub'a erişmesi, `PLAYTEST_CHECKLIST.md` ~satır 1849–1920); kutular işaretsiz, çözüm ve PIE doğrulama durumu belirsiz. İlk-10-dk rehberi bu doğrulama olmadan anlamlı değil.
- Muhafız Kael LLM satırları yerine 6 sabit replik gösteriyor (LLM kredisi sorunu) `[KOD+hafıza]`.

### 2.4 Ölüm cezası

`[KOD]` `ApplyDeathPenalty`: `kayıp = min(mevcutXP, ExperienceToNextLevel×0,10)`; seviye düşmez; altın/eşya kaybı yok; 5 sn sonra PlayerStart'ta doğar (`RespawnDelay`), 4 sn koruma; otomatik saldırı hedefi temizlenmiyor (QA bulgusu, `PLAYTEST_CHECKLIST.md` ~satır 2065; çözüm durumu doğrulanmadı). Seviye atlamadan hemen sonra (XP≈0) ölümün XP maliyeti sıfıra yakındır (`min(mevcutXP, …)`).

`[MODEL]` ölüm maliyeti = 0,1×eşik ÷ bölge ortalama XP'si ≈ Lv10: 0,16, Lv20: 0,29, Lv29: 0,96, Lv30 (F bölgesi): 0,65 öldürme. Yürüme: bölgeler hub'a 1,3–3,7 bin birim; yürüme hızı doğrulanmadı `[VARSAYIM]` (BP varsayılanı ≈500u/sn varsayımıyla 3–7 sn). Toplam ≈ 20–40 sn. **Ölüm riski caydırıcı değil.** Savunma duvarı (§2.6) ile birleşince Lv5–30 normal mobla ölüm neredeyse hiç olmayabilir `[MODEL]`; olursa maliyeti düşük kalır.

### 2.5 Parti motivasyonu

`[KOD: MMOMob.cpp HandleDeath]` Öldüren ve `GetPartyInRange(…, 3000u)` içindeki parti üyelerinin **her biri tam XP ve görev kredisi** alır (seviye farkı kuralı üye başına ayrı uygulanır); en çok 8 üye. Dolayısıyla parti kişi başı XP/saat artırır. Eksikler:
- **Son vuruş alır:** parti dışı oyuncu `LastDamageInstigator` değilse XP/görev kredisi yok. Tek mobluk elit spawner'larında (`Q_Boss`, Buz Lordu, Magma Tiranı, Obsidyen Reisi) rekabet var.
- Parti bonusu yok (üye sayısına göre XP çarpanı yok), parti dışı için XP cezası yok.
- Ganimet ilk gelene ait; parti ganimet kuralı, atama, zar yok.
- Parti gerektiren içerik yok: elitler aynı bandın normal mobundan yalnızca ~0,9–1,6 kat can taşır (Örümcek Ana L5 270, Ork Reisi L6 346, Kadim Golem L7 454, Gölge Suikastçı L7 262, Buz Lordu 558, Magma Tiranı 994, Obsidyen Reisi 1210; karşılaştırma normal Lv12 434, Lv22 746, Lv29 958 `[KOD]`). Modelde (Ek A, tam set) Savaşçı elitleri tek başına 2,4–6 sn'de keser ve MaxHP'nin ≤%1'ini kaybeder `[MODEL]` → boss hissi/parti gereği yok.
- Rahip'in `HealTarget` becerileri parti üyesi hedefleyebiliyor (yalnız bu sinerji var).

### 2.6 Diğer boşluklar

- **Hasar–savunma duvarı** `[KOD+MODEL]`: `max(1, ham − savunma)`; set savunması (örn. Lv25 set 46+19+26=91) ve Vitality/2, aynı seviye mob hasarını (Lv25–29 mob 88–100) 1–10'a indirir. Modelde (tam seviye-uygun set, Savaşçı %50 Str/%50 Vit) kill başına HP kaybı Lv5–29'da %0–1 (Lv1 Trol %3; setin üstündeki Lv31 Hayalet %6; %100 Str dağılımında Lv31 Hayalet %19). Gerçek oyuncunun donanımı kademe geride ya da stat dağılımı farklı olabilir; ölçülmeli.
- **Bölgesiz seviyeler:** Lv13–15 ve Lv23–24'te yeni normal mob yok; Lv13–15'te yeni beceri/ekipman da yok → "boş" bant `[KOD]`.
- **Tek hub, uzak bölgeler:** her 5 öldürmelik görev için hub'a gidiş-dönüş; Lv25+ bölgeleri 2,6–3,7 bin birim `[KOD-hesap]`. Görev tesliminin uzaktan yapılabilirliği yok.
- **Görev seviye kapısı vs oyuncu seviyesi:** Görevli yolda oyuncu çoğu zaman görev kapısının 5–10 seviye önünde (örn. `Q_Wolves` Lv2 gerekirken Lv3'te, `Q_Orcs` Lv3 gerekirken Lv7'de alınır) `[MODEL]` → görev seviyesi yönlendirme sinyali taşımıyor.
- **İki mob başlangıç alanı karışımı:** A bölgesi 5 türü eşit olasılıkla üretir; Troll (Lv5, 170 XP) Lv1 oyuncuya %20 olasılıkla çıkar; hedef mob için kuyruğu boşaltmak gerekiyor (QA MEDIUM-3, `MaxMobs=6` ile hafifletilmiş).
- **Doküman kayması:** `docs/PROGRESSION_PLAN.md` envanter sayıları (36/50/37) eski; kodda 48/73/53.

---

## 3. Öncelikli iş listesi (10 madde, uygulayıcıya devredilebilir)

KO/Rise tarzı beklenen döngü (yalnız genel referans, doğrulanmadı): hub → kısa görev → bölge avı → parti → boss; ölümün bir bedeli; ekonomide anlamlı harcama; beceri/stat seçimleri. Maddeler bağımlılık sırasıyla. Boyut: S ≈ ≤1 gün, M ≈ 2–4 gün, L ≈ >1 hafta (tahmin `[VARSAYIM]`). Tüm sayısal hedefler `[PLACEHOLDER]`.

### 1. PIE telemetrisi + BalanceReport v2 — **M**
- **Durum (2026-10-05): büyük kısmı yapıldı (master, PIE ölçümü yok).** `[MMOMETRIC]` `levelup`/`kill`/`death`/`gold` satırları ve `Scripts/PlaytestMetrics.py` özeti var; BalanceReport v2'nin işini `Scripts/ProgressionModel.py` (görevli, bölge sınırlı) görüyor. **Açık:** `quest_accept`/`quest_claim`/`first_*` olayları, (d) maddesinin karşılaştırması. Koşu adımları: `docs/PLAYTEST_RUNBOOK.md`.
- **Oyuncu deneyimi:** Doğrudan yok; sonraki her ayarın ölçülebilir olması.
- **Dosya kapsamı:** `MMOStatsComponent.cpp` (seviye atlama, ölüm cezası), `MMOMob.cpp` (öldürme, altın), `MMOInventoryComponent.cpp` (altın giriş/çıkış kaynağı: ganimet, satış, satın alma, yükseltme, iksir), `MMOQuestComponent.cpp` (kabul/teslim), `MMOPlayerController.cpp` (Şifacı), yeni `Scripts/PlaytestMetrics.py`, `Scripts/BalanceReport.py` v2 (bölge sınırlı + görev XP'si + senaryo seçeneği).
- **Kabul ölçütü:** (a) 20 dk'lık PIE koşusunda sunucu logunda `[MMOMETRIC]` satırları: `levelup`, `kill`, `death`, `gold`, `quest_accept`, `quest_claim`, `first_*`; (b) tek komutla seviye başına süre/öldürme/ölüm/altın giriş-çıkış/XP kaynağı tablosu; (c) yalnız sunucu, oyun davranışı değişmez (derleme + `SmokeTest.ps1` geçer); (d) BalanceReport v2 çıktısı §1.5 tablosunu ±1 öldürmeyle yeniden üretir.

### 2. Başlangıç paketi ve doğuş doğrulaması — **S**
- **Durum (2026-10-05): yapıldı (master, `AMMOCharacter::GrantStarterPack`; PIE'de doğrulanmadı).** Kısa Kılıç + Deri Zırh kuşanılı, 3 Can İksiri; yalnız kayıt dosyası olmayan yeni karakter dalında. Doğuşta 3 dk güvenlik testi `PLAYTEST_CHECKLIST.md`'de bekliyor.
- **Oyuncu deneyimi:** İlk dakikada silahsız/iksirsiz kalmamak; ilk görev ödülünün (120 g) silaha yetmemesi çözülür.
- **Dosya kapsamı:** `MMOCharacter.cpp` (yeni karakter dalı, ~satır 515–535), `MMOInventoryComponent.cpp` (verme + kuşanma). Sınıf ayrımı yok (eşya tablosunda sınıf kısıtı yok).
- **Kabul ölçütü:** Yeni karakter ilk 10 sn'de Kısa Kılıç + Deri Zırh kuşanılı, 3 Can İksiri; kayıt-yükle-kayıt sonrası tekrar verilmez (envanter sayısı sabit); var olan kayıtlar etkilenmez; başlangıç altını 0 kalır; yeni karakterle doğuş noktasında 3 dk beklemede hiçbir mob saldırmaz (§2.3'teki QA açığının PIE doğrulaması).

### 3. İlk 10 dakika etkileşimli rehberi — **M**
- **Oyuncu deneyimi:** Rehbersiz yeni oyuncunun ilk görevi teslim edip ekipman kuşanmasını sağlar; "bir sonraki adım" sürekli görünür.
- **Dosya kapsamı:** `MMOHUD.cpp` (ipucu şeridi), `MMOCharacter.h/.cpp` (yalnız sahibe replike adım), `Save/MMOSaveGame.h` + kayıt/yükleme (`TutorialStepMask`; mevcut kayıtlarda "tamamlandı"), `MMOPlayerController.cpp` (olay tetikleri). Metinler `docs/NpcDialogue.md` üslubunda.
- **Adımlar:** sınıf seç → hareket → Doran'a git (`!`) → `Q_Goblins` kabul → hedef seç/saldır → ganimet → teslim → ilk seviyede stat penceresi (C) → beceri kitabı (K) → satıcı/kuşanma (I) → iksir (F).
- **Kabul ölçütü:** Geliştirici olmayan 3 oyuncunun 3'ü rehbersiz müdahale olmadan 10 dk içinde ilk görevi teslim edip bir ekipman kuşanır (hedef tasarım ölçütü: ilk testlerde >%90); altı tuşun hepsi bir kez tanıtılır; ipucu pencere/ölüm ekranını örtmez; istemci (listen server 2. pencere) dahil çalışır; eski kayıtta görünmez.

### 4. XP hız yeniden ayarı (Lv1–28) — **M** (1. maddeye bağlı)
- **Oyuncu deneyimi:** İlk 10 dk hızlı, sonra her bölgede 20–30 dk kalma hissi; görev ödülü ile öldürme dengeli.
- **Dosya kapsamı:** `MMOMobArchetype.cpp` (tek `GetXPMultiplier(mobLevel)` veri kolu), `MMOQuestTypes.cpp` (Lv1–28 zincir/kilometre taşı/`Q_Gold*` `RewardXP`), XP eğrisine (`ComputeExperienceForLevel`) **dokunma**. Lv29+ değerleri korunur.
- **Kabul ölçütü:** BalanceReport v2 (görevli) bant hedefleri ±%20 içinde (1–5: 3–5, 6–10: 6–9, 11–15: 9–13, 16–20: 11–15, 21–25: 13–18, 26–30: 14–20 öldürme/seviye); hiçbir seviye <2 ya da >25 öldürme değil; PIE solo Savaşçı (konsol komutu yok) Lv5 ≥ 6 dk, Lv10 ≥ 18 dk, Lv20 ≥ 55 dk, Lv30 ≥ 110 dk (alt sınırlar; gerçek süre §4 metrik M1 ile).

### 5. Beceri hasarı ölçeklemesi ve sınıf öldürme hızı dengesi — **M** (1'e bağlı)
- **Oyuncu deneyimi:** Erken beceriler Lv20'de hâlâ anlamlı; sınıflar benzer hızda öğütür.
- **Dosya kapsamı:** `MMOSkillComponent.cpp` (`ExecuteSkill`: Power'a stat bağı; ör. `Power + k×AttackPower`, Büyücü/Rahip için INT-türevli), `MMOSkillDatabase.cpp` (Power/Mana/CD ince ayarı), `MMOStatsComponent.cpp` (mana yenilenmesi gerekirse). Seçenek: temel vuruşun menzilli sınıflara uyarlanması (tasarım kararı).
- **Kabul ölçütü:** M12 (beceri kullanım oranı): Lv10–20'de en az 3 farklı hasar becerisi toplam atışların ≥%10'unu oluşturur; M6 (TTK): dört sınıfın aynı seviyedeki ortalama öldürme süresi Savaşçı'nın ±%35'inde; "mana bitti" olayı başına dinlenme süresi ≤ 15 sn (Lv≤30).

### 6. Beceri açılış ritmi ve puan ekonomisi — **L** (5'ten sonra)
- **Oyuncu deneyimi:** Her 3 seviyede bir yeni/yükseltilmiş beceri; puan harcamak gerçek bir tercih.
- **Dosya kapsamı:** `MMOSkillDatabase.cpp` (Lv11–15, Lv17–23 ve Lv25–32 boşluklarına sınıf başına ≥1 yeni beceri ya da mevcut becerinin rütbe yükseltmesi), `MMOSkillComponent.cpp`/`MMOStatsComponent.cpp` (`SkillPointCost` ya da rütbe maliyet eğrisi veya `SkillPointsPerLevel`), `MMOHUD.cpp` (beceri kitabı), `Scripts/ValidateData.py` (boşluk denetimi).
- **Kabul ölçütü:** Lv1–30'da hiçbir 3 seviyelik pencere beceri değişimsiz değil (veri betiği doğrular); harcanmamış beceri puanı Lv30'da ≤ 6 (bugün 49–51) ve oyuncu her ağaçta tüm rütbeleri alamaz (en az iki ağaçta tercih); 2 oyunculu PIE'de rütbe yatırımı/öğrenme/hotbar sunucu-istemci tutarlı.

### 7. Hasar–savunma formülü ve tehdit dengesi — **M** (1'e bağlı; yüksek regresyon riski)
- **Oyuncu deneyimi:** Bölge ve elit karşılaşmaları tehdit hissettirir; uygun donanımla bile bazen iksir/kaçış gerekir; donanımsız bölge girişi caydırılır.
- **Dosya kapsamı:** `MMOStatsComponent.cpp::ApplyDamage` (ör. oransal azalma), `MMOMobArchetype.cpp` (hasar), `MMOItemDatabase.cpp` (savunma değerleri); yanında mob vuruş hızı/etki sıklığı.
- **Kabul ölçütü:** M6: seviye-uygun donanımla kill başına alınan hasar medyanı MaxHP'nin %5–30'u (bölge bandı başına) ve aynı mobda en eski kademe donanımla %40+; hedef ölüm sıklığı: 1 ölüm / 30–80 öldürme (M4); Lv1–5 doğuş güvenli (madde 2 ile aynı test); tüm sınıflarda ölçülür.

### 8. Ölüm cezası ve geri dönüş maliyeti — **S/M** (4 ve 7'den sonra ayarlanır)
- **Oyuncu deneyimi:** Ölümün bir bedeli var ama sinir bozucu değil; seviye atlama hemen sonrası ölüm bedava değil.
- **Dosya kapsamı:** `MMOStatsComponent.cpp::ApplyDeathPenalty` (taban ceza; ör. `max(%10×eşik, X×mobXP)`), `MMOCharacter.cpp::RespawnAtStart` (son kullanılan çıkış noktası/hub), otomatik saldırı hedefini ölümde temizleme (QA BULGU 1).
- **Kabul ölçütü:** M4: ölüm maliyeti (XP kaybı + geri dönüş süresi) ≈ 1–3 öldürme eşdeğeri (bugün <1 + ~10–20 sn); XP=0 anında ölüm artık ≥ tabanı öder; doğuşta 4 sn korumadan sonra hedef peşine kendiliğinden yürüme olmaz; ceza hiçbir koşulda seviye düşürmez/negatif XP üretmez.

### 9. Ekonomi: sink, ölçek, `Q_Gold*` yeniden tasarımı — **M** (1 ve 4'ten sonra)
- **Oyuncu deneyimi:** Her seviyede harcayacak anlamlı şey var; kıtlık yalnız Lv1–8'de; iksir/şifacı seviyeyle ölçeklenir.
- **Dosya kapsamı:** `MMOItemDatabase.cpp` (`GetUpgradeCost` tier ölçekli, `GetBuyPrice`), `MMOConsumableDatabase.cpp` (kademe iksirler veya %'lik iyileşme), `MMOHealerNPC` + `ServerRequestHeal` (`HealCost` ölçeği), `MMOQuestTypes.cpp` (`Q_Gold*`: birikim yerine "harca/yükselt" ya da XP düşürme), `MMOCharacter.cpp` (F tuşu iksir seçimi/bekleme).
- **Kabul ölçütü:** M5: her bandda altın giriş÷çıkış 0,5–1,5; Lv30'da cüzdan, bir sonraki tier set fiyatının ≤3 katı; ilk set + `+3` yükseltme Lv10 öncesi karşılanabilir; `Q_Gold*` açıldığı an tamamlanmaz; +5 yükseltme maliyeti Lv20–30'da ≥8–10 öldürme eşdeğeri; F tuşu kendi kademe iksirini kullanır.

### 10. Parti: katkı kredisi, bonus ve ganimet kuralı — **M/L** (1 ve 4'ten sonra)
- **Oyuncu deneyimi:** Tek moblu elitlerde rekabet yerine paylaşma; partilemek ödüllü.
- **Dosya kapsamı:** `MMOMob.cpp` (HandleDeath: son vuruş yerine hasar katkısı penceresi, ör. son 20 sn içinde ≥%X katkı), `MMOPartyComponent.cpp` (üye sayısına göre XP çarpanı), `MMOLootPickup.cpp` (ilk 10 sn sahiplik), elit arketipleri (parti ölçekli can opsiyonu).
- **Kabul ölçütü:** 2 pencereli PIE: B oyuncusu hasar verip A öldürürse B de kredi alır (XP + görev ilerlemesi), çift kredi/exploit yok (sunucu otoriter); parti kişi başı XP/saat solo'nun ≥1,2 katı (M10); elit ganimeti ilk 10 sn yalnız katkı verenlerce alınır; Lv1 oyuncunun Lv40+ partide kazancı mevcut tavanı (eşiğin %25'i, `ScaleXPForLevelGap`) aşmaz.

---

## 4. PIE'de ölçülecek metrikler ve toplama yöntemi

### 4.1 Metrikler

| ID | Metrik | Birim | Neden | "Bozuk" eşiği `[PLACEHOLDER]` |
| --- | --- | --- | --- | --- |
| M1 | Seviye başına süre (gerçek + aktif; 30 sn girdisiz süre hariç) | sn | Hız hedefi (§1.5) | Bant ortancası hedefin ±%30'u dışında |
| M2 | Seviye başına öldürme sayısı; hedef mob / diğer ayrımı | adet | BalanceReport doğrulaması | <2 ya da >25 |
| M3 | XP kaynağı payı: öldürme / görev / kilometre taşı / altın görevi | % | Görev baskınlığı | Görev payı >%60 |
| M4 | Ölüm sayısı, ölüm anındaki arketip, XP kaybı, doğuştan ilk öldürmeye kadar süre | adet, XP, sn | Risk ve ceza | <1 ölüm/80 öldürme (tehdit yok) veya >1/10 (aşırı) |
| M5 | Altın giriş/çıkış (kaynağa göre), cüzdan, yükseltme denemesi sonucu | altın | Faucet/sink | Giriş÷çıkış >2 veya <0,3 |
| M6 | Öldürme süresi, alınan hasar (MaxHP %), mana tüketimi, iksir kullanımı, sınıfa göre | sn, % | Dövüş hissi | Hasar <%5 (tehdit yok) veya >%40 |
| M7 | Zaman dağılımı: savaş / yürüyüş / arayüz / boşta (5 sn örnek) | % | Yürüme/gezi yükü | Yürüme >%40 |
| M8 | Görev döngüsü: kabul→tamam süresi, teslim gecikmesi, hub gidiş-dönüş mesafesi | sn, u | Hub sürtünmesi | Gidiş-dönüş >%50 görev süresi |
| M9 | "İlk"ler: ilk hareket/saldırı/öldürme/görev kabul/teslim/kuşanma/seviye/beceri/ölüm zamanı; rehbersiz tamamlama oranı | sn, % | Onboarding | <%90 rehbersiz tamamlama (10 dk) |
| M10 | Solo vs parti: kişi başı XP/sa, aynı spawner'da oyuncu sayısı, kredisiz vuruş sayısı | XP/sa, adet | Parti motivasyonu | Parti <1× solo |
| M11 | Çanta doluluğu olayları, süresi dolan ganimet | adet | Ganimet akışı | Dolu çanta > 3 dk |
| M12 | Beceri başına atış sayısı (seviye bandı başına), "mana yetersiz" olayı | adet | Beceri anlamı | Bir becerinin kullanımı bandda <%2 (ölü beceri) |

### 4.2 Toplama yöntemi

1. **Sunucu log satırları (iş 1):** tek satır `key=value`, `Saved/Logs/MyProject.log` içine, örn. `[MMOMETRIC] t=312.4 ev=kill who=Host cls=Warrior lv=9 mob=CursedKnight mlv=9 xp=400 gold=118 target=0`. Olaylar: `login`, `levelup`, `kill`, `death`, `gold` (kaynak: loot/quest/sell/buy/upgrade/potion/heal/trade), `quest_accept`, `quest_claim`, `skill`, `idle_sample` (5 sn'de bir durum: combat/move/ui/idle), `first_<olay>`. Yalnız otorite çalışır; istemci log'a bağımlı olmaz. Mevcut `[MMODBG]` etiketiyle çakışmaz.
2. **Ayrıştırıcı (iş 1):** `Scripts/PlaytestMetrics.py` logu CSV + Markdown tabloya çevirir (seviye başına satır; bu belgenin tablolarıyla aynı kolonlar). `Scripts/RunAllChecks.ps1`'e eklenebilir.
3. **Koşu protokolleri:**
   - **P1 solo Savaşçı:** yeni kayıt, 60 dk, konsol komutu yok (`MMOGainXP`/`MMOGiveItem` kullanılmaz); amaç M1–M9.
   - **P2 sınıf karşılaştırması:** Okçu, Büyücü, Rahip için 30'ar dk (Lv1–~10); amaç M6, M12, M1.
   - **P3 iki oyunculu parti:** 30 dk listen server + istemci; amaç M10, ganimet/kredi.
   - **P4 ilk kez oynayan kişi:** 15 dk, geliştirici olmayan, sesli düşünme + ekran kaydı; amaç M9. Başarı ölçütü tasarım kuralı: ilk testlerde rehbersiz tamamlama >%90.
   - **P5 ekonomi probları:** `MMOGainXP` ile Lv16 ve Lv25'e atla (kullanım loga `cheat=1` ile işaretlenir, analizde ayrı), 20 dk öldür+sat+yükselt; amaç M5.
4. **Gürültü kontrolü:** yürüme hızını ve `RespawnDelay` gibi sabitleri koşunun başında loga yaz (BP varsayılanı doğrulanmadı); aynı düzen için her koşu yeni kayıt slotu (`MMOSetName`); ekran çözünürlüğü/FPS, mob aggro'sundaki gecikmeyi etkileyebilir, kaydedilir.
5. **Kayıt karşılaştırma:** her koşu sonrası BalanceReport v2 çıktısı ile ölçülen M1/M2 yan yana konur; fark >%30 olan bant bir sonraki ayar turunun girdisidir. Hiçbir `[PLACEHOLDER]` değer, ölçüm olmadan kilitlenmez.

---

## 5. Karar bekleyen konular (proje sahibi)

1. **Hız seçeneği:** A (bugünkü, Lv30 ≈ 20–45 dk), B (öneri, 2–3 sa), C (KO tarzı, 8+ sa; ek içerik gerektirir). §1.5.
2. **Gold sink felsefesi:** yükseltme kumarı mı, harcama görevleri mi, ek sink'ler mi (onarım vb.)?
3. **Görev teslimi:** Lv16+ bölgeler için ikinci görev veren/uzaktan teslim istiyor muyuz (Lv25+ bölgeler hub'dan 2,6–3,7 bin birim)?
4. **Ölüm cezası şiddeti:** KO'daki gibi gerçek XP kaybı mı (seviye düşebilir mi), yoksa bugünkü hafif model mi?
5. **Sınıf kimliği:** menzilli sınıfların temel vuruşu menzilli mi olacak, yoksa yalnız beceri ile mi dövüşecek?
6. **Bölgesiz seviyeler:** Lv13–15 ve Lv23–24 için yeni bölge/mob mu, yoksa mevcut bandın genişletilmesi mi?

---

## Ek A: Hesap yöntemi ve varsayımlar

**Formüller** `[KOD]`: XP eşiği `L≤25: round(100×1,25^(L−1))`, `L>25: round(100×1,25^24 + 4725(L−25) + 591(L−25)²)`; MaxHP `50 + 8×Vit + 12×L`; MaxMana `25 + 5×Int + 5,5×L`; AttackPower `1,5×Str + 0,5×Dex + silah×(1+0,2×artı)`; Savunma `0,5×Vit + zırh toplamı`; hasar `max(1, ham − savunma)`; mob can/savunma aynı formülle (Str/Vit arketipten, Dex=10, Int=6); mob vuruşu 1,5 sn; oyuncu temel vuruşu 0,6 sn; mana yenilenmesi `7 + 0,25×L` /sn, savaşta ×0,25; seviye farkı XP: oyuncu mobdan >5 seviye üstteyse her ek seviye −%10 (taban %10), mob >10 üstteyse −%10 (taban %20). Sınıf başlangıç statları: Savaşçı 18/10/6/16, Okçu 10/20/8/12, Büyücü 6/8/22/10, Rahip 8/8/18/14 (Str/Dex/Int/Vit).

**Bölge/görev simülasyonu:** bölge seçimi: bandın ortalama mob seviyesi ≤ L+k (k=0 ihtiyatlı, k=3 iddialı); her öldürmede bölge arketiplerinden sırayla (eşit olasılık ortalaması), XP `ScaleXPForLevelGap` ile; görev modu: önce `ReachLevel` kilometre taşları, sonra seviye kapısı ve ön koşulu uygun ilk ana zincir görevi (hedef mobu `adet` kez öldür, ödül XP/altın), uygun görev yoksa bölgede öldür. Dahil değil: yan/elit görevler, `HaveGold` görevleri, parti, yürüme/ölüm. Altın: mob ortalaması (min+max)/2 + görev altını.

**Sınıf TTK modeli:** stat puanı dağılımı Savaşçı %50 Str/%50 Vit, Okçu %50 Dex/%50 Vit, Büyücü %70 Int/%30 Vit, Rahip %60 Int/%40 Vit; her seviyede kuşanılabilir en yüksek tier set +0; becerilerden net hasarı en yüksek hazır olanı sırayla, hazır beceri yoksa temel vuruş; beceri başına 0,5 sn animasyon; mana savaşta ×0,25 yenilenir; tek hedef, menzil/ıskalama yok. Bu varsayımlar belirleyicidir; sonuçlar hipotezdir.

**Yükseltme beklenen maliyeti:** durum zinciri (artı k → k+1 başarı p_k; başarısızlıkta yanma b_k → eşyayı yeniden al + sıfırdan; yanmazsa aynı artıda tekrar); lineer denklem çözümü. Yeniden alma fiyatı: Lv1 silah 171, Lv16 1194, Lv25 1587.

**Ekipman satış EV'si:** mob başına `0,25 × ort(satış, havuz: gerekenSeviye ≤ mobSeviyesi+3) × 1,1865 × (1+0,3×0,15×1)`; havuz tüm yuvalarda düz dağılım (`MMOMob.cpp` HandleDeath ekipman bölümü).

**Çalıştırma:** `"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/BalanceReport.py` (mevcut) çalıştırıldı; v2 iş 1'de. Bu belgedeki ek modeller geçici betiklerle üretildi ve depoda yoktur; iş 1 bunları kalıcılaştırır.

**Bilinmeyenler (PIE'de ölçülmedi):** oyuncu yürüme hızı, öldürme başına gerçek süre, mob yoğunluğunun hissi, gerçek ölüm sıklığı, UI okunabilirliği, çok oyunculu davranış, elit karşılaşmaların süresi, Winter/Summer geçişi (Lv1–30'u kapsamaz).

---

## Güncelleme (2026-10-04, bulut)

Madde 10'un **hasar payı XP'si** `AMMOMob`'a eklendi (derlenmedi): XP, mobda bırakılan hasarın toplam hasara oranıyla dağıtılır (%1 hasar = %1 XP, eşik yok); parti tek birim sayılır ve partinin payı **üye sayısına bölünür** (8 kişilik partide 1/8). Görev kredisi paydan bağımsız tam verilir. Eski "parti üyesi tam XP alır" davranışı kalktı. Parti bonusu, ganimet sahipliği (ilk 10 sn) ve parti ölçekli elit can açık. Kalan işler ve model bulguları: `BALANCE_MODEL.md`.
