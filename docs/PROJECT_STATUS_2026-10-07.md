# MyProject — yaptık / yapacağız

**Güncelleme:** 9 Ekim 2026 (8 Ekim UTC). Kaynak kod ve tamamlanan kontroller esas alınmıştır.
Önceki HEAD `ec27984` (§66/O29), kaynağı yok olan oyuncu DoT hasarını ve callback
sonrası bayat pending tick'i reddetti. Son sınırlı düzeltme (§67/O30), bütün
/klan komutları ve /g mesajını gönderim anındaki pawn'a bağlar. Gerçek native
possession değişiminde eski ayrıl isteği yeni pawn'ın farklı klanını boşalttı
(FAIL45/next46); /g kapısı kaldırılınca eski mesaj HUD'a ulaştı (FAIL49/next50).
Mevcut ortak throttle, lider/ölüm/cooldown/ad kuralları ve kabul ID kontrolü
korunur; yeni RPC alanı, save/replikasyon şeması veya timer yok. ECC plan/review/
verify ve readonly Unreal/multiplayer/security/QA incelemesi kullanıldı.
Final gerçek compile/link, 73 native klan kontrolü (chat41) ve değişen CPP'nin
PCH'siz /W4 derlemesi/warning0 ve 39/39 güncel paket geçti. Timestamp korunmuş restore'un eski DLL'i
testte yakalandı; gerçek rebuild sonrası PASS73, kaynak bytes yedekle aynı.
Gerçek remote/Slate/PIE, aynı pawn üyelik leave/rejoin generation, hedef kick
rejoin, atomik roster/bildirim ve Server/Shipping hâlâ açık.

**Ulaştığımız aşama:** UE 5.7 üzerinde C++ MMORPG sistemleri ve Lv1–80 içerik
tanımları bulunan, yerel kayıt ve iki haritalı çok oyunculu bağlantısı test edilen
bir prototip. Lv1–30 akışının baştan sona oynanış, süre ve denge doğrulaması açık.
Toplam tamamlanma yüzdesi hesaplanmadı; içerik tanımı ile oynanarak doğrulanmış
ürün farklı kanıtlar gerektirir.

## 1. Proje iskeleti ve ilerleme tablosu

