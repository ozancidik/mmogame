// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "GameFramework/HUD.h"
#include "MMOHUD.generated.h"

/** Havada ucan hasar sayisi kaydi (dunya konumu + miktar + dogus zamani). */
struct FMMOFloatingText
{
	FVector WorldLocation = FVector::ZeroVector;
	float Amount = 0.f;
	float SpawnTime = 0.f;
	bool bIsHeal = false;
	// Bos degilse sayi yerine bu metin cizilir (orn. "+12000 EXP").
	FString Label;
};

/** Sohbet kaydi satiri (sol altta cizilir). */
struct FMMOChatLine
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float Time = 0.f;
};

/** HUD uzerinde tiklanabilir bir bolge (skill kitabi satiri veya hotbar slotu). */
struct FMMOClickRegion
{
	FBox2D Rect = FBox2D(ForceInit);
	FName SkillId;      // skill satiri icin
	int32 Slot = -1;    // hotbar slotu icin (-1 = skill satiri)
	int32 InstanceId = 0; // canta/satis satirlari icin ekipman kimligi (0 = yok)
	int32 Plus = -1;      // canta satirinda cizim anindaki arti seviyesi (yukseltme istegi dogrulamasi)
};

/**
 * Basit Canvas tabanl� oyun i�i HUD: sol �st k��ede HP / MP / EXP barlar� ve seviye/say�sal bilgiler.
 * Her frame yerel oyuncunun MMOCharacter stat bile�enini okuyarak �izer (UMG asset'i gerektirmez).
 * �leride g�rsel cila i�in UMG'ye ta��nabilir.
 */
UCLASS()
class MYPROJECT_API AMMOHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// Bir karakter hasar aldiginda/iyilestiginde cagrilir; o dunya konumunda ucan sayi ekler
	// (hasar=kirmizimsi altin, iyilesme=yesil +N). bIsHeal=false varsayilan - eski cagri
	// yerleri (sadece hasar biliyordu) degismeden calismaya devam eder.
	void AddDamageNumber(const FVector& WorldLocation, float Amount, bool bIsHeal = false);

	// Ucan serbest metin (XP kazanci vb.); hasar sayisindan biraz daha uzun kalir.
	void AddFloatingLabel(const FVector& WorldLocation, const FString& Label);

	// Sohbet satiri ekler (sunucudan gelen genel/parti/sistem mesaji). Son 50 satir tutulur.
	void AddChatLine(const FString& Line, const FLinearColor& Color);

	// --- NPC diyalog balonu (konusan NPC'ye tiklaninca alt-ortada gosterilir) ---
	// Pencere DEGILDIR: hareketi engellemez, sadece bir sure gorunur ve solar.
	void ShowNpcDialogue(const FString& SpeakerName, const FString& Text);

	// --- Skill kitabi (K ile ac/kapa) ---
	void ToggleSkillBook();
	bool IsSkillBookOpen() const { return bSkillBookOpen; }

	// Sol tik skill kitabi/hotbar bolgesine denk geldiyse isler; islediyse true (hareket iptal).
	// Ogrenilmis skile basildiginda surukleme de baslar (KO tarzi drag&drop).
	bool HandleSkillBookClick(const FVector2D& Pos);

	// Sol tus birakildiginda: surukleme aktifse ve bir hotbar slotunun ustundeyse skili oraya atar.
	void HandleSkillBookRelease(const FVector2D& Pos);

	// Sag tik: ogrenilmis skile sag tiklaninca ilk BOS hotbar slotuna yerlestirir (KO tarzi). Islediyse true.
	bool HandleSkillBookRightClick(const FVector2D& Pos);

	// --- Envanter/ekipman penceresi (I ile ac/kapa) ---
	void ToggleInventory();
	bool IsInventoryOpen() const { return bInventoryOpen; }

	// Sol tik: kusanilana tikla = cikar; cantadakine tikla = kusan.
	bool HandleInventoryClick(const FVector2D& Pos);

	// Sag tik: cantadaki ekipmani +1 yukseltmeyi dener (yakma riski!).
	bool HandleInventoryRightClick(const FVector2D& Pos);

	// --- Satici dukkani (NPC'ye tiklaninca acilir) ---
	void OpenShop();
	void CloseShop();
	bool IsShopOpen() const { return bShopOpen; }

	// Depo penceresi (depocu NPC tiklaninca). Sol: canta -> depoya koy, sag: depo -> cantaya al.
	void OpenWarehouse();
	bool IsWarehouseOpen() const { return bWarehouseOpen; }
	void CloseWarehouse() { bWarehouseOpen = false; }

	// Ticaret penceresi: yerel oyuncunun ticaret bileseni aktifken (replike durumdan) acik sayilir.
	bool IsTradeOpen() const;
	bool HandleShopClick(const FVector2D& Pos);

	// --- Gorev penceresi (gorevli NPC'ye tiklaninca acilir) ---
	void OpenQuestWindow();
	void CloseQuestWindow();
	bool IsQuestWindowOpen() const { return bQuestWindowOpen; }
	bool HandleQuestClick(const FVector2D& Pos);

	// --- Karakter/stat penceresi (C ile ac/kapa) ---
	void ToggleStatWindow();
	bool IsStatWindowOpen() const { return bStatWindowOpen; }
	// Sol tik: bir statin [+] butonuna basinca puan harcar. Islediyse true.
	bool HandleStatClick(const FVector2D& Pos);

	// Acik pencerelerden birine sol-tiklandiysa isler (ustteki pencere oncelikli; X ile kapatma dahil).
	// Bir pencereyi tuketti ise true doner (karakter hareket etmez).
	bool HandleWindowClick(const FVector2D& Pos);
	// Acik pencereye sag-tiklandiysa isler (skill ata / ekipman yukselt). Tuketti ise true.
	bool HandleWindowRightClick(const FVector2D& Pos);
	// Yeni karakterin sinif secimi panelindeki tiklamayi isler; dunyaya hareketi engeller.
	bool HandleClassChoiceClick(const FVector2D& Pos);
	// Listen server host'unun tum oturumu diger bolgeye tasima dugmesi.
	bool HandleZoneTravelClick(const FVector2D& Pos);
	// Herhangi bir pencere acik mi?
	bool IsAnyWindowOpen() const { return bSkillBookOpen || bInventoryOpen || bShopOpen || bQuestWindowOpen || bStatWindowOpen || bWarehouseOpen || IsTradeOpen(); }

