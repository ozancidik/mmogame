# İlerleme planı: 1–80. seviyeler

**Durum:** Tasarım planı, 2026-09-28. `[IMPLEMENTED]` mevcut C++ kaynak kodunda uygulanmış olanı, `[PROPOSED]` önerilen işi, `[PLACEHOLDER]` ise henüz oynanışta sınanmamış geçici denge hedefini belirtir. Kaynak koddaki eski denge çalışmalarına ilişkin yorumlar geçmişe dair bağlam sağlar; güncel oynanış testi sonucu sayılmaz. Bu belge kaynak kod incelemesine dayanır. Haritada erişilebilirlik, dövüş süresi ve ekonomi hissi PIE içinde ayrıca gözlemlenmelidir.

## Oyuncu hedefi ve öncelikler

Oyuncu her yeni seviye aralığında ulaşabileceği bir düşman, tamamlayabileceği bir görev ve anlamlı bir ekipman veya beceri kararı görmeli. Kaynak kodunda döngü 40. seviye boss'una kadar genişletildi; bu alanların PIE oynanışı bekliyor. 41–80. seviyelerde uygun düşman ve ekipman zinciri hâlâ eksik. Çalışan erken oyun korunarak döngü, küçük ve ölçülebilir aralıklarla genişletilmeli.

| Öncelik | Eksik ve oyuncuya etkisi | Kaynak kanıtı |
| --- | --- | --- |
| P0 | 41–80. seviyeler için normal mob, elit ve erişilebilir spawner yok. Lv25–40 alanları kodda yerleştirildi; PIE doğrulaması bekliyor. | `Source/MyProject/Private/Characters/MMOMobArchetype.cpp` (en yüksek normal `DuskWraith` Lv39, elit `DuskOverlord` Lv40); `Source/MyProject/Private/Core/MMOGameMode.cpp` (31–39 normal bandı ve Lv40 elit). |
| P0 | Ana `KillSpecificMob` zinciri artık `Q_DuskOverlord` ile bitiyor. Sonraki seviyelerde yeni dövüş görevi yok. | `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`; `Source/MyProject/Private/Components/MMOQuestComponent.cpp`. |
| P0 | En yüksek temel ekipman gereksinimi Lv33 (dört Alacakaranlık parçası). 34–80 arasında yeni ekipman kademesi yok. | `Source/MyProject/Private/Items/MMOItemDatabase.cpp`. |
| P1, kodda düzeltildi | Dolu ekipman çantasında görev ödülü kayboluyordu. `117e5fc` ile teslim öncesi kapasite kontrolü eklendi; görev talep edilebilir kalıyor. PIE doğrulaması bekliyor. | `Source/MyProject/Private/Components/MMOQuestComponent.cpp` (`ClaimReward`). |
| P1, kodda düzeltildi | Boş seviye aralığı başka seviyeden rastgele mob üretiyordu. `117e5fc` ile spawner doğumu atlıyor ve hata kaydı bırakıyor. PIE doğrulaması bekliyor. | `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`; `Source/MyProject/Private/Core/MMOMobSpawner.cpp`. |
| P1 | Beceri tanımları 80. seviyeye kadar uzanıyor; fakat 41–80. seviye beceri açılışlarını anlamlı kılacak uygun düşman ve ekipman ilerlemesi yok. | `Source/MyProject/Private/Skills/MMOSkillDatabase.cpp`; `Source/MyProject/Private/Components/MMOSkillComponent.cpp`. |

## Mevcut ilerleme envanteri `[IMPLEMENTED]`