| Katman / sistem | Yaptık | Mevcut kanıt | Yapacağız |
| --- | --- | --- | --- |
| Motor ve kod yapısı | UE 5.7, `Source/MyProject` C++ modülü; Blueprint görsel/içerik katmanı; Enhanced Input | Editor hedefi derlendi; önceki19 dosya + son değişen controller CPP ayrı PCH olmadan /W4 ile 0 uyarı | Gerçek Server hedefi ve Shipping paketini ayrı derlemek |
| Karakter ve statlar | STR/DEX/INT/VIT, can/mana, XP/seviye, stat puanı, ölüm/diriliş; sunucu otoritesi | Kayıt stat/XP/puan bütçesi ve dört sınıf için 15 UE kontrolü; ölüm kuralları ayrı fixture'larda | Gerçek ölüm/respawn, kayıt sonrası istemci stat paneli ve uzun oturum |
| Savaş ve PvP | Hedefleme, normal saldırı, beceriler, güvenli hub, PvP/parti/klan kuralları; hasar/şifa bildirimi; tek cooldown kapısı ve double damga | Temel saldırı 21 kontrol; yeni cooldown 89 kontrol, yedi sentetik uptime/eşik sınırları; eski RPC önce UE'de FAIL0 verdi | İki oyuncuda gerçek hasar, gecikme, güvenli bölge ve görsel sayı kontrolü; gerçek uzun oturum |
| Sınıf ve beceri | Dört sınıf, mastery/rütbe, öğrenme, hotbar, mana/cooldown; rütbe + bakiye bütçesi; kayıtlı ücretli beceriyi minimum ağaç rütbesine taşıma | 81 beceri; 12 bütçe/iade + 12 yeni öğrenilmiş-beceri kontrolü; hata önce UE'de yeniden üretildi | Gerçek legacy dosya göçü, yatırım/kayıt/sınıf RPC ve HUD; sınıf dengesi, Lv33+ beceri boşlukları |
| Efekt ve yenilenme | 12 buff/debuff tanımı; replike bitiş zamanı; can/mana yenilenmesinde 0,5 sn adım | Native DoT33: lost player source/callback/refresh; live policy, mob hub, Regen/expiry; önce FAIL10, negatif32/15 | Gerçek elapsed tick/expiry, istemci efekt süresi/HUD, source rebinding ve Network Profiler bant ölçümü |
| Ekipman ve envanter | 53 ekipman; InstanceId ile kuşanma/satış/yükseltme; çanta/yığın sınırları; nadirlik/+; ilk başarılı istek ölçümü | Kayıtlı ekipman seviye şartı: 11 kontrol; yeni kuşanma ölçümü: 8 kontrol, otomatik/restore hariç | Gecikmeli çift tıklama, uzak HUD, gerçek dosyadan yeniden giriş |
| Satıcı ve ekonomi | Ekipman/iksir alış-satışı; yalnız teslim edilen adet kadar ücret; ölü/uzak işlem reddi | Yeni satıcı regresyonu: 14 kontrol geçti | Gelir/harcama dengesi, uzun oturum, gerçek RPC ve kayıt kalıcılığı |
| İksir/parşömen | Ortak bekleme, ölümde ret; dolu can/manada faydasız saf iksir korunur | 11 UE kontrolü | Gecikmeli sahip-istemci kullanımı ve HUD/adet eşleşmesi |
| Görev | 79 tanım, NPC kabul/teslim, ön koşul/ilerleme/ödül; ölüm/kapasite korumaları ve kabul/teslim telemetrisi | 8 yaşam durumu + 10 kapasite/tek teslim UE kontrolü; 2 başarılı kabul / 1 teslim telemetrisi | Lv1–30 zincirini gerçek avla tamamlamak; dolu çanta/yığın HUD'ı, uzak istemci ölçümü |
| Mob, elit ve ganimet | 51 arketip (13 elit), 29 spawner tanımı; XP/katkı paylaşımı; loot rezervasyonu/kapasite; ilk gerçek pozitif teslim ölçümü | Ganimet ölüm/tek grant için 10 kontrol + ilk loot için 6 kontrol ve üç türde teslim logu | Fiziksel overlap/timer, gerçek parti kredi ve boss/loot oynanışı |
| Haritalar | Summer ve Winter; hub/NPC/spawner yerleşimleri, nav/doğuş çalışmaları | Son smoke: Summer47, Winter29 mob doğumu; iki haritada 0 hata | Cep rotaları, zemin, erişim, leash/aggro ve Lv1 doğuş güvenliği |
| Başlangıç ve rehber | Yeni kayıt dalında Kısa Kılıç + Deri Zırh + 3 can iksiri; sınıf seçimi ve 8 bağlamsal HUD ipucu | Kodda mevcut; ipucu maskesi yerel oyuncunun oturumunda tutulur | İlk 10 dakika kullanıcı koşusu; kalıcı rehber ilerlemesi gerekiyorsa kayıt tasarımı |
| Parti | Üyelik/menzil/XP paylaşımı, lider rızası; davet/üyelik kimliği; chat pawn bağlamı; authority EndPlay cleanup | 83 native: önceki73 +10 gerçek lifecycle; UnPossess/Destroy/leader/member/transition route; baseline/negatif FAIL75 | Gerçek disconnect/travel/owner-only teslim/HUD/XP; kalıcı reconnect/presence; kick stale bağlamı ve roster atomik görünümü |
| Ticaret ve depo | Görülen session/revision teklif/onay; session cancel/X; recipient invite generation accept/decline; chat gönderim pawn/kimlik/iptal niyeti; kimlik/kapasite/altın tavanı ve NPC depo | 218 native kontrol: önceki182 +28 invite +8 controller chat; eski decline önce FAIL165/check166; tam component ret snapshot'ı ve native Say/OnChatCommitted/endpoint kanıtı | Gerçek davet/RPC/HUD; bildirimle kimliğin ortak görünümü; reentrant rollback; ortak kalıcı işlem/çökme/yeniden giriş; gerçek depo RPC |
| Klan, arkadaş ve sohbet | Klan üyelik/liderlik/davet, recipient generation; bütün /klan ve /g gönderim pawn eşleşmesi; arkadaş listesi, sohbet/hız sınırı | Klan73: önceki43 +30 native command/HUD/possession; chat41; baseline45/g negatif49; arkadaş presence24 | Gerçek remote/owner-only teslim; aynı pawn clan üyelik generation, hedef kick rejoin; çevrimdışı üyelik/rıza ve yeniden giriş |
| NPC diyalog / LLM kota | Sunucu NPC isteği, yedek replik, günlük/oturum/oyuncu/dakika/eş-zamanlı bütçe; 4096 takip anahtarı dolunca yeni anahtar reddi | Hata önce UE FAIL11; 25 bellek kontrolü PASS, API/HTTP/disk kullanılmadı | Gerçek HTTP/hata/callback, UTC/gece yarısı, dosya yeniden giriş; hesap kimliği ve çoklu sunucuda ortak kota |
| Yerel kayıt / JSON | Store arayüzü, yerel .sav, JSON v2 ve sıkı giriş doğrulaması; yazım numarası takibi | 111 JSON negatif test; son paket433 yuva okundu; 26 native detach/save kontrolü; JSON eşleşmesi/roundtrip geçti | Gerçek Lv30+ kayıtlar; bulut adaptörü, depoda sıra garantisi/iptal ve işlem bütünlüğü |
| Bölge geçişi | Asenkron onay bariyeri, timeout/iptal, eski/çift callback koruması; son XP/altını yeniden yazma | Normal/timeout/durum/yazım hatası/ilerleme testleri + callback için 9 kontrol | Son kayıt–pawn yok oluş penceresi; gerçek gecikmeli mağaza ve motor yükleme hatası |
| Ağ / replikasyon | Sunucuda mutasyon; kişisel alanlarda owner-only; 1 host + 1 istemci bağlantısı | NetSmoke: 1 join, 1 welcome, iki tarafta 0 hata | Gerçek iki oyunculu oyun, paket gecikmesi/kaybı, yük ve bant ölçümü |
| Hesap ve güvenlik | Yükleme/işlem doğrulamaları, debug Shipping kapıları ve çeşitli kötüye kullanım düzeltmeleri | Derleme + sınırlı regresyon kanıtları | Ad bağımlı kayıt kimliğini hesap/karakter kimliğine ayırmak; kayıt imzası/yetkilendirme |
| Dedicated / backend | Admin kapısı merkezileştirildi; kayıt arayüzü ve GameState hazırlığı mevcut | Editor hedefinde derleniyor | Kaynak UE 5.7, Server hedefi/paketleme, hesap servisi, gerçek asenkron depolama |
| Telemetri ve denge | Kill/death/levelup/gold, görev XP/süre; controller oturumunda ilk görev/loot/öldürme kredisi/kuşanma isteği/kabul edilen beceri/temel saldırı/oyuncu ölümü; otomatik başlangıç kuşanması hariç | 6 oturum + 6 loot + 10 ölüm + 8 kuşanma kontrolü; beceri 17 UE kontrolü/7 tür; temel saldırı 21 kontrol/8 olay, sıfır hasarlı kabul ve native RPC dahil; ilk oyuncu ölümü14/3first+5death ve gerçek loss5/0/2; parser576/576 assert; yanlış XP kaybı metadata mutasyonu yakalandı | Diğer ilk eylemler; gerçek solo/parti süreleri, model karşılaştırması ve ardından denge ayarı |

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
| `7aa7f42` | Sunucu kabulünde hasar/XP öncesi lv ve gerçek hedef; controller oturumunda tek ölçüm | 21 native kontrol/8 olay; 6 oturum kontrolü genişletildi; parser312/312; negatif metadata/sıfır hasar, native ilk RPC ve tam öldürme XP'si; 34/34 paket; gerçek ağ/PIE açık |
| `3dec6f5` | RPC'nin çift cooldown/sentetik saatini ortak tek kapı ve double gerçek damgayla değiştirme; mevcut %10 tolerans | Önce UE FAIL0 uptime100, ardından 89 kontrol; unflagged test komutu reddi ve eksik koruma negatif kanıtı; 35/35 paket; gerçek RPC/jitter/PIE açık |
| `222fa92` | Mevcut revizyon/çift onay/ekonomik bütünlük kurallarını otomatik pakete alma; üretim kuralı değişmedi | 79 kontrol; revizyon koruması mutasyonunda FAIL12/check13; 36/36 paket, 18 PCH'siz dosya; gerçek ağ/kalıcı işlem açık |
| `97b9f76` | Eski onayın yeni aynı-revizyon takasına taşınmasını reddetme; owner-only uint64 session ve çizilen HUD çifti | Önce FAIL80/check81, ardından96 native kontrol; normal DLL ve 36/36 paket geçti; 18 PCH'siz dosya, 0 derleyici uyarısı (§55); gerçek ağ/HUD açık |
| `ddea095` | Add/remove/gold RPC'lerini görülen session/revision'a bağlama; çizilen gold delta'sı | Önce FAIL79/check80; DLL,162 kontrol ve 36/36 son paket geçti; 18 PCH'siz dosya/warning0 (§56); gerçek ağ/HUD açık; aktif cancel sonraki §57'de düzeltildi |
| `e1cf40e` | Cancel/X görülen session; teklif değişimi iptali engellemez; ayrı inaktif davet reddi | Önce FAIL145/check146; DLL,182 kontrol ve36/36 tam paket geçti; 18 PCH'siz dosya/warning0 (§57); davet nonce'u sonraki §58'de eklendi; gerçek ağ/HUD açık |
| `c4effd1` | Recipient generation; gönderim pawn/kimlik/iptal niyeti; farklı pawn'a fallback yok | Önce FAIL165/check166; DLL/218 native ve36/36 tam paket geçti; 18 PCH'siz dosya/warning0 (§58); gerçek bildirim görünümü/ağ açık |
| `dd9f354` | Recipient generation accept/decline; chat gönderim pawn/ID; eski davete/pawn'a fallback yok | Önce FAIL10/negatif FAIL13; normal DLL/46 native ve36/36 tam paket,18 PCH'siz/warning0 (§59); gerçek ağ/bildirim görünümü açık |
| `3df7a3e` | Controller ilk death zamanı/by/gerçek xploss; normal death toplamlarını çift saymaz | Native14/3first+5death; oturum6 genişletildi; Lost+1 negatif PSexit1, restore; parser576/576, DLL/37 paket/18 PCHdosya (§60); gerçek callback/PIE açık |
| `fa39045` | Recipient generation ve chat gönderim pawn eşleşmesi; stale kabul pending'i tüketmez | Önce FAIL8; ID/pawn negatifleri FAIL12/FAIL38; normal DLL/43 native,37/37 paket,18 PCHdosya/warning0 (§61/O24); gerçek bildirim/ağ/PIE açık |
| `967800c` | Grup session + kişisel membership; rejoin/merge eski leave reddi; chat pawn eşleşmesi | Önce FAIL38; negatif FAIL40/65; normal DLL/73 native,37/37 paket,18 PCHdosya/warning0; gerçek PIE ve lifecycle açık |
| `b9ae6e0` (§63/O26) | PlayerState'ten bağımsız authority EndPlay cleanup; survivor üyelik kimliği korunur | Önce/negatif FAIL75, normal DLL/PASS83,37/37 paket,18 PCHdosya/warning0; gerçek remote/save/PIE açık |
| 8 Ekim kayıt lifecycle (§64/O27) | PlayerState silinmeden force save; detached load/save/nav timer ve initial-load kapısı kapanır; EndPlay tek yazım | Önce FAIL4; negatif FAIL9/17/10; DLL/PASS26,38/38 paket,18 PCHdosya/warning0; gerçek reconnect/presence/PIE açık |
| 8 Ekim arkadaş presence (§65/O28) | Identity detach öncesi offline; initial-load/save bağımsız, mutual/travel/EndPlay korumalı | Önce/negatif FAIL6/6/18; DLL/PASS24,39/39 paket,19 PCHdosya/warning0; gerçek remote/PIE/cache sınırları açık |
| 8 Ekim DoT kaynak ömrü (§66/O29/D2) | Player kaynak kaybolunca anonim hasar reddi; pending weak reference her dispatch'te çözülür; refresh provenance | Önce FAIL10; negatif FAIL32/15; DLL/PASS33,40/40 paket,19 PCHdosya/warning0; gerçek remote/elapsed/source rebinding açık |
| 8 Ekim bütün klan chat komutlarında pawn bağlamı (§67/O30) | Eski /klan ve /g yeni pawn'a uygulanmaz; fresh authority/cooldown kuralları korunur | BaselineFAIL45, g negatifFAIL49; gerçek DLL/PASS73/chat41,39/39 paket, tek değişen CPP NoPCH/warning0; remote/aynı pawn üyelik generation açık |

