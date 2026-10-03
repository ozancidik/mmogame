# İlerleme planı: 1–80. seviyeler

**Durum:** Tasarım planı, 2026-09-28. `[IMPLEMENTED]` mevcut C++ kaynak kodunda uygulanmış olanı, `[PROPOSED]` önerilen işi, `[PLACEHOLDER]` ise henüz oynanışta sınanmamış geçici denge hedefini belirtir. Kaynak koddaki eski denge çalışmalarına ilişkin yorumlar geçmişe dair bağlam sağlar; güncel oynanış testi sonucu sayılmaz. Bu belge kaynak kod incelemesine dayanır. Haritada erişilebilirlik, dövüş süresi ve ekonomi hissi PIE içinde ayrıca gözlemlenmelidir.

## Oyuncu hedefi ve öncelikler

Oyuncu her yeni seviye aralığında ulaşabileceği bir düşman, tamamlayabileceği bir görev ve anlamlı bir ekipman veya beceri kararı görmeli. Summer_scene döngüsü 40. seviye boss'una kadar kodlandı; PIE oynanışı bekliyor. Winter_scene için 41–50. seviye mob/görev/ekipman tanımları, navmesh ve ayrı spawner/hub yerleşimi eklendi; host komutuyla çok oyunculu geçiş prototipi derleniyor ama PIE oynanışı bekliyor. 51–80. seviye içerikleri eksik. Çalışan erken oyun korunarak döngü, küçük ve ölçülebilir aralıklarla genişletilmeli.

| Öncelik | Eksik ve oyuncuya etkisi | Kaynak kanıtı |
| --- | --- | --- |
| P0 | Lv41–50 Winter spawner'ları statik nav yolu ölçülerek eklendi ve host geçiş prototipi yazıldı, fakat PIE doğrulaması yok; Lv51–80 düşmanları da tanımlı değil. Lv25–40 alanlarının PIE doğrulaması bekliyor. | `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`; `Source/MyProject/Private/Core/MMOGameMode.cpp`. |
| P0 | Ana `KillSpecificMob` zinciri `Q_WinterSovereign` ile bitiyor. Winter görevlerinin oynanarak tamamlanması ve haritalar arası geçiş doğrulanmadı; Lv51–80 dövüş görevleri eksik. | `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`; `Source/MyProject/Private/Components/MMOQuestComponent.cpp`. |
| P0 | En yüksek temel ekipman gereksinimi Lv42 (dört Kış parçası). Lv43–80 arasında yeni ekipman kademesi yok. | `Source/MyProject/Private/Items/MMOItemDatabase.cpp`. |
| P1, kodda düzeltildi | Dolu ekipman çantasında görev ödülü kayboluyordu. `117e5fc` ile teslim öncesi kapasite kontrolü eklendi; görev talep edilebilir kalıyor. PIE doğrulaması bekliyor. | `Source/MyProject/Private/Components/MMOQuestComponent.cpp` (`ClaimReward`). |
| P1, kodda düzeltildi | Boş seviye aralığı başka seviyeden rastgele mob üretiyordu. `117e5fc` ile spawner doğumu atlıyor ve hata kaydı bırakıyor. PIE doğrulaması bekliyor. | `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`; `Source/MyProject/Private/Core/MMOMobSpawner.cpp`. |
| P1 | Beceri tanımları 80. seviyeye kadar uzanıyor; fakat 41–80. seviye beceri açılışlarını anlamlı kılacak uygun düşman ve ekipman ilerlemesi yok. | `Source/MyProject/Private/Skills/MMOSkillDatabase.cpp`; `Source/MyProject/Private/Components/MMOSkillComponent.cpp`. |

## Mevcut ilerleme envanteri `[IMPLEMENTED]`

| Sistem | Kaynak kodundaki durum |
| --- | --- |
| Seviye ve statlar | `Level=1`, `MaxLevel=80`; her seviye 3 stat puanı ve 2 beceri puanı veriyor. Can formülü `50 + Vitality × 8 + Level × 12`; mana formülü `25 + Intelligence × 5 + Level × 5.5`. Ölüm, mevcut birikmiş XP'den en fazla o seviyenin XP eşiğinin %10'u kadar düşürüyor. `GainExperience`, 80. seviyede yeni XP kabul etmiyor ve XP'yi sıfırlıyor. Kaynaklar: `Source/MyProject/Public/Components/MMOStatsComponent.h`, `Source/MyProject/Private/Components/MMOStatsComponent.cpp`. |
| Moblar ve dünya | 36 arketip var: 27 normal (41–50 için dört Kış normali eklendi) ve 9 elit (en yükseği Lv50 Kış Hükümdarı). Kış normalleri ve eliti Winter_scene'in batıdaki tam nav yolu ölçülmüş ceplerinde spawner'a bağlandı; gerçek PIE oynanışı bekliyor. Obsidyen ve Alacakaranlık spawner'ları Summer_scene zemin/navmesh ölçümüyle yerleştirildi; gerçek PIE oynanışı bekliyor. Sunucudaki ölüm akışı, mobun tam XP ödülünü öldüren oyuncuya ve yakındaki uygun her parti üyesine veriyor; oyuncu/mob seviye farkına göre ödül ayarı yok. Kaynaklar: `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`, `Source/MyProject/Private/Characters/MMOMob.cpp`, `Source/MyProject/Private/Core/MMOGameMode.cpp`. |
| Görevler | 50 tanım var. Ana öldürme zinciri `Q_Goblins` ile başlıyor ve `Q_WinterSovereign` (Lv50) ile bitiyor; Winter hedefleri spawner'a bağlandı, fakat bölgeye oyun içi geçiş ve görevlerin PIE tamamlanması açık iş. Bağımsız öldürme görevleri ile tek seferlik seviye/altın kilometre taşları zinciri tamamlıyor. Bütün görevler görev veren NPC'nin yakınında kabul edilmeli ve ödülleri yine orada alınmalı. Kaynaklar: `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`, `Source/MyProject/Private/Components/MMOQuestComponent.cpp`. |
| Ekipman | Silah, zırh, kask ve bot olmak üzere dört yuvada 37 tanım var; en yüksek gereken seviye 42. Kuşanma için gereken seviye kontrol ediliyor. Normal moblarda varsayılan ekipman düşürme şansı %25; seçim havuzu mob seviyesi + 3'e kadar gereken seviyedeki eşyaları içerebiliyor. Elitlerde ekipman şansı zar başına %50; 1 veya 2 kez zar atılıyor ve uygun en yüksek kademe filtresi kullanılıyor. Yükseltme üst sınırı +8; her +1 temel saldırı/savunmaya %20 ekliyor. Altın maliyeti `50 × (mevcut artı + 1)`. Kaynaklar: `Source/MyProject/Private/Items/MMOItemDatabase.cpp`, `Source/MyProject/Private/Characters/MMOMob.cpp`, `Source/MyProject/Private/Components/MMOInventoryComponent.cpp`. |
| Beceriler | 61 tanım var: Warrior 18, Archer 14, Mage 13, Priest 16. Dört sınıfın da 70 ve 80. seviyelerde `Special3` becerileri var. Öğrenmek için hem gereken seviye hem ustalık rütbesi sağlanmalı; ustalığa yatırılan her rütbe 1 beceri puanına mal oluyor ve o rütbedeki birden fazla beceriyi açabiliyor. Bu akışta veri tablosundaki `SkillPointCost`, beceri başına ayrı bir ücret olarak alınmıyor. Örnekler: Warrior `War_WarCyclone` seviye 45 / güç 510, `War_DoomsdayBlow` seviye 80 / güç 1,725 / mana 400; Priest `Pri_HandOfHeaven` seviye 80 / iyileştirme gücü 1,650 / mana 420. Kaynaklar: `Source/MyProject/Private/Skills/MMOSkillDatabase.cpp`, `Source/MyProject/Private/Components/MMOSkillComponent.cpp`. |

