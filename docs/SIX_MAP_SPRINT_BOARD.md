# Altı harita sprint panosu

**Tarih:** 2026-10-15 · **Taban:** `4e0f0c5` + incelenen yerel belge değişiklikleri.
Kaynaklar: [tasarım ve kabul kapıları](SIX_MAP_PLAN.md), [teknik değerlendirme ve O70 kanıtı](TPM_ROADMAP_REVIEW_2026-10-15.md), [uzman iş akışı](AGENT_WORKFLOW.md), [kararlar](DECISIONS_NEEDED.md).

Bu pano uygulamadan önce paylaşılan yürütme sırasıdır. Bu dosyanın hazırlanması sırasında kod, build veya runtime testi yapılmadı. Sprint kimlikleri sabittir; eski A etiketleri aşağıdaki eşlemeden okunur. Sprintler sabit iki haftalık taahhüt değil, kabul kapısı olan iş paketleridir.

## Yetki ve değişmez kapsam

- Kullanıcının yeni otonom geliştirme/heartbeat yetkisi yalnız mevcut sohbet içindir. Eski sohbetin durdurma talimatı korunur; oraya mesaj gönderilmez. Koordinatör mevcut duraklatılmış heartbeat'i bu sohbete yönlendirir; yeni scheduler yaratılmaz. Bu pano otomasyon ayarını değiştirmez veya yeniden başlatıldığını iddia etmez. Yeni `durdur` talimatında yeni iş açılmaz ve otomasyon durdurulur.
- H2 İnsan **Lv25+**, H3 Ork **Lv25+**: giriş/kalış üst sınırı yok; Lv59/Lv80'de zorunlu çıkış yok. Gelecekte genel tavan yükselse de bu iki haritanın uygunluk kuralı değişmez. H4/H5 Lv59+ açılması H2/H3 erişimini kaldırmaz. İçerik bantları, mob seviyeleri ve mevcut genel seviye tavanı ayrı ve değişmeden kalır.
- Altı ayrı harita süreci, PostgreSQL + ara servis, SteamID ve Steam-first korunur. PvP H6 Lv55+; PvP XP kaybı var, eşya/altın düşmez. Pazar alfada zorunludur. Parola sistemi, offline pazar, kanal/instance ve kamuya açık 7/24 yayın bu programa eklenmez.
- O70 üç staged takas dosyasının native/build kanıtı bağımsız incelenmiştir: **2.644 kontrol / 336 vaka PASS**. Aynı testler gerekçesiz tekrarlanmaz, dosya sahipliği devralınmaz. Gerçek uzak istemci/PIE ve iki kaydın çökmede ortak transaction garantisi bu sonuçla kapanmaz.

## Sprint sırası

**Durum:** AKTİF HEDEF = ilk sıradaki iş, henüz bu pano kapsamında çalıştırılmadı; HAZIRLIK = bağımsız hazırlık yapılabilir; PLAN = bağımlılık kapısı bekleniyor. Hiçbir sprint TAMAM olarak işaretlenmedi.

