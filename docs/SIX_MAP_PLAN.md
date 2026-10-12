# Altı harita ve ırk planı (kapalı alfa, ilk aşama)

**Tarih:** 2026-10-12 · **Durum:** tasarım `[PROPOSED]`, kod yok · **Hazırlayan:** oyun-tasarimcisi ajanı
**Gereksinim (kullanıcı, 2026-10-12, uygulanacak kapsam):**

| # | Harita | Kim girer | Seviye | İçerik |
|---|---|---|---|---|
| H1 | Ortak merkez (KO Moradon / Rise Dorion) | İnsan + Ork | 1-25 | Kasma, görev, **pazar kurma**, eşya alış-satışı |
| H2 | İnsan krallığı (KO El Morad / Rise Lunaskar) | Yalnız İnsan | 25-59 | Kasma, görev |
| H3 | Ork krallığı (KO Luferson / Rise Protean) | Yalnız Ork | 25-59 | Kasma, görev |
| H4 | İnsan üst bölge (KO Eslant / Rise Haddar-Hagard) | Yalnız İnsan | 59-80 | Kasma, görev |
| H5 | Ork üst bölge | Yalnız Ork | 59-80 | Kasma, görev |
| H6 | Ortak PvP | İnsan + Ork | karar (§6) | PvP + PvE |

Her haritada NPC ve mob zorunlu.

Etiketler: **[Kesin]** = koddan/dosyadan okundu (dosya:satır) · **[Muhtemel]** = koddan çıkarım, oyunda doğrulanmadı · **[Tahmin]** = öngörü.
`[PLACEHOLDER]` sayılar ölçümden önce değişir.

---

## 0. Oyuncu deneyimi ve neden şimdi

Oyuncu karakterini yaratırken İnsan ya da Ork seçer. İlk 25 seviyeyi iki ırkın paylaştığı merkez kasabada geçirir: kasar, görev yapar,
pazar kurar, ticaret yapar. Lv25'te kendi ırkının krallığına geçer, Lv59'da ırkının üst bölgesine çıkar. Karşı ırkla yalnız
merkezde (barış) ve PvP haritasında (savaş) karşılaşır. Bu, Knight Online'ın iki ulus döngüsüdür ve oyunun kimliğini taşır.
Neden şimdi: kapalı alfanın kapsamı bu. Irk, kayıt, geçiş, PvP ve sosyal sistemlerin hepsine dokunuyor. Alfa öncesinde
tasarlanmazsa her sistem ikinci kez açılır.

---

## 1. Mevcut durum (kod gerçeği)

### 1.1 Haritalar [Kesin]

| Harita | Seviye | Spawner | NPC | Kaynak |
|---|---|---:|---|---|
| `Summer_scene` | Lv1-40 + Lv58-65 dış cepler | **18** (Lv1-24: 11, Lv25-30: 2, Lv31-40: 2, Lv58-65: 3) | 5: satıcı, görevli, konuşan (Kael), şifacı, depocu | `MMOGameMode.cpp:1094-1434` |
| `Winter_scene` | Lv42-57 + Lv66-80 | **17** (Lv42-57: 8, Lv66-80: 9) | 4: satıcı, görevli, şifacı, depocu | `MMOGameMode.cpp:972-1093` |
| `Lvl_ThirdPerson`, `Overview` | — | 0 | 0 | `SpawnMobArea` uyarı yazıp çıkar (`:1097-1101`) |

- NPC sınıfları haritadan bağımsız: satıcı tüm stoğu satar, görevli tüm görevleri gösterir (`MMOVendorNPC.h`, `MMOQuestGiverNPC.h`).
  Yerleşim PlayerStart'a göre sabit ofsetle yapılır (`MMOGameMode.cpp:1395-1434`). İlk bulunan PlayerStart kullanılır (`:956-962`).
- İçerik tek doğrusal zincir (`CONTENT_MAP.md`): **59 mob (13 elit), 97 görev**. Bantlara bölünce (CONTENT_MAP satırlarından sayıldı):
  Lv1-24 = 19 normal + 6 elit + 37 görev, Lv25-58 = 17 + 4 + 35, Lv59-80 = 10 + 3 + 25.

### 1.2 Bu kapsam için **olmayan** sistemler [Kesin, grep]

| Sistem | Bugün | Kanıt |
|---|---|---|
| Irk / ulus | **Yok.** `Source`'ta Race/Faction/Nation kavramı yok (koordinatör grep'i + bu inceleme) | — |
| Karakter yaratma ekranı | Yok. Ad `?Name=` ile gelir, sınıf oyun içinde ilk girişte seçilir (`bClassChoicePending`, `MMOCharacter.h:136`; HUD paneli `ClassChoiceRegions`) | `MMOGameMode.cpp:445-619` |
| Pazar kurma (oyuncu tezgâhı) | **Yok.** Alış-satış = NPC satıcı + oyuncular arası takas (`UMMOTradeComponent`) | grep `stall/merchant/pazar` boş |
| Harita başına PvP | Yok. PvP **sunucu genelinde tek anahtar**: `AMMOGameMode::bAllowPvP` (`MMOGameMode.h:86`), ayna `AMMOGameState::bPvPEnabled`, konsol `MMOPvP` | `MMOCharacter.cpp:2221-2254` |
| Ölüm cezası ayrımı | Ölüm her zaman XP kaybı (`ApplyDeathPenalty`, `MMOStatsComponent.cpp:544`). Öldüren oyuncu mu mob mu ayrımı ceza tarafında yok | `MMOCharacter.cpp:1891` [Muhtemel] |
| Irka göre doğuş noktası | Yok, ilk PlayerStart | `MMOGameMode.cpp:956` |

### 1.3 Geçiş bugün [Kesin]

- **Yalnız Summer ↔ Winter aç/kapa** (`MMOGameMode.cpp:647-662`). Diğer haritalarda "Bu haritadan bolge gecisi yok".
- **Bütün oturum birlikte taşınır.** Yalnız listen server hostu tetikler (`MMOAdmin::IsServerAdmin`), tüm oyuncular canlı ve savaş
  dışında olmalı, her karakter asenkron kaydedilir (15 sn bariyer, `MMOGameMode.h:107`), sonra `World->ServerTravel` (`:630-884`).
- Varışta kayıtlı `MapName` farklıysa karakter PlayerStart'a konur (`MMOCharacter.cpp:1456-1475`). `MapName` kayıtta zaten var (`MMOSaveGame.h:113`).
- Karakter kayıtları **host makinesinde** yerel dosya (`MMOLocalFileCharacterStore`). Başka bir hosta bağlanan oyuncunun karakteri orada yok [Kesin].

### 1.4 KRİTİK mimari çatışma: ırka özel haritalar ile "tek oturum tek harita" [Kesin + çıkarım]

Bugünkü model, bir sunucu sürecinde aynı anda **yalnız bir harita** yükler ve herkes birlikte taşınır. Bunun sonuçları:
- Karışık ırklı bir oturum H2-H5'e **hiç giremez**: İnsan haritasına geçiş, oturumdaki Ork oyuncuyu da taşır.
- İnsan oyuncu H2'de kasarken Ork oyuncu aynı anda H3'te olamaz. KO'daki "iki krallık aynı anda yaşıyor" deneyimi bu modelde yok.
- Gerçek çözüm **harita başına sunucu**: her harita ayrı süreç, oyuncu başına `ClientTravel` ve **paylaşılan karakter deposu**
  (bulut/DB). Engelleri: dedicated Server target yok ve kurulu motor desteklemiyor (`NETWORK_AUDIT.md:445`). Paylaşılan depo yok
  (`ACCOUNT_BACKEND_COMPARISON.md`), kimlik açığı K-1 (`SECURITY_AUDIT.md`). **Bu planın kapsamı dışında, haftalar-aylar sürer [Tahmin].**

**Alfa için uygulanabilir yol (öneri):** "oturum ırkı" kuralı.
- Bir oturumdaki oyuncular H2-H5'e yalnız **hepsi aynı ırktaysa** geçebilir. Geçiş, karşı ırktan biri varsa reddedilir ve nedeni hostta görünür.
- H1 ve H6 karışık oturuma açıktır. Alfa oturumları "İnsan akşamı / Ork akşamı / savaş akşamı" diye planlanır.
- Karakterler tek host makinesinde durduğu için alfa boyunca **aynı makine** host olmalıdır. Aksi halde karakterler bölünür.
- Bu kural ileride harita-başına-sunucuya geçişte kaldırılır. Tablo ve ırk kapısı aynı kalır, yalnız taşıma biçimi değişir.