**Erken ödüllerin kullanım zamanı:** `[IMPLEMENTED]` `Q_Boss` 5. seviyede açılıyor ama verdiği `GreatSword` için 6. seviye gerekiyor. `Q_CursedKnights` 9. seviyede açılıyor ama verdiği `MithrilBoots` için 10. seviye gerekiyor. Bunlar bir sonraki seviyeye hazırlık ödülleri; görev arayüzü ödül alınmadan önce kuşanma seviyesini göstermeli. Hemen kullanılabilir diye anlatılmamalılar. Kaynaklar: `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`, `Source/MyProject/Private/Items/MMOItemDatabase.cpp`.

## XP eğrisi ve mevcut ödüllerin anlamı

`[IMPLEMENTED]` `ComputeExperienceForLevel(L)`, hesaplamadan sonra `int32`'ye yuvarlanan **L → L+1 için gereken XP** değerini döndürüyor:

```text
L <= 25: round(100 × 1.25^(L-1))
L > 25:  round(100 × 1.25^24 + 4725 × (L-25) + 591 × (L-25)^2)
```

Kaynak kod sonuçları `MAX_int32` ile sınırlıyor. `GainExperience`, 80. seviyede daha fazla XP kabul etmiyor; hesaplanan L80→81 değeri oynanabilir bir gereksinim değil. Kaynak: `Source/MyProject/Private/Components/MMOStatsComponent.cpp` (`ComputeExperienceForLevel`, `GainExperience`).

| Mevcut seviye → sonraki | Gereken XP | En yüksek normal mobla (12,000 XP) teorik öldürme sayısı | En yüksek elitle (19,000 XP) teorik öldürme sayısı |
| --- | ---: | ---: | ---: |
| 1 → 2 | 100 | — | — |
| 24 → 25 | 16,941 | 2 | 1 |
| 25 → 26 | 21,176 | 2 | 2 |
| 26 → 27 | 26,492 | 3 | 2 |
| 30 → 31 | 59,576 | 5 | 4 |
| 40 → 41 | 225,026 | 19 | 12 |
| 50 → 51 | 508,676 | 43 | 27 |
| 65 → 66 | 1,155,776 | 97 | 61 |
| 79 → 80 | 1,999,682 | 167 | 106 |

Öldürme sayıları `ceil(eşik / mobun tabloda yazan XP'si)` ile hesaplandı; seviye uygunluğu, görev XP'si, ölüm cezası, yol, bekleme süresi ve parti etkileri dahil değil. Bunlar ölçülmüş ilerleme hızı değil, ölçeği gösteren hesaplar. 1→80 arasındaki seviye eşiklerinin toplamı **40,153,013 XP**; yalnızca 25→80 arası **40,068,710 XP**. Görev ödülleri hesaba katılmadığında, 1→80 toplam eşiği için yalnızca `DuskOverlord` öldürülen kesintisiz ve gerçekçi olmayan bir senaryoda bile en az **2,114 öldürme** gerekiyor. Asıl eksik, tekrar oynanabilir ve seviyeye uygun içerik. Tek seferlik `Q_Level80` görevinin 90,000 XP ödülü, 79→80 eşiğinin o aşamada alınması halinde **%4.5'i**; oyuncu 80. seviyedeyken ise seviye ilerlemesine katkısı yok. Bu görev şu anda 80'e ulaşma yolu değil, seviye tavanına varış kutlaması ve altın ödülü işlevi görüyor.

`[PROPOSED]` Seviyeye uygun mob XP'si ve görevler eklenirken mevcut XP eğrisi korunsun. Yalnızca kaynak koda bakarak 1–25 formülü değiştirilmesin veya yeni bir XP çarpanı belirlenmesin. Her yeni aralık için `seviye başına öldürme = ceil((seviye XP eşiği − o seviyede beklenen tek seferlik görev XP'si) / ortalama mob XP'si)` hesaplanmalı; oynama süresi ayrıca ölçülen öldürme ve yol sürelerinden modellenmeli. `[PLACEHOLDER]` İlk oynanış testi hedefi: ilgili görev etkinken seviye başına 12–25 normal mob öldürme. Her sınıfla bir solo, ayrıca bir parti koşusu yapıldıktan sonra yeniden değerlendirilmeli. Bu aralık uygulanmış bir ilerleme hızı değil, sınanacak bir varsayım.

## Seviye aralıklarına göre yol haritası `[PROPOSED]`

