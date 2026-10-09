// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOStatsComponent.h"
#include "Core/MMOMetrics.h"
#include "Effects/MMOEffectDatabase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Save/MMOSaveGame.h"
#include "Components/MMOSkillComponent.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Player/MMOPlayerController.h"
#include "Effects/MMODotDamageType.h"

namespace
{
	// Oyuncu -> mob DoT tick'ine eklenen saldiri payi (tick = efekt tabani + AttackPower x bu). PLACEHOLDER; PIE'de
	// dogrulanmadi (DOT_SCALING_DESIGN.md, onerilen 0,08-0,12).
	constexpr float DotPlayerAttackScale = 0.10f;
}

#if !UE_BUILD_SHIPPING
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Core/MMOGameMode.h"
#include "Player/MMOPlayerController.h"
#include "Components/MMOPartyComponent.h"
#include "Components/MMOClanComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"

namespace
{
	bool RunMMOStatsSaveBudgetTest(UWorld* World, FString& OutError, int32& OutChecks)
	{
		OutChecks = 0;
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
		{
			OutError = TEXT("server game world required");
			return false;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOCharacter* Character = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1000000, 1000000, 5000), FRotator::ZeroRotator, Params);
		if (!Character) { OutError = TEXT("fixture spawn failed"); return false; }
		const bool bPassed = [&]()
		{
			UMMOStatsComponent* S = Character->GetStats();
			if (!S) { OutError = TEXT("fixture stats missing"); return false; }
			auto Check = [&](bool Condition, const TCHAR* Label)
			{
				if (!Condition) { OutError = Label; return false; }
				++OutChecks;
				return true;
			};
			auto Total = [&]() { return S->Strength + S->Dexterity + S->Intelligence + S->Vitality; };
			auto ValidData = [](EMMOClass Class)
			{
				FMMOCharacterSaveData Data;
				Data.CharacterClass = Class;
				Data.Level = 10;
				Data.Experience = 17;
				UMMOSkillComponent::GetClassBaseStats(Class, Data.Strength, Data.Dexterity, Data.Intelligence, Data.Vitality);
				Data.Strength += 7;
				Data.Dexterity += 3;
				Data.UnspentStatPoints = 17; // 9 * 3 = 27; 10 harcanmis, 17 kalan.
				Data.UnspentSkillPoints = 18;
				return Data;
			};
			for (EMMOClass Class : { EMMOClass::Warrior, EMMOClass::Archer, EMMOClass::Mage, EMMOClass::Priest })
			{
				const FMMOCharacterSaveData Legal = ValidData(Class);
				S->ApplySaveData(Legal);
				if (!Check(S->Level == 10 && S->Experience == 17 && S->UnspentStatPoints == 17 && S->UnspentSkillPoints == 18
					&& S->Strength == Legal.Strength && S->Dexterity == Legal.Dexterity
					&& S->Intelligence == Legal.Intelligence && S->Vitality == Legal.Vitality, TEXT("legal class stats changed"))) return false;
				const int32 Budget = Total() + S->UnspentStatPoints;
				FMMOCharacterSaveData Hostile = Legal;
				Hostile.Strength = Hostile.Dexterity = Hostile.Intelligence = Hostile.Vitality = MAX_int32;
				Hostile.UnspentStatPoints = Hostile.UnspentSkillPoints = Hostile.Experience = MAX_int32;
				S->ApplySaveData(Hostile);
				if (!Check(Total() == Budget && S->UnspentStatPoints == 0 && S->UnspentSkillPoints == 18
					&& S->Experience == S->ExperienceToNextLevel - 1 && S->Level == 10,
					TEXT("hostile class stats exceeded budget or granted levels"))) return false;
			}
			FMMOCharacterSaveData Bad = ValidData(EMMOClass::Warrior);
			Bad.Level = Bad.Experience = Bad.UnspentStatPoints = Bad.UnspentSkillPoints = MIN_int32;
			Bad.Strength = Bad.Dexterity = Bad.Intelligence = Bad.Vitality = MIN_int32;
			S->ApplySaveData(Bad);
			if (!Check(S->Level == 1 && S->Experience == 0 && Total() == 4 && S->UnspentStatPoints == 0 && S->UnspentSkillPoints == 0
				&& FMath::IsFinite(S->MaxHealth) && S->MaxHealth >= 1.f && FMath::IsFinite(S->MaxMana) && S->MaxMana >= 0.f,
				TEXT("negative save values survived"))) return false;
			Bad.Level = Bad.Experience = Bad.UnspentStatPoints = Bad.UnspentSkillPoints = MAX_int32;
			Bad.Strength = Bad.Dexterity = Bad.Intelligence = Bad.Vitality = MAX_int32;
			S->ApplySaveData(Bad);
			if (!Check(S->Level == S->MaxLevel && S->Experience == 0 && Total() == 50 + (S->MaxLevel - 1) * S->StatPointsPerLevel
				&& S->UnspentStatPoints == 0 && S->UnspentSkillPoints == (S->MaxLevel - 1) * S->SkillPointsPerLevel,
				TEXT("max-level save exceeded budget"))) return false;
			Bad.Level = 1;
			Bad.CharacterClass = static_cast<EMMOClass>(255);
			S->ApplySaveData(Bad);
			if (!Check(Total() == 50 && S->UnspentStatPoints == 0 && S->UnspentSkillPoints == 0,
				TEXT("invalid class bypassed warrior budget"))) return false;
			const FMMOCharacterSaveData Legal = ValidData(EMMOClass::Warrior);
			S->ApplySaveData(Legal);
			FMMOCharacterSaveData Roundtrip;
			S->FillSaveData(Roundtrip);
			S->ApplySaveData(Roundtrip);
			if (!Check(S->Level == Legal.Level && S->Experience == Legal.Experience && S->Strength == Legal.Strength
				&& S->Dexterity == Legal.Dexterity && S->Intelligence == Legal.Intelligence && S->Vitality == Legal.Vitality
				&& S->UnspentStatPoints == Legal.UnspentStatPoints && S->UnspentSkillPoints == Legal.UnspentSkillPoints,
				TEXT("legal stats roundtrip lost points"))) return false;
			S->Health = 25.f; S->Mana = 8.f;
			if (!Check(S->SpendStatPoint(TEXT("Vitality"), 1) && S->Vitality == Legal.Vitality + 1
				&& S->UnspentStatPoints == 16 && S->Health == 25.f && S->Mana == 8.f,
				TEXT("spending vitality refilled health or lost points"))) return false;
			const int32 BeforeTotal = Total();
			if (!Check(!S->SpendStatPoint(TEXT("Unknown"), 1) && !S->SpendStatPoint(TEXT("Strength"), 0)
				&& !S->SpendStatPoint(TEXT("Strength"), -1) && !S->SpendStatPoint(TEXT("Strength"), MAX_int32)
				&& Total() == BeforeTotal && S->UnspentStatPoints == 16 && S->Health == 25.f && S->Mana == 8.f,
				TEXT("invalid stat request changed state"))) return false;
			int32 Str, Dex, Int, Vit;
			UMMOSkillComponent::GetClassBaseStats(EMMOClass::Mage, Str, Dex, Int, Vit);
			S->ResetPointsForClassChange(Str, Dex, Int, Vit, 0);
			S->ResetPointsForClassChange(Str, Dex, Int, Vit, 0);
			return Check(S->UnspentStatPoints == 27 && S->UnspentSkillPoints == 18 && Total() == 46
				&& S->Health == 25.f && S->Mana == 8.f, TEXT("repeated class refund created points or healing"));
		}();
		Character->Destroy(); // Controllersiz: AutoLoad/AutoSave ve oyuncu diski yok.
		return bPassed;
	}

	struct FMMOStatsSaveBudgetTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestStatsSaveBudget"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = RunMMOStatsSaveBudgetTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_STATS_SAVE_BUDGET_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOStatsSaveBudgetTestExec GMMOStatsSaveBudgetTestExec;
}
#endif

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

	const bool bInCombat = (World->GetTimeSeconds() - LastCombatActivityTime) < CombatDuration;
	const float Mult = FMath::Max(0.f, bInCombat ? CombatRegenMult : 1.f);

	// Y3: yenilenme her karede birikir ama Health/Mana'ya yalniz RegenApplyInterval (0.5 sn) adimlarinda
	// ve tam puan olarak yazilir; boylece degerler saniyede en fazla 2 kez kirlenir/replike olur
	// (eskiden her karede). Toplam yenilenme miktari degismez (kesir birikimde kalir).
	// Hasar/iyilestirme/olum hesaplari bu yoldan bagimsiz, tam hassasiyetle ApplyDamage/Heal'de.
	if (Health < MaxHealth)
	{
		HealthRegenAccum += FMath::Max(0.f, HealthRegenPerSec) * Mult * DeltaTime;
	}
	else
	{
		HealthRegenAccum = 0.f;
	}
	if (Mana < MaxMana)
	{
		// Seviye ile buyuyen yenilenme (QA MEDIUM #5): sabit 7/sn ile Lv80 capstone (400 mana,
		// 26sn CD) savasta ~174sn'de bir atilabiliyordu.
		const float RegenPerSec = ManaRegenPerSec + Level * ManaRegenPerLevel;
		ManaRegenAccum += FMath::Max(0.f, RegenPerSec) * Mult * DeltaTime;
	}
	else
	{
		ManaRegenAccum = 0.f;
	}

	RegenApplyTimer += DeltaTime;
	if (RegenApplyTimer < RegenApplyInterval)
	{
		return;
	}
	RegenApplyTimer = 0.f;

	if (HealthRegenAccum >= 1.f && Health < MaxHealth)
	{
		const float Step = FMath::FloorToFloat(HealthRegenAccum);
		HealthRegenAccum -= Step;
		Health = FMath::Clamp(Health + Step, 0.f, MaxHealth);
		OnHealthChanged.Broadcast(Health, MaxHealth);
	}
	if (ManaRegenAccum >= 1.f && Mana < MaxMana)
	{
		const float Step = FMath::FloorToFloat(ManaRegenAccum);
		ManaRegenAccum -= Step;
		Mana = FMath::Clamp(Mana + Step, 0.f, MaxMana);
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
	// NETWORK_AUDIT O9: puanlar, EXP ve mana yalniz sahibine (HUD bunlari kendi pawn'i icin
	// cizer; parti listesi yalniz Level/sinif/can/olu bilgisini kullanir). Level, Health,
	// MaxHealth, AttackPower, Defense, bIsDead ve efektler herkese kalir.
	DOREPLIFETIME_CONDITION(UMMOStatsComponent, UnspentStatPoints, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOStatsComponent, UnspentSkillPoints, COND_OwnerOnly);
	DOREPLIFETIME(UMMOStatsComponent, Level);
	DOREPLIFETIME_CONDITION(UMMOStatsComponent, Experience, COND_OwnerOnly);
	DOREPLIFETIME(UMMOStatsComponent, Health);
	DOREPLIFETIME(UMMOStatsComponent, MaxHealth);
	DOREPLIFETIME_CONDITION(UMMOStatsComponent, Mana, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOStatsComponent, MaxMana, COND_OwnerOnly);
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
	// GUVENLIK/BUG-HUNT: bu egri MaxLevel=80'e kadar tasarlanmis araligin ICINDE bile
	// int32'yi tasiyordu (100*1.25^76 ~= 1.85 milyar, 1.25^77 ~= 2.32 milyar > INT32_MAX).
	// Tasan deger negatife donuyor -> GainExperience'daki "Experience >= ExperienceToNextLevel"
	// kontrolu hep dogru oluyor -> oyuncu Lv77'den itibaren araya sikisan seviyeleri atlayip
	// direkt Lv80'e sicriyordu, HUD XP bari bozuluyordu, ApplyDeathPenalty negatif kayip
	// hesaplayip olum cezasini sessizce iptal ediyordu. Hile gerekmiyor, MaxLevel'a normal
	// oynanisla ulasan HERKES tetikleyecekti. double'da hesaplayip (float'in ~7 basamaklik
	// hassasiyeti bu buyuklukte deger icin yetersiz) int32'ye guvenle sigacak sekilde tepeye
	// kirp (MaxLevel'daki gercek deger zaten ulasilamayacak kadar yuksek bir hedef; kirpma
	// oyun deneyimini degistirmez).
	//
	// QA bulgusu (2026-09-15, HIGH #2): int32 tasmasi kapandiktan SONRA bile bu egri
	// MaxLevel=80'e ulasmayi MATEMATIKSEL olarak imkansiz kiliyordu - mob XP'si (en
	// yuksegi Magma Tirani, 6800 XP, Lv24) sabit kalirken gereken XP ustel buyudugu icin
	// Lv80'e ulasmak icin ~2.66 MILYON kill gerekiyordu (kumulatif Lv1-80 ~18 MILYAR XP).
	// Icerigin gercekten var oldugu Lv1-25 araligi (mevcut mob/gorev dengesiyle zaten
	// tuned/kabul edilmis) HIC DOKUNULMADAN korunuyor; Lv25'ten sonra (henuz mob/spawner
	// icerigi olmayan, "hedef tavan" araligi - bkz. MMOStatsComponent.h'deki MaxLevel
	// yorumu) egri ustelden yumusak bir parabole donusuyor (asagidaki QA MEDIUM #4 notuna
	// bak) - hala ciddi bir "endgame grind" ama artik SONLU ve ulasilabilir.
	const double Base = 100.0;
	const double Growth = 1.25;
	const int32 LinearBoundaryLevel = 25;
	// QA MEDIUM #4: eski duz +37000 artis Lv25->26'da tek adimda x2.75 duvar yaratiyordu.
	// Simdi Lv25 sonrasi egri, us egrinin egimiyle (~%22) baslayip kademeli hizlanan bir
	// parabol: n=Level-25 icin B + 4725*n + 591*n^2 (Lv26 ~26.5K, Lv50 ~509K, Lv79 ~2.0M;
	// Lv25->80 toplami ~40M XP ~ 5.900 Magma Tirani kill'i).
	const double LinearSlope = 4725.0;
	const double QuadCoeff = 591.0;

	double Raw;
	if (InLevel <= LinearBoundaryLevel)
	{
		Raw = Base * FMath::Pow(Growth, (double)FMath::Max(0, InLevel - 1));
	}
	else
	{
		const double BoundaryValue = Base * FMath::Pow(Growth, (double)(LinearBoundaryLevel - 1));
		const double N = (double)(InLevel - LinearBoundaryLevel);
		Raw = BoundaryValue + LinearSlope * N + QuadCoeff * N * N;
	}
	const double Clamped = FMath::Min(Raw, (double)MAX_int32);
	return FMath::RoundToInt(Clamped);
}

