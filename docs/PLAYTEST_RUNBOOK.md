# Oyun içi test runbook'u (tek sayfa, sıralı kapılar)

**Tarih:** 2026-10-05. Yerelde (Windows, UE 5.7) ilk oturumda **yukarıdan aşağı** izlenecek tek sayfa. Her kapı bir öncekinin geçmesini ister: bir kapı **KALDI** ise dur, sonucu aşağıdaki şablonla yaz, sonraki kapıya geçme. Ayrıntılar bağlantılı belgelerde; bu sayfa yalnız sırayı ve geçme ölçütünü tutar.

| Kapı | Ne | Ayrıntı | Süre [TAHMİN] | Geçme ölçütü |
|---|---|---|---|---|
| 0 | Derle + otomatik kontroller | `PR1_VERIFY.md` §1 | 10–20 dk | `Result: Succeeded` ve `RunAllChecks.ps1` hatasız |
| 1 | PR #1 davranış testleri 1–18 | `PR1_VERIFY.md` §2 | ≈20 dk | 18 satırın hepsi "Beklenen" sütunuyla aynı |
| 2 | İlk 7 oyun içi test | `PLAYTEST_PRIORITY.md` §2 (ayrıntılı adımlar `FIRST_PLAYTEST.md`) | ≈45 dk | 1–3 kesin geçmeli; 4–7'de veri kaybı/kopyalama yok |
| 3 | `[MMOMETRIC]` ölçüm koşusu | aşağıda | 20 dk oyun + 2 dk | Yapılan eylemlerin olayları logda; görev kabul/teslim ve özet tablo çıktı |

## Kapı 0: Derle

```powershell
git fetch origin claude/determined-cannon-a8zts0
git checkout claude/determined-cannon-a8zts0
Stop-Process -Name UnrealEditor -ErrorAction SilentlyContinue
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64 Development "-Project=<proje yolu>\MyProject.uproject" -WaitMutex
powershell -File Scripts\RunAllChecks.ps1 -SkipBuild
```

Derleme hatasında olası yerler `PR1_VERIFY.md` §1 tablosunda. Yeni dosya bulunamazsa "Generate Visual Studio project files".

## Kapı 1: PR #1 doğrulaması

`PR1_VERIFY.md` §2 tablosundaki 1–18. adımlar (2 pencere, "Play As Listen Server"). Adım 14–18 çok oyunculu XP payı, parti bonusu, ganimet ve XP eğrisi içindir; bunlarda `[MMOMETRIC] ev=kill` satırlarını logdan oku.

## Kapı 2: İlk 7 test

`PLAYTEST_PRIORITY.md` §2 tablosu (doğuş + başlangıç paketi → temel döngü → uzak istemci pencereleri → spawner zemini → ticaret/depo/klan/arkadaş → `MMOTravel` → ölçüm). 7. test Kapı 3'tür. Komut ve tuş ayrıntıları `FIRST_PLAYTEST.md`'de.

## Kapı 3: `[MMOMETRIC]` koşusu

1. Editörü kapatıp aç (eski logu temizlemek için `Saved/Logs/MyProject.log` yeni başlar).
2. Yeni karakter (konsol komutu **kullanma**: `MMOGainXP`/`MMOGiveItem` ölçümü bozar), tek sınıf, 20 dk normal oyna: görev al/teslim et, öldür, sat/al, gerekirse öl.
3. Oyunu kapat, çalıştır:
   ```powershell
   python Scripts\PlaytestMetrics.py Saved\Logs\MyProject.log
   ```
