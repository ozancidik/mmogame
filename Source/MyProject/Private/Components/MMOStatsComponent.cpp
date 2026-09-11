// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOStatsComponent.h"
#include "Effects/MMOEffectDatabase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UI/MMOHUD.h"
#include "Save/MMOSaveGame.h"

UMMOStatsComponent::UMMOStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UMMOStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Durum efektleri + yenilenme sadece sunucuda islenir.
	if (!HasAuthorityInternal() || bIsDead)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Aktif buff/debuff'lari isle (moblar dahil herkes icin — DoT moba da vurabilmeli).
	ProcessEffects(DeltaTime);

	// DoT karakteri oldurmus olabilir; oyleyse yenilenme yapma.
	if (bIsDead)
	{
		return;
	}

	// Yenilenme yalnizca oyuncu icin (moblar tam canla spawn olur, savasta yenilenmez).
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsPlayerControlled())
	{
		return;
	}

	const bool bInCombat = (World->GetTimeSeconds() - LastDamageTime) < CombatDuration;
	const float Mult = bInCombat ? CombatRegenMult : 1.f;

	if (Health < MaxHealth)
	{
		Health = FMath::Min(MaxHealth, Health + HealthRegenPerSec * Mult * DeltaTime);
		OnHealthChanged.Broadcast(Health, MaxHealth);
	}
	if (Mana < MaxMana)
	{
		Mana = FMath::Min(MaxMana, Mana + ManaRegenPerSec * Mult * DeltaTime);
		OnManaChanged.Broadcast(Mana, MaxMana);
	}
}

void UMMOStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMMOStatsComponent, Strength);
	DOREPLIFETIME(UMMOStatsComponent, Dexterity);
	DOREPLIFETIME(UMMOStatsComponent, Intelligence);
	DOREPLIFETIME(UMMOStatsComponent, Vitality);
	DOREPLIFETIME(UMMOStatsComponent, UnspentStatPoints);
	DOREPLIFETIME(UMMOStatsComponent, UnspentSkillPoints);
	DOREPLIFETIME(UMMOStatsComponent, Level);
	DOREPLIFETIME(UMMOStatsComponent, Experience);
	DOREPLIFETIME(UMMOStatsComponent, Health);
	DOREPLIFETIME(UMMOStatsComponent, MaxHealth);
	DOREPLIFETIME(UMMOStatsComponent, Mana);
	DOREPLIFETIME(UMMOStatsComponent, MaxMana);
	DOREPLIFETIME(UMMOStatsComponent, AttackPower);
	DOREPLIFETIME(UMMOStatsComponent, Defense);
	DOREPLIFETIME(UMMOStatsComponent, bIsDead);
	DOREPLIFETIME(UMMOStatsComponent, ActiveEffects);
}

void UMMOStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	// Eşik Level'dan deterministik türetilir; hem sunucu hem istemci hesaplayabilir.
	ExperienceToNextLevel = ComputeExperienceForLevel(Level);
	LastKnownHealth = Health;

	// Sahibin varsayilan yurume hizini cache'le (hiz buff'lari bunun ustune oranlanir).
	if (const ACharacter* Ch = Cast<ACharacter>(GetOwner()))
	{
		if (const UCharacterMovementComponent* Move = Ch->GetCharacterMovement())
		{
			BaseWalkSpeed = Move->MaxWalkSpeed;
		}
	}

	if (HasAuthorityInternal())
	{
		// Spawn anında statları ana statlardan hesapla ve can/manayı doldur.
		RecalculateDerivedStats(/*bRefill=*/true);
	}
}

void UMMOStatsComponent::RefreshBaseWalkSpeed()
{
	if (const ACharacter* Ch = Cast<ACharacter>(GetOwner()))
	{
		if (const UCharacterMovementComponent* Move = Ch->GetCharacterMovement())
		{
			BaseWalkSpeed = Move->MaxWalkSpeed;
		}
	}
}

bool UMMOStatsComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->HasAuthority() : false;
}

int32 UMMOStatsComponent::ComputeExperienceForLevel(int32 InLevel) const
{
	// Basit üstel eğri: level yükseldikçe gereken exp artar.
	// L1->L2 = 100, sonrasında her seviyede ~%25 artış.
	const float Base = 100.f;
	const float Growth = 1.25f;
	return FMath::RoundToInt(Base * FMath::Pow(Growth, (float)FMath::Max(0, InLevel - 1)));
}