void UMMOStatsComponent::RecalculateDerivedStats(bool bRefill, bool bPreserveCurrentVitals)
{
	const float OldMaxHealth = MaxHealth;
	const float OldMaxMana = MaxMana;

	MaxHealth = 50.f + (Vitality * HealthPerVitality) + (Level * HealthPerLevel);
	MaxMana = 25.f + (Intelligence * ManaPerIntelligence) + (Level * ManaPerLevel);
	AttackPower = Strength * AttackPerStrength + Dexterity * 0.5f + EquipmentAttackBonus
		+ SumEffectMagnitude(EMMOEffectType::AttackBuff);
	Defense = Vitality * DefensePerVitality + EquipmentDefenseBonus
		+ SumEffectMagnitude(EMMOEffectType::DefenseBuff);
	// Negatif buff'lar statlari 0'in altina indirmesin.
	AttackPower = FMath::Max(0.f, AttackPower);
	Defense = FMath::Max(0.f, Defense);
	// Bozuk/eski-surum bir kayittan negatif Level/Vitality/Intelligence gelirse (ApplySaveData
	// hicbir dogrulama yapmadan kopyaliyor) MaxHealth/MaxMana negatife dusup bRefill ile
	// Health/Mana'yi da negatif baslatabilirdi (bug-hunt, savunma amacli sinirlama).
	MaxHealth = FMath::Max(1.f, MaxHealth);
	MaxMana = FMath::Max(0.f, MaxMana);

	if (bRefill)
	{
		Health = MaxHealth;
		Mana = MaxMana;
	}
	else if (bPreserveCurrentVitals)
	{
		Health = FMath::Clamp(Health, 0.f, MaxHealth);
		Mana = FMath::Clamp(Mana, 0.f, MaxMana);
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

float UMMOStatsComponent::ApplyDamage(float RawDamage, AActor* DamageInstigator)
{
	return ApplyDamageCommon(RawDamage, DamageInstigator, /*bIgnoreDefense=*/false);
}

float UMMOStatsComponent::ApplyDamageIgnoringDefense(float RawDamage, AActor* DamageInstigator)
{
	return ApplyDamageCommon(RawDamage, DamageInstigator, /*bIgnoreDefense=*/true);
}

float UMMOStatsComponent::ApplyDamageCommon(float RawDamage, AActor* /*DamageInstigator*/, bool bIgnoreDefense)
{
	if (!HasAuthorityInternal() || bIsDead || RawDamage <= 0.f)
	{
		return 0.f;
	}

	if (const UWorld* World = GetWorld())
	{
		LastCombatActivityTime = World->GetTimeSeconds();
	}

	// Defense hasarı azaltır ama en az 1 hasar geçer.
	const float Mitigated = bIgnoreDefense ? RawDamage : FMath::Max(1.f, RawDamage - Defense);
	const float ActualDamage = FMath::Min(Mitigated, Health);
	LastAppliedDamage = ActualDamage; // OnDeath'ten once: olumcul vurusun hasari HandleDeath'te okunur

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

	const float OldHealth = Health;
	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);

	// Ucan "+N" iyilesme sayisi. BILEREK sadece burada (Heal() cagrilarinda - potion/skill/NPC
	// sifaci): TickComponent'teki pasif yenilenme dogrudan Health'i mutasyona ugratir, BU
	// yolu KULLANMAZ - yoksa her saniye "+2" gibi surekli spam olurdu.
	const float ActualHeal = Health - OldHealth;
	if (ActualHeal > 0.f)
	{
		if (AMMOCharacter* OwnerCharacter = Cast<AMMOCharacter>(GetOwner()))
		{
			OwnerCharacter->MulticastCombatText(
				OwnerCharacter->GetActorLocation() + FVector(0.f, 0.f, 90.f), ActualHeal, true);
		}
	}
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

	if (bGainExperienceInProgress)
	{
		// Refill/OnLevelUp callbacks can award XP synchronously. The outer call
		// owns the progression loop; do not overwrite its balance or recurse.
		PendingExperienceBalance += FMath::Min<int64>(Amount, MAX_int64 - PendingExperienceBalance);
		return;
	}

	{
		TGuardValue<bool> GrantGuard(bGainExperienceInProgress, true);
		// Keep large awards and callback grants in 64 bits until level thresholds
		// have been consumed. Only the final bounded balance is replicated/saved.
		PendingExperienceBalance = static_cast<int64>(Experience) + Amount;
		while (Level < MaxLevel && PendingExperienceBalance >= ExperienceToNextLevel)
		{
			PendingExperienceBalance -= ExperienceToNextLevel;
			HandleLevelUp();
		}
		Experience = (Level >= MaxLevel) ? 0 : static_cast<int32>(PendingExperienceBalance);
		PendingExperienceBalance = 0;
	} // Release before the existing final OnExperienceChanged notification.

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
	const int32 OldLevel = Level;
	Level++;
	ExperienceToNextLevel = ComputeExperienceForLevel(Level);
	UnspentStatPoints += StatPointsPerLevel;
	UnspentSkillPoints += SkillPointsPerLevel;

#if !UE_BUILD_SHIPPING
	// Capture the committed adjacent transition before any refill/level delegate
	// can grant nested XP, detach the controller or destroy its pawn.
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()); IsValid(OwnerPawn) && OwnerPawn->IsPlayerControlled())
	{
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(OwnerPawn->GetController()); IsValid(PC) && PC->GetPawn() == OwnerPawn)
		{
			PC->RecordFirstLevelUpMetric(OldLevel);
		}
	}
