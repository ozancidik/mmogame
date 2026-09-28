# İlerleme planı: 1–80. seviyeler

**Durum:** Tasarım planı, 2026-09-28. `[IMPLEMENTED]` mevcut C++ kaynak kodunda uygulanmış olanı, `[PROPOSED]` önerilen işi, `[PLACEHOLDER]` ise henüz oynanışta sınanmamış geçici denge hedefini belirtir. Kaynak koddaki eski denge çalışmalarına ilişkin yorumlar geçmişe dair bağlam sağlar; güncel oynanış testi sonucu sayılmaz. Bu belge kaynak kod incelemesine dayanır. Haritada erişilebilirlik, dövüş süresi ve ekonomi hissi PIE içinde ayrıca gözlemlenmelidir.

## Oyuncu hedefi ve öncelikler

Oyuncu her yeni seviye aralığında ulaşabileceği bir düşman, tamamlayabileceği bir görev ve anlamlı bir ekipman veya beceri kararı görmeli. Mevcut temel döngü, 24. seviye boss'una kadar bu yapıyı sunuyor. Sonrasında seviye ve beceri kilometre taşları var; ancak oyuncu kalan seviyeler için daha düşük seviyeli düşmanları öldürmeye devam etmek zorunda. Çalışan erken oyun korunarak döngü, küçük ve ölçülebilir aralıklarla genişletilmeli.

| Öncelik | Eksik ve oyuncuya etkisi | Kaynak kanıtı |
| --- | --- | --- |
| P0 | 24. seviyenin üzerinde mob arketipi, 22. seviyenin üzerinde normal mob üreten spawner aralığı yok. 25–80. seviyedeki karakterin kendi seviyesine uygun normal dövüş hedefi bulunmuyor. | `Source/MyProject/Private/Characters/MMOMobArchetype.cpp` (en yüksek normal: `InfernalWraith`, seviye 22, 3,700 XP; en yüksek elit: `MagmaTyrant`, seviye 24, 6,800 XP); `Source/MyProject/Private/Core/MMOGameMode.cpp` (son normal spawner aralığı: 16–22; zorunlu arketiple üretilen seviye 24 elit). |
| P0 | Ana `KillSpecificMob` görev zinciri `Q_MagmaTyrant` ile bitiyor (gereken seviye 24; ödül 12,000 XP, 8,000 altın, `AdamantineBlade`). Sonraki görevler yalnızca seviye veya altın durumunu kontrol ediyor; boss'tan sonra yeni bir dövüş hedefi yok. | `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`; `Source/MyProject/Private/Components/MMOQuestComponent.cpp` (`ReachLevel` ve `HaveGold` anlık durumu kontrol ediyor). |
| P0 | Ekipmanların en yüksek seviye gereksinimi 16. Sonraki seviyeler stat kazandırıyor ama yeni temel ekipman kademesi veya kuşanma seçimi sunmuyor. | `Source/MyProject/Private/Items/MMOItemDatabase.cpp` (25 tanım; `AdamantineBlade` 62 saldırı, `AdamantinePlate` 38 savunma, `AdamantineHelm` 16 savunma, `AdamantineBoots` 22 savunma; hepsinin gereken seviyesi 16). |
| P1, kodda düzeltildi | Dolu ekipman çantasında görev ödülü kayboluyordu. `117e5fc` ile teslim öncesi kapasite kontrolü eklendi; görev talep edilebilir kalıyor. PIE doğrulaması bekliyor. | `Source/MyProject/Private/Components/MMOQuestComponent.cpp` (`ClaimReward`). |
| P1, kodda düzeltildi | Boş seviye aralığı başka seviyeden rastgele mob üretiyordu. `117e5fc` ile spawner doğumu atlıyor ve hata kaydı bırakıyor. PIE doğrulaması bekliyor. | `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`; `Source/MyProject/Private/Core/MMOMobSpawner.cpp`. |
| P1 | Beceri tanımları 80. seviyeye kadar uzanıyor; fakat 24–80. seviye beceri açılışlarını anlamlı kılacak uygun düşman ve ekipman ilerlemesi yok. | `Source/MyProject/Private/Skills/MMOSkillDatabase.cpp`; `Source/MyProject/Private/Components/MMOSkillComponent.cpp`. |

