// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOHUD.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Characters/MMOMobArchetype.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOPartyComponent.h"
#include "Components/MMOTradeComponent.h"
#include "Components/MMOClanComponent.h"
#include "Effects/MMOEffectDatabase.h"
#include "Items/MMOConsumableTypes.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOSkillComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Skills/MMOSkillDatabase.h"
#include "Items/MMOItemDatabase.h"
#include "Quests/MMOQuestTypes.h"
#include "Quests/MMOQuestDialogue.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOQuestGiverNPC.h"
#include "World/MMOTalkerNPC.h"
#include "World/MMOHealerNPC.h"
#include "World/MMOWarehouseNPC.h"
#include "World/MMOLootPickup.h"
#include "Engine/LocalPlayer.h"
#include "Player/MMOPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

// Sinif adi (Turkce) artik Skills/MMOSkillTypes.h'deki GetMMOClassDisplayName'den gelir —
// HUD ve sinif degisim mesajlari ayni adi gostersin diye tek yerde tanimli.

void AMMOHUD::DrawStatBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& FillColor)
{
	Percent = FMath::Clamp(Percent, 0.f, 1.f);

	// �er�eve (barlar�n biraz d���nda koyu bir kenarl�k).
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f), X - 2.f, Y - 2.f, Width + 4.f, Height + 4.f);
	// Bo� zemin.
	DrawRect(FLinearColor(0.14f, 0.14f, 0.16f, 0.9f), X, Y, Width, Height);
	// Doluluk.
	if (Percent > 0.f)
	{
		DrawRect(FillColor, X, Y, Width * Percent, Height);
	}
}

void AMMOHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	// Dusman saglik barlari ve ucan hasar sayilari (stat'lardan bagimsiz, dunya projeksiyonu).
	DrawEnemyHealthBars();
	DrawVendorNameplates();
	DrawPlayerNameplates();
	DrawFloatingTexts();
	DrawTargetBar();
	DrawChat();
	if (AMMOCharacter* ThreatPlayer = Cast<AMMOCharacter>(GetOwningPawn()))
	{
		DrawNearbyThreat(ThreatPlayer);
	}

	AMMOCharacter* MMOChar = Cast<AMMOCharacter>(GetOwningPawn());
	if (!MMOChar)
	{
		DrawPlayerCursor();
		return;
	}

	// Gorev kabul/teslim balonlari: sunucu sonucu (replike liste degisimi) gelince goster.
	UpdatePendingQuestLines(MMOChar->GetQuestComponent());

	UMMOStatsComponent* S = MMOChar->GetStats();
	if (!S)
	{
		DrawPlayerCursor();
		return;
	}

	// Yeni oyuncu ipuclari (yerel durum izleme; pencereler/diyalog bos ise tek balon).
	UpdateTutorialHints(MMOChar);

	const float X = BarOriginX;
	const float Y = BarOriginY;

	// Seviye + sinif yazisi (barlarin ustunde). Sinif adi eskiden sadece skill kitabinda
	// (K) goruluyordu; artik her an ana HUD'da da gorunur.
	FString LevelClassLine = FString::Printf(TEXT("Lv %d"), S->Level);
	if (const UMMOSkillComponent* SkillComp = MMOChar->GetSkillComponent())
	{
		LevelClassLine += FString::Printf(TEXT("  %s"), *GetMMOClassDisplayName(SkillComp->CharacterClass));
	}
	DrawText(LevelClassLine, FLinearColor::White, X, Y - 26.f, nullptr, 1.3f);

	// HP bar� (k�rm�z�).
	DrawStatBar(X, Y, BarWidth, 26.f, S->GetHealthPercent(), FLinearColor(0.78f, 0.12f, 0.12f, 1.f));
	DrawText(FString::Printf(TEXT("HP  %d / %d"), FMath::RoundToInt(S->Health), FMath::RoundToInt(S->MaxHealth)),
		FLinearColor::White, X + 8.f, Y + 4.f);

	// Canlanma korumasi aktifken HP barinin sagina kucuk 'KORUMA' etiketi (replike bayrak).
	if (MMOChar->IsSpawnProtected())
	{
		DrawRect(FLinearColor(0.1f, 0.3f, 0.6f, 0.85f), X + BarWidth + 8.f, Y + 2.f, 78.f, 22.f);
		DrawText(TEXT("KORUMA"), FLinearColor(0.75f, 0.9f, 1.f), X + BarWidth + 14.f, Y + 5.f, nullptr, 1.1f);
	}

	// MP bar� (mavi).
	const float ManaY = Y + 34.f;
	DrawStatBar(X, ManaY, BarWidth, 20.f, S->GetManaPercent(), FLinearColor(0.15f, 0.35f, 0.9f, 1.f));
	DrawText(FString::Printf(TEXT("MP  %d / %d"), FMath::RoundToInt(S->Mana), FMath::RoundToInt(S->MaxMana)),
		FLinearColor::White, X + 8.f, ManaY + 2.f);

	// EXP bar� (ince, alt�n).
	const float ExpY = ManaY + 28.f;
	DrawStatBar(X, ExpY, BarWidth, 8.f, S->GetExperiencePercent(), FLinearColor(0.92f, 0.76f, 0.16f, 1.f));
	DrawText(FString::Printf(TEXT("EXP  %d / %d"), S->Experience, S->ExperienceToNextLevel),
		FLinearColor(0.85f, 0.85f, 0.85f), X + BarWidth + 10.f, ExpY - 4.f);

	// Altin + iksir sayisi.
	if (UMMOInventoryComponent* Inv = MMOChar->GetInventory())
	{
		DrawText(FString::Printf(TEXT("Altin: %d    Iksir: %d (F)"), Inv->Gold, Inv->GetItemCount(FName(TEXT("HealthPotion")))),
			FLinearColor(1.f, 0.85f, 0.2f), X, ExpY + 16.f, nullptr, 1.1f);
	}

	// Aktif buff/debuff ikonlari (altin satirinin altinda).
	DrawActiveEffects(MMOChar, X, ExpY + 40.f);

	// Parti paneli (efekt ikonlarinin altinda).
	DrawParty(MMOChar, X, ExpY + 84.f);

	// Sag ust minimap + gorev takibi (her zaman) + gorev penceresi (acikken).
	DrawMinimap(MMOChar);
	DrawQuestTracker(MMOChar);
	if (bQuestWindowOpen)
	{
		DrawQuestWindow(MMOChar);
	}

	// Hotbar (her zaman) + acik pencereler.
	DrawHotbar(MMOChar);
	if (bSkillBookOpen)
	{
		DrawSkillBook(MMOChar);
	}
	if (bInventoryOpen)
	{
		DrawInventory(MMOChar);
	}
	if (bShopOpen)
	{
		DrawShop(MMOChar);
	}
	if (bWarehouseOpen)
	{
		DrawWarehouse(MMOChar);
	}
	if (IsTradeOpen())
	{
		DrawTrade(MMOChar);
	}
	if (bStatWindowOpen)
	{
		DrawStatWindow(MMOChar);
	}

	// Acik pencerelerin basliklarini kapatmamasi icin ipucunu pencereler kapanana kadar gizle.
	// Secim yapilmadiysa pencere kapatildiginda tekrar gorunur.
	const bool bAnyWindowOpen = bQuestWindowOpen || bSkillBookOpen || bInventoryOpen ||
		bShopOpen || bStatWindowOpen || bWarehouseOpen || IsTradeOpen();
	ZoneTravelRect = FBox2D(ForceInit);
	const AMMOPlayerController* LocalPC = Cast<AMMOPlayerController>(GetOwningPlayerController());
	const FString MapName = GetWorld() ? GetWorld()->GetMapName() : FString();
	const bool bSupportedMap = MapName.EndsWith(TEXT("Summer_scene")) || MapName.EndsWith(TEXT("Winter_scene"));
	if (!bAnyWindowOpen && !S->bIsDead && LocalPC && LocalPC->HasAuthority()
		&& LocalPC->IsLocalController() && bSupportedMap)
	{
		const float BW = FMath::Min(262.f, FMath::Max(180.f, (float)Canvas->SizeX - 32.f));
		const float BH = 34.f;
		const float BX = ((float)Canvas->SizeX - BW) * 0.5f;
		const float BY = (float)Canvas->SizeY - 154.f;
		ZoneTravelRect = FBox2D(FVector2D(BX, BY), FVector2D(BX + BW, BY + BH));
		DrawRect(FLinearColor(0.07f, 0.23f, 0.35f, 0.9f), BX, BY, BW, BH);
		DrawRect(FLinearColor(0.3f, 0.8f, 1.f, 0.95f), BX, BY, BW, 2.f);
		const FString Label = MapName.EndsWith(TEXT("Summer_scene"))
			? TEXT("Tum oyunculari Kis'a tasi") : TEXT("Tum oyunculari Yaz'a tasi");
		float LabelW = 0.f, LabelH = 0.f;
		GetTextSize(Label, LabelW, LabelH, nullptr, 0.9f);
		DrawText(Label, FLinearColor::White, BX + (BW - LabelW) * 0.5f,
			BY + (BH - LabelH) * 0.5f, nullptr, 0.9f);
	}
	ClassChoiceRegions.Reset();
	ClassChoicePanelRect = FBox2D(ForceInit);
	if (MMOChar->ShouldShowClassHint() && !bAnyWindowOpen)
	{
		constexpr float Gap = 8.f;
		const float ButtonW = FMath::Clamp(((float)Canvas->SizeX - 48.f - 3.f * Gap) / 4.f, 60.f, 110.f);
		const float PW = 4.f * ButtonW + 3.f * Gap + 24.f;
		const float PH = 68.f;
		const float PX = ((float)Canvas->SizeX - PW) * 0.5f;
		const float PY = 84.f;
		ClassChoicePanelRect = FBox2D(FVector2D(PX, PY), FVector2D(PX + PW, PY + PH));
		DrawRect(FLinearColor(0.04f, 0.07f, 0.14f, 0.85f), PX, PY, PW, PH);
		DrawRect(FLinearColor(0.35f, 0.6f, 1.f, 0.95f), PX, PY, PW, 2.f);
		DrawText(TEXT("Sinifini sec (varsayilan: Savasci)"), FLinearColor(0.75f, 0.88f, 1.f),
			PX + 12.f, PY + 6.f, nullptr, 0.85f);
		const EMMOClass Classes[] = { EMMOClass::Warrior, EMMOClass::Archer, EMMOClass::Mage, EMMOClass::Priest };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Classes); ++i)
		{
			const float BX = PX + 12.f + i * (ButtonW + Gap);
			const float BY = PY + 29.f;
			DrawRect(Classes[i] == EMMOClass::Warrior ? FLinearColor(0.2f, 0.35f, 0.55f) : FLinearColor(0.13f, 0.22f, 0.35f),
				BX, BY, ButtonW, 30.f);
			const FString Name = GetMMOClassDisplayName(Classes[i]);
			const float TextScale = ButtonW < 90.f ? 0.7f : 0.9f;
			float TextW = 0.f, TextH = 0.f;
			GetTextSize(Name, TextW, TextH, nullptr, TextScale);
			DrawText(Name, FLinearColor::White, BX + (ButtonW - TextW) * 0.5f,
				BY + (30.f - TextH) * 0.5f, nullptr, TextScale);
			FMMOClickRegion Region;
			Region.Slot = i;
			Region.Rect = FBox2D(FVector2D(BX, BY), FVector2D(BX + ButtonW, BY + 30.f));
			ClassChoiceRegions.Add(Region);
		}
	}

	// Olum ekrani: karartma + OLDUN + geri sayim.
	if (S->bIsDead)
	{
		const float W = (float)Canvas->SizeX;
		const float H = (float)Canvas->SizeY;
		DrawRect(FLinearColor(0.25f, 0.f, 0.f, 0.45f), 0.f, 0.f, W, H);

		DrawText(TEXT("OLDUN"), FLinearColor(1.f, 0.15f, 0.15f), W * 0.5f - 64.f, H * 0.42f, nullptr, 3.2f);

		const float Remaining = MMOChar->GetRespawnTimeRemaining();
		if (Remaining > 0.f)
		{
			DrawText(FString::Printf(TEXT("Yeniden dogus: %.0f"), FMath::CeilToFloat(Remaining)),
				FLinearColor::White, W * 0.5f - 70.f, H * 0.42f + 58.f, nullptr, 1.4f);
		}
	}

	// NPC diyalog balonu (varsa) — pencerelerin uzerinde ama imlecin altinda.
	DrawNpcDialogue();

	// Imleç EN SON çizilir ki tüm pencerelerin (görev/skill/envanter/dükkan) üstünde kalsin.
	DrawPlayerCursor();
}

void AMMOHUD::DrawPlayerCursor()
{
	AMMOPlayerController* MPC = Cast<AMMOPlayerController>(PlayerOwner);
	if (!MPC)
	{
		return;
	}

	float MX = 0.f, MY = 0.f;
	if (!MPC->GetMousePosition(MX, MY))
	{
		return;
	}

	// Suruklenen skill "hayaleti" imleci takip eder.
	if (bDragging && !DraggingSkill.IsNone())
	{
		if (const FMMOSkillDef* DragDef = FMMOSkillDatabase::Find(DraggingSkill))
		{
			DrawRect(FLinearColor(0.1f, 0.15f, 0.3f, 0.8f), MX + 14.f, MY + 10.f, 110.f, 22.f);
			DrawText(DragDef->Name, FLinearColor(1.f, 0.9f, 0.4f), MX + 18.f, MY + 13.f, nullptr, 0.8f);
		}
	}

	DrawMouseCursor(MX, MY, MPC->IsInAttackMode());
}

void AMMOHUD::DrawActiveEffects(AMMOCharacter* PlayerChar, float StartX, float StartY)
{
	if (!PlayerChar || !PlayerChar->GetStats())
	{
		return;
	}

	const TArray<FMMOActiveEffect>& Effects = PlayerChar->GetStats()->GetActiveEffects();
	if (Effects.Num() == 0)
	{
		return;
	}

	const float BoxW = 46.f;
	const float BoxH = 34.f;
	const float Gap = 6.f;

	// Kalan sure replike EndServerTime'dan hesaplanir (RemainingTime istemciye replike edilmez).
	const UWorld* EffectWorld = GetWorld();
	const AGameStateBase* EffectGS = EffectWorld ? EffectWorld->GetGameState() : nullptr;
	const float ServerNow = EffectGS ? EffectGS->GetServerWorldTimeSeconds() : (EffectWorld ? EffectWorld->GetTimeSeconds() : 0.f);

	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		const FMMOActiveEffect& E = Effects[i];
		const float Remaining = FMath::Max(0.f, E.EndServerTime - ServerNow);
		const float BX = StartX + i * (BoxW + Gap);

		// Buff = yesil, debuff = kirmizi ton.
		const FLinearColor Border = E.bIsDebuff ? FLinearColor(0.85f, 0.25f, 0.2f) : FLinearColor(0.3f, 0.8f, 0.4f);
		const FLinearColor Fill = E.bIsDebuff ? FLinearColor(0.25f, 0.05f, 0.05f, 0.85f) : FLinearColor(0.05f, 0.18f, 0.08f, 0.85f);

		// Kutu (arka plan + kenarlik).
		DrawRect(Border, BX - 1.f, StartY - 1.f, BoxW + 2.f, BoxH + 2.f);
		DrawRect(Fill, BX, StartY, BoxW, BoxH);

		// Kisa etiket (efekt veritabanindan).
		FString Label = E.EffectId.ToString();
		if (const FMMOEffectDef* Def = FMMOEffectDatabase::Find(E.EffectId))
		{
			if (!Def->ShortLabel.IsEmpty())
			{
				Label = Def->ShortLabel;
			}
		}
		DrawText(Label, FLinearColor::White, BX + 5.f, StartY + 3.f, nullptr, 1.0f);

		// Kalan sure (saniye).
		DrawText(FString::Printf(TEXT("%.0f"), FMath::CeilToFloat(Remaining)),
			FLinearColor(0.9f, 0.9f, 0.9f), BX + 5.f, StartY + 18.f, nullptr, 0.85f);

		// Suzulme cubugu (kalan/toplam).
		const float Pct = E.Duration > 0.f ? FMath::Clamp(Remaining / E.Duration, 0.f, 1.f) : 0.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), BX, StartY + BoxH - 4.f, BoxW, 4.f);
		DrawRect(Border, BX, StartY + BoxH - 4.f, BoxW * Pct, 4.f);
	}
}

void AMMOHUD::DrawParty(AMMOCharacter* PlayerChar, float StartX, float StartY)
{
	if (!PlayerChar)
	{
		return;
	}
	UMMOPartyComponent* Party = PlayerChar->GetPartyComponent();
	if (!Party || !Party->IsInParty())
	{
		return;
	}

	// Lider kimse gorunsun: uye cikarma (MMOPartyKick) sadece liderde calisiyor.
	DrawText(Party->IsPartyLeader() ? TEXT("PARTI  (lider sensin)") : TEXT("PARTI"),
		FLinearColor(0.6f, 0.85f, 1.f), StartX, StartY, nullptr, 0.85f);
	float Y = StartY + 18.f;

	const float BarW = 150.f;
	const float BarH = 8.f;

	for (const TObjectPtr<AMMOCharacter>& MPtr : Party->Members)
	{
		AMMOCharacter* M = MPtr.Get();
		UMMOStatsComponent* S = M ? M->GetStats() : nullptr;
		if (!S)
		{
			continue;
		}

		FString Name = TEXT("Uye");
		if (const APlayerState* PS = M->GetPlayerState())
		{
			Name = PS->GetPlayerName();
		}

		// Sinif adi da goster (CharacterClass tum istemcilere replike edilir, sadece
		// sahibine degil) - ayni tutarlilik gecisinin bir parcasi: heal-target skili
		// kullanirken parti listesinde kimin Priest oldugunu gormek faydali.
		FString ClassSuffix;
		if (const UMMOSkillComponent* MSkill = M->GetSkillComponent())
		{
			ClassSuffix = FString::Printf(TEXT("  %s"), *GetMMOClassDisplayName(MSkill->CharacterClass));
		}

		const FString LeaderTag = (Party->Leader == M) ? TEXT("[L] ") : TEXT("");

		DrawText(FString::Printf(TEXT("%s%s  Lv%d%s"), *LeaderTag, *Name, S->Level, *ClassSuffix),
			S->bIsDead ? FLinearColor(0.7f, 0.3f, 0.3f) : FLinearColor::White, StartX, Y, nullptr, 0.75f);
		Y += 14.f;

		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f), StartX - 1.f, Y - 1.f, BarW + 2.f, BarH + 2.f);
		DrawRect(FLinearColor(0.15f, 0.15f, 0.16f, 0.9f), StartX, Y, BarW, BarH);
		DrawRect(FLinearColor(0.2f, 0.75f, 0.3f), StartX, Y, BarW * S->GetHealthPercent(), BarH);
		Y += BarH + 8.f;
	}
}

