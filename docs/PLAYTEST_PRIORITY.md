# Güncel durum, açık sorunlar ve ilk oyun içi test sırası

**Tarih:** 2026-10-04. Kaynak: `PLAYTEST_CHECKLIST.md`, `SECURITY_AUDIT.md`, `NETWORK_AUDIT.md`, `VERTICAL_SLICE_LV1_30.md`, `FIRST_PLAYTEST.md` ve kod sayımı. Bu belge oynanarak doğrulanmış hiçbir şey içermez; etiketler: **[KOD]** kaynakta okundu, **[BELGE]** başka bir belgeden alındı, **[TAHMİN]** doğrulanmadı.

## 1. Durum

- 48 mob arketipi, 73 görev, 53 ekipman, 73 beceri (12'si 2026-10-04'te boşluk dolgusu, PLACEHOLDER değerli), 12 efekt; Lv1'den Lv80'e kesintisiz görev zinciri **[KOD]**.
- Sistemler: stat, envanter, beceri, görev, parti, ticaret, klan, arkadaş, depo, sohbet, PvP kuralları, bölge geçişi (host `MMOTravel`), kayıt **[KOD]**.
- Otomatik kontroller (veri denetimi, derleme, Summer/Winter duman testi, ağ bağlantı testi) geçiyor **[BELGE]**. Bunlar oyunun oynanabildiğini göstermez.
- `PLAYTEST_CHECKLIST.md` içinde ~880 madde var, yalnızca 4'ü işaretli **[BELGE]**. Doğrulama borcu özellik üretiminden hızlı büyüyor.

## 2. İlk oyun içi test sırası (yukarıdan aşağı; bir adım bozuksa durup düzelt)

Süreler tahmindir **[TAHMİN]**. 2 pencere, "Play As Listen Server".

| # | Test | Neden bu sırada | Ne bozuksa ne kırılır |
|---|---|---|---|
| 1 | Yeni karakterle doğuş noktasında 3 dk bekle, öl ve yeniden doğ. Başlangıç paketi (Kısa Kılıç + Deri Zırh kuşanılı, 3 iksir) geldi mi? | Önceki doğuş hataları ölüm döngüsü üretmişti; paket yeni, hiç oynanmadı. | Her yeni oyuncu ilk dakikada ölür ya da silahsızdır. |
| 2 | Temel döngü: hareket, saldırı, ganimet, `Q_Goblins` kabul/teslim, replik balonu. | Gerisinin ön koşulu. | Diğer testlerin anlamı kalmaz. |
| 3 | **Uzak istemcide** envanter, görev, beceri ve stat pencereleri. Çantadan kuşan/yükselt/sat. | Son ağ düzeltmesi bu alanları yalnızca sahibe replike etti ve işlemleri `InstanceId`'ye taşıdı. Derlendi, hiç oynanmadı; istemci UI'sını bozma riski en yüksek değişiklik bu **[TAHMİN]**. | İstemci altınını/çantasını göremez, işlemler başarısız olur. |
| 4 | Spawner'lar zeminde mi: Summer ve Winter cepleri, özellikle Lv58–80. Erken bölgelerde Lv58+ mob çekiliyor mu? | Spawner'lar daha önce iki kez haritanın dışında çıktı. | Zincirin görevleri kilitli kalır. |
| 5 | Ticaret, depo, klan, arkadaş (2 oyuncu). | En çok kötüye kullanım riski taşıyan sistemler. | Eşya/altın kopyalama, kayıp. |
| 6 | Bölge geçişi `MMOTravel`: karakter, klan, depo, görev ilerlemesi geçişte korunuyor mu; çıkış-giriş sonrası kayıt. | Kayıt ve geçiş bağlı. | Veri kaybı. |
| 7 | 20 dakikalık koşuda `[MMOMETRIC]` satırlarını topla, `Scripts/PlaytestMetrics.py` çalıştır. | Denge ayarı ölçüme dayanmalı. | Hız/ekonomi ayarı körlemesine olur. |

## 3. Açık sorunlar (öncelik sırasıyla)

**Güvenlik ve kimlik**

