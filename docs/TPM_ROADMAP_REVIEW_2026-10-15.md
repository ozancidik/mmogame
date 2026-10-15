# İki ırk / altı harita: teknik değerlendirme ve çalışma programı

Tarih: 2026-10-15. İncelenen taban: `4e0f0c5`. Bu belge, mevcut `SIX_MAP_PLAN.md` planını değiştirmeden, kullanıcı tarafından paylaşılan bulut/yerel konuşmasını ve dış yol haritasını güncel kodla karşılaştırır. Yeni uygulama, derleme veya oyun testi bu analiz sırasında yapılmadı.

## Karar

Bulut commit'leri yerelde. Yeniden indirme veya körlemesine `git pull` gerekmiyor. Altı haritalı mimari ise henüz çalışan ürüne dönüşmedi: mevcut oyun yerel karakter deposu ve Summer/Winter arasında toplu geçiş kullanıyor. Dış raporun küçük teslimler, bağımsız QA ve veri bütünlüğü yaklaşımı yararlı; mimari sırası ve bazı mevcut durum iddiaları düzeltilmeli.

Genel tamamlanma yüzdesi hesaplanamaz. Kabul edilmiş işlerin ağırlıklandırılmış listesi ve oyun içi kanıtı yok; checklist satır sayısı, dosya boyutu ve commit yüzdesi ürün ilerlemesi değildir.

## 1. İncelenen kapsam ve kanıt

- Yerel `master`, `origin/master` ve `origin/claude/determined-cannon-a8zts0`, inceleme sırasında `4e0f0c5` noktasında; HEAD/uzak master ayrışması `0 0`. Uzak referanslar bu görevde fetch ile kontrol edildi.
- Commit aralığı: `f70e5ad..4e0f0c5`. Ana çalışma klasöründe ayrıca başka göreve ait üç staged takas dosyası var: üretim bileşeni, transfer self-test'i ve runner. Diff toplamı **+78/-5**. Bu analiz bunları değiştirmedi veya commit etmedi.
- `.agents/` ve `.codex/agents/` altındaki izlenmeyen görev dosyalarına dokunulmadı. Eski/detached worktree'ler otomatik güncellenmedi; sahiplik ve yerel değişiklikler incelenmeden reset/rebase/pull yapılmamalı.
- TPM uzmanı **Boole**, backend/network/DevOps uzmanı **Peirce**, bağımsız QA uzmanı **Carson** gerçek alt ajan olarak görevlendirildi. Runtime türü `default`, uzmanlık görev talimatıyla verildi; Claude süreci çalıştırıldığı iddia edilmiyor.
- Kullanılan beceriler: `ecc-mmo-plan`, `ecc-mmo-review`. Ajanların yazma kapsamı yok; koordinatörün çıktısı yalnız bu yeni analiz belgesidir.

| Alan | Doğrulanan durum | Kanıtın sınırı |
|---|---|---|
| Bulutun yerel kopyaya gelmesi | Commit'ler mevcut, uzak referansla aynı | Staged takas işi bu commit'in parçası değil |
| Editör / genel headless taban | Önceki koşuda 70/70; native başarım 102, günlük 87, posta 144, ad kilidi 82 | Yeni koşu yapılmadı; önceki sonuçlar bugünkü her değişikliğe aktarılmaz |
| Harita duman testleri | Önceki koşuda Summer ve Winter başarılı | Gerçek oyuncu/HUD/Blueprint kanıtı değil |
| NetSmoke | Önceki genel taban koşusunda yalnız Summer | Winter ağ kabulü açık |
| Staged takas | O70 ham build/Transfer kanıtı, mevcut üç staged blob ve DLL ile eşleşiyor; 2.644 kontrol / 336 vaka, sıfır FAIL | PIE, gerçek uzak istemci ve kalıcı iki taraflı işlem kanıtı ayrı |
| Oyun içi taban | Mevcut kayıtlar PIE yapılmadığını söylüyor | H1/Human/Orc/PvP oynanmış kabul edilemez |
| Dedicated / Steam / ortak DB / bağımsız geçiş | Tasarım ve kısmi hazırlık mevcut | Uçtan uca çalışan altı sunucu mimarisi yok |

70/70 sonucu `%TEMP%/runall_orch_20261014.log` içindeki 70 `-> GECTI`, sıfır `-> KALDI` satırıyla tekrar sayıldı. “140 geçti” önceki çift sayımdır. Tarihli özet: `docs/GUNLUK_OZET_2026-10-14.md:18`.

