# Lv1–30 Metin Boşluk Analizi (görev + NPC replikleri)

**UYGULANDI: 2026-10-05, commit 138c642** (Codex'in "Ticaret ve parti davet kapma korumalarini tamamla" commit'ine karisti: ayni anda commit alindi; MMOQuestTypes.cpp ve MMOGameMode.cpp degisiklikleri o commit'in icinde). Yalniz metin dizeleri; ID/sayi/odul/mantik degismedi. Derleme + `RunAllChecks.ps1 -SkipBuild` GECTI; oyunda gorulmedi.
- Uygulanan (MMOQuestTypes.cpp, ASCII): G1 Q_Goblins; G2 Q_Trolls (kismen, asagida); G3 Q_Warlocks/OrcWarriors/DireWolves; G5 Q_SpiderQueen/StoneGolem; G6 Q_FrostWraiths; G7 Q_ObsidianWarlord; G8 Q_ObsidianStalkers/Sentinels/Raiders; G10 Q_Gold500/1000/5000 + Q_Level5. Hub: Kael FallbackLines'a G1 satiri (MMOGameMode.cpp).
- Q_Trolls: fiil duzeltildi ("avla") ama "Bataklik kiyisi" yazilmadi, "Kiyilarda" notr birakildi; troller haritada baslangic alaninda, yer karari §3 soru 5 acik.
- Uygulanmayan: G4 ve G9 (yalniz Lore.md/replik degisikligi; Lore.md'ye dokunulmadi, GenQuestDialogue.py ile uretilecek). G2 "alis" metni de ayni sebeple. Q_Orcs, Q_Gold15000/30000: belgede taslak yok.
- Hub satirlari: yalniz tek basina anlasilan Kael G1 eklendi. Karakaya/Obsidyen/Ayazdis/Alev bolge satirlari uygulanmadi (havuz seviyeye bakmiyor, §3 soru 8 acik). Lyra satirlari uygulanmadi (havuz iyilestirme onayi balonu olarak da kullaniliyor, "Tanrica" sorusu §3 soru 1 acik). Merek G4/G10 satirlari goreve bagli ("onu kendin getireceksin") oldugu icin uygulanmadi.
- Q_DuskGuardians "geçit" (§4) yeni bolgeye ait, taslak yok, dokunulmadi.

**Durum (asagisi ilk analiz):** Salt okunur analiz, 2026-10-05. Bu belgedeki taslaklar yukaridaki notta belirtilenler disinda **öneri, koda işlenmedi**.

**Okunan kaynaklar:** `docs/Lore.md`, `docs/NpcDialogue.md`, `docs/VERTICAL_SLICE_LV1_30.md`, `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`, `Quests/MMOQuestDialogue.cpp`, `World/MMO*NPC.cpp` ve `MMOHealerNPC.h`, `Core/MMOGameMode.cpp` (Kael ve elit spawner bölümü), `Items/MMOItemDatabase.cpp` ile `MMOConsumableDatabase.cpp` (ödül adları), `UI/MMOHUD.cpp` (görev metninin gösterildiği yer).

**Oyunda metin nereden geliyor (koddan okundu):**

| Metin | Kaynak | Not |
| --- | --- | --- |
| Görev adı + kısa açıklama (görev penceresi) | `MMOQuestTypes.cpp` `MakeQuest(Id, Name, Desc…)` | Elle yazılıyor, ASCII. `MMOHUD.cpp:2458` `Description`'ı gösterir. |
| Görev alış/teslim repliği | `MMOQuestDialogue.cpp` | **Otomatik üretiliyor** (`Scripts/GenQuestDialogue.py`, kaynak `docs/Lore.md`). Değişiklik Lore.md'de yapılıp betik yeniden çalıştırılmalı. Konuşan her zaman "Yasli Doran" (`MMOHUD.cpp:2552`). |
| Hub NPC karşılama | `GreetingLines` (Doran, Merek, Lyra, Rana) / `FallbackLines` (Kael, `MMOGameMode.cpp:743`) | Tek havuzdan rastgele seçiliyor; seviyeye ya da bölgeye göre değişmiyor. |

---

## 1. Lv1–30 boşluk analizi

### 1.1 Görevler: seviye bandına göre (kapı seviyesi = `ReqLevel`)

Lv1–30 aralığında 36 görev var. **36'sının da alış ve teslim repliği var** (`MMOQuestDialogue.cpp:17-89`, 73/73 kayıt). Yani eksik replik yok; sorun açıklama alanının yavan/meta olması ve birkaç çelişki.

| Bant | Görev sayısı | Görevler | Yavan/meta açıklama | Lore/veriyle çelişen |
| --- | ---: | --- | ---: | ---: |
| 1–5 | 11 | Goblins, Wolves, Orcs, Bandits, Boss, Cleanup, Level5, Gold500, Gold1000, SpiderQueen, Trolls | 6 | 3 |
| 6–10 | 8 | Warlocks, OrcWarriors, DireWolves, StoneGolem, ShadowAssassin, FrostWraiths, CursedKnights, VenomSpiders | 4 | 3 |
| 11–15 | 2 | OrcBerserkers (12), FrostLich (13) | 0 | 0 |
| 16–20 | 5 | EmberHounds, MagmaGolems, AshOrcs, Level20, Gold5000 | 1 | 0 |
| 21–25 | 4 | InfernalWraiths, MagmaTyrant, Level30, ObsidianStalkers | 1 | 1 (dilbilgisi) |
| 26–30 | 6 | ObsidianSentinels, ObsidianRaiders, ObsidianWarlord, Level40, Gold15000, Gold30000 | 5 | 2 |

**Görevsiz kapı seviyeleri:** 11, 14, 15, 17, 19, 21, 23, 26. Lv13–15'te yeni normal mob da yok (`VERTICAL_SLICE_LV1_30.md` §2.6). Bu bir metin değil içerik boşluğu; yeni görev ID'si/hedef mob gerektirdiği için taslak yazılmadı (bkz. §3 soru 7).

### 1.2 Yavan, meta ya da hatalı görev açıklamaları (`MMOQuestTypes.cpp`)

| Görev | Satır | Mevcut açıklama | Sorun |
| --- | ---: | --- | --- |
| Q_Goblins | 38 | "Koydeki goblinleri temizle" | Lore'da köy yok: Akçakule bir kasaba, goblinler "Çakılı vadilerde" (Lore.md:22). Oyuncunun gördüğü ilk görev. |
| Q_Orcs | 48 | "Guclu orklari avla" | Yavan; yer ve neden yok. |
| Q_Level5 | 287 | "Kendini gelistir" | Yavan (kabul edilebilir). |
| Q_Gold500 | 295 | "500 altin biriktir." | Yalnızca hedef var, söz yok. |
| Q_SpiderQueen | 299 | "Magaranin dehsetini yok et" | Lore: Solgun Ormanlar'ın oyuğu, ağ (Lore.md:84). Teslim repliği de "orman" diyor. |
| Q_StoneGolem | 304 | "Harabelerin bekcisini devir" | Lore: Karakaya'nın taş bekçisi (Lore.md:92). "Harabe" Ayazdiş'e ait. |
| Q_Trolls | 314 | "Ormandaki trolleri sec" | "seç" fiili anlamsız (avla/devir olmalı); Lore'a göre troller "Bataklık kıyısında" (Lore.md:30). |
| Q_Warlocks | 324 | "Ikinci bolgedeki kara buyuculeri" | Meta dil ("ikinci bölge"); Karakaya Geçidi adı yok. |
| Q_OrcWarriors | 332 | "Ikinci bolgedeki ork savascilarini" | Aynı. |
| Q_DireWolves | 339 | "Ikinci bolgedeki korku kurtlarini" | Aynı. |
| Q_Gold1000 | 344 | "1000 altin biriktir." | Yavan. |
| Q_FrostWraiths | 62 | "Ucuncu bolgenin buzul sisini" | Meta dil; Ayazdiş adı yok. |
| Q_Gold5000 | 375 | "5000 altin biriktir." | Yavan; görev adı "Vadinin Tüccarı" hangi vadi belli değil. |
| Q_ObsidianStalkers | 112 | "Obsidyen avcilarini durdur: 5 Obsidyen Avcisi oldur." | Adı iki kez tekrar ediyor, söz yok. |
| Q_ObsidianSentinels | 116 | "Gecidi koruyan muhafizlari" | Obsidyen Sırtları'nda Lore'da geçit yok; "geçit" Karakaya/Alev Vadisi'ne ait. |
| Q_ObsidianRaiders | 120 | "Obsidyen akincilarini dagit" | Tekrar. |
| Q_ObsidianWarlord | 124 | "Obsidyen ordusunun reisini devir" | Yavan; Lore'daki kan bağı yok. |
| Q_Gold15000 / Q_Gold30000 | 378 / 395 | "N altin biriktir." | Yavan. |

İyi durumda olanlar (dokunma önerisi yok): Q_Wolves, Q_Bandits, Q_Boss, Q_Cleanup, Q_CursedKnights, Q_VenomSpiders, Q_OrcBerserkers, Q_FrostLich, Alev Vadisi zinciri (89–109), Q_Level20/30/40 açıklamaları.

### 1.3 Replik hataları (Lore.md → `MMOQuestDialogue.cpp`)

| Görev | Lore.md | MMOQuestDialogue.cpp | Sorun |
| --- | ---: | ---: | --- |
| Q_OrcWarriors | 728–729 | 70 | Teslimde "Çelik Miğfer" diyor; eşyanın adı **"Celik Kask"** (`MMOItemDatabase.cpp:77`). |
| Q_ShadowAssassin | 118–119 | 78 | Teslimde "Ejder Miğferi" diyor; eşyanın adı **"Ejder Kask"** (`MMOItemDatabase.cpp:78`). |
| Q_Level30 | 842–843 | 57 | "Al şu Büyük Mana İksirleri; en zor ders de budur." Ek eksik ("-'ni"), "en zor ders" cümlesi ödül cümlesinin arkasına düşmüş, anlam kopuyor. |
| Q_Level40 | 846–847 | 58 | "Al şu Büyük Can İksirleri; o hikâyeleri…" Aynı ek eksikliği. |
| Q_ObsidianWarlord | 763–764 | 68 | "gölgeler daha **batıda** uzuyor" deniyor ve bir sonraki durak Alacakaranlık Korusu. Lore'a göre Koru Solgun Ormanlar'ın derinliğinde, Solgun Ormanlar da Akçakule'nin **doğusunda** (Lore.md:8, 331). |
| Q_Trolls | 722 | 81 | "Ormanın kıyısında trol" ve Lore "Bataklık kıyısında" (Lore.md:30) birbirini tutmuyor. |
| Q_FrostWraiths | 229 | 43 | Teslimde ödülden (3 Büyük Can İksiri) hiç söz edilmiyor. Hata değil, ama zincirde ödülü anmayan tek eşyalı teslim bu. |

### 1.4 NPC replikleri: kodda olan ile belgede olan

| NPC | Kodda (dosya:satır) | NpcDialogue.md'de | Lv1–30'a özel satır (kodda / belgede) |
| --- | --- | ---: | --- |
| Yaşlı Doran | 5 (`MMOQuestGiverNPC.cpp:14-18`) | 16 | 0 / 2 (Ayazdiş/Alev: 49–51) |
| Satıcı Merek | 5 (`MMOVendorNPC.cpp:14-18`) | 16 | 0 / 3 (26–28) |
| Muhafız Kael | 6 (`MMOGameMode.cpp:744-749`) | 16 | 3 (kurt/ork/haydut, Lv1–5) / 3 + 3 (62–66, 72–74) |
| Şifacı Lyra | 7 (`MMOHealerNPC.cpp:15-21`) + 2 durum (`MMOHealerNPC.h:32,36`) | 19 | 0 / 3 (98–100) |
| Depocu Rana | 4 (`MMOWarehouseNPC.cpp:13-16`) | 8 | Bantla ilgisi yok |

Bantlara göre bölgeye özel NPC repliği:

| Bant / bölge | Belgede | Kodda |
| --- | ---: | ---: |
| 1–5 Solgun Ormanlar | 3 (Kael) | 3 |
| 6–7 Karakaya Geçidi | **0** | 0 |
| 8–13 Ayazdiş Buzulu | 6 | 0 |
| 16–24 Alev Vadisi | 6 | 0 |
| 25–30 Obsidyen Sırtları | **0** | 0 |

Sonuç: Karakaya ve Obsidyen Sırtları için hiç hub repliği yazılmamış (metin boşluğu). Ayazdiş/Alev replikleri yazılmış ama koda işlenmemiş; bu bağlama işi developer'ın. Ayrıca kod havuzu seviyeye bakmadığı için bölge satırları eklenirse Lv1 oyuncu da "Buzula mı çıkıyorsun?" duyar. Seviyeye göre seçim bir sistem kararı (bkz. §3 soru 8).

---

## 2. En önemli 10 boşluk için taslaklar

> Hepsi **öneri, koda işlenmedi**. Kodda ASCII kullanıldığı için developer Türkçe karakterleri çevirmeli. Replik değişiklikleri `docs/Lore.md`'ye yazılıp `GenQuestDialogue.py` ile üretilmeli; `MMOQuestDialogue.cpp` elle düzenlenmemeli. Hedef sayılar mevcut tablodan alındı, değiştirilmedi.

### G1. Q_Goblins: ilk görevde "köy" hatası (öncelik: en yüksek, her oyuncu görür)
- **Açıklama** (`MMOQuestTypes.cpp:38`): "Çakılı vadilerde pusu kuran goblin çetelerini dağıt: 5 Goblin öldür."
- **Teslim:** Mevcut olan (Lore.md:694) yeterli, değişiklik yok.
- **NPC tek satır (Kael):** "Goblin tek başına korkutmaz. Beşi birden çıkarsa sırtını taşa ver."

### G2. Q_Trolls: anlamsız fiil ve yer çelişkisi
- **Açıklama** (`:314`): "Bataklık kıyısında dolaşan trolleri avla: 3 Trol öldür."
- **Alış** (Lore.md:722 yerine): "Bataklığın kıyısında troller dolaşıyor, evlat. Bir tanesi bile bir müfrezeyi dağıtır; üçünü devir, ama tek başına dalma."
- **Teslim:** Mevcut olan yeterli.
- **NPC tek satır (Lyra):** "Trol yumruğu kemiği kırmaz, ezer. Döndüğünde önce bana gel."
- *Not:* Troller haritada başlangıç alanında çıkıyor (Lv1–5 spawner). "Bataklık" görsel olarak yoksa §3 soru 5'e bakılmalı.

### G3. Karakaya üçlüsü: "ikinci bölge" meta dili
- **Q_Warlocks** (`:324`): "Karakaya'da orkları bir bayrak altında toplayan büyücüleri sustur: 4 Kara Büyücü öldür."
- **Q_OrcWarriors** (`:332`): "Karakaya Geçidi'ni tutan baltalı savaşçıları devir: 4 Ork Savaşçısı öldür."
- **Q_DireWolves** (`:339`): "Karakaya'nın kayalıklarında yuvalanan iri kurtları avla: 4 Korku Kurdu öldür."
- **NPC tek satır (Kael, Karakaya için yeni):** "Karakaya'da geceyi geçirme. Korku kurtları karanlıkta görür, sen görmezsin."

### G4. Eşya adı uyuşmazlığı (Miğfer / Kask)
- **Q_OrcWarriors teslim** (Lore.md:728): "Baltalar sustu. Şu Çelik Kask'ı tak; ikinci bir vuruşa fırsat vermesin."
- **Q_ShadowAssassin teslim** (Lore.md:118): "Kervanlar sabaha sağ varıyor artık. Al şu Ejder Kask'ı; gölgeler seni bir daha arkandan bulmasın."
- **NPC tek satır (Merek):** "Kask kafayı korur, yiğit; akıl ayrıca satılmaz. Onu kendin getireceksin."
- *Alternatif:* Eşya adlarını "Çelik Miğfer"/"Ejder Miğferi" yapmak (bkz. §3 soru 4).

### G5. Q_SpiderQueen ile Q_StoneGolem: yer çelişkisi
- **Q_SpiderQueen** (`:299`): "Solgun Ormanlar'ın en karanlık oyuğundaki ağı yak: Örümcek Ana'yı öldür."
- **Q_StoneGolem** (`:304`): "Yanlış bir sözle uyanan taş bekçiyi yeniden uykuya yatır: Kadim Golem'i öldür."
- **Teslim:** İkisinde de mevcut olan yeterli.
- **NPC tek satır (Lyra):** "Örümcek Ana'nın zehri kanı dondurmaz, yavaşlatır. Ağdan çıkar çıkmaz bana gel, evlat."

### G6. Q_FrostWraiths: meta dil, ödül anılmıyor
- **Açıklama** (`:62`): "Ayazdiş'in sisinde dolaşan Uzun Kış ölülerini dağıt: 5 Buz Hayaleti öldür."
- **Teslim** (Lore.md:229): "Sis inceldi. Şimdi buzun altındaki taşları görebiliyorsun, değil mi? Şu Büyük Can İksirleri'ni yanına al; buzulda ateş yakacak vakit bulamazsın."
- **NPC tek satır:** Gerek yok, Kael'in Ayazdiş repliği belgede var (NpcDialogue.md:72). Yalnızca koda bağlanması gerekiyor.

### G7. Q_ObsidianWarlord: yön çelişkisi (Lv30 zincir sonu)
- **Açıklama** (`:124`): "Karakaya'da düşen reisin kanından gelen yeni reisi devir: Obsidyen Reisi'ni öldür."
- **Teslim** (Lore.md:763): "Kan bağı koptu, ordu başsız. Al şu Obsidyen Kılıç'ı. Ama fısıltı buna rağmen susmadı; ormanın derininde gölgeler uzuyor."
- **NPC tek satır (Doran):** "Aynı kandan gelen aynı yoldan düşer, evlat. Yeter ki sen aynı yolda kalma."

### G8. Obsidyen Sırtları: yavan açıklamalar, hiç hub repliği yok
- **Q_ObsidianStalkers** (`:112`): "Kara camın gölgesinde avlanan zehirli avcıları durdur: 5 Obsidyen Avcısı öldür."
- **Q_ObsidianSentinels** (`:116`): "Sırtların yolunu tutan ağır bekçileri devir: 5 Obsidyen Muhafızı öldür."
- **Q_ObsidianRaiders** (`:120`): "Kara cam kalkanlı ork akıncılarını dağıt: 5 Obsidyen Akıncısı öldür."
- **Q_ObsidianSentinels alış** (Lore.md:750, "geçit" kalkar): "Sırtların yolunu ağır bekçiler tutuyor; yavaşlar ama geri çekilmezler, darbeleri adımını ağırlaştırır. Beşini devir ki yol açılsın." Teslimdeki "Geçit açık." da "Yol açık." olmalı.
- **NPC tek satırlar (yeni bölge bloğu):**
  - Merek: "Obsidyen keskin ama gevrektir. Bileyi yanlış tutarsan tezgâhıma kırık getirirsin, yiğit."
  - Kael: "Sırtlarda kara cam güneşi yansıtır. Gözünü kıs, pusuyu yansımasından tanırsın."
  - Lyra: "Kara cam yarası temiz görünür ama derin keser, evlat. Kanaması dinse de sarmadan gitme."
  - Doran: "Ateş bile ardında bir şey bırakır, evlat. Kimi zaman kül, kimi zaman bir kılıç ağzı."

### G9. Q_Level30 / Q_Level40: bozuk teslim cümleleri
- **Q_Level30 teslim** (Lore.md:842): "Usta Savaşçı… Kılıcı artık elin değil, aklın tutuyor, evlat; en zor ders de budur. Al şu Büyük Mana İksirleri'ni."
- **Q_Level40 teslim** (Lore.md:846): "Efsanevi Şövalye. Akçakule'nin ocak başlarında adın anılıyor, evlat; o hikâyeleri boşa çıkarma. Al şu Büyük Can İksirleri'ni."
- **Q_Level30 açıklama:** Mevcut olan ("Ustaligini taclandir") yeterli.

### G10. Erken servet/seviye görevleri: yalnızca hedef sayısı yazıyor
- **Q_Gold500** (`:295`): "Pazar boş keseye selam vermez: 500 altın biriktir."
- **Q_Gold1000** (`:344`): "Hazırlıksız maceracının hesabı bellidir: 1000 altın biriktir."
- **Q_Gold5000** (`:375`): "Adın tüccarlar arasında geçsin: 5000 altın biriktir."
- **Q_Level5** (`:287`): "İlk sınavını geç: 5. seviyeye ulaş."
- **Teslim:** Mevcut olanlar (Lore.md:863–870, 834) yeterli.
- **NPC tek satır (Merek, Q_Gold500 sonrası için):** "Beş yüz altın mı? Hıh. Bir kılıç parası bile etmez ama, saymaya başlamışsın ya."

---

## 3. Kullanıcı onayı gereken ton/lore soruları

1. **Lyra ve "Tanrıça":** Kodda (`MMOHealerNPC.cpp:21`) ve NpcDialogue.md:94, 119'da "Tanrıça" geçiyor, Lore.md'de ise hiçbir tanrı/tanrıça yok (yalnız Gümüş Dal Tarikatı). Tarikatın bir tanrıçası mı var, yoksa "Tanrıça" yerine "Gümüş Dal" mı denmeli?
2. **Lyra parasız hastayı geri çeviriyor mu?** Kod geri çeviriyor ("şifa da bir bedel ister", `MMOHealerNPC.h:32`). NpcDialogue.md:87 ve 91 ise "kapım herkese açık" ve "Kesen boş mu? Yaranı yine de sararım" diyor. Hangisi esas?
3. **Kilometre taşı teslimlerini kim söylüyor?** NpcDialogue.md:104–133 Q_Level20/50'yi Kael'e, Q_Level65'i Lyra'ya, Q_Gold*'u Merek'e veriyor. Kod ise hepsini Doran'a söyletiyor (`MMOHUD.cpp:2552`, metin Lore.md:823+). Aynı görev için iki farklı teslim metni var.
4. **Miğfer mi Kask mı?** Replikleri mi düzeltelim (G4), eşya adını mı değiştirelim?
5. **Ork Reisi, Kadim Golem ve troller nerede?** Lore Ork Reisi ile Kadim Golem'i Karakaya Geçidi'ne koyuyor (Lore.md:88, 92). Spawner'ları ise başlangıç mob alanının yanında (`MMOGameMode.cpp:493-529`), Karakaya alanı ayrı bir yerde (`:555+`). Troller Lore'da bataklıkta, haritada ise başlangıç alanında. Lore mu uyarlansın, açıklama metinleri mi yere göre yazılsın?
6. **Sınıf-nötr unvanlar:** Rahip ya da Büyücü de "Kıdemli Savaşçı", "Usta Savaşçı", "Efsanevi Şövalye" unvanını alıyor. Kalsın mı?
7. **Lv13–15 ve 23–24 içerik boşluğu:** Yan görev isteniyorsa hedef mob, ID ve ödül developer/kullanıcıdan gelmeli; metni sonra yazarım.
8. **Bölgeye özel hub replikleri** seviyeye göre mi gösterilsin (sistem işi), yoksa genel havuza mı eklensin?
9. **"Vadinin Tüccarı" (Q_Gold5000):** Alev Vadisi kastediliyorsa (kapı Lv16) bu böyle mi kalsın, yoksa ad değişsin mi?

## 4. Ek tutarsızlıklar (Lv1–30 dışı ya da küçük)

- **Lyra kod repliği kaymış:** `MMOHealerNPC.cpp:18` "Kanin durdu, ruhun da dinlensin", NpcDialogue.md:86 ise "Kanın durdu, ama ruhun hâlâ titriyor".
- **Hitap farkı:** Lyra kodda "evladim" (`MMOHealerNPC.h:32,36`), belgede "evlat" diyor.
- **Rana'nın durum replikleri** (NpcDialogue.md:208–219) kullanılmıyor. Kod "Depo dolu!" / "Canta dolu!" sistem mesajı veriyor (`MMOInventoryComponent.cpp:729, 771`).
- **Q_DuskGuardians** (`MMOQuestTypes.cpp:134`, Lv33) da "Gecidin muhafizlarini" diyor; Koru'da geçit yok (G8 ile aynı sorun).
- **Kael için belge-kod sayısı:** `VERTICAL_SLICE_LV1_30.md` §1.1'deki "6 yedek replik" kodla tutuyor (`MMOGameMode.cpp:743`), NpcDialogue.md'de ise 16 Kael repliği var.
