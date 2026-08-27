# Oynayarak Test Kontrol Listesi

Bu dosya, Claude'un otonom (sen yokken) geliştirdiği özelliklerin **senin PIE'de test etmen** gereken maddelerini biriktirir.
Her özellik derlenip commit'lendiğinde buraya eklenir. Test edip onayladıkça maddeleri işaretle (`[x]`).

> Not: Claude kodu yazar, derler ve derlendiğini garanti eder. Oynanış/his/görsel doğrulaması sana kalır.

---

## Bekleyen testler

### Elit ganimet iyilestirmesi (garantili + daha iyi ekipman) — batch #5
- [ ] **Elit garantili ekipman:** Bir eliti oldur (SpiderQueen/StoneGolem/ShadowAssassin). HER seferinde en az 1 ekipman lootu (silah/zirh) yere dusmeli — %25 sansa artik takilma yok.
- [ ] **Boss cift drop:** Ork Reisi (OrcWarlord) oldurulunce **2 ayri ekipman** dusmeli (+ altin + iksir sansi). Ayrica ganimeti en comert olmali (+2..+3).
- [ ] **Daha yuksek +N:** Elit dropu +0 gelmemeli. SpiderQueen +1..+2; StoneGolem/ShadowAssassin +1..+3; OrcWarlord +2..+3 aralikta olmali (loot toplayinca ekipman cantasinda +N gozle).
- [ ] **Seviyeye uygun ust-tier:** Elit droplari mob seviyesine yakin ust-tier itemler olmali (or. Lv7 elit: Ejder Kilici / Buyuk Kilic / Plaka Zirh / Ejder Kask bandindan), baslangic Kisa Kilic/Deri Baslik degil.
- [ ] **Nadirlik hala yuksek:** Elit ekipmani cift-zar sayesinde ortalama daha nadir gelmeli (mavi/mor/turuncu isim daha sik).
- [ ] **Normal mob regresyonu (kritik):** Goblin..Trol + OrcWarrior/DireWolf/Warlock (bIsElite=false) ganimeti DEGISMEMELI: altin ayni, ~%25 ekipman sansi, cogu +0, nadiren +1, tek drop. Yeni alanlar default=0 oldugu icin hicbir sey degismemeli.
- [ ] **Loot sadece server'da:** (host oyunda) loot yalniz otoritede basiliyor; client tarafinda cift/hayalet loot olusmamali.

### Gorev odul-esyalari (quest reward items) — batch #4
- [ ] **Ekipman odulu geliyor mu:** Odul-esyali bir elit gorevi tamamla (or. Q_SpiderQueen). Gorev penceresinde tikla → odul al. Envanter/canta'ya ilgili ekipman (Plaka Zirh) +0, Siradan nadirlikte GELMELI. Hangi gorev hangi item: Q_SpiderQueen=Plaka Zirh, Q_StoneGolem=Ejder Kilici, Q_ShadowAssassin=Ejder Kask, Q_Warlocks=Savas Baltasi, Q_Boss=Buyuk Kilic.
- [ ] **Tuketilebilir odulu geliyor mu:** Q_Goblins tamamla → envantere **3 Can Iksiri** (F ile kullanilan HealthPotion sayaci 3 artmali). Q_Wolves → 3 Mana Iksiri. Q_Level5 → 2 Buyuk Mana Iksiri.
- [ ] **Cift-odul YOK:** Odul alindiktan sonra gorev aktif listeden kalkmali; tekrar tiklanamaz olmali. Envantere item yalniz BIR kez eklenmeli (2 kopya gelmemeli).
- [ ] **XP/gold hala dogru:** Odul-esyali gorevler eskisi gibi XP + altini da vermeli (item EK olarak, XP/gold yerine degil).
- [ ] **Odulsuz gorevler regresyonu:** Item odulu ATANMAYAN gorevler (Q_Orcs, Q_Bandits, Q_Cleanup, Q_Gold500, Q_Gold1000, Q_Trolls) eskisi gibi calismali; hicbir item vermemeli, sadece XP+altin.
- [ ] **HUD odul satiri:** Gorev penceresinde odul-esyali gorevlerin alt satirinda "Odul: X XP, Y altin, +ItemAdi" (ekipman) / "+N Can Iksiri" (tuketilebilir) gorunmeli; odulsuz gorevlerde ek olmamali.