protected:
	// Sahnedeki dusmanlarin tepesine saglik bari cizer (dunya->ekran projeksiyonu).
	void DrawEnemyHealthBars();

	// KO'nun TargetBar'i: Z ile secilen hedefin adi + HP'si ust-ortada; hedefin tepesine isaret cizer.
	void DrawTargetBar();
	void DrawNearbyThreat(class AMMOCharacter* PlayerChar);

	// Aktif ucan hasar sayilarini cizer/gunceller.
	void DrawFloatingTexts();
	void DrawChat();

	// Aktif hasar sayilari.
	TArray<FMMOFloatingText> FloatingTexts;
	TArray<FMMOChatLine> ChatLines;

	// Tek bir bar �izer: koyu zemin + �er�eve + doluluk oran�na g�re renkli dolgu.
	void DrawStatBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& FillColor);

	// Ozel fare imleci: normalde ucgen ok, saldiri modunda kilic.
	void DrawMouseCursor(float X, float Y, bool bAttackMode);

	// Imleci (ve suruklenen skill hayaletini) mevcut fare konumunda cizer.
	// Her seyin ustunde kalmasi icin DrawHUD'un EN SONUNDA cagrilir.
	void DrawPlayerCursor();

	// Aktif buff/debuff ikonlari (barlarin altinda: kisa etiket + kalan sure + suzulme cubugu).
	void DrawActiveEffects(class AMMOCharacter* PlayerChar, float StartX, float StartY);

	// Parti uyeleri paneli (ad + HP bari). Partide degilse cizmez.
	void DrawParty(class AMMOCharacter* PlayerChar, float StartX, float StartY);

	// Alt-orta hotbar (0-9 slot: atanan skill + cooldown). Tiklama bolgelerini de doldurur.
	void DrawHotbar(class AMMOCharacter* PlayerChar);

	// Skill kitabi penceresi (agac + ogrenme). Tiklama bolgelerini doldurur.
	void DrawSkillBook(class AMMOCharacter* PlayerChar);

	// Envanter/ekipman penceresi. Tiklama bolgelerini doldurur.
	void DrawInventory(class AMMOCharacter* PlayerChar);

	// Satici dukkani penceresi.
	void DrawShop(class AMMOCharacter* PlayerChar);
	void DrawWarehouse(class AMMOCharacter* PlayerChar);
	bool HandleWarehouseClick(const FVector2D& Pos);

	// Oyuncular arasi ticaret penceresi (teklif ekle/cikar, altin, onayla, iptal).
	void DrawTrade(class AMMOCharacter* PlayerChar);
	bool HandleTradeClick(const FVector2D& Pos);

	// Sahnedeki saticilarin/gorevlilerin tepesine isim plakasi.
	void DrawVendorNameplates();

	// Sag ust minimap (oyuncu merkezli, north-up).
	void DrawMinimap(class AMMOCharacter* PlayerChar);

	// Ekranda aktif gorev takibi.
	void DrawQuestTracker(class AMMOCharacter* PlayerChar);

	// Gorev penceresi (kabul et / odul al).
	void DrawQuestWindow(class AMMOCharacter* PlayerChar);

	// Karakter/stat penceresi (ana+turetilmis statlar + puan dagitma).
	void DrawStatWindow(class AMMOCharacter* PlayerChar);

	// Bir pencerenin sag ustune X kapat butonu cizer; butonun dikdortgenini dondurur.
	FBox2D DrawCloseButton(float PX, float PY, float PW);

	// Diger oyuncularin tepesine isim + level + HP bari (MP).
	void DrawPlayerNameplates();

	// Aktif NPC diyalog balonunu alt-ortada cizer (suresi dolunca kaybolur).
	void DrawNpcDialogue();

	// Aktif diyalog balonu durumu.
	FString NpcDialogueSpeaker;
	FString NpcDialogueText;
	float NpcDialogueStartTime = -1000.f;         // -1000 = gosterilecek diyalog yok
	static constexpr float NpcDialogueDuration = 9.f;

	// --- Ilk 10 dakika ipuclari (yalniz yerel HUD; sunucu mantigi/RPC/kayit yok) ---
	// Yerel replike durumu (Stats/Quest/Inventory) izleyip olay anlarinda ipucu siraya koyar; ayni anda tek ipucu.
	void UpdateTutorialHints(class AMMOCharacter* PlayerChar);
	void QueueHint(int32 HintIndex, int32 PlayerLevel);
	bool IsNpcDialogueShowing(float Now) const { return NpcDialogueStartTime > -900.f && Now - NpcDialogueStartTime <= NpcDialogueDuration; }

	uint16 HintPendingMask = 0;
	float HintQueuedAt[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	float HintNextAllowedTime = 0.f;
	// Ekranda gosterilmekte olan ipucu (-1 = yok) ve balonun basladigi an. "Gosterildi" biti ancak
	// balon tam suresi boyunca ezilmeden kaldiysa atilir; ezilirse ipucu yeniden siraya girer.
	int32 HintActiveIndex = -1;
	float HintActiveStart = -1000.f;
	TWeakObjectPtr<class AMMOCharacter> HintPawn;
	float HintPawnFirstSeen = -1.f;
	float HintMoveWatchStart = -1.f;
	FVector HintMoveOrigin = FVector::ZeroVector;
	float HintNextNpcScan = 0.f;
	float HintNextLootScan = 0.f;
	int32 HintPrevLevel = 0;
	int32 HintPrevQuestCount = -1;
	bool bHintPrevDead = false;
	bool bHintPrevBagFull = false;
	float HintQuestAcceptedAt = -1.f;
	int32 HintQuestAcceptLevel = 0;
	int32 HintQuestAcceptExp = 0;

	bool bSkillBookOpen = false;
	bool bInventoryOpen = false;
	bool bShopOpen = false;
	bool bWarehouseOpen = false;
	FBox2D WarehouseRect = FBox2D(ForceInit);
	FBox2D WarehouseCloseRect = FBox2D(ForceInit);
	TArray<FMMOClickRegion> WarehouseBagRegions;
	TArray<FMMOClickRegion> WarehouseStoreRegions;

	// Ticaret penceresi cerceveleri + tiklama bolgeleri (Slot = InstanceId; altin dugmelerinde delta indeksi).
	FBox2D TradeRect = FBox2D(ForceInit);
	FBox2D TradeCloseRect = FBox2D(ForceInit);
	FBox2D TradeConfirmRect = FBox2D(ForceInit);
	FBox2D TradeCancelRect = FBox2D(ForceInit);
	TArray<FMMOClickRegion> TradeBagRegions;
	TArray<FMMOClickRegion> TradeOfferRegions;
	TArray<FMMOClickRegion> TradeGoldRegions;
	bool bQuestWindowOpen = false;
	bool bStatWindowOpen = false;

	// Stat penceresi [+] tiklama bolgeleri (SkillId = "STR"/"DEX"/"INT"/"VIT").
	TArray<FMMOClickRegion> StatPlusRegions;
	FBox2D ClassChoicePanelRect = FBox2D(ForceInit);
	TArray<FMMOClickRegion> ClassChoiceRegions;
	FBox2D ZoneTravelRect = FBox2D(ForceInit);

	// Her pencerenin cerceve dikdortgeni + X-kapat buton dikdortgeni (her kare Draw'da guncellenir).
	FBox2D SkillBookRect = FBox2D(ForceInit);
	FBox2D SkillBookCloseRect = FBox2D(ForceInit);
	FBox2D InventoryRect = FBox2D(ForceInit);
	FBox2D InventoryCloseRect = FBox2D(ForceInit);
	FBox2D ShopRect = FBox2D(ForceInit);
	FBox2D ShopCloseRect = FBox2D(ForceInit);
	FBox2D ShopBuyPrevRect = FBox2D(ForceInit);
	FBox2D ShopBuyNextRect = FBox2D(ForceInit);
	FBox2D ShopSellPrevRect = FBox2D(ForceInit);
	FBox2D ShopSellNextRect = FBox2D(ForceInit);
	FBox2D QuestRect = FBox2D(ForceInit);
	FBox2D QuestCloseRect = FBox2D(ForceInit);
	FBox2D StatRect = FBox2D(ForceInit);
	FBox2D StatCloseRect = FBox2D(ForceInit);

	// Envanter tiklama bolgeleri: kusanilan slotlar (Slot=(int)EMMOEquipSlot) ve canta satirlari (Slot=canta index).
	TArray<FMMOClickRegion> InvEquippedRegions;
	TArray<FMMOClickRegion> InvBagRegions;
	// Tuketilebilir istif satirlari (SkillId=ItemId) - bag-hunt: satici Mana/Can iksirleri
	// disindaki tum tuketilebilirleri (Buyuk iksirler, parsomenler) satiyordu ama hicbir
	// UI/tus yolu onlari KULLANMAYA izin vermiyordu (satin alinip sonsuza dek kullanilamaz
	// kaliyorlardi). Envanter penceresi artik Items istifini de gosterip tiklamayla kullaniyor.
	TArray<FMMOClickRegion> InvConsumableRegions;

	// Satin alma sayfasi ve bolgeleri: SkillId=ItemId, Slot=0 iksir / 1 tuketilebilir / 2 ekipman.
	static constexpr int32 ShopBuyPageSize = 14;
	int32 ShopBuyPage = 0;
	TArray<FMMOClickRegion> ShopBuyRegions;
	// Satma sayfasi ve bolgeleri: Slot=EquipmentBag icindeki mutlak canta index'i.
	static constexpr int32 ShopSellPageSize = 12;
	int32 ShopSellPage = 0;
	TArray<FMMOClickRegion> ShopSellRegions;

	// Gorev penceresi tiklama bolgeleri (SkillId=questId): kabul et ve odul al.
	TArray<FMMOClickRegion> QuestAcceptRegions;
	TArray<FMMOClickRegion> QuestClaimRegions;

	// Tiklanmis ama sunucu sonucu henuz gorulmemis kabul/teslim istekleri. Balon, durum gercekten
	// degisince (ActiveQuests'e girdi / CompletedQuests'e girdi) gosterilir; 2 sn icinde degismezse
	// (canta dolu, uzak, hiz siniri) istek atilir ve balon cikmaz.
	struct FPendingQuestLine
	{
		FName QuestId;
		bool bClaim = false;
		double ExpireTime = 0.0;
	};
	TArray<FPendingQuestLine> PendingQuestLines;
	void UpdatePendingQuestLines(const class UMMOQuestComponent* Q);

	// Atama icin "eline alinan" skill (kitapta ogrenilmise tiklaninca).
	FName PendingAssignSkill;

	// Surukle-birak durumu (sol tus basiliyken tasinan skill).
	FName DraggingSkill;
	bool bDragging = false;

	// Her karede yeniden olusturulan tiklama bolgeleri.
	TArray<FMMOClickRegion> HotbarRegions;
	TArray<FMMOClickRegion> SkillBookRegions;

	// Bar yerle�imi (ekran koordinat�, sol �st).
	UPROPERTY(EditDefaultsOnly, Category = "MMO|HUD")
	float BarOriginX = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|HUD")
	float BarOriginY = 48.f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|HUD")
	float BarWidth = 340.f;
};