## 2. Dış raporda kullanılacak ve düzeltilecek bilgiler

| İddia / öneri | Değerlendirme ve plana etkisi |
|---|---|
| Büyük C++ dosyaları sorumluluk ayrımı ister | Katılıyorum. PlayerController 133.483, HUD 109.444, Character 115.082 bayt; GameMode **66.879** bayt. Boyut tek başına hata değildir. İlk ayrım, davranışı değiştirmeyen bölge yerleşimi çıkarımı olmalı; dört yeni base class açmak zorunlu değil. Gerçek yollar `Private/Player`, `UI`, `Core`, `Characters` altındadır. |
| S1=B, farklı haritalarda eşzamanlı iki ırkı reddediyor | Ters yorum. Kabul edilen tam olarak eşzamanlı farklı haritalardır; reddedilen “oturum ırkı” kısıtıdır. `SIX_MAP_PLAN.md:11`. |
| Yerel karakter deposu JSON | Yanlış. `MMOLocalFileCharacterStore.cpp:10` ve `:27`, `LoadGameFromSlot` / `SaveGameToSlot` kullanır; karakterler `.sav`. Başarım, günlük ve posta ayrı JSON yan depolarıdır. |
| DB adapter eklemek yeterli | Eksik. Ara servis, asenkron çağrılar, revision ile eski yazıyı reddetme, kimlik/sahiplik, karakter kilidi ve eski veri içe aktarma birlikte tasarlanmalı. `MMOCharacterStore.h:66` mevcut callback korumasının depodaki yazım sırasını garanti etmediğini açıklar. |
| Takas RPC'leri ve testleri yok | Yanlış. `MMOTradeComponent.h:123` itibarıyla Server RPC'leri ve transfer testleri var. Staged paket de test/runner değişikliği içeriyor. Native kanıt, gerçek ağ/PIE kabulüyle karıştırılmamalı. |
| PacketValidator tüm canlı RPC'leri reddediyor | Yanlış. Eksik kontroller çağrılırsa false döner; mevcut C++ üretim çağrı bağlantısı bulunmadı. Bu çalışan bir anti-cheat sistemi de değildir. “Acil devre dışı bırakma” işi çıkarmak gereksiz. Binary Blueprint davranışı bu analizde editörde incelenmedi. |
| IsServerAdmin her zaman false | Yalnız dedicated için doğru. Listen hostunun yerel controller'ı yetkilidir (`MMOAdmin.cpp:6`). Dedicated yönetici yetkisi ileride sunucuda doğrulanan hesaba bağlanmalı; istemciden değiştirilebilen `bIsSuperUser` yeterli olmaz. |
| H1 PIE yapıldı / checklist'in %68'i kaldı | Kanıt yok; tarihli özet PIE yapılmadığını söylüyor. H1 gelecek mimarinin bölgesidir; bugünkü Summer kabulüyle özdeş değil. |
| Her haritaya 10 mob / 8 görev / 5 item | Kabul edilmiş kapsam değil. Önce mevcut 59 mob, 97 görev, 57 ekipman ve 93 beceri hedef bantlara eşlenmeli; yalnız kanıtlanan boşluklara içerik eklenmeli (`CONTENT_MAP.md:5`). “130 item” aynı ekipman ölçüsü olarak kullanılamaz. |
| Yeni harita eklenemiyor | Fazla kesin. Eklenebilir; bugünkü sabit tabloların ve araçların genişletilmesi gerekir. Yerleşim verisini ayırmak bu maliyeti azaltır. `MapNames.ini` tek başına spawn, seviye, ırk, sunucu adresi ve geçiş sözleşmesini çözmez. |
| A5=Trade, A3=Irk, A4=PvP | Mevcut planın etiketleriyle çakışıyor. A5=Steam, A3=hesap/sahiplik/bilet, A4=oyuncu başına sunucu geçişi. Irk aşama 2, PvP aşama 7; pazar aşama 8 ve alfada zorunlu. |
| Dokümantasyon %40 artırılsın / commit'lerin %60'ı feature | Ölçüm veya kapasite hesabı verilmemiş. Her teslimde ilgili kanıt ve karar güncellenmeli; genel yüzde kotası koymaya gerek yok. |