#endif
	// Level başına küçük otomatik stat artışı + can/mana yenile.
	RecalculateDerivedStats(/*bRefill=*/true);
	OnLevelUp.Broadcast(Level);

	// Olcum gunlugu (yalniz oyuncular): seviye atlama.
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()); OwnerPawn && OwnerPawn->IsPlayerControlled())
	{
		UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.1f ev=levelup who=%s from=%d to=%d"),
			MMOMetrics::Seconds(), *MMOMetrics::WhoName(OwnerPawn), OldLevel, Level);
	}
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
	// Puani VIT/INT'e yatirmak maksimumu buyutur, mevcut can/mana eklemez.
	// Aksi halde sinif degisimiyle iade edilen puanlar yeniden harcanarak
	// bekleme suresinde sinirsiz anlik iyilesme/mana yenilemesi saglayabiliyordu.
	RecalculateDerivedStats(/*bRefill=*/false, /*bPreserveCurrentVitals=*/true);
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

void UMMOStatsComponent::ResetPointsForClassChange(int32 NewStrength, int32 NewDexterity, int32 NewIntelligence, int32 NewVitality, int32 /*SpentSkillPoints*/)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	// Only HandleLevelUp grants points. A complete reset restores exactly that budget,
	// including legacy skills, without trusting a corrupt balance or refund addition.
	// The fifth argument remains for existing C++ callers; it cannot enlarge the budget.
	UnspentStatPoints = UMMOSkillComponent::GetMasteryPointBudget(Level, StatPointsPerLevel);
	UnspentSkillPoints = UMMOSkillComponent::GetMasteryPointBudget(Level, SkillPointsPerLevel);

	Strength = NewStrength;
	Dexterity = NewDexterity;
	Intelligence = NewIntelligence;
	Vitality = NewVitality;

	// Doldurma YOK (QA MEDIUM-6: sinif degistirip can/mana fullemek exploit'ti). Delta'li
	// RecalculateDerivedStats(false) yerine mevcut degeri koruyup yeni maksimuma kirp:
	// delta yontemi VIT'i dusen sinifa geciste cani 0'a (olmeden) indirebiliyordu.
	const float OldHealth = Health;
	const float OldMana = Mana;
	RecalculateDerivedStats(/*bRefill=*/false);
	Health = FMath::Clamp(OldHealth, 1.f, MaxHealth);
	Mana = FMath::Clamp(OldMana, 0.f, MaxMana);
	OnHealthChanged.Broadcast(Health, MaxHealth);
	OnManaChanged.Broadcast(Mana, MaxMana);
}