4. Çıktıyı (seviye başına süre/öldürme/ölüm/altın/nominal görev XP'si; görev kabul→teslim süresi) olduğu gibi sonuç şablonuna yapıştır. Model karşılaştırması: `python Scripts\ProgressionModel.py --max 30` (model, ölçüm değil).

**7 Ekim görev ölçüm notu:** `quest_accept`/`quest_claim` yalnız başarılı sunucu
işlemleridir. Reddedilen/tekrarlanan teslim olay üretmez. `XP(quest*)` veri
tablosundaki nominal ödüldür; net XP bakiyesi veya seviye tavanındaki gerçek kazanç
değildir. Altın ayrıca `ev=gold` ile sayılır. Kabul kaydı aynı log/süreç/oyuncu adı
altında yoksa `ort_sure=?`; önceki oturumdan yüklenen görevin süresi tahmin edilmez.
Eski loglarda görev sütununun 0 olması sıfır görev kazancı kanıtı değildir.
Bu araç henüz kalıcı hesap kimliğiyle farklı oturumları birleştirmez.

**İlk görev eylemi ölçümü:** Sunucu `first_quest_accept` / `first_quest_claim`
olaylarını başarılı kod yolunda birer kez yazar. `scope=controller`, `sid`
rastgele geçici oturum kimliği, `start` controller BeginPlay zamanı, `elapsed`
bu başlangıçtan geçen saniyedir. Yeni pawn aynı controller altında tekrar olay
üretmez; controller seamless aktarımında durum taşınır. Yeniden bağlanma/yeni
controller yeni sid demektir; kalıcı karakterin ilk görevi ölçülmez. Kaydedilmiş
aktif görev teslimi, bu oturumun ilk kabulünden önce ilk teslim olabilir.
Parser sid bazında ilk süreyi korur; tekrar/geçersiz kayıtları sayar. Eksik olay
`sure=?`, eski logda ilk olay yoksa süre bilinmiyor; sıfır varsayılmaz. `--who`
oyuncu adını filtreler, ad değiştiren oyuncunun farklı ad altındaki satırlarını
birleştirmez. İlk olaylar görev adedi, XP ve altını tekrar toplamaz.

**İlk gerçek ganimet:** `first_loot` aynı controller sid/start/elapsed kapsamını
kullanır. Yalnız pickup'ın gerçekten verdiği ilk pozitif miktar kaydedilir:
`kind=gold|item|equipment item=<id> amount=<teslim>`. Kısmi sarf toplaması ilk
olaydır; kalan yerde olabilir. Dolu çanta/yığın, ölü/rezervasyona yetkisiz
karakter ve sıfır altın kazancı ilk olay değildir. İlk altın mı, iksir mi,
ekipman mı olduğundan bağımsız olarak sid başına tek loot olayı vardır.
Başlangıç paketi/görev/satıcı/debug verme bu olayı tetiklemez. Ekipman loot'u
kuşanma değildir; ayrı `first_equip` olayı kullanılır. Eski logda eksik loot `sure=?`; first
satırı normal `gold` akışını veya XP/görev sayısını yeniden toplamaz.

**İlk öldürme kredisi:** `first_kill` aynı controller sid/start/elapsed
kapsamında, mevcut mob ölüm akışındaki canlı kredi alıcısına bir kez yazılır.
`mob`, `mlv`, `xp`, `elite` ve XP verilmeden önceki `lv` kaydedilir.
`xp` mevcut paylaşımın hesapladığı ödüldür; seviye tavanında gerçek XP
artışı ölçümü değildir (normal `kill` satırıyla aynı anlam).

| credit | Anlamı |
| --- | --- |
| fatal | Geçerli karakterin son vuruşu; kendi kredisi |
| contribution | Son vuruşu yapmayan, geçerli kendi hasar katkısı olan alıcı |
| party | Kendi hasar katkısı olmadan parti paylaşımı alan alıcı |
| fallback | Son vuruş kaynağı geçersiz/oyuncu dışı; sunucunun taze katkılardan seçtiği yedek öldüren |

Parti veya fallback olayı gerçek son vuruş olarak yorumlanmamalı. `xp=0`
kredisi geçerlidir; ölçüm XP kazancı şartına bağlı değildir. Ölmeden yapılan
vuruşlar ve ölü alıcılar olay üretmez; sonraki öldürmeler tekilliği sıfırlamaz.
Olay normal kill/XP sayısını ikinci kez artırmaz. Eski logda eksik olayın
süresi bilinmez. Pawn/gerçek seamless geçiş/reconnect ve uzaktaki istemci
oynanışı PIE'de açık; fixture süreleri gerçek av süresi değildir.

**İlk başarılı kuşanma isteği:** `first_equip`, mevcut sunucu kuşanma isteği
çanta/kimlik/seviye/slot kontrollerini geçip ekipman ve bonusları güncelledikten
sonra, aynı controller sid/start/elapsed kapsamında bir kez yazılır.
`item=<id> instance=<InstanceId> slot=<0..3>`; slotlar silah=0, zırh=1,
başlık=2, bot=3. InstanceId oyuncunun envanterine aittir, global eşya kimliği
değildir. Başlangıç paketinin otomatik kuşanması ve kayıtlı ekipmanın yüklenmesi
ilk olay sayılmaz. Çantada olmayan/eskimiş kimlik, seviye şartını sağlamayan veya
geçersiz slot isteği de sayılmaz; başarılı ikinci kuşanma tekilliği sıfırlamaz.
Ölçülen şey mevcut EquipFromBag/EquipByInstanceId isteğinin başarı zamanıdır;
debug/C++ üzerinden aynı istek de sayılır. Loot, satın alma veya envantere verme
tek başına kuşanma değildir. İlk olay XP/altın/görev/kill toplamını artırmaz.
Eksik ölçüm `sure=?`; kayıtlar kalıcı karakterin ilk kuşanmasını kanıtlamaz.
Fixture otomatik kuşanma girişini ve bellekte kayıt uygulamasını sınar; gerçek
başlangıç dalı, dosyadan yeniden giriş, istemci RPC/HUD ve ServerTravel PIE'de açık.

Ölçüm satırları yalnız sunucuda (host) basılır. Logda hiç satır yoksa Kapı 3 **KALDI**: büyük olasılıkla istemci penceresinin logu okunuyor.

## Sonuç şablonu (kopyala, doldur, bana gönder)

```text
Tarih / commit (git rev-parse --short HEAD):
Kapı 0  Derleme: GEÇTİ | KALDI   RunAllChecks: GEÇTİ | KALDI
        Hata (ilk 5 satır):
Kapı 1  GEÇTİ | KALDI   Kalan adımlar (no + ne gördün):
Kapı 2  1:__ 2:__ 3:__ 4:__ 5:__ 6:__ 7:__   (G = geçti, K = kaldı, A = atlandı)
        Kalan testler (no + ne gördün):
Kapı 3  GEÇTİ | KALDI   Sınıf: ____  Ulaşılan seviye: __  Ölüm: __
        PlaytestMetrics.py çıktısı:
Log satırları ([MMODBG] / Error:, ilgili olanlar):
Genel his (hız, zorluk, kafa karıştıran yer):
```

Bulgular `PLAYTEST_PRIORITY.md` §3'e işlenir; geçen maddeler `PLAYTEST_CHECKLIST.md`'de `[x]` yapılır.

Kapı 3 çıktısı, `docs/DECISIONS_NEEDED.md`'deki 7 kararın (XP hızı, ölüm cezası, altın sink'i, hız hedefi vb.) her birinin "Gereken veri" satırını besler; karar vermeden önce o sayfaya bak.

