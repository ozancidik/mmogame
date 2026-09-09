# Harita Seçenekleri (Moradon/Dorion tarzı kasaba) — 2026-08-28 araştırması

**Neden import etmedim:** Gerçek bir kasaba haritası tasarlamak/yerleştirmek görsel iterasyon ister; ben viewport'u göremediğim için körlemesine büyük bir ortam import edersem bozuk/yanlış-lisanslı sonuç riski yüksek. Bu yüzden **seçenekleri araştırıp bıraktım; kararı sen ver, sonra ben import/yerleştirme yaparım.**

## Önerilen yol: Fab'daki ÜCRETSIZ Epic paketleri (login gerekir)
Epic periyodik olarak Fab'da ücretsiz "ayın ücretsizi" ortam paketleri yayınlıyor — genelde MMO/RPG kasabası için birebir:
- **Medieval Village** (Marco Maria Rossi) — 400+ modüler ortaçağ köyü asseti, bir dönem Fab'da ücretsizdi. KO Moradon / Rise Dorion havasına en yakın ücretsiz aday.
- **Fantasy Town** (George Shachnev, "Flying Fantasy Town") — 350+ fantezi kasaba asseti, ücretsiz dönemi olmuştu.
- Fab hesabına "claim" edince kalıcı; UE5 (Lumen/Nanite) uyumlu, binalar girilebilir.

## Ücretli (yüksek kalite, istersen)
- **Modular Medieval Town** — tüm binalar girilebilir, UE5 optimizasyonlu.
- **Modular Fantasy Medieval Town** — 78 bina parçası + 58 prop + 13 hazır bina + su shader'ı; RPG için tasarlanmış.
- **Medieval Fantasy Megapack** — "gelişmiş MMO dünyaları için" diye pazarlanıyor.

## Nasıl ilerleriz (sen seçince)
1. Sen Fab'dan bir paketi hesabına **claim** et (login bende olmadığı + kararı senin vermen gerektiği için).
2. UE'de "Add to Project" ile projeye ekle (ya da bana söyle, Epic Games Launcher/Fab akışını tarayıcıdan senin oturumunla deneyeyim).
3. Ben `GameMode` spawn noktalarını (vendor/quest/healer/talker + mob spawner'lar) yeni haritanın kasaba merkezine göre **kod tarafından** ayarlarım; NavMesh volume'u haritaya sığdırırım.
4. `GameDefaultMap`/`EditorStartupMap`'i yeni haritaya çeviririm.

## Not
- Kaynaklar: [CG Channel - 400 free medieval village assets](https://www.cgchannel.com/2025/12/get-400-free-assets-for-building-a-medieval-village-in-unreal-engine/), [CG Channel - 350 free fantasy town assets](https://www.cgchannel.com/2025/07/get-350-free-assets-for-building-a-fantasy-town-in-unreal-engine/), [Fab - Modular Medieval Town](https://www.fab.com/listings/7564ffa6-ab7b-4f1a-a551-0b7983cedc41), [Fab - Modular Fantasy Medieval Town](https://www.fab.com/listings/476bd048-799d-4988-91b6-d71645792257)
- Mixamo yaratık pipeline'ı (Warrok gibi) çalışıyor; ama Mixamo insansı ağırlıklı — kurt/örümcek yok. Kasaba haritası ayrı bir iş.
