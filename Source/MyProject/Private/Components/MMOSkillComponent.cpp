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

namespace
{
	int64 GetUtcCooldownExpirySeconds(const FDateTime& UtcNow, float RemainingSeconds)
	{
		const FDateTime ExactExpiry = UtcNow + FTimespan::FromSeconds(RemainingSeconds);
		int64 ExpirySeconds = ExactExpiry.ToUnixTimestamp();
		if (FDateTime::FromUnixTimestamp(ExpirySeconds) < ExactExpiry)
		{
			++ExpirySeconds;
		}
		return ExpirySeconds;
	}
}

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
	DOREPLIFETIME(UMMOSkillComponent, MasteryRankArray);
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
	if (MasteryRankArray.Num() != MasteryTreeCount)
	{
		MasteryRankArray.Init(0, MasteryTreeCount);
	}

	// Seviye atlayinca mastery senkronu (RequiredLevel'i sonradan karsilanan skiller icin).
	if (AMMOCharacter* OwnerChar = Cast<AMMOCharacter>(GetOwner()))
	{
		if (UMMOStatsComponent* S = OwnerChar->GetStats())
		{
			S->OnLevelUp.AddDynamic(this, &UMMOSkillComponent::HandleLevelUpMasterySync);
		}
	}
}

void UMMOSkillComponent::HandleLevelUpMasterySync(int32 /*NewLevel*/)
{
	// OnLevelUp hem sunucuda hem istemcide (OnRep_Level) tetiklenir; LearnedSkills replike
	// oldugu icin sadece otorite yazmali.
	if (HasAuthorityInternal())
	{
		SyncLearnedFromMastery();
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
		ChangeClass(NewClass);
	}
	else
	{
		// Ilk Warrior secimi de sunucuda kabul edilmeli; replike sinif tek basina yeterli degil.
		ServerSetupForClass(NewClass);
	}
}

void UMMOSkillComponent::ServerSetupForClass_Implementation(EMMOClass NewClass)
{
	// QA MEDIUM-6: eskiden dogrudan SetupForClass'a gidiyordu — savas ortasinda can/mana
	// fulleniyor, cooldown'lar sifirlaniyordu. Artik dogrulamali yol.
	// Sahte RPC parametresi (gecersiz enum) reddedilir.
	if ((uint8)NewClass > (uint8)EMMOClass::Priest)
	{
		return;
	}
	ChangeClass(NewClass);
}

void UMMOSkillComponent::ChangeClass(EMMOClass NewClass)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	UMMOStatsComponent* S = Owner ? Owner->GetStats() : nullptr;
	const bool bLocalFeedback = GEngine && IsLocallyControlledOwner();

	if (NewClass == CharacterClass)
	{
		// Yeni karakterde varsayilan Warrior secimini de gecerli bir secim say.
		if (Owner && Owner->bClassChoicePending && S && !S->bIsDead)
		{
			Owner->bClassChoicePending = false;
		}
		else if (bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Zaten bu siniftasin"));
		}
		return;
	}
	if (!S || S->bIsDead)
	{
		return;
	}
	// QA: sinif degistir + iade edilen puanlari VIT/INT'e basip cani/manayi anlik doldurma
	// dongusunu ve savas-disi hileyi kesmek icin sinif degisimi arasinda bekleme suresi.
	if (const UWorld* CW = GetWorld())
	{
		const float Now = CW->GetTimeSeconds();
		if (Now - LastClassChangeTime < ClassChangeCooldownSec)
		{
			if (bLocalFeedback)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(255, 120, 80),
					FString::Printf(TEXT("Sinif degisimi icin %.0f sn beklemelisin"), ClassChangeCooldownSec - (Now - LastClassChangeTime)));
			}
			return;
		}
	}
	if (S->IsInCombat())
	{
		if (bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(255, 120, 80),
				FString::Printf(TEXT("Savasta sinif degistirilemez (%.0f sn hasar almamalisin)"), S->CombatDuration));
		}
		return;
	}

	// QA MEDIUM-5: harcanan skill puani = mastery rutbelerinin toplami (her rutbe 1 puan).
	int32 SpentSkillPoints = 0;
	for (const int32 Rank : MasteryRankArray)
	{
		SpentSkillPoints += FMath::Max(0, Rank);
	}

	CharacterClass = NewClass;
	if (const UWorld* CW2 = GetWorld())
	{
		LastClassChangeTime = CW2->GetTimeSeconds();
	}
	LearnedSkills.Reset();
	Hotbar.Init(NAME_None, HotbarSize);
	MasteryRankArray.Init(0, MasteryTreeCount);
	// CooldownEndTimes BILEREK korunur (sifirlamak exploit'ti).

	int32 Str, Dex, Int, Vit;
	GetClassBaseStats(NewClass, Str, Dex, Int, Vit);
	S->ResetPointsForClassChange(Str, Dex, Int, Vit, SpentSkillPoints);

	GrantStarterSkill();

	// Sinif secildi: yeni karakter ipucu paneli kapanir (replike, sahibine gider).
	Owner->bClassChoicePending = false;

	if (bLocalFeedback)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor(120, 200, 255),
			FString::Printf(TEXT("Sinif: %s. Puanlar iade edildi (stat %d, skill %d)"),
				*GetMMOClassDisplayName(NewClass), S->UnspentStatPoints, S->UnspentSkillPoints));
	}
}