## İlk kabul edilen beceri (7 Ekim 2026)

`first_skill` mana ödemesinden sonra, `ExecuteSkill` etkilerinden önce sunucuda
üretilir; alanlar `skill=<DB id> effect=<0..6> mana=<maliyet, 3 ondalık>` ve
etki öncesi `lv`dir. Mana alanı o becerinin ödenen DB maliyetidir; buff sonrası
mana bakiyesi, toplam tüketim veya hasar/şifa faydası ölçümü değildir.

| effect | Beceri türü |
| --- | --- |
| 0 | Yakın hasar |
| 1 | Uzak hasar |
| 2 | Kendinin çevresine alan hasarı |
| 3 | Hedefin çevresine alan hasarı |
| 4 | Kendine şifa |
| 5 | Kendine buff |
| 6 | Hedefe şifa (mevcut kendine dönüş kuralı dahil) |

Tam canla şifa ve boş hedef alanında AoESelf mevcut kurala göre kabul edilir;
ikisi de ilk kullanım olabilir. Bilinmeyen/öğrenilmemiş/başka sınıf veya düşük
seviye becerisi, ölüm, mana/cooldown reddi ve hedef gerektiren beceride hedef
yokluğu olay değildir. İstemcinin iyimser cooldown'u sunucu kabulü sayılmaz.
C++/debug yolundan kabul edilen kullanım da sayılır; HUD'a basılma ölçümü değildir.