## 3. Sıradaki çalışma sırası

| Öncelik | İş | Tamamlandı sayılma koşulu |
| --- | --- | --- |
| P0 | Doğrulanmış sınırlı bugları ve kalan riskli kayıt/ekonomi yollarını ele almak | Önce hata kanıtı; düzeltme sonrası UE regresyonu, derleme ve ilgili paket geçer |
| P0 | İlk iki oyunculu PIE koşusu: doğuş → görev → loot → ekipman → kayıt/yeniden giriş | Host/uzak istemci için açık geçti/kaldı notları ve gerçek loglar |
| P1 | Friends presence ve gerçek detach/reconnect kimlik ömrü | Save O27/offline O28 native düzeltildi; gerçek remote reconnect/load/nav/error/retained seamless ve presence PIE; 300 sn/cache world izolasyonu/committed-disconnect açık |
| P1 | Kayıt kimliği/hesap ve işlem bütünlüğü | Sunucunun doğruladığı kimlik; başka karakter kaydını yükleme/ezme reddi; göç ve hata testleri |
| P1 | O6 kalan geçiş penceresi ve gelecekteki depoda yazım sırası | Gecikmeli/başarısız/iptal edilen yazımda ilerleme geri sarılmaz; depoda atomiklik kanıtı |
| P1 | Lv1–30 ölçümünü tamamlamak | Görev/ilk eylem telemetrisi; dört sınıf solo ve parti koşusunda süre/ölüm/altın tablosu |
| P2 | Rehber, görev ritmi, beceri ve ekonomi ayarı | İlk 10 dakika kullanıcı testi; ayarlar gerçek ölçüme dayanır |
| P2 | Dedicated doğrulaması | Kaynak motorla gerçek Server hedefi derlenir/paketlenir; ayrı istemcilerle çalışır |

