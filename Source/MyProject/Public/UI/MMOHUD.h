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
};

/** HUD uzerinde tiklanabilir bir bolge (skill kitabi satiri veya hotbar slotu). */
struct FMMOClickRegion
{
	FBox2D Rect = FBox2D(ForceInit);
	FName SkillId;      // skill satiri icin
	int32 Slot = -1;    // hotbar slotu icin (-1 = skill satiri)
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

	// Bir karakter hasar aldiginda cagrilir; o dunya konumunda ucan hasar sayisi ekler.
	void AddDamageNumber(const FVector& WorldLocation, float Amount);

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
	// Herhangi bir pencere acik mi?
	bool IsAnyWindowOpen() const { return bSkillBookOpen || bInventoryOpen || bShopOpen || bQuestWindowOpen || bStatWindowOpen; }

protected:
	// Sahnedeki dusmanlarin tepesine saglik bari cizer (dunya->ekran projeksiyonu).
	void DrawEnemyHealthBars();

	// KO'nun TargetBar'i: Z ile secilen hedefin adi + HP'si ust-ortada; hedefin tepesine isaret cizer.
	void DrawTargetBar();

	// Aktif ucan hasar sayilarini cizer/gunceller.
	void DrawFloatingTexts();

	// Aktif hasar sayilari.
	TArray<FMMOFloatingText> FloatingTexts;

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

	bool bSkillBookOpen = false;
	bool bInventoryOpen = false;
	bool bShopOpen = false;
	bool bQuestWindowOpen = false;
	bool bStatWindowOpen = false;

	// Stat penceresi [+] tiklama bolgeleri (SkillId = "STR"/"DEX"/"INT"/"VIT").
	TArray<FMMOClickRegion> StatPlusRegions;

	// Her pencerenin cerceve dikdortgeni + X-kapat buton dikdortgeni (her kare Draw'da guncellenir).
	FBox2D SkillBookRect = FBox2D(ForceInit);
	FBox2D SkillBookCloseRect = FBox2D(ForceInit);
	FBox2D InventoryRect = FBox2D(ForceInit);
	FBox2D InventoryCloseRect = FBox2D(ForceInit);
	FBox2D ShopRect = FBox2D(ForceInit);
	FBox2D ShopCloseRect = FBox2D(ForceInit);
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

	// Dukkan tiklama bolgeleri: satin alma (SkillId=ItemId veya "__POTION__") ve satma (Slot=canta index).
	TArray<FMMOClickRegion> ShopBuyRegions;
	TArray<FMMOClickRegion> ShopSellRegions;

	// Gorev penceresi tiklama bolgeleri (SkillId=questId): kabul et ve odul al.
	TArray<FMMOClickRegion> QuestAcceptRegions;
	TArray<FMMOClickRegion> QuestClaimRegions;

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
