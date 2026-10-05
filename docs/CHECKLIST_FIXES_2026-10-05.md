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
