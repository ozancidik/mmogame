# İlk oyun içi test planı (≈45 dk, 2 oyuncu)

Bu plan `docs/PLAYTEST_CHECKLIST.md`'nin (2900+ satır) en riskli yerlerini sıraya koyar. Hepsi
derlendi ve görüntüsüz (headless) duman testinden geçti (`Scripts/SmokeTest.ps1`, `Scripts/NetSmokeTest.ps1`),
ama **hiçbiri gerçek oyunda oynanmadı**. Önce 1–3 çalışmıyorsa diğerlerinin anlamı yok.

## Hazırlık
1. Editörü aç, `Summer_scene` açık olsun. Play ayarları: 2 oyuncu, "Play As Listen Server" (zaten ayarlı).
2. Her iki pencerede konsol: `` ` `` tuşu. Adlandırma: host'ta `MMOSetName Host`, istemcide `MMOSetName Istemci`
   (misafir `PlayerN` adları arkadaş listesine eklenemez).
3. Sınıf seç (üstteki Savaşçı/Okçu/Büyücü/Rahip düğmeleri).

## 1) Temel döngü (5 dk) — bozuksa dur
- WASD / tıkla-git, Z ile hedef, R saldırı. Hub'da (başlangıç) mob saldırmamalı.
- Başlangıç bölgesinde bir mob kes: üstte `+N EXP` uçmalı, loot düşmeli.
- Ekranın üstünde "Yakinda: <mob> LvN" tehdit yazısı çıkmalı.

## 2) Sohbet (5 dk)
- Enter → yaz → Enter: iki ekranda görünmeli. `/yardim`, `/who`, `/w Istemci selam`.
- Yazarken WASD karakteri oynatmamalı.

## 3) Görevler ve replikler (5 dk)
- Görev NPC'sine (sarı) tıkla, `Q_Goblins` kabul et: **Yaşlı Doran** balonu çıkmalı. Öldür, teslim et.
- `MMOGainXP 5000` ile hızlan, zincirin sonraki görevi açılmalı.

## 4) Parti + PvP kuralları (5 dk)
- `MMOInvite` / istemcide `MMOPartyAccept`. Parti XP paylaşımı.
- `MMOPvP 1`: partideyken birbirine vuramamalı ("Parti üyesine vuramazsın"); partiden çık, hub dışında vurabilmeli;
  hub'da (başlangıç noktası 700u) vuramamalı.

## 5) Ticaret (10 dk) — en çok exploit riski olan sistem
- `MMOGiveItem ShortSword 2` (iki tarafa farklı eşya ver) → `/trade` (ya da `MMOTrade`), istemci `/trade kabul`.
- Eşya ekle/çıkar, altın: `MMOTradeGold 100`. Birinin teklifi değiştiği an diğerinin onayı sıfırlanmalı.
- İki taraf onaylayınca eşya + altın karşılıklı geçmeli; çantada aynı eşya iki kopya olmamalı.
- Ticaret açıkken uzaklaş (800u+) → iptal. 3 dk bekle → zaman aşımı.

## 6) Depo, klan, arkadaş (10 dk)
- Depocu Rana'ya (satıcının karşısı) yaklaş, tıkla: eşya koy/al; oyunu kapat-aç, depo korunmalı.
- `/klan kur Demir`, `/klan davet` → istemci `/klan kabul`, `/g merhaba` (yeşil), isim plakasında `[Demir]`.
- `/arkadas ekle Istemci`, `/arkadas`.

## 7) Bölgeler (5 dk)
- Host: `MMOTravel` (ya da HUD düğmesi) → Winter'a geç. Karakter, klan, depo, görev ilerlemesi korunmalı.
- Winter'da `MMOGainXP 600000` ile Lv51+ ol, `Q_FrostboundScouts` al, `(3000,0)` Gözcü cebine git.
- `MMOSave` / çıkış-giriş sonrası konum ve envanter doğru mu?

## Bulduğun her şeyi
- Hata satırını `Saved/Logs/MyProject.log` içinden `[MMODBG]`, `Error:` ile ara.
- Bulguyu checklist'teki ilgili maddeyi `[x]` yaparak işaretle; çökme/duvar varsa Claude'a ilet.

## Bilinen sınırlar (bilerek)
- Kimlik oyuncu adına bağlı, şifre/hesap yok (klan, arkadaş, kayıt hepsi ad anahtarlı).
- `bAllowPvP` varsayılan açık (test için); beceriler yalnız mob'lara vurur, PvP yalnız yakın dövüş.
- Klan lideri devri komutu yok; arkadaş eklemede karşı taraf bilgilendirilmez.
