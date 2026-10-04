# MCP Pilot Değerlendirmesi: possess-unreal-mcp

Tarih: 2026-10-04. Durum: yalnız araştırma; hiçbir şey kurulmadı, projeye/koda/git'e dokunulmadı.
Kaynaklar: GitHub API ve ham dosyalar (aarx0/possess-unreal-mcp, ChiR24/Unreal_mcp) ile Epic dokümantasyonu. Kaynak kodun yalnız ilgili dosyaları okundu; **derlenmedi, çalıştırılmadı**. Okunamayan veya çıkarım olan her şey "doğrulanmadı" diye işaretlidir.

## 1. Araç gerçekte ne yapıyor

### Künye
| Alan | Değer |
|---|---|
| Depo | https://github.com/aarx0/possess-unreal-mcp (tek geliştirici: "Aaron") |
| Yıldız / fork / açık issue | 1 / 0 / 0 |
| Oluşturma / son push | 2026-07-26 / 2026-07-27 (bugüne göre yaklaşık 10 hafta hareketsiz) |
| Commit geçmişi | 30 commit, 2026-07-20 ile 07-27 arası (tek haftalık yoğun çalışma) |
| Lisans | MIT (LICENSE satırı: "Copyright (c) 2025 Unreal Engine MCP Server Contributors") |
| Dil | C++ (UE eklentisi) |
| UE sürümü | README: "geliştirildi ve test edildi: UE 5.7", diğer 5.x "test edilmedi". Bizim sürüm 5.7 ile eşleşiyor |
| Sürüm/etiket | Release yok; CHANGELOG yalnız "Unreleased" |

Köken notu (doğrulanmadı, güçlü çıkarım): eklenti adı (`McpAutomationBridge`) ve lisans telif satırı, daha popüler `ChiR24/Unreal_mcp` ile birebir aynı. Bu depo büyük olasılıkla onun bir türevi/sert fork'u. ChiR24 deposu: 902 yıldız, 171 fork, MIT, son push bugün (2026-10-04), 15 açık issue. Yani aynı kod soyunun aktif bakımlı kolu ayrı bir depoda duruyor.

### Mimari
- Sidecar süreç yok (Node/Python gerekmez). Editörün içine yüklenen bir C++ eklentisi (`plugins/McpAutomationBridge`) kendi HTTP sunucusunu açar: `POST http://127.0.0.1:3123/mcp`, JSON-RPC 2.0 (MCP Streamable HTTP; yanıtlar SSE ile akabiliyor).
- Katmanlar: araç şeması (`Private/MCP/Tools/McpTool_*.cpp`), eylem yönlendirme (`McpConsolidatedActionRouting.h`), işleyiciler (`McpAutomationBridge_*Handlers.cpp`). Editör işlemleri oyun thread'inde çalışır.
- Yaklaşık 22 "birleşik" araç, toplam ~1000 eylem (README iddiası; **doğrulanmadı**, sayılmadı). Araç listesi (kaynak dosya adlarından): `control_actor`, `control_editor`, `inspect`, `manage_asset`, `manage_blueprint`, `manage_level`, `manage_level_structure`, `manage_ai`, `manage_input`, `manage_sequence`, `manage_effect`, `manage_audio`, `manage_geometry`, `manage_networking`, `manage_gas`, `manage_character`, `manage_combat`, `manage_inventory`, `manage_interaction`, `animation_physics`, `build_environment`, `system_control`.