| ID | Sorun | Durum |
|---|---|---|
| K-1 | Kayıt yuvası oyuncunun seçtiği ada bağlı **[BELGE]**. PR #1'de kısmi hafifletme: çevrimiçi oyuncunun adıyla bağlanma reddedilir, ad değişince başka yuvanın üzerine yazma engellenir, isteğe bağlı `-MMOPassword=` (derlenmedi). | **Açık:** çevrimdışı birinin adını `?Name=` ile kapıp kaydını yüklemek. Kalıcı çözüm hesap sistemi (`ACCOUNT_BACKEND_COMPARISON.md`). |
| Y-2 | Kayıt yüklemede ana stat, altın/yığın/nadirlik tavanı, yanlış slot, yinelenen `InstanceId`, görev sayacı doğrulaması PR #1'de eklendi (derlenmedi). Klan alanı ve görev ön koşul zinciri hâlâ doğrulanmıyor **[BELGE]**. | Büyük kısmı kapandı; derleme bekliyor. |
| Y-3 | `NVIDIA_API_KEY` ve `ANTHROPIC_API_KEY` kalıcı kullanıcı ortam değişkeni; ajanlar geniş izinle çalışıyor **[BELGE]**. | Yerelde senin kararın; anahtar rotasyonu önerilir. |
| Y-5 | Varsayılan oyuncu adı OS kullanıcı adını içeriyordu; `<ad>-<hex>` kalıbı `Gezgin######` ile değiştirilir (`ChangeName`, PR #1, derlenmedi; `AGameModeBase::ChangeName` imzası UE 5.7'de doğrulanmalı) **[BELGE]**. | Kod tarafı kapandı; derleme bekliyor. |
| Y-6 | Debug/hile komutları (`MMOGiveItem`, `MMOGainXP`, `MMOSpawnMob` vb.) Shipping derlemede erken döner (PR #1, derlenmedi). `~` konsolunun Shipping'de kapalı olduğu doğrulanmadı **[BELGE]**. | Kod tarafı kapandı; Shipping paketinde doğrulama bekliyor. |
| Y-7 | `ServerSetName` için uzak istemcilere 30 sn soğuma eklendi (PR #1, derlenmedi). `/klan kur` disk taraması 30 sn önbellekli (önceki tur). | Kapandı; derleme bekliyor. |

**Ağ ve sunucu (`NETWORK_AUDIT.md`)**: Y3 (can/mana adımlama ve efekt süresi `EndServerTime`) PR #1'de yapıldı, derlenmedi. Açık: O4 (parti/ticaret davet kapma), O5 (arkadaş rızası), O6, O7, O8, O11, O12 (denetim logu) yapılmadı **[BELGE]**. Ölü karakterin satıcıdan işlem yapması ve yükseltmesi engellendi (D3, PR #1, derlenmedi). `MMOPartyComponent.cpp` ve `MMOTradeComponent.cpp` yerelde Codex'in elinde olduğu için bulutta dokunulmadı **[BELGE]**. Uzak istemcilere bazı geri bildirim mesajları hâlâ gitmiyor olabilir; dedicated server'a geçişin önünde engeller listelenmiş.

**Oynanış ve denge (hepsi `[PLACEHOLDER]`, ölçülmedi)** **[BELGE]**
- Lv1–30 yolu çok kısa (~80–85 öldürme); Lv30'da 49–51 harcanmamış beceri puanı birikiyor (yeni 8 beceri aynı rütbeyi paylaştığı için puan fazlasını azaltmaz).
- Altın kaynağı bol, harcama yolu zayıf; `Q_Gold*` görevleri harcama değil birikim.
- Ölüm neredeyse bedava; savunma formülü tehdidi büyük ölçüde siliyor.
- Warrior ve Priest'te Lv24–70 arası hasar becerisi platosu; Warrior Lv80 capstone'u Lv70'inkinden zayıf (QA Bulgu #5, #6).
- Lv61→65 ve Lv75→80 aralıkları görevsiz uzun öğütme.

**PR #1'deki kod değişikliklerinin hiçbiri derlenmedi** (güvenlik/ağ düzeltmeleri, 12 beceri veri satırı, Y3 replikasyon değişiklikleri). Ayrıntı ve olası derleme hata yerleri: PR açıklaması. Yerelde `Scripts/RunAllChecks.ps1` çalıştırılmadan birleştirilmemeli.

## 4. Önerilen sıra (geliştirme)

1. Yukarıdaki test sırasını bir kez oyna; bulguları bu belgenin 3. bölümüne işle (checklist'i değil).
2. Test sonucuna göre: temel döngü bozuksa onu düzelt. Bozuk değilse K-1 (kayıt yuvasını görünen addan ayırma) önce, hesap sistemi sonra.
3. İçerik üretimine (yeni bölge, yeni sistem) ancak 1. adım tamamlandıktan sonra dön.

## 5. Bulutta bilerek yapılmayanlar

- Kalan beceri boşluğu: her sınıfta Lv28→33 (5 seviye), `ValidateData.py` UYARI basıyor. Eklenen 12 beceri (Lv13, Lv20, Lv28) tahmini değerlerle yazıldı; ölçüm sonrası ayarlanmalı.
- Denge ayarı (XP hızı, altın sink, ölüm cezası): `[MMOMETRIC]` verisi gelmeden körlemesine olur.
- K-1 (kalıcı çözüm: hesap sistemi) ve `MaxTokens`: tasarım kararı ya da ölçüm gerektiriyor.

## 6. Parti ve çok oyunculu adalet (2026-10-04)

- **Parti üye sınırı kodda yok [KOD].** `MMOPartyComponent.cpp` içinde azami üye sayısı kontrolü bulunmuyor; XP bölme mantığı (`AMMOMob::HandleDeath`) parti büyüklüğünü sınırsız kabul ediyor. 8 kişilik üst sınır isteniyorsa davet/kabul yolunda eklenmeli. `MMOPartyComponent.cpp` yerelde Codex'te olduğu için bulutta dokunulmadı; yerelde `ApplyPartySet`/davet kabulüne `Members.Num() >= 8` kontrolü eklenebilir.
- **Kill XP'si hasar payına orantılı ve parti payı üye sayısına bölünür** (PR #1, derlenmedi): %1 hasar = %1 XP, 8 kişilik partide herkes payın 1/8'ini alır. Parti artık toplam XP'yi artırmıyor, yalnızca paylaştırıyor; parti teşviki (bonus) ayrı bir tasarım kararı.
- **Ganimet** ilk 10 sn yalnızca XP payı olanlara rezerve (PR #1, derlenmedi).
- `Scripts/ProgressionModel.py` ve `EconomyModel.py` **solo** oyuncuyu modeller; parti/paylaşım yok.