| ID / teslim | Eski plan | Bağımlılık | Ana sahip / bağımsız kabul | Kapanış kapısı | Durum |
|---|---|---|---|---|---|
| **S00 Taban** | Aşama 0, §11 adım 1–3 | Yok | devops + qa-automation; kullanıcı PIE, orchestrator kabul | Winter NetSmoke harita/port/join/welcome/hata kanıtı; iki oyunculu Summer↔Winter, NPC/görev/parti/HUD, yeniden giriş ve takasta kopma. Önceki build/headless kanıtı adayla eşleştirilir. | **AKTİF HEDEF:** S00.01; PIE açık |
| **S01 Dedicated** | **A1** | S00 tam kabul; betik/kurulum hazırlığı şimdi | devops + ue5-developer; QA, performance | Gerçek Server ve Editor build; Summer/Winter dedicated bağlantısı; iki portta iki süreç; CPU/RAM ve uzak bildirimler. Kaynak motor dış önkoşul. | HAZIRLIK: S01.01 |
| **S02 Yerleşim tablosu** | Aşama 1 | S00; S01 bitişi gerekmez | ue5-developer + npc-ai; QA | Summer 18/Winter 17 cep, NPC 5/4; koordinat/bant/MaxMobs eşdeğer; veri, smoke ve PIE eşdeğerliği. | PLAN |
| **S03 Kalıcı ırk** | Aşama 2 | S02; ara depo/eski kayıt kararları | ue5-developer + game-designer; QA/network | Tek seçim, replikasyon, eski kayıt ve yeniden giriş/geçişte veri korunması. Ork seçimi S10'a kadar sunucuda reddedilir. | PLAN |
| **S04 DB ve servis** | **A2** | S03; servis dili/posta biçimi kararları; S01 bitişi gerekmez | backend + ue5-developer; QA/security | Asenkron JSONB/revision; eski yazı ret; `.sav` + yan veri idempotent içe aktarma; DB kapalıyken durum ilerlemez; DB modu ve bayraksız yol doğrulanır. | PLAN; sözleşme hazırlığı bağımsız |
| **S05 Steam** | **A5** | S01 + S04 + App ID/anahtar/test hesapları | backend + ue5-developer + network; QA/security | İki gerçek hesapla doğrulanmış SteamID; anahtar yalnız serviste; ret/zaman aşımı giriş açmaz. Relay denemeleri kaydedilir; başarısızsa IP/özel ağ yedeği kabul, Steam kimliği korunur. | PLAN |
| **S06 Sahiplik ve bilet** | **A3** | S05 + S04 + S01 | backend + network + ue5-developer; QA/security | Başkasının karakteri, sahte/süresi geçmiş/tekrar kullanılan bilet ve çift giriş ret; eski karakter sahiplenmesi; dedicated yönetici sunucuda doğrulanır. | PLAN |
| **S07 Oyuncu başına geçiş** | **A4** | S01 + S04 + S05 + S06 | network + backend + ue5-developer; QA/security/performance | İki dedicated/iki istemci ayrı haritalarda; yalnız isteyen geçer; tek kullanımlık hedef bileti/kilit-kira devri; eski sunucu yazısı ret; hedef arızası/çökme kurtarma ve kayıt korunması. | PLAN |
| **S08 Bölge kapıları** | Aşama 3 | S07 | ue5-developer + network; QA | Hedefte yeniden denetim. H2/H3 doğru ırkta Lv24 ret; Lv25/59/80 kabul, mevcut tavanı değiştirmeyen gelecekteki Lv>80 uygunluk fikstürü kabul; yanlış ırk ret, üst sınırla çıkarma yok. H3 gerçek kabulü S10'da tamamlanır. | PLAN |
| **S09 İnsan haritaları** | Aşama 4 + 5 | S08; harita/içerik eşlemesi kararları | asset-integration + npc-ai + ue5-developer; QA | Önce H4 kabul, sonra içerik taşıma; H1/H2/H4 Lv1–80 yolu, eski kayıtlar; probe/soak/smoke. H4 Lv59+; H2 erişimi sürer. | PLAN |
| **S10 Ork ve sosyal** | Aşama 6 | S09; karşı ırk ticaret/fısıltı kararları | npc-ai + ue5-developer + network/backend; QA/security | H3/H5 ayna içerik, Ork seçimi açılır; İnsan/Ork aynı anda ayrı krallıklarda; ırk/sosyal kuralları, H3 üst sınır olmadan giriş/kalış, iki zincirde kayıt korunması. | PLAN |
| **S11 PvP** | Aşama 7 | S10; kalan PvP tasarım kararları | game-designer + ue5-developer + asset-integration; QA/network/security | H6 Lv54 ret/Lv55 kabul; ırk kampı/güvenli bölge; karşı ırka hasar; onaylanan XP cezası, seviye düşmez ve eşya/altın korunur. Oran taslakları ölçüm öncesi kesin sayılmaz. | PLAN |
| **S12 Pazar** | Aşama 8 | S10 + S04; vergi/slot/çevrimiçi tezgâh kararları | backend + ue5-developer + game-designer; QA/network/security | H1 tezgâhı; eşya kilidi ve tek DB satış işlemi; çift alıcıdan biri kazanır; kopma/DB arızasında ikileme/kayıp yok; restart kayıtları doğru. | PLAN |
| **S13 Alfa sertleştirme** | Aşama 9 | S11 + S12, S05 gerçek kabulü | devops + qa-automation + orchestrator; kullanıcı/performance | Paketli altı süreç, iki gerçek makine; eşzamanlı İnsan/Ork yolları, PvP, pazar, Steam girişi/dağıtım ve DB yedeğinden dönüş; tüm ilgili kapılar kanıtlı. | PLAN |

S00'dan sonra **S01 ∥ S02 → S03 → S04**; S01 ve S04, S05'te birleşir. S11/S12, S10'dan sonra ayrık kapsamlarla paralel ilerleyebilir; UE build/runtime yine sırayladır. Kaynak motor, PIE veya Steamworks beklenirken bağımsız betik, şema/API sözleşmesi, metin ve kurulum hazırlığı sürer; bekleyen kapı geçilmiş sayılmaz ve bağımlı gameplay entegrasyonu açılmaz.