void AMMOHUD::DrawMouseCursor(float X, float Y, bool bAttackMode)
{
	const FLinearColor Dark(0.02f, 0.02f, 0.02f, 1.f);

	if (!bAttackMode)
	{
		// Ucgen ok imleci: tepe (X,Y), govde asagi-saga dogru.
		const FVector2D A(X, Y);
		const FVector2D B(X + 16.f, Y + 7.f);
		const FVector2D C(X + 7.f, Y + 16.f);
		const FLinearColor White(1.f, 1.f, 1.f, 1.f);

		// Koyu kenarlik (kalin), sonra beyaz ic (ince) -> kontrast.
		DrawLine(A.X, A.Y, B.X, B.Y, Dark, 4.f);
		DrawLine(B.X, B.Y, C.X, C.Y, Dark, 4.f);
		DrawLine(C.X, C.Y, A.X, A.Y, Dark, 4.f);

		DrawLine(A.X, A.Y, B.X, B.Y, White, 2.f);
		DrawLine(B.X, B.Y, C.X, C.Y, White, 2.f);
		DrawLine(C.X, C.Y, A.X, A.Y, White, 2.f);
		// Dolgu hissi: tepe -> taban ortasi.
		DrawLine(A.X, A.Y, (B.X + C.X) * 0.5f, (B.Y + C.Y) * 0.5f, White, 2.f);
	}
	else
	{
		// Kilic imleci: capraz bicak + gold korkuluk + sap.
		const FLinearColor Blade(0.85f, 0.88f, 0.95f, 1.f);
		const FLinearColor Gold(1.f, 0.8f, 0.15f, 1.f);
		const FLinearColor Handle(0.35f, 0.2f, 0.08f, 1.f);

		// Bicak: uc (X,Y) -> asagi-saga.
		DrawLine(X, Y, X + 22.f, Y + 22.f, Dark, 6.f);
		DrawLine(X, Y, X + 22.f, Y + 22.f, Blade, 3.f);
		// Uc parlaklik.
		DrawLine(X, Y, X + 7.f, Y + 7.f, FLinearColor::White, 2.f);
		// Korkuluk (capraz).
		DrawLine(X + 4.f, Y + 20.f, X + 20.f, Y + 4.f, Gold, 3.f);
		// Sap + topuz.
		DrawLine(X + 22.f, Y + 22.f, X + 30.f, Y + 30.f, Handle, 4.f);
	}
}

void AMMOHUD::AddDamageNumber(const FVector& WorldLocation, float Amount, bool bIsHeal)
{
	FMMOFloatingText FT;
	FT.WorldLocation = WorldLocation;
	FT.Amount = Amount;
	FT.SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	FT.bIsHeal = bIsHeal;
	FloatingTexts.Add(FT);
}

void AMMOHUD::AddChatLine(const FString& Line, const FLinearColor& Color)
{
	FMMOChatLine L;
	L.Text = Line;
	L.Color = Color;
	L.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	ChatLines.Add(L);
	if (ChatLines.Num() > 50)
	{
		ChatLines.RemoveAt(0, ChatLines.Num() - 50);
	}
}

void AMMOHUD::DrawChat()
{
	if (!Canvas || !GetWorld())
	{
		return;
	}
	const AMMOPlayerController* PC = Cast<AMMOPlayerController>(GetOwningPlayerController());
	const bool bOpen = PC && PC->IsChatInputOpen();
	if (ChatLines.Num() == 0 && !bOpen)
	{
		return;
	}

	// Kutu kapaliyken satirlar 15 sn sonra 5 sn'de solar; kutu acikken son 8 satir hep gorunur.
	const float Now = GetWorld()->GetTimeSeconds();
	const int32 MaxLines = 8;
	const float LineH = 18.f;
	const float X = 16.f;
	const float BottomY = (float)Canvas->SizeY - 132.f;

	if (bOpen)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 6.f, BottomY - MaxLines * LineH - 4.f, 412.f, MaxLines * LineH + 8.f);
	}

	const int32 First = FMath::Max(0, ChatLines.Num() - MaxLines);
	for (int32 i = ChatLines.Num() - 1, Row = 0; i >= First; --i, ++Row)
	{
		const FMMOChatLine& L = ChatLines[i];
		const float Age = Now - L.Time;
		float Alpha = 1.f;
		if (!bOpen)
		{
			Alpha = Age < 15.f ? 1.f : 1.f - (Age - 15.f) / 5.f;
		}
		if (Alpha <= 0.f)
		{
			continue;
		}
		FLinearColor Col = L.Color;
		Col.A = Alpha;
		DrawText(L.Text, Col, X, BottomY - (Row + 1) * LineH, nullptr, 1.f);
	}
}

void AMMOHUD::AddFloatingLabel(const FVector& WorldLocation, const FString& Label)
{
	FMMOFloatingText FT;
	FT.WorldLocation = WorldLocation;
	FT.SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	FT.Label = Label;
	FloatingTexts.Add(FT);
}

void AMMOHUD::ShowNpcDialogue(const FString& SpeakerName, const FString& Text)
{
	NpcDialogueSpeaker = SpeakerName;
	NpcDialogueText = Text;
	NpcDialogueStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AMMOHUD::DrawNpcDialogue()
{
	if (!Canvas || NpcDialogueStartTime < -900.f || NpcDialogueText.IsEmpty())
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float Age = Now - NpcDialogueStartTime;
	if (Age > NpcDialogueDuration)
	{
		NpcDialogueStartTime = -1000.f; // suresi doldu, gizle
		return;
	}

	// Son 1 saniyede yumusakca solsun.
	const float Alpha = FMath::Clamp(NpcDialogueDuration - Age, 0.f, 1.f);

	const float W = (float)Canvas->SizeX;
	const float H = (float)Canvas->SizeY;

	// Alt-ortada bir kutu (genislik sabit; yukseklik satir sayisina gore buyur).
	const float BoxW = FMath::Min(760.f, W * 0.6f);
	const float Pad = 16.f;
	const float TextScale = 1.1f;

	// Replik metnini kutu genisligine gore kelime kelime satirlara bol.
	const float MaxTextW = BoxW - 2.f * Pad;
	TArray<FString> Lines;
	{
		TArray<FString> Words;
		NpcDialogueText.ParseIntoArray(Words, TEXT(" "), true);
		FString Current;
		for (const FString& Word : Words)
		{
			const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
			float TW = 0.f, TH = 0.f;
			GetTextSize(Candidate, TW, TH, nullptr, TextScale);
			if (TW > MaxTextW && !Current.IsEmpty())
			{
				Lines.Add(Current);
				Current = Word;
			}
			else
			{
				Current = Candidate;
			}
		}
		if (!Current.IsEmpty())
		{
			Lines.Add(Current);
		}
	}
	if (Lines.Num() == 0)
	{
		Lines.Add(NpcDialogueText);
	}

	// Model "en fazla 2 cumle" istegini asabilir; balon ekrandan tasmasin diye satiri klampla.
	const int32 MaxLines = 5;
	if (Lines.Num() > MaxLines)
	{
		Lines.SetNum(MaxLines);
		Lines[MaxLines - 1] += TEXT(" ...");
	}

	const float LineH = 22.f;
	const float BoxH = 44.f + Lines.Num() * LineH + 12.f; // ad satiri + replik satirlari + alt bosluk
	const float BoxX = (W - BoxW) * 0.5f;
	const float BoxY = FMath::Max(40.f, H - BoxH - 140.f);  // ekranin ustunden tasmasin

	// Zemin + cerceve.
	DrawRect(FLinearColor(0.05f, 0.04f, 0.09f, 0.82f * Alpha), BoxX, BoxY, BoxW, BoxH);
	DrawRect(FLinearColor(0.6f, 0.45f, 0.85f, 0.9f * Alpha), BoxX, BoxY, BoxW, 3.f);                 // ust cizgi
	DrawRect(FLinearColor(0.6f, 0.45f, 0.85f, 0.9f * Alpha), BoxX, BoxY + BoxH - 3.f, BoxW, 3.f);    // alt cizgi

	// Konusan adi (mor tonu) + kaydirilmis replik satirlari (beyaz).
	DrawText(NpcDialogueSpeaker, FLinearColor(0.78f, 0.6f, 1.f, Alpha), BoxX + Pad, BoxY + 10.f, nullptr, 1.2f);
	float LineY = BoxY + 40.f;
	for (const FString& Line : Lines)
	{
		DrawText(Line, FLinearColor(1.f, 1.f, 1.f, Alpha), BoxX + Pad, LineY, nullptr, TextScale);
		LineY += LineH;
	}
}

void AMMOHUD::DrawEnemyHealthBars()
{
	UWorld* World = GetWorld();
	if (!Canvas || !World)
	{
		return;
	}

	const APawn* PlayerPawn = GetOwningPawn();

	for (TActorIterator<AMMOCharacter> It(World); It; ++It)
	{
		AMMOCharacter* C = *It;
		if (!C || C == PlayerPawn)
		{
			continue;
		}

		UMMOStatsComponent* S = C->GetStats();
		if (!S || S->bIsDead)
		{
			continue;
		}

		// Kafanin biraz uzerini ekrana projekte et.
		const FVector Head = C->GetActorLocation() + FVector(0.f, 0.f, 105.f);
		const FVector Screen = Canvas->Project(Head);
		if (Screen.Z <= 0.f)
		{
			continue; // kamera arkasinda
		}

		const float BarW = 62.f;
		const float BarH = 7.f;
		const float BX = Screen.X - BarW * 0.5f;
		const float BY = Screen.Y;

		// Isim + seviye plakasi (bar ustunde). Elit mob'lar (arketip DB'den, istemcide de
		// erisilebilir - ArchetypeId zaten replike) bug-hunt: bIsElite'in kendisi replike
		// DEGIL ama buna gerek yok, ArchetypeId + statik DB istemcide de ayni cevabi verir.
		// Oncesinde elit mob'lar (Orumcek Ana, Kadim Golem vb.) normal moblarla AYNI
		// gorunuyordu - oyuncunun tehlikeyi onceden anlamasinin hicbir yolu yoktu.
		if (const AMMOMob* Mob = Cast<AMMOMob>(C))
		{
			const FMMOMobArchetype* Arch = FMMOMobDatabase::Find(Mob->ArchetypeId);
			const bool bElite = Arch && Arch->bIsElite;
			const FString Plate = bElite
				? FString::Printf(TEXT("[ELIT] %s Lv%d"), *Mob->GetMobName(), Mob->GetMobLevel())
				: FString::Printf(TEXT("%s Lv%d"), *Mob->GetMobName(), Mob->GetMobLevel());
			DrawText(Plate, bElite ? FLinearColor(1.f, 0.55f, 0.15f) : FLinearColor(1.f, 0.75f, 0.7f),
				BX, BY - 15.f, nullptr, bElite ? 0.8f : 0.72f);
		}

		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f), BX - 1.f, BY - 1.f, BarW + 2.f, BarH + 2.f);
		DrawRect(FLinearColor(0.15f, 0.15f, 0.16f, 0.9f), BX, BY, BarW, BarH);
		DrawRect(FLinearColor(0.85f, 0.12f, 0.12f, 1.f), BX, BY, BarW * S->GetHealthPercent(), BarH);
	}
}

void AMMOHUD::DrawNearbyThreat(AMMOCharacter* PlayerChar)
{
	UWorld* World = GetWorld();
	UMMOStatsComponent* MyStats = PlayerChar ? PlayerChar->GetStats() : nullptr;
	if (!Canvas || !World || !MyStats || MyStats->bIsDead)
	{
		return;
	}

	// Istemcide spawner'lar yok (replike degil); yakindaki en yakin canli mob bolgeyi anlatir.
	const AMMOMob* Nearest = nullptr;
	float BestSq = 3000.f * 3000.f;
	for (TActorIterator<AMMOMob> It(World); It; ++It)
	{
		const AMMOMob* M = *It;
		if (!M || !M->GetStats() || M->GetStats()->bIsDead)
		{
			continue;
		}
		const float D = FVector::DistSquared(M->GetActorLocation(), PlayerChar->GetActorLocation());
		if (D < BestSq)
		{
			BestSq = D;
			Nearest = M;
		}
	}
	if (!Nearest)
	{
		return;
	}

	// Zorluk: oyuncu seviyesine gore. >10 ust: olumcul, >5: tehlikeli, <-5: gri (XP duser).
	const int32 Diff = Nearest->GetMobLevel() - MyStats->Level;
	FLinearColor Col = FLinearColor(0.85f, 0.85f, 0.85f);
	const TCHAR* Tag = TEXT("");
	if (Diff > 10)      { Col = FLinearColor(1.f, 0.2f, 0.2f);  Tag = TEXT("  [COK TEHLIKELI]"); }
	else if (Diff > 5)  { Col = FLinearColor(1.f, 0.55f, 0.15f); Tag = TEXT("  [tehlikeli]"); }
	else if (Diff < -5) { Col = FLinearColor(0.55f, 0.55f, 0.55f); Tag = TEXT("  [dusuk XP]"); }
	else                { Col = FLinearColor(0.7f, 0.95f, 0.7f); }

	const FMMOMobArchetype* NearestArch = FMMOMobDatabase::Find(Nearest->ArchetypeId);
	const FString Txt = FString::Printf(TEXT("Yakinda: %s Lv%d%s%s"), *Nearest->GetMobName(), Nearest->GetMobLevel(),
		(NearestArch && NearestArch->bIsElite) ? TEXT(" (elit)") : TEXT(""), Tag);
	DrawText(Txt, Col, (float)Canvas->SizeX * 0.5f - 130.f, 14.f, nullptr, 1.0f);
}

void AMMOHUD::DrawTargetBar()
{
	if (!Canvas)
	{
		return;
	}

	const AMMOPlayerController* MPC = Cast<AMMOPlayerController>(PlayerOwner);
	AMMOCharacter* Target = MPC ? MPC->GetSelectedTarget() : nullptr;
	if (!Target)
	{
		return; // KO: hedef yoksa/olduyse target bar gizlenir
	}

	UMMOStatsComponent* S = Target->GetStats();
	if (!S)
	{
		return;
	}

	// --- Ust-orta hedef paneli (KO m_pUITargetBar) ---
	const float BarW = 260.f;
	const float BarH = 18.f;
	const float BX = ((float)Canvas->SizeX - BarW) * 0.5f;
	const float BY = 26.f;

	// Ad + seviye (dusman kirmizisi — KO hedef rengi). Elit isareti nameplate'le tutarli.
	FString TargetName = TEXT("Canavar");
	bool bTargetElite = false;
	if (const AMMOMob* Mob = Cast<AMMOMob>(Target))
	{
		TargetName = Mob->GetMobName();
		const FMMOMobArchetype* Arch = FMMOMobDatabase::Find(Mob->ArchetypeId);
		bTargetElite = Arch && Arch->bIsElite;
	}
	const FString TargetPlate = FString::Printf(TEXT("%s%s  Lv %d"), bTargetElite ? TEXT("[ELIT] ") : TEXT(""), *TargetName, S->Level);
	DrawText(TargetPlate, bTargetElite ? FLinearColor(1.f, 0.6f, 0.15f) : FLinearColor(1.f, 0.35f, 0.3f),
		BX, BY - 18.f, nullptr, 1.f);

	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f), BX - 2.f, BY - 2.f, BarW + 4.f, BarH + 4.f);
	DrawRect(FLinearColor(0.15f, 0.15f, 0.16f, 0.9f), BX, BY, BarW, BarH);
	DrawRect(FLinearColor(0.85f, 0.15f, 0.15f, 1.f), BX, BY, BarW * S->GetHealthPercent(), BarH);
	DrawText(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(S->Health), FMath::RoundToInt(S->MaxHealth)),
		FLinearColor::White, BX + BarW * 0.5f - 30.f, BY + 1.f, nullptr, 0.85f);

	// --- Secili hedefin tepesine isaret (asagi bakan kirmizi ucgen) ---
	const FVector Head = Target->GetActorLocation() + FVector(0.f, 0.f, 150.f);
	const FVector Screen = Canvas->Project(Head);
	if (Screen.Z > 0.f)
	{
		const FLinearColor MarkCol(1.f, 0.25f, 0.2f, 1.f);
		const float MX = Screen.X;
		const float MY = Screen.Y;
		// Ucgen: iki ust kose -> alt uc.
		DrawLine(MX - 9.f, MY - 14.f, MX + 9.f, MY - 14.f, MarkCol, 3.f);
		DrawLine(MX - 9.f, MY - 14.f, MX, MY, MarkCol, 3.f);
		DrawLine(MX + 9.f, MY - 14.f, MX, MY, MarkCol, 3.f);
	}
}