Kullanıcı bilgisayar başında değilken kod, fixture, derleme ve belge işleri
ilerletilebilir. Gerçek PIE oyun koşuları yapılmış gibi işaretlenmez. Dedicated
derlemesi için kurulu Launcher motoruna ek olarak kaynak motor gerekir.

## 4. Son test paketi ve belge bağlantıları

**O30 güncel paket:** `RunAllChecks.ps1 -SkipBuild` **39/39 geçti**, exit0.
Gerçek restore compile/link ayrıca Succeeded4,23 sn; değişen controller CPP
NoPCH/NoLink/SingleFile12,54 sn ve /W4 warning0. FullCompile19 yeniden yapılmadı.
Klan73/chat41, DoT33, save26, presence24, parti83, takas218 ve parser576/576;
JSON schema2/negative111, store loaded433/invalid0/loadFailed0/existsMismatch0/
jsonMismatch0/maxLevel6/writeorder1/roundtrip equal1. Summer47/Winter29 hata0;
NetSmoke1join/1welcome iki tarafta hata0. Güncel TEMP logu
`mmo_all_clan_commands_final_20261008.log`; NetSmoke `mmo_net_server_000117.log` /
`mmo_net_client_000117.log`. 150 Source/Scripts SHA256 paket öncesi/sonrası aynı;
engine/UBT0, klan fixture kayıt0. NativeHUD buffer uzak RPC/Canvas/Slate kanıtı
değildir. §67/O30 ve PLAYTEST_CHECKLIST açık maddeleri sınırları kaydeder.