Aynı controller sid/start/elapsed kapsamında yalnız ilk beceri yazılır;
pawn değişimi ve controller aktarımı tekilliği korur, yeni controller yeni
oturumdur. Kalıcı karakterin ilk becerisi veya öğrenme zamanı ölçülmez.
`first_skill` normal kill/XP/altın/görev toplamlarını artırmaz. Eski/eksik olay
`sure=?`; hasar vermemiş veya hiç beceri kullanmamış olduğu çıkarılmaz.
Gerçek 20 dk koşu, uzak istemci reddi, hub, seamless travel ve reconnect PIE'de açık.

Geçersiz first marker saatleri gözlem süresine eklenmez; hata sayısı raporda
kalır. Native test logları aynı parser ile who/saat/payload yönünden ayrıca
doğrulanır. Sentetik bozuk log gerçek oyun koşusu sayılmaz.

## İlk kabul edilen temel saldırı (7 Ekim 2026)

`first_basic_attack` normal `AttackTarget` sunucu dalında bütün ölüm/cooldown/
hedef/menzil/PvP/parti/klan/hub/doğum koruması kapılarından sonra, `ApplyDamage`
öncesinde yazılır. Alanlar `target=mob|player targetid=<arketip|Player> tlv=<hedef
seviyesi>`; `lv` saldıranın hasar ve XP öncesi seviyesidir. Oyuncu hedefinin adı
yazılmaz. Bu kabul edilen saldırı ölçümüdür; ilk pozitif hasar, mouse tıklaması,
animasyon başlangıcı veya beceri kullanımı değildir. Geçersiz/ölü hedefin mevcut
en yakın canlı moba dönüşü başarılı olursa gerçek seçilen mob kaydedilir.

Mevcut controller `sid/start/elapsed` kapsamında yalnız bir kez yazılır;
pawn değişimi ve controller aktarımı sıfırlamaz, yeni controller yeni oturumdur.
Kalıcı karakterin ilk vuruşu değildir. C++/debug üzerinden kabul edilen saldırı
da sayılır; controllerless saldırı sayılmaz. Shipping'de bu yeni hook kapalıdır.
Temel saldırı ve beceri birbirinden ayrı ilk olaylardır; kill/XP/altın/görev
toplamını artırmazlar. Eski veya eksik logda `sure=?`; hiç saldırılmadığı sonucunu
vermeyin. Dört sınıfın temel saldırısı halen mevcut ortak yakın vuruş yoludur;
bu değişiklik okçu/büyücüye yeni menzil veya oyun kuralı eklemez.

`TestFirstBasicAttack.ps1`: izole native dört sınıf, PvP, nearest fallback,
öldürücü vuruş/XP öncesi seviye ve ret kapıları; log ayrıca ortak parser'dan geçer.
Gerçek uzak RPC, kullanıcı giriş süresi, seamless travel ve 20 dk koşu PIE'de açık.