void AMMOHUD::DrawFloatingTexts()
{
	UWorld* World = GetWorld();
	if (!Canvas || !World || FloatingTexts.Num() == 0)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	// Serbest etiketler (XP kazanci vb.) hasar sayisindan uzun kalir.

	// Geriye dogru gez (suresi dolani guvenle sil).
	for (int32 i = FloatingTexts.Num() - 1; i >= 0; --i)
	{
		const FMMOFloatingText& FT = FloatingTexts[i];
		const float Lifetime = FT.Label.IsEmpty() ? 1.1f : 2.0f;
		const float Age = Now - FT.SpawnTime;
		if (Age >= Lifetime)
		{
			FloatingTexts.RemoveAt(i);
			continue;
		}

		const float Alpha = 1.f - (Age / Lifetime);
		const FVector RisenLoc = FT.WorldLocation + FVector(0.f, 0.f, Age * 75.f);
		const FVector Screen = Canvas->Project(RisenLoc);
		if (Screen.Z <= 0.f)
		{
			continue;
		}

		if (!FT.Label.IsEmpty())
		{
			DrawText(FT.Label, FLinearColor(0.6f, 0.8f, 1.f, Alpha), Screen.X - 30.f, Screen.Y, nullptr, 1.2f);
			continue;
		}

		const FString Txt = FT.bIsHeal
			? FString::Printf(TEXT("+%d"), FMath::RoundToInt(FT.Amount))
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(FT.Amount));
		const FLinearColor Col = FT.bIsHeal ? FLinearColor(0.35f, 0.95f, 0.4f, Alpha) : FLinearColor(1.f, 0.85f, 0.2f, Alpha);
		DrawText(Txt, Col, Screen.X - 8.f, Screen.Y, nullptr, 1.4f);
	}
}

void AMMOHUD::ToggleSkillBook()
{
	// Pencereler artik bagimsiz: digerlerini kapatma (ayni anda birden fazla acilabilir).
	bSkillBookOpen = !bSkillBookOpen;
	if (!bSkillBookOpen)
	{
		PendingAssignSkill = NAME_None;
	}
}

void AMMOHUD::ToggleInventory()
{
	bInventoryOpen = !bInventoryOpen;
}

void AMMOHUD::DrawHotbar(AMMOCharacter* PlayerChar)
{
	HotbarRegions.Reset();
	if (!Canvas || !PlayerChar)
	{
		return;
	}
	UMMOSkillComponent* SkillComp = PlayerChar->GetSkillComponent();
	if (!SkillComp)
	{
		return;
	}

	const int32 Count = UMMOSkillComponent::HotbarSize;
	const float SlotW = 62.f;
	const float SlotH = 56.f;
	const float Gap = 6.f;
	const float TotalW = Count * SlotW + (Count - 1) * Gap;
	const float StartX = ((float)Canvas->SizeX - TotalW) * 0.5f;
	const float Y = (float)Canvas->SizeY - SlotH - 16.f;

	const TCHAR* Keys[10] = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"),
		TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("0") };

	DrawText(TEXT("K: Skill Kitabi"), FLinearColor(0.7f, 0.8f, 1.f), StartX, Y - 20.f, nullptr, 0.9f);

	for (int32 i = 0; i < Count; ++i)
	{
		const float SX = StartX + i * (SlotW + Gap);

		DrawRect(FLinearColor(0.05f, 0.05f, 0.07f, 0.85f), SX, Y, SlotW, SlotH);
		DrawRect(FLinearColor(0.25f, 0.35f, 0.55f, 0.9f), SX, Y, SlotW, 2.f);

		FMMOClickRegion R;
		R.Slot = i;
		R.Rect = FBox2D(FVector2D(SX, Y), FVector2D(SX + SlotW, Y + SlotH));
		HotbarRegions.Add(R);

		DrawText(Keys[i], FLinearColor(1.f, 0.85f, 0.2f), SX + 4.f, Y + 3.f, nullptr, 0.85f);

		const FName Id = SkillComp->GetSlotSkill(i);
		if (!Id.IsNone())
		{
			if (const FMMOSkillDef* Def = FMMOSkillDatabase::Find(Id))
			{
				DrawText(Def->Name.Left(8), FLinearColor::White, SX + 3.f, Y + 24.f, nullptr, 0.62f);
				const float Cd = SkillComp->GetCooldownRemaining(Id);
				if (Cd > 0.f)
				{
					DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), SX, Y, SlotW, SlotH);
					DrawText(FString::Printf(TEXT("%.0f"), FMath::CeilToFloat(Cd)),
						FLinearColor(1.f, 0.6f, 0.6f), SX + SlotW * 0.5f - 6.f, Y + SlotH * 0.5f - 8.f, nullptr, 1.2f);
				}
			}
		}

		if (bSkillBookOpen)
		{
			DrawRect(FLinearColor(0.9f, 0.8f, 0.2f, 0.15f), SX, Y, SlotW, SlotH);
		}
	}
}

void AMMOHUD::DrawSkillBook(AMMOCharacter* PlayerChar)
{
	SkillBookRegions.Reset();
	if (!Canvas || !PlayerChar)
	{
		return;
	}
	UMMOSkillComponent* SkillComp = PlayerChar->GetSkillComponent();
	UMMOStatsComponent* Stats = PlayerChar->GetStats();
	if (!SkillComp || !Stats)
	{
		return;
	}

	const float PW = 470.f;
	const float PH = 486.f;
	const float PX = 60.f;
	const float PY = 90.f;
	DrawRect(FLinearColor(0.03f, 0.03f, 0.05f, 0.93f), PX, PY, PW, PH);
	DrawRect(FLinearColor(0.3f, 0.5f, 0.8f, 0.95f), PX, PY, PW, 4.f);
	SkillBookRect = FBox2D(FVector2D(PX, PY), FVector2D(PX + PW, PY + PH));
	SkillBookCloseRect = DrawCloseButton(PX, PY, PW);

	const FString ClassName = GetMMOClassDisplayName(SkillComp->CharacterClass);

	DrawText(FString::Printf(TEXT("SKILL KITABI - %s"), *ClassName), FLinearColor::White, PX + 14.f, PY + 10.f, nullptr, 1.3f);
	DrawText(FString::Printf(TEXT("Skill Puani: %d     Level: %d"), Stats->UnspentSkillPoints, Stats->Level),
		FLinearColor(1.f, 0.85f, 0.2f), PX + 14.f, PY + 36.f, nullptr, 1.f);
	DrawText(TEXT("Skile tikla = agacina 1 rutbe yatir.  Ogrenilene tikla = ele al, hotbar slotuna tikla = ata."),
		FLinearColor(0.7f, 0.75f, 0.8f), PX + 14.f, PY + 58.f, nullptr, 0.75f);

	const TArray<FMMOSkillDef> ClassSkills = FMMOSkillDatabase::GetForClass(SkillComp->CharacterClass);

	const EMMOSkillTree Trees[5] = { EMMOSkillTree::Basic, EMMOSkillTree::Special0, EMMOSkillTree::Special1, EMMOSkillTree::Special2, EMMOSkillTree::Special3 };
	const TCHAR* TreeNames[5] = { TEXT("Temel"), TEXT("Ustalik 1"), TEXT("Ustalik 2"), TEXT("Ustalik 3"), TEXT("Ustalik 4") };

	// Skilleri agaclara onceden dagit (hem sayim hem cizim ayni veriyi kullansin).
	TArray<TArray<const FMMOSkillDef*>> TreeSkills;
	TreeSkills.SetNum(5);
	int32 ActiveTreeCount = 0;
	int32 TotalSkillCount = 0;
	for (int32 t = 0; t < 5; ++t)
	{
		for (const FMMOSkillDef& D : ClassSkills)
		{
			if (D.Tree == Trees[t])
			{
				TreeSkills[t].Add(&D);
			}
		}
		// Listeleme duzeni: agac icinde seviyeye gore (esitlikte tablo sirasi korunur); tablo sirasi
		// yeni eklenen ust-kademe becerileri (Lv45/58) Lv28-39'un onune dusuruyordu. Yalniz cizim
		// sirasi degisir; tiklanabilir bolgeler cizim konumundan uretildigi icin birlikte tasinir.
		TreeSkills[t].StableSort([](const FMMOSkillDef& A, const FMMOSkillDef& B)
		{
			return A.RequiredLevel < B.RequiredLevel;
		});
		if (TreeSkills[t].Num() > 0)
		{
			++ActiveTreeCount;
			TotalSkillCount += TreeSkills[t].Num();
		}
	}

	const float HeaderH = 22.f;
	const float TreeGapH = 6.f;
	const float ColGapW = 12.f;

	// QA denetimi (2026-09-14, commit 2e401c2 sonrasi BULGU #3): 57 skil'e ciktiktan sonra
	// tek sutunlu duzende bazi siniflar (16 skil, 5 agac) RowH'in okunabilirlik tabanina
	// (20px, iki satirlik hucre icin gereken minimum) carpip panel disina tasiyordu — "asla
	// tasmaz" varsayimi artik gecersizdi. Sabit RowH'i daha da kucultmek yerine (metin
	// cakismasina yol acardi) shop'un buy/sell ikili-sutun desenini burada da kullanip
	// agaclari 2 sutuna dagitiyoruz; boylece sutun basina dusen satir sayisi yariya iner ve
	// mevcut RowH araligi (20-32px) her sinif icin tekrar yeterli olur.
	const bool bTwoColumns = ActiveTreeCount > 1;
	const float ColW = bTwoColumns ? (PW - 28.f - ColGapW) / 2.f : (PW - 28.f);
	const float Col0X = PX + 14.f;
	const float Col1X = Col0X + ColW + ColGapW;

	// Agaclari iki sutuna dengeli dagit (o ana kadar hangi sutunun toplam skil sayisi
	// azsa oraya ekle) — sutunlarin RowH'i birbirinden cok sapmasin diye.
	TArray<int32> ColOfTree;
	ColOfTree.Init(0, 5);
	int32 ColSkillCount[2] = { 0, 0 };
	int32 ColTreeCount[2] = { 0, 0 };
	for (int32 t = 0; t < 5; ++t)
	{
		if (TreeSkills[t].Num() == 0)
		{
			continue;
		}
		const int32 Col = (!bTwoColumns || ColSkillCount[0] <= ColSkillCount[1]) ? 0 : 1;
		ColOfTree[t] = Col;
		ColSkillCount[Col] += TreeSkills[t].Num();
		++ColTreeCount[Col];
	}

	// Satir yuksekligini sutun basina mevcut alana gore daralt (shop/quest/inventory ile ayni
	// felsefe): her sutun kendi skil/agac sayisina gore kendi RowH'ini hesaplar.
	const float AvailableH = (PY + PH - 40.f) - (PY + 82.f); // altta "hotbara ata" mesaji icin pay birak
	float ColRowH[2];
	for (int32 c = 0; c < 2; ++c)
	{
		const float OverheadH = ColTreeCount[c] * (HeaderH + TreeGapH);
		ColRowH[c] = FMath::Clamp((AvailableH - OverheadH) / (float)FMath::Max(1, ColSkillCount[c]), 20.f, 32.f);
	}

	float ColRowY[2] = { PY + 82.f, PY + 82.f };

	for (int32 t = 0; t < 5; ++t)
	{
		const TArray<const FMMOSkillDef*>& InTree = TreeSkills[t];
		if (InTree.Num() == 0)
		{
			continue;
		}

		const int32 Col = ColOfTree[t];
		const float RowX = (Col == 0) ? Col0X : Col1X;
		const float RowW = ColW;
		const float RowH = ColRowH[Col];
		float& RowY = ColRowY[Col];

		// Agac basliginda o agacin guncel mastery rutbesi (KO: skill puani agaca yatirilir).
		DrawText(FString::Printf(TEXT("%s (Rutbe: %d)"), TreeNames[t], SkillComp->GetMasteryRank(Trees[t])),
			FLinearColor(0.6f, 0.8f, 1.f), RowX, RowY, nullptr, 0.95f);
		RowY += HeaderH;

		for (const FMMOSkillDef* D : InTree)
		{
			const bool bLearned = SkillComp->IsLearned(D->SkillId);
			FString Reason;
			const bool bCanLearn = !bLearned && SkillComp->CanLearn(D->SkillId, Reason);

			FLinearColor RowCol = bLearned ? FLinearColor(0.1f, 0.24f, 0.1f, 0.9f)
				: (bCanLearn ? FLinearColor(0.1f, 0.15f, 0.28f, 0.9f) : FLinearColor(0.12f, 0.12f, 0.12f, 0.85f));
			if (PendingAssignSkill == D->SkillId)
			{
				RowCol = FLinearColor(0.42f, 0.36f, 0.1f, 0.95f);
			}
			DrawRect(RowCol, RowX, RowY, RowW, RowH - 4.f);

			FMMOClickRegion R;
			R.SkillId = D->SkillId;
			R.Rect = FBox2D(FVector2D(RowX, RowY), FVector2D(RowX + RowW, RowY + RowH - 4.f));
			SkillBookRegions.Add(R);

			// Kilitli skillerde CanLearn zaten "Rutbe X gerekli" sebebini uretir (KO mastery gating).
			const FString Status = bLearned ? TEXT("[Ogrenildi]") : (bCanLearn ? TEXT("[Ogren]") : FString::Printf(TEXT("[%s]"), *Reason));
			DrawText(FString::Printf(TEXT("%s   %s"), *D->Name, *Status), FLinearColor::White, RowX + 6.f, RowY + 3.f, nullptr, 0.82f);
			DrawText(FString::Printf(TEXT("Lv%d  Rutbe%d  Mana%d"), D->RequiredLevel, D->RequiredMasteryRank, FMath::RoundToInt(D->ManaCost)),
				FLinearColor(0.7f, 0.75f, 0.85f), RowX + 6.f, RowY + 16.f, nullptr, 0.68f);

			RowY += RowH;
		}
		RowY += TreeGapH;
	}

	if (!PendingAssignSkill.IsNone())
	{
		const FMMOSkillDef* PD = FMMOSkillDatabase::Find(PendingAssignSkill);
		DrawText(FString::Printf(TEXT(">> '%s' elde. Bir hotbar slotuna tikla."), PD ? *PD->Name : TEXT("?")),
			FLinearColor(1.f, 0.9f, 0.3f), PX + 14.f, PY + PH - 26.f, nullptr, 0.9f);
	}
}

bool AMMOHUD::HandleSkillBookClick(const FVector2D& Pos)
{
	if (!bSkillBookOpen)
	{
		return false;
	}

	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	UMMOSkillComponent* SkillComp = Char ? Char->GetSkillComponent() : nullptr;
	if (!SkillComp)
	{
		return true; // kitap acik: yine de hareketi engelle
	}

	// 1) Hotbar slotuna mi tiklandi?
	for (const FMMOClickRegion& R : HotbarRegions)
	{
		if (R.Slot >= 0 && R.Rect.IsInside(Pos))
		{
			if (!PendingAssignSkill.IsNone())
			{
				SkillComp->AssignToSlot(R.Slot, PendingAssignSkill);
				PendingAssignSkill = NAME_None;
			}
			else
			{
				SkillComp->ClearSlot(R.Slot);
			}
			return true;
		}
	}

	// 2) Skill satirina mi tiklandi?
	for (const FMMOClickRegion& R : SkillBookRegions)
	{
		if (!R.SkillId.IsNone() && R.Rect.IsInside(Pos))
		{
			if (SkillComp->IsLearned(R.SkillId))
			{
				// Ele al + suruklemeyi baslat (birakinca slota duser).
				PendingAssignSkill = R.SkillId;
				DraggingSkill = R.SkillId;
				bDragging = true;
			}
			else
			{
				SkillComp->LearnSkill(R.SkillId);
			}
			return true;
		}
	}

	// Kitap acikken bosluga tiklama da hareketi engellesin.
	return true;
}

void AMMOHUD::HandleSkillBookRelease(const FVector2D& Pos)
{
	if (!bDragging)
	{
		return;
	}

	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	UMMOSkillComponent* SkillComp = Char ? Char->GetSkillComponent() : nullptr;

	if (SkillComp && !DraggingSkill.IsNone())
	{
		for (const FMMOClickRegion& R : HotbarRegions)
		{
			if (R.Slot >= 0 && R.Rect.IsInside(Pos))
			{
				SkillComp->AssignToSlot(R.Slot, DraggingSkill);
				PendingAssignSkill = NAME_None;
				break;
			}
		}
	}

	bDragging = false;
	DraggingSkill = NAME_None;
}

