# Varlık Üretim Promptları (3D model / animasyon / efekt / harita)

**Durum:** Taslak v1 (P1), 2026-10-06. **Promptların hiçbiri denenmedi.** Kaynak: `docs/Lore.md`,
`docs/NpcDialogue.md`, `docs/MapOptions.md`, `docs/PROGRESSION_PLAN.md`, `docs/CONTENT_PLAN_LV11_45.md`,
`MMOMobArchetype.cpp`, `MMOSkillDatabase.cpp`, `MMOSkillTypes.h`, `MMOGameMode.cpp`, `World/*NPC.cpp`,
`MMOQuestTypes.cpp`, `MMOItemDatabase.cpp`. Etiketler: **[Tahmin]** = sayı/öneri benim tahminim,
**[Yorum]** = Lore'da açıkça yazmayan görsel yorum, **[Doğrulanmadı]** = araç davranışı kontrol edilmedi.

---

## 1. Nasıl kullanılır

1. Her prompt = `{STYLE}` + ilgili aile bloğu (`{FAM_*}`) + satırdaki özel prompt + `{RIG_*}` eki. Hepsini tek
   paragraf hâlinde araca yapıştır. Negatif alanı olan araçlara `{NEG}` listesini ver.
2. **Önce 3-4 varlıkla dene:** `Goblin`, `Orc` (karşılaştırma için elde Warrok var), `FrostWraith`, `Satıcı Merek`.
   Dördü yan yana aynı görünmüyorsa önce `{STYLE}`'ı düzelt, sonra toplu üretime geç. `{STYLE}`'ı değiştirdiğinde
   prompt sürümünü (P1 → P2) artır ve takip tablosuna yaz.
3. Tutarlılık için öneri: önce aynı `{STYLE}` ile 2D konsept görseli üret, beğenileni **image-to-3D**'ye ver.
   Metinden doğrudan 3D'ye geçmek her seferinde farklı oranlar verebilir. [Tahmin]
4. **Araç çıktısı olduğu gibi kullanılmaz.** UE'de beklenen el işi: ölçek/pivot, normal yönleri, malzeme
   kurulumu, LOD, çarpışma (collision), iskelet retarget'ı, kök hareketi kontrolü.
5. **Lisans:** Meshy, Tripo, Ludo.ai, Mixamo, Cascadeur ve Fab'ın ticari kullanım, ücretsiz katman ve
   atıf şartları farklıdır ve değişebilir. Her aracın kendi sitesindeki güncel şartları **üretimden önce**
   kontrol et; ücretsiz katmanda üretilen varlığın ticari oyunda kullanılıp kullanılamayacağını not et.
6. Promptlara başka oyunların adını (Knight Online, Rise Online vb.) **yazma**. IP riski taşır, bazı
   araçlar bu yüzden isteği reddedebilir. Stil `{STYLE}` içinde tarif ediliyor.
7. Bu dosya yalnızca metin. Varlık yolları ve koddaki `MeshPath`/`Anim*Path` bağlantıları geliştiricinin işi.

---

## 2. Stil kilidi

Kaynak: Lore (Akçakule sınır kalesi, Uzun Kış, Solgun Fısıltı; her bölgenin kendi rengi), haritalar
`/Game/HandPaintedEnvironment/...` paketinden geliyor (`MMOGameMode.cpp`). Bu yüzden el boyaması stilize görünüm
seçildi.

**{STYLE}**
```
Stylized hand-painted medieval dark-fantasy MMORPG game asset, classic early-2000s Asian MMORPG look, low-to-mid poly with a clean readable chunky silhouette, slightly heroic proportions (broad shoulders, large hands and weapons, about 7 heads tall for humans), hand-painted albedo texture with soft painted ambient occlusion and subtle edge highlights, no strong directional shadows baked in, muted earthy base palette of weathered steel, worn leather, dark bronze, bone white and wool grey, with exactly one saturated accent color per asset, designed to read clearly from a 3/4 top-down camera at 10-15 meters, single object, centered, neutral background
```

