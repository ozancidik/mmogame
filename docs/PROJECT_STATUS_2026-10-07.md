# MyProject — yaptık / yapacağız

**Güncelleme:** 7 Ekim 2026. Kaynak kod ve son tamamlanan test paketi esas alınmıştır.
Önceki kod/test kanıtı: `b3d352b`; ilk kabul edilen temel saldırı da
sunucu controller oturumunda, hasar/XP öncesinde ölçülüyor (düzeltme kaydı §52).

**Ulaştığımız aşama:** UE 5.7 üzerinde C++ MMORPG sistemleri ve Lv1–80 içerik
tanımları bulunan, yerel kayıt ve iki haritalı çok oyunculu bağlantısı test edilen
bir prototip. Lv1–30 akışının baştan sona oynanış, süre ve denge doğrulaması açık.
Toplam tamamlanma yüzdesi hesaplanmadı; içerik tanımı ile oynanarak doğrulanmış
ürün farklı kanıtlar gerektirir.

## 1. Proje iskeleti ve ilerleme tablosu

| Katman / sistem | Yaptık | Mevcut kanıt | Yapacağız |
| --- | --- | --- | --- |
| Motor ve kod yapısı | UE 5.7, `Source/MyProject` C++ modülü; Blueprint görsel/içerik katmanı; Enhanced Input | Editor hedefi derlendi; 16 kritik dosya PCH olmadan /W4 ile 0 uyarı | Gerçek Server hedefi ve Shipping paketini ayrı derlemek |
| Karakter ve statlar | STR/DEX/INT/VIT, can/mana, XP/seviye, stat puanı, ölüm/diriliş; sunucu otoritesi | Kayıt stat/XP/puan bütçesi ve dört sınıf için 15 UE kontrolü; ölüm kuralları ayrı fixture'larda | Gerçek ölüm/respawn, kayıt sonrası istemci stat paneli ve uzun oturum |
| Savaş ve PvP | Hedefleme, normal saldırı, beceriler, güvenli hub, PvP/parti/klan kuralları; hasar/şifa bildirimi | Kod/derleme; GameState'te PvP aynası; temel saldırı için 21 native kontrol, ret kapıları/PvP/XP öncesi kayıt | İki oyuncuda gerçek hasar, gecikme, güvenli bölge ve görsel sayı kontrolü |
| Sınıf ve beceri | Dört sınıf, mastery/rütbe, öğrenme, hotbar, mana/cooldown; rütbe + bakiye bütçesi; kayıtlı ücretli beceriyi minimum ağaç rütbesine taşıma | 81 beceri; 12 bütçe/iade + 12 yeni öğrenilmiş-beceri kontrolü; hata önce UE'de yeniden üretildi | Gerçek legacy dosya göçü, yatırım/kayıt/sınıf RPC ve HUD; sınıf dengesi, Lv33+ beceri boşlukları |
| Efekt ve yenilenme | 12 buff/debuff tanımı; replike bitiş zamanı; can/mana yenilenmesinde 0,5 sn adım | Veri/kod/derleme | İstemci efekt süresi ve Network Profiler bant ölçümü |
| Ekipman ve envanter | 53 ekipman; InstanceId ile kuşanma/satış/yükseltme; çanta/yığın sınırları; nadirlik/+; ilk başarılı istek ölçümü | Kayıtlı ekipman seviye şartı: 11 kontrol; yeni kuşanma ölçümü: 8 kontrol, otomatik/restore hariç | Gecikmeli çift tıklama, uzak HUD, gerçek dosyadan yeniden giriş |
| Satıcı ve ekonomi | Ekipman/iksir alış-satışı; yalnız teslim edilen adet kadar ücret; ölü/uzak işlem reddi | Yeni satıcı regresyonu: 14 kontrol geçti | Gelir/harcama dengesi, uzun oturum, gerçek RPC ve kayıt kalıcılığı |
| İksir/parşömen | Ortak bekleme, ölümde ret; dolu can/manada faydasız saf iksir korunur | 11 UE kontrolü | Gecikmeli sahip-istemci kullanımı ve HUD/adet eşleşmesi |
| Görev | 79 tanım, NPC kabul/teslim, ön koşul/ilerleme/ödül; ölüm/kapasite korumaları ve kabul/teslim telemetrisi | 8 yaşam durumu + 10 kapasite/tek teslim UE kontrolü; 2 başarılı kabul / 1 teslim telemetrisi | Lv1–30 zincirini gerçek avla tamamlamak; dolu çanta/yığın HUD'ı, uzak istemci ölçümü |
| Mob, elit ve ganimet | 51 arketip (13 elit), 29 spawner tanımı; XP/katkı paylaşımı; loot rezervasyonu/kapasite; ilk gerçek pozitif teslim ölçümü | Ganimet ölüm/tek grant için 10 kontrol + ilk loot için 6 kontrol ve üç türde teslim logu | Fiziksel overlap/timer, gerçek parti kredi ve boss/loot oynanışı |
| Haritalar | Summer ve Winter; hub/NPC/spawner yerleşimleri, nav/doğuş çalışmaları | Son smoke: Summer 49, Winter 32 mob doğumu; iki haritada 0 hata | Cep rotaları, zemin, erişim, leash/aggro ve Lv1 doğuş güvenliği |
| Başlangıç ve rehber | Yeni kayıt dalında Kısa Kılıç + Deri Zırh + 3 can iksiri; sınıf seçimi ve 8 bağlamsal HUD ipucu | Kodda mevcut; ipucu maskesi yerel oyuncunun oturumunda tutulur | İlk 10 dakika kullanıcı koşusu; kalıcı rehber ilerlemesi gerekiyorsa kayıt tasarımı |
| Parti | Üyelik/menzil/XP paylaşımı, lider kuralları; parti birleşmesinde lider onayı | Lider/rıza regresyonu: 10 kontrol | Çok pencerede birleşme/ayrılma, gerçek katkı ve XP paylaşımı |
| Ticaret ve depo | Çift onay/teklif revizyonu, kimlikle eşya, kapasite/altın tavanı; NPC yakınlığında depo | Kod/derleme; satış kapasitesi yeni satıcı testinde | Ticaretin iki kaydı birlikte kalıcılaştırması; çökme/bağlantı kopması; gerçek depo RPC |
| Klan, arkadaş ve sohbet | Klan üyelik/liderlik/davet, arkadaş listesi, sohbet ve hız sınırları | Klan davetinde güncel lider yetkisi: 9 kontrol | Çevrimdışı üyelik/rıza, yeniden giriş ve çok oyunculu sosyal akış |
| NPC diyalog / LLM kota | Sunucu NPC isteği, yedek replik, günlük/oturum/oyuncu/dakika/eş-zamanlı bütçe; 4096 takip anahtarı dolunca yeni anahtar reddi | Hata önce UE FAIL11; 25 bellek kontrolü PASS, API/HTTP/disk kullanılmadı | Gerçek HTTP/hata/callback, UTC/gece yarısı, dosya yeniden giriş; hesap kimliği ve çoklu sunucuda ortak kota |
| Yerel kayıt / JSON | Store arayüzü, yerel .sav, JSON v2 ve sıkı giriş doğrulaması; yazım numarası takibi | 111 JSON negatif test; 322 yuva okundu; JSON eşleşmesi ve seçilen kayıt roundtrip'i geçti | Gerçek Lv30+ kayıtlar; bulut adaptörü, depoda sıra garantisi/iptal ve işlem bütünlüğü |
| Bölge geçişi | Asenkron onay bariyeri, timeout/iptal, eski/çift callback koruması; son XP/altını yeniden yazma | Normal/timeout/durum/yazım hatası/ilerleme testleri + callback için 9 kontrol | Son kayıt–pawn yok oluş penceresi; gerçek gecikmeli mağaza ve motor yükleme hatası |
| Ağ / replikasyon | Sunucuda mutasyon; kişisel alanlarda owner-only; 1 host + 1 istemci bağlantısı | NetSmoke: 1 join, 1 welcome, iki tarafta 0 hata | Gerçek iki oyunculu oyun, paket gecikmesi/kaybı, yük ve bant ölçümü |
| Hesap ve güvenlik | Yükleme/işlem doğrulamaları, debug Shipping kapıları ve çeşitli kötüye kullanım düzeltmeleri | Derleme + sınırlı regresyon kanıtları | Ad bağımlı kayıt kimliğini hesap/karakter kimliğine ayırmak; kayıt imzası/yetkilendirme |
| Dedicated / backend | Admin kapısı merkezileştirildi; kayıt arayüzü ve GameState hazırlığı mevcut | Editor hedefinde derleniyor | Kaynak UE 5.7, Server hedefi/paketleme, hesap servisi, gerçek asenkron depolama |
| Telemetri ve denge | Kill/death/levelup/gold, görev XP/süre; controller oturumunda ilk görev/loot/öldürme kredisi/kuşanma isteği/kabul edilen beceri/temel saldırı; otomatik başlangıç kuşanması hariç | 6 oturum + 6 loot + 10 ölüm + 8 kuşanma kontrolü; beceri 17 UE kontrolü/7 tür; temel saldırı 21 kontrol/8 olay, sıfır hasarlı kabul ve native RPC dahil; parser 312/312 assert; yanlış hedef seviyesi ve eksik sıfır hasar olayı negatifte yakalandı | Diğer ilk eylemler; gerçek solo/parti süreleri, model karşılaştırması ve ardından denge ayarı |

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
| `6b32ba9` | İlk gerçek pozitif loot teslimi; controller ömründe tek süre ve gerçek adet/altın | 6 yeni UE kontrolü; ölüm/oturum testleri genişletildi; yanlış miktar negatif testi yakalandı; parser 13 olay; 28/28 paket |
| `a81fda4` | Mevcut ölüm akışında ilk kredi; fatal/contribution/party/fallback ve XP öncesi seviye | 10 yeni UE kontrolü; oturum tekilliği genişletildi; yanlış fallback etiketi negatif testte yakalandı; parser 17 olay; 29/29 paket |
| `2ac517d` | Başarılı oyuncu isteği; otomatik başlangıç/kayıt yükleme hariç, controller ömründe tek ölçüm | 8 yeni kontrol + 6 oturum kontrolü genişletildi; otomatik giriş negatif testi yakalandı; parser 17 olay ve gerçek UE logu geçti; 30/30 paket |
| `6b5630f` | Ücretli kayıtlı beceriyi rütbe/bakiye bütçesine katma; ücretsiz starter ve geçerli legacy hotbar korunur | Önce FAIL checks=0, düzeltme sonrası 12 yeni UE kontrolü; stats-first/skill-first, roundtrip, max ağaç maliyeti ve iade; 31/31 paket |
| `dfa39de` | Tablo doluyken yeni anahtarın oyuncu kotasını atlamasını reddetme; mevcut oyuncular devam eder | Önce UE FAIL11, ardından 25 kontrol PASS; 32/32 tam paket; HTTP/disk/callback testleri açık |
| `b3d352b` | Mana ödemesinden sonra etki/XP öncesi ilk kabul; skill/effect/mana ve controller tekilliği | 17 yeni UE kontrolü, 7 tür/olay; 6 oturum kontrolü genişletildi; parser 150/150 assert; yanlış log saat etkisi düzeltildi; 33/33 son paket; gerçek RPC/PIE açık |
| 7 Ekim ilk temel saldırı (bu belgeyle aynı commit) | Sunucu kabulünde hasar/XP öncesi lv ve gerçek hedef; controller oturumunda tek ölçüm | 21 native kontrol/8 olay; 6 oturum kontrolü genişletildi; parser312/312; negatif metadata/sıfır hasar, native ilk RPC ve tam öldürme XP'si; 34/34 son paket; gerçek ağ/PIE açık |

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