Önceki O29 HEAD'de `RunAllChecks.ps1 -SkipBuild -FullCompile`: **40/40 geçti**.
Bu paragraf O29'un tarihsel paketidir; O30'un güncel doğrulaması aşağıda kaydedilir.
Normal Editor derlemesi ayrıca geçti. 19 dosyanın PCH olmadan /W4 derlemesinde
warning C0. Veri denetiminde Lv33+ beceri boşluğu uyarıları, StaticLint'te
InitGameState override beyaz liste uyarısı açık. Yerel test edilen en yüksek gerçek
kayıt seviyesi 6; 426 kaydın okunması oyuncu sayısı veya Lv1–30 koşusu değildir.
DoT source33, save lifecycle26, arkadaş presence24, klan43, parti83, takas218,
ilk oyuncu ölümü14/3first+5death, parser576/576 ve eski kontroller bu pakette geçti.
JSON schema2/negative111; store loaded426, invalid/loadFailed/existsMismatch/
jsonMismatch0, writeorder1 ve roundtrip equal1. Summer51/Winter27 smoke hata0;
NetSmoke1join/1welcome, iki tarafta hata0. LLM25 bellek helper'larını sınar;
gerçek HTTP/disk/callback veya uzak HUD için kanıt değildir. Presence24 native
standalone HUD buffer'ıdır. DoT33 actual damage/health-delegate callback ama
elapsed scheduler/uzak effect replication ve normal PvP skill erişimi kanıtı değildir.
Son paket logu `mmo_all_dot_source_final_20261008.log` (TEMP); NetSmoke logları
`mmo_net_server_234322.log` / `mmo_net_client_234322.log`. 150 Source/Scripts SHA256
listesi paket öncesi/sonrası aynı; bu kaynak/runner doğrulandı, editör kapalı.
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