**Bölge vurgu renkleri** (satır promptuna `accent: ...` diye eklenir; Lore'dan):

| Kod | Bölge | Vurgu |
|---|---|---|
| `ACC_START` | Akçakule çevresi, Solgun Ormanlar | moss green and rust brown |
| `ACC_KARAKAYA` | Karakaya Geçidi | charcoal black rock with dull red |
| `ACC_FROST` | Ayazdiş Buzulu / Kış Diyarı / Buzbağlı | pale ice blue and frost white |
| `ACC_THAW` | Çözülme Yolu | teal meltwater and slag brown |
| `ACC_EMBER` | Alev Vadisi | ember orange and lava red glow |
| `ACC_OBSIDIAN` | Obsidyen Sırtları | glossy black glass with violet sheen |
| `ACC_DUSK` | Alacakaranlık Korusu | twilight violet and slate blue |
| `ACC_RIFT` | Yarık | glowing purple crack light |
| `ACC_ASH` | Kül Hattı | ash grey with smoldering coal red |
| `ACC_ABYSS` | Uçurum Yaylası | deep indigo, light-absorbing black |

**{NEG}**
```
photorealistic, realistic skin pores, sci-fi, modern clothing, firearms, guns, plastic toy look, chibi, anime big eyes, text, logo, watermark, UI, multiple characters, pedestal, base, ground plane, scenery background, floating disconnected parts, extra limbs, fused fingers, holes in mesh, blurry texture, harsh baked shadows, neon cyberpunk colors, glossy clearcoat
```

**Rig ekleri:**

| Kod | Ek | Kullanım |
|---|---|---|
| `{RIG_H}` | `standing in A-pose, arms 45 degrees down, feet shoulder width, symmetrical, full body visible, weapon separate or held loosely` | İnsansı: Mixamo/Meshy/Tripo auto-rig |
| `{RIG_Q}` | `four-legged animal in neutral standing pose, legs straight, tail straight back, mouth closed` | Dört ayaklı: auto-rig desteği [Doğrulanmadı] |
| `{RIG_S}` | `eight-legged spider in neutral pose, legs evenly spread, body level` | Örümcek: elle rig ya da Fab'dan hazır rig |
| `{RIG_F}` | `floating humanoid upper body in A-pose, lower body fades into tattered cloth wisps, no legs` | Süzülen: insansı rig, bacak kemikleri gizli |

---

## 3. Teknik şartname (öneri, hepsi [Tahmin])

| Konu | Öneri |
|---|---|
| Poligon (tri) | Oyuncu sınıfı 18-25k · NPC 10-15k · normal mob 8-15k · elit 15-30k · silah 1-3k · prop 0.5-5k |
| Doku | Oyuncu/elit 2048² · normal mob/NPC 1024²-2048² · silah/prop 512²-1024². Albedo + Normal + ORM (R=AO, G=Roughness, B=Metallic) |
| LOD | LOD0 + UE otomatik LOD1 (%50) / LOD2 (%25). Elit ve oyuncuda LOD2'yi elle kontrol et |
| Poz | İnsansı: **A-pose** (Manny/Quinn A-pose'da). Mixamo T-pose da kabul eder; retarget'tan önce IK Retargeter'da pozu eşitle |
| Mesh | Karakter başına tek skeletal mesh, tek ya da en fazla iki malzeme yuvası. Silah ayrı static mesh (soket ile takılır) |
| Ölçek | 1 UU = 1 cm. İnsan ≈ 180 cm. **Mob'u temel boyda modelle**: kod `Scale` alanıyla 0.8-2.3 arası büyütüyor (`MMOMobArchetype.cpp`). Elitleri modelde büyütme, yoksa çift büyür |
| Pivot / yön | Pivot ayak tabanında. Yön Manny ile aynı olsun: mevcut Warrok kurulumu `MeshOffsetZ=-89`, `MeshYaw=-90`, `MeshScaleMultiplier=1` ile Manny hizasında çalışıyor. Yeni mesh'ler bu üç değerle uyumlu gelirse kodda ayar gerekmez |
| İskelet | İnsansı moblar ve sınıflar için hedef: UE Manny iskeleti (`SK_Mannequin`). Mixamo çıktısı `mixamorig:Hips`, `mixamorig:Spine` adlarını, Meshy/Tripo kendi adlarını kullanır. Bu yüzden IK Rig + IK Retargeter ile Manny'ye retarget et; kemikleri elle yeniden adlandırma. Aynı ailedeki moblar **tek iskeleti paylaşsın** (ör. bütün `FAM_ORC` ve `FAM_BRUTE` Warrok iskeletinde) ki animasyonlar ortak kullanılsın |
| Adlandırma | `SK_` skeletal mesh, `SM_` static mesh, `T_<Ad>_D/_N/_ORM` doku, `M_`/`MI_` malzeme, `AS_<Ad>_<Klip>` animasyon, `ABP_` anim blueprint, `NS_` Niagara sistemi, `NE_` emitter. Mob için `SK_Mob_<ArchetypeId>` (ör. `SK_Mob_FrostWraith`), NPC için `SK_Npc_<Ad>` (ör. `SK_Npc_Merek`), sınıf için `SK_Pc_<Class>_<M/F>` |
| FBX export | FBX 2020, birim cm, Y-up/Z-up dönüşümünü UE import'a bırak ("Convert Scene" açık), smoothing groups açık, tangent'lar dahil, animasyonda "Bake Animation" ve 30 fps, root kemik adı `root`. Mixamo'dan "Without Skin" ile yalnızca klip indir (mesh tekrar gelmesin) |
| Malzeme notu | NPC'ler ve Warrok olmayan moblar şu an Manny + `Tint`/`Color` malzeme parametresiyle renkleniyor (`MMONpcBase.cpp`, arketip `Tint`). Yeni dokulu mesh'lerde bu tint dokuyu boyayabilir. Geliştirici tint'i kapatmalı ya da çarpanı beyaz yapmalı [Doğrulanmadı] |

---

## 4. Oyuncu sınıfları

Kod adları (`MMOSkillTypes.h`): `Warrior` = **Savasci**, `Archer` = **Okcu**, `Mage` = **Buyucu**, `Priest` = **Rahip**.
Her sınıf için erkek (Manny) ve kadın (Quinn) varyantı üret, prompta `male` ya da `female` ekle.
**Not:** Eşya veritabanında silah olarak yalnızca kılıç ve balta var (`MMOItemDatabase.cpp`). Yay, asa ve topuz
henüz eşya olarak tanımlı değil. Aşağıdakiler sınıfın **varsayılan görünüm silahı**; eşya olup olmayacağına
geliştirici karar verir.

**Savaşçı:** Kalkanlı ağır yakın dövüşçü (Kalkan Vuruşu, Savurma, Demir Deri).
```
{STYLE} {RIG_H} male human warrior of a frontier fortress, battered steel half-plate over chainmail and a padded gambeson, round iron-rimmed wooden shield on the back, one-handed broadsword at the hip, short cape in faded red wool, scarred determined face, sturdy boots, accent: dull crimson
```

**Okçu:** Hafif, çevik menzilli avcı (Gölge Adımı, Zehirli Ok, Çivili Tuzak).
```
{STYLE} {RIG_H} female human ranger, fitted dark leather jerkin with hood and half-cape, bracer on the bow arm, quiver of fletched arrows on the back, recurve longbow held loosely, small trap pouches and a skinning knife on the belt, light soft boots, alert lean build, accent: forest green
```

**Büyücü:** Ateş, buz ve arkane büyüleri (Ateş Topu, Buz Oku, Arkane Patlama).
```
{STYLE} {RIG_H} male human battle mage, layered long robe with a leather corset belt and pouches, high collar, rune-stitched hem, tall wooden staff topped with a cracked crystal, spellbook chained to the hip, sharp focused eyes, accent: arcane blue
```

**Rahip:** Kutsal hasar ve şifa (Kutsal Işık, Ceza, Cennetin Eli). Lore'daki Gümüş Dal Tarikatı'na bağlanması [Yorum].
```
{STYLE} {RIG_H} female human battle priestess of a temple order, white and silver robes over light scale armor, embroidered silver tree branch emblem on the chest, one-handed flanged mace, small holy tome on a chain, calm compassionate face, accent: warm gold light
```

### Ekipman kademeleri (şablon)

Eşya yuvaları: silah, zırh, kask, bot (`MMOItemDatabase.cpp`). Şablon:
```
{STYLE} single {SLOT} game item for a {CLASS} character, {TIER_MATERIAL}, isolated on neutral background, no character
```
`{SLOT}` = `one-handed sword` / `two-handed great sword` / `battle axe` / `chest armor` / `helmet` / `pair of boots`

| Kademe (Lv) | Kod önekleri | `{TIER_MATERIAL}` |
|---|---|---|
| Başlangıç (1-9) | ShortSword, LongSword, LeatherArmor, LeatherCap, LeatherBoots, IronHelmet, IronBoots, ChainMail | `plain iron and stitched brown leather, simple and worn` |
| Çelik / Plaka (≈5-9) | SteelHelmet, SteelBoots, PlateArmor, BattleAxe, GreatSword | `polished steel plates with bronze rivets, practical soldier gear` |
| Ejder | DragonBlade, DragonPlate, DragonHelm, DragonBoots | `dark steel shaped like overlapping dragon scales, red enamel accents` |
| Mithril (10) | Mithril* | `silver-white feather-light metal with a faint moonlight sheen, knightly bastion style` |
| Adamantin (16) | Adamantine* | `black-red forged metal still glowing faintly at the edges, heavy and unbreakable` |
| Obsidyen (25) | Obsidian* | `glossy black volcanic glass inlays on dark iron, razor edges, violet sheen` |
| Alacakaranlık (33) | Dusk* | `deep twilight-blue steel with soft violet edge glow, shadowy leaf motifs` |
| Kış (42) | Winter* | `frost-tempered pale steel with ice-blue rime and white fur trim` |
| Buzbağlı (51) | Frostbound* | `oath-sealed steel wrapped in blue ice bands, carved vow runes` |
| Yarık (58) | Rift* | `dark metal with glowing purple fissures as if cracked open` |
| Kül (66) | Ashen* | `ash-grey metal with smoldering coal veins and frost patches together` |
| Uçurum (72) | Abyss* | `light-absorbing matte black metal with faint indigo outline, organic grown shapes` |
| Taht (78) | Throne* (satıcıda satılmaz; Naib/Lv75+ düşüşü, `Q_AbyssRegent` ödülü) | `obsidian-black regal armor with hollow-crown motifs, indigo edge glow, throne-room majesty` |

---

## 5. Moblar

**Kodda sayılan arketip: 59** (`Table.Add(MakeMob(` sayısı; 2026-10-10). **46 normal + 13 elit.** (#52-59 sekiz yeni Lv47-78 normal mob.) `PROGRESSION_PLAN.md`'deki
"48 arketip" sayısı Çözülme Yolu'nun üç mobu eklenmeden önceki duruma ait.

### Aile blokları (aynı ailedeki her mob bu bloğu kullanır)

| Kod | Blok (prompta eklenir) | Rig | Not |
|---|---|---|---|
| `{FAM_GOBLIN}` | `small hunched goblin, grey-green skin, long ears, scrap leather and rope bindings, crude rusty weapon` | H | |
| `{FAM_WOLF}` | `lean wolf-like canine predator, shaggy fur, long muzzle, visible fangs` | Q | Mixamo'da dört ayaklı yok (`MapOptions.md`) |
| `{FAM_ORC}` | `massive orc brute, broad hunched shoulders, lower tusks, tribal war paint, crude armor plates strapped with leather, heavy two-handed axe or cleaver` | H | Kodda Mixamo **Warrok** mesh'i + 4 anim kullanıyor |
| `{FAM_BRUTE}` | `towering heavy humanoid brute with orc-like body proportions (same bulk and stance as an orc), non-orc head, slow and immovable` | H | Kodda Warrok kullanan ama Lore'da ork denmeyen moblar; Warrok iskeletinde kalsın ki anim ortak olsun [Yorum] |
| `{FAM_HUMAN}` | `human or once-human fighter, realistic medieval gear` | H | |
| `{FAM_WRAITH}` | `spectral floating ghost of the dead, gaunt face with glowing eyes, tattered robes dissolving into wisps, semi-transparent emissive accents` | F | |
| `{FAM_GOLEM}` | `hulking elemental construct, blocky massive limbs, no mouth, glowing eye slits and glowing cracks` | H | Kadim Golem'in akrabaları (Lore) |
| `{FAM_SPIDER}` | `giant spider, segmented carapace, hairy legs, multiple glowing eyes, dripping fangs` | S | |
| `{FAM_STALKER}` | `lean low-slung predator creature, elongated limbs, spiny ridge along the spine, fast hunting stance` | Q | Lore yalnızca "hızlı, sinsi yaratıklar" diyor; vücut tipi [Yorum] |
| `{FAM_TROLL}` | `huge troll, long arms, hunched back, moss and lichen on thick hide, small eyes, huge hands` | H | |

### Arketip tablosu

Harita/konum: `MMOGameMode.cpp`. **S** = Summer_scene, **W** = Winter_scene. Lv1-5 ana spawner'da, Lv6-7 ikinci,
Lv8-12 üçüncü, Lv16-22 dördüncü bölge spawner'ında çıkıyor. Satır promptu = `{STYLE} {FAM_x} {RIG_x}` + aşağıdaki metin.

| # | Id | Ad | Lv | Elit | Bölge | Açıklama (TR) | Satır promptu |
|---|---|---|---|---|---|---|---|
| 1 | Goblin | Goblin | 1 | - | S · Akçakule çevresi | Çakılı vadilerde pusu kuran küçük yağmacı | `skinny goblin ambusher with a chipped dagger and a stolen pot helmet, accent: moss green` |
| 2 | WolfPup | Kurt | 2 | - | S · kuzey tepeleri | Sürüyle avlanan kurt | `grey wolf, medium size, ragged fur, accent: wool grey` |
| 3 | Orc | Ork | 3 | - | S · doğu geçitleri | İri, disiplinsiz ork | `basic orc grunt, rust-red war paint, crude cleaver, accent: dull red` |
| 4 | Bandit | Haydut | 4 | - | S · ticaret yolu | Kırık Zincir haydudu, zehirli hançer | `human highway bandit, hooded leather coat, broken chain worn as a sash, poisoned curved dagger with green drip, accent: rust brown` |
| 5 | Troll | Trol | 5 | - | S · kıyılar | Nadir ama devasa yaratık | `shore troll with barnacles and wet moss, accent: swamp green` |
| 6 | OrcWarrior | Ork Savaşçısı | 6 | - | S · Karakaya Geçidi | Ordunun baltalı bel kemiği | `orc soldier, heavy battle axe, iron shoulder plate, black rock dust, accent: dull red on charcoal` |
| 7 | DireWolf | Korku Kurdu | 6 | - | S · Karakaya kayalıkları | İri kurt, kor gibi yanan gözler | `huge dire wolf, dark grey fur, eyes glowing like embers, accent: ember orange eyes` |
| 8 | Warlock | Kara Büyücü | 7 | - | S · Karakaya Geçidi | Fısıltıya kanan sapkın büyücü | `corrupted human sorcerer, tattered black-violet robes, pale veined skin, bone staff with a poisonous green-violet orb, accent: sickly violet` |
| 9 | FrostWraith | Buz Hayaleti | 8 | - | S · Ayazdiş Buzulu | Uzun Kış'ta donanların hayaleti | `ghost of a frozen villager, icicles on the hair, frost mist, accent: pale ice blue` |
| 10 | CursedKnight | Lanetli Şövalye | 9 | - | S · Ayazdiş harabeleri | Fısıltıyla kalkan düşmüş şövalye | `undead bastion knight in heavy frost-rimed plate, cracked visor with cold blue glow, longsword and tower shield, accent: frost blue` |
| 11 | VenomSpider | Zehir Örümceği | 10 | - | S · Ayazdiş oyukları | Örümcek Ana'nın soyu | `medium venom spider, green-mottled carapace, swollen venom sac, accent: toxic green` |
| 12 | OrcBerserker | Ork Berserk | 12 | - | S · kuzey ork kampı | Reisini kaybetmiş gözü dönmüş ork | `shirtless berserker orc, fur cloak with frost, two hand axes, bloodshot eyes, accent: red on frost white` |
| 13 | ThawStalker | Çözülme Avcısı | 13 | - | S · Çözülme Yolu | Eriyen buzda avlanan zehirli avcı | `wet sleek stalker with half-melted ice plates on its back, dripping teal venom, accent: teal` |
| 14 | CrustWarden | Kabuk Bekçisi | 14 | - | S · Çözülme Yolu | Yarı buz yarı cüruf kabuklu bekçi | `construct made half of cracked ice and half of cooling slag, split down the middle, accent: teal and slag brown` |
| 15 | ScorchedRaider | Kavrulmuş Akıncı | 15 | - | S · Çözülme Yolu | Derisi kavrulmuş ork akıncısı | `orc raider with scorched cracked skin, smoking leather, accent: ember orange` |
| 16 | EmberHound | Kor Tazısı | 16 | - | S · Alev Vadisi | Tüyleri kor, nefesi duman | `fire hound with fur made of glowing embers, smoke from the nostrils, accent: ember orange` |
| 17 | MagmaGolem | Magma Golemi | 18 | - | S · Alev Vadisi lav gölü | Lav gölünün yavaş bekçisi | `magma golem of dark basalt with flowing lava seams, accent: lava red` |
| 18 | AshOrc | Kül Orku | 20 | - | S · Alev Vadisi kül ovası | Yeni ork kabilesi, dumanlı gözler | `ash-caked orc with smoky grey eyes, ash cloud around the shoulders, accent: ash grey` |
| 19 | InfernalWraith | Cehennem Hayaleti | 22 | - | S · Alev Vadisi dibi | Yanarak ölenlerin gölgesi | `burning ghost, robes made of flame wisps, charred face, accent: crimson fire` |
| 20 | ObsidianStalker | Obsidyen Avcısı | 25 | - | S · Obsidyen Sırtları | Kara camda sinsi, zehirli avcı | `stalker with glossy obsidian shards along the spine, accent: violet sheen on black` |
| 21 | ObsidianSentinel | Obsidyen Muhafızı | 27 | - | S · Obsidyen Sırtları | Yavaş, ağır bekçi | `heavy sentinel construct of black volcanic glass slabs, accent: violet sheen on black` |
| 22 | ObsidianRaider | Obsidyen Akıncısı | 29 | - | S · Obsidyen Sırtları | Kara cam kalkanlı ork | `orc raider carrying a shield of black obsidian glass, accent: violet sheen on black` |
| 23 | DuskProwler | Alacakaranlık Avcısı | 31 | - | S · Alacakaranlık Korusu | Gün ile gece arasında avlanan | `twilight prowler with shadowy fur fading into mist at the edges, accent: twilight violet` |
| 24 | DuskGuardian | Alacakaranlık Muhafızı | 33 | - | S · Alacakaranlık Korusu | Korunun yollarını tutan ağır bekçi | `guardian construct of ancient bark and mossy stone with dim violet runes, accent: slate blue` |
| 25 | DuskReaver | Alacakaranlık Yağmacısı | 36 | - | S · Alacakaranlık Korusu | Koruya sığınmış iri ork | `orc reaver in dark leather hood, twilight-stained blades, accent: twilight violet` |
| 26 | DuskWraith | Alacakaranlık Hayaleti | 39 | - | S · Alacakaranlık Korusu | Korunun sisinde süzülen gölge | `shadow ghost blending into dusk mist, soft violet eyes, accent: twilight violet` |
| 27 | WinterStalker | Kış İzcisi | 42 | - | W · Kış Diyarı | Karda iz bırakmadan koşan izci [Yorum: insansı] | `pale-skinned northern scout in white fur and frosted leather, bone spear, accent: frost white` |
| 28 | WinterGuardian | Kış Muhafızı | 44 | - | W · Kış Diyarı | Geçitleri tutan sabırlı bekçi | `guardian construct of packed blue ice and snow, accent: ice blue` |
| 29 | WinterReaver | Kış Yağmacısı | 46 | - | W · Kış Diyarı | Fırtınadan çıkan iri ork | `frost orc reaver with snow-crusted fur armor, icicle beard, accent: ice blue` |
| 30 | WinterWraith | Kış Hayaleti | 48 | - | W · Kış Diyarı | Uzun Kış'ın en eski ölüleri | `ancient frozen ghost with a crown of ice shards, accent: frost white` |
| 31 | FrostboundScout | Buzbağlı Gözcü | 51 | - | W · Buzbağlı Sınır (doğu) | Yeminli, hızlı sınır gözcüsü | `oath-bound frontier scout, light armor sealed with blue ice bands, hooded, frozen eyes, accent: frost blue` |
| 32 | FrostboundSentinel | Buzbağlı Nöbetçi | 55 | - | W · Buzbağlı Sınır (kuzey) | Ölümden sonra da nöbet tutan iri muhafız | `undead oath-bound sentinel giant in ice-sealed heavy plate, carved vow runes, accent: frost blue` |
| 33 | RiftSeeker | Yarık Arayıcısı | 58 | - | S · dış yamaçlar | Yarıktan ilk çıkan aç yaratık | `eyeless seeker creature sniffing the ground, skin cracked with purple light, accent: rift purple` |
| 34 | RiftKeeper | Yarık Bekçisi | 62 | - | S · dış yamaçlar | Yarığı açık tutmaya yeminli iri bekçi | `rift keeper brute with purple crystal growths on the back and arms, accent: rift purple` |
| 35 | AshVanguard | Kül Öncüsü | 66 | - | W · Kül Hattı (güney) | Hattın önünde koşan akıncı [Yorum: insansı] | `ash-cloaked fast raider, face wrapped in scorched cloth, glowing coal blade, accent: coal red on ash grey` |
| 36 | AshBinder | Kül Bağlayıcı | 68 | - | W · Kül Hattı | Külü buza bağlayan iri yaratık | `binder brute wrapped in chains half-frozen and half-smoldering, accent: ash grey with coal red and frost` |
| 37 | AbyssStrider | Uçurum Gezgini | 72 | - | W · Uçurum Yaylası (kuzeybatı) | Uçurum kenarında durmadan dolaşan | `long-legged strider creature, matte black hide that absorbs light, faint indigo outline, accent: deep indigo` |
| 38 | AbyssGuardian | Uçurum Koruyucusu | 76 | - | W · Uçurum Yaylası (kuzeydoğu) | Uçuruma inen yolu tutan bekçi | `abyss guardian brute, smooth light-absorbing armor grown from its body, accent: deep indigo` |
| 39 | SpiderQueen | Örümcek Ana | 5 | Elit | S · Solgun Ormanlar | Ağ kuran dev ana örümcek | `huge spider matriarch, egg sacs on the abdomen, silk strands, purple-black carapace, accent: dark violet` |
| 40 | OrcWarlord | Ork Reisi | 6 | Elit | S · Karakaya | Fısıltının ilk büyük kılıcı | `orc warlord, horned iron helmet, skull trophies, war banner pole on the back, giant cleaver, accent: blood red` |
| 41 | StoneGolem | Kadim Golem | 7 | Elit | S · Karakaya | Uyanmış kadim taş bekçi | `ancient stone golem of carved black-veined rock, worn guardian runes, accent: dull red rune glow` |
| 42 | ShadowAssassin | Gölge Suikastçı | 7 | Elit | S · patikalar | Karanlıkla kaynaşan tek kişilik ölüm | `slender assassin in layered black wraps, masked face, twin poisoned daggers, accent: smoky black with green poison` |
| 43 | FrostLich | Buz Lordu | 13 | Elit | S · Ayazdiş kalbi | Buz tahtının kadim lordu | `ancient ice lord, crown of frozen spikes, long frost-rimed robes, staff of ice, skeletal hands, accent: ice blue glow` |
| 44 | MagmaTyrant | Magma Tiranı | 24 | Elit | S · Alev Vadisi | Ateş dağının kalbinden doğan dev | `colossal magma tyrant with a molten crown of basalt, lava pouring from the chest, accent: lava red` |
| 45 | ObsidianWarlord | Obsidyen Reisi | 30 | Elit | S · Obsidyen Sırtları | Ork Reisi'nin kan bağı | `orc warlord in obsidian-plated armor, black glass greataxe, accent: violet sheen on black` |
| 46 | DuskOverlord | Alacakaranlık Hükümdarı | 40 | Elit | S · Koru kalbi | Korunun iri efendisi | `twilight overlord brute, antler-like crown of dead branches, cloak of shadow leaves, accent: twilight violet` |
| 47 | WinterSovereign | Kış Hükümdarı | 50 | Elit | W · Kış Diyarı | Uzun Kış'ın asıl sahibi | `winter sovereign, towering regal figure in ice armor, storm swirling around the crown, accent: ice blue and white` |
| 48 | FrostboundWarden | Buzbağlı Muhafız | 57 | Elit | W · Buz Kapısı | Kapıya sadık son bekçi | `last warden of the ice gate, immense oath-bound knight, gate key chained to the belt, ice-sealed tower shield, accent: deep frost blue` |
| 49 | RiftHerald | Yarık Habercisi | 65 | Elit | S · dış yamaçlar | Karşı yakadan söz taşıyan haberci | `herald figure in long robes split by glowing purple fissures, faceless hood, carries a cracked horn, accent: rift purple` |
| 50 | AshTyrant | Kül Tiranı | 70 | Elit | W · Kül Hattı | Magma Tiranı'nın külünden doğan | `ash tyrant, burnt-out colossus of grey ash and cooled basalt, dim coal glow inside cracks, frost on the shoulders, accent: coal red` |
| 52 | WinterWolf | Don Kurdu | 47 | - | W · Kış Diyarı (batı cebi) | Hükümdar'ın fırtınadaki sürüsü, hızlı akıncı | `large pale-furred winter wolf with frost-crusted mane and icicle fangs, breath steam, accent: frost white and grey` |
| 53 | WinterBehemoth | Buz Devi | 49 | - | W · Kış Diyarı (doğu cebi) | Karın altında uyuyan buzdan dev, yavaş ve öfkeli | `towering ice behemoth, body of packed blue ice slabs with snow on the shoulders, glowing pale eye slits, accent: ice blue` |
| 54 | FrostboundSworn | Buzbağlı Yeminli | 53 | - | W · Buzbağlı Sınır (güneydoğu cep) | Gözcü ile Nöbetçi arası, yeminini bozmayan dengeli asker | `oath-bound frontier soldier in mid-weight ice-sealed plate, carved vow runes, frozen helm, accent: frost blue` |
| 55 | RiftWhisperer | Yarık Fısıldayanı | 60 | - | S · dış yamaçlar (Arayıcı cebi) | Yarığın ağzında fısıldaşan hızlı gölge | `hunched whispering shadow creature, cracked skin leaking purple light, too many thin fingers, accent: rift purple` |
| 56 | RiftWalker | Yarık Yürüyeni | 64 | - | S · dış yamaçlar (Bekçi cebi) | Yarığın derininden yürüyerek çıkan ağır yaratık | `slow rift walker brute, crystalline purple growths over charred plates, dragging heavy arms, accent: rift purple` |
| 57 | AshMarcher | Kül Yürüyeni | 69 | - | W · Kül Hattı (güneybatı cep) | Hat çözüldükten sonra kayan ağır sürü | `ash marcher brute, grey ash-caked armor plates with coal glow in the seams, frost patches on the shoulders, accent: coal red on ash grey` |
| 58 | AbyssEcho | Uçurum Yankısı | 74 | - | W · Uçurum Yaylası (batı cep) | Uçurumdan yükselen, geri dönen her yankıdan bir şey eksilten gölge | `hovering echo figure with a blurred doubled outline, matte black with faint indigo rim, soundless, accent: deep indigo` |
| 59 | AbyssEnvoy | Naibin Elçisi | 78 | - | W · Uçurum Yaylası (güney cep) | Naib'in sesini yeryüzünde taşıyan iri elçi | `abyss envoy brute in smooth light-absorbing armor with a hollow crown motif, indigo-edged cloak, accent: deep indigo` |
| 51 | AbyssRegent | Uçurum Naibi | 80 | Elit | W · Uçurum Yaylası (doğu) | Unutulmuş gücün yeryüzündeki sesi | `abyss regent, tall shadow figure on an empty throne motif, robes of light-absorbing black with indigo edges, faceless with a hollow crown, accent: deep indigo` |

**Aile eşleşmesi (52-59 eklendi):** GOBLIN: 1 · WOLF: 2, 7, 16, 52 · ORC: 3, 6, 12, 15, 18, 22, 25, 29, 40, 45 · BRUTE: 32, 34, 36, 38, 46, 53, 54, 56, 57, 59 ·
HUMAN: 4, 8, 10, 27, 31, 35, 42 · WRAITH: 9, 19, 26, 30, 43, 49, 51, 55, 58 · GOLEM: 14, 17, 21, 24, 28, 41, 44, 50 ·
SPIDER: 11, 39 · STALKER: 13, 20, 23, 33, 37 · TROLL: 5 · Ailesiz elit insansı: 47, 48 (`{FAM_HUMAN}` ile kullan).
Toplam: 1+4+10+10+7+9+8+2+5+1+2 = **59**.

**Kodla uyum (2026-10-10: +5 yeni Warrok kullanıcısı: WinterBehemoth, FrostboundSworn, RiftWalker, AshMarcher, AbyssEnvoy = 20):** Kod şu 15 eski arketipte ayrıca Warrok mesh'ini kullanıyor: Orc, OrcWarrior, OrcBerserker, ScorchedRaider,
AshOrc, ObsidianRaider, DuskReaver, WinterReaver, FrostboundSentinel, RiftKeeper, AshBinder, AbyssGuardian,
OrcWarlord, ObsidianWarlord, DuskOverlord. Bunların 10'u ORC, 5'i BRUTE ailesinde. Yeni modeller Warrok iskeletine
rig'lenirse mevcut 4 Warrok animasyonu (Idle/Run/Attack/Death) hemen çalışır. Geri kalan 39 arketip şu an Manny +
renk ile görünüyor.

---

## 6. NPC'ler

Kaynak: `World/*NPC.cpp`, `MMOGameMode.cpp`, `NpcDialogue.md`, `Lore.md`. Summer hub'ında 5 NPC var. Winter hub'ında
4 NPC var (Satıcı, Görevci, Şifacı, Depocu; Kael yok). Aynı sınıflar olduğu için **aynı model** kullanılır. Şu an
hepsi Manny + `ABP_Unarmed` + renk ile görünüyor (`MMONpcBase.cpp`). Kış için kalın giysili varyant isteğe bağlı.

**Satıcı Merek (vendor):** Aksi ama dürüst silah satıcısı, paraya düşkün.
```
{STYLE} {RIG_H} gruff middle-aged human weapon merchant, thick forearms, soot-stained leather apron over a rolled-sleeve tunic, heavy coin purse and a ring of keys on the belt, short trimmed beard, permanent frown, a sheathed sword tucked under one arm, accent: dark bronze
```

**Yaşlı Doran (quest giver):** Bilge, sakin, bilmece gibi konuşur; "terazi" sözü onun simgesi.
```
{STYLE} {RIG_H} very old wise human sage, long grey beard, travel-worn layered wool robe with a hood down, gnarled walking staff, small brass balance scale hanging from the belt, scrolls tucked in a satchel, kind tired eyes, slightly stooped, accent: warm amber
```

**Muhafız Kael (talker, yalnız Summer):** Kapı muhafızı; kısa, disiplinli. Kül Muhafızları'ndan olması [Yorum].
```
{STYLE} {RIG_H} disciplined human gate guard, open-faced steel half-helm, chainmail under an ash-grey tabard with a white tower emblem, kite shield and spear, stern weathered face, upright posture, accent: ash grey and white
```

**Şifacı Lyra (healer):** Gümüş Dal Tarikatı'nın şefkatli tapınak şifacısı.
```
{STYLE} {RIG_H} gentle young human temple healer, flowing white and pale silver robes, embroidered silver branch emblem, herb pouches and small potion vials on the belt, bandage roll, soft kind face, long braided hair, accent: soft silver and pale green
```

**Depocu Rana (warehouse):** Sakin, düzenli ambar kâtibi; büyükannesinden kalan defter.
```
{STYLE} {RIG_H} calm orderly human warehouse clerk woman, practical dark wool dress with an apron, thick leather-bound ledger under one arm, quill behind the ear, large iron key ring at the hip, reading spectacles on a cord, tidy bun, accent: deep teal
```

---

## 7. Animasyon listesi

Kodda mob için **dört** animasyon yuvası var: `AnimIdlePath`, `AnimRunPath`, `AnimAttackPath`, `AnimDeathPath`.
"Hit" ve "Walk" kodda yok; aşağıda **[öneri]** olarak işaretli. Klip adları Mixamo'da arama terimi olarak verildi
[Doğrulanmadı: kesin adlar sitede değişebilir]. Meshy animasyon kütüphanesinde benzer adlarla ara [Doğrulanmadı].

### Ortak (oyuncu sınıfları)

| Klip | Kullanım | Mixamo arama |
|---|---|---|
| Idle / Combat Idle | bekleme | `Idle`, `Sword And Shield Idle`, `Standing Idle` |
| Walk / Run | tıkla-yürü hareketi | `Walking`, `Running`, `Sword And Shield Run` |
| Jump | şablondaki zıplama | `Jump` |
| Hit React [öneri] | hasar alınca | `Hit Reaction`, `Standing React Small From Front` |
| Death | `OnCharacterDied` | `Dying`, `Death From The Front` |
| Revive [öneri] | `Revive` sonrası kalkış | `Getting Up`, `Stand Up` |

### Beceri türü → klip (kodda `EMMOSkillEffect`)

| Etki türü | Sınıf | Beceriler (kod) | Klip önerisi |
|---|---|---|---|
| MeleeDamage | Savaşçı | PowerStrike, MortalBlow, ShatterBlow, CrushingBlow, Execute, Rampage, Skullsplitter, WarCyclone, TitanWrath | `Sword And Shield Slash`, `Sword And Shield Attack`, ağır olanlar için `Great Sword Slash` / `Standing Melee Attack Downward` |
| MeleeDamage (kalkan) | Savaşçı | ShieldBash | `Sword And Shield Bash` |
| AoESelf | Savaşçı | Cleave, BloodStorm, TempestSlash, Earthshaker, WarDeluge, DoomsdayBlow | `Standing Melee Attack 360`, `Sword And Shield Slash` (dönüşlü); Earthshaker/Doomsday için `Standing Melee Attack Downward` + yere vuruş |
| HealSelf / BuffSelf | Savaşçı | BloodDrain, LifeReap, BattleShout, BloodLord, IronSkin, SteelArmor, Unbreakable | `Battlecry`, `Sword And Shield Power Up` |
| RangedDamage | Okçu | QuickShot, PowerShot, PoisonArrow, PiercingShot, EagleEye, Snipe, DragonArrow, DeathMark, Impaler, HuntersReckoning | `Standing Draw Arrow`, `Standing Aim Recoil`, `Standing Aim Overdraw` |
| AoETarget | Okçu | ArrowRain, Volley, ThornVolley, SpikeTrap, PoisonCloud, ArrowTempest | yukarı nişan: `Standing Aim Overdraw` (gövde yukarı açılı); SpikeTrap/PoisonCloud için `Throw` |
| BuffSelf | Okçu | Sprint, ShadowStep, WindsBreath | `Standing Taunt Battlecry` ya da kısa `Power Up` |
| RangedDamage | Büyücü | IceBolt, Fireball, ChainLightning, IceLance, Thunderclap, IceChain, Disintegrate, Starfall | `Standing 1H Magic Attack 01`, `Standing 1H Cast Spell 01` |
| AoETarget | Büyücü | Meteor, FlameStorm, ArcaneBlast, Inferno, ArcaneBurst, ManaStorm, MeteorShower, AbsoluteAnnihilation | `Standing 2H Magic Area Attack 01`, `Standing 2H Cast Spell 01` |
| AoESelf | Büyücü | FrostNova, FreezingTempest | `Standing 2H Magic Area Attack 02` (yere doğru) |
| RangedDamage | Rahip | Smite, HolyFlame, Judgement, Retribution, AngelicBlade | `Standing 1H Magic Attack 01` |
| AoESelf / AoETarget | Rahip | HolyBurst, WrathOfAngels, RadiantNova, AngelicStorm, DivineJudgment | `Standing 2H Magic Area Attack 01` |
| HealSelf / HealTarget | Rahip | HolyLight, GreaterHeal, DivineRestoration, Mending, HolyRain, LifeBond, SalvationLight, HandOfHeaven | `Standing 2H Cast Spell 01`, `Praying` (uzun şifa) |
| BuffSelf | Rahip | Renew, HolyShield, ResiliencePrayer | `Praying`, `Standing 1H Cast Spell 01` |

Sınıf başına en az set: 1 combat idle, 1 run, 2-3 saldırı, 1 alan, 1 buff/cast, hit, death. Kalan beceriler bu
klipleri paylaşabilir. Farkı efekt (bölüm 8) taşır.

### Mob aileleri

| Aile | Idle | Run | Attack | Death | [öneri] Walk/Hit |
|---|---|---|---|---|---|
| ORC / BRUTE | mevcut Warrok_Idle | Warrok_Run | Warrok_Attack | Warrok_Death | `Mutant Walking`, `Mutant Hit` |
| GOBLIN, HUMAN | `Idle` | `Running` | `Sword And Shield Slash` / `Stabbing` (Bandit, ShadowAssassin) / `Standing 1H Magic Attack 01` (Warlock) | `Dying` | `Walking`, `Hit Reaction` |
| GOLEM, TROLL | `Mutant Idle` | `Mutant Run` (yavaş oynat) | `Mutant Swiping`, `Mutant Punch` | `Mutant Dying` | `Mutant Walking` |
| WRAITH | `Floating` / `Idle` (üst gövde) | aynı klip, kod yer değiştirir | `Zombie Attack`, `Standing 2H Magic Attack 01` | `Dying` | — |
| WOLF, STALKER | Mixamo'da yok → Fab dört ayaklı paket ya da Meshy/Tripo hayvan rig [Doğrulanmadı] | — | ısırık/atılma | yana devrilme | — |
| SPIDER | Fab'dan rig'li örümcek paketi önerilir | — | ön bacak saplama | ters dönme | — |

**Cascadeur notu (dövüş klipleri):** Saldırı ve elit kliplerinde Mixamo'nun "yüzen" vuruş hissi sorun olursa
Cascadeur'da: (1) Mixamo klibini ya da Manny'yi içe al, (2) 3-5 anahtar poz ver (hazırlık → vuruş → takip →
dönüş), (3) AutoPhysics ile ağırlık ve denge ekle, (4) vuruş karesini koddaki hasar anına yakın tut (mob vuruşu
sunucuda anlık uygulanıyor, kliple senkron değil [Doğrulanmadı]), (5) aynı iskeletle FBX export, kök hareketi
kapalı (karakter hareketi kodda). Önce elitlerin Attack ve Death klipleriyle başla, en çok fark orada görünür.

---

## 8. Efektler (Niagara)

Koddaki **93 beceri** (2026-10-10): Savaşçı 26, Okçu 22, Büyücü 21, Rahip 24. Aşağıdaki kapsama tablosu 81 becerilik eski sayıma aittir; **12 yeni Lv52/64/75 beceri mevcut efekt gruplarını yeniden kullanır** (bkz. tablonun altındaki not). Dokümanlardaki "73" sayısı Lv31/39 halkaları
eklenmeden önceki duruma ait. Uygulanan durum efektleri: `Poison`, `Slow`, `Rage`, `RageII`, `IronSkin`, `IronSkinII`,
`IronSkinIII`, `Haste`, `HasteII`, `HasteIII`, `Regen`, `RegenII`.

Niagara AI aracı belirsiz olduğu için "prompt/tarif" sütunu hem metin-üretici (doku/flipbook) için prompt hem de
elle kurulum tarifidir. Doku promptu = `{STYLE_FX}` + metin.

**{STYLE_FX}**
```
stylized hand-painted game VFX texture, soft painted shapes, readable from a distance, on pure black background, seamless, no text
```

| # | Grup (NS adı) | Beceriler (kod Id) | Efekt tarifi | Doku/flipbook promptu | Fab arama |
|---|---|---|---|---|---|
| 1 | Çelik darbe (`NS_Melee_Impact`) | War_PowerStrike, War_ShieldBash, War_MortalBlow, War_CrushingBlow, War_Execute, War_Rampage, War_Skullsplitter, War_WarCyclone, War_TitanWrath, War_ShatterBlow (+Slow) | Silah ucunda kısa yay izi (ribbon), vuruşta kıvılcım + toz halkası. Güç arttıkça iz kalınlaşır; Titan Öfkesi'nde kırmızı vurgu | `white-gold sword slash arc, 4x4 flipbook` / `metal spark burst sprites` | `stylized slash`, `sword trail`, `hit impact` |
| 2 | Savaş girdabı (`NS_War_Whirl`) | War_Cleave, War_BloodStorm, War_TempestSlash, War_WarDeluge, War_DoomsdayBlow, War_Earthshaker (+Slow) | Karakter etrafında yarıçapa uygun (320-650 u) dönen kesik halkası; Earthshaker/Doomsday'de yer çatlağı decal + toz şok dalgası | `circular sword slash ring top view`, `ground crack decal stylized` | `ground slam`, `shockwave`, `whirlwind slash` |
| 3 | Kan ve öfke (`NS_War_Blood`) | War_BloodDrain, War_LifeReap, War_BattleShout (Rage), War_BloodLord (RageII) | Kan Emme: hedeften oyuncuya akan koyu kırmızı ribbon. Öfke: kısa kırmızı nara dalgası + buff süresince ayakta kırmızı buhar | `dark red energy wisps`, `red roar shockwave ring` | `blood drain`, `rage aura`, `battle cry` |
| 4 | Demir kabuk (`NS_Buff_IronSkin`) | War_IronSkin, War_SteelArmor, War_Unbreakable | Gövdede metalik fresnel parıltısı + çevrede dönen 3 küçük kalkan; I/II/III kademede parlaklık artar | `metallic shield rune glyph` | `shield buff`, `armor aura` |
| 5 | Ok izi (`NS_Arrow_Trail`) | Arc_QuickShot, Arc_PowerShot, Arc_EagleEye, Arc_Snipe, Arc_Impaler, Arc_HuntersReckoning, Arc_PiercingShot (+Slow), Arc_DragonArrow (+Slow) | Mermi arkasında ince beyaz iz, isabette kıymık patlaması. Ejder Oku'nda turuncu ejder başı biçimli iz, Avcının Sonu'nda altın iz | `thin wind streak trail`, `dragon head flame silhouette` | `arrow trail`, `projectile trail` |
| 6 | Zehir (`NS_Poison`) | Arc_PoisonArrow, Arc_DeathMark + durum `Poison` (mob vuruşları dahil) | Yeşil damla izi; hedefte süre boyunca kabarcıklar ve yeşil duman | `toxic green bubbles and drips` | `poison`, `toxic cloud` |
| 7 | Ok yağmuru (`NS_Arrow_Rain`) | Arc_ArrowRain, Arc_Volley (Poison), Arc_ThornVolley (Poison), Arc_ArrowTempest | Hedef alanda (360-620 u) yukarıdan düşen ok sağanağı + zemin dairesi; zehirli olanlarda yeşil iz | `arrow silhouettes falling top view`, `target circle decal` | `arrow rain`, `AOE indicator` |
| 8 | Tuzak ve bulut (`NS_Trap`) | Arc_SpikeTrap, Arc_PoisonCloud | Zeminden fırlayan çiviler; ardından yerde kalan yeşil zehir sisi | `wooden spike trap burst`, `low green fog` | `spike trap`, `poison fog` |
| 9 | Rüzgâr hızı (`NS_Buff_Haste`) | Arc_Sprint (Haste), Arc_ShadowStep (HasteII), Arc_WindsBreath (HasteIII) | Ayak çevresinde rüzgâr çizgileri; Gölge Adımı'nda kısa siyah duman izi | `stylized wind streaks` | `speed buff`, `wind trail` |
| 10 | Ateş (`NS_Fire`) | Mag_Fireball, Mag_Meteor, Mag_FlameStorm, Mag_Inferno, Mag_MeteorShower | Ateş topu mermisi; Meteor'da gökten düşen kaya + çarpma; Fırtına/Patlama'da alan alevi | `stylized fireball flipbook`, `flame burst`, `molten rock` | `stylized fire`, `meteor`, `fireball` |
| 11 | Buz (`NS_Ice`) | Mag_IceBolt, Mag_IceLance, Mag_IceChain, Mag_FrostNova (Slow), Mag_FreezingTempest (Slow) + durum `Slow` | Buz kristali mermisi; Nova'da yerde buz halkası; Slow'da hedefin ayağında kırağı | `ice shard crystals`, `frost ring decal`, `snow sparkle` | `ice spell`, `frost nova` |
| 12 | Yıldırım (`NS_Lightning`) | Mag_ChainLightning (Slow), Mag_Thunderclap (Slow) | Dallanan beam (Niagara Beam), çarpmada mavi-beyaz parlama | `branching lightning bolt` | `lightning`, `chain lightning` |
| 13 | Arkane (`NS_Arcane`) | Mag_ArcaneBlast, Mag_Disintegrate, Mag_Starfall, Mag_ArcaneBurst, Mag_ManaStorm, Mag_AbsoluteAnnihilation | Mavi-mor rün çemberleri ve mana parçacıkları; Yıldız Düşüşü'nde gökten yıldız ışını; Mutlak Yıkım'da büyük rün çemberi + çöküş | `arcane rune circle`, `mana particles`, `falling star beam` | `arcane magic`, `magic circle` |
| 14 | Kutsal ışık (`NS_Holy_Bolt`) | Pri_Smite, Pri_HolyFlame, Pri_Judgement, Pri_Retribution, Pri_AngelicBlade | Altın-beyaz ışık oku; Meleklerin Kılıcı'nda gökten inen ışık kılıcı | `golden holy light beam`, `glowing sword of light` | `holy`, `light beam`, `divine` |
| 15 | Kutsal alan (`NS_Holy_Area`) | Pri_HolyBurst, Pri_WrathOfAngels, Pri_RadiantNova, Pri_AngelicStorm, Pri_DivineJudgment | Zeminde altın rün çemberi + yukarıdan ışık sütunları; melek kanadı tüyü parçacıkları | `white feathers floating`, `golden ground sigil` | `holy nova`, `divine AOE` |
| 16 | Şifa (`NS_Heal`) | Pri_HolyLight, Pri_GreaterHeal, Pri_DivineRestoration, Pri_Mending, Pri_HolyRain, Pri_LifeBond, Pri_SalvationLight, Pri_HandOfHeaven | Hedefte yükselen yeşil-altın parçacıklar; Kutsal Yağmur'da üstten ışık damlaları; Yaşam Bağı'nda rahip-hedef arası ışık bağı; Cennetin Eli'nde gökten inen ışık eli | `soft green and gold healing sparkles`, `glowing hand of light silhouette` | `heal`, `healing aura` |
| 17 | Kutsal koruma (`NS_Holy_Buff`) | Pri_Renew (Regen), Pri_HolyShield (IronSkinII), Pri_ResiliencePrayer (RegenII) + durum `Regen` | Gövde çevresinde yarı saydam altın kalkan küresi; Regen'de nabız gibi atan küçük ışık | `golden bubble shield`, `pulse ring` | `shield bubble`, `regeneration` |
| 18 | Mob öfkesi (`NS_Status_Enrage`) | Durum `Rage` (mobların `EnrageEffectId`, can eşiği altında) | Mob üstünde kırmızı buhar + göz parlaması (Savaşçı Rage'inden daha koyu) | `red steam wisps` | `enrage`, `berserk aura` |

**Kapsama kontrolü:** Savaşçı: 10 + 6 + 4 + 3 = 23 · Okçu: 8 + 2 + 4 + 2 + 3 = 19 · Büyücü: 5 + 5 + 2 + 6 = 18 ·
Rahip: 5 + 5 + 8 + 3 = 21 · **Toplam 81, dışarıda kalan beceri yok.**

**Yeni 12 beceri → mevcut efekt grubu (2026-10-10; yeni Niagara varlığı gerekmez):** War_BladeStorm → #2 NS_War_Whirl · War_Obliterate, War_WorldBreaker → #1 NS_Melee_Impact (büyüklük artsın) · Arc_ShatterShot, Arc_SunderingArrow → #5 NS_Arrow_Trail · Arc_Multishot → #7 NS_Arrow_Rain · Mag_Combustion → #10 NS_Fire · Mag_GlacialSpike → #11 NS_Ice (Slow) · Mag_CosmicBlast → #13 NS_Arcane · Pri_SmitingLight → #14 NS_Holy_Bolt · Pri_Sanctify → #15 NS_Holy_Area · Pri_HealingSurge → #16 NS_Heal (Regen).

Ek dünya efekti [öneri]: `NS_Loot_Glow` (`AMMOLootPickup` için yerde hafif ışık sütunu).

---

## 9. Harita ve bölgeler

Kaynak: `MMOGameMode.cpp` (koordinatlar, harita başına), `Lore.md`, `MapOptions.md`. Koordinatlar UU (cm).
**Önemli:** Ayazdiş Buzulu (buz) ve Alev Vadisi (ateş) yaz haritası `Summer_scene` içindeki ceplerde. Bu ceplerin
havası, haritanın genel yaz havasından ayrı olarak **yerel süslemeyle** (decal, prop, sis, renk) verilmeli.

### PCG genel notu (her bölge için)

- Spawner merkezinin çevresinde **yarıçap + 450 u (gezinme)** kadar alanı PCG'de dışlama bölgesi yap. Çarpışmalı
  prop'lar spawner'ın zemin ve navmesh doğrulamasını bozabilir.
- Prop ekledikten sonra cebi yeniden ölç:
  `Scripts/SmokeTest.ps1 -Map Summer_scene -Cmds "MMOProbeSurface X Y 225,quit"`. Editör probu tek başına yanılmıştı
  (`CLAUDE.md`).
- Büyük çarpışmalı prop'lar NavMesh'i deler. Hub ile cep arasındaki yürüme yolunu açık bırak.
- Öneri: PCG Graph → Surface Sampler → Density Filter (eğim) → Difference (dışlama hacimleri) → Static Mesh Spawner.
  Bölge başına ayrı PCG Volume.

### Summer_scene (Lv1-40 + Lv58-65 dış cepler)

PlayerStart (-816, -47, 113). İlk bölgeler PlayerStart'a göre (`Origin` + yön), sonrakiler mutlak koordinatla tanımlı.

| Bölge (Lore) | Lv | Konum (kod) | Ruh hali | Ana varlıklar | Fab arama | AI arazi/skybox promptu |
|---|---|---|---|---|---|---|
| Akçakule (hub) | — | PlayerStart çevresi, NPC'ler +250/+400 u | Sıcak son ocak, sınır kalesi | Beyaz taş kule, palisat/sur, han, tapınak, pazar tezgâhı, ambar kapısı | `medieval village`, `fantasy town`, `market stall` (`MapOptions.md`) | `stylized hand-painted frontier fortress town, white lightning-scarred stone tower, wooden palisade, warm hearth light, late afternoon` |
| Solgun Ormanlar kenarı / çakılı vadiler | 1-5 | Ana spawner, Origin + yön × 1.5 | Solgun, sisli orman kıyısı | Soluk yapraklı ağaçlar, çakıl, devrik kütükler, goblin kamp çadırı | `stylized forest`, `hand painted trees` | `pale washed-out forest edge, gravel valley, light mist, stylized` |
| Karakaya Geçidi | 6-7 | İkinci bölge, arka-sağ | Dar, karanlık yarık | Siyah damarlı kaya duvarları, kırık kervan arabaları, ork bayrakları | `canyon rocks stylized`, `orc camp` | `narrow dark canyon pass, black-veined rock walls, broken caravan wagons, orc war banners` |
| Ayazdiş Buzulu | 8-12 (+Buz Lordu Lv13) | Üçüncü bölge, arka-orta; Lich arka-sağ köşe | Hiç erimeyen kış parçası | Kar decal'ı, buz kristalleri, yıkık şövalye tabyası, buz tahtı | `ice crystals stylized`, `snow decal`, `castle ruins` | `frozen plateau with ruined knight bastion, eternal blizzard mist, ice throne` |
| Çözülme Yolu | 13-15 | (-5000, -3000) | Ne kış ne yaz, eriyen buz | Eriyen buz kütleleri, su birikintisi, cüruf taşları | `melting ice`, `puddle decal`, `slag rocks` | `thawing slope, half-melted ice next to first embers, wet ground` |
| Alev Vadisi | 16-22 (+Magma Tiranı Lv24) | Dördüncü bölge, arka-sol; Tiran ön-sol | Kızıl gök, kavrulmuş | Lav akıntısı, bazalt, kül ovası, buhar menfezi | `lava stylized`, `volcanic rocks`, `basalt` | `volcanic valley, red sky, cooling lava rivers, ash plain, basalt columns` |
| Obsidyen Sırtları | 25-29 (+Reis Lv30) | (-2800, -1800); elit (-4000, 0) | Güneşte ayna gibi parlayan kara cam | Obsidyen kristal kümeleri, keskin sırtlar, ork bayrağı | `obsidian crystals`, `black glass rocks` | `ridges of glossy black obsidian glass reflecting the sun, sharp edges, violet sheen` |
| Alacakaranlık Korusu | 31-39 (+Hükümdar Lv40) | (1000, 3200); elit (-3200, 1800) | Hiç doğmayan, hiç batmayan güneş | Koyu yapraklı yaşlı ağaçlar, mor sis, kök kemerleri | `dark forest stylized`, `twilight fog`, `giant roots` | `ancient forest stuck in eternal twilight, violet fog, huge roots arching over paths` |
| Yarık (dış yamaçlar) | 58-65 | Arayıcı (800, -4800), Bekçi (-4800, 4800), Haberci (4800, 4000) | Tanıdık ovada mor çatlaklar | Toprak yarıkları (mor emissive), uçuşan taşlar, kristal | `ground fissure`, `magic crack decal`, `purple crystal` | `familiar green meadow torn by glowing purple fissures, floating rock fragments` |

### Winter_scene (Lv42-80)

| Bölge (Lore) | Lv | Konum (kod) | Ruh hali | Ana varlıklar | Fab arama | AI arazi/skybox promptu |
|---|---|---|---|---|---|---|
| Kış hub'ı | — | PlayerStart çevresi, 4 NPC | Kuzey Yolu'nun sonu, kafile kampı | Çadır, ateş çukuru, kızak, ambar kapısı | `winter camp`, `nordic tent` | `northern caravan camp in snow, campfire glow, tents and sleds` |
| Kış Diyarı | 42-48 (+Hükümdar Lv50) | (-3000, -1500); elit (-3000, 0) | Fırtına hiç dinmiyor, ufuk yok | Kar yığını, donmuş çam, buz sütunu, taht | `winter environment stylized`, `snow pine` | `endless white blizzard land, frozen pines, no visible horizon` |
| Buzbağlı Sınır | 51-55 (+Muhafız Lv57) | Gözcü (3000, 0), Nöbetçi (-3500, 3000), Muhafız (2000, 2800) | Buzdan sur, yemin | Buz duvarı, nöbet kuleleri, Buz Kapısı | `ice wall`, `frozen gate`, `ice castle` | `wall of ice stretching like a fortress border, giant sealed ice gate` |
| Kül Hattı | 66-68 (+Tiran Lv70) | Öncü (1200, -5200), Bağlayıcı (-2000, -6800), Tiran (4400, -5200) | Gri kar, gökten kül | Gri kar decal'ı, kömürleşmiş kütük, kara hat (zincir/çit) | `ash snow`, `burnt trees`, `ember particles` | `snowfield turned grey by falling ash, black line of chains across the snow` |
| Uçurum Yaylası | 72-76 (+Naib Lv80) | Gezgin (-5500, 4500), Koruyucu (4400, 3600), Naib (6800, 2000) | Çıplak yayla, dipsiz boşluk | Çıplak kaya, uçurum kenarı, boş taht | `cliff edge`, `abyss`, `bare plateau` | `bare windswept plateau around a bottomless dark chasm, an empty throne on the east edge` |

---

## 10. Takip tablosu

Durum değerleri: `yok` / `üretildi` / `rig'lendi` / `UE'ye aktarıldı`. Prompt sürümü varsayılan P1.

| Varlık | Araç | Prompt sürümü | Durum | Notlar |
|---|---|---|---|---|
| SK_Pc_Warrior_M/F | | P1 | yok | |
| SK_Pc_Archer_M/F | | P1 | yok | |
| SK_Pc_Mage_M/F | | P1 | yok | |
| SK_Pc_Priest_M/F | | P1 | yok | |
| SK_Npc_Merek | | P1 | yok | şimdi Manny + tint |
| SK_Npc_Doran | | P1 | yok | şimdi Manny + tint |
| SK_Npc_Kael | | P1 | yok | yalnız Summer |
| SK_Npc_Lyra | | P1 | yok | |
| SK_Npc_Rana | | P1 | yok | |
| SK_Mob_Goblin | | P1 | yok | deneme seti |
| SK_Mob_WolfPup | | P1 | yok | dört ayaklı |
| SK_Mob_Orc | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici; P1 ile değişirse Warrok iskeletinde kalsın |
| SK_Mob_Bandit | | P1 | yok | |
| SK_Mob_Troll | | P1 | yok | |
| SK_Mob_OrcWarrior | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici |
| SK_Mob_DireWolf | | P1 | yok | dört ayaklı |
| SK_Mob_Warlock | | P1 | yok | |
| SK_Mob_FrostWraith | | P1 | yok | deneme seti |
| SK_Mob_CursedKnight | | P1 | yok | |
| SK_Mob_VenomSpider | | P1 | yok | örümcek rig |
| SK_Mob_OrcBerserker | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici |
| SK_Mob_ThawStalker | | P1 | yok | dört ayaklı |
| SK_Mob_CrustWarden | | P1 | yok | |
| SK_Mob_ScorchedRaider | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici |
| SK_Mob_EmberHound | | P1 | yok | dört ayaklı |
| SK_Mob_MagmaGolem | | P1 | yok | |
| SK_Mob_AshOrc | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici |
| SK_Mob_InfernalWraith | | P1 | yok | |
| SK_Mob_ObsidianStalker | | P1 | yok | dört ayaklı |
| SK_Mob_ObsidianSentinel | | P1 | yok | |
| SK_Mob_ObsidianRaider | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici |
| SK_Mob_DuskProwler | | P1 | yok | dört ayaklı |
| SK_Mob_DuskGuardian | | P1 | yok | |
| SK_Mob_DuskReaver | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici |
| SK_Mob_DuskWraith | | P1 | yok | |
| SK_Mob_WinterStalker | | P1 | yok | |
| SK_Mob_WinterGuardian | | P1 | yok | |
| SK_Mob_WinterReaver | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici |
| SK_Mob_WinterWraith | | P1 | yok | |
| SK_Mob_FrostboundScout | | P1 | yok | |
| SK_Mob_FrostboundSentinel | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici; BRUTE |
| SK_Mob_RiftSeeker | | P1 | yok | dört ayaklı |
| SK_Mob_RiftKeeper | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici; BRUTE |
| SK_Mob_AshVanguard | | P1 | yok | |
| SK_Mob_AshBinder | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici; BRUTE |
| SK_Mob_AbyssStrider | | P1 | yok | dört ayaklı |
| SK_Mob_AbyssGuardian | Mixamo (Warrok) | — | UE'ye aktarıldı | geçici; BRUTE |
| SK_Mob_SpiderQueen | | P1 | yok | elit, örümcek rig |
| SK_Mob_OrcWarlord | Mixamo (Warrok) | — | UE'ye aktarıldı | elit, geçici |
| SK_Mob_StoneGolem | | P1 | yok | elit |
| SK_Mob_ShadowAssassin | | P1 | yok | elit |
| SK_Mob_FrostLich | | P1 | yok | elit |
| SK_Mob_MagmaTyrant | | P1 | yok | elit |
| SK_Mob_ObsidianWarlord | Mixamo (Warrok) | — | UE'ye aktarıldı | elit, geçici |
| SK_Mob_DuskOverlord | Mixamo (Warrok) | — | UE'ye aktarıldı | elit, geçici; BRUTE |
| SK_Mob_WinterSovereign | | P1 | yok | elit |
| SK_Mob_FrostboundWarden | | P1 | yok | elit |
| SK_Mob_RiftHerald | | P1 | yok | elit |
| SK_Mob_AshTyrant | | P1 | yok | elit |
| SK_Mob_WinterWolf | | P1 | yok | 2026-10-10 eklendi; dört ayaklı (WOLF) |
| SK_Mob_WinterBehemoth | Mixamo (Warrok) | — | UE'ye aktarıldı | 2026-10-10; geçici; BRUTE |
| SK_Mob_FrostboundSworn | Mixamo (Warrok) | — | UE'ye aktarıldı | 2026-10-10; geçici; BRUTE |
| SK_Mob_RiftWhisperer | | P1 | yok | 2026-10-10 eklendi; hızlı gölge (WRAITH) |
| SK_Mob_RiftWalker | Mixamo (Warrok) | — | UE'ye aktarıldı | 2026-10-10; geçici; BRUTE |
| SK_Mob_AshMarcher | Mixamo (Warrok) | — | UE'ye aktarıldı | 2026-10-10; geçici; BRUTE |
| SK_Mob_AbyssEcho | | P1 | yok | 2026-10-10 eklendi; süzülen gölge (WRAITH) |
| SK_Mob_AbyssEnvoy | Mixamo (Warrok) | — | UE'ye aktarıldı | 2026-10-10; geçici; BRUTE |
| SK_Mob_AbyssRegent | | P1 | yok | elit, son boss |
| NS_Melee_Impact … NS_Status_Enrage (18 grup) | | P1 | yok | bölüm 8 |
| Bölge prop setleri (Summer 9, Winter 5) | Fab + PCG | P1 | yok | bölüm 9 |

---

## 11. Kısıtlar ve bilinmeyenler

- **Hiçbir prompt denenmedi.** `{STYLE}` ilk 3-4 varlıkla ayarlanmadan toplu üretim yapma.
- **Sayılar (kodla doğrulandı, 2026-10-10):** 59 mob arketipi (46 normal + 13 elit), 93 beceri (26/22/21/24), 5 NPC.
  Dokümanlardaki "48 arketip" ve "73 beceri" sayıları bayat; görevdeki "73 beceri" de bu yüzden 81 olarak ele alındı.
- **[Tahmin] olanlar:** Poligon ve doku bütçeleri, LOD oranları, FBX ayarları, image-to-3D önerisi.
- **[Yorum] olanlar:** Lore'da beden tipi verilmeyen mobların görünümü (STALKER ailesi; WinterStalker, FrostboundScout,
  AshVanguard'ın insansı olması; BRUTE ailesinin "ork olmayan iri yaratık" yorumu), Doran'ın terazisi, Kael'in Kül
  Muhafızları üyeliği, Rahip'in Gümüş Dal bağlantısı, bütün renk vurguları.
- **[Doğrulanmadı]:** Mixamo/Meshy klip adlarının kesin yazımı. Meshy/Tripo/Ludo.ai'nin dört ayaklı ve örümcek
  rig desteği. Arketip `Tint`'inin dokulu mesh üzerindeki etkisi. Mob saldırı vuruşunun animasyonla senkron olup
  olmadığı. Ludo.ai'nin 3D/rig yetenekleri.
- **Kodda olmayanlar:** Mob için Hit/Walk animasyon yuvası. Yay, asa ve topuz eşyası (eşya DB'de yalnızca kılıç ve
  balta var). Oyuncu sınıfı başına ayrı mesh seçimi. Bunlar geliştirici kararıdır; bu belge yalnızca prompt hazırlar.
- **Harita:** Summer_scene/Winter_scene `HandPaintedEnvironment` paketinden geliyor. Paketin kendi prop'ları ve stili
  incelenmedi (ikili dosya). `{STYLE}` bu pakete göre seçildi ama gözle karşılaştırılmadı. Buz ve ateş cepleri yaz
  haritasında olduğu için yerel süsleme gerekiyor; bu iş görsel iterasyon ister.
- **Lisans:** Hiçbir aracın şartları bu belgede doğrulanmadı. Ticari kullanımı her araçta ayrıca kontrol et.