void UMMOStatsComponent::RecalculateDerivedStats(bool bRefill)
{
	const float OldMaxHealth = MaxHealth;
	const float OldMaxMana = MaxMana;

	MaxHealth = 50.f + (Vitality * HealthPerVitality) + (Level * HealthPerLevel);
	MaxMana = 25.f + (Intelligence * ManaPerIntelligence);
	AttackPower = Strength * AttackPerStrength + Dexterity * 0.5f + EquipmentAttackBonus
		+ SumEffectMagnitude(EMMOEffectType::AttackBuff);
	Defense = Vitality * DefensePerVitality + EquipmentDefenseBonus
		+ SumEffectMagnitude(EMMOEffectType::DefenseBuff);
	// Negatif buff'lar statlari 0'in altina indirmesin.
	AttackPower = FMath::Max(0.f, AttackPower);
	Defense = FMath::Max(0.f, Defense);

	if (bRefill)
	{
		Health = MaxHealth;
		Mana = MaxMana;
	}
	else
	{
		// Maks değiştiyse anlık değerleri orantılı koru, sonra clamp'le.
		if (OldMaxHealth > 0.f)
		{
			Health = FMath::Clamp(Health + (MaxHealth - OldMaxHealth), 0.f, MaxHealth);
		}
		if (OldMaxMana > 0.f)
		{
			Mana = FMath::Clamp(Mana + (MaxMana - OldMaxMana), 0.f, MaxMana);
		}
	}

	// Sunucuda değişiklikleri hemen dinleyicilere bildir (istemcide OnRep yapar).
	OnHealthChanged.Broadcast(Health, MaxHealth);
	OnManaChanged.Broadcast(Mana, MaxMana);
}

float UMMOStatsComponent::ApplyDamage(float RawDamage, AActor* /*DamageInstigator*/)
{
	if (!HasAuthorityInternal() || bIsDead || RawDamage <= 0.f)
	{
		return 0.f;
	}

	if (const UWorld* World = GetWorld())
	{
		LastDamageTime = World->GetTimeSeconds();
	}

	// Defense hasarı azaltır ama en az 1 hasar geçer.
	const float Mitigated = FMath::Max(1.f, RawDamage - Defense);
	const float ActualDamage = FMath::Min(Mitigated, Health);

	Health = FMath::Clamp(Health - ActualDamage, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);

	if (Health <= 0.f)
	{
		bIsDead = true;
		OnDeath.Broadcast();
		// Olunce tum buff/debuff'lar kalkar.
		ClearAllEffects();
	}

	return ActualDamage;
}

void UMMOStatsComponent::Heal(float Amount)
{
	if (!HasAuthorityInternal() || bIsDead || Amount <= 0.f)
	{
		return;
	}

	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UMMOStatsComponent::RestoreMana(float Amount)
{
	if (!HasAuthorityInternal() || bIsDead || Amount <= 0.f)
	{
		return;
	}

	Mana = FMath::Clamp(Mana + Amount, 0.f, MaxMana);
	OnManaChanged.Broadcast(Mana, MaxMana);
}

bool UMMOStatsComponent::ConsumeMana(float Amount)
{
	if (!HasAuthorityInternal() || Amount <= 0.f)
	{
		return true;
	}

	if (Mana < Amount)
	{
		return false;
	}

	Mana = FMath::Clamp(Mana - Amount, 0.f, MaxMana);
	OnManaChanged.Broadcast(Mana, MaxMana);
	return true;
}

void UMMOStatsComponent::GainExperience(int32 Amount)
{
	if (!HasAuthorityInternal() || bIsDead || Amount <= 0 || Level >= MaxLevel)
	{
		return;
	}

	Experience += Amount;

	// Tek seferde birden fazla level atlanabilir.
	while (Level < MaxLevel && Experience >= ExperienceToNextLevel)
	{
		Experience -= ExperienceToNextLevel;
		HandleLevelUp();
	}

	if (Level >= MaxLevel)
	{
		Experience = 0;
	}

	OnExperienceChanged.Broadcast(Experience, ExperienceToNextLevel);
}

int32 UMMOStatsComponent::ApplyDeathPenalty()
{
	if (!HasAuthorityInternal())
	{
		return 0;
	}

	const int32 Loss = FMath::Min(Experience, FMath::RoundToInt(ExperienceToNextLevel * DeathXPPenaltyPct));
	if (Loss > 0)
	{
		Experience = FMath::Max(0, Experience - Loss);
		OnExperienceChanged.Broadcast(Experience, ExperienceToNextLevel);
	}
	return Loss;
}

void UMMOStatsComponent::HandleLevelUp()
{
	Level++;
	ExperienceToNextLevel = ComputeExperienceForLevel(Level);
	UnspentStatPoints += StatPointsPerLevel;
	UnspentSkillPoints += SkillPointsPerLevel;

	// Level başına küçük otomatik stat artışı + can/mana yenile.
	RecalculateDerivedStats(/*bRefill=*/true);

	OnLevelUp.Broadcast(Level);
}

bool UMMOStatsComponent::SpendStatPoint(FName StatName, int32 Amount)
{
	if (!HasAuthorityInternal() || Amount <= 0 || UnspentStatPoints < Amount)
	{
		return false;
	}

	if (StatName == FName("Strength"))
	{
		Strength += Amount;
	}
	else if (StatName == FName("Dexterity"))
	{
		Dexterity += Amount;
	}
	else if (StatName == FName("Intelligence"))
	{
		Intelligence += Amount;
	}
	else if (StatName == FName("Vitality"))
	{
		Vitality += Amount;
	}
	else
	{
		return false;
	}

	UnspentStatPoints -= Amount;
	RecalculateDerivedStats(/*bRefill=*/false);
	return true;
}

void UMMOStatsComponent::Revive()
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	bIsDead = false;
	ActiveEffects.Reset();
	ApplyMovementSpeedFromEffects();
	RecalculateDerivedStats(/*bRefill=*/true);
	OnRevived.Broadcast();
	OnEffectsChanged.Broadcast();
}

