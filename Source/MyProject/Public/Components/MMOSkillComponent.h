// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Skills/MMOSkillTypes.h"
#include "MMOSkillComponent.generated.h"

class AMMOCharacter;

/**
 * KO tarzi skill bileseni: sinif, ogrenilen skiller ve 0-9 hotbar.
 * Skill puani ile skill ogrenilir (skill agacindan), ogrenilen skiller hotbar slotlarina atanir,
 * numara tuslariyla (hedefe) kullanilir. Skill tanimlari FMMOSkillDatabase'den gelir.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOSkillComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr int32 HotbarSize = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Skill")
	EMMOClass CharacterClass = EMMOClass::Warrior;

	// Ogrenilmis skill id'leri (sunucu yazar, istemcilere replike olur).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Skill")
	TArray<FName> LearnedSkills;

	// 10 slotluk hotbar (index 0-9). Bos slot = NAME_None.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Skill")
	TArray<FName> Hotbar;

	/** Sinifi ayarlar: (oyuncuda) stat dagilimini uygular, ogrenilenleri/hotbar'i sifirlar, baslangic skilini verir. Sadece otoritede cagir. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	void SetupForClass(EMMOClass NewClass);

	/** Sinif degistirme istegi: otoritedeyse dogrudan, istemcideyse Server RPC ile. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	void RequestSetupForClass(EMMOClass NewClass);

	UFUNCTION(BlueprintPure, Category = "MMO|Skill")
	bool IsLearned(FName SkillId) const;

	// Ogrenilebilir mi? Degilse OutReason sebebi verir.
	bool CanLearn(FName SkillId, FString& OutReason) const;

	/** Skill ogrenir (kosullari saglarsa skill puani harcar). */
	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	bool LearnSkill(FName SkillId);

	/** Ogrenilmis bir skilli hotbar slotuna atar. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	void AssignToSlot(int32 Slot, FName SkillId);

	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	void ClearSlot(int32 Slot);

	UFUNCTION(BlueprintPure, Category = "MMO|Skill")
	FName GetSlotSkill(int32 Slot) const;

	/** Hotbar slotundaki skilli kullanir (numara tuslari buraya baglanir). */
	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	bool UseHotbarSlot(int32 Slot);

	/** Id ile skill kullanir (mana + cooldown kontrol eder). */
	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	bool UseSkill(FName SkillId);

	UFUNCTION(BlueprintPure, Category = "MMO|Skill")
	float GetCooldownRemaining(FName SkillId) const;

	// Kalicilik: sinif/ogrenilenler/hotbar verisini kayda yazar / kayittan uygular (sadece otorite).
	void FillSaveData(struct FMMOCharacterSaveData& Out) const;
	void ApplySaveData(const struct FMMOCharacterSaveData& In);

protected:
	// Skill id -> cooldown bitis zamani (dunya saati; her makine kendi kopyasini tutar).
	TMap<FName, float> CooldownEndTimes;

	AMMOCharacter* FindNearestEnemy(float Range) const;
	void ExecuteSkill(AMMOCharacter* Caster, const FMMOSkillDef& Def);

	bool HasAuthorityInternal() const;
	bool IsLocallyControlledOwner() const;

	// --- Server RPC'ler ---
	UFUNCTION(Server, Reliable)
	void ServerUseSkill(FName SkillId);

	UFUNCTION(Server, Reliable)
	void ServerLearnSkill(FName SkillId);

	UFUNCTION(Server, Reliable)
	void ServerAssignToSlot(int32 Slot, FName SkillId);

	UFUNCTION(Server, Reliable)
	void ServerClearSlot(int32 Slot);

	UFUNCTION(Server, Reliable)
	void ServerSetupForClass(EMMOClass NewClass);

	// Skill gorselini TUM makinelerde oynat (sunucu tetikler; her istemci yerel cizer).
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSkillFX(EMMOSkillEffect Effect, FVector Start, FVector End, float Radius);
};
