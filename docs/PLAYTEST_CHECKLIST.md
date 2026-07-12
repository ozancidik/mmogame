# Oynayarak Test Kontrol Listesi

Bu dosya, Claude'un otonom (sen yokken) geliştirdiği özelliklerin **senin PIE'de test etmen** gereken maddelerini biriktirir.
Her özellik derlenip commit'lendiğinde buraya eklenir. Test edip onayladıkça maddeleri işaretle (`[x]`).

> Not: Claude kodu yazar, derler ve derlendiğini garanti eder. Oynanış/his/görsel doğrulaması sana kalır.

---

## Bekleyen testler

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

---

## Onaylanmış (test edilip çalıştığı doğrulandı)

<!-- Test edip onayladığın maddeleri buraya taşı -->
