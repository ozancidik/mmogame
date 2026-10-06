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

float UMMOStatsComponent::ApplyDamage(float RawDamage, AActor* /*DamageInstigator*/)
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
	const float Mitigated = FMath::Max(1.f, RawDamage - Defense);
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

	// Buyuk tek seferlik odul/debug XP'si int32 toplamayi tasirmasin. Ara bakiye
	// seviye esikleri dusulene kadar 64 bit kalir; saklanan Experience tekrar
	// ilgili seviyenin (int32) esiginin altina inince yazilir.
	int64 PendingExperience = static_cast<int64>(Experience) + Amount;

	// Tek seferde birden fazla level atlanabilir.
	while (Level < MaxLevel && PendingExperience >= ExperienceToNextLevel)
	{
		PendingExperience -= ExperienceToNextLevel;
		HandleLevelUp();
	}

	if (Level >= MaxLevel)
	{
		Experience = 0;
	}
	else
	{
		Experience = static_cast<int32>(PendingExperience);
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
	const int32 OldLevel = Level;
	Level++;
	ExperienceToNextLevel = ComputeExperienceForLevel(Level);
	UnspentStatPoints += StatPointsPerLevel;
	UnspentSkillPoints += SkillPointsPerLevel;

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

void UMMOStatsComponent::ResetPointsForClassChange(int32 NewStrength, int32 NewDexterity, int32 NewIntelligence, int32 NewVitality, int32 SpentSkillPoints)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	// Iade: puanlarin tek kaynagi HandleLevelUp (seviye basina StatPointsPerLevel /
	// SkillPointsPerLevel, Lv1'de 0). Stat puaninin toplami seviyeden, skill puaninin
	// harcanani mastery rutbelerinden gelir; ikisi de "en az seviyeden hesaplanan toplam"
	// ile alttan sinirlanir (eski skill sisteminden kalan, rutbeye yazilmamis harcamalar icin).
	const int32 LevelsGained = FMath::Max(0, Level - 1);
	UnspentStatPoints = FMath::Max(UnspentStatPoints, LevelsGained * StatPointsPerLevel);
	UnspentSkillPoints = FMath::Max(UnspentSkillPoints + FMath::Max(0, SpentSkillPoints), LevelsGained * SkillPointsPerLevel);

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
	const float SourceHubRadius = (SourceMob && SourceMob->bHasHub) ? SourceMob->HubSafeRadius : 0.f;
	const FVector SourceHubLocation = SourceMob ? SourceMob->HubLocation : FVector::ZeroVector;

	// Bitis zamani (sunucu dunya saati): istemci kalan sureyi bundan hesaplar (replike alan yalniz uygulamada/tazelemede degisir).
	const UWorld* EffectWorld = GetWorld();
	const AGameStateBase* EffectGS = EffectWorld ? EffectWorld->GetGameState() : nullptr;
	const float EffectNow = EffectGS ? EffectGS->GetServerWorldTimeSeconds() : (EffectWorld ? EffectWorld->GetTimeSeconds() : 0.f);
	const float EffectEnd = EffectNow + Def->Duration;

	// Ayni efekt zaten varsa: suresini tazele (KO tarzi yenileme; ust uste yigilmaz).
	for (FMMOActiveEffect& E : ActiveEffects)
	{
		if (E.EffectId == EffectId)
		{
			E.RemainingTime = Def->Duration;
			E.EndServerTime = EffectEnd;
			E.TimeSinceLastTick = 0.f;
			E.Instigator = EffectInstigator;
			E.MobSourceHubLocation = SourceHubLocation;
			E.MobSourceHubRadius = SourceHubRadius;
			OnEffectsChanged.Broadcast();
			return;
		}
	}

	FMMOActiveEffect A;
	A.EffectId = Def->EffectId;
	A.Type = Def->Type;
	A.Magnitude = Def->Magnitude;
	A.RemainingTime = Def->Duration;
	A.EndServerTime = EffectEnd;
	A.Duration = Def->Duration;
	A.bIsDebuff = Def->bIsDebuff;
	A.TickInterval = Def->TickInterval;
	A.TimeSinceLastTick = 0.f;
	A.Instigator = EffectInstigator;
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
		AActor* Instigator;
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
				Pending.Add({ E.Type, E.Magnitude, E.Instigator.Get(), E.MobSourceHubRadius, E.MobSourceHubLocation });
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
	UnspentSkillPoints = FMath::Clamp(In.UnspentSkillPoints, 0, LevelsGained * FMath::Max(0, SkillPointsPerLevel));
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
