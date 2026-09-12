// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOSkillComponent.h"
#include "Skills/MMOSkillDatabase.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Core/MMOTargeting.h"
#include "Player/MMOPlayerController.h"
#include "Components/MMOStatsComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Save/MMOSaveGame.h"

UMMOSkillComponent::UMMOSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMMOSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMMOSkillComponent, CharacterClass);
	DOREPLIFETIME(UMMOSkillComponent, LearnedSkills);
	DOREPLIFETIME(UMMOSkillComponent, Hotbar);
}

void UMMOSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	// Sinif kurulumu sunucuda possession aninda yapilir (AMMOCharacter::PossessedBy).
	// Burada sadece UI guvenligi: hotbar dizisi her makinede 10 slot olsun.
	if (Hotbar.Num() != HotbarSize)
	{
		Hotbar.Init(NAME_None, HotbarSize);
	}
}

bool UMMOSkillComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->HasAuthority() : false;
}

bool UMMOSkillComponent::IsLocallyControlledOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? OwnerPawn->IsLocallyControlled() : false;
}

void UMMOSkillComponent::RequestSetupForClass(EMMOClass NewClass)
{
	if (HasAuthorityInternal())
	{
		SetupForClass(NewClass);
	}
	else
	{
		ServerSetupForClass(NewClass);
	}
}

void UMMOSkillComponent::ServerSetupForClass_Implementation(EMMOClass NewClass)
{
	SetupForClass(NewClass);
}

void UMMOSkillComponent::MulticastSkillFX_Implementation(EMMOSkillEffect Effect, FVector Start, FVector End, float Radius, int32 AnimIndex)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Skill animasyonunu her makinede oynat (KO'daki gibi: skill kullanimi bir hareket tetikler).
	if (AMMOCharacter* OwnerChar = Cast<AMMOCharacter>(GetOwner()))
	{
		OwnerChar->PlayAttackAnimLocal(AnimIndex);
	}

	switch (Effect)
	{
	case EMMOSkillEffect::HealSelf:
		// Yesil sifa halkasi.
		DrawDebugSphere(World, Start, Radius > 0.f ? Radius : 90.f, 12, FColor::Green, false, 0.4f, 0, 2.f);
		break;

	case EMMOSkillEffect::MeleeDamage:
		// Hedefte kisa kirmizi darbe patlamasi.
		DrawDebugSphere(World, End, 40.f, 8, FColor(255, 80, 60), false, 0.25f, 0, 3.f);
		break;

	case EMMOSkillEffect::RangedDamage:
		// Mermi izi + hedefte patlama.
		DrawDebugLine(World, Start, End, FColor(120, 180, 255), false, 0.3f, 0, 3.f);
		DrawDebugSphere(World, End, 45.f, 8, FColor(150, 200, 255), false, 0.3f, 0, 3.f);
		break;

	case EMMOSkillEffect::AoESelf:
	case EMMOSkillEffect::AoETarget:
		// Alan halkasi.
		DrawDebugSphere(World, Start, Radius, 16, FColor(255, 120, 40), false, 0.5f, 0, 3.f);
		break;
	}
}

void UMMOSkillComponent::SetupForClass(EMMOClass NewClass)
{
	CharacterClass = NewClass;

	LearnedSkills.Reset();
	Hotbar.Init(NAME_None, HotbarSize);
	CooldownEndTimes.Reset();

	AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());

	// Sinifa gore ana stat dagilimi (sadece oyuncu, otorite).
	if (HasAuthorityInternal() && Owner && Owner->IsPlayerControlled())
	{
		if (UMMOStatsComponent* S = Owner->GetStats())
		{
			switch (NewClass)
			{
			case EMMOClass::Warrior: S->InitializePrimaryStats(18, 10, 6, 16); break;
			case EMMOClass::Archer:  S->InitializePrimaryStats(10, 20, 8, 12); break;
			case EMMOClass::Mage:    S->InitializePrimaryStats(6, 8, 22, 10);  break;
			case EMMOClass::Priest:  S->InitializePrimaryStats(8, 8, 18, 14);  break;
			}
		}
	}

	// Baslangic skili: sinifin en dusuk seviyeli Basic skilini bedava ver ve slot 0'a koy.
	const FMMOSkillDef* Starter = nullptr;
	for (const FMMOSkillDef& D : FMMOSkillDatabase::GetAll())
	{
		if (D.RequiredClass == NewClass && D.Tree == EMMOSkillTree::Basic)
		{
			if (!Starter || D.RequiredLevel < Starter->RequiredLevel)
			{
				Starter = &D;
			}
		}
	}
	if (Starter)
	{
		LearnedSkills.Add(Starter->SkillId);
		Hotbar[0] = Starter->SkillId;
	}
}