| Aralık | İçerik kararı | Bağımlılık ve denge kontrolü |
| --- | --- | --- |
| 1–24 | Mevcut arketipleri, görev zincirini, ekipman kademelerini ve beceri koşullarını koru. Yeni tek seferlik ekipman ödüllerinden önce ödül teslimini düzelt. | Erken oyundaki sınıf dövüşlerini ve görev tamamlanmasını PIE içinde ölç; yalnızca gözleme dayanarak ayarla. |
| 25–30 | `[IMPLEMENTED, PIE bekliyor]` Üç normal mob, bir elit, `Q_MagmaTyrant` sonrası dört görev, dört Obsidyen ekipman ve Summer_scene'de iki spawner eklendi. | Zemin/navmesh ve ulaşım headless probla ölçüldü; oyuncu ve yaratık davranışı, görev akışı ve denge PIE'de doğrulanmalı. |
| 31–40 | `[IMPLEMENTED, PIE bekliyor]` Dört normal mob, bir elit, beş görev ve dört Alacakaranlık ekipmanı eklendi. `Q_Level30`/`Q_Level40` paralel kilometre taşları olarak kaldı. | İki spawner alanı headless zemin/navmesh probuyla ölçüldü. 30→31 (59,576 XP) ve 40→41 (225,026 XP) için gerçek öldürme süresi ile boss alanı örtüşmesi PIE'de doğrulanmalı. |
| 41–50 | Mevcut 45. seviye becerilerini anlamlı kılan üçüncü dövüş ve ekipman kademesini, ayrıca 50. seviye karşılaşmasını ekle. | `Winter_scene` aday haritasına PlayerStart ve navmesh sınırı ekleyip oyuncu yolu ile mob doğuşlarını doğrula; ardından çok oyunculu bölge geçişini tasarla. Sadece vuruş hasarını değil, mana giderinin sürdürülebilirliğini de kontrol et. |
| 51–65 | 58 ve 65. seviyeler çevresinde en az iki alt aralıkla devam et; öldürme görevlerini `Q_Level65` ile ilişkilendir. | 50→51 (508,676 XP) ve 65→66 (1,155,776 XP) eşiklerini gerçek görev sıklığı ve oturum süresiyle birlikte modelle. |
| 66–80 | 70 ve 80. seviye son becerilerinin çevresine karşılaşmalar, son ekipman hedefi ve görev zincirine bir sonuç ekle. | `Q_Level80` tamamlanma ödülü olarak kalabilir; XP kısmına işlev kazandır veya yalnızca seviye tavanındaki davranış tasarlandıktan sonra değiştir. |

Her kademe bir tasarım varsayımıdır. `[PLACEHOLDER]` Eşya statları, mob hasarı/canı/XP'si, görev ödülleri, düşen eşyaların fiyatları, bekleme süreleri ve karşılaşma süresi; formül ile ölçülen dövüş kayıtlarına göre seçilmeli ve ancak oynanış testinden sonra dengelenmiş sayılmalı. Yeni eşya ve görev kimlikleri kendi kayıtlarında bulunmalı. Yeni öldürme görevlerinin hedefleri, erişilebilir bir spawner'ın gerçekten ürettiği arketiplere karşılık gelmeli. XP, görev ilerlemesi, ganimet ve kuşanma doğrulaması sunucu otoritesinde kalmalı.

### 41–50 harita bağımlılığı

`Winter_scene` projenin içinde ayrı bir `.umap` olarak mevcut. 2026-09-28 tarihli salt okunur headless incelemede harita aktörleri arasında PlayerStart veya NavMeshBoundsVolume görülmedi; merkez çevresindeki 25 zemin örneğinin hiçbirinde navmesh projeksiyonu elde edilemedi. 2026-09-29'da `(0,-1500,150)` PlayerStart ve X±4000/Y±3000/Z±1000 kapsayan NavMeshBoundsVolume editör otomasyonuyla kaydedildi. Navmesh daha sonra üretilip kaydedildi; batıdaki tam yollar ölçüldü. **Oyuncu doğuşu ve dövüş erişimi PIE'de henüz doğrulanmadı.** Geçiş prototipinin tasarım kararı listen server'ın tüm bağlı oyuncuları birlikte taşımasıdır; tek oyuncuya özel bölge seyahati uygulanmıyor.

2026-09-29 çapraz incelemesinde 25/25 noktada zemin izi bulunduğu, fakat 0/25 noktada navmesh projeksiyonu olduğu teyit edildi. NavMeshBoundsVolume kaydedildikten sonraki ayrı, salt okunur headless açılışta da NavMeshBoundsVolume ve PlayerStart haritada göründü, `NavigationSystemV1` mevcuttu; buna rağmen aynı 25 noktanın **tamamı `NO_NAV`** döndü ve log `Unable to find RecastNavMesh instance` uyarısı verdi. Dolayısıyla hacmin varlığı navigasyon verisinin üretildiğini göstermiyor. Sıradaki adım editörde navmesh üretimini/kaydedilmesini ve yeşil alanı incelemek, ardından başlangıçtan savaş alanlarına bağlı yolları ölçmek; headless `-NullRHI` sonucunu PIE navigasyonu varmış gibi yorumlamamak. `(0,-1500)` civarı (zemin Z≈0) başlangıç/hub, `(-3000,-1500)` ve `(3000,-1500)` civarı ise uzak savaş alanı için yalnızca **aday** konumlardır; halka, çarpışma ve tam nav yolu ölçülmeden spawner etkinleştirilmemeli. `MMOGameMode::SpawnMobArea` Winter_scene'de şimdilik içerik üretmez: mevcut Lv1–22 Summer_scene yerleşimi Winter'a taşınmamalı. Sıradaki harita dilimi: kaydedilen NavMeshBounds ile bağlı navmesh'i ve hub/NPC ayak noktalarını ölç, Lv41–50 arketiplerinin ayrı Winter spawner'larını ekle, son olarak bölge geçişini tasarla. Bunlar henüz tamamlanmış sayılmıyor.

**29 Eylül navmesh ilerlemesi:** `RebuildNavigation` konsol komutu ve `on_navigation_bounds_updated` ile Recast verisi headless editörde üretildi ve Winter_scene kaydedildi. Ayrı süreçte harita yeniden açılınca 25 zemin örneğinin 18'inde nav projeksiyonu bulundu; diğer 7 nokta kapsama dışında veya yürünemeyen zeminde. Hub `(0,-1500)` → batı adayı `(-3000,-1500)` yolu geçerli ve tam (9 nokta). Hub → doğu adayı `(3000,-1500)` yolu **kısmi** (15 nokta); bu doğu konumu spawner için kullanılmamalı. Bu salt okunur yol hesabı PIE oynanışının, düşmanların geçişinin veya güvenli yerleşimin yerine geçmez. Nav verisini yeniden üretmek için `Scripts/BuildWinterNavigation.py` eklendi. Sonraki iş batı bölgesinde hub'dan uzak ve tam yolları olan normal/elit cepleri belirlemek, ardından Winter spawner'larını ve bölge geçişini bağlamak.

**29 Eylül Winter yerleşimi:** Batı normal cep `(-3000,-1500)` için 225u ve 150u halkalarda 32/32 örnek, elit cep `(-3000,0)` için aynı halkalarda 32/32 örnek hub'a tam nav yoluyla bağlı. Normal cebin zemin aralığı -9.4…-3.9u, elit cebin -0.8…-0.4u; spawner tabanları bu ölçümlere göre Z=42 ve Z=50 seçildi. Winter_scene açıldığında dört Lv42–48 normali bir spawner'dan, Lv50 `WinterSovereign` ayrı spawner'dan üretilir. Görevli/satıcı/şifacı Winter başlangıcına eklenir. Spawner'ın kendi zemin+nav doğrulaması ayrıca her üretimde çalışır. Bu yerleşimin harita içindeki görünürlüğü, dövüş erişimi, görev teslimi ve çok oyunculu davranışı **PIE'de henüz oynanmadı**. Görünür geçiş arayüzü olmadığından iki harita arasındaki zincir oyuncuya henüz doğal bir akışla sunulmuyor.