void UMMOSkillComponent::GetClassBaseStats(EMMOClass InClass, int32& OutStr, int32& OutDex, int32& OutInt, int32& OutVit)
{
	switch (InClass)
	{
	case EMMOClass::Archer:  OutStr = 10; OutDex = 20; OutInt = 8;  OutVit = 12; break;
	case EMMOClass::Mage:    OutStr = 6;  OutDex = 8;  OutInt = 22; OutVit = 10; break;
	case EMMOClass::Priest:  OutStr = 8;  OutDex = 8;  OutInt = 18; OutVit = 14; break;
	case EMMOClass::Warrior:
	default:                 OutStr = 18; OutDex = 10; OutInt = 6;  OutVit = 16; break;
	}
}

void UMMOSkillComponent::GrantStarterSkill()
{
	// Baslangic skilleri: sinifin en dusuk seviyeli Basic skil(ler)ini bedava ver ve hotbar'a
	// sirayla (slot 0, 1, ...) koy. Ayni en-dusuk seviyede birden fazla Basic skil varsa
	// hepsi verilir (Rahip: Kutsal Isik + Ceza, Lv1'de hem sifa hem hasar skili olsun diye).
	int32 MinLevel = MAX_int32;
	for (const FMMOSkillDef& D : FMMOSkillDatabase::GetAll())
	{
		if (D.RequiredClass == CharacterClass && D.Tree == EMMOSkillTree::Basic)
		{
			MinLevel = FMath::Min(MinLevel, D.RequiredLevel);
		}
	}
	int32 Slot = 0;
	for (const FMMOSkillDef& D : FMMOSkillDatabase::GetAll())
	{
		if (D.RequiredClass == CharacterClass && D.Tree == EMMOSkillTree::Basic && D.RequiredLevel == MinLevel)
		{
			LearnedSkills.AddUnique(D.SkillId);
			if (Hotbar.IsValidIndex(Slot))
			{
				Hotbar[Slot] = D.SkillId;
			}
			++Slot;
		}
	}
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
	MasteryRankArray.Init(0, MasteryTreeCount);
	CooldownEndTimes.Reset();

	AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());

	// Sinifa gore ana stat dagilimi (sadece oyuncu, otorite).
	if (HasAuthorityInternal() && Owner && Owner->IsPlayerControlled())
	{
		if (UMMOStatsComponent* S = Owner->GetStats())
		{
			int32 Str, Dex, Int, Vit;
			GetClassBaseStats(NewClass, Str, Dex, Int, Vit);
			S->InitializePrimaryStats(Str, Dex, Int, Vit);
		}
	}

	GrantStarterSkill();
}

bool UMMOSkillComponent::IsLearned(FName SkillId) const
{
	return LearnedSkills.Contains(SkillId);
}

int32 UMMOSkillComponent::GetMasteryRank(EMMOSkillTree Tree) const
{
	const int32 Idx = (int32)Tree;
	return MasteryRankArray.IsValidIndex(Idx) ? MasteryRankArray[Idx] : 0;
}

