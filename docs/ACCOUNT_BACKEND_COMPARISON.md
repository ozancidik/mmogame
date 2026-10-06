# Hesap + kalıcı karakter arka ucu: Open World Server (OWS) ve PlayFab karşılaştırması

Tarih: 4 Ekim 2026. Durum: araştırma/plan, hiçbir kod değişmedi. Kaynak bağlantıları en altta.
Doğrulanamayan varsayımlar "[?]" ile işaretlidir ve bölüm 9'da toplanmıştır.

## 1. Bugünkü durum (koddan okunan)

- **Kimlik = oyuncu adı.** `AMMOCharacter::GetSaveSlotName()` (`Private/Characters/MMOCharacter.cpp` ~397) PlayerState adını
  temizleyip `MMOSave_<ad>` yapar (en çok 32 karakter, büyük/küçük harf duyarsız dosya sistemi). Şifre, hesap, sahiplik yok:
  aynı adı yazan herkes o karakteri açar.
- **Veri = tek struct.** `FMMOCharacterSaveData` (`Public/Save/MMOSaveGame.h`), `UMMOSaveGame` içinde, `SaveGameToSlot` ile yerel
  `.sav`. İçerik: sınıf/seviye/XP/statlar, altın, `Items` (yığınlar), `EquipmentBag`/`EquippedItems`/`Warehouse`
  (`FMMOEquipmentInstance`, `NextInstanceId` sayacı), skill/hotbar/mastery/cooldown (`TMap<FName,int64>`), görevler, klan adı +
  lider bayrağı, arkadaş adları (en çok 30), harita adı + konum.
- **Yazım zaten sunucu-otoriter.** `SaveCharacterForTravel()` `HasAuthority()` ve `bInitialLoadComplete` olmadan `false` döner;
  `LoadCharacter()` da yalnız otoritede çalışır. Yani "yalnız sunucu yazar" kuralı mantıken var, eksik olan sunucunun *nereye* yazdığı.
- **Harita geçişi tek süreçte.** `AMMOGameMode` bölge geçişi (Summer_scene <-> Winter_scene) tüm bağlı oyuncuları kaydedip
  `World->ServerTravel(...)` ile aynı sunucu sürecinde haritayı değiştirir (`MMOGameMode.cpp` ~100-160). Ayrı "dünya sunucusu"
  süreci yok; bütün oyuncular birlikte taşınır.
- **Kaydetme senkron.** `SaveCharacterForTravel()` `bool` döner ve geçiş buna bağlı. Bulut yazımı HTTP, yani asenkron olacağı için
  bu akış "yazıldı onayı gelmeden travel yok" biçiminde yeniden kurulmalı (bölüm 8).
- İlgili çağıranlar: otomatik kayıt 60 sn (`MMOCharacter.cpp` ~246), klan/arkadaş/ticaret sonrası `SaveCharacter()`,
  `ServerSaveCharacter`/`ServerLoadCharacter` RPC'leri (`MMOCharacter.h` ~317-320; istemci tetikli, hesap sistemi gelirse gözden geçirilmeli).

## 2. Adaylar (özet)

