# Güncel durum, açık sorunlar ve ilk oyun içi test sırası

**Tarih:** 2026-10-04. Kaynak: `PLAYTEST_CHECKLIST.md`, `SECURITY_AUDIT.md`, `NETWORK_AUDIT.md`, `VERTICAL_SLICE_LV1_30.md`, `FIRST_PLAYTEST.md` ve kod sayımı. Bu belge oynanarak doğrulanmış hiçbir şey içermez; etiketler: **[KOD]** kaynakta okundu, **[BELGE]** başka bir belgeden alındı, **[TAHMİN]** doğrulanmadı.

## 1. Durum

- 48 mob arketipi, 73 görev, 53 ekipman, 61 beceri, 6 efekt; Lv1'den Lv80'e kesintisiz görev zinciri **[KOD]**.
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
| K-1 | Kayıt yuvası oyuncunun seçtiği ada bağlı; ad değiştirerek başkasının kaydı ezilebilir ya da yüklenebilir **[BELGE]**. | Açık. Kritik, ama yalnızca tanıdıklarla oynarsan pratik risk düşük. Hesap sistemi planı `ACCOUNT_BACKEND_COMPARISON.md`. |
| Y-2 | Kayıt yüklemede ana stat doğrulaması PR #1'de eklendi (derlenmedi). Eşya, görev ve klan alanları hâlâ doğrulanmıyor **[BELGE]**. | Kısmi. |
| Y-3 | `NVIDIA_API_KEY` ve `ANTHROPIC_API_KEY` kalıcı kullanıcı ortam değişkeni; ajanlar geniş izinle çalışıyor **[BELGE]**. | Yerelde senin kararın; anahtar rotasyonu önerilir. |
| Y-5 | Varsayılan oyuncu adı Windows kullanıcı adını içeriyor **[BELGE]**. | Açık. |
| Y-6 | Debug komutları (`MMOGiveItem`, `MMOGainXP` vb.) Shipping'den çıkarılmamış **[BELGE]**. | Açık. Test aşamasında zararsız, yayında kapatılmalı. |

**Ağ ve sunucu (`NETWORK_AUDIT.md`)**: Can/mana adımlama (Y3), O4–O8, O11, O12 yapılmadı **[BELGE]**. Uzak istemcilere bazı geri bildirim mesajları hâlâ gitmiyor olabilir; dedicated server'a geçişin önünde engeller listelenmiş.

**Oynanış ve denge (hepsi `[PLACEHOLDER]`, ölçülmedi)** **[BELGE]**
- Lv1–30 yolu çok kısa (~80–85 öldürme); Lv30'da 49–51 harcanmamış beceri puanı birikiyor.
- Altın kaynağı bol, harcama yolu zayıf; `Q_Gold*` görevleri harcama değil birikim.
- Ölüm neredeyse bedava; savunma formülü tehdidi büyük ölçüde siliyor.
- Warrior ve Priest'te Lv24–70 arası hasar becerisi platosu; Warrior Lv80 capstone'u Lv70'inkinden zayıf (QA Bulgu #5, #6).
- Lv61→65 ve Lv75→80 aralıkları görevsiz uzun öğütme.

**Bu PR (#1) derlenmedi.** Yerelde `Scripts/RunAllChecks.ps1` çalıştırılmadan birleştirilmemeli.

## 4. Önerilen sıra (geliştirme)

1. Yukarıdaki test sırasını bir kez oyna; bulguları bu belgenin 3. bölümüne işle (checklist'i değil).
2. Test sonucuna göre: temel döngü bozuksa onu düzelt. Bozuk değilse K-1 (kayıt yuvasını görünen addan ayırma) önce, hesap sistemi sonra.
3. İçerik üretimine (yeni bölge, yeni sistem) ancak 1. adım tamamlandıktan sonra dön.