Mevcut `UMMOCharacterStoreSubsystem` doğrudan yerel store oluşturuyor (`Private/Save/MMOCharacterStoreSubsystem.cpp:151`). Gerçek geçiş `MMOGameMode.cpp:867` içinde `ServerTravel`; bir oyuncuyu başka sunucuya gönderen akış mevcut değil. V1 SQL ve Server target bulunması DB'nin uygulandığını veya dedicated ikilisinin çalıştığını kanıtlamaz.

## 3. İlk 10 iş gününün öncelikleri: üç paket

Bu bir çalışma sırası ve kapasite tahminidir; bugün başlatılmış uygulama değildir. Tek UE derleyici/runtime test sahibi, ayrık dosya kapsamları ve düzenli kullanıcı PIE zamanı varsayılır. Kapı geçmezse sonraki bağımlı işe başlanmaz; bağımsız tasarım/kurulum hazırlığı sürdürülebilir.

| Paket | Sorumlu / yazma kapsamı | Teslim ve kabul | Tahmini pencere |
|---|---|---|---|
| **P0 — Mevcut tabanı kabul et** | Takasın mevcut sahibi + bağımsız QA/network; taban QA; kullanıcı PIE. Üç staged dosyaya yalnız sahibi yazar; sonuç belgeleri ayrıca sahiplenilir. | O70 kanıtını aday blob'larıyla eşleştir; sahip olunan docs/index incelemesinden sonra takas paketini teslim et. Winter NetSmoke; iki oyunculu Summer↔Winter, HUD, görev/parti, yeniden giriş ve takas onayında kopma kabulü. MailRefund/ReadFailure testleri gerekiyorsa kendi izole worktree sözleşmeleriyle çalıştırılır, guard atlanmaz. | Gün 1–3; beklenmeyen veri kaybı varsa uzar |
| **P1 — Dedicated hazırlığını ölç** | DevOps + UE; QA bağımsız. Server target, motor yolu/build script'leri, yeni dedicated smoke aracı. | Kaynak motor erişimi/disk/build zamanı envanteri; kurulu motorda `-server` ara yol deneyi. Ardından gerçek Server/Editor derlemesi, Summer ve Winter bağlantısı, farklı portlarda iki süreç, CPU/RAM ve uzak bildirim ölçümü. `Client/All` seçenekleri mevcut hedeflerle eşleştirilir; ortak build kilidi kullanılır. | Gün 1–5 hazırlık/deneme; A1 bütünü mevcut tahminle 2–4 hafta |
| **P2 — Yerleşim verisini ayır** | UE + NPC uzmanı; QA; documentation. Yeni `World/MMOZoneLayout.*`, GameMode, ValidateData/PocketSpacing ve ilgili testler. | Yalnız mevcut Summer/Winter: 18/17 cep, NPC 5/4; koordinat, bant, MaxMobs ve oynanış birebir. İki harita smoke + veri kontrolü + PIE eşdeğerliği. Irk/DB/Steam eklenmez. | P0 geçince Gün 4–10 içinde başlama önceliği; iş bütçesi 1–2 hafta, bitiş kapılara/kapasiteye bağlı |

P1'in motor derlemesi ile P2'nin UE derlemesi aynı anda çalışmaz. Servis/DB şema sözleşmesi ve Steamworks/App ID hazırlığı bu sırada derlemesiz paralel yürüyebilir. Kaynak motor indirme, hesap işlemleri veya anahtar girişi bu analiz kapsamında yapılmadı.

Runbook'taki eski `git checkout` ve genel `Stop-Process` komutları otomatik çalıştırılmamalı. Önce aday checkout/sahiplik doğrulanır; başka göreve ait editör/süreç durdurulmaz. `build.ps1 -Clean` bu analizde çalıştırılmadı. Tur 6 Saved silmeyi kaldırmış olsa da gerçek oyuncu verisi yedek/geri yükleme planıyla korunmalıdır.

## 4. Alfa için bağımlılık zinciri

```text
Mevcut Summer/Winter tabanı
  +-- A1: kaynak UE + dedicated Server kanıtı ------------------+
  +-- 1: yerleşim tablosu -> 2: kalıcı ırk -> A2: DB/servis ---+--> A5: Steam
  +-- Steamworks/App ID hazırlığı ----------------------------+
       -> A3: hesap sahipliği + servis bileti
       -> A4: iki sunucu arasında oyuncu başına güvenli geçiş
       -> 3: kapılar -> 4/5: İnsan yolu -> 6: Ork + sosyal
       -> 7: PvP ve 8: pazar -> 9: alfa sertleştirme/dağıtım
```

