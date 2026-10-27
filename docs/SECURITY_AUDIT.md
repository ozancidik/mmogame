# Güvenlik Denetimi (Salt Okunur) - MyProject MMORPG

Güncelleme 2026-10-07: Aşağıdaki bulgular 2026-10-04 tarihli denetimin tarihsel
kaydıdır. Y-2'nin kayıtlı ekipmanda seviye şartı kısmı düzeltildi: stats yüklemesinden
sonra bilinen eşyanın RequiredLevel ve slot şartı denetlenir. Uygunsuz ekipman bonus
vermeden çantaya, çanta doluysa depoya taşınır. Mevcut kayıpsız yükleme politikası
depo kapasitesinin aşılmasına izin verir; bilinmeyen eşya kimlikleri korunur ve
bonus vermez. UE izole regresyonunda 11 kontrol geçti. Gerçek dosyadan yeniden giriş,
sahip-istemci replikasyonu ve HUD PIE'de oynanmadı. Bu kısmi düzeltme Y-2'yi tümüyle
kapatmaz; dosya imzası, hesap kimliği ve diğer doğrulama maddeleri için ayrı çalışma
gerekir. Ayrıntı: CHECKLIST_FIXES_2026-10-05.md §39.

Ek güncelleme 2026-10-07: Y-2'nin mastery/harcanmamış skill puanı bütçesi kısmı
UE'de doğrulandı ve düzeltildi. Lv10'da 50 yatırılmış + 18 harcanmamış puan
yüklenebiliyordu; kazanılabilir toplam 18'dir. Rütbeler ve bakiye artık aynı
seviye bütçesine sığar. Sınıf değişimi tam seviye bütçesini iade eder; bozuk
bakiye/harcama toplamını kullanmaz, taşma veya fazla puan üretmez. 12 izole UE
kontrolü geçti (ayrıntı §43). Ön koşul sunucunun bozuk/değiştirilmiş kayıt
yüklemesidir; uzaktan dosya yazma kanıtı değildir. Eski kayıt uyumluluğu için
geçerli LearnedSkills korunur; bu listenin mastery maliyeti sonraki §49
düzeltmesinde bütçeye katıldı. Kayıt imzası, hesap kimliği ve gerçek istemci PIE
kontrolleri açıktır. Y-2 tümüyle
kapatılmadı.

Ek güncelleme 2026-10-07 (§49): LearnedSkills listesinde ücretli beceri varken
rütbeler sıfır ve harcanmamış puanlar tam bütçe olabiliyordu; UE testi önce
FAIL checks=0 verdi. Ortak yükleme helper'ı geçerli öğrenilmiş becerilerden
gereken minimum ağaç rütbesini çıkarır, kayıtlı rütbelerle birleştirip aynı seviye
bütçesine sığdırır. Aynı ağaçta maksimum rütbe maliyeti kullanılır; ücretsiz
Basic başlangıç becerileri hariçtir. Bütçesi karşılanmayan beceri/hotbar kaldırılır;
geçerli eski ücretli beceriler bütçe içinde rütbelere taşınır. Skill ve stat
yükleme yolları aynı kaynak kaydı hesaplar; yükleme sırası bağımsızdır. 12 yeni
izole UE kontrolü geçti; gerçek dosya göçü ve uzak istemci PIE açık. Bu, sunucuya
yüklenen bozuk/değiştirilmiş kayıt senaryosudur; uzaktan kayıt değiştirme kanıtı
değildir. Hesap kimliği, imza ve atomik ticaret bu düzeltmeyle kapanmadı.

Tarih: 2026-10-04. Yöntem: yalnızca statik inceleme (kod okuma, grep, git geçmişi taraması, motor kaynağı doğrulaması). Hiçbir dosya düzenlenmedi, derleme/commit/push yapılmadı, oyun çalıştırılmadı. Bu dosya tek yazma çıktısıdır.

Kapsam: `Source/MyProject/**` (69 dosya, yaklaşık 15 bin satır .cpp), `Config/*.ini`, `Scripts/*`, `.claude/`, `.gitignore`, git geçmişi (236 commit), `Saved/SaveGames`, `Saved/Logs`.

Not: Kod, denetim sırasında başka bir ajan tarafından eş zamanlı değiştiriliyordu (satır numaraları denetim anına göre yaklaşık değerdir, tipik kayma +/- 10 satır). "doğrulanmalı" işareti, koddan çıkarılmış ama çalıştırılarak teyit edilmemiş maddeleri gösterir.

Gizlilik kuralı: Gerçek sır değerleri bu raporda yazılmaz (en fazla ilk 4 karakter).

---

## 1. Özet Tablo