## Bir sonraki kesin görev: S00.01 — Winter ağ kanıtı

- **Sahip:** devops uygulama; qa-automation bağımsız runner/kanıt incelemesi; network bağlantı kabulü; orchestrator entegrasyon. İzole worktree'nin base SHA'sı ve kullanılan DLL/asset kaynağı görev kaydında sabitlenir; dirty main sessizce kopyalanmaz.
- **Yazma allowlist'i:** yeni `Scripts/InvokeWinterNetSmoke.ps1` ve yalnız bu görevin `docs/tasks/` manifest/kanıt kaydı. Mevcut `Scripts/NetSmokeTest.ps1` ilk görevde salt okunur çağrılır; ek değişiklik gerekirse koordinatör ayrı sahiplik atar. Takasın üç staged dosyası, ajan tanımları ve ana checkout oyuncu verisi kapsam dışıdır.
- **İş:** önce mevcut Winter ağ kanıtını ara ve adayla eşleştir. Yeterli kanıt yoksa mevcut NetSmoke akışına süre sınırı olan güvenli wrapper hazırla: ortak `agent-build.lock`, process/port ön kontrolü başarısızsa çalışmama, izole proje/veri yolu, benzersiz log, yalnız başlattığı process nesneleriyle sınırlı cleanup. Kilit altındayken yalnız kendi child NetSmoke çağrısı yürür; başka UE işi eşzamanlı açılmaz.
- **Koşu kapısı:** worktree'nin uygun derlenmiş modülü/assetleri ve boş UE çalışma sırası doğrulanmadan başlatma. Hazır değilse eksik önkoşulu kaydet, ana checkout'ta deneme veya süreç kapatma yapma; bağımsız S01.01 hazırlığına devam et. Yeni C++ gerekmedikçe O70 test/build paketini tekrar koşma.
- **Kabul:** runner bounded süre içinde exit sonucu verir; kendi sunucusu `Winter_scene` ve seçilen portu yüklemiştir; aynı koşuda server join ≥1, client welcome ≥1, iki logda hata 0 ve gözlem boyunca süreçler canlıdır. Komut/aday SHA/DLL kimliği/log yolları/exit ve çalışmayan kontroller kaydedilir. Port/envanter/erken process çıkışı/zaman aşımı hataları PASS üretemez. Ana Saved ve yabancı süreçler korunur. Bu kanıt PIE/HUD/gameplay RPC veya temiz native shutdown kabulü değildir.

**Paralel hazır iş S01.01:** devops, ayrı worktree'de `Scripts/build.ps1` ve motor yolu/hedef seçimi sözleşmesini inceler ve sınırları belirli betik hazırlığını yapar; Server target taşıma/C++ yazarlığı ue5-developer'a ayrılır. Kaynak motor yolu, olmayan Client hedefi, Saved koruması ve ortak kilit davranışı somut kabul listesine bağlanır. Motor indirme/build/kurulum yapılmış sayılmaz. S00.01 wrapper dosyasına yazmaz.

## On iki rolün yönlendirmesi

| Rol | Bu programdaki görevi |
|---|---|
| orchestrator | Her işte base/allowlist/ajan kimliği, disjoint sahiplik, sprint kabulü; mevcut heartbeat'in bu sohbet hedefi. |
| ue5-developer | Tüm UE C++ uygulaması; S01 hedef, S02 yerleşim, S03 ve S05–S12 gameplay bağlantıları. |
| network | RPC/replikasyon, S05–S08 kimlik/geçiş, S10 sosyal ve S11/S12 ağ sözleşmeleri. |
| backend | S04 servis/depolar, S05/S06 kimlik, S07 kilit-kira, S10 varlık, S12 atomik satış. |
| qa-automation | Her gameplay paketinde uygulayandan bağımsız son diff incelemesi; S00/S13 kanıt ve regresyon kapıları. |
| game-designer | S03 seçim, S08 erişim, S09/S10 içerik eşlemesi, S11 ceza, S12 ekonomi; verilmiş kararları yeniden açmaz. |
| npc-ai | S02 yerleşim eşdeğerliği, S09–S11 NPC/mob/görev/nav davranışı. |
| security | Authority/kimlik/ekonomi/kayıt/geçiş incelemesi; salt okunur, S03–S12 ilgili paketlere. |
| asset-integration | S09–S11 harita kopyası, PlayerStart, giydirme ve editor yönetimli Content; atanmış binary scope. |
| devops | S00 güvenli runner; S01 engine/hedef/betik; ortak UE kilidi; S13 paketleme/yedek/dağıtım hazırlığı. |
| performance | S01/S07/S13 CPU/RAM/replication ölçümü; salt okunur, profiler kanıtı olmadan kapasite sözü vermez. |
| documentation | Her teslimde pano/karar/test kanıtı; yalnız atanmış belgeler. |

