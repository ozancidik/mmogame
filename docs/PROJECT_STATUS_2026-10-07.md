# MyProject — yaptık / yapacağız

**Güncelleme:** 7 Ekim 2026. Kaynak kod ve son tamamlanan test paketi esas alınmıştır.
Önceki kod/test kanıtı: `72419fa`; ilk gerçek ganimet ölçümü ayrıca eklenmiştir
(ayrıntı: düzeltme kaydı §46).

**Ulaştığımız aşama:** UE 5.7 üzerinde C++ MMORPG sistemleri ve Lv1–80 içerik
tanımları bulunan, yerel kayıt ve iki haritalı çok oyunculu bağlantısı test edilen
bir prototip. Lv1–30 akışının baştan sona oynanış, süre ve denge doğrulaması açık.
Toplam tamamlanma yüzdesi hesaplanmadı; içerik tanımı ile oynanarak doğrulanmış
ürün farklı kanıtlar gerektirir.

## 1. Proje iskeleti ve ilerleme tablosu

| Katman / sistem | Yaptık | Mevcut kanıt | Yapacağız |
| --- | --- | --- | --- |
| Motor ve kod yapısı | UE 5.7, `Source/MyProject` C++ modülü; Blueprint görsel/içerik katmanı; Enhanced Input | Editor hedefi derlendi; 15 kritik dosya PCH olmadan /W4 ile 0 uyarı | Gerçek Server hedefi ve Shipping paketini ayrı derlemek |
| Karakter ve statlar | STR/DEX/INT/VIT, can/mana, XP/seviye, stat puanı, ölüm/diriliş; sunucu otoritesi | Kayıt stat/XP/puan bütçesi ve dört sınıf için 15 UE kontrolü; ölüm kuralları ayrı fixture'larda | Gerçek ölüm/respawn, kayıt sonrası istemci stat paneli ve uzun oturum |
| Savaş ve PvP | Hedefleme, normal saldırı, beceriler, güvenli hub, PvP/parti/klan kuralları; hasar/şifa bildirimi | Kod/derleme; GameState'te PvP aynası | İki oyuncuda gerçek hasar, gecikme, güvenli bölge ve görsel sayı kontrolü |
| Sınıf ve beceri | Dört sınıf, mastery/rütbe, öğrenme, hotbar, mana/cooldown; kayıtlı rütbe + bakiye ortak bütçesi, sınıf iadesinde fazla puan önleme | 81 beceri; yeni bütçe/iade regresyonu 12 UE kontrolü | Gerçek yatırım/kayıt/sınıf RPC ve HUD; LearnedSkills maliyet denetimi; sınıf dengesi, Lv33+ beceri boşlukları |
| Efekt ve yenilenme | 12 buff/debuff tanımı; replike bitiş zamanı; can/mana yenilenmesinde 0,5 sn adım | Veri/kod/derleme | İstemci efekt süresi ve Network Profiler bant ölçümü |
| Ekipman ve envanter | 53 ekipman; InstanceId ile kuşanma/satış/yükseltme; çanta/yığın sınırları; nadirlik/+ | Kaydedilmiş ekipman seviye şartı: 11 UE kontrolü | Gecikmeli çift tıklama, uzak HUD, gerçek dosyadan yeniden giriş |
| Satıcı ve ekonomi | Ekipman/iksir alış-satışı; yalnız teslim edilen adet kadar ücret; ölü/uzak işlem reddi | Yeni satıcı regresyonu: 14 kontrol geçti | Gelir/harcama dengesi, uzun oturum, gerçek RPC ve kayıt kalıcılığı |
| İksir/parşömen | Ortak bekleme, ölümde ret; dolu can/manada faydasız saf iksir korunur | 11 UE kontrolü | Gecikmeli sahip-istemci kullanımı ve HUD/adet eşleşmesi |
| Görev | 79 tanım, NPC kabul/teslim, ön koşul/ilerleme/ödül; ölüm/kapasite korumaları ve kabul/teslim telemetrisi | 8 yaşam durumu + 10 kapasite/tek teslim UE kontrolü; 2 başarılı kabul / 1 teslim telemetrisi | Lv1–30 zincirini gerçek avla tamamlamak; dolu çanta/yığın HUD'ı, uzak istemci ölçümü |
| Mob, elit ve ganimet | 51 arketip (13 elit), 29 spawner tanımı; XP/katkı paylaşımı; loot rezervasyonu/kapasite; ilk gerçek pozitif teslim ölçümü | Ganimet ölüm/tek grant için 10 kontrol + ilk loot için 6 kontrol ve üç türde teslim logu | Fiziksel overlap/timer, gerçek parti kredi ve boss/loot oynanışı |
| Haritalar | Summer ve Winter; hub/NPC/spawner yerleşimleri, nav/doğuş çalışmaları | Son smoke: Summer 47, Winter 28 mob doğumu; iki haritada 0 hata | Cep rotaları, zemin, erişim, leash/aggro ve Lv1 doğuş güvenliği |
| Başlangıç ve rehber | Yeni kayıt dalında Kısa Kılıç + Deri Zırh + 3 can iksiri; sınıf seçimi ve 8 bağlamsal HUD ipucu | Kodda mevcut; ipucu maskesi yerel oyuncunun oturumunda tutulur | İlk 10 dakika kullanıcı koşusu; kalıcı rehber ilerlemesi gerekiyorsa kayıt tasarımı |
| Parti | Üyelik/menzil/XP paylaşımı, lider kuralları; parti birleşmesinde lider onayı | Lider/rıza regresyonu: 10 kontrol | Çok pencerede birleşme/ayrılma, gerçek katkı ve XP paylaşımı |
| Ticaret ve depo | Çift onay/teklif revizyonu, kimlikle eşya, kapasite/altın tavanı; NPC yakınlığında depo | Kod/derleme; satış kapasitesi yeni satıcı testinde | Ticaretin iki kaydı birlikte kalıcılaştırması; çökme/bağlantı kopması; gerçek depo RPC |
| Klan, arkadaş ve sohbet | Klan üyelik/liderlik/davet, arkadaş listesi, sohbet ve hız sınırları | Klan davetinde güncel lider yetkisi: 9 kontrol | Çevrimdışı üyelik/rıza, yeniden giriş ve çok oyunculu sosyal akış |
| Yerel kayıt / JSON | Store arayüzü, yerel .sav, JSON v2 ve sıkı giriş doğrulaması; yazım numarası takibi | 111 JSON negatif test; 274 yuva okundu; JSON eşleşmesi ve seçilen kayıt roundtrip'i geçti | Gerçek Lv30+ kayıtlar; bulut adaptörü, depoda sıra garantisi/iptal ve işlem bütünlüğü |
| Bölge geçişi | Asenkron onay bariyeri, timeout/iptal, eski/çift callback koruması; son XP/altını yeniden yazma | Normal/timeout/durum/yazım hatası/ilerleme testleri + callback için 9 kontrol | Son kayıt–pawn yok oluş penceresi; gerçek gecikmeli mağaza ve motor yükleme hatası |
| Ağ / replikasyon | Sunucuda mutasyon; kişisel alanlarda owner-only; 1 host + 1 istemci bağlantısı | NetSmoke: 1 join, 1 welcome, iki tarafta 0 hata | Gerçek iki oyunculu oyun, paket gecikmesi/kaybı, yük ve bant ölçümü |
| Hesap ve güvenlik | Yükleme/işlem doğrulamaları, debug Shipping kapıları ve çeşitli kötüye kullanım düzeltmeleri | Derleme + sınırlı regresyon kanıtları | Ad bağımlı kayıt kimliğini hesap/karakter kimliğine ayırmak; kayıt imzası/yetkilendirme |
| Dedicated / backend | Admin kapısı merkezileştirildi; kayıt arayüzü ve GameState hazırlığı mevcut | Editor hedefinde derleniyor | Kaynak UE 5.7, Server hedefi/paketleme, hesap servisi, gerçek asenkron depolama |
| Telemetri ve denge | Kill/death/levelup/gold, görev XP/süre; controller oturumunda ilk görev kabul/teslim ve gerçek loot süreleri | 6 UE oturum kontrolünde ortak tekillik + 6 ilk loot kontrolü; parser 16 görev/13 loot olayı ve yanlış teslim miktarını yakalayan negatif test | İlk öldürme/kuşanma; gerçek solo/parti süreleri, model karşılaştırması ve ardından denge ayarı |