bool UMMOSkillComponent::IsLearned(FName SkillId) const
{
	return LearnedSkills.Contains(SkillId);
}

bool UMMOSkillComponent::CanLearn(FName SkillId, FString& OutReason) const
{
	const FMMOSkillDef* Def = FMMOSkillDatabase::Find(SkillId);
	if (!Def)
	{
		OutReason = TEXT("Skill yok");
		return false;
	}
	if (Def->RequiredClass != CharacterClass)
	{
		OutReason = TEXT("Sinif uygun degil");
		return false;
	}
	if (IsLearned(SkillId))
	{
		OutReason = TEXT("Zaten ogrenildi");
		return false;
	}

	const AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	const UMMOStatsComponent* S = Owner ? Owner->GetStats() : nullptr;
	if (!S)
	{
		OutReason = TEXT("Stat yok");
		return false;
	}
	if (S->Level < Def->RequiredLevel)
	{
		OutReason = FString::Printf(TEXT("Level %d gerekli"), Def->RequiredLevel);
		return false;
	}
	if (!Def->PrereqSkillId.IsNone() && !IsLearned(Def->PrereqSkillId))
	{
		const FMMOSkillDef* Pre = FMMOSkillDatabase::Find(Def->PrereqSkillId);
		OutReason = FString::Printf(TEXT("Once '%s' gerekli"), Pre ? *Pre->Name : TEXT("?"));
		return false;
	}
	if (S->UnspentSkillPoints < Def->SkillPointCost)
	{
		OutReason = FString::Printf(TEXT("%d skill puani gerekli"), Def->SkillPointCost);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool UMMOSkillComponent::LearnSkill(FName SkillId)
{
	FString Reason;
	if (!CanLearn(SkillId, Reason))
	{
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, FString::Printf(TEXT("Ogrenilemedi: %s"), *Reason));
		}
		return false;
	}

	// Istemci: sunucuya istek gonder (LearnedSkills/puan sunucuda degisir, geri replike olur).
	if (!HasAuthorityInternal())
	{
		ServerLearnSkill(SkillId);
		return true;
	}

	const FMMOSkillDef* Def = FMMOSkillDatabase::Find(SkillId);
	AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	if (!Def || !Owner || !Owner->GetStats())
	{
		return false;
	}

	if (!Owner->GetStats()->SpendSkillPoints(Def->SkillPointCost))
	{
		return false;
	}

	LearnedSkills.Add(SkillId);

	if (GEngine && IsLocallyControlledOwner())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(120, 220, 120), FString::Printf(TEXT("Ogrenildi: %s"), *Def->Name));
	}
	return true;
}

void UMMOSkillComponent::ServerLearnSkill_Implementation(FName SkillId)
{
	LearnSkill(SkillId);
}

void UMMOSkillComponent::AssignToSlot(int32 Slot, FName SkillId)
{
	if (!Hotbar.IsValidIndex(Slot) || !IsLearned(SkillId))
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerAssignToSlot(Slot, SkillId);
		return;
	}

	Hotbar[Slot] = SkillId;
}

void UMMOSkillComponent::ServerAssignToSlot_Implementation(int32 Slot, FName SkillId)
{
	AssignToSlot(Slot, SkillId);
}

void UMMOSkillComponent::ClearSlot(int32 Slot)
{
	if (!Hotbar.IsValidIndex(Slot))
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerClearSlot(Slot);
		return;
	}

	Hotbar[Slot] = NAME_None;
}