void UMMOSkillComponent::SyncLearnedFromMastery()
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	const AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	const UMMOStatsComponent* S = Owner ? Owner->GetStats() : nullptr;

	for (const FMMOSkillDef& D : FMMOSkillDatabase::GetAll())
	{
		if (D.RequiredClass != CharacterClass)
		{
			continue;
		}
		if (D.RequiredMasteryRank > GetMasteryRank(D.Tree))
		{
			continue;
		}
		// Rutbe <= seviye kurali tek basina Lv7 bir skili Lv3'te acabilirdi; skill
		// tablosundaki RequiredLevel dengesi bozulmasin diye ayrica kontrol ediliyor
		// (seviye gelince OnLevelUp senkronu bu skili otomatik acar).
		if (S && S->Level < D.RequiredLevel)
		{
			continue;
		}
		LearnedSkills.AddUnique(D.SkillId);
	}
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

	// KO mastery: bu skil, agacinin rutbesi RequiredMasteryRank'e ulasinca acilir.
	// Tek tiklamayla ancak BIR rutbe yatirilabilir, o yuzden "bir yatirim uzakta" degilse
	// kullaniciya kac rutbe gerektigini soyle.
	const int32 Rank = GetMasteryRank(Def->Tree);
	if (Def->RequiredMasteryRank > Rank + 1)
	{
		OutReason = FString::Printf(TEXT("Rutbe %d gerekli"), Def->RequiredMasteryRank);
		return false;
	}
	// Rutbe zaten yeterliyse skill acilmis olmaliydi (IsLearned yukarida elendi) — bu
	// durumda blokaj RequiredLevel'dir, onu yukarida bildirdik; yine de guvenli davran.
	if (Def->RequiredMasteryRank <= Rank)
	{
		OutReason = FString::Printf(TEXT("Level %d gerekli"), Def->RequiredLevel);
		return false;
	}
	// KO kurali: bir gruptaki rutbe karakter seviyesini asamaz.
	if (Rank + 1 > S->Level)
	{
		OutReason = FString::Printf(TEXT("Level %d gerekli"), Rank + 1);
		return false;
	}
	if (S->UnspentSkillPoints < 1)
	{
		OutReason = TEXT("1 skill puani gerekli");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool UMMOSkillComponent::RequestInvestMastery(EMMOSkillTree Tree)
{
	const int32 Idx = (int32)Tree;

	// Istemci: sunucuya istek gonder (rutbe/puan sunucuda degisir, geri replike olur).
	if (!HasAuthorityInternal())
	{
		ServerInvestMastery(Tree);
		return true;
	}

	if (!MasteryRankArray.IsValidIndex(Idx))
	{
		MasteryRankArray.SetNum(MasteryTreeCount);
		if (!MasteryRankArray.IsValidIndex(Idx))
		{
			return false;
		}
	}

	AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	UMMOStatsComponent* S = Owner ? Owner->GetStats() : nullptr;
	if (!S || S->UnspentSkillPoints < 1)
	{
		return false;
	}
	// KO: (rutbe + 1) > karakter seviyesi ise yatirim yapilamaz.
	if (MasteryRankArray[Idx] < 0 || MasteryRankArray[Idx] >= S->Level)
	{
		return false;
	}
	if (!S->SpendSkillPoints(1))
	{
		return false;
	}

	MasteryRankArray[Idx] += 1;
	SyncLearnedFromMastery();

	if (GEngine && IsLocallyControlledOwner())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(120, 220, 120),
			FString::Printf(TEXT("Ustalik rutbesi: %d"), MasteryRankArray[Idx]));
	}
	return true;
}