| | Open World Server (OWS) | PlayFab (Microsoft) |
|---|---|---|
| Ne | Kendin barındırdığın .NET + Docker mikroservis yığını: hesap/giriş, karakter kalıcılığı, UE sunucu örneği (zone instance) yönetimi | Barındırılan oyun arka ucu (SaaS): kimlik, oyuncu/karakter verisi, ekonomi, CloudScript (Azure Functions), liderlik vb. |
| Lisans | MIT (tam ticari kullanım, kaynak elinde) | SDK/eklenti Apache-2.0; **servis kendisi kapalı SaaS**, Microsoft şartlarına tabi |
| UE 5.7 | Resmi başlangıç projeleri yalnız **UE 5.5.0** ve 4.27.2. 5.6/5.7 resmî değil [?] | **Unreal Integration Bundle v2: "UE 5.7 ve üzeri" için** (PlayFabUnreal + OnlineSubsystemPlayFab); Marketplace eklentisi 5.6 ve 5.7 uyumu duyurulmuş |
| Bakım | Repo arşivlenmemiş, 415 yıldız, 3 açık issue, son push **30 Mart 2026**; son görünür commitler Şubat 2026 (saklı yordam -> parametreli sorgu dönüşümü, ValKey tabanlı UserSession sunucusu "work in progress") | Microsoft ürünü, Nisan 2026'da UE 5.7 odaklı yeni paket, eklenti reposu Eylül 2026'da güncellenmiş |
| Barındırma | Kendi sunucun (Docker; MSSQL/MySQL/Postgres, RabbitMQ, ek servisler) | Bulut; sunucu işletmezsin. Yalnız oyun sunucuların (UE dedicated) sende |
| Maliyet | Yazılım bedava; VPS/DB/yedekleme + işletme emeği | Geliştirme modu bedava ama **başlık başına 1.000 ömür boyu oyuncu**; PAYG $0/ay + kullanım; Standard $99/ay; Premium $1.999/ay |

## 3. Ölçüt ölçüt karşılaştırma