## Mevcut ilerleme envanteri `[IMPLEMENTED]`

| Sistem | Kaynak kodundaki durum |
| --- | --- |
| Seviye ve statlar | `Level=1`, `MaxLevel=80`; her seviye 3 stat puanı ve 2 beceri puanı veriyor. Can formülü `50 + Vitality × 8 + Level × 12`; mana formülü `25 + Intelligence × 5 + Level × 5.5`. Ölüm, mevcut birikmiş XP'den en fazla o seviyenin XP eşiğinin %10'u kadar düşürüyor. `GainExperience`, 80. seviyede yeni XP kabul etmiyor ve XP'yi sıfırlıyor. Kaynaklar: `Source/MyProject/Public/Components/MMOStatsComponent.h`, `Source/MyProject/Private/Components/MMOStatsComponent.cpp`. |
| Moblar ve dünya | 22 arketip var: 16 normal (seviye aralıkları 1–5, 6–7, 8–12, 16–22) ve 6 elit (seviyeleri 5, 6, 7, 7, 13, 24). `MagmaTyrant` 6,800 XP, 1,100–2,300 altın ve 95 temel vuruş hasarı; `InfernalWraith` 3,700 XP, 520–1,200 altın ve 82 temel vuruş hasarı veriyor. Sunucudaki ölüm akışı, mobun tam XP ödülünü öldüren oyuncuya ve yakındaki uygun her parti üyesine veriyor; bu akışta oyuncu/mob seviye farkına göre ödül ayarı yok. Kaynaklar: `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`, `Source/MyProject/Private/Characters/MMOMob.cpp`. |
| Görevler | 36 tanım var. 15 görevlik ana öldürme zinciri `Q_Goblins` ile başlıyor (seviye 1, 5 Goblin, 200 XP) ve `Q_MagmaTyrant` ile bitiyor (seviye 24). Bağımsız öldürme görevleri ile tek seferlik seviye/altın kilometre taşları zinciri tamamlıyor. `Q_Level50`, `Q_Level65` ve `Q_Level80` sırasıyla 35,000/55,000/90,000 XP ve 20,000/30,000/50,000 altın veriyor; `Q_Gold50000` ise 45,000 XP ve sıfır altın veriyor. Bütün görevler, görev veren NPC'nin yakınında kabul edilmeli ve ödülleri yine orada alınmalı. Kaynaklar: `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`, `Source/MyProject/Private/Components/MMOQuestComponent.cpp`. |
| Ekipman | Silah, zırh, kask ve bot olmak üzere dört yuvada 25 tanım var; en yüksek gereken seviye 16. Kuşanma için gereken seviye kontrol ediliyor. Normal moblarda varsayılan ekipman düşürme şansı %25; seçim havuzu mob seviyesi + 3'e kadar gereken seviyedeki eşyaları içerebiliyor. Elitlerde ekipman şansı zar başına %50; 1 veya 2 kez zar atılıyor ve uygun en yüksek kademe filtresi kullanılıyor. Yükseltme üst sınırı +8; her +1, temel saldırı/savunmaya %20 ekliyor. Altın maliyeti `50 × (mevcut artı + 1)`. Kaynaklar: `Source/MyProject/Private/Items/MMOItemDatabase.cpp`, `Source/MyProject/Private/Characters/MMOMob.cpp`, `Source/MyProject/Private/Components/MMOInventoryComponent.cpp`. |
| Beceriler | 61 tanım var: Warrior 18, Archer 14, Mage 13, Priest 16. Dört sınıfın da 70 ve 80. seviyelerde `Special3` becerileri var. Öğrenmek için hem gereken seviye hem ustalık rütbesi sağlanmalı; ustalığa yatırılan her rütbe 1 beceri puanına mal oluyor ve o rütbedeki birden fazla beceriyi açabiliyor. Bu akışta veri tablosundaki `SkillPointCost`, beceri başına ayrı bir ücret olarak alınmıyor. Örnekler: Warrior `War_WarCyclone` seviye 45 / güç 510, `War_DoomsdayBlow` seviye 80 / güç 1,725 / mana 400; Priest `Pri_HandOfHeaven` seviye 80 / iyileştirme gücü 1,650 / mana 420. Kaynaklar: `Source/MyProject/Private/Skills/MMOSkillDatabase.cpp`, `Source/MyProject/Private/Components/MMOSkillComponent.cpp`. |