### Bizi ilgilendiren eylemler (docs/handler-mapping.md'den)
| İhtiyaç | Araç / eylem | Not |
|---|---|---|
| Blueprint oluştur / derle | `manage_blueprint`: `create_blueprint`, `compile_blueprint` | |
| Blueprint reparent | `manage_blueprint`: `set_parent_class` | |
| Blueprint değişken / düğüm | `add_variable`, `add_node` (+ README: düğüm bağlama, fonksiyon, bileşen, SCS şablonu varsayılanları, grafik otomatik düzen) | Graf düzenlemesi en değerli fark; kalitesi **doğrulanmadı** |
| Aktör yerleştirme | `control_actor`: `spawn_actor`, `set_actor_transform` | |
| NavMesh | `manage_ai`: `create_navmesh_volume`, `rebuild_navigation` | |
| PIE | `control_editor`: `start_pie`, `stop_pie` (+ ekran görüntüsü, viewport kamerası, girdi simülasyonu) | |
| Log okuma | `system_control`: `get_log` (parametreler: count<=2000, sinceSeq, verbosity, category, contains) | Artımlı okuma var |
| Harita yükle / kaydet | `manage_level`: `load_level`, `save_level`, `save_all`; `manage_asset`: `save_asset` | Hepsi ikili .uasset/.umap yazar |
| Malzeme | README: malzeme düğüm grafikleri, decal | Bizde şimdilik öncelik değil |
| C++ | `system_control`: `live_coding_compile`, `run_ubt` + `get_build_status` | Derleme hatalarını yapısal döndürür |
| Rastgele Python / konsol | `system_control`: `execute_python`, `execute_command` | Aşağıda güvenlik bölümü |

### Güvenlik yüzeyi (kaynaktan okunan)
- **Bağlanma**: varsayılan `127.0.0.1`; `bAllowNonLoopback=false` iken loopback dışı adres yazılsa bile `127.0.0.1`'e geri düşer (McpNativeTransport.cpp ~satır 223-226).
- **Kimlik doğrulama**: `bRequireCapabilityToken` + `CapabilityToken` ayarı var ve açıksa POST ve OPTIONS için token zorunlu. Varsayılanı ve token'ın hangi HTTP başlığıyla gönderildiği **doğrulanmadı**; README'de belgelenmemiş. Varsayılan kurulumda **kimlik doğrulama yok** kabul edilmeli.
- **Origin/CORS**: yalnız localhost/127.0.0.1/::1 origin'lerine izin veriliyor (tarayıcıdan gelen çapraz-site isteklerine karşı yararlı). Host başlığı doğrulaması **doğrulanmadı**.
- **Sınırlar**: gövde en çok 5 MB, bekleyen istek en çok 64, eşzamanlı bağlantı sınırı var; istemci/token başına hız sınırı yok.
- **Rastgele kod**: `execute_python` (inline `code` veya `file`) açık bir araç. Yalnız "modal-API koruması" var (`reload_packages`, `*_with_dialog`, `EditorDialog` varsayılan engelli; donmayı önlemek için, güvenlik için değil). Yani bağlanan her istemci editör süreci yetkisinde (dosya sisteminde) keyfi Python çalıştırabilir. Bu bizim headless `ExecutePythonScript` ile aynı yetki, ama artık bir ağ soketinden erişilebilir.
- **Konsol komutları**: kara liste var (shutdown/quit/crash/ubt/delete vb.), `py`/`python` komutu engelli, bazı Python parçacıkları (`import os`, `subprocess.`, `exec(`) engelli. Kara liste güvenlik sınırı sayılmamalı; `execute_python` ayrı eylem olarak zaten açık.
- Epic'in UE 5.8 resmi MCP'si de benzer: "kimlik doğrulama katmanı yok, uzak kullanım için tasarlanmadı, loopback dışı Origin'i reddeder".
- Dolaylı risk: prompt injection. Ajan, içine kötü niyetli metin giren bir içerik (indirilen varlık, web sayfası) okursa `execute_python` ile yerel dosyalara erişebilir. Pilotta ajana internet içeriği okutmayın.