void UMMOStatsComponent::InitializePrimaryStats(int32 NewStrength, int32 NewDexterity, int32 NewIntelligence, int32 NewVitality)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	Strength = NewStrength;
	Dexterity = NewDexterity;
	Intelligence = NewIntelligence;
	Vitality = NewVitality;

	RecalculateDerivedStats(/*bRefill=*/true);
}

bool UMMOStatsComponent::SpendSkillPoints(int32 Cost)
{
	if (!HasAuthorityInternal() || Cost <= 0 || UnspentSkillPoints < Cost)
	{
		return false;
	}

	UnspentSkillPoints -= Cost;
	return true;
}

void UMMOStatsComponent::SetEquipmentBonuses(float AttackBonus, float DefenseBonus)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	EquipmentAttackBonus = AttackBonus;
	EquipmentDefenseBonus = DefenseBonus;
	RecalculateDerivedStats(/*bRefill=*/false);
}

// ----------------------------------------------------------------------
// Durum efektleri (buff/debuff)
// ----------------------------------------------------------------------

float UMMOStatsComponent::SumEffectMagnitude(EMMOEffectType InType) const
{
	float Sum = 0.f;
	for (const FMMOActiveEffect& E : ActiveEffects)
	{
		if (E.Type == InType)
		{
			Sum += E.Magnitude;
		}
	}
	return Sum;
}

bool UMMOStatsComponent::HasEffect(FName EffectId) const
{
	for (const FMMOActiveEffect& E : ActiveEffects)
	{
		if (E.EffectId == EffectId)
		{
			return true;
		}
	}
	return false;
}

void UMMOStatsComponent::ApplyMovementSpeedFromEffects()
{
	ACharacter* Ch = Cast<ACharacter>(GetOwner());
	if (!Ch)
	{
		return;
	}
	UCharacterMovementComponent* Move = Ch->GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	// Ilk cagrida varsayilan hizi yakala.
	if (BaseWalkSpeed <= 0.f)
	{
		BaseWalkSpeed = Move->MaxWalkSpeed;
	}

	const float Pct = SumEffectMagnitude(EMMOEffectType::SpeedBuff); // negatif olabilir (yavaslatma)
	Move->MaxWalkSpeed = FMath::Max(50.f, BaseWalkSpeed * (1.f + Pct / 100.f));
}