bool UMMOStatsComponent::IsInCombat() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastCombatActivityTime) < CombatDuration;
}

void UMMOStatsComponent::MarkCombatActivity()
{
	if (HasAuthorityInternal())
	{
		if (const UWorld* World = GetWorld())
		{
			LastCombatActivityTime = World->GetTimeSeconds();
		}
	}
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
	const AMMOMob* SourceMob = Cast<AMMOMob>(EffectInstigator);
	const bool bPlayerSource = Cast<AMMOCharacter>(EffectInstigator) && !SourceMob;
	const float SourceHubRadius = (SourceMob && SourceMob->bHasHub) ? SourceMob->HubSafeRadius : 0.f;
	const FVector SourceHubLocation = SourceMob ? SourceMob->HubLocation : FVector::ZeroVector;

	// Bitis zamani (sunucu dunya saati): istemci kalan sureyi bundan hesaplar (replike alan yalniz uygulamada/tazelemede degisir).
	const UWorld* EffectWorld = GetWorld();
	const AGameStateBase* EffectGS = EffectWorld ? EffectWorld->GetGameState() : nullptr;
	const float EffectNow = EffectGS ? EffectGS->GetServerWorldTimeSeconds() : (EffectWorld ? EffectWorld->GetTimeSeconds() : 0.f);
	const float EffectEnd = EffectNow + Def->Duration;

	// Oyuncunun mob uzerindeki DoT'u buyuklugunu kaynagin saldirisina baglar (DOT_SCALING_DESIGN.md A): sabit 8/sn
	// Lv31+ hedefte anlamsizdi. Tick = taban + saldiri x DotPlayerAttackScale. Mob -> oyuncu DoT'u degismez (ayri karar).
	float EffectMagnitude = Def->Magnitude;
	if (Def->Type == EMMOEffectType::PeriodicDamage && bPlayerSource && Cast<AMMOMob>(GetOwner()))
	{
		if (const AMMOCharacter* SourcePlayer = Cast<AMMOCharacter>(EffectInstigator))
		{
			if (const UMMOStatsComponent* SourceStats = SourcePlayer->GetStats())
			{
				EffectMagnitude += SourceStats->AttackPower * DotPlayerAttackScale;
			}
		}
	}

	// Ayni efekt zaten varsa: suresini tazele (KO tarzi yenileme; ust uste yigilmaz).
	for (FMMOActiveEffect& E : ActiveEffects)
	{
		if (E.EffectId == EffectId)
		{
			// QA 2026-10-09 ORTA-3: buyukluk artik kaynagin saldirisina bagli; daha zayif bir OYUNCU, mobdaki daha guclu oyuncu
			// DoT'unu yeniledigi icin tick'i dusuremez ve kalan tick'lerin olduruden kredisini alamaz: yalniz sure tazelenir.
			// Match the target mob's damage hub guard: a retained source may have entered safety since applying DoT.
			// Replace both magnitude and credit with the incoming effect when that source's ticks are blocked.
			const AMMOMob* TargetMob = Cast<AMMOMob>(GetOwner());
			const APawn* RetainedSource = Cast<APawn>(E.Instigator.Get());
			const bool bRetainedSourceInHub = TargetMob && RetainedSource && RetainedSource->IsPlayerControlled()
				&& TargetMob->bHasHub && TargetMob->HubSafeRadius > 0.f
				&& FVector::Dist2D(RetainedSource->GetActorLocation(), TargetMob->HubLocation) < TargetMob->HubSafeRadius;
			if (Def->Type == EMMOEffectType::PeriodicDamage && bPlayerSource && E.bPlayerSource && E.Instigator.IsValid()
				&& !bRetainedSourceInHub && EffectMagnitude < E.Magnitude)
			{
				E.RemainingTime = Def->Duration;
				E.EndServerTime = EffectEnd;
				E.TimeSinceLastTick = 0.f;
				OnEffectsChanged.Broadcast();
				return;
			}
			E.Magnitude = EffectMagnitude;
			E.RemainingTime = Def->Duration;
			E.EndServerTime = EffectEnd;
			E.TimeSinceLastTick = 0.f;
			E.Instigator = EffectInstigator;
			E.bPlayerSource = bPlayerSource;
			E.MobSourceHubLocation = SourceHubLocation;
			E.MobSourceHubRadius = SourceHubRadius;
			OnEffectsChanged.Broadcast();
			return;
		}
	}

	FMMOActiveEffect A;
	A.EffectId = Def->EffectId;
	A.Type = Def->Type;
	A.Magnitude = EffectMagnitude;
	A.RemainingTime = Def->Duration;
	A.EndServerTime = EffectEnd;
	A.Duration = Def->Duration;
	A.bIsDebuff = Def->bIsDebuff;
	A.TickInterval = Def->TickInterval;
	A.TimeSinceLastTick = 0.f;
	A.Instigator = EffectInstigator;
	A.bPlayerSource = bPlayerSource;
	A.MobSourceHubLocation = SourceHubLocation;
	A.MobSourceHubRadius = SourceHubRadius;
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
		TWeakObjectPtr<AActor> Instigator;
		bool bPlayerSource;
		float MobSourceHubRadius;
		FVector MobSourceHubLocation;
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
				Pending.Add({ E.Type, E.Magnitude, E.Instigator, E.bPlayerSource, E.MobSourceHubRadius, E.MobSourceHubLocation });
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
			// Mob zehiri guvenli bolgeye giren oyuncuyu oldurmemeli. Oyuncunun moba
			// uyguladigi DoT bu kapidan etkilenmez.
			if (P.MobSourceHubRadius > 0.f)
			{
				if (const AMMOCharacter* Player = Cast<AMMOCharacter>(GetOwner()))
				{
					if (Player->IsPlayerControlled()
						&& FVector::Dist2D(Player->GetActorLocation(), P.MobSourceHubLocation) < P.MobSourceHubRadius)
					{
						continue;
					}
				}
			}
			// Resolve again for EACH pending tick: an earlier damage callback may destroy the source.
			AActor* Instigator = P.Instigator.Get();
			if (P.bPlayerSource && !Instigator)
			{
				continue; // Orphan player DoT cannot bypass PvP/party/clan/hub or kill without attribution.
			}
			AController* InstController = nullptr;
			if (APawn* InstPawn = Cast<APawn>(Instigator))
			{
				InstController = InstPawn->GetController();
			}
			// Oyuncunun mob uzerindeki DoT'u savunmayi atlar (TakeDamage UMMODotDamageType'i taniyip
			// ApplyDamageIgnoringDefense'e yonlendirir); diger tum DoT'lar eskisi gibi savunmadan dusulur.
			const bool bPlayerDotOnMob = P.bPlayerSource && Cast<AMMOMob>(GetOwner()) != nullptr;
			UGameplayStatics::ApplyDamage(GetOwner(), P.Magnitude, InstController, Instigator,
				bPlayerDotOnMob ? TSubclassOf<UDamageType>(UMMODotDamageType::StaticClass()) : TSubclassOf<UDamageType>());
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

	// GUVENLIK/BUG-HUNT (qa-kontrolcu bulgusu): once hicbir dogrulama yapmadan
	// kopyalaniyordu. Bozuk/elle-duzenlenmis bir kayitta Level MaxLevel'i asarsa
	// dogrudan ComputeExperienceForLevel'in tasma bolgesine duserdi (o fonksiyon artik
	// kendi icinde kirpiyor ama burada da makul araliga sikistirmak ikinci bir savunma
	// katmani), Experience de negatif olamaz.
	Level = FMath::Clamp(In.Level, 1, MaxLevel);
	Experience = FMath::Max(0, In.Experience);
	// Harcanmamis puanlar yalniz level-up'tan gelir; bozuk bir kaydin negatif
	// veya asiri buyuk bakiyesi class-change iadesini tasirmasin.
	const int32 LevelsGained = Level - 1;
	const int32 PointsFromLevels = LevelsGained * FMath::Max(0, StatPointsPerLevel);
	const int32 SkillPointBudget = UMMOSkillComponent::GetMasteryPointBudget(Level, SkillPointsPerLevel);
	// Use the source save and the same repair rule, independent of component load order.
	TArray<int32> SavedRanks;
	const int32 SpentSkillPoints = UMMOSkillComponent::SanitizeSavedMasteryRanks(SavedRanks, In, Level, SkillPointBudget);
	UnspentSkillPoints = FMath::Clamp(In.UnspentSkillPoints, 0, SkillPointBudget - SpentSkillPoints);
	// GUVENLIK (docs/SECURITY_AUDIT.md Y-2 + QA): ana statlar ve harcanmamis puan birlikte
	// dogrulanir: (harcanmis statlar + harcanmamis puan) <= sinif tabani + (Level-1)*StatPointsPerLevel.
	// Sinif tabanlari farkli (Savasci/Okcu 50, Rahip 48, Buyucu 46); SkillComponent::ApplySaveData
	// sinifi zaten yukledi ama kayittaki sinifi ayni kuralla (gecersiz -> Savasci) burada da okuyoruz.
	const EMMOClass SavedClass = static_cast<uint8>(In.CharacterClass) <= static_cast<uint8>(EMMOClass::Priest)
		? In.CharacterClass : EMMOClass::Warrior;
	int32 Base[4] = { 0, 0, 0, 0 };
	UMMOSkillComponent::GetClassBaseStats(SavedClass, Base[0], Base[1], Base[2], Base[3]);
	const int32 MaxStatTotal = Base[0] + Base[1] + Base[2] + Base[3] + PointsFromLevels;
	int32 Loaded[4] = { In.Strength, In.Dexterity, In.Intelligence, In.Vitality };
	int32 Total = 0;
	for (int32& V : Loaded)
	{
		V = FMath::Clamp(V, 1, MaxStatTotal);
		Total += V;
	}
	// Fazlayi, taban ustu payi en buyuk olan stattan keser (sinif tabaninin altina inmez).
	while (Total > MaxStatTotal)
	{
		int32 Largest = 0;
		for (int32 i = 1; i < 4; ++i)
		{
			if (Loaded[i] - Base[i] > Loaded[Largest] - Base[Largest])
			{
				Largest = i;
			}
		}
		if (Loaded[Largest] <= FMath::Max(1, Base[Largest]))
		{
			break;
		}
		--Loaded[Largest];
		--Total;
	}
	// Harcanmamis puan yalniz statlara dagitilmamis kalan pay kadar olabilir (ve level-up toplamini asamaz).
	UnspentStatPoints = FMath::Clamp(In.UnspentStatPoints, 0, FMath::Clamp(MaxStatTotal - Total, 0, PointsFromLevels));
	if (UnspentStatPoints != In.UnspentStatPoints)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplySaveData: harcanmamis stat puani %d -> %d kirpildi (sinif tabani + seviye toplamini asiyordu)"),
			In.UnspentStatPoints, UnspentStatPoints);
	}
	Strength = Loaded[0];
	Dexterity = Loaded[1];
	Intelligence = Loaded[2];
	Vitality = Loaded[3];

	ExperienceToNextLevel = ComputeExperienceForLevel(Level);
	// Egri degistigi icin eski bir kayittaki Experience yeni esigi asabilir ("40000/26492"
	// HUD'u + sonraki XP'de kontrolsuz level atlama). Fazlayi kirp; stat/skill puani vermeden
	// seviye atlatmak yerine bu daha guvenli.
	Experience = (Level >= MaxLevel) ? 0 : FMath::Min(Experience, FMath::Max(0, ExperienceToNextLevel - 1));
	// Turetilmisleri (MaxHealth vb.) yeniden hesapla ve tam canla basla.
	RecalculateDerivedStats(/*bRefill=*/true);
	OnExperienceChanged.Broadcast(Experience, ExperienceToNextLevel);
}