### 3.1 UE 5.7 uyumu ve bakım
- **OWS:** Çekirdek (.NET API'leri) motor sürümünden bağımsız; risk UE tarafındaki `OWSPluginUE5` eklentisinde. Resmî örnek 5.5.0;
  5.7'de derlenip derlenmediği doğrulanmadı [?]. Eklenti açık kaynak olduğundan 5.7'ye uyarlamak mümkün ama bakımı bize düşer.
  Proje tek geliştirici ağırlıklı görünüyor (bus factor riski) [?].
- **PlayFab:** Microsoft 5.7'yi doğrudan hedefleyen paket yayımladı (otomatik token yenileme, birleşik entity kimlikleri).
  Paket platform listesi: Windows, Xbox, Steam Deck. Dedicated server tarafında eklenti zorunlu değil: Server API düz REST,
  `FHttpModule` ile çağrılabilir (bölüm 8); böylece sunucu derlemesine Microsoft eklentisi sokmak şart olmaz.
- **Avantaj: PlayFab** (5.7 için birinci sınıf destek, OWS için resmî destek yok).

### 3.2 Lisans (ticari)
- **OWS:** MIT; bağımlılık olarak kullanılan paketlerin (RabbitMQ vb.) kendi lisansları ayrıca doğrulanmalı [?]. Kaynak elimizde olduğu için
  servis kapanma/koşul değişikliği riski yok.
- **PlayFab:** Eklenti Apache-2.0 (ticari serbest). Asıl bağ servise: fiyat ve kota Microsoft'un tek taraflı kararı. Bunun
  kanıtı yakın tarihli: Mart 2026'da geliştirme modu oyuncu sınırı üçüncü taraf kaynaklara göre 100.000'den 1.000'e indirildi
  (Microsoft fiyat sayfası bugün 1.000 diyor; "Foundation Mode" yalnız Xbox'ta yayımlayanlara).
- **Avantaj: OWS** (sahiplik/lock-in), ama PlayFab ticari kullanıma engel değil.

### 3.3 Kimlik doğrulama modeli
- **OWS:** Public API kayıt/giriş/oyuna bağlanmayı yapar; Xsolla/Google/Facebook gibi harici giriş sağlayıcıları için bir proje
  var. Kimlik bilgilerini kendi veritabanımızda (hash, sıfırlama e-postası, kilitleme, KVKK sorumluluğu) biz işletiriz. Servisler
  arası yetki "CustomerGUID" üstbilgisiyle; bunun istemciden mi sunucudan mı çağrılabildiği ayrımı belgelerde net değil [?].
- **PlayFab:** Hazır: `LoginWithEmailAddress`, `RegisterPlayFabUser`, `LoginWithCustomID` (misafir/cihaz), platform girişleri
  (Steam, Xbox vb.), parola sıfırlama. Client login bir **SessionTicket** döndürür; dedicated server bu bileti
  `Server/AuthenticateSessionTicket` ile (başlık **gizli anahtarı** `X-SecretKey` sunucuda) doğrulayıp PlayFabId alır.
  Sunucu tarafı kimlik de var: `LoginWithServerCustomId` (bizim "ad -> kimlik" geçiş köprümüz için işe yarar).
- **Avantaj: PlayFab** (parola/oturum güvenliğini devreder; şifre saklama sorumluluğu azalır).

### 3.4 Şema esnekliği (FMMOCharacterSaveData)
- **OWS:** Karakter ilişkisel şemada (Characters tablosu) ve geri kalan her şey `CustomCharacterData`: **anahtar/değer, değerler
  string**. Yani iç içe diziler (ekipman örnekleri, depo, görevler) için ya tek bir büyük JSON string alanı yazacağız ya da kendi
  tablolarımızı ekleyeceğiz (OWS'u fork edip şemayı genişletmek). Bu alanın maksimum uzunluğu doğrulanmadı [?].
  Envanter için OWS'un kendi envanter/yetenek tabloları var ama bizim `InstanceId` + rastgele özellikli ekipman modelimize
  uymuyor; kullanmayıp sadece JSON alanına gömmek mantıklı.
- **PlayFab:** Esnek JSON. Belgelerin yeni projeler için önerdiği yöntem **Entity Objects**: oyuncu/karakter varlığına bağlı
  küçük JSON nesneleri (`SetObjects`/`GetObjects`); ücretsiz katmanda bir varlığa en çok **3 nesne** (üst katmanlarda daha çok).
  Nesne başına boyut sınırını doğrulayamadım [?]. Eski yol Player Data/UserData (`Internal`/`ReadOnly`/`Client` üç erişim kipi).
  Planım: `character` varlığı başına tek `CharacterSave` JSON nesnesi (gerekirse Items/Warehouse/Quests olarak 3'e bölünür).
  `FMMOCharacterSaveData`'yı `FJsonObjectConverter` ile JSON'a çevirmek gerekir; `TMap<FName,int64>` ve `int64` (JSON'da
  string'e dönüşür) için gidiş-dönüş testi şart [?].
- Sürümleme: yapıda "eski kayıtlarda boş gelir" desenini zaten kullanıyoruz; JSON'a `SchemaVersion` alanı ekleyip aynı deseni sürdürürüz (iki seçenekte de).
- **Avantaj: PlayFab** (blob/JSON doğal). OWS'ta da yapılır ama ilişkisel şemayla savaşırız.

### 3.5 Sunucu-otoriter yazım (yalnız dedicated server yazsın)
- **PlayFab:** Sağlam model. Player Data belgesi üç kip tanımlıyor: *Client* (istemci okur/yazar), *Read Only* (**yalnız sunucu yazar**,
  istemci okuyabilir), *Internal* (yalnız sunucu görür). Biz karakter blob'unu sunucu API'siyle `UpdateUserInternalData`
  (ya da sunucu yetkisiyle entity `SetObjects`) yazarız; **gizli anahtar yalnız dedicated server ortamında** durur, istemci paketine girmez.
  Başlık ayarlarında istemcinin veri/istatistik yazmasının kapalı olması gerekir (Game Manager API ayarları) [?]. Entity
  nesnelerinin erişim ilkesi ayrıca doğrulanmalı; şüphede Internal Data kullan [?].
- **OWS:** API'ler hem Public hem Character Persistence/Instance Management olarak ayrı; istemci için "cosmetic" gibi güvenli
  olmayan uç noktalar bile tanımlı (istemciden çağrılması amaçlı). Yazımın yalnız sunucuya kısıtlanması **ağ düzeyinde**
  (Persistence/Instance API'lerini internete açmamak, güvenlik duvarı) ve CustomerGUID'in istemcide bulunmamasıyla sağlanmalı;
  istemci-çağrılabilir uç noktaların tam listesi incelenmeli [?].
- **Avantaj: PlayFab** (hazır, belgelenmiş ayrım). OWS'ta ayrım bizim işletme disiplinimize bağlı.

### 3.6 Birden çok dünya sunucusu / harita geçişi (Summer/Winter)
- **OWS:** Asıl gücü burası. Instance Launcher host üzerinde UE dedicated süreçlerini istek üzerine açar/kapatır; Instance Management
  hangi bölge örneğine bağlanılacağını söyler (`GetServerToConnectTo`); bölge başına ayrı süreç, yük dengeleme, aynı bölgenin
  birden çok örneği. Dokümandaki hedef 100.000 eşzamanlı oyuncu; bölge başına sınır UE'nin kendi sınırı. İlk bağlanışta sunucu açılışı
  için ~15 sn bekleme tarif ediliyor. Kurulum: UDP 7777-7787 aralığı, Docker, DB.
- **PlayFab:** Süreç yönetmez. Harita geçişini biz çözeriz: iki kalıcı dedicated süreç (Summer, Winter), adresleri Title Data'da; geçişte
  kaynak sunucu kaydedip onay bekler, istemci hedef sunucuya `ClientTravel` yapar, hedef sunucu bileti yeniden doğrular ve veriyi buluttan
  okur. PlayFab Multiplayer Servers (MPS) var ama oturum-tabanlı maç sunucuları içindir; kalıcı bölge mimarisine ağır ve gereksiz [?].
- **Avantaj: OWS** süreç orkestrasyonunda. Ancak bugün yalnız iki sabit harita var; sabit iki süreç yeterli, orkestrasyon ihtiyacı
  henüz doğmadı.

### 3.7 Maliyet
- **OWS:** Lisans 0. Maliyet: yığın için VPS (API'ler + DB + RabbitMQ + ek servisler) ve bizim UE sunucularımız; yedekleme, güncelleme,
  izleme, güvenlik yaması, olay müdahalesi bizim işimiz. Gerçek aylık rakamı bu belgede yazmıyorum, ölçülmedi [?].
- **PlayFab:** Geliştirme/test: bedava ama 1.000 ömür boyu oyuncu sınırı (10 başlık); çıkışta PAYG $0/ay + kullanım ya da Standard
  $99/ay (profil okuma/yazma, depolama, PlayStream olayı kotaları dahil). Bizim kullanım örüntüsü: oyuncu başına dakikada ~1 yazım
  (otomatik kayıt) + giriş/geçiş okumaları; kota hesabı yapmadım, PAYG birim fiyatlarına bakmadım [?]. CloudScript
  (Azure Functions) kullanırsak Azure faturası ayrı. Oyun sunucu barındırması iki seçenekte de aynı.
- **Genel:** küçük ölçekte PlayFab (işletme maliyeti sıfır) daha ucuz; büyük ölçekte kullanım faturası kritik hale gelebilir.

### 3.8 Kendi barındırma / bulut
- OWS = tam kendi barındırma (veri senin elinde, KVKK konumu kontrol edilebilir, ama 7/24 işletme yükü).
- PlayFab = bulut. Veri yerleşimi bölgesi/KVKK/GDPR için başlık bölgesi ve e-posta saklama konusu ayrıca değerlendirilmeli [?].

### 3.9 Geçiş zahmeti (mevcut .sav'dan)
- İki seçenekte de aynı çekirdek iş: `FMMOCharacterSaveData` <-> JSON. `GetSaveSlotName()` ad-tabanlı kimlik yerine `CharacterId`'ye geçer.
- **Eski kayıtlar sahipsiz:** `.sav`'larda hesap yok. Güvenli yol: içe aktarma aracı her `MMOSave_*.sav`'ı *sahipsiz* karakter olarak
  sahiplenme koduyla (admin üretir) yükler; kod olmadan "ilk gelen alır" olur. Geliştirme aşamasında pratik yol: elle az sayıda hesap açıp
  karakterleri o hesaplara atamak.
- Klan ve arkadaş listeleri **ad (string)** tutuyor. Karakter adı benzersiz kalacağı sürece değişmeden çalışır; bu yüzden ad benzersizliği
  arka uçta zorlanmalı (PlayFab'ta karakter adı için yerleşik benzersizlik garantisi yok, hesap görünen adı başlık genelinde benzersiz
  olabilir; karakter adı için ek bir mekanizma lazım [?]; OWS'ta DB kısıtı ile yapılır [?]).
- Asenkron kaydın eklenmesi (`SaveCharacterForTravel` senkron `bool`) iki seçenekte de aynı refactor.

### 3.10 Güvenlik (token, hile)
- **PlayFab:** Kısa ömürlü SessionTicket/EntityToken, gizli anahtar yalnız sunucuda; çifte giriş denetimi için "karakter kilidi" (hangi
  sunucu süreci aktif) bizim eklememiz gereken kısım. Entity nesne yazımında `ExpectedProfileVersion` ile iyimser eşzamanlılık
  (çift kayıt / duplication açığına karşı) mümkün görünüyor [?]. Zayıf nokta: gizli anahtar sızarsa tüm başlık risk altında; anahtarı
  dedicated server yapılandırmasında tutup depoya koymamak ve dönüşümlü yenilemek gerekir.
- **OWS:** Güvenlik yüzeyini biz tanımlarız: TLS, API'lerin iç ağda tutulması, CustomerGUID yönetimi, parola hash politikası, oran sınırlama,
  yamalar. Tam kontrol ve tam sorumluluk.
- Her iki durumda hile açısından asıl kazanç aynı: *istemci veri yazmaz*, sunucu otoritesi korunur. Mevcut kod zaten otoritede kaydediyor.

## 4. Puan tablosu (bu proje için, 1-5; öznel)

| Ölçüt | OWS | PlayFab |
|---|:-:|:-:|
| UE 5.7 uyumu / bakım | 2 | 5 |
| Lisans / sahiplik | 5 | 3 |
| Kimlik doğrulama hazırlığı | 3 | 5 |
| Şema esnekliği | 3 | 4 |
| Sunucu-otoriter yazım netliği | 3 | 5 |
| Çoklu dünya sunucusu | 5 | 2 |
| Maliyet (şimdi / küçük ölçek) | 3 | 5 |
| Maliyet (büyük ölçek) | 4 | 3 |
| Geçiş zahmeti | 3 | 4 |
| İşletme yükü (düşük = iyi) | 2 | 5 |

## 5. ÖNERİ

**PlayFab ile başla, ama arka ucu bir arayüzün arkasına koy.**

Gerekçe, bu projenin bugünkü gerçek ihtiyacına göre:
1. Eksik olan şey *hesap + güvenli kalıcılık*; harita orkestrasyonu değil (iki sabit harita var). OWS'un asıl farkı (süreç yönetimi)
   şu an kullanılmayacak, buna karşılık .NET + Docker + DB + RabbitMQ yığınını işletme yükü ve UE 5.5'te kalmış eklenti riski kalıcı.
2. PlayFab UE 5.7'yi resmî hedefliyor, parola/oturum güvenliğini devralıyor ve "yalnız sunucu yazar" modeli (Internal/Read Only veri +
   gizli anahtar) bizim zaten otoritede kaydeden mimarimize birebir oturuyor.
3. Şemamız JSON blob'a doğal olarak sığıyor; `FMMOCharacterSaveData` üzerinde ilişkisel yeniden tasarım gerekmiyor.
4. Kilitlenme riski (fiyat/kota politikası, Mart 2026 örneği) arayüzle sınırlanır: `IMMOCharacterStore` (LoadByCharacterId /
   SaveByCharacterId, asenkron, tamamlanma geri çağrısıyla) arkasında `LocalFile` (bugünkü .sav, geliştirme/yedek) ve `PlayFab`
   uygulamaları. Sonradan OWS'a ya da kendi Postgres servisine geçmek yalnız yeni bir uygulama yazmak demektir.

OWS'u şu durumda yeniden değerlendir: aynı haritanın birden çok örneği / otomatik sunucu aç-kapa gerçekten gerekirse, ya da PlayFab
fiyatı ölçekte kabul edilemezse. O noktada yalnız Instance Launcher/Instance Management fikrini alıp kendi çözümümüzle birleştirmek de bir seçenek.

## 6. Üç aşamalı geçiş planı (en küçük çalışan dilim önce)

### Aşama 1: Sunucu tarafı bulut kaydı (kimlik hâlâ ad; istemci değişmez)  [en küçük dilim]
- **Hazırlık durumu (2026-10-06):** `IMMOCharacterStore`, yerel dosya uygulaması ve
  `UGameInstanceSubsystem` erişim noktası eklendi; kayıt okuma/yazma çağıranları bu arayüze
  taşındı. `FMMOCharacterSaveJson` sürüm 1 zarfı (`SchemaVersion` + `Data`) tüm
  `FMMOCharacterSaveData` alanlarını JSON'a çeviriyor ve bilinmeyen ileri sürümleri reddediyor.
  Gerçek UE yansımasıyla çalışan `Scripts/TestCharacterSaveJson.ps1`, dolu fixture'ın tüm
  alanlarını (özellikle `TMap<FName,int64>`) gidiş-dönüşte karşılaştırıyor; ölçülen örnek blob
  1158 UTF-8 bayt. PlayFab HTTP uygulaması, asenkron kuyruk/lazy migration ve travel onayı
  bu hazırlık diliminden sonra hâlâ yapılacak.
- `IMMOCharacterStore` arayüzü + `LocalFile` uygulaması (mevcut davranış) + `PlayFab` uygulaması (dedicated server, REST,
  `FHttpModule`; eklenti şart değil).
- Sunucu `LoginWithServerCustomId` ile ad-tabanlı sunucu kimliğiyle (`legacyname:<ad>`) giriş yapar, blob'u karakter başına yazar/okur.
  `FMMOCharacterSaveData` <-> JSON dönüştürücü + `SchemaVersion`.
- Önce yerel .sav'ı yaz, bulut yazımı arkada; okumada bulut varsa onu tercih et, yoksa .sav'dan yükle ve bulutta oluştur ("lazy migration").
- Bölge geçişini asenkron onaya bağla: kaydın tamamlanma geri çağrısı gelmeden `ServerTravel` yapma (şu an senkron `bool` bekliyor).
- Dirty-flag + 60 sn otomatik kaydı oran sınırına uygun biçimde sıkıştır (aynı veriyi tekrar yazma).
- Çıkış ölçütü: JSON gidiş-dönüş eşitlik testi (tüm alanlar, `TMap<FName,int64>` dahil); gerçek blob boyutu ölçümü; yazım gecikmesi ölçümü;
  istek başarısızlığında .sav'a düşme.
- Bu aşama güvenliği artırmaz (kimlik hâlâ ad) ama bütün teknik belirsizlikleri ucuza çözer (boyut, gecikme, JSON, kota).

### Aşama 2: Gerçek hesaplar (e-posta + parola) ve karakter kimliği
- İstemci giriş ekranı: `RegisterPlayFabUser`/`LoginWithEmailAddress` (+ isteğe bağlı `LoginWithCustomID` misafir). Alınan SessionTicket,
  dedicated server'a bağlantı seçenekleri/giriş mesajıyla gider; `AMMOGameMode::PreLogin`'de sunucu `AuthenticateSessionTicket` ile doğrular.
- `GetSaveSlotName()` yerine `CharacterId` (hesap başına karakter listesi, karakter yaratma ekranı, benzersiz karakter adı rezervasyonu).
- Aynı hesabın çift girişini reddet; karakter kilidi (aktif sunucu + zaman damgası) bulutta tutulur.
- Eski .sav içe aktarma: sahiplenme koduyla (veya elle atama) hesaplara bağla. İstemci tarafı `ServerLoadCharacter`/`ServerSaveCharacter`
  RPC'leri ve MMOSave/MMOLoad komutları ya kaldırılır ya da yalnız yönetici/sunucu için bırakılır.
- Gizli anahtar yalnız sunucu yapılandırmasında (depoya girmez, istemci paketine girmez); istemci başlık ayarlarında veri yazımı kapalı.
- Çıkış ölçütü: iki istemciyle: yanlış parola reddi, aynı hesaba ikinci giriş reddi, başka hesabın karakterini açamama, kaydın sadece sunucudan yazıldığının kanıtı.

### Aşama 3: Çok süreçli dünya (Summer ve Winter ayrı sunucu) ve işletme sertleştirme
- İki kalıcı dedicated süreç; adresler Title Data'da. `AMMOGameMode`'daki toplu `ServerTravel` akışı oyuncu-başı geçişe dönüşür: kaynak
  sunucu kaydeder + onay bekler -> karakter kilidi hedef sunucuya devredilir -> `ClientTravel` -> hedef sunucu bileti yeniden doğrular,
  veriyi okur. (Bugünkü "tüm oyuncular canlı ve savaş dışı olmalı" ön koşulu oyuncu-başı hale gelir.)
- Çökmeye dayanıklılık: kilit zaman aşımı, yarım geçiş kurtarma, kayıt tekrar denemeleri (idempotent), `ExpectedProfileVersion` ile çift yazım koruması.
- İzleme: kayıt hatası/gecikme sayaçları, kota kullanım uyarısı, günlük bulut yedeği (JSON dışa aktarma).
- Yük ya da örnekleme ihtiyacı doğarsa burada OWS Instance Launcher fikri (ya da kendi sürecimiz) değerlendirilir.

## 7. Basit ölçek notu

Bugünkü kod tek sunucu sürecinde çalışıyor; Aşama 1-2 için tek dedicated server yeterli. PlayFab kotası açısından en büyük yük: oyuncu
başına dakikada bir otomatik kayıt. Dirty-flag ile gereksiz yazımlar kesilirse hem kota hem gecikme rahatlar.

## 8. Teknik notlar (uygulama sırasında dikkat)

- Sunucu -> PlayFab çağrıları `FHttpModule` ile, zaman aşımı + üstel geri çekilme + kuyruk (oyun iş parçacığını bloklama).
- Yazım kuyruğu karakter başına sıralı olmalı (önceki yazım bitmeden yenisi gönderilmesin); çıkışta (logout/travel) "son yazım" bayrağı.
- Sunucu çökmesinde veri kaybı penceresi = son başarılı yazımdan sonrası; mevcut 60 sn aralığı korunur, kritik olaylarda (ticaret, klan,
  ekipman kazanımı) anında yazım (mevcut `SaveCharacter()` çağrıları zaten bu noktalarda).
- Hesap e-postası kişisel veri: KVKK/GDPR için gizlilik metni, silme isteği akışı (hesap + karakter verisi silme) planlanmalı.

## 9. Doğrulanmamış varsayımlar

1. OWS'un UE 5.6/5.7'de derlenip çalıştığı: resmî sayfalar yalnız 5.5.0 ve 4.27.2 diyor. Bir arama özeti "5.7.X" geçirdi ama resmî
   başlangıç sayfasında yok; kanıt sayma.
2. OWS'ta hangi uç noktaların istemciden çağrılabildiği ve `CustomerGUID` ayrımının güvenlik anlamı (API belge sayfasına erişilemedi).
3. OWS `CustomCharacterData` değer alanı maksimum uzunluğu; OWS'ta karakter adı benzersizlik kısıtı.
4. PlayFab entity nesnesi başına boyut sınırı ve ücretli katmandaki nesne sayısı; Internal Data anahtar başına güncel sınır (eskiden 100 KB'tı).
5. PlayFab pay-as-you-go birim fiyatları ve bizim kullanım tahminimizin kotaya oranı (hesaplanmadı).
6. PlayFab başlık ayarlarında istemci veri yazımını kapatma seçeneklerinin tam adları; entity nesne erişim ilkesinin istemci yazımını nasıl kısıtladığı.
7. `ExpectedProfileVersion` ile iyimser eşzamanlılık davranışı (SetObjects).
8. Karakter adı benzersizliği için PlayFab'ta yerleşik çözüm olmadığı (bu belgede "yok" varsayıldı; bir Azure Function veya ek tablo gerekebilir).
9. `FJsonObjectConverter` ile `FMMOCharacterSaveData` (özellikle `TMap<FName,int64>`, `int64`, `FVector/FRotator`, `EMMOClass` enum) gidiş-dönüşü: denenmedi.
10. PlayFab bölgesi/veri yerleşimi ve KVKK uygunluğu; Türkiye'den gecikme (denenmedi).
11. Dedicated server hedef işletim sistemi (Windows/Linux) belirtilmedi; PlayFab paketi platform listesi Windows/Xbox/Steam Deck. REST yolu bunu aşıyor ama denenmedi.
12. Üçüncü taraf bloglardaki "free tier %99 küçüldü" haberi Microsoft'un kendi fiyat sayfasındaki "1.000 ömür boyu oyuncu" ifadesiyle uyumlu; geçmiş değer (100.000) yalnız üçüncü taraftan.
13. Hiçbir eklenti/SDK bu depoda derlenmedi ya da denenmedi; bu belge yalnız belge ve repo okumasına dayanır.

## 10. Kaynaklar

- OWS repo (MIT, bileşenler, DB dizinleri, eklentiler): https://github.com/Dartanlla/OWS ; GitHub API meta verisi https://api.github.com/repos/Dartanlla/OWS
- OWS ana sayfa / SSS / UE 5 başlangıç projesi: http://www.openworldserver.com/ , http://www.openworldserver.com/faq.html , http://www.openworldserver.com/getting-started/starter-project/ue5.html
- OWS karakter kalıcılığı (CustomCharacterData, X-CustomerGUID; eski v1 belge): http://rpgwebapi.sabredartstudios.com/Docs/API (erişilemedi, yalnız arama özeti)
- PlayFab fiyat: https://developer.microsoft.com/en-us/games/products/playfab/pricing/
- PlayFab Unreal Integration Bundle v2: https://developer.microsoft.com/en-us/games/articles/2026/04/playfab-unreal-integration-bundle-v2/
- PlayFab Player Data (Client/Read Only/Internal): https://learn.microsoft.com/en-us/gaming/playfab/player-progression/player-data/
- PlayFab Entity Objects: https://learn.microsoft.com/en-us/gaming/playfab/features/entities/entity-objects
- PlayFab Server API kimlik doğrulama (AuthenticateSessionTicket, LoginWithServerCustomId): https://learn.microsoft.com/en-us/rest/api/playfab/server/authentication
- PlayFab Unreal Marketplace eklentisi (Apache-2.0): https://github.com/PlayFab/UnrealMarketplacePlugin
- Ücretsiz katman değişikliği (üçüncü taraf, ikincil): https://accelbyte.io/blog/playfab-just-cut-its-free-tier-by-99.-heres-what-that-means-for-your-game