bool AMMOHUD::HandleSkillBookRightClick(const FVector2D& Pos)
{
	if (!bSkillBookOpen)
	{
		return false;
	}

	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	UMMOSkillComponent* SkillComp = Char ? Char->GetSkillComponent() : nullptr;
	if (!SkillComp)
	{
		return false;
	}

	for (const FMMOClickRegion& R : SkillBookRegions)
	{
		if (!R.SkillId.IsNone() && R.Rect.IsInside(Pos) && SkillComp->IsLearned(R.SkillId))
		{
			// KO tarzi: ilk bos slota otomatik yerlestir.
			for (int32 Slot = 0; Slot < UMMOSkillComponent::HotbarSize; ++Slot)
			{
				if (SkillComp->GetSlotSkill(Slot).IsNone())
				{
					SkillComp->AssignToSlot(Slot, R.SkillId);
					return true;
				}
			}
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Hotbar dolu - once bir slotu bosalt"));
			}
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------
// Envanter / Ekipman penceresi
// ----------------------------------------------------------------------

static const TCHAR* GMMOSlotNames[(int32)EMMOEquipSlot::MAX] = { TEXT("Silah"), TEXT("Zirh"), TEXT("Kask"), TEXT("Bot") };

void AMMOHUD::DrawInventory(AMMOCharacter* PlayerChar)
{
	InvEquippedRegions.Reset();
	InvBagRegions.Reset();
	InvConsumableRegions.Reset();
	if (!Canvas || !PlayerChar)
	{
		return;
	}

	UMMOInventoryComponent* Inv = PlayerChar->GetInventory();
	UMMOStatsComponent* Stats = PlayerChar->GetStats();
	if (!Inv || !Stats)
	{
		return;
	}

	const float PW = 470.f;
	const float PH = 500.f;
	const float PX = (float)Canvas->SizeX - PW - 60.f;
	const float PY = 90.f;
	DrawRect(FLinearColor(0.03f, 0.03f, 0.05f, 0.93f), PX, PY, PW, PH);
	DrawRect(FLinearColor(0.8f, 0.6f, 0.2f, 0.95f), PX, PY, PW, 4.f);
	InventoryRect = FBox2D(FVector2D(PX, PY), FVector2D(PX + PW, PY + PH));
	InventoryCloseRect = DrawCloseButton(PX, PY, PW);

	DrawText(TEXT("ENVANTER - EKIPMAN"), FLinearColor::White, PX + 14.f, PY + 10.f, nullptr, 1.3f);
	DrawText(FString::Printf(TEXT("Altin: %d     Atk: %d   Def: %d"),
		Inv->Gold, FMath::RoundToInt(Stats->AttackPower), FMath::RoundToInt(Stats->Defense)),
		FLinearColor(1.f, 0.85f, 0.2f), PX + 14.f, PY + 36.f, nullptr, 1.f);
	DrawText(TEXT("Sol tik: kusan / cikar.   Cantada SAG tik: +1 dene (yakma riski!)"),
		FLinearColor(0.7f, 0.75f, 0.8f), PX + 14.f, PY + 58.f, nullptr, 0.72f);

	const float RowX = PX + 14.f;
	const float RowW = PW - 28.f;
	const float RowH = 28.f;
	float RowY = PY + 82.f;

	// --- Kusanilanlar ---
	DrawText(TEXT("KUSANILAN"), FLinearColor(0.6f, 0.8f, 1.f), RowX, RowY, nullptr, 0.95f);
	RowY += 20.f;

	for (int32 s = 0; s < (int32)EMMOEquipSlot::MAX; ++s)
	{
		const FMMOEquipmentInstance Inst = Inv->GetEquipped((EMMOEquipSlot)s);
		const bool bHas = Inst.IsValidInstance();

		DrawRect(bHas ? FLinearColor(0.1f, 0.2f, 0.12f, 0.9f) : FLinearColor(0.1f, 0.1f, 0.12f, 0.85f),
			RowX, RowY, RowW, RowH - 4.f);

		FMMOClickRegion R;
		R.Slot = s;
		R.Rect = FBox2D(FVector2D(RowX, RowY), FVector2D(RowX + RowW, RowY + RowH - 4.f));
		InvEquippedRegions.Add(R);

		FString Line = FString::Printf(TEXT("%s:  "), GMMOSlotNames[s]);
		FLinearColor LineColor = FLinearColor::White;
		if (bHas)
		{
			if (const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId))
			{
				const float RM = FMMOItemDatabase::GetRarityMultiplier(Inst.Rarity);
				LineColor = FMMOItemDatabase::GetRarityColor(Inst.Rarity);
				Line += FString::Printf(TEXT("[%s] %s +%d  (Atk %d / Def %d)"),
					*FMMOItemDatabase::GetRarityName(Inst.Rarity), *Def->Name, Inst.Plus,
					FMath::RoundToInt(FMMOItemDatabase::GetAttack(*Def, Inst.Plus) * RM),
					FMath::RoundToInt(FMMOItemDatabase::GetDefense(*Def, Inst.Plus) * RM));
			}
		}
		else
		{
			Line += TEXT("(bos)");
		}
		DrawText(Line, LineColor, RowX + 6.f, RowY + 4.f, nullptr, 0.8f);
		RowY += RowH;
	}

	RowY += 8.f;

	// --- Canta + Tuketilebilirler: ortak alan payi ---
	// Iki degisken-uzunluklu liste ayni sabit-yukseklikli panelde: satir yuksekligini
	// TOPLAM satir sayisina gore ortaklasa kucult (skill book'taki agac-toplami deseniyle
	// ayni) - yoksa ikisinden biri buyudukce panel disina tasabilirdi.
	const int32 BagCount = Inv->EquipmentBag.Num();
	const int32 ConsumableCount = Inv->Items.Num();
	// Canta alani HER ZAMAN tam bir baslik satiri kullanir: doluysa "CANTA (N)" basligi,
	// bosken ayni yukseklikte bir ipucu metni - ikisi karsilikli dislayici oldugu icin tek
	// satir olarak butceye dahil edilir (bug-hunt: bos-canta ipucu daha once bu butcenin
	// DISINDA cizilip SharedRowH kadar ekstra yer kapliyordu - az sayida tuketilebilirle
	// birlikte panelin altindan hafifce tasmaya sebep olabilirdi).
	const int32 SectionHeaderCount = 1 + (ConsumableCount > 0 ? 1 : 0);
	const float SectionHeaderH = 20.f;
	const float SharedAvailH = FMath::Max(0.f, (PY + PH - 12.f) - RowY - SectionHeaderCount * SectionHeaderH);
	const float SharedRowH = FMath::Min(RowH, SharedAvailH / (float)FMath::Max(1, BagCount + ConsumableCount));
	const float SharedCellH = FMath::Max(1.f, SharedRowH - 4.f);

	// --- Canta ---
	if (BagCount > 0)
	{
		DrawText(FString::Printf(TEXT("CANTA (%d)"), BagCount), FLinearColor(0.6f, 0.8f, 1.f), RowX, RowY, nullptr, 0.95f);
	}
	else
	{
		DrawText(TEXT("(canta bos - moblardan yesil kureler dusur)"), FLinearColor(0.6f, 0.6f, 0.6f), RowX, RowY, nullptr, 0.75f);
	}
	RowY += SectionHeaderH;

	for (int32 i = 0; i < BagCount; ++i)
	{
		const FMMOEquipmentInstance& Inst = Inv->EquipmentBag[i];
		const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId);
		if (!Def)
		{
			continue;
		}

		const bool bMaxed = Inst.Plus >= FMMOItemDatabase::MaxPlus;
		DrawRect(FLinearColor(0.1f, 0.13f, 0.2f, 0.9f), RowX, RowY, RowW, SharedCellH);

		FMMOClickRegion R;
		R.Slot = i;
		R.InstanceId = Inst.InstanceId; // RPC'ler indeks degil kimlik gonderir (NETWORK_AUDIT Y4)
		R.Plus = Inst.Plus;
		R.Rect = FBox2D(FVector2D(RowX, RowY), FVector2D(RowX + RowW, RowY + SharedCellH));
		InvBagRegions.Add(R);

		const float RM = FMMOItemDatabase::GetRarityMultiplier(Inst.Rarity);
		const FLinearColor RowColor = FMMOItemDatabase::GetRarityColor(Inst.Rarity);
		FString Line = FString::Printf(TEXT("[%s] %s +%d  (Atk %d / Def %d, Lv%d)"),
			*FMMOItemDatabase::GetRarityName(Inst.Rarity), *Def->Name, Inst.Plus,
			FMath::RoundToInt(FMMOItemDatabase::GetAttack(*Def, Inst.Plus) * RM),
			FMath::RoundToInt(FMMOItemDatabase::GetDefense(*Def, Inst.Plus) * RM),
			Def->RequiredLevel);

		if (!bMaxed)
		{
			const int32 BurnPct = FMMOItemDatabase::GetBurnChancePct(Inst.Plus);
			const FString BurnTag = BurnPct > 0
				? FString::Printf(TEXT(" YANMA %%%d"), BurnPct)
				: FString();
			Line += FString::Printf(TEXT("   [+%d: %da %%%d%s]"), Inst.Plus + 1,
				FMMOItemDatabase::GetUpgradeCost(Inst.Plus),
				FMMOItemDatabase::GetUpgradeChancePct(Inst.Plus),
				*BurnTag);
		}
		else
		{
			Line += TEXT("   [MAX]");
		}

		DrawText(Line, RowColor, RowX + 6.f, RowY + 4.f, nullptr, 0.72f);
		RowY += SharedRowH;
	}

	// --- Tuketilebilirler (Can/Mana iksirleri, buyuk iksirler, parsomenler) ---
	// bug-hunt: satici bunlarin hepsini satiyordu ama Can Iksiri (F) / Mana Iksiri (G)
	// disindakileri kullanmanin HICBIR yolu yoktu (satin alinip sonsuza dek envanterde
	// kullanilmadan kalirlardi). Sol tik = kullan (UseHealthPotion/UseConsumable).
	if (ConsumableCount > 0)
	{
		DrawText(FString::Printf(TEXT("TUKETILEBILIRLER (%d) - sol tik: kullan"), ConsumableCount),
			FLinearColor(0.6f, 1.f, 0.7f), RowX, RowY, nullptr, 0.95f);
		RowY += SectionHeaderH;

		for (const FMMOItemStack& Stack : Inv->Items)
		{
			DrawRect(FLinearColor(0.12f, 0.18f, 0.12f, 0.9f), RowX, RowY, RowW, SharedCellH);

			FMMOClickRegion R;
			R.SkillId = Stack.ItemId;
			R.Rect = FBox2D(FVector2D(RowX, RowY), FVector2D(RowX + RowW, RowY + SharedCellH));
			InvConsumableRegions.Add(R);

			FString DisplayName;
			if (const FMMOConsumableDef* CDef = FMMOConsumableDatabase::Find(Stack.ItemId))
			{
				DisplayName = CDef->Name;
			}
			else if (Stack.ItemId == FName(TEXT("HealthPotion")))
			{
				DisplayName = TEXT("Can Iksiri");
			}
			else
			{
				DisplayName = Stack.ItemId.ToString();
			}

			DrawText(FString::Printf(TEXT("%s  x%d"), *DisplayName, Stack.Count),
				FLinearColor::White, RowX + 6.f, RowY + 4.f, nullptr, 0.78f);
			RowY += SharedRowH;
		}
	}
}

bool AMMOHUD::HandleInventoryClick(const FVector2D& Pos)
{
	if (!bInventoryOpen)
	{
		return false;
	}

	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	UMMOInventoryComponent* Inv = Char ? Char->GetInventory() : nullptr;
	if (!Inv)
	{
		return true;
	}

	for (const FMMOClickRegion& R : InvEquippedRegions)
	{
		if (R.Slot >= 0 && R.Rect.IsInside(Pos))
		{
			Inv->UnequipSlot((EMMOEquipSlot)R.Slot);
			return true;
		}
	}

	for (const FMMOClickRegion& R : InvBagRegions)
	{
		if (R.Slot >= 0 && R.Rect.IsInside(Pos))
		{
			Inv->EquipByInstanceId(R.InstanceId);
			return true;
		}
	}

	for (const FMMOClickRegion& R : InvConsumableRegions)
	{
		if (!R.SkillId.IsNone() && R.Rect.IsInside(Pos))
		{
			// Can Iksiri (HealthPotion) ayri, eski bir sistem uzerinden calisir (F tusuyla
			// ayni yol); geri kalan tum tuketilebilirler (Mana/Buyuk iksirler, parsomenler)
			// genel UseConsumable yoluna gider.
			if (R.SkillId == FName(TEXT("HealthPotion")))
			{
				Char->UseHealthPotion();
			}
			else
			{
				Char->UseConsumable(R.SkillId);
			}
			return true;
		}
	}

	// Pencere acikken bosluga tiklama da hareketi engellesin.
	return true;
}

bool AMMOHUD::HandleInventoryRightClick(const FVector2D& Pos)
{
	if (!bInventoryOpen)
	{
		return false;
	}

	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	UMMOInventoryComponent* Inv = Char ? Char->GetInventory() : nullptr;
	if (!Inv)
	{
		return false;
	}

	for (const FMMOClickRegion& R : InvBagRegions)
	{
		if (R.Slot >= 0 && R.Rect.IsInside(Pos))
		{
			Inv->UpgradeByInstanceId(R.InstanceId, R.Plus);
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------
// Satici dukkani
// ----------------------------------------------------------------------

void AMMOHUD::OpenShop()
{
	ShopBuyPage = 0;
	ShopSellPage = 0;
	ShopBuyRegions.Reset();
	ShopSellRegions.Reset();
	ShopBuyPrevRect = FBox2D(ForceInit);
	ShopBuyNextRect = FBox2D(ForceInit);
	ShopSellPrevRect = FBox2D(ForceInit);
	ShopSellNextRect = FBox2D(ForceInit);
	bShopOpen = true;
}

void AMMOHUD::CloseShop()
{
	bShopOpen = false;
}

void AMMOHUD::DrawShop(AMMOCharacter* PlayerChar)
{
	ShopBuyRegions.Reset();
	ShopSellRegions.Reset();
	ShopBuyPrevRect = FBox2D(ForceInit);
	ShopBuyNextRect = FBox2D(ForceInit);
	ShopSellPrevRect = FBox2D(ForceInit);
	ShopSellNextRect = FBox2D(ForceInit);
	if (!Canvas || !PlayerChar)
	{
		return;
	}

	UMMOInventoryComponent* Inv = PlayerChar->GetInventory();
	if (!Inv)
	{
		return;
	}

	const float PW = 640.f;
	const float PH = 548.f;
	const float PX = ((float)Canvas->SizeX - PW) * 0.5f;
	const float PY = 90.f;
	DrawRect(FLinearColor(0.03f, 0.03f, 0.05f, 0.94f), PX, PY, PW, PH);
	DrawRect(FLinearColor(0.2f, 0.55f, 0.95f, 0.95f), PX, PY, PW, 4.f);
	ShopRect = FBox2D(FVector2D(PX, PY), FVector2D(PX + PW, PY + PH));
	ShopCloseRect = DrawCloseButton(PX, PY, PW);

	DrawText(TEXT("SATICI"), FLinearColor::White, PX + 14.f, PY + 10.f, nullptr, 1.4f);
	DrawText(FString::Printf(TEXT("Altin: %d     (E ile kapat)"), Inv->Gold),
		FLinearColor(1.f, 0.85f, 0.2f), PX + 14.f, PY + 38.f, nullptr, 1.f);

	const float RowH = 26.f;
	const float ColW = (PW - 42.f) * 0.5f;
	const float LeftX = PX + 14.f;
	const float RightX = PX + 28.f + ColW;

	// --- SATIN AL (sol sutun) ---
	DrawText(TEXT("SATIN AL (sol tik)"), FLinearColor(0.6f, 0.9f, 0.6f), LeftX, PY + 68.f, nullptr, 0.95f);
	const float BuyStartY = PY + 90.f;
	const TArray<FMMOConsumableDef>& Consumables = FMMOConsumableDatabase::GetAll();
	const TArray<FMMOItemDef>& Equipment = FMMOItemDatabase::GetAll();
	const int32 BuyRowCount = 1 + Consumables.Num() + Equipment.Num();
	const int32 BuyPageCount = FMath::DivideAndRoundUp(BuyRowCount, ShopBuyPageSize);
	ShopBuyPage = FMath::Clamp(ShopBuyPage, 0, BuyPageCount - 1);
	const int32 FirstIndex = ShopBuyPage * ShopBuyPageSize;
	const int32 LastIndex = FMath::Min(FirstIndex + ShopBuyPageSize, BuyRowCount);
	for (int32 Index = FirstIndex; Index < LastIndex; ++Index)
	{
		const float LY = BuyStartY + (Index - FirstIndex) * RowH;
		FMMOClickRegion R;
		FString Label;
		FLinearColor Fill;
		float TextScale = 0.72f;
		if (Index == 0)
		{
			R.SkillId = FName(TEXT("__POTION__"));
			R.Slot = 0;
			Label = FString::Printf(TEXT("Can Iksiri  -  %d altin"), FMMOItemDatabase::PotionPrice);
			Fill = FLinearColor(0.1f, 0.16f, 0.12f, 0.9f);
			TextScale = 0.78f;
		}
		else if (Index <= Consumables.Num())
		{
			const FMMOConsumableDef& Def = Consumables[Index - 1];
			R.SkillId = Def.ItemId;
			R.Slot = 1;
			Label = FString::Printf(TEXT("%s  -  %d altin"), *Def.Name, Def.Price);
			Fill = FLinearColor(0.1f, 0.16f, 0.18f, 0.9f);
		}
		else
		{
			const FMMOItemDef& Def = Equipment[Index - 1 - Consumables.Num()];
			R.SkillId = Def.ItemId;
			R.Slot = 2;
			Label = FString::Printf(TEXT("%s  -  %d altin (Lv%d)"), *Def.Name, FMMOItemDatabase::GetBuyPrice(Def), Def.RequiredLevel);
			Fill = FLinearColor(0.1f, 0.13f, 0.2f, 0.9f);
		}
		DrawRect(Fill, LeftX, LY, ColW, RowH - 4.f);
		R.Rect = FBox2D(FVector2D(LeftX, LY), FVector2D(LeftX + ColW, LY + RowH - 4.f));
		ShopBuyRegions.Add(R);
		DrawText(Label, FLinearColor::White, LeftX + 6.f, LY + 4.f, nullptr, TextScale);
	}

	const float NavY = BuyStartY + ShopBuyPageSize * RowH + 12.f;
	const float NavW = 80.f;
	const float NavH = 26.f;
	const float NextX = LeftX + ColW - NavW;
	const bool bHasPrev = ShopBuyPage > 0;
	const bool bHasNext = ShopBuyPage + 1 < BuyPageCount;
	DrawRect(bHasPrev ? FLinearColor(0.16f, 0.32f, 0.48f, 0.95f) : FLinearColor(0.12f, 0.12f, 0.14f, 0.9f), LeftX, NavY, NavW, NavH);
	DrawText(TEXT("< Onceki"), bHasPrev ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.48f), LeftX + 4.f, NavY + 5.f, nullptr, 0.7f);
	DrawText(FString::Printf(TEXT("%d / %d"), ShopBuyPage + 1, BuyPageCount), FLinearColor::White,
		LeftX + ColW * 0.5f - 20.f, NavY + 5.f, nullptr, 0.8f);
	DrawRect(bHasNext ? FLinearColor(0.16f, 0.32f, 0.48f, 0.95f) : FLinearColor(0.12f, 0.12f, 0.14f, 0.9f), NextX, NavY, NavW, NavH);
	DrawText(TEXT("Sonraki >"), bHasNext ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.48f), NextX + 4.f, NavY + 5.f, nullptr, 0.7f);
	if (bHasPrev)
	{
		ShopBuyPrevRect = FBox2D(FVector2D(LeftX, NavY), FVector2D(LeftX + NavW, NavY + NavH));
	}
	if (bHasNext)
	{
		ShopBuyNextRect = FBox2D(FVector2D(NextX, NavY), FVector2D(NextX + NavW, NavY + NavH));
	}

	// --- SAT (sag sutun) ---
	DrawText(TEXT("SAT (sol tik)"), FLinearColor(1.f, 0.85f, 0.4f), RightX, PY + 68.f, nullptr, 0.95f);
	float RY = PY + 90.f;

	const int32 SellPageCount = FMath::Max(1, FMath::DivideAndRoundUp(Inv->EquipmentBag.Num(), ShopSellPageSize));
	ShopSellPage = FMath::Clamp(ShopSellPage, 0, SellPageCount - 1);
	const int32 FirstSellIndex = ShopSellPage * ShopSellPageSize;
	const int32 LastSellIndex = FMath::Min(FirstSellIndex + ShopSellPageSize, Inv->EquipmentBag.Num());
	for (int32 i = FirstSellIndex; i < LastSellIndex; ++i)
	{
		const FMMOEquipmentInstance& Inst = Inv->EquipmentBag[i];
		const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId);
		if (!Def)
		{
			continue;
		}

		DrawRect(FLinearColor(0.18f, 0.15f, 0.1f, 0.9f), RightX, RY, ColW, RowH - 4.f);
		FMMOClickRegion R;
		R.Slot = i;
		R.InstanceId = Inst.InstanceId; // RPC'ler indeks degil kimlik gonderir (NETWORK_AUDIT Y4)
		R.Plus = Inst.Plus;
		R.Rect = FBox2D(FVector2D(RightX, RY), FVector2D(RightX + ColW, RY + RowH - 4.f));
		ShopSellRegions.Add(R);
		const int32 SellPrice = FMath::RoundToInt(FMMOItemDatabase::GetSellPrice(*Def, Inst.Plus)
			* FMMOItemDatabase::GetRarityMultiplier(Inst.Rarity));
		DrawText(FString::Printf(TEXT("[%s] %s +%d  ->  %d altin"),
			*FMMOItemDatabase::GetRarityName(Inst.Rarity), *Def->Name, Inst.Plus, SellPrice),
			FMMOItemDatabase::GetRarityColor(Inst.Rarity), RightX + 6.f, RY + 4.f, nullptr, 0.72f);
		RY += RowH;
	}

	if (Inv->EquipmentBag.Num() == 0)
	{
		DrawText(TEXT("(satacak ekipman yok)"), FLinearColor(0.6f, 0.6f, 0.6f), RightX, RY, nullptr, 0.72f);
	}

	const float SellNextX = RightX + ColW - NavW;
	const bool bHasSellPrev = ShopSellPage > 0;
	const bool bHasSellNext = ShopSellPage + 1 < SellPageCount;
	DrawRect(bHasSellPrev ? FLinearColor(0.16f, 0.32f, 0.48f, 0.95f) : FLinearColor(0.12f, 0.12f, 0.14f, 0.9f), RightX, NavY, NavW, NavH);
	DrawText(TEXT("< Onceki"), bHasSellPrev ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.48f), RightX + 4.f, NavY + 5.f, nullptr, 0.7f);
	DrawText(FString::Printf(TEXT("%d / %d"), ShopSellPage + 1, SellPageCount), FLinearColor::White,
		RightX + ColW * 0.5f - 20.f, NavY + 5.f, nullptr, 0.8f);
	DrawRect(bHasSellNext ? FLinearColor(0.16f, 0.32f, 0.48f, 0.95f) : FLinearColor(0.12f, 0.12f, 0.14f, 0.9f), SellNextX, NavY, NavW, NavH);
	DrawText(TEXT("Sonraki >"), bHasSellNext ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.48f), SellNextX + 4.f, NavY + 5.f, nullptr, 0.7f);
	if (bHasSellPrev)
	{
		ShopSellPrevRect = FBox2D(FVector2D(RightX, NavY), FVector2D(RightX + NavW, NavY + NavH));
	}
	if (bHasSellNext)
	{
		ShopSellNextRect = FBox2D(FVector2D(SellNextX, NavY), FVector2D(SellNextX + NavW, NavY + NavH));
	}
}