A2, ırk arayüzünden sonra A1'in bitmesini beklemeden editör hedefinde sınanabilir. PvP ve pazar, temel bağımlılıkları hazırsa ayrık kapsamlarla paralelleştirilebilir. A1/A2/A5/A3 tamamlanmadan altı sunucuya genişlemek veri sahipliği problemini büyütür.

| Kilometre taşı | Ana sorumlu | Tamamlanma ölçütü |
|---|---|---|
| **Kalıcı ırk, aşama 2** | UE/game design + QA/network | Sunucu tek seçim kabul eder; durum replike ve kalıcıdır. Eski kayıt, yeniden giriş ve geçişte envanter korunur. |
| **Ortak depo, A2** | Backend + UE + DevOps; QA/security | Ara servis + PostgreSQL, accounts/karakter kimliği, JSONB sürümlü kayıt; eski revision reddi, DB kapalıyken kapalı başarısızlık, idempotent `.sav`/yan dosya içe aktarma ve geri dönüş. Bayraksız yerel yol korunur. |
| **Steam ve sahiplik, A5→A3** | Backend/network + UE; security/QA | SteamID sunucuda doğrulanır; anahtar yalnız serviste. Başkasının karakteri, süresi geçmiş/tekrar kullanılan bilet ve çakışan oturum reddedilir. Dedicated yönetici yetkisi doğrulanmış hesaba bağlıdır. İki gerçek Steam hesabıyla kimlik denemesi gerekir. Steam Sockets taşıması ayrıca ölçülür; başarısızsa belgelenen IP/özel ağ yedeği kullanılabilir, Steam kimliğinden vazgeçilmez. |
| **Bağımsız geçiş, A4** | Network/backend/UE; QA/security/performance | İki sunucu ve iki istemci: yalnız isteyen oyuncu geçer. Kaydın başından itibaren tüm envanter mutasyonları kilitlenir. Kira/nesil koşulu eski sunucunun yazısını reddeder; bilet tek kullanımlı, hedefe bağlıdır. Hedef çökmesi/tekrar deneme eşya veya altın çoğaltmaz. |
| **Altı bölge, 3–7** | UE/NPC/game design/assets | H1 ortak merkez; H2 İnsan **Lv25+**, H3 Ork **Lv25+**, bu iki haritada üst seviye sınırı yok (kullanıcı kararı, 2026-10-15). H4/H5 üst bölgelerin mevcut giriş koşulları ayrı kalır; bu bölgelere erişim açılması H2/H3'ten çıkmayı zorunlu kılmaz. H6 Lv55+ ortak PvP. Irk/seviye kapısı ve sosyal kurallar sunucuda uygulanır. PvP XP kaybı var; altın/eşya düşmez. Oranlar tasarımda netleşir. |
| **Pazar, 8** | Backend + UE + economy; QA/security | Oyuncu tezgâhı; satıştaki eşyanın kilidi ve atomik DB ödeme/teslim işlemi. Ayrı emanet deposu seçilirse ayrıca tasarlanır, bugün kesinleşmiş şart değildir. Aynı eşyayı çift satın alma, çanta doluluğu, kopma ve yeniden deneme test edilir. SQL'de `auction_house` bulunması pazarın hazır olduğu anlamına gelmez. |
| **Kapalı alfa, 9** | Orchestrator/QA/DevOps; kullanıcı | Altı süreç kapasitesi, iki gerçek makine, Steam girişi, iki ırkın eşzamanlı ayrı bölgeleri, PvP, pazar ve yedekten dönüş kanıtlanır. Kamuya açık 7/24 yayın ayrı kapsamdır. |

## 5. Süre ve karar yönetimi

Mevcut planın **24–40 hafta temel iş, kapı payıyla 28–48 hafta (yaklaşık 6,5–11 ay)** aralığı korunabilecek bir planlama varsayımıdır; bu analiz yeni süre ölçümü üretmedi. Steamworks dış beklemeleri, kullanıcı test takvimi, kaynak motor ve altı süreç kapasitesi tahmini değiştirebilir. “Altı ay kesin” veya “alfa %50 tamam” sonucu çıkarılamaz.