void UMMOStatsComponent::ApplyEffect(FName EffectId, AActor* EffectInstigator)
{
	if (!HasAuthorityInternal() || bIsDead)
	{
		return;
	}

	const FMMOEffectDef* Def = FMMOEffectDatabase::Find(EffectId);
	if (!Def)
	{
		return;
	}

	// Ayni efekt zaten varsa: suresini tazele (KO tarzi yenileme; ust uste yigilmaz).
	for (FMMOActiveEffect& E : ActiveEffects)
	{
		if (E.EffectId == EffectId)
		{
			E.RemainingTime = Def->Duration;
			E.TimeSinceLastTick = 0.f;
			E.Instigator = EffectInstigator;
			OnEffectsChanged.Broadcast();
			return;
		}
	}

	FMMOActiveEffect A;
	A.EffectId = Def->EffectId;
	A.Type = Def->Type;
	A.Magnitude = Def->Magnitude;
	A.RemainingTime = Def->Duration;
	A.Duration = Def->Duration;
	A.bIsDebuff = Def->bIsDebuff;
	A.TickInterval = Def->TickInterval;
	A.TimeSinceLastTick = 0.f;
	A.Instigator = EffectInstigator;
	ActiveEffects.Add(A);

	// Stat/hiz buff'lari aninda turetilmis degerlere yansimali.
	if (Def->Type == EMMOEffectType::AttackBuff || Def->Type == EMMOEffectType::DefenseBuff)
	{
		RecalculateDerivedStats(/*bRefill=*/false);
	}
	else if (Def->Type == EMMOEffectType::SpeedBuff)
	{
		ApplyMovementSpeedFromEffects();
	}

	OnEffectsChanged.Broadcast();
}

void UMMOStatsComponent::RemoveEffect(FName EffectId)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	bool bBuffChanged = false;
	bool bRemoved = false;
	for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
	{
		if (ActiveEffects[i].EffectId == EffectId)
		{
			const EMMOEffectType T = ActiveEffects[i].Type;
			bBuffChanged |= (T == EMMOEffectType::AttackBuff || T == EMMOEffectType::DefenseBuff || T == EMMOEffectType::SpeedBuff);
			ActiveEffects.RemoveAt(i);
			bRemoved = true;
		}
	}

	if (bBuffChanged)
	{
		RecalculateDerivedStats(/*bRefill=*/false);
		ApplyMovementSpeedFromEffects();
	}
	if (bRemoved)
	{
		OnEffectsChanged.Broadcast();
	}
}

void UMMOStatsComponent::ClearAllEffects()
{
	if (!HasAuthorityInternal() || ActiveEffects.Num() == 0)
	{
		return;
	}

	ActiveEffects.Reset();
	RecalculateDerivedStats(/*bRefill=*/false);
	ApplyMovementSpeedFromEffects();
	OnEffectsChanged.Broadcast();
}

void UMMOStatsComponent::ProcessEffects(float DeltaTime)
{
	if (ActiveEffects.Num() == 0)
	{
		return;
	}

	// Periyodik uygulamalari once topla (diziyi degistirirken ApplyDamage'in reentrancy'sinden kacinmak icin).
	struct FPendingTick
	{
		EMMOEffectType Type;
		float Magnitude;
		AActor* Instigator;
	};
	TArray<FPendingTick> Pending;

	// 1) Sureleri ilerlet + periyodik tick'leri say.
	for (FMMOActiveEffect& E : ActiveEffects)
	{
		E.RemainingTime -= DeltaTime;

		if (E.TickInterval > 0.f && (E.Type == EMMOEffectType::PeriodicDamage || E.Type == EMMOEffectType::PeriodicHeal))
		{
			E.TimeSinceLastTick += DeltaTime;
			while (E.TimeSinceLastTick >= E.TickInterval)
			{
				E.TimeSinceLastTick -= E.TickInterval;
				Pending.Add({ E.Type, E.Magnitude, E.Instigator.Get() });
			}
		}
	}

	// 2) Suresi biten efektleri kaldir (ters yonde).
	bool bBuffChanged = false;
	bool bAnyRemoved = false;
	for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
	{
		if (ActiveEffects[i].RemainingTime <= 0.f)
		{
			const EMMOEffectType T = ActiveEffects[i].Type;
			bBuffChanged |= (T == EMMOEffectType::AttackBuff || T == EMMOEffectType::DefenseBuff || T == EMMOEffectType::SpeedBuff);
			ActiveEffects.RemoveAt(i);
			bAnyRemoved = true;
		}
	}

	// 3) Periyodik hasar/sifayi uygula (dizi yapisal olarak sabit; olum ClearAllEffects'i tetiklese bile guvenli).
	for (const FPendingTick& P : Pending)
	{
		if (bIsDead)
		{
			break;
		}
		if (P.Type == EMMOEffectType::PeriodicHeal)
		{
			Heal(P.Magnitude);
		}
		else // PeriodicDamage (DoT) — oldurme XP kredisi icin sahibin TakeDamage akisindan gecir.
		{
			AController* InstController = nullptr;
			if (APawn* InstPawn = Cast<APawn>(P.Instigator))
			{
				InstController = InstPawn->GetController();
			}
			UGameplayStatics::ApplyDamage(GetOwner(), P.Magnitude, InstController, P.Instigator, nullptr);
		}
	}

	if (bBuffChanged)
	{
		RecalculateDerivedStats(/*bRefill=*/false);
		ApplyMovementSpeedFromEffects();
	}
	if (bAnyRemoved || Pending.Num() > 0)
	{
		OnEffectsChanged.Broadcast();
	}
}

