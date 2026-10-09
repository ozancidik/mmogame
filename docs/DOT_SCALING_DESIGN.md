# Zehir / DoT ölçekleme tasarımı (karar bekliyor, kod yazılmadı)

**Tarih:** 2026-10-09. **Durum:** tasarım notu. Kod değişmedi; karar sahibi proje sahibi, değerler ölçümle ayarlanır.
Etiketler: **[Kesin]** kodda okundu · **[Hesap]** elle hesaplandı · **[Tahmin]** ölçülmedi.

## Sorun

`Poison` tek bir sabit: `PeriodicDamage`, tick başına **8**, 8 sn, tick aralığı 1 sn, yani uygulama başına en çok **64** hasar
(`MMOEffectDatabase.cpp:33`) **[Kesin]**. Üç şey birleşince etkisiz kalıyor:

1. **Büyüklük seviyeyle büyümüyor.** Lv58+ moblar vuruş başına 222–350 hasar verir; 64 toplam DoT, tek vuruşun yaklaşık %20–30'u **[Hesap]**.
2. **Savunma DoT'tan da düşüyor.** DoT `UGameplayStatics::ApplyDamage` ile ana hasar yoluna girer
   (`MMOStatsComponent.cpp:938`), orada `Mitigated = max(1, Ham - Defense)` uygulanır (`:412`). `Defense = Vit × 0,5 + ekipman + buff`
   (`:360`) **[Kesin]**. Savunması 8'in üstündeki herkese tick başına fiilen **1** hasar gelir: oyuncuda yaklaşık Lv10'dan sonra,
   Lv31–39 mobunda (Vit 74–100 → savunma 37–50 **[Hesap, SKILL_PLAN_LV31_45.md]**) her zaman.
3. **Sahip yoksa DoT düşer.** Oyuncu kaynaklı DoT kaynak kaybolunca atlanır (`:929-931`); bu doğru ve korunmalı.

Sonuç: Zehir efekti kullanan 10+ mob ve 6 beceri (Arc_PoisonArrow, Arc_DeathMark, Arc_ThornVolley, Arc_SpikeTrap, Arc_PoisonCloud,
bkz. `MMOSkillDatabase.cpp`) pratikte yalnızca bir ikon gösteriyor. Lore'da Kül mobları "yakar", kodda zehir verir; ayrı bir `Burn`
eklemek aynı nedenle etkisiz kalır, o yüzden eklenmedi (`CONTENT_PLAN_LV46_80.md` G8).

## Seçenekler

| | Değişiklik | Etki | Risk |
|---|---|---|---|
| **A** (önerilen) | DoT **savunmayı atlar** (gerçek hasar) **ve** büyüklük kaynağın saldırısına bağlanır: tick = `Def.Magnitude`'a ek olarak `kaynak vuruş hasarı × k` | DoT seviyeyle büyür, savunma yatırımı DoT'u silmez | PvP ve mob DoT'u toplam hasarı artırır; `k` ve süre denge ayarı gerektirir |
| B | Yalnız büyüklük ölçeklenir, savunma atlanmaz | Küçük düzeltme | Savunma ölçeği (Vit×0,5 + ekipman) hâlâ ölçeğin üstünde kalabilir; yeterli olmayabilir |
| C | Tick = hedefin **azami canının %'si** (üst sınırla) | Seviyeden bağımsız çalışır | Boss/elit ve oyuncuya karşı ayrı sınır gerekir; PvP'de can havuzu büyükse patlayabilir |
| D | Bırak, Zehir'i vurgu/ikon olarak kabul et | Sıfır risk | Altı beceri ve 10 mob "ölü efekt" kalır |

## A için taslak (uygulama notu, kod değil)

- `FMMOActiveEffect` büyüklüğünü **uygulama anında** hesapla: `ApplyEffect` (`MMOStatsComponent.cpp:739`) içinde `PeriodicDamage` için
  `A.Magnitude = Def->Magnitude + SourceHit × k`. `SourceHit` mobda `MobAttackDamage`, oyuncuda `AttackPower`. Yenileme dalı (`:763`)
  da aynı hesabı yapmalı. `k` başlangıç önerisi **0,08–0,12** (8 sn'de toplam ≈ tek vuruşun 0,6–1,0 katı) **[Tahmin]**.
- Savunmayı atlama: `UMMOStatsComponent::ApplyDamage` (`:411` civarı) için `bIgnoreDefense` parametresi ya da ayrı bir `ApplyTrueDamage`.
  DoT tick'i (`:938`) şimdi `UGameplayStatics::ApplyDamage` kullanıyor; bayrağı taşımak için `UDamageType` alt sınıfı
  (`UMMODotDamageType`) kullanıp `AMMOCharacter::TakeDamage` ve `AMMOMob::TakeDamage` içinde sınıfa bakmak en az iz bırakır.
- **Güvenlik kapıları korunmalı:** hub güvenli bölge (`:916-926`), kaynak kaybolunca atlama (`:929-931`), PvP kapısı. Bunların hepsi
  DoT'un **hasar yoluna girmeden önce** işleniyor, değişmemeli.
- **Test:** mevcut `Scripts/TestDotSourceLifecycle.ps1` (kaynak yok olması, reentrant tick) beklentisini hasar
  büyüklüğüne bağlıyorsa güncellenir (okunmadı); yeni 3 kontrol: (1) yüksek savunmalı hedef DoT'tan hasar alır, (2) büyüklük kaynak vuruşuyla
  artar, (3) hub'daki oyuncu DoT'tan ölmez.

## Neden şimdi uygulanmadı

- Savaş dengesini değiştirir (özellikle mob → oyuncu zehri ve PvP); PIE ölçümü olmadan `k` seçmek tahmin.
- `MMOStatsComponent.cpp` ve mob/karakter `TakeDamage` yolları Codex'in test kapsamında; değişiklik onun regresyonlarını etkiler.
- Gerekli veri: `[MMOMETRIC]` içinde DoT'tan gelen hasar payı yok. Önce "toplam hasarın yüzde kaçı DoT" ölçülmeli.

**Önerim [Tahmin]:** A, `k = 0,10` ile; önce yalnız oyuncu → mob yönünde (becerilerdeki Zehir) açılsın, mob → oyuncu zehri bir sonraki
ölçümden sonra. İlk playtest öncesi gerekli değil.