**Erken ödüllerin kullanım zamanı:** `[IMPLEMENTED]` `Q_Boss` 5. seviyede açılıyor ama verdiği `GreatSword` için 6. seviye gerekiyor. `Q_CursedKnights` 9. seviyede açılıyor ama verdiği `MithrilBoots` için 10. seviye gerekiyor. Bunlar bir sonraki seviyeye hazırlık ödülleri; görev arayüzü ödül alınmadan önce kuşanma seviyesini göstermeli. Hemen kullanılabilir diye anlatılmamalılar. Kaynaklar: `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`, `Source/MyProject/Private/Items/MMOItemDatabase.cpp`.

## XP eğrisi ve mevcut ödüllerin anlamı

`[IMPLEMENTED]` `ComputeExperienceForLevel(L)`, hesaplamadan sonra `int32`'ye yuvarlanan **L → L+1 için gereken XP** değerini döndürüyor:

```text
L <= 25: round(100 × 1.25^(L-1))
L > 25:  round(100 × 1.25^24 + 4725 × (L-25) + 591 × (L-25)^2)
```

Kaynak kod sonuçları `MAX_int32` ile sınırlıyor. `GainExperience`, 80. seviyede daha fazla XP kabul etmiyor; hesaplanan L80→81 değeri oynanabilir bir gereksinim değil. Kaynak: `Source/MyProject/Private/Components/MMOStatsComponent.cpp` (`ComputeExperienceForLevel`, `GainExperience`).

| Mevcut seviye → sonraki | Gereken XP | Mevcut en yüksek normal mobla (3,700 XP) teorik öldürme sayısı | Mevcut en yüksek elitle (6,800 XP) teorik öldürme sayısı |
| --- | ---: | ---: | ---: |
| 1 → 2 | 100 | — | — |
| 24 → 25 | 16,941 | 5 | 3 |
| 25 → 26 | 21,176 | 6 | 4 |
| 26 → 27 | 26,492 | 8 | 4 |
| 30 → 31 | 59,576 | 17 | 9 |
| 40 → 41 | 225,026 | 61 | 34 |
| 50 → 51 | 508,676 | 138 | 75 |
| 65 → 66 | 1,155,776 | 313 | 170 |
| 79 → 80 | 1,999,682 | 541 | 295 |

Öldürme sayıları `ceil(eşik / mobun tabloda yazan XP'si)` ile hesaplandı; görev XP'si, ölüm cezası, yol, bekleme süresi ve parti etkileri dahil değil. Bunlar ölçülmüş ilerleme hızı değil, ölçeği gösteren hesaplar. 1→80 arasındaki seviye eşiklerinin toplamı **40,153,013 XP**; yalnızca 25→80 arası **40,068,710 XP**. Görev ödülleri hesaba katılmadığında, 1→80 toplam eşiği için yalnızca `MagmaTyrant` öldürülen kesintisiz ve gerçekçi olmayan bir senaryoda bile en az **5,905 öldürme** gerekiyor. Asıl eksik, tekrar oynanabilir ve seviyeye uygun içerik. Tek seferlik `Q_Level80` görevinin 90,000 XP ödülü, 79→80 eşiğinin o aşamada alınması halinde **%4.5'i**; oyuncu 80. seviyedeyken ise seviye ilerlemesine katkısı yok. Bu görev şu anda 80'e ulaşma yolu değil, seviye tavanına varış kutlaması ve altın ödülü işlevi görüyor.