7 Ekim'deki son tamamlanan `RunAllChecks.ps1 -SkipBuild -FullCompile`: **34/34 geçti**.
Normal Editor derlemesi ayrıca geçti. 16 dosyanın PCH olmadan /W4 derlemesinde
derleyici uyarısı yok. Veri denetiminde Lv33+ beceri boşluğu uyarıları, StaticLint'te
InitGameState override beyaz liste uyarısı açık. Yerel test edilen en yüksek gerçek
kayıt seviyesi 6; 322 kaydın okunması oyuncu sayısı veya Lv1–30 test tamamlanması değildir.
Önceki stats-first fixture dahil beceri12, LLM25, skill17, yeni temel saldırı21/8 olay, parser312/312 ve tüm eski kontroller bu
pakette geçti. Summer49/Winter32 smoke hata0; NetSmoke1join/1welcome, iki tarafta
hata0. LLM fixture'i bellek helper'larını sınar; gerçek HTTP, dosya kalıcılığı,
callback zamanlaması ve uzak istemci HUD'ı için kanıt değildir.
Son parser sayısal/eksik alan korumaları da bu tam pakette geçti.
İlk eylemler sunucu controller ömrüne aittir; kalıcı karakterin
ilk görevi/becerisi veya gerçek 20 dakika koşusu olarak yorumlanmaz.
İlk loot yalnız pozitif pickup teslimidir; kısmi sarf toplamayı içerir,
ekipman kuşanma veya bütün ilk eylemler planının bitmesi anlamına gelmez.
İlk kill, mevcut ölüm akışındaki kredidir; `credit` alanı son vuruşu parti
ve katkıdan ayırır. XP hesaplanan ödüldür; tavanda gerçek artış ölçümü değildir.
İlk equip başarılı kuşanma isteğidir; otomatik başlangıç ve kayıt yükleme
ölçüme dahil değildir. Fixture tüm başlangıç dalını veya gerçek RPC/HUD'ı
çalıştırmaz; debug/C++ üzerinden aynı public istek de sayılır.