**29 Eylül çok oyunculu geçiş prototipi:** `MMOTravel` komutunu yalnız listen server host'u çalıştırabilir. `AMMOGameMode` o anki Summer/Winter haritasından ötekini seçer, bütün bağlı oyuncuların canlı ve savaş dışında olduğunu denetler, çakışan kayıt slotlarını reddeder ve her karakterin diske yazımının başarılı olmasını şart koşar. Ardından `ServerTravel` ile tüm oturumu taşır; seamless travel açılmıştır. Hedef haritadaki yeni pawn'un mevcut yükleme akışı karakter verisini aynı slottan alır ve kayıtlı harita adı farklı olduğu için konumu hedef `PlayerStart`'a taşır. **Bu yalnızca derlenen bir prototiptir; iki oyunculu PIE'de geçiş, slot kimliği, sınıf/stat/görev korunumu, spawn ve dönüş henüz doğrulanmadı.** Kalıcı hesap kimliği henüz yok. Harita değişimi anlık can/mana korumaz; mevcut kayıt biçimi bunları tutmaz.

**1 Ekim görünür geçiş denetimi:** Host'un HUD'ına, açık pencere ve ölüm ekranı yokken, "Tüm oyuncuları Kış'a/Yaz'a taşı" düğmesi eklendi. Tıklama mevcut `MMOTravel` sunucu denetimine gider; uzak istemcilerde düğme çizilmez. Reddedilen geçişin nedeni host ekranında gösterilir. Kayıt slotları Windows dosya sistemiyle uyumlu olarak büyük/küçük harf gözetmeden karşılaştırılır. `MyProjectEditor` C++ derlemesi başarılıdır. Düğmenin ekrandaki konumu, tıklaması ve iki oyunculu Summer↔Winter geçişi **PIE'de henüz doğrulanmadı**.

## Lv51–80 içerik genişletmesi: uygulanabilir tasarım dilimleri `[PROPOSED]`

### Mevcut sınır ve bu bölümün varsayımları

`[IMPLEMENTED]` `WinterSovereign` Lv50 / 30.000 XP, normal Kış mobları Lv42–48 / 14.000–22.000 XP. Ana öldürme zinciri `Q_WinterSovereign` ile biter. `Q_Level65` ve `Q_Level80` yalnızca `ReachLevel` kilometre taşlarıdır; ilki 55.000, ikincisi 90.000 XP verir. En yüksek temel ekipman `Winter` setidir (gereken seviye 42; silah 108 saldırı, zırh/kask/bot 65/27/36 savunma). 58. seviyede dört sınıfın yeni becerileri, 70 ve 80'de dört sınıfın `Special3` becerileri tanımlıdır. 51–80 için mob, ana öldürme görevi, temel ekipman veya yerleşik spawner yoktur. Bunlar kaynak tablo durumlarıdır; aşağıdaki adlar, sayılar ve bölgeler henüz oyunda mevcut değildir.

`[PROPOSED]` Lv51'e geçiş kapısı, önce Winter'ın yürünebilirliğinin ve `Q_WinterSovereign` tamamlanabilirliğinin sağlanmasıdır. Ana zincir bu görevi ön koşul alır. Ayrı harita mı aynı haritada genişleme mi yapılacağı, listen server bölge geçişi ve kayıt durumunun nasıl korunacağı karara bağlanmadan konum veya harita adı kesinleştirilmez. Yeni görev ve düşman kimlikleri aşağıda **ayrılmış öneri adlarıdır**; veri tablosu kaydı değildir.

### Dünya kapasitesi ve görev zinciri `[PROPOSED]`

Her satır için oyuncunun ulaşabildiği bir görev veren/güvenli dönüş noktası, birbirinden ayrılmış iki normal mob alanı ve zorunlu elit alanı gerekir: en az **8 normal spawner + 4 elit spawner**. İlk kapasite taslağı her normal alanda en çok 5 eşzamanlı mob, her elit alanda 1 elittir; bu sayı sunucu yükü, doğma aralığı ve eşzamanlı oyuncu sayısı ölçülünce değişir. Her doğum halkası zemine ve bağlı navmesh'e oturmalı; NPC/yeniden doğuş noktası aggro dışında kalmalı. Elit tek bir zorunlu hedefse bekleme ve parti paylaşımı, görevin tamamlanmasını tıkamamalı. Bu koşullar doğrulanmadan salt tablo satırı bir seviye bandını oynanabilir yapmaz.

| Bant / sıra | Normal hedefler ve elit (önerilen ID, Lv) | Dört görevlik ana zincir (önerilen ID; hedef/adet; açılış) | Alan ve oyuncu kararı |
| --- | --- | --- | --- |
| 51–57, `Q_WinterSovereign` sonrası | `FrostboundScout` 51, `FrostboundSentinel` 55; `FrostboundWarden` 57 elit | `Q_FrostboundScouts` 5×Scout / Lv51 → `Q_FrostboundSentinels` 5×Sentinel / Lv55 → `Q_FrostboundLine` 6×Sentinel / Lv55 → `Q_FrostboundWarden` 1×Warden / Lv57 | Winter çıkışı için iki ayrı savaş cebi ve kapı eliti; Lv51 oyuncusu önce mevcut Kış setiyle başlayabilir. |
| 58–65, önceki elit sonrası | `RiftSeeker` 58, `RiftKeeper` 62; `RiftHerald` 65 elit | `Q_RiftSeekers` 5×Seeker / Lv58 → `Q_RiftKeepers` 5×Keeper / Lv62 → `Q_RiftWatch` 6×Keeper / Lv62 → `Q_RiftHerald` 1×Herald / Lv65 | 58 becerileri için normal ve çoklu hedef karşılaşması; `Q_Level65` ana zincirden bağımsız kilometre taşı olarak kalır. |
| 66–70, önceki elit sonrası | `AshVanguard` 66, `AshBinder` 68; `AshTyrant` 70 elit | `Q_AshVanguard` 5×Vanguard / Lv66 → `Q_AshBinders` 5×Binder / Lv68 → `Q_AshLine` 6×Binder / Lv68 → `Q_AshTyrant` 1×Tyrant / Lv70 | 70 becerisini deneyecek normal/elit yoğunluğu; elit alanı normal mob doğum halkasıyla çakışmaz. |
| 71–80, önceki elit sonrası | `AbyssStrider` 72, `AbyssGuardian` 76; `AbyssRegent` 80 elit | `Q_AbyssStriders` 5×Strider / Lv72 → `Q_AbyssGuardians` 5×Guardian / Lv76 → `Q_AbyssLine` 6×Guardian / Lv76 → `Q_AbyssRegent` 1×Regent / Lv79 (uygulamada) | Lv71'de önceki alan erişilebilir kalır; Lv76 normali 79→80 için tekrarlanabilir hedef olur. Lv80 elit, 80 becerisi için son karşılaşmadır; `Q_Level80` ayrı varış görevidir. |