İlk oyuncu death, gerçek server ölüm/XP cezası callback'inden sonra yazılır;
by mob arketipi veya bilinmeyen/non-mob için none, xploss gerçek kayıptır.
Sıfır XP kaybı da ilk olabilir. Marker yeni ölüm/XP toplamı üretmez; regular
death toplamları taşır. Native14 gerçek ölüm/ceza gövdelerini aynı frame çalıştırır,
zamanlayıcıyı tick etmez. Oturum6 sentetik deadstate/pawn/controller aktarımıdır.
Eski logda ölüm zamanı tahmin edilmez. Blueprint/XP callback'i ownership/health
değiştirirse mevcut-pawn/dead/health kapıları event'i atlayabilir; bu akış,
DoT/PvP kaynak kimliği, gerçek reentry/remote timer ve20dkPIE doğrulanmadı.

Cooldown fixture'ı yedi sentetik dünya zamanıyla çift/tam/toleranslı eşikleri
sınar; dünya tick/timer'ları veya gerçek uzun oturum çalıştırmaz. Test komutu
başlatma bayrağı olmadan fixture oluşturmadan reddedilir. Bayrak yalnız ayrı
başlatılıp hemen kapanan test süreçleri içindir; normal editöre eklenmez,
çünkü dünya saatini geri almak sentetik HUD yazılarını geri almaz.

Takas component fixture'ları iki controller'sız karakteri aynı frame içinde kullanır;
§58 ayrıca üç native controller/pawn ile chat endpoint ve Say/OnChatCommitted'ı
çalıştırır. Gerçek RPC taşıma/Slate input kanıtı değildir. İlk79 kontrolün
aynasında adet/onay kontrolü vardır; §56 offer retlerinde iki taraftaki tüm
`TheirOffer.Items` payload'ı da bağımsız snapshot karşılaştırmasına girer.
Dünya saati değiştirilmez; kayıt yükleme ve kalıcı takas save kapısı açılmaz.
Session discriminator §55'te eklendi; eski onay kabulü native UE'de yeniden
üretildi ve reddi doğrulandı. İki oyuncunun ortak kalıcı işlemi, fault injection
ile rollback, gerçek owner-only replication/davet/HUD doğrulaması açık.
Confirm RPC imzası değişti; iki uç aynı yeni derlemeyi kullanmalı. Console/C++
RequestConfirm ve offer wrapper'ları güncel yerel snapshot'ı, HUD ise son çizilen
çifti taşır. Add/remove/gold RPC imzaları §56'da, cancel RPC imzası §57'de değişti.
Aktif cancel/X yalnız çizilen session'ı taşır; aynı takastaki revizyon değişimi
çıkışı engellemez. İnaktif davet reddi ayrı RPC ile aktif takastan ayrılır.
§58 davet kabul/reddine generation ekledi. Chat gönderim pawn'ı mevcut sahip
olunan pawn ile eşleşir; kimlik/iptal niyeti sunucuda yeniden seçilmez. Bildirim
metni ile latest replicated ID birlikte atomik değildir; eski görünen satırı
okuyup yeni ID geldikten sonra kabul yazma ayrı açık risktir. Ek chat metadata
bant maliyeti ölçülmedi; ServerSendChat sözleşmesi de iki uçta aynı DLL gerektirir.