Bu tablo dispatch kanıtı değildir. Koordinatör ilgili gerçek uzmanları başlatır; tüm on iki rolün aynı anda çalışması gerekmez. Alt görevler descendants açmaz. Her yazara izole worktree; main paylaşımı yalnız açıkça ayrık kapsamla. Build/runtime ortak kilitle sırayla; yabancı süreç durdurma ve gerekçesiz retest yok.

## Yaşayan kayıt ve dış önkoşullar

### Başlatma kaydı — 2026-10-15

- Mevcut `myproject-otonom-geli-tirme` heartbeat'i yeni scheduler açılmadan **ACTIVE** yapıldı ve bu sohbetin kimliğine (`01a138cc-1bfe-76e0-bb67-797f3f97d180`) bağlandı. Kontrol aralığı 5 dakika. Yerel çalışma için bilgisayar ve uygulama açık olmalıdır.
- **S01.01 atandı:** Wegener (`01a13fdd-f402-70e1-89fb-88abf0c4adef`), runtime `default`, DevOps uygulama uzmanı. Worktree: `C:/Users/OZAN CIDIK/.codex/worktrees/six-map-sprint0/MyProject`, base `4e0f0c5`. Yalnız `Scripts/build.ps1` ve `Scripts/TestBuildScript.ps1`; olmayan Client hedefi, gerçek All hedef listesi, ortak kilit ve süreç ön kontrolü. Gerçek UE build/Clean yetkisi bu alt göreve verilmedi.
- **S00.01 atandı:** Erdos (`01a13fde-a22c-7582-8c44-ade7659aadfc`), runtime `default`, QA/DevOps runner uzmanı. Aynı worktree'de açıkça ayrık dosyalar: `Scripts/InvokeWinterNetSmoke.ps1`, `Scripts/TestWinterNetSmokeWrapper.ps1`, `docs/tasks/S00_01_WINTER_NETWORK_2026-10-15.md`. Önce güvenli wrapper ve oyun başlatmayan testler; gerçek Winter koşusu modül/asset/provenance ve tek UE sahibi kapısını bekler.
- Bu kayıt dispatch bilgisidir; kodun tamamlandığı, testlerin geçtiği veya Winter ağ kabulünün kapandığı anlamına gelmez. Üç önceki uzman devam çağrısı yürütme sınırı hatası verdi; bunlardan yeni inceleme/test sonucu üretilmiş sayılmaz. Yeni worker sonuçları koordinatör tarafından alınacak, diff ve bağımsız kabul sonrası durum güncellenecek.
- Ana checkout'ta bu göreve ait olmayan yeni `MMORace*`, Character/HUD/controller ve `RunSystemTests` düzenlemeleri gözlendi. Sahipliği teyit edilmeden bunlara yazılmaz, stage/commit yapılmaz veya bu programın tamamlanan S03 işi sayılmaz. İzole worker'lar bu düzenlemeleri kopyalamaz; başka bir aracın UE süreçleri kapatılmaz.

Her teslimde: `görev ID / base ve aday diff / gerçek ajan kimliği / owned paths / kod / build / native / ağ / PIE / kanıt / açık risk / tek sonraki görev`. Durumlar yalnız gerçek sonuca göre güncellenir; native PASS, PIE PASS yerine geçmez. Mevcut checklist maddelerine bağlanılır; tekrar checklist üretilmez. Süreler SIX_MAP_PLAN §9.4'teki varsayımlardır; genel tamamlanma yüzdesi kullanılmaz.

Kullanıcı önkoşulları: S00/S02/S03 ve sonraki oyun içi kapılar için PIE zamanı; S01 için kaynak motor erişimi/disk; S04 için PostgreSQL ve servis dili/posta kararı; S05 için Steamworks/App ID/serviste saklanan anahtar ve iki hesap; S09–S11 için editor varlıkları. Eksik bir önkoşul yalnız ilgili kabulü bekletir; bağımsız hazırlığı durdurmaz. Bekleyen kararlar sessizce kesinleştirilmez. Bu pano oluşturulurken otomasyon/uygulama başlatılmadı, stage/commit yapılmadı.