void UMMOSkillComponent::ServerClearSlot_Implementation(int32 Slot)
{
	ClearSlot(Slot);
}

FName UMMOSkillComponent::GetSlotSkill(int32 Slot) const
{
	return Hotbar.IsValidIndex(Slot) ? Hotbar[Slot] : NAME_None;
}

float UMMOSkillComponent::GetCooldownRemaining(FName SkillId) const
{
	if (!GetWorld())
	{
		return 0.f;
	}
	const float* End = CooldownEndTimes.Find(SkillId);
	return End ? FMath::Max(0.f, *End - GetWorld()->GetTimeSeconds()) : 0.f;
}

bool UMMOSkillComponent::UseHotbarSlot(int32 Slot)
{
	const FName SkillId = GetSlotSkill(Slot);
	if (SkillId.IsNone())
	{
		return false;
	}
	return UseSkill(SkillId);
}

bool UMMOSkillComponent::UseSkill(FName SkillId)
{
	const FMMOSkillDef* Def = FMMOSkillDatabase::Find(SkillId);
	AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Def || !Owner || !World || !Owner->GetStats() || Owner->GetStats()->bIsDead || !IsLearned(SkillId))
	{
		return false;
	}

	const bool bLocalFeedback = IsLocallyControlledOwner();
	const float Now = World->GetTimeSeconds();

	// Cooldown (her makine kendi kopyasini tutar; istemcide RPC spamini da keser).
	const float* End = CooldownEndTimes.Find(SkillId);
	if (End && Now < *End)
	{
		if (GEngine && bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver,
				FString::Printf(TEXT("%s bekliyor (%.1fs)"), *Def->Name, *End - Now));
		}
		return false;
	}

	// Mana on kontrolu (replike degerle; kesin karar sunucuda).
	if (Owner->GetStats()->Mana < Def->ManaCost)
	{
		if (GEngine && bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor(80, 120, 255), TEXT("Yeterli mana yok!"));
		}
		return false;
	}

	// Istemci: cooldown'u iyimser baslat, istegi sunucuya gonder.
	if (!HasAuthorityInternal())
	{
		CooldownEndTimes.Add(SkillId, Now + Def->Cooldown);
		ServerUseSkill(SkillId);
		if (GEngine && bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor(120, 220, 120), FString::Printf(TEXT(">> %s"), *Def->Name));
		}
		return true;
	}

	// Hedef gerektiren skiller (Melee/Ranged/AoETarget) icin ONCE hedef var mi kontrol et —
	// yoksa ExecuteSkill zaten "Menzilde hedef yok" deyip hicbir sey yapmiyordu ama mana
	// harcanip cooldown baslatiliyordu (bosa harcanan kaynak bug'i). Kendine-etki skiller
	// (HealSelf/BuffSelf/AoESelf) hedef gerektirmez, her zaman devam eder.
	const bool bNeedsTarget = (Def->Effect == EMMOSkillEffect::MeleeDamage
		|| Def->Effect == EMMOSkillEffect::RangedDamage
		|| Def->Effect == EMMOSkillEffect::AoETarget);
	if (bNeedsTarget && !FindNearestEnemy(Def->Range))
	{
		if (GEngine && bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Menzilde hedef yok"));
		}
		return false;
	}

	// Sunucu: kesin mana harcamasi + etki.
	if (!Owner->GetStats()->ConsumeMana(Def->ManaCost))
	{
		return false;
	}

	ExecuteSkill(Owner, *Def);
	CooldownEndTimes.Add(SkillId, Now + Def->Cooldown);

	if (GEngine && bLocalFeedback)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor(120, 220, 120), FString::Printf(TEXT(">> %s"), *Def->Name));
	}
	return true;
}