### Bilinen zayıflıklar (kendi dokümanlarından)
- **Aktarım düşmesi** (`docs/transport-mid-call-drop-problem.md`): editörün oyun thread'i doyunca (harita açılışı, asset registry taraması, GC) istemci "transport dropped mid-call" alır; işlem sunucuda gerçekleşmiş olabilir ve "istek indi mi?" sorgusu yok. Silme/oluşturma/yeniden adlandırma gibi idempotent olmayan işlemlerde çift uygulama riski. Belge tarihi itibarıyla çözülmemiş.
- Kaydetme (`safe-asset-saving.md`): editörün normal kayıt hattı otomasyonda çökme riski taşıdığı için eklenti kendi paket yazma yolunu (`McpDirectPackageSave`) kullanıyor; Git için "kontrol etmeden dosya değişmiş görünür" davranışı. Kilit olmaması git ile korunmak için iyi, ama ham paket yazımı doğrulamayı atlıyor.
- Derleme: tam yeniden derleme `-NoUBA` ile yapılmalı, yoksa Live Coding `.voltbl` hatası verir. Bizim `Build.bat` komutumuza bu bayrak eklenmeli (şu an CLAUDE.md'de yok).
- Şema değişince istemci yeniden başlatılmalı.
- 3500 satırlık TODO.md "gerçek kullanımdan hata günlüğü" olarak duruyor: ürün olgunlaşmamış.
- Test ve CI: depoda otomatik test/CI dosyası görmedim (doğrulanmadı, ağaç listesinde özellikle aranmadı).

## 2. Bizim kullanım örnekleri için uygunluk

Mevcut yöntem: `UnrealEditor-Cmd -ExecutePythonScript Scripts/*.py -unattended -NullRHI` (örnek: `Scripts/SetupWinterNavigation.py`, idempotent, etiketle bulma). Editör arayüzü gerekmez, tekrarlanabilir, git'e dosya olarak girer.

| Kullanım | Headless Python | MCP (possess) | Yorum |
|---|---|---|---|
| Blueprint reparent | Çalışıyor (daha önce yapıldı) | `set_parent_class` | Fark yok; MCP gerektirmez |
| Harita yerleşimi (aktör/PlayerStart/spawner) | Çalışıyor, betik repoda | `spawn_actor`, `set_actor_transform`, `save_level` | Python daha iyi: betik tekrar çalıştırılabilir ve incelenebilir |
| NavMesh | Çalışıyor (`Setup/Extend*Navigation.py`) | `create_navmesh_volume`, `rebuild_navigation` | Eşdeğer. Yürünebilirlik doğrulaması ikisinde de ayrı iş |
| PIE çalıştırıp log okuma | Zor: `-NullRHI` headless ortamda PIE sınırlı; mevcut `SmokeTest.ps1` ayrı yol | `start_pie`, `get_log` (artımlı), `stop_pie`, ekran görüntüsü | **MCP'nin en net artısı**; ama GPU'lu etkileşimli editör gerektirir, gözetimsiz gece çalıştırması riskli |
| Blueprint **grafiği** düzenleme (düğüm ekle/bağla) | Python API'si pratikte sınırlı (5.7 için kesin liste **doğrulanmadı**) | `add_node`, bağlama, graf düzeni | **Asıl fark burada**; ancak bu depo için kalite/güvenilirlik **doğrulanmadı** |
| Malzeme grafiği, Niagara | Kısıtlı | Var | Şu an önceliğimiz değil |
| Canlı gözlem (aktör inceleme, BT/Blackboard) | Yok | `inspect`, `get_agent_state` | Hata ayıklamada yararlı |

Artılar (MCP):
- İkili varlıklarda grafik düzeyi düzenleme ve PIE kontrol/log döngüsü: şu an ajanların yapamadığı iki şey.
- Sidecar yok, kurulum tek eklenti; yapısal derleme hatası dönüşü.

Eksiler (MCP):
- Editör açık ve etkileşimli olmalı. Bizim kuralımız "C++ derlemek için editör kapalı olmalı"; ikisi çakışır. Live Coding ile yumuşar ama CLAUDE.md'deki akışı değiştirir.
- Betik izi yok: eylemler sohbette kalır, repoda yeniden üretilebilir kayıt oluşmaz (Python betikleri bu açıdan üstün). Pilotta her eylem sonucu betiğe veya belgeye dökülmeli.
- Çift uygulama riski (aktarım düşmesi); tek geliştiricili, 1 yıldızlı, 10 haftadır hareketsiz, test/sürüm yok.
- Güvenlik: varsayılan kimlik doğrulamasız yerel soket + `execute_python`.
- Kaynak, kendi test haritalarına göre ayarlı olabilir; MMORPG haritalarımızda (büyük harita / World Partition) davranışı **doğrulanmadı**.

Alternatifler (kurulmadı, doğrulanmadı):
- `ChiR24/Unreal_mcp`: aynı kod soyu, 902 yıldız, bugün güncellenmiş, UE 5.0-5.8 desteği iddiası; mimari TypeScript sunucu + C++ köprü (ek Node süreci). Pilotta karşılaştırma adayı.
- Epic resmi "Unreal MCP" (UE 5.8, deneysel, kimlik doğrulaması yok, loopback). Bizim sürüm 5.7 olduğundan şimdilik kullanılamaz; 5.8'e geçiş planlanırsa daha doğru kaynak odur.

## 3. Pilot planı (kurulum önerisi; hiçbiri çalıştırılmadı)

### İzolasyon
Ana kopyaya ve çalışan geliştirmeye dokunulmaz. Depo: `.git` 944 MB, `Content` 1.1 GB, git'te 706 ikili `.uasset/.umap`, Git LFS yok (`.gitattributes` yok). İki seçenek:
- (A, önerilen) Ayrı klasöre tam kopya: `robocopy` ile `MyProject` -> `MyProject_McpPilot` (`Binaries/Intermediate/DerivedDataCache/Saved` hariç). Hem git dalı hem fiziksel izolasyon verir; ana proje etkilenmez.
- (B) `git worktree add ../MyProject_McpPilot -b pilot/mcp`: diski az kullanır ama `.gitignore` ile dışlanan klasörler yoktur, ilk açılışta shader/DDC yeniden üretilir; `.git` nesne deposunu paylaşır.

### Komut dizisi önerisi (PowerShell; yalnız öneri)
```powershell
# 0) Önkoşul: ana projedeki modifiye dosyalar commit'li veya stash'li olsun (şu an M durumunda dosyalar var)
$src = "C:\Users\OZAN CIDIK\Desktop\ozan online\MyProject"
$dst = "C:\Users\OZAN CIDIK\Desktop\ozan online\MyProject_McpPilot"

# 1) İzole kopya (Binaries/Intermediate/DDC/Saved hariç)
robocopy $src $dst /E /XD Binaries Intermediate DerivedDataCache Saved .idea /NFL /NDL

# 2) Pilot dalı + başlangıç görüntüsü
git -C $dst checkout -b pilot/mcp
git -C $dst add -A
git -C $dst commit -m "pilot baslangic anlik goruntusu"
git -C $dst tag pilot-baseline

# 3) Eklentiyi sabit bir commit'e kilitleyerek al (SHA'yı bu belgeye yaz)
git clone https://github.com/aarx0/possess-unreal-mcp "$env:TEMP\possess-unreal-mcp"
git -C "$env:TEMP\possess-unreal-mcp" rev-parse HEAD
#    Önce kaynağı incele (özellikle SystemControlHandlers ve McpNativeTransport), sonra kopyala:
Copy-Item "$env:TEMP\possess-unreal-mcp\plugins\McpAutomationBridge" "$dst\Plugins\McpAutomationBridge" -Recurse

# 4) Yapılandırma: Config\DefaultGame.ini (Saved/Config DEĞİL)
#    [/Script/McpAutomationBridge.McpAutomationBridgeSettings]
#    bEnableNativeMCP=True
#    NativeMCPPort=3123
#    bAllowNonLoopback=False
#    bRequireCapabilityToken=True
#    CapabilityToken=<rastgele uzun dize; repoya girmesin; istemciden hangi başlıkla gönderileceği doğrulanmadı>

# 5) Derleme (editör kapalı; -NoUBA zorunlu)
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64 Development "-Project=$dst\MyProject.uproject" -WaitMutex -NoUBA

# 6) Editörü pilot projeyle aç; durum çubuğunda MCP uç noktasının canlı olduğunu gör.
#    netstat -ano | findstr 3123   -> yalnız 127.0.0.1 üzerinde dinlemeli
# 7) İstemci kaydı (yalnız pilot oturumunda):
#    claude mcp add --transport http unreal-mcp http://127.0.0.1:3123/mcp
#    (token başlığı gerekiyorsa --header ile; biçim doğrulanmadı)
# 8) Pilot bitince: claude mcp remove unreal-mcp ; editörü kapat ; ini'de bEnableNativeMCP=False
```
Ek önlemler: pilot sırasında ana projedeki editör açık olmasın (port/DDC çakışması); pilot oturumunda ajana internet erişimi/başka MCP verilmesin.

### Üç küçük görev ve başarı ölçütleri
**Görev 1: salt-okunur gözlem + log (risk sıfıra yakın).**
Pilot kopyada haritayı yükle, `inspect`/aktör listesiyle `MMO_Winter_PlayerStart` ve NavMesh hacmi bilgisini oku, `get_log` ile son hataları çek.
Ölçüt: aktör sayısı ve konumları headless Python çıktısıyla birebir; `get_log` `sinceSeq` ile artımlı okur; bir saatlik oturumda "transport dropped" sayısı 0-1; `git status` temiz (yazma yok).

**Görev 2: tek kullanımlık Blueprint, reparent ve graf düzenleme (asıl değer testi).**
`/Game/Pilot/BP_PilotChar` oluştur (parent `ACharacter`), `set_parent_class` ile `AMMOCharacter`'a çevir, bir değişken ve basit bir olay grafiği (ör. `OnCharacterDied` -> Print String) `add_node` ile kur, `compile_blueprint`.
Ölçüt: derleme hatasız; Blueprint editöründe grafik beklenen düğüm/bağlantıları gösteriyor (elle doğrula); aynı işlemin Python ile yapılamadığı en az bir adım MCP ile güvenilir yapıldı; diske yazılan tek yer `Content/Pilot/*`; işlem sayısı ve elle düzeltme sayısı kaydedilir.

**Görev 3: harita kopyasında yerleşim + NavMesh + PIE + log döngüsü.**
Winter haritasını `/Game/Pilot/Winter_copy` olarak çoğalt (ana haritayı DEĞİL), `spawn_actor` ile PlayerStart, `create_navmesh_volume` + `rebuild_navigation`, `save_level`; sonra `start_pie`, 60 sn bekle, `get_log` ile hata/uyarıları çek, `stop_pie`.
Ölçüt: sonuç `Scripts/SetupWinterNavigation.py` çıktısıyla aynı (aktör etiketleri, hacim sınırları); PIE hatasız başlıyor ve loga yapısal erişim var; editör çökmesi 0; kayıt sonrası harita Python ile yeniden yüklenip aktörler sayılınca eşleşiyor.

Genel kabul ölçütü: 3 görev de tamamlanır, "çift uygulama" veya veri bozulması olayı yok, Görev 2'de Python'un yapamadığı en az bir şey MCP ile güvenilir yapılır. Başarısızlık: bir görev ikili dosyayı yarım bırakır, editör modal pencerede donar veya aktarım düşmesinde sonucu doğrulayamazsak.

### Risk ve geri alma (ikili dosya koruması)
- **Tek yazan kuralı**: aynı anda tek yazıcı (headless Python, MCP, elle editör). İkili `.uasset/.umap` birleştirilemez; çakışmada yalnız bir sürüm seçilir.
- **Her görev öncesi anlık görüntü**: `git add -A; git commit -m "oncesi: gorev N"`. Görev sonrası `git diff --stat` ile hangi ikili dosyaların değiştiği görülür (beklenen yol dışında `Content/` değişikliği = başarısızlık).
- **Geri alma**: `git restore --source=pilot-baseline --worktree -- Content Config` (dosya bazlı) veya pilot kopyayı tamamen silmek (ana proje hiç etkilenmediği için en güvenlisi). Ana projeye hiçbir MCP değişikliği taşınmaz; sonuçlar yalnız Python betiğine veya belgeye çevrilerek aktarılır.
- **Depo şişmesi**: her kayıt tüm `.umap`'i git geçmişine ekler (LFS yok). Pilot dalı `.git`'i büyütür; pilot kopyada çalış, dalı ana depoya birleştirme. Kalıcı benimsenirse Git LFS (`*.uasset`, `*.umap`) ayrı bir karar olarak planlanmalı.
- **Çökme/donma**: modal pencere editörü donduruyor (belgeye göre); gözetimsiz gece çalıştırması önerilmez. Editör çökerse kayıtsız değişiklikler kaybolur.
- **Gizli bilgi**: pilotta API anahtarı ve kişisel dosyalar proje klasörü dışında kalsın; `execute_python` yerel dosya sistemine erişebilir.

## 4. Karar

**ERTELENİR.** Gerekçe:
1. Olgunluk: 1 yıldız, tek geliştirici, 30 commit/1 hafta, sürüm yok, 10 haftadır hareketsiz, kendi TODO'su 3500 satırlık hata günlüğü. 706 ikili varlıklı MMORPG deposunu bu araca emanet etmek için erken.
2. Mevcut yöntem harita yerleşimi, NavMesh, reparent için zaten çalışıyor; tekrarlanabilir ve incelenebilir. MCP bunlara anlamlı katkı yapmıyor.
3. Gerçek kazanım iki alanda: Blueprint grafik düzenleme ve PIE/log döngüsü. Mantığı C++ modülünde tuttuğumuz sürece bunlar şu an darboğaz değil.
4. Güvenlik: kimlik doğrulama varsayılan olarak kapalı/belgelenmemiş, `execute_python` açık; ajanın internet içeriği okuduğu otonom oturumlarla birleşince risk artar.
5. Zamanlama: UE 5.8'in resmi MCP'si deneysel olarak çıktı; 5.8'e geçilirse Epic'in sunucusu veya ChiR24 (aktif bakımlı) daha sağlam başlangıçtır.

Yeniden değerlendirme koşulları: (a) Blueprint grafiğini elle düzenleme sürekli engel olursa, (b) possess deposu yeniden aktifleşir ve etiketli sürüm çıkarırsa, (c) 5.8'e geçiş planlanırsa. O durumda yukarıdaki pilot, izole kopyada 3 görevle uygulanır; karşılaştırma için `ChiR24/Unreal_mcp` da aynı görevlerle denenebilir.

## Doğrulanmadı listesi
- "~1000 eylem" ve "22 araç" sayıları (README iddiası; yalnız dosya adlarından 22 araç dosyası görüldü).
- `add_node`/graf bağlama eylemlerinin gerçek kalitesi ve UE 5.7'de kararlılığı (hiç çalıştırılmadı).
- CapabilityToken'ın varsayılan değeri ve istemci başlık biçimi; Host başlığı doğrulaması.
- `execute_python` uygulama ayrıntıları (SystemControlHandlers.cpp okunmadı); eylem adları `handler-mapping.md` özetinden alındı.
- possess ile ChiR24 arasındaki türev ilişkisi (yalnız ad ve telif satırı benzerliği).
- Depoda test/CI olup olmadığı.
- Bizim büyük harita / World Partition kurulumunda davranış.
- UE 5.7 Python API'sinde Blueprint düğüm düzenleme kapsamı.
