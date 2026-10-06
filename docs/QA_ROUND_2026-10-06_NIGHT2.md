# QA turu — 2026-10-06 gece 2

Kapsam: `56de087` (4 sınıfa Lv31/Lv39 beceri), `0a4f845` (Lv17/Lv19 Alev Vadisi yan görevleri), `a9c2d8d` (Lv13-15 Çözülme Yolu: 3 mob, 4 görev, spawner), `d9843ee` (Lore + 79 görev replik tablosu), `fd83c1e` + `eab6212` (`AMMOHUD::UpdateTutorialHints`) ve bunların etkileştiği kod (`MMOSkillComponent`, `MMOMobSpawner`, `MMOQuestComponent`, `MMOHUD::DrawSkillBook/DrawNpcDialogue`). Plan: `docs/CONTENT_PLAN_LV11_45.md`.

Denetim **salt okunur**: kod değiştirilmedi, commit/push atılmadı, `PLAYTEST_CHECKLIST.md`'ye dokunulmadı (test adımları bu dosyanın sonunda, bölüm 5).

**Oyunda (PIE) hiçbir şey denenmedi.** "Doğrulandı" = kaynakta ya da araç çıktısında görüldü; "doğrulanmalı" = PIE/ağ testi gerekir.

Çalışma ağacı notu: denetim sırasında başka bir ajanın commit'lenmemiş işleri vardı (`MMOGameMode.cpp` Kael satırı, `Source/.../Save/MMOCharacterStore*` yeni dosyalar). Tek-dosya derlemeler çalışma ağacını derler, yani `MMOGameMode.cpp` derlemesi Kael satırı değişikliğini de kapsar; "yalnız commit'lenmiş durumun derlemesi" ayrıca yapılmadı.

---

## 1. Araç / derleme kanıtı