void UMMOSkillComponent::ServerUseSkill_Implementation(FName SkillId)
{
	// Sunucu-OTORITER cooldown: istemcinin dogru cadence'te gonderdigine GUVENME.
	// (Eskiden kosulsuz Remove yapiliyordu => sunucuda cooldown yok, sadece mana
	// sinirliyordu; hilelı istemci skill spamlayabiliyordu.) Saat farki/jitter icin
	// kucuk grace (0.1s) birak; cooldown hala doluysa istegi sessizce reddet.
	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (const float* End = CooldownEndTimes.Find(SkillId))
	{
		if (Now < *End - 0.1f)
		{
			return; // cooldown dolmadi (otorite reddi)
		}
	}
	// UseSkill kendi kapisini yeniden uygular; cakismamasi icin kaydi temizle,
	// UseSkill mana harcayip etkiyi calistirir ve cooldown'u yeniden yazar.
	CooldownEndTimes.Remove(SkillId);
	UseSkill(SkillId);
}

void UMMOSkillComponent::FillSaveData(FMMOCharacterSaveData& Out) const
{
	Out.CharacterClass = CharacterClass;
	Out.LearnedSkills = LearnedSkills;
	Out.Hotbar = Hotbar;
}

void UMMOSkillComponent::ApplySaveData(const FMMOCharacterSaveData& In)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	CharacterClass = In.CharacterClass;
	LearnedSkills = In.LearnedSkills;
	Hotbar = In.Hotbar;
	if (Hotbar.Num() != HotbarSize)
	{
		Hotbar.SetNum(HotbarSize);
	}
	CooldownEndTimes.Reset();
}

AMMOCharacter* UMMOSkillComponent::FindNearestEnemy(float Range) const
{
	const AMMOCharacter* Self = Cast<AMMOCharacter>(GetOwner());
	if (!Self)
	{
		return nullptr;
	}

	// Oyuncunun Z/tikla ile kilitledigi hedef (SelectedTarget) varsa ve gecerliyse
	// (mob, canli, menzilde) ONA oncelik ver. Aksi halde skill'ler normal saldiridan
	// (AttackTarget, secili hedefi kullanir) FARKLI davranip yakindaki BASKA bir moba
	// gidebiliyordu — oyuncu "su moba" nisan alip skill basinca baskasina vurma bug'i.
	// Cast<AMMOMob> ile PvP-kapali invariant'i korunur: oyuncu baska bir OYUNCUYU
	// secmisse bu yol atlanir, asagidaki mob-only fallback'e duser.
	if (const AMMOPlayerController* MMOPc = Cast<AMMOPlayerController>(Self->GetController()))
	{
		if (AMMOMob* Selected = Cast<AMMOMob>(MMOPc->GetSelectedTarget()))
		{
			if (Selected->GetStats() && !Selected->GetStats()->bIsDead
				&& FVector::Dist(Self->GetActorLocation(), Selected->GetActorLocation()) <= Range)
			{
				return Selected;
			}
		}
	}

	// Sadece moblari hedefle (PvP simdilik kapali).
	return MMOTargeting::FindNearestMob(Self, Self->GetActorLocation(), Range);
}