**Test sınırı:** Bir satırdaki fixture sonucu yalnız belirtilen senaryoları kapsar.
NetSmoke bağlantıyı kontrol eder. Smoke harita yüklenmesi ve doğuş loglarını kontrol
eder. Bunlar gerçek kullanıcı oynanışı, tüm sistemlerin replikasyonu veya üretim
sunucusu kapasitesi için kanıt oluşturmaz.

## 2. Son tamamlanan işler

| Commit | Tamamlanan iş | Doğrulama |
| --- | --- | --- |
| `b87dac3` | Bölge geçişinde ilk snapshot sonrası XP/altını son kayda alma | İlerleme testinde değişim/XP/altın/başlama/kilit temizleme geçti |
| `4204872` | Ölü karakterin görev kabul/ödülünü reddetme; ödülü dirilişe koruma | 8 kontrol |
| `16a741e` | Ölü karakterin ganimet toplamasını engelleme | 10 kontrol |
| `a89931a` | Can/mana doluyken saf iksir ve cooldown harcanmasını önleme | 11 kontrol |
| `c41741e` | Kayıtlı ekipmanda seviye şartı; eşyayı kayıpsız çanta/depo kurtarması | 11 kontrol |
| `6152cc8` | Satıcı ekonomi kurallarını otomatik UE paketine alma | 14 kontrol; oynanış kuralı değişmedi |
| `5ff77d5` | Görev kabul/teslim logları; nominal XP ve süre raporu | 8 UE kuralı + olay tekilliği; eski/yeni parser fixture'ları; 23/23 paket |
| `832b1f9` | Mevcut kayıt stat/XP/puan kurallarını otomatik pakete alma | 15 UE kontrolü; puan clamp'i bozulunca test hatayı yakaladı; 24/24 paket |
| `75805c7` | Rütbe + bakiye ortak bütçesi; sınıf iadesinde fazla puan ve toplama taşmasını önleme | Önce FAIL checks=1, düzeltme sonrası 12 UE kontrolü; 25/25 paket |
| `416e5f4` | Controller oturumunda ilk kabul/teslim; pawn/aktarımda tekillik, parser süre/eksik ölçüm raporu | 6 UE oturum kontrolü + 8 görev kuralı/hook; parser 16 olay ve negatif kontrol; 26/26 paket |
| `72419fa` | Mevcut yığın/çanta kapasitesi kurallarını regresyona alma; yer açıp tam ödül ve tekrar teslim | 10 UE kontrolü; kapasite koruması bozulunca test ilk kontrolde hatayı yakaladı; 27/27 paket; oynanış kuralı değişmedi |
| 7 Ekim ilk ganimet (bu belgeyle aynı commit) | İlk gerçek pozitif loot teslimi; controller ömründe tek süre ve gerçek adet/altın | 6 yeni UE kontrolü; ölüm/oturum testleri genişletildi; yanlış miktar negatif testi yakalandı; parser 13 olay; 28/28 paket |

