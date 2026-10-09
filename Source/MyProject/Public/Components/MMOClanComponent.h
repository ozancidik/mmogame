// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOClanComponent.generated.h"

class AMMOCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOOnClanChanged);

/**
 * Basit klan (lonca) bileseni. Sunucu-otoriteli.
 * Her karakterin kayitli bir klan adi ve lider bayragi vardir; klan "uyelerin ortak adi"dir
 * (ayri bir klan nesnesi yok). Uyelik karakter kaydinda saklanir (FMMOCharacterSaveData).
 * Komutlar sohbetten gelir (AMMOPlayerController::ServerSendChat zaten sunucuda calisir),
 * bu yuzden burada ayri RPC yok: Cmd* fonksiyonlari yalniz otoritede calisir.
 * ClanName herkese replike olur (isim plakasi icin), bIsLeader yalniz sahibine.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOClanComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOClanComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Klan bilgisi degisince HUD'u tazelemek icin.
	UPROPERTY(BlueprintAssignable, Category = "MMO|Clan")
	FMMOOnClanChanged OnClanChanged;

	// Bos = klansiz. Herkese replike (isim plakasi).
	UPROPERTY(ReplicatedUsing = OnRep_Clan, BlueprintReadOnly, Category = "MMO|Clan")
	FString ClanName;

	// Yalniz sahibine replike.
	UPROPERTY(ReplicatedUsing = OnRep_Clan, BlueprintReadOnly, Category = "MMO|Clan")
	bool bIsLeader = false;

	UFUNCTION(BlueprintPure, Category = "MMO|Clan")
	bool IsInClan() const { return !ClanName.IsEmpty(); }

	static constexpr int32 MinNameLength = 3;
	static constexpr int32 MaxNameLength = 12;
	static constexpr float InviteRange = 600.f;
	static constexpr float InviteCooldown = 3.f;
	static constexpr float InviteExpirySeconds = 30.f;

	// Klan adi kurali: 3-12 karakter, yalniz harf/rakam.
	static bool IsValidClanName(const FString& Name);

	// --- Sohbet komutlari (yalniz sunucu/otorite; ServerSendChat icinden cagrilir) ---
	void CmdCreate(const FString& Name);
	void CmdInvite();
	void CmdAccept();
	// Chat must pass the generation captured by its sending pawn; no current-server fallback.
	void CmdAcceptSeen(uint64 SeenInviteId);
	// Owner-only runtime generation; retained after consumption, never saved or wrapped.
	UPROPERTY(Replicated)
	uint64 PendingInviteId = 0;
	// Owner-only runtime membership context; never persisted, renewed on entry/load.
	UPROPERTY(Replicated)
	FGuid ClanMembershipId;
	void CmdLeave();
	void CmdKick(const FString& TargetName);
	void CmdList();
	// Klan sohbeti: yalniz ayni klan adindaki cevrimici oyunculara (kanal 5).
	void CmdChat(const FString& Text);

	// --- Kalicilik (yalniz otorite) ---
	void FillSaveData(struct FMMOCharacterSaveData& Out) const;
	void ApplySaveData(const struct FMMOCharacterSaveData& In);

#if !UE_BUILD_SHIPPING
	static bool RunInviteAuthoritySelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

protected:
	UFUNCTION()
	void OnRep_Clan();

	bool HasAuthorityInternal() const;
	bool SetPendingInviteServer(AMMOCharacter* Inviter);
	AMMOCharacter* GetOwnerCharacter() const;

	// Sahibin ekranina sistem sohbet satiri gonderir.
	void NotifyOwner(const FString& Text) const;

	// Ayni klan adindaki cevrimici uyeleri toplar (ExcludeSelf ile kendini haric tutar).
	void GatherOnlineMembers(const FString& InClanName, TArray<AMMOCharacter*>& Out, bool bExcludeSelf) const;

	// Klanin cevrimici uyelerine sistem satiri gonderir (ExcludeSelf ile kendini haric tutar).
	void NotifyClan(const FString& InClanName, const FString& Text, bool bExcludeSelf) const;

	// Klanin cevrimici uyeleri arasinda lider yoksa en eski cevrimici uyeyi lider yapar.
	// bExcludeSelf: bu bilesen aday sayilmaz (ayrilan / dusen lider).
	void EnsureLeader(const FString& InClanName, bool bExcludeSelf);

	// Replike alanlari degistirdikten sonra yerel delegeyi tetikler (listen-server host UI).
	void MarkChanged();

	// Katilma sirasi (sunucu durumu, replike degil): lider devrinde "en eski uye" icin.
	int64 JoinSeq = 0;

	// Calisma zamani: bu lider kayitli degil, yalniz cevrimici lider olmadigi icin vekil terfi etti.
	// Kayda yazilmaz; asil lider geri donerse liderlik ona doner.
	bool bActingLeader = false;

	// Bekleyen davet (sunucu durumu, replike degil).
	TWeakObjectPtr<AMMOCharacter> PendingInviteFrom;
	FString PendingInviteClan;
	float PendingInviteTime = -1.f;
	float LastInviteTime = -100.f;
};