void UMMOSkillComponent::ExecuteSkill(AMMOCharacter* Caster, const FMMOSkillDef& Def)
{
	UWorld* World = GetWorld();
	if (!World || !Caster)
	{
		return;
	}

	AController* Instigator = Caster->GetController();
	const FVector CasterLoc = Caster->GetActorLocation();

	// Skill animasyonu: tanimda belirtilmisse onu, degilse efekt tipinden turet
	// (yakin dovus -> vurus varyasyonu, digerleri -> buyu okuma/buyuk vurus).
	const int32 AnimIndex = (Def.AnimIndex >= 0)
		? Def.AnimIndex
		: ((Def.Effect == EMMOSkillEffect::MeleeDamage) ? FMath::RandRange(0, 2) : AMMOCharacter::ChargedAnimIndex);

	switch (Def.Effect)
	{
	case EMMOSkillEffect::HealSelf:
	{
		if (Caster->GetStats())
		{
			Caster->GetStats()->Heal(Def.Power);
			// Iyilestirme skili bir buff da tasiyabilir (or. yenilenme HoT).
			if (!Def.AppliedEffectId.IsNone())
			{
				Caster->GetStats()->ApplyEffect(Def.AppliedEffectId, Caster);
			}
		}
		MulticastSkillFX(Def.Effect, CasterLoc, CasterLoc, 90.f, AnimIndex);
		break;
	}

	case EMMOSkillEffect::HealTarget:
	{
		// Secili hedef canli bir MUTTEFIK (baska oyuncu, mob degil) ise onu iyilestir;
		// aksi halde HealSelf ile ayni sekilde kendini iyilestir (self-fallback).
		AMMOCharacter* Ally = nullptr;
		if (const AMMOPlayerController* PC = Cast<AMMOPlayerController>(Caster->GetController()))
		{
			if (AMMOCharacter* Sel = PC->GetSelectedTarget())
			{
				if (Sel != Caster && !Cast<AMMOMob>(Sel) && Sel->GetStats())
				{
					Ally = Sel;
				}
			}
		}

		AMMOCharacter* Recipient = Ally ? Ally : Caster;
		if (Recipient->GetStats())
		{
			Recipient->GetStats()->Heal(Def.Power);
			if (!Def.AppliedEffectId.IsNone())
			{
				Recipient->GetStats()->ApplyEffect(Def.AppliedEffectId, Caster);
			}
		}
		MulticastSkillFX(EMMOSkillEffect::HealSelf, Recipient->GetActorLocation(), Recipient->GetActorLocation(), 90.f, AnimIndex);
		break;
	}

	case EMMOSkillEffect::BuffSelf:
	{
		// Sadece buff uygula (hasar yok). FX icin sifa halkasini kullan.
		if (Caster->GetStats() && !Def.AppliedEffectId.IsNone())
		{
			Caster->GetStats()->ApplyEffect(Def.AppliedEffectId, Caster);
		}
		MulticastSkillFX(EMMOSkillEffect::HealSelf, CasterLoc, CasterLoc, 90.f, AnimIndex);
		break;
	}

	case EMMOSkillEffect::MeleeDamage:
	case EMMOSkillEffect::RangedDamage:
	{
		if (AMMOCharacter* Target = FindNearestEnemy(Def.Range))
		{
			const FVector Dir = (Target->GetActorLocation() - CasterLoc).GetSafeNormal2D();
			if (!Dir.IsNearlyZero())
			{
				Caster->SetActorRotation(Dir.Rotation());
			}
			UGameplayStatics::ApplyDamage(Target, Def.Power, Instigator, Caster, nullptr);

			// Vurulan hedefe debuff uygula (or. zehirli ok -> Poison DoT).
			if (!Def.AppliedEffectId.IsNone() && Target->GetStats() && !Target->GetStats()->bIsDead)
			{
				Target->GetStats()->ApplyEffect(Def.AppliedEffectId, Caster);
			}

			MulticastSkillFX(Def.Effect, CasterLoc + FVector(0, 0, 40), Target->GetActorLocation() + FVector(0, 0, 40), 0.f, AnimIndex);
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Menzilde hedef yok"));
		}
		break;
	}

	case EMMOSkillEffect::AoESelf:
	case EMMOSkillEffect::AoETarget:
	{
		FVector Center = CasterLoc;
		if (Def.Effect == EMMOSkillEffect::AoETarget)
		{
			AMMOCharacter* Target = FindNearestEnemy(Def.Range);
			if (!Target)
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Menzilde hedef yok"));
				}
				return;
			}
			Center = Target->GetActorLocation();
		}

		MulticastSkillFX(Def.Effect, Center, Center, Def.Radius, AnimIndex);

		// Yaricap icindeki tum MOBLARA vur (PvP simdilik kapali).
		for (TActorIterator<AMMOMob> It(World); It; ++It)
		{
			AMMOMob* Other = *It;
			if (!Other || Other == Caster || !Other->GetStats() || Other->GetStats()->bIsDead)
			{
				continue;
			}
			if (FVector::Dist(Center, Other->GetActorLocation()) <= Def.Radius)
			{
				UGameplayStatics::ApplyDamage(Other, Def.Power, Instigator, Caster, nullptr);

				// Alan hasarina debuff da eslik edebilir (or. buz patlamasi -> Slow).
				if (!Def.AppliedEffectId.IsNone() && Other->GetStats() && !Other->GetStats()->bIsDead)
				{
					Other->GetStats()->ApplyEffect(Def.AppliedEffectId, Caster);
				}
			}
		}
		break;
	}
	}
}