void AMMOHUD::OpenWarehouse()
{
	WarehouseBagRegions.Reset();
	WarehouseStoreRegions.Reset();
	bWarehouseOpen = true;
}

void AMMOHUD::DrawWarehouse(AMMOCharacter* PlayerChar)
{
	WarehouseBagRegions.Reset();
	WarehouseStoreRegions.Reset();
	if (!Canvas || !PlayerChar)
	{
		return;
	}
	UMMOInventoryComponent* Inv = PlayerChar->GetInventory();
	if (!Inv)
	{
		return;
	}

	const float PW = 640.f;
	const float PH = 548.f;
	const float PX = ((float)Canvas->SizeX - PW) * 0.5f;
	const float PY = 90.f;
	DrawRect(FLinearColor(0.03f, 0.03f, 0.05f, 0.94f), PX, PY, PW, PH);
	DrawRect(FLinearColor(0.75f, 0.5f, 0.25f, 0.95f), PX, PY, PW, 4.f);
	WarehouseRect = FBox2D(FVector2D(PX, PY), FVector2D(PX + PW, PY + PH));
	WarehouseCloseRect = DrawCloseButton(PX, PY, PW);

	DrawText(TEXT("DEPO"), FLinearColor::White, PX + 14.f, PY + 10.f, nullptr, 1.4f);
	DrawText(TEXT("Depodaki esyalar Yaz ve Kis hub'indaki depocularda ortaktir."),
		FLinearColor(0.7f, 0.7f, 0.75f), PX + 14.f, PY + 38.f, nullptr, 0.8f);

	// 24 depo satiri pencereye sigsin diye satirlar dukkandan daha ince.
	const float StartY = PY + 90.f;
	// Depo yuklemede 24'u asabilir (esya silinmez kurali): fazla satirlar panelin disina tasip
	// tiklanamaz olmasin diye satir yuksekligi en kalabalik listeye gore kuculur.
	const int32 MaxListRows = FMath::Max(UMMOInventoryComponent::MaxWarehouseSize,
		FMath::Max(Inv->Warehouse.Num(), Inv->EquipmentBag.Num()));
	const float RowH = FMath::Min(18.f, (PH - 90.f - 12.f) / (float)MaxListRows);
	const float ColW = (PW - 42.f) * 0.5f;
	const float LeftX = PX + 14.f;
	const float RightX = PX + 28.f + ColW;

	auto DrawEntry = [&](const FMMOEquipmentInstance& Inst, float X, float Y, TArray<FMMOClickRegion>& Out)
	{
		const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId);
		if (!Def)
		{
			return;
		}
		DrawRect(FLinearColor(0.14f, 0.12f, 0.1f, 0.9f), X, Y, ColW, RowH - 2.f);
		FMMOClickRegion R;
		R.Slot = Inst.InstanceId;
		R.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + ColW, Y + RowH - 2.f));
		Out.Add(R);
		DrawText(FString::Printf(TEXT("[%s] %s +%d (Lv%d)"), *FMMOItemDatabase::GetRarityName(Inst.Rarity),
			*Def->Name, Inst.Plus, Def->RequiredLevel),
			FMMOItemDatabase::GetRarityColor(Inst.Rarity), X + 6.f, Y + (RowH >= 16.f ? 2.f : 0.f), nullptr, 0.65f);
	};

	DrawText(FString::Printf(TEXT("CANTA %d/%d (tikla: depoya koy)"), Inv->EquipmentBag.Num(), UMMOInventoryComponent::MaxBagSize),
		FLinearColor(1.f, 0.85f, 0.4f), LeftX, PY + 68.f, nullptr, 0.9f);
	for (int32 i = 0; i < Inv->EquipmentBag.Num(); ++i)
	{
		DrawEntry(Inv->EquipmentBag[i], LeftX, StartY + i * RowH, WarehouseBagRegions);
	}
	if (Inv->EquipmentBag.Num() == 0)
	{
		DrawText(TEXT("(canta bos)"), FLinearColor(0.6f, 0.6f, 0.6f), LeftX, StartY, nullptr, 0.72f);
	}

	DrawText(FString::Printf(TEXT("DEPO %d/%d (tikla: cantaya al)"), Inv->Warehouse.Num(), UMMOInventoryComponent::MaxWarehouseSize),
		FLinearColor(0.6f, 0.9f, 0.6f), RightX, PY + 68.f, nullptr, 0.9f);
	for (int32 i = 0; i < Inv->Warehouse.Num(); ++i)
	{
		DrawEntry(Inv->Warehouse[i], RightX, StartY + i * RowH, WarehouseStoreRegions);
	}
	if (Inv->Warehouse.Num() == 0)
	{
		DrawText(TEXT("(depo bos)"), FLinearColor(0.6f, 0.6f, 0.6f), RightX, StartY, nullptr, 0.72f);
	}
}

bool AMMOHUD::HandleWarehouseClick(const FVector2D& Pos)
{
	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	UMMOInventoryComponent* Inv = Char ? Char->GetInventory() : nullptr;
	if (!Inv)
	{
		return true;
	}
	for (const FMMOClickRegion& R : WarehouseBagRegions)
	{
		if (R.Rect.IsInside(Pos))
		{
			Inv->DepositEquipment(R.Slot);
			return true;
		}
	}
	for (const FMMOClickRegion& R : WarehouseStoreRegions)
	{
		if (R.Rect.IsInside(Pos))
		{
			Inv->WithdrawEquipment(R.Slot);
			return true;
		}
	}
	return true;
}

namespace
{
	// Ticaret penceresi altin dugmeleri: son dugme (indeks = sayi) altini sifirlar.
	const int32 TradeGoldDeltas[] = { 100, 1000, 10000, 100000 };
	const TCHAR* TradeGoldLabels[] = { TEXT("+100"), TEXT("+1K"), TEXT("+10K"), TEXT("+100K"), TEXT("Sifirla") };
}

bool AMMOHUD::IsTradeOpen() const
{
	const AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	const UMMOTradeComponent* Trade = Char ? Char->GetTradeComponent() : nullptr;
	return Trade && Trade->IsTrading();
}

void AMMOHUD::DrawTrade(AMMOCharacter* PlayerChar)
{
	TradeBagRegions.Reset();
	TradeOfferRegions.Reset();
	TradeGoldRegions.Reset();
	if (!Canvas || !PlayerChar)
	{
		return;
	}
	UMMOInventoryComponent* Inv = PlayerChar->GetInventory();
	UMMOTradeComponent* Trade = PlayerChar->GetTradeComponent();
	if (!Inv || !Trade)
	{
		return;
	}

	const float PW = 700.f;
	const float PH = 380.f;
	const float PX = ((float)Canvas->SizeX - PW) * 0.5f;
	const float PY = 90.f;
	DrawRect(FLinearColor(0.03f, 0.03f, 0.05f, 0.94f), PX, PY, PW, PH);
	DrawRect(FLinearColor(0.3f, 0.75f, 0.45f, 0.95f), PX, PY, PW, 4.f);
	TradeRect = FBox2D(FVector2D(PX, PY), FVector2D(PX + PW, PY + PH));
	TradeCloseRect = DrawCloseButton(PX, PY, PW);

	DrawText(FString::Printf(TEXT("TICARET - %s"), *Trade->PartnerName), FLinearColor::White, PX + 14.f, PY + 10.f, nullptr, 1.4f);
	DrawText(TEXT("Cantandan esyaya tikla: teklife eklenir. Teklif degisirse iki onay da sifirlanir. X = iptal."),
		FLinearColor(0.7f, 0.7f, 0.75f), PX + 14.f, PY + 38.f, nullptr, 0.8f);

	const float RowH = 18.f;
	const float X1 = PX + 14.f;
	const float W1 = 220.f;
	const float X2 = PX + 248.f;
	const float W2 = 210.f;
	const float X3 = PX + 472.f;
	const float W3 = 214.f;
	const float StartY = PY + 90.f;

	auto DrawEntry = [&](const FMMOEquipmentInstance& Inst, float X, float Y, float W, TArray<FMMOClickRegion>* Out)
	{
		const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId);
		if (!Def)
		{
			return;
		}
		DrawRect(FLinearColor(0.12f, 0.14f, 0.12f, 0.9f), X, Y, W, RowH - 2.f);
		if (Out)
		{
			FMMOClickRegion R;
			R.Slot = Inst.InstanceId;
			R.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + RowH - 2.f));
			Out->Add(R);
		}
		DrawText(FString::Printf(TEXT("[%s] %s +%d"), *FMMOItemDatabase::GetRarityName(Inst.Rarity), *Def->Name, Inst.Plus),
			FMMOItemDatabase::GetRarityColor(Inst.Rarity), X + 6.f, Y + 2.f, nullptr, 0.65f);
	};

	// Sol: canta (zaten teklifteki esyalar gizlenir).
	DrawText(FString::Printf(TEXT("CANTA %d/%d"), Inv->EquipmentBag.Num(), UMMOInventoryComponent::MaxBagSize),
		FLinearColor(1.f, 0.85f, 0.4f), X1, PY + 68.f, nullptr, 0.9f);
	int32 BagRow = 0;
	for (const FMMOEquipmentInstance& Inst : Inv->EquipmentBag)
	{
		const bool bOffered = Trade->MyOffer.Items.ContainsByPredicate([&Inst](const FMMOEquipmentInstance& O)
		{
			return O.InstanceId == Inst.InstanceId;
		});
		if (bOffered)
		{
			continue;
		}
		DrawEntry(Inst, X1, StartY + BagRow * RowH, W1, &TradeBagRegions);
		++BagRow;
	}
	if (BagRow == 0)
	{
		DrawText(TEXT("(teklif edilecek ekipman yok)"), FLinearColor(0.6f, 0.6f, 0.6f), X1, StartY, nullptr, 0.72f);
	}

	// Orta: senin teklifin.
	DrawText(TEXT("SENIN TEKLIFIN (tikla: cikar)"), FLinearColor(0.6f, 0.9f, 0.6f), X2, PY + 68.f, nullptr, 0.9f);
	for (int32 i = 0; i < Trade->MyOffer.Items.Num(); ++i)
	{
		DrawEntry(Trade->MyOffer.Items[i], X2, StartY + i * RowH, W2, &TradeOfferRegions);
	}
	const float GoldY = StartY + UMMOTradeComponent::MaxOfferItems * RowH + 8.f;
	DrawText(FString::Printf(TEXT("Altin: %d  (cuzdan: %d)"), Trade->MyOffer.Gold, Inv->Gold),
		FLinearColor(1.f, 0.85f, 0.3f), X2, GoldY, nullptr, 0.8f);
	{
		const float BtnW = 38.f;
		const float BtnH = 20.f;
		const float BtnY = GoldY + 22.f;
		for (int32 b = 0; b < 5; ++b)
		{
			const float BX = X2 + b * (BtnW + 4.f);
			DrawRect(FLinearColor(0.2f, 0.18f, 0.08f, 0.95f), BX, BtnY, BtnW, BtnH);
			DrawText(TradeGoldLabels[b], FLinearColor(1.f, 0.9f, 0.5f), BX + 3.f, BtnY + 4.f, nullptr, 0.6f);
			FMMOClickRegion R;
			R.Slot = b;
			R.Rect = FBox2D(FVector2D(BX, BtnY), FVector2D(BX + BtnW, BtnY + BtnH));
			TradeGoldRegions.Add(R);
		}
	}
	DrawText(Trade->MyOffer.bConfirmed ? TEXT("Sen: ONAYLADIN") : TEXT("Sen: onay bekliyor"),
		Trade->MyOffer.bConfirmed ? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor(0.75f, 0.75f, 0.75f),
		X2, GoldY + 54.f, nullptr, 0.85f);

	// Sag: karsi tarafin teklifi.
	DrawText(FString::Printf(TEXT("%s TEKLIFI"), *Trade->PartnerName.ToUpper()), FLinearColor(0.6f, 0.8f, 1.f), X3, PY + 68.f, nullptr, 0.9f);
	for (int32 i = 0; i < Trade->TheirOffer.Items.Num(); ++i)
	{
		DrawEntry(Trade->TheirOffer.Items[i], X3, StartY + i * RowH, W3, nullptr);
	}
	DrawText(FString::Printf(TEXT("Altin: %d"), Trade->TheirOffer.Gold), FLinearColor(1.f, 0.85f, 0.3f), X3, GoldY, nullptr, 0.8f);
	DrawText(Trade->TheirOffer.bConfirmed ? TEXT("Karsi taraf: ONAYLADI") : TEXT("Karsi taraf: onay bekliyor"),
		Trade->TheirOffer.bConfirmed ? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor(0.75f, 0.75f, 0.75f),
		X3, GoldY + 54.f, nullptr, 0.85f);

	// Alt dugmeler: onayla / iptal.
	const float BtnY = PY + PH - 46.f;
	TradeConfirmRect = FBox2D(FVector2D(X2, BtnY), FVector2D(X2 + 200.f, BtnY + 34.f));
	DrawRect(Trade->MyOffer.bConfirmed ? FLinearColor(0.12f, 0.12f, 0.14f, 0.95f) : FLinearColor(0.1f, 0.4f, 0.18f, 0.95f), X2, BtnY, 200.f, 34.f);
	DrawText(Trade->MyOffer.bConfirmed ? TEXT("Onaylandi") : TEXT("ONAYLA"), FLinearColor::White, X2 + 56.f, BtnY + 8.f, nullptr, 1.05f);
	TradeCancelRect = FBox2D(FVector2D(X3, BtnY), FVector2D(X3 + 200.f, BtnY + 34.f));
	DrawRect(FLinearColor(0.45f, 0.12f, 0.12f, 0.95f), X3, BtnY, 200.f, 34.f);
	DrawText(TEXT("IPTAL"), FLinearColor::White, X3 + 72.f, BtnY + 8.f, nullptr, 1.05f);
}