## 3. Sıradaki çalışma sırası

| Öncelik | İş | Tamamlandı sayılma koşulu |
| --- | --- | --- |
| P0 | Doğrulanmış sınırlı bugları ve kalan riskli kayıt/ekonomi yollarını ele almak | Önce hata kanıtı; düzeltme sonrası UE regresyonu, derleme ve ilgili paket geçer |
| P0 | İlk iki oyunculu PIE koşusu: doğuş → görev → loot → ekipman → kayıt/yeniden giriş | Host/uzak istemci için açık geçti/kaldı notları ve gerçek loglar |
| P1 | Kayıt kimliği/hesap ve işlem bütünlüğü | Sunucunun doğruladığı kimlik; başka karakter kaydını yükleme/ezme reddi; göç ve hata testleri |
| P1 | O6 kalan geçiş penceresi ve gelecekteki depoda yazım sırası | Gecikmeli/başarısız/iptal edilen yazımda ilerleme geri sarılmaz; depoda atomiklik kanıtı |
| P1 | Lv1–30 ölçümünü tamamlamak | Görev/ilk eylem telemetrisi; dört sınıf solo ve parti koşusunda süre/ölüm/altın tablosu |
| P2 | Rehber, görev ritmi, beceri ve ekonomi ayarı | İlk 10 dakika kullanıcı testi; ayarlar gerçek ölçüme dayanır |
| P2 | Dedicated doğrulaması | Kaynak motorla gerçek Server hedefi derlenir/paketlenir; ayrı istemcilerle çalışır |

Kullanıcı bilgisayar başında değilken kod, fixture, derleme ve belge işleri
ilerletilebilir. Gerçek PIE oyun koşuları yapılmış gibi işaretlenmez. Dedicated
derlemesi için kurulu Launcher motoruna ek olarak kaynak motor gerekir.

## 4. Son test paketi ve belge bağlantıları

7 Ekim'deki son tamamlanan `RunAllChecks.ps1 -SkipBuild -FullCompile`: **28/28 geçti**.
Normal Editor derlemesi ayrıca geçti. 15 dosyanın PCH olmadan /W4 derlemesinde
derleyici uyarısı yok. Veri denetiminde Lv33+ beceri boşluğu uyarıları, StaticLint'te
InitGameState override beyaz liste uyarısı açık. Yerel test edilen en yüksek gerçek
kayıt seviyesi 6; 274 kaydın okunması oyuncu sayısı veya Lv1–30 test tamamlanması değildir.
Son parser sayısal/eksik alan korumaları da bu tam pakette geçti.
İlk eylemler sunucu controller ömrüne aittir; kalıcı karakterin
ilk görevi veya gerçek 20 dakika koşusu olarak yorumlanmaz.
İlk loot yalnız pozitif pickup teslimidir; kısmi sarf toplamayı içerir,
ekipman kuşanma veya bütün ilk eylemler planının bitmesi anlamına gelmez.

- [Ayrıntılı düzeltme ve test kaydı](CHECKLIST_FIXES_2026-10-05.md)
- [Oynanacak kontroller](PLAYTEST_CHECKLIST.md)
- [İlk koşu yöntemi](PLAYTEST_RUNBOOK.md)
- [Lv1–30 tasarım ve ölçüm planı](VERTICAL_SLICE_LV1_30.md)
- [Ağ / dedicated sınırları](NETWORK_AUDIT.md)
- [Güvenlik ve hesap sınırları](SECURITY_AUDIT.md)

Agency-agents rol belgeleri geliştirme/review rehberi olarak kullanılıyor. Bu
oturumdaki kontrollerde bağımsız paralel ajan çalışmıyor; rol belgeleri ayrı bir
çalışan ajan veya oyunun runtime AI bağımlılığı olarak sayılmamalıdır.