| Kontrol | Sonuç |
|---|---|
| `Build.bat MyProjectEditor Win64 Development -WaitMutex -NoPCH -NoLink -SingleFile=<dosya>` (rsp'de `/W4` var, `/Yu` yok; bastırmalar `/wd4100 /wd4244 /wd4267 /wd4819 /wd4838 /wd5054`) | `MMOSkillDatabase.cpp`, `MMOMobArchetype.cpp`, `MMOQuestTypes.cpp`, `MMOGameMode.cpp`, `MMOHUD.cpp`: **5/5 `Result: Succeeded`, 0 uyarı, 0 hata** |
| `Scripts/ValidateData.py` | `Mob: 51 (elit 13)  Item: 53  Gorev: 79  Beceri: 81  Efekt: 12  Spawner: 29`, `Veri denetimi temiz.`; yalnız 20 beceri-boşluğu UYARI'sı (`Lv33->39 (6)`, `Lv39->45 (6)`, `45->58`, `58->70`, `70->80`; plan "20 -> 20" beklentisiyle birebir). Yeni "ölü içerik" / "gorev hedefi üretilmiyor" yok |
| `Scripts/TestValidateData.py` | 14 senaryo geçti |
| `Scripts/ProgressionModel.py --max 30` | 11-15 `9.7 / 9.2 [5/5 grind=0]`, 16-20 `16.8 / 5.9 [4/5]`, 21-25 `20.3 / 18.5`, toplam `~362 / ~324` — plan D tablosundaki (A3 sonrası) beklenenle **birebir** |
| `Scripts/BalanceReport.py` | Uyarı 0; Lv11 `CrustWarden L14` 185 XP→6 öldürme, Lv12 `ScorchedRaider L15` 220 XP→6 — plan A.4/5 ile aynı |
| Gizli bilgi taraması (`git diff 325de21..eab6212`, `api_key/secret/password/token/sk-`) | Eşleşme yok |
| `Scripts/StaticLint.py` | **HATA** (denetlenen commit'lerden değil): `Save/MMOCharacterStoreSubsystem.cpp` ve `Save/MMOLocalFileCharacterStore.cpp` için `include bulunamadi -> Save/MMOLocalFileCharacterStore.h`. Başka ajanın yarım (commit'lenmemiş) `Save/` işi; bitene dek `RunAllChecks.ps1` statik adımı kırmızı kalır |

---

## 2. Bulgular (ciddiyet sırasıyla)

Kritik (çöker / yanlış çalışır): **yok**. Aşağıdakilerin hiçbiri derleme/çökme sorunu değil; ORTA-1 ve ORTA-2 oynanışı doğrudan etkiler.

### ORTA-1 — Hub → Çözülme cebi düz rotası Obsidyen (Lv25-29) cebinin içinden geçiyor; plandaki erişim modeli yalnız cep-cep mesafesine bakıyor (doğrulanmalı — PIE)

Plan/yorumdaki model (`MMOGameMode.cpp:~694`, plan A.2) "spawn yarıçapı 225 + gezinme 450 + aggro 600 ≈ 1275 u + oyuncu duruş 835 u ≥ 2110 u" der ve cep-cep mesafesi ile karşılaştırır. **Bu kısım tutuyor** (hesap `route.py`, kaynak: koddaki sabitler):

| Çözülme (-5000,-3000) → | Mesafe | Gerekli (cebin erişimi + Çözülme savaş alanı 835) | Sonuç |
|---|---:|---:|---|
| Obsidyen normal cebi (-2800,-1800) r225 | 2506 | 2110 | OK (396 u pay) |
| Obsidyen Reisi (-4000,0) r150, aggro 1200 | 3162 | 2635 | OK |
| Dörtlü/Üçlü bölge, Magma Tirani, hub | 3901 / 4128 / 5810 / 5121 | 2285-2785 | OK |

**Ama model rotayı hesaba katmıyor.** Hub (`-816,-47`, PlayerStart) ile Çözülme merkezi arasındaki **düz çizgi**, Obsidyen cebinin merkezinden yalnız **288 u** geçiyor (Obsidyen mutlak koordinat, hub kullanıcıdan alındı: yaw'dan bağımsız). Obsidyen mobunun oyuncuyu fark etme zarfı 1275 u (kendi gerekçesi) olduğundan rotanın ≈ 2480 u'luk kısmı (rotanın yarısı) bu zarfın içinde; aggro 600 + doğma halkası 150-225 olduğundan mobların rotaya en yakın noktası ~100-300 u. Obsidyen Avcısı Lv25 (hasar 88, hız 440) karşısında Lv13-15 oyuncu (can tahmini 350-450, ölçülmedi) birkaç vuruşta ölebilir.

Ek (yaw=0 varsayımıyla, `PLAYTEST_CHECKLIST.md:2702`: PlayerStart (0,0)/+X varsayımı; bu haritada PlayerStart (-816,-47) olduğundan **yaw doğrulanmalı**):
- Hub → Çözülme düz hattı Dördüncü bölgeye (Lv16-22, r400) ~690 u ve Üçüncü bölgeye (Lv8-12) ~750 u yakın.
- Buz Lordu'nun (Çözülme görevlerinin önkoşulu) bulunduğu köşeden Çözülme'ye giden hat Obsidyen Reisi'ne (Lv30 elit, aggro 1200) ~676 u'dan geçiyor.

Görev metinleri yer tarif etmiyor ("yamaçlarda", "Alev Vadisi'ne yürüyen"), HUD'da görev işareti yok; minimap yalnız 4000 u içindeki kırmızı noktaları gösteriyor (cep hub'dan 5121 u). Oyuncu yeri ancak keşifle bulur ve en kısa yol ona Obsidyen'in içinden geçirir.

Tetikleyen durum: Q_FrostLich'i bitirmiş Lv13-15 oyuncu görev almak için cebe yürür.
Öneri yönü (kodu yazmıyorum): (a) cep probu ölçütlerine "rota açıklığı" ekle (hub ve Buz Lordu'ndan cebe en kısa nav yolunun her cepten ≥ erişim zarfı kadar uzak olması) ve cebi buna göre taşı; ya da (b) güvenli bir koridor/yol tarifi (NPC repliği "önce batıya, Obsidyen'i solda bırak") + görev takibinde yön; ya da (c) Obsidyen'in Lv25-29 mobları için aggro'yu rota yönünde kısıtlayan bir engel/nav ayrımı. **Test:** T1.

### ORTA-2 — Kayıttan yüklenen yüksek seviyeli karakter yeni becerileri açılmıyor (`ApplySaveData` mastery senkronu çağırmıyor) (doğrulandı kodda; PIE'de görülmedi)

- Commit mesajı/kod yorumu: "Rütbe 6 zaten yatırıldığı için seviyede otomatik açılır". Doğru yalnız **seviye atlarken** (`HandleLevelUpMasterySync` → `SyncLearnedFromMastery`, `MMOSkillComponent.cpp:82`) ve **rütbe yatırırken** (`:452`). `SyncLearnedFromMastery` çağrıları: tüm kodda yalnız bu iki yer (`grep`).
- `ApplySaveData` (`:~735-790`) `LearnedSkills`'i kayıttan alıp `RequiredLevel > SavedLevel` olanları siler, ama senkron çağırmaz. Yani bu değişiklikten önce kaydedilmiş, Lv≥31 (ya da ≥39) ve ilgili ağaçta rütbe 6 (Rahip'te 4) olan karakter `War_Earthshaker`/`Arc_ThornVolley`/`Mag_Inferno`/`Pri_RadiantNova` (Lv31) ve Lv39 becerilerini **bir sonraki seviye atlamaya kadar** almıyor.
- Lv80 (MaxLevel) karakterde seviye atlama olmadığından hiç açılmıyor; UI de yardım etmiyor: `CanLearn` rütbe zaten yeterliyse "Level N gerekli" (`:395-399`) döndürüyor — oysa oyuncu çok üstünde. Savaşçıda Special0 rütbe 7 (`War_WarDeluge`) yatırmak dolaylı olarak senkron tetikler; Büyücü/Okçu/Rahip'te yatırılacak üst rütbe yok.
- Tetikleyici girdi: yeni sürümden önce kaydedilmiş Lv≥31 karakter ile oyuna girmek.
- Öneri yönü: `ApplySaveData` sonunda (Stats seviyesi yüklendikten sonra) `SyncLearnedFromMastery()` çağır (gelecekte eklenecek her beceri için de geçerli); `CanLearn`'daki yanıltıcı "Level N gerekli" mesajı seviye zaten yeterliyken değişsin. **Test:** T4.

### DÜŞÜK-1 — İpucu "gösterildi" biti, balon gerçekten ekranda kaldı mı bakmadan işaretleniyor; gerçek NPC repliği ipucunu ezince ipucu kalıcı tüketilir; bayat H2 (`MMOHUD.cpp` `UpdateTutorialHints`, doğrulandı kodda)

Kullanıcının sorusu "ipucu gerçek NPC repliğini siliyor mu?": **Hayır, ters yön var.**
- İpucu gösterimi `IsNpcDialogueShowing(Now)` (9 sn pencere) ve `IsAnyWindowOpen()` ile korunuyor; gerçek replik ekrandayken ipucu **başlamıyor** (doğrulandı: `:2840`). Tek slot olsa da ipucu gerçek repliği ezemez.
- Ters yön: replik **sonradan** gelirse (LLM yanıtı `ClientReceiveNpcDialogue`, görev kabul/teslim satırı `UpdatePendingQuestLines` ≤2 sn gecikmeli, NPC'ye tıklama selamı) `ShowNpcDialogue` koşulsuz üzerine yazıyor. İpucu o anda 9 sn'nin ilk saniyelerindeyse kaybolur ama `ShownMask |= Bit` zaten atıldığı için bir daha gösterilmez (oturum boyunca). Etkisi: oyuncu özellikle H2/H3/H5'i hiç okumamış olabilir.
- Bayat ipucu: H2 (Lv1-2, görev yok, NPC ≤900 u) kuyruğa girdikten sonra görev kabul edilirse kuyruktan düşmüyor (yalnız 20 sn süre/seviye kontrolü var); pencere kapanınca ve önceki replik bitince "Yasli Doran'in ustundeki ! isaretine tikla" görev zaten alınmış oyuncuya çıkabilir (kuyruğa girişinden ≤20 sn sonra).
- Öneri yönü: gösterim anında koşulları yeniden doğrula (H2: `QuestCount==0`), bit'i balon `NpcDialogueDuration` dolana dek aynı metin ekranda kaldıysa işaretle. **Test:** T6.

### DÜŞÜK-2 — Ekonomi: plan "Lv10-15 gelir ~15.100 (4,1×)" demişti, model 18.806 (5,2×) veriyor

`EconomyModel.py` (Lv10-15 kademesi): taban `0a4f845~1` 11.021 / 3.648 = 3,0×; şimdi **18.806 / 3.648 = 5,2×**. Yeni görev altını +4.100 (plan doğru), ama yeni üç mobun altını (130-415, bandın üstündeki mobların ortalaması) +3.685 daha ekliyor; plan "ayrıca küçük artar" demişti. Lv16-24 kademesi 93.035 → 91.494 (+3.000 görev altınına rağmen düşüyor: model "gorevli" öldürme sayısı 9,6→5,9'a inince mob altını azalıyor). Altın sink kararı `DECISIONS_NEEDED.md` 3'te açık; yalnız sayı farkı bildirimi, denge zevk kararı değil. Öneri yönü: karar belgesine güncel 5,2× değerini yaz.

### DÜŞÜK-3 — Runtime probe 63/64, belgedeki "10 ardışık 64/64"ten farklı (zararsız; belge/gerçek uyuşmazlığı)

`MMOProbeSpawn` rastgele örnekler (`FRandRange`, `MaxAttempts=1`); 64 örnekte 1 ret görülmüş. `SpawnOne` ise `bValidateSpawnOnNavGround` için `MaxAttempts = 8` kullanıyor; 1/64 oranında hata ile 8 ardışık ret olasılığı ihmal edilebilir → oynanış etkisi yok. Ancak commit/plan "headless 10×64/64" diyor, runtime farklı: runtime navmesh ile headless grid arasında küçük fark olabilir. Öneri yönü: PIE'de `MMOProbeSpawn -5000 -3000 50 225` ~10 kez çalıştır, log satırındaki `trace_miss / egim_yukseklik / nav` hangisinin kaçırdığına bak (ret hep aynı bölgedeyse cep merkezini 100-200 u kaydır). **Test:** T2.

### DÜŞÜK-4 — `Arc_Impaler` (Lv39) ve `Mag_Starfall` (Lv39) sürekli-hasar ekseninde komşularını geçmiyor (kullanılmayabilir; baskın değil)

Baskınlık taraması (Ek A mantığı, 81 beceri): yeni beceri içeren **10 seviye penceresinde tam baskın çift yok** (plan iddiası doğrulandı). Tüm mesafelerde yeni beceriyi **geçen** çiftler yalnız ≥18 seviye farklı üst-kademe becerilerdir (`Arc_PoisonCloud`>`Arc_ThornVolley`, `War_TitanWrath`>`War_Skullsplitter` vb.; normal ilerleme). Yeni beceriyi **geçtiği** çiftler: `Arc_ThornVolley`>`Arc_Volley` (Lv13), `Mag_Inferno`>`Mag_Meteor` (Lv6), `Pri_RadiantNova`>`Pri_HolyBurst` (Lv4), `Pri_SalvationLight`>`Pri_HolyRain` (Lv16), `War_Earthshaker`>`War_Cleave/BloodStorm` — eski düşük kademeli becerileri geçmek mevcut kalıpla aynı.

Yine de pratik "yan geçit" olanlar (güç/sn = güç÷bekleme, güç/mana = güç÷mana):

| Yeni | güç/sn | güç/mana | Komşu | güç/sn | güç/mana | Fark |
|---|---:|---:|---|---:|---:|---|
| `Mag_Starfall` L39 (540, CD17, 170 mana) | 31,8 | 3,18 | `Mag_Disintegrate` L28 (420, CD12, 120) | **35,0** | **3,50** | Starfall yalnız anlık patlamada +%29 ve +100 u menzil; sürekli kullanımda Disintegrate hem hızlı hem verimli |
| `Arc_Impaler` L39 (470, CD16, 160) | 29,4 | 2,94 | `Arc_DeathMark` L28 (430, CD15, 125, Zehir) | 28,7 | **3,44** | +%2 hız, −%15 mana verimi, Zehir yok; anlık +%9 |
| `Pri_SalvationLight` L39 (1100, CD30, 260) | 36,7 | 4,23 | `Pri_Mending` L20 (520, CD14, 120) | **37,1** | **4,33** | tek vuruşta 2×; sürekli iyileşmede Mending marjinal üstün; mana havuzunun %79'u |
| `War_Skullsplitter` L39 | 27,8 | 2,70 | `War_WarCyclone` L45 (510, CD18, 195) | 28,3 | 2,62 | iki beceri neredeyse aynı çizgide |

Bu bir "çalışmıyor" değil, "ölü beceri riski"; plan zaten metrik M12'yi (`<%2 kullanım`) öngörüyor. Öneri yönü: Starfall CD 17→14 ya da güç 540→600 (güç/sn ≥ Disintegrate'e yakın), Impaler'a Zehir/Yavaşlatma ya da CD 16→14; zevk/denge kararı geliştiricide.

### DÜŞÜK-5 — Skill kitabında Special0 listesi seviye sırasına göre değil, tablo sırasına göre çiziliyor (kozmetik)

`DrawSkillBook` `FMMOSkillDatabase::GetForClass` sırasıyla çiziyor. Savaşçı Special0: `War_WarCyclone` (Lv45) ve `War_WarDeluge` (Lv58), `War_Rampage` (28), `War_Earthshaker` (31), `War_Skullsplitter` (39)'dan **önce** listeleniyor; Rahip Special0'da `Pri_AngelicBlade`/`AngelicStorm` (45/58) `Pri_Retribution`/`RadiantNova`'dan (28/31) önce. Yeni satırlar bu düzensizliği büyüttü (Rampage ile başlamıştı). Çökme/işlev yok.

### DÜŞÜK-6 — `KillAnyMob` görevi (`Q_AyazdisPatrol`) gri mob öldürmesini sayıyor (kabul edilmiş desen, `Q_Cleanup` emsali)

`NotifyMobKilled` (`MMOQuestComponent.cpp:248`) `KillAnyMob`'da her mobu sayıyor; hiçbir seviye/menzil kapısı yok. Lv11+ oyuncu 14 Lv1 Goblin keserek 3.000 ham (~255 etkin) XP + 800 altın + 3 Büyük Can İksiri alabilir. Etkin XP eşiğin %27'si; plan "kabul edilmiş desen" diyor. Yalnız bilgi.

### DÜŞÜK-7 — Lv13 mob Lv16 Adamantin ekipmanı düşürebilir (planda bilgi olarak var, ek doğrulama)

`MMOMob.cpp:1042` normal düşüş havuzu `RequiredLevel <= MobLevel + 3`; `ThawStalker` (Lv13) ve sonrası Adamantin (Lv16) setini düşürebilir. Bu setin elemanları `Q_MagmaGolems`/`Q_AshOrcs`/`Q_InfernalWraiths` zincir ödülü olarak tasarlanmış. Plan bilinçli kabul etmiş (bilgi). Düşüş oranı PIE'de ölçülmeli.

### DÜŞÜK-8 — HUD ipuçları: küçük yanlış-pozitif / sessiz-kalma durumları (kodda doğrulandı; hepsi H-düzeyi, çökme yok)

1. **H4 (ilk ganimet):** koşul "`AMMOLootPickup` 1500 u içinde" (her 0,5 sn tarama, Lv≤5); başka oyuncunun düşürdüğü ya da sahipsiz ganimet de tetikler, "karakterin ilk ganimeti" anlamı yok. Zararsız metin.
2. **H5 (seviye atlama):** `Lv == HintPrevLevel + 1` tam +1 arar. `MMOGainXP`/tek seferde çok seviyeli ödülle (replike Level bir karede +2) sessiz kalır. İlk katılımda/harita geçişinde yanlış pozitif **yok**: yeni pawn'da `HintPrevLevel=0`, 3 sn ısınma (`HintWarmupSeconds`) ve her karede `Prev` güncelleniyor — doğrulandı. Ölüm cezası seviye düşürmüyor (`ApplyDeathPenalty` yalnız EXP). Seviye ile puan replikasyonu aynı bunch'ta gelir varsayımı doğrulanmalı (gelmezse `UnspentPoints>0` kontrolü o karede başarısız, ipucu hiç çıkmaz).
3. **H3:** görev sayısı 0→>0 geçişi; yavaş replikasyonda (>3 sn) kayıtlı aktif görevi olan Lv≤10 oyuncuda da "ilk görev kabul edildi" sayılıp 20 sn sonra H3 çıkabilir.
4. **Oturum maskesi** (bellekte, `ULocalPlayer` başına): her oturumda Lv≤10 karakter ipuçlarını yeniden görür (plan "kabul edilen zayıflık"); `static TMap<TWeakObjectPtr,...>` boyutu >8 olunca geçersiz anahtarları temizliyor, aksi halde PIE yeniden başlatmalarında birikebilir (küçük, sınırsız değil).
5. **Perf:** kare başına maliyet erken-çıkışlarla küçük; `TActorIterator` yalnız Lv≤2 & görevsiz iken 1 Hz (NPC) ve Lv≤5 & H4 gösterilmemişken 2 Hz (ganimet); dize işlemi yalnız gösterimde. `FMMOQuestDatabase::Find` Lv≤10 & aktif görev varken her karede doğrusal (79 giriş × birkaç görev) — önemsiz. Aynı karede `DrawMinimap` zaten 6 `TActorIterator` çalıştırıyor, yeni yük onun yanında küçük.
6. **Metin:** 8 metin ASCII ve ≤50 karakter (`wc -c`: 47/42/46/50/46/47/47/50); balon sarıyor. Tuş çağrıları (`Z`, `R`, `1-0`, `F`, `G`, `I`, `K`, `C`, sağ tık kamera) kodla uyumlu (`MMOPlayerController.cpp:84-95`, `MMOCharacter.cpp:1018-1031`).

### DÜŞÜK-9 — `docs/CONTENT_MAP.md` eski

`Toplam: 48 mob (13 elit), 73 gorev, 53 ekipman, 73 beceri` (güncel: 51/79/81). Plan D adım 1/3/4'te `ContentMap.py` çalıştırılması öngörülmüştü, `--write` yapılmamış.

---

## 3. Sağlam bulunanlar (sessiz kalmıyorum)

**Beceriler (`56de087`)**
- Sınıf/ağaç/rütbe/seviye: 81 beceri, her ağaçta rütbeler ardışık (`ValidateData`), `rütbe <= seviye` hep sağlanıyor (rütbe 6 ≤ 31/39; Rahip 4 ≤ 31/39). Önkoşul zincirleri (aynı sınıf, `PrereqSkillId.RequiredLevel <= beceri.RequiredLevel`, `önkoşul.rütbe <= beceri.rütbe`) hepsi tutarlı; tek sapma eski dört önkoşul (`Cleave`, `PoisonArrow`, `HolyBurst`, `GreaterHeal`: `Basic -> Special0/1`), yeni becerilerin hiçbiri değil.
- **Ek puan istemiyor (doğrulandı):** `RequestInvestMastery` her rütbeye 1 puan harcıyor (`SkillPointCost` hiçbir yerde kullanılmıyor; yalnız `MMOSkillTypes.h:88`), yeni beceriler zaten gereken rütbede (Savaşçı/Okçu/Büyücü Special0 r6; Rahip Special0/Special1 r4); `SyncLearnedFromMastery` seviye kapısını (`S->Level < D.RequiredLevel`) ayrıca uyguluyor. Puan çok: 2 puan/seviye (`SkillPointsPerLevel=2`), tüm ağaç rütbelerini toplamı Savaşçı için ~16; yani Lv31'e gelen oyuncu ≥60 puan biriktirir — puan kıtlığı yok (yalnız bilgi).
- **Efekt/menzil/yarıçap/Etki:** `ExecuteSkill` switch'i `MeleeDamage`, `RangedDamage`, `AoESelf`, `AoETarget`, `HealTarget`'ı destekliyor; yeni becerilerin hepsi bunlardan; `Slow`/`Poison` efekt kimlikleri `MMOEffectDatabase`'de var. `AoESelf` yarıçapı etrafındaki tüm `AMMOMob`'lara `Power` uyguluyor (Earthshaker r460 Slow), `AoETarget` en yakın düşmanı merkez alıyor (ThornVolley r420 Zehir, Inferno r470, RadiantNova r440), `HealTarget` `Range=0` iken 800 u varsayılan (SalvationLight) — **yeni beceriler gerçekten çalışır, boşa gitmez** (kodda doğrulandı; PIE'de görülmedi).
- Güç/mana/bekleme eğrisi: her yeni beceri komşu halkaların arasında (güç 330-540 @Lv31-39; Lv28 340-430 ve Lv45 510); mana/sn 8,7-10,3 (Lv28 8,3-10,0, Lv45 10,8). Mana havuzu (INT'siz): Savaşçı %58/%61, Okçu %53/%57, Büyücü %43/%49, Rahip %47/**%79** (`Pri_SalvationLight`; plan doğru söylemişti: tek atımda havuzun dörtte üçü).
- **HUD sığması:** `DrawSkillBook` iki sütun; Savaşçı Special0 tek sütunda 12 satır → `RowH` (364−28)/12 = **28 px** (taban 20); en kalabalık sol sütun (Savaşçı 11 skill, 4 ağaç) 22,9 px; Okçu/Büyücü/Rahip 28 px. Tasma yok. Hotbar 10 yuva; Savaşçı'da 17 uygun beceri (seçim zorunlu, plan kabul etmiş).
- Derleme 0 uyarı.

**Görevler/moblar (`a9c2d8d`, `0a4f845`)**
- Zincir kilitlenmiyor: dört yeni yan görev yaprak (hiçbir görev onlara bağlı değil); `Q_AyazdisPatrol` önkoşulu `Q_VenomSpiders` (kapı 10, kendi kapı 11) doğru; `Q_ThawStalkers/CrustWardens/ScorchedRaiders` önkoşulu `Q_FrostLich` (13); `Q_EmberHounds` hâlâ `Q_FrostLich`'e bağlı; paralel kabul kısıtı yok (`MaxActiveQuests` yok).
- Seviye farkı: kapı − hedef mob seviyesi 0-1 (`ValidateData` uyarı yok); tüm ödül tüketilebilir kimlikleri (`GreaterHealthPotion`/`GreaterManaPotion`) `MMOConsumableDatabase.cpp:30,34`'te var; sayılar ve ödül adları replik metinleriyle uyumlu ("Buyuk Can/Mana Iksiri").
- Spawner bandı: `GetRandomInLevelRange(13,15)` elit olmayan arketiplerden **yalnız** `ThawStalker`(13), `CrustWarden`(14), `ScorchedRaider`(15) — Lv12 `OrcBerserker`, Lv16 `EmberHound`, Lv13 elit `FrostLich` dışarıda. Diğer spawner'lar (ana `MaxMobLevel=5`, 6-7, 8-12, 16-22, Obsidyen 25-29, ...) yeni mobları seçmiyor; bant/zorunlu arketipi olmayan spawner yok (`ValidateData` "bant da zorunlu arketip de yok" uyarısı çıkmıyor). Yalnız `Summer_scene` (`GetMapName().Contains("Summer_scene")`, PIE adı `UEDPIE_*_Summer_scene` dahil), Winter'da erken dönüş (cep oluşmaz, o haritada Lv13-15 görevleri zaten oynanmaz).
- Navmesh: Summer `NavMeshBoundsVolume` ±8500; cep (-5000,-3000) + `SpawnRadius` 225 + leash 1800 ≤ 7025 → sınır içinde.
- Hub mesafesi: 5121 u; mob leash 1800 + aggro 600 → hub'a ulaşım yok (`HubSafeRadius` 700 ayrıca).
- `AggroRange` 600 (bölge sınırı kuralı) üç mobda uygulanmış; Warrok görsel/animasyon `SetWarrokVisual` kullanılıyor (görsel doğrulama PIE).
- Replik tablosu: 79 görev ↔ 79 replik girdisi (`comm` ile eşleşme, fark yok); `MMOQuestDialogue.cpp` tamamen ASCII; en uzun dize 252 karakter (balon `MaxLines=5` ile kırpıyor).

**HUD ipuçları (`fd83c1e`)**
- Sunucu kodu/RPC/replikasyon/`MMOSaveGame` değişikliği yok; yalnız yerel HUD; `PlayerChar->IsLocallyControlled()` koşulu uzak istemcide de doğru çalışır (HUD zaten yalnız yerel). Seviye üst sınırları (H1:1, H2:2, H3/H5/H6:10, H4:5, H7/H8:20) hem kuyruğa alırken hem gösterirken uygulanıyor → seviye aşılınca ipuçları susuyor. Ölüyken yalnız H7 gösteriliyor. Pencere/diyalog açıkken bekleme çalışıyor (yalnız 20 sn aşımında düşüyor).
- `static TMap` içindeki başvuru (`ShownMask`) aynı karede yalnız aynı anahtarla yeniden aranıyor; yeni anahtar eklenmediği sürece başvuru geçersizleşmez (çoklu-PIE'de de aynı çağrı zincirinde yeni anahtar eklenmiyor).

---

## 4. Plan beklentisi karşılaştırması (bölüm 5 özeti)

| Beklenen (plan D) | Ölçülen | |
|---|---|---|
| 11-15: `9.7 / 9.2 5/5` | `9.7 / 9.2 5/5` | tutuyor |
| 16-20 (A3 sonrası): `16.8 / 5.9 [4/5]` | `16.8 / 5.9 [4/5]` | tutuyor; **ama** hedef 11-15 altında ("çok hızlı"), 4/5 seviyede grind=0 → plan A.4 seçenekleri (a)-(d) hâlâ karar bekliyor |
| 21-25: `20.3 / 18.5` | `20.3 / 18.5` | tutuyor |
| Toplam `~362 / ~324` | `~362 / ~324` | tutuyor |
| Ekonomi 10-15: ≈15.100 (4,1×) | 18.806 (5,2×) | **sapma** (DÜŞÜK-2) |
| `ValidateData`: 20 uyarı, `Lv33->39 (6)`, `Lv39->45 (6)` | aynı | tutuyor |

---

## 5. PIE test adımları (kullanıcı için, tıklanabilir)

Hepsi **denenmedi**. Hazırlık: Summer_scene, listen-server, 2 pencere (host + istemci) tercih. Konsol komutları geliştirme derlemesinde (`MMODebugCommandsEnabled`) çalışır.

**T1 — Çözülme rotası (ORTA-1)**
- [ ] Lv14 bir karakter aç; hub'dan (PlayerStart) Çözülme cebine (-5000,-3000) **düz** yürü (tıkla-yürü ile cebin dışına noktalar koy). Yolda Obsidyen Avcısı/Muhafızı/Akıncısı (Lv25-29) sana saldırdı mı? Saldırdıysa kaçıncı metrede ve cep merkezine kaç u yakınken oldu not et. Minimap'te kırmızı noktaların Çözülme'ye doğru yön gösterip göstermediğine bak.
- [ ] Aynısını `Q_FrostLich`'i bitirdikten sonra Buz Lordu'nun durduğu köşeden başlayarak yap; Obsidyen Reisi (Lv30 elit, aggro 1200) tetiklendi mi?
- [ ] Obsidyen'in kuzeyinden/güneyinden dolaşan rota var mı? Varsa görev metnine/NPC'ye yer tarifi eklenmesi gerekip gerekmediğine karar ver.

**T2 — Spawner probu (DÜŞÜK-3)**
- [ ] Konsol: `MMOProbeSpawn -5000 -3000 50 225` ~10 kez; log satırı `[MMOPROBE] ... ok=N/64 trace_miss=.. egim_yukseklik=.. nav=..`. Her seferinde 64/64 mü? Hangi sayaç doluyorsa (nav/trace/eğim) not et.
- [ ] 5 mob gerçekten doğuyor mu; hepsi yalnız `ThawStalker/CrustWarden/ScorchedRaider`; `[MMODBG] Spawner ... no nearby ground/nav location` uyarısı logda yok mu.

**T3 — Çözülme görevleri ve mobları**
- [ ] `Q_VenomSpiders` biter bitmez Lv11'de `Ayazdiş Devriyesi` görünüyor; ilerleme herhangi bir mobda artıyor; teslim: 3000 ham XP (etkin ~255) + 800 altın + 3 Büyük Can İksiri.
- [ ] `Q_FrostLich` sonrası Lv13/14/15 kapılarında üç Çözülme görevi sırayla görünüyor; üçünü aynı anda kabul et; her mob yalnız kendi görevini ilerletiyor; ödül tüketilebilirler envantere giriyor.
- [ ] Kavrulmuş Akıncı Warrok mesh'iyle yerde duruyor (havada/yer altında değil), vurunca ~%30 Yavaşlatma; Çözülme Avcısı ~%35 Zehir; Kabuk Bekçisi <%45 can Öfke.
- [ ] 3 dk PlayerStart/hub'da bekle: Çözülme mobu hub'a gelmiyor.

**T4 — Kayıttan yüklenen yüksek seviyeli karakter (ORTA-2)**
- [ ] Bu sürümden önce kaydedilmiş Lv31+ bir karakterle gir (ya da `MMOGainXP` ile Lv40 yap, kaydet, oyundan çık, yükle): `K` ile beceri kitabını aç; `War_Earthshaker`/`Arc_ThornVolley`/`Mag_Inferno`/`Pri_RadiantNova` (Lv31) ve Lv39 becerileri **[Ogrenildi]** mi? Değilse bir seviye atla ve tekrar bak (açılması gerekiyor). Lv80'de aynısı.
- [ ] Taze karakterle: Special0'a rütbe 6 yatır; Lv31'e çık → Lv31 beceri açılıyor; Lv39'a çık → Lv39 beceri açılıyor; ek skill puanı harcamadan.

**T5 — Yeni beceri efektleri**
- [ ] Savaşçı: `War_Earthshaker` kullan → 3+ mobda hasar + Yavaşlatma; `War_Skullsplitter` tek hedef 240 u menzil.
- [ ] Okçu: `Arc_ThornVolley` hedef çevresinde Zehir; `Arc_Impaler` tek hedef 2000 u.
- [ ] Büyücü: `Mag_Inferno` alan; `Mag_Starfall` tek hedef.
- [ ] Rahip: `Pri_RadiantNova` alan hasarı; `Pri_SalvationLight` seçili parti üyesine 1100 şifa (800 u içinde), hedef yoksa kendine; CD 30 sn; yetersiz mana "Yeterli mana yok!".
- [ ] Skill kitabı: Savaşçı Special0 12 satır, yazılar taşmıyor, tıklanabilir; Rahip/Okçu/Büyücü satırları 28 px. Satır sırasının (45/58 önce) kafa karıştırıp karıştırmadığına bak.
- [ ] Hotbar'a 10'dan fazla beceri atanmaya çalışıldığında seçim yapılabiliyor mu.

**T6 — HUD ipuçları (DÜŞÜK-1/8)**
- [ ] Yeni Savaşçı: sınıf panelini seç, 8 sn hareket etme → H1; ilk adımda kalıcı iptal. Doran'a yaklaş → H2 (gerçek balon varken çıkmıyor).
- [ ] Doran'a tıkla, görevi hızlıca kabul et, pencereyi hemen kapat: replik balonu ile ipucu çakışıyor mu? H2 görev kabulünden sonra tekrar çıkıyor mu (bayat)?
- [ ] Görevi kabul et, 20 sn öldürme yapma → H3. İlk ganimet → H4. İlk seviye atlama → H5. 5 Goblin biter, teslim etmeden → H6. Lv1'de bilerek öl → H7. Çantayı 12'ye doldur → H8.
- [ ] Her ipucu oturumda bir kez; envanter/stat/görev penceresi açıkken çıkmıyor, kapanınca (≤20 sn) çıkıyor.
- [ ] Lv11+ karakterde H1-H6 hiç çıkmıyor; Lv20 üstünde H7/H8 de susuyor.
- [ ] İpucu varken NPC'ye tıkla: gerçek replik görünüyor mu, ipucu bir daha çıkıyor mu (çıkmamalı; DÜŞÜK-1 tüketim davranışı).
- [ ] Uzak istemci penceresinde de balon görünüyor.

**T7 — Ekonomi/ilerleme ölçümü (isteğe bağlı)**
- [ ] `[MMOMETRIC]` ile Lv11-15 ve Lv16-20 seviye başına öldürme/süre ölç (`Scripts/PlaytestMetrics.py`); Lv16-20 5,9'a yakın çıkarsa plan A.4 seçenek (c)/(d).

---

## 6. Düzeltme önerileri özeti (kodu SEN değil mmo-developer yazar)

1. **ORTA-1:** Çözülme cebi için rota açıklığı ölçütü ekle ve cebi taşı ya da yön tarifi/koridor ekle (gerekçe: hub→cep düz hattı Obsidyen merkezinden 288 u).
2. **ORTA-2:** `UMMOSkillComponent::ApplySaveData` sonunda `SyncLearnedFromMastery()` çağır; `CanLearn`'daki "Level N gerekli" mesajını seviye yeterliyken düzelt.
3. **DÜŞÜK-1:** İpucu gösterim anında koşulu yeniden doğrula (H2); "gösterildi" bit'ini balon tam süre kaldıysa işaretle.
4. **DÜŞÜK-4:** Starfall/Impaler/SalvationLight için güç/CD/mana ince ayarı ya da yan etki (zevk kararı).
5. **DÜŞÜK-5/9:** Skill kitabı listesini `RequiredLevel`'e göre sırala; `ContentMap.py --write` çalıştır.
6. **DÜŞÜK-2/3:** Ekonomi karar belgesine 5,2×'i yaz; runtime `MMOProbeSpawn` tekrar ölçümü.
7. Ortam: başka ajanın `Save/` işi bitene kadar `StaticLint.py` kırmızı (bu turun commit'lerinden bağımsız).