// ----------------------------------------------------------------------
// OnRep — istemci tarafında değer replike olunca UI delegate'lerini tetikle
// ----------------------------------------------------------------------

void UMMOStatsComponent::OnRep_Health()
{
	// Kozmetik hasar/iyilestirme sayilari ayri Unreliable multicast ile gelir. OnRep yalniz
	// gercek durum/UI delegate'ini yeniler; ayni vurus icin iki sayi uretilmez.
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


#if WITH_EDITOR
void UMMOStatsComponent::DestroyDotFixtureSourceOnHealthChanged(float, float)
{
	AActor* Source = DotFixtureSourceToDestroy.Get();
	DotFixtureSourceToDestroy.Reset();
	if (Source) Source->Destroy();
}
#endif

#if !UE_BUILD_SHIPPING
bool UMMOStatsComponent::RunDotSourceLifecycleSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutError.Reset(); OutChecks = 0;
	AMMOGameMode* GM = World ? Cast<AMMOGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!GM || World->GetNetMode() != NM_Standalone
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		OutError = TEXT("disposable standalone headless MMO world required"); return false;
	}
	const bool bOriginalPvP = GM->bAllowPvP;
	const FVector Far(1000000., 1000000., 5000.);
	const FString Tag = TEXT("ZDot") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
	TArray<AMMOCharacter*> Pawns;
	TArray<AMMOPlayerController*> PCs;
	for (int32 I = 0; I < 7; ++I)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* Class = I < 4 ? AMMOCharacter::StaticClass() : AMMOMob::StaticClass();
		AMMOCharacter* C = World->SpawnActor<AMMOCharacter>(Class, Far + FVector(I * 200., 0., 0.), FRotator::ZeroRotator, Params);
		Pawns.Add(C);
		if (!C || !C->GetStats()) { OutError = TEXT("fixture actor/stats missing"); break; }
		C->SetActorTickEnabled(false); C->GetStats()->SetComponentTickEnabled(false);
		if (I < 3)
		{
			AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>(); PCs.Add(PC);
			if (!PC) { OutError = TEXT("fixture controller missing"); break; }
			PC->SetActorTickEnabled(false); PC->Possess(C);
			if (!C->GetPlayerState()) { OutError = TEXT("fixture PlayerState missing"); break; }
			C->GetPlayerState()->SetPlayerName(Tag + FString::FromInt(I));
			C->EndSpawnProtection();
		}
	}
	const auto Check = [&](bool bOk, const TCHAR* Why)
	{
		++OutChecks; if (!bOk && OutError.IsEmpty()) OutError = Why; return bOk;
	};
	const auto Reset = [](UMMOStatsComponent* S, float HP = 100.f)
	{
		S->ClearAllEffects(); S->bIsDead = false; S->MaxHealth = 200.f; S->Health = HP; S->Defense = 0.f; S->LastAppliedDamage = -1.f; S->LastCombatActivityTime = -100.f;
	};
	const auto Run = [&]() -> bool
	{
		if (!OutError.IsEmpty()) return false;
		AMMOCharacter* Victim = Pawns[0]; AMMOCharacter* Source = Pawns[1];
		UMMOStatsComponent* V = Victim->GetStats(); UMMOStatsComponent* MobTarget = Pawns[4]->GetStats();
		if (!Check(Victim->HasAuthority() && Source->IsPlayerControlled() && !Victim->IsSpawnProtected(), TEXT("invalid native controlled fixture"))) return false;
		GM->SetPvPAllowed(false); Reset(V); V->ApplyEffect(TEXT("Poison"), Source); V->ProcessEffects(1.f);
		if (!Check(V->Health == 100.f, TEXT("live source bypassed PvP off"))) return false;
		GM->SetPvPAllowed(true); V->ProcessEffects(1.f);
		if (!Check(V->Health == 92.f, TEXT("live permitted player DoT changed"))) return false;
		// QA 2026-10-09 DUSUK-1: gercek PvP DoT'u (kaynak != hedef) savunmadan dusulur ve olceklenmez.
		Reset(V); V->Defense = 50.f; V->ApplyEffect(TEXT("Poison"), Source); V->ProcessEffects(1.f);
		if (!Check(V->Health == 99.f && V->ActiveEffects.Num() == 1 && V->ActiveEffects[0].Magnitude == 8.f, TEXT("real PvP DoT ignored defense or got scaled"))) return false;
		// QA ORTA-3: daha zayif oyuncunun yenilemesi mobdaki daha guclu oyuncu DoT'unun buyuklugunu/kredisini ezmez.
		Reset(MobTarget); Source->GetStats()->AttackPower = 1000.f; Pawns[2]->GetStats()->AttackPower = 80.f;
		MobTarget->ApplyEffect(TEXT("Poison"), Source); MobTarget->ApplyEffect(TEXT("Poison"), Pawns[2]);
		if (!Check(MobTarget->ActiveEffects.Num() == 1 && MobTarget->ActiveEffects[0].Magnitude == 108.f
			&& MobTarget->ActiveEffects[0].Instigator.Get() == Source, TEXT("weaker player refresh overwrote the stronger DoT"))) return false;
		// A stronger source entering this mob's hub must not suppress a valid weaker refresh.
		AMMOMob* RefreshTarget = CastChecked<AMMOMob>(Pawns[4]);
		const bool bOriginalHub = RefreshTarget->bHasHub;
		const FVector OriginalHubLocation = RefreshTarget->HubLocation;
		const float OriginalHubRadius = RefreshTarget->HubSafeRadius;
		RefreshTarget->bHasHub = true; RefreshTarget->HubLocation = Far; RefreshTarget->HubSafeRadius = 500.f;
		Pawns[2]->SetActorLocation(Far + FVector(1000., 0., 0.));
		Source->SetActorLocation(Far + FVector(600., 0., 0.));
		Reset(MobTarget, 200.f); MobTarget->ApplyEffect(TEXT("Poison"), Source);
		Source->SetActorLocation(Far + FVector(200., 0., 0.)); MobTarget->ProcessEffects(1.f);
		if (!Check(MobTarget->Health == 200.f && MobTarget->LastAppliedDamage == -1.f,
			TEXT("strong source inside mob hub dealt DoT"))) return false;
		MobTarget->ApplyEffect(TEXT("Poison"), Pawns[2]);
		if (!Check(MobTarget->ActiveEffects.Num() == 1 && MobTarget->ActiveEffects[0].Magnitude == 16.f
			&& MobTarget->ActiveEffects[0].Instigator.Get() == Pawns[2] && MobTarget->ActiveEffects[0].bPlayerSource
			&& MobTarget->ActiveEffects[0].RemainingTime == 8.f && MobTarget->ActiveEffects[0].TimeSinceLastTick == 0.f,
			TEXT("hub-blocked stronger source suppressed eligible weaker refresh"))) return false;
		MobTarget->ProcessEffects(1.f);
		if (!Check(MobTarget->Health == 184.f && MobTarget->LastAppliedDamage == 16.f,
			TEXT("replacement source DoT failed to damage mob"))) return false;
		MobTarget->ApplyEffect(TEXT("Poison"), Pawns[2]); MobTarget->ProcessEffects(1.f);
		if (!Check(MobTarget->Health == 168.f && MobTarget->ActiveEffects[0].Instigator.Get() == Pawns[2],
			TEXT("repeated weaker refresh restored hub-blocked credit"))) return false;
		// Exact radius boundary is allowed (<), and a disabled hub must not reduce a stronger DoT.
		for (bool bEnabledHub : {true, false})
		{
			RefreshTarget->HubSafeRadius = bEnabledHub ? 500.f : 0.f;
			Source->SetActorLocation(Far + FVector(bEnabledHub ? 500. : 200., 0., 0.));
			Reset(MobTarget, 200.f); MobTarget->ApplyEffect(TEXT("Poison"), Source); MobTarget->ApplyEffect(TEXT("Poison"), Pawns[2]);
			if (!Check(MobTarget->ActiveEffects.Num() == 1 && MobTarget->ActiveEffects[0].Magnitude == 108.f
				&& MobTarget->ActiveEffects[0].Instigator.Get() == Source,
				TEXT("hub boundary or disabled hub overwrote eligible stronger source"))) return false;
			MobTarget->ProcessEffects(1.f);
			if (!Check(MobTarget->Health == 92.f && MobTarget->LastAppliedDamage == 108.f,
				TEXT("eligible stronger source failed boundary/disabled-hub tick"))) return false;
		}
		RefreshTarget->bHasHub = bOriginalHub; RefreshTarget->HubLocation = OriginalHubLocation; RefreshTarget->HubSafeRadius = OriginalHubRadius;
		Source->SetActorLocation(Far + FVector(200., 0., 0.)); Pawns[2]->SetActorLocation(Far + FVector(400., 0., 0.));
		Reset(V); V->ApplyEffect(TEXT("Poison"), Source); Source->GetPartyComponent()->Members.Add(Victim); V->ProcessEffects(1.f);
		if (!Check(V->Health == 100.f, TEXT("live source bypassed party"))) return false;
		Source->GetPartyComponent()->Members.Reset();
		Source->GetClanComponent()->ClanName = Tag; Victim->GetClanComponent()->ClanName = Tag; V->ProcessEffects(1.f);
		if (!Check(V->Health == 100.f, TEXT("live source bypassed clan"))) return false;
		Source->GetClanComponent()->ClanName.Reset(); Victim->GetClanComponent()->ClanName.Reset();
		APlayerStart* Start = nullptr; for (TActorIterator<APlayerStart> It(World); It; ++It) { Start = *It; break; }
		if (!Check(Start != nullptr, TEXT("hub fixture PlayerStart missing"))) return false;
		Victim->SetActorLocation(Start->GetActorLocation()); V->ProcessEffects(1.f);
		if (!Check(V->Health == 100.f, TEXT("live source bypassed player hub"))) return false;
		Victim->SetActorLocation(Far);
		GM->SetPvPAllowed(false); PCs[1]->UnPossess(); V->ProcessEffects(1.f);
		if (!Check(V->Health == 100.f, TEXT("detached live source became anonymous"))) return false;
		PCs[1]->Possess(Source); Source->EndSpawnProtection();
		Reset(V); Reset(MobTarget); V->ApplyEffect(TEXT("Poison"), Source); MobTarget->ApplyEffect(TEXT("Poison"), Source);
		if (!Check(Source->Destroy(), TEXT("source Destroy failed"))) return false;
		V->ProcessEffects(1.f);
		if (!Check(V->Health == 100.f && !V->bIsDead && V->LastAppliedDamage == -1.f && V->LastCombatActivityTime == -100.f, TEXT("destroyed player source bypassed PvP off"))) return false;
		GM->SetPvPAllowed(true); V->ProcessEffects(1.f);
		if (!Check(V->Health == 100.f, TEXT("destroyed player source damaged anonymously"))) return false;
		MobTarget->ProcessEffects(1.f);
		if (!Check(MobTarget->Health == 100.f && !MobTarget->bIsDead && MobTarget->LastAppliedDamage == -1.f && MobTarget->LastCombatActivityTime == -100.f, TEXT("destroyed player source damaged mob without credit"))) return false;
		V->ProcessEffects(2.f);
		if (!Check(V->Health == 100.f && V->ActiveEffects.Num() == 1, TEXT("orphan catch-up ticks damaged"))) return false;
		V->ProcessEffects(8.f);
		if (!Check(V->Health == 100.f && V->ActiveEffects.IsEmpty(), TEXT("orphan effect failed to expire"))) return false;

		// Refresh must replace source classification in BOTH directions.
		Reset(V); V->ApplyEffect(TEXT("Poison"), Pawns[2]); V->ProcessEffects(0.5f); V->ApplyEffect(TEXT("Poison"), Pawns[5]);
		if (!Check(V->ActiveEffects.Num() == 1 && !V->ActiveEffects[0].bPlayerSource
			&& V->ActiveEffects[0].Instigator.Get() == Pawns[5] && V->ActiveEffects[0].RemainingTime == 8.f
			&& V->ActiveEffects[0].TimeSinceLastTick == 0.f, TEXT("refresh mob provenance/timers stale"))) return false;
		if (!Check(Pawns[5]->Destroy(), TEXT("refresh mob Destroy failed"))) return false;
		V->ProcessEffects(1.f);
		if (!Check(V->Health == 92.f && V->ActiveEffects.Num() == 1, TEXT("player-to-mob refresh retained player classification"))) return false;
		Reset(V); V->ApplyEffect(TEXT("Poison"), Pawns[2]); V->ApplyEffect(TEXT("Poison"), nullptr); V->ProcessEffects(1.f);
		if (!Check(V->Health == 92.f && !V->ActiveEffects[0].bPlayerSource, TEXT("player-to-null refresh stale"))) return false;
		Reset(V); V->ApplyEffect(TEXT("Poison"), Pawns[6]); V->ProcessEffects(0.5f); V->ApplyEffect(TEXT("Poison"), Pawns[2]);
		if (!Check(V->ActiveEffects.Num() == 1 && V->ActiveEffects[0].bPlayerSource
			&& V->ActiveEffects[0].Instigator.Get() == Pawns[2] && V->ActiveEffects[0].RemainingTime == 8.f
			&& V->ActiveEffects[0].TimeSinceLastTick == 0.f && V->ActiveEffects[0].MobSourceHubRadius == 0.f,
			TEXT("refresh player provenance/timers stale"))) return false;
		if (!Check(Pawns[2]->Destroy(), TEXT("refresh player Destroy failed"))) return false;
		V->ProcessEffects(1.f);
		if (!Check(V->Health == 100.f && V->ActiveEffects.Num() == 1, TEXT("mob-to-player refresh lost player classification"))) return false;

		AMMOMob* HubSource = Cast<AMMOMob>(Pawns[6]);
		HubSource->bHasHub = true; HubSource->HubLocation = Far; HubSource->HubSafeRadius = 500.f;
		Reset(V); V->ApplyEffect(TEXT("Poison"), HubSource);
		if (!Check(HubSource->Destroy(), TEXT("cached hub source Destroy failed"))) return false;
		V->ProcessEffects(1.f);
		if (!Check(V->Health == 100.f, TEXT("destroyed mob lost cached hub protection"))) return false;
		Victim->SetActorLocation(Far + FVector(1000., 0., 0.)); V->ProcessEffects(1.f);
		if (!Check(V->Health == 92.f, TEXT("destroyed mob poison stopped outside hub"))) return false;
		Victim->SetActorLocation(Far); V->ApplyEffect(TEXT("Poison"), nullptr); V->ProcessEffects(1.f);
		if (!Check(V->Health == 84.f && V->ActiveEffects[0].MobSourceHubRadius == 0.f, TEXT("mob-to-null refresh kept hub"))) return false;
		Reset(V); V->ApplyEffect(TEXT("Poison"), nullptr); V->ProcessEffects(1.f);
		if (!Check(V->Health == 92.f, TEXT("environmental poison changed"))) return false;
		Reset(V, 50.f); V->ApplyEffect(TEXT("Regen"), Pawns[3]);
		if (!Check(Pawns[3]->Destroy(), TEXT("healer source Destroy failed"))) return false;
		V->ProcessEffects(1.f);
		if (!Check(V->Health == 62.f, TEXT("orphan periodic heal changed"))) return false;
		GM->SetPvPAllowed(false); Reset(V); V->ApplyEffect(TEXT("Poison"), Victim); V->ProcessEffects(1.f);
		if (!Check(V->Health == 92.f, TEXT("live self poison changed"))) return false;
		Reset(V); V->ApplyEffect(TEXT("Poison"), Victim); Victim->bSpawnProtected = true; V->ProcessEffects(1.f); Victim->bSpawnProtected = false;
		if (!Check(V->Health == 100.f, TEXT("DoT bypassed spawn protection"))) return false;
#if WITH_EDITOR
		FActorSpawnParameters CallbackParams; CallbackParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOCharacter* CallbackSource = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Far + FVector(2000., 0., 0.), FRotator::ZeroRotator, CallbackParams);
		Pawns.Add(CallbackSource);
		if (!Check(CallbackSource != nullptr, TEXT("callback source spawn failed"))) return false;
		CallbackSource->SetActorTickEnabled(false); CallbackSource->GetStats()->SetComponentTickEnabled(false);
		GM->SetPvPAllowed(true); Reset(V); V->ApplyEffect(TEXT("Poison"), CallbackSource);
		V->DotFixtureSourceToDestroy = CallbackSource;
		V->OnHealthChanged.AddDynamic(V, &UMMOStatsComponent::DestroyDotFixtureSourceOnHealthChanged);
		V->ProcessEffects(2.f); // The real health delegate destroys source during the first accepted tick.
		V->OnHealthChanged.RemoveDynamic(V, &UMMOStatsComponent::DestroyDotFixtureSourceOnHealthChanged);
		V->DotFixtureSourceToDestroy.Reset();
		if (!Check(V->Health == 92.f && !IsValid(CallbackSource), TEXT("queued tick reused destroyed callback source"))) return false;
