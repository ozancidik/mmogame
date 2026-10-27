# Güncel durum, açık sorunlar ve ilk oyun içi test sırası

**2026-10-09:** Lv46–80 içerik, 12 yeni beceri, DoT/ekonomi/spawner değişiklikleri ve güncel sayılar (59 mob, 104 görev (2026-10-10: 7 ara seviye yan görev eklendi; 2026-10-27: +7 Q_Gold bağış satırı, karar 3 P1), 57 ekipman,
93 beceri, 35 spawner) için [GUNLUK_OZET_2026-10-09.md](GUNLUK_OZET_2026-10-09.md); yeni içeriğin ilk oyun testi
[PLAYTEST_LV46_80.md](PLAYTEST_LV46_80.md) (`MMOTeleport` ile cepleri gezme). 104 görev tanımı var. **Son doğrulanan aggregate paket (O41, a1897b8+O41+port): 41/41 kontrol geçti**;
Son NetSmoke çalışma değişkeni düzeltmesi ardından iki mod ek smoke geçti (§79).
19 CPP NoPCH /W4 derlendi, takas230, DoT46 ve parti185 native kontrolü geçti. Summer/Winter ve NetSmoke başarılı;
gerçek PIE hâlâ açık. O41 ana master entegrasyonu, actual build ve native185/vendor18
tamamlandı; kaynak snapshot aynı. Sabit snapshot ve kanıt sınırları
[CHECKLIST_FIXES_2026-10-05.md, §79](CHECKLIST_FIXES_2026-10-05.md)
içindedir.

**2026-10-07:** Güncel yaptık/yapacağız tablosu ve doğrulama sınırları
[PROJECT_STATUS_2026-10-07.md](PROJECT_STATUS_2026-10-07.md) içindedir. Son otomatik
paket 23/23 geçti; gerçek PIE oynanışı açık. Aşağıdaki tarihli değerlendirmelerdeki
"derlenmedi" ve eski içerik sayıları o değerlendirme tarihini anlatır; güncel
derleme/içerik durumu için yeni tablo ve CHECKLIST_FIXES_2026-10-05.md kullanılır.

> **Test oturumu için başlangıç noktası: `docs/PLAYTEST_RUNBOOK.md`** (derle → `PR1_VERIFY` 1–18 → aşağıdaki ilk 7 test → `[MMOMETRIC]` koşusu, geçti/kaldı şablonu). Bu belge durum ve açık sorun listesidir.

**Tarih:** 2026-10-04. Kaynak: `PLAYTEST_CHECKLIST.md`, `SECURITY_AUDIT.md`, `NETWORK_AUDIT.md`, `VERTICAL_SLICE_LV1_30.md`, `FIRST_PLAYTEST.md` ve kod sayımı. Bu belge oynanarak doğrulanmış hiçbir şey içermez; etiketler: **[KOD]** kaynakta okundu, **[BELGE]** başka bir belgeden alındı, **[TAHMİN]** doğrulanmadı.

## 1. Durum

- 48 mob arketipi, 73 görev, 53 ekipman, 73 beceri (12'si 2026-10-04'te boşluk dolgusu, PLACEHOLDER değerli), 12 efekt; Lv1'den Lv80'e kesintisiz görev zinciri **[KOD]**.
- Sistemler: stat, envanter, beceri, görev, parti, ticaret, klan, arkadaş, depo, sohbet, PvP kuralları, bölge geçişi (host `MMOTravel`), kayıt **[KOD]**.
- Otomatik kontroller (veri denetimi, derleme, Summer/Winter duman testi, ağ bağlantı testi) geçiyor **[BELGE]**. Bunlar oyunun oynanabildiğini göstermez.
- `PLAYTEST_CHECKLIST.md` içinde 992 açık (`- [ ]`) madde var, yalnızca 4'ü işaretli (2026-10-05 sayımı) **[KOD]**. Doğrulama borcu özellik üretiminden hızlı büyüyor.

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

**Ağ ve sunucu (`NETWORK_AUDIT.md`)**: Y3 can/mana adımlama ve efekt süresi tamamlandı. O6'nın asenkron kayıt bariyeri, zaman aşımı ve kilit kurtarma kısmı tamamlandı; travel sonrası ilerleme penceresi PIE bekliyor. O11'de sunucunun ilk HUD'ına doğrudan çizilen hasar/iyileştirme sayıları `Unreliable NetMulticast` yoluna taşındı ve derlendi. Kurulu Epic binary motor Server target'ını desteklemedi; gerçek dedicated executable için kaynak UE 5.7 motoru gerekiyor. Açık: O4 (parti/ticaret davet kapma), O5 (arkadaş rızası), O7, O8, O11'in admin/Server-target kalanları ve O12 denetim logu.

**Oynanış ve denge (hepsi `[PLACEHOLDER]`, ölçülmedi)** **[BELGE]**
- Lv1–30 yolu çok kısa (~80–85 öldürme); Lv30'da 49–51 harcanmamış beceri puanı birikiyor (yeni 8 beceri aynı rütbeyi paylaştığı için puan fazlasını azaltmaz).
- Altın kaynağı bol, harcama yolu zayıf; `Q_Gold*` görevleri harcama değil birikim.
- Ölüm neredeyse bedava; savunma formülü tehdidi büyük ölçüde siliyor.
- Warrior ve Priest'te Lv24–70 arası hasar becerisi platosu **kısmen kapandı** (2026-10-10): Lv52/64/75'e her sınıfta üç yeni beceri eklendi; Lv80 capstone'lar Lv75 halkalarının üstünde (güç/sn: Savaşçı `War_DoomsdayBlow` 1725/26 ≈ 66,3 vs `War_WorldBreaker` 1400/24 ≈ 58,3; Büyücü 1350/26 ≈ 51,9 vs 1100/24 ≈ 45,8; QA #5 2026-09-14'te kapandı), yani "capstone zayıf" bulgusu bayat. Gerçek denge verisi (`[MMOMETRIC]`) hâlâ gerek.
- Lv61→65 ve Lv75→80 görevsiz öğütme **kapandı** (2026-10-10): `CONTENT_MAP.md` Lv59–79 arasında her seviyede en az bir görev gösteriyor (yan görevler dahil); XP kalibi hâlâ placeholder.

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

- **Parti üye sınırı 8'dir [KOD]:** `UMMOPartyComponent::FormOrJoin` içinde `MaxParty = 8`; birleşme 8'i aşarsa tümden reddedilir ("Parti dolu (maksimum 8)"). XP bölme (`AMMOMob::HandleDeath`) parti büyüklüğünü `Party->GetPartyInRange` ile okuyor, yani en çok 8'e bölünür. (Önceki bir sürümde burada "sınır yok" yazıyordu; yanlıştı, düzeltildi.)
- **Kill XP'si hasar payına orantılı ve parti payı üye sayısına bölünür** (PR #1, derlenmedi): %1 hasar = %1 XP; parti bonusu (+%10/ekstra üye, `MMOXPTuning::PartyMultiplier`) ile 8 kişilik partide herkes payın 1,7/8'ini alır (2 kişide %55).
- **Ganimet** ilk 10 sn yalnızca XP payı olanlara rezerve (PR #1, derlenmedi).
- `Scripts/ProgressionModel.py` ve `EconomyModel.py` **solo** oyuncuyu modeller; parti/paylaşım yok.