bool AMMOHUD::HandleTradeClick(const FVector2D& Pos)
{
	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	UMMOTradeComponent* Trade = Char ? Char->GetTradeComponent() : nullptr;
	if (!Trade)
	{
		return true;
	}
	if (TradeConfirmRect.IsInside(Pos))
	{
		Trade->RequestConfirm();
		return true;
	}
	if (TradeCancelRect.IsInside(Pos))
	{
		Trade->RequestCancel();
		return true;
	}
	for (const FMMOClickRegion& R : TradeBagRegions)
	{
		if (R.Rect.IsInside(Pos))
		{
			Trade->RequestAddItem(R.Slot);
			return true;
		}
	}
	for (const FMMOClickRegion& R : TradeOfferRegions)
	{
		if (R.Rect.IsInside(Pos))
		{
			Trade->RequestRemoveItem(R.Slot);
			return true;
		}
	}
	for (const FMMOClickRegion& R : TradeGoldRegions)
	{
		if (R.Rect.IsInside(Pos))
		{
			const int32 NumDeltas = UE_ARRAY_COUNT(TradeGoldDeltas);
			if (R.Slot >= NumDeltas)
			{
				Trade->RequestSetGold(0);
			}
			else
			{
				// Sunucu cuzdana gore tekrar sikistirir; burada yalnizca tasmayi onle.
				const int64 Next = (int64)Trade->MyOffer.Gold + TradeGoldDeltas[R.Slot];
				Trade->RequestSetGold((int32)FMath::Min<int64>(Next, MAX_int32));
			}
			return true;
		}
	}
	return true;
}

bool AMMOHUD::HandleShopClick(const FVector2D& Pos)
{
	if (!bShopOpen)
	{
		return false;
	}

	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	UMMOInventoryComponent* Inv = Char ? Char->GetInventory() : nullptr;
	if (!Inv)
	{
		return true;
	}
	if (ShopBuyPrevRect.bIsValid && ShopBuyPrevRect.IsInside(Pos))
	{
		--ShopBuyPage;
		ShopBuyRegions.Reset();
		ShopBuyPrevRect = FBox2D(ForceInit);
		ShopBuyNextRect = FBox2D(ForceInit);
		return true;
	}
	if (ShopBuyNextRect.bIsValid && ShopBuyNextRect.IsInside(Pos))
	{
		++ShopBuyPage;
		ShopBuyRegions.Reset();
		ShopBuyPrevRect = FBox2D(ForceInit);
		ShopBuyNextRect = FBox2D(ForceInit);
		return true;
	}
	if (ShopSellPrevRect.bIsValid && ShopSellPrevRect.IsInside(Pos))
	{
		--ShopSellPage;
		ShopSellRegions.Reset();
		ShopSellPrevRect = FBox2D(ForceInit);
		ShopSellNextRect = FBox2D(ForceInit);
		return true;
	}
	if (ShopSellNextRect.bIsValid && ShopSellNextRect.IsInside(Pos))
	{
		++ShopSellPage;
		ShopSellRegions.Reset();
		ShopSellPrevRect = FBox2D(ForceInit);
		ShopSellNextRect = FBox2D(ForceInit);
		return true;
	}

	for (const FMMOClickRegion& R : ShopBuyRegions)
	{
		if (!R.SkillId.IsNone() && R.Rect.IsInside(Pos))
		{
			if (R.Slot == 0)
			{
				Inv->BuyPotion(1);
			}
			else if (R.Slot == 1)
			{
				Inv->BuyConsumable(R.SkillId, 1);
			}
			else
			{
				Inv->BuyEquipment(R.SkillId);
			}
			return true;
		}
	}

	for (const FMMOClickRegion& R : ShopSellRegions)
	{
		if (R.Slot >= 0 && R.Rect.IsInside(Pos))
		{
			Inv->SellByInstanceId(R.InstanceId);
			return true;
		}
	}

	return true;
}

void AMMOHUD::DrawPlayerNameplates()
{
	UWorld* World = GetWorld();
	if (!Canvas || !World)
	{
		return;
	}

	const APawn* LocalPawn = GetOwningPawn();

	for (TActorIterator<AMMOCharacter> It(World); It; ++It)
	{
		AMMOCharacter* C = *It;
		if (!C || C == LocalPawn || Cast<AMMOMob>(C))
		{
			continue; // kendini ve moblari atla
		}

		const APlayerState* PS = C->GetPlayerState();
		UMMOStatsComponent* S = C->GetStats();
		if (!PS || !S)
		{
			continue; // sadece diger oyuncular
		}

		const FVector Head = C->GetActorLocation() + FVector(0.f, 0.f, 110.f);
		const FVector Screen = Canvas->Project(Head);
		if (Screen.Z <= 0.f)
		{
			continue;
		}

		DrawText(FString::Printf(TEXT("%s  Lv%d"), *PS->GetPlayerName(), S->Level),
			FLinearColor(0.5f, 0.85f, 1.f), Screen.X - 40.f, Screen.Y - 14.f, nullptr, 0.78f);

		// Klan adi (varsa) isim satirinin ustunde.
		if (const UMMOClanComponent* Clan = C->GetClanComponent())
		{
			if (Clan->IsInClan())
			{
				DrawText(FString::Printf(TEXT("[%s]"), *Clan->ClanName),
					FLinearColor(0.4f, 0.9f, 0.4f), Screen.X - 40.f, Screen.Y - 28.f, nullptr, 0.72f);
			}
		}

		const float BarW = 62.f;
		const float BarH = 6.f;
		const float BX = Screen.X - BarW * 0.5f;
		const float BY = Screen.Y;
		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f), BX - 1.f, BY - 1.f, BarW + 2.f, BarH + 2.f);
		DrawRect(FLinearColor(0.15f, 0.15f, 0.16f, 0.9f), BX, BY, BarW, BarH);
		DrawRect(FLinearColor(0.2f, 0.75f, 0.3f, 1.f), BX, BY, BarW * S->GetHealthPercent(), BarH);
	}
}

void AMMOHUD::DrawVendorNameplates()
{
	UWorld* World = GetWorld();
	if (!Canvas || !World)
	{
		return;
	}

	for (TActorIterator<AMMOVendorNPC> It(World); It; ++It)
	{
		AMMOVendorNPC* Vendor = *It;
		if (!Vendor)
		{
			continue;
		}
		const FVector Head = Vendor->GetActorLocation() + FVector(0.f, 0.f, 110.f);
		const FVector Screen = Canvas->Project(Head);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		DrawText(FString::Printf(TEXT("%s [tikla]"), *Vendor->VendorName),
			FLinearColor(0.5f, 0.8f, 1.f), Screen.X - 34.f, Screen.Y, nullptr, 0.8f);
	}

	for (TActorIterator<AMMOQuestGiverNPC> It(World); It; ++It)
	{
		AMMOQuestGiverNPC* Npc = *It;
		if (!Npc)
		{
			continue;
		}
		const FVector Head = Npc->GetActorLocation() + FVector(0.f, 0.f, 110.f);
		const FVector Screen = Canvas->Project(Head);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		DrawText(FString::Printf(TEXT("! %s [tikla]"), *Npc->NpcName),
			FLinearColor(1.f, 0.85f, 0.2f), Screen.X - 34.f, Screen.Y, nullptr, 0.8f);
	}

	// Konusan NPC'ler (mor): tiklaninca LLM diyalogu.
	for (TActorIterator<AMMOTalkerNPC> It(World); It; ++It)
	{
		AMMOTalkerNPC* Npc = *It;
		if (!Npc)
		{
			continue;
		}
		const FVector Head = Npc->GetActorLocation() + FVector(0.f, 0.f, 110.f);
		const FVector Screen = Canvas->Project(Head);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		DrawText(FString::Printf(TEXT("%s [konus]"), *Npc->NpcName),
			FLinearColor(0.8f, 0.6f, 1.f), Screen.X - 34.f, Screen.Y, nullptr, 0.8f);
	}

	// Sifaci NPC (yesil-teal): tiklaninca iyilesme.
	for (TActorIterator<AMMOHealerNPC> It(World); It; ++It)
	{
		AMMOHealerNPC* Npc = *It;
		if (!Npc)
		{
			continue;
		}
		const FVector Head = Npc->GetActorLocation() + FVector(0.f, 0.f, 110.f);
		const FVector Screen = Canvas->Project(Head);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		DrawText(FString::Printf(TEXT("+ %s [iyilesme]"), *Npc->HealerName),
			FLinearColor(0.4f, 1.f, 0.7f), Screen.X - 34.f, Screen.Y, nullptr, 0.8f);
	}

	// Depocu NPC (toprak rengi): tiklaninca depo penceresi.
	for (TActorIterator<AMMOWarehouseNPC> It(World); It; ++It)
	{
		AMMOWarehouseNPC* Npc = *It;
		if (!Npc)
		{
			continue;
		}
		const FVector Head = Npc->GetActorLocation() + FVector(0.f, 0.f, 110.f);
		const FVector Screen = Canvas->Project(Head);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		DrawText(FString::Printf(TEXT("%s [depo]"), *Npc->WarehouseName),
			FLinearColor(0.95f, 0.75f, 0.45f), Screen.X - 34.f, Screen.Y, nullptr, 0.8f);
	}
}

// ----------------------------------------------------------------------
// Gorevler
// ----------------------------------------------------------------------

void AMMOHUD::OpenQuestWindow()
{
	bQuestWindowOpen = true;
}

void AMMOHUD::ToggleStatWindow()
{
	bStatWindowOpen = !bStatWindowOpen;
}

void AMMOHUD::CloseQuestWindow()
{
	bQuestWindowOpen = false;
}

void AMMOHUD::DrawMinimap(AMMOCharacter* PlayerChar)
{
	UWorld* World = GetWorld();
	if (!Canvas || !World || !PlayerChar)
	{
		return;
	}

	const float Size = 160.f;
	const float MX = (float)Canvas->SizeX - Size - 16.f;
	const float MY = 16.f;
	const float CX = MX + Size * 0.5f;
	const float CY = MY + Size * 0.5f;
	const float Half = Size * 0.5f;
	const float Range = 4000.f; // dunya birimi -> minimap yaricapi

	// Zemin + cerceve.
	DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.7f), MX, MY, Size, Size);
	DrawRect(FLinearColor(0.3f, 0.45f, 0.7f, 0.9f), MX, MY, Size, 2.f);
	DrawRect(FLinearColor(0.3f, 0.45f, 0.7f, 0.9f), MX, MY + Size - 2.f, Size, 2.f);
	DrawRect(FLinearColor(0.3f, 0.45f, 0.7f, 0.9f), MX, MY, 2.f, Size);
	DrawRect(FLinearColor(0.3f, 0.45f, 0.7f, 0.9f), MX + Size - 2.f, MY, 2.f, Size);
	DrawText(TEXT("K"), FLinearColor(0.6f, 0.75f, 1.f), CX - 4.f, MY + 2.f, nullptr, 0.6f); // kuzey (north-up)

	const FVector PlayerLoc = PlayerChar->GetActorLocation();

	auto PlotDot = [&](const FVector& WorldLoc, const FLinearColor& Color, float DotSize)
	{
		const FVector Rel = WorldLoc - PlayerLoc;
		// north-up: +X (kuzey) yukari, +Y (dogu) saga.
		float dx = (Rel.Y / Range) * Half;
		float dy = -(Rel.X / Range) * Half;
		if (FMath::Abs(dx) > Half - 3.f || FMath::Abs(dy) > Half - 3.f)
		{
			return; // menzil disi
		}
		DrawRect(Color, CX + dx - DotSize * 0.5f, CY + dy - DotSize * 0.5f, DotSize, DotSize);
	};

	// Moblar (kirmizi; elit/boss'lar turuncu ve biraz daha buyuk nokta - nameplate'teki
	// [ELIT] etiketiyle tutarli, oyuncu haritada da tehlikeyi onceden secebilsin).
	for (TActorIterator<AMMOMob> It(World); It; ++It)
	{
		if (*It && (*It)->GetStats() && !(*It)->GetStats()->bIsDead)
		{
			const FMMOMobArchetype* Arch = FMMOMobDatabase::Find((*It)->ArchetypeId);
			const bool bElite = Arch && Arch->bIsElite;
			PlotDot((*It)->GetActorLocation(), bElite ? FLinearColor(1.f, 0.55f, 0.15f) : FLinearColor(0.9f, 0.2f, 0.2f), bElite ? 6.f : 4.f);
		}
	}
	// Saticilar (mavi)
	for (TActorIterator<AMMOVendorNPC> It(World); It; ++It)
	{
		if (*It)
		{
			PlotDot((*It)->GetActorLocation(), FLinearColor(0.3f, 0.6f, 1.f), 5.f);
		}
	}
	// Gorevliler (altin)
	for (TActorIterator<AMMOQuestGiverNPC> It(World); It; ++It)
	{
		if (*It)
		{
			PlotDot((*It)->GetActorLocation(), FLinearColor(1.f, 0.82f, 0.2f), 5.f);
		}
	}
	// Sifacilar (yesil-teal)
	for (TActorIterator<AMMOHealerNPC> It(World); It; ++It)
	{
		if (*It)
		{
			PlotDot((*It)->GetActorLocation(), FLinearColor(0.4f, 1.f, 0.7f), 5.f);
		}
	}
	// Depocular (toprak)
	for (TActorIterator<AMMOWarehouseNPC> It(World); It; ++It)
	{
		if (*It)
		{
			PlotDot((*It)->GetActorLocation(), FLinearColor(0.95f, 0.75f, 0.45f), 5.f);
		}
	}
	// Konusan NPC'ler (mor)
	for (TActorIterator<AMMOTalkerNPC> It(World); It; ++It)
	{
		if (*It)
		{
			PlotDot((*It)->GetActorLocation(), FLinearColor(0.75f, 0.55f, 1.f), 5.f);
		}
	}
	// Diger oyuncular (yesil)
	const APawn* LocalPawn = GetOwningPawn();
	for (TActorIterator<AMMOCharacter> It(World); It; ++It)
	{
		AMMOCharacter* C = *It;
		if (C && C != LocalPawn && !Cast<AMMOMob>(C) && C->GetPlayerState())
		{
			PlotDot(C->GetActorLocation(), FLinearColor(0.3f, 0.9f, 0.4f), 5.f);
		}
	}

	// Oyuncu merkezde (beyaz) + bakis yonu cizgisi.
	const FVector Fwd = PlayerChar->GetActorForwardVector();
	DrawLine(CX, CY, CX + (Fwd.Y / 1.f) * 10.f, CY - (Fwd.X / 1.f) * 10.f, FLinearColor::White, 1.5f);
	DrawRect(FLinearColor::White, CX - 2.5f, CY - 2.5f, 5.f, 5.f);
}

void AMMOHUD::DrawQuestTracker(AMMOCharacter* PlayerChar)
{
	if (!Canvas || !PlayerChar)
	{
		return;
	}
	UMMOQuestComponent* Q = PlayerChar->GetQuestComponent();
	if (!Q || Q->ActiveQuests.Num() == 0)
	{
		return;
	}

	const float X = (float)Canvas->SizeX - 250.f;
	float Y = 200.f;
	DrawText(TEXT("-- Gorevler --"), FLinearColor(1.f, 0.85f, 0.3f), X, Y, nullptr, 0.9f);
	Y += 20.f;

	// Tracker'i minimap altindan asagi tasmaktan koru: gorunur satiri sinirla,
	// artani "+N daha" ozetiyle goster (gorev sayisi arttikca ekrani doldurmasin).
	const int32 MaxTrackerRows = 8;
	int32 Shown = 0;
	for (const FMMOQuestProgress& P : Q->ActiveQuests)
	{
		const FMMOQuestDef* Def = FMMOQuestDatabase::Find(P.QuestId);
		if (!Def)
		{
			continue;
		}
		if (Shown >= MaxTrackerRows)
		{
			const int32 Remaining = Q->ActiveQuests.Num() - Shown;
			if (Remaining > 0)
			{
				DrawText(FString::Printf(TEXT("... +%d gorev daha (Gorevli'ye tikla)"), Remaining),
					FLinearColor(0.7f, 0.7f, 0.5f), X, Y, nullptr, 0.68f);
			}
			break;
		}
		const bool bDone = Q->IsQuestComplete(*Def, P);
		const FLinearColor Col = bDone ? FLinearColor(0.5f, 1.f, 0.5f) : FLinearColor(0.85f, 0.85f, 0.85f);
		DrawText(FString::Printf(TEXT("%s  %d/%d%s"), *Def->Name, Q->GetProgress(*Def), Def->TargetCount,
			bDone ? TEXT("  (tamam!)") : TEXT("")), Col, X, Y, nullptr, 0.72f);
		Y += 16.f;
		++Shown;
	}
}

// Gorev odul-esyasi etiketi (", +Ejder Kilici" / ", +3 Can Iksiri"). Yoksa bos.
static FString BuildQuestRewardItemSuffix(const FMMOQuestDef& Def)
{
	FString S;
	if (!Def.RewardEquipmentId.IsNone())
	{
		if (const FMMOItemDef* ID = FMMOItemDatabase::Find(Def.RewardEquipmentId))
		{
			S += FString::Printf(TEXT(", +%s"), *ID->Name);
		}
	}
	if (!Def.RewardConsumableId.IsNone() && Def.RewardConsumableCount > 0)
	{
		FString CName;
		if (const FMMOConsumableDef* CD = FMMOConsumableDatabase::Find(Def.RewardConsumableId))
		{
			CName = CD->Name;
		}
		else if (Def.RewardConsumableId == FName(TEXT("HealthPotion")))
		{
			CName = TEXT("Can Iksiri");
		}
		else
		{
			CName = Def.RewardConsumableId.ToString();
		}
		S += FString::Printf(TEXT(", +%d %s"), Def.RewardConsumableCount, *CName);
	}
	return S;
}