Her ok, bir önceki **görevin tamamlanmasını** ön koşul yapar; satır başındaki görev önceki satırın elit görevine bağlanır. Normal hedefler kendi seviyelerinde üretilebilir olmalı; 51/58/66/72 başlangıçları arasında erişilemez bir XP veya seyahat boşluğu oluşmamalı. Dört görev/bant, tek seferlik 5+5+6 normal ve 1 elit öldürme hedefidir; tekrar eden günlük görev veya yeni görev tipi varsayılmaz. Ödül NPC'sine gidiş dönüş ve dolu ekipman çantasında ödül alma akışı da bu zincirin parçasıdır. Ödül XP'si ve altını aşağıdaki denge çalışmasıyla belirlenir.

### Ekipman kademeleri `[PROPOSED]` ve değer varsayımları `[PLACEHOLDER]`

Her bantta silah, zırh, kask ve bot için dört yeni **temel eşya tanımı** gerekir; önerilen gereken seviyeler sırasıyla 51, 58, 66 ve 72'dir. Her bandın ilk iki normal görevi bot/kaskı, üçüncü normal görevi zırhı, elit görevi silahı garanti ödül olarak verecek şekilde tasarlanır. Böylece dört yuvaya erişim rastgele düşüşe bağlı olmaz ve ilk ödül alındığında kuşanılabilir. Normal/elit ganimet havuzları ve satıcı fiyatları ayrıca incelenir; mevcut düşüş olasılığı otomatik olarak yeni ekonomiye uygun sayılmaz.

| Gereken Lv | Kademe için geçici silah temel saldırısı | Geçici zırh / kask / bot temel savunması | Karşılaştırma amacı |
| ---: | ---: | ---: | --- |
| 51 | 124 | 75 / 31 / 41 | 42 Kış setinden küçük artış; mevcut yükseltilmiş eşyayı hemen değersizleştirmeme. |
| 58 | 142 | 86 / 36 / 47 | 58 becerilerinin mana ve hasar ihtiyacıyla birlikte ölçme. |
| 66 | 164 | 99 / 42 / 55 | 70 becerisi öncesi hayatta kalma ve saldırı hızını ayrı ölçme. |
| 72 | 188 | 114 / 48 / 64 | 79→80 ve Lv80 elit için son temel kademe; +8 yükseltmeyle yeniden karşılaştırma. |

Bu rakamlar **uygulanmış denge değildir**. Mevcut +8 yükseltme temel değere toplam %160 eklediğinden, çıplak yeni silahı yükseltilmiş eski silahla değil aynı yükseltme düzeyinde de karşılaştır. Sınıfın tercih ettiği statlar, iksir/mana gideri, ekipman satış geliri ve yükseltme altın çıkışı birlikte değerlendirilir. Kesin eşya adları/ID'leri, görev ödülü kayıtları ve satıcı erişimi içerik diliminde belirlenir.

### XP, dövüş ve ekonomi kabul varsayımları `[PLACEHOLDER]`

Mevcut XP eğrisi korunur. Aşağıdaki mob XP değerleri, **görev XP'si yokmuş gibi** tek seviye eşiğini yaklaşık 12–25 normal öldürmede karşılamak için başlangıç hipotezidir; elit XP'si bunlardan ayrı tasarlanır. Gerçek hedef, her seviyede o seviyeye dek alınabilecek tek seferlik görev XP'si çıkarılarak `ceil((eşik − görev XP'si) / uygun normal mob XP'si)` ile tekrar hesaplanır. Görevlerin dört adımlı olması her seviyeye görev XP'si geleceği anlamına gelmez.

