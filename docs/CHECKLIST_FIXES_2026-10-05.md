# Playtest listesi: 2026-10-05 duzeltmeleri

Kod yazildi ve derlendi, PIE'de OYNANMADI. Asagidakiler kullanici donunce denenecek.
(Ana liste `PLAYTEST_CHECKLIST.md` baska oturumun; bu dosya yalniz bu uc duzeltmeyi kapsar.)

## 1. Esya kaybi: yigin tavani 999

- [ ] Depocu/saticida iksir yigini 990'a getir (`MMOGiveItem HealthPotion 990`), 20 iksir satin almayi dene:
      yalniz 9 alinmali, altin yalniz 9 iksirlik dusmeli, mesajda "tavana ulasildi" yazmali.
- [ ] Yigin 999 iken satin alma: "En fazla 999 tasiyabilirsin", altin degismemeli.
- [ ] `MMOGiveItem HealthPotion 5000` (host debug): yigin 999'da durmali. Kaydet (`MMOSave`), cik, yukle: hala 999.
- [ ] Yerdeki sarf loot'u (ornegin 5 adet) yigin 997 iken topla: 2 alinmali, yerde 3 kalmali, "yigini dolu" mesaji 3 sn'de bir gelmeli;
      birkac iksir harcayinca pickup'in ustune tekrar girince kalan alinmali.
- [ ] Sarf odullu gorevi yigin tavanina yakin iken teslim et: odul reddedilmeli (XP/altin verilmemeli, gorev acik kalmali),
      yer acip tekrar teslim edince tam odul gelmeli.
- [ ] Depo: bozuk/eski kayitla depoyu >24 esyaya cikar (kayit elle duzenleme) ya da test icin sabiti gecici artir;
      depo penceresinde TUM satirlar panel icinde cizilmeli, en alttaki esya tiklanip cantaya cekilebilmeli.

## 2. Beceri bekleme geri alma (uzak istemci)

- [ ] Listen-server + 1 istemci: istemcide menzilde dusman yokken hedef gerektiren beceriye bas: "Menzilde hedef yok"
      gelmeli ve bekleme gostergesi (hotbar sayaci) hemen kapanmali, beceri tekrar kullanilabilmeli.
- [ ] Istemcide mana neredeyse bitikken (replike mana gecikmesi yuzunden) beceri dene: "Yeterli mana yok!" gelmeli, bekleme kalmamali.
- [ ] Gecerli kullanimda bekleme normal calismali (reddedilmeyen beceride geri alma OLMAMALI, tam sure beklemeli).
- [ ] Cooldown icindeyken hizli tekrar basma: sunucu reddederse istemci sayaci sunucunun GERCEK kalan suresine inmeli
      (silinmemeli; beceri erkenden acilmamali).

## 3. Uzak istemci mesajlari (ToOwner)

- [ ] Listen-server + 1 istemci, istemcide konsola `MMOSave`: "Kaydedildi" istemcide de gorunmeli (sohbet sistem satiri).
      Host'ta `MMOSave`: ekranda "Kaydedildi". 60 sn'lik OTOMATIK kayitta (ve klan/ticaret kaydinda) mesaj GORUNMEMELI.
- [ ] Istemci altin yigini topla: "+N altin" gorunmeli. Art arda 3-4 altin topla (0.5 sn icinde): TEK satir, toplam tutar dogru olmali.
      Host'ta ayni davranis (ekranda tek "+N altin", ~0.5 sn gecikmeli).
- [ ] Istemci ile moba temel saldiri: ara vuruslarda "VURDUN" satiri GELMEMELI (sohbeti doldurmamali); olduren vurusta
      "VURDUN: ... >>> OLDU!" gelmeli. Hasar sayilari / kritik gostergesi eskisi gibi calismali.
- [ ] Host kendi saldirisinda her vurusta VURDUN mesajini eskisi gibi gormeli (yerel yol degismedi).

## 4. QA bulgu 2: ActiveSaveSlot erken atama + kayit reddi mesaji

Kod yazildi ve derlendi, PIE'de OYNANMADI.

- [ ] Navmesh yokken (`-NullRHI` hizli baslangic ya da Winter erken `MMOLoad`) mevcut kayitli karakterle gir; host'ta
      `MMOSetName <diskte kayitli baska bir ad>`, sonra `MMOSave`: "Bu ad baska bir kayda ait: kaydedilmedi..." mesaji gelmeli,
      diskteki o baska kayit DEGISMEMELI.
- [ ] Navmesh bekleme dali bittikten sonra (karakter konuma tasindiktan sonra) normal `MMOSave` calismali ("Kaydedildi").
- [ ] Uzak istemci olarak ayni red durumu: istemcide de mesaj gorunmeli (otomatik kayitta da, ~60 sn'de bir tekrar gelebilir; beklenen).