---

## 2. Harita yapısı ve mevcut içeriğin eşlenmesi

### 2.1 Arazi kaynağı: kopyala + yeniden giydir

- Mevcut iki harita **yeniden adlandırılmaz** (kayıtlardaki `MapName`, betikler, CI bu adlara bağlı).
- Yeni haritalar editörde kopya olarak çıkar: `/Game/HandPaintedEnvironment/Maps/<Ad>_scene/<Ad>_scene`. Bu yol kalıbını
  `SmokeTest.ps1:15` değiştirmeden okur. Arazi ve çarpışma aynı kaldığı için **ölçülmüş cep koordinatları kopyada geçerli kalır**
  [Muhtemel; kopya başına `MMOProbeSurface` ile yeniden ölçülür].
- **İnsan/Ork çiftinin aynı araziyi paylaşması türe uygun:** KO'da El Morad/Luferson ve Eslant'ın iki yarısı ayna tasarımlardır.
  Fark giydirme (renk, bayrak, mimari prop) ve NPC'lerdedir.
- Winter'da **17** ölçülmüş cep konumu var (`MMOGameMode.cpp:1038-1076`). Summer'da hub'a ≥2.600 u uzak ve birbirinden ≥1.800 u
  açık **8** ölçülmüş konum var: Obsidyen, Obsidyen Reisi, Çözülme, Alacakaranlık, Hükümdar, Arayıcı, Bekçi, Haberci
  (PlayerStart (-816,-47)'ye 2.648-6.922 u; hesap bu incelemede yapıldı).

### 2.2 Harita → dosya → cep eşlemesi

| # | Kimlik (kod) | Görünen ad `[PLACEHOLDER]`, `metin-yazari` | Dosya | Yapım | Spawner | Cepler (mevcut koordinatlar) |
|---|---|---|---|---|---:|---|
| H1 | `Hub` | Akçakule (ortak merkez) | `Summer_scene` | Var | **11** | Summer Lv1-24 cepleri yerinde: ana 1-5, 6-7, 8-12, 13-15, 16-22 + 6 elit (SpiderQueen, OrcWarlord, StoneGolem, ShadowAssassin, FrostLich, MagmaTyrant) |
| H2 | `Human25` | Ravenmoor (Lore'da adı geçen İnsan sınırı, `Lore.md:5`) | `Winter_scene` | Var, İnsan giydirmesi | **16** | Winter A-konumları (bugünkü 42-57, 8 cep) yerinde. Winter B-konumlarında (bugünkü 66-80, 9 konum): Obsidyen 25-27, 27-29, Reis 30, Alacakaranlık 31-33, 36, 39, Hükümdar 40, Yarık Arayıcısı 58 (8) |
| H3 | `Orc25` | Ork yurdu (adı yok, Lore yazılacak) | `OrcLand_scene` (yeni) | Winter kopyası, Ork giydirmesi | **16** | H2 ile aynı koordinat ve bantlar (ayna) |
| H4 | `Human59` | İnsan üst bölge | `HumanHigh_scene` (yeni) | Winter kopyası | **13** | B-konumları: Kül 66/68/69 + Tiran, Uçurum 72/74/76/78 + Naib (9, bugünkü koordinatlar **aynen**). A-konumlarında: Yarık 59-60, 61-62, 63-64 + Haberci (4) |
| H5 | `Orc59` | Ork üst bölge | `OrcHigh_scene` (yeni) | Winter kopyası | **13** | H4 ile aynı (ayna) |
| H6 | `PvP` | Savaş alanı | `PvP_scene` (yeni) | Summer kopyası | 4-6 `[PLACEHOLDER]` + iki ırk kampı | Summer'ın 8 uzak konumundan PvE cepleri (bant kararı §6). İki kamp için iki PlayerStart (ırk etiketi) |

Toplam **69-71 spawner** (bugün 35). Yüklü haritada en çok 16 (bugün Summer'da 18), sunucu yükü artmaz.
Winter arazisi 4 haritada (H2-H5) kullanılır. Ayna çift türe uygun ama H2 ile H4'ün aynı arazide olması **gerçek bir tekrar**.
Giydirme farkı büyük tutulmalı ya da H4/H5 sonra özgün araziye taşınmalı (§7, alfa sonrası).

**H2/H3 rota riski [Muhtemel]:** Lv25 oyuncusu hub'dan Lv25-41 ceplerine (B-konumları, hub'a 3.800-6.800 u) yürürken
Lv42-57 ceplerinin (A-konumları, hub'a 3.000-6.000 u) yanından geçebilir. Winter için `ProbeThawRoute.py` eşdeğeri yok
(`CONTENT_PLAN_LV46_80.md` §3.5). Seçilen eşlemede rota açıklığı ölçülmeli. Gerekirse düşük bantla yüksek bant konum takası yapılır.

### 2.3 Mevcut içeriğin yeni yapıya eşlenmesi

| Bant | Bugün nerede | Yeni yer | Mob (normal+elit) | Görev |
|---|---|---|---:|---:|
| Lv1-24 | Summer | H1 (ortak) | 19 + 6 | 37 |
| Lv25-41 (Obsidyen, Alacakaranlık) | Summer | H2 **ve** H3 | 7 + 2 | ~20 |
| Lv42-57 (Kış, Buzbağlı) | Winter | H2 **ve** H3 | 9 + 2 | ~14 |
| Lv58 (Yarık Arayıcısı) | Summer | H2/H3 (59 kapısına ulaşmak için) | 1 | ~1 |
| Lv59-65 (Yarık) | Summer | H4 **ve** H5 | 3 + 1 | ~7 |
| Lv66-80 (Kül, Uçurum) | Winter | H4 **ve** H5 | 7 + 2 | ~18 |

Görev sayıları `~` ile verildi: görev kapı seviyesine göre sayıldı (`CONTENT_MAP.md`). Lv25-58 toplamı 35, Lv59-80 toplamı 25 [Kesin].

**İki paralel zincir nasıl kurulur. Seçenekler:**

| Seçenek | Ne yapılır | Yeni tablo satırı | İş [Tahmin] | Sonuç |
|---|---|---:|---|---|
| **P1 (önerilen alfa)** | Aynı mob ve aynı görev kimlikleri iki ırk haritasında da doğar. Görevler karakter başına zaten ayrı, zincir kodda tek kalır. Irka özgü olan: NPC adları, konuşan NPC replikleri, harita giydirmesi | 0 mob, 0 görev | 1-2 gün (yerleşim satırı + metin) | İki ırk aynı hikâyeyi farklı kampta yaşar. Görev metni ırk-nötr olmalı |
| P2 | Görev metinleri ırka göre iki sürüm. Kimlik ve mob aynı | 0 kod satırı; `Lore.md`/diyalog ~60 görev × 2 | 4-6 gün (`metin-yazari` + `GenQuestDialogue.py`'ye ırk alanı) | Lore zenginleşir, kod değişmez |
| P3 | Lv25+ için Ork tarafına ayrı mob ve görev seti (yeni kimlikler) | +34 mob, +60 görev, +eşya kademeleri | 10-15 gün + `ValidateData`/`ContentMap`/denge modeli iki katı | **Alfa için değmez:** aynı dövüşü ad değiştirerek kopyalamak test edilecek yeni sistem kazandırmaz, bakım yükünü ikiye katlar |

**Lore çatışması (P1/P2 için zorunlu metin işi) [Kesin]:** Mob tablosunda Ork düşmanlar var (Orc, OrcWarrior, OrcWarlord, OrcBerserker,
AshOrc ve Obsidyen/Alacakaranlık "ork akıncıları", `Lore.md:365-410`). Ork oyuncu Ork mob kesecek. `metin-yazari` bunları
"Kırık Klan / Fısıltıya uymuş dönek orklar" gibi bir fraksiyona bağlamalı. Mob kimlikleri değişmez, yalnız görünen ad ve Lore değişir.
Akçakule ve Kül Muhafızları İnsan-merkezli. H1'in "iki ırkın ortak sınır pazarı" olarak yeniden anlatılması gerekir.

### 2.4 Summer'ın Lv1-40'tan Lv1-25'e inmesinin etkisi

| Etki | Ayrıntı | Etiket |
|---|---|---|
| Summer'dan çıkan spawner | Obsidyen 25-29, Obsidyen Reisi, Alacakaranlık 31-39, Hükümdar, Yarık ×3 = **7**. Summer **11** spawner'a iner | [Kesin] |
| Rota riski kalkar | Summer'da Lv58+ ve Lv25+ cep kalmaz. Lv7-40 oyuncunun Yarık mobu çekmesi (QA 527ca1f) ve Çözülme rotası sorunu (`MMOGameMode.cpp:1277-1298`) ortadan kalkar | [Kesin] |
| Boşalan 8 uzak konum | Lv13-24 için ek cep olabilir (merkezde kalış uzar) ya da pazar alanı için boş bırakılır | [Tahmin] |
| Merkezde kalış süresi | Lv25'e ~2,4-2,6 saat (TimelineModel 25 sn/öldürme; Lv20 = 1,6 s, Lv30 = 3,0 s) | [Tahmin] |
| Eski kayıtlar | Summer'da Lv25+ konumdaki karakter boş arazide doğar, yürür. Zararsız | [Kesin] `MMOCharacter.cpp:1459` |
| Görevli NPC | Lv25+ görevleri de gösterir (tek görev penceresi). Alfada kabul. Seviye kuralı listeyi zaten süzüyor [Muhtemel] | — |
| `Q_Level30`, `Q_Gold15000` vb. | Kilometre taşları haritadan bağımsız, etkilenmez | [Kesin] |

### 2.5 NPC seti (her harita)

| Rol | Sınıf | H1 | H2-H5 | H6 |
|---|---|---|---|---|
| Satıcı | `AMMOVendorNPC` | Zorunlu | Zorunlu | Zorunlu (her kampta) |
| Görev veren | `AMMOQuestGiverNPC` | Zorunlu | Zorunlu | Zorunlu (PvE cepleri için) |
| Şifacı | `AMMOHealerNPC` | Zorunlu | Zorunlu | Zorunlu (her kampta) |
| Depocu | `AMMOWarehouseNPC` | Zorunlu | Zorunlu | Önerilir |
| Konuşan | `AMMOTalkerNPC` | Var (Kael) | Önerilir (ırk sesi) | İsteğe bağlı |

**Zorunlu minimum: 4 NPC/harita. H6'da iki kampın her birinde 3.** H6 kamp NPC'leri ırka bağlıdır: karşı ırkın satıcısı
kullanılamaz. Bu yeni bir kontrol (§4.3).
**Değmez (alfa):** harita başına ayrı satıcı stoğu. Seviye kuralı (`VendorLevelSlack` 3) zaten süzüyor.

### 2.6 Geçiş grafiği

```
                         +--(Lv25, Insan)--> H2 Human25 --(Lv59)--> H4 Human59
H1 Hub (Lv1-25, ortak) --+
                         +--(Lv25, Ork)----> H3 Orc25 ----(Lv59)--> H5 Orc59
   ^      |                   |                    |                    |
   |      +--(Lv>=X, karar)--> H6 PvP <------------+--------------------+
   +------------------- her haritadan H1'e donus ------------------------+
```

| Kenar | Kapı (sunucuda) | Etiket |
|---|---|---|
| H1 → H2/H3 | Oturumdaki **herkes** aynı ırk ve hedefin ırkı. Herkes Lv ≥ 25 `[PLACEHOLDER]` | Öneri |
| H2 → H4, H3 → H5 | Aynı ırk kuralı + herkes Lv ≥ 59 | Öneri |
| Herhangi → H6 | Herkes Lv ≥ X (§6 K1). Karışık ırk serbest | Karar |
| Herhangi → H1 | Kapı yok | Öneri |
| H2 ↔ H3, H4 ↔ H5 | **Yasak** (karşı ırk) | Öneri |

"Herkes" kuralı tek-oturum modelinden gelir (§1.4). Seviye kapısında alternatif: yalnız uyarı (alfada karışık seviyeli test grubu).
Irk kapısında alternatif yok, çünkü karşı ırk haritasına girmek gereksinimi çiğner.
Irk haritasındaki **varış**: tek PlayerStart (ırk haritası tek ırka ait). H6'da varış ırk etiketli iki PlayerStart'tan seçilir.

---

## 3. Irk sistemi (yeni)

### 3.1 Bileşenler

| # | Parça | Tasarım | Sunucu otoritesi / replikasyon |
|---|---|---|---|
| I1 | Tür | `enum class EMMORace : uint8 { None, Human, Orc }` (ASCII, yeni başlık `Public/Core/MMORace.h`) | — |
| I2 | Kalıcılık | **Ayrı yan dosya** `Saved/MMORace/<SlotKey>.json` (başarım/günlük deseni: `.tmp` → Move, bozuk dosya `.tmp.bad`, yuva taşımada eski dosyayı silme kuralları aynı). `MMOSaveGame.h`'e **dokunulmaz** | Yalnız sunucu yazar. Okuma `AMMOCharacter::IsInitialLoadComplete()` sonrasında |
| I3 | Seçim | İlk girişte, sınıf seçimi gibi HUD paneli (`ClassChoiceRegions` deseni). Irk yoksa seçilene kadar H1 dışına geçiş yok | `ServerChooseRace(EMMORace)`: yalnız `Race == None` iken, `_Validate` ile değer aralığı. Kalıcı yazım başarılı olunca uygulanır. **Değiştirilemez** (alfa) |
| I4 | Görünürlük | `AMMOCharacter` üzerinde `ReplicatedUsing=OnRep_Race`. Ad etiketi rengi ve hedef imleci için | `DOREPLIFETIME`, `StaticLint` kuralları |
| I5 | Kimlik bağı | Irk, kayıt yuvasına (ad = yuva, K-1) bağlı. K-1 açığı ırkı da etkiler: adı ele geçiren ırkı da alır, yeni açık değil | `ACCOUNT_IDENTITY_DESIGN.md` |
| I6 | Eski karakterler | Irksız kayıt → ilk girişte seçim paneli (H1'e konur) | Göç kodu yok, yalnız `None` durumu |

**Yan dosya riski [Muhtemel]:** ırk dosyası kaybolur ya da bozulursa karakter `None` olur ve yeniden seçer, yani ırk değiştirebilir.
Önlem: `.bad` durumunda seçimi kilitle ("ırk kaydı okunamadı, yöneticiye bildir"). Ana kayıtta ırk olmaması bu riskin bedeli.
`MMOSaveGame.h` korunan yol olduğu için kullanıcı kararı (§8 S3).

### 3.2 Irk kuralları (her sistem için)

| Sistem | Dosya (bugün) | Kural (öneri, KO) | Dokunuş | Çakışma |
|---|---|---|---|---|
| Geçiş | `MMOGameMode.cpp:630` | §2.6 kapıları | Orta | Sıcak dosya |
| Hasar / PvP | `MMOCharacter.cpp:2215-2260` (PvP kapısı) | H6'da yalnız **karşı ırka** hasar. Diğer haritalarda oyuncu-oyuncu hasarı yok. Global `bAllowPvP` yerine bölge tablosundan `bPvPZone` (konsol anahtarı test için kalır) | Orta | `MMOCharacter` sıcak |
| Parti | `UMMOPartyComponent` | Yalnız aynı ırk | Küçük kontrol | **Codex'in dosyası** (`CheckPrPaths.py` uyarısı). Codex'le sıra planla |
| Klan | `UMMOClanComponent` | Yalnız aynı ırk | Küçük | — |
| Takas | `UMMOTradeComponent` | H1'de karşı ırkla **serbest** (ortak pazar fikri, karar §8 S4). Diğer yerlerde aynı ırk | Küçük | **Codex'in dosyası** |
| Posta | `UMMOMailComponent` | Aynı ırk. Çevrimdışı alıcının ırkı yan dosyadan okunur. Okunamıyorsa gönderim reddedilir ve iade edilir (bugünkü `StoreError` deseni) | Orta | — |
| Fısıltı `/w` ve sohbet | `MMOPlayerController.cpp` | Genel sohbet ırka göre kanal. Karşı ırka fısıltı yok. H1/H6'da "yakın" sohbet herkes | Orta | `MMOPlayerController` sıcak |
| Arkadaş | `UMMOFriendsComponent` | Yalnız aynı ırk | Küçük | — |
| NPC | `AMMONpcBase` | H6 kamp NPC'si yalnız kendi ırkına hizmet eder | Küçük | — |
| `/who` | `MMOPlayerController.cpp` | Irk sütunu | Küçük | — |

### 3.3 Pazar kurma (H1 gereksinimi, yeni sistem)

Bugün yok (§1.2). En küçük uygulanabilir tasarım:
- `UMMOMerchantComponent`: oyuncu H1'in pazar alanında (hub güvenli bölgesi) tezgâh açar. Çantasından en çok 8 `[PLACEHOLDER]`
  eşya ve fiyat koyar. Tezgâh açıkken hareket edemez. Diğer oyuncu tıklayıp satın alır.
- Sunucu otoritesi: satın alma tek RPC'dir. Sunucu satıcının hâlâ orada ve eşyanın hâlâ çantada olduğunu doğrular. Altın ve eşya
  tek işlemde yer değiştirir. **Her iki taraf işlemden sonra kaydedilir** (takastaki `TryExecuteTrade` deseni). Çevrimdışı satış yok
  (KO'daki gibi oyuncu bağlıyken).
- Ekonomi: satıştan **%2-3 `[PLACEHOLDER]` pazar vergisi** altın sink'i olur (postadaki %2 ile tutarlı). Yeni altın kaynağı yok.
- Güvenlik: eşya ikileme (aynı eşya hem tezgâhta hem takasta veya postada) en büyük risk. Tezgâhtaki eşya kilitlenir.
  `ag-guvenlik-denetcisi` zorunlu.
- **Alfa alternatifi:** pazar kurma ertelenir. H1'de NPC satıcı + mevcut takas "eşya alış-satışı" gereksinimini karşılar. Kullanıcı
  "pazar kurma" dedi, bu yüzden plan sistemi içerir. Erteleme yalnız kullanıcı onayıyla (§8 S5).

---

## 4. Kodda değişecek noktalar

### 4.1 İki haritayı varsayan kod ve betikler [Kesin]

| # | Yer | Bugün | Yeni |
|---|---|---|---|
| K1 | `MMOGameMode.cpp:647-662` | Summer↔Winter aç/kapa | Bölge tablosundan hedef + ırk/seviye kapısı + paket-var kontrolü |
| K2 | `MMOGameMode.h:72` | `RequestZoneTravel(APlayerController*, FString&)` | Hedef kimliği alan aşırı yükleme. Eski imza Summer↔Winter'ı korur, böylece 6 `TestZoneTravel*.ps1` değişmez |
| K3 | `MMOGameMode.cpp:972`, `1097`, `1262` | Harita adına göre if/else | Bölge tablosu |
| K4 | `MMOGameMode.cpp:944-1435` `SpawnMobArea` (~490 satır) | Koordinatlar fonksiyonda, iki lambda | Yeni `World/MMOZoneLayout.{h,cpp}`, `MakeZone`/`MakePocket`/`MakeNpc` satırları |
| K5 | `MMOGameMode.cpp:956-962` | İlk PlayerStart | H6'da ırk etiketli PlayerStart seçimi (`ChoosePlayerStart` override; yeni motor override → `StaticLint.py` `OVERRIDE_WHITELIST`) |
| K6 | `MMOPlayerController.h:28-30`, `.cpp:58-78` | `MMOTravel()` | `MMOTravel <Bolge>` (argümansız eski davranış kalır) |
| K7 | `MMOHUD.cpp:189-210`, `3102` | Tek düğme "Kış'a/Yaz'a taşı" | Hedef listesi + ırk seçim paneli |
| K8 | `MMOCharacter.cpp:2215-2260` | Global PvP | Bölge `bPvPZone` + karşı ırk kuralı |
| K9 | `MMOCharacter.cpp:1458-1460` | Harita adsız eski kayıt = Summer | **Değişmez** |
| K10 | `Config/DefaultGame.ini:12-13` `MapsToCook` | 2 harita | 6 harita (Config korunan yol: yerel oturum) |
| S1 | `SoakTest.ps1:9`, `NetSmokeTest.ps1:6` ValidateSet, `RunAllChecks.ps1:408` dizi | 2 harita | 6 harita |
| S2 | `PocketSpacing.py:28,45` | Harita = lambda adı | Harita = `MakePocket` bölge alanı. `TestPocketSpacing.py` |
| S3 | `ValidateData.py:167-277` | `MMOGameMode.cpp` regex | `MakePocket` satırları + yeni kurallar (§5). `TestValidateData.py` |
| S4 | `docs/PLAYTEST_LV46_80.md` üçlüleri | Haritasız | Harita etiketi |

### 4.2 Sıcak dosya / Codex çakışma planı

`git status` (2026-10-12) temiz. Son commit'ler `MMOGameMode.cpp`'nin `ChangeName/PreLogin` bölgesine (445-619) dokundu.

| Dosya | Dokunuş | Plan |
|---|---|---|
| `MMOGameMode.cpp` | `SpawnMobArea` taşınır (bir kerelik büyük diff), `RequestZoneTravel` başı | Tek ajan, tek commit. 445-619'a dokunulmaz |
| `MMOCharacter.cpp/.h` (sıcak) | Irk alanı + RPC + PvP kapısı | Irk mantığı yeni `UMMORaceComponent`'te. Karakterde yalnız bileşen oluşturma ve PvP kapısında tek çağrı |
| `MMOPlayerController` (sıcak) | `MMOTravel` argümanı, sohbet kanalı | En sona, küçük commit'ler |
| `MMOPartyComponent.*`, `MMOTradeComponent.*` | Irk kontrolü | **Codex'e devredilir** ya da Codex'in temiz anında tek satırlık çağrı (`MMORace::AreAllied(A,B)`) |
| `RunAllChecks.ps1`, `PLAYTEST_CHECKLIST.md` (sıcak) | Dizi satırı, yeni adımlar, PIE maddeleri | Yalnız ekleme |
| `MMOSaveGame.h` | **Dokunulmaz** | Irk yan dosyada |

---

## 5. Doğrulama (her harita + ırk)

| Kontrol | Araç | "Bitti" | Bulut? |
|---|---|---|---|
| Harita açılıyor, mob ve NPC doğuyor | `SmokeTest.ps1 -Map <Ad>_scene` | Çıkış 0, hata 0, `Mob dogumu` > 0, NPC log satırları (H6'da iki kamp) | Hayır |
| Cep kalitesi | `MMOProbeSurface X Y 225` (elit 150) | ok ≥ 60/64 | Hayır |
| Takılma ve ilk doldurma | `SoakTest.ps1 -Seconds 90` + `SpawnAudit.py` | Takılma ≤ 1, her cep ilk doldurmada ≥ 4/6 | Hayır |
| Cep aralığı | `PocketSpacing.py` (bölge bazlı) | Hata yok | Evet |
| Veri | `ValidateData.py` yeni kurallar: (1) her bölgede ≥1 normal spawner ve zorunlu NPC'ler; (2) cep bandı bölge bandında; (3) her `KillSpecificMob` hedefi görevin seviyesinde **her iki ırkın** erişebildiği bir bölgede doğuyor; (4) Lv1-80 her seviye her ırk için bir bölgede; (5) ırk haritası çiftleri (H2/H3, H4/H5) aynı arketip kümesini üretiyor (ayna eşitliği) | Hata yok + her kural için negatif test | Evet |
| Irk native testi | Yeni `Scripts/TestRace.ps1` (`TestAchievements.ps1` deseni) | Seçim bir kez; ikinci seçim reddedilir; `None` iken H1 dışı geçiş reddedilir; karışık oturumda H2 geçişi reddedilir ve kilit açık kalır; yan dosya `.tmp`/`.bad`; karşı ırka posta, parti ve fısıltı reddedilir; H6'da karşı ırka hasar var, aynı ırka yok | Hayır |
| Mutasyonlar | `test-muhendisi` | Irk kapısı silinince, `Race == None` kontrolü silinince, H6 aynı-ırk hasar kontrolü silinince, posta ırk kontrolü silinince ve paket-var kontrolü silinince ilgili test **kalmalı** | Hayır |
| Geçiş | Yeni headless `TestZoneTravel` varyantı H1→H2→H1 | Seviye, altın, görev ve ırk korunur | Hayır |
| Güvenlik | `ag-guvenlik-denetcisi` | `ServerChooseRace`, pazar satın alma, posta ırk kontrolü | Evet (okuma) |

---

## 6. H6 PvP haritası: karar listesi

| # | Karar | Seçenekler | Öneri `[PLACEHOLDER]` |
|---|---|---|---|
| K1 | Giriş seviyesi | 25 / 35 / 59 | **Lv35**: iki ırkın krallık ortasındaki oyuncuları buluşur |
| K2 | PvE mob bandı | Tek bant / karışık / oyuncu seviyesine göre | **Lv40-70 karışık, 4-6 cep + 1 elit** (mevcut arketipler: Dusk, Winter, Frostbound, Rift, Ash). Elit iki kampa eşit uzaklıkta, çekişme noktası |
| K3 | Irk düşmanlığı | Yalnız karşı ırk / herkes herkese | Yalnız karşı ırk. Aynı ırka hasar yok, aynı ırk parti kurabilir |
| K4 | PvP ölüm cezası | XP kaybı var / yok | **Oyuncu öldürünce XP kaybı yok.** Mob öldürünce bugünkü ceza. `ApplyDeathPenalty` öldüren türüne göre ayrılır (`MMOCharacter.cpp:1891`) |
| K5 | Ganimet | Eşya düşer / altın düşer / hiçbir şey | **Hiçbir şey** (alfa). Eşya düşmesi ikileme ve yas yönetimi riski taşır |
| K6 | PvP ödülü | Ulus puanı / başarım / altın | **Yalnız başarım sayacı** (mevcut `UMMOAchievementComponent`'e "karşı ırktan N öldürme"). Altın ödülü işbirlikçi öldürme sömürüsüne açık (iki arkadaş birbirini kesip altın basar). **Değmez** |
| K7 | Yeniden doğuş | Kendi kampı | Kendi ırkının kampı (etiketli PlayerStart) + mevcut doğuş koruması (`OnRep_SpawnProtected`) |
| K8 | Kamp güvenliği | Kamp güvenli bölge mi | Evet, iki kamp güvenli bölge. Bugünkü PlayerStart tabanlı güvenli bölge iki PlayerStart'la kendiliğinden çalışır [Muhtemel, `NETWORK_AUDIT.md` D10] |
| K9 | Global `bAllowPvP` | Kalsın / kalksın | Test anahtarı olarak kalır. Bölge kuralı önceliklidir |
| K10 | Kamp başında bekleme (spawn camping) | Önlem / yok | Alfada gözlem. Kamp güvenli bölgesi yeterli olabilir |

---

## 7. Kapsam: yalnız risk notu

Kullanıcı kararı (2026-10-12): **6 harita + ırk sistemi + pazar aynen uygulanır, kapsam küçültülmez, gerekirse süre uzar.**
Kalan riskler:
- **Tek oturum tek harita (§1.4):** alfada iki ırk aynı anda farklı haritada olamaz. Bu, kapsam değil mimari sınır.
  Kaldırmak harita-başına-sunucu ister (kaynak motor, dedicated target, paylaşılan depo, K-1 kimlik). Alfa sonrası, **aylar** [Tahmin].
- **Arazi tekrarı:** Winter arazisi 4 haritada (H2-H5), Summer 2'de (H1, H6). Özgün arazi sonradan harita harita yapılabilir
  (harita başına +3-6 gün [Tahmin]). Bölge tablosu değişmez, yalnız `MapAsset` ve o haritanın cep koordinatları yeniden ölçülür.
- **İçerik ikizliği:** İnsan/Ork haritaları aynı mob ve görevi taşır (P1). Irka özgü fark, NPC/metin/giydirme ile sınırlı kalır.

---

## 8. Riskler ve açık sorular (kullanıcı kararları)

| # | Soru / risk | Öneri |
|---|---|---|
| S1 | **"Oturum ırkı" kuralı alfa için kabul mü (§1.4)?** Değilse harita-başına-sunucu gerekir (§7, aylar) | Kabul. Alfa oturumları ırka göre planlanır, aynı makine host olur |
| S2 | Kopya + giydirme (Winter 4 haritada, Summer 2'de) kabul mü? | Kabul, E'ye harita harita geçilir |
| S3 | Irk ana kayıtta değil yan dosyada (`MMOSaveGame.h` korunan yol). Kayıp/bozulma = yeniden seçim riski. Kabul mü, yoksa `MMOSaveGame.h`'e bir alan izni mi? | Yan dosya + `.bad` durumunda kilit |
| S4 | H1'de karşı ırkla takas ve pazar serbest mi? | Serbest (ortak pazar = merkez haritanın anlamı). Parti/klan/posta yine aynı ırk |
| S5 | Pazar kurma alfada şart mı, ertelenebilir mi? | Kullanıcı "pazar kurma" dedi, plan içerir. Erteleme yalnız onayla |
| S6 | İçerik P1 (aynı mob/görev) mi, P2 (iki metin sürümü) mi? | P1, ardından P2 metin işi. P3 değmez |
| S7 | Ork oyuncunun Ork mob kesmesi: "dönek ork" fraksiyonu mu, mob görünen adlarını değiştirmek mi? | Fraksiyon (yalnız metin) |
| S8 | Seviye kapısı zorlayıcı (oturumdaki herkes) mı, uyarı mı? | Irk kapısı zorlayıcı (gereksinim). Seviye kapısı alfada zorlayıcı, `[PLACEHOLDER]` 25/59 |
| S9 | H6 kararları K1-K10 (§6) | Tablodaki öneriler |
| S10 | Eski karakterler (ırksız) ilk girişte seçer. Mevcut test karakterlerinin silinmesi gerekmez | Kabul |
| S11 | 4 yeni `.umap` (~9-18 MB her biri, LFS yok) → repo ~50-70 MB büyür | Kullanıcı kararı: LFS ya da doğrudan |
| R1 | O6 penceresi: geçiş sıklığı artar (`NETWORK_AUDIT.md:303-331`) | Yerel mağazada 0 kare. Bulut mağazadan önce O6 tasarımı |
| R2 | `ServerTravel` true, harita yüklenmedi → kilit kalıcı (`NETWORK_AUDIT.md:325`). 6 haritada yanlış yol olasılığı artar | Paket-var kontrolü (K1), `MapsToCook` 6 satır |
| R3 | Seamless travel'da parti/klan sürekliliği doğrulanmadı (`NETWORK_AUDIT.md:301`) | PIE iki oyunculu zincir testi |
| R4 | Codex'in parti/takas dosyalarına ırk kontrolü | Codex'le sıra veya devretme |
| R5 | Boş `TransitionMap` (`DefaultEngine.ini:7`) paketli yapıda | Paketli yapıda bir geçiş testi |

---

## 9. Aşamalı yol haritası (asıl çıktı)

### 9.0 Kurallar

- **Kapı:** her aşama, **iki pencereli PIE** (Editor Preferences > Play: *Number of Players = 2*, *Net Mode = Play As Listen Server*)
  ile kısa bir kabul listesini geçmeden kapanmaz. Kapanmayan aşamadan sonrakine geçilmez. Sonuç geçti/kaldı şablonuyla
  `PLAYTEST_CHECKLIST.md` sonuna yazılır (yalnız ekleme).
- **Her aşama sonunda oyun uçtan uca oynanabilir:** yeni karakter Lv1'den başlar, mevcut en yüksek içeriğe kadar bir yol vardır. **Eski kayıtlar yüklenir:**
  her aşamada eski bir Summer ve bir Winter kaydıyla giriş yapılır, seviye/altın/envanter/görev korunur.
- **Mevcut Summer/Winter bozulmaz:** cep silmek ancak içeriğin yeni yeri kendi kapısını geçtikten sonra yapılır (önce ekle, sonra taşı, en son sil).
- Ajan sırası CLAUDE.md'deki gibidir. Derleyen ajanlar sırayla çalışır. Her kod aşamasında `qa-kontrolcu`, RPC/kayıt/altın/kimlik
  değiştiyse paralel olarak `ag-guvenlik-denetcisi` çalışır.
- Üç iş yeri: **Bulut** = belge, Lore/metin, Python betikleri ve negatif testleri, derlenmemiş C++ taslak PR'ı (`CLOUD_TASKS.md`: derleme yok, `.umap` yok).
  **Yerel oturum** = derleme, headless testler (Smoke/Soak/Probe/NetSmoke, native self-test), `Config/` ve harita commit'i.
  **Kullanıcı (editör)** = harita kopyalama, giydirme, PlayerStart, PIE oynama.
- Süreler tek geliştirici ajan zinciri + kullanıcının editör/PIE zamanı içindir. Hepsi **[Tahmin]**. PIE'de kalan bir kapı aşamaya
  +0,5-1 hafta ekler.

### 9.1 Sıranın gerekçesi

1. **Önce taban (Aşama 0):** Summer/Winter ve Summer↔Winter geçişi PIE'de hiç doğrulanmadı (`PROGRESSION_PLAN.md`, `PLAYTEST_PRIORITY.md`).
   Bozuk bir tabanın üstüne 4 harita kurmak, hatanın kaynağını belirsizleştirir.
2. **Davranışı değiştirmeyen altyapı önce (Aşama 1):** yerleşim tablosu, sonraki her harita için tek eklenti noktasıdır. Eşdeğerlik ölçülebilir
   (aynı 18/17 spawner).
3. **Irk karar ve veri modeli, harita işinden önce (Aşama 2):** geçiş kapısı, PvP ve sosyal kurallar ırk alanına bağlı. Kayıt biçimi (yan dosya, K-1)
   sonradan değişirse göç gerekir.
4. **Önce tek ırk (İnsan) uçtan uca (Aşama 3-5):** mevcut Lore İnsan-merkezli (Akçakule, Ravenmoor). Winter_scene İnsan krallığı olur.
   Mevcut tüm içerik yalnız yeniden yerleşimle tek ırk için Lv1-80 oynanabilir kalır. Ork seçimi bu sırada **kapalıdır** (sunucu reddeder):
   açık olsaydı Ork karakter Lv25'te kapıda kalırdı.
5. **Ork + sosyal kurallar birlikte (Aşama 6):** iki ırk H1'de ilk kez bir araya geldiğinde parti/posta/sohbet kuralları hazır olmalı.
6. **PvP (Aşama 7) iki ırktan sonra:** karşı ırk yoksa PvP kuralı sınanamaz.
7. **Pazar (Aşama 8) bağımsız:** yalnız ırk alanına (S4: karşı ırkla ticaret) bağlı. Öncelik değişirse Aşama 3'ten sonraya çekilebilir.
8. **Sertleştirme (Aşama 9):** altı haritanın birlikte soak'u, paketli yapı, belgeler.

### 9.2 Aşamalar

#### Aşama 0. Taban: yerelde derle + mevcut Summer/Winter PIE doğrulaması
- **Amaç:** bugünkü iki haritanın ve geçişin gerçekten çalıştığını kanıtlamak. Plan bu tabana dayanıyor.
- **Kapsam:** kod değişikliği yok. `PLAYTEST_RUNBOOK.md` akışı: derleme → `PR1_VERIFY` 1-18 → `FIRST_PLAYTEST` ilk 7 test → `[MMOMETRIC]`.
- **Bulut:** bu aşamanın kabul listesini `PLAYTEST_CHECKLIST.md` biçiminde hazırlamak.
- **Yerel oturum:** `Build.bat`, `RunAllChecks.ps1` (6 `TestZoneTravel*` dahil), `NetSmokeTest.ps1` her iki harita.
- **Kullanıcı (editör):** iki pencereli PIE.
- **Bağımlılık:** yok.
- **Kabul (PIE):** (1) iki pencerede Summer'da doğuş, 5 NPC tıklanıyor; (2) iki oyuncu mob kesiyor, görev sayacı artıyor, parti XP paylaşıyor;
  (3) host "Tüm oyuncuları Kış'a taşı" → iki oyuncu Winter'da, seviye/altın/envanter/görev aynı; (4) Winter'da 4 NPC ve mob; (5) Yaz'a dönüş;
  (6) oyun kapatılıp açılınca iki karakter son haritada yükleniyor; (7) parti geçişten sonra var mı, not (R3).
- **Süre:** 0,5-1 hafta [Tahmin].
- **Karar:** yok.

#### Aşama 1. Yerleşim tablosu (davranış eşdeğer)
- **Amaç:** harita eklemeyi tek tablo satırına indirmek. Oyuncu farkı yok.
- **Kapsam:** yeni `World/MMOZoneLayout.{h,cpp}` (`MakeZone`/`MakePocket`/`MakeNpc`), `SpawnMobArea` tek çağrıya iner. Yalnız Summer ve Winter satırları.
  `ValidateData.py`/`PocketSpacing.py` yeni kalıbı okur. Bunların negatif testleri güncellenir.
- **Bulut:** Python betik değişiklikleri ve negatif testleri. İsteğe bağlı derlenmemiş C++ taslağı.
- **Yerel oturum:** C++ taşıma, derleme, Smoke iki harita, eşdeğerlik diff'i.
- **Kullanıcı:** PIE.
- **Bağımlılık:** Aşama 0.
- **Kabul:** headless: `[MMOLAYOUT]` Summer **18**, Winter **17** satır. Koordinat, bant ve MaxMobs refaktör öncesiyle bire bir. NPC 5/4. `RunAllChecks` yeşil.
  PIE: Aşama 0 listesinin 1-5. maddeleri aynen geçer.
- **Süre:** 1 hafta [Tahmin].
- **Karar:** yok.

#### Aşama 2. Irk: karar + veri modeli + seçim (iki mevcut haritada)
- **Amaç:** her karakterin değişmez bir ırkı olsun, kalıcı ve replike. Henüz harita veya kural etkisi yok.
- **Kapsam:** `EMMORace`, `UMMORaceComponent`, yan dosya `Saved/MMORace/<SlotKey>.json` (§3.1 I1-I6), `ServerChooseRace`, HUD seçim paneli.
  Ork **seçilebilir değil** (panelde "yakında"; sunucu `Orc` isteğini reddeder). Irk ad etiketinde görünür. `Scripts/TestRace.ps1` (çekirdek senaryolar).
- **Bulut:** veri modeli ve K-1 etki notu (`ACCOUNT_IDENTITY_DESIGN.md`'ye ek), test senaryo listesi.
- **Yerel oturum:** kod, `TestRace`, mutasyonlar (`None` kontrolü, ikinci seçim reddi), NetSmoke (istemci ırkı görüyor).
- **Kullanıcı:** PIE.
- **Bağımlılık:** Aşama 1 (yerleşim değil ama aynı sıcak dosya sırası).
- **Kabul (PIE):** (1) yeni karakterde panel çıkıyor, İnsan seçiliyor, panel kapanıyor; (2) öbür pencerede ırk etiketi görünüyor; (3) yeniden bağlanınca ırk aynı,
  panel yok; (4) **eski kayıt** (ırksız) yükleniyor, panel bir kez çıkıyor, seviye/altın/envanter korunuyor; (5) Summer↔Winter geçişinden sonra ırk aynı.
- **Süre:** 1-1,5 hafta [Tahmin].
- **Kararlar (bu aşamadan önce):** S3 (yan dosya mı, `MMOSaveGame.h` alanı mı), S10 (eski karakterler seçer), ırkın değiştirilemezliği.

#### Aşama 3. Bölge tablosu ve kapılı geçiş (hâlâ 2 harita)
- **Amaç:** N-haritalı geçiş: hedef menüsü, ırk ve seviye kapısı, paket kontrolü. Summer = H1 rolü, Winter = geçici İnsan krallığı rolü.
- **Kapsam:** `RequestZoneTravel` hedef aşırı yüklemesi, `MMOTravel <Bolge>`, HUD hedef listesi, kapılar (§2.6), `FPackageName::DoesPackageExist`.
  Argümansız eski yol yalnız `!UE_BUILD_SHIPPING` test yolu olarak kapısız kalır, böylece 6 `TestZoneTravel*` değişmez.
  Yeni self-test `MMOTestZoneTable`. İçerik ve cep değişmez.
- **Bulut:** kapı kuralı tablosu, self-test senaryoları.
- **Yerel oturum:** kod, self-test, mutasyonlar (ırk kapısı, paket kontrolü, admin kapısı).
- **Kullanıcı:** PIE.
- **Bağımlılık:** Aşama 2.
- **Kabul (PIE):** (1) host menüsünde Summer ve Winter görünüyor, uzak pencerede menü yok; (2) Lv<25 oyuncu varken Winter'a geçiş reddediliyor, neden
  hostta görünüyor; (3) iki İnsan Lv25+ (test komutuyla seviye) geçebiliyor; (4) ırk seçmemiş oyuncu varken ret; (5) reddedilen istekten sonra
  ikinci istek kabul ediliyor (kilit açık); (6) eski kayıtla giriş korunuyor.
  **Not:** Winter içeriği bu aşamada hâlâ Lv42-80. Lv25-41 oyuncusu Summer'daki 25-40 ceplerinde kalır, oyun uçtan uca oynanabilir kalır.
- **Süre:** 1 hafta [Tahmin].
- **Kararlar:** S1 ("oturum ırkı" kuralı), S8 (seviye kapısı zorlayıcı mı, 25/59 değerleri).

#### Aşama 4. H4 İnsan üst bölge (`HumanHigh_scene`), ilk yeni harita
- **Amaç:** ilk yeni haritayı mevcut haritalara dokunmadan eklemek. Lv59-80 İnsan içeriği burada.
- **Kapsam:** Winter kopyası, 13 cep (Kül/Uçurum B-konumları aynen + Yarık 59-64 ve Haberci A-konumlarında), 4 NPC (+ isteğe bağlı konuşan).
  Winter ve Summer'daki eski cepler **yerinde kalır** (geçici ikizlik serbest; `ValidateData` ikizliği uyarı sayar, hata değil).
- **Bulut:** giydirme prompt'ları (`ASSET_PROMPTS.md` §9'a yeni bölüm), bölge adı ve NPC replikleri (`metin-yazari`), genelleştirilmiş navmesh
  Python betiği (harita yolu argüman; `ExtendSummerNavigation.py` deseni). Çalıştırma yerelde.
- **Yerel oturum:** tablo satırları, `MapsToCook` (Config), navmesh betiği, Smoke/Probe/Soak, harita commit'i.
- **Kullanıcı (editör):** `Winter_scene` → `Maps/HumanHigh_scene/HumanHigh_scene` kopyası, giydirme (ışık, sis, post-process, malzeme, prop; PCG dışlama:
  her cep merkezinde yarıçap + 450 u boş), tek PlayerStart kontrolü, PIE.
- **Bağımlılık:** Aşama 3.
- **Kabul:** headless: 13 cebin her biri probe ≥ 60/64, Soak 90 sn takılma ≤ 1, ilk doldurma ≥ 4/6, Smoke 0 hata.
  PIE: (1) iki İnsan Lv59+ Winter→HumanHigh geçiyor; (2) 4 NPC çalışıyor; (3) Lv66 ve Lv72 cebinde mob kesiliyor, görev sayacı artıyor;
  (4) HumanHigh→Summer dönüşünde kayıt korunuyor; (5) kapatıp açınca karakter HumanHigh'da yükleniyor.
- **Süre:** 1-1,5 hafta [Tahmin] (giydirme kalitesine göre kullanıcı zamanı artar).
- **Kararlar:** S2 (kopya + giydirme), S11 (LFS ya da doğrudan commit), harita görünen adları.

#### Aşama 5. İnsan yolu tamam: Winter = Ravenmoor (Lv25-58), Summer = merkez (Lv1-24)
- **Amaç:** İnsan için 3 haritada Lv1-80 uçtan uca, kopya içerik yok.
- **Kapsam:** Winter yerleşimi §2.2 H2 (A-konumları 42-57 yerinde, B-konumlarına 25-41 + 58). Summer'dan 7 cep çıkar (11 kalır). Winter'dan
  66-80 çıkar (artık HumanHigh'da). Winter İnsan giydirmesi (isteğe bağlı, bu aşamanın kapısı değil). Winter için rota açıklığı ölçümü
  (`ProbeThawRoute.py`'nin Winter eşdeğeri; Lv25 oyuncusu Lv42+ cebin zarfından geçmemeli).
- **Bulut:** rota ölçüm betiği (Python, editör içinde çalışır), `CONTENT_MAP`/`ASSET_PROMPTS`/`PLAYTEST_LV46_80.md` taslakları.
- **Yerel oturum:** tablo değişiklikleri, probe (8 yeni bant-konum ataması), Soak iki harita, `ContentMap.py --write`, `DocCounts`.
- **Kullanıcı:** PIE (gerekirse Winter giydirmesi).
- **Bağımlılık:** Aşama 4 (66-80 yeni yerinde doğrulanmış olmalı).
- **Kabul:** headless: Summer 11, Winter 16, HumanHigh 13 spawner. `ValidateData` kural (3)-(4) İnsan için hata yok. Probe ≥ 60/64. Takılma ≤ 1.
  PIE: (1) Lv24→25 olan oyuncu Summer'dan Ravenmoor'a geçiyor; (2) Lv25 Obsidyen ve Lv31 Alacakaranlık cebi Ravenmoor'da; (3) Lv58 Arayıcı cebinde
  Lv59'a çıkış, HumanHigh'a geçiş; (4) **eski kayıt**: Summer'da Lv30 konumunda kaydedilmiş karakter yükleniyor (boş arazide, yürüyebiliyor) ve
  Ravenmoor'a geçebiliyor; (5) Summer'da Lv25+ mob görünmüyor.
- **Süre:** 1 hafta [Tahmin].
- **Kararlar:** §2.2 bant eşlemesinin onayı, S6 (içerik P1).
- **Bu aşama sonunda:** yalnız İnsan için 6 haritanın 3'ü ve Lv1-80 oynanabilir.

#### Aşama 6. Ork: iki harita + ırk sosyal kuralları + metin
- **Amaç:** iki ırk. H1 ortak, H3/H5 Ork haritaları, karşı ırk kuralları.
- **Kapsam:** `OrcLand_scene` (H3) ve `OrcHigh_scene` (H5): Winter kopyaları, Ravenmoor/HumanHigh ile aynı cep satırları (ayna). Ork seçimi açılır.
  Sosyal kurallar §3.2 (parti, klan, posta, fısıltı/sohbet kanalı, arkadaş, `/who`; H1 takası S4'e göre). Codex'in parti/takas dosyaları sırayla.
  `ValidateData` kural (5) ayna eşitliği. Lore: dönek ork fraksiyonu, Ork yurdu adı, ırk-nötr görev metinleri (P1).
- **Bulut:** Lore ve diyalog (`metin-yazari`; `GenQuestDialogue.py --check` yerelde), giydirme prompt'ları (Ork mimarisi), `ValidateData` kural (5) ve negatif testi.
- **Yerel oturum:** tablo, sosyal kural kodu, `TestRace` sosyal senaryoları, mutasyonlar, `TestMail`/`TestTrade*`/`TestPartyConsent` gerilemesi, 2 harita probe/soak.
- **Kullanıcı (editör):** 2 kopya + Ork giydirmesi, PIE.
- **Bağımlılık:** Aşama 5.
- **Kabul:** headless: H3 16, H5 13 cep, probe/soak ölçütleri, ayna kuralı yeşil.
  PIE: (1) bir pencere İnsan, bir pencere Ork; Summer'da ikisi de doğuyor, aynı NPC'leri kullanıyor; (2) karşı ırka parti daveti, posta ve fısıltı reddediliyor;
  (3) karışık oturumda Ravenmoor ve OrcLand'e geçiş reddediliyor; (4) iki Ork oturumu Summer→OrcLand→OrcHigh→Summer, kayıt korunuyor;
  (5) Summer'da karşı ırkla takas S4 kararına uygun.
- **Süre:** 2-3 hafta [Tahmin].
- **Kararlar:** S4 (H1'de karşı ırkla takas/pazar), S7 (dönek ork), Ork yurdu adı.

#### Aşama 7. H6 PvP haritası
- **Amaç:** iki ırkın savaştığı ve PvE yapabildiği ortak harita.
- **Kapsam:** `PvP_scene` (Summer kopyası), ırk etiketli iki PlayerStart (`ChoosePlayerStart` override; `StaticLint` whitelist), iki kamp (kamp başına
  satıcı/görevli/şifacı, ırka bağlı hizmet), 4-6 PvE cebi + 1 elit (K2), bölge PvP kuralı (`bPvPZone` + karşı ırk), PvP ölümünde XP kaybı yok (K4),
  ganimet yok (K5), başarım sayacı (K6).
- **Bulut:** K1-K10 karar notu, başarım satırı (`MakeAch`) ve metin, `ValidateData` kuralı (H6'da iki kamp NPC seti).
- **Yerel oturum:** kod, `TestRace` PvP senaryoları, mutasyon (aynı ırk hasarı, PvP ölüm cezası), probe/soak, `TestAchievements` gerilemesi.
- **Kullanıcı (editör):** kopya + giydirme, iki kamp alanı + iki PlayerStart (etiketli), PIE.
- **Bağımlılık:** Aşama 6.
- **Kabul (PIE):** (1) İnsan ve Ork aynı oturumda PvP'ye geçiyor, her biri kendi kampında doğuyor; (2) kampta saldırı yok; (3) kamp dışında karşı ırka hasar var,
  aynı ırka (ikinci İnsan penceresiyle) yok; (4) PvP'de ölen XP kaybetmiyor, mobla ölen kaybediyor; (5) kendi kampında yeniden doğuyor;
  (6) PvE cebinde mob kesiliyor; (7) diğer haritalarda oyuncu-oyuncu hasarı yok.
- **Süre:** 1,5-2 hafta [Tahmin].
- **Kararlar:** S9 (K1-K10).

#### Aşama 8. Pazar kurma (H1)
- **Amaç:** merkez haritada oyuncu tezgâhı.
- **Kapsam:** `UMMOMerchantComponent` (§3.3), tezgâh UI, eşya kilidi (tezgâhtaki eşya takas/posta/satıcıya gidemez), satış sonrası iki taraf kaydı, vergi sink'i,
  `EconomyModel.py` varsayımı, `[MMOMETRIC] market_sale`.
- **Bulut:** ekonomi modeli (`ekonomi-tasarimcisi`: vergi oranı, slot sayısı), test senaryoları, metin.
- **Yerel oturum:** kod, native test (ikileme senaryoları), mutasyonlar (eşya kilidi, sunucu fiyat doğrulaması), güvenlik denetimi.
- **Kullanıcı:** PIE (pazar alanı için prop isteğe bağlı).
- **Bağımlılık:** Aşama 2 (ırk), Aşama 6 (S4 kararı uygulanmış).
- **Kabul (PIE):** (1) A tezgâh açıyor, B görüyor; (2) B satın alıyor, altın/eşya doğru, vergi düşülmüş; (3) tezgâhtaki eşya takasa/postaya konamıyor;
  (4) A bağlantıyı kesince tezgâh kapanıyor, eşya A'da; (5) kapatıp açınca iki kayıt doğru.
- **Süre:** 1,5-2 hafta [Tahmin].
- **Kararlar:** S5 detayları (slot sayısı, vergi, H1'de hangi alan).

#### Aşama 9. Alfa sertleştirme
- **Amaç:** 6 haritanın birlikte, paketli yapıda, kayıp olmadan çalışması.
- **Kapsam:** `RunAllChecks` 6 harita döngüsü, NetSmoke 6 harita, Soak 6 harita, paketli yapı (`MapsToCook` 6, `TransitionMap` R5), `PLAYTEST_RUNBOOK`
  alfa sürümü, `CLAUDE.md` harita listesi, `[MMOMETRIC]` `zone/race` alanları.
- **Bulut:** belgeler, runbook, metrik ayrıştırıcı (`PlaytestMetrics.py`) ve fikstür testi.
- **Yerel oturum:** tüm kontroller, paketli yapı.
- **Kullanıcı:** paketli yapıyla 2 makinede (host + istemci) oturum.
- **Bağımlılık:** Aşama 7 ve 8.
- **Kabul:** `RunAllChecks` yeşil (6 harita). Paketli yapıda 2 makinede: İnsan zinciri H1→H2→H4→H1, Ork zinciri H1→H3→H5→H1, karışık H1→H6→H1;
  her geçişten sonra kayıt korunuyor, hata satırı 0.
- **Süre:** 1 hafta [Tahmin].

### 9.3 Kararların zamanlaması

| Karar | En geç bu aşamadan önce |
|---|---|
| S3 ırk kaydı yeri, S10 eski karakterler, ırk değiştirilemez | Aşama 2 |
| S1 oturum ırkı kuralı, S8 seviye kapısı | Aşama 3 |
| S2 kopya + giydirme, S11 LFS, harita adları | Aşama 4 |
| §2.2 bant eşlemesi, S6 içerik P1 | Aşama 5 |
| S4 karşı ırkla ticaret, S7 dönek ork, Ork yurdu adı | Aşama 6 |
| S9 PvP K1-K10 | Aşama 7 |
| S5 pazar ayrıntıları | Aşama 8 |

### 9.4 Toplam süre ve alfa noktası (dürüst)

| Aşama | Süre [Tahmin] | Kümülatif |
|---|---|---|
| 0 Taban | 0,5-1 hf | 0,5-1 |
| 1 Yerleşim tablosu | 1 hf | 1,5-2 |
| 2 Irk çekirdeği | 1-1,5 hf | 2,5-3,5 |
| 3 Kapılı geçiş | 1 hf | 3,5-4,5 |
| 4 HumanHigh | 1-1,5 hf | 4,5-6 |
| 5 İnsan yolu tamam | 1 hf | 5,5-7 |
| 6 Ork + sosyal | 2-3 hf | 7,5-10 |
| 7 PvP | 1,5-2 hf | 9-12 |
| 8 Pazar | 1,5-2 hf | 10,5-14 |
| 9 Sertleştirme | 1 hf | **11,5-15 hafta** |

Kapıda kalma payıyla gerçekçi aralık **~12-16 hafta [Tahmin]**.

- **Kullanıcının tanımladığı kapalı alfa (6 harita, iki ırk, PvP, pazar): Aşama 9 sonunda.** Daha erken değil, çünkü H6 ve pazar gereksinimin parçası.
- **Aşama 5 sonunda (~6-7 hafta):** yalnız İnsan için 3 haritada Lv1-80 oynanabilir. Bu bir **iç test** noktasıdır, alfa değil. Gereksinimin
  "6 harita" ve "iki ırk" koşulları sağlanmaz.
- **Aşama 7 sonunda (~9-12 hafta):** 6 haritanın hepsi ve PvP var, pazar yok. Kullanıcı pazarı erteleme kararı verirse (S5) alfa buraya çekilebilir. Kararsız kalırsa çekilmez.
- **Alfa sonrasına kalan, bu plan çözmez:** iki ırkın aynı anda farklı haritada olması (§1.4), tek host makinesi zorunluluğu (yerel kayıt), K-1 kimlik açığı
  (ad = yuva). Kapalı alfa güvenilir küçük bir grupla bu sınırlarla yapılabilir. Açık test için bu üçü önce çözülmeli.

---

## 10. Ekonomi / denge etkisi

- **Altın kaynağı eklenmez.** P1 içerikte aynı moblar iki haritada doğar ama oyuncu yalnız birinde oynar. Kişi başı gelir değişmez.
- **Yeni sink:** pazar vergisi %2-3 `[PLACEHOLDER]`. Model: `EconomyModel.py`'ye pazar hacmi varsayımı. Ölçüm: `[MMOMETRIC] market_sale`.
- **XP hızı [Tahmin]:** ırk haritalarında cepler hub'a 3-7 bin u (bugünkü Winter mesafeleri). Yürüme bugünkünden kısa değil.
  H1'de Lv1-24 cep yoğunluğu aynı. Lv80 toplam süresi (~11 saat, `TIMELINE_MODEL.md`) değişmez. Bu kısa süre KO tarzı için ayrı bir karar.
- **PvP'de ekonomi sızıntısı yok** (K5/K6: ganimet ve altın ödülü yok).
- **Ölçüm:** `[MMOMETRIC]` `levelup` satırına `zone` ve `race` alanı, `zone_travel from/to/players/ok` olayı, H6'da `pvp_kill` sayımı.