`[PROPOSED]` Seviyeye uygun mob XP'si ve görevler eklenirken mevcut XP eğrisi korunsun. Yalnızca kaynak koda bakarak 1–25 formülü değiştirilmesin veya yeni bir XP çarpanı belirlenmesin. Her yeni aralık için `seviye başına öldürme = ceil((seviye XP eşiği − o seviyede beklenen tek seferlik görev XP'si) / ortalama mob XP'si)` hesaplanmalı; oynama süresi ayrıca ölçülen öldürme ve yol sürelerinden modellenmeli. `[PLACEHOLDER]` İlk oynanış testi hedefi: ilgili görev etkinken seviye başına 12–25 normal mob öldürme. Her sınıfla bir solo, ayrıca bir parti koşusu yapıldıktan sonra yeniden değerlendirilmeli. Bu aralık uygulanmış bir ilerleme hızı değil, sınanacak bir varsayım.

## Seviye aralıklarına göre yol haritası `[PROPOSED]`

| Aralık | İçerik kararı | Bağımlılık ve denge kontrolü |
| --- | --- | --- |
| 1–24 | Mevcut arketipleri, görev zincirini, ekipman kademelerini ve beceri koşullarını koru. Yeni tek seferlik ekipman ödüllerinden önce ödül teslimini düzelt. | Erken oyundaki sınıf dövüşlerini ve görev tamamlanmasını PIE içinde ölç; yalnızca gözleme dayanarak ayarla. |
| 25–30 | Küçük bir normal mob ailesi ve aralık sonu eliti, `Q_MagmaTyrant` görevini doğrudan devam ettiren bir zincir ve Adamantine sonrasındaki ilk dört yuvalı ekipman kademesini ekle. | Yeni spawner yerleştirmeden önce üzerinde yürünebilir dövüş alanını ve doğuş/NPC noktalarına güvenli mesafeyi doğrula. `MMOQuestTypes.cpp` yorumları, haritanın daha uzak kısımlarındaki zemin/navmesh'in henüz doğrulanmadığını açıkça söylüyor. |
| 31–40 | İkinci bir mob ailesi, elit ve ana öldürme zinciri ekle; `Q_Level30`/`Q_Level40` görevlerini tek hedef olmak yerine ek kilometre taşları olarak kullan. | 30→31 (59,576 XP) ve 40→41 (225,026 XP) için hedef öldürme sayısını yeniden hesapla; ekipman ve beceri açılışlarının dövüş kararlarını değiştirip değiştirmediğini gözle. |
| 41–50 | Mevcut 45. seviye becerilerini anlamlı kılan üçüncü dövüş ve ekipman kademesini, ayrıca 50. seviye karşılaşmasını ekle. | Sadece vuruş hasarını değil, mana giderinin sürdürülebilirliğini de kontrol et (örneğin birkaç 45. seviye hasar becerisi 195 mana istiyor). |
| 51–65 | 58 ve 65. seviyeler çevresinde en az iki alt aralıkla devam et; öldürme görevlerini `Q_Level65` ile ilişkilendir. | 50→51 (508,676 XP) ve 65→66 (1,155,776 XP) eşiklerini gerçek görev sıklığı ve oturum süresiyle birlikte modelle. |
| 66–80 | 70 ve 80. seviye son becerilerinin çevresine karşılaşmalar, son ekipman hedefi ve görev zincirine bir sonuç ekle. | `Q_Level80` tamamlanma ödülü olarak kalabilir; XP kısmına işlev kazandır veya yalnızca seviye tavanındaki davranış tasarlandıktan sonra değiştir. |