§59 parti daveti accept/decline da recipient generation ve chat gönderim ID'sine
bağlandı. Mevcut /parti leave/kick protokolü değişmedi. Native46 içinde iki
recipient (aynı inviter reissue ve farklı sender), tam ret snapshot/MAX; üç PC
ile gerçek nearest, Say/OnChatCommitted ve matching nonce ile native possession
replacement bulunur. Gerçek remote respawn/expiry/Slate/ownership/owner-only teslim
ve bildirim/güncel kimlik ortak görünümü açık. Ek party ID bütün chat trafiğine
metadata ekler; gerçek profiler ölçümü yapılmadı. Son paket36/36/exit0, Party46 ve
Trade218 aynı yeni DLL'de geçti; derlenen Source/Scripts hash listesi değişmedi.

- [Ayrıntılı düzeltme ve test kaydı](CHECKLIST_FIXES_2026-10-05.md)
- [Oynanacak kontroller](PLAYTEST_CHECKLIST.md)
- [İlk koşu yöntemi](PLAYTEST_RUNBOOK.md)
- [Lv1–30 tasarım ve ölçüm planı](VERTICAL_SLICE_LV1_30.md)
- [Ağ / dedicated sınırları](NETWORK_AUDIT.md)
- [Güvenlik ve hesap sınırları](SECURITY_AUDIT.md)

Agency-agents rol belgeleri geliştirme/review rehberi olarak kullanılıyor.
§55 takas işinde Pasteur salt okunur Unreal Multiplayer Architect/QA incelemesi
yaptı; üç bulgu giderildi, son incelemede yeni engel bulunmadı ve ajan kapatıldı.
§56 offer işinde Noether salt okunur caller/test kapsamını inceledi; future/negative
revision ve fresh0 kontrolleri de eklendi. Post-accept eski çift testinde no-op
payload zayıflığı giderildi; son inceleme sayıyı/geçerli payload'ı doğruladı,
ajan kapatıldı. Parent tüm kod/test/derlemeyi yürüttü.
§57 cancel işi için Boyle salt okunur incelemeye açıldı; kullanım sınırıyla hata
verdi, inceleme üretemedi ve kapatıldı. Bağımsız ajan onayı yok; parent caller,
native20 kontrol ve iki HUD dispatch dalını yerel olarak inceledi.
§58'de Nietzsche salt okunur Unreal Multiplayer Architect/QA caller incelemesi
yaptı: iki gönderim, iki server trade dalı, pawn replacement ve latest/drawn
ayrımı uygulandı. Parent native controller testini ve kod/test/build'i yürüttü.
Son inceleme count218 ve imza/dispatch'i doğruladı; iki test kapsam sınırı §58'de
kaydedildi. Ajan build/test çalıştırmadı; kapatıldı.
Son cooldown işinde Archimedes paralel ajanı Unreal Multiplayer Architect
rolüyle ortak kapı/eşik/ret damgalarını salt okunur inceledi; sentetik HUD zamanı
riski için disposable süreç başlatma koruması eklendi ve guard/PS tekrar incelendi.
C++/PS/test/derleme parent tarafından yürütüldü, ajan kapatıldı. Önceki §52 işinde
Ptolemy parser/test/fixture/validator dosyalarını yazıp UE Python assertlerini
çalıştırdı; önerdiği sıfır hasarlı kabul, ilk native RPC ve tam XP kontrolleri
tamamlandı. Rol belgeleri oyunun runtime AI bağımlılığı değildir.