void UMMOStatsComponent::FillSaveData(FMMOCharacterSaveData& Out) const
{
	Out.Level = Level;
	Out.Experience = Experience;
	Out.UnspentStatPoints = UnspentStatPoints;
	Out.UnspentSkillPoints = UnspentSkillPoints;
	Out.Strength = Strength;
	Out.Dexterity = Dexterity;
	Out.Intelligence = Intelligence;
	Out.Vitality = Vitality;
}

void UMMOStatsComponent::ApplySaveData(const FMMOCharacterSaveData& In)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	Level = In.Level;
	Experience = In.Experience;
	UnspentStatPoints = In.UnspentStatPoints;
	UnspentSkillPoints = In.UnspentSkillPoints;
	Strength = In.Strength;
	Dexterity = In.Dexterity;
	Intelligence = In.Intelligence;
	Vitality = In.Vitality;

	ExperienceToNextLevel = ComputeExperienceForLevel(Level);
	// Turetilmisleri (MaxHealth vb.) yeniden hesapla ve tam canla basla.
	RecalculateDerivedStats(/*bRefill=*/true);
	OnExperienceChanged.Broadcast(Experience, ExperienceToNextLevel);
}

// ----------------------------------------------------------------------
// OnRep — istemci tarafında değer replike olunca UI delegate'lerini tetikle
// ----------------------------------------------------------------------

void UMMOStatsComponent::OnRep_Health()
{
	// Istemci: can dususunu yerel HUD'a ucan hasar sayisi olarak bildir.
	// (OnRep sunucuda calismaz; sunucu/host sayilari TakeDamage'de ekler — cift sayim olmaz.)
	const float Delta = Health - LastKnownHealth;
	if (LastKnownHealth >= 0.f && Delta < 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (AMMOHUD* HUD = Cast<AMMOHUD>(PC->GetHUD()))
				{
					if (const AActor* Owner = GetOwner())
					{
						HUD->AddDamageNumber(Owner->GetActorLocation() + FVector(0.f, 0.f, 90.f), -Delta);
					}
				}
			}
		}
	}
	LastKnownHealth = Health;

	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UMMOStatsComponent::OnRep_IsDead()
{
	// Istemci: olum/dirilme durum degisimini yerel dinleyicilere bildir.
	if (bIsDead)
	{
		OnDeath.Broadcast();
	}
	else
	{
		OnRevived.Broadcast();
	}
}

void UMMOStatsComponent::OnRep_Mana()
{
	OnManaChanged.Broadcast(Mana, MaxMana);
}

void UMMOStatsComponent::OnRep_Experience()
{
	OnExperienceChanged.Broadcast(Experience, ExperienceToNextLevel);
}

void UMMOStatsComponent::OnRep_Level()
{
	// Eşiği istemcide de Level'a göre tazele (ExperienceToNextLevel replike edilmiyor).
	ExperienceToNextLevel = ComputeExperienceForLevel(Level);
	OnLevelUp.Broadcast(Level);
	OnExperienceChanged.Broadcast(Experience, ExperienceToNextLevel);
}

void UMMOStatsComponent::OnRep_PrimaryStats()
{
	// Ana statlar değişince UI'ın türetilmiş değerleri tazelemesi için tetikle.
	OnHealthChanged.Broadcast(Health, MaxHealth);
	OnManaChanged.Broadcast(Mana, MaxMana);
}

void UMMOStatsComponent::OnRep_Effects()
{
	// Istemci: aktif efekt listesi degisti. Hiz'i yerel olarak tazele (AttackPower/Defense zaten replike).
	ApplyMovementSpeedFromEffects();
	OnEffectsChanged.Broadcast();
}