Verilmiş kararlar yeniden açılmaz: S1 eşzamanlı farklı haritalar; PostgreSQL + SteamID; ara servis; Steam-first; PvP Lv55; PvP XP kaybı; eşya/altın düşmemesi; pazarın alfada bulunması. E-posta/parola sistemi yazmak bu kapsamda gereksiz iş olur.

**2026-10-15 seviye kararı:** H2/H3 için uygunluk yalnız `seviye >= 25` ve doğru ırktır; haritaya giriş/kalış için 59, 80 veya bugünkü genel seviye tavanı üst sınır olarak kullanılmaz. İleride genel tavan artırıldığında bu iki haritanın erişim kuralı değişmez. Mevcut mob/görev bantları içerik dağılımını gösterir, erişim üst sınırı değildir; yeni yüksek seviye içeriği ayrıca planlanır. Bu kararın kabul ölçütü: doğru ırkta Lv24 ret, Lv25 ve üstü kabul; ileride desteklenen 80 üstü seviyede de kabul, karşı ırkta ret.

Kararlar ihtiyaç noktasına bağlanır: ırk ara deposu ve eski kayıt politikası aşama 2'den önce; servis dili, posta şeması ve kullanılmayan migrator/validator sınıfları A2'den önce; App ID/test hesapları A5'ten önce; hesap başına karakter sayısı A3'ten önce. Bu analiz kullanıcıya yeni soru yöneltmedi veya bu kararları kendi başına kesinleştirmedi.

İlerleme raporu her teslim için **kod tamam / derleme geçti / native test geçti / ağ kabulü geçti / PIE geçti / açık risk** alanlarını kullanmalı. Aynı hata ailesinin bütün depo/yaşam döngüsü yolları bir paket içinde incelenmeli; yalnız adı değiştirilmiş yeni denetim turlarıyla proje takvimi belirsiz bırakılmamalı.

## 6. Ajan değerlendirmesi ve sonraki somut adım

Boole ve Peirce, dış raporun S1, `.sav`, canlı validator, admin, A5 ve PIE iddialarını bağımsız olarak düzeltti; yukarıdaki paket/bağımlılıklar bu sonuçlarla eşlendi. Carson staged takas yamasında yeni eyleme dönük hata bulmadı. Onun ham kanıt denetimi şu sonuçları verdi:

- Mevcut üç staged blob, `UE-FROZEN.json` kaydındaki `bf441a5…`, `58a937e…`, `b6351a5…` ile eşleşiyor; dosya SHA256'ları ve mevcut DLL de main kabul kaydına uyuyor.
- Ham build çıktısında iki değişen C++ dosyasının derlenmesi, DLL link'i ve `Result: Succeeded` var. Transfer ham logunun hash'i doğrulandı: **2.644 benzersiz kontrol, 336 vaka, sıfır FAIL**. Bu sonuç yeni test koşusu değil, mevcut kanıtın bağımsız incelemesidir.
- Aynı fixture'larla eski üretim kodu baseline'ında sekiz retirement vakasında **24 FAIL** bulunuyor; düzeltilmiş kodda bunlar geçiyor. Bu, yalnız test sayısından daha güçlü regresyon kanıtıdır.
- Native retirement hatası için kanıt yeterli. Gerçek ağ/PIE, garbage collection ve iki karakterin kayıtlarının çökme/başarısızlıkta ortak işlem halinde korunması ayrıca açık; iki bağımsız `SaveCharacter` çağrısı ortak transaction değildir.

Ham kabul kaydı: `C:/Users/OZAN CIDIK/AppData/Local/Temp/mmo_trade_gold_atomicity_20261014/PARENT_MAIN_ACCEPTANCE.json`. Ayrıntılı görev kaydı, takas worktree'sindeki `docs/tasks/TRADE_GOLD_ATOMICITY_2026-10-14.md` dosyasındadır; bu belge ve başka görevin index'i bu analizde taşınmadı/değiştirilmedi. “Takas test edilmedi” iddiası bu snapshot için kullanılmamalı.

Sonraki uygulama adımı, mevcut takas paketinin sahibiyle kanıt/index/doc kabulünü kapatmak ve Aşama 0'ın eksik Winter NetSmoke + iki oyunculu PIE kapılarını tamamlamaktır. Kaynak motor ve servis sözleşmesi hazırlığı bunlarla paralel yürüyebilir. Bu belge uygulama veya test başlatma yetkisi veren yeni bir otomasyon değildir.