void UMMOSkillComponent::ServerInvestMastery_Implementation(EMMOSkillTree Tree)
{
	RequestInvestMastery(Tree);
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

	// Artik tek skill ogrenilmiyor: skilin AGACINA 1 rutbe yatiriliyor, o rutbede acilan
	// tum skiller birlikte geliyor (KO mastery mantigi).
	const FMMOSkillDef* Def = FMMOSkillDatabase::Find(SkillId);
	return Def ? RequestInvestMastery(Def->Tree) : false;
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
	if (Def->Effect == EMMOSkillEffect::MeleeDamage || Def->Effect == EMMOSkillEffect::RangedDamage
		|| Def->Effect == EMMOSkillEffect::AoESelf || Def->Effect == EMMOSkillEffect::AoETarget)
	{
		Owner->GetStats()->MarkCombatActivity();
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
	Out.MasteryRanks = MasteryRankArray;
	Out.ClassChangeCooldownExpiryUtcSeconds = 0;
	Out.SkillCooldownExpiryUtcSeconds.Reset();
	if (const UWorld* World = GetWorld())
	{
		const float WorldNow = World->GetTimeSeconds();
		const FDateTime UtcNow = FDateTime::UtcNow();
		// Unix saniyesine yukari yuvarla; kesirli saniye cooldown'u erken bitirmesin.
		const float ClassRemaining = ClassChangeCooldownSec - (WorldNow - LastClassChangeTime);
		if (ClassRemaining > 0.f)
		{
			Out.ClassChangeCooldownExpiryUtcSeconds = GetUtcCooldownExpirySeconds(UtcNow, ClassRemaining);
		}
		for (const TPair<FName, float>& Cooldown : CooldownEndTimes)
		{
			const float Remaining = Cooldown.Value - WorldNow;
			if (Remaining > 0.f)
			{
				Out.SkillCooldownExpiryUtcSeconds.Add(Cooldown.Key, GetUtcCooldownExpirySeconds(UtcNow, Remaining));
			}
		}
	}
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
	// Mastery rutbeleri: eski kayitlarda bu alan bos gelir (hepsi 0) — ogrenilmis skiller
	// zaten LearnedSkills'ten dogrudan yuklendigi icin eski kayitlar bozulmaz.
	MasteryRankArray = In.MasteryRanks;
	if (MasteryRankArray.Num() != MasteryTreeCount)
	{
		MasteryRankArray.SetNum(MasteryTreeCount);
	}
	// Kayittan gelen bozuk rutbeleri sinirla. ServerInvestMastery'de bir sonraki
	// rutbe icin yapilan toplama boylece tasmaz ve KO'nun rutbe<=seviye kurali korunur.
	const AMMOCharacter* OwnerCharacter = Cast<AMMOCharacter>(GetOwner());
	const UMMOStatsComponent* Stats = OwnerCharacter ? OwnerCharacter->GetStats() : nullptr;
	const int32 SavedLevel = FMath::Clamp(In.Level, 1, Stats ? Stats->MaxLevel : 80);
	for (int32& Rank : MasteryRankArray)
	{
		Rank = FMath::Clamp(Rank, 0, SavedLevel);
	}
	// Eski Priest kayitlari, Ceza baslangic skili eklenmeden once yalnizca Kutsal Isik'i
	// sakliyordu. Ogrenilmis skili tamamla; oyuncunun dolu hotbar slotlarina dokunma.
	if (CharacterClass == EMMOClass::Priest && LearnedSkills.Contains(TEXT("Pri_HolyLight"))
		&& !LearnedSkills.Contains(TEXT("Pri_Smite")))
	{
		LearnedSkills.Add(TEXT("Pri_Smite"));
		if (Hotbar.IsValidIndex(1) && Hotbar[1].IsNone())
		{
			Hotbar[1] = TEXT("Pri_Smite");
		}
	}
	CooldownEndTimes.Reset();
	LastClassChangeTime = -1000.f;
	if (const UWorld* World = GetWorld())
	{
		const float WorldNow = World->GetTimeSeconds();
		const FDateTime UtcNow = FDateTime::UtcNow();
		const int64 UtcNowSeconds = UtcNow.ToUnixTimestamp();
		TArray<FName> SkillIds;
		TArray<float> RemainingSeconds;
		if (In.ClassChangeCooldownExpiryUtcSeconds > UtcNowSeconds)
		{
			// Kayittaki bozuk/gelecekteki zaman damgasi sinifi yillarca kilitlemesin.
			const int64 Seconds = FMath::Min<int64>(In.ClassChangeCooldownExpiryUtcSeconds - UtcNowSeconds,
				FMath::CeilToInt(ClassChangeCooldownSec) + 1);
			const float Remaining = static_cast<float>(Seconds);
			LastClassChangeTime = WorldNow + Remaining - ClassChangeCooldownSec;
		}
		for (const TPair<FName, int64>& Cooldown : In.SkillCooldownExpiryUtcSeconds)
		{
			const FMMOSkillDef* Def = FMMOSkillDatabase::Find(Cooldown.Key);
			if (Def && Def->Cooldown > 0.f && Cooldown.Value > UtcNowSeconds)
			{
				const int64 Seconds = FMath::Min<int64>(Cooldown.Value - UtcNowSeconds,
					FMath::CeilToInt(Def->Cooldown) + 1);
				const float Remaining = static_cast<float>(Seconds);
				CooldownEndTimes.Add(Cooldown.Key, WorldNow + Remaining);
				SkillIds.Add(Cooldown.Key);
				RemainingSeconds.Add(Remaining);
			}
		}
		if (!IsLocallyControlledOwner())
		{
			ClientRestoreSkillCooldowns(SkillIds, RemainingSeconds);
		}
	}
}

void UMMOSkillComponent::ClientRestoreSkillCooldowns_Implementation(const TArray<FName>& SkillIds, const TArray<float>& RemainingSeconds)
{
	CooldownEndTimes.Reset();
	if (const UWorld* World = GetWorld())
	{
		const float WorldNow = World->GetTimeSeconds();
		for (int32 Index = 0; Index < SkillIds.Num() && Index < RemainingSeconds.Num(); ++Index)
		{
			if (RemainingSeconds[Index] > 0.f)
			{
				CooldownEndTimes.Add(SkillIds[Index], WorldNow + RemainingSeconds[Index]);
			}
		}
	}
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

	// Hasar veren skill kullanan oyuncu canlanma korumasini kaybeder (dokunulmazken saldiramasin).
	// Iyilestirme/buff skilleri korumayi bozmaz.
	if (Def.Effect == EMMOSkillEffect::MeleeDamage || Def.Effect == EMMOSkillEffect::RangedDamage
		|| Def.Effect == EMMOSkillEffect::AoESelf || Def.Effect == EMMOSkillEffect::AoETarget)
	{
		Caster->EndSpawnProtection();
	}

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