#else
		OutError = TEXT("editor callback fixture required"); return false;
#endif
		// DOT_SCALING_DESIGN.md A: oyuncu -> mob DoT'u savunmayi atlar ve saldiriyla olceklenir (8 + AttackPower x 0,10);
		// mob -> oyuncu ve oyuncu -> oyuncu DoT'u eskisi gibi savunmadan dusulur (tek tick 8, Defense 50 -> en az 1).
		Reset(MobTarget); MobTarget->Defense = 50.f; V->AttackPower = 100.f; MobTarget->ApplyEffect(TEXT("Poison"), Victim);
		MobTarget->ProcessEffects(1.f);
		if (!Check(MobTarget->Health == 82.f && MobTarget->ActiveEffects.Num() == 1 && MobTarget->ActiveEffects[0].Magnitude == 18.f,
			TEXT("player DoT on mob did not ignore defense / scale with attack"))) return false;
		Reset(V); V->Defense = 50.f; V->ApplyEffect(TEXT("Poison"), Pawns[4]); V->ProcessEffects(1.f);
		if (!Check(V->Health == 99.f && V->ActiveEffects[0].Magnitude == 8.f, TEXT("mob DoT on player lost defense mitigation or got scaled"))) return false;
		GM->SetPvPAllowed(false); Reset(V); V->Defense = 50.f; V->ApplyEffect(TEXT("Poison"), Victim); V->ProcessEffects(1.f);
		if (!Check(V->Health == 99.f && V->ActiveEffects[0].Magnitude == 8.f, TEXT("self DoT ignored defense or got scaled"))) return false;
		Reset(V, 5.f); V->ApplyEffect(TEXT("Poison"), Victim); V->ProcessEffects(2.f);
		if (!Check(V->Health == 0.f && V->bIsDead && V->ActiveEffects.IsEmpty(), TEXT("fatal catch-up DoT reentrancy changed"))) return false;
		return true;
	};
	const bool bPassed = Run();
	GM->SetPvPAllowed(bOriginalPvP);
	for (AMMOCharacter* C : Pawns) if (IsValid(C))
	{
		C->SetTravelSaveCommitted(true); C->GetPartyComponent()->Members.Reset(); C->GetClanComponent()->ClanName.Reset();
	}
	for (AMMOPlayerController* PC : PCs) if (IsValid(PC)) { PC->UnPossess(); PC->Destroy(); }
	for (AMMOCharacter* C : Pawns) if (IsValid(C)) C->Destroy();
	return bPassed;
}
namespace
{
	struct FMMODotSourceLifecycleExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestDotSourceLifecycle"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOStatsComponent::RunDotSourceLifecycleSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_DOT_SOURCE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMODotSourceLifecycleExec GMMODotSourceLifecycleExec;
}
#endif