### Ikinci savas bolgesi (Lv6-7 normal moblar) — batch #3
- [ ] **Ikinci bolge uzakta doguyor mu:** Play bas, baslangictan eksende ileri (Facing×4 ≈ 2800u) + saga ~1600u kaydirilmis ayri bir alanda ~5 mob dogmali. Diger spawner/NPC'lerle cakismamali (en yakini Golge Suikastci, ~1695u uzakta).
- [ ] **Sadece Lv6-7 normal cikiyor mu:** Ikinci bolgede yalniz **Ork Savascisi (Lv6)**, **Korku Kurdu (Lv6)**, **Kara Buyucu (Lv7)** dogmali. Lv1-5 (Goblin/Kurt/Ork/Haydut/Trol) veya elit ASLA cikmamali.
- [ ] **Baslangic alani regresyonu (kritik):** Ana mob alaninda (Facing×1, ~700u) hala YALNIZ Lv1-5 normaller (Goblin..Trol) cikmali. Yeni Lv6-7 moblar baslangic havuzuna SIZMAMALI.
- [ ] **Ork Savascisi:** koyu kirmizi-kahve renk, Scale ~1.2, orta hiz. Vurusta %30 sansla **Slow (YVS)** — oyuncu yavaslamali.
- [ ] **Korku Kurdu:** celik grisi, Scale ~1.1, **cok hizli** (Speed ~470), genis aggro. Vurusta %35 sansla **Slow (YVS)**.
- [ ] **Kara Buyucu:** mor renk, Scale ~1.05. Vurusta %40 sansla **Poison (ZHR)** — periyodik hasar. Cani %35 altina inince **Rage (OFK)** ile ofkelenmeli.
- [ ] **Q_Warlocks görevi:** Lv6+ iken görevliden "Kara Buyuculer" alinabilmeli. Ikinci bolgede 4 Kara Buyucu öldürünce tamamlanip 800 XP + 500 altin vermeli. (Target = Warlock; sadece o mob sayilmali.)
- [ ] **Q_Warlocks temposu (denge notu):** Ikinci bolgede aninda en fazla ~1-2 Kara Buyucu bulunur (5 slot, 3 mob turu esit sansla; band'da tek Warlock var). 4 Warlock icin respawn beklenir (TopUp her 4sn). Gorevin bitmesi makul surede mumkun mu, yoksa cok mu yavas — gozle.

### Dünya eliti batch #2 (Kadim Golem + Golge Suikastci)
- [ ] **Kadim Golem** doğuyor mu: mob alanının **sol** köşesinde (Facing yönünün soluna ~700u, Orumcek Ana'nın tam karşı tarafı), tek başına. Devasa (Scale ~2.1), gri taş renkli, çok yavaş hareket. Lv7.
- [ ] Golem canı %45 altına inince **öfkelenir** (Rage): saldırı gücü artmalı (üstünde OFK ikonu/efekti).
- [ ] **Golge Suikastci** doğuyor mu: eksende çok daha ileride (Facing×3.2 ≈ 2240u), tek başına. Küçük/ince (Scale ~1.35), koyu renk, çok hızlı hareket (Speed ~480). Lv7.
- [ ] Suikastci vuruşta **%50 sansla zehir** (Poison/ZHR) uygulamalı — vurulunca üstünde zehir ikonu ve periyodik hasar.
- [ ] Her iki elit de random spawn havuzuna KARIŞMAMALI (başlangıç mob alanında normal moblar arası golem/suikastci çıkmamalı) — sadece kendi forced spawner'larında.
- [ ] Spawner konumları çakışmıyor: 4 forced spawner (Ork Reisi, Orumcek Ana, Kadim Golem, Golge Suikastci) birbirinden görünür şekilde ayrı yerlerde.
- [ ] Görev **Q_StoneGolem** ("Kadim Golem"): Orumcek Ana görevini (Q_SpiderQueen) bitirdikten sonra, Lv6+ iken görevliden alınabilmeli. Golem'i öldürünce tamamlanıp 1000 XP + 700 altın vermeli.
- [ ] Görev **Q_ShadowAssassin** ("Golge Suikastci"): Q_StoneGolem'den sonra, Lv7 iken alınabilmeli. Suikastci'yi öldürünce 1100 XP + 750 altın vermeli.
- [ ] Yeni ekipman satıcıda görünmeli: **Ejder Kilici** (Weapon, 38 atk, Lv7) ve **Ejder Kask** (Helmet, 9 def, Lv7). Kuşanınca stat bonusu doğru yansımalı.
- [ ] **Satıcı paneli taşması (regresyon):** Satıcıyı aç (E). Satın alma sütunu artık 20 satır (1 iksir + 6 tüketilebilir + 13 ekipman). **Ejder Kask, Deri Bot, Demir Bot** panelin mavi çerçevesinin **altına taşıyor mu / çerçeve dışına mı düşüyor**? En alttaki item'lar hâlâ tıklanabiliyor mu? (Panelde kaydırma/kırpma yok — liste büyüdükçe alt satırlar panel dışına çıkabilir.)

### Konuşan NPC + LLM diyaloğu (Muhafız Kael)
- [ ] Play bas. Satıcı (mavi) ve görevli (altın) NPC'lerin ilerisinde **mor** bir NPC olmalı: "Muhafiz Kael [konus]".
- [ ] A/D tuşları: karakteri sola/sağa **döndürmeli** (yerinde dönüş), yana kaydırmamalı. W/S ileri-geri.
- [ ] Kael'e yaklaş, üstüne sol tık → alt-ortada mor çerçeveli bir diyalog balonu çıkmalı (konuşan adı + replik), ~9 sn sonra solup kaybolmalı.
- [ ] **Kredi/anahtar YOKKEN** (şu anki durum): balon Kael'in yedek repliklerinden birini göstermeli (örn. "Dur. Adini ve derdini soyle..."). Output Log'da `[MMODialogue] HTTP 400` (kredi düşük) görünür — beklenen.
- [ ] **Kredi eklendikten SONRA**: balon LLM'den üretilen, seviyene göre değişen özgün bir replik göstermeli. Output Log'da `[NPC]` satırı.
- [ ] Uzaktan tıklayınca "NPC'ye yaklas" uyarısı çıkmalı (menzil dışı).
- [ ] Üst üste hızlı tıklama yeni istek atmamalı (1.5 sn spam koruması).
- [ ] 2 pencereli PIE: istemci ekranında da Kael'in ismi "Muhafiz Kael" görünmeli (varsayılan "Kasabali" DEĞİL) ve diyalog balonu çalışmalı.

### NPC karşılama replikleri (Satıcı Merek / Yaşlı Doran)
- [ ] Satıcıya tıkla → dükkan açılır VE alt-ortada Merek'ten bir karşılama repliği balonu (örn. "Celigim namusludur..."). İsim plakası "Satici Merek" olmalı.
- [ ] Görevliye tıkla → görev penceresi açılır VE Doran'dan bir karşılama repliği. İsim plakası "Yasli Doran" olmalı.
- [ ] Balon pencereyi kapatmıyor, ~9 sn sonra soluyor. Farklı tıklamalarda farklı replikler gelmeli (rastgele).

### PvP sistemi
- [ ] 2 pencereli PIE aç (Play > ListenServer, 2 client).
- [ ] Bir pencerede diğer oyuncunun karakterine sol tık → yaklaşıp saldırır, canı düşer.
- [ ] Konsola `MMOPvP 0` → artık oyuncuya vuramazsın ("PvP kapali"), ama moblara vurmaya devam.
- [ ] `MMOPvP 1` → PvP tekrar açılır.

### Kontrol düzeltmeleri
- [ ] Sağ tuşa **basmadan** fareyi oynat → kamera dönmemeli.
- [ ] Sağ tuş **basılı** + fare → kamera etrafında dönmeli.
- [ ] **WASD** ile kameranın baktığı yöne göre hareket.
- [ ] **Space** ile zıplama.
- [ ] Görevciye/satıcıya tıklayınca imleç panelin **üstünde** kalıyor.

### Buff/Debuff (durum efekti) sistemi
- [ ] Konsol: `MMOBuff Rage` → sağ altta yeşil "OFK" ikonu + geri sayım; AttackPower artmalı (bir moba vur, hasar arttı mı).
- [ ] `MMOBuff IronSkin` → "ZRH" ikonu; savunma artmalı (aldığın hasar azaldı mı).
- [ ] `MMOBuff Haste` → "HIZ" ikonu; belirgin şekilde daha hızlı koşmalısın.
- [ ] `MMOBuff Poison` → kırmızı "ZHR" ikonu; her saniye canın azalmalı (DoT).
- [ ] `MMOBuff Regen` → "YEN" ikonu; her saniye canın artmalı (HoT).
- [ ] `MMOBuff Slow` → "YVS" ikonu; yavaşlamalısın.
- [ ] Süre bitince ikon kaybolmalı ve stat/hız eski haline dönmeli.
- [ ] Ölünce tüm ikonlar temizlenmeli.
- [ ] Skill üzerinden: Okçu `Arc_PoisonArrow` ile moba vur → mobun üstünde zehir işlemeli (mob HP zamanla düşer).
- [ ] Büyücü `Mag_FrostNova` → alandaki moblar yavaşlamalı.
- [ ] Savaşçı `War_BattleShout` (Ofke), `War_IronSkin`; Okçu `Arc_Sprint` (Hız); Priest `Pri_Renew` (Yenilenme) → kendine buff bindirmeli.

### Elit/Boss moblar + mob yetenekleri
- [ ] Konsol: `MMOSpawnMob OrcWarlord` → önünde büyük kırmızı "Ork Reisi" (boss) belirmeli.
- [ ] Boss'la dövüş: sana vurunca ara sıra zehir (sağ altta "ZHR" ikonu) bulaşmalı.
- [ ] Boss'un canı %50'nin altına inince öfkelenmeli (vuruşları sertleşir — enrage).
- [ ] `MMOSpawnMob SpiderQueen` → mor "Örümcek Ana" (elit); vurunca sık sık seni yavaşlatmalı ("YVS").
- [ ] Normal `Haydut` mobu ara sıra zehir bulaştırmalı; `Trol` düşük canda öfkelenmeli.
- [ ] Elit/boss'lar normal spawn'da rastgele çıkmamalı (sadece komutla veya özel yerleştirmeyle).

### Mob leash / eve dönüş
- [ ] Bir mobu kışkırt, sonra ondan uzağa (spawn noktasından ~18m+) koş.
- [ ] Mob kovalamayı bırakıp spawn noktasına geri dönmeli.
- [ ] Eve varınca canı **tam dolmalı** (ve varsa öfke/efektleri sıfırlanmalı).
- [ ] Eve dönerken sana saldırmamalı; eve varınca tekrar normal aggro yapmalı.

### Hasarda aggro (menzilli pull)
- [ ] Uzaktaki bir moba menzilli skille vur (örn. Okçu `Arc_QuickShot`) → mob AggroRange dışında olsa bile sana doğru gelip saldırmalı.
- [ ] Vurmayı bırakınca ~8 sn sonra zorla-aggro düşer (leash/normal aggro kuralları devreye girer).

### Tüketilebilirler (mana iksiri + buff scroll'ları)
- [ ] Satıcıya git → alım listesinde "Mana Iksiri" ve 4 parşömen (Ofke/Hız/Koruma/Yenilenme) görünmeli.
- [ ] Bir parşömen satın al (altın düşmeli, envantere eklenmeli).
- [ ] Hızlı test: `MMOGiveItem ScrollRage 3`, `MMOGiveItem ManaPotion 5` ile envantere ekle.
- [ ] **G** tuşu → mana iksiri içilir, MP artar ("Kullanildi: Mana Iksiri").
- [ ] `MMOUse ScrollRage` → Öfke buff'ı biner (sağ altta "OFK" ikonu, saldırı gücü artar).
- [ ] `MMOUse ScrollHaste` / `ScrollIronSkin` / `ScrollRegen` → ilgili buff binmeli.
- [ ] Envanterde yokken kullanınca "yok!" uyarısı vermeli.

### Parti (grup) sistemi — 2 pencereli PIE gerekir
- [ ] İki oyuncu birbirine yaklaşsın. Oyuncu A konsola `MMOInvite` yazsın → en yakın oyuncu (B) davet edilmeli.
- [ ] Oyuncu B ekranında "Parti daveti aldin!" mesajı çıkmalı; B `MMOPartyAccept` yazsın.
- [ ] Her iki oyuncunun sol tarafında **PARTI** paneli belirmeli (diğer üyenin adı + HP barı).
- [ ] Bir üye hasar alınca partnerin panelindeki HP barı düşmeli (replikasyon).
- [ ] Ortak XP: A bir mob öldürsün, B menzildeyken (~30m) **B de XP almalı**.
- [ ] `MMOPartyLeave` → panel kaybolmalı, iki oyuncu da partisiz olmalı.
- [ ] Tek üye kalınca parti otomatik dağılmalı.

### Mob gezinme (idle wander) + World Boss
- [ ] Kışkırtılmamış moblar spawn noktaları çevresinde ağır ağır rastgele gezinmeli (yerinde donmamalı).
- [ ] Bir oyuncu aggro menziline girince gezinmeyi bırakıp saldırıya geçmeli.
- [ ] Oyuncu başlangıcının epey ilerisinde tek bir **Ork Reisi (world boss)** bulunmalı.
- [ ] Boss öldürülünce ~30 sn sonra aynı bölgede yeniden doğmalı.
- [ ] Normal spawn alanında elit/boss çıkmamalı (sadece bu özel boss noktasında).

### Eşya nadirlik (rarity) sistemi
- [ ] Moblardan ekipman düşür → düşerken "[Siradan/Nadir/Ender/Destansi/Efsanevi] X +N dustu" mesajı, nadirliğe göre renkli.
- [ ] Envanteri aç (I) → çantadaki ve kuşanılan eşya isimleri nadirlik rengiyle (gri/yeşil/mavi/mor/turuncu) görünmeli.
- [ ] Nadir bir eşya kuşan → Atk/Def değerleri Common'a göre daha yüksek olmalı (çarpan: Nadir 1.25x … Efsanevi 3.0x), karakter statlarına yansımalı.
- [ ] Elit/boss mobların loot'u ortalamada daha yüksek nadirlikte olmalı (iki zar, iyisini seçer).
- [ ] Satıcıda nadir eşya daha pahalıya satılmalı; satış satırı renkli görünmeli.
- [ ] Kaydet/yükle sonrası eşyaların nadirliği korunmalı (SaveGame).

### Görev zinciri + daha çok içerik
- [ ] Görevciye tıkla → başta sadece "Goblin Avi" ve yan görevler (Temizlik/Güçlen/Tüccar Ruhu) görünmeli; zincir görevleri (Kurt/Ork/Haydut/Boss) **görünmemeli** (kilitli).
- [ ] "Goblin Avi"yi bitir ve ödülü al → "Kurt Surusu" görev listesinde belirmeli.
- [ ] Zinciri takip et: Goblin → Kurt → Ork → Haydut → **Ork Reisini Yen** (boss görevi, 1500 XP + 1000 altın).
- [ ] Ön koşulu bitmemiş bir görevi almaya çalışırsan (normalde gizli) "Once '...' gorevini bitir" uyarısı.
- [ ] Boss görevi: world boss'u (Ork Reisi) öldürünce görev tamamlanmalı.

### Karakter/Stat penceresi (stat puanı dağıtma) — ÖNEMLİ boşluk kapandı
- [ ] **C** tuşu → Karakter penceresi açılmalı (STR/DEX/INT/VIT + türetilmiş statlar + dağıtılacak puan).
- [ ] Level atla → "Dagitilacak puan" artmalı (her level +3).
- [ ] Puan varken bir statın **[+]** butonuna tıkla → o stat +1, puan -1, türetilmiş statlar güncellenmeli (ör. VIT → Max HP artar).
- [ ] Puan bitince [+] butonları pasifleşmeli (tıklama iş görmemeli).
- [ ] `MMOStat str` / `dex` / `int` / `vit` konsol komutu da çalışmalı.
- [ ] Kaydet/yükle sonrası dağıtılmış statlar korunmalı.

### Pencere yönetimi: X kapat + çoklu pencere
- [ ] Her pencerenin (satıcı/görev/envanter/skill/karakter) sağ üstünde kırmızı **X** butonu olmalı; tıklayınca o pencere kapanmalı.
- [ ] **Aynı anda birden çok pencere** açılabilmeli (ör. K skill + I envanter + C karakter birlikte).
- [ ] Pencereler farklı konumlarda (skill sol, envanter sağ, görev sol-orta, karakter sağ-orta) — üst üste binmemeli.
- [ ] Üst üste binen iki pencerede tıklama, **üstteki** pencereye gitmeli.
- [ ] Bir pencereye tıklamak karakteri hareket ettirmemeli; sağ tık pencere üstündeyken kamerayı döndürmemeli.

### Saldırı ve skill animasyonları (KO tarzı)
- [ ] Bir moba saldır (sol tık / R) → karakter **kılıç savurma hareketi** yapmalı (her vuruşta 3 varyasyondan biri).
- [ ] Animasyon oynarken yürüme/koşma bozulmamalı (üst katmanda oynuyor).
- [ ] Yakın dövüş skill'i kullan (ör. Savaşçı `War_PowerStrike`, hotbar 1) → vuruş animasyonu oynamalı.
- [ ] Menzilli/büyü/şifa/buff skill'i kullan (ör. Büyücü `Mag_IceBolt`, Priest `Pri_HolyLight`) → **büyü okuma (charged)** animasyonu oynamalı.
- [ ] Mob sana vurduğunda **mob da** savurma animasyonu yapmalı.
- [ ] 2 pencereli PIE: diğer oyuncunun/mobun animasyonu senin ekranında da görünmeli (replikasyon).

### Şifacı Lyra (iyileştirici NPC / altın gideri)
- [ ] Oyun başında Şifacı Lyra, oyuncu doğuş noktasının yanında (satıcının karşı yanında) **spawn olmalı**; gövdesi açık mor/lavanta tonunda görünmeli.
- [ ] Bir moba can/mana kaybettir, sonra Lyra'ya (menzil içinde) **tıkla** → can **ve** mana tam dolmalı (full heal).
- [ ] İyileştirme sonrası **altın 100 (HealCost) azalmalı**; HUD'daki altın göstergesi güncellenmeli.
- [ ] İyileştirme sonrası Lyra'nın **onay balonu** çıkmalı (şefkatli bir replik).
- [ ] **Altın yetersizken** (Gold < HealCost) tıkla → iyileştirme olmamalı, altın düşmemeli, "yeterli altın yok" balonu çıkmalı.
- [ ] **Zaten tam sağlıklıyken** (can+mana dolu) tıkla → altın **düşmemeli**, "yaran yok, altınına kıyma" balonu çıkmalı.
- [ ] Menzil dışından tıkla → "Sifaciya yaklas" uyarısı çıkmalı, iyileştirme olmamalı.
- [ ] Ölüyken Lyra'ya tıkla (altın yeterliyse) → **dirilmeli** (tam can/mana), altın düşmeli.
- [ ] 2 pencereli PIE: Lyra'nın **tint rengi diğer client'ta da** aynı görünmeli (replikasyon).
- [ ] 2 pencereli PIE: iyileştirme + altın düşüşü yalnızca **sunucuda** olmalı; client'ta değer sunucudan replike gelmeli (client tek başına altın/canı değiştirememeli).
- [ ] **Tam can ama düşük mana** (canı dolu, manası eksik) iken tıkla → mana dolmalı **ve** altın 100 düşmeli (bu durum "zaten tam sağlıklı" sayılmamalı; sadece ikisi de doluyken bedava-geçiş olmalı).
- [ ] **Hızlı üst üste tıkla** (partial can iken 5-6 kez art arda) → altın **yalnızca bir kez** (100) düşmeli, çift/üçlü harcama olmamalı (ilk iyileştirmeden sonra oyuncu tam olduğu için sonraki istekler bedava-geçişe düşmeli).

### Örümcek Ana elit spawn + yeni içerik paketi
- [ ] Oyun başında mob alanının **yan köşesinde** (world boss'tan ayrı, farklı yönde) tek bir mor **Örümcek Ana (elit)** doğmalı.
- [ ] Örümcek Ana ile dövüş: vurunca sık sık seni **yavaşlatmalı** ("YVS" ikonu), can/hareket etkilenmeli.
- [ ] Örümcek Ana öldürülünce ~45 sn sonra aynı köşede yeniden doğmalı.
- [ ] Normal spawn alanında hâlâ elit/boss çıkmamalı; Örümcek Ana yalnızca kendi köşesinde.
- [ ] Görevciye tıkla: "Kurt Surusu"nu bitirdikten sonra **"Örümcek Ana"** yan görevi listede belirmeli (RequiredLevel 5).
- [ ] Örümcek Ana'yı öldür → görev tamamlanmalı; ödül **900 XP + 600 altın** olmalı.
- [ ] **"Trol Avi"** yan görevi (bağımsız, Lv5): 3 Trol öldür → tamamlanmalı, ödül 550 XP + 380 altın.
- [ ] **"Buyuk Servet"** yan görevi (bağımsız, Lv3): 1000 altın biriktir → tamamlanmalı, ödül 500 XP.
- [ ] Satıcıya git: alım listesinde yeni **"Buyuk Kilic"** (Lv6, 30 atk), **"Plaka Zirh"** (Lv5, 15 def) ve **"Buyuk Mana Iksiri"** (120 MP) görünmeli.
- [ ] Yeni ekipmanları kuşan → Atk/Def değerleri karakter statlarına yansımalı; nadirlik rengiyle görünmeli.
- [ ] `MMOGiveItem GreaterManaPotion 3` ile ekle, kullan → MP 120 artmalı.

---

## Onaylanmış (test edilip çalıştığı doğrulandı)

<!-- Test edip onayladığın maddeleri buraya taşı -->
