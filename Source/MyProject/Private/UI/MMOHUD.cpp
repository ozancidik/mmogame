// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOHUD.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOPartyComponent.h"
#include "Effects/MMOEffectDatabase.h"
#include "Items/MMOConsumableTypes.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOSkillComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Skills/MMOSkillDatabase.h"
#include "Items/MMOItemDatabase.h"
#include "Quests/MMOQuestTypes.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOQuestGiverNPC.h"
#include "World/MMOTalkerNPC.h"
#include "World/MMOHealerNPC.h"
#include "Player/MMOPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

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

	AMMOCharacter* MMOChar = Cast<AMMOCharacter>(GetOwningPawn());
	if (!MMOChar)
	{
		DrawPlayerCursor();
		return;
	}

	UMMOStatsComponent* S = MMOChar->GetStats();
	if (!S)
	{
		DrawPlayerCursor();
		return;
	}

	const float X = BarOriginX;
	const float Y = BarOriginY;

	// Seviye yaz�s� (barlar�n �st�nde).
	DrawText(FString::Printf(TEXT("Lv %d"), S->Level), FLinearColor::White, X, Y - 26.f, nullptr, 1.3f);

	// HP bar� (k�rm�z�).
	DrawStatBar(X, Y, BarWidth, 26.f, S->GetHealthPercent(), FLinearColor(0.78f, 0.12f, 0.12f, 1.f));
	DrawText(FString::Printf(TEXT("HP  %d / %d"), FMath::RoundToInt(S->Health), FMath::RoundToInt(S->MaxHealth)),
		FLinearColor::White, X + 8.f, Y + 4.f);

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
	if (bStatWindowOpen)
	{
		DrawStatWindow(MMOChar);
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

	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		const FMMOActiveEffect& E = Effects[i];
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
		DrawText(FString::Printf(TEXT("%.0f"), FMath::CeilToFloat(E.RemainingTime)),
			FLinearColor(0.9f, 0.9f, 0.9f), BX + 5.f, StartY + 18.f, nullptr, 0.85f);

		// Suzulme cubugu (kalan/toplam).
		const float Pct = E.Duration > 0.f ? FMath::Clamp(E.RemainingTime / E.Duration, 0.f, 1.f) : 0.f;
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

	DrawText(TEXT("PARTI"), FLinearColor(0.6f, 0.85f, 1.f), StartX, StartY, nullptr, 0.85f);
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

		DrawText(FString::Printf(TEXT("%s  Lv%d"), *Name, S->Level),
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

void AMMOHUD::AddDamageNumber(const FVector& WorldLocation, float Amount)
{
	FMMOFloatingText FT;
	FT.WorldLocation = WorldLocation;
	FT.Amount = Amount;
	FT.SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
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

		// Isim + seviye plakasi (bar ustunde).
		if (const AMMOMob* Mob = Cast<AMMOMob>(C))
		{
			const FString Plate = FString::Printf(TEXT("%s Lv%d"), *Mob->GetMobName(), Mob->GetMobLevel());
			DrawText(Plate, FLinearColor(1.f, 0.75f, 0.7f), BX, BY - 15.f, nullptr, 0.72f);
		}

		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f), BX - 1.f, BY - 1.f, BarW + 2.f, BarH + 2.f);
		DrawRect(FLinearColor(0.15f, 0.15f, 0.16f, 0.9f), BX, BY, BarW, BarH);
		DrawRect(FLinearColor(0.85f, 0.12f, 0.12f, 1.f), BX, BY, BarW * S->GetHealthPercent(), BarH);
	}
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

	// Ad + seviye (dusman kirmizisi — KO hedef rengi).
	FString TargetName = TEXT("Canavar");
	if (const AMMOMob* Mob = Cast<AMMOMob>(Target))
	{
		TargetName = Mob->GetMobName();
	}
	DrawText(FString::Printf(TEXT("%s  Lv %d"), *TargetName, S->Level), FLinearColor(1.f, 0.35f, 0.3f),
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
	const float Lifetime = 1.1f;

	// Geriye dogru gez (suresi dolani guvenle sil).
	for (int32 i = FloatingTexts.Num() - 1; i >= 0; --i)
	{
		const FMMOFloatingText& FT = FloatingTexts[i];
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

		const FString Txt = FString::Printf(TEXT("%d"), FMath::RoundToInt(FT.Amount));
		DrawText(Txt, FLinearColor(1.f, 0.85f, 0.2f, Alpha), Screen.X - 8.f, Screen.Y, nullptr, 1.4f);
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

	FString ClassName;
	switch (SkillComp->CharacterClass)
	{
	case EMMOClass::Warrior: ClassName = TEXT("Savasci"); break;
	case EMMOClass::Archer:  ClassName = TEXT("Okcu");    break;
	case EMMOClass::Mage:    ClassName = TEXT("Buyucu");  break;
	case EMMOClass::Priest:  ClassName = TEXT("Priest");  break;
	}

	DrawText(FString::Printf(TEXT("SKILL KITABI - %s"), *ClassName), FLinearColor::White, PX + 14.f, PY + 10.f, nullptr, 1.3f);
	DrawText(FString::Printf(TEXT("Skill Puani: %d     Level: %d"), Stats->UnspentSkillPoints, Stats->Level),
		FLinearColor(1.f, 0.85f, 0.2f), PX + 14.f, PY + 36.f, nullptr, 1.f);
	DrawText(TEXT("Skile tikla = ogren / ele al.  Sonra bir hotbar slotuna tikla = ata."),
		FLinearColor(0.7f, 0.75f, 0.8f), PX + 14.f, PY + 58.f, nullptr, 0.75f);

	const TArray<FMMOSkillDef> ClassSkills = FMMOSkillDatabase::GetForClass(SkillComp->CharacterClass);

	const EMMOSkillTree Trees[5] = { EMMOSkillTree::Basic, EMMOSkillTree::Special0, EMMOSkillTree::Special1, EMMOSkillTree::Special2, EMMOSkillTree::Special3 };
	const TCHAR* TreeNames[5] = { TEXT("Temel"), TEXT("Ustalik 1"), TEXT("Ustalik 2"), TEXT("Ustalik 3"), TEXT("Ustalik 4") };

	float RowY = PY + 82.f;
	const float RowX = PX + 14.f;
	const float RowW = PW - 28.f;
	const float RowH = 32.f;

	for (int32 t = 0; t < 5; ++t)
	{
		TArray<const FMMOSkillDef*> InTree;
		for (const FMMOSkillDef& D : ClassSkills)
		{
			if (D.Tree == Trees[t])
			{
				InTree.Add(&D);
			}
		}
		if (InTree.Num() == 0)
		{
			continue;
		}

		DrawText(TreeNames[t], FLinearColor(0.6f, 0.8f, 1.f), RowX, RowY, nullptr, 0.95f);
		RowY += 22.f;

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

			const FString Status = bLearned ? TEXT("[Ogrenildi]") : (bCanLearn ? TEXT("[Ogren]") : FString::Printf(TEXT("[%s]"), *Reason));
			DrawText(FString::Printf(TEXT("%s   %s"), *D->Name, *Status), FLinearColor::White, RowX + 6.f, RowY + 3.f, nullptr, 0.82f);
			DrawText(FString::Printf(TEXT("Lv%d  SP%d  Mana%d"), D->RequiredLevel, D->SkillPointCost, FMath::RoundToInt(D->ManaCost)),
				FLinearColor(0.7f, 0.75f, 0.85f), RowX + 6.f, RowY + 16.f, nullptr, 0.68f);

			RowY += RowH;
		}
		RowY += 6.f;
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

	// --- Canta ---
	DrawText(FString::Printf(TEXT("CANTA (%d)"), Inv->EquipmentBag.Num()), FLinearColor(0.6f, 0.8f, 1.f), RowX, RowY, nullptr, 0.95f);
	RowY += 20.f;

	const int32 MaxRows = 8;
	const int32 ShowCount = FMath::Min(Inv->EquipmentBag.Num(), MaxRows);
	for (int32 i = 0; i < ShowCount; ++i)
	{
		const FMMOEquipmentInstance& Inst = Inv->EquipmentBag[i];
		const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId);
		if (!Def)
		{
			continue;
		}

		const bool bMaxed = Inst.Plus >= FMMOItemDatabase::MaxPlus;
		DrawRect(FLinearColor(0.1f, 0.13f, 0.2f, 0.9f), RowX, RowY, RowW, RowH - 4.f);

		FMMOClickRegion R;
		R.Slot = i;
		R.Rect = FBox2D(FVector2D(RowX, RowY), FVector2D(RowX + RowW, RowY + RowH - 4.f));
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
			Line += FString::Printf(TEXT("   [+%d: %da %%%d%s]"), Inst.Plus + 1,
				FMMOItemDatabase::GetUpgradeCost(Inst.Plus),
				FMMOItemDatabase::GetUpgradeChancePct(Inst.Plus),
				FMMOItemDatabase::BurnsOnFail(Inst.Plus) ? TEXT(" YAKAR!") : TEXT(""));
		}
		else
		{
			Line += TEXT("   [MAX]");
		}

		DrawText(Line, RowColor, RowX + 6.f, RowY + 4.f, nullptr, 0.72f);
		RowY += RowH;
	}

	if (Inv->EquipmentBag.Num() > MaxRows)
	{
		DrawText(FString::Printf(TEXT("... ve %d daha"), Inv->EquipmentBag.Num() - MaxRows),
			FLinearColor(0.6f, 0.6f, 0.6f), RowX, RowY, nullptr, 0.75f);
	}
	else if (Inv->EquipmentBag.Num() == 0)
	{
		DrawText(TEXT("(canta bos - moblardan yesil kureler dusur)"), FLinearColor(0.6f, 0.6f, 0.6f), RowX, RowY, nullptr, 0.75f);
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
			Inv->EquipFromBag(R.Slot);
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
			Inv->TryUpgrade(R.Slot);
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
	float LY = BuyStartY;

	// Satin-al sutununu panele sigdir: satir sayisi (iksir + tuketilebilir + ekipman)
	// arttikca satir yuksekligini kucultup tasmayi onle (kaydirma yok, hepsi gorunur kalir).
	const int32 BuyRowCount = 1 + FMMOConsumableDatabase::GetAll().Num() + FMMOItemDatabase::GetAll().Num();
	const float BuyAvailH = (PY + PH - 12.f) - BuyStartY;
	const float BuyRowH = FMath::Min(RowH, BuyAvailH / (float)FMath::Max(1, BuyRowCount));
	const float BuyCellH = FMath::Max(1.f, BuyRowH - 4.f);

	// Iksir satiri.
	{
		DrawRect(FLinearColor(0.1f, 0.16f, 0.12f, 0.9f), LeftX, LY, ColW, BuyCellH);
		FMMOClickRegion R;
		R.SkillId = FName(TEXT("__POTION__"));
		R.Rect = FBox2D(FVector2D(LeftX, LY), FVector2D(LeftX + ColW, LY + BuyCellH));
		ShopBuyRegions.Add(R);
		DrawText(FString::Printf(TEXT("Can Iksiri  -  %d altin"), FMMOItemDatabase::PotionPrice),
			FLinearColor::White, LeftX + 6.f, LY + 4.f, nullptr, 0.78f);
		LY += BuyRowH;
	}

	// Tuketilebilirler (mana iksiri, buff parsomenleri).
	for (const FMMOConsumableDef& Def : FMMOConsumableDatabase::GetAll())
	{
		DrawRect(FLinearColor(0.1f, 0.16f, 0.18f, 0.9f), LeftX, LY, ColW, BuyCellH);
		FMMOClickRegion R;
		R.SkillId = Def.ItemId;
		R.Rect = FBox2D(FVector2D(LeftX, LY), FVector2D(LeftX + ColW, LY + BuyCellH));
		ShopBuyRegions.Add(R);
		DrawText(FString::Printf(TEXT("%s  -  %d altin"), *Def.Name, Def.Price),
			FLinearColor::White, LeftX + 6.f, LY + 4.f, nullptr, 0.72f);
		LY += BuyRowH;
	}

	for (const FMMOItemDef& Def : FMMOItemDatabase::GetAll())
	{
		DrawRect(FLinearColor(0.1f, 0.13f, 0.2f, 0.9f), LeftX, LY, ColW, BuyCellH);
		FMMOClickRegion R;
		R.SkillId = Def.ItemId;
		R.Rect = FBox2D(FVector2D(LeftX, LY), FVector2D(LeftX + ColW, LY + BuyCellH));
		ShopBuyRegions.Add(R);
		DrawText(FString::Printf(TEXT("%s  -  %d altin (Lv%d)"), *Def.Name, FMMOItemDatabase::GetBuyPrice(Def), Def.RequiredLevel),
			FLinearColor::White, LeftX + 6.f, LY + 4.f, nullptr, 0.72f);
		LY += BuyRowH;
	}

	// --- SAT (sag sutun) ---
	DrawText(TEXT("SAT (sol tik)"), FLinearColor(1.f, 0.85f, 0.4f), RightX, PY + 68.f, nullptr, 0.95f);
	float RY = PY + 90.f;

	const int32 MaxRows = 12;
	const int32 ShowCount = FMath::Min(Inv->EquipmentBag.Num(), MaxRows);
	for (int32 i = 0; i < ShowCount; ++i)
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

	for (const FMMOClickRegion& R : ShopBuyRegions)
	{
		if (!R.SkillId.IsNone() && R.Rect.IsInside(Pos))
		{
			if (R.SkillId == FName(TEXT("__POTION__")))
			{
				Inv->BuyPotion(1);
			}
			else if (FMMOConsumableDatabase::Find(R.SkillId))
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
			Inv->SellEquipment(R.Slot);
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

	// Moblar (kirmizi)
	for (TActorIterator<AMMOMob> It(World); It; ++It)
	{
		if (*It && (*It)->GetStats() && !(*It)->GetStats()->bIsDead)
		{
			PlotDot((*It)->GetActorLocation(), FLinearColor(0.9f, 0.2f, 0.2f), 4.f);
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
	const float Line2Y = FMath::Min(24.f, RowH * 0.5f);

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

		DrawText(FString::Printf(TEXT("%s   [Lv%d gerekli]"), *Def.Name, Def.RequiredLevel),
			FLinearColor::White, RowX + 6.f, RowY + 3.f, nullptr, 0.78f);
		DrawText(FString::Printf(TEXT("%s   Odul: %d XP, %d altin%s"), *Def.Description, Def.RewardXP, Def.RewardGold, *BuildQuestRewardItemSuffix(Def)),
			FLinearColor(0.7f, 0.75f, 0.85f), RowX + 6.f, RowY + Line2Y, nullptr, 0.64f);
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
		DrawText(FString::Printf(TEXT("%s   %d/%d %s"), *Def->Name, Q->GetProgress(*Def), Def->TargetCount,
			bDone ? TEXT("- TAMAM, tikla!") : TEXT("")), FLinearColor::White, RowX + 6.f, RowY + 3.f, nullptr, 0.78f);
		DrawText(FString::Printf(TEXT("Odul: %d XP, %d altin%s"), Def->RewardXP, Def->RewardGold, *BuildQuestRewardItemSuffix(*Def)),
			FLinearColor(0.7f, 0.75f, 0.85f), RowX + 6.f, RowY + Line2Y, nullptr, 0.64f);
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
			Q->AcceptQuest(R.SkillId);
			return true;
		}
	}
	for (const FMMOClickRegion& R : QuestClaimRegions)
	{
		if (!R.SkillId.IsNone() && R.Rect.IsInside(Pos))
		{
			Q->ClaimReward(R.SkillId);
			return true;
		}
	}
	return true;
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

	DrawText(TEXT("KARAKTER"), FLinearColor::White, PX + 14.f, PY + 10.f, nullptr, 1.15f);

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

bool AMMOHUD::HandleWindowClick(const FVector2D& Pos)
{
	// Ustteki pencere oncelikli (DrawHUD ciziminin tersi): Stat -> Shop -> Inventory -> SkillBook -> Quest.
	// Tiklama bir pencerenin icindeyse: X'e denk geldiyse kapat, yoksa pencerenin kendi tiklamasini isle.
	if (bStatWindowOpen && StatRect.IsInside(Pos))
	{
		if (StatCloseRect.IsInside(Pos)) { bStatWindowOpen = false; } else { HandleStatClick(Pos); }
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
	if (bShopOpen && ShopRect.IsInside(Pos)) { return true; }
	if (bInventoryOpen && InventoryRect.IsInside(Pos)) { HandleInventoryRightClick(Pos); return true; }
	if (bSkillBookOpen && SkillBookRect.IsInside(Pos)) { HandleSkillBookRightClick(Pos); return true; }
	if (bQuestWindowOpen && QuestRect.IsInside(Pos)) { return true; }
	return false;
}