void AMMOHUD::DrawQuestWindow(AMMOCharacter* PlayerChar)
{
	QuestAcceptRegions.Reset();
	QuestClaimRegions.Reset();
	if (!Canvas || !PlayerChar)
	{
		return;
	}
	UMMOQuestComponent* Q = PlayerChar->GetQuestComponent();
	if (!Q)
	{
		return;
	}

	const float PW = 480.f;
	const float PH = 466.f;
	const float PX = ((float)Canvas->SizeX - PW) * 0.5f - 180.f;
	const float PY = 90.f;
	DrawRect(FLinearColor(0.03f, 0.03f, 0.05f, 0.94f), PX, PY, PW, PH);
	DrawRect(FLinearColor(0.95f, 0.75f, 0.1f, 0.95f), PX, PY, PW, 4.f);
	QuestRect = FBox2D(FVector2D(PX, PY), FVector2D(PX + PW, PY + PH));
	QuestCloseRect = DrawCloseButton(PX, PY, PW);

	DrawText(TEXT("GOREVLER"), FLinearColor::White, PX + 14.f, PY + 10.f, nullptr, 1.35f);

	const float RowX = PX + 14.f;
	const float RowW = PW - 28.f;
	float RowY = PY + 44.f;

	// Iki bolumun (alinabilir + aktif) toplam satirini panele sigdir: gorev sayisi
	// arttikca satir yuksekligini kucultup tasmayi + erisilemez "odul al" satirlarini onle.
	int32 AvailCount = 0;
	for (const FMMOQuestDef& Def : FMMOQuestDatabase::GetAll())
	{
		if (Q->IsQuestAvailable(Def.QuestId)) { ++AvailCount; }
	}
	const int32 ActiveCount = Q->ActiveQuests.Num();
	const int32 TotalRows = AvailCount + ActiveCount;
	// Sabit dikey masraf: iki bolum basligi (20+20) + aralarindaki bosluk (10).
	const float RowSpace = (PY + PH - 12.f) - (PY + 44.f) - 50.f;
	// Alt sinir (eski: 24) KALDIRILDI: 15+ satirda (16. görev eklendiginde) alt sinira
	// carpip panelin disina tasiyor, tasan gorevlerin "odul al" satiri tiklanamaz hale
	// geliyordu (bug-hunt, latent — su an 14 görevle henuz tetiklenmiyor ama sonraki
	// icerik eklemesinde patlardi). Shop'taki BuyRowH ile ayni felsefe: sikisik ama
	// erisilebilir, tasan-ve-kilitli degil.
	const float RowH = FMath::Min(46.f, RowSpace / (float)FMath::Max(1, TotalRows));
	const float CellH = FMath::Max(1.f, RowH - 4.f);
	// Iki metin satirini gercek font yuksekliklerine gore hucreye sigdir.
	// DetailHead (aciklama) pencere genisligine sigmazsa "..." ile kisaltilir; DetailTail (odul
	// satiri) her zaman korunur. Yukseklik/tiklanabilir bolge hesabi degismez (tek metin satiri).
	const auto DrawQuestRowText = [this, RowX, RowW, RowH, CellH](const FString& Title, const FString& DetailHead, const FString& DetailTail, float Y)
	{
		float TextW = 0.f, TitleH = 0.f, DetailH = 0.f;
		GetTextSize(Title, TextW, TitleH, nullptr, 0.78f);
		GetTextSize(DetailHead + DetailTail, TextW, DetailH, nullptr, 0.64f);
		const float Scale = FMath::Min(1.f, FMath::Max(1.f, CellH - 5.f) / FMath::Max(1.f, TitleH + DetailH));
		const float DetailY = FMath::Max(4.f + TitleH * Scale,
			FMath::Min(FMath::Min(24.f, RowH * 0.5f), CellH - 1.f - DetailH * Scale));

		// Satir ici sol bosluk 6 px; sag tarafta da 6 px pay birak.
		const float MaxW = RowW - 12.f;
		const float DetailScale = 0.64f * Scale;
		const auto Fits = [this, MaxW, DetailScale](const FString& S)
		{
			float W = 0.f, H = 0.f;
			GetTextSize(S, W, H, nullptr, DetailScale);
			return W <= MaxW;
		};
		FString Detail = DetailHead + DetailTail;
		if (!Fits(Detail))
		{
			// En uzun sigan aciklama onekini ikili aramayla bul (karede ~7 olcum).
			int32 Lo = 0, Hi = DetailHead.Len();
			while (Lo < Hi)
			{
				const int32 Mid = (Lo + Hi + 1) / 2;
				if (Fits(DetailHead.Left(Mid) + TEXT("...") + DetailTail)) { Lo = Mid; } else { Hi = Mid - 1; }
			}
			Detail = DetailHead.Left(Lo) + TEXT("...") + DetailTail;
			if (!Fits(Detail))
			{
				// Odul satirinin kendisi bile sigmiyor (cok uzun esya adi): butunu kes.
				const FString Full = DetailHead + DetailTail;
				Lo = 0; Hi = Full.Len();
				while (Lo < Hi)
				{
					const int32 Mid = (Lo + Hi + 1) / 2;
					if (Fits(Full.Left(Mid) + TEXT("..."))) { Lo = Mid; } else { Hi = Mid - 1; }
				}
				Detail = Full.Left(Lo) + TEXT("...");
			}
		}

		DrawText(Title, FLinearColor::White, RowX + 6.f, Y + 3.f, nullptr, 0.78f * Scale);
		DrawText(Detail, FLinearColor(0.7f, 0.75f, 0.85f), RowX + 6.f, Y + DetailY, nullptr, DetailScale);
	};

	DrawText(TEXT("Alinabilir gorevler (tikla = al)"), FLinearColor(0.6f, 0.9f, 0.6f), RowX, RowY, nullptr, 0.9f);
	RowY += 20.f;

	for (const FMMOQuestDef& Def : FMMOQuestDatabase::GetAll())
	{
		// Sadece su an alinabilir gorevler (aktif/tamam olan ve on kosulu/level'i saglanmayan gizli).
		if (!Q->IsQuestAvailable(Def.QuestId))
		{
			continue;
		}
		DrawRect(FLinearColor(0.1f, 0.15f, 0.2f, 0.9f), RowX, RowY, RowW, CellH);
		FMMOClickRegion R;
		R.SkillId = Def.QuestId;
		R.Rect = FBox2D(FVector2D(RowX, RowY), FVector2D(RowX + RowW, RowY + CellH));
		QuestAcceptRegions.Add(R);

		DrawQuestRowText(FString::Printf(TEXT("%s   [Lv%d gerekli]"), *Def.Name, Def.RequiredLevel),
			Def.Description,
			FString::Printf(TEXT("   Odul: %d XP, %d altin%s"), Def.RewardXP, Def.RewardGold, *BuildQuestRewardItemSuffix(Def)), RowY);
		RowY += RowH;
	}

	RowY += 10.f;
	DrawText(TEXT("Aktif gorevler (tamamsa tikla = odul al)"), FLinearColor(1.f, 0.85f, 0.4f), RowX, RowY, nullptr, 0.9f);
	RowY += 20.f;

	for (const FMMOQuestProgress& P : Q->ActiveQuests)
	{
		const FMMOQuestDef* Def = FMMOQuestDatabase::Find(P.QuestId);
		if (!Def)
		{
			continue;
		}
		const bool bDone = Q->IsQuestComplete(*Def, P);
		DrawRect(bDone ? FLinearColor(0.1f, 0.22f, 0.1f, 0.9f) : FLinearColor(0.13f, 0.13f, 0.16f, 0.9f), RowX, RowY, RowW, CellH);
		if (bDone)
		{
			FMMOClickRegion R;
			R.SkillId = Def->QuestId;
			R.Rect = FBox2D(FVector2D(RowX, RowY), FVector2D(RowX + RowW, RowY + CellH));
			QuestClaimRegions.Add(R);
		}
		DrawQuestRowText(FString::Printf(TEXT("%s   %d/%d %s"), *Def->Name, Q->GetProgress(*Def), Def->TargetCount,
			bDone ? TEXT("- TAMAM, tikla!") : TEXT("")),
			FString(),
			FString::Printf(TEXT("Odul: %d XP, %d altin%s"), Def->RewardXP, Def->RewardGold, *BuildQuestRewardItemSuffix(*Def)), RowY);
		RowY += RowH;
	}
}

bool AMMOHUD::HandleQuestClick(const FVector2D& Pos)
{
	if (!bQuestWindowOpen)
	{
		return false;
	}

	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	UMMOQuestComponent* Q = Char ? Char->GetQuestComponent() : nullptr;
	if (!Q)
	{
		return true;
	}

	for (const FMMOClickRegion& R : QuestAcceptRegions)
	{
		if (!R.SkillId.IsNone() && R.Rect.IsInside(Pos))
		{
			// Gorev alis repligi (docs/Lore.md -> MMOQuestDialogue.cpp): sunucu gorevi gercekten
			// kabul edince gosterilir (UpdatePendingQuestLines); yoksa sessizce gec.
			// Host'ta AcceptQuest senkron calisir; bu yuzden bekleme kaydi cagridan ONCE eklenir.
			const bool bWasActive = Q->IsQuestActive(R.SkillId);
			if (!bWasActive)
			{
				PendingQuestLines.Add({ R.SkillId, false, GetWorld()->GetTimeSeconds() + 2.0 });
			}
			Q->AcceptQuest(R.SkillId);
			return true;
		}
	}
	for (const FMMOClickRegion& R : QuestClaimRegions)
	{
		if (!R.SkillId.IsNone() && R.Rect.IsInside(Pos))
		{
			// Teslim repligi: gorev CompletedQuests'e girince gosterilir (kayit cagridan once).
			if (!Q->IsCompleted(R.SkillId))
			{
				PendingQuestLines.Add({ R.SkillId, true, GetWorld()->GetTimeSeconds() + 2.0 });
			}
			Q->ClaimReward(R.SkillId);
			return true;
		}
	}
	return true;
}

void AMMOHUD::UpdatePendingQuestLines(const UMMOQuestComponent* Q)
{
	if (PendingQuestLines.Num() == 0 || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 i = PendingQuestLines.Num() - 1; i >= 0; --i)
	{
		const FPendingQuestLine P = PendingQuestLines[i];
		const bool bDone = Q && (P.bClaim ? Q->IsCompleted(P.QuestId) : Q->IsQuestActive(P.QuestId));
		if (bDone)
		{
			const FString Line = P.bClaim ? FMMOQuestDialogue::GetTurnInLine(P.QuestId)
				: FMMOQuestDialogue::GetAcceptLine(P.QuestId);
			if (!Line.IsEmpty())
			{
				ShowNpcDialogue(TEXT("Yasli Doran"), Line);
			}
			PendingQuestLines.RemoveAt(i);
		}
		else if (!Q || Now > P.ExpireTime)
		{
			PendingQuestLines.RemoveAt(i);
		}
	}
}

// --- Ilk 10 dakika ipuclari (docs/CONTENT_PLAN_LV11_45.md bolum C) ---------------------------------
// Gosterim kanali: ShowNpcDialogue (alt-orta balon, 9 sn). Neden: AddFloatingLabel/ClientShowHint karakter
// ustunde 2 sn'de yukari kayip siliniyor, ~50 karakterlik cumle okunamaz; NPC balonu pencere degil
// (hareketi engellemez), okunacak kadar kalir ve tek slot oldugu icin "ayni anda tek ipucu" kendiliginden saglanir.
// Tetikleyiciler yerel replike durumun izlenmesiyle bulunur (HUD yalniz yerel oyuncuda vardir): sunucu kodu,
// RPC, replikasyon ve MMOSaveGame degismez. Gosterilenler maskesi oturumluktur (ULocalPlayer basina bellekte).
namespace
{
	enum EMMOTutorialHint : int32
	{
		HintMove = 0,   // H1
		HintNpc,        // H2
		HintCombat,     // H3
		HintLoot,       // H4
		HintLevelUp,    // H5
		HintQuestDone,  // H6
		HintDeath,      // H7
		HintBagFull,    // H8
		HintCount
	};

	struct FTutorialHintDef
	{
		const TCHAR* Text;
		int32 MaxLevel; // bu seviyenin ustundeki oyuncuya gosterilmez
	};

	// Metinler ASCII Turkce, <=50 karakter. Seviye kosullari plandan; H3/H8 icin plan vermedigi icin
	// koruyucu ust sinir konuldu (yuksek seviyeli oyuncuya "ilk gorev" ipucu cikmasin).
	const FTutorialHintDef GTutorialHints[HintCount] =
	{
		{ TEXT("Zemine sol tikla: yuru. Sag tik surukle: kamera"), 1 },
		{ TEXT("Yasli Doran'in ustundeki ! isaretine tikla"), 2 },
		{ TEXT("Z: hedef sec  R: saldir  1-0: beceri  F: iksir"), 10 },
		{ TEXT("Esya dustu: uzerinden gec. I: canta, tikla = kusan"), 5 },
		{ TEXT("Seviye atladin! C: stat puani, K: beceri puani"), 10 },
		{ TEXT("Gorev tamam! Yasli Doran'a don, tikla, odulu al"), 10 },
		{ TEXT("Oldun: 5 sn sonra dogarsin, biraz EXP kaybettin"), 20 },
		{ TEXT("Canta dolu! Satici NPC'ye esya sat veya depoya koy"), 20 },
	};

	constexpr float HintPendingExpire = 20.f;  // siradaki ipucu bu kadar sn icinde gosterilemezse dusurulur
	constexpr float HintGapSeconds = 3.f;      // iki ipucu arasi bosluk (balon suresinin ustune)
	constexpr float HintWarmupSeconds = 3.f;   // pawn ilk goruldugunde replikasyonun oturmasini bekle
	constexpr float HintQuestAcceptQuietSeconds = 4.f; // gorev kabulunden sonra NPC kabul satiri icin susma