| No | Ciddiyet | Başlık | Konum |
|----|----------|--------|-------|
| K-1 | KRİTİK (herkese açık oturum) / YÜKSEK (LAN) | Kimlik doğrulama yok: kayıt yuvası istemcinin seçtiği ada bağlı; karakter ezme, çalma ve kopyalama | `MMOCharacter.cpp` GetSaveSlotName/ServerSetName/EndPlay, motor `GameModeBase.cpp:815` |
| Y-2 | ORTA | SaveGame yüklemede eksik doğrulama (ana statlar, eşyalar, görevler, klan) ve dosya imzasız | `MMOStatsComponent.cpp:802`, `MMOInventoryComponent.cpp:752-790`, `MMOQuestComponent.cpp:389`, `MMOClanComponent.cpp:567` |
| Y-3 | ORTA | API anahtarları kalıcı kullanıcı ortam değişkeni; AI geliştirme ajanları bypassPermissions ile çalışıyor | kullanıcı ortamı, `.claude/settings.local.json`, global `settings.json` |
| Y-4 | ORTA | LLM maliyet/kota suistimali (genel bütçe sınırı yok, `MaxTokens=2048`) | `MMODialogueSubsystem.h:81`, `MMOPlayerController.cpp:1166-1223` |
| Y-5 | ORTA | PII sızıntısı: varsayılan oyuncu adı işletim sistemi kullanıcı adını içeriyor, herkese görünür | `Saved/SaveGames/MMOSave_CIDIK-*.sav` (59 dosya) |
| Y-6 | ORTA | Debug Exec komutları ve debug RPC gövdeleri Shipping'de derleniyor (yalnızca host'a kapalı) | `MMOCharacter.h/.cpp` (MMOGiveItem, MMOSpawnMob, MMOBuff, MMOPvP...) |
| Y-7 | DÜŞÜK-ORTA | Sunucu DoS yüzeyleri: sohbet uzunluğu/throttle, `/klan kur` senkron disk taraması, `MMOSetName` cooldown'u yok | `MMOPlayerController.cpp:128-360`, `MMOClanComponent.cpp:46-62,250`, `MMOCharacter.cpp:665` |
| Y-8 | DÜŞÜK-ORTA | Envanter/altın/ana stat verisi tüm istemcilere replike ediliyor (bilgi ifşası) | `MMOInventoryComponent.cpp:31-35`, `MMOStatsComponent.cpp:77-92` |
| D-9 | DÜŞÜK | Kullanıcı dizgelerinde kontrol/bidi karakter ve homoglif denetimi yok | `?Name=` yolu, klan adı, arkadaş adı |
| D-10 | DÜŞÜK | HTTP hata gövdesi loglanıyor (anahtar yankısı olasılığı, doğrulanmalı) | `MMODialogueSubsystem.cpp:170` |
| D-11 | DÜŞÜK | `DefaultEngine.ini` içinde commit'li AndroidFileServer `SecurityToken` | `Config/DefaultEngine.ini:89` |
| D-12 | DÜŞÜK | `.gitignore` boşlukları (`.idea/`, `*.sav`, `.env`, `.claude/settings.local.json` yalnızca kullanıcı-global ignore'a bağlı) | `.gitignore` |
| B-13 | BİLGİ | Ağ mimarisi: parolasız listen server, dedicated server hedefi yok, `bAllowPvP=true` varsayılanı | `NetSmokeTest.ps1`, `MMOGameMode.h:40` |

Bulunmayanlar (olumlu sonuçlar): repoda ve git geçmişinde gerçek API anahtarı/token yok; FString::Printf'e kullanıcı verisinin biçim dizgisi olarak geçtiği yer yok; yol enjeksiyonu (`../`) mümkün değil; NPC LLM'ine oyuncu serbest metni gitmiyor (prompt injection sömürülemiyor).

---

## 2. Bulgular (Ayrıntılı)

### K-1. Kimlik doğrulama yok: kayıt yuvası = istemcinin seçtiği ad

Dosya:satır (yaklaşık):
- `Source/MyProject/Private/Characters/MMOCharacter.cpp:407` `GetSaveSlotName()` (yuva adı `MMOSave_` + PlayerState adı)
- `MMOCharacter.cpp:665` `ServerSetName_Implementation` (herhangi bir istemci, cooldown'suz, adı değiştirebilir)
- `MMOCharacter.cpp:374` `EndPlay` (çıkışta otomatik `SaveCharacter()`), `MMOCharacter.cpp:627` `ServerSaveCharacter_Implementation` (10 sn cooldown)
- Motor doğrulaması: `C:\Program Files\Epic Games\UE_5.7\Engine\Source\Runtime\Engine\Private\GameModeBase.cpp:815` `InitNewPlayer`: bağlantı URL'indeki `?Name=` seçeneği `.Left(20)` ile doğrudan oyuncu adı olur; `ChangeName` (satır 1416) benzersizlik veya karakter denetimi yapmaz.
- Proje `MMOGameMode`'da `PreLogin`/`Login`/`ChangeName` override'ı yok (grep sonucu boş).
- Kendi yorumu da riski kabul ediyor: `MMOCharacter.h` içindeki `MMOSetName` açıklaması ("Kalici bir kimlik sistemi olmadan tam duzeltilemez").

Ciddiyet: KRİTİK, oyun host'u tanımadığı kişilere açıksa; YÜKSEK, yalnızca tanıdık kişilerle LAN/Hamachi testinde.

Sömürü senaryoları:
1. Ezme (hasar verme): Mallory bağlanır, `MMOSetName Victim` yazar (Victim çevrimdışıysa isim çakışma denetimini geçer), sonra `MMOSave` veya sadece oyundan çıkar. `EndPlay` -> `SaveCharacter()` yuvayı Victim adına yazar; Victim'in gerçek karakteri Mallory'nin karakteriyle kalıcı olarak silinir. Yedek/sürüm yok.
2. Çalma: Mallory `open <host>:7777?Name=Victim` ile bağlanır (motor kodu adı doğrulamadan kabul eder). `AutoLoadDeferred` -> `LoadCharacter` Victim'in kaydını yükler: seviye, ekipman, altın, klan liderliği Mallory'ye geçer; ticaretle başka karaktere aktarılabilir. (Bağlantı URL'indeki `Name` yolu koddan ve motor kaynağından doğrulandı; uçtan uca oyun testi yapılmadı, doğrulanmalı.)
3. Kopyalama (duplikasyon): Aynı slot adıyla iki oyuncu aynı anda çevrimiçi olabilir: `ServerSetName` çakışmayı denetler, `?Name=` yolu denetlemez. İki bağlantı da aynı kaydı yükler; biri eşyaları ticaretle verirken diğeri hâlâ elinde tutar. `RequestZoneTravel` yalnızca bölge geçişinde yinelenen yuvayı reddeder (`MMOGameMode.cpp` yaklaşık satır 125), o zamana kadar pencere açık kalır.

Düzeltme (öncelik sırasıyla):
- Slot adını görünen addan ayır. Sunucuda saklanan kalıcı kimlik kullan: ilk girişte sunucu 128 bit rastgele "karakter anahtarı" üretip istemciye verir (istemci yerel dosyada saklar), yuva = `MMOSave_` + SHA-256(anahtar) ilk 16 hex. Ya da gerçek hesap/OSS `UniqueNetId`.
- `AMMOGameMode::PreLogin` veya `Login` override et: aynı kimlikle ikinci bağlantıyı reddet; `?Name=` değerini yok say/temizle; görünen adı ayrıca `ChangeName` override'ında doğrula.
- `ServerSetName` yalnızca görünen adı değiştirsin, yuvayı değiştiremesin; çağrıya 30 sn cooldown ekle.
- Yazmadan önce `.bak` döndür (son 3 sürüm), ezme olayını geri alınabilir yap.

#### K-1 yeni yuz (2026-10-11): Ad degistirerek karakter catallama (fork)

**DURUM (guncel, 2026-10-14):** tur 5 (`f70e5ad`, testler `76ceed9`) ve Sprint 1 denetimi tur 6 (`1f4a557`) sonrasi native testler
basarim **102** / gunluk **87** / posta **144** / ad kilidi **82** kontrol GECTI ve `RunAllChecks.ps1 -SkipBuild` 70/70 adim GECTI
(2026-10-14, `1f4a557` uzerinde; loglar `%TEMP%`, bu belge denetcisi sayilari loglardan okudu). Tur 5/6 maddeleri asagida
"Tur 5/6 bulgulari". F4 ACIK; PIE ve gercek uzak istemciyle yine HICBIRI denenmedi.

**DURUM (2026-10-11 aksami; tur 2 `afba349`, tur 3 `2bb2023`, tur 4 `d031b90`, testler `7932979`):** F1, F2, F3, F5
DUZELTILDI ve native testli: `Scripts/RunSystemTests.ps1` basarim **96** / gunluk **81** / posta **130** / ad kilidi **58**
kontrol GECTI, `RunAllChecks.ps1 -SkipBuild` GECTI (commit mesajlari; bu belge denetcisi testleri yeniden kosmadi). Tur 3/4'te
kapanan Y1-Y4, Y6-Y8 ve Y-NEW-1..6 asagida "Tur 3/4 bulgulari". F4 (Linux harf varyanti) ACIK, kalan dusuk maddeler asagida.
PIE ve gercek uzak istemciyle HICBIRI denenmedi. Kalici cozum (hesap modu) ACIK.
*Gecmis kayit (tur 2 aninda):* commit'lenmemisti; RunSystemTests 61/53/89 + SaveLifecycle 26 + NetSmokeTest GECTI; yeni
davranislar icin native test yoktu. F1/F2/F3/F5 "Durum" satirlarindaki "native test yok" ifadeleri o ana aittir; ad kilidi
testi `7932979` ile eklendi.
Asagidaki bulgu satirlari `43e87ca` icindir; duzeltme satirlari calisma agacindaki `MMOCharacter.cpp` icindir.
Kaynak: 2026-10-11'de iki bagimsiz denetim (qa-kontrolcu, ag-guvenlik-denetcisi), ikisi de commit `43e87ca`'yi inceledi.
Satirlar `43e87ca` icindir; `MMOCharacter.cpp` o commit'ten bulgu anina kadar (HEAD `b0f22bc`) degismemisti, sonra tur 2-4
duzeltmeleriyle degisti: asagidaki bulgu satirlari artik kaymistir (guncel satirlar `ACCOUNT_IDENTITY_DESIGN.md` §2.2, §10.2).
Hepsi **kod okumasiyla** bulundu: PIE'da, NetSmokeTest'te ya da gercek uzak istemciyle denenmedi.
Ciddiyet K-1 ile ayni (herkese acik oturumda KRITIK, LAN'da YUKSEK), cunku tek bir uzak istemci yeter.

Bu yuz, yukaridaki senaryo 1'in (ezme) tersidir: `ActiveSaveSlot` korumasi baskasinin yuvasini ezmeyi engelliyor, ama
karakterin **yeni ve bos bir yuvaya kopyalanmasini** engellemiyor.

**F1. Ad degistirerek karakter catallama (ana bulgu)**
- `Source/MyProject/Private/Characters/MMOCharacter.cpp:1631-1640` (`ServerSetName_Implementation`): uzak istemci yalniz
  diskte kaydi OLAN bir ada gecemez (`NameStore->Exists(CandidateSlot)`). Kaydi olmayan her yeni ada gecebilir; kendi
  karakterinin kayitli olup olmadigina bakilmaz.
- `MMOCharacter.cpp:952-957` (`UnPossessed`) ve `:1093-1096` (`EndPlay`): cikista `SaveCharacterForTravel(true)` karakteri
  `GetSaveSlotName()`, yani YENI adin yuvasina tam envanter, altin, depo ve gorevlerle yazar. `:933` 60 sn otomatik kayit da
  ayni yuvaya yazar. Basarili yazimdan sonra `ActiveSaveSlot` yeni yuva olur (`:1211-1213`).
- `MMOCharacter.cpp:1243-1252` (`BuildCharacterSaveRequest`): K-1 korumasi yalniz hedef yuva ZATEN VARSA yazimi reddeder.
  Yeni ad icin hedef yok, yazim gecer.
- Eski yuva silinmez (`MMOCharacter.cpp` ve `MMOLocalFileCharacterStore.cpp` icinde silme cagrisi yok). Eski `.sav` son
  kayitli haliyle (en cok 60 sn onceki otomatik kayit) diskte kalir.
- Senaryo: kayitli `Ali` (ornek 100.000 altin) `MMOSetName Ali2` yazar ve cikar. Diskte artik `MMOSave_Ali` ve `MMOSave_Ali2`
  ayni karakterdir. Ayni istemci sirayla `?Name=Ali2` ve `?Name=Ali` ile girip iki oturumda da altini posta, ticaret ya da
  depo uzerinden ucuncu bir karaktere aktarir. Altin ve esya her turda ikiye katlanir; tur sinirsiz tekrarlanabilir
  (her yeni ad icin 30 sn ad sogumasi, `MMOCharacter.h:461`). Ikinci istemci gerekmez.
- Posta B2 korumasi (`MMOMailComponent.cpp:155-163`, `IsNameChanged`) yalniz ad degisip yeni yuva henuz yazilmamisken
  islemi durdurur. Yeni adla yeniden girildiginde ad ve yuva eslesir, koruma devreye girmez.
- **Durum: DUZELTILDI (2026-10-11, kod okumasi + derleme; native test/PIE yok).** `ServerSetName_Implementation`
  (`MMOCharacter.cpp:1614-1637`): uzak istemci (yonetici disi) icin `ActiveSaveSlot` doluysa ve `Store->Exists(ActiveSaveSlot)`
  (magaza yoksa da) ret: "Kayitli karakterin adi degistirilemez". Bos `ActiveSaveSlot` (yukleme bitmedi) catallama
  sayilmaz, izinli. Host/yerel ve yonetici muaf. Test kancasi: `SetRemoteNameRulesForTest(true)` (yalniz non-shipping;
  yerel pawn'a uzak + yonetici disi kurallarini uygular). Eski yuvanin silinmesi gibi bir tasima YOK: kural yalniz
  catallamayi baslatan ad degisimini engeller.

**F2. Basarim ve gunluk yan dosyalari cikista yeni yuvaya tasinmiyor**
- Tasima yalniz `TryLoad` icinde var: `MMOAchievementComponent.cpp:278-290`, `MMODailyComponent.cpp:245-258` (bagli yuva
  `GetActiveSaveSlot()` degisince dosya yeni yola yazilir). `TryLoad` periyodik taramada (5 sn) ve olay anlarinda calisir.
- Bilesen `EndPlay`'i (`MMOAchievementComponent.cpp:99-110`, `MMODailyComponent.cpp:80-91`) `TryLoad` cagirmaz, yalniz eski
  `StoragePath`'e `SaveNow()` yapar. Karakter ise cikista yeni yuvaya yazilir (F1). Sonuc: ad degistirip, sonraki otomatik
  kayittan once cikan oyuncunun basarim ve gunluk dosyasi eski adda kalir, yeni yuvada hic dosya olmaz.
- Yeni adla girince basarimlar bos yuklenir. `EvaluateAll` (`MMOAchievementComponent.cpp:403-433`) seviye ve tamamlanan gorev
  basarimlarini canli durumdan hemen yeniden verir (`ProgressOf`, `:119-135`). Lv80 ve 90 gorevli bir karakter icin
  (`MMOAchievementDatabase.cpp:42-48, 62-64`) [Hesap]: seviye 100+300+800+1.500+3.000+5.000+10.000 = 20.700, gorev
  200+1.000+4.000 = 5.200, toplam **25.900 altin (~26.000) her ad degistirme dongusunde**, ayrica unvanlar. Gunluk gorevler de
  yeni yuvada ayni gun sifirdan acilir (Lv80'de 15 x 80 x (1+1+2) = 4.800 altin/gun ek; oldurme ve gorev teslimi ister).
- Bu ekonomi kalemi `EconomyModel.py`'deki basarim/gunluk satirlarinda yok (model tek seferlik basarim varsayiyor).
- **Durum: DUZELTILDI (2026-10-11; native test/PIE yok).** Uzak istemci kayitli karakterin adini artik degistiremez (F1),
  yani uzak yol kapali. Savunma derinligi: (1) basarim ve gunluk bileseni `EndPlay`'i, yuklu ise `SaveNow`'dan once
  `TryLoad()` cagirir (`MMOAchievementComponent.cpp:111`, `MMODailyComponent.cpp:92`); motor sirasi (UE 5.7 kaynagindan):
  `APawn::Destroyed` -> `UnPossessed` (karakter yeni yuvaya yazilir, `ActiveSaveSlot` guncellenir) -> `RouteEndPlay` ->
  `AMMOCharacter::EndPlay` -> `AActor::EndPlay` -> bilesen `EndPlay`; yani tasima guncel yuvaya gider. (2) Basarim:
  Lv>1 karakterin dosyasi yoksa ya da bozuksa o an saglanan ReachLevel/QuestsDone/GoldHeld basarimlari ODULSUZ acik
  isaretlenip yazilir (`MarkCurrentProgressUnlockedSilently`, `:365`). Bedeli: basarim sisteminden once Lv>1 olmus kayitli
  karakterler bu basarimlarin altinini geriye donuk almaz (unvanlar acilir); oldurme/olum sayaclari sifirdan baslar.
  Gunluk icin benzer savunma yok (ayni gunun gorevleri yeni dosyada sifirdan acilir; uzak yol F1 ile kapali).

**F3. `MMOLoad` "kayit yok" dali yuvayi degistirip basarim/gunlugu sifirliyor**
- `MMOCharacter.cpp:1382-1397` (`LoadCharacter`): canli adin yuvasi yoksa yuklu karakter bellekte kalir ama `ActiveSaveSlot`
  yeni yuvaya atanir, `bClassChoicePending = true` olur ve `:1369` `++LoadSerial` basarim ve gunluk bilesenlerini
  `ResetLoadedState` ile sifirlayip yeni (bos) yuvadan yukletir (`MMOAchievementComponent.cpp:266-276`,
  `MMODailyComponent.cpp:233-244`). Sonraki kayit karakteri yeni yuvaya yazar, eski yuva kalir: F1 ile ayni catallama ve F2
  ile ayni odul tekrari. Bu pawn'da baslangic paketi daha once verilmediyse (yuklenen karakterde verilmez) `GrantStarterPack`
  (`:1330-1351`) bir kez daha calisir.
- Erisim: uzak `ServerLoadCharacter` yalniz yoneticiye acik (`:1552-1562`); host yerel `MMOLoad` (`:1494-1499`) ve host ad
  degistirmede kayitli-ad kontrolune tabi degil. Yani bugun **yalniz host/yonetici** yolu. Host diske zaten tam erisimli
  oldugu icin guvenlik siniri degil, ama kazara catallama ve test verisinin bozulmasi riski var. Ciddiyet: DUSUK.
- **Durum: DUZELTILDI (2026-10-11; native test/PIE yok).** `LoadCharacter` (`MMOCharacter.cpp:1366-1378`): karakter
  zaten yukluyken (`bInitialLoadComplete` ve dolu `ActiveSaveSlot`) canli adin kaydi yoksa "Bu adin kaydi yok: yuklu
  karakter degismedi" der ve `LoadSerial`, `ActiveSaveSlot`, `bInitialLoadComplete`'e dokunmadan doner; basarim/gunluk
  sifirlanmaz, baslangic paketi verilmez. (Onerilen bilesen-tarafi "sifirlama" yerine kaynakta kesildi: daha az kod, ayni
  sonuc.) Host kendi adini degistirip `MMOSave` yaparsa karakter yine yeni yuvaya yazilir: host muaf, bilincli.

**F4. Linux'ta buyuk/kucuk harf ad varyanti ayri `.sav` yaratiyor**
- Yuva adi harf buyuklugunu korur: `GetSaveSlotName` (`MMOCharacter.cpp:1113-1144`) ve `CandidateSlot` (`:1615`).
  `Exists` `UGameplayStatics::DoesSaveGameExist`'e gider (`MMOLocalFileCharacterStore.cpp:32`), yani dosya sisteminin
  kuralina uyar. Yalniz cevrimici oyuncu cakismasi harf duyarsiz (`:1622`).
- Windows'ta `MMOSave_ALI` ile `MMOSave_Ali` ayni dosyadir. Harf duyarli bir dosya sisteminde (Linux dedicated/listen sunucu)
  ise farkli dosyalardir: `:1635` ve `:1244-1245` kontrolleri gecer, `Ali` cevrimdisiyken uzak istemci `ALI` adini alip ayri
  bir yuva olusturur.
- Posta kutusu ise kucuk harfe cevrilmis adla anahtarli (`MMOMailStore.cpp:39`) ve toplayan taraf canli PlayerState adini
  kullaniyor (`MMOMailComponent.cpp:40-54`, `:305`). Bu yuzden Linux'ta `ALI` adini alan biri `Ali`'nin posta kutusunu (ekli
  altinla birlikte) toplayabilir [Muhtemel, kod okumasi].
- Proje bugun Windows listen-server'da calisiyor; bu madde **Linux'ta denenmedi**, sunucu Linux'a tasininca gecerli olur.
- **Durum: ACIK (kod degismedi, bilerek).** F1 duzeltmesi kayitli karakterin harf varyantina gecmesini de engeller, ama
  `?Name=ALI` ile dogrudan baglanma yolu Linux'ta hala ayri yuva acar. Cozum yollari `ACCOUNT_IDENTITY_DESIGN.md` §10.3.
- **Durum guncellemesi (tur 4, `d031b90`):** ASCII ad kurali Windows'taki Unicode harf varyantini kapatti (Y-NEW-1, asagida);
  cevrimici cakisma kontrolleri kanonik anahtarla (ASCII kucuk harf). Ancak yuva adi (`MMONameRules::SlotKey`) harf buyuklugunu
  korur ve `Exists` dosya sistemine uyar: Linux'ta cevrimdisi `Ali`'nin `ALI` varyanti hala ayri `.sav` acar ve posta kutusu
  (`MMOMailStore.cpp:108`, kucuk harf) paylasilir. F4 ACIK; Linux'ta denenmedi.

**F5. Kayitli-ad reddinde soguma yok (ad sorgulama)**
- `MMOCharacter.cpp:1635-1639`: "Bu ad kayitli bir karaktere ait" reddi `LastSetNameServerTime`'i guncellemeden doner;
  damga yalniz basarili degisimde vurulur (`:1641-1644`). Bu §9.4'teki "basarisiz dogrulama soguma baslatmaz" kuraliyla
  tutarli, ama burada bilgi sizdiriyor.
- Senaryo: uzak istemci aday ad listesini tek tek `MMOSetName` ile dener. Kayitli (cevrimdisi) adlar bedava ve sinirsiz
  hizda "kayitli" diye doner; kayitli olmayan bir tahmin adi gercekten degistirir ve 30 sn soguma baslatir. Cevrimdisi
  karakter adlari boylece listelenir ve K-1 senaryo 2'ye (`?Name=` ile calma) girdi olur. Ciddiyet: DUSUK-ORTA.
- **Durum: DUZELTILDI (2026-10-11; native test yok).** Kayitli-ad reddi artik soguma damgasini vurur
  (`MMOCharacter.cpp:1675`); kayitli karakter zaten F1 kilidine takilip aday ada hic bakmaz. Kalan: yeni (kaydedilmemis)
  karakter 30 sn'de bir aday deneyebilir; `?Name=` ile yeniden baglanmak sogumasiz ayni bilgiyi verir (yeniden baglanti
  maliyeti dahilinde).

**Tur 3/4 bulgulari (2026-10-11; qa-kontrolcu + ag-guvenlik-denetcisi, kod okumasi; duzeltmeler `2bb2023` ve `d031b90`,
testler `7932979`; PIE YOK).** Satirlar `d031b90` sonrasi HEAD icindir. Etiketler commit mesajlarindan; tur 3 "Y5" etiketinin
icerigi commit mesajlarinda ve kodda bulunamadi (bu belge dogrulayamadi).

| Etiket | Bulgu (kisa) | Durum ve yer |
|---|---|---|
| Y1 | `.tmp` kurtarma/karantina `Move`'u motorun 10x0,5 sn tekrar denemesiyle sunucuyu dondurabiliyordu | KAPANDI (tur 3). Tekrar denemesiz `Move` (`MMOMailStore.cpp:38-41`); basarisizlikta geri cekilme: posta 60 sn (`:19`), basarim/gunluk tur 4'te 60 -> 10 sn (`MMOAchievementComponent.cpp:40`, `MMODailyComponent.cpp:37`) |
| Y2 | Bozuk/okunamayan posta kutusu bos sayilip ustune yazilabiliyordu | KAPANDI (tur 3, `a0cb86d` ustune). `Load` bozuk kutuyu `.bad`'a ayirip false doner, `.bak` varsa geri koyar (`MMOMailStore.cpp:205-236`); `SendMail` alici kutusunu `Load` ile okur, false'ta hicbir sey dusulmez (`StoreError`, `MMOMailComponent.cpp:236-241`) |
| Y3 | `LoadCharacter` F3 erken donusu `bInitialLoadComplete`'e de bakiyordu | KAPANDI (tur 3). Kosul yalniz `!ActiveSaveSlot.IsEmpty()` (`MMOCharacter.cpp:1359`) |
| Y4 (QA ORTA-1) | Posta `WriteBox` yedeksiz yaziyordu: basarisiz `Move` eski kutuyu kaybettirebilirdi | KAPANDI (tur 3). Once `.bak` kopyasi (alinamazsa hic yazilmaz), `.tmp` -> `Move`, olmazsa dogrudan yazim, o da olmazsa `.bak` geri konur (`MMOMailStore.cpp:246-310`); kurtarma sirasi `.bak` > `.tmp` (`:65-93`) |
| Y6 | Bozuk basarim/gunluk dosyasinin eski `.tmp`'si sonra "yarim yazim" sanilip geri konabiliyordu | KAPANDI (tur 3). Eski `.tmp` -> `.tmp.bad` (`MMOAchievementComponent.cpp:399-405`, gunlukte ayni desen `MMODailyComponent.cpp:359-366`) |
| Y7 | Yuva tasimasindan sonra eski yan dosyalar kaliyordu (eski ad yeniden alininca catal yuklenirdi) | KAPANDI (tur 3). Yeni yol basariyla yazildiktan sonra eski `.json` + `.json.tmp` silinir (`MMOAchievementComponent.cpp:496-500`) |
| Y8 (QA DUSUK-2) | Ad degistirme sogumasi disk `Exists` kontrolunden sonra geliyordu; F1 reddi soguma vurmuyordu | KAPANDI (tur 3). Soguma once (`MMOCharacter.cpp:1624-1647`), F1 reddi de damga vurur (`:1654-1659`) |
| Y-NEW-1 | Unicode/harf varyanti: `FChar::IsAlnum` Turkce harfleri geciriyor, `IgnoreCase` yalniz ASCII'de harf duyarsiz, NTFS Unicode'da da harf duyarsiz: buyuk/kucuk O-umlautlu iki ad farkli sayilip AYNI `.sav`'a yaziliyordu (ayni kayda ikinci oturum); `A B` / `A_B` gibi ayni yuvayi veren adlar uc yerde ayri ayri (ham ad + `Replace` + `IgnoreCase`) karsilastiriliyordu | **KAPANDI (`d031b90`).** Yalniz ASCII `[A-Za-z0-9 _-]` (`Public/Core/MMONameRules.h`); PreLogin (`MMOGameMode.cpp:577-598`), ChangeName (`:459-481`) ve ServerSetName (`MMOCharacter.cpp:1664-1681`) cakismayi tek `CanonicalKey` (bosluk -> `_`, ASCII kucuk harf) ile yapar (test: `S T` / `s_t` / `S_T`, ad kilidi 58 kontrol). Migrasyon yok (karar: `DECISIONS_NEEDED.md` "guvenlik tur 4") |
| Y-NEW-2 | Posta `WriteBox` ve basarim/gunluk `SaveNow` `Move`'u kilitli dosyada ~5,5 sn donduruyordu | KAPANDI (`d031b90`). Tekrar denemesiz (`MMOMailStore.cpp:285-293`, `MMOAchievementComponent.cpp:489-493`); test `Error moving file` satiri 0 bekler |
| Y-NEW-3/4 | Eski yuva silme: harf duyarsiz ayni dosyayi (`MMOSave_Ali` -> `MMOSave_ali`) ya da cevrimici baska oyuncunun yuvasini silebiliyordu | KAPANDI (`d031b90`). `AchDeleteOldSlotFiles` (`MMOAchievementComponent.cpp:66-88`) ve `DailyDeleteOldSlotFiles` iki durumda silmez |
| Y-NEW-5 | Posta `BoxFull`/`StoreError` sogumaya yazilmiyordu: cevrimdisi ad varligi sinirsiz hizda sorgulanabilirdi | KAPANDI (`d031b90`, `MMOMailComponent.cpp:233-246`) |
| Y-NEW-6 | Kaydi olmayan yeni karakter ayni yuvayla yeniden possess edilince kayit kapisi kapali kaliyordu | KAPANDI (`d031b90`, `MMOCharacter.cpp:1364-1373`) |
| QA DUSUK-3 (tur 4) | Ilk postada basarisiz yazim ana dosyayi kosulsuz siliyordu | KAPANDI: ana dosya `.bad`'a ayrilir, silinemeyen `.tmp` `.tmp.bad` olur; o da olmazsa yazim basarili sayilir (iade yok: altin kopyalanmaz, en kotu kaybolur) (`MMOMailStore.cpp:271-283, 311-330`) |

**Kalan acik / dusuk maddeler (guncel; hepsi kod okumasi, PIE/olcum YOK):**
- **`.bad` ustune yazma (DUSUK):** bozuk dosya `Move(..., bReplace=true)` ile ayrilir (`MMOMailStore.cpp:214`, `:316`;
  `MMOAchievementComponent.cpp:398`; `MMODailyComponent.cpp:360`). Ayni kutu/dosya ikinci kez bozulursa onceki `.bad` kaybolur
  (inceleme kaniti gider; oyun verisi degil). Basarim/gunlukte bu `Move`'un donusu denetlenmez.
- **`WriteBox` false ama yeni icerik diskte (DUSUK):** kutu varken `Move` ve dogrudan yazim basarisiz sayilip `.bak` geri konamazsa
  (`MMOMailStore.cpp:300-309`) `WriteBox` false doner ve gonderen iade alir; dogrudan yazim aslinda tam yazildiysa sonraki `Load`
  yeni kutuyu okur ve posta da teslim edilir (altin kopyasi). Iki ust uste dosya sistemi hatasi gerektirir. Ilgili mutasyon
  "c2" (anlik `.bak` geri koyma) surec icinde ulasilamadigi icin testle yakalanmadi (`7932979` mesaji).
- **Cokme aninda posta kaybi (DUSUK, bilerek):** sira ucret dus -> gonderen kaydi -> kutuya yaz (`MMOMailComponent.cpp:271-318`).
  Gonderen kaydi ile kutu yazimi arasinda cokme altini kaybettirir, kopyalamaz (tasarim tercihi, K1).
- ~~**Baglanti adinda harf kirpma (DUSUK):** `?Name=Ömer` -> `mer` (`MMOGameMode.cpp:381-392`); `mer` kaydi varsa o yuklenir,
  oyuncuya yalniz 3 sn sonra bildirim gider (`:537`). Karar adayi: kirpmak yerine reddetmek (`DECISIONS_NEEDED.md`).~~
  **KAPANDI (tur 5, `f70e5ad`):** asagida "Tur 5/6 bulgulari" T5-1.
- **F4 Linux harf varyanti:** ACIK (yukarida F4 "Durum guncellemesi"). ASCII kurali bunu cozmez.
- **Birden cok sunucu sureci (DUSUK, bugun tek surec):** posta deposunda surecler arasi kilit yok (`MMOMailStore.cpp`'de
  kilit/lock yok; geri cekilme haritasi da surec ici `static`, `:21-25`). Ayni `Saved/` klasorunu kullanan iki sunucu ayni kutuyu
  oku-degistir-yaz ile ezebilir. Ad sahipligi tasarimindaki tek-surec varsayimiyla ayni (`ACCOUNT_IDENTITY_DESIGN.md` §9).
- **Turkce harfli eski kayitlar:** migrasyon yok; ASCII disi `MMOSave_*` / posta kutulari erisilemez kalir (karar adayi ICU
  tabanli kanonik anahtar, `DECISIONS_NEEDED.md`).

**Tur 5/6 bulgulari (2026-10-11 .. 2026-10-14; kod okumasi + native test, PIE YOK).** Tur 5 = `f70e5ad` (kod) + `76ceed9`
(testler: basarim 96 -> 102, gunluk 81 -> 87, posta 130 -> 144, ad kilidi 68 -> 82; 9 mutasyonun 9'u yakalandi). Tur 6 = bulut
PR #3'un (`9855256`, Sprint 1 altyapisi) yerel QA/guvenlik denetimi, duzeltmeler `1f4a557`. Satirlar `1f4a557` icindir.

| Etiket | Bulgu (kisa) | Durum ve yer |
|---|---|---|
| T5-1 | Baglanti adindaki izinsiz karakterler sessizce kirpiliyordu: `Ömer` -> `mer`, `Ali!` -> `Ali` baskasinin kaydini acardi | **KAPANDI (tur 5).** Kirpma yok: `PreLogin` temizlemenin (bas/son bosluk disinda) degistirdigi adi reddeder (`MMOGameMode.cpp:595-599`, `InitialNameWasAltered` `:397-400`); `ChangeName` ayni durumda `Gezgin######` verir (`:466-468`, `:502`). URL kodlu ad (`%C3%96mer`) de ret (test) |
| T5-2 | Ayrilmis adlar: motorun misafir adi `Player<Id>` (Id her surecte 256'dan baslar, sonraki misafir oncekinin kaydina duserdi) ve ortak yedek yuva `Default` istenerek alinabiliyordu | **KAPANDI (tur 5).** `MMONameRules::IsReservedName` (kanonik `default`, `player` + yalniz rakam; `MMONameRules.h:103`); `PreLogin` (`MMOGameMode.cpp:600`) ve `ServerSetName` (`MMOCharacter.cpp:1613`) reddeder, `ChangeName` `Gezgin` verir, posta `IsDeliverableName` (`MMOMailStore.cpp:107`) teslim etmez ve ayrilmis adli gonderen gonderemez |
| T5-3 | Pawn'i olmayan bagli oyuncu (olu/izleyici/henuz possess edilmemis) cevrimici sayilmiyordu: adi `ServerSetName` ile alinabilir, eski yuva dosyalari silinebilirdi | **KAPANDI (tur 5).** `GameState->PlayerArray` de taranir: `ServerSetName` (`MMOCharacter.cpp:1693`), basarim (`MMOAchievementComponent.cpp:92`) ve gunluk (`MMODailyComponent.cpp:88`) eski yuva silme korumasi |
| S1-1 | `build.ps1 -Clean` `Saved/`'i siliyordu (karakter `.sav`, basarim, gunluk, posta) | **KAPANDI (tur 6).** Yalniz `Binaries/` + `Intermediate/`; DDC ayri `-CleanDdc`; editor/UBT calisirken durur (`build.ps1:167`); `-ProjectPath` dogrulanir. Betik bu denetimde calistirilarak denenmedi |
| S1-2 | `UDatabaseMigrator` sahte basari: `ExecuteSQL`, `RecordMigration`, `IsDatabaseInitialized`, `ValidateSchema` hicbir sey yapmadan `true` | **KAPANDI (tur 6).** Hepsi `false` + `LastError = "not implemented: no database backend"` (`DatabaseMigrator.cpp:195-203` vb.); `*_rollback.sql` ileri yonde elenir. Sinifi kimse cagirmiyor; silmek/tutmak = karar S15 |
| S1-3 | `FPacketValidator::ValidateRPCFrequency` (TODO) ve `ValidatePacketIntegrity` her zaman `true` (calisan anti-cheat sanilabilirdi) | **KAPANDI (tur 6).** Ikisi de `false` + `LastError` (`PacketValidator.cpp:286-292`, `:111`'den). Sonuc: `ValidateRPC` (`:58`) cagrilirsa her RPC'yi reddeder; kimse cagirmiyor |
| S1-4 | `V1__initial_schema.sql`: `update_warehouse_updated_at` tetikleyicisi olmayan `character_warehouse.updated_at`'e yaziyordu; altin/adet/fiyat negatif olabiliyordu | **KAPANDI (tur 6, metin olarak).** `updated_at` sutunu + `CHECK (>= 0)` (`gold`, `count`, `gold_attached`, `price`, `quantity`). **psql ile uygulanmadi** |
| S1-5 | `MyProjectServer.Target.cs` Shipping'de `UE_LOG`'u derleme disi birakirdi (takas denetimi `LogMMOTradeAudit`, `[MMOMETRIC]`, anti-cheat loglari kaybolurdu) | **KAPANDI (tur 6, metin olarak).** `bUseLoggingInShipping = true` + `BuildEnvironment = Unique`; konsol Shipping'de kapali kalir. Server hedefi **derlenmedi** (kaynak motor gerekir) |
| S1-6 | Posta iadesi odenemezse (sahip/envanter gitti, yeniden yukleme, ad/yuva degisimi, kilit) altin sessizce kayboluyordu | **GOZLEMLENEBILIR (tur 6).** Mantik ayni (altin kopyalanmaz, kaybolur); `[MMOMETRIC] ev=gold_lost src=posta-iade` + `LogMMOMail` uyarisi (`MMOMailComponent.cpp:294-303`) |
| S1-7 | Takas altini hep-ya-hic degil: `9855256` (icindeki Codex O69 posta iade isi) `ApplyGoldInternal`'a "sahip/bilesen yok ediliyorsa yok say, 0 don" kosulu ekledi (`MMOInventoryComponent.cpp:410`); takasin dort altin cagrisi donus degerine bakmaz (`MMOTradeComponent.cpp:880-883`) | **ACIK.** Bir cagri 0 donerse altin yaratilir ya da yok olur. Onay kapisi (`:548-549`) aktorleri hemen once kontrol eder, aradaki yolda senkron callback yok: pratik pencere dar [Muhtemel], tetiklenebildigi gosterilmedi. `MMOTradeComponent.*` Codex'in dosyasi: karar/devir `DECISIONS_NEEDED.md` "Takas altini hep-ya-hic degil"; PIE maddesi `PLAYTEST_CHECKLIST.md` (A-1) |
| S1-8 | PL1-PL3 plan riskleri (`SIX_MAP_PLAN.md`) | **ACIK.** Tur 6 denetim raporunda geciyor; icerikleri hicbir belgede ya da commit mesajinda yazili degil, bu belge denetcisi dogrulayamadi. Raporun sahibi bu satiri doldurmali |
| S1-9 | `Scripts/k6_load_test.js` var olmayan HTTP uclarina (`/api/auth/login`, `/api/character/move` ...) istek atiyor | **ACIK (bilerek).** Calistirilamaz; A3'te giris servisi yuk testi olarak yeniden yazilacak (`SIX_MAP_PLAN.md` §1.5) |

**Onerilen kisa vadeli cozum (2026-10-11'de uygulandi; F4 haric, yukaridaki "Durum" satirlari):**
- Uzak istemci (`!IsLocallyControlled()`) icin `ServerSetName` yalniz **hic kaydedilmemis yeni karaktere** izin versin:
  kosul `Store->Exists(ActiveSaveSlot) == false` (yalniz `ActiveSaveSlot.IsEmpty()` yetmez, cunku yeni karakter dali
  `:1394` dosya yokken de `ActiveSaveSlot`'u atiyor). Ilk otomatik kayittan (60 sn) sonra ad kilitlenir. Host ve yonetici
  muaf (test akisi bozulmasin). Bu tek kosul F1'i, uzak istemci icin F2'yi ve kayitli karakterler icin F5'i kapatir.
- F5 icin ek: kayitli-ad reddi de soguma damgasini vursun (yeni karakterin ad denemesi de yavaslasin).
- F2 icin ek (savunma derinligi): basarim ve gunluk bileseninin `EndPlay`'i `SaveNow()`'dan once `TryLoad()` (ya da yalniz
  tasima adimini) cagirsin. Not: bilesen `EndPlay`'i karakterin kaydindan sonra mi calisiyor, siralama dogrulanmali.
- F3 icin: karakter zaten yukluyken (`bInitialLoadComplete` ve dolu `ActiveSaveSlot`) `MMOLoad` "kayit yok" dalina dusmesin;
  "Bu adin kaydi yok" deyip hicbir sey degistirmesin.
- F4 icin: harf duyarsiz `Exists` (`ListSlotNames` + `IgnoreCase`) ya da yuva adini kanonik kucuk harfe cevirme; ikincisi
  mevcut `.sav` dosyalari icin tek seferlik goc ister (`ACCOUNT_IDENTITY_DESIGN.md` "Kisa vadeli ara onlem").
- Karar: `DECISIONS_NEEDED.md` "Ad degistirme kisiti (uzak istemci)". Dokunulacak dosya sicak (`MMOCharacter.cpp`); Codex
  kurallarina gore ayri, kucuk bir commit.

**Kalici cozum:** `docs/ACCOUNT_IDENTITY_DESIGN.md` hesap modu (B-min): ad hesaba ait ve degismez (§4.3 adim 5, hesap
modunda uzak `ServerSetName` her zaman ret). Ad degismeyince F1, F2, F3 (uzak) ve F5 kendiliginden kapanir; F4 icin
`names/` anahtari zaten kucuk harf.

**Dogrulama plani (yapilmadi):** iki istemcili NetSmokeTest varyanti: (1) kayitli karakter uzaktan ad degistirmeyi denesin,
ret beklenir; (2) yeni karakter adini degistirebilsin; (3) ad degistir + hemen cik, sonra yeni adla gir: basarim altini
ikinci kez verilmemeli. `43e87ca` kodunda (1) ve (3) **basarisiz olmasi beklenirdi**; 2026-10-11 duzeltmesinden sonra
yesil beklenir ama hala kosulmadi. Native test icin kanca: `AMMOCharacter::SetRemoteNameRulesForTest` (test-muhendisi).
Guncel (2026-10-11): bu kancayla native `Scripts/TestNameLock.ps1` (58 kontrol, (1) ve (2)'nin tek surec benzetimi) GECTI;
iki istemcili NetSmokeTest varyanti ve (3) gercek ag uzerinde hala kosulmadi.
Guncel (2026-10-14): ad kilidi testi tur 5 ile 82 kontrol (`76ceed9`), `1f4a557` uzerinde GECTI; iki istemcili varyant hala kosulmadi.

---

### Y-2. SaveGame yükleme doğrulaması eksik, dosya imzasız

Sömürü ön koşulu: kayıt dosyasını değiştirebilmek (host makinesi, paylaşılan/bulut senkronize `Saved/`, ileride dedicated sunucuya taşınan kayıtlar, veya K-1 ile başka birinin yuvasına yönlendirme). Dosya biçimi düz UE SaveGame (binary ama imzasız, dizgeler okunabilir).

Doğrulama yapılmayanlar:
- `MMOStatsComponent.cpp:802-805`: `Strength/Dexterity/Intelligence/Vitality = In.*` hiçbir sınır/toplam kontrolü olmadan kopyalanıyor (seviye başına verilebilecek toplam puanla karşılaştırma yok). Kodun kendi yorumu bunu kabul ediyor (`RecalculateDerivedStats` içinde: "ApplySaveData hicbir dogrulama yapmadan kopyaliyor"). Düzenlenmiş kayıt ile `Vitality=2000000000` gibi değerler sınırsız can/saldırı verir.
- `MMOInventoryComponent.cpp:752-790`: `Gold` yalnızca `Max(0)` (üst sınır yok); `Items` içinde eşya kimliğinin veritabanında var olup olmadığı ve `Count` üst sınırı denetlenmiyor; `EquipmentBag` için `IsValidInstance()`/`ItemId` varlığı/`Rarity` enum aralığı denetlenmiyor (`Warehouse` için var); `EquippedItems[i]` öğesinin gerçekten `i` slotuna ait olduğu ve `RequiredLevel`'ın sağlandığı yüklemede doğrulanmıyor (normal `EquipFromBag` bunları denetler); yinelenen `InstanceId` engellenmiyor; dizi uzunluğu sınırsız (replikasyon şişmesi, bellek).
- `MMOQuestComponent.cpp:389-390`: `ActiveQuests`/`CompletedQuests` olduğu gibi alınıyor. Düzenlenmiş `KillProgress` ile görev anında "tamam" görünür ve `ClaimReward` ödülleri verir (XP, altın, ekipman); `CompletedQuests` ile ön koşullar atlanır; veritabanında olmayan kimlikler birikir.
- `MMOClanComponent.cpp:567-640`: klan adı yalnızca `IsValidClanName` (uzunluk + alfasayısal) ile doğrulanıyor; kayıtta yazan klana davet olmadan üye olunur. `bClanLeader=true` ve o klanın çevrimiçi gerçek lideri yoksa kayıt sahibi lider olur (lider çevrimdışıyken klan devralma).
- `Friends` yalnızca uzunluk/yineleme denetimi alıyor.

Pozitif: Level, Experience, UnspentStat/SkillPoints, mastery, öğrenilmiş yetenekler, cooldown'lar, Warehouse ve konum güvenli şekilde sıkıştırılıyor/denetleniyor; bozuk yükleme mevcut dosyayı ezmiyor (`bInitialLoadComplete`).

Düzeltme:
- Tek bir `SanitizeSaveData(FMMOCharacterSaveData&)` fonksiyonu: ana stat toplamı <= taban + (Level-1) * puan/seviye; negatif/aşırı değerleri sıfırla; `Gold <= GoldCap`; `Items` yalnızca bilinen kimlikler ve `Count <= 999` ve dizi <= N; `EquipmentBag/EquippedItems` için `IsValidInstance + Find(ItemId) + slot eşleşmesi + RequiredLevel + Plus/Rarity aralığı + benzersiz InstanceId`; görevler yalnızca bilinen kimlik, `KillProgress <= TargetCount`, ön koşul zinciri tutarlı; klan için sunucu tarafı klan kaydı (var olan klana yalnızca kayıtlı üyelik tablosundan katılım).
- Dosya içine sürüm numarası + HMAC (anahtar sunucuda, repoda değil) ekle; imza uyuşmazsa yüklemeyi reddet ve olayı logla.

---

### Y-3. API anahtarı kalıcı ortam değişkeninde; geliştirme ajanları geniş yetkiyle çalışıyor

Gözlem (değerler okunmadı/yazılmadı, yalnızca varlık kontrolü yapıldı):
- `NVIDIA_API_KEY`: kullanıcı düzeyinde kalıcı (HKCU\Environment) ve bu oturumun süreç ortamında tanımlı. `ANTHROPIC_API_KEY`: kullanıcı düzeyinde kalıcı tanımlı.
- Kodda anahtar doğru şekilde yalnızca ortamdan okunuyor (`MMODialogueSubsystem.cpp:16-25`), loga değeri yazılmıyor (satır 64 yalnızca değişken adını yazıyor), istemciye giden yol yok (anahtar yalnızca sunucuda HTTP başlığına konuyor, satır 135/140).
- Risk: ortam değişkeni, o kullanıcı altında çalışan her sürece görünür: Unreal Editor, derleme betikleri, Python betikleri ve AI ajanları. Genel `~/.claude/settings.json`: `defaultMode=bypassPermissions`, `skipDangerousModePermissionPrompt=true`. `.claude/settings.local.json`: sınırsız `PowerShell`, `Agent`, `Bash(gh auth *)`, `Bash(gh api *)`, `Bash(curl ...)`, `WebFetch(domain:github.com)`; ayrıca üçüncü taraf bir depodan `bash scripts/install.sh --tool claude-code` çalıştırılmış (çok sayıda üçüncü taraf ajan tanımı kurulmuş). Ajanlar GitHub'dan (Open-KO, karpathy vb.) güvenilmeyen içerik okuyor; bu içerikte gizli talimat olursa (dolaylı prompt injection) onay istemeden komut çalıştırıp ortam değişkenini okuyup dışarı gönderebilir. Bu, doğrulanmış bir sömürü değil, kontrol zayıflığıdır (doğrulanmalı: pratikte ağ çıkışı kısıtı var mı).

Sömürü senaryosu: güvenilmeyen bir README/ajan tanımı "ortamdaki anahtarı şu adrese gönder" der; ajan bypassPermissions ile `curl` veya `Invoke-WebRequest` çalıştırır.

Düzeltme:
- Anahtarı kalıcı kullanıcı ortamından kaldır; yalnızca oyunu/ sunucuyu başlatan betikte süreç kapsamında ver (örn. Windows Credential Manager'dan okuyup yalnızca o sürece ekleyen `Scripts/StartServer.ps1`). Mevcut anahtarları, ajan oturumlarında göründükleri için rotasyona almayı değerlendir (NVIDIA ve Anthropic panelinden yenile). Yeni anahtarlara harcama limiti koy.
- `defaultMode=bypassPermissions`'ı kapat veya en azından `permissions.deny` ile `Bash(printenv*)`, `Bash(env*)`, `PowerShell(Get-ChildItem Env:*)`, `PowerShell(Invoke-WebRequest*)`, `Bash(curl*)` (yazma/POST) reddet. Genel `PowerShell` iznini kaldırıp gerekli komutları tek tek listele.
- Üçüncü taraf ajan/skill paketlerini kurmadan önce içeriğini incele; güvenilmeyen depo içeriğini okuyan oturumlarda ağ erişimini kapat.

---

### Y-4. LLM maliyet / kota suistimali

Konum: `MMOPlayerController.cpp:1166-1223` (`ServerRequestNpcDialogue_Implementation`), `MMODialogueSubsystem.h:81` (`MaxTokens = 2048`, varsayılan Anthropic modeli `claude-opus-5`), `MMODialogueSubsystem.cpp:81/107`.

Mevcut koruma: PlayerController başına 2 sn sunucu cooldown'u ve 700 birim mesafe denetimi (iyi).

Eksikler:
- Genel (oturum/gün) bütçe veya eşzamanlı istek sınırı yok. Cooldown denetleyici başına tutulduğu ve kimlik doğrulama olmadığı için (K-1) çok sayıda bağlantı/yeniden bağlantı ile istek hızı çarpılır; her istek faturalanır (Anthropic seçilirse gerçek para; NVIDIA ücretsiz katmanda kota tükenmesiyle özelliğin kapanması).
- `MaxTokens=2048` "en fazla 2 cümle" isteyen bir replik için aşırı yüksek (NVIDIA yolunda düşünme yok; maliyet tavanı gereksiz yüksek).
- Model çıktısı sınırsız uzunlukta güvenilir RPC (`ClientReceiveNpcDialogue`) ile yollanıyor; kontrol karakteri/uzunluk süzgeci yok.

Prompt injection değerlendirmesi (sonuç: sömürülemiyor):
- Prompt'a giren dizgeler: `NpcName`, `NpcRole` (tasarımcı tanımlı, oyuncu yazamaz) ve `PlayerInfo` (`"seviye %d bir maceraci"`, sunucu tarafında üretilen sayı). Oyuncu adı, sohbet, görev metni LLM'e gitmiyor. Sistem istemi ile kullanıcı iletisi ayrı rollerde gidiyor. Araç/fonksiyon çağrısı yok. Çıktı yalnızca isteyen oyuncunun HUD balonuna gidiyor; sunucu mantığını (altın, eşya, görev) etkilemiyor.
- Uyarı: ileride oyuncu serbest metni (sohbet, ad, görev notu) bu prompt'a eklenirse gerçek bir prompt injection yüzeyi doğar; o zaman metin kullanıcı rolünde, uzunluk sınırlı ve süzgeçli geçmeli, çıktı hiçbir zaman oyun mantığına bağlanmamalı.

Düzeltme: genel token-bucket (ör. dakikada 20 istek, günlük tavan, eşzamanlı en fazla 3); `MaxTokens` 150-256; çıktıyı 240 karakterle kırp, kontrol/bidi karakterlerini sil; Anthropic seçiliyken daha küçük model; kota hatasında (429) geri çekilme.

---

### Y-5. PII sızıntısı: oyuncu adı işletim sistemi kullanıcı adını içeriyor

Kanıt: `Saved/SaveGames/` altında 59 adet `MMOSave_CIDIK-<14 hex>.sav` var. `MMOSetName` kullanılmadığında varsayılan ad (çevrimiçi alt sistem "Null" kaynaklı) Windows kullanıcı adı + rastgele sonek. Bu ad `PlayerState`'e yazılıyor ve herkese görünüyor (`/who`, sohbet gönderen etiketi, klan listesi).

Etkiler:
- Kullanıcı adı (gerçek ad olabilir) tüm oyunculara açılıyor.
- Her oturumda yeni rastgele ad = yeni kayıt yuvası: ilerleme sürekli kayboluyor ve diskte yetim .sav birikiyor (şu an 59). `SavedClanNameInUse` (Y-7) bunların hepsini her `/klan kur` çağrısında yüklüyor.

Düzeltme: istemci tarafında OS kullanıcı adını bağlantı seçeneğine koyma (`GameInstance::GetGameLoginOptions`'ı override edip `Name=Gezgin####` üret veya oyuncuya ad seçtir); K-1'deki kalıcı kimlikle birlikte adı yuvadan bağımsız yap.

---

### Y-6. Debug Exec komutları Shipping'de derleniyor

Gözlem:
- Yalnızca `MMOGameMode.h` (`MMOProbeSpawn/MMOProbeSurface`) `#if !UE_BUILD_SHIPPING` ile korunuyor. `grep UE_BUILD_SHIPPING|WITH_EDITOR` başka sonuç vermedi.
- Korumasız Exec komutları: `MMOGiveItem`, `MMOGainXP`, `MMODamage`, `MMOHeal`, `MMOUseMana`, `MMOBuff`, `MMOSpawnMob`, `MMOPvP`, `MMOClass`, `MMOLoad`, `MMONpcTalk` vb. (`MMOCharacter.h` satır 86-285).
- Pozitif: uzak istemci tarafı güvenli. `ServerGiveItem` (satır ~1613), `ServerSpawnMob` (~1393), `ServerApplyBuff` (~1163), `ServerSetPvP` (~1126), `ServerLoadCharacter` (~640) `IsLocalController()` ile yalnızca host'a açık; `UMMOStatsComponent` değiştiricileri `HasAuthorityInternal()` ile korunuyor, dolayısıyla uzak istemcide `MMOGainXP/MMODamage/MMOHeal` etkisiz. `ServerGiveItem` `Count`'u 1-999 sıkıştırıyor.
- Kalan risk: (a) host, listen-server modelinde otorite olduğu için tüm komutları kullanabilir. Paylaşılan bir ekonomide (host = oyuncu) bu hile yüzeyidir. (b) Shipping'de konsol motor tarafından normalde kapalıdır (`ALLOW_CONSOLE`), ama `DefaultInput.ini`'de `ConsoleKeys=Tilde` ve `bShowConsoleOnFourFingerTap=True` var; Shipping paketinde gerçekten kapalı olduğu doğrulanmalı. (c) Kötü niyetli değiştirilmiş istemci, RPC gövdelerindeki `IsLocalController()` kapılarına güvenmek zorunda; bunlar doğru ama tek savunma.

Düzeltme: debug Exec fonksiyonlarını ve ilgili `Server*_Implementation` gövdelerini `#if !UE_BUILD_SHIPPING` (veya `WITH_EDITOR`) ile sar; ya da GameMode'da `bool bAllowDebugCommands` (varsayılan false, yalnızca editor/PIE'de true) ile kapıla. Paketlenmiş Shipping build'de `~` tuşu ve `MMOGiveItem 1 1` ile test edip kapalı olduğunu doğrula. RPC'lere `_Validate` ekleyerek bariz geçersiz parametrede istemciyi düşürmeyi düşün (şu an hiçbir RPC'de `WithValidation` yok).

---

### Y-7. Sunucu DoS yüzeyleri

1. `MMOPlayerController.cpp:128-360` `ServerSendChat`: Mesaj uzunluğu yalnızca normal sohbet/fısıltı/klan dalında `Left(120)` ile kırpılıyor; komut dalları (`/klan`, `/w` hedef adı, `/arkadas`) ham ve sınırsız dizgeyi işliyor, hata iletilerinde yankılıyor (yalnızca gönderene). Kullanım hatası dönüşleri (`/w` argümansız, hedef bulunamadı) `LastChatServerTime`'ı güncellemediği için 0.7 sn throttle atlanıyor. Güvenilir RPC ile yüzlerce KB dizge gönderilebilir. Düzeltme: fonksiyonun başında `if (Message.Len() > 256) return;` ve throttle'ı fonksiyon girişinde güncelle.
2. `MMOClanComponent.cpp:46-62,250` `SavedClanNameInUse`: `/klan kur` çağrısında `Saved/SaveGames/MMOSave_*.sav` dosyalarının hepsi oyun thread'inde senkron `LoadGameFromSlot` ile okunuyor. Şu an 59 dosya var, sınırsız büyür (Y-5). Bir oyuncu geçersiz/kullanımdaki adlarla sürekli tetikleyerek oyun thread'ini bloke edebilir (0.7 sn throttle ile bile). Düzeltme: klan adlarını bellekte/ayrı küçük bir indeks dosyasında tut.
3. `MMOCharacter.cpp:665` `ServerSetName`: cooldown yok. Her başarılı çağrı `AnnouncePresence(false)`+`AnnouncePresence(true)` ile arkadaşlara bildirim yağdırır (iki adı döngüyle değiştirerek sohbet spamı). Düzeltme: 30 sn cooldown.
4. Hiçbir RPC için genel hız sınırlaması yok (yalnızca bazılarında özel throttle var); güvenilir RPC kuyruğu taşırılarak bağlantı düşürme/yük oluşturma denenebilir.

---

### Y-8. Replikasyon bilgi ifşası

`MMOInventoryComponent.cpp:31-34`: `Gold`, `Items`, `EquipmentBag`, `EquippedItems` koşulsuz (`DOREPLIFETIME`) replike edilir; yalnızca `Warehouse` `COND_OwnerOnly`. `MMOStatsComponent.cpp:81-82`: `UnspentStatPoints/UnspentSkillPoints` de herkese. Değiştirilmiş bir istemci, ilgililik alanındaki başka oyuncuların altınını ve çanta içeriğini okuyabilir (hedefli hırsızlık/dolandırıcılık ve ticaret sahtekarlığı için istihbarat). Aşağıdakiler doğrulanmalı: ticaret/inceleme arayüzü bu alanları başkalarının verisi için kullanıyor mu (kullanıyorsa ayrı, sunucu kontrollü bir kanal gerekir).

Düzeltme: `Gold`, `Items`, `EquipmentBag` için `COND_OwnerOnly`; `EquippedItems` görsel için gerekliyse yalnızca görsel için gereken alanları (ItemId) herkese, `Plus/Rarity` sahibe.

---

### D-9. Kullanıcı dizgelerinde karakter denetimi

- Sohbet: yalnızca `\r`/`\n` temizleniyor (`MMOPlayerController.cpp:141`). Sekme, sıfır genişlikli, çift yönlü geçersiz kılma (U+202E) karakterleri geçer; satırın görünümü taklit/yanıltma için kullanılabilir.
- `?Name=` yolundan gelen ad (K-1) `ServerSetName` süzgecinden geçmez (yalnızca `Left(20)`); `/who`, sohbet göndereni, klan listesi, fısıltı hedef eşleşmesinde kullanılır. Log enjeksiyonu: kullanıcı verisi `UE_LOG` biçim dizgesine girmiyor (yalnızca sanitize edilmiş slot adı loglanıyor, `MMOCharacter.cpp` ~537; bu yüzden log enjeksiyonu riski düşük).
- Klan adı `FChar::IsAlnum` ile doğrulanıyor: Unicode harfler geçer, Kiril/Latin homoglif ile var olan klanı taklit etmek mümkün.
- Biçim dizgisi enjeksiyonu: tüm `FString::Printf` çağrıları `TEXT("...")` sabit biçim kullanıyor; kullanıcı verisi yalnızca `%s` argümanı. SORUN YOK.
- Yol enjeksiyonu: `GetSaveSlotName` yalnızca `[A-Za-z0-9_-]`+boşluk(`_`) bırakıyor, 32 karakter; `MMOSave_` öneki Windows ayrılmış aygıt adlarını (CON, NUL...) engelliyor. SORUN YOK.

Düzeltme: ortak `SanitizeDisplayText()` (kontrol karakterleri + bidi biçim karakterleri sil, uzunluk sınırı) ve `AMMOGameMode::ChangeName` override'ında uygula.

---

### D-10. HTTP hata gövdesinin loglanması

`MMODialogueSubsystem.cpp:170`: `UE_LOG(... "HTTP %d: %s", Code, *RespBody)`. Bazı sağlayıcılar hata iletisinde anahtarın maskeli bir parçasını yankılar; ayrıca motorun `LogHttp` kategorisi VeryVerbose'ta istek başlıklarını (Authorization) yazabilir (doğrulanmalı). `Saved/Logs` yerelde kalıyor (ignore'da), fakat çökme raporları/oyuncu log paylaşımı ile dışarı çıkabilir. Düzeltme: gövdeyi 200 karakterle kırp, `Bearer`/`nvapi-`/`sk-` kalıplarını maskele; `LogHttp`'i VeryVerbose'a çıkarma.

Not: `Saved/Logs/*.log` ve `Saved/Config` içinde anahtar kalıbı bulunmadı.

---

### D-11. `Config/DefaultEngine.ini:89` AndroidFileServer SecurityToken

`[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` bölümünde `SecurityToken=323F...` (32 hex) commit'li ve `bAllowNetworkConnection=True`. Bu, editörün Android cihaza dosya göndermesi için kullanılan geliştirme token'ıdır (`bIncludeInShipping=False`, `bAllowExternalStartInShipping=False`, `ConnectionType=USBOnly` ile etkisi sınırlı). Düşük risk; depo herkese açık olursa token herkesçe bilinir. Android dağıtımı yapılmıyorsa bölümü kaldır veya `bAllowNetworkConnection=False` yap; token'ı yeniden üret.

---

### D-12. `.gitignore` boşlukları

Olumlu: `Saved/`, `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Build/`, `Art/`, `*.sln`, `UpgradeLog*.htm` ignore'da; git geçmişinde hiç `.sav`, log, `.env`, `Saved/` dosyası yok (doğrulandı).

Eksikler:
- `.idea/` (JetBrains/Rider; `workspace.xml` yerel yollar ve ayarlar içerir) ignore'da değil, şu an `??` (izlenmeyen); bir `git add -A` ile yanlışlıkla commit edilebilir.
- `*.sav`, `*.log`, `.env*`, `*.pem`, `*.key` için emniyet kuralı yok.
- `.claude/settings.local.json` ignore'u yalnızca bu makinenin kullanıcı-global `~/.config/git/ignore` dosyasında; başka bir klonda/CI'da korumasız (içinde makineye özgü yollar ve geniş izinler var; sır bulunmadı).
- `.agents/`, `.codex/`, `AGENTS.md` izlenmiyor; commit edilecekse içeriğinde sır olmadığı doğrulandı (grep temiz).

Düzeltme: `.gitignore`'a `.idea/`, `*.sav`, `*.log`, `.env*`, `*.pem`, `*.key`, `.claude/settings.local.json` ekle.

---

### B-13. Ağ ve yapılandırma bilgileri

- `Scripts/NetSmokeTest.ps1`: `?listen` ile 7777 portunda tüm arayüzlerde dinleme; parola/`PreLogin` denetimi yok. İnternete açık bir host için K-1 ile birleşince risk büyür.
- `Source/*.Target.cs`: yalnızca `Game` ve `Editor` hedefi var; dedicated `Server` hedefi yok. Tüm otorite ve kayıt host makinesinde; host güvenilir kabul ediliyor (listen-server modelinin doğası).
- `Source/MyProject/Public/Core/MMOGameMode.h:40`: `bAllowPvP = true` ("Test için varsayılan açık"); yayın öncesi `false` olmalı. PvP kuralları (parti/klan/hub) merkezi girişte uygulanıyor (iyi).
- Bağımlılıklar (`MyProject.Build.cs`): `Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule, NavigationSystem, NetCore, Slate, SlateCore, HTTP, Json, JsonUtilities`; üçüncü taraf kütüphane yok. `JsonUtilities`'in kullanımı doğrulanmalı (kullanılmıyorsa kaldırarak yüzey küçültülebilir). `PythonScriptPlugin` ve `ModelingToolsEditorMode` yalnızca `Editor` hedefinde. HTTP trafiği `https://` ile gidiyor (TLS sertifika doğrulaması motor varsayılanı; `Config` içinde devre dışı bırakan ayar yok).

---

## 3. Sır Taraması Sonuçları

- Repo (Source, Config, Scripts, docs, .claude/agents|skills, CLAUDE.md, AGENTS.md, .agents, .codex, .idea): `nvapi-`, `sk-ant-`, `sk-...`, `AKIA...`, `ghp_...`, özel anahtar blokları, `Bearer <değer>`, `Authorization: <değer>` kalıpları bulunmadı.
- Git geçmişi (`git log --all -p -G...`, 236 commit, `Content/` hariç patch taraması + `Content` ikili dosyaları için grep): gerçek sır yok. Geçmişte `.env`, `settings.local.json`, `.sav`, `Saved/` dosyası eklenmemiş.
- `.claude/settings.local.json` (192 satır): API anahtarı/parola/token yok; fakat Y-3'teki geniş izinler var.
- `Scripts/*` (Python/PowerShell): ağ çağrısı yok, sır yok; `Remove-Item` yalnızca kendi log dosyasına.
- `docs/PLAYTEST_CHECKLIST.md:2788`: "anahtar/parola/token benzeri bir şey yok" notu doğrulandı.
- Tek bulgu: `Config/DefaultEngine.ini:89` AndroidFileServer geliştirme token'ı (D-11, ilk 4 karakter `323F`).
- LLM anahtarının yolu: ortam değişkeni -> sunucu süreci -> HTTPS başlığı. İstemciye replike edilmiyor, loga değeri yazılmıyor, koda gömülü değil (kodun kendi yorumu da aynısını belirtiyor).

---

## 4. Güvenli Bulunan Alanlar (inceleme notu)

- Sunucu tarafı mesafe/yakınlık denetimleri: satıcı, depocu, görevli, şifacı, NPC diyalog RPC'leri.
- Tamsayı taşması korumaları: `AddGold` (int64 + clamp), `AddItem`, `BuyPotion/BuyConsumable` (1-999 sıkıştırma), ticarette altın toplam denetimi.
- Ticaret (`MMOTradeComponent`): teklif revizyonu, altın üst sınırı, yeni `InstanceId` atama.
- Parti atma (`ServerKick`): lider ve üyelik denetimi sunucuda.
- Saldırı/yetenek: sunucu otoriter cooldown, menzil, PvP/parti/klan/hub kuralları (`CanHarmPlayer`).
- Sınıf değişimi: enum doğrulaması + cooldown + savaş denetimi.
- Bozuk kayıt: mevcut dosya ezilmiyor (`bInitialLoadComplete`), konum doğrulanıyor.

---

## 5. Önerilen Düzeltme Sırası

1. K-1 (kalıcı kimlik, `PreLogin` ile tekrar eden slot reddi, `?Name=` yok sayma, `ServerSetName` slotu değiştirmesin, `.bak`)
2. Y-3 (anahtarları kalıcı ortamdan kaldır, bypassPermissions/PowerShell iznini daralt)
3. Y-2 (merkezi `SanitizeSaveData` + HMAC)
4. Y-6 (debug komutlarını Shipping'den çıkar; paketlenmiş build'de konsolu doğrula)
5. Y-4 (LLM genel bütçe + `MaxTokens` + çıktı süzgeci)
6. Y-5, Y-7, Y-8, D-9 (ad/PII, DoS, replikasyon koşulları, metin süzgeci)
7. D-10, D-11, D-12, B-13 (log maskeleme, token, gitignore, PvP varsayılanı)

## 6. Doğrulanmalı Listesi

- `?Name=` ile başkasının kaydının yüklenmesi (uçtan uca oyun testi; motor kaynağı ve proje kodu bunu destekliyor).
- Paketlenmiş Shipping build'de konsolun gerçekten kapalı olması.
- `LogHttp` ayrıntı seviyesinde Authorization başlığının loglanıp loglanmadığı ve NVIDIA/Anthropic hata gövdelerinin anahtar yankılayıp yankılamadığı.
- Ticaret/inceleme arayüzünün replike `Gold/Items/EquipmentBag` alanlarına bağımlılığı (Y-8 düzeltmesinden önce).
- Ajan oturumlarında ağ çıkışının kısıtlı olup olmadığı (Y-3).
- `JsonUtilities` modülünün kullanılıp kullanılmadığı.
- Kayıt dosyası düzenlemesinin fiilen kabul edildiği (Y-2 sömürüsü kod okumasından çıkarıldı; düzenlenmiş .sav ile test edilmedi).

---

## 7. Uygulama durumu (2026-10-04 güncellemesi)

PR [ozancidik/mmogame#1](https://github.com/ozancidik/mmogame/pull/1) üzerindeki değişiklikler **derlenmedi** (bulutta derleyici yok); yerelde `RunAllChecks.ps1` gerekir.

| No | Durum | Ne yapıldı / ne kaldı |
|----|-------|------------------------|
| K-1 | **Kısmen** | Çevrimiçi bir oyuncunun adıyla bağlanma reddedilir (`AMMOGameMode::PreLogin`); ad değişince başka kayıt yuvasının üzerine yazma engellenir (`ActiveSaveSlot`); isteğe bağlı sunucu parolası `-MMOPassword=`. **Açık:** çevrimdışı birinin adını `?Name=` ile kapıp kaydını yüklemek, `MMOLoad` ile sahiplenme. Kalıcı çözüm hesap sistemi (`ACCOUNT_BACKEND_COMPARISON.md`). |
| Y-2 | **Büyük kısmı** | Ana stat toplam tavanı, altın/yığın/nadirlik tavanı, yanlış slot, yinelenen `InstanceId`, görev sayacı ve yinelenen kimlikler. **Açık:** klan alanı (sunucu kaydı gerekir), görev ön koşul zinciri tutarlılığı, dosya imzası/HMAC. |
| Y-3 | Açık | Yerelde kullanıcı kararı (anahtar rotasyonu, ajan izinleri). |
| Y-4 | **Büyük kısmı** | LLM çıktısı 240 karakter + kontrol karakteri temizliği; sunucu geneli hız sınırı (dakikada 30, eşzamanlı 4; sınır aşılınca NPC yedek replik söyler). **Açık:** `MaxTokens` (düşünme tokenları aynı tavana sayıldığı için ölçümsüz düşürülmedi), günlük bütçe tavanı. |
| Y-5 | **Kod kapandı** | `<ad>-<hex>` varsayılan adı `Gezgin######` olur (`AGameModeBase::ChangeName`, derlenmedi; imza UE 5.7'de doğrulanmalı). Eski `MMOSave_<KULLANICI>-<hex>.sav` dosyaları yetim kalır. |
| Y-6 | **Kod kapandı** | Yedi debug Exec komutu ve üç Server RPC gövdesi `UE_BUILD_SHIPPING`'de erken döner. Shipping paketinde konsolun kapalı olduğu doğrulanmadı. |
| Y-7 | **Kapandı** | `ServerSetName` uzak istemciler için 30 sn soğuma; sohbet girişi ve `/klan kur` önbelleği önceki turda. |
| Y-8 | Önceki turda kapandı | Envanter/görev/beceri/stat özel alanları yalnız sahibe. |
| Y-9 | **Kod kapandı** | Uzak `ServerExec` ile admin kapısız Exec (bölüm 10): `AMMOPlayerController` uzak istemci konsol komutunu / `ServerPause`'u reddeder; sertlestirme turu: fail-closed, `_Validate` ile baglanti koparma, log izin listesi, `AddCheats`, uzunluk siniri. Native test `TestRemoteExecGuard.ps1` (82 vaka); PIE'de gerçek istemciyle denenmedi. |
| D-9 | **Kısmen** | Sohbette kontrol ve bidi karakterleri temizlenir. **Açık:** klan adı homoglif denetimi. |
| D-10 | Kapandı | HTTP hata gövdesi 200 karakterle loglanır. |
| D-11 | Açık | `DefaultEngine.ini` AndroidFileServer token'ı (`Config/` bulutta düzenlenmez). |
| D-12 | Önceki turda kapandı | `.gitignore` (`.idea/`, `*.sav`, `.env`). |

## 8. Ek not (2026-10-05, QA denetimi): komut satiri parolasi

- `-MMOPassword=` sunucu komut satirinda gorunur (islem listesi, kisayol/betik gecmisi) ve istemci `?Password=` degeri motorun `LogNet` "Login request: ..." satirina duser (sunucu log dosyasi). Kod DEGISTIRILMEDI.
- Yerel/ozel host icin kabul edilebilir; kamuya acik sunucuda parolayi ortam degiskeninden okuyup `Login request` logunu kapat ya da maskele (LogNet verbosity), log dosyalarini paylasma.
- Bu parola kimlik dogrulamanin yerini tutmaz (K-1 notu gecerli); kalici cozum hesap sistemi.

## 9. Uygulama notu (2026-10-05): kod gerektiren dusuk riskli maddeler

### 9.1 Debug/hile Exec komutlari (Y-6) - son liste

`MMODebugCommandsEnabled()` (`MMOCharacter.cpp`) Shipping'de `false` doner; asagidaki komut/RPC govdeleri erken cikar:

| Komut / RPC | Dosya | Shipping'de |
|---|---|---|
| `MMODamage`, `MMOHeal`, `MMOGainXP`, `MMOUseMana` | `MMOCharacter` | etkisiz |
| `MMOGiveItem` + `ServerGiveItem` | `MMOCharacter` | etkisiz |
| `MMOBuff` + `ServerApplyBuff` | `MMOCharacter` | etkisiz |
| `MMOSpawnMob` + `ServerSpawnMob` | `MMOCharacter` | etkisiz |
| `MMOClass` (konsol kisayolu; HUD sinif paneli `RequestSetupForClass` ile calismaya devam eder) | `MMOCharacter` | etkisiz (bu turda eklendi) |
| `MMONpcTalk` (ucretli LLM test istegi) | `MMOCharacter` | etkisiz (bu turda eklendi) |
| `MMOProbeSpawn`, `MMOProbeSurface` | `MMOGameMode` | `#if !UE_BUILD_SHIPPING` ile bos |

Oyun komutu sayilip ACIK birakilanlar: `MMOSave`, `MMOLoad`, `MMOSetName`, `MMOTravel` (host), `MMOUse`, `MMOAttack`, `MMOStat`, parti (`MMOInvite/PartyAccept/PartyLeave/PartyKick`), ticaret (`MMOTrade*`), `Say`; `MMOPvP` yalniz host (`ServerSetPvP` `IsLocalController` kapisi) oldugu icin acik (PvP'yi acmanin tek yolu). Not: UHT `UFUNCTION`'i `#if` icinde kabul etmedigi icin fonksiyonlar derlenir, govdeler Shipping'de bostur. Paketlenmis Shipping'de `~` konsolunun kapali oldugu hala dogrulanmadi.

### 9.2 Altin tavani (Y-2 tutarsizligi)

Tek sabit: `UMMOInventoryComponent::MaxGold = 999999999`. `AddGold` (int64 toplam + clamp `[0, MaxGold]`), ticaret tasma reddi (`MMOTradeComponent`) ve `ApplySaveData` AYNI sabiti kullanir (eskiden calisma zamani INT32_MAX, kayit 999999999: 1 milyar ustu yuklemede sessizce kirpiliyordu). Kayit formati (`MMOSaveGame.h`) degismedi.

Tasma kurali (mevcut davranis korundu, sessizlik giderildi): loot/gorev/satis gibi tek tarafli kazanc tavani asarsa fazlasi KIRPILIR (kaybolur) ve sahibe "Altin siniri (999999999): fazlasi kaybedildi" ipucu gider; ticaret tavani asacaksa takas ONCE reddedilir ("altin siniri asilir"), iki taraf da altinini korur. Negatif/harcama yolu ve int64 koruma bozulmadi.

### 9.3 LLM (Y-4)

**7 Ekim ek doğrulama:** Mevcut günlük/oyuncu kotasında 4096 takip anahtarı
dolunca yeni anahtar için oyuncu muhasebesi atlanıyordu. Genel tavanı aşmaz,
oyuncu kotasını atlatır. Rezervasyon öncesi kapasite reddi eklendi; mevcut
anahtarlar devam eder, açıkça sınırsız günlük mod korunur. Önce UE FAIL11,
sonra 25 bellek kontrolü PASS (§50). Sıfır sayaçlı iade anahtarları tabloyu
doldurabilir; varsayılan günlük1500 sayılan isteklerle tek başına dolduramaz.
Bu test API/HTTP/disk veya gerçek uzak istemci suistimal kanıtı değildir.
Ad temelli kimlik ve çoklu süreç toplam kotası ayrı açık işlerdir.


- `MaxTokens` 2048 -> 400 (NVIDIA yolu; dusunme yok). Anthropic yolu ayri `AnthropicMaxTokens = 1024` kullanir: Opus 5'te dusunme + metin ayni tavana sayildigi icin 400 cevabi kesip yedege dusurebilir (olculmedi; Anthropic'e gecilirse yedek replik sikligina bak).
- Mevcut sunucu geneli sinir (dakikada 30, eszamanli 4) korundu; eklenenler: oyuncu basina sunucu cooldown'u 2 sn -> 5 sn (dakikada en fazla 12 istek), ve oturum toplam butcesi `MaxSessionRequests = 400` (`SessionRequestCount`; dolunca istek atilmaz, NPC yedek replige duser; 0 = sinirsiz; baslatilamayan istek butceyi tuketmez).
- Oyuncu metni LLM'e gecmez (dogrulandi): `RequestNpcLine` yalniz `NpcName`/`NpcRole` (NPC aktor alanlari, tasarimci verisi) ve `PlayerInfo` alir; iki cagiran da sunucunun kurdugu sabit sablonu ("seviye N bir maceraci") verir; oyuncu adi/sohbet dahil degil. `RequestRaw` yalniz kodda tanimli; hicbir RPC'ye bagli degil. Yorum `RequestNpcLine` basina eklendi.

### 9.4 `MMOSetName` soguma (Y-7)

Tek damga (`LastSetNameServerTime`) ve tek kapi (`ServerSetName`): motorun `ChangeName` yolu da `MMOSetName` uzerinden ayni kapiya yonlendirildigi icin cift soguma yok. Uzak istemci 30 sn (degismedi), host/yerel konsol 5 sn (eskiden sinirsizdi). Reddedilince sahibe `ToOwner(Hint)` gider ("Ad degistirmek icin N sn beklemelisin"). Basarisiz dogrulama (gecersiz karakter, kullanimda vb.) soguma baslatmaz.

### 9.5 Sohbet kontrol karakterleri (D-9)

`ServerSendChat` basinda zaten ASCII < 32 ve bidi/sifir genislikli karakterler bosluga cevriliyordu (`\n` dahil); bu turda DEL (127) ve C1 kontrolleri (0x80-0x9F) eklendi. Klan adi homoglif denetimi hala acik.

### 9.6 Varsayilan oyuncu adi / OS adi (Y-5) - dogrulandi, degisiklik yok

`AMMOGameMode::ChangeName(bNameChange=false)`: `?Name=` `SanitizeInitialName` ile suzulur; bos, cevrimici/yuva cakismali ya da `<ad>-<12+ hex>` (Null OSS OS adi) kalibindaysa `Gezgin######` verilir. Bu turdaki NetSmokeTest'te dogrulandi: istemci `?Name=CIDIK-<32 hex>` ile baglandi, sunucu `Join succeeded: Gezgin393932` yazdi (PlayerState adi OS adi degil). OS/bilgisayar adi yalniz sunucu log dosyasinda (`LogNet: Login request` / `UniqueId: NULL:CIDIK-...`) kalir; kod hicbir yerde `UniqueId`'yi oyuncuya gostermez (grep temiz). Log notu bolum 8'de.

## 10. Y-9 Uzak ServerExec ile admin kapisiz Exec (2026-10-27)

**Bulgu.** Shipping disi derlemelerde (gelistirme sunucusu, kapali alfa, PIE) uzak istemci `APlayerController::ServerExec` -> `ServerExecRPC` ile sunucuda `ConsoleCommand` calistirabiliyordu (UE 5.7 `PlayerController.cpp` ServerExecRPC_Implementation; yalniz `!UE_BUILD_SHIPPING`). Komut `UPlayer::Exec` zincirinden (`Player.cpp`: `FExec::Exec`, `UWorld::Exec`, PlayerInput, PC `ProcessConsoleExec`, Pawn, HUD, GameMode, CheatManager, GameState, CameraManager) `UFUNCTION(Exec)` fonksiyonlarina ulasir. Uzak oyuncunun pawn'i sunucuda yetkili oldugu icin `MMOLoad` (`MMOCharacter.cpp` HasAuthority dali) ve `MMOPvP` `IsServerAdmin` kapisini atliyordu; `MMOGainXP`, `MMOGiveItem`, `MMOBuff`, `MMOHeal`/`MMODamage`, `MMOSave` (spam), `MMOClass`, `AMMOGameMode` `MMOTestZoneTravel*`/`MMOProbe*` ile motorun `EnableCheats` (CheatManager) ve `SwitchLevel` komutlari da admin kapisiz calisiyordu. Ayrica `ServerPause` RPC'si Shipping disinda her istemciye acikti.

**Duzeltme (tek nokta, `Private/Player/MMOPlayerControllerExecGuard.cpp`).** `AMMOPlayerController`: (1) `ServerExecRPC_Implementation` uzak istemcide hicbir sey calistirmaz (`UWorld::Exec` dahil; o zincirde PC'den once sorgulanir), (2) `ProcessConsoleExec(const TCHAR*, FOutputDevice&, UObject*)` (`UObject` sanal, `Object.h`) uzak PC'ye ulasan her komutu reddeder ve `true` donerek zinciri keser, (3) `ServerPause_Implementation` uzak istemcide durur. Uzak = `Player` var ve `ULocalPlayer` degil (sunucuda `UNetConnection`). Yerel kontrolcu (host, standalone) ve sunucu-yerel konsol (`-ExecCmds`) etkilenmez; host komutlari zaten `IsServerAdmin` ile kapili. Her engel `LogMMOExecGuard` Warning: `[MMOMETRIC] ev=remote_exec_blocked cmd=<ilk belirtec, en cok 32 karakter, kontrol karakteri temiz> player=<ad>`.

**Sertlestirme turu (bagimsiz dogrulama bulgulari, ayni bolum).** (1) *Fail-closed:* `MMOExecGuard::IsConsoleExecAllowed`: komuta yalniz `Player` bir `ULocalPlayer` ise ya da `Player` yok VE `GetNetMode()==NM_Standalone` ise izin verilir; dedicated/listen'de `Player`'i henuz olmayan PC ve uzak istemci reddedilir (ilk surum `Player==nullptr` PC'yi "uzak degil" sayiyordu = fail-open). (2) *Baglanti koparma:* `ServerExecRPC_Validate` ve `ServerPause_Validate` ezildi (UE 5.7 `PlayerController.generated.h`: `virtual bool ..._Validate`); izinsiz PC icin `false` doner, motor RPC'yi gonderen baglantiyi keser (`ServerPause_Validate` Shipping'de ayni mekanizmayi kullanir, `PlayerController.cpp:1968-1976`); engel PC basina bir kez `[MMOMETRIC] ev=remote_exec_blocked` loglar (log seli yok). `_Implementation` ayni karar ile ikinci savunma hattidir. (3) *Log enjeksiyonu:* `cmd=` ve `player=` alanlari izin listesi `[A-Za-z0-9_.-]`; digeri `?`, `=` atilir (U+00A0, U+2028, kontrol karakterleri, `ev=gold_lost` taklidi dahil); `Scripts/PlaytestMetrics.py` satiri `who=` alani olmadigi icin atlar, bozulmaz ve sahte olay uretilemez (`=` yok). (4) *CheatManager:* `AddCheats(bool)` ezildi (`PlayerController.h:1999`, `EnableCheats` -> `AddCheats(true)`); izinsiz PC icin `CheatManager` olusmaz. `ServerExecRPC` mesaji 256 karakteri asarsa reddedilir (uzak PC'de `_Validate` false = baglanti kesilir; yerel PC'de `ev=exec_too_long` logu). (5) *GameMode:* `AMMOGameMode::PostLogin` / `HandleSeamlessTravelPlayer` yeni oyuncunun PC'si `AMMOPlayerController` degilse `[MMOMETRIC] ev=exec_guard_missing pc_class=<sinif>` Error loglar (baglantiyi kesmez; test edilmedi, yalniz derlendi).

**Kalan risk (kapatilmadi).** (a) `ConsoleCommand` yolundan gelen `UWorld::Exec` komutlari (`DEMOREC`/`DEMOPLAY`..., `FLUSHPERSISTENTDEBUGLINES`) PC korumasindan ONCE islenir; gercek uzak yol RPC oldugu icin (1) ile kapali, ama bu sunucuda baska kod uzak PC'ye `ConsoleCommand` cagirirsa o komutlar calisir (testte `residual/world_exec_precedes_pc_via_consolecommand`). (b) `PlayerInput` Exec fonksiyonlari PC'den once sorgulanir; ayni sekilde yalniz (1) ile kapali. (c) Blueprint `ExecuteConsoleCommand` sunucuda 0. oyuncunun (host) denetleyicisiyle calisir; uzak istemciyi etkilemez. (d) `AMMOPlayerController` olmayan bir PC sinifi kullanilirsa koruma yoktur (artik `ev=exec_guard_missing` ile loglanir, engellenmez). (e) `_Validate` ile baglanti koparma yalniz gercek istemciyle denenmedi (testte dogrudan cagri; motorun baglantiyi gercekten kestigi PIE/dedicated'de dogrulanmadi). (f) Log kisitlamasi yalniz `_Validate` yolunda; `_Implementation`/`ProcessConsoleExec` engelleri her cagrida loglar (uzak istemci `_Validate` ile zaten kopar).

**Kalici kural.** Kapali alfa ve uretim sunuculari Shipping (ya da `UE_ALLOW_EXEC_COMMANDS=0`) derlenir; gelistirme sunucusunda bu koruma zorunludur. Yeni `UFUNCTION(Exec)` yazarken uzak istemcinin bu yola ulasabilecegini varsay: yetki kapisini fonksiyonun HasAuthority dalinda da uygula.