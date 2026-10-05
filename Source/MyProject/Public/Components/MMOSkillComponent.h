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

	// EMMOSkillTree eleman sayisi (Basic + Special0..3).
	static constexpr int32 MasteryTreeCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Skill")
	EMMOClass CharacterClass = EMMOClass::Warrior;

	// Ogrenilmis skill id'leri (sunucu yazar, istemcilere replike olur).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Skill")
	TArray<FName> LearnedSkills;

	// KO mastery rutbeleri: index = (int32)EMMOSkillTree, deger = o agactaki rutbe.
	// Skill puani tek tek skillere degil buraya yatirilir; rutbe yukseldikce o agactaki
	// RequiredMasteryRank'i saglayan skiller otomatik acilir (LearnedSkills'e eklenir).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Skill")
	TArray<int32> MasteryRankArray;

	// 10 slotluk hotbar (index 0-9). Bos slot = NAME_None.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Skill")
	TArray<FName> Hotbar;

	/** Sinifi ayarlar: (oyuncuda) stat dagilimini uygular, ogrenilenleri/hotbar'i sifirlar, baslangic skilini verir. Sadece otoritede cagir. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	void SetupForClass(EMMOClass NewClass);

	/**
	 * Sinif degistirme istegi: otoritedeyse dogrudan, istemcideyse Server RPC ile. Sunucuda
	 * ChangeClass'a gider (savas disi + farkli sinif sarti, puan iadesi, can/mana/cooldown korunur).
	 */
	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	void RequestSetupForClass(EMMOClass NewClass);

	UFUNCTION(BlueprintPure, Category = "MMO|Skill")
	bool IsLearned(FName SkillId) const;

	/** Bir mastery agacindaki guncel rutbe (0 = hic yatirim yok). */
	UFUNCTION(BlueprintPure, Category = "MMO|Skill")
	int32 GetMasteryRank(EMMOSkillTree Tree) const;

	// Bu skill, SIMDI 1 skill puani yatirarak acilabilir mi? Degilse OutReason sebebi verir.
	bool CanLearn(FName SkillId, FString& OutReason) const;

	/**
	 * KO mastery yatirimi: 1 skill puani harcar, Tree'nin rutbesini +1 yapar ve o agacta
	 * artik acilan skilleri ogretir. Istemcide Server RPC'ye dallanir. Kural (KO): rutbe
	 * karakter seviyesini gecemez.
	 */
	UFUNCTION(BlueprintCallable, Category = "MMO|Skill")
	bool RequestInvestMastery(EMMOSkillTree Tree);

	/**
	 * Geriye donuk uyumlu giris noktasi (HUD "skile tikla" akisi buraya baglidir):
	 * ARTIK tek bir skill ogretmez, o skilin AGACINA 1 rutbe yatirir.
	 */
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

	// Sinifin baslangic ana stat dagilimi (SetupForClass, ChangeClass ve kayit dogrulamasi ortak).
	static void GetClassBaseStats(EMMOClass InClass, int32& OutStr, int32& OutDex, int32& OutInt, int32& OutVit);

protected:
	// Skill id -> cooldown bitis zamani (dunya saati; her makine kendi kopyasini tutar).
	TMap<FName, float> CooldownEndTimes;
	// Yalnizca mevcut sunucu beceri cagrisi boyunca gecerli; istemcinin hedef onerisi
	// FindNearestEnemy/HealTarget icinde tekrar dogrulanir.
	TWeakObjectPtr<AMMOCharacter> RequestedTargetForCast;

	AMMOCharacter* FindNearestEnemy(float Range) const;
	void ExecuteSkill(AMMOCharacter* Caster, const FMMOSkillDef& Def);

	/**
	 * Oyuncu kaynakli sinif degisimi (sadece otorite; QA MEDIUM-5/6). Ayni sinif = no-op,
	 * savasta/oluyken reddedilir. Gercek degisimde: harcanan stat/skill puanlari iade edilir,
	 * ana statlar yeni sinifin baslangic dagilimina, rutbeler/ogrenilenler/hotbar sifirlanir.
	 * Can/mana doldurulmaz, cooldown'lar sifirlanmaz.
	 */
	void ChangeClass(EMMOClass NewClass);


	// Sinifin en dusuk seviyeli Basic skilini ogretir ve slot 0'a koyar.
	void GrantStarterSkill();

	/** Mevcut rutbelere gore acilmis olmasi gereken tum skilleri LearnedSkills'e yazar (sadece otorite). */
	void SyncLearnedFromMastery();

	// Seviye atlayinca tekrar senkronize et: rutbesi yeterli ama RequiredLevel'i o an
	// karsilanmayan skiller (or. Kan Firtinasi Lv7) seviye gelince otomatik acilsin.
	UFUNCTION()
	void HandleLevelUpMasterySync(int32 NewLevel);

	bool HasAuthorityInternal() const;
	bool IsLocallyControlledOwner() const;

	// --- Server RPC'ler ---
	UFUNCTION(Server, Reliable)
	void ServerUseSkill(FName SkillId, AMMOCharacter* RequestedTarget);

	UFUNCTION(Server, Reliable)
	void ServerInvestMastery(EMMOSkillTree Tree);

	UFUNCTION(Server, Reliable)
	void ServerAssignToSlot(int32 Slot, FName SkillId);

	UFUNCTION(Server, Reliable)
	void ServerClearSlot(int32 Slot);

	UFUNCTION(Server, Reliable)
	void ServerSetupForClass(EMMOClass NewClass);

	// Kayittan yuklenen sunucu cooldown'larini sahip istemcinin dunya saatine uyarlar.
	UFUNCTION(Client, Reliable)
	void ClientRestoreSkillCooldowns(const TArray<FName>& SkillIds, const TArray<float>& RemainingSeconds);

	// Sinif degisimleri arasi bekleme (sunucu, saniye) ve son degisim zamani.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Skills", meta = (ClampMin = "0"))
	float ClassChangeCooldownSec = 60.f;
	float LastClassChangeTime = -1000.f;

	// Skill gorselini TUM makinelerde oynat (sunucu tetikler; her istemci yerel cizer).
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSkillFX(EMMOSkillEffect Effect, FVector Start, FVector End, float Radius, int32 AnimIndex);
};