	uint16& GetSessionHintMask(const ULocalPlayer* LP)
	{
		static TMap<TWeakObjectPtr<const ULocalPlayer>, uint16> Masks;
		static uint16 Fallback = 0;
		if (!LP)
		{
			return Fallback;
		}
		if (Masks.Num() > 8)
		{
			for (auto It = Masks.CreateIterator(); It; ++It)
			{
				if (!It.Key().IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
		return Masks.FindOrAdd(TWeakObjectPtr<const ULocalPlayer>(LP));
	}
}

void AMMOHUD::QueueHint(int32 HintIndex, int32 PlayerLevel)
{
	if (HintIndex < 0 || HintIndex >= HintCount || PlayerLevel > GTutorialHints[HintIndex].MaxLevel || !GetWorld())
	{
		return;
	}
	const APlayerController* PC = GetOwningPlayerController();
	const uint16 Bit = (uint16)(1u << HintIndex);
	// Ekranda gosterilmekte olan ipucu (henuz "gosterildi" biti atilmadi) tekrar siraya girmesin.
	if ((GetSessionHintMask(PC ? PC->GetLocalPlayer() : nullptr) & Bit) != 0 || (HintPendingMask & Bit) != 0
		|| HintActiveIndex == HintIndex)
	{
		return;
	}
	HintPendingMask |= Bit;
	HintQueuedAt[HintIndex] = GetWorld()->GetTimeSeconds();
}

void AMMOHUD::UpdateTutorialHints(AMMOCharacter* PlayerChar)
{
	UWorld* World = GetWorld();
	UMMOStatsComponent* S = PlayerChar ? PlayerChar->GetStats() : nullptr;
	if (!World || !S || !PlayerChar->IsLocallyControlled())
	{
		return;
	}
	const APlayerController* PC = GetOwningPlayerController();
	uint16& ShownMask = GetSessionHintMask(PC ? PC->GetLocalPlayer() : nullptr);
	const float Now = World->GetTimeSeconds();
	const int32 Lv = S->Level;
	const UMMOQuestComponent* Q = PlayerChar->GetQuestComponent();
	const UMMOInventoryComponent* Inv = PlayerChar->GetInventory();
	const int32 QuestCount = Q ? Q->ActiveQuests.Num() : 0;
	const bool bBagFull = Inv && Inv->EquipmentBag.Num() >= UMMOInventoryComponent::MaxBagSize;
	const bool bDead = S->bIsDead;

	// Yeni pawn (ilk kare / yeniden olusma): izleme taban cizgisini sifirla, replikasyon oturana kadar sessiz kal.
	if (HintPawn.Get() != PlayerChar)
	{
		HintPawn = PlayerChar;
		HintPawnFirstSeen = Now;
		HintMoveWatchStart = -1.f;
		HintQuestAcceptedAt = -1.f;
		HintPrevLevel = 0;
		HintPrevQuestCount = -1;
	}
	const bool bWarm = Now - HintPawnFirstSeen >= HintWarmupSeconds;

	// Gosterilmekte olan ipucunun akibeti: balon tam sure kaldiysa "gosterildi" say; arada baska bir
	// replik (NPC/LLM/gorev satiri) balonu ezdiyse maskeyi atma, ipucunu yeniden siraya koy.
	if (HintActiveIndex >= 0)
	{
		const uint16 ActiveBit = (uint16)(1u << HintActiveIndex);
		if (Now - HintActiveStart >= NpcDialogueDuration)
		{
			ShownMask |= ActiveBit;
			HintActiveIndex = -1;
		}
		else if (NpcDialogueStartTime != HintActiveStart || NpcDialogueText != GTutorialHints[HintActiveIndex].Text)
		{
			HintPendingMask |= ActiveBit;
			HintQueuedAt[HintActiveIndex] = Now;
			HintActiveIndex = -1;
		}
	}

	if (bWarm)
	{
		// Gorev kabul edildi (0 -> >0): H2 ("! isaretine tikla") artik bayat; ayrica NPC'nin kabul satiri
		// <=2 sn gecikmeyle gelip balonu ezecegi icin kisa sure ipucu gosterme.
		if (HintPrevQuestCount == 0 && QuestCount > 0)
		{
			ShownMask |= (uint16)(1u << HintNpc);
			HintPendingMask &= ~(uint16)(1u << HintNpc);
			HintNextAllowedTime = FMath::Max(HintNextAllowedTime, Now + HintQuestAcceptQuietSeconds);
		}

		// H5: seviye atlama (OnLevelUp'in istemci karsiligi: replike Level tam +1 artti), harcanmamis puan var.
		if (HintPrevLevel > 0 && Lv == HintPrevLevel + 1 && (S->UnspentStatPoints + S->UnspentSkillPoints) > 0)
		{
			QueueHint(HintLevelUp, Lv);
		}
		// H7: ilk olum.
		if (!bHintPrevDead && bDead)
		{
			QueueHint(HintDeath, Lv);
		}
		// H8: canta doldu (bos -> dolu gecisi).
		if (!bHintPrevBagFull && bBagFull)
		{
			QueueHint(HintBagFull, Lv);
		}
		// H3: gorev kabul edildi (0 -> >0); 20 sn icinde seviye/EXP degismediyse (olduruldugu yok) kombo ipucu.
		if (HintPrevQuestCount == 0 && QuestCount > 0 && Lv <= GTutorialHints[HintCombat].MaxLevel)
		{
			HintQuestAcceptedAt = Now;
			HintQuestAcceptLevel = Lv;
			HintQuestAcceptExp = S->Experience;
		}
		if (HintQuestAcceptedAt >= 0.f)
		{
			if (Lv != HintQuestAcceptLevel || S->Experience != HintQuestAcceptExp)
			{
				HintQuestAcceptedAt = -1.f; // oyuncu zaten dovusuyor
			}
			else if (Now - HintQuestAcceptedAt >= 20.f)
			{
				HintQuestAcceptedAt = -1.f;
				QueueHint(HintCombat, Lv);
			}
		}
		// H6: hedefe ulasmis ama teslim edilmemis gorev.
		if (Q && QuestCount > 0 && Lv <= GTutorialHints[HintQuestDone].MaxLevel)
		{
			for (const FMMOQuestProgress& P : Q->ActiveQuests)
			{
				const FMMOQuestDef* Def = FMMOQuestDatabase::Find(P.QuestId);
				if (Def && Q->IsQuestComplete(*Def, P))
				{
					QueueHint(HintQuestDone, Lv);
					break;
				}
			}
		}

		// H1: yeni karakter (Lv1, 0 EXP, gorev yok), sinif paneli kapali; 8 sn hareket etmediyse. Hareket = kalici iptal.
		const uint16 MoveBit = (uint16)(1u << HintMove);
		if ((ShownMask & MoveBit) == 0)
		{
			if (Lv == 1 && S->Experience == 0 && QuestCount == 0 && !bDead && !PlayerChar->ShouldShowClassHint())
			{
				if (HintMoveWatchStart < 0.f)
				{
					HintMoveWatchStart = Now;
					HintMoveOrigin = PlayerChar->GetActorLocation();
				}
				else if (FVector::DistSquared(PlayerChar->GetActorLocation(), HintMoveOrigin) > 80.f * 80.f)
				{
					ShownMask |= MoveBit;
					HintPendingMask &= ~MoveBit;
				}
				else if (Now - HintMoveWatchStart >= 8.f)
				{
					QueueHint(HintMove, Lv);
				}
			}
			else
			{
				HintMoveWatchStart = -1.f;
				HintPendingMask &= ~MoveBit;
			}
		}

		// H2: Lv1-2, gorev yok (aktif/tamamlanmis), gorevli NPC'ye <=900 u. 1 sn'de bir tara.
		if (Now >= HintNextNpcScan && Lv <= GTutorialHints[HintNpc].MaxLevel && QuestCount == 0
			&& (!Q || Q->CompletedQuests.Num() == 0) && (ShownMask & (1u << HintNpc)) == 0)
		{
			HintNextNpcScan = Now + 1.f;
			const FVector PawnLoc = PlayerChar->GetActorLocation();
			for (TActorIterator<AMMOQuestGiverNPC> It(World); It; ++It)
			{
				if (FVector::DistSquared(It->GetActorLocation(), PawnLoc) <= 900.f * 900.f)
				{
					QueueHint(HintNpc, Lv);
					break;
				}
			}
		}

		// H4: yakinda yerde ganimet var (ilk oldurmenin dusurdugu). 0.5 sn'de bir tara.
		if (Now >= HintNextLootScan && Lv <= GTutorialHints[HintLoot].MaxLevel && (ShownMask & (1u << HintLoot)) == 0)
		{
			HintNextLootScan = Now + 0.5f;
			const FVector PawnLoc = PlayerChar->GetActorLocation();
			for (TActorIterator<AMMOLootPickup> It(World); It; ++It)
			{
				if (FVector::DistSquared(It->GetActorLocation(), PawnLoc) <= 1500.f * 1500.f)
				{
					QueueHint(HintLoot, Lv);
					break;
				}
			}
		}
	}

	HintPrevLevel = Lv;
	HintPrevQuestCount = QuestCount;
	bHintPrevDead = bDead;
	bHintPrevBagFull = bBagFull;

	// Gosterim: tek slot. Pencere acik / NPC balonu ekranda / onceki ipucundan sonra bosluk bitmedi ise bekle.
	if (HintPendingMask == 0 || Now < HintNextAllowedTime || IsNpcDialogueShowing(Now) || IsAnyWindowOpen())
	{
		return;
	}
	for (int32 i = 0; i < HintCount; ++i)
	{
		const uint16 Bit = (uint16)(1u << i);
		if ((HintPendingMask & Bit) == 0)
		{
			continue;
		}
		// Eskimis ya da seviyesi gecmis istekleri dusur.
		if (Now - HintQueuedAt[i] > HintPendingExpire || Lv > GTutorialHints[i].MaxLevel)
		{
			HintPendingMask &= ~Bit;
			continue;
		}
		// Olu iken yalniz olum ipucu (olum ekraninin altinda, hemen anlami var); digerleri canlanmayi bekler.
		if (bDead && i != HintDeath)
		{
			continue;
		}
		// Gosterim aninda kosulu yeniden dogrula: H2 kuyrukta beklerken gorev alindiysa bayat kalir.
		if (i == HintNpc && (QuestCount > 0 || (Q && Q->CompletedQuests.Num() > 0)))
		{
			HintPendingMask &= ~Bit;
			continue;
		}
		ShowNpcDialogue(TEXT("Ipucu"), GTutorialHints[i].Text);
		// "Gosterildi" biti hemen atilmaz: balon tam sure ezilmeden kalirsa yukarida atilir.
		HintActiveIndex = i;
		HintActiveStart = NpcDialogueStartTime;
		HintPendingMask &= ~Bit;
		HintNextAllowedTime = Now + NpcDialogueDuration + HintGapSeconds;
		break;
	}
}

void AMMOHUD::DrawStatWindow(AMMOCharacter* PlayerChar)
{
	StatPlusRegions.Reset();
	if (!Canvas || !PlayerChar)
	{
		return;
	}
	UMMOStatsComponent* S = PlayerChar->GetStats();
	if (!S)
	{
		return;
	}

	const float PW = 380.f;
	const float PH = 320.f;
	const float PX = ((float)Canvas->SizeX - PW) * 0.5f + 210.f;
	const float PY = 110.f;
	DrawRect(FLinearColor(0.03f, 0.03f, 0.05f, 0.94f), PX, PY, PW, PH);
	DrawRect(FLinearColor(0.6f, 0.75f, 1.f, 0.95f), PX, PY, PW, 4.f);
	StatRect = FBox2D(FVector2D(PX, PY), FVector2D(PX + PW, PY + PH));
	StatCloseRect = DrawCloseButton(PX, PY, PW);

	FString CharTitle = TEXT("KARAKTER");
	if (const UMMOSkillComponent* SkillComp = PlayerChar->GetSkillComponent())
	{
		CharTitle += FString::Printf(TEXT(" - %s"), *GetMMOClassDisplayName(SkillComp->CharacterClass));
	}
	DrawText(CharTitle, FLinearColor::White, PX + 14.f, PY + 10.f, nullptr, 1.15f);

	DrawText(FString::Printf(TEXT("Seviye %d     Dagitilacak puan: %d"), S->Level, S->UnspentStatPoints),
		S->UnspentStatPoints > 0 ? FLinearColor(1.f, 0.85f, 0.3f) : FLinearColor(0.7f, 0.75f, 0.8f),
		PX + 14.f, PY + 42.f, nullptr, 0.9f);

	// Ana statlar + [+] butonu.
	struct FStatRow { const TCHAR* Label; const TCHAR* Key; int32 Value; };
	const FStatRow Rows[] = {
		{ TEXT("Guc (STR)"),    TEXT("STR"), S->Strength },
		{ TEXT("Ceviklik (DEX)"), TEXT("DEX"), S->Dexterity },
		{ TEXT("Zeka (INT)"),   TEXT("INT"), S->Intelligence },
		{ TEXT("Dayaniklilik (VIT)"), TEXT("VIT"), S->Vitality },
	};

	float RowY = PY + 74.f;
	const float RowH = 30.f;
	const bool bHasPoints = S->UnspentStatPoints > 0;
	for (const FStatRow& Row : Rows)
	{
		DrawText(FString::Printf(TEXT("%s:  %d"), Row.Label, Row.Value), FLinearColor::White,
			PX + 18.f, RowY, nullptr, 0.95f);

		// [+] butonu (puan varsa aktif).
		const float BtnW = 30.f;
		const float BtnX = PX + PW - BtnW - 18.f;
		const FLinearColor BtnColor = bHasPoints ? FLinearColor(0.2f, 0.5f, 0.25f) : FLinearColor(0.2f, 0.2f, 0.22f);
		DrawRect(BtnColor, BtnX, RowY - 2.f, BtnW, 24.f);
		DrawText(TEXT("+"), FLinearColor::White, BtnX + 10.f, RowY, nullptr, 1.1f);

		if (bHasPoints)
		{
			FMMOClickRegion R;
			R.SkillId = FName(Row.Key);
			R.Rect = FBox2D(FVector2D(BtnX, RowY - 2.f), FVector2D(BtnX + BtnW, RowY + 22.f));
			StatPlusRegions.Add(R);
		}
		RowY += RowH;
	}

	// Turetilmis statlar (ozet).
	RowY += 8.f;
	DrawRect(FLinearColor(0.2f, 0.25f, 0.35f, 0.6f), PX + 14.f, RowY, PW - 28.f, 2.f);
	RowY += 10.f;
	DrawText(FString::Printf(TEXT("Saldiri: %d      Savunma: %d"),
		FMath::RoundToInt(S->AttackPower), FMath::RoundToInt(S->Defense)),
		FLinearColor(0.8f, 0.85f, 0.95f), PX + 18.f, RowY, nullptr, 0.85f);
	RowY += 22.f;
	DrawText(FString::Printf(TEXT("Max HP: %d      Max MP: %d"),
		FMath::RoundToInt(S->MaxHealth), FMath::RoundToInt(S->MaxMana)),
		FLinearColor(0.8f, 0.85f, 0.95f), PX + 18.f, RowY, nullptr, 0.85f);
}

bool AMMOHUD::HandleStatClick(const FVector2D& Pos)
{
	if (!bStatWindowOpen)
	{
		return false;
	}

	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	if (!Char)
	{
		return true;
	}

	for (const FMMOClickRegion& R : StatPlusRegions)
	{
		if (!R.SkillId.IsNone() && R.Rect.IsInside(Pos))
		{
			Char->RequestSpendStat(R.SkillId);
			return true;
		}
	}
	return true;
}

FBox2D AMMOHUD::DrawCloseButton(float PX, float PY, float PW)
{
	const float S = 22.f;
	const float BX = PX + PW - S - 6.f;
	const float BY = PY + 6.f;
	DrawRect(FLinearColor(0.55f, 0.12f, 0.12f, 0.95f), BX, BY, S, S);
	DrawRect(FLinearColor(0.85f, 0.3f, 0.3f, 1.f), BX, BY, S, 2.f);
	DrawText(TEXT("X"), FLinearColor::White, BX + 6.f, BY + 2.f, nullptr, 1.05f);
	return FBox2D(FVector2D(BX, BY), FVector2D(BX + S, BY + S));
}

bool AMMOHUD::HandleClassChoiceClick(const FVector2D& Pos)
{
	AMMOCharacter* Char = Cast<AMMOCharacter>(GetOwningPawn());
	if (!Char || !Char->ShouldShowClassHint() || IsAnyWindowOpen()
		|| !ClassChoicePanelRect.IsInside(Pos))
	{
		return false;
	}

	for (const FMMOClickRegion& Region : ClassChoiceRegions)
	{
		if (Region.Rect.IsInside(Pos) && Region.Slot >= 0 && Region.Slot <= (int32)EMMOClass::Priest)
		{
			if (UMMOSkillComponent* Skills = Char->GetSkillComponent())
			{
				Skills->RequestSetupForClass((EMMOClass)Region.Slot);
			}
			break;
		}
	}
	return true; // Panelin bos alanina tiklama da karakteri hareket ettirmez.
}

bool AMMOHUD::HandleZoneTravelClick(const FVector2D& Pos)
{
	if (!ZoneTravelRect.bIsValid || !ZoneTravelRect.IsInside(Pos) || IsAnyWindowOpen())
	{
		return false;
	}
	AMMOPlayerController* PC = Cast<AMMOPlayerController>(GetOwningPlayerController());
	if (PC && PC->HasAuthority() && PC->IsLocalController())
	{
		PC->MMOTravel();
	}
	return true;
}

bool AMMOHUD::HandleWindowClick(const FVector2D& Pos)
{
	// Ustteki pencere oncelikli (DrawHUD ciziminin tersi): Stat -> Shop -> Inventory -> SkillBook -> Quest.
	// Tiklama bir pencerenin icindeyse: X'e denk geldiyse kapat, yoksa pencerenin kendi tiklamasini isle.
	if (bStatWindowOpen && StatRect.IsInside(Pos))
	{
		if (StatCloseRect.IsInside(Pos)) { bStatWindowOpen = false; } else { HandleStatClick(Pos); }
		return true;
	}
	if (IsTradeOpen() && TradeRect.IsInside(Pos))
	{
		if (TradeCloseRect.IsInside(Pos))
		{
			// X = ticareti iptal et.
			if (AMMOCharacter* TC = Cast<AMMOCharacter>(GetOwningPawn()))
			{
				if (UMMOTradeComponent* T = TC->GetTradeComponent()) { T->RequestCancel(); }
			}
		}
		else
		{
			HandleTradeClick(Pos);
		}
		return true;
	}
	if (bWarehouseOpen && WarehouseRect.IsInside(Pos))
	{
		if (WarehouseCloseRect.IsInside(Pos)) { bWarehouseOpen = false; } else { HandleWarehouseClick(Pos); }
		return true;
	}
	if (bShopOpen && ShopRect.IsInside(Pos))
	{
		if (ShopCloseRect.IsInside(Pos)) { bShopOpen = false; } else { HandleShopClick(Pos); }
		return true;
	}
	if (bInventoryOpen && InventoryRect.IsInside(Pos))
	{
		if (InventoryCloseRect.IsInside(Pos)) { bInventoryOpen = false; } else { HandleInventoryClick(Pos); }
		return true;
	}
	if (bSkillBookOpen && SkillBookRect.IsInside(Pos))
	{
		if (SkillBookCloseRect.IsInside(Pos)) { bSkillBookOpen = false; PendingAssignSkill = NAME_None; } else { HandleSkillBookClick(Pos); }
		return true;
	}
	// Kitap acikken hotbar'a tiklama (atama/temizleme icin) SkillBookRect'in DISINDA
	// kalabiliyor (panel hotbar cizgisinin ustunde bitiyor, cogu cozunurlukte hotbar
	// disarida kalir) — bu yuzden "skil sec -> hotbar'a tikla = ata" akisi hic
	// tetiklenmiyordu (bug-hunt). HandleSkillBookClick zaten HotbarRegions'i kontrol
	// ediyor; SkillBookRect sartindan BAGIMSIZ olarak da dene.
	if (bSkillBookOpen)
	{
		for (const FMMOClickRegion& R : HotbarRegions)
		{
			if (R.Slot >= 0 && R.Rect.IsInside(Pos))
			{
				HandleSkillBookClick(Pos);
				return true;
			}
		}
	}
	if (bQuestWindowOpen && QuestRect.IsInside(Pos))
	{
		if (QuestCloseRect.IsInside(Pos)) { bQuestWindowOpen = false; } else { HandleQuestClick(Pos); }
		return true;
	}
	return false;
}

bool AMMOHUD::HandleWindowRightClick(const FVector2D& Pos)
{
	// Sag tik: skill kitabinda skili hotbara ata; envanterde ekipmani yukselt (imlecin ustundeki pencereye gore).
	if (bStatWindowOpen && StatRect.IsInside(Pos)) { return true; }
	if (IsTradeOpen() && TradeRect.IsInside(Pos)) { return true; }
	if (bWarehouseOpen && WarehouseRect.IsInside(Pos)) { return true; }
	if (bShopOpen && ShopRect.IsInside(Pos)) { return true; }
	if (bInventoryOpen && InventoryRect.IsInside(Pos)) { HandleInventoryRightClick(Pos); return true; }
	if (bSkillBookOpen && SkillBookRect.IsInside(Pos)) { HandleSkillBookRightClick(Pos); return true; }
	if (bQuestWindowOpen && QuestRect.IsInside(Pos)) { return true; }
	return false;
}