§59'da Arendt yerel Unreal Multiplayer Architect TOML rehberi ve repo QA rolüyle
salt okunur caller/test incelemesi yaptı. Agency paket kökeni doğrulanmadı.
Dört fixture boşluğu giderildi: issuance dönüşleri, aynı inviter reissue,
matching nonce ile native possession değişimi, MAX pending zamanı. Son inceleme
46 sayısı ve guard/dispatch'i doğruladı; yeni engel bulmadı. Ajan build/test
çalıştırmadı, dosya yazmadı; kapatıldı. Parent tüm kod/test/derlemeyi yürüttü.


§60'da Volta dört parser/test/fixture/validator dosyasını yazıp UE Python
576/576 assert çalıştırdı; native validator'ın global normalized sid kontrolü
QA önerisiyle eklendi. Helmholtz yerel Unreal Multiplayer Architect TOML ve
repo QA rehberiyle salt okunur native14/PS/callback sınırı incelemesi yaptı;
build/test çalıştırmadı. Agency paket kökeni doğrulanmadı. Parent mevcut-pawn
hook kapısını, C++/PS/docs ve sonDLL/37paketi yürüttü. Testler sırasında hiçbir
Source/Scripts yazıcısı aktif değildi; iki ajan da kapalı.


**ECC becerileri (8 Ekim):** ecc-mmo-plan, ecc-mmo-review ve ecc-mmo-verify
okundu ve klan işinin kapsam/inceleme/doğrulama akışına uygulandı. Genel web
komutları veya coverage hedefi eklenmedi. Gerçek PIE açık kalır. Yeni işlerde
plan → server-authoritative uygulama → inceleme → mevcut Unreal kontrolleri
akışı kullanılır; skill kurulumu başka sohbetin .agents dosyalarında korunur.

Otonom geliştirme otomasyonu ACTIVE tutuldu; aynı sohbet ve sıklık korunarak
ECC plan/review/verify kullanımını sonraki çalışmalara taşıyan talimat eklendi.
Yalnız anlamlı tamamlanma/hata/müdahale bildirilir; kullanıcı “durdur” dediğinde
yeni işe başlanmaz ve otomasyon durdurulur.


§62'de Lovelace salt okunur Unreal Multiplayer Architect/QA + ECC incelemesi
yaptı; shared-session tasarımındaki same-party rejoin boşluğu personal membership
ile uygulama öncesi giderildi. Actual diff ve düzeltilen lider beklentisi incelendi;
ajan dosya/build/test yazmadı, tam paket öncesi kapatıldı. Parent dört C++ dosyası,
runner, baseline/iki negatif/restore ve son37 paketi yürüttü. Aynı checkout'taki
ECC kurulum sohbeti idle görüldü; .agents/.codex/AGENTS.md korunup commit'e alınmadı.


§63'te Descartes salt okunur Unreal Multiplayer Architect/QA + ECC incelemesi
yaptı; yerel UE Pawn/Actor destruction sırası ve component lifetime doğrulandı.
Actual diff/83 runner ve baseline/fixed log marker'ları incelendi; process exit
kodlarını parent doğruladı. RouteEndPlay sonrası Destroy ikinci EndPlay değildir
sınırı netleştirildi. Ajan kod/build/test yazmadı, full paket öncesi kapatıldı.
Parent iki C++ dosyası/runner, baseline/gate negatifi/restore ve son37 paketi
yürüttü; Source/Scripts147 hash sabit. .agents/.codex/AGENTS.md korunur.


§64'te Anscombe salt okunur user-level Unreal Multiplayer Architect/QA + ECC
incelemesi yaptı. Yerel motor identity/detach/EndPlay sırası ve actual diff
incelendi; üretim kapsamında engel bulmadı. Cleanup ilk silme hatasında durma
P3'ü giderildi. Nav-retry timer'ı fixture'da gerçekten arm edilerek eksik iptal
negatifi FAIL10 ile yakalandı. Ajan kod/build/test yazmadı ve full paket öncesi
kapatıldı; Agency paket kökeni doğrulanmadı. Parent iki C++ dosyası, yeni runner/
RunAllChecks, baseline/üç negatif/restore ve son38 paketi yürüttü. 148 Source/
Scripts hash sabit; geçici GUID kayıt yuvası0 ve sonunda engine process0.
.agents/.codex/AGENTS.md diğer sohbetten korunup commit'e alınmadı. ECC plan,
review, verify mevcut mmo-feature/ue-build ile uygulandı; gerçek PIE açık kalır.
