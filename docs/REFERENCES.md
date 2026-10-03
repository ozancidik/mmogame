# Dış referanslar (GitHub araştırması, 3 Ekim)

GitHub arama API'siyle "Unreal + MMORPG" ve genel MMO sunucu çerçeveleri tarandı. Yıldız sayıları o günkü değerlerdir.

## Unreal'a özel MMORPG depoları
Hepsi çok küçük (en yüksek 14 yıldız): `edwinyung/MMORPG` (14, Blueprint), `AlStartlight/NEMEMO` (3, GPL-3.0),
`juanmarques/ragna-th` (1, MIT; Ragnarok Online'ı UE5 + C++ + GAS ile yeniden yapıyor), `Cussk/MMORPGFramework` (1).
**Kullanılabilir olgun bir Unreal MMORPG çerçevesi yok;** bizim kod tabanı zaten bunların hepsinden ileride.

## Genel MMO sunucu depoları (en çok yıldız)
| Depo | Yıldız | Dil | Lisans | Not |
|---|---:|---|---|---|
| TrinityCore/TrinityCore | 10.8k | C++ | **GPL-2.0** | WoW sunucu emülatörü; ayrı dünya/giriş/veritabanı sunucuları |
| azerothcore/azerothcore-wotlk | 9.0k | C++ | **GPL-2.0** | WoW (3.3.5) modüler sunucu |
| rathena/rathena | 3.6k | C++ | **GPL-3.0** | Ragnarok Online sunucu emülatörü |
| ketoo/NoahGameFrame | 4.2k | C++ | Apache-2.0 | Dağıtık oyun sunucusu çerçevesi (motor bağımsız); son güncelleme 2023 |
| otland/forgottenserver | 1.8k | C++ | **GPL-2.0** | Tibia sunucu emülatörü |

## Bizim için anlamı
- **GPL kodunu projeye kopyalamak olmaz:** GPL, türev çalışmanın tamamını GPL yapar; ticari oyun kapanır.
  TrinityCore/AzerothCore/rAthena yalnızca **tasarım referansı** olabilir (formüller, görev/ödül akışı,
  hesap/karakter ayrımı) — fikirler serbest, kod değil.
- **Mimari uyuşmazlık:** Bunlar ayrı sunucu süreçleri (giriş + dünya + veritabanı); biz Unreal'in kendi
  istemci–sunucu replikasyonunu ve `listen/dedicated server` modelini kullanıyoruz. Doğrudan "kullanmak"
  yeniden mimari demek.
- **Asıl boşluğumuz** (kimlik/hesap, kalıcı veritabanı, birden çok dünya sunucusu) için iki gerçekçi yol:
  1. Unreal dedicated server + ayrı küçük bir hesap/karakter servisi (kendi yazımımız; Apache-2.0
     `NoahGameFrame` gibi bir çerçeve yalnızca ağ/soket katmanı için referans olabilir),
  2. Barındırılan bir backend (hesap + veritabanı) kullanmak.
  Her iki durumda önce kimlik tasarımı gerekir (şu an her şey oyuncu **adına** bağlı, şifre yok).
- **Klasik KO referansı:** `Open-KO/KnightOnline` (daha önce skill/hotbar tasarımında kullanıldı).

## Karar bekleyen konu
"Hesap + kalıcı veritabanı" ayrı bir mimari iş; GPL depoları bunun için kod kaynağı olarak kullanılamaz.