Her kademe bir tasarım varsayımıdır. `[PLACEHOLDER]` Eşya statları, mob hasarı/canı/XP'si, görev ödülleri, düşen eşyaların fiyatları, bekleme süreleri ve karşılaşma süresi; formül ile ölçülen dövüş kayıtlarına göre seçilmeli ve ancak oynanış testinden sonra dengelenmiş sayılmalı. Yeni eşya ve görev kimlikleri kendi kayıtlarında bulunmalı. Yeni öldürme görevlerinin hedefleri, erişilebilir bir spawner'ın gerçekten ürettiği arketiplere karşılık gelmeli. XP, görev ilerlemesi, ganimet ve kuşanma doğrulaması sunucu otoritesinde kalmalı.

## Sıradaki üç sınırları belirlenmiş kod dilimi

### 1. Tek seferlik ödül ve spawner aralığı hatalarını görünür kıl (kod tamamlandı; PIE bekliyor)

- **Oyuncu kararı/deneyimi:** Zor bir görevi bitiren oyuncu, vaat edilen ekipmanı güvenilir biçimde alır. İçeriği boş bir seviye aralığı sessizce erken oyun moblarının çiftliğine dönüşmez.
- **Girdi → çıktı:** `ClaimReward` sırasında, ödülleri vermeden veya görevi tamamlandı saymadan önce ekipman çantasında yer olup olmadığını kontrol et. Çanta doluysa açık bir hata göster ve görevi ödülü alınabilir durumda bırak. Tanımlanan seviye aralığında eşleşen normal arketip yoksa geçersiz aralığı bildir ve genel normal mob havuzundan seçim yapmak yerine hiçbir şey üretme. Geçerli mevcut aralıklar çalışmaya devam etsin.
- **Dosya kapsamı:** `Source/MyProject/Private/Components/MMOQuestComponent.cpp`, `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`, `Source/MyProject/Private/Core/MMOMobSpawner.cpp`; yalnızca API değişikliği gerekiyorsa bunların başlık dosyaları.
- **Sınır/hata durumları:** Ekipman ödülü olmayan görevler, ekipman çantası doluyken de alınabilsin. Gelecekte iki eşyalı ödül eklenirse her iki yuva da önceden ayrılmalı. Hiç satırı olmayan `[25,30]` aralığı Goblin değil tanı mesajı üretmeli. Parti XP'si ve görev kredisi sunucunun kontrolünde kalmalı.
- **Tamamlanma ölçütü:** İki durum da PIE içinde sunucu ve istemciyle gösterilebilmeli; kabul edilmiş görev ödülü dolu çanta yüzünden kaybolmamalı, boş spawner aralığı başka seviyeden mob üretmemeli. Bu dilimde denge sayıları değişmez.

### 2. Oynanabilir bir 25–30. seviye aralığı sun (P0 içerik)