İlk skill kabul edilen sunucu kullanımının zamanıdır, ilk hasar/faydalı şifa
değildir; tam can şifası ve boş AoESelf dahil. Mana DB maliyetidir; buff sonrası
bakiye ölçümü değildir. Genel beceri sayacı veya öğrenme zamanını kapsamaz.

İlk basic_attack kabul zamanıdır; hedef türü/arketip veya anonim Player ve
hedef seviyesi yazılır. Hasar veya kullanıcı girdisi zamanı değildir.
Sıfır saldırı gücüyle kabul edilen olay da fixture'da sayılır; öldürücü ilk
vuruş seviye artmadan yazılır. Aktarım testi kayıt sonrasını kapsar; gerçek
RPC, input gecikmesi ve 20 dk sınıf ölçümü açık.

- [Ayrıntılı düzeltme ve test kaydı](CHECKLIST_FIXES_2026-10-05.md)
- [Oynanacak kontroller](PLAYTEST_CHECKLIST.md)
- [İlk koşu yöntemi](PLAYTEST_RUNBOOK.md)
- [Lv1–30 tasarım ve ölçüm planı](VERTICAL_SLICE_LV1_30.md)
- [Ağ / dedicated sınırları](NETWORK_AUDIT.md)
- [Güvenlik ve hesap sınırları](SECURITY_AUDIT.md)

Agency-agents rol belgeleri geliştirme/review rehberi olarak kullanılıyor.
Bu son işte Ptolemy paralel ajanı parser/test/fixture/validator dosyalarını yazıp
UE Python assertlerini çalıştırdı; C++/PS entegrasyonunu salt okunur inceledi.
Önerdiği sıfır hasarlı kabul, ilk native RPC ve tam XP kontrolleri tamamlandı.
Unreal/QA rol rehberleri kullanıldı; C++/PS/derleme parent tarafından yürütüldü. Önceki işlerin rol
belgeleri ayrı çalışan ajan veya oyunun runtime AI bağımlılığı değildir.