| Bant | Geçici normal XP/öldürme | Hesap örneği (görev XP'si = 0) | İncelenecek diğer maliyet |
| --- | ---: | --- | --- |
| 51–57 | 30.000–38.000 | 51→52: 543.542 / 30.000 ≈ 19 öldürme | Winter'dan yol süresi, görev NPC'sine dönüş. |
| 58–65 | 45.000–55.000 | 65→66: 1.155.776 / 55.000 ≈ 22 öldürme | 58 becerileriyle mana/iksir tüketimi. |
| 66–70 | 60.000–72.000 | 70→71: 1.430.576 / 72.000 ≈ 20 öldürme | 70 becerisi ve parti ile elit öldürme süresi. |
| 71–80 | 80.000–100.000 | 79→80: 1.999.682 / 100.000 ≈ 20 öldürme | Lv76 hedefinin tekrar yoğunluğu, ölüm XP kaybı ve son kademe maliyeti. |

XP sayıları `FMMOMobArchetype` kayıtları için öneridir; can/hasar, elit XP'si, görev XP/altını, düşüş/satış fiyatları ve yeniden doğuş süreleri **açık denge kararı** olarak kalır. Tam mob XP'si yakındaki uygun parti üyelerine de verildiği ve seviye farkı indirimi bulunmadığı için aynı değerlerin solo, parti ve düşük seviye çiftlik koşularında nasıl davrandığı gözlenmeli. Her bantta dört sınıfla öldürme süresi, can/mana tüketimi, ölüm, çanta doluluğu, altın girişi/çıkışı ve görev başına gidiş dönüş süresi kaydedilmeden değerler kilitlenmez. `Q_Level80` 80'e ulaştıktan sonra alınırsa XP bölümü `GainExperience` tarafından kullanılmaz; bu görev 79→80 açığını kapatıyor gibi bütçelenmez.

**Uygulama sırası:** Winter erişimi ve geçiş kuralı → 51–57 için arketip/görev/dört eşya ve erişilebilir 2+1 alan → ölçüm → aynı kalıbı sırasıyla 58–65, 66–70, 71–80 için açma. Bir sonraki banda geçmeden önce görev hedefi arketipinin gerçekten üretildiği, zincir/ödül kimliklerinin bulunduğu ve tekli/partili akışın ilerlediği gösterilir.

**3 Ekim Lv51–57 Frostbound dilimi `[IMPLEMENTED, PIE bekliyor]`:** `FrostboundScout` (Lv51, 30.000 XP), `FrostboundSentinel` (Lv55, 36.000 XP), elit `FrostboundWarden` (Lv57, 52.000 XP), Lv51 `Frostbound` dört parça seti (124 / 75 / 31 / 41, plandaki geçici değerler) ve `Q_WinterSovereign` sonrası dört görevlik zincir (`Q_FrostboundScouts` → `Q_FrostboundSentinels` → `Q_FrostboundLine` → `Q_FrostboundWarden`; ödüller bot → kask → zırh → silah) eklendi. Winter_scene'de üç spawner: Gözcü cebi doğu `(3000,0,Z210)`, Nöbetçi cebi kuzeybatı `(-3800,1200,Z45)`, Muhafız eliti `(-2000,1000,Z48)`. Headless probla spawner'ın kendi kontrolünün aynısı (zemin ±60u, eğim, navmesh 50/35u sapma, hub'a tam yol) halkalarda ölçüldü: Gözcü 48/48, Nöbetçi 48/48, Muhafız 16/16 (ilk aday `(-2200,1200)` 8/16 navmesh sapması verdiği için taşındı). Cepler birbirinden ve Lv42–50 ceplerinden ≥1400u. Derleme başarılı; dövüş, denge ve görev akışı **PIE'de henüz oynanmadı**.

**3 Ekim Lv58–65 Rift dilimi `[IMPLEMENTED, PIE bekliyor]`:** Winter'da başka üçlü cep sığmadığı için (kalan adaylar mevcut ceplere <1300u) bant **Summer_scene dış ceplerine** yerleştirildi; Summer grid probu (±8000u, 800u adım) mevcut tüm ceplerden ≥1400u uzakta üç uygun cep buldu. `RiftSeeker` (Lv58, 45.000 XP) `(2400,2400,Z205)` — halka 46/48 (2 örnek izole nav adacığı, nadiren erişilemez doğum olabilir); `RiftKeeper` (Lv62, 52.000 XP) `(-4000,3200,Z-120)` 48/48; elit `RiftHerald` (Lv65, 75.000 XP) `(3200,0,Z216)` 16/16. Lv58 `Rift` seti (142 / 86 / 36 / 47) ve `Q_FrostboundWarden` sonrası dört görev (`Q_RiftSeekers` → `Q_RiftKeepers` → `Q_RiftWatch` → `Q_RiftHerald`). Oyuncu Lv58'de Winter'dan Summer'a geri döner (host geçiş düğmesi). PIE yok.

**3 Ekim Winter navmesh genişletmesi + Lv66–80 dilimleri `[IMPLEMENTED, PIE bekliyor]`:** Zemin taraması iki haritada da arazinin ±12000u'ya kadar uzandığını gösterdi; Winter NavMeshBoundsVolume `Scripts/ExtendWinterNavigation.py` ile X±8000 / Y −8500..5500 / Z±1500'e büyütüldü, Recast verisi yeniden üretilip Winter_scene kaydedildi. Yeni grid probu onlarca uygun cep verdi. **Kül Hattı (66–70):** `AshVanguard` Lv66 (60.000 XP) `(1200,-5200)`, `AshBinder` Lv68 (66.000 XP) `(-2000,-6800)`, elit `AshTyrant` Lv70 (100.000 XP) `(4400,-5200,Z340)`; Lv66 `Ashen` seti (164 / 99 / 42 / 55); `Q_RiftHerald` sonrası `Q_AshVanguard` → `Q_AshBinders` → `Q_AshLine` → `Q_AshTyrant`. **Uçurum (71–80):** `AbyssStrider` Lv72 (82.000 XP) `(-5200,3600)`, `AbyssGuardian` Lv76 (95.000 XP) `(4400,3600,Z550)`, oyunun son eliti `AbyssRegent` Lv80 (140.000 XP, +4..+6 loot) `(-7600,-1200,Z1197 yayla)`; Lv72 `Abyss` seti (188 / 114 / 48 / 64); `Q_AbyssStriders` (Lv71'de açılır — plandaki 72 yerine, boş seviye kalmasın) → `Q_AbyssGuardians` → `Q_AbyssLine` → `Q_AbyssRegent` (Lv80'de XP kullanılmadığından ödülün anlamı altın + silah). Yeniden kurulan navmesh üzerinde 11 Winter cebinin tamamı (yeni 6 + eski 5) spawner-eşdeğer probda %100 geçti. Böylece **Lv1–80 ana öldürme zinciri kodda kesintisiz**; hiçbir dilim PIE'de oynanmadı.

**3 Ekim QA düzeltmeleri (Frostbound denetimi):** Nöbetçi cebi `(-3800,1200)` → `(-2800,4400)`, Muhafız eliti `(-2000,1000)` → `(2000,2800)` taşındı (Kış Hükümdarı'na ~1400u'luk yakınlık, gezinme+aggro ile istenmeyen çekişlere yol açabiliyordu; Nöbetçi eski navmesh kenarındaydı). Yeni konumlar probda 47/48 ve 16/16. Bant içi seviye boşluklarını kapatmak için ikinci normal görevlerin seviye kapısı indirildi: `Q_FrostboundSentinels` 53, `Q_FrostboundLine` 54, `Q_RiftKeepers` 60, `Q_RiftWatch` 61, `Q_AshBinders` 67, `Q_AbyssGuardians` 74, `Q_AbyssLine` 75. Tüm Lv51+ normal ceplerinde eşzamanlı mob 5 → 6.

**3 Ekim QA düzeltmeleri (Rift/Kül/Uçurum denetimi):** Rift cepleri Summer'da erken bölgelere 1450–1660u mesafedeydi; Rift mob'u spawn+gezinme+aggro ile ~1275u'dan oyuncuyu fark ettiği için Lv7–40 oyuncular Lv58+ mob çekebiliyordu. Summer NavMeshBoundsVolume `Scripts/ExtendSummerNavigation.py` ile X/Y ±8500'e genişletildi (harita kaydedildi) ve cepler taşındı: Arayıcı `(800,-4800)`, Bekçi `(-4800,4800)`, Haberci eliti `(4800,4000)` — en yakın eski cebe ≥~3400u, probda 48/48, 48/48, 16/16. Uçurum Naibi gezinmesi Winter nav sınırını aştığı için `(-7600,-1200)` → `(6800,2000)` taşındı. `Q_AbyssRegent` kapısı Lv80 → Lv79 (Lv79 oyuncu Naibi kesip kredi alabilsin). Açık kalan: Lv61→65, Lv71→74, Lv75→80 görevsiz grind aralıkları.

**Grind aralığı yan görevleri `[IMPLEMENTED, PIE yok]`:** Ana zincirden dallanan, zinciri kilitlemeyen 7 yan görev: `Q_RiftPurge` (Lv61, 10 Arayıcı, 450K) → `Q_RiftDepths` (Lv63, 10 Bekçi, 550K); `Q_AbyssPatrol` (Lv71, 10 Gezgin, 650K) → `Q_AbyssEdge` (Lv73, 12 Gezgin, 750K); `Q_AbyssVigil` (Lv75, 10 Koruyucu, 850K) → `Q_AbyssDescent` (Lv77, 12 Gezgin, 900K) → `Q_AbyssBastion` (Lv78, 12 Koruyucu, 1.0M). Ödül: altın + 5 büyük iksir, ekipman yok. Görevsiz kill ihtiyacı tahmini: Lv61→65 ~80 → ~43, Lv71→74 ~48 → ~18, Lv75→80 ~92 → ~37.

**Denge raporu (`Scripts/BalanceReport.py`, statik, görev XP'si hariç = en kötü durum):** Her seviye için mob seviyesi [L−8, L+3] penceresindeki en çok XP veren normal mob ve seviye atlamak için gereken öldürme sayısı hesaplanır (seviye farkı kuralı dahil). 3 Ekim sonucu: Lv1–23 arası 1–4 öldürme (bilinçli hızlı giriş), Lv24–28 4–6, Lv29–35 8–14, Lv36–47 13–19, Lv48–62 15–20, Lv63–79 17–22. Hiçbir seviye 25'i aşmıyor, uygun normal mob olmayan seviye yok; plan hedefi (12–25) Lv29+ için tutuyor. Uyarı: tabloda görev XP'si yok, yani gerçek sayılar daha düşük; ilk PIE koşusunda gerçek süreler ölçülmeli. `Scripts/ValidateData.py` de kimlik/ön koşul denetimini yapar.

**Seviye farkı XP ayarı `[IMPLEMENTED, PIE yok]`:** Yukarıdaki envanterde "seviye farkı indirimi yok" diye geçen açık kapatıldı. `AMMOMob::ScaleXPForLevelGap` mob ölümünde her alıcı için ayrı uygulanır: oyuncu mob'dan >5 seviye yüksekse her ek seviye −%10 (taban %10, "gri mob"); mob oyuncudan >10 seviye yüksekse (parti ile power-leveling) her ek seviye −%10 (taban %20). Görev kredisi ve altın değişmez. Etkisi: karışık bantlarda üst sınırdaki oyuncu en düşük mob'dan daha az alır (ör. Lv40 → Lv31 Alacakaranlık %60, Lv50 → Lv42 Kış %70); Lv1 karakterin Lv76 partide aldığı XP %20'ye iner. Tasarlanmış bant/görev akışında (oyuncu ≤ mob+5) değişiklik yok.

## Sıradaki üç çalışma dilimi

### 1. Winter yürünebilirliğini ve güvenli doğuşu tamamla

`Winter_scene` içine PlayerStart, NavMeshBoundsVolume ve Recast navmesh kaydedildi. Batı savaş alanlarından hub'a tam yollar headless editörde ölçüldü; doğu adayı kısmi yol nedeniyle dışarıda bırakıldı. Başlangıcın zeminin üzerinde güvenli olduğunu ve oyuncu/mob hareketini PIE'de doğrula. Yeni cep eklerken yalnız nav kapsamasını değil, tam yol ve zemin yüksekliğini de ölç.

### 2. Winter içeriğini dünyaya bağla

Lv42–48 normalleri ve Lv50 elit için Winter'a özgü batı spawner'ları eklendi. Görevli, satıcı ve şifacı Winter başlangıcına yerleştirildi; Summer yerleşimi Winter'da açılmıyor. `Q_WinterStalkers`–`Q_WinterSovereign` zincirinin erişilebilirliği, sunucu ve istemci PIE oynanışına kadar açık iş olarak kalsın.

### 3. Harita geçişi ve Lv51–80 içeriği

Listen server'da bütün oyuncuların harita değiştirip değiştirmeyeceğine ilişkin geçiş kuralını belirleyip sunucu otoritesinde uygula; görev ve kayıt durumunu koru. Sonra 51–80 aralığını oynanabilir harita kapasitesiyle birlikte küçük mob/görev/ekipman dilimleri halinde genişlet. Yalnız veri tablosuna satır eklemek, oynanabilir seviye aralığı sayılmaz.

## Önceki kod dilimleri ve kalan PIE doğrulaması

### 1. Tek seferlik ödül ve spawner aralığı hatalarını görünür kıl (kod tamamlandı; PIE bekliyor)

- **Oyuncu kararı/deneyimi:** Zor bir görevi bitiren oyuncu, vaat edilen ekipmanı güvenilir biçimde alır. İçeriği boş bir seviye aralığı sessizce erken oyun moblarının çiftliğine dönüşmez.
- **Girdi → çıktı:** `ClaimReward` sırasında, ödülleri vermeden veya görevi tamamlandı saymadan önce ekipman çantasında yer olup olmadığını kontrol et. Çanta doluysa açık bir hata göster ve görevi ödülü alınabilir durumda bırak. Tanımlanan seviye aralığında eşleşen normal arketip yoksa geçersiz aralığı bildir ve genel normal mob havuzundan seçim yapmak yerine hiçbir şey üretme. Geçerli mevcut aralıklar çalışmaya devam etsin.
- **Dosya kapsamı:** `Source/MyProject/Private/Components/MMOQuestComponent.cpp`, `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`, `Source/MyProject/Private/Core/MMOMobSpawner.cpp`; yalnızca API değişikliği gerekiyorsa bunların başlık dosyaları.
- **Sınır/hata durumları:** Ekipman ödülü olmayan görevler, ekipman çantası doluyken de alınabilsin. Gelecekte iki eşyalı ödül eklenirse her iki yuva da önceden ayrılmalı. Hiç satırı olmayan `[25,30]` aralığı Goblin değil tanı mesajı üretmeli. Parti XP'si ve görev kredisi sunucunun kontrolünde kalmalı.
- **Tamamlanma ölçütü:** İki durum da PIE içinde sunucu ve istemciyle gösterilebilmeli; kabul edilmiş görev ödülü dolu çanta yüzünden kaybolmamalı, boş spawner aralığı başka seviyeden mob üretmemeli. Bu dilimde denge sayıları değişmez.

### 2. Oynanabilir bir 25–30. seviye aralığı sun (P0 içerik)

**Güncel durum:** Üç normal arketip (Lv25/27/29), bir Lv30 elit, dört Obsidyen eşya ve dört görevlik zincir kaynak koduna eklendi; derleme başarılı. `Summer_scene` üzerinde zemin, navmesh halkaları ve bağlantı yolları headless olarak ölçülüp normal/elit spawner'ları yerleştirildi. Bu iki spawner rastgele doğum noktasında zemin ve navmesh katmanını sunucuda ayrıca doğruluyor. Gerçek PIE oynanışı ve denge doğrulaması bekliyor.

- **Oyuncu kararı/deneyimi:** `Q_MagmaTyrant` sonrasında oyuncu yeni bir hedef seçebilir ve ana göreve devam ederken kullanışlı dört yuvalı bir seti yükseltebilir.
- **Girdi → çıktı:** 25–29 arasında gerçek seviyeleri bulunan `[PLACEHOLDER]` 2–3 normal arketip ve bir 30. seviye elit ekle. `Q_MagmaTyrant` tamamlanmasını ön koşul yapan kısa ve sıralı bir `KillSpecificMob` zinciri ekle. Bu aralıkta gereken seviyesi açıkça tanımlı silah/zırh/kask/bot ekle. 25–30 normal spawner'ı ve zorunlu elit spawner'ını yalnızca zemin/navmesh doğrulanmış konumlara yerleştir. Görev ekipman ödülleri verildikleri anda kayıtlı olmalı ve doğru seviye koşuluyla kuşanılmalı.
- **Dosya kapsamı:** `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`, `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`, `Source/MyProject/Private/Items/MMOItemDatabase.cpp`, `Source/MyProject/Private/Core/MMOGameMode.cpp`. `MMOMob.cpp` dosyasına yalnızca ölçülmüş ganimet veya XP davranışı gerektirirse dokun.
- **Denge yöntemi:** Mevcut 22. seviye normali (3,700 XP, 82 hasar) ve 24. seviye eliti (6,800 XP, 95 hasar) alt aralık referansları olsun. 25→26 (21,176) ile 30→31 (59,576) arasındaki XP eşiklerini, görev ödülü payını ve `[PLACEHOLDER]` seviye başına 12–25 öldürme hedefini birlikte hesapla. Canı ve hasarı ölçülen öldürme süresi ve hayatta kalma verisine göre seç; yeniden doğuş noktasında mob aggro'su olmaması açık koşul olsun. Elitin 6,800 XP ödülünü sorgulamadan normal mob ödülü olarak kullanma.
- **Tamamlanma ölçütü:** Eklenen her öldürme hedefi üretilebilir ve erişilebilir olsun. 24. seviye sonrası görev zinciri kabul, tamamlama ve ödül alma adımlarından geçsin. Dört yuvanın ekipmanı belirtilen seviyelerde kuşanılabilsin. Kaynak tablolarda eksik veya yinelenen kimlik bulunmasın. Solo ve parti PIE koşularında seviye başına öldürme, ölüm oranı ve altın akışı kaydedilsin. Haritada uygun alan yoksa veriyi tamamla; harita bağımlılığı çözülene kadar spawner'ı etkinleştirme.

### 3. Dövüş ilerlemesini 40. seviyeye kadar sürdür (P0 içerik)

**Güncel durum:** Dört Lv31–39 normal arketip, bir Lv40 elit, dört Lv33 ekipman ve `Q_ObsidianWarlord` sonrasında beş görevlik zincir kaynak koduna eklendi; UE 5.7 derlemesi başarılı. `Summer_scene` üzerinde normal alan `(1000,3200)`, elit alan `(-3200,1800)` için zemin/navmesh halkaları ve PlayerStart'a tam nav yolları headless probla ölçüldü. Her doğum denemesinde sunucu tarafındaki zemin/navmesh doğrulaması etkin. İlk normal alan `(-4100,-1400)` Obsidyen Reisi'nin aggro menziline girdiği için yaklaşık 5936 birim uzaktaki yeni merkeze taşındı. Oynanış dengesi ve gerçek doğum davranışı henüz PIE'de doğrulanmadı.

- **Oyuncu kararı/deneyimi:** 31–40. seviyeler, mevcut 40. seviye kilometre taşından önce yeni dövüş tehditleri ve ekipman kararları sunar.
- **Girdi → çıktı:** 31, 33, 36 ve 39. seviyelerde dört normal arketip ile bir 40. seviye elit eklendi. `Q_ObsidianWarlord` ön koşullu sıralı öldürme zinciri, Lv33 dört yuvalı ekipman ve zemin/navmesh doğrulamalı iki spawner eklendi. `Q_Level30`/`Q_Level40` paralel, tek seferlik başarı ödülleri olarak kaldı.
- **Dosya kapsamı:** İkinci dilimdeki aynı dört C++ veri/spawn dosyası. Bu dilimde beceriler veya genel XP formülü yeniden yazılmaz.
- **Denge yöntemi:** Uygulanmış 30→31 için 59,576 XP ve 40→41 için 225,026 XP eşiklerini kullan. `[PLACEHOLDER]` görev XP'sini her seviye hedefinin açık bir payı olarak belirle; kalan XP ve gözlenen seviye başına öldürme sayısından normal mob XP'sini türet. Yeni ekipmanı 16. seviye Adamantine ile ve +8 yükseltme sonuçlarıyla karşılaştır: erken yükseltmeleri değersizleştirmeden yeni temel kademeyi tercih etmek için gerekçe oluşmalı.
- **Tamamlanma ölçütü:** 30. seviye görev tamamlanınca yeni zincir açılmalı. Gereken bütün öldürmeler dünyada mümkün olmalı. 40. seviyeye ulaşmak için yalnızca 24. seviye eliti tekrar tekrar öldürmek gerekmemeli. Dört sınıf da mevcut 33. seviye becerilerini ve yeni ekipmanı sunucu/istemci PIE koşusunda kullanabilmeli. 41–80. seviye değerlerini belirlemeden önce gözlenen ilerleme hızını kaydet.

## Revizyon kaydı

- 2026-09-28: Kaynak koda dayalı ilk plan.
- 2026-09-28: Plan proje sahibi için Türkçeye çevrildi. `[PLACEHOLDER]` işaretli sayısal hedefler henüz oynanışta sınanmadı.
- 2026-09-28: İlk kod dilimindeki iki hata `117e5fc` ile giderildi ve derlendi; oynanış doğrulaması bekliyor.
- 2026-09-28: Seviye 25–30 veri tabloları eklendi; oynanabilir bölge yerleşimi arazi/navmesh doğrulamasını bekliyor.
- 2026-09-28: Seviye 31–40 veri tabloları ve iki ölçülmüş bölge eklendi; UE derlemesi geçti, PIE ve denge ölçümü bekliyor.
- 2026-09-28: Lv31–39 normal bölgesi, Obsidyen Reisi aggro örtüşmesi nedeniyle `(1000,3200)` konumuna alındı; 64/64 halka ve PlayerStart nav yolu headless doğrulandı.
- 2026-09-28: `Winter_scene` haritasında 25 zemin örneği ve aktör envanteri salt okunur incelendi; Lv41–50 için PlayerStart/navmesh ve çok oyunculu geçiş bağımlılığı belirlendi.