- **Oyuncu kararı/deneyimi:** `Q_MagmaTyrant` sonrasında oyuncu yeni bir hedef seçebilir ve ana göreve devam ederken kullanışlı dört yuvalı bir seti yükseltebilir.
- **Girdi → çıktı:** 25–29 arasında gerçek seviyeleri bulunan `[PLACEHOLDER]` 2–3 normal arketip ve bir 30. seviye elit ekle. `Q_MagmaTyrant` tamamlanmasını ön koşul yapan kısa ve sıralı bir `KillSpecificMob` zinciri ekle. Bu aralıkta gereken seviyesi açıkça tanımlı silah/zırh/kask/bot ekle. 25–30 normal spawner'ı ve zorunlu elit spawner'ını yalnızca zemin/navmesh doğrulanmış konumlara yerleştir. Görev ekipman ödülleri verildikleri anda kayıtlı olmalı ve doğru seviye koşuluyla kuşanılmalı.
- **Dosya kapsamı:** `Source/MyProject/Private/Characters/MMOMobArchetype.cpp`, `Source/MyProject/Private/Quests/MMOQuestTypes.cpp`, `Source/MyProject/Private/Items/MMOItemDatabase.cpp`, `Source/MyProject/Private/Core/MMOGameMode.cpp`. `MMOMob.cpp` dosyasına yalnızca ölçülmüş ganimet veya XP davranışı gerektirirse dokun.
- **Denge yöntemi:** Mevcut 22. seviye normali (3,700 XP, 82 hasar) ve 24. seviye eliti (6,800 XP, 95 hasar) alt aralık referansları olsun. 25→26 (21,176) ile 30→31 (59,576) arasındaki XP eşiklerini, görev ödülü payını ve `[PLACEHOLDER]` seviye başına 12–25 öldürme hedefini birlikte hesapla. Canı ve hasarı ölçülen öldürme süresi ve hayatta kalma verisine göre seç; yeniden doğuş noktasında mob aggro'su olmaması açık koşul olsun. Elitin 6,800 XP ödülünü sorgulamadan normal mob ödülü olarak kullanma.
- **Tamamlanma ölçütü:** Eklenen her öldürme hedefi üretilebilir ve erişilebilir olsun. 24. seviye sonrası görev zinciri kabul, tamamlama ve ödül alma adımlarından geçsin. Dört yuvanın ekipmanı belirtilen seviyelerde kuşanılabilsin. Kaynak tablolarda eksik veya yinelenen kimlik bulunmasın. Solo ve parti PIE koşularında seviye başına öldürme, ölüm oranı ve altın akışı kaydedilsin. Haritada uygun alan yoksa veriyi tamamla; harita bağımlılığı çözülene kadar spawner'ı etkinleştirme.

### 3. Dövüş ilerlemesini 40. seviyeye kadar sürdür (P0 içerik)

- **Oyuncu kararı/deneyimi:** 31–40. seviyeler, mevcut 40. seviye kilometre taşından önce yeni dövüş tehditleri ve ekipman kararları sunar.
- **Girdi → çıktı:** 31–39 arasında `[PLACEHOLDER]` 2–3 normal arketip ve bir 40. seviye elit; 30. seviye elit görevinin tamamlanmasını ön koşul yapan sıralı öldürme zinciri; yeni bir dört yuvalı ekipman kademesi ve doğrulanmış oynanabilir alanda sınırlandırılmış spawner'lar ekle. `Q_Level30`/`Q_Level40` paralel, tek seferlik başarı ödülleri olarak kalsın.
- **Dosya kapsamı:** İkinci dilimdeki aynı dört C++ veri/spawn dosyası. Bu dilimde beceriler veya genel XP formülü yeniden yazılmaz.
- **Denge yöntemi:** Uygulanmış 30→31 için 59,576 XP ve 40→41 için 225,026 XP eşiklerini kullan. `[PLACEHOLDER]` görev XP'sini her seviye hedefinin açık bir payı olarak belirle; kalan XP ve gözlenen seviye başına öldürme sayısından normal mob XP'sini türet. Yeni ekipmanı 16. seviye Adamantine ile ve +8 yükseltme sonuçlarıyla karşılaştır: erken yükseltmeleri değersizleştirmeden yeni temel kademeyi tercih etmek için gerekçe oluşmalı.
- **Tamamlanma ölçütü:** 30. seviye görev tamamlanınca yeni zincir açılmalı. Gereken bütün öldürmeler dünyada mümkün olmalı. 40. seviyeye ulaşmak için yalnızca 24. seviye eliti tekrar tekrar öldürmek gerekmemeli. Dört sınıf da mevcut 33. seviye becerilerini ve yeni ekipmanı sunucu/istemci PIE koşusunda kullanabilmeli. 41–80. seviye değerlerini belirlemeden önce gözlenen ilerleme hızını kaydet.

## Revizyon kaydı

- 2026-09-28: Kaynak koda dayalı ilk plan.
- 2026-09-28: Plan proje sahibi için Türkçeye çevrildi. `[PLACEHOLDER]` işaretli sayısal hedefler henüz oynanışta sınanmadı.
- 2026-09-28: İlk kod dilimindeki iki hata `117e5fc` ile giderildi ve derlendi; oynanış doğrulaması bekliyor.