| Sistem | Kaynak kodundaki durum |
| --- | --- |
| Seviye ve statlar | `Level=1`, `MaxLevel=80`; her seviye 3 stat puanı ve 2 beceri puanı veriyor. Can formülü `50 + Vitality × 8 + Level × 12`; mana formülü `25 + Intelligence × 5 + Level × 5.5`. Ölüm, mevcut birikmiş XP'den en fazla o seviyenin XP eşiğinin %10'u kadar düşürüyor. `GainExperience`, 80. seviyede yeni XP kabul etmiyor ve XP'yi sıfırlıyor. Kaynaklar: `Source/MyProject/Public/Components/MMOStatsComponent.h`, `Source/MyProject/Private/Components/MMOStatsComponent.cpp`. |
| Moblar ve dünya | 31 arketip var: 23 normal (aralıklar 1–5, 6–7, 8–12, 16–22, 25–29, 31–39) ve 8 elit (en yükseği Lv40). Obsidyen ve Alacakaranlık spawner'ları Summer_scene zemin ve navmesh ölçümüyle yerleştirildi; gerçek PIE oynanışı bekliyor. Sunucudaki ölüm akışı, mobun tam XP ödülünü öldüren oyuncuya ve yakındaki uygun her parti üyesine veriyor; bu akışta oyuncu/mob seviye farkına göre ödül ayarı yok. Kaynaklar: `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`, `Source/MyProject/Private/Characters/MMOMob.cpp`, `Source/MyProject/Private/Core/MMOGameMode.cpp`. |
| Görevler | 45 tanım var. Ana öldürme zinciri `Q_Goblins` ile başlıyor ve `Q_DuskOverlord` (Lv40) ile bitiyor. Bağımsız öldürme görevleri ile tek seferlik seviye/altın kilometre taşları zinciri tamamlıyor. Bütün görevler, görev veren NPC'nin yakınında kabul edilmeli ve ödülleri yine orada alınmalı. Kaynaklar: `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`, `Source/MyProject/Private/Components/MMOQuestComponent.cpp`. |
| Ekipman | Silah, zırh, kask ve bot olmak üzere dört yuvada 33 tanım var; en yüksek gereken seviye 33. Kuşanma için gereken seviye kontrol ediliyor. Normal moblarda varsayılan ekipman düşürme şansı %25; seçim havuzu mob seviyesi + 3'e kadar gereken seviyedeki eşyaları içerebiliyor. Elitlerde ekipman şansı zar başına %50; 1 veya 2 kez zar atılıyor ve uygun en yüksek kademe filtresi kullanılıyor. Yükseltme üst sınırı +8; her +1, temel saldırı/savunmaya %20 ekliyor. Altın maliyeti `50 × (mevcut artı + 1)`. Kaynaklar: `Source/MyProject/Private/Items/MMOItemDatabase.cpp`, `Source/MyProject/Private/Characters/MMOMob.cpp`, `Source/MyProject/Private/Components/MMOInventoryComponent.cpp`. |
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

`Winter_scene` projenin içinde ayrı bir `.umap` olarak mevcut. 2026-09-28 tarihli salt okunur headless incelemede harita aktörleri arasında PlayerStart veya NavMeshBoundsVolume görülmedi; merkez çevresindeki 25 zemin örneğinin hiçbirinde navmesh projeksiyonu elde edilemedi. Bu, haritanın görsel olarak hazır olmasının mevcut mob AI'siyle oynanabilir bölge olduğu anlamına gelmediğini gösterir. Önce geçerli oyuncu başlangıcı ve navmesh alanı hazırlanmalı, sonra merkez/halka örnekleri ile PlayerStart'a kısmi olmayan yollar ölçülmeli. Ayrı haritaya oyuncu geçişinin listen server'da tüm oyuncuları mı yoksa yalnız ilgili oyuncuyu mu taşıyacağı ayrıca tasarlanmalı; mevcut kodda buna ilişkin bir geçiş uygulanmış sayılmıyor.

2026-09-29 çapraz incelemesinde 25/25 noktada zemin izi bulunduğu, fakat 0/25 noktada navmesh projeksiyonu olduğu teyit edildi. `(0,-1500)` civarı (zemin Z≈0) PlayerStart/hub, `(-3000,-1500)` ve `(3000,-1500)` civarı ise uzak savaş alanı için yalnızca **aday** konumlardır; halka, çarpışma ve tam nav yolu ölçülmeden spawner etkinleştirilmemeli. `MMOGameMode::SpawnMobArea` Winter_scene'de şimdilik içerik üretmez: mevcut Lv1–22 Summer_scene yerleşimi yeni PlayerStart eklendiğinde Winter'a taşınmamalı. Sıradaki harita dilimi: PlayerStart + NavMeshBounds yerleştir, bağlı navmesh'i ve hub/NPC ayak noktalarını ölç, Lv41–50 arketipleri ve ayrı Winter yerleşimini ekle, son olarak bölge geçişini tasarla. Bu adımların hiçbiri tamamlanmış sayılmıyor.

## Sıradaki üç sınırları belirlenmiş kod dilimi

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
