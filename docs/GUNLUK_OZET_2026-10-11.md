# Günlük özet — 2026-10-11 (Claude)

Önceki gün: `GUNLUK_OZET_2026-10-10.md`. Bu belge yalnız 11 Ekim'de değişenleri toplar. **Oyun içinde (PIE) ve gerçek uzak
istemciyle hiçbir şey denenmedi.** Test sayıları commit mesajlarından alındı; bu özeti yazan belge denetçisi native testleri
yeniden koşmadı (yalnız aşağıdaki Python denetimlerini koştu).

## Yapılanlar

| Alan | Ne | Kanıt |
|---|---|---|
| Ekonomi | Şifacı bedeli seviyeyle büyür: `taban + 0,4 x (seviye-1)^2` (Lv80 2.596; tamsayı formül) | `da26db5`; model `b93a456` (`EconomyModel.py` yeni kalemler) |
| Yeni sistem | Başarımlar ve unvanlar: 28 başarım, `/basarim`, `Saved/MMOAchievements/<yuva>.json` | `c3e9693`, `144b158` (unity derleme adı çakışması) |
| Yeni sistem | Günlük görevler: UTC günü başına 3 görev, `/gunluk`, `Saved/MMODaily/<yuva>.json` | `1241445` |
| Yeni sistem | Posta: çevrimdışı oyuncuya mesaj + altın eki, 10 altın + %2 ücret (altın sink'i), `Saved/MMOMail/MMOMail_<ad>.json` | `8365173`, `fe573e7` (kutu adı küçük harf) |
| Araç | `Scripts/RunSystemTests.ps1`: başarım/günlük/posta (sonra ad kilidi) native testleri tek komut | `a46bbf2`, `7932979` |
| Süreç | Proje ajanları + CLAUDE.md "Agent usage" zorunlu yönlendirme tablosu | `a702590` |
| Tasarım | Hesap kimliği ara adımı (B-min) tasarımı `ACCOUNT_IDENTITY_DESIGN.md` | `b93a456` |
| Güvenlik tur 1 | QA + güvenlik bulguları: posta altın kopyalama (K1), kayıt yolu/yuva sabitleme, ad değişimi istismarı (B1/B2) | `43e87ca` |
| Güvenlik kaydı | K-1 "yeni yüz" (ad değiştirerek karakter çatallama F1-F5) AÇIK olarak işlendi | `941860d` |
| Güvenlik tur 2 | Uzak istemci kayıtlı karakterin adını değiştiremez (F1), ad sorgulama soğuması (F5), `MMOLoad` kayıtsız adda değişiklik yok (F3), EndPlay'de yan dosya taşıma (F2) | `afba349` |
| Güvenlik tur 3 | Posta `.bak` yedekli yazım, bozuk/okunamayan kutuda `Load` false + gönderimde iade, tekrar denemesiz kurtarma + 60 sn geri çekilme, ad kilidi soğuması diskten önce, yan dosya temizliği (Y1-Y4, Y6-Y8) | `2bb2023` |
| Güvenlik tur 4 | ASCII ad kuralı + kanonik çakışma anahtarı (`Public/Core/MMONameRules.h`, Y-NEW-1), tekrar denemesiz yazım `Move`'u (Y-NEW-2), güvenli eski yuva silme (Y-NEW-3/4), posta `BoxFull`/`StoreError` soğuması (Y-NEW-5), yeni karakter yeniden possess (Y-NEW-6), ilk-posta `.bad` politikası (QA DÜŞÜK-2/3), mesajlar (DÜŞÜK-4/5) | `d031b90` |
| Testler | Başarım 35 -> 61 -> 82 -> **96**, günlük 30 -> 53 -> 67 -> **81**, posta 41 -> 89 -> 104 -> **130**, ad kilidi 23 -> **58** kontrol; tur 4'te 14 mutasyon denendi, 13'ü yakalandı ("c2" süreç içinde ulaşılamaz). Dört çalıştırıcı `RunAllChecks.ps1` adımı | `4c58f2d`, `a1f6145`, `7932979` (mesajlar: RunSystemTests 96/81/130/58 GEÇTİ, `RunAllChecks -SkipBuild` GEÇTİ) |
| Belge (bu tur) | CLAUDE.md, `SECURITY_AUDIT.md` K-1 (tur 3/4 tablosu + kalan açıklar), `ACCOUNT_IDENTITY_DESIGN.md` (§2 satırları, §10), `DECISIONS_NEEDED.md` (tur 4 karar adayları), `MAIL.md`, `ACHIEVEMENTS.md`, `DAILY_BOUNTIES.md`, `PLAYTEST_CHECKLIST.md` (K1H + GÜNCEL notları), `FIRST_PLAYTEST.md`, `QA_ROUND_2026-10-07_CODEX.md` kodla eşlendi | bu commit |
| Denetim | `ContentMap.py --check` "koddan üretilenle aynı"; `DocCounts.py` 59 mob / 97 görev / 57 ekipman / 93 beceri, sapma 0; `PocketSpacing.py` 18 cep, 0 hata, 2 bilinen uyarı (P48–Gözcü 1733 u, Nöbetçi–P78 1581 u) | 2026-10-11 akşam, belge denetçisi |

## Kim neye dokundu

- **Claude** (trailer'lı commit'ler): yukarıdaki tablonun Codex satırları dışındaki her şey; kaynakta `MMOCharacter.*`,
  `MMOGameMode.cpp` (ad kuralı), `MMOClanComponent.cpp` (klan adı ASCII), başarım/günlük/posta bileşenleri, `Mail/`,
  `Core/MMONameRules.h`, testler ve `Scripts/Test*.ps1`.
- **Codex** (trailer'sız commit'ler, görev kayıtları `docs/tasks/`): `b0f22bc` (12 uzman ajan + güvenli görev akışı),
  `a0a1b91` (iç içe hasarda ölüm/XP cezası tekilleştirme, `TestReentrantDeath.ps1`, O66), `aac6dff` (O67 posta kutusu uyumluluk
  staging betiği, etkinleştirme yok), `a0cb86d` (O68 posta okuma hatasında üstüne yazma engeli, `TestMailReadFailure.ps1` 24 kontrol;
  `RunAllChecks.ps1` adımı değil). Codex'in commit'siz `.agents/` klasörüne dokunulmadı.

## Dürüst riskler

- **PIE yok:** başarım/günlük/posta/ad kilidi native testleri tek süreçte senkron mantık, depo ve dosya sistemini doğrular;
  sohbet komutu, HUD, zamanlayıcı, gerçek RPC ve iki istemci kanıtı değildir. İlk oyun içi liste: `PLAYTEST_CHECKLIST.md` K1F/K1G/K1H.
- **Kimlik = oyuncu adı** (K-1) hâlâ temel açık: ad kilidi yalnız çatallamayı kapatır; `?Name=<başkası>` ile çevrimdışı karakteri
  devralma açık. Kalıcı çözüm hesap modu (tasarımda, kod yok).
- **Açık düşük maddeler** (`SECURITY_AUDIT.md` K-1 "Kalan açık / düşük maddeler"): ikinci bozulmada `.bad` üstüne yazılır;
  `WriteBox` false dönüp içerik diskte kalırsa iade + teslim olabilir (iki ardışık dosya hatası gerekir; mutasyon c2 yakalanmadı);
  gönderen kaydı ile kutu yazımı arasında çökme altını kaybettirir (kopyalamaz); `?Name=Ömer` -> `mer` kırpması; Linux harf
  varyantı (F4) ASCII kuralıyla çözülmedi; birden çok sunucu süreci için posta kilidi yok.
- **Göç yok:** Türkçe harfli eski kayıt/posta kutuları erişilemez kalır (bugün diskte elle seçilmiş ad yok, `ACCOUNT_IDENTITY_DESIGN.md` §2.4).
- **Başarım geriye dönük ödülü yok:** dosyası olmayan Lv>1 karakterde seviye/görev başarımları ödülsüz açılır (karar adayı).
- Sayılar (ücretler, ödüller, şifacı katsayısı) PLACEHOLDER; ekonomi modeli ölçüm değil.

## Açık kararlar (`DECISIONS_NEEDED.md`)

- Kayıtlı karakter için ad değiştirme özelliği gerekli mi (bugün uzak istemcide kilitli; geçici önlem).
- Türkçe harfli ad gerekli mi -> ICU tabanlı kanonik anahtar + tek seferlik göç.
- Bağlantı adındaki izinsiz harf: kırpmak mı (bugün), `PreLogin`'de reddetmek mi (öneri), yoksa `Gezgin######` mi.
- Başarım: sistem öncesi karakterlerin geriye dönük ödül almaması kabul mü.
- Şifacı katsayısı (kare 0,4 yetersiz; küp öneri) ve iksir fiyatı ölçeklemesi.
- Önceki günlerden: ölüm cezası, yan görev XP kalıbı, mob -> oyuncu zehri, Burn/Lore, hesap/kimlik, dedicated server.
